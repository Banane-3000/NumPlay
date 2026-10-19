#!/usr/bin/env python3
"""Cookie Clicker's data for the calculator: src/data.h.

The names of the upgrades, the achievements and the news are Orteil's, as the game has them
(version 2.05). They go into one block of text packed with DEFLATE (zopfli), which the game
unpacks once when it starts (NumPlay's own decoder, launcher/src/inflate.c), so they take about
half their size in flash. The pictures of the store are 12 x 12 masks drawn below.

Usage: data.py [OUT]   (src/data.h by default; needs the zopfli module)
"""
import os
import sys

import zopfli.zlib

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = sys.argv[1] if len(sys.argv) > 1 else os.path.join(HERE, "..", "src", "data.h")

TIER_N = 5       # tiered upgrades for each building (the game has 15)
ACH_TIERS = 1    # building achievements for each (have 1)

# ------------------------------------------------------------------ upgrades, in the order of the save
CURSOR = ["Reinforced index finger", "Carpal tunnel prevention cream", "Ambidextrous", "Thousand fingers"] + \
    [n + " fingers" for n in ["Million", "Billion", "Trillion", "Quadrillion", "Quintillion", "Sextillion", "Septillion",
                              "Octillion", "Nonillion", "Decillion", "Undecillion"]]
MOUSE = [n + " mouse" for n in ["Plastic", "Iron", "Titanium", "Adamantium", "Unobtainium", "Eludium", "Wishalloy",
                                "Fantasteel", "Nevercrack", "Armythril", "Technobsidian", "Plasmarble", "Miraculite",
                                "Aetherice", "Omniplast"]]
KITTEN = ["Kitten " + n for n in ["helpers", "workers", "engineers", "overseers", "managers", "accountants"]]
GOLD = ["Lucky day", "Serendipity", "Get lucky"]
GRANDMA = ["Farmer grandmas", "Miner grandmas", "Worker grandmas", "Banker grandmas", "Priestess grandmas",
           "Witch grandmas", "Cosmic grandmas", "Transmuted grandmas", "Altered grandmas", "Grandmas' grandmas",
           "Antigrandmas", "Rainbow grandmas", "Lucky grandmas", "Metagrandmas", "Binary grandmas", "Alternate grandmas",
           "Brainy grandmas", "Clone grandmas"]
