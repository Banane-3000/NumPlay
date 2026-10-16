/* The CORE's monsters: Final Froggit, Whimsalot, Astigmatism, Madjick and Knight Knight (stats, ACTs and how they're
 * spared from the Undertale Wiki's records of the game); then Mettaton's last show: his box until Frisk turns him
 * round and flips his switch, and Mettaton EX with the show's ratings. Their words are short ones of our own. */
#include <stdio.h>
#include "battle.h"
#include "game.h"
#include "gfx.h"
#include "items.h"
#include "monsters.h"
#include "res.h"
#include "world.h"

/* ---------------------------------------------------------------- Final Froggit: mystify it, or two compliments
 * or threats (a compliment lowers its ATTACK, a threat its DEFENSE) */
static void act_ffroggit(Battle *b, Monster *m, int act, char *t, int cap) {
  (void)b;
  if (act == 0) {
    snprintf(t, (size_t)cap, "* You compliment Final Froggit.\n* It understood. Its ATTACK dropped.");
    m->at = (int16_t)max_i(1, m->at - 2);
    if (++m->state >= 2) m->spare = true;
  } else if (act == 1) {
    snprintf(t, (size_t)cap, "* You threaten Final Froggit.\n* It understood. Its DEFENSE dropped.");
    m->df = (int16_t)(m->df - 2);
    if (++m->state >= 2) m->spare = true;
  } else if (!m->state2) {
    snprintf(t, (size_t)cap, "* You do something mysterious.\n* Final Froggit sees it still has\n  much to learn.");
    m->state2 = 1, m->spare = true;
  } else snprintf(t, (size_t)cap, "* You do something mysterious.\n* Nothing happens.");
}
static const char *talk_ffroggit(Battle *b, Monster *m) {
  (void)b;
  if (m->state2) return "(A thoughtful\ncroak.)";
  static const char *const lines[] = {"Skip, hop.", "Ribbit.", "Creak.", "Woof."};
  return lines[rnd_n(4)];
}
static int attack_ffroggit(Battle *b, Monster *m) { (void)b, (void)m; return rnd_n(2) ? ATK_FROG_FLIES : ATK_FROG_JUMP; }

/* ---------------------------------------------------------------- Whimsalot: console it, or pray twice */
static void act_whimsalot(Battle *b, Monster *m, int act, char *t, int cap) {
  (void)b;
  if (act == 0) {
    snprintf(t, (size_t)cap, "* You tell Whimsalot what it's\n  doing is all right.");
    m->spare = true, m->state = 1;
  } else if (act == 1) {
    snprintf(t, (size_t)cap, "* You leap at Whimsalot,\n  threateningly.");
    m->state = 2;
  } else {
    snprintf(t, (size_t)cap, "* You kneel and pray for safety.\n* Whimsalot remembers its\n  conscience.");
    if (++m->state2 >= 2) m->spare = true;
    m->state = 3;
  }
}
static const char *talk_whimsalot(Battle *b, Monster *m) {
  (void)b;
  if (m->state == 1) return "I'm at\npeace now.";
  if (m->state == 2) return "You don't\nscare me.";
  if (m->state == 3) return "There's\nstill hope.";
  static const char *const lines[] = {"My mind is\nmade up.", "Not this\ntime.", "No\nregrets.", "(Shine.)"};
  return lines[rnd_n(4)];
}
static int attack_whimsalot(Battle *b, Monster *m) { (void)b, (void)m; return rnd_n(2) ? ATK_WHIM_RING : ATK_WHIM_COLS; }

/* ---------------------------------------------------------------- Astigmatism: it says what it wants (pick on it, or
 * don't); do as it says, and it's done. Challenging it makes the others with it want to stop */
static void act_astig(Battle *b, Monster *m, int act, char *t, int cap) {
  if (act == 2) {
    bool others = false;
    for (int i = 0; i < b->nmon; i++) {
      Monster *o = &b->mon[i];
      if (o != m && o->alive && !o->gone) o->spare = true, others = true;
    }
    snprintf(t, (size_t)cap, others ? "* You stand up to Astigmatism.\n* The others are impressed." : "* You stand up to Astigmatism.");
    m->state2 = 3;
    return;
  }
  snprintf(t, (size_t)cap, act == 0 ? "* You pick on Astigmatism." : "* You don't pick on Astigmatism.");
  if (m->state && act == m->state - 1) m->spare = true, m->state2 = 1;
  else if (m->state) m->state2 = 2;
}
static const char *talk_astig(Battle *b, Monster *m) {
  (void)b;
  if (m->state2 == 1) return m->state == 1 ? "That's\nright!" : "Finally.\nSomeone\ngets it.";
  if (m->state2 == 2) return m->state == 1 ? "Scared?" : "How\ndare you.";
  if (m->state2 == 3) return "You dare\ndefy me?";
  if (!m->state) {
    /* what it wants: 1 to be picked on, 2 not */
    m->state = (int16_t)(1 + rnd_n(2));
    return m->state == 1 ? "... Pick\non me." : "... Don't\npick on me.";
  }
  static const char *const lines[] = {"You've seen\nenough.", "Take a last\nlook.", "...", "... ..."};
  return lines[rnd_n(4)];
}
static const char *flavor_astig(Battle *b, Monster *m) {
  (void)b;
  if (m->state2) m->state2 = 0;
  return m->spare ? "* Astigmatism doesn't care about\n  fighting anymore." : NULL;
}
static int attack_astig(Battle *b, Monster *m) { (void)b, (void)m; return rnd_n(2) ? ATK_LOOX_BOUNCE : ATK_LOOX_WORM; }

/* ---------------------------------------------------------------- Madjick: clear your mind, then talk; or raise the
 * Stick, a magic wand to it. Talking first dizzies Frisk (DEFENSE down) */
