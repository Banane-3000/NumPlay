/* Star Light Zone's objects: elevators (and the vents that send them up one after another),
 * circling platforms, the staircase that drops when stood on, the pylons in front of everything,
 * fans that blow Sonic back, seesaws with their spiked balls; the Bomb badnik; and Dr. Robotnik's
 * machine that drops spiked balls on the seesaws (which Sonic catapults back at him). */
#include "s1.h"
#include "collide.h"
#include "game.h"
#include "objs.h"

/* ------------------------------------------------------------------ elevators */
#define elev_origY 0x30
#define elev_origX 0x32
#define elev_moveddistance 0x34
#define elev_acceleration 0x38
#define elev_slowingdown 0x3A
#define elev_half_distance 0x3C
#define elev_spawner_delay 0x3C
#define elev_spawner_delaybase 0x3E

static void Elev_Move(Obj *o) {
  int16_t d0 = OW(o, elev_acceleration);
  if (!OB(o, elev_slowingdown)) {
    if ((uint16_t)d0 < 0x800) d0 = (int16_t)(d0 + 0x10);
  } else if (d0) d0 = (int16_t)(d0 - 0x10);
  OW(o, elev_acceleration) = d0;
  OFIX(o, elev_moveddistance).l += (int32_t)d0 << 8;
  uint16_t m = (uint16_t)OFIX(o, elev_moveddistance).w.i, d2 = OUW(o, elev_half_distance);
  if (m > d2) OB(o, elev_slowingdown) = 1;
  if (m == (uint16_t)(d2 * 2)) obSubtype(o) = 0;
}

static void Elev_Types(Obj *o) {
  int16_t m;
  switch (obSubtype(o) & 0xF) {
  case 1:
  case 3:
  case 5:
  case 7:
    if (o->routine == 4) obSubtype(o)++;
    break;
  case 2:
    Elev_Move(o);
    o->y.w.i = (int16_t)(OW(o, elev_origY) - OFIX(o, elev_moveddistance).w.i);
    break;
  case 4:
    Elev_Move(o);
    o->y.w.i = (int16_t)(OW(o, elev_origY) + OFIX(o, elev_moveddistance).w.i);
    break;
  case 6:
    Elev_Move(o);
    m = OFIX(o, elev_moveddistance).w.i;
    o->y.w.i = (int16_t)(OW(o, elev_origY) - (m >> 1));
    o->x.w.i = (int16_t)(OW(o, elev_origX) + m);
    break;
  case 8:
    Elev_Move(o);
    m = OFIX(o, elev_moveddistance).w.i;
    o->y.w.i = (int16_t)(OW(o, elev_origY) + (m >> 1));
    o->x.w.i = (int16_t)(OW(o, elev_origX) - m);
    break;
  case 9:
    Elev_Move(o);
    o->y.w.i = (int16_t)(OW(o, elev_origY) - OFIX(o, elev_moveddistance).w.i);
    if (obSubtype(o)) break;
    if (o->status & status_on_object) {
      player->status |= status_in_air;
      player->status &= (uint8_t)~status_on_object;
      player->routine = 2;
    }
    DeleteObject(o);
    break;
  }
}

void Elevator(Obj *o) {
  /* (Elev_Var1 and Elev_Var2, one table: the first entries of the second are read through the first) */
  static const uint8_t var[32] = {80 / 2, 0, 16, 1, 32, 1, 52, 1, 16, 3, 32, 3, 52, 3, 20, 1,
                                  36, 1, 44, 1, 20, 3, 36, 3, 44, 3, 32, 5, 32, 7, 48, 9};
  switch (o->routine) {
  case 0: {
    o->routine += 2;
    uint8_t d0 = obSubtype(o);
    if (d0 & 0x80) {
      o->routine += 4;
      OW(o, elev_spawner_delay) = OW(o, elev_spawner_delaybase) = (int16_t)((d0 & 0x7F) * 6);
      return;
    }
    int k = (d0 >> 3) & 0x1E;
    o->actwid = var[k];
    o->frame = var[k + 1];
    int j = 2 + ((d0 * 2) & 0x1E);
    OW(o, elev_half_distance) = (int16_t)(var[j] << 2);
    obSubtype(o) = var[j + 1];
    o->map = map_get(MAP_Elev);
    o->gfx = ArtTile_Level | Tile_Pal3;
    o->render = sprite_cam_field;
    o->priority = 4;
    OW(o, elev_origX) = o->x.w.i;
    OW(o, elev_origY) = o->y.w.i;
  }
    /* fall through */
  case 2:
    PlatformObject(o, o->actwid);
    Elev_Types(o);
    break;
  case 4: {
    ExitPlatform(o, o->actwid);
    int16_t x = o->x.w.i;
    Elev_Types(o);
    if (!o->id) return;
    MvSonicOnPtfm2(o, x);
    break;
  }
  case 6:
    if (!--OW(o, elev_spawner_delay)) {
      OW(o, elev_spawner_delay) = OW(o, elev_spawner_delaybase);
      Obj *a1 = FindFreeObj();
      if (a1) {
        a1->id = id_Elevator;
        a1->x.w.i = o->x.w.i;
        a1->y.w.i = o->y.w.i;
        obSubtype(a1) = 0xE;
      }
    }
    if (obj_far(o)) DeleteObject(o);
    return;
  }
  if (!o->id) return;
  if (obj_far_x(OW(o, elev_origX))) {
    DeleteObject(o);
    return;
  }
  DisplaySprite(o);
}

