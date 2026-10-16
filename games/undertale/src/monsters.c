/* The Ruins' monsters: stats, texts and ACTs (from the Undertale Wiki's records of the game), their formations,
 * and their attacks, as the wiki describes them. */
#include <stdio.h>
#include <string.h>
#include "battle.h"
#include "game.h"
#include "gfx.h"
#include "monsters.h"
#include "res.h"

/* ---------------------------------------------------------------- the ACTs */
static void act_dummy(Battle *b, Monster *m, int act, char *t, int cap) {
  (void)b;
  snprintf(t, (size_t)cap, "* You chat with the DUMMY.\n* It doesn't answer.|* TORIEL looks pleased.");
  m->gone = true, m->fade = 1;
  set_flag(F_DUMMY, 1);
}
static const char *flavor_dummy(Battle *b, Monster *m) {
  if (b->turn >= 4) return "* Dummy tires of your aimless\n  shenanigans.";
  return NULL;
}

static void act_froggit(Battle *b, Monster *m, int act, char *t, int cap) {
  (void)b;
  if (act == 0) {
    snprintf(t, (size_t)cap, "* Froggit didn't understand\n  what you said, but was\n  flattered anyway.");
    m->state = 1;
  } else {
    snprintf(t, (size_t)cap, "* Froggit didn't understand\n  what you said, but was scared\n  anyway.");
    m->state = 2;
  }
  m->spare = true;
}
static const char *talk_froggit(Battle *b, Monster *m) {
  if (m->state == 1) { m->state = 3; return "(Blushes\ndeeply.)\nRibbit.."; }
  if (m->state == 2) { m->state = 3; return "Shiver,\nshiver."; }
  static const char *const q[] = {"Ribbit,\nribbit.", "Croak,\ncroak.", "Hop,\nhop.", "Meow."};
  return q[rnd_n(4)];
}
static int attack_froggit(Battle *b, Monster *m) { return rnd_n(10) == 0 ? ATK_FROG_RAIN : rnd_n(2) ? ATK_FROG_FLIES : ATK_FROG_JUMP; }

static void act_whimsun(Battle *b, Monster *m, int act, char *t, int cap) {
  if (act == 0) {
    snprintf(t, (size_t)cap, "* Halfway through your first\n  word, Whimsun bursts into\n  tears and runs away.");
    m->gone = true, m->fade = 1;
    m->spare_gold = m->state ? 2 : 0;
  } else {
    snprintf(t, (size_t)cap, "* You raise your arms and\n  wiggle your fingers.\n* Whimsun freaks out!");
    m->state = 1;
  }
  (void)b;
}
static const char *talk_whimsun(Battle *b, Monster *m) {
  if (m->state == 1) { m->state = 2; return "I can't\nhandle\nthis..."; }
  static const char *const q[] = {"I'm sorry...", "*sniff sniff*", "I have no\nchoice..", "Forgive me..."};
  return q[rnd_n(4)];
}
static const char *flavor_whimsun(Battle *b, Monster *m) { return m->state ? "* Whimsun is hyperventilating." : NULL; }
static int attack_whimsun(Battle *b, Monster *m) { return rnd_n(2) ? ATK_WHIM_RING : ATK_WHIM_COLS; }

static void act_moldsmal(Battle *b, Monster *m, int act, char *t, int cap) {
  if (act == 0) snprintf(t, (size_t)cap, "* You lie still beside\n  Moldsmal.\n* It feels peaceful.");
  else snprintf(t, (size_t)cap, "* You wiggle your hips.\n* Moldsmal wiggles back.\n* What a meaningful conversation!");
  m->spare_gold = 1;
  (void)b;
}
static int attack_moldsmal(Battle *b, Monster *m) { return rnd_n(2) ? ATK_MOLD_ZIG : ATK_MOLD_BURST; }

static void act_loox(Battle *b, Monster *m, int act, char *t, int cap) {
  if (act == 0) {
    snprintf(t, (size_t)cap, "* You don't pick on Loox.");
    if (m->exp > 4) m->exp = (int16_t)(m->exp - 2);
    m->spare = true, m->state = 2;
  } else {
    snprintf(t, (size_t)cap, "* You do something mean...\n* You pick on Loox. Loox is\n  furious!");
    if (m->exp < 20 && !m->spare) m->exp = (int16_t)(m->exp + 5);
    m->state = 1;
  }
  (void)b;
}
static const char *talk_loox(Battle *b, Monster *m) {
  if (m->state == 1) { m->state = 0; m->state2 = 1; return "You rude\nlittle\nsnipe!"; }
  if (m->state == 2) { m->state = 0; m->state2 = 0; return "Finally\nsomeone\ngets it."; }
  if (b->turn <= 1) return "Please don't\npick on me.";
  static const char *const q[] = {"Quit staring\nat me.", "I've got my\neye on you.", "How about a\nstaring\ncontest?", "Don't point\nthat at me.", "What an\neyesore."};
  return q[rnd_n(5)];
}
static int attack_loox(Battle *b, Monster *m) { return rnd_n(2) ? ATK_LOOX_BOUNCE : ATK_LOOX_WORM; }

