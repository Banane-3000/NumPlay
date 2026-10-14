"""The sprites and fonts the pack holds.

A sprite: (name, source or [sources], options). Sources (tools/pack.py load_frames):
  demo:spr_x     the free demo's sprite spr_x, all its frames (the game's size)
  demobg:bg_x    one of the demo's backgrounds
  wiki:File.png  a file of the Undertale Wiki, every frame of a GIF; the wiki shows sprites at twice the size,
                 halved here (wiki1: as it is)
Options: origin (x, y) in the source's pixels, center (its middle), feet (the middle of its bottom),
frames (which of them), keep (no crop to the opaque pixels)."""

SPRITES = [
    # Frisk walking: down and up 4 frames, left and right 2 (the game's own frames, from the demo)
    ("frisk_d", "demo:spr_maincharad", {"origin": (0, 0), "keep": True}),
    ("frisk_u", "demo:spr_maincharau", {"origin": (0, 0), "keep": True}),
    ("frisk_l", "demo:spr_maincharal", {"origin": (0, 0), "keep": True}),
    ("frisk_r", "demo:spr_maincharar", {"origin": (0, 0), "keep": True}),
    ("toriel_d", "demo:spr_toriel_d", {"origin": (0, 0), "keep": True}),
    ("toriel_u", "demo:spr_toriel_u", {"origin": (0, 0), "keep": True}),
    ("toriel_l", "demo:spr_toriel_l", {"origin": (0, 0), "keep": True}),
    ("toriel_r", "demo:spr_toriel_r", {"origin": (0, 0), "keep": True}),
    ("savepoint", "demo:spr_savepoint", {"origin": (0, 0), "keep": True}),
    ("flowey", "demo:spr_flowey", {"origin": (0, 0), "keep": True}),
    ("heart", "demo:spr_heart", {"center": True, "keep": True}),
] + [
    # faces in the dialogue box (the demo's: a head, and Toriel's shoulders under it); 2 frames: talking
    ("face_" + n, "demo:spr_face_" + n, {"keep": True}) for n in [
        "torieltalk", "torieltalkside", "torielhappytalk", "torielwhat", "torielwhatside", "torielmad", "torielcold",
        "torielsurprised", "torielembarrassed", "torielglasses", "torielrevengetalk", "torielscared", "torielblink",
        "torielhappyblink", "torielbody", "sans", "sanschuckle", "sanswink", "papyrus", "papyrusevil",
        "papyruslaugh", "papyrusmad", "papyrusside"]
] + [
    # Flowey's faces (his talks and his battle)
    (n, "demo:spr_" + n, {"keep": True}) for n in [
        "floweynice", "floweyniceside", "floweysassy", "floweyevil", "floweygrin", "floweyplain", "floweypissed",
        "floweywink", "floweylaugh", "floweyhurt", "floweytoriel", "floweytoriel2", "floweysideshock"]
]

FONTS = [
    # the dialogue's font: Pixel Operator Mono Bold, 8 pixels a letter
    dict(name="main", ttf="PixelOperatorMono-Bold.ttf", size=16, height=12, advance=8, ascent=13),
    # small text (the menu's numbers)
    dict(name="small", ttf="PixelOperator8.ttf", size=8, height=8, ascent=8),
]
