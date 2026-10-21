#!/usr/bin/env python3
"""Plays 2 Players in the ARM emulator for the README's pictures.

Runs the calculator build (TwoPlayers.nwa) in tools/emu.py as player 1 against the hard computer,
game after game: it reads the screen (the ball, the puck, the tanks, the light cycles) and presses
the keys a player would. Saves the raw frames, then the gallery's GIF (a few seconds of each game)
and a screenshot of each game.

Usage: bot.py TwoPlayers.nwa OUT [--fps 25] [--seconds 8]   (needs NumPy; gifsicle for small GIFs)
"""
import argparse
import math
import os
import sys

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "..", "..", "tools"))
import emu  # noqa: E402
import record  # noqa: E402

K = emu.KEYS
STEP = 30  # ms between two looks at the screen


def rgb(scr):
    s = scr.astype(np.int32)
    return (s >> 11) * 255 // 31, ((s >> 5) & 63) * 255 // 63, (s & 31) * 255 // 31


def near(scr, colour, tol=24):
    r, g, b = rgb(scr)
    return (abs(r - (colour >> 16)) < tol) & (abs(g - (colour >> 8 & 255)) < tol) & (abs(b - (colour & 255)) < tol)


class Bot:
    def __init__(self, nwa, out, fps):
        self.c = emu.Calculator(nwa)
        self.out, self.fps, self.frame = out, fps, 0
        self.marks = []  # (start ms, game) of each game played
        self.last = None
        self.dir = 0
        self.fired = 0

    def grab(self, calc):
        open(os.path.join(self.out, f"{self.frame:06d}_{int(calc.now_ms):06d}.raw"), "wb").write(bytes(calc.screen))
        self.frame += 1

    def run(self, ms):
        self.c.run(self.c.now_ms + ms, self.grab, 1000 / self.fps)

    def tap(self, key, wait=250):
        t = self.c.now_ms
        self.c.keys.append((t, t + 60, K[key]))
        self.run(wait)

    def hold(self, keys):
        t = self.c.now_ms
        for k in keys:
            self.c.keys.append((t, t + STEP, K[k]))
        self.run(STEP)

    def screen(self):
        return np.frombuffer(bytes(self.c.screen), dtype="<u2").reshape(240, 320).copy()

    # ---------------------------------------------------------------- the games
    def pong(self):
        s = self.screen()
        white = s == 0xFFFF
        col = np.nonzero(white[8:232, 26])[0]
        top = col.min() + 8 if len(col) else 105
        m = white.copy()
        m[:, :34] = m[:, 286:] = m[:, 155:165] = m[:9] = m[231:] = False
        m[14:46, 90:130] = m[14:46, 190:230] = False
        ys, xs = np.nonzero(m)
        target = 120
        if len(xs):
            bx, by = xs.mean(), ys.mean()
            if self.last and bx < self.last[0] - 0.5:  # coming: where it will be at the paddle
                vx, vy = bx - self.last[0], by - self.last[1]
                y = by + vy * (bx - 30) / -vx - 11
                span = 232 - 6 - 11
                for _ in range(6):
                    y = -y if y < 0 else 2 * span - y if y > span else y
                target = y + 11 + 3 + (9 if int(bx) % 3 == 0 else -9)
            self.last = (bx, by)
        c = top + 15
        self.hold(["up"] if c > target + 3 else ["down"] if c < target - 3 else [])

    def hockey(self):
        s = self.screen()
        rink = np.zeros(s.shape, bool)
        rink[30:232, 10:310] = True
        puck = (near(s, 0x22252E, 14) | near(s, 0x3B3F4C, 14)) & rink
        mallet = near(s, 0x88251F, 30) & rink
        mallet[:, 160:] = False
        if not puck.any() or not mallet.any():
            return self.hold([])
        qy, qx = (v.mean() for v in np.nonzero(puck))
        my, mx = (v.mean() for v in np.nonzero(mallet))
        if qx < 168:
            if mx > qx - 10:  # in front of it: round it, then strike
                tx, ty = qx - 26, qy + (24 if qy < 131 else -24)
            else:
                tx, ty = qx + 12, qy + (131 - qy) * 0.2
        else:
            tx, ty = 46, 131 + (qy - 131) / 2
        keys = []
        if tx > mx + 3:
            keys.append("right")
        if tx < mx - 3:
            keys.append("left")
        if ty > my + 3:
            keys.append("down")
        if ty < my - 3:
            keys.append("up")
        self.hold(keys)

    def tanks(self):
        s = self.screen()
        s[:24] = 0
        me = near(s, 0xEF4136, 30) | near(s, 0x88251F, 30)
        you = near(s, 0x2E7CF6, 30) | near(s, 0x1A478C, 30)
        if me.sum() < 40 or you.sum() < 40:
            return self.hold([])
        y0, x0 = (v.mean() for v in np.nonzero(me))
        y1, x1 = (v.mean() for v in np.nonzero(you))
        ys, xs = np.nonzero(near(s, 0x88251F, 30) & (np.abs(np.arange(320) - x0) < 16)[None, :]
                            & (np.abs(np.arange(240) - y0) < 16)[:, None])
        if not len(xs):
            return self.hold([])
        i = np.argmax((xs - x0) ** 2 + (ys - y0) ** 2)  # the gun's end
        face = math.atan2(ys[i] - y0, xs[i] - x0)
        want = math.atan2(y1 - y0, x1 - x0)
        d = (want - face + math.pi) % (2 * math.pi) - math.pi
        keys = ["right"] if d > 0.12 else ["left"] if d < -0.12 else []
        if abs(d) < 0.6 and math.hypot(x1 - x0, y1 - y0) > 50:
            keys.append("up")
        if abs(d) < 0.15 and self.c.now_ms - self.fired > 700:
            keys.append("shift")
            self.fired = self.c.now_ms
        self.hold(keys)

    def cycles(self):
        s = self.screen()
        grid = s[26:240:4, 2:320:4]
        r, g, b = rgb(grid)
        free = (r < 140) & (g < 120) & (b < 140)  # (the glow round a head is not a trail)
        heads = [tuple(h) for h in np.argwhere(grid == 0xFFFF)]
        if not self.last:  # a new round: player 1 sets off on the left
            self.last, self.dir = (19, 16), 0
        if not heads:
            return self.hold([])
        mine = min(heads, key=lambda h: abs(h[0] - self.last[0]) + abs(h[1] - self.last[1]))
        if abs(mine[0] - self.last[0]) + abs(mine[1] - self.last[1]) > 6:  # a round began again
            mine, self.last, self.dir = (19, 16), (19, 16), 0
        if self.last and self.last != mine:
            dy, dx = mine[0] - self.last[0], mine[1] - self.last[1]
            self.dir = {(0, 1): 0, (1, 0): 1, (0, -1): 2, (-1, 0): 3}.get((int(np.sign(dy)), int(np.sign(dx))), self.dir)
        self.last = mine
        steps = [(0, 1), (1, 0), (0, -1), (-1, 0)]
        best, bv = self.dir, -1
        for t in (0, 1, 3):
            d = (self.dir + t) % 4
            y, x = mine[0] + steps[d][0], mine[1] + steps[d][1]
            if not (0 <= y < free.shape[0] and 0 <= x < free.shape[1]) or not free[y, x]:
                continue
            seen, todo = {(y, x)}, [(y, x)]
            while todo and len(seen) < 300:
                cy, cx = todo.pop(0)
                for oy, ox in steps:
                    n = (cy + oy, cx + ox)
                    if n not in seen and 0 <= n[0] < free.shape[0] and 0 <= n[1] < free.shape[1] and free[n]:
                        seen.add(n)
                        todo.append(n)
            run = 0  # and a long way straight on
            while run < 12 and 0 <= y + steps[d][0] * run < free.shape[0] and 0 <= x + steps[d][1] * run < free.shape[1] \
                    and free[y + steps[d][0] * run, x + steps[d][1] * run]:
                run += 1
            v = len(seen) * 4 + run * 2 + (3 if t == 0 else 0)
            if v > bv:
                best, bv = d, v
        self.hold([] if best == self.dir else [["right", "down", "left", "up"][best]])

    def play(self, game, seconds):
        self.tap("ok", 1400)  # the rules
        self.tap("ok", 300)  # play
        self.marks.append((self.c.now_ms, game))
        self.last = None
        end = self.c.now_ms + seconds * 1000
        while self.c.now_ms < end:
            [self.pong, self.hockey, self.tanks, self.cycles][game]()
        self.marks.append((self.c.now_ms, -1))
        self.tap("back")  # Paused, Other games
        self.tap("down")
        self.tap("down")
        self.tap("ok", 400)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("nwa")
    ap.add_argument("out")
    ap.add_argument("--fps", type=int, default=25)
    ap.add_argument("--seconds", type=int, default=8)
    a = ap.parse_args()
    raw = os.path.join(a.out, "raw")
    os.makedirs(raw, exist_ok=True)
    bot = Bot(a.nwa, raw, a.fps)
    bot.run(900)
    for _ in range(3):  # the hard computer
        bot.tap("right", 150)
    bot.run(500)
    for g in range(4):
        bot.play(g, a.seconds)
        bot.tap("down", 300)
    frames = record.load_frames(raw)
    # the gallery's GIF: a few seconds of each game, from a moment into it
    part, t = [], 0
    for (start, g), (end, _) in zip(bot.marks[::2], bot.marks[1::2]):
        clip = [f for f in frames if start + 1500 <= f[0] <= min(end, start + 4500)]
        for i, (ms, im) in enumerate(clip):
            part.append((t + ms - clip[0][0], im))
        t = part[-1][0] + 1000 // a.fps
        clip[len(clip) // 2][1].save(os.path.join(a.out, f"game{g}.png"))
    record.save_gif(part, os.path.join(a.out, "twoplayer.gif"), 1, a.fps)
    print(f"bot: {len(frames)} frames, gallery GIF {os.path.getsize(os.path.join(a.out, 'twoplayer.gif')) // 1024} KB")


if __name__ == "__main__":
    main()
