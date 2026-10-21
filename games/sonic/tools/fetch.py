#!/usr/bin/env python3
"""Gathers what the pack is made from, into REF (not committed): the Sonic Retro disassembly of
Sonic the Hedgehog (github.com/sonicretro/s1disasm), at the commit the game was made from.

    python3 -I tools/fetch.py REF
    make data REF=REF/s1disasm

It holds the game's art, mappings, animations, levels, object placement, palettes and demos as the
disassembly keeps them; tools/pack.py reads them and the assembly source's tables."""
import io
import os
import shutil
import sys
import urllib.request
import zipfile

REPO = "sonicretro/s1disasm"
COMMIT = "15dfa3a6ee4c53aeb482233e493755c84b4f3479"


def main():
    if len(sys.argv) != 2:
        sys.exit(__doc__)
    ref = sys.argv[1]
    dest = os.path.join(ref, "s1disasm")
    if os.path.exists(os.path.join(dest, "sonic.asm")):
        print(f"{dest}: already there")
        return
    url = f"https://codeload.github.com/{REPO}/zip/{COMMIT}"
    print(f"fetching {url}")
    data = urllib.request.urlopen(url, timeout=300).read()
    tmp = dest + ".part"
    shutil.rmtree(tmp, ignore_errors=True)
    with zipfile.ZipFile(io.BytesIO(data)) as z:
        z.extractall(tmp)
    top = os.path.join(tmp, os.listdir(tmp)[0])
    os.makedirs(ref, exist_ok=True)
    shutil.move(top, dest)
    shutil.rmtree(tmp, ignore_errors=True)
    print(f"{dest}: ready")


if __name__ == "__main__":
    main()
