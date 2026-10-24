/* The road, the roadside and the cars, seen from behind your car (view.c). */
#ifndef TR_VIEW_H
#define TR_VIEW_H
#include "game.h"

#define VIEW_HZ 100                 /* the horizon's row */
#define VIEW_MAP 64                 /* points in a circuit's outline */

/* the colours for a circuit at a time of day (call before view_frame when either changes) */
void view_setup(const tr_track *t, int mood, int rain);
/* a frame of the race: between the step before (a) and the last one (b), alpha/256 of the way */
void view_frame(const tr_track *t, const tr_race *a, const tr_race *b, int alpha);
void view_draw(void);               /* into the strip gfx_y0..gfx_y1 */
/* a car on a menu: centred on x, standing on `bottom`, `width` pixels wide; angle 0 left, 1 ahead, 2 right */
void view_car(int x, int bottom, int width, int paint, int style, int angle);
/* the circuit's outline (view_setup), and where a distance is on it */
extern int16_t view_map_x[VIEW_MAP], view_map_y[VIEW_MAP];
void view_map_at(const tr_track *t, int32_t z, int *x, int *y);
uint16_t view_paint(int paint);     /* a paint's colour, for swatches */
#endif
