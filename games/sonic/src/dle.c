/* Dynamic level events: each act's camera limits as Sonic goes through it (the bottom boundary
 * moves 2 pixels a frame towards its target, 8 when he falls), and the bosses set off when the
 * camera reaches their arena: the screen locks and the boss art is loaded. */
#include "s1.h"
#include "game.h"

static void spawn_boss(uint8_t id, int16_t x, int16_t y, bool place) {
  Obj *a1 = FindFreeObj();
  if (a1) {
    a1->id = id;
    if (place) a1->x.w.i = x, a1->y.w.i = y;
  }
  V.f_lockscreen = 1;
  V.dle_routine += 2;
  AddPLC(plcid_Boss);
}

static void dle_ghz(void) {
  int16_t cx = V.cam[0].x.w.i, cy = V.cam[0].y.w.i;
  switch (V.act) {
  case 0:
    V.limitbtm1 = 0x300;
    if ((uint16_t)cx >= 0x1780) V.limitbtm1 = 0x400;
    break;
  case 1:
    V.limitbtm1 = 0x300;
    if ((uint16_t)cx < 0xED0) break;
    V.limitbtm1 = 0x200;
    if ((uint16_t)cx < 0x1600) break;
    V.limitbtm1 = 0x400;
    if ((uint16_t)cx < 0x1D60) break;
    V.limitbtm1 = 0x300;
    break;
  default:
    switch (V.dle_routine) {
    case 0:
      V.limitbtm1 = 0x300;
      if ((uint16_t)cx < 0x380) return;
      V.limitbtm1 = 0x310;
      if ((uint16_t)cx < 0x960) return;
      if ((uint16_t)cy >= 0x280) {
        V.limitbtm1 = 0x400;
        if ((uint16_t)cx < 0x1380) V.limitbtm1 = V.limitbtm2 = 0x4C0;
        if ((uint16_t)cx < 0x1700) return;
      }
      V.limitbtm1 = boss_ghz_y;
      V.dle_routine += 2;
      return;
    case 2:
      if ((uint16_t)cx < 0x960) V.dle_routine -= 2;
      if ((uint16_t)cx < boss_ghz_x) return;
      spawn_boss(id_BossGreenHill, boss_ghz_x + 0x100, boss_ghz_y - 0x80, true);
      return;
    default: V.limitleft2 = cx; return;
    }
  }
}

static void dle_lz(void) {
  int16_t cx = V.cam[0].x.w.i, cy = V.cam[0].y.w.i;
  if (V.act == 2) {
    if (V.f_switch[0xF]) {
      uint8_t *a1 = &lvllayout[2][0][6]; /* (v_lvllayout_fg + layout_row * 2 + 6) */
      if (*a1 != 7) *a1 = 7;
    }
    if (V.dle_routine) return;
    if ((uint16_t)cx < boss_lz_x - 0x140 || (uint16_t)cy >= boss_lz_y + 0x540) return;
    spawn_boss(id_BossLabyrinth, 0, 0, false);
  } else if (V.act == 3) {
    if ((uint16_t)cx < 0xD00 || (uint16_t)player->y.w.i >= 0x18) return;
    V.lastlamp = 0;
    V.f_restart = 1;
    V.zone = id_SBZ, V.act = 2;
    V.f_playerctrl = 1;
  }
}

static void dle_mz(void) {
  int16_t cx = V.cam[0].x.w.i, cy = V.cam[0].y.w.i;
  if (V.act == 0) {
    switch (V.dle_routine) {
    case 0:
      V.limitbtm1 = 0x1D0;
      if ((uint16_t)cx < 0x700) return;
      V.limitbtm1 = 0x220;
      if ((uint16_t)cx < 0xD00) return;
      V.limitbtm1 = 0x340;
      if ((uint16_t)cy < 0x340) return;
      V.dle_routine += 2;
      return;
    case 2:
      if ((uint16_t)cy < 0x340) {
        V.dle_routine -= 2;
        return;
      }
      V.limittop2 = 0;
      if ((uint16_t)cx >= 0xE00) return;
      V.limittop2 = V.limitbtm1 = 0x340;
      if ((uint16_t)cx >= 0xA90) return;
      V.limitbtm1 = 0x500;
      if ((uint16_t)cy < 0x370) return;
      V.dle_routine += 2;
      return;
    case 4:
      if ((uint16_t)cy < 0x370) {
        V.dle_routine -= 2;
        return;
      }
      if ((uint16_t)cy < 0x500 || (uint16_t)cx < 0xB80) return;
      V.limittop2 = 0x500;
      V.dle_routine += 2;
      return;
    default:
      if ((uint16_t)cx < 0xB80) {
        if (V.limittop2 == 0x340) return;
        V.limittop2 -= 2;
        return;
      }
      if (V.limittop2 != 0x500) {
        if ((uint16_t)cy < 0x500) return;
        V.limittop2 = 0x500;
      }
      if ((uint16_t)cx < 0xE70) return;
      V.limittop2 = 0;
      V.limitbtm1 = 0x500;
      if ((uint16_t)cx < 0x1430) return;
      V.limitbtm1 = 0x210;
      return;
    }
  }
  if (V.act == 1) {
    V.limitbtm1 = 0x520;
    if ((uint16_t)cx >= 0x1700) V.limitbtm1 = 0x200;
    return;
  }
  if (V.dle_routine == 0) {
    V.limitbtm1 = 0x720;
    if ((uint16_t)cx < boss_mz_x - 0x2A0) return;
    V.limitbtm1 = boss_mz_y;
    if ((uint16_t)cx < boss_mz_x - 0x10) return;
    spawn_boss(id_BossMarble, boss_mz_x + 0x1F0, boss_mz_y + 0x1C, true);
    return;
  }
  V.limitleft2 = cx;
}

