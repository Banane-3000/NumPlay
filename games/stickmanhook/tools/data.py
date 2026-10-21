#!/usr/bin/env python3
"""src/levels.c, src/art.c and src/art.h again, from Stickman Hook's web build.

    python3 -I tools/data.py WEB

WEB holds the web build as published (bundle.js, images/, fonts/JUNEGULL.ttf), for instance the copy in
github.com/Gr4ys0n/Gr4ys0n.github.io at 29006b0f, public/assets/games/stickman-hook (bundle.js: sha256
1022ae002395b485b3d213a0ed186bf13014f2662288f8f12d5c4a8342843a5e). The bundle is only read, as text: its data
modules (the levels, the walls' outlines, the skins, the atlases) are JavaScript literals, parsed here;
nothing of it runs. Needs Python 3 and Pillow.
"""
import base64
import io
import json
import math
import os
import re
import sys

from PIL import Image, ImageDraw, ImageFont

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
PX = 240           # screen pixels in one world unit (the game scales its scene by the screen's height)
SCALE = 5e-4       # the game's Sc.SCALE: a texture pixel at scale 1


# ---------------------------------------------------------------- the bundle, as data

def modules(text):
    """webpack's module list: each starts a line with `}, function(` (the first with `}([function(`)."""
    lines = text.split("\n")
    starts = [i for i, l in enumerate(lines) if re.match(r"^(\}, |\}\(\[)function\(", l)]
    return ["\n".join(lines[s:e]) for s, e in zip(starts, starts[1:] + [len(lines)])]


class Lit:
    """A JavaScript literal as webpack writes JSON files: objects, arrays, strings, numbers, !0/!1."""
    def __init__(self, s, i=0):
        self.s, self.i = s, i

    def ws(self):
        while self.s[self.i] in " \t\r\n":
            self.i += 1

    def value(self):
        self.ws()
        c = self.s[self.i]
        if c in "{[":
            self.i += 1
            out = {} if c == "{" else []
            while True:
                self.ws()
                if self.s[self.i] in "}]":
                    self.i += 1
                    return out
                if c == "{":
                    m = re.compile(r'"([^"]*)"|([\w$]+)').match(self.s, self.i)
                    self.i = m.end()
                    self.ws()
                    assert self.s[self.i] == ":"
                    self.i += 1
                    out[m.group(1) if m.group(1) is not None else m.group(2)] = self.value()
                else:
                    out.append(self.value())
                self.ws()
                if self.s[self.i] == ",":
                    self.i += 1
        if c == '"':
            m = re.compile(r'"((?:[^"\\]|\\.)*)"').match(self.s, self.i)
            self.i = m.end()
            return m.group(1).encode().decode("unicode_escape")
        m = re.compile(r"-?(\d+\.?\d*|\.\d+)([eE][-+]?\d+)?").match(self.s, self.i)
        if m:
            self.i = m.end()
            t = m.group(0)
            return float(t) if any(ch in t for ch in ".eE") else int(t)
        for word, v in (("!0", True), ("!1", False), ("void 0", None)):
            if self.s.startswith(word, self.i):
                self.i += len(word)
                return v
        m = re.compile(r"[\w$]+(\.[\w$]+)*").match(self.s, self.i)   # another module (the skins' miniatures)
        self.i = m.end()
        return {"$ref": m.group(0)}


class Bundle:
    def __init__(self, web):
        self.web = web
        self.mods = modules(open(os.path.join(web, "bundle.js"), encoding="utf-8").read())
        self.main = self.mods[284]
        self.calls = dict(re.findall(r"([\w$]+) = n\((\d+)\)", self.main))
        self.aliases = dict(re.findall(r"([\w$]+) = n\.n\(([\w$]+)\)", self.main))

    def exports(self, i):
        m = self.mods[i]
        return Lit(m, m.index("t.exports = ") + len("t.exports = ")).value()

    def context(self, i):
        """A require.context: file name -> module."""
        m = self.mods[i]
        return Lit(m, m.index("{", m.index("var r = {"))).value()

    def var_module(self, var):
        var = var.split(".")[0]
        return int(self.calls[self.aliases.get(var, var)])

    def image(self, i):
        m = self.mods[i]
        uri = re.search(r't\.exports = "data:image/\w+;base64,([^"]+)"', m)
        if uri:
            return Image.open(io.BytesIO(base64.b64decode(uri.group(1)))).convert("RGBA")
        path = re.search(r't\.exports = n\.p \+ "([^"]+)"', m).group(1)
        return Image.open(os.path.join(self.web, path)).convert("RGBA")

    def resources(self):
        """The loader's table (`Qh`): name -> module."""
        block = self.main[self.main.index("Qh = {") + 5:]
        block = block[:block.index("}") + 1]
        return {k: self.var_module(v) for k, v in re.findall(r"(\w+): ([\w$]+)(?:\.a)?", block)}

    def skins(self):
        k = self.main.index("var As = [") + len("var As = ")
        return Lit(self.main, k).value()


