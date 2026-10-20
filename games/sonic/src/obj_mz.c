/* Marble Zone's objects: lava balls and their launchers (Star Light's fireballs too), grassy
 * platforms that sink into lava and catch fire, green glass pillars, chained spiked stompers,
 * pushable blocks that float on lava (Labyrinth's too), wobbling bricks, lava geysers and the
 * wall of lava that chases Sonic, blocks that break when jumped on, moving blocks (in Labyrinth
 * and Scrap Brain too), invisible lava that hurts; the badniks Basaran and Caterkiller (found in
 * Spring Yard and Scrap Brain too); and Dr. Robotnik's fire-dropping machine. */
#include "s1.h"
#include "collide.h"
#include "game.h"
#include "objs.h"

#define sonic_roll_height 14
#define sonic_roll_width 7

/* ------------------------------------------------------------------ lava balls */
#define lball_fromboss 0x29
#define lball_origY 0x30

void LavaBall(Obj *o);

void LavaMaker(Obj *o) {
  static const uint8_t rates[6] = {30, 60, 90, 120, 150, 180};
  if (o->routine == 0) {
    o->routine += 2;
    o->delayani = rates[(obSubtype(o) >> 4) & 0xF];
    o->timeframe = o->delayani;
    obSubtype(o) &= 0xF;
  }
  if (!--o->timeframe) {
    o->timeframe = o->delayani;
    if (ChkObjectVisible(o)) {
      Obj *a1 = FindFreeObj();
      if (a1) {
        a1->id = id_LavaBall;
        a1->x.w.i = o->x.w.i;
        a1->y.w.i = o->y.w.i;
        obSubtype(a1) = obSubtype(o);
      }
    }
  }
  if (obj_far(o)) DeleteObject(o);
}

void LavaBall(Obj *o) {
  static const int16_t speeds[9] = {-0x400, -0x500, -0x600, -0x700, -0x200, 0x200, -0x200, 0x200, 0};
  switch (o->routine) {
  case 0:
    o->routine += 2;
    o->height = o->width = 16 / 2;
    o->map = map_get(MAP_Fire);
    o->gfx = V.zone == id_SLZ ? ArtTile_SLZ_Fireball : ArtTile_MZ_Fireball;
    o->render = sprite_cam_field;
    o->priority = 3;
    o->coltype = col_16x16 | col_hurt;
    OW(o, lball_origY) = o->y.w.i;
    if (OB(o, lball_fromboss)) o->priority += 2;
    o->vy = speeds[obSubtype(o) < 9 ? obSubtype(o) : 8];
    o->actwid = 16 / 2;
    if (obSubtype(o) >= 6) {
      o->actwid = 32 / 2;
      o->anim = 2;
      o->vx = o->vy;
      o->vy = 0;
    }
    /* fall through */
  case 2: {
    int16_t d1;
    switch (obSubtype(o)) {
    case 0:
    case 1:
    case 2:
    case 3:
      o->vy = (int16_t)(o->vy + 0x18);
      if ((uint16_t)OW(o, lball_origY) < (uint16_t)o->y.w.i) o->routine += 2;
      o->status &= (uint8_t)~2;
      if (o->vy < 0) o->status |= 2;
      break;
    case 4:
      o->status |= 2;
      if (ObjHitCeiling(o, NULL) < 0) obSubtype(o) = 8, o->anim = 1, o->vy = 0;
      break;
    case 5:
      o->status &= (uint8_t)~2;
      if (ObjFloorDist(o, NULL) < 0) obSubtype(o) = 8, o->anim = 1, o->vy = 0;
      break;
    case 6:
      o->status |= 1;
      d1 = ObjHitWallLeft(o, -8, NULL);
      if (d1 < 0) obSubtype(o) = 8, o->anim = 3, o->vx = 0;
      break;
    case 7:
      o->status &= (uint8_t)~1;
      d1 = ObjHitWallRight(o, 8, NULL);
      if (d1 < 0) obSubtype(o) = 8, o->anim = 3, o->vx = 0;
      break;
    }
    SpeedToPos(o);
    AnimateSprite(o, ani_get(ANI_Fire));
    if (obj_far(o)) {
      DeleteObject(o);
      return;
    }
    break;
  }
  default: DeleteObject(o); return;
  }
  DisplaySprite(o);
}

/* ------------------------------------------------------------------ grassy platforms and their fire */
#define lgrass_origX 0x2A
#define lgrass_origY 0x2C
#define lgrass_coldata 0x30
#define lgrass_nudge 0x34
#define lgrass_burning 0x35
#define lgrass_flames 0x36
#define gfire_origX 0x2A
#define gfire_origY 0x2C
#define gfire_coldata 0x30
#define gfire_platform 0x38
#define gfire_nudge 0x3C

static uint8_t lgrass_heights[3][76];

static const uint8_t *lgrass_data(int k) {
  if (!lgrass_heights[0][0]) {
    uint8_t *p = lgrass_heights[0];
    for (int i = 0; i < 14; i++) *p++ = 0x20;
    for (int v = 0x21; v <= 0x2F; v++) *p++ = (uint8_t)v;
    for (int i = 0; i < 18; i++) *p++ = 0x30;
    for (int v = 0x2F; v >= 0x21; v--) *p++ = (uint8_t)v;
    for (int i = 0; i < 14; i++) *p++ = 0x20;
    p = lgrass_heights[1];
    for (int i = 0; i < 6; i++) *p++ = 0x20;
    for (int v = 0x21; v <= 0x3F; v++) *p++ = (uint8_t)v;
    for (int i = 0; i < 18; i++) *p++ = 0x40;
    for (int v = 0x3F; v >= 0x31; v--) *p++ = (uint8_t)v;
    for (int i = 0; i < 6; i++) *p++ = 0x30;
    p = lgrass_heights[2];
    for (int i = 0; i < 44; i++) *p++ = 0x30;
  }
  return lgrass_heights[k];
}

static void LGrass_AddChildToList(Obj *platform, Obj *a1) {
  uint8_t n = OB(platform, lgrass_flames)++;
  OB(platform, lgrass_flames + 1 + n) = SLOT(a1);
}

static void LGrass_Types(Obj *o) {
  uint8_t d0;
  int16_t d1;
  switch (obSubtype(o) & 7) {
  case 1: d0 = OSC_B(0x2), d1 = 0x20; break;
  case 2: d0 = OSC_B(0x6), d1 = 0x30; break;
  case 3: d0 = OSC_B(0xA), d1 = 0x40; break;
  case 4: d0 = OSC_B(0xE), d1 = 0x60; break;
  case 5: {
    uint8_t n = OB(o, lgrass_nudge);
    if (!o->routine2) n = n >= 2 ? (uint8_t)(n - 2) : 0;
    else {
      n = (uint8_t)(n + 4);
      if (n >= 0x40) n = 0x40;
    }
    OB(o, lgrass_nudge) = n;
    int16_t s = (int16_t)((uint16_t)CalcSine(n, NULL) >> 4);
    o->y.w.i = (int16_t)(s + OW(o, lgrass_origY));
    if (n == 0x20 && !OB(o, lgrass_burning)) {
      OB(o, lgrass_burning) = 1;
      Obj *a1 = FindNextFreeObj(o);
      if (a1) {
        a1->id = id_GrassFire;
        a1->x.w.i = (int16_t)(o->x.w.i - 128 / 2);
        OW(a1, gfire_origY) = (int16_t)(OW(o, lgrass_origY) + 8 - 3);
        OB(a1, gfire_coldata) = OB(o, lgrass_coldata);
        OUW(a1, gfire_platform) = OADDR(o);
        LGrass_AddChildToList(o, a1);
      }
    }
    int n2 = OB(o, lgrass_flames);
    for (int i = 0; i < n2; i++) OW(OBJ(OB(o, lgrass_flames + 1 + i)), gfire_nudge) = s;
    return;
  }
  default: return;
  }
  int16_t v = d0;
  if (obSubtype(o) & 8) v = (int16_t)(-v + d1);
  o->y.w.i = (int16_t)(OW(o, lgrass_origY) - v);
}