static void act_madjick(Battle *b, Monster *m, int act, char *t, int cap) {
  if (act == 0) {
    if (m->state) {
      snprintf(t, (size_t)cap, "* Madjick talks over you, to\n  itself. It seems satisfied.");
      m->spare = true;
    } else {
      snprintf(t, (size_t)cap, "* Madjick talks over you.\n* Its gibberish dizzies you...\n* Your DEFENSE drops.");
      b->df_loss += 1;
    }
  } else if (act == 1) {
    snprintf(t, (size_t)cap, "* You glare at its orbs.\n* They waver a little.");
    if (++m->state2 >= 2) m->spare = true;
  } else {
    snprintf(t, (size_t)cap, "* You think of pollen and sunshine.\n* Your DEFENSE goes up.");
    m->state = 1, b->df_loss -= 1;
  }
}
static bool item_madjick(Battle *b, Monster *m, int item, char *t, int cap) {
  (void)b;
  if (item != ITEM_STICK) return false;
  snprintf(t, (size_t)cap, "* You raise the Stick.\n* Madjick takes it for a wand!");
  m->spare = true, m->state = 2;
  return true;
}
static const char *talk_madjick(Battle *b, Monster *m) {
  (void)b;
  if (m->state == 2) return "Ah! A fellow\nwizard!";
  static const char *const lines[] = {"Abracadabra.", "Alakazam!", "Hocus\npocus.", "Please and\nthank you."};
  return lines[rnd_n(4)];
}
static const char *flavor_madjick(Battle *b, Monster *m) {
  (void)b;
  return m->spare ? "* Madjick believes you're a\n  fellow wizard." : NULL;
}
static int attack_madjick(Battle *b, Monster *m) { (void)b, (void)m; return rnd_n(2) ? ATK_MADJICK_CHASER : ATK_MADJICK_CORNER; }

/* ---------------------------------------------------------------- Knight Knight: three lullabies put her to sleep */
static void act_knight(Battle *b, Monster *m, int act, char *t, int cap) {
  (void)b;
  if (act == 0) {
    if (m->spare) snprintf(t, (size_t)cap, "* You talk to Knight Knight.\n* She hums in her sleep...");
    else snprintf(t, (size_t)cap, "* You ask Knight Knight about her\n  day. There's no answer.");
    m->state2++;
  } else {
    m->state++;
    if (m->state == 1) snprintf(t, (size_t)cap, "* You sing an old lullaby.\n* Knight Knight looks sleepy...");
    else if (m->state == 2) snprintf(t, (size_t)cap, "* You keep singing.\n* Her eyes start to close...");
    else if (m->state == 3) snprintf(t, (size_t)cap, "* You keep singing.\n* Knight Knight falls asleep."), m->spare = true;
    else snprintf(t, (size_t)cap, "* You keep singing.\n* Nothing happens.");
  }
}
static const char *talk_knight(Battle *b, Monster *m) {
  (void)b;
  if (m->spare) return "Zzzz...";
  if (m->state2 == 3) return "...... ......\n...... It's\nokay.";
  static const char *const lines[] = {"Good knight.", "Farewell.", "Adieu.", "Close your\neyes...", "Goodbye."};
  return lines[rnd_n(5)];
}
static const char *flavor_knight(Battle *b, Monster *m) {
  (void)b;
  return m->spare ? "* Knight Knight is snoring." : NULL;
}
static int attack_knight(Battle *b, Monster *m) {
  (void)b;
  if (m->spare) return ATK_NONE;
  static const uint8_t pick[3] = {ATK_KNIGHT_MOON, ATK_KNIGHT_SUN, ATK_KNIGHT_STAR};
  return pick[rnd_n(3)];
}

/* ---------------------------------------------------------------- Mettaton, in his box: no hurting him. Alphys calls
 * about the switch on his back; Turn (a mirror behind him!) and Frisk flips it */
static void act_mttbox(Battle *b, Monster *m, int act, char *t, int cap) {
  (void)m;
  if (act == 0 && b->state[0]) {
    snprintf(t, (size_t)cap, "* You point behind Mettaton:\n  \"Look! A mirror!\"|* He turns to look...\n* ...and you flip his switch!");
    b->state[1] = 1;
  } else snprintf(t, (size_t)cap, "* You look for a way around him.\n* There isn't one.");
}
static const char *talk_mttbox(Battle *b, Monster *m) {
  (void)m;
  if (b->state[1]) return "A MIRROR?\nWHERE?\n...OH NO.";
  if (b->turn <= 1) return "NO MORE\nGAMES,\nDARLING.";
  static const char *const lines[] = {"YOUR SOUL\nWILL MAKE ME\nA STAR!", "THE CROWD\nIS WAITING!", "STAY STILL,\nWON'T YOU?"};
  return lines[rnd_n(3)];
}
static const char *flavor_mttbox(Battle *b, Monster *m) {
  (void)m;
  if (b->turn == 2 && !b->state[0]) {
    b->state[0] = 1;
    return "* Alphys calls: \"The switch on his\n  back! Get him to turn around!\"";
  }
  return b->state[0] ? "* Mettaton blocks the way.\n* (Try turning him around.)" : "* Mettaton blocks the way.";
}
static bool hit_mttbox(Battle *b, Monster *m, int dmg) {
  (void)b, (void)dmg;
  m->dmg_shown = 0;
  return true;
}
static int attack_mttbox(Battle *b, Monster *m) { (void)m; return b->state[1] ? -1 : ATK_MTT_BOXES; }
static bool ends_mttbox(Battle *b, Monster *m) {
  (void)m;
  if (b->state[1]) b->result = BR_SCRIPTED;   /* (the battle ends here: the switch is flipped) */
  return false;
}

/* ---------------------------------------------------------------- Mettaton EX: the show's ratings. They start at 4000
 * and fall while Frisk dawdles; Boast raises them through his turn (unless he lands a hit), Pose by how hurt Frisk
 * is, Heel Turn has the crowd cheering every hit on Frisk; his brand's food, the Stick, a fight, a hit taken add
 * more (Junk Food takes some away). Past 12000 (10000 once his legs are gone) his show is a hit, and it ends. His
 * arms fall off after turn 11, his legs after turn 17; from then on, ratings count double */
