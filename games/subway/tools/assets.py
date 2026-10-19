#!/usr/bin/env python3
"""Makes src/assets.c: Subway Surfers' fonts and logo, from Lilita One.

Lilita One (Juan Montoreano, SIL Open Font License 1.1) is the round, heavy
lettering the game's own buttons and numbers look like. Its glyphs are
rendered 4x supersampled with the baseline on a pixel boundary and kept as
4-bit coverage, two pixels a byte (the launcher's fonts do the same). Each
letter also comes grown by 1 and by 2 pixels all round and 1 more below: the
game's lettering is white with a dark outline and a shadow, drawn in two.

The logo, "SUBWAY SURFERS" on two lines, is drawn from the same font: filled
with the game's yellow, a thick navy outline and a shadow under it, tilted a
little. It is kept as one byte a pixel, its alpha (4 bits) and its colour
(4 bits, from a palette of 16), in runs: a count, then the byte, row by row.

The output is committed; run it again only to change sizes or colours.
Usage: assets.py src/assets.c
"""
import hashlib
import sys
import urllib.request
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw, ImageFilter, ImageFont

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


# ---------------------------------------------------------------- the logo
NAVY = (22, 34, 92)
SHADOW = (10, 16, 48)


def word(text, cap, top, bottom, outline):
    """A word drawn big (4x), filled with a gradient from top to bottom, its outline around it."""
    up = 4
    f = load(cap, up)
    bb = f.getbbox(text, anchor="ls")
    pad = (outline + 4) * up
    w, h = bb[2] - bb[0] + 2 * pad, bb[3] - bb[1] + 2 * pad
    mask = Image.new("L", (w, h), 0)
    ImageDraw.Draw(mask).text((pad - bb[0], pad - bb[1]), text, font=f, fill=255, anchor="ls")
    fill = np.asarray(mask).astype(np.float32) / 255
    ring = np.asarray(mask.filter(ImageFilter.MaxFilter(2 * outline * up + 1))).astype(np.float32) / 255
    ys = np.linspace(0, 1, h)[:, None]
    top, bottom = np.array(top, np.float32), np.array(bottom, np.float32)
    grad = top[None, None, :] * (1 - ys[..., None]) + bottom[None, None, :] * ys[..., None]
    # a light band near the top of the letters, as on the game's lettering
    shine = np.clip(1 - np.abs(ys - 0.33) * 9, 0, 1)[..., None] * 0.35
    grad = grad * (1 - shine) + 255 * shine
    rgb = np.zeros((h, w, 3), np.float32) + np.array(NAVY, np.float32)
    rgb = rgb * (1 - fill[..., None]) + grad * fill[..., None]
    img = np.dstack([rgb, ring * 255]).astype(np.uint8)
    return Image.fromarray(img, "RGBA")


def logo(lines):
    up = 4
    a = word("SUBWAY", 30, (255, 255, 255), (150, 220, 255), 3)
    b = word("SURFERS", 34, (255, 238, 60), (255, 150, 20), 3)
    w = max(a.width, b.width)
    gap = -6 * up
    canvas = Image.new("RGBA", (w + 24 * up, a.height + b.height + gap + 24 * up), (0, 0, 0, 0))
    canvas.alpha_composite(a, ((canvas.width - a.width) // 2, 8 * up))
    canvas.alpha_composite(b, ((canvas.width - b.width) // 2, 8 * up + a.height + gap))
    # the shadow, down and to the right
    alpha = canvas.getchannel("A")
    shadow = Image.new("RGBA", canvas.size, SHADOW + (0,))
    shadow.putalpha(alpha.point(lambda v: v * 0.7))
    out = Image.new("RGBA", canvas.size, (0, 0, 0, 0))
    out.alpha_composite(shadow, (3 * up, 4 * up))
    out.alpha_composite(canvas)
    out = out.rotate(5, resample=Image.BICUBIC, expand=True)
    out = out.resize((out.width // up, out.height // up), Image.LANCZOS)
    box = out.getbbox()
    out = out.crop(box)
    px = np.asarray(out).astype(np.int32)
    # 15 colours for the opaque parts (pngquant would do as well; this keeps it to Pillow)
    opaque = px[..., 3] > 0
    rgb = Image.fromarray(px[..., :3].astype(np.uint8), "RGB")
    pal_img = rgb.quantize(colors=15, method=Image.MEDIANCUT, dither=Image.NONE)
    pal = pal_img.getpalette()[:45]
    idx = np.asarray(pal_img).astype(np.int32)
    alpha4 = np.clip(np.round(px[..., 3] / 255 * 15), 0, 15).astype(np.int32)
    alpha4[~opaque] = 0
    vals = (alpha4 << 4) | np.where(alpha4 > 0, idx, 0)
    h, w = vals.shape
    data = bytearray()
    rows = []
    for y in range(h):
        rows.append(len(data))
        row = vals[y].tolist()
        x = 0
        while x < w:
            v = row[x]
            n = 1
            while x + n < w and row[x + n] == v and n < 255:
                n += 1
            data += bytes([n, v])
            x += n
    rows.append(len(data))
    colours = []
    for i in range(15):
        r, g, bl = pal[3 * i:3 * i + 3]
        colours.append(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (bl >> 3))
    lines.append(f"#define LOGO_W {w}")
    lines.append(f"#define LOGO_H {h}")
    lines.append(f"const uint16_t logo_w = {w}, logo_h = {h};")
    lines.append("const uint16_t logo_pal[16] = {" + ", ".join(f"0x{c:04X}" for c in colours + [0]) + "};")
    lines.append(f"const uint16_t logo_rows[{len(rows)}] = {{")
    for i in range(0, len(rows), 16):
        lines.append("  " + ",".join(str(r) for r in rows[i:i + 16]) + ",")
    lines.append("};")
    lines.append(f"const uint8_t logo_runs[{len(data)}] = {{")
    for i in range(0, len(data), 32):
        lines.append("  " + ",".join(str(v) for v in data[i:i + 32]) + ",")
    lines.append("};")
    print(f"logo: {w}x{h}, {len(data)} bytes of runs")
    preview = Image.fromarray(np.dstack([px[..., :3], px[..., 3]]).astype(np.uint8), "RGBA")
    return preview, len(data) + 2 * len(rows)


def main(out):
    lines = ["/* Generated by tools/assets.py from Lilita One by Juan Montoreano (SIL Open Font License 1.1). */",
             '#include "assets.h"', ""]
    total = fonts(lines)
    preview, n = logo(lines)
    total += n
    Path(out).write_text("\n".join(lines) + "\n")
    if len(sys.argv) > 2:
        bg = Image.new("RGBA", preview.size, (90, 180, 240, 255))
        bg.alpha_composite(preview)
        bg.save(sys.argv[2])
    print(f"total {total} bytes")


if __name__ == "__main__":
    main(sys.argv[1])
