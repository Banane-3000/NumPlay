"""Reading Subway Surfers' own data out of its Android package (a Unity game): the bundles, meshes (positions,
normals, places in pictures, skins), skeletons (trees of transforms) and animation clips (curves on transforms).

Everything stays in Unity's own units and axes (x right, y up, z forward, the way the runner goes); tools/apk.py
scales it to metres. The package is read as data, nothing in it is run."""
import glob
import math
import os
import struct
import zlib

import numpy as np
import UnityPy


class Package:
    """the addressable bundles of an extracted package (its assets/aa/Android/<version> folder)"""

    def __init__(self, folder):
        self.folder = folder
        self.cache = {}

    def path(self, name):
        found = glob.glob(os.path.join(self.folder, name + "_" + "?" * 32 + ".bundle"))
        if len(found) != 1:
            raise FileNotFoundError(name)
        return found[0]

    def env(self, name):
        if name not in self.cache:
            self.cache[name] = UnityPy.load(self.path(name))
        return self.cache[name]

    def objects(self, name, kind):
        """{object name: object} of one kind in a bundle"""
        out = {}
        for o in self.env(name).objects:
            if o.type.name == kind:
                d = o.read()
                out.setdefault(getattr(d, "m_Name", "") or str(o.path_id), d)
        return out

    def texture(self, name, tex):
        return self.objects(name, "Texture2D")[tex].image.convert("RGBA")

    def mesh(self, name, mesh):
        return mesh_arrays(self.objects(name, "Mesh")[mesh])


def mesh_arrays(m):
    """m: a UnityPy Mesh object (read). Returns dict: pos (n,3), nrm (n,3) or None, uv (n,2) or None,
    col (n,4) or None, tris list per submesh (k,3), skin (idx (n,4), w (n,4)) or None, bindpose (b,4,4)"""
    from UnityPy.helpers.MeshHelper import MeshHandler
    h = MeshHandler(m)
    h.process()
    pos = np.array(h.m_Vertices, dtype=np.float64).reshape(-1, 3)
    out = {"pos": pos}
    out["nrm"] = np.array(h.m_Normals, dtype=np.float64).reshape(-1, 3) if h.m_Normals else None
    out["uv"] = np.array(h.m_UV0, dtype=np.float64).reshape(-1, 2) if h.m_UV0 else None
    out["col"] = np.array(h.m_Colors, dtype=np.float64).reshape(-1, 4) if h.m_Colors else None
    subs = []
    tri = h.get_triangles()
    for t in tri:
        subs.append(np.array(t, dtype=np.int64).reshape(-1, 3))
    out["subs"] = subs
    if h.m_BoneIndices is not None and len(h.m_BoneIndices):
        out["bidx"] = np.array(h.m_BoneIndices, dtype=np.int64).reshape(-1, 4)
        w = np.array(h.m_BoneWeights, dtype=np.float64).reshape(-1, 4)
        # (some packed weights come out wrong in the last place: it is what the others leave)
        bad = (w < 0).any(axis=1) | (w.sum(axis=1) > 1.001)
        w[bad, 3] = np.clip(1 - w[bad, :3].sum(axis=1), 0, 1)
        w = np.clip(w, 0, 1)
        w /= np.maximum(w.sum(axis=1, keepdims=True), 1e-9)
        out["bw"] = w
    else:
        out["bidx"] = out["bw"] = None
    bp = getattr(m, "m_BindPose", None) or []
    out["bindpose"] = np.array([[[getattr(b, f"e{r}{c}") for c in range(4)] for r in range(4)] for b in bp]) if bp else None
    out["bonehash"] = list(getattr(m, "m_BoneNameHashes", []) or [])
    return out


def quat_mat(q):
    x, y, z, w = q
    n = math.sqrt(x * x + y * y + z * z + w * w) or 1
    x, y, z, w = x / n, y / n, z / n, w / n
    return np.array([[1 - 2 * (y * y + z * z), 2 * (x * y - z * w), 2 * (x * z + y * w)],
                     [2 * (x * y + z * w), 1 - 2 * (x * x + z * z), 2 * (y * z - x * w)],
                     [2 * (x * z - y * w), 2 * (y * z + x * w), 1 - 2 * (x * x + y * y)]])


def trs(t, q, s):
    M = np.eye(4)
    M[:3, :3] = quat_mat(q) * np.asarray(s)[None, :]
    M[:3, 3] = t
    return M


class Skeleton:
    """the transforms under a root: names, parents, rest TRS, paths (for clip bindings)"""

    def __init__(self, root_tr):
        self.names, self.parent, self.t, self.q, self.s, self.paths, self.pid = [], [], [], [], [], [], []
        self._walk(root_tr, -1, "")
        self.by_pid = {p: i for i, p in enumerate(self.pid)}
        self.by_hash = {zlib.crc32(p.encode()): i for i, p in enumerate(self.paths)}

    def _walk(self, tr, parent, path):
        go = tr.m_GameObject.read()
        i = len(self.names)
        self.names.append(go.m_Name)
        self.parent.append(parent)
        p, r, s = tr.m_LocalPosition, tr.m_LocalRotation, tr.m_LocalScale
        self.t.append((p.x, p.y, p.z))
        self.q.append((r.x, r.y, r.z, r.w))
        self.s.append((s.x, s.y, s.z))
        self.paths.append(path)
        self.pid.append(tr.object_reader.path_id)
        for c in tr.m_Children:
            ct = c.read()
            cn = ct.m_GameObject.read().m_Name
            self._walk(ct, i, (path + "/" if path else "") + cn)

    def world(self, local=None):
        """world matrices of every transform; local: {index: (t, q, s)} overrides"""
        W = []
        for i in range(len(self.names)):
            t, q, s = self.t[i], self.q[i], self.s[i]
            if local and i in local:
                lt = local[i]
                t = lt[0] if lt[0] is not None else t
                q = lt[1] if lt[1] is not None else q
                s = lt[2] if lt[2] is not None else s
            M = trs(t, q, s)
            W.append(M if self.parent[i] < 0 else W[self.parent[i]] @ M)
        return W


