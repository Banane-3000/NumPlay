/* The scenery and what stands on the tracks (scene.h).
 *
 * The ground is not polygons: it is flat, so each row of the screen below the horizon is one distance
 * ahead, and across the row the ground goes by evenly. Its colours across the yard (gravel, the rails,
 * the sleepers) are kept in tables, sharp ones near and blurred ones far (each half as wide as the one
 * before), and a row only reads its table at a steady step: no flicker on the far rails. The sleepers come
 * one row in so many: rows that see part of one get a table with them mixed in. */
#include <string.h>
#include "actors.h"
#include "scene.h"

#define GROUND_W 384               /* entries of the sharpest table: 12 m at 32 a metre */
#define GROUND_X0 (-6.0f)
#define LEVELS 7
#define SLEEPER 0.7f               /* from one sleeper to the next */
#define SLEEPER_W 0.26f
#define WALL_X 5.8f
#define BLOCK 24.0f

enum { G_NONE, G_SLEEPER, G_MIX, G_KINDS };
static uint16_t ground[2][G_KINDS][GROUND_W * 2];   /* outside and in tunnels; levels one after the other */
static const int level_at[LEVELS] = {0, 384, 576, 672, 720, 744, 756};
static uint16_t sky[SCREEN_H];
static int horizon;                /* the first row of ground */

/* each row of ground: its table, where it starts and its step (16.16), its haze */
typedef struct {
  const uint16_t *lut;
  int32_t u, du;
  uint16_t n;
  uint8_t haze;
} GroundRow;
static GroundRow rows[SCREEN_H];

/* ---------------------------------------------------------------- the ground's colours */
static uint32_t ground_at(float x, int kind) {
  uint32_t edge = 0x9C8B78, bed = 0x86735F, ballast = 0x7A6957, wood = 0x5E4433, rail = 0xC9CFD6, rail_side = 0x6E7378;
  uint32_t c = x < -4.2f || x > 4.2f ? edge : bed;
  for (int l = 0; l < 3; l++) {
    float d = x - LANE_X(l);
    if (d < 0) d = -d;
    if (d < 1.3f) c = ballast;
    if (d < 1.12f && kind == G_SLEEPER) c = wood;
    if (d >= 0.70f && d < 0.86f) c = d < 0.73f || d >= 0.83f ? rail_side : rail;
    else if (d >= 0.86f && d < 0.92f && kind != G_SLEEPER) c = 0x4F4338;   /* the rail's shadow */
  }
  /* a little grain */
  uint32_t h = world_hash((uint32_t)((x + 8) * 32)) & 15;
  if (c == bed || c == ballast || c == edge) c = r_mix888(c, h & 1 ? 0xFFFFFF : 0, (int)(h >> 1) * 3);
  return c;
}

static void ground_tables(void) {
  uint32_t acc[GROUND_W];
  for (int env = 0; env < 2; env++)
    for (int kind = 0; kind < G_KINDS; kind++) {
      for (int i = 0; i < GROUND_W; i++) {
        float x = GROUND_X0 + ((float)i + 0.5f) / 32;
        uint32_t c = kind == G_MIX ? r_mix888(ground_at(x, G_NONE), ground_at(x, G_SLEEPER), 95) : ground_at(x, kind);
        if (env) c = r_mix888(r_mix888(c, 0, 150), 0x182434, 40);
        acc[i] = c;
      }
      for (int l = 0; l < LEVELS; l++) {
        int n = GROUND_W >> l;
        for (int i = 0; i < n; i++) ground[env][kind][level_at[l] + i] = r_565(acc[i]);
        /* the next level: each two averaged */
        for (int i = 0; i < n / 2; i++) {
          uint32_t a = acc[2 * i], b = acc[2 * i + 1];
          acc[i] = ((((a >> 16) & 255) + ((b >> 16) & 255)) / 2) << 16 | ((((a >> 8) & 255) + ((b >> 8) & 255)) / 2) << 8 |
                   (((a & 255) + (b & 255)) / 2);
        }
      }
    }
}

void scene_init(void) { ground_tables(); }

float scene_light(float z) { return world_tunnel(z) ? 0.5f : 1.0f; }
uint32_t scene_lit(uint32_t rgb, float z) { return world_tunnel(z) ? r_mix888(r_mix888(rgb, 0, 120), 0x10203A, 30) : rgb; }

/* ---------------------------------------------------------------- the camera, the sky, the ground's rows */
/* the part of [a, b] on sleepers */
static float on_sleepers(float a, float b) {
  float n = 0, s = (float)(int)(a / SLEEPER) * SLEEPER;
  if (a < 0) s -= SLEEPER;
  for (; s < b; s += SLEEPER) {
    float lo = s > a ? s : a, hi = s + SLEEPER_W < b ? s + SLEEPER_W : b;
    if (hi > lo) n += hi - lo;
  }
  return n / (b - a);
}

