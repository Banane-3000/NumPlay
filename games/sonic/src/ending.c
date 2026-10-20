/* The game's last screens: the continue screen (Sonic tapping his foot under the countdown), the
 * ending (Sonic running back through Green Hill to the flowers, the emeralds round him if he has all
 * six), the credits with their demos, and Dr. Robotnik juggling the emeralds Sonic did not get. */
#include "s1.h"
#include "game.h"
#include "objs.h"

#define ART(a) ((const uint8_t *)DATA(D_TILES) + (uint32_t)DATAW(D_ARTS)[(a) * 2] * 32)
extern const uint8_t *ending_flowers;

/* ------------------------------------------------------------------ the continue screen */
void ContScrItem(Obj *o) {
  static const int16_t minsonx[15] = {0x116, 0x12A, 0x102, 0x13E, 0xEE, 0x152, 0xDA, 0x166,
                                      0xC6, 0x17A, 0xB2, 0x18E, 0x9E, 0x1A2, 0x8A};
  switch (o->routine) {
  case 0:
    o->routine += 2;
    o->map = map_get(MAP_ContScr);
    o->gfx = ArtTile_Continue_Sonic | Tile_Prio;
    o->render = sprite_cam_screen;
    o->actwid = 120 / 2;
    o->x.w.i = 0x80 + 320 / 2;
    obScreenY(o) = 0x80 + 64;
    V.rings = 0;
    /* fall through */
  case 2: DisplaySprite(o); return;
  case 4: {
    /* a little Sonic for each continue left after this one */
    int d1 = V.continues - 2;
    if (d1 < 0) {
      DeleteObject(o);
      return;
    }
    uint8_t d3 = 1;
    if (d1 >= 15 - 1) d3 = 0, d1 = 15 - 1;
    bool odd = d1 & 1;
    Obj *a1 = o;
    for (int i = 0; i <= d1; i++, a1++) {
      a1->id = id_ContScrItem;
      a1->x.w.i = (int16_t)(minsonx[i] - (odd ? 10 : 0));
      obScreenY(a1) = 0x80 + 0x50;
      a1->frame = 6;
      a1->routine = 6;
      a1->map = map_get(MAP_ContScr);
      a1->gfx = ArtTile_Mini_Sonic | Tile_Prio;
      a1->render = sprite_cam_screen;
    }
    obSubtype(a1 - 1) = d3;
  }
    /* fall through */
  case 6:
    /* (the last one, the continue being used, goes when Sonic runs off) */
    if (obSubtype(o) && player->routine >= 6 && !(vblank_byte & 1)) {
      if (player->vx) DeleteObject(o);
      return;
    }
    if (!(vblank_byte & 0xF)) o->frame ^= 1;
    DisplaySprite(o);
    return;
  }
}

void ContSonic(Obj *o) {
  switch (o->routine) {
  case 0:
    o->routine += 2;
    o->x.w.i = 0xA0;
    o->y.w.i = 0xC0;
    o->map = map_get(MAP_Sonic);
    o->gfx = ArtTile_Sonic;
    o->render = sprite_cam_field;
    o->priority = 2;
    o->anim = id_Float3;
    o->vy = 0x400;
    /* fall through */
  case 2:
    if (o->y.w.i != 0x1A0) {
      SpeedToPos(o);
      Sonic_Animate(o);
      Sonic_LoadGfx(o);
      break;
    }
    o->routine += 2;
    o->vy = 0;
    o->map = map_get(MAP_ContScr);
    o->gfx = ArtTile_Continue_Sonic | Tile_Prio;
    o->anim = 0;
    /* fall through */
  case 4:
    /* (Start, or the jump buttons, as the title takes them) */
    if (!(V.jpadpress1 & (btnStart | btnABC))) {
      AnimateSprite(o, ani_get(ANI_CSon));
      break;
    }
    o->routine += 2;
    o->map = map_get(MAP_Sonic);
    o->gfx = ArtTile_Sonic;
    o->anim = id_Float4;
    o->inertia = 0;
    o->y.w.i = (int16_t)(o->y.w.i - 8);
    /* fall through */
  case 6:
    if (o->inertia == 0x40 * 0x20) o->vx = 0x1000;
    else o->inertia = (int16_t)(o->inertia + 0x20);
    SpeedToPos(o);
    Sonic_Animate(o);
    Sonic_LoadGfx(o);
    break;
  }
  DisplaySprite(o);
}

