/* Scrap Brain Zone's objects: the Ball Hog and its bouncing bombs, the small doors, the rotating
 * junctions, running discs, conveyor belts, trapdoors and spinning platforms, saws and pizza cutters,
 * stompers and sliding doors, vanishing platforms, flamethrowers, electrocuters, the spinning
 * platforms' conveyors, girders and teleporters; Act 2's Dr. Robotnik and his collapsing floor; and
 * the Final Zone's crushing cylinders and plasma balls. */
#include "s1.h"
#include "collide.h"
#include "game.h"
#include "objs.h"

#define GO 0x474F /* "GO" and "SW": the commands bosses leave in their pieces' subtype words */
#define SW 0x5357

/* ------------------------------------------------------------------ Ball Hog */
#define hog_launched 0x32
#define cbal_time 0x30

void BallHog(Obj *o) {
  if (o->routine == 0) {
    o->height = 38 / 2;
    o->width = 16 / 2;
    o->map = map_get(MAP_Hog);
    o->gfx = ArtTile_Ball_Hog | Tile_Pal2;
    o->render = sprite_cam_field;
    o->priority = 4;
    o->coltype = col_24x36 | col_badnik;
    o->actwid = 24 / 2;
    ObjectFall(o);
    uint8_t a;
    int16_t d1 = ObjFloorDist(o, &a);
    if (d1 < 0) {
      o->y.w.i = (int16_t)(o->y.w.i + d1);
      o->vy = 0;
      o->routine += 2;
    }
    return;
  }
  AnimateSprite(o, ani_get(ANI_Hog));
  if (o->frame != 1) OB(o, hog_launched) = 0;
  else if (!OB(o, hog_launched)) {
    OB(o, hog_launched) = 1;
    Obj *a1 = FindFreeObj();
    if (a1) {
      a1->id = id_Cannonball;
      a1->x.w.i = o->x.w.i;
      a1->y.w.i = o->y.w.i;
      a1->vx = -0x100;
      a1->vy = 0;
      int16_t d0 = -4;
      if (o->status & 1) d0 = 4, a1->vx = 0x100;
      a1->x.w.i = (int16_t)(a1->x.w.i + d0);
      a1->y.w.i = (int16_t)(a1->y.w.i + 12);
      obSubtype(a1) = obSubtype(o);
    }
  }
  RememberState(o);
}

void Cannonball(Obj *o) {
  if (o->routine == 0) {
    o->routine += 2;
    o->height = 14 / 2;
    o->map = map_get(MAP_Hog);
    o->gfx = ArtTile_Ball_Hog | Tile_Pal2;
    o->render = sprite_cam_field;
    o->priority = 3;
    o->coltype = col_12x12 | col_hurt;
    o->actwid = 16 / 2;
    OW(o, cbal_time) = (int16_t)(obSubtype(o) * 60);
    o->frame = 4;
  }
  ObjectFall(o);
  if (o->vy >= 0) {
    uint8_t a;
    int16_t d1 = ObjFloorDist(o, &a);
    if (d1 < 0) {
      o->y.w.i = (int16_t)(o->y.w.i + d1);
      o->vy = -0x300;
      /* bounced off a slope: away from it */
      if ((int8_t)a < 0) {
        if (o->vx >= 0) o->vx = (int16_t)-o->vx;
      } else if (a && o->vx < 0) o->vx = (int16_t)-o->vx;
    }
  }
  if (--OW(o, cbal_time) < 0) {
    o->id = id_Explosion;
    o->routine = 0;
    Explosion(o);
    return;
  }
  if ((int8_t)--o->timeframe < 0) {
    o->timeframe = 6 - 1;
    o->frame ^= 1;
  }
  DisplaySprite(o);
  if ((uint16_t)(V.limitbtm2 + 224) < (uint16_t)o->y.w.i) DeleteObject(o);
}

/* ------------------------------------------------------------------ small doors */
void AutoDoor(Obj *o) {
  if (o->routine == 0) {
    o->routine += 2;
    o->map = map_get(MAP_ADoor);
    o->gfx = ArtTile_SBZ_Door | Tile_Pal3;
    o->render |= sprite_cam_field;
    o->actwid = 16 / 2;
    o->priority = 4;
  }
  o->anim = 0;
  uint16_t d0 = (uint16_t)(player->x.w.i + 64), x = (uint16_t)o->x.w.i;
  if (d0 >= x && (uint16_t)(d0 - 128) < x) {
    /* opens for Sonic coming from the side it faces */
    if ((uint16_t)player->x.w.i >= x ? (o->status & 1) : !(o->status & 1)) o->anim = 1;
  }
  AnimateSprite(o, ani_get(ANI_ADoor));
  if (!o->frame) SolidObject(o, 12 / 2 + sonic_solid_width, 64 / 2, 64 / 2 + 1, o->x.w.i);
  RememberState(o);
}

/* ------------------------------------------------------------------ rotating junctions */
#define jun_unused 0x30
#define jun_grabframe 0x32
#define jun_direction 0x34
#define jun_switchdown 0x36
#define jun_switchid 0x38

static void Jun_Rotate(Obj *o) {
  if (V.f_switch[OB(o, jun_switchid) & 15] & 1) {
    if (!OB(o, jun_switchdown)) {
      OB(o, jun_direction) = (uint8_t)-OB(o, jun_direction);
      OB(o, jun_switchdown) = 1;
    }
  } else OB(o, jun_switchdown) = 0;
  if ((int8_t)--o->timeframe >= 0) return;
  o->timeframe = 8 - 1;
  o->frame = (uint8_t)((o->frame + OB(o, jun_direction)) & 0xF);
}

static void Jun_ChgPos(Obj *o) {
  static const int8_t off[16][2] = {{-0x20, 0}, {-0x1E, 0xE}, {-0x18, 0x18}, {-0xE, 0x1E}, {0, 0x20}, {0xE, 0x1E},
                                    {0x18, 0x18}, {0x1E, 0xE}, {0x20, 0}, {0x1E, -0xE}, {0x18, -0x18}, {0xE, -0x1E},
                                    {0, -0x20}, {-0xE, -0x1E}, {-0x18, -0x18}, {-0x1E, -0xE}};
  player->x.w.i = (int16_t)(o->x.w.i + off[o->frame & 15][0]);
  player->y.w.i = (int16_t)(o->y.w.i + off[o->frame & 15][1]);
}

void Junction(Obj *o) {
  switch (o->routine) {
  case 0:
    o->routine += 2;
    for (int i = 0; i < 2; i++) {
      Obj *a1 = o;
      if (i) {
        a1 = FindFreeObj();
        if (!a1) continue;
        a1->id = id_Junction;
        a1->routine += 4;
        a1->x.w.i = o->x.w.i;
        a1->y.w.i = o->y.w.i;
        a1->priority = 3;
        a1->frame = 0x10;
      }
      a1->map = map_get(MAP_Jun);
      a1->gfx = ArtTile_SBZ_Junction | Tile_Pal3;
      a1->render |= sprite_cam_field;
      a1->actwid = 112 / 2;
    }
    o->actwid = 96 / 2;
    o->priority = 4;
    OW(o, jun_unused) = 60;
    OB(o, jun_direction) = 1;
    OB(o, jun_switchid) = obSubtype(o);
    /* fall through */
  case 2: {
    Jun_Rotate(o);
    if (!(o->render & 0x80)) break;
    int16_t d1 = 74 / 2 + sonic_solid_width;
    SolidObject(o, d1, d1, (int16_t)(d1 + 1), o->x.w.i);
    if (!(o->status & status_pushing)) break;
    Obj *a1 = player;
    uint8_t f = (uint16_t)a1->x.w.i < (uint16_t)o->x.w.i ? 0xE : 7;
    if (f != o->frame) break;
    /* Sonic pushed into the opening: rolled up inside, carried round */
    OB(o, jun_grabframe) = f;
    o->routine += 4;
    V.f_playerctrl = 1;
    a1->anim = id_Roll;
    a1->inertia = 0x800;
    a1->vx = a1->vy = 0;
    o->status &= (uint8_t)~status_pushing;
    a1->status &= (uint8_t)~status_pushing;
    a1->status |= status_in_air;
    int16_t x = a1->x.w.i, y = a1->y.w.i;
    Jun_ChgPos(o);
    a1->x.w.i = (int16_t)((int16_t)(a1->x.w.i + x) >> 1);
    a1->y.w.i = (int16_t)((int16_t)(a1->y.w.i + y) >> 1);
    break;
  }
  case 4: break;
  case 6: {
    uint8_t f = o->frame;
    if ((f == 4 || f == 7) && f != OB(o, jun_grabframe)) {
      player->vx = 0;
      player->vy = 0x800;
      if (f != 4) player->vx = 0x800;
      V.f_playerctrl = 0;
      o->routine -= 4;
    }
    Jun_Rotate(o);
    Jun_ChgPos(o);
    break;
  }
  }
  RememberState(o);
}

/* ------------------------------------------------------------------ running discs */
#define disc_origY 0x30
#define disc_origX 0x32
#define disc_spot_distance 0x34
#define disc_spot_speed 0x36
#define disc_triggersize 0x38
#define disc_sonic_attached 0x3A

