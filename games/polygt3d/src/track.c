/* The roads: three circuits, each a loop of pieces (so many segments, a bend, a climb) made into segments with
 * eased bends and hills and closed exactly; and the endless road, laid segment by segment ahead of you from a
 * seed, its bends, hills and scenery changing as it goes. Whole numbers only (game.h). */
#include "game.h"

/* ------------------------------------------------------------------ fixed point */
static const int16_t SIN[257] = {
  0, 201, 402, 603, 804, 1005, 1206, 1407, 1608, 1809, 2009, 2210, 2410, 2611, 2811, 3012,
  3212, 3412, 3612, 3811, 4011, 4210, 4410, 4609, 4808, 5007, 5205, 5404, 5602, 5800, 5998, 6195,
  6393, 6590, 6786, 6983, 7179, 7375, 7571, 7767, 7962, 8157, 8351, 8545, 8739, 8933, 9126, 9319,
  9512, 9704, 9896, 10087, 10278, 10469, 10659, 10849, 11039, 11228, 11417, 11605, 11793, 11980, 12167, 12353,
  12539, 12725, 12910, 13094, 13279, 13462, 13645, 13828, 14010, 14191, 14372, 14553, 14732, 14912, 15090, 15269,
  15446, 15623, 15800, 15976, 16151, 16325, 16499, 16673, 16846, 17018, 17189, 17360, 17530, 17700, 17869, 18037,
  18204, 18371, 18537, 18703, 18868, 19032, 19195, 19357, 19519, 19680, 19841, 20000, 20159, 20317, 20475, 20631,
  20787, 20942, 21096, 21250, 21403, 21554, 21705, 21856, 22005, 22154, 22301, 22448, 22594, 22739, 22884, 23027,
  23170, 23311, 23452, 23592, 23731, 23870, 24007, 24143, 24279, 24413, 24547, 24680, 24811, 24942, 25072, 25201,
  25329, 25456, 25582, 25708, 25832, 25955, 26077, 26198, 26319, 26438, 26556, 26674, 26790, 26905, 27019, 27133,
  27245, 27356, 27466, 27575, 27683, 27790, 27896, 28001, 28105, 28208, 28310, 28411, 28510, 28609, 28706, 28803,
  28898, 28992, 29085, 29177, 29268, 29358, 29447, 29534, 29621, 29706, 29791, 29874, 29956, 30037, 30117, 30195,
  30273, 30349, 30424, 30498, 30571, 30643, 30714, 30783, 30852, 30919, 30985, 31050, 31113, 31176, 31237, 31297,
  31356, 31414, 31470, 31526, 31580, 31633, 31685, 31736, 31785, 31833, 31880, 31926, 31971, 32014, 32057, 32098,
  32137, 32176, 32213, 32250, 32285, 32318, 32351, 32382, 32412, 32441, 32469, 32495, 32521, 32545, 32567, 32589,
  32609, 32628, 32646, 32663, 32678, 32692, 32705, 32717, 32728, 32737, 32745, 32752, 32757, 32761, 32765, 32766,
  32767};

int32_t pg_sin(int32_t a) {
  uint32_t u = (uint32_t)a & 0xFFFF;
  uint32_t q = u >> 14, i = (u >> 6) & 255;
  int32_t f = (int32_t)(u & 63), v;
  if (q & 1) v = SIN[256 - i] + (SIN[255 - i] - SIN[256 - i]) * f / 64;
  else v = SIN[i] + (SIN[i + 1] - SIN[i]) * f / 64;
  return q & 2 ? -v : v;
}
int32_t pg_cos(int32_t a) { return pg_sin(a + 16384); }

/* CORDIC: the angle of (x, y) in 1/65536 of a turn, 0 along x, growing towards y */
static const int32_t ATAN16[16] = {131072, 77376, 40884, 20753, 10417, 5213, 2607, 1304, 652, 326, 163, 81, 41, 20,
                                   10, 5};
