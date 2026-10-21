"""The rooms of a floor: 13 x 7 cells each, in the game's style (its rocks, poop, pits and spikes, its monsters
standing where the game's own rooms put them), and the monsters each chapter fills them with.

A layout's cells: '.' floor, 'r' rock, 'm' metal block, 't' tinted rock, 'b' bomb rock, 'p' poop, 'q' corny poop,
'o' pit, 's' spikes, 'x' TNT, 'k' key block, 'w' cobweb, 'f' fire place, 'F' red fire place, '1' to '4' a monster
of the room's group (1: a walker, 2: a flier, 3: one that stays and shoots, 4: a tough one), '*' a pickup, 'c' a
chest, 'I' an item on its pedestal, and capitals a layout names for itself.

The packed rooms (ROOMS in data.bin): kind, chapters (bits: 1 Basement, 2 Caves, 4 Depths, 8 Womb, 16 Sheol), weight,
group (for a normal room: 0, its group picked when the room is made), then 7 x 13 cells: CELL_* below the
monsters, MON_* + CELL_MON above."""
import struct

# Rebirth's numbers of the items in the game (src/items.c holds what each does)
ITEM_IDS = [
    1, 2, 3, 4, 5, 6, 7, 8, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32,
    33, 34, 35, 36, 37, 38, 39, 41, 42, 44, 45, 46, 47, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 60, 62, 63, 64,
    65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79, 80, 81, 82, 83, 84, 85, 86, 87, 88, 89, 90, 91, 92,
    94, 95, 96, 97, 98, 99, 100, 101, 102, 103, 104, 105, 106, 108, 109, 110, 111, 112, 113, 114, 115, 117, 118,
    119, 120, 121, 122, 123, 124, 126, 127, 128, 129, 130, 131, 132, 133, 134, 135, 136, 137, 138, 140, 142, 143,
    144, 145, 146, 147, 149, 150, 152, 153, 154, 155, 157, 158, 159, 160, 161, 162, 163, 164, 165, 166, 167, 169,
    170, 171, 173, 174, 175, 176, 177, 178, 179, 182, 183, 184, 185, 186, 188, 189, 190, 192, 193, 194, 196, 197,
    199, 201, 245, 261,
]

CELLS = ["EMPTY", "ROCK", "BLOCK", "TINTED", "BOMB_ROCK", "POOP", "CORNY_POOP", "PIT", "SPIKES", "TNT", "KEY_BLOCK",
         "COBWEB", "FIRE", "RED_FIRE", "SLOT1", "SLOT2", "SLOT3", "SLOT4", "PICKUP", "CHEST", "ITEM", "RED_POOP",
         "GOLDEN_POOP"]
CELL_MON = 64   # a monster: CELL_MON + MON_*

CHARS = {".": "EMPTY", "r": "ROCK", "m": "BLOCK", "t": "TINTED", "b": "BOMB_ROCK", "p": "POOP", "q": "CORNY_POOP",
         "o": "PIT", "s": "SPIKES", "x": "TNT", "k": "KEY_BLOCK", "w": "COBWEB", "f": "FIRE", "F": "RED_FIRE",
         "1": "SLOT1", "2": "SLOT2", "3": "SLOT3", "4": "SLOT4", "*": "PICKUP", "c": "CHEST", "I": "ITEM"}

