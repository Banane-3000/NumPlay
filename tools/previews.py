#!/usr/bin/env python3
"""Records the clips NumPlay Studio plays on its calculator's screen when you point at a game.

Most games use their gallery GIF (docs/media/gallery); those filmed on a real calculator get a
clip of their own from the emulator instead, here, in docs/media/previews/<id>.gif: the game
played by a short key script, recorded from the screen itself.

Usage: tools/previews.py [id ...]     (build/apps/*.nwa must be built)
"""
import os
import subprocess
import sys
import tempfile

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
OUT = os.path.join(ROOT, "docs", "media", "previews")


def presses(*items):
    return ",".join(f"{t}-{t + (h[0] if h else 90)}:{k}" for t, k, *h in items)


def every(t0, t1, step, key, hold=90):
    return ",".join(f"{t}-{t + hold}:{key}" for t in range(t0, t1, step))


# id: (app, key script, the part to keep in ms, frames a second)
DEMOS = {
    "numdash": ("NumDash.nwa", presses((2500, "ok"), (3700, "ok", 8000)), (3900, 10900), 25),
    "crossyroad": ("CrossyRoad.nwa", every(1500, 10000, 420, "up") + "," + presses((4300, "left")), (1700, 5400), 20),
    "numdrive": ("NumDrive.nwa", presses((1500, "right", 7000)), (2200, 5500), 20),
    "balatro": ("Balatro.nwa", presses((1500, "ok"), (2500, "ok"), (3500, "ok"), (5000, "ok"), (5400, "right"),
                                        (5800, "ok"), (6200, "right"), (6600, "ok"), (7100, "exe")), (4600, 11600), 15),
    # past the door and the dealer, a name typed on the waiver and signed (EXE), then the game
    "buckshot": ("BuckshotRoulette.nwa", every(1500, 15000, 900, "ok") + "," + presses((15300, "exe")) + "," +
                 every(17000, 27000, 1300, "ok"), (18500, 26500), 15),
    "tetris": ("Tetris.nwa", presses((1200, "ok")) + "," + every(2200, 9000, 700, "up") + "," +
               every(2500, 9000, 1400, "left") + "," + every(3100, 9000, 1400, "right") + "," +
               every(3400, 9000, 700, "down", 300), (2400, 8400), 20),
    "celeste": ("Celeste.nwa", presses((2500, "ok"), (4000, "ok"), (5500, "ok"), (7000, "ok"), (8500, "ok"),
                                        (10500, "right", 5500)) + "," + every(11200, 16000, 900, "ok", 200),
                (10200, 16000), 20),
}


def main():
    ids = sys.argv[1:] or list(DEMOS)
    os.makedirs(OUT, exist_ok=True)
    for gid in ids:
        app, keys, (t0, t1), fps = DEMOS[gid]
        with tempfile.TemporaryDirectory() as d:
            frames = os.path.join(d, "frames")
            subprocess.run([sys.executable, os.path.join(ROOT, "tools", "emu.py"), os.path.join(ROOT, "build", "apps", app),
                            "--ms", str(t1 + 200), "--keys", keys, "--frames", frames, "--fps", str(fps), "--out", d],
                           check=True, stdout=subprocess.DEVNULL)
            gif = os.path.join(OUT, gid + ".gif")
            subprocess.run([sys.executable, os.path.join(ROOT, "tools", "record.py"), "--frames", frames, "--out", d,
                            "--gif", f"{t0}-{t1}:{gid}.gif", "--fps", str(fps)], check=True, stdout=subprocess.DEVNULL)
            os.replace(os.path.join(d, gid + ".gif"), gif)
            subprocess.run(["gifsicle", "-O3", "--batch", gif], check=False)
            print(f"{gid}: {os.path.getsize(gif) // 1024} KB")


if __name__ == "__main__":
    main()
