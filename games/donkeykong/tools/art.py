#!/usr/bin/env python3
"""src/art.h: Donkey Kong's pictures, drawn for this port (no Nintendo graphics data).

Bobydeluxe drew the first set for his adaptation; they are redrawn here at the arcade's sizes, with its
poses and animation frames: every sprite is a 16x16 cell with three colours and transparency, looked up
by the arcade's sprite code (see the dkdasm notes), and Kong is assembled from parts by the game's own
pose tables. Tiles (girders, ladders, belts, rivets, letters) are 8x8.

    python3 -I tools/art.py src/art.h [preview.png]   (the preview needs Pillow)
"""
import sys

# ---------------------------------------------------------------- palettes (index 1..3; 0 is transparent)
PAL = {
    "mario": [0xFF2010, 0x2830FF, 0xFFB855],
    "kong": [0xB81000, 0xFFB855, 0xFF6800],
    "kface": [0xB81000, 0xFFB855, 0xFFFFFF],
    "paul_top": [0xFF6800, 0xFFF4E8, 0xFF47AA],
    "paul_low": [0xFF47AA, 0xFFFFFF, 0x1010B0],
    "barrel": [0xFF6800, 0xFFB855, 0x2020FF],
    "blue": [0x2020FF, 0x00F0FF, 0xFFFFFF],
    "fire": [0xFFC200, 0xFF2000, 0xFFFFFF],
    "fire_blue": [0x9797FF, 0xFF2000, 0x00F0FF],
    "hammer": [0xFFB800, 0x972000, 0xFFFFFF],
    "hammer_flash": [0xFF2000, 0xFFE070, 0xFFFFFF],
    "points": [0xB81000, 0xFFB855, 0xFFFFFF],
    "pie": [0xF8B999, 0x2020F0, 0xFF7100],
    "spring": [0xA4A5FF, 0xFF2000, 0xFFFFFF],
    "elevator": [0xFF2C62, 0xA40000, 0xFF2C62],
    "motor": [0xFFC200, 0xA40000, 0xFF6800],
    "ladder_white": [0xFCFCFF, 0xFCFCFF, 0xFCFCFF],
    "pulley": [0x9B9BFF, 0xFA0000, 0x00F0FF],
    "flame": [0xFF2000, 0xFFB800, 0xFFFFFF],
    "items": [0xFF47AA, 0xFFFFFF, 0x1010B0],
    "heart": [0xFF6800, 0xFFFFFF, 0xFF47AA],
    "black": [0x000000, 0x000000, 0x000000],
    "smash": [0x2020FF, 0x00F0FF, 0xFFFFFF],
    "dizzy": [0xFFFFFF, 0xFFC200, 0x00F0FF],
}

# ---------------------------------------------------------------- sprites: code -> (palette, 16 rows); letters map to 1..3


def grid(rows, keys):
    out = []
    for r in rows:
        r = r.ljust(16, ".")[:16]
        out.append([0 if c == "." else keys.index(c) + 1 for c in r])
    while len(out) < 16:
        out.append([0] * 16)
    return out


def mirror(g):
    return [list(reversed(r)) for r in g]


def vflip(g):
    return [list(r) for r in reversed(g)]


def rot_cw(g):
    return [[g[15 - x][y] for x in range(16)] for y in range(16)]


def rot_ccw(g):
    return [[g[x][15 - y] for x in range(16)] for y in range(16)]


def shift(g, dx, dy):
    out = [[0] * 16 for _ in range(16)]
    for y in range(16):
        for x in range(16):
            sx, sy = x - dx, y - dy
            if 0 <= sx < 16 and 0 <= sy < 16:
                out[y][x] = g[sy][sx]
    return out


S = {}  # code -> (palette, grid)

# Mario (Jumpman): drawn facing right, stored facing left as the arcade's codes 0x00-0x0F are
M = "RBP"  # red, blue, peach
HEAD = [
    "......RRRRR.....",
    ".....RRRRRRRRR..",
    ".....BBBPPBP....",
    "....BPBPPPBPPP..",
    "....BPBBPPPBPPP.",
    "....BBPPPPBBBB..",
    "......PPPPPPP...",
]
mario = {
    0x00: ["................"] + HEAD + [
        ".....BBRBBB.....",
        "....BBBRBBRBBB..",
        "...BBBBRRRRBBBB.",
        "...PPBRPRRPRBPP.",
        "...PPPRRRRRRPPP.",
        "....PRRRRRRRRP..",
        ".....RRR..RRR...",
        "....BBBB..BBBB..",
    ],
    0x01: ["................"] + HEAD + [
        "....BBBRRBB.P...",
        "...PBBBRRRBBPP..",
        "..PPBBRRRRRR.P..",
        "..PP.RRRRRRR....",
        ".....RRRRRRRR...",
        "....RRRR..RRRR..",
        "...BBBB....BBB..",
        "...BBB......BBB.",
    ],
    0x02: ["................"] + HEAD + [
        ".....BBRBBB.....",
        "....BBBRRBBB....",
        "....BBRRPRRB....",
        "....BPPRRRRR....",
        "....RPPRRRR.....",
        ".....RRRBBB.....",
        ".....BBBBBB.....",
        "......BBBB......",
    ],
    # climbing, seen from behind: left hand up / over the top / back
    0x03: [
        "...PP...........",
        "...PP.RRRRR.....",
        "...BB.RRRRRR....",
        "...BBRRRRRRR....",
        "...BBBBBBBBB....",
        "....BBBBBBBBB...",
        ".....BRBBBRBB...",
        ".....BRBBBRBBB..",
        ".....RRRRRRR.BB.",
        ".....RRRRRRR.PP.",
        ".....RRRRRRR.PP.",
        ".....RRR.RRR....",
        ".....RRR.RRBB...",
        "....BBRR..BBB...",
        "....BBB.........",
        "....BBB.........",
    ],
    0x04: [
        "................",
        "................",
        "................",
        "......RRRRR.....",
        ".....RRRRRRR....",
        ".PP..RRRRRRR..PP",
        ".PPBBBBBBBBBBBPP",
        "..BBBBBBBBBBBBB.",
        "...BBRBBBBBRBB..",
        ".....RRRRRRR....",
        ".....RRRRRRR....",
        ".....RRRRRRR....",
        ".....RRR.RRR....",
        ".....RRR.RRR....",
        "....BBBB.BBBB...",
        "....BBB...BBB...",
    ],
    0x05: [
        "................",
        "................",
        "................",
        "................",
        "................",
        ".....RRRRRRR....",
        ".PP.RRRRRRRRR.PP",
        ".PPBBBBBBBBBBBPP",
        "..BBBBBBBBBBBBB.",
        "...BBRBBBBBRBB..",
        "....RRRRRRRRR...",
        "....RRRRRRRRR...",
        "....RRRR.RRRR...",
        "...BBBB...BBBB..",
        "...BBB.....BBB..",
        "................",
    ],
    0x06: [
        "................",
        "......RRRRR.....",
        ".....RRRRRRR....",
        ".....RRRRRRR....",
        ".....BBBBBBB....",
        "....BBBBBBBBB...",
        "...BBRBBBBBRBB..",
        "..PBBRBBBBBRBBP.",
        "..PPBRRRRRRRBPP.",
        "..PP.RRRRRRR.PP.",
        ".....RRRRRRR....",
        ".....RRRRRRR....",
        ".....RRR.RRR....",
        ".....RRR.RRR....",
        "....BBBB.BBBB...",
        "....BBB...BBB...",
    ],
    # with the hammer: held up (even) or swung (odd), standing and the two running steps
    0x08: [
        "......PP........",
        "......PPRRR.....",
        ".....RBRRRRRRR..",
        ".....BBBPPBP....",
        "....BPBPPPBPPP..",
        "....BPBBPPPBPPP.",
        "....BBPPPPBBBB..",
        ".....BPPPPPPP...",
        ".....BBRBBB.....",
        "....BBBRBBRBB...",
        "....BBBRRRRBB...",
        "....RRRRRRRRR...",
        "....RRRRRRRRR...",
        ".....RRR..RRR...",
        "....BBBB..BBBB..",
        "................",
    ],
    0x09: ["................"] + HEAD + [
        ".....BBBBBBBBPP.",
        "....BBRBBRBBBPP.",
        "...BBBRRRRB.....",
        "....RRRRRRR.....",
        "....RRRRRRRR....",
        ".....RRR..RRR...",
        "....BBBB..BBBB..",
        "................",
    ],
    0x0A: [
        "......PP........",
        "......PPRRR.....",
        ".....RBRRRRRRR..",
        ".....BBBPPBP....",
        "....BPBPPPBPPP..",
        "....BPBBPPPBPPP.",
        "....BBPPPPBBBB..",
        ".....BPPPPPPP...",
        "....BBBRRBB.....",
        "....BBBRRRBB....",
        "....BBRRRRRR....",
        ".....RRRRRRR....",
        ".....RRRRRRRR...",
        "....RRRR..RRRR..",
        "...BBBB....BBB..",
        "...BBB......BBB.",
    ],
    0x0B: ["................"] + HEAD + [
        "....BBBBBBBBBPP.",
        "...BBBBRRBBBBPP.",
        "...BBBRRRRR.....",
        ".....RRRRRRR....",
        ".....RRRRRRRR...",
        "....RRRR..RRRR..",
        "...BBBB....BBB..",
        "...BBB......BBB.",
    ],
    0x0C: [
        "......PP........",
        "......PPRRR.....",
        ".....RBRRRRRRR..",
        ".....BBBPPBP....",
        "....BPBPPPBPPP..",
        "....BPBBPPPBPPP.",
        "....BBPPPPBBBB..",
        ".....BPPPPPPP...",
        ".....BBRBBB.....",
        "....BBBRRBBB....",
        "....BBRRRRRB....",
        ".....RRRRRR.....",
        ".....RRRBBB.....",
        ".....BBBBBB.....",
        "......BBBB......",
        "................",
    ],
    0x0D: ["................"] + HEAD + [
        ".....BBBBBBBBPP.",
        "....BBBRRBBBBPP.",
        "....BBRRRRR.....",
        ".....RRRRRR.....",
        ".....RRRBBB.....",
        ".....BBBBBB.....",
        "......BBBB......",
        "................",
    ],
    0x0E: ["................"] + HEAD + [
        "..PP.BBBRBBB.PP.",
        "..PPBBBBRBBRBPP.",
        ".......RRRRRR...",
        "......RRRRRRRR..",
        ".....RRRR..RRR..",
        "....BBBB....BBB.",
        "...BBBB......BB.",
        "................",
    ],
    0x0F: ["................", "................"] + HEAD + [
        "....BBBRBBBB....",
        "...PBBBRRBBBP...",
        "..PPBBRRRRBBPP..",
        "..PP.RRRRRRR.PP.",
        "....RRRR.RRRR...",
        "...BBBB...BBBB..",
        "..BBBB.....BBBB.",
    ],
}
for code, rows in mario.items():
    S[code] = ("mario", mirror(grid(rows, M)))
