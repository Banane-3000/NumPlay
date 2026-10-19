#!/usr/bin/env python3
"""The Binding of Isaac's pictures, rooms and font -> src/data.bin + src/data.h.

    python3 -I tools/pack.py REF        (REF: the folder tools/fetch.py filled)

The game's pictures come at its own size, 26 pixels a cell of a room; the calculator shows a room whole at 20
(a room's walls and floor 360 x 240, its screen 320 x 240), so every picture is scaled by 20/26 here.

data.bin: "ISA1", the number of sections, their offsets, then the sections (SEC_* in data.h):
- FRAMES: the count, each frame's offset, then the frames: w, h, origin x, y, colours (u16 x 5, 0), the colours
  (RGB565), their opacity (0 to 32, a byte each, to an even count), each row's offset (u16) from the rows, then
  the rows: the number of runs, then each run's skip, length and palette indices
- SPRITES: for each sprite (SPR_*) its first frame and its number of frames (u16, u16)
- BACKDROPS: the count and offsets; each: w, h, colours (u16 x 3, 0), the colours, then w x h indices: the
  top-left quarter of a room, the rest of it mirrored
- FONTS: the count and offsets; each: height, first character, count, capitals only (bytes), 8 bytes a glyph
  (advance, w, h, ox, oy, 0, bitmap offset u16), then the bitmaps (rows of whole bytes, highest bit first)
- ROOMS: the count, then the rooms' layouts (tools/rooms.py): kind, chapters, weight, flags, 7 x 13 cells"""
import glob
import os
import struct
import sys

import numpy as np
from PIL import Image, ImageDraw, ImageFont

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import art as A  # noqa: E402
import rooms as R  # noqa: E402

OUT_BIN = os.path.join(HERE, "..", "src", "data.bin")
OUT_H = os.path.join(HERE, "..", "src", "data.h")
SECTIONS = ["FRAMES", "SPRITES", "BACKDROPS", "FONTS", "ROOMS"]
SCALE = 20 / 26
REF = None


