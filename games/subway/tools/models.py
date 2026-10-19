#!/usr/bin/env python3
"""The people and the dog of Subway Surfers, as meshes for the calculator (used by tools/pack.py).

The people are fitted to the game's own 3D models, seen from the front in their T pose (the Subway Surfers
Wiki's "3D Models" pictures, tools/fetch.py): the picture is cut from its white background, and each part
of the body (the head with its hair, hood or cap, the chest, the hips, the arms, the legs and the shoes) is
a tube of rings that follow its outline, row by row or column by column. The picture itself is then laid
on the front as the texture (one for the body, a finer one for the head), and from behind each part takes
the colour its back has in the game (Jake's grey hood, the Inspector's olive jacket): so the people look
like the game's from every side.

A person has the bones of BONES. Each corner follows one bone, or two at a joint (blended), so elbows and
knees bend smoothly. The dog, which has no T pose picture, is built of rounded shapes on its own bones."""
import math
from pathlib import Path

import numpy as np
from PIL import Image

BONES = ["hips", "chest", "head", "arm_l", "fore_l", "arm_r", "fore_r", "leg_l", "shin_l", "leg_r", "shin_r"]
PARENT = [-1, 0, 1, 1, 3, 1, 5, 0, 7, 0, 9]
B = {n: i for i, n in enumerate(BONES)}
TEX = 255            # (the colour of a textured triangle)
BODY_TEX = (128, 128)
HEAD_TEX = (64, 64)

# Where each person is in its picture (pixels): its middle cx, the rows where its parts meet, the band of
# rows its arms are in, how far from the middle its chest may reach, its elbows and wrists (from the
# middle), and its height in metres. "back" says what colour each part shows from behind: a list of
# (part, from row, to row, the picture's point whose colour it is, or None for the picture itself).
PEOPLE = {
    "jake": dict(src="3D_Models_5_Jakes.jpg", x=(250, 760), cx=502, height=1.75,
                 head=(30, 198), neck=190, chest=(192, 302), waist=302, knee=400, ankle=437,
                 arms=(186, 226), clip=56, elbow=118, wrist=166, depth=dict(head=0.98, chest=0.62, hips=0.72),
                 peak=dict(row=95, colour=(450, 106), size=(0.13, 0.11)),
                 back=[("head", 0, 999, (445, 160)), ("chest", 0, 999, (455, 280)), ("hips", 0, 999, (480, 315)),
                       ("leg", 446, 999, (455, 514))]),
    "tricky": dict(src="3D_Models_5_Trickys.jpg", x=(250, 760), cx=505, height=1.72,
                   head=(18, 200), neck=192, chest=(198, 290), waist=290, knee=420, ankle=468,
                   arms=(182, 210), clip=56, elbow=120, wrist=196, depth=dict(head=0.95, chest=0.7, hips=0.75),
                   hair_depth=(135, 0.16),
                   back=[("head", 72, 999, (430, 160)), ("chest", 0, 258, (505, 240))]),
    "fresh": dict(src="3D_Models_Cathy_and_Fresh.jpg", x=(1200, 1800), cx=1500, height=1.78,
                  head=(26, 206), neck=200, chest=(204, 358), waist=358, knee=425, ankle=478,
                  arms=(222, 250), clip=56, elbow=125, wrist=180, depth=dict(head=0.98, chest=0.62, hips=0.75),
                  back=[("head", 92, 186, (1458, 60)), ("head", 186, 999, (1470, 196))]),
    "spike": dict(src="3D_Models_Amira_and_Spike.jpg", x=(1200, 1800), cx=1498, height=1.85,
                  head=(25, 214), neck=208, chest=(212, 350), waist=350, knee=410, ankle=446,
                  arms=(240, 264), clip=50, elbow=110, wrist=168, depth=dict(head=0.98, chest=0.65, hips=0.75),
                  mohawk=(106, 0.2), side=(104, (1499, 40)),
                  back=[("head", 106, 186, (1484, 111)), ("head", 186, 999, (1470, 205)), ("chest", 0, 999, (1460, 300))]),
    "inspector": dict(src="3D_Models_Ballerina_Tricky_and_Guard_King.jpg", x=(1200, 1800), cx=1498, height=1.95,
                      head=(9, 206), neck=200, chest=(204, 385), waist=385, knee=455, ankle=468,
                      arms=(206, 250), clip=84, elbow=140, wrist=192, depth=dict(head=0.95, chest=0.9, hips=0.8),
                      sym=True, belly=0.05,
                      peak=dict(row=128, colour=(1470, 125), size=(0.15, 0.12)),
                      back=[("head", 0, 118, (1450, 85)), ("head", 118, 999, (1448, 146)), ("chest", 0, 999, (1440, 300))]),
}