stand = grid(mario[0x00], M)
S[0x78] = ("mario", mirror(vflip(stand)))          # dying: upside down
S[0x79] = ("mario", rot_cw(mirror(stand)))          # dying: head to the right
S[0x7A] = ("mario", grid([                          # dead, with a halo
    "................",
    "................",
    "................",
    "................",
    "................",
    "................",
    "..PPPP..........",
    ".PPPPPP.........",
    ".PPPPPP.........",
    "..PPPP..........",
    "................",
    "...RRRB.BB......",
    "..RRRRBBBBRRRBBB",
    ".BPPPRBBBBRRRRBB",
    ".BPPPPBBRRRRRRBB",
    "..PPP..PP.RRR...",
], M))

# Pauline (facing right): her top half, her lower half in three poses, carried by Kong
P_TOP = "OWP"   # orange hair, skin, pink dress
S[0x10] = ("paul_top", grid([
    "................",
    "................",
    ".......OOOO.....",
    "......OOOOOO....",
    ".....OOOOWWO....",
    "...OOOOOWWWWW...",
    "..OOOOOOWWOWW...",
    "..OO.OOOWWWW....",
    ".OO...OOPWW.....",
    ".O.....PPPPW....",
    "......PPPPPPWW..",
    ".....PPPPPPPP...",
    ".....PPPPPP.....",
    "......PPPPP.....",
    ".....PPPPPPP....",
    ".....PPPPPPPP...",
], P_TOP))
P_LOW = "PWB"   # pink dress, white trim, blue shoes
S[0x11] = ("paul_low", grid([
    ".....PPPPPPPP...",
    "....PPPPPPPPPP..",
    "....PPPPPPPPPP..",
    "...PPPPPPPPPPPP.",
    "...PPPWPPPPPPPP.",
    "..PPPPWWPPPPPPP.",
    "..PPPPPWWWPPPPP.",
    ".PPPPPPPPWWWWW..",
    ".PPPPPPPPPPPP...",
    "....WW...WW.....",
    "...BBB...BB.....",
    "..BBBB...BBB....",
    "..BB......BB....",
], P_LOW))
S[0x12] = ("paul_low", grid([
    ".....PPPPPPPP...",
    "....PPPPPPPPPP..",
    "....PPPPPPPPPPP.",
    "...PPPPPPPPPPPPP",
    "...PPWPPPPPPPPPP",
    "..PPPPWWPPPPPPP.",
    "..PPPPPPWWWPPP..",
    "..PPPPPPPPWWW...",
    "...PPPPPPPPP....",
    ".....WW..WW.....",
    "....BBB..BB.....",
    "...BBBB..BBB....",
    "...BB.....BB....",
], P_LOW))
S[0x13] = ("paul_low", grid([
    "....PPPPPPPPP...",
    "...PPPPPPPPPPP..",
    "...PPPPPPPPPPP..",
    "..PPPPPPPPPPPPP.",
    "..PPPWPPPPPWPPP.",
    ".PPPPWWPPPWWPPPP",
    ".PPPPPWWWWWPPPPP",
    "PPPPPPPPPPPPPPPP",
    ".PPPPPPPPPPPPPP.",
    ".....WW..WW.....",
    "....BBB..BBB....",
    "...BBBB..BBBB...",
    "...BB......BB...",
], P_LOW))
S[0x14] = ("paul_low", grid([  # carried under Kong's arm
    "......PPPPP.....",
    ".....PPPPPPP....",
    ".....PPPPPPP....",
    "....PPPPPPPPP...",
    "...PPPPPPPPPPP..",
    "...PPPWPPPPWPP..",
    "..PPPPWWPPWWPPP.",
    "..PPPPPPWWPPPPP.",
    "...PPPPPPPPPPP..",
    ".....WW...WW....",
    ".....WW....WW...",
    "....BBB.....BB..",
    "....BB......BBB.",
], P_LOW))

