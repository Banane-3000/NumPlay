#!/usr/bin/env python3
"""Gathers the public material the pack is made from, into REF (not committed):

    python3 -I tools/fetch.py REF

- REF/br: Basement Renovator (github.com/Basement-Renovator/basement-renovator), the room editor of The Binding
  of Isaac's modders. Its resources hold the game's own pictures, as the editor shows them: every monster, boss,
  pickup, grid object and item (resources/Entities), and each floor's walls, rocks and pits (resources/Backgrounds)
- REF/fonts/upheavtt.ttf: Upheaval, by Brian Kent (freeware), the font of the game's titles

Files already there are kept: run it again to fetch only what is missing."""
import io
import os
import subprocess
import sys
import urllib.request
import zipfile

BR = "https://github.com/Basement-Renovator/basement-renovator"
BR_COMMIT = "c2ae4956764096238f47615face6d67325489cbc"
UPHEAVAL = "https://dl.dafont.com/dl/?f=upheaval"
UA = {"User-Agent": "NumPlay-Isaac/1.0 (fan port for the NumWorks calculator; one-time asset fetch)"}


def main():
    if len(sys.argv) != 2:
        sys.exit(__doc__)
    ref = sys.argv[1]
    os.makedirs(ref, exist_ok=True)
    br = os.path.join(ref, "br")
    if not os.path.isdir(os.path.join(br, "resources")):
        subprocess.run(["git", "clone", "-q", BR, br], check=True)
        subprocess.run(["git", "-C", br, "checkout", "-q", BR_COMMIT], check=True)
    fonts = os.path.join(ref, "fonts")
    os.makedirs(fonts, exist_ok=True)
    if not os.path.exists(os.path.join(fonts, "upheavtt.ttf")):
        data = urllib.request.urlopen(urllib.request.Request(UPHEAVAL, headers=UA), timeout=60).read()
        with zipfile.ZipFile(io.BytesIO(data)) as z:
            for n in ("upheavtt.ttf", "upheaval.txt"):
                open(os.path.join(fonts, n), "wb").write(z.read(n))
    print("REF ready:", ref)


if __name__ == "__main__":
    main()
