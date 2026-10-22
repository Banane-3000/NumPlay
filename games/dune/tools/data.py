#!/usr/bin/env python3
"""Dune's pictures and font, from the web build (python3 -I tools/data.py WEB [--preview DIR]).

WEB is the game's folder (index.html, js/, assets/); it is only read. Writes src/art.c and src/art.h:
the 26 balls, the coin and the interface's icons, the seven landscapes (their sky, layers, sprites, ground,
preview and shop card) and the Bango Pro font, all at the calculator's scale (320 / 900 of the game's points).

Pictures are 4-bit, with their own 16 colours (index 0 is transparent): `dn_raw` stored as they are (for
pictures drawn turned or scaled), `dn_rle` as rows of runs (the big ones, drawn a row at a time).
Glyphs and white icons are 4-bit coverage masks.
"""
import json
import math
import os
import re
import sys

from PIL import Image, ImageDraw

S = 320 / 900          # calculator pixels per game point
OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "src")


def rgb565(c):
    return ((c[0] >> 3) << 11) | ((c[1] >> 2) << 5) | (c[2] >> 3)


class Out:
    def __init__(self):
        self.c, self.h, self.bytes = [], [], 0

    def arr(self, typ, name, values, static=True):
        size = {"uint8_t": 1, "uint16_t": 2, "int16_t": 2, "uint32_t": 4}[typ]
        self.bytes += size * len(values)
        lines = [",".join(str(v) for v in values[i:i + 32]) for i in range(0, len(values), 32)]
        self.c.append(f"{'static ' if static else ''}const {typ} {name}[{len(values)}] = {{\n" + ",\n".join(lines) + "};")

    def line(self, s):
        self.c.append(s)


def pixels(im):
    return list(im.get_flattened_data()) if hasattr(im, "get_flattened_data") else list(im.getdata())


def quantize(images, ncolors=15):
    """One palette (index 0 transparent, then up to 15 colours) for RGBA images; returns (palette, index lists)."""
    opaque = []
    for im in images:
        opaque += [p[:3] for p in pixels(im) if p[3] >= 128]
    if not opaque:
        opaque = [(0, 0, 0)]
    strip = Image.new("RGB", (len(opaque), 1))
    strip.putdata(opaque)
    n = min(ncolors, len(set(opaque)))
    q = strip.quantize(colors=n, method=Image.Quantize.MEDIANCUT, dither=Image.Dither.NONE)
    pal = q.getpalette()[:3 * n]
    cols = [tuple(pal[i * 3:i * 3 + 3]) for i in range(n)]
    cache = {}

    def near(c):
        if c not in cache:
            cache[c] = min(range(n), key=lambda i: (cols[i][0] - c[0]) ** 2 * 3 + (cols[i][1] - c[1]) ** 2 * 4 + (cols[i][2] - c[2]) ** 2 * 2)
        return cache[c]
    out = []
    for im in images:
        out.append([0 if p[3] < 128 else near(p[:3]) + 1 for p in pixels(im)])
    return [0] + [rgb565(c) for c in cols] + [0] * (15 - n), out


def nib(idx):
    return [(idx[i] << 4) | (idx[i + 1] if i + 1 < len(idx) else 0) for i in range(0, len(idx), 2)]


def rle_row(row):
    """Runs: a byte >= 128 is a run of (byte - 127) pixels of the colour in the next byte; below 128, (byte + 1)
    pixels follow, two to a byte."""
    out, x, w = [], 0, len(row)
    while x < w:
        e = x + 1
        while e < w and row[e] == row[x] and e - x < 128:
            e += 1
        if e - x >= 3 or row[x] == 0:
            out += [127 + e - x, row[x]]
            x = e
            continue
        s = x
        while x < w and x - s < 128:
            if row[x] == 0 or (x + 2 < w and row[x] == row[x + 1] == row[x + 2]):
                break
            x += 1
        out += [x - s - 1] + nib(row[s:x])
    return out


