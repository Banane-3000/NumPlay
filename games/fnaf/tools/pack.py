#!/usr/bin/env python3
"""Five Nights at Freddy's: its pictures and fonts -> src/data.bin + src/data.h.

    python3 -I tools/pack.py REF        (REF: the folder tools/fetch.py filled; Python + Pillow + NumPy)

data.bin: "FNF1", the number of sections, their offsets, then the sections (SEC_* in data.h):
- HUFF: the four Huffman tables (Y DC, Y AC, colour DC, colour AC), 1424 bytes each (tools/codec.py)
- QUANT: for each quality, the Y and colour tables (64 u16 each, natural order, scaled for the IDCT)
- PICS: 12 bytes a picture (PIC_*): its width and height in 16 x 16 squares, its quality, 0, the picture it
  shares squares with (u16, 0xFFFF none), how many squares it has of its own then (u16) and where its table
  starts (u32)
- TABLE: a whole picture's squares (3 bytes each: where in BANK), or a picture sharing another's: the squares
  that differ (u16 the square's index, then 3 bytes)
- BANK: the squares, each from a byte boundary
- FONTS: for each font (FONT_*) its line height, baseline and 95 glyphs (32 to 126): advance, width, height,
  x and y from the top of the line (s8), 0, the offset of its 4-bit alpha rows (u32, from the section)
- IMGS: small pictures with alpha (IMG_*): width, height (u16), then 4-bit alpha (or 4-bit grey and alpha)
- MAP: the camera map's walls, 1 bit a pixel, and where the camera buttons are (MAP_*)

The office and the cameras are the game's 1600 x 720 pictures at a third (533 x 240): the calculator shows 320
of it and turns, as the game turns its 1280. Doors, buttons and the fan are drawn over the office."""
import os
import struct
import sys

import numpy as np
from PIL import Image, ImageDraw, ImageFont

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import codec as C  # noqa: E402

OUT_BIN = os.path.join(HERE, "..", "src", "data.bin")
OUT_H = os.path.join(HERE, "..", "src", "data.h")
ROOT = os.path.join(HERE, "..", "..", "..")
SECTIONS = ["HUFF", "QUANT", "PICS", "TABLE", "BANK", "FONTS", "IMGS", "MAP"]
QUALITY = {"scene": 80, "scare": 70, "paper": 84, "face": 76}
FONT_DIRS = ["/usr/share/fonts/truetype/dejavu", "/usr/share/fonts/TTF", "/usr/local/share/fonts",
             "/Library/Fonts", os.path.expanduser("~/Library/Fonts")]

OW, OH = 533, 240     # the office and the cameras on the calculator
REF = None


def web(path):
    return os.path.join(REF, "web", "src", "media", "Textures", path)


def hx(path):
    return os.path.join(REF, "hxnaf", "assets", "images", path)


def rgb(path, frame=0):
    im = Image.open(path)
    if frame:
        im.seek(frame)
    return im.convert("RGB")


def third(im, size=(OW, OH)):
    return np.asarray(im.resize(size, Image.LANCZOS)).astype(np.float64)


