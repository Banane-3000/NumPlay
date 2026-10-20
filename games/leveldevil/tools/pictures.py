#!/usr/bin/env python3
"""Level Devil's own pictures and fonts, packed into src/pictures.h.

They come from the game's web build (the one tools/fetch.py reads): its packed library (default.pak)
has every picture at 1x, 1.5x, 2x and 4x; these are the 1x ones (assets/graphics/1x), where a sprite
is "sprite-ID-ANIMATION.png" (the frames of an animation side by side, then down) and a font is
"font-ID.png" with its "font-ID.fnt" (AngelCode BMFont text). The sprites' names and frames are the
game's (data/resources.mbs): 591 Hero, 599 Door, 610 Spikes, 681 Portals, 711 and 713
Scene_Change_Top and _Bottom (the jaws with the Devil's eyes), 743 the title...

A picture is up to 15 colours and clear: its colours are in pic_pal, from its pal on; a pixel is
4 bits (0 clear, n its n-th colour), rows of whole bytes. The game paints its traps grey
(99, 99, 99) and swaps that grey for the world's ground colour as a room starts; such a picture
says which of its colours is that grey (tint), and the game draws it in the room's colour.
Big pictures (the title, the jaws, the credits) are runs instead: a byte a run, its colour in the
high 4 bits and its length less one in the low 4 (15: 16 more than the next byte), each row
starting where the row table (16 bits a row, from the picture's start) says.

A font: for each letter from ' ' to '~', its advance, its bitmap's size and place (from the top of
the line), and its bits (row by row, the first pixel the lowest bit, whole bytes a letter).

Usage: pictures.py [--from DIR]   (DIR: a folder with "Level Devil.js" and default.pak; without it they
                                   are downloaded, as tools/fetch.py does)
"""
import argparse
import io
import os
import re
import sys
import tempfile
import urllib.parse
import zlib

from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import fetch  # noqa: E402

OUT = os.path.join(HERE, "..", "src", "pictures.h")
GREY = (99, 99, 99)

# name: file (sprite-ID-ANIM), the frame size (or None: the whole picture), its frames (or one), runs?
PICTURES = [
    ("HERO_IDLE", "sprite-591-5", None, None),
    ("HERO_JUMP_L", "sprite-591-6", None, None),
    ("HERO_JUMP_R", "sprite-591-7", None, None),
    ("HERO_RUN_L", "sprite-591-8", (16, 16), 4),
    ("HERO_RUN_R", "sprite-591-9", (16, 16), 4),
    ("FLAP_STAND", "sprite-693-0", None, None),
    ("FLAP_R", "sprite-693-2", None, None),
    ("FLAP_L", "sprite-693-3", None, None),
    ("DOOR", "sprite-599-20", None, None),
    ("DOOR_SHUT", "sprite-599-21", (16, 16), 5),     # Level_Complete: the door shutting
    ("DOOR_HERO", "sprite-599-15", None, None),      # Hero_Dummy: the door dressed as the runner
    ("DOOR_DUMMY", "sprite-683-0", None, None),
    ("SPIKE", "sprite-610-17", None, None),
    ("COIN", "sprite-614-0", None, None),
    ("BAD_COIN", "sprite-660-0", None, None),
    ("WARP", "sprite-620-0", (16, 16), 2),
    ("SPRING", "sprite-622-0", None, None),
    ("SPRING_UP", "sprite-622-34", None, None),
    ("BUTTON", "sprite-662-0", None, None),
    ("BUTTON_DOWN", "sprite-662-1", None, None),
    ("SAW", "sprite-685-0", None, None),
    ("SAW_BIG", "sprite-685-1", None, None),
    ("GSPIKE", "sprite-699-14", None, None),         # Grav_Spike_Block
    ("THRU", "sprite-563-0", None, None),            # Tile_Jumpthrough_2
    ("DEATH_SQUARE", "sprite-847-17", None, None),
    ("BODY", "sprite-593-%d", None, (1, 2, 3, 4)),   # Body_Parts: the runner in pieces
    ("PRE", "sprite-715-%d", None, (0, 1, 2, 3)),    # Body_Precursors: the pieces coming together
    ("KEYS", "sprite-767-0", None, None),            # Keyboard_Tutorial
    ("ROOM_SQUARE", "sprite-849-0", None, None),     # Level_Indicator
    ("ROOM_DONE", "sprite-849-1", None, None),
    ("BACK", "sprite-760-0", (16, 16), 1),           # Btn_Back
    ("PORTAL_LOCKED", "sprite-681-17", None, None),
    ("PORTAL", "sprite-681-%d", None, tuple(range(18, 33))),
    ("BOSS_LOCKED", "sprite-723-0", None, None),
    ("BOSS_OPEN", "sprite-723-1", None, None),
    ("DIRT", "sprite-725-0", None, None),            # Portal_Dirt, under each portal
    ("HILITE", "sprite-745-0", (12, 30), 4),         # Portal_Highlight
    ("HILITE_BOSS", "sprite-745-1", (22, 37), 4),
    ("CARD", "sprite-855-0", None, "runs"),          # Btn_1_Player
    ("CARD_ON", "sprite-855-1", None, "runs"),
    ("EYES", "sprite-833-0", None, "runs"),          # Boss_Eyes
    ("EYES_LAUGH", "sprite-833-1", None, "runs"),
    ("TITLE", "background-743-0", None, "runs"),     # Title_Background
    ("JAW_TOP", "sprite-711-2", None, "runs"),       # Scene_Change_Top (Slim)
    ("JAW_BOTTOM", "sprite-713-2", None, "runs"),    # Scene_Change_Bottom (Slim)
    ("CREDITS", "sprite-719-1", None, "runs"),
]
FONTS = [("FONT_SMALL", 543), ("FONT", 783)]  # "8px" and "13px Yellow" (drawn in any colour)


