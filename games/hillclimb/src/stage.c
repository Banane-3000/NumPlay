/* The stages (stage.h): their ground, ceilings, bridges, trees, coins, fuel and levels. */
#include <math.h>
#include "data.h"
#include "stage.h"

/* name, price, gravity, friction; sky, surface, ground, ceiling's surface, icon, particle; ceiling, feature, clouds,
 * stars; seed; the four noises; a cave's height; fuel; coins; levels; the surface's colour. The prices, the
 * levels' lengths and rewards, fuel spacing and where the big coins start are the game's (Hill Climb Racing Wiki). */
const Stage stages[NSTAGES] = {
  {"Countryside", 0, 9.8f, 1.0f, IMG_SKY, IMG_SURF_COUNTRY, IMG_GROUND_COUNTRY, IMG_SURF_CAVE, IMG_STAGE_COUNTRY,
   IMG_GRASS, 0, FEAT_NONE, 1, 0, 11,
   {90, 30, 9, 2.6f}, {6, 2.5f, 0.4f, 0}, {10, 5, 1.2f, 0.22f}, {18, 8, 2.1f, 0.42f}, 0, 0,
   90, 90, 0, 90, 100, 200, 422, 5613, 200, 200, 5000, 3000, 0x7CC62B},
  {"Desert", 35000, 9.8f, 0.9f, IMG_SKY, IMG_SURF_DESERT, IMG_GROUND_DESERT, IMG_SURF_CAVE, IMG_STAGE_DESERT,
   IMG_DIRT, 0, FEAT_BRIDGES, 1, 0, 23,
   {70, 26, 9, 3.2f}, {3, 1.4f, 0.2f, 0}, {5, 3.2f, 0.8f, 0.12f}, {13, 7, 1.8f, 0.3f}, 0, 0,
   90, 90, 0, 90, 100, 220, 480, 6695, 200, 0, 3000, 2000, 0xE8D99A},
  {"Arctic", 75000, 9.8f, 0.42f, IMG_SKY, IMG_SURF_ARCTIC, IMG_GROUND_ARCTIC, IMG_SURF_CAVE, IMG_STAGE_ARCTIC,
   IMG_SNOW, 0, FEAT_NONE, 1, 0, 11,
   {90, 30, 9, 2.6f}, {6, 2.5f, 0.4f, 0}, {10, 5, 1.2f, 0.22f}, {18, 8, 2.1f, 0.42f}, 0, 0,
   90, 90, 4, 200, 90, 160, 270, 3604, 200, 0, 6000, 2000, 0xF2F6FA},
  {"Highway", 125000, 9.8f, 1.1f, IMG_SKY_HIGHWAY, IMG_SURF_HIGHWAY, IMG_GROUND_HIGHWAY, IMG_SURF_CAVE,
   IMG_STAGE_HIGHWAY, IMG_SMOKE, 0, FEAT_NONE, 1, 0, 37,
   {110, 40, 12, 3}, {0.4f, 0.2f, 0, 0}, {3.2f, 2, 0.35f, 0.05f}, {16, 8, 1.4f, 0.16f}, 0, 0,
   150, 160, 18, 650, 120, 260, 600, 6090, 600, 300, 17000, 7500, 0x6E6E6E},
  {"Cave", 150000, 9.8f, 1.0f, IMG_SKY_CAVE, IMG_SURF_CAVE, IMG_GROUND_COUNTRY, IMG_SURF_CAVE, IMG_STAGE_CAVE,
   IMG_ROCKS, 1, FEAT_NONE, 0, 0, 41,
   {48, 18, 7, 2.4f}, {1.8f, 0.8f, 0.25f, 0.05f}, {3, 2.4f, 1, 0.2f}, {7, 4.4f, 1.8f, 0.4f}, 4.3f, 2.2f,
   150, 120, 0, 150, 90, 180, 300, 4143, 300, 0, 11000, 9000, 0xB7B2A8},
  {"Moon", 175000, 3.6f, 0.9f, IMG_SKY_MOON, IMG_SURF_MOON, IMG_GROUND_MOON, IMG_SURF_CAVE, IMG_STAGE_MOON,
   IMG_SMOKE, 0, FEAT_NONE, 0, 1, 53,
   {56, 20, 7, 2.2f}, {3, 1.4f, 0.4f, 0.04f}, {4.5f, 3, 1.2f, 0.25f}, {12, 6.5f, 2.4f, 0.5f}, 0, 0,
   90, 90, 0, 90, 80, 160, 272, 4500, 200, 300, 10000, 12000, 0xB9BAD8},
  {"Mars", 250000, 5.2f, 0.95f, IMG_SKY_MARS, IMG_SURF_MARS, IMG_GROUND_MARS, IMG_SURF_MARS, IMG_STAGE_MARS,
   IMG_ROCKS_MARS, 2, FEAT_NONE, 0, 0, 67,
   {60, 22, 8, 2.6f}, {2.4f, 1.2f, 0.3f, 0.04f}, {4, 2.6f, 1, 0.2f}, {10, 5.5f, 2, 0.42f}, 4.8f, 2.4f,
   150, 150, 10, 420, 90, 180, 302, 4800, 200, 300, 10000, 12000, 0xC77B5A},
  {"Alien Planet", 750000, 9.8f, 0.85f, IMG_SKY_FOREST, IMG_SURF_ALIEN, IMG_GROUND_ALIEN, IMG_SURF_ALIEN,
   IMG_STAGE_ALIEN, IMG_SLIME, 0, FEAT_NONE, 0, 1, 71,
   {44, 19, 7, 2.2f}, {4, 1.8f, 0.4f, 0.05f}, {6, 3.5f, 1.3f, 0.25f}, {13, 7, 2.6f, 0.5f}, 0, 0,
   120, 120, 8, 320, 90, 180, 300, 4000, 300, 0, 15000, 12000, 0x6FA34A},
  {"Arctic Cave", 800000, 9.8f, 0.45f, IMG_SKY_ARCTIC_CAVE, IMG_SURF_ARCTIC, IMG_GROUND_ARCTIC, IMG_SURF_ARCTIC,
   IMG_STAGE_ARCTIC_CAVE, IMG_SNOW, 1, FEAT_ICICLES, 0, 0, 83,
   {50, 19, 7, 2.4f}, {1.8f, 0.8f, 0.25f, 0.05f}, {3, 2.4f, 1, 0.2f}, {7, 4.4f, 1.8f, 0.4f}, 4.6f, 2.2f,
   120, 120, 5, 260, 90, 180, 300, 4000, 300, 0, 15000, 12000, 0xEAF3F8},
  {"Forest", 850000, 9.8f, 1.0f, IMG_SKY_FOREST, IMG_SURF_FOREST, IMG_GROUND_FOREST, IMG_SURF_CAVE,
   IMG_STAGE_FOREST, IMG_GRASS, 0, FEAT_TREES, 1, 0, 97,
   {52, 20, 7.5f, 2.6f}, {3, 1.5f, 0.3f, 0.02f}, {5, 3, 1.1f, 0.22f}, {12, 6.5f, 2.2f, 0.42f}, 0, 0,
   90, 90, 0, 90, 90, 180, 300, 4500, 200, 250, 10000, 10000, 0x4E9A2E},
};

