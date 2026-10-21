#!/usr/bin/env python3
"""Starts a new NumWorks game from NumPlay's template (sdk/template, "Star Catcher"): a small game that
already draws, reads the keys, pauses, saves and quits the way NumPlay's games do. Edit its src/main.c.

  python3 tools/new_game.py "My Game"                 games/mygame/, MyGame.nwa
  python3 tools/new_game.py "My Game" --id mg         games/mg/
  python3 tools/new_game.py "My Game" --numplay       also on NumPlay's home screen (games/games.json)

Then:  make -C games/mygame                           output/mygame.nwa, ready to install
       python3 tools/emu.py games/mygame/output/mygame.nwa --ms 5000 --shots 2000 --out /tmp/shots
See docs/make-a-game.md for the whole guide. Works on macOS, Linux and Windows (Python 3 and Pillow).
"""
import argparse
import json
import os
import re
import shutil
import sys

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
TEMPLATE = os.path.join(ROOT, "sdk", "template")


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("title", help="the game's name, as players see it (\"My Game\")")
    ap.add_argument("--id", help="its folder and file name: lower case letters and digits (default: from the title)")
    ap.add_argument("--numplay", action="store_true", help="also add it to NumPlay's launcher (games/games.json)")
    a = ap.parse_args()
    gid = a.id or re.sub(r"[^a-z0-9]", "", a.title.lower())
    if not re.fullmatch(r"[a-z][a-z0-9]{1,15}", gid):
        sys.exit("new_game: the id must be 2 to 16 lower case letters and digits, starting with a letter (--id)")
    app = re.sub(r"[^A-Za-z0-9]", "", a.title.title()) or gid
    dest = os.path.join(ROOT, "games", gid)
    if os.path.exists(dest):
        sys.exit(f"new_game: games/{gid} is already there")
    if len(a.title.encode()) > 40:
        sys.exit("new_game: keep the title under 40 letters (the calculator's home screen shows it)")

    shutil.copytree(TEMPLATE, dest, ignore=shutil.ignore_patterns("output", "build"))
    for path in (os.path.join(dest, "Makefile"), os.path.join(dest, "src", "main.c")):
        s = open(path, encoding="utf-8").read()
        s = s.replace("Star Catcher", a.title).replace("StarCatcher", app).replace("starcatcher", gid)
        open(path, "w", encoding="utf-8").write(s)
    open(os.path.join(dest, "README.md"), "w", encoding="utf-8").write(
        f"# {a.title}\n\nA game for the NumWorks calculator, made with [NumPlay](../../docs/make-a-game.md)'s "
        f"template.\n\n## Controls\n\n| Key | Action |\n| --- | --- |\n| Left, Right | Move |\n"
        f"| OK or EXE | Choose |\n| Back | Pause: Resume, Restart, Quit game |\n| Home | Quit |\n\n"
        f"Build: `make` makes `output/{gid}.nwa` (needs `arm-none-eabi-gcc` and Node.js for nwlink).\n")

    if a.numplay:
        p = os.path.join(ROOT, "games", "games.json")
        games = json.load(open(p, encoding="utf-8"))
        games.append({"id": gid, "title": a.title, "tagline": "A new game", "credit": None, "top": "0x1C2B5E",
                      "bottom": "0x0B1638", "accent": "0xFFD34E", "records": [f"{gid}.sav"], "shots": ["docs/shot.png"],
                      "app": f"{app}.nwa", "repo": f"https://github.com/Mason363/NumPlay/tree/main/games/{gid}"})
        open(p, "w", encoding="utf-8").write(json.dumps(games, indent=2, ensure_ascii=False) + "\n")
        os.makedirs(os.path.join(dest, "docs"), exist_ok=True)
        print(f"Added to games/games.json. NumPlay's card needs games/{gid}/docs/shot.png (320x240): take one with\n"
              f"  python3 tools/emu.py games/{gid}/output/{gid}.nwa --ms 4000 --keys 1500-1600:ok --shots 3000 "
              f"--out games/{gid}/docs\n  and rename it shot.png.")
    print(f"games/{gid}/ is ready: its game is in games/{gid}/src/main.c, its icon in src/icon.png "
          f"(tools/icon.py makes one from any picture).\n"
          f"  make -C games/{gid}                 builds output/{gid}.nwa\n"
          f"  python3 tools/emu.py games/{gid}/output/{gid}.nwa --ms 5000 --keys 1500-1600:ok --shots 3000 --out shots\n"
          f"Install the .nwa from https://my.numworks.com/apps with the calculator plugged in.")


if __name__ == "__main__":
    main()
