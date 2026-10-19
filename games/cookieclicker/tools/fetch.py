#!/usr/bin/env python3
"""Gathers the public material the pack is made from, into REF (not committed):

    python3 -I tools/fetch.py REF

- REF/main.js, REF/minigame*.js, REF/loc/EN.js, REF/style.css: the game itself (version 2.058), as the mirror
  github.com/ozh/cookieclicker keeps it at the commit below; tools/extract.js runs it for its numbers and texts
- REF/img/...: its pictures: the big cookie and its shine, the backgrounds, milks, buildings, icons, golden and
  wrath cookies, wrinklers, sugar lumps, the dragon, the heavenly upgrades' sky, the minigames' backgrounds
- REF/fonts/: Merriweather, Kavoon and Noto Sans (SIL Open Font License), from Google Fonts

Files already there are kept: run it again to fetch only what is missing."""
import os
import sys
import time
import urllib.request

GAME = "https://raw.githubusercontent.com/ozh/cookieclicker/c77f00c51854a667da35a3135120797a579c2d72/"
FONTS = {
    "Merriweather.ttf": "https://raw.githubusercontent.com/google/fonts/main/ofl/merriweather/"
                        "Merriweather%5Bopsz%2Cwdth%2Cwght%5D.ttf",
    "Kavoon.ttf": "https://raw.githubusercontent.com/google/fonts/main/ofl/kavoon/Kavoon-Regular.ttf",
    "NotoSans.ttf": "https://raw.githubusercontent.com/google/fonts/main/ofl/notosans/NotoSans%5Bwdth%2Cwght%5D.ttf",
    "NotoSans-Italic.ttf": "https://raw.githubusercontent.com/google/fonts/main/ofl/notosans/"
                           "NotoSans-Italic%5Bwdth%2Cwght%5D.ttf",
}
SCRIPTS = ["main.js", "minigameGrimoire.js", "minigameGarden.js", "minigamePantheon.js", "minigameMarket.js",
           "loc/EN.js", "style.css"]
BUILDINGS = ["grandma", "farm", "mine", "factory", "bank", "temple", "wizardtower", "shipment", "alchemylab", "portal",
             "timemachine", "antimattercondenser", "prism", "chancemaker", "fractalEngine", "javascriptconsole",
             "idleverse", "cortex", "you"]
GRANDMAS = ["farmerGrandma", "workerGrandma", "minerGrandma", "cosmicGrandma", "transmutedGrandma", "alteredGrandma",
            "grandmasGrandma", "antiGrandma", "rainbowGrandma", "bankGrandma", "templeGrandma", "witchGrandma",
            "luckyGrandma", "metaGrandma", "scriptGrandma", "alternateGrandma", "brainyGrandma", "cloneGrandma",
            "elfGrandma", "bunnyGrandma"]
MILKS = ["Plain", "Chocolate", "Raspberry", "Orange", "Caramel", "Banana", "Lime", "Blueberry", "Strawberry",
         "Vanilla", "Honey", "Coffee", "Tea", "Coconut", "Cherry", "Spiced", "Maple", "Mint", "Licorice", "Rose",
         "Dragonfruit", "Melon", "Blackcurrant", "Peach", "Hazelnut"]
IMAGES = [
    "perfectCookie.png", "cookieShadow.png", "flare.png", "flareGold.png", "flareSuckRed.png", "shine.png",
    "shineSpoke.png", "glint.png", "bgBlue.jpg", "grandmas1.jpg", "grandmas2.jpg", "grandmas3.jpg", "darkNoise.jpg",
    "shadedBordersSoft.png", "shadedBordersGold.png", "shadedBordersRed.png", "cookieShower1.png",
    "cookieShower2.png", "cookieShower3.png", "smallCookies.png", "cursor.png", "goldCookie.png", "wrathCookie.png",
    "buildings.png", "icons.png", "storeTile.jpg", "money.png", "upgradeFrame.png", "panelVertical.png",
    "panelHorizontal.png", "brokenCookie.png", "brokenCookieHalo.png", "heavenRing1.jpg", "heavenRing2.jpg",
    "starbg.jpg", "heavenlyMoney.png", "wrinkler.png", "shinyWrinkler.png", "wrinklerBits.png",
    "shinyWrinklerBits.png", "wrinklerShadow.png", "wrinklerBlink.png", "wrinklerGooglies.png", "sugarLump.png",
    "dragon.png", "dragonBG2.png", "shimmeringVeil.png", "BGgrimoire.jpg", "BGpantheon.jpg", "BGgarden.jpg",
    "BGmarket.jpg", "gardenPlants.png", "gardenPlots.png", "spellBG.png", "spinnyBig.png", "spinnySmall.png",
    "levelUp.png", "imperfectCookie.png", "youBackground.png", "elfGrandma.png", "frameBorder.png", "santa.png",
    "frostedReindeer.png", "goldCookieWreath.png", "wrathCookieWreath.png", "hearts.png", "bunnies.png",
    "spookyCookie.png", "contract.png", "wrathContract.png", "familiars.png",
    "ascendSlot.png",
] + [b + ".png" for b in BUILDINGS] + [b + "Background.png" for b in BUILDINGS if b != "you"] + [
    g + ".png" for g in GRANDMAS] + ["milk" + m + ".png" for m in MILKS]


def get(url, tries=4):
    for t in range(tries):
        try:
            with urllib.request.urlopen(url, timeout=120) as r:
                return r.read()
        except urllib.error.HTTPError as e:
            if e.code == 404:
                return None
            if t == tries - 1:
                raise SystemExit(f"fetch: {url}: {e}")
        except Exception as e:
            if t == tries - 1:
                raise SystemExit(f"fetch: {url}: {e}")
        time.sleep(2 << t)


def main(ref):
    jobs = [(GAME + p, os.path.join(ref, p)) for p in SCRIPTS]
    jobs += [(GAME + "img/" + p, os.path.join(ref, "img", p)) for p in dict.fromkeys(IMAGES)]
    jobs += [(url, os.path.join(ref, "fonts", name)) for name, url in FONTS.items()]
    missing = []
    for url, dst in jobs:
        if os.path.exists(dst) and os.path.getsize(dst):
            continue
        os.makedirs(os.path.dirname(dst), exist_ok=True)
        data = get(url)
        if data is None:
            missing.append(url)
            continue
        with open(dst, "wb") as f:
            f.write(data)
        print(f"{len(data):9d} {dst}")
    for url in missing:
        print("not found:", url)


if __name__ == "__main__":
    if len(sys.argv) != 2:
        raise SystemExit(__doc__)
    main(sys.argv[1])
