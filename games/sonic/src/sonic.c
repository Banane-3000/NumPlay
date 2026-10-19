/* Sonic: the player object, as the original moves him. On the ground his speed along it (inertia)
 * grows by $C a frame up to $600 and turns into x and y speeds by the floor's angle; he sticks to
 * the floor by two sensors at his feet (or at his side on walls, at his head on ceilings), slows on
 * slopes, rolls with Down, jumps at $680 perpendicular to the floor; in the air gravity pulls $38 a
 * frame, a jump is cut short when the button is let go, and he lands by sensors in the direction
 * he moves. Then the hurt and death states, loops, the animations (walking and running frames
 * chosen by angle and speed) and the art of each frame copied to video memory. */
#include "s1.h"
#include "collide.h"
#include "game.h"
#include "objs.h"

#define son_maxspeed 0x600
#define son_acceleration 0xC
#define son_deceleration 0x80
#define son_jumpspeed 0x680
#define sonic_width 9
#define sonic_height 19
#define sonic_roll_width 7
#define sonic_roll_height 14
#define sonic_quick_size 10

uint16_t tracksonic[64][2];   /* v_tracksonic: where he was, for the invincibility stars */

/* ------------------------------------------------------------------ collision probes */
typedef struct {
  int16_t d1, d0; /* the nearer distance, and the other */
  uint8_t d3;     /* the angle of the nearer surface */
} Probe;

static Probe find_smaller(int16_t d1, int16_t d0, uint8_t snap) {
  Probe p;
  uint8_t a = V.anglebuffer2;
  if (d1 > d0) {
    a = V.anglebuffer;
    int16_t t = d0;
    d0 = d1, d1 = t;
  }
  if (a & 1) a = snap;
  p.d1 = d1, p.d0 = d0, p.d3 = a;
  return p;
}

Probe Sonic_FindFloor(Obj *o) {
  int16_t y = (int16_t)(o->y.w.i + (int8_t)o->height);
  int16_t a = FindFloor(o, y, (int16_t)(o->x.w.i + (int8_t)o->width), 0xD, 0, 0x10, &V.anglebuffer, NULL);
  int16_t b = FindFloor(o, y, (int16_t)(o->x.w.i - (int8_t)o->width), 0xD, 0, 0x10, &V.anglebuffer2, NULL);
  return find_smaller(b, a, 0);
}
static Probe Sonic_FindCeiling(Obj *o) {
  int16_t y = (int16_t)((o->y.w.i - (int8_t)o->height) ^ 0xF);
  int16_t a = FindFloor(o, y, (int16_t)(o->x.w.i + (int8_t)o->width), 0xE, 0x1000, -0x10, &V.anglebuffer, NULL);
  int16_t b = FindFloor(o, y, (int16_t)(o->x.w.i - (int8_t)o->width), 0xE, 0x1000, -0x10, &V.anglebuffer2, NULL);
  return find_smaller(b, a, 0x80);
}
static Probe Sonic_FindWallRight(Obj *o) {
  int16_t x = (int16_t)(o->x.w.i + (int8_t)o->height);
  int16_t a = FindWall(o, (int16_t)(o->y.w.i - (int8_t)o->width), x, 0xE, 0, 0x10, &V.anglebuffer, NULL);
  int16_t b = FindWall(o, (int16_t)(o->y.w.i + (int8_t)o->width), x, 0xE, 0, 0x10, &V.anglebuffer2, NULL);
  return find_smaller(b, a, 0xC0);
}
static Probe Sonic_FindWallLeft(Obj *o) {
  int16_t x = (int16_t)((o->x.w.i - (int8_t)o->height) ^ 0xF);
  int16_t a = FindWall(o, (int16_t)(o->y.w.i - (int8_t)o->width), x, 0xE, 0x800, -0x10, &V.anglebuffer, NULL);
  int16_t b = FindWall(o, (int16_t)(o->y.w.i + (int8_t)o->width), x, 0xE, 0x800, -0x10, &V.anglebuffer2, NULL);
  return find_smaller(b, a, 0x40);
}

static Probe snap(int16_t d1, uint8_t to) {
  Probe p;
  p.d1 = d1, p.d0 = 0, p.d3 = V.anglebuffer & 1 ? to : V.anglebuffer;
  return p;
}
static Probe floor_quick(Obj *o, int16_t y, int16_t x) {
  return snap(FindFloor(o, (int16_t)(y + sonic_quick_size), x, 0xE, 0, 0x10, &V.anglebuffer, NULL), 0);
}
static Probe ceiling_quick(Obj *o, int16_t y, int16_t x) {
  return snap(FindFloor(o, (int16_t)((y - sonic_quick_size) ^ 0xF), x, 0xE, 0x1000, -0x10, &V.anglebuffer, NULL), 0x80);
}
static Probe wallright_quick(Obj *o, int16_t y, int16_t x) {
  return snap(FindWall(o, y, (int16_t)(x + sonic_quick_size), 0xE, 0, 0x10, &V.anglebuffer, NULL), 0xC0);
}
static Probe wallleft_quick(Obj *o, int16_t y, int16_t x) {
  return snap(FindWall(o, y, (int16_t)((x - sonic_quick_size) ^ 0xF), 0xE, 0x800, -0x10, &V.anglebuffer, NULL), 0x40);
}

