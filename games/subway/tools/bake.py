"""The game's detailed models as a few flat faces with their detail painted on, for the calculator.

The trains, barriers and the city along the tracks are made of thousands of small triangles (window frames, sills,
bolts) coloured from a texture atlas. Seen from the tracks they are mostly flat walls, fronts and roofs, so each
model becomes those: the triangles that can be seen from where the camera goes are grouped by the plane they lie
in, each plane's parts become rectangles, and each rectangle gets a picture of everything near its plane, drawn
looking straight at it (with its depth: what stands out covers what is behind). Rectangles where the picture is
empty keep it see-through. Round things (trees, bushes) become one picture facing the camera.

Everything is in the model's units; pictures are packed into one texture per model."""
import math

import numpy as np
from PIL import Image

AXES = np.array([[1, 0, 0], [-1, 0, 0], [0, 1, 0], [0, -1, 0], [0, 0, 1], [0, 0, -1]], float)


class Mesh:
    """triangles to bake: P (m,3,3), UV (m,3,2), material index (m,) into textures (RGBA arrays) with kinds"""

    def __init__(self):
        self.P, self.UV, self.M = [], [], []

    def add(self, pos, uv, faces, mat, offset=(0, 0, 0), keep=None):
        pos = np.asarray(pos, float) + np.asarray(offset, float)
        for f in faces:
            p = pos[f]
            if keep is not None and not keep(p):
                continue
            self.P.append(p)
            self.UV.append(uv[f] if uv is not None else np.zeros((3, 2)))
            self.M.append(mat)

    def arrays(self):
        return np.array(self.P).reshape(-1, 3, 3), np.array(self.UV).reshape(-1, 3, 2), np.array(self.M, int)


def shade(tex, uv, n, light):
    """colours of a triangle's texels: the texture, lit"""
    return light


def sample(tex, u, v):
    th, tw = tex.shape[:2]
    tx = np.floor(np.mod(u, 1.0) * tw).astype(int) % tw
    ty = np.floor((1 - np.mod(v, 1.0)) * th).astype(int) % th
    return tex[ty, tx]


def raster_ortho(P2, Z, UV, mats, textures, kinds, lights, W, H, keep_alpha=False):
    """triangles already in picture space: P2 (m,3,2) texel coordinates (x right, y down), Z (m,3) nearness (larger
    is nearer the viewer); returns RGBA (H,W,4) float and the nearness buffer"""
    img = np.zeros((H, W, 4))
    zb = np.full((H, W), -np.inf)
    shadows = []
    for i in range(len(P2)):
        kind = kinds[mats[i]]
        if kind == "shadow":
            shadows.append(i)
            continue
        _tri(img, zb, P2[i], Z[i], UV[i], textures[mats[i]], kind, lights[i], W, H)
    for i in shadows:
        _tri(img, zb, P2[i], Z[i], UV[i], textures[mats[i]], "shadow", lights[i], W, H)
    return img, zb


