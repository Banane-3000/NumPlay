#!/usr/bin/env python3
"""Draws src/icon.png (55 x 56): the warehouse keeper next to a wooden crate, between olive brick walls on
the sand floor, in the game's colours (the way Sokoban is usually drawn). Drawn 8 times bigger, then scaled
down; run tools/round_icons.py on it after (NumPlay's rounded corners).

Usage: icon.py [src/icon.png]
"""
import os
import sys

from PIL import Image, ImageDraw

K = 8
W, H = 55 * K, 56 * K
im = Image.new("RGB", (W, H), (0xDE, 0xD6, 0xAE))
d = ImageDraw.Draw(im)


def rgb(v):
    return (v >> 16, v >> 8 & 255, v & 255)


def box(x0, y0, x1, y1, c):
    d.rectangle((x0 * K, y0 * K, x1 * K - 1, y1 * K - 1), fill=rgb(c))


def bricks(y0, y1):
    """Olive bricks, rows of 5 pixels with dark joints, staggered."""
    box(0, y0, 55, y1, 0xA19555)
    for r, y in enumerate(range(y0, y1, 5)):
        box(0, y + 4, 55, y + 5, 0x444024)
        for x in range(-6 + 6 * (r & 1), 55, 12):
            box(x, y, x + 1, y + 4, 0x444024)


bricks(0, 10)
bricks(46, 56)
# the sand's marks
for x, y in ((6, 14), (40, 15), (9, 42), (50, 41), (27, 43)):
    box(x, y, x + 3, y + 1, 0xC6BE95)
# the crate: a black edge, a frame and an X of planks
x0, y0, s = 22, 13, 30
box(x0, y0, x0 + s, y0 + s, 0x000000)
box(x0 + 1, y0 + 1, x0 + s - 1, y0 + s - 1, 0xDA7D00)
box(x0 + 4, y0 + 4, x0 + s - 4, y0 + s - 4, 0xFFB650)
m = s - 8
for k in range(m - 2):
    box(x0 + 4 + k, y0 + 4 + k, x0 + 7 + k, y0 + 7 + k, 0xDA7D00)
    box(x0 + s - 7 - k, y0 + 4 + k, x0 + s - 4 - k, y0 + 7 + k, 0xDA7D00)
# the keeper, facing us, his hands on the crate
c = 12
box(c - 6, 26, c + 11, 31, 0xFFDAAA)   # the arms, out to the crate
box(c - 5, 24, c + 5, 34, 0xDADADA)   # the shirt
box(c + 2, 24, c + 5, 34, 0xC6C6C6)
box(c - 5, 34, c + 5, 41, 0x2895FF)   # the trousers
box(c, 36, c + 1, 41, 0x0061BA)
box(c - 6, 41, c - 1, 43, 0x996D40)   # the shoes
box(c + 1, 41, c + 6, 43, 0x996D40)
box(c - 4, 13, c + 4, 24, 0xFFDAAA)   # the face
box(c - 5, 11, c + 5, 16, 0x000000)   # the hair
box(c - 3, 18, c - 1, 20, 0x000000)   # the eyes
box(c + 1, 18, c + 3, 20, 0x000000)
out = sys.argv[1] if len(sys.argv) > 1 else os.path.join(os.path.dirname(__file__), "..", "src", "icon.png")
im.resize((55, 56), Image.LANCZOS).save(out)
