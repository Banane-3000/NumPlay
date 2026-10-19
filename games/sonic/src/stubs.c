/* (being written) */
#include "s1.h"
#include "game.h"

void DebugMode(Obj *o) { (void)o; }
void ResumeMusic(void) { V.air = 30; }
void HUD_Update(void) {}
void Hud_Base(void) {}
void AnimateLevelGfx(void) {}
void LZWaterFeatures(void) {}
void PalCycle_SS(void) {}
void Lamp_LoadInfo(void) {}
void pause_overlay_draw(void) {}
void PauseGame(void) {}
void save_restore_copy(void) {}
void game_save_on_quit(void) {}
extern int host_level, host_act, host_lives;
void game_init(void) {
  V.gamemode = id_Level;
  V.lives = 3;
#ifdef HOST
  if (host_level >= 0) V.zone = (uint8_t)host_level, V.act = (uint8_t)host_act;
#endif
}
void GM_Sega(void) {}
void GM_Title(void) {}
void GM_Special(void) {}
void GM_Continue(void) {}
void GM_Ending(void) {}
void GM_Credits(void) {}
