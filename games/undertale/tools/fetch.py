#!/usr/bin/env python3
"""Gathers the public material the pack is made from, into REF (not committed):

    python3 -I tools/fetch.py REF

- REF/wiki/maps/*.png: the Undertale Wiki's maps of each area (undertale.wiki, Category:Maps): every room,
  one pixel for one, laid out as in the game
- REF/wiki/sprites/*: the wiki's sprite files (Category:Overworld sprites, Battle sprites, Face sprites,
  Sprites, Item files), as their own names
- REF/wiki/pages/*.txt: the wiki's pages (their wikitext) that the game's texts are checked against
- REF/demo/: the free demo from undertale.com (its Mac build keeps its sprites and backgrounds as PNG files)
- REF/fonts/: Pixel Operator (CC0), by Jayvee Enaguas, once called 8-bit Operator: the font the game credits

Files already there are kept: run it again to fetch only what is missing."""
import json
import os
import sys
import time
import urllib.parse
import urllib.request
import zipfile

API = "https://undertale.wiki/api.php"
UA = {"User-Agent": "NumPlay-Undertale/1.0 (fan port for the NumWorks calculator; one-time asset fetch)"}
CATEGORIES = {
    "maps": ["Maps"],
    "sprites": ["Overworld_sprites", "Battle_sprites", "Face_sprites", "Sprites", "Item_files", "Location_files"],
    "screens": ["Screenshots", "Cut_content_files"],
}
PAGES = ["Ruins", "Ruins/Puzzles", "Toriel", "Toriel/In_battle", "Toriel's_Home", "Flowey", "Napstablook",
         "Cell_Phone/Toriel", "SAVE_Point", "Froggit", "Whimsun", "Moldsmal", "Loox", "Vegetoid", "Migosp",
         "Dummy", "Froggit_NPC", "Spider_Bake_Sale", "Items", "Monster_Candy", "Spider_Donut", "Spider_Cider",
         "Butterscotch_Pie", "Snail_Pie", "Toy_Knife", "Faded_Ribbon", "Bandage", "Stick", "LOVE", "EXP",
         "HP", "AT", "DF", "Stats", "FIGHT", "ACT", "MERCY", "ITEM", "Flowey/In_battle", "Napstablook/In_battle",
         "Encounter", "Home", "Annoying_Dog", "Rock", "Mouse", "Golden_flower", "Chara", "Frisk", "Name",
         "Item", "Consumables", "Weapons", "Armor", "Cell_Phone", "Intro", "Game_Over", "Damage", "Monster_Candy",
         "Toriel's_Home", "Blue_SOUL", "Bullet_board", "Spare", "Flee", "Gold", "Save",
         # Snowdin
         "Snowdin", "Snowdin/Puzzles", "Snowdin_Town", "Sans", "Papyrus", "Papyrus/In_battle", "Sans/In_battle",
         "Snowdrake", "Ice_Cap", "Gyftrot", "Doggo", "Lesser_Dog", "Greater_Dog", "Dogamy_and_Dogaressa", "Jerry",
         "Chilldrake", "Glyde", "Snowman", "Monster_Kid", "Multicolor_Tile_Puzzle", "Nice_Cream_guy", "Nice_Cream",
         "Dimensional_Box", "Ball_Game", "Grillby's", "Grillby", "Papyrus_and_Sans's_House", "Papyrus's_room",
         "Sans's_room", "Shop", "Snowdin_Shop", "Snowed_Inn", "Innkeeper", "Librarby", "Gift_Bear", "Ice_Wolf",
         "Cell_Phone/Papyrus", "Papyrus_date", "Mysterious_door", "Snowdin_Forest", "Royal_Guard", "Blue_attack",
         "Faun", "Big_Mouth", "Crazy_Bun", "Drunk_Bun", "Politics_Bear", "Punk_Hamster", "Rabbit_Kid",
         "Rabbit_Lady_and_Cinnamon", "Red_Bird", "Red_Demon", "Scarf_Lady", "Slime_family", "Ugly_Fish",
         "Library_Lizard", "Library_Loox", "Knock_lover", "Lady_Garf", "Wrap_Mouse", "Innkeeper's_child",
         "Snoring_guests", "Mad_Mew_Mew", "Snowdrake's_mother", "Snowdrake's_father", "Bonetrousle",
         # Waterfall
         "Waterfall", "Waterfall/Puzzles", "Aaron", "Woshua", "Moldbygg", "Shyren", "Temmie", "Temmie_Village",
         "Mad_Dummy", "Undyne", "Undyne/In_battle", "Onionsan", "Gerson", "Echo_Flower", "Bridge_seed",
         "Undyne's_House", "Napstablook's_house", "Garbage_Dump", "Shyren/In_battle", "Riverman", "Piano_puzzle",
         "Legendary_Artifact", "Astronaut_Food", "Glamburger", "Hot_Dog...?", "Cloudy_Glasses", "Torn_Notebook",
         "Stained_Apron", "Burnt_Pan", "Toy_Knife", "Temmie_Flakes", "Tem_Shop", "Small_bird", "Lemon_Bread",
         "Tough_Glove", "Manly_Bandanna", "Old_Tutu", "Ballet_Shoes", "Cinnamon_Bunny", "Bisicle", "Unisicle",
         "Nice_Cream_(item)", "Snowman_Piece", "Snowman_Piece", "Punch_Card", "Dog_Residue", "Abandoned_Quiche",
         "Astronaut_Food", "Hot_Dog...?", "Hot_Cat", "Spaghetti", "Glamburger", "Sea_Tea", "Temmie_Flakes",
         "Junk_Food", "Annoying_Dog/In_battle", "Lesser_Dog/In_battle", "Greater_Dog/In_battle", "Doggo/In_battle",
         "Dogamy_and_Dogaressa/In_battle",
         # Hotland and the CORE
         "Hotland", "Hotland/Puzzles", "Alphys", "Alphys's_Lab", "Mettaton", "Mettaton/In_battle", "Mettaton_EX",
         "Muffet", "Muffet/In_battle", "Royal_Guard/In_battle", "Vulkin", "Tsunderplane", "Pyrope", "So_Sorry",
         "Madjick", "Knight_Knight", "Final_Froggit", "Whimsalot", "Astigmatism", "CORE", "MTT_Resort", "Burgerpants",
         "Heats_Flamesman", "Bratty_and_Catty", "Spider_Bake_Sale", "Elevator", "Sans/Hotland", "Hot_Dog...?",
         "Cell_Phone/Alphys", "Steam_Vent", "Conveyor_belt", "Mettaton_quiz_show", "Laser", "Royal_Guards", "Bomb",
         "Stained_Apron", "Hot_Dog_Harpy", "Art_Club_room", "Spider_Entrance", "Multicolor_Tile_Puzzle", "Lab",
         # New Home
         "New_Home", "Asgore", "Asgore/In_battle", "Asgore's_Home", "Last_Corridor", "Throne_Room", "Barrier",
         "Photoshop_Flowey", "Neutral_Route", "Worn_Dagger", "Heart_Locket"]