static void Disc_MoveSonic(Obj *o) {
  uint16_t d2 = OB(o, disc_triggersize), d3 = (uint16_t)(d2 * 2);
  Obj *a1 = player;
  if ((uint16_t)(a1->x.w.i - OW(o, disc_origX) + d2) >= d3 || (uint16_t)(a1->y.w.i - OW(o, disc_origY) + d2) >= d3) {
    if (OB(o, disc_sonic_attached)) {
      OB(a1, sticktoconvex) = 0;
      OB(o, disc_sonic_attached) = 0;
    }
    return;
  }
  if (a1->status & status_in_air) {
    OB(o, disc_sonic_attached) = 0;
    return;
  }
  if (!OB(o, disc_sonic_attached)) {
    OB(o, disc_sonic_attached) = 1;
    if (!(a1->status & status_rolling)) a1->anim = 0;
    a1->status &= (uint8_t)~status_pushing;
    a1->prevani = id_Run;
    OB(a1, sticktoconvex) = 1;
  }
  /* kept between a least and a most speed round the disc */
  int16_t d0 = a1->inertia;
  if (OW(o, disc_spot_speed) < 0) {
    if (d0 > -0x400) a1->inertia = -0x400;
    else if (d0 < -0xF00) a1->inertia = -0xF00;
  } else {
    if (d0 < 0x400) a1->inertia = 0x400;
    else if (d0 > 0xF00) a1->inertia = 0xF00;
  }
}

void RunningDisc(Obj *o) {
  if (o->routine == 0) {
    o->routine += 2;
    o->map = map_get(MAP_Disc);
    o->gfx = ArtTile_SBZ_Disc | Tile_Pal3 | Tile_Prio;
    o->render = sprite_cam_field;
    o->priority = 4;
    o->actwid = 16 / 2;
    OW(o, disc_origX) = o->x.w.i;
    OW(o, disc_origY) = o->y.w.i;
    OB(o, disc_spot_distance) = 0x18;
    OB(o, disc_triggersize) = 0x48;
    if (obSubtype(o) & 0xF) {
      OB(o, disc_spot_distance) = 0x10;
      OB(o, disc_triggersize) = 0x38;
    }
    OW(o, disc_spot_speed) = (int16_t)((int8_t)(obSubtype(o) & 0xF0) * 8);
    o->angle = (uint8_t)((o->status & 3) << 6);
  }
  Disc_MoveSonic(o);
  /* the spot going round */
  set_anglew(o, (uint16_t)(anglew(o) + OW(o, disc_spot_speed)));
  int16_t cos, sin = CalcSine(o->angle, &cos);
  int32_t r = OB(o, disc_spot_distance) << 8;
  o->y.w.i = (int16_t)(OW(o, disc_origY) + (int16_t)(r * sin >> 16));
  o->x.w.i = (int16_t)(OW(o, disc_origX) + (int16_t)(r * cos >> 16));
  if (obj_far_x(OW(o, disc_origX))) {
    DeleteObject(o);
    return;
  }
  DisplaySprite(o);
}

/* ------------------------------------------------------------------ conveyor belts */
#define conv_speed 0x36
#define conv_width 0x38

void Conveyor(Obj *o) {
  if (o->routine == 0) {
    o->routine += 2;
    OB(o, conv_width) = 256 / 2;
    if (obSubtype(o) & 0xF) OB(o, conv_width) = 112 / 2;
    OW(o, conv_speed) = (int16_t)((int8_t)(obSubtype(o) & 0xF0) >> 4);
  }
  uint16_t d2 = OB(o, conv_width);
  Obj *a1 = player;
  if ((uint16_t)(a1->x.w.i - o->x.w.i + d2) < (uint16_t)(d2 * 2) && (uint16_t)(a1->y.w.i - o->y.w.i + 48) < 48 &&
      !(a1->status & status_in_air))
    a1->x.w.i = (int16_t)(a1->x.w.i + OW(o, conv_speed));
  if (obj_far(o)) DeleteObject(o);
}

/* ------------------------------------------------------------------ trapdoors, spinning platforms */
#define spin_timer 0x30
#define spin_timelen 0x32
#define spin_spinning 0x34
#define spin_syncmask 0x36

/* not solid while open (or turned edge on): Sonic dropped */
static void spin_solid(Obj *o, int16_t d1, int16_t d2) {
  if (!o->frame) {
    SolidObject(o, d1, d2, (int16_t)(d2 + 1), o->x.w.i);
  } else if (o->status & status_on_object) {
    player->status &= (uint8_t)~status_on_object;
    o->status &= (uint8_t)~status_on_object;
    o->routine2 = 0;
  }
  RememberState(o);
}

void SpinPlatform(Obj *o) {
  switch (o->routine) {
  case 0:
    o->routine += 2;
    o->map = map_get(MAP_Trap);
    o->gfx = ArtTile_SBZ_Trap_Door | Tile_Pal3;
    o->render |= sprite_cam_field;
    o->actwid = 256 / 2;
    OW(o, spin_timelen) = (int16_t)((obSubtype(o) & 0xF) * 60);
    if (obSubtype(o) & 0x80) {
      o->routine += 2;
      o->map = map_get(MAP_Spin);
      o->gfx = ArtTile_SBZ_Spinning_Platform;
      o->actwid = 32 / 2;
      o->anim = 2;
      OW(o, spin_timer) = OW(o, spin_timelen) = (int16_t)((obSubtype(o) & 0xF) * 6);
      OW(o, spin_syncmask) = (int16_t)((((obSubtype(o) & 0x70) + 0x10) << 2) - 1);
      goto spinner;
    }
    /* fall through */
  case 2:
    if (--OW(o, spin_timer) < 0) {
      OW(o, spin_timer) = OW(o, spin_timelen);
      o->anim ^= 1;
    }
    AnimateSprite(o, ani_get(ANI_Spin));
    spin_solid(o, 128 / 2 + sonic_solid_width, 24 / 2);
    return;
  case 4:
  spinner:
    if (!(V.framecount & OUW(o, spin_syncmask))) OB(o, spin_spinning) = 1;
    if (OB(o, spin_spinning) && --OW(o, spin_timer) < 0) {
      OW(o, spin_timer) = OW(o, spin_timelen);
      OB(o, spin_spinning) = 0;
      o->anim ^= 1;
    }
    AnimateSprite(o, ani_get(ANI_Spin));
    spin_solid(o, 32 / 2 + sonic_solid_width, 14 / 2);
    return;
  }
}

/* ------------------------------------------------------------------ saws and pizza cutters */
#define saw_origY 0x38
#define saw_origX 0x3A
#define saw_shot 0x3D

static void saw_spin(Obj *o) {
  if ((int8_t)--o->timeframe >= 0) return;
  o->timeframe = 3 - 1;
  o->frame ^= 1;
}

/* the saws that come speeding at Sonic (from the left, or from the right): false while hidden */
static bool saw_speeding(Obj *o, bool from_right) {
  if (OB(o, saw_shot)) {
    SpeedToPos(o);
    OW(o, saw_origX) = o->x.w.i;
    saw_spin(o);
    return true;
  }
  if (from_right) {
    if ((uint16_t)(player->x.w.i + 224) >= (uint16_t)o->x.w.i) return false;
  } else {
    uint16_t d0 = (uint16_t)(player->x.w.i - 192);
    if ((uint16_t)player->x.w.i < 192 || d0 < (uint16_t)o->x.w.i) return false;
  }
  uint16_t d0 = (uint16_t)(player->y.w.i - 128);
  if (d0 >= (uint16_t)o->y.w.i || (uint16_t)(d0 + 256) < (uint16_t)o->y.w.i) return true;
  OB(o, saw_shot) = 1;
  o->vx = from_right ? -0x600 : 0x600;
  o->coltype = col_48x48_alt | col_hurt;
  o->frame = 2;
  return false;
}

void Saws(Obj *o) {
  if (o->routine == 0) {
    o->routine += 2;
    o->map = map_get(MAP_Saw);
    o->gfx = ArtTile_SBZ_Saw | Tile_Pal3;
    o->render = sprite_cam_field;
    o->priority = 4;
    o->actwid = 64 / 2;
    OW(o, saw_origX) = o->x.w.i;
    OW(o, saw_origY) = o->y.w.i;
    if (obSubtype(o) < 3) o->coltype = col_48x48_alt | col_hurt;
  }
  int16_t d0;
  switch (obSubtype(o) & 7) {
  case 1:
    d0 = OSC_B(0xE);
    if (o->status & 1) d0 = (int16_t)(-d0 + 0x60);
    o->x.w.i = (int16_t)(OW(o, saw_origX) - d0);
    saw_spin(o);
    break;
  case 2:
    d0 = OSC_B(6);
    if (o->status & 1) d0 = (int16_t)(-d0 + 0x60 + 0x20);
    o->y.w.i = (int16_t)(OW(o, saw_origY) - d0);
    saw_spin(o);
    break;
  case 3:
    if (!saw_speeding(o, false)) return;
    break;
  case 4:
    if (!saw_speeding(o, true)) return;
    break;
  }
  if (obj_far_x(OW(o, saw_origX))) {
    DeleteObject(o);
    return;
  }
  DisplaySprite(o);
}