# ---------------------------------------------------------------- C output

def cf(x):
    s = format(float(x), ".9g")
    if "." not in s and "e" not in s and "inf" not in s:
        s += ".0"
    return s + "f"


def row(v):
    return "{" + ",".join(cf(x) if isinstance(x, float) else str(x) for x in v) + "}"


def table(ctype, name, rows, per=1):
    body = ",\n".join(",".join(rows[i:i + per]) for i in range(0, len(rows), per))
    return f"const {ctype} {name}[{len(rows)}] = {{\n{body}\n}};"


def rgb565(c):
    return (c[0] >> 3) << 11 | (c[1] >> 2) << 5 | c[2] >> 3


# ---------------------------------------------------------------- the levels

def levels(b):
    sets = [("intro", 165), ("start", 175), ("middle", 207), ("end", 251)]
    parts, first = [], {}
    for name, cid in sets:
        files = b.context(cid)
        first[name] = len(parts)
        for f in sorted(files):
            d = b.exports(files[f])
            d["file"] = f[2:-5]
            parts.append(d)
    count = {name: len(b.context(cid)) for name, cid in sets}
    assert count == {"intro": 9, "start": 31, "middle": 43, "end": 31}, count
    recipes = b.exports(20)
    assert len(recipes) == 30
    index = {p["file"]: i for i, p in enumerate(parts)}
    shapes = b.exports(75)
    names = list(shapes)
    rotations, profiles = [], []
    hooks, bumpers, walls, part_rows = [], [], [], []
    maxb = 0
    for p in parts:
        part_rows.append(row([float(p["spawnPoint"]["x"]), float(p["spawnPoint"]["y"]), float(p["finishLine"]),
                              len(hooks), len(bumpers), len(walls), len(p["hooks"]), len(p["bumpers"]),
                              len(p.get("obstacles", [])), int(p["background"][-1])]))
        hooks += [row([float(h["x"]), float(h["y"])]) for h in p["hooks"]]
        for v in p["bumpers"]:
            assert (v["w"], v["h"]) == (0.15, 0.04)
            bo = v.get("bounce") or {}
            prof = (float(v["rotation"]), float(bo.get("bounce") or 1.2), float(bo.get("force") or 0),
                    float(bo.get("angle", 0)), int("angle" in bo))
            if prof not in profiles:
                profiles.append(prof)
            bumpers.append(row([float(v["x"]), float(v["y"]), profiles.index(prof)]))
        for v in p.get("obstacles", []):
            r = float(v.get("rotation") or 0)
            if r not in rotations:
                rotations.append(r)
            walls.append(row([float(v["x"]), float(v["y"]), rotations.index(r), names.index(v["name"])]))
    # the biggest course: the most bumpers of a start, a middle and an end (or of an introductory level)
    def most(name, key):
        lo = first[name]
        return max(len(p.get(key, [])) for p in parts[lo:lo + count[name]])
    maxb = max(most("intro", "bumpers"), most("start", "bumpers") + most("middle", "bumpers") + most("end", "bumpers"))
    maxh = max(most("intro", "hooks"), most("start", "hooks") + most("middle", "hooks") + most("end", "hooks"))
    edges, shape_rows = [], []
    for name in names:
        lo, radius = len(edges), 0
        for poly in shapes[name]:
            pts = [((poly["shape"][i] - 256) * SCALE, (poly["shape"][i + 1] - 256) * SCALE)
                   for i in range(0, len(poly["shape"]), 2)]
            for i, (x, y) in enumerate(pts):
                x2, y2 = pts[(i + 1) % len(pts)]
                dx, dy = x2 - x, y2 - y
                l = math.hypot(dx, dy)
                if l:
                    edges.append(row([x, y, dx, dy, -dy / l, dx / l, 1 / (l * l)]))
                radius = max(radius, math.hypot(x, y))
        shape_rows.append(row([lo, len(edges) - lo, radius]))
    recipe_rows = [row([index[r["start"]], index[r["mid"]], index[r["end"]]]) for r in recipes]
    skins = b.skins()
    assert len(skins) == 23
    unlock = [s.get("condition", {}).get("level", 0) for s in skins]
    out = ["/* Stickman Hook's courses and skins, from its web build (tools/data.py: do not edit). */",
           '#include "game.h"',
           f"/* sections: {count['intro']} introductory levels, then {count['start']} starts, {count['middle']} middles "
           f"and {count['end']} ends */",
           table("sh_part", "sh_parts", part_rows),
           table("sh_vec", "sh_hooks", hooks, 4),
           table("sh_bumper", "sh_bumpers", bumpers, 4),
           table("sh_bumper_kind", "sh_bumper_kinds", [row([r, math.cos(r), math.sin(r), bo, f, a, ang])
                                                        for r, bo, f, a, ang in profiles]),
           table("sh_wall", "sh_walls", walls, 4),
           table("sh_vec", "sh_turns", [row([math.cos(r), math.sin(r)]) for r in rotations], 4),
           table("float", "sh_turn_angles", [cf(r) for r in rotations], 8),
           table("sh_edge", "sh_edges", edges, 2),
           table("sh_shape", "sh_shapes", shape_rows),
           table("uint8_t", "sh_recipes", recipe_rows, 6).replace("uint8_t sh_recipes[30]", "uint8_t sh_recipes[30][3]"),
           table("uint8_t", "sh_unlock_level", [str(u) for u in unlock], 23)]
    meta = (f"#define SH_PARTS {len(parts)}\n#define SH_INTRO {count['intro']}\n#define SH_STARTS {count['start']}\n"
            f"#define SH_MIDDLES {count['middle']}\n#define SH_ENDS {count['end']}\n"
            f"#define SH_FIRST_START {first['start']}\n#define SH_FIRST_MIDDLE {first['middle']}\n"
            f"#define SH_FIRST_END {first['end']}\n#define SH_RECIPES {len(recipes)}\n"
            f"#define SH_MAX_BUMPERS {maxb}\n#define SH_MAX_HOOKS {maxh}\n#define SH_WALL_KINDS {len(names)}\n")
    # for the physics check against the original (tests/oracle.cjs): the walls' outlines, as in the game
    json.dump({n: [p["shape"] for p in shapes[n]] for n in names}, open(os.path.join(ROOT, "tests", "walls.json"), "w"),
              separators=(",", ":"))
    return "\n".join(out) + "\n", meta, names, skins


