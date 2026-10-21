#!/usr/bin/env python3
"""Makes a NumWorks app icon from any picture, the way every NumPlay icon is: 55 x 56 pixels, NumPlay's
rounded corners (tools/make_icon.corner_mask, a radius of 12) and white outside them, as the calculator's
home screen is white.

For a port, start from the original game's own app icon or key art, so it is recognised at a glance.

  python3 tools/icon.py picture.png games/mygame/src/icon.png
  python3 tools/icon.py picture.png out.png --fit           the whole picture, on --background (white)
  python3 tools/icon.py picture.png out.png --crop 10,0,90,100   a part of it first (left,top,right,bottom %)

Python 3 with Pillow; macOS, Linux and Windows.
"""
import argparse
import os
import sys

from PIL import Image, ImageChops

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from make_icon import corner_mask  # noqa: E402

W, H = 55, 56


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("picture")
    ap.add_argument("out")
    ap.add_argument("--fit", action="store_true", help="fit the whole picture instead of filling the icon")
    ap.add_argument("--background", default="ffffff", help="behind a fitted or transparent picture (hex)")
    ap.add_argument("--crop", help="left,top,right,bottom in percent of the picture, before anything else")
    a = ap.parse_args()
    im = Image.open(a.picture).convert("RGBA")
    if a.crop:
        l, t, r, b = (float(v) / 100 for v in a.crop.split(","))
        im = im.crop((round(l * im.width), round(t * im.height), round(r * im.width), round(b * im.height)))
    bg = tuple(int(a.background[i:i + 2], 16) for i in (0, 2, 4)) + (255,)
    scale = (min if a.fit else max)(W / im.width, H / im.height)
    big = im.resize((max(1, round(im.width * scale)), max(1, round(im.height * scale))), Image.LANCZOS)
    canvas = Image.new("RGBA", (W, H), bg)
    canvas.alpha_composite(big, ((W - big.width) // 2, (H - big.height) // 2)) if a.fit else \
        canvas.alpha_composite(big.crop(((big.width - W) // 2, (big.height - H) // 2,
                                         (big.width - W) // 2 + W, (big.height - H) // 2 + H)))
    alpha = ImageChops.darker(canvas.getchannel("A"), corner_mask(W, H))
    flat = Image.new("RGB", (W, H), (255, 255, 255))
    flat.paste(canvas.convert("RGB"), (0, 0), alpha)
    flat.putalpha(alpha)
    flat.save(a.out, optimize=True)
    print(f"{a.out}: {W}x{H}, NumPlay's corners")


if __name__ == "__main__":
    main()
