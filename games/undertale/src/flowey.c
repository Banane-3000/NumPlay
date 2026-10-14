/* The first battle: Flowey and his "friendliness pellets", then Toriel. A scripted battle: no buttons, the SOUL
 * free in a small box, Flowey's words in his bubble (from the Undertale Wiki's records of the game). */
#include <stdio.h>
#include <string.h>
#include "battle.h"
#include "game.h"
#include "gfx.h"
#include "monsters.h"
#include "res.h"

enum {
  F_TALK1, F_PELLETS, F_AFTER, F_RING, F_FIREBALL, F_TORIEL_IN, F_TORIEL_TALK, F_DONE
};
/* bt.state: [0] step, [1] times dodged, [2] Flowey's face (a sprite), [3] Flowey's x offset, [4] Toriel's x */
static const char *talk_text;
static int ring_r;

static void bubble(const char *s) {
  talk_text = s;
  writer_start(&bt.bw, s, 0, 0, FONT_SMALL);
  bt.bw.line_h = 9;
}

static const char intro_text[] =
  "See that\nheart? That\nis your SOUL,\nthe very\nculmination\nof your\nbeing!|"
  "Your SOUL\nstarts off\nweak, but can\ngrow strong\nif you gain a\nlot of LV.|"
  "What's LV\nstand for?\nWhy, LOVE,\nof course!|"
  "You want\nsome LOVE,\ndon't you?|"
  "`7Don't worry,\nI'll share\nsome with\nyou!";
static const char share_text[] =
  "Down here,\nLOVE is\nshared\nthrough...|"
  "Little\nwhite...\n\"friendliness\npellets.\"|"
  "Are you\nready?|"
  "Move around!\nGet as many\nas you can!";
static const char hit_text[] =
  "`4You idiot.|"
  "In this\nworld, it's\nkill or BE\nkilled.|"
  "Why would\nANYONE pass\nup an\nopportunity\nlike this!?|"
  "`3Die.";
static const char miss1_text[] = "`6Hey buddy,\nyou missed\nthem.|`0Let's try\nagain, okay?";
static const char miss2_text[] = "`6Is this a\njoke? Are you\nbraindead?|RUN. INTO.\nTHE.\nfriendliness\npellets";
static const char miss3_text[] = "`2You know\nwhat's going\non here,\ndon't you?|You just\nwanted to see\nme suffer.|`3Die.";
static const char toriel_text[] =
  "What a\nterrible\ncreature,\ntorturing\nsuch a poor,\ninnocent\nyouth...|"
  "Ah, do not\nbe afraid, my\nchild.|"
  "I am #YTORIEL#X,\ncaretaker of\nthe #YRUINS#X.|"
  "I pass\nthrough this\nplace every\nday to see if\nanyone has\nfallen down.|"
  "You are the\nfirst human\nto come here\nin a long\ntime.|"
  "Come! I will\nguide you\nthrough the\ncatacombs.";

static const uint16_t faces[] = {SPR_FLOWEYNICE, SPR_FLOWEYNICESIDE, SPR_FLOWEYSASSY, SPR_FLOWEYEVIL, SPR_FLOWEYGRIN,
                                 SPR_FLOWEYPLAIN, SPR_FLOWEYPISSED, SPR_FLOWEYWINK, SPR_FLOWEYLAUGH, SPR_FLOWEYHURT};

void flowey_battle_start(void) {
  bt.scripted = true;
  bt.state[0] = F_TALK1;
  bt.state[2] = 0;
  board_set(84, 76);
  bt.x0 = bt.tx0, bt.y0 = bt.ty0, bt.x1 = bt.tx1, bt.y1 = bt.ty1;
  bt.sx = board_cx() * FX, bt.sy = board_cy() * FX + 8 * FX;
  bubble(intro_text);
}

static void pellets(void) {
  /* five pellets in an arc over Flowey, then at the SOUL, a little aimed */
  bullets_clear();
  for (int k = 0; k < 5; k++) {
    int x = 160 + (k - 2) * 14, y = 72 - (k == 0 || k == 4 ? 0 : k == 2 ? 14 : 9);
    Bullet *b = bullet_new(SPR_SPINBULLET, x * FX, y * FX, 0, 0);
    if (b) b->kind = BK_SCRIPT, b->a = (int16_t)(20 + k * 3), b->anim = 3, b->dmg = 19, b->flags = BF_NOCLIP;
  }
}