void GM_Continue(void) {
  PaletteFadeOut();
  vdp_backdrop = 0;
  ClearScreen();
  memset(objs, 0, sizeof objs);
  vdp_load_art(ART_Nem_TitleCard, ArtTile_Title_Card);
  vdp_load_art(ART_Nem_ContSonic, ArtTile_Continue_Sonic);
  vdp_load_art(ART_Nem_MiniSonic, ArtTile_Mini_Sonic);
  ContScrCounter(10);
  PalLoad_Fade(palid_Continue);
  V.generictimer = 11 * 60 - 1;
  V.cam[0].x.l = 0;
  V.cam[0].y.l = 0x1000000;
  player->id = id_ContSonic;
  OBJ(1)->id = id_ContScrItem;
  OBJ(2)->id = id_ContScrItem;
  OBJ(2)->priority = 3;
  OBJ(2)->frame = 4;
  OBJ(3)->id = id_ContScrItem;
  OBJ(3)->routine = 4;
  ExecuteObjects();
  BuildSprites();
  PaletteFadeIn();
  for (;;) {
    V.vblank_routine = id_VBlank_Continue;
    WaitForVBlank();
    if (player->routine < 6) ContScrCounter((uint32_t)(V.generictimer / 60) & 0xF);
    ExecuteObjects();
    BuildSprites();
    if ((uint16_t)player->x.w.i >= 320 + 64) break;
    if (player->routine >= 6 || V.generictimer) continue;
    V.gamemode = id_Sega;
    return;
  }
  V.gamemode = id_Level;
  V.lives = 3;
  V.rings = 0;
  V.timemin = V.timesec = V.timecent = 0;
  V.score = 0;
  V.lastlamp = 0;
  V.continues--;
}

/* ------------------------------------------------------------------ the ending */
#define eson_time 0x30
#define echa_origX 0x38
#define echa_origY 0x3A
#define echa_radius 0x3C
#define echa_angle 0x3E

void EndSonic(Obj *o) {
  switch (o->routine2) {
  case 0:
    if (V.emeralds != 6) {
      o->routine2 += 0x10;
      OW(o, eson_time) = 3 * 60 + 36;
      break;
    }
    o->routine2 += 2;
    o->map = map_get(MAP_ESon);
    o->gfx = ArtTile_Ending_Sonic;
    o->render = sprite_cam_field;
    o->status = 0;
    o->priority = 2;
    o->frame = 0;
    OW(o, eson_time) = 1 * 60 + 20;
    /* fall through */
  case 2:
    /* holding up the emeralds */
    if (--OW(o, eson_time)) break;
    o->routine2 += 2;
    o->anim = 0, o->prevani = 1;
    OBJ(16)->id = id_EndChaos;
    break;
  case 6:
    if (OW(OBJ(16), echa_radius) != 0x20 * 0x100) break;
    V.f_restart = 1;
    OW(o, eson_time) = 1 * 60 + 30;
    o->routine2 += 2;
    break;
  case 8:
    if (--OW(o, eson_time)) break;
    memset(OBJ(16), 0, sizeof(Obj) * 16);
    V.f_restart = 1;
    o->routine2 += 2;
    o->anim = 1;
    OW(o, eson_time) = 1 * 60;
    break;
  case 0xC:
    if (--OW(o, eson_time)) break;
    o->routine2 += 2;
    OW(o, eson_time) = 3 * 60;
    o->anim = 2;
    OBJ(16)->id = id_EndSTH;
    break;
  case 0x10:
    if (--OW(o, eson_time)) break;
    o->routine2 += 2;
    o->map = map_get(MAP_ESon);
    o->gfx = ArtTile_Ending_Sonic;
    o->render = sprite_cam_field;
    o->status = 0;
    o->priority = 2;
    o->frame = 5;
    o->anim = 2;
    OBJ(16)->id = id_EndSTH;
    /* fall through */
  case 4:
  case 0xA:
  case 0xE:
  case 0x12: AnimateSprite(o, ani_get(ANI_ESon)); break;
  }
  DisplaySprite(o);
}

