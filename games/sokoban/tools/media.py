#!/usr/bin/env python3
"""The pictures of the README and How to play, from the host build (make host; make media):

  docs/shot.png, docs/levels.png    a level, and the level select (shot.png is NumPlay's card too)
  ../../docs/media/sokoban.gif         levels 2 and 4 solved, for How to play
  ../../docs/media/gallery/sokoban.gif level 9, for the README's gallery
  ../../docs/media/thumb_sokoban.png   the README's table (level 4 on the way)

The moves come from tests/solutions.txt. Python 3 with Pillow.
"""
import os
import subprocess
import tempfile

from PIL import Image

HERE = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
PLAY = os.path.join(HERE, "build", "play")
MEDIA = os.path.join(HERE, "..", "..", "docs", "media")


def solution(level):
    for line in open(os.path.join(HERE, "tests", "solutions.txt")):
        if line.startswith(f"{level}:"):
            return line.split(":", 1)[1].strip()
    raise SystemExit(f"no solution for level {level}")


def film(level):
    with tempfile.TemporaryDirectory() as d:
        subprocess.run([PLAY, "film", str(level), d, solution(level)], check=True, capture_output=True)
        return [Image.open(os.path.join(d, f)).convert("RGB") for f in sorted(os.listdir(d))]


def gif(path, clips):
    """Clips of frames, a step each 110 ms (as when an arrow is held); a clip's first frame and last
    (Solved!) are held."""
    frames, times = [], []
    for c in clips:
        for i, f in enumerate(c):
            frames.append(f)
            times.append(700 if i == 0 else 1600 if i == len(c) - 1 else 110)
    sheet = Image.new("RGB", (320, 240 * len(frames[::4])))
    for i, f in enumerate(frames[::4]):
        sheet.paste(f, (0, 240 * i))
    pal = sheet.quantize(64, method=Image.Quantize.MEDIANCUT, dither=Image.Dither.NONE)
    out = [f.quantize(palette=pal, dither=Image.Dither.NONE) for f in frames]
    out[0].save(path, save_all=True, append_images=out[1:], duration=times, loop=0, optimize=True, disposal=1)
    print(f"{path}: {len(out)} frames, {os.path.getsize(path)} bytes")


def main():
    for args, name in (([PLAY, "shot", "1", "build/shot.ppm"], "shot.png"), ([PLAY, "hub", "build/hub.ppm", "17"], "levels.png")):
        subprocess.run(args, check=True, cwd=HERE)
        Image.open(os.path.join(HERE, args[-1] if name == "shot.png" else args[2])).convert("RGB").save(
            os.path.join(HERE, "docs", name), optimize=True)
    four = film(4)
    gif(os.path.join(MEDIA, "sokoban.gif"), [film(2), four])
    gif(os.path.join(MEDIA, "gallery", "sokoban.gif"), [film(9)])
    thumb = os.path.join(MEDIA, "thumb_sokoban.png")
    four[len(four) // 3].crop((40, 30, 280, 210)).resize((160, 120), Image.LANCZOS).save(thumb, optimize=True)
    subprocess.run(["python3", os.path.join(HERE, "..", "..", "tools", "media", "round_corners.py"), thumb], check=True)


if __name__ == "__main__":
    main()
