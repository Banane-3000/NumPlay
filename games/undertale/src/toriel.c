/* Toriel, the Ruins' boss: her fire, her spares, her words (from the Undertale Wiki's records of the fight). */
#include <stdio.h>
#include <string.h>
#include "battle.h"
#include "game.h"
#include "gfx.h"
#include "monsters.h"
#include "res.h"

/* b->state[0]: spares; [1]: talks; [2]: 1 she stopped fighting, 2 she gives up, 3 defeated, 4 betrayed */
static const char *const spare_lines[] = {
  ".....", "..... .....", "..... ..... .....", "...?", "What are\nyou doing?", "Attack or\nrun away!",
  "What are you\nproving this\nway?", "Fight me or\nleave!", "Stop it.", "Stop looking\nat me that\nway.", "Go away!", "...",
  "... ...", "I know you\nwant to go\nhome, but...", "But please...\ngo upstairs\nnow.", "I promise I\nwill take\ngood care of\nyou here.",
  "I know we do\nnot have\nmuch, but...", "We can have\na good life\nhere.", "Why are you\nmaking this\nso difficult?",
  "Please, go\nupstairs.", ".....", "Ha ha...", "Pathetic, is\nit not? I\ncannot save\neven a single\nchild.", "...",
  "No, I\nunderstand.|You would\njust be\nunhappy\ntrapped down\nhere.|The RUINS\nare very small\nonce you get\nused to them.|It would not\nbe right for\nyou to grow up\nin a place\nlike this.|My\nexpectations...\nMy\nloneliness...\nMy fear...|For you, my\nchild... I\nwill put them\naside."};
#define NSPARES (int)(sizeof spare_lines / sizeof spare_lines[0])

static void act_toriel(Battle *b, Monster *m, int act, char *t, int cap) {
  int n = b->state[1]++;
  if (n == 0) snprintf(t, (size_t)cap, "* You couldn't think of any\n  conversation topics.");
  else if (n == 1) snprintf(t, (size_t)cap, "* You tried to think of\n  something to say again,\n  but...");
  else snprintf(t, (size_t)cap, "* Ironically, talking does not\n  seem to be the solution to\n  this situation.");
  (void)m;
}

static const char *talk_toriel(Battle *b, Monster *m) {
  if (b->state[2] == 3)
    return "Urgh...|You are\nstronger than\nI thought...|Listen to me,\nsmall one...|If you go\nbeyond this\ndoor,|Keep walking\nas far as you\ncan.|Eventually\nyou will reach\nan exit.|... ....|#RASGORE#X...\nDo not let\n#RASGORE#X\ntake your\nsoul.|His plan\ncannot be\nallowed to\nsucceed.|......|Be good,\nwon't you?|M y   c h i l d .";
  if (b->state[2] == 4)
    return "You...|... at my\nmost\nvulnerable\nmoment...|To think I\nwas worried\nyou wouldn't\nfit in out\nthere...|Eheheheh!!!\nYou really are\nno different\nthan them!|Ha... ha...";
  if (b->last_action == 3) {
    int k = b->state[0] - 1;
    if (k >= 0 && k < NSPARES) return spare_lines[k];
  }
  return NULL;
}

static void spare_toriel(Battle *b, Monster *m) {
  (void)m;
  b->state[0]++;
  if (b->state[0] >= 12) b->no_flee = true;
}

static const char *flavor_toriel(Battle *b, Monster *m) {
  /* her DF falls when the SOUL is weak, and when she is */
  m->df = (int16_t)(sv.hp < 3 ? -15 : m->hp < 150 ? -140 : 1);
  if (b->state[0] >= 12) return "* ...";
  return NULL;
}

/* she uses one of her attacks; her fire falters when the SOUL is weak, and stops after enough spares */
static int attack_toriel(Battle *b, Monster *m) {
  if (b->state[2]) return -1;
  if (b->state[0] >= 12) return -1;
  if (sv.hp <= 2 || b->state[0] >= 11) return ATK_TOR_FALTER;
  int k = b->turn % 3;
  return k == 0 ? ATK_TOR_HANDS : k == 1 ? ATK_TOR_WAVES : ATK_TOR_HELIX;
}

static bool hit_toriel(Battle *b, Monster *m, int dmg) {
  /* hit once she stopped fighting: at once, betrayed */
  if (b->state[0] >= 12) {
    m->hp = 0, m->hurt = 30;
    b->state[2] = 4;
    return true;
  }
  if (dmg >= m->hp) {
    m->hp = 0, m->hurt = 30;
    b->state[2] = 3;
    return true;
  }
  return false;
}