void scene_camera(float x, float y, float z, float pitch) {
  r_camera(x, y, z, pitch, 250, 160, 112);
  /* the horizon: the row looking level */
  float kh = cam.sp / cam.cp;
  horizon = (int)(cam.cy - kh * cam.f) + 1;
  if (horizon < 0) horizon = 0;
  for (int y = 0; y < SCREEN_H; y++) {
    int t = horizon > 0 ? y * 256 / (horizon + 8) : 256;
    sky[y] = r_565(r_mix888(0x3C9FE8, 0xCFEFFF, t > 256 ? 256 : t));
  }
  for (int y = horizon; y < SCREEN_H; y++) {
    GroundRow *g = &rows[y];
    float k = (cam.cy - ((float)y + 0.5f)) / cam.f, den = k * cam.cp - cam.sp;
    if (den > -1e-4f) {
      g->lut = NULL;
      continue;
    }
    float zc = -cam.y / den;                     /* the distance ahead, from the camera */
    float wz = cam.z + zc * (k * cam.sp + cam.cp);
    float k2 = (cam.cy - ((float)y + 1.5f)) / cam.f, den2 = k2 * cam.cp - cam.sp;
    float wz2 = den2 < -1e-4f ? cam.z + -cam.y / den2 * (k2 * cam.sp + cam.cp) : wz + 100;
    float dz = wz2 - wz;
    if (zc > 400) {
      g->lut = NULL;
      continue;
    }
    int kind;
    if (dz < 0.1f) {
      float s = wz - (float)(int)(wz / SLEEPER) * SLEEPER;
      kind = s < SLEEPER_W ? G_SLEEPER : G_NONE;
    } else if (dz >= SLEEPER) kind = G_MIX;
    else {
      float c = on_sleepers(wz, wz + dz);
      kind = c < 0.25f ? G_NONE : c > 0.75f ? G_SLEEPER : G_MIX;
    }
    float step = zc / cam.f * 32;   /* entries of the sharpest table a pixel */
    int l = 0;
    while (step > 1.4f && l < LEVELS - 1) step *= 0.5f, l++;
    float scale = (float)(32 >> l);
    g->lut = ground[world_tunnel(wz)][kind] + level_at[l];
    g->n = (uint16_t)(GROUND_W >> l);
    g->u = (int32_t)(((cam.x + (0.5f - cam.cx) * zc / cam.f) - GROUND_X0) * scale * 65536);
    g->du = (int32_t)(zc / cam.f * scale * 65536);
    float h = (zc - r_haze0) / (r_haze1 - r_haze0);
    g->haze = (uint8_t)(h <= 0 ? 0 : h >= 1 ? 240 : (int)(h * 240));
  }
}

/* clouds: a few, far away, made of discs */
static void clouds(uint16_t *row, int y) {
  static const int8_t puff[5][3] = {{0, 0, 14}, {-16, 5, 10}, {16, 4, 11}, {-28, 9, 7}, {28, 9, 7}};
  for (int c = 0; c < 4; c++) {
    float cx = (float)((c * 97 + 30) % 340) - 10 - cam.x * 2, cy = (float)(horizon - 46 + (c * 37) % 30);
    for (int p = 0; p < 5; p++) {
      float px = cx + puff[p][0], py = cy + puff[p][1], r = puff[p][2];
      float dy = (float)y + 0.5f - py;
      if (dy * dy >= r * r) continue;
      float w = __builtin_sqrtf(r * r - dy * dy);
      int x0 = (int)(px - w), x1 = (int)(px + w);
      if (x0 < 0) x0 = 0;
      if (x1 > SCREEN_W) x1 = SCREEN_W;
      for (int x = x0; x < x1; x++) row[x] = r_mix565(row[x], 0xFFFF, 26);
    }
  }
}

void scene_background(void) {
  static uint16_t hazed[GROUND_W];
  uint16_t haze = r_565(r_haze_rgb);
  for (int y = r_y0; y < r_y1; y++) {
    uint16_t *p = r_col + (y - r_y0) * SCREEN_W;
    if (y < horizon || !rows[y].lut) {
      uint16_t c = y < horizon ? sky[y] : haze;
      for (int x = 0; x < SCREEN_W; x++) p[x] = c;
      if (y < horizon) clouds(p, y);
      continue;
    }
    const GroundRow *g = &rows[y];
    const uint16_t *lut = g->lut;
    uint32_t n = g->n;
    if (g->haze) {
      int t = (g->haze + 4) >> 3;
      for (uint32_t i = 0; i < n; i++) hazed[i] = r_mix565(lut[i], haze, t);
      lut = hazed;
    }
    uint16_t left = lut[0], right = lut[n - 1];
    int32_t u = g->u, du = g->du;
    /* the pixels left of the table's first entry, those on it, those right of its last */
    int x = 0, xa = 0, xb = SCREEN_W;
    if (u < 0) xa = (int)((-(int64_t)u + du - 1) / du);
    int64_t end = ((int64_t)n << 16) - u;
    if (end <= 0) xb = 0;
    else if ((int64_t)du * SCREEN_W > end) xb = (int)((end + du - 1) / du);
    if (xa > SCREEN_W) xa = SCREEN_W;
    if (xb < xa) xb = xa;
    for (; x < xa; x++) p[x] = left;
    u += du * xa;
    for (; x < xb; x++, u += du) p[x] = lut[(uint32_t)u >> 16];
    for (; x < SCREEN_W; x++) p[x] = right;
  }
}