enum { EXR_START = 4000, EXR_MAX = 30000 };
static struct {
  int ratings, boast, heel, hits, fights, brand, glam, stick, steak, body, last_hp, popup, popup_t, done;
  uint32_t last_frame;
} ex;
void mttex_reset(void) {
  ex.ratings = EXR_START, ex.boast = ex.heel = ex.hits = ex.fights = ex.brand = ex.glam = ex.stick = ex.steak = 0;
  ex.body = 0, ex.last_hp = sv.hp, ex.popup = 0, ex.popup_t = 0, ex.done = 0;
}
int mttex_ratings(void) { return ex.ratings; }
static int ex_target(void) { return ex.body >= 2 ? 10000 : 12000; }
static void ratings_add(int n) {
  if (n > 0 && ex.body >= 2) n *= 2;
  ex.ratings = min_i(EXR_MAX, max_i(0, ex.ratings + n));
  ex.popup = n, ex.popup_t = 45;
}
/* a hit on Frisk in his turn: Boast lost (the crowd's disappointed), or the crowd cheers */
static void ex_hit_taken(void) {
  if (ex.boast) {
    ex.boast = 0;
    ratings_add(-100);
    return;
  }
  static const int16_t violence[5] = {50, 25, 20, 15, 10};
  ratings_add(violence[min_i(ex.hits, 4)] + (ex.heel ? 100 : 0));
  ex.hits++;
}
static void act_mttex(Battle *b, Monster *m, int act, char *t, int cap) {
  (void)b, (void)m;
  if (act == 0) {
    snprintf(t, (size_t)cap, "* You say you won't get hit even\n  once.\n* Ratings climb through his turn.");
    ex.boast = 1;
  } else if (act == 1) {
    int n = sv.hp == 1 ? 600 : sv.hp < 4 ? 500 : sv.hp * 4 < sv.maxhp ? 400 : sv.hp * 2 < sv.maxhp ? 250 : sv.hp * 3 < sv.maxhp * 2 ? 150 : 100;
    const char *how = n >= 500 ? "With the last of your strength,\n  you pose. The audience screams." :
                      n >= 400 ? "Wounded as you are, you pose.\n  The audience gasps." :
                      n >= 150 ? "Hurt as you are, you pose.\n  The audience applauds." : "You pose dramatically.\n  The audience nods.";
    snprintf(t, (size_t)cap, "* %s", how);
    ratings_add(n);
  } else {
    snprintf(t, (size_t)cap, "* You turn and sneer at the\n  audience.\n* They want to see you hit!");
    ex.heel = 1;
  }
}
static bool item_mttex(Battle *b, Monster *m, int item, char *t, int cap) {
  (void)b, (void)m, (void)t, (void)cap;
  switch (item) {
    case ITEM_STICK:
      snprintf(t, (size_t)cap, "* You throw the Stick.\n* Mettaton catches it in his teeth,\n  and winks.");
      ratings_add(ex.stick++ ? 1 : 700);
      return true;
    case ITEM_MYSTERY_KEY:
      snprintf(t, (size_t)cap, "* You hold up the Mystery Key.\n* Mettaton pretends not to see it.");
      return true;
    case ITEM_GLAMBURGER: ratings_add(ex.glam++ ? 300 : 500); break;
    case ITEM_STARFAIT: case ITEM_LEGENDARY_HERO: ratings_add(ex.brand++ ? 200 : 300); break;
    case ITEM_STEAK: ratings_add(700); break;
    case ITEM_JUNK_FOOD: ratings_add(-50); break;
  }
  return false;   /* (eaten as ever) */
}
static bool hit_mttex(Battle *b, Monster *m, int dmg) {
  (void)b, (void)m, (void)dmg;
  static const int16_t action[5] = {300, 200, 150, 100, 50};
  ratings_add(action[min_i(ex.fights++, 4)]);
  return false;
}
static const char *talk_mttex(Battle *b, Monster *m) {
  (void)m;
  if (ex.ratings >= ex_target()) {
    ex.done = 1;
    return "OOH! THESE\nRATINGS!\nTHEY'RE\nINCREDIBLE!";
  }
  if (b->turn == 11) return "OH MY! MY\nARMS!\nSTILL... THE\nSHOW GOES ON!";
  if (b->turn == 17) return "MY LEGS?!\n...FINE.\nTHE FINALE,\nTHEN!";
  if (b->turn == 10) return "A UNION\nBREAK,\nDARLING.\nENJOY IT.";
  if (b->turn <= 1) return "LIGHTS!\nCAMERA!\nAND...\nGO!";
  static const char *const lines[] = {"ISN'T THIS\nBODY JUST\nDIVINE?", "SMILE FOR\nTHE CAMERA!", "THE CROWD\nLOVES YOU...\nFOR NOW!",
                                      "RATINGS!\nRATINGS!", "LET'S MAKE\nTHIS\nBEAUTIFUL!", "ONE MORE\nTIME, WITH\nFEELING!"};
  return lines[rnd_n(6)];
}
static const char *flavor_mttex(Battle *b, Monster *m) {
  (void)m;
  /* his limbs, after their turns */
  if (b->turn == 12 && ex.body < 1) ex.body = 1;
  if (b->turn == 18 && ex.body < 2) ex.body = 2;
  if (b->turn <= 1) return "* Mettaton EX makes his premiere!";
  if (ex.body == 2) return "* Mettaton's legs are gone.\n* He keeps on smiling.";
  if (ex.body == 1) return "* Mettaton's arms are gone.\n* The show must go on.";
  static const char *const lines[] = {"* Mettaton strikes a pose.", "* The audience watches, breathless.",
                                      "* Smells like a premiere.", "* The spotlight is on you both."};
  return lines[rnd_n(4)];
}
/* his turns, as the wiki lists them, shortened; from turn 18, the last two in turn */
static int attack_mttex(Battle *b, Monster *m) {
  (void)m;
  static const uint8_t seq[] = {ATK_EX_LEGS, ATK_EX_BOMBS, ATK_EX_ARMS, ATK_EX_HEART, ATK_EX_BOTS, ATK_EX_DISCO,
                                ATK_EX_DISCO, ATK_EX_PAIRS, ATK_EX_PAIRS, ATK_EX_BREAK, ATK_EX_HEART, ATK_EX_REWIND,
                                ATK_EX_REWIND, ATK_EX_LEGS, ATK_EX_BOTS, ATK_EX_PAIRS, ATK_EX_HEART, ATK_EX_LASER,
                                ATK_EX_BOTS};
  int n = (int)sizeof seq;
  int i = b->turn - 1;
  if (i >= n) i = n - 2 + (i - n) % 2;
  ex.last_hp = sv.hp;
  return seq[max_i(0, i)];
}
static bool ends_mttex(Battle *b, Monster *m) {
  (void)m;
  if (ex.done) b->result = BR_SCRIPTED;
  return false;
}
static void draw_mttex(Battle *b, Monster *m, int x, int y) {
  (void)b;
  /* (the ratings fall while Frisk takes their time) */
  if (g_frame != ex.last_frame) {
    ex.last_frame = g_frame;
    if (battle_ui() != BUI_NONE && g_frame % 6 == 0) ex.ratings = max_i(0, ex.ratings - 1);
    if (ex.popup_t) ex.popup_t--;
  }
  uint16_t s = m->hurt ? SPR_MTTEX_HURT : ex.body == 2 ? SPR_MTTEX_TOP : ex.body == 1 ? SPR_MTTEX_ARMLESS : SPR_MTTEX_B;
  int fr = m->hurt || ex.body ? 0 : (int)(g_frame / 3);
  if (m->dust) {
    int a = 255 - m->dust * 4;
    if (a > 0) gfx_sprite_color(s, 0, x, y, 0, C_WHITE, (uint8_t)a);
    return;
  }
  gfx_sprite(s, fr, x, y - (s == SPR_MTTEX_TOP ? 50 : 0), 0, m->fade ? (uint8_t)max_i(0, 128 - m->fade * 4) : 255);
  char r[32];
  snprintf(r, sizeof r, "RATINGS %d", ex.ratings);
  gfx_text(FONT_SMALL, 226, 6, r, C_WHITE, 255);
  if (ex.popup_t) {
    snprintf(r, sizeof r, "%+d", ex.popup);
    gfx_text(FONT_SMALL, 262, 16, r, ex.popup < 0 ? C_RED : C_YELLOW, 255);
  }
}

