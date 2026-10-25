/* The race, step by step (20 ms): your car, the rivals (or the traffic in Free Ride), what they bump into, laps
 * and the finish. Whole numbers only. */
#include <string.h>
#include "game.h"

#define M(x) ((int32_t)((x) * FX))  /* metres (or m/s) to FX */
#define ABS(v) ((v) < 0 ? -(v) : (v))
#define CLAMP(v, a, b) ((v) < (a) ? (a) : (v) > (b) ? (b) : (v))

static const uint8_t GEAR_TOP[6] = {0, 30, 48, 66, 84, 100};   /* each gear's top, % of the car's */

uint32_t tr_rng(uint32_t *s) {
  uint32_t x = *s ? *s : 0x2545F491u;
  x ^= x << 13, x ^= x >> 17, x ^= x << 5;
  return *s = x;
}

int tr_top_speed(const uint8_t up[4]) { return 61 + 3 * up[UP_ENGINE]; }
int tr_kmh(int32_t v) { return (int)((int64_t)v * 36 / (FX * 10)); }
int tr_traffic(const tr_race *r) { return r->mode == MODE_FREE ? 2 + 2 * r->level : TR_CARS - 1; }
static bool active(const tr_race *r, int i) { return i == 0 || i <= tr_traffic(r); }
static int32_t race_end(const tr_race *r, const tr_track *t) { return (int32_t)r->laps * t->length; }

void tr_new(tr_race *r, tr_track *t, const tr_setup *s, uint32_t seed) {
  memset(r, 0, sizeof *r);
  tr_build(t, s->track);
  r->track = t->id, r->mode = s->mode <= MODE_FREE ? s->mode : MODE_RACE, r->level = s->level <= 2 ? s->level : 1;
  r->mood = s->mood <= 2 ? s->mood : 0, r->rain = s->rain & 1, r->manual = s->manual & 1;
  for (int k = 0; k < 4; k++) r->up[k] = s->up[k] <= 4 ? s->up[k] : 4;
  r->seed = seed ? seed : 1;
  r->laps = r->mode == MODE_FREE ? 0 : TR_LAPS;
  r->phase = r->mode == MODE_FREE ? PH_RACE : PH_GRID;
  r->countdown = r->mode == MODE_FREE ? 0 : 3 * TR_HZ + TR_HZ / 2;
  r->nitro = 1000, r->gear = 1, r->lap = 1;
  tr_car *me = &r->car[0];
  me->paint = s->paint & 7, me->style = s->style & 1;
  /* the rivals: the other seven paints, skills 0 (slowest) to 6 shuffled, both bodies */
  uint8_t skills[TR_CARS - 1];
  for (int i = 0; i < TR_CARS - 1; i++) skills[i] = (uint8_t)i;
  for (int i = TR_CARS - 2; i > 0; i--) {
    int j = (int)(tr_rng(&r->seed) % (uint32_t)(i + 1));
    uint8_t x = skills[i];
    skills[i] = skills[j], skills[j] = x;
  }
  if (s->skill)
    for (int i = 0; i < TR_CARS - 1; i++) skills[i] = s->skill[i] < TR_CARS - 1 ? s->skill[i] : (uint8_t)i;
  for (int i = 1; i < TR_CARS; i++) {
    tr_car *c = &r->car[i];
    c->paint = (uint8_t)((me->paint + i) & 7);
    c->style = (uint8_t)((i + me->style) & 1);
    c->skill = skills[i - 1];
    c->think = (uint16_t)(10 + i * 3);
  }
  if (r->mode == MODE_FREE) {
    me->z = 0, me->x = 0;
    for (int i = 1; i <= tr_traffic(r); i++) {
      tr_car *c = &r->car[i];
      c->z = M(90) + (i - 1) * M(130) + (int32_t)(tr_rng(&r->seed) % 40) * FX;
      c->x = c->lane = ((int32_t)(tr_rng(&r->seed) % 3) - 1) * TR_LANE;
      c->v = c->cruise = M(22) + (int32_t)(tr_rng(&r->seed) % 14) * FX;
      r->ahead[i] = 1;
    }
    return;
  }
  /* the grid, behind the line: the best rival on pole, you at the back */
  for (int i = 1; i < TR_CARS; i++) {
    tr_car *c = &r->car[i];
    int slot = 6 - c->skill;
    c->z = -M(6) - slot * M(6);
    c->x = c->lane = slot & 1 ? M(2.4) : -M(2.4);
  }
  me->z = -M(6) - 7 * M(6), me->x = M(2.4);
  r->cam_x = (int16_t)(me->x * 3 / 4);
}

