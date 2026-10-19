/* Collision with the level: the 16x16 block at a point, and the distance from a point to the
 * surface in it (or in the next block) along one of the four directions, from each block's height
 * map (16 heights, one a column, or one a row for walls) and its angle. Blocks can be flipped; a
 * chunk marked as a loop has a twin that Sonic uses when he is "behind" it (the render flag $40). */
#include "s1.h"
#include "collide.h"

static const uint16_t chunk0; /* (the original's blank chunk reads a word that is always 0) */
uint16_t col_tile;            /* the block word FindNearestTile found last (the original's (a1)) */

static const uint16_t *nearest(Obj *o, int16_t y, int16_t x) {
  int idx = (((uint16_t)y >> 1) & 0x380) + (((uint16_t)x >> 8) & 0x7F);
  uint8_t c = (&lvllayout[0][0][0])[idx];
  if (!c) return &chunk0;
  if (c & 0x80) {
    c &= 0x7F;
    if (o->render & 0x40) {
      c++;
      if (c == 0x29) c = 0x51;
    }
  }
  int n = (uint8_t)(c - 1);
  extern int level_nchunks(void);
  if (n >= level_nchunks()) return &chunk0;
  return &chunks256[n * 256 + (((uint16_t)y >> 4) & 15) * 16 + (((uint16_t)x >> 4) & 15)];
}

const uint16_t *FindNearestTile(Obj *o, int16_t y, int16_t x) {
  const uint16_t *p = nearest(o, y, x);
  col_tile = *p;
  return p;
}

static uint8_t col_index(uint16_t block) { return V.collindex[block]; }
static const uint8_t *angle_map(void) { return DATA(D_ANGLEMAP); }

/* (FindFloor2 and FindWall2: the same for the next block, without looking further) */
static int16_t floor2(Obj *o, int16_t y, int16_t x, int bit, uint16_t eor, uint8_t *angle, uint16_t *d4out) {
  uint16_t d4 = *FindNearestTile(o, y, x), id = d4 & 0x7FF;
  *d4out = d4;
  uint8_t h;
  if (!id || !(d4 >> bit & 1) || !(h = col_index(id))) return (int16_t)(0xF - (y & 0xF));
  *angle = angle_map()[h];
  int16_t d1 = x;
  if (d4 & 0x800) d1 = (int16_t)~d1, *angle = (uint8_t)-*angle;
  if (d4 & 0x1000) *angle = (uint8_t)(-(uint8_t)(*angle + 0x40) - 0x40);
  int16_t d0 = ((const int8_t *)DATA(D_COLLARRAY1))[h * 16 + (d1 & 0xF)];
  d4 ^= eor;
  *d4out = d4;
  if (d4 & 0x1000) d0 = (int16_t)-d0;
  if (!d0) return (int16_t)(0xF - (y & 0xF));
  if (d0 < 0) {
    if ((int16_t)((y & 0xF) + d0) >= 0) return (int16_t)(0xF - (y & 0xF));
    return (int16_t)~(y & 0xF);
  }
  return (int16_t)(0xF - ((y & 0xF) + d0));
}

int16_t FindFloor(Obj *o, int16_t y, int16_t x, int bit, uint16_t eor, int16_t step, uint8_t *angle, uint16_t *d4out) {
  uint16_t d4 = *FindNearestTile(o, y, x), id = d4 & 0x7FF, tmp;
  if (!d4out) d4out = &tmp;
  *d4out = d4;
  uint8_t h;
  if (!id || !(d4 >> bit & 1) || !(h = col_index(id))) goto blank;
  *angle = angle_map()[h];
  int16_t d1 = x;
  if (d4 & 0x800) d1 = (int16_t)~d1, *angle = (uint8_t)-*angle;
  if (d4 & 0x1000) *angle = (uint8_t)(-(uint8_t)(*angle + 0x40) - 0x40);
  int16_t d0 = ((const int8_t *)DATA(D_COLLARRAY1))[h * 16 + (d1 & 0xF)];
  d4 ^= eor;
  *d4out = d4;
  if (d4 & 0x1000) d0 = (int16_t)-d0;
  if (!d0) goto blank;
  if (d0 < 0) {
    if ((int16_t)((y & 0xF) + d0) >= 0) goto blank;
    goto max;
  }
  if (d0 == 0x10) goto max;
  return (int16_t)(0xF - ((y & 0xF) + d0));
blank:
  return (int16_t)(floor2(o, (int16_t)(y + step), x, bit, eor, angle, d4out) + 0x10);
max:
  return (int16_t)(floor2(o, (int16_t)(y - step), x, bit, eor, angle, d4out) - 0x10);
}