def rgb565(r, g, b):
    return ((r * 31 + 127) // 255) << 11 | ((g * 63 + 127) // 255) << 5 | ((b * 31 + 127) // 255)


# ---------------------------------------------------------------- loading
def ent(name):
    """a picture of the editor's resources/Entities, by its file name (without .png)"""
    p = os.path.join(REF, "br", "resources", "Entities", name + ".png")
    if not os.path.exists(p):
        hits = glob.glob(os.path.join(REF, "br", "resources", "Entities", "**", name + ".png"), recursive=True)
        if not hits:
            raise SystemExit(f"pack: no picture {name}")
        p = hits[0]
    return Image.open(p).convert("RGBA")


def item_pic(n):
    hits = glob.glob(os.path.join(REF, "br", "resources", "Entities", "Items", f"5.100.{n} - *.png"))
    if not hits:
        raise SystemExit(f"pack: no item {n}")
    return Image.open(hits[0]).convert("RGBA")


def bg(name):
    return Image.open(os.path.join(REF, "br", "resources", "Backgrounds", name + ".png")).convert("RGBA")


def trim(im):
    bb = im.getbbox()
    return im.crop(bb) if bb else im


def scaled(im, s=SCALE):
    w, h = max(1, round(im.size[0] * s)), max(1, round(im.size[1] * s))
    # premultiplied, so the transparent pixels' colours don't bleed into the edges
    pm = im.convert("RGBa").resize((w, h), Image.LANCZOS)
    return pm.convert("RGBA")


# ---------------------------------------------------------------- frames
class Frame:
    def __init__(self, im, ox, oy):
        self.im, self.ox, self.oy = im, ox, oy


def feet(im, s=SCALE, dy=0):
    """a picture trimmed and scaled, its origin the middle of its bottom (where it stands)"""
    im = scaled(trim(im), s) if s != 1 else trim(im)
    return Frame(im, im.size[0] // 2, im.size[1] - 1 + dy)


def centred(im, s=SCALE):
    im = scaled(trim(im), s) if s != 1 else trim(im)
    return Frame(im, im.size[0] // 2, im.size[1] // 2)


def at(im, ox, oy):
    return Frame(im, ox, oy)


def encode_frame(fr):
    im = fr.im.convert("RGBA")
    w, h = im.size
    px = im.load()
    # opacity in 5 steps; colours as the screen has them
    keyed = {}
    cells = [[None] * w for _ in range(h)]
    for y in range(h):
        for x in range(w):
            r, g, b, a = px[x, y]
            al = (a * 4 + 127) // 255 * 8   # 0, 8, 16, 24, 32
            if al == 0:
                continue
            k = (rgb565(r, g, b), al)
            keyed[k] = keyed.get(k, 0) + 1
            cells[y][x] = k
    if len(keyed) > 254:
        # too many: the colours brought down to 254 by a median cut, opacities kept
        rgb = Image.new("RGB", (w, h))
        rp = rgb.load()
        for y in range(h):
            for x in range(w):
                r, g, b, a = px[x, y]
                rp[x, y] = (r, g, b)
        q = rgb.quantize(colors=200, method=Image.Quantize.MEDIANCUT, dither=Image.Dither.NONE)
        qp, qpal = q.load(), q.getpalette()
        keyed = {}
        for y in range(h):
            for x in range(w):
                if cells[y][x] is None:
                    continue
                i = qp[x, y]
                k = (rgb565(*qpal[3 * i:3 * i + 3]), cells[y][x][1])
                keyed[k] = keyed.get(k, 0) + 1
                cells[y][x] = k
        assert len(keyed) <= 254, len(keyed)
    pal = sorted(keyed, key=lambda k: -keyed[k])
    index = {k: i for i, k in enumerate(pal)}
    rows, offs = bytearray(), []
    for y in range(h):
        offs.append(len(rows))
        runs, x, last = [], 0, 0
        while x < w:
            if cells[y][x] is None:
                x += 1
                continue
            s = x
            while x < w and cells[y][x] is not None and x - s < 255:
                x += 1
            runs.append((s - last, [index[cells[y][i]] for i in range(s, x)]))
            last = x
        # skips over 255 are split into empty runs
        out = []
        for skip, ids in runs:
            while skip > 255:
                out.append((255, []))
                skip -= 255
            out.append((skip, ids))
        assert len(out) < 256
        rows.append(len(out))
        for skip, ids in out:
            rows += bytes([skip, len(ids)]) + bytes(ids)
    assert len(rows) < 65536
    n = len(pal)
    head = struct.pack("<6H", w, h, fr.ox & 0xFFFF, fr.oy & 0xFFFF, n, 0)
    body = b"".join(struct.pack("<H", c) for c, _ in pal) + bytes(a for _, a in pal) + bytes(n & 1)
    body += b"".join(struct.pack("<H", o) for o in offs)
    out = head + body + bytes(rows)
    return out + bytes(-len(out) % 4)


# ---------------------------------------------------------------- the sprites
SPRITES = []   # (name, [Frame])


def add(name, frames):
    if not isinstance(frames, list):
        frames = [frames]
    SPRITES.append((name, frames))


def rock_cells(sheet, cells, s=SCALE):
    """32 x 32 cells of a rocks sheet, by (column, row), standing on the bottom of their cell's 26 pixels"""
    out = []
    for c, r in cells:
        im = sheet.crop((c * 32, r * 32, c * 32 + 32, r * 32 + 32))
        im = scaled(im, s)
        out.append(Frame(im, im.size[0] // 2, im.size[1] // 2))
    return out


def sprites():
    heads, bodies = A.isaac_frames()
    add("ISAAC_HEAD", [Frame(scaled(h), round(14 * SCALE), round(23 * SCALE)) for h in heads])
    add("ISAAC_BODY", [Frame(scaled(b), round(14 * SCALE), round(9 * SCALE)) for b in bodies])
    # tears and shots
    add("TEAR", centred(ent("2.0.0 - Tear"), 1))
    add("BLOOD_TEAR", centred(ent("2.1.0 - Blood Tear"), 1))
    add("ENEMY_SHOT", centred(ent("9.0.0 - Enemy Tear"), 1))
    add("BONE_SHOT", centred(ent("9.0.1 - Bone")))
    add("KNIFE", centred(ent("8.0.0 - Knife")))
    # monsters (Rebirth's ids)
    monsters = [
        ("GAPER", "10.1.0 - Gaper"), ("FROWNING_GAPER", "10.0.0 - Frowning Gaper"), ("GUSHER", "11.0.0 - Gusher"),
        ("PACER", "11.1.0 - Pacer"), ("HORF", "12.0.0 - Horf"), ("FLY", "13.0.0 - Fly"), ("POOTER", "14.0.0 - Pooter"),
        ("SUPER_POOTER", "14.1.0 - Super Pooter"), ("CLOTTY", "15.0.0 - Clotty"), ("CLOT", "15.1.0 - Clot"),
        ("IBLOB", "15.2.0 - I.Blob"), ("MULLIGAN", "16.0.0 - Mulligan"), ("ATTACK_FLY", "18.0.0 - Attack Fly"),
        ("MAGGOT", "21.0.0 - Maggot"), ("HIVE", "22.0.0 - Hive"), ("CHARGER", "23.0.0 - Charger"),
        ("GLOBIN", "24.0.0 - Globin"), ("BOOM_FLY", "25.0.0 - Boom Fly"), ("RED_BOOM_FLY", "25.1.0 - Red Boom Fly"),
        ("MAW", "26.0.0 - Maw"), ("RED_MAW", "26.1.0 - Red Maw"), ("HOST", "27.0.0 - Host"), ("RED_HOST", "27.1.0 - Red Host"),
        ("HOPPER", "29.0.0 - Hopper"), ("TRITE", "29.1.0 - Trite"), ("BOIL", "30.0.0 - Boil"), ("SPITTY", "31.0.0 - Spitty"),
        ("BRAIN", "32.0.0 - Brain"), ("LEAPER", "34.0.0 - Leaper"), ("MR_MAW", "35.0.0 - Mr. Maw"), ("BABY", "38.0.0 - Baby"),
        ("ANGELIC_BABY", "38.1.0 - Angelic Baby"), ("VIS", "39.0.0 - Vis"), ("GUTS", "40.0.0 - Guts"), ("KNIGHT", "41.0.0 - Knight"),
        ("SELFLESS_KNIGHT", "41.1.0 - Selfless Knight"), ("LEECH", "55.0.0 - Leech"), ("KAMIKAZE_LEECH", "55.1.0 - Kamikaze Leech"),
        ("PARA_BITE", "58.0.0 - Para-Bite"), ("EYE", "60.0.0 - Eye"), ("BLOODSHOT_EYE", "60.1.0 - Bloodshot Eye"),
        ("SUCKER", "61.0.0 - Sucker"), ("SPIT", "61.1.0 - Spit"), ("EMBRYO", "77.0.0 - Embryo"), ("MOTER", "80.0.0 - Moter"),
        ("SPIDER", "85.0.0 - Spider"), ("KEEPER", "86.0.0 - Keeper"), ("GURGLE", "87.0.0 - Gurgle"),
        ("WALKING_BOIL", "88.0.0 - Walking Boil"), ("BUTTLICKER", "89.0.0 - Buttlicker"), ("HANGER", "90.0.0 - Hanger"),
        ("SWARMER", "91.0.0 - Swarmer"), ("HEART", "92.0.0 - Heart"), ("MASK", "93.0.0 - Mask"), ("BIG_SPIDER", "94.0.0 - Big Spider"),
        ("NEST", "205.0.0 - Nest"), ("BABY_LONG_LEGS", "206.0.0 - Baby Long Legs"), ("FATTY", "208.0.0 - Fatty"),
        ("PALE_FATTY", "208.1.0 - Pale Fatty"), ("FAT_SACK", "209.0.0 - Fat Sack"), ("DEATHS_HEAD", "212.0.0 - Death's Head"),
        ("MOMS_HAND", "213.0.0 - Mom's Hand"), ("DIP", "217.0.0 - Dip"), ("SQUIRT", "220.0.0 - Squirt"),
        ("SKINNY", "226.0.0 - Skinny"), ("BONY", "227.0.0 - Bony"), ("ONE_TOOTH", "234.0.0 - One Tooth"),
        ("GAPING_MAW", "235.0.0 - Gaping Maw"), ("RAGLING", "246.0.0 - Ragling"), ("NIGHT_CRAWLER", "255.0.0 - Night Crawler"),
        ("DART_FLY", "256.0.0 - Dart Fly"), ("FAT_BAT", "258.0.0 - Fat Bat"), ("IMP", "259.0.0 - Imp"),
        ("TUMOR", "229.0.0 - Tumor"), ("POKY", "44.0.0 - Poky"), ("STONE_GRIMACE", "42.0.0 - Stone Grimace"),
        ("CONJOINED_FATTY", "257.0.0 - Conjoined Fatty"), ("ROUND_WORM", "244.0.0 - Round Worm"),
        ("WALL_CREEP", "240.0.0 - Wall Creep"), ("MEMBRAIN", "57.0.0 - MemBrain"), ("LUMP", "56.0.0 - Lump"),
        ("FRED", "59.0.0 - Fred"), ("HOMUNCULUS", "228.0.0 - Homunculus"), ("BEGOTTEN", "251.0.0 - Begotten"),
        ("SQUIRT_DANK", "220.1.0 - Dank Squirt"), ("CHUBBER", "39.2.0 - Chubber"), ("FLAMING_HOPPER", "54.0.0 - Flaming Hopper"),
    ]
    for n, f in monsters:
        add(n, feet(ent(f)))
    # bosses
    bosses = [
        ("MONSTRO", "20.0.0 - Monstro"), ("LARRY", "19.0.0 - Larry Jr"), ("THE_HOLLOW", "19.1.0 - The Hollow"),
        ("DUKE", "67.0.0 - The Duke of Flies"), ("HUSK", "67.1.0 - The Husk"), ("GEMINI", "79.0.0 - Gemini"),
        ("GEMINI_BABY", "79.10.0 - Gemini Baby"), ("STEVEN", "79.1.0 - Steven"), ("STEVEN_BABY", "79.11.0 - Steven Baby"),
        ("BLIGHTED_OVUM", "79.2.0 - The Blighted Ovum"), ("UMBILICAL", "79.20.0 - Umbilical Cord"),
        ("GURDY", "36.0.0 - Gurdy"), ("GURDY_JR", "99.0.0 - Gurdy Jr"), ("PEEP", "68.0.0 - Peep"), ("BLOAT", "68.1.0 - The Bloat"),
        ("CHUB", "28.0.0 - Chub"), ("CARRION_QUEEN", "28.2.0 - The Carrion Queen"), ("LOKI", "69.0.0 - Loki"),
        ("LOKII", "69.1.0 - Lokii"), ("MOM_EYE", "45.0.0 - Mom"), ("MOMS_HEART", "78.0.0 - Mom's Heart"),
        ("IT_LIVES", "78.1.0 - It Lives"), ("FAMINE", "63.0.0 - Famine"), ("PESTILENCE", "64.0.0 - Pestilence"),
        ("WAR", "65.0.0 - War"), ("CONQUEST", "65.1.0 - Conquest"), ("DEATH", "66.0.0 - Death"), ("DEATH_HORSE", "66.20.0 - Death Horse"),
        ("PIN", "62.0.0 - Pin"), ("SCOLEX", "62.1.0 - Scolex"), ("FRAIL", "62.2.0 - The Frail"),
        ("MONSTRO2", "43.0.0 - Monstro II"), ("GISH", "43.1.0 - Gish"), ("MASK_OF_INFAMY", "97.0.0 - Mask of Infamy"),
        ("HEART_OF_INFAMY", "98.0.0 - Heart of Infamy"), ("WIDOW", "100.0.0 - Widow"), ("WRETCHED", "100.1.0 - The Wretched"),
        ("DADDY_LONG_LEGS", "101.0.0 - Daddy Long Legs"), ("TRIACHNID", "101.1.0 - Triachnid"),
        ("FISTULA_BIG", "71.0.0 - Fistula Big"), ("FISTULA_MED", "72.0.0 - Fistula Medium"), ("FISTULA_SMALL", "73.0.0 - Fistula Small"),
        ("TERATOMA_BIG", "71.1.0 - Teratoma Big"), ("BLASTOCYST_BIG", "74.0.0 - Blastocyst Big"),
        ("BLASTOCYST_MED", "75.0.0 - Blastocyst Medium"), ("BLASTOCYST_SMALL", "76.0.0 - Blastocyst Small"),
        ("SATAN", "84.0.0 - Satan"), ("SATAN_STOMP", "84.10.0 - Satan Stomp"), ("FALLEN", "81.0.0 - The Fallen"),
        ("KRAMPUS", "81.1.0 - Krampus"), ("ISAAC_BOSS", "102.0.0 - Isaac (final boss)"), ("BLUE_BABY", "102.1.0 - Blue Baby (final boss)"),
        ("HEADLESS_HORSEMAN", "82.0.0 - Headless Horseman Body"), ("HORSEMAN_HEAD", "83.0.0 - Headless Horsemans Head"),
        ("DINGLE", "261.0.0 - Dingle"), ("HAUNT", "260.0.0 - Haunt"), ("LIL_HAUNT", "260.10.0 - Lil' Haunt"),
        ("MEGA_FATTY", "264.0.0 - Mega Fatty"), ("CAGE", "265.0.0 - The Cage"), ("MAMA_GURDY", "266.0.0 - Mama Gurdy"),
        ("DARK_ONE", "267.0.0 - Dark One"), ("ADVERSARY", "268.0.0 - The Adversary"), ("POLYCEPHALUS", "269.0.0 - Polycephalus"),
        ("MR_FRED", "270.0.0 - Mr. Fred"), ("LAMB", "273.0.0 - The Lamb"), ("URIEL", "271.0.0 - Uriel"), ("GABRIEL", "272.0.0 - Gabriel"),
        ("GURGLING", "237.1.0 - Gurgling (boss)"), ("MEGA_MAW", "262.0.0 - Mega Maw"), ("GATE", "263.0.0 - The Gate"),
        ("SLOTH", "46.0.0 - Sloth"), ("LUST", "47.0.0 - Lust"), ("WRATH", "48.0.0 - Wrath"), ("GLUTTONY", "49.0.0 - Gluttony"),
        ("GREED", "50.0.0 - Greed"), ("ENVY", "51.0.0 - Envy"), ("ENVY_HALF", "51.10.0 - Envy (1-2)"), ("ENVY_QUARTER", "51.20.0 - Envy (1-4)"),
        ("PRIDE", "52.0.0 - Pride"), ("SUPER_SLOTH", "46.1.0 - Super Sloth"),
    ]
    for n, f in bosses:
        add(n, feet(ent(f)))
    # grid things
    poop = ent("1500.0.0 - Poop")
    add("POOP", [feet(poop.crop((0, int(poop.size[1] * k / 4), poop.size[0], poop.size[1])), dy=0) for k in range(4)])
    add("RED_POOP", feet(ent("1490.0.0 - Red Poop")))
    add("GOLDEN_POOP", feet(ent("1496.0.0 - Golden Poop")))
    add("CORNY_POOP", feet(ent("1495.0.0 - Chunky Poop")))
    add("TNT", feet(ent("1300.0.0 - TNT")))
    add("BLOCK", feet(ent("1900.0.0 - Block")))
    add("KEY_BLOCK", feet(ent("4000.0.0 - Key Block")))
    add("SPIKES", centred(ent("1930.0.0 - Spikes")))
    add("COBWEB", centred(ent("1940.0.0 - Cobweb")))
    add("PIT", centred(ent("3000.0.0 - Pit")))
    add("TRAPDOOR", centred(ent("9000.0.0 - Trap Door")))
    for st in ("generic", "cellar", "caves", "catacombs", "depths", "womb", "sheol", "cathedral",
               "secretroom", "scarredwomb"):
        sheet = bg("rocks_" + st)
        # three rocks, a metal block, a tinted rock, a bomb rock, a pot, rubble
        add("ROCKS_" + st.upper(), rock_cells(sheet, [(0, 0), (1, 0), (2, 0), (0, 1), (3, 2) if sheet.size[0] > 128 else (3, 2),
                                                      (0, 2), (2, 1), (3, 0)]))
    add("MOM_FOOT", feet(A.mom_foot()))
    add("FIREPLACE", feet(ent("33.0.0 - Fire Place")))
    add("RED_FIREPLACE", feet(ent("33.1.0 - Red Fire Place")))
    add("BLUE_FIREPLACE", feet(ent("33.2.0 - Blue Fire Place")))
    add("PURPLE_FIREPLACE", feet(ent("33.3.0 - Purple Fire Place")))
    # pickups
    for n, f in [("HEART_FULL", "5.10.1 - Heart"), ("HEART_HALF", "5.10.2 - Heart (half)"), ("HEART_SOUL", "5.10.3 - Heart (soul)"),
                 ("HEART_ETERNAL", "5.10.4 - Heart (eternal)"), ("HEART_DOUBLE", "5.10.5 - Heart (double)"),
                 ("HEART_BLACK", "5.10.6 - Black Heart"), ("HEART_HALFSOUL", "5.10.8 - Heart (halfsoul)"),
                 ("PENNY", "5.20.1 - Penny"), ("NICKEL", "5.20.2 - Nickel"), ("DIME", "5.20.3 - Dime"), ("LUCKY_PENNY", "5.20.5 - Lucky Penny"),
                 ("KEY", "5.30.1 - Key"), ("GOLDEN_KEY", "5.30.2 - Golden Key"), ("KEY_RING", "5.30.3 - Key Ring"),
                 ("BOMB_PICKUP", "5.40.1 - Bomb"), ("DOUBLE_BOMB", "5.40.2 - Double Bomb"), ("TROLL_BOMB", "5.40.3 - Troll Bomb"),
                 ("GOLDEN_BOMB", "5.40.4 - Golden Bomb"),
                 ("CHEST", "5.50.0 - Chest"), ("BOMB_CHEST", "5.51.0 - Bomb Chest"), ("SPIKED_CHEST", "5.52.0 - Spiked Chest"),
                 ("ETERNAL_CHEST", "5.53.0 - Eternal Chest"), ("LOCKED_CHEST", "5.60.0 - Locked Chest"), ("RED_CHEST", "5.360.0 - Red Chest"),
                 ("GRAB_BAG", "5.69.1 - Grab Bag"), ("BATTERY", "5.90.1 - Lil' Battery"), ("TROPHY", "5.370.0 - Trophy"),
                 ("BIG_CHEST", "5.340.0 - Big Chest"), ("PRICE", "5.150.0 - Shop Item")]:
        add(n, feet(ent(f)))
    pills = ["5.70.1 - Blue Pill", "5.70.2 - White-Blue Pill", "5.70.3 - Orange Pill", "5.70.4 - White Pill",
             "5.70.5 - Dots-Red Pill", "5.70.6 - Pink-Red Pill", "5.70.7 - Blue-Cadetblue Pill", "5.70.8 - Yellow-Orange Pill",
             "5.70.9 - Dots-White Pill", "5.70.10 - White-Azure Pill", "5.70.11 - Black-Yellow Pill",
             "5.70.12 - White-Black Pill", "5.70.13 - White-Yellow Pill"]
    add("PILL", [feet(ent(p)) for p in pills])
    cards = sorted(glob.glob(os.path.join(REF, "br", "resources", "Entities", "5.300.*.png")),
                   key=lambda p: int(os.path.basename(p).split(" ")[0].split(".")[2]))
    card_pics = {int(os.path.basename(p).split(" ")[0].split(".")[2]): p for p in cards}
    add("CARD", [feet(Image.open(card_pics[i]).convert("RGBA")) for i in range(1, 32)])   # the 22 tarot, playing cards, joker
    add("RUNE", [feet(Image.open(card_pics[i]).convert("RGBA")) for i in range(32, 42)])
    add("PEDESTAL", feet(ent("5.100.0 - Collectible").crop((0, 30, 32, 44))))
    # the items, by Rebirth's numbers (src/items.c gives the list)
    for n in R.ITEM_IDS:
        add(f"ITEM_{n}", feet(item_pic(n)))
    # machines, beggars, shopkeepers
    for n, f in [("SLOT_MACHINE", "6.1.0 - Slot Machine"), ("BLOOD_MACHINE", "6.2.0 - Blood Donation Machine"),
                 ("FORTUNE_MACHINE", "6.3.0 - Fortune Telling Machine"), ("BEGGAR", "6.4.0 - Beggar"),
                 ("DEVIL_BEGGAR", "6.5.0 - Devil Beggar"), ("KEY_MASTER", "6.7.0 - Key Master"), ("BOMB_BUM", "6.9.0 - Bomb Bum"),
                 ("SHOPKEEPER", "17.0.0 - Keeper"), ("HANGING_KEEPER", "17.1.0 - Hanging Keeper"),
                 ("BOMB", "4.0.0 - Bomb"), ("BIG_BOMB", "4.0.10 - Mr Mega Bomb"), ("TROLL_BOMB_LIT", "4.0.3 - Troll Bomb"),
                 ("POISON_BOMB", "4.0.5 - Poison Bomb"), ("SMOKE", "1000.141.1 - Smoke Cloud")]:
        add(n, feet(ent(f)))
    # the HUD (drawn at the screen's size)
    hearts = [A.heart(k, h) for k, h in (("red", False), ("red", True), ("empty", False), ("soul", False), ("soul", True),
                                          ("black", False), ("black", True), ("eternal", False))]
    add("HUD_HEART", [Frame(h, 0, 0) for h in hearts])
    add("HUD_ICON", [Frame(A.art(A.ICONS[k], A.ICON_PAL), 0, 0) for k in ("coin", "bomb", "key")])
    icons = ["boss", "treasure", "shop", "secret", "curse", "arcade", "sacrifice", "library", "miniboss", "challenge", "chest"]
    add("MAP_ICON", [Frame(A.map_icon(k, (255, 255, 255)), 3, 3) for k in icons])
    # the doors, in the top wall; the game turns them for the others
    kinds = ["normal", "treasure", "boss", "secret", "shop", "curse", "devil", "angel", "arcade"]
    frames = []
    for k in kinds:
        for o in (True, False):
            d = scaled(A.door(k, o))
            frames.append(Frame(d, d.size[0] // 2, d.size[1] - 1))
    add("DOOR", frames)
    for k in kinds:
        side = []
        for o in (True, False):
            d = scaled(A.door(k, o).rotate(90, expand=True))   # (the left wall's: its floor side on the right)
            side.append(Frame(d, d.size[0] - 1, d.size[1] // 2))
        add("DOOR_SIDE_" + k.upper(), side)


# ---------------------------------------------------------------- the rooms' walls and floors
BACKDROPS = [
    ("BASEMENT", "01_basement"), ("CELLAR", "02_cellar"), ("CAVES", "03_caves"), ("CATACOMBS", "04_catacombs"),
    ("DEPTHS", "05_depths"), ("NECROPOLIS", "06_necropolis"), ("WOMB", "07_womb"), ("UTERO", "08_utero"),
    ("SHEOL", "09_sheol"), ("CATHEDRAL", "10_cathedral"), ("CHEST", "11_chest"), ("DARKROOM", "12_darkroom"),
    ("SCARRED", "16_scarredwomb"), ("LIBRARY", "0a_library"), ("SHOP", "0b_shop"), ("ISAACS_ROOM", "0c_isaacsroom"),
    ("BARREN", "0d_barrenroom"), ("ARCADE", "0e_arcade"), ("SECRET", "0f_secretroom"), ("SACRIFICE", "0g_sacrificeroom"),
]


def encode_backdrop(name):
    src = bg(name)
    if src.getextrema()[3][0] < 255:
        # (the Cathedral's floor has a cross left open for a layer under it: the floor's own colour, darker)
        a = np.asarray(src.crop((100, 80, 230, 150)))
        mean = tuple(int(v) * 3 // 4 for v in a[a[:, :, 3] == 255][:, :3].mean(axis=0))
        under = Image.new("RGBA", src.size, mean + (255,))
        under.alpha_composite(src)
        src = under
    im = scaled(src).convert("RGB")   # 234 x 156 -> 180 x 120
    assert im.size == (180, 120), im.size
    q = im.quantize(colors=256, method=Image.Quantize.MEDIANCUT, dither=Image.Dither.NONE)
    pal = q.getpalette()[:256 * 3]
    n = len(pal) // 3
    head = struct.pack("<4H", im.size[0], im.size[1], n, 0)
    cols = b"".join(struct.pack("<H", rgb565(*pal[3 * i:3 * i + 3])) for i in range(n))
    out = head + cols + q.tobytes()
    return out + bytes(-len(out) % 4)


# ---------------------------------------------------------------- the font
def encode_font(path, size, first=32, last=126, caps=True):
    f = ImageFont.truetype(path, size)
    asc, desc = f.getmetrics()
    glyphs, bits = [], bytearray()
    for c in range(first, last + 1):
        ch = chr(c)
        im = Image.new("L", (size * 3, size * 3), 0)
        ImageDraw.Draw(im).text((size, size), ch, font=f, fill=255)
        im = im.point(lambda v: 255 if v > 127 else 0)
        bb = im.getbbox()
        adv = round(f.getlength(ch))
        if not bb:
            glyphs.append((adv, 0, 0, 0, 0, len(bits)))
            continue
        g = im.crop(bb)
        w, h = g.size
        off = len(bits)
        stride = (w + 7) // 8
        gp = g.load()
        for y in range(h):
            row = bytearray(stride)
            for x in range(w):
                if gp[x, y]:
                    row[x >> 3] |= 0x80 >> (x & 7)
            bits += row
        glyphs.append((adv, w, h, bb[0] - size, bb[1] - size, off))
    out = bytearray([asc + desc, first, last - first + 1, 1 if caps else 0])
    for adv, w, h, ox, oy, off in glyphs:
        out += struct.pack("<BBBbbBH", adv, w, h, ox, oy, 0, off)
    out += bits
    return bytes(out) + bytes(-len(out) % 4)


# ---------------------------------------------------------------- writing
def table(entries):
    out, offs = bytearray(), []
    base = 4 + 4 * len(entries)
    for e in entries:
        offs.append(base + len(out))
        out += e
        out += bytes(-len(out) % 4)
    return struct.pack("<I", len(entries)) + b"".join(struct.pack("<I", o) for o in offs) + bytes(out)


def main():
    global REF
    if len(sys.argv) != 2:
        sys.exit(__doc__)
    REF = sys.argv[1]
    sprites()
    frames, spr = [], bytearray(struct.pack("<I", len(SPRITES)))
    for name, fr in SPRITES:
        spr += struct.pack("<HH", len(frames), len(fr))
        frames += [encode_frame(f) for f in fr]
    font = os.path.join(REF, "fonts", "upheavtt.ttf")
    rooms = R.encode()
    secs = {"FRAMES": table(frames), "SPRITES": bytes(spr), "BACKDROPS": table([encode_backdrop(f) for _, f in BACKDROPS]),
            "FONTS": table([encode_font(font, 10)]), "ROOMS": rooms}
    head = b"ISA1" + struct.pack("<I", len(SECTIONS))
    base = len(head) + 4 * len(SECTIONS)
    offs, body = [], bytearray()
    for s in SECTIONS:
        offs.append(base + len(body))
        body += secs[s]
        body += bytes(-len(body) % 4)
    data = head + b"".join(struct.pack("<I", o) for o in offs) + bytes(body)
    open(OUT_BIN, "wb").write(data)
    with open(OUT_H, "w") as h:
        h.write("/* Generated by tools/pack.py: what data.bin holds. */\n#ifndef IS_DATA_H\n#define IS_DATA_H\n\n")
        for i, s in enumerate(SECTIONS):
            h.write(f"#define SEC_{s} {i}\n")
        h.write("\nenum {\n")
        for name, _ in SPRITES:
            h.write(f"  SPR_{name},\n")
        h.write("  SPR_COUNT\n};\n\nenum {\n")
        for name, _ in BACKDROPS:
            h.write(f"  BD_{name},\n")
        h.write("  BD_COUNT\n};\n\n#define FONT_UPHEAVAL 0\n")
        h.write(R.header())
        h.write("\n#endif\n")
    sizes = {s: len(secs[s]) for s in SECTIONS}
    print(f"data.bin: {len(data)} bytes, {len(SPRITES)} sprites, {len(frames)} frames", sizes)


if __name__ == "__main__":
    main()
