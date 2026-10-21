#!/usr/bin/env python3
"""panel.py NWA [--model n0120|n0110] [--hz 40] [--dir ltr|rtl] [--from MS] [--ms MS] [--keys SPEC] [--out DIR]:
Doom in the emulator (tools/emu.py) with the calculator's display panel as it refreshes itself: every 1/hz s, after
its blanking time, it goes over its memory a column at a time (left to right, or right to left), and
eadk_display_wait_for_vblank waits for the blanking time. Each refresh's picture is compared with the frames the app
sent: a refresh that shows parts of two frames is torn (its picture is saved in DIR, the first few).

Time is the calculator's: the emulator runs 216 million instructions a second, which is the N0110's clock; on the N0120
(550 MHz) everything the app does takes 216/550 of the time. Sending pixels takes 40 ns each."""
import argparse
import os
import struct
import sys

import numpy as np

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "..", "tools"))
import emu  # noqa: E402

ap = argparse.ArgumentParser()
ap.add_argument("nwa")
ap.add_argument("--model", default="n0120", choices=["n0120", "n0110"])
ap.add_argument("--hz", type=float, default=40.0)
ap.add_argument("--dir", default="ltr", choices=["ltr", "rtl"])
ap.add_argument("--vblank", default="end", choices=["start", "end"], help="when the wait for vblank returns")
ap.add_argument("--from", dest="t0", type=int, default=2000, help="real ms: refreshes before are not counted")
ap.add_argument("--ms", type=int, default=12000, help="real ms to run")
ap.add_argument("--keys", default="")
ap.add_argument("--out", default="")
ap.add_argument("--ns-per-pixel", type=float, default=40.0)
a = ap.parse_args()

emu.configure(model=a.model, ram_length=133652)
K = 550 / 216 if a.model == "n0120" else 1.0  # emulator ms for one real ms
T = 1000.0 / a.hz  # the refresh period, real ms
V = T * 24 / 344  # its blanking time (24 lines of porch for 320)
DT = (T - V) / 320  # the time to refresh one column


