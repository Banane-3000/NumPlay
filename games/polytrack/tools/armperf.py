#!/usr/bin/env python3
"""The frame rate of each official track at an N0110's speed (tools/emu.py): armperf.py build/armperf.nwa
[TRACK...] (1 to 17; all by default, several at a time). Each track's recorded run is driven and drawn as in a
race; the work of each frame is timed. Steady 30 fps: no frame over 33 ms."""
import os
import struct
import sys
from concurrent.futures import ProcessPoolExecutor

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "..", "..", "tools"))
import emu  # noqa: E402

NAMES = ["Summer 1", "Summer 2", "Summer 3", "Summer 4", "Summer 5", "Summer 6", "Summer 7", "Winter 1", "Winter 2",
         "Winter 3", "Winter 4", "Winter 5", "Desert 1", "Desert 2", "Desert 3", "Desert 4", "Desert 5"]


def one(args):
    nwa, t = args
    emu.configure("n0110")
    c = emu.Calculator(nwa)
    c.run(100)  # (the app starts, then waits for its track)
    c.uc.mem_write(c.symbols["perf_track"], struct.pack("<I", t))
    c.uc.mem_write(c.symbols["perf_go"], struct.pack("<I", 1))
    c.run(30 * 60 * 1000)
    out = struct.unpack("<16I", bytes(c.uc.mem_read(c.symbols["perf_out"], 64)))
    done, = struct.unpack("<I", bytes(c.uc.mem_read(c.symbols["perf_done"], 4)))
    return t, out, done


def main():
    nwa = sys.argv[1]
    tracks = [int(a) - 1 for a in sys.argv[2:]] or list(range(17))
    bad = 0
    with ProcessPoolExecutor(max_workers=min(8, len(tracks))) as ex:
        for t, o, done in ex.map(one, [(nwa, t) for t in tracks]):
            frames, work, worst, over = o[0], o[1], o[2], o[3]
            det = "/".join(str(x) for x in [o[13], o[12], o[9]] + list(o[4:8]))
            print(f"{NAMES[t]:9}: {frames:5} frames, {work / max(frames, 1):5.1f} ms a frame, slowest {worst:3} ms, "
                  f"{over:3} over 33 ms (quality 0-6: {det}), finish {o[8]} ms" +
                  (f", slow ones: {o[10] // over} triangles, {o[11] // over} chunks, {o[14] / over:.1f} ms of physics, {o[15] // over} cached triangles" if over else "") + ("" if done else ", NOT done"))
            bad += over > 0 or not done
    sys.exit(1 if bad else 0)


if __name__ == "__main__":
    main()
