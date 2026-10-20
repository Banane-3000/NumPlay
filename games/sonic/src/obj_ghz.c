/* Green Hill Zone's objects (some found in other zones too): the log bridge that sags under Sonic,
 * swinging platforms and wrecking balls, the rotating spiked helix, the moving and falling
 * platforms, collapsing ledges and floors, scenery, the purple rock, walls that break when rolled
 * into; the badniks Motobug, Crabmeat, Buzz Bomber (and its missiles), Chopper, Newtron; and
 * Dr. Robotnik's wrecking ball machine. */
#include "s1.h"
#include "collide.h"
#include "game.h"
#include "objs.h"

/* ------------------------------------------------------------------ the bridge */
#define bridge_origY 0x3C
#define bridge_nudge 0x3E
#define bridge_currentlog 0x3F

static void Bri_Bend(Obj *o) {
  static const uint8_t ymax[17][16] = {
    {0},
    {2},
    {2, 2},
    {2, 4, 2},
    {2, 4, 4, 2},
    {2, 4, 6, 4, 2},
    {2, 4, 6, 6, 4, 2},
    {2, 4, 6, 8, 6, 4, 2},
    {2, 4, 6, 8, 8, 6, 4, 2},
    {2, 4, 6, 8, 10, 8, 6, 4, 2},
    {2, 4, 6, 8, 10, 10, 8, 6, 4, 2},
    {2, 4, 6, 8, 10, 12, 10, 8, 6, 4, 2},
    {2, 4, 6, 8, 10, 12, 12, 10, 8, 6, 4, 2},
    {2, 4, 6, 8, 10, 12, 14, 12, 10, 8, 6, 4, 2},
    {2, 4, 6, 8, 10, 12, 14, 14, 12, 10, 8, 6, 4, 2},
    {2, 4, 6, 8, 10, 12, 14, 16, 14, 12, 10, 8, 6, 4, 2},
    {2, 4, 6, 8, 10, 12, 14, 16, 16, 14, 12, 10, 8, 6, 4, 2},
  };
  static const uint8_t align[16][16] = {
    {0xFF},
    {0xB5, 0xFF},
    {0x7E, 0xDB, 0xFF},
    {0x61, 0xB5, 0xEC, 0xFF},
    {0x4A, 0x93, 0xCD, 0xF3, 0xFF},
    {0x3E, 0x7E, 0xB0, 0xDB, 0xF6, 0xFF},
    {0x38, 0x6D, 0x9D, 0xC5, 0xE4, 0xF8, 0xFF},
    {0x31, 0x61, 0x8E, 0xB5, 0xD4, 0xEC, 0xFB, 0xFF},
    {0x2B, 0x56, 0x7E, 0xA2, 0xC1, 0xDB, 0xEE, 0xFB, 0xFF},
    {0x25, 0x4A, 0x73, 0x93, 0xB0, 0xCD, 0xE1, 0xF3, 0xFC, 0xFF},
    {0x1F, 0x44, 0x67, 0x88, 0xA7, 0xBD, 0xD4, 0xE7, 0xF4, 0xFD, 0xFF},
    {0x1F, 0x3E, 0x5C, 0x7E, 0x98, 0xB0, 0xC9, 0xDB, 0xEA, 0xF6, 0xFD, 0xFF},
    {0x19, 0x38, 0x56, 0x73, 0x8E, 0xA7, 0xBD, 0xD1, 0xE1, 0xEE, 0xF8, 0xFE, 0xFF},
    {0x19, 0x38, 0x50, 0x6D, 0x83, 0x9D, 0xB0, 0xC5, 0xD8, 0xE4, 0xF1, 0xF8, 0xFE, 0xFF},
    {0x19, 0x31, 0x4A, 0x67, 0x7E, 0x93, 0xA7, 0xBD, 0xCD, 0xDB, 0xE7, 0xF3, 0xF9, 0xFE, 0xFF},
    {0x19, 0x31, 0x4A, 0x61, 0x78, 0x8E, 0xA2, 0xB5, 0xC5, 0xD4, 0xE1, 0xEC, 0xF4, 0xFB, 0xFE, 0xFF},
  };
  uint16_t d4 = (uint16_t)CalcSine(OB(o, bridge_nudge), NULL);
  int n = obSubtype(o), cur = OB(o, bridge_currentlog);
  const uint8_t *flat = &ymax[0][0];
  int d5 = flat[(cur + n * 16) & 0x1FF];
  const uint8_t *a3 = align[cur & 0xF];
  int k = 0;
  for (int i = 0; i <= cur; i++, k++) {
    Obj *a1 = CHILD(o, k);
    uint32_t d0 = (uint32_t)(uint16_t)((a3[i] + 1) * d5) * d4;
    a1->y.w.i = (int16_t)((d0 >> 16) + OW(a1, bridge_origY));
  }
  int d3 = (int8_t)(uint8_t)-(uint8_t)(cur + 1 - n);
  if (d3 < 0 || d3 == 0) return;
  const uint8_t *row = align[d3 & 0xF] + d3;
  for (int i = 0; i < d3; i++, k++) {
    Obj *a1 = CHILD(o, k);
    uint32_t d0 = (uint32_t)(uint16_t)((*--row + 1) * d5) * d4;
    a1->y.w.i = (int16_t)((d0 >> 16) + OW(a1, bridge_origY));
  }
}

static bool Bri_ChkDel(Obj *o) {
  if (!obj_far(o)) return false;
  int n = obSubtype(o);
  for (int i = 0; i < n; i++) {
    Obj *a1 = CHILD(o, i);
    if (a1 != o) DeleteObject(a1);
  }
  DeleteObject(o);
  return true;
}

void Bridge(Obj *o) {
  switch (o->routine) {
  case 0: {
    o->routine += 2;
    o->map = map_get(MAP_Bri);
    o->gfx = ArtTile_GHZ_Bridge | Tile_Pal3;
    o->render = sprite_cam_field;
    o->priority = 3;
    o->actwid = 256 / 2;
    int16_t d2 = o->y.w.i, d3 = o->x.w.i;
    uint8_t d1 = obSubtype(o);
    obSubtype(o) = 0;
    uint8_t *list = &o->s.b[1];
    d3 = (int16_t)(d3 - (d1 >> 1) * 16);
    if (d1 >= 2) {
      for (int i = 0; i <= d1 - 2; i++) {
        Obj *a1 = FindFreeObj();
        if (!a1) break;
        obSubtype(o)++;
        if (d3 == o->x.w.i) {
          d3 = (int16_t)(d3 + 16);
          o->y.w.i = d2;
          OW(o, bridge_origY) = d2;
          *list++ = SLOT(o);
          obSubtype(o)++;
        }
        *list++ = SLOT(a1);
        a1->routine = 0xA;
        a1->id = id_Bridge;
        a1->y.w.i = d2;
        OW(a1, bridge_origY) = d2;
        a1->x.w.i = d3;
        a1->map = map_get(MAP_Bri);
        a1->gfx = ArtTile_GHZ_Bridge | Tile_Pal3;
        a1->render = sprite_cam_field;
        a1->priority = 3;
        a1->actwid = 16 / 2;
        d3 = (int16_t)(d3 + 16);
      }
    }
  }
    /* fall through */
  case 2: {
    /* stepping on it */
    int16_t d1 = (int16_t)(obSubtype(o) * 8), d2 = (int16_t)(d1 * 2);
    d1 = (int16_t)(d1 + 8);
    if (player->vy >= 0) {
      int16_t d0 = (int16_t)(player->x.w.i - o->x.w.i + d1);
      if (d0 >= 0 && (uint16_t)d0 < (uint16_t)d2) Plat_NoXCheck(o);
    }
    if (OB(o, bridge_nudge)) {
      OB(o, bridge_nudge) -= 4;
      Bri_Bend(o);
    }
    DisplaySprite(o);
    Bri_ChkDel(o);
    return;
  }
  case 4: {
    int16_t d1 = (int16_t)(obSubtype(o) * 8), d2 = d1;
    d1 = (int16_t)(d1 + 8);
    int d0 = ExitPlatform2(o, d1, d2);
    if (d0 >= 0) {
      OB(o, bridge_currentlog) = (uint8_t)(d0 >> 4);
      if (OB(o, bridge_nudge) != 0x40) OB(o, bridge_nudge) += 4;
      Bri_Bend(o);
      Obj *a2 = CHILD(o, OB(o, bridge_currentlog));
      player->y.w.i = (int16_t)(a2->y.w.i - 8 - (uint8_t)player->height);
    }
    DisplaySprite(o);
    Bri_ChkDel(o);
    return;
  }
  case 0xA: DisplaySprite(o); return;
  default: DeleteObject(o); return;
  }
}

