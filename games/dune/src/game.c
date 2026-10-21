/* The ride, as the web game's SceneGame and its p2.js world (js/game.js; the numbers are in game.h and below).
 *
 * Each step is one frame of the game at 60 a second: first the scene's update (input, zoom, recycling the
 * dunes, the slides, the crash), then one p2 step of 1/60 s: damping, the sensors' contacts and their events,
 * the contact solver (p2's Gauss-Seidel, 10 iterations, with its SPOOK stiffness and relaxation) and the
 * integration. p2 works in metres, -0.05 of a point on both axes (a half turn and a scale), so the same physics
 * runs here in points: gravity 1000, the skin 0.1. */
#include <math.h>
#include <string.h>
#include "game.h"
#include "art.h"

#define STEP (1.0f / 60)
#define R_BALL 42.0f            /* 60 x 0.7 */
#define R_GROUND 70.0f          /* the ground sensor: 100 x 0.7 */
#define R_SLIDE 35.0f           /* the slide sensor: 50 x 0.7, 30 below */
#define R_PICK 72.8f            /* the ball and a coin's sensor (43 x 1.2 / 2 + 5) */
#define SKIN 0.1f               /* the contact material's contactSkinSize, 0.005 m */

uint32_t dn_rand(uint32_t *s) {
  uint32_t x = *s;
  x ^= x << 13, x ^= x >> 17, x ^= x << 5;
  return *s = x ? x : 0x9E3779B9u;
}
float dn_frand(uint32_t *s) { return (float)(dn_rand(s) >> 8) * (1.0f / 16777216.0f); }

static float clampf(float v, float a, float b) { return v < a ? a : v > b ? b : v; }

/* ---------------------------------------------------------------- the dunes (CreateTerrainSegment) */

void dn_shape(const dn_seg *s, dn_curve *c) {
  for (int n = 0; n < 63; n++) c->y[n] = s->flat ? 0 : sinf(n / 20.0f) * s->h * n / s->m;
  c->y[63] = 0;
  /* the physics: every 8th point and the end, 15 down (each line is there three times: +0, +18, +36) */
  for (int k = 0; k < 8; k++) c->lx[k] = s->d / 90 * (k * 8), c->ly[k] = c->y[k * 8] + 15;
  c->lx[8] = s->w, c->ly[8] = 15;
  /* p2 places each Line at the middle of its two points, length + 0.1, at their angle */
  for (int k = 0; k < 8; k++) {
    float x0 = c->lx[k], y0 = c->ly[k], dx = c->lx[k + 1] - x0, dy = c->ly[k + 1] - y0;
    float len = sqrtf(dx * dx + dy * dy) + 0.1f, a = atan2f(dy, dx), mx = x0 + dx / 2, my = y0 + dy / 2;
    float hx = cosf(a) * len / 2, hy = sinf(a) * len / 2, ul = sqrtf(hx * hx + hy * hy);
    float *l = c->line[k];
    l[0] = mx - hx, l[1] = my - hy, l[2] = mx + hx, l[3] = my + hy, l[4] = hx / ul, l[5] = hy / ul;
  }
}

void dn_shapes(dn_run *r) {
  for (int i = 0; i < DN_SEGS; i++) dn_shape(&r->seg[i], &r->curve[i]);
}

void dn_make_seg(dn_seg *s, float r0, float r1, float r2, bool flat) {
  float d = 700 + 1700 * r0 + (flat ? 1000 : 0);
  if (d > 2048) d = 2048;
  memset(s, 0, sizeof *s);
  s->d = d, s->h = d / 10 + (220 * r1 - 50), s->m = 42 + 15 * r2, s->flat = flat;
  s->w = d / 90 * 63;   /* the curve dips below 0 at n = 63 (sin(63/20) < 0): it ends there */
}

float dn_curve_at(const dn_run *r, int seg, float x) {
  const dn_seg *s = &r->seg[seg];
  const float *y = r->curve[seg].y;
  float n = (x - s->x) * 90 / s->d;
  if (n <= 0) return y[0];
  if (n >= 63) return 0;
  int i = (int)n;
  return y[i] + (y[i + 1] - y[i]) * (n - i);
}

int dn_seg_at(const dn_run *r, float x) {
  for (int i = 0; i < 8; i++) {
    const dn_seg *s = &r->seg[r->order[i]];
    if (x >= s->x && x < s->x + s->w + 2) return r->order[i];
  }
  return -1;
}

