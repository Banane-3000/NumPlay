#!/usr/bin/env python3
"""Downloads the shareware DOOM1.WAD (version 1.9, which id Software lets anyone copy) from Ubuntu's
doom-wad-shareware package, checks it is the one, and writes it to OUT/doom1.wad. Then:

    make data WAD=OUT/doom1.wad

packs it into src/doom1.whx (RP2040 Doom's format, read in place by the game; no sound or music).
Needs dpkg-deb, or the zstd command, to open the package.

Usage: fetch.py OUT"""
import hashlib
import io
import os
import shutil
import subprocess
import sys
import tarfile
import tempfile
import urllib.request

URL = "http://archive.ubuntu.com/ubuntu/pool/multiverse/d/doom-wad-shareware/doom-wad-shareware_1.9.fixed-4_all.deb"
MD5 = "f0cefca49926d00903cf57551d901abe"  # the 1.9 shareware DOOM1.WAD
INSIDE = "usr/share/games/doom/doom1.wad"


def ar_members(data):
    """The files of a Debian package (an ar archive)."""
    assert data[:8] == b"!<arch>\n", "not a .deb"
    p, out = 8, {}
    while p + 60 <= len(data):
        name = data[p:p + 16].decode().strip().rstrip("/")
        size = int(data[p + 48:p + 58].decode().strip())
        out[name] = data[p + 60:p + 60 + size]
        p += 60 + size + (size & 1)
    return out


def main():
    out = sys.argv[1] if len(sys.argv) > 1 else "build/ref"
    os.makedirs(out, exist_ok=True)
    deb = urllib.request.urlopen(URL).read()
    with tempfile.TemporaryDirectory() as d:
        path = os.path.join(d, "doom-wad-shareware.deb")
        open(path, "wb").write(deb)
        if shutil.which("dpkg-deb"):
            subprocess.run(["dpkg-deb", "-x", path, d], check=True)
            wad = open(os.path.join(d, INSIDE), "rb").read()
        else:
            members = ar_members(deb)
            name = next(n for n in members if n.startswith("data.tar"))
            raw = members[name]
            if name.endswith(".zst"):
                raw = subprocess.run(["zstd", "-d", "-c"], input=raw, capture_output=True, check=True).stdout
            with tarfile.open(fileobj=io.BytesIO(raw)) as t:
                wad = t.extractfile("./" + INSIDE).read()
    if hashlib.md5(wad).hexdigest() != MD5:
        raise SystemExit("fetch: this is not the shareware DOOM1.WAD 1.9")
    open(os.path.join(out, "doom1.wad"), "wb").write(wad)
    print(f"{out}/doom1.wad: {len(wad)} bytes")


if __name__ == "__main__":
    main()