/* ------------------------------------------------------------------ swinging platforms */
#define swing_origY 0x38
#define swing_origX 0x3A
#define swing_radius 0x3C
#define GBall_AnchorPos 0x32
#define GBall_LinkDist 0x3C
#define GBall_Swing_Direction 0x3D
#define GBall_Swing_Speed 0x3E

/* the chain hanging from (origX, origY) at this angle, each link at its radius */
static void Swing_UpdateSwingPosition(Obj *o, uint8_t angle) {
  int16_t cos, sin = CalcSine(angle, &cos);
  int16_t d2 = OW(o, swing_origY), d3 = OW(o, swing_origX);
  int n = obSubtype(o);
  for (int i = 0; i <= n; i++) {
    Obj *a1 = CHILD(o, i);
    int32_t r = OB(a1, swing_radius);
    a1->y.w.i = (int16_t)((int16_t)((r * sin) >> 8) + d2);
    a1->x.w.i = (int16_t)((int16_t)((r * cos) >> 8) + d3);
  }
}

static void Swing_Move(Obj *o) {
  uint8_t d0 = OSC_B(0x1A);
  if (o->status & 1) d0 = (uint8_t)(-d0 + 0x80);
  Swing_UpdateSwingPosition(o, d0);
}

static void GBall_Move(Obj *o) {
  int16_t d0;
  if (!OB(o, GBall_Swing_Direction)) {
    d0 = (int16_t)(OW(o, GBall_Swing_Speed) + 8);
    OW(o, GBall_Swing_Speed) = d0;
    set_anglew(o, (uint16_t)(anglew(o) + d0));
    if (d0 == 0x200) OB(o, GBall_Swing_Direction) = 1;
  } else {
    d0 = (int16_t)(OW(o, GBall_Swing_Speed) - 8);
    OW(o, GBall_Swing_Speed) = d0;
    set_anglew(o, (uint16_t)(anglew(o) + d0));
    if (d0 == -0x200) OB(o, GBall_Swing_Direction) = 0;
  }
  Swing_UpdateSwingPosition(o, o->angle);
}

static bool Swing_ChkDel(Obj *o) {
  if (!obj_far_x(OW(o, swing_origX))) return false;
  int n = obSubtype(o);
  for (int i = 0; i <= n; i++) DeleteObject(CHILD(o, i));
  return true;
}

void SwingingPlatform(Obj *o) {
  switch (o->routine) {
  case 0: {
    o->routine += 2;
    o->map = map_get(MAP_Swing_GHZ);
    o->gfx = ArtTile_GHZ_MZ_Swing | Tile_Pal3;
    o->render = sprite_cam_field;
    o->priority = 3;
    o->actwid = 48 / 2;
    o->height = 16 / 2;
    OW(o, swing_origY) = o->y.w.i;
    OW(o, swing_origX) = o->x.w.i;
    if (V.zone == id_SLZ) {
      o->map = map_get(MAP_Swing_SLZ);
      o->gfx = ArtTile_SLZ_Swing | Tile_Pal3;
      o->actwid = 64 / 2;
      o->height = 32 / 2;
      o->coltype = col_64x16 | col_hurt;
    }
    if (V.zone == id_SBZ) {
      o->map = map_get(MAP_BBall);
      o->gfx = ArtTile_SBZ_Swing;
      o->actwid = 48 / 2;
      o->height = 48 / 2;
      o->coltype = col_32x32 | col_hurt;
      o->routine = 0xC;
    }
    uint8_t sub = obSubtype(o);
    int d1 = sub & 0xF;
    obSubtype(o) = 0;
    uint8_t *list = &o->s.b[1];
    uint8_t d3 = (uint8_t)(d1 * 16 + 8);
    OB(o, swing_radius) = d3;
    d3 -= 8;
    if (o->frame) d3 += 8, d1--;
    for (int i = 0; i <= d1; i++) {
      Obj *a1 = FindFreeObj();
      if (!a1) break;
      obSubtype(o)++;
      *list++ = SLOT(a1);
      a1->routine = 0xA;
      a1->id = o->id;
      a1->map = o->map;
      a1->gfx = o->gfx & (uint16_t)~0x4000;
      a1->render = sprite_cam_field;
      a1->priority = 4;
      a1->actwid = 16 / 2;
      a1->frame = 1;
      OB(a1, swing_radius) = d3;
      if (d3 < 0x10) {
        a1->frame = 2;
        a1->priority = 3;
        a1->gfx |= 0x4000;
      }
      d3 = (uint8_t)(d3 - 0x10);
    }
    *list++ = SLOT(o);
    set_anglew(o, 0x4080);
    OW(o, GBall_Swing_Speed) = -0x200;
    if (sub & 0x10) {
      o->map = map_get(MAP_GBall);
      o->gfx = ArtTile_GHZ_Giant_Ball | Tile_Pal3;
      o->frame = 1;
      o->priority = 2;
      o->coltype = col_40x40 | col_hurt;
    }
    if (V.zone == id_SBZ) goto swinging;
  }
    /* fall through */
  case 2:
    PlatformObject_CustomHeight(o, o->actwid, o->height);
    /* fall through */
  case 0xC:
  swinging:
    Swing_Move(o);
    DisplaySprite(o);
    Swing_ChkDel(o);
    return;
  case 4: {
    ExitPlatform(o, o->actwid);
    int16_t x = o->x.w.i;
    Swing_Move(o);
    MvSonicOnPtfm(o, x, (int16_t)(o->height + 1));
    DisplaySprite(o);
    Swing_ChkDel(o);
    return;
  }
  case 0xA: DisplaySprite(o); return;
  default: DeleteObject(o); return;
  }
}

/* ------------------------------------------------------------------ the spiked helix */
#define helix_frame 0x3E

static void Hel_RotateSpikes(Obj *o) {
  o->coltype = col_none;
  o->frame = (uint8_t)((V.ani_frame[0] + OB(o, helix_frame)) & 7);
  if (!o->frame) o->coltype = col_8x32 | col_hurt;
}