/* p2's circleLine: a circle at (cx, cy) against the segment a..b (u: its direction); the contact normal from
   the circle to the line and the overlap term (distance - radius); justTest when n is 0 */
static bool circle_line(float cx, float cy, float rad, float ax, float ay, float bx, float by, float ux, float uy, float *n, float *pen) {
  float gx = uy, gy = -ux;   /* rotate90cw */
  float s = (cx - ax) * gx + (cy - ay) * gy;
  if (fabsf(s) < rad) {
    float px = cx - gx * s, py = cy - gy * s;
    float e = ux * px + uy * py, m0 = ux * ax + uy * ay, m1 = ux * bx + uy * by;
    if (m0 < e && e < m1) {
      if (n) {
        float l = fabsf(s);
        n[0] = l > 0 ? -gx * s / l : 0, n[1] = l > 0 ? -gy * s / l : 0;
        *pen = l - rad;
      }
      return true;
    }
  }
  for (int i = 0; i < 2; i++) {
    float ex = (i ? bx : ax) - cx, ey = (i ? by : ay) - cy, d2 = ex * ex + ey * ey;
    if (d2 < rad * rad) {
      if (n) {
        float l = sqrtf(d2);
        n[0] = l > 0 ? ex / l : 0, n[1] = l > 0 ? ey / l : 0;
        *pen = l - rad;
      }
      return true;
    }
  }
  return false;
}

/* ---------------------------------------------------------------- coins */

static void add_coin(dn_run *r, float x, float y) {
  for (int i = 0; i < DN_COINS; i++)
    if (!r->coin[i].on) {
      r->coin[i].x = x, r->coin[i].y = y, r->coin[i].on = 1, r->coin[i].shrink = 0;
      return;
    }
}

/* the curve points 70 apart (coinPoints), from n = 0 */
static int coin_points(const dn_seg *s, const dn_curve *cv, float *px, float *py, int max) {
  int k = 0;
  for (int n = 0; n < 63 && k < max; n++) {
    float x = s->d / 90 * n, y = cv->y[n];
    if (k == 0 || hypotf(px[k - 1] - x, py[k - 1] - y) > 70) px[k] = x, py[k] = y, k++;
  }
  return k;
}

/* SpawnCoinsInAir: 4 or 5 on the way up, then a shape of them ahead when the ball is high enough */
static void air_coins(dn_run *r) {
  float x = r->x, y = r->y, vx = r->vx, vy = r->vy;
  float a = 3 + 2 * dn_frand(&r->rng);
  for (int b = 0; b < a; b++) {
    float cy = y + b / 10.0f * vy + b * b * b;
    if (!(cy > 170)) add_coin(r, x + b / 10.0f * vx, cy);
  }
  float px = x + 0.5f * vx, py = y + 0.5f * vy + 125;
  if (!(py < 160)) return;
  py = clampf(py, -400, 200);
  static const int16_t arrow[][2] = {{0, 0}, {80, -80}, {160, -160}, {80, -160}, {0, -160}, {160, -80}, {160, 0}};
  if (!(py < -200)) {
    for (int i = 0; i < 7; i++) add_coin(r, px + arrow[i][0], py + arrow[i][1] + 100);
    return;
  }
  float p = dn_frand(&r->rng);
  if (p < 0.25f) {
    for (int b = 0; b < 3; b++)
      for (int c = 0; c < 3; c++) add_coin(r, px + 80 * c, py - 80 * b + 100);
  } else if (p < 0.4f) {
    /* five letters of coins (columns, a diagonal, bars), from (400, -250) */
    static const int16_t word[][2] = {
        {0, 0}, {0, 80}, {0, 160}, {0, 240}, {0, 320}, {55, 100}, {100, 160}, {145, 220},
        {200, 0}, {200, 80}, {200, 160}, {200, 240}, {200, 320},
        {400, 0}, {400, 80}, {400, 160}, {400, 240}, {400, 320},
        {600, 0}, {600, 80}, {600, 160}, {600, 240}, {600, 320}, {680, 0}, {760, 0}, {680, 320}, {760, 320},
        {950, 0}, {950, 80}, {950, 160}, {950, 240}, {950, 320}, {1030, 0}, {1110, 0}, {1030, 160}, {1110, 160}, {1030, 320}, {1110, 320}};
    for (unsigned i = 0; i < sizeof word / sizeof word[0]; i++) add_coin(r, px + word[i][0] + 400, py + word[i][1] - 250 + 100);
  } else if (p < 0.7f) {
    for (int b = 0; b < 3; b++)
      for (int c = 0; c < 5; c++) add_coin(r, px + 80 * c, py - 80 * b + 100);
  } else {
    for (int b = 0; b < 6; b++)
      for (int c = 0; c < 3; c++) add_coin(r, px + 80 * c, py - 80 * b + 100);
  }
}