void LargeGrass(Obj *o) {
  static const struct {
    uint8_t data, frame, actwid;
  } info[3] = {{0, 0, 128 / 2}, {1, 1, 128 / 2}, {2, 2, 64 / 2}};
  if (o->routine == 0) {
    o->routine += 2;
    o->map = map_get(MAP_LGrass);
    o->gfx = ArtTile_Level | Tile_Pal3 | Tile_Prio;
    o->render = sprite_cam_field;
    o->priority = 5;
    OW(o, lgrass_origY) = o->y.w.i;
    OW(o, lgrass_origX) = o->x.w.i;
    int k = ((obSubtype(o) >> 2) & 0x1C) >> 2;
    if (k > 2) k = 2;
    OB(o, lgrass_coldata) = info[k].data;
    o->frame = info[k].frame;
    o->actwid = info[k].actwid;
    obSubtype(o) &= 0xF;
    o->height = 128 / 2;
    o->render |= 0x10;
  }
  LGrass_Types(o);
  const uint8_t *heights = lgrass_data(OB(o, lgrass_coldata));
  int16_t d1 = (int16_t)(o->actwid + sonic_solid_width);
  if (o->routine2) {
    ExitPlatform(o, d1);
    if (player->status & status_on_object) SlopeObject_AssumeStoodOn(o, d1, heights, o->x.w.i);
    else o->routine2 = 0;
  } else SolidObject_Heightmap(o, d1, o->frame == 2 ? 96 / 2 : 64 / 2, heights);
  DisplaySprite(o);
  if (OB(o, lgrass_burning) && !(o->render & 0x80)) {
    int n = OB(o, lgrass_flames);
    OB(o, lgrass_flames) = 0;
    for (int i = 0; i < n; i++) {
      DeleteObject(OBJ(OB(o, lgrass_flames + 1 + i)));
      OB(o, lgrass_flames + 1 + i) = 0;
    }
    OB(o, lgrass_burning) = 0;
    OB(o, lgrass_nudge) = 0;
    return;
  }
  if (obj_far_x(OW(o, lgrass_origX))) DeleteObject(o);
}

void GrassFire(Obj *o) {
  switch (o->routine) {
  case 0:
    o->routine += 2;
    o->map = map_get(MAP_Fire);
    o->gfx = ArtTile_MZ_Fireball;
    OW(o, gfire_origX) = o->x.w.i;
    o->render = sprite_cam_field;
    o->priority = 1;
    o->coltype = col_16x16 | col_hurt;
    o->actwid = 16 / 2;
    if (obSubtype(o)) {
      o->routine += 2;
      goto move;
    }
    /* fall through */
  case 2: {
    const uint8_t *heights = lgrass_data(OB(o, gfire_coldata));
    int16_t d1 = (int16_t)(o->x.w.i - OW(o, gfire_origX) + 12);
    int16_t d0 = (int16_t)(OW(o, gfire_origY) - heights[(uint16_t)d1 >> 1]);
    int16_t d2 = d0;
    o->y.w.i = (int16_t)(d0 + OW(o, gfire_nudge));
    if ((uint16_t)d1 < 132) {
      o->x.l += 0x10000;
      if ((uint16_t)d1 < 128 && !((o->x.l + 0x80000) & 0xFFFFF)) {
        Obj *a1 = FindNextFreeObj(o);
        if (a1) {
          a1->id = id_GrassFire;
          a1->x.w.i = o->x.w.i;
          OW(a1, gfire_origY) = d2;
          OW(a1, gfire_nudge) = OW(o, gfire_nudge);
          obSubtype(a1) = 1;
          LGrass_AddChildToList(OPTR(OUW(o, gfire_platform)), a1);
        }
      }
    }
    break;
  }
  case 4:
  move:
    o->y.w.i = (int16_t)(OW(o, gfire_origY) + OW(o, gfire_nudge));
    break;
  }
  AnimateSprite(o, ani_get(ANI_GFire));
  DisplaySprite(o);
}

/* ------------------------------------------------------------------ glass pillars */
#define glass_origY 0x30
#define glass_distanceY 0x32
#define glass_switch_flag 0x34
#define glass_stomp_flags 0x34
#define glass_stomp_first 0x35
#define glass_stomp_distance 0x36
#define glass_stomp_delay 0x38
#define glass_parent 0x3C

static void Glass_Types(Obj *o) {
  int16_t d0;
  switch (obSubtype(o) & 7) {
  case 1:
  case 2:
    d0 = OSC_B(0x12);
    if ((obSubtype(o) & 7) == 2) d0 = (int16_t)(-d0 + 64);
    if (obSubtype(o) & 8) d0 = (int16_t)((uint8_t)(-d0 + 64) >> 1) + 32;
    break;
  case 3:
    if (obSubtype(o) & 8) {
      d0 = (int16_t)(OSC_B(0x12) - 16);
      break;
    }
    if (!(o->status & status_on_object)) OB(o, glass_stomp_flags) &= (uint8_t)~1;
    else if (!OB(o, glass_stomp_flags)) {
      OB(o, glass_stomp_flags) = 1;
      uint8_t first = OB(o, glass_stomp_first);
      OB(o, glass_stomp_first) |= 1;
      if (first & 1) {
        OB(o, glass_stomp_flags) |= 0x80;
        OW(o, glass_stomp_distance) = 16;
        OB(o, glass_stomp_delay) = 10;
        if (OW(o, glass_distanceY) == 64) OW(o, glass_stomp_distance) = 64;
      }
    }
    if ((OB(o, glass_stomp_flags) & 0x80) && (!OB(o, glass_stomp_delay) || !--OB(o, glass_stomp_delay))) {
      if (!OW(o, glass_distanceY)) OB(o, glass_stomp_flags) &= 0x7F;
      else {
        OW(o, glass_distanceY)--;
        if (!--OW(o, glass_stomp_distance)) OB(o, glass_stomp_flags) &= 0x7F;
      }
    }
    d0 = OW(o, glass_distanceY);
    break;
  case 4:
    if (obSubtype(o) & 8) {
      d0 = (int16_t)(OSC_B(0x12) - 16);
      break;
    }
    if (!OB(o, glass_switch_flag) && V.f_switch[obSubtype(o) >> 4]) OB(o, glass_switch_flag) = 1;
    if (OB(o, glass_switch_flag) && OW(o, glass_distanceY)) OW(o, glass_distanceY) -= 2;
    d0 = OW(o, glass_distanceY);
    break;
  default: return;
  }
  o->y.w.i = (int16_t)(OW(o, glass_origY) - d0);
}

void GlassBlock(Obj *o) {
  static const uint8_t vars1[2][3] = {{2, 0, 0}, {4, 0, 1}}, vars2[2][3] = {{6, 0, 2}, {8, 0, 1}};
  switch (o->routine) {
  case 0: {
    const uint8_t(*v)[3] = vars1;
    o->height = 144 / 2;
    if (obSubtype(o) >= 3) v = vars2, o->height = 112 / 2;
    Obj *a1 = o;
    int i;
    for (i = 0; i < 2; i++) {
      if (i) {
        a1 = FindNextFreeObj(o);
        if (!a1) break;
      }
      a1->routine = v[i][0];
      a1->id = id_GlassBlock;
      a1->x.w.i = o->x.w.i;
      a1->y.w.i = (int16_t)(o->y.w.i + (int8_t)v[i][1]);
      a1->map = map_get(MAP_Glass);
      a1->gfx = ArtTile_MZ_Glass_Pillar | Tile_Pal3 | Tile_Prio;
      a1->render = sprite_cam_field;
      OW(a1, glass_origY) = a1->y.w.i;
      obSubtype(a1) = obSubtype(o);
      a1->actwid = 64 / 2;
      a1->priority = 4;
      a1->frame = v[i][2];
      OUW(a1, glass_parent) = OADDR(o);
    }
    if (i == 2) {
      a1->actwid = 32 / 2;
      a1->priority = 3;
      obSubtype(a1) = (uint8_t)((obSubtype(a1) + 8) & 0xF);
    }
    OW(o, glass_distanceY) = 144;
    o->render |= 0x10;
  }
    /* fall through */
  case 2:
    Glass_Types(o);
    SolidObject(o, 64 / 2 + sonic_solid_width, 144 / 2, 146 / 2, o->x.w.i);
    break;
  case 4:
    OW(o, glass_distanceY) = OW(OPTR(OUW(o, glass_parent)), glass_distanceY);
    Glass_Types(o);
    break;
  case 6:
    Glass_Types(o);
    SolidObject(o, 64 / 2 + sonic_solid_width, 112 / 2, 114 / 2, o->x.w.i);
    break;
  case 8: {
    Obj *a1 = OPTR(OUW(o, glass_parent));
    OW(o, glass_distanceY) = OW(a1, glass_distanceY);
    OW(o, glass_origY) = a1->y.w.i;
    Glass_Types(o);
    break;
  }
  }
  if (obj_far(o)) {
    DeleteObject(o);
    return;
  }
  DisplaySprite(o);
}

/* ------------------------------------------------------------------ chained stompers */
#define cstom_origY 0x30
#define cstom_current 0x32
#define cstom_length 0x34
#define cstom_rising 0x36
#define cstom_delay 0x38
#define cstom_switch 0x3A
#define cstom_parent 0x3C

static void CStom_UpdateBlockY(Obj *o) { o->y.w.i = (int16_t)(OWH(o, cstom_current) + OW(o, cstom_origY)); }

/* falling to its length; true when it lands */
static bool cstom_fall(Obj *o) {
  uint16_t d1 = OUW(o, cstom_length);
  if (d1 == OUW(o, cstom_current)) return false;
  int16_t d0 = o->vy;
  o->vy = (int16_t)(o->vy + 0x70);
  OUW(o, cstom_current) = (uint16_t)(OUW(o, cstom_current) + d0);
  if (d1 > OUW(o, cstom_current)) return false;
  OUW(o, cstom_current) = d1;
  o->vy = 0;
  return true;
}

