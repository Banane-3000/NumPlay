/* Hotland's monsters: stats and ACTs (from the Undertale Wiki's records of the game), their formations, where they
 * come out and their attacks. Their words are short ones of our own. */
#include <stdio.h>
#include "battle.h"
#include "game.h"
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

static const MonDef ht_defs[MON_COUNT - MON_WF_COUNT] = {
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
};
const MonDef *ht_mon_def(int id) { return &ht_defs[id - MON_WF_COUNT]; }

static const Form ht_forms[FORM_COUNT - FORM_WF_COUNT] = {
  [FORM_VULKIN - FORM_WF_COUNT] = {{MON_VULKIN}, 1, "* Vulkin strolls in, happy as can be."},
  [FORM_TSUNDERPLANE - FORM_WF_COUNT] = {{MON_TSUNDERPLANE}, 1, "* Tsunderplane gets in the way!\n* Not on purpose or anything."},
  [FORM_TSUNDER_VULKIN - FORM_WF_COUNT] = {{MON_VULKIN, MON_TSUNDERPLANE}, 2,
                                           "* Tsunderplane and Vulkin show up.\n* (Not that Tsunderplane cares.)"},
  [FORM_PYROPE - FORM_WF_COUNT] = {{MON_PYROPE}, 1, "* Pyrope bounces in, burning bright!"},
  [FORM_PYROPE2 - FORM_WF_COUNT] = {{MON_PYROPE, MON_PYROPE}, 2, "* Two Pyropes hop in.\n* They're up to something."},
  [FORM_MTT_QUIZ - FORM_WF_COUNT] = {{MON_METTATON}, 1, ""},
  [FORM_MTT_COOK - FORM_WF_COUNT] = {{MON_METTATON}, 1, ""},
  [FORM_LASERS - FORM_WF_COUNT] = {{MON_METTATON}, 1, ""},
};
const Form *ht_form(int form) { return &ht_forms[form - FORM_WF_COUNT]; }

/* random encounters: past the Lab (the belts and the Magma Chamber) */
int ht_encounter(int room) {
  switch (room) {
    case ROOM_HT_BELT1: case ROOM_HT_BELT2: case ROOM_HT_MAGMA: {
      static const uint8_t pool[] = {FORM_VULKIN, FORM_TSUNDERPLANE, FORM_TSUNDER_VULKIN, FORM_PYROPE, FORM_PYROPE2};
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

int ht_attack_length(int atk) { (void)atk; return 160; }

void ht_attack_run(Battle *b, int atk, int t) {
  (void)b;
  if (t < 0) {
    board_set(atk == ATK_TSUNDER_BOMBS || atk == ATK_PYROPE_ROPE ? 150 : 120, 100);
    soul_set_mode(SOUL_RED);
    return;
  }
  switch (atk) {
    case ATK_VULKIN_THUNDER: atk_vulkin_thunder(t); break;
    case ATK_VULKIN_FIRE: atk_vulkin_fire(t); break;
    case ATK_TSUNDER_BOMBS: atk_tsunder_bombs(t); break;
    case ATK_TSUNDER_DIVE: atk_tsunder_dive(t); break;
    case ATK_PYROPE_BOMBS: atk_pyrope_bombs(t); break;
    case ATK_PYROPE_ROPE: atk_pyrope_rope(t); break;
  }
}