/* ---------------------------------------------------------------- the landscapes' moving parts */

static const float sprite_w[7][7] = {   /* the sprites' widths (points), for their starts and wrapping */
    {0}, {0}, {856, 856}, {764, 266}, {1013, 462, 345, 310, 184}, {0}, {314, 363, 143, 353, 353}};

static void bg_reset(dn_bg *b, int land) {
  uint32_t rng = b->rng;
  memset(b, 0, sizeof *b);
  b->rng = rng ? rng : 0x1234567u;
  b->alpha = 1;
  for (int i = 0; i < 7; i++) b->scale[i] = 1, b->ax[i] = 0.5f;
  switch (land) {
    case 2:   /* the volcano at 0.8, its fire at 1 */
      b->scale[0] = 0.8f, b->start[0] = DN_W + 856 * 0.8f, b->start[1] = DN_W + 856;
      break;
    case 3:   /* the big mushroom; the purple one (placed by the first one's width) */
      b->start[0] = DN_W + 764, b->start[1] = DN_W + 764;
      b->y[1] = 460 + 100 * dn_frand(&b->rng);
      break;
    case 4: { /* the planets at 0 to 4 screens, shuffled, plus up to 200 */
      int k[5] = {0, 1, 2, 3, 4}, left = 5;
      for (int i = 0; i < 5; i++) {
        int j = (int)(dn_frand(&b->rng) * left);
        b->start[i] = DN_W * k[j] + 200 * dn_frand(&b->rng);
        for (int m = j; m < left - 1; m++) k[m] = k[m + 1];
        left--;
        b->y[i] = 300 * dn_frand(&b->rng);
      }
      break;
    }
    case 6:
      b->start[0] = DN_W + 314, b->start[1] = 1.8f * DN_W + 363, b->start[2] = 1.2f * DN_W + 143;
      b->start[3] = 2 * DN_W + 353, b->start[4] = 2.9f * DN_W + 353;
      break;
  }
}

static void bg_step(dn_bg *b, int land, float bx, float vx) {
  uint32_t *g = &b->rng;
  switch (land) {
    case 0: b->alpha = clampf(b->alpha + 0.02f * dn_frand(g) - 0.01f, 0.05f, 1); break;
    case 1: b->alpha = clampf(b->alpha + 0.02f * dn_frand(g) - 0.01f, 0.5f, 1); break;
    case 2:
      if (b->start[0] - bx / 7 < -856 * b->scale[0]) b->start[0] += 2 * DN_W, b->scale[0] = 0.5f + 0.4f * dn_frand(g);
      if (b->start[1] - bx / 8.5f < -856 * b->scale[1]) b->start[1] += 2 * DN_W, b->scale[1] = 0.8f + 0.3f * dn_frand(g);
      break;
    case 3:
      if (b->start[0] - bx / 18 < -764 * b->scale[0]) b->start[0] += 2 * DN_W, b->scale[0] = 0.8f + 0.3f * dn_frand(g);
      if (b->start[1] - bx / 8 < -266 * b->scale[1])
        b->start[1] += 2 * DN_W, b->y[1] = 460 + 100 * dn_frand(g), b->scale[1] = 0.8f + 0.3f * dn_frand(g);
      break;
    case 4: {
      static const float scroll[5] = {30, 18, 14, 12, 9};
      b->ty -= 0.1f;
      for (int i = 0; i < 5; i++)
        if (b->start[i] - bx / scroll[i] < -sprite_w[4][i] * b->scale[i]) {
          b->start[i] += DN_W * (2 + 2 * dn_frand(g));
          b->y[i] = -10 + dn_frand(g) * DN_H * 0.82f;
          b->scale[i] = 0.8f + 0.3f * dn_frand(g);
        }
      break;
    }
    case 5:
      b->tx += 0.5f - vx / 300, b->ty += 2;
      b->tx2 += -0.5f - vx / 500, b->ty2 += 2;
      b->tx = fmodf(b->tx, 4096), b->tx2 = fmodf(b->tx2, 4096), b->ty = fmodf(b->ty, 4096), b->ty2 = fmodf(b->ty2, 4096);
      break;
    case 6: {
      static const float scroll[5] = {15, 15, 8, 8, 8}, more[5] = {2, 1, 2, 2, 2};
      for (int i = 0; i < 5; i++)
        if (b->start[i] - bx / scroll[i] < -sprite_w[6][i] * b->scale[i]) {
          b->start[i] += DN_W * (2 + more[i] * dn_frand(g));
          float lo = i == 2 ? 0.6f : i >= 3 ? 0.7f : 0.8f, span = i == 2 ? 0.6f : i >= 3 ? 0.7f : 0.3f;
          b->scale[i] = lo + span * dn_frand(g);
          if (i >= 3) b->ax[i] = dn_frand(g);
        }
      break;
    }
  }
}

