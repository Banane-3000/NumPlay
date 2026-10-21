/* The Binding of Isaac: drawing.
 *
 * A frame is a draw list, rasterised band by band (the calculator has no
 * frame buffer) and pushed to the screen, painted in the order given. The
 * room shows whole, as in the game: its walls and floor 360 x 240 pixels
 * (the game's own, 20 for its 26), 20 of them cut off on each side. */
#ifndef IS_GFX_H
#define IS_GFX_H
#include <stdbool.h>
#include <stdint.h>
#include "plat.h"

static inline uint16_t rgb565(uint32_t rgb) {   /* 0xRRGGBB */
  return (uint16_t)(((rgb >> 8) & 0xF800) | ((rgb >> 5) & 0x07E0) | ((rgb >> 3) & 0x001F));
}
#define C_BLACK 0x0000
#define C_WHITE 0xFFFF
#define C_RED 0xF800

enum { GFX_FLIPX = 1, GFX_FLIPY = 2 };

#define GFX_BAND 16           /* screen rows rasterised at a time */
#define GFX_MAX_ITEMS 560     /* commands in a frame (more are dropped) */
#define GFX_TEXT_POOL 2048    /* bytes of text in a frame */

void gfx_set_band_memory(uint16_t *px);   /* SCREEN_W * GFX_BAND pixels (main's stack on the calculator) */
void gfx_begin(void);
void gfx_end(void);
/* later commands draw only inside this rectangle (gfx_clip_all(): all of the screen) */
void gfx_clip(int x, int y, int w, int h);
void gfx_clip_all(void);

/* alpha goes from 0 (nothing) to 32 (opaque) */
void gfx_backdrop(int b);                                         /* a room's walls and floor, all of the screen */
void gfx_rect(int x, int y, int w, int h, uint16_t color, int alpha);
void gfx_ellipse(int cx, int cy, int rx, int ry, uint16_t color, int alpha);
/* frame i of sprite s, drawn with its origin at (x, y) */
void gfx_sprite(int s, int i, int x, int y, int flags, int alpha);
/* the same, mixed towards a colour by amount (0 to 32: a flash, a silhouette) */
void gfx_sprite_tint(int s, int i, int x, int y, int flags, uint16_t color, int amount, int alpha);
/* the same, scaled (8.8 fixed point, 256 = as it is) around its origin */
void gfx_sprite_scaled(int s, int i, int x, int y, int sx, int sy, int flags, uint16_t color, int amount, int alpha);
/* text, its top-left at (x, y), each pixel `scale` pixels; an outline of black around it; returns its width */
int gfx_text(int font, int x, int y, const char *s, uint16_t color, int scale, bool outline);
int gfx_text_width(int font, const char *s, int scale);
/* text centred on x */
void gfx_text_c(int font, int x, int y, const char *s, uint16_t color, int scale, bool outline);
/* the darkness of the Curse of the Darkness: all but a circle around (x, y) dimmed */
void gfx_dark(int x, int y, int r, int alpha);

uint16_t gfx_mix(uint16_t f, uint16_t b, int a);   /* f over b, a from 0 (b) to 32 (f) */
extern int gfx_dropped;
#endif
