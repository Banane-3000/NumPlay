/* The stages (stage.h): the game's own ground, ceilings, bridges, pits, trees, coins, fuel cans; the levels. */
#include <math.h>
#include <stdlib.h>
#include "data.h"
#include "stage.h"

#define PI 3.14159265f

/* name, price, gravity, friction, coin multiplier and rows; sky, surface, ground, ceiling's surface, icon, particle;
 * ceiling, feature, clouds, stars; bridges and their spacing; levels; the surface's colour; pits. Gravity, friction,
 * coins, ceilings, bridges, pits are the game's (Stage::create, Stage*::get*), the prices and levels the game's
 * (StageManager::initialize). */
const Stage stages[NSTAGES] = {
  {"Countryside", 0, 10, 0.6f, 1.25f, 0, IMG_SKY, IMG_SURF_COUNTRY, IMG_GROUND_COUNTRY, IMG_SURF_CAVE, IMG_STAGE_COUNTRY,
   IMG_GRASS, 0, FEAT_NONE, 1, 0, 1, 12.76214f, 70, 200, 200, 15, 0x7CC62B, PIT_NONE},
  {"Desert", 35000, 10, 0.5f, 1.75f, 0, IMG_SKY, IMG_SURF_DESERT, IMG_GROUND_DESERT, IMG_SURF_CAVE, IMG_STAGE_DESERT,
   IMG_DIRT, 0, FEAT_NONE, 1, 0, 1, 5.76214f, 110, 200, 200, 25, 0xE8D99A, PIT_NONE},
  {"Arctic", 75000, 10, 0.15f, 1.95f, 0, IMG_SKY, IMG_SURF_ARCTIC, IMG_GROUND_ARCTIC, IMG_SURF_CAVE, IMG_STAGE_ARCTIC,
   IMG_SNOW, 0, FEAT_NONE, 1, 0, 1, 12.76214f, 70, 200, 200, 30, 0xF2F6FA, PIT_NONE},
  {"Highway", 125000, 10, 0.4f, 1.15f, 0, IMG_SKY_HIGHWAY, IMG_SURF_HIGHWAY, IMG_GROUND_HIGHWAY, IMG_SURF_CAVE,
   IMG_STAGE_HIGHWAY, IMG_SMOKE, 0, FEAT_NONE, 1, 0, 0, 7.76214f, 70, 600, 300, 25, 0x6E6E6E, PIT_NONE},
  {"Cave", 150000, 10, 0.6f, 1.7f, -0.5f, IMG_SKY_CAVE, IMG_SURF_CAVE, IMG_GROUND_COUNTRY, IMG_SURF_CAVE, IMG_STAGE_CAVE,
   IMG_ROCKS, 1, FEAT_NONE, 0, 0, 1, 12.76214f, 70, 300, 300, 30, 0xB7B2A8, PIT_NONE},
  {"Moon", 175000, 3.5f, 0.8f, 1.95f, 0, IMG_SKY_MOON, IMG_SURF_MOON, IMG_GROUND_MOON, IMG_SURF_CAVE, IMG_STAGE_MOON,
   IMG_SMOKE, 0, FEAT_NONE, 0, 1, 0, 7.76214f, 70, 200, 300, 40, 0xB9BAD8, PIT_NONE},
  {"Mars", 250000, 5, 0.6f, 1.7f, -0.5f, IMG_SKY_MARS, IMG_SURF_MARS, IMG_GROUND_MARS, IMG_SURF_MARS, IMG_STAGE_MARS,
   IMG_ROCKS_MARS, 1, FEAT_NONE, 0, 0, 1, 12.76214f, 70, 200, 300, 40, 0xC77B5A, PIT_NONE},
  {"Alien Planet", 750000, 10, 0.07f, 1.15f, 0, IMG_SKY_FOREST, IMG_SURF_ALIEN, IMG_GROUND_ALIEN, IMG_SURF_ALIEN,
   IMG_STAGE_ALIEN, IMG_SLIME, 0, FEAT_NONE, 0, 1, 0, 7.76214f, 70, 200, 300, 40, 0x6FA34A, PIT_NONE},
  {"Arctic Cave", 800000, 10, 0.07f, 1.7f, -0.5f, IMG_SKY_ARCTIC_CAVE, IMG_SURF_ARCTIC, IMG_GROUND_ARCTIC,
   IMG_SURF_ARCTIC, IMG_STAGE_ARCTIC_CAVE, IMG_SNOW, 1, FEAT_ICICLES, 0, 0, 1, 12.76214f, 70, 200, 300, 40,
   0xEAF3F8, PIT_NONE},
  {"Forest", 850000, 10, 0.6f, 1.7f, 0, IMG_SKY_FOREST, IMG_SURF_FOREST, IMG_GROUND_FOREST, IMG_SURF_CAVE,
   IMG_STAGE_FOREST, IMG_GRASS, 0, FEAT_TREES, 1, 0, 1, 12.76214f, 70, 200, 250, 40, 0x4E9A2E, PIT_NONE},
  {"Mountain", 850000, 10, 0.6f, 1.7f, 0, IMG_SKY_CLIFFS, IMG_SURF_CAVE, IMG_GROUND_CLIFFS, IMG_SURF_CAVE,
   IMG_STAGE_MOUNTAIN, IMG_ROCKS, 0, FEAT_NONE, 1, 0, 1, 12.76214f, 70, 200, 250, 40, 0x8E8A84, PIT_NONE},
  {"Mudpool", 1000000, 10, 0.1f, 1.25f, 0, IMG_SKY_MUD, IMG_SURF_MUD, IMG_GROUND_MUD, IMG_SURF_CAVE,
   IMG_STAGE_MUDPOOL, IMG_DIRT_MUD, 0, FEAT_NONE, 0, 0, 0, 12.76214f, 70, 200, 250, 40, 0x8A6A44, PIT_MUD},
  {"Volcano", 1000000, 10, 0.6f, 1.7f, -0.5f, IMG_SKY_VOLCANO, IMG_SURF_VOLCANO, IMG_GROUND_VOLCANO, IMG_SURF_CAVE,
   IMG_STAGE_VOLCANO, IMG_ROCKS, 0, FEAT_NONE, 0, 0, 0, 12.76214f, 70, 200, 250, 40, 0x2E2A28, PIT_LAVA},
};