/* ---------------------------------------------------------------- the level */

void dn_style(dn_run *r, uint8_t ball, uint8_t land) {
  r->ball = ball < DN_BALLS ? ball : 0;
  r->rotates = art_ball_info_[r->ball].rotates;
  if (!r->rotates) r->w = 0;
  if (land >= DN_LANDS) land = 0;
  if (land != r->land || !r->bg.rng) r->land = land, bg_reset(&r->bg, land);
}

void dn_reset(dn_run *r) {
  r->cur = r->order[0];
  float x = -300;
  for (int i = 0; i < 8; i++) {
    dn_seg *s = &r->seg[r->order[i]];
    s->x = x;
    x = floorf(s->x - 1) + s->w;
  }
  r->seg[r->spare].x = -9999;
  r->x = 700, r->y = 200, r->vx = r->vy = 0, r->w = 0, r->angle = 0;
  r->polys = 0, r->touching = 0;
  r->damping = 0.01f;
  r->spawned = r->score = 0, r->slide_lines = 0;
  bg_reset(&r->bg, r->land);
}

void dn_new(dn_run *r, uint32_t seed, uint8_t ball, uint8_t land) {
  memset(r, 0, sizeof *r);
  r->rng = seed ? seed : 0x44554E45u;
  r->bg.rng = r->rng * 2654435761u | 1;
  /* the nine segments, made once: the first flat, the last set aside (and left without a material) */
  for (int i = 0; i < DN_SEGS; i++) {
    float a = dn_frand(&r->rng), b = dn_frand(&r->rng), c = dn_frand(&r->rng);
    dn_make_seg(&r->seg[i], a, b, c, i == 0);
  }
  r->seg[8].sticky = 1;
  dn_shapes(r);
  for (int i = 0; i < 8; i++) r->order[i] = (uint8_t)i;
  r->spare = 8;
  r->zoom = 1, r->trail_sx = 1, r->can_slide = 1, r->alive = 1;
  r->land = 0xFF;
  dn_style(r, ball, land);
  dn_reset(r);
}

void dn_start(dn_run *r) {
  r->alive = 1, r->start_wait = 18;   /* 300 ms */
  r->score = 0, r->guide = 1;
}

static void end_game(dn_run *r) {
  r->running = 0;
  r->vx = r->vy = 0, r->damping = 0.5f;
  r->zoom_from = r->zoom, r->crash_t = 12;   /* back to 1 in 200 ms */
  for (int i = 0; i < DN_COINS; i++) r->coin[i].on = 0;
}

void dn_menu(dn_run *r) {
  r->start_wait = 0;
  end_game(r);
  dn_reset(r);
}

/* the ball over the line (aboveLine's sensor) */
static void line_passed(dn_run *r) {
  int n = r->vy > -800 ? 1 : r->vy > -1300 ? 2 : 3;
  r->score += n;
  air_coins(r);
  r->line_t = 42, r->line_n = (uint8_t)n, r->flash_t = 18;
}

