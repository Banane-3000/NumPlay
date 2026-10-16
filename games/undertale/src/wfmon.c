/* Waterfall's monsters: stats and ACTs (from the Undertale Wiki's records of the game), their formations, where
 * they come out and their attacks. Their words are short ones of our own. */
#include <stdio.h>
#include <string.h>
#include "battle.h"
#include "game.h"
#include "gfx.h"
#include "monsters.h"
#include "res.h"
#include "world.h"

/* ---------------------------------------------------------------- Aaron: flex three times and he flexes away */
static void act_aaron(Battle *b, Monster *m, int act, char *t, int cap) {
  (void)b;
  if (act == 0) {   /* Flex */
    m->state++;
    m->at = (int16_t)(m->at + 1);
    if (m->state == 1) snprintf(t, (size_t)cap, "* You flex at Aaron.\n* He takes it as a challenge and flexes back!");
    else if (m->state == 2) snprintf(t, (size_t)cap, "* You flex harder.\n* Aaron flexes harder still!");
    else {
      snprintf(t, (size_t)cap, "* You flex with everything you've got.\n* Aaron flexes so hard he flexes himself out of the room.");
      m->gone = true, m->fade = 1, m->spare = true;
    }
  } else {   /* Shoo */
    snprintf(t, (size_t)cap, "* You tell Aaron to go away.\n* He just flexes at you.");
  }
}
static const char *talk_aaron(Battle *b, Monster *m) {
  (void)b;
  static const char *const lines[] = {"Check out\nthese fins\n;)", "Feeling the\nburn?\n;)", "Don't be\nshy ;)",
                                      "Want a\ncloser look?\n;)"};
  if (m->state == 1) return "Oh, a\ncontest?\n;)";
  if (m->state == 2) return "You're\ngood...\n;)";
  return lines[rnd_n(4)];
}
static int attack_aaron(Battle *b, Monster *m) { (void)b, (void)m; return rnd_n(2) ? ATK_AARON_ARMS : ATK_AARON_SWEAT; }

/* ---------------------------------------------------------------- Woshua: ask to be cleaned, catch its green */
static void act_woshua(Battle *b, Monster *m, int act, char *t, int cap) {
  (void)b;
  if (act == 0) {   /* Clean */
    snprintf(t, (size_t)cap, "* You ask Woshua to clean you.\n* It gets very excited.");
    if (!m->state) m->state = 1;
  } else if (act == 1) {   /* Touch */
    if (m->spare) snprintf(t, (size_t)cap, "* You give Woshua a friendly pat.\n* It wobbles happily.");
    else snprintf(t, (size_t)cap, "* Woshua won't let you touch it.\n* You're not clean enough!");
  } else {   /* Joke */
    snprintf(t, (size_t)cap, "* You tell a dirty joke.\n* Woshua is horrified.");
  }
}
static const char *talk_woshua(Battle *b, Monster *m) {
  (void)b;
  static const char *const lines[] = {"Scrub\nscrub!", "Germs,\nbegone!", "Wash! Rinse!\nRepeat!", "Too\ndirty!"};
  if (m->state == 1) return "Hold\nstill!\nWashing!";
  if (m->state == 2) return "Squeaky\nclean!";
  return lines[rnd_n(4)];
}
static const char *flavor_woshua(Battle *b, Monster *m) {
  (void)b;
  if (m->state == 1) return "* Woshua's water looks clean and green.";
  if (m->state == 2) return "* Woshua is proud of how clean you are.";
  return NULL;
}
static int attack_woshua(Battle *b, Monster *m) {
  (void)b;
  if (m->state == 1) m->state2 = (int16_t)b->greens;   /* (what it had healed before this one) */
  return m->state == 1 || rnd_n(2) ? ATK_WOSHUA_DROPS : ATK_WOSHUA_BUBBLES;
}

/* ---------------------------------------------------------------- Moldbygg: let go of it */
static void act_moldbygg(Battle *b, Monster *m, int act, char *t, int cap) {
  if (act == 0) {   /* Hug */
    snprintf(t, (size_t)cap, "* You hug Moldbygg.\n* You're covered in slime...\n* You feel slower.");
    if (b->speed > FX) b->speed -= FX / 2;
    m->state = 1;
  } else if (act == 1) {   /* Lie Down */
    snprintf(t, (size_t)cap, "* You lie down next to Moldbygg.\n* It's surprisingly comfy.");
  } else {   /* Unhug */
    snprintf(t, (size_t)cap, m->state == 1 ? "* You let go of Moldbygg.\n* It seems relieved." : "* You give Moldbygg some room.\n* It seems relieved.");
    m->spare = true, m->state = 2;
  }
}
static const char *talk_moldbygg(Battle *b, Monster *m) {
  (void)b, (void)m;
  static const char *const lines[] = {"Burble...", "*squish*", "Blub\nblub.", "(wiggle)"};
  return lines[rnd_n(4)];
}
static int attack_moldbygg(Battle *b, Monster *m) { (void)b, (void)m; return rnd_n(2) ? ATK_MOLDBYGG_HOPS : ATK_MOLDBYGG_RAIN; }