const Stage *stage = &stages[0];
int stage_id;

static float clampf(float v, float lo, float hi) { return v < lo ? lo : v > hi ? hi : v; }
/* the game's way to grow a stage's hills with the distance: a factor from lo, rising by d, at most hi + lo */
static float grow(float d, float hi, float lo) { return d > hi ? hi + lo : d < 0 ? lo : d + lo; }

/* ------------------------------------------------------------------ the ground: Stage*::getHeightAtX */
static float h_countryside(float x) {
  float u = 10 + 1.5f * x, k2 = clampf(0.00075f * u, 0, 1);
  float h = 2 * cosf(u / 10) + 0.2f * cosf(u + sinf(u / 4)) * clampf(0.0015f * u, 0, 1) +
            2 * cosf(0.17f * u + sinf(u / 4)) * k2 + 2 * cosf(0.0952f * u + sinf(0.15f * u)) * clampf(0.00275f * u, 0, 1) +
            4 * sinf(u / 30 + cosf(u / 32.14f)) * k2 - 1;
  return h * grow(u / 2000, 1.5f, 0.5f);
}

static float h_desert(float x) {
  float u = 10 + 1.5f * x;
  float h = 0.35f * cosf(u / 3 + sinf(u / 6.56f)) + 0.3f * cosf(u + sinf(0.175f * u)) * clampf(0.0015f * u, 0, 1) +
            2.5f * cosf(0.11f * u + sinf(0.0515f * u)) * clampf(0.00075f * u, 0, 1) +
            3.5f * sinf(0.132f * u + cosf(0.0151f * u)) * clampf(0.00275f * u, 0, 1) +
            2.5f * sinf(u / 30 + cosf(u / 64.14f)) * clampf(0.00175f * u, 0, 1) - 1;
  return h * grow(u / 2000, 1.5f, 0.5f);
}

