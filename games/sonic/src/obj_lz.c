/* Labyrinth Zone's objects: the drowning countdown (bubbles from Sonic's mouth, the numbers, and
 * drowning), the splash and the water's surface, air bubbles and the vents that make them, the
 * pole Sonic hangs on, flapping doors, harpoons, sinking and rising blocks, gargoyles spitting
 * fire, conveyor belts carrying platforms round, waterfalls; the badniks Jaws, Burrobot and
 * Orbinaut (Star Light's and Scrap Brain's too); and Dr. Robotnik's flight up the shaft. */
#include "s1.h"
#include "collide.h"
#include "game.h"
#include "objs.h"

#define sonic_height 19
#define sonic_width 9
#define sonic_roll_height 14

/* ------------------------------------------------------------------ the drowning countdown */
#define drown_restarttime 0x2C
#define drown_origX 0x30
#define drown_displaytime 0x32
#define drown_type 0x33
#define drown_extrabubbles 0x34
#define drown_extrabubflag 0x36
#define drown_numtime 0x38
#define drown_delaytime 0x3A

static const int8_t *wobble(void) { return (const int8_t *)DATA(D_TAB_DROWN_WOBBLEDATA); }

static void Drown_ShowNumber(Obj *o) {
  if (!OW(o, drown_numtime) || --OW(o, drown_numtime) || o->anim >= 7) return;
  OW(o, drown_numtime) = 15;
  o->vy = 0;
  o->render = sprite_rendered;
  o->x.w.i = (int16_t)(o->x.w.i - V.cam[0].x.w.i + 0x80);
  obScreenY(o) = (int16_t)(o->y.w.i - V.cam[0].y.w.i + 0x80);
  o->routine = 0xC;
}

static void Drown_Display(Obj *o) {
  Drown_ShowNumber(o);
  AnimateSprite(o, ani_get(ANI_Drown));
  DisplaySprite(o);
}

static void Drown_Countdown(Obj *o) {
  Obj *s = player;
  bool spawn = false;
  if (OW(o, drown_restarttime)) {
    if (!--OW(o, drown_restarttime)) {
      s->routine = 6;
      return;
    }
    SpeedToPos(s);
    s->vy = (int16_t)(s->vy + 0x10);
  } else {
    if (s->routine >= 6 || !(s->status & status_underwater)) return;
    if (--OW(o, drown_numtime) < 0) {
      OW(o, drown_numtime) = 60 - 1;
      OUW(o, drown_extrabubflag) = 1;
      OB(o, drown_extrabubbles) = (uint8_t)(RandomNumber() & 1);
      uint16_t air = V.air;
      if (air == 25 || air == 20 || air == 15) {
        /* (the warning sound) */
      } else if (air <= 12) {
        if ((int8_t)--OB(o, drown_displaytime) < 0) {
          OB(o, drown_displaytime) = OB(o, drown_type);
          OUW(o, drown_extrabubflag) |= 0x8000;
        }
      }
      if (V.air-- == 0) {
        V.air = 0xFFFF;
        ResumeMusic();
        V.f_playerctrl = 0x81;
        OB(o, drown_extrabubbles) = 10;
        OUW(o, drown_extrabubflag) = 1;
        OW(o, drown_restarttime) = 2 * 60;
        Sonic_ResetOnFloor(s);
        s->anim = id_Drown;
        s->status |= status_in_air;
        s->gfx |= 0x8000;
        s->vy = s->vx = s->inertia = 0;
        V.f_nobgscroll = 1;
        return;
      }
      spawn = true;
    }
  }
  if (!spawn) {
    if (!OUW(o, drown_extrabubflag)) return;
    if (--OW(o, drown_delaytime) >= 0) return;
  }
  OW(o, drown_delaytime) = (int16_t)(RandomNumber() & 0xF);
  Obj *a1 = FindFreeObj();
  if (!a1) return;
  a1->id = id_DrownCount;
  a1->x.w.i = s->x.w.i;
  int16_t d0 = 6;
  if (s->status & 1) {
    d0 = -6;
    a1->angle = 0x40;
  }
  a1->x.w.i = (int16_t)(a1->x.w.i + d0);
  a1->y.w.i = s->y.w.i;
  obSubtype(a1) = 6;
  if (OW(o, drown_restarttime)) {
    OW(o, drown_delaytime) &= 7;
    a1->y.w.i = (int16_t)(s->y.w.i - 12);
    a1->angle = (uint8_t)RandomNumber();
    if (!(V.framecount & 3)) obSubtype(a1) = 0xE;
  } else if (OUW(o, drown_extrabubflag) & 0x8000) {
    uint8_t d2 = (uint8_t)(V.air >> 1);
    if (!(RandomNumber() & 3)) {
      uint16_t old = OUW(o, drown_extrabubflag);
      OUW(o, drown_extrabubflag) |= 0x4000;
      if (old & 0x4000) goto dec;
      obSubtype(a1) = d2;
      OW(a1, drown_numtime) = 28;
    }
    if (!OB(o, drown_extrabubbles)) {
      uint16_t old = OUW(o, drown_extrabubflag);
      OUW(o, drown_extrabubflag) |= 0x4000;
      if (!(old & 0x4000)) {
        obSubtype(a1) = d2;
        OW(a1, drown_numtime) = 28;
      }
    }
  }
dec:
  if ((int8_t)--OB(o, drown_extrabubbles) < 0) OUW(o, drown_extrabubflag) = 0;
}

void DrownCount(Obj *o) {
  switch (o->routine) {
  case 0:
    o->routine += 2;
    o->map = map_get(MAP_Bub);
    o->gfx = ArtTile_LZ_Bubbles | Tile_Prio;
    o->render = sprite_rendered | sprite_cam_field;
    o->actwid = 32 / 2;
    o->priority = 1;
    if (obSubtype(o) & 0x80) {
      o->routine += 8;
      o->map = map_get(MAP_UnusedFace);
      o->gfx = ArtTile_LZ_UnusedFace;
      OB(o, drown_type) = obSubtype(o) & 0x7F;
      Drown_Countdown(o);
      return;
    }
    o->anim = obSubtype(o);
    OW(o, drown_origX) = o->x.w.i;
    o->vy = -0x88;
    /* fall through */
  case 2: AnimateSprite(o, ani_get(ANI_Drown));
    /* fall through */
  case 4:
    if ((uint16_t)V.waterpos1 >= (uint16_t)o->y.w.i) {
      o->routine = 6;
      o->anim += 7;
      Drown_Display(o);
      return;
    }
    if (V.f_wtunnelmode) OW(o, drown_origX) += 4;
    {
      uint8_t a = o->angle++;
      o->x.w.i = (int16_t)(wobble()[a & 0x7F] + OW(o, drown_origX));
    }
    Drown_ShowNumber(o);
    SpeedToPos(o);
    if (!(o->render & 0x80)) {
      DeleteObject(o);
      return;
    }
    DisplaySprite(o);
    return;
  case 6:
  case 0xE: Drown_Display(o); return;
  case 0xA: Drown_Countdown(o); return;
  case 0xC:
    if (V.air > 12) {
      DeleteObject(o);
      return;
    }
    if (!--OW(o, drown_numtime)) {
      o->routine = 0xE;
      o->anim += 7;
      Drown_Display(o);
      return;
    }
    AnimateSprite(o, ani_get(ANI_Drown));
    if (!(o->render & 0x80)) {
      DeleteObject(o);
      return;
    }
    DisplaySprite(o);
    return;
  default: DeleteObject(o); return;
  }
}