# the monsters (src/monsters.c says how each one moves)
MONSTERS = [
    "GAPER", "FROWNING_GAPER", "GUSHER", "PACER", "HORF", "FLY", "POOTER", "SUPER_POOTER", "CLOTTY", "CLOT", "IBLOB",
    "MULLIGAN", "ATTACK_FLY", "MAGGOT", "HIVE", "CHARGER", "GLOBIN", "BOOM_FLY", "RED_BOOM_FLY", "MAW", "RED_MAW",
    "HOST", "RED_HOST", "HOPPER", "TRITE", "BOIL", "SPITTY", "BRAIN", "LEAPER", "MR_MAW", "BABY", "ANGELIC_BABY",
    "VIS", "GUTS", "KNIGHT", "SELFLESS_KNIGHT", "LEECH", "KAMIKAZE_LEECH", "PARA_BITE", "EYE", "BLOODSHOT_EYE",
    "SUCKER", "SPIT", "EMBRYO", "MOTER", "SPIDER", "KEEPER", "GURGLE", "WALKING_BOIL", "BUTTLICKER", "HANGER",
    "SWARMER", "HEART", "MASK", "BIG_SPIDER", "NEST", "BABY_LONG_LEGS", "FATTY", "PALE_FATTY", "FAT_SACK",
    "DEATHS_HEAD", "MOMS_HAND", "DIP", "SQUIRT", "SKINNY", "BONY", "ONE_TOOTH", "GAPING_MAW", "RAGLING",
    "NIGHT_CRAWLER", "DART_FLY", "FAT_BAT", "IMP", "TUMOR", "POKY", "STONE_GRIMACE", "CONJOINED_FATTY",
    "ROUND_WORM", "WALL_CREEP", "MEMBRAIN", "LUMP", "FRED", "HOMUNCULUS", "BEGOTTEN", "CHUBBER", "FLAMING_HOPPER",
    "BLUE_FLY",
    # bosses and their parts
    "MONSTRO", "LARRY", "THE_HOLLOW", "DUKE", "HUSK", "GEMINI", "GEMINI_BABY", "STEVEN", "STEVEN_BABY",
    "BLIGHTED_OVUM", "GURDY", "GURDY_JR", "PEEP", "BLOAT", "CHUB", "CARRION_QUEEN", "LOKI", "LOKII", "MOM",
    "MOMS_HEART", "IT_LIVES", "FAMINE", "PESTILENCE", "WAR", "CONQUEST", "DEATH", "PIN", "SCOLEX", "FRAIL",
    "MONSTRO2", "GISH", "MASK_OF_INFAMY", "HEART_OF_INFAMY", "WIDOW", "WRETCHED", "DADDY_LONG_LEGS", "TRIACHNID",
    "FISTULA", "TERATOMA", "BLASTOCYST", "SATAN", "FALLEN", "KRAMPUS", "ISAAC", "HEADLESS_HORSEMAN", "DINGLE",
    "HAUNT", "LIL_HAUNT", "MEGA_FATTY", "MAMA_GURDY", "DARK_ONE", "POLYCEPHALUS", "MR_FRED", "GURGLING",
    "SLOTH", "LUST", "WRATH", "GLUTTONY", "GREED", "ENVY", "PRIDE", "MOM_HAND", "MOM_FOOT", "SATAN_STOMP",
    "DEATH_SCYTHE",
]
MON = {n: i for i, n in enumerate(MONSTERS)}

KINDS = ["NORMAL", "START", "TREASURE", "SHOP", "BOSS", "SECRET", "SUPERSECRET", "CURSE", "ARCADE", "SACRIFICE",
         "LIBRARY", "MINIBOSS", "CHALLENGE", "DEVIL", "ANGEL"]

