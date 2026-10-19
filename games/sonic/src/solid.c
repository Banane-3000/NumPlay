/* Objects Sonic can stand on or bump into. A solid object pushes him out of its box by the
 * shortest way (to a side: he pushes against it; on top: he stands on it and moves with it;
 * from below: he stops, or is squashed against the floor); a platform only holds him from above,
 * and a slope reads its top from a table of heights. The results are the original's: 0 nothing,
 * 1 at a side, -1 on top or below. */
#include "s1.h"
#include "game.h"

#define standonobject 0x3D

static void Solid_ResetFloor(Obj *o, Obj *a1) {
  if (a1->status & 8) {
    Obj *a2 = &objs[OB(a1, standonobject)];
    a2->status &= (uint8_t)~8;
    a2->routine2 = 0;
  }
  OB(a1, standonobject) = (uint8_t)((o - objs) & 0x7F);
  a1->angle = 0;
  a1->vy = 0;
  a1->inertia = a1->vx;
  if (a1->status & 2) Sonic_ResetOnFloor(a1);
  a1->status |= 8;
  o->status |= 8;
}

static int no_collision(Obj *o, Obj *a1) {
  if (o->status & 0x20) {
    a1->anim = 0, a1->prevani = id_Run; /* (a word written over both) */
    o->status &= (uint8_t)~0x20;
    a1->status &= (uint8_t)~0x20;
  }
  return 0;
}

static int solid_collision(Obj *o, Obj *a1, int16_t d0, int16_t d1, int16_t d2, int16_t d3, int16_t d4) {
  if (V.f_playerctrl & 0x80) return no_collision(o, a1);
  if (player->routine >= 6 || V.debuguse) return 0;
  int16_t d5 = d0;
  if ((uint16_t)d1 < (uint16_t)d0) {
    d0 = (int16_t)(d0 - d1 * 2);
    d5 = (int16_t)-d0;
  }
  int16_t e1 = d3;
  if ((uint16_t)d2 < (uint16_t)d3) {
    d3 = (int16_t)(d3 - 4 - d4);
    e1 = (int16_t)-d3;
  }
  if ((uint16_t)d5 > (uint16_t)e1) { /* above or below */
    if (d3 < 0) {
      if (a1->vy == 0) {
        if (!(a1->status & 2)) KillSonic(a1, o);
        return -1;
      }
      if (a1->vy < 0) {
        a1->y.w.i -= d3;
        a1->vy = 0;
      }
      return -1;
    }
    if ((uint16_t)d3 >= 0x10) return no_collision(o, a1);
    d3 -= 4;
    int16_t w = o->actwid, x = (int16_t)(w + a1->x.w.i - o->x.w.i);
    if (x < 0 || (uint16_t)x >= (uint16_t)(w * 2) || a1->vy < 0) return 0;
    a1->y.w.i -= d3;
    a1->y.w.i -= 1;
    Solid_ResetFloor(o, a1);
    o->routine2 = 2;
    o->status |= 8;
    return -1;
  }
  if ((uint16_t)e1 <= 4) goto side_air;
  if (d0 > 0) {
    if (a1->vx >= 0) a1->inertia = 0, a1->vx = 0;
  } else if (d0 < 0) {
    if (a1->vx < 0) a1->inertia = 0, a1->vx = 0;
  }
  a1->x.w.i -= d0;
  if (a1->status & 2) goto side_air;
  a1->status |= 0x20;
  o->status |= 0x20;
  return 1;
side_air:
  o->status &= (uint8_t)~0x20;
  a1->status &= (uint8_t)~0x20;
  return 1;
}

static int solid_check(Obj *o, int16_t d1, int16_t d2) {
  Obj *a1 = player;
  int16_t d0 = (int16_t)(a1->x.w.i - o->x.w.i + d1);
  if (d0 < 0 || (uint16_t)d0 > (uint16_t)(d1 * 2)) return no_collision(o, a1);
  d2 = (int16_t)(d2 + (int8_t)a1->height);
  int16_t d3 = (int16_t)(a1->y.w.i - o->y.w.i + 4 + d2);
  int16_t d4 = (int16_t)(d2 * 2);
  if (d3 < 0 || (uint16_t)d3 >= (uint16_t)d4) return no_collision(o, a1);
  return solid_collision(o, a1, d0, d1, d2, d3, d4);
}

void MvSonicOnPtfm(Obj *o, int16_t d2, int16_t d3) {
  Obj *a1 = player;
  if ((V.f_playerctrl & 0x80) || player->routine >= 6 || V.debuguse) return;
  a1->y.w.i = (int16_t)(o->y.w.i - d3 - a1->height);
  a1->x.w.i = (int16_t)(a1->x.w.i - (d2 - o->x.w.i));
}
void MvSonicOnPtfm2(Obj *o, int16_t d2) { MvSonicOnPtfm(o, d2, 9); }

static int solid_standing(Obj *o, int16_t d1, int16_t d3, int16_t d4) {
  Obj *a1 = player;
  int16_t d0 = (int16_t)(a1->x.w.i - o->x.w.i + d1);
  if (!(a1->status & 2) && d0 >= 0 && (uint16_t)d0 < (uint16_t)(d1 * 2)) {
    MvSonicOnPtfm(o, d4, d3);
    return 0;
  }
  a1->status &= (uint8_t)~8;
  o->status &= (uint8_t)~8;
  o->routine2 = 0;
  return 0;
}