int tr_rank(const tr_race *r, int i) {
  int rank = 1;
  const tr_car *a = &r->car[i];
  for (int j = 0; j < TR_CARS; j++) {
    if (j == i) continue;
    const tr_car *b = &r->car[j];
    bool ahead = b->place ? !a->place || b->place < a->place : !a->place && (b->z > a->z || (b->z == a->z && j < i));
    if (ahead) rank++;
  }
  return rank;
}

void tr_order(const tr_race *r, uint8_t order[TR_CARS]) {
  for (int i = 0; i < TR_CARS; i++) order[tr_rank(r, i) - 1] = (uint8_t)i;
}

/* ------------------------------------------------------------------ your car */
static int32_t top_of(const tr_race *r) {
  int32_t top = M(tr_top_speed(r->up));
  if (r->damage > 300) top -= top * (r->damage - 300) * 15 / (700 * 100);   /* up to 15% slower when wrecked */
  return top;
}

static int auto_gear(int32_t v, int32_t top) {
  int g = 1;
  while (g < 5 && v >= top * GEAR_TOP[g] * 92 / 10000) g++;
  return g;
}

static void hit(tr_race *r, int32_t speed, int scale) {
  if (r->mode == MODE_FREE || speed < M(4)) return;
  int dmg = (int)(speed / FX) * scale * (12 - 2 * r->up[UP_ARMOR]) / 12 / 10;
  r->damage = (uint16_t)CLAMP(r->damage + dmg, 0, 1000);
  r->impact = 30;
}

/* the roadside: a tree, a lamp or a building where you are is solid */
static void scenery_crash(tr_race *r, const tr_track *t) {
  tr_car *c = &r->car[0];
  if (ABS(c->x) < TR_ROAD + M(1)) return;
  int32_t per = TR_SEG * FX;
  int32_t base = c->z >= 0 ? c->z / per : -((-c->z + per - 1) / per);
  for (int k = -1; k <= 1; k++) {
    int32_t oz = (base + k) * per + per / 2, dz = oz - c->z;
    if (ABS(dz) >= M(2.6)) continue;
    for (int side = -1; side <= 1; side += 2) {
      if ((side < 0) != (c->x < 0)) continue;
      tr_thing o = tr_scenery(t, (int)(base + k), side);
      if (o.kind < 0) continue;
      int32_t reach = o.half + M(1.1);
      if (ABS(c->x - o.x) >= reach) continue;
      int32_t before = c->v;
      c->v = c->v / 4;
      /* out the side you came from: the road's, or the far side when you're already past its middle */
      bool past = ABS(c->x) > ABS(o.x) && ABS(o.x) + reach + M(0.05) <= TR_OUT;
      c->x = o.x + (past ? side : -side) * (reach + M(0.05));
      c->vx = 0, r->steer = 0;
      hit(r, before, 30);
      if (before > M(4)) r->impact = 30;
    }
  }
}