def _tri(img, zb, p, z, uv, tex, kind, light, W, H):
    sx, sy = p[:, 0], p[:, 1]
    x0, x1 = int(max(0, math.floor(sx.min()))), int(min(W - 1, math.ceil(sx.max())))
    y0, y1 = int(max(0, math.floor(sy.min()))), int(min(H - 1, math.ceil(sy.max())))
    if x0 > x1 or y0 > y1:
        return
    area = (sx[1] - sx[0]) * (sy[2] - sy[0]) - (sx[2] - sx[0]) * (sy[1] - sy[0])
    if abs(area) < 1e-9:
        return
    xs, ys = np.meshgrid(np.arange(x0, x1 + 1) + 0.5, np.arange(y0, y1 + 1) + 0.5)
    w0 = ((sx[1] - xs) * (sy[2] - ys) - (sx[2] - xs) * (sy[1] - ys)) / area
    w1 = ((sx[2] - xs) * (sy[0] - ys) - (sx[0] - xs) * (sy[2] - ys)) / area
    w2 = 1 - w0 - w1
    # (a little wider than the triangle: thin parts still show, no cracks between triangles)
    e = 0.15 / max(1.0, abs(area) ** 0.5)
    inside = (w0 >= -e) & (w1 >= -e) & (w2 >= -e)
    if not inside.any():
        return
    zz = w0 * z[0] + w1 * z[1] + w2 * z[2]
    zbuf = zb[y0:y1 + 1, x0:x1 + 1]
    u = w0 * uv[0, 0] + w1 * uv[1, 0] + w2 * uv[2, 0]
    v = w0 * uv[0, 1] + w1 * uv[1, 1] + w2 * uv[2, 1]
    col = sample(tex, u, v).astype(float)
    if kind == "shadow":
        m = inside & (zz >= zbuf - 3.0) & (zbuf > -np.inf)
        dark = 1 - 0.45 * col[..., 3] / 255 if tex.shape[2] == 4 else 0.55
        sub = img[y0:y1 + 1, x0:x1 + 1]
        sub[m, :3] *= (dark[m, None] if np.ndim(dark) else dark)
        return
    if kind == "cutout":
        inside &= col[..., 3] > 127
    m = inside & (zz > zbuf)
    if not m.any():
        return
    rgb = col[..., :3] * light
    if kind == "glass":
        rgb = rgb * 0.75 + np.array([150, 190, 225]) * 0.25
    sub = img[y0:y1 + 1, x0:x1 + 1]
    sub[m, :3] = rgb[m]
    sub[m, 3] = 255
    zbuf[m] = zz[m]


def plane_basis(n):
    """(u right, v up) on a plane seen from its front (looking along -n); Unity's axes are left-handed"""
    if abs(n[1]) > 0.9:
        v = np.array([0, 0, 1.0]) if n[1] > 0 else np.array([0, 0, -1.0])
        v = v - n * (v @ n)
    else:
        v = np.array([0, 1.0, 0]) - n * n[1]
    v /= np.linalg.norm(v)
    u = np.cross(v, -n)
    u /= np.linalg.norm(u)
    return u, v


def lighting(n):
    """the game's look: full light from above and the right, a little darker turned away"""
    L = np.array([0.35, 0.8, -0.45])
    L /= np.linalg.norm(L)
    return 0.8 + 0.2 * max(0.0, float(n @ L))