/* ------------------------------------------------------------------ the splash, the surface */
void Splash(Obj *o) {
  switch (o->routine) {
  case 0:
    o->routine += 2;
    o->map = map_get(MAP_Splash);
    o->render |= sprite_cam_field;
    o->priority = 1;
    o->actwid = 32 / 2;
    o->gfx = ArtTile_LZ_Splash | Tile_Pal3;
    o->x.w.i = player->x.w.i;
    /* fall through */
  case 2:
    o->y.w.i = V.waterpos1;
    AnimateSprite(o, ani_get(ANI_Splash));
    DisplaySprite(o);
    return;
  default: DeleteObject(o); return;
  }
}

#define surf_origX 0x30
#define surf_freeze 0x32

void WaterSurface(Obj *o) {
  if (o->routine == 0) {
    o->routine += 2;
    o->map = map_get(MAP_Surf);
    o->gfx = ArtTile_LZ_Water_Surface | Tile_Pal3 | Tile_Prio;
    o->render = sprite_cam_field;
    o->actwid = 256 / 2;
    OW(o, surf_origX) = o->x.w.i;
  }
  int16_t x = (int16_t)((V.cam[0].x.w.i & 0xFFE0) + OW(o, surf_origX));
  if (V.framecount & 1) x = (int16_t)(x + 0x20);
  o->x.w.i = x;
  o->y.w.i = V.waterpos1;
  if (OB(o, surf_freeze)) {
    if (!V.f_pause) {
      OB(o, surf_freeze) = 0;
      o->frame -= 3;
      goto animate;
    }
  } else if (V.jpadpress1 & btnStart) {
    o->frame += 3;
    OB(o, surf_freeze) = 1;
  } else {
  animate:
    if ((int8_t)--o->timeframe < 0) {
      o->timeframe = 8 - 1;
      if (++o->frame >= 3) o->frame = 0;
    }
  }
  DisplaySprite(o);
}

/* ------------------------------------------------------------------ air bubbles */
#define bub_inhalable 0x2E
#define bub_origX 0x30
#define bub_time 0x32
#define bub_timebase 0x33
#define bub_minicount 0x34
#define bub_bubbleflag 0x36
#define bub_randomtime 0x38
#define bub_typelist 0x3C

static bool Bub_ChkSonic(Obj *o) {
  if (V.f_playerctrl & 0x80) return false;
  uint16_t px = (uint16_t)player->x.w.i, py = (uint16_t)player->y.w.i;
  uint16_t x = (uint16_t)o->x.w.i, y = (uint16_t)o->y.w.i;
  if ((uint16_t)(x - 16) >= px || (uint16_t)(x + 16) < px) return false;
  if (y >= py || (uint16_t)(y + 16) < py) return false;
  return true;
}

void Bubble(Obj *o) {
  static const uint8_t types[18] = {0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 1, 0, 0, 1, 0};
  switch (o->routine) {
  case 0:
    o->routine += 2;
    o->map = map_get(MAP_Bub);
    o->gfx = ArtTile_LZ_Bubbles | Tile_Prio;
    o->render = sprite_rendered | sprite_cam_field;
    o->actwid = 32 / 2;
    o->priority = 1;
    if (obSubtype(o) & 0x80) {
      o->routine += 8;
      OB(o, bub_time) = OB(o, bub_timebase) = obSubtype(o) & 0x7F;
      o->anim = 6;
      goto maker;
    }
    o->anim = obSubtype(o);
    OW(o, bub_origX) = o->x.w.i;
    o->vy = -0x88;
    o->angle = (uint8_t)RandomNumber();
    /* fall through */
  case 2:
    AnimateSprite(o, ani_get(ANI_Bub));
    if (o->frame == 6) OB(o, bub_inhalable) = 1;
    /* fall through */
  case 4: {
    if ((uint16_t)V.waterpos1 >= (uint16_t)o->y.w.i) goto burst;
    uint8_t a = o->angle++;
    o->x.w.i = (int16_t)(wobble()[a & 0x7F] + OW(o, bub_origX));
    if (OB(o, bub_inhalable) && Bub_ChkSonic(o)) {
      Obj *a1 = player;
      ResumeMusic();
      a1->vx = a1->vy = a1->inertia = 0;
      a1->anim = id_GetAir;
      OW(a1, locktime) = 35;
      OB(a1, jumping) = 0;
      a1->status &= (uint8_t)~(status_pushing | status_rolljumping);
      if (a1->status & status_rolling) {
        a1->status &= (uint8_t)~status_rolling;
        a1->height = sonic_height;
        a1->width = sonic_width;
        a1->y.w.i = (int16_t)(a1->y.w.i - (sonic_height - sonic_roll_height));
      }
      goto burst;
    }
    SpeedToPos(o);
    if (!(o->render & 0x80)) {
      DeleteObject(o);
      return;
    }
    DisplaySprite(o);
    return;
  }
  burst:
    o->routine = 6;
    o->anim += 3;
    /* fall through */
  case 6:
    AnimateSprite(o, ani_get(ANI_Bub));
    if (!(o->render & 0x80)) {
      DeleteObject(o);
      return;
    }
    DisplaySprite(o);
    return;
  case 0xA:
  maker: {
    bool spawn = false;
    if (!OUW(o, bub_bubbleflag)) {
      if ((uint16_t)V.waterpos1 >= (uint16_t)o->y.w.i || !(o->render & 0x80)) goto display;
      if (--OW(o, bub_randomtime) >= 0) goto animate;
      OUW(o, bub_bubbleflag) = 1;
      uint32_t r;
      do r = RandomNumber();
      while ((r & 7) >= 6);
      OB(o, bub_minicount) = (uint8_t)(r & 7);
      OB(o, bub_typelist) = (uint8_t)(r & 0xC);
      if ((int8_t)--OB(o, bub_time) < 0) {
        OB(o, bub_time) = OB(o, bub_timebase);
        OUW(o, bub_bubbleflag) |= 0x8000;
      }
      spawn = true;
    } else if (--OW(o, bub_randomtime) < 0) spawn = true;
    if (spawn) {
      OW(o, bub_randomtime) = (int16_t)(RandomNumber() & 0x1F);
      Obj *a1 = FindFreeObj();
      if (a1) {
        a1->id = id_Bubble;
        a1->x.w.i = (int16_t)(o->x.w.i + (int16_t)((RandomNumber() & 0xF) - 8));
        a1->y.w.i = o->y.w.i;
        obSubtype(a1) = types[OB(o, bub_typelist) + OB(o, bub_minicount)];
        if (OUW(o, bub_bubbleflag) & 0x8000) {
          bool done = false;
          if (!(RandomNumber() & 3)) {
            uint16_t old = OUW(o, bub_bubbleflag);
            OUW(o, bub_bubbleflag) |= 0x4000;
            if (old & 0x4000) done = true;
            else obSubtype(a1) = 2;
          }
          if (!done && !OB(o, bub_minicount)) {
            uint16_t old = OUW(o, bub_bubbleflag);
            OUW(o, bub_bubbleflag) |= 0x4000;
            if (!(old & 0x4000)) obSubtype(a1) = 2;
          }
        }
      }
      if ((int8_t)--OB(o, bub_minicount) < 0) {
        OW(o, bub_randomtime) = (int16_t)(OW(o, bub_randomtime) + (RandomNumber() & 0x7F) + 0x80);
        OUW(o, bub_bubbleflag) = 0;
      }
    }
  animate:
    AnimateSprite(o, ani_get(ANI_Bub));
  display:
    if (obj_far(o)) {
      DeleteObject(o);
      return;
    }
    if ((uint16_t)V.waterpos1 < (uint16_t)o->y.w.i) DisplaySprite(o);
    return;
  }
  default: DeleteObject(o); return;
  }
}