int SolidObject(Obj *o, int16_t d1, int16_t d2, int16_t d3, int16_t d4) {
  if (o->routine2) return solid_standing(o, d1, d3, d4);
  if (!(o->render & 0x80)) return no_collision(o, player);
  return solid_check(o, d1, d2);
}

int SolidObject_NoRenderChk(Obj *o, int16_t d1, int16_t d2, int16_t d3, int16_t d4) {
  if (o->routine2) return solid_standing(o, d1, d3, d4);
  return solid_check(o, d1, d2);
}

int SolidObject_Heightmap(Obj *o, int16_t d1, int16_t d2, const uint8_t *heights) {
  Obj *a1 = player;
  if (!(o->render & 0x80)) return no_collision(o, a1);
  int16_t d0 = (int16_t)(a1->x.w.i - o->x.w.i + d1);
  if (d0 < 0) return no_collision(o, a1);
  int16_t d3 = (int16_t)(d1 * 2);
  if ((uint16_t)d0 > (uint16_t)d3) return no_collision(o, a1);
  int16_t d5 = d0;
  if (o->render & 1) d5 = (int16_t)(~d5 + d3);
  uint8_t h = (uint8_t)(heights[(uint16_t)d5 >> 1] - heights[0]);
  int16_t top = (int16_t)(o->y.w.i - h);
  d2 = (int16_t)(d2 + (int8_t)a1->height);
  d3 = (int16_t)(a1->y.w.i - top + 4 + d2);
  int16_t d4 = (int16_t)(d2 * 2);
  if (d3 < 0 || (uint16_t)d3 >= (uint16_t)d4) return no_collision(o, a1);
  return solid_collision(o, a1, d0, d1, d2, d3, d4);
}

/* ------------------------------------------------------------------ platforms */
static void Plat_NoCheck(Obj *o, Obj *a1) {
  if (a1->status & 8) {
    Obj *a2 = &objs[OB(a1, standonobject)];
    a2->status &= (uint8_t)~8;
    a2->routine2 = 0;
    if (a2->routine == 4) a2->routine -= 2;
  }
  OB(a1, standonobject) = (uint8_t)((o - objs) & 0x7F);
  a1->angle = 0;
  a1->vy = 0;
  a1->inertia = a1->vx;
  if (a1->status & 2) Sonic_ResetOnFloor(a1);
  a1->status |= 8;
  o->status |= 8;
}

static void plat_alt_y(Obj *o, int16_t d0) {
  Obj *a1 = player;
  int16_t d2 = a1->y.w.i, d1 = (int16_t)(d2 + (int8_t)a1->height + 4);
  if ((uint16_t)d0 > (uint16_t)d1) return;
  d0 = (int16_t)(d0 - d1);
  if ((uint16_t)d0 < (uint16_t)-16) return;
  if ((V.f_playerctrl & 0x80) || a1->routine >= 6) return;
  a1->y.w.i = (int16_t)(d2 + d0 + 3);
  o->routine += 2;
  Plat_NoCheck(o, a1);
}

static bool plat_x(Obj *o, int16_t d1) {
  Obj *a1 = player;
  if (a1->vy < 0) return false;
  int16_t d0 = (int16_t)(a1->x.w.i - o->x.w.i + d1);
  return d0 >= 0 && (uint16_t)d0 < (uint16_t)(d1 * 2);
}

void PlatformObject(Obj *o, int16_t d1) {
  if (plat_x(o, d1)) plat_alt_y(o, (int16_t)(o->y.w.i - 8));
}
void Plat_NoXCheck(Obj *o) { plat_alt_y(o, (int16_t)(o->y.w.i - 8)); }
void Plat_NoXCheck_AltY(Obj *o, int16_t y) { plat_alt_y(o, y); }
void PlatformObject_CustomHeight(Obj *o, int16_t d1, int16_t d3) {
  if (plat_x(o, d1)) plat_alt_y(o, (int16_t)(o->y.w.i - d3));
}
void Plat_NoCheck_ext(Obj *o) { Plat_NoCheck(o, player); }

void SlopeObject(Obj *o, int16_t d1, const uint8_t *heights) {
  Obj *a1 = player;
  if (a1->vy < 0) return;
  int16_t d0 = (int16_t)(a1->x.w.i - o->x.w.i + d1);
  if (d0 < 0) return;
  d1 = (int16_t)(d1 * 2);
  if ((uint16_t)d0 >= (uint16_t)d1) return;
  if (o->render & 1) d0 = (int16_t)(~d0 + d1);
  plat_alt_y(o, (int16_t)(o->y.w.i - heights[(uint16_t)d0 >> 1]));
}

void ExitPlatform2(Obj *o, int16_t d1, int16_t d2) {
  Obj *a1 = player;
  d2 = (int16_t)(d2 * 2);
  if (!(a1->status & 2)) {
    int16_t d0 = (int16_t)(a1->x.w.i - o->x.w.i + d1);
    if (d0 >= 0 && (uint16_t)d0 < (uint16_t)d2) return;
  }
  a1->status &= (uint8_t)~8;
  o->routine = 2;
  o->status &= (uint8_t)~8;
}
void ExitPlatform(Obj *o, int16_t d1) { ExitPlatform2(o, d1, d1); }