# files the categories leave out, as their wiki names
FILES = ["QC sprite.png", "QC screenshot shop.png", "Gerson screenshot shop.png", "Tem Shop screenshot Temmie Armor.png"]
DEMO = {
    "MacUnderTale.zip": "https://thisisatestofthevideogamedeliverysystem.sfo2.digitaloceanspaces.com/game/MacUnderTale.zip",
}
FONT = ("pixel_operator.zip", "https://dl.dafont.com/dl/?f=pixel_operator")


def get(url, tries=4):
    for t in range(tries):
        try:
            with urllib.request.urlopen(urllib.request.Request(url, headers=UA), timeout=60) as r:
                return r.read()
        except Exception as e:  # network hiccups: wait and try again
            if t == tries - 1:
                raise
            print("  retry", url, e)
            time.sleep(2 ** (t + 1))


def api(**q):
    q["format"] = "json"
    return json.loads(get(API + "?" + urllib.parse.urlencode(q)))


def category_files(cat):
    out, cont = [], {}
    while True:
        d = api(action="query", list="categorymembers", cmtitle="Category:" + cat, cmlimit=500, cmtype="file", **cont)
        out += [m["title"] for m in d["query"]["categorymembers"]]
        if "continue" not in d:
            return out
        cont = {"cmcontinue": d["continue"]["cmcontinue"]}


def file_url(title):
    d = api(action="query", prop="imageinfo", iiprop="url", titles=title)
    for p in d["query"]["pages"].values():
        if "imageinfo" in p:
            return p["imageinfo"][0]["url"].split("?")[0]
    return None


def save(path, data):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path + ".part", "wb") as f:
        f.write(data)
    os.replace(path + ".part", path)


def main():
    if len(sys.argv) != 2:
        sys.exit(__doc__)
    ref = sys.argv[1]
    for kind, cats in CATEGORIES.items():
        for cat in cats:
            for title in category_files(cat):
                name = title.split(":", 1)[1].replace(" ", "_")
                path = os.path.join(ref, "wiki", kind, name)
                if os.path.exists(path):
                    continue
                url = file_url(title)
                if url:
                    print(kind, name)
                    save(path, get(url))
    for name in FILES:
        path = os.path.join(ref, "wiki", "sprites", name.replace(" ", "_"))
        if not os.path.exists(path):
            url = file_url("File:" + name)
            if url:
                print("file", name)
                save(path, get(url))
    for page in PAGES:
        path = os.path.join(ref, "wiki", "pages", page.replace("/", "__") + ".txt")
        if os.path.exists(path):
            continue
        d = api(action="parse", page=page.replace("_", " "), prop="wikitext", redirects=1)
        if "parse" in d:
            print("page", page)
            save(path, d["parse"]["wikitext"]["*"].encode())
    for name, url in DEMO.items():
        path = os.path.join(ref, "demo", name)
        if not os.path.exists(path):
            print("demo", name)
            save(path, get(url))
        out = os.path.join(ref, "demo", "mac")
        if not os.path.exists(out):
            with zipfile.ZipFile(path) as z:
                for m in z.namelist():
                    if "/MacOS/sprites/" in m or "/MacOS/bgs/" in m or m.endswith("_manifest.txt"):
                        if m.startswith("__MACOSX") or m.endswith("/"):
                            continue
                        save(os.path.join(out, *m.split("/MacOS/")[1].split("/")), z.read(m))
    path = os.path.join(ref, "fonts", FONT[0])
    if not os.path.exists(path):
        print("font", FONT[0])
        save(path, get(FONT[1]))
    with zipfile.ZipFile(path) as z:
        for m in z.namelist():
            if m.endswith(".ttf") or m.endswith(".txt"):
                dst = os.path.join(ref, "fonts", os.path.basename(m))
                if not os.path.exists(dst):
                    save(dst, z.read(m))


if __name__ == "__main__":
    main()
