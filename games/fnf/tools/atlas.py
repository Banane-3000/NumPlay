"""The game's pictures, read from its own files (Friday Night Funkin' keeps them in three formats): Sparrow sheets
(a PNG and an XML of named frames), packer sheets (a PNG and a text list) and Adobe Animate texture atlases
(Animation.json, spritemap*.json and their PNGs: symbols made of placed pieces). A frame comes back as a list of
leaves, (piece, 2x3 matrix, alpha), that flatten() draws into one image. Nothing in the reference is run.

After Bobydeluxe's reader for the first port. Used by tools/data.py."""
import json
import math
import re
from pathlib import Path

from PIL import Image, ImageChops

IDENTITY = (1, 0, 0, 1, 0, 0)
opened = set()   # every file read, for tools/ref.txt


def read_json(path):
    opened.add(Path(path))
    return json.loads(Path(path).read_text(encoding="utf-8-sig"))


def open_image(path):
    opened.add(Path(path))
    return Image.open(path).convert("RGBA")


def matrix(d):
    if "MX" in d:
        return tuple(d["MX"])
    if "M3D" in d:
        a = d["M3D"]
        return (a[0], a[1], a[4], a[5], a[12], a[13])
    return IDENTITY


def multiply(a, b):
    return (a[0] * b[0] + a[2] * b[1], a[1] * b[0] + a[3] * b[1], a[0] * b[2] + a[2] * b[3], a[1] * b[2] + a[3] * b[3],
            a[0] * b[4] + a[2] * b[5] + a[4], a[1] * b[4] + a[3] * b[5] + a[5])


def point(m, x, y):
    return (m[0] * x + m[2] * y + m[4], m[1] * x + m[3] * y + m[5])


def bounds(leaves):
    """The box round the visible pixels of a frame, in its own units."""
    pts = []
    for im, m, a in leaves:
        b = im.getbbox()
        if b and a > 0:
            pts.extend(point(m, x, y) for x, y in ((b[0], b[1]), (b[2], b[1]), (b[2], b[3]), (b[0], b[3])))
    if not pts:
        return (0, 0, 1, 1)
    return (min(p[0] for p in pts), min(p[1] for p in pts), max(p[0] for p in pts), max(p[1] for p in pts))


