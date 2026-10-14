#!/usr/bin/env python3
"""Undertale's pictures, rooms and fonts -> src/data.bin + src/data.h.

    python3 -I tools/pack.py REF        (REF: the folder tools/fetch.py filled)

data.bin: "UTD1", the number of sections, their offsets, then the sections (SEC_* in data.h):
- SPRITES: for each sprite (SPR_*) its first frame and its number of frames (u16, u16)
- FRAMES: each frame: w, h, origin x, y (u16, u16, s16, s16), bpp, colours (u8, u8), the colours (RGB565),
  each row's offset (u16) from the rows' data, then the rows: runs of (skip, length, length pixels of bpp bits,
  lowest first, the run padded to a byte), until the row's width
- TILES: the areas' 20 x 20 tiles: bpp (0: one colour), colours, the colours (RGB565), 400 pixels of bpp bits
- ROOMS: 32 bytes each (res.c room_info): the room's rectangle on its area's map, its tile grid and
  collision map (4-pixel cells, 1 walkable), its area, flags and name
- GRID, COLL: the rooms' tile ids (u16, 0xFFFF none) and collision bits
- FONTS: height, first character, count, flags, then 8 bytes a glyph (advance, w, h, ox, oy, 0,
  bitmap offset u16) and the bitmaps (rows of whole bytes, highest bit first)
- STRINGS: the room names and other texts the data refers to
- DOORS: for each room its first door and how many (u16, u16), then the doors: 16 bytes each (x, y, w, h on
  the map, the room it leads to, where Frisk's top-left lands, the way he faces)"""
import os
import struct
import sys

import numpy as np
from PIL import Image, ImageDraw, ImageFont, ImageSequence

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import rooms as R  # noqa: E402
import sprites as S  # noqa: E402

OUT_BIN = os.path.join(HERE, "..", "src", "data.bin")
OUT_H = os.path.join(HERE, "..", "src", "data.h")
SECTIONS = ["SPRITES", "FRAMES", "TILES", "ROOMS", "GRID", "COLL", "FONTS", "STRINGS", "DOORS"]


