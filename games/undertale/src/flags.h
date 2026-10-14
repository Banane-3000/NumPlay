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
  FLAG_COUNT = 256
};
#endif
