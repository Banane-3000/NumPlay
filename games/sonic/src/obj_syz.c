/* Spring Yard Zone's objects: the spinning lights, bumpers, floating blocks (Star Light's stairs
 * and Labyrinth's doors are the same object), spiked balls swinging on chains (Labyrinth's too),
 * the big spiked ball; the badniks Roller and Yadrin; and Dr. Robotnik's machine that pulls the
 * floor out from under Sonic, block by block. */
#include "s1.h"
#include "collide.h"
#include "game.h"
#include "objs.h"

extern int obj_d7;

void SpinningLight(Obj *o) {
  if (o->routine == 0) {
    o->routine += 2;
    o->map = map_get(MAP_Light);
    o->gfx = ArtTile_Level;
    o->render = sprite_cam_field;
    o->actwid = 32 / 2;
    o->priority = 6;
  }
  if ((int8_t)--o->timeframe < 0) {
    o->timeframe = 8 - 1;
    if (++o->frame >= 6) o->frame = 0;
  }
  if (obj_far(o)) {
    DeleteObject(o);
    return;
  }
  DisplaySprite(o);
}

/* ------------------------------------------------------------------ Roller */
#define roll_waitunfolded 0x30
#define roll_stateflags 0x32

void Roller(Obj *o) {
  if (o->routine == 0) {
    o->height = 28 / 2;
    o->width = 16 / 2;
    ObjectFall(o);
    int16_t f = ObjFloorDist(o, NULL);
    if (f >= 0) return;
    o->y.w.i = (int16_t)(o->y.w.i + f);
    o->vy = 0;
    o->routine += 2;
    o->map = map_get(MAP_Roll);
    o->gfx = ArtTile_Roller;
    o->render = sprite_cam_field;
    o->priority = 4;
    o->actwid = 32 / 2;
    return;
  }
  switch (o->routine2) {
  case 0: {
    uint16_t px = (uint16_t)player->x.w.i;
    if (px >= 256 && (uint16_t)(px - 256) >= (uint16_t)o->x.w.i) {
      o->routine2 += 4;
      o->anim = 2;
      o->vx = 0x700;
      o->coltype = col_28x28 | col_hurt;
    }
    return; /* (not shown until it rolls) */
  }
  case 2:
    if (o->anim == 2) o->routine2 += 2;
    else if (--OW(o, roll_waitunfolded) < 0) {
      o->anim = 1;
      o->vx = 0x700;
      o->coltype = col_28x28 | col_hurt;
    }
    break;
  case 4: {
    if (!(OB(o, roll_stateflags) & 0x80) && (uint16_t)(player->x.w.i - 48) < (uint16_t)o->x.w.i) {
      o->anim = 0;
      o->coltype = col_28x28 | col_badnik;
      o->vx = 0;
      OW(o, roll_waitunfolded) = 2 * 60;
      o->routine2 = 2;
      OB(o, roll_stateflags) |= 0x80;
    }
    SpeedToPos(o);
    int16_t f = ObjFloorDist(o, NULL);
    if (f < -8 || f >= 0xC) {
      o->routine2 += 2;
      uint8_t old = OB(o, roll_stateflags);
      OB(o, roll_stateflags) |= 1;
      if (old & 1) o->vy = -0x600;
    } else o->y.w.i = (int16_t)(o->y.w.i + f);
    break;
  }
  case 6: {
    ObjectFall(o);
    if (o->vy < 0) break;
    int16_t f = ObjFloorDist(o, NULL);
    if (f >= 0) break;
    o->y.w.i = (int16_t)(o->y.w.i + f);
    o->routine2 -= 2;
    o->vy = 0;
    break;
  }
  }
  AnimateSprite(o, ani_get(ANI_Roll));
  int16_t d = (int16_t)((o->x.w.i & 0xFF80) - ((V.cam[0].x.w.i - 0x80) & 0xFF80));
  if (d > 0x280) {
    if (o->respawn) V.objstate[2 + o->respawn] &= 0x7F;
    DeleteObject(o);
    return;
  }
  DisplaySprite(o);
}

/* ------------------------------------------------------------------ bumpers */
#define jumping 0x3C