# barrels: rolling (turned a quarter at a time by flips), falling down a ladder (seen from the side),
# standing (Kong's pile), and the blue one
B = "OPL"   # orange, peach, blue
S[0x15] = ("barrel", grid([
    "................",
    "................",
    ".....PPPPPP.....",
    "...PPOOOOOOPP...",
    "..POOOOOOOLLOP..",
    "..POOOOOOLOLOP..",
    ".POOOOOOLOOOOOP.",
    ".POOOOOLOOOOOOP.",
    ".POOOOLOOOOOOOP.",
    "..POLLOOOOOOOP..",
    "..POLLOOOOOOOP..",
    "...PPOOOOOOPP...",
    ".....PPPPPP.....",
], B))
SIDE = [
    "................",
    "................",
    "..PPPPPPPPPPPP..",
    ".PLLLLLLLLLLLLP.",
    ".POOOOOOOOOOOOP.",
    ".POOOOOOOOOOOOP.",
    ".PLLLLLLLLLLLLP.",
    ".POOOOOOOOOOOOP.",
    ".POOOOOOOOOOOOP.",
    ".PLLLLLLLLLLLLP.",
    "..PPPPPPPPPPPP..",
]
S[0x16] = ("barrel", grid(SIDE, B))
S[0x17] = ("barrel", grid([
    "................",
    "................",
    "..PPPPPPPPPPPP..",
    ".PLLLLLLLLLLLLP.",
    ".POOOOOOOOOOOOP.",
    ".POOOPOOOOPOOOP.",
    ".PLLLLLLLLLLLLP.",
    ".POOOOPOOOOPOOP.",
    ".POOOOOOOOOOOOP.",
    ".PLLLLLLLLLLLLP.",
    "..PPPPPPPPPPPP..",
], B))
S[0x18] = ("barrel", grid([
    "....PPPPPPPP....",
    "...PLLLLLLLLP...",
    "..POOOOOOOOOOP..",
    "..POOOPPOOOOOP..",
    "..POOOOOOOOOOP..",
    "..PLLLLLLLLLLP..",
    "..POOOOOOOOOOP..",
    "..POOOOPPOOOOP..",
    "..POOOOOOOOOOP..",
    "..POOOOOOOOOOP..",
    "..PLLLLLLLLLLP..",
    "..POOOOOOOOOOP..",
    "..POOOOOOPPOOP..",
    "..POOOOOOOOOOP..",
    "...PLLLLLLLLP...",
    "....PPPPPPPP....",
], B))
BL = "LCW"  # blue, cyan, white
S[0x19] = ("blue", grid([
    "................",
    "................",
    ".....CCCCCC.....",
    "...CCLLLLLLCC...",
    "..CLLLWWWWLLLC..",
    "..CLLWLLLLWLLC..",
    ".CLLWLLWWLLWLLC.",
    ".CLLWLWLLWLWLLC.",
    ".CLLWLLWWLWLLLC.",
    "..CLLWLLLLWLLC..",
    "..CLLLWWWWLLLC..",
    "...CCLLLLLLCC...",
    ".....CCCCCC.....",
], BL))
S[0x1A] = ("blue", grid([r.replace("P", "C").replace("O", "W").replace("L", "L") for r in SIDE], BL))
S[0x1B] = ("blue", grid([
    "................",
    "................",
    "..CCCCCCCCCCCC..",
    ".CLLLLLLLLLLLLC.",
    ".CWWWWWWWWWWWWC.",
    ".CWWWLWWWWLWWWC.",
    ".CLLLLLLLLLLLLC.",
    ".CWWWWLWWWWLWWC.",
    ".CWWWWWWWWWWWWC.",
    ".CLLLLLLLLLLLLC.",
    "..CCCCCCCCCCCC..",
], BL))

# the hammer: upright over Mario's head, and swung down in front of him (drawn for facing left)
H = "YRW"   # head, handle, shine
S[0x1E] = ("hammer", grid([
    "................",
    "................",
    "................",
    "....YYYYYYYY....",
    "...YWWYYYYYYY...",
    "...YWYYYYYYYY...",
    "...YYYYYYYYYY...",
    "....YYYYYYYY....",
    "......RRRR......",
    "......RRRR......",
    ".......RR.......",
    ".......RR.......",
    ".......RR.......",
    ".......RR.......",
    ".......RR.......",
    ".......RR.......",
], H))
S[0x1F] = ("hammer", grid([
    "................",
    "................",
    "................",
    "................",
    "...YYYY.........",
    "..YWWYYY........",
    "..YWYYYY........",
    "..YYYYYYRR......",
    "..YYYYYYRRRRRRRR",
    "..YYYYYYRRRRRRRR",
    "..YYYYYY........",
    "..YYYYYY........",
    "...YYYY.........",
    "................",
    "................",
    "................",
], H))

# fires (0x3D/0x3E) and firefoxes on 100m (0x4D/0x4E): a living flame with eyes
F = "YRW"   # body, flame / eyes, white
S[0x3D] = ("fire", grid([
    "..........R.....",
    ".......R..RR....",
    "......RR.RRR....",
    "......RRRRYR..R.",
    ".....RRYYRYRRRR.",
    "....RRYYYYYYYR..",
    "...RRYYYYYYYYRR.",
    "...RYYWWYYWWYYR.",
    "..RRYYWRYYWRYYR.",
    "..RYYYWRYYWRYYR.",
    "..RYYYYYYYYYYYR.",
    "..RYYYYYYYYYYYR.",
    "..RRYYYYYYYYYRR.",
    "...RRYYYYYYYRR..",
    "....RRRYYYRRR...",
    "......RRRRR.....",
], F))
S[0x3E] = ("fire", grid([
    "................",
    "...R.......R....",
    "...RR..R..RR....",
    "...RRR.RRRRR....",
    "....RYRRYYRR.R..",
    "....RRYYYYYRRR..",
    "...RRYYYYYYYYRR.",
    "...RYYWWYYWWYYR.",
    "..RRYYWRYYWRYYR.",
    "..RYYYWRYYWRYYR.",
    "..RYYYYYYYYYYYR.",
    "..RYYYYYYYYYYYR.",
    "..RRYYYYYYYYYRR.",
    "...RRYYYYYYYRR..",
    "....RRRYYYRRR...",
    "......RRRRR.....",
], F))
S[0x4D] = ("fire", grid([
    ".......R........",
    "......RR....R...",
    "......RRR..RR...",
    ".....RRYRRRR....",
    "....RRYYYYYRR...",
    "...RRYYYYYYYRR..",
    "...RYYWWYYWWYR..",
    "..RRYYWRYYWRYR..",
    "..RYYYYYYYYYYRR.",
    "..RYYYYRRYYYYYR.",
    "..RYYYYYYYYYYYR.",
    "..RRYYYYYYYYYRR.",
    "...RRYYYYYYYRR..",
    "...RR.RRRRR.RR..",
    "..RR...R.R...RR.",
    "..R....R.R....R.",
], F))
S[0x4E] = ("fire", grid([
    "............R...",
    "...R.......RR...",
    "...RR..R..RRR...",
    "....RRRRRRYR....",
    "....RRYYYYYRR...",
    "...RRYYYYYYYRR..",
    "...RYYWWYYWWYR..",
    "..RRYYWRYYWRYR..",
    "..RYYYYYYYYYYRR.",
    "..RYYYYRRYYYYYR.",
    "..RYYYYYYYYYYYR.",
    "..RRYYYYYYYYYRR.",
    "...RRYYYYYYYRR..",
    "....RRRRRRRRR...",
    "...RR.R...R.RR..",
    "...R..R...R..R..",
], F))