/* ---------------------------------------------------------------- the definitions */
static const MonDef core_defs[MON_COUNT - MON_HT_COUNT] = {
  [MON_FFROGGIT - MON_HT_COUNT] = {
    .name = "Final Froggit", .spare_gold = 50, .hp = 100, .at = 8, .df = 0, .exp = 120, .gold = 80,
    .spr_idle = SPR_FFROGGIT_B, .spr_hurt = SPR_FFROGGIT_HURT, .idle_speed = 6, .check = "Its future looks brighter and brighter.",
    .acts = {"Compliment", "Threaten", "Mystify"}, .nacts = 3,
    .neutral = {"* Final Froggit knows exactly why it's here.", "* Smells like mustard seed.",
                "* Final Froggit hops up and down, ominously."},
    .low_hp = "* Final Froggit stands its ground.", .spareable = "* Final Froggit seems reluctant to fight you.",
    .act = act_ffroggit, .talk = talk_ffroggit, .attack = attack_ffroggit},
  [MON_WHIMSALOT - MON_HT_COUNT] = {
    .name = "Whimsalot", .spare_gold = 50, .hp = 95, .at = 8, .df = 0, .exp = 110, .gold = 80,
    .spr_idle = SPR_WHIMSALOT_B, .spr_hurt = SPR_WHIMSALOT_HURT, .idle_speed = 6, .check = "It finally stopped worrying.",
    .acts = {"Console", "Terrorize", "Pray"}, .nacts = 3,
    .neutral = {"* Whimsalot spins its weapon around.", "* Smells like nightshade and bleach.",
                "* Whimsalot flutters silently.", "* Whimsalot locks eyes with you."},
    .low_hp = "* Whimsalot's flying stutters.", .spareable = "* Whimsalot doesn't want to fight anymore.",
    .act = act_whimsalot, .talk = talk_whimsalot, .attack = attack_whimsalot},
  [MON_ASTIG - MON_HT_COUNT] = {
    .name = "Astigmatism", .spare_gold = 50, .hp = 120, .at = 8, .df = 0, .exp = 130, .gold = 85,
    .spr_idle = SPR_ASTIG_B, .spr_hurt = SPR_ASTIG_HURT, .idle_speed = 8, .check = "This bully ALWAYS gets its way.",
    .acts = {"Pick On", "Don't Pick", "Challenge"}, .nacts = 3,
    .neutral = {"* Astigmatism gazes through your SOUL.", "* Astigmatism clicks its teeth.", "* Smells like glasses.",
                "* Astigmatism gives you a hypnotic glare."},
    .low_hp = "* Astigmatism is watering.",
    .act = act_astig, .talk = talk_astig, .attack = attack_astig, .flavor = flavor_astig},
  [MON_MADJICK - MON_HT_COUNT] = {
    .name = "Madjick", .spare_gold = 60, .hp = 190, .at = 8, .df = 0, .exp = 150, .gold = 120,
    .spr_idle = SPR_MADJICK_B, .spr_hurt = SPR_MADJICK_HURT, .idle_speed = 6, .check = "This magic mercenary only says magic words.",
    .acts = {"Talk", "Stare", "Clear Mind"}, .nacts = 3,
    .neutral = {"* Madjick does a mysterious jig.", "* Madjick flaunts its orbs.", "* Smells like magic.",
                "* Madjick whispers arcane swear words."},
    .act = act_madjick, .talk = talk_madjick, .attack = attack_madjick, .flavor = flavor_madjick, .on_item = item_madjick},
  [MON_KNIGHT - MON_HT_COUNT] = {
    .name = "Knight Knight", .spare_gold = 70, .hp = 230, .at = 8, .df = 2, .exp = 180, .gold = 150, .dy = 6,
    .spr_idle = SPR_KNIGHT_B, .spr_hurt = SPR_KNIGHT_HURT, .idle_speed = 8, .check = "This mercenary swings the Good Morningstar.",
    .acts = {"Talk", "Sing"}, .nacts = 2,
    .neutral = {"* Knight Knight smashes her morningstar.", "* Knight Knight breathes deeply.", "* Smells like stardust.",
                "* Knight Knight watches quietly."},
    .act = act_knight, .talk = talk_knight, .attack = attack_knight, .flavor = flavor_knight},
  [MON_MTT_BOX - MON_HT_COUNT] = {
    .name = "Mettaton", .hp = 9999, .at = 6, .df = 0, .spr_idle = SPR_MTT_B, .spr_hurt = SPR_MTT_B, .idle_speed = 6,
    .check = "His metal body makes him invincible.", .acts = {"Turn"}, .nacts = 1,
    .flags = MF_BOSS | MF_NOT_KILLABLE, .act = act_mttbox, .talk = talk_mttbox, .attack = attack_mttbox,
    .flavor = flavor_mttbox, .on_hit = hit_mttbox, .ends = ends_mttbox},
  [MON_MTT_EX - MON_HT_COUNT] = {
    .name = "Mettaton EX", .hp = 1600, .at = 6, .df = 1, .exp = 800, .gold = 0, .spr_idle = SPR_MTTEX_B,
    .spr_hurt = SPR_MTTEX_HURT, .idle_speed = 3, .check = "His weak point is his heart-shaped core.",
    .acts = {"Boast", "Pose", "Heel Turn"}, .nacts = 3,
    .flags = MF_BOSS, .act = act_mttex, .talk = talk_mttex, .attack = attack_mttex, .flavor = flavor_mttex,
    .on_hit = hit_mttex, .draw = draw_mttex, .ends = ends_mttex, .on_item = item_mttex},
};
const MonDef *core_mon_def(int id) { return &core_defs[id - MON_HT_COUNT]; }

