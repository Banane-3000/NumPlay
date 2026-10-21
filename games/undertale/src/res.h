/* Undertale: data.bin (tools/pack.py), read where it lies in flash.
 *
 * Its sections (SEC_* in data.h): sprites and their frames, the areas' tiles,
 * rooms (a tile grid and a collision map each), fonts and strings. Frames and
 * tiles are packed in LZMA blocks (blk.h): their pointers hold until the next
 * frame_get or tile_get. */
#ifndef UT_RES_H
#define UT_RES_H
#include <stdbool.h>
#include <stdint.h>
#include "data.h"

extern const uint8_t *ut_bin;
static inline uint16_t rd16(const uint8_t *p) { return (uint16_t)(p[0] | p[1] << 8); }
static inline int16_t rds16(const uint8_t *p) { return (int16_t)rd16(p); }
static inline uint32_t rd32(const uint8_t *p) { return p[0] | p[1] << 8 | p[2] << 16 | (uint32_t)p[3] << 24; }
static inline const uint8_t *section(int s) { return ut_bin + rd32(ut_bin + 8 + 4 * s); }

/* a sprite's frame: its size and origin, palette and rows */
typedef struct {
  uint16_t w, h;
  int16_t ox, oy;
  uint8_t bpp, ncol;
  const uint8_t *pal;       /* ncol RGB565 colours (unaligned: read with rd16) */
  const uint8_t *rows;      /* u16 offsets of each row, from `data` */
  const uint8_t *data;
} Frame;
bool frame_get(uint16_t spr, int i, Frame *f);
int spr_frames(uint16_t spr);
int spr_w(uint16_t spr);
int spr_h(uint16_t spr);

/* a 20 x 20 tile: bpp 0 (one colour), 1, 2, 4 or 8 */
typedef struct {
  uint8_t bpp, ncol;
  const uint8_t *pal;       /* ncol RGB565 colours */
  const uint8_t *bits;      /* 400 pixels, bpp bits each, lowest bits first */
} Tile;
void tile_get(uint16_t id, Tile *t);

/* a room's facts */
typedef struct {
  int16_t x, y, w, h;       /* its rectangle on the area's map */
  int16_t gx, gy;           /* the tile grid's top-left on the map */
  uint16_t gcols, grows;
  const uint8_t *grid;      /* gcols * grows tile ids (u16), 0xFFFF none */
  const uint8_t *coll;      /* cw * ch bits, row by row ((cw + 7) / 8 bytes a row): 1 walkable */
  uint16_t cw, ch;          /* 4-pixel cells over the rectangle */
  uint8_t area, flags;
  uint16_t name;            /* string: what SAVE shows */
  const uint8_t *ice;       /* the same bits for ice (Frisk slides), NULL none */
  int16_t vx, vy, vw, vh;   /* what the camera keeps to: the game's room */
} RoomInfo;
#define ROOM_ICE 1
void room_info(int room, RoomInfo *r);

/* a door of a room: walking into its rectangle takes Frisk to another room */
typedef struct {
  int16_t x, y, w, h;
  uint16_t room;
  int16_t tx, ty;           /* where Frisk lands; DOOR_KEEP: where he is */
  uint16_t dir;
} Door;
#define DOOR_KEEP (-32768)
int room_doors(int room, Door *out, int max);

const char *str(uint16_t id);
#endif
