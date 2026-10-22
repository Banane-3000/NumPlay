#!/usr/bin/env python3
"""Gathers the public material the pack is made from, into REF (not committed):

    python3 -I tools/fetch.py REF

- REF/wiki/*.png: the game's own pictures, as the Hill Climb Racing Wiki (hillclimbracing.fandom.com) keeps them:
  the vehicles' chassis and wheels, Bill, the coins and fuel, the stages' sky, surface and ground, their icons,
  the vehicles' and upgrades' icons, the pedals, the meters and the menus
- REF/game/*.png: the pictures the wiki doesn't keep (the Tractor, Tourist Bus, Super Diesel 4x4 and Truck, the
  bus' passengers, the Mountain's, Mudpool's and Volcano's sky, ground and icon, mud, lava and fire), from the game's
  own files: the assets of its Android package 1.10.2, as archive.org keeps it (REF/game/hcr-1.10.2.apk, only read)
- REF/fonts/Teko.ttf: Teko (SIL Open Font License), the closest free font to the game's own

Files already there are kept: run it again to fetch only what is missing."""
import hashlib
import json
import os
import sys
import time
import urllib.parse
import urllib.request
import zipfile

API = "https://hillclimbracing.fandom.com/api.php"
UA = {"User-Agent": "NumPlay-HillClimb/1.0 (fan port for the NumWorks calculator; one-time asset fetch)"}
FONT = ("Teko.ttf", "https://raw.githubusercontent.com/google/fonts/main/ofl/teko/Teko%5Bwght%5D.ttf")
GAME_APK = ("hcr-1.10.2.apk", "https://archive.org/download/hill-climb-racing-v.-1.10.2/Hill%20Climb%20Racing%20v.1.10.2%20MOD.apk",
            "3f109505b18b4c707b9fd6570b2eeebb")
GAME_FILES = [
    "car-tractor.png", "car-tractor-tire.png", "car-tractor-exhaust.png", "car-tractor-steering-wheel.png",
    "car-touristbus.png", "passanger1-head.png", "passanger2-head.png", "passanger3-head.png", "car-pickup.png",
    "car-truck.png", "sky-bg-cliffs.png", "sky-bg-mud.png", "sky-bg-volcano.png", "lowres-terrain-cliffs-ground.png",
    "lowres-terrain-mud.png", "lowres-terrain-mud-surface.png", "lowres-terrain-volcano.png",
    "lowres-terrain-volcano-surface.png", "stage-icon-cliffs.png", "stage-icon-mud.png", "stage-icon-volcano.png",
    "mud.png", "lava.png", "fire.png", "dirt-mud.png",
]