static const Form core_forms[FORM_COUNT - FORM_HT_COUNT] = {
  [FORM_ASTIG - FORM_HT_COUNT] = {{MON_ASTIG}, 1, "* Astigmatism draws near."},
  [FORM_WHIM_FROG - FORM_HT_COUNT] = {{MON_WHIMSALOT, MON_FFROGGIT}, 2, "* Whimsalot and Final Froggit appear."},
  [FORM_WHIM_ASTIG - FORM_HT_COUNT] = {{MON_WHIMSALOT, MON_ASTIG}, 2, "* Whimsalot and Astigmatism appear."},
  [FORM_FROG_ASTIG - FORM_HT_COUNT] = {{MON_FFROGGIT, MON_ASTIG}, 2, "* Final Froggit and Astigmatism appear."},
  [FORM_KNIGHT_MADJICK - FORM_HT_COUNT] = {{MON_KNIGHT, MON_MADJICK}, 2, "* Mercenaries step out of the\n  shadows."},
  [FORM_FROG_ASTIG_WHIM - FORM_HT_COUNT] = {{MON_FFROGGIT, MON_ASTIG, MON_WHIMSALOT}, 3, "* What a nightmare!"},
  [FORM_KNIGHT - FORM_HT_COUNT] = {{MON_KNIGHT}, 1, "* Knight Knight blocks the way!"},
  [FORM_MADJICK - FORM_HT_COUNT] = {{MON_MADJICK}, 1, "* Madjick pops out of its hat!"},
  [FORM_MTT_BOX - FORM_HT_COUNT] = {{MON_MTT_BOX}, 1, "* Mettaton attacks!"},
  [FORM_MTT_EX - FORM_HT_COUNT] = {{MON_MTT_EX}, 1, "* Mettaton EX makes his premiere!"},
};
const Form *core_form(int form) { return &core_forms[form - FORM_HT_COUNT]; }

/* random encounters in the Core Branch (Astigmatism alone just once) */
int core_encounter(int room) {
  if (room != ROOM_CORE_BRANCH) return -1;
  static const uint8_t pool[] = {FORM_WHIM_FROG, FORM_WHIM_ASTIG, FORM_FROG_ASTIG, FORM_KNIGHT_MADJICK,
                                 FORM_FROG_ASTIG_WHIM, FORM_KNIGHT, FORM_MADJICK};
  if (!flag(F_CORE_ASTIG)) {
    set_flag(F_CORE_ASTIG, 1);
    return FORM_ASTIG;
  }
  return pool[rnd_n((int)sizeof pool)];
}

/* ---------------------------------------------------------------- the attacks */
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
static Bullet *rect(int x, int y, int w, int h, int vx, int vy) {
  Bullet *b = shot(SPR_RECT, x, y, vx, vy);
  if (b) b->flags |= BF_BOX, b->w = (int16_t)w, b->h = (int16_t)h;
  return b;
}
/* a shot of the yellow SOUL's within (rx, ry) of b: it's spent */
static bool shot_at(const Bullet *b, int rx, int ry) {
  for (int i = 0; i < MAX_BULLETS; i++) {
    Bullet *s = &bullets[i];
    if (s->kind == BK_NONE || !(s->flags & BF_PLAYER)) continue;
    if (abs_i(s->x - b->x) < P(rx) && abs_i(s->y - b->y) < P(ry)) {
      s->kind = BK_NONE;
      return true;
    }
  }
  return false;
}
static int count_spr(uint16_t spr) {
  int n = 0;
  for (int i = 0; i < MAX_BULLETS; i++) n += bullets[i].kind != BK_NONE && bullets[i].spr == spr;
  return n;
}

