"""The game's areas and rooms, cut from the wiki's maps (tools/fetch.py).

An area is one map: every room of the area, one pixel for one, laid out as in the game. A room is a
rectangle of it (rect: x, y, w, h on the map); its rooms touch where their doors are, so walking out of one
walks into the next (world.c). Walkable: the area's floor colours (and the room's floor_colors), inside the
room's floor_rects if it has some (walls of the floor's colour are left out that way), then its fixes
("walk" or "wall" rectangles). Patches change the map first (objects drawn into it, like the rocks, are
taken out: the game draws them).

Rooms are named as the game names them (room_ruins1 is ruins1)."""

# Toriel's home: wood, the rugs, the kitchen's tiles, the hallway's, the bedrooms' floors
HOUSE_FLOOR = [(245, 215, 158), (246, 231, 189), (194, 133, 78), (255, 186, 95), (151, 97, 26), (187, 106, 52),
               (238, 203, 131), (230, 245, 120), (186, 133, 4), (235, 186, 67), (233, 77, 42), (255, 213, 166),
               (166, 189, 255), (74, 151, 201)]
BASEMENT_FLOOR = [(132, 53, 125)]
# the darker floor: cracks, the rocks' tracks
CRACKED = [(132, 53, 125)]
LEAVES = [(128, 17, 17), (232, 35, 19)]
RUINS_PLAIN = [(163, 73, 164), (185, 98, 185)]
RUINS_FLOOR = [
    (163, 73, 164), (185, 98, 185),                      # the floor and its paths
    (128, 17, 17), (232, 35, 19), (233, 77, 42), (123, 24, 19), (121, 18, 0), (64, 0, 0), (157, 32, 0),  # red leaves
    (58, 57, 72), (95, 94, 119), (200, 194, 226), (34, 177, 76),  # the landing: stone, its rings, grass
    (249, 240, 0), (219, 177, 0), (124, 106, 0),  # golden flowers
    (167, 112, 82), (112, 66, 56), (169, 85, 38), (105, 58, 27),  # bridges
]

AREAS = [
    dict(name="ruins", map="Ruins_map.png", floor=RUINS_FLOOR, margin=3, piles=LEAVES, patches=[
        ("mirror", 95, 5988, 27, 60, 178, 5988),     # Toriel tending the flowers (the game's last pages)
        ("fill", 509, 5768, 23, 12, (0, 0, 0)), ("fill", 509, 5780, 23, 11, (34, 177, 76)),   # Flowey
        # what the game draws itself: the switch room's spikes, the Dummy, the pillar Toriel hides behind, the
        # leaf pile's Froggit, the SAVE stars
        ("erase", 1058, 4738, 26, 44, RUINS_PLAIN), ("erase", 1296, 4688, 26, 36, RUINS_PLAIN),
        ("copy", 4521, 4380, 38, 61, 4601, 4380), ("erase", 4598, 4441, 44, 21, RUINS_PLAIN),
        ("erase", 4898, 4441, 28, 23, RUINS_PLAIN),
        ("copy", 531, 5274, 20, 22, 511, 5274), ("copy", 5010, 4480, 22, 20, 5010, 4500),
        ("erase", 7168, 4856, 26, 26, RUINS_PLAIN), ("copy", 8902, 3116, 22, 26, 8942, 3116),
        # the spikes the game raises and lowers (not the blueprint room's: those stay)
    ] + [("erase", x, y, 20, 20, RUINS_PLAIN) for x, y in [
        (5740, 4680), (5740, 4700), (5740, 4720), (5740, 4740), (5740, 4760), (5740, 4780),
        (6880, 4820), (6880, 4840), (6880, 4860), (6900, 4820), (6900, 4840), (6900, 4860),
        (8680, 4620), (8680, 4640), (8760, 4620), (8760, 4640), (8960, 4840), (8980, 4840),
        (8960, 4140), (8980, 4140), (8760, 4340), (8760, 4360), (8660, 4340), (8660, 4360),
        (8440, 4140), (8460, 4140), (8440, 3960), (8460, 3960), (8660, 3740), (8660, 3760)]] + [
        # the rocks, the Froggits, the candy, what lies in the one switch puzzle's rooms, the Toy Knife
        ("erase", 5620, 4735, 20, 20, RUINS_PLAIN), ("erase", 6700, 4795, 20, 20, RUINS_PLAIN),
        ("erase", 6680, 4835, 20, 20, RUINS_PLAIN), ("erase", 6720, 4875, 20, 20, RUINS_PLAIN),
        ("erase", 9400, 3700, 20, 20, RUINS_PLAIN), ("erase", 7660, 4540, 20, 20, RUINS_PLAIN),
        ("erase", 7780, 4540, 20, 20, RUINS_PLAIN), ("erase", 7900, 4540, 20, 20, RUINS_PLAIN),
        ("erase", 4950, 4215, 40, 29, RUINS_PLAIN), ("erase", 8300, 4980, 20, 20, RUINS_PLAIN),
        ("erase", 8660, 5117, 20, 20, RUINS_PLAIN), ("erase", 8260, 5120, 15, 15, RUINS_PLAIN),
        ("erase", 8493, 5115, 35, 19, RUINS_PLAIN), ("erase", 9364, 3566, 15, 15, RUINS_PLAIN),
        # Napstablook on the leaves; Toriel at the door; Flowey in the last room's light
        ("erase", 7524, 4832, 34, 19, LEAVES), ("erase", 10003, 2011, 28, 53, BASEMENT_FLOOR + [(97, 44, 97)]),
        ("erase", 10007, 204, 27, 27, [(0, 0, 0), (200, 194, 226), (95, 94, 119), (58, 57, 72), (34, 177, 76)]),
    ]),
]

