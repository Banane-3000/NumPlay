#!/usr/bin/env python3
"""Five Nights at Freddy's: gathers the public material tools/pack.py turns into src/data.bin.

    python3 tools/fetch.py REF        (then: make data REF=REF)

The game's own pictures, as two fan ports keep them in their repositories, at the commits used:
- wellsousaaa's Five Nights at Freddy's Web (github.com/souwell/Five-Nights-at-Freddys-Web): the office and its
  lights, the cameras, the jumpscares, Foxy's run, the custom night's faces and the camera map;
- BaranMuzu's HX-NAF (github.com/BaranMuzu/HX-NAF): the title's faces, words and star, and the newspaper.
Both are cloned with git into REF."""
import os
import subprocess
import sys

SOURCES = [
    ("web", "https://github.com/souwell/Five-Nights-at-Freddys-Web", "6573cf1679e3ea62f1aa5e779bb3caaa627c8491"),
    ("hxnaf", "https://github.com/BaranMuzu/HX-NAF", "85c6b74c45ef419564eea94ae381d25ecddd60ff"),
]


def main():
    if len(sys.argv) != 2:
        raise SystemExit(__doc__)
    ref = sys.argv[1]
    os.makedirs(ref, exist_ok=True)
    for name, url, commit in SOURCES:
        d = os.path.join(ref, name)
        if not os.path.isdir(os.path.join(d, ".git")):
            subprocess.run(["git", "clone", "--quiet", url, d], check=True)
        subprocess.run(["git", "-C", d, "checkout", "--quiet", commit], check=True)
        print(f"{name}: {commit[:10]}")


if __name__ == "__main__":
    main()