static bool ends_toriel(Battle *b, Monster *m) {
  if (b->state[2] == 3 || b->state[2] == 4) {
    m->killed = true, m->alive = false, m->dust = 1;
    set_flag(F_TORIEL_FIGHT, 2);
    sv.kills++;
    return true;
  }
  if (b->state[0] >= 25) {
    set_flag(F_TORIEL_FIGHT, 1);
    m->spare = true;
    return true;
  }
  return false;
}

static void draw_toriel(Battle *b, Monster *m, int x, int y) {
  uint16_t s = SPR_TORIEL_B;
  if (m->hurt) s = SPR_TORIEL_HURT;
  else if (b->state[2] == 3 || b->state[2] == 4 || m->hp <= 0) s = SPR_TORIEL_DEAD;
  else if (b->atk == ATK_TOR_FALTER && b->phase >= 10) s = SPR_TORIEL_AWAY;
  if (m->dust) {
    int a = 255 - m->dust * 4;
    if (a > 0) gfx_sprite_color(SPR_TORIEL_DEAD, 0, x, y, 0, C_WHITE, (uint8_t)a);
    return;
  }
  gfx_sprite(s, 0, x, y, 0, 255);
}

const MonDef toriel_def = {
  .name = "Toriel", .hp = 440, .at = 6, .df = 1, .exp = 150, .gold = 0, .spr_idle = SPR_TORIEL_B, .spr_hurt = SPR_TORIEL_HURT,
  .check = "Knows best for you.", .acts = {"Talk"}, .nacts = 1,
  .neutral = {"* Toriel looks through you.", "* Toriel prepares a magical\n  attack.", "* Toriel takes a deep breath.",
              "* Toriel is acting aloof."},
  .flags = MF_BOSS | MF_MISS_IS_SPARE, .on_spare = spare_toriel, .act = act_toriel, .talk = talk_toriel, .attack = attack_toriel, .flavor = flavor_toriel,
  .on_hit = hit_toriel, .ends = ends_toriel, .draw = draw_toriel,
};

/* ---------------------------------------------------------------- her attacks */
#define P(v) ((v) * FX)
static int bx0(void) { return bt.x0 + 3; }
static int bx1(void) { return bt.x1 - 3; }
static int by0(void) { return bt.y0 + 3; }
static int by1(void) { return bt.y1 - 3; }

static Bullet *fire(int x, int y, int vx, int vy, int dmg) {
  Bullet *b = bullet_new(SPR_FIREBULLET, P(x), P(y), vx, vy);
  if (b) b->owner = (uint8_t)bt.atk_owner, b->dmg = (int8_t)dmg, b->anim = 4;
  return b;
}

/* the paws: from the top-left (and, later, the bottom-right) they sweep across, leaving fire that then flies
 * at the SOUL and bounces off the walls; touching a paw ends it */
static void tor_hands(int t) {
  bool two = bt.turn >= 4;
  if (t < 0) {
    board_set(110, 80);
    return;
  }
  int sweep = 40;
  for (int k = 0; k < (two ? 2 : 1); k++) {
    if (t <= sweep) {
      /* the arc: from one top corner to the other, dipping down */
      int a = t * 128 / sweep;   /* half a turn */
      int cx = board_cx(), r = (bx1() - bx0()) / 2 - 6;
      int x = cx - r * gfx_cos(a) / 1024, y = (k ? by1() - 6 - r * gfx_sin(a) / 1024 / 3 : by0() + 6 + r * gfx_sin(a) / 1024 / 3);
      if (k) x = cx + r * gfx_cos(a) / 1024;
      bt.as[2 * k] = x, bt.as[2 * k + 1] = y;
      if (t % 4 == 0) {
        Bullet *b = fire(x, y, 0, 0, 3);
        if (b) b->kind = BK_SCRIPT, b->a = 0;
      }
    }
    /* the paw touching the SOUL: 4 and the attack is over */
    int px = (int)bt.as[2 * k], py = (int)bt.as[2 * k + 1];
    if (t <= sweep + 6 && abs_i(px - soul_x()) < 12 && abs_i(py - soul_y()) < 8) {
      soul_hurt(damage_to_player(4), 0);
      bt.atk_t = bt.atk_len;
    }
  }
  if (t == sweep + 4)
    for (int i = 0; i < MAX_BULLETS; i++) {
      Bullet *b = &bullets[i];
      if (b->kind != BK_SCRIPT) continue;
      int dx = bt.sx - b->x, dy = bt.sy - b->y, d = abs_i(dx) + abs_i(dy);
      if (!d) d = 1;
      b->vx = dx * P(2) / d, b->vy = dy * P(2) / d, b->a = 1;
    }
  for (int i = 0; i < MAX_BULLETS; i++) {
    Bullet *b = &bullets[i];
    if (b->kind != BK_SCRIPT || !b->a) continue;
    b->x += b->vx, b->y += b->vy;
    if (b->x < P(bx0() + 3) || b->x > P(bx1() - 3)) b->vx = -b->vx;
    if (b->y < P(by0() + 3) || b->y > P(by1() - 3)) b->vy = -b->vy;
    if (++b->t > 90) b->kind = BK_NONE;
  }
}

