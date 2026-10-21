"""The other characters of Rebirth: Isaac's frames (tools/art.py) with their hair, hats and colours drawn over the top
of the head (the face below it is his), and their bodies in their own colours."""
from PIL import Image, ImageDraw

import art as A

COLOURS = {
    "h": (214, 176, 84), "H": (156, 116, 46),       # Magdalene's blond hair
    "c": (112, 72, 42), "C": (72, 42, 22),          # Cain's brown hair
    "f": (204, 34, 34), "F": (132, 12, 12), "t": (20, 20, 20),   # Judas's fez and its tassel
    "e": (44, 38, 44), "E": (14, 10, 14),           # Eve's black hair
    "s": (122, 82, 52), "S": (82, 52, 32),          # Samson's long hair
    "a": (70, 24, 24), "A": (204, 186, 166),        # Azazel's horns
}
HAIR = {
    "magdalene": [
        "..........########..........",
        "........##hhhhhhhh##........",
        "......##hhhhhhhhhhhh##......",
        ".....#hhhhhhhHHhhhhhhh#.....",
        "....#hhhhhhhHHHHhhhhhhh#....",
        "...#hhhhhhhH....Hhhhhhhh#...",
        "..#hhhhhhH........Hhhhhhh#..",
        "..#hhhhH............Hhhhh#..",
        ".#hhhH................Hhhh#.",
        ".#hhH..................Hhh#.",
        "#hhH....................Hhh#",
        "#hH......................Hh#",
    ],
    "cain": [
        "........###.##.###..........",
        ".......#ccc#cc#ccc##........",
        "......#cccccccccccccc#......",
        ".....#ccCccccCcccccCcc#.....",
        "....#cccCCcccCCcccCCccc#....",
        "...#ccC..CccC..CccC..Ccc#...",
        "..#cC.....CC....CC....Ccc#..",
        "..#C...................Cc#..",
        ".#C.....................C#..",
    ],
    "judas": [
        ".........######.............",
        "........#ffffff#............",
        ".......#ffffffff#...........",
        ".......#fFffffff#tt.........",
        ".......#fFffffff#.tt........",
        "......##FFFFFFFF##.t........",
        "......#fffffffffff#.........",
    ],
    "eve": [
        "..........########..........",
        "........##eeeeeeee##........",
        "......##eeeeeeeeeeee##......",
        ".....#eeeeeeeeeeeeeeee#.....",
        "....#eeeeeeeeeeeeeeeeee#....",
        "...#eeeeeeeeeeeeeeeeeeee#...",
        "..#eeeeeeeeeeeeeeeeeeeeee#..",
        "..#eeeEEe.eeEe..eEee.eEEe#..",
        ".#eeeE....E........E...Eee#.",
        ".#eeE...................Ee#.",
        "#eeE.....................Ee#",
        "#eE.......................e#",
    ],
    "samson": [
        "..........##.##.##..........",
        "........##ss#ss#ss##........",
        "......##ssssssssssss##......",
        ".....#ssssssssssssssss#.....",
        "....#ssssSsssssssSsssss#....",
        "...#sssSS.sSssssS.SSssss#...",
        "..#ssSS.....SS.S......Sss#..",
        "..#sS...................Ss#.",
        ".#sS.....................Ss#",
        ".#S.......................S#",
        "#sS.......................S#",
        "#S........................s#",
    ],
    "azazel": [
        "AA......................AA..",
        "aAA....................AAa..",
        ".aAA..................AAa...",
        "..aAA................AAa....",
        "...aa................aa.....",
    ],
}
# the colours of a body: ??? is blue, Azazel black with red eyes' tears
SKINS = {
    "bluebaby": {"+": (140, 168, 220), "o": (110, 136, 196), "r": (80, 100, 170), "#": (10, 10, 40)},
    "azazel": {"+": (56, 48, 54), "o": (42, 36, 40), "r": (28, 24, 28), "#": (0, 0, 0), "b": (180, 30, 30),
               "B": (120, 10, 10)},
}
CHARACTERS = ["isaac", "magdalene", "cain", "judas", "bluebaby", "eve", "samson", "azazel"]


def frames(name):
    """a character's head (6 frames) and body (9 frames), as art.isaac_frames() gives Isaac's"""
    pal = dict(A.ISAAC)
    pal.update(SKINS.get(name, {}))
    pal.update(COLOURS)
    heads = []
    for k, rows in enumerate([A.HEAD_FRONT, A.HEAD_FRONT_CRY, A.HEAD_SIDE, A.HEAD_SIDE_CRY, A.HEAD_BACK, A.HEAD_BACK]):
        rows = [list(r) for r in rows]
        for y, line in enumerate(HAIR.get(name, [])):
            for x, ch in enumerate(line):
                if ch != "." and y < len(rows) and x < len(rows[y]):
                    rows[y][x] = ch
        im = A.art(["".join(r) for r in rows], pal)
        if name == "cain" and k < 2:   # his eye patch over his right eye, and its strap
            d = ImageDraw.Draw(im)
            d.rectangle([4, 11, 9, 15], fill=(20, 20, 20, 255))
            d.line([(3, 9), (24, 12)], fill=(20, 20, 20, 255))
        heads.append(im)
    bodies = []
    for rows in [A.BODY_FRONT, A.BODY_FRONT_STEP, A.BODY_FRONT, None, A.BODY_SIDE, A.BODY_SIDE_STEP1, A.BODY_SIDE,
                 A.BODY_SIDE_STEP2, A.BODY_HOLD]:
        bodies.append(A.art(A.BODY_FRONT_STEP, pal).transpose(Image.FLIP_LEFT_RIGHT) if rows is None else A.art(rows, pal))
    return heads, bodies
