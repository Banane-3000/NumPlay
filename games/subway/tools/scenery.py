#!/usr/bin/env python3
"""The pictures of Subway Surfers' world, drawn the way the game's classic yard looks (used by
tools/pack.py): the trains' sides and ends in their liveries (the orange and lime commuter train, the
silver one with its orange stripe, the red, white and blue one, the green box car), the orange viaduct
walls with their arches, the bushy trees over them, graffiti, the barriers' striped boards, the ramps'
chevrons and the tunnels' brick mouths.

Each is drawn at 4 times its size and made smaller, for soft edges. Run alone, it writes them all to
a folder to look at: scenery.py DIR"""
import math
import random
import sys
from pathlib import Path

from PIL import Image, ImageDraw, ImageFilter, ImageFont

UP = 4


def font(size):
    sys.path.insert(0, str(Path(__file__).resolve().parent))
    import assets
    return ImageFont.truetype(str(assets.font_file()), size)


def canvas(w, h, colour=(0, 0, 0, 0)):
    im = Image.new("RGBA", (w * UP, h * UP), colour)
    return im, ImageDraw.Draw(im)


def done(im, w, h):
    return im.resize((w, h), Image.LANCZOS)


def shade(c, k):
    return tuple(max(0, min(255, int(v * k))) for v in c[:3]) + ((c[3],) if len(c) > 3 else ())


def mix(a, b, t):
    return tuple(int(a[i] + (b[i] - a[i]) * t) for i in range(3))


def blots(im, spots):
    """Soft see-through spots (x, y, rx, ry, rgba), laid over the picture."""
    layer = Image.new("RGBA", im.size, (0, 0, 0, 0))
    d = ImageDraw.Draw(layer)
    for (x, y, rx, ry, c) in spots:
        d.ellipse([(x - rx) * UP, (y - ry) * UP, (x + rx) * UP, (y + ry) * UP], fill=c)
    im.alpha_composite(layer.filter(ImageFilter.GaussianBlur(UP * 0.6)))


def R(d, box, fill, k=UP, **kw):
    d.rectangle([v * k for v in box], fill=fill, **kw)


def RR(d, box, r, fill, k=UP, **kw):
    d.rounded_rectangle([v * k for v in box], r * k, fill=fill, **kw)


# ---------------------------------------------------------------- trains
# a car's side is 11 m long and 2.8 m tall (from 0.5 m to 3.3 m): 128 x 32 texels, u along the car
LIVERIES = {
    # the classic commuter train: orange red, a lime yellow top, teal windows
    "a": dict(body=(232, 78, 30), top=(206, 222, 44), skirt=(150, 40, 18), glass=(40, 112, 130), frame=(250, 236, 200),
              stripe=None),
    # the silver one, an orange stripe along the roof
    "b": dict(body=(214, 220, 226), top=(242, 140, 30), skirt=(120, 128, 136), glass=(36, 70, 96), frame=(70, 76, 84),
              stripe=(242, 140, 30)),
    # red, white and blue
    "c": dict(body=(240, 240, 236), top=(214, 44, 38), skirt=(36, 74, 160), glass=(34, 60, 110), frame=(214, 44, 38),
              stripe=(36, 74, 160)),
}


