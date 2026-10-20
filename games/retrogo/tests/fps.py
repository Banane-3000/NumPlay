#!/usr/bin/env python3
"""Retro-Go's speed in tools/emu.py: starts the one game of a RetroGo.nwa built with ROMS="that game"
(OK on the tab, OK on the game, New game, OK on the controls), plays it with the keys given, and prints
the frames emulated and drawn a second, from the app's perf_frames and perf_drawn counters.

  fps.py RetroGo.nwa [--model n0110|n0120] [--keys "ms-ms:key,..."] [--ms 20000]

tools/emu.py runs the CPU at 216 million instructions a second (the N0110 and N0115's clock);
--model n0120 runs it at 550 million, the N0120's. Build retrogo-full.nwa (unstripped) for this."""
import argparse
import os
import struct
import sys

here = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(here, "..", "..", "..", "tools"))
import emu  # noqa: E402

ap = argparse.ArgumentParser()
ap.add_argument("nwa")
ap.add_argument("--model", choices=["n0110", "n0120"], default="n0120")
ap.add_argument("--keys", default="")
ap.add_argument("--ms", type=int, default=20000)
ap.add_argument("--from-ms", type=int, default=8000, help="when the counting starts")
ap.add_argument("--shot", help="a picture of the end, PNG")
a = ap.parse_args()

emu.configure(a.model)
emu.CPU_HZ = 550_000_000 if a.model == "n0120" else 216_000_000
c = emu.Calculator(a.nwa)
start = "300-400:ok,900-1000:ok,1500-1600:ok,2100-2200:ok"  # tab, game, New game, controls
c.keys = emu.parse_keys(start + ("," + a.keys if a.keys else ""))


def counters():
    f = struct.unpack("<I", bytes(c.uc.mem_read(c.symbols["perf_frames"], 4)))[0]
    d = struct.unpack("<I", bytes(c.uc.mem_read(c.symbols["perf_drawn"], 4)))[0]
    return f, d


c.run(a.from_ms)
f0, d0 = counters()
t0, s0 = c.now_ms, c.slept_ms
c.run(a.ms)
f1, d1 = counters()
span = (c.now_ms - t0) / 1000
busy = 100 - 100 * (c.slept_ms - s0) / max(c.now_ms - t0, 1)
print(f"{os.path.basename(a.nwa)} {a.model}: {(f1 - f0) / span:.1f} frames emulated a second, "
      f"{(d1 - d0) / span:.1f} drawn, CPU busy {busy:.0f}%")
if a.shot:
    c.save_png(a.shot)