def bake(mesh, textures, kinds, visible, texel=2.0, max_side=128, min_area=12.0, slab=(-8.0, 8.0), major_area=40.0,
         budget=10000, weight=None, atlas_max=(256, 256), name="?", cams=None, plane_cos=0.97, plane_gap=1.2):
    """mesh: Mesh; textures: [RGBA arrays]; kinds: ['opaque'|'cutout'|'glass'|'shadow'|'skip'] per material;
    visible(c, n) -> bool for each triangle (centre, normal). Returns (quads, atlas RGBA image): quads are
    (corners (4,3), uv (4,2) texels in the atlas), clockwise seen from the front"""
    if weight is None:
        weight = lambda n, d: 1.0
    P, UV, M = mesh.arrays()
    if not len(P):
        return [], Image.new("RGBA", (8, 8))
    N = np.cross(P[:, 1] - P[:, 0], P[:, 2] - P[:, 0])
    A = np.linalg.norm(N, axis=1)
    ok = A > 1e-9
    N[ok] /= A[ok, None]
    A = A / 2
    C = P.mean(axis=1)
    lights = np.array([lighting(n) for n in N])
    kind_of = np.array([kinds[m] for m in M])
    draw = ok & (kind_of != "skip")
    vis = np.array([draw[i] and kind_of[i] != "shadow" and visible(C[i], N[i]) for i in range(len(P))])
    # planes: the biggest triangles first, the others join a plane they lie in
    order = np.argsort(-A)
    groups = []   # [normal, offset, [tris], area]
    for i in order:
        if not vis[i]:
            continue
        n = N[i]
        best = None
        for g in groups:
            if g[0] @ n > plane_cos and abs(g[0] @ C[i] - g[1]) < plane_gap:
                best = g
                break
        if best is None:
            nn = n.copy()
            for ax in AXES:
                if ax @ nn > 0.985:
                    nn = ax.copy()
            groups.append([nn, nn @ C[i], [i], A[i]])
        else:
            best[2].append(i)
            best[3] += A[i]
    # the big planes keep their own picture; a small plane's triangles are painted on a big plane they face
    # nearly the same way as, just in front of or behind (bevels, window frames, bolts), or are left out
    major = [g for g in groups if g[3] >= major_area]
    for g in major:
        pts = P[g[2]]
        u_ax, v_ax = plane_basis(g[0])
        g.append((u_ax, v_ax, (pts @ u_ax).min() - 2, (pts @ u_ax).max() + 2, (pts @ v_ax).min() - 2, (pts @ v_ax).max() + 2))
    planes = list(major)
    for g in groups:
        if g[3] >= major_area:
            continue
        keep = []
        for i in g[2]:
            best, bc = None, 1e9
            for mg in major:
                cosang = mg[0] @ N[i]
                if cosang < 0.6:
                    continue
                dist = mg[0] @ C[i] - mg[1]
                if dist < slab[0] or dist > slab[1]:
                    continue
                u_ax, v_ax, a0, a1, b0, b1 = mg[4]
                cu, cv = C[i] @ u_ax, C[i] @ v_ax
                if not (a0 <= cu <= a1 and b0 <= cv <= b1):
                    continue
                cost = abs(dist) + 6 * (1 - cosang)
                if cost < bc:
                    best, bc = mg, cost
            if best is not None:
                best[2].append(i)
            else:
                keep.append(i)
        if keep and sum(A[k] for k in keep) >= min_area * 0.5:
            planes.append([g[0], g[1], keep, sum(A[k] for k in keep)])
    # each plane's parts: rectangles round the pieces that touch (on a grid of 3 units)
    rects = []   # (normal, offset, u axis, v axis, u0, v0, u1, v1, triangles)
    for g in planes:
        n, d, tris = g[0], g[1], g[2]
        u_ax, v_ax = plane_basis(n)
        pts = P[tris]
        pu, pv = pts @ u_ax, pts @ v_ax
        cell = 3.0
        gu0, gv0 = pu.min(), pv.min()
        gw, gh = int((pu.max() - gu0) / cell) + 2, int((pv.max() - gv0) / cell) + 2
        occ = np.zeros((gh, gw), bool)
        for k in range(len(tris)):
            a0, a1 = int((pu[k].min() - gu0) / cell), int((pu[k].max() - gu0) / cell)
            b0, b1 = int((pv[k].min() - gv0) / cell), int((pv[k].max() - gv0) / cell)
            occ[b0:b1 + 1, a0:a1 + 1] = True
        lab = _label(occ)
        for lb in range(1, lab.max() + 1):
            ys, xs = np.nonzero(lab == lb)
            r_u0, r_u1 = gu0 + xs.min() * cell, gu0 + (xs.max() + 1) * cell
            r_v0, r_v1 = gv0 + ys.min() * cell, gv0 + (ys.max() + 1) * cell
            sel = [k for k in range(len(tris)) if (pu[k].max() >= r_u0) and (pu[k].min() <= r_u1) and (pv[k].max() >= r_v0)
                   and (pv[k].min() <= r_v1)]
            if not sel:
                continue
            r_u0, r_u1 = max(r_u0, pu[sel].min()), min(r_u1, pu[sel].max())
            r_v0, r_v1 = max(r_v0, pv[sel].min()), min(r_v1, pv[sel].max())
            if (r_u1 - r_u0) * (r_v1 - r_v0) < min_area:
                continue
            rects.append([n, d, u_ax, v_ax, r_u0, r_v0, r_u1, r_v1, [tris[k] for k in sel]])
    # the most worth seeing first (big, facing where the camera goes); past the budget a rectangle's triangles are
    # painted on a kept one they nearly face the same way as, or left out
    def score(r):
        n, u0, v0, u1, v1 = r[0], r[4], r[5], r[6], r[7]
        return (u1 - u0) * (v1 - v0) * weight(n, r[1])
    rects.sort(key=lambda r: -score(r))
    kept, extra = (rects[:budget], rects[budget:]) if cams is None else (rects[:budget * 3], rects[budget * 3:])
    for r in extra:
        for i in r[8]:
            best, bc = None, 1e9
            for k in kept:
                cosang = k[0] @ N[i]
                if cosang < 0.5:
                    continue
                dist = k[0] @ C[i] - k[1]
                if dist < slab[0] or dist > slab[1]:
                    continue
                cu, cv = C[i] @ k[2], C[i] @ k[3]
                if not (k[4] - 1 <= cu <= k[6] + 1 and k[5] - 1 <= cv <= k[7] + 1):
                    continue
                cost = abs(dist) + 6 * (1 - cosang)
                if cost < bc:
                    best, bc = k, cost
            if best is not None:
                best[8].append(i)
    rects = kept
    # pictures: each rectangle's own triangles, and the shadows on them
    shadow_idx = np.nonzero(draw & (kind_of == "shadow"))[0]
    pics = []
    for n, d, u_ax, v_ax, u0, v0, u1, v1, tris in rects:
        t = texel
        while max(u1 - u0, v1 - v0) / t > max_side:
            t *= 1.25
        tw, th = max(2, int(math.ceil((u1 - u0) / t))), max(2, int(math.ceil((v1 - v0) / t)))
        idx = np.array(sorted(set(tris)) + [k for k in shadow_idx if abs(P[k] @ n - d).min() < 3 and N[k] @ n > 0.3], int)
        dist = P[idx] @ n - d
        pu, pv = P[idx] @ u_ax, P[idx] @ v_ax
        P2 = np.stack([(pu - u0) / t, (v1 - pv) / t], axis=2)
        img, _ = raster_ortho(P2, dist, UV[idx], M[idx], textures, kinds, lights[idx], tw, th)
        if (img[..., 3] > 0).mean() < 0.05:
            continue
        # (only as big as what shows)
        ys, xs = np.nonzero(img[..., 3] > 0)
        x0, x1, y0, y1 = xs.min(), xs.max() + 1, ys.min(), ys.max() + 1
        img = img[y0:y1, x0:x1]
        u0, u1, v0, v1 = u0 + x0 * t, u0 + x1 * t, v1 - y1 * t, v1 - y0 * t
        pics.append((img, n, d, u_ax, v_ax, u0, v0, u1, v1, t))
    if cams is not None:
        pics = _seen(pics, cams, budget)
    # pictures with big empty parts: cut into pieces round what shows (empty texels cost as much as full ones)
    cut = []
    for p in pics:
        cut += _split(p)
    # (big pictures that stay mostly empty, like the overhead wires: left out, they cost a lot for little)
    pics = [p for p in cut if not (p[0].shape[0] * p[0].shape[1] > 200 and _fill(p[0]) < 0.3)]
    # one texture: shelves, tallest first; bigger texels if it does not fit
    return _pack(pics, atlas_max, name)


