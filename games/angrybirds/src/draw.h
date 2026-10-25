/* Angry Birds: drawing (draw.c), a strip of 16 rows at a time with the launcher's gfx (its fonts, panels and text)
 * and the game's pictures (16 colours, turned and scaled). */
#ifndef AB_DRAW_H
#define AB_DRAW_H
#include <stdbool.h>
#include <stdint.h>
#include "gfx.h"

typedef struct {
  float cx, cy, zoom;   /* the world point at the screen's centre, pixels a meter */
} View;

#define COL_PANEL HEX(0x3B2414)
#define COL_PANEL_LIGHT HEX(0x5A3A22)
#define COL_CREAM HEX(0xFFF1C4)
#define COL_FOCUS HEX(0xFFD43B)
#define COL_INK HEX(0x2B1A0E)
#define COL_MUTED HEX(0xC9B79A)
#define COL_WHITE 0xFFFF

/* a picture, its top-left at (x, y) (scale: screen pixels for one of its own) */
void draw_pic(int pic, int x, int y);
void draw_pic_scaled(int pic, int x, int y, int w, int h);
/* a picture with its pivot at screen (sx, sy), scaled and turned (sine and cosine of the angle) */
void draw_pic_at(int pic, float sx, float sy, float scale, float sn, float cs);
/* the level as the view shows it (no interface) */
void draw_world(const View *v);
/* world to screen */
float draw_sx(const View *v, float x);
float draw_sy(const View *v, float y);
void draw_line(int x0, int y0, int x1, int y1, int t, color_t c);
#endif