/* ------------------------------------------------------------------ circling platforms */
#define circ_origY 0x30
#define circ_origX 0x32

static void Circ_Types(Obj *o) {
  int16_t d1 = (int8_t)(OSC_B(0x22) - 0x50), d2 = (int8_t)(OSC_B(0x26) - 0x50);
  if (obSubtype(o) & 1) d1 = (int16_t)-d1, d2 = (int16_t)-d2;
  if (obSubtype(o) & 2) {
    int16_t t = (int16_t)-d1;
    d1 = d2, d2 = t;
  }
  if (obSubtype(o) & 4) d1 = (int16_t)-d1;
  o->x.w.i = (int16_t)(d1 + OW(o, circ_origX));
  o->y.w.i = (int16_t)(d2 + OW(o, circ_origY));
}

void CirclingPlatform(Obj *o) {
  switch (o->routine) {
  case 0:
    o->routine += 2;
    o->map = map_get(MAP_Circ);
    o->gfx = ArtTile_Level | Tile_Pal3;
    o->render = sprite_cam_field;
    o->priority = 4;
    o->actwid = 48 / 2;
    OW(o, circ_origX) = o->x.w.i;
    OW(o, circ_origY) = o->y.w.i;
    /* fall through */
  case 2:
    PlatformObject(o, o->actwid);
    Circ_Types(o);
    break;
  case 4: {
    ExitPlatform(o, o->actwid);
    int16_t x = o->x.w.i;
    Circ_Types(o);
    MvSonicOnPtfm2(o, x);
    break;
  }
  }
  if (obj_far_x(OW(o, circ_origX))) {
    DeleteObject(o);
    return;
  }
  DisplaySprite(o);
}

/* ------------------------------------------------------------------ the staircase */
#define stair_origX 0x30
#define stair_origY 0x32
#define stair_delay 0x34
#define stair_touch 0x36
#define stair_parentYindex 0x37
#define stair_childrenY 0x38
#define stair_parent 0x3C

static void Stair_Types(Obj *o) {
  switch (obSubtype(o) & 7) {
  case 0:
    if (OW(o, stair_delay)) {
      if (!--OW(o, stair_delay)) obSubtype(o)++;
    } else if (OB(o, stair_touch) == 1) OW(o, stair_delay) = 30;
    break;
  case 2:
    if (!OW(o, stair_delay)) {
      if (OB(o, stair_touch) & 0x80) OW(o, stair_delay) = 60;
      break;
    }
    if (!--OW(o, stair_delay)) {
      obSubtype(o)++;
      break;
    }
    {
      uint8_t d0 = (uint8_t)((OUW(o, stair_delay) & 0xFF) >> 2 & 1);
      for (int i = 0; i < 4; i++, d0 ^= 1) OB(o, stair_childrenY + i) = d0;
    }
    break;
  case 1:
  case 3: {
    uint8_t *c = &OB(o, stair_childrenY);
    if (c[0] == 128) break;
    uint32_t v = ++c[0];
    c[1] = (uint8_t)((v * 3) >> 2);
    c[2] = (uint8_t)(v >> 1);
    c[3] = (uint8_t)(v >> 2);
    break;
  }
  }
}

void Staircase(Obj *o) {
  switch (o->routine) {
  case 0: {
    o->routine += 2;
    uint8_t d3 = stair_childrenY;
    int8_t d4 = 1;
    if (o->status & 1) d3 = stair_childrenY + 3, d4 = -1;
    int16_t d2 = o->x.w.i;
    Obj *a1 = o;
    for (int i = 0; i < 4; i++) {
      if (i) {
        a1 = FindNextFreeObj(o);
        if (!a1) break;
        a1->routine = 4;
      }
      a1->id = id_Staircase;
      a1->map = map_get(MAP_Stair);
      a1->gfx = ArtTile_Level | Tile_Pal3;
      a1->render = sprite_cam_field;
      a1->priority = 3;
      a1->actwid = 32 / 2;
      obSubtype(a1) = obSubtype(o);
      a1->x.w.i = d2;
      a1->y.w.i = o->y.w.i;
      OW(a1, stair_origX) = o->x.w.i;
      OW(a1, stair_origY) = a1->y.w.i;
      d2 = (int16_t)(d2 + 32);
      OB(a1, stair_parentYindex) = d3;
      OUW(a1, stair_parent) = OADDR(o);
      d3 = (uint8_t)(d3 + d4);
    }
  }
    /* fall through */
  case 2: Stair_Types(o);
    /* fall through */
  case 4: {
    Obj *a2 = OPTR(OUW(o, stair_parent));
    o->y.w.i = (int16_t)(a2->s.b[OB(o, stair_parentYindex) - 0x28] + OW(o, stair_origY));
    int d4 = SolidObject(o, (int16_t)(o->actwid + sonic_solid_width), 32 / 2, 34 / 2, o->x.w.i);
    if ((int8_t)d4 < 0) OB(a2, stair_touch) = (uint8_t)d4;
    if (o->status & status_on_object) OB(a2, stair_touch) = 1;
    break;
  }
  }
  if (obj_far_x(OW(o, stair_origX))) {
    DeleteObject(o);
    return;
  }
  DisplaySprite(o);
}