SNOW = [(133, 163, 216), (114, 129, 177)]
ICE = [(139, 163, 186), (180, 192, 201), (195, 207, 216)]
WOOD = [(169, 124, 41), (91, 65, 0), (62, 35, 0)]
# the paths' outlines
SNOW_LINES = [(44, 46, 63)]
INN_FLOOR = [(249, 210, 97)]
PS_FLOOR = [(109, 85, 127), (74, 109, 127)]
# the XOXO puzzles' tiles (drawn by the game, X or O), the spikes it raises and lowers
XOXO1 = [(7460, 1240), (7540, 1240)]
XOXO2 = [(8180, 1200), (8240, 1200), (8280, 1200), (8240, 1220), (8280, 1220), (8200, 1240), (8240, 1240),
         (8260, 1240), (8280, 1240), (8180, 1260), (8240, 1260), (8180, 1280), (8200, 1280), (8220, 1280), (8240, 1280)]
XOXO3 = [(9540, 1160), (9700, 1160), (9540, 1180), (9580, 1180), (9660, 1180), (9620, 1220), (9580, 1240),
         (9620, 1240), (9580, 1260), (9660, 1260), (9540, 1300), (9700, 1300)]
SNOW_SPIKES = [(7120, 940), (7120, 960), (7120, 980), (7680, 1220), (7680, 1240), (7680, 1260)] + \
              [(8360, 1160 + 20 * i) for i in range(8)]
BLUE = [(0, 0, 255)]
SPIKE_COLOURS = [(128, 128, 128), (192, 192, 192), (255, 255, 255), (0, 0, 0)]
AREAS.append(dict(name="snowdin", map="Snowdin_map.png", floor=SNOW + ICE + WOOD, margin=2, lines=SNOW_LINES, ice=ICE,
                  patches=[
    # Sans, Papyrus and the monsters who come out after Papyrus' fight or at the very end; the SAVE stars
    ("erase", 2881, 400, 26, 40, "ring"), ("erase", 3236, 374, 28, 34, "ring"), ("erase", 4668, 380, 37, 37, "ring"),
    ("erase", 5440, 290, 42, 56, "ring"), ("erase", 6108, 280, 54, 58, "ring"), ("erase", 6508, 610, 54, 56, "ring"),
    ("erase", 6743, 695, 24, 27, "ring"), ("erase", 7332, 944, 94, 52, "ring"), ("erase", 7858, 1170, 44, 60, "ring"),
    ("erase", 8008, 1160, 44, 49, "ring"), ("erase", 8374, 1186, 32, 42, "ring"), ("erase", 8816, 1254, 34, 46, "ring"),
    ("erase", 9258, 1300, 24, 27, "ring"), ("erase", 9692, 1488, 36, 44, "ring"), ("erase", 10630, 1270, 38, 50, "ring"),
    ("erase", 11772, 1176, 38, 48, "ring"), ("erase", 10182, 2274, 38, 46, "ring"),
    ("erase", 10328, 2255, 69, 58, "ring"), ("erase", 10532, 2274, 38, 46, "ring"),
    ("erase", 7026, 576, 58, 50, "ring"), ("erase", 5948, 258, 54, 52, "ring"),
    # Snowdrake and his parents by Papyrus' sentry station: the trees and the path there from beside (the trees
    # repeat every 40 pixels)
    ("copy", 3620, 316, 80, 72, 3700, 316), ("copy", 3620, 316, 80, 72, 3780, 316), ("copy", 3620, 316, 30, 72, 3860, 316),
]))
# (the puzzles' tiles and spikes first: what's beside them is erased after)
AREAS[-1]["patches"][:0] = [("erase", x, y, 20, 20, "ring", BLUE) for x, y in XOXO1 + XOXO2 + XOXO3] + \
    [("erase", x, y, 20, 20, "ring", SPIKE_COLOURS) for x, y in SNOW_SPIKES]
AREAS.append(dict(name="snowtown", map="Snowdin_Town_map.png", floor=SNOW, margin=2, lines=SNOW_LINES, patches=[
    # the SAVE star; Papyrus and Undyne by the house, the dogs at the library, Papyrus in the fog and at home
    # (they come later)
    ("erase", 518, 1076, 22, 22, "ring"),
    ("erase", 1860, 1008, 34, 82, "ring"), ("erase", 1898, 1010, 56, 78, "ring"), ("erase", 1952, 1004, 44, 86, "ring"),
    ("erase", 1998, 1012, 40, 78, "ring"), ("erase", 2038, 1018, 40, 72, "ring"),
    ("erase", 2132, 1026, 32, 84, "ring"), ("erase", 2163, 1040, 32, 72, "ring"),
    ("erase", 3376, 1036, 30, 66, "ring"), ("erase", 2212, 854, 32, 50, "ring"), ("erase", 2262, 580, 36, 52, "ring"),
]))

