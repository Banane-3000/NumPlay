#!/usr/bin/env python3
"""Checks of Undertale on a computer (make test): the host build, with
AddressSanitizer and UBSan, through the menus, every battle, the saves, the
whole Ruins and the whole of Snowdin.

    python3 tests/check.py build/play [--quick]

--quick skips playing the whole Ruins and Snowdin (the longest parts)."""
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

MODE_WORLD, MODE_BATTLE, MODE_END, MODE_SHOP = 2, 3, 8, 9
# rooms and battles (src/data.h, src/monsters.h)
ROOMS = {m.group(1): int(m.group(2)) for m in
         re.finditer(r"#define ROOM_(\w+) (\d+)", open(os.path.join(HERE, "..", "src", "data.h")).read())}
FORM_SNOWDRAKE, FORM_PAPYRUS = 19, 31


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

# Snowdin's battles: fled from, fought (the Snowdrake, the Gyftrot, the Glyde, the Greater Dog...), and Papyrus,
# who can't be fled: spared once he spares you, or (the SOUL hurt) he captures you
for form in range(FORM_SNOWDRAKE, FORM_PAPYRUS):
    s = run(["--god", "--battle", str(form), "--path", "flee"], 1500, what=f"battle {form}, fleeing")
    check(s and s["mode"] == MODE_WORLD, f"battle {form}: fleeing did not end it: {s}")
for form in (19, 20, 26, 27, 28, 30):
    s = run(["--god", "--battle", str(form), "--path", "fight"], 4000, what=f"battle {form}, fighting")
    check(s and s["mode"] == MODE_WORLD and s["kills"] >= 1, f"battle {form}: FIGHT did not win it: {s}")
s = run(["--god", "--battle", str(FORM_PAPYRUS), "--path", "spare"], 15000, what="Papyrus, spared")
check(s and s["mode"] == MODE_WORLD and s["kills"] == 0, f"Papyrus was not spared: {s}")
s = run(["--battle", str(FORM_PAPYRUS), "--path", "spare"], 6000, what="Papyrus, captured")
check(s and s["mode"] == MODE_WORLD and s["hp"] == 1, f"Papyrus did not capture Frisk: {s}")

# the shop in Snowdin: a Cinnamon Bunny for 25G; Papyrus's date: his phone number
s = run(["--name", "Frisk", "--noenc", "--room", str(ROOMS["SNOWTOWN"]), "--at", "446,1092", "--gold", "100", "--keys",
         "2-24:up,60:z,90:down,100:down,110:down,120:z,140:z,170:x,190:down,200:down,210:down,220:z"], 400, what="the shop")
check(s and s["gold"] == 75 and s["items"][0] == 16, f"the shop did not sell a Cinnamon Bunny: {s}")
s = run(["--name", "Frisk", "--noenc", "--room", str(ROOMS["PSHOUSE"]), "--at", "2436,914", "--flag",
         f"{FLAGS['F_PAPYRUS']}=1", "--flag", f"{FLAGS['F_PAP_DATE']}=1", "--path",
         "go:2300,918;l;w5;l;w5;l;w5;z;pick:0;talk;w60"], 2500, what="Papyrus's date")
check(s and flag(s, "PAP_DATE") == 2, f"the date did not end with Papyrus's number: {s}")

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
    check(s and s["mode"] == MODE_WORLD and s["room"] == ROOMS["SNOWDIN1"], f"the Ruins' door did not lead to Snowdin: {s}")
    if s:
        for name, v in (("FLOWEY_MET", 1), ("DUMMY", 1), ("RUINS5_DONE", 1), ("RUINS6_DONE", 1), ("PHONE", 1),
                        ("RUINS9_ROCK", 1), ("RUINS11_ROCK", 5), ("NAPSTA", 1), ("ONESWITCH", 1), ("ROTATE4", 1),
                        ("TORIEL_HOME", 1), ("HOME_TOUR", 1), ("TORIEL_BASEMENT", 1), ("BASEMENT_TALK", 3),
                        ("TORIEL_FIGHT", 1), ("FLOWEY_END", 1), ("RUINS_EXIT", 1)):
            check(flag(s, name) == v, f"the Ruins: F_{name} is {flag(s, name)}, not {v}")
        check(s["kills"] == 0, f"the Ruins: {s['kills']} killed on the friendly way")
        check(s["items"].count(1) == 2, f"the Ruins: not the two pieces of Monster Candy: {s['items']}")

# the whole of Snowdin, the friendly way: Sans, Papyrus's puzzles (the maze, the X/O ones, the ice, the tiles),
# Doggo, Dogamy and Dogaressa, Greater Dog, the Gauntlet, the town, Papyrus spared, the way on
if not QUICK:
    s = run(["--name", "Frisk", "--noenc", "--god", "--room", str(ROOMS["SNOWDIN1"]), "--at", "34,392", "--path-file",
             os.path.join(HERE, "snowdin.path")], 30000, what="Snowdin")
    check(s and s["mode"] == MODE_END, f"Snowdin did not end on the way to Waterfall: {s}")
    if s:
        for name, v in (("BRANCH", 1), ("SANS_MET", 1), ("PAP_SENTRY", 1), ("DOGGO", 1), ("ELECMAZE", 2),
                        ("WORDSEARCH", 1), ("SNOWPUZZLE", 1), ("DOGI", 1), ("XOXO1", 1), ("XOXO2", 1), ("XOXO3", 1),
                        ("PAP_XOXO1", 2), ("TILES", 1), ("GREATERDOG", 1), ("GAUNTLET", 1), ("PAPYRUS", 1),
                        ("PAP_DATE", 1), ("TOWN_HOUSE", 1), ("SNOW_EXIT", 1)):
            check(flag(s, name) == v, f"Snowdin: F_{name} is {flag(s, name)}, not {v}")
        check(s["kills"] == 0, f"Snowdin: {s['kills']} killed on the friendly way")

if failures:
    print("\n".join("FAIL " + f for f in failures))
    sys.exit(1)
print("all checks passed")
