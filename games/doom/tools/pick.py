#!/usr/bin/env python3
"""pick.py IWAD [PWAD...] [--maps "E1M1 E1M2 ..."] --out DIR: the WAD a calculator build is made from (the
Makefile's WAD=, PWAD= and MAPS=, see the README).

The PWADs go over the IWAD in order, as Chocolate Doom's -merge puts them: their maps, sprites, flats and wall
patches replace those of the same name (or are added), and their other lumps replace the IWAD's (TEXTURE1,
PNAMES...). Then only the chosen maps are kept, and only what they can show: the wall textures their sides use
(and those vanilla Doom needs by name: switches, animations, skies), the wall patches of those, the flats of their
floors and ceilings (and the animated ones, the sky's and the screens'), and the sprites of the things in them,
of what those things can turn into, fire or leave behind, and of the player and the weapons. What is left out of
the sprites and wall patches becomes an empty picture (a thing that is missing shows nothing, it does not stop
the game). A lump NWMODE says which game the full WAD is (shareware, registered, retail or commercial), as the
maps left out can no longer tell.

MAPS: map names (E2M1, MAP07), episodes (E2: its maps) or ranges (MAP01-MAP06); all of them when not given. The
WAD is written as DIR/<the IWAD's file name> (its name tells the game which one it is: doom.wad, doom2.wad,
freedoom1.wad...)."""
import argparse
import os
import re
import struct
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
SRC = os.path.join(os.path.dirname(HERE), "src")
MAP_LUMPS = ("THINGS", "LINEDEFS", "SIDEDEFS", "VERTEXES", "SEGS", "SSECTORS", "NODES", "SECTORS", "REJECT",
             "BLOCKMAP")
SECTIONS = {"S": ("S_START", "S_END"), "F": ("F_START", "F_END"), "P": ("P_START", "P_END")}
# a 1 x 1 picture with nothing in it
EMPTY_PATCH = struct.pack("<hhhhI", 1, 1, 0, 0, 12) + b"\xff"
# the flats the game draws besides the maps' (the ending's text screens, the keys page, the border of the smaller
# screen sizes, the sky's)
SCREEN_FLATS = {"FLOOR4_8", "SFLR6_1", "MFLR8_4", "MFLR8_3", "SLIME16", "RROCK14", "RROCK07", "RROCK17", "RROCK13",
                "RROCK19", "FLOOR7_2", "GRNROCK", "F_SKY1"}
# the player's own: the weapons, what they fire and hit, teleporting, items coming back, crushed bodies
ALWAYS_SPRITES = {"PLAY", "PUNG", "PISG", "PISF", "SHTG", "SHTF", "SHT2", "CHGG", "CHGF", "MISG", "MISF", "SAWG",
                  "PLSG", "PLSF", "BFGG", "BFGF", "MISL", "PLSS", "PLSE", "BFS1", "BFE1", "BFE2", "PUFF", "BLUD",
                  "TFOG", "IFOG", "POL5"}
# what action functions spawn (p_enemy.c), and what monsters drop when killed (P_KillMobj)
SPAWNS = {"A_TroopAttack": ["MT_TROOPSHOT"], "A_HeadAttack": ["MT_HEADSHOT"], "A_CyberAttack": ["MT_ROCKET"],
          "A_BruisAttack": ["MT_BRUISERSHOT"], "A_BspiAttack": ["MT_ARACHPLAZ"], "A_FatAttack1": ["MT_FATSHOT"],
          "A_FatAttack2": ["MT_FATSHOT"], "A_FatAttack3": ["MT_FATSHOT"], "A_SkelMissile": ["MT_TRACER"],
          "A_Tracer": ["MT_SMOKE"], "A_VileTarget": ["MT_FIRE"], "A_PainAttack": ["MT_SKULL"],
          "A_PainDie": ["MT_SKULL"], "A_BrainSpit": ["MT_SPAWNSHOT"], "A_SpawnFly": ["MT_SPAWNFIRE"],
          "A_BrainScream": ["MT_ROCKET"], "A_BrainExplode": ["MT_ROCKET"]}
