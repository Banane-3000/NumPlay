/* Hotland's monsters: stats and ACTs (from the Undertale Wiki's records of the game), their formations, where they
 * come out and their attacks. Their words are short ones of our own. */
#include <stdio.h>
#include "battle.h"
#include "game.h"
#include "items.h"
#include "gfx.h"
#include "monsters.h"
#include "res.h"
#include "world.h"

/* ---------------------------------------------------------------- Vulkin: it means well (its lava doesn't heal) */
static void act_vulkin(Battle *b, Monster *m, int act, char *t, int cap) {
  (void)b;
  if (act == 0) {   /* Criticize */
    snprintf(t, (size_t)cap, "* You tell Vulkin its lava isn't helping.\n* Vulkin looks crushed. It stops attacking.");
    m->state = 2;
  } else if (act == 1) {   /* Encourage */
    snprintf(t, (size_t)cap, "* You tell Vulkin it's doing great.\n* Vulkin glows with pride... and heat.");
    m->at = (int16_t)(m->at + 2), m->spare = true;
    if (m->state != 2) m->state = 1;
  } else {   /* Hug */
    snprintf(t, (size_t)cap, "* You hug Vulkin.\n* It's very, very warm.\n* Your DEFENSE dropped.");
    m->spare = true;
    if (m->state != 2) m->state = 1;
    b->df_loss += 5;
  }
}
static const char *talk_vulkin(Battle *b, Monster *m) {
  (void)b;
  if (m->state == 2) return "(sniff)";
  if (m->state == 1) return "Feeling\nbetter?";
  static const char *const lines[] = {"I'll heal\nyou!", "Lava\nhelps!", "Warm\nhugs!", "Here, take\nsome lava!"};
  return lines[rnd_n(4)];
}
static int attack_vulkin(Battle *b, Monster *m) {
  (void)b;
  if (m->state == 2) return ATK_NONE;
  return rnd_n(2) ? ATK_VULKIN_THUNDER : ATK_VULKIN_FIRE;
}

/* ---------------------------------------------------------------- Tsunderplane: come closer, through its green */
static void act_tsunder(Battle *b, Monster *m, int act, char *t, int cap) {
  (void)b;
  if (act == 0) {   /* Flirt */
    snprintf(t, (size_t)cap, "* You flirt with Tsunderplane.\n* It turns away, blushing.");
  } else {   /* Approach */
    snprintf(t, (size_t)cap, "* You move a little closer.\n* Tsunderplane pretends not to notice.");
    m->state = 1;
  }
}
static const char *talk_tsunder(Battle *b, Monster *m) {
  (void)b;
  if (m->spare) return "I-it's not\nlike I like\nyou!!";
  if (m->state) return "Don't get\nthe wrong\nidea!";
  static const char *const lines[] = {"Hmph!", "Baka!", "Wh-what are\nyou looking\nat?", "As if!"};
  return lines[rnd_n(4)];
}
static const char *flavor_tsunder(Battle *b, Monster *m) {
  /* its green zones touched enough (four times, three with Vulkin by it): it can be spared */
  int need = 4;
  for (int i = 0; i < b->nmon; i++)
    if (b->mon[i].def == mon_def(MON_VULKIN) && !b->mon[i].gone) need = 3;
  if (m->state && b->greens >= need) m->spare = true;
  if (m->spare) return "* Tsunderplane is pretending it doesn't want to be spared.";
  return NULL;
}
static int attack_tsunder(Battle *b, Monster *m) {
  (void)b, (void)m;
  return rnd_n(2) ? ATK_TSUNDER_BOMBS : ATK_TSUNDER_DIVE;
}

/* ---------------------------------------------------------------- Pyrope: turn up the heat */
static void act_pyrope(Battle *b, Monster *m, int act, char *t, int cap) {
  (void)b;
  if (act == 0) {   /* Heat Up */
    m->state++;
    if (m->state >= 2) snprintf(t, (size_t)cap, "* You rub your hands together.\n* The room is sweltering!"), m->spare = true;
    else snprintf(t, (size_t)cap, "* You rub your hands together.\n* It's getting warmer...");
  } else if (act == 1) {   /* Cool Down */
    snprintf(t, (size_t)cap, "* You blow on Pyrope.\n* Its flame shrinks. Its ATTACK dropped!");
    m->at = (int16_t)max_i(1, m->at - 2);
  } else {   /* Invite */
    snprintf(t, (size_t)cap, "* You invite Pyrope to hang out.\n* It just wants to be warmer.");
  }
}
static const char *talk_pyrope(Battle *b, Monster *m) {
  (void)b;
  if (m->spare) return "Toasty!";
  static const char *const lines[] = {"Brr...", "Hotter!", "I'm all\nfired up!", "Got a\nlight?"};
  return lines[rnd_n(4)];
}
static int attack_pyrope(Battle *b, Monster *m) { (void)b, (void)m; return rnd_n(2) ? ATK_PYROPE_BOMBS : ATK_PYROPE_ROPE; }

/* ---------------------------------------------------------------- the Royal Guards: 01 and 02, always a team.
 * Clean 02's armor, and its green stars caught five times, 02 can't stand the heat and takes the armor off; then a
 * whisper to 01 to say what he feels, and the two can be spared */
