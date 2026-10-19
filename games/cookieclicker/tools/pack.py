#!/usr/bin/env python3
"""Cookie Clicker's numbers, texts, pictures and fonts -> src/data.bin + src/data.h.

    node tools/extract.js REF build/game.json
    python3 -I tools/pack.py REF build/game.json      (REF: the folder tools/fetch.py filled)

The game's left column (about 384 pixels wide in a browser) is drawn at 0.37 of its size, its store at about half.

data.bin, read where it lies in flash (offsets from its start; data.h names them):
- IMAGES: 16 bytes each (IM_*): w, h (u16), format (u8: 0 RGB565, 1 a byte a pixel, 2 half a byte a pixel, the
  first pixel lowest), colours - 1 (u8), 0 (u16), the pixels' offset, the palette's (u32): its colours (RGB565),
  then their opacities (u8 each)
- ICONS: the game's icon sheet (icons.png, 36 x 37 icons of 48 pixels) at 24 pixels: an image index (u16, 0xFFFF
  none) for each icon, row by row
- FONTS: the offset (u32) of each (F_*): first character, count, ascent, line height (u8 x 4), then 8 bytes a glyph
  (w, h, x, y from the baseline, advance (u8, u8, s8, s8, u8), 0, its data's offset from the glyphs' end (u16)):
  4-bit coverage, 2 pixels a byte, rows padded to a byte
- TEXT: the texts, one byte a character (ASCII, and the few others they use from 0x80: see CHARS), each ending
  with 0. In them: 0x01 bold, 0x02 back to plain, 0x03 italic, 0x04 back, 0x05 a line across, \\n a new line,
  0x06 nothing (it keeps two numbers apart when the game rewrites numbers: src/text.c)
- the tables of the game's buildings, upgrades, achievements and tiers (their C structures are in data.h)
"""
import html
import json
import math
import os
import re
import struct
import sys

import numpy as np
from PIL import Image, ImageFont

HERE = os.path.dirname(os.path.abspath(__file__))
OUT_BIN = os.path.join(HERE, "..", "src", "data.bin")
OUT_H = os.path.join(HERE, "..", "src", "data.h")
S = 0.37            # the left column's pictures (the big cookie: 256 -> 95 pixels)

# the characters past ASCII the texts and fonts have, from 0x80 on
CHARS = "éèêëàâäïîôöûüùçñÉÀ×÷°•…’‘“”–—½¼¾²³™©®♥"


# ------------------------------------------------------------------ pictures
def rgb565(r, g, b):
    return (int(r) >> 3) << 11 | (int(g) >> 2) << 5 | int(b) >> 3


def premul_resize(img, size, box=None):
    """an RGBA picture resized without dark fringes (its colours weighted by their opacity)"""
    img = img.convert("RGBA")
    if box:
        img = img.crop(box)
    a = np.asarray(img).astype(np.float32) / 255
    a[..., :3] *= a[..., 3:4]
    pm = Image.fromarray((a * 255 + 0.5).astype(np.uint8), "RGBA").resize(size, Image.LANCZOS)
    b = np.asarray(pm).astype(np.float32) / 255
    al = b[..., 3:4]
    b[..., :3] = np.where(al > 0, b[..., :3] / np.maximum(al, 1e-6), 0)
    return Image.fromarray((np.clip(b, 0, 1) * 255 + 0.5).astype(np.uint8), "RGBA")


def quantize(img, n):
    """palette (RGBA tuples) and indices, at most n colours; the fully clear pixels all at index 0"""
    arr = np.asarray(img.convert("RGBA"))
    clear = arr[..., 3] < 8
    if clear.all():
        return [(0, 0, 0, 0)], np.zeros(arr.shape[:2], np.uint8)
    q = img.convert("RGBA").quantize(n - (1 if clear.any() else 0), method=Image.Quantize.FASTOCTREE)
    pal = q.getpalette("RGBA")
    if pal is None:  # (Pillow keeps the alpha in the palette of an RGBA quantize)
        raise SystemExit("pack: no RGBA palette")
    idx = np.asarray(q).astype(np.int32)
    used = sorted(set(idx[~clear].ravel().tolist()))
    remap = {}
    out_pal = [(0, 0, 0, 0)] if clear.any() else []
    for u in used:
        remap[u] = len(out_pal)
        out_pal.append(tuple(pal[4 * u:4 * u + 4]))
    out = np.zeros(idx.shape, np.uint8)
    for u, v in remap.items():
        out[idx == u] = v
    out[clear] = 0
    return out_pal, out


