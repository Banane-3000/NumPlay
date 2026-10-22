#!/usr/bin/env python3
"""Retro-Go's ROM table: tools/romtable.py's tables read back as they were written, and damaged
tables (bad magic or version, counts too big, offsets and sizes out of range, misaligned, names
without an end or with broken UTF-8, cut short, random bytes) never read past the table nor crash.

usage: test_romtable.py HOST_ROMTABLE"""
import os
import random
import struct
import subprocess
import sys
import tempfile

here = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(here, "..", "..", "..", "tools"))
import romtable  # noqa: E402

reader = sys.argv[1]
tmp = tempfile.mkdtemp()
failures = []


def read(blob):
    path = os.path.join(tmp, "t.bin")
    open(path, "wb").write(blob)
    r = subprocess.run([reader, path], capture_output=True, text=True, timeout=60)
    if r.returncode != 0:
        failures.append(f"crash ({r.returncode}): {r.stderr[-400:]}")
        return None
    lines = r.stdout.splitlines()
    return int(lines[0]), [line.split(" ", 3) for line in lines[1:]]


def expect(what, ok):
    print(("ok   " if ok else "FAIL ") + what)
    if not ok:
        failures.append(what)


def roms_file(name, data):
    p = os.path.join(tmp, name)
    open(p, "wb").write(data)
    return p


rng = random.Random(7)
# ---- written, then read back
empty = romtable.table([])
expect("the empty table: 12 bytes, no game", len(empty) == 12 and read(empty) == (0, []))
games = [roms_file("Zelda Like.gb", bytes(rng.getrandbits(8) for _ in range(0x8000))),
         roms_file("alpha.nes", b"NES\x1a" + bytes(rng.getrandbits(8) for _ in range(0x6000 - 4 + 7))),
         roms_file("Colour.gbc", bytes(rng.getrandbits(8) for _ in range(0x10001))),
         roms_file("Only Color.gb", bytes(0x143) + b"\xc0" + bytes(0x8000 - 0x144)),
         roms_file("Ünïcödé name that is far too long for forty bytes.gb", bytes(0x8000))]
blob = romtable.table(games)
got = read(blob)
expect("five games read back", got is not None and got[0] == 5)
if got:
    systems = [g[0] for g in got[1]]
    names = [g[3] for g in got[1]]
    expect("sorted by system, then name", systems == ["1", "1", "1", "2", "3"] and
           names[0] == "Only Color" and names[1] == "Zelda Like")
    expect("names: ASCII, '?' for other letters, cut short", names[2].startswith("?n?c?d? name") and
           len(names[2]) <= 40)
# the layout, rebuilt here the way the website builds it: offsets, padding, names, the end
ref = bytearray(struct.pack("<III", romtable.MAGIC, 1, len(games)) + bytes(52 * len(games)))
for i, p in enumerate(games):
    data = open(p, "rb").read()
    ref += bytes(-len(ref) % 4)
    nm = os.path.splitext(os.path.basename(p))[0].encode()
    k = min(len(nm), 40)
    while k < len(nm) and (nm[k] & 0xC0) == 0x80:
        k -= 1
    ext = os.path.splitext(p)[1].lower()
    struct.pack_into("<IIB3x40s", ref, 12 + 52 * i, len(ref), len(data),
                     {".gb": 1, ".gbc": 2, ".nes": 3}[ext], nm[:k].ljust(40, b"\0"))
    ref += data
ref += bytes(-len(ref) % 4)
expect("the same bytes as the layout's rules", bytes(ref) == blob)