static void act_vegetoid(Battle *b, Monster *m, int act, char *t, int cap) {
  if (act == 0) {
    snprintf(t, (size_t)cap, "* You try to talk with\n  Vegetoid, but it just smiles.");
    m->state = 3;
  } else if (act == 1) {
    if (m->spare || m->hp < m->maxhp / 2) {
      snprintf(t, (size_t)cap, "* You took a bite out of\n  Vegetoid.\n* You recovered 5 HP!");
      sv.hp = (int16_t)min_i(sv.maxhp, sv.hp + 5);
      m->gone = true, m->fade = 1;
    } else
      snprintf(t, (size_t)cap, "* You tried to eat Vegetoid,\n  but it wasn't weakened\n  enough.");
  } else {
    snprintf(t, (size_t)cap, "* You pat your stomach.\n* Vegetoid offers a healthy\n  meal.");
    m->state = 1;
  }
  (void)b;
}
static const char *talk_vegetoid(Battle *b, Monster *m) {
  if (m->state == 3) { m->state = 0; return "Plants Can't\nTalk Dummy"; }
  if (m->state == 1) return "Eat Your\nGreens";
  if (m->state == 2) return "Ate Your\nGreens";
  static const char *const q[] = {"Farmed\nLocally,\nVery Locally", "Part Of A\nComplete\nBreakfast", "Fresh\nMorning\nTaste", "Contains\nVitamin A"};
  return q[rnd_n(4)];
}
static int attack_vegetoid(Battle *b, Monster *m) { return rnd_n(2) ? ATK_VEG_FALL : ATK_VEG_CARROT; }

static void act_migosp(Battle *b, Monster *m, int act, char *t, int cap) {
  if (count_alive() == 1) {
    snprintf(t, (size_t)cap, "* You talk to Migosp.\n* It doesn't seem to care about\n  anything.");
    m->state = 1;
  } else
    snprintf(t, (size_t)cap, "* You talk to Migosp.\n* It doesn't seem to want to\n  listen.");
  (void)b;
}
static const char *talk_migosp(Battle *b, Monster *m) {
  if (count_alive() == 1) {
    m->spare = true;
    if (m->state == 1) { m->state = 0; return "Hiya~"; }
    static const char *const q[] = {"Bein' me is\nthe best!", "La la~ Just\nbe yourself~", "Nothin' like\nalone time!", "Mmm, cha\ncha cha!", "Swing your\narms, baby"};
    return q[rnd_n(5)];
  }
  if (m->state == 1) { m->state = 0; return "I DON'T\nCARE."; }
  static const char *const q[] = {"FILTHY\nSINGLE\nMINDER..", "OBEY THE\nOVERMIND ..", "LEGION! WE\nARE LEGION", "HEED THE\nSWARM", "IN UNISON,\nNOW"};
  return q[rnd_n(5)];
}
static const char *flavor_migosp(Battle *b, Monster *m) {
  if (count_alive() == 1) return "* Migosp doesn't have a care\n  in the world.";
  return NULL;
}
static int attack_migosp(Battle *b, Monster *m) {
  if (count_alive() == 1) return ATK_MIGOSP_DANCE;
  return rnd_n(8) == 0 ? ATK_FROG_FLIES : ATK_MIGOSP_ROACH;
}

/* ---------------------------------------------------------------- Napstablook */
static void act_napsta(Battle *b, Monster *m, int act, char *t, int cap) {
  /* state: cheers; state2: 1 after the hat, 2 morose (turns of it in b->state[1]) */
  if (act == 2) {   /* Cheer */
    if (m->state2 == 2) {
      snprintf(t, (size_t)cap, "* You try to console\n  Napstablook...");
      b->state[1]++;
      if (b->state[1] >= 8) m->state2 = 0, m->state = 0;
      return;
    }
    m->state++;
    if (m->state == 1) snprintf(t, (size_t)cap, "* You gave Napstablook a\n  patient smile.");
    else if (m->state == 2) snprintf(t, (size_t)cap, "* You told Napstablook a\n  little joke.");
    else if (m->state == 3) snprintf(t, (size_t)cap, "* Napstablook wants to show\n  you something.");
    else {
      snprintf(t, (size_t)cap, "* ...");
      b->state[2] = 1;   /* the end: Napstablook goes */
    }
  } else if (act == 0) {   /* Flirt */
    if (m->state >= 3) {
      snprintf(t, (size_t)cap, "* You flirt with Napstablook.");
      b->state[2] = 2;
    } else {
      snprintf(t, (size_t)cap, "* You try your best pickup\n  line.");
      m->state2 = 2, b->state[1] = 0;
    }
  } else {   /* Threat */
    snprintf(t, (size_t)cap, "* You give Napstablook a\n  cruel look.");
    m->state2 = 2, b->state[1] = 0, b->state[3] = m->state >= 3;
    m->state = 0;
  }
}
static const char *talk_napsta(Battle *b, Monster *m) {
  if (b->state[2] == 3)
    return "oh... i'm a\nghost, you\nknow|i just\nlowered my hp\nto be polite|sorry... that\nwas awkward|let's say you\nwon...";
  if (b->state[2] == 1) return "oh gee...";
  if (b->state[2] == 2) return "oh no...";
  if (b->state[3]) { b->state[3] = 0; return "i knew it..."; }
  if (m->acts[0] && m->state2 == 2 && b->state[1] == 0 && b->state[4] != 1) { b->state[4] = 1; return "i'd just\nweigh you\ndown."; }
  if (m->acts[1] && m->state2 == 2 && b->state[1] == 0 && b->state[4] != 2) { b->state[4] = 2; return "go ahead,\ndo it."; }
  if (m->state == 1 && b->state[5] != 1) { b->state[5] = 1; return "heh..."; }
  if (m->state == 2 && b->state[5] != 2) { b->state[5] = 2; return "heh heh..."; }
  if (m->state == 3 && b->state[5] != 3) { b->state[5] = 3; return "let me\ntry...|i call it\n\"dapper\nblook\"|do you\nlike it..."; }
  static const char *const q[] = {"nnnnnnggghhh.", "just pluggin\nalong...", "i'm fine,\nthanks."};
  return q[rnd_n(3)];
}
static const char *flavor_napsta(Battle *b, Monster *m) {
  if (m->state2 == 2) return NULL;
  if (m->state == 1) return "* Napstablook looks just a\n  little bit better.";
  if (m->state == 2) return "* Cheering seems to have\n  improved Napstablook's mood\n  again.";
  if (m->state >= 3) return "* Napstablook eagerly awaits\n  your response.";
  return NULL;
}
static int attack_napsta(Battle *b, Monster *m) {
  if (b->state[2]) return -1;
  if (b->turn == 2) return ATK_NAPSTA_TEXT;
  if (m->state == 3 && b->state[5] == 3 && !b->state[6]) { b->state[6] = 1; return ATK_NAPSTA_HAT; }
  if (m->state >= 3) return ATK_IDLE;
  if (m->state2 == 2) return b->state[1] >= 6 ? ATK_NAPSTA_TEARS : ATK_NAPSTA_FLOOD;
  return rnd_n(2) ? ATK_NAPSTA_TEARS : ATK_NAPSTA_WIGGLE;
}
static bool ends_napsta(Battle *b, Monster *m) {
  if (b->state[2] == 3) m->killed = false, m->exp = 0;
  if (b->state[2]) set_flag(F_NAPSTA, b->state[2] == 3 ? 2 : 1);
  return b->state[2] != 0;
}
static bool hit_napsta(Battle *b, Monster *m, int dmg) {
  m->hp = (int16_t)(m->hp - dmg);
  m->hurt = 30;
  if (m->hp <= 0) {
    m->hp = 0;
    b->state[2] = 3;   /* "you cant kill ghosts": then gone, no EXP */
  }
  return true;
}

