/* The scenery and what stands on the tracks (scene.h).
 *
 * The ground is not polygons: it is flat, so each row of the screen below the horizon is one distance
 * ahead, and across the row the ground goes by evenly. Its colours across the yard (gravel, the rails,
 * the sleepers) are kept in tables, sharp ones near and blurred ones far (each half as wide as the one
 * before), and a row only reads its table at a steady step: no flicker on the far rails. The sleepers come
 * one row in so many: rows that see part of one get a table with them mixed in. */
#include "actors.h"
#include "data.h"
#include "scene.h"

#define GROUND_W 384               /* entries of the sharpest table: 12 m at 32 a metre */
#define GROUND_X0 (-6.0f)
#define LEVELS 7
#define SLEEPER 0.7f               /* from one sleeper to the next */
#define SLEEPER_W 0.26f
#define WALL_X 5.8f
#define BLOCK 24.0f
#define ROOF 8.5f                  /* a tunnel's ceiling */

enum { G_NONE, G_SLEEPER, G_MIX, G_KINDS };
static uint16_t ground[2][G_KINDS][GROUND_W * 2];   /* outside and in tunnels; levels one after the other */
static const int level_at[LEVELS] = {0, 384, 576, 672, 720, 744, 756};
static uint16_t sky[SCREEN_H];
/* how far the details go: on the slower calculators (N0110, N0115) they stop sooner, and the haze comes nearer */
static float detail = 1;
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
  uint32_t edge = 0x9A6A48, bed = 0x8E6E52, ballast = 0x7C624C, wood = 0x5A3A26, rail = 0xBCC0C6, rail_side = 0x55585E;
  uint32_t c = x < -4.2f || x > 4.2f ? edge : bed;
  for (int l = 0; l < 3; l++) {
    float d = x - LANE_X(l);
    if (d < 0) d = -d;
    if (d < 1.3f) c = ballast;
    if (d < 1.12f && kind == G_SLEEPER) c = wood;
    if (d >= 0.70f && d < 0.86f) c = d < 0.73f || d >= 0.83f ? rail_side : rail;
    else if (d >= 0.86f && d < 0.92f && kind != G_SLEEPER) c = 0x5A4434;   /* the rail's shadow */
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

void scene_init(void) {
  ground_tables();
  if (plat_slow()) {
    detail = 0.65f;
    r_haze0 *= detail, r_haze1 *= detail;
  }
}

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

/* ---------------------------------------------------------------- textured faces */
/* a rectangle's corners a, b, c, d (clockwise as the camera sees them) with the picture's u0..u1 from a
 * to b and v0..v1 from a to d, in texels (they repeat) */
static void tquad(V3 a, V3 b, V3 c, V3 d, int tex, float u0, float v0, float u1, float v1, int flags) {
  V3 v[4] = {a, b, c, d};
  float uv[8] = {u0, v0, u1, v0, u1, v1, u0, v1};
  r_poly_tex(v, uv, 4, tex, (int)lit, flags);
}
/* a textured wall at x facing +x (dir 1) or -x (dir -1), from (z0, y0) to (z1, y1), u along z and v
 * down: it reads the right way round from the tracks */
static void side_tex(float x, int dir, float z0, float z1, float y0, float y1, int tex, float u0, float u1, float v0, float v1,
                     int flags) {
  uint32_t keep = lit;
  if (dir > 0) tquad((V3){x, y1, z0}, (V3){x, y1, z1}, (V3){x, y0, z1}, (V3){x, y0, z0}, tex, u0, v0, u1, v1, flags);
  else tquad((V3){x, y1, z1}, (V3){x, y1, z0}, (V3){x, y0, z0}, (V3){x, y0, z1}, tex, u0, v0, u1, v1, flags);
  lit = keep;
}
/* facing the camera (-z) */
static void front_tex(float z, float x0, float x1, float y0, float y1, int tex, float u0, float u1, float v0, float v1, int flags) {
  uint32_t keep = lit;
  tquad((V3){x0, y1, z}, (V3){x1, y1, z}, (V3){x1, y0, z}, (V3){x0, y0, z}, tex, u0, v0, u1, v1, flags);
  lit = keep;
}
/* a picture standing up, facing the camera, see-through where it is (trees) */
static void billboard(float x, float y, float z, float hw, float h, int tex, int size) {
  tquad((V3){x - hw, y + h, z}, (V3){x + hw, y + h, z}, (V3){x + hw, y, z}, (V3){x - hw, y, z}, tex, 0, 0, (float)size,
        (float)size, P_TWOSIDED);
}

/* ---------------------------------------------------------------- the yard */
static const uint32_t house[8] = {0xC4553A, 0xE8C9A0, 0x5E8FC4, 0xF2E6C8, 0x3FA6A0, 0xE8893A, 0xE89AB0, 0xA8ADB3};

static void building(int dir, float z0, float z1, float near_x, float depth, float h, uint32_t col, float dist) {
  float x0 = dir > 0 ? -near_x - depth : near_x, x1 = dir > 0 ? -near_x : near_x + depth;
  cube(x0, 0, z0, x1, h, z1, col, 2);
  if (dist > 110 * detail) return;
  /* windows, floor by floor, on the side facing the tracks and the end facing the camera */
  uint32_t glass = L(0x2C4A6E), sill = L(0xF4EEDC);
  float wx = dir > 0 ? x1 : x0;
  for (float y = 3.6f; y + 1.6f < h - 0.8f; y += 3.2f) {
    if (dist < 60 * detail)
      for (float wz = z0 + 1.0f; wz + 1.4f < z1 - 0.6f; wz += 2.6f) {
        side(wx, dir, wz - 0.12f, wz + 1.52f, y - 0.2f, y + 1.7f, sill, P_DECAL);
        side(wx, dir, wz, wz + 1.4f, y, y + 1.5f, glass, P_DECAL2);
      }
    else side(wx, dir, z0 + 0.8f, z1 - 0.8f, y, y + 1.5f, glass, P_DECAL);
    if (dist < 70 * detail) front(z0, x0 + 0.8f, x1 - 0.8f, y, y + 1.5f, glass, P_DECAL);
  }
  /* the roof's edge */
  cube(x0 - 0.2f, h, z0 - 0.2f, x1 + 0.2f, h + 0.4f, z1 + 0.2f, r_mix888(col, 0, 70), 2);
}

/* a station: platforms on both sides under a roof on pillars, tiles, posters and the station's sign */
static void station_side(int dir, float z0, float dist, uint32_t hs) {
  float s = dir > 0 ? -1.0f : 1.0f;   /* -1 on the left */
  float e = 4.3f * s, back = 6.7f * s, x0 = s < 0 ? back : e, x1 = s < 0 ? e : back;
  cube(x0, 0, z0, x1, 1.1f, z0 + BLOCK, 0xCFCAC0, 2 | (s < 0 ? 4 : 8));
  /* the yellow line along the edge */
  float y0 = s < 0 ? e - 0.45f : e + 0.15f, y1 = s < 0 ? e - 0.15f : e + 0.45f;
  V3 line[4] = {{y0, 1.101f, z0 + BLOCK}, {y1, 1.101f, z0 + BLOCK}, {y1, 1.101f, z0}, {y0, 1.101f, z0}};
  r_poly(line, 4, L(0xF2C230), P_DECAL, 0);
  /* the wall behind: tiles, posters, the sign */
  side(back, dir, z0, z0 + BLOCK, 1.1f, 6.4f, L(0xE9E2CF), 0);
  if (dist < 100 * detail) {
    for (int k = 0; k < 3; k++) {
      float pz = z0 + 2.5f + (float)k * 8;
      side(back + 0.01f * -s, dir, pz, pz + 2.6f, 2.0f, 4.2f, L(0x2A2A30), P_DECAL);
      side_tex(back + 0.02f * -s, dir, pz + 0.12f, pz + 2.48f, 2.12f, 4.08f, TX_GRAFFITI_0 + (int)((hs >> (k * 3)) % 4), 0, 64, 0, 32,
               P_DECAL2);
    }
    side(back + 0.01f * -s, dir, z0 + 9.6f, z0 + 14.4f, 4.6f, 5.6f, L(0x2E5FA8), P_DECAL);
    side(back + 0.01f * -s, dir, z0 + 10.0f, z0 + 14.0f, 4.9f, 5.3f, L(0xF4F4F4), P_DECAL2);
  }
  /* pillars, and the roof on them */
  for (int k = 0; k < 3; k++) {
    float pz = z0 + 3.5f + (float)k * 8, px = 5.4f * s;
    cube(px - 0.22f, 1.1f, pz, px + 0.22f, 6.4f, pz + 0.44f, 0x7E8A96, 2);
  }
  float r0 = s < 0 ? back : 3.9f, r1 = s < 0 ? -3.9f : back;
  ceiling(6.4f, r0, r1, z0, z0 + BLOCK, L(0x5E6870));
  side(s < 0 ? -3.9f : 3.9f, -dir, z0, z0 + BLOCK, 6.4f, 7.0f, L(0x2E5FA8), 0);
  cube(r0, 6.4f, z0, r1, 7.0f, z0 + BLOCK, 0x2E5FA8, 1 | 2 | 4 | 8 | 32);
}

/* the orange viaduct of the classic yard: arches, its coping, bushy trees over it */
#define VIADUCT_H 4.3f
static void viaduct(int dir, float z0, float dist, uint32_t hs) {
  float wx = dir > 0 ? -WALL_X : WALL_X, back = dir > 0 ? -WALL_X - 1.4f : WALL_X + 1.4f;
  side_tex(wx, dir, z0, z0 + BLOCK, 0, VIADUCT_H, TX_ARCH_WALL, 0, 128, 0, 64, 0);
  /* the coping's top */
  float a = dir > 0 ? back : wx, b = dir > 0 ? wx : back;
  quad((V3){a, VIADUCT_H, z0 + BLOCK}, (V3){b, VIADUCT_H, z0 + BLOCK}, (V3){b, VIADUCT_H, z0}, (V3){a, VIADUCT_H, z0}, L(0xF58A4A), 0);
  if (dist > 110 * detail) return;
  /* the trees beyond it, in two rows */
  for (int k = 0; k < 5; k++) {
    uint32_t g = world_hash(hs + (uint32_t)k * 977u);
    float tz = z0 + 2.0f + (float)k * 4.8f + (float)(g & 7) * 0.2f;
    float far = (float)((g >> 3) & 1);
    float tx = (7.6f + far * 4.0f + (float)((g >> 4) & 3) * 0.5f) * (dir > 0 ? -1.0f : 1.0f);
    float size = 7.0f + (float)((g >> 6) & 3) * 0.9f + far * 1.5f;
    billboard(tx, VIADUCT_H - 2.8f + far * 0.6f, tz, size * 0.55f, size, TX_TREE, 64);
  }
}

/* a grey wall of concrete with graffiti, buildings behind */
static void concrete(int dir, float z0, float dist, uint32_t hs, bool buildings) {
  float wx = dir > 0 ? -WALL_X : WALL_X, wh = 4.0f;
  float outer = dir > 0 ? wx - 0.5f : wx + 0.5f;
  side_tex(wx, dir, z0, z0 + BLOCK, 0, wh, TX_CONCRETE, 0, 128, 0, 32, 0);
  float a = dir > 0 ? outer : wx, b = dir > 0 ? wx : outer;
  quad((V3){a, wh, z0 + BLOCK}, (V3){b, wh, z0 + BLOCK}, (V3){b, wh, z0}, (V3){a, wh, z0}, L(0xC8C2B6), 0);
  if (dist < 110 * detail)
    for (int k = 0; k < 2; k++) {
      uint32_t g = world_hash(hs + (uint32_t)k * 31);
      if (g % 4 == 0) continue;
      float gz = z0 + 1.5f + (float)k * 12 + (float)(g >> 8 & 3) * 0.5f, gw = 7.0f + (float)(g >> 12 & 3) * 0.6f;
      side_tex(wx + (dir > 0 ? 0.02f : -0.02f), dir, gz, gz + gw, 0.3f, 0.3f + gw * 0.5f, TX_GRAFFITI_0 + (int)((g >> 16) & 3), 0, 64,
               0, 32, P_DECAL);
    }
  for (int k = 0; k < 2 && buildings; k++) {
    uint32_t g = world_hash(hs * 3 + (uint32_t)k);
    float bh = 9 + (float)(g % 14), depth = 8 + (float)(g >> 4 & 7);
    float bz = z0 + (float)k * 12 + 0.6f;
    building(dir, bz, bz + 10.8f, 7.0f + (float)(g >> 8 & 3), depth, bh, house[(g >> 12) & 7], dist);
  }
}

/* a bridge over water: blue steel trusses on both sides, beams across overhead */
static void bridge(float z0, float dist) {
  uint32_t water = L(0x2F86C8), shine = L(0x6FB6E6), steel = 0x2F6FB4;
  for (int dir = -1; dir <= 1; dir += 2) {
    float s = (float)-dir, e = 4.5f * s;
    /* the water, and the deck's edge */
    float xa = s < 0 ? -90 : e, xb = s < 0 ? e : 90;
    quad((V3){xa, -1.2f, z0 + BLOCK}, (V3){xb, -1.2f, z0 + BLOCK}, (V3){xb, -1.2f, z0}, (V3){xa, -1.2f, z0}, water, 0);
    if (dist < 60) {
      float xc = s < 0 ? -30 : 12, xd = s < 0 ? -12 : 30;
      quad((V3){xc, -1.19f, z0 + 14}, (V3){xd, -1.19f, z0 + 14}, (V3){xd, -1.19f, z0 + 13}, (V3){xc, -1.19f, z0 + 13}, shine, P_DECAL);
    }
    side(e, -dir, z0, z0 + BLOCK, -1.2f, 0.02f, L(0x6E6A64), 0);
    /* the truss: posts, diagonals, the top chord */
    float tx = 5.3f * s;
    for (int k = 0; k < 4; k++) {
      float pz = z0 + (float)k * 6;
      cube(tx - 0.25f, 0, pz, tx + 0.25f, 8.0f, pz + 0.4f, steel, 2);
      if (dist < 120 * detail) {
        V3 dg[4] = {{tx, 7.6f, pz + 0.4f}, {tx, 7.6f, pz + 1.0f}, {tx, 0.2f, pz + 6.0f}, {tx, 0.2f, pz + 5.4f}};
        r_poly(dg, 4, L(r_shade(steel, (V3){-s, 0, 0})), P_TWOSIDED, 0);
      }
    }
    cube(tx - 0.3f, 7.6f, z0, tx + 0.3f, 8.3f, z0 + BLOCK, steel, 0);
    cube(tx - 0.3f, 0, z0, tx + 0.3f, 0.6f, z0 + BLOCK, steel, 2);
  }
  /* beams across, overhead */
  for (int k = 0; k < 2; k++) cube(-5.3f, 7.7f, z0 + (float)k * 12, 5.3f, 8.2f, z0 + (float)k * 12 + 0.4f, 0x2A62A4, 0);
}

/* the overhead line: a gantry across the tracks and a wire over each one */
static void catenary(float z0, float dist, uint32_t h) {
  float gz = z0 + 12;
  uint32_t pole = 0x4E5A66;
  cube(-5.2f, 0, gz, -4.85f, 7.8f, gz + 0.35f, pole, 2);
  cube(4.85f, 0, gz, 5.2f, 7.8f, gz + 0.35f, pole, 2);
  cube(-5.4f, 7.4f, gz, 5.4f, 7.8f, gz + 0.35f, 0x3A4450, 0);
  if (dist > 90 * detail) return;
  for (int l = 0; l < 3; l++) {
    float x = LANE_X(l);
    cube(x - 0.06f, 6.9f, gz + 0.1f, x + 0.06f, 7.4f, gz + 0.25f, 0x26262A, 2);
    V3 w[4] = {{x - 0.03f, 6.9f, z0 + BLOCK}, {x + 0.03f, 6.9f, z0 + BLOCK}, {x + 0.03f, 6.9f, z0}, {x - 0.03f, 6.9f, z0}};
    r_poly(w, 4, L(0x2A2A2E), P_TWOSIDED, 0);
    if (h % 3 == 0) {
      /* a signal over the track, its lamps facing the runner */
      float sx = x + 0.75f;
      cube(sx - 0.22f, 6.3f, gz - 0.25f, sx + 0.22f, 7.4f, gz, 0x1E2024, 2);
      uint32_t lamp = (h >> (3 + l)) & 1 ? 0xFF3B30 : 0x3CFF6A;
      front(gz - 0.26f, sx - 0.12f, sx + 0.12f, 7.0f, 7.24f, (h >> (3 + l)) & 1 ? lamp : 0x3A3A3A, P_DECAL | P_NOHAZE);
      front(gz - 0.26f, sx - 0.12f, sx + 0.12f, 6.55f, 6.79f, (h >> (3 + l)) & 1 ? 0x3A3A3A : lamp, P_DECAL | P_NOHAZE);
    }
  }
}

/* the scenery's plan, beside the game's: now and then three blocks of bridge */
static bool bridge_at(int b) {
  if (b < 12) return false;
  int g = b / 10, k = b % 10;
  uint32_t h = world_hash((uint32_t)g * 2654435761u + 17 + world_seed) % 100;
  return h >= 45 && h < 72 && k >= 4 && k <= 6;
}

static void open_block(int b, float z0, float dist) {
  uint32_t h = world_hash((uint32_t)b * 7919u + 3 + world_seed);
  bool before_tunnel = world_tunnel(z0 + BLOCK + 1);
  catenary(z0, dist, h);
  if (bridge_at(b)) {
    bridge(z0, dist);
    return;
  }
  /* now and then a station: two blocks in a row */
  bool station = !before_tunnel && !world_tunnel(z0 - 1) && world_hash((uint32_t)(b / 2) * 31u + world_seed) % 9 == 0 && z0 > 200;
  /* the classic yard: mostly the orange viaduct, now and then a stretch of concrete wall and houses */
  bool walls = world_hash((uint32_t)(b / 3) * 131u + world_seed) % 3 == 0;
  for (int dir = -1; dir <= 1; dir += 2) {
    uint32_t hs = world_hash(h + (uint32_t)(dir + 1));
    if (station) station_side(dir, z0, dist, hs);
    else if (walls) concrete(dir, z0, dist, hs, !before_tunnel);
    else viaduct(dir, z0, dist, hs);
  }
}

static void tunnel_block(float z0, float dist) {
  bool first = !world_tunnel(z0 - 1);
  uint32_t wall = 0x5A5F66, dark = 0x3A3E44;
  side(-WALL_X, 1, z0, z0 + BLOCK, 0, ROOF, scene_lit(wall, z0), 0);
  side(WALL_X, -1, z0, z0 + BLOCK, 0, ROOF, scene_lit(wall, z0), 0);
  ceiling(ROOF, -WALL_X, WALL_X, z0, z0 + BLOCK, scene_lit(dark, z0));
  if (dist < 120 * detail)
    for (int k = 0; k < 3; k++) {
      float lz = z0 + 4 + (float)k * 8;
      side(-WALL_X + 0.01f, 1, lz, lz + 1.2f, 5.6f, 6.0f, 0xFFF2B0, P_DECAL | P_NOHAZE);
      side(WALL_X - 0.01f, -1, lz, lz + 1.2f, 5.6f, 6.0f, 0xFFF2B0, P_DECAL | P_NOHAZE);
      side(-WALL_X + 0.01f, 1, lz - 2, lz - 1.6f, 0, ROOF, scene_lit(dark, z0), P_DECAL);
      side(WALL_X - 0.01f, -1, lz - 2, lz - 1.6f, 0, ROOF, scene_lit(dark, z0), P_DECAL);
    }
  if (first) {
    /* the way in: a face of brick round the arched opening, the subway's sign over it */
    lit = 256;
    front_tex(z0, -15, 15, 0, 14, TX_TUNNEL, 0, 128, 0, 64, 0);
    front(z0, -60, -15, 0, 14, 0x965038, 0);
    front(z0, 15, 60, 0, 14, 0x965038, 0);
    front(z0, -60, 60, 14, 15, 0x6E3A28, 0);
  }
}

/* ---------------------------------------------------------------- trains, barriers, coins */
/* the trains' looks: the classic commuter train (most), the silver one, the red, white and blue one,
 * the green box car */
enum { TR_CLASSIC, TR_SILVER, TR_RWB, TR_BOX, TR_OPEN };
static const uint8_t train_kind[8] = {TR_CLASSIC, TR_CLASSIC, TR_OPEN, TR_CLASSIC, TR_SILVER, TR_CLASSIC, TR_BOX, TR_RWB};
static const uint32_t train_top[4] = {0xCEDE2C, 0xC8CED4, 0xD62C26, 0x288064};

/* the blue open wagon: its load of earth a little below its rim */
static void wagon(float lx, float z0, float z1) {
  float x0 = lx - TRAIN_W, x1 = lx + TRAIN_W, t = 0.14f;
  side_tex(x0, -1, z0, z1, 0.55f, TRAIN_H, TX_WAGON_SIDE, 0, 128, 0, 32, 0);
  side_tex(x1, 1, z0, z1, 0.55f, TRAIN_H, TX_WAGON_SIDE, 0, 128, 0, 32, 0);
  front_tex(z0, x0, x1, 0.55f, TRAIN_H, TX_WAGON_END, 0, 64, 0, 64, 0);
  float y = TRAIN_H - 0.25f;
  tquad((V3){x0 + t, y, z1 - t}, (V3){x1 - t, y, z1 - t}, (V3){x1 - t, y, z0 + t}, (V3){x0 + t, y, z0 + t}, TX_CARGO, 0, 64, 32, 0, 0);
  /* the rim: its top, and the inside of the walls over the load */
  uint32_t rim = L(0x5C8ED0), in = L(0x2A4E80);
  quad((V3){x0, TRAIN_H, z1}, (V3){x0 + t, TRAIN_H, z1}, (V3){x0 + t, TRAIN_H, z0}, (V3){x0, TRAIN_H, z0}, rim, 0);
  quad((V3){x1 - t, TRAIN_H, z1}, (V3){x1, TRAIN_H, z1}, (V3){x1, TRAIN_H, z0}, (V3){x1 - t, TRAIN_H, z0}, rim, 0);
  quad((V3){x0, TRAIN_H, z0 + t}, (V3){x1, TRAIN_H, z0 + t}, (V3){x1, TRAIN_H, z0}, (V3){x0, TRAIN_H, z0}, rim, 0);
  side(x0 + t, 1, z0, z1, y, TRAIN_H, in, 0);
  side(x1 - t, -1, z0, z1, y, TRAIN_H, in, 0);
  front(z1 - t, x0, x1, y, TRAIN_H, in, 0);
}

static void car(float lx, float z0, float z1, int look, bool head, bool moving, float dist) {
  int kind = train_kind[look & 7];
  if (moving && head && kind >= TR_BOX) kind = TR_CLASSIC;   /* (what comes at you has a cab) */
  float x0 = lx - TRAIN_W, x1 = lx + TRAIN_W, bt = kind == TR_BOX ? 3.4f : 3.3f;
  /* underneath: the frame and two bogies */
  cube(lx - 0.95f, 0.3f, z0 + 0.4f, lx + 0.95f, 0.55f, z1 - 0.4f, 0x2B2B30, 1 | 2);
  cube(lx - 1.0f, 0.0f, z0 + 1.0f, lx + 1.0f, 0.5f, z0 + 3.2f, 0x232327, 1 | 2);
  cube(lx - 1.0f, 0.0f, z1 - 3.2f, lx + 1.0f, 0.5f, z1 - 1.0f, 0x232327, 1 | 2);
  if (kind == TR_OPEN) {
    wagon(lx, z0, z1);
    return;
  }
  /* the sides */
  int side_t = kind == TR_BOX ? TX_BOXCAR_SIDE : TX_TRAIN_A_SIDE + kind, front_t = kind == TR_BOX ? TX_BOXCAR_END : TX_TRAIN_A_FRONT + kind;
  side_tex(x0, -1, z0, z1, 0.55f, bt, side_t, 0, 128, 0, 32, 0);
  side_tex(x1, 1, z0, z1, 0.55f, bt, side_t, 0, 128, 0, 32, 0);
  /* the roof: curved down to the sides (flat on a box car) */
  uint32_t top = train_top[kind];
  float r0 = x0 + 0.32f, r1 = x1 - 0.32f;
  if (kind == TR_BOX) r0 = x0, r1 = x1;
  else {
    quad((V3){x0, bt, z1}, (V3){r0, TRAIN_H, z1}, (V3){r0, TRAIN_H, z0}, (V3){x0, bt, z0}, L(r_shade(top, (V3){-0.5f, 0.86f, 0})), 0);
    quad((V3){r1, TRAIN_H, z1}, (V3){x1, bt, z1}, (V3){x1, bt, z0}, (V3){r1, TRAIN_H, z0}, L(r_shade(top, (V3){0.5f, 0.86f, 0})), 0);
  }
  tquad((V3){r0, TRAIN_H, z1}, (V3){r1, TRAIN_H, z1}, (V3){r1, TRAIN_H, z0}, (V3){r0, TRAIN_H, z0}, TX_ROOF_A + kind, 0, 64, 32, 0, 0);
  /* its end facing the camera: the cab's face, or the gangway */
  if (dist > 160 * detail) return;
  V3 f[6] = {{x0, bt, z0}, {r0, TRAIN_H, z0}, {r1, TRAIN_H, z0}, {x1, bt, z0}, {x1, 0.55f, z0}, {x0, 0.55f, z0}};
  float k = 64 / (TRAIN_H - 0.55f), w = 64 / (x1 - x0);
  float uv[12] = {0, (TRAIN_H - bt) * k, (r0 - x0) * w, 0, (r1 - x0) * w, 0, 64, (TRAIN_H - bt) * k, 64, 64, 0, 64};
  uint32_t keep = lit;
  r_poly_tex(f, uv, 6, head || kind == TR_BOX ? front_t : side_t, (int)lit, 0);
  lit = keep;
  if (head && moving) {
    /* the headlights' glow */
    V3 g[4] = {{x0 - 0.6f, 2.0f, z0 - 0.3f}, {x1 + 0.6f, 2.0f, z0 - 0.3f}, {x1 + 0.6f, 0.2f, z0 - 0.3f}, {x0 - 0.6f, 0.2f, z0 - 0.3f}};
    r_poly(g, 4, 0xFFF6C0, P_ALPHA | P_NOHAZE, 70);
  }
}

static void ramp(float lx, float z0, float z1) {
  uint32_t dark = L(0x4A5058);
  float x0 = lx - 1.05f, x1 = lx + 1.05f, h = TRAIN_H;
  uint32_t keep = lit;
  tquad((V3){x0, h, z1}, (V3){x1, h, z1}, (V3){x1, 0, z0}, (V3){x0, 0, z0}, TX_RAMP, 0, 0, 32, 64, 0);
  lit = keep;
  /* the sides: dark plates, a yellow post at the top */
  V3 a[3] = {{x0, 0, z0}, {x0, h, z1}, {x0, 0, z1}};
  V3 b[3] = {{x1, 0, z1}, {x1, h, z1}, {x1, 0, z0}};
  r_poly(a, 3, r_shade(dark, (V3){-1, 0, 0}), 0, 0);
  r_poly(b, 3, r_shade(dark, (V3){1, 0, 0}), 0, 0);
  cube(x0 - 0.08f, 0, z1 - 0.3f, x0 + 0.08f, h, z1, 0xF2B81E, 2);
  cube(x1 - 0.08f, 0, z1 - 0.3f, x1 + 0.08f, h, z1, 0xF2B81E, 2);
}

static void barrier(const Obstacle *o) {
  float lx = LANE_X(o->lane), z = o->z0;
  if (o->type == O_HURDLE) {
    /* a striped board on two posts with lamps, on a blue base */
    cube(lx - 1.15f, 0, z - 0.25f, lx + 1.15f, 0.1f, z + 0.45f, 0x3C78C8, 2);
    cube(lx - 1.0f, 0.1f, z + 0.05f, lx - 0.84f, 1.2f, z + 0.2f, 0xD9C7A2, 2);
    cube(lx + 0.84f, 0.1f, z + 0.05f, lx + 1.0f, 1.2f, z + 0.2f, 0xD9C7A2, 2);
    cube(lx - 1.1f, 0.62f, z, lx + 1.1f, 1.12f, z + 0.1f, 0xF4F4F4, 2 | 16);
    front_tex(z, lx - 1.1f, lx + 1.1f, 0.62f, 1.12f, TX_BARRIER, 0, 64, 0, 16, 0);
    cube(lx - 1.06f, 1.12f, z + 0.0f, lx - 0.78f, 1.4f, z + 0.26f, 0x7A8088, 2);
    cube(lx + 0.78f, 1.12f, z + 0.0f, lx + 1.06f, 1.4f, z + 0.26f, 0x7A8088, 2);
  } else if (o->type == O_HIGHBAR) {
    /* a frame with a striped sign up high: roll under it */
    cube(lx - 1.12f, 0, z + 0.05f, lx - 0.96f, 2.45f, z + 0.22f, 0xD9C7A2, 2);
    cube(lx + 0.96f, 0, z + 0.05f, lx + 1.12f, 2.45f, z + 0.22f, 0xD9C7A2, 2);
    cube(lx - 1.12f, 1.45f, z, lx + 1.12f, 2.35f, z + 0.12f, 0xF4F4F4, 2 | 16);
    front_tex(z, lx - 1.12f, lx + 1.12f, 1.45f, 2.35f, TX_CHEVRONS, 0, 64, 0, 26, 0);
  } else {
    /* the big one: a wall of chevrons on a frame, lamps on top */
    cube(lx - 1.15f, 0, z, lx + 1.15f, 2.6f, z + 0.45f, 0xF4F4F4, 2 | 16);
    front_tex(z, lx - 1.15f, lx + 1.15f, 0.25f, 2.6f, TX_CHEVRONS, 0, 32, 0, 36, 0);
    front(z - 0.004f, lx - 1.15f, lx + 1.15f, 0, 0.25f, L(0x3A3A40), P_DECAL);
    cube(lx - 0.9f, 2.6f, z + 0.1f, lx - 0.5f, 2.95f, z + 0.35f, 0xFF3B30, 2);
    cube(lx + 0.5f, 2.6f, z + 0.1f, lx + 0.9f, 2.95f, z + 0.35f, 0xFF3B30, 2);
  }
}

/* a coin: the game's gold coin with its star, turning; far away a gold disc */
static void coin(const Item *it, float t) {
  float s, c, r = 0.42f;
  r_sincos(t * 5 + it->z * 0.6f, &s, &c);
  float d = it->z - cam.z;
  if (d < 45 * detail) {
    float hw = r * (c < 0 ? -c : c);
    if (hw < 0.05f) hw = 0.05f;
    tquad((V3){it->x - hw, it->y + r, it->z}, (V3){it->x + hw, it->y + r, it->z}, (V3){it->x + hw, it->y - r, it->z},
          (V3){it->x - hw, it->y - r, it->z}, TX_COIN, c < 0 ? 64 : 0, 0, c < 0 ? 0 : 64, 64, P_TWOSIDED | P_NOHAZE);
    return;
  }
  static const float k[8][2] = {{0, 1}, {0.707f, 0.707f}, {1, 0}, {0.707f, -0.707f}, {0, -1}, {-0.707f, -0.707f}, {-1, 0}, {-0.707f, 0.707f}};
  V3 v[8];
  uint32_t gold = scene_lit(0xFFC21F, it->z);
  for (int i = 0; i < 8; i++) v[i] = (V3){it->x + c * k[i][0] * r * 0.85f, it->y + k[i][1] * r * 0.85f, it->z + s * k[i][0] * r};
  r_poly(v, 8, gold, P_TWOSIDED, 0);
}

void scene_draw(float t) {
  /* (near things first: what they hide is then left out pixel by pixel, without being drawn) */
  float zfar = cam.z + 175 * detail;
  int b0 = (int)(cam.z / BLOCK) - (cam.z < 0), b1 = (int)(zfar / BLOCK);
  for (int i = 0; i < nobs; i++) {
    const Obstacle *o = &obs[i];
    if (o->z0 > zfar || o->z1 < cam.z) continue;
    lit = world_tunnel(o->z0) ? 128 : 256;
    if (o->type == O_TRAIN || o->type == O_MOVING) {
      float z = o->z0;
      for (int c = 0; c < o->cars; c++, z += CAR_LEN + CAR_GAP) {
        if (z > zfar) break;
        car(LANE_X(o->lane), z, z + CAR_LEN, o->look, c == 0, o->type == O_MOVING, z - cam.z);
        if (c + 1 < o->cars) cube(LANE_X(o->lane) - 0.4f, 1.0f, z + CAR_LEN, LANE_X(o->lane) + 0.4f, 2.8f, z + CAR_LEN + CAR_GAP, 0x2B2B30, 1 | 2);
      }
    } else if (o->type == O_RAMP) ramp(LANE_X(o->lane), o->z0, o->z1);
    else barrier(o);
  }
  for (int i = 0; i < nitems; i++) {
    const Item *it = &items[i];
    if (it->z > cam.z + 120 * detail || it->z < cam.z + 1) continue;
    lit = world_tunnel(it->z) ? 128 : 256;
    if (it->kind == I_COIN) coin(it, t);
    else actors_pickup(it, t);
  }
  for (int b = b0; b <= b1; b++) {
    float z0 = (float)b * BLOCK, dist = z0 - cam.z;
    lit = 256;
    if (world_tunnel(z0 + 1)) tunnel_block(z0, dist);
    else open_block(b, z0, dist);
  }
  lit = 256;
}
