/* The six circuits: each a loop of pieces (so many segments, a bend, a climb), made into segments with smooth
 * bends and hills; what stands beside the road; and the outline drawn on the screens. */
#include <math.h>
#include "game.h"

typedef struct {
  uint8_t len;                      /* segments */
  int8_t curve;                     /* the bend it settles into */
  int8_t hill;                      /* metres up (or down) over the piece */
} piece;

#define P(...) {__VA_ARGS__}
static const piece COAST[] = {
  P(30, 0, 0), P(34, 12, 4), P(16, 0, 0), P(30, 22, -2), P(18, 0, -4), P(26, -16, 3), P(20, 0, 0), P(38, 20, 5),
  P(14, 0, 0), P(24, -14, -4), P(18, 0, -3), P(42, 26, 0), P(20, 0, 2), P(30, -10, 2), P(22, 0, -3), P(40, 18, 0),
  P(26, 0, 0)};
static const piece VALLEY[] = {
  P(28, 0, 0), P(24, 0, 12), P(30, -24, 6), P(20, 0, -14), P(30, 26, -8), P(16, 0, 4), P(28, -20, 10), P(28, 24, 0),
  P(20, 0, -12), P(36, 30, -6), P(24, 0, 8), P(30, -18, 6), P(26, 0, -10), P(44, 22, -4), P(20, 0, 8), P(30, 26, 0),
  P(24, 0, 0)};
static const piece DESERT[] = {
  P(44, 0, 0), P(30, 0, 16), P(26, 0, -16), P(34, 30, 0), P(40, 0, 10), P(20, 0, -10), P(30, -26, 0), P(24, 18, 4),
  P(46, 0, 14), P(24, 0, -22), P(36, 36, 0), P(30, 0, 6), P(28, -22, -2), P(24, 0, 0), P(36, 34, 0), P(22, 0, 0)};
static const piece HARBOR[] = {
  P(40, 0, 0), P(14, 44, 0), P(30, 0, 0), P(14, 44, 0), P(20, 0, 1), P(12, -40, 0), P(16, 0, 0), P(12, 40, 0),
  P(44, 0, -1), P(16, 44, 0), P(26, 0, 0), P(12, -36, 0), P(14, 0, 0), P(12, 36, 0), P(28, 0, 0), P(16, 46, 0),
  P(34, 0, 0)};
static const piece ALPINE[] = {
  P(30, 0, 0), P(30, 16, 18), P(14, 0, 8), P(16, -56, 4), P(20, 0, 14), P(16, 56, 6), P(24, 0, 10), P(30, -28, 0),
  P(20, 0, -16), P(18, 50, -12), P(24, 0, -14), P(16, -50, -8), P(30, 22, -12), P(20, 0, 4), P(36, 40, 6),
  P(24, 0, -8), P(26, -18, 0), P(20, 0, 0), P(30, 34, 0), P(22, 0, 0)};
static const piece METRO[] = {
  P(32, 0, 0), P(18, 34, 0), P(14, 0, 2), P(10, -30, 0), P(10, 30, 0), P(26, 0, 3), P(22, 28, 0), P(30, 0, -3),
  P(14, -38, 0), P(16, 0, -2), P(24, 38, 0), P(14, 0, 0), P(10, 30, 0), P(10, -30, 0), P(24, 0, 2), P(26, 32, 0),
  P(20, 0, 0), P(18, 24, 0), P(22, 0, -2), P(20, 20, 0), P(18, 0, 0)};
#undef P

typedef struct {
  const piece *p;
  uint8_t n, sky, theme;
} circuit;
#define C(a, sky, theme) {a, sizeof a / sizeof a[0], sky, theme}
static const circuit CIRCUITS[TR_TRACKS] = {C(COAST, SKY_COAST, TH_COAST), C(VALLEY, SKY_ALPS, TH_VALLEY),
                                            C(DESERT, SKY_DESERT, TH_DESERT), C(HARBOR, SKY_CITY, TH_HARBOR),
                                            C(ALPINE, SKY_ALPS, TH_ALPINE), C(METRO, SKY_CITY, TH_METRO)};
#undef C
const char *const tr_track_name[TR_TRACKS] = {"Sunset Coast", "Pine Valley", "Red Rock Run",
                                              "Harbor Sprint", "Alpine Pass", "Metro Circuit"};
const char *const tr_track_note[TR_TRACKS] = {"Easy sweeping bends", "Rolling hills", "Fast, with big crests",
                                              "Sharp city corners", "Hairpins and climbs", "Tight and twisty"};

/* how far a bend eases in from the one before */
static int ease_of(int len) { return len / 2 < 8 ? len / 2 : 8; }

static uint32_t isqrt(uint32_t v) {
  uint32_t r = 0, b = 1u << 30;
  while (b > v) b >>= 2;
  while (b) {
    if (v >= r + b) v -= r + b, r = (r >> 1) + b;
    else r >>= 1;
    b >>= 2;
  }
  return r;
}

