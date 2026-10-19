#!/usr/bin/env python3
"""Checks of Doom on a computer (make test): the host build, with AddressSanitizer and UBSan, through the
title and its demos, the menus, every level of the episode, an intermission, the ending, the saves and the
moment kept on Home.

    python3 tests/check.py build/play"""
import os
import re
import shutil
import subprocess
import sys
import tempfile

PLAY = sys.argv[1] if len(sys.argv) > 1 else "build/play"
failures = []
ENV = dict(os.environ, ASAN_OPTIONS="detect_leaks=0", UBSAN_OPTIONS="halt_on_error=1:print_stacktrace=1")
GS_LEVEL, GS_INTERMISSION, GS_FINALE, GS_DEMOSCREEN = 0, 1, 2, 3


def run(args, saves=None, what=""):
    """Plays (in a save folder, a new one if None); returns the end state and the folder's files."""
    own = saves is None
    if own:
        saves = tempfile.mkdtemp(prefix="doom_check_")
    shots = tempfile.mkdtemp(prefix="doom_shots_")
    p = subprocess.run([PLAY, "--saves", saves, "--out", shots, "--stats"] + args, capture_output=True, text=True,
                       env=ENV, timeout=1800)
    shutil.rmtree(shots, ignore_errors=True)
    files = sorted(os.listdir(saves))
    if own:
        shutil.rmtree(saves, ignore_errors=True)
    if p.returncode or "runtime error" in p.stderr or "AddressSanitizer" in p.stderr or "I_Error" in p.stderr:
        failures.append(f"{what or args}: crashed, an error or a sanitizer error\n{p.stderr[-2000:]}")
        return None
    m = re.search(r"END time (\d+) quit (\d+) state (\d+) map E(\d+)M(\d+) leveltime (-?\d+) health (-?\d+) kills (\d+)",
                  p.stdout)
    s = re.search(r"STATS zone_free (-?\d+) columns peak (\d+) overflows (\d+) zone peak (\d+)", p.stdout)
    if not m or not s:
        failures.append(f"{what}: no end state\n{p.stdout[-500:]}")
        return None
    end = dict(zip(("time", "quit", "state", "episode", "map", "leveltime", "health", "kills"), map(int, m.groups())))
    end.update(zone_free=int(s.group(1)), columns=int(s.group(2)), overflows=int(s.group(3)), zone=int(s.group(4)),
               files=files)
    return end


def check(cond, what):
    if not cond:
        failures.append(what)
    print(("ok  " if cond else "FAIL"), what, flush=True)


# the title, the credits and the three demos, which must play as they were recorded (they end in their
# levels at the same moment as on a PC: the title loop comes back around)
e = run(["--ms", "120000"], what="the demos")
if e:
    check(e["quit"] == 1, "the title and the demos play for two minutes and quit on Home")

# every level, walking, turning and firing on Ultra-Violence
walk = ",".join(f"{t}-{t + 900}:up,{t + 900}-{t + 1300}:right,{t + 300}-{t + 400}:ok" for t in range(1500, 20000, 1500))
for m in range(1, 10):
    e = run(["--warp", f"1{m}", "--keys", walk, "--ms", "21000"], what=f"E1M{m}")
    if e:
        check(e["state"] == GS_LEVEL and e["map"] == m and e["leveltime"] > 500,
              f"E1M{m}: played for {e['leveltime']} tics, health {e['health']}, {e['kills']} kills, "
              f"{e['columns']} columns at most, {e['zone'] // 1024} KB of the zone")

# a level's exit: the intermission, then the next level
e = run(["--warp", "11", "--exit-at", "3000", "--keys", "9000:ok,11000:ok,13000:ok", "--ms", "20000"],
        what="E1M1's exit")
if e:
    check(e["state"] == GS_LEVEL and e["map"] == 2, "E1M1's exit: the intermission, then E1M2")

# the episode's end: E1M8's exit, the text, then the picture
e = run(["--warp", "18", "--exit-at", "3000", "--keys", "40000:ok,41000:ok", "--ms", "45000"], what="the ending")
if e:
    check(e["state"] == GS_FINALE, "E1M8's exit: the ending")

# a new game from the menus: title, menu, New Game, the episode, Hurt Me Plenty; then the menus and the automap
keys = "2000:ok,2700:ok,3400:ok,4100:ok,6000-8000:up,9000:toolbox,10000:plus,11000:toolbox,12000:back,12600:down," \
       "13000:down,13400:back,14000:back"
e = run(["--keys", keys, "--ms", "16000"], what="a new game")
if e:
    check(e["state"] == GS_LEVEL and e["map"] == 1, "a new game from the menus, the automap and the menus over it")

# saving in a slot from the menu, then loading it; Home keeps the moment, and the next start is back there
saves = tempfile.mkdtemp(prefix="doom_saves_")
# (the menu: Save Game is the fourth line; the first slot; its name is the level's: OK saves)
save = "2000:ok,2700:ok,3400:ok,4100:ok,6000-7000:up,8000:back,8500:down,8800:down,9100:down,9600:ok,10200:ok,10800:ok"
e = run(["--keys", save, "--ms", "13000"], saves=saves, what="a save in a slot")
if e:
    check("doomsav0.dsg" in e["files"] and "doomres.sav" in e["files"] and "doom_saves.py" in e["files"],
          "saved in the first slot, the moment kept on Home, both copied into doom_saves.py")
    first = e["leveltime"]
    e = run(["--ms", "4000"], saves=saves, what="the moment back")
    if e:
        check(e["state"] == GS_LEVEL and e["map"] == 1 and e["leveltime"] > first,
              "the next start is back in the level, where Home left it")
    # (the files gone, as when the app is installed again: they come back from doom_saves.py)
    for f in os.listdir(saves):
        if not f.endswith(".py"):
            os.remove(os.path.join(saves, f))
    load = "2000:ok,2700:down,3000:down,3400:ok,4000:ok"  # the menu, Load Game, the first slot
    e = run(["--keys", load, "--ms", "7000"], saves=saves, what="loading the slot")
    if e:
        check(e["state"] == GS_LEVEL and e["map"] == 1 and "doomsav0.dsg" in e["files"],
              "after a reinstall, the slot comes back from doom_saves.py and loads")
shutil.rmtree(saves, ignore_errors=True)

if failures:
    print(f"\n{len(failures)} failed:")
    for f in failures:
        print(" ", f)
    sys.exit(1)
print("\nall good")