class Pack:
    def __init__(self):
        self.blob = bytearray(b"CCK1")
        self.images = []          # (name, w, h, fmt, ncolors, px_off, pal_off)
        self.names = {}
        self.defines = []
        self.sizes = {}           # bytes by group of pictures, for the report

    def align(self, n=4):
        while len(self.blob) % n:
            self.blob.append(0)

    def put(self, data, align=4):
        self.align(align)
        off = len(self.blob)
        self.blob += data
        return off

    def image(self, name, img, fmt="p8", ncolors=256):
        """adds a picture; returns its index"""
        w, h = img.size
        start = len(self.blob)
        if fmt == "rgb":
            arr = np.asarray(img.convert("RGB")).astype(np.uint16)
            px = ((arr[..., 0] >> 3) << 11 | (arr[..., 1] >> 2) << 5 | arr[..., 2] >> 3).astype("<u2")
            off = self.put(px.tobytes())
            rec = (name, w, h, 0, 1, off, 0)
        else:
            n = 16 if fmt == "p4" else ncolors
            pal, idx = quantize(img, n)
            pal_bytes = b"".join(struct.pack("<H", rgb565(*c[:3])) for c in pal) + bytes(c[3] for c in pal)
            pal_off = self.put(pal_bytes)
            if fmt == "p4":
                if w % 2:
                    idx = np.concatenate([idx, np.zeros((h, 1), np.uint8)], 1)
                packed = (idx[:, 0::2] | idx[:, 1::2] << 4).astype(np.uint8)
                off = self.put(packed.tobytes())
                rec = (name, w, h, 2, len(pal), off, pal_off)
            else:
                off = self.put(idx.astype(np.uint8).tobytes())
                rec = (name, w, h, 1, len(pal), off, pal_off)
        if name:
            if name in self.names:
                raise SystemExit(f"pack: two pictures named {name}")
            self.names[name] = len(self.images)
        group = re.sub(r"(_.*|\d+)$", "", name) if name else "ICONS"
        self.sizes[group] = self.sizes.get(group, 0) + len(self.blob) - start
        self.images.append(rec)
        return len(self.images) - 1


def load(ref, rel):
    return Image.open(os.path.join(ref, "img", rel)).convert("RGBA")


def scaled(ref, rel, size, box=None):
    return premul_resize(load(ref, rel), size, box)


def opaque(ref, rel, size, box=None):
    img = Image.open(os.path.join(ref, "img", rel)).convert("RGB")
    if box:
        img = img.crop(box)
    return img.resize(size, Image.LANCZOS)


# ------------------------------------------------------------------ fonts
def load_font(path, size, axes=None):
    f = ImageFont.truetype(path, size)
    if axes:
        f.set_variation_by_axes(axes)
    return f


def font_bytes(f, chars, line=None):
    """first character 32, all of CHARS mapped from 0x80 on"""
    asc, desc = f.getmetrics()
    glyphs, data = [], bytearray()
    for ch in chars:
        if ch is None:
            glyphs.append((0, 0, 0, 0, 0, len(data)))
            continue
        adv = int(round(f.getlength(ch)))
        mask, off = f.getmask2(ch, mode="L", anchor="ls")
        w, h = mask.size
        if w == 0 or h == 0:
            glyphs.append((0, 0, 0, 0, adv, len(data)))
            continue
        a = np.asarray(Image.frombytes("L", (w, h), bytes(mask))).astype(np.int32)
        a4 = (a * 15 + 127) // 255
        ys, xs = np.nonzero(a4)
        if len(xs) == 0:
            glyphs.append((0, 0, 0, 0, adv, len(data)))
            continue
        x0, x1, y0, y1 = xs.min(), xs.max() + 1, ys.min(), ys.max() + 1
        a4 = a4[y0:y1, x0:x1]
        gw, gh = x1 - x0, y1 - y0
        if gw % 2:
            a4 = np.concatenate([a4, np.zeros((gh, 1), np.int32)], 1)
        packed = (a4[:, 0::2] | a4[:, 1::2] << 4).astype(np.uint8)
        glyphs.append((gw, gh, off[0] + x0, off[1] + y0, adv, len(data)))
        data += packed.tobytes()
        if len(data) > 0xFFFF:
            raise SystemExit("pack: a font's glyphs past 64 KB")
    out = bytearray(struct.pack("<BBBB", 32, len(chars), asc, line or (asc + desc)))
    for gw, gh, gx, gy, adv, o in glyphs:
        out += struct.pack("<BBbbBBH", gw, gh, gx, gy, adv, 0, o)
    return out + data