int32_t pg_atan2(int32_t y, int32_t x) {
  if (!x && !y) return 0;
  int64_t X = x, Y = y;
  int32_t a = 0;
  if (X < 0) X = -X, Y = -Y, a = 32768 * 16;
  while (X > (1 << 28) || Y > (1 << 28) || Y < -(1 << 28)) X /= 2, Y /= 2;
  X *= 4, Y *= 4;
  for (int i = 0; i < 16; i++) {
    int64_t nx;
    if (Y > 0) nx = X + (Y >> i), Y = Y - (X >> i), X = nx, a += ATAN16[i];
    else nx = X - (Y >> i), Y = Y + (X >> i), X = nx, a -= ATAN16[i];
  }
  return ((a + 8) >> 4) & 0xFFFF;
}

uint32_t pg_isqrt(uint64_t v) {
  uint64_t r = 0, b = (uint64_t)1 << 62;
  while (b > v) b >>= 2;
  while (b) {
    if (v >= r + b) v -= r + b, r = (r >> 1) + b;
    else r >>= 1;
    b >>= 2;
  }
  return (uint32_t)r;
}

uint32_t pg_rng(uint32_t *s) {
  uint32_t x = *s;
  x ^= x << 13, x ^= x >> 17, x ^= x << 5;
  return *s = x;
}

static int32_t wrap16(int32_t a) { return (int16_t)(uint16_t)(a & 0xFFFF); }

/* ------------------------------------------------------------------ circuits */
typedef struct {
  uint8_t len;                      /* segments */
  int16_t turn;                     /* degrees over the piece, + right */
  int8_t climb;                     /* metres */
} piece;

#define P(...) {__VA_ARGS__}
static const piece RIDGE[] = {
  P(24, 0, 0), P(18, 65, 2), P(8, 0, 3), P(9, -38, 2), P(9, 48, 0), P(14, 0, -4), P(22, 105, -3), P(38, 0, 0),
  P(9, -35, 2), P(6, 0, 2), P(11, 95, 0), P(12, 0, -4), P(14, 55, -3), P(10, 0, 0), P(9, -30, 2), P(12, 0, 0),
  P(13, 95, 0), P(10, 0, 0)};
static const piece WANGAN[] = {
  P(36, 0, 0), P(8, 10, 3), P(16, 0, 4), P(14, 95, 2), P(34, 0, 0), P(12, 85, -2), P(28, 0, -4), P(6, -18, 0),
  P(6, 18, 0), P(3, 0, 0), P(11, 50, 2), P(12, 0, 4), P(9, -25, 2), P(12, 0, 0), P(18, 130, -3), P(7, 0, -3),
  P(6, 15, -2), P(9, 0, 0)};
static const piece AKAGI[] = {
  P(14, 0, 0), P(10, 80, 4), P(6, 0, 5), P(8, -165, 4), P(10, 0, 7), P(8, 170, 5), P(10, 0, 6), P(9, -55, 3),
  P(9, 65, 2), P(8, 0, 0), P(9, -150, -2), P(10, 0, -6), P(8, 160, -5), P(12, 0, -8), P(12, 110, -6), P(9, -45, -4),
  P(43, 0, -3), P(11, 140, 0), P(7, 0, 0), P(9, -70, 0), P(24, 0, 0), P(13, 120, 0), P(8, 0, 0)};
#undef P

typedef struct {
  const piece *p;
  uint8_t n;
  uint8_t trees;                    /* a tree (or lamp) every so many segments */
  uint8_t gantry0, gantry1;         /* lit gantries over the road from..to (segments) */
} circuit;
#define C(a, ...) {a, sizeof a / sizeof a[0], __VA_ARGS__}
static const circuit CIRCUITS[PG_CIRCUITS] = {C(RIDGE, 4, 56, 74), C(WANGAN, 3, 24, 120), C(AKAGI, 3, 0, 0)};
#undef C
const char *const pg_circuit_name[PG_CIRCUITS] = {"Ridge Valley GT", "Neo-Tokyo Wangan", "Mt. Akagi Touge"};
const char *const pg_circuit_note[PG_CIRCUITS] = {"Flowing bends", "Long straights", "Hairpins and climbs"};
const char *const pg_zone_name[5] = {"Alpine Valley", "Sunset Canyon", "Neon Wangan", "Glacier Pass", "Volcanic Ash"};