DROPS = {"MT_WOLFSS": "MT_CLIP", "MT_POSSESSED": "MT_CLIP", "MT_SHOTGUY": "MT_SHOTGUN", "MT_CHAINGUY": "MT_CHAINGUN"}
# what the boss brain's cubes turn into (A_SpawnFly)
BRAIN_SPAWNS = ["MT_TROOP", "MT_SERGEANT", "MT_SHADOWS", "MT_PAIN", "MT_HEAD", "MT_VILE", "MT_UNDEAD", "MT_BABY",
                "MT_FATSO", "MT_KNIGHT", "MT_BRUISER"]


def fail(msg):
    sys.exit(f"pick.py: {msg}")


def read_wad(path):
    d = open(path, "rb").read()
    if d[:4] not in (b"IWAD", b"PWAD"):
        fail(f"{path} is not a WAD")
    n, off = struct.unpack("<II", d[4:12])
    lumps = []
    for i in range(n):
        o, s, name = struct.unpack("<II8s", d[off + 16 * i:off + 16 * i + 16])
        lumps.append([name.split(b"\0")[0].decode("latin1").upper(), d[o:o + s]])
    return lumps


def write_wad(path, lumps):
    data = bytearray(b"IWAD" + struct.pack("<II", len(lumps), 0))
    dir_ = bytearray()
    for name, d in lumps:
        dir_ += struct.pack("<II8s", len(data), len(d), name.encode("latin1"))
        data += d
    struct.pack_into("<I", data, 8, len(data))
    open(path, "wb").write(bytes(data + dir_))


def is_map(name):
    return bool(re.fullmatch(r"E\dM\d|MAP\d\d", name))


def section_of(lumps, i):
    """the section (S, F, P) lump i is in, or None"""
    for k, (a, b) in SECTIONS.items():
        start = None
        for j in range(i, -1, -1):
            n = lumps[j][0]
            if n in (a, a[0] + a):
                start = j
                break
            if n in (b, b[0] + b):
                break
        if start is not None:
            return k
    return None


def split(lumps):
    """maps (name -> its lumps), sections (S/F/P -> [name, data] in order), the rest in order"""
    maps, sections, rest = {}, {"S": [], "F": [], "P": []}, []
    i = 0
    sec = None
    while i < len(lumps):
        name, d = lumps[i]
        if is_map(name):
            j = i + 1
            while j < len(lumps) and lumps[j][0] in MAP_LUMPS + ("BEHAVIOR",):
                j += 1
            if any(lumps[k][0] == "BEHAVIOR" for k in range(i + 1, j)):
                fail(f"{name} is a Hexen format map, which Doom does not play")
            maps[name] = lumps[i:j]
            i = j
            continue
        for k, (a, b) in SECTIONS.items():
            if name in (a, k + a):
                sec = k
                break
            if name in (b, k + b):
                sec = None
                break
        else:
            if sec:
                sections[sec].append([name, d])
            else:
                rest.append([name, d])
        i += 1
    return maps, sections, rest


def merge(iwad, pwads):
    maps, sections, rest = split(iwad)
    for pw in pwads:
        pmaps, psections, prest = split(pw)
        maps.update(pmaps)
        for k, items in psections.items():
            for name, d in items:
                for item in sections[k]:
                    if item[0] == name:
                        item[1] = d
                        break
                else:
                    sections[k].append([name, d])
        for name, d in prest:
            for item in reversed(rest):
                if item[0] == name:
                    item[1] = d
                    break
            else:
                rest.append([name, d])
    return maps, sections, rest


def map_order(name):
    m = re.fullmatch(r"E(\d)M(\d)", name)
    return (int(m.group(1)), int(m.group(2))) if m else (0, int(name[3:]))


