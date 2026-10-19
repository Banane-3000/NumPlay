#!/usr/bin/env python3
"""Makes src/data.c and src/data.h: the pictures and the models of Subway Surfers.

- The people (tools/models.py): meshes fitted to the game's own 3D models, with their pictures as textures
  (a body's and a finer head's), and the dog.
- The logo, and the icons of coins, keys, power-ups and items from the Subway Surfers Wiki (gathered by
  tools/fetch.py): for the screens (rows of runs of pixels, with their alpha) and as textures for the
  pickups in the world.

Textures are palette indices (a palette each, its first colour the average: far away a texture is only
that); index 0 is see-through in the pickups'.
Usage: pack.py [REF]   (the folder tools/fetch.py filled)"""
import os
import sys
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import models  # noqa: E402

ROOT = Path(__file__).resolve().parent.parent
REF = Path(sys.argv[1]) if len(sys.argv) > 1 else ROOT / "tools" / "ref"


def c565(r, g, b):
    return ((int(r) & 0xF8) << 8) | ((int(g) & 0xFC) << 3) | (int(b) >> 3)


class Out:
    def __init__(self):
        self.c = ["/* Made by tools/pack.py from the Subway Surfers Wiki's pictures (tools/fetch.py). */",
                  '#include "data.h"', ""]
        self.h = ["/* Subway Surfers: pictures and models (data.c, made by tools/pack.py). */", "#ifndef SS_DATA_H",
                  "#define SS_DATA_H", '#include "models.h"', ""]
        self.textures = []   # names, in r_tex order
        self.bytes = 0

    def array(self, ctype, name, values, per=24):
        self.c.append(f"static const {ctype} {name}[{len(values)}] = {{")
        for i in range(0, len(values), per):
            self.c.append("  " + ",".join(str(int(v)) for v in values[i:i + per]) + ",")
        self.c.append("};")
        self.bytes += len(values) * (1 if "8" in ctype else 2 if "16" in ctype else 4)

    def texture(self, name, rgba, clear=False, colors=64):
        """A texture from a picture whose sides are powers of two."""
        w, h = rgba.size
        assert w & (w - 1) == 0 and h & (h - 1) == 0, (name, w, h)
        px = np.asarray(rgba.convert("RGBA")).astype(np.int32)
        alpha = px[..., 3]
        first = 2 if clear else 1          # (0: see-through; then the average)
        n = colors - first
        rgb = Image.fromarray(px[..., :3].astype(np.uint8), "RGB")
        q = rgb.quantize(colors=n, method=Image.Quantize.MEDIANCUT, dither=Image.Dither.NONE)
        pal = q.getpalette()[:3 * n]
        idx = np.asarray(q).astype(np.int32) + first
        seen = alpha >= 128
        if clear:
            idx[~seen] = 0
        avg = px[..., :3][seen].mean(axis=0) if seen.any() else np.array([128, 128, 128])
        palette = ([0] if clear else []) + [c565(*avg)] + [c565(*pal[3 * i:3 * i + 3]) for i in range(n)]
        self.array("uint8_t", f"{name}_px", idx.flatten().tolist())
        self.array("uint16_t", f"{name}_pal", palette, 12)
        wl, hl = w.bit_length() - 1, h.bit_length() - 1
        self.c.append(f"static const Texture {name} = {{{wl}, {hl}, {1 if clear else 0}, 0, {name}_px, {name}_pal}};")
        self.c.append("")
        self.textures.append(name)
        return len(self.textures) - 1

    def image(self, name, rgba, colors=32):
        """A picture for the screens: rows of runs (count, alpha 0..7 << 5 | colour 0..31)."""
        px = np.asarray(rgba.convert("RGBA")).astype(np.int32)
        h, w = px.shape[:2]
        assert w < 256 and h < 256
        a8 = np.clip(np.round(px[..., 3] / 255 * 7), 0, 7).astype(np.int32)
        rgb = Image.fromarray(px[..., :3].astype(np.uint8), "RGB")
        q = rgb.quantize(colors=colors, method=Image.Quantize.MEDIANCUT, dither=Image.Dither.NONE)
        pal = q.getpalette()[:3 * colors]
        pal += [0] * (3 * colors - len(pal))
        idx = np.asarray(q).astype(np.int32)
        vals = np.where(a8 > 0, a8 << 5 | idx, 0)
        runs, rows = [], []
        for y in range(h):
            rows.append(len(runs))
            row = vals[y].tolist()
            x = 0
            while x < w:
                v, n = row[x], 1
                while x + n < w and row[x + n] == v and n < 255:
                    n += 1
                runs += [n, v]
                x += n
        rows.append(len(runs))
        self.array("uint8_t", f"{name}_runs", runs, 32)
        self.array("uint16_t", f"{name}_rows", rows, 16)
        self.array("uint16_t", f"{name}_pal", [c565(*pal[3 * i:3 * i + 3]) for i in range(colors)], 12)
        self.c.append(f"const Image {name} = {{{w}, {h}, {name}_pal, {name}_rows, {name}_runs}};")
        self.c.append("")
        self.h.append(f"extern const Image {name};")