void Helix(Obj *o) {
  switch (o->routine) {
  case 0: {
    o->routine += 2;
    o->map = map_get(MAP_Hel);
    o->gfx = ArtTile_GHZ_Spike_Pole | Tile_Pal3;
    o->status = 7;
    o->render = sprite_cam_field;
    o->priority = 3;
    o->actwid = 16 / 2;
    int16_t d2 = o->y.w.i, d3 = o->x.w.i;
    uint8_t d1 = obSubtype(o);
    obSubtype(o) = 0;
    uint8_t *list = &o->s.b[1];
    d3 = (int16_t)(d3 - (d1 >> 1) * 16);
    uint8_t d6 = 0;
    if (d1 >= 2) {
      for (int i = 0; i <= d1 - 2; i++) {
        Obj *a1 = FindFreeObj();
        if (!a1) break;
        obSubtype(o)++;
        *list++ = SLOT(a1);
        a1->routine = 8;
        a1->id = o->id;
        a1->y.w.i = d2;
        a1->x.w.i = d3;
        a1->map = o->map;
        a1->gfx = ArtTile_GHZ_Spike_Pole | Tile_Pal3;
        a1->render = sprite_cam_field;
        a1->priority = 3;
        a1->actwid = 16 / 2;
        OB(a1, helix_frame) = d6;
        d6 = (d6 + 1) & 7;
        d3 = (int16_t)(d3 + 0x10);
        if (d3 == o->x.w.i) {
          OB(o, helix_frame) = d6;
          d6 = (d6 + 1) & 7;
          d3 = (int16_t)(d3 + 0x10);
          obSubtype(o)++;
        }
      }
    }
  }
    /* fall through */
  case 2:
  case 4:
    Hel_RotateSpikes(o);
    DisplaySprite(o);
    if (obj_far(o)) {
      int n = obSubtype(o);
      for (int i = 0; i < n - 1; i++) DeleteObject(CHILD(o, i));
      DeleteObject(o);
    }
    return;
  case 8:
    Hel_RotateSpikes(o);
    DisplaySprite(o);
    return;
  default: DeleteObject(o); return;
  }
}

/* ------------------------------------------------------------------ platforms */
#define plat_rawY 0x2C
#define plat_origX 0x32
#define plat_origY 0x34
#define plat_nudge 0x38
#define plat_delay 0x3A

static void Plat_Move(Obj *o) {
  Obj *a1 = player;
  int8_t d1;
  switch (obSubtype(o) & 0xF) {
  case 0:
  case 9: return;
  case 1:
    d1 = (int8_t)(o->angle - 0x40);
    o->x.w.i = (int16_t)(OW(o, plat_origX) + d1);
    break;
  case 5:
    d1 = (int8_t)(-o->angle + 0x40);
    o->x.w.i = (int16_t)(OW(o, plat_origX) + d1);
    break;
  case 2:
    d1 = (int8_t)(o->angle - 0x40);
    OFIX(o, plat_rawY).w.i = (int16_t)(OW(o, plat_origY) + d1);
    break;
  case 6:
    d1 = (int8_t)(-o->angle + 0x40);
    OFIX(o, plat_rawY).w.i = (int16_t)(OW(o, plat_origY) + d1);
    break;
  case 0xB:
    d1 = (int8_t)(OSC_B(0xE) - 0x30);
    OFIX(o, plat_rawY).w.i = (int16_t)(OW(o, plat_origY) + d1);
    break;
  case 0xC:
    d1 = (int8_t)(-OSC_B(0xE) + 0x30);
    OFIX(o, plat_rawY).w.i = (int16_t)(OW(o, plat_origY) + d1);
    break;
  case 0xA:
    d1 = (int8_t)(o->angle - 0x40);
    OFIX(o, plat_rawY).w.i = (int16_t)(OW(o, plat_origY) + (d1 >> 1));
    break;
  case 3:
    if (!OW(o, plat_delay)) {
      if (o->status & status_on_object) OW(o, plat_delay) = 30;
    } else if (!--OW(o, plat_delay)) {
      OW(o, plat_delay) = 32;
      obSubtype(o)++;
    }
    return;
  case 4:
    if (OW(o, plat_delay) && !--OW(o, plat_delay)) {
      if (o->status & status_on_object) {
        a1->status |= status_in_air;
        a1->status &= (uint8_t)~status_on_object;
        a1->routine = 2;
        o->status &= (uint8_t)~status_on_object;
        o->routine2 = 0;
        a1->vy = o->vy;
      }
      o->routine = 8;
    }
    OFIX(o, plat_rawY).l += (int32_t)o->vy << 8;
    o->vy = (int16_t)(o->vy + 0x38);
    if ((uint16_t)(V.limitbtm2 + 224) < (uint16_t)OFIX(o, plat_rawY).w.i) o->routine = 6;
    return;
  case 7:
    if (!OW(o, plat_delay)) {
      if (V.f_switch[obSubtype(o) >> 4]) OW(o, plat_delay) = 60;
    } else if (!--OW(o, plat_delay)) obSubtype(o)++;
    return;
  case 8:
    OFIX(o, plat_rawY).w.i -= 2;
    if ((int16_t)(OW(o, plat_origY) - 0x200) == OFIX(o, plat_rawY).w.i) obSubtype(o) = 0;
    return;
  default: return;
  }
  o->angle = OSC_B(0x1A);
}

static void Plat_Nudge(Obj *o) {
  int16_t s = CalcSine(OB(o, plat_nudge), NULL);
  o->y.w.i = (int16_t)(((int32_t)s * 0x400 >> 16) + OFIX(o, plat_rawY).w.i);
}

void BasicPlatform(Obj *o) {
  switch (o->routine) {
  case 0:
    o->routine += 2;
    o->gfx = ArtTile_Level | Tile_Pal3;
    o->map = map_get(MAP_Plat_GHZ);
    o->actwid = 64 / 2;
    if (V.zone == id_SYZ) o->map = map_get(MAP_Plat_SYZ);
    if (V.zone == id_SLZ) {
      o->map = map_get(MAP_Plat_SLZ);
      obSubtype(o) = 3;
    }
    o->render = sprite_cam_field;
    o->priority = 4;
    OFIX(o, plat_rawY).w.i = o->y.w.i;
    OW(o, plat_origY) = o->y.w.i;
    OW(o, plat_origX) = o->x.w.i;
    set_anglew(o, 0x80);
    o->frame = 0;
    if (obSubtype(o) == 0xA) o->frame = 1, o->actwid = 64 / 2;
    /* fall through */
  case 2:
    if (OB(o, plat_nudge)) OB(o, plat_nudge) -= 4;
    PlatformObject(o, o->actwid);
    /* fall through */
  case 8:
    Plat_Move(o);
    Plat_Nudge(o);
    break;
  case 4: {
    if (OB(o, plat_nudge) != 0x40) OB(o, plat_nudge) += 4;
    ExitPlatform(o, o->actwid);
    int16_t x = o->x.w.i;
    Plat_Move(o);
    Plat_Nudge(o);
    MvSonicOnPtfm2(o, x);
    break;
  }
  default: DeleteObject(o); return;
  }
  DisplaySprite(o);
  if (obj_far_x(OW(o, plat_origX))) DeleteObject(o);
}

/* ------------------------------------------------------------------ collapsing ledges and floors */
#define collapsible_timedelay 0x38
#define collapsible_flag 0x3A

