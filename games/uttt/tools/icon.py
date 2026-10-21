#!/usr/bin/env python3
"""Draws src/icon.png (55 x 56): the big board, X on the diagonal through its middle, O in a corner.

Usage: python3 tools/icon.py (from games/uttt)"""
from PIL import Image, ImageDraw

S = 8  # drawn 8 times bigger, then scaled down smoothly
W, H = 55, 56
NAVY, PLATE, SQUARE = (22, 26, 51), (46, 54, 94), (78, 89, 145)
RED, BLUE = (255, 77, 109), (62, 197, 255)


def mix(a, b, t):
    return tuple(round(x * t + y * (1 - t)) for x, y in zip(a, b))


im = Image.new("RGBA", (W * S, H * S), (0, 0, 0, 0))
d = ImageDraw.Draw(im)
d.rounded_rectangle((0, 0, W * S - 1, H * S - 1), radius=12 * S, fill=NAVY)
pitch, size, x0, y0 = 16, 14, 5, 5.5
for b in range(9):
    bx, by = (x0 + b % 3 * pitch) * S, (y0 + b // 3 * pitch) * S
    won = {4: RED, 2: BLUE, 6: RED}.get(b)
    d.rounded_rectangle((bx, by, bx + size * S, by + size * S), radius=3 * S, fill=mix(won, PLATE, 0.35) if won else PLATE)
    if won == RED:
        t = 2.2 * S
        for a, c in (((bx + 3.5 * S, by + 3.5 * S), (bx + 10.5 * S, by + 10.5 * S)),
                     ((bx + 3.5 * S, by + 10.5 * S), (bx + 10.5 * S, by + 3.5 * S))):
            d.line((a, c), fill=RED, width=round(t))
            for p in (a, c):
                d.ellipse((p[0] - t / 2, p[1] - t / 2, p[0] + t / 2, p[1] + t / 2), fill=RED)
    elif won == BLUE:
        d.ellipse((bx + 2.5 * S, by + 2.5 * S, bx + 11.5 * S, by + 11.5 * S), outline=BLUE, width=round(2.2 * S))
    else:  # its nine cells
        for c in range(9):
            cx, cy = bx + (1.5 + c % 3 * 4) * S, by + (1.5 + c // 3 * 4) * S
            d.rounded_rectangle((cx, cy, cx + 3 * S, cy + 3 * S), radius=S, fill=SQUARE)
im.resize((W, H), Image.LANCZOS).save("src/icon.png")
