#!/usr/bin/env python3
"""PolyTrack's data for the calculator, read from Kodub's web build of PolyTrack 0.6.3 (kodub.itch.io/polytrack):
the 17 official tracks, the track parts' models (render and physics), the car, the materials and the track maps.

  python3 tools/pack.py WEB      WEB: the web build's folder (index.html, main.bundle.js, models/, tracks/)

Writes src/data.c and src/data.h. Needs numpy, DracoPy (the models are Draco compressed) and Pillow. The web build
is only read as data: its JavaScript is parsed with regular expressions, never run.
"""
import collections
import heapq
import json
import math
import os
import re
import struct
import sys
import zlib

import numpy as np
import DracoPy

HERE = os.path.dirname(os.path.abspath(__file__))
SRC = os.path.join(HERE, '..', 'src')
FIX = 256.0                      # vertex units a metre
TRACKS = ['summer%d' % i for i in range(1, 8)] + ['winter%d' % i for i in range(1, 6)] + \
         ['desert%d' % i for i in range(1, 6)]
SCENES = {'Road': 'road', 'RoadWide': 'road_wide', 'Planes': 'planes', 'Blocks': 'blocks', 'Pillar': 'pillar',
          'Signs': 'signs', 'WallTrack': 'wall_track'}
MATERIALS = ['Road', 'RoadEdgeRed', 'RoadEdgeWhite', 'RoadBarrier', 'FinishWhite', 'FinishBlack', 'Finish', 'StartLine',
             'Checkpoint', 'PlaneWall', 'PlaneWallDetail', 'BlockSurface', 'Pillar', 'PillarEdge', 'WallTrack',
             'WallTrackBottom', 'WallTrackSides', 'SignRed', 'SignBlack', 'SignYellow', 'RoadEdge']
CAR_MATERIALS = ['Main', 'Stripe', 'AirIntake', 'Metal', 'BrakeLight', 'Tire', 'Rim']
EDGE = {'RoadEdgeRed': 'RoadEdge', 'RoadEdgeWhite': 'RoadEdge'}
CHUNK = 20.0                     # parts are cut into 20 m squares for culling and detail
CELL = 5.0                       # the physics grid inside a part
TCELL = 8                        # the track grid: 8 tiles (40 m)


# ------------------------------------------------------------------ glb (Draco)
def load_glb(path):
    b = open(path, 'rb').read()
    _, _, length = struct.unpack_from('<III', b, 0)
    off, js, binc = 12, None, None
    while off < length:
        cl, ct = struct.unpack_from('<II', b, off)
        off += 8
        if ct == 0x4E4F534A:
            js = json.loads(b[off:off + cl])
        elif ct == 0x004E4942:
            binc = b[off:off + cl]
        off += cl
    return js, binc


def quat_mat(q):
    x, y, z, w = q
    return np.array([[1 - 2 * (y * y + z * z), 2 * (x * y - z * w), 2 * (x * z + y * w)],
                     [2 * (x * y + z * w), 1 - 2 * (x * x + z * z), 2 * (y * z - x * w)],
                     [2 * (x * z - y * w), 2 * (y * z + x * w), 1 - 2 * (x * x + y * y)]])


def glb_meshes(path):
    """name -> list of primitives (material, local points, points with the node's transform, faces, uv)"""
    js, binc = load_glb(path)
    mats = [m.get('name') for m in js.get('materials', [])]
    out = {}
    for n in js['nodes']:
        if 'mesh' not in n:
            continue
        T = np.array(n.get('translation', [0, 0, 0]), float)
        R = quat_mat(n.get('rotation', [0, 0, 0, 1]))
        S = np.array(n.get('scale', [1, 1, 1]), float)
        prims = []
        for pr in js['meshes'][n['mesh']]['primitives']:
            ext = pr['extensions']['KHR_draco_mesh_compression']
            bv = js['bufferViews'][ext['bufferView']]
            o = bv.get('byteOffset', 0)
            m = DracoPy.decode(binc[o:o + bv['byteLength']])
            pts = np.array(m.points, float).reshape(-1, 3)
            faces = np.array(m.faces, int).reshape(-1, 3)
            uv = np.array(m.tex_coord, float).reshape(-1, 2) if m.tex_coord is not None and len(m.tex_coord) else None
            world = (R @ (pts * S).T).T + T
            prims.append(dict(material=mats[pr['material']] if pr.get('material') is not None else None,
                              local=pts, verts=world, faces=faces, uv=uv))
        out[n['name']] = prims
    return out


class Models:
    def __init__(self, web):
        self.web, self.cache = web, {}

    def scene(self, name):
        if name not in self.cache:
            self.cache[name] = glb_meshes(os.path.join(self.web, 'models', SCENES.get(name, name) + '.glb'))
        return self.cache[name]


# ------------------------------------------------------------------ the bundle
def module(src, mid):
    i = src.find(',' + mid + ':(e,t,n)=>')
    if i < 0:
        raise SystemExit('module %s not found in main.bundle.js' % mid)
    k = src.find('{', i)
    d = 0
    while True:
        c = src[k]
        if c == '{':
            d += 1
        elif c == '}':
            d -= 1
            if d == 0:
                break
        elif c in '"\'`':
            q = c
            k += 1
            while src[k] != q:
                k += 2 if src[k] == '\\' else 1
        k += 1
    return src[i:k + 1]


NUM = r'-?(?:\d+\.?\d*|\.\d+)(?:e-?\d+)?'


def nums(s):
    return [float(x) for x in re.findall(NUM, s)]


def split_top(s):
    out, d, cur, q = [], 0, '', None
    for ch in s:
        if q:
            cur += ch
            if ch == q:
                q = None
            continue
        if ch in '"\'':
            q = ch
        if ch in '([{':
            d += 1
        if ch in ')]}':
            d -= 1
        if ch == ',' and d == 0:
            out.append(cur)
            cur = ''
        else:
            cur += ch
    if cur:
        out.append(cur)
    return out


def inner(s):
    return nums(re.search(r'\(([^()]*)\)', s).group(1))


def parse_opts(s):
    o = {}
    for k in ('flipX', 'flipY', 'flipZ'):
        if k + ':!0' in s:
            o[k] = True
    m = re.search(r'offset:new i\.Pq0\(([^)]*)\)', s)
    if m:
        o['offset'] = nums(m.group(1))
    m = re.search(r'scale:new i\.Pq0\(([^)]*)\)', s)
    if m:
        o['scale'] = nums(m.group(1))
    m = re.search(r'setFromEuler\(new i\.O9p\(([^)]*)\)\)', s)
    if m:
        o['euler'] = nums(m.group(1))
    return o


