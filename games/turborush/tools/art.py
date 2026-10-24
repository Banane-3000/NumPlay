#!/usr/bin/env python3
"""Packs Turbo Rush's pictures (art/, indexed PNGs) into C: art.c and art.h in OUT.

  art.py ART_DIR OUT_DIR

- cars.png: the six car pictures side by side (96 x 64: the coupe turning left, straight, right, then the
  supercar), index 63 transparent; palette entries 0-23 are the body paint from darkest to lightest, recoloured
  here for each of the eight paints, 24-31 the lights;
- scenery/*.png: the roadside pictures, each with its own palette, index 63 transparent;
- sky/*.png: the four 400 x 100 backdrops, 64 colours each.

Each sprite also gets two smaller copies (half and quarter size) so that far away it doesn't shimmer. Everything
here is integer arithmetic, so the output is the same on every computer.
"""
import sys
from pathlib import Path

import numpy as np
from PIL import Image

T = 63
SCENERY = ["palm", "pine", "rock", "tower", "sign", "cactus", "villa", "lamp"]
SKIES = ["coast", "alps", "desert", "city"]
# the paints, as the body's middle shade (sRGB)
PAINTS = [(206, 26, 30), (240, 186, 30), (30, 96, 206), (26, 150, 84), (226, 226, 220), (126, 70, 196),
          (236, 110, 24), (52, 56, 66)]


def load(path):
    im = Image.open(path)
    assert im.mode == "P", path
    pal = im.getpalette()[:192]
    pal += [0] * (192 - len(pal))
    return np.array(im, dtype=np.int64), np.array(pal, dtype=np.int64).reshape(64, 3)


def rgb565(c):
    r, g, b = (int(v) for v in c)
    return (r >> 3) << 11 | (g >> 2) << 5 | b >> 3


def lum(c):
    return (299 * int(c[0]) + 587 * int(c[1]) + 114 * int(c[2])) // 1000


def nearest(colors, pal, allowed):
    """For each colour (N x 3), the closest palette entry among `allowed` (lowest index on ties)."""
    cand = pal[allowed]
    d = ((colors[:, None, :] - cand[None, :, :]) ** 2).sum(-1)
    return np.array(allowed)[d.argmin(1)]


