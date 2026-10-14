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
    # the SOUL, 8 x 8 as the game's 16 x 16 halved; frame 1: blue
    ("heart", "demo2:spr_heart", {"center": True, "keep": True}),
] + [
    # the battle: its buttons (orange, then yellow when chosen), the FIGHT target and its bar, the slash, the HP label
    (n, "demo2:spr_" + n, {"keep": True}) for n in [
        "fightbt", "talkbt", "itembt", "sparebt", "target", "targetchoice", "hpname", "battlebg"]
] + [
    ("strike", "demo2:spr_strike", {"center": True, "keep": True}),
    ("heartbreak", "demo2:spr_heartbreak", {"center": True, "keep": True}),
    ("heartshards", "demo2:spr_heartshards", {"center": True, "keep": True}),
    ("heartgtfo", "demo2:spr_heartgtfo", {"center": True, "keep": True}),
] + [
    # bullets, their middle at their origin
    (n, "demo2:spr_" + n, {"center": True}) for n in [
        "flybullet", "smallfrogbullet", "butterflybllt", "bulletmd", "bulletsm", "circlebulletmd1", "circlebulletsm",
        "vegbullet", "carrotbullet", "roachbullet", "roachdance", "teardrop", "firebullet", "handbullet", "spinbullet",
        "streambullet", "dirbullet", "bulletgenmd"]
] + [
    # the monsters, from the wiki (the game's battle sprites, halved); their origin at their feet
    ("dummy_b", "wiki:Dummy_battle.png", {"feet": True}),
    ("froggit_b", "wiki:Froggit_battle_idle.gif", {"feet": True, "dedupe": True, "every": 2}),
    ("froggit_hurt", "wiki:Froggit_battle_hurt.png", {"feet": True}),
    ("whimsun_b", "wiki:Whimsun_battle_idle.gif", {"feet": True, "dedupe": True, "every": 4}),
    ("whimsun_hurt", "wiki:Whimsun_battle_hurt.png", {"feet": True}),
    ("moldsmal_b", "wiki:Moldsmal_battle.gif", {"feet": True, "dedupe": True, "every": 4}),
    ("loox_b", "wiki:Loox_battle_idle.gif", {"feet": True, "dedupe": True}),
    ("loox_hurt", "wiki:Loox_battle_hurt.png", {"feet": True}),
    ("vegetoid_b", "wiki:Vegetoid_battle_idle.gif", {"feet": True, "dedupe": True}),
    ("vegetoid_hurt", "wiki:Vegetoid_battle_hurt.png", {"feet": True}),
    ("migosp_b", "wiki:Migosp_battle_idle.gif", {"feet": True, "dedupe": True, "every": 2}),
    ("migosp_hurt", "wiki:Migosp_battle_hurt.png", {"feet": True}),
    ("napstablook_b", "wiki:Napstablook_battle_idle.gif", {"feet": True, "dedupe": True}),
    ("napsta_hat", "demo:spr_blookhat", {"feet": True}),
    ("toriel_b", "wiki:Toriel_battle.png", {"feet": True}),
    ("toriel_hurt", "wiki:Toriel_battle_hurt.png", {"feet": True}),
    ("toriel_away", "wiki:Toriel_battle_shocked.png", {"feet": True}),
    ("toriel_fight", "wiki:Toriel_battle_in_fight.png", {"feet": True}),
    ("toriel_dead", "wiki:Toriel_battle_fatal_Neutral.png", {"feet": True}),
] + [
    # the Ruins' things, from the demo (their top-left at their origin, as the game places them)
    (n.replace("spr_", "o_"), "demo:" + n, {"keep": True}) for n in [
        "spr_ruinsdoor1", "spr_switch", "spr_groundswitch1", "spr_spiketile", "spr_rock", "spr_colorswitch",
        "spr_faceswitch", "spr_dummy", "spr_exc", "spr_floweygrow", "spr_floweyshrink", "spr_floweysink",
        "spr_floweylaughoverworld", "spr_toriel_handhold_d", "spr_toriel_handhold_u", "spr_toriel_handhold_l",
        "spr_toriel_handhold_r", "spr_hole", "spr_smallweb", "spr_bigweb", "spr_napstablook_gr", "spr_napstablook_grsm",
        "spr_napstablook_l", "spr_chairempty", "spr_chairiel", "spr_chairielgetup", "spr_candydish", "spr_candydish2",
        "spr_candydish_bad", "spr_cheesetable", "spr_mousehole", "spr_npc_d", "spr_npc_u", "spr_npc_l", "spr_npc_r",
        "spr_pillar", "spr_pieslice", "spr_bigpie", "spr_vegetableoutside", "spr_smallfrog", "spr_ribbon",
        "spr_groundknife", "spr_torielflame", "spr_winkstar", "spr_tornote", "spr_diningtable", "spr_nastytree",
        "spr_toriel_d_phone", "spr_toriel_dt", "spr_toriel_ut", "spr_toriel_lt", "spr_toriel_rt", "spr_toriel_hug",
        "spr_toriel_ruffle"]
] + [
    ("frog_ow", "wiki:Froggit_overworld.png", {"keep": True}),
    ("napsta_ow", "wiki:Napstablook_overworld.png", {"keep": True}),
] + [
    # the intro's pictures, the title, GAME OVER
    ("introimage", "demo:spr_introimage", {"keep": True, "clear_black": True}),
    ("introlast", "demo:spr_introlast", {"keep": True, "clear_black": True}),
    ("undertaletitle", "demo:spr_undertaletitle", {"clear_black": True}),
    ("gameoverbg", "demo2:spr_gameoverbg", {"clear_black": True}),
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
    # the menus' text: Pixel Operator Bold
    dict(name="menu", ttf="PixelOperator-Bold.ttf", size=16, height=12, ascent=13),
    # small text (the menu's numbers)
    dict(name="small", ttf="PixelOperator8.ttf", size=8, height=8, ascent=8),
]