def pad16(a):
    h, w = a.shape[:2]
    H, W = -(-h // 16) * 16, -(-w // 16) * 16
    return np.pad(a, ((0, H - h), (0, W - w), (0, 0)), mode="edge")


# ------------------------------------------------------------------ pictures
class Pic:
    def __init__(self, name, a, quality, base=None, refs=()):
        self.name, self.q, self.base = name, quality, base
        self.w, self.h = a.shape[1], a.shape[0]
        self.a = pad16(a)
        self.refs = [r for r in ([base] if base else []) + list(refs) if r]  # pictures to take squares from
        self.mw, self.mh = self.a.shape[1] // 16, self.a.shape[0] // 16


PICS = []


def pic(name, a, quality="scene", base=None, refs=()):
    p = Pic(name, np.clip(a, 0, 255), quality, base, refs)
    PICS.append(p)
    return p


def same_square(a, b, strict):
    d = np.abs(a - b)
    return d.max() <= (6 if strict else 14) and d.mean() <= (1.2 if strict else 3.0)


def encode_all():
    qt = {k: (C.qtable(C.STD_LUMA, v), C.qtable(C.STD_CHROMA, v)) for k, v in QUALITY.items()}
    qnames = list(QUALITY)
    # each picture's squares: its own symbols, or a square of a picture it shares with
    for p in PICS:
        y, cb, cr = C.ycbcr(p.a)
        ql, qc = qt[p.q]
        yq = C.quantize(C.blocks_of(y), ql, 0.18)
        cbq = C.quantize(C.blocks_of(cb), qc, 0.22)
        crq = C.quantize(C.blocks_of(cr), qc, 0.22)
        p.sq = []
        for my in range(p.mh):
            for mx in range(p.mw):
                src = None
                tile = p.a[my * 16:my * 16 + 16, mx * 16:mx * 16 + 16]
                for r in p.refs:
                    if mx < r.mw and my < r.mh and r.q == p.q:
                        if same_square(tile, r.a[my * 16:my * 16 + 16, mx * 16:mx * 16 + 16], r is not p.base):
                            src = r.sq[my * r.mw + mx]
                            break
                if src is None:
                    blocks = [yq[2 * my + i, 2 * mx + j] for i in (0, 1) for j in (0, 1)]
                    src = ("own", tuple(C.mcu_symbols(blocks, cbq[my, mx], crq[my, mx])))
                p.sq.append(src)
    # Huffman tables from every square of its own, once
    freq = [[{}, {}], [{}, {}]]
    seen = set()
    for p in PICS:
        for s in p.sq:
            if id(s) in seen:
                continue
            seen.add(id(s))
            for comp, tab, sym, _, _ in s[1]:
                f = freq[comp][tab]
                f[sym] = f.get(sym, 0) + 1
    tables, huff = [[None, None], [None, None]], b""
    for comp in (0, 1):
        for tab in (0, 1):
            codes, bits, vals = C.canonical(C.huffman_lengths(freq[comp][tab]))
            tables[comp][tab] = codes
            huff += C.decode_tables(bits, vals)
    bank, where = bytearray(), {}
    by_id = {}
    for p in PICS:
        p.off = []
        for s in p.sq:
            if id(s) not in by_id:
                data = C.encode_mcu(s[1], tables)
                if data not in where:
                    where[data] = len(bank)
                    bank += data
                by_id[id(s)] = where[data]
            p.off.append(by_id[id(s)])
    bank += bytes(8)
    assert len(bank) < 1 << 24
    quant = b""
    for k in qnames:
        for t in qt[k]:
            quant += b"".join(struct.pack("<H", (int(t[i]) * C.AANSCALES[i] + 2048) >> 12) for i in range(64))
    pics, table = b"", bytearray()
    for i, p in enumerate(PICS):
        diff = []
        if p.base:
            b = p.base
            assert (b.mw, b.mh) == (p.mw, p.mh) and not b.base
            diff = [k for k in range(len(p.off)) if p.off[k] != b.off[k]]
        if p.base and 5 * len(diff) < 3 * len(p.off):   # (else a whole table is smaller)
            pics += struct.pack("<BBBBHHI", p.mw, p.mh, qnames.index(p.q), 0, PICS.index(b), len(diff), len(table))
            for k in diff:
                table += struct.pack("<H", k) + p.off[k].to_bytes(3, "little")
        else:
            pics += struct.pack("<BBBBHHI", p.mw, p.mh, qnames.index(p.q), 0, 0xFFFF, 0, len(table))
            for o in p.off:
                table += o.to_bytes(3, "little")
    print(f"pictures: {len(PICS)}, squares {len(bank)} bytes, tables {len(table)} bytes")
    return huff, quant, pics, bytes(table), bytes(bank)


# ------------------------------------------------------------------ fonts and small pictures
def find_font(name):
    for d in FONT_DIRS:
        p = os.path.join(d, name)
        if os.path.exists(p):
            return p
    raise SystemExit(f"pack: {name} not found (the DejaVu fonts)")


def nibbles(alpha):
    """an h x w array of 0-15 -> rows of 4-bit pixels, the first in the low half of each byte"""
    h, w = alpha.shape
    out = bytearray()
    for r in range(h):
        row = list(alpha[r]) + [0]
        for c in range(0, w, 2):
            out.append(int(row[c]) | int(row[c + 1]) << 4)
    return bytes(out)


def to4(a):
    return np.clip(np.round(np.asarray(a, dtype=np.float64) / 255 * 15), 0, 15).astype(np.uint8)


def ttf_font(path, size, line=None):
    font = ImageFont.truetype(path, size)
    asc, desc = font.getmetrics()
    line = line or asc + desc
    glyphs = []
    for c in range(32, 127):
        ch = chr(c)
        adv = int(round(font.getlength(ch)))
        box = font.getbbox(ch)
        if box[2] <= box[0] or ch == " ":
            glyphs.append((adv, 0, 0, 0, 0, b""))
            continue
        im = Image.new("L", (box[2] - box[0], box[3] - box[1]))
        ImageDraw.Draw(im).text((-box[0], -box[1]), ch, font=font, fill=255)
        glyphs.append((adv, im.width, im.height, box[0], box[1], nibbles(to4(np.asarray(im)))))
    return line, asc, glyphs


def fnt_font(fnt, png, scale, extra_adv=0):
    """an AngelCode .fnt bitmap font (white glyphs), scaled down"""
    sheet = Image.open(png).convert("RGBA")
    a = np.asarray(sheet).astype(np.float64)
    alpha = a[..., 3] * a[..., :3].mean(axis=2) / 255 if a[..., 3].min() < 255 else a[..., :3].mean(axis=2)
    chars, line, base = {}, 0, 0
    for row in open(fnt):
        kv = dict(t.split("=", 1) for t in row.split() if "=" in t)
        if row.startswith("common"):
            line, base = int(kv["lineHeight"]), int(kv["base"])
        if row.startswith("char "):
            chars[int(kv["id"])] = {k: int(v) for k, v in kv.items() if k in
                                     ("x", "y", "width", "height", "xoffset", "yoffset", "xadvance")}
    glyphs = []
    for c in range(32, 127):
        g = chars.get(c)
        if not g or not g["width"]:
            adv = int(round((g["xadvance"] if g else line * 0.4) * scale)) + extra_adv
            glyphs.append((adv, 0, 0, 0, 0, b""))
            continue
        crop = Image.fromarray(alpha[g["y"]:g["y"] + g["height"], g["x"]:g["x"] + g["width"]].astype(np.uint8))
        x0, y0 = g["xoffset"] * scale, g["yoffset"] * scale
        w = max(1, int(np.ceil(x0 + g["width"] * scale)) - int(np.floor(x0)))
        h = max(1, int(np.ceil(y0 + g["height"] * scale)) - int(np.floor(y0)))
        # placed at its fraction of a pixel, then reduced
        big = Image.new("L", (w * 8, h * 8))
        big.paste(crop.resize((max(1, int(round(g["width"] * scale * 8))), max(1, int(round(g["height"] * scale * 8)))),
                              Image.LANCZOS), (int(round((x0 - np.floor(x0)) * 8)), int(round((y0 - np.floor(y0)) * 8))))
        im = big.resize((w, h), Image.BOX)
        glyphs.append((int(round(g["xadvance"] * scale)) + extra_adv, w, h, int(np.floor(x0)), int(np.floor(y0)),
                       nibbles(to4(np.asarray(im)))))
    return int(round(line * scale)), int(round(base * scale)), glyphs


def pack_fonts(fonts):
    head, data = b"", b""
    base = 4 + 4 * len(fonts)
    blobs = []
    for line, asc, glyphs in fonts:
        b = struct.pack("<BBBB", line, asc, 32, 95)
        rows = b""
        offs = []
        for adv, w, h, x, y, r in glyphs:
            offs.append(len(rows))
            rows += r
        g0 = len(b) + 12 * 95
        for (adv, w, h, x, y, r), o in zip(glyphs, offs):
            b += struct.pack("<BBBbbBHI", adv, w, h, x, y, 0, 0, g0 + o)
        blobs.append(b + rows)
    out = struct.pack("<I", len(fonts))
    p = base
    for b in blobs:
        out += struct.pack("<I", p)
        p += len(b)
    return out + b"".join(blobs)


# ------------------------------------------------------------------ the game's pictures
DEFINES = []


def define(name, value):
    DEFINES.append((name, value))


def office_pictures():
    o = lambda n: third(rgb(web(f"Office/{n}.webp")))  # noqa: E731
    office = pic("OFFICE", o("Default"))
    for n, src in (("OFFICE_LL", "LL"), ("OFFICE_LLB", "LL_BONNIE"), ("OFFICE_RL", "RL"), ("OFFICE_RLC", "RL_CHICA")):
        pic(n, o(src), base=office)
    dark = pic("DARK", o("304"))
    pic("DARK_FREDDY", o("305"), base=dark)
    # the doors: where they differ from the open office, closed
    default = np.asarray(rgb(web("Office/Default.webp"))).astype(int)
    for side, src in (("L", "LD"), ("R", "RD")):
        full = np.asarray(rgb(web(f"Office/{src}.webp"))).astype(int)
        d = np.abs(full - default).max(axis=2) > 24
        ys, xs = np.nonzero(d)
        x0, x1 = int(np.floor(xs.min() / 3)), int(np.ceil((xs.max() + 1) / 3))
        y1 = int(np.ceil((ys.max() + 1) / 3))
        closed = o(src)
        pic(f"DOOR_{side}", closed[0:y1, x0:x1])
        define(f"DOOR_{side}_X", x0)
        define(f"DOOR_{side}_W", x1 - x0)
        define(f"DOOR_{side}_H", y1)
    # the buttons: off as the game shows them; the door's lit green, the light's lit white
    for side, (x0, x1) in (("L", (0, 32)), ("R", (496, 544))):
        mx0, mx1, my0, my1 = x0 // 16, x1 // 16, 96 // 16, 176 // 16
        for state in range(4):
            base = o("LL" if side == "L" and state & 2 else "RL" if side == "R" and state & 2 else "Default")
            base = pad16(base)[my0 * 16:my1 * 16, mx0 * 16:mx1 * 16].copy()
            light_buttons(base, side, state)
            pic(f"PANEL_{side}{state}", base)
        define(f"PANEL_{side}_X", mx0 * 16)
    define("PANEL_Y", 96)
    # the fan: its blades turning, three frames
    fan_frames(o("Default"))
    golden_office(o("Default"))


def golden_office(office):
    """Golden Freddy, slumped in front of the desk: Freddy as he comes at you in the dark (lit, on black),
    gold, his eyes empty, his head fallen to a side; a patch of the office around him"""
    im = Image.open(web("Freddy-Jumpscare1.gif"))
    im.seek(9)
    a = np.asarray(im.convert("RGB")).astype(np.float64)
    lum = a.mean(axis=2)
    eyes = (lum > 150) & (a.max(axis=2) - a.min(axis=2) < 60)
    alpha = np.clip((lum - 6) / 18, 0, 1)
    gold = np.stack([lum * 1.35, lum * 1.05, lum * 0.32], axis=2)
    gold[eyes] = 0
    rgba = np.dstack([np.clip(gold, 0, 255), alpha * 255]).astype(np.uint8)
    sprite = Image.fromarray(rgba, "RGBA").rotate(-24, resample=Image.BICUBIC, expand=True)
    box = sprite.getbbox()
    sprite = sprite.crop(box)
    h = 128
    sprite = sprite.resize((int(sprite.width * h / sprite.height), h), Image.LANCZOS)
    x0, y0 = 266 - sprite.width // 2, OH - h + 30
    base = Image.fromarray(office.astype(np.uint8))
    dim = sprite.copy()
    dim.putdata([(int(r * 0.55), int(g * 0.55), int(b * 0.55), al) for r, g, b, al in sprite.getdata()])
    base.paste(dim, (x0, y0), dim)
    gx0, gy0 = x0 // 16 * 16, y0 // 16 * 16
    gx1 = min(OW, -(-(x0 + sprite.width) // 16) * 16)
    patch = np.asarray(base).astype(np.float64)[gy0:OH, gx0:gx1]
    pic("OFFICE_GOLD", patch)
    define("GOLD_X", gx0)
    define("GOLD_Y", gy0)
    define("GOLD_W", gx1 - gx0)
    define("GOLD_H", OH - gy0)


def light_buttons(a, side, state):
    """paints the lit buttons on a panel (16-pixel squares from the office at a third)"""
    r, g, b = a[..., 0], a[..., 1], a[..., 2]
    red = (r > 90) & (r > 2.2 * g) & (r > 2.2 * b)
    ys, xs = np.nonzero(red)
    if not len(ys):
        return
    top, bottom = ys.min(), ys.max()
    h = bottom - top + 1
    if state & 1:   # the door's button goes green
        lum = r[red]
        a[red, 0] = lum * 0.25
        a[red, 1] = np.minimum(255, lum * 1.05 + 20)
        a[red, 2] = lum * 0.25
    if state & 2:   # the light's button, just under it, lights up
        x0, x1 = xs.min(), xs.max() + 1
        ly0, ly1 = bottom + 1 + max(1, h // 6), min(a.shape[0], bottom + 1 + h + h // 3)
        box = a[ly0:ly1, x0:x1]
        lum = box.mean(axis=2, keepdims=True)
        lit = lum > 40
        box[:] = np.where(lit, np.minimum(255, lum * 1.9 + 60), box)


def fan_frames(office):
    """the desk fan's blades, turning: the inside of its grille blurred around its centre at three angles"""
    cx, cy, rad = 285.0, 124.0, 17.0
    mx0, my0, mx1, my1 = int((cx - rad) // 16), int((cy - rad) // 16), int((cx + rad) // 16) + 1, int((cy + rad) // 16) + 1
    region = pad16(office)[my0 * 16:my1 * 16, mx0 * 16:mx1 * 16]
    img = Image.fromarray(region.astype(np.uint8))
    ccx, ccy = cx - mx0 * 16, cy - my0 * 16
    yy, xx = np.mgrid[0:region.shape[0], 0:region.shape[1]]
    rr = np.hypot(xx - ccx, yy - ccy)
    inside = (rr < rad * 0.86)[..., None]
    for k in range(3):
        acc = np.zeros_like(region)
        n = 8
        for i in range(n):
            ang = k * 40 + i * 60 / n
            acc += np.asarray(img.rotate(ang, resample=Image.BILINEAR, center=(ccx, ccy))).astype(np.float64)
        pic(f"FAN{k}", np.where(inside, acc / n, region))
    define("FAN_X", mx0 * 16)
    define("FAN_Y", my0 * 16)


CAMS = {   # the camera's pictures: who is there (b Bonnie, c Chica, f Freddy) -> the file
    "1A": [("", "Stage"), ("f", "Stage-f"), ("bf", "Stage-b-f"), ("cf", "Stage-c-f"), ("bcf", "Stage-b-c-f")],
    "1B": [("", "DinningArea"), ("b", "DinningArea-b"), ("c", "DinningArea-c"), ("f", "DinningArea-f"),
           ("bc", "DinningArea-b-c"), ("bf", "DinningArea-b-f"), ("cf", "DinningArea-c-f"),
           ("bcf", "DinningArea-b-c-f")],
    "2A": [("", "West_Hall"), ("b", "West_Hall-b")],
    "2B": [("", "WHallCorner"), ("b", "WHallCorner-b")],
    "3": [("", "SupplyRoom"), ("b", "SupplyRoom-b")],
    "4A": [("", "East_Hall"), ("c", "East_Hall-c"), ("f", "East_Hall-f"), ("cf", "East_Hall-c-f")],
    "4B": [("", "EHallCorner"), ("c", "EHallCorner-c"), ("f", "EHallCorner-f")],
    "5": [("", "Backstage"), ("b", "Backstage-b")],
    "7": [("", "Restrooms"), ("c", "Restrooms-c"), ("f", "Restrooms-f"), ("cf", "Restrooms-c-f")],
    "1C": [("0", "Pirate_Cove"), ("1", "Pirate_Cove-1"), ("2", "Pirate_Cove-2"), ("3", "Pirate_Cove-3")],
}


def camera_pictures():
    for cam, states in CAMS.items():
        first = None
        for who, f in states:
            a = third(rgb(web(f"Cams/{f}.webp")))
            name = f"CAM{cam}_{who.upper() or 'EMPTY'}"
            if first is None:
                first = pic(name, a)
            else:
                pic(name, a, base=first)
    # 2A's hall is dark but when its light flickers on: the same hall, its lamp off
    hall = third(rgb(web("Cams/West_Hall.webp")))
    pic("CAM2A_DARK", dark_hall(hall), base=PICS[[p.name for p in PICS].index("CAM2A_EMPTY")])
    # 2B's poster, Golden Freddy's face for a while (the easter egg)
    corner = third(rgb(web("Cams/WHallCorner.webp")))
    gold = golden_poster(corner, rgb(web("golden_freddy.webp")))
    pic("CAM2B_GOLD", gold, base=PICS[[p.name for p in PICS].index("CAM2B_EMPTY")])


def dark_hall(a):
    """West Hall with its light off: dim, and the far end (lit by the lamp) dark"""
    h, w = a.shape[:2]
    yy, xx = np.mgrid[0:h, 0:w]
    far = np.exp(-(((xx - 0.71 * w) / (0.09 * w)) ** 2 + ((yy - 0.42 * h) / (0.5 * h)) ** 2))
    return a * (0.42 - 0.3 * far)[..., None]


def golden_poster(corner, face):
    """2B's poster (its corners at a third: on the wall, in perspective) with Golden Freddy's face in it"""
    quad = [(162, 8), (243, 6), (252, 98), (189, 125)]   # top left, top right, bottom right, bottom left
    fw, fh = face.size
    src = [(fw * 0.1, 0), (fw * 0.9, 0), (fw * 0.9, fh), (fw * 0.1, fh)]
    # the perspective from the wall to the face: 8 unknowns from the 4 corners
    m, v = [], []
    for (x, y), (u, w) in zip(quad, src):
        m.append([x, y, 1, 0, 0, 0, -u * x, -u * y])
        m.append([0, 0, 0, x, y, 1, -w * x, -w * y])
        v += [u, w]
    coef = np.linalg.solve(np.array(m, dtype=np.float64), np.array(v, dtype=np.float64))
    warped = np.asarray(face.transform((OW, OH), Image.PERSPECTIVE, tuple(coef), Image.BICUBIC)).astype(np.float64)
    mask = Image.new("L", (OW, OH))
    ImageDraw.Draw(mask).polygon(quad, fill=255)
    mask = np.asarray(mask).astype(np.float64)[..., None] / 255
    region = corner[mask[..., 0] > 0.5]
    light = region.mean() / 255
    return corner * (1 - mask) + warped * (0.1 + light * 0.95) * mask


def scare(name, path, frames, x0, size=(OW, OH), step=1):
    """a jumpscare's frames, 320 pixels wide from x0 (office coordinates at a third)"""
    im = Image.open(path)
    n = im.n_frames
    out = []
    prev = None
    first = None
    for i in range(0, min(n, frames), step):
        im.seek(i)
        a = np.asarray(im.convert("RGB").resize(size, Image.LANCZOS)).astype(np.float64)[:, x0:x0 + 320]
        p = pic(f"{name}{len(out)}", a, "scare", base=first, refs=[prev] if prev else [])
        first = first or p
        prev = p
        out.append(p)
    define(f"{name}_N", len(out))
    define(f"{name}_X", x0)
    return out


def scare_pictures():
    scare("BONNIE", web("Bonnie-Jumpscare.webp"), 10, 106)
    scare("CHICA", web("Chica-Jumpscare.webp"), 15, 95)
    scare("FREDDY", web("Freddy-Jumpscare.webp"), 20, 180)
    scare("FOXY", web("Foxy-Jumpscare.gif"), 19, 0)
    scare("FOXYRUN", web("Foxy-Hallway.webp"), 18, 213)
    scare("POWEROUT", web("Freddy-Jumpscare1.gif"), 21, 53, size=(427, 240))
    scare("FLIP", web("Up.webp"), 11, 53, size=(427, 240))   # the monitor coming up
    g = rgb(web("golden_freddy.webp")).resize((427, 240), Image.LANCZOS)
    pic("GOLDEN", np.asarray(g).astype(np.float64)[:, 53:373], "scare")


def menu_pictures():
    # the title's four faces of Freddy (the atlas has two of them turned)
    atlas = Image.open(hx("mainmenu/freddy.png")).convert("RGBA")
    first = None
    for i, (x, y, rot) in enumerate([(2, 2, False), (1286, 2, True), (2, 726, True), (726, 1286, False)]):
        w, h = (720, 1280) if rot else (1280, 720)
        f = atlas.crop((x, y, x + w, y + h))
        if rot:
            f = f.transpose(Image.Transpose.ROTATE_90)
        bg = Image.new("RGB", f.size)
        bg.paste(f, (0, 0), f)
        a = np.asarray(bg.resize((427, 240), Image.LANCZOS)).astype(np.float64)[:, 107:427]
        p = pic(f"TITLE{i}", a, "face", base=first)
        first = first or p
    news = Image.open(hx("mainmenu/newspaper.png")).convert("RGB").resize((427, 240), Image.LANCZOS)
    pic("NEWSPAPER", np.asarray(news).astype(np.float64)[:, 53:373], "paper")
    for n in ("freddy", "bonnie", "chica", "foxy"):
        f = Image.open(web(f"CustomNight/{n}.png")).convert("RGBA")
        bg = Image.new("RGB", f.size)
        bg.paste(f, (0, 0), f)
        pic(f"FACE_{n.upper()}", np.asarray(bg.resize((64, 64), Image.LANCZOS)).astype(np.float64), "face")
    for name, draw in (("PAYCHECK", paycheck), ("OVERTIME", overtime), ("PINKSLIP", pink_slip)):
        pic(name, np.asarray(draw()).astype(np.float64), "paper")
    # the game over: Backstage, where the empty suits are, darker
    back = third(rgb(web("Cams/Backstage.webp")))[:, 106:426] * 0.7
    pic("GAMEOVER", back, "scene")


def paper(color):
    im = Image.new("RGB", (320, 240), (12, 12, 12))
    d = ImageDraw.Draw(im)
    rng = np.random.default_rng(7)
    d.rectangle((22, 30, 297, 210), fill=color)
    a = np.asarray(im).astype(np.float64)
    grain = rng.normal(0, 5, a.shape[:2])[..., None]
    a[30:211, 22:298] += grain[30:211, 22:298]
    return Image.fromarray(np.clip(a, 0, 255).astype(np.uint8))


def text(d, xy, s, size, bold=False, serif=False, fill=(30, 30, 30), anchor="la"):
    name = ("DejaVuSerif" if serif else "DejaVuSans") + ("-Bold" if bold else "") + ".ttf"
    d.text(xy, s, font=ImageFont.truetype(find_font(name), size), fill=fill, anchor=anchor)


def cheque(amount, words, memo):
    im = paper((222, 228, 214))
    d = ImageDraw.Draw(im)
    d.rectangle((26, 34, 293, 206), outline=(120, 140, 120))
    text(d, (34, 42), "Fazbear Entertainment, Inc.", 11, bold=True, serif=True)
    text(d, (34, 56), "Freddy Fazbear's Pizza", 9, serif=True, fill=(70, 70, 70))
    text(d, (286, 42), "No. 0451", 9, fill=(90, 90, 90), anchor="ra")
    text(d, (34, 88), "PAY TO THE", 8, fill=(80, 80, 80))
    text(d, (34, 98), "ORDER OF", 8, fill=(80, 80, 80))
    text(d, (92, 92), "Mike Schmidt", 14, serif=True, fill=(20, 30, 90))
    d.line((90, 110, 220, 110), fill=(80, 80, 80))
    d.rectangle((228, 86, 290, 106), outline=(80, 80, 80))
    text(d, (286, 96), amount, 13, bold=True, fill=(20, 30, 90), anchor="rm")
    text(d, (34, 124), words, 10, serif=True, fill=(20, 30, 90))
    d.line((34, 138, 286, 138), fill=(80, 80, 80))
    text(d, (34, 172), "MEMO", 8, fill=(80, 80, 80))
    text(d, (64, 168), memo, 11, serif=True, fill=(20, 30, 90))
    d.line((62, 184, 150, 184), fill=(80, 80, 80))
    d.line((180, 184, 286, 184), fill=(80, 80, 80))
    text(d, (233, 188), "Fazbear Entertainment", 7, fill=(80, 80, 80), anchor="ma")
    return im


def paycheck():
    return cheque("$120.50", "One hundred twenty and 50/100 dollars", "One week's work")


def overtime():
    return cheque("$0.50", "Zero and 50/100 dollars", "Overtime")


def pink_slip():
    im = paper((236, 178, 190))
    d = ImageDraw.Draw(im)
    text(d, (160, 40), "NOTICE OF TERMINATION", 13, bold=True, serif=True, anchor="ma")
    text(d, (160, 58), "Freddy Fazbear's Pizza", 9, serif=True, fill=(80, 40, 50), anchor="ma")
    d.line((34, 74, 286, 74), fill=(120, 60, 70))
    rows = [("Employee:", "Mike Schmidt"), ("Position:", "Night security"), ("Reason:", "Tampering with the"),
            ("", "animatronics, general"), ("", "unprofessionalism, odor.")]
    y = 86
    for k, v in rows:
        text(d, (36, y), k, 10, bold=True, fill=(60, 20, 30))
        text(d, (110, y), v, 11, serif=True, fill=(30, 20, 20))
        y += 18
    text(d, (160, 186), "Please return your uniform.", 9, serif=True, fill=(80, 40, 50), anchor="ma")
    return im


# ------------------------------------------------------------------ the map, the title's words, the star
def camera_map():
    im = np.asarray(Image.open(web("Cams/Complete_Map.png")).convert("RGBA")).astype(np.int32)
    a = im[..., 3]
    grey = (a > 128) & (im[..., 0] < 140) & (im[..., 0] > 40)
    # the CAM labels: grey boxes; where each is (the buttons go there), and none of them in the walls
    seen = np.zeros_like(grey)
    boxes = []
    H, W = grey.shape
    for y in range(H):
        for x in range(W):
            if grey[y, x] and not seen[y, x]:
                stack, pts = [(y, x)], []
                seen[y, x] = True
                while stack:
                    cy, cx = stack.pop()
                    pts.append((cy, cx))
                    for dy, dx in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                        ny, nx = cy + dy, cx + dx
                        if 0 <= ny < H and 0 <= nx < W and grey[ny, nx] and not seen[ny, nx]:
                            seen[ny, nx] = True
                            stack.append((ny, nx))
                if len(pts) > 150:
                    ys, xs = zip(*pts)
                    boxes.append((min(xs), min(ys), max(xs), max(ys)))
    walls = (a > 100) & (im[..., 0] > 200)
    for x0, y0, x1, y1 in boxes:
        walls[max(0, y0 - 3):y1 + 4, max(0, x0 - 3):x1 + 4] = False
    expect = {"1A": (135, 38), "1B": (122, 100), "1C": (75, 178), "2A": (133, 300), "2B": (133, 338),
              "3": (60, 268), "4A": (225, 300), "4B": (225, 338), "5": (25, 128), "6": (345, 250), "7": (345, 112)}
    s = 1 / 3
    size = int(400 * s)
    small = Image.fromarray((walls * 255).astype(np.uint8)).resize((size, size), Image.BOX)
    bits = np.asarray(small) > 70
    out = struct.pack("<HH", size, size)
    stride = (size + 7) // 8
    for r in range(size):
        row = bytearray(stride)
        for c in range(size):
            if bits[r, c]:
                row[c >> 3] |= 1 << (c & 7)
        out += row
    centres = {}
    for cam, (ex, ey) in expect.items():
        best = min(boxes, key=lambda b: ((b[0] + b[2]) / 2 - ex) ** 2 + ((b[1] + b[3]) / 2 - ey) ** 2)
        centres[cam] = (int(round((best[0] + best[2]) / 2 * s)), int(round((best[1] + best[3]) / 2 * s)))
    for cam in ("1A", "1B", "1C", "2A", "2B", "3", "4A", "4B", "5", "6", "7"):
        out += struct.pack("<HH", *centres[cam])
    define("MAP_SIZE", size)
    return out


IMGS = []


def img(name, alpha, grey=None):
    """a small picture with alpha (and a grey level, when not white)"""
    a = to4(alpha)
    h, w = a.shape
    data = struct.pack("<HHBBH", w, h, 1 if grey is not None else 0, 0, 0) + nibbles(a)
    if grey is not None:
        data += nibbles(to4(grey))
    define(f"IMG_{name}", len(IMGS))
    IMGS.append(data)


def small_pictures():
    t = Image.open(hx("mainmenu/texts/FIVE_GAME_TITLE.png")).convert("RGBA")
    t = t.resize((int(t.width * 0.5), int(t.height * 0.5)), Image.LANCZOS)
    a = np.asarray(t).astype(np.float64)
    img("TITLE", a[..., 3] * a[..., :3].mean(axis=2) / 255)
    s = Image.open(hx("mainmenu/STAR.png")).convert("RGBA").resize((22, 21), Image.LANCZOS)
    a = np.asarray(s).astype(np.float64)
    img("STAR", a[..., 3] * a[..., :3].mean(axis=2) / 255)


def app_icon():
    """src/icon.png: Freddy's face as it comes out of the dark, with NumPlay's rounded corners"""
    sys.path.insert(0, os.path.join(ROOT, "tools"))
    from make_icon import corner_mask
    im = Image.open(web("Freddy-Jumpscare1.gif"))
    im.seek(7)
    f = im.convert("RGB")
    w, h = f.size
    s = 0.95 * h
    face = f.crop((int(w / 2 - s / 2), int(0.40 * h - s / 2), int(w / 2 + s / 2), int(0.40 * h + s / 2)))
    face = face.resize((55, 56), Image.LANCZOS).point(lambda v: min(255, int(v * 1.15)))
    face.putalpha(corner_mask(55, 56))
    face.save(os.path.join(HERE, "..", "src", "icon.png"), optimize=True)


def pack_imgs():
    out = struct.pack("<I", len(IMGS))
    p = 4 + 4 * len(IMGS)
    for d in IMGS:
        out += struct.pack("<I", p)
        p += len(d) + (-len(d) % 4)
    return out + b"".join(d + bytes(-len(d) % 4) for d in IMGS)


def main():
    global REF
    if len(sys.argv) != 2:
        raise SystemExit(__doc__)
    REF = sys.argv[1]
    office_pictures()
    camera_pictures()
    scare_pictures()
    menu_pictures()
    huff, quant, pics, table, bank = encode_all()
    fonts = [
        ttf_font(find_font("DejaVuSansMono-Bold.ttf"), 11),      # FONT_HUD
        ttf_font(find_font("DejaVuSansMono.ttf"), 17),           # FONT_BIG
        ttf_font(find_font("DejaVuSans.ttf"), 11),               # FONT_TEXT
        fnt_font(hx("numbers/NUMBER.fnt"), hx("numbers/NUMBER.png"), 0.36),          # FONT_CLOCK
        fnt_font(hx("numbers/PIXEL_NUMBER.fnt"), hx("numbers/PIXEL_NUMBER.png"), 0.36),  # FONT_PIXEL
        ttf_font(find_font("DejaVuSansMono.ttf"), 24),           # FONT_HUGE
    ]
    small_pictures()
    app_icon()
    sections = [huff, quant, pics, table, bank, pack_fonts(fonts), pack_imgs(), camera_map()]
    head = b"FNF1" + struct.pack("<I", len(sections))
    offs, p = [], len(head) + 4 * len(sections)
    for s in sections:
        offs.append(p)
        p += len(s) + (-len(s) % 4)
    out = head + b"".join(struct.pack("<I", o) for o in offs)
    for s in sections:
        out += s + bytes(-len(s) % 4)
    open(OUT_BIN, "wb").write(out)
    h = ["/* Made by tools/pack.py: what is in data.bin. */", "#ifndef FNAF_DATA_H", "#define FNAF_DATA_H", ""]
    h += [f"#define SEC_{n} {i}" for i, n in enumerate(SECTIONS)]
    h += [f"#define PIC_{p.name} {i}" for i, p in enumerate(PICS)]
    h += [f"#define PIC_COUNT {len(PICS)}"]
    h += [f"#define FONT_{n} {i}" for i, n in enumerate(["HUD", "BIG", "TEXT", "CLOCK", "PIXEL", "HUGE"])]
    h += [f"#define {n} {v}" for n, v in DEFINES]
    h += ["", "#endif", ""]
    open(OUT_H, "w").write("\n".join(h))
    print(f"data.bin: {len(out)} bytes")


if __name__ == "__main__":
    main()
