"""The game's areas and rooms, cut from the wiki's maps (tools/fetch.py).

An area is one map: every room of the area, one pixel for one, laid out as in the game. A room is a
rectangle of it (rect: x, y, w, h on the map); its rooms touch where their doors are, so walking out of one
walks into the next (world.c). Walkable: the area's floor colours (and the room's floor_colors), inside the
room's floor_rects if it has some (walls of the floor's colour are left out that way), then its fixes
("walk" or "wall" rectangles). Patches change the map first (objects drawn into it, like the rocks, are
taken out: the game draws them).

Rooms are named as the game names them (room_ruins1 is ruins1)."""

RUINS_FLOOR = [
    (163, 73, 164), (185, 98, 185),                      # the floor and its paths
    (128, 17, 17), (232, 35, 19), (233, 77, 42), (123, 24, 19), (121, 18, 0), (64, 0, 0), (157, 32, 0),  # red leaves
    (58, 57, 72), (95, 94, 119), (200, 194, 226), (34, 177, 76),  # the landing: stone, its rings, grass
    (249, 240, 0), (219, 177, 0), (124, 106, 0),  # golden flowers
    (167, 112, 82), (112, 66, 56), (169, 85, 38), (105, 58, 27),  # bridges
]

AREAS = [
    dict(name="ruins", map="Ruins_map.png", floor=RUINS_FLOOR, margin=3, patches=[
        ("mirror", 95, 5988, 27, 60, 178, 5988),     # Toriel tending the flowers (the game's last pages)
        ("fill", 509, 5768, 23, 12, (0, 0, 0)), ("fill", 509, 5780, 23, 11, (34, 177, 76)),   # Flowey
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
         doors=[((420, 5894, 200, 6), "area1", (608, 6030), "down")]),
    # the entrance: two staircases, the red leaves, the first SAVE Point
    dict(name="ruins1", area="ruins", rect=(360, 5080, 320, 480), title="Ruins - Entrance"),
    # the middle road puzzle: six switches on the floor and a lever
    dict(name="ruins2", area="ruins", rect=(360, 4840, 320, 240), title="Ruins"),
    # the switch room, with Toriel's labels; spikes at its end
    dict(name="ruins3", area="ruins", rect=(360, 4600, 720, 240), title="Ruins"),
    # the training dummy
    dict(name="ruins4", area="ruins", rect=(1080, 4600, 320, 240), title="Ruins"),
    # the blueprint and the spikes over the water
    dict(name="ruins5", area="ruins", rect=(1100, 4340, 1180, 260), title="Ruins"),
    # the long hallway: the independence test
    dict(name="ruins6", area="ruins", rect=(2280, 4340, 2560, 240), title="Ruins"),
    # the leaf pile, its SAVE Point, the first Froggit to talk
    dict(name="ruins7", area="ruins", rect=(4840, 4360, 240, 440), title="Ruins - Leaf Pile"),
    # the Monster Candy
    dict(name="ruins7a", area="ruins", rect=(4860, 4100, 200, 260), title="Ruins"),
    # the cracked floor and the room under it
    dict(name="ruins8", area="ruins", rect=(5080, 4600, 410, 220), title="Ruins"),
    dict(name="ruins8b", area="ruins", rect=(5140, 5020, 280, 200), title="Ruins"),
    # the first rock puzzle
    dict(name="ruins9", area="ruins", rect=(5490, 4600, 420, 220), title="Ruins"),
    # the leaf puzzle: cracked floor above, the way through in leaves below
    dict(name="ruins10", area="ruins", rect=(5910, 4600, 580, 320), title="Ruins"),
    dict(name="ruins10b", area="ruins", rect=(5940, 5020, 540, 300), title="Ruins"),
    # the second rock puzzle: the rock that won't move
    dict(name="ruins11", area="ruins", rect=(6490, 4680, 560, 260), title="Ruins"),
    # the mouse hole, its SAVE Point, the cheese
    dict(name="ruins12", area="ruins", rect=(7050, 4720, 380, 200), title="Ruins - Mouse Hole"),
    # Napstablook, pretending to sleep
    dict(name="ruins13", area="ruins", rect=(7430, 4700, 260, 220), title="Ruins"),
    # the Spider Bake Sale
    dict(name="ruins14", area="ruins", rect=(7690, 4700, 340, 220), title="Ruins"),
    # the Froggits' hall
    dict(name="ruins15", area="ruins", rect=(7520, 4400, 540, 300), title="Ruins"),
    # the one switch puzzle, and its six rooms under it
    dict(name="ruins16", area="ruins", rect=(8060, 4480, 640, 280), title="Ruins"),
    dict(name="ruins16a", area="ruins", rect=(8230, 4915, 160, 125), title="Ruins"),
    dict(name="ruins16b", area="ruins", rect=(8410, 4915, 140, 125), title="Ruins"),
    dict(name="ruins16c", area="ruins", rect=(8550, 4915, 140, 125), title="Ruins"),
    dict(name="ruins16d", area="ruins", rect=(8230, 5040, 160, 120), title="Ruins"),
    dict(name="ruins16e", area="ruins", rect=(8410, 5040, 140, 120), title="Ruins"),
    dict(name="ruins16f", area="ruins", rect=(8550, 5040, 140, 120), title="Ruins"),
    # the rotating rooms: the same room, turned; and the one a wrong switch drops Frisk in
    dict(name="ruins17a", area="ruins", rect=(8700, 4500, 360, 400), title="Ruins"),
    dict(name="ruins17b", area="ruins", rect=(8720, 4060, 340, 380), title="Ruins"),
    dict(name="ruins17c", area="ruins", rect=(8380, 4000, 340, 440), title="Ruins"),
    dict(name="ruins17d", area="ruins", rect=(8380, 3610, 320, 390), title="Ruins"),
    dict(name="ruins17x", area="ruins", rect=(9140, 4540, 280, 160), title="Ruins"),
    # the crossroads, the Froggit who saw Toriel, the balcony over Home
    dict(name="ruins18", area="ruins", rect=(8700, 3460, 590, 370), title="Ruins"),
    dict(name="ruins19", area="ruins", rect=(9290, 3620, 250, 200), title="Ruins"),
    dict(name="ruins20", area="ruins", rect=(9300, 3360, 360, 260), title="Ruins"),
    # Toriel's tree and her door
    dict(name="ruins21", area="ruins", rect=(8860, 2920, 320, 540), title="Ruins - Home"),
    # Toriel's home
    dict(name="torhouse1", area="ruins", rect=(8860, 2660, 320, 260), title="Home"),
    dict(name="torhouse2", area="ruins", rect=(8540, 2670, 320, 250), title="Home"),
    dict(name="torkitchen", area="ruins", rect=(8540, 2500, 200, 170), title="Home"),
    dict(name="torhouse3", area="ruins", rect=(9180, 2700, 760, 220), title="Home"),
    dict(name="asrielroom", area="ruins", rect=(9200, 2460, 260, 230), title="Home"),
    dict(name="torielroom", area="ruins", rect=(9510, 2430, 280, 260), title="Home"),
    # the basement, to the end of the Ruins
    dict(name="basement1", area="ruins", rect=(8860, 2220, 1260, 180), title="Ruins"),
    dict(name="basement2", area="ruins", rect=(9930, 1960, 220, 260), title="Ruins"),
    dict(name="basement3", area="ruins", rect=(9940, 300, 180, 1660), title="Ruins"),
    dict(name="ruinsexit", area="ruins", rect=(9940, 0, 200, 300), title="Ruins - Exit"),
]