# (name, % more CpS, price: so many nines, times 5 or not)
COOKIES = [
    ("Plain cookies", 1, 6, 0), ("Sugar cookies", 1, 6, 1), ("Oatmeal raisin cookies", 1, 7, 0),
    ("Peanut butter cookies", 2, 7, 1), ("Coconut cookies", 2, 8, 0), ("White chocolate cookies", 2, 8, 1),
    ("Macadamia nut cookies", 2, 8, 0), ("Double-chip cookies", 2, 9, 1),
    ("White chocolate macadamia nut cookies", 2, 10, 0), ("All-chocolate cookies", 2, 10, 1),
    ("Dark chocolate-coated cookies", 5, 11, 0), ("White chocolate-coated cookies", 5, 11, 0),
    ("Eclipse cookies", 2, 11, 1), ("Zebra cookies", 2, 12, 0), ("Snickerdoodles", 2, 12, 1),
    ("Stroopwafels", 2, 13, 0), ("Macaroons", 2, 13, 1), ("Empire biscuits", 2, 14, 0),
    ("British tea biscuits", 2, 14, 0), ("Madeleines", 2, 14, 1), ("Palmiers", 2, 14, 1), ("Palets", 2, 15, 0),
    ("Sables", 2, 15, 0), ("Caramoas", 3, 16, 0), ("Sagalongs", 3, 16, 0), ("Shortfoils", 3, 16, 0),
    ("Win mints", 3, 16, 0), ("Gingerbread men", 2, 16, 0), ("Gingerbread trees", 2, 16, 0),
    ("Pure black chocolate cookies", 5, 16, 1), ("Pure white chocolate cookies", 5, 16, 1),
    ("Ladyfingers", 3, 17, 0), ("Tuiles", 3, 17, 1), ("Chocolate-stuffed biscuits", 3, 18, 0),
    ("Checker cookies", 3, 18, 1), ("Butter cookies", 3, 19, 0), ("Cream cookies", 3, 19, 1),
]
TIERED = {  # each building's upgrades, tier 1 first
    "Grandma": ["Forwards from grandma", "Steel-plated rolling pins", "Lubricated dentures", "Prune juice",
                "Double-thick glasses", "Aging agents", "Xtreme walkers", "The Unbridling", "Reverse dementia",
                "Timeproof hair dyes"],
    "Farm": ["Cheap hoes", "Fertilizer", "Cookie trees", "Genetically-modified cookies", "Gingerbread scarecrows",
             "Pulsar sprinklers", "Fudge fungus", "Wheat triffids", "Humane pesticides", "Barnstars"],
    "Mine": ["Sugar gas", "Megadrill", "Ultradrill", "Ultimadrill", "H-bomb mining", "Coreforge", "Planetsplitters",
             "Canola oil wells", "Mole people", "Mine canaries"],
    "Factory": ["Sturdier conveyor belts", "Child labor", "Sweatshop", "Radium reactors", "Recombobulators",
                "Deep-bake process", "Cyborg workforce", "78-hour days", "Machine learning", "Brownie point system"],
    "Bank": ["Taller tellers", "Scissor-resistant credit cards", "Acid-proof vaults", "Chocolate coins",
             "Exponential interest rates", "Financial zen", "Way of the wallet", "The stuff rationale", "Edible money",
             "Grand supercycles"],
    "Temple": ["Golden idols", "Sacrifices", "Delicious blessing", "Sun festival", "Enlarged pantheon",
               "Great Baker in the sky", "Creation myth", "Theocracy", "Sick rap prayers", "Psalm-reading"],
    "Wizard tower": ["Pointier hats", "Beardlier beards", "Ancient grimoires", "Kitchen curses", "School of sorcery",
                     "Dark formulas", "Cookiemancy", "Rabbit trick", "Deluxe tailored wands", "Immobile spellcasting"],
    "Shipment": ["Vanilla nebulae", "Wormholes", "Frequent flyer", "Warp drive", "Chocolate monoliths",
                 "Generation ship", "Dyson sphere", "The final frontier", "Autopilot",
                 "Restaurants at the end of the universe"],
    "Alchemy lab": ["Antimony", "Essence of dough", "True chocolate", "Ambrosia", "Aqua crustulae", "Origin crucible",
                    "Theory of atomic fluidity", "Beige goo", "The advent of chemistry", "On second thought"],
    "Portal": ["Ancient tablet", "Insane oatling workers", "Soul bond", "Sanity dance", "Brane transplant",
               "Deity-sized portals", "End of times back-up plan", "Maddening chants", "The real world",
               "Dimensional garbage gulper"],
    "Time machine": ["Flux capacitors", "Time paradox resolver", "Quantum conundrum", "Causality enforcer",
                     "Yestermorrow comparators", "Far future enactment", "Great loop hypothesis",
                     "Cookietopian moments of maybe", "Second seconds", "Additional clock hands"],
    "Antimatter condenser": ["Sugar bosons", "String theory", "Large macaron collider", "Big bang bake",
                             "Reverse cyclotrons", "Nanocosmics", "The Pulse",
                             "Some other super-tiny fundamental particle? Probably?", "Quantum comb",
                             "Baking Nobel prize"],
    "Prism": ["Gem polish", "9th color", "Chocolate light", "Grainbow", "Pure cosmic light", "Glow-in-the-dark",
              "Lux sanctorum", "Reverse shadows", "Crystal mirrors", "Reverse theory of light"],
    "Chancemaker": ["Your lucky cookie", "\"All Bets Are Off\" magic coin", "Winning lottery ticket",
                    "Four-leaf clover field", "A recipe book about books", "Leprechaun village", "Improbability drive",
                    "Antisuperstistronics", "Bunnypedes", "Revised probabilistics"],
    "Fractal engine": ["Metabakeries", "Mandelbrown sugar", "Fractoids", "Nested universe theory",
                       "Menger sponge cake", "One particularly good-humored cow", "Chocolate ouroboros", "Nested",
                       "Space-filling fibers", "Endless book of prose"],
    "Javascript console": ["The JavaScript console for dummies", "64bit arrays", "Stack overflow",
                           "Enterprise compiler", "Syntactic sugar", "A nice cup of coffee", "Just-in-time baking",
                           "cookies++", "Software updates", "Game.Loop"],
    "Idleverse": ["Manifest destiny", "The multiverse in a nutshell", "All-conversion", "Multiverse agents",
                  "Escape plan", "Game design", "Sandbox universes", "Multiverse wars", "Mobile ports",
                  "Encapsulated realities"],
    "Cortex baker": ["Principled neural shackles", "Obey", "A sprinkle of irrationality", "Front and back hemispheres",
                     "Neural networking", "Cosmic brainstorms", "Megatherapy", "Synaptic lubricant", "Psychokinesis",
                     "Spines"],
    "You": ["Cloning vats", "Energized nutrients", "Stunt doubles", "Clone recycling plant", "Free-range clones",
            "Genetic tailoring", "Power in diversity", "Self-betterment", "Source control", "United workforce"],
}
BUILDINGS = ["Cursor", "Grandma", "Farm", "Mine", "Factory", "Bank", "Temple", "Wizard tower", "Shipment",
             "Alchemy lab", "Portal", "Time machine", "Antimatter condenser", "Prism", "Chancemaker", "Fractal engine",
             "Javascript console", "Idleverse", "Cortex baker", "You"]
