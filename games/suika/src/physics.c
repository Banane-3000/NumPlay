/* SPDX-License-Identifier: GPL-3.0-only
 * Suika for NumPlay: bounded storage, fixed-step circle physics, no heap.
 */
#include "suika.h"
#include <math.h>
#include <string.h>

const uint8_t sk_radius[] = {0, 5, 7, 9, 11, 14, 17, 20, 25, 28, 31, 40};
const char *const sk_names[] = {"", "CHERRY", "STRAWBERRY", "GRAPE", "DEKOPON", "PERSIMMON",
                               "APPLE", "PEAR", "PEACH", "PINEAPPLE", "MELON", "WATERMELON"};
static float clamp(float x, float lo, float hi) { return x < lo ? lo : x > hi ? hi : x; }
static void add_score(sk_game *g, uint32_t n) {
  g->score = UINT32_MAX - g->score < n ? UINT32_MAX : g->score + n;
  if (g->score > g->best) g->best = g->score;
}
static uint32_t random32(sk_game *g) {
  uint32_t x = g->rng;
  x ^= x << 13;
  x ^= x >> 17;
  x ^= x << 5;
  return g->rng = x;
}
/* Independent uniform draws; repeats are intentional, with no bag or pity. */
static uint8_t take(sk_game *g) {return (uint8_t)(1+random32(g)%5);}
static void advance(sk_game *g) {
  g->current=g->next;g->next=take(g);sk_aim(g,g->aim);
}
void sk_new(sk_game *g, uint32_t seed, uint32_t best) {
  memset(g, 0, sizeof *g);
  g->rng = seed ? seed : 0xA341316Cu;
  g->best = best;
  g->current = take(g);
  g->next = take(g);
  g->aim = SK_WIDTH * 0.5f;
  g->highest = 1;
}
void sk_aim(sk_game *g, float x) {
  float r = sk_radius[g->current];
  g->aim = clamp(x, r + 0.5f, SK_WIDTH - r - 0.5f);
}
bool sk_drop(sk_game *g) {
  if (g->over || g->cooldown) return false;
  float r = sk_radius[g->current];
  /* No interpenetrating spawn when a stack reaches the dropper. Allow the
   * pile to settle/merge and let the danger timer decide whether it is over. */
  for (unsigned i = 0; i < SK_CAPACITY; ++i) {
    const sk_fruit *b = &g->fruit[i];
    if (!b->kind) continue;
    float dx = b->x - g->aim, dy = b->y - SK_DROP_Y, sum = sk_radius[b->kind] + r;
    if (dx * dx + dy * dy < sum * sum) { g->notice = 60; return false; }
  }
  for (unsigned i = 0; i < SK_CAPACITY; ++i) {
    sk_fruit *b = &g->fruit[i];
    if (b->kind) continue;
    *b = (sk_fruit){g->aim, SK_DROP_Y, 0, 0, 0, g->current, 0};
    if (b->kind > g->highest) g->highest = b->kind;
    ++g->count;
    g->started = 1;
    g->cooldown = 24;
    g->notice = 0;
    advance(g);
    return true;
  }
  g->notice = 60;
  return false;
}
static void walls(sk_fruit *b) {
  float r = sk_radius[b->kind];
  if (b->x < r) {
    b->x = r;
    if (b->vx < 0) b->vx *= -0.18f;
  }
  if (b->x > SK_WIDTH - r) {
    b->x = SK_WIDTH - r;
    if (b->vx > 0) b->vx *= -0.18f;
  }
  if (b->y > SK_HEIGHT - r) {
    b->y = SK_HEIGHT - r;
    if (b->vy > 0) b->vy = b->vy < 0.45f ? 0 : -b->vy * 0.12f;
    b->vx *= 0.85f;
  }
  /* A finite ceiling bound protects fixed-size saves even in dense stacks. */
  if (b->y < -48) b->y = -48, b->vy = 0;
  b->vx = clamp(b->vx, -8, 8);
  b->vy = clamp(b->vy, -8, 8);
}
static void merge(sk_game *g, sk_fruit *a, sk_fruit *b) {
  unsigned kind = a->kind;
  float x = (a->x + b->x) * 0.5f, y = (a->y + b->y) * 0.5f;
  float vx = (a->vx + b->vx) * 0.5f, vy = (a->vy + b->vy) * 0.5f;
  /* Preserve mature stack age. Give growth a short grace period only. */
  unsigned age = a->age > b->age ? a->age : b->age;
  if (age > 90) age = 90;
  memset(b, 0, sizeof *b);
  --g->count;
  add_score(g, kind * (kind + 1) / 2);
  if (g->merges < UINT32_MAX) ++g->merges;
  if (kind == SK_RANKS) {
    memset(a, 0, sizeof *a);
    --g->count;
  } else {
    *a = (sk_fruit){x, y, vx, vy, (uint16_t)age, (uint8_t)(kind + 1), 16};
    if (a->kind > g->highest) g->highest = a->kind;
    walls(a);
  }
}
void sk_step(sk_game *g) {
  if (g->over) return;
  if (g->cooldown) --g->cooldown;
  if (g->notice) --g->notice;
  uint64_t merged = 0;
  for (unsigned i = 0; i < SK_CAPACITY; ++i) {
    sk_fruit *b = &g->fruit[i];
    if (!b->kind) continue;
    if (b->age < 65535) ++b->age;
    if (b->pop) --b->pop;
    b->vx *= 0.994f;
    b->vy = clamp(b->vy + 0.115f, -8, 8);
  }
  /* Two substeps prevent small fruit passing through a fast contact. Six
   * total solver passes, mass proportional to area, damped contact friction. */
  for (unsigned sub = 0; sub < 2; ++sub) {
    for (unsigned i = 0; i < SK_CAPACITY; ++i) {
      sk_fruit *b = &g->fruit[i];
      if (!b->kind) continue;
      b->x += b->vx * 0.5f;
      b->y += b->vy * 0.5f;
      walls(b);
    }
    for (unsigned pass = 0; pass < 3; ++pass) {
      for (unsigned i = 0; i < SK_CAPACITY; ++i) {
        sk_fruit *a = &g->fruit[i];
        if (!a->kind) continue;
        for (unsigned j = i + 1; j < SK_CAPACITY; ++j) {
          sk_fruit *b = &g->fruit[j];
          if (!b->kind) continue;
          float dx = b->x - a->x, dy = b->y - a->y;
          float ra = sk_radius[a->kind], rb = sk_radius[b->kind], sum = ra + rb;
          if (fabsf(dx) >= sum || fabsf(dy) >= sum) continue;
          float d2 = dx * dx + dy * dy;
          if (d2 >= sum * sum) continue;
          uint64_t mask = (UINT64_C(1) << i) | (UINT64_C(1) << j);
          if (a->kind == b->kind && !(merged & mask)) {
            merged |= mask;
            merge(g, a, b);
            if (!a->kind) break;
            continue;
          }
          /* Coincident centres need a real separation axis, not dx=1 with
           * an unrelated zero distance. The tie-break is deterministic. */
          float dist;
          if (d2 < 0.0001f) dx = 1, dy = 0, dist = 1;
          else dist = sqrtf(d2);
          float nx = dx / dist, ny = dy / dist;
          float wa = rb * rb / (ra * ra + rb * rb), wb = 1 - wa;
          float overlap = sum - dist + 0.002f;
          a->x -= nx * overlap * wa;
          a->y -= ny * overlap * wa;
          b->x += nx * overlap * wb;
          b->y += ny * overlap * wb;
          float relative = (b->vx - a->vx) * nx + (b->vy - a->vy) * ny;
          if (relative < 0) {
            float impulse = -relative * (relative < -0.8f ? 1.12f : 1.0f);
            a->vx -= impulse * nx * wa;
            a->vy -= impulse * ny * wa;
            b->vx += impulse * nx * wb;
            b->vy += impulse * ny * wb;
            float slip = ((b->vx - a->vx) * -ny + (b->vy - a->vy) * nx) * 0.035f;
            a->vx -= slip * ny * wa;
            a->vy += slip * nx * wa;
            b->vx += slip * ny * wb;
            b->vy -= slip * nx * wb;
          }
          walls(a);
          walls(b);
        }
      }
    }
  }
  bool danger = false;
  for (unsigned i = 0; i < SK_CAPACITY; ++i) {
    const sk_fruit *b = &g->fruit[i];
    if (b->kind && b->age > 120 && b->y - sk_radius[b->kind] < SK_DANGER) danger = true;
  }
  if (danger) { if (g->danger < 180) ++g->danger; }
  else g->danger = 0;
  if (g->danger >= 180) g->over = 1;
}
