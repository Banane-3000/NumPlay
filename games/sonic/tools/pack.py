#!/usr/bin/env python3
"""Packs what the game reads from the Sonic Retro disassembly (REF, see fetch.py) into
src/data.bin, with src/data.h naming what is where and src/s1const.h the disassembly's numbers.

    python3 -I tools/pack.py REF

Everything is decompressed here, once, so the calculator reads it in place from flash:
- the patterns of every piece of art (Nemesis, Kosinski and uncompressed), 32 bytes each, one pool;
- sprite mappings, animation scripts and Sonic's pattern load cues, as the game keeps them, little-endian;
- each zone's 16x16 blocks, 256x256 chunks and collision index, the angle map and height arrays;
- the act layouts, object placement, start positions, palettes and palette cycles;
- the special stage layouts, the tile maps of the title screen, the SEGA logo and the special stages;
- the demos' button presses.
"""
import os
import re
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import s1  # noqa: E402

REF = sys.argv[1] if len(sys.argv) > 1 else "build/ref/s1disasm"
OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "src")


def rd(p):
    return open(os.path.join(REF, p), "rb").read()


def asm_lines(p):
    return [s1.strip(l) for l in open(os.path.join(REF, p), encoding="latin1")]


def ident(name):
    return re.sub(r"[^A-Za-z0-9]+", "_", name).strip("_")


# ---------------------------------------------------------------- symbols

def all_asm():
    out = []
    for d, _, fs in os.walk(REF):
        if "build_tools" in d or "sound" in d:
            continue
        out += [os.path.join(d, f) for f in fs if f.endswith(".asm")]
    return sorted(out)


SYM = {"Revision": 1, "FixBugs": 0}
pending = []
for path in all_asm():
    for line in open(path, encoding="latin1"):
        m = re.match(r"^\s*([A-Za-z_][A-Za-z_0-9]*):?\s+(?:=|equ)\s+(.+)$", s1.strip(line))
        if m:
            pending.append((m.group(1), m.group(2).strip()))
for _ in range(6):
    left = []
    for n, e in pending:
        if n in SYM:
            continue
        try:
            SYM[n] = s1.number(e, SYM)
        except Exception:
            left.append((n, e))
    pending = left

# object ids: their order in the object pointer table
OBJ = []
skip = False
for l in asm_lines("_inc/Object Pointers.asm"):
    if re.match(r"^\s*if\s+UnusedOptimization", l):
        skip = True
        continue
    if re.match(r"^\s*else\b", l):
        skip = not skip
        continue
    if re.match(r"^\s*endif\b", l):
        skip = False
        continue
    m = re.match(r"^(id_\w+):\s+objptr\s+(\w+)", l)
    if m and not skip:
        OBJ.append((m.group(1), m.group(2)))
for i, (n, _) in enumerate(OBJ, 1):
    SYM.setdefault(n, i)

# label -> file, from the binary includes
FILES = {}
for path in [os.path.join(REF, "sonic.asm")] + [p for p in all_asm() if "_inc" in p]:
    rev = None
    for line in open(path, encoding="latin1"):
        l = s1.strip(line)
        if re.match(r"^\s*if\s+Revision\s*=\s*0", l):
            rev = 0
        elif re.match(r"^\s*if\s+\(Revision=0\)", l):
            rev = 0
        elif re.match(r"^\s*else\b", l) and rev == 0:
            rev = 1
        elif re.match(r"^\s*endif\b", l):
            rev = None
        m = re.match(r'^([A-Za-z_]\w*):\s*binclude(?:EndMarker)?\s+"([^"]+)"', l)
        if m and rev != 0:
            FILES[m.group(1)] = m.group(2)