static void drive_you(tr_race *r, const tr_track *t, unsigned in, int shift) {
  tr_car *c = &r->car[0];
  int32_t top = top_of(r), v = c->v;
  if (r->manual) r->gear = (uint8_t)CLAMP(r->gear + shift, 1, 5);
  else r->gear = (uint8_t)auto_gear(v, top);
  int32_t band = top * GEAR_TOP[r->gear] / 100, low = r->gear > 1 ? top * GEAR_TOP[r->gear - 1] * 60 / 10000 : 0;
  int32_t accel = M(8) + M(0.4) * r->up[UP_ENGINE] - (int32_t)((int64_t)M(6) * v / top);
  if (r->manual) accel = v < low ? accel * 2 / 5 : accel * 104 / 100;
  /* nitro: OK (or EXE) held, not braking; empty, it locks until a quarter is back */
  int drain = 6;
  bool want = (in & IN_NITRO) && !(in & IN_BRAKE) && r->phase == PH_RACE;
  r->boost = want && !r->nitro_lock && r->nitro >= drain && v > M(5);
  if (r->boost) {
    r->nitro = (uint16_t)(r->nitro - drain);
    if (r->nitro < drain) r->nitro_lock = 1;
    top += M(12), band = r->manual ? band + M(12) : top, accel += M(5);
    in |= IN_GAS;
  } else {
    r->nitro = (uint16_t)CLAMP(r->nitro + 2 + r->up[UP_NITRO], 0, 1000);
    if (r->nitro >= 250) r->nitro_lock = 0;
  }
  if (!r->manual) band = top;
  r->offroad = ABS(c->x) > TR_ROAD + M(0.6);
  int32_t cap = band < top ? band : top;
  if (r->offroad && cap > M(24)) cap = M(24);
  if (in & IN_BRAKE) v -= M(14) / TR_HZ;
  else if ((in & IN_GAS) && v < cap) v = v + accel / TR_HZ < cap ? v + accel / TR_HZ : cap;
  else if (!(in & IN_GAS)) v -= (M(1.5) + v / 40) / TR_HZ;
  if (v > cap) {   /* over the gear's or the grass's limit, or the nitro gone: down gently */
    v -= (r->offroad ? M(12) : M(8)) / TR_HZ;
    if (v < cap && (in & IN_GAS)) v = cap;
  }
  /* hills: half of gravity along the slope */
  int32_t dh = tr_height_at(t, c->z + M(2.5)) - tr_height_at(t, c->z - M(2.5));
  v -= dh * 49 / 2500;
  c->v = v < 0 ? 0 : v;
  /* steering eases in and out; the bend pulls you outwards with the square of your speed */
  int target = ((in & IN_RIGHT) ? 1000 : 0) - ((in & IN_LEFT) ? 1000 : 0);
  r->steer = (int16_t)(r->steer + (target - r->steer) / 4);
  if (ABS(target - r->steer) < 8) r->steer = (int16_t)target;
  int32_t grip = 100 + 6 * r->up[UP_TIRES] - (r->rain ? 12 : 0);
  int32_t reach = c->v < M(30) ? c->v : M(30);
  int32_t authority = M(9) * grip / 100 * reach / M(30);
  int32_t drift = (int32_t)((int64_t)c->v * c->v * tr_curve_at(t, c->z) / 1905200);
  c->vx = r->steer * authority / 1000 - drift;
  c->x = CLAMP(c->x + c->vx / TR_HZ, -TR_OUT, TR_OUT);
  c->z += c->v / TR_HZ;
  scenery_crash(r, t);
}

/* after the line, or out of the race */
static void coast(tr_race *r) {
  tr_car *c = &r->car[0];
  r->boost = 0, r->steer = 0, c->vx = 0;
  c->v = c->v > M(8) / TR_HZ ? c->v - M(8) / TR_HZ : 0;
  c->z += c->v / TR_HZ;
}

/* ------------------------------------------------------------------ rivals and traffic */
static int32_t cruise_of(const tr_race *r, int i) {
  const tr_car *c = &r->car[i];
  static const uint8_t BASE[3] = {47, 52, 57};
  return M(BASE[r->level]) + c->skill * M(1.2);
}

/* room in a lane for car i: the distance to the first car ahead in it (up to 80 m), or -1 when a car is
   alongside there */
static int32_t lane_room(const tr_race *r, const tr_track *t, int i, int32_t lane) {
  int32_t room = M(80);
  for (int j = 0; j < TR_CARS; j++) {
    if (j == i || !active(r, j)) continue;
    if (ABS(r->car[j].x - lane) >= TR_CAR_WID + M(0.4)) continue;
    int32_t dz = tr_wrap(t, r->car[j].z - r->car[i].z);
    if (dz > -M(7) && dz < M(7)) return -1;
    if (dz >= M(7) && dz < room) room = dz;
  }
  return room;
}