def library(script, pak):
    """the 1x pictures and fonts of the packed library: {name: bytes}"""
    js = script.decode("utf-8", "replace")
    end = js.find('libraryType":"lime.utils.PackedAssetLibrary')
    manifest = js[js.rfind('{"name":null,"assets":"', 0, end):end]
    out = {}
    for m in re.finditer(r"(?:position|R0)i(\d+)(?:y4:size|R1)(?:z|i\d+)(?:y4:type|R2)(?:y\d+:[A-Z]+|R\d+)(?:y2:id|R4)y(\d+):",
                         manifest):
        pos, n = int(m.group(1)), int(m.group(2))
        rest = manifest[m.end():]
        ident = urllib.parse.unquote(rest[:n])
        length = re.match(r"(?:y6:length|R6)i(\d+)", rest[n:])
        if length and ident.startswith("assets/graphics/1x/"):
            raw = pak[pos:pos + int(length.group(1))]
            try:
                raw = zlib.decompress(raw, 31)
            except zlib.error:
                pass
            out[ident.rsplit("/", 1)[1]] = raw
    return out


def rgb565(c):
    r, g, b = c
    return (r & 0xF8) << 8 | (g & 0xFC) << 3 | b >> 3


class Packer:
    def __init__(self):
        self.pal, self.data, self.pics, self.names = [], bytearray(), [], []

    def palette(self, colours):
        """where these colours are in pic_pal (shared when the same)"""
        v = [rgb565(c) for c in colours]
        for i in range(len(self.pal) - len(v) + 1):
            if self.pal[i:i + len(v)] == v:
                return i
        self.pal += v
        return len(self.pal) - len(v)

    def add(self, name, im, runs=False):
        im = im.convert("RGBA")
        w, h = im.size
        px = im.load()
        colours = []
        for y in range(h):
            for x in range(w):
                r, g, b, a = px[x, y]
                if a >= 128 and (r, g, b) not in colours:
                    colours.append((r, g, b))
        assert len(colours) <= 15, (name, colours)
        colours.sort(key=lambda c: c != GREY)
        idx = [[colours.index(px[x, y][:3]) + 1 if px[x, y][3] >= 128 else 0 for x in range(w)] for y in range(h)]
        at = len(self.data)
        if runs:
            body = bytearray()
            table = []
            for row in idx:
                table.append(2 * h + len(body))
                x = 0
                while x < w:
                    n = 1
                    while x + n < w and row[x + n] == row[x] and n < 16 + 255:
                        n += 1
                    if n <= 15:
                        body.append(row[x] << 4 | (n - 1))
                    else:
                        body += bytes([row[x] << 4 | 15, n - 16])
                    x += n
            assert 2 * h + len(body) < 65536
            self.data += b"".join(t.to_bytes(2, "little") for t in table) + body
        else:
            for row in idx:
                row = row + [0] * (w & 1)
                self.data += bytes(row[i] | row[i + 1] << 4 for i in range(0, len(row), 2))
        tint = colours.index(GREY) + 1 if GREY in colours else 0
        self.pics.append((w, h, self.palette(colours), tint, int(runs), at))
        self.names.append(name)


