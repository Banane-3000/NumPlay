/* New Home's battles: Asgore at the barrier (the Undertale Wiki's records of the game: his HP, his ACT, how his
 * guard drops, his fire and his trident; he breaks MERCY before it starts), and Omega Flowey with the six SOULs he
 * took: not turn by turn, his attacks one after another; now and then a SOUL fights for him, until Frisk reaches the
 * ACT that appears among its attack and calls for help; all six helped, they turn on him, and FIGHT can reach him.
 * Their words are short ones of our own. */
#include <stdio.h>
#include "battle.h"
#include "game.h"
#include "gfx.h"
#include "items.h"
#include "monsters.h"
#include "res.h"
#include "world.h"

/* ---------------------------------------------------------------- Asgore */
enum { AS_DOWN, AS_TALKS };   /* (b->state[]) */
static void act_asgore(Battle *b, Monster *m, int act, char *t, int cap) {
  (void)act;
  int n = ++b->state[AS_TALKS];
  if (n < 3 || sv.exp > 0) snprintf(t, (size_t)cap, "* You tell Asgore you don't want\n  to fight him.\n* He doesn't answer.");
  else if (n == 3) {
    snprintf(t, (size_t)cap, "* You tell Asgore you don't want\n  to fight. His hands tremble.\n* His ATTACK and DEFENSE drop.");
    m->at = (int16_t)max_i(1, m->at - 1), m->df = (int16_t)(m->df - 10);
  } else snprintf(t, (size_t)cap, "* Asgore's hands are shaking.");
}
static bool item_asgore(Battle *b, Monster *m, int item, char *t, int cap) {
  (void)b, (void)t, (void)cap;
  /* her pie: its smell reminds him of someone */
  if (item == ITEM_BUTTERSCOTCH_PIE) m->at = (int16_t)max_i(1, m->at - 1), m->df = (int16_t)(m->df - 5);
  return false;
}
static const char *talk_asgore(Battle *b, Monster *m) {
  (void)m;
  if (b->state[AS_DOWN]) return "Ah...";
  return NULL;   /* (he says nothing: he looks away) */
}
static const char *flavor_asgore(Battle *b, Monster *m) {
  /* after turn 20 his guard falls, turn by turn */
  if (b->turn >= 20 && m->df > -120) m->df = (int16_t)(m->df - 10);
  if (b->turn <= 1) return "* ASGORE attacks!";
  if (b->state[AS_TALKS] == 3 && sv.exp == 0) return "* Asgore's hands are shaking.";
  static const char *const lines[] = {"* Asgore looks away.", "* Asgore's trident glints.", "* You feel the heat of his fire.",
                                      "* The barrier hums behind him.", "* Seems like no one can save you now."};
  return lines[rnd_n(5)];
}
static bool hit_asgore(Battle *b, Monster *m, int dmg) {
  if (m->hp - dmg > 0) return false;
  /* down on his knees: what's next is Frisk's choice, not the battle's */
  m->hp = 1, m->hurt = 30, m->shake = 8;
  b->state[AS_DOWN] = 1;
  return true;
}
static int attack_asgore(Battle *b, Monster *m) {
  (void)m;
  if (b->state[AS_DOWN]) return -1;
  static const uint8_t seq[6] = {ATK_ASG_HELIX, ATK_ASG_TRIDENT, ATK_ASG_RINGS, ATK_ASG_RAIN, ATK_ASG_HANDS, ATK_ASG_TRIDENT};
  return seq[(b->turn - 1) % 6];
}
static bool ends_asgore(Battle *b, Monster *m) {
  (void)m;
  if (b->state[AS_DOWN]) b->result = BR_SCRIPTED;
  return false;
}
static void draw_asgore(Battle *b, Monster *m, int x, int y) {
  uint16_t s = b->state[AS_DOWN] ? SPR_ASGORE_KNEEL : m->hurt ? SPR_ASGORE_HURT : SPR_ASGORE_B;
  int fr = m->hurt || b->state[AS_DOWN] ? 0 : (int)(g_frame / 8);
  gfx_sprite(s, fr, x, y + 4, 0, 255);
}

