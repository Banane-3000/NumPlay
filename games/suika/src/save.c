/* SPDX-License-Identifier: GPL-3.0-only */
#include "suika.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

static void put16(uint8_t *p, uint32_t v) { p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); }
static unsigned get16(const uint8_t *p) { return p[0] | (unsigned)p[1] << 8; }
static void put32(uint8_t *p, uint32_t v) { put16(p, v); put16(p + 2, v >> 16); }
static uint32_t get32(const uint8_t *p) { return get16(p) | (uint32_t)get16(p + 2) << 16; }
static uint32_t checksum(const uint8_t *p, size_t n) {
  uint32_t h = 2166136261u;
  while (n--) h = (h ^ *p++) * 16777619u;
  return h;
}
void sk_encode(const sk_game *g, uint8_t out[SK_SAVE_BYTES]) {
  memset(out, 0, SK_SAVE_BYTES);
  memcpy(out, "SKA2", 4);
  put32(out + 4, g->score);
  put32(out + 8, g->best);
  put32(out + 12, g->rng);
  put32(out + 16, g->merges);
  put16(out + 20, (int)(g->aim * 64));
  put16(out + 22, g->danger);
  out[24] = g->cooldown;
  out[25] = g->current;
  out[26] = g->next;
  out[31] = g->highest;
  out[32] = g->over;
  out[33] = g->started;
  unsigned n = 0;
  for (unsigned i = 0; i < SK_CAPACITY; ++i) {
    const sk_fruit *b = &g->fruit[i];
    if (!b->kind) continue;
    uint8_t *p = out + 48 + n++ * 11;
    p[0] = b->kind;
    put16(p + 1, (int)(b->x * 64));
    put16(p + 3, (int)(b->y * 64));
    put16(p + 5, (int)(b->vx * 256));
    put16(p + 7, (int)(b->vy * 256));
    put16(p + 9, b->age);
  }
  out[45] = (uint8_t)n;
  put32(out + SK_SAVE_BYTES - 4, checksum(out, SK_SAVE_BYTES - 4));
}
bool sk_decode(sk_game *g, const uint8_t *p, size_t n) {
  if (!p || n != SK_SAVE_BYTES || (memcmp(p, "SKA1", 4) && memcmp(p, "SKA2", 4)) ||
      get32(p + n - 4) != checksum(p, n - 4)) return false;
  bool legacy=p[3]=='1';
  static const uint8_t old_radius[]={0,6,8,10,12,14,17,20,24,28,33,38};
  unsigned width=legacy?194:SK_WIDTH,height=legacy?178:SK_HEIGHT;
  if (!get32(p+12)||get32(p+4)>get32(p+8)||get16(p+20)>width*64||
      get16(p+22)>180||p[24]>24||p[25]<1||p[25]>(legacy?4:5)||
      p[26]<1||p[26]>(legacy?4:5)||p[31]<1||p[31]>SK_RANKS||
      p[32]>1||p[33]>1||p[45]>SK_CAPACITY)return false;
  if(legacy) {
    if(p[29]>4||p[30]>1||p[34]>8||p[43]>99||p[44]>75)return false;
    for(unsigned i=27;i<29;++i)if(p[i]<1||p[i]>4)return false;
    unsigned counts[5]={0};
    for(unsigned i=35;i<43;++i) {
      if(p[i]<1||p[i]>4)return false;
      ++counts[p[i]];
    }
    if(counts[1]!=3||counts[2]!=2||counts[3]!=2||counts[4]!=1)return false;
  }
  /* Validate the complete payload before touching the caller's live state. */
  for (unsigned i = 0; i < p[45]; ++i) {
    const uint8_t *b = p + 48 + i * 11;
    if (!b[0] || b[0] > SK_RANKS) return false;
    float x = (int16_t)get16(b + 1) / 64.0f, y = (int16_t)get16(b + 3) / 64.0f;
    unsigned r=legacy?old_radius[b[0]]:sk_radius[b[0]];
    if (x < r - 0.05f || x > width - r + 0.05f ||
        y < -48 || y > height - r + 0.05f ||
        abs((int16_t)get16(b + 5)) > 2048 || abs((int16_t)get16(b + 7)) > 2048) return false;
  }
  memset(g, 0, sizeof *g);
  g->score = get32(p + 4);
  g->best = get32(p + 8);
  g->rng = get32(p + 12);
  g->merges = get32(p + 16);
  g->danger = get16(p + 22);
  g->cooldown = p[24];
  g->current = p[25];
  g->next = p[26];
  g->highest = p[31];
  g->over = p[32];
  g->started = p[33];
  g->count = p[45];
  sk_aim(g, get16(p + 20) / 64.0f * (legacy?SK_WIDTH/194.0f:1));
  for (unsigned i = 0; i < g->count; ++i) {
    const uint8_t *b = p + 48 + i * 11;
    g->fruit[i] = (sk_fruit){(int16_t)get16(b + 1) / 64.0f, (int16_t)get16(b + 3) / 64.0f,
                           (int16_t)get16(b + 5) / 256.0f, (int16_t)get16(b + 7) / 256.0f,
                           (uint16_t)get16(b + 9), b[0], 0};
    if(legacy) {
      sk_fruit *f=&g->fruit[i];float r=sk_radius[f->kind];
      f->x*=SK_WIDTH/194.0f;f->y*=SK_HEIGHT/178.0f;
      f->vx*=SK_WIDTH/194.0f;f->vy*=SK_HEIGHT/178.0f;
      if(f->x<r)f->x=r;
      if(f->x>SK_WIDTH-r)f->x=SK_WIDTH-r;
      if(f->y>SK_HEIGHT-r)f->y=SK_HEIGHT-r;
    }
  }
  return true;
}