# the oil drum, its flames, the drum on 50m is the same
DR = "LCW"
S[0x49] = ("blue", grid([
    "................",
    "LLLLLLLLLLLLLLLL",
    "LCCCCCCCCCCCCCCL",
    "LCLLLLLLLLLLLLLL",
    "LCLLLLLLLLLLLLLL",
    "LCWWWLWWWLWLLLLL",
    "LCWLWLLWLLWLLLLL",
    "LCWLWLLWLLWLLLLL",
    "LCWLWLLWLLWLLLLL",
    "LCWWWLWWWLWWWLLL",
    "LCLLLLLLLLLLLLLL",
    "LCLLLLLLLLLLLLLL",
    "LCCCCCCCCCCCCCCL",
    "LCLLLLLLLLLLLLLL",
    "LCLLLLLLLLLLLLLL",
    "LLLLLLLLLLLLLLLL",
], DR))
FL = "RYW"
flames = [
    [
        "................",
        "................",
        "................",
        "................",
        "................",
        "..........R.....",
        "....R.....R.....",
        "....R....RR.....",
        "...RR..R.RRR..R.",
        "...RRR.RRRYR..R.",
        "..RRYRRRYRYRRRR.",
        "..RYYRRYYYYRRYR.",
        ".RRYYYRYYWYYRYR.",
        ".RYYWYYYWWYYYYR.",
        ".RYWWYYYWWWYWYR.",
        ".RYWWWYYWWWYWWR.",
    ],
    [
        "................",
        "................",
        "................",
        "................",
        "................",
        "......R.........",
        "......R....R....",
        ".R....RR...R....",
        ".R...RRR..RR.R..",
        ".RR..RYR..RRRR..",
        ".RRRRRYRRRYRR...",
        "..RYYRYYRYYYRR..",
        ".RRYYYYYYYWYYR..",
        ".RYYWYYYWWWYYRR.",
        ".RYWWWYYWWWYWYR.",
        ".RYWWWYWWWWYWWR.",
    ],
]
S[0x40] = ("flame", grid(flames[0], FL))
S[0x41] = ("flame", grid(flames[1], FL))
big = [
    [
        "................",
        "......R.....R...",
        "..R...R....RR...",
        "..R..RR.R..RR.R.",
        "..RR.RRRR.RRR.R.",
        ".RRR.RYRRRRYRRR.",
        ".RYRRRYYRRYYRYR.",
        ".RYYRYYYRYYYRYR.",
        "RRYYYYWYYYWYYYRR",
        "RYYWYYWYYYWWYYYR",
        "RYYWWYYWYWWWYWYR",
        "RYWWWYYWWWWWYWYR",
        "RYWWWYWWWWWWYWWR",
        "RYWWWWWWWWWWWWWR",
        ".RYWWWWWWWWWWWR.",
        "..RRYWWWWWWWYR..",
    ],
]
S[0x42] = ("flame", grid(big[0], FL))
S[0x43] = ("flame", mirror(grid(big[0], FL)))

# springs on 75m: open and squashed
SP = "LRW"
S[0x3B] = ("spring", grid([
    "................",
    "...LLLLLLLLLL...",
    "....LLLLLLLL....",
    ".....R....R.....",
    "......R..R......",
    ".......RR.......",
    "......R..R......",
    ".....R....R.....",
    "......R..R......",
    ".......RR.......",
    "......R..R......",
    ".....R....R.....",
    "....LLLLLLLL....",
    "...LLLLLLLLLL...",
    "................",
    "................",
], SP))
S[0x3C] = ("spring", grid([
    "................",
    "................",
    "................",
    "................",
    "................",
    "...LLLLLLLLLL...",
    "....LLLLLLLL....",
    "....RRRRRRRR....",
    ".....R.RR.R.....",
    "....RRRRRRRR....",
    ".....R.RR.R.....",
    "....RRRRRRRR....",
    "....LLLLLLLL....",
    "...LLLLLLLLLL...",
    "................",
    "................",
], SP))

# 75m: an elevator's platform (a short girder) and the shafts' motors
EL = "ADT"
S[0x44] = ("elevator", grid([
    "................",
    "................",
    "................",
    "................",
    "AAAAAAAAAAAAAAAA",
    "DDDDDDDDDDDDDDDD",
    "...TT......TT...",
    "..T..T....T..T..",
    ".T....T..T....T.",
    "T......TT......T",
    "AAAAAAAAAAAAAAAA",
    "DDDDDDDDDDDDDDDD",
], EL))
MO = "YRO"
S[0x45] = ("motor", grid([
    "................",
    "..YYYYYYYYYYYY..",
    ".YYYYYYYYYYYYYY.",
    ".YRRRRRRRRRRRRY.",
    ".YROOOOOOOOOORY.",
    ".YROROROROROORY.",
    ".YROOOOOOOOOORY.",
    ".YRRRRRRRRRRRRY.",
    ".YYYYYYYYYYYYYY.",
    "...YYYYYYYYYY...",
    ".....YYRRYY.....",
    "......YRRY......",
    ".......RR.......",
    ".......RR.......",
    ".......RR.......",
    ".......RR.......",
], MO))

# 50m: the ladders that slide (a sprite there), the pies, the belts' pulleys
S[0x46] = ("ladder_white", grid([
    "...A........A...",
    "...AAAAAAAAAA...",
    "...A........A...",
    "...A........A...",
    "...A........A...",
    "...AAAAAAAAAA...",
    "...A........A...",
    "...A........A...",
    "...A........A...",
    "...AAAAAAAAAA...",
    "...A........A...",
    "...A........A...",
    "...A........A...",
    "...AAAAAAAAAA...",
    "...A........A...",
    "...A........A...",
], "A"))
PI = "PBO"
S[0x4B] = ("pie", grid([
    "................",
    "................",
    "................",
    "................",
    "......PPPP......",
    "....PPPOPPPP....",
    "...PPOPPPPOPP...",
    "..PPPPPPOPPPPP..",
    ".PPOPPPPPPPOPPP.",
    ".BBBBBBBBBBBBBB.",
    "..BBBBBBBBBBBB..",
    "................",
], PI))
S[0x4C] = ("pie", grid([
    "................",
    "................",
    "................",
    "................",
    "................",
    "....PPPPPPPP....",
    "..PPPOPPPPOPPP..",
    ".PPPPPPOPPPPPPP.",
    "PPOPPPPPPPPPOPPP",
    "BBBBBBBBBBBBBBBB",
    ".BBBBBBBBBBBBBB.",
    "................",
], PI))
PU = "LRC"
pulley = [
    "................",
    "....LLLLL.......",
    "...LLLLLLL......",
    "..LLLRRRLLL.....",
    "..LLRRCCRLL.....",
    ".LLRRCCRRRLL....",
    ".LLRCCRRRRLL....",
    ".LLRCRRRRRLL....",
    "..LLRRRRRLL.....",
    "...LLLLLLL......",
    "....LLLLL.......",
]
S[0x50] = ("pulley", grid(pulley, PU))
S[0x51] = ("pulley", grid([r.replace("C", "x").replace("R", "C").replace("x", "R") for r in pulley], PU))
S[0x52] = ("pulley", vflip(grid(pulley, PU)))

# a hammer's hit: a ring, a smaller ring, a burst
SM = "LCW"
S[0x60] = ("smash", grid([
    "................",
    ".....WWWWWW.....",
    "...WWCCCCCCWW...",
    "..WCC......CCW..",
    "..WC........CW..",
    ".WC..........CW.",
    ".WC..........CW.",
    ".WC..........CW.",
    ".WC..........CW.",
    ".WC..........CW.",
    "..WC........CW..",
    "..WCC......CCW..",
    "...WWCCCCCCWW...",
    ".....WWWWWW.....",
], SM))
S[0x61] = ("smash", grid([
    "................",
    "................",
    "................",
    "......WWWW......",
    ".....WCCCCW.....",
    "....WC....CW....",
    "....WC....CW....",
    "....WC....CW....",
    "....WC....CW....",
    ".....WCCCCW.....",
    "......WWWW......",
], SM))
S[0x62] = ("smash", grid([
    "................",
    "................",
    "................",
    "................",
    "................",
    "......WWWW......",
    ".....WCCCCW.....",
    ".....WC..CW.....",
    ".....WCCCCW.....",
    "......WWWW......",
], SM))
S[0x63] = ("smash", grid([
    "................",
    "...W.......W....",
    "....W..W..W.....",
    ".....W.W.W......",
    "..W...CCC...W...",
    "...W.CLLLC.W....",
    ".....CLWLC......",
    ".WWW.CLLLC.WWW..",
    ".....CLWLC......",
    "...W.CLLLC.W....",
    "..W...CCC...W...",
    ".....W.W.W......",
    "....W..W..W.....",
    "...W.......W....",
], SM))