static const uint8_t Ledge_SlopeData[48] = {
  0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x21, 0x21, 0x22, 0x22, 0x23, 0x23, 0x24, 0x24,
  0x25, 0x25, 0x26, 0x26, 0x27, 0x27, 0x28, 0x28, 0x29, 0x29, 0x2A, 0x2A, 0x2B, 0x2B, 0x2C, 0x2C,
  0x2D, 0x2D, 0x2E, 0x2E, 0x2F, 0x2F, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30,
};

/* the platform's frame as pieces that fall one after another */
static void FragmentatePlatform(Obj *o, const uint8_t *delays, int n) {
  const uint8_t *p = map_frame(o->map, o->frame) + 2;
  o->render |= sprite_rawmappings;
  uint8_t id = o->id, r = o->render;
  Obj *a1 = o;
  for (int i = 0; i < n; i++) {
    if (i) {
      a1 = FindFreeObj();
      if (!a1) break;
      p += 6;
    }
    a1->routine = 6;
    a1->id = id;
    a1->map = p;
    a1->render = r;
    a1->x.w.i = o->x.w.i;
    a1->y.w.i = o->y.w.i;
    a1->gfx = o->gfx;
    a1->priority = o->priority;
    a1->actwid = o->actwid;
    OB(a1, collapsible_timedelay) = delays[i];
    if (a1 < o) DisplaySprite(a1);
  }
  DisplaySprite(o);
}

static void Fragmentate_GHZLedge(Obj *o) {
  static const uint8_t d[25] = {0x1C, 0x18, 0x14, 0x10, 0x1A, 0x16, 0x12, 0x0E, 0x0A, 0x06, 0x18, 0x14, 0x10,
                                0x0C, 0x08, 0x04, 0x16, 0x12, 0x0E, 0x0A, 0x06, 0x02, 0x14, 0x10, 0x0C};
  o->frame += 2;
  FragmentatePlatform(o, d, 25);
}

static void Fragmentate_8x2Floor(Obj *o) {
  static const uint8_t swipe[8] = {0x1E, 0x16, 0x0E, 0x06, 0x1A, 0x12, 0x0A, 0x02};
  static const uint8_t shuffle[8] = {0x16, 0x1E, 0x1A, 0x12, 0x06, 0x0E, 0x0A, 0x02};
  o->frame++;
  FragmentatePlatform(o, (obSubtype(o) & 1) ? shuffle : swipe, 8);
}

/* a piece: waits its delay (the one under Sonic holds him a little longer), then falls */
static void collapse_piece(Obj *o, void (*walkoff)(Obj *)) {
  if (!OB(o, collapsible_timedelay)) {
    ObjectFall(o);
    DisplaySprite(o);
    if (!(o->render & 0x80)) DeleteObject(o);
    return;
  }
  if (!OB(o, collapsible_flag)) {
    OB(o, collapsible_timedelay)--;
    DisplaySprite(o);
    return;
  }
  OB(o, collapsible_timedelay)--;
  walkoff(o);
  if (player->status & status_on_object) {
    if (OB(o, collapsible_timedelay)) return;
    player->status &= (uint8_t)~(status_on_object | status_pushing);
    player->prevani = id_Run;
  }
  OB(o, collapsible_flag) = 0;
  o->routine = 6;
}

static void Ledge_WalkOff(Obj *o) {
  ExitPlatform(o, 96 / 2);
  SlopeObject_AssumeStoodOn(o, 96 / 2, Ledge_SlopeData, o->x.w.i);
  RememberState(o);
}

void CollapseLedge(Obj *o) {
  switch (o->routine) {
  case 0:
    o->routine += 2;
    o->map = map_get(MAP_Ledge);
    o->gfx = ArtTile_Level | Tile_Pal3;
    o->render |= sprite_cam_field;
    o->priority = 4;
    OB(o, collapsible_timedelay) = 7;
    o->actwid = 200 / 2;
    o->frame = obSubtype(o);
    o->height = 112 / 2;
    o->render |= 0x10;
    /* fall through */
  case 2:
    if (OB(o, collapsible_flag)) {
      if (!OB(o, collapsible_timedelay)) {
        OB(o, collapsible_flag) = 0;
        Fragmentate_GHZLedge(o);
        return;
      }
      OB(o, collapsible_timedelay)--;
    }
    SlopeObject(o, 96 / 2, Ledge_SlopeData);
    RememberState(o);
    return;
  case 4:
    if (!OB(o, collapsible_timedelay)) {
      Fragmentate_GHZLedge(o);
      return;
    }
    OB(o, collapsible_flag) = 1;
    OB(o, collapsible_timedelay)--;
    /* fall through */
  case 0xA: Ledge_WalkOff(o); return;
  case 6: collapse_piece(o, Ledge_WalkOff); return;
  default: DeleteObject(o); return;
  }
}

static void CFlo_WalkOff(Obj *o) {
  ExitPlatform(o, 64 / 2);
  MvSonicOnPtfm2(o, o->x.w.i);
  RememberState(o);
}

void CollapseFloor(Obj *o) {
  switch (o->routine) {
  case 0:
    o->routine += 2;
    o->map = map_get(MAP_CFlo);
    o->gfx = ArtTile_MZ_Block | Tile_Pal3;
    if (V.zone == id_SLZ) {
      o->gfx = ArtTile_SLZ_Collapsing_Floor | Tile_Pal3;
      o->frame += 2;
    }
    if (V.zone == id_SBZ) o->gfx = ArtTile_SBZ_Collapsing_Floor | Tile_Pal3;
    o->render |= sprite_cam_field;
    o->priority = 4;
    OB(o, collapsible_timedelay) = 7;
    o->actwid = 136 / 2;
    /* fall through */
  case 2:
    if (OB(o, collapsible_flag)) {
      if (!OB(o, collapsible_timedelay)) {
        OB(o, collapsible_flag) = 0;
        Fragmentate_8x2Floor(o);
        return;
      }
      OB(o, collapsible_timedelay)--;
    }
    PlatformObject(o, 64 / 2);
    if ((obSubtype(o) & 0x80) && (player->status & status_on_object)) {
      o->render &= (uint8_t)~sprite_xflip;
      if ((uint16_t)player->x.w.i < (uint16_t)o->x.w.i) o->render |= sprite_xflip;
    }
    RememberState(o);
    return;
  case 4:
    if (!OB(o, collapsible_timedelay)) {
      Fragmentate_8x2Floor(o);
      return;
    }
    OB(o, collapsible_flag) = 1;
    OB(o, collapsible_timedelay)--;
    /* fall through */
  case 0xA: CFlo_WalkOff(o); return;
  case 6: collapse_piece(o, CFlo_WalkOff); return;
  default: DeleteObject(o); return;
  }
}

/* ------------------------------------------------------------------ scenery, the purple rock, smashable walls */
void Scenery(Obj *o) {
  if (o->routine == 0) {
    o->routine += 2;
    if (obSubtype(o) == 3) {
      o->map = map_get(MAP_Bri);
      o->gfx = ArtTile_GHZ_Bridge | Tile_Pal3;
      o->frame = 1, o->actwid = 32 / 2, o->priority = 1;
    } else {
      o->map = map_get(MAP_Scen);
      o->gfx = ArtTile_SLZ_Fireball_Launcher | Tile_Pal3;
      o->frame = 0, o->actwid = 16 / 2, o->priority = 2;
    }
    o->render |= sprite_cam_field;
    o->coltype = col_none;
  }
  if (obj_far(o)) {
    DeleteObject(o);
    return;
  }
  DisplaySprite(o);
}