/* ------------------------------------------------------------------ the pole, flapping doors, harpoons */
#define pole_breaktime 0x30
#define pole_grabbed 0x32

void Pole(Obj *o) {
  Obj *a1 = player;
  switch (o->routine) {
  case 0:
    o->routine += 2;
    o->map = map_get(MAP_Pole);
    o->gfx = ArtTile_LZ_Pole | Tile_Pal3;
    o->render = sprite_cam_field;
    o->actwid = 16 / 2;
    o->priority = 4;
    o->coltype = col_8x64 | col_special;
    OW(o, pole_breaktime) = (int16_t)(obSubtype(o) * 60);
    /* fall through */
  case 2:
    if (OB(o, pole_grabbed)) {
      if (OW(o, pole_breaktime) && !--OW(o, pole_breaktime)) {
        o->frame = 1;
        goto release;
      }
      int16_t d0 = (int16_t)(o->y.w.i - 24);
      if (V.jpadhold1 & btnUp) {
        a1->y.w.i--;
        if ((uint16_t)d0 >= (uint16_t)a1->y.w.i) a1->y.w.i = d0;
      }
      d0 = (int16_t)(d0 + 12 + 24);
      if (V.jpadhold1 & btnDn) {
        a1->y.w.i++;
        if ((uint16_t)d0 < (uint16_t)a1->y.w.i) a1->y.w.i = d0;
      }
      if (!(V.jpadpress2 & btnABC)) break;
    release:
      o->coltype = 0;
      o->routine += 2;
      V.f_playerctrl = 0;
      V.f_wtunneldisallow = 0;
      OB(o, pole_grabbed) = 0;
      break;
    }
    if (!o->colprop) break;
    if ((uint16_t)(o->x.w.i + 20) >= (uint16_t)a1->x.w.i) break;
    o->colprop = 0;
    if (a1->routine >= 4) break;
    a1->vx = a1->vy = 0;
    a1->x.w.i = (int16_t)(o->x.w.i + 20);
    a1->status &= (uint8_t)~1;
    a1->anim = id_Hang;
    V.f_playerctrl = 1;
    V.f_wtunneldisallow = 1;
    OB(o, pole_grabbed) = 1;
    break;
  }
  RememberState(o);
}

#define flap_wait 0x30
#define flap_time 0x32

void FlapDoor(Obj *o) {
  if (o->routine == 0) {
    o->routine += 2;
    o->map = map_get(MAP_Flap);
    o->gfx = ArtTile_LZ_Flapping_Door | Tile_Pal3;
    o->render |= sprite_cam_field;
    o->actwid = 80 / 2;
    OW(o, flap_time) = (int16_t)(obSubtype(o) * 60);
  }
  if (--OW(o, flap_wait) < 0) {
    OW(o, flap_wait) = OW(o, flap_time);
    o->anim ^= 1;
  }
  AnimateSprite(o, ani_get(ANI_Flap));
  V.f_wtunneldisallow = 0;
  if (!o->frame && (uint16_t)player->x.w.i < (uint16_t)o->x.w.i) {
    V.f_wtunneldisallow = 1;
    SolidObject(o, 16 / 2 + sonic_solid_width, 64 / 2, 64 / 2 + 1, o->x.w.i);
  }
  RememberState(o);
}

#define harp_time 0x30

void Harpoon(Obj *o) {
  static const uint8_t cols[6] = {col_16x8 | col_hurt, col_48x8 | col_hurt, col_80x8 | col_hurt,
                                  col_8x16 | col_hurt, col_8x48 | col_hurt, col_8x80 | col_hurt};
  switch (o->routine) {
  case 0:
    o->routine += 2;
    o->map = map_get(MAP_Harp);
    o->gfx = ArtTile_LZ_Harpoon;
    o->render |= sprite_cam_field;
    o->priority = 4;
    o->anim = obSubtype(o);
    o->actwid = 40 / 2;
    OW(o, harp_time) = 60;
    /* fall through */
  case 2:
    AnimateSprite(o, ani_get(ANI_Harp));
    o->coltype = cols[o->frame < 6 ? o->frame : 0];
    break;
  case 4:
    if (--OW(o, harp_time) < 0) {
      OW(o, harp_time) = 60;
      o->routine -= 2;
      o->anim ^= 1;
    }
    break;
  }
  RememberState(o);
}