/* the fastest a car of grip 1 takes a bend: 24 m/s^2 sideways */
static uint8_t corner_limit(int32_t seg, int32_t turn) {
  if (turn < 0) turn = -turn;
  if (turn < 4) return 255;
  /* R = seg / (turn * 2pi / 65536); v = sqrt(24 R) */
  int64_t r_fx = (int64_t)seg * 65536 * 1000 / ((int64_t)turn * 6283);
  uint32_t v = pg_isqrt((uint64_t)(24 * r_fx / FX));
  return v > 255 ? 255 : (uint8_t)v;
}

static void finish_nodes(pg_track *t, int n) {
  /* headings from the positions: each segment's, then each node's halfway between its two segments */
  static uint16_t hs[PG_MAXN];
  for (int k = 0; k < n; k++) {
    int j = (k + 1) % n;
    hs[k] = (uint16_t)pg_atan2(t->px[j] - t->px[k], t->pz[j] - t->pz[k]);
  }
  for (int k = 0; k < n; k++) {
    int32_t a = hs[(k + n - 1) % n], b = hs[k];
    t->nh[k] = (uint16_t)(a + wrap16(b - a) / 2);
  }
  for (int k = 0; k < n; k++) t->cv[k] = (int16_t)wrap16(t->nh[(k + 1) % n] - t->nh[k]);
  for (int k = 0; k < n; k++) {
    int32_t c = (t->cv[(k + n - 1) % n] + 2 * t->cv[k] + t->cv[(k + 1) % n]) / 4;
    t->limit[k] = corner_limit(t->seg, c);
  }
}