/* ------------------------------------------------------------------ stompers and sliding doors */
#define sto_origY 0x30
#define sto_origX 0x34
#define sto_delay 0x36
#define sto_active 0x38
#define sto_offset_now 0x3A
#define sto_offset_max 0x3C
#define sto_switch 0x3E

static void sto_forget(Obj *o) {
  if (o->respawn) V.objstate[2 + o->respawn] &= 0x7F;
}

static void sto_setx(Obj *o) {
  int16_t d0 = OW(o, sto_offset_now);
  if (o->status & 1) d0 = (int16_t)(-d0 + 128);
  o->x.w.i = (int16_t)(OW(o, sto_origX) - d0);
}

static void sto_sety(Obj *o) {
  int16_t d0 = OW(o, sto_offset_now);
  if (o->status & 1) d0 = (int16_t)(-d0 + 56);
  o->y.w.i = (int16_t)(OW(o, sto_origY) + d0);
}

static void Sto_Types(Obj *o) {
  uint8_t *st = &V.objstate[2 + o->respawn];
  switch (obSubtype(o) & 0xF) {
  case 1: /* a door sliding open when its switch is pressed */
    if (!OB(o, sto_active)) {
      if (!(V.f_switch[OB(o, sto_switch) & 15] & 1)) break;
      OB(o, sto_active) = 1;
    }
    if (OW(o, sto_offset_max) == OW(o, sto_offset_now)) {
      obSubtype(o)++;
      OW(o, sto_delay) = 3 * 60;
      OB(o, sto_active) = 0;
      if (o->respawn) *st |= 1;
    } else OW(o, sto_offset_now) += 2;
    break;
  case 2: /* and shutting a while after */
    if (!OB(o, sto_active)) {
      if (--OW(o, sto_delay)) break;
      OB(o, sto_active) = 1;
    }
    if (!OW(o, sto_offset_now)) {
      obSubtype(o)--;
      OB(o, sto_active) = 0;
      if (o->respawn) *st &= (uint8_t)~1;
    } else OW(o, sto_offset_now) -= 2;
    sto_setx(o);
    return;
  case 3: /* stomps down, then rises slowly */
    if (!OB(o, sto_active)) {
      if (OW(o, sto_offset_now)) {
        OW(o, sto_offset_now)--;
        sto_sety(o);
        return;
      }
      if (--OW(o, sto_delay) >= 0) {
        sto_sety(o);
        return;
      }
      OW(o, sto_delay) = 60;
      OB(o, sto_active) = 1;
    }
    OW(o, sto_offset_now) += 8;
    if (OW(o, sto_offset_now) == OW(o, sto_offset_max)) OB(o, sto_active) = 0;
    sto_sety(o);
    return;
  case 4: /* stomps down and up */
    if (!OB(o, sto_active)) {
      if (OW(o, sto_offset_now)) {
        OW(o, sto_offset_now) -= 8;
        sto_sety(o);
        return;
      }
      if (--OW(o, sto_delay) >= 0) {
        sto_sety(o);
        return;
      }
      OW(o, sto_delay) = 60;
      OB(o, sto_active) = 1;
    }
    if (OW(o, sto_offset_now) != OW(o, sto_offset_max)) OW(o, sto_offset_now) += 8;
    else if (--OW(o, sto_delay) < 0) {
      OW(o, sto_delay) = 60;
      OB(o, sto_active) = 0;
    }
    sto_sety(o);
    return;
  case 5: /* the lift down to Act 3 (in Labyrinth's water), going when its switch is pressed */
    if (!OB(o, sto_active)) {
      if (!(V.f_switch[OB(o, sto_switch) & 15] & 1)) return;
      OB(o, sto_active) = 1;
      if (o->respawn) *st |= 1;
    }
    o->x.l -= 0x10000;
    o->y.l += 0x8000;
    OW(o, sto_origX) = o->x.w.i;
    if (o->x.w.i == 0x980) obSubtype(o) = 0, OB(o, sto_active) = 0;
    return;
  default: return;
  }
  sto_setx(o);
}

void ScrapStomp(Obj *o) {
  static const uint8_t var[5][4] = {
    {128 / 2, 24 / 2, 128, 1}, {56 / 2, 64 / 2, 56, 3}, {56 / 2, 64 / 2, 64, 4}, {56 / 2, 64 / 2, 96, 4}, {256 / 2, 128 / 2, 0, 5},
  };
  if (o->routine == 0) {
    o->routine += 2;
    int k = (obSubtype(o) >> 4) & 7;
    const uint8_t *a3 = var[k < 5 ? k : 4];
    o->actwid = a3[0];
    o->height = a3[1];
    o->frame = (uint8_t)k;
    o->map = map_get(MAP_Stomp);
    o->gfx = ArtTile_SBZ_Moving_Block_Short | Tile_Pal2;
    if (V.zone == id_LZ) {
      /* (Act 3: only one of them at a time, and the lift gone once used) */
      uint8_t old = V.obj6B;
      V.obj6B |= 1;
      if (old & 1) {
        sto_forget(o);
        DeleteObject(o);
        return;
      }
      o->gfx = (ArtTile_Level + 0x1F0) | Tile_Pal3;
      if (o->x.w.i == 0xA80 && o->respawn && (V.objstate[2 + o->respawn] & 1)) {
        V.obj6B = 0;
        sto_forget(o);
        DeleteObject(o);
        return;
      }
    }
    o->render |= sprite_cam_field;
    o->priority = 4;
    OW(o, sto_origX) = o->x.w.i;
    OW(o, sto_origY) = o->y.w.i;
    OW(o, sto_offset_max) = a3[2];
    if (obSubtype(o) & 0x80) {
      OB(o, sto_switch) = obSubtype(o) & 0xF;
      obSubtype(o) = a3[3];
      if (a3[3] == 5) o->render |= 0x10; /* (seen by its height) */
      sto_forget(o);
    }
  }
  int16_t x = o->x.w.i;
  Sto_Types(o);
  if (o->render & 0x80)
    SolidObject(o, (int16_t)(o->actwid + sonic_solid_width), o->height, (int16_t)(o->height + 1), x);
  if (obj_far_x(OW(o, sto_origX))) {
    if (V.zone == id_LZ) {
      V.obj6B = 0;
      sto_forget(o);
    }
    DeleteObject(o);
    return;
  }
  DisplaySprite(o);
}

/* ------------------------------------------------------------------ vanishing platforms */
#define vanp_timer 0x30
#define vanp_timelen 0x32
#define vanp_syncoffset 0x36
#define vanp_syncmask 0x38

void VanishPlatform(Obj *o) {
  switch (o->routine) {
  case 0: {
    o->routine += 6;
    o->map = map_get(MAP_VanP);
    o->gfx = ArtTile_SBZ_Vanishing_Block | Tile_Pal3;
    o->render |= sprite_cam_field;
    o->actwid = 32 / 2;
    o->priority = 4;
    uint16_t d0 = (uint16_t)(((obSubtype(o) & 0xF) + 1) << 7), d1 = d0;
    OW(o, vanp_timer) = OW(o, vanp_timelen) = (int16_t)(d0 - 1);
    d1 = (uint16_t)(d1 + 0x80);
    OW(o, vanp_syncoffset) = (int16_t)((uint32_t)(obSubtype(o) & 0xF0) * d1 >> 8);
    OW(o, vanp_syncmask) = (int16_t)(d1 - 1);
  }
    /* fall through */
  case 6:
    /* (all of them in step: they start together) */
    if ((uint16_t)(V.framecount - OW(o, vanp_syncoffset)) & OUW(o, vanp_syncmask)) {
      AnimateSprite(o, ani_get(ANI_Van));
      RememberState(o);
      return;
    }
    o->routine -= 4;
    /* fall through */
  case 2:
  case 4:
    if (--OW(o, vanp_timer) < 0) {
      OW(o, vanp_timer) = 0x80 - 1;
      if (o->anim) OW(o, vanp_timer) = OW(o, vanp_timelen);
      o->anim ^= 1;
    }
    AnimateSprite(o, ani_get(ANI_Van));
    if (!(o->frame & 2)) {
      if (o->routine == 2) PlatformObject(o, o->actwid);
      else {
        ExitPlatform(o, o->actwid);
        MvSonicOnPtfm2(o, o->x.w.i);
      }
    } else if (o->status & status_on_object) {
      player->status &= (uint8_t)~status_on_object;
      o->status &= (uint8_t)~status_on_object;
      o->routine = 2;
      o->routine2 = 0;
    }
    RememberState(o);
    return;
  }
}

/* ------------------------------------------------------------------ flamethrowers, electrocuters */
#define flame_timer 0x30
#define flame_firetime 0x32
#define flame_pausetime 0x34
#define flame_hurtframe 0x36

