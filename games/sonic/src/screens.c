/* The SEGA screen (its light sweeping across the logo), and the title screen: SONIC TEAM PRESENTS,
 * then Sonic rising out of the emblem over Green Hill's scrolling background, PRESS START BUTTON;
 * left alone it plays a demo of Green Hill, Marble, Spring Yard or the special stage in turn.
 * Start (or OK) plays from the beginning, or offers to continue a game left with Home. */
#include "s1.h"
#include "game.h"
#include "objs.h"
#include "ui.h"

/* ------------------------------------------------------------------ SEGA */
/* the light across the logo, then the logo fading in; false when done */
static bool PalCycle_Sega(void) {
  if (!(V.pcyc_time & 0xFF)) {
    uint16_t *a1 = &V.palette[16];
    const uint16_t *a0 = DATAW(D_PAL_SEGA1);
    int d1 = N_PAL_SEGA1 - 1;
    int16_t d0 = (int16_t)V.pcyc_num;
    while (d0 < 0) a0++, d1--, d0 = (int16_t)(d0 + 2);
    for (; d1 >= 0; d1--) {
      if (!(d0 & 0x1E)) d0 = (int16_t)(d0 + 2);
      if ((uint16_t)d0 < 0x60) a1[d0 >> 1] = *a0++;
      d0 = (int16_t)(d0 + 2);
    }
    d0 = (int16_t)(V.pcyc_num + 2);
    if (!(d0 & 0x1E)) d0 = (int16_t)(d0 + 2);
    if (d0 >= 0x60 + 4) {
      V.pcyc_time = 0x401;
      d0 = -6 * 2;
    }
    V.pcyc_num = (uint16_t)d0;
    return true;
  }
  uint8_t t = (uint8_t)(V.pcyc_time >> 8);
  if ((int8_t)--t >= 0) {
    V.pcyc_time = (uint16_t)(t << 8 | (V.pcyc_time & 0xFF));
    return true;
  }
  V.pcyc_time = (uint16_t)(4 << 8 | (V.pcyc_time & 0xFF));
  int16_t d0 = (int16_t)(V.pcyc_num + 6 * 2);
  if ((uint16_t)d0 >= 6 * 2 * 4) return false;
  V.pcyc_num = (uint16_t)d0;
  const uint16_t *a0 = DATAW(D_PAL_SEGA2) + d0 / 2;
  for (int i = 0; i < 5; i++) V.palette[2 + i] = *a0++;
  uint16_t *a1 = &V.palette[16];
  d0 = 0;
  for (int i = 0; i < 0x30 - 3; i++) {
    if (!(d0 & 0x1E)) d0 = (int16_t)(d0 + 2);
    a1[d0 >> 1] = *a0;
    d0 = (int16_t)(d0 + 2);
  }
  return true;
}

void GM_Sega(void) {
  ClearPLC();
  PaletteFadeOut();
  V.f_wtr_state = 0;
  vdp_display = false;
  ClearScreen();
  vdp_backdrop = 0;
  vdp_load_art(ART_Nem_SegaLogo, ArtTile_Sega_Tiles);
  const uint16_t *eni = DATAW(D_ENI_SEGALOGO);
  vdp_fill_plane(plane_b, 8, 10, 24, 8, eni, ArtTile_Sega_Tiles);
  vdp_fill_plane(plane_a, 0, 0, 40, 28, eni + 24 * 8, ArtTile_Sega_Tiles);
  PalLoad(palid_SegaBG);
  V.pcyc_num = (uint16_t)-0xA;
  V.pcyc_time = 0;
  V.pal_buffer[9] = V.pal_buffer[8] = 0;
  vdp_display = true;
  do {
    V.vblank_routine = id_VBlank_Sega;
    WaitForVBlank();
  } while (PalCycle_Sega());
  V.vblank_routine = id_VBlank_SegaPCM;
  WaitForVBlank();
  V.generictimer = 30;
  do {
    V.vblank_routine = id_VBlank_Sega;
    WaitForVBlank();
  } while (V.generictimer && !(V.jpadpress1 & btnStart));
  V.gamemode = id_Title;
}