/* the fastest a plain car takes a bend of this size: the sideways pull v*v*c grows past what the tyres hold */
static int corner_speed(int c) {
  if (c < 0) c = -c;
  if (c <= 18) return 255;
  int v = (int)isqrt((uint32_t)(61 * 61 * 18 / c));
  return v < 255 ? v : 255;
}

void tr_build(tr_track *t, int id) {
  if (id < 0 || id >= TR_TRACKS) id = 0;
  const circuit *c = &CIRCUITS[id];
  t->id = (uint8_t)id, t->sky = c->sky, t->theme = c->theme;
  int n = 0, prev = c->p[c->n - 1].curve;
  int32_t y = 0;
  for (int k = 0; k < c->n && n < TR_MAXSEG; k++) {
    const piece *p = &c->p[k];
    int e = ease_of(p->len);
    for (int i = 0; i < p->len && n < TR_MAXSEG; i++, n++) {
      t->curve[n] = (int8_t)(i < e ? prev + (p->curve - prev) * (i + 1) / (e + 1) : p->curve);
      /* smoothstep: s = 3u^2 - 2u^3, u = i / len (in 1/4096) */
      int64_t u = (int64_t)i * 4096 / p->len, s = (3 * u * u * 4096 - 2 * u * u * u) / (4096 * 4096);
      t->y[n] = (int16_t)(y + (int64_t)p->hill * 100 * s / 4096);
    }
    y += p->hill * 100;
    prev = p->curve;
  }
  t->n = (uint16_t)n;
  /* a lap ends where it began: anything left over (there shouldn't be) is shared out along the way */
  for (int i = 0; i < n; i++) t->y[i] = (int16_t)(t->y[i] - (int64_t)y * i / n);
  t->y[n] = t->y[0];
  t->length = (int32_t)n * TR_SEG * FX;
  /* the corner speeds, braking for what comes (at 10 m/s^2), over the lap twice so the end sees the start; in
     squared speeds, so that nothing is lost to rounding on the way */
  uint32_t v2[TR_MAXSEG];
  for (int i = 0; i < n; i++) v2[i] = (uint32_t)(corner_speed(t->curve[i]) * corner_speed(t->curve[i]));
  for (int pass = 0; pass < 2; pass++)
    for (int i = n - 1; i >= 0; i--) {
      uint32_t v = v2[(i + 1) % n] + 2 * 10 * TR_SEG;
      if (v < v2[i]) v2[i] = v;
    }
  for (int i = 0; i < n; i++) {
    uint32_t v = isqrt(v2[i]);
    t->limit[i] = (uint8_t)(v < 255 ? v : 255);
  }
  /* the heading, scaled so that a lap turns exactly once */
  int64_t total = 0;
  for (int i = 0; i < n; i++) total += t->curve[i];
  if (!total) total = 1;
  int64_t sum = 0;
  for (int i = 0; i <= n; i++) {
    t->heading[i] = (uint16_t)(sum * 65536 / total);
    if (i < n) sum += t->curve[i];
  }
}

int tr_seg_of(const tr_track *t, int32_t z) {
  int32_t per = TR_SEG * FX, s = z >= 0 ? z / per : -((-z + per - 1) / per);
  s %= t->n;
  return s < 0 ? s + t->n : s;
}

int tr_curve_at(const tr_track *t, int32_t z) { return t->curve[tr_seg_of(t, z)]; }

int32_t tr_height_at(const tr_track *t, int32_t z) {
  int32_t per = TR_SEG * FX;
  int s = tr_seg_of(t, z);
  int32_t f = z % per;
  if (f < 0) f += per;
  int32_t a = t->y[s], b = t->y[s + 1];
  return (a * FX + (b - a) * FX * f / per) / 100;
}

int32_t tr_wrap(const tr_track *t, int32_t dz) {
  dz %= t->length;
  if (dz >= t->length / 2) dz -= t->length;
  if (dz < -t->length / 2) dz += t->length;
  return dz;
}

/* ------------------------------------------------------------------ the roadside */
/* heights in metres, and how much of the picture's width stands on the ground (in eighths) */
static const uint8_t TALL[SC_KINDS] = {10, 12, 7, 30, 3, 5, 8, 8};
static const uint8_t BASE[SC_KINDS] = {2, 3, 7, 7, 7, 2, 7, 1};
/* the pictures' sizes (art/scenery), for their widths in metres */
static const uint8_t PIC_W[SC_KINDS] = {80, 73, 80, 64, 47, 57, 80, 43};
static const uint8_t PIC_H[SC_KINDS] = {114, 120, 67, 120, 77, 120, 57, 120};

static uint32_t hash(uint32_t a) {
  a ^= a >> 16, a *= 0x7FEB352Du, a ^= a >> 15, a *= 0x846CA68Bu, a ^= a >> 16;
  return a;
}