void Flamethrower(Obj *o) {
  if (o->routine == 0) {
    o->routine += 2;
    o->map = map_get(MAP_Flame);
    o->gfx = ArtTile_SBZ_Flamethrower | Tile_Prio;
    o->render |= sprite_cam_field;
    o->priority = 1;
    o->actwid = 24 / 2;
    OW(o, flame_timer) = OW(o, flame_firetime) = (int16_t)((obSubtype(o) & 0xF0) * 2);
    OW(o, flame_pausetime) = (int16_t)((obSubtype(o) & 0xF) << 5);
    OB(o, flame_hurtframe) = 0xA;
    if (o->status & 2) {
      o->anim = 2;
      OB(o, flame_hurtframe) = 0x15;
    }
  }
  if (--OW(o, flame_timer) < 0) {
    OW(o, flame_timer) = OW(o, flame_pausetime);
    uint8_t old = o->anim;
    o->anim ^= 1;
    if (old & 1) OW(o, flame_timer) = OW(o, flame_firetime);
  }
  AnimateSprite(o, ani_get(ANI_Flame));
  o->coltype = col_none;
  if (o->frame == OB(o, flame_hurtframe)) o->coltype = col_24x48 | col_hurt;
  if (obj_far(o)) {
    DeleteObject(o);
    return;
  }
  DisplaySprite(o);
}

#define elec_freq 0x34

void Electro(Obj *o) {
  if (o->routine == 0) {
    o->routine += 2;
    o->map = map_get(MAP_Elec);
    o->gfx = ArtTile_SBZ_Electric_Orb;
    o->render |= sprite_cam_field;
    o->actwid = 80 / 2;
    OW(o, elec_freq) = (int16_t)((obSubtype(o) << 4) - 1);
  }
  if (!(V.framecount & OUW(o, elec_freq))) o->anim = 1;
  AnimateSprite(o, ani_get(ANI_Elec));
  o->coltype = col_none;
  if (o->frame == 4) o->coltype = col_144x16 | col_hurt;
  RememberState(o);
}

/* ------------------------------------------------------------------ the spinning platforms' conveyors */
#define spinc_groupid 0x2F
#define spinc_baseX 0x30
#define spinc_nextX 0x34
#define spinc_nextY 0x36
#define spinc_posindex 0x38
#define spinc_count 0x39
#define spinc_increment 0x3A
#define spinc_reversed 0x3B
#define spinc_targetdata 0x3C

#define SPINC(x, y, a, b, c, d) {4 * 4, x, x - 0x6C, y + a, x + 0x6F, y + b, x + 0x6F, y + c, x - 0x6C, y + d}
static const uint16_t spinc_data[6][2 + 8] = {
  SPINC(0xE80, 0x358, 0x18, -0x56, -0x18, 0x56), SPINC(0xF80, 0x2C8, 0x18, -0x56, -0x18, 0x56),
  SPINC(0x1080, 0x228, 0x48, -0x26, 0x18, 0x86), SPINC(0xF80, 0x558, 0x18, -0x56, -0x18, 0x56),
  SPINC(0x1B80, 0x658, 0x18, -0x56, -0x18, 0x56), SPINC(0x1C80, 0x5C8, 0x18, -0x56, -0x18, 0x56),
};

static void spinc_target(Obj *o, uint8_t d1) {
  const uint16_t *t = &spinc_data[OB(o, spinc_targetdata)][2];
  OB(o, spinc_posindex) = d1;
  OW(o, spinc_nextX) = (int16_t)t[d1 / 2];
  OW(o, spinc_nextY) = (int16_t)t[d1 / 2 + 1];
  /* (turning at the corners: on its edge going up and down the sides) */
  if (d1 == 0) o->anim = 1;
  if (d1 == 4 * 2) o->anim = 0;
  LCon_ChangeDir(o);
}

static uint8_t spinc_next(Obj *o) {
  uint8_t d1 = (uint8_t)(OB(o, spinc_posindex) + OB(o, spinc_increment));
  if (d1 >= OB(o, spinc_count)) {
    uint8_t d0 = d1;
    d1 = 0;
    if (d0 & 0x80) d1 = (uint8_t)(OB(o, spinc_count) - 4);
  }
  return d1;
}

static void SpinC_Platform_Update(Obj *o) {
  if (o->x.w.i == OW(o, spinc_nextX) && o->y.w.i == OW(o, spinc_nextY)) spinc_target(o, spinc_next(o));
  SpeedToPos(o);
}

static void SpinC_Solid(Obj *o) {
  AnimateSprite(o, ani_get(ANI_SpinConvey));
  if (!o->frame) {
    int16_t x = o->x.w.i;
    SpinC_Platform_Update(o);
    SolidObject(o, 32 / 2 + sonic_solid_width, 14 / 2, 14 / 2 + 1, x);
    return;
  }
  if (o->status & status_on_object) {
    player->status &= (uint8_t)~status_on_object;
    o->status &= (uint8_t)~status_on_object;
    o->routine2 = 0;
  }
  SpinC_Platform_Update(o);
}

void SpinConvey(Obj *o) {
  if (o->routine == 0) {
    if (obSubtype(o) & 0x80) {
      /* a group's spawner: all its platforms placed at once (only once while it is near) */
      uint8_t d0 = obSubtype(o);
      OB(o, spinc_groupid) = d0;
      d0 &= 0x7F;
      if (d0 < sizeof V.obj63) {
        uint8_t old = V.obj63[d0];
        V.obj63[d0] |= 1;
        if (old & 1) {
          DeleteObject(o);
          return;
        }
      }
      const uint16_t *pf = DATAW(((const uint32_t *)DATA(D_PLATFORMS))[(8 + (d0 & 0xF)) & 15]);
      int n = pf[0];
      pf++;
      for (int i = 0; i <= n; i++, pf += 3) {
        Obj *a1 = i ? FindFreeObj() : o;
        if (!a1) continue;
        a1->id = id_SpinConvey;
        a1->x.w.i = (int16_t)pf[0];
        a1->y.w.i = (int16_t)pf[1];
        obSubtype(a1) = (uint8_t)pf[2];
      }
      return;
    }
    o->routine += 2;
    o->map = map_get(MAP_Spin);
    o->gfx = ArtTile_SBZ_Spinning_Platform;
    o->actwid = 32 / 2;
    o->render |= sprite_cam_field;
    o->priority = 4;
    int g = (obSubtype(o) >> 4) & 0xF;
    if (g > 5) g = 5;
    OB(o, spinc_targetdata) = (uint8_t)g;
    OB(o, spinc_count) = (uint8_t)spinc_data[g][0];
    OW(o, spinc_baseX) = (int16_t)spinc_data[g][1];
    OB(o, spinc_posindex) = (uint8_t)((obSubtype(o) & 0xF) << 2);
    OB(o, spinc_increment) = 4;
    uint8_t d1 = OB(o, spinc_posindex);
    if (V.f_conveyrev) {
      OB(o, spinc_reversed) = 1;
      OB(o, spinc_increment) = (uint8_t)-4;
      d1 = spinc_next(o);
    }
    spinc_target(o, d1);
  }
  SpinC_Solid(o);
  uint16_t d0 = (uint16_t)((OW(o, spinc_baseX) & 0xFF80) - ((V.cam[0].x.w.i - 128) & 0xFF80));
  if (d0 > 128 + 320 + 192 && !(V.act == 2 && d0 >= 0xFF80)) {
    uint8_t g = OB(o, spinc_groupid);
    if ((g & 0x80) && (g & 0x7F) < sizeof V.obj63) V.obj63[g & 0x7F] &= (uint8_t)~1;
    DeleteObject(o);
    return;
  }
  DisplaySprite(o);
}

/* ------------------------------------------------------------------ girders */
#define gird_origY 0x30
#define gird_origX 0x32
#define gird_time 0x34
#define gird_set 0x38
#define gird_delay 0x3A

static void Gird_ChgMove(Obj *o) {
  static const int16_t settings[4][3] = {{0x100, 0, 96}, {0, 0x100, 48}, {-0x100, -0x40, 96}, {0, -0x100, 24}};
  const int16_t *a1 = settings[(OB(o, gird_set) & 0x18) >> 3];
  o->vx = a1[0];
  o->vy = a1[1];
  OW(o, gird_time) = a1[2];
  OB(o, gird_set) += 8;
  OW(o, gird_delay) = 7;
}

void Girder(Obj *o) {
  if (o->routine == 0) {
    o->routine += 2;
    o->map = map_get(MAP_Gird);
    o->gfx = ArtTile_SBZ_Girder | Tile_Pal3;
    o->render |= sprite_cam_field;
    o->priority = 4;
    o->actwid = 192 / 2;
    o->height = 48 / 2;
    OW(o, gird_origX) = o->x.w.i;
    OW(o, gird_origY) = o->y.w.i;
    Gird_ChgMove(o);
  }
  int16_t x = o->x.w.i;
  if (!OW(o, gird_delay) || !--OW(o, gird_delay)) {
    SpeedToPos(o);
    if (!--OW(o, gird_time)) Gird_ChgMove(o);
  }
  if (o->render & 0x80)
    SolidObject(o, (int16_t)(o->actwid + sonic_solid_width), o->height, (int16_t)(o->height + 1), x);
  if (obj_far_x(OW(o, gird_origX))) {
    DeleteObject(o);
    return;
  }
  DisplaySprite(o);
}

/* ------------------------------------------------------------------ teleporters */
#define tele_time 0x2E
#define tele_prebump 0x32
#define tele_targetX 0x36
#define tele_targetY 0x38
#define tele_current 0x3A
#define tele_entries 0x3B
#define tele_dataptr 0x3C

