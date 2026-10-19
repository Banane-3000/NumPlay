/* The cards: the zone's name sliding in before an act, GAME OVER and TIME OVER, and the tally
 * after an act (time and ring bonuses counted into the score, then the next act in the original's
 * order). The signpost that ends an act, spinning when passed, and the lampposts that keep where
 * Sonic restarts. */
#include "s1.h"
#include "game.h"
#include "objs.h"

#define card_mainX 0x30
#define card_finalX 0x32

/* x towards target by step a frame; on screen: shown */
static bool card_move(Obj *o, int16_t target, int16_t step) {
  if (target != o->x.w.i) o->x.w.i = (int16_t)(o->x.w.i + (target > o->x.w.i ? step : -step));
  return target == o->x.w.i;
}
static void card_show(Obj *o) {
  if (o->x.w.i >= 0 && o->x.w.i < 0x80 + 320 + 64) DisplaySprite(o);
}

void TitleCard(Obj *o) {
  static const int16_t con[7][8] = {
    {0x000, 0x120, -0x104, 0x13C, 0x414, 0x154, 0x214, 0x154},
    {0x000, 0x120, -0x10C, 0x134, 0x40C, 0x14C, 0x20C, 0x14C},
    {0x000, 0x120, -0x120, 0x120, 0x3F8, 0x138, 0x1F8, 0x138},
    {0x000, 0x120, -0x104, 0x13C, 0x414, 0x154, 0x214, 0x154},
    {0x000, 0x120, -0x0FC, 0x144, 0x41C, 0x15C, 0x21C, 0x15C},
    {0x000, 0x120, -0x0FC, 0x144, 0x41C, 0x15C, 0x21C, 0x15C},
    {0x000, 0x120, -0x11C, 0x124, 0x3EC, 0x3EC, 0x1EC, 0x12C},
  };
  static const struct {
    int16_t y;
    uint8_t routine, frame;
  } items[4] = {{0xD0, 2, 0}, {0xE4, 2, 6}, {0xEA, 2, 7}, {0xE0, 2, 0xA}};
  switch (o->routine) {
  case 0: {
    int z = V.zone;
    if (ZONE_ACT == id_LZ_act4) z = 5;
    int d2 = z;
    if (ZONE_ACT == id_FZ) z = 6, d2 = 0xB;
    for (int i = 0; i < 4; i++) {
      Obj *a1 = o + i;
      a1->id = id_TitleCard;
      a1->x.w.i = OW(a1, card_finalX) = con[z][i * 2];
      OW(a1, card_mainX) = con[z][i * 2 + 1];
      obScreenY(a1) = items[i].y;
      a1->routine = items[i].routine;
      uint8_t f = items[i].frame;
      if (!f) f = (uint8_t)d2;
      if (f == 7) {
        f = (uint8_t)(f + V.act);
        if (V.act == 3) f--;
      }
      a1->frame = f;
      a1->map = map_get(MAP_Card);
      a1->gfx = ArtTile_Title_Card | Tile_Prio;
      a1->actwid = 240 / 2;
      a1->render = sprite_cam_screen;
      a1->priority = 0;
      set_timew(a1, 60);
    }
  }
    /* fall through */
  case 2:
    card_move(o, OW(o, card_mainX), 0x10);
    card_show(o);
    return;
  default:
    if (timew(o)) {
      set_timew(o, (uint16_t)(timew(o) - 1));
      DisplaySprite(o);
      return;
    }
    if ((o->render & 0x80) && OW(o, card_finalX) != o->x.w.i) {
      card_move(o, OW(o, card_finalX), 0x20);
      card_show(o);
      return;
    }
    if (o->routine == 4) {
      AddPLC(plcid_Explode);
      AddPLC(plcid_GHZAnimals + V.zone);
    }
    DeleteObject(o);
  }
}