/* the room ahead in the direction angle, from where he will be next frame */
static Probe Sonic_CalcRoomAhead(Obj *o, uint8_t d0) {
  int16_t x = (int16_t)((o->x.l + ((int32_t)o->vx << 8)) >> 16);
  int16_t y = (int16_t)((o->y.l + ((int32_t)o->vy << 8)) >> 16);
  V.anglebuffer = V.anglebuffer2 = d0;
  uint8_t d1 = d0;
  uint8_t q;
  if ((int8_t)(uint8_t)(d0 + 0x20) >= 0) q = (uint8_t)(((int8_t)d1 < 0 ? d1 + 1 : d1) + 0x1F);
  else q = (uint8_t)(((int8_t)d1 < 0 ? d1 - 1 : d1) + 0x20);
  q &= 0xC0;
  if (q == 0) return floor_quick(o, y, x);
  if (q == 0x80) return ceiling_quick(o, y, x);
  if (!(d1 & 0x38)) y += 8;
  if (q == 0x40) return wallleft_quick(o, y, x);
  return wallright_quick(o, y, x);
}

static Probe Sonic_CalcHeadroom(Obj *o, uint8_t d0) {
  V.anglebuffer = V.anglebuffer2 = d0;
  switch ((uint8_t)(d0 + 0x20) & 0xC0) {
  case 0x40: return Sonic_FindWallLeft(o);
  case 0x80: return Sonic_FindCeiling(o);
  case 0xC0: return Sonic_FindWallRight(o);
  default: return Sonic_FindFloor(o);
  }
}

/* ------------------------------------------------------------------ on the ground */
static void Sonic_SetAngle(Obj *o, int16_t *d1, int16_t d0) {
  uint8_t a = V.anglebuffer2;
  if (*d1 > d0) a = V.anglebuffer, *d1 = d0;
  if (a & 1) o->angle = (uint8_t)((o->angle + 0x20) & 0xC0);
  else o->angle = a;
}

static void off_ledge(Obj *o) {
  o->status |= 2;
  o->status &= (uint8_t)~0x20;
  o->prevani = id_Run;
}

void Sonic_AnglePos(Obj *o) {
  if (o->status & 8) {
    V.anglebuffer = V.anglebuffer2 = 0;
    return;
  }
  V.anglebuffer = V.anglebuffer2 = 3;
  uint8_t a = o->angle, q;
  if ((int8_t)(uint8_t)(a + 0x20) >= 0) q = (uint8_t)(((int8_t)a < 0 ? a + 1 : a) + 0x1F);
  else q = (uint8_t)(((int8_t)a < 0 ? a - 1 : a) + 0x20);
  q &= 0xC0;
  int8_t h = (int8_t)o->height, w = (int8_t)o->width;
  int16_t d1, d0;
  if (q == 0x40) { /* walking up a wall on his left */
    int16_t x = (int16_t)((o->x.w.i - h) ^ 0xF);
    d0 = FindWall(o, (int16_t)(o->y.w.i - w), x, 0xD, 0x800, -0x10, &V.anglebuffer, NULL);
    d1 = FindWall(o, (int16_t)(o->y.w.i + w), x, 0xD, 0x800, -0x10, &V.anglebuffer2, NULL);
    Sonic_SetAngle(o, &d1, d0);
    if (!d1) return;
    if (d1 < 0) {
      if (d1 >= -0xE) o->x.w.i -= d1;
      return;
    }
    if (d1 > 0xE && !OB(o, sticktoconvex)) off_ledge(o);
    else o->x.w.i -= d1;
    return;
  }
  if (q == 0x80) { /* on the ceiling */
    int16_t y = (int16_t)((o->y.w.i - h) ^ 0xF);
    d0 = FindFloor(o, y, (int16_t)(o->x.w.i + w), 0xD, 0x1000, -0x10, &V.anglebuffer, NULL);
    d1 = FindFloor(o, y, (int16_t)(o->x.w.i - w), 0xD, 0x1000, -0x10, &V.anglebuffer2, NULL);
    Sonic_SetAngle(o, &d1, d0);
    if (!d1) return;
    if (d1 < 0) {
      if (d1 >= -0xE) o->y.w.i -= d1;
      return;
    }
    if (d1 > 0xE && !OB(o, sticktoconvex)) off_ledge(o);
    else o->y.w.i -= d1;
    return;
  }
  if (q == 0xC0) { /* up a wall on his right */
    int16_t x = (int16_t)(o->x.w.i + h);
    d0 = FindWall(o, (int16_t)(o->y.w.i - w), x, 0xD, 0, 0x10, &V.anglebuffer, NULL);
    d1 = FindWall(o, (int16_t)(o->y.w.i + w), x, 0xD, 0, 0x10, &V.anglebuffer2, NULL);
    Sonic_SetAngle(o, &d1, d0);
    if (!d1) return;
    if (d1 < 0) {
      if (d1 >= -0xE) o->x.w.i += d1;
      return;
    }
    if (d1 > 0xE && !OB(o, sticktoconvex)) off_ledge(o);
    else o->x.w.i += d1;
    return;
  }
  int16_t y = (int16_t)(o->y.w.i + h);
  d0 = FindFloor(o, y, (int16_t)(o->x.w.i + w), 0xD, 0, 0x10, &V.anglebuffer, NULL);
  d1 = FindFloor(o, y, (int16_t)(o->x.w.i - w), 0xD, 0, 0x10, &V.anglebuffer2, NULL);
  Sonic_SetAngle(o, &d1, d0);
  if (!d1) return;
  if (d1 < 0) {
    if (d1 >= -0xE) o->y.w.i += d1;
    return;
  }
  if (d1 > 0xE && !OB(o, sticktoconvex)) off_ledge(o);
  else o->y.w.i += d1;
}