/* ---------------------------------------------------------------- polygons */
static uint32_t lit;      /* 256: full light (outside), less in tunnels */
static uint32_t L(uint32_t c) { return lit >= 256 ? c : r_mix888(r_mix888(c, 0, (int)(256 - lit)), 0x10203A, 24); }

static void quad(V3 a, V3 b, V3 c, V3 d, uint32_t rgb, int flags) {
  V3 v[4] = {a, b, c, d};
  r_poly(v, 4, rgb, flags, 0);
}
/* a wall at x facing +x (dir 1) or -x (dir -1), from (z0, y0) to (z1, y1) */
static void side(float x, int dir, float z0, float z1, float y0, float y1, uint32_t rgb, int flags) {
  V3 n = {(float)dir, 0, 0};
  uint32_t c = flags & P_NOHAZE ? rgb : r_shade(rgb, n);
  if (dir > 0) quad((V3){x, y1, z0}, (V3){x, y1, z1}, (V3){x, y0, z1}, (V3){x, y0, z0}, c, flags);
  else quad((V3){x, y1, z1}, (V3){x, y1, z0}, (V3){x, y0, z0}, (V3){x, y0, z1}, c, flags);
}
/* facing the camera (-z), from (x0, y0) to (x1, y1) */
static void front(float z, float x0, float x1, float y0, float y1, uint32_t rgb, int flags) {
  uint32_t c = flags & P_NOHAZE ? rgb : r_shade(rgb, (V3){0, 0, -1});
  quad((V3){x0, y1, z}, (V3){x1, y1, z}, (V3){x1, y0, z}, (V3){x0, y0, z}, c, flags);
}
static void ceiling(float y, float x0, float x1, float z0, float z1, uint32_t rgb) {
  quad((V3){x1, y, z1}, (V3){x0, y, z1}, (V3){x0, y, z0}, (V3){x1, y, z0}, rgb, 0);
}
static void cube(float x0, float y0, float z0, float x1, float y1, float z1, uint32_t rgb, int skip) {
  r_cube(x0, y0, z0, x1, y1, z1, L(rgb), skip);
}

/* ---------------------------------------------------------------- graffiti */
static const uint32_t spray[8] = {0xFF4FA3, 0xFFD23F, 0x3FD0FF, 0x6BE04A, 0xFF8A2B, 0x9B5CFF, 0xFF3B3B, 0xF4F4F4};

/* a 2D polygon (u along the wall, v up) put on the wall at x facing dir; mirrored on the right so that
 * it reads the same way round */
static void on_wall(float x, int dir, float z0, float w, const float *uv, int n, uint32_t rgb, int flags) {
  V3 v[8];
  for (int i = 0; i < n; i++) {
    float u = uv[2 * i];
    v[i] = (V3){x, uv[2 * i + 1], dir > 0 ? z0 + u : z0 + w - u};
  }
  if (dir < 0)
    for (int i = 0; i < n / 2; i++) {
      V3 t = v[i];
      v[i] = v[n - 1 - i], v[n - 1 - i] = t;
    }
  r_poly(v, n, L(r_shade(rgb, (V3){(float)dir, 0, 0})), flags, 0);
}

/* letters for the tags, on a grid 3 wide and 5 tall: their strokes as boxes (x0, y0, x1, y1) */
typedef struct {
  char c;
  uint8_t n;
  uint8_t bar[5][4];
} Letter;
static const Letter letters[] = {
  {'S', 5, {{0, 4, 3, 5}, {0, 2, 1, 5}, {0, 2, 3, 3}, {2, 0, 3, 3}, {0, 0, 3, 1}}},
  {'U', 3, {{0, 0, 1, 5}, {2, 0, 3, 5}, {0, 0, 3, 1}}},
  {'B', 5, {{0, 0, 1, 5}, {0, 4, 2, 5}, {0, 2, 3, 3}, {0, 0, 3, 1}, {2, 0, 3, 5}}},
  {'W', 3, {{0, 0, 1, 5}, {2, 0, 3, 5}, {1, 0, 2, 3}}},
  {'A', 4, {{0, 0, 1, 5}, {2, 0, 3, 5}, {0, 4, 3, 5}, {0, 2, 3, 3}}},
  {'Y', 3, {{0, 2, 1, 5}, {2, 2, 3, 5}, {0, 2, 3, 3}}},
  {'R', 5, {{0, 0, 1, 5}, {0, 4, 3, 5}, {2, 2, 3, 5}, {0, 2, 3, 3}, {2, 0, 3, 2}}},
  {'F', 3, {{0, 0, 1, 5}, {0, 4, 3, 5}, {0, 2, 2, 3}}},
  {'E', 4, {{0, 0, 1, 5}, {0, 4, 3, 5}, {0, 2, 2, 3}, {0, 0, 3, 1}}},
  {'J', 3, {{2, 0, 3, 5}, {0, 0, 3, 1}, {0, 0, 1, 2}}},
  {'K', 4, {{0, 0, 1, 5}, {0, 2, 2, 3}, {2, 3, 3, 5}, {2, 0, 3, 2}}},
  {'O', 4, {{0, 0, 1, 5}, {2, 0, 3, 5}, {0, 4, 3, 5}, {0, 0, 3, 1}}},
  {'N', 3, {{0, 0, 1, 5}, {2, 0, 3, 5}, {0, 4, 3, 5}}},
  {'C', 3, {{0, 0, 1, 5}, {0, 4, 3, 5}, {0, 0, 3, 1}}},
  {'L', 2, {{0, 0, 1, 5}, {0, 0, 3, 1}}},
  {'T', 2, {{0, 4, 3, 5}, {1, 0, 2, 5}}},
  {'H', 3, {{0, 0, 1, 5}, {2, 0, 3, 5}, {0, 2, 3, 3}}},
  {'I', 1, {{1, 0, 2, 5}}},
  {'D', 4, {{0, 0, 1, 5}, {0, 4, 2, 5}, {0, 0, 2, 1}, {2, 1, 3, 4}}},
  {'P', 4, {{0, 0, 1, 5}, {0, 4, 3, 5}, {2, 2, 3, 5}, {0, 2, 3, 3}}},
  {'G', 5, {{0, 0, 1, 5}, {0, 4, 3, 5}, {0, 0, 3, 1}, {2, 0, 3, 3}, {2, 2, 3, 3}}},
  {'!', 2, {{1, 2, 2, 5}, {1, 0, 2, 1}}},
};
static const char *const words[] = {"SURF", "SUBWAY", "JAKE", "YO!", "RUN", "COOL", "WOW", "FRESH", "TRICKY", "SPIKE",
                                    "HEY", "RAD", "GO!", "FUN", "TAG", "HOP", "BOLT", "YEAH", "PARK", "DASH"};