void GameOverCard(Obj *o) {
  switch (o->routine) {
  case 0:
    o->routine += 2;
    o->x.w.i = 0x80 - 48;
    if (o->frame & 1) o->x.w.i = 0x80 + 320 + 48;
    obScreenY(o) = 0x80 + 224 / 2;
    o->map = map_get(MAP_Over);
    o->gfx = ArtTile_Game_Over | Tile_Prio;
    o->render = sprite_cam_screen;
    o->priority = 0;
    /* fall through */
  case 2:
    if (o->x.w.i != 0x80 + 320 / 2) {
      o->x.w.i = (int16_t)(o->x.w.i + ((uint16_t)o->x.w.i < 0x80 + 320 / 2 ? 0x10 : -0x10));
      DisplaySprite(o);
      return;
    }
    set_timew(o, 12 * 60);
    o->routine += 2;
    return;
  case 4:
    if (!(V.jpadpress1 & btnABC)) {
      if (o->frame & 1) {
        DisplaySprite(o);
        return;
      }
      if (timew(o)) {
        set_timew(o, (uint16_t)(timew(o) - 1));
        DisplaySprite(o);
        return;
      }
    }
    if (V.f_timeover) {
      V.lamp_timemin = V.lamp_timesec = V.lamp_timecent = 0;
      V.f_restart = 1;
    } else {
      V.gamemode = id_Continue;
      if (!V.continues) V.gamemode = id_Sega, save_clear_progress();
    }
    DisplaySprite(o);
  }
}

#define got_mainX 0x30
#define got_finalX 0x32

static const uint16_t level_order[6][4] = {
  {id_GHZ_act2, id_GHZ_act3, id_MZ_act1, 0},  {id_LZ_act2, id_LZ_act3, id_SLZ_act1, id_FZ},
  {id_MZ_act2, id_MZ_act3, id_SYZ_act1, 0},   {id_SLZ_act2, id_SLZ_act3, id_SBZ_act1, 0},
  {id_SYZ_act2, id_SYZ_act3, id_LZ_act1, 0},  {id_SBZ_act2, id_LZ_act4, 0, 0},
};

void GotThroughCard(Obj *o) {
  static const struct {
    int16_t x, main, y;
    uint8_t routine, frame;
  } items[7] = {{0x004, 0x124, 0xBC, 2, 0}, {-0x120, 0x120, 0xD0, 2, 1}, {0x40C, 0x14C, 0xD6, 2, 6},
                {0x520, 0x120, 0xEC, 2, 2},  {0x540, 0x120, 0xFC, 2, 3},  {0x560, 0x120, 0x10C, 2, 4},
                {0x20C, 0x14C, 0xCC, 2, 5}};
  switch (o->routine) {
  case 0:
    for (int i = 0; i < 7; i++) {
      Obj *a1 = o + i;
      a1->id = id_GotThroughCard;
      a1->x.w.i = OW(a1, got_finalX) = items[i].x;
      OW(a1, got_mainX) = items[i].main;
      obScreenY(a1) = items[i].y;
      a1->routine = items[i].routine;
      uint8_t f = items[i].frame;
      if (f == 6) f = (uint8_t)(f + V.act);
      a1->frame = f;
      a1->map = map_get(MAP_Got);
      a1->gfx = ArtTile_Title_Card | Tile_Prio;
      a1->render = sprite_cam_screen;
    }
    /* fall through */
  case 2:
    if (!card_move(o, OW(o, got_mainX), 0x10)) {
      card_show(o);
      return;
    }
    if (OBJ(23 + 5)->routine == 0xE) {
      o->routine = 0xE;
      goto moveout;
    }
    if (o->frame != 4) {
      card_show(o);
      return;
    }
    o->routine += 2;
    set_timew(o, 3 * 60);
    /* fall through */
  case 4:
  case 8:
  case 0xC:
    set_timew(o, (uint16_t)(timew(o) - 1));
    if (!timew(o)) o->routine += 2;
    DisplaySprite(o);
    return;
  case 6: {
    DisplaySprite(o);
    V.f_endactbonus = 1;
    int d0 = 0;
    if (V.timebonus) d0 += 10, V.timebonus -= 10;
    if (V.ringbonus) d0 += 10, V.ringbonus -= 10;
    if (d0) {
      AddPoints(d0);
      return;
    }
    o->routine += 2;
    if (ZONE_ACT == id_SBZ_act2) o->routine += 4;
    set_timew(o, 3 * 60);
    return;
  }
  case 0xA: {
    uint16_t next = level_order[V.zone & 7][V.act & 3];
    set_zone_act(next);
    if (!next) V.gamemode = id_Sega;
    else {
      V.lastlamp = 0;
      if (V.f_bigring) V.gamemode = id_Special;
      else V.f_restart = 1;
    }
    DisplaySprite(o);
    return;
  }
  case 0xE:
  moveout:
    if (!card_move(o, OW(o, got_finalX), 0x20)) {
      card_show(o);
      return;
    }
    if (o->frame != 4) {
      DeleteObject(o);
      return;
    }
    o->routine += 2;
    V.f_lockctrl = 0;
    return;
  case 0x10:
    V.limitright2 = (int16_t)(V.limitright2 + 2);
    if (V.limitright2 == boss_sbz2_x + 0xB0) DeleteObject(o);
    return;
  }
}