static void CStom_Types(Obj *o) {
  switch (obSubtype(o) & 0xF) {
  case 0:
    if (V.f_switch[OB(o, cstom_switch)]) {
      if ((V.obj31ypos >= 0 || OWH(o, cstom_current) != 0x10) && OUW(o, cstom_current)) {
        if (OUW(o, cstom_current) >= 0x80) {
          OUW(o, cstom_current) -= 0x80;
          break;
        }
        OUW(o, cstom_current) = 0;
      }
      o->vy = 0;
      break;
    }
    cstom_fall(o);
    break;
  case 1:
  case 2:
  case 4:
  case 6:
    if (OW(o, cstom_rising)) {
      if (OW(o, cstom_delay)) {
        OW(o, cstom_delay)--;
        break;
      }
      if (OUW(o, cstom_current) >= 0x80) {
        OUW(o, cstom_current) -= 0x80;
        break;
      }
      OUW(o, cstom_current) = 0;
      o->vy = 0;
      OW(o, cstom_rising) = 0;
      break;
    }
    if (cstom_fall(o)) {
      OW(o, cstom_rising) = 1;
      OW(o, cstom_delay) = 60;
    }
    break;
  case 3:
  case 5: {
    int16_t d0 = (int16_t)(player->x.w.i - o->x.w.i);
    if ((uint16_t)player->x.w.i < (uint16_t)o->x.w.i) d0 = (int16_t)-d0;
    if ((uint16_t)d0 < 144) obSubtype(o)++;
    break;
  }
  }
  CStom_UpdateBlockY(o);
}

void ChainStomp(Obj *o) {
  static const uint8_t var[4][3] = {{2, 0, 0}, {4, 0x1C, 1}, {8, 0xCC, 3}, {6, 0xF0, 2}};
  static const uint16_t lengths[7] = {0x7000, 0xA000, 0x5000, 0x7800, 0x3800, 0x5800, 0xB800};
  static const uint8_t var2[3][2] = {{0x70 / 2, 0}, {0x60 / 2, 9}, {0x20 / 2, 0xA}};
  switch (o->routine) {
  case 0: {
    uint8_t d0 = obSubtype(o);
    if (d0 & 0x80) {
      static const uint8_t swch[2][2] = {{0, 0}, {1, 0}};
      int k = d0 & 0x7F;
      OB(o, cstom_switch) = swch[k & 1][0];
      d0 = swch[k & 1][1];
      obSubtype(o) = d0;
    }
    int idx = d0 & 0xF;
    uint16_t d2 = lengths[idx < 7 ? idx : 6];
    if (!idx) OUW(o, cstom_current) = d2;
    Obj *a1 = o;
    int vi = 0, d1 = 3;
    goto make;
    for (;;) {
      if (d1-- == 0) {
        a1->priority = 3;
        break;
      }
      a1 = FindNextFreeObj(o);
      if (!a1) goto setup_main;
    make:
      a1->routine = var[vi][0];
      a1->id = id_ChainStomp;
      a1->x.w.i = o->x.w.i;
      a1->y.w.i = (int16_t)(o->y.w.i + (int8_t)var[vi][1]);
      a1->map = map_get(MAP_CSto);
      a1->gfx = ArtTile_MZ_Spike_Stomper;
      a1->render = sprite_cam_field;
      OW(a1, cstom_origY) = a1->y.w.i;
      obSubtype(a1) = obSubtype(o);
      a1->actwid = 32 / 2;
      OUW(a1, cstom_length) = d2;
      a1->priority = 4;
      a1->frame = var[vi][2];
      vi++;
      if (a1->frame == 1) {
        d1--;
        if ((obSubtype(o) & 0xF0) == 0x20) goto make;
        a1->actwid = 112 / 2;
        a1->coltype = col_80x32 | col_hurt;
        d1++;
      }
      OUW(a1, cstom_parent) = OADDR(o);
    }
  setup_main: {
    int k = ((obSubtype(o) >> 3) & 0xE) >> 1;
    o->actwid = var2[k < 3 ? k : 2][0];
    o->frame = var2[k < 3 ? k : 2][1];
  }
  }
    /* fall through */
  case 2:
    CStom_Types(o);
    V.obj31ypos = o->y.w.i;
    SolidObject(o, (int16_t)(o->actwid + sonic_solid_width), 24 / 2, 26 / 2, o->x.w.i);
    if ((o->status & status_on_object) && OWH(o, cstom_current) < 0x10) KillSonic(player, o);
    DisplaySprite(o);
    if (obj_far(o)) DeleteObject(o);
    return;
  case 8: {
    o->height = 256 / 2;
    o->render |= 0x10;
    Obj *a1 = OPTR(OUW(o, cstom_parent));
    o->frame = (uint8_t)((OWH(a1, cstom_current) >> 5) + 3);
  }
    /* fall through */
  case 4:
    o->y.w.i = (int16_t)(OWH(OPTR(OUW(o, cstom_parent)), cstom_current) + OW(o, cstom_origY));
    /* fall through */
  case 6:
    DisplaySprite(o);
    if (obj_far(o)) DeleteObject(o);
    return;
  }
}

/* ------------------------------------------------------------------ pushable blocks */
#define pblock_lavaspeed 0x30
#define pblock_onlava 0x32
#define pblock_origX 0x34
#define pblock_origY 0x36
#define gmake_parent 0x3C

static void pblock_landed(Obj *o) {
  if ((col_tile & 0x3FF) < 0x16A) return;
  o->vx = (int16_t)(OW(o, pblock_lavaspeed) >> 3);
  OB(o, pblock_onlava) = 1;
  o->y.w.f = 0;
}

static void PushB_SolidAction(Obj *o, int16_t d1, int16_t d2, int16_t d3, int16_t d4) {
  Obj *a1 = player;
  switch (o->routine2) {
  case 2:
    ExitPlatform(o, d1);
    if (a1->status & status_on_object) MvSonicOnPtfm(o, d4, d3);
    else o->routine2 = 0;
    return;
  case 4: {
    SpeedToPos(o);
    o->vy = (int16_t)(o->vy + 0x18);
    int16_t f = ObjFloorDist(o, NULL);
    if (f >= 0) return;
    o->y.w.i = (int16_t)(o->y.w.i + f);
    o->vy = 0;
    o->routine2 = 0;
    pblock_landed(o);
    return;
  }
  case 6:
    SpeedToPos(o);
    if (o->x.w.i & 0xC) return;
    o->x.w.i &= (int16_t)0xFFF0;
    OW(o, pblock_lavaspeed) = o->vx;
    o->vx = 0;
    o->routine2 -= 2;
    return;
  }
  int r = Solid_ChkCollision(o, d1, d2);
  if (r <= 0 || OB(o, pblock_onlava)) return;
  int16_t d0 = solid_d0, dx, speed;
  if (!d0) return;
  if (d0 > 0) {
    if (a1->status & 1) return;
    if (ObjHitWallRight(o, o->actwid, NULL) < 0) return;
    o->x.l += 0x10000;
    dx = 1, speed = 0x40;
  } else {
    if (!(a1->status & 1)) return;
    if (ObjHitWallLeft(o, (int16_t)~o->actwid, NULL) < 0) return;
    o->x.l -= 0x10000;
    dx = -1, speed = -0x40;
  }
  a1 = player;
  a1->x.w.i = (int16_t)(a1->x.w.i + dx);
  a1->inertia = speed;
  a1->vx = 0;
  if (obSubtype(o) & 0x80) return;
  int16_t f = ObjFloorDist(o, NULL);
  if (f > 4) {
    o->vx = dx > 0 ? 0x400 : -0x400;
    o->routine2 = 6;
    return;
  }
  o->y.w.i = (int16_t)(o->y.w.i + f);
}

static void PushB_SpawnLavaGeysers(Obj *o) {
  int16_t d2;
  if (ZONE_ACT == id_MZ_act2) {
    d2 = -32;
    if (o->x.w.i != 0xDD0 && o->x.w.i != 0xCC0 && o->x.w.i != 0xBA0) return;
  } else if (ZONE_ACT == id_MZ_act3) {
    d2 = 32;
    if (o->x.w.i != 0x560 && o->x.w.i != 0x5C0) return;
  } else return;
  Obj *a1 = FindFreeObj();
  if (!a1) return;
  a1->id = id_GeyserMaker;
  a1->x.w.i = (int16_t)(o->x.w.i + d2);
  a1->y.w.i = (int16_t)(o->y.w.i + 16);
  OUW(a1, gmake_parent) = OADDR(o);
}

/* back where it started once that place is off screen */
static void PushB_ChkVisible(Obj *o) {
  if (ChkPartiallyVisible(o)) return;
  o->routine = 2;
  OB(o, pblock_onlava) = 0;
  o->vx = o->vy = 0;
}

