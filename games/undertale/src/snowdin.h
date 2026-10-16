/* Undertale: Snowdin, the forest (snowdin1.c, snowdin2.c) and the town (snowtown.c), its monsters (snowmon.c),
 * Papyrus (papyrus.c), the shops (shop.c). */
#ifndef UT_SNOWDIN_H
#define UT_SNOWDIN_H
#include "event.h"
void snow1_setup(int room, int from);
int snow1_event(Ev *c);   /* -1: not one of its */
void snow1_update(void);
void snow1_draw(void);    /* what its rooms draw over the floor (footprints, the tiles) */
void snow2_setup(int room, int from);
int snow2_event(Ev *c);
void snow2_update(void);
void snow2_draw(void);
void town_setup(int room, int from);
int town_event(Ev *c);
void snow_doggo_stick(void);
void snow_battle_done(int form);   /* snowmon.c: who was killed, who met */   /* Doggo was spared with the stick (his talk after) */
/* Sans and Papyrus, their feet at (x, y) */
Obj *obj_sans(int x, int y, int dir);
Obj *obj_papyrus(int x, int y, int dir);
/* the camera moves to (x, y) (its top-left), sp pixels a frame; true once there (it stays until world_enter or
 * cam_free) */
bool cam_pan(int x, int y, int sp);
void cam_free(void);
#endif