void pg_build(pg_track *t, int id) {
  if (id < 0 || id >= PG_CIRCUITS) id = 0;
  const circuit *c = &CIRCUITS[id];
  static int32_t turn[PG_MAXN], climb[PG_MAXN], tmp[PG_MAXN];
  int n = 0;
  for (int i = 0; i < c->n; i++)
    for (int k = 0; k < c->p[i].len && n < PG_MAXN; k++, n++) {
      /* the piece's bend over its segments, in 1/16 angle unit; its climb in FX */
      turn[n] = (int32_t)c->p[i].turn * 65536 * 16 / 360 / c->p[i].len;
      climb[n] = (int32_t)c->p[i].climb * FX / c->p[i].len;
    }
  /* bends and hills ease in and out */
  for (int pass = 0; pass < 4; pass++) {
    for (int k = 0; k < n; k++) tmp[k] = (turn[(k + n - 1) % n] + 2 * turn[k] + turn[(k + 1) % n]) / 4;
    for (int k = 0; k < n; k++) turn[k] = tmp[k];
    for (int k = 0; k < n; k++) tmp[k] = (climb[(k + n - 1) % n] + 2 * climb[k] + climb[(k + 1) % n]) / 4;
    for (int k = 0; k < n; k++) climb[k] = tmp[k];
  }
  /* exactly one turn round, and back to the height it started at */
  int64_t st = 0, sc = 0;
  for (int k = 0; k < n; k++) st += turn[k], sc += climb[k];
  int32_t dt = (int32_t)(65536 * 16 - st), dc = (int32_t)-sc;
  int32_t rt = dt - dt / n * n, rc = dc - dc / n * n;   /* (what's left, spread one by one) */
  for (int k = 0; k < n; k++) {
    turn[k] += dt / n + (k < rt ? 1 : 0) - (k < -rt ? 1 : 0);
    climb[k] += dc / n + (k < rc ? 1 : 0) - (k < -rc ? 1 : 0);
  }
  /* laid out, each segment along its middle heading */
  int32_t seg = PG_SEG;
  int64_t x = 0, y = 0, z = 0, h = 0;
  for (int k = 0; k < n; k++) {
    t->px[k] = (int32_t)x, t->py[k] = (int32_t)y, t->pz[k] = (int32_t)z;
    int32_t mid = (int32_t)((h + turn[k] / 2) / 16);
    x += (int64_t)seg * pg_sin(mid) / 32767;
    z += (int64_t)seg * pg_cos(mid) / 32767;
    y += climb[k];
    h += turn[k];
  }
  /* what's left of a closed loop, spread along it */
  for (int k = 0; k < n; k++) {
    t->px[k] -= (int32_t)(x * k / n);
    t->py[k] -= (int32_t)(y * k / n);
    t->pz[k] -= (int32_t)(z * k / n);
  }
  t->endless = 0, t->id = (uint8_t)id, t->n = (uint16_t)n, t->seg = seg, t->length = seg * n;
  t->first = 0, t->last = n - 1;
  finish_nodes(t, n);
  /* the roadside and the orbs */
  int orbs = 0;
  for (int k = 0; k < n; k++) {
    int32_t cv = t->cv[k];
    int side = (k & 1) ? 0 : 8;
    uint8_t p = PROP_NONE;
    if (k == 0 || k == n / 3 || k == 2 * n / 3) p = PROP_ARCH;
    else if (k >= c->gantry0 && k < c->gantry1 && k % 4 == 0) p = PROP_GANTRY;
    else if ((cv > 700 || cv < -700) && k % 2 == 0) p = PROP_CHEVRON, side = cv > 0 ? 8 : 0;   /* on the outside */
    else if (k % c->trees == 0) p = PROP_TREE;
    t->prop[k] = (uint8_t)(p | (p ? side : 0));
    t->zone[k] = (uint8_t)id;
    t->orb[k] = -128, t->orb_id[k] = -1;
    if (k >= 8 && k % 15 == 8 && orbs < PG_ORBS) {
      int32_t x8;
      if (cv > 400) x8 = 34;            /* towards the inside of the bend */
      else if (cv < -400) x8 = -34;
      else x8 = (int32_t)((uint32_t)(k * 2654435761u) >> 26) - 32;   /* -32..31: up to 4 m off the middle */
      t->orb[k] = (int8_t)x8, t->orb_id[k] = (int8_t)orbs++;
    }
  }
  t->orbs = (uint8_t)orbs;
}

/* ------------------------------------------------------------------ the endless road */
#define R_MASK (PG_RING - 1)