static const MonDef nh_defs[MON_COUNT - MON_CORE_COUNT] = {
  [MON_ASGORE - MON_CORE_COUNT] = {
    .name = "ASGORE", .hp = 3500, .at = 8, .df = -30, .spr_idle = SPR_ASGORE_B, .spr_hurt = SPR_ASGORE_HURT,
    .idle_speed = 8, .check = "The king of the monsters.", .acts = {"Talk"}, .nacts = 1, .flags = MF_BOSS,
    .act = act_asgore, .talk = talk_asgore, .attack = attack_asgore, .flavor = flavor_asgore, .on_hit = hit_asgore,
    .ends = ends_asgore, .draw = draw_asgore, .on_item = item_asgore},
  [MON_OMEGA - MON_CORE_COUNT] = {.name = "Flowey", .hp = 9999, .flags = MF_BOSS | MF_NOT_KILLABLE},
};
const MonDef *nh_mon_def(int id) { return &nh_defs[id - MON_CORE_COUNT]; }
static const Form nh_forms[FORM_COUNT - FORM_CORE_COUNT] = {
  [FORM_ASGORE - FORM_CORE_COUNT] = {{MON_ASGORE}, 1, "* (Twilight shines through the\n  barrier.)\n* (You're filled with DETERMINATION.)"},
  [FORM_OMEGA - FORM_CORE_COUNT] = {{MON_OMEGA}, 1, ""},
};
const Form *nh_form(int form) { return &nh_forms[form - FORM_CORE_COUNT]; }

/* ---------------------------------------------------------------- Asgore's attacks */
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
static Bullet *fire(int x, int y, int vx, int vy) {
  Bullet *b = shot(SPR_FIREBULLET, x, y, vx, vy);
  if (b) b->r = 3, b->flags |= BF_NOCLIP;
  return b;
}
/* four waves of fire, curling down in a double helix */
static void atk_helix(int t) {
  if (t % 4 || t > 140) return;
  int cx = board_cx(), a = gfx_sin(t * 6) * ((bx1() - bx0()) / 2 - 6) / 1024;
  fire(cx + a, by0() - 6, 0, P(2));
  fire(cx - a, by0() - 6, 0, P(2));
}
/* a stream down the middle, and a cloud of fire over one side, then the other */
static void atk_rain(int t) {
  if (t % 5 == 0 && t < 150) fire(board_cx() - 6 + rnd_n(13), by0() - 6, 0, P(2));
  if (t % 3 == 0 && t < 150) {
    bool left = (t / 50) % 2;
    int w = (bx1() - bx0()) / 2 - 16;
    fire(left ? bx0() + rnd_n(w) : bx1() - rnd_n(w), by0() - 6, 0, P(3));
  }
}
/* rings of fire, a gap in each, closing in round the SOUL */
static void atk_rings(int t) {
  if (t % 45 || t > 120) return;
  int gap = rnd_n(12);
  for (int k = 0; k < 12; k++) {
    if (k == gap || k == (gap + 1) % 12) continue;
    Bullet *b = fire(board_cx(), board_cy(), 0, 0);
    if (!b) continue;
    b->kind = BK_ORBIT, b->a = (int16_t)board_cx(), b->b = (int16_t)board_cy(), b->c = (int16_t)P(80);
    b->d = (int16_t)(k * 256 / 12 * 16), b->vx = 12 + t / 10, b->vy = -P(1) / 2, b->life = 150;
  }
}
/* his hands sweep down the sides, leaving fire; then it all flies at where the SOUL was */
static int aim_x, aim_y;
static void atk_hands(int t) {
  if (t < 80 && t % 8 == 0) {
    for (int side = 0; side < 2; side++) {
      Bullet *b = fire(side ? bx1() - 4 : bx0() + 4, by0() + 4 + t * (by1() - by0() - 8) / 80, 0, 0);
      if (b) b->kind = BK_SCRIPT, b->a = 1;
    }
  }
  if (t == 85) aim_x = soul_x(), aim_y = soul_y();
  if (t >= 85)
    for (int i = 0; i < MAX_BULLETS; i++) {
      Bullet *b = &bullets[i];
      if (b->kind != BK_SCRIPT || b->a != 1) continue;
      int dx = aim_x - b->x / FX, dy = aim_y - b->y / FX, d = abs_i(dx) + abs_i(dy);
      b->kind = BK_LINEAR;
      if (d) b->vx = dx * P(3) / d, b->vy = dy * P(3) / d;
    }
}
/* his eyes flash blue or orange, then his trident sweeps the whole board in that colour */
static uint8_t trident[3];
static void atk_trident(int t) {
  if (t == 0)
    for (int k = 0; k < 3; k++) trident[k] = (uint8_t)(rnd_n(2) ? BC_BLUE : BC_ORANGE);
  int k = (t - 20) / 45, ph = (t - 20) % 45;
  if (t < 20 || k >= 3) return;
  if (ph == 0) {   /* the warning: a flash of the colour along the top */
    Bullet *w = shot(SPR_RECT, board_cx(), by0() + 2, 0, 0);
    if (w) w->flags |= BF_BOX | BF_HARMLESS, w->w = (int16_t)(bx1() - bx0()), w->h = 3, w->color = trident[k], w->life = 18;
  }
  if (ph == 20) {
    bool left = k % 2;
    Bullet *s = shot(SPR_RECT, left ? bx0() - 4 : bx1() + 4, board_cy(), left ? P(9) : -P(9), 0);
    if (s) s->flags |= BF_BOX | BF_NOCLIP, s->w = 8, s->h = (int16_t)(by1() - by0()), s->color = trident[k], s->life = 30;
  }
}