def part_table(src):
    names = {int(v): k for k, v in re.findall(r'\[e\.(\w+)=(\d+)\]', module(src, '494'))}
    byname = {v: k for k, v in names.items()}
    m = module(src, '2600')
    body = m[m.find('const u=[') + 9:]
    parts = {}
    for item in re.finditer(r'new d\("([0-9a-f]{64})",', body):
        k, d, q = item.end(), 1, None
        while d:
            ch = body[k]
            if q:
                if ch == q:
                    q = None
            elif ch in '"\'':
                q = ch
            elif ch == '(':
                d += 1
            elif ch == ')':
                d -= 1
            k += 1
        args = split_top(body[item.end():k - 1])
        name = args[1].split('.')[-1]
        models = [(mm.group(1), mm.group(2), parse_opts(mm.group(3) or ''))
                  for mm in re.finditer(r'\["(\w+)","(\w+)"(,\{.*?\})?\]', args[2])]
        det = start = None
        if len(args) > 5 and args[5] != 'null':
            dm = re.search(r'type:o\.A\.(\w+),center:\[([^\]]*)\],size:\[([^\]]*)\]', args[5])
            det = dict(type=dm.group(1), center=nums(dm.group(2)), size=nums(dm.group(3)))
        if len(args) > 6:
            start = inner(args[6])
        parts[byname[name]] = dict(id=byname[name], name=name, cat=args[0].split('.')[-1], models=models,
                                   palette=args[3].strip(), tiles=json.loads(args[4]), detector=det, start=start)
    colors = {}
    for env, block in (('Winter', 'Winter'), ('Desert', 'Desert')):
        mm = re.search(r'id:a\.A\.' + block + r',colors:\{([^}]*)\}', m)
        colors[env] = dict(re.findall(r'(\w+):"#([0-9a-f]{6})"', mm.group(1)))
    custom = re.findall(r'id:a\.A\.Custom(\d),colors:\{BlockSurface:"#([0-9a-f]{6})"\}', m)
    colors['Custom'] = [c for _, c in sorted(custom)]
    return names, parts, colors


def orient_table(src):
    i = src.find(',5494:(e,t,n)=>')
    seg = src[i:i + 3000]
    seg = seg[seg.find('const a=[') + 9:]
    rows = []
    for row in re.finditer(r'\[((?:new i\.PTz\([^)]*\),?)+)\]', seg):
        rows.append([nums(q) for q in re.findall(r'new i\.PTz\(([^)]*)\)', row.group(1))])
        if len(rows) == 6:
            break
    mats = []
    for axis in range(6):
        for rot in range(4):
            M = np.round(quat_mat(rows[axis][rot])).astype(int)
            mats.append(M)
    return mats, rows


def mountain_table(src):
    i = src.find('a={value:[.12047764760664692')
    seg = src[i + 10:src.find(']}', i)]
    return nums(seg)


# ------------------------------------------------------------------ part meshes
def euler_quat(e):
    x, y, z = e
    c1, c2, c3 = math.cos(x / 2), math.cos(y / 2), math.cos(z / 2)
    s1, s2, s3 = math.sin(x / 2), math.sin(y / 2), math.sin(z / 2)
    return (s1 * c2 * c3 + c1 * s2 * s3, c1 * s2 * c3 - s1 * c2 * s3, c1 * c2 * s3 + s1 * s2 * c3,
            c1 * c2 * c3 - s1 * s2 * s3)


def build_part(models, part):
    """The part's triangles, as the game merges its models: [(material, a, b, c)] in metres, part space."""
    tris = []
    for scene, node, o in part['models']:
        prims = models.scene(scene)[node]
        p = max(pr['verts'][:, 1].max() for pr in prims) if o.get('flipY') else 0.0
        for pr in prims:
            v = pr['verts'].copy()
            f = pr['faces'].copy()
            if 'scale' in o:
                v = v * np.array(o['scale'])
            if 'euler' in o:
                v = (quat_mat(euler_quat(o['euler'])) @ v.T).T
            flip = [o.get('flipX'), o.get('flipY'), o.get('flipZ')]
            v = v * np.array([-1.0 if k else 1.0 for k in flip])
            if any(flip):
                f = f[:, [0, 2, 1]]
            off = np.array(o.get('offset', [0, 0, 0]), float)
            if o.get('flipY'):
                off = off + np.array([0, p, 0])
            v = v + off
            for a, b, c in f:
                tris.append((pr['material'], v[a], v[b], v[c]))
    return tris


# ------------------------------------------------------------------ simplification (quadric edge collapse)
QW = 1e-4


def weld(tris, matmap=None):
    idx, verts, faces = {}, [], []
    for mat, *vs in tris:
        if matmap:
            mat = matmap.get(mat, mat)
        ids = []
        for v in vs:
            k = (round(v[0] / QW), round(v[1] / QW), round(v[2] / QW))
            if k not in idx:
                idx[k] = len(verts)
                verts.append(np.array(v, float))
            ids.append(idx[k])
        if len(set(ids)) == 3:
            faces.append([mat, ids[0], ids[1], ids[2]])
    return np.array(verts), faces


def plane(V, f):
    a, b, c = V[f[1]], V[f[2]], V[f[3]]
    n = np.cross(b - a, c - a)
    l = np.linalg.norm(n)
    if l < 1e-12:
        return None, 0.0
    n = n / l
    return np.append(n, -np.dot(n, a)), l / 2


