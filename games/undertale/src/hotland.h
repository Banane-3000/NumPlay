/* Undertale: Hotland (ht1.c: its first floor), its monsters (htmon.c), Mettaton's shows (htshows.c). */
#ifndef UT_HOTLAND_H
#define UT_HOTLAND_H
#include "event.h"
void ht1_setup(int room, int from);
int ht1_event(Ev *c);     /* -1: not one of its */
void ht1_update(void);    /* the belts, the lasers */
void ht1_draw(void);      /* under everything: the lasers, the turning vents' arrows */
#endif