/* ------------------------------------------------------------------ Jaws, Burrobot */
#define jaws_turndelay_current 0x30
#define jaws_turndelay_base 0x32

void Jaws(Obj *o) {
  if (o->routine == 0) {
    o->routine += 2;
    o->map = map_get(MAP_Jaws);
    o->gfx = ArtTile_Jaws | Tile_Pal2;
    o->render |= sprite_cam_field;
    o->coltype = col_32x24 | col_badnik;
    o->priority = 4;
    o->actwid = 32 / 2;
    int16_t d0 = (int16_t)((obSubtype(o) << 6) - 1);
    OW(o, jaws_turndelay_current) = OW(o, jaws_turndelay_base) = d0;
    o->vx = -0x40;
    if (o->status & 1) o->vx = 0x40;
  }
  if (--OW(o, jaws_turndelay_current) < 0) {
    OW(o, jaws_turndelay_current) = OW(o, jaws_turndelay_base);
    o->vx = (int16_t)-o->vx;
    o->status ^= 1;
    o->prevani = 1;
  }
  AnimateSprite(o, ani_get(ANI_Jaws));
  SpeedToPos(o);
  RememberState(o);
}

#define burro_timedelay 0x30
#define burro_checktype 0x32

static uint16_t Burro_CheckDistance(Obj *o, int16_t *d1) {
  *d1 = 0x80;
  o->status |= 1;
  int16_t d0 = (int16_t)(player->x.w.i - o->x.w.i);
  if ((uint16_t)player->x.w.i < (uint16_t)o->x.w.i) {
    d0 = (int16_t)-d0;
    *d1 = -0x80;
    o->status &= (uint8_t)~1;
  }
  return (uint16_t)d0;
}

static void Burro_NextAction(Obj *o) {
  if (vblank_byte & 4) {
    o->routine2 -= 2;
    OW(o, burro_timedelay) = 60 - 1;
    o->vx = 0;
    o->anim = 0;
  } else {
    o->routine2 += 2;
    o->vy = -0x400;
    o->anim = 2;
  }
}

void Burrobot(Obj *o) {
  if (o->routine == 0) {
    o->routine += 2;
    o->height = 38 / 2;
    o->width = 16 / 2;
    o->map = map_get(MAP_Burro);
    o->gfx = ArtTile_Burrobot;
    o->render |= sprite_cam_field;
    o->priority = 4;
    o->coltype = col_24x36 | col_badnik;
    o->actwid = 24 / 2;
    o->routine2 += 6;
    o->anim = 2;
  }
  int16_t d1;
  switch (o->routine2) {
  case 0:
    if (--OW(o, burro_timedelay) >= 0) break;
    o->routine2 += 2;
    OW(o, burro_timedelay) = 255;
    o->vx = 0x80;
    o->anim = 1;
    o->status ^= 1;
    if (!(o->status & 1)) o->vx = -0x80;
    break;
  case 2: {
    if (--OW(o, burro_timedelay) < 0) {
      Burro_NextAction(o);
      break;
    }
    SpeedToPos(o);
    uint8_t old = OB(o, burro_checktype);
    OB(o, burro_checktype) ^= 1;
    if (old & 1) {
      o->y.w.i = (int16_t)(o->y.w.i + ObjFloorDist(o, NULL));
      break;
    }
    int16_t d3 = (int16_t)(o->x.w.i + 12);
    if (!(o->status & 1)) d3 = (int16_t)(d3 - 24);
    if (ObjFloorDist2(o, d3, NULL) >= 0xC) Burro_NextAction(o);
    break;
  }
  case 4: {
    SpeedToPos(o);
    o->vy = (int16_t)(o->vy + 0x18);
    if (o->vy < 0) break;
    o->anim = 3;
    int16_t f = ObjFloorDist(o, NULL);
    if (f >= 0) break;
    o->y.w.i = (int16_t)(o->y.w.i + f);
    o->vy = 0;
    o->anim = 1;
    OW(o, burro_timedelay) = 255;
    o->routine2 -= 2;
    Burro_CheckDistance(o, &d1);
    break;
  }
  case 6: {
    if (Burro_CheckDistance(o, &d1) >= 96) break;
    uint16_t py = (uint16_t)player->y.w.i, y = (uint16_t)o->y.w.i;
    if (py >= y) break;
    if ((uint16_t)(py - y) < (uint16_t)-128) break;
    if (V.debuguse) break;
    o->routine2 -= 2;
    o->vx = d1;
    o->vy = -0x400;
    break;
  }
  }
  AnimateSprite(o, ani_get(ANI_Burro));
  RememberState(o);
}

/* ------------------------------------------------------------------ Orbinaut */
#define orb_circledir 0x36
#define orb_balldata 0x37
#define orb_parent 0x3C