def _seen(pics, cams, budget, W=160, H=120, f=100.0):
    """the pictures that show the most from the cameras (positions and points they look at), the rest left out:
    each camera draws them all (their see-through texels left out), and a picture counts the pixels where it is
    in front"""
    seen = np.zeros(len(pics))
    for pos, look in cams:
        pos, look = np.asarray(pos, float), np.asarray(look, float)
        fwd = look - pos
        fwd /= np.linalg.norm(fwd)
        right = np.cross([0, 1, 0], fwd)
        right /= np.linalg.norm(right)
        up = np.cross(fwd, right)
        R = np.stack([right, up, fwd])
        zb = np.full((H, W), np.inf)
        ids = np.full((H, W), -1)
        for k, (img, n, d, u_ax, v_ax, u0, v0, u1, v1, t) in enumerate(pics):
            o = n * d
            corners = np.array([o + u_ax * u0 + v_ax * v1, o + u_ax * u1 + v_ax * v1, o + u_ax * u1 + v_ax * v0, o + u_ax * u0 + v_ax * v0])
            uv = np.array([[0, 0], [1, 0], [1, 1], [0, 1]], float)
            for tri in ((0, 1, 2), (0, 2, 3)):
                _id_tri(zb, ids, (corners[list(tri)] - pos) @ R.T, uv[list(tri)], img, k, W, H, f)
        cnt = np.bincount(ids[ids >= 0].ravel(), minlength=len(pics))
        seen += cnt
    order = np.argsort(-seen)
    keep = [i for i in order[:budget] if seen[i] >= 3]
    return [pics[i] for i in sorted(keep, key=lambda i: -seen[i])]


