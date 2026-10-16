/* The bullet board: the SOUL, the bullets, what hurts (battle.h). */
#include <string.h>
#include "battle.h"
#include "game.h"
#include "gfx.h"
#include "items.h"
#include "res.h"

Bullet bullets[MAX_BULLETS];

Bullet *bullet_new(uint16_t spr, int x, int y, int vx, int vy) {
  for (int i = 0; i < MAX_BULLETS; i++)
    if (bullets[i].kind == BK_NONE) {
      Bullet *b = &bullets[i];
      memset(b, 0, sizeof *b);
      b->kind = BK_LINEAR, b->spr = spr, b->x = x, b->y = y, b->vx = vx, b->vy = vy;
      b->alpha = 255;
      Frame f;
      if (frame_get(spr, 0, &f)) b->r = (int16_t)(((f.w < f.h ? f.w : f.h) * 2 / 5));
      if (b->r < 2) b->r = 2;
      return b;
    }
  return NULL;
}

void bullets_clear(void) { memset(bullets, 0, sizeof bullets); }

int soul_x(void) { return bt.sx / FX; }
int soul_y(void) { return bt.sy / FX; }

/* the SOUL: 8 x 8, moved inside the board by the arrows (X held: slower, as in the game) */
/* the blue SOUL: it falls, and up jumps (higher while held), as the game's (halved) */
static int32_t soul_vy;
#define BLUE_JUMP (3 * FX)
#define BLUE_GRAVITY 3          /* 1/16 px a frame, each frame */
void soul_set_mode(int mode) {
  bt.soul_mode = mode;
  soul_vy = 0;
}
void soul_update(uint32_t keys) {
  int sp = bt.speed;
  if (keys & K_X) sp /= 2;
  int dx = ((keys & K_RIGHT) != 0) - ((keys & K_LEFT) != 0), dy = ((keys & K_DOWN) != 0) - ((keys & K_UP) != 0);
  int x0 = (bt.x0 + 2 + 4) * FX, x1 = (bt.x1 - 2 - 4) * FX, y0 = (bt.y0 + 2 + 4) * FX, y1 = (bt.y1 - 2 - 4) * FX;
  if (bt.soul_mode == SOUL_BLUE) {
    bt.sx += dx * sp;
    if (bt.sy >= y1) {
      soul_vy = (keys & K_UP) ? -BLUE_JUMP : 0;
    } else {
      /* letting go of up cuts the jump short */
      if (soul_vy < -FX / 2 && !(keys & K_UP)) soul_vy = -FX / 2;
      soul_vy += BLUE_GRAVITY;
    }
    bt.sy += soul_vy;
    if (bt.sy < y0) bt.sy = y0, soul_vy = 0;
  } else {
    bt.sx += dx * sp, bt.sy += dy * sp;
  }
  if (bt.sx < x0) bt.sx = x0;
  if (bt.sx > x1) bt.sx = x1;
  if (bt.sy < y0) bt.sy = y0;
  if (bt.sy > y1) bt.sy = y1;
}

void bullets_update(void) {
  for (int i = 0; i < MAX_BULLETS; i++) {
    Bullet *b = &bullets[i];
    if (b->kind == BK_NONE) continue;
    if (b->kind != BK_SCRIPT) b->t++;
    switch (b->kind) {
      case BK_LINEAR:
      case BK_GRAVITY:
        b->vx += b->ax, b->vy += b->ay;
        b->x += b->vx, b->y += b->vy;
        break;
      case BK_HOMING: {
        /* turns towards the SOUL, at its speed */
        int dx = bt.sx - b->x, dy = bt.sy - b->y;
        int d = abs_i(dx) + abs_i(dy);
        if (d) {
          b->vx += dx * b->a / d, b->vy += dy * b->a / d;
          int cap = b->b;
          if (b->vx > cap) b->vx = cap;
          if (b->vx < -cap) b->vx = -cap;
          if (b->vy > cap) b->vy = cap;
          if (b->vy < -cap) b->vy = -cap;
        }
        b->x += b->vx, b->y += b->vy;
        break;
      }
      case BK_ORBIT: {
        /* around (a, b) in pixels, radius c (1/16 px), angle d (1/256 turns, x16) */
        b->d = (int16_t)(b->d + b->vx);
        int r = b->c;
        b->x = b->a * FX + r * gfx_cos(b->d / 16) / 1024;
        b->y = b->b * FX + r * gfx_sin(b->d / 16) / 1024;
        b->c = (int16_t)(b->c + b->vy);
        break;
      }
      default:
        break;
    }
    if (b->kind != BK_SCRIPT && b->anim && b->t % b->anim == 0) b->frame++;
    if (b->life && b->t >= b->life) b->kind = BK_NONE;
    /* far out of the board: gone */
    int px = b->x / FX, py = b->y / FX;
    if (px < -60 || px > SCREEN_W + 60 || py < -60 || py > SCREEN_H + 60) b->kind = BK_NONE;
  }
}