FONT_CHARS = [chr(c) for c in range(32, 127)] + [None] + list(CHARS)


def char_code(c):
    if 32 <= ord(c) < 127 or c == "\n":
        return ord(c)
    i = CHARS.find(c)
    return 0x80 + i if i >= 0 else None


# ------------------------------------------------------------------ texts
ENTITIES = {"&bull;": "•", "&nbsp;": " ", "&eacute;": "é", "&deg;": "°", "&reg;": "®", "&agrave;": "à"}
UNKNOWN = {}


def beautify_int(n):
    return f"{n:,}"


def beautify_in_text(s):
    # the game's BeautifyInText: every run of digits (and commas) written again with commas
    return re.sub(r"(([\d]+[,]*)+)", lambda m: beautify_int(int(m.group(0).replace(",", "") or 0))
                  if m.group(0).replace(",", "").isdigit() else m.group(0), s)


def clean(s, numbers=True):
    """the game's HTML as the calculator's markup (see the TEXT table)"""
    if s is None:
        return ""
    s = s.replace("\r", "").replace("\t", "")
    s = s.replace("<span></span>", "\x06")   # (keeps two numbers apart: see text.c)
    s = re.sub(r"<div class=\"line\"></div>", "\x05", s)
    s = re.sub(r"<br\s*/?>", "\n", s)
    s = re.sub(r"</?(b|strong)>", lambda m: "\x02" if m.group(0).startswith("</") else "\x01", s)
    s = re.sub(r"</?(i|q|em)>", lambda m: "\x04" if m.group(0).startswith("</") else "\x03", s)
    s = re.sub(r"<div[^>]*>", "\n", s)
    s = re.sub(r"</div>", "\n", s)
    s = re.sub(r"<[^>]*>", "", s)
    for k, v in ENTITIES.items():
        s = s.replace(k, v)
    s = html.unescape(s)
    out = []
    for c in s:
        if c in "\x01\x02\x03\x04\x05\x06":
            out.append(c)
            continue
        if unicodedata_combining(c):
            continue
        code = char_code(c)
        if code is None:
            UNKNOWN[c] = UNKNOWN.get(c, 0) + 1
            out.append("?")
        else:
            out.append(chr(code))
    s = "".join(out)
    s = re.sub(r"\n{2,}", "\n", s).strip("\n")
    return s


def unicodedata_combining(c):
    import unicodedata
    return unicodedata.combining(c) != 0


def split_quote(s):
    """a description and its quote (the game's <q>, said in italics)"""
    if s is None:
        return "", ""
    m = re.search(r"<q>(.*)</q>\s*$", s, re.S)
    if m:
        return s[:m.start()], m.group(1)
    return s, ""


class Texts:
    def __init__(self):
        self.blob = bytearray()
        self.index = {}

    def add(self, s):
        if s in self.index:
            return self.index[s]
        off = len(self.blob)
        self.blob += s.encode("latin-1") + b"\0"
        self.index[s] = off
        return off


# ------------------------------------------------------------------ the game's tables
POOLS = {"": 0, "cookie": 1, "prestige": 2, "toggle": 3, "tech": 4, "debug": 5}
ACH_POOLS = {"normal": 0, "shadow": 1, "dungeon": 2}
SEASONS = {None: 0, "christmas": 1, "halloween": 2, "valentines": 3, "easter": 4, "fools": 5}