static void drive_rival(tr_race *r, const tr_track *t, int i) {
  tr_car *c = &r->car[i];
  bool traffic = r->mode == MODE_FREE && i != 0;
  int32_t target;
  if (traffic) {
    target = c->cruise;
  } else {
    /* their corner speeds, as a share of what the tyres hold (nobody drives on the very limit) */
    static const uint8_t GRIP[3] = {86, 92, 98};
    target = i == 0 ? M(42) : cruise_of(r, i);
    int32_t lim = (int32_t)t->limit[tr_seg_of(t, c->z + M(3))] * FX * GRIP[r->level] / 100;
    if (lim < target) target = lim;
    if (i != 0 && !c->place && r->car[0].place == 0) {
      int32_t gap = c->z - r->car[0].z;                     /* a little give and take with you */
      if (gap > M(220)) target = target * 95 / 100;
      else if (gap < -M(150)) target = target * 104 / 100;
    }
    if (c->place && i != 0) target = target * 3 / 4;        /* over the line: a lap of honour */
  }
  /* the car in front, in my way */
  int block = -1;
  int32_t near = M(40);
  for (int j = 0; j < TR_CARS; j++) {
    if (j == i || !active(r, j)) continue;
    int32_t dz = tr_wrap(t, r->car[j].z - c->z);
    if (dz > 0 && dz < near && ABS(r->car[j].x - c->x) < TR_CAR_WID + M(0.3)) near = dz, block = j;
  }
  if (c->think) c->think--;
  bool slow = block >= 0 && r->car[block].v < target - (traffic ? target / 3 : M(1));
  if (!c->think || (slow && near < M(15) && c->think > 4)) {
    c->think = (uint16_t)(12 + (tr_rng(&r->seed) & 7));
    static const int32_t LANES[3] = {0, -TR_LANE, TR_LANE};
    int32_t room[3];
    for (int k = 0; k < 3; k++) room[k] = lane_room(r, t, i, LANES[k]);
    int now = 0;
    for (int k = 1; k < 3; k++)
      if (ABS(LANES[k] - c->lane) < ABS(LANES[now] - c->lane)) now = k;
    if (slow) {
      /* overtake: the lane with the most room; crossing the middle needs the middle clear */
      int best = now;
      for (int k = 0; k < 3; k++) {
        if (room[k] < 0 || (k && now && k != now && room[0] < 0)) continue;
        if (room[k] > room[best] + M(10) || (room[best] < 0 && room[k] >= 0)) best = k;
      }
      c->lane = LANES[best];
    } else if (!traffic && block < 0) {
      int32_t home = (c->skill % 3 - 1) * TR_LANE;
      if (ABS(c->lane - home) > M(0.5) && lane_room(r, t, i, home) >= M(30)) c->lane = home;
    }
  }
  if (block >= 0) {
    /* keep 6 m or so behind it: match its speed as the gap closes */
    int32_t want = r->car[block].v + (near - M(10)) * 2;
    if (want < target) target = want;
  }
  /* moving over to a lane with room: never so slow that it can't */
  if (ABS(c->lane - c->x) > M(0.5) && target < M(4) && lane_room(r, t, i, c->lane) > M(9)) target = M(4);
  if (target < 0) target = 0;
  int32_t step = M(3.5) / TR_HZ, dx = CLAMP(c->lane - c->x, -step, step);
  if (c->v < M(2)) dx = dx * c->v / M(2);
  c->x += dx;
  c->vx = dx * TR_HZ;
  int32_t lim = TR_ROAD - M(1.2);
  c->x = CLAMP(c->x, -lim, lim);
  if (c->v < target) {
    int32_t a = M(7) - (int32_t)((int64_t)M(4) * c->v / M(64));
    c->v = c->v + a / TR_HZ < target ? c->v + a / TR_HZ : target;
  } else {
    int32_t b = M(13) / TR_HZ;
    c->v = c->v - b > target ? c->v - b : target;
  }
  c->z += c->v / TR_HZ;
}

