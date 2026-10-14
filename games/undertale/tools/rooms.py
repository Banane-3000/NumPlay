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
]

# rooms one above the other through a dark doorway (tools/pack.py: apply_links)
LINKS = [
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