static void PushB_ChkWithinOrigin(Obj *o) {
  if (obj_far_x(OW(o, pblock_origX))) {
    if (o->respawn) V.objstate[2 + o->respawn] &= (uint8_t)~1;
    DeleteObject(o);
    return;
  }
  o->x.w.i = OW(o, pblock_origX);
  o->y.w.i = OW(o, pblock_origY);
  o->routine = 4;
  PushB_ChkVisible(o);
}

static void PushB_Display(Obj *o) {
  if (obj_far(o)) {
    PushB_ChkWithinOrigin(o);
    return;
  }
  DisplaySprite(o);
}

void PushBlock(Obj *o) {
  static const uint8_t var[2][2] = {{32 / 2, 0}, {128 / 2, 1}};
  switch (o->routine) {
  case 0: {
    o->routine += 2;
    o->height = o->width = 30 / 2;
    o->map = map_get(MAP_Push);
    o->gfx = ArtTile_MZ_Block | Tile_Pal3;
    if (V.zone == id_LZ) o->gfx = ArtTile_LZ_Push_Block | Tile_Pal3;
    o->render = sprite_cam_field;
    o->priority = 3;
    OW(o, pblock_origX) = o->x.w.i;
    OW(o, pblock_origY) = o->y.w.i;
    int k = obSubtype(o) & 7;
    o->actwid = var[k & 1][0];
    o->frame = var[k & 1][1];
    if (obSubtype(o)) o->gfx = ArtTile_MZ_Block | Tile_Pal3 | Tile_Prio;
    if (o->respawn) {
      uint8_t *st = &V.objstate[2 + o->respawn];
      *st &= 0x7F;
      uint8_t old = *st;
      *st |= 1;
      if (old & 1) {
        DeleteObject(o);
        return;
      }
    }
  }
    /* fall through */
  case 2:
    if (OB(o, pblock_onlava)) {
      int16_t x = o->x.w.i;
      if (o->routine2 < 4) SpeedToPos(o);
      if (o->status & status_in_air) {
        o->vy = (int16_t)(o->vy + 0x18);
        int16_t f = ObjFloorDist(o, NULL);
        if (f < 0) {
          o->y.w.i = (int16_t)(o->y.w.i + f);
          o->vy = 0;
          o->status &= (uint8_t)~status_in_air;
          pblock_landed(o);
        }
      } else if (o->vx) {
        int16_t d1 = o->vx > 0 ? ObjHitWallRight(o, o->actwid, NULL) : ObjHitWallLeft(o, (int16_t)~o->actwid, NULL);
        if (d1 < 0) o->vx = 0;
      } else {
        o->y.l += 0x2000 + 1;
        if ((uint8_t)o->y.w.f >= 160) {
          player->status &= (uint8_t)~status_on_object;
          o->status &= (uint8_t)~status_on_object;
          PushB_ChkWithinOrigin(o);
          return;
        }
      }
      PushB_SolidAction(o, (int16_t)(o->actwid + sonic_solid_width), 32 / 2, 34 / 2, x);
      PushB_SpawnLavaGeysers(o);
      PushB_Display(o);
      return;
    }
    PushB_SolidAction(o, (int16_t)(o->actwid + sonic_solid_width), 32 / 2, 34 / 2, o->x.w.i);
    if (ZONE_ACT == id_MZ_act1) {
      obSubtype(o) &= 0x7F;
      if ((uint16_t)o->x.w.i >= 0xA20 && (uint16_t)o->x.w.i < 0xAA1) {
        o->y.w.i = (int16_t)(V.obj31ypos - 0x1C);
        V.obj31ypos = (int16_t)(V.obj31ypos | (int16_t)0x8000);
        obSubtype(o) |= 0x80;
      }
    }
    PushB_Display(o);
    return;
  case 4: PushB_ChkVisible(o); return;
  }
}

/* ------------------------------------------------------------------ bricks */
#define brick_origY 0x30

void MarbleBrick(Obj *o) {
  if (o->routine == 0) {
    o->routine += 2;
    o->height = o->width = 30 / 2;
    o->map = map_get(MAP_Brick);
    o->gfx = ArtTile_Level | Tile_Pal3;
    o->render = sprite_cam_field;
    o->priority = 3;
    o->actwid = 32 / 2;
    OW(o, brick_origY) = o->y.w.i;
    OW(o, 0x32) = 0x5C0;
  }
  if (o->render & 0x80) {
    switch (obSubtype(o) & 7) {
    case 2: {
      int16_t d0 = (int16_t)(player->x.w.i - o->x.w.i);
      if ((uint16_t)player->x.w.i < (uint16_t)o->x.w.i) d0 = (int16_t)-d0;
      if ((uint16_t)d0 < 0x90) obSubtype(o) = 3;
    }
      /* fall through */
    case 1: {
      int16_t d0 = OSC_B(0x16);
      if (obSubtype(o) & 8) d0 = (int16_t)(-d0 + 0x10);
      o->y.w.i = (int16_t)(OW(o, brick_origY) - d0);
      break;
    }
    case 3: {
      SpeedToPos(o);
      o->vy = (int16_t)(o->vy + 0x18);
      int16_t f = ObjFloorDist(o, NULL);
      if (f >= 0) break;
      o->y.w.i = (int16_t)(o->y.w.i + f);
      o->vy = 0;
      OW(o, brick_origY) = o->y.w.i;
      obSubtype(o) = 4;
      if ((col_tile & 0x3FF) < 0x16A) obSubtype(o) = 0;
      break;
    }
    case 4: o->y.w.i = (int16_t)(OW(o, brick_origY) - (OSC_B(0x12) >> 3)); break;
    }
    SolidObject(o, 32 / 2 + sonic_solid_width, 32 / 2, 34 / 2, o->x.w.i);
  }
  if (obj_far(o)) {
    DeleteObject(o);
    return;
  }
  DisplaySprite(o);
}

/* ------------------------------------------------------------------ lava geysers */
#define gmake_timer 0x32
#define gmake_time 0x34
#define geyser_origY 0x30

void GeyserMaker(Obj *o) {
  switch (o->routine) {
  case 0:
    o->routine += 2;
    o->map = map_get(MAP_Geyser);
    o->gfx = ArtTile_MZ_Lava | Tile_Pal4 | Tile_Prio;
    o->render = sprite_cam_field;
    o->priority = 1;
    o->actwid = 112 / 2;
    OW(o, gmake_time) = 120;
    /* fall through */
  case 2:
    if (--OW(o, gmake_timer) < 0) {
      OW(o, gmake_timer) = OW(o, gmake_time);
      uint16_t py = (uint16_t)player->y.w.i, y = (uint16_t)o->y.w.i;
      if (py < y && py >= (uint16_t)(y - 0x170)) o->routine += 2;
    }
    break;
  case 4:
    if (obSubtype(o)) {
      o->routine += 2;
      break;
    }
    goto display;
  case 6: {
    o->routine += 2;
    Obj *a1 = FindNextFreeObj(o);
    if (a1) {
      a1->id = id_LavaGeyser;
      a1->x.w.i = o->x.w.i;
      a1->y.w.i = o->y.w.i;
      obSubtype(a1) = obSubtype(o);
      OUW(a1, gmake_parent) = OADDR(o);
    }
    o->anim = 1;
    if (obSubtype(o)) o->anim = 4;
    else {
      Obj *p = OPTR(OUW(o, gmake_parent));
      p->status |= status_in_air;
      p->vy = -0x580;
    }
    goto display;
  }
  case 8:
  display:
    AnimateSprite(o, ani_get(ANI_Geyser));
    DisplaySprite(o);
    break;
  case 0xA:
    o->anim = 0;
    o->routine = 2;
    if (!obSubtype(o)) {
      DeleteObject(o);
      return;
    }
    break;
  }
  if (obj_far(o)) DeleteObject(o);
}

static Obj *geyser_piece(Obj *o, Obj *a1) {
  a1->id = id_LavaGeyser;
  a1->map = map_get(MAP_Geyser);
  a1->gfx = ArtTile_MZ_Lava | Tile_Pal4;
  a1->render = sprite_cam_field;
  a1->actwid = 64 / 2;
  a1->x.w.i = o->x.w.i;
  a1->y.w.i = o->y.w.i;
  obSubtype(a1) = obSubtype(o);
  a1->priority = 1;
  a1->anim = 5;
  if (obSubtype(o)) a1->anim = 2;
  return a1;
}