def choose(maps, spec):
    names = sorted(maps, key=map_order)
    if not spec or spec.strip().lower() == "all":
        return names
    out = []
    for item in spec.replace(",", " ").upper().split():
        if "-" in item:
            a, b = item.split("-")
            if a not in maps or b not in maps:
                fail(f"no map {a if a not in maps else b} in the WAD (it has {' '.join(names)})")
            out += [n for n in names if map_order(a) <= map_order(n) <= map_order(b)]
        elif re.fullmatch(r"E\d", item):
            got = [n for n in names if n.startswith(item + "M")]
            if not got:
                fail(f"no episode {item[1]} in the WAD (it has {' '.join(names)})")
            out += got
        elif item in maps:
            out.append(item)
        else:
            fail(f"no map {item} in the WAD (it has {' '.join(names)})")
    return sorted(set(out), key=map_order)


def named(pattern, path):
    return set(re.findall(pattern, open(path).read()))


def info_tables():
    """vanilla's states (sprite, next state, action) and things (doomednum, their states), from src/doom/info.c"""
    text = open(os.path.join(SRC, "doom", "info.c")).read()
    states = {}
    for m in re.finditer(r"STATE\(SPR_(\w+),[^,]+,[^,]+,\s*(\w+),\s*(S_\w+),[^)]*\),?\s*//\s*(S_\w+)", text):
        states[m.group(4)] = (m.group(1), m.group(3), m.group(2))
    things = {}
    body = text[text.index("mobjinfo[NUMMOBJTYPES]"):]
    for m in re.finditer(r"\{\s*//\s*(MT_\w+)(.*?)\n\s*\}", body, re.S):
        fields = dict((k, v) for v, k in re.findall(r"\s*([^\n]+?),?\s*//\s*(\w+)", m.group(2)))
        things[m.group(1)] = fields
    return states, things


def needed_sprites(kept_maps, maps):
    states, things = info_tables()
    by_num = {}
    for mt, f in things.items():
        try:
            by_num[int(f["doomednum"])] = mt
        except (KeyError, ValueError):
            pass
    todo = []
    for name in kept_maps:
        lumps = dict(maps[name][1:])
        t = lumps.get("THINGS", b"")
        for i in range(0, len(t) - 9, 10):
            num = struct.unpack("<h", t[i + 6:i + 8])[0]
            if num in by_num:
                todo.append(by_num[num])
    todo += ["MT_PLAYER", "MT_BARREL", "MT_PUFF", "MT_BLOOD", "MT_TFOG", "MT_IFOG"]
    seen_mt, seen_st, sprites = set(), set(), set(ALWAYS_SPRITES)
    while todo:
        mt = todo.pop()
        if mt in seen_mt or mt not in things:
            continue
        seen_mt.add(mt)
        if mt in DROPS:
            todo.append(DROPS[mt])
        if mt in ("MT_BOSSBRAIN", "MT_BOSSSPIT", "MT_SPAWNSHOT"):
            todo += BRAIN_SPAWNS
        for field in ("spawnstate", "seestate", "painstate", "meleestate", "missilestate", "deathstate",
                      "xdeathstate", "raisestate"):
            st = things[mt].get(field, "S_NULL")
            while st in states and st not in seen_st and st != "S_NULL":
                seen_st.add(st)
                spr, nxt, action = states[st]
                sprites.add(spr)
                todo += SPAWNS.get(action, [])
                st = nxt
    return sprites


