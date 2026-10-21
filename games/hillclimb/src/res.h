/* Hill Climb Racing: the pictures and fonts in data.bin (tools/pack.py). */
#ifndef HC_RES_H
#define HC_RES_H
#include <stdint.h>
#include "data.h"

typedef struct {
  uint16_t w, h, ncolors, flags;   /* flags 1: opaque */
  uint16_t ppm4;                   /* its pixels a meter x 4 (0: a picture of the menus) */
  int16_t px4, py4;                /* its pivot x 4 */
  uint16_t pad;
} Img;
enum { IMG_OPAQUE = 1 };

typedef struct {
  uint8_t w, h;
  int8_t x, y;        /* the bitmap's top-left from the pen, y from the baseline */
  uint8_t adv, pad[3];
  uint32_t off;
} Glyph;
typedef struct {
  uint8_t first, count, height, ascent;
} Font;

extern const uint8_t hc_data[];
const Img *img(int id);
static inline const uint32_t *img_pal(const Img *i) { return (const uint32_t *)(const void *)(i + 1); }
static inline const uint16_t *img_pal565(const Img *i) { return (const uint16_t *)(const void *)(img_pal(i) + i->ncolors); }
static inline const uint8_t *img_px(const Img *i) {
  return (const uint8_t *)(img_pal(i) + i->ncolors) + (i->flags & IMG_OPAQUE ? 2 * i->ncolors : 0);
}
const Font *font(int id);
static inline const Glyph *font_glyphs(const Font *f) { return (const Glyph *)(const void *)(f + 1); }
static inline const uint8_t *font_bits(const Font *f) { return (const uint8_t *)(font_glyphs(f) + f->count); }
#endif
