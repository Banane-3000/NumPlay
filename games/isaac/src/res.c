/* The data.bin sections (tools/pack.py says what each holds). */
#include "res.h"

const uint8_t *is_bin;

static uint32_t rd32(const uint8_t *p) { return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24; }
static const uint8_t *section(int s) { return is_bin + rd32(is_bin + 8 + 4 * s); }
/* entry i of a section that is a table: its count, then each entry's offset from the section's start */
static const uint8_t *entry(int s, uint32_t i) {
  const uint8_t *t = section(s);
  if (i >= rd32(t)) return 0;
  return t + rd32(t + 4 + 4 * i);
}

int sprite_frames(int spr) {
  const uint8_t *t = section(SEC_SPRITES);
  if ((uint32_t)spr >= rd32(t)) return 0;
  const uint16_t *s = (const uint16_t *)(t + 4 + 4 * spr);
  return s[1];
}

bool frame_get(int spr, int i, Frame *f) {
  const uint8_t *t = section(SEC_SPRITES);
  if ((uint32_t)spr >= rd32(t)) return false;
  const uint16_t *s = (const uint16_t *)(t + 4 + 4 * spr);
  if (!s[1]) return false;
  if (i < 0 || i >= s[1]) i = ((i % s[1]) + s[1]) % s[1];
  const uint8_t *p = entry(SEC_FRAMES, s[0] + (uint32_t)i);
  if (!p) return false;
  const uint16_t *h = (const uint16_t *)p;
  f->w = h[0], f->h = h[1], f->ox = (int16_t)h[2], f->oy = (int16_t)h[3];
  int n = h[4];
  f->pal = h + 6;
  f->alpha = (const uint8_t *)(f->pal + n);
  f->rowoff = (const uint16_t *)(f->alpha + ((n + 1) & ~1));
  f->rows = (const uint8_t *)(f->rowoff + f->h);
  return true;
}

bool backdrop_get(int b, Backdrop *bd) {
  const uint8_t *p = entry(SEC_BACKDROPS, (uint32_t)b);
  if (!p) return false;
  const uint16_t *h = (const uint16_t *)p;
  bd->w = h[0], bd->h = h[1];
  bd->pal = h + 4;
  bd->px = (const uint8_t *)(bd->pal + h[2]);
  return true;
}

int font_height(int font) {
  const uint8_t *p = entry(SEC_FONTS, (uint32_t)font);
  return p ? p[0] : 0;
}

bool glyph_get(int font, int c, Glyph *g) {
  const uint8_t *p = entry(SEC_FONTS, (uint32_t)font);
  if (!p) return false;
  int first = p[1], count = p[2];
  if (c >= 'a' && c <= 'z' && p[3]) c -= 32;   /* (a font of capitals only) */
  if (c < first || c >= first + count) return false;
  const uint8_t *e = p + 4 + 8 * (c - first);
  g->adv = e[0], g->w = e[1], g->h = e[2], g->ox = (int8_t)e[3], g->oy = (int8_t)e[4];
  g->bits = p + 4 + 8 * count + (e[6] | e[7] << 8);
  return true;
}

int room_count(void) { return (int)rd32(section(SEC_ROOMS)); }
const RoomTemplate *room_get(int i) {
  if (i < 0 || i >= room_count()) return 0;
  return (const RoomTemplate *)(section(SEC_ROOMS) + 4 + sizeof(RoomTemplate) * (uint32_t)i);
}