void PurpleRock(Obj *o) {
  if (o->routine == 0) {
    o->routine += 2;
    o->map = map_get(MAP_PRock);
    o->gfx = ArtTile_GHZ_Purple_Rock | Tile_Pal4;
    o->render = sprite_cam_field;
    o->actwid = 38 / 2;
    o->priority = 4;
  }
  SolidObject(o, 32 / 2 + sonic_solid_width, 32 / 2, 32 / 2, o->x.w.i);
  DisplaySprite(o);
  if (obj_far(o)) DeleteObject(o);
}

#define smash_speed 0x30

void SmashWall(Obj *o) {
  static const int16_t spd1[16] = {0x400, -0x500, 0x600, -0x100, 0x600, 0x100, 0x400, 0x500,
                                   0x600, -0x600, 0x800, -0x200, 0x800, 0x200, 0x600, 0x600};
  static const int16_t spd2[16] = {-0x600, -0x600, -0x800, -0x200, -0x800, 0x200, -0x600, 0x600,
                                   -0x400, -0x500, -0x600, -0x100, -0x600, 0x100, -0x400, 0x500};
  Obj *a1 = player;
  switch (o->routine) {
  case 0:
    o->routine += 2;
    o->map = map_get(MAP_Smash);
    o->gfx = ArtTile_GHZ_SLZ_Smashable_Wall | Tile_Pal3;
    o->render = sprite_cam_field;
    o->actwid = 32 / 2;
    o->priority = 4;
    o->frame = obSubtype(o);
    /* fall through */
  case 2: {
    OW(o, smash_speed) = a1->vx;
    SolidObject(o, 32 / 2 + sonic_solid_width, 64 / 2, 64 / 2, o->x.w.i);
    if (!(o->status & status_pushing) || a1->anim != id_Roll) break;
    int16_t sp = OW(o, smash_speed);
    if ((uint16_t)(sp < 0 ? -sp : sp) < 0x480) break;
    a1->vx = sp;
    a1->x.w.i = (int16_t)(a1->x.w.i + 4);
    const int16_t *speeds = spd1;
    if ((uint16_t)o->x.w.i >= (uint16_t)a1->x.w.i) {
      a1->x.w.i = (int16_t)(a1->x.w.i - 8);
      speeds = spd2;
    }
    a1->inertia = a1->vx;
    o->status &= (uint8_t)~status_pushing;
    a1->status &= (uint8_t)~status_pushing;
    SmashObject(o, speeds, 8, 0x38 * 2);
  }
    /* fall through */
  case 4:
    SpeedToPos(o);
    o->vy = (int16_t)(o->vy + 0x38 * 2);
    DisplaySprite(o);
    if (!(o->render & 0x80)) DeleteObject(o);
    break;
  }
  RememberState(o);
}

/* ------------------------------------------------------------------ Motobug */
#define moto_ledgewait 0x30
#define moto_smokewait 0x33

void MotoBug(Obj *o) {
  switch (o->routine) {
  case 0:
    o->map = map_get(MAP_Moto);
    o->gfx = ArtTile_Moto_Bug;
    o->render = sprite_cam_field;
    o->priority = 4;
    o->actwid = 40 / 2;
    if (o->anim) {
      o->routine += 4;
      goto smoke;
    }
    o->height = 28 / 2;
    o->width = 16 / 2;
    o->coltype = col_40x32 | col_badnik;
    ObjectFall(o);
    {
      int16_t d1 = ObjFloorDist(o, NULL);
      if (d1 < 0) {
        o->y.w.i = (int16_t)(o->y.w.i + d1);
        o->vy = 0;
        o->routine += 2;
        o->status ^= 1;
      }
    }
    return;
  case 2:
    if (o->routine2 == 0) {
      if (--OW(o, moto_ledgewait) < 0) {
        o->routine2 += 2;
        o->vx = -0x100;
        o->anim = 1;
        o->status ^= 1;
        if (o->status & 1) o->vx = 0x100;
      }
    } else {
      SpeedToPos(o);
      int16_t d1 = ObjFloorDist(o, NULL);
      if (d1 < -8 || d1 >= 0xC) {
        o->routine2 -= 2;
        OW(o, moto_ledgewait) = 60 - 1;
        o->vx = 0;
        o->anim = 0;
      } else {
        o->y.w.i = (int16_t)(o->y.w.i + d1);
        if ((int8_t)--OB(o, moto_smokewait) < 0) {
          OB(o, moto_smokewait) = 16 - 1;
          Obj *a1 = FindFreeObj();
          if (a1) {
            a1->id = id_MotoBug;
            a1->x.w.i = o->x.w.i;
            a1->y.w.i = o->y.w.i;
            a1->status = o->status;
            a1->anim = 2;
          }
        }
      }
    }
    AnimateSprite(o, ani_get(ANI_Moto));
    RememberState(o);
    return;
  case 4:
  smoke:
    AnimateSprite(o, ani_get(ANI_Moto));
    DisplaySprite(o);
    return;
  default: DeleteObject(o); return;
  }
}

/* ------------------------------------------------------------------ Crabmeat */
#define crab_timedelay 0x30
#define crab_flags 0x32

static uint8_t Crab_SetAni(Obj *o) {
  uint8_t d3 = o->angle;
  if (d3 & 0x80) {
    if (d3 > 0xFA) return 0;
    return o->status & 1 ? 2 : 1;
  }
  if (d3 < 6) return 0;
  return o->status & 1 ? 1 : 2;
}

void Crabmeat(Obj *o) {
  switch (o->routine) {
  case 0: {
    o->height = 32 / 2;
    o->width = 16 / 2;
    o->map = map_get(MAP_Crab);
    o->gfx = ArtTile_Crabmeat;
    o->render = sprite_cam_field;
    o->priority = 3;
    o->coltype = col_badnik | col_32x32;
    o->actwid = 42 / 2;
    ObjectFall(o);
    uint8_t a;
    int16_t d1 = ObjFloorDist(o, &a);
    if (d1 < 0) {
      o->y.w.i = (int16_t)(o->y.w.i + d1);
      o->angle = a;
      o->vy = 0;
      o->routine += 2;
    }
    return;
  }
  case 2:
    if (o->routine2 == 0) {
      if (--OW(o, crab_timedelay) >= 0) goto animate;
      if (o->render & 0x80) {
        uint8_t old = OB(o, crab_flags);
        OB(o, crab_flags) ^= 2;
        if (old & 2) {
          OW(o, crab_timedelay) = 60 - 1;
          o->anim = 6;
          for (int side = -1; side <= 1; side += 2) {
            Obj *a1 = FindFreeObj();
            if (!a1) continue;
            a1->id = id_Crabmeat;
            a1->routine = 6;
            a1->x.w.i = (int16_t)(o->x.w.i + side * 0x10);
            a1->y.w.i = o->y.w.i;
            a1->vx = (int16_t)(side * 0x100);
          }
          goto animate;
        }
      }
      o->routine2 += 2;
      OW(o, crab_timedelay) = 128 - 1;
      o->vx = 0x80;
      o->anim = (uint8_t)(Crab_SetAni(o) + 3);
      o->status ^= 1;
      if (o->status & 1) o->vx = (int16_t)-o->vx;
    } else {
      if (--OW(o, crab_timedelay) < 0) goto fire;
      SpeedToPos(o);
      uint8_t old = OB(o, crab_flags);
      OB(o, crab_flags) ^= 1;
      if (old & 1) {
        uint8_t a;
        int16_t d1 = ObjFloorDist(o, &a);
        o->y.w.i = (int16_t)(o->y.w.i + d1);
        o->angle = a;
        o->anim = (uint8_t)(Crab_SetAni(o) + 3);
      } else {
        int16_t d3 = (int16_t)(o->x.w.i + 16);
        if (o->status & 1) d3 = (int16_t)(d3 - 32);
        int16_t d1 = ObjFloorDist2(o, d3, NULL);
        if (d1 < -8 || d1 >= 0xC) goto fire;
      }
    }
    goto animate;
  fire:
    o->routine2 -= 2;
    OW(o, crab_timedelay) = 60 - 1;
    o->vx = 0;
    o->anim = Crab_SetAni(o);
  animate:
    AnimateSprite(o, ani_get(ANI_Crab));
    RememberState(o);
    return;
  case 6:
    o->routine += 2;
    o->map = map_get(MAP_Crab);
    o->gfx = ArtTile_Crabmeat;
    o->render = sprite_cam_field;
    o->priority = 3;
    o->coltype = col_12x12 | col_hurt;
    o->actwid = 16 / 2;
    o->vy = -0x400;
    o->anim = 7;
    /* fall through */
  case 8:
    AnimateSprite(o, ani_get(ANI_Crab));
    ObjectFall(o);
    DisplaySprite(o);
    if ((uint16_t)(V.limitbtm2 + 224) < (uint16_t)o->y.w.i) DeleteObject(o);
    return;
  default: DeleteObject(o); return;
  }
}

