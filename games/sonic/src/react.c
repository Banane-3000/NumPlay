/* Sonic touching objects: each object on screen with a collision type has a box from a table of
 * 36 sizes; the first one Sonic's box overlaps decides. An enemy is destroyed if he rolls into it
 * (or is invincible), else he is hurt; an item (ring, monitor) is collected; a hurting thing hurts.
 * Hurt with rings, he drops them and is knocked back; with none, he dies. Points for enemies grow
 * with each one destroyed without landing: 100, 200, 500, then 1000. */
#include "s1.h"
#include "game.h"

#define flashtime 0x30
#define exitem_pointsframe 0x3E

static const uint8_t react_sizes[][2] = {
  {20, 20}, {12, 20}, {20, 12}, {4, 16}, {12, 18}, {16, 16}, {6, 6}, {24, 12}, {12, 16}, {16, 12},
  {8, 8}, {20, 16}, {20, 8}, {14, 14}, {24, 24}, {40, 16}, {16, 24}, {8, 16}, {32, 112}, {64, 32},
  {128, 32}, {32, 32}, {8, 8}, {4, 4}, {32, 8}, {12, 12}, {8, 4}, {24, 4}, {40, 4}, {4, 8},
  {4, 24}, {4, 40}, {4, 32}, {24, 24}, {12, 24}, {72, 8},
};
enum { col_32x32 = 6, col_16x16 = 11, col_40x32 = 12, col_16x16_alt = 23, col_8x64 = 33 };

void AddPoints(int points) {
  V.f_scorecount = 1;
  V.score += (uint32_t)points;
  if (V.score >= 999999) V.score = 999999;
  if (V.score < V.scorelife) return;
  V.scorelife += 5000;
  /* (only a Japanese Mega Drive gives a life for points: the game plays as on the others) */
}


int HurtSonic(Obj *o, Obj *a2) {
  if (!V.shield) {
    if (!V.rings) {
      if (!V.f_debugmode) {
        KillSonic(o, a2);
        return -1;
      }
    } else {
      Obj *a1 = FindFreeObj();
      if (a1) {
        a1->id = id_RingLoss;
        a1->x.w.i = o->x.w.i, a1->y.w.i = o->y.w.i;
      }
    }
  }
  V.shield = 0;
  o->routine = 4;
  Sonic_ResetOnFloor(o);
  o->status |= 2;
  o->vy = -0x400, o->vx = -0x200;
  if (o->status & 0x40) o->vy = -0x200, o->vx = -0x100;
  if ((uint16_t)o->x.w.i >= (uint16_t)a2->x.w.i) o->vx = (int16_t)-o->vx;
  o->inertia = 0;
  o->anim = id_Hurt;
  OW(o, flashtime) = 2 * 60;
  return -1;
}

void KillSonic(Obj *o, Obj *a2) {
  (void)a2;
  if (V.debuguse) return;
  V.invinc = 0;
  o->routine = 6;
  Sonic_ResetOnFloor(o);
  o->status |= 2;
  o->vy = -0x700, o->vx = 0, o->inertia = 0;
  OW(o, 0x38) = o->y.w.i;
  o->anim = id_Death;
  o->gfx |= 0x8000;
}

static int chk_hurt(Obj *o, Obj *a1) {
  if (V.invinc) return -1;
  if (OW(o, flashtime)) return -1;
  return HurtSonic(o, a1);
}