# ---------------------------------------------------------------- the pictures
def cut_out(path, x0, x1, tol=20):
    """The picture between columns x0 and x1, and where it is not white background (filled in from the
    edges: white inside the person stays)."""
    a = np.asarray(Image.open(path).convert("RGB")).astype(np.int32)[:, x0:x1]
    white = np.sqrt(((255 - a) ** 2).sum(axis=2)) < tol
    bg = np.zeros_like(white)
    bg[0, :], bg[-1, :], bg[:, 0], bg[:, -1] = white[0, :], white[-1, :], white[:, 0], white[:, -1]
    while True:
        g = bg.copy()
        g[1:, :] |= bg[:-1, :]
        g[:-1, :] |= bg[1:, :]
        g[:, 1:] |= bg[:, :-1]
        g[:, :-1] |= bg[:, 1:]
        g &= white
        if (g == bg).all():
            break
        bg = g
    return a, ~bg


def bleed(rgb, fg, steps=10):
    """The colours of the person spread out over the background (and over its blended edge), so that a
    texture looked up just outside the outline still finds the person's colour."""
    rgb = rgb.astype(np.float64).copy()
    keep = fg.copy()
    # (the outermost pixel is half background)
    inner = keep.copy()
    inner[1:, :] &= keep[:-1, :]
    inner[:-1, :] &= keep[1:, :]
    inner[:, 1:] &= keep[:, :-1]
    inner[:, :-1] &= keep[:, 1:]
    keep = inner
    for _ in range(steps):
        acc = np.zeros_like(rgb)
        cnt = np.zeros(keep.shape)
        for dy, dx in ((1, 0), (-1, 0), (0, 1), (0, -1), (1, 1), (-1, -1), (1, -1), (-1, 1)):
            k = np.roll(np.roll(keep, dy, 0), dx, 1)
            c = np.roll(np.roll(rgb, dy, 0), dx, 1)
            acc += c * k[..., None]
            cnt += k
        new = ~keep & (cnt > 0)
        rgb[new] = acc[new] / cnt[new][:, None]
        keep = keep | new
    return rgb.clip(0, 255).astype(np.uint8)


def runs(line):
    """The runs of True in a line: (start, end) inclusive."""
    out, i, n = [], 0, len(line)
    while i < n:
        if line[i]:
            s = i
            while i < n and line[i]:
                i += 1
            out.append((s, i - 1))
        i += 1
    return out


def run_at(line, c, lo=None, hi=None):
    """The run of True in line that holds c (or the nearest one), cut to [lo, hi]."""
    best, bd = None, 1e9
    for s, e in runs(line):
        d = 0 if s <= c <= e else min(abs(s - c), abs(e - c))
        if d < bd:
            best, bd = (s, e), d
    if best is None:
        return None
    s, e = best
    if lo is not None:
        s = max(s, lo)
    if hi is not None:
        e = min(e, hi)
    return (s, e) if e >= s else None


# ---------------------------------------------------------------- meshes
class Part:
    """Triangles with one texture (or none): corners (position, normal, bones, texture place)."""

    def __init__(self, name, tex=None):
        self.name, self.tex = name, tex
        self.v = []      # [x, y, z, nx, ny, nz, b0, b1, w, u, v]
        self.t = []      # (a, b, c, colour)

    def vert(self, p, bones, uv=(0, 0), n=(0, 0, 0)):
        b0, b1, w = bones
        self.v.append([p[0], p[1], p[2], n[0], n[1], n[2], b0, b1, w, uv[0], uv[1]])
        return len(self.v) - 1


def sub(a, b):
    return (a[0] - b[0], a[1] - b[1], a[2] - b[2])


def cross(a, b):
    return (a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0])


def norm(v):
    l = math.sqrt(v[0] ** 2 + v[1] ** 2 + v[2] ** 2) or 1
    return (v[0] / l, v[1] / l, v[2] / l)