/* ------------------------------------------------------------------ Buzz Bomber and missiles */
#define buzz_timedelay 0x32
#define buzz_buzzstate 0x34
#define msl_timedelay 0x32
#define msl_parent 0x3C

void BuzzBomber(Obj *o) {
  switch (o->routine) {
  case 0:
    o->routine += 2;
    o->map = map_get(MAP_Buzz);
    o->gfx = ArtTile_Buzz_Bomber;
    o->render = sprite_cam_field;
    o->priority = 3;
    o->coltype = col_48x24 | col_badnik;
    o->actwid = 48 / 2;
    /* fall through */
  case 2:
    if (o->routine2 == 0) {
      if (--OW(o, buzz_timedelay) >= 0) break;
      if (OB(o, buzz_buzzstate) & 2) {
        Obj *a1 = FindFreeObj();
        if (!a1) break;
        a1->id = id_Missile;
        a1->x.w.i = o->x.w.i;
        a1->y.w.i = (int16_t)(o->y.w.i + 0x1C);
        a1->vy = 0x200;
        a1->vx = 0x200;
        int16_t d0 = 0x18;
        if (!(o->status & 1)) d0 = -d0, a1->vx = (int16_t)-a1->vx;
        a1->x.w.i = (int16_t)(a1->x.w.i + d0);
        a1->status = o->status;
        OW(a1, msl_timedelay) = 15 - 1;
        OUW(a1, msl_parent) = OADDR(o);
        OB(o, buzz_buzzstate) = 1;
        OW(o, buzz_timedelay) = 60 - 1;
        o->anim = 2;
        break;
      }
      o->routine2 += 2;
      OW(o, buzz_timedelay) = 128 - 1;
      o->vx = 0x400;
      o->anim = 1;
      if (!(o->status & 1)) o->vx = (int16_t)-o->vx;
    } else {
      if (--OW(o, buzz_timedelay) < 0) {
        OB(o, buzz_buzzstate) = 0;
        o->status ^= 1;
        OW(o, buzz_timedelay) = 60 - 1;
      } else {
        SpeedToPos(o);
        if (OB(o, buzz_buzzstate)) break;
        int16_t d0 = (int16_t)(player->x.w.i - o->x.w.i);
        if (d0 < 0) d0 = (int16_t)-d0;
        if ((uint16_t)d0 >= 96 || !(o->render & 0x80)) break;
        OB(o, buzz_buzzstate) = 2;
        OW(o, buzz_timedelay) = 30 - 1;
      }
      o->routine2 -= 2;
      o->vx = 0;
      o->anim = 0;
    }
    break;
  default: DeleteObject(o); return;
  }
  AnimateSprite(o, ani_get(ANI_Buzz));
  RememberState(o);
}

static bool Msl_ChkCancel(Obj *o) {
  if (!OUW(o, msl_parent)) return false;
  if (OPTR(OUW(o, msl_parent))->id != id_ExplosionItem) return false;
  DeleteObject(o);
  return true;
}

void Missile(Obj *o) {
  switch (o->routine) {
  case 0:
    if (--OW(o, msl_timedelay) >= 0) {
      Msl_ChkCancel(o);
      return;
    }
    o->routine += 2;
    o->map = map_get(MAP_Missile);
    o->gfx = ArtTile_Buzz_Bomber | Tile_Pal2;
    o->render = sprite_cam_field;
    o->priority = 3;
    o->actwid = 16 / 2;
    o->status &= 3;
    if (obSubtype(o)) {
      o->routine = 8;
      o->coltype = col_12x12 | col_hurt;
      o->anim = 1;
      goto newt_animate;
    }
    /* fall through */
  case 2:
    if (Msl_ChkCancel(o)) return;
    AnimateSprite(o, ani_get(ANI_Missile));
    DisplaySprite(o);
    return;
  case 4:
    if (o->status & status_defeated) {
      o->id = id_UnusedExplosion;
      o->routine = 0;
      UnusedExplosion(o);
      return;
    }
    o->coltype = col_12x12 | col_hurt;
    o->anim = 1;
    SpeedToPos(o);
    AnimateSprite(o, ani_get(ANI_Missile));
    DisplaySprite(o);
    if ((uint16_t)(V.limitbtm2 + 224) < (uint16_t)o->y.w.i) DeleteObject(o);
    return;
  case 8:
    if (!(o->render & 0x80)) {
      DeleteObject(o);
      return;
    }
    SpeedToPos(o);
  newt_animate:
    AnimateSprite(o, ani_get(ANI_Missile));
    DisplaySprite(o);
    return;
  default: DeleteObject(o); return;
  }
}

void UnusedExplosion(Obj *o) {
  if (o->routine == 0) {
    o->routine += 2;
    o->map = map_get(MAP_UnkExplode);
    o->gfx = ArtTile_UnusedExplosion;
    o->render = sprite_cam_field;
    o->priority = 1;
    o->coltype = col_none;
    o->actwid = 24 / 2;
    o->timeframe = 10 - 1;
    o->frame = 0;
  }
  if ((int8_t)--o->timeframe < 0) {
    o->timeframe = 10 - 1;
    if (++o->frame == 4) {
      DeleteObject(o);
      return;
    }
  }
  DisplaySprite(o);
}

/* ------------------------------------------------------------------ Chopper */
#define chop_origY 0x30