enum { RG_CLEANED, RG_ARMOR_OFF, RG_TOLD };   /* (b->state[]) */
static Monster *rg_other(Battle *b, Monster *m) {
  for (int i = 0; i < b->nmon; i++)
    if (&b->mon[i] != m) return &b->mon[i];
  return m;
}
static bool rg_alone(Battle *b, Monster *m) { return rg_other(b, m)->killed; }
static void act_rg(Battle *b, Monster *m, int act, char *t, int cap) {
  bool one = m->def == mon_def(MON_RG01);
  Monster *o = rg_other(b, m);
  if (act == 0) {   /* Clean Armor */
    if (one) snprintf(t, (size_t)cap, "* You try to polish 01's armor.\n* Your hands slide right off.");
    else if (b->state[RG_ARMOR_OFF]) snprintf(t, (size_t)cap, "* You drum on 02's bare chest.\n* 01 can't look.");
    else {
      snprintf(t, (size_t)cap, "* You scrub 02's armor.\n* The grime starts to come off.\n* 01 looks bothered.");
      b->state[RG_CLEANED] = 1;
    }
  } else {   /* Whisper */
    if (!one) snprintf(t, (size_t)cap, "* You whisper a secret to 02.\n* 02 won't tell.");
    else if (!b->state[RG_ARMOR_OFF] || o->gone)
      snprintf(t, (size_t)cap, "* You tell 01 to be honest about his feelings.\n* 01 doesn't get it.");
    else {
      snprintf(t, (size_t)cap, "* You tell 01 to be honest about his feelings.");
      if (!b->state[RG_TOLD]) b->state[RG_TOLD] = 1;
      m->spare = o->spare = true;
    }
  }
}
static const char *talk_rg(Battle *b, Monster *m) {
  bool one = m->def == mon_def(MON_RG01);
  if (rg_alone(b, m)) return one ? "02...!\nThis one's\nfor you!" : "01...!\n...!!!";
  if (b->state[RG_TOLD] == 1) {
    if (one) return "02... I...\nI like,\nLIKE you,\nbro.";
    b->state[RG_TOLD] = 2;
    return "... want ice\ncream... after\nthis?";
  }
  if (b->state[RG_TOLD]) return one ? "Ice cream?\nSure, dude!" : "... heh.";
  if (b->state[RG_ARMOR_OFF]) return one ? "D-dude..." : "... much\nbetter.";
  if (b->state[RG_CLEANED]) return one ? "Hey! Hands\noff the\narmor!" : "... getting\n... hot.";
  static const char *const l01[] = {"Like, hand\nover the\nSOUL, bro.", "Like, you're\ntoast and\nstuff.", "Team\nattack!"};
  static const char *const l02[] = {"... ... hm.", "... ... sigh.", "... team\nattack."};
  return one ? l01[rnd_n(3)] : l02[rnd_n(3)];
}
static const char *flavor_rg(Battle *b, Monster *m) {
  (void)m;
  if (b->state[RG_CLEANED] && !b->state[RG_ARMOR_OFF] && b->greens >= 5) {
    b->state[RG_ARMOR_OFF] = 1;
    return "* 02 can't take the heat, and pulls the armor off!\n* 01 starts to sweat.";
  }
  if (b->state[RG_TOLD]) return "* 01 and 02 can't stop smiling at each other.";
  if (b->state[RG_ARMOR_OFF]) return "* 01 is shaking. 02 looks a lot cooler.";
  return NULL;
}
static int attack_rg(Battle *b, Monster *m) { (void)b, (void)m; return ATK_RG_TEAM; }

/* ---------------------------------------------------------------- Mettaton's bombs: defuse each in the DEFUSE ZONE.
 * Each moves its own way (the wiki's account of them), the zone in the middle; Defuse (or FIGHT) only works while
 * the bomb is in it */
int g_bomb_kind;
#define ZONE_X0 128
#define ZONE_X1 192
#define ZONE_Y0 34
#define ZONE_Y1 104
static int bomb_tri(int t, int period, int lo, int hi) {   /* a triangle wave, lo to hi and back */
  int p = t % period, half = period / 2;
  return lo + (hi - lo) * (p < half ? p : period - p) / half;
}
/* where the bomb's middle is, t frames into the battle */
static void bomb_pos(int t, int *x, int *y) {
  switch (g_bomb_kind) {
    case BOMB_DOG:   /* side to side, low, in waves */
      *x = 160 + 110 * gfx_sin(t / 2 % 256) / 1024, *y = 92 + 6 * gfx_sin(t * 3 % 256) / 1024;
      break;
    case BOMB_WATER:   /* fast, to the right, round again from the left */
      *x = 20 + t * 3 % 280, *y = 70;
      break;
    case BOMB_SCRIPT:   /* across and back, doubling back a little before each turn */
      *x = 160 + (95 * gfx_sin(t * 256 / 300 % 256) + 22 * gfx_sin(t * 768 / 300 % 256)) / 1024, *y = 66;
      break;
    case BOMB_BALL: {   /* bouncing from side to side */
      int b = gfx_sin(t * 256 / 50 % 256);
      *x = bomb_tri(t, 240, 50, 270), *y = 96 - 56 * (b < 0 ? -b : b) / 1024;
      break;
    }
    case BOMB_PRESENT:   /* up through the zone, round again from below */
      *x = 160, *y = 150 - t * 3 / 2 % 170;
      break;
    default: {   /* the game: side to side, keeping to the edges (across the middle in a rush) */
      int v = 240 * gfx_sin(t * 256 / 400 % 256) / 1024;
      *x = 160 + clamp_i(v, -115, 115), *y = 70;
      break;
    }
  }
}
static bool bomb_in_zone(Battle *b) {
  int x, y;
  bomb_pos((uint16_t)b->state[0], &x, &y);
  return x >= ZONE_X0 && x <= ZONE_X1 && y >= ZONE_Y0 && y <= ZONE_Y1;
}
static void bomb_defused(Monster *m) { m->gone = true, m->fade = 1, m->dmg_t = 0; }
static void act_bomb(Battle *b, Monster *m, int act, char *t, int cap) {
  (void)act;
  if (bomb_in_zone(b)) {
    bomb_defused(m);
    snprintf(t, (size_t)cap, g_bomb_kind == BOMB_DOG ? "* Dog defused!" : "* Bomb defused!");
  } else snprintf(t, (size_t)cap, "* Missed! Try while it's in\n  the DEFUSE ZONE.");
}
static bool hit_bomb(Battle *b, Monster *m, int dmg) {
  (void)dmg;
  if (bomb_in_zone(b)) bomb_defused(m);
  else m->dmg_shown = 0;   /* (MISS) */
  return true;
}
static const char *talk_bomb(Battle *b, Monster *m) {
  (void)b, (void)m;
  if (g_bomb_kind == BOMB_DOG) return "Zzz...";
  if (g_bomb_kind == BOMB_WATER) return "Swish.";
  return rnd_n(2) ? "Tick." : "Tock.";
}
static const char *flavor_bomb(Battle *b, Monster *m) {
  (void)b, (void)m;
  return g_bomb_kind == BOMB_DOG ? "* The dog is still active." : "* The bomb is still ticking.";
}
static int attack_bomb(Battle *b, Monster *m) { (void)b, (void)m; return -1; }
static void draw_bomb(Battle *b, Monster *m, int x0, int y0) {
  (void)y0;
  if (!m->gone) b->state[0]++;
  /* the zone */
  uint16_t g = rgb565(0x00C000);
  gfx_frame_rect(ZONE_X0 - 14, ZONE_Y0 - 14, ZONE_X1 - ZONE_X0 + 28, ZONE_Y1 - ZONE_Y0 + 28, 1, g);
  gfx_text(FONT_SMALL, 160 - gfx_text_width(FONT_SMALL, "DEFUSE ZONE", -1) / 2, ZONE_Y0 - 11, "DEFUSE ZONE", g, 255);
  int x, y;
  bomb_pos((uint16_t)b->state[0], &x, &y);
  x += x0 - m->x;   /* (its shake, hit) */
  uint16_t s = g_bomb_kind == BOMB_DOG ? SPR_DOGBOMB_B : g_bomb_kind == BOMB_WATER ? SPR_WATER_B : SPR_BOMB_B;
  int half = g_bomb_kind == BOMB_DOG ? 6 : g_bomb_kind == BOMB_WATER ? 11 : 23;
  uint8_t alpha = m->fade ? (uint8_t)max_i(0, 128 - m->fade * 4) : 255;
  gfx_sprite(s, (int)(g_frame / 8), x, y + half, 0, alpha);
}
/* ---------------------------------------------------------------- Muffet: purple strings, spiders, pastries, her
 * pet. She lets Frisk go once her spiders send word (at turn 17), or at once if Frisk eats one of their treats.
 * Paying her makes her hits weaker (a hit less each time, and the price doubles); struggling three times halves it */
