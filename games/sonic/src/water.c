/* Labyrinth Zone's water (and Scrap Brain's third act, which is in Labyrinth): its height, which
 * rises and falls as Sonic reaches places in each act, and the line on screen from which the
 * underwater colours are shown; the wind tunnels that carry Sonic, and the slides. */
#include "s1.h"
#include "game.h"
#include "objs.h"

static void DynWater_LZ1(void) {
  uint16_t x = (uint16_t)V.cam[0].x.w.i;
  uint16_t d1;
  switch (V.wtr_routine) {
  case 0:
    d1 = 0xB8;
    if (x >= 0x600) {
      d1 = 0x108;
      if ((uint16_t)player->y.w.i < 0x200) {
        if (x >= 0xC80) {
          d1 = 0xE8;
          if (x >= 0x1500) d1 = 0x108;
        }
      } else if (x >= 0xC00) {
        d1 = 0x318;
        if (x >= 0x1080) {
          V.f_switch[5] = 0x80;
          d1 = 0x5C8;
          if (x >= 0x1380) {
            d1 = 0x3A8;
            if (d1 == (uint16_t)V.waterpos2) V.wtr_routine = 1;
          }
        }
      }
    }
    V.waterpos3 = (int16_t)d1;
    return;
  case 1:
    if ((uint16_t)player->y.w.i >= 0x2E0) return;
    d1 = 0x3A8;
    if (x >= 0x1300) {
      d1 = 0x108;
      V.wtr_routine = 2;
    }
    V.waterpos3 = (int16_t)d1;
    return;
  }
}

static void DynWater_LZ2(void) {
  uint16_t x = (uint16_t)V.cam[0].x.w.i;
  V.waterpos3 = x < 0x500 ? 0x328 : x < 0xB00 ? 0x3C8 : 0x428;
}

static void DynWater_LZ3(void) {
  uint16_t x = (uint16_t)V.cam[0].x.w.i, py = (uint16_t)player->y.w.i;
  int16_t d1;
  switch (V.wtr_routine) {
  case 0:
    d1 = 0x900;
    if (x >= 0x600 && py >= 0x3C0 && py < 0x600) {
      d1 = 0x4C8;
      lvllayout[2][0][6] = 0x4B;
      V.wtr_routine = 1;
    }
    V.waterpos3 = V.waterpos2 = d1;
    return;
  case 1:
    d1 = 0x4C8;
    if (x >= 0x770) {
      d1 = 0x308;
      if (x >= 0x1400) {
        if (V.waterpos3 == 0x508 || py >= 0x600 || py < 0x280) {
          d1 = 0x508;
          V.waterpos2 = d1;
          if (x >= 0x1770) V.wtr_routine = 2;
        }
      }
    }
    V.waterpos3 = d1;
    return;
  case 2:
    d1 = 0x508;
    if (x >= 0x1860) {
      d1 = 0x188;
      if (x >= 0x1AF0 || d1 == V.waterpos2) V.wtr_routine = 3;
    }
    V.waterpos3 = d1;
    return;
  case 3:
    d1 = 0x188;
    if (x >= 0x1AF0) {
      d1 = 0x900;
      if (x >= 0x1BC0) {
        V.wtr_routine = 4;
        V.waterpos3 = 0x608;
        V.waterpos2 = 0x7C0;
        V.f_switch[8] = 1;
        return;
      }
    }
    V.waterpos3 = V.waterpos2 = d1;
    return;
  default:
    if (x >= 0x1E00) V.waterpos3 = 0x128;
    return;
  }
}

static void LZDynamicWater(void) {
  switch (V.act) {
  case 0: DynWater_LZ1(); break;
  case 1: DynWater_LZ2(); break;
  case 2: DynWater_LZ3(); break;
  default: V.waterpos3 = (uint16_t)V.cam[0].x.w.i < 0xF00 ? 0x228 : 0x4C8; break;
  }
  int16_t d1 = V.f_water;
  if (V.waterpos3 == V.waterpos2) return;
  if ((uint16_t)V.waterpos3 < (uint16_t)V.waterpos2) d1 = (int16_t)-d1;
  V.waterpos2 = (int16_t)(V.waterpos2 + d1);
}