# Waterfall: dark blue floors whose colours its walls share too, so a room's floor is its rectangles
WF_FLOOR = [(54, 47, 121), (20, 26, 73)]
WF_WATER = [(27, 33, 90), (27, 21, 72)]
WF_CYAN = [(123, 251, 254), (47, 234, 255)]
AREAS.append(dict(name="waterfall", map="Waterfall_map.png", floor=WF_FLOOR, margin=0, close=True, patches=[
    # the people the game moves about (they're the game's objects here): Monster Kid, the fish by the first
    # echo flower, Undyne (her black ledges kept black), Woshua, the Goner Kid, Onionsan, Shyren, the grey
    # figure by the piano, Aaron
    ("erase", 716, 1498, 28, 36, "ring"), ("erase", 716, 1404, 34, 50, "ring"),
    ("erase", 1800, 1336, 46, 62, "ring", []), ("copy", 3398, 554, 40, 38, 3438, 554),
    ("copy", 4042, 541, 24, 32, 4002, 541), ("erase", 4354, 370, 42, 60, "ring", []),
    ("copy", 9130, 528, 72, 84, 9232, 528), ("copy", 10100, 630, 34, 40, 10134, 630),
    ("copy", 10130, 682, 32, 58, 10168, 682), ("erase", 11897, 614, 28, 36, "ring"),
    ("copy", 14530, 1636, 42, 70, 14476, 1636), ("copy", 15806, 1714, 40, 66, 15846, 1714),
    # the bridge seeds (the game's objects too) and the bridges they make, drawn solved
    *[("erase", x, y, 20, 20, WF_FLOOR) for x, y in [(2300, 1420), (2280, 1440), (2320, 1440), (2300, 1460),
                                                     (2960, 1340), (3000, 1340), (2960, 1380), (3000, 1380)]],
    # the bird (it waits across the gap), the tutu and the artifact (objects, gone once taken)
    ("erase", 8466, 578, 15, 19, [(27, 33, 90), (27, 21, 72), (23, 13, 43)]),
    ("erase", 1240, 1240, 22, 10, [(23, 13, 43), (27, 21, 72), (27, 33, 90)]),
    ("erase", 10156, 150, 12, 7, [(27, 21, 72), (27, 33, 90), (54, 47, 121)]),
    ("fill", 10156, 157, 12, 1, (0, 0, 0)), ("fill", 10156, 158, 12, 3, (99, 102, 135)),
    ("erase", 2380, 1500, 80, 20, "ring"), ("erase", 2850, 1240, 30, 80, "ring"), ("erase", 3040, 1540, 80, 20, "ring"),
]))