int nh_attack_length(int atk) { return atk == ATK_ASG_TRIDENT ? 160 : 165; }
void nh_attack_run(Battle *b, int atk, int t) {
  (void)b;
  if (t < 0) {
    board_set(atk == ATK_ASG_RINGS ? 110 : 150, 100);
    soul_set_mode(SOUL_RED);
    return;
  }
  switch (atk) {
    case ATK_ASG_HELIX: atk_helix(t); break;
    case ATK_ASG_RAIN: atk_rain(t); break;
    case ATK_ASG_RINGS: atk_rings(t); break;
    case ATK_ASG_HANDS: atk_hands(t); break;
    case ATK_ASG_TRIDENT: atk_trident(t); break;
  }
}

/* ---------------------------------------------------------------- Omega Flowey */
enum { OF_ATTACK, OF_WARNING, OF_SOUL, OF_REBEL, OF_DONE };
enum { OA_EYES, OA_FLAME, OA_PELLETS, OA_VINES, OA_BOMBS, OA_FINGERS, OA_COUNT };
static struct {
  int st, t, atk, n, souls, helped, hits, msg_t, refused;
  const char *msg;
  Bullet *act;
} om;
static const uint16_t soul_spr[6] = {SPR_SOUL_LIGHT_BLUE, SPR_SOUL_ORANGE, SPR_SOUL_BLUE, SPR_SOUL_PURPLE, SPR_SOUL_GREEN,
                                     SPR_SOUL_YELLOW};
static const uint32_t soul_rgb[6] = {0x42FCFF, 0xFCA600, 0x003CFF, 0xD535D9, 0x00C000, 0xFFFF00};
#define OM_HITS 6
static void say_top(const char *s) { om.msg = s, om.msg_t = 110; }

void omega_start(void) {
  bt.scripted = true;
  board_set_rect(65, 120, 255, 194);
  bt.x0 = bt.tx0, bt.y0 = bt.ty0, bt.x1 = bt.tx1, bt.y1 = bt.ty1;
  bt.sx = P(board_cx()), bt.sy = P(board_cy());
  soul_set_mode(SOUL_RED);
  om.st = OF_ATTACK, om.t = 0, om.atk = 0, om.n = 0, om.souls = 0, om.helped = 0, om.hits = 0, om.refused = 0, om.act = NULL;
  say_top("* Flowey holds the six SOULs.");
}

/* his own: the eyes' X shots, the flamethrowers, a ring of pellets closing in, vines along flashing lines, bombs,
 * finger guns from both sides */