void Orbinaut(Obj *o) {
  switch (o->routine) {
  case 0: {
    o->map = map_get(MAP_Orb);
    o->gfx = ArtTile_SBZ_Orbinaut;
    if (V.zone != id_SBZ) o->gfx = ArtTile_SLZ_Orbinaut | Tile_Pal2;
    if (V.zone == id_LZ) o->gfx = ArtTile_LZ_Orbinaut;
    o->render |= sprite_cam_field;
    o->priority = 4;
    o->coltype = col_16x16 | col_badnik;
    o->actwid = 24 / 2;
    uint8_t d2 = 0;
    for (int i = 0; i < 4; i++) {
      Obj *a1 = FindNextFreeObj(o);
      if (!a1) break;
      OB(o, orb_balldata + 1 + OB(o, orb_balldata)) = SLOT(a1);
      OB(o, orb_balldata)++;
      a1->id = o->id;
      a1->routine = 6;
      a1->map = o->map;
      a1->gfx = o->gfx;
      a1->render |= sprite_cam_field;
      a1->priority = 4;
      a1->actwid = 16 / 2;
      a1->frame = 3;
      a1->coltype = col_8x8 | col_hurt;
      a1->angle = d2;
      d2 = (uint8_t)(d2 + 0x40);
      OUW(a1, orb_parent) = OADDR(o);
    }
    OB(o, orb_circledir) = (o->status & 1) ? 0xFF : 1;
    o->routine = (uint8_t)(obSubtype(o) + 2);
    o->vx = -0x40;
    if (o->status & 1) o->vx = 0x40;
    return;
  }
  case 2: {
    int16_t dx = (int16_t)(player->x.w.i - o->x.w.i), dy = (int16_t)(player->y.w.i - o->y.w.i);
    if ((uint16_t)player->x.w.i < (uint16_t)o->x.w.i) dx = (int16_t)-dx;
    if ((uint16_t)player->y.w.i < (uint16_t)o->y.w.i) dy = (int16_t)-dy;
    if ((uint16_t)dx < 160 && (uint16_t)dy < 80 && !V.debuguse) o->anim = 1;
    AnimateSprite(o, ani_get(ANI_Orb));
    goto display;
  }
  case 4:
    SpeedToPos(o);
  display:
    if (obj_far(o)) {
      if (o->respawn) V.objstate[2 + o->respawn] &= 0x7F;
      int n = OB(o, orb_balldata);
      for (int i = 0; i < n; i++) DeleteObject(OBJ(OB(o, orb_balldata + 1 + i) & 0x7F));
      DeleteObject(o);
      return;
    }
    DisplaySprite(o);
    return;
  case 6: {
    Obj *a1 = OPTR(OUW(o, orb_parent));
    if (a1->id != id_Orbinaut) {
      DeleteObject(o);
      return;
    }
    if (a1->frame == 2 && o->angle == 0x40) {
      o->routine += 2;
      if (!--OB(a1, orb_balldata)) a1->routine += 2;
      o->vx = -0x200;
      if (a1->status & 1) o->vx = 0x200;
      DisplaySprite(o);
      return;
    }
    int16_t cos, sin = CalcSine(o->angle, &cos);
    o->x.w.i = (int16_t)((cos >> 4) + a1->x.w.i);
    o->y.w.i = (int16_t)((sin >> 4) + a1->y.w.i);
    o->angle = (uint8_t)(o->angle + OB(a1, orb_circledir));
    DisplaySprite(o);
    return;
  }
  case 8:
    SpeedToPos(o);
    if (!(o->render & 0x80)) {
      DeleteObject(o);
      return;
    }
    DisplaySprite(o);
    return;
  }
}

/* ------------------------------------------------------------------ blocks */
#define lblk_origY 0x30
#define lblk_origX 0x34
#define lblk_time 0x36
#define lblk_untouched 0x38
#define lblk_nudge 0x3E
#define lblk_touchtype 0x3F

void LabyrinthBlock(Obj *o) {
  static const uint8_t var[4][2] = {{32 / 2, 32 / 2}, {64 / 2, 24 / 2}, {32 / 2, 32 / 2}, {32 / 2, 32 / 2}};
  if (o->routine == 0) {
    o->routine += 2;
    o->map = map_get(MAP_LBlock);
    o->gfx = ArtTile_LZ_Blocks | Tile_Pal3;
    o->render = sprite_cam_field;
    o->priority = 3;
    int k = ((obSubtype(o) >> 3) & 0xE) >> 1;
    o->actwid = var[k][0];
    o->height = var[k][1];
    o->frame = (uint8_t)k;
    OW(o, lblk_origX) = o->x.w.i;
    OW(o, lblk_origY) = o->y.w.i;
    uint8_t t = obSubtype(o) & 0xF;
    if (t && t != 7) OB(o, lblk_untouched) = 1;
  }
  int16_t x = o->x.w.i;
  switch (obSubtype(o) & 0xF) {
  case 1:
  case 3:
    if (OW(o, lblk_time)) {
      if (!--OW(o, lblk_time)) {
        obSubtype(o)++;
        OB(o, lblk_untouched) = 0;
      }
    } else if (o->status & status_on_object) OW(o, lblk_time) = 30;
    break;
  case 2:
  case 6: {
    SpeedToPos(o);
    o->vy = (int16_t)(o->vy + 8);
    int16_t f = ObjFloorDist(o, NULL);
    if (f >= 0) break;
    o->y.w.i = (int16_t)(o->y.w.i + f + 1);
    o->vy = 0;
    obSubtype(o) = 0;
    break;
  }
  case 4: {
    SpeedToPos(o);
    o->vy = (int16_t)(o->vy - 8);
    int16_t c = ObjHitCeiling(o, NULL);
    if (c >= 0) break;
    o->y.w.i = (int16_t)(o->y.w.i - c);
    o->vy = 0;
    obSubtype(o) = 0;
    break;
  }
  case 5:
    if (OB(o, lblk_touchtype) == 1) {
      obSubtype(o)++;
      OB(o, lblk_untouched) = 0;
    }
    break;
  case 7: {
    int16_t d0 = (int16_t)(V.waterpos1 - o->y.w.i);
    if (!d0) break;
    if ((uint16_t)V.waterpos1 < (uint16_t)o->y.w.i) {
      if (d0 < -2) d0 = -2;
      o->y.w.i = (int16_t)(o->y.w.i + d0);
      int16_t c = ObjHitCeiling(o, NULL);
      if (c < 0) o->y.w.i = (int16_t)(o->y.w.i - c);
    } else {
      if (d0 > 2) d0 = 2;
      o->y.w.i = (int16_t)(o->y.w.i + d0);
      int16_t f = ObjFloorDist(o, NULL);
      if (f < 0) o->y.w.i = (int16_t)(o->y.w.i + f + 1);
    }
    break;
  }
  }
  if (o->render & 0x80) {
    int r = SolidObject(o, (int16_t)(o->actwid + sonic_solid_width), o->height, (int16_t)(o->height + 1), x);
    OB(o, lblk_touchtype) = (uint8_t)r;
    if (OB(o, lblk_untouched)) {
      if (o->status & status_on_object) {
        if (OB(o, lblk_nudge) == 0x40) goto chkdel;
        OB(o, lblk_nudge) += 4;
      } else if (OB(o, lblk_nudge)) OB(o, lblk_nudge) -= 4;
      else goto chkdel;
      int16_t s = CalcSine(OB(o, lblk_nudge), NULL);
      o->y.w.i = (int16_t)(((int32_t)s * 0x400 >> 16) + OW(o, lblk_origY));
    }
  }
chkdel:
  if (obj_far_x(OW(o, lblk_origX))) {
    DeleteObject(o);
    return;
  }
  DisplaySprite(o);
}