static const Letter *letter(char c) {
  for (unsigned i = 0; i < sizeof letters / sizeof letters[0]; i++)
    if (letters[i].c == c) return &letters[i];
  return NULL;
}

/* a piece of graffiti, w metres wide and h tall, from its number: a word in bubble-bright letters leaning
 * forward, with a shadow, on a cloud of another colour */
static void graffiti(float x, int dir, float z0, float w, float h, uint32_t seed, float dist) {
  uint32_t s = world_hash(seed);
  const char *word = words[s % (sizeof words / sizeof words[0])];
  int n = 0;
  while (word[n]) n++;
  uint32_t cloud = (s >> 5) & 1 ? 0x1D1D2B : 0xF8F8F8, shadow = (s >> 5) & 1 ? 0x0A0A12 : 0x30304A;
  uint32_t fill = spray[(s >> 8) % 7], second = spray[((s >> 8) % 7 + 1 + (s >> 12) % 5) % 7];
  /* the letters' size: the word fills the width, at most the height */
  float cell = w / ((float)n * 4.2f + 1), lh = cell * 5;
  if (lh > h * 0.7f) lh = h * 0.7f, cell = lh / 5;
  float ww = cell * ((float)n * 4.2f - 1.2f), u0 = (w - ww) / 2, v0 = (h - lh) / 2 + 0.25f, lean = 0.22f;
  /* the cloud behind: a rounded box */
  float cu0 = u0 - cell * 1.1f, cu1 = u0 + ww + cell * 1.1f + lean * lh, cv0 = v0 - cell * 1.2f, cv1 = v0 + lh + cell * 1.1f, r = cell * 1.2f;
  float c[16] = {cu0 + r, cv1, cu1 - r, cv1, cu1, cv1 - r, cu1, cv0 + r, cu1 - r, cv0, cu0 + r, cv0, cu0, cv0 + r, cu0, cv1 - r};
  on_wall(x, dir, z0, w, c, 8, cloud, P_DECAL);
  if (dist > 85) return;
  for (int i = 0; i < n; i++) {
    const Letter *L = letter(word[i]);
    if (!L) continue;
    float lu = u0 + (float)i * cell * 4.2f;
    uint32_t col = i & 1 ? second : fill;
    for (int b = 0; b < L->n; b++) {
      float x0 = lu + L->bar[b][0] * cell, x1 = lu + L->bar[b][2] * cell;
      float y0 = v0 + L->bar[b][1] * cell, y1 = v0 + L->bar[b][3] * cell;
      /* leaning: the higher, the further along */
      float q[8] = {x0 + lean * (y1 - v0), y1, x1 + lean * (y1 - v0), y1, x1 + lean * (y0 - v0), y0, x0 + lean * (y0 - v0), y0};
      if (dist < 50) {
        float d = cell * 0.35f;
        float sh[8] = {q[0] + d, q[1] - d, q[2] + d, q[3] - d, q[4] + d, q[5] - d, q[6] + d, q[7] - d};
        on_wall(x, dir, z0, w, sh, 4, shadow, P_DECAL2);
      }
      on_wall(x, dir, z0, w, q, 4, col, P_DECAL3);
    }
  }
}

/* ---------------------------------------------------------------- the yard */
static const uint32_t house[8] = {0xC4553A, 0xE8C9A0, 0x5E8FC4, 0xF2E6C8, 0x3FA6A0, 0xE8893A, 0xE89AB0, 0xA8ADB3};