def simplify(tris, tol, matmap=None, keep_materials=True, boundary_weight=100.0):
    """Garland-Heckbert edge collapse onto existing vertices (so planar surfaces stay exact), keeping open and
    material boundaries. tol: the error allowed (metres)."""
    V, F = weld(tris, matmap)
    if not keep_materials:
        for f in F:
            f[0] = 'x'
    nv = len(V)
    Qv = [np.zeros((4, 4)) for _ in range(nv)]
    vf = [set() for _ in range(nv)]
    alive = [True] * len(F)
    for i, f in enumerate(F):
        p, _ = plane(V, f)
        for k in (1, 2, 3):
            vf[f[k]].add(i)
        if p is not None:
            K = np.outer(p, p)
            for k in (1, 2, 3):
                Qv[f[k]] += K
    emap = collections.defaultdict(list)
    for i, f in enumerate(F):
        for a, b in ((f[1], f[2]), (f[2], f[3]), (f[3], f[1])):
            emap[(min(a, b), max(a, b))].append(i)
    for (a, b), fs in emap.items():
        if len(fs) == 1 or len({F[i][0] for i in fs}) > 1:
            for i in fs:
                p, _ = plane(V, F[i])
                if p is None:
                    continue
                n2 = np.cross(V[b] - V[a], p[:3])
                l = np.linalg.norm(n2)
                if l < 1e-12:
                    continue
                n2 /= l
                p2 = np.append(n2, -np.dot(n2, V[a]))
                K = np.outer(p2, p2) * boundary_weight
                Qv[a] += K
                Qv[b] += K
    ver = [0] * nv
    gone = [False] * nv

    def cost(u, v):
        Qs = Qv[u] + Qv[v]
        best = None
        for t, w in ((v, u), (u, v)):
            x = np.append(V[t], 1.0)
            c = float(x @ Qs @ x)
            if best is None or c < best[0] - 1e-15:
                best = (c, w, t)
        return best

    heap = []
    for (a, b) in sorted(emap):
        heap.append((cost(a, b)[0], a, b, 0, 0))
    heapq.heapify(heap)
    lim = tol * tol

    def neighbors(x):
        s = set()
        for i in vf[x]:
            if alive[i]:
                s.update(F[i][1:])
        s.discard(x)
        return s

    while heap:
        c, a, b, va, vb = heapq.heappop(heap)
        if c > lim:
            break
        if gone[a] or gone[b] or ver[a] != va or ver[b] != vb:
            continue
        c2, w, t = cost(a, b)
        if c2 > lim:
            continue
        shared = [i for i in vf[w] if alive[i] and t in F[i][1:]]
        if not shared or len(neighbors(w) & neighbors(t)) != len(shared):
            continue
        ok = True
        for i in vf[w]:
            if not alive[i] or t in F[i][1:]:
                continue
            f = F[i]
            old, _ = plane(V, f)
            if old is None:
                continue          # a sliver already: it may stay one, or go
            new, a1 = plane(V, [f[0]] + [t if k == w else k for k in f[1:]])
            if new is None or a1 < 1e-7 or np.dot(new[:3], old[:3]) < 0.5:
                ok = False
                break
        if not ok:
            continue
        for i in shared:
            alive[i] = False
        for i in list(vf[w]):
            if alive[i]:
                f = F[i]
                for k in (1, 2, 3):
                    if f[k] == w:
                        f[k] = t
                vf[t].add(i)
        vf[w] = set()
        gone[w] = True
        Qv[t] = Qv[t] + Qv[w]
        ver[t] += 1
        for x in sorted(neighbors(t)):
            a2, b2 = min(t, x), max(t, x)
            heapq.heappush(heap, (cost(a2, b2)[0], a2, b2, ver[a2], ver[b2]))
    return [(f[0], V[f[1]], V[f[2]], V[f[3]]) for i, f in enumerate(F) if alive[i] and plane(V, f)[1] > 1e-7]


def simplify_budget(tris, tol, budget, area, matmap=None, top=1.0):
    """simplify, raising the error allowed until there are at most `budget` triangles per 400 m2 of the part"""
    while True:
        out = simplify(tris, tol, matmap=matmap)
        if len(out) <= budget * max(1.0, area / 400.0) or tol >= top:
            return out
        tol *= 1.5


# ------------------------------------------------------------------ tracks
B62 = 'ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789'
B62R = {c: i for i, c in enumerate(B62)}


def b62(e):
    t, n, L = 0, bytearray(), len(e)
    for s, ch in enumerate(e):
        l = B62R[ch]
        nb = 5 if (l & 30) == 30 else 6
        a = t // 8
        while a >= len(n):
            n.append(0)
        sh = t - 8 * a
        n[a] |= (l << sh) & 255
        if sh > 8 - nb and s != L - 1:
            if a + 1 >= len(n):
                n.append(0)
            n[a + 1] |= l >> (8 - sh)
        t += nb
    return bytes(n)


def read_track(text, cp_ids, start_ids):
    assert text.startswith('PolyTrack2')
    b = zlib.decompress(b62(zlib.decompress(b62(text[10:])).decode('ascii')))
    h = 0
    n = b[h]; h += 1
    name = b[h:h + n].decode(); h += n
    n = b[h]; h += 1
    author = b[h:h + n].decode(); h += n
    if b[h]:
        h += 4
    h += 1
    env, sun = b[h], b[h + 1]; h += 2
    mx, my, mz = struct.unpack_from('<iii', b, h); h += 12
    w = b[h]; h += 1
    wx, wy, wz = w & 3, (w >> 2) & 3, (w >> 4) & 3
    parts = []
    while h < len(b):
        pid = b[h]; h += 1
        cnt = struct.unpack_from('<i', b, h)[0]; h += 4
        for _ in range(cnt):
            x = int.from_bytes(b[h:h + wx], 'little') + mx; h += wx
            y = int.from_bytes(b[h:h + wy], 'little') + my; h += wy
            z = int.from_bytes(b[h:h + wz], 'little') + mz; h += wz
            r = b[h]; col = b[h + 1]; h += 2
            p = dict(id=pid, x=x, y=y, z=z, rot=r & 3, axis=(r >> 2) & 7, color=col)
            if pid in cp_ids:
                p['cp'] = b[h] | b[h + 1] << 8; h += 2
            if pid in start_ids:
                p['start'] = struct.unpack_from('<I', b, h)[0]; h += 4
            parts.append(p)
    return dict(name=name, author=author, env=env, sun=sun, parts=parts)


def tile_rotate(x, y, z, rot, axis):
    """module 5494's sR: a part's tile turned like the part"""
    if axis in (1, 3, 5):
        if rot == 1: x, z = -z - 1, x
        elif rot == 2: x, z = -x - 1, -z - 1
        elif rot == 3: x, z = z, -x - 1
    else:
        if rot == 1: x, z = z, -x - 1
        elif rot == 2: x, z = -x - 1, -z - 1
        elif rot == 3: x, z = -z - 1, x
    if axis == 1: x, y = -x - 1, -y - 1
    elif axis == 2: x, y = y, -x - 1
    elif axis == 3: x, y = -y - 1, x
    elif axis == 4: y, z = -z - 1, y
    elif axis == 5: y, z = z, -y - 1
    return x, y, z


def part_tiles(part):
    out = []
    for (a, b) in part['tiles']:
        for x in range(a[0], b[0] + 1):
            for y in range(a[1], b[1] + 1):
                for z in range(a[2], b[2] + 1):
                    out.append((x, y, z))
    return out


