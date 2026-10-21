/* Hill Climb Racing: drawing.
 *
 * A frame is a list of commands, rasterised a band of rows at a time (the calculator has no frame buffer for the
 * whole screen) and pushed. Commands are in screen pixels and painted in the order given. Pictures turn and scale
 * around their pivot and are sampled with bilinear filtering, so the vehicles roll and tilt smoothly. */
#ifndef HC_GFX_H
#define HC_GFX_H
#include <stdbool.h>
#include <stdint.h>
#include "plat.h"
#include "res.h"

static inline uint16_t rgb565(uint32_t rgb) {   /* 0xRRGGBB */
  return (uint16_t)(((rgb >> 8) & 0xF800) | ((rgb >> 5) & 0x07E0) | ((rgb >> 3) & 0x001F));
}
#define C_BLACK 0x0000
#define C_WHITE 0xFFFF

#define GFX_BAND 24           /* rows rasterised at a time */
#define GFX_MAX_CMDS 360
#define GFX_TEXT_POOL 1024

/* the band: SCREEN_W * GFX_BAND pixels */
void gfx_init(uint16_t *band);
void gfx_begin(void);
void gfx_end(void);

/* alpha: 0 (none) to 255 */
void gfx_fill(int x, int y, int w, int h, uint16_t c, int alpha);
void gfx_rrect(int x, int y, int w, int h, int r, uint16_t c, int alpha);   /* rounded corners */
void gfx_vgrad(int x, int y, int w, int h, uint32_t top, uint32_t bottom);  /* 0xRRGGBB, top to bottom */
/* each row of the screen its colour (the sky), from rows[0] at y0 */
void gfx_rows(const uint16_t *rows, int y0, int y1);
/* picture id, its pivot at (x, y), `scale` screen pixels for one of its own, turned by `angle` radians
 * (counterclockwise on the screen) */
void gfx_image(int id, float x, float y, float scale, float angle, int alpha);
/* the same, its top-left at (x, y), w x h screen pixels */
void gfx_image_box(int id, int x, int y, int w, int h, int alpha);
/* text with a dark outline: y is the baseline; align 0 left, 1 centred, 2 right of x; its width */
int gfx_text(int font, int x, int y, const char *s, uint16_t color, uint16_t outline, int align);
int gfx_text_width(int font, const char *s);
void gfx_circle(float x, float y, float r, uint16_t c, int alpha);
void gfx_ring(float x, float y, float r, float t, uint16_t c, int alpha);   /* a circle's edge t wide */
/* a tube from (x0, y0) to (x1, y1), r thick each side, round ends */
void gfx_tube(float x0, float y0, float x1, float y1, float r, uint16_t c, int alpha);

/* The ground of the frame, filled column by column by the game (see game.c) before gfx_ground_draw() */
typedef struct {
  int16_t floor[SCREEN_W];    /* the surface's line, screen rows x 8; past the bottom: none */
  int16_t ceil[SCREEN_W];     /* the ceiling's line x 8; above the top: none */
  uint8_t su[SCREEN_W];       /* the surface strip's column (0-255) */
  uint8_t gu[SCREEN_W];       /* the ground's column (0-127) */
  int32_t gv0, gdv;           /* the ground's row at the screen's top and down each row (16.16) */
  int16_t above, below;       /* rows of the surface strip above and below its line */
  uint8_t surf, ground, csurf, cground;   /* pictures: the strip and the ground, under it and over the ceiling */
  bool ceiling;
  uint16_t shade;             /* the ground darkens towards this colour, deeper (0: not) */
} Ground;
extern Ground gfx_ground;
void gfx_ground_draw(void);

/* screen points from a picture's own: where its pixel (u, v) lands */
void gfx_image_point(int id, float x, float y, float scale, float angle, float u, float v, float *sx, float *sy);
#endif
