#!/usr/bin/env python3
"""Draws Retro-Go's icon (55x56): the three systems' colours of Retro-Go's tabs (NES red, Game Boy
orange, Game Boy Color blue) in bands, and a white handheld over them, tilted like the tabs' consoles.
Then ../../../tools/round_icons.py gives it NumPlay's corners.

usage: icon.py out.png"""
import math
import sys

from PIL import Image, ImageDraw

SS = 8
W, H = 55, 56
BANDS = [(0xE8, 0x4B, 0x39), (0xE8, 0x81, 0x1F), (0x31, 0x99, 0xDC)]

im = Image.new("RGB", (W * SS, H * SS))
d = ImageDraw.Draw(im)
# diagonal bands, top left to bottom right
for y in range(H * SS):
    for x in range(W * SS):
        t = (x + y) / ((W + H) * SS)
        im.putpixel((x, y), BANDS[min(2, int(t * 3))])
# the handheld, drawn upright then turned
hh = Image.new("RGBA", (W * SS, H * SS), (0, 0, 0, 0))
h = ImageDraw.Draw(hh)
cx, cy = W * SS / 2, H * SS / 2
bw, bh = 26 * SS, 40 * SS
h.rounded_rectangle([cx - bw / 2, cy - bh / 2, cx + bw / 2, cy + bh / 2], radius=4 * SS, fill=(255, 255, 255, 255))
h.rectangle([cx - bw / 2 + 4 * SS, cy - bh / 2 + 4 * SS, cx + bw / 2 - 4 * SS, cy - 2 * SS], fill=(40, 46, 70, 255))
# d-pad and buttons
px, py = cx - 6 * SS, cy + 8 * SS
h.rectangle([px - 4 * SS, py - 1.2 * SS, px + 4 * SS, py + 1.2 * SS], fill=(40, 46, 70, 255))
h.rectangle([px - 1.2 * SS, py - 4 * SS, px + 1.2 * SS, py + 4 * SS], fill=(40, 46, 70, 255))
for bx, by in ((cx + 5 * SS, cy + 9.5 * SS), (cx + 8.5 * SS, cy + 6 * SS)):
    h.ellipse([bx - 2 * SS, by - 2 * SS, bx + 2 * SS, by + 2 * SS], fill=(232, 75, 57, 255))
hh = hh.rotate(-20, resample=Image.BICUBIC, center=(cx, cy))
im.paste(hh, (0, 0), hh)
im = im.resize((W, H), Image.LANCZOS)
im.save(sys.argv[1])
