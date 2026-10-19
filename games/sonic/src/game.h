/* Sonic the Hedgehog: the parts that call each other across files. */
#ifndef S1_GAME_H
#define S1_GAME_H
#include "s1.h"

/* game modes (game.c, screens.c, special.c, ending.c) */
void game_init(void);
void GM_Sega(void);
void GM_Title(void);
void GM_Level(void);
void GM_Special(void);
void GM_Continue(void);
void GM_Ending(void);
void GM_Credits(void);
void PauseGame(void);
void pause_overlay_draw(void);

/* saves (save.c) */
void save_restore_copy(void);
void game_save_on_quit(void);
void save_progress(void);
bool save_load_progress(void);
bool save_exists(void);
void save_clear_progress(void);
void save_peek(int *zone, int *act, int *lives);
bool save_seen_keys(void);
void save_set_seen_keys(void);

/* Sonic (sonic.c, react.c) */
void SonicPlayer(Obj *o);
void Sonic_ResetOnFloor(Obj *o);
void Sonic_AnglePos(Obj *o);
void Sonic_LoadGfx(Obj *o);
int ReactToItem(Obj *o);
int HurtSonic(Obj *o, Obj *by);
void KillSonic(Obj *o, Obj *by);
void DebugMode(Obj *o);
extern uint16_t tracksonic[64][2];

/* solid objects and platforms (solid.c) */
int SolidObject(Obj *o, int16_t d1, int16_t d2, int16_t d3, int16_t d4);
int Solid_ChkCollision(Obj *o, int16_t d1, int16_t d2);
extern int16_t solid_d0;
int SolidObject_NoRenderChk(Obj *o, int16_t d1, int16_t d2, int16_t d3, int16_t d4);
int SolidObject_Heightmap(Obj *o, int16_t d1, int16_t d2, const uint8_t *heights);
void PlatformObject(Obj *o, int16_t d1);
void PlatformObject_CustomHeight(Obj *o, int16_t d1, int16_t d3);
void Plat_NoXCheck(Obj *o);
void Plat_NoXCheck_AltY(Obj *o, int16_t y);
void Plat_NoCheck_ext(Obj *o);
void SlopeObject(Obj *o, int16_t d1, const uint8_t *heights);
int ExitPlatform(Obj *o, int16_t d1);
int ExitPlatform2(Obj *o, int16_t d1, int16_t d2);
void SlopeObject_AssumeStoodOn(Obj *o, int16_t d1, const uint8_t *heights, int16_t d2);
void MvSonicOnPtfm(Obj *o, int16_t d2, int16_t d3);
void MvSonicOnPtfm2(Obj *o, int16_t d2);

/* the HUD, water, level art (hud.c, water.c, anim.c) */
void HUD_Update(void);
void Hud_Base(void);
void ResumeMusic(void);
void PalCycle_SS(void);
void Lamp_LoadInfo(void);
int level_plc1(void);
void palette_cycles_reset(void);
bool obj_far_x(int16_t x);
extern const int8_t *const drown_wobble_ptr;
#endif