static int16_t wall2(Obj *o, int16_t y, int16_t x, int bit, uint16_t eor, uint8_t *angle, uint16_t *d4out) {
  uint16_t d4 = *FindNearestTile(o, y, x), id = d4 & 0x7FF;
  *d4out = d4;
  uint8_t h;
  if (!id || !(d4 >> bit & 1) || !(h = col_index(id))) return (int16_t)(0xF - (x & 0xF));
  *angle = angle_map()[h];
  int16_t d1 = y;
  if (d4 & 0x1000) d1 = (int16_t)~d1, *angle = (uint8_t)(-(uint8_t)(*angle + 0x40) - 0x40);
  if (d4 & 0x800) *angle = (uint8_t)-*angle;
  int16_t d0 = ((const int8_t *)DATA(D_COLLARRAY2))[h * 16 + (d1 & 0xF)];
  d4 ^= eor;
  *d4out = d4;
  if (d4 & 0x800) d0 = (int16_t)-d0;
  if (!d0) return (int16_t)(0xF - (x & 0xF));
  if (d0 < 0) {
    if ((int16_t)((x & 0xF) + d0) >= 0) return (int16_t)(0xF - (x & 0xF));
    return (int16_t)~(x & 0xF);
  }
  return (int16_t)(0xF - ((x & 0xF) + d0));
}

int16_t FindWall(Obj *o, int16_t y, int16_t x, int bit, uint16_t eor, int16_t step, uint8_t *angle, uint16_t *d4out) {
  uint16_t d4 = *FindNearestTile(o, y, x), id = d4 & 0x7FF, tmp;
  if (!d4out) d4out = &tmp;
  *d4out = d4;
  uint8_t h;
  if (!id || !(d4 >> bit & 1) || !(h = col_index(id))) goto blank;
  *angle = angle_map()[h];
  int16_t d1 = y;
  if (d4 & 0x1000) d1 = (int16_t)~d1, *angle = (uint8_t)(-(uint8_t)(*angle + 0x40) - 0x40);
  if (d4 & 0x800) *angle = (uint8_t)-*angle;
  int16_t d0 = ((const int8_t *)DATA(D_COLLARRAY2))[h * 16 + (d1 & 0xF)];
  d4 ^= eor;
  *d4out = d4;
  if (d4 & 0x800) d0 = (int16_t)-d0;
  if (!d0) goto blank;
  if (d0 < 0) {
    if ((int16_t)((x & 0xF) + d0) >= 0) goto blank;
    goto max;
  }
  if (d0 == 0x10) goto max;
  return (int16_t)(0xF - ((x & 0xF) + d0));
blank:
  return (int16_t)(wall2(o, y, (int16_t)(x + step), bit, eor, angle, d4out) + 0x10);
max:
  return (int16_t)(wall2(o, y, (int16_t)(x - step), bit, eor, angle, d4out) - 0x10);
}

/* ------------------------------------------------------------------ the object helpers */
/* ObjFloorDist: from the bottom of the object to the floor below its x; *angle_out the floor's angle */
int16_t ObjFloorDist2(Obj *o, int16_t x, uint8_t *angle_out) {
  int16_t y = (int16_t)(o->y.w.i + (int8_t)o->height);
  V.anglebuffer = 0;
  int16_t d1 = FindFloor(o, y, x, 0xD, 0, 0x10, &V.anglebuffer, NULL);
  uint8_t a = V.anglebuffer;
  if (a & 1) a = 0;
  if (angle_out) *angle_out = a;
  return d1;
}
int16_t ObjFloorDist(Obj *o, uint8_t *angle_out) { return ObjFloorDist2(o, o->x.w.i, angle_out); }

int16_t ObjHitWallRight(Obj *o, int16_t dx, uint8_t *angle_out) {
  V.anglebuffer = 0;
  int16_t d1 = FindWall(o, o->y.w.i, (int16_t)(o->x.w.i + dx), 0xE, 0, 0x10, &V.anglebuffer, NULL);
  uint8_t a = V.anglebuffer;
  if (a & 1) a = 0xC0;
  if (angle_out) *angle_out = a;
  return d1;
}
int16_t ObjHitWallLeft(Obj *o, int16_t dx, uint8_t *angle_out) {
  V.anglebuffer = 0;
  int16_t d1 = FindWall(o, o->y.w.i, (int16_t)(o->x.w.i + dx), 0xE, 0x800, -0x10, &V.anglebuffer, NULL);
  uint8_t a = V.anglebuffer;
  if (a & 1) a = 0x40;
  if (angle_out) *angle_out = a;
  return d1;
}
int16_t ObjHitCeiling(Obj *o, uint8_t *angle_out) {
  int16_t y = (int16_t)((o->y.w.i - (int8_t)o->height) ^ 0xF);
  int16_t d1 = FindFloor(o, y, o->x.w.i, 0xE, 0x1000, -0x10, &V.anglebuffer, NULL);
  uint8_t a = V.anglebuffer;
  if (a & 1) a = 0x80;
  if (angle_out) *angle_out = a;
  return d1;
}
