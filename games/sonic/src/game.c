/* The level game mode, as the original runs it: the title card while the art loads, the level's
 * size, layout and palette, Sonic, the HUD and the objects, a fade in, then the main loop (objects,
 * camera and background, sprites, the object manager, colour cycles) until the level restarts or
 * the game mode changes; demos replay their recorded buttons. And pausing. */
#include "s1.h"
#include "game.h"
#include "plat.h"

static const uint32_t demo_data[8] = {D_DEMO_INTRO_GHZ, D_DEMO_INTRO_GHZ, D_DEMO_INTRO_MZ, D_DEMO_INTRO_MZ,
                                      D_DEMO_INTRO_SYZ, D_DEMO_INTRO_SYZ, D_DEMO_INTRO_SPECIAL_STAGE,
                                      D_DEMO_INTRO_SPECIAL_STAGE};
static const uint32_t demo_end_data[8] = {D_DEMO_ENDING_GHZ1, D_DEMO_ENDING_MZ, D_DEMO_ENDING_SYZ, D_DEMO_ENDING_LZ,
                                          D_DEMO_ENDING_SLZ, D_DEMO_ENDING_SBZ1, D_DEMO_ENDING_SBZ2,
                                          D_DEMO_ENDING_GHZ2};

static const uint8_t *demo_ptr(void) {
  if (V.f_demo < 0) return DATA(demo_end_data[(V.creditsnum - 1) & 7]);
  int z = V.zone;
  if (V.gamemode == id_Special) z = 6;
  return DATA(demo_data[z & 7]);
}

void MoveSonicInDemo(void) {
  if (!V.f_demo) return;
  if ((V.jpadhold1 & btnStart) && V.f_demo > 0) V.gamemode = id_Title;
  const uint8_t *a1 = demo_ptr() + V.btnpushtime1;
  uint8_t d0 = a1[0];
  V.jpadhold1 = d0;
  V.jpadpress1 = d0; /* (REV01: d2 is 0, so every held button reads as pressed) */
  uint8_t t = (uint8_t)V.btnpushtime2;
  V.btnpushtime2 = (uint8_t)(t - 1);
  if (!t) {
    V.btnpushtime2 = a1[3];
    V.btnpushtime1 += 2;
  }
}

static void clear_level_ram(void) {
  memset(objs, 0, sizeof objs);
  /* v_misc_variables, v_levelvariables, v_timingandscreenvariables */
  V.vblank_routine = 0;
  V.pcyc_num = V.pcyc_time = 0;
  V.f_pause = 0;
  V.f_hblank_pal = 0;
  V.waterpos1 = V.waterpos2 = V.waterpos3 = 0;
  V.f_water = V.wtr_routine = V.f_wtr_state = V.f_doupdatesinhblank = 0;
  memset(V.cam, 0, sizeof V.cam);
  V.limitleft1 = V.limitright1 = V.limittop1 = V.limitbtm1 = 0;
  V.limitleft2 = V.limitright2 = V.limittop2 = V.limitbtm2 = 0;
  V.scrshiftx = V.scrshifty = 0;
  V.lookshift = 0;
  V.dle_routine = 0;
  V.f_nobgscroll = 0;
  V.fg_xblock = V.fg_yblock = V.bg1_xblock = V.bg1_yblock = V.bg2_xblock = V.bg2_yblock = V.bg3_xblock = V.bg3_yblock = 0;
  memset(V.scroll_flags, 0, sizeof V.scroll_flags);
  V.f_bgscrollvert = 0;
  V.sonframenum = 0, V.f_sonframechg = 0;
  V.anglebuffer = V.anglebuffer2 = 0;
  V.opl_routine = 0;
  V.opl_screen = 0;
  V.ssangle = V.ssrotate = 0;
  V.btnpushtime1 = V.btnpushtime2 = 0;
  V.palchgspeed = 0;
  V.obj31ypos = 0;
  V.bossstatus = 0;
  V.trackpos = 0;
  V.f_lockscreen = 0;
  memset(V.lani, 0, sizeof V.lani);
  V.gfxbigring = 0;
  V.f_conveyrev = 0;
  memset(V.obj63, 0, sizeof V.obj63);
  V.f_wtunnelmode = V.f_playerctrl = V.f_wtunneldisallow = V.f_slidemode = 0;
  V.obj6B = V.f_lockctrl = V.f_bigring = V.f_obj56 = 0;
  V.itembonus = V.timebonus = V.ringbonus = 0;
  V.f_endactbonus = V.sonicend = 0;
  V.lz_deform = 0;
  memset(V.f_switch, 0, sizeof V.f_switch);
  V.oscillate = 0;
  memset(V.osc, 0, sizeof V.osc);
  memset(V.ani_time, 0, sizeof V.ani_time);
  memset(V.ani_frame, 0, sizeof V.ani_frame);
  V.ani3_buf = 0;
  memset(V.cam_dup, 0, sizeof V.cam_dup);
  memset(V.scroll_flags_dup, 0, sizeof V.scroll_flags_dup);
  memset(&bgscroll, 0, sizeof bgscroll);
  memset(spritequeue, 0, sizeof spritequeue);
  palette_cycles_reset();
}

static const uint16_t water_height[4] = {0xB8, 0x328, 0x900, 0x228};