# the golden cookie's building special, for each building
SPECIAL = ["High-five", "Congregation", "Luxuriant harvest", "Ore vein", "Oiled-up", "Juicy profits",
           "Fervent adoration", "Manabloom", "Delicious lifeforms", "Breakthrough", "Righteous cataclysm",
           "Golden ages", "Extra cycles", "Solar flare", "Winning streak", "Macrocosm", "Refactoring",
           "Cosmic nursery", "Brainstorm", "Deduplication"]

# ------------------------------------------------------------------ achievements, in the order of the save
BANK = ["Wake and bake", "Making some dough", "So baked right now", "Fledgling bakery", "Affluent bakery",
        "World-famous bakery", "Cosmic bakery", "Galactic bakery", "Universal bakery", "Timeless bakery",
        "Infinite bakery", "Immortal bakery", "Don't stop me now", "You can stop now", "Cookies all the way down",
        "Overdose", "How?", "The land of milk and cookies", "He who controls the cookies controls the universe",
        "Tonight on Hoarders", "Are you gonna eat all that?", "We're gonna need a bigger bakery",
        "In the mouth of madness"]
CPS = ["Casual baking", "Hardcore baking", "Steady tasty stream", "Cookie monster", "Mass producer", "Cookie vortex",
       "Cookie pulsar", "Cookie quasar", "Oh hey, you're still here", "Let's never bake again",
       "A world filled with cookies", "When this baby hits 36 quadrillion cookies per hour", "Fast and delicious",
       "Cookiehertz : a really, really tasty hertz", "Woops, you solved world hunger", "Turbopuns", "Faster menner",
       "And yet you're still hungry", "The Abakening"]
CLICK = ["Clicktastic", "Clickathlon", "Clickolympics", "Clickorama", "Clickasmic", "Clickageddon", "Clicknarok",
         "Clickastrophe", "Clickataclysm", "The ultimate clickdown"]