static void omega_attack(int a, int t) {
  int cx = board_cx();
  switch (a) {
    case OA_EYES:
      if (t % 18 == 0 && t < 140)
        for (int k = -3; k <= 3; k++) {
          Bullet *b = shot(SPR_XSHOT, cx + (t / 18 % 2 ? 40 : -40), by0() - 10, k * P(1) / 2, P(2));
          if (b) b->r = 3, b->flags |= BF_NOCLIP, b->dmg = 6;
        }
      break;
    case OA_FLAME:
      if (t % 30 == 10 && t < 140)
        for (int side = 0; side < 2; side++) {
          int y = by0() + 10 + rnd_n(by1() - by0() - 20);
          Bullet *b = shot(SPR_RECT, side ? bx1() : bx0(), y, side ? -P(3) : P(3), 0);
          if (b) b->flags |= BF_BOX | BF_NOCLIP, b->w = 40, b->h = 8, b->color = BC_WHITE, b->life = 40, b->dmg = 6;
        }
      break;
    case OA_PELLETS:
      if (t % 60 == 0 && t < 130)
        for (int k = 0; k < 10; k++) {
          int an = k * 256 / 10;
          Bullet *b = shot(SPR_SPINBULLET, soul_x() + gfx_cos(an) * 40 / 1024, soul_y() + gfx_sin(an) * 40 / 1024, 0, 0);
          if (b) b->kind = BK_SCRIPT, b->a = 30, b->r = 3, b->anim = 3, b->dmg = 6, b->flags |= BF_NOCLIP;
        }
      for (int i = 0; i < MAX_BULLETS; i++) {
        Bullet *b = &bullets[i];
        if (b->kind != BK_SCRIPT || b->spr != SPR_SPINBULLET) continue;
        if (b->a) {   /* (still, a moment: the warning) */
          b->a--;
          b->alpha = b->a % 4 < 2 ? 120 : 255, b->flags |= BF_HARMLESS;
          if (!b->a) {
            int dx = soul_x() - b->x / FX, dy = soul_y() - b->y / FX, d = abs_i(dx) + abs_i(dy);
            b->kind = BK_LINEAR, b->alpha = 255, b->flags &= (uint8_t)~BF_HARMLESS;
            if (d) b->vx = dx * P(2) / d, b->vy = dy * P(2) / d;
          }
        }
      }
      break;
    case OA_VINES:
      if (t % 40 == 0 && t < 130)
        for (int k = 0; k < 3; k++) {
          int x = soul_x() + (k - 1) * 22;
          Bullet *w = shot(SPR_RECT, x, board_cy(), 0, 0);
          if (w) w->flags |= BF_BOX | BF_HARMLESS, w->w = 1, w->h = (int16_t)(by1() - by0()), w->color = BC_ORANGE, w->life = 20;
          Bullet *v = shot(SPR_RECT, x, by0() - 60, 0, 0);
          if (v) v->kind = BK_SCRIPT, v->a = 20, v->flags |= BF_BOX | BF_NOCLIP, v->w = 5, v->h = 100, v->dmg = 6;
        }
      for (int i = 0; i < MAX_BULLETS; i++) {
        Bullet *v = &bullets[i];
        if (v->kind != BK_SCRIPT || v->spr != SPR_RECT) continue;
        if (v->a) v->a--;
        else v->y += P(8);
        if (v->y > P(by1() + 70)) v->kind = BK_NONE;
      }
      break;
    case OA_BOMBS:
      if (t % 20 == 0 && t < 130) {
        Bullet *b = shot(SPR_BOMB, bx0() + 8 + rnd_n(bx1() - bx0() - 16), by0() - 8, 0, P(2));
        if (b) b->kind = BK_SCRIPT, b->r = 5, b->dmg = 6, b->flags |= BF_NOCLIP;
      }
      for (int i = 0; i < MAX_BULLETS; i++) {
        Bullet *b = &bullets[i];
        if (b->kind != BK_SCRIPT || b->spr != SPR_BOMB) continue;
        b->y += b->vy;
        if (b->y >= P(by1() - 4)) {   /* it hits the floor: a burst */
          int x = b->x / FX, y = b->y / FX;
          b->kind = BK_NONE;
          for (int k = 0; k < 8; k++) {
            Bullet *s = shot(SPR_SPINBULLET, x, y, gfx_cos(k * 32) * 2 * FX / 1024, gfx_sin(k * 32) * 2 * FX / 1024);
            if (s) s->r = 3, s->dmg = 6, s->life = 50;
          }
        }
      }
      break;
    case OA_FINGERS:
      if (t % 16 == 0 && t < 140) {
        bool left = (t / 16) % 2;
        Bullet *b = shot(SPR_ORB, left ? bx0() - 6 : bx1() + 6, soul_y(), left ? P(3) : -P(3), 0);
        if (b) b->r = 4, b->dmg = 6, b->flags |= BF_NOCLIP;
      }
      break;
  }
}

/* a SOUL's: the light blue's knives, the orange's fists, the blue's stars and shoes, the purple's words, the
 * green's pans and their fire, the yellow's gun; once called on, what they throw heals instead */