# ------------------------------------------------------------------ output helpers
class Out:
    FMT = {'int8_t': 'b', 'uint8_t': 'B', 'int16_t': 'h', 'uint16_t': 'H', 'int32_t': 'i', 'uint32_t': 'I'}

    def __init__(self):
        self.c, self.h = [], []
        self.bin = bytearray()

    def blob(self, ctype, name, values):
        """a big table in data.bin, reached through a macro"""
        values = [int(v, 16) if isinstance(v, str) else int(v) for v in values] or [0]
        while len(self.bin) % 4:
            self.bin.append(0)
        off = len(self.bin)
        self.bin += struct.pack('<%d%s' % (len(values), self.FMT[ctype]), *values)
        self.h.append('#define %s ((const %s *)(pt_blob + %d))   /* %d */' % (name, ctype, off, len(values)))

    def blob_raw(self, ctype, name, data, count):
        while len(self.bin) % 4:
            self.bin.append(0)
        off = len(self.bin)
        self.bin += data
        self.h.append('#define %s ((const %s *)(pt_blob + %d))   /* %d */' % (name, ctype, off, count))

    def arr(self, ctype, name, values, per_line=16, static=False):
        values = list(values)
        self.c.append('%sconst %s %s[%d] = {' % ('static ' if static else '', ctype, name, max(1, len(values))))
        if not values:
            values = [0]
        for i in range(0, len(values), per_line):
            self.c.append('  ' + ', '.join(str(v) for v in values[i:i + per_line]) + ',')
        self.c.append('};')
        if not static:
            self.h.append('extern const %s %s[%d];' % (ctype, name, max(1, len(values))))


def fl(v):
    s = '%.9g' % v
    if '.' not in s and 'e' not in s and 'n' not in s:
        s += '.0'
    return s + 'f'


def q16(v):
    r = int(round(v * FIX))
    if not -32768 <= r <= 32767:
        raise SystemExit('vertex out of range: %r' % v)
    return r


def srgb_to_linear(c):
    c /= 255.0
    return c / 12.92 if c <= 0.04045 else ((c + 0.055) / 1.055) ** 2.4


def hex_linear(h):
    return [srgb_to_linear(int(h[i:i + 2], 16)) for i in (0, 2, 4)]


# ------------------------------------------------------------------ the stripe on the car (images/pattern_stripe.svg)
STRIPE = [[(259.51, 81.711), (400.05, 81.711), (400.05, 227.23), (259.51, 227.23)],
          [(276.18, 236.23), (383.37, 236.23), (368.7, 373.52), (290.86, 373.52)],
          [(259.51, 387.11), (400.05, 387.11), (400.05, 644.93), (391.43, 708.02), (368.75, 845.56),
           (353.28, 893.41), (306.28, 893.41), (290.81, 845.56), (268.13, 708.02), (259.51, 644.93)]]


def in_poly(pt, poly):
    x, y = pt
    c = False
    for i in range(len(poly)):
        x1, y1 = poly[i - 1]
        x2, y2 = poly[i]
        if (y1 > y) != (y2 > y) and x < x1 + (y - y1) * (x2 - x1) / (y2 - y1):
            c = not c
    return c


