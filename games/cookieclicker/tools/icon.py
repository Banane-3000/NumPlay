#!/usr/bin/env python3
"""src/icon.png: the big cookie on Cookie Clicker's blue, as the calculator's home screen shows apps."""
import os

from PIL import Image, ImageDraw

S = 8  # drawn 8 times larger, then scaled down smoothly
W, H = 55, 56
img = Image.new("RGBA", (W * S, H * S), (0, 0, 0, 0))
d = ImageDraw.Draw(img)
d.rounded_rectangle([0, 0, W * S - 1, H * S - 1], radius=12 * S, fill=(38, 74, 116, 255))
d.rounded_rectangle([0, 0, W * S - 1, H * S // 2], radius=12 * S, fill=(52, 96, 146, 255))
d.rectangle([0, H * S // 4, W * S - 1, H * S // 2], fill=(52, 96, 146, 255))
cx, cy, r = 27.5 * S, 28 * S, 22 * S


def disc(x, y, rr, fill):
    d.ellipse([x - rr, y - rr, x + rr, y + rr], fill=fill)


disc(cx, cy + 1.5 * S, r, (60, 32, 12, 120))  # its shadow
disc(cx, cy, r, (156, 91, 38, 255))
disc(cx - 0.5 * S, cy - 0.5 * S, r - 1.6 * S, (217, 162, 94, 255))
disc(cx - 5 * S, cy - 5 * S, r * 0.5, (232, 186, 124, 255))
for x, y, rr in [(-10, -11, 4.2), (6, -14, 3.2), (14, -1, 4.2), (-15, 3, 3.2), (-3, 2, 4.6), (8, 10, 3.6),
                 (-7, 13, 3.6), (1, -6, 2.4)]:
    disc(cx + x * S, cy + y * S, rr * S, (75, 38, 17, 255))
    disc(cx + (x - rr / 3) * S, cy + (y - rr / 3) * S, rr * S / 3, (120, 72, 40, 255))
img = img.resize((W, H), Image.LANCZOS)
img.save(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "src", "icon.png"))
