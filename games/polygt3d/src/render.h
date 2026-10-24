/* The 3D picture (render.c): the road, the roadside and the cars, flat-shaded triangles drawn with a depth buffer
 * a strip of the screen at a time. */
#ifndef PG_RENDER_H
#define PG_RENDER_H
#include "game.h"

typedef struct {
  bool on;
  int32_t s, x;                     /* where the ghost is */
  int32_t ang;                      /* where it points, from the road */
  uint8_t model;
} rd_ghost;

/* a frame of the race, between the step before (a) and the last one (b), alpha/256 of the way */
void rd_race(const pg_track *t, const pg_race *a, const pg_race *b, int alpha, int view, const rd_ghost *g,
             uint32_t clock_ms);
/* a car on a turntable, for the menus; zone: the scenery behind it */
void rd_showroom(int vehicle, int32_t angle, int zone, uint32_t clock_ms);
/* draws what was set up into the strip gfx_y0..gfx_y1 of gfx_buf */
void rd_draw(void);
/* whether the last frame ran out of room for triangles, and how many it had */
extern int rd_dropped, rd_tris;
uint16_t rd_rgb(const uint8_t c[3]);
extern const uint8_t rd_paint[8][3];  /* the rivals' paints */
#endif