# ---------------------------------------------------------------- icons
def icon(src, size, pad=0.0):
    im = Image.open(src).convert("RGBA")
    im = im.crop(im.getbbox())
    w, h = im.size
    s = size * (1 - pad) / max(w, h)
    im = im.resize((max(1, round(w * s)), max(1, round(h * s))), Image.LANCZOS)
    out = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    out.alpha_composite(im, ((size - im.width) // 2, (size - im.height) // 2))
    return out


def sneaker_icon(size):
    """Super Sneakers: a sneaker with an up arrow (the game's pickup), drawn at 4 times the size."""
    S = size * 4
    im = Image.new("RGBA", (S, S), (0, 0, 0, 0))
    d = ImageDraw.Draw(im)
    k = S / 32
    d.polygon([(2 * k, 25 * k), (4 * k, 12 * k), (12 * k, 10 * k), (16 * k, 16 * k), (28 * k, 18 * k), (30 * k, 25 * k)],
              fill=(120, 40, 120))
    d.polygon([(3.5 * k, 24 * k), (5.5 * k, 13.5 * k), (11.5 * k, 12 * k), (15 * k, 17.5 * k), (27 * k, 19.5 * k), (28.5 * k, 24 * k)],
              fill=(214, 70, 200))
    d.rectangle((2 * k, 24 * k, 30 * k, 29 * k), fill=(250, 246, 236))
    d.rectangle((2 * k, 27.5 * k, 30 * k, 29 * k), fill=(190, 182, 170))
    d.polygon([(22 * k, 2 * k), (29 * k, 9 * k), (25 * k, 9 * k), (25 * k, 16 * k), (19 * k, 16 * k), (19 * k, 9 * k), (15 * k, 9 * k)],
              fill=(120, 220, 60), outline=(30, 110, 20))
    return im.resize((size, size), Image.LANCZOS)


# ---------------------------------------------------------------- models
def pack_models(o):
    names = []
    tex = {}
    for m in models.models(REF):
        name = m["name"]
        if m["textures"] is not None:
            # (a model with fewer corners shares its pictures)
            tex = {}
            for k, img in m["textures"].items():
                tex[k] = o.texture(f"tx_{name}_{k}", img.convert("RGBA"), colors=160 if k == "body" else 128)
        verts, tris, parts = [], [], []
        for p in m["parts"]:
            assert len(p.v) < 256, (name, p.name, len(p.v))
            v0, t0 = len(verts), len(tris)
            for (x, y, z, nx, ny, nz, b0, b1, w, u, v) in p.v:
                verts.append((round(x * 1000), round(y * 1000), round(z * 1000), round(nx * 127), round(ny * 127),
                              round(nz * 127), w, u, v, b0, b1))
            tris += [tuple(t) for t in p.t]
            parts.append((v0, len(p.v), t0, len(p.t), tex[p.tex] if p.tex else -1))
        o.c.append(f"static const MV {name}_v[{len(verts)}] = {{")
        for i in range(0, len(verts), 4):
            o.c.append("  " + " ".join("{%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d}," % v for v in verts[i:i + 4]))
        o.c.append("};")
        o.c.append(f"static const Tri {name}_t[{len(tris)}] = {{")
        for i in range(0, len(tris), 8):
            o.c.append("  " + " ".join("{%d,%d,%d,%d}," % t for t in tris[i:i + 8]))
        o.c.append("};")
        o.c.append(f"static const MPart {name}_p[{len(parts)}] = {{" + " ".join("{%d,%d,%d,%d,%d}," % p for p in parts) + "};")
        o.array("uint16_t", f"{name}_pal", [c565((c >> 16) & 255, (c >> 8) & 255, c & 255) for c in m["palette"]] or [0], 12)
        o.c.append(f"static const int16_t {name}_j[{len(m['joints'])}][3] = {{" +
                   ", ".join("{%d,%d,%d}" % tuple(round(v * 1000) for v in j) for j in m["joints"]) + "};")
        o.c.append(f"static const int8_t {name}_parent[{len(m['parent'])}] = {{" + ",".join(map(str, m["parent"])) + "};")
        o.c.append(f"const Model model_{name} = {{{name}_p, {name}_v, {name}_t, {name}_pal, {name}_j, {name}_parent, "
                   f"{len(parts)}, {len(m['joints'])}}};")
        o.c.append("")
        o.h.append(f"extern const Model model_{name};")
        o.bytes += len(verts) * 16 + len(tris) * 4
        names.append(name)
        print(f"{name}: {len(verts)} corners, {len(tris)} triangles")


def main():
    o = Out()
    pack_models(o)
    # pickups in the world
    for key, f in (("coin", "Coin.png"), ("key", "Key.png"), ("magnet", "Coin_Magnet.png"), ("jetpack", "Jetpack.png"),
                   ("double", "2x_Multiplier.png"), ("mystery", "Mystery_Box.png")):
        o.texture(f"tx_{key}", icon(REF / f, 32), clear=True, colors=48)
    o.texture("tx_sneakers", sneaker_icon(32), clear=True, colors=32)
    # the screens' icons, at the sizes they are drawn
    for key, f in (("coin", "Coin.png"), ("key", "Key.png"), ("double", "2x_Multiplier.png"), ("magnet", "Coin_Magnet.png"),
                   ("jetpack", "Jetpack.png"), ("mystery", "Mystery_Box.png"), ("board", "Hoverboard.png"),
                   ("headstart", "Headstart.png"), ("booster", "Score_Booster.png")):
        o.image(f"im_{key}", icon(REF / f, 24))
    o.image("im_sneakers", sneaker_icon(24))
    o.image("im_coin_s", icon(REF / "Coin.png", 16))
    o.image("im_key_s", icon(REF / "Key.png", 16))
    # the logo
    lg = Image.open(REF / "Logo_001.png").convert("RGBA")
    lg = lg.crop(lg.getbbox())
    o.image("im_logo", lg.resize((190, round(lg.height * 190 / lg.width)), Image.LANCZOS), colors=32)
    # the textures' table
    o.h.append("")
    o.h.append("enum {")
    for i, n in enumerate(o.textures):
        o.h.append(f"  {n.upper()} = {i},")
    o.h.append("};")
    o.c.append(f"const Texture *const r_tex[{len(o.textures)}] = {{" + ", ".join("&" + n for n in o.textures) + "};")
    o.c.append("")
    o.h += ["#endif", ""]
    (ROOT / "src" / "data.c").write_text("\n".join(o.c) + "\n")
    (ROOT / "src" / "data.h").write_text("\n".join(o.h) + "\n")
    print(f"data: {o.bytes} bytes")


if __name__ == "__main__":
    main()