void flowey_battle_update(uint32_t keys, uint32_t pressed) {
  int st = bt.state[0];
  if (st != F_FIREBALL && st != F_TORIEL_IN) soul_update(keys);
  switch (st) {
    case F_TALK1:
    case F_AFTER:
    case F_TORIEL_TALK:
      bt.state[2] = bt.bw.face_expr;
      if (!writer_update(&bt.bw, pressed)) {
        if (st == F_TORIEL_TALK) {
          bt.state[0] = F_DONE;
          bt.result = BR_SCRIPTED;
          return;
        }
        if (talk_text == intro_text) {
          bubble(share_text);
          bt.state[2] = 7;
        } else if (talk_text == hit_text || talk_text == miss3_text) {
          bt.state[0] = F_RING, bt.t = 0;
          ring_r = 70;
          bullets_clear();
          for (int k = 0; k < 24; k++) {
            Bullet *b = bullet_new(SPR_SPINBULLET, 0, 0, 0, 0);
            if (b) b->kind = BK_SCRIPT, b->a = (int16_t)(k * 256 / 24), b->anim = 3, b->flags = BF_HARMLESS | BF_NOCLIP;
          }
          bt.state[2] = 8;
        } else {
          bt.state[0] = F_PELLETS, bt.t = 0;
          pellets();
        }
      }
      break;
    case F_PELLETS: {
      int alive = 0;
      for (int i = 0; i < MAX_BULLETS; i++) {
        Bullet *b = &bullets[i];
        if (b->kind != BK_SCRIPT) continue;
        alive++;
        if (b->a > 0) {
          b->t++;
          if (b->t % 3 == 0) b->frame++;
          if (--b->a == 0) {
            int dx = bt.sx - b->x, dy = bt.sy - b->y, d = abs_i(dx) + abs_i(dy);
            if (!d) d = 1;
            b->vx = dx * FX * 2 / d, b->vy = dy * FX * 2 / d;
          }
          continue;
        }
        /* a little homing */
        b->vx += (bt.sx > b->x ? 1 : -1) * FX / 32;
        b->x += b->vx, b->y += b->vy;
        if (++b->t % 3 == 0) b->frame++;
        if (b->y > (bt.y1 + 10) * FX || b->x < 0 || b->x > SCREEN_W * FX) b->kind = BK_NONE;
        int dx = b->x / FX - soul_x(), dy = b->y / FX - soul_y();
        if (dx * dx + dy * dy < 36) {
          /* hit: 19, down to 1 */
          sv.hp = 1;
          bullets_clear();
          bt.state[0] = F_AFTER;
          bubble(hit_text);
          bt.state[2] = 4;
          return;
        }
      }
      if (!alive) {
        bt.state[1]++;
        bt.state[0] = F_AFTER;
        bubble(bt.state[1] == 1 ? miss1_text : bt.state[1] == 2 ? miss2_text : miss3_text);
      }
      break;
    }
    case F_RING:
      /* the ring closes in, slowly; Flowey laughs */
      if (bt.t % 3 == 0 && ring_r > 14) ring_r--;
      for (int i = 0; i < MAX_BULLETS; i++) {
        Bullet *b = &bullets[i];
        if (b->kind != BK_SCRIPT) continue;
        b->x = bt.sx + ring_r * gfx_cos(b->a + bt.t) * FX / 1024;
        b->y = bt.sy + ring_r * gfx_sin(b->a + bt.t) * FX / 1024;
        if (bt.t % 3 == 0) b->frame++;
      }
      if (ring_r <= 14 && bt.t > 150) bt.state[0] = F_FIREBALL, bt.t = 0;
      break;
    case F_FIREBALL:
      /* a fireball from the right knocks Flowey away */
      if (bt.t == 10) {
        bullets_clear();
        bt.state[2] = 9;
      }
      if (bt.t > 10) bt.state[3] = (int16_t)(bt.state[3] - 6);
      if (bt.t == 30) sv.hp = sv.maxhp;
      if (bt.t > 40) bt.state[0] = F_TORIEL_IN, bt.t = 0, bt.state[4] = 340;
      break;
    case F_TORIEL_IN:
      bt.state[4] = (int16_t)max_i(160, bt.state[4] - 4);
      if (bt.state[4] == 160 && bt.t > 30) {
        bt.state[0] = F_TORIEL_TALK;
        bubble(toriel_text);
      }
      break;
  }
}

void flowey_battle_draw(void) {
  int st = bt.state[0];
  gfx_sprite(SPR_BATTLEBG, 0, 0, 0, 0, 255);
  int fx = 160 + bt.state[3];
  if (st <= F_FIREBALL) {
    int e = bt.state[2];
    if (e < 0 || e > 9) e = 0;
    uint16_t s = faces[e];
    int talking = writer_talking(&bt.bw);
    gfx_sprite(s, talking ? (int)(g_frame / 4) % 2 : 0, fx - 21, 72, 0, 255);
  }
  if (st == F_FIREBALL && bt.t < 12) gfx_sprite(SPR_FIREBULLET, (int)(g_frame / 2) % 2, 330 - bt.t * 15, 90, 0, 255);
  if (st >= F_TORIEL_IN) gfx_sprite(SPR_TORIEL_B, 0, bt.state[4], 120, 0, 255);
  gfx_rect(bt.x0, bt.y0, bt.x1 - bt.x0, bt.y1 - bt.y0, C_WHITE, 255);
  gfx_rect(bt.x0 + 2, bt.y0 + 2, bt.x1 - bt.x0 - 4, bt.y1 - bt.y0 - 4, C_BLACK, 255);
  bullets_draw();
  gfx_sprite(SPR_HEART, 0, soul_x(), soul_y(), 0, 255);
  /* the bubble */
  if (st == F_TALK1 || st == F_AFTER || st == F_TORIEL_TALK) {
    int bx = (st == F_TORIEL_TALK ? bt.state[4] : fx) + 30, by = 40;
    gfx_rect(bx, by, 96, 72, C_WHITE, 255);
    gfx_rect(bx - 6, by + 20, 6, 5, C_WHITE, 255);
    bt.bw.x = bx + 5, bt.bw.y = by + 5;
    writer_draw(&bt.bw, C_BLACK);
  }
  /* the HUD, without the buttons */
  char s[32];
  gfx_text(FONT_SMALL, 70, 201, "LV 1", C_WHITE, 255);
  gfx_sprite(SPR_HPNAME, 0, 122, 202, 0, 255);
  int w = sv.maxhp * 6 / 10 + 1, f = max_i(1, sv.hp * 6 / 10);
  gfx_rect(137, 200, w, 10, rgb565(0xC00000), 255);
  gfx_rect(137, 200, f, 10, C_YELLOW, 255);
  snprintf(s, sizeof s, "%02d / %d", sv.hp, sv.maxhp);
  gfx_text(FONT_SMALL, 137 + w + 8, 201, s, C_WHITE, 255);
}