def font(png, fnt):
    """(line height, base, glyphs: (adv, w, h, x, y, bits) for ' ' to '~')"""
    im = Image.open(io.BytesIO(png)).convert("RGBA")
    text = fnt.decode("utf-8", "replace")
    common = re.search(r'lineHeight="(\d+)" base="(\d+)"', text)
    chars = {}
    for m in re.finditer(r"<char ([^>]*)>", text):
        a = dict(re.findall(r'(\w+)="(-?\d+)"', m.group(1)))
        chars[int(a["id"])] = {k: int(v) for k, v in a.items()}
    glyphs = []
    for cp in range(32, 127):
        c = chars.get(cp, chars[32])
        bits = bytearray()
        for y in range(c["height"]):
            for x0 in range(0, c["width"], 8):
                v = 0
                for i in range(min(8, c["width"] - x0)):
                    if im.getpixel((c["x"] + x0 + i, c["y"] + y))[3] >= 128:
                        v |= 1 << i
                bits.append(v)
        glyphs.append((c["xadvance"], c["width"], c["height"], c["xoffset"], c["yoffset"], bytes(bits)))
    return int(common.group(1)), int(common.group(2)), glyphs


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--from", dest="src")
    a = ap.parse_args()
    folder = a.src or tempfile.mkdtemp()
    script = fetch.fetch("Level Devil.js", folder)
    pak = (open(os.path.join(folder, "default.pak"), "rb").read() if a.src
           else fetch.fetch("lib/default.pak", folder))
    files = library(script, pak)
    p = Packer()
    for name, src, size, frames in PICTURES:
        if frames == "runs":
            p.add(name, Image.open(io.BytesIO(files[src + ".png"])), runs=True)
        elif isinstance(frames, tuple):
            for i, f in enumerate(frames):
                p.add(f"{name}{i}", Image.open(io.BytesIO(files[(src % f) + ".png"])))
        elif size:
            im = Image.open(io.BytesIO(files[src + ".png"]))
            across = im.width // size[0]
            for k in range(frames):
                x, y = k % across * size[0], k // across * size[1]
                p.add(f"{name}{k}" if frames > 1 else name, im.crop((x, y, x + size[0], y + size[1])))
        else:
            p.add(name, Image.open(io.BytesIO(files[src + ".png"])))
    out = ["/* Level Devil's own pictures and fonts (tools/pictures.py, from the game's web build) */",
           "typedef struct {", "  uint16_t w, h, pal;", "  uint8_t tint, runs;", "  uint32_t at;", "} pic_t;",
           "enum {"]
    out += [f"  P_{n}," for n in p.names]
    out += ["  PICS", "};"]
    out.append("static const uint16_t pic_pal[%d] = {" % len(p.pal))
    for i in range(0, len(p.pal), 12):
        out.append("  " + " ".join("0x%04X," % v for v in p.pal[i:i + 12]))
    out.append("};")
    out.append("static const pic_t pics[PICS] = {")
    for (w, h, pal, tint, runs, at), n in zip(p.pics, p.names):
        out.append(f"  {{{w}, {h}, {pal}, {tint}, {runs}, {at}}}, /* {n} */")
    out.append("};")
    out.append("static const uint8_t pic_data[%d] = {" % len(p.data))
    for i in range(0, len(p.data), 32):
        out.append("  " + ",".join(str(b) for b in p.data[i:i + 32]) + ",")
    out.append("};")
    out += ["typedef struct {", "  uint8_t adv, w, h;", "  int8_t x, y;", "  uint16_t at;", "} glyph_t;",
            "typedef struct {", "  uint8_t line, base;", "  const glyph_t *g;", "  const uint8_t *bits;", "} pfont_t;"]
    for name, fid in FONTS:
        line, base, glyphs = font(files[f"font-{fid}.png"], files[f"font-{fid}.fnt"])
        bits, table = bytearray(), []
        for adv, w, h, x, y, b in glyphs:
            table.append(f"{{{adv}, {w}, {h}, {x}, {y}, {len(bits)}}}")
            bits += b
        low = name.lower()
        out.append(f"static const glyph_t {low}_glyphs[95] = {{")
        for i in range(0, 95, 8):
            out.append("  " + ", ".join(table[i:i + 8]) + ",")
        out.append("};")
        out.append(f"static const uint8_t {low}_bits[{len(bits)}] = {{")
        for i in range(0, len(bits), 32):
            out.append("  " + ",".join(str(v) for v in bits[i:i + 32]) + ",")
        out.append("};")
        out.append(f"static const pfont_t {low} = {{{line}, {base}, {low}_glyphs, {low}_bits}};")
    with open(OUT, "w") as f:
        f.write("\n".join(out) + "\n")
    print(f"{len(p.pics)} pictures, {len(p.data)} bytes, {len(p.pal)} colours into {os.path.relpath(OUT)}",
          file=sys.stderr)


if __name__ == "__main__":
    main()