/* ------------------------------------------------------------------ pylons, fans */
void Pylon(Obj *o) {
  if (o->routine == 0) {
    o->routine += 2;
    o->map = map_get(MAP_Pylon);
    o->gfx = ArtTile_SLZ_Pylon | Tile_Prio;
    o->actwid = 32 / 2;
  }
  o->x.w.i = (int16_t)-(int16_t)((uint32_t)V.cam[0].x.l * 2 >> 16);
  obScreenY(o) = (int16_t)(-(int16_t)((uint32_t)V.cam[0].y.l * 2 >> 16 & 0x3F) + 0x100);
  DisplaySprite(o);
}

#define fan_time 0x30
#define fan_switch 0x32

void Fan(Obj *o) {
  if (o->routine == 0) {
    o->routine += 2;
    o->map = map_get(MAP_Fan);
    o->gfx = ArtTile_SLZ_Fan | Tile_Pal3;
    o->render |= sprite_cam_field;
    o->actwid = 32 / 2;
    o->priority = 4;
  }
  if (!(obSubtype(o) & 2) && --OW(o, fan_time) < 0) {
    OW(o, fan_time) = 2 * 60;
    uint8_t old = OB(o, fan_switch);
    OB(o, fan_switch) ^= 1;
    if (old & 1) OW(o, fan_time) = 3 * 60;
  }
  if (!OB(o, fan_switch)) {
    Obj *a1 = player;
    int16_t d0 = (int16_t)(a1->x.w.i - o->x.w.i);
    if (!(o->status & 1)) d0 = (int16_t)-d0;
    d0 = (int16_t)(d0 + 0x50);
    if ((uint16_t)d0 < 0xA0 + 0x50) {
      uint16_t d1 = (uint16_t)(a1->y.w.i + 0x60);
      if (d1 >= (uint16_t)o->y.w.i && (uint16_t)(d1 - o->y.w.i) < 0x10 + 0x60) {
        if ((uint16_t)d0 < 0x50) d0 = (int16_t)(~(d0 - 0x50) * 2);
        else d0 = (int16_t)(d0 - 0x50);
        d0 = (int16_t)(d0 + 0x60);
        if (!(o->status & 1)) d0 = (int16_t)-d0;
        d0 = (int16_t)((d0 & 0xFF00) | (uint8_t)-(uint8_t)d0);
        d0 = (int16_t)(d0 >> 4);
        if (obSubtype(o) & 1) d0 = (int16_t)-d0;
        a1->x.w.i = (int16_t)(a1->x.w.i + d0);
      }
    }
    if ((int8_t)--o->timeframe < 0) {
      o->timeframe = 0;
      if (++o->aniframe >= 3) o->aniframe = 0;
      o->frame = (uint8_t)(((obSubtype(o) & 1) ? 2 : 0) + o->aniframe);
    }
  }
  DisplaySprite(o);
  if (obj_far(o)) DeleteObject(o);
}

/* ------------------------------------------------------------------ seesaws */
#define see_origX 0x30
#define see_origY 0x34
#define see_landspeed 0x38
#define see_state 0x3A
#define see_parent 0x3C

static const uint8_t See_DataSlope[48] = {
  0x24, 0x24, 0x26, 0x28, 0x2A, 0x2C, 0x2A, 0x28, 0x26, 0x24, 0x23, 0x22, 0x21, 0x20, 0x1F, 0x1E,
  0x1D, 0x1C, 0x1B, 0x1A, 0x19, 0x18, 0x17, 0x16, 0x15, 0x14, 0x13, 0x12, 0x11, 0x10, 0x0F, 0x0E,
  0x0D, 0x0C, 0x0B, 0x0A, 0x09, 0x08, 0x07, 0x06, 0x05, 0x04, 0x03, 0x02, 0x02, 0x02, 0x02, 0x02,
};
static const uint8_t See_DataFlat[48] = {
  0x15, 0x15, 0x15, 0x15, 0x15, 0x15, 0x15, 0x15, 0x15, 0x15, 0x15, 0x15, 0x15, 0x15, 0x15, 0x15,
  0x15, 0x15, 0x15, 0x15, 0x15, 0x15, 0x15, 0x15, 0x15, 0x15, 0x15, 0x15, 0x15, 0x15, 0x15, 0x15,
  0x15, 0x15, 0x15, 0x15, 0x15, 0x15, 0x15, 0x15, 0x15, 0x15, 0x15, 0x15, 0x15, 0x15, 0x15, 0x15,
};
static const int16_t ball_yoffsets[5] = {-8, -28, -47, -28, -8};

static void See_ChgFrame(Obj *o, uint8_t d1) {
  uint8_t d0 = o->frame;
  if (d0 == d1) return;
  if (d0 < d1) d0 += 2;
  d0--;
  o->frame = d0;
  OB(o, see_state) = d1;
  o->render &= (uint8_t)~sprite_xflip;
  if (o->frame & 2) o->render |= sprite_xflip;
}

/* the seesaw's height at the ball's end; the ball's x from its middle */
static int16_t see_ball_y(Obj *ball, Obj *saw, int16_t origY) {
  int k = saw->frame;
  if ((uint16_t)ball->x.w.i < (uint16_t)OW(ball, see_origX)) k += 2;
  return (int16_t)(origY + ball_yoffsets[k < 5 ? k : 4]);
}