# Pauline's things and the heart
IT = "PWB"
S[0x73] = ("items", grid([
    "................",
    "................",
    "................",
    "................",
    "................",
    "................",
    "......WWWW......",
    ".....WWWWWW.....",
    ".....WPPPPW.....",
    "....WWWWWWWW....",
    ".PPWWWWWWWWWWPP.",
    "..PPPPPPPPPPPP..",
    "................",
    "................",
    "................",
    "................",
], IT))
S[0x74] = ("items", grid([
    "................",
    "................",
    "................",
    "................",
    "......PPPP......",
    ".....P....P.....",
    ".....P....P.....",
    "...PPPPPPPPPP...",
    "..PPWWPPPPPPPP..",
    "..PWPPPPPPPPPP..",
    "..PPPPPWWPPPPP..",
    "..PPPPPWWPPPPP..",
    "..PPPPPPPPPPPP..",
    "...PPPPPPPPPP...",
    "................",
    "................",
], IT))
S[0x75] = ("items", grid([
    "................",
    ".......B........",
    ".....PPWPP......",
    "...PPWWPPWWPP...",
    "..PPWWPPPPWWPP..",
    ".PPWWPPPPPPWWPP.",
    ".WPPWPPWPPWPPWW.",
    ".W..W..W..W...W.",
    ".......W........",
    ".......W........",
    ".......W........",
    ".......W........",
    ".......W........",
    ".......W..W.....",
    "........WW......",
    "................",
], IT))
HE = "OWP"
heart = [
    "................",
    "................",
    "..PPPP....PPPP..",
    ".PPWWPP..PPPPPP.",
    "PPWWPPPPPPPPPPPP",
    "PPWPPPPPPPPPPPPP",
    "PPPPPPPPPPPPPPPP",
    "PPPPPPPPPPPPPPPP",
    ".PPPPPPPPPPPPPP.",
    "..PPPPPPPPPPPP..",
    "...PPPPPPPPPP...",
    "....PPPPPPPP....",
    ".....PPPPPP.....",
    "......PPPP......",
    ".......PP.......",
]
S[0x76] = ("heart", grid(heart, HE))
broken = [[0] * 16 for _ in range(16)]
whole = grid(heart, HE)
crack = [7, 7, 8, 8, 7, 6, 7, 8, 9, 8, 7, 7, 8, 8, 7, 7]   # a jagged line down the middle
for y in range(16):
    for x in range(16):
        if not whole[y][x] or x == crack[y]:
            continue
        nx = x - 1 if x < crack[y] else x + 1               # the two halves apart
        if 0 <= nx < 16:
            broken[y][nx] = whole[y][x]
S[0x77] = ("heart", broken)

# the points: 100, 200, 300, 500, 800 (in Kong's face colours, white digits)
DIG = {
    "0": ["###", "#.#", "#.#", "#.#", "###"],
    "1": [".#.", "##.", ".#.", ".#.", "###"],
    "2": ["###", "..#", "###", "#..", "###"],
    "3": ["###", "..#", ".##", "..#", "###"],
    "5": ["###", "#..", "###", "..#", "###"],
    "8": ["###", "#.#", "###", "#.#", "###"],
}
for code, txt in ((0x7B, "100"), (0x7C, "200"), (0x7D, "300"), (0x7E, "500"), (0x7F, "800")):
    g = [[0] * 16 for _ in range(16)]
    for i, ch in enumerate(txt):
        for y, row in enumerate(DIG[ch]):
            for x, c in enumerate(row):
                if c == "#":
                    g[5 + y][2 + i * 4 + x] = 3
    S[code] = ("points", g)

S[0x3A] = ("black", [[1] * 16 for _ in range(16)])   # a black square (hides the girders over Kong's ladders)
S[0x39] = ("dizzy", grid([                         # stars over Kong's head (100m)
    "................",
    "..W.........Y...",
    ".WWW.......YYY..",
    "..W.........Y...",
    "................",
    "......C.........",
    ".....CCC........",
    "......C......W..",
    ".............W..",
    "...Y.......WWWWW",
    "..YYY........W..",
    "...Y.........W..",
], "WYC"))

