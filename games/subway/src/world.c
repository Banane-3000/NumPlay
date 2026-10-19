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
  Obstacle *o = add(O_TRAIN, 0, 2.5f, 2.5f + train_len(2));
  if (o) o->cars = 2, o->look = 1;
  o = add(O_TRAIN, 2, 4.5f, 4.5f + train_len(1));
  if (o) o->cars = 1, o->look = 0;
}

/* ---------------------------------------------------------------- the scenery's plan */
bool world_tunnel(float z) {
  if (z < 240) return false;
  int b = (int)(z / 24), g = b / 10, k = b % 10;
  return world_hash((uint32_t)g * 2654435761u + 17 + world_seed) % 100 < 45 && k >= 3 && k <= 6;
}

/* ---------------------------------------------------------------- placing things */
static float train_len(int cars) { return (float)cars * CAR_LEN + (float)(cars - 1) * CAR_GAP; }

static Obstacle *add(int type, int lane, float z0, float z1) {
  if (nobs >= MAXOBS || z0 < kept[lane]) return NULL;
  Obstacle *o = &obs[nobs++];
  memset(o, 0, sizeof *o);
  o->type = (uint8_t)type, o->lane = (uint8_t)lane, o->z0 = z0, o->z1 = z1;
  o->look = (uint8_t)rn(8);
  return o;
}
static float train(int lane, float z, int cars) {
  Obstacle *o = add(O_TRAIN, lane, z, z + train_len(cars));
  if (o) o->cars = (uint8_t)cars;
  return z + train_len(cars);
}
static float ramp_train(int lane, float z, int cars) {
  add(O_RAMP, lane, z, z + RAMP_LEN);
  return train(lane, z + RAMP_LEN, cars);
}
/* a train coming the other way, met by the runner about at z */
static void moving(int lane, float z, int cars, float speed) {
  float v = 11 + rf() * 4;
  float lead = 110 * v / (speed + v);   /* it starts moving 110 m before the runner reaches it */
  Obstacle *o = add(O_MOVING, lane, z + lead, z + lead + train_len(cars));
  if (o) o->cars = (uint8_t)cars, o->v = v, kept[lane] = o->z1 + 2;
}
static void barrier(int type, int lane, float z) { add(type, lane, z, z + (type == O_BLOCK ? 0.5f : 0.3f)); }