CURSOR_ACH = ["Click", "Double-click", "Mouse wheel", "Of Mice and Men", "The Digital"]
BUILDING_ACH = {
    "Grandma": ["Grandma's cookies", "Sloppy kisses", "Retirement home"],
    "Farm": ["Bought the farm", "Reap what you sow", "Farm ill"],
    "Mine": ["You know the drill", "Excavation site", "Hollow the planet"],
    "Factory": ["Production chain", "Industrial revolution", "Global warming"],
    "Bank": ["Pretty penny", "Fit the bill", "A loan in the dark"],
    "Temple": ["Your time to shrine", "Shady sect", "New-age cult"],
    "Wizard tower": ["Bewitched", "The sorcerer's apprentice", "Charms and enchantments"],
    "Shipment": ["Expedition", "Galactic highway", "Far far away"],
    "Alchemy lab": ["Transmutation", "Transmogrification", "Gold member"],
    "Portal": ["A whole new world", "Now you're thinking", "Dimensional shift"],
    "Time machine": ["Time warp", "Alternate timeline", "Rewriting history"],
    "Antimatter condenser": ["Antibatter", "Quirky quarks", "It does matter!"],
    "Prism": ["Lone photon", "Dazzling glimmer", "Blinding flash"],
    "Chancemaker": ["Lucked out", "What are the odds", "Grandma needs a new pair of shoes"],
    "Fractal engine": ["Self-contained", "Threw you for a loop", "The sum of its parts"],
    "Javascript console": ["F12", "Variable success", "No comments"],
    "Idleverse": ["They'll never know what hit 'em", "Well-versed", "Ripe for the picking"],
    "Cortex baker": ["It's big brain time", "Just my imagination", "Now there's an idea"],
    "You": ["My own clone", "Multiplicity", "Born for this job"],
}
GOLDEN = ["Golden cookie", "Lucky cookie", "A stroke of luck", "Fortune", "Leprechaun", "Black cat's paw"]
OWNED = ["Builder", "Architect", "Engineer", "Lord of Constructs"]
UPGRADED = ["Enhancer", "Augmenter", "Upgrader", "Lord of Progress"]
# (their conditions are in the game, in this order)
MISC = ["One with everything", "Centennial", "Just wrong", "Neverclick", "True Neverclick", "Uncanny clicker",
        "Early bird", "Fading luck", "Elder", "Rebirth", "Sacrifice", "Oblivion", "From scratch", "You win a cookie"]
MISC_DESC = ["Have at least 1 of every building.", "Have at least 100 of everything.", "Sell a grandma.",
             "Make 1 million cookies by only having clicked 15 times.",
             "Make 1 million cookies with no cookie clicks.", "Click really, really fast.",
             "Click a golden cookie less than 1 second after it spawns.",
             "Click a golden cookie less than 1 second before it dies.", "Own at least 7 grandma types.",
             "Ascend at least once.", "Ascend with 1 million cookies baked.", "Ascend with 1 billion cookies baked.",
             "Ascend with 1 trillion cookies baked.",
             "This is for baking 100 trillion cookies and making it on the local news."]
MILK = ["Plain milk", "Chocolate milk", "Raspberry milk", "Orange milk", "Caramel milk", "Banana milk", "Lime milk"]

# ------------------------------------------------------------------ the news
LORE = [  # as the bakery grows: two for each power of ten from 1,000 cookies on
    "You feel like making cookies. But nobody wants to eat your cookies.",
    "Your first batch goes to the trash. The neighborhood raccoon barely touches it.",
    "Your family agrees to try some of your cookies.",
    "Your cookies are popular in the neighborhood.", "People are starting to talk about your cookies.",
    "Your cookies are talked about for miles around.", "Your cookies are renowned in the whole town!",
    "Your cookies bring all the boys to the yard.", "Your cookies now have their own website!",
    "Your cookies are worth a lot of money.", "Your cookies sell very well in distant countries.",
    "People come from very far away to get a taste of your cookies.",
    "Kings and queens from all over the world are enjoying your cookies.",
    "There are now museums dedicated to your cookies.", "A national day has been created in honor of your cookies.",
    "Your cookies have been named a part of the world wonders.",
    "History books now include a whole chapter about your cookies.",
    "Your cookies have been placed under government surveillance.", "The whole planet is enjoying your cookies!",
    "Strange creatures from neighboring planets wish to try your cookies.",
    "Elder gods from the whole cosmos have awoken to taste your cookies.",
    "Beings from other dimensions lapse into existence just to get a taste of your cookies.",
    "Your cookies have achieved sentience.", "The universe has now turned into cookie dough, to the molecular level.",
    "Your cookies are rewriting the fundamental laws of the universe.",
    "A local news station runs a 10-minute segment about your cookies. Success! (you win a cookie)",
    "it's time to stop playing",
]
GRANNY = ["\"Moist cookies.\" - grandma", "\"We're nice grandmas.\" - grandma", "\"Indentured servitude.\" - grandma",
          "\"Come give grandma a kiss.\" - grandma", "\"Why don't you visit more often?\" - grandma",
          "\"Call me...\" - grandma"]