# ---------------------------------------------------------------- monster groups: what each chapter's rooms hold
# (chapters, walker, flier, shooter, tough): a slot left None stays empty
GROUPS = [
    # the Basement and the Cellar
    (1, "GAPER", "FLY", "HORF", "GLOBIN"),
    (1, "FROWNING_GAPER", "ATTACK_FLY", "POOTER", "MULLIGAN"),
    (1, "HOPPER", "FLY", "CLOTTY", "BOOM_FLY"),
    (1, "GAPER", "POOTER", "HOST", "GUSHER"),
    (1, "SPIDER", "ATTACK_FLY", "HORF", "BIG_SPIDER"),
    (1, "MAGGOT", "FLY", "CLOT", "CHARGER"),
    (1, "DIP", "POOTER", "SQUIRT", "SQUIRT"),
    (1, "PACER", "MOTER", "MAW", "GLOBIN"),
    (1, "GUSHER", "SUCKER", "HOST", "HIVE"),
    (1, "GAPER", "ATTACK_FLY", "CLOTTY", "MULLIGAN"),
    (1, "HOPPER", "POOTER", "HORF", "BIG_SPIDER"),
    (1, "SPIDER", "FLY", "HOST", "BOOM_FLY"),
    (1, "FROWNING_GAPER", "FLY", "MAW", "FATTY"),
    # the Caves and the Catacombs
    (2, "HOPPER", "SUCKER", "HOST", "KNIGHT"),
    (2, "TRITE", "MOTER", "MAW", "LEAPER"),
    (2, "CHARGER", "BOOM_FLY", "CLOTTY", "FATTY"),
    (2, "GLOBIN", "ATTACK_FLY", "BOIL", "MR_MAW"),
    (2, "SPIDER", "DART_FLY", "SPITTY", "BIG_SPIDER"),
    (2, "SKINNY", "POOTER", "HORF", "WALKING_BOIL"),
    (2, "GURGLE", "SWARMER", "IBLOB", "BUTTLICKER"),
    (2, "BONY", "FAT_BAT", "RED_HOST", "KNIGHT"),
    (2, "NIGHT_CRAWLER", "SUCKER", "TUMOR", "HIVE"),
    (2, "GAPER", "SUPER_POOTER", "CLOT", "FAT_SACK"),
    (2, "TRITE", "BOOM_FLY", "SPITTY", "PALE_FATTY"),
    (2, "RAGLING", "MOTER", "HOST", "CHUBBER"),
    # the Depths and the Necropolis
    (4, "KNIGHT", "RED_BOOM_FLY", "RED_MAW", "SELFLESS_KNIGHT"),
    (4, "BONY", "FAT_BAT", "RED_HOST", "VIS"),
    (4, "BABY", "MOTER", "EYE", "FRED"),
    (4, "LEAPER", "SWARMER", "BRAIN", "HANGER"),
    (4, "IMP", "SUCKER", "BLOODSHOT_EYE", "GAPING_MAW"),
    (4, "DEATHS_HEAD", "RED_BOOM_FLY", "STONE_GRIMACE", "HOMUNCULUS"),
    (4, "GUTS", "ATTACK_FLY", "MR_MAW", "BEGOTTEN"),
    (4, "GLOBIN", "DART_FLY", "ONE_TOOTH", "KNIGHT"),
    (4, "SKINNY", "MOTER", "RED_HOST", "PALE_FATTY"),
    (4, "RAGLING", "SWARMER", "SPITTY", "CONJOINED_FATTY"),
    # the Womb and Utero
    (8, "GUTS", "SWARMER", "EYE", None),
    (8, "PARA_BITE", "LEECH", "BLOODSHOT_EYE", "FRED"),
    (8, "KAMIKAZE_LEECH", "EMBRYO", "LUMP", "MEMBRAIN"),
    (8, "BABY", "HEART", "VIS", "WALL_CREEP"),
    (8, "GURGLE", "LEECH", "NEST", "CONJOINED_FATTY"),
    (8, "ROUND_WORM", "SUCKER", "EYE", "VIS"),
    (8, "GUTS", "BOOM_FLY", "BLOODSHOT_EYE", "BEGOTTEN"),
    (8, "PARA_BITE", "SWARMER", "LUMP", "FAT_SACK"),
    (8, "BABY", "LEECH", "TUMOR", "HOMUNCULUS"),
    # Sheol and the Cathedral
    (16, "IMP", "RED_BOOM_FLY", "ONE_TOOTH", "BEGOTTEN"),
    (16, "BABY", "FAT_BAT", "BLOODSHOT_EYE", "KNIGHT"),
    (16, "ANGELIC_BABY", "SWARMER", "EYE", "SELFLESS_KNIGHT"),
    (16, "DEATHS_HEAD", "ATTACK_FLY", "STONE_GRIMACE", "VIS"),
    (16, "BONY", "LEECH", "RED_HOST", "FRED"),
]

# ---------------------------------------------------------------- layouts
L = []   # (kind, chapters, weight, rows, own monsters)


def room(rows, kind="NORMAL", chapters=31, weight=1, mons=None):
    assert len(rows) == 7 and all(len(r) == 13 for r in rows), rows
    L.append((kind, chapters, weight, rows, mons or {}))


# the starting room: empty (the game shows its keys on the floor)
room(["............."] * 7, "START")

# ---- normal rooms
room([".............",
      "..1.......1..",
      ".............",
      "......r......",
      ".............",
      "..1.......1..",
      "............."], weight=3)
room([".............",
      ".r.........r.",
      "...2.....2...",
      ".............",
      "...1.....1...",
      ".r.........r.",
      "............."], weight=3)
room(["r...........r",
      ".............",
      "..rr..3..rr..",
      "..r.......r..",
      "..rr.....rr..",
      ".......1.....",
      "r...........r"], weight=2)
room([".............",
      "..ppp...ppp..",
      "..p1.....1p..",
      ".............",
      "..p.......p..",
      "..ppp.2.ppp..",
      "............."], weight=2)
room([".............",
      ".1.........1.",
      "....rrrrr....",
      "....r.4.r....",
      "....r...r....",
      ".1.........1.",
      "............."], weight=2)
room([".....t.......",
      ".2...........",
      "...r.....r...",
      "......3......",
      "...r.....r...",
      "...........2.",
      "............."], weight=2)
room(["ooo.......ooo",
      "o.....1.....o",
      "...ooo.ooo...",
      ".3.o.....o.3.",
      "...ooo.ooo...",
      "o...........o",
      "ooo.......ooo"], chapters=30)
room([".............",
      ".mmm.....mmm.",
      ".m.........m.",
      "......4......",
      ".m.........m.",
      ".mmm.....mmm.",
      "............."], chapters=30)