ROOMS = [
    # the landing: the bed of golden flowers, a hallway, the Delta Rune arch
    dict(name="area1", area="ruins", rect=(0, 5900, 680, 260), title="Ruins", margin=0,
         fix=[("walk", 600, 6040, 36, 22)],
         doors=[((602, 6040, 32, 8), "area1_2", (510, 5858), "up")]),
    # Flowey's patch of grass in the dark (the floor is black: all of it is walked on), the purple arch north
    dict(name="area1_2", area="ruins", rect=(360, 5560, 320, 340), title="Ruins", margin=0,
         floor_colors=[(0, 0, 0)], floor_rects=[(420, 5640, 200, 260), (500, 5560, 40, 80)],
         fix=[("walk", 500, 5560, 40, 80)],   # under the arch
         doors=[((420, 5894, 200, 6), "area1", (608, 6030), "down")]),
    # the entrance: two staircases, the red leaves, the first SAVE Point
    dict(name="ruins1", area="ruins", rect=(360, 5080, 320, 480), title="Ruins - Entrance", margin=0,
         floor_rects=[(380, 5280, 280, 280), (480, 5240, 80, 40), (400, 5200, 240, 22), (400, 5222, 80, 80),
                      (560, 5222, 80, 80)],
         fix=[("walk", 420, 5276, 40, 28), ("walk", 580, 5276, 40, 28)]),
    # the middle road puzzle: six switches on the floor and a lever
    dict(name="ruins2", area="ruins", rect=(360, 4840, 320, 240), title="Ruins",
         fix=[("walk", 482, 4840, 36, 86)]),
    # the switch room, with Toriel's labels; spikes at its end
    dict(name="ruins3", area="ruins", rect=(360, 4600, 720, 240), title="Ruins", fix=[("walk", 482, 4820, 36, 20)]),
    # the training dummy
    dict(name="ruins4", area="ruins", rect=(1080, 4600, 320, 240), title="Ruins"),
    # the blueprint and the spikes over the water
    dict(name="ruins5", area="ruins", rect=(1100, 4340, 1180, 260), title="Ruins",
         fix=[("wall", 1940, 4460, 280, 80), ("walk", 2216, 4460, 12, 40), ("walk", 1932, 4500, 12, 40)] + [
             ("walk", 1940 + 20 * c, 4460 + 20 * r, 20, 20)
             for r, row in enumerate(["..#####...####", "..#####...####", "####.#######..", "####.#######.."])
             for c, v in enumerate(row) if v == "#"]),
    # the long hallway: the independence test
    dict(name="ruins6", area="ruins", rect=(2280, 4340, 2520, 240), title="Ruins"),
    # the leaf pile, its SAVE Point, the first Froggit to talk
    dict(name="ruins7", area="ruins", rect=(4800, 4360, 280, 440), title="Ruins - Leaf Pile"),
    # the Monster Candy
    dict(name="ruins7a", area="ruins", rect=(4860, 4100, 200, 260), title="Ruins"),
    # the cracked floor and the room under it
    dict(name="ruins8", area="ruins", floor_colors=CRACKED, rect=(5080, 4600, 410, 220), title="Ruins",
         fix=[("walk", 5244, 4704, 72, 56)]),
    dict(name="ruins8b", area="ruins", rect=(5140, 5020, 280, 200), title="Ruins"),
    # the first rock puzzle
    dict(name="ruins9", area="ruins", floor_colors=CRACKED, rect=(5490, 4600, 420, 220), title="Ruins",
         fix=[("walk", 5676, 4736, 28, 28)]),
    # the leaf puzzle: cracked floor above, the way through in leaves below
    dict(name="ruins10", area="ruins", floor_colors=CRACKED, rect=(5910, 4600, 580, 320), title="Ruins",
         fix=[("walk", 6152, 4736, 128, 164), ("walk", 6280, 4778, 188, 122), ("wall", 6280, 4700, 190, 78),
              ("wall", 5910, 4600, 380, 98)]),
    dict(name="ruins10b", area="ruins", rect=(5940, 5020, 540, 300), title="Ruins"),
    # the second rock puzzle: the rock that won't move
    dict(name="ruins11", area="ruins", floor_colors=CRACKED, rect=(6490, 4680, 560, 260), title="Ruins",
         fix=[("walk", 6776, y, 28, 28) for y in (4796, 4836, 4876)]),
    # the mouse hole, its SAVE Point, the cheese
    dict(name="ruins12", area="ruins", rect=(7050, 4720, 380, 200), title="Ruins - Mouse Hole"),
    # Napstablook, pretending to sleep
    dict(name="ruins13", area="ruins", rect=(7430, 4700, 260, 220), title="Ruins"),
    # the Spider Bake Sale
    dict(name="ruins14", area="ruins", rect=(7690, 4700, 340, 220), title="Ruins"),
    # the Froggits' hall
    dict(name="ruins15", area="ruins", rect=(7520, 4400, 540, 300), title="Ruins"),
    # the one switch puzzle, and its six rooms under it
    dict(name="ruins16", area="ruins", floor_colors=CRACKED, rect=(8060, 4480, 640, 280), title="Ruins",
         fix=[("walk", x, y, 60, 60) for x in (8320, 8440, 8560) for y in (4560, 4680)]),
    dict(name="ruins16a", area="ruins", rect=(8230, 4915, 160, 125), title="Ruins"),
    dict(name="ruins16b", area="ruins", rect=(8410, 4915, 140, 125), title="Ruins"),
    dict(name="ruins16c", area="ruins", rect=(8550, 4915, 140, 125), title="Ruins"),
    dict(name="ruins16d", area="ruins", rect=(8230, 5040, 160, 120), title="Ruins"),
    dict(name="ruins16e", area="ruins", rect=(8410, 5040, 140, 120), title="Ruins"),
    dict(name="ruins16f", area="ruins", rect=(8550, 5040, 140, 120), title="Ruins"),
    # the rotating rooms: the same room, turned; and the one a wrong switch drops Frisk in
    dict(name="ruins17a", area="ruins", rect=(8700, 4500, 360, 400), title="Ruins",
         fix=[("walk", 8752, 4620, 36, 40), ("walk", 8960, 4832, 40, 36)],
         doors=[((8960, 4894, 40, 6), "ruins17b", (8970, 4124), "down")]),
    dict(name="ruins17b", area="ruins", rect=(8720, 4060, 340, 380), title="Ruins",
         fix=[("walk", 8962, 4126, 36, 14), ("walk", 8960, 4132, 40, 36)],
         doors=[((8960, 4126, 40, 6), "ruins17a", (8970, 4858), "up")]),
    dict(name="ruins17c", area="ruins", rect=(8380, 4000, 340, 440), title="Ruins",
         fix=[("walk", 8652, 4340, 36, 40)]),
    dict(name="ruins17d", area="ruins", rect=(8380, 3610, 320, 390), title="Ruins",
         fix=[("walk", 8440, 3952, 40, 36)]),
    dict(name="ruins17x", area="ruins", rect=(9140, 4480, 280, 220), title="Ruins"),
    # the crossroads, the Froggit who saw Toriel, the balcony over Home
    dict(name="ruins18", area="ruins", rect=(8700, 3460, 590, 370), title="Ruins",
         fix=[("walk", 8846, 3720, 48, 80), ("walk", 9166, 3720, 48, 80)]),   # vines on the floor
    dict(name="ruins19", area="ruins", rect=(9290, 3620, 250, 200), title="Ruins"),
    dict(name="ruins20", area="ruins", rect=(9300, 3360, 360, 260), title="Ruins"),
    # Toriel's tree and her door
    dict(name="ruins21", area="ruins", rect=(8860, 2920, 320, 540), title="Ruins - Home"),
    # Toriel's home
    dict(name="torhouse1", area="ruins", floor_colors=HOUSE_FLOOR, margin=0, rect=(8860, 2660, 320, 260), title="Home",
         floor_rects=[(8860, 2815, 320, 75), (8900, 2760, 25, 60), (9085, 2740, 80, 80), (8995, 2890, 45, 30)],
         fix=[("walk", 9062, 2742, 24, 16), ("wall", 9100, 2736, 46, 18)], doors=[((9062, 2742, 14, 16), "basement1", (8952, 2316), "right")]),
    dict(name="torhouse2", area="ruins", floor_colors=HOUSE_FLOOR, margin=0, rect=(8540, 2670, 320, 250), title="Home",
         floor_rects=[(8560, 2735, 280, 150), (8585, 2670, 42, 65), (8840, 2800, 20, 85)],
         fix=[("wall", 8655, 2680, 90, 82), ("wall", 8753, 2690, 60, 62), ("wall", 8815, 2705, 22, 47),
              ("wall", 8592, 2822, 108, 50), ("wall", 8578, 2830, 16, 24), ("wall", 8700, 2830, 16, 24)]),
    dict(name="torkitchen", area="ruins", floor_colors=HOUSE_FLOOR, margin=0, rect=(8540, 2500, 200, 170), title="Home",
         floor_rects=[(8562, 2600, 156, 62), (8587, 2660, 40, 12)]),
    dict(name="torhouse3", area="ruins", floor_colors=HOUSE_FLOOR, margin=0, rect=(9180, 2700, 760, 220), title="Home",
         floor_rects=[(9180, 2820, 720, 60)],
         doors=[((9326, 2814, 28, 8), "asrielroom", (9377, 2630), "up"),
                ((9536, 2814, 28, 8), "torielroom", (9607, 2630), "up")]),
    dict(name="asrielroom", area="ruins", floor_colors=HOUSE_FLOOR, margin=0, rect=(9200, 2460, 260, 230), title="Home",
         floor_rects=[(9240, 2540, 180, 120), (9366, 2655, 42, 30)],
         fix=[("wall", 9368, 2536, 40, 46), ("wall", 9405, 2540, 16, 40), ("wall", 9394, 2578, 24, 14)],
         doors=[((9366, 2676, 42, 8), "torhouse3", (9330, 2826), "down")]),
    dict(name="torielroom", area="ruins", floor_colors=HOUSE_FLOOR, margin=0, rect=(9510, 2430, 280, 260), title="Home",
         floor_rects=[(9530, 2540, 240, 120), (9597, 2655, 42, 35)],
         fix=[("wall", 9530, 2536, 66, 56), ("wall", 9540, 2592, 48, 44), ("wall", 9575, 2580, 26, 30),
              ("wall", 9695, 2605, 28, 36)],
         doors=[((9597, 2676, 41, 8), "torhouse3", (9540, 2826), "down")]),
    # the basement, to the end of the Ruins
    dict(name="basement1", area="ruins", floor_colors=BASEMENT_FLOOR, margin=0, rect=(8860, 2220, 1260, 180), title="Ruins",
         fix=[("walk", 8930, 2320, 20, 40)], doors=[((8930, 2320, 8, 40), "torhouse1", (9084, 2764), "down")]),
    dict(name="basement2", area="ruins", floor_colors=BASEMENT_FLOOR, margin=0, rect=(9930, 1960, 220, 260), title="Ruins",
         fix=[("walk", 10000, 2024, 40, 10)], doors=[((10000, 2026, 40, 6), "basement3", (10010, 1914), "up")]),
    dict(name="basement3", area="ruins", floor_colors=[(97, 44, 97), (132, 53, 125), (0, 0, 0)], margin=0,
         rect=(9940, 300, 180, 1660), floor_rects=[(9980, 300, 80, 1660)], title="Ruins",
         fix=[("walk", 10000, 1950, 40, 10)], doors=[((10000, 1956, 40, 4), "basement2", (10010, 2034), "down")]),
    dict(name="ruinsexit", area="ruins", floor_colors=[(0, 0, 0), (97, 44, 97)], margin=0, rect=(9940, 0, 200, 300),
         floor_rects=[(9980, 70, 80, 230)], title="Ruins - Exit"),

    # ---- Snowdin: the forest, from the Ruins' door to the town
    # the forest's walkway: the path is walked all along (trunks and a branch drawn over it are in front)
    dict(name="snowdin1", area="snowdin", rect=(0, 240, 1070, 240), title="Snowdin Forest",
         fix=[("walk", 40, 404, 1030, 52)]),
    dict(name="snowdin2", area="snowdin", rect=(1100, 240, 670, 240), title="Snowdin Forest", fix=[("walk", 1100, 404, 670, 52)]),
    dict(name="snowdin3", area="snowdin", rect=(1800, 240, 410, 240), title="Snowdin Forest", fix=[("walk", 1800, 404, 410, 52)]),
    dict(name="snowdin4", area="snowdin", rect=(2240, 240, 300, 240), title="Snowdin Forest", fix=[("walk", 2240, 404, 300, 52)]),
    # Papyrus' gate on the bridge: its bars are too wide to stop anyone
    dict(name="snowdin5", area="snowdin", rect=(2570, 240, 590, 240), title="Snowdin Forest",
         fix=[("walk", 2570, 412, 100, 44)]),
    dict(name="boxroad", area="snowdin", rect=(3160, 240, 380, 240), title="Snowdin - Box Road"),
    dict(name="river", area="snowdin", rect=(3220, 0, 320, 240), title="Snowdin Forest"),
    dict(name="papsentry", area="snowdin", rect=(3540, 240, 580, 240), title="Snowdin Forest"),
    dict(name="doggo", area="snowdin", rect=(4120, 240, 460, 240), title="Snowdin Forest"),
    dict(name="icefield", area="snowdin", rect=(4580, 240, 620, 240), ice=True, title="Snowdin Forest"),
    dict(name="snowman", area="snowdin", rect=(4720, 0, 380, 240), title="Snowdin Forest"),
    dict(name="elecmaze", area="snowdin", rect=(5200, 240, 300, 240), title="Snowdin Forest"),
    dict(name="ballgame", area="snowdin", rect=(5500, 240, 800, 580), ice=True, title="Snowdin Forest",
         fix=[("walk", 5716, 318, 68, 22)]),   # the plank bridge over the chasm
    dict(name="dogi", area="snowdin", rect=(6300, 240, 300, 220), title="Snowdin Forest"),
    dict(name="wordsearch", area="snowdin", rect=(6300, 520, 320, 300), title="Snowdin Forest"),
    dict(name="spaghetti", area="snowdin", rect=(6620, 520, 300, 300), title="Snowdin - Spaghetti"),
    dict(name="snowpuzzle", area="snowdin", rect=(6920, 540, 620, 580), title="Snowdin Forest",
         fix=[("walk", 7254, 958, 50, 22)]),   # the plank bridge
    dict(name="xoxo1", area="snowdin", rect=(7260, 1120, 440, 240), title="Snowdin Forest"),
    dict(name="xoxo2", area="snowdin", rect=(7700, 1120, 800, 240), title="Snowdin Forest"),
    dict(name="tilepuzzle", area="snowdin", rect=(8500, 1120, 400, 240), title="Snowdin Forest",
         floor_colors=[(255, 192, 192)], fix=[("walk", 8614, 1238, 52, 22)]),   # the plank bridge
    dict(name="doghouse", area="snowdin", rect=(8900, 1120, 540, 240), title="Snowdin - Dog House"),
    # the ice puzzle: once solved, a bridge to the trees Frisk slides through (an object blocks it until then)
    dict(name="xoxo3", area="snowdin", rect=(9440, 1120, 400, 220), ice=True, title="Snowdin Forest",
         fix=[("walk", 9780, 1204, 60, 36), ("walk", 9520, 1320, 264, 20)], doors=[((9836, 1205, 4, 40), "poffzone", (10196, 1200), "right")]),
    dict(name="xoxofail", area="snowdin", rect=(9440, 1340, 340, 260), title="Snowdin Forest"),
    dict(name="poffzone", area="snowdin", rect=(10140, 1120, 820, 240), title="Snowdin Forest",
         fix=[("walk", 10190, 1212, 40, 24), ("walk", 10840, 1260, 120, 14)],
         doors=[((10190, 1195, 4, 30), "xoxo3", (9796, 1200), "left"),
                ((10302, 1350, 36, 8), "southcliff", (10590, 2190), "down")]),
    dict(name="dangerbridge", area="snowdin", rect=(10960, 1120, 1040, 240), title="Snowdin Forest",
         fix=[("walk", 11110, 1226, 690, 26)], doors=[((11990, 1236, 10, 50), "snowtown", (4, 1088), "right")]),
    # the mysterious door, up a long corridor from the cliff under the ice bridge
    dict(name="doorroom", area="snowdin", rect=(9780, 1590, 320, 590), title="Snowdin Forest",
         floor_colors=[(22, 67, 88), (27, 89, 109), (30, 106, 122), (6, 14, 22), (13, 37, 51), (19, 56, 75)]),
    dict(name="southcliff", area="snowdin", rect=(9480, 2180, 1200, 240), title="Snowdin Forest",
         fix=[("walk", 9922, 2180, 40, 60)], doors=[((10585, 2180, 30, 6), "poffzone", (10310, 1320), "up")]),

    # ---- Snowdin Town
    dict(name="snowtown", area="snowtown", rect=(0, 960, 3260, 240), title="Snowdin Town",
         fix=[("walk", 0, 1080, 8, 50, "floor")],
         doors=[((0, 1080, 4, 50), "dangerbridge", (11968, 1232), "left"),
                ((624, 1066, 28, 24), "inn1", (628, 918), "up"), ((1312, 1070, 26, 20), "grillbys", (1236, 914), "up"),
                ((1840, 1066, 26, 24), "library", (2026, 664), "up"),
                # the undersnow tunnels: an igloo each end of the town
                ((726, 1066, 24, 24), "snowtown", (2336, 1094), "down"), ((2336, 1084, 26, 16), "snowtown", (728, 1078), "down")]),
    dict(name="townnorth", area="snowtown", rect=(1400, 700, 640, 260), title="Snowdin Town"),
    dict(name="towndock", area="snowtown", rect=(1400, 480, 410, 220), title="Snowdin Town"),
    dict(name="fogroom", area="snowtown", rect=(3260, 960, 360, 240), title="Snowdin Town",
         fix=[("walk", 3440, 1084, 180, 48)],   # the way on: Waterfall's blue ground
         doors=[((3612, 1080, 8, 56), "wf1", (4, 1480), "right")]),
    # the interiors: their floors are many colours, so their floor and furniture are rectangles
    dict(name="inn1", area="snowtown", rect=(500, 740, 260, 220), title="Snowed Inn", open=True, margin=0,
         floor_rects=[(520, 800, 222, 100), (600, 900, 80, 60), (536, 756, 44, 48)],
         fix=[("wall", 705, 850, 37, 50)],
         doors=[((604, 952, 72, 8), "snowtown", (626, 1078), "down"), ((536, 756, 44, 6), "inn2", (512, 676), "right")]),
    dict(name="inn2", area="snowtown", rect=(500, 480, 320, 230), title="Snowed Inn", open=True, margin=0,
         floor_rects=[(522, 600, 116, 76), (662, 600, 136, 76), (504, 684, 312, 22), (566, 670, 32, 16), (706, 670, 32, 16)],
         doors=[((504, 684, 4, 22), "inn1", (548, 790), "down")]),
    dict(name="grillbys", area="snowtown", rect=(1080, 710, 320, 250), title="Grillby's", open=True, margin=0,
         floor_rects=[(1090, 800, 300, 150)],
         fix=[("wall", 1092, 826, 64, 44), ("wall", 1140, 884, 104, 48), ("wall", 1300, 820, 90, 130),
              ("wall", 1098, 924, 44, 26)],
         doors=[((1220, 944, 50, 6), "snowtown", (1314, 1080), "down")]),
    dict(name="library", area="snowtown", rect=(1810, 480, 300, 225), title="Librarby", open=True, margin=0,
         floor_rects=[(1822, 566, 276, 132)], fix=[("wall", 1822, 620, 88, 80), ("wall", 1976, 616, 88, 66)],
         doors=[((2000, 694, 70, 6), "snowtown", (1842, 1078), "down")]),
    dict(name="pshouse", area="snowtown", rect=(2210, 710, 300, 245), title="Snowdin Town", open=True, margin=0,
         floor_rects=[(2220, 848, 280, 100), (2220, 768, 280, 34), (2222, 800, 56, 52)],
         fix=[("wall", 2300, 892, 82, 40), ("wall", 2318, 836, 52, 30), ("wall", 2462, 868, 40, 64)],
         doors=[((2420, 944, 50, 8), "snowtown", (2240, 1096), "down"), ((2228, 766, 34, 4), "paproom", (2296, 660), "up"), ((2440, 766, 30, 4), "sansroom", (2560, 660), "up")]),
    dict(name="paproom", area="snowtown", rect=(2150, 500, 260, 200), title="Snowdin Town", open=True, margin=0,
         floor_rects=[(2160, 574, 240, 120)],
         fix=[("wall", 2160, 636, 92, 56), ("wall", 2338, 556, 64, 44), ("wall", 2160, 556, 40, 64),
              ("wall", 2358, 648, 44, 44)],
         doors=[((2286, 690, 40, 8), "pshouse", (2234, 774), "down")]),
    dict(name="sansroom", area="snowtown", rect=(2470, 510, 180, 190), title="Snowdin Town", open=True, margin=0,
         floor_rects=[(2480, 566, 160, 126)], fix=[("wall", 2480, 560, 52, 32), ("wall", 2598, 556, 42, 56)],
         doors=[((2550, 690, 40, 8), "pshouse", (2444, 774), "down")]),
    dict(name="shed", area="snowtown", rect=(2550, 710, 290, 245), title="Snowdin Town", open=True, margin=0,
         floor_rects=[(2580, 770, 180, 120), (2560, 886, 80, 64)], fix=[("wall", 2580, 730, 56, 100)],
         doors=[((2560, 945, 80, 8), "snowtown", (2400, 1096), "down")]),
    # Waterfall, the west: from Snowdin to the bridge Undyne cuts
    dict(name="wf1", area="waterfall", rect=(0, 1330, 1170, 310), title="Waterfall",
         fix=[("walk", 1040, 1505, 20, 135), ("walk", 1150, 1452, 20, 56)],
         doors=[((0, 1450, 4, 96), "fogroom", (3580, 1088), "left")]),
    dict(name="wf_falls", area="waterfall", rect=(1170, 1140, 180, 500), title="Waterfall", floor_colors=WF_WATER,
         floor_rects=[(1170, 1446, 180, 194), (1236, 1326, 48, 124), (1180, 1236, 160, 96)],
         fix=[("walk", 1170, 1452, 14, 56), ("walk", 1336, 1500, 14, 36), ("walk", 1236, 1396, 48, 56)]),
    dict(name="wf_falls_low", area="waterfall", rect=(1030, 1640, 450, 180), title="Waterfall", floor_colors=WF_WATER,
         fix=[("walk", 1040, 1640, 20, 14), ("walk", 1160, 1676, 20, 80), ("walk", 1360, 1676, 20, 80),
              ("wall", 1180, 1640, 160, 14)]),
    dict(name="wf2", area="waterfall", rect=(1350, 1140, 750, 500), title="Waterfall",
         fix=[("walk", 1350, 1500, 16, 36), ("walk", 1756, 1500, 172, 36)]),   # (the tall grass)
    dict(name="wf_seeds1", area="waterfall", rect=(2100, 1300, 520, 340), title="Waterfall"),
    dict(name="wf_seeds2", area="waterfall", rect=(2620, 1120, 500, 520), title="Waterfall",
         fix=[("walk", 2716, 1228, 46, 34), ("walk", 2810, 1494, 100, 28)],   # (the doorway, the planks)
         doors=[((2716, 1232, 46, 24), "wf_wish", (2727, 1080), "up")]),
    # (reached by the secret bridge of seeds, then the planks)
    dict(name="wf_quiche", area="waterfall", rect=(3120, 1340, 260, 300), title="Waterfall", floor_colors=WF_WATER,
         floor_rects=[(3180, 1440, 180, 100)], fix=[("walk", 3120, 1540, 100, 20)]),
    dict(name="wf_wish", area="waterfall", rect=(2640, 780, 740, 340), title="Waterfall", floor_colors=WF_WATER,
         floor_rects=[(2660, 1040, 700, 62), (3098, 816, 64, 226), (2712, 1100, 50, 22)],
         fix=[("walk", 3112, 812, 36, 50)], doors=[((2712, 1118, 50, 4), "wf_seeds2", (2729, 1250), "down")]),
    dict(name="wf_hist", area="waterfall", rect=(2980, 400, 530, 380), title="Waterfall"),
    dict(name="wf_chase1", area="waterfall", rect=(4000, 300, 2520, 400), title="Waterfall"),
    dict(name="wf_grass1", area="waterfall", rect=(6520, 400, 1320, 300), title="Waterfall", floor_colors=WF_WATER,
         floor_rects=[(6520, 540, 360, 72), (6880, 560, 960, 44)], fix=[("walk", 6520, 544, 360, 64)]),
    dict(name="wf_sans2", area="waterfall", rect=(7840, 0, 360, 600), title="Waterfall", floor_colors=WF_WATER,
         floor_rects=[(7860, 92, 300, 118), (7956, 206, 38, 360), (7840, 556, 360, 50)],
         fix=[("walk", 7956, 420, 38, 140)]),
    # the marsh, where the colours turn: its land is black, its water bright blue (the planks cross it), and around
    # it plain black is nothing (its rectangles); the boardwalk ends at a gap the bird carries Frisk over, a ladder
    # goes down into the marsh
    dict(name="wf_marsh_top", area="waterfall", rect=(8200, 400, 450, 200), title="Waterfall",
         floor_colors=WF_WATER + [(23, 13, 43), (0, 0, 0)], floor_rects=[(8200, 556, 284, 64), (8536, 556, 114, 64)]),
    dict(name="wf_marsh_mid", area="waterfall", rect=(7400, 600, 1250, 200), title="Waterfall",
         floor_colors=[(0, 0, 0)], floor_rects=[(8060, 600, 590, 100), (8060, 700, 110, 100), (7760, 780, 300, 20)],
         fix=[("walk", 8140, 600, 20, 84)]),
    dict(name="wf_marsh", area="waterfall", rect=(7400, 800, 1650, 660), title="Waterfall",
         floor_colors=[(0, 0, 0)], floor_rects=[(7660, 800, 510, 660), (8170, 940, 190, 520), (8360, 1000, 380, 460),
                                               (8740, 800, 55, 660), (8795, 800, 85, 120), (8795, 1000, 265, 460),
                                               (7460, 1150, 200, 190)],
         fix=[("walk", x, y - 2, w, h + 4) for x, y, w, h in [(7821, 825, 58, 55), (7921, 945, 58, 55), (8301, 1005, 58, 55),
              (8741, 1005, 58, 55), (8561, 1145, 58, 55), (7941, 1220, 59, 60), (8501, 1360, 59, 57), (8741, 1440, 56, 17)]]),
    # (up from the marsh, a narrow black path by Onionsan's water)
    dict(name="wf_onion", area="waterfall", rect=(8650, 500, 1250, 300), title="Waterfall", floor_colors=[(0, 0, 0)],
         floor_rects=[(8740, 656, 1160, 48), (8740, 656, 60, 144)], fix=[("walk", 9860, 660, 40, 40)]),
    dict(name="wf_piano", area="waterfall", rect=(9900, 20, 600, 780), title="Waterfall", floor_colors=WF_WATER,
         floor_rects=[(10060, 140, 200, 64), (10136, 200, 48, 104), (10118, 300, 64, 100), (10040, 398, 220, 64),
                      (10118, 460, 64, 140), (10040, 598, 220, 124), (9900, 638, 142, 46), (10258, 638, 242, 46)]),
    dict(name="wf_statue", area="waterfall", rect=(10500, 520, 1000, 260), title="Waterfall", floor_colors=WF_WATER,
         fix=[("walk", 10500, 640, 1000, 60)]),
    # (the rain's puddles are black)
    dict(name="wf_rain1", area="waterfall", rect=(11500, 520, 1360, 300), title="Waterfall",
         floor_colors=WF_WATER + [(0, 0, 0)], floor_rects=[(11500, 636, 1360, 68), (12740, 700, 80, 120)]),
    dict(name="wf_rain2", area="waterfall", rect=(12700, 820, 220, 980), title="Waterfall", floor_colors=WF_WATER),
    dict(name="wf_castle", area="waterfall", rect=(12920, 1560, 1480, 260), title="Waterfall", floor_colors=WF_WATER,
         fix=[("walk", 12920, 1676, 1480, 48)]),
    dict(name="wf_aaron", area="waterfall", rect=(14400, 1560, 960, 260), title="Waterfall", floor_colors=WF_WATER),
    dict(name="wf_bridge", area="waterfall", rect=(15360, 1540, 2300, 460), title="Waterfall"),
]