BUILDING_NEWS = {  # one for each building from the farm on
    "Farm": "News : cookie farms suspected of employing undeclared elderly workforce!",
    "Mine": "News : chocolate mines found to cause earthquakes and sinkholes!",
    "Factory": "News : cookie factories linked to global warming!",
    "Bank": "News : cookie loans on the rise as people can no longer afford them with regular money.",
    "Temple": "News : theists of the world discover new cookie religion - \"Oh boy, guess we were wrong all along!\"",
    "Wizard tower": "News : get your new charms and curses at the yearly National Spellcrafting Fair! Exclusive prices "
                    "on runes and spellbooks.",
    "Shipment": "News : new chocolate planet found, becomes target of cookie-trading spaceships!",
    "Alchemy lab": "News : national gold reserves dwindle as more and more of the precious mineral is turned to "
                   "cookies!",
    "Portal": "News : nation worried as more and more unsettling creatures emerge from dimensional portals!",
    "Time machine": "News : time machines involved in history-rewriting scandal! Or are they?",
    "Antimatter condenser": "News : first antimatter condenser successfully turned on, doesn't rip apart reality!",
    "Prism": "News : cookies now being baked at the literal speed of light thanks to new prismatic contraptions.",
    "Chancemaker": "News : strange statistical anomalies continue as weather forecast proves accurate an "
                   "unprecedented 3 days in a row!",
    "Fractal engine": "News : local man sails around the world to find himself - right where he left it.",
    "Javascript console": "News : nation holds breath as nested ifs about to hatch.",
    "Idleverse": "News : is another you living out their dreams in an alternate universe? Probably, you lazy bum!",
    "Cortex baker": "News : are you smarter than a cortex baker? New game show deemed \"unfair\" by contestants.",
    "You": "News : the person of the year is, this year again, you! How unexpected!",
}
MISC_NEWS = [  # once the bakery has made 10,000 cookies
    "News : man robs bank, buys cookies.",
    "News : doctors recommend twice-daily consumption of fresh cookies.",
    "News : \"all-you-can-eat\" cookie restaurant opens in big city; waiters trampled in minutes.",
    "News : cookie shortage strikes town, people forced to eat cupcakes; \"just not the same\", concedes mayor.",
    "News : \"Ook\", says interviewed orangutan.",
    "News : new cookie-based religion sweeps the nation.",
    "News : what do golden cookies taste like? Study reveals a flavor \"somewhere between spearmint and liquorice\".",
    "News : 97-year-old baker still makes cookies the old-fashioned way!",
]
KITTEN_NEWS = "News : faint meowing heard around local cookie facilities; suggests new ingredient being tested."

