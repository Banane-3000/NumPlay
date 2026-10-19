#!/usr/bin/env python3
"""Draws src/icon.png, the calculator's 55x56 icon: a subway train coming head-on down its track under a
blue sky, the way the game's trains look. Drawn 8x larger, then scaled down.

Usage: icon.py src/icon.png"""
import sys

from PIL import Image, ImageDraw

S = 8
W, H = 55 * S, 56 * S


def main(out):
    im = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    d = ImageDraw.Draw(im)
    # the rounded square: sky above, gravel below
    mask = Image.new("L", (W, H), 0)
    ImageDraw.Draw(mask).rounded_rectangle((0, 0, W - 1, H - 1), radius=12 * S, fill=255)
    bg = Image.new("RGBA", (W, H))
    bd = ImageDraw.Draw(bg)
    for y in range(H):
        t = y / H
        if t < 0.62:
            c = (int(60 + 140 * t), int(159 + 90 * t), int(232 + 20 * t))
        else:
            c = (134, 115, 95)
        bd.line((0, y, W, y), fill=c + (255,))
    # rails going away from the eye
    for x0, x1 in ((14, 22), (41, 33)):
        bd.polygon([(x0 * S - S, H), (x0 * S + S, H), (x1 * S + S // 2, int(0.62 * H)), (x1 * S - S // 2, int(0.62 * H))],
                   fill=(201, 207, 214, 255))
    im.paste(bg, (0, 0), mask)
    # the train's face: yellow, a dark roof edge, two windows, two lamps
    x0, x1, y0, y1 = 9 * S, 46 * S, 8 * S, 50 * S
    d.rounded_rectangle((x0, y0, x1, y1), radius=6 * S, fill=(11, 42, 85, 255))
    d.rounded_rectangle((x0 + S, y0 + S, x1 - S, y1 - S), radius=5 * S, fill=(242, 194, 48, 255))
    d.rectangle((x0 + S, y0 + 30 * S, x1 - S, y0 + 33 * S), fill=(46, 95, 168, 255))
    for wx0, wx1 in ((13, 26), (29, 42)):
        d.rounded_rectangle((wx0 * S, 14 * S, wx1 * S, 27 * S), radius=2 * S, fill=(43, 66, 96, 255))
        d.polygon([(wx0 * S + 2 * S, 15 * S), (wx0 * S + 6 * S, 15 * S), (wx0 * S + 2 * S, 22 * S)], fill=(120, 170, 220, 255))
    for lx in (15, 40):
        d.ellipse((lx * S - 3 * S, 41 * S - 3 * S, lx * S + 3 * S, 41 * S + 3 * S), fill=(255, 250, 220, 255))
    d.rectangle((23 * S, 10 * S, 32 * S, 12 * S), fill=(30, 30, 36, 255))
    small = im.resize((55, 56), Image.LANCZOS)
    small.save(out)


if __name__ == "__main__":
    main(sys.argv[1])
