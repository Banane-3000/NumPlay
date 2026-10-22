#!/usr/bin/env python3
"""The NES palettes of Retro-Go on NumPlay (src/nes/nes.c), as C tables of RGB565:
- Nofrendo: nofrendo's own palette, from its generator (nes_pal.c: hue 334, tint 0.4);
- NTSC: the 2C02's composite signal decoded: the luma levels of the NESdev wiki, the chroma a
  30-degree step for each of the 12 hues (hue 1 blue, 6 red, 10 green).

usage: nespal.py [picture.png]   (prints the tables; the picture shows both, 16 colours a row)"""
import math
import sys


def nofrendo():
    brightness = [[0.50, 0.75, 1.00, 1.00], [0.29, 0.45, 0.73, 0.90], [0.00, 0.24, 0.47, 0.77],
                  [0.02, 0.04, 0.05, 0.07]]
    angles = [0, 240, 210, 180, 150, 120, 90, 60, 30, 0, 330, 300, 270, 0, 0, 0]
    hue, tint = 334.0, 0.4
    out = [None] * 64
    for x in range(4):
        for z in range(16):
            if z == 0:
                s, y = 0, brightness[0][x]
            elif z == 13:
                s, y = 0, brightness[2][x]
            elif z in (14, 15):
                s, y = 0, brightness[3][x]
            else:
                s, y = tint, brightness[1][x]
            t = math.pi * ((angles[z] + hue) / 180.0)
            r = int(256.0 * (y + s * math.sin(t)))
            g = int(256.0 * (y - ((27 / 53.0) * s * math.sin(t)) + ((10 / 53.0) * s * math.cos(t))))
            b = int(256.0 * (y - (s * math.cos(t))))
            out[(x << 4) + z] = tuple(max(0, min(255, v)) for v in (r, g, b))
    return out


def ntsc():
    lo = [0.228, 0.312, 0.552, 0.880]  # each luma level's low and high voltage
    hi = [0.616, 0.840, 1.100, 1.100]
    black, white = lo[1], hi[3]
    out = []
    for c in range(64):
        level, h = c >> 4, c & 15
        if h >= 14:
            out.append((0, 0, 0))
            continue
        # luma: the square wave's mean; chroma: its fundamental
        if h == 0:
            y, a = (hi[level] - black) / (white - black), 0.0
        elif h == 13:
            y, a = (lo[level] - black) / (white - black), 0.0
        else:
            y = ((hi[level] + lo[level]) / 2 - black) / (white - black)
            a = (hi[level] - lo[level]) / (white - black) * 2 / math.pi * 0.72
        ang = math.radians(181 - 30 * h)
        i, q = a * math.cos(ang), a * math.sin(ang)
        rgb = (y + 0.956 * i + 0.621 * q, y - 0.272 * i - 0.647 * q, y - 1.106 * i + 1.703 * q)
        out.append(tuple(int(round(255 * max(0.0, min(1.0, v)))) for v in rgb))
    return out


def c_table(pal):
    vals = ["0x%04X" % (((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3)) for r, g, b in pal]
    return "   {\n   " + ",\n   ".join(", ".join(vals[k:k + 8]) for k in range(0, 64, 8)) + "\n   },"


if __name__ == "__main__":
    print(c_table(nofrendo()))
    print(c_table(ntsc()))
    if len(sys.argv) > 1:
        from PIL import Image
        im = Image.new("RGB", (16 * 16, 8 * 16))
        for k, pal in enumerate((nofrendo(), ntsc())):
            for c, rgb in enumerate(pal):
                for yy in range(16):
                    for xx in range(16):
                        im.putpixel(((c & 15) * 16 + xx, (k * 4 + (c >> 4)) * 16 + yy), rgb)
        im.save(sys.argv[1])