/* two streams of fire falling, crossing over each other, with holes to slip through */
static void tor_waves(int t) {
  bool hard = bt.turn >= 6;
  if (t < 0) {
    board_set(110, hard ? 60 : 80);
    return;
  }
  if (t < 150 && t % 4 == 0) {
    int w = bx1() - bx0() - 16;
    for (int s = 0; s < 2; s++) {
      if ((t / 4 + s * 3) % 9 >= 7) continue;   /* the holes */
      int x = board_cx() + (s ? 1 : -1) * w / 2 * gfx_sin(t * 2) / 1024;
      Bullet *b = fire(x, by0() - 4, 0, P(3) / 2, 4);
      if (b) b->kind = BK_SCRIPT;
    }
  }
  if (hard && t % 30 == 0) {
    fire(bx0() + 3, by0() + 10 + rnd_n(by1() - by0() - 20), 0, 0, 4);
    fire(bx1() - 3, by0() + 10 + rnd_n(by1() - by0() - 20), 0, 0, 4);
  }
  for (int i = 0; i < MAX_BULLETS; i++) {
    Bullet *b = &bullets[i];
    if (b->kind != BK_SCRIPT) continue;
    b->t++;
    if (b->y < P(by1() - 3)) b->y += b->vy;
    else if (b->t > 120) b->kind = BK_NONE;   /* (they gather at the bottom a while) */
  }
}

/* thicker streams that stay in their lanes: safe between them */
static void tor_helix(int t) {
  if (t < 0) {
    board_set(120, 80);
    return;
  }
  if (t < 150 && t % 5 == 0) {
    int lanes = 3, w = bx1() - bx0();
    for (int l = 0; l < lanes; l++) {
      int cx = bx0() + w * (2 * l + 1) / (2 * lanes) + 10 * gfx_sin(t * 3 + l * 85) / 1024;
      for (int k = -1; k <= 1; k++) fire(cx + k * 6, by0() - 4, 0, P(3) / 2, 4);
    }
  }
}

/* her fire falls anywhere, but turns away from the SOUL: she cannot bring herself to hurt it */
static void tor_falter(int t) {
  if (t < 0) {
    board_set(110, 80);
    return;
  }
  if (t < 120 && t % 6 == 0) {
    Bullet *b = fire(bx0() + 4 + rnd_n(bx1() - bx0() - 8), by0() - 4, 0, P(3) / 2, 0);
    if (b) b->kind = BK_SCRIPT, b->flags |= BF_HARMLESS;
  }
  for (int i = 0; i < MAX_BULLETS; i++) {
    Bullet *b = &bullets[i];
    if (b->kind != BK_SCRIPT) continue;
    int dx = b->x - bt.sx, dy = b->y - bt.sy;
    if (abs_i(dx) < P(16) && abs_i(dy) < P(16)) b->x += dx > 0 ? P(2) : -P(2);
    b->y += b->vy;
    if (b->y > P(by1() + 6)) b->kind = BK_NONE;
  }
}

void toriel_attack(int atk, int t) {
  switch (atk) {
    case ATK_TOR_HANDS: tor_hands(t); break;
    case ATK_TOR_WAVES: tor_waves(t); break;
    case ATK_TOR_HELIX: tor_helix(t); break;
    case ATK_TOR_FALTER: tor_falter(t); break;
  }
}

void toriel_draw_attack(int atk, int t) {
  if (atk == ATK_TOR_HANDS && t <= 46) {
    bool two = bt.turn >= 4;
    for (int k = 0; k < (two ? 2 : 1); k++) gfx_sprite(SPR_HANDBULLET, 0, (int)bt.as[2 * k], (int)bt.as[2 * k + 1], k ? GFX_FLIPX : 0, 255);
  }
}
