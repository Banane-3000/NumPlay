/* The world ahead (game.h): trains, ramps, barriers, coins and power-ups, laid out a stretch at a time from a
 * set of patterns the way the game does it - trains to weave round or climb, rows of barriers to jump or roll
 * under, coins showing the way - harder as the run gets faster, always with a way through. */
#include <string.h>
#include "game.h"

Obstacle obs[MAXOBS];
int nobs;
Item items[MAXITEMS];
int nitems;

static uint32_t rng;
uint32_t world_seed;     /* the scenery of this run */
static Obstacle *add(int type, int lane, float z0, float z1);
static float train_len(int cars);
static float gen_z;      /* the world is made up to here */
static float kept[3];    /* each track is kept clear up to here for a train coming down it */
static int patterns;     /* made so far this run */
static float next_power, next_key;

uint32_t world_hash(uint32_t a) {
  a ^= a >> 16;
  a *= 0x7FEB352Du;
  a ^= a >> 15;
  a *= 0x846CA68Bu;
  a ^= a >> 16;
  return a;
}
static uint32_t rnd(void) {
  rng ^= rng << 13;
  rng ^= rng >> 17;
  rng ^= rng << 5;
  return rng;
}
static int rn(int n) { return (int)(rnd() % (uint32_t)n); }
static float rf(void) { return (float)(rnd() & 0xFFFF) / 65536.0f; }

void world_reset(uint32_t seed) {
  rng = seed ? seed : 0x9E3779B9u;
  world_seed = world_hash(rng) & 0xFFFFF;
  nobs = nitems = patterns = 0;
  gen_z = 0;
  kept[0] = kept[1] = kept[2] = 0;
  next_power = 140 + (float)rn(80);
  next_key = 900 + (float)rn(600);
}

/* the title screen's yard: a train standing on each side of the runner */
void world_title(void) {
  Obstacle *o = add(O_TRAIN, 0, 3.0f, 3.0f + train_len(2));
  if (o) o->cars = 2, o->look = TR_SUB;
  o = add(O_TRAIN, 2, 4.5f, 4.5f + train_len(2));
  if (o) o->cars = 2, o->look = TR_CARGO;
}

/* ---------------------------------------------------------------- the scenery's plan */
/* the city in groups of 8 stretches: streets, and in most groups a section of the elevated line, a station or a
 * tunnel (its start, middle and end) */
Stretch world_stretch(int seg) {
  Stretch s = {SC_STREET, 0, 0};
  if (seg < 0) seg = 0;
  int g = seg / 8, k = seg % 8;
  uint32_t h = world_hash((uint32_t)g * 2654435761u + 17 + world_seed);
  uint32_t hs = world_hash((uint32_t)seg * 7919u + world_seed);
  /* streets: a look for each side, kept for two or three stretches */
  uint32_t hl = world_hash((uint32_t)(seg / 3) * 131u + world_seed);
  s.part = (uint8_t)(hl % 8), s.right = (uint8_t)((hl >> 8) % 8);
  (void)hs;
  if (g == 0) return s;
  int kind = (int)(h % 7);   /* 0..2 nothing, 3..4 a tunnel, 5 the elevated line, 6 a station */
  int len = 3 + (int)((h >> 4) % 3), at = 1 + (int)((h >> 8) % 2);
  if (kind < 3 || k < at || k >= at + len) return s;
  s.kind = kind < 5 ? SC_TUBE : kind == 5 ? SC_PILLARS : SC_STATION;
  s.part = k == at ? 0 : k == at + len - 1 ? 2 : 1;
  return s;
}

bool world_tunnel(float z) {
  Stretch s = world_stretch((int)(z / SEG) - (z < 0));
  return s.kind == SC_TUBE && !(s.part == 0 && z - (float)(int)(z / SEG) * SEG < 6 * U);
}

float world_roof(float z) {
  Stretch s = world_stretch((int)(z / SEG) - (z < 0));
  /* (the game's roof colliders: the elevated line's deck, the station's roof, the tunnel's) */
  return s.kind == SC_PILLARS ? 24.5f * U : s.kind == SC_STATION ? 29 * U : s.kind == SC_TUBE ? 78 * U : 1000;
}

bool world_covered(float z) { return world_roof(z) < 5; }

/* ---------------------------------------------------------------- placing things */
static float train_len(int cars) { return (float)cars * CAR_LEN - CAR_GAP; }