# (labels before a REV00/REV01 choice)
FILES["Nem_SegaLogo"] = "artnem/Sega Logo (REV01).nem"
FILES["Eni_SegaLogo"] = "tilemaps/Sega Logo (REV01).eni"
FILES["Blk256_MZ"] = "map256/MZ (REV01).kos"
FILES["Blk256_SBZ"] = "map256/SBZ (REV01).kos"
FILES["SS_5"] = "sslayout/5 (REV01).eni"
FILES["SS_6"] = "sslayout/6 (REV01).eni"

# ---------------------------------------------------------------- the blob

blob = bytearray()
defs = []          # (name, value) for data.h


def align(n=4):
    while len(blob) % n:
        blob.append(0)


def put(name, data, a=4):
    align(a)
    defs.append((name, len(blob)))
    blob.extend(data)
    return len(blob) - len(data)


# --- art: one pool of patterns
pool = bytearray()
ART = []           # (name, first, count)


def add_art(name, tiles):
    first = len(pool) // 32
    pool.extend(tiles)
    ART.append((name, first, len(tiles) // 32))


for label in sorted(FILES):
    f = FILES[label]
    if label.startswith("Nem_") and f.endswith(".nem"):
        add_art(label, s1.nemesis(rd(f)))
    elif label.startswith("Art_") and f.endswith(".unc"):
        add_art(label, rd(f))
add_art("Kos_EndFlowers", s1.kosinski(rd("artkos/Flowers at Ending.kos")))
put("D_TILES", pool)
t = bytearray()
for _, first, count in ART:
    t += struct.pack("<HH", first, count)
put("D_ARTS", t)

# --- mappings: every table of every mappings file

def map_tables(path):
    """{table label: frames} of a mappings file (local labels are scoped to their table)."""
    tables, cur, entries, frames, piece_of = {}, None, [], {}, None

    def close():
        if cur:
            tables[cur] = [frames.get(e, []) for e in entries]

    for l in asm_lines(path):
        m = re.match(r"^([A-Za-z_][\w.]*):\s*mappingsTable\b", l)
        if m:
            close()
            cur, entries, frames = re.sub(r"[_.]internal$", "", m.group(1)), [], {}
            continue
        m = re.match(r"^\s*mappingsTableEntry\.w\s+(\S+)", l)
        if m:
            entries.append(m.group(1))
            continue
        m = re.match(r"^([.A-Za-z_]\w*):?\s+spriteHeader\b", l)
        if m:
            piece_of = m.group(1)
            frames[piece_of] = []
            continue
        m = re.match(r"^\s*spritePiece\s+(.*)$", l)
        if m and piece_of:
            x, y, w, h, tile, xf, yf, pal, pri = [s1.number(v, SYM) for v in m.group(1).split(",")]
            word = ((pri & 1) << 15 | (pal & 3) << 13 | (yf & 1) << 12 | (xf & 1) << 11) + tile
            frames[piece_of].append((x, y, ((w - 1) & 3) << 2 | ((h - 1) & 3), word & 0xFFFF))
    close()
    return tables


MAPS = []
for f in sorted(os.listdir(os.path.join(REF, "_maps"))):
    if not f.endswith(".asm") or f.startswith("_") or "Dynamic Gfx" in f or "REV00" in f:
        continue
    for label, frames in map_tables(os.path.join("_maps", f)).items():
        b = bytearray(struct.pack("<H", len(frames)))
        offs, body = [], bytearray()
        start = 2 + 2 * len(frames)
        for fr in frames:
            offs.append(start + len(body))
            body += struct.pack("<BB", len(fr), 0)
            for x, y, size, word in fr:
                body += struct.pack("<bBHbb", y, size, word, x, 0)
        for o in offs:
            b += struct.pack("<H", o)
        MAPS.append((label, bytes(b + body)))

# --- animations: the scripts as the game keeps them


def anim_tables(path):
    tables, cur, entries, scripts, sc = {}, None, [], {}, None

    def close():
        if cur:
            tables[cur] = [scripts[e] for e in entries]

    for l in asm_lines(path):
        m = re.match(r"^([A-Za-z_]\w*):\s*dc\.w\s+(\S+)-(\S+)", l)
        if m:
            close()
            cur, entries, scripts, sc = None, [], {}, None
            if m.group(1).startswith("Ani_"):
                cur, entries = m.group(1), [m.group(2)]
            continue
        m = re.match(r"^(Ani_\w+):\s*$", l)
        if m:
            close()
            cur, entries, scripts, sc = m.group(1), [], {}, None
            continue
        m = re.match(r"^(\w+):\s+sonani\s+(\w+)", l)
        if m and cur:
            SYM[m.group(1)] = len(entries)
            entries.append(m.group(2))
            continue
        if re.match(r"^[A-Za-z_]\w*:", l) and not re.match(r"^[A-Za-z_]\w*:\s*dc\.b", l):
            close()
            cur, entries, scripts, sc = None, [], {}, None
            continue
        m = re.match(r"^\s*dc\.w\s+(\S+)-(\S+)", l)
        if m and cur and sc is None:
            entries.append(m.group(1))
            continue
        m = re.match(r"^([.A-Za-z_]\w*):\s*dc\.b\s+(.*)$", l)
        if m and cur:
            sc = m.group(1)
            scripts[sc] = [s1.number(v, SYM) & 0xFF for v in m.group(2).split(",")]
            continue
        m = re.match(r"^\s*dc\.b\s+(.*)$", l)
        if m and sc:
            scripts[sc] += [s1.number(v, SYM) & 0xFF for v in m.group(1).split(",")]
    close()
    return tables


ANIS = []
anim_files = [os.path.join("_anim", f) for f in sorted(os.listdir(os.path.join(REF, "_anim"))) if f.endswith(".asm")]
for f in anim_files:
    for label, scripts in anim_tables(f).items():
        b = bytearray(struct.pack("<H", len(scripts)))
        start = 2 + 2 * len(scripts)
        body = bytearray()
        offs = []
        for sc in scripts:
            offs.append(start + len(body))
            body += bytes(sc)
            if len(body) % 2:
                body.append(0)
        for o in offs:
            b += struct.pack("<H", o)
        ANIS.append((label, bytes(b + body)))


def table(prefix, items):
    """A table of offsets (from the start of data.bin) to each item, and the items."""
    offs = []
    for name, data in items:
        align(4)
        offs.append(len(blob))
        blob.extend(data)
    t = bytearray()
    for o in offs:
        t += struct.pack("<I", o)
    put("D_" + prefix, t)


table("MAPS", MAPS)
table("ANIS", ANIS)

# (the Caterkiller's own animation: a frame for each of 128 steps)
cat, on = bytearray(), False
for l in asm_lines("_incObj/78 Badnik - Caterkiller.asm"):
    if l.startswith("Ani_Cat:"):
        on = True
    elif on and re.match(r"^\s*dc\.b\s", l):
        cat += bytes(s1.number(v, SYM) & 0xFF for v in l.split("dc.b", 1)[1].split(","))
    elif on and l.strip() == "even":
        break
put("D_ANI_CAT", cat)

# --- Sonic's pattern load cues
dp = s1.dplcs(os.path.join(REF, "_maps", "Sonic - Dynamic Gfx Script.asm"))
b = bytearray(struct.pack("<H", len(dp)))
body, offs = bytearray(), []
for fr in dp:
    offs.append(2 + 2 * len(dp) + len(body))
    body += struct.pack("<BB", len(fr), 0)
    for tile, n in fr:
        body += struct.pack("<H", (n - 1) << 12 | tile)
for o in offs:
    b += struct.pack("<H", o)
put("D_SONICDPLC", b + body)

# --- levels
ZONES = ["GHZ", "LZ", "MZ", "SLZ", "SYZ", "SBZ"]
for z in ZONES:
    blocks = s1.enigma(rd(FILES["Blk16_" + z]))
    put("D_BLK16_" + z, struct.pack("<%dH" % len(blocks), *blocks))
    chunks = s1.kosinski(rd(FILES["Blk256_" + z]))
    words = struct.unpack(">%dH" % (len(chunks) // 2), chunks)
    # (the chunks the layouts use: the empty ones at the end are left out)
    n = len(words) // 256
    while n and not any(words[(n - 1) * 256:n * 256]):
        n -= 1
    defs.append(("N_BLK256_" + z, n))
    put("D_BLK256_" + z, struct.pack("<%dH" % (n * 256), *words[:n * 256]))

# (the collision indexes back to back, as in the original: a block past the end of one reads the next)
cols = bytearray()
for z in ZONES:
    defs.append(("D_COL_" + z, len(blob) + ((-len(blob)) % 4) + len(cols)))
    cols += rd(FILES["Col_" + z])
put("D_COLS", cols)
put("D_ANGLEMAP", rd(FILES["AngleMap"]))
put("D_COLLARRAY1", rd(FILES["CollArray1"]))
put("D_COLLARRAY2", rd(FILES["CollArray2"]))

for f in sorted(os.listdir(os.path.join(REF, "levels"))):
    if "REV00" in f:
        continue
    put("D_LEVEL_" + ident(f[:-4].replace(" (REV01)", "")).upper(), rd(os.path.join("levels", f)), 2)

for f in sorted(os.listdir(os.path.join(REF, "objpos"))):
    if not f.endswith(".bin") or "REV00" in f:
        continue
    d = rd(os.path.join("objpos", f))
    o = bytearray()
    for i in range(0, len(d) - 5, 6):
        x, y, idb, sub = struct.unpack(">HHBB", d[i:i + 6])
        o += struct.pack("<HHBB", x, y, idb, sub)
        if x == 0xFFFF:
            break
    o += struct.pack("<HHBB", 0xFFFF, 0, 0, 0)
    # (an end marker before the list too, as in the original: the object manager looks one entry back)
    at = put("OBJPOS_START_" + ident(f[:-4]).upper(), struct.pack("<HHBB", 0xFFFF, 0, 0, 0) + o, 2)
    defs.append(("D_OBJPOS_" + ident(f[:-4].replace(" (REV01)", "")).upper(), at + 6))

# the platforms on Labyrinth's and Scrap Brain's conveyors, in the order of ObjPosLZPlatform_Index and
# ObjPosSBZPlatform_Index: a count less one, then x, y and subtype words (little-endian here)
pf = ["lz1pf1", "lz1pf2", "lz2pf1", "lz2pf2", "lz3pf1", "lz3pf2", "lz1pf1", "lz1pf2",
      "sbz1pf1", "sbz1pf2", "sbz1pf3", "sbz1pf4", "sbz1pf5", "sbz1pf6", "sbz1pf1", "sbz1pf2"]
pf_at = {}
for name in sorted(set(pf)):
    d = rd(os.path.join("objpos", "platforms", name + ".bin"))
    n = struct.unpack(">H", d[:2])[0] + 1
    pf_at[name] = put("D_PF_" + name.upper(), struct.pack("<%dH" % (1 + n * 3), *struct.unpack(">%dH" % (1 + n * 3), d[:2 + n * 6])), 2)
put("D_PLATFORMS", struct.pack("<16I", *(pf_at[p] for p in pf)), 4)

for f in sorted(os.listdir(os.path.join(REF, "startpos"))):
    if f.endswith(".bin"):
        d = rd(os.path.join("startpos", f))
        put("D_START_" + ident(f[:-4]).upper(), struct.pack("<2H", *struct.unpack(">2H", d[:4])), 2)
for d_ in ("Credits Demos", "Special Stages"):
    p = os.path.join(REF, "startpos", d_)
    for f in sorted(os.listdir(p)):
        d = open(os.path.join(p, f), "rb").read()
        put("D_START_" + ident(d_ + " " + f[:-4]).upper(), struct.pack("<%dH" % (len(d) // 2), *struct.unpack(">%dH" % (len(d) // 2), d)), 2)

for f in sorted(os.listdir(os.path.join(REF, "palette"))):
    d = rd(os.path.join("palette", f))
    put("D_PAL_" + ident(f[:-4]).upper(), struct.pack("<%dH" % (len(d) // 2), *struct.unpack(">%dH" % (len(d) // 2), d)), 2)
    defs.append(("N_PAL_" + ident(f[:-4]).upper(), len(d) // 2))

# --- tile maps and special stages
for label, start in (("Eni_Title", 0), ("Eni_SegaLogo", 0), ("Eni_SSBg1", 0), ("Eni_SSBg2", 0), ("Eni_JapNames", 0)):
    w = s1.enigma(rd(FILES[label]), start)
    put("D_" + label.upper(), struct.pack("<%dH" % len(w), *w))
    defs.append(("N_" + label.upper(), len(w)))
for i in range(1, 7):
    w = s1.enigma(rd(FILES["SS_%d" % i]))
    # (the layouts are bytes: Enigma packs them as words)
    put("D_SS_%d" % i, bytes(v & 0xFF for v in w) if max(w) < 256 else struct.pack(">%dH" % len(w), *w))
    defs.append(("N_SS_%d" % i, len(w)))

# --- demos: (buttons, frames) pairs
BTN = {"U": 1, "D": 2, "L": 4, "R": 8, "B": 0x10, "C": 0x20, "A": 0x40, "S": 0x80}
for f in sorted(os.listdir(os.path.join(REF, "demodata"))):
    o = bytearray()
    for l in asm_lines(os.path.join("demodata", f)):
        m = re.match(r"^\s*demoinput\s+([A-Z]*)\s*,\s*(\S+)", l)
        if m:
            mask = 0
            for c in m.group(1):
                mask |= BTN[c]
            o += bytes([mask, (s1.number(m.group(2), SYM) - 1) & 0xFF])
            continue
        m = re.match(r"^\s*dc\.b\s+(.*)$", l)
        if m:
            o += bytes(s1.number(v, SYM) & 0xFF for v in m.group(1).split(","))
    put("D_DEMO_" + ident(f[:-4]).upper(), o, 2)

# --- pattern load cues: (art, first tile) lists, in the order of their ids
art_ids = {n: i for i, (n, _, _) in enumerate(ART)}
plc_order, plc_lists, cur, skip = [], {}, None, False
for l in asm_lines("_inc/Pattern Load Cues.asm"):
    m = re.match(r"^plcid_\w+:\s+plcptr\s+(\w+)", l)
    if m:
        plc_order.append(m.group(1))
        continue
    if re.match(r"^\s*if\s+\(Revision=0\)", l):
        skip = True
        continue
    if re.match(r"^\s*endif", l):
        skip = False
        continue
    m = re.match(r"^(PLC_\w+):\s*(plcheader)?", l)
    if m:
        cur = None if m.group(1).endswith("_end") else m.group(1)
        if cur:
            plc_lists[cur] = []
        continue
    m = re.match(r"^\s*plcm\s+(\w+)\s*,\s*([^;]+)", l)
    if m and cur and not skip:
        plc_lists[cur].append((art_ids[m.group(1)], s1.number(m.group(2), SYM)))
offs, body = [], bytearray()
for name in plc_order:
    offs.append(len(body))
    lst = plc_lists.get(name, [])
    body += struct.pack("<H", len(lst))
    for a, t in lst:
        body += struct.pack("<HH", a, t)
b = bytearray(struct.pack("<H", len(plc_order)))
for o in offs:
    b += struct.pack("<H", 2 + 2 * len(plc_order) + o)
put("D_PLC", b + body)

# --- tables of numbers written in the code: (file, label, b or w)
TABLES = [
    ("_incObj/sub CalcSine.asm", "Sine_Data", "w"),
    ("_incObj/sub CalcAngle.asm", "Angle_Data", "b"),
    ("_incObj/0A LZ Drowning Countdown.asm", "Drown_WobbleData", "b"),
]


def asm_table(path, label, size):
    vals, on, skip = [], False, False
    for l in asm_lines(path):
        if re.match(r"^\s*if\s+\(?Revision\s*=\s*0", l):
            skip = True
            continue
        if re.match(r"^\s*if\s+Revision\s*<>\s*0", l):
            continue
        if re.match(r"^\s*else\b", l):
            skip = not skip
            continue
        if re.match(r"^\s*endif\b", l):
            skip = False
            continue
        if skip:
            continue
        m = re.match(r"^([.A-Za-z_]\w*):\s*(.*)$", l)
        if m and m.group(1) == label:
            on = True
            l = m.group(2)
        elif on and re.match(r"^[.A-Za-z_]\w*:", l):
            break
        if not on:
            continue
        m = re.match(r"^\s*rept\s+(\S+)", l)
        if m:
            rept_at, rept_n = len(vals), s1.number(m.group(1), SYM)
            continue
        if re.match(r"^\s*endr\b", l):
            vals += vals[rept_at:] * (rept_n - 1)
            continue
        m = re.match(r"^\s*dc\.([bw])\s+(.*)$", l)
        if m:
            vals += [s1.number(v, SYM) for v in m.group(2).split(",")]
        elif re.match(r"^\s*dcb\.b\s+", l):
            n, v = [s1.number(x, SYM) for x in l.split("dcb.b", 1)[1].split(",")]
            vals += [v] * n
        elif l.strip() and l.strip() != "even":
            break
    if size == "b":
        return bytes(v & 0xFF for v in vals)
    return struct.pack("<%dh" % len(vals), *[(v + 0x8000) % 0x10000 - 0x8000 for v in vals])


for path, label, size in TABLES:
    put("D_TAB_" + label.upper(), asm_table(path, label, size), 2)

# ---------------------------------------------------------------- output
align(4)
open(os.path.join(OUT, "data.bin"), "wb").write(blob)
with open(os.path.join(OUT, "data.h"), "w") as h:
    h.write("/* made by tools/pack.py: what is where in data.bin */\n#ifndef S1_DATA_H\n#define S1_DATA_H\n")
    for n, v in defs:
        h.write(f"#define {n} {v}\n")
    for i, (n, first, count) in enumerate(ART):
        h.write(f"#define ART_{ident(n)} {i}\n")
    for i, (n, _) in enumerate(MAPS):
        h.write(f"#define MAP_{ident(n.split('_', 1)[1])} {i}\n")
    for i, (n, _) in enumerate(ANIS):
        h.write(f"#define ANI_{ident(n.split('_', 1)[1])} {i}\n")
    h.write(f"#define N_ARTS {len(ART)}\n#define N_MAPS {len(MAPS)}\n#define N_ANIS {len(ANIS)}\n#endif\n")

KEEP = re.compile(r"^(ArtTile_|id_(?!VBlank)|status_|sprite_|fr_|btn|bit[A-Z]|aniXFlip|aniYFlip|boss_)")
with open(os.path.join(OUT, "s1const.h"), "w") as h:
    h.write("/* made by tools/pack.py: the disassembly's names for its numbers */\n#ifndef S1_CONST_H\n#define S1_CONST_H\n")
    for n in sorted(SYM):
        if KEEP.match(n) and isinstance(SYM[n], int):
            h.write(f"#define {n} {SYM[n]}\n")
    h.write("#endif\n")
print(f"data.bin: {len(blob)} bytes ({len(pool) // 32} patterns, {len(MAPS)} mappings, {len(ANIS)} animations)")