def _id_tri(zb, ids, P, uv, img, k, W, H, f):
    if (P[:, 2] < 0.5).any():
        # (cut at the near plane: keep it simple, the part in front as two triangles is not needed for a count)
        if (P[:, 2] < 0.5).all():
            return
        P = P.copy()
        P[:, 2] = np.maximum(P[:, 2], 0.5)
    sx = W / 2 + P[:, 0] / P[:, 2] * f
    sy = H / 2 - P[:, 1] / P[:, 2] * f
    x0, x1 = int(max(0, np.floor(sx.min()))), int(min(W - 1, np.ceil(sx.max())))
    y0, y1 = int(max(0, np.floor(sy.min()))), int(min(H - 1, np.ceil(sy.max())))
    if x0 > x1 or y0 > y1:
        return
    area = (sx[1] - sx[0]) * (sy[2] - sy[0]) - (sx[2] - sx[0]) * (sy[1] - sy[0])
    if abs(area) < 1e-9:
        return
    xs, ys = np.meshgrid(np.arange(x0, x1 + 1) + 0.5, np.arange(y0, y1 + 1) + 0.5)
    w0 = ((sx[1] - xs) * (sy[2] - ys) - (sx[2] - xs) * (sy[1] - ys)) / area
    w1 = ((sx[2] - xs) * (sy[0] - ys) - (sx[0] - xs) * (sy[2] - ys)) / area
    w2 = 1 - w0 - w1
    inside = (w0 >= 0) & (w1 >= 0) & (w2 >= 0)
    if not inside.any():
        return
    iz = w0 / P[0, 2] + w1 / P[1, 2] + w2 / P[2, 2]
    z = 1 / iz
    pu = (w0 * uv[0, 0] / P[0, 2] + w1 * uv[1, 0] / P[1, 2] + w2 * uv[2, 0] / P[2, 2]) * z
    pv = (w0 * uv[0, 1] / P[0, 2] + w1 * uv[1, 1] / P[1, 2] + w2 * uv[2, 1] / P[2, 2]) * z
    h, w = img.shape[:2]
    tx = np.clip((pu * w).astype(int), 0, w - 1)
    ty = np.clip((pv * h).astype(int), 0, h - 1)
    m = inside & (img[ty, tx, 3] > 0) & (z < zb[y0:y1 + 1, x0:x1 + 1])
    zb[y0:y1 + 1, x0:x1 + 1][m] = z[m]
    ids[y0:y1 + 1, x0:x1 + 1][m] = k


def _close(img):
    """small holes filled with the colour beside them"""
    a = img[..., 3] > 0
    for _ in range(2):
        h, w = a.shape
        nb = np.zeros((h, w), int)
        src = np.zeros(img.shape)
        for dy, dx in ((1, 0), (-1, 0), (0, 1), (0, -1)):
            sh = np.zeros_like(a)
            sh[max(0, dy):h + min(0, dy), max(0, dx):w + min(0, dx)] = a[max(0, -dy):h + min(0, -dy), max(0, -dx):w + min(0, -dx)]
            si = np.zeros(img.shape)
            si[max(0, dy):h + min(0, dy), max(0, dx):w + min(0, dx)] = img[max(0, -dy):h + min(0, -dy), max(0, -dx):w + min(0, -dx)]
            nb += sh
            src = np.where((sh & (src[..., 3] == 0))[..., None], si, src)
        fill = ~a & (nb >= 3)
        img[fill] = src[fill]
        a = img[..., 3] > 0
    return img