static float h_arctic(float x) {
  float u = 10 + 1.5f * x;
  if (u > 2190) u = 2190 + (u - 2190) * 0.875f;
  float k = clampf(0.00145f * u, 0, 1);
  float h = 2 * cosf(u / 10) + 0.1f * cosf(u + sinf(0.3215f * u)) * clampf(0.0015f * u, 0, 1) +
            1.5f * cosf(0.18f * u + sinf(0.165f * u)) * k +
            1.75f * cosf(0.0952f * u + sinf(0.115f * u)) * clampf(0.00275f * u, 0, 1) +
            4 * sinf(u / 11 + cosf(u / 32.14f)) * k;
  return h * grow(u / 1600, 2.5f, 0.5f);
}

static float h_highway(float x) {
  float a = (x + 3) * 6.7f, b = (x + 3) * 2.5f;
  float h = 0.5f * cosf((a / 80 + PI) * 4) + 1.6f * cosf(a / 73 + PI) +
            1.6f * cosf((a / 57 + PI) * 0.5f) * clampf(b / 300, 0, 1) +
            1.6f * cosf((a / 78 + PI) / 3) * clampf(b / 600, 0, 1) +
            2.6f * cosf((a / 57 + PI) / 7) * clampf(b / 1200, 0, 1) +
            1.35f * cosf((a / 86 + PI) * 0.25f) * clampf(b / 1400, 0, 1) +
            1.25f * sinf((a / 67 + PI) * 0.25f) * clampf(b / 1500, 0, 1);
  /* long flat stretches where this is near 1 */
  float flat = clampf(powf(0.5f + 0.5f * sinf(b / 225), 8), 0, 1);
  h = h + flat * (0.05f * h - h);
  return h * grow(b / 1350, 2.25f, 1.35f);
}

/* the caves' ground (Cave, Mars, Arctic Cave share its shape) */
typedef struct {
  float ux, c1, c2, c3, s1, c4, c5, c6, c7, a2, c8, c9, a3, c10;
} CaveShape;
static float h_cave_shape(const CaveShape *c, float x) {
  float u = 10 + c->ux * x;
  float h = (sinf(u / 185) + c->s1) * cosf(u / c->c1 + sinf(u / c->c2)) +
            c->c3 * cosf(u + sinf(c->c4 * u)) * sinf(u / 200) * clampf(x / 100, 0, 1) +
            c->a2 * cosf(u / c->c5 + sinf(u / c->c6)) * clampf(x / 900, 0, 1) +
            c->a3 * cosf(u / c->c7 + sinf(u / c->c8)) * clampf(x / 1900, 0, 1) +
            5 * sinf(u / c->c9 + cosf(u / c->c10)) * clampf(x / 2500, 0, 1) - 1;
  return h;
}
static float cave_tail(float h, float x, float u, float wave) {
  float flat = clampf(powf(0.5f + 0.5f * sinf(x / wave), 8), 0, 1);
  h = h + flat * (0.05f * h - h);
  return h * grow(u / 1200, 2.5f, 0.75f);
}
static const CaveShape CAVE = {1.5f, 7.21f, 17, 0.2f, 1.5f, 0.1265f, 7.651f, 25.521f, 11.518f, 2, 15.412f, 35, 2, 42.14f};
static const CaveShape MARS = {1.75f, 9.21f, 67, 0.2f, 1.95f, 0.1265f, 8.651f, 33.521f, 13.518f, 1.8f, 22.412f, 42.5f,
                               2.2f, 64.14f};
static float h_cave(float x) { return cave_tail(h_cave_shape(&CAVE, x), x, 10 + 1.5f * x, 95); }
static float h_mars(float x) { return cave_tail(h_cave_shape(&MARS, x), x, 10 + 1.75f * x, 65); }