static void LZWindTunnels(void) {
  static const uint16_t data[5][4] = {{0xA80, 0x300, 0xC10, 0x380}, {0xF80, 0x100, 0x1410, 0x180},
                                      {0x460, 0x400, 0x710, 0x480}, {0xA20, 0x600, 0x1610, 0x6E0},
                                      {0xC80, 0x600, 0x13D0, 0x680}};
  if (V.debuguse) return;
  const uint16_t(*a2)[4] = &data[1 + V.act];
  int n = 1;
  if (!V.act) n = 2, a2 = &data[0];
  Obj *a1 = player;
  for (int i = 0; i < n; i++, a2++) {
    uint16_t x = (uint16_t)a1->x.w.i, y = (uint16_t)a1->y.w.i;
    if (x < (*a2)[0] || x >= (*a2)[2] || y < (*a2)[1] || y >= (*a2)[3]) continue;
    if (V.f_wtunneldisallow) return;
    if (a1->routine >= 4) {
      V.f_wtunnelmode = 0;
      return;
    }
    V.f_wtunnelmode = 1;
    if ((uint16_t)(x - 128) < (*a2)[0]) a1->y.w.i = (int16_t)(a1->y.w.i + (V.act == 1 ? -2 : 2));
    a1->x.w.i += 4;
    a1->vx = 0x400;
    a1->vy = 0;
    a1->anim = id_Float2;
    a1->status |= status_in_air;
    if (V.jpadhold2 & btnUp) a1->y.w.i--;
    if (V.jpadhold2 & btnDn) a1->y.w.i++;
    return;
  }
  if (!V.f_wtunnelmode) return;
  a1->anim = id_Walk;
  V.f_wtunnelmode = 0;
}

static void LZWaterSlides(void) {
  static const int8_t speeds[7] = {0xA, -0xB, 0xA, -0xA, -0xB, -0xC, 0xB};
  static const uint8_t chunks[7] = {2, 7, 3, 0x4C, 0x4B, 8, 4};
  Obj *a1 = player;
  int d1 = -1;
  if (!(a1->status & status_in_air)) {
    int idx = (((uint16_t)a1->y.w.i >> 1) & 0x380) + (((uint16_t)a1->x.w.i >> 8) & 0x7F);
    uint8_t c = (&lvllayout[0][0][0])[idx];
    for (d1 = 6; d1 >= 0 && chunks[d1] != c; d1--) {
    }
  }
  if (d1 < 0) {
    if (V.f_slidemode) {
      OW(a1, locktime) = 5;
      V.f_slidemode = 0;
    }
    return;
  }
  a1->status &= (uint8_t)~1;
  int8_t s = speeds[d1];
  a1->inertia = (int16_t)(s * 256);
  if (s < 0) a1->status |= 1;
  a1->anim = id_Slide;
  V.f_slidemode = 1;
}

void LZWaterFeatures(void) {
  if (V.zone != id_LZ) return;
  if (!V.f_nobgscroll && player->routine < 6) {
    LZWindTunnels();
    LZWaterSlides();
    LZDynamicWater();
  }
  V.f_wtr_state = 0;
  V.waterpos1 = (int16_t)((OSC_B(2) >> 1) + V.waterpos2);
  int16_t d0 = (int16_t)(V.waterpos1 - V.cam[0].y.w.i);
  if ((uint16_t)V.waterpos1 < (uint16_t)V.cam[0].y.w.i && d0 < 0) {
    V.hblank_line = 223;
    V.f_wtr_state = 1;
  }
  if ((uint16_t)d0 >= 223) d0 = 223;
  V.hblank_line = (uint8_t)d0;
}

/* air back (out of the water, or a bubble) */
void ResumeMusic(void) {
  V.air = 30;
  OB(OBJ(13), 0x32) = 0; /* (the drowning countdown's number delay, as the original clears it) */
}