room([".r.r.r.r.r.r.",
      ".............",
      "..1.......1..",
      "......2......",
      "..1.......1..",
      ".............",
      ".r.r.r.r.r.r."], weight=2)
room([".............",
      ".............",
      "..s.s.s.s.s..",
      "..s.s.3.s.s..",
      "..s.s.s.s.s..",
      ".1.........1.",
      "............."])
room([".............",
      ".....o.o.....",
      "..1..o.o..1..",
      ".oo.......oo.",
      "..2..o.o..2..",
      ".....o.o.....",
      "............."], chapters=30)
room([".............",
      ".....rrr.....",
      "..2..r*r..2..",
      ".....rbr.....",
      ".............",
      "..1.......1..",
      "............."], weight=2)
room(["pp.........pp",
      "p...........p",
      "...1.....1...",
      "......q......",
      "...1.....1...",
      "p...........p",
      "pp.........pp"])
room([".............",
      ".............",
      "...f.....f...",
      "......1......",
      "...f.....f...",
      ".............",
      "............."], weight=2)
room(["2...........2",
      ".............",
      ".............",
      "......4......",
      ".............",
      ".............",
      "2...........2"], weight=2)
room(["r.r.r...r.r.r",
      ".............",
      "r.....3.....r",
      ".............",
      "r...1...1...r",
      ".............",
      "r.r.r...r.r.r"])
room([".............",
      ".rr.......rr.",
      ".r..1...1..r.",
      ".....mmm.....",
      ".r..1...1..r.",
      ".rr.......rr.",
      "............."], weight=2)
room([".............",
      "......r......",
      "...3..r..3...",
      ".............",
      "...2..r..2...",
      "......r......",
      "............."])
room([".............",
      ".............",
      ".1..1..1..1..",
      ".............",
      ".............",
      ".............",
      "............."])
room([".............",
      "...rrrrrrr...",
      "...r.....r...",
      "...r..4..r...",
      "...r.....r...",
      "...rrr.rrr...",
      "............."])
room(["o.o.o...o.o.o",
      ".............",
      "o.....2.....o",
      ".............",
      "o...2...2...o",
      ".............",
      "o.o.o...o.o.o"], chapters=30)
room([".............",
      "..x.......x..",
      ".............",
      "...1.....1...",
      ".............",
      "..x.......x..",
      "............."])
room(["mmmmm...mmmmm",
      "m...........m",
      "m..1.....1..m",
      ".............",
      "m..3.....3..m",
      "m...........m",
      "mmmmm...mmmmm"], chapters=30)
room([".............",
      ".s.........s.",
      "..s...1...s..",
      "...s.....s...",
      "..s...1...s..",
      ".s.........s.",
      "............."], chapters=30)
room(["..ppp...ppp..",
      ".............",
      "p.....2.....p",
      ".............",
      "p.....2.....p",
      ".............",
      "..ppp...ppp.."])
room([".............",
      "....rr.rr....",
      "...r.....r...",
      "...r..c..r...",
      "...r.....r...",
      "....rr.rr....",
      ".1.........1."])
room([".............",
      ".1.r.....r.1.",
      "...r.....r...",
      "...r..3..r...",
      "...r.....r...",
      ".1.r.....r.1.",
      "............."])
room(["w...........w",
      ".............",
      "..2.......2..",
      ".....www.....",
      "..2.......2..",
      ".............",
      "w...........w"], chapters=6)
room([".............",
      "......1......",
      ".............",
      ".1....t....1.",
      ".............",
      "......1......",
      "............."], weight=2)
room(["...rr...rr...",
      "...r.....r...",
      ".3.........3.",
      ".............",
      ".............",
      "...r..1..r...",
      "...rr...rr..."])
room([".............",
      "..r.r.r.r.r..",
      ".............",
      "..r.r4r.r.r..",
      ".............",
      "..r.r.r.r.r..",
      "............."])
room(["oooo.....oooo",
      "oooo..2..oooo",
      ".............",
      ".1.........1.",
      ".............",
      "oooo.....oooo",
      "oooo.....oooo"], chapters=30)
room(["............."] * 3 + ["......*......"] + ["............."] * 3)   # an empty room, a pickup in it
room([".............",
      ".....mmm.....",
      "..1..m.m..1..",
      ".....m*m.....",
      "..2..m.m..2..",
      ".....m.m.....",
      "............."], chapters=30)
room(["F...........F",
      ".............",
      "...1.....1...",
      ".............",
      "...1.....1...",
      ".............",
      "F...........F"], chapters=28)
room([".............",
      ".f.........f.",
      ".............",
      "...r..2..r...",
      ".............",
      ".f.........f.",
      "............."])