void Bumper(Obj *o) {
  if (o->routine == 0) {
    o->routine += 2;
    o->map = map_get(MAP_Bump);
    o->gfx = ArtTile_SYZ_Bumper;
    o->render = sprite_cam_field;
    o->actwid = 32 / 2;
    o->priority = 1;
    o->coltype = col_16x16_alt | col_special;
  }
  if (o->colprop) {
    o->colprop = 0;
    Obj *a1 = player;
    int16_t cos, sin = CalcSine(CalcAngle((int16_t)(o->x.w.i - a1->x.w.i), (int16_t)(o->y.w.i - a1->y.w.i)), &cos);
    a1->vx = (int16_t)((int32_t)cos * -0x700 >> 8);
    a1->vy = (int16_t)((int32_t)sin * -0x700 >> 8);
    a1->status |= status_in_air;
    a1->status &= (uint8_t)~(status_rolljumping | status_pushing);
    OB(a1, jumping) = 0;
    o->anim = 1;
    bool points = true;
    if (o->respawn) {
      uint8_t *st = &V.objstate[2 + o->respawn];
      if (*st >= 10 + 0x80) points = false;
      else (*st)++;
    }
    if (points) {
      AddPoints(1);
      Obj *p = FindFreeObj();
      if (p) {
        p->id = id_Points;
        p->x.w.i = o->x.w.i;
        p->y.w.i = o->y.w.i;
        p->frame = 4;
      }
    }
  }
  AnimateSprite(o, ani_get(ANI_Bump));
  if (obj_far(o)) {
    if (o->respawn) V.objstate[2 + o->respawn] &= 0x7F;
    DeleteObject(o);
    return;
  }
  DisplaySprite(o);
}

/* ------------------------------------------------------------------ Yadrin */
#define yad_timedelay 0x30

/* a wall ahead, checked every fourth frame */
static bool ChkHitLeftRightWall(Obj *o) {
  if ((V.framecount + obj_d7) & 3) return false;
  if (o->vx >= 0) return ObjHitWallRight(o, o->actwid, NULL) < 0;
  return ObjHitWallLeft(o, (int16_t)~o->actwid, NULL) < 0;
}

void Yadrin(Obj *o) {
  if (o->routine == 0) {
    o->map = map_get(MAP_Yad);
    o->gfx = ArtTile_Yadrin | Tile_Pal2;
    o->render = sprite_cam_field;
    o->priority = 4;
    o->actwid = 40 / 2;
    o->height = 34 / 2;
    o->width = 16 / 2;
    o->coltype = col_40x32 | col_special;
    ObjectFall(o);
    int16_t f = ObjFloorDist(o, NULL);
    if (f >= 0) return;
    o->y.w.i = (int16_t)(o->y.w.i + f);
    o->vy = 0;
    o->routine += 2;
    o->status ^= 1;
    return;
  }
  if (o->routine2 == 0) {
    if (--OW(o, yad_timedelay) < 0) {
      o->routine2 += 2;
      o->vx = -0x100;
      o->anim = 1;
      o->status ^= 1;
      if (o->status & 1) o->vx = 0x100;
    }
  } else {
    SpeedToPos(o);
    int16_t f = ObjFloorDist(o, NULL);
    bool stop = f < -8 || f >= 0xC;
    if (!stop) {
      o->y.w.i = (int16_t)(o->y.w.i + f);
      stop = ChkHitLeftRightWall(o);
    }
    if (stop) {
      o->routine2 -= 2;
      OW(o, yad_timedelay) = 60 - 1;
      o->vx = 0;
      o->anim = 0;
    }
  }
  AnimateSprite(o, ani_get(ANI_Yad));
  RememberState(o);
}

/* ------------------------------------------------------------------ floating blocks, Labyrinth's doors */
#define fb_origY 0x30
#define fb_origX 0x34
#define fb_moving 0x38
#define fb_distance 0x3A
#define fb_switch 0x3C

static void fb_door_y(Obj *o) {
  int16_t d0 = OW(o, fb_distance);
  if (o->status & 1) d0 = (int16_t)-d0;
  o->y.w.i = (int16_t)(OW(o, fb_origY) + d0);
}
static void fb_door_x(Obj *o) {
  int16_t d0 = OW(o, fb_distance);
  if (o->status & 1) d0 = (int16_t)(-d0 + 128);
  o->x.w.i = (int16_t)(OW(o, fb_origX) + d0);
}

/* a door opening (to distance 0) or closing (to its full distance), then the next state */
static void fb_door(Obj *o, bool open, int16_t full, void (*place)(Obj *)) {
  if (!OB(o, fb_moving)) {
    uint8_t s = V.f_switch[OB(o, fb_switch)];
    if (open ? !(s & 1) : !(s & 0x80)) {
      place(o);
      return;
    }
    OB(o, fb_moving) = 1;
  }
  if (open ? OW(o, fb_distance) != 0 : OW(o, fb_distance) != full) {
    OW(o, fb_distance) = (int16_t)(OW(o, fb_distance) + (open ? -2 : 2));
    place(o);
    return;
  }
  if (open) obSubtype(o)++;
  else obSubtype(o)--;
  OB(o, fb_moving) = 0;
  if (o->respawn) {
    if (open) V.objstate[2 + o->respawn] |= 1;
    else V.objstate[2 + o->respawn] &= (uint8_t)~1;
  }
  place(o);
}