void LavaGeyser(Obj *o) {
  switch (o->routine) {
  case 0: {
    o->routine += 2;
    OW(o, geyser_origY) = o->y.w.i;
    if (obSubtype(o)) o->y.w.i = (int16_t)(o->y.w.i - 0x250);
    o->vy = obSubtype(o) ? 0 : -0x500;
    Obj *a1 = geyser_piece(o, o);
    Obj *n = FindNextFreeObj(o);
    if (n) a1 = geyser_piece(o, n);
    a1->y.w.i = (int16_t)(a1->y.w.i + 0x60);
    OW(a1, geyser_origY) = (int16_t)(OW(o, geyser_origY) + 0x60);
    a1->coltype = col_64x224 | col_hurt;
    a1->height = 256 / 2;
    a1->render |= 0x10;
    a1->routine += 4;
    OUW(a1, gmake_parent) = OADDR(o);
    if (obSubtype(o)) {
      n = FindNextFreeObj(o);
      if (n) a1 = geyser_piece(o, n);
      a1->routine += 2;
      a1->gfx |= 0x1000;
      a1->y.w.i = (int16_t)(a1->y.w.i + 0x100);
      a1->priority = 0;
      OW(a1, geyser_origY) = OW(o, geyser_origY);
      OUW(a1, gmake_parent) = OUW(o, gmake_parent);
      obSubtype(o) = 0;
    }
  }
    /* fall through */
  case 2:
    o->vy = (int16_t)(o->vy + 0x18);
    if ((uint16_t)OW(o, geyser_origY) < (uint16_t)o->y.w.i) {
      o->routine += 4;
      OPTR(OUW(o, gmake_parent))->anim = obSubtype(o) ? 1 : 3;
    }
    SpeedToPos(o);
    AnimateSprite(o, ani_get(ANI_Geyser));
    if (obj_far(o)) {
      DeleteObject(o);
      return;
    }
    break;
  case 4: {
    Obj *a1 = OPTR(OUW(o, gmake_parent));
    if (a1->routine == 6) {
      DeleteObject(o);
      return;
    }
    int16_t d0 = (int16_t)(a1->y.w.i + 0x60);
    o->y.w.i = d0;
    d0 = (int16_t)-(d0 - OW(o, geyser_origY));
    uint8_t d1 = 8;
    if (d0 < 0x40) d1 = 0xB;
    if (d0 > 0x80) d1 = 0xE;
    if ((int8_t)--o->timeframe < 0) {
      o->timeframe = 8 - 1;
      if (++o->aniframe >= 2) o->aniframe = 0;
    }
    o->frame = (uint8_t)(o->aniframe + d1);
    if (obj_far(o)) {
      DeleteObject(o);
      return;
    }
    break;
  }
  default: DeleteObject(o); return;
  }
  DisplaySprite(o);
}

/* ------------------------------------------------------------------ the wall of lava */
#define lwall_flag 0x36
#define lwall_parent 0x3C

void LavaWall(Obj *o) {
  switch (o->routine) {
  case 0: {
    o->routine += 4;
    Obj *a1 = o;
    for (int i = 0; i < 2; i++) {
      if (i) {
        Obj *n = FindNextFreeObj(o);
        if (!n) continue;
        a1 = n;
      }
      a1->id = id_LavaWall;
      a1->map = map_get(MAP_LWall);
      a1->gfx = ArtTile_MZ_Lava | Tile_Pal4;
      a1->render = sprite_cam_field;
      a1->actwid = 160 / 2;
      a1->x.w.i = o->x.w.i;
      a1->y.w.i = o->y.w.i;
      a1->priority = 1;
      a1->anim = 0;
      a1->coltype = col_128x64 | col_hurt;
      OUW(a1, lwall_parent) = OADDR(o);
    }
    a1->routine += 6;
    a1->frame = 4;
  }
    /* fall through */
  case 4: {
    int16_t dx = (int16_t)(player->x.w.i - o->x.w.i), dy = (int16_t)(player->y.w.i - o->y.w.i);
    if ((uint16_t)player->x.w.i < (uint16_t)o->x.w.i) dx = (int16_t)-dx;
    if ((uint16_t)player->y.w.i < (uint16_t)o->y.w.i) dy = (int16_t)-dy;
    if ((uint16_t)dx < 192 && (uint16_t)dy < 96) OB(o, lwall_flag) = 1;
    else if (OB(o, lwall_flag)) {
      o->vx = 0x180;
      o->routine -= 2;
    }
  }
    /* fall through */
  case 2: {
    uint8_t r = o->routine;
    SolidObject(o, 64 / 2 + sonic_solid_width, 48 / 2, 48 / 2 + 1, o->x.w.i);
    o->routine = r;
    if (o->x.w.i == 0x6A0) {
      o->vx = 0;
      OB(o, lwall_flag) = 0;
    }
    AnimateSprite(o, ani_get(ANI_LWall));
    if (player->routine < 4) SpeedToPos(o);
    DisplaySprite(o);
    if (OB(o, lwall_flag) || !obj_far(o)) return;
    V.objstate[2 + o->respawn] &= 0x7F;
    o->routine = 8;
    return;
  }
  case 6: {
    Obj *a1 = OPTR(OUW(o, lwall_parent));
    if (a1->routine == 8) {
      DeleteObject(o);
      return;
    }
    o->x.w.i = (int16_t)(a1->x.w.i - 128);
    DisplaySprite(o);
    return;
  }
  default: DeleteObject(o); return;
  }
}

/* ------------------------------------------------------------------ blocks that break, invisible lava */
#define smab_sonani 0x32
#define smab_combo 0x34

void SmashBlock(Obj *o) {
  static const int16_t speeds[8] = {-0x200, -0x200, -0x100, -0x100, 0x200, -0x200, 0x100, -0x100};
  static const uint16_t scores[4] = {10, 20, 50, 100};
  switch (o->routine) {
  case 0:
    o->routine += 2;
    o->map = map_get(MAP_Smab);
    o->gfx = ArtTile_MZ_Block | Tile_Pal3;
    o->render = sprite_cam_field;
    o->actwid = 32 / 2;
    o->priority = 4;
    o->frame = obSubtype(o);
    /* fall through */
  case 2: {
    Obj *a1 = player;
    OUW(o, smab_combo) = V.itembonus;
    OB(o, smab_sonani) = a1->anim;
    SolidObject(o, 32 / 2 + sonic_solid_width, 32 / 2, 34 / 2, o->x.w.i);
    if (!(o->status & status_on_object) || OB(o, smab_sonani) != id_Roll) break;
    V.itembonus = OUW(o, smab_combo);
    a1->status |= status_rolling;
    a1->height = sonic_roll_height;
    a1->width = sonic_roll_width;
    a1->anim = id_Roll;
    a1->vy = -0x300;
    a1->status |= status_in_air;
    a1->status &= (uint8_t)~status_on_object;
    a1->routine = 2;
    o->status &= (uint8_t)~status_on_object;
    o->routine2 = 0;
    o->frame = 1;
    SmashObject(o, speeds, 4, 0x38);
    Obj *p = FindFreeObj();
    if (p) {
      p->id = id_Points;
      p->x.w.i = o->x.w.i;
      p->y.w.i = o->y.w.i;
      uint16_t d2 = V.itembonus;
      V.itembonus += 2;
      if (d2 >= 6) d2 = 6;
      uint16_t pts = scores[d2 >> 1];
      if (V.itembonus >= 32) pts = 1000, d2 = 10;
      AddPoints(pts);
      p->frame = (uint8_t)(d2 >> 1);
    }
  }
    /* fall through */
  case 4:
    SpeedToPos(o);
    o->vy = (int16_t)(o->vy + 0x38);
    DisplaySprite(o);
    if (!(o->render & 0x80)) DeleteObject(o);
    break;
  }
  RememberState(o);
}

void LavaTag(Obj *o) {
  static const uint8_t types[3] = {col_64x64 | col_hurt, col_128x64 | col_hurt, col_256x64 | col_hurt};
  if (o->routine == 0) {
    o->routine += 2;
    o->coltype = types[obSubtype(o) < 3 ? obSubtype(o) : 0];
    o->map = map_get(MAP_LTag);
    o->render = sprite_rendered | sprite_cam_field;
  }
  if (obj_far(o)) DeleteObject(o);
}

/* ------------------------------------------------------------------ moving blocks */
#define mblock_origX 0x30
#define mblock_origY 0x32
#define mblock_slide_wait 0x34
#define mblock_slide_goback 0x36

/* false: hidden (Labyrinth's raft before its switch) */
static bool MBlock_Move(Obj *o) {
  int16_t d0, d1;
  switch (obSubtype(o) & 0xF) {
  case 1:
    d0 = OSC_B(0xE);
    if (o->status & 1) d0 = (int16_t)(-d0 + 0x60);
    o->x.w.i = (int16_t)(OW(o, mblock_origX) - d0);
    break;
  case 2:
  case 4:
  case 9:
    if (o->routine == 4) obSubtype(o)++;
    break;
  case 3:
  case 5:
    if (ObjHitWallRight(o, o->actwid, NULL) < 0) {
      if ((obSubtype(o) & 0xF) == 3) obSubtype(o) = 0;
      else obSubtype(o)++;
      break;
    }
    o->x.w.i++;
    OW(o, mblock_origX) = o->x.w.i;
    break;
  case 6: {
    SpeedToPos(o);
    o->vy = (int16_t)(o->vy + 0x18);
    int16_t f = ObjFloorDist(o, NULL);
    if (f >= 0) break;
    o->y.w.i = (int16_t)(o->y.w.i + f);
    o->vy = 0;
    obSubtype(o) = 0;
    break;
  }
  case 7:
    if (V.f_switch[2]) obSubtype(o) -= 3;
    if (obj_far_x(OW(o, mblock_origX))) DeleteObject(o);
    return false;
  case 8:
    d0 = OSC_B(0x1E);
    if (o->status & 1) d0 = (int16_t)(-d0 + 0x80);
    o->y.w.i = (int16_t)(OW(o, mblock_origY) - d0);
    break;
  case 0xA: {
    int16_t d3 = (int16_t)(o->actwid * 2);
    d1 = 8;
    if (o->status & 1) d1 = -8, d3 = (int16_t)-d3;
    if (!OW(o, mblock_slide_goback)) {
      if ((int16_t)(o->x.w.i - OW(o, mblock_origX)) != d3) {
        o->x.w.i = (int16_t)(o->x.w.i + d1);
        OW(o, mblock_slide_wait) = 5 * 60;
      } else if (!--OW(o, mblock_slide_wait)) OW(o, mblock_slide_goback) = 1;
    } else if (o->x.w.i != OW(o, mblock_origX)) o->x.w.i = (int16_t)(o->x.w.i - d1);
    else {
      OW(o, mblock_slide_goback) = 0;
      obSubtype(o)--;
    }
    break;
  }
  }
  return true;
}