static void soul_attack(int s, int t) {
  int cx = board_cx(), cy = board_cy();
  uint8_t col = om.helped ? BC_GREEN : BC_WHITE;
  Bullet *b = NULL;
  switch (s) {
    case 0:
      if (t % 12 == 0) b = shot(SPR_KNIFE, bx0() - 4, by0() + rnd_n(by1() - by0()), P(3), (t / 60 % 2 ? 1 : -1) * P(1));
      break;
    case 1:
      if (t % 50 == 0)
        for (int k = 0; k < 8; k++) {
          b = shot(SPR_DOGPAW, cx, cy, 0, 0);
          if (b) b->kind = BK_ORBIT, b->a = (int16_t)cx, b->b = (int16_t)cy, b->c = (int16_t)P(70), b->d = (int16_t)(k * 32 * 16),
                 b->vx = 30, b->vy = -P(1) / 2, b->life = 130, b->r = 4, b->color = col, b->dmg = om.helped ? 2 : 6;
          b = NULL;
        }
      break;
    case 2:
      if (t % 14 == 0) {
        b = shot(SPR_SHOE, bx1() + 6, cy + gfx_sin(t * 8) * 30 / 1024, -P(2), 0);
        if (b) b->color = col, b->r = 4, b->dmg = om.helped ? 2 : 6;
        b = shot(SPR_STAR, bx0() + rnd_n(bx1() - bx0()), by0() - 4, 0, P(1));
      }
      break;
    case 3:
      if (t % 10 == 0) {
        bool left = (t / 10) % 2;
        b = shot(SPR_RECT, left ? bx0() : bx1(), by0() + 8 + rnd_n(by1() - by0() - 16), left ? P(2) : -P(2), 0);
        if (b) b->flags |= BF_BOX, b->w = 22, b->h = 6;
      }
      break;
    case 4:
      if (t % 9 == 0) b = shot(SPR_FIREBULLET, cx + (rnd_n(3) - 1) * 40, cy - 10, (rnd_n(3) - 1) * P(1) / 2, -P(3));
      if (b) b->ay = P(1) / 8;
      break;
    case 5:
      if (t % 15 == 0) {
        int dx = soul_x() - cx, dy = soul_y() - by0(), d = abs_i(dx) + abs_i(dy);
        b = shot(SPR_ORB, cx, by0() + 4, d ? dx * P(3) / d : 0, d ? dy * P(3) / d : P(3));
      }
      break;
  }
  if (b) b->color = col, b->r = b->r ? b->r : 4, b->dmg = om.helped ? 2 : 6, b->flags |= BF_NOCLIP;
}
static void clear_bullets(void) {
  for (int i = 0; i < MAX_BULLETS; i++) bullets[i].kind = BK_NONE;
}
/* a button among the bullets (ACT, then FIGHT): it drifts to the SOUL; touched, it's pressed */
static Bullet *button_new(uint16_t spr) {
  Bullet *b = bullet_new(spr, P(board_cx()), P(bt.y0 + 6), 0, 0);
  if (b) b->kind = BK_SCRIPT, b->flags |= BF_HARMLESS | BF_NOCLIP, b->a = 7;
  return b;
}
static bool is_button(const Bullet *b) {
  return b && b->kind == BK_SCRIPT && b->a == 7 && (b->spr == SPR_ACTBUTTON || b->spr == SPR_FIGHTBT);
}
static bool button_step(Bullet *b) {
  if (!is_button(b)) return false;
  int dx = soul_x() - b->x / FX, dy = soul_y() - b->y / FX;
  b->x += dx > 0 ? FX * 3 / 4 : dx < 0 ? -FX * 3 / 4 : 0, b->y += dy > 0 ? FX * 3 / 4 : dy < 0 ? -FX * 3 / 4 : 0;
  if (abs_i(dx) < 9 && abs_i(dy) < 7) {
    b->kind = BK_NONE;
    return true;
  }
  return false;
}