enum { MF_PRICE, MF_STRUGGLE, MF_TREAT };   /* (b->state[]) */
static void act_muffet(Battle *b, Monster *m, int act, char *t, int cap) {
  if (!b->state[MF_PRICE]) b->state[MF_PRICE] = 100;
  if (act == 0) {   /* Pay */
    if (sv.gold >= b->state[MF_PRICE]) {
      sv.gold = (uint16_t)(sv.gold - b->state[MF_PRICE]);
      snprintf(t, (size_t)cap, "* You pay Muffet %dG.\n* Her attacks soften a little.", b->state[MF_PRICE]);
      m->at = (int16_t)max_i(1, m->at - 1);
      b->state[MF_PRICE] = (int16_t)min_i(b->state[MF_PRICE] * 2, 9000);
    } else {
      snprintf(t, (size_t)cap, "* You don't have %dG.\n* Muffet sighs, and lowers her price.", b->state[MF_PRICE]);
      b->state[MF_PRICE] = (int16_t)max_i(10, b->state[MF_PRICE] / 2);
    }
  } else {   /* Struggle */
    if (++b->state[MF_STRUGGLE] == 3) {
      snprintf(t, (size_t)cap, "* You struggle with all your might.\n* Muffet halves her price, laughing.");
      b->state[MF_PRICE] = (int16_t)max_i(10, b->state[MF_PRICE] / 2);
    } else snprintf(t, (size_t)cap, "* You struggle against the web.\n* It's too sticky to get free.");
  }
}
static const char *talk_muffet(Battle *b, Monster *m) {
  if (b->state[MF_TREAT] == 1) {
    b->state[MF_TREAT] = 2;
    return "Oh? You\'ve had\none of our\ntreats~";
  }
  if (m->spare) return "My mistake,\ndeary~\nRun along\nnow~";
  if (b->turn == 1) return "Purple\nsuits you\nmuch better,\ndeary~";
  if (b->turn == 3 || b->turn == 7 || b->turn == 11) {
    static const char *const meal[3] = {"Time for\nbreakfast,\nmy pet~", "Time for\nlunch,\nmy pet~", "Time for\ndinner,\nmy pet~"};
    return meal[b->turn / 4];
  }
  static const char *const lines[] = {"Ahuhuhu~", "Someone paid\nme well for\nyour SOUL~", "The spiders\ntold me all\nabout you~",
                                      "Too stingy\nto buy a\ndonut~?", "Pay up, and\nI might go\neasy~"};
  return lines[rnd_n(5)];
}
static const char *flavor_muffet(Battle *b, Monster *m) {
  if (!m->spare && b->turn >= 17) {
    m->spare = true;
    return "* A spider brings Muffet a note.\n* She reads it... and smiles.";
  }
  if (m->spare) return "* Muffet isn't going to fight anymore.";
  return NULL;
}
static bool item_muffet(Battle *b, Monster *m, int item, char *t, int cap) {
  (void)t, (void)cap;
  /* a treat from the spiders' bake sale: it's eaten as ever, and she lets Frisk go */
  if (item == ITEM_SPIDER_DONUT || item == ITEM_SPIDER_CIDER) m->spare = true, b->state[MF_TREAT] = 1;
  return false;
}
static int attack_muffet(Battle *b, Monster *m) {
  if (m->spare) return ATK_NONE;
  if (b->turn == 3 || b->turn == 7 || b->turn == 11) return ATK_MUFFET_PET;
  static const uint8_t seq[3] = {ATK_MUFFET_SPIDERS, ATK_MUFFET_CROISSANTS, ATK_MUFFET_DONUTS};
  return seq[(b->turn - 1) % 3];
}

/* ---------------------------------------------------------------- Mettaton, after the tile puzzle: no hurting him;
 * Alphys's yellow button (ACT) turns the SOUL yellow, and its shots wear him down */