/* Free Ride: cars that fall behind come back far ahead, in a lane with room */
static void respawn(tr_race *r, const tr_track *t, int i) {
  tr_car *c = &r->car[i], *me = &r->car[0];
  int32_t gap = tr_wrap(t, c->z - me->z);
  if (gap > -M(120) && gap < M(1000)) return;
  for (int tries = 0; tries < 8; tries++) {
    int32_t z = me->z + M(600) + (int32_t)(tr_rng(&r->seed) % 300) * FX;
    int32_t lane = ((int32_t)(tr_rng(&r->seed) % 3) - 1) * TR_LANE;
    bool ok = true;
    for (int j = 1; j <= tr_traffic(r); j++)
      if (j != i && ABS(tr_wrap(t, r->car[j].z - z)) < M(60) && ABS(r->car[j].x - lane) < TR_CAR_WID) ok = false;
    if (!ok && tries < 7) continue;
    c->z = z % t->length, c->x = c->lane = lane;
    c->cruise = M(22) + (int32_t)(tr_rng(&r->seed) % 14) * FX;
    c->v = c->cruise, c->think = 20, c->bump = 0;
    r->ahead[i] = 1;
    return;
  }
}

/* ------------------------------------------------------------------ touching */
static void contacts(tr_race *r, const tr_track *t) {
  for (int i = 0; i < TR_CARS; i++) {
    if (!active(r, i)) continue;
    for (int j = i + 1; j < TR_CARS; j++) {
      if (!active(r, j)) continue;
      tr_car *a = &r->car[i], *b = &r->car[j];
      int32_t dz = tr_wrap(t, b->z - a->z), dx = b->x - a->x;
      if (ABS(dz) >= TR_CAR_LEN || ABS(dx) >= TR_CAR_WID) continue;
      int32_t pz = TR_CAR_LEN - ABS(dz), px = TR_CAR_WID - ABS(dx);
      bool you = i == 0;
      int32_t closing;
      if ((int64_t)px * TR_CAR_LEN < (int64_t)pz * TR_CAR_WID) {
        /* side by side: apart sideways */
        int32_t s = dx >= 0 ? 1 : -1, half = px / 2 + 1;
        a->x -= s * half, b->x += s * half;
        a->v -= a->v / 50, b->v -= b->v / 50;
        closing = ABS(a->vx - b->vx);
        if (you) r->steer = (int16_t)(r->steer / 2);
      } else {
        /* one behind the other: the one behind stops at the other's bumper */
        tr_car *front = dz > 0 ? b : a, *rear = dz > 0 ? a : b;
        rear->z -= pz;
        closing = rear->v - front->v;
        if (closing > 0) {
          front->v += closing * 2 / 5;
          rear->v = front->v - M(0.5) > 0 ? front->v - M(0.5) : 0;
        }
        closing = ABS(closing);
      }
      a->x = CLAMP(a->x, -TR_OUT, TR_OUT), b->x = CLAMP(b->x, -TR_OUT, TR_OUT);
      if (you && !b->bump && closing > M(1.5)) {
        hit(r, closing, 30);
        b->bump = 25;
      }
    }
  }
}

/* ------------------------------------------------------------------ a step */
static void banner(tr_race *r, int b, int arg) { r->banner = (uint8_t)b, r->banner_arg = (uint8_t)arg, r->banner_t = 2 * TR_HZ; }