void omega_update(uint32_t keys, uint32_t pressed) {
  (void)pressed;
  if (bt.inv) bt.inv--;
  soul_update(keys);
  bullets_update();
  soul_hit_test();
  if (om.msg_t) om.msg_t--;
  /* at 0 HP: but it refused (the SOULs won't let Frisk go) */
  if (sv.hp < 1) {
    sv.hp = sv.maxhp, om.refused++;
    say_top("* Your SOUL won't give up.");
  }
  om.t++;
  switch (om.st) {
    case OF_ATTACK:
      omega_attack(om.atk, om.t);
      if (om.t >= 160) {
        om.t = 0, om.n++;
        om.atk = (om.atk + 1 + rnd_n(OA_COUNT - 1)) % OA_COUNT;
        if (om.souls < 6 && om.n % 2 == 0) om.st = OF_WARNING, clear_bullets(), say_top("* WARNING!");
      }
      break;
    case OF_WARNING:
      if (om.t >= 45) om.st = OF_SOUL, om.t = 0, om.helped = 0, om.act = NULL;
      break;
    case OF_SOUL:
      soul_attack(om.souls, om.t);
      if (om.t == 70) om.act = button_new(SPR_ACTBUTTON);
      if (!om.helped && button_step(om.act)) {
        om.helped = 1, om.t = 200;
        say_top("* You called for help...");
        for (int i = 0; i < MAX_BULLETS; i++)
          if (bullets[i].kind != BK_NONE) bullets[i].color = BC_GREEN, bullets[i].dmg = 2;
      }
      if (om.t >= 300) {
        clear_bullets();
        om.t = 0;
        if (om.helped) {
          om.souls++;
          say_top(om.souls < 6 ? "* A SOUL is on your side." : "* The SOULs turn on him! DEFENSE 0!");
        }
        om.st = om.souls >= 6 ? OF_REBEL : OF_ATTACK;
      }
      break;
    case OF_REBEL:
      /* his attacks, slower now; FIGHT comes down among them */
      if (om.t % 2 == 0) omega_attack(om.atk, om.t / 2);
      if (om.t >= 320) om.t = 0, om.atk = (om.atk + 1) % OA_COUNT;
      if (!is_button(om.act)) {
        if (om.t % 120 == 60) om.act = button_new(SPR_FIGHTBT);
      } else if (button_step(om.act)) {
        om.hits++, om.act = NULL;
        g_shake = 8;
        say_top(om.hits < OM_HITS ? "* You hit Flowey!" : "* Flowey falls apart.");
        if (om.hits >= OM_HITS) om.st = OF_DONE, om.t = 0, clear_bullets();
      }
      break;
    case OF_DONE:
      if (om.t >= 90) bt.result = BR_SCRIPTED;
      break;
  }
}

void omega_draw(void) {
  gfx_rect(0, 0, SCREEN_W, SCREEN_H, C_BLACK, 255);
  /* his screen: his face, laughing while he wins; the SOUL he's using, beside it */
  bool hurt = om.st == OF_REBEL || om.st == OF_DONE;
  if (om.st != OF_DONE || om.t % 6 < 3) {
    if (hurt && g_frame % 30 < 15) gfx_sprite(SPR_OMEGA, 0, 160 + (g_frame % 4 < 2 ? 2 : -2), 56, 0, 255);
    else if (om.st == OF_ATTACK) gfx_sprite(SPR_OMEGA_LAUGH, (int)(g_frame / 6), 160, 56, 0, 255);
    else gfx_sprite(SPR_OMEGA, 0, 160, 56, 0, 255);
  }
  for (int k = 0; k < 6; k++) {   /* the six SOULs: dim once they've sided with Frisk */
    int x = k < 3 ? 46 + k * 16 : 242 + (k - 3) * 16;
    gfx_sprite(soul_spr[k], 0, x, 20, 0, k < om.souls ? 90 : 255);
  }
  if (om.st == OF_WARNING && om.t % 10 < 6) {
    gfx_rect(120, 92, 80, 14, rgb565(soul_rgb[om.souls]), 255);
    gfx_text(FONT_SMALL, 134, 94, "WARNING", C_BLACK, 255);
  }
  /* the board */
  gfx_rect(bt.x0, bt.y0, bt.x1 - bt.x0, bt.y1 - bt.y0, C_WHITE, 255);
  gfx_rect(bt.x0 + 2, bt.y0 + 2, bt.x1 - bt.x0 - 4, bt.y1 - bt.y0 - 4, C_BLACK, 255);
  gfx_clip(bt.x0 + 2, bt.y0 + 2, bt.x1 - bt.x0 - 4, bt.y1 - bt.y0 - 4);
  bullets_draw();
  gfx_clip(0, 0, SCREEN_W, SCREEN_H);
  if (!bt.inv || (bt.inv / 2) % 2) gfx_sprite(SPR_HEART, 0, soul_x(), soul_y(), 0, 255);
  if (om.msg_t && om.msg) gfx_text(FONT_SMALL, 70, 107, om.msg, C_WHITE, 255);
  battle_draw_hud();
}
