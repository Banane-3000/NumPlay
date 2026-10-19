#!/usr/bin/env python3
"""Level Devil's rooms: sixteen levels of five, and how to clear each one.

Writes OUT/rooms.inc, the rooms packed (raw DEFLATE) for src/main.c, which unpacks them when the
game starts. The format is described in src/main.c; the numbers below (kinds, flags, triggers,
motions, modes) follow it. tests/solutions.txt has the keys that clear each room.

The room is 160 x 120 pixels of the game: the ceiling is the top 10 rows, the side walls 4 wide,
and most floors are at 100. The runner is 5 x 7 and starts at x 14 on the floor. It runs a
little over a pixel a step and jumps about 20 high and 30 across (60 steps a second).


Usage: rooms.py OUT
"""
import os
import sys
import zlib

# kinds, flags, triggers, motions, modes (src/main.c)
BLOCK, SECRET, SPIKE_U, SPIKE_D, SPIKE_L, SPIKE_R, DOOR, SAW, SPRING, WARP, SWITCH, BUTTON = range(12)
HIDDEN, VANISH, GHOST, TRAP = 0x10, 0x20, 0x40, 0x80
START, RIGHT, LEFT, UP, DOWN, TOUCH, AFTER, NEAR = range(8)
ONCE, LOOP, CHASE, FALL = range(4)
BACK, ICE, AIR, FLIP, WRAP = 1, 2, 4, 8, 16
SPEEDS = [0, 2, 4, 6, 8, 12, 16, 20, 24, 32, 40, 48, 64, 80, 96, 128]  # 16ths of a pixel a step
FLOOR_Y = 100


class Obj:
    def __init__(self, kind, x, y, w, h, flags=0, trap=None):
        self.kind, self.x, self.y, self.w, self.h, self.flags, self.trap = kind, x, y, w, h, flags, trap