/* ---------------------------------------------------------------- the monsters */
static const MonDef defs[MON_RUINS_COUNT] = {
  [MON_DUMMY] = {
    .name = "Dummy", .hp = 15, .at = 0, .df = -5, .exp = 0, .gold = 0, .spr_idle = SPR_DUMMY_B, .spr_hurt = SPR_DUMMY_B,
    .check = "A cotton heart and a button\n  eye\n* You are the apple of my eye",
    .acts = {"Talk"}, .nacts = 1, .neutral = {"* Dummy looks like it's going\n  to fall over.", "* Dummy stands around\n  absentmindedly."},
    .quotes = {"....."}, .act = act_dummy, .flavor = flavor_dummy, .flags = MF_SPARE_START},
  [MON_FROGGIT_FIRST] = {
    .name = "Froggit", .hp = 20, .at = 4, .df = 1, .exp = 10, .gold = 20, .spr_idle = SPR_FROGGIT_B, .spr_hurt = SPR_FROGGIT_HURT,
    .idle_speed = 8, .check = "Life is difficult for this\n  enemy.", .acts = {"Compliment", "Threaten"}, .nacts = 2,
    .neutral = {"* Froggit doesn't seem to know\n  why it's here.", "* Froggit hops to and fro.",
                "* The battlefield is filled\n  with the smell of mustard\n  seed.", "* You are intimidated by\n  Froggit's raw strength.\n* Only kidding."},
    .low_hp = "* Froggit is trying to run\n  away.", .spareable = "* Froggit seems reluctant to\n  fight you.",
    .act = act_froggit, .talk = talk_froggit, .attack = attack_froggit},
  [MON_FROGGIT] = {
    .name = "Froggit", .hp = 30, .at = 4, .df = 4, .exp = 3, .gold = 2, .spr_idle = SPR_FROGGIT_B, .spr_hurt = SPR_FROGGIT_HURT,
    .idle_speed = 8, .check = "Life is difficult for this\n  enemy.", .acts = {"Compliment", "Threaten"}, .nacts = 2,
    .neutral = {"* Froggit doesn't seem to know\n  why it's here.", "* Froggit hops to and fro.",
                "* The battlefield is filled\n  with the smell of mustard\n  seed.", "* You are intimidated by\n  Froggit's raw strength.\n* Only kidding."},
    .low_hp = "* Froggit is trying to run\n  away.", .spareable = "* Froggit seems reluctant to\n  fight you.",
    .act = act_froggit, .talk = talk_froggit, .attack = attack_froggit},
  [MON_WHIMSUN] = {
    .name = "Whimsun", .hp = 10, .at = 4, .df = 0, .exp = 2, .gold = 2, .spr_idle = SPR_WHIMSUN_B, .spr_hurt = SPR_WHIMSUN_HURT,
    .idle_speed = 4, .dy = -10, .check = "This monster is too sensitive\n  to fight...", .acts = {"Console", "Terrorize"}, .nacts = 2,
    .neutral = {"* Whimsun continues to mutter\n  apologies.", "* Whimsun avoids eye contact.", "* Whimsun is fluttering.",
                "* It's starting to smell like\n  lavender and mothballs."},
    .flags = MF_ONE_HIT | MF_SPARE_START, .act = act_whimsun, .talk = talk_whimsun, .attack = attack_whimsun, .flavor = flavor_whimsun},
  [MON_MOLDSMAL] = {
    .name = "Moldsmal", .hp = 50, .at = 4, .df = 0, .exp = 3, .gold = 3, .spr_idle = SPR_MOLDSMAL_B, .spr_hurt = SPR_MOLDSMAL_B,
    .idle_speed = 6, .check = "Stereotypical: Curvaceously\n  attractive, but no brains...", .acts = {"Imitate", "Flirt"}, .nacts = 2,
    .neutral = {"* Moldsmal waits pensively.", "* Moldsmal burbles quietly.", "* Moldsmal is ruminating.",
                "* The sound of slurping fills\n  the room.", "* Moldsmal hasn't brushed its\n  teeth."},
    .quotes = {"Squorch...", "Burble\nburb...", "*Slime\nsounds*", "*Sexy\nwiggle*"},
    .flags = MF_SPARE_START, .act = act_moldsmal, .attack = attack_moldsmal},
  [MON_LOOX] = {
    .name = "Loox", .hp = 50, .at = 5, .df = 4, .exp = 7, .gold = 5, .spr_idle = SPR_LOOX_B, .spr_hurt = SPR_LOOX_HURT,
    .idle_speed = 6, .check = "Don't pick on him.\n* Family name: Eyewalker", .acts = {"Don't Pick On", "Pick On"}, .nacts = 2,
    .neutral = {"* Loox is staring right\n  through you.", "* Loox is gazing at you.", "* Loox gnashes its teeth.",
                "* Smells like eyedrops."},
    .low_hp = "* Loox is watering.", .act = act_loox, .talk = talk_loox, .attack = attack_loox},
  [MON_VEGETOID] = {
    .name = "Vegetoid", .hp = 72, .at = 5, .df = 0, .exp = 6, .gold = 1, .spr_idle = SPR_VEGETOID_B, .spr_hurt = SPR_VEGETOID_HURT,
    .idle_speed = 6, .check = "Serving Size: 1 Monster\n* Not monitored by the USDA", .acts = {"Talk", "Devour", "Dinner"}, .nacts = 3,
    .neutral = {"* Vegetoid cackles softly.", "* Vegetoid's here for your\n  health.", "* Vegetoid gave a mysterious\n  smile.",
                "* It smells like steamed\n  carrots and peas."},
    .low_hp = "* Vegetoid seems kind of\n  bruised.", .act = act_vegetoid, .talk = talk_vegetoid, .attack = attack_vegetoid},
  [MON_MIGOSP] = {
    .name = "Migosp", .hp = 40, .at = 5, .df = 4, .exp = 5, .gold = 2, .spr_idle = SPR_MIGOSP_B, .spr_hurt = SPR_MIGOSP_HURT,
    .idle_speed = 4, .check = "It seems evil, but it's just\n  with the wrong crowd...", .acts = {"Talk"}, .nacts = 1,
    .neutral = {"* Migosp skitters around.", "* Migosp is knitting its brow.", "* Migosp is whispering to the\n  others.",
                "* It's starting to smell like a\n  roach motel."},
    .low_hp = "* Migosp refuses to give up.", .act = act_migosp, .talk = talk_migosp, .attack = attack_migosp,
    .flavor = flavor_migosp},
  [MON_NAPSTABLOOK] = {
    .name = "Napstablook", .hp = 88, .at = 3, .df = 4, .exp = 0, .gold = 0, .spr_idle = SPR_NAPSTABLOOK_B, .spr_hurt = SPR_NAPSTABLOOK_B,
    .idle_speed = 6, .dy = -16, .check = "This monster doesn't seem to\n  have a sense of humor...", .acts = {"Flirt", "Threat", "Cheer"}, .nacts = 3,
    .neutral = {"* Napstablook is wishing they\n  weren't here.", "* Napstablook is staring into\n  the distance.",
                "* Napstablook is pretending to\n  sleep.", "* The faint odor of ectoplasm\n  permeates the vicinity."},
    .flags = MF_BOSS, .act = act_napsta, .talk = talk_napsta, .attack = attack_napsta, .flavor = flavor_napsta,
    .on_hit = hit_napsta, .ends = ends_napsta},
};