static Obstacle *add(int type, int lane, float z0, float z1) {
  if (nobs >= MAXOBS || z0 < kept[lane]) return NULL;
  Obstacle *o = &obs[nobs++];
  memset(o, 0, sizeof *o);
  o->type = (uint8_t)type, o->lane = (uint8_t)lane, o->z0 = z0, o->z1 = z1;
  o->look = (uint8_t)(rn(5) < 2 ? TR_SUB : rn(2) ? TR_STANDARD : TR_CARGO);
  return o;
}
static float train(int lane, float z, int cars) {
  Obstacle *o = add(O_TRAIN, lane, z, z + train_len(cars));
  if (o) o->cars = (uint8_t)cars;
  return z + train_len(cars);
}
/* the ramp car (its ramp from z), then cars - 1 more behind it */
static float ramp_train(int lane, float z, int cars) {
  Obstacle *o = add(O_RAMP, lane, z, z + RAMP_LEN);
  float end = z + RAMP_LEN;
  if (cars > 1) {
    Obstacle *t = add(O_TRAIN, lane, end + CAR_GAP, end + CAR_GAP + train_len(cars - 1));
    if (t) {
      t->cars = (uint8_t)(cars - 1);
      if (o) t->look = TR_CARGO;
      t->v = 1;   /* (wagons only: no cab) */
    }
    end += CAR_GAP + train_len(cars - 1);
  }
  return end;
}
/* a train coming the other way, met by the runner about at z */
static void moving(int lane, float z, int cars, float speed) {
  float v = 11 + rf() * 4;
  float lead = 110 * v / (speed + v);   /* it starts moving 110 m before the runner reaches it */
  Obstacle *o = add(O_MOVING, lane, z + lead, z + lead + train_len(cars));
  if (o) o->cars = (uint8_t)cars, o->v = v, kept[lane] = o->z1 + 2;
}
static void barrier(int type, int lane, float z) { add(type, lane, z, z + 0.4f); }

static void item(int kind, float x, float y, float z) {
  if (nitems >= MAXITEMS) return;
  Item *it = &items[nitems++];
  it->x = x, it->y = y, it->z = z, it->kind = (uint8_t)kind, it->flying = 0;
}
/* the game's coin lines: a coin every 30 units, at the runner's waist */
#define COIN_GAP (30 * U)
#define COIN_Y (6 * U)
static void coins(int lane, float z0, float z1, float y) {
  for (float z = z0; z <= z1; z += COIN_GAP) item(I_COIN, LANE_X(lane), y, z);
}
/* coins over a jump made at z: the game's jump curve, 7 coins over its 98 units, 20 high */
static void coin_arc(int lane, float z, float base) {
  for (int i = 0; i < 7; i++) {
    float t = (float)i / 6;
    item(I_COIN, LANE_X(lane), base + COIN_Y + 19.9f * U * 4 * t * (1 - t), z + (t - 0.5f) * 98.4f * U);
  }
}
/* coins up a ramp and along the roof */
static void coin_ramp(int lane, float z, float roof_end) {
  for (float d = 6 * U; d < RAMP_LEN; d += COIN_GAP) item(I_COIN, LANE_X(lane), COIN_Y + TRAIN_H * d / RAMP_LEN, z + d);
  coins(lane, z + RAMP_LEN + COIN_GAP * 0.5f, roof_end - 1, TRAIN_H + COIN_Y);
}

static bool lane_blocked(int lane, float z0, float z1) {
  for (int i = 0; i < nobs; i++) {
    const Obstacle *o = &obs[i];
    if (o->lane == lane && o->type != O_MOVING && o->z0 < z1 && o->z1 > z0) return true;
  }
  return false;
}

static void shuffle3(int *l) {
  for (int i = 2; i > 0; i--) {
    int j = rn(i + 1), t = l[i];
    l[i] = l[j], l[j] = t;
  }
}

/* ---------------------------------------------------------------- the patterns */
static float hardness(float speed) {
  float d = (speed - SPEED_MIN) / (SPEED_MAX - SPEED_MIN);   /* how hard, 0 to 1 */
  return d < 0 ? 0 : d > 1 ? 1 : d;
}

/* under the elevated line or in a station the roof is low: barriers only */
static float covered_pattern(float z, float d, int *l) {
  int k = rn(100);
  float end = z;
  if (k < 30) {
    for (int i = 0; i < 3; i++) {
      barrier(O_STANDARD, i, z);
      if (i == l[0]) coin_arc(i, z, 0);
    }
    end = z + 4;
  } else if (k < 55) {
    barrier(O_HURDLE, l[0], z);
    barrier(O_HIGHBAR, l[1], z);
    barrier(O_STANDARD, l[2], z);
    coin_arc(l[0], z, 0);
    end = z + 4;
  } else if (k < 80) {
    float gap = 14 - 4 * d;
    for (int i = 0; i < 3; i++) {
      int t = rn(3) == 0 ? O_HIGHBAR : rn(2) ? O_HURDLE : O_STANDARD;
      barrier(t, l[i], z + (float)i * gap);
      coins(l[(i + 1) % 3], z + (float)i * gap - 3, z + (float)i * gap + 3, COIN_Y);
    }
    end = z + 2 * gap + 2;
  } else {
    for (int i = 0; i < 3; i++) barrier(O_HIGHBAR, i, z);
    coins(l[0], z - 6, z + 6, 0.4f);
    end = z + 6;
  }
  return end;
}