static void fb_stair(Obj *o, int16_t d1, int16_t d0, uint16_t d3) {
  if (!d3) {
    o->status++;
    o->status &= 3;
  }
  int d2 = o->status & 3;
  switch (d2) {
  case 0:
    o->x.w.i = (int16_t)(d0 - d1 + OW(o, fb_origX));
    o->y.w.i = (int16_t)(-d1 + OW(o, fb_origY));
    break;
  case 1:
    o->y.w.i = (int16_t)(-(d0 - (d1 - 1)) + OW(o, fb_origY));
    o->x.w.i = (int16_t)(d1 + OW(o, fb_origX));
    break;
  case 2:
    o->x.w.i = (int16_t)(-(d0 - (d1 - 1)) + OW(o, fb_origX));
    o->y.w.i = (int16_t)(d1 + OW(o, fb_origY));
    break;
  default:
    o->y.w.i = (int16_t)(d0 - d1 + OW(o, fb_origY));
    o->x.w.i = (int16_t)(-d1 + OW(o, fb_origX));
    break;
  }
}

static void FBlock_Move(Obj *o) {
  int16_t d0, d1;
  switch (obSubtype(o) & 0xF) {
  case 1:
  case 2:
    d1 = (obSubtype(o) & 0xF) == 1 ? 0x40 : 0x80;
    d0 = (obSubtype(o) & 0xF) == 1 ? OSC_B(0xA) : OSC_B(0x1E);
    if (o->status & 1) d0 = (int16_t)(-d0 + d1);
    o->x.w.i = (int16_t)(OW(o, fb_origX) - d0);
    break;
  case 3:
  case 4:
    d1 = (obSubtype(o) & 0xF) == 3 ? 0x40 : 0x80;
    d0 = (obSubtype(o) & 0xF) == 3 ? OSC_B(0xA) : OSC_B(0x1E);
    if (o->status & 1) d0 = (int16_t)(-d0 + d1);
    o->y.w.i = (int16_t)(OW(o, fb_origY) - d0);
    break;
  case 5:
    if (!OB(o, fb_moving) && ZONE_ACT == id_LZ_act1 && OB(o, fb_switch) == 3) {
      V.f_wtunneldisallow = 0;
      if ((uint16_t)player->x.w.i < (uint16_t)o->x.w.i) V.f_wtunneldisallow = 1;
    }
    if (!OB(o, fb_moving) && (V.f_switch[OB(o, fb_switch)] & 1) && ZONE_ACT == id_LZ_act1 &&
        OB(o, fb_switch) == 3)
      V.f_wtunneldisallow = 0;
    fb_door(o, true, 0, fb_door_y);
    break;
  case 6: fb_door(o, false, (int16_t)(o->height * 2), fb_door_y); break;
  case 7:
    if (!OB(o, fb_moving)) {
      if (!V.f_switch[0xF]) break;
      OB(o, fb_moving) = 1;
      OW(o, fb_distance) = 0;
    }
    o->x.w.i++;
    OW(o, fb_origX) = o->x.w.i;
    if (++OW(o, fb_distance) != 0x380) break;
    V.f_obj56 = 1;
    OB(o, fb_moving) = 0;
    obSubtype(o) = 0;
    break;
  case 8: fb_stair(o, 0x10, (int16_t)(OSC_B(0x2A) >> 1), OSC_W(0x2C)); break;
  case 9: fb_stair(o, 0x30, OSC_B(0x2E), OSC_W(0x30)); break;
  case 0xA: fb_stair(o, 0x50, OSC_B(0x32), OSC_W(0x34)); break;
  case 0xB: fb_stair(o, 0x70, OSC_B(0x36), OSC_W(0x38)); break;
  case 0xC: fb_door(o, true, 0, fb_door_x); break;
  case 0xD: fb_door(o, false, 128, fb_door_x); break;
  }
}