static float h_arctic_cave(float x) {
  /* bumps of ice on the Arctic Cave's floor */
  double s = sin((double)(10 * cosf(0.016f * x)) + (double)x * 0.2);
  float k = x / 1000;
  float bumps = (float)(s * (k > 2 ? 2.0 : k < 0.5f ? 0.5 : (double)k));
  if (bumps < 0) bumps = -bumps;
  float u = 10 + 1.5f * x;
  if (u > 1569) u = 1569 + (u - 1569) * 0.875f;
  float h = (sinf(u / 185) + 1.5f) * cosf(u / 7.21f + sinf(u / 17)) +
            0.25f * cosf(u + sinf(0.1565f * u)) * sinf(u / 200) * clampf(x / 100, 0, 1) +
            2 * cosf(u / 8.651f + sinf(u / 28.521f)) * clampf(x / 900, 0, 1) +
            2 * cosf(u / 13.518f + sinf(u / 14.412f)) * clampf(x / 1900, 0, 1) +
            5 * sinf(u / 37 + cosf(u / 62.14f)) * clampf(x / 2500, 0, 1) - 1;
  return cave_tail(h + bumps, x, u, 95);
}

static float h_moon(float x) {
  float u = 10 + 1.5f * x;
  float h = 2 * cosf(0.125f * u) + 0.2f * cosf(u + sinf(1 + 0.25f * u)) * clampf(0.0015f * u, 0, 1) +
            2 * cosf(0.14f * u + sinf(0.45f * u)) * clampf(0.00075f * u, 0, 2) +
            2 * cosf(0.0752f * u + sinf(0.095f * u)) * clampf(0.00275f * u, 0, 1) +
            4 * sinf(u / 30 + cosf(u / 21.14f)) * clampf(0.00275f * u, 0, 2) - 1;
  return h * grow(u / 1500, 1.5f, 0.5f);
}

static float h_alien(float x) {
  /* two roads of waves, blended back and forth */
  float a = (x + 3) * 22.7f, b = (x + 3) * 2.5f;
  float c1 = clampf(b / 300, 0, 1), c2 = clampf(b / 600, 0, 1), c3 = clampf(b / 1200, 0, 1),
        c4 = clampf(b / 1400, 0, 1), c5 = clampf(b / 1500, 0, 1);
  float r1 = 0.5f * cosf((a / 80 + PI) * 0.45f) + 1.6f * cosf((a / 73 + PI) * 0.81f) +
             1.6f * cosf((a / 57 + PI) / 2.126f) * c1 + 1.6f * cosf((a / 78 + PI) / 2.895f) * c2 +
             2.6f * cosf((a / 57 + PI) / 4.721f) * c3 + 1.35f * cosf((a / 86 + PI) / 4.791f) * c4 +
             1.25f * sinf((a / 67 + PI) / 3.971f) * c5;
  float r2 = 0.5f * cosf((a / 95.1f + PI) * 0.35f) + 1.6f * cosf((a / 33 + PI) * 0.51f) +
             1.6f * cosf((a / 87 + PI) / 3.126f) * c1 + 1.6f * cosf((a / 84.2f + PI) / 1.895f) * c2 +
             2.6f * cosf((a / 77.7f + PI) / 5.721f) * c3 + 1.35f * cosf((a / 84.35f + PI) / 4.791f) * c4 +
             1.25f * sinf((a / 96.7f + PI) / 3.971f) * c5;
  float h = r1 + (0.5f + 0.5f * sinf(b / 100)) * (r2 - r1);
  return h * grow(b / 2050, 2.25f, 0.95f);
}

static float h_forest(float x) {
  float u = 10 + 1.5f * x, s = sinf(0.215f * u), k2 = clampf(0.00075f * u, 0, 1);
  float h = 2 * cosf(u / 10) +
            (0.2f * sinf(u + s) + 0.25f * cosf(0.25f * u + sinf(0.515f * u))) * clampf(0.0015f * u, 0, 1) +
            2 * cosf(0.16f * u + sinf(0.1515f * u) + 2.411f) * k2 +
            2 * cosf(0.0952f * u + s + 5.6f) * clampf(0.00275f * u, 0, 1) +
            4 * sinf(u / 32 + cosf(u / 31.14f)) * k2 - 1;
  return h * grow(u / 2000, 1.5f, 0.5f);
}