static float pattern(float z, float speed) {
  float d = hardness(speed);
  int l[3] = {0, 1, 2};
  shuffle3(l);
  int k = rn(100);
  /* (easier ones while the run is young) */
  if (patterns < 3) k = k % 40;
  float end = z;
  /* (no train may run on under a low roof: anything near one is barriers only) */
  bool low = false;
  for (float dz = -10; dz <= 130; dz += 10) low |= world_covered(z + dz);
  if (low) {
    end = covered_pattern(z, d, l);
    patterns++;
    return end;
  }
  if (k < 14) {
    /* two trains side by side; coins down the free track */
    int cars = 2 + rn(3);
    end = train(l[0], z, cars);
    train(l[1], z + (float)rn(3) * 3, cars - rn(2));
    coins(l[2], z, end, COIN_Y);
  } else if (k < 24) {
    /* a row of low barriers: jump */
    for (int i = 0; i < 3; i++) {
      barrier(O_HURDLE, i, z);
      if (i == l[0]) coin_arc(i, z, 0);
    }
    end = z + 4;
  } else if (k < 34) {
    /* a row of high barriers: roll */
    for (int i = 0; i < 3; i++) barrier(rn(3) ? O_HIGHBAR : O_STANDARD, i, z);
    coins(l[0], z - 6, z + 6, 0.4f);
    end = z + 6;
  } else if (k < 46) {
    /* a ramp up to a train, others beside it: the roof is the way */
    int cars = 3 + rn(2);
    end = ramp_train(l[0], z, cars);
    coin_ramp(l[0], z, end);
    if (rn(3)) train(l[1], z + RAMP_LEN * 0.5f + (float)rn(4), cars);
    else {
      barrier(O_STANDARD, l[1], z + 10);
      barrier(O_HURDLE, l[1], z + 24);
    }
    if (rn(2)) barrier(rn(2) ? O_HURDLE : O_HIGHBAR, l[2], z + 6 + (float)rn(10));
    else coins(l[2], z + 4, z + 20, COIN_Y);
  } else if (k < 56) {
    /* one of each barrier */
    barrier(O_HURDLE, l[0], z);
    barrier(O_HIGHBAR, l[1], z);
    barrier(O_STANDARD, l[2], z);
    coin_arc(l[0], z, 0);
    end = z + 4;
  } else if (k < 66) {
    /* a train coming: get off its track */
    int cars = 2 + rn(3);
    moving(l[0], z, cars, speed);
    end = train(l[1], z - 10, 2 + rn(2));
    coins(l[2], z - 12, z + 8, COIN_Y);
    if (d > 0.3f && rn(2)) barrier(O_HURDLE, l[2], z + 2);
    if (end < z + 10) end = z + 10;
  } else if (k < 76) {
    /* a corridor between two long trains, with barriers in it */
    int cars = 3 + rn(3);
    float len = train_len(cars);
    train(0, z, cars);
    train(2, z, cars);
    int n = 1 + (int)(d * 2.5f);
    for (int i = 0; i < n; i++) {
      float bz = n > 1 ? z + 8 + (float)i * (len - 16) / (float)(n - 1) : z + len / 2;
      int t = rn(3) == 0 ? O_STANDARD : rn(2) ? O_HURDLE : O_HIGHBAR;
      barrier(t, 1, bz);
      if (t == O_HURDLE) coin_arc(1, bz, 0);
    }
    end = z + len;
  } else if (k < 86) {
    /* across the roofs: a ramp, then trains one after the other */
    int n = 2 + rn(2);
    float zz = ramp_train(l[0], z, 2 + rn(2));
    coin_ramp(l[0], z, zz);
    for (int i = 1; i < n; i++) {
      float gap = 3 + rf() * 3;
      int cars = 2 + rn(2);
      coin_arc(l[0], zz + gap / 2, TRAIN_H);
      zz = train(l[0], zz + gap, cars);
      coins(l[0], zz - train_len(cars) + 1.5f, zz - 1, TRAIN_H + COIN_Y);
    }
    train(l[1], z + RAMP_LEN + 2, 3);
    if (rn(2)) train(l[2], z + RAMP_LEN + 6, 3);
    else barrier(O_STANDARD, l[2], z + 12);
    end = zz;
  } else if (k < 94) {
    /* weave: trains in two tracks, then two others */
    float gap = 16 - 4 * d;
    int free = l[0];
    for (int i = 0; i < 3; i++) {
      for (int t = 0; t < 3; t++)
        if (t != free) barrier(rn(2) ? O_HIGHBAR : O_HURDLE, t, z + (float)i * gap);
      coins(free, z + (float)i * gap - 3, z + (float)i * gap + 2, COIN_Y);
      int nf = free + (rn(2) ? 1 : -1);
      free = nf < 0 ? 1 : nf > 2 ? 1 : nf;
    }
    end = z + 2 * gap + 2;
  } else {
    /* a long train, the others free: just coins */
    int cars = 4;
    end = train(l[0], z, cars);
    coins(l[1], z + 2, z + 26, COIN_Y);
    barrier(rn(2) ? O_HURDLE : O_HIGHBAR, l[2], z + 10);
  }
  patterns++;
  return end;
}

