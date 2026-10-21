/* Cookie Clicker: drawing, a band of the screen at a time.
 *
 * gfx_render(x, y, w, h, draw) has draw() paint the rectangle a band of GFX_BAND rows at a
 * time, then sends each band to the screen: everything below is drawn in the screen's
 * coordinates, clipped to the band (g_x0..g_x1, g_y0..g_y1). Colours are RGB565; opacities
 * 0 to 256. Pictures are data.bin's (IM_*), drawn as they are, scaled, rotated or tiled. */
#ifndef CC_GFX_H
#define CC_GFX_H
#include <stdbool.h>
#include <stdint.h>
#include "plat.h"

#define GFX_BAND 16
#define RGB(r, g, b) ((uint16_t)(((r) >> 3) << 11 | ((g) >> 2) << 5 | (b) >> 3))
#define HEX(c) RGB(((c) >> 16) & 255, ((c) >> 8) & 255, (c) & 255)
#define WHITE 0xFFFF
#define BLACK 0x0000

void gfx_set_band_memory(uint16_t *band);   /* SCREEN_W * GFX_BAND colours */
void gfx_render(int x, int y, int w, int h, void (*draw)(void));
extern int g_x0, g_y0, g_x1, g_y1;          /* the band being drawn (clip), in screen coordinates */
static inline bool gfx_visible(int x, int y, int w, int h) { return x < g_x1 && y < g_y1 && x + w > g_x0 && y + h > g_y0; }
/* narrows the clip (within the band) until gfx_unclip */
void gfx_clip(int x, int y, int w, int h);
void gfx_unclip(void);

/* blending: normal, or added light (the game's "lighter") */
enum { BLEND_NORMAL, BLEND_ADD };
void gfx_blend(int mode);

uint16_t gfx_mix(uint16_t a, uint16_t b, int t);       /* a to b, t from 0 to 256 */
void gfx_fill(int x, int y, int w, int h, uint16_t c);
void gfx_fill_alpha(int x, int y, int w, int h, uint16_t c, int alpha);
void gfx_hgrad(int x, int y, int w, int h, uint16_t c, int alpha0, int alpha1);   /* opacity left to right */
void gfx_vgrad(int x, int y, int w, int h, uint16_t c, int alpha0, int alpha1);   /* top to bottom */
void gfx_rect(int x, int y, int w, int h, uint16_t c, int alpha);                  /* an outline */
void gfx_round_rect(int x, int y, int w, int h, int r, uint16_t c, int alpha);
void gfx_line(float x0, float y0, float x1, float y1, uint16_t c, int alpha);
void gfx_disc(float cx, float cy, float r, uint16_t c, int alpha);

int gfx_image_w(int im);
int gfx_image_h(int im);
void gfx_image(int im, int x, int y, int alpha);
void gfx_image_part(int im, int sx, int sy, int sw, int sh, int x, int y, int alpha);
/* scaled to w x h at (x, y), smoothly; or fast, from its nearest pixels (smooth overlays) */
void gfx_image_scaled(int im, float x, float y, float w, float h, int alpha);
void gfx_image_scaled_fast(int im, float x, float y, float w, float h, int alpha);
/* centred on (cx, cy), w x h, turned by angle (radians, clockwise on screen) */
void gfx_image_rotated(int im, float cx, float cy, float w, float h, float angle, int alpha);
/* the same, sampled every other pixel both ways (soft pictures, drawn large: the flares) */
void gfx_image_rotated_soft(int im, float cx, float cy, float w, float h, float angle, int alpha);
/* the rectangle tiled with the picture at tw x th, the tiles shifted by (ox, oy) */
void gfx_pattern(int im, int x, int y, int w, int h, float tw, float th, float ox, float oy, int alpha);
/* an icon of the game's sheet (its index: y * ICON_COLS + x), 24 pixels, or scaled to size */
void gfx_icon(int index, int x, int y, int alpha);
void gfx_icon_scaled(int index, float x, float y, float size, int alpha);
/* the picture as a dark silhouette (the store's buildings not yet seen) */
void gfx_image_dark(int im, int x, int y, int alpha);

/* text: y is the baseline; returns the x after it */
int gfx_text(int font, int x, int y, const char *s, uint16_t c, int alpha);
int gfx_text_width(int font, const char *s);
int gfx_text_n_width(int font, const char *s, int n);
int gfx_text_center(int font, int cx, int y, const char *s, uint16_t c, int alpha);
int gfx_text_right(int font, int rx, int y, const char *s, uint16_t c, int alpha);
/* outlined in black, as the game writes its titles and numbers */
void gfx_text_outline(int font, int x, int y, const char *s, uint16_t c, int alpha);
void gfx_text_outline_center(int font, int cx, int y, const char *s, uint16_t c, int alpha);
int gfx_font_ascent(int font);
int gfx_font_line(int font);

/* a paragraph of the game's markup (0x01 bold, 0x02 plain, 0x03 italic, 0x04 back, 0x05 a line across,
 * \n), wrapped to w pixels from (x, y: the top); returns its height. With draw false, only measures. */
typedef struct {
  int font, bold, italic;   /* the fonts for plain, bold and italic text */
  uint16_t color, bold_color, italic_color;
  int line;                 /* line height */
  int align;                /* 0 left, 1 centred */
} ParaStyle;
int gfx_para(const ParaStyle *st, int x, int y, int w, const char *s, bool draw);
#endif
