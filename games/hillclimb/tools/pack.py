#!/usr/bin/env python3
"""Hill Climb Racing's pictures and fonts -> src/data.bin, src/data.h and src/art.h.

    python3 -I tools/pack.py REF        (REF: the folder tools/fetch.py filled)

data.bin: "HCRD", the number of pictures and fonts, then a table of their offsets (u32), then each one:
- a picture: w, h, colours, flags (u16 each; flags 1: opaque), its pixels a meter x 4 (0: a picture of the menus),
  its pivot x 4 (s16, s16), 0; its colours as premultiplied R, G, B, A bytes; for an opaque one also as RGB565;
  then w x h colour indices, row by row
- a font: first character, count, line height, ascent (u8 each); then a glyph each (w, h, x and y of its top-left
  from the pen, advance, 3 bytes of 0, offset of its bitmap: u8 u8 s8 s8 u8, u32) and the bitmaps, a byte a pixel: the
  letter's coverage in the high 4 bits, its dark outline's in the low 4

data.h names them (IMG_*, FONT_*); art.h is tools/art.py's vehicles in meters, for src/run.c.
icon(REF) drew src/icon.png once (it is kept as it is now, with NumPlay's corners: tools/round_icons.py)."""
import math
import os
import struct
import subprocess
import sys
import tempfile

import numpy as np
from PIL import Image, ImageDraw, ImageFont

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import art  # noqa: E402

SRC = os.path.join(HERE, "..", "src")
VPPM = 44   # pixels a meter the vehicles, Bill and the things on the way are kept at (the game draws them at 30 or less)