void Seesaw(Obj *o) {
  switch (o->routine) {
  case 0:
    o->routine += 2;
    o->map = map_get(MAP_Seesaw);
    o->gfx = ArtTile_SLZ_Seesaw;
    o->render |= sprite_cam_field;
    o->priority = 4;
    o->actwid = 96 / 2;
    OW(o, see_origX) = o->x.w.i;
    if (!obSubtype(o)) {
      Obj *a1 = FindNextFreeObj(o);
      if (a1) {
        a1->id = id_Seesaw;
        a1->routine += 6;
        a1->x.w.i = o->x.w.i;
        a1->y.w.i = o->y.w.i;
        a1->status = o->status;
        OUW(a1, see_parent) = OADDR(o);
      }
    }
    if (o->status & 1) o->frame = 2;
    OB(o, see_state) = o->frame;
    /* fall through */
  case 2:
    See_ChgFrame(o, OB(o, see_state));
    OW(o, see_landspeed) = player->vy;
    SlopeObject(o, 96 / 2, (o->frame & 1) ? See_DataFlat : See_DataSlope);
    break;
  case 4: {
    uint8_t d1 = 2;
    int16_t d0 = (int16_t)(o->x.w.i - player->x.w.i);
    if ((uint16_t)o->x.w.i < (uint16_t)player->x.w.i) d0 = (int16_t)-d0, d1 = 0;
    if ((uint16_t)d0 < 8) d1 = 1;
    See_ChgFrame(o, d1);
    const uint8_t *h = (o->frame & 1) ? See_DataFlat : See_DataSlope;
    ExitPlatform(o, 96 / 2);
    SlopeObject_AssumeStoodOn(o, 96 / 2, h, o->x.w.i);
    break;
  }
  case 6:
    o->routine += 2;
    o->map = map_get(MAP_SSawBall);
    o->gfx = ArtTile_SLZ_Spikeball;
    o->render |= sprite_cam_field;
    o->priority = 4;
    o->coltype = col_16x16 | col_hurt;
    o->actwid = 24 / 2;
    OW(o, see_origX) = o->x.w.i;
    o->x.w.i = (int16_t)(o->x.w.i + 40);
    OW(o, see_origY) = o->y.w.i;
    o->frame = 1;
    if (o->status & 1) {
      o->x.w.i = (int16_t)(o->x.w.i - 80);
      OB(o, see_state) = 2;
    }
    /* fall through */
  case 8: {
    Obj *a1 = OPTR(OUW(o, see_parent));
    uint8_t d0 = (uint8_t)(OB(o, see_state) - OB(a1, see_state));
    if (!d0) {
      int16_t d2 = 40;
      int k = a1->frame;
      if ((uint16_t)o->x.w.i < (uint16_t)OW(o, see_origX)) d2 = -40, k += 2;
      o->y.w.i = (int16_t)(OW(o, see_origY) + ball_yoffsets[k < 5 ? k : 4]);
      o->x.w.i = (int16_t)(OW(o, see_origX) + d2);
      o->y.w.f = o->x.w.f = 0;
      break;
    }
    if (d0 & 0x80) d0 = (uint8_t)-d0;
    int16_t d1 = -0x818, d2 = -0x114;
    if (d0 != 1) {
      d1 = -0xAF0, d2 = -0xCC;
      if (OW(a1, see_landspeed) >= 0xA00) d1 = -0xE00, d2 = -0xA0;
    }
    o->vy = d1;
    o->vx = d2;
    if ((uint16_t)o->x.w.i < (uint16_t)OW(o, see_origX)) o->vx = (int16_t)-o->vx;
    o->routine += 2;
  }
    /* fall through */
  case 0xA:
    if (o->vy < 0) {
      ObjectFall(o);
      if ((int16_t)(OW(o, see_origY) - 47) <= o->y.w.i) ObjectFall(o);
      break;
    }
    ObjectFall(o);
    {
      Obj *a1 = OPTR(OUW(o, see_parent));
      if (see_ball_y(o, a1, OW(o, see_origY)) > o->y.w.i) break;
      uint8_t d1 = o->vx < 0 ? 2 : 0;
      OB(a1, see_state) = d1;
      OB(o, see_state) = d1;
      if (d1 != a1->frame && (a1->status & status_on_object)) {
        a1->status &= (uint8_t)~status_on_object;
        a1->routine2 = 0;
        a1->routine = 2;
        Obj *s = player;
        s->vy = (int16_t)-o->vy;
        s->status |= status_in_air;
        s->status &= (uint8_t)~status_on_object;
        OB(s, jumping) = 0;
        s->anim = id_Spring;
        s->routine = 2;
      }
      o->vx = o->vy = 0;
      o->routine -= 2;
    }
    break;
  }
  uint16_t d = (uint16_t)((OW(o, see_origX) & 0xFF80) - ((V.cam[0].x.w.i - 128) & 0xFF80));
  if (d > 128 + 320 + 192) {
    DeleteObject(o);
    return;
  }
  DisplaySprite(o);
}

/* ------------------------------------------------------------------ Bomb */
#define bom_time 0x30
#define bom_origY 0x34
#define bom_parent 0x3C

