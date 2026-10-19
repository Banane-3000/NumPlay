#!/usr/bin/env python3
"""Checks of The Binding of Isaac on a computer (make test): the host build, with AddressSanitizer and UBSan,
through the menus, a run kept and continued, dying, every boss, every special room, every item, pill and card,
and runs played by the bot (tests/play.c) down through the floors.

    python3 tests/check.py build/play [--quick]

--quick plays fewer and shorter runs."""
import os
import re
import shutil
import subprocess
import sys
import tempfile

PLAY = sys.argv[1] if len(sys.argv) > 1 else "build/play"
HERE = os.path.dirname(os.path.abspath(__file__))
QUICK = "--quick" in sys.argv
ENV = dict(os.environ, ASAN_OPTIONS="detect_leaks=0", UBSAN_OPTIONS="halt_on_error=1:print_stacktrace=1")
failures = []

H = open(os.path.join(HERE, "..", "src", "data.h")).read()
MON = {m.group(1): i for i, m in enumerate(re.finditer(r"^  MON_(\w+),", H, re.M))}
ROOM = {m.group(1): i for i, m in enumerate(re.finditer(r"^  ROOM_(\w+),", H, re.M))}
ITEMS = [int(x) for x in re.search(r"#define ITEM_ID_TABLE \{([\d,]+)\}", H).group(1).split(",")]
MODE_MENU, MODE_PLAY, MODE_DEAD = 1, 2, 5


def run(args, what, saves=None):
    own = saves is None
    if own:
        saves = tempfile.mkdtemp(prefix="isaac_check_")
    p = subprocess.run([PLAY, "--saves", saves, "--out", saves] + args, capture_output=True, text=True, env=ENV,
                       timeout=1800)
    if own:
        shutil.rmtree(saves, ignore_errors=True)
    if p.returncode or "runtime error" in p.stderr or "AddressSanitizer" in p.stderr:
        failures.append(f"{what}: crashed or a sanitizer error\n{p.stderr[-2000:]}")
        return None
    m = re.search(r"END mode (\d+) stage (\d+) room (\d+) kind (\d+) hp (\d+)/(\d+) soul (\d+) coins (\d+) bombs (\d+) "
                  r"keys (\d+) items (\d+) frames (\d+) floors (\d+) dropped (\d+)", p.stdout)
    if not m:
        failures.append(f"{what}: no end state\n{p.stdout[-500:]}")
        return None
    keys = ["mode", "stage", "room", "kind", "hp", "max", "soul", "coins", "bombs", "keys", "items", "frames", "floors",
            "dropped"]
    st = dict(zip(keys, map(int, m.groups())))
    pk = re.search(r"POCKET (\d+) (\d+) ACTIVE (\d+) LIVES (\d+)", p.stdout)
    st["pocket"], st["active"] = (int(pk.group(1)), int(pk.group(3))) if pk else (0, 0)
    if st["dropped"]:
        failures.append(f"{what}: {st['dropped']} drawing commands dropped")
    return st


def expect(cond, what):
    if not cond:
        failures.append(what)


# ---- the menus: the title, a new run, the floor's name, then the first room
st = run(["--frames", "120", "--keys", "5:ok,15:ok,30:ok"], "menus")
expect(st and st["mode"] == MODE_PLAY and st["stage"] == 1 and st["kind"] == ROOM["START"], f"menus: not playing ({st})")
st = run(["--frames", "60", "--keys", "5:ok,10:down,12:down,15:ok,30:back"], "how to play")
expect(st and st["mode"] == MODE_MENU, f"how to play: not back to the menu ({st})")

# ---- a run kept on Home, continued
saves = tempfile.mkdtemp(prefix="isaac_check_")
run(["--run", "--frames", "200", "--bot", "--god", "--keys", "190:home", "--seed", "4"], "keep the run", saves)
expect(os.path.exists(os.path.join(saves, "isaacrun.sav")), "the run wasn't kept on Home")
expect(os.path.exists(os.path.join(saves, "isaac_saves.py")), "no copy of the saves in isaac_saves.py")
st = run(["--frames", "60", "--keys", "5:ok,15:ok"], "continue", saves)
expect(st and st["mode"] == MODE_PLAY, f"continue: not playing ({st})")
# the copy brings the run back once the files are gone (as installing apps does)
os.remove(os.path.join(saves, "isaacrun.sav"))
st = run(["--frames", "60", "--keys", "5:ok,15:ok"], "continue from the copy", saves)
expect(st and st["mode"] == MODE_PLAY, f"continue from the copy: not playing ({st})")
shutil.rmtree(saves, ignore_errors=True)