# name, wiki file, crop box or None, how it is sized: ("ppm", source pixels a meter) keeps VPPM pixels a meter;
# ("w", width) or ("h", height) or ("size", w, h) for the menus; pivot ("c": centre, or (x, y) source pixels)
PICTURES = [
    # coins and fuel cans: 1.5 m squares in the game (Coin::create, FuelCanister::create)
    ("COIN5", "Coin5.png", None, ("ppm", 64 / 1.5), "c"),
    ("COIN25", "Coin25.png", None, ("ppm", 64 / 1.5), "c"),
    ("COIN100", "Coin100.png", None, ("ppm", 64 / 1.5), "c"),
    ("COIN500", "Coin500.png", None, ("ppm", 64 / 1.5), "c"),
    ("FUELCAN", "Fuel-canister.png", None, ("ppm", 64 / 1.5), "c"),
    ("CLOUD", "Sky-cloud.png", None, ("w", 76), "c"),
    ("TREE_TRUNK", "Lowres-tree-part1.png", None, ("ppm", 64 / 1.3), (32, 31)),
    ("TREE_TOP", "Lowres-tree-part2.png", None, ("ppm", 64 / 1.9), (32, 31)),
    ("PLANK", "BridgeWoodBlock.png", None, ("ppm", 64 / 1.08), "c"),   # (a plank: 1.08 x 0.3 m, its picture 0.59 m high)
    ("FLAG", "Flag.png", None, ("ppm", 128 / 1.6), (4, 62)),
    ("ROCKS", "Rocks.png", None, ("ppm", 64 / 1.2), "c"),
    ("ROCKS_MARS", "Rocks-mars.png", None, ("ppm", 64 / 1.2), "c"),
    ("ICICLE", "Iceshard.png", None, ("ppm", 64 / 1.4), (32, 2)),
    ("SMOKE", "Smoke.png", None, ("w", 20), "c"),
    ("DIRT", "Dirt.png", None, ("w", 20), "c"),
    ("SNOW", "Snow.png", None, ("w", 20), "c"),
    ("GRASS", "Countryside_Particle.png", None, ("w", 20), "c"),
    ("SLIME", "Alien-slime.png", None, ("w", 20), "c"),
    # skies (a column of colours from top to bottom) and the stages' ground
    ("SKY", "Sky-bg.png", None, ("size", 1, 32), "c"),
    ("SKY_CAVE", "Sky-bg-cave.png", None, ("size", 1, 32), "c"),
    ("SKY_FOREST", "Sky-bg-forest.png", None, ("size", 1, 32), "c"),
    ("SKY_MARS", "Sky-bg-mars.png", None, ("size", 1, 32), "c"),
    ("SKY_ARCTIC_CAVE", "Sky-bg-arctic-cave.png", None, ("size", 1, 32), "c"),
    ("SKY_HIGHWAY", "Highway-sky-bg.png", None, ("size", 1, 32), "c"),
    ("SKY_MOON", "Moon-bg.png", None, ("size", 1, 64), "c"),
    ("SURF_COUNTRY", "Lowres-terrain-surface.png", None, ("size", 256, 16), "c"),
    ("GROUND_COUNTRY", "Lowres-terrain-ground.png", None, ("size", 128, 128), "c"),
    ("SURF_DESERT", "Lowres-desert-surface.png", None, ("size", 256, 16), "c"),
    ("GROUND_DESERT", "Lowres-desert-ground.png", None, ("size", 128, 128), "c"),
    ("SURF_ARCTIC", "Lowres-arctic-surface.png", None, ("size", 256, 16), "c"),
    ("GROUND_ARCTIC", "Lowres-arctic-ground.png", None, ("size", 128, 128), "c"),
    ("SURF_HIGHWAY", "Lowres-highway-surface.png", None, ("size", 256, 16), "c"),
    ("GROUND_HIGHWAY", "Lowres-highway-ground.png", None, ("size", 128, 128), "c"),
    ("SURF_CAVE", "Lowres-terrain-cave-surface.png", None, ("size", 256, 16), "c"),
    ("SURF_MOON", "Lowres-moon-surface.png", None, ("size", 256, 16), "c"),
    ("GROUND_MOON", "Lowres-moon-ground.png", None, ("size", 128, 128), "c"),
    ("SURF_MARS", "Lowres-terrain-mars-surface.png", None, ("size", 256, 16), "c"),
    ("GROUND_MARS", "Lowres-mars-ground.png", None, ("size", 128, 128), "c"),
    ("SURF_ALIEN", "Lowres-terrain-alien-surface.png", None, ("size", 256, 16), "c"),
    ("GROUND_ALIEN", "Lowres-terrain-alien.png", None, ("size", 128, 128), "c"),
    ("SURF_FOREST", "Lowres-terrain-forest-surface.png", None, ("size", 256, 16), "c"),
    ("GROUND_FOREST", "Lowres-terrain-forest-ground.png", None, ("size", 128, 128), "c"),
    # the menus
    ("STAGE_COUNTRY", "Stage-icon-countryside.png", None, ("size", 144, 72), "c"),
    ("STAGE_DESERT", "Stage-icon-desert.png", None, ("size", 144, 72), "c"),
    ("STAGE_ARCTIC", "Stage-icon-arctic.png", None, ("size", 144, 72), "c"),
    ("STAGE_HIGHWAY", "Stage-icon-highway.png", None, ("size", 144, 72), "c"),
    ("STAGE_CAVE", "Stage-icon-cave.png", None, ("size", 144, 72), "c"),
    ("STAGE_MOON", "Stage-icon-moon.png", None, ("size", 144, 72), "c"),
    ("STAGE_MARS", "Stage-icon-mars.png", None, ("size", 144, 72), "c"),
    ("STAGE_ALIEN", "Stage-icon-alienplanet.png", None, ("size", 144, 72), "c"),
    ("STAGE_ARCTIC_CAVE", "Stage-icon-arctic-cave.png", None, ("size", 144, 72), "c"),
    ("STAGE_FOREST", "Stage-icon-forest.png", None, ("size", 144, 72), "c"),
    ("STAGE_LOCKED", "Stage-icon-locked.png", None, ("size", 144, 72), "c"),
    ("ICON_JEEP", "Mission-vehicle-jeep.png", None, ("w", 112), "c"),
    ("ICON_BIKE", "Mission-vehicle-bike.png", None, ("w", 112), "c"),
    ("ICON_MONSTER", "Mission-vehicle-monster.png", None, ("w", 112), "c"),
    ("ICON_QUAD", "Mission-vehicle-quadbike.png", None, ("w", 112), "c"),
    ("ICON_RACE", "Mission-vehicle-racecar.png", None, ("w", 112), "c"),
    ("ICON_RALLY", "Mission-vehicle-rallycar.png", None, ("w", 112), "c"),
    ("ICON_BUGGY", "Mission-vehicle-dunebuggy.png", None, ("w", 112), "c"),
    ("ICON_TANK", "Mission-vehicle-tank.png", None, ("w", 112), "c"),
    ("UP_ENGINE", "MainMenuUpgradeEngine.png", None, ("w", 40), "c"),
    ("UP_SUSPENSION", "MainMenuUpgradeSuspension.png", None, ("w", 40), "c"),
    ("UP_TIRES", "MainMenuUpgradeTires.png", None, ("w", 40), "c"),
    ("UP_AWD", "MainMenuUpgradeAWD.png", None, ("w", 40), "c"),
    ("UP_AIR", "MainMenuUpgradeBikeRotation.png", None, ("w", 40), "c"),
    ("UP_DOWNFORCE", "MainMenuUpgradeDownforce.png", None, ("w", 40), "c"),
    ("UP_FUEL", "MainMenuUpgradeFuel.png", None, ("w", 40), "c"),
    ("PEDAL_GAS", "Pedal-gas-normal.png", "trim", ("h", 43), "c"),
    ("PEDAL_GAS_DOWN", "Pedal-gas-pressed.png", "trim", ("h", 43), "c"),
    ("PEDAL_BRAKE", "Pedal-brake-normal.png", "trim", ("h", 43), "c"),
    ("PEDAL_BRAKE_DOWN", "Pedal-brake-pressed.png", "trim", ("h", 43), "c"),
    ("METER_RPM", "Meter-rpm.png", None, ("w", 50), "c"),
    ("METER_BOOST", "Meter-boost.png", None, ("w", 50), "c"),
    ("NEEDLE", "Meter-needle.png", None, ("h", 27), (16, 100)),
    ("FUEL_ICON", "Fuel-icon.png", None, ("w", 20), "c"),
    ("FUEL_LOW", "Fuel-warning.png", None, ("w", 70), "c"),
    ("COIN_ICON", "Coin-icon.png", None, ("w", 18), "c"),
    ("MENU_BG", "Menu-bg.png", None, ("size", 320, 240), "c"),
    ("LOGO", "Game-logo.png", "trim", ("w", 200), "c"),
    ("LOCK", "Lock.png", None, ("w", 28), "c"),
    ("START", "Icon-start.png", None, ("w", 30), "c"),
]