# ------------------------------------------------------------------ the store's pictures, 12 x 12
ICONS = {
    "cursor": """
X...........
XX..........
XXX.........
XXXX........
XXXXX.......
XXXXXX......
XXXXXXX.....
XXXXXXXX....
XXXXX.......
XX.XXX......
X...XX......
.....XX.....""",
    "grandma": """
....XXXX....
...XXXXXX...
.XXXXXXXXXX.
.XX......XX.
.X.XX..XX.X.
.XX..XX..XX.
.X........X.
.X..XXXX..X.
..X......X..
...XXXXXX...
..XXX..XXX..
.XXXXXXXXXX.""",
    "farm": """
.....XX.....
...X.XX.X...
..XXXXXXXX..
.X..XXXX..X.
..XXXXXXXX..
...X.XX.X...
.....XX.....
..XX.XX.XX..
...XXXXXX...
.....XX.....
.....XX.....
XXXXXXXXXXXX""",
    "mine": """
..XXXXXXXX..
.XXXXXXXXXX.
XX...XX...XX
X....XX....X
.....XX.....
.....XX.....
.....XX.....
.....XX.....
.....XX.....
.....XX.....
.....XX.....
.....XX.....""",
    "factory": """
.XX.........
.XX.........
.XX.........
.XX..X...X..
.XX.XX..XX..
.XXXXX.XXX..
XXXXXXXXXXXX
XX.XX.XX.XXX
XX.XX.XX.XXX
XXXXXXXXXXXX
XXXXXXXXXXXX
XXXXXXXXXXXX""",
    "bank": """
.....XX.....
...XXXXXX...
.XXXXXXXXXX.
XXXXXXXXXXXX
............
.XX.XX.XX.XX
.XX.XX.XX.XX
.XX.XX.XX.XX
.XX.XX.XX.XX
............
XXXXXXXXXXXX
XXXXXXXXXXXX""",
    "temple": """
.....XX.....
....XXXX....
....X..X....
...XXXXXX...
...XXXXXX...
..XXXXXXXX..
..XXX..XXX..
.XXXX..XXXX.
.XXXXXXXXXX.
XXXXXXXXXXXX
XXXXXXXXXXXX
............""",
    "wizard": """
......XX....
.....XXX....
.....XXXX...
....XXX.X...
....XX.XX...
...XXXXXXX..
...XX.XXXX..
..XXXXXX.XX.
..XXXXXXXXX.
XXXXXXXXXXXX
.XXXXXXXXXX.
............""",
    "shipment": """
.....XX.....
....XXXX....
....XXXX....
...XX..XX...
...XX..XX...
...XXXXXX...
...XXXXXX...
..XXXXXXXX..
.XXXXXXXXXX.
XX.XXXXXX.XX
X...X..X...X
....X..X....""",
    "alchemy": """
....XXXX....
.....XX.....
.....XX.....
.....XX.....
....X..X....
...X....X...
..X......X..
.X.XXXXXX.X.
X.XXXXXXXX.X
X.XXXXXXXX.X
X.XXXXXXXX.X
.XXXXXXXXXX.""",
    "portal": """
...XXXXXX...
..XX....XX..
.XX..XX..XX.
XX..X..X..XX
X..X....X..X
X..X....X..X
X..X....X..X
X..X....X..X
XX..X..X..XX
.XX..XX..XX.
..XX....XX..
...XXXXXX...""",
    "time": """
XXXXXXXXXXXX
.X........X.
.XX......XX.
..XXXXXXXX..
...XXXXXX...
....XXXX....
....X..X....
...X....X...
..X..XX..X..
.X.XXXXXX.X.
.XXXXXXXXXX.
XXXXXXXXXXXX""",
    "antimatter": """
.....XX.....
..XX.XX.XX..
.X..X..X..X.
.X.X....X.X.
..XX.XX.XX..
XXX.XXXX.XXX
XXX.XXXX.XXX
..XX.XX.XX..
.X.X....X.X.
.X..X..X..X.
..XX.XX.XX..
.....XX.....""",
    "prism": """
.....XX.....
.....XX.....
....XXXX....
....XXXX....
...XX..XX...
...XX..XX...
..XX....XX..
..XX....XX..
.XX......XX.
.XX......XX.
XXXXXXXXXXXX
XXXXXXXXXXXX""",
    "chance": """
..XXX..XXX..
.XXXXX.XXXX.
.XXXXXXXXXX.
..XXXXXXXX..
XXXXX..XXXXX
XXXX....XXXX
XXXX....XXXX
XXXXX..XXXXX
..XXXXXXXX..
.XXXXXXXXXX.
.XXXX.XXXXX.
..XX...XXX..""",
    "fractal": """
.....XX.....
....XXXX....
....X..X....
...XXXXXX...
..XX....XX..
..XXX..XXX..
.X..X..X..X.
.XXXXXXXXXX.
XX........XX
XXX......XXX
X..X....X..X
XXXXXXXXXXXX""",
    "console": """
XXXXXXXXXXXX
X..........X
X.X........X
X..X.......X
X...X......X
X..X.......X
X.X........X
X.....XXXX.X
X..........X
XXXXXXXXXXXX
............
............""",
    "idleverse": """
....XXXX....
..XXXXXXXX..
.XXXXXXXXXX.
.XXXXXXXXXX.
XXXXXXXXXXXX
............
XXXXXXXXXXXX
.XXXXXXXXXX.
.XXXXXXXXXX.
..XXXXXXXX..
....XXXX....
............""",
    "cortex": """
...XX.XX....
..XXXXXXXX..
.XX.XX.XXXX.
XXXXX.XX.XXX
XX.XXXXXXX.X
XXXX.XX.XXXX
.XXXXXXXXXX.
..XX.XXXX...
.....XX.....
.....XX.....
............
............""",
    "you": """
....XXXX....
...XXXXXX...
...XXXXXX...
...XXXXXX...
....XXXX....
............
..XXXXXXXX..
.XXXXXXXXXX.
XXXXXXXXXXXX
XXXXXXXXXXXX
XXXXXXXXXXXX
............""",
    "mouse": """
....XXXX....
...X.XX.X...
..X..XX..X..
..X..XX..X..
..XXXXXXXX..
..X......X..
..X......X..
..X......X..
..X......X..
...X....X...
....XXXX....
............""",
    "kitten": """
X..........X
XX........XX
XXX......XXX
XXXXXXXXXXXX
XXXXXXXXXXXX
XX..XXXX..XX
XX..XXXX..XX
XXXXXXXXXXXX
XXXXX..XXXXX
.XXXXXXXXXX.
..XXXXXXXX..
............""",
}


