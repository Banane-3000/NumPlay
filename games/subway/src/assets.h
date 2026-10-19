/* Subway Surfers: the fonts and the logo (assets.c, made by tools/assets.py). */
#ifndef SS_ASSETS_H
#define SS_ASSETS_H
#include <stdint.h>

typedef struct {
  uint8_t w, h;
  int8_t x, y;      /* the top-left pixel from the pen (y from the baseline) */
  uint8_t advance;
  uint16_t offset;  /* into data: 4-bit coverage, two pixels a byte, row by row */
} Glyph;

typedef struct {
  uint8_t first, count, ascent, descent;
  const Glyph *glyphs;
  const uint8_t *data;
  const Glyph *outline[2];    /* the letters grown by 1 and 2 pixels (and 1 lower): their outline */
  const uint8_t *outline_data[2];
} Font;

extern const Font font_big, font_mid, font_small;

/* the logo: each row is runs of (count, byte), the byte's high 4 bits its alpha, the low ones its colour */
extern const uint16_t logo_w, logo_h;
extern const uint16_t logo_pal[16];
extern const uint16_t logo_rows[];
extern const uint8_t logo_runs[];
#endif