/* each tube's bends: their count (in bytes), then x and y words */
static const uint16_t tele_data[8][15] = {
  {1 * 4, 0x794, 0x98C},
  {1 * 4, 0x94, 0x38C},
  {7 * 4, 0x794, 0x2E8, 0x7A4, 0x2C0, 0x7D0, 0x2AC, 0x858, 0x2AC, 0x884, 0x298, 0x894, 0x270, 0x894, 0x190},
  {1 * 4, 0x894, 0x690},
  {7 * 4, 0x1194, 0x470, 0x1184, 0x498, 0x1158, 0x4AC, 0xFD0, 0x4AC, 0xFA4, 0x4C0, 0xF94, 0x4E8, 0xF94, 0x590},
  {1 * 4, 0x1294, 0x490},
  {7 * 4, 0x1594, 0xFFE8, 0x1584, 0xFFC0, 0x1560, 0xFFAC, 0x14D0, 0xFFAC, 0x14A4, 0xFF98, 0x1494, 0xFF70, 0x1494, 0xFD90},
  {1 * 4, 0x894, 0x90},
};

/* divs.w: a long over a word, the quotient in the low word (left as it was when it does not fit) */
static int16_t divs(int32_t n, int16_t d) {
  int32_t q = n / d;
  return q < -32768 || q > 32767 ? (int16_t)n : (int16_t)q;
}

/* Sonic sent straight to the next bend, 16 pixels a frame along the longer way */
static void Tele_NextDirection(Obj *o) {
  Obj *a1 = player;
  int16_t d2 = 0x1000, d3 = 0x1000;
  int16_t dx = (int16_t)(OW(o, tele_targetX) - a1->x.w.i), dy = (int16_t)(OW(o, tele_targetY) - a1->y.w.i);
  uint16_t adx = (uint16_t)dx, ady = (uint16_t)dy;
  if (dx < 0) adx = (uint16_t)-dx, d2 = -0x1000;
  if (dy < 0) ady = (uint16_t)-dy, d3 = -0x1000;
  int16_t t;
  if (ady >= adx) {
    int16_t d1 = divs((int32_t)dy * 65536, d3), d0 = 0;
    if (dx) d0 = divs((int32_t)dx * 65536, d1);
    a1->vx = d0;
    a1->vy = d3;
    t = d1;
  } else {
    int16_t d0 = divs((int32_t)dx * 65536, d2), d1 = 0;
    if (dy) d1 = divs((int32_t)dy * 65536, d0);
    a1->vy = d1;
    a1->vx = d2;
    t = d0;
  }
  if (t < 0) t = (int16_t)-t;
  OB(o, tele_time) = (uint8_t)((uint16_t)t >> 8);
}

void Teleport(Obj *o) {
  Obj *a1 = player;
  const uint16_t *t = &tele_data[OB(o, tele_dataptr)][1];
  switch (o->routine) {
  case 0:
    o->routine += 2;
    OB(o, tele_dataptr) = obSubtype(o) & 7;
    t = &tele_data[OB(o, tele_dataptr)][1];
    OB(o, tele_current) = 0;
    OB(o, tele_entries) = (uint8_t)t[-1];
    OW(o, tele_targetX) = (int16_t)t[0];
    OW(o, tele_targetY) = (int16_t)t[1];
    /* fall through */
  case 2: {
    int16_t d0 = (int16_t)(a1->x.w.i - o->x.w.i);
    if (o->status & 1) d0 = (int16_t)(d0 + 15);
    if ((uint16_t)d0 >= 16) break;
    if ((uint16_t)(a1->y.w.i - o->y.w.i + 32) >= 32 * 2) break;
    if (V.f_playerctrl) break;
    if (obSubtype(o) == 7 && V.rings < 50) break;
    o->routine += 2;
    V.f_playerctrl = 0x81;
    a1->anim = id_Roll;
    a1->inertia = 0x800;
    a1->vx = a1->vy = 0;
    o->status &= (uint8_t)~status_pushing;
    a1->status &= (uint8_t)~status_pushing;
    a1->status |= status_in_air;
    a1->x.w.i = o->x.w.i;
    a1->y.w.i = o->y.w.i;
    OB(o, tele_prebump) = 0;
    break;
  }
  case 4: {
    uint8_t a = OB(o, tele_prebump);
    OB(o, tele_prebump) += 2;
    a1->y.w.i = (int16_t)(o->y.w.i - (CalcSine(a, NULL) >> 5));
    if (OB(o, tele_prebump) == 0x80) {
      Tele_NextDirection(o);
      o->routine += 2;
    }
    break;
  }
  case 6:
    if ((int8_t)--OB(o, tele_time) >= 0) {
      a1->x.l += (int32_t)a1->vx << 8;
      a1->y.l += (int32_t)a1->vy << 8;
      return;
    }
    a1->x.w.i = OW(o, tele_targetX);
    a1->y.w.i = OW(o, tele_targetY);
    {
      uint8_t d1 = (uint8_t)(OB(o, tele_current) + 4);
      if (d1 < OB(o, tele_entries)) {
        OB(o, tele_current) = d1;
        OW(o, tele_targetX) = (int16_t)t[d1 / 2];
        OW(o, tele_targetY) = (int16_t)t[d1 / 2 + 1];
        Tele_NextDirection(o);
        return;
      }
    }
    a1->y.w.i &= 0x7FF;
    o->routine = 0;
    V.f_playerctrl = 0;
    a1->vx = 0;
    a1->vy = 0x200;
    return;
  }
  if (obj_far(o)) DeleteObject(o);
}

/* ------------------------------------------------------------------ Act 2's Dr. Robotnik and his trap */
#define SEgg_ParentObj 0x36
#define SEgg_GenericTimer 0x3C

void ScrapEggman(Obj *o) {
  switch (o->routine) {
  case 0: {
    o->x.w.i = boss_sbz2_x + 0x110;
    o->y.w.i = boss_sbz2_y + 0x94;
    o->coltype = col_48x48 | col_boss;
    obBossHits(o) = 16;
    o->status &= (uint8_t)~1;
    o->routine2 = 0;
    o->routine = 2;
    o->anim = 0;
    o->priority = 3;
    o->map = map_get(MAP_SEgg);
    o->gfx = ArtTile_Eggman;
    o->render = sprite_cam_field | sprite_rendered;
    o->actwid = 64 / 2;
    Obj *a1 = FindNextFreeObj(o);
    if (a1) {
      /* the button he presses */
      OUW(a1, SEgg_ParentObj) = OADDR(o);
      a1->id = id_ScrapEggman;
      a1->x.w.i = boss_sbz2_x + 0xE0;
      a1->y.w.i = boss_sbz2_y + 0xAC;
      a1->routine = 4;
      a1->anim = 0;
      a1->priority = 3;
      a1->map = map_get(MAP_But);
      a1->gfx = ArtTile_Eggman_Button;
      a1->render = sprite_cam_field | sprite_rendered;
      a1->actwid = 32 / 2;
      a1->frame = 0;
    }
  }
    /* fall through */
  case 2:
    switch (o->routine2) {
    case 0:
      if ((uint16_t)(o->x.w.i - player->x.w.i) < 128) {
        o->routine2 += 2;
        OW(o, SEgg_GenericTimer) = 180;
        o->anim = 1;
      }
      break;
    case 2:
      if (!--OW(o, SEgg_GenericTimer)) {
        o->routine2 += 2;
        o->anim = 2;
        o->y.w.i = (int16_t)(o->y.w.i + 4);
        OW(o, SEgg_GenericTimer) = 15;
      }
      break;
    case 4: {
      int16_t t = --OW(o, SEgg_GenericTimer);
      if (t > 0) break;
      if (t == 0) o->vx = -0xFC, o->vy = -0x3C0;
      if (o->x.w.i <= boss_sbz2_x + 0xE2) o->vx = 0;
      o->vy = (int16_t)(o->vy + 0x24);
      if (o->vy >= 0 && (uint16_t)o->y.w.i >= boss_sbz2_y + 0x85) {
        set_subw(o, SW);
        if ((uint16_t)o->y.w.i >= boss_sbz2_y + 0x8B) o->y.w.i = boss_sbz2_y + 0x8B, o->vy = 0;
      }
      if (o->vx | o->vy) break;
      /* landed on the button: the floor goes */
      for (int i = 1; i < 64; i++)
        if (objs[i].id == id_FalseFloor) {
          set_subw(&objs[i], GO);
          o->routine2 += 2;
          o->anim = 1;
          break;
        }
      break;
    }
    }
    SpeedToPos(o);
    AnimateSprite(o, ani_get(ANI_SEgg));
    DisplaySprite(o);
    return;
  case 4:
    if (!o->routine2 && subw(OPTR(OUW(o, SEgg_ParentObj))) == SW) {
      o->frame = 1;
      o->routine2 += 2;
    }
    DisplaySprite(o);
    return;
  }
}

#define FFloor_BlockListStart 0x30