/* the Mountain and the Volcano climb all the way (their u/3 and u/6) */
static float h_mountain(float x) {
  float u = 10 + 1.5f * x;
  float h = 2 * cosf(u / 6.41f) +
            (0.2f * sinf(u + sinf(0.415f * u)) + 0.25f * cosf(0.655f * u + sinf(0.215f * u))) * clampf(0.0015f * u, 0, 1) +
            2 * cosf(0.16f * u + sinf(0.3515f * u) + 2.411f) * clampf(0.00075f * u, 0, 1) +
            2 * cosf(0.1952f * u + sinf(1.215f * u) + 5.6f) * clampf(0.00275f * u, 0, 1) + u / 3;
  return h * grow(u / 4000, 1.8f, 0.2f);
}

static float h_mudpool(float x) {
  float u = 10 + 1.5f * x, s = sinf(0.25f * u);
  float h = 2 * cosf(u / 10) + 0.2f * cosf(u + s) * clampf(0.0013f * u, 0, 1) +
            2 * cosf(0.17f * u + s) * clampf(0.00035f * u, 0, 1) +
            2 * cosf(0.0952f * u + sinf(0.15f * u)) * clampf(0.00125f * u, 0, 1) +
            (4 * sinf(u / 30 + cosf(u / 32.14f)) * clampf(0.0007f * u, 0, 1) - 1);
  return h * grow(u / 2000, 1.5f, 0.5f);
}

static float h_volcano(float x) {
  float u = 10 + 1.5f * x;
  float h = 2 * cosf(u / 6.41f) +
            (0.2f * sinf(u + sinf(0.415f * u)) + 0.25f * cosf(0.855f * u + sinf(0.215f * u))) * clampf(0.0015f * u, 0, 1) +
            2 * cosf(0.3f * u + sinf(0.3515f * u) + 2.411f) * clampf(0.00075f * u, 0, 1) +
            2 * cosf(0.2052f * u + sinf(1.215f * u) + 5.6f) * clampf(0.00275f * u, 0, 0.1f) + u / 6;
  return h * grow(u / 4000, 1.8f, 0.2f);
}

static float (*const heights[NSTAGES])(float) = {h_countryside, h_desert, h_arctic, h_highway, h_cave, h_moon,
                                                 h_mars, h_alien, h_arctic_cave, h_forest, h_mountain, h_mudpool,
                                                 h_volcano};

/* the caves' ceiling over the ground (StageCave::getCaveHeightAtX); Mars opens up now and then */
static float cave_height(float x) {
  float w = powf(clampf(fabsf(x) / 50, 0, 1), 0.15f);
  float c = (cosf(x / 12.21f) + 1) * 0.175f + (cosf(0.0952f * x + sinf(0.421f * x)) + 1) * 0.25f;
  float k = x / 1700;
  c += (cosf(0.127f * x + sinf(0.751f * x)) + 1) * (k > 0.75f ? 0.25f : k < 0 ? 1 : 1 - k);
  k = x / 1900;
  c += (cosf(0.1952f * x + sinf(0.21f * x)) + 1) * (k > 0.85f ? 0.15f : k < 0 ? 1 : 1 - k);
  k = x / 1000;
  c += (sinf(x / 25 + cosf(x / 19.14f)) + 1) * 0.75f * (k > 1 ? 0 : k < 0 ? 1 : 1 - k);
  float top = 3;
  if (stage_id == ST_ARCTIC_CAVE) top = 4;
  if (stage_id == ST_MARS) top = clampf(50 * sinf(x / 100), 0, 50) + 3;
  return 50 + w * (c + top - 50);
}

/* ------------------------------------------------------------------ bridges and pits (GameContentLayer::addNewBridge,
 * addNewPit): a stage has one or the other, so they share their memory */
#define MAXGAP 400
static float gp_x0[MAXGAP], gp_len[MAXGAP], gp_y[MAXGAP], gp_shift[MAXGAP];
static int gp_n;   /* worked out so far */