def add_tri(part, a, b, c, col, centre):
    """A triangle turned to face away from centre: the game draws the side whose corners go clockwise on
    the screen, which is the side cross(b - a, c - a) points to."""
    pa, pb, pc = part.v[a][:3], part.v[b][:3], part.v[c][:3]
    n = cross(sub(pb, pa), sub(pc, pa))
    if abs(n[0]) + abs(n[1]) + abs(n[2]) < 1e-12:
        return
    m = tuple((pa[k] + pb[k] + pc[k]) / 3 for k in range(3))
    o = sub(m, centre)
    if n[0] * o[0] + n[1] * o[1] + n[2] * o[2] < 0:
        b, c = c, b
    part.t.append([a, b, c, col])


def smooth_normals(part):
    acc = [[0.0, 0.0, 0.0] for _ in part.v]
    for a, b, c, _ in part.t:
        pa, pb, pc = part.v[a][:3], part.v[b][:3], part.v[c][:3]
        n = cross(sub(pb, pa), sub(pc, pa))
        for k in (a, b, c):
            for q in range(3):
                acc[k][q] += n[q]
    for i, n in enumerate(acc):
        if part.v[i][3:6] == [0, 0, 0] or True:
            part.v[i][3:6] = list(norm(n))


def tube(part, rings, segs, cap0=True, cap1=True):
    """Rings of corners joined into a tube, closed at its ends. A ring: (centre, axis, ra, rb, bones),
    axis 'y' (a ring across the body, ra along x, rb along z) or 'x' (along an arm: ra along y, rb
    along z). Returns the rows of corners (their centres, for turning triangles outwards)."""
    rows, centres = [], []
    for (c, axis, ra, rb, bones) in rings:
        row = []
        for j in range(segs):
            a = 2 * math.pi * j / segs
            s, co = math.sin(a), math.cos(a)
            if axis == "y":
                p = (c[0] + ra * s, c[1], c[2] + rb * co)
            else:
                p = (c[0], c[1] + ra * s, c[2] + rb * co)
            row.append(part.vert(p, bones))
        rows.append(row)
        centres.append(c)
    for i in range(len(rows) - 1):
        cen = tuple((centres[i][k] + centres[i + 1][k]) / 2 for k in range(3))
        for j in range(segs):
            a, b, c, d = rows[i][j], rows[i][(j + 1) % segs], rows[i + 1][(j + 1) % segs], rows[i + 1][j]
            add_tri(part, a, b, c, 0, cen)
            add_tri(part, a, c, d, 0, cen)
    for end, (k, step) in ((cap0, (0, 1)), (cap1, (len(rows) - 1, -1))):
        if not end:
            continue
        c = centres[k]
        # (a flat cap pushed out a little, so that its edge stays round)
        nxt = centres[k + step] if 0 <= k + step < len(centres) else c
        out = sub(c, nxt)
        l = math.sqrt(sum(v * v for v in out)) or 1
        r = min(rings[k][2], rings[k][3]) * 0.35
        tip = tuple(c[q] + out[q] / l * r for q in range(3))
        t = part.vert(tip, rings[k][4])
        for j in range(segs):
            add_tri(part, rows[k][j], rows[k][(j + 1) % segs], t, 0, nxt)
    return rows


def blob(part, c, r, nu, nv, bones, e1=1.0, e2=1.0, rot=None, lat=(-90, 90), keep=None):
    """A squashed sphere (a rounded box when e1, e2 < 1): centre c, radii r."""
    def sp(x, e):
        return math.copysign(abs(x) ** e, x) if x else 0.0
    grid = []
    for i in range(nu + 1):
        la = math.radians(lat[0] + (lat[1] - lat[0]) * i / nu)
        row = []
        for j in range(nv):
            lo = 2 * math.pi * j / nv
            cu, su, cv, sv = math.cos(la), math.sin(la), math.cos(lo), math.sin(lo)
            p = (r[0] * sp(cu, e1) * sp(sv, e2), r[1] * sp(su, e1), r[2] * sp(cu, e1) * sp(cv, e2))
            if rot:
                p = rot(p)
            row.append(part.vert((c[0] + p[0], c[1] + p[1], c[2] + p[2]), bones))
        grid.append(row)
    for i in range(nu):
        for j in range(nv):
            a, b, cc, d = grid[i][j], grid[i][(j + 1) % nv], grid[i + 1][(j + 1) % nv], grid[i + 1][j]
            for t in ((a, b, cc), (a, cc, d)):
                if len(set(t)) < 3:
                    continue
                m = tuple(sum(part.v[k][q] for k in t) / 3 for q in range(3))
                if keep and not keep(m):
                    continue
                add_tri(part, t[0], t[1], t[2], 0, c)


