#!/usr/bin/env python3
"""Makes src/icon.png, the calculator's 55x56 icon: the game's own app icon (Jake with his spray can), from its
Android package's adaptive icon (its background and foreground layers), the part a phone shows, full to the
edges. tools/round_icons.py at the root gives it NumPlay's rounded corners afterwards.

Usage: icon.py PACKAGE src/icon.png   (the folder the package was unzipped into; it is only read)"""
import sys
from pathlib import Path

from PIL import Image


def main(package, out):
    res = Path(package) / "res" / "mipmap-xxxhdpi-v4"
    bg = Image.open(res / "ic_launcher_background.png").convert("RGBA")
    fg = Image.open(res / "ic_launcher_foreground.png").convert("RGBA")
    im = bg.copy()
    im.alpha_composite(fg.resize(bg.size, Image.LANCZOS))
    # an adaptive icon is 108 units across, the middle 72 shown
    w, h = im.size
    k = w * 18 // 108
    im = im.crop((k, k, w - k, h - k))
    im.resize((55, 56), Image.LANCZOS).save(out)


if __name__ == "__main__":
    main(sys.argv[1], sys.argv[2])