static void Bom_CheckStartFuse(Obj *o) {
  int16_t dx = (int16_t)(player->x.w.i - o->x.w.i), dy = (int16_t)(player->y.w.i - o->y.w.i);
  if ((uint16_t)player->x.w.i < (uint16_t)o->x.w.i) dx = (int16_t)-dx;
  if ((uint16_t)player->y.w.i < (uint16_t)o->y.w.i) dy = (int16_t)-dy;
  if ((uint16_t)dx >= 96 || (uint16_t)dy >= 96 || V.debuguse) return;
  o->routine2 = 4;
  OW(o, bom_time) = 2 * 60 + 24 - 1;
  o->vx = 0;
  o->anim = 2;
  Obj *a1 = FindNextFreeObj(o);
  if (!a1) return;
  a1->id = id_Bomb;
  a1->x.w.i = o->x.w.i;
  a1->y.w.i = o->y.w.i;
  OW(a1, bom_origY) = o->y.w.i;
  a1->status = o->status;
  obSubtype(a1) = 4;
  a1->anim = 3;
  a1->vy = 0x10;
  if (o->status & 2) a1->vy = -0x10;
  OW(a1, bom_time) = 2 * 60 + 24 - 1;
  OUW(a1, bom_parent) = OADDR(o);
}

void Bomb(Obj *o) {
  static const int16_t speeds[4][2] = {{-0x200, -0x300}, {-0x100, -0x200}, {0x200, -0x300}, {0x100, -0x200}};
  switch (o->routine) {
  case 0:
    o->routine += 2;
    o->map = map_get(MAP_Bomb);
    o->gfx = ArtTile_Bomb;
    o->render |= sprite_cam_field;
    o->priority = 3;
    o->actwid = 24 / 2;
    if (obSubtype(o)) {
      o->routine = obSubtype(o);
      return;
    }
    o->coltype = col_24x24 | col_hurt;
    o->status ^= 1;
    /* fall through */
  case 2:
    switch (o->routine2) {
    case 0:
      Bom_CheckStartFuse(o);
      if (--OW(o, bom_time) >= 0) break;
      o->routine2 += 2;
      OW(o, bom_time) = 25 * 60 + 36 - 1;
      o->vx = 0x10;
      o->anim = 1;
      o->status ^= 1;
      if (!(o->status & 1)) o->vx = -0x10;
      break;
    case 2:
      Bom_CheckStartFuse(o);
      if (--OW(o, bom_time) < 0) {
        o->routine2 -= 2;
        OW(o, bom_time) = 3 * 60 - 1;
        o->vx = 0;
        o->anim = 0;
      } else SpeedToPos(o);
      break;
    case 4:
      if (--OW(o, bom_time) < 0) {
        o->id = id_Explosion;
        o->routine = 0;
      }
      break;
    }
    AnimateSprite(o, ani_get(ANI_Bomb));
    RememberState(o);
    return;
  case 4:
    if (--OW(o, bom_time) >= 0) {
      SpeedToPos(o);
      AnimateSprite(o, ani_get(ANI_Bomb));
      RememberState(o);
      return;
    }
    OW(o, bom_time) = 0;
    o->routine = 0;
    o->y.w.i = OW(o, bom_origY);
    for (int i = 0; i < 4; i++) {
      Obj *a1 = i ? FindNextFreeObj(o) : o;
      if (!a1) continue;
      a1->id = id_Bomb;
      a1->x.w.i = o->x.w.i;
      a1->y.w.i = o->y.w.i;
      obSubtype(a1) = 6;
      a1->anim = 4;
      a1->vx = speeds[i][0];
      a1->vy = speeds[i][1];
      a1->coltype = col_8x8 | col_hurt;
      a1->render |= sprite_rendered;
    }
    o->routine = 6;
    /* fall through */
  case 6:
    SpeedToPos(o);
    o->vy = (int16_t)(o->vy + 0x18);
    AnimateSprite(o, ani_get(ANI_Bomb));
    if (!(o->render & 0x80)) {
      DeleteObject(o);
      return;
    }
    DisplaySprite(o);
    return;
  }
}

/* ------------------------------------------------------------------ the boss */
#define BossStarLight_SeesawList 0x2A
#define BossStarLight_ParentObj 0x36
#define BossStarLight_SeesawSide 0x3A
#define BossStarLight_GenericTimer 0x3C
#define BossStarLight_SineCounter 0x3F
#define BossSpikeball_SeesawY 0x34
#define BossSpikeball_SeesawPtr 0x3C

/* the spiked ball's subtype is a word (the subtype and the byte after it) */
static uint16_t sbw(const Obj *o) { return (uint16_t)(o->s.b[0] << 8 | o->s.b[1]); }
static void set_sbw(Obj *o, uint16_t v) { o->s.b[0] = (uint8_t)(v >> 8), o->s.b[1] = (uint8_t)v; }

static void BSLZ_StatusUpdate(Obj *o) {
  if (o->routine2 >= 6) return;
  if (o->status & 0x80) {
    AddPoints(100);
    o->routine2 = 6;
    OB(o, BossStarLight_GenericTimer) = 120;
    o->vx = 0;
    return;
  }
  if (o->coltype) return;
  boss_hit_flash(o, col_48x48 | col_boss);
}

static void BSLZ_ShipUpdate(Obj *o) {
  BossMove(o);
  uint8_t s = OB(o, BossStarLight_SineCounter);
  OB(o, BossStarLight_SineCounter) += 2;
  o->y.w.i = (int16_t)((CalcSine(s, NULL) >> 6) + obBossY(o).w.i);
  o->x.w.i = obBossX(o).w.i;
  BSLZ_StatusUpdate(o);
}