enum { MT_YELLOW, MT_HITS };   /* (b->state[]) */
static void act_mtt_tiles(Battle *b, Monster *m, int act, char *t, int cap) {
  (void)m;
  if (act == 0) snprintf(t, (size_t)cap, "* You yell...\n* Nothing happens.");
  else {
    snprintf(t, (size_t)cap, "* You press the yellow button.\n* The phone hums near Mettaton...!");
    b->state[MT_YELLOW] = 1;
  }
}
static const char *talk_mtt_tiles(Battle *b, Monster *m) {
  (void)m;
  if (b->state[MT_HITS] >= 6) return "OH NO...\nI\'M OUT OF\nPOWER?!";
  if (b->state[MT_YELLOW] && b->state[MT_HITS]) return "THAT\nBUTTON IS\nCHEATING!";
  if (b->turn <= 1) return "NOW FOR\nTHE GRAND\nFINALE!";
  static const char *const lines[] = {"NO MORE\nPUZZLES,\nDARLING!", "STAY\nTUNED!", "THE\nRATINGS\nARE GREAT!"};
  return lines[rnd_n(3)];
}
static const char *flavor_mtt_tiles(Battle *b, Monster *m) {
  (void)m;
  if (b->turn == 2 && !b->state[MT_YELLOW]) return "* Alphys calls: \"The yellow button!\n  It's in your ACT menu!\"";
  if (b->turn >= 2 && !b->state[MT_YELLOW]) return "* Your phone's ACT menu is glowing.";
  return NULL;
}
static bool hit_mtt_tiles(Battle *b, Monster *m, int dmg) {
  (void)b, (void)dmg;
  m->dmg_shown = 0;   /* (MISS: metal) */
  return true;
}
static int attack_mtt_tiles(Battle *b, Monster *m) { (void)m; return b->state[MT_YELLOW] ? ATK_MTT_YELLOW : ATK_MTT_BOXES; }
static bool ends_mtt_tiles(Battle *b, Monster *m) { (void)m; return b->state[MT_HITS] >= 6; }

#define BOMB(nm, chk) {.name = nm, .hp = 1, .idle_speed = 8, .check = chk, .acts = {"Defuse"}, .nacts = 1, \
                       .flags = MF_BOSS | MF_NOT_KILLABLE, .act = act_bomb, .talk = talk_bomb, .attack = attack_bomb, \
                       .flavor = flavor_bomb, .on_hit = hit_bomb, .draw = draw_bomb}
static const MonDef bomb_defs[BOMB_COUNT] = {
  [BOMB_DOG] = BOMB("Dog", "It's fast asleep. It has no idea."),
  [BOMB_WATER] = BOMB("Glass of Water", "A glass of water. A very, very fast one."),
  [BOMB_SCRIPT] = BOMB("Script", "A movie script. Every page ends with a bang."),
  [BOMB_BALL] = BOMB("Basketball", "Painted to look like a basketball. Stylish, at least."),
  [BOMB_PRESENT] = BOMB("Present", "Wrapped up with a bow. It's ticking."),
  [BOMB_GAME] = BOMB("Game", "A video game. The box is oddly heavy."),
};
static const Form bomb_forms[BOMB_COUNT] = {
  [BOMB_DOG] = {{MON_BOMB}, 1, "* It's the dog!\n* Defuse it!"},
  [BOMB_WATER] = {{MON_BOMB}, 1, "* The glass of water dashes about!\n* Defuse it!"},
  [BOMB_SCRIPT] = {{MON_BOMB}, 1, "* Defuse the script!"},
  [BOMB_BALL] = {{MON_BOMB}, 1, "* Defuse the basketball!"},
  [BOMB_PRESENT] = {{MON_BOMB}, 1, "* Defuse the present!"},
  [BOMB_GAME] = {{MON_BOMB}, 1, "* Defuse the game!"},
};

