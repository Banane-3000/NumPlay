#!/usr/bin/env python3
"""Checks of Hill Climb Racing on a computer (make test): the host build, with AddressSanitizer and UBSan, through
the menus, buying, every stage with every vehicle, the ends of a run and the saves.

    python3 tests/check.py build/play"""
import os
import re
import shutil
import subprocess
import sys
import tempfile

PLAY = sys.argv[1] if len(sys.argv) > 1 else "build/play"
ENV = dict(os.environ, ASAN_OPTIONS="detect_leaks=0", UBSAN_OPTIONS="halt_on_error=1:print_stacktrace=1")
failures = []
STAGES = ["Countryside", "Desert", "Arctic", "Highway", "Cave", "Moon", "Mars", "Alien Planet", "Arctic Cave", "Forest"]
VEHICLES = ["Hill Climber", "Motocross Bike", "Monster Truck", "Quad Bike", "Race Car", "Rally Car", "Dune Buggy", "Tank"]
MODE_RUN, MODE_END = 4, 6


def run(args, saves, what):
    p = subprocess.run([PLAY, "--saves", saves, "--out", os.path.join(saves, "shots")] + args, capture_output=True,
                       text=True, env=ENV, timeout=900)
    if p.returncode or "runtime error" in p.stderr or "AddressSanitizer" in p.stderr:
        failures.append(f"{what}: crashed or a sanitizer error\n{p.stderr[-2000:]}")
        return None
    m = re.search(r"END mode (\d+) dist (-?\d+) coins (\d+) total (\d+) over (\d) end (\d) level (\d+) flips (\d+) "
                  r"saved (\d+) running (\d)", p.stdout)
    if not m:
        failures.append(f"{what}: no end state\n{p.stdout[-500:]}")
        return None
    k = ["mode", "dist", "coins", "total", "over", "end", "level", "flips", "saved", "running"]
    return dict(zip(k, map(int, m.groups())))


def check(cond, what):
    if not cond:
        failures.append(what)
    return cond


def fresh():
    return tempfile.mkdtemp(prefix="hillclimb_check_")


def main():
    # the menus: title, garage, stage, how to drive, then the run; Home there keeps the run's coins and quits
    d = fresh()
    keys = "30:ok,70:ok,110:ok,150:ok,170-900:right,950:home"
    s = run(["--frames", "1000", "--keys", keys], d, "menus")
    if s and check(s["running"] == 0, "Home did not quit"):
        check(s["saved"] > 0 and s["saved"] == s["total"], f"the run's coins were not kept on Home: {s}")
        check(os.path.exists(os.path.join(d, "hillclimb.sav")), "no hillclimb.sav")
        copy = open(os.path.join(d, "hillclimb_saves.py"), "rb").read() if os.path.exists(
            os.path.join(d, "hillclimb_saves.py")) else b""
        check(b"#>hillclimb.sav:" in copy, "hillclimb_saves.py does not hold the save")
        # installing apps again erases everything but scripts: the save comes back from the copy
        os.remove(os.path.join(d, "hillclimb.sav"))
        s2 = run(["--frames", "60", "--keys", "50:home"], d, "the save from its copy")
        if s2:
            check(s2["saved"] == s["saved"], f"the coins did not come back from hillclimb_saves.py: {s2['saved']}")
    shutil.rmtree(d, ignore_errors=True)

    # buying: with 10,000 coins, the Hill Climber's engine to level 2 (4,000), then its suspension (4,000)
    d = fresh()
    keys = "30:ok,70:down,90:ok,110:right,130:ok,150:right,170:ok,200:home"
    s = run(["--frames", "260", "--coins", "10000", "--keys", keys], d, "buying upgrades")
    if s:
        check(s["saved"] == 2000, f"two upgrades of 4,000 should leave 2,000 coins, not {s['saved']}")
    shutil.rmtree(d, ignore_errors=True)

    # every stage with every vehicle, a player driving: no crash, and it gets somewhere
    for st in range(len(STAGES)):
        for v in range(len(VEHICLES)):
            d = fresh()
            s = run(["--run", f"{st},{v}", "--levels", "6,6,6,6", "--auto", "--frames", "2400"], d,
                    f"{STAGES[st]} with the {VEHICLES[v]}")
            if s:
                check(s["dist"] > 60, f"{STAGES[st]} with the {VEHICLES[v]}: only {s['dist']} m")
            shutil.rmtree(d, ignore_errors=True)

    # the ends of a run: Bill's neck (the bike flipped on the Moon, gas held) and an empty tank (reversing)
    d = fresh()
    s = run(["--run", "5,1", "--keys", "0-1500:right", "--frames", "1500"], d, "driver down")
    if s:
        check(s["mode"] == MODE_END and s["end"] == 0, f"the bike held at full gas on the Moon did not go down: {s}")
    shutil.rmtree(d, ignore_errors=True)
    d = fresh()
    s = run(["--run", "0,0", "--keys", "0-3200:left", "--frames", "3200"], d, "out of fuel")
    if s:
        check(s["mode"] == MODE_END and s["end"] == 1, f"reversing into the wall did not run out of fuel: {s}")
    shutil.rmtree(d, ignore_errors=True)

    # a long run: levels reached, coins collected, the best kept
    d = fresh()
    s = run(["--run", "0,0", "--levels", "10,10,10,8", "--auto", "--frames", "9000"], d, "a long run")
    if s:
        check(s["level"] >= 2 and s["coins"] > 1000, f"a long run on the Countryside: {s}")
    shutil.rmtree(d, ignore_errors=True)

    for f in failures:
        print("FAIL", f)
    print("ok" if not failures else f"{len(failures)} failures")
    sys.exit(1 if failures else 0)


if __name__ == "__main__":
    main()