def trap(when=START, at=0, does=ONCE, dx=0, dy=0, speed=0, delay=0, after=0, before=0):
    """What sets it off (when, at), how many deaths it waits for (after: there from death n on;
    before: there until death n), what it does: dx, dy at speed (pixels a step; 0 at once),
    delay in steps (rounded to fours)."""
    s = min(range(16), key=lambda i: abs(SPEEDS[i] - speed * 16)) if speed else 0
    gate = after if after else (before + 4 if before else 0)
    return dict(when=when, at=at, does=does, dx=dx, dy=dy, speed=s, delay=(delay + 3) // 4, gate=gate)


# the usual traps
def falls(at, **k):
    """drops away once the runner's right side passes x = at"""
    return trap(RIGHT, at, FALL, **k)


def pops(at, dy=-5, **k):
    """spikes out of the floor (or of whatever hides them) once the runner passes x = at"""
    return trap(RIGHT, at, ONCE, dy=dy, speed=4, **k)


def block(x, y, w, h, trap=None, flags=0):
    return Obj(BLOCK, x, y, w, h, flags, trap)


def floor(x, w, y=FLOOR_Y, **k):
    return block(x, y, w, 120 - y, **k)


def spikes(x, y, n, side="u", trap=None, flags=0):
    """n teeth (5 pixels each) on a floor (u, pointing up), under a ceiling (d), on a wall (l: on its
    left side, pointing left; r: on its right side, pointing right)"""
    kind = {"u": SPIKE_U, "d": SPIKE_D, "l": SPIKE_L, "r": SPIKE_R}[side]
    w, h = (5 * n, 5) if side in "ud" else (5, 5 * n)
    return Obj(kind, x, y, w, h, flags, trap)


def door(x, y=FLOOR_Y - 20, trap=None, fake=False, hanging=False, flags=0):
    return Obj(DOOR, x, y, 11, 1 if hanging else 20, flags | (GHOST if fake else 0), trap)


def saw(x, y, size, trap=None, flags=0):
    """a saw of that size, its top-left at (x, y)"""
    return Obj(SAW, x, y, size, size, flags, trap)


def spring(x, y=FLOOR_Y - 3, w=8, trap=None, flags=0):
    return Obj(SPRING, x, y, w, 3, flags, trap)


def warp(a, b, trap=None, flags=0):
    """two ends, 8 x 12 each, their top-left corners"""
    return Obj(WARP, a[0], a[1], b[0], b[1], flags, trap)


def switch(x, y, w, h, toggles, when=RIGHT, at=0, **k):
    """an invisible line that toggles modes once"""
    t = trap(when, at, ONCE, **k)
    t["dx"] = toggles
    return Obj(SWITCH, x, y, w, h, 0, t)


def button(x, toggles, y=FLOOR_Y - 2, w=8, ceiling=False):
    """a button on the floor (or under a ceiling, y its top) that toggles modes each time it is touched"""
    t = trap(TOUCH, 0, ONCE)
    t["dx"] = toggles
    return Obj(BUTTON, x, y, w, 2, GHOST if ceiling else 0, t)


ROOMS = []  # (modes, start, objects)


def room(objs, modes=0, start=(14, FLOOR_Y)):
    ROOMS.append((modes, start, objs))


def chain(objs, first, delay, does=FALL, **k):
    """each object's trap goes off a while after the one before it; the first one's when the runner
    passes x = first"""
    for i, o in enumerate(objs):
        o.trap = trap(AFTER if i else RIGHT, objs[i - 1] if i else first, does, delay=delay if i else 0, **k)
    return objs


def popping(x, n, at, y=FLOOR_Y, **k):
    """spikes hidden in the floor at y, out once the runner passes x = at"""
    return spikes(x, y, n, trap=pops(at, **k))


def always(**k):
    """no trap, but there only after (or before) so many deaths"""
    return trap(START, 0, ONCE, **k)


DOOR_X = 140  # where most doors are, at the end of the floor

# ------------------------------------------------------------------ 1: the floor drops away
# the floor in front of the door drops; after a death it holds, and spikes come up where the runner lands
room([floor(0, 96), floor(96, 24, trap=falls(88, before=1)), floor(96, 24, trap=always(after=1)),
      popping(120, 3, 92, after=1), floor(120, 40), door(DOOR_X)])
room([floor(0, 50), floor(50, 22, trap=falls(44)), floor(72, 30), floor(102, 22, trap=falls(96)), floor(124, 36),
      door(DOOR_X)])
# the floor behind the runner drops away, a piece after the other: no stopping
room([floor(0, 24)] + chain([floor(24 + 20 * i, 20) for i in range(5)], 30, 18) + [floor(124, 36), door(DOOR_X)])
# stepping stones over a pit, dropping soon after they are stepped on
room([floor(0, 40)] + [block(52 + 30 * i, 100, 16, 6, trap=trap(TOUCH, 0, FALL, delay=14)) for i in range(3)]
     + [floor(140, 20), door(144)])
# the door's own floor drops when the runner comes near: jump into the door from afar
room([floor(0, 100), block(100, 100, 60, 20, trap=trap(NEAR, 22, FALL)), door(130, trap=trap(NEAR, 18, FALL))])

# ------------------------------------------------------------------ 2: spikes
# spikes come up in front of the door
room([floor(0, 160), popping(118, 3, 104), door(DOOR_X)])
# a low ceiling, whose spikes drop when the runner jumps: hop low over the spikes on the floor
room([floor(0, 160), block(50, 10, 60, 58), spikes(50, 63, 12, "d", trap=trap(UP, 90, ONCE, dy=5, speed=4)),
      spikes(76, 95, 2), door(DOOR_X)])
# spike pits, and more spikes come up where a long jump would land
room([floor(0, 160), spikes(30, 95, 3), popping(57, 2, 34), spikes(85, 95, 3), popping(112, 2, 89), door(DOOR_X)])
# a step: spikes come out of its side as the runner nears it, then out of its top
room([floor(0, 160), block(70, 84, 40, 16), spikes(70, 84, 3, "l", trap=trap(RIGHT, 56, ONCE, dx=-5, speed=4)),
      popping(90, 2, 78, y=84), door(DOOR_X)])
# spikes come up behind the runner, one after another, and in front of the door
room([floor(0, 160)] + chain([spikes(30 + 15 * i, 100, 3) for i in range(6)], 50, 14, does=ONCE, dy=-5, speed=4)
     + [popping(122, 2, 112), door(DOOR_X)])

# ------------------------------------------------------------------ 3: walls
# over the gap, a pillar comes up out of the floor where the runner would land
room([floor(0, 70), floor(80, 80), block(86, 100, 8, 20, trap=trap(RIGHT, 64, ONCE, dy=-16, speed=3)), door(DOOR_X)])
# a wall slides out of the right wall at the runner: jump it
room([floor(0, 160), block(148, 86, 12, 14, trap=trap(RIGHT, 40, ONCE, dx=-110, speed=1.5)), door(128)])
# a wall comes down from the ceiling, low enough to walk under but not to jump; then a crusher
room([floor(0, 160), block(66, 10, 14, 60, trap=trap(RIGHT, 50, ONCE, dy=22, speed=3)),
      block(100, 10, 14, 30, trap=trap(RIGHT, 76, LOOP, dy=60, speed=3)), door(DOOR_X)])
# the ceiling comes down on the whole room: hurry
room([floor(0, 160), block(4, 10, 152, 10, trap=trap(RIGHT, 20, ONCE, dy=78, speed=0.5)), spikes(60, 95, 2),
      spikes(100, 95, 3), door(DOOR_X)])
# the left wall comes after the runner
room([floor(0, 160), block(0, 10, 8, 90, trap=trap(RIGHT, 22, ONCE, dx=124, speed=1)), spikes(60, 95, 2),
      spikes(100, 95, 3), door(DOOR_X)])

# ------------------------------------------------------------------ 4: the door
# the door goes behind the runner, and spikes come up between
d = door(120, trap=trap(NEAR, 30, ONCE, dx=-96))
room([floor(0, 160), d, spikes(60, 100, 3, trap=trap(AFTER, d, ONCE, dy=-5, speed=4))])
# the door rises out of reach on a pillar: a ledge left behind helps
room([floor(0, 160), block(120, 100, 30, 20, trap=trap(NEAR, 26, ONCE, dy=-26, speed=1)),
      door(130, trap=trap(NEAR, 26, ONCE, dy=-26, speed=1)), block(76, 84, 20, 4)])
# a fake door: spikes under it; the real one shows up at the end
s = spikes(52, 100, 4, trap=trap(NEAR, 14, ONCE, dy=-5, speed=4))
room([floor(0, 160), door(56, fake=True), s, door(DOOR_X, flags=HIDDEN, trap=trap(AFTER, s, ONCE))])
# the door rides back and forth on a platform over a pit of spikes
room([floor(0, 44), block(48, 112, 108, 8), spikes(48, 107, 21),
      block(60, 94, 24, 4, trap=trap(START, 0, LOOP, dx=60, speed=0.5)),
      door(70, y=74, trap=trap(START, 0, LOOP, dx=60, speed=0.5))])
# the door jumps to the start, and the floor between drops away
d = door(130, trap=trap(NEAR, 22, ONCE, dx=-110))
room([floor(0, 70), floor(70, 26, trap=trap(AFTER, d, FALL, delay=8)), floor(96, 64), d])

# ------------------------------------------------------------------ 5: backwards
room([floor(0, 160), spikes(70, 95, 3), door(DOOR_X)], modes=BACK)
# the keys turn round halfway
room([floor(0, 160), switch(0, 0, 0, 0, BACK, at=66), spikes(84, 95, 3), door(DOOR_X)])
# back to front, and the right way round again in the middle of a jump
room([floor(0, 56), floor(84, 76), switch(0, 0, 0, 0, BACK, when=UP, at=90), door(DOOR_X)], modes=BACK)
# back to front over dropping floors
room([floor(0, 50), floor(50, 24, trap=falls(46)), floor(74, 26), floor(100, 24, trap=falls(96)), floor(124, 36),
      door(DOOR_X)], modes=BACK)
# the keys turn round, again and again
room([floor(0, 160), switch(0, 0, 0, 0, BACK, at=40), spikes(56, 95, 2), switch(0, 0, 0, 0, BACK, at=80),
      spikes(96, 95, 2), switch(0, 0, 0, 0, BACK, at=116), popping(126, 2, 116), door(DOOR_X)])

# ------------------------------------------------------------------ 6: fake ground
# a piece of floor that isn't there
room([floor(0, 60), floor(60, 26, flags=GHOST), floor(86, 74), door(DOOR_X)])
# no floor, but there is one: an invisible bridge; the platform over it is the fake
room([floor(0, 40), block(44, 112, 76, 8), spikes(44, 107, 15)] + [Obj(SECRET, 40 + 20 * i, 100, 20, 4) for i in range(4)]
     + [block(62, 76, 36, 4, flags=GHOST), floor(120, 40), door(DOOR_X)])
# a wall to the ceiling: it isn't there, but the pit behind it is
room([floor(0, 120), block(110, 10, 10, 90, flags=GHOST), floor(136, 24), door(DOOR_X)])
# floors, real and fake, side by side
room([floor(0, 40), floor(40, 20, flags=GHOST), floor(60, 20), floor(80, 20), floor(100, 20, flags=GHOST),
      floor(120, 40), door(DOOR_X)])
# platforms that vanish once stepped on, and the last one is fake
room([floor(0, 30), block(34, 112, 106, 8), spikes(34, 107, 21)]
     + [block(40 + 24 * i, 96, 14, 4, trap=trap(TOUCH, 0, ONCE, delay=10), flags=VANISH) for i in range(3)]
     + [block(112, 96, 14, 4, flags=GHOST), floor(140, 20), door(144)])

# ------------------------------------------------------------------ 7: springs
room([floor(0, 160), spring(60), block(100, 58, 56, 42), door(140, y=38)])
# a spring comes out under the runner, into the spikes on the ceiling
room([floor(0, 160), spring(70, flags=HIDDEN, trap=trap(RIGHT, 72)), block(56, 10, 40, 30), spikes(56, 40, 8, "d"),
      door(DOOR_X)])
# from spring to spring over the spikes
room([floor(0, 40), block(40, 108, 80, 12), spikes(40, 103, 16), block(62, 96, 10, 12), spring(63, y=93),
      block(96, 96, 10, 12), spring(97, y=93), floor(120, 40), door(DOOR_X)])
# the spring slides away as the runner comes
room([floor(0, 40), block(40, 108, 116, 12), spikes(40, 103, 23), block(56, 96, 40, 12),
      spring(60, y=93, trap=trap(NEAR, 12, ONCE, dx=26, speed=2)), block(120, 50, 36, 8), door(140, y=30)])
# up a spring, between the spikes of the ceiling
room([floor(0, 160), spring(46), spikes(30, 10, 3, "d"), spikes(60, 10, 8, "d"), block(84, 56, 72, 6),
      popping(100, 3, 100, y=56), door(DOOR_X, y=36)])

# ------------------------------------------------------------------ 8: warps
room([floor(0, 160), block(76, 10, 8, 90), warp((50, 88), (110, 88)), door(DOOR_X)])
# the warp moves as the runner comes
room([floor(0, 160), block(76, 10, 8, 90), warp((40, 88), (126, 88), trap=trap(NEAR, 10, ONCE, dx=16, speed=2)),
      spikes(30, 95, 2), door(DOOR_X)])
# out of the warp over spikes: keep running
room([floor(0, 160), block(56, 10, 8, 90), warp((30, 88), (70, 66)), block(66, 78, 20, 4), spikes(64, 95, 12),
      door(DOOR_X)])
# a warp comes up behind the runner over the gap, and takes it back to the start
room([floor(0, 60), floor(84, 76), warp((90, 76), (14, 88), flags=HIDDEN, trap=trap(UP, 90, ONCE)), door(DOOR_X)])
# warps, up and down
room([floor(0, 160), block(50, 10, 6, 90), block(104, 10, 6, 90), block(56, 52, 22, 4), block(84, 52, 20, 4),
      spikes(56, 95, 10), warp((36, 88), (62, 40)), warp((92, 40), (120, 88)), door(DOOR_X)])

# ------------------------------------------------------------------ 9: crushers
room([floor(0, 160), block(70, 10, 20, 30, trap=trap(RIGHT, 50, LOOP, dy=60, speed=2)), door(DOOR_X)])
# a piece of the ceiling falls as the runner passes
room([floor(0, 160), block(60, 10, 30, 14, trap=trap(RIGHT, 58, ONCE, dy=76, speed=4)), spikes(100, 95, 2),
      door(DOOR_X)])
# two crushers out of step
room([floor(0, 160), block(54, 10, 18, 30, trap=trap(RIGHT, 26, LOOP, dy=60, speed=2)),
      block(96, 10, 18, 30, trap=trap(RIGHT, 26, LOOP, dy=60, speed=2, delay=20)), door(DOOR_X)])
# pieces of the ceiling fall as the runner comes near: wait for each one
room([floor(0, 160)] + [block(36 + 26 * i, 10, 16, 14, trap=trap(NEAR, 28, FALL)) for i in range(4)] + [door(DOOR_X)])
# crushers over dropping floors
room([floor(0, 40), floor(40, 30, trap=falls(36)), floor(70, 90),
      block(90, 10, 16, 30, trap=trap(START, 0, LOOP, dy=60, speed=2.5)), door(DOOR_X)])

# ------------------------------------------------------------------ 10: saws
room([floor(0, 160), saw(50, 86, 14, trap=trap(START, 0, LOOP, dx=60, speed=1)), door(DOOR_X)])
# a saw comes up out of the floor
room([floor(0, 160), saw(84, 100, 16, trap=trap(RIGHT, 70, ONCE, dy=-12, speed=3)), door(DOOR_X)])
# a saw drops from the ceiling
room([floor(0, 160), saw(96, 10, 16, trap=trap(RIGHT, 84, FALL)), spikes(120, 95, 2), door(DOOR_X)])
# saws up and down
room([floor(0, 160), saw(50, 20, 14, trap=trap(START, 0, LOOP, dy=66, speed=1.5)),
      saw(84, 86, 14, trap=trap(START, 0, LOOP, dy=-66, speed=1.5)),
      saw(118, 20, 14, trap=trap(START, 0, LOOP, dy=66, speed=1.5)), door(DOOR_X)])
# the saw in the door's way follows it
room([floor(0, 160), saw(110, 84, 16, trap=trap(NEAR, 30, ONCE, dx=-40, speed=1.5)), spikes(40, 95, 2), door(DOOR_X)])

# ------------------------------------------------------------------ 11: chase
room([floor(0, 160), saw(0, 84, 16, trap=trap(RIGHT, 40, CHASE, dx=1, speed=0.9)), spikes(64, 95, 2),
      spikes(104, 95, 2), door(DOOR_X)], start=(26, FLOOR_Y))
# a wall of spikes
room([block(0, 10, 6, 90, trap=trap(RIGHT, 30, ONCE, dx=124, speed=1)),
      spikes(6, 10, 18, "r", trap=trap(RIGHT, 30, ONCE, dx=124, speed=1)), floor(60, 20, trap=falls(56)),
      floor(0, 60), floor(80, 80), door(DOOR_X)])
# a saw that follows the runner everywhere, slowly
room([floor(0, 160), block(40, 84, 30, 16), block(80, 68, 30, 32), block(120, 52, 36, 48),
      saw(10, 20, 14, trap=trap(START, 0, CHASE, dx=1, dy=1, speed=0.5, delay=20)), door(140, y=32)])
# saws from both sides
room([floor(0, 160), saw(0, 86, 14, trap=trap(RIGHT, 40, CHASE, dx=1, speed=0.7)),
      saw(146, 86, 14, trap=trap(RIGHT, 40, CHASE, dx=1, speed=0.7)), block(70, 82, 20, 4), door(75, y=62)],
     start=(26, FLOOR_Y))
# chased over dropping floors
room([floor(0, 30), saw(0, 84, 16, trap=trap(RIGHT, 36, CHASE, dx=1, speed=0.9))]
     + chain([floor(30 + 20 * i, 20) for i in range(5)], 44, 16) + [floor(130, 30), door(DOOR_X)], start=(22, FLOOR_Y))

# ------------------------------------------------------------------ 12: jumps in the air
room([floor(0, 160), block(70, 40, 12, 60), door(DOOR_X)], modes=AIR)
# careful: spikes on the ceiling
room([floor(0, 50), block(54, 112, 70, 8), spikes(54, 107, 14), floor(124, 36), spikes(40, 10, 18, "d"), door(DOOR_X)],
     modes=AIR)
# the jumps in the air stop over the pit
room([floor(0, 50), block(54, 112, 70, 8), spikes(54, 107, 14), floor(124, 36),
      switch(0, 0, 0, 0, AIR, at=96), door(DOOR_X)], modes=AIR)
# up a tower with spikes on its side, under spikes on the ceiling
room([floor(0, 160), block(70, 34, 30, 66), spikes(65, 44, 10, "l"), spikes(20, 10, 9, "d"), door(80, y=14)],
     modes=AIR)
# a wide pit, saws in the air
room([floor(0, 30), block(30, 112, 100, 8), spikes(30, 107, 20), floor(130, 30),
      saw(60, 40, 14, trap=trap(START, 0, LOOP, dy=40, speed=1)), saw(96, 70, 14, trap=trap(START, 0, LOOP, dy=-40, speed=1)),
      door(DOOR_X)], modes=AIR)

# ------------------------------------------------------------------ 13: gravity
room([floor(0, 40), block(40, 104, 116, 16), spikes(40, 99, 23), button(20, FLIP), door(140, y=10, hanging=True)])
# buttons on the floor and on the ceiling
room([floor(0, 40), block(40, 104, 60, 16), spikes(40, 99, 12), floor(100, 60), button(20, FLIP),
      spikes(52, 10, 3, "d"), button(84, FLIP, y=10, ceiling=True), spikes(100, 10, 6, "d"), door(DOOR_X)])
# gravity flips as the runner passes under the spikes
room([floor(0, 160), spikes(60, 10, 8, "d"), switch(0, 0, 0, 0, FLIP, at=64), switch(0, 0, 0, 0, FLIP, at=106),
      door(DOOR_X)])
# a jump flips gravity
room([floor(0, 160), switch(0, 0, 0, 0, FLIP, when=UP, at=92), spikes(50, 95, 3), spikes(74, 10, 5, "d"),
      button(58, FLIP, y=10, ceiling=True), door(DOOR_X)])
# up and down over the spikes, the door on the ceiling
room([floor(0, 30), block(30, 104, 126, 16), spikes(30, 99, 25), button(16, FLIP), block(50, 10, 20, 16),
      spikes(70, 10, 6, "d"), button(56, FLIP, y=26, ceiling=True), block(100, 50, 24, 4), door(140, y=10, hanging=True)])

# ------------------------------------------------------------------ 14: ice
room([floor(0, 100), block(100, 112, 30, 8), spikes(100, 107, 6), floor(130, 30), door(DOOR_X)], modes=ICE)
room([floor(0, 40), block(40, 112, 116, 8), spikes(40, 107, 23), block(60, 100, 14, 4), block(94, 100, 14, 4),
      floor(130, 30), door(DOOR_X)], modes=ICE)
# the floor turns to ice before the spikes
room([floor(0, 160), switch(0, 0, 0, 0, ICE, at=60), spikes(100, 95, 3), door(DOOR_X)])
room([floor(0, 50), floor(50, 30, trap=falls(60)), floor(80, 80), spikes(110, 95, 2), door(DOOR_X)], modes=ICE)
room([floor(0, 160), saw(60, 86, 14, trap=trap(START, 0, LOOP, dx=50, speed=1)), spikes(130, 95, 1), door(DOOR_X)],
     modes=ICE)

# ------------------------------------------------------------------ 15: round the edges
room([floor(0, 160), block(120, 10, 8, 90), door(DOOR_X)], modes=WRAP)
room([floor(0, 50), floor(110, 50), block(90, 10, 8, 90), door(60, y=60), block(50, 80, 30, 4)], modes=WRAP)
room([floor(0, 160), popping(140, 4, 120), block(30, 10, 8, 90), door(10)], modes=WRAP, start=(60, FLOOR_Y))
room([floor(0, 160), saw(110, 84, 16, trap=trap(LEFT, 60, CHASE, dx=1, speed=0.9)), block(4, 10, 10, 90),
      door(20)], modes=WRAP, start=(60, FLOOR_Y))
# a wall in the way, and a pit at the other edge: jump through the edge
room([floor(0, 50), block(50, 10, 6, 90), floor(56, 84), block(140, 112, 20, 8), spikes(140, 107, 4), door(100)],
     modes=WRAP, start=(24, FLOOR_Y))

# ------------------------------------------------------------------ 16: the Devil
room([floor(0, 50), floor(50, 24, trap=falls(44)), floor(74, 86), popping(110, 3, 100),
      door(DOOR_X, trap=trap(NEAR, 18, ONCE, dx=-16, speed=2))])
room([floor(0, 40), block(40, 104, 116, 16), spikes(40, 99, 23), button(20, FLIP), spikes(80, 10, 4, "d"),
      door(140, y=10, hanging=True)], modes=BACK)
room([floor(0, 160), block(60, 10, 20, 30, trap=trap(START, 0, LOOP, dy=60, speed=2)),
      saw(100, 86, 14, trap=trap(START, 0, LOOP, dx=26, speed=1)), door(DOOR_X)], modes=ICE)
room([floor(0, 40), floor(40, 20, flags=GHOST), floor(60, 100), block(90, 10, 8, 90), warp((70, 88), (110, 88)),
      popping(112, 2, 100), door(DOOR_X)])
s = spikes(66, 100, 5, trap=trap(NEAR, 16, ONCE, dy=-5, speed=4))
room([floor(0, 160), door(70, fake=True), s, switch(0, 0, 0, 0, BACK, when=AFTER, at=s),
      door(DOOR_X, flags=HIDDEN, trap=trap(AFTER, s, ONCE))])


def pack_trap(t, index):
    at = t["at"]
    if isinstance(at, Obj):
        at = index[id(at)]
    return [t["when"] | t["gate"] << 3 | t["does"] << 6, at & 255, t["dx"] & 255, t["dy"] & 255,
            t["speed"] | t["delay"] << 4]


def pack(modes, start, objs):
    out = [modes, start[0], start[1], len(objs)]
    index = {id(o): i for i, o in enumerate(objs)}
    for o in objs:
        for v in (o.x, o.y, o.w, o.h):
            assert 0 <= v < 256, (o.kind, o.x, o.y, o.w, o.h)
        out += [o.kind | o.flags | (TRAP if o.trap else 0), o.x, o.y, o.w, o.h]
        if o.trap:
            out += pack_trap(o.trap, index)
    return out


def main():
    out = sys.argv[1]
    os.makedirs(out, exist_ok=True)
    data = []
    for modes, start, objs in ROOMS:
        assert len(objs) <= 28, "a room has 28 objects at most"
        data += pack(modes, start, objs)
    raw = bytes(data)
    c = zlib.compressobj(9, zlib.DEFLATED, -15, 9)
    z = c.compress(raw) + c.flush()
    try:  # smaller still with zopfli, as tools/shots.py does (the zlib stream without its header and check)
        import zopfli.zlib
        z2 = zopfli.zlib.compress(raw, numiterations=60)[2:-4]
        if len(z2) < len(z):
            z = z2
    except ImportError:
        pass
    assert zlib.decompress(z, -15) == raw
    with open(os.path.join(out, "rooms.inc"), "w") as f:
        f.write(f"/* Made by tools/rooms.py: {len(ROOMS)} rooms, {len(raw)} bytes packed into {len(z)} */\n")
        f.write(f"#define ROOM_BYTES {len(raw)}\n")
        f.write(f"#define ROOM_COUNT {len(ROOMS)}\n")
        f.write(f"static const uint8_t rooms_z[{len(z)}] = {{\n")
        for i in range(0, len(z), 24):
            f.write("  " + ",".join(str(b) for b in z[i:i + 24]) + ",\n")
        f.write("};\n")
    print(f"rooms: {len(ROOMS)}, {len(raw)} bytes, packed {len(z)}")


if __name__ == "__main__":
    main()