def shrink(idx, pal, classes=None):
    """Half size: each 2 x 2 block becomes opaque when at least two of its pixels are, with the average colour of
    those, matched in the block's majority class (classes: an entry -> class table, the body paint averaging its
    shade instead)."""
    h, w = idx.shape
    h2, w2 = (h + 1) // 2, (w + 1) // 2
    pad = np.full((h2 * 2, w2 * 2), T, np.int64)
    pad[:h, :w] = idx
    out = np.full((h2, w2), T, np.int64)
    for y in range(h2):
        for x in range(w2):
            block = [v for v in pad[2 * y:2 * y + 2, 2 * x:2 * x + 2].flatten() if v != T]
            if len(block) < 2:
                continue
            if classes is None:
                c = pal[block].sum(0) // len(block)
                out[y, x] = nearest(c[None, :], pal, list(range(T)))[0]
                continue
            cls = [classes[v] for v in block]
            best = max(set(cls), key=lambda k: (cls.count(k), -k))
            mine = [v for v in block if classes[v] == best]
            if best == 0:   # the paint: its shade
                out[y, x] = (sum(mine) + len(mine) // 2) // len(mine)
            else:
                c = pal[mine].sum(0) // len(mine)
                out[y, x] = nearest(c[None, :], pal, [i for i in range(T) if classes[i] == best])[0]
    return out


class Out:
    def __init__(self):
        self.c, self.h = [], []

    def array(self, kind, name, values, per=24):
        vals = [int(v) for v in values]
        self.c.append(f"const {kind} {name}[{len(vals)}] = {{")
        for i in range(0, len(vals), per):
            self.c.append("  " + ",".join(str(v) for v in vals[i:i + per]) + ",")
        self.c.append("};")
        self.h.append(f"extern const {kind} {name}[{len(vals)}];")


def sprite(out, name, idx, pal, classes=None):
    """A sprite and its two smaller copies: pixels row by row, and each row's first and last opaque pixel."""
    levels = [idx]
    for _ in range(2):
        levels.append(shrink(levels[-1], pal, classes))
    mips = []
    for k, im in enumerate(levels):
        h, w = im.shape
        spans = []
        for row in im:
            xs = np.where(row != T)[0]
            spans += [int(xs[0]), int(xs[-1]) + 1] if len(xs) else [0, 0]
        out.array("uint8_t", f"{name}_px{k}", im.flatten())
        out.array("uint8_t", f"{name}_sp{k}", spans)
        mips.append(f"{{{w}, {h}, {name}_px{k}, {name}_sp{k}}}")
    return "{{" + ", ".join(mips) + "}}"


def main():
    art, dst = Path(sys.argv[1]), Path(sys.argv[2])
    dst.mkdir(parents=True, exist_ok=True)
    out = Out()
    out.c.append("/* Made by tools/art.py from the pictures in art/. */")
    out.c.append('#include "art.h"')

    # cars
    idx, pal = load(art / "cars.png")
    assert idx.shape == (64, 96 * 6)
    classes = [0] * 24 + [1] * 8 + [2] * (T - 32) + [3]
    cars = [sprite(out, f"car{k}", idx[:, k * 96:(k + 1) * 96], pal, classes) for k in range(6)]
    out.c.append(f"const art_sprite art_cars[6] = {{{', '.join(cars)}}};")
    shades = [lum(pal[k]) for k in range(24)]
    mid, top = shades[12], shades[23]
    paints = []
    for p in PAINTS:
        row = []
        for k in range(64):
            if k < 24:
                c = []
                for ch in range(3):
                    if shades[k] <= mid:
                        v = p[ch] * shades[k] // mid
                    else:
                        t = min(1000, (shades[k] - mid) * 1000 // max(1, top - mid))
                        v = p[ch] + (255 - p[ch]) * t * 8 // 10000
                    c.append(max(0, min(255, v)))
                row.append(rgb565(c))
            else:
                row.append(rgb565(pal[k]))
        paints += row
    out.array("uint16_t", "art_car_pal", paints, 16)
    out.h.append("#define ART_PAINTS 8")

    # scenery
    entries, pals, glow = [], [], []
    for name in SCENERY:
        idx, pal = load(art / "scenery" / f"{name}.png")
        entries.append(sprite(out, f"sc_{name}", idx, pal))
        pals += [rgb565(c) for c in pal]
        # lit windows and lamps: warm, bright and saturated; they stay lit at night
        bits = 0
        for k in range(T):
            r, g, b = (int(v) for v in pal[k])
            if r > 190 and g > 150 and b < 140 and r - b > 90:
                bits |= 1 << k
        glow.append(bits)
    out.c.append(f"const art_sprite art_scenery[{len(SCENERY)}] = {{{', '.join(entries)}}};")
    out.array("uint16_t", "art_scenery_pal", pals, 16)
    out.c.append("const uint64_t art_scenery_glow[%d] = {%s};" % (len(glow), ", ".join(f"{g}u" for g in glow)))
    out.h.append(f"extern const uint64_t art_scenery_glow[{len(glow)}];")

    # skies
    pals = []
    for k, name in enumerate(SKIES):
        idx, pal = load(art / "sky" / f"{name}.png")
        assert idx.shape == (100, 400), name
        out.array("uint8_t", f"art_sky{k}", idx.flatten(), 40)
        pals += [rgb565(c) for c in pal]
    out.c.append("const uint8_t *const art_sky[4] = {art_sky0, art_sky1, art_sky2, art_sky3};")
    out.array("uint16_t", "art_sky_pal", pals, 16)

    head = ["/* Made by tools/art.py. */", "#ifndef TR_ART_H", "#define TR_ART_H", "#include <stdint.h>",
            "#define ART_T 63          /* transparent */",
            "typedef struct { uint8_t w, h; const uint8_t *px, *span; } art_img;",
            "typedef struct { art_img mip[3]; } art_sprite;",
            "extern const art_sprite art_cars[6];", f"extern const art_sprite art_scenery[{len(SCENERY)}];",
            "extern const uint8_t *const art_sky[4];"]
    (dst / "art.h").write_text("\n".join(head + out.h + ["#endif"]) + "\n")
    (dst / "art.c").write_text("\n".join(out.c) + "\n")


if __name__ == "__main__":
    main()