void FloatingBlock(Obj *o) {
  static const uint8_t var[8][2] = {{32 / 2, 32 / 2}, {64 / 2, 64 / 2}, {32 / 2, 64 / 2}, {64 / 2, 52 / 2},
                                    {32 / 2, 78 / 2}, {32 / 2, 32 / 2}, {16 / 2, 64 / 2}, {128 / 2, 32 / 2}};
  if (o->routine == 0) {
    o->routine += 2;
    o->map = map_get(MAP_FBlock);
    o->gfx = ArtTile_Level | Tile_Pal3;
    if (V.zone == id_LZ) o->gfx = ArtTile_LZ_Door | Tile_Pal3;
    o->render = sprite_cam_field;
    o->priority = 3;
    int k = ((obSubtype(o) >> 3) & 0xE) >> 1;
    o->actwid = var[k][0];
    o->height = var[k][1];
    o->frame = (uint8_t)k;
    OW(o, fb_origX) = o->x.w.i;
    OW(o, fb_origY) = o->y.w.i;
    OW(o, fb_distance) = (int16_t)(var[k][1] * 2);
    if (obSubtype(o) == 0x37) {
      if (o->x.w.i == 0x1BB8) {
        if (V.f_obj56) {
          DeleteObject(o);
          return;
        }
      } else {
        obSubtype(o) = 0;
        if (!V.f_obj56) {
          DeleteObject(o);
          return;
        }
      }
    }
    if (V.zone != id_LZ && (obSubtype(o) & 0xF) >= 8) {
      if ((int16_t)OSC_W(0x2C + ((obSubtype(o) & 0xF) - 8) * 4) < 0) o->status ^= 1;
    }
    if (obSubtype(o) & 0x80) {
      OB(o, fb_switch) = obSubtype(o) & 0xF;
      obSubtype(o) = 5;
      if (o->frame == 7) {
        obSubtype(o) = 0xC;
        OW(o, fb_distance) = 128;
      }
      if (o->respawn) {
        uint8_t *st = &V.objstate[2 + o->respawn];
        *st &= 0x7F;
        if (*st & 1) {
          obSubtype(o)++;
          OW(o, fb_distance) = 0;
        }
      }
    }
  }
  int16_t x = o->x.w.i;
  FBlock_Move(o);
  if (o->render & 0x80)
    SolidObject(o, (int16_t)(o->actwid + sonic_solid_width), o->height, (int16_t)(o->height + 1), x);
  if (obj_far_x(OW(o, fb_origX)) && (obSubtype(o) != 0x37 || !OB(o, fb_moving))) {
    DeleteObject(o);
    return;
  }
  DisplaySprite(o);
}

/* ------------------------------------------------------------------ spiked balls on chains */
#define sball_children 0x29
#define sball_origY 0x38
#define sball_origX 0x3A
#define sball_radius 0x3C
#define sball_speed 0x3E

static void Sball_Twirl(Obj *o) {
  set_anglew(o, (uint16_t)(anglew(o) + OW(o, sball_speed)));
  int16_t cos, sin = CalcSine(o->angle, &cos);
  int16_t d2 = OW(o, sball_origY), d3 = OW(o, sball_origX);
  int n = OB(o, sball_children);
  for (int i = 0; i <= n; i++) {
    Obj *a1 = OBJ(OB(o, sball_children + 1 + i) & 0x7F);
    int32_t r = OB(a1, sball_radius);
    a1->y.w.i = (int16_t)((int16_t)((r * sin) >> 8) + d2);
    a1->x.w.i = (int16_t)((int16_t)((r * cos) >> 8) + d3);
  }
}

void SpikeBall(Obj *o) {
  switch (o->routine) {
  case 0: {
    o->routine += 2;
    o->map = map_get(MAP_SBall);
    o->gfx = ArtTile_SYZ_Spikeball_Chain;
    o->render = sprite_cam_field;
    o->priority = 4;
    o->actwid = 16 / 2;
    OW(o, sball_origX) = o->x.w.i;
    OW(o, sball_origY) = o->y.w.i;
    o->coltype = col_8x8 | col_hurt;
    if (V.zone == id_LZ) {
      o->coltype = col_none;
      o->gfx = ArtTile_LZ_Spikeball_Chain;
      o->map = map_get(MAP_SBall2);
    }
    OW(o, sball_speed) = (int16_t)((int16_t)(int8_t)(obSubtype(o) & 0xF0) << 3);
    o->angle = (uint8_t)((uint8_t)((o->status >> 2) | (o->status << 6)) & 0xC0);
    uint8_t *list = &OB(o, sball_children);
    *list++ = 0;
    int d1 = obSubtype(o) & 7;
    uint8_t d3 = (uint8_t)(d1 << 4);
    OB(o, sball_radius) = d3;
    d1--;
    if (d1 >= 0 && (obSubtype(o) & 8)) d1--;
    for (; d1 >= 0; d1--) {
      Obj *a1 = FindFreeObj();
      if (!a1) break;
      OB(o, sball_children)++;
      *list++ = SLOT(a1);
      a1->routine = 4;
      a1->id = o->id;
      a1->map = o->map;
      a1->gfx = o->gfx;
      a1->render = o->render;
      a1->priority = o->priority;
      a1->actwid = o->actwid;
      a1->coltype = o->coltype;
      d3 = (uint8_t)(d3 - 0x10);
      OB(a1, sball_radius) = d3;
      if (V.zone == id_LZ && !d3) a1->frame = 2;
    }
    *list++ = SLOT(o);
    if (V.zone == id_LZ) {
      o->coltype = col_16x16 | col_hurt;
      o->frame = 1;
    }
  }
    /* fall through */
  case 2:
    Sball_Twirl(o);
    if (obj_far_x(OW(o, sball_origX))) {
      int n = OB(o, sball_children);
      for (int i = 0; i <= n; i++) DeleteObject(OBJ(OB(o, sball_children + 1 + i) & 0x7F));
      return;
    }
    DisplaySprite(o);
    return;
  case 4: DisplaySprite(o); return;
  }
}