/* ------------------------------------------------------------------ the modes */
static void look_reset(void) {
  if (V.lookshift == 0x60) return;
  if ((uint16_t)V.lookshift < 0x60) V.lookshift += 4;
  V.lookshift -= 2;
}

static void Sonic_WallSpeedAdjust(Obj *o) {
  if ((int8_t)(uint8_t)(o->angle + 0x40) < 0) return;
  if (!o->inertia) return;
  uint8_t d1 = o->inertia < 0 ? 0x40 : 0xC0;
  uint8_t d0 = (uint8_t)(o->angle + d1);
  Probe p = Sonic_CalcRoomAhead(o, d0);
  if (p.d1 >= 0) return;
  int16_t d = (int16_t)(p.d1 << 8);
  switch ((uint8_t)(d0 + 0x20) & 0xC0) {
  case 0x00: o->vy += d; break;
  case 0x40: o->vx -= d, o->status |= 0x20, o->inertia = 0; break;
  case 0x80: o->vy -= d; break;
  default: o->vx += d, o->status |= 0x20, o->inertia = 0; break;
  }
}

static void Sonic_AngleSpeed(Obj *o) {
  int16_t c, s = CalcSine(o->angle, &c);
  o->vx = (int16_t)(((int32_t)c * o->inertia) >> 8);
  o->vy = (int16_t)(((int32_t)s * o->inertia) >> 8);
  Sonic_WallSpeedAdjust(o);
}

static void Sonic_MoveLeft(Obj *o, int16_t d4, int16_t d5, int16_t d6) {
  int16_t d0 = o->inertia;
  if (d0 > 0) {
    uint16_t u = (uint16_t)d0;
    d0 = u < (uint16_t)d4 ? -0x80 : (int16_t)(u - d4);
    o->inertia = d0;
    /* (the angle's quadrant overwrites the low byte of the speed before it is compared) */
    uint8_t q = (uint8_t)((o->angle + 0x20) & 0xC0);
    d0 = (int16_t)((d0 & 0xFF00) | q);
    if (q) return;
    if (d0 < 0x400) return;
    o->anim = id_Stop;
    o->status &= (uint8_t)~1;
    return;
  }
  if (!(o->status & 1)) {
    o->status |= 1;
    o->status &= (uint8_t)~0x20;
    o->prevani = id_Run;
  }
  d0 = (int16_t)(d0 - d5);
  if (d0 <= -d6) d0 = (int16_t)-d6;
  o->inertia = d0;
  o->anim = id_Walk;
}

static void Sonic_MoveRight(Obj *o, int16_t d4, int16_t d5, int16_t d6) {
  int16_t d0 = o->inertia;
  if (d0 < 0) {
    uint32_t s = (uint32_t)(uint16_t)d0 + (uint16_t)d4;
    d0 = s > 0xFFFF ? 0x80 : (int16_t)s;
    o->inertia = d0;
    uint8_t q = (uint8_t)((o->angle + 0x20) & 0xC0);
    d0 = (int16_t)((d0 & 0xFF00) | q);
    if (q) return;
    if (d0 > -0x400) return;
    o->anim = id_Stop;
    o->status |= 1;
    return;
  }
  if (o->status & 1) {
    o->status &= (uint8_t)~1;
    o->status &= (uint8_t)~0x20;
    o->prevani = id_Run;
  }
  d0 = (int16_t)(d0 + d5);
  if (d0 >= d6) d0 = d6;
  o->inertia = d0;
  o->anim = id_Walk;
}

static void Sonic_Move(Obj *o) {
  int16_t d6 = V.sonspeedmax, d5 = V.sonspeedacc, d4 = V.sonspeeddec;
  if (V.f_slidemode) {
    Sonic_AngleSpeed(o);
    return;
  }
  if (!OW(o, locktime)) {
    if (V.jpadhold2 & btnL) Sonic_MoveLeft(o, d4, d5, d6);
    if (V.jpadhold2 & btnR) Sonic_MoveRight(o, d4, d5, d6);
    if (((o->angle + 0x20) & 0xC0) || o->inertia) goto reset;
    o->status &= (uint8_t)~0x20;
    o->anim = id_Wait;
    if (o->status & 8) {
      Obj *a1 = &objs[OB(o, standonobject)];
      if (a1->status & 0x80) goto lookup;
      int16_t d1 = a1->actwid, d2 = (int16_t)(d1 * 2 - 4);
      d1 = (int16_t)(d1 + o->x.w.i - a1->x.w.i);
      if (d1 < 4) goto leftbalance;
      if (d1 >= d2) goto rightbalance;
      goto lookup;
    }
    if (ObjFloorDist(o, NULL) < 12) goto lookup;
    if (OB(o, angleright) == 3) goto rightbalance;
    if (OB(o, angleleft) != 3) goto lookup;
  leftbalance:
    o->status |= 1;
    goto balance;
  rightbalance:
    o->status &= (uint8_t)~1;
  balance:
    o->anim = id_Balance;
    goto reset;
  lookup:
    if (V.jpadhold2 & btnUp) {
      o->anim = id_LookUp;
      if (V.lookshift != 0xC8) V.lookshift += 2;
      goto letgo;
    }
    if (V.jpadhold2 & btnDn) {
      o->anim = id_Duck;
      if (V.lookshift != 8) V.lookshift -= 2;
      goto letgo;
    }
  }
reset:
  look_reset();
letgo:
  if (!(V.jpadhold2 & (btnL | btnR))) {
    int16_t d0 = o->inertia;
    if (d0 > 0) o->inertia = (uint16_t)d0 < (uint16_t)d5 ? 0 : (int16_t)(d0 - d5);
    else if (d0 < 0) {
      uint32_t s = (uint32_t)(uint16_t)d0 + (uint16_t)d5;
      o->inertia = s > 0xFFFF ? 0 : (int16_t)s;
    }
  }
  Sonic_AngleSpeed(o);
}