static void building(int dir, float z0, float z1, float near_x, float depth, float h, uint32_t col, float dist) {
  float x0 = dir > 0 ? -near_x - depth : near_x, x1 = dir > 0 ? -near_x : near_x + depth;
  cube(x0, 0, z0, x1, h, z1, col, 2);
  if (dist > 110) return;
  /* windows, floor by floor, on the side facing the tracks and the end facing the camera */
  uint32_t glass = L(0x2C4A6E);
  float wx = dir > 0 ? x1 : x0;
  for (float y = 3.6f; y + 1.6f < h - 0.8f; y += 3.2f) {
    side(wx, dir, z0 + 0.8f, z1 - 0.8f, y, y + 1.5f, glass, P_DECAL);
    if (dist < 70) front(z0, x0 + 0.8f, x1 - 0.8f, y, y + 1.5f, glass, P_DECAL);
  }
}

static void open_block(int b, float z0, float dist) {
  uint32_t h = world_hash((uint32_t)b * 7919u + 3 + world_seed);
  bool before_tunnel = world_tunnel(z0 + BLOCK + 1);
  for (int dir = -1; dir <= 1; dir += 2) {
    uint32_t hs = world_hash(h + (uint32_t)(dir + 1));
    float wx = dir > 0 ? -WALL_X : WALL_X;   /* the wall's face towards the tracks: the left one faces +x */
    float wh = 3.4f + (float)(hs % 3) * 0.4f;
    static const uint32_t walls[6] = {0xB9B1A3, 0xA9A39A, 0xA65A42, 0xC9BFA8, 0x8E9AA3, 0xB46B4E};
    uint32_t wc = walls[(hs >> 4) % 6];
    float outer = dir > 0 ? wx - 0.5f : wx + 0.5f;
    cube(dir > 0 ? outer : wx, 0, z0, dir > 0 ? wx : outer, wh, z0 + BLOCK, wc, 2 | (dir > 0 ? 4 : 8));
    /* pillars */
    for (int k = 0; k < 2; k++) {
      float pz = z0 + (float)k * 12;
      float px0 = dir > 0 ? wx - 0.2f : wx - 0.25f, px1 = dir > 0 ? wx + 0.25f : wx + 0.2f;
      cube(px0, 0, pz, px1, wh + 0.3f, pz + 0.7f, 0x8E877D, 2);
    }
    if (dist < 110) {
      for (int k = 0; k < 2; k++) {
        uint32_t g = world_hash(hs + (uint32_t)k * 31);
        if (g % 4 == 0) continue;
        float gw = 7 + (float)(g >> 8 & 3), gz = z0 + 1.2f + (float)k * 12;
        graffiti(wx + (dir > 0 ? 0.01f : -0.01f), dir, gz, gw > 9.8f ? 9.8f : gw, wh - 0.4f, g, dist + (float)k * 12);
      }
    }
    /* the buildings behind (not right before a tunnel: its face is there) */
    for (int k = 0; k < 2 && !before_tunnel; k++) {
      uint32_t g = world_hash(hs * 3 + (uint32_t)k);
      float bh = 9 + (float)(g % 16), depth = 8 + (float)(g >> 4 & 7);
      float bz = z0 + (float)k * 12 + 0.6f;
      building(dir, bz, bz + 10.8f, 7.5f + (float)(g >> 8 & 3), depth, bh, house[(g >> 12) & 7], dist);
      if ((g >> 16) % 3 == 0 && dist < 80) {
        /* a water tank on the roof */
        float cx = dir > 0 ? -12 : 12;
        cube(cx - 1.2f, bh, bz + 3, cx + 1.2f, bh + 2.2f, bz + 5.4f, 0x8A5A3C, 2);
      }
    }
  }
  /* a gantry with signals over the tracks, now and then */
  if (h % 4 == 0 && !before_tunnel) {
    float gz = z0 + 6;
    cube(-5.2f, 0, gz, -4.8f, 7.6f, gz + 0.4f, 0x6B7A86, 2);
    cube(4.8f, 0, gz, 5.2f, 7.6f, gz + 0.4f, 0x6B7A86, 2);
    cube(-5.4f, 7.0f, gz - 0.1f, 5.4f, 7.6f, gz + 0.5f, 0x7D8B96, 0);
    for (int l = 0; l < 3; l++) {
      float lx = LANE_X(l);
      cube(lx - 0.3f, 5.9f, gz - 0.2f, lx + 0.3f, 7.0f, gz + 0.2f, 0x2A2D33, 2);
      uint32_t lamp = (h >> (4 + l)) & 1 ? 0xFF3B30 : 0x3CFF6A;
      front(gz - 0.21f, lx - 0.15f, lx + 0.15f, 6.5f, 6.8f, L(lamp), P_DECAL | P_NOHAZE);
    }
  }
}