const Stage *stage = &stages[0];
static int stage_id;
void stage_set(int s) { stage = &stages[s], stage_id = s; }

/* ------------------------------------------------------------------ noise */
static uint32_t hash(uint32_t a, uint32_t b) {
  uint32_t h = a * 0x9E3779B1u ^ (b + 0x7F4A7C15u) * 0x85EBCA77u;
  h ^= h >> 15;
  h *= 0x2C1B3C6Du;
  h ^= h >> 12;
  h *= 0x297A2D39u;
  h ^= h >> 15;
  return h;
}
static float rnd(uint32_t a, uint32_t b) { return (float)(hash(a, b) >> 8) * (1.0f / 8388608.0f) - 1.0f; }   /* -1..1 */
static float frnd(uint32_t a, uint32_t b) { return (float)(hash(a, b) >> 8) * (1.0f / 16777216.0f); }       /* 0..1 */

/* a smooth curve through a hashed height at every whole t (Catmull-Rom) */
static float noise(uint32_t seed, float t) {
  float f = floorf(t);
  int32_t i = (int32_t)f;
  float u = t - f;
  float p0 = rnd(seed, (uint32_t)(i - 1)), p1 = rnd(seed, (uint32_t)i), p2 = rnd(seed, (uint32_t)(i + 1)),
        p3 = rnd(seed, (uint32_t)(i + 2));
  return p1 + 0.5f * u * (p2 - p0 + u * (2 * p0 - 5 * p1 + 4 * p2 - p3 + u * (3 * (p1 - p2) + p3 - p0)));
}