def train_side(look):
    L = LIVERIES[look]
    w, h = 128, 32
    im, d = canvas(w, h, L["body"] + (255,))
    # the top: the edge of the curved roof
    R(d, (0, 0, w, 5), L["top"] + (255,))
    R(d, (0, 5, w, 5.6), shade(L["top"], 0.7) + (255,))
    # a stripe under the windows, the skirt at the bottom
    if L["stripe"]:
        R(d, (0, 21, w, 23.5), L["stripe"] + (255,))
    R(d, (0, 27.5, w, h), L["skirt"] + (255,))
    R(d, (0, 27, w, 27.6), shade(L["body"], 0.6) + (255,))
    # windows between the doors, two doors
    doors = (24, 92)

    def window(x0, x1, y0=8, y1=17.5):
        RR(d, (x0 - 0.6, y0 - 0.6, x1 + 0.6, y1 + 0.6), 1.2, L["frame"] + (255,))
        RR(d, (x0, y0, x1, y1), 0.9, L["glass"] + (255,))
        # the sky in it
        d.polygon([(x0 * UP, y0 * UP), ((x0 + (x1 - x0) * 0.45) * UP, y0 * UP), (x0 * UP, (y0 + (y1 - y0) * 0.7) * UP)],
                  fill=shade(L["glass"], 1.45) + (255,))
    for x in (3, 12):
        window(x, x + 7.5)
    for x in (41, 50, 59, 68, 77):
        window(x, x + 7.5)
    for x in (109, 118):
        window(x, x + 7.5)
    for dx in doors:
        RR(d, (dx - 0.5, 6.5, dx + 12.5, 27.3), 0.8, shade(L["body"], 0.55) + (255,))
        R(d, (dx, 7, dx + 12, 27), shade(L["body"], 0.92) + (255,))
        R(d, (dx + 5.8, 7, dx + 6.2, 27), shade(L["body"], 0.5) + (255,))
        for x in (dx + 1.2, dx + 7):
            window(x, x + 3.8, 9, 18)
    # dirt and wear along the bottom
    rnd = random.Random(ord(look))
    blots(im, [(rnd.uniform(0, w), rnd.uniform(24, 27.5), rnd.uniform(1, 3), 0.8, (60, 40, 30, 40)) for _ in range(30)])
    return done(im, w, h)


def train_front(look, freight=False):
    """A car's end, 2.36 m wide and 3.0 m tall (0.5 to 3.5 m, the roof's curve in its top): 64 x 64."""
    w = h = 64
    if freight:
        return boxcar_end()
    L = LIVERIES[look]
    im, d = canvas(w, h, L["body"] + (255,))
    R(d, (0, 0, w, 11), L["top"] + (255,))
    R(d, (0, 11, w, 12), shade(L["top"], 0.7) + (255,))
    if L["stripe"]:
        R(d, (0, 42, w, 45), L["stripe"] + (255,))
    # two big windscreens, a pillar between
    for x0, x1 in ((4, 29), (35, 60)):
        RR(d, (x0 - 1, 14, x1 + 1, 33), 2.5, (30, 30, 34, 255))
        RR(d, (x0, 15, x1, 32), 2, L["glass"] + (255,))
        d.polygon([(x0 * UP, 15 * UP), ((x0 + 10) * UP, 15 * UP), (x0 * UP, 27 * UP)], fill=shade(L["glass"], 1.5) + (255,))
    if look == "a":
        # the classic one: a door in the middle
        R(d, (25, 13, 39, 56), shade(L["body"], 0.6) + (255,))
        R(d, (26, 14, 38, 56), shade(L["body"], 0.95) + (255,))
        RR(d, (28, 16, 36, 30), 1, L["glass"] + (255,))
    if look == "c":
        d.ellipse([27 * UP, 36 * UP, 37 * UP, 46 * UP], outline=(36, 74, 160, 255), width=int(1.6 * UP))
    # headlights, round, near the sides; a number
    for cx in (9, 55):
        d.ellipse([(cx - 4.2) * UP, (45 - 4.2) * UP, (cx + 4.2) * UP, (45 + 4.2) * UP], fill=(40, 40, 44, 255))
        d.ellipse([(cx - 3.2) * UP, (45 - 3.2) * UP, (cx + 3.2) * UP, (45 + 3.2) * UP], fill=(255, 250, 214, 255))
        d.ellipse([(cx - 1.8) * UP, (45 - 2.2) * UP, (cx + 0.8) * UP, (45 + 0.2) * UP], fill=(255, 255, 255, 255))
    # the bumper with its black and yellow stripes
    R(d, (0, 54, w, h), (44, 44, 48, 255))
    for i in range(-2, 12):
        x = i * 6
        d.polygon([(x * UP, 58 * UP), ((x + 3) * UP, 58 * UP), ((x + 7) * UP, 63 * UP), ((x + 4) * UP, 63 * UP)],
                  fill=(250, 200, 30, 255))
    R(d, (0, 56, w, 58), (60, 60, 64, 255))
    return done(im, w, h)