void GM_Level(void) {
  V.gamemode |= 0x80;
  ClearPLC();
  PaletteFadeOut();
  if (V.f_demo >= 0) {
    vdp_load_art(ART_Nem_TitleCard, ArtTile_Title_Card);
    AddPLC(level_plc1());
    AddPLC(plcid_Main2);
  }
  clear_level_ram();
  ClearScreen();
  vdp_backdrop = 0x20;
  V.hblank_line = 223;
  if (V.zone == id_LZ) {
    V.waterpos1 = V.waterpos2 = V.waterpos3 = (int16_t)water_height[V.act & 3];
    V.wtr_routine = 0;
    V.f_wtr_state = 0;
    V.f_water = 1;
  }
  V.air = 30;
  PalLoad(palid_Sonic);
  if (V.zone == id_LZ) {
    PalLoad_Fade_Water(V.act == 3 ? palid_SBZ3SonWat : palid_LZSonWater);
    if (V.lastlamp) V.f_wtr_state = V.lamp_wtrstat;
  }
  if (V.f_demo >= 0) {
    OBJ(2)->id = id_TitleCard;
    do {
      V.vblank_routine = id_VBlank_TitleCards;
      WaitForVBlank();
      ExecuteObjects();
      BuildSprites();
      RunPLC();
    } while (OBJ(4)->x.w.i != OW(OBJ(4), 0x30) || plc_pending());
    Hud_Base();
  }
  PalLoad_Fade(palid_Sonic);
  LevelSizeLoad();
  DeformLayers();
  V.scroll_flags[0] |= 4;
  LevelDataLoad();
  LoadTilesFromStart();
  ColIndexLoad();
  LZWaterFeatures();
  player->id = id_SonicPlayer;
  if (V.f_demo >= 0) OBJ(1)->id = id_HUD;
  if (V.f_debugcheat && (V.jpadhold1 & btnA)) V.f_debugmode = 1;
  V.jpadhold2 = V.jpadpress2 = 0;
  V.jpadhold1 = V.jpadpress1 = 0;
  if (V.zone == id_LZ) {
    OBJ(30)->id = id_WaterSurface, OBJ(30)->x.w.i = 0x60;
    OBJ(31)->id = id_WaterSurface, OBJ(31)->x.w.i = 0x120;
  }
  ObjPosLoad();
  ExecuteObjects();
  BuildSprites();
  if (!V.lastlamp) {
    V.rings = 0;
    V.timemin = V.timesec = V.timecent = 0;
    V.lifecount = 0;
  }
  V.f_timeover = 0;
  V.shield = V.invinc = V.shoes = V.unused1 = 0;
  V.debuguse = 0;
  V.f_restart = 0;
  V.framecount = 0;
  OscillateNumInit();
  V.f_scorecount = V.f_ringcount = V.f_timecount = 1;
  V.btnpushtime1 = 0;
  const uint8_t *demo = demo_ptr();
  V.btnpushtime2 = (uint16_t)(uint8_t)(demo[1] - 1);
  V.generictimer = 1800;
  if (V.f_demo < 0) V.generictimer = V.creditsnum == 4 ? 510 : 540;
  if (V.zone == id_LZ) PalLoad_Water(V.act == 3 ? palid_SBZ3Water : palid_LZWater);
  for (int i = 0; i < 4; i++) {
    V.vblank_routine = id_VBlank_Levels;
    WaitForVBlank();
  }
  V.pfade_start = 0x20, V.pfade_size = 0x2F;
  PalFadeIn_Alt();
  if (V.f_demo >= 0) {
    OBJ(2)->routine += 2;
    OBJ(3)->routine += 4;
    OBJ(4)->routine += 4;
    OBJ(5)->routine += 4;
  } else {
    AddPLC(plcid_Explode);
    AddPLC(plcid_GHZAnimals + V.zone);
  }
  V.gamemode &= 0x7F;
  if (!V.f_demo) save_progress(); /* (each act kept as it starts) */
  for (;;) {
    PauseGame();
    V.vblank_routine = id_VBlank_Levels;
    WaitForVBlank();
    V.framecount++;
    MoveSonicInDemo();
    LZWaterFeatures();
    ExecuteObjects();
    if (V.f_restart) return; /* (REV01: GM_Level again, from the main loop) */
    if (V.debuguse || player->routine < 6) DeformLayers();
    BuildSprites();
    ObjPosLoad();
    PaletteCycle();
    RunPLC();
    OscillateNumDo();
    SynchroAnimate();
    SignpostArtLoad();
    if (V.gamemode == id_Demo) {
      if (V.f_restart || !V.generictimer) goto end_demo;
      if (V.gamemode == id_Demo) continue;
      V.gamemode = id_Sega;
      return;
    }
    if (V.gamemode != id_Level) return;
  }
end_demo:
  if (V.gamemode == id_Demo) {
    V.gamemode = id_Sega;
    if (V.f_demo < 0) V.gamemode = id_Credits;
  }
  V.generictimer = 60;
  V.pfade_start = 0, V.pfade_size = 0x3F;
  V.palchgspeed = 0;
  while (V.generictimer) {
    V.vblank_routine = id_VBlank_Levels;
    WaitForVBlank();
    MoveSonicInDemo();
    ExecuteObjects();
    BuildSprites();
    ObjPosLoad();
    if ((int16_t)--V.palchgspeed < 0) {
      V.palchgspeed = 2;
      FadeOut_ToBlack();
    }
  }
}