extern const MonDef toriel_def;
const MonDef *mon_def(int id) {
  if (id >= MON_RUINS_COUNT) return snow_mon_def(id);
  return id == MON_TORIEL ? &toriel_def : &defs[id];
}

/* ---------------------------------------------------------------- formations */
static const Form forms[FORM_RUINS_COUNT] = {
  [FORM_DUMMY] = {{MON_DUMMY}, 1, "* You encountered the Dummy."},
  [FORM_FROGGIT_FIRST] = {{MON_FROGGIT_FIRST}, 1, "* Froggit attacks you!"},
  [FORM_FROGGIT] = {{MON_FROGGIT}, 1, "* Froggit hopped close!"},
  [FORM_WHIMSUN] = {{MON_WHIMSUN}, 1, "* Whimsun approached meekly!"},
  [FORM_FROGGIT_WHIMSUN] = {{MON_FROGGIT, MON_WHIMSUN}, 2, "* Froggit and Whimsun drew\n  near!"},
  [FORM_FROGGIT2] = {{MON_FROGGIT, MON_FROGGIT}, 2, "* A pair of Froggits hop\n  towards you."},
  [FORM_MOLDSMAL] = {{MON_MOLDSMAL}, 1, "* Moldsmal blocks the way!"},
  [FORM_MOLDSMAL2] = {{MON_MOLDSMAL, MON_MOLDSMAL}, 2, "* Moldsmal and Moldsmal block\n  the way."},
  [FORM_MOLDSMAL3] = {{MON_MOLDSMAL, MON_MOLDSMAL, MON_MOLDSMAL}, 3, "* You're surrounded by\n  Moldsmal!"},
  [FORM_MOLDSMAL_MIGOSP] = {{MON_MIGOSP, MON_MOLDSMAL}, 2, "* Migosp crawled up close!"},
  [FORM_LOOX] = {{MON_LOOX}, 1, "* Loox drew near!"},
  [FORM_LOOX2] = {{MON_LOOX, MON_LOOX}, 2, "* A pair of Loox decided to\n  pick on you!"},
  [FORM_VEGETOID] = {{MON_VEGETOID}, 1, "* Vegetoid came out of the\n  earth!"},
  [FORM_VEGETOID2] = {{MON_VEGETOID, MON_VEGETOID}, 2, "* A pair of Vegetoids came out\n  of the ground!"},
  [FORM_VEGETOID_MIGOSP] = {{MON_MIGOSP, MON_VEGETOID}, 2, "* Vegetoid came out of the\n  earth!"},
  [FORM_LOOX_VEG_MIGOSP] = {{MON_VEGETOID, MON_LOOX, MON_MIGOSP}, 3, "* Loox and co. decided to pick\n  on you!"},
  [FORM_NAPSTABLOOK] = {{MON_NAPSTABLOOK}, 1, "* Here comes Napstablook."},
  [FORM_TORIEL] = {{MON_TORIEL}, 1, "* Toriel blocks the way!"},
  [FORM_FLOWEY] = {{MON_FLOWEY}, 1, ""},
};