static const MonDef ht_defs[MON_HT_COUNT - MON_WF_COUNT] = {
  [MON_VULKIN - MON_WF_COUNT] = {
    .name = "Vulkin", .spare_gold = 40, .hp = 20, .at = 8, .df = 0, .exp = 70, .gold = 40, .spr_idle = SPR_VULKIN_B,
    .spr_hurt = SPR_VULKIN_HURT, .idle_speed = 6, .check = "Thinks its lava makes people feel better.",
    .acts = {"Criticize", "Encourage", "Hug"}, .nacts = 3,
    .neutral = {"* Vulkin is making coffee in its crater.", "* Vulkin hums a warm little song.", "* Smells like a campfire."},
    .low_hp = "* Vulkin's lava is cooling.", .act = act_vulkin, .talk = talk_vulkin, .attack = attack_vulkin},
  [MON_TSUNDERPLANE - MON_WF_COUNT] = {
    .name = "Tsunderplane", .spare_gold = 60, .hp = 80, .at = 8, .df = 6, .exp = 95, .gold = 60, .spr_idle = SPR_TSUNDER_B,
    .spr_hurt = SPR_TSUNDER_B, .idle_speed = 6, .check = "Acts mean, but maybe it secretly likes you?",
    .acts = {"Flirt", "Approach"}, .nacts = 2,
    .neutral = {"* Tsunderplane shakes its nose at you.", "* Tsunderplane is looking the other way.",
                "* Smells like jet fuel and perfume."},
    .low_hp = "* Tsunderplane's engine is smoking.", .act = act_tsunder, .talk = talk_tsunder, .attack = attack_tsunder,
    .flavor = flavor_tsunder},
  [MON_PYROPE - MON_WF_COUNT] = {
    .name = "Pyrope", .spare_gold = 45, .hp = 110, .at = 8, .df = 1, .exp = 80, .gold = 30, .spr_idle = SPR_PYROPE_B,
    .spr_hurt = SPR_PYROPE_HURT, .idle_speed = 5, .dy = 18, .check = "A troublemaker who's never warm enough.",
    .acts = {"Heat Up", "Cool Down", "Invite"}, .nacts = 3,
    .neutral = {"* Pyrope is protected by its winning smile.", "* Pyrope crackles.", "* Smells like a burnt rope."},
    .act = act_pyrope, .talk = talk_pyrope, .attack = attack_pyrope},
  [MON_METTATON - MON_WF_COUNT] = {
    .name = "Mettaton", .hp = 9999, .at = 0, .df = 0, .spr_idle = SPR_MTT_B, .spr_hurt = SPR_MTT_B, .idle_speed = 6,
    .check = "A robot with a TV show.", .flags = MF_BOSS | MF_NOT_KILLABLE},
  [MON_RG01 - MON_WF_COUNT] = {
    .name = "RG 01", .spare_gold = 50, .hp = 150, .at = 8, .df = 4, .exp = 110, .gold = 60, .spr_idle = SPR_RG01_B,
    .spr_hurt = SPR_RG01_B, .idle_speed = 6, .check = "A Royal Guard. Armor polished to a shine.",
    .acts = {"Clean Armor", "Whisper"}, .nacts = 2,
    .neutral = {"* 01 polishes his helmet.", "* 02 watches your every step.", "* Smells like a locker room.",
                "* 01 stands guard."},
    .flags = MF_BOSS, .act = act_rg, .talk = talk_rg, .attack = attack_rg, .flavor = flavor_rg},
  [MON_RG02 - MON_WF_COUNT] = {
    .name = "RG 02", .spare_gold = 50, .hp = 150, .at = 8, .df = 4, .exp = 110, .gold = 60, .spr_idle = SPR_RG02_B,
    .spr_hurt = SPR_RG02_B, .idle_speed = 6, .check = "A Royal Guard. Armor looks stuffy and hot.",
    .acts = {"Clean Armor", "Whisper"}, .nacts = 2,
    .neutral = {"* Sweat drips from 02's armor.", "* 02 is clenching his fists.", "* Smells like a locker room."},
    .flags = MF_BOSS, .act = act_rg, .talk = talk_rg, .attack = attack_rg, .flavor = flavor_rg},
  [MON_BOMB - MON_WF_COUNT] = BOMB("Bomb", ""),
  [MON_MUFFET - MON_WF_COUNT] = {
    .name = "Muffet", .spare_gold = 100, .hp = 1250, .at = 8, .df = 0, .exp = 300, .gold = 100, .spr_idle = SPR_MUFFET_B,
    .spr_hurt = SPR_MUFFET_HURT, .idle_speed = 5, .check = "If she asks you up to her parlor, say no thanks.",
    .acts = {"Pay", "Struggle"}, .nacts = 2,
    .neutral = {"* Muffet pours herself a cup of tea.", "* Muffet counts her money, twice.",
                "* Smells like cobwebs and fresh pastry."},
    .flags = MF_BOSS, .act = act_muffet, .talk = talk_muffet, .attack = attack_muffet, .flavor = flavor_muffet,
    .on_item = item_muffet},
  [MON_MTT_TILES - MON_WF_COUNT] = {
    .name = "Mettaton", .hp = 9999, .at = 8, .df = 0, .spr_idle = SPR_MTT_B, .spr_hurt = SPR_MTT_B, .idle_speed = 6,
    .check = "Still made of metal. Still can't be hurt.", .acts = {"Yell", "Yellow"}, .nacts = 2,
    .neutral = {"* Mettaton.", "* Mettaton strikes a pose."},
    .flags = MF_BOSS | MF_NOT_KILLABLE, .act = act_mtt_tiles, .talk = talk_mtt_tiles, .attack = attack_mtt_tiles,
    .flavor = flavor_mtt_tiles, .on_hit = hit_mtt_tiles, .ends = ends_mtt_tiles},
};
const MonDef *ht_mon_def(int id) {
  if (id >= MON_HT_COUNT) return core_mon_def(id);
  return id == MON_BOMB ? &bomb_defs[g_bomb_kind] : &ht_defs[id - MON_WF_COUNT];
}

static const Form ht_forms[FORM_HT_COUNT - FORM_WF_COUNT] = {
  [FORM_VULKIN - FORM_WF_COUNT] = {{MON_VULKIN}, 1, "* Vulkin strolls in, happy as can be."},
  [FORM_TSUNDERPLANE - FORM_WF_COUNT] = {{MON_TSUNDERPLANE}, 1, "* Tsunderplane gets in the way!\n* Not on purpose or anything."},
  [FORM_TSUNDER_VULKIN - FORM_WF_COUNT] = {{MON_VULKIN, MON_TSUNDERPLANE}, 2,
                                           "* Tsunderplane and Vulkin show up.\n* (Not that Tsunderplane cares.)"},
  [FORM_PYROPE - FORM_WF_COUNT] = {{MON_PYROPE}, 1, "* Pyrope bounces in, burning bright!"},
  [FORM_PYROPE2 - FORM_WF_COUNT] = {{MON_PYROPE, MON_PYROPE}, 2, "* Two Pyropes hop in.\n* They're up to something."},
  [FORM_MTT_QUIZ - FORM_WF_COUNT] = {{MON_METTATON}, 1, ""},
  [FORM_MTT_COOK - FORM_WF_COUNT] = {{MON_METTATON}, 1, ""},
  [FORM_LASERS - FORM_WF_COUNT] = {{MON_METTATON}, 1, ""},
  [FORM_RGUARDS - FORM_WF_COUNT] = {{MON_RG01, MON_RG02}, 2, "* The Royal Guards attack!"},
  [FORM_BOMB - FORM_WF_COUNT] = {{MON_BOMB}, 1, ""},
  [FORM_MUFFET - FORM_WF_COUNT] = {{MON_MUFFET}, 1, "* Muffet blocks the way!"},
  [FORM_MTT_TILES - FORM_WF_COUNT] = {{MON_MTT_TILES}, 1, "* Mettaton attacks!"},
};
const Form *ht_form(int form) {
  if (form >= FORM_HT_COUNT) return core_form(form);
  return form == FORM_BOMB ? &bomb_forms[g_bomb_kind] : &ht_forms[form - FORM_WF_COUNT];
}