static void dle_slz(void) {
  int16_t cx = V.cam[0].x.w.i;
  if (V.act != 2) return;
  switch (V.dle_routine) {
  case 0:
    if ((uint16_t)cx < boss_slz_x - 0x190) return;
    V.limitbtm1 = boss_slz_y;
    V.dle_routine += 2;
    return;
  case 2:
    if ((uint16_t)cx < boss_slz_x) return;
    spawn_boss(id_BossStarLight, 0, 0, false);
    return;
  default: V.limitleft2 = cx; return;
  }
}

static void dle_syz(void) {
  int16_t cx = V.cam[0].x.w.i;
  if (V.act == 1) {
    V.limitbtm1 = 0x520;
    if ((uint16_t)cx < 0x25A0) return;
    V.limitbtm1 = 0x420;
    if ((uint16_t)player->y.w.i < 0x4D0) return;
    V.limitbtm1 = 0x520;
    return;
  }
  if (V.act != 2) return;
  switch (V.dle_routine) {
  case 0: {
    if ((uint16_t)cx < boss_syz_x - 0x140) return;
    Obj *a1 = FindFreeObj();
    if (!a1) return;
    a1->id = id_BossBlock;
    V.dle_routine += 2;
    return;
  }
  case 2: {
    if ((uint16_t)cx < boss_syz_x) return;
    V.limitbtm1 = boss_syz_y;
    Obj *a1 = FindFreeObj();
    if (a1) a1->id = id_BossSpringYard, V.dle_routine += 2;
    V.f_lockscreen = 1;
    AddPLC(plcid_Boss);
    return;
  }
  default: V.limitleft2 = cx; return;
  }
}

static void dle_sbz(void) {
  int16_t cx = V.cam[0].x.w.i;
  if (V.act == 0) {
    V.limitbtm1 = 0x720;
    if ((uint16_t)cx < 0x1880) return;
    V.limitbtm1 = 0x620;
    if ((uint16_t)cx < 0x2000) return;
    V.limitbtm1 = 0x2A0;
    return;
  }
  if (V.act == 1) {
    switch (V.dle_routine) {
    case 0:
      V.limitbtm1 = 0x800;
      if ((uint16_t)cx < 0x1800) return;
      V.limitbtm1 = boss_sbz2_y;
      if ((uint16_t)cx < 0x1E00) return;
      V.dle_routine += 2;
      return;
    case 2: {
      if ((uint16_t)cx < boss_sbz2_x - 0x1A0) return;
      Obj *a1 = FindFreeObj();
      if (!a1) return;
      a1->id = id_FalseFloor;
      V.dle_routine += 2;
      AddPLC(plcid_EggmanSBZ2);
      return;
    }
    case 4:
      if ((uint16_t)cx >= boss_sbz2_x - 0xF0) {
        Obj *a1 = FindFreeObj();
        if (a1) a1->id = id_ScrapEggman, V.dle_routine += 2;
        V.f_lockscreen = 1;
      }
      V.limitleft2 = V.cam[0].x.w.i;
      return;
    default:
      if ((uint16_t)cx < boss_sbz2_x) V.limitleft2 = cx;
      return;
    }
  }
  /* the Final Zone */
  switch (V.dle_routine) {
  case 0:
    if ((uint16_t)cx >= boss_fz_x - 0x308) {
      V.dle_routine += 2;
      AddPLC(plcid_FZBoss);
    }
    break;
  case 2:
    if ((uint16_t)cx >= boss_fz_x - 0x150) {
      Obj *a1 = FindFreeObj();
      if (a1) {
        a1->id = id_BossFinal;
        V.dle_routine += 2;
        V.f_lockscreen = 1;
      }
    }
    break;
  case 4:
    if ((uint16_t)cx >= boss_fz_x) V.dle_routine += 2;
    break;
  case 6: return;
  default: break;
  }
  V.limitleft2 = V.cam[0].x.w.i;
}

void DynamicLevelEvents(void) {
  switch (V.zone) {
  case id_GHZ: dle_ghz(); break;
  case id_LZ: dle_lz(); break;
  case id_MZ: dle_mz(); break;
  case id_SLZ: dle_slz(); break;
  case id_SYZ: dle_syz(); break;
  case id_SBZ: dle_sbz(); break;
  default: break;
  }
  int16_t d1 = 2;
  int16_t diff = (int16_t)(V.limitbtm1 - V.limitbtm2);
  if (!diff) return;
  if ((uint16_t)V.limitbtm1 >= (uint16_t)V.limitbtm2) {
    if ((uint16_t)(V.cam[0].y.w.i + 8) >= (uint16_t)V.limitbtm2 && (player->status & 2)) d1 = 8;
    V.limitbtm2 += d1;
    V.f_bgscrollvert = 1;
    return;
  }
  if ((uint16_t)V.cam[0].y.w.i > (uint16_t)V.limitbtm1) V.limitbtm2 = (int16_t)(V.cam[0].y.w.i & 0xFFFE);
  V.limitbtm2 -= 2;
  V.f_bgscrollvert = 1;
}
