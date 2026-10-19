#!/usr/bin/env python3
"""Makes src/assets.c: Subway Surfers' fonts, from Lilita One.

Lilita One (Juan Montoreano, SIL Open Font License 1.1) is the round, heavy
lettering the game's own buttons and numbers look like. Its glyphs are
rendered 4x supersampled with the baseline on a pixel boundary and kept as
4-bit coverage, two pixels a byte (the launcher's fonts do the same). Each
letter also comes grown by 1 and by 2 pixels all round and 1 more below: the
game's lettering is white with a dark outline and a shadow, drawn in two.

(The logo is the game's own: tools/pack.py.)

The output is committed; run it again only to change sizes or colours.
Usage: assets.py src/assets.c
"""
import hashlib
import sys
import urllib.request
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parent
CACHE = ROOT / ".fonts"
UP = 4
SOURCE = ("LilitaOne.ttf", "https://raw.githubusercontent.com/google/fonts/main/ofl/lilitaone/LilitaOne-Regular.ttf",
          "f5b641c45c69d772ee4eda687bc9fda411d5cad6b0b45371491da4580cbc8d59")

# name, cap height in pixels, extra letter spacing (1/4 px)
FONTS = [
    ("font_big", 19, 2),
    ("font_mid", 12, 1),
    ("font_small", 8, 2),
]
FIRST, LAST = 32, 126


def font_file():
    name, url, digest = SOURCE
    p = CACHE / name
    if not p.exists():
        CACHE.mkdir(exist_ok=True)
        data = urllib.request.urlopen(url).read()
        if hashlib.sha256(data).hexdigest() != digest:
            raise SystemExit("unexpected font data")
        p.write_bytes(data)
    return str(p)


def load(cap, up=UP):
    for size in range(6, 400):
        f = ImageFont.truetype(font_file(), size * up)
        bb = f.getbbox("H", anchor="ls")
        if -bb[1] >= cap * up - up // 2:
            return f
    raise SystemExit("size")


def glyph(f, ch, spacing, outline=0):
    """A letter's 4-bit coverage, its offset from the pen and its advance. With outline, the letter grown
    by that many pixels all round and one more below (its outline and shadow, drawn under it)."""
    adv = (f.getlength(ch) + spacing) / UP
    bb = f.getbbox(ch, anchor="ls")
    if bb[2] <= bb[0] or bb[3] <= bb[1]:
        return None, round(adv)
    pad = (2 + outline + 1) * UP
    x0 = pad - bb[0]
    y0 = pad - bb[1]
    x0 += (-x0) % UP
    y0 += (-y0) % UP
    w = x0 + bb[2] + pad
    h = y0 + bb[3] + pad
    w += (-w) % UP
    h += (-h) % UP
    im = Image.new("L", (w, h), 0)
    ImageDraw.Draw(im).text((x0, y0), ch, font=f, fill=255, anchor="ls")
    a = np.asarray(im).astype(np.float32) / 255
    if outline:
        r = outline * UP
        g = np.zeros_like(a)
        for dy in range(-r, r + 1):
            for dx in range(-r, r + 1):
                if dx * dx + dy * dy <= r * r + r:
                    g = np.maximum(g, np.roll(np.roll(a, dy, 0), dx, 1))
        a = np.maximum(g, np.roll(g, UP, 0))
    a = a.reshape(h // UP, UP, w // UP, UP).mean(axis=(1, 3))
    q = np.clip(np.round(a * 15), 0, 15).astype(np.uint8)
    ys, xs = np.nonzero(q)
    if not len(ys):
        return None, round(adv)
    q = q[ys.min():ys.max() + 1, xs.min():xs.max() + 1]
    ox = xs.min() - x0 // UP
    oy = ys.min() - y0 // UP  # top relative to the baseline
    return (q, ox, oy), round(adv)


def table(lines, name, f, spacing, outline):
    data = bytearray()
    glyphs = []
    asc = desc = 0
    for c in range(FIRST, LAST + 1):
        g, adv = glyph(f, chr(c), spacing * UP / 4, outline)
        if g is None:
            glyphs.append((0, 0, 0, 0, adv, len(data)))
            continue
        q, ox, oy = g
        h, w = q.shape
        asc = max(asc, -oy)
        desc = max(desc, h + oy)
        flat = q.flatten().tolist()
        if len(flat) % 2:
            flat.append(0)
        off = len(data)
        for i in range(0, len(flat), 2):
            data.append(flat[i] | flat[i + 1] << 4)
        glyphs.append((w, h, ox, oy, adv, off))
    lines.append(f"static const uint8_t {name}_data[{len(data)}] = {{")
    for i in range(0, len(data), 24):
        lines.append("  " + ",".join(str(b) for b in data[i:i + 24]) + ",")
    lines.append("};")
    lines.append(f"static const Glyph {name}_glyphs[{len(glyphs)}] = {{")
    for w, h, ox, oy, adv, off in glyphs:
        lines.append(f"  {{{w}, {h}, {ox}, {oy}, {adv}, {off}}},")
    lines.append("};")
    return asc, desc, len(data) + 7 * len(glyphs)


def fonts(lines):
    total = 0
    for name, cap, spacing in FONTS:
        f = load(cap)
        asc, desc, n = table(lines, name, f, spacing, 0)
        total += n
        for t in (1, 2):
            total += table(lines, f"{name}_o{t}", f, spacing, t)[2]
        lines.append(f"const Font {name} = {{{FIRST}, {LAST - FIRST + 1}, {asc}, {desc}, {name}_glyphs, {name}_data,")
        lines.append(f"  {{{name}_o1_glyphs, {name}_o2_glyphs}}, {{{name}_o1_data, {name}_o2_data}}}};")
        lines.append("")
        print(f"{name}: cap {cap}px, ascent {asc}, descent {desc}")
    return total


def main(out):
    lines = ["/* Made by tools/assets.py from Lilita One by Juan Montoreano (SIL Open Font License 1.1). */",
             '#include "assets.h"', ""]
    total = fonts(lines)
    Path(out).write_text("\n".join(lines) + "\n")
    print(f"total {total} bytes")


if __name__ == "__main__":
    main(sys.argv[1])