static void Sonic_AngledRollSpeed(Obj *o) {
  int16_t c, s = CalcSine(o->angle, &c);
  o->vy = (int16_t)(((int32_t)s * o->inertia) >> 8);
  int16_t d1 = (int16_t)(((int32_t)c * o->inertia) >> 8);
  if (d1 > 0x1000) d1 = 0x1000;
  if (d1 < -0x1000) d1 = -0x1000;
  o->vx = d1;
  Sonic_WallSpeedAdjust(o);
}

static void Sonic_RollSpeed(Obj *o) {
  int16_t d6 = (int16_t)(V.sonspeedmax << 1), d5 = (int16_t)(V.sonspeedacc >> 1), d4 = (int16_t)(V.sonspeeddec >> 2);
  (void)d6;
  if (V.f_slidemode) {
    Sonic_AngledRollSpeed(o);
    return;
  }
  if (!OW(o, locktime)) {
    if (V.jpadhold2 & btnL) {
      int16_t d0 = o->inertia;
      if (d0 > 0) {
        o->inertia = (uint16_t)d0 < (uint16_t)d4 ? -0x80 : (int16_t)(d0 - d4);
      } else {
        o->status |= 1;
        o->anim = id_Roll;
      }
    }
    if (V.jpadhold2 & btnR) {
      int16_t d0 = o->inertia;
      if (d0 < 0) {
        uint32_t s = (uint32_t)(uint16_t)d0 + (uint16_t)d4;
        o->inertia = s > 0xFFFF ? 0x80 : (int16_t)s;
      } else {
        o->status &= (uint8_t)~1;
        o->anim = id_Roll;
      }
    }
  }
  int16_t d0 = o->inertia;
  if (d0 > 0) o->inertia = (uint16_t)d0 < (uint16_t)d5 ? 0 : (int16_t)(d0 - d5);
  else if (d0 < 0) {
    uint32_t s = (uint32_t)(uint16_t)d0 + (uint16_t)d5;
    o->inertia = s > 0xFFFF ? 0 : (int16_t)s;
  }
  if (!o->inertia) {
    o->status &= (uint8_t)~4;
    o->height = sonic_height, o->width = sonic_width;
    o->anim = id_Wait;
    o->y.w.i -= sonic_height - sonic_roll_height;
  }
  Sonic_AngledRollSpeed(o);
}

static void Sonic_JumpDirection(Obj *o) {
  int16_t d6 = V.sonspeedmax, d5 = (int16_t)(V.sonspeedacc << 1);
  if (!(o->status & 0x10)) {
    int16_t d0 = o->vx;
    if (V.jpadhold2 & btnL) {
      o->status |= 1;
      d0 = (int16_t)(d0 - d5);
      if (d0 <= -d6) d0 = (int16_t)-d6;
    }
    if (V.jpadhold2 & btnR) {
      o->status &= (uint8_t)~1;
      d0 = (int16_t)(d0 + d5);
      if (d0 >= d6) d0 = d6;
    }
    o->vx = d0;
  }
  look_reset();
  /* air drag: only while going up slower than 4 pixels a frame */
  if ((uint16_t)o->vy < (uint16_t)-0x400) return;
  int16_t d0 = o->vx, d1 = (int16_t)(d0 >> 5);
  if (!d1) return;
  if (d1 > 0) o->vx = (uint16_t)d0 < (uint16_t)d1 ? 0 : (int16_t)(d0 - d1);
  else o->vx = (uint16_t)d0 < (uint16_t)d1 ? (int16_t)(d0 - d1) : 0;
}

void KillSonic(Obj *o, Obj *killer);

static void Sonic_LevelBound(Obj *o) {
  int16_t x = (int16_t)((o->x.l + ((int32_t)o->vx << 8)) >> 16);
  int16_t d0 = (int16_t)(V.limitleft2 + 16);
  if ((uint16_t)d0 > (uint16_t)x) goto sides;
  d0 = (int16_t)(V.limitright2 + 320 - 24);
  if (!V.f_lockscreen) d0 += 64;
  if ((uint16_t)d0 <= (uint16_t)x) goto sides;
bottom:
  if ((int16_t)(V.limitbtm2 + 224) < o->y.w.i) {
    if (V.zone == id_SBZ && V.act == 1 && (uint16_t)player->x.w.i >= 0x2000) {
      V.lastlamp = 0;
      V.f_restart = 1;
      V.zone = id_LZ, V.act = 3;
      return;
    }
    KillSonic(o, o);
  }
  return;
sides:
  o->x.w.i = d0;
  o->x.w.f = 0;
  o->vx = 0;
  o->inertia = 0;
  goto bottom;
}

