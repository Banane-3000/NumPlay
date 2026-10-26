#!/usr/bin/env python3
"""The official tracks' runs on the calculator's processor (tools/emu.py, the Cortex-M7 the game runs on):
armcheck.py build/armcheck.nwa [TRACKS] checks that each finishes at the frame and with the hash in tests/routes.h,
as on the computers. (Slow: a few minutes a track.)"""
import os
import re
import struct
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "..", "..", "tools"))
import emu  # noqa: E402


def main():
    nwa = sys.argv[1]
    tracks = int(sys.argv[2]) if len(sys.argv) > 2 else 17
    src = open(os.path.join(HERE, "..", "tests", "routes.h")).read()
    ms = [int(x) for x in re.search(r"pt_route_ms\[17\] = \{([^}]*)\}", src).group(1).split(",")]
    hs = [int(x, 16) for x in re.search(r"pt_route_hash\[17\] = \{([^}]*)\}", src).group(1).replace("\n", "").split(",")]
    c = emu.Calculator(nwa)
    c.run(100)  # (the app starts, then waits)
    c.uc.mem_write(c.symbols["check_tracks"], struct.pack("<I", tracks))
    c.uc.mem_write(c.symbols["check_go"], struct.pack("<I", 1))
    c.run(60 * 60 * 1000)
    out = struct.unpack(f"<{2 * 17}I", bytes(c.uc.mem_read(c.symbols["check_out"], 4 * 2 * 17)))
    done, = struct.unpack("<I", bytes(c.uc.mem_read(c.symbols["check_done"], 4)))
    bad = 0
    for t in range(tracks):
        ok = out[2 * t] == ms[t] and out[2 * t + 1] == hs[t]
        bad += not ok
        print(f"track {t + 1:2}: {out[2 * t]:6} ms, hash {out[2 * t + 1]:08x}" + ("" if ok else
              f"  NOT the computers' ({ms[t]} ms, {hs[t]:08x})"))
    print(f"{c.insns / 1e9:.1f} G instructions" + ("" if done else ", NOT done"))
    sys.exit(1 if bad or not done else 0)


if __name__ == "__main__":
    main()