/* ------------------------------------------------------------------ gargoyles */
void Gargoyle(Obj *o) {
  static const uint8_t rates[8] = {30, 60, 90, 120, 150, 180, 210, 240};
  switch (o->routine) {
  case 0:
    o->routine += 2;
    o->map = map_get(MAP_Gar);
    o->gfx = ArtTile_LZ_Gargoyle | Tile_Pal3;
    o->render |= sprite_cam_field;
    o->priority = 3;
    o->actwid = 32 / 2;
    o->delayani = rates[obSubtype(o) & 7];
    o->timeframe = o->delayani;
    obSubtype(o) &= 0xF;
    /* fall through */
  case 2:
    if (!--o->timeframe) {
      o->timeframe = o->delayani;
      if (ChkObjectVisible(o)) {
        Obj *a1 = FindFreeObj();
        if (a1) {
          a1->id = id_Gargoyle;
          a1->routine += 4;
          a1->x.w.i = o->x.w.i;
          a1->y.w.i = o->y.w.i;
          a1->render = o->render;
          a1->status = o->status;
        }
      }
    }
    break;
  case 4:
    o->routine += 2;
    o->height = o->width = 16 / 2;
    o->map = map_get(MAP_Gar);
    o->gfx = ArtTile_LZ_Gargoyle;
    o->render |= sprite_cam_field;
    o->priority = 4;
    o->coltype = col_8x8 | col_hurt;
    o->actwid = 16 / 2;
    o->frame = 2;
    o->y.w.i += 8;
    o->vx = (o->status & 1) ? 0x200 : -0x200;
    /* fall through */
  case 6:
    if (!(V.framecount & 7)) o->frame ^= 1;
    SpeedToPos(o);
    if ((o->status & 1) ? ObjHitWallRight(o, 8, NULL) < 0 : ObjHitWallLeft(o, -8, NULL) < 0) {
      DeleteObject(o);
      return;
    }
    break;
  }
  RememberState(o);
}

/* ------------------------------------------------------------------ conveyor belts */
#define lcon_groupid 0x2F
#define lcon_baseX 0x30
#define lcon_nextX 0x34
#define lcon_nextY 0x36
#define lcon_posindex 0x38
#define lcon_count 0x39
#define lcon_increment 0x3A
#define lcon_reversed 0x3B
#define lcon_targetdata 0x3C

/* the corners of each belt: count (in bytes), base x, then corner x and y words */
static const uint16_t lcon_data[6][2 + 12] = {
  {6 * 4, 0x1070, 0x1078, 0x21A, 0x10BE, 0x260, 0x10BE, 0x393, 0x108C, 0x3C5, 0x1022, 0x390, 0x1022, 0x244},
  {5 * 4, 0x1280, 0x127E, 0x280, 0x12CE, 0x2D0, 0x12CE, 0x46E, 0x1232, 0x420, 0x1232, 0x2CC},
  {4 * 4, 0xD68, 0xD22, 0x482, 0xD22, 0x5DE, 0xDAE, 0x5DE, 0xDAE, 0x482},
  {4 * 4, 0xDA0, 0xD62, 0x3A2, 0xDEE, 0x3A2, 0xDEE, 0x4DE, 0xD62, 0x4DE},
  {5 * 4, 0xD00, 0xCAC, 0x242, 0xDDE, 0x242, 0xDDE, 0x3DE, 0xC52, 0x3DE, 0xC52, 0x29C},
  {4 * 4, 0x1300, 0x1252, 0x20A, 0x13DE, 0x20A, 0x13DE, 0x2BE, 0x1252, 0x2BE},
};

static const uint16_t *lcon_targets(Obj *o) { return &lcon_data[OB(o, lcon_targetdata)][2]; }

void LCon_ChangeDir(Obj *o) {
  int16_t d2 = -0x100, d3 = -0x100;
  uint16_t d0 = (uint16_t)(o->x.w.i - OW(o, lcon_nextX)), d1 = (uint16_t)(o->y.w.i - OW(o, lcon_nextY));
  if ((uint16_t)o->x.w.i < (uint16_t)OW(o, lcon_nextX)) d0 = (uint16_t)-d0, d2 = 0x100;
  if ((uint16_t)o->y.w.i < (uint16_t)OW(o, lcon_nextY)) d1 = (uint16_t)-d1, d3 = 0x100;
  if (d1 >= d0) {
    int16_t dx = (int16_t)(o->x.w.i - OW(o, lcon_nextX));
    int32_t q = 0;
    if (dx) {
      int32_t n = (int32_t)dx << 8;
      int16_t quot = (int16_t)(n / (int16_t)d1), rem = (int16_t)(n % (int16_t)d1);
      q = (int32_t)(uint16_t)rem << 16 | (uint16_t)(int16_t)-quot;
    }
    o->vx = (int16_t)q;
    o->vy = d3;
    o->x.w.f = (uint16_t)(q >> 16);
    o->y.w.f = 0;
  } else {
    int16_t dy = (int16_t)(o->y.w.i - OW(o, lcon_nextY));
    int32_t q = 0;
    if (dy) {
      int32_t n = (int32_t)dy << 8;
      int16_t quot = (int16_t)(n / (int16_t)d0), rem = (int16_t)(n % (int16_t)d0);
      q = (int32_t)(uint16_t)rem << 16 | (uint16_t)(int16_t)-quot;
    }
    o->vy = (int16_t)q;
    o->vx = d2;
    o->y.w.f = (uint16_t)(q >> 16);
    o->x.w.f = 0;
  }
}

static void lcon_next_corner(Obj *o) {
  uint8_t d1 = (uint8_t)(OB(o, lcon_posindex) + OB(o, lcon_increment));
  if (d1 >= OB(o, lcon_count)) {
    uint8_t d0 = d1;
    d1 = 0;
    if (d0 & 0x80) d1 = (uint8_t)(OB(o, lcon_count) - 4);
  }
  OB(o, lcon_posindex) = d1;
  const uint16_t *t = lcon_targets(o);
  OW(o, lcon_nextX) = (int16_t)t[d1 / 2];
  OW(o, lcon_nextY) = (int16_t)t[d1 / 2 + 1];
  LCon_ChangeDir(o);
}

static void LCon_Platform_Update(Obj *o) {
  if (V.f_switch[0xE] && !OB(o, lcon_reversed)) {
    OB(o, lcon_reversed) = 1;
    V.f_conveyrev = 1;
    OB(o, lcon_increment) = (uint8_t)-OB(o, lcon_increment);
    lcon_next_corner(o);
  } else if (o->x.w.i == OW(o, lcon_nextX) && o->y.w.i == OW(o, lcon_nextY))
    lcon_next_corner(o);
  SpeedToPos(o);
}