int form_setup(int form, Battle *b) {
  const Form *f = form >= FORM_RUINS_COUNT ? snow_form(form) : &forms[form];
  static const int16_t xs[3][3] = {{160}, {108, 212}, {70, 160, 250}};
  b->nmon = f->n;
  for (int i = 0; i < f->n; i++) {
    Monster *m = &b->mon[i];
    memset(m, 0, sizeof *m);
    m->def = mon_def(f->mon[i]);
    m->x = (int16_t)(xs[f->n - 1][i] + m->def->dx), m->y = (int16_t)(120 + m->def->dy);
  }
  /* Toriel and Flowey can't be fled from; Napstablook can */
  if (form == FORM_TORIEL || form == FORM_FLOWEY || form == FORM_PAPYRUS || form == FORM_MAD_DUMMY ||
      form == FORM_RGUARDS || form == FORM_BOMB || form == FORM_MUFFET || form == FORM_MTT_TILES || form == FORM_MTT_BOX ||
      form == FORM_MTT_EX)
    b->no_flee = true;
  b->captures = form == FORM_PAPYRUS;
  return 0;
}
const char *form_text(int form) { return form >= FORM_RUINS_COUNT ? snow_form(form)->text : forms[form].text; }

/* ---------------------------------------------------------------- attacks */
/* the board's inside, in pixels */
static int bx0(void) { return bt.x0 + 3; }
static int bx1(void) { return bt.x1 - 3; }
static int by0(void) { return bt.y0 + 3; }
static int by1(void) { return bt.y1 - 3; }
#define P(v) ((v) * FX)

static Bullet *shot(uint16_t spr, int x, int y, int vx, int vy) {
  Bullet *b = bullet_new(spr, P(x), P(y), vx, vy);
  if (b) b->owner = (uint8_t)bt.atk_owner;
  return b;
}

static int toward(int from, int to, int sp, int dist) { return dist ? (to - from) * sp / dist : 0; }
static int isqrt(int v) {
  int r = 0;
  while ((r + 1) * (r + 1) <= v) r++;
  return r;
}

static void atk_frog_flies(int t) {
  /* five flies come down one after another, stop, then dart at the SOUL */
  if (t < 0) return;
  if (t < 50 && t % 10 == 0) {
    Bullet *b = shot(SPR_FLYBULLET, bx0() + 6 + rnd_n(bx1() - bx0() - 12), by0() - 6, 0, P(3) / 2);
    if (b) b->kind = BK_SCRIPT, b->anim = 3, b->a = (int16_t)(by0() + 6 + rnd_n(20)), b->b = 0;
  }
  for (int i = 0; i < MAX_BULLETS; i++) {
    Bullet *b = &bullets[i];
    if (b->kind != BK_SCRIPT) continue;
    if (b->b == 0 && b->y / FX >= b->a) b->vy = 0, b->b = 1, b->c = 0;
    else if (b->b == 1 && ++b->c >= 14) {
      int dx = bt.sx - b->x, dy = bt.sy - b->y, d = isqrt((dx / 4) * (dx / 4) + (dy / 4) * (dy / 4)) * 4;
      b->vx = toward(b->x, bt.sx, P(2), d), b->vy = toward(b->y, bt.sy, P(2), d);
      b->b = 2;
    }
    b->x += b->vx, b->y += b->vy;
    b->t++;
    if (b->t % 3 == 0) b->frame++;
  }
}

static void atk_frog_jump(int t) {
  /* a little frog hops from the bottom right to the other side, with random force */
  if (t < 0) return;
  if (t == 0) {
    Bullet *b = shot(SPR_SMALLFROGBULLET, bx1() - 8, by1() - 8, 0, 0);
    if (b) b->kind = BK_SCRIPT, b->a = 20, b->r = 7;
  }
  for (int i = 0; i < MAX_BULLETS; i++) {
    Bullet *b = &bullets[i];
    if (b->kind != BK_SCRIPT) continue;
    if (b->a > 0) {
      b->frame = 0;
      if (--b->a == 0) b->vx = -(P(1) + rnd_n(P(3) / 2)), b->vy = -(P(2) + rnd_n(P(1)));
      continue;
    }
    b->frame = 1;
    b->vy += P(1) / 7;
    b->x += b->vx, b->y += b->vy;
    if (b->y >= P(by1() - 8)) {
      b->y = P(by1() - 8), b->vy = 0, b->vx = 0, b->a = 18;
    }
  }
}

static void atk_frog_rain(int t) {
  /* flies falling smoothly down the board, not aimed */
  if (t < 0) return;
  if (t < 100 && t % 9 == 0) {
    Bullet *b = shot(SPR_FLYBULLET, bx0() + 4 + rnd_n(bx1() - bx0() - 8), by0() - 6, 0, P(1));
    if (b) b->anim = 3;
  }
}

