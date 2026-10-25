#!/usr/bin/env python3
"""PolyTrack's logo and icon from Kodub's web build (images/logo.svg, images/icon.svg):
logo.py WEB writes src/logo.h (the title's logo) and src/icon.png (the app's icon).

The SVGs are read as data: their groups' transforms, rectangles and straight-edged paths (M L H V Z, the only
commands they have), filled (non-zero winding) and outlined as their styles say, sampled 4 x 4 times a pixel.
The logo keeps 4 bits a pixel: the top one says which colour (the letters' lavender or the outline's dark blue),
the other three how much of the pixel is covered."""
import os
import re
import sys
import xml.etree.ElementTree as ET

import numpy as np
from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
SS = 4


def mat_mul(a, b):
    """2D affine matrices (a, b, c, d, e, f) as in SVG: x' = a x + c y + e, y' = b x + d y + f"""
    return (a[0] * b[0] + a[2] * b[1], a[1] * b[0] + a[3] * b[1], a[0] * b[2] + a[2] * b[3],
            a[1] * b[2] + a[3] * b[3], a[0] * b[4] + a[2] * b[5] + a[4], a[1] * b[4] + a[3] * b[5] + a[5])


def parse_transform(s):
    m = (1, 0, 0, 1, 0, 0)
    for name, args in re.findall(r"(\w+)\(([^)]*)\)", s or ""):
        v = [float(x) for x in re.split(r"[ ,]+", args.strip())]
        if name == "matrix":
            t = tuple(v)
        elif name == "scale":
            t = (v[0], 0, 0, v[1] if len(v) > 1 else v[0], 0, 0)
        elif name == "translate":
            t = (1, 0, 0, 1, v[0], v[1] if len(v) > 1 else 0)
        else:
            raise ValueError("transform " + name)
        m = mat_mul(m, t)
    return m


def style_of(el, inherited):
    st = dict(inherited)
    st.pop("opacity", None)
    for k in ("fill", "stroke", "stroke-width", "fill-rule"):
        if el.get(k):
            st[k] = el.get(k)
    for kv in (el.get("style") or "").split(";"):
        if ":" in kv:
            k, v = kv.split(":", 1)
            st[k.strip()] = v.strip()
    return st


def path_polys(d):
    toks = re.findall(r"[MmLlHhVvZz]|-?(?:\d+\.?\d*|\.\d+)(?:[eE][-+]?\d+)?", d)
    polys, cur, x, y, start, cmd, i = [], [], 0.0, 0.0, (0.0, 0.0), None, 0
    while i < len(toks):
        t = toks[i]
        if re.match(r"[A-Za-z]", t):
            cmd = t
            i += 1
            if cmd in "Zz":
                if cur:
                    polys.append(cur)
                cur, (x, y) = [], start
            continue
        if cmd in "Mm":
            nx, ny = float(toks[i]), float(toks[i + 1])
            i += 2
            if cmd == "m":
                nx, ny = x + nx, y + ny
            if cur:
                polys.append(cur)
            x, y = nx, ny
            cur, start = [(x, y)], (x, y)
            cmd = "l" if cmd == "m" else "L"
        elif cmd in "Ll":
            nx, ny = float(toks[i]), float(toks[i + 1])
            i += 2
            x, y = (x + nx, y + ny) if cmd == "l" else (nx, ny)
            cur.append((x, y))
        elif cmd in "Hh":
            v = float(toks[i])
            i += 1
            x = x + v if cmd == "h" else v
            cur.append((x, y))
        elif cmd in "Vv":
            v = float(toks[i])
            i += 1
            y = y + v if cmd == "v" else v
            cur.append((x, y))
        else:
            raise ValueError("path command " + str(cmd))
    if cur:
        polys.append(cur)
    return polys


def collect(el, m, st, out):
    tag = el.tag.split("}")[-1]
    m = mat_mul(m, parse_transform(el.get("transform")))
    st = style_of(el, st)
    if tag == "rect":
        x, y = float(el.get("x", 0)), float(el.get("y", 0))
        w, h = float(el.get("width")), float(el.get("height"))
        out.append(([[(x, y), (x + w, y), (x + w, y + h), (x, y + h)]], m, st))
    elif tag == "path":
        out.append((path_polys(el.get("d")), m, st))
    for ch in el:
        if ch.tag.split("}")[-1] in ("g", "rect", "path"):
            collect(ch, m, st, out)


def hexc(s):
    s = s.strip().lstrip("#")
    if len(s) == 3:
        s = "".join(c * 2 for c in s)
    return np.array([int(s[i:i + 2], 16) for i in (0, 2, 4)], float) / 255.0


def fill_mask(polys, px, py):
    """non-zero winding of the sample points (px, py) round the closed polygons"""
    wind = np.zeros(px.shape, np.int32)
    for poly in polys:
        n = len(poly)
        for k in range(n):
            (x0, y0), (x1, y1) = poly[k], poly[(k + 1) % n]
            if y0 == y1:
                continue
            up = (y0 <= py) & (py < y1)
            down = (y1 <= py) & (py < y0)
            xc = x0 + (py - y0) * (x1 - x0) / (y1 - y0)
            right = xc > px
            wind += np.where(up & right, 1, 0) - np.where(down & right, 1, 0)
    return wind != 0