/* random encounters: past the Lab (the belts and the Magma Chamber); on the second floor, the crossroads and the
 * Bad Opinion Zone */
int ht_encounter(int room) {
  switch (room) {
    case ROOM_HT_BELT1: case ROOM_HT_BELT2: case ROOM_HT_MAGMA: {
      static const uint8_t pool[] = {FORM_VULKIN, FORM_TSUNDERPLANE, FORM_TSUNDER_VULKIN, FORM_PYROPE, FORM_PYROPE2};
      return pool[rnd_n(5)];
    }
    case ROOM_HT_CROSS2: case ROOM_HT_BOZ: {
      static const uint8_t pool[] = {FORM_TSUNDERPLANE, FORM_TSUNDER_VULKIN, FORM_PYROPE, FORM_PYROPE2};
      return pool[rnd_n(4)];
    }
  }
  return core_encounter(room);
}

/* ---------------------------------------------------------------- the attacks */
static int bx0(void) { return bt.x0 + 3; }
static int bx1(void) { return bt.x1 - 3; }
static int by0(void) { return bt.y0 + 3; }
static int by1(void) { return bt.y1 - 3; }
#define P(v) ((v) * FX)
static Bullet *shot(uint16_t spr, int x, int y, int vx, int vy) { return bullet_new(spr, P(x), P(y), vx, vy); }

/* Vulkin: a thundercloud over the board drops bolts that spread as they fall (it drifts, sometimes) */
static void atk_vulkin_thunder(int t) {
  static Bullet *cloud;
  if (t == 0) {
    cloud = shot(SPR_CLOUD, board_cx(), by0() + 8, rnd_n(2) ? P(1) / 2 : 0, 0);
    if (cloud) cloud->flags |= BF_HARMLESS | BF_NOCLIP, cloud->life = 150;
  }
  if (cloud && cloud->kind != BK_NONE && (cloud->x < P(bx0() + 14) || cloud->x > P(bx1() - 14))) cloud->vx = -cloud->vx;
  if (cloud && cloud->kind != BK_NONE && t < 130 && t % 20 == 10) {
    for (int k = -1; k <= 1; k++) {
      Bullet *b = bullet_new(SPR_BOLT, cloud->x, cloud->y + P(6), k * P(1) / 2, P(2));
      if (b) b->r = 2, b->flags |= BF_NOCLIP;
    }
  }
}
/* and fire: two columns at the sides throw fireballs at the middle, and they fall back */
static void atk_vulkin_fire(int t) {
  if (t < 130 && t % 16 == 0) {
    bool left = (t / 16) % 2;
    int y = by0() + 8 + rnd_n(by1() - by0() - 16);
    Bullet *b = shot(SPR_FIREBULLET, left ? bx0() + 2 : bx1() - 2, y, (left ? 1 : -1) * P(3), 0);
    if (b) b->r = 4, b->ax = (left ? -1 : 1) * 3, b->anim = 3, b->flags |= BF_NOCLIP, b->life = 120;
  }
}

/* Tsunderplane: little planes cross the board dropping bombs (a bomb at the bottom sends smoke all the way up);
 * approached, its planes come wrapped in green: touch it */
static int tsunder_approached(void) {
  for (int i = 0; i < bt.nmon; i++)
    if (bt.mon[i].def == mon_def(MON_TSUNDERPLANE) && bt.mon[i].state && !bt.mon[i].gone) return 1;
  return 0;
}
static void green_wrap(Bullet *p) {
  Bullet *g = bullet_new(SPR_ORB, p->x, p->y - P(9), p->vx, p->vy);
  if (g) g->color = BC_GREEN, g->dmg = 0, g->r = 4, g->flags |= BF_NOCLIP, g->life = p->life;
}
static void atk_tsunder_bombs(int t) {
  if (t < 120 && t % 30 == 0) {
    bool left = (t / 30) % 2;
    int y = by0() + 6;
    Bullet *p = shot(SPR_PLANE, left ? bx0() - 10 : bx1() + 10, y, (left ? 1 : -1) * P(2), 0);
    if (p) p->r = 4, p->flags |= BF_NOCLIP, p->life = 110, p->angle = left ? 0 : 128;
    if (p && tsunder_approached()) green_wrap(p);
  }
  for (int i = 0; i < MAX_BULLETS; i++) {
    Bullet *b = &bullets[i];
    if (b->kind == BK_NONE) continue;
    if (b->spr == SPR_PLANE && b->t % 24 == 12 && b->x > P(bx0()) && b->x < P(bx1())) {
      Bullet *d = bullet_new(SPR_BOMB, b->x, b->y + P(6), 0, P(1));
      if (d) d->kind = BK_GRAVITY, d->ay = 2, d->r = 3, d->a = 1;
    }
    if (b->spr == SPR_BOMB && b->a == 1 && b->y > P(by1() - 4)) {
      /* the smoke: a column up through the whole board, a moment */
      int x = b->x / FX;
      b->kind = BK_NONE;
      for (int y = by1() - 4; y > by0(); y -= 9) {
        Bullet *s = shot(SPR_ORB, x, y, 0, 0);
        if (s) s->r = 3, s->life = 18, s->alpha = 170;
      }
    }
  }
}
/* or big planes dive from the top, a trail of smoke behind (it moves across, unless approached) */
static void atk_tsunder_dive(int t) {
  if (t < 120 && t % 40 == 0) {
    int x = bx0() + 10 + rnd_n(bx1() - bx0() - 20);
    Bullet *p = shot(SPR_PLANE, x, by0() - 10, 0, P(2));
    if (p) p->r = 5, p->flags |= BF_NOCLIP, p->life = 80, p->angle = 64;
    if (p && tsunder_approached()) green_wrap(p);
  }
  bool still = tsunder_approached();
  for (int i = 0; i < MAX_BULLETS; i++) {
    Bullet *b = &bullets[i];
    if (b->kind == BK_NONE || b->spr != SPR_PLANE || b->angle != 64 || b->t % 8) continue;
    Bullet *s = bullet_new(SPR_ORB, b->x, b->y - P(8), still ? 0 : (b->t % 16 ? P(1) : -P(1)), 0);
    if (s) s->r = 2, s->life = 50, s->alpha = 150;
  }
}