static void tunnel_block(float z0, float dist) {
  bool first = !world_tunnel(z0 - 1);
  uint32_t wall = 0x5A5F66, dark = 0x3A3E44;
  side(-WALL_X, 1, z0, z0 + BLOCK, 0, 7.2f, scene_lit(wall, z0), 0);
  side(WALL_X, -1, z0, z0 + BLOCK, 0, 7.2f, scene_lit(wall, z0), 0);
  ceiling(7.2f, -WALL_X, WALL_X, z0, z0 + BLOCK, scene_lit(dark, z0));
  if (dist < 120)
    for (int k = 0; k < 3; k++) {
      float lz = z0 + 4 + (float)k * 8;
      side(-WALL_X + 0.01f, 1, lz, lz + 1.2f, 4.6f, 5.0f, 0xFFF2B0, P_DECAL | P_NOHAZE);
      side(WALL_X - 0.01f, -1, lz, lz + 1.2f, 4.6f, 5.0f, 0xFFF2B0, P_DECAL | P_NOHAZE);
      side(-WALL_X + 0.01f, 1, lz - 2, lz - 1.6f, 0, 7.2f, scene_lit(dark, z0), P_DECAL);
      side(WALL_X - 0.01f, -1, lz - 2, lz - 1.6f, 0, 7.2f, scene_lit(dark, z0), P_DECAL);
    }
  if (first) {
    /* the way in: a big face of brick round the opening, with a frame */
    uint32_t brick = 0x9A5A44, trim = 0x6E6A64, top = 0x7E4636;
    front(z0, -60, -WALL_X, 0, 24, brick, 0);
    front(z0, WALL_X, 60, 0, 24, brick, 0);
    front(z0, -WALL_X, WALL_X, 7.2f, 24, brick, 0);
    /* a band of darker brick on top, and a frame round the opening */
    front(z0, -60, 60, 21, 24, top, P_DECAL);
    front(z0, -WALL_X - 0.9f, -WALL_X, 0, 8.1f, trim, P_DECAL);
    front(z0, WALL_X, WALL_X + 0.9f, 0, 8.1f, trim, P_DECAL);
    front(z0, -WALL_X - 0.9f, WALL_X + 0.9f, 7.2f, 8.1f, trim, P_DECAL);
    /* the subway's sign over it */
    front(z0, -3.0f, 3.0f, 10.0f, 12.4f, 0x1A1A22, P_DECAL);
    front(z0, -2.8f, 2.8f, 10.2f, 12.2f, 0x2E5FA8, P_DECAL2);
    for (int i = 0; i < 3; i++) front(z0, -2.2f + 1.6f * (float)i, -1.2f + 1.6f * (float)i, 10.7f, 11.7f, 0xF4F4F4, P_DECAL3);
    if (dist < 100)
      for (int i = 0; i < 6; i++) {
        /* lamps along the top */
        float x = -15 + 6 * (float)i;
        front(z0, x - 0.4f, x + 0.4f, 18, 18.8f, 0xFFF2B0, P_DECAL | P_NOHAZE);
      }
  }
}

/* ---------------------------------------------------------------- trains, barriers, coins */
static const uint32_t livery[8] = {0x9EC3DD, 0xD8433A, 0xF2C230, 0x4FAE5B, 0x2FB5B5, 0xE4E7EA, 0xF07F2D, 0x6D7BD8};
static const uint32_t stripe[8] = {0xF07F2D, 0xF4F4F4, 0x2E5FA8, 0xF2C230, 0xF4F4F4, 0xD8433A, 0x2E2E36, 0xF2C230};

static void car(float lx, float z0, float z1, int look, bool head, bool moving, float dist) {
  uint32_t body = livery[look & 7], band = stripe[look & 7];
  float x0 = lx - TRAIN_W, x1 = lx + TRAIN_W;
  cube(x0, 0.5f, z0, x1, 3.3f, z1, body, 2);
  cube(x0 + 0.1f, 3.3f, z0 + 0.25f, x1 - 0.1f, TRAIN_H, z1 - 0.25f, r_mix888(body, 0xBFC4C9, 150), 2);
  cube(lx - 0.85f, 0.18f, z0 + 0.6f, lx + 0.85f, 0.5f, z1 - 0.6f, 0x2B2B30, 1 | 2);
  cube(lx - 1.0f, 0.0f, z0 + 0.8f, lx + 1.0f, 0.55f, z0 + 3.0f, 0x232327, 1 | 2);
  cube(lx - 1.0f, 0.0f, z1 - 3.0f, lx + 1.0f, 0.55f, z1 - 0.8f, 0x232327, 1 | 2);
  if (dist > 120) return;
  uint32_t glass = L(0x2B4260), door = L(r_mix888(body, 0, 60)), b = L(band);
  for (int s = -1; s <= 1; s += 2) {
    float x = s < 0 ? x0 - 0.005f : x1 + 0.005f;
    int dir = s;
    side(x, dir, z0 + 0.2f, z1 - 0.2f, 1.25f, 1.55f, b, P_DECAL);
    float len = z1 - z0;
    /* two doors, windows between */
    for (int d = 0; d < 2; d++) {
      float dz = z0 + len * (d ? 0.68f : 0.22f);
      side(x, dir, dz, dz + 1.3f, 0.6f, 3.0f, door, P_DECAL);
      side(x, dir, dz + 0.2f, dz + 1.1f, 1.9f, 2.8f, glass, P_DECAL2);
    }
    if (dist < 80)
      for (int w = 0; w < 4; w++) {
        float wz = z0 + len * (0.05f + 0.115f * (float)w) + (w >= 2 ? len * 0.36f : 0);
        side(x, dir, wz, wz + len * 0.09f, 1.95f, 2.8f, glass, P_DECAL);
      }
    if (dist < 70 && (look >> 1) % 3 == 0) graffiti(x, dir, z0 + len * 0.33f, len * 0.3f, 1.25f, (uint32_t)look * 13 + (uint32_t)(z0 * 3), dist);
  }
  if (head) {
    /* the cab's face */
    uint32_t face = (look & 3) == 2 ? 0x2E5FA8 : 0xF2C230;
    front(z0 - 0.005f, x0 + 0.05f, x1 - 0.05f, 0.6f, 3.25f, L(face), P_DECAL);
    front(z0 - 0.01f, x0 + 0.25f, lx - 0.08f, 1.9f, 2.9f, glass, P_DECAL2);
    front(z0 - 0.01f, lx + 0.08f, x1 - 0.25f, 1.9f, 2.9f, glass, P_DECAL2);
    uint32_t lamp = moving ? 0xFFFFE0 : 0xD8D2B0;
    front(z0 - 0.01f, x0 + 0.25f, x0 + 0.65f, 0.95f, 1.3f, moving ? lamp : L(lamp), P_DECAL2 | (moving ? P_NOHAZE : 0));
    front(z0 - 0.01f, x1 - 0.65f, x1 - 0.25f, 0.95f, 1.3f, moving ? lamp : L(lamp), P_DECAL2 | (moving ? P_NOHAZE : 0));
    front(z0 - 0.01f, lx - 0.7f, lx + 0.7f, 3.0f, 3.2f, L(0x1E1E24), P_DECAL2);
    if (moving) {
      /* the headlights' glow */
      V3 g[4] = {{x0 - 0.6f, 2.0f, z0 - 0.3f}, {x1 + 0.6f, 2.0f, z0 - 0.3f}, {x1 + 0.6f, 0.2f, z0 - 0.3f}, {x0 - 0.6f, 0.2f, z0 - 0.3f}};
      r_poly(g, 4, 0xFFF6C0, P_ALPHA | P_NOHAZE, 70);
    }
  }
}