/* ------------------------------------------------------------------ the signpost */
#define spintime 0x30
#define sparkletime 0x32
#define sparkle_id 0x34

void GotThroughAct(void) {
  if (OBJ(23)->id) return;
  V.limitleft2 = V.limitright2;
  V.invinc = 0;
  V.f_timecount = 0;
  OBJ(23)->id = id_GotThroughCard;
  AddPLC(plcid_TitleCard);
  V.f_endactbonus = 1;
  static const uint16_t bonuses[21] = {5000, 5000, 1000, 500, 400, 400, 300, 300, 200, 200, 200,
                                       200,  100,  100,  100, 100, 50,  50,  50,  50,  0};
  unsigned t = (V.timemin * 60u + V.timesec) / 15;
  if (t > 20) t = 20;
  V.timebonus = bonuses[t];
  V.ringbonus = (uint16_t)(V.rings * 10);
}

void Signpost(Obj *o) {
  static const int8_t spark[8][2] = {{-0x18, -0x10}, {8, 8},     {-0x10, 0}, {0x18, -8},
                                     {0, -8},        {0x10, 0}, {-0x18, 8}, {0x18, 0x10}};
  switch (o->routine) {
  case 0:
    o->routine += 2;
    o->map = map_get(MAP_Sign);
    o->gfx = ArtTile_Signpost;
    o->render = sprite_cam_field;
    o->actwid = 48 / 2;
    o->priority = 4;
    /* fall through */
  case 2: {
    uint16_t d = (uint16_t)(player->x.w.i - o->x.w.i);
    if (d < 32) {
      V.f_timecount = 0;
      V.limitleft2 = V.limitright2;
      o->routine += 2;
    }
    break;
  }
  case 4: {
    if ((int16_t)--OW(o, spintime) < 0) {
      OW(o, spintime) = 60;
      if (++o->anim == 3) o->routine += 2;
    }
    if ((int16_t)--OW(o, sparkletime) >= 0) break;
    OW(o, sparkletime) = 12 - 1;
    int k = OB(o, sparkle_id) >> 1;
    OB(o, sparkle_id) = (uint8_t)((OB(o, sparkle_id) + 2) & 0xE);
    Obj *a1 = FindFreeObj();
    if (!a1) break;
    a1->id = id_Rings;
    a1->routine = 6;
    a1->x.w.i = (int16_t)(o->x.w.i + spark[k][0]);
    a1->y.w.i = (int16_t)(o->y.w.i + spark[k][1]);
    a1->map = map_get(MAP_Ring);
    a1->gfx = ArtTile_Ring | Tile_Pal2;
    a1->render = sprite_cam_field;
    a1->priority = 2;
    a1->actwid = 8;
    break;
  }
  case 6:
    if (V.debuguse) break;
    if (!(player->status & status_in_air)) {
      V.f_lockctrl = 1;
      V.jpadhold2 = btnR, V.jpadpress2 = 0;
    }
    if (player->id && (uint16_t)player->x.w.i < (uint16_t)(V.limitright2 + 320 - 24)) break;
    o->routine += 2;
    GotThroughAct();
    break;
  }
  AnimateSprite(o, ani_get(ANI_Sign));
  DisplaySprite(o);
  if (obj_far(o)) DeleteObject(o);
}

/* ------------------------------------------------------------------ lampposts */
#define lamp_origX 0x30
#define lamp_origY 0x32
#define lamp_time 0x36

static void Lamp_StoreInfo(Obj *o) {
  V.lastlamp = obSubtype(o);
  V.lastlamp_dup = V.lastlamp;
  V.lamp_xpos = o->x.w.i;
  V.lamp_ypos = o->y.w.i;
  V.lamp_rings = V.rings;
  V.lamp_lives = V.lifecount;
  V.lamp_timemin = V.timemin, V.lamp_timesec = V.timesec, V.lamp_timecent = V.timecent;
  V.lamp_dle = V.dle_routine;
  V.lamp_limitbtm = V.limitbtm2;
  V.lamp_scrx = V.cam[0].x.w.i, V.lamp_scry = V.cam[0].y.w.i;
  V.lamp_bgscrx = V.cam[1].x.w.i, V.lamp_bgscry = V.cam[1].y.w.i;
  V.lamp_bg2scrx = V.cam[2].x.w.i, V.lamp_bg2scry = V.cam[2].y.w.i;
  V.lamp_bg3scrx = V.cam[3].x.w.i, V.lamp_bg3scry = V.cam[3].y.w.i;
  V.lamp_wtrpos = V.waterpos2;
  V.lamp_wtrrout = V.wtr_routine;
  V.lamp_wtrstat = V.f_wtr_state;
}