/* Pyrope: bombs come from every side, bounce about and burst at the board's edges; or its rope of fire runs across
 * (orange: run through it) */
static void atk_pyrope_bombs(int t) {
  if (t < 110 && t % 22 == 0) {
    int side = rnd_n(4);
    int x = side < 2 ? bx0() + 8 + rnd_n(bx1() - bx0() - 16) : side == 2 ? bx0() : bx1();
    int y = side >= 2 ? by0() + 8 + rnd_n(by1() - by0() - 16) : side == 0 ? by0() : by1();
    int vx = side == 2 ? P(2) : side == 3 ? -P(2) : (rnd_n(2) ? P(1) : -P(1));
    int vy = side == 0 ? P(2) : side == 1 ? -P(2) : (rnd_n(2) ? P(1) : -P(1));
    Bullet *b = shot(SPR_BOMB, x, y, vx, vy);
    if (b) b->kind = BK_SCRIPT, b->r = 3, b->a = 3, b->flags |= BF_NOCLIP;
  }
  for (int i = 0; i < MAX_BULLETS; i++) {
    Bullet *b = &bullets[i];
    if (b->kind != BK_SCRIPT || b->spr != SPR_BOMB) continue;
    b->x += b->vx, b->y += b->vy, b->t++;
    bool hit_x = b->x < P(bx0() + 2) || b->x > P(bx1() - 2), hit_y = b->y < P(by0() + 2) || b->y > P(by1() - 2);
    if (b->t > 8 && (hit_x || hit_y)) {
      if (--b->a > 0) {
        if (hit_x) b->vx = -b->vx;
        if (hit_y) b->vy = -b->vy;
        continue;
      }
      /* it bursts: sparks */
      b->kind = BK_NONE;
      for (int k = 0; k < 4; k++) {
        static const int8_t sx[4] = {1, -1, 1, -1}, sy[4] = {1, 1, -1, -1};
        Bullet *s = bullet_new(SPR_ORB, b->x, b->y, sx[k] * P(1), sy[k] * P(1));
        if (s) s->r = 2, s->life = 30;
      }
    }
  }
}
static void atk_pyrope_rope(int t) {
  if (t < 120 && t % 40 == 0) {
    Bullet *f = shot(SPR_FIREBULLET, bx1() + 6, board_cy(), -P(2) - P(t / 40) / 2, 0);
    if (f) f->color = BC_ORANGE, f->flags |= BF_BOX | BF_NOCLIP, f->w = 8, f->h = (int16_t)(by1() - by0()), f->life = 120, f->anim = 3;
  }
}

/* the Royal Guards' team attack: stars falling from the top, and others rising from the bottom. Once 02's armor's
 * been cleaned, some of the falling ones are green (dropped right over the SOUL); once it's off, 01's aim goes shaky and they fly out at the
 * sides. One guard fallen, the other attacks twice as hard */
static void atk_rg_team(int t) {
  bool angry = false, off = bt.state[RG_ARMOR_OFF], green = bt.state[RG_CLEANED] && !off;
  for (int i = 0; i < bt.nmon; i++) angry |= bt.mon[i].killed;
  int every = angry ? 10 : 20;
  if (t < 130 && t % every == 0) {
    /* (a green one falls right over the SOUL) */
    bool g = green && t % 40 == 20;
    int x = g ? bt.sx / FX : bx0() + 6 + rnd_n(bx1() - bx0() - 12);
    Bullet *s = shot(SPR_STAR, x, by0() - 4, off ? (rnd_n(2) ? P(3) / 2 : -P(3) / 2) : 0, P(1) / 2);
    if (s) {
      s->kind = BK_GRAVITY, s->ay = 2, s->r = 3, s->flags |= BF_NOCLIP, s->life = 120;
      if (g) s->color = BC_GREEN, s->dmg = 0;
    }
  }
  if (t < 130 && t % (every * 3 / 2) == 15) {
    int x = bx0() + 6 + rnd_n(bx1() - bx0() - 12);
    Bullet *s = shot(SPR_STAR, x, by1() + 4, 0, -P(2));
    if (s) s->r = 3, s->flags |= BF_NOCLIP, s->life = 90;
  }
}

/* Muffet's: spiders crawl along the strings (riding them when they're pulled); croissants thrown along one come
 * back; donuts bounce about the board; and at mealtimes her pet waits at the bottom while the strings are pulled down
 * to it */
