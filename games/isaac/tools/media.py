#!/usr/bin/env python3
"""The README's pictures and GIFs, played by the host build (make host) with tests/play.c's bot:

    python3 tools/media.py build/play

- docs/*.png: screens of the game, 320 x 240 as the calculator shows them
- ../../docs/media/isaac.gif (640 x 480, How to play), ../../docs/media/gallery/isaac.gif (320 x 240),
  ../../docs/media/thumb_isaac.png (160 x 120)
A run's rooms come from its seed, so the same seeds give the same pictures."""
import glob
import os
import subprocess
import sys
import tempfile

from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
GAME = os.path.join(HERE, "..")
MEDIA = os.path.join(GAME, "..", "..", "docs", "media")
sys.path.insert(0, os.path.join(GAME, "..", "..", "tools"))
import record  # noqa: E402  (NumPlay's GIF writer: one palette for all the frames)

PLAY = sys.argv[1] if len(sys.argv) > 1 else os.path.join(GAME, "build", "play")
ENV = dict(os.environ, ASAN_OPTIONS="detect_leaks=0")
ROOM_BOSS, ROOM_SHOP, ROOM_TREASURE, ROOM_DEVIL = 4, 3, 2, 13


def frames(args, first, last, step=1):
    """the frames first..last of a run of play with these arguments"""
    with tempfile.TemporaryDirectory() as d:
        shots = ",".join(str(f) for f in range(first, last + 1, step))
        subprocess.run([PLAY, "--saves", d, "--out", d, "--frames", str(last + 1), "--shots", shots] + args,
                       env=ENV, check=True, capture_output=True)
        out = []
        for f in sorted(glob.glob(os.path.join(d, "shot_*.ppm"))):
            im = Image.open(f)
            im.load()
            out.append(im.convert("RGB"))
        return out


def screen(name, args, frame):
    frames(args, frame, frame)[0].save(os.path.join(GAME, "docs", name + ".png"))


def main():
    os.makedirs(os.path.join(GAME, "docs"), exist_ok=True)
    # the screens
    screen("title", ["--keys", "1:right"], 40)
    screen("characters", ["--keys", "5:ok,15:ok,25:right,30:right,35:right"], 50)
    screen("basement", ["--run", "--bot", "--immortal", "--seed", "8"], 160)
    screen("item", ["--run", "--goto", str(ROOM_TREASURE), "--keys", "5-25:up", "--seed", "8"], 40)
    screen("shop", ["--run", "--stage", "2", "--goto", str(ROOM_SHOP), "--seed", "4"], 30)
    screen("vs", ["--run", "--goto", str(ROOM_BOSS), "--char", "1", "--seed", "8"], 30)
    screen("monstro", ["--run", "--bot", "--immortal", "--seed", "8"], 1500)
    screen("mom", ["--run", "--stage", "6", "--goto", str(ROOM_BOSS), "--bot", "--immortal", "--give", "4,7", "--seed", "3"], 160)
    screen("womb", ["--run", "--stage", "7", "--bot", "--immortal", "--char", "3", "--seed", "6"], 150)
    screen("devil", ["--run", "--stage", "3", "--goto", str(ROOM_DEVIL), "--seed", "5"], 30)
    screen("pause", ["--run", "--bot", "--immortal", "--seed", "8", "--keys", "1500:back"], 1510)
    screen("caves", ["--run", "--stage", "3", "--bot", "--immortal", "--char", "2", "--seed", "11"], 400)
    # the GIFs: a room of the Basement, then Monstro
    clip = frames(["--run", "--bot", "--immortal", "--seed", "8"], 150, 290) + \
        frames(["--run", "--bot", "--immortal", "--seed", "8"], 1330, 1480)
    timed = [(i * 33, im) for i, im in enumerate(clip)]
    record.save_gif(timed, os.path.join(MEDIA, "isaac.gif"), 2, 30)
    half = [(i * 66, im) for i, im in enumerate(clip[::2])]
    record.save_gif(half, os.path.join(MEDIA, "gallery", "isaac.gif"), 1, 15)
    thumb = Image.open(os.path.join(GAME, "docs", "monstro.png")).resize((160, 120), Image.LANCZOS)
    thumb.save(os.path.join(MEDIA, "thumb_isaac.png"))
    for f in ("isaac.gif", "gallery/isaac.gif", "thumb_isaac.png"):
        print(f, os.path.getsize(os.path.join(MEDIA, f)) // 1024, "KB")


if __name__ == "__main__":
    main()
