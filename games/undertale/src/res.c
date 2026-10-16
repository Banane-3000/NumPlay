/* data.bin: sections, sprites, tiles, rooms and strings (see res.h). */
#include <stddef.h>
#include "blk.h"
#include "res.h"

const uint8_t *ut_bin;

int spr_frames(uint16_t spr) {
  const uint8_t *s = section(SEC_SPRITES);
  if (spr >= rd32(s)) return 0;
  return rd16(s + 4 + 4 * spr + 2);
}

/* a frame's or tile's bytes: in its block, unpacked (blk.h) */
static const uint8_t *in_block(int sec, int k) {
  const uint8_t *t = section(sec) + 4 + 4 * k;
  const uint8_t *b = blk_get(rd16(t));
  return b ? b + rd16(t + 2) : NULL;
}

bool frame_get(uint16_t spr, int i, Frame *f) {
  const uint8_t *s = section(SEC_SPRITES);
  if (spr >= rd32(s)) return false;
  int first = rd16(s + 4 + 4 * spr), n = rd16(s + 4 + 4 * spr + 2);
  if (n <= 0) return false;
  i %= n;
  if (i < 0) i += n;
  const uint8_t *p = in_block(SEC_FRAMES, first + i);
  if (!p) return false;
  f->w = rd16(p), f->h = rd16(p + 2);
  f->ox = rds16(p + 4), f->oy = rds16(p + 6);
  f->bpp = p[8], f->ncol = p[9];
  f->pal = p + 10;
  uint32_t o = 10 + 2u * f->ncol;
  f->rows = p + o;
  f->data = p + o + 2u * f->h;
  return true;
}

int spr_w(uint16_t spr) {
  Frame f;
  return frame_get(spr, 0, &f) ? f.w : 0;
}
int spr_h(uint16_t spr) {
  Frame f;
  return frame_get(spr, 0, &f) ? f.h : 0;
}

void tile_get(uint16_t id, Tile *t) {
  static const uint8_t black[4] = {0, 1, 0, 0};
  const uint8_t *p = in_block(SEC_TILES, id);
  if (!p) p = black;
  t->bpp = p[0], t->ncol = p[1];
  t->pal = p + 2;
  t->bits = p + 2 + 2 * t->ncol;
}

void room_info(int room, RoomInfo *r) {
  const uint8_t *s = section(SEC_ROOMS);
  const uint8_t *p = s + 4 + 32 * room;
  r->x = rds16(p), r->y = rds16(p + 2), r->w = rds16(p + 4), r->h = rds16(p + 6);
  r->gx = rds16(p + 8), r->gy = rds16(p + 10);
  r->gcols = rd16(p + 12), r->grows = rd16(p + 14);
  r->grid = section(SEC_GRID) + rd32(p + 16);
  r->coll = section(SEC_COLL) + rd32(p + 20);
  r->cw = rd16(p + 24), r->ch = rd16(p + 26);
  r->area = p[28], r->flags = p[29];
  r->name = rd16(p + 30);
  r->ice = (r->flags & ROOM_ICE) ? r->coll + ((r->cw + 7) >> 3) * r->ch : NULL;
}

const char *str(uint16_t id) {
  const uint8_t *s = section(SEC_STRINGS);
  if (id >= rd32(s)) return "";
  return (const char *)s + rd32(s + 4 + 4 * id);
}

int room_doors(int room, Door *out, int max) {
  const uint8_t *s = section(SEC_DOORS);
  int first = rd16(s + 4 * room), n = rd16(s + 4 * room + 2);
  const uint8_t *d = s + 4 * NUM_ROOMS + 16 * first;
  if (n > max) n = max;
  for (int i = 0; i < n; i++, d += 16) {
    out[i].x = rds16(d), out[i].y = rds16(d + 2), out[i].w = rds16(d + 4), out[i].h = rds16(d + 6);
    out[i].room = rd16(d + 8), out[i].tx = rds16(d + 10), out[i].ty = rds16(d + 12), out[i].dir = rd16(d + 14);
  }
  return n;
}