#define bball_origY 0x38
#define bball_origX 0x3A
#define bball_radius 0x3C
#define bball_speed 0x3E

void BigSpikeBall(Obj *o) {
  if (o->routine == 0) {
    o->routine += 2;
    o->map = map_get(MAP_BBall);
    o->gfx = ArtTile_SYZ_Big_Spikeball;
    o->render = sprite_cam_field;
    o->priority = 4;
    o->actwid = 48 / 2;
    OW(o, bball_origX) = o->x.w.i;
    OW(o, bball_origY) = o->y.w.i;
    o->coltype = col_32x32 | col_hurt;
    OW(o, bball_speed) = (int16_t)((int16_t)(int8_t)(obSubtype(o) & 0xF0) << 3);
    o->angle = (uint8_t)((uint8_t)((o->status >> 2) | (o->status << 6)) & 0xC0);
    OB(o, bball_radius) = 0x50;
  }
  int16_t d0;
  switch (obSubtype(o) & 7) {
  case 1:
    d0 = OSC_B(0xE);
    if (o->status & 1) d0 = (int16_t)(-d0 + 0x60);
    o->x.w.i = (int16_t)(OW(o, bball_origX) - d0);
    break;
  case 2:
    d0 = OSC_B(0xE);
    if (o->status & 1) d0 = (int16_t)(-d0 + 0x60 + 0x20);
    o->y.w.i = (int16_t)(OW(o, bball_origY) - d0);
    break;
  case 3: {
    set_anglew(o, (uint16_t)(anglew(o) + OW(o, bball_speed)));
    int16_t cos, sin = CalcSine(o->angle, &cos);
    int32_t r = OB(o, bball_radius);
    o->y.w.i = (int16_t)((int16_t)((r * sin) >> 8) + OW(o, bball_origY));
    o->x.w.i = (int16_t)((int16_t)((r * cos) >> 8) + OW(o, bball_origX));
    break;
  }
  }
  if (obj_far_x(OW(o, bball_origX))) {
    DeleteObject(o);
    return;
  }
  DisplaySprite(o);
}

/* ------------------------------------------------------------------ the boss */
#define BossSpringYard_ChildCmd 0x29
#define BossSpringYard_BlockIndex 0x34
#define BossSpringYard_ParentObj 0x36
#define BossSpringYard_ObjPointer 0x36
#define BossSpringYard_GenericTimer 0x3C
#define BossSpringYard_SineCounter 0x3F
/* (the phase timer is the low byte of the generic timer) */
#define PHASE(o) ((uint8_t)OW(o, BossSpringYard_GenericTimer))
#define SET_PHASE(o, v) (OUW(o, BossSpringYard_GenericTimer) = (uint16_t)((OUW(o, BossSpringYard_GenericTimer) & 0xFF00) | (uint8_t)(v)))

static void BSYZ_StatusUpdate(Obj *o) {
  OB(o, BossSpringYard_BlockIndex) = (uint8_t)((uint16_t)(o->x.w.i - boss_syz_x) >> 5);
  if (o->routine2 >= 6) return;
  if (o->status & 0x80) {
    AddPoints(100);
    o->routine2 = 6;
    OW(o, BossSpringYard_GenericTimer) = 180;
    o->vx = 0;
    return;
  }
  if (o->coltype) return;
  boss_hit_flash(o, col_48x48 | col_boss);
}

static void BSYZ_MoveUpdate(Obj *o) {
  BossMove(o);
  o->y.w.i = obBossY(o).w.i;
  o->x.w.i = obBossX(o).w.i;
  BSYZ_StatusUpdate(o);
}

static void BSYZ_CalcSine(Obj *o) {
  uint8_t s = OB(o, BossSpringYard_SineCounter);
  OB(o, BossSpringYard_SineCounter) += 2;
  o->vy = (int16_t)(CalcSine(s, NULL) >> 2);
  BSYZ_MoveUpdate(o);
}