static void atk_whim_ring(int t) {
  /* a ring of moths around the board's middle, growing and shrinking */
  int n = 10;
  if (t < 0) return;
  if (t == 0)
    for (int k = 0; k < n; k++) {
      Bullet *b = shot(SPR_BUTTERFLYBLLT, board_cx(), board_cy(), 0, 0);
      if (b) b->kind = BK_SCRIPT, b->a = (int16_t)(k * 256 / n), b->anim = 4;
    }
  int r = 26 + gfx_sin(t * 2) * 14 / 1024;
  for (int i = 0; i < MAX_BULLETS; i++) {
    Bullet *b = &bullets[i];
    if (b->kind != BK_SCRIPT) continue;
    int a = b->a + t;
    b->x = P(board_cx()) + r * gfx_cos(a) * FX / 1024, b->y = P(board_cy()) + r * gfx_sin(a) * FX / 1024;
    if (++b->t % 4 == 0) b->frame++;
  }
}

static void atk_whim_cols(int t) {
  /* two columns of moths rising on either side of the SOUL, following it */
  if (t < 0) return;
  if (t == 0) bt.as[0] = bt.sx;
  bt.as[0] += (bt.sx - bt.as[0]) / 12;
  if (t < 120 && t % 7 == 0)
    for (int s = -1; s <= 1; s += 2) {
      Bullet *b = shot(SPR_BUTTERFLYBLLT, 0, by1() + 6, 0, -P(3) / 2);
      if (b) b->kind = BK_SCRIPT, b->a = (int16_t)s, b->anim = 4, b->x = bt.as[0] + s * P(16);
    }
  for (int i = 0; i < MAX_BULLETS; i++) {
    Bullet *b = &bullets[i];
    if (b->kind != BK_SCRIPT) continue;
    b->x = bt.as[0] + b->a * P(16);
    b->y += b->vy;
    if (++b->t % 4 == 0) b->frame++;
    if (b->y < P(by0() - 10)) b->kind = BK_NONE;
  }
}

static void atk_mold_zig(int t) {
  /* pellets falling in a zigzag */
  if (t < 0) return;
  if (t < 110 && t % 10 == 0) {
    Bullet *b = shot(SPR_BULLETMD, bx0() + 8 + rnd_n(bx1() - bx0() - 16), by0() - 4, P(1), P(1));
    if (b) b->kind = BK_SCRIPT;
  }
  for (int i = 0; i < MAX_BULLETS; i++) {
    Bullet *b = &bullets[i];
    if (b->kind != BK_SCRIPT) continue;
    if (++b->t % 14 == 0) b->vx = -b->vx;
    b->x += b->vx, b->y += b->vy;
  }
}

static void atk_mold_burst(int t) {
  /* pellets that fall a little way, then burst into rings of smaller ones */
  if (t < 0) return;
  if (t < 100 && t % 25 == 0) {
    Bullet *b = shot(SPR_BULLETMD, bx0() + 10 + rnd_n(bx1() - bx0() - 20), by0() - 4, 0, P(1));
    if (b) b->kind = BK_SCRIPT, b->a = (int16_t)(12 + rnd_n(16));
  }
  for (int i = 0; i < MAX_BULLETS; i++) {
    Bullet *b = &bullets[i];
    if (b->kind != BK_SCRIPT) continue;
    b->y += b->vy;
    if (++b->t >= b->a) {
      for (int k = 0; k < 8; k++) {
        Bullet *s = shot(SPR_BULLETSM, b->x / FX, b->y / FX, gfx_cos(k * 32) * P(1) / 1024, gfx_sin(k * 32) * P(1) / 1024);
        if (s) s->life = 60;
      }
      b->kind = BK_NONE;
    }
  }
}

static void atk_loox_bounce(int t) {
  /* spheres come in from the sides and bounce around the board, faster and faster */
  bool mad = bt.mon[bt.atk_owner].state2;
  if (t < 0) return;
  if (t < 40 && t % 8 == 0) {
    int s = (t / 8) % 2 ? 1 : -1;
    Bullet *b = shot(SPR_CIRCLEBULLETMD1, s < 0 ? bx0() - 4 : bx1() + 4, by0() + 8 + rnd_n(by1() - by0() - 16), -s * P(1), (rnd_n(2) ? 1 : -1) * P(1));
    if (b) b->kind = BK_SCRIPT, b->a = 0;
  }
  for (int i = 0; i < MAX_BULLETS; i++) {
    Bullet *b = &bullets[i];
    if (b->kind != BK_SCRIPT) continue;
    if (++b->t % 20 == 0) b->vx += b->vx / (mad ? 4 : 8), b->vy += b->vy / (mad ? 4 : 8);
    b->x += b->vx, b->y += b->vy;
    if (b->t > 8) {
      if (b->x < P(bx0() + 3) || b->x > P(bx1() - 3)) b->vx = -b->vx;
      if (b->y < P(by0() + 3) || b->y > P(by1() - 3)) b->vy = -b->vy;
    }
  }
}