def icon_bytes(art):
    rows = art.strip().split("\n")
    assert len(rows) == 12 and all(len(r) == 12 for r in rows), art
    bits = [c == "X" for r in rows for c in r]
    out = []
    for i in range(0, 144, 8):  # lowest bit first
        out.append(sum(1 << k for k in range(8) if bits[i + k]))
    return out


# How much of the game fits in NumPlay (13 KB): the upgrades, achievements and news that come first, so
# far that milk (an achievement is 4%) takes the kittens to the engineers
N = dict(cursor=10, mouse=8, kitten=3, cookie=16, bank=16, cps=12, click=6, cursor_ach=3, golden=4, owned=2,
         upgraded=2, milk=3)
MISC_KEPT = [0, 3, 5, 6, 8]  # one with everything, neverclick, uncanny clicker, early bird, elder


def main():
    order = [b for b in BUILDINGS[1:]]
    cookies = COOKIES[:N["cookie"]]
    misc = [MISC[i] for i in MISC_KEPT]
    ups = CURSOR[:N["cursor"]] + MOUSE[:N["mouse"]] + KITTEN[:N["kitten"]] + GOLD + GRANDMA + [c[0] for c in cookies]
    for b in order:
        ups += TIERED[b][:TIER_N]
    achs = BANK[:N["bank"]] + CPS[:N["cps"]] + CLICK[:N["click"]] + CURSOR_ACH[:N["cursor_ach"]]
    for b in order:
        achs += BUILDING_ACH[b][:ACH_TIERS]
    achs += GOLDEN[:N["golden"]] + OWNED[:N["owned"]] + UPGRADED[:N["upgraded"]] + misc
    news = [LORE[i] for i in [0, 1, 2] + list(range(3, 27, 2))]  # a line for each power of ten
    assert len(MISC_DESC) == len(MISC)
    strings = ups + achs + news + [MISC_DESC[i] for i in MISC_KEPT] + MILK[:N["milk"]]
    text = "\0".join(strings).encode("ascii") + b"\0"
    z = zopfli.zlib.compress(text, numiterations=100)[2:-4]
    raw = len(text)
    L = ["/* Generated by tools/data.py: Cookie Clicker's names, news and the store's pictures. */",
         "#define TIER_N %d" % TIER_N, "#define ACH_TIERS %d" % ACH_TIERS,
         "/* the upgrades, in the order of the save */",
         "#define N_CURSOR %d" % N["cursor"], "#define U_MOUSE N_CURSOR", "#define N_MOUSE %d" % N["mouse"],
         "#define U_KITTEN (U_MOUSE + N_MOUSE)", "#define N_KITTEN %d" % N["kitten"],
         "#define U_GOLD (U_KITTEN + N_KITTEN)", "#define U_GRANDMA (U_GOLD + 3)",
         "#define U_COOKIE (U_GRANDMA + 18)", "#define N_COOKIE %d" % len(cookies),
         "#define U_TIER (U_COOKIE + N_COOKIE)", "#define N_UP (U_TIER + 19 * TIER_N)",
         "/* the achievements, in the order of the save */",
         "#define N_BANK %d" % N["bank"], "#define A_CPS N_BANK", "#define N_CPS %d" % N["cps"],
         "#define A_CLICK (A_CPS + N_CPS)", "#define N_CLICK %d" % N["click"],
         "#define A_BUILD (A_CLICK + N_CLICK) /* the cursor's, then ACH_TIERS for each other building */",
         "#define N_CURSOR_ACH %d" % N["cursor_ach"],
         "#define A_GOLDEN (A_BUILD + N_CURSOR_ACH + 19 * ACH_TIERS)", "#define N_GOLDEN %d" % N["golden"],
         "#define A_OWNED (A_GOLDEN + N_GOLDEN)", "#define A_UPGRADED (A_OWNED + %d)" % N["owned"],
         "#define A_MISC (A_UPGRADED + %d)" % N["upgraded"], "#define N_ACH (A_MISC + %d)" % len(misc),
         "/* the texts: the upgrades' names, the achievements', the news, the other achievements'",
         "   descriptions, the milks */",
         "#define X_ACH N_UP", "#define X_NEWS (X_ACH + N_ACH)",
         "#define N_LORE 15",
         "#define X_MDESC (X_NEWS + N_LORE)", "#define X_MILK (X_MDESC + %d)" % len(misc),
         "#define N_MILK %d" % N["milk"], "#define N_TEXT %d" % len(strings), "#define TEXT_SIZE %d" % raw]
    COOKIES[:] = cookies
    # a cookie upgrade: its % in the top 3 bits, then 5 if it is so many nines times 5, then the nines
    L.append("static const uint8_t cookie_up[N_COOKIE] = {%s};" %
             ", ".join(str(p << 5 | x5 << 4 | (n - 5)) for _, p, n, x5 in COOKIES))
    assert all(1 <= p <= 7 and 5 <= n < 21 for _, p, n, _ in COOKIES)
    names = list(ICONS)
    L.append("enum { %s };" % ", ".join("IC_" + n.upper() for n in names))
    data = [b for n in names for b in icon_bytes(ICONS[n])]
    L.append("static const uint8_t icons[%d][18] = {" % len(names))
    for i in range(len(names)):
        L.append("  {%s}," % ", ".join(str(b) for b in data[18 * i:18 * i + 18]))
    L.append("};")
    L.append("static const uint8_t text_z[%d] = {" % len(z))
    for i in range(0, len(z), 24):
        L.append("  " + ", ".join(str(b) for b in z[i:i + 24]) + ",")
    L.append("};")
    open(OUT, "w").write("\n".join(L) + "\n")
    print(f"data.h: {len(ups)} upgrades, {len(achs)} achievements, {len(news)} news; "
          f"text {raw} bytes, packed {len(z)}; icons {len(data)} bytes")


if __name__ == "__main__":
    main()