/* lay one more segment from the end of the road */
static void lay(pg_track *t) {
  pg_gen *g = &t->gen;
  int32_t k = g->count;
  if (k > 0 && k % PG_ZONE == 0) g->zone = (uint8_t)((g->zone + 1 + pg_rng(&g->rng) % 4) % 5);
  /* bends: straights, sweepers and S-bends, sharper in the mountains */
  if (g->turn_hold <= 0) {
    uint32_t r = pg_rng(&g->rng) & 7;
    if (r < 2) {
      g->turn_to = 0, g->turn_hold = (int16_t)(10 + pg_rng(&g->rng) % 14);
    } else {
      /* (1/16 angle unit a segment: sweepers from a 600 m bend down to 105 m, 70 m in the mountains) */
      int32_t hi = (g->zone == 3 || g->zone == 4) ? 14000 : 9300;
      int32_t mag = 1700 + (int32_t)(pg_rng(&g->rng) % (uint32_t)(hi - 1700));
      g->turn_to = (pg_rng(&g->rng) & 1) ? mag : -mag;
      g->turn_hold = (int16_t)(12 + pg_rng(&g->rng) % 18);
    }
  }
  g->turn_hold--;
  g->turn += (g->turn_to - g->turn) * 46 / 256;
  /* hills: steeper in the mountains, flat on the expressway */
  if (g->slope_hold <= 0) {
    int32_t max = g->zone == 3 ? 150 : g->zone == 4 || g->zone == 1 ? 120 : g->zone == 2 ? 45 : 90;   /* FX a seg */
    g->slope_to = (int32_t)(pg_rng(&g->rng) % (uint32_t)(2 * max + 1)) - max;
    g->slope_hold = (int16_t)(10 + pg_rng(&g->rng) % 16);
  }
  g->slope_hold--;
  g->slope += (g->slope_to - g->slope) * 41 / 256;
  /* keep the road within a few hundred metres up or down */
  if (g->y > 60 * FX && g->slope > 0) g->slope -= 8;
  if (g->y < -60 * FX && g->slope < 0) g->slope += 8;

  int s = k & R_MASK;
  int32_t head = g->head + g->turn / 32;          /* the segment's middle heading */
  t->px[s] = g->x, t->py[s] = g->y, t->pz[s] = g->z;
  t->zone[s] = (uint8_t)(3 + g->zone);
  /* the node's heading: halfway between the segment before and this one */
  int32_t hs = head;
  if (k == 0) g->prev_head = hs;
  t->nh[s] = (uint16_t)(g->prev_head + wrap16(hs - g->prev_head) / 2);
  if (k > 0) {
    int p = (k - 1) & R_MASK;
    t->cv[p] = (int16_t)wrap16(t->nh[s] - t->nh[p]);
    t->limit[p] = corner_limit(t->seg, t->cv[p]);
  }
  g->prev_head = hs;
  g->x += (int32_t)((int64_t)t->seg * pg_sin(hs) / 32767);
  g->z += (int32_t)((int64_t)t->seg * pg_cos(hs) / 32767);
  g->y += g->slope;
  g->head += g->turn / 16;
  /* the roadside: a gate at each new scenery, gantries on the expressway, chevrons on the outside of bends */
  int32_t bend = g->turn / 16;
  uint8_t p = PROP_NONE;
  int side = (k & 1) ? 0 : 8;
  if (k > 0 && k % PG_ZONE == 0) p = PROP_ARCH;
  else if (g->zone == 2 && k % 4 == 0) p = PROP_GANTRY;
  else if ((bend > 450 || bend < -450) && k % 2 == 0) p = PROP_CHEVRON, side = bend > 0 ? 8 : 0;
  else if (k % 3 == 0) p = PROP_TREE;
  t->prop[s] = (uint8_t)(p | (p ? side : 0));
  t->orb[s] = -128, t->orb_id[s] = -1;
  if (k >= 6 && k % 12 == 6) {
    int32_t x8 = bend > 250 ? 32 : bend < -250 ? -32 : (int32_t)(pg_rng(&g->rng) % 65) - 32;
    t->orb[s] = (int8_t)x8;
  }
  g->count = k + 1;
  t->last = k - 1;                              /* (the last segment whose bend is known) */
  t->first = k + 1 - PG_RING;
  if (t->first < 0) t->first = 0;
}

void pg_endless(pg_track *t, uint32_t seed) {
  t->endless = 1, t->n = 0, t->seg = PG_SEG, t->length = 0, t->orbs = 0;
  pg_gen *g = &t->gen;
  g->rng = seed ? seed : 0x9E3779B9u;
  for (int i = 0; i < 4; i++) pg_rng(&g->rng);
  g->zone = (uint8_t)(pg_rng(&g->rng) % 5);
  t->id = g->zone;
  g->head = 0, g->turn = 0, g->turn_to = 0, g->turn_hold = 14, g->slope = 0, g->slope_to = 0, g->slope_hold = 10;
  g->count = 0, g->x = g->y = g->z = 0, g->prev_head = 0, g->spare = 0;
  t->first = 0, t->last = -1;
  pg_extend(t, 60);
}

void pg_extend(pg_track *t, int32_t upto) {
  if (!t->endless) return;
  while (t->last < upto) lay(t);
}