static void atk_loox_worm(int t) {
  /* worms of spheres wiggling across the board */
  bool mad = bt.mon[bt.atk_owner].state2;
  if (t < 0) return;
  int every = mad ? 16 : 24;
  if (t < 110 && t % every == 0) {
    int s = (t / every) % 2 ? 1 : -1, y = by0() + 10 + rnd_n(by1() - by0() - 20);
    for (int k = 0; k < 4; k++) {
      Bullet *b = shot(SPR_CIRCLEBULLETSM, (s < 0 ? bx0() - 4 - k * 6 : bx1() + 4 + k * 6), y, -s * (mad ? P(3) / 2 : P(1)), 0);
      if (b) b->kind = BK_SCRIPT, b->a = (int16_t)y, b->b = (int16_t)(k * 20);
    }
  }
  for (int i = 0; i < MAX_BULLETS; i++) {
    Bullet *b = &bullets[i];
    if (b->kind != BK_SCRIPT) continue;
    b->t++;
    b->x += b->vx;
    b->y = P(b->a) + gfx_sin(b->t * 6 + b->b) * P(6) / 1024;
    if ((b->vx > 0 && b->x > P(bx1() + 8)) || (b->vx < 0 && b->x < P(bx0() - 8))) b->kind = BK_NONE;
  }
}

static void atk_veg_fall(int t) {
  /* vegetables fall and bounce off the bottom; after Dinner, one of them is green */
  Monster *m = &bt.mon[bt.atk_owner];
  if (t < 0) return;
  if (t < 100 && t % 14 == 0) {
    Bullet *b = shot(SPR_VEGBULLET, bx0() + 8 + rnd_n(bx1() - bx0() - 16), by0() - 6, (rnd_n(2) ? 1 : -1) * rnd_n(P(1)), P(1));
    if (b) {
      b->kind = BK_SCRIPT, b->frame = (uint8_t)rnd_n(3), b->a = 0;
      if (m->state == 1 && t == 42) b->color = BC_GREEN;
    }
  }
  for (int i = 0; i < MAX_BULLETS; i++) {
    Bullet *b = &bullets[i];
    if (b->kind != BK_SCRIPT) continue;
    b->vy += P(1) / 12;
    b->x += b->vx, b->y += b->vy;
    if (b->y > P(by1() - 5) && b->vy > 0 && !b->a) b->vy = -b->vy * 2 / 3, b->a = 1;
  }
  if (m->state == 1 && t == 99) m->state = 2, m->spare = true;
}

static void atk_veg_carrot(int t) {
  /* carrots dropping straight down */
  Monster *m = &bt.mon[bt.atk_owner];
  if (t < 0) return;
  if (t < 110 && t % 11 == 0) {
    Bullet *b = shot(SPR_CARROTBULLET, bx0() + 6 + rnd_n(bx1() - bx0() - 12), by0() - 8, 0, P(2));
    if (b && m->state == 1 && t == 44) b->color = BC_GREEN;
    if (b) b->kind = BK_SCRIPT;
  }
  for (int i = 0; i < MAX_BULLETS; i++) {
    Bullet *b = &bullets[i];
    if (b->kind != BK_SCRIPT) continue;
    b->y += b->vy;
    if (b->y > P(by1())) b->kind = BK_NONE;
  }
  if (m->state == 1 && t == 109) m->state = 2, m->spare = true;
}

static void atk_migosp_roach(int t) {
  /* roaches flying up the left and the right halves */
  if (t < 0) return;
  if (t < 110 && t % 8 == 0) {
    int left = (t / 8) % 2, w = (bx1() - bx0()) / 2 - 8;
    int x = left ? bx0() + 4 + rnd_n(w) : board_cx() + 4 + rnd_n(w);
    Bullet *b = shot(SPR_ROACHBULLET, x, by1() + 6, 0, -P(3) / 2);
    if (b) b->anim = 3;
  }
}

static void atk_migosp_dance(int t) {
  /* a tiny roach dancing at the bottom: touching it ends the turn */
  if (t < 0) return;
  if (t == 0) {
    Bullet *b = shot(SPR_ROACHDANCE, board_cx(), by1() - 6, P(1), 0);
    if (b) b->kind = BK_SCRIPT, b->anim = 5, b->flags = BF_HARMLESS;
  }
  for (int i = 0; i < MAX_BULLETS; i++) {
    Bullet *b = &bullets[i];
    if (b->kind != BK_SCRIPT) continue;
    b->x += b->vx;
    if (b->x < P(bx0() + 6) || b->x > P(bx1() - 6)) b->vx = -b->vx;
    if (++b->t % 5 == 0) b->frame++;
    int dx = b->x / FX - soul_x(), dy = b->y / FX - soul_y();
    if (dx * dx + dy * dy < 64) bt.atk_t = bt.atk_len;
  }
}

static void atk_napsta_tears(int t, int every) {
  /* tears arching out of Napstablook's eyes, then falling straight */
  if (t < 0) return;
  if (t < 120 && t % every == 0) {
    int s = rnd_n(2) ? 1 : -1;
    Bullet *b = shot(SPR_TEARDROP, board_cx() + s * 10, by0() - 4, s * (P(1) / 2 + rnd_n(P(1))), -P(1));
    if (b) b->kind = BK_GRAVITY, b->ay = P(1) / 16, b->dmg = 3;
  }
  for (int i = 0; i < MAX_BULLETS; i++) {
    Bullet *b = &bullets[i];
    if (b->kind == BK_GRAVITY && b->vy > P(1)) b->vx = b->vx * 7 / 8;
  }
}

static void atk_napsta_wiggle(int t) {
  /* long wiggly tears turning at the walls, falling once over the SOUL */
  if (t < 0) return;
  if (t < 100 && t % 20 == 0) {
    Bullet *b = shot(SPR_TEARDROP, bx0() + 6 + rnd_n(bx1() - bx0() - 12), by0() + 2, (rnd_n(2) ? 1 : -1) * P(3) / 2, 0);
    if (b) b->kind = BK_SCRIPT, b->dmg = 3, b->a = 0;
  }
  for (int i = 0; i < MAX_BULLETS; i++) {
    Bullet *b = &bullets[i];
    if (b->kind != BK_SCRIPT) continue;
    b->t++;
    if (!b->a) {
      b->x += b->vx;
      b->y = b->y + (gfx_sin(b->t * 10) > 0 ? FX / 4 : -FX / 4);
      if (b->x < P(bx0() + 4) || b->x > P(bx1() - 4)) b->vx = -b->vx;
      if (abs_i(b->x - bt.sx) < P(3) && b->t > 15) b->a = 1, b->vy = P(2);
    } else
      b->y += b->vy;
  }
}