/* ---------------------------------------------------------------- Shyren: hum, and she sings */
static void act_shyren(Battle *b, Monster *m, int act, char *t, int cap) {
  (void)b;
  if (act == 1) {   /* Hum */
    m->state++;
    m->spare = true;
    if (m->state >= 5) {
      snprintf(t, (size_t)cap, "* You hum along.\n* Shyren's song fills the whole cave.\n* She swims off, happy.");
      m->gone = true, m->fade = 1;
    } else if (m->state == 1) snprintf(t, (size_t)cap, "* You hum a little tune.\n* Shyren hums along, very quietly.");
    else snprintf(t, (size_t)cap, "* You hum again.\n* Shyren sings a little louder!");
  } else if (act == 0) {   /* Smile */
    snprintf(t, (size_t)cap, "* You smile at Shyren.\n* She gets even shyer.");
  } else {   /* Conduct */
    snprintf(t, (size_t)cap, m->state ? "* You conduct.\n* Shyren follows your hands!" : "* You wave your arms about.\n* Shyren doesn't know the tune yet.");
  }
}
static const char *talk_shyren(Battle *b, Monster *m) {
  (void)b;
  if (m->state >= 3) return "La la\nLAAA!";
  if (m->state) return "La...?";
  static const char *const lines[] = {"...hm...", "(blub)", "..."};
  return lines[rnd_n(3)];
}
static int attack_shyren(Battle *b, Monster *m) { (void)b, (void)m; return ATK_SHYREN_NOTES; }

static const MonDef wf_defs[MON_COUNT - MON_SNOW_COUNT] = {
  [MON_AARON - MON_SNOW_COUNT] = {
    .name = "Aaron", .spare_gold = 30, .hp = 98, .at = 7, .df = 2, .exp = 52, .gold = 30, .spr_idle = SPR_AARON_B,
    .spr_hurt = SPR_AARON_HURT, .check = "All muscle, and he knows it.", .acts = {"Flex", "Shoo"}, .nacts = 2,
    .neutral = {"* Aaron is flexing.", "* Aaron winks at nobody in particular.", "* Smells like protein powder."},
    .low_hp = "* Aaron's muscles are tired.", .act = act_aaron, .talk = talk_aaron, .attack = attack_aaron},
  [MON_WOSHUA - MON_SNOW_COUNT] = {
    .name = "Woshua", .spare_gold = 25, .hp = 70, .at = 7, .df = 1, .exp = 52, .gold = 25, .spr_idle = SPR_WOSHUA_B,
    .spr_hurt = SPR_WOSHUA_HURT, .idle_speed = 8, .check = "Can't stand a single speck of dirt.",
    .acts = {"Clean", "Touch", "Joke"}, .nacts = 3,
    .neutral = {"* Woshua is scrubbing the air.", "* Woshua hums a washing song.", "* Smells like soap."},
    .low_hp = "* Woshua's water is running low.", .act = act_woshua, .talk = talk_woshua, .attack = attack_woshua,
    .flavor = flavor_woshua},
  [MON_MOLDBYGG - MON_SNOW_COUNT] = {
    .name = "Moldbygg", .spare_gold = 20, .hp = 70, .at = 7, .df = 4, .exp = 46, .gold = 20, .spr_idle = SPR_MOLDBYGG_B,
    .spr_hurt = SPR_MOLDBYGG_B, .idle_speed = 8, .check = "Like Moldsmal, only a lot bigger.",
    .acts = {"Hug", "Lie Down", "Unhug"}, .nacts = 3,
    .neutral = {"* Moldbygg burbles.", "* Moldbygg wobbles.", "* Smells like a damp basement."},
    .act = act_moldbygg, .talk = talk_moldbygg, .attack = attack_moldbygg},
  [MON_SHYREN - MON_SNOW_COUNT] = {
    .name = "Shyren", .spare_gold = 20, .hp = 66, .at = 7, .df = 2, .exp = 52, .gold = 20, .spr_idle = SPR_SHYREN_B,
    .spr_hurt = SPR_SHYREN_TURNED, .check = "She'd love to sing, if nobody were watching.",
    .acts = {"Smile", "Hum", "Conduct"}, .nacts = 3, .neutral = {"* Shyren hides her face.", "* Shyren is humming to herself.", "* Shyren peeks at you."},
    .act = act_shyren, .talk = talk_shyren, .attack = attack_shyren},
};
const MonDef *wf_mon_def(int id) { return &wf_defs[id - MON_SNOW_COUNT]; }