static void BSLZ_MoveUpdate(Obj *o) {
  BossMove(o);
  o->y.w.i = obBossY(o).w.i;
  o->x.w.i = obBossX(o).w.i;
  BSLZ_StatusUpdate(o);
}

static void BSLZ_ShipMove(Obj *o) {
  int16_t d0 = obBossX(o).w.i;
  o->vx = 0x200;
  bool flip;
  if (!(o->status & 1)) {
    o->vx = -0x200;
    flip = d0 <= boss_slz_x + 8;
  } else flip = d0 >= boss_slz_x + 0x138;
  if (flip) o->status ^= 1;
  int16_t x = o->x.w.i, d4 = 0x28;
  if (o->vx < 0) d4 = -0x28;
  int d2;
  for (d2 = 2; d2 >= 0; d2--) {
    uint16_t a = OUW(o, BossStarLight_SeesawList + (2 - d2) * 2);
    if (!a) continue;
    Obj *a3 = OPTR(a);
    if (a3->status & status_on_object) continue;
    if ((int16_t)(a3->x.w.i + d4 - x) == 0) break;
  }
  obSubtype(o) = (uint8_t)d2;
  if (d2 >= 0) {
    o->routine2 += 2;
    OB(o, BossStarLight_GenericTimer) = 40;
  }
  BSLZ_ShipUpdate(o);
}

static void BSLZ_MakeBall(Obj *o) {
  if (OB(o, BossStarLight_GenericTimer) == 40) {
    int8_t d0 = (int8_t)obSubtype(o);
    if (d0 < 0) goto abort;
    uint16_t a = OUW(o, BossStarLight_SeesawList + (2 - d0) * 2);
    for (int i = 1; i < 64; i++)
      if (OUW(&objs[i], BossSpikeball_SeesawPtr) == 0xFFFF && OUW(&objs[i], BossSpikeball_SeesawPtr + 2) == a)
        goto abort;
    if (!a) goto abort;
    Obj *a2 = OPTR(a);
    Obj *a1 = FindNextFreeObj(a2);
    if (!a1) goto abort;
    a1->id = id_BossSpikeball;
    a1->x.w.i = o->x.w.i;
    a1->y.w.i = (int16_t)(o->y.w.i + 0x20);
    a1->status = a2->status;
    OUW(a1, BossSpikeball_SeesawPtr) = 0xFFFF;
    OUW(a1, BossSpikeball_SeesawPtr + 2) = a;
  }
  if (!--OB(o, BossStarLight_GenericTimer)) goto abort;
  BSLZ_StatusUpdate(o);
  return;
abort:
  o->routine2 -= 2;
  BSLZ_ShipUpdate(o);
}

void BossStarLight(Obj *o) {
  switch (o->routine) {
  case 0: {
    static const uint8_t data[4][3] = {{2, 0, 4}, {4, 1, 4}, {6, 7, 4}, {8, 0, 3}};
    o->x.w.i = boss_slz_x + 0x188;
    o->y.w.i = boss_slz_y + 0x18;
    obBossX(o).w.i = o->x.w.i;
    obBossY(o).w.i = o->y.w.i;
    o->coltype = col_48x48 | col_boss;
    obBossHits(o) = 8;
    Obj *a1 = o;
    for (int i = 0; i < 4; i++) {
      if (i) {
        a1 = FindNextFreeObj(o);
        if (!a1) break;
        a1->id = id_BossStarLight;
        a1->x.w.i = o->x.w.i;
        a1->y.w.i = o->y.w.i;
      }
      o->status &= (uint8_t)~1;
      a1->routine2 = 0;
      a1->routine = data[i][0];
      a1->anim = data[i][1];
      a1->priority = data[i][2];
      a1->map = map_get(MAP_Eggman);
      a1->gfx = ArtTile_Eggman;
      a1->render = sprite_cam_field;
      a1->actwid = 64 / 2;
      OUW(a1, BossStarLight_ParentObj) = OADDR(o);
    }
    int k = 0;
    for (int i = 1; i < 64; i++)
      if (objs[i].id == id_Seesaw && obSubtype(&objs[i]) && k < 3)
        OUW(o, BossStarLight_SeesawList + 2 * k++) = OADDR(&objs[i]);
  }
    /* fall through */
  case 2:
    switch (o->routine2) {
    case 0:
      o->vx = -0x100;
      if ((uint16_t)obBossX(o).w.i < boss_slz_x + 0x120) o->routine2 += 2;
      BSLZ_ShipUpdate(o);
      break;
    case 2: BSLZ_ShipMove(o); break;
    case 4: BSLZ_MakeBall(o); break;
    case 6:
      if ((int8_t)--OB(o, BossStarLight_GenericTimer) >= 0) {
        BossDefeated(o);
        break;
      }
      o->routine2 += 2;
      o->vy = 0;
      o->status |= 1;
      o->status &= 0x7F;
      o->vx = 0;
      OB(o, BossStarLight_GenericTimer) = (uint8_t)-24;
      if (!V.bossstatus) V.bossstatus = 1;
      BSLZ_StatusUpdate(o);
      break;
    case 8: {
      int8_t t = (int8_t)++OB(o, BossStarLight_GenericTimer);
      if (t == 0) o->vy = 0;
      else if (t < 0) o->vy = (int16_t)(o->vy + 0x18);
      else if ((uint8_t)t < 32) o->vy = (int16_t)(o->vy - 8);
      else if (t == 32) o->vy = 0;
      else if ((uint8_t)t >= 42) o->routine2 += 2;
      BSLZ_MoveUpdate(o);
      break;
    }
    case 0xA:
      o->vx = 0x400;
      o->vy = -0x40;
      if ((uint16_t)V.limitright2 >= boss_slz_end) {
        if (!(o->render & 0x80)) {
          DeleteObject(o);
          return;
        }
      } else V.limitright2 += 2;
      BossMove(o);
      BSLZ_ShipUpdate(o);
      break;
    }
    AnimateSprite(o, ani_get(ANI_Eggman));
    o->render = (uint8_t)((o->render & ~3) | (o->status & 3));
    DisplaySprite(o);
    return;
  case 4: {
    Obj *a1 = OPTR(OUW(o, BossStarLight_ParentObj));
    uint8_t d0 = a1->routine2, d1 = 1;
    if ((int8_t)(d0 - 6) >= 0) d1 = 0xA;
    else if (!a1->coltype) d1 = 5;
    else if (player->routine >= 4) d1 = 4;
    o->anim = d1;
    if (d0 == 0xA) {
      o->anim = 6;
      if (!(o->render & 0x80)) {
        DeleteObject(o);
        return;
      }
    }
    break;
  }
  case 6: {
    Obj *a1 = OPTR(OUW(o, BossStarLight_ParentObj));
    o->anim = 8;
    if (a1->routine2 == 0xA) {
      if (!(o->render & 0x80)) {
        DeleteObject(o);
        return;
      }
      o->anim = 0xB;
    } else if ((int8_t)a1->routine2 <= 8 && (int8_t)a1->routine2 >= 4) o->anim = 7;
    break;
  }
  case 8: {
    Obj *a1 = OPTR(OUW(o, BossStarLight_ParentObj));
    if (a1->routine2 == 0xA && !(o->render & 0x80)) {
      DeleteObject(o);
      return;
    }
    o->map = map_get(MAP_BossItems);
    o->gfx = ArtTile_Eggman_Weapons | Tile_Pal2;
    o->frame = 3;
    goto display;
  }
  }
  AnimateSprite(o, ani_get(ANI_Eggman));
display: {
  Obj *a1 = OPTR(OUW(o, BossStarLight_ParentObj));
  o->x.w.i = a1->x.w.i;
  o->y.w.i = a1->y.w.i;
  o->status = a1->status;
  o->render = (uint8_t)((o->render & ~3) | (o->status & 3));
  DisplaySprite(o);
}
}