void tr_step(tr_race *r, const tr_track *t, unsigned input, int shift) {
  if (t->id != r->track) return;
  r->ticks++;
  if (r->banner_t) r->banner_t--;
  if (r->impact) r->impact--;
  if (r->phase == PH_GRID) {
    if (r->countdown) r->countdown--;
    if (!r->countdown) r->phase = PH_RACE;
    r->steer = 0;
    return;
  }
  r->time_ms += TR_TICK;
  r->lap_ms += TR_TICK;
  if (r->phase == PH_DONE && r->done < 60000) r->done++;
  tr_car *me = &r->car[0];
  int32_t before = me->z;
  if (r->phase == PH_RACE) drive_you(r, t, input, shift);
  else if (r->end == END_FINISH) drive_rival(r, t, 0);
  else coast(r);
  for (int i = 1; i < TR_CARS; i++)
    if (active(r, i)) drive_rival(r, t, i);
  contacts(r, t);
  for (int i = 0; i < TR_CARS; i++)
    if (r->car[i].bump) r->car[i].bump--;
  if (r->mode == MODE_FREE) {
    r->damage = 0;
    int32_t moved = me->z - before;
    if (moved > 0) r->odometer += (uint32_t)moved;
    for (int i = 1; i <= tr_traffic(r); i++) {
      int32_t gap = tr_wrap(t, r->car[i].z - me->z);
      if (r->ahead[i] && gap < -M(5)) {
        r->ahead[i] = 0;
        if (r->passes < 99999) r->passes++;
      } else if (!r->ahead[i] && gap > M(5)) {
        r->ahead[i] = 1;
      }
      respawn(r, t, i);
    }
    for (int i = 0; i < TR_CARS; i++) {
      r->car[i].z %= t->length;
      if (r->car[i].z < 0) r->car[i].z += t->length;
    }
  } else {
    int32_t end = race_end(r, t);
    for (int i = 0; i < TR_CARS; i++) {
      tr_car *c = &r->car[i];
      if (!c->place && c->z >= end) c->place = ++r->finished, c->finish_ms = r->time_ms;
    }
    if (r->phase == PH_RACE) {
      int lap = me->z < 0 ? 1 : (int)(me->z / t->length) + 1;
      if (lap > r->lap && lap <= r->laps) {
        if (!r->best_lap_ms || r->lap_ms < r->best_lap_ms) r->best_lap_ms = r->lap_ms;
        r->last_lap_ms = r->lap_ms;
        banner(r, lap == r->laps ? BANNER_FINAL : BANNER_LAP, lap);
        r->lap = (uint8_t)lap, r->lap_ms = 0;
      }
      if (me->place) {
        if (!r->best_lap_ms || r->lap_ms < r->best_lap_ms) r->best_lap_ms = r->lap_ms;
        r->last_lap_ms = r->lap_ms;
        r->phase = PH_DONE, r->end = END_FINISH, r->boost = 0;
        banner(r, BANNER_FINISH, me->place);
      } else if (r->damage >= 1000) {
        r->phase = PH_DONE, r->end = END_WRECK, r->boost = 0;
        banner(r, BANNER_WRECK, 0);
      } else if (r->time_ms >= 10 * 60000) {
        r->phase = PH_DONE, r->end = END_TIME, r->boost = 0;
        banner(r, BANNER_TIME, 0);
      }
    }
  }
  int32_t goal = me->x * 3 / 4;
  r->cam_x = (int16_t)(r->cam_x + (goal - r->cam_x) / 6);
}

/* ------------------------------------------------------------------ a saved race is checked */
bool tr_valid(const tr_race *r) {
  if (r->track >= TR_TRACKS || r->mode > MODE_FREE || r->level > 2 || r->mood > 2 || r->rain > 1 || r->manual > 1)
    return false;
  if (r->laps != (r->mode == MODE_FREE ? 0 : TR_LAPS) || r->phase > PH_DONE || r->end > END_TIME) return false;
  if ((r->phase == PH_DONE) != (r->end != END_NONE) || r->gear < 1 || r->gear > 5 || r->lap < 1 || r->lap > TR_LAPS)
    return false;
  if (r->boost > 1 || r->offroad > 1 || r->finished > TR_CARS || r->banner > BANNER_TIME || r->nitro_lock > 1) return false;
  for (int k = 0; k < 4; k++)
    if (r->up[k] > 4) return false;
  if (r->countdown > 4 * TR_HZ || r->nitro > 1000 || r->damage > 1000 || r->impact > 30 || r->banner_t > 2 * TR_HZ)
    return false;
  if (ABS(r->steer) > 1000 || ABS(r->cam_x) > TR_OUT || r->time_ms > 11 * 60000 || r->lap_ms > r->time_ms) return false;
  if (r->best_lap_ms > r->time_ms || r->last_lap_ms > r->time_ms || r->spare) return false;
  int places = 0;
  for (int i = 0; i < TR_CARS; i++) {
    const tr_car *c = &r->car[i];
    if (c->paint > 7 || c->style > 1 || c->skill > 6 || c->place > TR_CARS) return false;
    if (c->z < -M(100) || c->z > M(TR_LAPS * TR_MAXSEG * TR_SEG + 2000) || ABS(c->x) > TR_OUT) return false;
    if (c->v < 0 || c->v > M(100) || ABS(c->vx) > M(100) || ABS(c->lane) > TR_ROAD || c->think > 64 || c->bump > 25)
      return false;
    if (c->cruise < 0 || c->cruise > M(40)) return false;
    if (c->place) places++;
  }
  return places == r->finished;
}
