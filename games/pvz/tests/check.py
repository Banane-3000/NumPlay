#!/usr/bin/env python3
"""Checks of Plants vs. Zombies on a computer (make test): the host build, with AddressSanitizer and UBSan,
through the first start, a level kept by Home and continued, the saves coming back from pvz_saves.py, every
screen pressed through, a level lost, and every level of the Adventure played by a robot (tests/play.c --sim).

    python3 tests/check.py build/play [--quick]

--quick plays a dozen levels instead of all fifty."""
import os
import random
import re
import shutil
import subprocess
import sys
import tempfile

PLAY = sys.argv[1] if len(sys.argv) > 1 else "build/play"
QUICK = "--quick" in sys.argv
ENV = dict(os.environ, ASAN_OPTIONS="detect_leaks=0", UBSAN_OPTIONS="halt_on_error=1:print_stacktrace=1")
ST_MENU, ST_PLAY, ST_PAUSE, ST_LOST = 1, 7, 8, 11
failures = []


def check(cond, msg):
    if not cond:
        failures.append(msg)


def run(args, what, timeout=1200):
    p = subprocess.run([PLAY] + args, capture_output=True, text=True, env=ENV, timeout=timeout)
    if p.returncode not in (0, 1) or "runtime error" in p.stderr or "AddressSanitizer" in p.stderr:
        failures.append(f"{what}: crashed or a sanitizer error\n{p.stderr[-2000:]}")
        return None
    return p.stdout


def end_state(out, what):
    m = re.search(r"state (\d+) level (\d+) phase (\d+) sun (-?\d+) zombies (\d+) plants (\d+) wave (\d+)/(\d+) "
                  r"progress (\d+) money (\d+) slots (\d+) items (\d+) running (\d+)", out or "")
    if not m:
        failures.append(f"{what}: no end state\n{(out or '')[-500:]}")
        return None
    return dict(zip(("state", "level", "phase", "sun", "zombies", "plants", "wave", "waves", "progress", "money",
                     "slots", "items", "running"), map(int, m.groups())))


def play(saves, keys, frames, what, extra=()):
    shots = tempfile.mkdtemp(prefix="pvz_shots_")
    out = run(["--saves", saves, "--out", shots, "--frames", str(frames), "--keys", keys] + list(extra), what)
    shutil.rmtree(shots, ignore_errors=True)
    return end_state(out, what)


saves = tempfile.mkdtemp(prefix="pvz_check_")
try:
    # the first start: the title, the keys, Adventure; the street, the sod rolled out, a peashooter planted on it,
    # then Home: the progress, the level and their copy are kept
    s = play(saves, "50:ok,150:ok,250:ok,1200:1,1250:ok,1500:home", 1600, "first start")
    if s:
        check(s["running"] == 0, "Home did not quit")
        check(s["level"] == 1 and s["plants"] >= 1, f"no peashooter on level 1-1: {s}")
    for f in ("pvz.sav", "pvzlevel.sav", "pvz_saves.py"):
        check(os.path.exists(os.path.join(saves, f)), f"{f} not written by Home")
    kept = tempfile.mkdtemp(prefix="pvz_copy_")
    for f in os.listdir(saves):
        shutil.copy(os.path.join(saves, f), kept)

    # next time: past the title (no keys sheet now), Continue, Back to Game: the level goes on from where it was
    s = play(saves, "50:ok,150:ok,250:ok", 2500, "continue")
    if s:
        check(s["state"] == ST_PLAY and s["level"] == 1 and s["plants"] >= 1, f"level 1-1 did not go on: {s}")
    check(not os.path.exists(os.path.join(saves, "pvzlevel.sav")), "the level kept after it was continued")

    # installing apps again erases everything but scripts: the saves come back from pvz_saves.py
    for f in os.listdir(kept):
        if not f.endswith(".py"):
            os.remove(os.path.join(kept, f))
    s = play(kept, "50:ok,150:ok,250:ok", 2500, "from the copy")
    if s:
        check(s["state"] == ST_PLAY and s["level"] == 1 and s["plants"] >= 1, f"the copy did not bring 1-1 back: {s}")
    shutil.rmtree(kept, ignore_errors=True)

    # a level with a sunflower and nothing else: the mowers go, then the zombies eat your brains
    fresh = tempfile.mkdtemp(prefix="pvz_lost_")
    s = play(fresh, "50:ok,150:ok,250:ok,1500:2,1550:ok", 40000, "a level lost", ["--progress", "6"])
    if s:
        check(s["state"] == ST_LOST, f"level 1-6 not lost with a sunflower alone: {s}")
    shutil.rmtree(fresh, ignore_errors=True)

    # every screen, pressed through at random from a well advanced Adventure: the menus, Choose a Level, the
    # Almanac, the shop, Crazy Dave, the seed chooser, the levels, the pause menu and its questions
    names = ["left", "right", "up", "down", "ok", "ok", "ok", "back", "shovel", "almanac"] + [str(d) for d in range(10)]
    for prog in (12, 25, 35, 45):
        rnd = random.Random(prog)
        keys, t = [], 60
        while t < 30000:
            keys.append(f"{t}:{rnd.choice(names)}")
            t += rnd.randint(8, 60)
        tour = tempfile.mkdtemp(prefix="pvz_tour_")
        s = play(tour, ",".join(keys), 30000, f"keys pressed at random from level {prog}",
                 ["--progress", str(prog), "--money", "60000"])
        shutil.rmtree(tour, ignore_errors=True)

    # every level, a robot planting (with all the sun it wants): it must not crash, and win nearly all of them
    levels = [1, 5, 10, 15, 20, 25, 30, 35, 41, 45, 49, 50] if QUICK else range(1, 51)
    won = 0
    for level in levels:
        out = run(["--sim", str(level), "--frames", "90000"], f"level {level} played by the robot")
        if out is None:
            continue
        line = out.strip().splitlines()[0] if out.strip() else ""
        if " won " in line:
            won += 1
        else:
            print("   ", " ".join(out.split()))
    need = len(levels) - 5
    check(won >= need, f"the robot won {won} of {len(levels)} levels (at least {need} expected)")
    print(f"robot: {won} of {len(levels)} levels won")
finally:
    shutil.rmtree(saves, ignore_errors=True)

if failures:
    print("FAILED:")
    for f in failures:
        print(" -", f)
    sys.exit(1)
print("all checks passed")
