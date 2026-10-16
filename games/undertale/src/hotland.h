/* Undertale: Hotland (ht1.c, ht2.c, ht3.c: its three floors; resort.c: the MTT Resort), its monsters
 * (htmon.c), Mettaton's shows (htshows.c). */
#ifndef UT_HOTLAND_H
#define UT_HOTLAND_H
#include "event.h"
void ht1_setup(int room, int from);
int ht1_event(Ev *c);     /* -1: not one of its */
void ht1_update(void);    /* the belts, the lasers */
void ht1_draw(void);      /* under everything: the lasers, the turning vents' arrows */
void ht2_setup(int room, int from);
int ht2_event(Ev *c);
void ht2_update(void);    /* the belt puzzle's switches, the news report's bombs and clock */
void ht2_draw(void);      /* under everything: force fields, the switches, the vent puzzle's arrows */
void ht2_draw_top(void);  /* over everything: the news report's clock */
void ht3_setup(int room, int from);
int ht3_event(Ev *c);
void ht3_update(void);    /* the webs' hold, the tile puzzle, its flames and clock */
void ht3_draw(void);      /* under everything: the flames, the turned-off force fields */
void ht3_draw_top(void);  /* over everything: the tile puzzle's clock */
int ht3_speed(int x, int y);
void res_setup(int room, int from);
int res_event(Ev *c);
#endif
