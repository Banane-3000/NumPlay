/* The tables of data.bin (res.h): "ABD1", their count, then where each one starts and its size. */
#include "res.h"

static uint32_t rd32(const uint8_t *p) { return p[0] | p[1] << 8 | p[2] << 16 | (uint32_t)p[3] << 24; }

const void *res_section(int id, uint32_t *size) {
  const uint8_t *e = ab_data + 8 + 8 * id;
  if (size) *size = rd32(e + 4);
  return ab_data + rd32(e);
}

const Material *res_material(int i) { return (const Material *)res_section(D_MATERIALS, 0) + i; }
const Type *res_type(int i) { return (const Type *)res_section(D_TYPES, 0) + i; }
const Shape *res_shape(int i) { return (const Shape *)res_section(D_SHAPES, 0) + i; }
const LevelInfo *res_level(int i) { return (const LevelInfo *)res_section(D_LEVELS, 0) + i; }
const TypeArt *res_type_art(int type) { return (const TypeArt *)res_section(D_TYPE_SPRITES, 0) + type; }
const uint8_t *res_level_stream(int level) {
  return (const uint8_t *)res_section(D_LEVEL_STREAMS, 0) + res_level(level)->off;
}

static const Pair *pair(int a, int b) {
  const Pair *p = (const Pair *)res_section(D_PAIRS, 0);
  int lo = 0, hi = PAIR_COUNT - 1;
  while (lo <= hi) {
    int mid = (lo + hi) / 2, key = p[mid].a << 8 | p[mid].b, want = a << 8 | b;
    if (key == want) return &p[mid];
    if (key < want) lo = mid + 1;
    else hi = mid - 1;
  }
  return 0;
}
float res_damage(int a, int b) {
  const Pair *p = pair(a, b);
  return p ? p->damage : 1.0f;
}
float res_velocity(int a, int b) {
  const Pair *p = pair(a, b);
  return p ? p->velocity : 1.0f;
}

int res_episode_first(int e) { return ((const uint16_t *)res_section(D_EPISODES, 0))[2 * e]; }
int res_episode_count(int e) { return ((const uint16_t *)res_section(D_EPISODES, 0))[2 * e + 1]; }
int res_episode_of(int level) { return res_level(level)->episode; }
int res_story_before(int level) {
  uint8_t s = ((const uint8_t *)res_section(D_STORY_BEFORE, 0))[level];
  return s == 0xFF ? -1 : s;
}
int res_story_after(int level) {
  uint8_t s = ((const uint8_t *)res_section(D_STORY_AFTER, 0))[level];
  return s == 0xFF ? -1 : s;
}
int res_golden(int i) { return ((const uint8_t *)res_section(D_GOLDEN, 0))[i]; }
int res_unlock(int level) {
  uint32_t n;
  const uint16_t *u = (const uint16_t *)res_section(D_UNLOCKS, &n);
  for (uint32_t k = 0; k + 1 < n / 2; k += 2)
    if (u[k] == level) return u[k + 1];
  return -1;
}

int res_episode_pic(int e) { return ((const uint16_t *)res_section(D_EPISODE_PICS, 0))[e]; }
int res_story_pic(int s) { return ((const uint16_t *)res_section(D_STORY_PICS, 0))[s]; }
const Pic *res_pic(int i) { return (const Pic *)res_section(D_PICTURES, 0) + i; }
const uint16_t *res_pic_palette(const Pic *p) {
  return (const uint16_t *)(const void *)((const uint8_t *)res_section(D_PIXELS, 0) + p->off);
}
const uint8_t *res_pic_pixels(const Pic *p) { return (const uint8_t *)res_section(D_PIXELS, 0) + p->off + 32; }
const Background *res_background(int theme) { return (const Background *)res_section(D_BACKGROUNDS, 0) + theme; }