static void BossSpikeball_CheckFlicker(Obj *o) {
  if (sbw(o) == 120) o->delayani = 5;
  if (sbw(o) == 60) o->delayani = 2;
  if ((int8_t)--o->timeframe > 0) return;
  o->frame ^= 1;
  o->timeframe = o->delayani;
}

static Obj *spikeball_saw(Obj *o) { return OPTR(OUW(o, BossSpikeball_SeesawPtr + 2)); }

/* the ball on the seesaw at the end it fell on: Sonic thrown up if he stood on the other end */
static void BossSpikeball_LaunchSonic(Obj *o, Obj *a1, uint8_t d1) {
  OB(a1, BossStarLight_SeesawSide) = d1;
  OB(o, BossStarLight_SeesawSide) = d1;
  if (d1 != a1->frame && (a1->status & status_on_object)) {
    a1->status &= (uint8_t)~status_on_object;
    a1->routine2 = 0;
    a1->routine = 2;
    Obj *s = player;
    s->vy = (int16_t)-o->vy;
    if (a1->frame == 1) s->vy = (int16_t)(s->vy >> 1);
    s->status |= status_in_air;
    s->status &= (uint8_t)~status_on_object;
    OB(s, jumping) = 0;
    Sonic_ChkRoll(s);
    s->routine = 2;
  }
  o->vx = o->vy = 0;
  o->routine += 2;
  BossSpikeball_CheckFlicker(o);
}

static bool spikeball_landed(Obj *o, Obj *a1) {
  int k = a1->frame;
  if ((uint16_t)o->x.w.i < (uint16_t)obBossX(o).w.i) k += 2;
  int16_t d1 = (int16_t)(OW(o, BossSpikeball_SeesawY) + ball_yoffsets[k < 5 ? k : 4]);
  return d1 <= o->y.w.i;
}

