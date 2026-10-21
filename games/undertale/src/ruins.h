/* Undertale: the Ruins' rooms and events (ruins1.c, ruins2.c, ruins3.c). */
#ifndef UT_RUINS_H
#define UT_RUINS_H
#include "event.h"
void ruins1_setup(int room, int from);
int ruins1_event(Ev *c);   /* -1: not one of its */
void ruins2_setup(int room, int from);
int ruins2_event(Ev *c);
void ruins2_update(void);
void ruins3_setup(int room, int from);
int ruins3_event(Ev *c);
void ruins3_draw(void);   /* the mirror */
#endif
