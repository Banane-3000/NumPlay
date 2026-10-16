/* Undertale: what the game remembers (the original's global.flag), named.
 * Saved: new flags go at the end, before FLAG_COUNT. */
#ifndef UT_FLAGS_H
#define UT_FLAGS_H
enum {
  F_NONE,
  /* the Ruins */
  F_FLOWEY_MET,          /* the first meeting is over */
  F_TORIEL_MET,
  F_RUINS1_DONE,         /* Toriel waited at the entrance's stairs */
  F_RUINS2_DONE,         /* the middle road puzzle */
  F_RUINS3_SW1, F_RUINS3_SW2, F_RUINS3_DONE,
  F_DUMMY,               /* 1 talked, 2 fought, 3 fled, 4 bored */
  F_RUINS5_DONE,         /* the spikes, hand in hand */
  F_RUINS6_DONE,         /* the independence test */
  F_PHONE,               /* Toriel's cell phone */
  F_LEAFPILE_CALL,
  F_CANDY_TAKEN,         /* Monster Candy taken (to 4) */
  F_CANDY_SPILT,
  F_RUINS9_ROCK,         /* the first rock pushed home */
  F_FLAVOR_CALL, F_FLAVOR,   /* 1 cinnamon, 2 butterscotch */
  F_RUINS11_ROCK,        /* the second rock puzzle: the reluctant rock moved */
  F_RUINS11_ASK,
  F_NAPSTA,              /* 1 spared, 2 killed, 3 fled */
  F_DONUT_BOUGHT, F_CIDER_BOUGHT,
  F_ITEM_CALL,
  F_FROGGIT_YELLOW,      /* 0 yellow names, 1 white, 2 pink */
  F_RIBBON, F_TOYKNIFE,
  F_ONESWITCH, F_ROTATE1, F_ROTATE2, F_ROTATE3, F_ROTATE4,
  F_ALLERGY_CALL,
  F_TORIEL_HOME,         /* Toriel found Frisk at her tree */
  F_PIE,                 /* the pie slice taken */
  F_TORIEL_TALKS,        /* questions asked her by the fire */
  F_TORIEL_BASEMENT,     /* she went downstairs */
  F_BASEMENT_TALK,       /* 1..3: what she said along the way */
  F_TORIEL_FIGHT,        /* 1 spared, 2 killed */
  F_FLOWEY_END,
  F_RUINS_EXIT,
  F_KILLS_RUINS,
  F_SKIPS,               /* text pages skipped with X (a Froggit notices) */
  F_FROG_TIP,
  F_SAVE_SEEN,
  F_FALL_LEAVES,
  F_SPIDER_CALL,
  F_ASRIEL_ROOM,
  F_MIRROR,
  F_LIGHTS,
  F_RUINS_SAVE_BED,
  F_HARDMODE,
  F_FIRST_FROGGIT,
  F_TORIEL_SNAIL,
  F_WHIMSUN_TALK,
  F_CALL_HELLO, F_CALL_MOM, F_CALL_FLIRT,
  F_NAPSTA_LIE,          /* Napstablook went away from the room under the one switch puzzle */
  F_HOME_TOUR,           /* Toriel showed Frisk the house */
  F_SNAIL_TALK,
  F_BED_SLEPT,           /* 1 slept; 2 the pie taken */
  /* Snowdin */
  F_BRANCH,              /* the branch broke behind Frisk */
  F_SANS_MET,            /* the gate, the lamp: Sans and Papyrus met */
  F_SANS_FAVOR,          /* Sans asked his favour */
  F_PAP_SENTRY,          /* Papyrus saw the human (not a rock) */
  F_DOGGO,               /* 1 spared, 2 killed */
  F_SANS_HINT,           /* the blue stop sign */
  F_SNOWMAN,             /* pieces taken */
  F_ELECMAZE,            /* 1 Papyrus zapped and gave the orb, 2 crossed */
  F_SANS_COOL,           /* 1 cool, 2 uncool */
  F_NICECREAM,           /* the Nice Cream guy met */
  F_FRIED_SNOW,          /* times Sans named his price */
  F_BALLGAME,            /* the flag won (1..7) */
  F_WORDSEARCH,          /* 1 crossword, 2 junior jumble */
  F_SPAGHETTI,
  F_SNOWPUZZLE,          /* the switch under the snow */
  F_DOGI,                /* 1 spared, 2 killed */
  F_XOXO1, F_XOXO2, F_XOXO3,
  F_PAP_XOXO1,           /* asked about the spaghetti */
  F_TILES,               /* the multicolor tiles explained */
  F_LESSERDOG,           /* 1 spared, 2 killed */
  F_POFF_GOLD,
  F_GREATERDOG,          /* 1 spared, 2 killed */
  F_GAUNTLET,
  F_SNOW_SANS_TALK,      /* Sans's talks along the way, a bit each */
  F_PAPYRUS,             /* 1 spared, 2 killed, 3 fled */
  F_PAP_CAPTURED,        /* times Papyrus captured Frisk */
  F_PAP_DATE,            /* 1 promised, 2 done */
  F_GRILLBYS,
  F_INN,                 /* nights at the inn */
  F_KILLS_SNOWDIN,
  F_BOX0, F_BOX1, F_BOX2, F_BOX3, F_BOX4, F_BOX5, F_BOX6, F_BOX7, F_BOX8, F_BOX9,   /* the Dimensional Box */
  F_SNOW_EXIT,
  F_TOWN_HOUSE,          /* Papyrus's house open */
  F_SNOWDIN_BOSSES,      /* boss fights (Bisicle's price) */
  F_SNOW_SANS_LAMP,      /* hidden behind the lamp */
  F_COUCH_GOLD,
  F_FISH,                /* the fishing rod's note read */
  F_SHOP_TALK,
  F_SNOWDRAKE_DEAD,      /* (then Chilldrake comes instead) */
  /* Waterfall */
  F_WF_MK,               /* Monster Kid met by the first echo flower */
  F_WF_GRILLBYS,         /* Sans took Frisk to Grillby's from his station */
  F_WF_TUTU, F_WF_QUICHE, F_WF_BALLET, F_WF_PUNCH, F_WF_ARTIFACT,   /* found */
  F_WF_UNDYNE1,          /* Undyne seen from the tall grass */
  F_SEEDS1, F_SEEDS2, F_SEEDS3,   /* the bridges of bridge seeds, made */
  F_CHASE1,              /* Undyne's first chase, over */
  F_WF_TELESCOPE,
  F_WF_ONION,            /* 1 met, 2 parted */
  F_PIANO,               /* its song played: the door is open */
  F_UMBRELLA,            /* 1 carried, 2 left with the statue */
  F_MK_RAIN,             /* Monster Kid walked with Frisk in the rain */
  F_SHYREN,              /* 1 spared, 2 killed */
  F_WF_BRIDGE,           /* the bridge cut: Frisk fell */
  F_KILLS_WATERFALL,
  F_WF_BIRD,             /* the small bird: 1 it carried Frisk west (it waits there), 2 back east */
  F_WF_DUMP,             /* woke up in the garbage dump */
  F_WF_COOLER,           /* the cooler's Astronaut Food taken: how many */
  F_MAD_DUMMY,           /* its fight, over (Napstablook came) */
  F_NAPSTA_HOME,         /* 1 invited home, 2 lay on the floor with them */
  F_SNAIL_RACE,          /* Thundersnail races run */
  F_TEM_VILLAGE,         /* found */
  F_WF_EAST1,            /* (unused: the mushroom maze's far side once ended the game) */
  F_DEADEND,             /* Undyne at the dead end, Monster Kid in her way: the way north lit */
  F_MK_CHASM,            /* Monster Kid on the chasm's bridge: 1 hanging, 2 helped up, 3 let fall, 4 left behind */
  F_UNDYNE,              /* her fight: 1 fled (she chases), 2 killed, 3 fallen in the heat, 4 given water */
  F_WATER_CUP,           /* a cup of the cooler's water: 1 carried, 2 poured */
  F_PAP_NEON,            /* Papyrus's call by the neon sign */
  F_HOTLAND,             /* (unused: Hotland's first room once ended the game) */
  /* Hotland */
  F_HT_LAB,              /* the Lab: 1 Alphys met (Mettaton's quiz), 2 the phone upgraded */
  F_HT_NOODLES,          /* the Instant Noodles from Alphys's fridge */
  F_HT_PAN,              /* the Burnt Pan taken */
  F_HT_CALL_LASER,       /* Alphys's calls: the lasers, the puzzle hub, the vents past the door, the CORE */
  F_HT_CALL_HUB,
  F_HT_CALL_ISLE,
  F_HT_CALL_CORE,
  F_HT_PUZ_R,            /* the shooting puzzles solved: the right one, the left one */
  F_HT_PUZ_L,
  F_HT_BLUE_OFF,         /* the blue laser before the left puzzle switched off */
  F_HT_COOK,             /* the cooking show: 1 the substitute reached, 2 out of time */
  F_HT_FLOOR1,           /* the elevator up from Hotland's first floor taken */
  F_HT_HEATS,            /* Heats Flamesman's name heard */
  F_HT_APRON,            /* the Stained Apron taken */
  F_HT_CALL_BELT,        /* Alphys's calls: the belt puzzle, by the CORE again */
  F_HT_CALL_CORE2,
  F_HT_CALL_BOZ,
  F_HT_BELT,             /* the belt puzzle's force field off */
  F_HT_VENTPUZ,          /* the vent puzzle crossed */
  F_HT_GUARDS,           /* the Royal Guards: 1 spared, 2 killed */
  F_HT_NEWS,             /* Mettaton's news report: 1 every bomb defused, 2 out of time */
  F_HT_FLOOR2,           /* the elevator up from the second floor taken */
  F_HT_CALL_HUB3,        /* Alphys's call by the third floor's vents */
  F_HT3_PUZ_N,           /* the third floor's shooting puzzles: north, south */
  F_HT3_PUZ_S,
  F_HT_ANIME,            /* Alphys asked Frisk to watch anime with her: 1 yes, 2 no */
  F_HT_MUFFET,           /* Muffet: 1 spared, 2 killed */
  F_HT_MUSICAL,          /* Mettaton's musical seen (and the trapdoor) */
  F_HT_TILES3,           /* the tile puzzle under the stage: 1 solved, 2 out of time; then Mettaton fought */
  F_HT_HEATS3,           /* Heats Flamesman asked about his name */
  F_HT_FLOOR3,           /* the MTT Resort reached */
  F_RES_SANS,            /* dinner with Sans at the Resort */
  F_RES_TEA,             /* room service brought: the Sea Tea (door 1), the Cinnamon Bunny (door 3) */
  F_RES_BUN,
  F_RES_ROOM,            /* the hotel's room: 1 paid for, 2 slept in */
  F_RES_DOG,             /* the Hot Dog slid under door 4 (for a Hush Puppy) */
  F_CORE_IN,             /* the CORE's approach: the black shapes seen backing away inside */
  F_CORE_CALLS,          /* Alphys's calls in the CORE, a bit each (core.c's CALL_*), then the rest */
  F_CORE_CALLS2,
  F_CORE_MADJICK,        /* the doorway's ambush, the room north of the crossroads: 1 spared, 2 killed */
  F_CORE_KNIGHT,
  F_CORE_SWITCH,         /* the switch by the laser ambush pressed */
  F_CORE_POWER,          /* the laser bridge: 1 its power cut, 2 crossed */
  F_CORE_SAGE,           /* the sage's path's puzzle solved */
  F_CORE_WARRIOR,        /* the warrior's path: its fights won (1 to 3), 4 its switch flipped */
  F_CORE_FIELD,          /* the force field before The End down */
  F_CORE_TRASH1,         /* the trash cans: the Glamburger, the 100G */
  F_CORE_TRASH2,
  F_CORE_ASTIG,          /* Astigmatism met on its own (just once) */
  F_CORE_END,            /* the Core End reached (the lobby's elevator works) */
  F_CORE_MTT,            /* Mettaton EX: 1 his show a hit, 2 killed */
  F_CORE_HALL,           /* Alphys's talk by the last elevator */
  F_CORE_DONE,           /* the elevator up to the castle taken (where the game now ends) */
  FLAG_COUNT = 256
};
#endif
