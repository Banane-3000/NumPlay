#!/usr/bin/env python3
"""fps.py [--maps 11,12,...] [--low] [--out FILE]: the frame rate in every level, measured in the emulator
(tools/emu.py, with the calculator's 133652 bytes of RAM). A build that starts in the level and draws it from
all over (src/nw/scan.c: the middle of each subsector, looking 8 ways round, monsters awake) is run until every
view has been drawn, and the time of each frame is noted: its instructions (each frame's game tics, drawing and
sending to the display) and 40 ns for each pixel sent.

Times are given for the N0110 (216 MHz) and the N0120 (550 MHz, the same instructions taken 216/550 as long).
A frame within 1/35 s keeps up with the game's 35 tics a second; the frame rate of a view is 35 when it does,
else 1000 / its time (the game runs its tics at their rate, and draws as often as it can).

Needs the ARM toolchain (TOOLCHAIN=<its bin folder>, or on the PATH) and nwlink, as the build does."""
import argparse
import os
import struct
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
GAME = os.path.dirname(HERE)
sys.path.insert(0, os.path.join(GAME, "..", "..", "tools"))
import emu  # noqa: E402

ap = argparse.ArgumentParser()
ap.add_argument("--maps", default="11,12,13,14,15,16,17,18,19")
ap.add_argument("--low", action="store_true", help="low detail")
ap.add_argument("--build", default=os.path.join(GAME, "build", "fps"))
ap.add_argument("--out", default="")
ap.add_argument("--toolchain", default=os.environ.get("TOOLCHAIN", ""))
ap.add_argument("--nwlink", default=os.environ.get("NWLINK", "nwlink"))
a = ap.parse_args()

PUSH_MS = 320 * 240 * 40e-6  # a frame's pixels to the display
TIC_MS = 1000 / 35
rows = []
for m in (int(x) for x in a.maps.split(",")):
    bd = os.path.join(a.build, f"{m}{'low' if a.low else ''}")
    args = ["make", "-C", GAME, "-j4", "build", f"NWLINK={a.nwlink}", f"BUILD_DIR={bd}",
            f"EXTRA=-DNW_WARP={m} -DNW_SCAN={2 if a.low else 1}"]
    if a.toolchain:
        args.append(f"TOOLCHAIN={a.toolchain}")
    r = subprocess.run(args, capture_output=True, text=True)
    if r.returncode:
        sys.exit(f"E{m // 10}M{m % 10}: the build failed\n{r.stderr[-2000:]}")
    emu.configure(model="n0110", ram_length=133652)
    c = emu.Calculator(os.path.join(bd, "doom.nwa"))
    sym = c.symbols

    def u32(name):
        return struct.unpack("<I", bytes(c.uc.mem_read(sym[name], 4)))[0]

    chunk = emu.CPU_HZ // 10000  # a tenth of a millisecond
    frames = []  # (work ms at 216 MHz, the view: subsector, direction)
    last_frames = u32("perf_frames")
    last_t = c.now_ms - c.slept_ms
    started = False
    while not c.exited and not u32("nw_scan_done"):
        c.uc.emu_start(c.pc, 0xFFFFFFFF, count=chunk)
        c.pc = c.reg(emu.UC_ARM_REG_PC) | 1
        c.insns += chunk
        c.now_ms += 0.1
        n = u32("perf_frames")
        if n != last_frames:
            t = c.now_ms - c.slept_ms
            ss = struct.unpack("<i", bytes(c.uc.mem_read(sym["nw_scan_ss"], 4)))[0]
            if started:
                frames.append((t - last_t, ss))
            started = started or ss >= 0
            last_frames, last_t = n, t
        if c.now_ms > 3_600_000:
            sys.exit(f"E{m // 10}M{m % 10}: the scan did not end")
    # (each frame's pushes were counted as instructions by the emulator, a pixel for a quarter of one)
    work = sorted(f[0] - 320 * 240 / 4 / 216e3 for f in frames)

    def model(k):
        ms = [w * k + PUSH_MS for w in work]
        fps = [35 if t <= TIC_MS else 1000 / t for t in ms]
        return {"median_ms": ms[len(ms) // 2], "p99_ms": ms[int(len(ms) * 0.99)], "max_ms": ms[-1],
                "at35": sum(1 for t in ms if t <= TIC_MS) / len(ms), "min_fps": min(fps),
                "mean_fps": len(fps) / sum(1 / f for f in fps)}

    row = {"map": f"E{m // 10}M{m % 10}", "frames": len(work), "n0110": model(1.0), "n0120": model(216 / 550)}
    rows.append(row)
    line = f"{row['map']} {'low' if a.low else 'high'} detail, {len(work)} views:"
    for k in ("n0120", "n0110"):
        s = row[k]
        line += (f"  {k.upper()} median {s['median_ms']:.1f} ms, 99% within {s['p99_ms']:.1f} ms, slowest "
                 f"{s['max_ms']:.1f} ms; {100 * s['at35']:.1f}% at 35 fps, lowest {s['min_fps']:.0f} fps, "
                 f"mean {s['mean_fps']:.1f} fps")
    print(line, flush=True)
if a.out:
    import json
    json.dump(rows, open(a.out, "w"), indent=1)