static float smoothstep(float a, float b, float x) {
  float t = (x - a) / (b - a);
  t = t < 0 ? 0 : t > 1 ? 1 : t;
  return t * t * (3 - 2 * t);
}

/* the hills: the noises, growing with the distance, after a flat start; `from` leaves out the large ones */
static float hills(float x, int from, int to) {
  if (x <= 12) return 0;
  float km = x * 0.001f, h = 0;
  for (int o = from; o < to; o++) {
    float a = stage->amp0[o] + stage->grow[o] * km;
    if (a > stage->amax[o]) a = stage->amax[o];
    if (a > 0) h += a * noise(stage->seed * 4 + (uint32_t)o, x / stage->len[o]);
  }
  return h * smoothstep(12, 70, x);
}

/* Highway: the road starts flat and only now and then a hill */
static float road(float x) {
  float h = hills(x, 0, 4);
  return x < 600 ? h * smoothstep(0, 600, x) * 0.7f : h;
}

/* ------------------------------------------------------------------ bridges */
static float load_x = -1e9f, load_d;
void stage_bridge_load(float x, float depth) { load_x = x, load_d = depth; }

/* bridge k: from x0 to x1 (k from 0) */
static void bridge_k(int k, float *x0, float *x1) {
  *x0 = 340.0f + (float)k * 300.0f + frnd(stage->seed, 9000u + (uint32_t)k) * 120.0f;
  *x1 = *x0 + 14.0f + frnd(stage->seed, 9500u + (uint32_t)k) * 10.0f;
}

bool stage_bridge(float x, float *x0, float *x1) {
  if (stage->feature != FEAT_BRIDGES || x < 300) return false;
  int k = (int)((x - 340.0f) / 300.0f);
  for (int j = k - 1; j <= k; j++) {
    if (j < 0) continue;
    bridge_k(j, x0, x1);
    if (x >= *x0 && x <= *x1) return true;
  }
  return false;
}

static float base_floor(float x) { return stage_id == ST_HIGHWAY ? road(x) : hills(x, 0, 4); }

/* the bridge's planks: a rope's sag between its ends, and a dip under the vehicle */
static float bridge_y(float x, float x0, float x1) {
  float t = (x - x0) / (x1 - x0), y0 = base_floor(x0), y1 = base_floor(x1);
  float sag = (x1 - x0) * 0.045f * sinf(3.14159265f * t);
  float d = (x - load_x) / 2.5f;
  float dip = d > -3 && d < 3 ? load_d * expf(-d * d) * sinf(3.14159265f * t) : 0;
  return y0 + (y1 - y0) * t - sag - dip;
}

float stage_floor(float x) {
  float x0, x1;
  if (stage_bridge(x, &x0, &x1)) return bridge_y(x, x0, x1);
  return base_floor(x);
}

float stage_ground(float x) {
  float x0, x1, h = base_floor(x);
  if (!stage_bridge(x, &x0, &x1)) return h;
  float t = (x - x0) / (x1 - x0), b = bridge_y(x, x0, x1) - 0.4f;
  if (h > b) h = b;
  return h - 9.0f * powf(sinf(3.14159265f * t), 0.35f);
}

/* ------------------------------------------------------------------ ceilings */
#define TUNNEL_START 260.0f
#define TUNNEL_EVERY 520.0f
#define TUNNEL_LEN 170.0f

bool stage_ceiling(float x, float *y) {
  if (!stage->ceiling) return false;
  float lift = 0;
  if (stage->ceiling == 2) {   /* Mars: tunnels now and then, their roof coming down over 30 m at each end */
    float p = x - TUNNEL_START;
    if (p < -30) return false;
    p = fmodf(p + 30, TUNNEL_EVERY) - 30;
    if (p > TUNNEL_LEN + 30) return false;
    float in = p < 0 ? (p + 30) / 30 : p > TUNNEL_LEN ? (TUNNEL_LEN + 30 - p) / 30 : 1;
    lift = (1 - smoothstep(0, 1, in)) * 40;
  } else if (x < 8) {
    lift = (8 - x) * 0.4f;   /* (the cave's mouth over the start) */
  }
  /* over the large hills (the small bumps come closer to it), with its own unevenness */
  float c = hills(x, 0, 2) + stage->clear + stage->clear_var * (0.5f + 0.5f * noise(stage->seed * 7 + 1, x / 13.0f)) +
            0.35f * noise(stage->seed * 7 + 2, x / 3.1f);
  float f = hills(x, 0, 4) + 2.7f;   /* never lower than a vehicle needs */
  *y = (c > f ? c : f) + lift;
  return true;
}