# ---------------------------------------------------------------- Kong, in parts placed by the game's pose tables
K = "BPO"   # body, peach, orange
KF = "BPW"  # body, peach, white (his face)
# front view, left half (columns 0..19 of 40), assembled by mirroring for the right half
KONG_PARTS = {
    0x23: ("kface", [  # the face (centre top)
        ".....BBBBBB.....",
        "...BBBBBBBBBB...",
        "..BBBBBBBBBBBB..",
        ".BBBPPPPPPPPBBB.",
        ".BBPPPPPPPPPPBB.",
        ".BPPWWPPPPWWPPB.",
        ".BPPBWPPPPWBPPB.",
        ".BPPPPPPPPPPPPB.",
        "BBBPPPPBBPPPPBBB",
        "BBPPPPPPPPPPPPBB",
        "BBPBBBBBBBBBBPBB",
        "BBPPPPPPPPPPPPBB",
        "BBBPPPPPPPPPPBBB",
        ".BBBPPPPPPPPBBB.",
        "..BBBBBBBBBBBB..",
        "....BBBBBBBB....",
    ], KF),
    0x24: ("kface", [  # growling, teeth out
        ".....BBBBBB.....",
        "...BBBBBBBBBB...",
        "..BBBBBBBBBBBB..",
        ".BBBPPPPPPPPBBB.",
        ".BBPBBPPPPBBPBB.",
        ".BPPWWBPPBWWPPB.",
        ".BPPBWPPPPWBPPB.",
        ".BPPPPPPPPPPPPB.",
        "BBBPPPPBBPPPPBBB",
        "BBPBBBBBBBBBBPBB",
        "BBPBWWWWWWWWBPBB",
        "BBPBBBBBBBBBBPBB",
        "BBPBWWWWWWWWBPBB",
        ".BBBBBBBBBBBBBB.",
        "..BBBBBBBBBBBB..",
        "....BBBBBBBB....",
    ], KF),
    0x25: ("kong", [  # chest and hips (centre bottom)
        "...BBPPPPPPBB...",
        "..BBPPPPPPPPBB..",
        "..BPPPPPPPPPPB..",
        "..BPPPPOOPPPPB..",
        "..BPPPPPPPPPPB..",
        "..BBPPPPPPPPBB..",
        "...BBPPPPPPBB...",
        "...BBBPPPPBBB...",
        "...BBBBPPBBBB...",
        "...BBBBBBBBBB...",
        "...BBBBBBBBBB...",
        "....BBB..BBB....",
        "....BB....BB....",
        "................",
        "................",
        "................",
    ], K),
    0x27: ("kong", [  # his right leg, standing (right of centre)
        "................",
        "................",
        "....BBBBBB......",
        "...BBBBBBBBB....",
        "...BBBBBBBBBB...",
        "...BBBBBBBBBBB..",
        "....BBBBBBBBBB..",
        ".....BBBBBBBBB..",
        "......BBBBBBBB..",
        ".......BBBBBBB..",
        "........BBBBBB..",
        "........BBBBBB..",
        "........OBBBBO..",
        ".......PPPPPPPP.",
        "......PPPPPPPPPP",
        "......PPP.PPP.PP",
    ], K),
    0x26: ("kong", [  # his left leg, lifted (left of centre)
        "................",
        "................",
        "................",
        "................",
        ".......BBBBBB...",
        ".....BBBBBBBBB..",
        "...BBBBBBBBBBB..",
        "..BBBBBBBBBBBB..",
        ".BBBBBBBBBBBB...",
        ".BBBBBBBBB......",
        ".BBBBBB.........",
        ".OBBBBO.........",
        "PPPPPPPP........",
        "PPPPPPPPP.......",
        "P.PPP.PPP.......",
        "................",
    ], K),
    0x29: ("kong", [  # his left arm, hanging (left)
        "........BBB.....",
        "......BBBBBBB...",
        ".....BBBBBBBBB..",
        "....BBBBBBBBBB..",
        "...BBBBBBBBBBB..",
        "...BBBBOBBBBB...",
        "..BBBBBOOBBB....",
        "..BBBBBBOBB.....",
        "..BBBBBBBB......",
        "..BBBBBBBB......",
        "..BBBBBBB.......",
        "..BBBBBBB.......",
        "..OBBBBBO.......",
        ".PPPPPPPP.......",
        ".PPPPPPPPP......",
        ".PPP.PP.PP......",
    ], K),
    0x28: ("kong", [  # his right arm, raised to beat his chest (right)
        "......PPPP......",
        ".....PPPPPP.....",
        ".....PPPPPP.....",
        "......OBBO......",
        "......BBBB......",
        ".....BBBBBB.....",
        "....BBBBBBB.....",
        "...BBBBOBBB.....",
        "..BBBBOBBBB.....",
        ".BBBBOBBBB......",
        "BBBBBBBBB.......",
        "BBBBBBBB........",
        "BBBBBBB.........",
        "BBBBBB..........",
        ".BBB............",
        "................",
    ], K),
    # holding a barrel in front (0x395A): upper and lower arm, drawn for his right side
    0x2A: ("kong", [
        "................",
        "................",
        "................",
        "................",
        "..BBBBB.........",
        ".BBBBBBBB.......",
        "BBBBBBBBBB......",
        "BBBBBBBBBBB.....",
        "BBBBBBBBBBBB....",
        ".BBBBBBBBBBBB...",
        "..BBBBBBBBBBB...",
        "....BBBBBBBBB...",
        "......BBBBBBB...",
        ".......BBBBBB...",
        ".......BBBBBB...",
        ".......BBBBB....",
    ], K),
    0x2B: ("kong", [
        ".......BBBBB....",
        "......BBBBB.....",
        ".....BBBBB......",
        "....OBBBO.......",
        "..PPPPPP........",
        ".PPPPPPP........",
        ".PPPPPPP........",
        "..PPPPP.........",
        "................",
        "................",
        "................",
        "................",
        "................",
        "................",
        "................",
        "................",
    ], K),
}
# side view, facing left, taking a barrel from the pile (0x3982): a 3x2 block of parts, cut from one picture
def side_kong():
    g = [["."] * 48 for _ in range(32)]
    def run(y, x0, x1, ch="B"):
        for x in range(x0, x1 + 1):
            g[y][x] = ch
    head = {0: (6, 10), 1: (4, 12), 2: (3, 13), 3: (2, 14), 4: (2, 14), 5: (1, 14), 6: (1, 14), 7: (0, 14), 8: (0, 14),
            9: (0, 14), 10: (0, 14), 11: (0, 14), 12: (1, 14), 13: (2, 14), 14: (3, 14), 15: (5, 14)}
    back = {0: (23, 28), 1: (20, 30), 2: (18, 31), 3: (16, 31), 22: (15, 30), 23: (17, 29)}
    for y in range(4, 16):
        back[y] = (14, 31)
    for y in range(16, 22):
        back[y] = (11, 31)
    rump = {16: (28, 38), 17: (27, 41), 18: (27, 43), 19: (27, 44), 20: (27, 45), 21: (28, 45), 22: (29, 45),
            23: (30, 45), 24: (32, 45), 25: (34, 45), 26: (36, 45), 27: (37, 45)}
    arm = {13: (9, 16), 14: (8, 16), 15: (7, 15), 16: (6, 14), 17: (5, 13), 18: (4, 12), 19: (3, 11), 20: (2, 10),
           21: (2, 9), 22: (1, 8), 23: (1, 8)}
    for part in (back, rump, arm, head):
        for y, (x0, x1) in part.items():
            run(y, x0, x1)
    for y, (x0, x1) in {6: (1, 6), 7: (0, 7), 8: (0, 8), 9: (0, 8), 10: (0, 8), 11: (0, 8), 12: (1, 7)}.items():
        run(y, x0, x1, "P")   # his muzzle
    run(10, 1, 7)             # the mouth
    g[4][4] = g[4][5] = g[5][4] = "W"
    for y, (x0, x1) in {24: (0, 8), 25: (0, 9), 26: (0, 9), 27: (0, 8)}.items():
        run(y, x0, x1, "P")   # his hand on the barrel
    for x in (0, 2, 3, 5, 6):
        g[28][x] = "P"
    g[23][1] = g[23][8] = "O"
    for y, (x0, x1) in {28: (36, 46), 29: (35, 47), 30: (34, 47)}.items():
        run(y, x0, x1, "P")   # his foot
    for x in (34, 36, 37, 39, 40, 42, 43, 45, 46):
        g[31][x] = "P"
    g[27][37] = g[27][45] = "O"
    for (y, x0, x1) in ((3, 19, 26), (4, 17, 18), (4, 27, 28), (5, 16, 16), (5, 29, 29), (18, 33, 38), (19, 31, 32),
                        (19, 39, 40), (16, 9, 9), (17, 8, 8), (18, 7, 7), (19, 6, 6)):
        run(y, x0, x1, "O")   # his fur catching the light
    return ["".join(r) for r in g]


SIDE_ROWS = side_kong()
side = [[r[0:16], r[16:32], r[32:48]] for r in SIDE_ROWS]
SIDE_CODES = {0x30: (0, 0, "kface"), 0x2F: (1, 0, "kong"), 0x2D: (0, 1, "kong"), 0x2E: (1, 1, "kong"),
              0x2C: (2, 1, "kong")}
# climbing his ladders, seen from behind (0x388C): both halves of his back, an arm up and one down (mirrored in
# turn by the game), his feet, and his right arm round Pauline
BACK = [
    "...........BBBBBBBBBB...........",
    ".........BBBBBBBBBBBBBB.........",
    "........BBBBBBBBBBBBBBBB........",
    "........BBBBBBBBBBBBBBBB........",
    "...BBBBBBBBBBBBBBBBBBBBBBBBBB...",
    ".BBBBBBBBBBBBBBBBBBBBBBBBBBBBBB.",
    "BBBBBOOBBBBBBBBBBBBBBBBBBOOBBBBB",
    "BBBBOBBBBBBBBBBBBBBBBBBBBBBOBBBB",
    "BBBBBBBBBBBBBBBOOBBBBBBBBBBBBBBB",
    ".BBBBBBBBBBBBBBOOBBBBBBBBBBBBBB.",
    "..BBBBBBBBBBBBBOOBBBBBBBBBBBBB..",
    "...BBBBBBBBBBBBBBBBBBBBBBBBBB...",
    "....BBBBBBBBBBBBBBBBBBBBBBBB....",
    "....BBBBBBBBBBBBBBBBBBBBBBBB....",
    ".....BBBBBBBBBBBBBBBBBBBBBB.....",
    "......BBBBBBBBBBBBBBBBBBBB......",
]
CLIMB = {
    0x32: [r[:16] for r in BACK],
    0x33: [r[16:] for r in BACK],
    0x34: [  # left arm up, on a rung
        ".....PPPPPP.....",
        "....PPPPPPPP....",
        "....PPPPPPPP....",
        ".....OBBBBO.....",
        ".....BBBBBB.....",
        ".....BBBBBB.....",
        ".....BBBBBBB....",
        "....BBBBBBBB....",
        "....BBBBBBBBB...",
        "....BBBBBBBBB...",
        "....BBBBBBBB....",
        ".....BBBBBB.....",
    ],
    0x35: [  # right arm down, on a lower rung
        "................",
        "................",
        "................",
        "................",
        "................",
        "................",
        "................",
        ".....BBBBBB.....",
        ".....BBBBBBB....",
        "....BBBBBBBB....",
        "....BBBBBBBB....",
        "....OBBBBBBO....",
        "...PPPPPPPP.....",
        "...PPPPPPPP.....",
        "....PPPPPP......",
    ],
    0x36: [  # left foot, down on a rung
        ".....BBBBBBBBB..",
        ".....BBBBBBBBB..",
        "....BBBBBBBBB...",
        "....BBBBBBBB....",
        "....BBBBBBB.....",
        "...BBBBBBB......",
        "...BBBBBB.......",
        "...BBBBBB.......",
        "...OBBBBO.......",
        "..PPPPPPPP......",
        "..PPPPPPPPP.....",
        "..PP.PP.PP......",
    ],
    0x37: [  # right foot, up on the next rung
        "..BBBBBBBBB.....",
        "..BBBBBBBBBB....",
        "...BBBBBBBBBB...",
        "....BBBBBBBBB...",
        ".....OBBBBBBO...",
        "....PPPPPPPPP...",
        "....PPPPPPPPP...",
        ".....PP.PP.PP...",
    ],
    0x31: [  # his right arm round Pauline
        "................",
        "................",
        "................",
        "..BBBBB.........",
        "..BBBBBBB.......",
        "..BBBBBBBBB.....",
        "...BBBBBBBBB....",
        "....BBBBBBBOO...",
        "......BBBBPPPP..",
        "........PPPPPPP.",
        "........PPPPPPP.",
        ".........PPPPP..",
    ],
}
for code, (pal, rows, keys) in {c: (v[0], v[1], v[2]) for c, v in KONG_PARTS.items()}.items():
    S[code] = (pal, grid(rows, keys))