static void ramp(float lx, float z0, float z1) {
  uint32_t steel = L(0x8D969E), dark = L(0x4A5058), yellow = L(0xF2C230);
  float x0 = lx - 1.05f, x1 = lx + 1.05f, h = TRAIN_H;
  V3 slope = {0, (z1 - z0), -h};   /* its normal, up and towards the camera */
  float l = __builtin_sqrtf(slope.y * slope.y + slope.z * slope.z);
  slope.y /= l, slope.z /= l;
  quad((V3){x0, h, z1}, (V3){x1, h, z1}, (V3){x1, 0, z0}, (V3){x0, 0, z0}, r_shade(steel, slope), 0);
  /* yellow edges and ribs */
  quad((V3){x0, h, z1}, (V3){x0 + 0.18f, h, z1}, (V3){x0 + 0.18f, 0, z0}, (V3){x0, 0, z0}, r_shade(yellow, slope), P_DECAL);
  quad((V3){x1 - 0.18f, h, z1}, (V3){x1, h, z1}, (V3){x1, 0, z0}, (V3){x1 - 0.18f, 0, z0}, r_shade(yellow, slope), P_DECAL);
  for (int i = 1; i < 6; i++) {
    float t = (float)i / 6, t2 = t + 0.025f;
    float za = z0 + (z1 - z0) * t, zb = z0 + (z1 - z0) * t2;
    quad((V3){x0 + 0.18f, h * t2, zb}, (V3){x1 - 0.18f, h * t2, zb}, (V3){x1 - 0.18f, h * t, za}, (V3){x0 + 0.18f, h * t, za},
         r_shade(dark, slope), P_DECAL);
  }
  /* the sides: triangles */
  V3 a[3] = {{x0, 0, z0}, {x0, h, z1}, {x0, 0, z1}};
  V3 b[3] = {{x1, 0, z1}, {x1, h, z1}, {x1, 0, z0}};
  r_poly(a, 3, r_shade(dark, (V3){-1, 0, 0}), 0, 0);
  r_poly(b, 3, r_shade(dark, (V3){1, 0, 0}), 0, 0);
}

/* red and white stripes across a board facing the camera */
static void striped(float z, float x0, float x1, float y0, float y1) {
  front(z, x0, x1, y0, y1, L(0xF4F4F4), 0);
  float w = (x1 - x0) / 5;
  for (int i = 0; i < 5; i += 2) front(z - 0.005f, x0 + w * (float)i, x0 + w * (float)(i + 1), y0, y1, L(0xE23B2E), P_DECAL);
}

