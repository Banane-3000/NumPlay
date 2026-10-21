#!/usr/bin/env python3
"""Puts screenshots (PPM or PNG) side by side, scaled up: sheet.py OUT.png COLS FILE..."""
import sys

from PIL import Image

out, cols, files = sys.argv[1], int(sys.argv[2]), sys.argv[3:]
ims = [Image.open(f).convert("RGB") for f in files]
w, h = ims[0].size
rows = (len(ims) + cols - 1) // cols
sheet = Image.new("RGB", (cols * w, rows * h))
for i, im in enumerate(ims):
    sheet.paste(im, ((i % cols) * w, (i // cols) * h))
scale = 2 if cols * w <= 700 else 1
sheet.resize((sheet.width * scale, sheet.height * scale), Image.NEAREST).save(out)
