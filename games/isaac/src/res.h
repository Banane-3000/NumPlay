/* The Binding of Isaac: the pictures, fonts and rooms in data.bin (tools/pack.py). */
#ifndef IS_RES_H
#define IS_RES_H
#include <stdbool.h>
#include <stdint.h>
#include "data.h"

extern const uint8_t *is_bin;   /* data.bin, wherever it lives */

/* a picture: palette indices in runs, row by row; each colour has its opacity (0 to 32) */
typedef struct {
  int w, h, ox, oy;          /* size, and the point it is drawn at (its feet, mostly) */
  const uint16_t *pal;
  const uint8_t *alpha;
  const uint16_t *rowoff;    /* each row's runs, from rows */
  const uint8_t *rows;       /* a row: its number of runs, then (skip, length, length indices) each */
} Frame;
bool frame_get(int spr, int i, Frame *f);   /* frame i of sprite spr (wrapping round its frames) */
int sprite_frames(int spr);

/* a room's walls and floor: a quarter of it (the rest is it mirrored), one byte a pixel */
typedef struct {
  int w, h;
  const uint16_t *pal;
  const uint8_t *px;
} Backdrop;
bool backdrop_get(int b, Backdrop *bd);

/* a glyph of a font: its bits, row by row, highest bit first, rows of whole bytes */
typedef struct {
  int adv, w, h, ox, oy;
  const uint8_t *bits;
} Glyph;
int font_height(int font);
bool glyph_get(int font, int c, Glyph *g);

/* a room's layout (tools/rooms.py): its kind, the floors it may be on, how often, and its 13 x 7 cells */
typedef struct {
  uint8_t kind, chapters, weight, flags;
  uint8_t cell[7][13];
} RoomTemplate;
int room_count(void);
const RoomTemplate *room_get(int i);
#endif
