#!/usr/bin/env python3
"""The README's downloads, stars and version badges, as shields.io endpoint files.

shields.io's own GitHub badges ask GitHub through a pool of tokens that runs dry ("invalid"), so the
site carries the numbers instead: the Pages workflow (.github/workflows/pages.yml) runs this with
GitHub's numbers before it builds the site, and the badges read docs/badges/*.json from there.

Usage: badges.py --downloads N --stars N --version v1.11.0 --out DIR
"""
import argparse
import json
import os

COLORS = {"downloads": "1E7F74", "stars": "E3B341", "version": "2D9F4F"}


def short(n):
    """As shields.io writes big numbers: 999, 8.1k, 22k, 1.2M."""
    if n >= 1000000:
        return f"{n // 100000 / 10:g}M"
    if n >= 10000:
        return f"{n // 1000}k"
    if n >= 1000:
        return f"{n // 100 / 10:g}k"
    return str(n)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--downloads", type=int, required=True)
    ap.add_argument("--stars", type=int, required=True)
    ap.add_argument("--version", required=True)
    ap.add_argument("--out", required=True)
    a = ap.parse_args()
    os.makedirs(a.out, exist_ok=True)
    for label, message in (("downloads", short(a.downloads)), ("stars", str(a.stars)), ("version", a.version)):
        with open(os.path.join(a.out, f"{label}.json"), "w") as f:
            json.dump({"schemaVersion": 1, "label": label, "message": message, "color": COLORS[label]}, f)
            f.write("\n")


if __name__ == "__main__":
    main()