void Chopper(Obj *o) {
  if (o->routine == 0) {
    o->routine += 2;
    o->map = map_get(MAP_Chop);
    o->gfx = ArtTile_Chopper;
    o->render = sprite_cam_field;
    o->priority = 4;
    o->coltype = col_24x32 | col_badnik;
    o->actwid = 32 / 2;
    o->vy = -0x700;
    OW(o, chop_origY) = o->y.w.i;
  }
  AnimateSprite(o, ani_get(ANI_Chop));
  SpeedToPos(o);
  o->vy = (int16_t)(o->vy + 0x18);
  uint16_t d0 = OUW(o, chop_origY);
  if (d0 < (uint16_t)o->y.w.i) {
    o->y.w.i = (int16_t)d0;
    o->vy = -0x700;
  }
  o->anim = 1;
  d0 = (uint16_t)(d0 - 0xC0);
  if (d0 < (uint16_t)o->y.w.i) {
    o->anim = 0;
    if (o->vy >= 0) o->anim = 2;
  }
  RememberState(o);
}

/* ------------------------------------------------------------------ Newtron */
#define newt_fired 0x32

void Newtron(Obj *o) {
  switch (o->routine) {
  case 0:
    o->routine += 2;
    o->map = map_get(MAP_Newt);
    o->gfx = ArtTile_Newtron;
    o->render = sprite_cam_field;
    o->priority = 4;
    o->actwid = 40 / 2;
    o->height = 32 / 2;
    o->width = 16 / 2;
    /* fall through */
  case 2:
    switch (o->routine2) {
    case 0: {
      o->status |= 1;
      int16_t d0 = (int16_t)(player->x.w.i - o->x.w.i);
      if ((uint16_t)player->x.w.i < (uint16_t)o->x.w.i) {
        d0 = (int16_t)-d0;
        o->status &= (uint8_t)~1;
      }
      if ((uint16_t)d0 >= 128) break;
      o->routine2 += 2;
      o->anim = 1;
      if (obSubtype(o)) {
        o->gfx = ArtTile_Newtron | Tile_Pal2;
        o->routine2 = 8;
        o->anim = 4;
      }
      break;
    }
    case 2:
      if (o->frame < 4) {
        o->status |= 1;
        if ((uint16_t)player->x.w.i < (uint16_t)o->x.w.i) o->status &= (uint8_t)~1;
        break;
      }
      if (o->frame == 1) o->coltype = col_40x32 | col_badnik;
      ObjectFall(o);
      {
        int16_t d1 = ObjFloorDist(o, NULL);
        if (d1 >= 0) break;
        o->y.w.i = (int16_t)(o->y.w.i + d1);
      }
      o->vy = 0;
      o->routine2 += 2;
      o->anim = 2;
      if (o->gfx & 0x2000) o->anim++;
      o->coltype = col_40x16 | col_badnik;
      o->vx = 0x200;
      if (!(o->status & 1)) o->vx = (int16_t)-o->vx;
      break;
    case 4: {
      SpeedToPos(o);
      int16_t d1 = ObjFloorDist(o, NULL);
      if (d1 < -8 || d1 >= 0xC) o->routine2 += 2;
      else o->y.w.i = (int16_t)(o->y.w.i + d1);
      break;
    }
    case 6: SpeedToPos(o); break;
    case 8: {
      if (o->frame == 1) o->coltype = col_40x32 | col_badnik;
      if (o->frame != 2 || OB(o, newt_fired)) break;
      OB(o, newt_fired) = 1;
      Obj *a1 = FindFreeObj();
      if (!a1) break;
      a1->id = id_Missile;
      a1->x.w.i = o->x.w.i;
      a1->y.w.i = (int16_t)(o->y.w.i - 8);
      a1->vx = 0x200;
      int16_t d0 = 0x14;
      if (!(o->status & 1)) d0 = -d0, a1->vx = (int16_t)-a1->vx;
      a1->x.w.i = (int16_t)(a1->x.w.i + d0);
      a1->status = o->status;
      obSubtype(a1) = 1;
      break;
    }
    }
    AnimateSprite(o, ani_get(ANI_Newt));
    RememberState(o);
    return;
  default: DeleteObject(o); return;
  }
}

/* ------------------------------------------------------------------ the boss */
#define BGHZ_ParentObj 0x34
#define BGHZ_BossGenericTimer 0x3C
#define BGHZ_SineCounter 0x3F

/* a boss's sprite flipped as it faces */
static void boss_flip(Obj *o) {
  o->render = (uint8_t)((o->render & ~3) | (o->status & 3));
}

/* hit: flashing white, untouchable for a moment */
void boss_hit_flash(Obj *o, uint8_t restore) {
  if (!OB(o, obBossFlash)) OB(o, obBossFlash) = 0x20;
  uint16_t *c = &V.palette[0x11];
  *c = *c ? 0 : 0xEEE;
  if (!--OB(o, obBossFlash)) o->coltype = restore;
}

static void BGHZ_ShipUpdate(Obj *o) {
  int16_t s = CalcSine(OB(o, BGHZ_SineCounter), NULL);
  o->y.w.i = (int16_t)((s >> 6) + obBossY(o).w.i);
  o->x.w.i = obBossX(o).w.i;
  OB(o, BGHZ_SineCounter) += 2;
  if (o->routine2 >= 8) return;
  if (o->status & 0x80) {
    AddPoints(100);
    o->routine2 = 8;
    OW(o, BGHZ_BossGenericTimer) = 0xB3;
    return;
  }
  if (o->coltype) return;
  boss_hit_flash(o, col_48x48 | col_boss);
}