def _enclosed(img):
    """holes closed all round (an opening in a wall seen straight on, a frame's middle) filled: with the darkest
    colour round them"""
    a = img[..., 3] > 0
    h, w = a.shape
    out = ~a
    # what of the empty is reached from the edge
    reach = np.zeros((h, w), bool)
    reach[0, :], reach[-1, :], reach[:, 0], reach[:, -1] = out[0, :], out[-1, :], out[:, 0], out[:, -1]
    while True:
        g = reach.copy()
        g[1:, :] |= reach[:-1, :]
        g[:-1, :] |= reach[1:, :]
        g[:, 1:] |= reach[:, :-1]
        g[:, :-1] |= reach[:, 1:]
        g &= out
        if (g == reach).all():
            break
        reach = g
    hole = out & ~reach
    if hole.any() and a.any():
        lum = img[..., :3].sum(axis=2)
        dark = img[a][np.argsort(lum[a])[len(lum[a]) // 10]]
        img[hole] = dark
    return img


def _split(p, depth=0):
    """a picture as pieces round what shows: cut where a cut saves the most empty texels"""
    img, n, d, u_ax, v_ax, u0, v0, u1, v1, t = p
    img = _enclosed(_close(img.copy()))
    a = img[..., 3] > 0
    if not a.any():
        return []
    ys, xs = np.nonzero(a)
    x0, x1, y0, y1 = xs.min(), xs.max() + 1, ys.min(), ys.max() + 1
    img, a = img[y0:y1, x0:x1], a[y0:y1, x0:x1]
    u0, u1, v0, v1 = u0 + x0 * t, u0 + x1 * t, v1 - y1 * t, v1 - y0 * t
    p = (img, n, d, u_ax, v_ax, u0, v0, u1, v1, t)
    h, w = a.shape
    if depth >= 3 or a.mean() > 0.85 or w * h < 64:
        return [p]
    best, bs = None, w * h * 0.8
    def box(m):
        if not m.any():
            return 0
        yy, xx = np.nonzero(m)
        return (xx.max() - xx.min() + 1) * (yy.max() - yy.min() + 1)
    for k in range(1, h):
        sc = box(a[:k]) + box(a[k:])
        if sc < bs:
            best, bs = ("y", k), sc
    for k in range(1, w):
        sc = box(a[:, :k]) + box(a[:, k:])
        if sc < bs:
            best, bs = ("x", k), sc
    if best is None:
        return [p]
    ax, k = best
    if ax == "y":
        top = (img[:k], n, d, u_ax, v_ax, u0, v1 - k * t, u1, v1, t)
        bot = (img[k:], n, d, u_ax, v_ax, u0, v0, u1, v1 - k * t, t)
        return _split(top, depth + 1) + _split(bot, depth + 1)
    left = (img[:, :k], n, d, u_ax, v_ax, u0, v0, u0 + k * t, v1, t)
    right = (img[:, k:], n, d, u_ax, v_ax, u0 + k * t, v0, u1, v1, t)
    return _split(left, depth + 1) + _split(right, depth + 1)


def _label(occ):
    """connected parts of a grid (8 neighbours)"""
    lab = np.zeros(occ.shape, int)
    n = 0
    h, w = occ.shape
    for y in range(h):
        for x in range(w):
            if occ[y, x] and not lab[y, x]:
                n += 1
                stack = [(y, x)]
                lab[y, x] = n
                while stack:
                    cy, cx = stack.pop()
                    for dy in (-1, 0, 1):
                        for dx in (-1, 0, 1):
                            ny, nx = cy + dy, cx + dx
                            if 0 <= ny < h and 0 <= nx < w and occ[ny, nx] and not lab[ny, nx]:
                                lab[ny, nx] = n
                                stack.append((ny, nx))
    return lab


def _pack(pics, atlas_max, name):
    scale = 1.0
    while True:
        imgs = []
        for p in pics:
            img = p[0]
            if scale < 1:
                h, w = img.shape[:2]
                nw, nh = max(2, int(w * scale)), max(2, int(h * scale))
                img = np.asarray(Image.fromarray(np.clip(img, 0, 255).astype(np.uint8), "RGBA").resize((nw, nh), Image.BOX)).astype(float)
            imgs.append(img)
        placed = _shelves([im.shape[1] for im in imgs], [im.shape[0] for im in imgs], *atlas_max)
        if placed is not None:
            break
        scale *= 0.85
    pos, W, H = placed
    atlas = np.zeros((H, W, 4))
    quads = []
    for (img, n, d, u_ax, v_ax, u0, v0, u1, v1, t), im, (x, y) in zip(pics, imgs, pos):
        h, w = im.shape[:2]
        atlas[y:y + h, x:x + w] = im
        o = n * d
        # the face: an octagon round what shows (its corners clockwise seen from the front, as the picture's)
        pts = _octagon(im[..., 3] > 0)
        corners, uvs = [], []
        for px, py in pts:
            uu, vv = u0 + px / w * (u1 - u0), v1 - py / h * (v1 - v0)
            corners.append(o + u_ax * uu + v_ax * vv)
            uvs.append([x + px, y + py])
        quads.append((np.array(corners), np.array(uvs, float)))
    if scale < 1:
        print(f"  {name}: pictures at {scale:.2f}")
    return quads, Image.fromarray(np.clip(atlas, 0, 255).astype(np.uint8), "RGBA")


def _fill(img):
    a = img[..., 3] > 0
    pts = _octagon(a)
    # the octagon's area (shoelace) against the texels that show
    area = 0.0
    for i in range(len(pts)):
        x0, y0 = pts[i - 1]
        x1, y1 = pts[i]
        area += x0 * y1 - x1 * y0
    return a.sum() / max(1.0, abs(area) / 2)


def _octagon(a):
    """the smallest octagon (sides across, up and down, and at 45 degrees) round the texels that show"""
    h, w = a.shape
    if not a.any():
        return [(0, 0), (w, 0), (w, h), (0, h)]
    ys, xs = np.nonzero(a)
    cx = np.concatenate([xs, xs + 1, xs, xs + 1]).astype(float)
    cy = np.concatenate([ys, ys, ys + 1, ys + 1]).astype(float)
    x0, x1, y0, y1 = cx.min(), cx.max(), cy.min(), cy.max()
    s0, s1 = (cx + cy).min(), (cx + cy).max()
    d0, d1 = (cx - cy).min(), (cx - cy).max()
    pts = [(s0 - y0, y0), (d1 + y0, y0), (x1, x1 - d1), (x1, s1 - x1), (s1 - y1, y1), (d0 + y1, y1), (x0, x0 - d0), (x0, s0 - x0)]
    out = []
    for px, py in pts:
        p = (min(max(px, x0), x1), min(max(py, y0), y1))
        if not out or abs(p[0] - out[-1][0]) + abs(p[1] - out[-1][1]) > 1e-6:
            out.append(p)
    if len(out) > 1 and abs(out[0][0] - out[-1][0]) + abs(out[0][1] - out[-1][1]) < 1e-6:
        out.pop()
    # (corners on a straight line between their neighbours are left out)
    k = 0
    while len(out) > 3 and k < len(out):
        a_, b_, c_ = out[k - 1], out[k], out[(k + 1) % len(out)]
        if abs((b_[0] - a_[0]) * (c_[1] - a_[1]) - (c_[0] - a_[0]) * (b_[1] - a_[1])) < 1e-6:
            out.pop(k)
        else:
            k += 1
    return out


def _shelves(ws, hs, max_w, max_h):
    order = sorted(range(len(ws)), key=lambda i: -hs[i])
    for W in (32, 64, 128, 256):
        if W > max_w:
            break
        if max(ws, default=0) > W:
            continue
        pos = [None] * len(ws)
        x = y = sh = 0
        for i in order:
            if x + ws[i] > W:
                x, y, sh = 0, y + sh, 0
            pos[i] = (x, y)
            x += ws[i]
            sh = max(sh, hs[i])
        H = y + sh
        Hp = 8
        while Hp < H:
            Hp *= 2
        if Hp <= max_h and (Hp <= W * 2 or W == max_w):
            return pos, W, Hp
    return None