/* the stage's own height with the bridges or pits so far taken out */
static float base_height(float x) { return heights[stage_id](x); }

static float gap_start(void) { return stage->pits ? 110 : 120; }

/* bridges: the first at 120 m, then every bridge_d (2 + sin(n / bridge_k)) m, 13 + 5 sin(2.172 n) m long; pits: the
 * first at 110 m, then every getDistanceToNextPit(n) = 70 (2 + sin(n / 12.76214)) m (the same on both stages),
 * 20 + 5 sin(2.172 n) m long. Each takes its length out of the road after it */
static void gaps_upto(int k) {
  while (gp_n <= k && gp_n < MAXGAP) {
    int n = gp_n;
    float x0 = gap_start(), len;
    if (stage->pits) {
      if (n) x0 = gp_x0[n - 1] + (float)(int)(70 * (2 + sinf((float)n / 12.76214f)));
      len = (float)(int)(20 + 5 * sinf((float)n * 2.172f));
    } else {
      if (n) x0 = gp_x0[n - 1] + (float)(int)(stage->bridge_d * (2 + sinf((float)n / stage->bridge_k)));
      len = (float)(int)(13 + 5 * sinf((float)n * 2.172f));
    }
    float shift = n ? gp_shift[n - 1] - gp_len[n - 1] : 0;
    gp_x0[n] = x0;
    gp_len[n] = len;
    gp_shift[n] = shift;
    gp_y[n] = base_height(x0 + shift);
    gp_n++;
  }
}

static bool gap_k(bool on, int k, float *x0, float *len, float *y) {
  if (!on || k < 0 || k >= MAXGAP) return false;
  gaps_upto(k);
  *x0 = gp_x0[k], *len = gp_len[k], *y = gp_y[k];
  return true;
}
bool stage_bridge_k(int k, float *x0, float *len, float *y) { return gap_k(stage->bridges, k, x0, len, y); }
bool stage_pit_k(int k, float *x0, float *len, float *y) { return gap_k(stage->pits, k, x0, len, y); }

/* the last bridge or pit starting before x, or -1 */
static int gap_before(float x) {
  if ((!stage->bridges && !stage->pits) || x <= gap_start()) return -1;
  int k = 0;
  for (;; k++) {
    gaps_upto(k + 1);
    if (k + 1 >= MAXGAP - 1 || gp_x0[k + 1] >= x) break;
  }
  return k;
}

static int gap_first(float x) {
  int k = gap_before(x);
  if (k < 0) return 0;
  return gp_x0[k] + gp_len[k] < x ? k + 1 : k;
}
int stage_bridge_first(float x) { return gap_first(x); }
int stage_pit_first(float x) { return gap_first(x); }

float stage_pit_depth(void) { return stage->pits == PIT_MUD ? 2.0f : stage->pits == PIT_LAVA ? 1.5f : 1.0f; }

/* the ground: past a bridge or a pit the road goes on as from its start (blended in over 5 m); under a bridge, a
 * canyon; in a pit, a dip stage_pit_depth deep */
float stage_floor(float x) {
  int k = gap_before(x);
  if (k < 0) return base_height(x);
  float x0 = gp_x0[k], len = gp_len[k];
  float b = powf(sinf(clampf((x - x0) / 5, 0, 1) * 3.14159265358979 * 0.5), 1.25f);
  float h = base_height(x + gp_shift[k] - len * b);
  if (x < x0 + len) {
    float s = sinf((x - x0) / len * 3.1415f);
    if (stage->pits) {
      float d = clampf(9 * powf(s, 2.75f), 0, 1);
      h = gp_y[k] - stage_pit_depth() * powf(s, 1.25f) * d;
    } else {
      float s1 = powf(s, 1.25f);
      h = gp_y[k] - s1 * 8 - powf(s, 0.85f) * 20 * s1;
    }
  }
  return h;
}

/* is x on a bridge's deck or a pit's surface (that of the last one starting before k_x), and its height */
static bool deck_at(float x, float k_x, float *y) {
  int k = gap_before(k_x);
  if (k < 0 || x >= gp_x0[k] + gp_len[k]) return false;
  *y = gp_y[k];
  return true;
}

