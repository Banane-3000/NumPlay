#!/usr/bin/env python3
"""Draws src/icon.png (55 x 56): the 1972 arcade Pong court that opens 2 Players: black, its white walls, the
dashed net, both paddles, the square ball and the scores in the arcade's three-by-five block digits. Drawn 8
times bigger, then scaled down; run tools/round_icons.py on it after (NumPlay's rounded corners).

Usage: icon.py [src/icon.png]
"""
import os
import sys

from PIL import Image, ImageDraw

K = 8
im = Image.new("RGB", (55 * K, 56 * K), (0, 0, 0))
d = ImageDraw.Draw(im)


def box(x0, y0, x1, y1):
    d.rectangle((x0 * K, y0 * K, x1 * K - 1, y1 * K - 1), fill=(255, 255, 255))


DIGITS = {1: ["010", "110", "010", "010", "111"], 3: ["111", "001", "111", "001", "111"]}


def digit(n, x, y, s=2):
    for r, row in enumerate(DIGITS[n]):
        for c, on in enumerate(row):
            if on == "1":
                box(x + c * s, y + r * s, x + c * s + s, y + r * s + s)


box(0, 4, 55, 6)    # the walls
box(0, 50, 55, 52)
for y in range(8, 50, 6):  # the net
    box(27, y, 29, y + 3)
digit(3, 15, 10)
digit(1, 34, 10)
box(6, 26, 8, 36)   # the paddles
box(47, 18, 49, 28)
box(36, 30, 38, 32)  # the ball
out = sys.argv[1] if len(sys.argv) > 1 else os.path.join(os.path.dirname(__file__), "..", "src", "icon.png")
im.resize((55, 56), Image.LANCZOS).save(out)