int pg_slot(const pg_track *t, int32_t k) {
  if (t->endless) return (int)(k & R_MASK);
  int32_t n = t->n, m = k % n;
  return (int)(m < 0 ? m + n : m);
}

int32_t pg_seg_of(const pg_track *t, int32_t s) {
  int32_t q = s / t->seg;
  if (s < 0 && q * t->seg != s) q--;
  return q;
}

int32_t pg_heading_at(const pg_track *t, int32_t s) {
  int32_t k = pg_seg_of(t, s), f = s - k * t->seg;
  int i = pg_slot(t, k);
  return (int32_t)t->nh[i] + (int32_t)((int64_t)t->cv[i] * f / t->seg);
}

int32_t pg_height_at(const pg_track *t, int32_t s) {
  int32_t k = pg_seg_of(t, s), f = s - k * t->seg;
  int i = pg_slot(t, k), j = pg_slot(t, k + 1);
  return t->py[i] + (int32_t)((int64_t)(t->py[j] - t->py[i]) * f / t->seg);
}

void pg_world(const pg_track *t, int32_t s, int32_t x, int32_t *wx, int32_t *wy, int32_t *wz) {
  int32_t k = pg_seg_of(t, s), f = s - k * t->seg;
  int i = pg_slot(t, k), j = pg_slot(t, k + 1);
  int32_t h = (int32_t)t->nh[i] + (int32_t)((int64_t)t->cv[i] * f / t->seg);
  *wx = t->px[i] + (int32_t)((int64_t)(t->px[j] - t->px[i]) * f / t->seg) + (int32_t)((int64_t)x * pg_cos(h) / 32767);
  *wz = t->pz[i] + (int32_t)((int64_t)(t->pz[j] - t->pz[i]) * f / t->seg) - (int32_t)((int64_t)x * pg_sin(h) / 32767);
  *wy = t->py[i] + (int32_t)((int64_t)(t->py[j] - t->py[i]) * f / t->seg);
}

int32_t pg_wrap(const pg_track *t, int32_t ds) {
  if (t->endless) return ds;
  int32_t L = t->length;
  ds %= L;
  if (ds >= L / 2) ds -= L;
  if (ds < -L / 2) ds += L;
  return ds;
}

int pg_zone_at(const pg_track *t, int32_t s) { return t->zone[pg_slot(t, pg_seg_of(t, s))]; }

/* the outline of a circuit, fit in w x h (y down, north up) */
int pg_map(const pg_track *t, int n, int16_t *xs, int16_t *ys, int w, int h) {
  int32_t x0 = t->px[0], x1 = x0, z0 = t->pz[0], z1 = z0;
  int nn = t->endless ? PG_RING : t->n;
  int32_t base = t->endless ? t->first : 0;
  for (int k = 0; k < nn; k++) {
    int i = pg_slot(t, base + k);
    if (t->px[i] < x0) x0 = t->px[i];
    if (t->px[i] > x1) x1 = t->px[i];
    if (t->pz[i] < z0) z0 = t->pz[i];
    if (t->pz[i] > z1) z1 = t->pz[i];
  }
  int32_t sx = x1 - x0 + 1, sz = z1 - z0 + 1;
  int32_t scale_n = w * sz > h * sx ? h : w, scale_d = w * sz > h * sx ? sz : sx;   /* the tighter fit */
  int32_t ox = (w - (int32_t)((int64_t)sx * scale_n / scale_d)) / 2, oy = (h - (int32_t)((int64_t)sz * scale_n / scale_d)) / 2;
  for (int i = 0; i < n; i++) {
    int k = pg_slot(t, base + (int32_t)((int64_t)i * nn / n));
    xs[i] = (int16_t)(ox + (int64_t)(t->px[k] - x0) * scale_n / scale_d);
    ys[i] = (int16_t)(h - 1 - oy - (int64_t)(t->pz[k] - z0) * scale_n / scale_d);
  }
  return n;
}