static int react_enemy(Obj *o, Obj *a1) {
  if (!V.invinc && o->anim != id_Roll) return chk_hurt(o, a1);
  if (a1->colprop) { /* a boss: obBossHits */
    o->vx = (int16_t)(-o->vx >> 1);
    o->vy = (int16_t)(-o->vy >> 1);
    a1->coltype = 0;
    if (!--a1->colprop) a1->status |= 0x80;
    return 0;
  }
  a1->status |= 0x80;
  uint16_t d0 = V.itembonus;
  V.itembonus += 2;
  if (d0 >= 6) d0 = 6;
  OW(a1, exitem_pointsframe) = (int16_t)d0;
  static const uint16_t combo[4] = {10, 20, 50, 100};
  int pts = combo[d0 >> 1];
  if (V.itembonus >= 32) pts = 1000, OW(a1, exitem_pointsframe) = 10;
  AddPoints(pts);
  a1->id = id_ExplosionItem;
  a1->routine = 0;
  if (o->vy < 0) o->vy += 0x100;
  else if ((uint16_t)o->y.w.i >= (uint16_t)a1->y.w.i) o->vy -= 0x100;
  else o->vy = (int16_t)-o->vy;
  return 0;
}

int ReactToItem(Obj *o) {
  int16_t d2 = (int16_t)(o->x.w.i - 8), d3 = o->y.w.i;
  int16_t d5 = (int16_t)((uint8_t)(o->height - 3));
  d3 = (int16_t)(d3 - d5);
  if (o->frame == fr_Duck) { /* (the original checks the frame: the box is lower while he ducks) */
    d3 = (int16_t)(d3 + ((19 - 3) - 10) * 2);
    d5 = 10;
  }
  int16_t d4 = 16;
  d5 = (int16_t)(d5 * 2);
  for (int i = NRESERVED; i < NOBJ; i++) {
    Obj *a1 = &objs[i];
    if (!(a1->render & 0x80) || !a1->coltype) continue;
    const uint8_t *sz = react_sizes[(a1->coltype & 0x3F) - 1];
    int16_t d1 = sz[0], d0 = (int16_t)(a1->x.w.i - d1 - d2);
    if ((uint16_t)(a1->x.w.i - d1) >= (uint16_t)d2) {
      if ((uint16_t)d0 > (uint16_t)d4) continue;
    } else if ((uint32_t)(uint16_t)d0 + (uint16_t)(d1 * 2) <= 0xFFFF) continue;
    d1 = sz[1];
    d0 = (int16_t)(a1->y.w.i - d1 - d3);
    if ((uint16_t)(a1->y.w.i - d1) >= (uint16_t)d3) {
      if ((uint16_t)d0 > (uint16_t)d5) continue;
    } else if ((uint32_t)(uint16_t)d0 + (uint16_t)(d1 * 2) <= 0xFFFF) continue;
    /* a hit */
    uint8_t kind = a1->coltype & 0xC0, size = a1->coltype & 0x3F;
    if (!kind) return react_enemy(o, a1);
    if (kind == 0xC0) {
      if (size == col_16x16) {
        a1->status |= 0x80;
        return chk_hurt(o, a1);
      }
      if (size == col_40x32) { /* the Yadrin: its spiked top hurts */
        if ((uint16_t)(d5 - d0) < 8) {
          int16_t x = (int16_t)(a1->x.w.i - 4);
          if (a1->status & 1) x -= 16;
          int16_t e = (int16_t)(x - d2);
          if ((uint16_t)x >= (uint16_t)d2) {
            if ((uint16_t)e <= (uint16_t)d4) return chk_hurt(o, a1);
          } else if ((uint32_t)(uint16_t)e + 24 > 0xFFFF) return chk_hurt(o, a1);
        }
        return react_enemy(o, a1);
      }
      if (size == col_16x16_alt || size == col_8x64) a1->colprop++;
      return 0;
    }
    if (kind & 0x80) return chk_hurt(o, a1);
    if (size == col_32x32) { /* a monitor */
      if (o->vy < 0) {
        if ((uint16_t)(o->y.w.i - 16) < (uint16_t)a1->y.w.i) return 0;
        o->vy = (int16_t)-o->vy;
        a1->vy = -0x180;
        if (!a1->routine2) a1->routine2 += 4;
        return 0;
      }
      if (o->anim != id_Roll) return 0;
      o->vy = (int16_t)-o->vy;
      a1->routine += 2;
      return 0;
    }
    if ((uint16_t)OW(o, flashtime) >= 90) return 0;
    a1->routine += 2;
    return 0;
  }
  return 0;
}