def boxcar_side():
    """The green box car: steel ribs, a sliding door, rust."""
    w, h = 128, 32
    green = (40, 128, 92)
    im, d = canvas(w, h, green + (255,))
    R(d, (0, 0, w, 2), shade(green, 1.25) + (255,))
    for i in range(16):
        x = 2 + i * 8
        R(d, (x, 2, x + 1.3, 29), shade(green, 0.72) + (255,))
        R(d, (x + 1.3, 2, x + 1.9, 29), shade(green, 1.18) + (255,))
    R(d, (52, 3, 76, 29), shade(green, 0.9) + (255,))
    R(d, (52, 3, 53, 29), shade(green, 0.6) + (255,))
    R(d, (75, 3, 76, 29), shade(green, 0.6) + (255,))
    R(d, (0, 29, w, h), (60, 46, 36, 255))
    R(d, (8, 12, 30, 15), (240, 240, 230, 160))
    rnd = random.Random(7)
    blots(im, [(rnd.uniform(0, w), rnd.uniform(18, 29), rnd.uniform(1, 3), rnd.uniform(0.6, 2), (130, 70, 30, 60)) for _ in range(30)])
    return done(im, w, h)


def wagon_side():
    """The blue open wagon of the classic yard: steel ribs and rivets, a lighter rail along the top."""
    w, h = 128, 32
    blue = (54, 118, 196)
    im, d = canvas(w, h, blue + (255,))
    R(d, (0, 0, w, 2.5), shade(blue, 1.3) + (255,))
    R(d, (0, 2.5, w, 3.2), shade(blue, 0.7) + (255,))
    for i in range(11):
        x = 3 + i * 12
        R(d, (x, 3, x + 2, 29), shade(blue, 0.75) + (255,))
        R(d, (x + 2, 3, x + 2.8, 29), shade(blue, 1.2) + (255,))
    for i in range(32):
        x = 1.5 + i * 4
        for y in (5, 26):
            d.ellipse([(x - 0.5) * UP, (y - 0.5) * UP, (x + 0.5) * UP, (y + 0.5) * UP], fill=shade(blue, 1.35) + (255,))
    R(d, (0, 29, w, h), (44, 44, 50, 255))
    rnd = random.Random(9)
    blots(im, [(rnd.uniform(0, w), rnd.uniform(8, 28), rnd.uniform(1, 4), rnd.uniform(1, 3), (120, 70, 40, 50)) for _ in range(25)])
    return done(im, w, h)


def wagon_end():
    w = h = 64
    blue = (54, 118, 196)
    im, d = canvas(w, h, blue + (255,))
    R(d, (0, 0, w, 4), shade(blue, 1.3) + (255,))
    for i in range(4):
        y = 10 + i * 11
        R(d, (2, y, w - 2, y + 2), shade(blue, 0.72) + (255,))
        R(d, (2, y + 2, w - 2, y + 3), shade(blue, 1.2) + (255,))
    R(d, (0, 0, 3, h), shade(blue, 0.7) + (255,))
    R(d, (61, 0, 64, h), shade(blue, 0.7) + (255,))
    R(d, (0, 56, w, h), (44, 44, 48, 255))
    for i in range(-2, 12):
        x = i * 6
        d.polygon([(x * UP, 58 * UP), ((x + 3) * UP, 58 * UP), ((x + 7) * UP, 63 * UP), ((x + 4) * UP, 63 * UP)],
                  fill=(250, 200, 30, 255))
    return done(im, w, h)