void LabyrinthConvey(Obj *o) {
  switch (o->routine) {
  case 0:
    if (obSubtype(o) & 0x80) {
      uint8_t d0 = obSubtype(o);
      OB(o, lcon_groupid) = d0;
      d0 &= 0x7F;
      uint8_t old = V.obj63[d0];
      V.obj63[d0] |= 1;
      if (old & 1) {
        DeleteObject(o);
        return;
      }
      int idx = (d0 * 2 & 0x1E) / 2;
      const uint16_t *pf = DATAW(((const uint32_t *)DATA(D_PLATFORMS))[idx & 15]);
      int n = pf[0];
      pf++;
      Obj *a1 = o;
      for (int i = 0; i <= n; i++, pf += 3) {
        if (i) {
          a1 = FindFreeObj();
          if (!a1) continue;
        }
        a1->id = id_LabyrinthConvey;
        a1->x.w.i = (int16_t)pf[0];
        a1->y.w.i = (int16_t)pf[1];
        obSubtype(a1) = (uint8_t)pf[2];
      }
      return;
    }
    o->routine += 2;
    o->map = map_get(MAP_LConv);
    o->gfx = ArtTile_LZ_Conveyor_Belt | Tile_Pal3;
    o->render |= sprite_cam_field;
    o->actwid = 32 / 2;
    o->priority = 4;
    if (obSubtype(o) == 0x7F) {
      o->routine += 4;
      o->gfx = ArtTile_LZ_Conveyor_Belt;
      o->priority = 1;
      goto wheel;
    }
    {
      o->frame = 4;
      int g = ((obSubtype(o) >> 3) & 0x1E) >> 1;
      if (g > 5) g = 5;
      OB(o, lcon_targetdata) = (uint8_t)g;
      OB(o, lcon_posindex) = (uint8_t)(lcon_data[g][0] >> 8);
      OB(o, lcon_count) = (uint8_t)lcon_data[g][0];
      OW(o, lcon_baseX) = (int16_t)lcon_data[g][1];
      uint8_t d1 = (uint8_t)((obSubtype(o) & 0xF) << 2);
      OB(o, lcon_posindex) = d1;
      OB(o, lcon_increment) = 4;
      if (V.f_conveyrev) {
        OB(o, lcon_reversed) = 1;
        OB(o, lcon_increment) = (uint8_t)-OB(o, lcon_increment);
        uint8_t n = (uint8_t)(OB(o, lcon_posindex) + OB(o, lcon_increment));
        if (n >= OB(o, lcon_count)) {
          uint8_t d0 = n;
          n = 0;
          if (d0 & 0x80) n = (uint8_t)(OB(o, lcon_count) - 4);
        }
        d1 = n;
        OB(o, lcon_posindex) = d1;
      }
      const uint16_t *t = lcon_targets(o);
      OW(o, lcon_nextX) = (int16_t)t[d1 / 2];
      OW(o, lcon_nextY) = (int16_t)t[d1 / 2 + 1];
      LCon_ChangeDir(o);
    }
    /* fall through */
  case 2:
    PlatformObject(o, o->actwid);
    LCon_Platform_Update(o);
    break;
  case 4: {
    ExitPlatform(o, o->actwid);
    int16_t x = o->x.w.i;
    LCon_Platform_Update(o);
    MvSonicOnPtfm2(o, x);
    break;
  }
  case 6:
  wheel:
    if (!(V.framecount & 3)) o->frame = (uint8_t)((o->frame + (V.f_conveyrev ? -1 : 1)) & 3);
    RememberState(o);
    return;
  }
  uint16_t d = (uint16_t)((OW(o, lcon_baseX) & 0xFF80) - ((V.cam[0].x.w.i - 128) & 0xFF80));
  if (d > 128 + 320 + 192 && !(V.act == 2 && d >= 0xFF80)) {
    uint8_t g = OB(o, lcon_groupid);
    if (g & 0x80) V.obj63[g & 0x7F] &= (uint8_t)~1;
    DeleteObject(o);
    return;
  }
  DisplaySprite(o);
}

/* ------------------------------------------------------------------ waterfalls */
void Waterfall(Obj *o) {
  switch (o->routine) {
  case 0:
    o->routine += 4;
    o->map = map_get(MAP_WFall);
    o->gfx = ArtTile_LZ_Splash | Tile_Pal3;
    o->render |= sprite_cam_field;
    o->actwid = 48 / 2;
    o->priority = 1;
    if (obSubtype(o) & 0x80) o->gfx |= 0x8000;
    o->frame = obSubtype(o) & 0xF;
    if (o->frame != 9) break;
    o->priority = 0;
    o->routine -= 2;
    if (obSubtype(o) & 0x40) o->routine = 6;
    if (obSubtype(o) & 0x20) o->routine = 8;
    /* fall through */
  case 2: AnimateSprite(o, ani_get(ANI_WFall)); break;
  case 6:
    o->y.w.i = (int16_t)(V.waterpos1 - 16);
    AnimateSprite(o, ani_get(ANI_WFall));
    break;
  case 8:
    o->gfx &= 0x7FFF;
    if (lvllayout[2][0][6] == 7) o->gfx |= 0x8000;
    AnimateSprite(o, ani_get(ANI_WFall));
    break;
  }
  RememberState(o);
}

/* ------------------------------------------------------------------ the boss */
#define BossLabyrinth_ParentObj 0x34
#define BossLabyrinth_GenericTimer 0x3C
#define BossLabyrinth_EarlyDefeatFlag 0x3D
#define BossLabyrinth_SineCounter 0x3F

static void BLZ_ShipUpdate(Obj *o) {
  if (OB(o, BossLabyrinth_EarlyDefeatFlag)) {
    BossDefeated(o);
    return;
  }
  if (o->status & 0x80) {
    AddPoints(100);
    OB(o, BossLabyrinth_EarlyDefeatFlag) = 0xFF;
    return;
  }
  if (o->coltype) return;
  boss_hit_flash(o, col_48x48 | col_boss);
}

static void BLZ_MoveBoss(Obj *o) {
  BossMove(o);
  o->y.w.i = obBossY(o).w.i;
  o->x.w.i = obBossX(o).w.i;
  BLZ_ShipUpdate(o);
}

/* moving to (tx, ty): true once both are reached */
static bool blz_reach(Obj *o, int16_t tx, int16_t ty) {
  bool rx = false, ry = false;
  if ((uint16_t)obBossX(o).w.i >= (uint16_t)tx) {
    obBossX(o).w.i = tx;
    o->vx = 0;
    rx = true;
  }
  if (obBossY(o).w.i <= ty) {
    obBossY(o).w.i = ty;
    o->vy = 0;
    ry = true;
  }
  return rx && ry;
}