/* ------------------------------------------------------------------ trees: in bunches */
#define TREE_BUNCH 70.0f
bool stage_tree(int k, float *x) {
  if (stage->feature != FEAT_TREES || k < 0) return false;
  int b = k / 6, i = k % 6;
  int n = 2 + (int)(frnd(stage->seed, 7000u + (uint32_t)b) * 5);   /* 2 to 6 trees in a bunch */
  if (i >= n) return false;
  *x = 120.0f + (float)b * TREE_BUNCH + frnd(stage->seed, 7500u + (uint32_t)b) * 20.0f + (float)i * 2.2f;
  return true;
}
static uint8_t fallen[TREES_MAX / 8];
void stage_trees_reset(void) {
  for (unsigned i = 0; i < sizeof fallen; i++) fallen[i] = 0;
}
bool stage_tree_standing(int k) { return k < 0 || k >= TREES_MAX || !(fallen[k >> 3] >> (k & 7) & 1); }
void stage_tree_fall(int k) {
  if (k >= 0 && k < TREES_MAX) fallen[k >> 3] |= (uint8_t)(1 << (k & 7));
}
int stage_tree_first(float x) {
  int b = (int)((x - 120.0f - 30.0f) / TREE_BUNCH);
  return b < 0 ? 0 : b * 6;
}

/* ------------------------------------------------------------------ coins */
int stage_coin_group(int k, float *xs, float *ys, int *vals) {
  if (k < 1 || k >= COIN_GROUPS) return 0;
  float x = (float)k * COIN_SPACING + 8.0f, d = x;
  int n = 4 + (k > 48 ? 4 : k / 12);
  uint32_t h = hash(stage->seed + 3u, (uint32_t)k);
  int value = d < stage->coin25 ? 5 : d < stage->coin100 ? 25 : d < stage->coin500 ? 100 : 500;
  if (value == 500 && d < stage->all500) {
    float share = (d - stage->coin500) / (stage->all500 - stage->coin500);
    if ((float)(h & 1023) / 1024.0f > share * 0.9f + 0.06f) value = 100;
  }
  int rows = (h >> 12) % 5 == 0 && k > 10 ? 2 : 1;
  if (n > 7 && rows == 2) n = 7;
  float cx = x - (n - 1) * 0.42f;
  int m = 0;
  for (int r = 0; r < rows; r++)
    for (int i = 0; i < n; i++) {
      float px = cx + (float)i * 0.84f;
      float g = stage_floor(px), cy = 1.05f + (float)r * 0.84f;
      float top;
      if (stage_ceiling(px, &top) && g + cy > top - 0.5f) cy = (top - g) * 0.5f;
      xs[m] = px, ys[m] = g + cy, vals[m] = value, m++;
    }
  return m;
}

/* ------------------------------------------------------------------ fuel */
bool stage_fuel(int k, float *x) {
  if (k < 0 || k > 4000) return false;
  if (stage_id == ST_CAVE) {   /* 150 m, 120 m, 90 m, again */
    static const float gaps[3] = {150, 120, 90};
    float p = 0;
    for (int i = 0; i <= k; i++) p += gaps[i % 3];
    *x = p;
    return true;
  }
  /* the gaps grow by fuel_grow a can, up to fuel_max: a sum in closed form */
  float g0 = stage->fuel_gap, dg = stage->fuel_grow, gmax = stage->fuel_max;
  int ramp = dg > 0 ? (int)((gmax - g0) / dg) : 0;
  if (ramp < 0) ramp = 0;
  float p;
  if (k <= ramp) p = (float)k * g0 + dg * (float)k * (float)(k + 1) * 0.5f;
  else p = (float)ramp * g0 + dg * (float)ramp * (float)(ramp + 1) * 0.5f + (float)(k - ramp) * gmax;
  *x = stage->fuel_first + p;
  return true;
}

/* ------------------------------------------------------------------ levels */
float stage_level_end(int n) {
  /* each one lvl_inc longer than the last */
  return (float)n * stage->lvl_len + stage->lvl_inc * (float)n * (float)(n - 1) * 0.5f;
}
uint32_t stage_level_reward(int n) { return stage->lvl_reward + stage->lvl_reward_inc * (uint32_t)(n - 1); }