def flatten(leaves, scale=1.0, box=None, resample=Image.Resampling.BICUBIC):
    """Draws the leaves scaled into one RGBA image; returns it and where its top left falls (in scaled units)."""
    box = box or bounds(leaves)
    x0, y0 = math.floor(box[0] * scale), math.floor(box[1] * scale)
    x1, y1 = math.ceil(box[2] * scale), math.ceil(box[3] * scale)
    out = Image.new("RGBA", (max(1, x1 - x0), max(1, y1 - y0)))
    for im, m, alpha in leaves:
        a, b, c, d, tx, ty = m
        a, b, c, d = a * scale, b * scale, c * scale, d * scale
        tx, ty = tx * scale - x0, ty * scale - y0
        det = a * d - b * c
        if abs(det) < 1e-8:
            continue
        inv = (d / det, -c / det, (c * ty - d * tx) / det, -b / det, a / det, (b * tx - a * ty) / det)
        src = im
        # shrinking a lot: shrink the piece first, so that the affine sampling doesn't alias
        shrink = min(math.hypot(a, b), math.hypot(c, d))
        if shrink < 0.5 and resample != Image.Resampling.NEAREST:
            f = max(1, int(1 / shrink / 2))
            if f > 1 and im.width >= 2 * f and im.height >= 2 * f:
                src = im.resize((max(1, im.width // f), max(1, im.height // f)), Image.Resampling.BOX)
                sx, sy = im.width / src.width, im.height / src.height
                inv = (inv[0] / sx, inv[1] / sx, inv[2] / sx, inv[3] / sy, inv[4] / sy, inv[5] / sy)
        layer = src.transform(out.size, Image.Transform.AFFINE, inv, resample)
        if alpha < 0.999:
            layer.putalpha(layer.getchannel("A").point(lambda v: round(v * alpha)))
        out.alpha_composite(layer)
    return out, x0, y0


class Atlas:
    """An Adobe Animate texture atlas (a folder with Animation.json)."""

    def __init__(self, path):
        self.path = Path(path)
        self.d = read_json(self.path / "Animation.json")
        self.symbols = {x["SN"]: x["TL"] for x in self.d.get("SD", {}).get("S", [])}
        self.textures, self.labels = {}, {}
        for p in sorted(self.path.glob("spritemap*.json")):
            im = open_image(p.with_suffix(".png"))
            for item in read_json(p)["ATLAS"]["SPRITES"]:
                s = item["SPRITE"]
                piece = im.crop((s["x"], s["y"], s["x"] + s["w"], s["y"] + s["h"]))
                if s.get("rotated"):
                    piece = piece.transpose(Image.Transpose.ROTATE_90)
                self.textures[str(s["name"])] = piece
        for layer in self.d["AN"]["TL"]["L"]:
            for f in layer["FR"]:
                if f.get("N"):
                    self.labels[f["N"]] = (f["I"], f["DU"])

    @staticmethod
    def duration(tl):
        return max((f["I"] + f["DU"] for l in tl["L"] for f in l["FR"]), default=1)

    def sequence(self, prefix, symbol=False):
        """(timeline, first frame, frame count) of an animation, by frame label or by symbol name."""
        def match(keys):
            exact = next((k for k in keys if k.lower() == prefix.lower()), None)
            return exact or next((k for k in keys if k.lower().startswith(prefix.lower())), None)
        if symbol:
            k = match(self.symbols)
            if k:
                return self.symbols[k], 0, self.duration(self.symbols[k])
            if self.d["AN"].get("SN", "").lower().startswith(prefix.lower()):
                return self.d["AN"]["TL"], 0, self.duration(self.d["AN"]["TL"])
        k = match(self.labels)
        if k:
            return (self.d["AN"]["TL"], *self.labels[k])
        k = match(self.symbols)
        if k:
            return self.symbols[k], 0, self.duration(self.symbols[k])
        raise KeyError((self.path.name, prefix))

    def walk(self, tl, frame, transform=IDENTITY, alpha=1.0, depth=0):
        if depth > 40:
            raise ValueError("cyclic symbols")
        result = []
        for l in reversed(tl["L"]):
            if l.get("LT") == "Clp":
                continue
            f = next((f for f in l["FR"] if f["I"] <= frame < f["I"] + f["DU"]), None)
            if not f:
                continue
            leaves = []
            for e in f.get("E", []):
                if "ASI" in e:
                    d = e["ASI"]
                    im = self.textures.get(str(d["N"]))
                    if im is not None:
                        leaves.append((im, multiply(transform, matrix(d)), alpha))
                elif "SI" in e:
                    d = e["SI"]
                    child = self.symbols.get(d["SN"])
                    if child is None:
                        continue
                    t, loop = d.get("FF", 0), d.get("LP", "LP")
                    if loop != "SF":
                        t += frame - f["I"]
                    length = self.duration(child)
                    t = t % length if loop == "LP" else min(t, length - 1)
                    a = alpha * d.get("C", {}).get("AM", 1)
                    leaves.extend(self.walk(child, t, multiply(transform, matrix(d)), a, depth + 1))
            if l.get("Clpb") and leaves:   # a clipping mask over this layer
                mask = next((x for x in tl["L"] if x.get("LN") == l["Clpb"]), None)
                if mask:
                    masks = self.walk({"L": [{"LN": mask["LN"], "FR": mask["FR"]}]}, frame, transform, alpha, depth + 1)
                    box = bounds(leaves)
                    im, x, y = flatten(leaves, box=box)
                    matte, _, _ = flatten(masks, box=box)
                    im.putalpha(ImageChops.multiply(im.getchannel("A"), matte.getchannel("A")))
                    b = im.getbbox()
                    leaves = [(im.crop(b), (1, 0, 0, 1, x + b[0], y + b[1]), 1)] if b else []
            result.extend(leaves)
        return result

    def frames(self, animation, picks):
        """The frames at the given indices of an animation (indices into its frameIndices, if it has any)."""
        tl, start, duration = self.sequence(animation["prefix"], animation.get("animType") == "symbol")
        ids = animation.get("frameIndices") or list(range(duration))
        return [self.walk(tl, start + ids[min(i, len(ids) - 1)]) for i in picks], len(ids)


class Reference:
    """The game's assets folder (funkin.assets)."""

    def __init__(self, root):
        self.root = Path(root)
        self.atlases, self.sparrows = {}, {}
        self.boxes = []   # the untrimmed boxes of the Sparrow frames frames() returned last

    def json(self, rel):
        return read_json(self.root / rel)

    def path(self, name, library="shared"):
        if ":" in name:
            library, name = name.split(":", 1)
        choices = [self.root / lib / "images" / name for lib in dict.fromkeys([library, "shared", "preload"])]
        return next((p for p in choices if p.is_dir() or p.with_suffix(".png").exists()), choices[0])

    def atlas(self, path):
        path = Path(path)
        if path not in self.atlases:
            self.atlases[path] = Atlas(path)
        return self.atlases[path]

    def sparrow(self, path):
        path = Path(path)
        if path not in self.sparrows:
            opened.add(path.with_suffix(".xml"))
            text = path.with_suffix(".xml").read_text(encoding="utf-8-sig")
            im = open_image(path.with_suffix(".png"))
            frames = []
            # (read with a pattern, not an XML parser: some names have a bare < or & in them)
            for tag in re.findall(r"<SubTexture\s(.*?)/>", text, re.S):
                d = dict(re.findall(r'(\w+)="(.*?)"', tag))
                x, y, w, h = (int(d[k]) for k in ("x", "y", "width", "height"))
                piece = im.crop((x, y, x + w, y + h))
                if d.get("rotated") == "true":
                    piece = piece.transpose(Image.Transpose.ROTATE_90)
                fw, fh = int(d.get("frameWidth", w)), int(d.get("frameHeight", h))
                name = d["name"].replace("&amp;", "&").replace("&lt;", "<").replace("&gt;", ">").replace("&quot;", '"')
                frames.append((name, piece, (1, 0, 0, 1, -int(d.get("frameX", 0)), -int(d.get("frameY", 0))), fw, fh))
            self.sparrows[path] = frames
        return self.sparrows[path]

    def frames(self, path, animation, picks=(0,)):
        """The frames at indices `picks` of an animation (a dict with prefix, and maybe frameIndices, animType), as
        leaves; and the animation's length in frames. (Its offsets are the caller's.)"""
        path = Path(path)
        if path.with_suffix(".xml").exists():
            frames = [f for f in self.sparrow(path) if f[0].startswith(animation.get("prefix", ""))]
            if not frames:
                raise KeyError((str(path), animation.get("prefix")))
            ids = animation.get("frameIndices") or list(range(len(frames)))
            out, self.boxes = [], []
            for i in picks:
                _, im, m, fw, fh = frames[ids[min(i, len(ids) - 1)] % len(frames)]
                out.append([(im, m, 1)])
                self.boxes.append((0, 0, fw, fh))
            return out, len(ids)
        if path.with_suffix(".txt").exists():
            opened.add(path.with_suffix(".txt"))
            im = open_image(path.with_suffix(".png"))
            items = []
            for line in path.with_suffix(".txt").read_text().splitlines():
                m = re.match(r"(.+?)\s*=\s*(\d+)\s+(\d+)\s+(\d+)\s+(\d+)", line)
                if m and m[1].startswith(animation.get("prefix", "")):
                    x, y, w, h = map(int, m.groups()[1:])
                    items.append((m[1], im.crop((x, y, x + w, y + h))))
            if not items:
                raise KeyError((str(path), animation.get("prefix")))
            items.sort(key=lambda it: it[0])
            ids = animation.get("frameIndices") or list(range(len(items)))
            return [[(items[ids[min(i, len(ids) - 1)] % len(items)][1], IDENTITY, 1)] for i in picks], len(ids)
        if path.is_dir():
            for p in sorted(path.rglob("Animation.json")):
                try:
                    return self.atlas(p.parent).frames(animation, picks)
                except KeyError:
                    pass
            raise KeyError((str(path), animation.get("prefix")))
        im = open_image(path.with_suffix(".png"))
        return [[(im, IDENTITY, 1)] for _ in picks], 1