/* ------------------------------------------------------------------ the title's objects */
void TitleSonic(Obj *o) {
  switch (o->routine) {
  case 0:
    o->routine += 2;
    o->x.w.i = 0x80 + 0x70;
    obScreenY(o) = 0x80 + 0x5E;
    o->map = map_get(MAP_TSon);
    o->gfx = ArtTile_Title_Sonic | Tile_Pal2;
    o->priority = 1;
    o->delayani = 30 - 1;
    AnimateSprite(o, ani_get(ANI_TSon));
    /* fall through */
  case 2:
    if ((int8_t)--o->delayani < 0) {
      o->routine += 2;
      DisplaySprite(o);
    }
    return;
  case 4:
    obScreenY(o) = (int16_t)(obScreenY(o) - 8);
    if (obScreenY(o) == 0x80 + 0x16) o->routine += 2;
    DisplaySprite(o);
    return;
  default:
    AnimateSprite(o, ani_get(ANI_TSon));
    DisplaySprite(o);
    return;
  }
}

void PSBTM(Obj *o) {
  switch (o->routine) {
  case 0:
    o->routine += 2;
    o->x.w.i = 0x80 + 0x50;
    obScreenY(o) = 0x80 + 0xB0;
    o->map = map_get(MAP_PSB);
    o->gfx = ArtTile_Title_Foreground;
    if (o->frame < 2) goto press;
    o->routine += 2;
    if (o->frame == 3) {
      o->gfx = ArtTile_Title_Trademark | Tile_Pal2;
      o->x.w.i = 0x80 + 0xF0;
      obScreenY(o) = 0x80 + 0x78;
    }
    break;
  case 2:
  press:
    AnimateSprite(o, ani_get(ANI_PSBTM));
    break;
  }
  DisplaySprite(o);
}

void CreditsText(Obj *o) {
  if (o->routine == 0) {
    o->routine += 2;
    o->x.w.i = 320 / 2 + 0x80;
    obScreenY(o) = 224 / 2 + 0x80;
    o->map = map_get(MAP_Cred);
    o->gfx = ArtTile_Credits_Font;
    o->frame = (uint8_t)V.creditsnum;
    o->render = sprite_cam_screen;
    o->priority = 0;
    if (V.gamemode == id_Title) {
      o->gfx = ArtTile_Sonic_Team_Font;
      o->frame = 0xA;
    }
  }
  DisplaySprite(o);
}

/* ------------------------------------------------------------------ the title screen */
static void new_game(void) {
  V.gamemode = id_Level;
  V.lives = 3;
  V.rings = 0;
  V.timemin = V.timesec = V.timecent = 0;
  V.score = 0;
  V.lastspecial = 0;
  V.emeralds = 0;
  memset(V.emldlist, 0, sizeof V.emldlist);
  V.continues = 0;
  V.scorelife = 5000;
}

static bool demo_tick(void) {
  player->x.w.i = (int16_t)(player->x.w.i + 2);
  if ((uint16_t)player->x.w.i < 0x1C00) return false;
  V.gamemode = id_Sega;
  return true;
}

/* Continue or New game, while the title goes on; then the game starts */
static void choose_game(void) {
  int sel = 0;
  for (;;) {
    continue_menu_draw(sel);
    V.vblank_routine = id_VBlank_Title;
    WaitForVBlank();
    ExecuteObjects();
    DeformLayers();
    BuildSprites();
    PalCycle_Title();
    uint8_t p = V.jpadpress1;
    if (p & (btnUp | btnDn)) sel ^= 1;
    if (p & (btnABC | btnStart)) break;
  }
  ui_clear();
  new_game();
  if (sel == 0) save_load_progress();
}

static void play(void) {
  if (save_exists()) choose_game();
  else new_game();
}

