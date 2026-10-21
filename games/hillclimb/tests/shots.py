#!/usr/bin/env python3
"""Puts the host's screenshots (PPM) side by side in one PNG, 2x: shots.py OUT.png a.ppm b.ppm ..."""
import sys

from PIL import Image

out, files = sys.argv[1], sys.argv[2:]
ims = [Image.open(f).convert("RGB") for f in files]
cols = 2 if len(ims) > 1 else 1
rows = (len(ims) + cols - 1) // cols
sheet = Image.new("RGB", (320 * 2 * cols + 8 * (cols - 1), 240 * 2 * rows + 8 * (rows - 1)), (255, 255, 255))
for i, im in enumerate(ims):
    sheet.paste(im.resize((640, 480), Image.NEAREST), ((i % cols) * 648, (i // cols) * 488))
sheet.save(out)