def scaled(im, w, h):
    im = im.convert("RGBA")
    # premultiplied, so the transparent edge doesn't darken the colours
    r, g, b, a = im.split()
    pm = Image.merge("RGBA", (Image.composite(r, Image.new("L", im.size), a), Image.composite(g, Image.new("L", im.size), a),
                              Image.composite(b, Image.new("L", im.size), a), a))
    pm = pm.resize((max(1, w), max(1, h)), Image.LANCZOS)
    px = []
    for (r_, g_, b_, a_) in pixels(pm):
        px.append((min(255, r_ * 255 // a_), min(255, g_ * 255 // a_), min(255, b_ * 255 // a_), a_) if a_ else (0, 0, 0, 0))
    o = Image.new("RGBA", pm.size)
    o.putdata(px)
    return o


class Art:
    def __init__(self, web, preview):
        self.web, self.o, self.preview = web, Out(), preview
        self.decl = []
        self.sizes = {}

    def path(self, *p):
        return os.path.join(self.web, "assets", *p)

    def show(self, name, im, pal=None, idx=None):
        if not self.preview:
            return
        os.makedirs(self.preview, exist_ok=True)
        if pal is not None:
            cols = [((c >> 11) * 255 // 31, (c >> 5 & 63) * 255 // 63, (c & 31) * 255 // 31, 255) for c in pal]
            cols[0] = (255, 0, 255, 0)
            im = Image.new("RGBA", im.size)
            im.putdata([cols[i] for i in idx])
        im.save(os.path.join(self.preview, name + ".png"))

    def count(self, group, n):
        self.sizes[group] = self.sizes.get(group, 0) + n

    def raw(self, name, im, group, pal=None, idx=None):
        if pal is None:
            pal, (idx,) = quantize([im])
        self.show(name, im, pal, idx)
        b0 = self.o.bytes
        self.o.arr("uint16_t", name + "_pal", pal)
        self.o.arr("uint8_t", name + "_px", nib(idx))
        self.o.line(f"const dn_raw {name} = {{{im.width}, {im.height}, {name}_pal, {name}_px}};")
        self.decl.append(f"extern const dn_raw {name};")
        self.count(group, self.o.bytes - b0)

    def rle(self, name, im, group, rows=None):
        pal, (idx,) = quantize([im])
        w, h = im.size
        if rows is not None:
            h = min(h, rows)
        self.show(name, im.crop((0, 0, w, h)), pal, idx[:w * h])
        data, offs = [], []
        for y in range(h):
            offs.append(len(data))
            data += rle_row(idx[y * w:(y + 1) * w])
        offs.append(len(data))
        assert len(data) < 65536, name
        b0 = self.o.bytes
        self.o.arr("uint16_t", name + "_pal", pal)
        self.o.arr("uint16_t", name + "_rows", offs)
        self.o.arr("uint8_t", name + "_data", data)
        self.o.line(f"const dn_rle {name} = {{{w}, {h}, {name}_pal, {name}_rows, {name}_data}};")
        self.decl.append(f"extern const dn_rle {name};")
        self.count(group, self.o.bytes - b0)

    def mask(self, name, im, group):
        """A white picture: its coverage, 4 bits."""
        a = [min(15, (p[3] * (p[0] + p[1] + p[2]) // 765 + 8) // 17) for p in pixels(im.convert("RGBA"))]
        b0 = self.o.bytes
        self.o.arr("uint8_t", name + "_px", nib(a))
        self.o.line(f"const dn_mask {name} = {{{im.width}, {im.height}, {name}_px}};")
        self.decl.append(f"extern const dn_mask {name};")
        self.count(group, self.o.bytes - b0)
        if self.preview:
            self.show(name, Image.merge("RGBA", [Image.new("L", im.size, 255)] * 3 + [Image.frombytes("L", im.size, bytes(v * 17 for v in a))]))


def atlas(web, name):
    d = json.load(open(os.path.join(web, "assets", "img_480", name + ".json")))
    im = Image.open(os.path.join(web, "assets", "img_480", name + ".png")).convert("RGBA")
    return {k: im.crop((v["frame"]["x"], v["frame"]["y"], v["frame"]["x"] + v["frame"]["w"], v["frame"]["y"] + v["frame"]["h"]))
            for k, v in d["frames"].items()}


def balls_from_source(web):
    """The BALLS table of js/game.js, read as data."""
    src = open(os.path.join(web, "js", "game.js"), encoding="utf8").read()
    start = src.index("BALLS = [", src.index("ballMoveInput = !1"))
    body = src[start:src.index("BALLS_ORIGINAL = BALLS", start)]
    balls = []
    for m in re.finditer(r'\{\s*rotates: !([01]),\s*spriteName: "",\s*name: "([^"]+)",\s*price: ([0-9e.]+),\s*trailData: \{(.*?)\}\s*,?\s*\}', body, re.S):
        rot, name, price, trail = m.groups()
        t = {}
        for k in ("gradient_type", "height", "gradient_offsetX", "offsetY"):
            mm = re.search(k + r": (-?[0-9.]+)", trail)
            if mm:
                t[k] = float(mm.group(1))
        cm = re.search(r'colors: \[([^\]]*)\]', trail)
        if cm:
            cols = re.findall(r'"#([0-9a-f]{6})"', cm.group(1))
        else:
            cols = re.search(r'colors: "([^"]*)"\.split', trail).group(1).replace("#", "").split()
        balls.append({"name": name, "price": int(float(price)), "rotates": rot == "0", "trail": t, "colors": cols})
    assert len(balls) == 26, len(balls)
    return balls


def landscapes_from_source(web):
    src = open(os.path.join(web, "js", "game.js"), encoding="utf8").read()
    start = src.index("LANDSCAPES = [")
    body = src[start:src.index("LANDSCAPES_ORIGINAL", start)]
    out = []
    for m in re.finditer(r'\{ name: "([^"]+)", price: (\d+), surfaceColor: "#(\w+)", lineColor: "#(\w+)", skyColor: "#(\w+)", groundColor: "#(\w+)" \}', body):
        out.append(m.groups())
    assert len(out) == 7, out
    return out


def texts(web):
    """The English texts of langs.xml, by id."""
    s = open(os.path.join(web, "assets", "dat", "langs.xml"), encoding="utf8").read()
    out = {}
    for m in re.finditer(r'<string id="([^"]*)">(.*?)</string>', s, re.S):
        en = re.search(r"<en>(.*?)</en>", m.group(2), re.S)
        if en:
            out[m.group(1)] = en.group(1)
    return out


def shop_tables(art, web, balls):
    """The shop: the balls (name as the game shows it, price, whether it turns, its trail) and the landscapes."""
    tx = texts(web)

    def shown(name):   # the game's STR(): the text for the name, its first letter capitalised
        t = tx.get(name.upper())
        return (t[0].upper() + t[1:]) if t else name.replace("_", " ")
    rows = []
    for i, b in enumerate(balls):
        t = b["trail"]
        d = int(t.get("height", 56))
        # the radial gradient's centre: (d / 2 + offsetX) || 0, so 0 (the tip) when offsetX is unset
        cx = int(d / 2 + t["gradient_offsetX"]) if "gradient_offsetX" in t else 0
        cols = [int(c, 16) for c in b["colors"]]
        art.o.arr("uint32_t", f"art_trail{i}", cols)
        rows.append('{"%s",%d,%d,%d,%d,%d,%d,%d,art_trail%d}' % (shown(b["name"]), b["price"], int(b["rotates"]), int(t.get("gradient_type", 0)),
                                                       d, len(cols), cx, round(t.get("offsetY", 0) * 100), i))
    art.o.line("const dn_ball_info art_ball_info_[26] = {\n" + ",\n".join(rows) + "};")
    lands = landscapes_from_source(web)
    rows = []
    for name, price, surf, line, sky, ground in lands:
        nm = "Night Land" if name == "Furst" else shown(name)   # (the first one's name has no text; its card is Night Land)
        rows.append('{"%s",%s,0x%s,0x%s,0x%s,0x%s}' % (nm, price, surf, line, sky, ground))
    art.o.line("const dn_land_info art_land_info_[7] = {\n" + ",\n".join(rows) + "};")
    art.decl += ["extern const dn_ball_info art_ball_info_[26];", "extern const dn_land_info art_land_info_[7];"]


def font(art, web, name, size, chars, src="font"):
    """Bango Pro from the game's BMFont, at `size` pixels: 4-bit coverage per glyph."""
    fnt = open(os.path.join(web, "assets", "fnt", src + ".fnt"), encoding="utf8").read()
    page = Image.open(os.path.join(web, "assets", "fnt", re.search(r'file="([^"]+)"', fnt).group(1))).convert("RGBA")
    info = int(re.search(r'<info[^>]*size="(\d+)"', fnt).group(1))
    base = int(re.search(r'base="(\d+)"', fnt).group(1))
    glyphs = {}
    for m in re.finditer(r'<char id="(\d+)" x="(\d+)" y="(\d+)" width="(\d+)" height="(\d+)" xoffset="(-?\d+)" yoffset="(-?\d+)" xadvance="(\d+)"', fnt):
        g = [int(v) for v in m.groups()]
        glyphs[g[0]] = g[1:]
    k = size / info

    def synth(ch):   # the font has no slash or apostrophe: drawn in its weight
        im = Image.new("RGBA", (40, 64), (0, 0, 0, 0))
        d = ImageDraw.Draw(im)
        if ch == "/":
            d.line([(30, 10), (8, 52)], fill=(255, 255, 255, 255), width=9)
            return im.crop((2, 6, 37, 57)), 2 - 0, 6, 34
        d.rounded_rectangle([14, 8, 22, 24], radius=4, fill=(255, 255, 255, 255))
        return im.crop((13, 7, 24, 26)), 1, 7, 14
    gl, data = [], []
    asc = 0
    for ch in chars:
        c = ord(ch)
        if c in glyphs:
            x, y, w, h, xo, yo, adv = glyphs[c]
            im = page.crop((x, y, x + w, y + h))
        else:
            im, xo, yo, adv = synth(ch)
            w, h = im.size
        # placed on the target pixel grid: the glyph's box scaled, its edges rounded outwards
        x0, y0 = xo * k, (yo - base) * k
        X0, Y0 = math.floor(x0), math.floor(y0)
        W = max(1, math.ceil(x0 + w * k) - X0) if w > 1 else 0
        H = max(1, math.ceil(y0 + h * k) - Y0) if h > 1 else 0
        cov = []
        if W and H:
            big = Image.new("L", (W * 8, H * 8), 0)
            sub = im.split()[3].resize((max(1, round(w * k * 8)), max(1, round(h * k * 8))), Image.LANCZOS)
            big.paste(sub, (round((x0 - X0) * 8), round((y0 - Y0) * 8)))
            small = big.resize((W, H), Image.BOX)
            cov = [min(15, (v + 8) // 17) for v in pixels(small)]
        asc = max(asc, -Y0)
        gl.append((W, H, X0, Y0, round(adv * k), len(data)))
        data += nib(cov) if cov else []
    b0 = art.o.bytes
    art.o.arr("uint8_t", name + "_px", data)
    art.o.line(f"static const dn_glyph {name}_g[{len(gl)}] = {{" + ",".join("{%d,%d,%d,%d,%d,%d}" % g for g in gl) + "};")
    art.o.line(f'const dn_font {name} = {{{json.dumps(chars)}, {len(chars)}, {round(base * k)}, {name}_g, {name}_px}};')
    art.decl.append(f"extern const dn_font {name};")
    art.count("font", art.o.bytes - b0)


def landscapes(art, web):
    bg = lambda n, f: os.path.join(web, "assets", "img_480", "BGs", f"BG{n + 1}", f)
    H = 665

    def sky(n):
        im = Image.open(bg(n, "BG.png")).convert("RGBA")
        w, h = im.size
        if w == 1:   # a vertical gradient, stretched across: one colour a row
            col = im.resize((1, round(h * S)), Image.LANCZOS)
            art.o.arr("uint16_t", f"art_sky{n}_rows", [rgb565(p) for p in pixels(col.convert("RGB"))])
            art.count("sky", 2 * col.height)
            art.o.line(f"static const dn_sky art_sky{n} = {{0, {col.height}, art_sky{n}_rows, 0}};")
            return
        f = max(900 / w, H / h)
        full = scaled(im, round(w * f * S), round(h * f * S)).crop((0, 0, 320, 240))
        if n in (0, 4):   # soft skies: half the pixels, drawn doubled
            art.rle(f"art_sky{n}_pic", scaled(full, 160, 120), "sky")
            art.o.line(f"static const dn_sky art_sky{n} = {{2, 0, 0, &art_sky{n}_pic}};")
        else:
            art.rle(f"art_sky{n}_pic", full, "sky")
            art.o.line(f"static const dn_sky art_sky{n} = {{1, 0, 0, &art_sky{n}_pic}};")

    def layer(name, n, f, scale=1.0, rows=None):
        im = Image.open(bg(n, f)).convert("RGBA")
        w, h = im.size
        art.rle(name, scaled(im, round(w * S * scale), round(h * S * scale)), "layers", rows)

    for n in range(7):
        sky(n)
    # the layers, cropped to the rows that can show (their tops are set by the formulas in draw.c)
    layer("art_dunes", 0, "Dunes.png", rows=240 - 136 + 2)
    layer("art_moon", 0, "Moon.png")
    layer("art_land1", 1, "LandBG_1.png", rows=240 - 87)
    layer("art_land2", 1, "LandBG_2.png", 1.8, rows=240 - 112)
    layer("art_sun", 1, "Sun.png")
    layer("art_hills_far", 2, "Hills_Far.png", rows=240 - 67)
    layer("art_vulcan", 2, "Vulcan.png", 0.9)
    layer("art_vulcan_fire", 2, "Vulcan_Fire.png", 1.1)
    layer("art_hills_close", 2, "Hills_Close.png", rows=240 - 158)
    layer("art_mush1", 3, "Mushroom_1.png", 1.1)
    layer("art_mush_far", 3, "Mushrooms_Far.png", rows=240 - 59)
    layer("art_mush_purple", 3, "Mushroom_Purple.png", 1.1)
    layer("art_mush_close", 3, "Mushrooms_Close.png", rows=240 - 139)
    layer("art_stars", 4, "Stars.png")
    for i in range(5):
        layer(f"art_planet{i}", 4, f"Planet_{i + 1}.png", 1.1)
    layer("art_snow_far", 5, "Snow_Layer_Far.png", rows=240 - 79)
    layer("art_snow_close", 5, "Snow_Layer_Close.png", rows=240 - 114)
    layer("art_flakes2", 5, "Falling_Snow_2.png")
    layer("art_flakes1", 5, "Falling_Snow_1.png", 2.0)
    layer("art_candy_far", 6, "Land_Far.png", rows=240 - 79)
    layer("art_candy_close", 6, "Land_Close.png", rows=240 - 82)
    layer("art_candy1", 6, "Candy_1.png", 1.1)
    layer("art_candy2", 6, "Candy_2.png", 1.1)
    layer("art_candy3", 6, "Candy_3.png", 1.2)
    layer("art_cloud", 6, "Cloud.png", 1.4)
    # the ground: a quarter of the texture's size (the world is drawn at 0.142 to 0.356 of its points)
    for n in range(7):
        if n == 5:
            continue
        im = Image.open(bg(n, f"Earth-texture-{n + 1}.png")).convert("RGBA")
        art.rle(f"art_earth{n}", scaled(im, 320, 68), "ground")
    art.o.line("const dn_sky *const art_skies[7] = {" + ",".join(f"&art_sky{n}" for n in range(7)) + "};")
    art.o.line("const dn_rle *const art_earths[7] = {" + ",".join("0" if n == 5 else f"&art_earth{n}" for n in range(7)) + "};")
    art.decl.append("extern const dn_sky *const art_skies[7];")
    art.decl.append("extern const dn_rle *const art_earths[7];")


def main():
    if len(sys.argv) < 2:
        raise SystemExit("usage: data.py WEB [--preview DIR]")
    web = sys.argv[1]
    preview = sys.argv[sys.argv.index("--preview") + 1] if "--preview" in sys.argv else None
    art = Art(web, preview)
    p1, p2 = atlas(web, "pak1"), atlas(web, "pak2")
    balls = balls_from_source(web)

    # balls: 20 pixels for the ride (56 points), 28 for the shop (80 points), one palette for both
    names = []
    for i, b in enumerate(balls):
        src = p1["B_" + b["name"]]
        small, shop = scaled(src, 20, 20), scaled(src, 28, 28)
        pal, (a, c) = quantize([small, shop])
        art.raw(f"art_ball{i}", small, "balls", pal, a)
        art.raw(f"art_shopball{i}", shop, "balls", pal, c)
        names.append(b["name"])
    art.o.line("const dn_raw *const art_balls[26] = {" + ",".join(f"&art_ball{i}" for i in range(26)) + "};")
    art.o.line("const dn_raw *const art_shopballs[26] = {" + ",".join(f"&art_shopball{i}" for i in range(26)) + "};")
    art.decl += ["extern const dn_raw *const art_balls[26];", "extern const dn_raw *const art_shopballs[26];"]

    # the interface
    art.raw("art_coin", scaled(p1["Score_Coin"], 15, 15), "ui")
    art.raw("art_coin_big", scaled(p1["Score_Coin"], 22, 22), "ui")
    art.mask("art_crown", scaled(p1["crown"], 18, 13), "ui")
    art.raw("art_play", scaled(p1["PLAY"], 60, 60), "ui")
    art.raw("art_shop", scaled(p1["Shop"], 60, 60), "ui")
    art.mask("art_cat_balls", scaled(p1["Balls"], 40, 40), "ui")
    art.mask("art_cat_lands", scaled(p1["Landscapes"], 40, 40), "ui")
    art.mask("art_lock", scaled(p1["Kladka-Landscape"], 17, 22), "ui")
    logo = p1["GameLogo"]
    box = logo.split()[3].getbbox()
    # (its anchor is the frame's centre: where the letters' centre is from it, at the menu's scale 1.2)
    art.o.line("const int8_t art_logo_dx = %d, art_logo_dy = %d;" % (round(((box[0] + box[2]) / 2 - logo.width / 2) * 1.2 * S),
                                                              round(((box[1] + box[3]) / 2 - logo.height / 2) * 1.2 * S)))
    art.decl.append("extern const int8_t art_logo_dx, art_logo_dy;")
    logo = logo.crop(box)
    art.mask("art_logo", scaled(logo, round(logo.width * 1.2 * S), round(logo.height * 1.2 * S)), "ui")
    guide = p1["Guide"]
    art.mask("art_guide", scaled(guide, round(guide.width * S), round(guide.height * S)), "ui")
    for n in range(7):
        art.rle(f"art_card{n}", scaled(p1[str(n + 1)], 37, 137), "shop")
        art.rle(f"art_preview{n}", scaled(p2[f"BG {n + 1}"], 141, 79), "shop")
    art.o.line("const dn_rle *const art_cards[7] = {" + ",".join(f"&art_card{n}" for n in range(7)) + "};")
    art.o.line("const dn_rle *const art_previews[7] = {" + ",".join(f"&art_preview{n}" for n in range(7)) + "};")
    art.decl += ["extern const dn_rle *const art_cards[7];", "extern const dn_rle *const art_previews[7];"]

    landscapes(art, web)
    shop_tables(art, web, balls)

    ascii_ = "".join(chr(c) for c in range(32, 127) if chr(c) not in '"#$%&()*;<>@[\\]^_`{|}~')
    font(art, web, "art_f13", 13, ascii_)
    font(art, web, "art_f21", 21, ascii_)
    font(art, web, "art_f32", 32, " !CIYacdehinorstuy")
    font(art, web, "art_f39", 39, "+0123")
    font(art, web, "art_d18", 17.8, "0123456789", "font2")
    font(art, web, "art_d50", 49.8, "0123456789", "font2")

    with open(os.path.join(OUT, "art.c"), "w", encoding="utf8") as f:
        f.write("/* Dune's pictures and font, from the web build (tools/data.py: do not edit). */\n#include \"art.h\"\n")
        f.write("\n".join(art.o.c) + "\n")
    with open(os.path.join(OUT, "art.h"), "w", encoding="utf8") as f:
        f.write("""/* Dune's pictures and font (art.c, made by tools/data.py from the web build). */
#ifndef DN_ART_H
#define DN_ART_H
#include <stdint.h>

/* 4-bit pictures with 16 colours (index 0 transparent): stored as they are, or as rows of runs */
typedef struct { uint16_t w, h; const uint16_t *pal; const uint8_t *px; } dn_raw;
typedef struct { uint16_t w, h; const uint16_t *pal; const uint16_t *rows; const uint8_t *data; } dn_rle;
/* white pictures: 4-bit coverage */
typedef struct { uint16_t w, h; const uint8_t *px; } dn_mask;
/* a sky: a picture (scale 1 or 2) or, when pic is 0, a colour for each row */
typedef struct { uint8_t scale; uint8_t n; const uint16_t *rows; const dn_rle *pic; } dn_sky;
/* Bango Pro: each glyph's box, its offset from the pen (y from the baseline), advance and coverage */
typedef struct { uint8_t w, h; int8_t x, y; uint8_t adv; uint16_t off; } dn_glyph;
typedef struct { const char *chars; uint8_t count, base; const dn_glyph *g; const uint8_t *px; } dn_font;
/* the shop, as the game's BALLS and LANDSCAPES: trail gradient type (0 down, 1 along, 2 checks, 3 round from
   (cx, d/2), 4 dots), its size d, colours (0xRRGGBB) and the anchor's offset (offsetY x 100) */
typedef struct { const char *name; uint16_t price; uint8_t rotates, type, d, n; int8_t cx, oy; const uint32_t *colors; } dn_ball_info;
typedef struct { const char *name; uint16_t price; uint32_t surface, line, sky, ground; } dn_land_info;

""")
        f.write("\n".join(art.decl) + "\n#endif\n")
    total = sum(art.sizes.values())
    print("art:", ", ".join(f"{k} {v // 1024} KB" for k, v in art.sizes.items()), f"= {total // 1024} KB")


if __name__ == "__main__":
    main()