def rotx(a):
    c, s = math.cos(a), math.sin(a)
    return lambda p: (p[0], p[1] * c - p[2] * s, p[1] * s + p[2] * c)


def rotz(a):
    c, s = math.cos(a), math.sin(a)
    return lambda p: (p[0] * c - p[1] * s, p[0] * s + p[1] * c, p[2])


# ---------------------------------------------------------------- a person
class Person:
    def __init__(self, name, spec, ref, lod=0):
        self.name, self.spec, self.lod = name, spec, lod
        x0, x1 = spec["x"]
        self.rgb, self.fg = cut_out(Path(ref) / spec["src"], x0, x1)
        self.x0 = x0
        ys = np.nonzero(self.fg.any(axis=1))[0]
        xs = np.nonzero(self.fg.any(axis=0))[0]
        self.top, self.ground = int(ys[0]), int(ys[-1])
        self.bx0, self.bx1 = int(xs[0]) + x0, int(xs[-1]) + x0
        self.s = spec["height"] / (self.ground - self.top)      # metres a pixel
        self.cx = spec["cx"]
        self.parts = []
        self.palette = {}
        self.colours = bleed(self.rgb, self.fg)
        # the head's picture: its rows, and as wide as it is
        h0, h1 = spec["head"]
        cols = np.nonzero(self.fg[h0:h1].any(axis=0))[0]
        self.hx0, self.hx1, self.hy0, self.hy1 = int(cols[0]) + x0 - 2, int(cols[-1]) + x0 + 2, h0 - 2, h1 + 2

    # pixels <-> metres (the person faces +z; the picture's right is the person's left, -x)
    def X(self, px):
        return -(px - self.cx) * self.s

    def Y(self, py):
        return (self.ground - py) * self.s

    def px(self, X):
        return self.cx - X / self.s

    def py(self, Y):
        return self.ground - Y / self.s

    def row(self, py):
        return self.fg[int(round(py))]

    def col(self, px):
        return self.fg[:, int(round(px)) - self.x0]

    def sample(self, p):
        x, y = p
        a = self.rgb[y - 2:y + 3, x - self.x0 - 2:x - self.x0 + 3].reshape(-1, 3)
        return tuple(int(v) for v in np.median(a, axis=0))

    def colour(self, rgb):
        key = "%02X%02X%02X" % rgb
        if key not in self.palette:
            self.palette[key] = len(self.palette)
        return self.palette[key]

    def uv(self, part, X, Y):
        px, py = self.px(X), self.py(Y)
        if part.tex == "head":
            u = (px - self.hx0) / (self.hx1 - self.hx0) * HEAD_TEX[0]
            v = (py - self.hy0) / (self.hy1 - self.hy0) * HEAD_TEX[1]
            W, H = HEAD_TEX
        else:
            u = (px - self.bx0) / (self.bx1 + 1 - self.bx0) * BODY_TEX[0]
            v = (py - self.top) / (self.ground + 1 - self.top) * BODY_TEX[1]
            W, H = BODY_TEX
        return (max(0, min(W * 4 - 1, round(u * 4))), max(0, min(H * 4 - 1, round(v * 4))))

    def textures(self):
        """The body's and the head's pictures (RGB, the person's colours bled over the background)."""
        img = Image.fromarray(self.colours)
        body = img.crop((self.bx0 - self.x0, self.top, self.bx1 + 1 - self.x0, self.ground + 1)).resize(BODY_TEX, Image.LANCZOS)
        head = img.crop((self.hx0 - self.x0, self.hy0, self.hx1 - self.x0, self.hy1)).resize(HEAD_TEX, Image.LANCZOS)
        return body, head

    # the parts, ring by ring
    def across(self, py0, py1, n, clip=None, cxr=None, lo=None, hi=None):
        """Rings across the body from row py0 to py1: (row, left, right) in pixels, following the outline."""
        out = []
        c = self.cx if cxr is None else cxr
        for i in range(n + 1):
            py = py0 + (py1 - py0) * i / n
            r = run_at(self.row(py), c - self.x0)
            if r is None:
                continue
            a, b = r[0] + self.x0, r[1] + self.x0
            if clip is not None:
                a, b = max(a, c - clip), min(b, c + clip)
            if lo is not None:
                a, b = max(a, lo), min(b, hi)
            if self.spec.get("sym") and cxr is None:
                h = c - a
                a, b = c - h, c + h
            out.append((py, a, b))
            c = (a + b) / 2
        return out

    def build(self):
        sp = self.spec
        X, Y = self.X, self.Y
        dep = sp["depth"]
        joints = {}
        cx = self.cx

        def jt(name, px, py, z=0.0):
            joints[name] = (X(px), Y(py), z)

        # the joints at rest (T pose)
        arm_y = (sp["arms"][0] + sp["arms"][1]) / 2
        jt("hips", cx, (sp["waist"] + self.crotch()) / 2)
        jt("chest", cx, sp["waist"])
        jt("head", cx, sp["neck"])
        for side, sg in (("l", 1), ("r", -1)):
            jt("arm_" + side, cx + sg * sp["clip"] * 0.8, arm_y)
            jt("fore_" + side, cx + sg * sp["elbow"], arm_y)
            lc = self.leg_centre(sg)
            jt("leg_" + side, lc, self.crotch() - 4)
            jt("shin_" + side, lc, sp["knee"])
        self.joints = [joints[b] for b in BONES]

        one = lambda b: (B[b], B[b], 0)
        two = lambda a, b: (B[a], B[b], 128)

        # the head, with its hair, hood or cap
        h0, h1 = sp["head"]
        head = Part("head", "head")
        L = (lambda hi, lo: lo if self.lod else hi)
        rows = self.across(h0 + 1, h1, L(12, 8))
        # (where the head meets the shoulders, the outline takes in the arms: keep to the neck)
        for i in range(len(rows) // 2, len(rows)):
            py, a, b = rows[i]
            pa, pb = rows[i - 1][1], rows[i - 1][2]
            if py > sp["neck"] - 14 and b - a > (pb - pa) * 1.08:
                rows[i] = (py, max(a, pa + 2), min(b, pb - 2))
        rings = []
        for i, (py, a, b) in enumerate(rows):
            rx = (b - a) / 2 * self.s
            rz = rx * dep["head"]
            cz = -0.01
            if "mohawk" in sp and py < sp["mohawk"][0]:
                # a fan of hair over the head, from the forehead to the back of the neck
                u = (sp["mohawk"][0] - py) / (sp["mohawk"][0] - h0)
                rz, cz = max(0.03, sp["mohawk"][1] * math.sqrt(max(0.0, 1 - u * u))), -0.03
            if "hair_depth" in sp and py > sp["hair_depth"][0]:
                rz = min(rz, sp["hair_depth"][1])
            bones = two("head", "chest") if i == len(rows) - 1 else one("head")
            rings.append(((X((a + b) / 2), Y(py), cz), "y", rx, rz, bones))
        tube(head, rings, L(12, 9))
        self.parts.append(head)
        if "peak" in sp:
            pk = sp["peak"]
            r = run_at(self.row(pk["row"]), cx - self.x0)
            hw = (r[1] - r[0]) / 2 * self.s
            cap = Part("peak")
            rz = hw * dep["head"]
            blob(cap, (0, Y(pk["row"]) - 0.01, rz * 0.62 + pk["size"][1] * 0.5), (hw * 0.8, 0.018, pk["size"][1]), 2, L(10, 7),
                 one("head"), rot=rotx(0.12), keep=lambda m: True)
            cap.solid = self.colour(self.sample(pk["colour"]))
            self.parts.append(cap)

        # the chest
        c0, c1 = sp["chest"]
        chest = Part("chest", "body")
        rows = self.across(c0, c1, L(7, 5), clip=sp["clip"])
        rings = []
        for i, (py, a, b) in enumerate(rows):
            rx = (b - a) / 2 * self.s
            t = (py - c0) / (c1 - c0)
            belly = sp.get("belly", 0) * math.sin(math.pi * min(1, t * 1.2))
            bones = two("chest", "hips") if i == len(rows) - 1 else one("chest")
            rings.append(((X((a + b) / 2), Y(py), belly), "y", rx, rx * dep["chest"] + belly, bones))
        tube(chest, rings, L(10, 8))
        self.parts.append(chest)

        # the hips
        hips = Part("hips", "body")
        cr = self.crotch()
        rows = self.across(sp["waist"] - 2, cr + 6, L(3, 2))
        rings = [((X((a + b) / 2), Y(py), 0), "y", (b - a) / 2 * self.s, (b - a) / 2 * self.s * dep["hips"], one("hips"))
                 for (py, a, b) in rows]
        tube(hips, rings, L(10, 8))
        self.parts.append(hips)

        # the legs and shoes
        for side, sg in (("l", 1), ("r", -1)):
            leg = Part("leg_" + side, "body")
            lc = self.leg_centre(sg)
            lo, hi = (cx + 2, cx + 999) if sg > 0 else (cx - 999, cx - 2)
            nt, ns = L(3, 2), L(2, 1)
            ys = [cr - 6] + [cr + (sp["knee"] - cr) * k / nt for k in range(1, nt + 1)] + \
                 [sp["knee"] + (sp["ankle"] - sp["knee"]) * k / ns for k in range(1, ns + 1)]
            rings = []
            c = lc
            for i, py in enumerate(ys):
                r = run_at(self.row(max(py, cr + 1)), c - self.x0)
                a, b = r[0] + self.x0, r[1] + self.x0
                a, b = max(a, lo), min(b, hi)
                c = (a + b) / 2
                rx = (b - a) / 2 * self.s
                if i == 0:
                    bones = two("leg_" + side, "hips")
                elif py < sp["knee"] - 1:
                    bones = one("leg_" + side)
                elif abs(py - sp["knee"]) <= 1:
                    bones = two("leg_" + side, "shin_" + side)
                else:
                    bones = one("shin_" + side)
                rings.append(((X(c), Y(py), 0), "y", rx, rx, bones))
            # the shoe: longer than it is wide, its toe forward
            g = self.ground
            nf = L(4, 3)
            for k in range(1, nf + 1):
                t = k / nf
                py = sp["ankle"] + (g - sp["ankle"]) * min(t, 0.97)
                r = run_at(self.row(py), c - self.x0)
                a, b = r[0] + self.x0, r[1] + self.x0
                a, b = max(a, lo), min(b, hi)
                rx = (b - a) / 2 * self.s
                rz = rx * (1.0 + 0.85 * math.sin(math.pi / 2 * t))
                rings.append(((X((a + b) / 2), Y(py), rx * 0.55 * t), "y", rx, rz, one("shin_" + side)))
            tube(leg, rings, L(8, 6))
            self.parts.append(leg)

        # the arms, column by column from the shoulder to the fingertips
        a0, a1 = sp["arms"]
        for side, sg in (("l", 1), ("r", -1)):
            arm = Part("arm_" + side, "body")
            start = cx + sg * sp["clip"] * 0.75
            col = self.fg[:, :]
            tip = None
            # (where the fingers end)
            x = int(start)
            while 0 <= x - self.x0 < self.fg.shape[1] and run_at(self.col(x)[a0 - 10:a1 + 12], int(arm_y) - a0 + 10) is not None:
                x += sg
            tip = x - sg
            n_up, n_fore, n_hand = L(3, 2), L(3, 2), L(2, 1)
            xs = [start + (cx + sg * sp["elbow"] - start) * k / n_up for k in range(n_up + 1)]
            xs += [cx + sg * (sp["elbow"] + (sp["wrist"] - sp["elbow"]) * k / n_fore) for k in range(1, n_fore + 1)]
            xs += [cx + sg * (sp["wrist"] + (abs(tip - cx) - 2 - sp["wrist"]) * k / n_hand) for k in range(1, n_hand + 1)]
            rings = []
            cy = arm_y
            for i, px in enumerate(xs):
                line = self.col(px)
                r = run_at(line[a0 - 10:a1 + 12], int(cy) - a0 + 10)
                ra, rb = r[0] + a0 - 10, r[1] + a0 - 10
                if i <= 1:
                    ra, rb = max(ra, a0), min(rb, a1)
                cy = (ra + rb) / 2
                ry = (rb - ra) / 2 * self.s
                hand = abs(px - cx) > sp["wrist"] + 1
                rz = ry * (0.55 if hand else 0.92)
                if i == 0:
                    bones = two("arm_" + side, "chest")
                elif abs(px - cx) < sp["elbow"] - 1:
                    bones = one("arm_" + side)
                elif abs(abs(px - cx) - sp["elbow"]) <= 1:
                    bones = two("arm_" + side, "fore_" + side)
                else:
                    bones = one("fore_" + side)
                rings.append(((X(px), Y(cy), 0), "x", ry, rz, bones))
            tube(arm, rings, L(7, 5))
            self.parts.append(arm)

        # texture places, normals, and the colours from behind
        backs = sp.get("back", [])
        for p in self.parts:
            smooth_normals(p)
            for v in p.v:
                v[9], v[10] = self.uv(p, v[0], v[1]) if p.tex else (0, 0)
            for t in p.t:
                if not p.tex:
                    t[3] = p.solid
                    continue
                ps = [p.v[k] for k in t[:3]]
                m = [sum(q[k] for q in ps) / 3 for k in range(3)]
                nz = sum(q[5] for q in ps) / 3
                t[3] = TEX
                nx = sum(q[3] for q in ps) / 3
                if "side" in sp and abs(nx) > 0.45 and p.name == "head" and self.py(m[1]) < sp["side"][0]:
                    t[3] = self.colour(self.sample((sp["side"][1][0], max(sp["side"][1][1], int(self.py(m[1]))))))
                elif nz < -0.15:
                    py = self.py(m[1])
                    for (name, r0, r1, pt) in backs:
                        if p.name.startswith(name) and r0 <= py < r1:
                            t[3] = TEX if pt is None else self.colour(self.sample(pt))
                            break
        return self

    def crotch(self):
        """The first row below the waist where the legs part."""
        y = self.spec["waist"]
        while self.fg[y, self.cx - self.x0] and y < self.ground:
            y += 1
        return y

    def leg_centre(self, sg):
        y = (self.crotch() + self.spec["knee"]) // 2
        r = run_at(self.row(y), self.cx - self.x0 + sg * 30)
        return (r[0] + r[1]) / 2 + self.x0


def person(name, ref, lod=0):
    p = Person(name, PEOPLE[name], ref, lod).build()
    body, head = p.textures()
    pal = [0] * len(p.palette)
    for k, i in p.palette.items():
        pal[i] = int(k, 16)
    return {"name": name, "joints": p.joints, "parent": PARENT, "parts": p.parts, "palette": pal,
            "textures": {"body": body, "head": head}, "obj": p}


# ---------------------------------------------------------------- the dog
DOG_BONES = ["body", "head", "leg_fl", "leg_fr", "leg_bl", "leg_br", "tail"]
DOG_PARENT = [-1, 0, 0, 0, 0, 0, 0]


def dog():
    """The Inspector's dog (a stocky cream bull terrier with a red spiked collar), built of rounded shapes:
    its own bones are the body, the head, four legs and the tail. Its colours are the game's (Dog.png)."""
    pal = [0xE8D3B0, 0xC8A882, 0x5A2A22, 0xC8281E, 0xE4E4EA, 0x2A1410, 0xE89C94, 0xB08A66]
    FUR, FUR2, NOSE, COLLAR, STUD, EYE, EAR, SHADE = range(8)
    joints = [(0, 0.44, 0), (0, 0.6, 0.26), (-0.1, 0.36, 0.17), (0.1, 0.36, 0.17), (-0.09, 0.36, -0.19),
              (0.09, 0.36, -0.19), (0, 0.5, -0.31)]
    parts = []

    def part(name, bone):
        p = Part(name)
        parts.append(p)
        return p

    def one(bone):
        b = DOG_BONES.index(bone)
        return (b, b, 0)

    body = part("body", "body")
    # a deep chest and a smaller rear
    blob(body, (0, 0.45, 0.1), (0.155, 0.165, 0.21), 5, 9, one("body"))
    blob(body, (0, 0.47, -0.15), (0.13, 0.13, 0.17), 4, 8, one("body"))
    body.colour = lambda m, n: SHADE if n[1] < -0.6 else FUR2 if n[1] < -0.2 else FUR
    head = part("head", "head")
    # the long egg-shaped head of a bull terrier, its snout forward and down, the collar
    blob(head, (0, 0.63, 0.41), (0.105, 0.115, 0.2), 6, 9, one("head"), rot=rotx(0.3))
    blob(head, (0, 0.53, 0.26), (0.13, 0.04, 0.12), 2, 10, one("head"))
    for s in (-1, 1):
        for k in range(2):
            a = 0.9 + 1.1 * k
            blob(head, (0.135 * math.sin(a) * s, 0.53, 0.26 + 0.125 * math.cos(a)), (0.02, 0.02, 0.02), 2, 3, one("head"))
    head.colour = lambda m, n: (STUD if m[1] < 0.575 and m[2] < 0.42 and (abs(m[0]) > 0.11 and abs(n[1]) < 0.5) else
                                COLLAR if m[1] < 0.575 and m[2] < 0.42 else FUR)
    # pointed ears standing up, a big dark nose, small angry eyes
    feat = part("face", "head")
    for s in (-1, 1):
        tube(feat, [((0.065 * s, 0.7, 0.32), "y", 0.048, 0.02, one("head")), ((0.08 * s, 0.87, 0.3), "y", 0.006, 0.003, one("head"))],
             5, cap1=False)
        blob(feat, (0.058 * s, 0.68, 0.5), (0.016, 0.013, 0.01), 2, 5, one("head"))
        tube(feat, [((0.06 * s, 0.705, 0.49), "x", 0.006, 0.004, one("head")), ((0.025 * s, 0.695, 0.52), "x", 0.006, 0.004, one("head"))], 4)
    blob(feat, (0, 0.58, 0.6), (0.045, 0.036, 0.035), 3, 6, one("head"))

    def feat_colour(m, n):
        if m[2] > 0.56:
            return NOSE
        if m[1] > 0.72:
            return EAR if n[2] > 0.3 else FUR
        return EYE
    feat.colour = feat_colour
    for leg in ("leg_fl", "leg_fr", "leg_bl", "leg_br"):
        p = part(leg, leg)
        j = joints[DOG_BONES.index(leg)]
        front = leg.startswith("leg_f")
        r = 0.062 if front else 0.058
        rings = [((j[0], j[1] + 0.05, j[2]), "y", r * 1.1, r * 1.3, one(leg)), ((j[0], j[1] - 0.14, j[2]), "y", r * 0.8, r * 0.9, one(leg)),
                 ((j[0], j[1] - 0.29, j[2] + 0.01), "y", r * 0.62, r * 0.7, one(leg)), ((j[0], j[1] - 0.345, j[2] + 0.035), "y", r * 0.72, r * 1.1, one(leg))]
        tube(p, rings, 6)
        p.colour = lambda m, n: FUR2 if n[1] < -0.3 else FUR
    tail = part("tail", "tail")
    j = joints[6]
    tube(tail, [((0, j[1], j[2] + 0.03), "y", 0.028, 0.028, one("tail")), ((0, j[1] + 0.07, j[2] - 0.13), "y", 0.012, 0.012, one("tail"))], 5)
    tail.colour = lambda m, n: FUR
    for p in parts:
        p.tex = None
        smooth_normals(p)
        for t in p.t:
            ps = [p.v[k] for k in t[:3]]
            m = [sum(q[k] for q in ps) / 3 for k in range(3)]
            n = norm([sum(q[3 + k] for q in ps) for k in range(3)])
            t[3] = p.colour(m, n)
    return {"name": "dog", "joints": joints, "parent": DOG_PARENT, "parts": parts, "palette": pal, "textures": {}}


def models(ref):
    """The people (and each again with fewer corners, for when it is small on the screen), and the dog."""
    out = []
    for n in PEOPLE:
        out.append(person(n, ref))
        lo = person(n, ref, 1)
        lo["name"] += "_lo"
        lo["textures"] = None
        out.append(lo)
    return out + [dog()]


if __name__ == "__main__":
    import sys
    for m in models(sys.argv[1] if len(sys.argv) > 1 else Path(__file__).resolve().parent / "ref"):
        nv = sum(len(p.v) for p in m["parts"])
        nt = sum(len(p.t) for p in m["parts"])
        print(m["name"], len(m["parts"]), "parts", nv, "corners", nt, "triangles", len(m["palette"]), "colours",
              max(len(p.v) for p in m["parts"]), "most corners in a part")