for code, (cx, cy, pal) in SIDE_CODES.items():
    rows = [side[cy * 16 + y][cx] for y in range(16)]
    S[code] = (pal, grid(rows, KF if pal == "kface" else K))
for code, rows in CLIMB.items():
    S[code] = ("kong", grid(rows, K))
# dizzy faces after his fall (shown upside down by the game)
for i, code in enumerate((0x20, 0x21, 0x22)):
    face = [list(r) for r in grid(KONG_PARTS[0x24][1], KF)]
    eyes = [(5, 4), (5, 10)]
    spin = [[(0, 0), (1, 1)], [(0, 1), (1, 0)], [(0, 0), (1, 0)]][i]
    for (ey, ex) in eyes:
        for yy in range(2):
            for xx in range(2):
                face[ey + yy][ex + xx] = 2
        for (dy, dx) in spin:
            face[ey + dy][ex + dx] = 3
    S[code] = ("kface", face)

# ---------------------------------------------------------------- tiles (8x8, 2 bits): drawn by the renderer
TILES = {
    "girder": [  # 1 rail, 2 its edge, 3 the truss (25m, 75m, 50m)
        "11111111",
        "22222222",
        "...33...",
        "..3..3..",
        ".3....3.",
        "33333333",
        "11111111",
        "22222222",
    ],
    "rivet_girder": [  # 100m: 1 the top edge, 2 the beam, 3 its holes
        "11111111",
        "22222222",
        "23322332",
        "33333333",
        "33333333",
        "23322332",
        "22222222",
        "22222222",
    ],
    "belt": [  # 50m: 1 orange, 2 peach
        "11111111",
        "22222222",
        "2....2..",
        "2....2..",
        "2....2..",
        "2....2..",
        "22222222",
        "11111111",
    ],
    "ladder": [
        "1......1",
        "11111111",
        "1......1",
        "1......1",
        "1......1",
        "11111111",
        "1......1",
        "1......1",
    ],
    "rivet_top": [  # B8: the rivet's head over the girder
        "........",
        "........",
        "........",
        "........",
        "........",
        "..1111..",
        ".111111.",
        ".111111.",
    ],
    "rivet_low": [  # B7: its foot in the girder
        "33111133",
        "33111133",
        "23111132",
        "33111133",
        "33111133",
        "23111132",
        "22111122",
        "22111122",
    ],
    "cross": [  # FE: the lattice under the 50m drum
        "1......1",
        ".1....1.",
        "..1..1..",
        "...11...",
        "...11...",
        "..1..1..",
        ".1....1.",
        "1......1",
    ],
}

# letters: Bobydeluxe's 5x7 font from his adaptation (columns, bit 0 at the top), drawn bold
FONT5 = [
    0x7e, 0x11, 0x11, 0x11, 0x7e, 0x7f, 0x49, 0x49, 0x49, 0x36, 0x3e, 0x41, 0x41, 0x41, 0x22, 0x7f, 0x41, 0x41, 0x22, 0x1c,
    0x7f, 0x49, 0x49, 0x49, 0x41, 0x7f, 0x09, 0x09, 0x09, 0x01, 0x3e, 0x41, 0x49, 0x49, 0x7a, 0x7f, 0x08, 0x08, 0x08, 0x7f,
    0x00, 0x41, 0x7f, 0x41, 0x00, 0x20, 0x40, 0x41, 0x3f, 0x01, 0x7f, 0x08, 0x14, 0x22, 0x41, 0x7f, 0x40, 0x40, 0x40, 0x40,
    0x7f, 0x02, 0x0c, 0x02, 0x7f, 0x7f, 0x04, 0x08, 0x10, 0x7f, 0x3e, 0x41, 0x41, 0x41, 0x3e, 0x7f, 0x09, 0x09, 0x09, 0x06,
    0x3e, 0x41, 0x51, 0x21, 0x5e, 0x7f, 0x09, 0x19, 0x29, 0x46, 0x46, 0x49, 0x49, 0x49, 0x31, 0x01, 0x01, 0x7f, 0x01, 0x01,
    0x3f, 0x40, 0x40, 0x40, 0x3f, 0x1f, 0x20, 0x40, 0x20, 0x1f, 0x3f, 0x40, 0x38, 0x40, 0x3f, 0x63, 0x14, 0x08, 0x14, 0x63,
    0x07, 0x08, 0x70, 0x08, 0x07, 0x61, 0x51, 0x49, 0x45, 0x43,
]
DIGITS5 = [0x3e, 0x51, 0x49, 0x45, 0x3e, 0x00, 0x42, 0x7f, 0x40, 0x00, 0x62, 0x51, 0x49, 0x49, 0x46, 0x22, 0x41, 0x49,
           0x49, 0x36, 0x18, 0x14, 0x12, 0x7f, 0x10, 0x27, 0x45, 0x45, 0x45, 0x39, 0x3c, 0x4a, 0x49, 0x49, 0x30, 0x01,
           0x71, 0x09, 0x05, 0x03, 0x36, 0x49, 0x49, 0x49, 0x36, 0x06, 0x49, 0x49, 0x29, 0x1e]
EXTRA5 = {".": [0, 0x60, 0x60, 0, 0], "-": [0, 0x08, 0x08, 0x08, 0], ":": [0, 0x36, 0x36, 0, 0],
          "=": [0, 0x14, 0x14, 0x14, 0], "!": [0, 0, 0x5f, 0, 0], "?": [0x02, 0x01, 0x51, 0x09, 0x06],
          "'": [0, 0, 0x03, 0, 0], ",": [0, 0x80, 0x60, 0, 0], "<": [0x08, 0x14, 0x22, 0x41, 0],
          ">": [0x41, 0x22, 0x14, 0x08, 0], "/": [0x20, 0x10, 0x08, 0x04, 0x02], "m": [0x7c, 0x04, 0x78, 0x04, 0x78],
          "(": [0, 0x1c, 0x22, 0x41, 0], ")": [0, 0x41, 0x22, 0x1c, 0]}