float stage_deck(float x) {
  float y;
  return deck_at(x, x, &y) ? y : stage_floor(x);
}

/* ------------------------------------------------------------------ the ground as built: 0.5 m apart */
#define VCACHE 256
static int vkey[VCACHE], rkey[VCACHE];
static float vval[VCACHE], rval[VCACHE];

float stage_vertex(int k) {
  int i = k & (VCACHE - 1);
  if (vkey[i] != k) vkey[i] = k, vval[i] = stage_floor(-40.0f + 0.5f * (float)(k > 0 ? k - 1 : 0));
  return vval[i];
}

float stage_ground(float x) {
  float f = (x + 40) * 2, k = floorf(f);
  float a = stage_vertex((int)k), b = stage_vertex((int)k + 1);
  return a + (b - a) * (f - k);
}

bool stage_ceiling(float x, float *y) {
  if (!stage->ceiling) return false;
  /* over the highest ground near x (GameContentLayer::getCeilingHeightAt) */
  float m = stage_deck(x - 2), v;
  if ((v = stage_deck(x - 0.5f)) > m) m = v;
  if ((v = stage_deck(x + 0.5f)) > m) m = v;
  if ((v = stage_deck(x + 2)) > m) m = v;
  *y = m + cave_height(x);
  return true;
}

float stage_roof_vertex(int k) {
  int i = k & (VCACHE - 1);
  if (rkey[i] != k) {
    float y = 0;
    stage_ceiling(-40.0f + 0.5f * (float)(k > 0 ? k - 1 : 0), &y);
    rkey[i] = k, rval[i] = y;
  }
  return rval[i];
}

bool stage_roof(float x, float *y) {
  if (!stage->ceiling) return false;
  float f = (x + 40) * 2, k = floorf(f);
  float a = stage_roof_vertex((int)k), b = stage_roof_vertex((int)k + 1);
  *y = a + (b - a) * (f - k);
  return true;
}

/* ------------------------------------------------------------------ trees: in bunches of 11, 16, 21... */
static float tr_x[TREES_MAX];
static int tr_n, tr_count, tr_group;
static float tr_next;
static uint8_t fallen[TREES_MAX / 8];

static void trees_upto(int k) {
  while (tr_n <= k && tr_n < TREES_MAX) {
    float x = tr_next;
    tr_x[tr_n++] = x;
    /* the next one: closer and closer within a bunch, 35 m after one */
    float after = x;
    if (++tr_count > tr_group * 5 + 10) tr_group++, tr_count = 0, after += 35;
    float g = (float)tr_group / 15;
    float gap = (g > 1 ? 1.75f : 2.75f - g) + 0.6f * cosf(1.7521f * x) + 0.6f * sinf(2.157f * x) + 0.75f * cosf(0.56f * x);
    tr_next = after + (gap < 1 ? 1 : gap);
  }
}

bool stage_tree(int k, float *x, float *y) {
  if (stage->feature != FEAT_TREES || k < 0 || k >= TREES_MAX) return false;
  trees_upto(k);
  float tx = tr_x[k];
  /* none on a bridge */
  for (int b = stage_bridge_first(tx - 3); stage->bridges; b++) {
    float x0, len, by;
    if (!stage_bridge_k(b, &x0, &len, &by) || x0 > tx + 1) break;
    if (tx + 1 >= x0 && tx - 2 <= x0 + len) return false;
  }
  float h = stage_floor(tx), v;
  if ((v = stage_floor(tx - 0.35f)) < h) h = v;
  if ((v = stage_floor(tx + 0.35f)) < h) h = v;
  *x = tx, *y = h - 0.15f;
  return true;
}

int stage_tree_first(float x) {
  if (stage->feature != FEAT_TREES) return 0;
  int lo = 0, hi = TREES_MAX - 1;
  trees_upto(hi < 64 ? hi : 64);
  while (tr_x[tr_n - 1] < x && tr_n < TREES_MAX) trees_upto(tr_n);
  hi = tr_n - 1;
  while (lo < hi) {
    int m = (lo + hi) / 2;
    if (tr_x[m] < x) lo = m + 1;
    else hi = m;
  }
  return lo;
}

