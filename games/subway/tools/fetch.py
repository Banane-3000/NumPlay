#!/usr/bin/env python3
"""Gathers the public pictures Subway Surfers is drawn from, for tools/pack.py: from the Subway Surfers
Wiki (subwaysurf.fandom.com), the game's own 3D models of Jake, Tricky, Fresh, Spike and the Inspector seen
from the front (tools/models.py fits the meshes to them and lays them on as their textures), the
logo, and the icons of coins, keys, power-ups and items. Nothing here is committed: pack.py turns them into
src/data.c, which is.

Usage: fetch.py DIR"""
import hashlib
import os
import sys
import urllib.parse
import urllib.request

FILES = [
    # the people, from the front (models.py)
    "3D_Models_5_Jakes.jpg", "3D_Models_5_Trickys.jpg", "3D_Models_Cathy_and_Fresh.jpg",
    "3D_Models_Amira_and_Spike.jpg", "3D_Models_Ballerina_Tricky_and_Guard_King.jpg",
    # the logo
    "Logo_001.png",
    # icons and pickups
    "Coin.png", "Key.png", "2x_Multiplier.png", "Coin_Magnet.png", "Jetpack.png",
    "Mystery_Box.png", "Hoverboard.png", "Headstart.png", "Score_Booster.png",
]


def url(name):
    """MediaWiki keeps a file under md5(name)[0]/md5(name)[:2]/name."""
    n = name.replace(" ", "_")
    h = hashlib.md5(n.encode()).hexdigest()
    return f"https://static.wikia.nocookie.net/subwaysurf/images/{h[0]}/{h[:2]}/{urllib.parse.quote(n)}"


def main(out):
    os.makedirs(out, exist_ok=True)
    for name in FILES:
        dst = os.path.join(out, name)
        if os.path.exists(dst):
            continue
        req = urllib.request.Request(url(name), headers={"User-Agent": "Mozilla/5.0"})
        data = urllib.request.urlopen(req, timeout=60).read()
        with open(dst, "wb") as f:
            f.write(data)
        print(f"{name}: {len(data)} bytes")


if __name__ == "__main__":
    main(sys.argv[1] if len(sys.argv) > 1 else os.path.join(os.path.dirname(os.path.abspath(__file__)), "ref"))