def cargo():
    """What the open wagon carries: a heap of brown earth and stones, 32 x 64."""
    w, h = 32, 64
    brown = (156, 112, 70)
    im, d = canvas(w, h, brown + (255,))
    rnd = random.Random(13)
    spots = []
    for _ in range(70):
        x, y, r = rnd.uniform(0, w), rnd.uniform(0, h), rnd.uniform(0.8, 2.5)
        c = (196, 150, 100, 150) if rnd.random() < 0.5 else (100, 70, 44, 150)
        spots.append((x, y, r, r, c))
    blots(im, spots)
    return done(im, w, h)


def boxcar_end():
    w = h = 64
    green = (40, 128, 92)
    im, d = canvas(w, h, green + (255,))
    for i in range(5):
        y = 6 + i * 11
        R(d, (2, y, w - 2, y + 1.5), shade(green, 0.72) + (255,))
        R(d, (2, y + 1.5, w - 2, y + 2.3), shade(green, 1.18) + (255,))
    R(d, (0, 0, 3, h), shade(green, 0.7) + (255,))
    R(d, (61, 0, 64, h), shade(green, 0.7) + (255,))
    R(d, (0, 56, w, h), (44, 44, 48, 255))
    for i in range(-2, 12):
        x = i * 6
        d.polygon([(x * UP, 58 * UP), ((x + 3) * UP, 58 * UP), ((x + 7) * UP, 63 * UP), ((x + 4) * UP, 63 * UP)],
                  fill=(250, 200, 30, 255))
    return done(im, w, h)


def roof(look):
    """A car's roof, along the car: vents and the stripe, 32 x 64 (u across, v along)."""
    w, h = 32, 64
    top = LIVERIES[look]["top"] if look in LIVERIES else (40, 128, 92)
    if look == "b":
        top = (200, 206, 212)
    im, d = canvas(w, h, top + (255,))
    if look == "b":
        R(d, (11, 0, 21, h), (242, 140, 30, 255))
    for y in (10, 30, 50):
        RR(d, (9, y, 23, y + 6), 1, shade(top, 0.62) + (255,))
        for k in range(4):
            R(d, (10, y + 1 + k * 1.3, 22, y + 1.6 + k * 1.3), shade(top, 0.45) + (255,))
    R(d, (0, 0, 1.2, h), shade(top, 0.75) + (255,))
    R(d, (30.8, 0, 32, h), shade(top, 0.75) + (255,))
    return done(im, w, h)


# ---------------------------------------------------------------- walls, trees, graffiti
def arch_wall():
    """A 24 m stretch of the orange viaduct (6 m tall): three arches, bricks, the coping: 128 x 64."""
    w, h = 128, 64
    orange = (248, 100, 28)
    im, d = canvas(w, h, orange + (255,))
    # light from the top down, a little
    for y in range(h):
        R(d, (0, y, w, y + 1), shade(orange, 1.06 - 0.16 * y / h) + (255,))
    # bricks, faint
    for row in range(0, 64, 4):
        R(d, (0, row, w, row + 0.3), shade(orange, 0.9) + (255,))
    # the arches: dark inside, a lit edge on one side
    for i in range(3):
        cx = 21.3 + i * 42.7
        rw, top = 15.5, 21
        inner = (128, 34, 14)
        d.rectangle([(cx - rw) * UP, top * UP, (cx + rw) * UP, h * UP], fill=(214, 76, 24, 255))
        d.ellipse([(cx - rw) * UP, (top - rw) * UP, (cx + rw) * UP, (top + rw) * UP], fill=(214, 76, 24, 255))
        rw2 = rw - 2.2
        d.rectangle([(cx - rw2) * UP, top * UP, (cx + rw2) * UP, h * UP], fill=inner + (255,))
        d.ellipse([(cx - rw2) * UP, (top - rw2) * UP, (cx + rw2) * UP, (top + rw2) * UP], fill=inner + (255,))
        # deeper in the dark, and a little light at the back
        d.ellipse([(cx - rw2 + 4) * UP, (top - rw2 + 6) * UP, (cx + rw2 - 1) * UP, (h + 12) * UP], fill=(80, 20, 10, 255))
        d.polygon([((cx - rw2) * UP, h * UP), ((cx - rw2) * UP, (top + 4) * UP), ((cx - rw2 + 3) * UP, (top + 6) * UP),
                   ((cx - rw2 + 3) * UP, h * UP)], fill=(168, 52, 20, 255))
        # the keystone
        R(d, (cx - 2, top - rw - 1.5, cx + 2, top - rw + 2.5), shade(orange, 1.15) + (255,))
    # the coping on top, and the pillars' edges
    R(d, (0, 0, w, 5), (250, 130, 60, 255))
    R(d, (0, 5, w, 6.2), (176, 58, 20, 255))
    return done(im, w, h)


