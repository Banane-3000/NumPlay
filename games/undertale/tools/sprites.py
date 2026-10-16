"""The sprites and fonts the pack holds.

A sprite: (name, source or [sources], options). Sources (tools/pack.py load_frames):
  demo:spr_x     the free demo's sprite spr_x, all its frames (the game's size)
  demobg:bg_x    one of the demo's backgrounds
  wiki:File.png  a file of the Undertale Wiki, every frame of a GIF; the wiki shows sprites at twice the size,
                 halved here (wiki1: as it is)
Options: origin (x, y) in the source's pixels, center (its middle), feet (the middle of its bottom),
frames (which of them), keep (no crop to the opaque pixels), every (one frame in n), clear (colours made
see-through), mirror (flipped left to right), box (x, y, w, h: a part of the source, after halving).
More sources: move:File.gif:x0,y0,x1,y1:frames (what moves in a wiki screenshot GIF), map:Map.png:x,y,w,h (a piece
of an area's map), gen:name (drawn here, GEN)."""
from PIL import Image, ImageDraw


def _img(w, h):
    return Image.new("RGBA", (w, h), (0, 0, 0, 0))


def gen_otile():
    # the XOXO puzzles' O (red), and once the puzzle's solved, the same in green
    out = []
    for c in ((255, 0, 0, 255), (0, 192, 0, 255)):
        im = _img(20, 20)
        d = ImageDraw.Draw(im)
        d.ellipse([3, 3, 16, 16], outline=c, width=2)
        out.append(im)
    return out


def gen_snowball():
    # the Ball Game's snowball, from big to small (it melts as it rolls)
    out = []
    for r in (10, 9, 8, 7, 6, 5, 4, 3):
        im = _img(22, 22)
        d = ImageDraw.Draw(im)
        d.ellipse([11 - r, 11 - r, 11 + r, 11 + r], fill=(255, 255, 255, 255), outline=(0, 0, 0, 255))
        d.arc([11 - r + 2, 11 - r + 2, 11 + r - 2, 11 + r - 2], 200, 300, fill=(180, 192, 220, 255))
        out.append(im)
    return out


def gen_bone():
    # a bone 6 pixels wide, its ends' knobs (drawn stretched to any length: gfx_bone)
    im = _img(6, 6)
    d = ImageDraw.Draw(im)
    d.rectangle([0, 0, 5, 5], fill=(255, 255, 255, 255))
    return [im]


def gen_orb():
    im = _img(10, 10)
    d = ImageDraw.Draw(im)
    d.ellipse([0, 0, 9, 9], fill=(200, 220, 255, 255), outline=(255, 255, 255, 255))
    return [im]


def gen_snowflake():
    im = _img(9, 9)
    d = ImageDraw.Draw(im)
    for a, b in (((4, 0), (4, 8)), ((0, 4), (8, 4)), ((1, 1), (7, 7)), ((1, 7), (7, 1))):
        d.line([a, b], fill=(255, 255, 255, 255))
    return [im]


def gen_iceshard():
    im = _img(7, 11)
    d = ImageDraw.Draw(im)
    d.polygon([(3, 0), (6, 4), (3, 10), (0, 4)], fill=(255, 255, 255, 255))
    return [im]


def gen_present():
    im = _img(10, 10)
    d = ImageDraw.Draw(im)
    d.rectangle([0, 2, 9, 9], fill=(255, 255, 255, 255))
    d.rectangle([4, 2, 5, 9], fill=(0, 0, 0, 255))
    d.rectangle([0, 5, 9, 5], fill=(0, 0, 0, 255))
    d.line([(2, 0), (4, 2)], fill=(255, 255, 255, 255))
    d.line([(7, 0), (5, 2)], fill=(255, 255, 255, 255))
    return [im]


def gen_knife():
    # Doggo's dog-knife: a blade, up
    im = _img(5, 30)
    d = ImageDraw.Draw(im)
    d.polygon([(2, 0), (4, 6), (4, 24), (0, 24), (0, 6)], fill=(255, 255, 255, 255))
    d.rectangle([0, 24, 4, 29], fill=(255, 255, 255, 255))
    return [im]


def gen_dogpaw():
    im = _img(9, 9)
    d = ImageDraw.Draw(im)
    d.ellipse([2, 4, 7, 8], fill=(255, 255, 255, 255))
    for x in (1, 4, 7):
        d.ellipse([x - 1, 0, x + 1, 2], fill=(255, 255, 255, 255))
    return [im]


def gen_heartbullet():
    im = _img(9, 8)
    d = ImageDraw.Draw(im)
    d.polygon([(0, 2), (2, 0), (4, 2), (6, 0), (8, 2), (4, 7)], fill=(255, 255, 255, 255))
    return [im]


GEN = {"otile": gen_otile, "snowball": gen_snowball, "bone": gen_bone, "orb": gen_orb, "snowflake": gen_snowflake,
       "iceshard": gen_iceshard, "present": gen_present, "knife": gen_knife, "dogpaw": gen_dogpaw,
       "heartbullet": gen_heartbullet}