# rooms one above the other through a dark doorway (tools/pack.py: apply_links); ("h", left, right, y, h): side
# by side with a gap, a door each way along the rows y .. y + h
LINKS = [
    ("h", "wf_hist", "wf_chase1", 540, 60),
    ("v", "wf_wish", "wf_hist", 3112, 36, 818),
    ("h", "snowdin1", "snowdin2", 404, 52),
    ("h", "snowdin2", "snowdin3", 404, 52),
    ("h", "snowdin3", "snowdin4", 404, 52),
    ("h", "snowdin4", "snowdin5", 404, 52),
    ("v", "ruins1", "ruins2", 506, 28, 5175),
    ("v", "ruins4", "ruins5", 1244, 32, 4640),
    ("v", "ruins7", "ruins7a", 4946, 28, 4400),
    ("v", "ruins13", "ruins15", 7580, 34, 4740),
    ("v", "ruins18", "ruins21", 9006, 30, 3500),
    ("v", "ruins19", "ruins20", 9468, 24, 3670),
    ("v", "ruins21", "torhouse1", 9006, 26, 3070),
    ("v", "ruins17c", "ruins17d", 8444, 32, 4080),
    # the chutes up from the rooms under the cracked floors
    ("up", "ruins8b", 5190, 30, 5070, "ruins8", (5167, 4684)),
    ("up", "ruins8b", 5350, 30, 5070, "ruins8", (5407, 4684)),
    ("up", "ruins10b", 6066, 30, 5070, "ruins10", (6066, 4690)),
    ("up", "ruins16a", 8263, 12, 4945, "ruins16", (8200, 4546)),
    ("up", "ruins16b", 8503, 12, 4945, "ruins16", (8440, 4546)),
    ("up", "ruins16c", 8643, 12, 4945, "ruins16", (8580, 4546)),
    ("up", "ruins16d", 8263, 12, 5065, "ruins16", (8200, 4546)),
    ("up", "ruins16e", 8503, 12, 5065, "ruins16", (8440, 4546)),
    ("up", "ruins16f", 8643, 12, 5065, "ruins16", (8580, 4546)),
    ("up", "ruins17x", 9268, 26, 4510, "ruins17a", (8790, 4606)),
]