static void barrier(const Obstacle *o) {
  float lx = LANE_X(o->lane), z = o->z0;
  if (o->type == O_HURDLE) {
    cube(lx - 1.0f, 0, z + 0.05f, lx - 0.86f, 1.15f, z + 0.2f, 0xDADDE0, 2);
    cube(lx + 0.86f, 0, z + 0.05f, lx + 1.0f, 1.15f, z + 0.2f, 0xDADDE0, 2);
    cube(lx - 1.1f, 0.62f, z, lx + 1.1f, 1.15f, z + 0.12f, 0xF4F4F4, 2);
    striped(z - 0.003f, lx - 1.1f, lx + 1.1f, 0.62f, 1.15f);
    /* feet */
    cube(lx - 1.05f, 0, z - 0.25f, lx - 0.8f, 0.08f, z + 0.5f, 0x8A8F95, 2);
    cube(lx + 0.8f, 0, z - 0.25f, lx + 1.05f, 0.08f, z + 0.5f, 0x8A8F95, 2);
  } else if (o->type == O_HIGHBAR) {
    cube(lx - 1.12f, 0, z + 0.05f, lx - 0.96f, 2.45f, z + 0.22f, 0xDADDE0, 2);
    cube(lx + 0.96f, 0, z + 0.05f, lx + 1.12f, 2.45f, z + 0.22f, 0xDADDE0, 2);
    cube(lx - 1.12f, 1.4f, z, lx + 1.12f, 2.35f, z + 0.12f, 0xF4F4F4, 2);
    striped(z - 0.003f, lx - 1.12f, lx + 1.12f, 1.4f, 1.75f);
    /* a sign: an arrow down */
    front(z - 0.003f, lx - 0.5f, lx + 0.5f, 1.8f, 2.3f, L(0x2E5FA8), P_DECAL);
    V3 arrow[3] = {{lx - 0.3f, 2.2f, z - 0.006f}, {lx + 0.3f, 2.2f, z - 0.006f}, {lx, 1.86f, z - 0.006f}};
    r_poly(arrow, 3, L(0xF4F4F4), P_DECAL2, 0);
  } else {
    /* the big one: yellow and black, lamps on top */
    cube(lx - 1.15f, 0, z, lx + 1.15f, 2.6f, z + 0.45f, 0xF2C230, 2);
    for (int i = 0; i < 4; i++) {
      float x = lx - 1.15f + 0.575f * (float)i;
      V3 chev[4] = {{x + 0.1f, 2.4f, z - 0.004f}, {x + 0.4f, 2.4f, z - 0.004f}, {x + 0.4f, 0.3f, z - 0.004f}, {x + 0.1f, 0.3f, z - 0.004f}};
      r_poly(chev, 4, L(r_shade(0x232327, (V3){0, 0, -1})), P_DECAL, 0);
    }
    cube(lx - 0.9f, 2.6f, z + 0.1f, lx - 0.5f, 2.95f, z + 0.35f, 0xFF3B30, 2);
    cube(lx + 0.5f, 2.6f, z + 0.1f, lx + 0.9f, 2.95f, z + 0.35f, 0xFF3B30, 2);
  }
}

/* a coin: a gold disc turning about its upright axis */
static void coin(const Item *it, float t) {
  float s, c, r = 0.38f;
  r_sincos(t * 5 + it->z * 0.6f, &s, &c);
  static const float k[8][2] = {{0, 1}, {0.707f, 0.707f}, {1, 0}, {0.707f, -0.707f}, {0, -1}, {-0.707f, -0.707f}, {-1, 0}, {-0.707f, 0.707f}};
  V3 v[8], w[8];
  float lgt = scene_light(it->z);
  uint32_t gold = 0xFFC21F, face = 0xFFE26A;
  if (lgt < 1) gold = scene_lit(gold, it->z), face = scene_lit(face, it->z);
  for (int i = 0; i < 8; i++) {
    float u = k[i][0] * r, y = k[i][1] * r;
    v[i] = (V3){it->x + c * u, it->y + y, it->z + s * u};
    w[i] = (V3){it->x + c * u * 0.62f, it->y + y * 0.62f, it->z + s * u * 0.62f};
  }
  r_poly(v, 8, r_shade(gold, (V3){0, 0.3f, -0.95f}), P_TWOSIDED, 0);
  r_poly(w, 8, face, P_TWOSIDED | P_DECAL, 0);
}

void scene_draw(float t) {
  float zfar = cam.z + 175;
  int b0 = (int)(cam.z / BLOCK) - (cam.z < 0), b1 = (int)(zfar / BLOCK);
  for (int b = b1; b >= b0; b--) {
    float z0 = (float)b * BLOCK, dist = z0 - cam.z;
    if (world_tunnel(z0 + 1)) {
      lit = 256;
      tunnel_block(z0, dist);
    } else {
      lit = 256;
      open_block(b, z0, dist);
    }
  }
  for (int i = 0; i < nobs; i++) {
    const Obstacle *o = &obs[i];
    if (o->z0 > zfar || o->z1 < cam.z) continue;
    lit = world_tunnel(o->z0) ? 128 : 256;
    float dist = o->z0 - cam.z;
    if (o->type == O_TRAIN || o->type == O_MOVING) {
      float z = o->z0;
      for (int c = 0; c < o->cars; c++, z += CAR_LEN + CAR_GAP) {
        if (z > zfar) break;
        car(LANE_X(o->lane), z, z + CAR_LEN, o->look, c == 0, o->type == O_MOVING, z - cam.z);
        if (c + 1 < o->cars) cube(LANE_X(o->lane) - 0.4f, 1.0f, z + CAR_LEN, LANE_X(o->lane) + 0.4f, 2.8f, z + CAR_LEN + CAR_GAP, 0x2B2B30, 1 | 2);
      }
    } else if (o->type == O_RAMP) ramp(LANE_X(o->lane), o->z0, o->z1);
    else barrier(o);
    (void)dist;
  }
  for (int i = 0; i < nitems; i++) {
    const Item *it = &items[i];
    if (it->z > cam.z + 120 || it->z < cam.z + 1) continue;
    if (it->kind == I_COIN) coin(it, t);
    else actors_pickup(it, t);
  }
  lit = 256;
}
