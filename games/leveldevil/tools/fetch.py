#!/usr/bin/env python3
"""Level Devil's rooms, as the game itself has them: fetches its web build, a Stencyl game, and writes
tools/scenes.json, the 80 rooms of its first world (16 levels of 5) with their tiles and actors.

The build is a script (an OpenFL app) and a packed library of assets (lib/default.pak): its manifest,
inside the script, says where each file is in the library (gzip'ed). A room is two files there:
  scene-N.scn  its tiles, run-length coded after a header of 12 or 16 bytes: runs of (2 bytes,
               tileset, tile, count), -1 for none, 33 x 21 tiles of 16 pixels, row by row;
  scene-N.mbs  the scene (Stencyl's "MBS" binary format: fixed fields, big-endian), with its actors:
               where each one is, its type, its scale and angle, its name.
What the traps do is in the script, one class a room (scripts.SceneEvents_N): tools/rooms.py has it,
written out by hand from there. The game's colours are there too (the ground of each level).

The rooms are kept as rectangles of tiles (the ground) and actors; scenes.json is committed, so the
build needs neither the network nor this script.

Usage: fetch.py [--from DIR]   (DIR: a folder that already has "Level Devil.js" and default.pak)
"""
import argparse
import json
import os
import re
import struct
import sys
import tempfile
import urllib.parse
import urllib.request
import zlib

URL = "https://st.8games.net/14/igra-level-devil-uroven-dyavola/"
WORLDS = ["Pits", "Spikes", "Push", "Coins", "Controls", "Platforms", "Springs", "Warps", "Scale", "Doors", "Saws",
          "Flappy", "Gravity", "Movement", "Wraparound", "Boss"]
W, H = 33, 21

SCENE = [("retainAtlases", "b"), ("depth", "i"), ("description", "s"), ("eventSnippetID", "i"), ("extendedHeight", "i"),
         ("extendedWidth", "i"), ("extendedX", "i"), ("extendedY", "i"), ("format", "s"), ("gravityX", "f"),
         ("gravityY", "f"), ("height", "i"), ("id", "i"), ("name", "s"), ("revision", "s"), ("savecount", "i"),
         ("tileDepth", "i"), ("tileHeight", "i"), ("tileWidth", "i"), ("type", "s"), ("width", "i"),
         ("actorInstances", "l"), ("atlasMembers", "l"), ("layers", "l")]
ACTOR = [("angle", "f"), ("id", "i"), ("customized", "b"), ("groupID", "i"), ("aid", "i"), ("name", "s"),
         ("scaleX", "f"), ("scaleY", "f"), ("x", "i"), ("y", "i"), ("z", "i"), ("orderInLayer", "i"), ("snippets", "l")]
ACTOR_SIZE = 49


def fetch(name, folder):
    path = os.path.join(folder, name)
    if not os.path.exists(path):
        with urllib.request.urlopen(URL + urllib.parse.quote(name), timeout=120) as r, open(path, "wb") as f:
            f.write(r.read())
    return open(path, "rb").read()


def library(script, pak):
    """The packed library's files: {path: bytes}"""
    js = script.decode("utf-8", "replace")
    end = js.find('libraryType":"lime.utils.PackedAssetLibrary')
    manifest = js[js.rfind('{"name":null,"assets":"', 0, end):end]
    out = {}
    for m in re.finditer(r"(?:position|R0)i(\d+)(?:y4:size|R1)(?:z|i\d+)(?:y4:type|R2)(?:y\d+:[A-Z]+|R\d+)(?:y2:id|R4)y(\d+):",
                         manifest):
        pos, n = int(m.group(1)), int(m.group(2))
        rest = manifest[m.end():]
        ident = urllib.parse.unquote(rest[:n])
        length = re.match(r"(?:y6:length|R6)i(\d+)", rest[n:])
        if length and ident.startswith("assets/data/scene-"):
            out[ident] = zlib.decompress(pak[pos:pos + int(length.group(1))], 31)
    return out


class Mbs:
    def __init__(self, data):
        self.d = data
        table = struct.unpack_from(">I", data, 12)[0]
        offs = struct.unpack_from(">%dI" % struct.unpack_from(">I", data, table)[0], data, table + 4)
        self.strings = [data[o + 4:o + 4 + struct.unpack_from(">I", data, o)[0]].decode("utf-8", "replace") for o in offs]

    def read(self, at, fields):
        out = {}
        for name, t in fields:
            if t == "b":
                out[name], at = self.d[at], at + 1
                continue
            v = struct.unpack_from(">f" if t == "f" else ">i" if t != "l" else ">I", self.d, at)[0]
            out[name] = self.strings[v] if t == "s" else v
            at += 4
        return out


def tiles(scn):
    cells, at = [], 12 if struct.unpack_from(">I", scn, 4)[0] == 0 else 16
    while at + 8 <= len(scn):
        _, tileset, _, count = struct.unpack_from(">HhhH", scn, at)
        cells += [tileset != -1] * count
        at += 8
    cells += [False] * (W * H - len(cells))
    return [cells[y * W:(y + 1) * W] for y in range(len(cells) // W)]


def rectangles(grid):
    """the solid tiles as few rectangles: runs along the rows, joined downwards when the same"""
    rects, open_ = [], {}
    for y, row in enumerate(grid + [[False] * W]):
        runs, x = [], 0
        while x < W:
            if row[x]:
                x0 = x
                while x < W and row[x]:
                    x += 1
                runs.append((x0, x - x0))
            x += 1
        nxt = {}
        for run in runs:
            nxt[run] = open_.pop(run, y)
        for (x0, w), y0 in open_.items():
            rects.append((x0, y0, w, y - y0))
        open_ = nxt
    return sorted(rects, key=lambda r: (r[1], r[0]))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--from", dest="src")
    a = ap.parse_args()
    folder = a.src or tempfile.mkdtemp()
    script = fetch("Level Devil.js", folder)
    pak = fetch("lib/default.pak", folder) if not a.src else open(os.path.join(folder, "default.pak"), "rb").read()
    files = library(script, pak)
    by_name = {}
    for path, data in files.items():
        if path.endswith(".mbs") and re.fullmatch(r"(%s)_[1-5]" % "|".join(WORLDS), Mbs(data).read(struct.unpack_from(">I", data, 16)[0], SCENE)["name"]):
            m = Mbs(data)
            s = m.read(struct.unpack_from(">I", data, 16)[0], SCENE)
            n = struct.unpack_from(">I", data, s["actorInstances"])[0]
            actors = [m.read(s["actorInstances"] + 8 + i * ACTOR_SIZE, ACTOR) for i in range(n)]
            grid = tiles(files[path[:-4] + ".scn"])
            assert len(grid) == H and len(grid[-1]) == W, s["name"]
            by_name[s["name"]] = dict(scene=s["id"], ground=rectangles(grid[:15]),
                                      actors=[dict(name=x["name"], type=x["aid"], x=x["x"], y=x["y"],
                                                   sx=round(x["scaleX"], 3), sy=round(x["scaleY"], 3),
                                                   angle=round(x["angle"]) % 360) for x in actors])
    rooms = []
    for w in WORLDS:
        for r in range(1, 6):
            rooms.append(dict(name=f"{w}_{r}", **by_name[f"{w}_{r}"]))
    out = os.path.join(os.path.dirname(os.path.abspath(__file__)), "scenes.json")
    with open(out, "w") as f:
        f.write("[\n" + ",\n".join(json.dumps(r, separators=(",", ":")) for r in rooms) + "\n]\n")
    print(f"{len(rooms)} rooms into {out}", file=sys.stderr)


if __name__ == "__main__":
    main()
