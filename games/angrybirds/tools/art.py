"""Angry Birds: the pictures of data.bin (tools/pack.py), from the web build's sprite sheets and images.

Every picture keeps 16 colours of its own (RGB565), 4 bits a pixel, the first colour see-through unless the picture
is opaque. Object pictures are made at 8 pixels a meter (the game's art is drawn at 20); the screen shows the world at
about 5, so they are only ever made smaller. A picture's pivot is where the game puts it (x 4, in its pixels).

The picture table: for each one its pixels' offset (u32, the 16 colours first), w, h, pivot x 4, pivot y 4, flags
(1: opaque), then padding: 16 bytes.
"""
import glob
import json
import os
import struct

from PIL import Image

SCALE = 0.4         # object pictures: 8 pixels a meter
CUT_H = 128         # the story pictures' height on the screen


def rgb565(r, g, b):
    return (r >> 3) << 11 | (g >> 2) << 5 | b >> 3


class Pictures:
    def __init__(self, ref):
        self.ref = ref
        self.sheets = {}
        for p in sorted(glob.glob(os.path.join(ref, "sprite_sheets", "*.json"))):
            with open(p) as f:
                d = json.load(f)
            for v in d.values():
                if isinstance(v, dict) and "id" in v:
                    self.sheets.setdefault(v["id"], (p[:-5] + ".png", v))
        self._sheet_images = {}
        self.rows = []          # (offset, w, h, px4, py4, flags)
        self.blob = bytearray()
        self.names = {}         # name -> index, for the header

    # -------------------------------------------------------------- sources
    def sprite(self, sid):
        path, v = self.sheets[sid]
        if path not in self._sheet_images:
            self._sheet_images[path] = Image.open(path).convert("RGBA")
        im = self._sheet_images[path].crop((v["x"], v["y"], v["x"] + v["width"], v["y"] + v["height"]))
        return im, (v.get("pivotx", v["width"] / 2), v.get("pivoty", v["height"] / 2))

    def image(self, rel):
        return Image.open(os.path.join(self.ref, rel)).convert("RGBA")

    # -------------------------------------------------------------- packing
    def add(self, im, pivot=(0, 0), opaque=False, name=None, dither=False):
        """A picture (RGBA), its pivot in its own pixels; its index."""
        im = im.convert("RGBA")
        w, h = im.size
        alpha = im.getchannel("A")
        rgb = Image.new("RGB", im.size, (0, 0, 0))
        rgb.paste(im.convert("RGB"), mask=alpha.point(lambda a: 255 if a >= 128 else 0))
        colors = 16 if opaque else 15
        q = rgb.quantize(colors=colors, method=Image.Quantize.MEDIANCUT,
                         dither=Image.Dither.FLOYDSTEINBERG if dither else Image.Dither.NONE)
        pal = (q.getpalette() or [])[:colors * 3]
        pal += [0] * (colors * 3 - len(pal))
        idx = list(q.getdata())
        if opaque:
            values = idx
            palette = [rgb565(*pal[i * 3:i * 3 + 3]) for i in range(16)]
        else:
            a = list(alpha.getdata())
            values = [p + 1 if av >= 128 else 0 for p, av in zip(idx, a)]
            palette = [0] + [rgb565(*pal[i * 3:i * 3 + 3]) for i in range(15)]
        off = len(self.blob)
        self.blob += struct.pack("<16H", *palette)
        for j in range(0, len(values), 2):
            lo = values[j]
            hi = values[j + 1] if j + 1 < len(values) else 0
            self.blob.append(lo | hi << 4)
        while len(self.blob) % 4:
            self.blob.append(0)
        self.rows.append((off, w, h, int(round(pivot[0] * 4)), int(round(pivot[1] * 4)), 1 if opaque else 0))
        index = len(self.rows) - 1
        if name:
            self.names[name] = index
        return index

    def scaled(self, im, pivot, scale):
        w = max(1, int(round(im.width * scale)))
        h = max(1, int(round(im.height * scale)))
        return im.resize((w, h), Image.Resampling.LANCZOS), (pivot[0] * w / im.width, pivot[1] * h / im.height)

    # -------------------------------------------------------------- objects
    def frames_of(self, name, obj):
        """The sprite names of an object, its undamaged look first (as the game picks them)."""
        mat = obj["material"]
        found = []
        for base in (name, name + "_1", name + "_01", mat, mat + "_1", mat + "_01"):
            if base in self.sheets:
                found = [base]
                break
        if not found:
            return []
        base = found[0]
        if base.endswith("_01"):
            prefix, fmt = base[:-3], "_%02d"
        elif base.endswith("_1"):
            prefix, fmt = base[:-2], "_%d"
        else:
            prefix, fmt = None, None
        if prefix:
            for j in range(2, 10):
                k = prefix + fmt % j
                if k in self.sheets:
                    found.append(k)
        return found

    def object_frames(self, name, obj, shape):
        """Pictures of an object: its damage states (the undamaged first), then a bird's flight and power pictures.
        Returns (first, damage states, flying picture, power picture); 0xFFFF where there is none."""
        ids = self.frames_of(name, obj)
        if name == "BIRD_ME" and not ids and "BIRD_ME_MOTION" in self.sheets:
            ids = ["BIRD_ME_MOTION"]
        extra = []
        if name.startswith("BIRD_"):
            for suffix in ("FLYING", "SPECIAL"):
                k = name + "_" + suffix
                extra.append(k if k in self.sheets and k not in ids else None)
        if not ids:
            return 0xFFFF, 0, 0xFFFF, 0xFFFF
        first = len(self.rows)
        ox, oy = shape.get("offsetX", 0) or 0, shape.get("offsetY", 0) or 0
        made = {}
        for sid in ids + [k for k in extra if k]:
            im, _ = self.sprite(sid)
            # the game turns a picture about its centre moved by the shape's offset, put on the body
            im, piv = self.scaled(im, (im.width / 2 + ox, im.height / 2 + oy), SCALE)
            made[sid] = self.add(im, piv)
        fly = made[extra[0]] if extra and extra[0] else 0xFFFF
        power = made[extra[1]] if len(extra) > 1 and extra[1] else 0xFFFF
        return first, len(ids), fly, power

    # -------------------------------------------------------------- the menus and the story
    def logo(self):
        """The classic Angry Birds wordmark: the largest piece of the title picture (no badge)."""
        im, _ = self.sprite("MAINMENU_AB_GOOGLE_PLUS_LOGO_HD")
        alpha = im.getchannel("A").load()
        w, h = im.size
        seen = [[False] * h for _ in range(w)]
        best = []
        for x in range(w):
            for y in range(h):
                if seen[x][y] or not alpha[x, y]:
                    continue
                stack, comp = [(x, y)], []
                seen[x][y] = True
                while stack:
                    cx, cy = stack.pop()
                    comp.append((cx, cy))
                    for nx, ny in ((cx - 1, cy), (cx + 1, cy), (cx, cy - 1), (cx, cy + 1)):
                        if 0 <= nx < w and 0 <= ny < h and not seen[nx][ny] and alpha[nx, ny]:
                            seen[nx][ny] = True
                            stack.append((nx, ny))
                if len(comp) > len(best):
                    best = comp
        mask = Image.new("L", im.size, 0)
        mp = mask.load()
        for x, y in best:
            mp[x, y] = alpha[x, y]
        im = im.copy()
        im.putalpha(mask)
        return im.crop(mask.getbbox())

    def fit(self, im, w, h):
        s = min(w / im.width, h / im.height)
        return im.resize((max(1, int(round(im.width * s))), max(1, int(round(im.height * s)))),
                         Image.Resampling.LANCZOS)

    def ui(self, episodes, story_names, cutscenes):
        self.add(self.fit(self.logo(), 240, 92), name="LOGO")
        menu = self.image("images/Menu/main_menu/chrome_mainmenu_bg.png").resize((320, 240), Image.Resampling.LANCZOS)
        self.add(menu, opaque=True, name="MENU_BG", dither=True)
        for e in episodes:
            im, _ = self.sprite(e["image"])
            self.add(self.fit(im, 64, 64), name="EP_" + e["image"])
        for sid, w, h in (("MAINMENU_BUTTON_PLAY_NORMAL", 84, 52), ("LEVELPLAY_BUTTON_PAUSE", 22, 22),
                          ("LEVELOVERWIN_BUTTON_RESTART", 34, 34), ("LEVELOVERWIN_BUTTON_MENU", 34, 34),
                          ("LEVELOVERWIN_BUTTON_NEXTLEVEL", 34, 34), ("LEVELSELECTION_LOCK", 18, 22),
                          ("ASSET_FEATHER", 12, 16), ("LEVELEND_FEATHER", 30, 40), ("BIRD_SARDINE", 20, 14)):
            if sid in self.sheets:
                im, _ = self.sprite(sid)
                self.add(self.fit(im, w, h), name=sid)
        for k in range(4):
            sid = "LEVELOVERWIN_STARS_BIG_%d" % k
            if sid in self.sheets:
                im, _ = self.sprite(sid)
                self.add(self.fit(im, 150, 56), name=sid)
        for sid in ("SMOKE_BIG_1", "SMOKE_BIG_2", "SMOKE_BIG_3", "SMOKE_BIG_4", "SMOKE_SMALL_1", "TRAIL_1", "TRAIL_2",
                    "TRAIL_3", "EXPLOSION_1", "EXPLOSION_3", "EXPLOSION_5", "EXPLOSION_7"):
            im, piv = self.sprite(sid)
            im, piv = self.scaled(im, piv, SCALE)
            self.add(im, piv, name=sid)
        # the slingshot: its back and front halves, at the objects' scale
        for part in ("SLINGSHOT_01_BACK", "SLINGSHOT_01_FRONT"):
            im = self.image("images/Slingshot/%s.png" % part)
            im, piv = self.scaled(im, (0, 0), SCALE)
            self.add(im, piv, name=part)
        # the story: each picture as tall as CUT_H on the screen (the tall one CUT_H x 320 / 180 wide), panned
        for n in story_names:
            im = self.image("images/cutscenes/" + cutscenes[n]["image"])
            if im.width >= im.height:
                s = CUT_H / im.height
            else:
                s = 320 / im.width
            im = im.resize((max(1, int(round(im.width * s))), max(1, int(round(im.height * s)))),
                           Image.Resampling.LANCZOS)
            self.add(im, opaque=True, name=n, dither=True)

    # -------------------------------------------------------------- backgrounds
    @staticmethod
    def parse_themes(js):
        """The game's background themes (its script's ZDb and SDb calls, the HD ones): for each, its layers (picture,
        parallax, x and y offsets in its pixels, scale, behind or in front of the world), the sky's colour and the
        ground's (ARGB)."""
        import re
        strs = dict(re.findall(r"^ ([A-Za-z0-9_$]+) = '([^']*)'", js, re.M))

        def val(tok):
            tok = tok.strip()
            if tok.startswith("'"):
                return tok.strip("'")
            return strs.get(tok, tok)

        def layers_of(text, var):
            out = []
            for c in re.finditer(r"SDb\(" + var + r", (\d+), ([^,]+), ([^,]+), ([^,]+), ([^,]+), ([^,]+), (true|false)\)",
                                 text):
                i, layer, p, x, y, sc, t = c.groups()
                out.append(dict(i=int(i), layer=val(layer), parallax=float(p), x=float(x), y=float(y), scale=float(sc),
                                back=t == "true"))
            norep = {int(k) for k in re.findall(var + r"\.v\[(\d+)\] = (?:\(kDc\(\), kDc\(\), )?iDc", text)}
            for l in out:
                l["repeat"] = l["i"] not in norep
            return out

        def colour(text, var, field, default):
            m = re.search(var + r"\." + field + r" = (-?\d+)", text)
            return (int(m.group(1)) if m else default) & 0xFFFFFF

        themes = {}
        for m in re.finditer(r"\((\w) = new ZDb\(([^,]+), (\d+)\), (.*?), \1\)\)", js):
            var, name, body = m.group(1), val(m.group(2)), m.group(4)
            themes[name] = dict(layers=layers_of(body, var), sky=colour(body, var, "i", -1),
                                ground=colour(body, var, "g", -16777216))
        lines = js.split("\n")
        for k, line in enumerate(lines):
            m = re.match(r" b = new (?:ZDb|EEb)\(([A-Za-z0-9_$]+) \+ \(a \? mzd : opd\), ", line)
            if not m:
                continue
            body = []
            j = k + 1
            while not lines[j].startswith(" return b"):
                body.append(lines[j])
                j += 1
            text = "\n".join(body)
            hd = text.split("} else {", 1)[1] if " if (a) {" in text and "} else {" in text else text
            themes[val(m.group(1))] = dict(layers=layers_of(hd, "b"), sky=colour(text, "b", "i", -1),
                                           ground=colour(text, "b", "g", -16777216))
        return themes

    def backgrounds(self, themes, defs):
        """Each theme: sky colour, ground colour, its layer count (u16 each, padding), then 8 layers: picture (u16),
        flags (1: in front of the world, 2: repeated across), parallax, x, y (the game's pixels), and the picture's
        pixels for one of the game's (f32 each)."""
        out = bytearray()
        made = {}
        for theme in themes:
            d = defs[theme]
            layers = []
            for l in d["layers"]:
                path = os.path.join(self.ref, "images/BackgroundThemes", l["layer"] + ".png")
                if not os.path.exists(path):
                    continue
                q = 0.125 if l["back"] and l["parallax"] < 1 else 0.25   # (far layers at half the detail)
                key = (l["layer"], l["scale"], q)
                if key not in made:
                    im = Image.open(path).convert("RGBA")
                    w = max(1, int(round(im.width * l["scale"] * q)))
                    h = max(1, int(round(im.height * l["scale"] * q)))
                    made[key] = self.add(im.resize((w, h), Image.Resampling.LANCZOS), dither=True)
                pic = made[key]
                flags = (0 if l["back"] else 1) | (2 if l["repeat"] else 0)
                layers.append(struct.pack("<HHffff", pic, flags, l["parallax"], l["x"], l["y"], q))
            assert len(layers) <= 8, theme
            sky, gr = d["sky"], d["ground"]
            out += struct.pack("<HHHH", rgb565(sky >> 16, (sky >> 8) & 255, sky & 255),
                               rgb565(gr >> 16, (gr >> 8) & 255, gr & 255), len(layers), 0)
            out += b"".join(layers) + bytes(20 * (8 - len(layers)))
        return out

    # -------------------------------------------------------------- output
    def table(self):
        t = bytearray()
        for off, w, h, px4, py4, flags in self.rows:
            t += struct.pack("<IHHhhHH", off, w, h, px4, py4, flags, 0)
        return t

    def pixels(self):
        return bytes(self.blob)

    def header(self):
        h = [f"#define PICTURE_COUNT {len(self.rows)}", "enum {"]
        for n, i in sorted(self.names.items(), key=lambda kv: kv[1]):
            h.append(f"  P_{n} = {i},")
        h.append("};")
        return h