/* Madjick: a chaser orb that leaves crosses behind; a corner orb, where the SOUL heads, firing crosses at it */
static void atk_madjick_chaser(int t) {
  static Bullet *orb;
  if (t == 0) {
    orb = shot(SPR_ORB, bx1() - 8, by0() + 8, 0, 0);
    if (orb) orb->kind = BK_HOMING, orb->a = 3, orb->b = P(2), orb->r = 5;
  }
  if (orb && orb->kind == BK_HOMING && t % 14 == 7 && t < 150) {
    Bullet *c = shot(SPR_STAR, orb->x / FX, orb->y / FX, 0, 0);
    if (c) c->r = 3, c->life = 70;
  }
}
static void atk_madjick_corner(int t) {
  static Bullet *orb;
  static int corner;
  if (t == 0) corner = 0, orb = shot(SPR_ORB, bx1() - 6, by0() + 6, 0, 0);
  if (!orb || orb->spr != SPR_ORB) return;
  orb->flags |= BF_HARMLESS;
  /* (it moves to the corner of the way the SOUL went last) */
  int c = soul_x() < board_cx() ? (soul_y() < board_cy() ? 1 : 2) : (soul_y() < board_cy() ? 0 : 3);
  if (t % 30 == 0) corner = c;
  int tx = corner == 1 || corner == 2 ? bx0() + 6 : bx1() - 6, ty = corner == 0 || corner == 1 ? by0() + 6 : by1() - 6;
  orb->x += (P(tx) - orb->x) / 6, orb->y += (P(ty) - orb->y) / 6;
  if (t % 12 == 6 && t < 150) {
    int dx = soul_x() - orb->x / FX, dy = soul_y() - orb->y / FX, d = abs_i(dx) + abs_i(dy);
    if (d) {
      Bullet *s = shot(SPR_STAR, orb->x / FX, orb->y / FX, dx * P(3) / d, dy * P(3) / d);
      if (s) s->r = 3;
    }
  }
}
/* Knight Knight: the Moon's meteors from the top left; the Sun's flares, spiralling out; her morningstars' slams */
static void atk_knight_moon(int t) {
  if (t % 6 == 0 && t < 150) {
    int x = bx0() - 30 + rnd_n(bx1() - bx0()), y = by0() - 10;
    Bullet *s = shot(SPR_STAR, x, y, P(2), P(3));
    if (s) s->r = 3;
  }
}
static void atk_knight_sun(int t) {
  static Bullet *sun;
  if (t == 0) {
    sun = shot(SPR_ORB, board_cx(), board_cy(), 0, 0);
    if (sun) sun->r = 5;
  }
  if (t % 4 == 0 && t < 150) {
    int a = t * 5;
    for (int k = 0; k < 2; k++) {
      int aa = a + k * 128;
      Bullet *s = shot(SPR_STAR, board_cx(), board_cy(), gfx_cos(aa) * 2 * FX / 1024, gfx_sin(aa) * 2 * FX / 1024);
      if (s) s->r = 3;
    }
  }
  if (sun && t == 150) sun->kind = BK_NONE;
}
static void atk_knight_star(int t) {
  /* five heads, at the top or the bottom; they shine a moment, then slam across */
  static int8_t top;
  int k = t % 50;
  if (k == 0 && t < 150) {
    top = (int8_t)rnd_n(2);
    int w = bx1() - bx0();
    for (int i = 0; i < 5; i++) {
      if (i == rnd_n(5)) continue;
      Bullet *s = shot(SPR_ORB, bx0() + w * (2 * i + 1) / 10, top ? by0() + 4 : by1() - 4, 0, 0);
      if (s) s->r = 6, s->flags |= BF_HARMLESS, s->life = 50, s->alpha = 120;
    }
  }
  if (k == 15)
    for (int i = 0; i < MAX_BULLETS; i++)
      if (bullets[i].kind != BK_NONE && bullets[i].spr == SPR_ORB && (bullets[i].flags & BF_HARMLESS))
        bullets[i].flags &= (uint8_t)~BF_HARMLESS, bullets[i].alpha = 255, bullets[i].vy = top ? P(5) : -P(5);
}