static void item(int kind, float x, float y, float z) {
  if (nitems >= MAXITEMS) return;
  Item *it = &items[nitems++];
  it->x = x, it->y = y, it->z = z, it->kind = (uint8_t)kind, it->flying = 0;
}
static void coins(int lane, float z0, float z1, float y) {
  for (float z = z0; z <= z1; z += 1.6f) item(I_COIN, LANE_X(lane), y, z);
}
/* coins over a jump made at z */
static void coin_arc(int lane, float z, float base) {
  for (int i = -3; i <= 3; i++) {
    float t = (float)i / 3.5f;
    item(I_COIN, LANE_X(lane), base + 0.9f + 1.3f * (1 - t * t), z + (float)i * 1.3f);
  }
}
/* coins up a ramp and along the roof */
static void coin_ramp(int lane, float z, float roof_end) {
  for (float d = 0.8f; d < RAMP_LEN; d += 1.6f) item(I_COIN, LANE_X(lane), 0.9f + TRAIN_H * d / RAMP_LEN, z + d);
  coins(lane, z + RAMP_LEN + 0.8f, roof_end - 1, TRAIN_H + 0.9f);
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
static float pattern(float z, float speed) {
  float d = (speed - 13) / 13;   /* how hard, 0 to 1 */
  if (d < 0) d = 0;
  if (d > 1) d = 1;
  int l[3] = {0, 1, 2};
  shuffle3(l);
  int k = rn(100);
  /* (easier ones while the run is young) */
  if (patterns < 3) k = k % 40;
  float end = z;
  if (k < 14) {
    /* two trains side by side; coins down the free track */
    int cars = 1 + rn(2);
    end = train(l[0], z, cars);
    train(l[1], z + (float)rn(3) * 3, cars);
    coins(l[2], z, end, 0.9f);
  } else if (k < 24) {
    /* a row of low barriers: jump */
    for (int i = 0; i < 3; i++) {
      barrier(O_HURDLE, i, z);
      if (i == l[0]) coin_arc(i, z, 0);
    }
    end = z + 4;
  } else if (k < 34) {
    /* a row of high barriers: roll */
    for (int i = 0; i < 3; i++) barrier(O_HIGHBAR, i, z);
    coins(l[0], z - 6, z + 6, 0.6f);
    end = z + 6;
  } else if (k < 46) {
    /* a ramp up to a train, others beside it: the roof is the way */
    int cars = 2 + rn(2);
    end = ramp_train(l[0], z, cars);
    coin_ramp(l[0], z, end);
    if (rn(3)) train(l[1], z + RAMP_LEN + (float)rn(4), cars - 1);
    else {
      barrier(O_BLOCK, l[1], z + 10);
      barrier(O_HURDLE, l[1], z + 24);
    }
    if (rn(2)) barrier(rn(2) ? O_HURDLE : O_HIGHBAR, l[2], z + 6 + (float)rn(10));
    else coins(l[2], z + 4, z + 20, 0.9f);
  } else if (k < 56) {
    /* one of each barrier */
    barrier(O_HURDLE, l[0], z);
    barrier(O_HIGHBAR, l[1], z);
    barrier(O_BLOCK, l[2], z);
    coin_arc(l[0], z, 0);
    end = z + 4;
  } else if (k < 66) {
    /* a train coming: get off its track */
    int cars = 1 + rn(2);
    moving(l[0], z, cars, speed);
    end = train(l[1], z - 10, 1 + rn(2));
    coins(l[2], z - 12, z + 8, 0.9f);
    if (d > 0.3f && rn(2)) barrier(O_HURDLE, l[2], z + 2);
    if (end < z + 10) end = z + 10;
  } else if (k < 76) {
    /* a corridor between two long trains, with barriers in it */
    int cars = 2 + rn(2);
    float len = train_len(cars);
    train(0, z, cars);
    train(2, z, cars);
    int n = 1 + (int)(d * 2.5f);
    for (int i = 0; i < n; i++) {
      float bz = n > 1 ? z + 8 + (float)i * (len - 16) / (float)(n - 1) : z + len / 2;
      int t = rn(2) ? O_HURDLE : O_HIGHBAR;
      barrier(t, 1, bz);
      if (t == O_HURDLE) coin_arc(1, bz, 0);
    }
    end = z + len;
  } else if (k < 86) {
    /* across the roofs: a ramp, then trains one after the other */
    int n = 2 + rn(2);
    float zz = ramp_train(l[0], z, 1 + rn(2));
    coin_ramp(l[0], z, zz);
    for (int i = 1; i < n; i++) {
      float gap = 3 + rf() * 3;
      int cars = 1 + rn(2);
      coin_arc(l[0], zz + gap / 2, TRAIN_H);
      zz = train(l[0], zz + gap, cars);
      coins(l[0], zz - train_len(cars) + 1.5f, zz - 1, TRAIN_H + 0.9f);
    }
    train(l[1], z + RAMP_LEN + 2, 2);
    if (rn(2)) train(l[2], z + RAMP_LEN + 6, 2);
    else barrier(O_BLOCK, l[2], z + 12);
    end = zz;
  } else if (k < 94) {
    /* weave: big barriers in two tracks, then two others */
    float gap = 13 - 4 * d;
    int free = l[0];
    for (int i = 0; i < 3; i++) {
      for (int t = 0; t < 3; t++)
        if (t != free) barrier(O_BLOCK, t, z + (float)i * gap);
      coins(free, z + (float)i * gap - 3, z + (float)i * gap + 2, 0.9f);
      int nf = free + (rn(2) ? 1 : -1);
      free = nf < 0 ? 1 : nf > 2 ? 1 : nf;
    }
    end = z + 2 * gap + 2;
  } else {
    /* a long train, the others free: just coins */
    int cars = 3;
    end = train(l[0], z, cars);
    coins(l[1], z + 2, z + 26, 0.9f);
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
    item(kind, LANE_X(ln), 1.1f, z);
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
      coins(1, 30, 66, 0.9f);   /* the start: coins straight ahead, nothing in the way */
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
      if (!lane_blocked(ln, end, end + 8)) item(I_KEY, LANE_X(ln), 1.1f, end + 4);
      next_key = end + 1200 + (float)rn(1200);
    }
    float d = (speed - 13) / 13;
    gen_z = end + 9 + speed * 0.45f + (1 - (d > 1 ? 1 : d)) * 6 + rf() * 6;
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
    if (h > g && h <= y + 0.7f) g = h;
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
  for (float z = z0; z < z1 && nitems < MAXITEMS - 2; z += 1.6f) {
    item(I_COIN, LANE_X(lane), world_tunnel(z) || world_tunnel(z + 12) ? 5.6f : 8.6f, z);
    if (rn(14) == 0) lane = lane == 1 ? rn(2) * 2 : 1;
  }
}