static void Sonic_ChkRoll(Obj *o) {
  if (o->status & 4) return;
  o->status |= 4;
  o->height = sonic_roll_height, o->width = sonic_roll_width;
  o->anim = id_Roll;
  o->y.w.i += sonic_height - sonic_roll_height;
  if (!o->inertia) o->inertia = 0x200;
}

static void Sonic_Roll(Obj *o) {
  if (V.f_slidemode) return;
  int16_t d0 = o->inertia < 0 ? (int16_t)-o->inertia : o->inertia;
  if ((uint16_t)d0 < 0x80) return;
  if (V.jpadhold2 & (btnL | btnR)) return;
  if (V.jpadhold2 & btnDn) Sonic_ChkRoll(o);
}

/* true when he jumped: the rest of the mode is skipped (the original drops its return address) */
static bool Sonic_Jump(Obj *o) {
  if (!(V.jpadpress2 & btnABC)) return false;
  Probe p = Sonic_CalcHeadroom(o, (uint8_t)(o->angle + 0x80));
  if (p.d1 < 6) return false;
  int16_t d2 = o->status & 0x40 ? son_jumpspeed - 0x300 : son_jumpspeed;
  int16_t c, s = CalcSine((uint8_t)(o->angle - 0x40), &c);
  o->vx = (int16_t)(o->vx + (((int32_t)c * d2) >> 8));
  o->vy = (int16_t)(o->vy + (((int32_t)s * d2) >> 8));
  o->status |= 2;
  o->status &= (uint8_t)~0x20;
  OB(o, jumping) = 1;
  OB(o, sticktoconvex) = 0;
  o->height = sonic_height, o->width = sonic_width;
  if (o->status & 4) {
    o->status |= 0x10;
    return true;
  }
  o->height = sonic_roll_height, o->width = sonic_roll_width;
  o->anim = id_Roll;
  o->status |= 4;
  o->y.w.i += sonic_height - sonic_roll_height;
  return true;
}

static void Sonic_JumpHeight(Obj *o) {
  if (OB(o, jumping)) {
    int16_t d1 = o->status & 0x40 ? -0x200 : -0x400;
    if (d1 <= o->vy) return;
    if (V.jpadhold2 & btnABC) return;
    o->vy = d1;
    return;
  }
  if (o->vy < -0xFC0) o->vy = -0xFC0;
}

static void Sonic_SlopeResistWalk(Obj *o) {
  if ((uint8_t)(o->angle + 0x60) >= 0xC0) return;
  int16_t d0 = (int16_t)(((int32_t)CalcSine(o->angle, NULL) * 0x20) >> 8);
  if (!o->inertia) return;
  if (o->inertia < 0 || d0) o->inertia = (int16_t)(o->inertia + d0);
}

static void Sonic_SlopeResistRoll(Obj *o) {
  if ((uint8_t)(o->angle + 0x60) >= 0xC0) return;
  int32_t d0 = ((int32_t)CalcSine(o->angle, NULL) * 0x50) >> 8;
  if (o->inertia >= 0) {
    if ((int16_t)d0 < 0) d0 >>= 2;
  } else {
    if ((int16_t)d0 >= 0) d0 >>= 2;
  }
  o->inertia = (int16_t)(o->inertia + (int16_t)d0);
}

static void Sonic_SlopeRepel(Obj *o) {
  if (OB(o, sticktoconvex)) return;
  if (OW(o, locktime)) {
    OW(o, locktime)--;
    return;
  }
  if (!((o->angle + 0x20) & 0xC0)) return;
  int16_t d0 = o->inertia < 0 ? (int16_t)-o->inertia : o->inertia;
  if ((uint16_t)d0 >= 0x280) return;
  o->inertia = 0;
  o->status |= 2;
  OW(o, locktime) = 30;
}

static void Sonic_JumpAngle(Obj *o) {
  uint8_t a = o->angle;
  if (!a) return;
  if ((int8_t)a < 0) {
    a = (uint8_t)(a + 2);
    if (a < 2) a = 0; /* (carried past $FF) */
  } else {
    a = a >= 2 ? (uint8_t)(a - 2) : 0;
  }
  o->angle = a;
}

void Sonic_ResetOnFloor(Obj *o) {
  o->status &= (uint8_t)~(0x20 | 2 | 0x10);
  if (o->status & 4) {
    o->status &= (uint8_t)~4;
    o->height = sonic_height, o->width = sonic_width;
    o->anim = id_Walk;
    o->y.w.i -= sonic_height - sonic_roll_height;
  }
  OB(o, jumping) = 0;
  V.itembonus = 0;
}