void FalseFloor(Obj *o) {
  switch (o->routine) {
  case 0: {
    o->x.w.i = boss_sbz2_x + 0x30;
    o->y.w.i = boss_sbz2_y + 0xC0;
    o->actwid = 256 / 2;
    o->height = 32 / 2;
    o->render = sprite_cam_field | sprite_rendered;
    int16_t d5 = boss_sbz2_x - 0x40;
    for (int i = 0; i < 8; i++) {
      Obj *a1 = FindFreeObj();
      if (!a1) break;
      OUW(o, FFloor_BlockListStart + 2 * i) = OADDR(a1);
      a1->id = id_FalseFloor;
      a1->map = map_get(MAP_FFloor);
      a1->gfx = ArtTile_Eggman_Trap_Floor | Tile_Pal3;
      a1->render = sprite_cam_field;
      a1->actwid = 32 / 2;
      a1->height = 32 / 2;
      a1->priority = 3;
      a1->x.w.i = d5;
      a1->y.w.i = boss_sbz2_y + 0xC0;
      d5 = (int16_t)(d5 + 0x20);
      a1->routine = 8;
    }
    o->routine += 2;
    return;
  }
  case 2:
    if (subw(o) == GO) {
      o->frame = 0;
      o->routine += 2;
    }
    break;
  case 4: {
    /* the blocks going one after another, from the left */
    uint8_t t = o->timeframe;
    o->timeframe = (uint8_t)(t - 0xE);
    if (t >= 0xE) break;
    uint16_t a = OUW(o, FFloor_BlockListStart + 2 * (o->frame & 7));
    if (a) set_subw(OPTR(a), GO);
    if (++o->frame != 8) break;
  }
    /* fall through */
  case 6:
    o->status &= (uint8_t)~status_on_object;
    player->status &= (uint8_t)~status_on_object;
    DeleteObject(o);
    return;
  case 8:
    if (subw(o) == GO) {
      static const int16_t speed[4] = {0x80, 0, 0x120, 0xC0};
      static const int16_t pos[4][2] = {{-8, -8}, {0x10, 0}, {0, 0x10}, {0x10, 0x10}};
      o->routine += 2;
      o->actwid = 16 / 2;
      o->height = 16 / 2;
      for (int i = 0; i < 4; i++) {
        Obj *a1 = o;
        if (i) {
          a1 = FindNextFreeObj(o);
          if (!a1) break;
          *a1 = *o;
        }
        a1->vy = speed[i];
        a1->x.w.i = (int16_t)(a1->x.w.i + pos[i][0]);
        a1->y.w.i = (int16_t)(a1->y.w.i + pos[i][1]);
        a1->frame = (uint8_t)(i + 1);
      }
    }
    DisplaySprite(o);
    return;
  case 0xA:
    if (!(o->render & 0x80)) {
      DeleteObject(o);
      return;
    }
    ObjectFall(o);
    DisplaySprite(o);
    return;
  }
  /* the floor left: solid, narrowing as it goes */
  int16_t d0 = (int16_t)((8 - (int8_t)o->frame) << 4);
  o->actwid = (uint8_t)d0;
  o->x.w.i = (int16_t)(boss_sbz2_x + 0xB0 - d0);
  SolidObject(o, (int16_t)(sonic_solid_width + d0), 16, 17, o->x.w.i);
}

/* ------------------------------------------------------------------ the Final Zone */
#define BossFinal_ChildCmd 0x29
#define BossFinal_AttackState 0x30
#define BossFinal_EscapeTimer 0x30
#define BossFinal_ChildCounter 0x32
#define BossFinal_Routine 0x34 /* the main object's (its pieces keep their parent's place here) */
#define BossFinal_HitFlash 0x35
#define BossFinal_ParentObj 0x36
#define BossFinal_PlasmaPtr 0x36
#define BossFinal_CylinderPtr 0x38
#define EggmanCylinder_HasEggman 0x30
#define EggmanCylinder_BaseY 0x38
#define EggmanCylinder_Displacement 0x3C
#define BossPlasma_TargetX 0x30
#define BossPlasma_BallsAlive 0x38

static Obj *fz_parent(Obj *o) { return OPTR(OUW(o, BossFinal_ParentObj)); }

/* the main object's last part: the screen let go right, and solid unless flying */
static void BossFinal_EggmanAnimate(Obj *o, bool animate) {
  if (animate) AnimateSprite(o, ani_get(ANI_SEgg));
  if (V.limitright2 < boss_fz_end) V.limitright2 += 2;
  if ((int8_t)OB(o, BossFinal_Routine) < 0xC) SolidObject(o, 32 / 2 + sonic_solid_width, 224 / 2, 226 / 2, o->x.w.i);
}

static void BossFinal_Crush(Obj *o) {
  if (OW(o, BossFinal_AttackState) < 0) {
    /* two cylinders at random: one carries him */
    static const uint16_t pairs[8] = {0, 2, 2, 4, 4, 6, 6, 0};
    OW(o, BossFinal_AttackState) = 0;
    uint32_t r = RandomNumber();
    uint16_t d0 = r & 0xC, d1 = (uint16_t)(d0 + 2);
    if (r & 0x80000000) {
      uint16_t t = d0;
      d0 = d1, d1 = t;
    }
    d0 = pairs[d0 / 2], d1 = pairs[d1 / 2];
    OW(o, BossFinal_AttackState) = (int16_t)d0;
    uint16_t a = OUW(o, BossFinal_CylinderPtr + d0);
    if (a) {
      OB(OPTR(a), BossFinal_ChildCmd) = 0xFF;
      OW(OPTR(a), EggmanCylinder_HasEggman) = -1;
    }
    a = OUW(o, BossFinal_CylinderPtr + d1);
    if (a) {
      OB(OPTR(a), BossFinal_ChildCmd) = 1;
      OW(OPTR(a), EggmanCylinder_HasEggman) = 0;
    }
    OW(o, BossFinal_ChildCounter) = 1;
    OB(o, BossFinal_HitFlash) = 0;
  }
  if (OW(o, BossFinal_ChildCounter) < 0) {
    if (obBossHits(o)) {
      OB(o, BossFinal_Routine) += 2;
      OW(o, BossFinal_AttackState) = -1;
      OW(o, BossFinal_ChildCounter) = 0;
      return;
    }
    AddPoints(100);
    OB(o, BossFinal_Routine) = 6;
    o->x.w.i = boss_fz_x + 0x170;
    o->y.w.i = boss_fz_y + 0x2C;
    o->height = 40 / 2;
    return;
  }
  o->status &= (uint8_t)~1;
  if ((uint16_t)player->x.w.i >= (uint16_t)o->x.w.i) o->status |= 1;
  int d4 = SolidObject(o, 64 / 2 + sonic_solid_width, 40 / 2, 40 / 2, o->x.w.i);
  if (d4 > 0) {
    V.random += 7u << 16;
    if (player->anim == id_Roll) {
      /* rolled into from the side: knocked back, and hit unless still flashing */
      int16_t v = 0x300;
      if (!(o->status & 1)) v = -v;
      player->vx = v;
      if (!OB(o, BossFinal_HitFlash)) {
        obBossHits(o)--;
        OB(o, BossFinal_HitFlash) = 100;
      }
    }
  }
  if (OB(o, BossFinal_HitFlash) && --OB(o, BossFinal_HitFlash)) o->anim = 3;
  else o->anim = 1;
  AnimateSprite(o, ani_get(ANI_SEgg));
}

static void BossFinal_Run(Obj *o) {
  o->status |= 1;
  o->anim = 4;
  SpeedToPos(o);
  o->vy = (int16_t)(o->vy + 0x10);
  if ((uint16_t)o->y.w.i >= boss_fz_y + 0x93) o->vy = -0x40;
  o->vx = 0x400;
  int16_t d = (int16_t)(o->x.w.i - player->x.w.i);
  if (d < 0) o->vx = 0x500;
  else {
    /* slowing as Sonic falls behind */
    static const struct {
      uint16_t gap;
      int16_t slow;
    } steps[7] = {{0x70, 0}, {8, 0x100}, {8, 0x100}, {8, 0x80}, {8, 0x80}, {0x38, 0x80}, {0, 0}};
    uint16_t d0 = (uint16_t)d;
    for (int i = 0; i < 6; i++) {
      if (d0 < steps[i].gap) goto checkpos;
      d0 = (uint16_t)(d0 - steps[i].gap);
      o->vx = (int16_t)(o->vx - steps[i + 1].slow);
    }
    o->vx = 0;
  }
checkpos:
  if ((uint16_t)o->x.w.i >= boss_fz_x + 0x250) {
    o->x.w.i = boss_fz_x + 0x250;
    o->vx = 0x240;
    o->vy = -0x4C0;
    OB(o, BossFinal_Routine) += 2;
  }
  BossFinal_EggmanAnimate(o, true);
}