def concrete_wall():
    """A grey wall of concrete panels, stains and dirt: 64 x 32 (12 m by 4 m)."""
    w, h = 64, 32
    grey = (178, 172, 160)
    im, d = canvas(w, h, grey + (255,))
    rnd = random.Random(3)
    for i in range(4):
        x = i * 16
        R(d, (x, 0, x + 0.6, h), shade(grey, 0.7) + (255,))
        R(d, (x + 0.6, 0, x + 1, h), shade(grey, 1.1) + (255,))
    R(d, (0, 0, w, 2), shade(grey, 1.12) + (255,))
    R(d, (0, 2, w, 2.6), shade(grey, 0.7) + (255,))
    blots(im, [(rnd.uniform(0, w), rnd.uniform(3, h), r, r * 2.5, (90, 84, 74, 36)) for r in [rnd.uniform(0.5, 2.5) for _ in range(50)]])
    R(d, (0, 28, w, h), (140, 128, 112, 255))
    return done(im, w, h)


def tree():
    """A bushy tree over the walls: round clumps of leaves, lit from the top left: 64 x 64, see-through."""
    w = h = 64
    im, d = canvas(w, h)
    rnd = random.Random(11)
    dark, mid, light, hi = (40, 110, 30), (78, 160, 40), (136, 200, 52), (206, 230, 90)
    # the trunk
    R(d, (29, 46, 35, 64), (96, 62, 34, 255))
    clumps = [(32, 30, 20), (18, 38, 13), (46, 37, 14), (24, 20, 13), (41, 19, 13), (32, 12, 11), (12, 28, 9), (53, 27, 9)]
    for (x, y, r) in clumps:
        d.ellipse([(x - r) * UP, (y - r) * UP, (x + r) * UP, (y + r) * UP], fill=dark + (255,))
    for (x, y, r) in clumps:
        d.ellipse([(x - r * 0.92) * UP, (y - r * 0.95) * UP, (x + r * 0.85) * UP, (y + r * 0.78) * UP], fill=mid + (255,))
    for (x, y, r) in clumps:
        d.ellipse([(x - r * 0.8) * UP, (y - r * 0.9) * UP, (x + r * 0.45) * UP, (y + r * 0.25) * UP], fill=light + (255,))
        d.ellipse([(x - r * 0.62) * UP, (y - r * 0.78) * UP, (x - r * 0.05) * UP, (y - r * 0.25) * UP], fill=hi + (255,))
    for _ in range(14):
        x, y = rnd.uniform(10, 54), rnd.uniform(8, 44)
        d.ellipse([(x - 1.6) * UP, (y - 1.2) * UP, (x + 1.6) * UP, (y + 1.2) * UP], fill=dark + (200,))
    return done(im, w, h)