def unpack_sides(lumps):
    """the map with each line's sides its own, the front one then the back one, in the lines' order (as the
    calculator's packed maps keep them: editors share sides that look the same, which vanilla Doom allows)"""
    d = dict(lumps[1:])
    lines, sides = bytearray(d["LINEDEFS"]), d["SIDEDEFS"]
    out = bytearray()
    for i in range(0, len(lines) - 13, 14):
        for k in (10, 12):
            sn = struct.unpack("<H", lines[i + k:i + k + 2])[0]
            if sn != 0xFFFF:
                struct.pack_into("<H", lines, i + k, len(out) // 30)
                out += sides[30 * sn:30 * sn + 30]
    return [[n, bytes(lines) if n == "LINEDEFS" else bytes(out) if n == "SIDEDEFS" else v] for n, v in lumps]


def renumber_bsp(lumps):
    """the map's BSP tree numbered as id's node builder numbers it (the calculator's packed nodes give their
    children as near numbers): each node after its children, a node child just before its parent (the left one
    when both are nodes, after the right one's subtree), and the subsectors in the same order, with their segs"""
    d = dict(lumps[1:])
    nodes, ss, segs = d["NODES"], d["SSECTORS"], d["SEGS"]
    if not nodes:
        return lumps
    new_nodes, new_ss, new_segs = [], [], bytearray()

    def leaf(c):
        num, first = struct.unpack("<HH", ss[4 * c:4 * c + 4])
        new_ss.append(struct.pack("<HH", num, len(new_segs) // 12))
        new_segs.extend(segs[12 * first:12 * (first + num)])
        return 0x8000 | (len(new_ss) - 1)

    def visit(i):
        n = nodes[28 * i:28 * i + 28]
        right, left = struct.unpack("<HH", n[24:28])
        if not right & 0x8000 and not left & 0x8000:
            r = visit(right)
            l_ = visit(left)
        elif not left & 0x8000:
            l_ = visit(left)
            r = leaf(right & 0x7fff)
        elif not right & 0x8000:
            r = visit(right)
            l_ = leaf(left & 0x7fff)
        else:
            r = leaf(right & 0x7fff)
            l_ = leaf(left & 0x7fff)
        new_nodes.append(n[:24] + struct.pack("<HH", r, l_))
        return len(new_nodes) - 1

    sys.setrecursionlimit(10000)
    visit(len(nodes) // 28 - 1)
    new = {"NODES": b"".join(new_nodes), "SSECTORS": b"".join(new_ss), "SEGS": bytes(new_segs)}
    return [[n, new.get(n, v)] for n, v in lumps]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("wads", nargs="+")
    ap.add_argument("--maps", default="")
    ap.add_argument("--out", required=True)
    a = ap.parse_args()
    iwad = read_wad(a.wads[0])
    pwads = [read_wad(p) for p in a.wads[1:]]
    maps, sections, rest = merge(iwad, pwads)
    if not maps:
        fail("the WAD has no maps")
    every = set(maps)
    mode = ("commercial" if any(n.startswith("MAP") for n in every) else "retail" if "E4M1" in every
            else "registered" if "E3M1" in every else "shareware")
    kept = choose(maps, a.maps)

    # wall textures and flats: the maps' own, with the rest of a switch's pair and of an animation's frames, and
    # the skies of their episodes
    lumps = dict(rest)
    whddata = open(os.path.join(SRC, "whddata.h")).read()
    tex_order = re.findall(r"TEXTURE_NAME\((\w+)\)", whddata[whddata.index("NAMED_TEXTURE_LIST"):])
    flat_order = re.findall(r"FLAT_NAME\((\w+)\)", whddata[whddata.index("NAMED_FLAT_LIST"):])
    groups = [[a_, b_] for a_, b_ in re.findall(r"TEXTURE_NAME\((\w+)\),\s*TEXTURE_NAME\((\w+)\)",
                                              open(os.path.join(SRC, "doom", "p_switch.c")).read())]
    spec = open(os.path.join(SRC, "doom", "p_spec.c")).read()
    for kind, order in (("TEXANIM_DEF", tex_order), ("FLATANIM_DEF", flat_order)):
        for end, start in re.findall(kind + r"\((\w+),\s*(\w+),", spec):
            if start in order and end in order:
                groups.append(order[order.index(start):order.index(end) + 1])
    used_tex, used_flats = set(), set(SCREEN_FLATS)
    for name in kept:
        ml = dict(maps[name][1:])
        sd = ml.get("SIDEDEFS", b"")
        for i in range(0, len(sd) - 29, 30):
            for k in (4, 12, 20):
                used_tex.add(sd[i + k:i + k + 8].split(b"\0")[0].decode("latin1").upper())
        se = ml.get("SECTORS", b"")
        for i in range(0, len(se) - 25, 26):
            for k in (4, 12):
                used_flats.add(se[i + k:i + k + 8].split(b"\0")[0].decode("latin1").upper())
        e, m = map_order(name)
        used_tex.add(f"SKY{e}" if e else "SKY1" if m < 12 else "SKY2" if m < 21 else "SKY3")
    for g in groups:
        for used in (used_tex, used_flats):
            if used & set(g):
                used |= set(g)
    pnames = lumps.get("PNAMES")
    if pnames is None or "TEXTURE1" not in lumps:
        fail("the WAD has no PNAMES or TEXTURE1")
    npn = struct.unpack("<i", pnames[:4])[0]
    pname = [pnames[4 + 8 * i:12 + 8 * i].split(b"\0")[0].decode("latin1").upper() for i in range(npn)]
    used_patches = set()
    kept_textures = 0
    for tl in ("TEXTURE1", "TEXTURE2"):
        if tl not in lumps:
            continue
        d = lumps[tl]
        n = struct.unpack("<i", d[:4])[0]
        offs = struct.unpack(f"<{n}i", d[4:4 + 4 * n])
        out = []
        for o in offs:
            name = d[o:o + 8].split(b"\0")[0].decode("latin1").upper()
            count = struct.unpack("<h", d[o + 20:o + 22])[0]
            entry = d[o:o + 22 + 10 * count]
            if name in used_tex:
                out.append(entry)
                for p in range(count):
                    used_patches.add(pname[struct.unpack("<h", entry[22 + 10 * p + 4:22 + 10 * p + 6])[0]])
        kept_textures += len(out)
        body = bytearray(struct.pack("<i", len(out)))
        o = 4 + 4 * len(out)
        for e in out:
            body += struct.pack("<i", o)
            o += len(e)
        for e in out:
            body += e
        for item in rest:
            if item[0] == tl:
                item[1] = bytes(body)
    if kept_textures > 255:
        fail(f"{kept_textures} wall textures: the calculator's build has room for 255 (choose fewer maps)")
    sprites = needed_sprites(kept, maps)
    # (the markers inside, P1_START..., stay as they are)
    sections["S"] = [[n, d if n[:4] in sprites or n.endswith(("_START", "_END")) else EMPTY_PATCH]
                     for n, d in sections["S"]]
    sections["P"] = [[n, d if n in used_patches or n.endswith(("_START", "_END")) else EMPTY_PATCH]
                     for n, d in sections["P"]]
    flats = sections["F"]
    sections["F"] = [[n, d] for n, d in flats if n in used_flats or n.endswith(("_START", "_END"))]
    if len(sections["F"]) > 255:
        fail(f"{len(sections['F'])} flats: the calculator's build has room for 255")

    # (a calculator has no sound: the music and sound effects are left out, and so are the demos of maps left
    # out, or recorded by other versions of Doom than 1.9)
    def keep_lump(name, d):
        if re.fullmatch(r"D_\w+|DS\w+|DP\w+|GENMIDI|DMXGUS\w*", name):
            return False
        if re.fullmatch(r"DEMO\d", name):
            if len(d) < 13 or d[0] != 109:
                return False
            demo_map = f"MAP{d[3]:02d}" if mode == "commercial" else f"E{d[2]}M{d[3]}"
            return demo_map in kept
        return True
    rest = [[n, d] for n, d in rest if keep_lump(n, d)]
    out = list(rest)
    for name in kept:
        out += renumber_bsp(unpack_sides(maps[name]))
    out.append(["NWMODE", mode.encode()])
    for k, (a_, b_) in SECTIONS.items():
        out.append([a_, b""])
        out += sections[k]
        out.append([b_, b""])
    os.makedirs(a.out, exist_ok=True)
    path = os.path.join(a.out, os.path.basename(a.wads[0]).lower())
    write_wad(path, out)
    print(f"pick.py: {path}: {mode}, maps {' '.join(kept)}; {kept_textures} wall textures, "
          f"{len(used_patches)} wall patches, {len(sections['F'])} flats, "
          f"{len({n[:4] for n, d in sections['S'] if d != EMPTY_PATCH})} sprites")


if __name__ == "__main__":
    main()