/* returns false when it has gone (the game on to the ending) */
static bool BossFinal_Eggman(Obj *o) {
  switch (OB(o, BossFinal_Routine)) {
  case 0:
    if (!plc_pending() && (uint16_t)V.cam[0].x.w.i >= boss_fz_x) OB(o, BossFinal_Routine) += 2;
    V.random++;
    break;
  case 2: BossFinal_Crush(o); break;
  case 4: {
    uint16_t a = OUW(o, BossFinal_PlasmaPtr);
    if (OW(o, BossFinal_AttackState) < 0) {
      OW(o, BossFinal_AttackState) = 0;
      if (a) OB(OPTR(a), BossFinal_ChildCmd) = 0xFF;
    }
    if (OW(o, BossFinal_ChildCounter)) {
      OB(o, BossFinal_Routine) -= 2;
      OW(o, BossFinal_AttackState) = -1;
      OW(o, BossFinal_ChildCounter) = 0;
    }
    break;
  }
  case 6:
    /* beaten: out of the cylinder, down to the floor */
    o->actwid = 96 / 2;
    o->status |= 1;
    SpeedToPos(o);
    o->frame = 6;
    o->vy = (int16_t)(o->vy + 0x10);
    if ((uint16_t)o->y.w.i >= boss_fz_y + 0x8C) {
      o->y.w.i = boss_fz_y + 0x8C;
      OB(o, BossFinal_Routine) += 2;
      o->actwid = 64 / 2;
      o->vx = 0x100;
      o->vy = -0x100;
      V.dle_routine += 2;
    }
    BossFinal_EggmanAnimate(o, false);
    break;
  case 8: BossFinal_Run(o); break;
  case 0xA:
    SpeedToPos(o);
    if ((uint16_t)o->x.w.i >= boss_fz_x + 0x290) o->vx = 0;
    o->vy = (int16_t)(o->vy + 0x34);
    if (o->vy >= 0 && (uint16_t)o->y.w.i >= boss_fz_y + 0x82) o->y.w.i = boss_fz_y + 0x82, o->vy = 0;
    if (!(o->vx | o->vy)) {
      OB(o, BossFinal_Routine) += 2;
      o->vy = -0x180;
      obBossHits(o) = 1;
    }
    BossFinal_EggmanAnimate(o, true);
    break;
  case 0xC:
    /* in his ship: one hit more */
    o->map = map_get(MAP_Eggman);
    o->gfx = ArtTile_Eggman;
    o->anim = 0;
    o->status |= 1;
    SpeedToPos(o);
    if ((uint16_t)o->y.w.i < boss_fz_y + 0x34) {
      o->vx = 0x180;
      o->vy = -0x18;
      o->coltype = col_48x48 | col_boss;
      OB(o, BossFinal_Routine) += 2;
    }
    BossFinal_EggmanAnimate(o, true);
    break;
  case 0xE: {
    o->status |= 1;
    SpeedToPos(o);
    bool hover = OW(o, BossFinal_EscapeTimer) != 0;
    if (!hover && !o->coltype) {
      OW(o, BossFinal_EscapeTimer) = 30;
      hover = true;
    }
    if (hover && !--OW(o, BossFinal_EscapeTimer)) {
      if (o->status & 0x80) o->vy = 0x60;
      else o->coltype = col_48x48 | col_boss;
    }
    /* Sonic stopped at the edge, looking up at him */
    bool ledge = true;
    if (player->x.w.i >= boss_fz_end + 0x90) {
      V.f_lockctrl = 1;
      V.jpadhold2 = V.jpadpress2 = 0;
      player->inertia = 0;
      if (o->vy >= 0) ledge = false;
      else V.jpadhold2 = btnUp;
    }
    if (ledge && player->x.w.i >= boss_fz_end + 0xE0) player->x.w.i = boss_fz_end + 0xE0;
    if ((uint16_t)o->x.w.i >= boss_fz_end + 0x200 && !(o->render & 0x80)) {
      V.gamemode = id_Ending;
      DeleteObject(o);
      return false;
    }
    BossFinal_EggmanAnimate(o, true);
    break;
  }
  }
  return true;
}

static void BossFinal_Display(Obj *o) {
  Obj *a1 = fz_parent(o);
  o->status = a1->status;
  o->render = (uint8_t)((o->render & ~3) | (o->status & 3));
  DisplaySprite(o);
}

static void BossFinal_FlamePos(Obj *o) {
  Obj *a1 = fz_parent(o);
  o->x.w.i = a1->x.w.i;
  o->y.w.i = a1->y.w.i;
  BossFinal_Display(o);
}

void BossFinal(Obj *o) {
  switch (o->routine) {
  case 0: {
    static const struct {
      int16_t x, y;
      uint16_t gfx;
      uint8_t map, routine, anim, priority, actwid, height;
    } data[6] = {
      {0x100, 0x100, ArtTile_FZ_Eggman_No_Vehicle, MAP_SEgg, 2, 0, 4, 64 / 2, 50 / 2},
      {boss_fz_x + 0x160, boss_fz_y + 0x80, ArtTile_FZ_Boss, MAP_EggCyl, 4, 0, 1, 36 / 2, 16 / 2},
      {boss_fz_x + 0x290, boss_fz_y + 0x86, ArtTile_FZ_Eggman_Fleeing, MAP_FZLegs, 6, 0, 3, 0, 0},
      {boss_fz_x + 0x290, boss_fz_y + 0x86, ArtTile_FZ_Eggman_No_Vehicle, MAP_SEgg, 8, 0, 3, 0, 0},
      {boss_fz_x + 0x290, boss_fz_y + 0x86, ArtTile_Eggman, MAP_Eggman, 0xA, 0, 3, 64 / 2, 64 / 2},
      {boss_fz_x + 0x290, boss_fz_y + 0x86, ArtTile_Eggman, MAP_Eggman, 0xC, 0, 3, 0, 0},
    };
    for (int i = 0; i < 6; i++) {
      Obj *a1 = o;
      if (i && !(a1 = FindNextFreeObj(o))) break;
      a1->id = id_BossFinal;
      a1->x.w.i = data[i].x;
      a1->y.w.i = data[i].y;
      a1->gfx = data[i].gfx;
      a1->map = map_get(data[i].map);
      a1->routine = data[i].routine;
      a1->anim = data[i].anim;
      a1->priority = data[i].priority;
      a1->actwid = data[i].actwid;
      a1->height = data[i].height;
      a1->render = sprite_cam_field;
      o->render |= sprite_rendered;
      OUW(a1, BossFinal_ParentObj) = OADDR(o);
    }
    Obj *a1 = FindFreeObj();
    if (a1) {
      a1->id = id_BossPlasma;
      OUW(o, BossFinal_PlasmaPtr) = OADDR(a1);
      OUW(a1, BossFinal_ParentObj) = OADDR(o);
      for (int i = 0; i < 4; i++) {
        a1 = FindNextFreeObj(o);
        if (!a1) break;
        OUW(o, BossFinal_CylinderPtr + 2 * i) = OADDR(a1);
        a1->id = id_EggmanCylinder;
        OUW(a1, BossFinal_ParentObj) = OADDR(o);
        obSubtype(a1) = (uint8_t)(2 * i);
      }
    }
    OB(o, BossFinal_Routine) = OB(o, BossFinal_HitFlash) = 0;
    obBossHits(o) = 8;
    OW(o, BossFinal_AttackState) = -1;
  }
    /* fall through */
  case 2:
    if (BossFinal_Eggman(o)) DisplaySprite(o);
    return;
  case 4: /* the control panel in front */
    o->frame = 0xB;
    if ((uint16_t)player->x.w.i >= (uint16_t)o->x.w.i && !(o->render & 0x80)) {
      DeleteObject(o);
      return;
    }
    DisplaySprite(o);
    return;
  case 6: { /* his legs, left behind when he flies off */
    o->status |= 1;
    Obj *a1 = fz_parent(o);
    if (a1->map != map_get(MAP_Eggman)) {
      BossFinal_Display(o);
      return;
    }
    o->x.w.i = a1->x.w.i;
    o->y.w.i = a1->y.w.i;
    if (!o->timeframe) o->timeframe = 0x14;
    if ((int8_t)--o->timeframe <= 0 && (int8_t)++o->frame > 2) {
      DeleteObject(o);
      return;
    }
    BossFinal_FlamePos(o);
    return;
  }
  case 8: { /* his face in the ship */
    Obj *a1 = fz_parent(o);
    if (a1->id != o->id) {
      DeleteObject(o);
      return;
    }
    if (a1->map != map_get(MAP_Eggman)) {
      o->frame = 0xA;
      BossFinal_Display(o);
      return;
    }
    o->anim = 1;
    if ((int8_t)obBossHits(a1) > 0) {
      o->anim = 6;
      o->map = map_get(MAP_Eggman);
      o->gfx = ArtTile_Eggman;
      AnimateSprite(o, ani_get(ANI_Eggman));
    } else {
      if (!(o->render & 0x80)) {
        DeleteObject(o);
        return;
      }
      BossDefeated(o);
      o->priority = 2;
      o->anim = 0;
      o->map = map_get(MAP_FZDamaged);
      o->gfx = ArtTile_FZ_Eggman_Fleeing;
      AnimateSprite(o, ani_get(ANI_FZEgg));
    }
    BossFinal_FlamePos(o);
    return;
  }
  case 0xA: { /* the ship waiting for him */
    o->frame = 0;
    o->status |= 1;
    Obj *a1 = fz_parent(o);
    if (OB(a1, BossFinal_Routine) == 0xC && a1->map == map_get(MAP_Eggman)) {
      DeleteObject(o);
      return;
    }
    BossFinal_Display(o);
    return;
  }
  case 0xC: { /* the ship's flame */
    Obj *a1 = fz_parent(o);
    if (a1->id != o->id) {
      DeleteObject(o);
      return;
    }
    o->anim = 7;
    if ((int8_t)OB(a1, BossFinal_Routine) < 0xC) {
      BossFinal_Display(o);
      return;
    }
    if (a1->vx) o->anim = 0xB;
    AnimateSprite(o, ani_get(ANI_Eggman));
    BossFinal_FlamePos(o);
    return;
  }
  }
}