void stage_trees_reset(void) {
  for (unsigned i = 0; i < sizeof fallen; i++) fallen[i] = 0;
}
bool stage_tree_standing(int k) { return k < 0 || k >= TREES_MAX || !(fallen[k >> 3] >> (k & 7) & 1); }
void stage_tree_fall(int k) {
  if (k >= 0 && k < TREES_MAX) fallen[k >> 3] |= (uint8_t)(1 << (k & 7));
}

/* ------------------------------------------------------------------ coins and fuel (GameContentLayer::addCoinChunk) */
/* the ground under a coin: a bridge's deck, or the highest of three points */
static float coin_floor(float x) {
  float h, v;
  if (deck_at(x, x + 0.01f, &h)) return h;
  h = stage_floor(x);
  if ((v = stage_floor(x + 0.75f)) > h) h = v;
  if ((v = stage_floor(x - 0.75f)) > h) h = v;
  return h;
}

int stage_coin_group(int k, float *xs, float *ys, int *vals) {
  if (k < 1 || k >= COIN_GROUPS) return 0;
  float X = (float)k * COIN_SPACING;
  int left = (int)(X * stage->coin_mult), n = 0, cols = 0, row = 1;
  while (left > 0) {
    int v = left > 500 ? 500 : left > 100 ? 100 : left > 25 ? 25 : left > 5 ? 5 : 0;
    if (!v) break;
    int col;
    if (cols <= 6) col = cols++;
    else if (row == 1) cols = 1, col = 0, row = 2;
    else break;
    float x = X + 4.5f + (float)col * 1.5f;
    xs[n] = x, ys[n] = coin_floor(x) + ((float)row + stage->coin_off) * 1.5f, vals[n] = v, n++;
    left -= v;
  }
  return n;
}

/* chunks between cans (Stage*::getFuelDensity), after n cans */
static int fuel_density(int n) {
  switch (stage_id) {
    case ST_ARCTIC: case ST_ARCTIC_CAVE: case ST_VOLCANO: return n > 19 ? n - 16 : 3;
    case ST_HIGHWAY: return 5 * n / 4 + 5;
    case ST_CAVE: return 5 - n % 3;
    case ST_MARS: return n / 3 + 5;
    case ST_ALIEN: return 3 * n / 4 + 2;
    default: return 3;
  }
}

#define MAXCAN 512
static int can_chunk[MAXCAN], can_n, can_scan, can_count, can_dens;

bool stage_fuel(int k, float *x, float *y) {
  if (k < 0 || k >= MAXCAN) return false;
  while (can_n <= k && can_scan < COIN_GROUPS) {
    can_scan++;   /* the chunk at 30 can_scan m */
    can_count = (can_count + 1) % can_dens;
    if (can_count == 0) {
      can_chunk[can_n++] = can_scan;
      can_dens = fuel_density(can_n);
    }
  }
  if (k >= can_n) return false;
  float cx = (float)can_chunk[k] * COIN_SPACING + 1.5f;
  *x = cx, *y = coin_floor(cx) + (1 + stage->coin_off) * 1.5f;
  return true;
}

/* ------------------------------------------------------------------ levels */
uint32_t stage_level_goal(int n) { return stage->lvl_base + stage->lvl_step * (uint32_t)n; }
uint32_t stage_level_bonus(int n) { return stage->lvl_mult * stage_level_goal(n) + 2500; }

void stage_set(int s) {
  stage = &stages[s], stage_id = s;
#ifdef HOST
  if (stage->bridges && stage->pits) abort();   /* (tests: they share their memory) */
#endif
  gp_n = 0;
  tr_n = 0, tr_count = 0, tr_group = 0, tr_next = 30;
  can_n = 0, can_scan = 0, can_count = 0, can_dens = fuel_density(0);
  for (int i = 0; i < VCACHE; i++) vkey[i] = rkey[i] = 0x7FFFFFFF;
}