room([".............",
      "..2.......2..",
      ".............",
      ".......4.....",
      ".............",
      "..2.......2..",
      "............."], weight=2)

# ---- special rooms
room([".............",
      ".............",
      ".............",
      "......I......",
      ".............",
      ".............",
      "............."], "TREASURE", weight=4)
room([".............",
      ".............",
      ".....r.r.....",
      "......I......",
      ".....r.r.....",
      ".............",
      "............."], "TREASURE", weight=2)
room([".............",
      ".............",
      "....sssss....",
      "....s.I.s....",
      "....sssss....",
      ".............",
      "............."], "TREASURE", chapters=30)
room([".............",
      "....rrrrr....",
      "...r.....r...",
      "...r..I..r...",
      "...r.....r...",
      "....rr.rr....",
      "............."], "TREASURE")
room([".............",
      ".............",
      "....oooo.....",
      "....oIoo.....",
      "....oooo.....",
      ".............",
      "............."], "TREASURE", chapters=30)
room([".............",
      ".............",
      ".............",
      ".....*.*.....",
      ".............",
      ".............",
      "............."], "SECRET", weight=3)
room([".............",
      ".............",
      ".............",
      "......I......",
      ".............",
      ".............",
      "............."], "SECRET", weight=2)
room([".............",
      "..*.......*..",
      ".............",
      "......c......",
      ".............",
      "..*.......*..",
      "............."], "SECRET", weight=2)
room([".............",
      ".............",
      ".............",
      "......I......",
      ".............",
      ".............",
      "............."], "SUPERSECRET")
room([".............",
      ".............",
      ".............",
      ".....c.c.....",
      ".............",
      ".............",
      "............."], "CURSE", weight=2)
room([".............",
      ".............",
      "......s......",
      ".....sIs.....",
      "......s......",
      ".............",
      "............."], "CURSE")
room([".............",
      "..1.......1..",
      ".............",
      "......c......",
      ".............",
      "..1.......1..",
      "............."], "CURSE")
for k in ("SHOP", "BOSS", "ARCADE", "SACRIFICE", "LIBRARY", "MINIBOSS", "CHALLENGE", "DEVIL", "ANGEL"):
    room(["............."] * 7, k)   # (laid out by the game itself)


SOLID = set("rmtbpqoxkfF")


def check(rows):
    """every door's cell reaches every other's, walking (no bombs, no flight)"""
    doors = [(6, 0), (0, 3), (6, 6), (12, 3)]
    for c, r in doors:
        assert rows[r][c] not in SOLID, (rows, "a door is blocked")
    seen, todo = {doors[0]}, [doors[0]]
    while todo:
        c, r = todo.pop()
        for dc, dr in ((1, 0), (-1, 0), (0, 1), (0, -1)):
            n = (c + dc, r + dr)
            if 0 <= n[0] < 13 and 0 <= n[1] < 7 and n not in seen and rows[n[1]][n[0]] not in SOLID:
                seen.add(n)
                todo.append(n)
    assert all(d in seen for d in doors), (rows, "a door can't be reached")


def encode():
    out = bytearray(struct.pack("<I", len(L)))
    for kind, chapters, weight, rows, mons in L:
        check(rows)
        out += bytes([KINDS.index(kind), chapters, weight, 0])
        for r in rows:
            for ch in r:
                if ch in mons:
                    out.append(CELL_MON + MON[mons[ch]])
                else:
                    out.append(CELLS.index(CHARS[ch]))
    return bytes(out)


def header():
    s = "\nenum {\n" + "".join(f"  CELL_{c},\n" for c in CELLS) + "  CELL_COUNT\n};\n"
    s += f"#define CELL_MON {CELL_MON}\n\nenum {{\n" + "".join(f"  MON_{m},\n" for m in MONSTERS) + "  MON_COUNT\n};\n"
    s += "\nenum {\n" + "".join(f"  ROOM_{k},\n" for k in KINDS) + "  ROOM_KIND_COUNT\n};\n"
    s += "\n/* the monster groups: chapters, then the monster of each slot (MON_COUNT: none) */\n"
    s += f"#define GROUP_COUNT {len(GROUPS)}\n"
    s += "#define GROUP_TABLE {"
    for g in GROUPS:
        s += "{" + str(g[0]) + "," + ",".join(f"MON_{m}" if m in MON else "MON_COUNT" for m in g[1:]) + "},"
    s += "}\n"
    s += f"#define ITEM_SPRITE_COUNT {len(ITEM_IDS)}\n#define ITEM_ID_TABLE {{" + ",".join(map(str, ITEM_IDS)) + "}\n"
    return s