/* the cylinders that crush from the floor and the ceiling */
void EggmanCylinder(Obj *o) {
  static const int16_t pos[4][2] = {{boss_fz_x + 0x80, boss_fz_y + 0x110},
                                    {boss_fz_x + 0x100, boss_fz_y + 0x110},
                                    {boss_fz_x + 0x40, boss_fz_y - 0x50},
                                    {boss_fz_x + 0xC0, boss_fz_y - 0x50}};
  fix_t *disp = &OFIX(o, EggmanCylinder_Displacement);
  switch (o->routine) {
  case 0: {
    const int16_t *a1 = pos[(obSubtype(o) >> 1) & 3];
    o->render = sprite_cam_field | sprite_rendered | 0x10;
    o->gfx = ArtTile_FZ_Boss;
    o->map = map_get(MAP_EggCyl);
    o->x.w.i = a1[0];
    o->y.w.i = a1[1];
    OFIX(o, EggmanCylinder_BaseY).l = (int32_t)a1[1] << 16;
    o->width = 192 / 2;
    o->actwid = 64 / 2;
    o->height = 192 / 2;
    o->priority = 3;
    o->routine += 2;
  }
    /* fall through */
  case 2:
    if (obSubtype(o) > 2) o->render |= sprite_yflip;
    disp->l = 0;
    if (OB(o, BossFinal_ChildCmd)) o->routine += 2;
    break;
  case 4: {
    Obj *a1 = fz_parent(o);
    bool top = obSubtype(o) > 2;
    int32_t sign = top ? -1 : 1;
    if (top) o->render |= sprite_yflip;
    if (!OB(o, BossFinal_ChildCmd)) {
      /* back in (slower once he is beaten, the explosions going) */
      if (!obBossHits(a1)) {
        BossDefeated(o);
        disp->l -= sign * 0x10000;
      }
      uint32_t old = (uint32_t)disp->l;
      disp->l += sign * 0x20000;
      bool carry = top ? old < 0x20000 : (uint32_t)disp->l < old;
      if (carry) {
        disp->l = 0;
        OW(a1, BossFinal_ChildCounter)--;
        OW(a1, BossFinal_AttackState) = 0;
        o->routine -= 2;
      }
      break;
    }
    /* out: fast, then slowing to its full length */
    if (top ? disp->w.i >= 0x10 : disp->w.i < -0x10) disp->l += sign * -0x28000;
    disp->l += sign * -0x8000;
    if (top ? disp->w.i >= 0xA0 : disp->w.i <= -0xA0) {
      disp->w.f = 0;
      disp->w.i = top ? 0xA0 : -0xA0;
      OB(o, BossFinal_ChildCmd) = 0;
    }
    break;
  }
  }
  /* (the bottom ones push up: their displacement goes negative) */
  int32_t y = OFIX(o, EggmanCylinder_BaseY).l + disp->l;
  int16_t d1 = (int16_t)(y >> 16);
  o->y.w.i = d1;
  if (o->routine == 4 && OW(o, EggmanCylinder_HasEggman) < 0) {
    Obj *a1 = fz_parent(o);
    a1->y.w.i = (int16_t)(d1 + (obSubtype(o) > 2 ? 0xE : -0xA));
    a1->x.w.i = o->x.w.i;
  }
  SolidObject(o, 64 / 2 + sonic_solid_width, 192 / 2, 194 / 2, o->x.w.i);
  uint8_t f = 0;
  int16_t d = disp->w.i;
  if (d < 0) {
    uint16_t n = (uint16_t)-d;
    if (n >= 8) f = (uint8_t)(1 + ((int16_t)(n - 8) >> 4));
  } else if ((uint16_t)d >= 0x27) f = (uint8_t)(1 + ((int16_t)(d - 0x27) >> 4));
  o->frame = f;
  int16_t dx = (int16_t)(player->x.w.i - o->x.w.i);
  if (dx >= 0 && dx - 0x140 >= 0 && !(o->render & 0x80)) {
    DeleteObject(o);
    return;
  }
  DisplaySprite(o);
}

static void BossPlasma_Collision(Obj *o) {
  SolidObject(o, 16 / 2 + sonic_solid_width, 16 / 2, 34 / 2, o->x.w.i);
  int16_t dx = (int16_t)(player->x.w.i - o->x.w.i);
  if (dx >= 0 && dx - 0x140 >= 0 && !(o->render & 0x80)) {
    DeleteObject(o);
    return;
  }
  AnimateSprite(o, ani_get(ANI_PLaunch));
  DisplaySprite(o);
}

/* the plasma launcher, and the balls it sends along the ceiling and down at Sonic */
void BossPlasma(Obj *o) {
  switch (o->routine) {
  case 0:
    o->x.w.i = boss_fz_x + 0x138;
    o->y.w.i = boss_fz_y + 0x2C;
    o->gfx = ArtTile_FZ_Boss;
    o->map = map_get(MAP_PLaunch);
    o->anim = 0;
    o->priority = 3;
    o->width = 16 / 2;
    o->height = 16 / 2;
    o->render = sprite_cam_field | sprite_rendered;
    o->routine += 2;
    /* fall through */
  case 2:
    if (OB(fz_parent(o), BossFinal_Routine) == 6) {
      o->id = id_Explosion;
      o->routine = 0;
      DisplaySprite(o);
      return;
    }
    o->anim = 0;
    if (OB(o, BossFinal_ChildCmd)) {
      o->routine += 2;
      o->anim = 1;
      obSubtype(o) = 62;
    }
    break;
  case 4:
    if (OB(o, BossFinal_ChildCmd)) {
      OB(o, BossFinal_ChildCmd) = 0;
      OW(o, BossPlasma_TargetX) += 4;
      OW(o, BossFinal_ChildCounter) = 0;
      for (int i = 0; i < 4; i++) {
        Obj *a1 = FindNextFreeObj(o);
        if (!a1) break;
        a1->id = id_BossPlasma;
        a1->x.w.i = o->x.w.i;
        a1->y.w.i = boss_fz_y + 0x2C;
        a1->routine = 8;
        a1->gfx = ArtTile_FZ_Boss | Tile_Pal2;
        a1->map = map_get(MAP_Plasma);
        a1->height = 24 / 2;
        a1->width = 24 / 2;
        a1->coltype = col_none;
        a1->priority = 3;
        set_subw(a1, 62);
        a1->render = sprite_cam_field | sprite_rendered;
        OUW(a1, BossFinal_ParentObj) = OADDR(o);
        uint32_t r = RandomNumber();
        int16_t d1 = (int16_t)(OW(o, BossFinal_ChildCounter) * -0x4F + boss_fz_x + 0x128);
        OW(a1, BossPlasma_TargetX) = (int16_t)((int16_t)(r & 0x1F) - 0x10 + d1);
        OW(o, BossFinal_ChildCounter)++;
        OW(o, BossPlasma_BallsAlive) = OW(o, BossFinal_ChildCounter);
      }
    }
    if (!OW(o, BossFinal_ChildCounter)) o->routine += 2;
    break;
  case 6:
    o->anim = 2;
    if (!OW(o, BossPlasma_BallsAlive)) {
      o->routine = 2;
      OW(fz_parent(o), BossFinal_ChildCounter) = -1;
    }
    break;
  case 8: {
    Obj *a1 = fz_parent(o);
    switch (o->routine2) {
    case 0:
      o->vx = (int16_t)((OW(o, BossPlasma_TargetX) - o->x.w.i) << 4);
      set_subw(o, 180);
      o->routine2 += 2;
      break;
    case 2:
      if (o->vx) {
        SpeedToPos(o);
        uint16_t x = (uint16_t)o->x.w.i, t = (uint16_t)OW(o, BossPlasma_TargetX);
        if (x < t) {
          o->vx = 0;
          o->x.w.i = (int16_t)(x + (uint16_t)(x - t));
          OW(a1, BossFinal_ChildCounter)--;
        }
      }
      o->anim = 0;
      set_subw(o, (uint16_t)(subw(o) - 1));
      if (subw(o)) break;
      /* dropping at Sonic */
      o->routine2 += 2;
      o->anim = 1;
      o->coltype = col_24x24 | col_hurt;
      set_subw(o, 180);
      o->vx = (int16_t)(player->x.w.i - o->x.w.i);
      o->vy = 0x140;
      break;
    case 4:
      SpeedToPos(o);
      if ((uint16_t)o->y.w.i >= boss_fz_y + 0xD0 || (set_subw(o, (uint16_t)(subw(o) - 1)), !subw(o))) {
        OW(a1, BossPlasma_BallsAlive)--;
        DeleteObject(o);
        return;
      }
      break;
    }
    AnimateSprite(o, ani_get(ANI_Plasma));
    DisplaySprite(o);
    return;
  }
  }
  BossPlasma_Collision(o);
}