void BossSpikeball(Obj *o) {
  static const int8_t bosshit[4] = {-0x18, 0x18 + 0x18, -0x18, 0x18 + 0x18};
  static const int8_t ballhit[4] = {8, -8 - 8, 8, -8 - 8};
  switch (o->routine) {
  case 0: {
    o->map = map_get(MAP_SSawBall);
    o->gfx = ArtTile_Eggman_Spikeball;
    o->frame = 1;
    o->render |= sprite_cam_field;
    o->priority = 4;
    o->coltype = col_16x16 | col_hurt;
    o->actwid = 24 / 2;
    Obj *a1 = spikeball_saw(o);
    obBossX(o).w.i = a1->x.w.i;
    OW(o, BossSpikeball_SeesawY) = a1->y.w.i;
    o->status |= 1;
    if (o->x.w.i <= a1->x.w.i) {
      o->status &= (uint8_t)~1;
      OB(o, BossStarLight_SeesawSide) = 2;
    }
    o->routine += 2;
  }
    /* fall through */
  case 2: {
    ObjectFall(o);
    Obj *a1 = spikeball_saw(o);
    if (!spikeball_landed(o, a1)) break;
    uint8_t d1 = (o->status & 1) ? 0 : 2;
    set_sbw(o, 240);
    o->delayani = 10;
    o->timeframe = o->delayani;
    BossSpikeball_LaunchSonic(o, a1, d1);
    break;
  }
  case 4: {
    Obj *a1 = spikeball_saw(o);
    uint8_t d0 = (uint8_t)(OB(o, BossStarLight_SeesawSide) - OB(a1, BossStarLight_SeesawSide));
    if (!d0) {
      int k = a1->frame;
      int16_t d2 = 40;
      if ((uint16_t)o->x.w.i < (uint16_t)obBossX(o).w.i) d2 = -40, k += 2;
      o->y.w.i = (int16_t)(OW(o, BossSpikeball_SeesawY) + ball_yoffsets[k < 5 ? k : 4]);
      o->x.w.i = (int16_t)(obBossX(o).w.i + d2);
      o->y.w.f = o->x.w.f = 0;
      set_sbw(o, (uint16_t)(sbw(o) - 1));
      if (!sbw(o)) {
        set_sbw(o, 32);
        o->routine = 8;
        break;
      }
      BossSpikeball_CheckFlicker(o);
      break;
    }
    if (d0 & 0x80) d0 = (uint8_t)-d0;
    int16_t d1 = -0x818, d2 = -0x114;
    if (d0 != 1) {
      d1 = -0x960, d2 = -0xF4;
      if (OW(a1, 0x38) >= 0x9C0) d1 = -0xA20, d2 = -0x80;
    }
    o->vy = d1;
    o->vx = d2;
    if ((uint16_t)o->x.w.i < (uint16_t)obBossX(o).w.i) o->vx = (int16_t)-o->vx;
    o->frame = 1;
    set_sbw(o, 32);
    o->routine += 2;
  }
    /* fall through */
  case 6: {
    Obj *boss = NULL;
    for (int i = 1; i < 64; i++)
      if (objs[i].id == id_BossStarLight) {
        boss = &objs[i];
        break;
      }
    if (boss) {
      uint16_t d0 = (uint16_t)(boss->x.w.i + bosshit[0]), d2 = (uint16_t)(o->x.w.i + ballhit[0]);
      uint16_t d1 = (uint16_t)(boss->y.w.i + bosshit[2]), d3 = (uint16_t)(o->y.w.i + ballhit[2]);
      if (d2 >= d0 && (uint16_t)(d0 + bosshit[1]) >= (uint16_t)(d2 + ballhit[1]) && d3 >= d1 &&
          (uint16_t)(d1 + bosshit[3]) >= (uint16_t)(d3 + ballhit[3])) {
        o->routine += 2;
        set_sbw(o, 0);
        boss->coltype = 0;
        if (!--obBossHits(boss)) {
          boss->status |= 0x80;
          o->vx = o->vy = 0;
        }
      }
    }
    if (o->vy < 0) {
      ObjectFall(o);
      if ((int16_t)(OW(o, BossSpikeball_SeesawY) - 0x2F) <= o->y.w.i) ObjectFall(o);
      BossSpikeball_CheckFlicker(o);
      break;
    }
    ObjectFall(o);
    Obj *a1 = spikeball_saw(o);
    if (!spikeball_landed(o, a1)) {
      BossSpikeball_CheckFlicker(o);
      break;
    }
    uint8_t d1 = o->vx < 0 ? 2 : 0;
    set_sbw(o, 0);
    BossSpikeball_LaunchSonic(o, a1, d1);
    break;
  }
  case 8: {
    static const int16_t speeds[4][2] = {{-0x100, -0x340}, {-0xA0, -0x240}, {0x100, -0x340}, {0xA0, -0x240}};
    o->id = id_Explosion;
    o->routine = 0;
    if (sbw(o) != 32) break;
    o->y.w.i = OW(o, BossSpikeball_SeesawY);
    for (int i = 0; i < 4; i++) {
      Obj *a1 = FindFreeObj();
      if (!a1) continue;
      a1->id = id_BossSpikeball;
      a1->routine = 0xA;
      a1->map = map_get(MAP_BSBall);
      a1->priority = 3;
      a1->gfx = ArtTile_Eggman_Spikeball;
      a1->x.w.i = o->x.w.i;
      a1->y.w.i = o->y.w.i;
      a1->vx = speeds[i][0];
      a1->vy = speeds[i][1];
      a1->coltype = col_8x8 | col_hurt;
      a1->render |= sprite_cam_field | sprite_rendered;
      a1->actwid = 24 / 2;
    }
    break;
  }
  case 0xA:
    SpeedToPos(o);
    obBossX(o).w.i = o->x.w.i;
    OW(o, BossSpikeball_SeesawY) = o->y.w.i;
    o->vy = (int16_t)(o->vy + 0x18);
    o->frame = (uint8_t)((V.vblank_count & 4) >> 2);
    if (!(o->render & 0x80)) {
      DeleteObject(o);
      return;
    }
    break;
  }
  if (obj_far_x(obBossX(o).w.i)) {
    DeleteObject(o);
    return;
  }
  DisplaySprite(o);
}