void BossGreenHill(Obj *o) {
  switch (o->routine) {
  case 0: {
    static const uint8_t data[3][2] = {{2, 0}, {4, 1}, {6, 7}};
    Obj *a1 = o;
    for (int i = 0; i < 3; i++) {
      if (i) {
        a1 = FindNextFreeObj(o);
        if (!a1) break;
      }
      a1->routine = data[i][0];
      a1->id = id_BossGreenHill;
      a1->x.w.i = o->x.w.i;
      a1->y.w.i = o->y.w.i;
      a1->map = map_get(MAP_Eggman);
      a1->gfx = ArtTile_Eggman;
      a1->render = sprite_cam_field;
      a1->actwid = 64 / 2;
      a1->priority = 3;
      a1->anim = data[i][1];
      OUW(a1, BGHZ_ParentObj) = OADDR(o);
    }
    obBossX(o).w.i = o->x.w.i;
    obBossY(o).w.i = o->y.w.i;
    o->coltype = col_48x48 | col_boss;
    obBossHits(o) = 8;
  }
    /* fall through */
  case 2:
    switch (o->routine2) {
    case 0:
      o->vy = 0x100;
      BossMove(o);
      if (obBossY(o).w.i == boss_ghz_y + 0x38) {
        o->vy = 0;
        o->routine2 += 2;
      }
      BGHZ_ShipUpdate(o);
      break;
    case 2:
      o->vx = -0x100;
      o->vy = -0x40;
      BossMove(o);
      if (obBossX(o).w.i == boss_ghz_x + 0xA0) {
        o->vx = o->vy = 0;
        o->routine2 += 2;
        Obj *a1 = FindNextFreeObj(o);
        if (a1) {
          a1->id = id_BossBall;
          a1->x.w.i = obBossX(o).w.i;
          a1->y.w.i = obBossY(o).w.i;
          OUW(a1, BGHZ_ParentObj) = OADDR(o);
        }
        OW(o, BGHZ_BossGenericTimer) = 120 - 1;
      }
      BGHZ_ShipUpdate(o);
      break;
    case 4:
      if (--OW(o, BGHZ_BossGenericTimer) < 0) {
        o->routine2 += 2;
        OW(o, BGHZ_BossGenericTimer) = 0x40 - 1;
        o->vx = 0x100;
        if (obBossX(o).w.i == boss_ghz_x + 0xA0) {
          OW(o, BGHZ_BossGenericTimer) = 0x40 * 2 - 1;
          o->vx = 0x40;
        }
      }
      if (!(o->status & 1)) o->vx = (int16_t)-o->vx;
      BGHZ_ShipUpdate(o);
      break;
    case 6:
      if (--OW(o, BGHZ_BossGenericTimer) < 0) {
        o->status ^= 1;
        OW(o, BGHZ_BossGenericTimer) = 64 - 1;
        o->routine2 -= 2;
        o->vx = 0;
      } else BossMove(o);
      BGHZ_ShipUpdate(o);
      break;
    case 8:
      if (--OW(o, BGHZ_BossGenericTimer) >= 0) {
        BossDefeated(o);
        break;
      }
      o->status |= 1;
      o->status &= 0x7F;
      o->vx = 0;
      o->routine2 += 2;
      OW(o, BGHZ_BossGenericTimer) = -38;
      if (!V.bossstatus) V.bossstatus = 1;
      break;
    case 0xA: {
      int16_t t = ++OW(o, BGHZ_BossGenericTimer);
      if (t == 0) o->vy = 0;
      else if (t < 0) o->vy = (int16_t)(o->vy + 0x18);
      else if ((uint16_t)t < 0x30) o->vy = (int16_t)(o->vy - 8);
      else if (t == 0x30) o->vy = 0;
      else if ((uint16_t)t >= 0x38) o->routine2 += 2;
      BossMove(o);
      BGHZ_ShipUpdate(o);
      break;
    }
    case 0xC:
      o->vx = 0x400;
      o->vy = -0x40;
      if (V.limitright2 != boss_ghz_end) V.limitright2 += 2;
      else if (!(o->render & 0x80)) {
        DeleteObject(o);
        return;
      }
      BossMove(o);
      BGHZ_ShipUpdate(o);
      break;
    }
    AnimateSprite(o, ani_get(ANI_Eggman));
    boss_flip(o);
    DisplaySprite(o);
    return;
  case 4: {
    Obj *a1 = OPTR(OUW(o, BGHZ_ParentObj));
    uint8_t d1 = 1;
    int8_t d0 = (int8_t)(a1->routine2 - 4);
    if (d0 == 0 && obBossX(a1).w.i == boss_ghz_x + 0xA0) d1 = 4;
    if (d0 != 0) {
      d0 = (int8_t)(d0 - 6);
      if (d0 >= 0) {
        d1 = 0xA;
        goto face;
      }
    } else d0 = -6;
    if (!a1->coltype) d1 = 5;
    else if (player->routine >= 4) d1 = 4;
  face:
    o->anim = d1;
    if ((int8_t)(d0 - 2) == 0) {
      o->anim = 6;
      if (!(o->render & 0x80)) {
        DeleteObject(o);
        return;
      }
    }
    goto display;
  }
  case 6: {
    Obj *a1 = OPTR(OUW(o, BGHZ_ParentObj));
    o->anim = 7;
    if (a1->routine2 == 0xC) {
      o->anim = 0xB;
      if (!(o->render & 0x80)) {
        DeleteObject(o);
        return;
      }
    } else if (a1->vx) o->anim = 8;
    goto display;
  }
  }
  return;
display: {
  Obj *a1 = OPTR(OUW(o, BGHZ_ParentObj));
  o->x.w.i = a1->x.w.i;
  o->y.w.i = a1->y.w.i;
  o->status = a1->status;
  AnimateSprite(o, ani_get(ANI_Eggman));
  boss_flip(o);
  DisplaySprite(o);
}
}

/* the wrecking ball: a chain from the ship that swings once lowered */
static void GBall_UpdateBase(Obj *o) {
  Obj *a1 = OPTR(OUW(o, BGHZ_ParentObj));
  uint8_t f = o->aniframe;
  o->aniframe = (uint8_t)(f + 32);
  if ((uint16_t)f + 32 > 0xFF) o->frame ^= 1;
  OW(o, swing_origX) = a1->x.w.i;
  OW(o, swing_origY) = (int16_t)(a1->y.w.i + OW(o, GBall_AnchorPos));
  o->status = a1->status;
  if (a1->status & 0x80) {
    o->id = id_Explosion;
    o->routine = 0;
  }
}

void BossBall(Obj *o) {
  switch (o->routine) {
  case 0: {
    o->routine += 2;
    set_anglew(o, 0x4080);
    OW(o, GBall_Swing_Speed) = -0x200;
    o->map = map_get(MAP_BossItems);
    o->gfx = ArtTile_Eggman_Weapons;
    uint8_t *list = &o->s.b[1];
    obSubtype(o) = 0;
    Obj *a1 = o;
    for (int i = 0; i < 6; i++) {
      if (i) {
        Obj *n = FindNextFreeObj(o);
        if (!n) break;
        a1 = n;
        a1->x.w.i = o->x.w.i;
        a1->y.w.i = o->y.w.i;
        a1->id = id_BossBall;
        a1->routine = 6;
        a1->map = map_get(MAP_Swing_GHZ);
        a1->gfx = ArtTile_GHZ_MZ_Swing;
        a1->frame = 1;
        obSubtype(o)++;
      }
      *list++ = SLOT(a1);
      a1->render = sprite_cam_field;
      a1->actwid = 16 / 2;
      a1->priority = 6;
      OUW(a1, BGHZ_ParentObj) = OUW(o, BGHZ_ParentObj);
    }
    a1->routine = 8;
    a1->map = map_get(MAP_GBall);
    a1->gfx = ArtTile_GHZ_Giant_Ball | Tile_Pal3;
    a1->frame = 1;
    a1->priority = 5;
    a1->coltype = col_40x40 | col_hurt;
    return;
  }
  case 2: {
    static const uint8_t pos[6] = {0, 0x10, 0x20, 0x30, 0x40, 0x60};
    int n = obSubtype(o);
    Obj *a1 = o;
    uint8_t d0 = 0;
    for (int i = 0; i <= n; i++) {
      a1 = CHILD(o, i);
      d0 = pos[i];
      if (OB(a1, GBall_LinkDist) != d0) OB(a1, GBall_LinkDist)++;
    }
    if (OB(a1, GBall_LinkDist) == d0 && OPTR(OUW(o, BGHZ_ParentObj))->routine2 == 6) o->routine += 2;
    if (OW(o, GBall_AnchorPos) != 32) OW(o, GBall_AnchorPos)++;
    GBall_UpdateBase(o);
    Swing_UpdateSwingPosition(o, o->angle);
    DisplaySprite(o);
    return;
  }
  case 4:
    GBall_UpdateBase(o);
    GBall_Move(o);
    DisplaySprite(o);
    return;
  case 6:
    if (OPTR(OUW(o, BGHZ_ParentObj))->status & 0x80) {
      o->id = id_Explosion;
      o->routine = 0;
    }
    DisplaySprite(o);
    return;
  case 8:
    o->frame = o->frame ? 0 : 1;
    if (OPTR(OUW(o, BGHZ_ParentObj))->status & 0x80) {
      o->coltype = col_none;
      BossDefeated(o);
      if ((int8_t)--OB(o, BGHZ_BossGenericTimer) < 0) {
        o->id = id_Explosion;
        o->routine = 0;
      }
    }
    DisplaySprite(o);
    return;
  }
}