def glyph(ch):
    if "A" <= ch <= "Z":
        return FONT5[(ord(ch) - 65) * 5:(ord(ch) - 65) * 5 + 5]
    if "0" <= ch <= "9":
        return DIGITS5[(ord(ch) - 48) * 5:(ord(ch) - 48) * 5 + 5]
    return EXTRA5.get(ch, [0] * 5)


def bold_rows(ch):
    """8 rows of 8 bits: the 5x7 glyph, each column doubled to the right (6 wide), one pixel in."""
    if ch == "m":   # (metres: too thin to double)
        return [0, 0, 0b01101100, 0b01111110, 0b01011010, 0b01011010, 0b01011010, 0]
    cols = glyph(ch)
    rows = []
    for y in range(8):
        bits = 0
        for x, c in enumerate(cols):
            if c >> y & 1:
                bits |= 0x80 >> (x + 1) | 0x80 >> (x + 2)
        rows.append(bits)
    return rows


FONT_CHARS = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ.-:=!?',<>/m()"

# the arcade's tile codes for the letters it prints in the play area
TILE_CHAR = {}
for i in range(10):
    TILE_CHAR[i] = str(i)
    TILE_CHAR[0x70 + i] = str(i)
    TILE_CHAR[0x80 + i] = str(i)
for i in range(26):
    TILE_CHAR[0x11 + i] = chr(65 + i)
TILE_CHAR.update({0x2B: ".", 0x2C: "-", 0x2E: ":", 0x30: "<", 0x31: ">", 0x34: "=", 0x38: "!", 0x39: "!",
                  0x3A: "'", 0x42: ".", 0x43: ",", 0x8A: "m", 0x8B: "m", 0xFB: "?"})

# HELP!, three tiles (0xED-0xEF facing right, 0xDD-0xDF facing left): small italic capitals
HELP = [
    "...........................",
    ".#..#.####.#....###...#.....",
    ".#..#.#....#....#..#..#.....",
    "#..#.###..#....###...#......",
    "####.#....#....#.....#......",
    "#..#.####.####.#.....#......",
    "...........................",
    "....................#.......",
]


def help_tiles():
    rows = [r.ljust(24, ".")[:24] for r in HELP]
    out = []
    for t in range(3):
        out.append([sum(0x80 >> x for x in range(8) if rows[y][t * 8 + x] == "#") for y in range(8)])
    return out


# the little Mario for each life left (tile 0xFF, shown in the side panel)
LIFE = [
    "..RRR...",
    ".RRRRR..",
    ".BPPB...",
    ".PPPPP..",
    "..BRB...",
    ".BBRBB..",
    "..R.R...",
    ".BB.BB..",
]

# ---------------------------------------------------------------- output


def pack2(g, w):
    out = []
    for row in g:
        for x in range(0, w, 4):
            b = 0
            for k in range(4):
                b |= (row[x + k] & 3) << (6 - 2 * k)
            out.append(b)
    return out


def rgb565(c):
    r, g, b = c >> 16 & 255, c >> 8 & 255, c & 255
    return (r >> 3) << 11 | (g >> 2) << 5 | b >> 3


def goofy():
    """The 48x32 Kong of 'HOW HIGH CAN YOU GET?', assembled from the standing pose."""
    canvas = [[0] * 48 for _ in range(32)]
    pose = [(0x47, 0x27, 0, 0x50), (0x2F, 0x27, 1, 0x50), (0x3B, 0x25, 0, 0x50), (0x3B, 0x23, 0, 0x40),
            (0x46, 0x29, 1, 0x44), (0x30, 0x29, 0, 0x44)]
    face_map = {1: 1, 2: 2, 3: 3}
    for (x, code, flip, y) in pose:
        pal, g = S[code]
        if flip:
            g = mirror(g)
        for yy in range(16):
            for xx in range(16):
                v = g[yy][xx]
                if not v:
                    continue
                if pal == "kface" and v == 3:
                    v = 3   # white eyes stay white (3 in the goofy palette)
                elif pal == "kong" and v == 3:
                    v = 1   # orange shading folds into the body colour
                cx, cy = x - 7 + xx - 0x28 + 4, y - 8 + yy - 0x38
                if 0 <= cx < 48 and 0 <= cy < 32:
                    canvas[cy][cx] = face_map[v]
    return canvas


def main():
    out = ["/* Generated by tools/art.py: Donkey Kong's pictures for this port (no Nintendo graphics data). */",
           "#ifndef DK_ART_H", "#define DK_ART_H", "#include <stdint.h>"]
    names = list(PAL)
    out.append("enum { " + ", ".join(f"PAL_{n.upper()}" for n in names) + ", PAL_COUNT };")
    out.append("static const uint16_t dk_pal[PAL_COUNT][3] = {")
    for n in names:
        out.append("  {" + ", ".join(f"0x{rgb565(c):04X}" for c in PAL[n]) + "},")
    out.append("};")
    codes = sorted(S)
    index = [255] * 128
    for i, c in enumerate(codes):
        index[c] = i
    out.append(f"static const uint8_t dk_spr[{len(codes)}][64] = {{")
    for c in codes:
        out.append("  {" + ",".join(f"0x{b:02X}" for b in pack2(S[c][1], 16)) + f"}}, /* {c:02X} */")
    out.append("};")
    out.append(f"static const uint8_t dk_spr_pal[{len(codes)}] = {{" +
               ",".join(f"PAL_{S[c][0].upper()}" for c in codes) + "};")
    out.append("static const uint8_t dk_spr_of[128] = {" + ",".join(str(v) for v in index) + "};")
    for name, rows in TILES.items():
        g = [[0 if ch == "." else int(ch) for ch in r] for r in rows]
        out.append(f"static const uint8_t dk_tile_{name}[16] = {{" + ",".join(f"0x{b:02X}" for b in pack2(g, 8)) + "};")
    out.append(f"static const char dk_font_chars[] = \"{FONT_CHARS}\";")
    out.append(f"static const uint8_t dk_font[{len(FONT_CHARS)}][8] = {{")
    for ch in FONT_CHARS:
        out.append("  {" + ",".join(f"0x{b:02X}" for b in bold_rows(ch)) + "},")
    out.append("};")
    tc = [0] * 256
    for code, ch in TILE_CHAR.items():
        tc[code] = FONT_CHARS.index(ch) + 1
    out.append("static const uint8_t dk_tile_char[256] = {" + ",".join(str(v) for v in tc) + "};")
    out.append("static const uint8_t dk_help[3][8] = {" + ",".join(
        "{" + ",".join(f"0x{b:02X}" for b in t) + "}" for t in help_tiles()) + "};")
    lg = [[0 if ch == "." else "RBP".index(ch) + 1 for ch in r] for r in LIFE]
    out.append("static const uint8_t dk_life[16] = {" + ",".join(f"0x{b:02X}" for b in pack2(lg, 8)) + "};")
    out.append("static const uint8_t dk_goofy[384] = {" + ",".join(f"0x{b:02X}" for b in pack2(goofy(), 48)) + "};")
    out.append("#endif")
    open(sys.argv[1], "w").write("\n".join(out) + "\n")
    if len(sys.argv) > 2:
        preview(sys.argv[2])


def preview(path):
    from PIL import Image
    codes = sorted(S)
    cols = 12
    im = Image.new("RGB", (cols * 18 * 4, ((len(codes) + cols - 1) // cols) * 18 * 4), (40, 40, 40))
    for i, c in enumerate(codes):
        pal, g = S[c]
        ox, oy = (i % cols) * 18 * 4, (i // cols) * 18 * 4
        for y in range(16):
            for x in range(16):
                v = g[y][x]
                col = (0, 0, 0) if not v else tuple(PAL[pal][v - 1].to_bytes(3, "big"))
                for dy in range(4):
                    for dx in range(4):
                        im.putpixel((ox + x * 4 + dx, oy + y * 4 + dy), col)
    im.save(path)


if __name__ == "__main__":
    main()
