#!/usr/bin/env python3
"""Draws src/icon.png (55 x 56): the keeper pushing a crate onto a goal, between brick walls, in the
game's colours. Drawn 8 times bigger, then scaled down.

Usage: icon.py [src/icon.png]
"""
import os
import sys

from PIL import Image, ImageDraw

K = 8
W, H = 55 * K, 56 * K
im = Image.new("RGBA", (W, H), (0, 0, 0, 0))
d = ImageDraw.Draw(im)
cell = 13 * K
x0, y0 = (W - 4 * cell) // 2, (H - 4 * cell) // 2


def rgb(v):
    return (v >> 16, v >> 8 & 255, v & 255, 255)


# the floor, a checkerboard, inside a rounded tile of walls
d.rounded_rectangle((0, 0, W - 1, H - 1), radius=12 * K, fill=rgb(0x8C5A3A))
for r in range(4):
    for c in range(4):
        x, y = x0 + c * cell, y0 + r * cell
        if r in (0, 3) or c in (0, 3):  # bricks around
            d.rectangle((x, y, x + cell - 1, y + cell - 1), fill=rgb(0x5B3A26))
            d.rectangle((x, y, x + cell - K - 1, y + cell - K - 1), fill=rgb(0xB57D55))
            d.rectangle((x + K, y + K, x + cell - K - 1, y + cell - K - 1), fill=rgb(0x8C5A3A))
        else:
            d.rectangle((x, y, x + cell - 1, y + cell - 1), fill=rgb(0xE4D7BC if (r ^ c) & 1 else 0xDCCDAF))
# a crate pushed onto its goal, the keeper behind it looking at it; below, a crate and a goal to go
def crate(x, y, dark, face):
    d.rounded_rectangle((x + K, y + K, x + cell - K, y + cell - K), radius=3 * K, fill=rgb(dark))
    d.rounded_rectangle((x + 3 * K, y + 3 * K, x + cell - 3 * K, y + cell - 3 * K), radius=2 * K, fill=rgb(face))
    d.rectangle((x + 3 * K, y + cell // 2 - K, x + cell - 3 * K, y + cell // 2 + K), fill=rgb(dark))


crate(x0 + 2 * cell, y0 + cell, 0x3F7A2C, 0x5DAE45)
crate(x0 + cell, y0 + 2 * cell, 0x8A5422, 0xD6943E)
gx, gy = x0 + 2 * cell + cell // 2, y0 + 2 * cell + cell // 2
d.ellipse((gx - 3 * K, gy - 3 * K, gx + 3 * K, gy + 3 * K), fill=rgb(0xD2452F))
x, y = x0 + cell, y0 + cell
cx, cy, rr = x + cell // 2, y + cell // 2, cell * 3 // 8
d.ellipse((cx - rr, cy - rr + K, cx + rr, cy + rr + K), fill=rgb(0x1D3F8A))
d.ellipse((cx - rr, cy - rr, cx + rr, cy + rr), fill=rgb(0x3C7BE8))
for e in (-1, 1):
    ex, ey = cx + e * cell // 7 + 2 * K, cy - cell // 12
    d.ellipse((ex - 2 * K, ey - 2 * K, ex + 2 * K, ey + 2 * K), fill=(255, 255, 255, 255))
    d.ellipse((ex, ey - K, ex + 2 * K, ey + K), fill=rgb(0x1C1A20))
out = sys.argv[1] if len(sys.argv) > 1 else os.path.join(os.path.dirname(__file__), "..", "src", "icon.png")
im.resize((55, 56), Image.LANCZOS).save(out)