def stroke_mask(polys, px, py, w):
    m = np.zeros(px.shape, bool)
    r2 = (w / 2) ** 2
    for poly in polys:
        n = len(poly)
        for k in range(n):
            (x0, y0), (x1, y1) = poly[k], poly[(k + 1) % n]
            dx, dy = x1 - x0, y1 - y0
            l2 = dx * dx + dy * dy
            t = np.clip(((px - x0) * dx + (py - y0) * dy) / l2, 0, 1) if l2 else 0
            ex, ey = x0 + t * dx - px, y0 + t * dy - py
            m |= ex * ex + ey * ey <= r2
    return m


def render(path, width, height=None):
    """the SVG as a (height, width, 4) array, colours and alpha 0..1"""
    root = ET.parse(path).getroot()
    vb = [float(v) for v in root.get("viewBox").split()]
    if height is None:
        height = int(round(vb[3] * width / vb[2]))
    sx, sy = width / vb[2], height / vb[3]
    W, H = width * SS, height * SS
    ys, xs = np.mgrid[0:H, 0:W]
    px = (xs + 0.5) / SS / sx + vb[0]
    py = (ys + 0.5) / SS / sy + vb[1]
    shapes = []
    for ch in root:
        if ch.tag.split("}")[-1] in ("g", "rect", "path"):
            collect(ch, (1, 0, 0, 1, 0, 0), {}, shapes)
    rgb, alpha = np.zeros((H, W, 3)), np.zeros((H, W))
    for polys, m, st in shapes:
        pts = [[(m[0] * x + m[2] * y + m[4], m[1] * x + m[3] * y + m[5]) for x, y in poly] for poly in polys]
        scale = abs(m[0] * m[3] - m[1] * m[2]) ** 0.5
        for kind in ("fill", "stroke"):
            colour = st.get(kind, "#000000" if kind == "fill" else "none")
            if colour == "none":
                continue
            if kind == "fill":
                mask = fill_mask(pts, px, py)
            else:
                mask = stroke_mask(pts, px, py, float(st.get("stroke-width", "1").replace("px", "")) * scale)
            c = hexc(colour)
            rgb[mask] = c
            alpha[mask] = 1.0
    # down to pixels: colours weighted by coverage
    def box(a):
        return a.reshape(height, SS, width, SS, *a.shape[2:]).mean(axis=(1, 3))
    a = box(alpha)
    c = box(rgb * alpha[..., None])
    c = np.where(a[..., None] > 0, c / np.maximum(a[..., None], 1e-9), 0)
    return np.concatenate([c, a[..., None]], axis=2)


def write_logo(path, img):
    h, w = img.shape[:2]
    lav, dark = hexc("#b3c1ff"), hexc("#112052")
    px = []
    for y in range(h):
        for x in range(w):
            c, a = img[y, x, :3], img[y, x, 3]
            level = int(a * 7 + 0.5)
            is_dark = np.sum((c - dark) ** 2) < np.sum((c - lav) ** 2)
            px.append(0 if level == 0 else (level | (8 if is_dark else 0)))
    data = bytes(px[i] | (px[i + 1] << 4 if i + 1 < len(px) else 0) for i in range(0, len(px), 2))
    lines = ["/* PolyTrack's logo, from images/logo.svg of Kodub's web build (tools/logo.py). 4 bits a pixel, two to a",
             " * byte (the first in the low half): bit 3 the dark blue outline (else the lavender), bits 0-2 how much it",
             " * covers (0 none, 7 all). Do not edit. */",
             "#ifndef PT_LOGO_HEADER", "#define PT_LOGO_HEADER", "#include <stdint.h>",
             f"#define PT_LOGO_W {w}", f"#define PT_LOGO_H {h}",
             "static const uint8_t pt_logo[] = {"]
    for i in range(0, len(data), 24):
        lines.append("  " + ", ".join(str(b) for b in data[i:i + 24]) + ",")
    lines += ["};", "#endif", ""]
    open(path, "w").write("\n".join(lines))


def main():
    web = sys.argv[1]
    logo = render(os.path.join(web, "images", "logo.svg"), 232)
    write_logo(os.path.join(HERE, "..", "src", "logo.h"), logo)
    # the icon: PolyTrack's own, its P on dark blue, at the calculator's 55 x 56
    icon = render(os.path.join(web, "images", "icon.svg"), 55, 56)
    Image.fromarray((np.clip(icon, 0, 1) * 255 + 0.5).astype(np.uint8), "RGBA").save(
        os.path.join(HERE, "..", "src", "icon.png"))
    if os.environ.get("LOGO_PNG"):
        bg = np.array([90, 140, 200], float) / 255
        out = logo[..., :3] * logo[..., 3:] + bg * (1 - logo[..., 3:])
        Image.fromarray((out * 255).astype(np.uint8)).resize((logo.shape[1] * 3, logo.shape[0] * 3),
                                                            Image.NEAREST).save(os.environ["LOGO_PNG"])
    print(f"logo: {logo.shape[1]} x {logo.shape[0]}, icon: 55 x 56")


if __name__ == "__main__":
    main()
