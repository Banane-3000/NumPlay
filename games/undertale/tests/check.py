#!/usr/bin/env python3
"""Checks of Undertale on a computer (make test): the host build, with
AddressSanitizer and UBSan, through the menus, every battle, the saves and
the whole Ruins.

    python3 tests/check.py build/play [--quick]

--quick skips playing the whole Ruins (the longest part)."""
import os
import re
import shutil
import subprocess
import sys
import tempfile

PLAY = sys.argv[1] if len(sys.argv) > 1 else "build/play"
HERE = os.path.dirname(os.path.abspath(__file__))
QUICK = "--quick" in sys.argv
failures = []
ENV = dict(os.environ, ASAN_OPTIONS="detect_leaks=0", UBSAN_OPTIONS="halt_on_error=1:print_stacktrace=1")

# the flags the checks look at (src/flags.h)
FLAGS = {}
src = open(os.path.join(HERE, "..", "src", "flags.h")).read()
body = re.sub(r"/\*.*?\*/", "", src[src.index("enum {") + 6:src.rindex("FLAG_COUNT")], flags=re.S)
for i, n in enumerate(n.strip() for n in body.split(",") if n.strip()):
    FLAGS[n] = i

MODE_WORLD, MODE_BATTLE, MODE_END = 2, 3, 8


def run(args, frames, saves=None, what=""):
    """Plays (in a save folder, a new one if None); returns the end state: mode, room, flags, items..."""
    own = saves is None
    if own:
        saves = tempfile.mkdtemp(prefix="undertale_check_")
    p = subprocess.run([PLAY, "--saves", saves, "--frames", str(frames)] + args, capture_output=True, text=True,
                       env=ENV, timeout=900)
    if own:
        shutil.rmtree(saves, ignore_errors=True)
    if p.returncode or "runtime error" in p.stderr or "AddressSanitizer" in p.stderr:
        failures.append(f"{what or args}: crashed or a sanitizer error\n{p.stderr[-2000:]}")
        return None
    m = re.search(r"END mode (\d+) room (-?\d+) at (-?\d+),(-?\d+) hp (-?\d+) lv (\d+) gold (\d+) kills (\d+) path (\w+)",
                  p.stdout)
    if not m:
        failures.append(f"{what}: no end state\n{p.stdout[-500:]}")
        return None
    s = dict(zip(("mode", "room", "x", "y", "hp", "lv", "gold", "kills"), map(int, m.groups()[:8])))
    s["path"] = m.group(9)
    fl = re.search(r"FLAGS(.*)", p.stdout).group(1).split()
    s["flags"] = {int(a): int(b) for a, b in (f.split("=") for f in fl)}
    s["items"] = [int(v) for v in re.search(r"ITEMS(.*)", p.stdout).group(1).split()]
    return s


def flag(s, name):
    return s["flags"].get(FLAGS["F_" + name], 0)


def check(cond, msg):
    if not cond:
        failures.append(msg)


# the intro, the title, the instructions, naming the fallen human "A", Done, Yes: the flowers
keys = "30:z,60:z,90:z,120:z," + ",".join("%d:down" % (150 + 10 * i) for i in range(8)) + \
       ",230:right,240:right,250:z,290:right,300:z"
s = run(["--keys", keys], 700, what="the menus to a new game")
check(s and s["mode"] == MODE_WORLD and s["room"] == 0 and s["hp"] == 20, f"the menus did not start a game: {s}")

# every battle of the Ruins, fled from (Toriel's can't be: she is spared) and fought to the end
for form in range(18):
    s = run(["--god", "--battle", str(form), "--path", "flee"], 1500, what=f"battle {form}, fleeing")
    if form == 17:
        continue
    check(s and s["mode"] == MODE_WORLD, f"battle {form}: fleeing did not end it: {s}")
for form in (2, 3, 6, 10, 12):
    s = run(["--god", "--battle", str(form), "--path", "fight"], 3000, what=f"battle {form}, fighting")
    check(s and s["mode"] == MODE_WORLD and s["kills"] >= 1, f"battle {form}: FIGHT did not win it: {s}")
s = run(["--god", "--battle", "16", "--path", "act:3"], 6000, what="Napstablook, cheered up")
check(s and s["mode"] == MODE_WORLD and s["kills"] == 0, f"Napstablook was not spared by cheering: {s}")

# the leaf pile's SAVE, then Continue from the title
d = tempfile.mkdtemp(prefix="undertale_check_")
s = run(["--name", "Frisk", "--noenc", "--room", "8", "--at", "5012,4498", "--path", "u;z;w20;z;w30;x"], 300, saves=d,
        what="saving at the leaf pile")
check(os.path.exists(d + "/undertale.sav") and os.path.exists(d + "/undertale_saves.py"), "the SAVE Point did not save")
s = run(["--keys", "30:z,60:z,90:z,120:z"], 300, saves=d, what="Continue")
check(s and s["mode"] == MODE_WORLD and s["room"] == 8, f"Continue did not go back to the leaf pile: {s}")
# Home keeps the moment; the next start is back there
s = run(["--keys", "30:z,60:z,90:z,120:z,130-170:right,200:home"], 300, saves=d, what="Home")
s = run([], 30, saves=d, what="back after Home")
check(s and s["mode"] == MODE_WORLD and s["room"] == 8 and s["x"] > 5000, f"Home did not bring the game back: {s}")
shutil.rmtree(d, ignore_errors=True)

# the whole Ruins, the friendly way: Flowey, Toriel's puzzles, the candy, the rocks, Napstablook, the switches,
# Toriel's home, her fight spared, Flowey's judgement, the door out
if not QUICK:
    s = run(["--name", "Frisk", "--noenc", "--god", "--path-file", os.path.join(HERE, "ruins.path")], 40000,
            what="the Ruins")
    check(s and s["mode"] == MODE_END, f"the Ruins did not end at their door: {s}")
    if s:
        for name, v in (("FLOWEY_MET", 1), ("DUMMY", 1), ("RUINS5_DONE", 1), ("RUINS6_DONE", 1), ("PHONE", 1),
                        ("RUINS9_ROCK", 1), ("RUINS11_ROCK", 5), ("NAPSTA", 1), ("ONESWITCH", 1), ("ROTATE4", 1),
                        ("TORIEL_HOME", 1), ("HOME_TOUR", 1), ("TORIEL_BASEMENT", 1), ("BASEMENT_TALK", 3),
                        ("TORIEL_FIGHT", 1), ("FLOWEY_END", 1), ("RUINS_EXIT", 1)):
            check(flag(s, name) == v, f"the Ruins: F_{name} is {flag(s, name)}, not {v}")
        check(s["kills"] == 0, f"the Ruins: {s['kills']} killed on the friendly way")
        check(s["items"].count(1) == 2, f"the Ruins: not the two pieces of Monster Candy: {s['items']}")

if failures:
    print("\n".join("FAIL " + f for f in failures))
    sys.exit(1)
print("all checks passed")