static void Sonic_Floor(Obj *o) {
  uint8_t d0 = (uint8_t)((CalcAngle(o->vx, o->vy) - 0x20) & 0xC0);
  Probe p;
  if (d0 == 0x40) { /* moving left */
    p = wallleft_quick(o, o->y.w.i, o->x.w.i);
    if (p.d1 < 0) {
      o->x.w.i -= p.d1, o->vx = 0, o->inertia = o->vy;
      return;
    }
    p = Sonic_FindCeiling(o);
    if (p.d1 < 0) {
      o->y.w.i -= p.d1;
      if (o->vy < 0) o->vy = 0;
      return;
    }
    if (o->vy < 0) return;
    p = Sonic_FindFloor(o);
    if (p.d1 >= 0) return;
    o->y.w.i += p.d1, o->angle = p.d3;
    Sonic_ResetOnFloor(o);
    o->anim = id_Walk, o->vy = 0, o->inertia = o->vx;
    return;
  }
  if (d0 == 0x80) { /* moving up */
    p = wallleft_quick(o, o->y.w.i, o->x.w.i);
    if (p.d1 < 0) o->x.w.i -= p.d1, o->vx = 0;
    p = wallright_quick(o, o->y.w.i, o->x.w.i);
    if (p.d1 < 0) o->x.w.i += p.d1, o->vx = 0;
    p = Sonic_FindCeiling(o);
    if (p.d1 >= 0) return;
    o->y.w.i -= p.d1;
    if (!((p.d3 + 0x20) & 0x40)) {
      o->vy = 0;
      return;
    }
    o->angle = p.d3;
    Sonic_ResetOnFloor(o);
    o->inertia = o->vy;
    if ((int8_t)p.d3 < 0) o->inertia = (int16_t)-o->inertia;
    return;
  }
  if (d0 == 0xC0) { /* moving right */
    p = wallright_quick(o, o->y.w.i, o->x.w.i);
    if (p.d1 < 0) {
      o->x.w.i += p.d1, o->vx = 0, o->inertia = o->vy;
      return;
    }
    p = Sonic_FindCeiling(o);
    if (p.d1 < 0) {
      o->y.w.i -= p.d1;
      if (o->vy < 0) o->vy = 0;
      return;
    }
    if (o->vy < 0) return;
    p = Sonic_FindFloor(o);
    if (p.d1 >= 0) return;
    o->y.w.i += p.d1, o->angle = p.d3;
    Sonic_ResetOnFloor(o);
    o->anim = id_Walk, o->vy = 0, o->inertia = o->vx;
    return;
  }
  /* moving down */
  p = wallleft_quick(o, o->y.w.i, o->x.w.i);
  if (p.d1 < 0) o->x.w.i -= p.d1, o->vx = 0;
  p = wallright_quick(o, o->y.w.i, o->x.w.i);
  if (p.d1 < 0) o->x.w.i += p.d1, o->vx = 0;
  p = Sonic_FindFloor(o);
  if (p.d1 >= 0) return;
  int8_t d2 = (int8_t)-(int8_t)((uint8_t)(o->vy >> 8) + 8);
  if (!((int8_t)p.d1 >= d2) && (int8_t)p.d0 < d2) return;
  o->y.w.i += p.d1, o->angle = p.d3;
  Sonic_ResetOnFloor(o);
  o->anim = id_Walk;
  if ((uint8_t)(p.d3 + 0x20) & 0x40) { /* a steep slope */
    o->vx = 0;
    if (o->vy > 0xFC0) o->vy = 0xFC0;
  } else if ((uint8_t)(p.d3 + 0x10) & 0x20) { /* a shallow slope */
    o->vy >>= 1;
  } else {
    o->vy = 0, o->inertia = o->vx;
    return;
  }
  o->inertia = o->vy;
  if ((int8_t)p.d3 < 0) o->inertia = (int16_t)-o->inertia;
}

static void Sonic_MdNormal(Obj *o) {
  if (Sonic_Jump(o)) return;
  Sonic_SlopeResistWalk(o);
  Sonic_Move(o);
  Sonic_Roll(o);
  Sonic_LevelBound(o);
  SpeedToPos(o);
  Sonic_AnglePos(o);
  Sonic_SlopeRepel(o);
}
static void Sonic_MdJump(Obj *o) {
  Sonic_JumpHeight(o);
  Sonic_JumpDirection(o);
  Sonic_LevelBound(o);
  ObjectFall(o);
  if (o->status & 0x40) o->vy -= 0x38 - 0x10;
  Sonic_JumpAngle(o);
  Sonic_Floor(o);
}
static void Sonic_MdRoll(Obj *o) {
  if (Sonic_Jump(o)) return;
  Sonic_SlopeResistRoll(o);
  Sonic_RollSpeed(o);
  Sonic_LevelBound(o);
  SpeedToPos(o);
  Sonic_AnglePos(o);
  Sonic_SlopeRepel(o);
}

/* ------------------------------------------------------------------ power-ups, water */
static void Sonic_Display(Obj *o) {
  uint16_t t = OUW(o, flashtime);
  if (t) {
    OUW(o, flashtime)--;
    if (t & 4) DisplaySprite(o);
  } else DisplaySprite(o);
  if (V.invinc && OUW(o, invtime)) {
    if (!--OUW(o, invtime)) V.invinc = 0;
  }
  if (V.shoes && OUW(o, shoetime)) {
    if (!--OUW(o, shoetime)) {
      V.sonspeedmax = son_maxspeed, V.sonspeedacc = son_acceleration, V.sonspeeddec = son_deceleration;
      V.shoes = 0;
    }
  }
}

static void Sonic_RecordPosition(Obj *o) {
  int i = (V.trackpos & 0xFF) >> 2;
  tracksonic[i][0] = (uint16_t)o->x.w.i;
  tracksonic[i][1] = (uint16_t)o->y.w.i;
  V.trackpos = (uint16_t)((V.trackpos & 0xFF00) | (uint8_t)(V.trackpos + 4));
}

