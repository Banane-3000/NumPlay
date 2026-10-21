#!/usr/bin/env python3
"""Checks of Sonic the Hedgehog on a computer (make test): the host build, with
AddressSanitizer and UBSan, through the keys screen, the title, a new game, the
saves and Continue, the attract demos (played back as the original plays them:
no deaths), every act, every special stage, the continue screen and the ending.

    python3 tests/check.py build/play [--quick]

--quick skips the acts, special stages and ending (the longest part)."""
import os
import re
import shutil
import struct
import subprocess
import sys
import tempfile

PLAY = sys.argv[1] if len(sys.argv) > 1 and not sys.argv[1].startswith("--") else "build/play"
QUICK = "--quick" in sys.argv
ENV = dict(os.environ, ASAN_OPTIONS="detect_leaks=0", UBSAN_OPTIONS="halt_on_error=1:print_stacktrace=1")
failures = []


def run(frames, keys="", saves=None, args=(), what=""):
    """Plays; returns the trace (a line a frame) and the last line."""
    own = saves is None
    if own:
        saves = tempfile.mkdtemp(prefix="sonic_check_")
    shots = tempfile.mkdtemp(prefix="sonic_shots_")
    p = subprocess.run([PLAY, "--saves", saves, "--out", shots, "--frames", str(frames), "--trace"]
                       + (["--keys", keys] if keys else []) + list(args),
                       capture_output=True, text=True, env=ENV, timeout=1200)
    shutil.rmtree(shots, ignore_errors=True)
    if own:
        shutil.rmtree(saves, ignore_errors=True)
    if p.returncode or "runtime error" in p.stderr or "AddressSanitizer" in p.stderr:
        failures.append(f"{what}: crashed or a sanitizer error\n{p.stderr[-2000:]}")
    lines = p.stdout.splitlines()
    return lines, lines[-1] if lines else ""


def frames_of(lines):
    """(frame, mode, zone, act, routine) of each traced frame."""
    out = []
    for l in lines:
        m = re.match(r"f(\d+) mode ([0-9a-f]+) zone (\d+)\.(\d+) sonic -?\d+,-?\d+ r(\d+)", l)
        if m:
            out.append((int(m[1]), int(m[2], 16), int(m[3]), int(m[4]), int(m[5])))
    return out


def save(d):
    p = os.path.join(d, "sonic.sav")
    if not os.path.exists(p):
        return None
    v = open(p, "rb").read()
    return {"ok": v[:4] == b"S1SV", "has_game": v[6], "zone": v[7], "act": v[8], "lives": v[9],
            "frames": struct.unpack_from("<I", v, 28)[0]}


def check(cond, msg):
    if not cond:
        failures.append(msg)


# a new game: the keys screen (the first time only), the title, Green Hill Zone; Home saves and quits
d = tempfile.mkdtemp(prefix="sonic_check_")
lines, last = run(800, "30-31:a,400-401:a,560-700:right", saves=d, what="a new game")
f = frames_of(lines)
check(any(m == 0x0C and z == 0 for _, m, z, a, _ in f), f"the new game did not start Green Hill Zone: {last}")
s = save(d)
check(s and s["ok"] and s["has_game"] == 1 and s["zone"] == 0 and s["frames"] > 0, f"Home did not save the game: {s}")
check(os.path.exists(os.path.join(d, "sonic_saves.py")), "no copy of the save in sonic_saves.py")
t0 = s["frames"] if s else 0

# next time: no keys screen, the title offers Continue, which goes back to the act
lines, last = run(700, "380-381:a,440-441:a", saves=d, what="Continue")
f = frames_of(lines)
check(f and f[0][1] != 0x0C, "the keys screen came back")
check(any(m == 0x0C and z == 0 for _, m, z, a, _ in f), f"Continue did not go back to Green Hill Zone: {last}")
s = save(d)
check(s and s["frames"] > t0, f"the time played did not go on: {s} (before {t0})")

# the copy in the Python script brings the save back when the file is gone
os.remove(os.path.join(d, "sonic.sav"))
run(10, saves=d, what="the save from its copy")
s = save(d)
check(s and s["has_game"] == 1, f"the save did not come back from sonic_saves.py: {s}")
shutil.rmtree(d, ignore_errors=True)

# the attract demos (Green Hill, Marble, Spring Yard, the Special Stage): the recorded buttons replayed,
# Sonic does not die (a wrong object or physics step soon sends him into a badnik or a pit)
d = tempfile.mkdtemp(prefix="sonic_check_")
open(os.path.join(d, "sonic.sav"), "wb").write(b"S1SV\1\1" + bytes(26))  # (the keys screen seen)
lines, last = run(10500, saves=d, what="the demos")
f = frames_of(lines)
demos = {z for _, m, z, a, _ in f if m == 0x08}
check({0, 2, 4} <= demos, f"not every level demo played: zones {sorted(demos)}")
check(any(m == 0x10 for _, m, z, a, _ in f), "the Special Stage demo did not play")
deaths = [x for x in f if x[1] == 0x08 and x[4] == 6]
check(not deaths, f"Sonic died in a demo: {deaths[:1]}")
shutil.rmtree(d, ignore_errors=True)

if not QUICK:
    # every act, running right (invincible), every special stage, the continue screen and the ending
    for z, acts in ((0, 3), (1, 4), (2, 3), (3, 3), (4, 3), (5, 3)):
        for a in range(acts):
            lines, last = run(900, "100-900:right,200-900:a", args=["--level", f"{z}.{a}", "--god"],
                              what=f"zone {z} act {a}")
            f = frames_of(lines)
            check(any(m == 0x0C and fz == z and fa == a for _, m, fz, fa, _ in f), f"zone {z} act {a} did not play")
    for n in range(6):
        lines, last = run(1500, "100-500:right,500-900:left,150-1500:a", args=["--level", f"7.{n}"],
                          what=f"special stage {n + 1}")
        check(any(x[1] == 0x10 for x in frames_of(lines)), f"special stage {n + 1} did not play")
    lines, last = run(900, "400-401:start", args=["--level", "9.2"], what="the continue screen")
    check(frames_of(lines)[-1][1] == 0x0C, f"the continue screen did not go back to the game: {last}")
    lines, last = run(3000, args=["--level", "8.6"], what="the good ending")
    check(any(x[1] == 0x1C for x in frames_of(lines)), f"the ending did not go on to the credits: {last}")
    lines, last = run(1500, args=["--level", "8.1"], what="the ending without the emeralds")

for x in failures:
    print("FAIL:", x)
print("sonic: all checks passed" if not failures else f"sonic: {len(failures)} failed")
sys.exit(1 if failures else 0)