void MovingBlock(Obj *o) {
  static const uint8_t var[5][2] = {{32 / 2, 0}, {64 / 2, 1}, {64 / 2, 2}, {128 / 2, 3}, {96 / 2, 4}};
  switch (o->routine) {
  case 0: {
    o->routine += 2;
    o->map = map_get(MAP_MBlock);
    o->gfx = ArtTile_MZ_Block | Tile_Pal3;
    if (V.zone == id_LZ) {
      o->map = map_get(MAP_MBlockLZ);
      o->gfx = ArtTile_LZ_Moving_Block | Tile_Pal3;
      o->height = 14 / 2;
    }
    if (V.zone == id_SBZ) {
      o->gfx = ArtTile_SBZ_Moving_Block_Short | Tile_Pal2;
      if (obSubtype(o) != 0x28) o->gfx = ArtTile_SBZ_Moving_Block_Long | Tile_Pal3;
    }
    o->render = sprite_cam_field;
    int k = ((obSubtype(o) >> 3) & 0x1E) >> 1;
    if (k > 4) k = 4;
    o->actwid = var[k][0];
    o->frame = var[k][1];
    o->priority = 4;
    OW(o, mblock_origX) = o->x.w.i;
    OW(o, mblock_origY) = o->y.w.i;
    obSubtype(o) &= 0xF;
  }
    /* fall through */
  case 2:
    if (!MBlock_Move(o)) return;
    PlatformObject(o, o->actwid);
    break;
  case 4: {
    ExitPlatform(o, o->actwid);
    int16_t x = o->x.w.i;
    if (!MBlock_Move(o)) return;
    MvSonicOnPtfm2(o, x);
    break;
  }
  }
  if (obj_far_x(OW(o, mblock_origX))) {
    DeleteObject(o);
    return;
  }
  DisplaySprite(o);
}

/* ------------------------------------------------------------------ Basaran */
#define bas_sonicY 0x36

extern int obj_d7;

/* faces Sonic; his distance, and the speed towards him */
static uint16_t Bas_CheckDistance(Obj *o, int16_t *d1) {
  *d1 = 0x100;
  o->status |= 1;
  int16_t d0 = (int16_t)(player->x.w.i - o->x.w.i);
  if ((uint16_t)player->x.w.i < (uint16_t)o->x.w.i) {
    d0 = (int16_t)-d0;
    *d1 = -0x100;
    o->status &= (uint8_t)~1;
  }
  return (uint16_t)d0;
}

void Basaran(Obj *o) {
  if (o->routine == 0) {
    o->routine += 2;
    o->map = map_get(MAP_Bas);
    o->gfx = ArtTile_Basaran | Tile_Prio;
    o->render = sprite_cam_field;
    o->height = 24 / 2;
    o->priority = 2;
    o->coltype = col_16x16 | col_badnik;
    o->actwid = 32 / 2;
  }
  int16_t d1;
  switch (o->routine2) {
  case 0: {
    if (Bas_CheckDistance(o, &d1) >= 128) break;
    int16_t py = player->y.w.i;
    OW(o, bas_sonicY) = py;
    if ((uint16_t)py < (uint16_t)o->y.w.i) break;
    if ((uint16_t)(py - o->y.w.i) >= 128 || V.debuguse) break;
    if ((uint8_t)(vblank_byte + obj_d7) & 7) break;
    o->anim = 1;
    o->routine2 += 2;
    break;
  }
  case 2: {
    SpeedToPos(o);
    o->vy = (int16_t)(o->vy + 0x18);
    Bas_CheckDistance(o, &d1);
    uint16_t sy = OUW(o, bas_sonicY);
    if (sy < (uint16_t)o->y.w.i) {
      if (!(o->render & 0x80)) {
        DeleteObject(o);
        return;
      }
      break;
    }
    if ((uint16_t)(sy - o->y.w.i) >= 16) break;
    o->vx = d1;
    o->vy = 0;
    o->anim = 2;
    o->routine2 += 2;
    break;
  }
  case 4: {
    SpeedToPos(o);
    int16_t d0 = (int16_t)(player->x.w.i - o->x.w.i);
    if ((uint16_t)player->x.w.i < (uint16_t)o->x.w.i) d0 = (int16_t)-d0;
    if ((uint16_t)d0 < 128) break;
    if ((uint8_t)(vblank_byte + obj_d7) & 7) break;
    o->routine2 += 2;
    break;
  }
  case 6: {
    SpeedToPos(o);
    o->vy = (int16_t)(o->vy - 0x18);
    int16_t c = ObjHitCeiling(o, NULL);
    if (c >= 0) break;
    o->y.w.i = (int16_t)(o->y.w.i - c);
    o->x.w.i &= (int16_t)0xFFF8;
    o->vx = o->vy = 0;
    o->anim = 0;
    o->routine2 = 0;
    break;
  }
  }
  AnimateSprite(o, ani_get(ANI_Bas));
  RememberState(o);
}

/* ------------------------------------------------------------------ Caterkiller */
#define cat_waittime 0x2A
#define cat_mode 0x2B
#define cat_floormap 0x2C
#define cat_segmentpos 0x3C
#define cat_parent 0x3E

static const uint8_t *cat_ani(void) { return DATA(D_ANI_CAT); }

static void Cat_Despawn(Obj *o) {
  if (o->respawn) V.objstate[2 + o->respawn] &= 0x7F;
  o->routine = 0xA;
}

static void Cat_Fragment(Obj *o) {
  ObjectFall(o);
  if (o->vy >= 0) {
    int16_t f = ObjFloorDist(o, NULL);
    if (f < 0) {
      o->y.w.i = (int16_t)(o->y.w.i + f);
      o->vy = -0x400;
    }
  }
  if (!(o->render & 0x80)) {
    Cat_Despawn(o);
    return;
  }
  DisplaySprite(o);
}

static void Cat_FragmentateBody(Obj *o) {
  static const int16_t speeds[4] = {-0x200, -0x180, 0x180, 0x200};
  int16_t d0 = speeds[((o->routine - 2) >> 1) & 3];
  if (o->status & 1) d0 = (int16_t)-d0;
  o->vx = d0;
  o->vy = -0x400;
  o->routine = 0xC;
  o->frame &= 0xF8;
  Cat_Fragment(o);
}

/* a moving piece: a step along x; true when its pixel changed */
static bool cat_step(Obj *o) {
  int32_t d2 = o->x.l;
  int16_t old = o->x.w.i, d0 = o->vx;
  if (o->status & 1) d0 = (int16_t)-d0;
  o->x.l = d2 + ((int32_t)d0 << 8);
  return old != o->x.w.i;
}

/* at a ledge: turns round */
static void cat_turn(Obj *o, bool check_speed) {
  o->x.w.f = (uint16_t)-o->x.w.f;
  if (o->x.w.f && (o->status & 1) && (!check_speed || o->vx == -0xC0)) {
    o->x.w.i--;
    OB(o, cat_segmentpos)++;
    OB(o, cat_floormap + OB(o, cat_segmentpos)) = 0;
  }
  o->status ^= 1;
  o->render = o->status;
  OB(o, cat_segmentpos) = (uint8_t)((OB(o, cat_segmentpos) + 1) & 0xF);
}