def _streamed(clip):
    """the streamed keys: {curve: [(time, coeff[4])]}"""
    sc = clip.m_MuscleClip.m_Clip.data.m_StreamedClip
    data = struct.pack(f"<{len(sc.data)}I", *sc.data)
    pos, keys = 0, {}
    while pos < len(data):
        t, n = struct.unpack_from("<fi", data, pos)
        pos += 8
        for _ in range(n):
            idx = struct.unpack_from("<i", data, pos)[0]
            co = struct.unpack_from("<4f", data, pos + 4)
            pos += 20
            keys.setdefault(idx, []).append((t, co))
    return keys, sc.curveCount


def _eval_streamed(ks, t):
    best = None
    for kt, co in ks:
        if math.isfinite(kt) and kt <= t + 1e-6:
            best = (kt, co)
    if best is None:
        for kt, co in ks:
            if math.isfinite(kt):
                return co[3]
        return ks[0][1][3]
    dt = t - best[0]
    co = best[1]
    return ((co[0] * dt + co[1]) * dt + co[2]) * dt + co[3]


class Clip:
    def __init__(self, clip):
        self.name = clip.m_Name
        mc = clip.m_MuscleClip
        self.start, self.stop = mc.m_StartTime, mc.m_StopTime
        self.length = self.stop - self.start
        self.keys, ns = _streamed(clip)
        dc = mc.m_Clip.data.m_DenseClip
        self.ns, self.nd = ns, dc.m_CurveCount
        self.dense = (dc.m_FrameCount, dc.m_CurveCount, dc.m_SampleRate, dc.m_BeginTime, list(dc.m_SampleArray))
        self.const = list(mc.m_Clip.data.m_ConstantClip.data)
        # bindings: (path hash, attribute, first curve)
        self.bind = []
        c = 0
        for b in clip.m_ClipBindingConstant.genericBindings:
            tid = getattr(b, "typeID", None)
            if tid is None:
                tid = b.classID
            n = (4 if b.attribute == 2 else 3) if tid == 4 else 1
            self.bind.append((b.path, tid, b.attribute, c))
            c += n

    def curve(self, i, t):
        if i < self.ns:
            ks = self.keys.get(i)
            return _eval_streamed(ks, t) if ks else 0.0
        i -= self.ns
        if i < self.nd:
            fc, cc, sr, bt, arr = self.dense
            f = (t - bt) * sr
            f0 = max(0, min(fc - 1, int(math.floor(f))))
            f1 = min(fc - 1, f0 + 1)
            a = f - f0 if f1 != f0 else 0
            a = max(0.0, min(1.0, a))
            return arr[f0 * cc + i] * (1 - a) + arr[f1 * cc + i] * a
        i -= self.nd
        return self.const[i] if i < len(self.const) else 0.0

    def sample(self, skel, t):
        """{bone index: (t, q, s)} at time t (from the clip's start)"""
        t += self.start
        out = {}
        for h, tid, attr, c in self.bind:
            if tid != 4 or h not in skel.by_hash:
                continue
            i = skel.by_hash[h]
            cur = out.setdefault(i, [None, None, None])
            if attr == 1:
                cur[0] = tuple(self.curve(c + k, t) for k in range(3))
            elif attr == 2:
                cur[1] = tuple(self.curve(c + k, t) for k in range(4))
            elif attr == 3:
                cur[2] = tuple(self.curve(c + k, t) for k in range(3))
            elif attr == 4:
                e = [math.radians(self.curve(c + k, t)) for k in range(3)]
                cur[1] = euler_quat(*e)
        return out


def euler_quat(x, y, z):
    """Unity's euler order (z, then x, then y)"""
    cx, sx = math.cos(x / 2), math.sin(x / 2)
    cy, sy = math.cos(y / 2), math.sin(y / 2)
    cz, sz = math.cos(z / 2), math.sin(z / 2)
    # q = qy * qx * qz
    qx = (sx, 0, 0, cx)
    qy = (0, sy, 0, cy)
    qz = (0, 0, sz, cz)
    return qmul(qmul(qy, qx), qz)


def qmul(a, b):
    ax, ay, az, aw = a
    bx, by, bz, bw = b
    return (aw * bx + ax * bw + ay * bz - az * by, aw * by - ax * bz + ay * bw + az * bx,
            aw * bz + ax * by - ay * bx + az * bw, aw * bw - ax * bx - ay * by - az * bz)


def mesh_materials(env):
    """{mesh name: [material name of each submesh]} from the bundle's mesh renderers"""
    out = {}
    for o in env.objects:
        if o.type.name != "MeshFilter":
            continue
        mf = o.read()
        try:
            mesh = mf.m_Mesh.read()
            go = mf.m_GameObject.read()
        except Exception:
            continue
        for c in go.m_Component:
            try:
                co = c.component.read()
            except Exception:
                continue
            if co.object_reader.type.name == "MeshRenderer":
                names = []
                for m in co.m_Materials:
                    try:
                        names.append(m.read().m_Name)
                    except Exception:
                        names.append("?")
                out.setdefault(mesh.m_Name, names)
    return out