/* InputHandler.update */
static void input(dn_run *r, bool held) {
  if (!r->running) {
    r->moving = 0;
    return;
  }
  if (!r->valid || !held) {
    r->moving = 0;
    return;
  }
  r->moving = 1;
  if (r->vx < 2700) {
    r->vx += 16;
    if (r->touching) {
      r->vx += 12;
      /* (the game also tests `curveFunction(...) > 0.5`: an object compared with a number, never true) */
      if (r->vx < 250) r->vx += 30, r->vy -= 30;
      else r->vy += r->vx / 80;
    }
  }
  if (r->vy < 2300 && !r->touching) r->vy += 80, r->vy += r->vx / 50;
}

static float u_on(const dn_run *r, int seg) { return (r->x - r->seg[seg].x) / r->seg[seg].w; }

/* the coins behind (CleanObjectsBehind) and the end of their pick-up */
static void coins_tick(dn_run *r) {
  for (int i = 0; i < DN_COINS; i++) {
    dn_coin *c = &r->coin[i];
    if (!c->on) continue;
    if (c->shrink && !--c->shrink) c->on = 0;
    if (r->tick % 50 == 0 && c->x < r->x - 500) c->on = 0;
  }
}

static void recycle(dn_run *r) {
  dn_seg *second = &r->seg[r->order[1]];
  if (!(second->x + second->w < r->x - 2000)) return;
  uint8_t id = r->order[1];
  for (int i = 1; i < 7; i++) r->order[i] = r->order[i + 1];
  if (dn_frand(&r->rng) < 0.6f) {
    uint8_t t = r->spare;
    r->spare = id, id = t;
  }
  dn_seg *last = &r->seg[r->order[6]], *s = &r->seg[id];
  s->x = floorf(last->x - 1) + last->w;
  r->order[7] = id;
  r->spawned++;
  if (r->spare != id) r->seg[r->spare].x = -9999;
  if (dn_frand(&r->rng) < 0.6f) {
    float px[64], py[64];
    int k = coin_points(s, &r->curve[id], px, py, 64), n = 2 + (int)(dn_frand(&r->rng) * 3);
    for (int c = 1; c <= n && c < k; c++) add_coin(r, s->x + px[c], DN_TY + py[c] - 30);
  }
}

/* moving everything back when the ball is far along, so the numbers keep their precision (a power of two: the
   floors of the recycling stay the same) */
static void rebase(dn_run *r) {
  if (r->x < 131072) return;
  const float k = 65536;
  r->x -= k, r->rebased++;
  for (int i = 0; i < DN_SEGS; i++)
    if (r->seg[i].x > -9999) r->seg[i].x -= k;
  for (int i = 0; i < DN_COINS; i++) r->coin[i].x -= k;
}

/* ---------------------------------------------------------------- one p2 step */

#ifdef HOST
void (*dn_trace)(const dn_run *r, int after);   /* the tests' look at each step, before the solver and after */
#endif

typedef struct { float nx, ny, pen, B, invC, eps, lambda, lo, hi, gx, gy, ga; uint8_t sticky; } eq_t;