/* a bone: its box w x h around (cx, cy), lying along its longer side; knobs at its ends */
void gfx_bone(int cx, int cy, int w, int h, uint16_t col, uint8_t a) {
  if (h >= w) {
    int x = cx - w / 2, y = cy - h / 2, k = w / 2 > 1 ? w / 2 : 1;
    gfx_rect(x + w / 4, y + k, w - 2 * (w / 4), h - 2 * k, col, a);
    gfx_circle(x + k / 2 + 1, y + k, k, col, a), gfx_circle(x + w - k / 2 - 1, y + k, k, col, a);
    gfx_circle(x + k / 2 + 1, y + h - k - 1, k, col, a), gfx_circle(x + w - k / 2 - 1, y + h - k - 1, k, col, a);
  } else {
    int x = cx - w / 2, y = cy - h / 2, k = h / 2 > 1 ? h / 2 : 1;
    gfx_rect(x + k, y + h / 4, w - 2 * k, h - 2 * (h / 4), col, a);
    gfx_circle(x + k, y + k / 2 + 1, k, col, a), gfx_circle(x + k, y + h - k / 2 - 1, k, col, a);
    gfx_circle(x + w - k - 1, y + k / 2 + 1, k, col, a), gfx_circle(x + w - k - 1, y + h - k / 2 - 1, k, col, a);
  }
}

void bullets_draw(void) {
  for (int i = 0; i < MAX_BULLETS; i++) {
    const Bullet *b = &bullets[i];
    if (b->kind == BK_NONE || (b->flags & BF_HIDDEN)) continue;
    uint16_t color = b->color == BC_BLUE ? rgb565(0x14A9FF) : b->color == BC_ORANGE ? rgb565(0xFCA600) : b->color == BC_GREEN ? rgb565(0x00C000) : 0;
    if (b->spr == SPR_BONE) {
      gfx_bone(b->x / FX, b->y / FX, b->w, b->h, color ? color : C_WHITE, b->alpha);
      continue;
    }
    if (b->angle) gfx_sprite_ex(b->spr, b->frame, b->x / FX, b->y / FX, 256, 256, b->angle, color, b->alpha);
    else if (color) gfx_sprite_color(b->spr, b->frame, b->x / FX, b->y / FX, 0, color, b->alpha);
    else gfx_sprite(b->spr, b->frame, b->x / FX, b->y / FX, 0, b->alpha);
  }
}

/* the damage of a monster's bullet, as the game reckons it: its AT, one more at each of the SOUL's HP
 * thresholds, less a fifth of DF (the shown one), at least 1 */
int damage_to_player(int at) {
  static const int16_t steps[] = {21, 30, 40, 50, 60, 70, 80, 90};
  int dmg = at;
  for (int i = 0; i < 8; i++)
    if (sv.maxhp >= steps[i]) dmg++;
  int df = sv.df - 10 + armor_df();
  dmg -= (df + 2) / 5;
  return dmg < 1 ? 1 : dmg;
}

void soul_hurt(int dmg, int color) {
  (void)color;
  if (bt.inv) return;
  sv.hp = (int16_t)(sv.hp - dmg);
  if (sv.hp < 0) sv.hp = 0;
  bt.inv = 30;
}

bool soul_hit_test(void) {
  int sx = bt.sx / FX, sy = bt.sy / FX;
  bool hit = false;
  static int last_x, last_y;
  bool moved = sx != last_x || sy != last_y;
  last_x = sx, last_y = sy;
  for (int i = 0; i < MAX_BULLETS; i++) {
    Bullet *b = &bullets[i];
    if (b->kind == BK_NONE || (b->flags & (BF_HIDDEN | BF_HARMLESS))) continue;
    int bx = b->x / FX, by = b->y / FX;
    bool touch;
    if (b->flags & BF_BOX) touch = sx + 3 > bx - b->w / 2 && sx - 3 < bx + b->w / 2 && sy + 3 > by - b->h / 2 && sy - 3 < by + b->h / 2;
    else touch = (sx - bx) * (sx - bx) + (sy - by) * (sy - by) <= (b->r + 3) * (b->r + 3);
    if (!touch) continue;
    if (b->color == BC_BLUE && !moved) continue;     /* blue: still, it passes */
    if (b->color == BC_ORANGE && moved) continue;    /* orange: moving, it passes */
    if (b->color == BC_GREEN) {
      sv.hp = (int16_t)min_i(sv.maxhp, sv.hp + (b->dmg ? b->dmg : 1));
      b->kind = BK_NONE;
      bt.greens++;
      continue;
    }
    if (bt.inv) continue;
    int at = b->dmg ? b->dmg : bt.mon[b->owner].at;
    soul_hurt(damage_to_player(at), b->color);
    hit = true;
    if (b->flags & BF_ONCE) b->kind = BK_NONE;
  }
  return hit;
}