/* Mettaton EX's. Shooting what he sends (the yellow SOUL) is good for the ratings, as his heart is */
static void ex_watch_hits(void) {
  if (sv.hp < ex.last_hp) ex_hit_taken();
  ex.last_hp = sv.hp;
  if (ex.boast && bt.atk_t % 9 == 0) ratings_add(3 + (bt.atk_t / 9) % 2);
}
static void atk_ex_legs(int t) {
  if (t % 26 == 0 && t < 130) {
    bool left = rnd_n(2);
    int y = by0() + 6 + rnd_n(by1() - by0() - 12);
    Bullet *w = rect(left ? bx0() + 2 : bx1() - 2, y, 3, 9, 0, 0);   /* the warning: where it comes */
    if (w) w->flags |= BF_HARMLESS, w->color = BC_ORANGE, w->life = 12;
    Bullet *l = shot(SPR_EXLEG, left ? bx0() - 40 : bx1() + 40, y, left ? P(7) : -P(7), 0);
    if (l) l->flags |= BF_BOX, l->w = 56, l->h = 7, l->angle = 64, l->c = 12, l->kind = BK_SCRIPT;
  }
  /* (after their warning, they kick across) */
  for (int i = 0; i < MAX_BULLETS; i++) {
    Bullet *l = &bullets[i];
    if (l->kind != BK_SCRIPT || l->spr != SPR_EXLEG) continue;
    if (l->c) l->c--;
    else l->x += l->vx;
    if (l->x < P(bx0() - 70) || l->x > P(bx1() + 70)) l->kind = BK_NONE;
  }
}
/* rows of bombs and blocks; a shot bomb blows up its row and its column; with `rewind`, back up they come */
static void ex_blast(int x, int y) {
  Bullet *h = rect(board_cx(), y, bx1() - bx0(), 6, 0, 0);
  if (h) h->life = 8, h->color = BC_WHITE;
  Bullet *v = rect(x, board_cy(), 6, by1() - by0(), 0, 0);
  if (v) v->life = 8;
  for (int i = 0; i < MAX_BULLETS; i++) {
    Bullet *b = &bullets[i];
    if (b->kind == BK_NONE || (b->spr != SPR_EXBLOCK && b->spr != SPR_BOMB)) continue;
    if (abs_i(b->y / FX - y) < 6 || abs_i(b->x / FX - x) < 6) b->kind = BK_NONE;
  }
}
static void ex_rows(int t, bool rewind, int cells) {
  int w = bx1() - bx0(), cw = w / cells;
  if (t % 34 == 0 && t < (rewind ? 100 : 136)) {
    int bomb = rnd_n(cells);
    for (int k = 0; k < cells; k++) {
      bool is_bomb = rewind ? k == bomb : k != bomb;
      Bullet *b = shot(is_bomb ? SPR_BOMB : SPR_EXBLOCK, bx0() + cw * k + cw / 2, by0() - 8, 0, P(1));
      if (b) b->r = 6, b->flags |= BF_NOCLIP, b->kind = BK_SCRIPT, b->vy = P(1);
    }
  }
  for (int i = 0; i < MAX_BULLETS; i++) {
    Bullet *b = &bullets[i];
    if (b->kind != BK_SCRIPT || (b->spr != SPR_EXBLOCK && b->spr != SPR_BOMB)) continue;
    if (rewind && t == 110) b->vy = -P(1);
    b->y += b->vy;
    if (shot_at(b, 7, 7)) {
      ratings_add(20);
      b->kind = BK_NONE;
      if (b->spr == SPR_BOMB) ex_blast(b->x / FX, b->y / FX);
    }
    if (b->y > P(by1() + 30) || b->y < P(by0() - 40)) b->kind = BK_NONE;
  }
}
/* pairs: a bomb beside a block; only the bomb's blast breaks the block */
static void atk_ex_pairs(int t) {
  int w = bx1() - bx0();
  if (t % 36 == 0 && t < 144) {
    int side = rnd_n(2);
    for (int k = 0; k < 2; k++) {
      Bullet *b = shot(k == side ? SPR_BOMB : SPR_EXBLOCK, bx0() + w / 4 + k * w / 2, by0() - 8, 0, 0);
      if (b) b->r = 6, b->flags |= BF_NOCLIP, b->kind = BK_SCRIPT, b->vy = P(1) + P(1) / 4;
    }
  }
  for (int i = 0; i < MAX_BULLETS; i++) {
    Bullet *b = &bullets[i];
    if (b->kind != BK_SCRIPT || (b->spr != SPR_EXBLOCK && b->spr != SPR_BOMB)) continue;
    b->y += b->vy;
    if (b->spr == SPR_BOMB && shot_at(b, 7, 7)) {
      ratings_add(20);
      int x = b->x / FX, y = b->y / FX;
      b->kind = BK_NONE;
      for (int j = 0; j < MAX_BULLETS; j++)   /* its block, beside it */
        if (bullets[j].kind == BK_SCRIPT && bullets[j].spr == SPR_EXBLOCK && abs_i(bullets[j].y / FX - y) < 6) bullets[j].kind = BK_NONE;
      Bullet *v = rect(x, board_cy(), 6, by1() - by0(), 0, 0);   /* (and its column, up and down) */
      if (v) v->life = 8;
    } else if (b->spr == SPR_EXBLOCK) shot_at(b, 7, 7);   /* (a block just stops a shot) */
    if (b->y > P(by1() + 30)) b->kind = BK_NONE;
  }
}
/* his long arms come down; shooting the box on one sends it back up */
static void atk_ex_arms(int t) {
  if (t % 40 == 0 && t < 120) {
    int w = bx1() - bx0();
    for (int k = 0; k < 3; k++) {
      int x = bx0() + w * (2 * k + 1) / 6 + (t / 40 % 2 ? 8 : -8);
      Bullet *a = rect(x, by0() - 40, 4, 70, 0, 0);
      if (!a) continue;
      a->kind = BK_SCRIPT, a->vy = P(1), a->flags |= BF_NOCLIP, a->a = 1;
      Bullet *box = shot(SPR_EXARM, x, by0() - 5, 0, 0);
      if (box) box->kind = BK_SCRIPT, box->vy = P(1), box->flags |= BF_NOCLIP | BF_HARMLESS, box->a = 2, box->b = (int16_t)(a - bullets);
    }
  }
  for (int i = 0; i < MAX_BULLETS; i++) {
    Bullet *b = &bullets[i];
    if (b->kind != BK_SCRIPT || (b->a != 1 && b->a != 2)) continue;
    b->y += b->vy;
    if (b->a == 2 && b->vy > 0 && shot_at(b, 7, 8)) {
      ratings_add(20);
      b->vy = -P(3);
      Bullet *arm = &bullets[b->b];
      if (arm->kind == BK_SCRIPT && arm->a == 1) arm->vy = -P(3);
    }
    if (b->y < P(by0() - 80) || b->y > P(by1() + 60)) b->kind = BK_NONE;
  }
}
/* his heart, out of his waist: sparks in a ring, again and again; enough shots at it and his turn is over */
static Bullet *heart;
static int heart_hp;
static void atk_ex_heart(int t, bool laser) {
  if (t == 0) {
    heart = shot(SPR_MTTHEART, board_cx(), by0() + 12, laser ? P(2) : P(1), 0);
    if (heart) heart->kind = BK_SCRIPT, heart->flags |= BF_HARMLESS | BF_NOCLIP;
    heart_hp = laser ? 25 : 20;
  }
  Bullet *h = heart;
  if (!h || h->kind != BK_SCRIPT || h->spr != SPR_MTTHEART) return;
  h->x += h->vx;
  if (h->x < P(bx0() + 8) || h->x > P(bx1() - 8)) h->vx = -h->vx;
  if (h->c) h->c--, h->alpha = h->c % 2 ? 90 : 255;
  if (shot_at(h, 9, 8)) heart_hp--, h->c = 6, ratings_add(20);
  if (t % 30 == 29) heart_hp--;   /* (it runs down by itself, too) */
  if (heart_hp <= 0) {
    h->kind = BK_NONE;
    bt.atk_len = min_i(bt.atk_len, t + 20);
    return;
  }
  if (!laser && t % 28 == 14) {
    for (int k = 0; k < 8; k++) {
      int a = k * 32 + t;
      Bullet *s = shot(SPR_BOLT, h->x / FX, h->y / FX, gfx_cos(a) * 3 * FX / 2048, gfx_sin(a) * 3 * FX / 2048);
      if (s) s->r = 3;
    }
  }
  if (laser && t % 20 == 10) {   /* three lines of sparks, at where the SOUL is */
    int dx = soul_x() - h->x / FX, dy = soul_y() - h->y / FX, d = abs_i(dx) + abs_i(dy);
    if (d)
      for (int k = -1; k <= 1; k++) {
        Bullet *s = shot(SPR_BOLT, h->x / FX, h->y / FX, (dx + k * 12) * P(3) / d, dy * P(3) / d);
        if (s) s->r = 3;
      }
  }
}
/* his bots: a swarm coming down; all shot, and his turn is over */
static void atk_ex_bots(int t) {
  if (t == 0) {
    int w = bx1() - bx0();
    for (int k = 0; k < 9; k++) {
      Bullet *b = shot(SPR_MTTMINI, bx0() + w * (k % 3 * 2 + 1) / 6, by0() - 14 - (k / 3) * 26, 0, 0);
      if (b) b->kind = BK_SCRIPT, b->r = 7, b->flags |= BF_NOCLIP, b->vy = P(1) / 2, b->a = (int16_t)rnd_n(256);
    }
  }
  for (int i = 0; i < MAX_BULLETS; i++) {
    Bullet *b = &bullets[i];
    if (b->kind != BK_SCRIPT || b->spr != SPR_MTTMINI) continue;
    b->y += b->vy, b->a = (int16_t)(b->a + 4);
    b->x += gfx_sin(b->a) * FX / 1024;
    if (shot_at(b, 8, 10)) b->kind = BK_NONE, ratings_add(20);
    if (b->y > P(by1() + 20)) b->kind = BK_NONE;
  }
  if (t > 10 && !count_spr(SPR_MTTMINI)) bt.atk_len = min_i(bt.atk_len, t + 10);
}
/* the disco ball: its beams sweep across the board, blue or white; a shot at the ball turns them */
static void atk_ex_disco(int t, bool fast) {
  static Bullet *ball;
  static uint8_t col;
  if (t == 0) {
    ball = shot(SPR_DISCO, board_cx(), by0() + 8, 0, 0);
    if (ball) ball->kind = BK_SCRIPT, ball->flags |= BF_HARMLESS | BF_NOCLIP;
    col = BC_BLUE;
  }
  if (ball && ball->kind == BK_SCRIPT && ball->spr == SPR_DISCO && shot_at(ball, 9, 9)) {
    col = col == BC_BLUE ? BC_WHITE : BC_BLUE;
    ratings_add(5);
    for (int i = 0; i < MAX_BULLETS; i++)
      if (bullets[i].kind != BK_NONE && bullets[i].spr == SPR_RECT && bullets[i].a == 3) bullets[i].color = col;
  }
  int every = fast ? 22 : 34;
  if (t > (fast ? 10 : 40) && t % every == 0 && t < 140) {
    bool left = (t / every) % 2;
    Bullet *b = rect(left ? bx0() : bx1(), board_cy(), 3, by1() - by0(), left ? P(fast ? 3 : 2) : -P(fast ? 3 : 2), 0);
    if (b) b->color = col, b->a = 3, b->life = 120;
  }
}