# ---------------------------------------------------------------- pictures

def lz_pack(data):
    """games/common/np_lz.h's packer, byte for byte: a flag byte for 8 items, a literal or a match (12 bits back,
    3 to 18 long)."""
    table, out, i, n, items, flag_at = {}, bytearray(), 0, len(data), 8, 0
    while i < n:
        if items == 8:
            flag_at = len(out)
            out.append(0)
            items = 0
        length = dist = 0
        if i + 3 <= n:
            h = (data[i] << 8 ^ data[i + 1] << 4 ^ data[i + 2]) & 4095
            cand = table.get(h)
            table[h] = i
            if cand is not None and cand < i and i - cand <= 4096:
                top = min(18, n - i)
                while length < top and data[cand + length] == data[i + length]:
                    length += 1
                dist = i - cand
        if length >= 3:
            out += bytes([(dist - 1) & 255, (dist - 1) >> 8 | (length - 3) << 4])
            for k in range(1, length):
                j = i + k
                if j + 3 <= n:
                    table[(data[j] << 8 ^ data[j + 1] << 4 ^ data[j + 2]) & 4095] = j
            i += length
        else:
            out[flag_at] |= 1 << items
            out.append(data[i])
            i += 1
        items += 1
    return bytes(out)


def rgb_of(im):
    b = im.convert("RGB").tobytes()
    return list(zip(b[0::3], b[1::3], b[2::3]))


def dist(a, b):
    """How far two colours look apart (red, green and blue weighted as the eye does, roughly)."""
    return 2 * (a[0] - b[0]) ** 2 + 4 * (a[1] - b[1]) ** 2 + 3 * (a[2] - b[2]) ** 2


