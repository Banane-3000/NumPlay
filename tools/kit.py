#!/usr/bin/env python3
"""Packs the kit the customizer website links a NumPlay of your own from (`make kit`).

The website (site/) runs GNU ld in the browser on the same objects the Makefile links
NumPlay.nwa from: base.o (the launcher), arena.o (the RAM the games share), an app name and
icon, and one object per game, with a script made of script.json's parts (tools/gen_games.py).
This copies them to OUT, gzipped, with kit.json (the games, their sizes, colours and
limits), each game's icon and thumbnail, and checks that linking the default games from OUT
gives NumPlay.nwa's bytes on the calculator.

Usage: kit.py games.json BUILD OUT --version V --variants "..." --default "ids" --upsilon "ids"
              --app-space N --ram N --upsilon-ram N --nwlink NWLINK
"""
import argparse
import gzip
import json
import os
import re
import shutil
import struct
import subprocess
import sys
import tempfile

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
SHF_ALLOC = 2
SHT_NOBITS = 8
LINK = ["-r", "--gc-sections", "-z", "noexecstack", "-e", "main",
        "-u", "eadk_app_name", "-u", "eadk_app_icon", "-u", "eadk_api_level"]


def sections(path):
    """(name, type, flags, size, align) of each section of an ELF object."""
    d = open(path, "rb").read()
    shoff, = struct.unpack_from("<I", d, 0x20)
    shentsize, shnum, shstrndx = struct.unpack_from("<HHH", d, 0x2E)
    hdr = [struct.unpack_from("<10I", d, shoff + i * shentsize) for i in range(shnum)]
    strtab = hdr[shstrndx]
    names = d[strtab[4]:strtab[4] + strtab[5]]
    out = []
    for h in hdr:
        n = names[h[0]:names.index(b"\0", h[0])].decode()
        out.append((n, h[1], h[2], h[5], max(h[8], 1)))
    return out


def installed_size(nwa):
    """Bytes the installer writes to flash for an .nwa: its 32 byte header, then its sections
    in the order of nwlink's script (the name, the icon, code, read-only data, the data image)."""
    secs = [s for s in sections(nwa) if s[2] & SHF_ALLOC and s[1] != SHT_NOBITS]
    def group(s):
        n = s[0]
        if n == ".rodata.eadk_app_name": return 0
        if n == ".rodata.eadk_app_icon": return 1
        if n == ".text" or n.startswith(".text."): return 2
        if n == ".rodata" or n.startswith(".rodata."): return 3 if n != ".rodata.eadk_api_level" else -1
        if n == ".data" or n.startswith(".data."): return 4
        return 3
    at = 32
    for g in range(5):
        if g in (2, 4): at = (at + 3) & ~3
        for s in secs:
            if group(s) == g:
                at = (at + s[4] - 1) & ~(s[4] - 1)
                at += s[3]
    return at


def icon_of(gid):
    for p in ("src/icon.png", "assets/icon.png", "tetris/src/data/icon.png", "docs/icon.png"):
        f = os.path.join(ROOT, "games", gid, p)
        if os.path.exists(f):
            return f
    return None