# the fonts, from Teko: name, the height of its capitals in pixels, weight, outline width
FONTS = [("FONT_S", 9, 560, 1), ("FONT_M", 13, 600, 1.5), ("FONT_L", 20, 620, 2), ("FONT_XL", 34, 640, 3)]


def ref_path(ref, name):
    return os.path.join(ref, "wiki", name)


def load(ref, name):
    if name == "buggy-tire":   # the Dune Buggy's tyre, from the picture of the whole buggy
        return Image.open(ref_path(ref, "Dune_Buggy_(Sprite).png")).convert("RGBA").crop((6, 133, 86, 213))
    return Image.open(ref_path(ref, name)).convert("RGBA")


def premul_resize(im, w, h):
    """Resized without the dark fringes straight alpha gives: in premultiplied colours."""
    a = np.asarray(im).astype(np.float32) / 255
    p = a.copy()
    p[..., :3] *= p[..., 3:4]
    planes = [Image.fromarray(p[..., i]).resize((w, h), Image.LANCZOS) for i in range(4)]
    q = np.stack([np.asarray(x) for x in planes], -1)
    q = np.clip(q, 0, 1)
    al = q[..., 3:4]
    rgb = np.where(al > 1e-4, q[..., :3] / np.maximum(al, 1e-4), 0)
    out = np.concatenate([np.clip(rgb, 0, 1), al], -1)
    return Image.fromarray((out * 255 + 0.5).astype(np.uint8), "RGBA")


def quantize(im, colors=256):
    """Palette and indices from pngquant (keeps the alpha's smooth edges)."""
    with tempfile.TemporaryDirectory() as d:
        src, dst = os.path.join(d, "in.png"), os.path.join(d, "out.png")
        im.save(src)
        subprocess.run(["pngquant", "--force", "--speed", "1", "--nofs", str(colors), "--output", dst, src],
                       check=True)
        q = Image.open(dst)
        q.load()
    if q.mode != "P":
        q = im.convert("P", palette=Image.ADAPTIVE, colors=colors)
    pal = q.getpalette()[:768]
    trns = q.info.get("transparency", b"")
    n = len(pal) // 3
    alpha = [255] * n
    if isinstance(trns, bytes):
        for i, a in enumerate(trns):
            alpha[i] = a
    elif isinstance(trns, int):
        alpha[trns] = 0
    idx = np.asarray(q, dtype=np.uint8)
    used = int(idx.max()) + 1
    cols = [(pal[3 * i], pal[3 * i + 1], pal[3 * i + 2], alpha[i]) for i in range(used)]
    return cols, idx


