#!/usr/bin/env python3
"""The README's pictures from tests/play's screenshots: docpics.py OUT.png SHOT.ppm [WIDTH HEIGHT] (smaller: for
thumbnails), kept small with a palette of 256 colours."""
import sys

from PIL import Image

out, src = sys.argv[1], sys.argv[2]
im = Image.open(src).convert("RGB")
if len(sys.argv) > 4:
    im = im.resize((int(sys.argv[3]), int(sys.argv[4])), Image.LANCZOS)
im.quantize(colors=256, method=Image.MEDIANCUT, dither=Image.NONE).save(out, optimize=True)