static void physics(dn_run *r) {
  const float h = STEP, a = 4 / (h * 17), b = 16.0f / 17;   /* SPOOK with relaxation 4 */
  /* World.internalStep: gravity's force, then damping: v *= (1 - damping)^h */
  float damp = r->damping == 0.5f ? 0.98851402f : 0.99983251f;
  r->vx *= damp, r->vy *= damp;

  /* the narrowphase, at the positions the step starts from */
  eq_t eq[48];
  int neq = 0;
  uint32_t g[DN_SEGS], sl[DN_SEGS];
  /* in the order p2's sweep-and-prune pairs them: by their boxes' left edges in its (mirrored) x, so the dune
     reaching furthest right first (the contacts are solved in turn, so their order counts) */
  uint8_t by[DN_SEGS];
  for (int i = 0; i < DN_SEGS; i++) {
    int k = i;
    while (k > 0 && r->seg[by[k - 1]].x + r->seg[by[k - 1]].w < r->seg[i].x + r->seg[i].w) by[k] = by[k - 1], k--;
    by[k] = (uint8_t)i;
  }
  for (int o = 0; o < DN_SEGS; o++) {
    int i = by[o];
    const dn_seg *s = &r->seg[i];
    g[i] = sl[i] = 0;
    if (!(r->x > s->x - 130 && r->x < s->x + s->w + 130)) continue;
    for (int k = 0; k < 8; k++)
      for (int l = 0; l < 3; l++) {
        const float *L = r->curve[i].line[k];
        float n[2], pen, ax = s->x + L[0], ay = DN_TY + L[1] + 18 * l, bx = s->x + L[2], by = DN_TY + L[3] + 18 * l;
        float ux = L[4], uy = L[5];
        if (fabsf(r->y - (ay + by) / 2) > fabsf(by - ay) / 2 + 110) continue;   /* (far from all three circles) */
        int bit = k * 3 + l;
        if (circle_line(r->x, r->y, R_GROUND, ax, ay, bx, by, ux, uy, 0, 0)) g[i] |= 1u << bit;
        if (circle_line(r->x, r->y + 30, R_SLIDE, ax, ay, bx, by, ux, uy, 0, 0)) sl[i] |= 1u << bit;
        if (neq < 48 && circle_line(r->x, r->y, R_BALL, ax, ay, bx, by, ux, uy, n, &pen))
          eq[neq].nx = n[0], eq[neq].ny = n[1], eq[neq].pen = pen, eq[neq].sticky = s->sticky, neq++;
      }
  }
  bool above = r->y - R_BALL < 180 && r->y + R_BALL > -4820;   /* aboveLine: a box over y < 180 */

  /* beginContact events, as the game's handlers (they act only while the level runs) */
  for (int i = 0; i < DN_SEGS; i++) {
    for (uint32_t m = g[i] & ~r->ground[i]; m; m &= m - 1)
      if (r->running) {
        if (r->polys == 0 && r->vy > 1000) r->vy = 1000;   /* the first line the ground sensor meets */
        if (r->polys < 255) r->polys++;
        r->touching = 1;
      }
    for (uint32_t m = sl[i] & ~r->slider[i]; m; m &= m - 1)
      if (r->running) {
        float u = u_on(r, r->cur);
        if (r->slide_lines == 0 && i != r->order[0] && u < 0.4f && r->vy > 10) r->sliding = 1, r->slide_start = u;
        if (r->slide_lines < 255) r->slide_lines++;
      }
  }
  if (above && !r->above && r->running) line_passed(r);
  for (int i = 0; i < DN_COINS; i++) {
    dn_coin *c = &r->coin[i];
    float dx = r->x - c->x, dy = r->y - c->y;
    bool t = dx * dx + dy * dy < R_PICK * R_PICK;
    if (t && !c->touch && r->running && c->on && !c->shrink) r->picked++, c->shrink = 6;
    c->touch = t;
  }
  /* endContact events */
  for (int i = 0; i < DN_SEGS; i++) {
    for (uint32_t m = r->ground[i] & ~g[i]; m; m &= m - 1)
      if (r->running) {
        if (r->polys) r->polys--;
        if (!r->polys) {
          r->touching = 0;
          if (r->vx > 0 && r->moving) r->vx *= 1.03f, r->vy *= 1.03f;   /* leaving the ground while held */
        }
      }
    for (uint32_t m = r->slider[i] & ~sl[i]; m; m &= m - 1)
      if (r->running) {
        if (r->slide_lines) r->slide_lines--;
        if (!r->slide_lines) r->can_slide = 0;
      }
    r->ground[i] = g[i], r->slider[i] = sl[i];
  }
  r->above = above;
#ifdef HOST
  if (dn_trace) dn_trace(r, 0);
#endif

  /* the solver (GSSolver.solve): B and 1/C with the velocity now; then 10 Gauss-Seidel sweeps */
  const float invI = r->rotates ? 1.0f / (R_BALL * R_BALL / 2) : 0;   /* a disc: m r^2 / 2 */
  int nc = neq;
  for (int i = 0; i < nc; i++) {
    eq_t *e = &eq[i];
    float k = e->sticky ? 1e6f : 1e11f;
    e->gx = -e->nx, e->gy = -e->ny, e->ga = 0;
    float gw = e->gx * r->vx + e->gy * r->vy, gimf = e->gy * 1000;
    e->eps = 4 / (h * h * k * 17);
    e->B = -(e->pen + SKIN) * a - gw * b - h * gimf;
    e->invC = 1 / (1 + e->eps);
    e->lambda = 0, e->lo = 0, e->hi = 3e37f;
  }
  for (int i = 0; i < nc && neq < 48; i++) {
    if (!eq[i].sticky) continue;
    /* friction (p2's default material only): tangent rotate90cw(n); |impulse| up to 0.3 x 1000 x h */
    eq_t *f = &eq[neq++];
    float tx = eq[i].ny, ty = -eq[i].nx;
    f->gx = -tx, f->gy = -ty, f->ga = R_BALL;
    float gw = f->gx * r->vx + f->gy * r->vy + f->ga * r->w, gimf = f->gy * 1000;
    f->eps = 4 / (h * h * 1e6f * 17);
    f->B = -gw * b - h * gimf;
    f->invC = 1 / (1 + invI * R_BALL * R_BALL + f->eps);
    f->lambda = 0, f->lo = -0.3f * 1000 * h, f->hi = 0.3f * 1000 * h;
  }
  float vlx = 0, vly = 0, wl = 0;
  const float tol = 2e-6f * neq;   /* tolerance 1e-7 m/s per equation */
  for (int it = 0; it < 10 && neq; it++) {
    float sum = 0;
    for (int i = 0; i < neq; i++) {
      eq_t *e = &eq[i];
      float gwl = e->gx * vlx + e->gy * vly + e->ga * wl;
      float d = e->invC * (e->B - gwl - e->eps * e->lambda), nl = e->lambda + d;
      if (nl < e->lo) d = e->lo - e->lambda;
      else if (nl > e->hi) d = e->hi - e->lambda;
      e->lambda += d;
      vlx += e->gx * d, vly += e->gy * d, wl += invI * e->ga * d;
      sum += fabsf(d);
    }
    if (sum * sum <= tol * tol) break;
  }
  r->vx += vlx, r->vy += vly, r->w += wl;

  /* integrate: gravity's force, then the position */
  r->vy += h * 1000;
  r->x += h * r->vx, r->y += h * r->vy;
  if (r->rotates) r->angle += h * r->w;
#ifdef HOST
  if (dn_trace) dn_trace(r, 1);
#endif
}