def rgb565(r, g, b):
    r, g, b = int(r), int(g), int(b)
    return ((r * 31 + 127) // 255) << 11 | ((g * 63 + 127) // 255) << 5 | ((b * 31 + 127) // 255)


class Strings:
    def __init__(self):
        self.list, self.ids = [], {}

    def add(self, s):
        if s not in self.ids:
            self.ids[s] = len(self.list)
            self.list.append(s)
        return self.ids[s]

    def blob(self):
        out, offs = bytearray(), []
        base = 4 + 4 * len(self.list)
        for s in self.list:
            offs.append(base + len(out))
            out += s.encode("latin-1") + b"\0"
        return struct.pack("<I", len(self.list)) + b"".join(struct.pack("<I", o) for o in offs) + bytes(out)


def table(entries):
    """u32 count, u32 offsets (from the section's start), then the entries"""
    out, offs = bytearray(), []
    base = 4 + 4 * len(entries)
    for e in entries:
        while (base + len(out)) % 2:
            out.append(0)
        offs.append(base + len(out))
        out += e
    return struct.pack("<I", len(entries)) + b"".join(struct.pack("<I", o) for o in offs) + bytes(out)


def bits_pack(values, bpp):
    """values (ints < 2**bpp), lowest bits first"""
    out = bytearray((len(values) * bpp + 7) // 8)
    for k, v in enumerate(values):
        b = k * bpp
        out[b >> 3] |= v << (b & 7)
    return out


def bpp_for(n):
    return 1 if n <= 2 else 2 if n <= 4 else 4 if n <= 16 else 8


# ------------------------------------------------------------------ frames
def encode_frame(img, ox, oy):
    """an RGBA image -> the frame's bytes (row RLE)"""
    a = np.array(img.convert("RGBA"))
    h, w = a.shape[:2]
    opaque = a[:, :, 3] >= 128
    cols = {}
    for c in map(tuple, a[opaque][:, :3]):
        k = rgb565(*c)
        cols[k] = cols.get(k, 0) + 1
    pal = sorted(cols, key=lambda k: -cols[k])
    if len(pal) > 255:
        raise ValueError("a frame with more than 255 colours")
    index = {k: i for i, k in enumerate(pal)}
    bpp = bpp_for(max(len(pal), 1))
    rows, data = [], bytearray()
    for y in range(h):
        rows.append(len(data))
        x = 0
        while x < w:
            skip = 0
            while x < w and not opaque[y, x] and skip < 255:
                x += 1
                skip += 1
            run = []
            while x < w and opaque[y, x] and len(run) < 255:
                run.append(index[rgb565(*a[y, x, :3])])
                x += 1
            data += bytes([skip, len(run)]) + bits_pack(run, bpp)
        if len(data) > 0xFFFF:
            raise ValueError("a frame too big")
    head = struct.pack("<HHhhBB", w, h, ox, oy, bpp, len(pal)) + b"".join(struct.pack("<H", k) for k in pal)
    return head + b"".join(struct.pack("<H", r) for r in rows) + bytes(data)


def halve(img):
    """a sprite the wiki shows at twice the game's size -> the game's size (the phase where 2 x 2 blocks agree)"""
    a = np.array(img.convert("RGBA")).astype(np.int32)
    h, w = a.shape[:2]
    best = None
    for py in (0, 1):
        for px in (0, 1):
            b = a[py:py + (h - py) // 2 * 2, px:px + (w - px) // 2 * 2]
            if not b.size:
                continue
            q = b.reshape(b.shape[0] // 2, 2, b.shape[1] // 2, 2, 4)
            err = np.abs(q - q[:, :1, :, :1]).sum()
            if best is None or err < best[0]:
                best = (err, py, px)
    _, py, px = best
    b = a[py::2, px::2]
    return Image.fromarray(b.astype(np.uint8), "RGBA")


def load_frames(ref, src):
    """'demo:spr_name' (all its numbered frames), 'wiki:File.png' or .gif (all frames, halved), 'png:path'"""
    kind, name = src.split(":", 1)
    if kind == "demo":
        d = os.path.join(ref, "demo", "mac", "sprites")
        out, k = [], 0
        while os.path.exists(os.path.join(d, "%s %d.png" % (name, k))):
            out.append(Image.open(os.path.join(d, "%s %d.png" % (name, k))).convert("RGBA"))
            k += 1
        if not out:
            raise FileNotFoundError(src)
        return out
    if kind == "demobg":
        return [Image.open(os.path.join(ref, "demo", "mac", "bgs", name + ".png")).convert("RGBA")]
    if kind in ("wiki", "wiki1"):
        im = Image.open(os.path.join(ref, "wiki", "sprites", name))
        frames = []
        for fr in ImageSequence.Iterator(im):
            f = fr.convert("RGBA")
            frames.append(halve(f) if kind == "wiki" else f)
        return frames
    if kind == "png":
        return [Image.open(os.path.join(HERE, name)).convert("RGBA")]
    raise ValueError(src)


def crop_frames(frames):
    """all frames cropped to the union of their opaque pixels; returns the frames and the crop's top-left"""
    box = None
    for f in frames:
        b = f.getchannel("A").point(lambda v: 255 if v >= 128 else 0).getbbox()
        if b:
            box = b if box is None else (min(box[0], b[0]), min(box[1], b[1]), max(box[2], b[2]), max(box[3], b[3]))
    if box is None:
        return frames, 0, 0
    return [f.crop(box) for f in frames], box[0], box[1]


# ------------------------------------------------------------------ fonts
def make_font(ref, spec):
    path = os.path.join(ref, "fonts", spec["ttf"]) if not spec["ttf"].startswith("/") else spec["ttf"]
    font = ImageFont.truetype(path, spec["size"])
    first, last = 32, 127
    glyphs, bits = bytearray(), bytearray()
    asc = spec.get("ascent", spec["size"])
    for c in range(first, last):
        ch = chr(c) if c < 127 else "♥"
        if c == 127 and "heart" not in spec:
            ch = "?"
        im = Image.new("L", (spec["size"] * 3, spec["size"] * 3), 0)
        d = ImageDraw.Draw(im)
        d.fontmode = "1"
        d.text((spec["size"], spec["size"]), ch, font=font, fill=255)
        adv = int(round(font.getlength(ch))) if "advance" not in spec else spec["advance"]
        bb = im.getbbox()
        if bb is None:
            glyphs += struct.pack("<BBBbbBH", adv, 0, 0, 0, 0, 0, len(bits))
            continue
        g = np.array(im.crop(bb)) >= 128
        gh, gw = g.shape
        ox, oy = bb[0] - spec["size"], bb[1] - spec["size"] - (spec["size"] - asc)
        off = len(bits)
        for row in g:
            rb = bytearray((gw + 7) // 8)
            for x, v in enumerate(row):
                if v:
                    rb[x >> 3] |= 0x80 >> (x & 7)
            bits += rb
        glyphs += struct.pack("<BBBbbBH", adv, gw, gh, ox, oy, 0, off)
    return struct.pack("<BBBB", spec["height"], first, last - first, 0) + bytes(glyphs) + bytes(bits)


# ------------------------------------------------------------------ areas and rooms
class Area:
    """an area's map: its colours, as indices into a palette, and its tiles"""

    def __init__(self, ref, spec):
        im = Image.open(os.path.join(ref, "wiki", "maps", spec["map"]))
        if im.mode != "P":
            im = im.convert("RGB").quantize(256, method=Image.Quantize.MAXCOVERAGE)
        self.idx = np.array(im)
        p = im.getpalette()
        self.rgb = [tuple(p[3 * i:3 * i + 3]) for i in range(len(p) // 3)]
        self.transparent = im.info.get("transparency", None)
        if isinstance(self.transparent, bytes):
            self.transparent = None
        self.spec = spec
        for patch in spec.get("patches", []):
            self.patch(patch)

    def black_index(self):
        for i, c in enumerate(self.rgb):
            if c == (0, 0, 0):
                return i
        return None

    def patch(self, p):
        """changes to the map, before rooms are cut from it: ('fill', x, y, w, h, (r, g, b)),
        ('copy', sx, sy, w, h, dx, dy) or ('mirror', ...) the same, flipped left to right"""
        if p[0] == "fill":
            _, x, y, w, h, c = p
            self.idx[y:y + h, x:x + w] = self.color_index(c)
        elif p[0] == "copy":
            _, sx, sy, w, h, dx, dy = p
            self.idx[dy:dy + h, dx:dx + w] = self.idx[sy:sy + h, sx:sx + w].copy()
        elif p[0] == "mirror":
            _, sx, sy, w, h, dx, dy = p
            self.idx[dy:dy + h, dx:dx + w] = self.idx[sy:sy + h, sx:sx + w][:, ::-1].copy()
        else:
            raise ValueError(p)

    def color_index(self, c):
        for i, k in enumerate(self.rgb):
            if k == tuple(c) and i != self.transparent:
                return i
        raise ValueError("colour %s not in the map" % (c,))


def room_pixels(area, r):
    """the room's rectangle of the map, as RGB565 per pixel; outside the map and its clear pixels black"""
    x, y, w, h = r["rect"]
    H, W = area.idx.shape
    out = np.zeros((h, w), np.int32)
    xs0, ys0, xs1, ys1 = max(x, 0), max(y, 0), min(x + w, W), min(y + h, H)
    sub = area.idx[ys0:ys1, xs0:xs1]
    lut = np.array([rgb565(*c) for c in area.rgb] + [0] * (256 - len(area.rgb)), np.int32)
    if area.transparent is not None:
        lut[area.transparent] = 0
    out[ys0 - y:ys1 - y, xs0 - x:xs1 - x] = lut[sub]
    return out, sub, (xs0 - x, ys0 - y)


def collision(area, r):
    """4-pixel cells over the room: walkable where the floor's colours fill at least half the cell and
    inside one of its floor rectangles (all of it if it has none), then its own fixes"""
    x, y, w, h = r["rect"]
    cw, ch = (w + 3) // 4, (h + 3) // 4
    H, W = area.idx.shape
    walk_idx = set()
    for c in area.spec["floor"] + r.get("floor_colors", []):
        for i, k in enumerate(area.rgb):
            if k == tuple(c) and i != area.transparent:
                walk_idx.add(i)
    sub = np.zeros((ch * 4, cw * 4), bool)
    xs0, ys0, xs1, ys1 = max(x, 0), max(y, 0), min(x + w, W), min(y + h, H)
    m = np.isin(area.idx[ys0:ys1, xs0:xs1], list(walk_idx))
    sub[ys0 - y:ys1 - y, xs0 - x:xs1 - x] = m
    cells = sub.reshape(ch, 4, cw, 4).mean(axis=(1, 3)) >= 0.5
    # walls drawn in the floor's colours give themselves away by their lines: a cell is floor only with no
    # other colour within `margin` pixels of it
    margin = r.get("margin", area.spec.get("margin", 0))
    if margin:
        other = ~sub
        pad = np.pad(other, margin, constant_values=False)
        near = np.zeros_like(other)
        for dy in range(2 * margin + 1):
            for dx in range(2 * margin + 1):
                near |= pad[dy:dy + other.shape[0], dx:dx + other.shape[1]]
        cells &= ~near.reshape(ch, 4, cw, 4).any(axis=(1, 3))
    if r.get("floor_rects"):
        keep = np.zeros_like(cells)
        for fx, fy, fw, fh in r["floor_rects"]:
            keep[max(0, (fy - y) // 4):max(0, (fy + fh - y + 3) // 4), max(0, (fx - x) // 4):max(0, (fx + fw - x + 3) // 4)] = True
        cells &= keep
    for kind, fx, fy, fw, fh in r.get("fix", []):
        cells[max(0, (fy - y) // 4):max(0, (fy + fh - y + 3) // 4), max(0, (fx - x) // 4):max(0, (fx + fw - x + 3) // 4)] = kind == "walk"
    out = bytearray()
    for row in cells:
        rb = bytearray((cw + 7) // 8)
        for i, v in enumerate(row):
            if v:
                rb[i >> 3] |= 1 << (i & 7)
        out += rb
    return out, cw, ch


class TileSet:
    def __init__(self):
        self.ids, self.blobs = {}, []
        self.blobs_size = 0

    def add(self, block):
        """a 20 x 20 block of RGB565 values -> its tile id"""
        key = block.tobytes()
        if key in self.ids:
            return self.ids[key]
        flat = block.reshape(-1)
        cols, counts = np.unique(flat, return_counts=True)
        order = [int(c) for c, _ in sorted(zip(cols, counts), key=lambda t: -t[1])]
        if len(order) == 1:
            blob = struct.pack("<BBH", 0, 1, order[0])
        else:
            index = {c: i for i, c in enumerate(order)}
            bpp = bpp_for(len(order))
            blob = struct.pack("<BB", bpp, len(order)) + b"".join(struct.pack("<H", c) for c in order)
            blob += bits_pack([index[int(v)] for v in flat], bpp)
        self.ids[key] = len(self.blobs)
        self.blobs.append(blob)
        return self.ids[key]


def pack_room(area, r, tiles, strings):
    x, y, w, h = r["rect"]
    px, _, _ = room_pixels(area, r)
    # the grid: 20-pixel tiles aligned on the map's own 20-pixel grid
    gx, gy = x // 20 * 20, y // 20 * 20
    gx1, gy1 = (x + w + 19) // 20 * 20, (y + h + 19) // 20 * 20
    cols, rows = (gx1 - gx) // 20, (gy1 - gy) // 20
    big = np.zeros((rows * 20, cols * 20), np.int32)
    big[y - gy:y - gy + h, x - gx:x - gx + w] = px
    grid = bytearray()
    for ty in range(rows):
        for tx in range(cols):
            block = big[ty * 20:ty * 20 + 20, tx * 20:tx * 20 + 20]
            grid += struct.pack("<H", 0xFFFF if not block.any() else tiles.add(block.astype(np.uint16)))
    coll, cw, ch = collision(area, r)
    return dict(rect=(x, y, w, h), g=(gx, gy, cols, rows), grid=bytes(grid), coll=bytes(coll), cw=cw, ch=ch,
                area=r["area_id"], flags=r.get("flags", 0), name=strings.add(r.get("title", "")))


# ------------------------------------------------------------------ main
def main():
    if len(sys.argv) < 2:
        sys.exit(__doc__)
    ref = sys.argv[1]
    strings = Strings()
    strings.add("")
    defines = []

    # sprites
    spr_entries, frames = [], []
    for k, spec in enumerate(S.SPRITES):
        name, src = spec[0], spec[1]
        opts = spec[2] if len(spec) > 2 else {}
        imgs = []
        for s in (src if isinstance(src, list) else [src]):
            imgs += load_frames(ref, s)
        if "frames" in opts:
            imgs = [imgs[i] for i in opts["frames"]]
        if "scale" in opts:
            imgs = [im.resize((im.width * opts["scale"], im.height * opts["scale"]), Image.NEAREST) for im in imgs]
        w0, h0 = imgs[0].size
        ox, oy = opts.get("origin", (0, 0))
        if opts.get("center"):
            ox, oy = w0 // 2, h0 // 2
        if opts.get("feet"):
            ox, oy = w0 // 2, h0
        if not opts.get("keep"):
            imgs, cx, cy = crop_frames(imgs)
            ox, oy = ox - cx, oy - cy
        spr_entries.append((len(frames), len(imgs)))
        for im in imgs:
            frames.append(encode_frame(im, ox, oy))
        defines.append(("SPR_" + name.upper(), k))
    sec_sprites = struct.pack("<I", len(spr_entries)) + b"".join(struct.pack("<HH", a, b) for a, b in spr_entries)
    sec_frames = table(frames)

    # areas and rooms
    tiles = TileSet()
    packed = []
    for ai, aspec in enumerate(R.AREAS):
        area = Area(ref, aspec)
        defines.append(("AREA_" + aspec["name"].upper(), ai))
        for r in R.ROOMS:
            if r["area"] != aspec["name"]:
                continue
            r["area_id"] = ai
            packed.append((r, pack_room(area, r, tiles, strings)))
    room_tab, grid_blob, coll_blob = bytearray(), bytearray(), bytearray()
    for k, (r, p) in enumerate(packed):
        defines.append(("ROOM_" + r["name"].upper(), k))
        x, y, w, h = p["rect"]
        gx, gy, cols, rows = p["g"]
        room_tab += struct.pack("<hhhhhhHHIIHHBBH", x, y, w, h, gx, gy, cols, rows, len(grid_blob), len(coll_blob),
                                p["cw"], p["ch"], p["area"], p["flags"], p["name"])
        grid_blob += p["grid"]
        coll_blob += p["coll"]
        while len(coll_blob) % 2:
            coll_blob.append(0)
    sec_rooms = struct.pack("<I", len(packed)) + bytes(room_tab)
    names = {r["name"]: k for k, (r, _) in enumerate(packed)}
    dirs = {"down": 0, "up": 1, "left": 2, "right": 3}
    door_index, doors = bytearray(), bytearray()
    ndoors = 0
    for r, _ in packed:
        ds = r.get("doors", [])
        door_index += struct.pack("<HH", ndoors, len(ds))
        for (dx, dy, dw, dh), to, (tx, ty), d in ds:
            doors += struct.pack("<hhhhHhhH", dx, dy, dw, dh, names[to], tx, ty, dirs[d])
            ndoors += 1
    sec_doors = bytes(door_index) + bytes(doors)
    sec_tiles = table(tiles.blobs)

    # fonts
    fonts = []
    for k, spec in enumerate(S.FONTS):
        fonts.append(make_font(ref, spec))
        defines.append(("FONT_" + spec["name"].upper(), k))
    sec_fonts = table(fonts)

    sections = [sec_sprites, sec_frames, sec_tiles, sec_rooms, bytes(grid_blob), bytes(coll_blob), sec_fonts,
                strings.blob(), sec_doors]
    head = b"UTD1" + struct.pack("<I", len(sections))
    offs, body = [], bytearray()
    base = len(head) + 4 * len(sections)
    for s in sections:
        while (base + len(body)) % 4:
            body.append(0)
        offs.append(base + len(body))
        body += s
    blob = head + b"".join(struct.pack("<I", o) for o in offs) + bytes(body)
    with open(OUT_BIN, "wb") as f:
        f.write(blob)

    with open(OUT_H, "w") as f:
        f.write("/* made by tools/pack.py: the contents of data.bin */\n#ifndef UT_DATA_H\n#define UT_DATA_H\n")
        for i, s in enumerate(SECTIONS):
            f.write("#define SEC_%s %d\n" % (s, i))
        for n, v in defines:
            f.write("#define %s %d\n" % (n, v))
        f.write("#define NUM_SPRITES %d\n#define NUM_ROOMS %d\n#define NUM_FONTS %d\n" % (len(spr_entries), len(packed), len(fonts)))
        f.write("#endif\n")
    sizes = dict(zip(SECTIONS, map(len, sections)))
    print("data.bin: %d bytes (%s); %d tiles, %d frames, %d rooms" % (
        len(blob), ", ".join("%s %d" % kv for kv in sizes.items()), len(tiles.blobs), len(frames), len(packed)))


if __name__ == "__main__":
    main()