void GM_Title(void) {
  static const uint16_t demo_levels[4] = {id_GHZ_act1, id_MZ_act1, id_SYZ_act1, 0x600};
  ClearPLC();
  PaletteFadeOut();
  V.f_wtr_state = 0;
  ClearScreen();
  vdp_backdrop = 0x20;
  memset(objs, 0, sizeof(Obj) * NOBJ);
  vdp_load_art(ART_Nem_JapNames, ArtTile_Title_Japanese_Text);
  vdp_load_art(ART_Nem_CreditText, ArtTile_Sonic_Team_Font);
  vdp_fill_plane(plane_a, 0, 0, 40, 28, DATAW(D_ENI_JAPNAMES), ArtTile_Title_Japanese_Text);
  memset(V.palette_fading, 0, sizeof V.palette_fading);
  PalLoad_Fade(palid_Sonic);
  OBJ(2)->id = id_CreditsText;
  ExecuteObjects();
  BuildSprites();
  PaletteFadeIn();
  /* (SONIC TEAM PRESENTS stays while the original unpacks the title's pictures) */
  for (int i = 0; i < 100; i++) {
    V.vblank_routine = id_VBlank_PaletteFade;
    WaitForVBlank();
  }
  vdp_load_art(ART_Nem_TitleFg, ArtTile_Title_Foreground);
  vdp_load_art(ART_Nem_TitleSonic, ArtTile_Title_Sonic);
  vdp_load_art(ART_Nem_TitleTM, ArtTile_Title_Trademark);
  vdp_load_art(ART_Art_Text, ArtTile_Level_Select_Font);
  V.lastlamp = 0;
  V.debuguse = 0;
  V.f_demo = 0;
  V.zone = id_GHZ, V.act = 0;
  V.pcyc_time = 0;
  LevelSizeLoad();
  DeformLayers();
  level_blocks_load();
  LevelLayoutLoad();
  PaletteFadeOut();
  ClearScreen();
  DrawChunks_BG();
  vdp_fill_plane(plane_a, 3, 4, 34, 22, DATAW(D_ENI_TITLE), ArtTile_Level);
  vdp_load_art(ART_Nem_GHZ_1st, ArtTile_Level);
  PalLoad_Fade(palid_Title);
  V.f_debugmode = 0;
  V.generictimer = 376;
  memset(OBJ(2), 0, sizeof(Obj) / 2);
  OBJ(1)->id = id_TitleSonic;
  OBJ(2)->id = id_PSBTM;
  OBJ(3)->id = id_PSBTM;
  OBJ(3)->frame = 3;
  OBJ(4)->id = id_PSBTM;
  OBJ(4)->frame = 2;
  ExecuteObjects();
  DeformLayers();
  BuildSprites();
  NewPLC(plcid_Main);
  V.title_dcount = V.title_ccount = 0;
  vdp_display = true;
  PaletteFadeIn();
  for (;;) {
    V.vblank_routine = id_VBlank_Title;
    WaitForVBlank();
    ExecuteObjects();
    DeformLayers();
    BuildSprites();
    PalCycle_Title();
    RunPLC();
    if (demo_tick()) return;
    if (!V.generictimer) break;
    if (V.jpadpress1 & (btnStart | btnABC)) {
      play();
      return;
    }
  }
  /* the demo, after half a second */
  V.generictimer = 30;
  for (;;) {
    V.vblank_routine = id_VBlank_Title;
    WaitForVBlank();
    DeformLayers();
    PaletteCycle();
    RunPLC();
    if (demo_tick()) return;
    if (V.jpadpress1 & (btnStart | btnABC)) {
      play();
      return;
    }
    if (!V.generictimer) break;
  }
  uint16_t za = demo_levels[V.demonum & 3];
  set_zone_act(za);
  if (++V.demonum >= 4) V.demonum = 0;
  V.f_demo = 1;
  V.gamemode = id_Demo;
  if (za == 0x600) {
    V.gamemode = id_Special;
    set_zone_act(0);
    V.lastspecial = 0;
  }
  V.lives = 3;
  V.rings = 0;
  V.timemin = V.timesec = V.timecent = 0;
  V.score = 0;
  V.scorelife = 5000;
}

void game_init(void) {
#ifdef HOST
  /* (tests: straight to an act, or to special stage n with --level 7.n) */
  extern int host_level, host_act, host_lives;
  if (host_level >= 0) {
    new_game();
    AddPLC(plcid_Main);
    if (host_lives > 0) V.lives = (uint8_t)host_lives;
    if (host_level == 7) {
      V.gamemode = id_Special;
      V.lastspecial = (uint8_t)host_act;
      return;
    }
    V.zone = (uint8_t)host_level, V.act = (uint8_t)host_act;
    return;
  }
#endif
  V.gamemode = id_Sega;
  if (!save_seen_keys()) keys_screen();
}