static void atk_napsta_text(int t) {
  /* REALLY NOT FEELIN UP TO IT RIGHT NOW. SORRY. in grey, harmless: drawn by attack_draw */
  (void)t;
}

static void atk_napsta_hat(int t) {
  /* the tears rise to make a top hat on Napstablook's head */
  if (t < 0) return;
  if (t < 60 && t % 4 == 0) {
    Bullet *b = shot(SPR_TEARDROP, bx0() + 6 + rnd_n(bx1() - bx0() - 12), by1() - 2, 0, -P(2));
    if (b) b->flags |= BF_HARMLESS, b->kind = BK_SCRIPT;
  }
  for (int i = 0; i < MAX_BULLETS; i++) {
    Bullet *b = &bullets[i];
    if (b->kind != BK_SCRIPT) continue;
    b->y += b->vy;
    if (b->y < P(by0())) b->kind = BK_NONE;
  }
  if (t == 70) bt.mon[0].spr_override = SPR_NAPSTA_HAT;
}

static const int16_t lengths[ATK_RUINS_COUNT] = {
  [ATK_FROG_FLIES] = 150, [ATK_FROG_JUMP] = 150, [ATK_FROG_RAIN] = 150, [ATK_WHIM_RING] = 150, [ATK_WHIM_COLS] = 150,
  [ATK_MOLD_ZIG] = 150, [ATK_MOLD_BURST] = 150, [ATK_LOOX_BOUNCE] = 160, [ATK_LOOX_WORM] = 150, [ATK_VEG_FALL] = 150,
  [ATK_VEG_CARROT] = 150, [ATK_MIGOSP_ROACH] = 150, [ATK_MIGOSP_DANCE] = 180, [ATK_NAPSTA_TEARS] = 150,
  [ATK_NAPSTA_WIGGLE] = 150, [ATK_NAPSTA_TEXT] = 120, [ATK_NAPSTA_HAT] = 100, [ATK_NAPSTA_FLOOD] = 150,
  [ATK_TOR_HANDS] = 170, [ATK_TOR_WAVES] = 180, [ATK_TOR_HELIX] = 180, [ATK_TOR_FALTER] = 150, [ATK_IDLE] = 60,
};
int attack_length(int atk) {
  if (atk >= ATK_RUINS_COUNT) return snow_attack_length(atk);
  return atk >= 0 && lengths[atk] ? lengths[atk] : 150;
}

void toriel_attack(int atk, int t);   /* toriel.c */

void attack_run(Battle *b, int atk, int t) {
  (void)b;
  switch (atk) {
    case ATK_FROG_FLIES: atk_frog_flies(t); break;
    case ATK_FROG_JUMP: atk_frog_jump(t); break;
    case ATK_FROG_RAIN: atk_frog_rain(t); break;
    case ATK_WHIM_RING: atk_whim_ring(t); break;
    case ATK_WHIM_COLS: atk_whim_cols(t); break;
    case ATK_MOLD_ZIG: atk_mold_zig(t); break;
    case ATK_MOLD_BURST: atk_mold_burst(t); break;
    case ATK_LOOX_BOUNCE: atk_loox_bounce(t); break;
    case ATK_LOOX_WORM: atk_loox_worm(t); break;
    case ATK_VEG_FALL: atk_veg_fall(t); break;
    case ATK_VEG_CARROT: atk_veg_carrot(t); break;
    case ATK_MIGOSP_ROACH: atk_migosp_roach(t); break;
    case ATK_MIGOSP_DANCE: atk_migosp_dance(t); break;
    case ATK_NAPSTA_TEARS: atk_napsta_tears(t, 9); break;
    case ATK_NAPSTA_FLOOD: atk_napsta_tears(t, 3); break;
    case ATK_NAPSTA_WIGGLE: atk_napsta_wiggle(t); break;
    case ATK_NAPSTA_TEXT: atk_napsta_text(t); break;
    case ATK_NAPSTA_HAT: atk_napsta_hat(t); break;
    case ATK_TOR_HANDS: case ATK_TOR_WAVES: case ATK_TOR_HELIX: case ATK_TOR_FALTER: toriel_attack(atk, t); break;
    default: if (atk >= ATK_RUINS_COUNT) snow_attack_run(b, atk, t); break;
  }
}

void toriel_draw_attack(int atk, int t);   /* toriel.c */

void attack_draw(Battle *b, int atk, int t) {
  (void)b;
  if (atk == ATK_NAPSTA_TEXT) {
    /* the grey words drift down: harmless */
    static const char *const words[] = {"REALLY", "NOT", "FEELIN", "UP", "TO", "IT", "RIGHT", "NOW.", "SORRY."};
    for (int k = 0; k < 9; k++) {
      int y = by0() - 10 + (t - k * 8) * 2 / 3;
      if (y > by0() - 12 && y < by1()) gfx_text(FONT_SMALL, bx0() + 4 + (k * 23) % (bx1() - bx0() - 30), y, words[k], rgb565(0x808080), 255);
    }
  }
  if (atk >= ATK_TOR_HANDS && atk <= ATK_TOR_FALTER) toriel_draw_attack(atk, t);
  if (atk >= ATK_RUINS_COUNT) snow_attack_draw(b, atk, t);
}