def section(obj, wanted):
    """the bytes and flags of an ELF32 section, and the np_roms symbol's value and size"""
    shoff, = struct.unpack_from("<I", obj, 0x20)
    shnum, shstrndx = struct.unpack_from("<HH", obj, 0x30)
    secs = [struct.unpack_from("<10I", obj, shoff + 40 * i) for i in range(shnum)]
    names = secs[shstrndx]
    sym = None
    for s in secs:
        if s[1] == 2:  # the symbol table
            strtab = secs[s[6]]
            for k in range(s[5] // 16):
                n, value, size, info, _, shndx = struct.unpack_from("<IIIBBH", obj, s[4] + 16 * k)
                nm = obj[strtab[4] + n:obj.index(b"\0", strtab[4] + n)]
                if nm == b"np_roms":
                    sym = (value, size, info, shndx)
    for i, s in enumerate(secs):
        nm = obj[names[4] + s[0]:obj.index(b"\0", names[4] + s[0])]
        if nm == wanted:
            return obj[s[4]:s[4] + s[5]], s[2], s[8], i, sym
    return None


# the object, for two ROMs, one with a long accented name: its section is the table, byte for byte
# (to compare with the website: "alpha.nes" is NES\x1a then 0x6003 zero bytes, 24583 bytes in all;
# the .gb is the bytes 0 to 255 over and over, 33280 bytes)
two = [roms_file("alpha.nes", b"NES\x1a" + bytes(0x6003)),
       roms_file("Légende de l'Été à Montréal, édition complète.gb", bytes(range(256)) * 130)]
obj_path = os.path.join(tmp, "roms.o")
subprocess.run([sys.executable, os.path.join(here, "..", "..", "..", "tools", "romtable.py"), "-o", obj_path] + two,
               check=True)
found = section(open(obj_path, "rb").read(), b".rodata.np_roms")
ref2 = bytearray(struct.pack("<III", romtable.MAGIC, 1, 2) + bytes(104))
for i, p in enumerate(two):
    data = open(p, "rb").read()
    ref2 += bytes(-len(ref2) % 4)
    nm = os.path.splitext(os.path.basename(p))[0].encode()
    k = min(len(nm), 40)
    while k < len(nm) and (nm[k] & 0xC0) == 0x80:
        k -= 1
    struct.pack_into("<IIB3x40s", ref2, 12 + 52 * i, len(ref2), len(data), (3, 1)[i], nm[:k].ljust(40, b"\0"))
    ref2 += data
ref2 += bytes(-len(ref2) % 4)
expect("the object's .rodata.np_roms is the table, byte for byte", found is not None and found[0] == bytes(ref2))
if found:
    data, flags, align, index, sym = found
    expect("read-only, 4-byte aligned", flags == 2 and align == 4)
    expect("np_roms: global, at its start, its size", sym == (0, len(ref2), 0x11, index))
    import hashlib
    print("     two-ROM table:", len(ref2), "bytes, sha1", hashlib.sha1(bytes(ref2)).hexdigest())
long_name = roms_file("A" * 40 + ".nes", b"NES\x1a" + bytes(0x6000))
got = read(romtable.table([long_name]))
expect("a name of all 40 bytes, without an ending zero", got is not None and got[1][0][3] == "A" * 40)
# every ROM starts 4-byte aligned, inside the table
count = struct.unpack_from("<I", blob, 8)[0]
for i in range(count):
    at, size = struct.unpack_from("<II", blob, 12 + 52 * i)
    expect(f"game {i}: aligned and inside", at % 4 == 0 and at + size <= len(blob))

# ---- damaged tables
def entry(at, size, system, name=b"x"):
    return struct.pack("<IIB3x", at, size, system) + name.ljust(40, b"\0")[:40]


head = lambda n, magic=romtable.MAGIC, version=1: struct.pack("<III", magic, version, n)  # noqa: E731
cases = {
    "bad magic": head(0, 0x12345678),
    "bad version": head(0, version=2),
    "cut inside the header": head(1)[:7],
    "count far too big": head(0xFFFFFFFF),
    "count bigger than the entries": head(3) + entry(64, 4, 1),
    "offset past the end": head(1) + entry(0x10000, 4, 1) + bytes(8),
    "offset inside the entries": head(1) + entry(12, 8, 1) + bytes(16),
    "offset not aligned": head(1) + entry(66, 2, 1) + bytes(8),
    "size past the end": head(1) + entry(64, 0x7FFFFFFF, 1) + bytes(8),
    "offset and size wrapping around": head(1) + entry(64, 0xFFFFFFF0, 1) + bytes(8),
    "size zero": head(1) + entry(64, 0, 1) + bytes(8),
    "system zero": head(1) + entry(64, 4, 0) + bytes(8),
    "system unknown": head(1) + entry(64, 4, 9) + bytes(8),
    "name with no end": head(1) + entry(64, 4, 3, b"N" * 40) + bytes(8),
    "name of broken UTF-8": head(1) + entry(64, 4, 3, b"\xff\xfe\x80\xc3") + bytes(8),
    "name of control letters": head(1) + entry(64, 4, 3, b"\x01\x02\x7f\t") + bytes(8),
}
for what, blob in cases.items():
    got = read(blob)
    if got is None:
        expect(what + ": no crash", False)
        continue
    n, entries = got
    ok = n <= 1 and all(int(e[1]) > 0 for e in entries)
    if what in ("bad magic", "bad version", "cut inside the header"):
        ok = n == -1
    if what in ("name with no end", "name of broken UTF-8", "name of control letters"):
        ok = n == 1 and all(32 <= ord(c) < 127 for c in entries[0][3]) and len(entries[0][3]) <= 40
    expect(what, ok)
# random damage to a good table, and random bytes after a good header
good = romtable.table(games)
for k in range(300):
    b = bytearray(good)
    for _ in range(rng.randint(1, 12)):
        i = rng.randrange(0, min(len(b), 12 + 52 * 5 + 8))
        b[i] = rng.getrandbits(8)
    if rng.random() < 0.3:
        b = b[:rng.randrange(0, len(b))]
    if read(bytes(b)) is None:
        break
for k in range(200):
    blob = head(rng.randint(0, 200)) + bytes(rng.getrandbits(8) for _ in range(rng.randint(0, 2000)))
    if read(blob) is None:
        break
expect("500 damaged tables: no crash, nothing read past the end", not any("crash" in f for f in failures))

if failures:
    print(f"{len(failures)} failures")
    sys.exit(1)
print("all good")