/* a power-up (or a key, or a mystery box) where the track is free */
static void place_power(float z) {
  int lanes[3] = {0, 1, 2};
  shuffle3(lanes);
  for (int i = 0; i < 3; i++) {
    int ln = lanes[i];
    if (lane_blocked(ln, z - 3, z + 3)) continue;
    int k = rn(100);
    int kind = k < 30 ? I_MAGNET : k < 48 ? I_JETPACK : k < 70 ? I_SNEAKERS : k < 92 ? I_DOUBLE : I_MYSTERY;
    item(kind, LANE_X(ln), 0.9f, z);
    return;
  }
}

void world_update(float pz, float speed) {
  /* the trains coming this way move once they are near */
  for (int i = 0; i < nobs; i++) {
    Obstacle *o = &obs[i];
    if (o->type == O_MOVING && o->z0 - pz < 115) o->z0 -= o->v * DT, o->z1 -= o->v * DT;
  }
  /* forget what is behind */
  int k = 0;
  for (int i = 0; i < nobs; i++)
    if (obs[i].z1 > pz - 12) obs[k++] = obs[i];
  nobs = k;
  k = 0;
  for (int i = 0; i < nitems; i++)
    if (items[i].z > pz - 8) items[k++] = items[i];
  nitems = k;
  /* make more ahead */
  if (gen_z < pz + 40) gen_z = pz + 40;
  while (gen_z < pz + 190 && nobs < MAXOBS - 12 && nitems < MAXITEMS - 40) {
    if (gen_z < 70) {
      coins(1, 30, 66, COIN_Y);   /* the start: coins straight ahead, nothing in the way */
      gen_z = 80;
      continue;
    }
    float end = pattern(gen_z, speed);
    if (end >= next_power) {
      place_power(end + 4);
      next_power = end + 220 + (float)rn(200);
    }
    if (end >= next_key) {
      int ln = rn(3);
      if (!lane_blocked(ln, end, end + 8)) item(I_KEY, LANE_X(ln), 0.9f, end + 4);
      next_key = end + 1200 + (float)rn(1200);
    }
    float d = hardness(speed);
    gen_z = end + 9 + speed * 0.45f + (1 - d) * 6 + rf() * 6;
  }
}

float world_ground(float x, float z, float y) {
  float g = 0;
  for (int i = 0; i < nobs; i++) {
    const Obstacle *o = &obs[i];
    if (z < o->z0 || z > o->z1) continue;
    float lx = LANE_X(o->lane), h = -1;
    if (x < lx - TRAIN_W - 0.2f || x > lx + TRAIN_W + 0.2f) continue;
    if (o->type == O_TRAIN || o->type == O_MOVING) h = TRAIN_H;
    else if (o->type == O_RAMP) h = TRAIN_H * (z - o->z0) / RAMP_LEN;
    if (h > g && h <= y + 0.9f) g = h;
  }
  return g;
}

void world_clear(float z0, float z1) {
  int k = 0;
  for (int i = 0; i < nobs; i++)
    if (obs[i].z1 < z0 || obs[i].z0 > z1) obs[k++] = obs[i];
  nobs = k;
  k = 0;
  for (int i = 0; i < nitems; i++)
    if (items[i].z < z0 || items[i].z > z1 || items[i].y < 2.5f || items[i].y > 5.5f) items[k++] = items[i];
  nitems = k;
  if (gen_z < z1) gen_z = z1;
}

void world_sky_coins(float z0, float z1) {
  int lane = rn(3);
  for (float z = z0; z < z1 && nitems < MAXITEMS - 2; z += COIN_GAP * 0.6f) {
    float roof = world_roof(z) < world_roof(z + 12) ? world_roof(z) : world_roof(z + 12);
    item(I_COIN, LANE_X(lane), roof < 9 ? roof - 1.2f : 8.6f, z);
    if (rn(14) == 0) lane = lane == 1 ? rn(2) * 2 : 1;
  }
}