/* ---------------------------------------------------------------- one frame */

static float screen_y(const dn_run *r) { return r->y * r->zoom - r->cam_y; }

void dn_step(dn_run *r, bool held, bool pressed) {
  r->frame++;
  /* timers: the airborne count reset 60 ms after a frame on the ground; the level starting 300 ms in */
  /* a press (OnDown, between frames): input counts from the first press while the level runs; the guide goes */
  if (pressed) {
    r->valid = r->running;
    if (r->running) r->guide = 0;
  }
  if (r->hist & 8) r->airborn = 0;
  if (r->start_wait && !--r->start_wait) r->running = 1;
  if (++r->tick > 60) r->tick = 0;

  /* SceneGame.update */
  if (r->running && r->alive) input(r, held);
  if (screen_y(r) > DN_H + 200) dn_reset(r);
  coins_tick(r);
  float target = clampf((r->y + DN_TY) / 750, 0.4f, 1), blend = 0.2f + clampf(-0.0004f * r->vy, 0.2f, 0.8f);
  float zoom = r->zoom + (target - r->zoom) * blend;
  if (r->vx < 1800) r->vx *= 1.005f;
  if (r->y < -650 && r->vy < 0) r->vx += 0.15f * -r->vy, r->vy *= 0.8f;
  else if (r->y < -300 && r->vy < 0) r->vy += 30;
  if (r->running) r->zoom = zoom;
  r->cam_y += (100 * zoom + (zoom - 1) * DN_H - r->cam_y) * 0.4f;
  recycle(r);
  float speed = sqrtf(r->vx * r->vx + r->vy * r->vy);
  r->trail_sx += (clampf((r->vx + r->vy) / (r->touching ? 400 : 160), 0.2f, 4.5f) - r->trail_sx) * 0.03f;
  r->trail_rot += ((speed > 0 ? atan2f(r->vy / speed, r->vx / speed) : 0) - r->trail_rot) * 0.3f;
  /* a slide: down most of a dune, fast, from near its top */
  float u = u_on(r, r->cur);
  if (r->sliding && r->can_slide && u - r->slide_start > 0.4f && r->slide_start < 0.45f && speed > 1900) {
    r->can_slide = 0;
    bool perfect = r->slide_start < 0.2f;
    r->score += perfect ? 3 : 1;
    float k = perfect ? 1.2f : 1.05f;
    r->vx *= k, r->vy *= k;
    r->slide_kind = perfect ? DN_POP_PERFECT : DN_POP_NICE;
    r->slide_t = 1, r->slide_hold = (uint8_t)(54 + 18 * dn_frand(&r->bg.rng));
    r->slide_y0 = 332 + 500 * dn_frand(&r->bg.rng), r->slide_y1 = DN_TY + 700 * dn_frand(&r->bg.rng);
  }
  if (r->rotates) r->angle += r->vx / 8000;
  for (int i = 0; i < 8; i++) {
    const dn_seg *s = &r->seg[r->order[i]];
    if (r->x > s->x && r->x < s->x + s->w) {
      if (r->cur != r->order[i]) r->can_slide = 1, r->sliding = 0;
      r->cur = r->order[i];
      break;
    }
  }
  /* the crash: a hard landing on the last 30% of a dune (its uphill side) after a jump */
  const dn_seg *c = &r->seg[r->cur];
  if (r->alive && r->last_vx > 10 && r->last_vy > 30 && fabsf(r->last_vx - r->vx) > 70 && fabsf(r->last_vy - r->vy) > 70 &&
      r->x > c->x + 0.7f * c->w && r->airborn > 20 && r->cur != r->order[0]) {
    r->alive = 0;
    end_game(r);
    r->shake_t = 6;
  }
  r->last_vx = r->vx, r->last_vy = r->vy;
  if (!r->touching) {
    if (r->airborn < 65535) r->airborn++;
  }
  r->hist = (uint8_t)(r->hist << 1 | r->touching);
  bg_step(&r->bg, r->land, dn_world_x(r), r->vx);

  /* the tweens (before the physics, as Phaser's) */
  if (r->line_t) r->line_t--;
  if (r->flash_t) r->flash_t--;
  if (r->shake_t) r->shake_t--;
  if (r->slide_t && ++r->slide_t > 24 + r->slide_hold) r->slide_t = 0;
  if (r->crash_t) {
    r->crash_t--;
    float t = 1 - r->crash_t / 12.0f, e = 1 - (1 - t) * (1 - t) * (1 - t);
    r->zoom = r->zoom_from + (1 - r->zoom_from) * e;
  }

  physics(r);
  rebase(r);
  if (!isfinite(r->x) || !isfinite(r->y) || !isfinite(r->vx) || !isfinite(r->vy) || fabsf(r->y) > 1e6f) {
    /* (never seen: a broken state starts the level again rather than going on) */
    r->vx = r->vy = 0, r->w = 0;
    dn_reset(r);
  }
}