void EndChaos(Obj *o) {
  if (o->routine == 0) {
    if (player->frame != 2) return;
    o->x.w.i = player->x.w.i;
    o->y.w.i = player->y.w.i;
    uint8_t d3 = 0;
    for (int i = 0; i < 6; i++) {
      Obj *a1 = o + i;
      a1->id = id_EndChaos;
      a1->routine += 2;
      a1->map = map_get(MAP_ECha);
      a1->gfx = ArtTile_Ending_Emeralds;
      a1->render = sprite_cam_field;
      a1->priority = 1;
      OW(a1, echa_origX) = o->x.w.i;
      OW(a1, echa_origY) = o->y.w.i;
      a1->anim = a1->frame = (uint8_t)(i + 1);
      a1->angle = d3;
      d3 = (uint8_t)(d3 + 0x100 / 6);
    }
  }
  /* round Sonic, spreading out, faster, rising */
  set_anglew(o, (uint16_t)(anglew(o) + OW(o, echa_angle)));
  int16_t cos, sin = CalcSine(o->angle, &cos);
  int16_t r = OWH(o, echa_radius);
  o->x.w.i = (int16_t)(((int32_t)cos * r >> 8) + OW(o, echa_origX));
  o->y.w.i = (int16_t)(((int32_t)sin * r >> 8) + OW(o, echa_origY));
  if (OW(o, echa_radius) != 0x20 * 0x100) OW(o, echa_radius) += 0x20;
  if (OW(o, echa_angle) != 0x20 * 0x100) OW(o, echa_angle) += 0x20;
  if (OW(o, echa_origY) != 0x140) OW(o, echa_origY)--;
  DisplaySprite(o);
}

#define esth_time 0x30

void EndSTH(Obj *o) {
  switch (o->routine) {
  case 0:
    o->routine += 2;
    o->x.w.i = 0x80 - 0xA0;
    obScreenY(o) = 0x80 + 0x58;
    o->map = map_get(MAP_ESth);
    o->gfx = ArtTile_Ending_STH;
    o->render = sprite_cam_screen;
    o->priority = 0;
    /* fall through */
  case 2:
    if (o->x.w.i != 0x80 + 0x40) {
      o->x.w.i = (int16_t)(o->x.w.i + 0x10);
      break;
    }
    o->routine += 2;
    OW(o, esth_time) = 5 * 60;
    /* fall through */
  case 4:
    if (--OW(o, esth_time) < 0) V.gamemode = id_Credits;
    break;
  }
  DisplaySprite(o);
}

/* Sonic run in from the right, stopped in the middle, then the ending's own Sonic */
static void End_MoveSonic(void) {
  switch (V.sonicend) {
  case 0:
    if ((uint16_t)player->x.w.i >= 320 / 2 - 16) return;
    V.sonicend += 2;
    V.f_lockctrl = 1;
    V.jpadhold2 = btnR, V.jpadpress2 = 0;
    return;
  case 2:
    if ((uint16_t)player->x.w.i < 320 / 2) return;
    V.sonicend += 2;
    V.f_lockctrl = 0;
    V.jpadhold2 = V.jpadpress2 = 0;
    player->inertia = 0;
    V.f_playerctrl = 0x81;
    player->frame = fr_Wait2;
    player->anim = player->prevani = id_Wait;
    player->timeframe = 3;
    return;
  case 4:
    V.sonicend += 2;
    player->x.w.i = 320 / 2;
    player->id = id_EndSonic;
    player->routine = player->routine2 = 0;
    return;
  }
}

static void ending_frame(bool cycle) {
  PauseGame();
  V.vblank_routine = id_VBlank_Ending;
  WaitForVBlank();
  V.framecount++;
  End_MoveSonic();
  ExecuteObjects();
  DeformLayers();
  BuildSprites();
  ObjPosLoad();
  if (cycle) PaletteCycle();
  OscillateNumDo();
  SynchroAnimate();
}