class Panel(emu.Calculator):
    def __init__(self, nwa):
        super().__init__(nwa)
        self.mem = np.zeros((240, 320), np.uint16)
        self.ref = 0  # the refresh being done
        self.col = 0  # the columns of it done
        self.comp = np.zeros((240, 320), np.uint16)
        self.frames = []  # the last whole frames sent
        self.last_frame_count = None
        self.refreshes = self.torn = 0
        self.torn_pixels = []
        self.saved = 0
        self.frame_times = []

    def real(self):
        return self.now_ms / K

    def scan_until(self, t):
        """the panel's refreshes up to real time t"""
        while True:
            start = self.ref * T + V
            n = int((t - start) / DT) + 1 if t >= start else 0
            n = min(n, 320)
            if n > self.col:
                cols = slice(self.col, n) if a.dir == "ltr" else slice(320 - n, 320 - self.col)
                self.comp[:, cols] = self.mem[:, cols]
                self.col = n
            if self.col < 320:
                return
            self.judge()
            self.ref += 1
            self.col = 0

    def judge(self):
        if self.ref * T < a.t0 or not self.frames:
            return
        self.refreshes += 1
        cands = self.frames + [self.mem]
        best = min(int(np.count_nonzero(self.comp != f)) for f in cands)
        if best:
            self.torn += 1
            self.torn_pixels.append(best)
            # which of the frames each band of 64 columns shows (-1: none of them)
            bands = []
            for b in range(5):
                cols = slice(64 * b, 64 * b + 64)
                m = [k for k, f in enumerate(cands) if not np.count_nonzero(self.comp[:, cols] != f[:, cols])]
                bands.append(m[-1] - len(cands) + 1 if m else None)
            print(f"torn at {self.ref * T:.0f} ms: bands show frames {bands} (0 the last sent)", flush=True)
            if a.out and self.saved < 6:
                from PIL import Image
                c = self.comp.astype(np.uint32)
                rgb = np.stack([((c >> 11) & 31) * 255 // 31, ((c >> 5) & 63) * 255 // 63, (c & 31) * 255 // 31], -1)
                Image.fromarray(rgb.astype(np.uint8), "RGB").save(f"{a.out}/torn_{int(self.ref * T):06d}.png")
                self.saved += 1

    def frame_count(self):
        return struct.unpack("<I", bytes(self.uc.mem_read(self.symbols["perf_frames"], 4)))[0]

    def _intr(self, uc, intno, data):
        if intno != 2:
            return super()._intr(uc, intno, data)
        pc = self.reg(emu.UC_ARM_REG_PC)
        n = struct.unpack("<H", uc.mem_read(pc - 2, 2))[0] & 0xFF
        r0, r1 = self.reg(emu.UC_ARM_REG_R0), self.reg(emu.UC_ARM_REG_R1)
        if n == 19:  # push: the frame before it, if one ended
            fc = self.frame_count()
            if fc != self.last_frame_count:
                if self.last_frame_count is not None:
                    self.frames = (self.frames + [self.mem.copy()])[-3:]
                    self.frame_times.append(self.real())
                self.last_frame_count = fc
            x, y, w, h = r0 & 0xFFFF, r0 >> 16, r1 & 0xFFFF, r1 >> 16
            t = self.real()
            self.scan_until(t)
            px = np.frombuffer(bytes(uc.mem_read(self.reg(emu.UC_ARM_REG_R2), w * h * 2)), "<u2").reshape(h, w)
            self.mem[y:y + h, x:x + w] = px
            self.now_ms += w * h * a.ns_per_pixel / 1e6 * K
            self.scan_until(self.real())
            return super()._intr(uc, intno, data)
        if n == 21:  # wait for vblank
            t = self.real()
            k = int(t // T) + 1
            when = k * T + (V if a.vblank == "end" else 0)
            if a.vblank == "end" and t < (k - 1) * T + V:
                when = (k - 1) * T + V
            self.scan_until(when)
            self.now_ms = when * K
            uc.reg_write(emu.UC_ARM_REG_R0, 1)
            return
        if n == 48:  # millis
            ms = int(self.real())
            uc.reg_write(emu.UC_ARM_REG_R0, ms & 0xFFFFFFFF)
            uc.reg_write(emu.UC_ARM_REG_R1, ms >> 32)
            return
        if n in (49, 50):  # msleep, usleep
            d = r0 if n == 49 else r0 / 1000
            self.now_ms += d * K
            self.slept_ms += d * K
            self.scan_until(self.real())
            return
        return super()._intr(uc, intno, data)

    def go(self, until_real_ms):
        chunk = emu.CPU_HZ // 10000  # a tenth of an emulator millisecond
        while not self.exited and self.real() < until_real_ms:
            self.uc.emu_start(self.pc, 0xFFFFFFFF, count=chunk)
            self.pc = self.reg(emu.UC_ARM_REG_PC) | 1
            self.insns += chunk
            self.now_ms += 0.1
            self.scan_until(self.real())


c = Panel(a.nwa)
c.keys = [(int(s * K), int(e * K), k) for s, e, k in emu.parse_keys(a.keys)] if a.keys else []
if a.out:
    os.makedirs(a.out, exist_ok=True)
c.go(a.ms)
ft = [t for t in c.frame_times if t >= a.t0]
fps = (len(ft) - 1) / ((ft[-1] - ft[0]) / 1000) if len(ft) > 1 else 0
print(f"{a.model} {a.hz:g} Hz {a.dir}: {c.refreshes} refreshes, {c.torn} torn"
      + (f" (pixels: median {sorted(c.torn_pixels)[len(c.torn_pixels) // 2]}, most {max(c.torn_pixels)})"
         if c.torn else "") + f", {fps:.1f} frames a second")