static void Sonic_Water(Obj *o) {
  if (V.zone != id_LZ) return;
  if (V.waterpos1 >= o->y.w.i) {
    if (!(o->status & 0x40)) return;
    o->status &= (uint8_t)~0x40;
    ResumeMusic();
    V.sonspeedmax = son_maxspeed, V.sonspeedacc = son_acceleration, V.sonspeeddec = son_deceleration;
    o->vy = (int16_t)(o->vy << 1);
    if (!o->vy) return;
    OBJ(12)->id = id_Splash;
    if (o->vy <= -0x1000) o->vy = -0x1000;
    return;
  }
  if (o->status & 0x40) return;
  o->status |= 0x40;
  ResumeMusic();
  OBJ(13)->id = id_DrownCount;
  obSubtype(OBJ(13)) = 0x81;
  V.sonspeedmax = son_maxspeed / 2, V.sonspeedacc = son_acceleration / 2, V.sonspeeddec = son_deceleration / 2;
  o->vx >>= 1;
  o->vy >>= 2;
  if (!o->vy) return;
  OBJ(12)->id = id_Splash;
}

/* ------------------------------------------------------------------ loops */
static void Sonic_Loops(Obj *o) {
  if (V.zone != id_SLZ && V.zone != id_GHZ) return;
  uint8_t c = (&lvllayout[0][0][0])[(((uint16_t)o->y.w.i >> 1) & 0x380) + ((uint8_t)(o->x.w.i >> 8) & 0x7F)];
  if (c == V.loop256[2] || c == V.loop256[3]) {
    Sonic_ChkRoll(o);
    return;
  }
  if (c == V.loop256[0]) goto chkifleft;
  if (c == V.loop256[1]) {
    if (!(o->status & 2)) goto chkifleft;
    o->render &= (uint8_t)~0x40;
    return;
  }
  o->render &= (uint8_t)~0x40;
  return;
chkifleft: {
  uint8_t x = (uint8_t)o->x.w.i;
  if (x < 44) {
    o->render &= (uint8_t)~0x40;
    return;
  }
  if (x >= 224) {
    o->render |= 0x40;
    return;
  }
  if (!(o->render & 0x40)) {
    if (!o->angle || o->angle > 0x80) return;
    o->render |= 0x40;
    return;
  }
  if (o->angle <= 0x80) return;
  o->render &= (uint8_t)~0x40;
}
}

/* ------------------------------------------------------------------ animation */
static void Sonic_Animate(Obj *o) {
  const uint8_t *tab = ani_get(ANI_Sonic);
  uint8_t a = o->anim;
  if (a != o->prevani) o->prevani = a, o->aniframe = 0, o->timeframe = 0;
  const uint8_t *s = tab + ((const uint16_t *)(const void *)tab)[1 + a];
  int8_t interval = (int8_t)s[0];
  uint8_t d3 = 0;
  bool add = false;
  if (interval >= 0) {
    o->render = (uint8_t)((o->render & ~3) | (o->status & 1));
    if ((int8_t)--o->timeframe >= 0) return;
    o->timeframe = (uint8_t)interval;
    goto loadframe;
  }
  if ((int8_t)--o->timeframe >= 0) return;
  if (interval == -1) { /* walking or running */
    uint8_t d1 = 0, d0 = o->angle, d2 = o->status & 1;
    if (!d2) d0 = (uint8_t)~d0;
    d0 = (uint8_t)(d0 + 0x10);
    if ((int8_t)d0 < 0) d1 = 3;
    o->render = (uint8_t)((o->render & ~3) | (d2 ^ d1));
    if (o->status & 0x20) goto push;
    d0 = (uint8_t)((d0 >> 4) & 6);
    int16_t sp = o->inertia < 0 ? (int16_t)-o->inertia : o->inertia;
    if ((uint16_t)sp >= 0x600) s = tab + ((const uint16_t *)(const void *)tab)[1 + id_Run];
    else {
      s = tab + ((const uint16_t *)(const void *)tab)[1 + id_Walk];
      d0 = (uint8_t)(d0 + (d0 >> 1));
    }
    d0 = (uint8_t)(d0 + d0);
    d3 = d0;
    int16_t t = (int16_t)(-sp + 0x800);
    if (t < 0) t = 0;
    o->timeframe = (uint8_t)((uint16_t)t >> 8);
    add = true;
    goto loadframe;
  }
  if (interval == -2) { /* rolling */
    int16_t sp = o->inertia < 0 ? (int16_t)-o->inertia : o->inertia;
    s = tab + ((const uint16_t *)(const void *)tab)[1 + ((uint16_t)sp >= 0x600 ? id_Roll2 : id_Roll)];
    int16_t t = (int16_t)(-sp + 0x400);
    if (t < 0) t = 0;
    o->timeframe = (uint8_t)((uint16_t)t >> 8);
    o->render = (uint8_t)((o->render & ~3) | (o->status & 1));
    goto loadframe;
  }
push: {
  int16_t sp = o->inertia;
  if (sp >= 0) sp = (int16_t)-sp;
  int16_t t = (int16_t)(sp + 0x800);
  if (t < 0) t = 0;
  o->timeframe = (uint8_t)((uint16_t)t >> 6);
  s = tab + ((const uint16_t *)(const void *)tab)[1 + id_Push];
  o->render = (uint8_t)((o->render & ~3) | (o->status & 1));
}
loadframe: {
  uint8_t i = o->aniframe, f = s[1 + i];
  if (f & 0x80) {
    if (f == 0xFF) o->aniframe = 0, f = s[1];
    else if (f == 0xFE) {
      uint8_t back = s[2 + i];
      o->aniframe = (uint8_t)(o->aniframe - back);
      f = s[1 + (uint8_t)(i - back)];
    } else {
      if (f == 0xFD) o->anim = s[2 + i];
      return;
    }
  }
  o->frame = f;
  o->aniframe++;
  if (add) o->frame = (uint8_t)(o->frame + d3);
}
}