void GM_Ending(void) {
  save_clear_progress(); /* (the game is over: Continue no longer goes back to it) */
  PaletteFadeOut();
  clear_level_ram();
  ClearScreen();
  vdp_backdrop = 0x20;
  V.hblank_line = 223;
  V.air = 30;
  set_zone_act(V.emeralds == 6 ? id_EndZ_good : id_EndZ_bad);
  QuickPLC(plcid_Ending);
  Hud_Base();
  LevelSizeLoad();
  DeformLayers();
  V.scroll_flags[0] |= 4;
  LevelDataLoad();
  LoadTilesFromStart();
  ColIndexLoad();
  ending_flowers = ART(ART_Kos_EndFlowers);
  PalLoad_Fade(palid_Sonic);
  player->id = id_SonicPlayer;
  player->status |= 1;
  V.f_lockctrl = 1;
  V.jpadhold2 = btnL, V.jpadpress2 = 0;
  player->inertia = -0x800;
  OBJ(1)->id = id_HUD;
  ObjPosLoad();
  ExecuteObjects();
  BuildSprites();
  V.rings = 0;
  V.timemin = V.timesec = V.timecent = 0;
  V.lifecount = 0;
  V.shield = V.invinc = V.shoes = V.unused1 = 0;
  V.debuguse = 0;
  V.f_restart = 0;
  V.framecount = 0;
  OscillateNumInit();
  V.f_scorecount = V.f_ringcount = 1;
  V.f_timecount = 0;
  V.generictimer = 1800;
  V.vblank_routine = id_VBlank_Ending;
  WaitForVBlank();
  V.pfade_start = 0, V.pfade_size = 0x3F;
  PaletteFadeIn();
  for (;;) {
    ending_frame(true);
    if (V.gamemode != id_Ending) break;
    if (!V.f_restart) continue;
    /* all the emeralds: a slow fade to white, then the flowers grown */
    V.f_restart = 0;
    V.pfade_start = 0, V.pfade_size = 0x3F;
    V.palchgspeed = 0;
    do {
      ending_frame(false);
      if (--V.palchgspeed < 0) {
        V.palchgspeed = 2;
        WhiteOut_ToWhite();
      }
    } while (!V.f_restart);
    V.f_restart = 0;
    lvllayout[1][0][0] = 0x2E, lvllayout[1][0][1] = 0x2F;
    DrawChunks_FG();
    PalLoad_Fade(palid_Ending);
    PaletteWhiteIn();
  }
  ending_flowers = NULL;
  V.gamemode = id_Credits;
  V.creditsnum = 0;
}

/* ------------------------------------------------------------------ the credits */
/* the next credit's demo: its level, and Labyrinth's from its lamppost */
static void EndingDemoLoad(void) {
  /* (the ninth, after the last demo, is read from past the list: the lamppost's first bytes) */
  static const uint16_t levels[9] = {id_GHZ_act1, id_MZ_act2, id_SYZ_act3, id_LZ_act3, id_SLZ_act3,
                                     id_SBZ_act1, id_SBZ_act2, id_GHZ_act1, 0x0101};
  set_zone_act(levels[V.creditsnum < 9 ? V.creditsnum : 8]);
  if (++V.creditsnum >= 9) return;
  V.f_demo = (int16_t)0x8001;
  V.gamemode = id_Demo;
  V.lives = 3;
  V.rings = 0;
  V.timemin = V.timesec = V.timecent = 0;
  V.score = 0;
  V.lastlamp = 0;
  if (V.creditsnum != 4) return;
  V.lastlamp = V.lastlamp_dup = 1;
  V.lamp_xpos = 0xA00, V.lamp_ypos = 0x62C;
  V.lamp_rings = 13;
  V.lamp_timemin = V.lamp_timesec = V.lamp_timecent = 0;
  V.lamp_dle = 0;
  V.lamp_limitbtm = 0x800;
  V.lamp_scrx = 0x957, V.lamp_scry = 0x5CC;
  V.lamp_bgscrx = 0x4AB, V.lamp_bgscry = 0x3A6;
  V.lamp_bg2scrx = 0, V.lamp_bg2scry = 0x28C;
  V.lamp_bg3scrx = 0, V.lamp_bg3scry = 0;
  V.lamp_wtrpos = 0x308;
  V.lamp_wtrrout = V.lamp_wtrstat = 1;
}

#define tcha_origX 0x38
#define tcha_origY 0x3A
#define tcha_radius 0x3C
#define tcha_juggledir 0x3E
#define eegg_time 0x30

void EndEggman(Obj *o) {
  switch (o->routine) {
  case 0:
    o->routine += 2;
    o->x.w.i = 0x80 + 0xA0;
    obScreenY(o) = 0x80 + 0x74;
    o->map = map_get(MAP_EEgg);
    o->gfx = ArtTile_Try_Again_Eggman;
    o->render = sprite_cam_screen;
    o->priority = 2;
    o->anim = 2;
    if (V.emeralds != 6) {
      /* TRY AGAIN, and the emeralds he has */
      OBJ(3)->id = id_CreditsText;
      V.creditsnum = 9;
      OBJ(32)->id = id_TryChaos;
      o->anim = 0;
    }
    /* fall through */
  case 2: AnimateSprite(o, ani_get(ANI_EEgg)); break;
  case 4: {
    o->routine += 2;
    int16_t d0 = 2;
    if (o->anim & 1) d0 = -2;
    for (int i = 0; i < 6; i++) {
      Obj *a1 = OBJ(32 + i);
      OB(a1, tcha_juggledir) = (uint8_t)d0;
      a1->angle = (uint8_t)(a1->angle + d0 * 8);
    }
    o->frame++;
    OW(o, eegg_time) = 2 * 60 - 8;
  }
    /* fall through */
  case 6:
    if (--OW(o, eegg_time) < 0) {
      o->anim ^= 1;
      o->routine = 2;
    }
    break;
  }
  DisplaySprite(o);
}

