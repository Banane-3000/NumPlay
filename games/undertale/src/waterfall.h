/* Undertale: Waterfall (wf1.c: from Snowdin to the bridge Undyne cuts), its monsters (wfmon.c). */
#ifndef UT_WATERFALL_H
#define UT_WATERFALL_H
#include "event.h"
void wf1_setup(int room, int from);
int wf1_event(Ev *c);     /* -1: not one of its */
void wf1_update(void);
bool wf_seed_enters(Obj *o, int x, int y);   /* a bridge seed pushed into the water */
void wf_seed_moved(Obj *o);
void wf_battle_done(int form);
/* wf2.c: past the bridge */
void wf2_setup(int room, int from);
int wf2_event(Ev *c);     /* -1: not one of its */
void wf_bird_setup(int room);   /* the small bird, on its side of the gap */
#endif