/* Woshua's clean green caught: then it can be spared (battle.c asks each turn, through its flavour) */
static void woshua_check(Battle *b) {
  for (int i = 0; i < b->nmon; i++) {
    Monster *m = &b->mon[i];
    if (m->def == &wf_defs[MON_WOSHUA - MON_SNOW_COUNT] && m->state == 1 && b->greens > m->state2) m->state = 2, m->spare = true;
  }
}

static const Form wf_forms[FORM_COUNT - FORM_SNOW_COUNT] = {
  [FORM_AARON - FORM_SNOW_COUNT] = {{MON_AARON}, 1, "* Aaron flexes in."},
  [FORM_WOSHUA - FORM_SNOW_COUNT] = {{MON_WOSHUA}, 1, "* Woshua sloshes up."},
  [FORM_WOSHUA_AARON - FORM_SNOW_COUNT] = {{MON_WOSHUA, MON_AARON}, 2, "* Woshua and Aaron show up."},
  [FORM_MOLDBYGG - FORM_SNOW_COUNT] = {{MON_MOLDBYGG}, 1, "* Moldbygg blocks the way."},
  [FORM_MOLDBYGG_MOLDSMAL - FORM_SNOW_COUNT] = {{MON_MOLDSMAL, MON_MOLDBYGG}, 2, "* Moldbygg and Moldsmal block the way."},
  [FORM_MOLDBYGG_WOSHUA - FORM_SNOW_COUNT] = {{MON_MOLDBYGG, MON_WOSHUA}, 2, "* Moldbygg and Woshua appear."},
  [FORM_SHYREN - FORM_SNOW_COUNT] = {{MON_SHYREN}, 1, "* Shyren hums, then hides."},
};
const Form *wf_form(int form) { return &wf_forms[form - FORM_SNOW_COUNT]; }

/* random encounters: the wiki's pools, room by room */
int wf_encounter(int room) {
  switch (room) {
    case ROOM_WF_SEEDS2: case ROOM_WF_WISH:
      return rnd_n(3) == 0 ? FORM_WOSHUA_AARON : rnd_n(2) ? FORM_AARON : FORM_WOSHUA;
    case ROOM_WF_MARSH: case ROOM_WF_MARSH_MID: {
      static const uint8_t pool[] = {FORM_WOSHUA, FORM_WOSHUA_AARON, FORM_MOLDBYGG, FORM_MOLDBYGG_MOLDSMAL, FORM_AARON};
      return pool[rnd_n(5)];
    }
  }
  return -1;
}

/* ---------------------------------------------------------------- the attacks */
static int bx0(void) { return bt.x0 + 3; }
static int bx1(void) { return bt.x1 - 3; }
static int by0(void) { return bt.y0 + 3; }
static int by1(void) { return bt.y1 - 3; }
#define P(v) ((v) * FX)
static Bullet *shot(uint16_t spr, int x, int y, int vx, int vy) { return bullet_new(spr, P(x), P(y), vx, vy); }