bool dn_valid(const dn_run *r) {
  if (!isfinite(r->x) || !isfinite(r->y) || !isfinite(r->vx) || !isfinite(r->vy) || !isfinite(r->w) || !isfinite(r->angle))
    return false;
  if (fabsf(r->x) > 2e5f || fabsf(r->y) > 1e5f || fabsf(r->vx) > 1e5f || fabsf(r->vy) > 1e5f) return false;
  if (!(r->zoom >= 0.39f && r->zoom <= 1.01f) || !isfinite(r->cam_y) || !isfinite(r->trail_sx) || !isfinite(r->trail_rot)) return false;
  if (r->ball >= DN_BALLS || r->land >= DN_LANDS || r->cur >= DN_SEGS || r->spare >= DN_SEGS) return false;
  if (r->order[0] != 0) return false;
  unsigned seen = 1u << r->spare;
  for (int i = 0; i < 8; i++) {
    if (r->order[i] >= DN_SEGS || (seen & 1u << r->order[i])) return false;
    seen |= 1u << r->order[i];
  }
  for (int i = 0; i < DN_SEGS; i++) {
    const dn_seg *s = &r->seg[i];
    if (!isfinite(s->x) || !(s->d >= 700 && s->d <= 2048) || !(s->h > 0 && s->h < 400) || !(s->m >= 42 && s->m <= 57)) return false;
    if (s->flat != (i == 0) || s->sticky != (i == 8)) return false;
  }
  for (int i = 0; i < DN_COINS; i++)
    if (r->coin[i].on && (!isfinite(r->coin[i].x) || !isfinite(r->coin[i].y) || r->coin[i].shrink > 6)) return false;
  if (r->damping != 0.01f && r->damping != 0.5f) return false;
  return true;
}