void TryChaos(Obj *o) {
  if (o->routine == 0) {
    uint8_t d2 = 0, d3 = 0;
    for (int i = 0; i <= 5 - V.emeralds; i++) {
      Obj *a1 = o + i;
      a1->id = id_TryChaos;
      a1->routine += 2;
      a1->map = map_get(MAP_ECha);
      a1->gfx = ArtTile_Try_Again_Emeralds;
      a1->render = sprite_cam_screen;
      a1->priority = 1;
      a1->x.w.i = 0x80 + 0x84;
      OW(a1, tcha_origX) = 0x80 + 0xA0;
      obScreenY(a1) = 0x80 + 0x6C;
      OW(a1, tcha_origY) = obScreenY(a1);
      OB(a1, tcha_radius) = 0x1C;
      /* the next emerald Sonic does not have */
      for (int k = V.emeralds - 1; k >= 0; k--)
        if (V.emldlist[k] == d2) d2++, k = V.emeralds;
      a1->frame = (uint8_t)(d2 + 1);
      d2++;
      a1->angle = 0x80;
      a1->timeframe = a1->delayani = d3;
      d3 = (uint8_t)(d3 + 10);
    }
  }
  if (OW(o, tcha_juggledir)) {
    if (!o->timeframe || !--o->timeframe) set_anglew(o, (uint16_t)(anglew(o) + OW(o, tcha_juggledir)));
    uint8_t a = o->angle;
    if (a == 0 || a == 0x80) {
      OW(o, tcha_juggledir) = 0;
      o->timeframe = o->delayani;
    }
    int16_t cos, sin = CalcSine(a, &cos);
    int16_t r = OB(o, tcha_radius);
    o->x.w.i = (int16_t)(((int32_t)cos * r >> 8) + OW(o, tcha_origX));
    obScreenY(o) = (int16_t)(((int32_t)sin * r >> 8) + OW(o, tcha_origY));
  }
  DisplaySprite(o);
}

static void TryAgainEnd(void) {
  PaletteFadeOut();
  vdp_backdrop = 0x20;
  V.f_wtr_state = 0;
  ClearScreen();
  memset(objs, 0, sizeof objs);
  QuickPLC(plcid_TryAgain);
  memset(V.palette_fading, 0, sizeof V.palette_fading);
  PalLoad_Fade(palid_Ending);
  V.palette_fading[32] = 0;
  OBJ(2)->id = id_EndEggman;
  ExecuteObjects();
  BuildSprites();
  V.generictimer = 1800;
  PaletteFadeIn();
  for (;;) {
    PauseGame();
    V.vblank_routine = id_VBlank_Title;
    WaitForVBlank();
    ExecuteObjects();
    BuildSprites();
    if ((V.jpadpress1 & (btnStart | btnABC)) || !V.generictimer || V.gamemode != id_Credits) break;
  }
  V.gamemode = id_Sega;
}

void GM_Credits(void) {
  PaletteFadeOut();
  vdp_backdrop = 0x20;
  V.f_wtr_state = 0;
  ClearScreen();
  memset(objs, 0, sizeof objs);
  vdp_load_art(ART_Nem_CreditText, ArtTile_Credits_Font);
  memset(V.palette_fading, 0, sizeof V.palette_fading);
  PalLoad_Fade(palid_Sonic);
  OBJ(2)->id = id_CreditsText;
  ExecuteObjects();
  BuildSprites();
  EndingDemoLoad();
  if (level_plc1() != 0xFF && level_plc1()) AddPLC(level_plc1());
  AddPLC(plcid_Main2);
  V.generictimer = 120;
  PaletteFadeIn();
  do {
    V.vblank_routine = id_VBlank_Title;
    WaitForVBlank();
    RunPLC();
  } while (V.generictimer || plc_pending());
  if (V.creditsnum == 9) TryAgainEnd();
}
