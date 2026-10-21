#!/usr/bin/env python3
"""The app's icon (src/icon.png, 55 x 56): the marine's face from the status bar (STFST01), twice its
size, on the dark rounded square of NumPlay's big games.

Usage: icon.py DOOM1.WAD src/icon.png"""
import struct
import sys

from PIL import Image, ImageDraw


def lumps(wad):
    data = open(wad, "rb").read()
    n, at = struct.unpack_from("<II", data, 4)
    out = {}
    for i in range(n):
        pos, size, name = struct.unpack_from("<II8s", data, at + 16 * i)
        out[name.rstrip(b"\0").decode()] = data[pos:pos + size]
    return out


def patch(lump, palette):
    w, h, _, _ = struct.unpack_from("<hhhh", lump, 0)
    im = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    for x in range(w):
        p = struct.unpack_from("<I", lump, 8 + 4 * x)[0]
        while lump[p] != 0xFF:
            top, length = lump[p], lump[p + 1]
            for j in range(length):
                c = lump[p + 3 + j]
                im.putpixel((x, top + j), tuple(palette[3 * c:3 * c + 3]) + (255,))
            p += length + 4
    return im


def main():
    wad, out = sys.argv[1], sys.argv[2]
    ls = lumps(wad)
    pal = ls["PLAYPAL"][:768]
    face = patch(ls["STFST01"], pal)
    face = face.resize((face.width * 2, face.height * 2), Image.NEAREST)
    icon = Image.new("RGBA", (55, 56), (0, 0, 0, 0))
    ImageDraw.Draw(icon).rounded_rectangle((0, 0, 54, 55), radius=11, fill=(12, 12, 12, 255))
    # the face fills the square, its chin and hair just cut
    icon.alpha_composite(face.crop((0, 1, face.width, 57)), ((55 - face.width) // 2, 0))
    mask = Image.new("L", (55, 56), 0)
    ImageDraw.Draw(mask).rounded_rectangle((0, 0, 54, 55), radius=11, fill=255)
    icon.putalpha(Image.composite(icon.getchannel("A"), Image.new("L", (55, 56), 0), mask))
    icon.save(out)


if __name__ == "__main__":
    main()
