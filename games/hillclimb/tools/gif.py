#!/usr/bin/env python3
"""Frames of tests/play (PPM screenshots) into a GIF with one palette for all of them.

    python3 -I tools/gif.py OUT.gif FPS [--scale 2] a.ppm b.ppm ...    (or folders: their frames in order)"""
import glob
import os
import sys

from PIL import Image


def main():
    args = sys.argv[1:]
    out, fps = args[0], float(args[1])
    scale = 1
    if args[2] == "--scale":
        scale = int(args[3])
        args = args[:2] + args[4:]
    files = []
    for a in args[2:]:
        files += sorted(glob.glob(os.path.join(a, "*.ppm"))) if os.path.isdir(a) else [a]
    frames = [Image.open(f).convert("RGB") for f in files]
    if scale != 1:
        frames = [f.resize((f.width * scale, f.height * scale), Image.NEAREST) for f in frames]
    # one palette from frames spread over the whole animation
    step = max(1, len(frames) // 24)
    sample = frames[::step]
    w, h = sample[0].size
    sheet = Image.new("RGB", (w, h * len(sample)))
    for i, f in enumerate(sample):
        sheet.paste(f, (0, i * h))
    pal = sheet.quantize(colors=255, method=Image.MEDIANCUT, dither=Image.NONE)
    q = [f.quantize(palette=pal, dither=Image.NONE) for f in frames]
    q[0].save(out, save_all=True, append_images=q[1:], duration=int(round(1000 / fps)), loop=0, optimize=True)
    print(f"{out}: {len(q)} frames, {os.path.getsize(out)} bytes")


if __name__ == "__main__":
    main()