SNOW_CLEAR = [(133, 163, 216), (114, 129, 177), (139, 163, 186), (180, 192, 201), (195, 207, 216)]
PAP_GIF = "move:Papyrus_screenshot_back_and_forth.gif:0,280,640,450:"

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
    ("heart", "demo2:spr_heart", {"center": True, "keep": True, "tints": [(0, 60, 255)]}),   # red, blue
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
    # ---- Snowdin
    # Papyrus walking (from a screenshot of him pacing at home: right; left is the same, mirrored), and facing you
    ("papyrus_r", PAP_GIF + "19,21,23,25", {"feet": True}),
    ("papyrus_l", PAP_GIF + "19,21,23,25", {"feet": True, "mirror": True}),
    ("papyrus_d", "wiki:Papyrus_overworld.png", {"feet": True}),
    ("sans_d", "wiki:Sans_overworld.png", {"feet": True}),
    ("doggo_ow", "wiki:Doggo_overworld.png", {"feet": True}),
    ("dogamy_ow", "wiki:Dogamy_overworld.png", {"feet": True}),
    ("dogaressa_ow", "wiki:Dogaressa_overworld.png", {"feet": True}),
    ("greaterdog_ow", "wiki:Greater_Dog_overworld.png", {"feet": True}),
    ("poff", "wiki:Greater_Dog_overworld_snow_poff.png", {"feet": True}),
    ("dog_ow", "wiki:Annoying_Dog_sprite.gif", {"feet": True}),
    ("dog_bone", "wiki:Annoying_Dog_sprite_bone.gif", {"feet": True, "dedupe": True}),
    # the puzzles' things: an X tile, its O, Snowdin's spikes (all cut from the map: its snow made see-through),
    # the switches, the snowball, the electric orb
    ("xtile", "map:Snowdin_map.png:7460,1240,20,20", {"keep": True, "clear": SNOW_CLEAR}),
    ("otile", "gen:otile", {"keep": True}),
    ("snowspikes", "map:Snowdin_map.png:7680,1220,20,20", {"keep": True, "clear": SNOW_CLEAR}),
    ("snowswitch", "map:Snowdin_map.png:7498,1196,24,14", {"keep": True, "clear": SNOW_CLEAR}),
    ("snowball", "gen:snowball", {"center": True, "keep": True}),
    ("lamp", "map:Snowdin_map.png:2960,365,19,31", {"keep": True, "clear": SNOW_CLEAR + [(44, 46, 63)]}),
    ("papyrus_zap", "wiki:Papyrus_overworld_electrocuted.gif", {"feet": True, "dedupe": True}),
] + [
    # the Ball Game's flags, by the time the ball took
    ("flag_" + c, "wiki:%s_flag_overworld.png" % n, {"feet": True}) for c, n in [
        ("lightblue", "Light_blue"), ("purple", "Purple"), ("green", "Green"), ("blue", "Blue"), ("yellow", "Yellow"),
        ("orange", "Orange"), ("red", "Red")]
] + [
    ("orb", "gen:orb", {"center": True, "keep": True}),
    # the shop's picture (QC behind her counter): the top of the wiki's screenshot
    ("shop_qc", "wiki:QC_screenshot_shop.png", {"keep": True, "box": (0, 0, 320, 120)}),
] + [
    # Snowdin's bullets, their middle at their origin
    (n, "gen:" + n, {"center": True}) for n in ["bone", "snowflake", "iceshard", "present", "knife", "dogpaw",
                                                 "heartbullet"]
] + [
    # Snowdin's monsters
    ("snowdrake_b", "wiki:Snowdrake_battle.gif", {"feet": True, "clear_bg": True, "every": 6}),
    ("snowdrake_hurt", "wiki:Snowdrake_battle_hurt.png", {"feet": True, "clear_bg": True}),
    ("icecap_b", "wiki:Ice_Cap_battle_idle.gif", {"feet": True, "clear_bg": True}),
    ("icecap_hurt", "wiki:Ice_Cap_battle_hurt.png", {"feet": True, "clear_bg": True}),
    ("icecap_hatless", "wiki:Ice_Cap_battle_hatless.png", {"feet": True, "clear_bg": True}),
    ("gyftrot_b", "wiki:Gyftrot_battle_idle.gif", {"feet": True, "clear_bg": True, "every": 14}),
    ("gyftrot_hurt", "wiki:Gyftrot_battle_hurt.png", {"feet": True, "clear_bg": True}),
    ("doggo_b", "wiki:Doggo_battle_idle.gif", {"feet": True, "clear_bg": True, "dedupe": True, "every": 8}),
    ("doggo_hurt", "wiki:Doggo_battle_hurt.png", {"feet": True, "clear_bg": True}),
    ("lesserdog_b", "wiki:Lesser_Dog_battle_idle.gif", {"feet": True, "clear_bg": True}),
    ("lesserdog_hurt", "wiki:Lesser_Dog_battle_hurt.png", {"feet": True, "clear_bg": True}),
    ("greaterdog_b", "wiki:Greater_Dog_battle.gif", {"feet": True, "clear_bg": True}),
    ("greaterdog_hurt", "wiki:Greater_Dog_battle_hurt.png", {"feet": True, "clear_bg": True}),
    ("dogamy_b", "wiki:Dogamy_battle.png", {"feet": True, "clear_bg": True}),
    ("dogaressa_b", "wiki:Dogaressa_battle.png", {"feet": True, "clear_bg": True}),
    ("jerry_b", "wiki:Jerry_battle.gif", {"feet": True, "clear_bg": True}),
    ("jerry_hurt", "wiki:Jerry_battle_hurt.png", {"feet": True, "clear_bg": True}),
    ("chilldrake_b", "wiki:Chilldrake_battle_idle.gif", {"feet": True, "clear_bg": True, "every": 6}),
    ("glyde_b", "wiki:Glyde_battle_idle.gif", {"feet": True, "clear_bg": True, "dedupe": True, "every": 15}),
    ("glyde_hurt", "wiki:Glyde_battle_hurt.png", {"feet": True, "clear_bg": True}),
    ("papyrus_b", "wiki:Papyrus_battle.png", {"feet": True, "clear_bg": True}),
    ("papyrus_hurt", "wiki:Papyrus_battle_hurt.png", {"feet": True, "clear_bg": True}),
    ("papyrus_confused", "wiki:Papyrus_battle_confused.gif", {"feet": True, "clear_bg": True, "dedupe": True}),
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
