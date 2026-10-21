#!/usr/bin/env python3
"""Gathers the public material the pack is made from, into REF (not committed):

    python3 -I tools/fetch.py REF

- REF/pvz/...: the game's pictures, as the open-source web port pvz-web (github.com/xyxiao001/pvz-web, at the
  commit below) keeps them: every animation of the 40 plants of Adventure and its zombies as frames on 14 sheets
  (public/assets/original: actors-*.png, animations.json), the lawns, the menus and the other pictures, and
  Dr. Zomboss as the game's own parts and their moves (public/assets/runtime/boss)
- REF/fonts/: Nunito and Rammetto One (SIL Open Font License), from Google Fonts

Files already there are kept: run it again to fetch only what is missing."""
import os
import sys
import time
import urllib.request

PVZ = "https://raw.githubusercontent.com/xyxiao001/pvz-web/2d9c64135da3d74ae090c59e1cdfb9b20a09c4cb/"
FONTS = {
    "Nunito.ttf": "https://raw.githubusercontent.com/google/fonts/main/ofl/nunito/Nunito%5Bwght%5D.ttf",
    "RammettoOne.ttf": "https://raw.githubusercontent.com/google/fonts/main/ofl/rammettoone/RammettoOne-Regular.ttf",
}
A = "public/assets/"
FILES = [A + "original/animations.json"] + [A + f"original/actors-{i}.png" for i in range(14)] + [
    A + "lawn.jpg", A + "yard.jpg", A + "original/night.jpg", A + "original/pool.png", A + "original/fog.png",
    A + "original/roof.png", A + "runtime/boss/background.webp", A + "original/award.jpg",
    A + "runtime/effects.json", A + "runtime/pea.webp",
] + [A + "runtime/original/" + n + ".webp" for n in (
    "menu", "logo", "dialog", "seedbank", "shovelbank", "shovel", "crazy-dave", "dave-bubble", "trophy", "button",
    "shop-background", "shop-slots", "shop-roof", "shop-rake", "shop-firstaid", "roof-cleaner", "rake",
    "coin-diamond", "coin-gold", "coin-silver", "meter-empty", "meter-full", "meter-flag", "meter-head",
    "seed-packet", "mower", "pool-cleaner", "grave", "crater", "ice-trail", "ladder", "bungee-target",
    "bungee-cord", "sled", "vases", "hammer", "bowling-stripe", "conveyor", "conveyor-back", "fog-cloud",
    "snow-pea", "puff", "star", "spike", "cabbage", "kernel", "butter", "melon", "basketball", "fire-pea",
    "flowerpot", "trophy")] + [A + "runtime/ui/" + n + ".webp" for n in (
    "seed-chooser", "almanac-card", "stone-panel", "parchment-button", "seed-start", "seed-start-disabled",
    "challenge-background", "seed-silhouette")] + [A + "portraits/almanac/" + n + ".png" for n in (
    "backup", "balloon", "bobsled", "bucket", "bungee", "catapult", "cone", "dancer", "digger", "dolphin", "door",
    "ducky", "flag", "football", "gargantuar", "jack", "ladder", "newspaper", "normal", "pogo", "pole", "snorkel",
    "zomboni")] + [A + "runtime/almanac/imp.webp", A + "runtime/boss/tracks.json"] + [
    A + "runtime/boss/" + n + ".webp" for n in (
        "ZOMBIE_BOSSDRIVER_BODY ZOMBIE_BOSSDRIVER_BRAIN ZOMBIE_BOSSDRIVER_FACE ZOMBIE_BOSSDRIVER_FACE2 "
        "ZOMBIE_BOSSDRIVER_INNERHAND ZOMBIE_BOSSDRIVER_JAW ZOMBIE_BOSSDRIVER_JAW2 ZOMBIE_BOSSDRIVER_LOWERARM "
        "ZOMBIE_BOSSDRIVER_LOWERARM2 ZOMBIE_BOSSDRIVER_OUTERHAND ZOMBIE_BOSSDRIVER_UPPERARM ZOMBIE_BOSS_ANTENNA "
        "ZOMBIE_BOSS_ANTENNA_LIT ZOMBIE_BOSS_ARM_UPPER2 ZOMBIE_BOSS_EYEGLOW ZOMBIE_BOSS_EYEGLOW_BLACK "
        "ZOMBIE_BOSS_EYEGLOW_RED ZOMBIE_BOSS_FIREBALL ZOMBIE_BOSS_FIREBALL_ADDITIVE ZOMBIE_BOSS_FIREBALL_CHUNKS "
        "ZOMBIE_BOSS_FIREBALL_MULTIPLY ZOMBIE_BOSS_FIREBALL_SUPERGLOW ZOMBIE_BOSS_FLAG ZOMBIE_BOSS_FLAGPOLE "
        "ZOMBIE_BOSS_FOOT ZOMBIE_BOSS_HEAD ZOMBIE_BOSS_HEAD2 ZOMBIE_BOSS_ICEBALL ZOMBIE_BOSS_ICEBALL_CRYSTAL1 "
        "ZOMBIE_BOSS_ICEBALL_CRYSTAL2 ZOMBIE_BOSS_ICEBALL_CRYSTAL3 ZOMBIE_BOSS_ICEBALL_HIGHLIGHT "
        "ZOMBIE_BOSS_ICEBALL_MULTIPLY ZOMBIE_BOSS_ICEBALL_OVERLAY ZOMBIE_BOSS_ICEFIRE_SHADOW "
        "ZOMBIE_BOSS_INNERARM_FINGER ZOMBIE_BOSS_INNERARM_FINGERROPE ZOMBIE_BOSS_INNERARM_HAND "
        "ZOMBIE_BOSS_INNERARM_LOWER ZOMBIE_BOSS_INNERARM_THUMB1 ZOMBIE_BOSS_INNERARM_THUMB2 "
        "ZOMBIE_BOSS_INNERARM_UPPER ZOMBIE_BOSS_INNERJAW ZOMBIE_BOSS_INNERLEG_UPPER ZOMBIE_BOSS_JAW "
        "ZOMBIE_BOSS_LEGBITS ZOMBIE_BOSS_LEG_LOWER ZOMBIE_BOSS_LOWERBODY ZOMBIE_BOSS_MOUTHGLOW "
        "ZOMBIE_BOSS_MOUTHGLOW_RED ZOMBIE_BOSS_NECK ZOMBIE_BOSS_OUTERARM_FINGER1 ZOMBIE_BOSS_OUTERARM_FINGER2 "
        "ZOMBIE_BOSS_OUTERARM_HAND ZOMBIE_BOSS_OUTERARM_LOWER ZOMBIE_BOSS_OUTERARM_THUMB1 "
        "ZOMBIE_BOSS_OUTERARM_THUMB2 ZOMBIE_BOSS_OUTERARM_UPPER ZOMBIE_BOSS_OUTERLEG_UPPER ZOMBIE_BOSS_RV "
        "ZOMBIE_BOSS_RVWHEEL ZOMBIE_BOSS_UPPERBODY").split()]


def get(url, tries=4):
    for t in range(tries):
        try:
            with urllib.request.urlopen(url, timeout=120) as r:
                return r.read()
        except Exception as e:
            if t == tries - 1:
                raise SystemExit(f"fetch: {url}: {e}")
            time.sleep(2 << t)


def main(ref):
    jobs = [(PVZ + p, os.path.join(ref, "pvz", p[len(A):])) for p in dict.fromkeys(FILES)]
    jobs += [(url, os.path.join(ref, "fonts", name)) for name, url in FONTS.items()]
    for url, dst in jobs:
        if os.path.exists(dst) and os.path.getsize(dst):
            continue
        os.makedirs(os.path.dirname(dst), exist_ok=True)
        data = get(url)
        with open(dst, "wb") as f:
            f.write(data)
        print(f"{len(data):9d} {dst}")


if __name__ == "__main__":
    if len(sys.argv) != 2:
        raise SystemExit(__doc__)
    main(sys.argv[1])