FILES = [
    # the vehicles: chassis, wheels, parts
    "Chassis-hillclimber-red.png", "Common_Wheel_-_Type_1.png",
    "Bike-motocross.png", "Bike-motocross-tire.png", "Bike-driver-leg-full.png",
    "Car-monster.png", "Car-monster-tire.png",
    "Car-quadbike.png", "Common_Wheel_-_Type_4.png",
    "Car-racecar.png", "Common_Wheel_-_Type_2.png",
    "Car-rallycar.png", "Common_Wheel_-_Type_8.png",
    "Dune_Buggy_Chassis_(Sprite).png", "Dune_Buggy_(Sprite).png",
    "Car-tank-bg.png", "Car-tank-fg.png", "Car-tank-cannon.png", "Car-tank-tire.png", "Chaintrack.png",
    # Bill
    "Driver-head.png", "Driver-body.png",
    # on the way
    "Coin5.png", "Coin25.png", "Coin100.png", "Coin500.png", "Fuel-canister.png", "Sky-cloud.png",
    "Lowres-tree-part1.png", "Lowres-tree-part2.png", "BridgeWoodBlock.png", "Flag.png", "Rocks.png", "Rocks-mars.png",
    "Iceshard.png", "Smoke.png", "Dirt.png", "Snow.png", "Countryside_Particle.png", "Alien-slime.png",
    # the stages: sky, surface, ground, icon
    "Sky-bg.png", "Sky-bg-cave.png", "Sky-bg-forest.png", "Sky-bg-mars.png", "Sky-bg-arctic-cave.png",
    "Highway-sky-bg.png", "Moon-bg.png",
    "Lowres-terrain-surface.png", "Lowres-terrain-ground.png", "Lowres-desert-surface.png",
    "Lowres-desert-ground.png", "Lowres-arctic-surface.png", "Lowres-arctic-ground.png",
    "Lowres-highway-surface.png", "Lowres-highway-ground.png", "Lowres-terrain-cave-surface.png",
    "Lowres-moon-surface.png", "Lowres-moon-ground.png", "Lowres-terrain-mars-surface.png", "Lowres-mars-ground.png",
    "Lowres-terrain-alien-surface.png", "Lowres-terrain-alien.png", "Lowres-terrain-forest-surface.png",
    "Lowres-terrain-forest-ground.png",
    "Stage-icon-countryside.png", "Stage-icon-desert.png", "Stage-icon-arctic.png", "Stage-icon-highway.png",
    "Stage-icon-cave.png", "Stage-icon-moon.png", "Stage-icon-mars.png", "Stage-icon-alienplanet.png",
    "Stage-icon-arctic-cave.png", "Stage-icon-forest.png", "Stage-icon-locked.png",
    # the menus
    "Mission-vehicle-jeep.png", "Mission-vehicle-bike.png", "Mission-vehicle-monster.png",
    "Mission-vehicle-quadbike.png", "Mission-vehicle-racecar.png", "Mission-vehicle-rallycar.png",
    "Mission-vehicle-dunebuggy.png", "Mission-vehicle-tank.png", "Mission-vehicle-tractor.png",
    "Mission-vehicle-touristbus.png", "Mission-vehicle-dieseltruck.png", "Mission-vehicle-snowmobile.png",
    "Mission-vehicle-truck.png", "Car-snowmobile-body.png", "Car-snowmobile-ski.png",
    "MainMenuUpgradeEngine.png", "MainMenuUpgradeSuspension.png", "MainMenuUpgradeTires.png",
    "MainMenuUpgradeAWD.png", "MainMenuUpgradeBikeRotation.png", "MainMenuUpgradeDownforce.png",
    "MainMenuUpgradeFuel.png",
    "Pedal-gas-normal.png", "Pedal-gas-pressed.png", "Pedal-brake-normal.png", "Pedal-brake-pressed.png",
    "Meter-rpm.png", "Meter-boost.png", "Meter-needle.png", "Fuel-icon.png", "Fuel-warning.png", "Coin-icon.png",
    "Menu-bg.png", "Game-logo.png", "Lock.png", "Icon-start.png", "Distance-icon.png",
]


def get(url):
    for attempt in range(4):
        try:
            return urllib.request.urlopen(urllib.request.Request(url, headers=UA), timeout=60).read()
        except Exception:
            if attempt == 3:
                raise
            time.sleep(2 ** attempt)


def main():
    ref = sys.argv[1]
    wiki = os.path.join(ref, "wiki")
    os.makedirs(wiki, exist_ok=True)
    todo = [n for n in FILES if not os.path.exists(os.path.join(wiki, n))]
    for i in range(0, len(todo), 40):
        chunk = todo[i:i + 40]
        q = urllib.parse.urlencode({"action": "query", "titles": "|".join("File:" + n for n in chunk),
                                    "prop": "imageinfo", "iiprop": "url|sha1", "format": "json"})
        pages = json.loads(get(API + "?" + q))["query"]["pages"].values()
        for p in pages:
            info = p.get("imageinfo")
            if not info:
                raise SystemExit(f"fetch: the wiki has no {p['title']}")
            name = p["title"][5:].replace(" ", "_")
            url = info[0]["url"]
            data = get(url + ("&" if "?" in url else "?") + "format=original")   # (not the WebP the CDN serves)
            if hashlib.sha1(data).hexdigest() != info[0]["sha1"]:
                print(f"fetch: {name} differs from the wiki's checksum (kept anyway)")
            with open(os.path.join(wiki, name), "wb") as f:
                f.write(data)
            print("wiki", name)
    game = os.path.join(ref, "game")
    os.makedirs(game, exist_ok=True)
    if any(not os.path.exists(os.path.join(game, n)) for n in GAME_FILES):
        apk = os.path.join(game, GAME_APK[0])
        if not os.path.exists(apk):
            data = get(GAME_APK[1])
            if hashlib.md5(data).hexdigest() != GAME_APK[2]:
                raise SystemExit("fetch: the game's package differs from the one the port was made from")
            with open(apk, "wb") as f:
                f.write(data)
            print("game", GAME_APK[0])
        with zipfile.ZipFile(apk) as z:   # (only its pictures are read)
            for n in GAME_FILES:
                with open(os.path.join(game, n), "wb") as f:
                    f.write(z.read("assets/" + n))
                print("game", n)
    fonts = os.path.join(ref, "fonts")
    os.makedirs(fonts, exist_ok=True)
    if not os.path.exists(os.path.join(fonts, FONT[0])):
        with open(os.path.join(fonts, FONT[0]), "wb") as f:
            f.write(get(FONT[1]))
        print("font", FONT[0])


if __name__ == "__main__":
    main()