void Caterkiller(Obj *o) {
  switch (o->routine) {
  case 0: {
    o->height = 14 / 2;
    o->width = 16 / 2;
    ObjectFall(o);
    int16_t f = ObjFloorDist(o, NULL);
    if (f >= 0) return;
    o->y.w.i = (int16_t)(o->y.w.i + f);
    o->vy = 0;
    o->routine += 2;
    o->map = map_get(MAP_Cat);
    o->gfx = V.zone == id_SBZ ? ArtTile_SBZ_Caterkiller | Tile_Pal2 : ArtTile_MZ_SYZ_Caterkiller | Tile_Pal2;
    o->render &= 3;
    o->render |= sprite_cam_field;
    o->status = o->render;
    o->priority = 4;
    o->actwid = 16 / 2;
    o->coltype = col_16x16 | col_badnik;
    int16_t d2 = o->x.w.i, d5 = (o->status & 1) ? -12 : 12;
    uint8_t d6 = 4, d4 = 4;
    Obj *a2 = o;
    for (int i = 0; i < 3; i++) {
      Obj *a1 = FindNextFreeObj(o);
      if (!a1) {
        Cat_Despawn(o);
        return;
      }
      a1->id = id_Caterkiller;
      a1->routine = d6;
      d6 += 2;
      a1->map = o->map;
      a1->gfx = o->gfx;
      a1->priority = 5;
      a1->actwid = 16 / 2;
      a1->coltype = col_16x16 | col_special;
      d2 = (int16_t)(d2 + d5);
      a1->x.w.i = d2;
      a1->y.w.i = o->y.w.i;
      a1->status = o->status;
      a1->render = o->status;
      a1->frame = 8;
      OUW(a1, cat_parent) = OADDR(a2);
      OB(a1, cat_segmentpos) = d4;
      d4 += 4;
      a2 = a1;
    }
    OB(o, cat_waittime) = 8 - 1;
    OB(o, cat_segmentpos) = 0;
  }
    /* fall through */
  case 2: {
    if (o->status & 0x80) {
      Cat_FragmentateBody(o);
      return;
    }
    if (o->routine2 == 0) {
      if ((int8_t)--OB(o, cat_waittime) >= 0) goto head_anim;
      o->routine2 += 2;
      OB(o, cat_waittime) = 17 - 1;
      o->vx = -0xC0;
      o->inertia = 0x40;
      uint8_t old = OB(o, cat_mode);
      OB(o, cat_mode) ^= 0x10;
      if (!(old & 0x10)) o->vx = 0, o->inertia = (int16_t)-o->inertia;
      OB(o, cat_mode) |= 0x80;
    }
    if ((int8_t)--OB(o, cat_waittime) < 0) {
      o->routine2 -= 2;
      OB(o, cat_waittime) = 8 - 1;
      o->vx = 0;
      o->inertia = 0;
    } else if (o->vx && cat_step(o)) {
      int16_t f = ObjFloorDist(o, NULL);
      if (f < -8 || f >= 0xC) {
        OB(o, cat_floormap + OB(o, cat_segmentpos)) = 0x80;
        cat_turn(o, false);
      } else {
        o->y.w.i = (int16_t)(o->y.w.i + f);
        uint8_t k = OB(o, cat_segmentpos);
        OB(o, cat_segmentpos) = (uint8_t)((k + 1) & 0xF);
        OB(o, cat_floormap + k) = (uint8_t)f;
      }
    }
  head_anim: {
    uint8_t d1 = OB(o, cat_mode);
    if (d1 & 0x80) {
      uint8_t d0 = cat_ani()[o->angle & 0x7F];
      o->angle += 4;
      if (d0 & 0x80) OB(o, cat_mode) &= 0x7F;
      else o->frame = (uint8_t)(d0 + (d1 & 0x10));
    }
  }
    if (obj_far(o)) {
      Cat_Despawn(o);
      return;
    }
    DisplaySprite(o);
    return;
  }
  case 6: {
    Obj *a1 = OPTR(OUW(o, cat_parent));
    OB(o, cat_mode) = OB(a1, cat_mode);
    if (OB(o, cat_mode) & 0x80) {
      const uint8_t *ani = cat_ani();
      int d0 = o->angle & 0x7F;
      o->angle += 4;
      if (ani[(d0 + 4) & 0x7F] & 0x80) o->angle += 4;
      o->frame = (uint8_t)(ani[d0] + 8);
    }
  }
    /* fall through */
  case 4:
  case 8: {
    Obj *a1 = OPTR(OUW(o, cat_parent));
    if (o->status & 0x80) {
      a1->status |= 0x80;
      Cat_FragmentateBody(o);
      return;
    }
    OB(o, cat_mode) = OB(a1, cat_mode);
    o->routine2 = a1->routine2;
    if (o->routine2) {
      o->inertia = a1->inertia;
      o->vx = (int16_t)(a1->vx + o->inertia);
      if (cat_step(o)) {
        uint8_t k = OB(o, cat_segmentpos);
        uint8_t d1 = OB(a1, cat_floormap + k);
        if (d1 == 0x80) {
          OB(o, cat_floormap + k) = d1;
          cat_turn(o, true);
        } else {
          o->y.w.i = (int16_t)(o->y.w.i + (int8_t)d1);
          OB(o, cat_segmentpos) = (uint8_t)((k + 1) & 0xF);
          OB(o, cat_floormap + k) = d1;
        }
      }
    }
    if (a1->routine == 0xC) {
      a1->status |= 0x80;
      Cat_FragmentateBody(o);
      return;
    }
    if (a1->id == id_ExplosionItem || a1->routine == 0xA) o->routine = 0xA;
    DisplaySprite(o);
    return;
  }
  case 0xA: DeleteObject(o); return;
  case 0xC: Cat_Fragment(o); return;
  }
}

/* ------------------------------------------------------------------ the boss */
#define BossFire_GenericTimer 0x29
#define BossFire_SpreadX 0x32
#define BossMarble_ParentObj 0x36
#define BossMarble_LavaTimer 0x34
#define BossMarble_GenericTimer 0x3C
#define BossMarble_SineCounter 0x3F

static void BMZ_ShipUpdate(Obj *o) {
  o->y.w.i = obBossY(o).w.i;
  o->x.w.i = obBossX(o).w.i;
  if (o->routine2 >= 4) return;
  if (o->status & 0x80) {
    AddPoints(100);
    o->routine2 = 4;
    OW(o, BossMarble_GenericTimer) = 180;
    o->vx = 0;
    return;
  }
  if (o->coltype) return;
  if (!OB(o, obBossFlash)) OB(o, obBossFlash) = 0x28;
  uint16_t *c = &V.palette[0x11];
  *c = *c ? 0 : 0xEEE;
  if (!--OB(o, obBossFlash)) o->coltype = col_48x48 | col_boss;
}

static void BossMarble_MakeLava(Obj *o) {
  if (OB(o, BossMarble_LavaTimer)-- == 0) {
    Obj *a1 = FindFreeObj();
    if (a1) {
      a1->id = id_LavaBall;
      a1->y.w.i = boss_mz_y + 0xD8;
      uint32_t r = RandomNumber() & 0xFFFF;
      a1->x.w.i = (int16_t)(r % 0x50 + boss_mz_x + 0x78);
      obSubtype(a1) = 0;
      OB(a1, lball_fromboss) = 0xFF;
    }
    OB(o, BossMarble_LavaTimer) = (uint8_t)((RandomNumber() & 0x1F) + 0x40);
  }
  if (o->status & 1) {
    if (obBossX(o).w.i < boss_mz_x + 0x110) return;
    obBossX(o).w.i = boss_mz_x + 0x110;
  } else {
    if (obBossX(o).w.i > boss_mz_x + 0x30) return;
    obBossX(o).w.i = boss_mz_x + 0x30;
  }
  o->vx = 0;
  o->vy = -0x180;
  if ((uint16_t)obBossY(o).w.i < boss_mz_y + 0x1C) o->vy = 0x180;
  obSubtype(o) += 2;
}

static void BMZ_ChgDir(Obj *o) {
  if (!o->vx) {
    int16_t d0 = 0x40;
    uint16_t by = (uint16_t)obBossY(o).w.i;
    if (by != boss_mz_y + 0x1C) {
      if (by > boss_mz_y + 0x1C) d0 = -d0;
      o->vy = d0;
      BossMove(o);
      return;
    }
    o->vx = 0x200;
    o->vy = 0x100;
    if (!(o->status & 1)) o->vx = (int16_t)-o->vx;
  }
  if (OB(o, obBossFlash) < 0x18) {
    BossMove(o);
    o->vy = (int16_t)(o->vy - 4);
  }
  BossMarble_MakeLava(o);
}

static void BMZ_DropFire(Obj *o) {
  BossMove(o);
  if ((int16_t)(obBossY(o).w.i - (boss_mz_y + 0x1C)) > 0) return;
  if (o->vy) {
    o->vy = 0;
    OW(o, BossMarble_GenericTimer) = 80;
    o->status ^= 1;
    Obj *a1 = FindFreeObj();
    if (a1) {
      a1->x.w.i = obBossX(o).w.i;
      a1->y.w.i = (int16_t)(obBossY(o).w.i + 0x18);
      a1->id = id_BossFire;
      obSubtype(a1) = 1;
    }
  }
  if (!--OW(o, BossMarble_GenericTimer)) obSubtype(o) += 2;
}

static void BossMarble_SetBits(Obj *o) {
  Obj *a1 = OPTR(OUW(o, BossMarble_ParentObj));
  o->x.w.i = a1->x.w.i;
  o->y.w.i = a1->y.w.i;
  o->status = a1->status;
  o->render = (uint8_t)((o->render & ~3) | (o->status & 3));
  DisplaySprite(o);
}