/* his art for the frame: the pattern load cues, applied to video memory at VBlank */
static uint16_t gfx_list[23];
static int gfx_n;
void sonic_dplc_apply(void) {
  const uint8_t *art = (const uint8_t *)DATA(D_TILES) + (uint32_t)DATAW(D_ARTS)[ART_Art_Sonic * 2] * 32;
  for (int i = 0; i < gfx_n; i++) vram[0x780 + i] = art + (uint32_t)gfx_list[i] * 32;
}

void Sonic_LoadGfx(Obj *o) {
  uint8_t f = o->frame;
  if (f == V.sonframenum) return;
  V.sonframenum = f;
  const uint8_t *d = DATA(D_SONICDPLC);
  const uint8_t *e = d + ((const uint16_t *)(const void *)d)[1 + f];
  int n = e[0];
  if (!n) return;
  V.f_sonframechg = 1;
  gfx_n = 0;
  const uint16_t *w = (const uint16_t *)(const void *)(e + 2);
  for (int i = 0; i < n; i++) {
    uint16_t t = w[i] & 0x0FFF, c = (uint16_t)((w[i] >> 12) + 1);
    for (int k = 0; k < c && gfx_n < 23; k++) gfx_list[gfx_n++] = (uint16_t)(t + k);
  }
}

/* ------------------------------------------------------------------ hurt and death */
static void Sonic_HurtStop(Obj *o) {
  if ((uint16_t)(V.limitbtm2 + 224) < (uint16_t)o->y.w.i) {
    KillSonic(o, o);
    return;
  }
  Sonic_Floor(o);
  if (o->status & 2) return;
  o->vy = o->vx = o->inertia = 0;
  o->anim = id_Walk;
  o->routine -= 2;
  OW(o, flashtime) = 2 * 60;
}

static void Sonic_Hurt(Obj *o) {
  SpeedToPos(o);
  o->vy += 0x38 - 8;
  if (o->status & 0x40) o->vy -= 0x38 - 0x18;
  Sonic_HurtStop(o);
  Sonic_LevelBound(o);
  Sonic_RecordPosition(o);
  Sonic_Animate(o);
  Sonic_LoadGfx(o);
  DisplaySprite(o);
}

static void Sonic_HandleDeath(Obj *o) {
  if ((uint16_t)(V.limitbtm2 + 0x100) >= (uint16_t)o->y.w.i) return;
  o->vy = -0x38;
  o->routine += 2;
  V.f_timecount = 0;
  V.f_lifecount++;
  if (--V.lives) {
    OW(o, restartime) = 60;
    if (!V.f_timeover) return;
    OW(o, restartime) = 0;
    OBJ(2)->id = OBJ(3)->id = id_GameOverCard;
    OBJ(2)->frame = 2, OBJ(3)->frame = 3;
  } else {
    OW(o, restartime) = 0;
    OBJ(2)->id = OBJ(3)->id = id_GameOverCard;
    OBJ(3)->frame = 1;
    V.f_timeover = 0;
  }
  AddPLC(plcid_GameOver);
}

static void Sonic_Death(Obj *o) {
  Sonic_HandleDeath(o);
  ObjectFall(o);
  Sonic_RecordPosition(o);
  Sonic_Animate(o);
  Sonic_LoadGfx(o);
  DisplaySprite(o);
}

static void Sonic_ResetLevel(Obj *o) {
  if (!OW(o, restartime)) return;
  if (--OW(o, restartime)) return;
  V.f_restart = 1;
}

/* ------------------------------------------------------------------ the object */
void SonicPlayer(Obj *o) {
  if (V.debuguse) {
    DebugMode(o);
    return;
  }
  switch (o->routine) {
  case 0:
    o->routine = 2;
    o->height = sonic_height, o->width = sonic_width;
    o->map = map_get(MAP_Sonic);
    o->gfx = ArtTile_Sonic;
    o->priority = 2;
    o->actwid = 24;
    o->render = 4;
    V.sonspeedmax = son_maxspeed, V.sonspeedacc = son_acceleration, V.sonspeeddec = son_deceleration;
    /* fall through */
  case 2:
    if (V.f_debugmode && (V.jpadpress1 & btnB)) {
      V.debuguse = 1;
      V.f_lockctrl = 0;
      return;
    }
    if (!V.f_lockctrl) V.jpadhold2 = V.jpadhold1, V.jpadpress2 = V.jpadpress1;
    if (!(V.f_playerctrl & 1)) {
      switch (o->status & 6) {
      case 0: Sonic_MdNormal(o); break;
      case 2: Sonic_MdJump(o); break;
      case 4: Sonic_MdRoll(o); break;
      default: Sonic_MdJump(o); break;
      }
    }
    Sonic_Display(o);
    Sonic_RecordPosition(o);
    Sonic_Water(o);
    OB(o, angleright) = V.anglebuffer;
    OB(o, angleleft) = V.anglebuffer2;
    if (V.f_wtunnelmode && !o->anim) o->anim = o->prevani;
    Sonic_Animate(o);
    if (!(V.f_playerctrl & 0x80)) ReactToItem(o);
    Sonic_Loops(o);
    Sonic_LoadGfx(o);
    break;
  case 4: Sonic_Hurt(o); break;
  case 6: Sonic_Death(o); break;
  case 8: Sonic_ResetLevel(o); break;
  }
}