def gz(src, dst):
    with open(src, "rb") as f, open(dst, "wb") as g:
        g.write(gzip.compress(f.read(), 9, mtime=0))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("manifest")
    ap.add_argument("build")
    ap.add_argument("out")
    ap.add_argument("--version", required=True)
    ap.add_argument("--variants", required=True)
    ap.add_argument("--default", required=True)
    ap.add_argument("--upsilon", default="")
    ap.add_argument("--app-space", type=int, required=True)
    ap.add_argument("--ram", type=int, required=True)
    ap.add_argument("--upsilon-ram", type=int, required=True)
    ap.add_argument("--nwlink", default="nwlink")
    ap.add_argument("--langs", default="", help="the languages make kit-langs adds (site-kit/<code>)")
    a = ap.parse_args()
    B, O = a.build, a.out
    manifest = json.load(open(a.manifest))
    index = {g["id"]: n for n, g in enumerate(manifest, 1)}
    default, upsilon = a.default.split(), set(a.upsilon.split())
    script = json.load(open(os.path.join(B, "gen", "script.json")))
    if os.path.exists(O):
        shutil.rmtree(O)
    for d in ("games", "icons", "thumbs", "variants"):
        os.makedirs(os.path.join(O, d))

    # ---- the objects, stripped of what no link needs, and gzipped
    tmp = tempfile.mkdtemp()
    def put(src, rel):
        t = os.path.join(tmp, os.path.basename(rel))
        subprocess.run(["arm-none-eabi-strip", "--strip-unneeded", src, "-o", t], check=True)
        gz(t, os.path.join(O, rel + ".gz"))
        return t
    local = {"base.o": put(os.path.join(B, "kit", "base.o"), "base.o"),
             "arena.o": put(os.path.join(B, "arm", "arena.o"), "arena.o")}
    variants = []
    for v in a.variants.split():
        for part in ("name", "icon"):
            local[f"{v}-{part}.o"] = put(os.path.join(B, "variant", f"{v}-{part}.o"), f"variants/{v}-{part}.o")
        png = "icon.png" if v == "NumPlay" else "icon-" + v.split("-", 1)[1].lower() + ".png"
        shutil.copy(os.path.join(ROOT, "launcher", "assets", png), os.path.join(O, "variants", v + ".png"))
        variants.append(v)

    games = []
    for g in manifest:
        i = g["id"]
        obj = os.path.join(B, "kit", "games", i + ".o")
        if not os.path.exists(obj):
            continue
        local[i] = put(obj, f"games/{i}.o")
        n = index[i]
        flash = 0
        for s in sections(obj):
            if s[2] & SHF_ALLOC and s[1] != SHT_NOBITS:
                flash = (flash + s[4] - 1) & ~(s[4] - 1)
                flash += s[3]
        mod = json.load(open(os.path.join(B, "modules", i + ".json")))
        ram = ((mod["data"] + 7) & ~7) + mod["bss"]
        icon = icon_of(i)
        if icon:
            shutil.copy(icon, os.path.join(O, "icons", i + ".png"))
        preview = os.path.exists(os.path.join(ROOT, "docs", "media", "gallery", f"{i}.gif"))
        thumb = os.path.join(ROOT, "docs", "media", f"thumb_{i}.png")
        if os.path.exists(thumb):
            shutil.copy(thumb, os.path.join(O, "thumbs", i + ".png"))
        games.append({
            "id": i, "index": n, "title": g["title"], "tagline": g["tagline"], "credit": g.get("credit"),
            "top": g["top"], "bottom": g["bottom"], "accent": g["accent"],
            "flash": flash, "ram": ram, "default": i in default, "upsilon": i in upsilon,
            "roms": bool(g.get("roms")), "systems": g.get("systems"), "xl": bool(g.get("xl")),
            "bytes": os.path.getsize(os.path.join(O, "games", i + ".o.gz")),
            "icon": bool(icon), "thumb": os.path.exists(thumb), "preview": preview})

    # ---- check: the default games, linked from the kit, are NumPlay.nwa on the calculator
    ld_script = os.path.join(tmp, "custom.ld")
    open(ld_script, "w").write(script["head"] + "".join(script["blocks"][i] for i in default) + script["tail"])
    nwa = os.path.join(tmp, "custom.nwa")
    subprocess.run(["arm-none-eabi-ld"] + LINK + ["-T", ld_script, local["base.o"], local["arena.o"],
                    local["NumPlay-name.o"], local["NumPlay-icon.o"]] + [local[i] for i in default] +
                   ["-o", nwa], check=True)
    subprocess.run(["arm-none-eabi-strip", "--strip-unneeded", nwa], check=True)
    # only what the installer gives may be left undefined (a game's weak `numplay_launcher`, say,
    # must find the launcher's)
    nm = subprocess.run(["arm-none-eabi-nm", "-u", nwa], capture_output=True, text=True, check=True).stdout
    loose = [l.split()[-1] for l in nm.splitlines()
             if not re.match(r"(eadk_|_heap_|_data_section_|_bss_section_|_sbrk$)", l.split()[-1])
             and l.split() not in (["w", "__sf"], ["w", "_printf_float"])]  # (newlib's own weak hooks)
    if loose:
        sys.exit(f"kit: left undefined: {' '.join(loose)}")
    ram = 148928
    bins = []
    for f in (nwa, os.path.join(B, "NumPlay.nwa")):
        b = f + ".bin" if f == nwa else os.path.join(tmp, "make.bin")
        subprocess.run([a.nwlink, "nwa-bin", "--ram-length", str(ram), f, b], check=True,
                       stdout=subprocess.DEVNULL)
        bins.append(open(b, "rb").read())
    if bins[0] != bins[1]:
        sys.exit("kit: the default games linked from the kit are not NumPlay.nwa")
    # the installer adds its own start code and EADK calls: the same few hundred bytes for any games
    runtime = len(bins[0]) - installed_size(nwa)
    if not 0 <= runtime < 4096:
        sys.exit(f"kit: installed_size says {installed_size(nwa)}, nwlink {len(bins[0])}")
    base = len(bins[0]) - sum(g["flash"] for g in games if g["default"])

    kit = {"version": a.version, "appSpace": a.app_space, "ram": a.ram, "upsilonRam": a.upsilon_ram,
           "arena": int(re.search(r"\.space (\d+)", open(os.path.join(B, "gen", "arena.s")).read()).group(1)),
           "base": base, "runtime": runtime, "numplay": len(bins[0]), "link": LINK, "variants": variants,
           "langs": ["en"] + a.langs.split(), "script": script, "games": games}
    json.dump(kit, open(os.path.join(O, "kit.json"), "w"), indent=1)
    shutil.rmtree(tmp)
    print(f"kit: {len(games)} games, NumPlay.nwa's {len(bins[0])} bytes linked again from {O}")


if __name__ == "__main__":
    main()