void BossMarble(Obj *o) {
  switch (o->routine) {
  case 0: {
    static const uint8_t data[4][3] = {{2, 0, 4}, {4, 1, 4}, {6, 7, 4}, {8, 0, 3}};
    obBossX(o).w.i = o->x.w.i;
    obBossY(o).w.i = o->y.w.i;
    o->coltype = col_48x48 | col_boss;
    obBossHits(o) = 8;
    Obj *a1 = o;
    for (int i = 0; i < 4; i++) {
      if (i) {
        a1 = FindNextFreeObj(o);
        if (!a1) break;
        a1->id = id_BossMarble;
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
      OUW(a1, BossMarble_ParentObj) = OADDR(o);
    }
  }
    /* fall through */
  case 2:
    switch (o->routine2) {
    case 0: {
      uint8_t s = OB(o, BossMarble_SineCounter);
      OB(o, BossMarble_SineCounter) += 2;
      o->vy = (int16_t)(CalcSine(s, NULL) >> 2);
      o->vx = -0x100;
      BossMove(o);
      if (obBossX(o).w.i == boss_mz_x + 0x110) {
        o->routine2 += 2;
        obSubtype(o) = 0;
        o->vx = o->vy = 0;
      }
      OB(o, BossMarble_LavaTimer) = (uint8_t)RandomNumber();
      BMZ_ShipUpdate(o);
      break;
    }
    case 2:
      if (obSubtype(o) & 2) BMZ_DropFire(o);
      else BMZ_ChgDir(o);
      obSubtype(o) &= 6;
      BMZ_ShipUpdate(o);
      break;
    case 4:
      if (--OW(o, BossMarble_GenericTimer) >= 0) {
        BossDefeated(o);
        break;
      }
      o->status |= 1;
      o->status &= 0x7F;
      o->vx = 0;
      o->routine2 += 2;
      OW(o, BossMarble_GenericTimer) = -38;
      if (!V.bossstatus) V.bossstatus = 1, o->vy = 0;
      break;
    case 6: {
      int16_t t = ++OW(o, BossMarble_GenericTimer);
      if (t == 0 || (t < 0 && (uint16_t)obBossY(o).w.i >= boss_mz_y + 0x60)) {
        o->vy = 0;
        OW(o, BossMarble_GenericTimer) = 0;
      } else if (t < 0) o->vy = (int16_t)(o->vy + 0x18);
      else if ((uint16_t)t < 48) o->vy = (int16_t)(o->vy - 8);
      else if (t == 48) o->vy = 0;
      else if ((uint16_t)t >= 56) o->routine2 += 2;
      BossMove(o);
      BMZ_ShipUpdate(o);
      break;
    }
    case 8:
      o->vx = 0x500;
      o->vy = -0x40;
      if ((uint16_t)V.limitright2 >= boss_mz_end) {
        if (!(o->render & 0x80)) {
          DeleteObject(o);
          return;
        }
      } else V.limitright2 += 2;
      BossMove(o);
      BMZ_ShipUpdate(o);
      break;
    }
    AnimateSprite(o, ani_get(ANI_Eggman));
    o->render = (uint8_t)((o->render & ~3) | (o->status & 3));
    DisplaySprite(o);
    return;
  case 4: {
    Obj *a1 = OPTR(OUW(o, BossMarble_ParentObj));
    uint8_t d1 = 1;
    int8_t d0 = (int8_t)(a1->routine2 - 2);
    if (d0 == 0) {
      if ((obSubtype(a1) & 2) && !a1->vy) {
        d1 = 4;
        goto write;
      }
    } else {
      d0 = (int8_t)(d0 - 2);
      if (d0 >= 0) {
        d1 = 0xA;
        goto write;
      }
    }
    if (!a1->coltype) d1 = 5;
    else if (player->routine >= 4) d1 = 4;
  write:
    o->anim = d1;
    if ((int8_t)(d0 - 4) == 0) {
      o->anim = 6;
      if (!(o->render & 0x80)) {
        DeleteObject(o);
        return;
      }
    }
    AnimateSprite(o, ani_get(ANI_Eggman));
    BossMarble_SetBits(o);
    return;
  }
  case 6: {
    Obj *a1 = OPTR(OUW(o, BossMarble_ParentObj));
    o->anim = 7;
    if ((int8_t)a1->routine2 >= 8) {
      o->anim = 0xB;
      if (!(o->render & 0x80)) {
        DeleteObject(o);
        return;
      }
    } else if (a1->vx) o->anim = 8;
    AnimateSprite(o, ani_get(ANI_Eggman));
    BossMarble_SetBits(o);
    return;
  }
  case 8: {
    Obj *a1 = OPTR(OUW(o, BossMarble_ParentObj));
    if (a1->routine2 == 8 && !(o->render & 0x80)) {
      DeleteObject(o);
      return;
    }
    o->map = map_get(MAP_BossItems);
    o->gfx = ArtTile_Eggman_Weapons | Tile_Pal2;
    o->frame = 4;
    BossMarble_SetBits(o);
    return;
  }
  }
}

void BossFire(Obj *o) {
  switch (o->routine) {
  case 0:
    o->height = o->width = 16 / 2;
    o->map = map_get(MAP_Fire);
    o->gfx = ArtTile_MZ_Fireball;
    o->render = sprite_cam_field;
    o->priority = 5;
    obBossY(o).w.i = o->y.w.i;
    o->actwid = 16 / 2;
    o->routine += 2;
    if (!obSubtype(o)) {
      o->coltype = col_16x16 | col_hurt;
      o->routine += 2;
      goto temp_fire;
    }
    OB(o, BossFire_GenericTimer) = 30;
    /* fall through */
  case 2:
    switch (o->routine2) {
    case 0:
      o->status |= 2;
      if ((int8_t)--OB(o, BossFire_GenericTimer) >= 0) break;
      o->coltype = col_16x16 | col_hurt;
      obSubtype(o) = 0;
      o->vy = (int16_t)(o->vy + 0x18);
      o->status &= (uint8_t)~2;
      if (ObjFloorDist(o, NULL) < 0) o->routine2 += 2;
      break;
    case 2: {
      o->y.w.i -= 2;
      o->gfx |= 0x8000;
      o->vx = 0xA0;
      o->vy = 0;
      obBossX(o).w.i = o->x.w.i;
      obBossY(o).w.i = o->y.w.i;
      OB(o, BossFire_GenericTimer) = 3;
      Obj *a1 = FindNextFreeObj(o);
      if (a1) {
        *a1 = *o;
        a1->vx = (int16_t)-a1->vx;
        a1->routine2 += 2;
      }
      o->routine2 += 2;
      break;
    }
    case 4: {
      if (ObjFloorDist(o, NULL) >= 0) {
        o->routine2 += 2;
        break;
      }
      int16_t d0 = o->x.w.i;
      if (d0 > boss_mz_x + 0x140) {
        o->routine += 2;
        break;
      }
      int16_t d1 = obBossX(o).w.i;
      if (d1 != d0 && (d0 & 0x10) != (d1 & 0x10)) {
        Obj *a1 = FindNextFreeObj(o);
        if (a1) {
          a1->x.w.i = o->x.w.i;
          a1->y.w.i = o->y.w.i;
          a1->id = id_BossFire;
          obSubtype(a1) = 0;
          OB(a1, BossFire_GenericTimer) = 103;
        }
        OW(o, BossFire_SpreadX) = o->x.w.i;
      }
      obBossX(o).w.i = o->x.w.i;
      break;
    }
    case 6: {
      o->status &= (uint8_t)~2;
      o->vy = (int16_t)(o->vy + 0x24);
      int16_t d0 = (int16_t)(o->x.w.i - OW(o, BossFire_SpreadX));
      if (d0 < 0) d0 = (int16_t)-d0;
      if (d0 == 0x12) o->gfx &= 0x7FFF;
      if (ObjFloorDist(o, NULL) >= 0) break;
      if (!--OB(o, BossFire_GenericTimer)) {
        DeleteObject(o);
        return;
      }
      o->vy = 0;
      o->x.w.i = OW(o, BossFire_SpreadX);
      o->y.w.i = obBossY(o).w.i;
      o->gfx |= 0x8000;
      o->routine2 -= 2;
      break;
    }
    }
    SpeedToPos(o);
    AnimateSprite(o, ani_get(ANI_Fire));
    if ((uint16_t)o->y.w.i > boss_mz_y + 0xD8) {
      DeleteObject(o);
      return;
    }
    break;
  case 4:
  temp_fire:
    o->gfx |= 0x8000;
    if (!--OB(o, BossFire_GenericTimer)) {
      o->anim = 1;
      o->y.w.i -= 4;
      o->coltype = 0;
    }
    AnimateSprite(o, ani_get(ANI_Fire));
    break;
  default: DeleteObject(o); return;
  }
  DisplaySprite(o);
}