def rgb565(r, g, b):
    return (r >> 3) << 11 | (g >> 2) << 5 | b >> 3


def picture(im, ppm, pivot, opaque=False):
    cols, idx = quantize(im)
    h, w = idx.shape
    flags = 1 if opaque else 0
    out = bytearray(struct.pack("<HHHHHhhH", w, h, len(cols), flags, int(round(ppm * 4)),
                                int(round(pivot[0] * 4)), int(round(pivot[1] * 4)), 0))
    for r, g, b, a in cols:
        if opaque:
            a = 255
        out += bytes([r * a // 255, g * a // 255, b * a // 255, a])
    if opaque:
        for r, g, b, a in cols:
            out += struct.pack("<H", rgb565(r, g, b))
    out += idx.tobytes()
    while len(out) % 4:
        out.append(0)
    return out


def sized(im, how, crop):
    if crop == "trim":
        im = im.crop(im.getbbox())
    elif crop:
        im = im.crop(crop)
    kind = how[0]
    if kind == "ppm":
        k = min(1.0, VPPM / how[1])
        return premul_resize(im, max(1, round(im.width * k)), max(1, round(im.height * k))), k, VPPM if k < 1 else how[1]
    if kind == "w":
        k = how[1] / im.width
    elif kind == "h":
        k = how[1] / im.height
    else:
        w, h = how[1], how[2]
        if w == 1:   # a sky: the middle column
            col = im.crop((im.width // 2, 0, im.width // 2 + 1, im.height))
            return col.resize((1, h), Image.BILINEAR), 1, 0
        return premul_resize(im, w, h), w / im.width, 0
    return premul_resize(im, max(1, round(im.width * k)), max(1, round(im.height * k))), k, 0


def vehicle_pictures(ref):
    """The vehicles' pictures, kept at VPPM pixels a meter, and art.h's tables."""
    pics = []
    lines = ["/* Generated by tools/pack.py from tools/art.py: how the vehicles and Bill are drawn, in meters. */",
             "#ifndef HC_ART_H", "#define HC_ART_H", '#include "data.h"', ""]
    B = art.BILL
    body, head, leg = load(ref, B["body"]), load(ref, B["head"]), load(ref, B["leg"])
    bppm = math.dist(B["body_neck"], B["body_hips"]) / B["torso_m"]
    hb = head.getbbox()
    hppm = (hb[2] - hb[0]) / B["head_m"]
    lppm = leg.height / B["leg_m"]
    for name, im, ppm, piv in (("BILL_BODY", body, bppm, B["body_hips"]),
                               ("BILL_HEAD", head, hppm, ((hb[0] + hb[2]) / 2, (hb[1] + hb[3]) / 2)),
                               ("BILL_LEG", leg, lppm, B["leg_hip"])):
        k = VPPM / ppm
        pics.append((name, premul_resize(im, round(im.width * k), round(im.height * k)), VPPM,
                     (piv[0] * k, piv[1] * k), False))
    # the body picture's neck from its hips, Bill upright (meters, y up)
    lines.append(f"#define BILL_NECK_X {(B['body_neck'][0] - B['body_hips'][0]) / bppm:.3f}f")
    lines.append(f"#define BILL_NECK_Y {-(B['body_neck'][1] - B['body_hips'][1]) / bppm:.3f}f")
    lines.append("")
    lines.append("typedef struct {")
    lines.append("  uint8_t chassis, wheel, legs, ntubes, nlayers;")
    lines.append("  float wheel_r;                  /* the radius the wheel's picture is drawn for */")
    lines.append("  float tube[2][5];               /* from (x, y) to axle (index), colour 0xRRGGBB, radius */")
    lines.append("  float layer[2][4];              /* picture, its centre (x, y), in front (1) or behind */")
    lines.append("} VehArt;")
    lines.append(f"#define NVEH {len(art.VEHICLES)}")
    lines.append("static const VehArt veh_art[NVEH] = {")
    f = lambda x: f"{x:.3f}f"  # noqa: E731

    def sized_on(im, size):
        """A picture stretched to size (meters), at VPPM pixels a meter, pivot at its centre."""
        w, h = max(1, round(size[0] * VPPM)), max(1, round(size[1] * VPPM))
        return premul_resize(im, w, h), (w / 2, h / 2)

    for v in art.VEHICLES:
        cname, wname = "CH_" + v["id"].upper(), "WHEEL_" + v["id"].upper()
        im, piv = sized_on(load(ref, v["chassis"]), v["size"])
        pics.append((cname, im, VPPM, piv, False))
        wim = load(ref, v["wheel"])
        if "wheel_px" in v:   # a tyre cropped from a bigger picture: its radius in pixels is the wheel's 1.04 r
            side = wim.width / v["wheel_px"] * 1.04 * v["wheel_r"]
        else:
            side = 2.15 * v["wheel_r"]
        im, piv = sized_on(wim, (side, side * wim.height / wim.width))
        pics.append((wname, im, VPPM, piv, False))
        layers = []
        for (lpic, size, at, front) in v.get("layers", []):
            lname = "LAYER_" + lpic.split(".")[0].upper().replace("-", "_")
            im, piv = sized_on(load(ref, lpic), size)
            pics.append((lname, im, VPPM, piv, False))
            layers.append((lname, at, front))
        tubes = [(p[0], p[1], axi, col, r) for (p, axi, col, r) in v.get("tubes", [])]
        tb = ", ".join("{" + ", ".join([f(t[0]), f(t[1]), f(t[2]), f"{float(t[3])}f", f(t[4])]) + "}" for t in tubes) or "{0}"
        ly = ", ".join("{IMG_" + n + ", " + f(p[0]) + ", " + f(p[1]) + ", " + ("1" if fr else "0") + "}"
                       for n, p, fr in layers) or "{0}"
        lines.append(f"  {{IMG_{cname}, IMG_{wname}, {1 if v.get('legs') else 0}, {len(tubes)}, {len(layers)}, "
                     f"{f(v['wheel_r'])}, {{{tb}}}, {{{ly}}}}},   /* {v['name']} */")
    lines.append("};")
    lines.append("#endif")
    return pics, lines


def font_data(ttf, cap, weight, outline):
    up = 4
    for size in range(6, 400):
        f = ImageFont.truetype(ttf, size * up)
        try:
            f.set_variation_by_axes([weight])
        except Exception:
            pass
        bb = f.getbbox("H", anchor="ls")
        if -bb[1] >= cap * up:
            break
    o = int(round(outline * up))
    glyphs, data = [], bytearray()
    asc = desc = 0
    for c in range(32, 127):
        ch = chr(c)
        adv = round(f.getlength(ch) / up + outline)
        bb = f.getbbox(ch, anchor="ls")
        if bb[2] <= bb[0]:
            glyphs.append((0, 0, 0, 0, adv, len(data)))
            continue
        pad = o + 2 * up
        x0, y0 = pad - bb[0], pad - bb[1]
        x0 += (-x0) % up
        y0 += (-y0) % up
        w, h = x0 + bb[2] + pad, y0 + bb[3] + pad
        w += (-w) % up
        h += (-h) % up
        fill = Image.new("L", (w, h))
        ImageDraw.Draw(fill).text((x0, y0), ch, font=f, fill=255, anchor="ls")
        line = Image.new("L", (w, h))
        ImageDraw.Draw(line).text((x0, y0), ch, font=f, fill=255, anchor="ls", stroke_width=o)
        fa = np.asarray(fill, np.float32).reshape(h // up, up, w // up, up).mean(axis=(1, 3)) / 255
        la = np.asarray(line, np.float32).reshape(h // up, up, w // up, up).mean(axis=(1, 3)) / 255
        fq = np.clip(np.round(fa * 15), 0, 15).astype(np.uint8)
        lq = np.clip(np.round(la * 15), 0, 15).astype(np.uint8)
        ys, xs = np.nonzero(lq)
        if not len(ys):
            glyphs.append((0, 0, 0, 0, adv, len(data)))
            continue
        y_a, y_b, x_a, x_b = ys.min(), ys.max() + 1, xs.min(), xs.max() + 1
        fq, lq = fq[y_a:y_b, x_a:x_b], lq[y_a:y_b, x_a:x_b]
        gw, gh = x_b - x_a, y_b - y_a
        ox, oy = x_a - x0 // up, y_a - y0 // up
        if c < 127:
            asc, desc = max(asc, -oy), max(desc, gh + oy)
        glyphs.append((gw, gh, ox, oy, adv, len(data)))
        data += (fq << 4 | lq).astype(np.uint8).tobytes()
    out = bytearray(struct.pack("<BBBB", 32, len(glyphs), asc + desc, asc))
    for gw, gh, ox, oy, adv, off in glyphs:
        out += struct.pack("<BBbbBxxxI", gw, gh, ox, oy, adv, off)
    out += data
    while len(out) % 4:
        out.append(0)
    return out


def icon(ref):
    """The app's icon (55 x 56): the Hill Climber on a hill under the game's sky, in a rounded square."""
    k = 4
    W, H = 55 * k, 56 * k
    sky = load(ref, "Sky-bg.png").convert("RGB").resize((1, 32))
    im = Image.new("RGBA", (W, H))
    for y in range(H):
        im.paste(sky.getpixel((0, y * 32 // H)) + (255,), (0, y, W, y + 1))
    d = ImageDraw.Draw(im)
    pts = [(x, int(H * 0.74 - 18 * k * math.sin(x / W * 2.6 + 0.4))) for x in range(0, W + 1, 4)]
    d.polygon(pts + [(W, H), (0, H)], fill=(84, 52, 28, 255))
    d.line(pts, fill=(126, 198, 43, 255), width=5 * k)
    car = load(ref, "Mission-vehicle-jeep.png").crop((0, 0, 206, 145))
    car = car.resize((52 * k, round(52 * k * car.height / car.width)), Image.LANCZOS).rotate(8, Image.BICUBIC, expand=True)
    im.alpha_composite(car, ((W - car.width) // 2, H - car.height - 6 * k))
    head = load(ref, "Driver-head.png")
    head = head.resize((19 * k, 19 * k), Image.LANCZOS)
    im.alpha_composite(head, (W // 2 - 11 * k, 8 * k))
    mask = Image.new("L", (W, H))
    ImageDraw.Draw(mask).rounded_rectangle((0, 0, W - 1, H - 1), radius=12 * k, fill=255)
    im.putalpha(Image.fromarray(np.minimum(np.asarray(im)[..., 3], np.asarray(mask))))
    im.resize((55, 56), Image.LANCZOS).save(os.path.join(SRC, "icon.png"))


def main():
    ref = sys.argv[1]
    pics = []
    for name, file, crop, how, pivot in PICTURES:
        im, k, ppm = sized(load(ref, file), how, crop)
        if pivot == "c":
            pv = (im.width / 2, im.height / 2)
        else:
            pv = (pivot[0] * k, pivot[1] * k)
        opaque = name.startswith(("GROUND_", "SKY", "MENU_BG", "STAGE_"))
        pics.append((name, im, ppm, pv, opaque))
    vp, art_lines = vehicle_pictures(ref)
    pics += vp
    blobs = [picture(im, ppm, pv, opaque) for _, im, ppm, pv, opaque in pics]
    ttf = os.path.join(ref, "fonts", "Teko.ttf")
    fonts = [font_data(ttf, cap, wt, ol) for _, cap, wt, ol in FONTS]
    head = 16 + 4 * (len(blobs) + len(fonts))
    offs, at = [], head
    for b in blobs + fonts:
        offs.append(at)
        at += len(b)
    out = bytearray(b"HCRD" + struct.pack("<III", len(blobs), len(fonts), 0))
    out += b"".join(struct.pack("<I", o) for o in offs)
    for b in blobs + fonts:
        out += b
    with open(os.path.join(SRC, "data.bin"), "wb") as f:
        f.write(out)
    h = ["/* Generated by tools/pack.py: what src/data.bin holds. */", "#ifndef HC_DATA_H", "#define HC_DATA_H",
         "#include <stdint.h>", "", "enum {"]
    h += [f"  IMG_{name}," for name, *_ in pics]
    h += ["  IMG_COUNT", "};", "enum {"]
    h += [f"  {name}," for name, *_ in FONTS]
    h += ["  FONT_COUNT", "};", "#endif", ""]
    with open(os.path.join(SRC, "data.h"), "w") as f:
        f.write("\n".join(h))
    with open(os.path.join(SRC, "art.h"), "w") as f:
        f.write("\n".join(art_lines) + "\n")
    print(f"data.bin: {len(out)} bytes, {len(blobs)} pictures, {len(fonts)} fonts")
    for (name, im, *_), b in sorted(zip(pics, blobs), key=lambda x: -len(x[1]))[:8]:
        print(f"  {name}: {im.width}x{im.height}, {len(b)} bytes")


if __name__ == "__main__":
    main()