static void spider_on(int string, bool left, int speed) {
  Bullet *b = shot(SPR_SPIDER_B, left ? bx0() - 6 : bx1() + 6, soul_string_y(string), (left ? 1 : -1) * speed, 0);
  if (b) b->kind = BK_SCRIPT, b->a = (int16_t)string, b->flags |= BF_NOCLIP, b->r = 3, b->life = 260;
}
static void spiders_ride(void) {
  for (int i = 0; i < MAX_BULLETS; i++) {
    Bullet *b = &bullets[i];
    if (b->kind != BK_SCRIPT || b->spr != SPR_SPIDER_B) continue;
    b->x += b->vx, b->y = soul_string_y(b->a) * FX;
    if (++b->t >= b->life) b->kind = BK_NONE;
  }
}
static void atk_muffet_spiders(int t) {
  if (t < 130 && t % 14 == 0) spider_on(rnd_n(NSTRINGS), rnd_n(2), P(2));
  spiders_ride();
}
static void atk_muffet_croissants(int t) {
  if (t < 110 && t % 26 == 0) {
    bool left = rnd_n(2);
    Bullet *b = shot(SPR_CROISSANT, left ? bx0() - 6 : bx1() + 6, soul_string_y(rnd_n(NSTRINGS)), (left ? 1 : -1) * P(4), 0);
    if (b) b->ax = left ? -1 : 1, b->r = 4, b->flags |= BF_NOCLIP, b->life = 140, b->angle = left ? 128 : 0;
  }
}
static void atk_muffet_donuts(int t) {
  if (t < 110 && t % 20 == 0) {
    bool left = (t / 20) % 2;
    Bullet *b = shot(SPR_DONUT, left ? bx0() + 4 : bx1() - 4, by0() + 4, (left ? 1 : -1) * P(2), P(2));
    if (b) b->kind = BK_SCRIPT, b->a = 3, b->r = 4, b->flags |= BF_NOCLIP;
  }
  for (int i = 0; i < MAX_BULLETS; i++) {
    Bullet *b = &bullets[i];
    if (b->kind != BK_SCRIPT || b->spr != SPR_DONUT) continue;
    b->x += b->vx, b->y += b->vy, b->t++;
    bool hx = b->x < P(bx0() + 2) || b->x > P(bx1() - 2), hy = b->y < P(by0() + 2) || b->y > P(by1() - 2);
    if (b->t > 6 && (hx || hy)) {
      if (--b->a <= 0) b->kind = BK_NONE;
      if (hx) b->vx = -b->vx;
      if (hy) b->vy = -b->vy;
    }
  }
}
static void atk_muffet_pet(int t) {
  if (t == 0) {
    Bullet *p = shot(SPR_MUFFET_PET, board_cx(), by1() + 8, 0, 0);
    if (p) p->flags |= BF_BOX | BF_NOCLIP, p->w = 90, p->h = 26, p->anim = 6, p->life = 150, p->dmg = 4;
  }
  /* the strings, down towards it (round again from the top) */
  soul_strings_shift += P(1) / 2;
  if (t < 130 && t % 30 == 0) spider_on(rnd_n(NSTRINGS), rnd_n(2), P(1));
  spiders_ride();
}

/* Mettaton's: rows of boxes falling, a gap in each; turned yellow, the SOUL shoots at him as he glides about over
 * the board (six hits, and he's out of power), a few boxes still falling */
static void atk_mtt_boxes(int t) {
  if (t < 130 && t % 26 == 0) {
    static const uint8_t gaps[6] = {1, 4, 2, 5, 0, 3};
    int n = (bx1() - bx0()) / 16, gap = gaps[(t / 26) % 6] % n;
    for (int k = 0; k < n; k++) {
      if (k == gap || k == gap + 1) continue;
      Bullet *b = shot(SPR_PRESENT, bx0() + 8 + k * 16, by0() - 8, 0, P(2));
      if (b) b->r = 5, b->flags |= BF_NOCLIP, b->life = 90;
    }
  }
}
static Bullet *mtt_target;
static void atk_mtt_yellow(int t) {
  if (t == 0) {
    mtt_target = shot(SPR_MTTMINI, board_cx(), by0() + 12, P(3) / 2, 0);
    if (mtt_target) mtt_target->kind = BK_SCRIPT, mtt_target->flags |= BF_HARMLESS;
  }
  Bullet *m = mtt_target;
  if (!m || m->kind != BK_SCRIPT || m->spr != SPR_MTTMINI) return;
  m->x += m->vx;
  if (m->x < P(bx0() + 10) || m->x > P(bx1() - 10)) m->vx = -m->vx;
  if (m->c) m->c--, m->alpha = m->c % 2 ? 90 : 255;
  for (int i = 0; i < MAX_BULLETS; i++) {
    Bullet *s = &bullets[i];
    if (s->kind == BK_NONE || !(s->flags & BF_PLAYER)) continue;
    if (abs_i(s->x - m->x) < P(9) && abs_i(s->y - m->y) < P(11)) {
      s->kind = BK_NONE;
      bt.state[MT_HITS]++, m->c = 10;
    }
  }
  if (t < 130 && t % 40 == 20) {
    Bullet *b = shot(SPR_PRESENT, m->x / FX, m->y / FX + 10, 0, P(2));
    if (b) b->r = 5, b->flags |= BF_NOCLIP, b->life = 80;
  }
}

int ht_attack_length(int atk) { return atk >= ATK_HT_COUNT ? core_attack_length(atk) : atk == ATK_MUFFET_PET ? 150 : 160; }

void ht_attack_run(Battle *b, int atk, int t) {
  if (atk >= ATK_HT_COUNT) {
    core_attack_run(b, atk, t);
    return;
  }
  if (t < 0) {
    bool muffet = atk >= ATK_MUFFET_SPIDERS && atk <= ATK_MUFFET_PET;
    board_set(muffet ? 190 : atk == ATK_TSUNDER_BOMBS || atk == ATK_PYROPE_ROPE ? 150 : atk == ATK_MTT_BOXES ? 140 : 120,
              muffet ? 90 : 100);
    soul_set_mode(muffet ? SOUL_PURPLE : atk == ATK_MTT_YELLOW ? SOUL_YELLOW : SOUL_RED);
    return;
  }
  switch (atk) {
    case ATK_VULKIN_THUNDER: atk_vulkin_thunder(t); break;
    case ATK_VULKIN_FIRE: atk_vulkin_fire(t); break;
    case ATK_TSUNDER_BOMBS: atk_tsunder_bombs(t); break;
    case ATK_TSUNDER_DIVE: atk_tsunder_dive(t); break;
    case ATK_PYROPE_BOMBS: atk_pyrope_bombs(t); break;
    case ATK_PYROPE_ROPE: atk_pyrope_rope(t); break;
    case ATK_RG_TEAM: atk_rg_team(t); break;
    case ATK_MUFFET_SPIDERS: atk_muffet_spiders(t); break;
    case ATK_MUFFET_CROISSANTS: atk_muffet_croissants(t); break;
    case ATK_MUFFET_DONUTS: atk_muffet_donuts(t); break;
    case ATK_MUFFET_PET: atk_muffet_pet(t); break;
    case ATK_MTT_BOXES: atk_mtt_boxes(t); break;
    case ATK_MTT_YELLOW: atk_mtt_yellow(t); break;
  }
}