void Lamp_LoadInfo(void) {
  V.lastlamp = V.lastlamp_dup;
  player->x.w.i = V.lamp_xpos;
  player->y.w.i = V.lamp_ypos;
  /* (the rings and the extra-life flags kept are cleared at once, as in the original) */
  V.rings = 0;
  V.lifecount = 0;
  V.timemin = V.lamp_timemin, V.timesec = V.lamp_timesec;
  V.timecent = 60 - 1;
  V.timesec--;
  V.dle_routine = V.lamp_dle;
  V.wtr_routine = V.lamp_wtrrout;
  V.limitbtm2 = V.limitbtm1 = V.lamp_limitbtm;
  V.cam[0].x.w.i = V.lamp_scrx, V.cam[0].y.w.i = V.lamp_scry;
  V.cam[1].x.w.i = V.lamp_bgscrx, V.cam[1].y.w.i = V.lamp_bgscry;
  V.cam[2].x.w.i = V.lamp_bg2scrx, V.cam[2].y.w.i = V.lamp_bg2scry;
  V.cam[3].x.w.i = V.lamp_bg3scrx, V.cam[3].y.w.i = V.lamp_bg3scry;
  if (V.zone == id_LZ) {
    V.waterpos2 = V.lamp_wtrpos;
    V.wtr_routine = V.lamp_wtrrout;
    V.f_wtr_state = V.lamp_wtrstat;
  }
  if (V.lastlamp & 0x80) V.limitleft2 = (int16_t)(V.lamp_xpos - 320 / 2);
}

void Lamppost(Obj *o) {
  uint8_t *st = &V.objstate[2 + o->respawn];
  switch (o->routine) {
  case 0:
    o->routine += 2;
    o->map = map_get(MAP_Lamp);
    o->gfx = ArtTile_Lamppost;
    o->render = sprite_cam_field;
    o->actwid = 16 / 2;
    o->priority = 5;
    *st &= 0x7F;
    if ((*st & 1) || (V.lastlamp & 0x7F) >= (obSubtype(o) & 0x7F)) {
      *st |= 1;
      o->routine = 4;
      o->frame = 3;
      break;
    }
    /* fall through */
  case 2: {
    if (V.debuguse || (V.f_playerctrl & 0x80)) break;
    if ((V.lastlamp & 0x7F) >= (obSubtype(o) & 0x7F)) {
      *st |= 1;
      o->routine = 4;
      o->frame = 3;
      break;
    }
    if ((uint16_t)(player->x.w.i - o->x.w.i + 8) >= 16) break;
    if ((uint16_t)(player->y.w.i - o->y.w.i + 0x40) >= 0x68) break;
    o->routine += 2;
    Obj *a1 = FindFreeObj();
    if (a1) {
      a1->id = id_Lamppost;
      a1->routine = 6;
      OW(a1, lamp_origX) = o->x.w.i;
      OW(a1, lamp_origY) = (int16_t)(o->y.w.i - 0x18);
      a1->map = map_get(MAP_Lamp);
      a1->gfx = ArtTile_Lamppost;
      a1->render = sprite_cam_field;
      a1->actwid = 16 / 2;
      a1->priority = 4;
      a1->frame = 2;
      OW(a1, lamp_time) = 32;
    }
    o->frame = 1;
    Lamp_StoreInfo(o);
    *st |= 1;
    break;
  }
  case 4: break;
  case 6: {
    if ((int16_t)--OW(o, lamp_time) < 0) o->routine = 4;
    uint8_t a = o->angle;
    o->angle = (uint8_t)(o->angle - 0x10);
    int16_t c, s = CalcSine((uint8_t)(a - 0x40), &c);
    o->x.w.i = (int16_t)(((int32_t)c * 0xC00 >> 16) + OW(o, lamp_origX));
    o->y.w.i = (int16_t)(((int32_t)s * 0xC00 >> 16) + OW(o, lamp_origY));
    break;
  }
  }
  RememberState(o);
}