static tr_thing thing(int kind, int32_t gap, bool flip, int side) {
  tr_thing o;
  o.kind = (int8_t)kind, o.flip = flip;
  o.height = TALL[kind] * FX;
  int32_t w = o.height * PIC_W[kind] / PIC_H[kind];
  o.half = w * BASE[kind] / 16;
  /* gap: from the road's edge to the thing's nearest side */
  o.x = side * (TR_ROAD + gap + o.half);
  return o;
}

tr_thing tr_scenery(const tr_track *t, int seg, int side) {
  tr_thing none = {-1, false, 0, 0, 0};
  int n = t->n;
  seg = ((seg % n) + n) % n;
  uint32_t h = hash((uint32_t)seg * 2654435761u ^ (uint32_t)(side + 2) * 40503u ^ (uint32_t)t->id * 977u);
  int c = t->curve[seg];
  /* arrows on the outside of a sharp bend, every few segments */
  if ((c >= 26 && side < 0) || (c <= -26 && side > 0)) {
    if (seg % 4 == 0) return thing(SC_SIGN, 2 * FX, c < 0, side);
    return none;
  }
  if (seg % n < 3 || seg % n >= n - 2) return none;     /* the start line stays clear */
  int32_t jitter = (int32_t)(h >> 8 & 255) * 6;        /* up to 6 metres */
  switch (t->theme) {
    case TH_COAST:
      if (seg % 19 == (side > 0 ? 7 : 16)) return thing(SC_VILLA, 9 * FX + jitter, h & 1, side);
      if (seg % 3 == (side > 0 ? 0 : 1)) return thing(SC_PALM, 3 * FX + jitter / 3, h >> 1 & 1, side);
      break;
    case TH_VALLEY:
    case TH_ALPINE:
      if (t->theme == TH_ALPINE && seg % 11 == (side > 0 ? 3 : 8)) return thing(SC_ROCK, 4 * FX + jitter, h & 1, side);
      if (seg % 2 == (side > 0 ? 0 : 1) && (h & 7) != 0) return thing(SC_PINE, 3 * FX + jitter / 2, h >> 3 & 1, side);
      break;
    case TH_DESERT:
      if (seg % 9 == (side > 0 ? 2 : 6)) return thing(SC_ROCK, 5 * FX + jitter * 2, h & 1, side);
      if (seg % 4 == (side > 0 ? 0 : 2) && (h & 3) != 0) return thing(SC_CACTUS, 3 * FX + jitter / 2, h >> 2 & 1, side);
      break;
    case TH_HARBOR:
      if (seg % 6 == 0) return thing(SC_LAMP, 2 * FX, side < 0, side);
      if (seg % 5 == (side > 0 ? 2 : 4)) return thing(SC_TOWER, 8 * FX + jitter, h & 1, side);
      break;
    case TH_METRO:
      if (seg % 5 == 0) return thing(SC_LAMP, 2 * FX, side < 0, side);
      if (seg % 4 == (side > 0 ? 1 : 3)) return thing(SC_TOWER, 5 * FX + jitter / 2, h & 1, side);
      break;
  }
  return none;
}

/* ------------------------------------------------------------------ the outline */
int tr_map(int id, int n, int16_t *xs, int16_t *ys, int w, int h) {
  static tr_track t;   /* (static: big) */
  tr_build(&t, id);
  float px[TR_MAXSEG + 1], py[TR_MAXSEG + 1];
  float x = 0, y = 0;
  for (int i = 0; i <= t.n; i++) {
    px[i] = x, py[i] = y;
    float a = (float)t.heading[i] * (6.2831853f / 65536.f);
    x += sinf(a), y -= cosf(a);
  }
  /* close the loop: what's left over is shared out along the way */
  float ex = px[t.n], ey = py[t.n];
  float x0 = 1e9f, x1 = -1e9f, y0 = 1e9f, y1 = -1e9f;
  for (int i = 0; i <= t.n; i++) {
    px[i] -= ex * (float)i / (float)t.n, py[i] -= ey * (float)i / (float)t.n;
    if (px[i] < x0) x0 = px[i];
    if (px[i] > x1) x1 = px[i];
    if (py[i] < y0) y0 = py[i];
    if (py[i] > y1) y1 = py[i];
  }
  float s = fminf((float)(w - 1) / fmaxf(x1 - x0, 1.f), (float)(h - 1) / fmaxf(y1 - y0, 1.f));
  float ox = ((float)(w - 1) - (x1 - x0) * s) / 2, oy = ((float)(h - 1) - (y1 - y0) * s) / 2;
  for (int k = 0; k < n; k++) {
    int i = k * t.n / n;
    xs[k] = (int16_t)lrintf((px[i] - x0) * s + ox);
    ys[k] = (int16_t)lrintf((py[i] - y0) * s + oy);
  }
  return t.n;
}