static void BossSpringYard_FindBlocks(Obj *o) {
  OUW(o, BossSpringYard_ObjPointer) = 0;
  for (int i = 1; i < 64; i++)
    if (objs[i].id == id_BossBlock && obSubtype(&objs[i]) == OB(o, BossSpringYard_BlockIndex)) {
      OUW(o, BossSpringYard_ObjPointer) = OADDR(&objs[i]);
      return;
    }
}

static Obj *grabbed(Obj *o) { return OUW(o, BossSpringYard_ObjPointer) ? OPTR(OUW(o, BossSpringYard_ObjPointer)) : NULL; }

static void BSYZ_ShipMove(Obj *o) {
  int16_t d0 = obBossX(o).w.i;
  o->vx = 0x140;
  bool flip;
  if (!(o->status & 1)) {
    o->vx = -0x140;
    flip = d0 <= boss_syz_x + 8;
  } else flip = d0 >= boss_syz_x + 0x138;
  if (flip) {
    o->status ^= 1;
    SET_PHASE(o, 0);
  }
  int16_t e = (int16_t)(((d0 - (boss_syz_x + 0x10)) & 0x1F) - 0x1F);
  if (e < 0) e = (int16_t)-e;
  if (e - 1 <= 0 && !PHASE(o)) {
    int16_t d1 = (int16_t)((player->x.w.i - boss_syz_x) >> 5);
    if ((uint8_t)d1 == OB(o, BossSpringYard_BlockIndex)) {
      obBossX(o).w.i = (int16_t)(OB(o, BossSpringYard_BlockIndex) * 32 + boss_syz_x + 0x10);
      BossSpringYard_FindBlocks(o);
      o->routine2 += 2;
      obSubtype(o) = 0;
      OB(o, BossSpringYard_ChildCmd) = 0;
      o->vx = 0;
    }
  }
  BSYZ_CalcSine(o);
}

static void BSYZ_Attack(Obj *o) {
  Obj *b = grabbed(o);
  switch (obSubtype(o)) {
  case 0:
    o->vy = 0x180;
    if ((uint16_t)obBossY(o).w.i >= boss_syz_y + 0x8A) {
      obBossY(o).w.i = boss_syz_y + 0x8A;
      OW(o, BossSpringYard_GenericTimer) = 0;
      if (b) {
        OB(b, BossSpringYard_ChildCmd) = 0xFF;
        OB(o, BossSpringYard_ChildCmd) = 0xFF;
        OUW(b, BossSpringYard_ParentObj) = OADDR(o);
        OW(o, BossSpringYard_GenericTimer) = 50;
      }
      o->vy = 0;
      obSubtype(o) += 2;
    }
    BSYZ_MoveUpdate(o);
    return;
  case 2: {
    int16_t d0 = 0;
    if (--OW(o, BossSpringYard_GenericTimer) < 0) {
      obSubtype(o) += 2;
      o->vy = -0x800;
      if (!b) o->vy = (int16_t)(o->vy >> 1);
    } else if (OW(o, BossSpringYard_GenericTimer) <= 30) {
      d0 = 2;
      if (PHASE(o) & 2) d0 = -2;
    }
    o->y.w.i = (int16_t)(obBossY(o).w.i + d0);
    o->x.w.i = obBossX(o).w.i;
    BSYZ_StatusUpdate(o);
    return;
  }
  case 4: {
    int16_t d0 = boss_syz_y + 0xE;
    if (b) d0 = (int16_t)(d0 - 0x18);
    if (d0 >= obBossY(o).w.i) {
      OW(o, BossSpringYard_GenericTimer) = b ? 45 : 8;
      obSubtype(o) += 2;
      o->vy = 0;
    } else if (o->vy < -0x40) o->vy = (int16_t)(o->vy + 0xC);
    BSYZ_MoveUpdate(o);
    return;
  }
  case 6: {
    int16_t t = --OW(o, BossSpringYard_GenericTimer);
    if (t == 0) {
      if (b) OB(b, BossSpringYard_ChildCmd) = 0xA;
      OUW(o, BossSpringYard_ObjPointer) = 0;
      b = NULL;
    } else if (t < 0) {
      if (t == -30) {
        OB(o, BossSpringYard_ChildCmd) = 0;
        o->routine2 -= 2;
        SET_PHASE(o, 0xFF);
        BSYZ_StatusUpdate(o);
        return;
      }
    }
    int16_t d0 = b ? 2 : 1;
    if (obBossY(o).w.i != boss_syz_y + 0xE) {
      if (obBossY(o).w.i >= boss_syz_y + 0xE) d0 = (int16_t)-d0;
      obBossY(o).w.i = (int16_t)(obBossY(o).w.i + d0);
    }
    d0 = 0;
    if (b) {
      d0 = 2;
      if (PHASE(o) & 1) d0 = -2;
    }
    o->y.w.i = (int16_t)(obBossY(o).w.i + d0);
    o->x.w.i = obBossX(o).w.i;
    BSYZ_StatusUpdate(o);
    return;
  }
  }
}