SPRAY = [(255, 79, 163), (255, 210, 63), (63, 208, 255), (107, 224, 74), (255, 138, 43), (155, 92, 255)]


def graffiti(word, seed):
    """A tag in bubble letters with an outline and a shadow, on a splash of colour: 64 x 32, see-through."""
    w, h = 64, 32
    rnd = random.Random(seed)
    im, d = canvas(w, h)
    c1, c2 = rnd.sample(SPRAY, 2)
    # the splash behind
    for _ in range(9):
        x, y, r = rnd.uniform(10, 54), rnd.uniform(10, 22), rnd.uniform(6, 11)
        d.ellipse([(x - r) * UP, (y - r * 0.7) * UP, (x + r) * UP, (y + r * 0.7) * UP], fill=c2 + (255,))
    f = font(int(19 * UP))
    bb = d.textbbox((0, 0), word, font=f, stroke_width=int(1.8 * UP))
    tw, th = bb[2] - bb[0], bb[3] - bb[1]
    s = min(1.0, (w * UP * 0.92) / tw)
    layer = Image.new("RGBA", (tw + 8 * UP, th + 8 * UP), (0, 0, 0, 0))
    ld = ImageDraw.Draw(layer)
    ld.text((4 * UP - bb[0] + int(1.4 * UP), 4 * UP - bb[1] + int(1.4 * UP)), word, font=f, fill=(20, 20, 30, 255),
            stroke_width=int(1.8 * UP), stroke_fill=(20, 20, 30, 255))
    ld.text((4 * UP - bb[0], 4 * UP - bb[1]), word, font=f, fill=c1 + (255,), stroke_width=int(1.8 * UP),
            stroke_fill=(255, 255, 255, 255))
    layer = layer.transform(layer.size, Image.AFFINE, (1, 0.25, -0.12 * layer.height, 0, 1, 0), Image.BICUBIC)
    layer = layer.resize((int(layer.width * s), int(layer.height * s)), Image.LANCZOS)
    im.alpha_composite(layer, ((w * UP - layer.width) // 2, (h * UP - layer.height) // 2))
    return done(im, w, h)


# ---------------------------------------------------------------- barriers, ramps, tunnels
def barrier_board():
    """The low barrier's board: red and white stripes slanting, a dark frame: 64 x 16."""
    w, h = 64, 16
    im, d = canvas(w, h, (246, 246, 240, 255))
    for i in range(-2, 12):
        x = i * 8
        d.polygon([(x * UP, 0), ((x + 4) * UP, 0), ((x + 4 + 6) * UP, h * UP), ((x + 6) * UP, h * UP)], fill=(222, 40, 36, 255))
    R(d, (0, 0, w, 1.3), (60, 60, 66, 255))
    R(d, (0, 14.7, w, h), (60, 60, 66, 255))
    return done(im, w, h)


def chevrons():
    """Red and white chevrons pointing down (the high barrier and the blockers): 32 x 32."""
    w = h = 32
    im, d = canvas(w, h, (246, 246, 240, 255))
    for k in range(-1, 5):
        y = k * 9
        d.polygon([(0, y * UP), (16 * UP, (y + 9) * UP), (32 * UP, y * UP), (32 * UP, (y + 5) * UP), (16 * UP, (y + 14) * UP),
                   (0, (y + 5) * UP)], fill=(222, 40, 36, 255))
    return done(im, w, h)


def ramp():
    """A ramp's slope: yellow with black chevrons pointing up it, worn: 32 x 64 (v from the top down)."""
    w, h = 32, 64
    yellow = (250, 196, 30)
    im, d = canvas(w, h, (70, 70, 76, 255))
    R(d, (0, 0, 4, h), yellow + (255,))
    R(d, (28, 0, 32, h), yellow + (255,))
    for k in range(5):
        y = 4 + k * 13
        d.polygon([(7 * UP, (y + 8) * UP), (16 * UP, y * UP), (25 * UP, (y + 8) * UP), (25 * UP, (y + 12) * UP), (16 * UP, (y + 4) * UP),
                   (7 * UP, (y + 12) * UP)], fill=yellow + (255,))
    rnd = random.Random(5)
    blots(im, [(rnd.uniform(0, w), rnd.uniform(0, h), 1.2, 1.2, (30, 30, 30, 70)) for _ in range(30)])
    return done(im, w, h)


def tunnel_mouth():
    """The tunnel's way in: dark bricks round an arched opening, the subway's sign: 128 x 64 for a
    30 m by 14 m face (the opening, 11.6 m by 8.5 m, cut out: see-through)."""
    w, h = 128, 64
    brick = (150, 74, 50)
    im, d = canvas(w, h, brick + (255,))
    for row in range(0, h, 2):
        R(d, (0, row, w, row + 0.22), shade(brick, 0.75) + (255,))
        off = 0 if (row // 2) % 2 else 2
        for x in range(off, w, 4):
            R(d, (x, row, x + 0.22, row + 2), shade(brick, 0.75) + (255,))
    # the opening (x 40.5..87.5 texels, from the bottom up to 8.5 m, rounded on top)
    cx, hw = 64, 23.5
    top = h - 8.5 / 14 * h
    stone = (196, 186, 170)
    d.rectangle([(cx - hw - 3) * UP, (top + 6) * UP, (cx + hw + 3) * UP, h * UP], fill=stone + (255,))
    d.ellipse([(cx - hw - 3) * UP, (top - 3) * UP, (cx + hw + 3) * UP, (top + 2 * hw + 3) * UP], fill=stone + (255,))
    d.rectangle([(cx - hw) * UP, (top + 6) * UP, (cx + hw) * UP, h * UP], fill=(0, 0, 0, 0))
    d.ellipse([(cx - hw) * UP, top * UP, (cx + hw) * UP, (top + 2 * hw) * UP], fill=(0, 0, 0, 0))
    # the sign
    RR(d, (cx - 14, 2, cx + 14, 10), 1.5, (30, 70, 150, 255), outline=(240, 240, 240, 255), width=int(0.8 * UP))
    f = font(int(6.5 * UP))
    d.text((cx * UP, 6.2 * UP), "SUBWAY", font=f, fill=(250, 250, 250, 255), anchor="mm")
    R(d, (0, 0, w, 1.5), shade(brick, 0.6) + (255,))
    return done(im, w, h)


ALL = {
    "train_a_side": lambda: train_side("a"), "train_b_side": lambda: train_side("b"), "train_c_side": lambda: train_side("c"),
    "train_a_front": lambda: train_front("a"), "train_b_front": lambda: train_front("b"), "train_c_front": lambda: train_front("c"),
    "boxcar_side": boxcar_side, "boxcar_end": boxcar_end, "wagon_side": wagon_side, "wagon_end": wagon_end, "cargo": cargo,
    "roof_a": lambda: roof("a"), "roof_b": lambda: roof("b"), "roof_c": lambda: roof("c"), "roof_box": lambda: roof("box"),
    "arch_wall": arch_wall, "concrete": concrete_wall, "tree": tree,
    "graffiti_0": lambda: graffiti("SURF", 1), "graffiti_1": lambda: graffiti("JAKE", 2), "graffiti_2": lambda: graffiti("YO!", 3),
    "graffiti_3": lambda: graffiti("RUN", 4),
    "barrier": barrier_board, "chevrons": chevrons, "ramp": ramp, "tunnel": tunnel_mouth,
}

if __name__ == "__main__":
    out = Path(sys.argv[1])
    out.mkdir(parents=True, exist_ok=True)
    for name, f in ALL.items():
        im = f()
        im.save(out / f"{name}.png")
        im.resize((im.width * 4, im.height * 4), Image.NEAREST).save(out / f"{name}_x4.png")