def quantize(counts, n=15):
    """At most n colours for these pixels: the most common first, then each time the colour the chosen ones
    stand for worst (so a small red dot keeps its red), then a few rounds of k-means."""
    cols = sorted(counts, key=lambda c: -counts[c])
    if len(cols) <= n:
        return cols
    centres = [cols[0]]
    best = {c: dist(c, centres[0]) for c in cols}
    while len(centres) < n:
        c = max(cols, key=lambda c: best[c] * counts[c] ** 0.5)
        centres.append(c)
        for k in cols:
            best[k] = min(best[k], dist(k, c))
    for _ in range(8):
        sums = [[0, 0, 0, 0] for _ in centres]
        for c in cols:
            k = min(range(len(centres)), key=lambda j: dist(c, centres[j]))
            w, t = counts[c], sums[k]
            t[0] += c[0] * w
            t[1] += c[1] * w
            t[2] += c[2] * w
            t[3] += w
        centres = [(round(t[0] / t[3]), round(t[1] / t[3]), round(t[2] / t[3])) if t[3] else centres[k]
                   for k, t in enumerate(sums)]
    return centres


class Sprites:
    def __init__(self):
        self.items = []   # (name, image, x, y, alpha)

    def add(self, name, im, scale, anchor=(0.5, 0.5), alpha=False, fit=None, group=None, packed=False):
        """A picture at `scale` screen pixels per texture pixel, cropped, its anchor at (0, 0); the pictures of a
        group share their 15 colours."""
        if fit:
            box = im.getbbox()
            im = im.crop(box)
            scale = min(fit[0] / im.width, fit[1] / im.height)
            anchor = (0.5, 0.5)
        size = (max(1, round(im.width * scale)), max(1, round(im.height * scale)))
        big = im.resize(size, Image.Resampling.LANCZOS)
        if not alpha:   # the screen has no transparency: in or out
            big.putalpha(big.getchannel("A").point(lambda a: 255 if a >= 100 else 0))
        box = big.getbbox() or (0, 0, 1, 1)
        x, y = box[0] - round(size[0] * anchor[0]), box[1] - round(size[1] * anchor[1])
        self.items.append((name, big.crop(box), x, y, alpha, group or name, packed))

    def pack(self):
        pixels, palettes, rows, defs = bytearray(), [], [], []
        seen_pal, seen_px = {}, {}
        groups = {}
        for name, im, x, y, alpha, group, packed in self.items:
            if not alpha:
                rgb, a = rgb_of(im), im.getchannel("A").tobytes()
                counts = groups.setdefault(group, {})
                for c, al in zip(rgb, a):
                    if al:
                        counts[c] = counts.get(c, 0) + 1
        colours = {g: quantize(c) for g, c in groups.items()}
        for i, (name, im, x, y, alpha, group, packed) in enumerate(self.items):
            assert 0 < im.width < 256 and 0 < im.height < 256 and -128 <= x <= 127 and -128 <= y <= 127, name
            a = im.getchannel("A").tobytes()
            if alpha:   # a glow (the finish light, the spawn's rings): 15 levels of its alpha, in its colour
                rgb = [c for c, al in zip(rgb_of(im), a) if al > 128] or [(255, 255, 255)]
                colour = tuple(sum(c[k] for c in rgb) // len(rgb) for k in range(3))
                pal = [0] + [rgb565(colour)] * 15
                vals = [(v * 15 + 127) // 255 for v in a]
            else:
                cols = colours[group]
                pal = [0] + [rgb565(c) for c in cols]
                memo = {}
                def nearest(c):
                    if c not in memo:
                        memo[c] = 1 + min(range(len(cols)), key=lambda k: dist(c, cols[k]))
                    return memo[c]
                vals = [nearest(c) if al else 0 for c, al in zip(rgb_of(im), a)]
            key = tuple(pal)
            if key not in seen_pal:
                seen_pal[key] = len(palettes)
                palettes += pal
            raw = bytes(vals[j] << 4 | (vals[j + 1] if j + 1 < len(vals) else 0) for j in range(0, len(vals), 2))
            blob, flag = raw, 0
            if packed:   # (unpacked by draw.c when it's on the screen: the menus' big pictures)
                z = lz_pack(raw)
                blob, flag = bytes([len(z) & 255, len(z) >> 8]) + z, 1 << 31
            if blob not in seen_px:
                seen_px[blob] = len(pixels)
                pixels += blob
            rows.append(row([seen_px[blob] | flag, seen_pal[key], im.width, im.height, x, y]))
            defs.append(f"#define SPR_{name.upper()} {i}")
        return pixels, palettes, rows, defs


def frame(tex, atlas, name):
    """A frame of a TexturePacker atlas, untrimmed back to its source size."""
    d = atlas["frames"][name]
    f, s = d["frame"], d["spriteSourceSize"]
    w, h = (f["h"], f["w"]) if d.get("rotated") else (f["w"], f["h"])
    im = tex.crop((f["x"], f["y"], f["x"] + w, f["y"] + h))
    if d.get("rotated"):
        im = im.transpose(Image.Transpose.ROTATE_90)
    canvas = Image.new("RGBA", (d["sourceSize"]["w"], d["sourceSize"]["h"]))
    canvas.paste(im, (s["x"], s["y"]))
    return canvas


POSES = ["spawn", "hooked0", "hooked1", "hooked2", "ball", "victory"]


def art(b, wall_names, skins):
    res = b.resources()
    tex = lambda k: b.image(res[k])
    data = lambda k: b.exports(res[k])
    sp = Sprites()
    k1 = SCALE * PX                          # objects at scale SCALE: 0.12 px per texture pixel
    sp.add("title", tex("title"), 1.75 * k1)
    for k in ("hook", "hook_near", "hook_active"):
        sp.add(k, tex(k), k1)
    for n in wall_names:
        sp.add("wall_" + n, tex(n), k1)
    bt, bj = tex("bumper_texture"), data("bumper_json")
    for i, n in enumerate(bj["animations"]["AN_Grap1_BumperLarge"]):
        sp.add(f"bumper{i}", frame(bt, bj, n), k1, (0.49, 0.635), group="bumper")
    ft, fj = tex("fxspawn_texture"), data("fxspawn_json")
    for i, n in enumerate(fj["animations"]["FX_Spawn"]):
        sp.add(f"spawn{i}", frame(ft, fj, n), k1, group="spawn")
    pt, pj = tex("finishplatform_texture"), data("finishplatform_json")
    for i, n in enumerate(pj["animations"]["AN_Grap1_FinishPlatform"]):
        sp.add(f"platform{i}", frame(pt, pj, n), k1, (0.5, -0.2), group="platform")
    sp.add("finishlight", tex("finishlight"), 1.5 * k1, (0.5, 0.4), alpha=True)
    sp.add("confetti", tex("confetti"), 1)   # (only its outline is used: the confetti are drawn as quads)
    sp.items.pop()
    # the interface: the title's play button, the victory's continue button, the pause button, the lock
    sp.add("btn_play", b.image(148), 0, fit=(110, 50))
    sp.add("btn_next", b.image(149), 0, fit=(46, 46))
    sp.add("btn_pause", b.image(150), 0, fit=(18, 18))
    sp.add("lock", b.image(29), 0, fit=(11, 11))
    first, dance, mini = [], [], []
    for i, s in enumerate(skins):
        st, sj = tex(s["texture"]), data(s["atlas"])
        anim = sj["animations"][s["id"]]
        first.append(len(sp.items))
        for pose in POSES:                   # in the level: scale 0.5 * SCALE
            sp.add(f"skin{i}_{pose}", frame(st, sj, anim[s["frames"][pose]]), 0.5 * k1, group=f"skin{i}")
        for pose in POSES[1:4]:              # on the title: scale SCALE
            sp.add(f"skin{i}_title_{pose}", frame(st, sj, anim[s["frames"][pose]]), k1, group=f"skin{i}", packed=True)
        names = sj["animations"].get("dance") or [anim[s["frames"]["victory"]]]
        dance.append(len(names))
        for j, n in enumerate(names):        # dancing on the victory's platform and in the shop: 1.5 * SCALE
            sp.add(f"skin{i}_dance{j}", frame(st, sj, n), 1.5 * k1, group=f"skin{i}", packed=True)
        sp.add(f"skin{i}_big_victory", frame(st, sj, anim[s["frames"]["victory"]]), 1.5 * k1, group=f"skin{i}",
               packed=True)
        mini.append(len(sp.items))
        sp.add(f"skin{i}_mini", b.image(b.var_module(s["thumbnail"]["$ref"])), 0, fit=(34, 34), packed=True)
    pixels, palettes, rows, defs = sp.pack()
    grads = []
    for k in ("background_0", "background_1"):
        g = tex(k).convert("RGB").resize((1, 240), Image.Resampling.LANCZOS)
        grads.append(row([rgb565(g.getpixel((0, y))) for y in range(240)]))
    # the shop's sunburst: which of 256 directions are a ray
    sun = tex("shop_background")
    cx, cy = sun.width / 2, sun.height / 2
    rays = [1 if sun.getpixel((int(cx + 450 * math.cos(a * math.tau / 256)), int(cy + 450 * math.sin(a * math.tau / 256))))[3] > 128
            else 0 for a in range(256)]
    ray_bits = [sum(rays[i * 8 + j] << j for j in range(8)) for i in range(32)]
    fonts, font_defs = [], []
    for name, size in (("small", 14), ("big", 22)):
        f = ImageFont.truetype(os.path.join(b.web, "fonts", "JUNEGULL.ttf"), size)
        top = f.getbbox("H")[1]
        height = f.getbbox("HQ")[3] - top
        glyphs, offs, widths, advances = bytearray(), [], [], []
        for c in range(32, 96):     # JUNEGULL is all capitals: a-z draw A-Z
            ch = chr(c)
            adv = max(1, round(f.getlength(ch)))
            w = max(adv, f.getbbox(ch)[2]) if ch != " " else adv
            im = Image.new("L", (w + 4, height + 2))
            ImageDraw.Draw(im).text((0, -top), ch, font=f, fill=255)
            offs.append(len(glyphs))
            widths.append(w)
            advances.append(adv)
            for y in range(height):
                bits = 0
                for x in range(w):
                    bits |= ((im.getpixel((x, y)) * 3 + 127) // 255) << (2 * (x % 4))
                    if x % 4 == 3 or x == w - 1:
                        glyphs.append(bits)
                        bits = 0
        fonts.append(f"const sh_font sh_font_{name} = {{{height}, {{{','.join(map(str, widths))}}}, "
                     f"{{{','.join(map(str, advances))}}}, {{{','.join(map(str, offs))}}}, sh_glyphs_{name}}};")
        fonts.insert(len(fonts) - 1, table("uint8_t", f"sh_glyphs_{name}", [str(v) for v in glyphs], 32))
        font_defs.append(f"extern const sh_font sh_font_{name};")
    h = ["/* Stickman Hook's pictures, from its web build (tools/data.py: do not edit). */",
         "#ifndef SH_ART_H", "#define SH_ART_H", "#include <stdint.h>",
         "/* offset: in sh_pixels; with its top bit, packed (np_lz.h) after a 16-bit size */",
         "typedef struct { uint32_t offset; uint16_t palette; uint8_t w, h; int8_t x, y; } sh_sprite;",
         "typedef struct { uint8_t height, width[64], advance[64]; uint16_t offset[64]; const uint8_t *glyphs; } sh_font;",
         *defs, f"#define SH_SPRITES {len(rows)}",
         "extern const sh_sprite sh_sprites[];", "extern const uint8_t sh_pixels[];", "extern const uint16_t sh_palettes[];",
         "extern const uint16_t sh_gradients[2][240];", "extern const uint8_t sh_rays[32];",
         "extern const uint16_t sh_skin_first[23], sh_skin_mini[23];", "extern const uint8_t sh_skin_dances[23];",
         *font_defs, "#endif"]
    c = ["/* Stickman Hook's pictures, from its web build (tools/data.py: do not edit). */", '#include "art.h"',
         table("sh_sprite", "sh_sprites", rows, 4),
         table("uint8_t", "sh_pixels", [str(v) for v in pixels], 40),
         table("uint16_t", "sh_palettes", [str(v) for v in palettes], 16),
         "const uint16_t sh_gradients[2][240] = {" + ",".join(grads) + "};",
         "const uint8_t sh_rays[32] = " + row(ray_bits) + ";",
         "const uint16_t sh_skin_first[23] = " + row(first) + ";",
         "const uint16_t sh_skin_mini[23] = " + row(mini) + ";",
         "const uint8_t sh_skin_dances[23] = " + row(dance) + ";", *fonts]
    return "\n".join(h) + "\n", "\n".join(c) + "\n", len(pixels)


def main():
    if len(sys.argv) != 2:
        sys.exit(__doc__)
    b = Bundle(sys.argv[1])
    lv, meta, wall_names, skins = levels(b)
    open(os.path.join(ROOT, "src", "levels.c"), "w").write(lv)
    open(os.path.join(ROOT, "src", "levels.h"), "w").write(
        "/* How much there is (tools/data.py: do not edit). */\n#ifndef SH_LEVELS_H\n#define SH_LEVELS_H\n" + meta + "#endif\n")
    h, c, npx = art(b, wall_names, skins)
    open(os.path.join(ROOT, "src", "art.h"), "w").write(h)
    open(os.path.join(ROOT, "src", "art.c"), "w").write(c)
    print(f"levels.c, art.c: {npx} bytes of pixels")


if __name__ == "__main__":
    main()