static void BossSpringYard_Display(Obj *o, Obj *a1) {
  o->status = a1->status;
  o->render = (uint8_t)((o->render & ~3) | (o->status & 3));
  DisplaySprite(o);
}

static void BossSpringYard_SetupAnim(Obj *o) {
  AnimateSprite(o, ani_get(ANI_Eggman));
  Obj *a1 = OPTR(OUW(o, BossSpringYard_ParentObj));
  o->x.w.i = a1->x.w.i;
  o->y.w.i = a1->y.w.i;
  BossSpringYard_Display(o, a1);
}

static uint8_t face_chkhit(Obj *a1, uint8_t d1) {
  if (!a1->coltype) return 5;
  if (player->routine >= 4) return 4;
  return d1;
}

void BossSpringYard(Obj *o) {
  switch (o->routine) {
  case 0: {
    static const uint8_t data[4][3] = {{2, 0, 5}, {4, 1, 5}, {6, 7, 5}, {8, 0, 5}};
    o->x.w.i = boss_syz_x + 0x1B0;
    o->y.w.i = boss_syz_y + 0xE;
    obBossX(o).w.i = o->x.w.i;
    obBossY(o).w.i = o->y.w.i;
    o->coltype = col_48x48 | col_boss;
    obBossHits(o) = 8;
    Obj *a1 = o;
    for (int i = 0; i < 4; i++) {
      if (i) {
        a1 = FindNextFreeObj(o);
        if (!a1) break;
        a1->id = id_BossSpringYard;
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
      OUW(a1, BossSpringYard_ParentObj) = OADDR(o);
    }
  }
    /* fall through */
  case 2:
    switch (o->routine2) {
    case 0:
      o->vx = -0x100;
      if ((uint16_t)obBossX(o).w.i < boss_syz_x + 0x138) o->routine2 += 2;
      BSYZ_CalcSine(o);
      break;
    case 2: BSYZ_ShipMove(o); break;
    case 4: BSYZ_Attack(o); break;
    case 6:
      if (--OW(o, BossSpringYard_GenericTimer) >= 0) {
        BossDefeated(o);
        break;
      }
      o->routine2 += 2;
      o->vy = 0;
      o->status |= 1;
      o->status &= 0x7F;
      o->vx = 0;
      OW(o, BossSpringYard_GenericTimer) = -1;
      if (!V.bossstatus) V.bossstatus = 1;
      BSYZ_StatusUpdate(o);
      break;
    case 8: {
      int16_t t = ++OW(o, BossSpringYard_GenericTimer);
      if (t == 0) o->vy = 0;
      else if (t < 0) o->vy = (int16_t)(o->vy + 0x18);
      else if ((uint16_t)t < 32) o->vy = (int16_t)(o->vy - 8);
      else if (t == 32) o->vy = 0;
      else if ((uint16_t)t >= 42) o->routine2 += 2;
      BSYZ_MoveUpdate(o);
      break;
    }
    case 0xA:
      o->vx = 0x400;
      o->vy = -0x40;
      if ((uint16_t)V.limitright2 >= boss_syz_end) {
        if (!(o->render & 0x80)) {
          DeleteObject(o);
          return;
        }
      } else V.limitright2 += 2;
      BossMove(o);
      BSYZ_CalcSine(o);
      break;
    }
    AnimateSprite(o, ani_get(ANI_Eggman));
    o->render = (uint8_t)((o->render & ~3) | (o->status & 3));
    DisplaySprite(o);
    return;
  case 4: {
    Obj *a1 = OPTR(OUW(o, BossSpringYard_ParentObj));
    uint8_t d1 = 1;
    switch (a1->routine2) {
    case 0:
    case 2: d1 = face_chkhit(a1, d1); break;
    case 4:
      if (obSubtype(a1) == 2) d1 = 6;
      d1 = face_chkhit(a1, d1);
      break;
    case 6:
    case 8: d1 = 0xA; break;
    case 0xA: d1 = 6; break;
    }
    o->anim = d1;
    if (o->id != a1->id) {
      DeleteObject(o);
      return;
    }
    BossSpringYard_SetupAnim(o);
    return;
  }
  case 6: {
    Obj *a1 = OPTR(OUW(o, BossSpringYard_ParentObj));
    o->anim = 7;
    if (a1->routine2 == 0xA) {
      o->anim = 0xB;
      if (!(o->render & 0x80)) {
        DeleteObject(o);
        return;
      }
    } else if (a1->vx) o->anim = 8;
    BossSpringYard_SetupAnim(o);
    return;
  }
  case 8: {
    o->map = map_get(MAP_BossItems);
    o->gfx = ArtTile_Eggman_Weapons | Tile_Pal2;
    o->frame = 5;
    Obj *a1 = OPTR(OUW(o, BossSpringYard_ParentObj));
    if (a1->routine2 == 0xA && !(o->render & 0x80)) {
      DeleteObject(o);
      return;
    }
    o->x.w.i = a1->x.w.i;
    o->y.w.i = a1->y.w.i;
    int16_t d0 = OW(o, BossSpringYard_GenericTimer);
    bool retract = true;
    if (a1->routine2 == 4) {
      if (obSubtype(a1) == 6) retract = OW(a1, BossSpringYard_GenericTimer) < 0;
      else {
        retract = false;
        if (!obSubtype(a1) && d0 < 0x94) d0 = (int16_t)(d0 + 7);
      }
    }
    if (retract && d0 > 0) d0 = (int16_t)(d0 - 5);
    OW(o, BossSpringYard_GenericTimer) = d0;
    o->y.w.i = (int16_t)(o->y.w.i + (d0 >> 2));
    o->actwid = 16 / 2;
    o->height = 24 / 2;
    o->coltype = 0;
    if (a1->coltype && !OB(a1, BossSpringYard_ChildCmd)) o->coltype = col_8x32 | col_hurt;
    BossSpringYard_Display(o, a1);
    return;
  }
  }
}

static void BossBlock_Break(Obj *o) {
  static const int16_t speed[4][2] = {{-0x180, -0x200}, {0x180, -0x200}, {-0x100, -0x100}, {0x100, -0x100}};
  static const int16_t pos[4][2] = {{-8, -8}, {0x10, 0}, {0, 0x10}, {0x10, 0x10}};
  o->routine += 2;
  o->actwid = 16 / 2;
  o->height = 16 / 2;
  Obj *a1 = o;
  for (int i = 0; i < 4; i++) {
    if (i) {
      a1 = FindNextFreeObj(o);
      if (!a1) break;
    }
    *a1 = *o;
    a1->vx = speed[i][0];
    a1->vy = speed[i][1];
    a1->x.w.i = (int16_t)(a1->x.w.i + pos[i][0]);
    a1->y.w.i = (int16_t)(a1->y.w.i + pos[i][1]);
    a1->frame = (uint8_t)(1 + i);
  }
}

void BossBlock(Obj *o) {
  switch (o->routine) {
  case 0: {
    Obj *a1 = o;
    for (int i = 0; i < 10; i++) {
      if (i) {
        a1 = FindFreeObj();
        if (!a1) return;
      }
      a1->id = id_BossBlock;
      a1->map = map_get(MAP_BossBlock);
      a1->gfx = ArtTile_Level | Tile_Pal3;
      a1->render = sprite_cam_field;
      a1->actwid = 32 / 2;
      a1->height = 32 / 2;
      a1->priority = 3;
      a1->x.w.i = (int16_t)(boss_syz_x + 0x10 + i * 0x20);
      a1->y.w.i = 0x582;
      obSubtype(a1) = (uint8_t)i;
      OB(a1, BossSpringYard_ChildCmd) = (uint8_t)i;
      a1->routine += 2;
    }
    return;
  }
  case 2: {
    uint8_t d0 = OB(o, BossSpringYard_ChildCmd);
    if (d0 == obSubtype(o)) {
      SolidObject(o, 16 + sonic_solid_width, 16, 17, o->x.w.i);
      DisplaySprite(o);
      return;
    }
    if (d0 & 0x80) {
      Obj *a1 = OPTR(OUW(o, BossSpringYard_ParentObj));
      if (obBossHits(a1)) {
        o->x.w.i = a1->x.w.i;
        o->y.w.i = (int16_t)(a1->y.w.i + 44);
        if (a1 >= o) o->y.w.i = (int16_t)(o->y.w.i + (a1->vy >> 8));
        DisplaySprite(o);
        return;
      }
    }
    BossBlock_Break(o);
    DisplaySprite(o);
    return;
  }
  case 4:
    if (!(o->render & 0x80)) {
      DeleteObject(o);
      return;
    }
    ObjectFall(o);
    DisplaySprite(o);
    return;
  }
}