def main():
    web = sys.argv[1] if len(sys.argv) > 1 else os.environ.get('POLYTRACK_WEB', '')
    if not web or not os.path.exists(os.path.join(web, 'main.bundle.js')):
        raise SystemExit(__doc__)
    src = open(os.path.join(web, 'main.bundle.js'), encoding='utf-8', errors='replace').read()
    names, table, envcolors = part_table(src)
    orients, _ = orient_table(src)
    mtable = mountain_table(src)
    models = Models(web)
    cp_ids = {k for k, v in table.items() if v['detector'] and v['detector']['type'] == 'Checkpoint'}
    start_ids = {k for k, v in table.items() if v['start']}

    tracks = []
    for t in TRACKS:
        tracks.append(read_track(open(os.path.join(web, 'tracks', 'official', t + '.track')).read().strip(),
                                 cp_ids, start_ids))
    used = sorted({p['id'] for t in tracks for p in t['parts']})
    pidx = {pid: i for i, pid in enumerate(used)}
    out = Out()

    # ---------------------------------------------------------------- parts
    pool, chunks, cvidx, rtris, ctris, cgrid, cgidx, partrec = [], [], [], [], [], [], [], []
    simple = {}
    part_mesh = {}
    print('parts:', len(used), file=sys.stderr)
    for pid in used:
        part = table[pid]
        raw = build_part(models, part)
        raw = [t for t in raw if t[0] in MATERIALS or t[0] is None]
        mats = {t[0] for t in raw}
        col = simplify(raw, 0.015, keep_materials=False)
        if len(raw) <= 64:
            exact = simplify(raw, 0.001)
            lod0 = lod1 = exact
            if len(exact) <= 32:
                col = [('x', a, b, c) for _, a, b, c in exact]
                simple[pid] = True
        else:
            av = np.array([v for t in raw for v in t[1:]])
            area = float((av[:, 0].max() - av[:, 0].min()) * (av[:, 2].max() - av[:, 2].min()))
            lod0 = simplify_budget(raw, 0.06, 360, area, top=0.4)
            lod1 = simplify_budget(lod0, 0.35, 120, area, matmap=EDGE, top=2.0)
        part_mesh[pid] = (lod0, lod1, col)
        # vertex pool for the part
        vbase = len(pool)
        vid = {}

        def vref(v):
            k = (q16(v[0]), q16(v[1]), q16(v[2]))
            if k not in vid:
                vid[k] = len(pool) - vbase
                pool.append(k)
            return vid[k]

        allv = np.array([v for t in raw for v in t[1:]])
        lo, hi = allv.min(0), allv.max(0)
        # chunks: by triangle centre, on a 20 m grid anchored at the part's minimum corner
        ncx = max(1, int(math.ceil((hi[0] - lo[0]) / CHUNK - 0.01)))
        ncz = max(1, int(math.ceil((hi[2] - lo[2]) / CHUNK - 0.01)))

        def chunk_of(t):
            c = (t[1] + t[2] + t[3]) / 3
            return (min(ncx - 1, max(0, int((c[0] - lo[0]) // CHUNK))), min(ncz - 1, max(0, int((c[2] - lo[2]) // CHUNK))))

        groups = collections.defaultdict(lambda: ([], []))
        for t in lod0:
            groups[chunk_of(t)][0].append(t)
        for t in lod1:
            groups[chunk_of(t)][1].append(t)
        # a chunk holds at most 240 vertices in each detail: cut bigger ones by triangle order
        pieces = []
        for key in sorted(groups):
            l0, l1 = groups[key]
            s0, s1 = cut(l0), cut(l1)
            for i in range(max(len(s0), len(s1))):
                pieces.append((s0[i] if i < len(s0) else [], s1[i] if i < len(s1) else []))
        first_chunk = len(chunks)
        for piece in pieces:
            rec = {}
            for li, lst in enumerate(piece):
                local = {}
                vlist = []
                tl = []
                for mat, a, b, c in lst:
                    ids = []
                    for v in (a, b, c):
                        g = vref(v)
                        if g not in local:
                            local[g] = len(vlist)
                            vlist.append(g)
                        ids.append(local[g])
                    qa = np.array(pool[vbase + vlist[ids[0]]], float)
                    qb = np.array(pool[vbase + vlist[ids[1]]], float)
                    qc = np.array(pool[vbase + vlist[ids[2]]], float)
                    n = np.cross(qb - qa, qc - qa)
                    ln = np.linalg.norm(n)
                    if ln < 1e-9:
                        continue
                    n = n / ln
                    tl.append((ids[0], ids[1], ids[2], MATERIALS.index(mat if mat else 'Road'),
                               int(round(n[0] * 127)), int(round(n[1] * 127)), int(round(n[2] * 127))))
                if len(vlist) > 255:
                    raise SystemExit('chunk with too many vertices in %s' % part['name'])
                rec[li] = (len(cvidx), len(vlist), len(rtris), len(tl))
                cvidx.extend(vlist)
                rtris.extend(tl)
            pts = [pool[vbase + cvidx[rec[0][0] + i]] for i in range(rec[0][1])] or [(0, 0, 0)]
            pts += [pool[vbase + cvidx[rec[1][0] + i]] for i in range(rec[1][1])]
            pa = np.array(pts)
            box = list(pa.min(0)) + list(pa.max(0))
            chunks.append((rec[0], rec[1], box))
        # physics triangles and their 5 m grid
        cfirst = len(ctris)
        ctl = []
        for _, a, b, c in col:
            ia, ib, ic = vref(a), vref(b), vref(c)
            if len({ia, ib, ic}) < 3:
                continue
            ctl.append((ia, ib, ic))
        ctris.extend(ctl)
        gx = max(1, int(math.ceil((hi[0] - lo[0]) / CELL - 1e-6)))
        gz = max(1, int(math.ceil((hi[2] - lo[2]) / CELL - 1e-6)))
        cells = [[] for _ in range(gx * gz)]
        for ti, (ia, ib, ic) in enumerate(ctl):
            P = np.array([pool[vbase + ia], pool[vbase + ib], pool[vbase + ic]], float) / FIX
            x0 = int(max(0, min(gx - 1, (P[:, 0].min() - lo[0] - 0.05) // CELL)))
            x1 = int(max(0, min(gx - 1, (P[:, 0].max() - lo[0] + 0.05) // CELL)))
            z0 = int(max(0, min(gz - 1, (P[:, 2].min() - lo[2] - 0.05) // CELL)))
            z1 = int(max(0, min(gz - 1, (P[:, 2].max() - lo[2] + 0.05) // CELL)))
            for cx in range(x0, x1 + 1):
                for cz in range(z0, z1 + 1):
                    cells[cz * gx + cx].append(ti)
        gfirst = len(cgrid)
        for cl in cells:
            cgrid.append(len(cgidx))
            cgidx.extend(cl)
        lo16 = [q16(v) for v in lo]
        hi16 = [q16(v) for v in hi]
        det = part['detector']
        partrec.append(dict(vbase=vbase, nv=len(pool) - vbase, chunk=first_chunk, nchunk=len(chunks) - first_chunk,
                            col=cfirst, ncol=len(ctl), grid=gfirst, gx=gx, gz=gz, lo=lo16, hi=hi16,
                            simple=1 if pid in simple else 0,
                            det=0 if not det else (1 if det['type'] == 'Checkpoint' else 2),
                            dc=det['center'] if det else [0, 0, 0], ds=det['size'] if det else [0, 0, 0],
                            start=part['start'] or [0, 0, 0], custom=1 if part['palette'] == 'h' else 0,
                            name=part['name'], id=pid))
        print('  %-26s raw %5d lod0 %5d lod1 %5d col %5d chunks %d' % (part['name'], len(raw), len(lod0), len(lod1),
                                                                     len(ctl), len(chunks) - first_chunk),
              file=sys.stderr)
    cgrid.append(len(cgidx))

    # ---------------------------------------------------------------- tracks
    tinst, tmasks, tcells, tspec, theads, tthumb = [], [], [], [], [], []
    M = [np.array(m, float) for m in orients]
    for ti, t in enumerate(tracks):
        insts = []
        # world triangles of simple parts, to find faces hidden against a neighbour (or the ground)
        planes = collections.defaultdict(list)
        for k, p in enumerate(t['parts']):
            pid = p['id']
            if pid not in simple:
                continue
            Mo = M[p['axis'] * 4 + p['rot']]
            base = np.array([p['x'], p['y'], p['z']], float) * 5
            for i, (_, a, b, c) in enumerate(part_mesh[pid][0]):
                W = [Mo @ v + base for v in (a, b, c)]
                n = np.cross(W[1] - W[0], W[2] - W[0])
                n = n / np.linalg.norm(n)
                key = (round(n[0], 2), round(n[1], 2), round(n[2], 2), round(float(np.dot(n, W[0])), 2))
                Wa = np.array(W)
                c0 = np.floor((Wa.min(0) - 0.01) / 10).astype(int)
                c1 = np.floor((Wa.max(0) + 0.01) / 10).astype(int)
                for gx_ in range(c0[0], c1[0] + 1):
                    for gy_ in range(c0[1], c1[1] + 1):
                        for gz_ in range(c0[2], c1[2] + 1):
                            planes[key + (gx_, gy_, gz_)].append((k, W))
        masks = {}
        for k, p in enumerate(t['parts']):
            pid = p['id']
            if pid not in simple:
                continue
            Mo = M[p['axis'] * 4 + p['rot']]
            base = np.array([p['x'], p['y'], p['z']], float) * 5
            mask = 0
            for i, (_, a, b, c) in enumerate(part_mesh[pid][0]):
                W = [Mo @ v + base for v in (a, b, c)]
                n = np.cross(W[1] - W[0], W[2] - W[0])
                n = n / np.linalg.norm(n)
                if n[1] < -0.99 and max(abs(w[1]) for w in W) < 1e-3:
                    continue      # on the ground, facing it
                key = (round(-n[0], 2), round(-n[1], 2), round(-n[2], 2), round(float(np.dot(-n, W[0])), 2))
                cen = (W[0] + W[1] + W[2]) / 3
                samples = [cen] + [w + (cen - w) * 0.02 for w in W]
                cov = True
                for s in samples:
                    hit = False
                    cs = tuple(int(x) for x in np.floor(s / 10))
                    for k2, W2 in planes.get(key + cs, ()):
                        if k2 == k:
                            continue
                        if point_in_tri(s, W2):
                            hit = True
                            break
                    if not hit:
                        cov = False
                        break
                if not cov:
                    mask |= 1 << i
            masks[k] = mask
        # instances
        mlist = [0xFFFFFFFF]
        mix = {0xFFFFFFFF: 0}
        recs = []
        for k, p in enumerate(t['parts']):
            pid = p['id']
            mi = 0
            if pid in simple:
                m = masks[k]
                if m == 0:
                    continue      # nothing of it can be seen or touched
                if m != (1 << len(part_mesh[pid][0])) - 1:
                    if m not in mix:
                        mix[m] = len(mlist)
                        mlist.append(m)
                    mi = mix[m]
            pr = partrec[pidx[pid]]
            Mo = M[p['axis'] * 4 + p['rot']]
            lo = np.array(pr['lo'], float) / FIX
            hi = np.array(pr['hi'], float) / FIX
            corners = np.array([[x, y, z] for x in (lo[0], hi[0]) for y in (lo[1], hi[1]) for z in (lo[2], hi[2])])
            W = (Mo @ corners.T).T + np.array([p['x'], p['y'], p['z']], float) * 5
            wl, wh = W.min(0), W.max(0)
            color = p['color']
            recs.append(dict(p=p, part=pidx[pid], mi=mi, lo=wl, hi=wh, color=color, k=k))
        if len(mlist) > 256:
            raise SystemExit('too many masks in %s' % t['name'])
        # grid of 40 m cells by centre
        cx = [int(math.floor(((r['lo'][0] + r['hi'][0]) / 2) / (TCELL * 5))) for r in recs]
        cz = [int(math.floor(((r['lo'][2] + r['hi'][2]) / 2) / (TCELL * 5))) for r in recs]
        x0, x1, z0, z1 = min(cx), max(cx), min(cz), max(cz)
        gw, gh = x1 - x0 + 1, z1 - z0 + 1
        order = sorted(range(len(recs)), key=lambda i: ((cz[i] - z0) * gw + (cx[i] - x0), recs[i]['k']))
        cellstart = []
        j = 0
        for c in range(gw * gh):
            cellstart.append(j)
            while j < len(order) and (cz[order[j]] - z0) * gw + (cx[order[j]] - x0) == c:
                j += 1
        cellstart.append(len(order))
        reach = 0.0
        for r in recs:
            reach = max(reach, float(max(r['hi'][0] - r['lo'][0], r['hi'][2] - r['lo'][2])) / 2)
        newidx = {}
        ibase = len(tinst)
        for n_, i in enumerate(order):
            r = recs[i]
            p = r['p']
            newidx[r['k']] = n_
            tinst.append((r['part'], p['rot'] | p['axis'] << 2, r['color'], p['y'], p['x'], p['z'], r['mi']))
        # start, checkpoints, finish
        starts = [r for r in recs if 'start' in r['p']]
        st = max(starts, key=lambda r: r['p']['start'])
        cps = sorted([r for r in recs if 'cp' in r['p']], key=lambda r: (r['p']['cp'], newidx[r['k']]))
        fins = [r for r in recs if table[r['p']['id']]['detector'] and table[r['p']['id']]['detector']['type'] == 'Finish']
        sbase = len(tspec)
        for r in cps:
            tspec.append((newidx[r['k']], r['p']['cp']))
        for r in fins:
            tspec.append((newidx[r['k']], 0xFFFF))
        norders = len({r['p']['cp'] for r in cps})
        # bounds (tiles) for the mountains and the map
        xs = [p['x'] for p in t['parts']]
        zs = [p['z'] for p in t['parts']]
        # map (module 9117's createThumbnail): a pixel a part (4 tiles)
        tiles = []
        for p in t['parts']:
            part = table[p['id']]
            kind = 1
            if part['start']:
                kind = 3
            elif part['detector'] and part['detector']['type'] == 'Checkpoint':
                kind = 2
            elif part['detector'] and part['detector']['type'] == 'Finish':
                kind = 4
            for (tx, ty, tz) in part_tiles(part):
                rx, ry, rz = tile_rotate(tx, ty, tz, p['rot'], p['axis'])
                tiles.append(((p['x'] + rx - 2) // 4, (p['z'] + rz - 2) // 4, kind))
        mx0 = min(a for a, _, _ in tiles); mx1 = max(a for a, _, _ in tiles)
        mz0 = min(b for _, b, _ in tiles); mz1 = max(b for _, b, _ in tiles)
        tw, th = mx1 - mx0 + 1, mz1 - mz0 + 1
        img = [0] * (tw * th)
        for a, b, kind in tiles:
            img[(b - mz0) * tw + (a - mx0)] = max(1, img[(b - mz0) * tw + (a - mx0)]) if kind == 1 else kind
        # (as the game: checkpoints, then starts, then finishes painted over the plain parts)
        for prio in (2, 3, 4):
            for a, b, kind in tiles:
                if kind == prio:
                    img[(b - mz0) * tw + (a - mx0)] = kind
        tbase = len(tthumb)
        for i in range(0, len(img), 2):
            tthumb.append(img[i] | ((img[i + 1] if i + 1 < len(img) else 0) << 4))
        mbase = len(tmasks)
        tmasks.extend(mlist)
        cbase = len(tcells)
        tcells.extend(cellstart)
        stp = st['p']
        theads.append(dict(name=t['name'], env=t['env'], sun=t['sun'], inst=ibase, ninst=len(order), mask=mbase,
                           cell=cbase, gx0=x0, gz0=z0, gw=gw, gh=gh, reach=int(math.ceil(reach)),
                           spec=sbase, ncp=len(cps), nfin=len(fins), norders=norders,
                           start=newidx[st['k']], bx0=min(xs), bx1=max(xs), bz0=min(zs), bz1=max(zs),
                           thumb=tbase, tw=tw, th=th))
        print('track %-9s parts %4d kept %4d masks %3d cells %dx%d reach %.0f orders %d map %dx%d' % (
            t['name'], len(t['parts']), len(order), len(mlist), gw, gh, reach, norders, tw, th), file=sys.stderr)

    # ---------------------------------------------------------------- the car
    car = models.scene('car')
    body = []
    for pr in car['Body']:
        for f in pr['faces']:
            a, b, c = (pr['verts'][i] for i in f)
            mat = pr['material']
            if mat == 'Main' and pr['uv'] is not None:
                uv = (pr['uv'][f[0]] + pr['uv'][f[1]] + pr['uv'][f[2]]) / 3 * 1024
                if any(in_poly(uv, s) for s in STRIPE):
                    mat = 'Stripe'
            body.append((mat, a, b, c))
    body = simplify(body, 0.004)
    wheel = make_wheel(car['Wheel0'])
    extra = []
    for node in ('Suspension', 'Exhaust0'):
        for pr in car[node]:
            for f in pr['faces']:
                extra.append((pr['material'], *(pr['verts'][i] for i in f)))
    extra = simplify(extra, 0.01)
    hull = []
    for pr in car['Collision']:
        hull = [tuple(float(x) for x in v) for v in pr['verts']]
    print('car: body %d wheel %d extra %d hull %d' % (len(body), len(wheel), len(extra), len(hull)), file=sys.stderr)

    def mesh_arrays(name, tris):
        vid, verts, tl = {}, [], []
        for mat, a, b, c in tris:
            ids = []
            for v in (a, b, c):
                k = (q16(v[0]), q16(v[1]), q16(v[2]))
                if k not in vid:
                    vid[k] = len(verts)
                    verts.append(k)
                ids.append(vid[k])
            if len(set(ids)) < 3:
                continue
            tl.append((ids[0], ids[1], ids[2], CAR_MATERIALS.index(mat)))
        out.arr('int16_t', 'pt_%s_v' % name, [x for v in verts for x in v], 12)
        out.arr('uint8_t', 'pt_%s_t' % name, [x for t in tl for x in t], 16)
        out.h.append('#define PT_%s_NV %d' % (name.upper(), len(verts)))
        out.h.append('#define PT_%s_NT %d' % (name.upper(), len(tl)))
        if len(verts) > 255:
            raise SystemExit('car mesh %s has too many vertices' % name)

    # ---------------------------------------------------------------- materials (linear RGB, x 4096)
    base = {}
    for name in ('road', 'road_wide', 'planes', 'blocks', 'pillar', 'signs', 'wall_track'):
        js, _ = load_glb(os.path.join(web, 'models', name + '.glb'))
        for m in js['materials']:
            c = m.get('pbrMetallicRoughness', {}).get('baseColorFactor', [1, 1, 1, 1])
            base.setdefault(m['name'], c[:3])
    base['RoadEdge'] = [(base['RoadEdgeRed'][i] + base['RoadEdgeWhite'][i]) / 2 for i in range(3)]
    envmat = []
    for env in ('Summer', 'Winter', 'Desert'):
        over = envcolors.get(env, {})
        for mname in MATERIALS:
            if mname in over:
                c = hex_linear(over[mname])
            elif mname == 'RoadEdge' and ('RoadEdgeRed' in over or 'RoadEdgeWhite' in over):
                r = hex_linear(over.get('RoadEdgeRed', '%02x%02x%02x' % tuple(0 for _ in range(3))))
                w = hex_linear(over.get('RoadEdgeWhite', 'ffffff'))
                c = [(r[i] + w[i]) / 2 for i in range(3)]
            else:
                c = base[mname]
            envmat.extend(int(round(min(1.0, x) * 4096)) for x in c)
    custom = []
    for h in envcolors['Custom']:
        custom.extend(int(round(x * 4096)) for x in hex_linear(h))
    carbase = {'AirIntake': [0, 0, 0], 'Metal': [0.077, 0.077, 0.077], 'BrakeLight': [0.1, 0, 0],
               'Tire': [0.03, 0.03, 0.03], 'Rim': [0.4, 0.4, 0.4]}

    # ---------------------------------------------------------------- write
    out.h.append('#define PT_NPARTS %d' % len(used))
    out.h.append('#define PT_NTRACKS %d' % len(tracks))
    out.h.append('#define PT_NMAT %d' % len(MATERIALS))
    for i, m in enumerate(MATERIALS):
        out.h.append('#define PT_M_%s %d' % (re.sub(r'(?<!^)(?=[A-Z])', '_', m).upper(), i))
    for i, m in enumerate(CAR_MATERIALS):
        out.h.append('#define PT_CM_%s %d' % (m.upper(), i))
    out.blob('int16_t', 'pt_pool', [x for v in pool for x in v])
    out.blob('uint16_t', 'pt_cvidx', cvidx)
    out.blob('uint8_t', 'pt_rtri', [x & 0xFF for t in rtris for x in t])
    out.c.append('const pt_chunk pt_chunks[%d] = {' % len(chunks))
    for r0, r1, box in chunks:
        out.c.append('  {{{%d, %d, 0, %d, %d}, {%d, %d, 0, %d, %d}}, {%s}},' % (
            r0[0], r0[1], r0[3], r0[2], r1[0], r1[1], r1[3], r1[2], ','.join(str(int(b)) for b in box)))
    out.c.append('};')
    out.h.append('extern const pt_chunk pt_chunks[%d];' % len(chunks))
    out.blob('uint16_t', 'pt_ctri', [x for t in ctris for x in t])
    out.blob('uint32_t', 'pt_cgrid', cgrid)
    out.blob('uint16_t', 'pt_cgidx', cgidx)
    out.c.append('const pt_part pt_parts[PT_NPARTS] = {')
    for p in partrec:
        out.c.append('  {%d, %d, %d, %d, %d, %d, %d, %d, %d, {%s}, {%s}, %d, %d, %d, {%s}, {%s}, {%s}}, /* %s */' % (
            p['vbase'], p['nv'], p['chunk'], p['nchunk'], p['col'], p['ncol'], p['grid'], p['gx'], p['gz'],
            ', '.join(map(str, p['lo'])), ', '.join(map(str, p['hi'])), p['simple'], p['det'], p['custom'],
            ', '.join(fl(v) for v in p['dc']), ', '.join(fl(v) for v in p['ds']),
            ', '.join(fl(v) for v in p['start']), p['name']))
    out.c.append('};')
    out.h.append('extern const pt_part pt_parts[PT_NPARTS];')
    out.arr('uint8_t', 'pt_part_id', [p['id'] for p in partrec], 16)
    out.arr('int8_t', 'pt_orient', [int(x) for m in orients for x in m.flatten()], 18)
    out.blob_raw('pt_inst', 'pt_insts', b''.join(struct.pack('<hhBBBBBB', r[4], r[5], r[0], r[1], r[2], r[3], r[6], 0)
                                                for r in tinst), len(tinst))
    out.blob('uint32_t', 'pt_masks', tmasks)
    out.blob('uint16_t', 'pt_tcells', tcells)
    out.blob_raw('pt_spec', 'pt_specs', b''.join(struct.pack('<HH', *r) for r in tspec), len(tspec))
    out.blob('uint8_t', 'pt_thumbs', tthumb)
    out.c.append('const pt_track pt_tracks[PT_NTRACKS] = {')
    for h in theads:
        out.c.append('  {"%s", %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d},' % (
            h['name'], h['env'], h['sun'], h['inst'], h['ninst'], h['mask'], h['cell'], h['gx0'], h['gz0'], h['gw'],
            h['gh'], h['reach'], h['spec'], h['ncp'], h['nfin'], h['norders'], h['start'], h['bx0'], h['bx1'],
            h['bz0'], h['bz1'], h['thumb'], h['tw'], h['th']))
    out.c.append('};')
    out.h.append('extern const pt_track pt_tracks[PT_NTRACKS];')
    out.arr('uint16_t', 'pt_envmat', envmat, 15)
    out.arr('uint16_t', 'pt_custom', custom, 15)
    out.arr('uint16_t', 'pt_carmat', [int(round(x * 4096)) for m in CAR_MATERIALS[2:] for x in carbase[m]], 15)
    mesh_arrays('body', body)
    mesh_arrays('wheel', wheel)
    mesh_arrays('extra', extra)
    out.c.append('const float pt_hull[%d][3] = {' % len(hull))
    for v in hull:
        out.c.append('  {%s, %s, %s},' % tuple(fl(x) for x in v))
    out.c.append('};')
    out.h.append('extern const float pt_hull[%d][3];' % len(hull))
    out.h.append('#define PT_HULL_N %d' % len(hull))
    out.c.append('const float pt_mountain[%d] = {' % len(mtable))
    for i in range(0, len(mtable), 6):
        out.c.append('  ' + ', '.join(fl(v) for v in mtable[i:i + 6]) + ',')
    out.c.append('};')
    out.h.append('extern const float pt_mountain[%d];' % len(mtable))
    out.h.append('#define PT_MOUNTAIN_N %d' % len(mtable))

    hdr = ['/* PolyTrack\'s data, made by tools/pack.py from Kodub\'s web build of PolyTrack 0.6.3: the official tracks,',
           ' * the track parts and the car (their models simplified), the materials. Do not edit. */',
           '#ifndef PT_DATA_H', '#define PT_DATA_H', '#include <stdint.h>', '',
           '#define PT_FIX %d   /* vertex units a metre */' % int(FIX),
           'typedef struct { uint32_t vfirst; uint8_t nv, pad; uint16_t ntri; uint32_t tfirst; } pt_lod;',
           'typedef struct { pt_lod lod[2]; int16_t box[6]; } pt_chunk;',
           'typedef struct {',
           '  uint16_t vbase, nv;          /* its vertices in pt_pool */',
           '  uint16_t chunk; uint8_t nchunk;',
           '  uint16_t col, ncol;          /* physics triangles in pt_ctri */',
           '  uint16_t grid; uint8_t gx, gz; /* their 5 m grid, from lo */',
           '  int16_t lo[3], hi[3];',
           '  uint8_t simple;              /* one mesh for all: faces can be hidden per instance (pt_masks) */',
           '  uint8_t det;                 /* 1 checkpoint, 2 finish */',
           '  uint8_t custom;              /* takes the custom block colours */',
           '  float dc[3], ds[3];          /* its detector box */',
           '  float start[3];              /* a start: where the car goes */',
           '} pt_part;',
           'typedef struct { int16_t x, z; uint8_t part, orient, color, y, mask, pad; } pt_inst;',
           'extern const uint8_t pt_blob[];   /* data.bin (data.S) */',
           'typedef struct { uint16_t inst, order; } pt_spec;',
           'typedef struct {',
           '  const char *name;',
           '  uint8_t env, sun;',
           '  uint16_t inst, ninst, mask, cell;',
           '  int16_t gx0, gz0; uint8_t gw, gh, reach;',
           '  uint16_t spec; uint8_t ncp, nfin, norders;',
           '  uint16_t start;',
           '  int16_t bx0, bx1, bz0, bz1;',
           '  uint16_t thumb; uint8_t tw, th;',
           '} pt_track;', '']
    open(os.path.join(SRC, 'data.h'), 'w').write('\n'.join(hdr + out.h + ['#endif', '']))
    open(os.path.join(SRC, 'data.bin'), 'wb').write(bytes(out.bin))
    open(os.path.join(SRC, 'data.c'), 'w').write(
        '/* Made by tools/pack.py from Kodub\'s web build of PolyTrack 0.6.3. Do not edit. */\n#include "data.h"\n\n' +
        '\n'.join(out.c) + '\n')
    size = len(pool) * 6 + len(cvidx) * 2 + len(rtris) * 7 + len(chunks) * 28 + len(ctris) * 6 + len(cgrid) * 2 + \
        len(cgidx) * 2 + len(tinst) * 10 + len(tmasks) * 4 + len(tcells) * 2 + len(tthumb)
    print('pool %d verts, render %d tris, physics %d tris, chunks %d, instances %d: data.bin %d bytes' % (
        len(pool), len(rtris), len(ctris), len(chunks), len(tinst), len(out.bin)), file=sys.stderr)


def make_wheel(prims):
    """The wheel (car.glb's Wheel0) at the calculator's size: its tyre as a ten-sided drum of the same radius and
    width, its rim as a ten-sided disc in the rim colour with the hub's spokes alternating in metal."""
    tv = np.concatenate([pr['local'] for pr in prims if pr['material'] == 'Tire'])
    rv = np.concatenate([pr['local'] for pr in prims if pr['material'] == 'Rim'])
    R = float(np.max(np.hypot(tv[:, 1], tv[:, 2])))
    W = float(np.max(np.abs(tv[:, 0])))
    r = float(np.max(np.hypot(rv[:, 1], rv[:, 2])))
    n = 10
    ring = [(math.cos(2 * math.pi * (k + 0.5) / n), math.sin(2 * math.pi * (k + 0.5) / n)) for k in range(n)]
    tris = []
    def P(x, rad, k):
        return np.array([x, rad * ring[k % n][0], rad * ring[k % n][1]])
    for k in range(n):
        a, b = P(W, R, k), P(W, R, k + 1)
        c, d = P(-W, R, k), P(-W, R, k + 1)
        tris.append(('Tire', a, c, b))
        tris.append(('Tire', b, c, d))
    for side in (1, -1):
        x = W * side
        for k in range(n):
            a, b = P(x, R, k), P(x, R, k + 1)
            c, d = P(x, r, k), P(x, r, k + 1)
            if side > 0:
                tris.append(('Tire', a, b, c)); tris.append(('Tire', c, b, d))
            else:
                tris.append(('Tire', a, c, b)); tris.append(('Tire', c, d, b))
        cen = np.array([x, 0.0, 0.0])
        for k in range(n):
            c, d = P(x, r, k), P(x, r, k + 1)
            mat = 'Rim' if k % 2 == 0 else 'Metal'
            tris.append((mat, cen, c, d) if side > 0 else (mat, cen, d, c))
    return tris


def cut(tris, limit=240):
    out, cur, vs = [], [], set()
    for t in tris:
        keys = {tuple(np.round(v * FIX).astype(int)) for v in t[1:]}
        if len(vs | keys) > limit:
            out.append(cur)
            cur, vs = [], set()
        cur.append(t)
        vs |= keys
    if cur:
        out.append(cur)
    return out


def point_in_tri(p, W):
    a, b, c = W
    n = np.cross(b - a, c - a)
    for u, v in ((a, b), (b, c), (c, a)):
        if np.dot(np.cross(v - u, p - u), n) < -1e-6 * np.dot(n, n):
            return False
    return True


if __name__ == '__main__':
    main()
