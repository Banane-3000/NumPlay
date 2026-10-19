#!/usr/bin/env python3
"""Doom's pictures and animations, from the computer build (which draws exactly what the calculator does):
docs/*.png, and the repository's docs/media/doom.gif (640 x 480), docs/media/gallery/doom.gif (320 x 240)
and docs/media/thumb_doom.png (160 x 120).

    make build/play && python3 tools/media.py build/play

The fights are id Software's own: the first demo of the title screen, Phobos Lab, played by the engine."""
import os
import shutil
import subprocess
import sys
import tempfile

from PIL import Image

GAME = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ROOT = os.path.dirname(os.path.dirname(GAME))
sys.path.insert(0, os.path.join(ROOT, "tools"))
from record import save_gif  # noqa: E402 (the repository's GIF maker: one palette, gifsicle)

PLAY = os.path.abspath(sys.argv[1] if len(sys.argv) > 1 else os.path.join(GAME, "build/play"))
FRAME = 1000 // 35  # (a frame drawn moves the computer's clock on by this many ms)
ENV = dict(os.environ, ASAN_OPTIONS="detect_leaks=0")


def play(args, shots, ms):
    """Plays (with settings already saved, so without the first game's word on the keys) and returns
    {ms: picture} for each shot asked."""
    saves, out = tempfile.mkdtemp(prefix="doom_media_"), tempfile.mkdtemp(prefix="doom_media_")
    with open(os.path.join(saves, "doom.cfg"), "wb") as f:
        f.write(b"D\x01\x01\x00")
    shots = sorted(set(shots))
    subprocess.run([PLAY, "--saves", saves, "--out", out, "--ms", str(ms), "--shots", ",".join(map(str, shots))]
                   + args, check=True, capture_output=True, env=ENV)
    pics = {}
    for t in shots:
        with Image.open(os.path.join(out, f"shot_{t:06d}.ppm")) as im:
            pics[t] = im.convert("RGB")
    shutil.rmtree(saves)
    shutil.rmtree(out)
    return pics


def frames(a, b, step=2):
    """the times of every step-th frame from a to b ms"""
    return list(range(a - a % FRAME, b, FRAME * step))


def save_png(im, path):
    im.quantize(colors=256, method=Image.Quantize.MEDIANCUT, dither=Image.Dither.NONE).save(path, optimize=True)
    print(path, os.path.getsize(path) // 1024, "KB")


docs = os.path.join(GAME, "docs")
media = os.path.join(ROOT, "docs", "media")
os.makedirs(docs, exist_ok=True)

# the title screen and the first demo, Phobos Lab: imps down a corridor, then zombiemen around the yellow key
IMPS, KEY = (9200, 12700), (35300, 38500)
title_shots = {"title": 2000, "imps": 10058, "fight": 36058, "lab": 57058}
gif_times = frames(*IMPS) + frames(*KEY)
pics = play([], list(title_shots.values()) + gif_times + [1000], 58000)
for name, t in title_shots.items():
    save_png(pics[t], os.path.join(docs, f"{name}.png"))

# the animations: the title, then the two fights
anim = [(0, pics[1000])] + [(1500 + i * 2 * FRAME, pics[t]) for i, t in enumerate(gif_times)]
save_gif(anim, os.path.join(media, "doom.gif"), 2, 18)
print("doom.gif", os.path.getsize(os.path.join(media, "doom.gif")) // 1024, "KB")
small = [(0, pics[1000])] + [(1500 + i * 4 * FRAME, pics[t]) for i, t in enumerate(gif_times[::2])]
save_gif(small, os.path.join(media, "gallery", "doom.gif"), 1, 9)
print("gallery/doom.gif", os.path.getsize(os.path.join(media, "gallery", "doom.gif")) // 1024, "KB")
thumb = pics[title_shots["fight"]].resize((160, 120), Image.LANCZOS)
save_png(thumb, os.path.join(media, "thumb_doom.png"))

# Hangar: through the courtyard, the automap, the menu over the game, then the exit and the intermission
walk = ("1500-2200:up,2200-2500:right,1700:ok,2700-3400:up,3400-3700:right,2900:ok,3900-4600:up,4600-4900:right,"
        "4100:ok,5100-5800:up,5800-6100:right,6300-7000:up,7000-7300:right,9000:toolbox,10500:toolbox,11500:back,"
        "13000:back,21000:ok")
hangar_shots = {"hangar": 5200, "automap": 9800, "menu": 12400, "intermission": 19600}
pics = play(["--warp", "11", "--exit-at", "14000", "--keys", walk], hangar_shots.values(), 20000)
for name, t in hangar_shots.items():
    save_png(pics[t], os.path.join(docs, f"{name}.png"))

# Read This!: the calculator's keys
pics = play(["--keys", "2000:ok,2600:down,2900:down,3200:down,3500:down,4000:ok"], [5000], 6000)
save_png(pics[5000], os.path.join(docs, "keys.png"))

# E1M8's exit: the episode's ending
pics = play(["--warp", "18", "--exit-at", "3000"], [44000], 45000)
save_png(pics[44000], os.path.join(docs, "ending.png"))