int core_attack_length(int atk) {
  switch (atk) {
    case ATK_EX_BREAK: return 90;
    case ATK_EX_REWIND: return 200;
    default: return 165;
  }
}

void core_attack_run(Battle *b, int atk, int t) {
  (void)b;
  bool ex_atk = atk >= ATK_EX_LEGS;
  if (t < 0) {
    int w = atk == ATK_EX_BOMBS || atk == ATK_EX_REWIND ? 76 : atk == ATK_EX_PAIRS ? 44 : ex_atk ? 120 : 130;
    board_set(w, ex_atk ? 110 : 100);
    soul_set_mode(atk == ATK_EX_LEGS || atk == ATK_EX_BREAK || !ex_atk ? SOUL_RED : SOUL_YELLOW);
    heart = NULL;
    return;
  }
  if (ex_atk) ex_watch_hits();
  switch (atk) {
    case ATK_MADJICK_CHASER: atk_madjick_chaser(t); break;
    case ATK_MADJICK_CORNER: atk_madjick_corner(t); break;
    case ATK_KNIGHT_MOON: atk_knight_moon(t); break;
    case ATK_KNIGHT_SUN: atk_knight_sun(t); break;
    case ATK_KNIGHT_STAR: atk_knight_star(t); break;
    case ATK_EX_LEGS: atk_ex_legs(t); break;
    case ATK_EX_BOMBS: ex_rows(t, false, 4); break;
    case ATK_EX_REWIND: ex_rows(t, true, 4); break;
    case ATK_EX_ARMS: atk_ex_arms(t); break;
    case ATK_EX_HEART: atk_ex_heart(t, false); break;
    case ATK_EX_LASER: atk_ex_heart(t, true); break;
    case ATK_EX_BOTS: atk_ex_bots(t); break;
    case ATK_EX_DISCO: atk_ex_disco(t, ex.hits < 2 && bt.turn >= 7); break;
    case ATK_EX_PAIRS: atk_ex_pairs(t); break;
    case ATK_EX_BREAK: break;
  }
}