# ---- dying: the bot in the Womb without help, then a new run from the death screen
st = run(["--run", "--stage", "7", "--frames", "3000", "--bot", "--seed", "5"], "death")
expect(st and st["mode"] in (MODE_DEAD, MODE_PLAY), f"death: {st}")

# ---- every boss, fought by the bot for a while
bosses = ["MONSTRO", "LARRY", "DUKE", "GEMINI", "GURDY_JR", "FAMINE", "STEVEN", "WIDOW", "PIN", "HAUNT", "DINGLE",
          "BLIGHTED_OVUM", "CHUB", "GURDY", "PEEP", "PESTILENCE", "CARRION_QUEEN", "MEGA_FATTY", "POLYCEPHALUS", "HUSK",
          "MONSTRO2", "LOKI", "WAR", "MASK_OF_INFAMY", "GISH", "BLOAT", "WRETCHED", "DARK_ONE", "BLASTOCYST",
          "DADDY_LONG_LEGS", "SCOLEX", "DEATH", "LOKII", "CONQUEST", "MAMA_GURDY", "MR_FRED", "TRIACHNID", "TERATOMA",
          "SLOTH", "LUST", "WRATH", "GLUTTONY", "GREED", "ENVY", "PRIDE"]
killed = 0
for b in bosses[:12] if QUICK else bosses:
    st = run(["--run", "--stage", "2", "--boss", str(MON[b]), "--goto", str(ROOM["BOSS"]), "--bot", "--god",
              "--give", "4,7,12", "--frames", "2500", "--seed", "3"], f"boss {b}")
    if st and st["kind"] != ROOM["BOSS"]:
        killed += 1   # (beaten, and the bot went on)
for b, stage in (("MOM", 6), ("MOMS_HEART", 8), ("SATAN", 9), ("ISAAC", 9)):
    st = run(["--run", "--stage", str(stage), "--goto", str(ROOM["BOSS"]), "--bot", "--god", "--give", "4,7,12,51,79",
              "--frames", "4000", "--seed", "3"], f"boss {b}")
print(f"bosses: {killed} beaten by the bot in time")

# ---- the special rooms, on a few floors
for kind in ("TREASURE", "SHOP", "SECRET", "SUPERSECRET", "CURSE", "ARCADE", "SACRIFICE", "LIBRARY", "MINIBOSS",
             "CHALLENGE", "DEVIL", "ANGEL"):
    for stage in (2, 5):
        run(["--run", "--stage", str(stage), "--goto", str(ROOM[kind]), "--bot", "--god", "--frames", "400",
             "--seed", str(stage)], f"room {kind}")

# ---- every item, ten at a time, the bot playing with them (actives used now and then)
for i in range(0, len(ITEMS), 10):
    group = ITEMS[i:i + 10]
    st = run(["--run", "--stage", "3", "--give", ",".join(map(str, group)), "--bot", "--god", "--frames", "900",
              "--seed", str(i)], f"items {group}")
for it in (33, 34, 35, 36, 37, 38, 39, 41, 42, 44, 45, 47, 49, 56, 58, 65, 66, 77, 78, 83, 84, 85, 86, 97, 102, 105,
           111, 123, 124, 126, 127, 130, 133, 135, 136, 137, 145, 146, 158, 160, 164, 166, 170, 171, 175, 177, 186, 192):
    st = run(["--run", "--stage", "2", "--give", f"{it},63", "--bot", "--god", "--frames", "200",
              "--keys", "40:item,120:item", "--seed", str(it)], f"active {it}")

# ---- pills (each colour) and cards (each one)
for c in range(1, 14):
    st = run(["--run", "--pocket", f"1,{c}", "--frames", "60", "--keys", "20:card"], f"pill {c}")
    expect(st and st["pocket"] == 0, f"pill {c} not taken")
for c in range(1, 32):
    st = run(["--run", "--stage", "2", "--pocket", f"2,{c}", "--frames", "60", "--keys", "20:card"], f"card {c}")
    expect(st and st["pocket"] == 0, f"card {c} not used")

# ---- runs played by the bot, down the floors
seeds = range(1, 3) if QUICK else range(1, 9)
deepest = 0
for s in seeds:
    st = run(["--run", "--bot", "--god", "--frames", "12000" if QUICK else "40000", "--seed", str(s)], f"bot run {s}")
    if st:
        deepest = max(deepest, st["floors"])
print(f"the bot's deepest floor: {deepest}")
expect(deepest >= 3, f"the bot never got past the Caves ({deepest})")

if failures:
    print(f"{len(failures)} failures:")
    for f in failures:
        print(" -", f)
    sys.exit(1)
print("all checks passed")