def cname(s):
    s = s.upper().replace("'", "").replace("&", "AND")
    s = re.sub(r"[^A-Z0-9]+", "_", s).strip("_")
    if s and s[0].isdigit():
        s = "_" + s
    return s


ROW_FRAMES = {"javascriptconsole": 2, "idleverse": 4, "cortex": 4}


def main(ref, game_json):
    G = json.load(open(game_json))
    P = Pack()
    T = Texts()
    T.add("")
    defs = []

    # ---- the left column
    L = lambda name, rel, size, fmt="p8", box=None, n=256: P.image(name, scaled(ref, rel, size, box), fmt, n)
    L("COOKIE", "perfectCookie.png", (104, 104))
    L("COOKIE_SHADOW", "cookieShadow.png", (40, 40))
    L("FLARE", "flare.png", (112, 112))
    L("FLARE_GOLD", "flareGold.png", (112, 112))
    L("FLARE_RED", "flareSuckRed.png", (112, 112))
    L("GLINT", "glint.png", (16, 16))
    tile = round(512 * S)
    P.image("BG_BLUE", opaque(ref, "bgBlue.jpg", (tile, tile)), "p8")
    for i in (1, 2, 3):
        P.image(f"BG_GRANDMAS{i}", opaque(ref, f"grandmas{i}.jpg", (128, 128)), "p8")   # (drawn at tile)
    for k, f in (("SOFT", "shadedBordersSoft.png"), ("GOLD", "shadedBordersGold.png"), ("RED", "shadedBordersRed.png")):
        L("BORDER_" + k, f, (64, 64))
    for i in (1, 2, 3):
        L(f"SHOWER{i}", f"cookieShower{i}.png", (tile, tile))
    for i in range(8):
        L(f"SMALL_COOKIE{i}", "smallCookies.png", (24, 24), box=(64 * i, 0, 64 * i + 64, 64))
    L("CURSOR", "cursor.png", (32, 32), box=(0, 0, 32, 32))
    L("GOLD_COOKIE", "goldCookie.png", (48, 48))
    L("WRATH_COOKIE", "wrathCookie.png", (48, 48))
    # the milks: the game draws its 240-pixel pictures at 480; here at 120, drawn at 178 (480 x S): the top of
    # them that the milk shows, at most 0.35 of the column
    milk_tile = round(480 * S)
    milk_w, milk_h = 120, math.ceil((0.35 * 220 + 2) * 120 / milk_tile)
    defs.append(f"#define MILK_TILE {milk_tile}")
    for m in G["milks"]:
        rows = round(milk_h / milk_w * 240)
        L("MILK_" + cname(m["name"].replace(" milk", "")), m["pic"], (milk_w, milk_h), box=(0, 0, 240, rows))
    L("WRINKLER", "wrinkler.png", (38, 76))
    L("SHINY_WRINKLER", "shinyWrinkler.png", (38, 76))
    L("SHIMMERING_VEIL", "shimmeringVeil.png", (64, 64))
    L("DRAGON_BG", "dragonBG2.png", (112, 112))
    for i in range(10):
        L(f"BROKEN_COOKIE{i}", "brokenCookie.png", (64, 64), box=(256 * i, 0, 256 * i + 256, 256))
    L("BROKEN_HALO", "brokenCookieHalo.png", (72, 72))
    L("SHINE_SPOKE", "shineSpoke.png", (48, 48))
    P.image("HEAVEN_RING1", opaque(ref, "heavenRing1.jpg", (64, 64)), "p8")
    P.image("HEAVEN_RING2", opaque(ref, "heavenRing2.jpg", (64, 64)), "p8")
    P.image("STARS", opaque(ref, "starbg.jpg", (192, 192)), "p8")

    # ---- the store
    for i in range(4):
        P.image(f"STORE_TILE{i}", opaque(ref, "storeTile.jpg", (174, 32), (0, 64 * i, 300, 64 * i + 64)), "rgb")
    L("UPGRADE_FRAME", "upgradeFrame.png", (87, 58))
    L("MONEY", "money.png", (10, 10))
    L("HEAVENLY_MONEY", "heavenlyMoney.png", (10, 10))
    L("LUMP_MONEY", "sugarLump.png", (10, 10))
    P.image("PANEL_V", opaque(ref, "panelVertical.png", (4, 128), (0, 0, 16, 512)), "rgb")
    P.image("PANEL_H", opaque(ref, "panelHorizontal.png", (128, 4), (0, 0, 512, 16)), "rgb")
    P.image("DARK_NOISE", opaque(ref, "darkNoise.jpg", (128, 128)), "p8")
    for b in G["objects"]:
        r = b["icon"]
        for col, tag in ((0, ""), (1, "_OFF")):
            L(f"BUILD_{cname(b['name'])}{tag}", "buildings.png", (32, 32), box=(64 * col, 64 * r, 64 * col + 64, 64 * r + 64))

    # ---- the buildings' rows (the middle column), at 0.375: rows 48 pixels high
    pics = ["grandma", "farm", "mine", "factory", "bank", "temple", "wizardtower", "shipment", "alchemylab", "portal",
            "timemachine", "antimattercondenser", "prism", "chancemaker", "fractalEngine", "javascriptconsole",
            "idleverse", "cortex", "you"]
    for p in pics:
        if p != "you":
            L("ROW_BG_" + cname(p), p + "Background.png", (48, 48))
        else:
            L("ROW_BG_YOU", "youBackground.png", (48, 48))
        src = load(ref, p + ".png")
        frames = ROW_FRAMES.get(p, 1)
        if frames == 1:
            L("ROW_" + cname(p), p + ".png", (24, 24))
        else:   # (slices side by side, as the game cuts them)
            fw, fh = src.size[0] // frames, src.size[1]
            for k in range(frames):
                L(f"ROW_{cname(p)}{k}", p + ".png", (max(1, round(fw * 0.375)), round(fh * 0.375)),
                  box=(fw * k, 0, fw * k + fw, fh))
    grandmas = ["farmerGrandma", "workerGrandma", "minerGrandma", "cosmicGrandma", "transmutedGrandma",
                "alteredGrandma", "grandmasGrandma", "antiGrandma", "rainbowGrandma", "bankGrandma", "templeGrandma",
                "witchGrandma", "luckyGrandma", "metaGrandma", "scriptGrandma", "alternateGrandma", "brainyGrandma",
                "cloneGrandma", "elfGrandma", "bunnyGrandma"]
    for g in grandmas:
        L("ROW_" + cname(g), g + ".png", (24, 24))
    for k, f in (("GRIMOIRE", "BGgrimoire.jpg"), ("PANTHEON", "BGpantheon.jpg"), ("GARDEN", "BGgarden.jpg"),
                 ("MARKET", "BGmarket.jpg")):
        P.image("MG_BG_" + k, opaque(ref, f, (192, 48)), "p8")
    for i in range(9):
        L(f"DRAGON{i}", "dragon.png", (48, 48), box=(96 * i, 0, 96 * i + 96, 96))
    for i in range(15):
        L(f"SANTA{i}", "santa.png", (48, 48), box=(96 * i, 0, 96 * i + 96, 96))
    L("SPELL_BG", "spellBG.png", (120, 111))
    for i in range(3):   # (the heavenly upgrades' slots: bought, chosen, not bought)
        L(f"ASCEND_SLOT{i}", "ascendSlot.png", (40, 40), box=(88 * i, 0, 88 * i + 88, 88))

    # ---- the seasons' shimmers: reindeer, and golden cookies as wreaths, hearts, eggs, familiars
    L("REINDEER", "frostedReindeer.png", (62, 78))
    L("GOLD_WREATH", "goldCookieWreath.png", (48, 48))
    L("WRATH_WREATH", "wrathCookieWreath.png", (48, 48))
    for name, f, n in (("HEART", "hearts.png", 8), ("BUNNY", "bunnies.png", 4), ("FAMILIAR", "familiars.png", 4)):
        for row in range(2):
            for k in range(n):
                L(f"{name}{row}_{k}", f, (48, 48), box=(96 * k, 96 * row, 96 * k + 96, 96 * row + 96))

    # ---- the icons: 36 x 37 of 48 pixels, at 24
    sheet = load(ref, "icons.png")
    cols, rows = sheet.size[0] // 48, sheet.size[1] // 48
    icon_index = []
    for y in range(rows):
        for x in range(cols):
            cell = sheet.crop((48 * x, 48 * y, 48 * x + 48, 48 * y + 48))
            if np.asarray(cell)[..., 3].max() < 8:
                icon_index.append(0xFFFF)
                continue
            icon_index.append(P.image(None, premul_resize(cell, (24, 24)), "p4"))
    icons_off = P.put(struct.pack(f"<{len(icon_index)}H", *icon_index))
    defs += [f"#define ICON_COLS {cols}", f"#define ICON_ROWS {rows}", f"#define OFF_ICONS {icons_off}"]

    # ---- fonts
    F = os.path.join(ref, "fonts")
    mw = os.path.join(F, "Merriweather.ttf")
    ns = os.path.join(F, "NotoSans.ttf")
    nsi = os.path.join(F, "NotoSans-Italic.ttf")
    fonts = [
        ("TITLE", load_font(mw, 13, [900, 100, 18]), None),
        ("NAME", load_font(mw, 11, [900, 100, 18]), None),
        ("NAME_SMALL", load_font(mw, 9, [900, 100, 18]), None),
        ("OWNED", load_font(mw, 19, [900, 100, 18]), None),
        ("BODY", load_font(ns, 10, [500, 100]), 12),
        ("BOLD", load_font(ns, 10, [800, 100]), 12),
        ("ITALIC", load_font(nsi, 10, [500, 100]), 12),
        ("SMALL", load_font(ns, 9, [700, 100]), 11),
        ("KAVOON", load_font(os.path.join(F, "Kavoon.ttf"), 14), None),
    ]
    font_offs = []
    for name, f, line in fonts:
        font_offs.append(P.put(font_bytes(f, FONT_CHARS, line)))
        defs.append(f"#define F_{name} {len(font_offs) - 1}")
    fonts_off = P.put(struct.pack(f"<{len(font_offs)}I", *font_offs))
    defs.append(f"#define OFF_FONTS {fonts_off}")

    # ---- the game: buildings, tiers, upgrades, achievements
    up_by_name = {u["name"]: u["id"] for u in G["upgrades"]}
    obj_by_name = {o["name"]: o["id"] for o in G["objects"]}
    tier_keys = list(G["tiers"].keys())

    def icon_of(ic):
        if not ic:
            return 0xFFFF
        return ic[1] * cols + ic[0]

    # buildings: 56 bytes each
    brec = bytearray()
    for o in G["objects"]:
        brec += struct.pack("<ddIIIIIIIIhhhH", o["basePrice"], o.get("baseCps") or 0.1, T.add(clean(o["name"], False)),
                            T.add(clean(o["single"], False)), T.add(clean(o["plural"], False)),
                            T.add(clean(o["actionName"], False)), T.add(clean(o["extraName"] or "", False)),
                            T.add(clean(o["extraPlural"] or "", False)), T.add(clean(o["desc"])),
                            0, o["icon"], o["iconColumn"], 0, 0)
    assert len(brec) == 56 * len(G["objects"])
    defs.append(f"#define OFF_BUILDINGS {P.put(bytes(brec), 8)}")
    defs.append(f"#define N_BUILDINGS {len(G['objects'])}")
    for o in G["objects"]:
        defs.append(f"#define B_{cname(o['name'])} {o['id']}")

    # tiers: 24 bytes each
    trec = bytearray()
    for k in tier_keys:
        t = G["tiers"][k]
        c = int(t["color"][1:], 16)
        trec += struct.pack("<dIhhhhHH", float(t["price"]), T.add(clean(t["name"], False)), t["unlock"],
                            t.get("achievUnlock", -1), t["iconRow"], 1 if t.get("special") else 0,
                            rgb565(c >> 16, c >> 8 & 255, c & 255),
                            up_by_name.get(t.get("req"), 0xFFFF) if t.get("req") else 0xFFFF)
    assert len(trec) == 24 * len(tier_keys)
    defs.append(f"#define OFF_TIERS {P.put(bytes(trec), 8)}")
    defs.append(f"#define N_TIERS {len(tier_keys)}")
    for i, k in enumerate(tier_keys):
        defs.append(f"#define TIER_{cname(k)} {i}")

    # upgrades: 56 bytes each
    urec = bytearray()
    for u in G["upgrades"]:
        desc, quote = split_quote(u["desc"])
        tier = tier_keys.index(str(u["tier"])) if str(u.get("tier", 0)) in tier_keys else 0xFF
        power = u["power"] if isinstance(u.get("power"), (int, float)) else 0
        ua = u.get("unlockAt") or {}
        req = up_by_name.get(ua.get("require"), 0xFFFF) if ua.get("require") else 0xFFFF
        parents = [up_by_name[p] for p in u.get("parents", [])][:6] + [0xFFFF] * 6
        urec += struct.pack("<dfIIIHBBbbBBfH6HhhHd", float(u["basePrice"] or 0), float(u["order"] or 0),
                            T.add(clean(u["name"], False)), T.add(clean(desc)), T.add(clean(quote)),
                            icon_of(u["icon"]), POOLS[u["pool"]], tier,
                            obj_by_name.get(u.get("buildingTie1") or u.get("buildingTie"), -1),
                            obj_by_name.get(u.get("buildingTie2"), -1), 1 if u.get("kitten") else 0,
                            SEASONS.get(u.get("season"), 0) | (0x80 if u.get("vanilla") else 0), float(power),
                            req, *parents[:6], int(u.get("posX") or 0), int(u.get("posY") or 0), 0,
                            float(ua.get("cookies") or 0))
    assert len(urec) == 64 * len(G["upgrades"]), len(urec) / len(G["upgrades"])
    defs.append(f"#define OFF_UPGRADES {P.put(bytes(urec), 8)}")
    defs.append(f"#define N_UPGRADES {len(G['upgrades'])}")
    seen = {}
    for u in G["upgrades"]:
        n = cname(u["name"])
        if n in seen:
            n = f"{n}_{u['id']}"
        seen[n] = 1
        defs.append(f"#define U_{n} {u['id']}")

    # achievements: 20 bytes each
    arec = bytearray()
    for a in G["achievements"]:
        desc, quote = split_quote(a["desc"])
        arec += struct.pack("<IIIfHBb", T.add(clean(a["name"], False)), T.add(clean(desc)), T.add(clean(quote)),
                            float(a["order"] or 0), icon_of(a["icon"]), ACH_POOLS[a["pool"]],
                            obj_by_name.get(a.get("buildingTie"), -1))
    assert len(arec) == 20 * len(G["achievements"])
    defs.append(f"#define OFF_ACHIEVS {P.put(bytes(arec), 4)}")
    defs.append(f"#define N_ACHIEVS {len(G['achievements'])}")
    seen = {}
    for a in G["achievements"]:
        n = cname(a["name"])
        if n in seen:
            n = f"{n}_{a['id']}"
        seen[n] = 1
        defs.append(f"#define A_{n} {a['id']}")

    # milks
    defs.append(f"#define N_MILKS {len(G['milks'])}")
    mrec = b"".join(struct.pack("<IH", T.add(clean(m["name"], False)), icon_of(m["icon"])) + b"\0\0"
                    for m in G["milks"])
    defs.append(f"#define OFF_MILKS {P.put(mrec)}")

    # ---- what ties them together (res.h's BuildingLinks; lists of ids; thresholds)
    L = G["links"]
    NONE = 0xFFFF
    u16 = lambda v: NONE if v is None or v < 0 else v
    lrec = bytearray()
    for o in L["objects"]:
        tiered = [u16(o["tieredUpgrades"].get(k, -1)) for k in tier_keys]
        tach = [u16(o["tieredAchievs"].get(k, -1)) for k in tier_keys]
        syn = (o["synergies"] + [-1] * 6)[:6]
        prod = (o["production"] + [[0, -1]] * 4)[:3]
        lrec += struct.pack(f"<{len(tier_keys)}H{len(tier_keys)}H6HHHHH4HHH3d", *tiered, *tach, *map(u16, syn),
                            u16(o["fortune"]), u16(o["grandma"]), u16(o["unshackle"]), u16(o["level10"]),
                            *[u16(p[1]) for p in prod], NONE, 0, 0, *[float(p[0]) for p in prod])
    assert len(lrec) == 128 * len(L["objects"]), len(lrec)
    defs.append(f"#define OFF_BUILDING_LINKS {P.put(bytes(lrec), 8)}")

    def id_list(name, ids):
        defs.append(f"#define OFF_{name} {P.put(struct.pack(f'<{len(ids)}H', *map(u16, ids)) if ids else b'')}")
        defs.append(f"#define N_{name} {len(ids)}")

    id_list("TIER_UNSHACKLE", [L["tierUnshackle"].get(k, -1) for k in tier_keys])
    for name, key in (("COOKIE_UPGRADES", "cookieUpgrades"), ("GRANDMA_SYNERGIES", "grandmaSynergies"),
                      ("KITTENS", "kittens"), ("GOLDEN_UPGRADES", "goldenCookieUpgrades"), ("LASTING", "lasting"),
                      ("NO_PERM", "noPerm"), ("HEAVENLY", "heavenly"), ("EASTER_EGGS", "easterEggs"),
                      ("HALLOWEEN_DROPS", "halloweenDrops"), ("HEART_DROPS", "heartDrops"),
                      ("SANTA_DROPS", "santaDrops"), ("REINDEER_DROPS", "reindeerDrops"), ("FORTUNES", "fortuneTier"),
                      ("SHOW_IF", "showIf"), ("CONFIRM", "requiresConfirmation")):
        id_list(name, L[key])
    for name, key in (("BANK_ACHIEVS", "bank"), ("CPS_ACHIEVS", "cps")):
        rec = b"".join(struct.pack("<dHHI", float(t), u16(i), 0, 0) for t, i in L[key])
        defs.append(f"#define OFF_{name} {P.put(rec, 8)}")
        defs.append(f"#define N_{name} {len(L[key])}")
    rec = b"".join(struct.pack("<dHHHHB7x", float(u["cookies"]), u16(u["up"]), u16(u["ach"]), u16(u["requireUp"]),
                               u16(u["requireAch"]), SEASONS.get(u["season"], 0)) for u in L["unlockAt"])
    defs.append(f"#define OFF_UNLOCK_AT {P.put(rec, 8)}")
    defs.append(f"#define N_UNLOCK_AT {len(L['unlockAt'])}")

    # ---- the texts, the pictures' table
    text_off = P.put(bytes(T.blob))
    defs.append(f"#define OFF_TEXT {text_off}")
    P.align(4)
    table = bytearray()
    for name, w, h, fmt, n, px, pal in P.images:
        table += struct.pack("<HHBBHII", w, h, fmt, n - 1, 0, px, pal)
    img_off = P.put(bytes(table))
    defs.append(f"#define OFF_IMAGES {img_off}")
    defs.append(f"#define N_IMAGES {len(P.images)}")
    for name, i in P.names.items():
        defs.append(f"#define IM_{name} {i}")
    defs.append(f"#define DATA_SIZE {len(P.blob)}")

    with open(OUT_BIN, "wb") as f:
        f.write(P.blob)
    lines = ["/* Written by tools/pack.py: what data.bin holds. */", "#ifndef CC_DATA_H", "#define CC_DATA_H", ""]
    lines += defs
    lines += ["", "#endif", ""]
    with open(OUT_H, "w") as f:
        f.write("\n".join(lines))
    if UNKNOWN:
        print("pack: characters left out:", "".join(sorted(UNKNOWN)))
    sizes = {}
    print(f"data.bin: {len(P.blob)} bytes, {len(P.images)} pictures, texts {len(T.blob)}")
    print("  " + ", ".join(f"{k} {v // 1024} KB" for k, v in sorted(P.sizes.items(), key=lambda kv: -kv[1])))


if __name__ == "__main__":
    if len(sys.argv) != 3:
        raise SystemExit(__doc__)
    main(sys.argv[1], sys.argv[2])