/* Aaron: flexing arms rise up both sides, faster and faster; sweat drips down */
static void atk_aaron_arms(int t) {
  if (t < 140 && t % 16 == 0) {
    int k = t / 16;
    bool left = k % 2;
    int x = left ? bx0() + 10 + rnd_n(30) : bx1() - 10 - rnd_n(30);
    Bullet *b = shot(SPR_ARM, x, by1() + 8, 0, -(P(1) + P(k) / 6));
    if (b) b->r = 6, b->ay = -1;
  }
}
static void atk_aaron_sweat(int t) {
  if (t < 140 && t % 7 == 0) {
    Bullet *b = shot(SPR_DROP, bx0() + 4 + rnd_n(bx1() - bx0() - 8), by0() - 6, 0, P(1));
    if (b) b->kind = BK_GRAVITY, b->ay = 2, b->r = 3;
  }
}
/* Woshua: drops fall in columns, some green (they heal) once it's cleaning you; bubbles rise and sway */
static void atk_woshua_drops(int t) {
  bool clean = false;
  for (int i = 0; i < bt.nmon; i++)
    if (bt.mon[i].def == wf_mon_def(MON_WOSHUA) && bt.mon[i].state == 1 && bt.mon[i].alive && !bt.mon[i].gone) clean = true;
  if (t < 140 && t % 10 == 0) {
    /* (the green ones fall right over the SOUL: to be caught) */
    bool green = clean && (t / 10) % 4 == 1;
    int x = green ? clamp_i(soul_x(), bx0() + 6, bx1() - 6) : bx0() + 6 + rnd_n(bx1() - bx0() - 12);
    Bullet *b = shot(SPR_DROP, x, by0() - 6, 0, P(2));
    if (b) {
      b->r = 3;
      if (green) b->color = BC_GREEN, b->dmg = 1;
    }
  }
  if (t == 150) woshua_check(&bt);
}
static void atk_woshua_bubbles(int t) {
  if (t < 130 && t % 12 == 0) {
    Bullet *b = shot(SPR_ORB, bx0() + 8 + rnd_n(bx1() - bx0() - 16), by1() + 6, 0, -P(1));
    if (b) b->kind = BK_SCRIPT, b->a = (int16_t)(b->x / FX), b->r = 4;
  }
  for (int i = 0; i < MAX_BULLETS; i++) {
    Bullet *b = &bullets[i];
    if (b->kind != BK_SCRIPT || b->spr != SPR_ORB) continue;
    b->y -= P(1);
    b->x = P(b->a) + gfx_sin(b->t * 6) * 12 * FX / 1024;
    if (b->y < P(by0() - 10)) b->kind = BK_NONE;
  }
}
/* Moldbygg: molds hop across the floor; spores fall */
static void atk_moldbygg_hops(int t) {
  if (t < 120 && t % 30 == 0) {
    bool left = (t / 30) % 2;
    Bullet *b = shot(SPR_ORB, left ? bx0() : bx1(), by1() - 6, (left ? 1 : -1) * P(2), -P(4));
    if (b) b->kind = BK_GRAVITY, b->ay = 6, b->r = 5, b->a = 3;
  }
  for (int i = 0; i < MAX_BULLETS; i++) {
    Bullet *b = &bullets[i];
    if (b->kind != BK_GRAVITY || b->spr != SPR_ORB || b->a <= 0) continue;
    if (b->y > P(by1() - 5) && b->vy > 0) b->vy = -P(4), b->a--;   /* (it bounces a few times) */
  }
}
static void atk_moldbygg_rain(int t) {
  if (t < 140 && t % 5 == 0) {
    Bullet *b = shot(SPR_ORB, bx0() + 4 + rnd_n(bx1() - bx0() - 8), by0() - 6, rnd_n(P(1)) - P(1) / 2, P(1));
    if (b) b->r = 3;
  }
}
/* Shyren: notes swim in from the right on waves, more of them for each hum */
static void atk_shyren_notes(int t) {
  int hums = 0;
  for (int i = 0; i < bt.nmon; i++)
    if (bt.mon[i].def == wf_mon_def(MON_SHYREN)) hums = bt.mon[i].state;
  int every = 16 - hums * 2;
  if (t < 140 && t % every == 0) {
    Bullet *b = shot(SPR_NOTE, bx1() + 6, by0() + 8 + rnd_n(by1() - by0() - 16), -(P(2) + P(hums) / 2), 0);
    if (b) b->kind = BK_SCRIPT, b->b = (int16_t)(b->y / FX), b->r = 4;
  }
  for (int i = 0; i < MAX_BULLETS; i++) {
    Bullet *b = &bullets[i];
    if (b->kind != BK_SCRIPT || b->spr != SPR_NOTE) continue;
    b->x += b->vx;
    b->y = P(b->b) + gfx_sin(b->t * 8) * 10 * FX / 1024;
    if (b->x < P(bx0() - 10)) b->kind = BK_NONE;
  }
}

int wf_attack_length(int atk) { (void)atk; return 160; }

void wf_attack_run(Battle *b, int atk, int t) {
  (void)b;
  if (t < 0) {
    board_set(atk == ATK_SHYREN_NOTES ? 160 : 130, 110);
    return;
  }
  switch (atk) {
    case ATK_AARON_ARMS: atk_aaron_arms(t); break;
    case ATK_AARON_SWEAT: atk_aaron_sweat(t); break;
    case ATK_WOSHUA_DROPS: atk_woshua_drops(t); break;
    case ATK_WOSHUA_BUBBLES: atk_woshua_bubbles(t); break;
    case ATK_MOLDBYGG_HOPS: atk_moldbygg_hops(t); break;
    case ATK_MOLDBYGG_RAIN: atk_moldbygg_rain(t); break;
    case ATK_SHYREN_NOTES: atk_shyren_notes(t); break;
  }
}