void BossLabyrinth(Obj *o) {
  switch (o->routine) {
  case 0: {
    static const uint8_t data[3][2] = {{2, 0}, {4, 1}, {6, 7}};
    o->x.w.i = boss_lz_x + 0x30;
    o->y.w.i = boss_lz_y + 0x500;
    obBossX(o).w.i = o->x.w.i;
    obBossY(o).w.i = o->y.w.i;
    o->coltype = col_48x48 | col_boss;
    obBossHits(o) = 8;
    o->priority = 4;
    Obj *a1 = o;
    for (int i = 0; i < 3; i++) {
      if (i) {
        a1 = FindNextFreeObj(o);
        if (!a1) break;
        a1->id = id_BossLabyrinth;
        a1->x.w.i = o->x.w.i;
        a1->y.w.i = o->y.w.i;
      }
      o->status &= (uint8_t)~1;
      a1->routine2 = 0;
      a1->routine = data[i][0];
      a1->anim = data[i][1];
      a1->priority = o->priority;
      a1->map = map_get(MAP_Eggman);
      a1->gfx = ArtTile_Eggman;
      a1->render = sprite_cam_field;
      a1->actwid = 64 / 2;
      OUW(a1, BossLabyrinth_ParentObj) = OADDR(o);
    }
  }
    /* fall through */
  case 2: {
    Obj *s = player;
    switch (o->routine2) {
    case 0:
      if ((uint16_t)s->x.w.i >= boss_lz_x - 0x40) {
        o->vy = -0x180;
        o->vx = 0x60;
        o->routine2 += 2;
      }
      BLZ_MoveBoss(o);
      break;
    case 2:
      if (blz_reach(o, boss_lz_x + 0x68, boss_lz_y + 0x440)) {
        o->vx = 0x140;
        o->vy = -0x200;
        o->routine2 += 2;
      }
      BLZ_MoveBoss(o);
      break;
    case 4:
      if (blz_reach(o, boss_lz_x + 0x90, boss_lz_y + 0x400)) {
        o->vy = -0x180;
        o->routine2 += 2;
        OB(o, BossLabyrinth_SineCounter) = 0;
      }
      BLZ_MoveBoss(o);
      break;
    case 6: {
      if (obBossY(o).w.i <= boss_lz_y + 0x40) {
        obBossY(o).w.i = boss_lz_y + 0x40;
        o->vx = 0x140;
        o->vy = -0x80;
        if (OB(o, BossLabyrinth_EarlyDefeatFlag)) o->vx = (int16_t)(o->vx * 2), o->vy = (int16_t)(o->vy * 2);
        o->routine2 += 2;
        BLZ_MoveBoss(o);
        break;
      }
      o->status |= 1;
      OB(o, BossLabyrinth_SineCounter) += 2;
      int16_t cos, sin = CalcSine(OB(o, BossLabyrinth_SineCounter), &cos);
      if (cos < 0) o->status &= (uint8_t)~1;
      o->x.w.i = (int16_t)(obBossX(o).w.i + (sin >> 4));
      int16_t d0 = o->vy;
      uint16_t py = (uint16_t)s->y.w.i, y = (uint16_t)o->y.w.i;
      if (py >= y) {
        uint16_t t = (uint16_t)(py - y);
        if (t >= 72) {
          d0 = (int16_t)(d0 >> 1);
          t -= 72;
          if (t >= 40) {
            d0 = (int16_t)(d0 >> 1);
            t -= 40;
            if (t >= 40) d0 = 0;
          }
        }
      }
      int32_t dl = (int32_t)d0 << 8;
      if (OB(o, BossLabyrinth_EarlyDefeatFlag)) dl *= 2;
      obBossY(o).l += dl;
      o->y.w.i = obBossY(o).w.i;
      BLZ_ShipUpdate(o);
      break;
    }
    case 8:
      if (blz_reach(o, boss_lz_x + 0x16C, boss_lz_y)) {
        o->routine2 += 2;
        o->status &= (uint8_t)~1;
      }
      BLZ_MoveBoss(o);
      break;
    case 0xA:
      if (OB(o, BossLabyrinth_EarlyDefeatFlag) ||
          (s->x.w.i >= boss_lz_x + 0xE8 && s->y.w.i <= boss_lz_y + 0x30)) {
        if (!OB(o, BossLabyrinth_EarlyDefeatFlag)) OB(o, BossLabyrinth_GenericTimer) = 50;
        V.f_lockscreen = 0;
        o->status |= 1;
        o->routine2 += 2;
      }
      BLZ_MoveBoss(o);
      break;
    case 0xC:
      if (OB(o, BossLabyrinth_EarlyDefeatFlag) || !--OB(o, BossLabyrinth_GenericTimer)) {
        OB(o, BossLabyrinth_GenericTimer) = 0;
        o->vx = 0x400;
        o->vy = -0x40;
        OB(o, BossLabyrinth_EarlyDefeatFlag) = 0;
        o->routine2 += 2;
      }
      BLZ_MoveBoss(o);
      break;
    case 0xE:
      if ((uint16_t)V.limitright2 >= boss_lz_end) {
        if (!(o->render & 0x80)) {
          DeleteObject(o);
          return;
        }
      } else V.limitright2 += 2;
      BLZ_MoveBoss(o);
      break;
    }
    AnimateSprite(o, ani_get(ANI_Eggman));
    o->render = (uint8_t)((o->render & ~3) | (o->status & 3));
    DisplaySprite(o);
    return;
  }
  case 4:
  case 6: {
    Obj *a1 = OPTR(OUW(o, BossLabyrinth_ParentObj));
    if (a1->id != o->id) {
      DeleteObject(o);
      return;
    }
    if (o->routine == 4) {
      uint8_t d1 = 1;
      if (OB(o, BossLabyrinth_EarlyDefeatFlag)) d1 = 0xA;
      else if (!a1->coltype) d1 = 5;
      else if (player->routine >= 4) d1 = 4;
      o->anim = d1;
      if (a1->routine2 == 0xE) {
        o->anim = 6;
        if (!(o->render & 0x80)) {
          DeleteObject(o);
          return;
        }
      }
    } else {
      o->anim = 7;
      if (a1->routine2 == 0xE) {
        o->anim = 0xB;
        if (!(o->render & 0x80)) {
          DeleteObject(o);
          return;
        }
      }
    }
    AnimateSprite(o, ani_get(ANI_Eggman));
    o->x.w.i = a1->x.w.i;
    o->y.w.i = a1->y.w.i;
    o->status = a1->status;
    o->render = (uint8_t)((o->render & ~3) | (o->status & 3));
    DisplaySprite(o);
    return;
  }
  }
}
