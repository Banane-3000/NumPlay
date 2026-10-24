/* The race: your car and the rivals' on the road, a step of 20 ms at a time. Whole numbers only (game.h).
 *
 * Your car goes where it points; steering turns it as far as its tyres hold (24 m/s^2 sideways at grip 1, less on
 * the grass and on the touge), and a bend you don't steer through carries you to its outside. Braking while you
 * steer hard at speed slides the car: a tighter line for less braking. The engine pulls through six gears, nitro
 * adds a push and top speed, and driving close behind a car pulls you along. The rivals keep to their lines, brake
 * for the bends at three levels, pass slower cars and take orbs. Cars never go through each other or the barriers.
 */
#include <string.h>
#include "game.h"

const pg_vehicle pg_vehicles[PG_VEHICLES] = {
  /* name, kind, top speed and pull (1/8), grip, slide, grass (1/128), horse, redline, body, accent */
  {"Apex GT", "Rear drive, twin turbo", 81 * 8, 88, 128, 147, 128, 0, 8500, {225, 32, 42}, {245, 190, 40}},
  {"Proto R", "Mid engine, sharp grip", 79 * 8, 82, 146, 115, 128, 0, 9000, {240, 240, 245}, {220, 30, 35}},
  {"Rally IV", "Four-wheel drive", 76 * 8, 96, 138, 134, 64, 0, 7800, {35, 105, 230}, {245, 205, 45}},
  {"Black Stallion", "A horse: quick off the mark", 72 * 8, 104, 153, 138, 80, 1, 9200, {24, 24, 32}, {255, 45, 35}},
  {"Brown Mustang", "A horse: sure-footed", 70 * 8, 100, 157, 134, 72, 1, 8800, {148, 84, 42}, {225, 175, 55}},
};

/* grip of the road in each scenery (1/128): the three circuits, then the endless road's five */
static const uint8_t ZONE_GRIP[8] = {128, 133, 118, 128, 125, 133, 119, 123};
static const uint16_t TORQUE[18] = {206, 206, 211, 223, 235, 245, 255, 262, 268, 272, 274, 274, 272, 268, 262, 254,
                                   245, 236};
static const uint16_t PULL[6] = {346, 289, 211, 165, 133, 133};
static const uint8_t GEAR_TOP[6] = {22, 35, 48, 60, 71, 84};   /* each gear's top, in 80ths of the car's */

#define MS(x) ((int32_t)((x) * FX))      /* metres (a second, a second squared) */
#define A_LAT MS(24)
#define BRAKE MS(28)
#define DRIFT_BRAKE MS(12)
#define AI_BRAKE MS(20)
#define NITRO_A MS(7)
#define NITRO_TOP MS(13.5)
#define DRAFT_A MS(2.5)
#define DRAFT_TOP MS(5)
#define DRAG0 MS(2.1)
#define TURN_MAX 14394                  /* 1.38 radians a second, at most */
#define RAD 10430                       /* angle units in a radian */
#define COUNTDOWN (3 * PG_HZ + PG_HZ / 2)
#define ORB_WAIT 275                    /* steps before an orb comes back (5.5 s) */
#define GRID_GAP MS(8)

static int32_t clampi(int32_t v, int32_t lo, int32_t hi) { return v < lo ? lo : v > hi ? hi : v; }
static int32_t absi(int32_t v) { return v < 0 ? -v : v; }

uint32_t pg_sum(const void *data, uint32_t n) {
  const uint8_t *p = data;
  uint32_t h = 2166136261u;
  for (uint32_t i = 0; i < n; i++) h = (h ^ p[i]) * 16777619u;
  return h;
}

int pg_kmh(int32_t v) { return (int)((int64_t)v * 36 / (10 * FX)); }

static int32_t top_of(const pg_car *c) { return (int32_t)pg_vehicles[c->model].top * FX / 8; }
static int32_t gear_max(const pg_car *c, int g) { return top_of(c) * GEAR_TOP[g - 1] / 80; }

int pg_rpm(const pg_race *r) {
  const pg_car *c = &r->car[0];
  int red = pg_vehicles[c->model].redline;
  int32_t norm = (int32_t)((int64_t)c->v * 256 / gear_max(c, c->gear));
  int rpm = 1250 + (int)((int64_t)norm * (red - 1250) / 256);
  return rpm > red + 200 ? red + 200 : rpm;
}

/* the engine's push in the car's gear (FX/s^2), skill: 256 = all of it */
static int32_t engine(const pg_car *c, int skill) {
  int32_t gm = gear_max(c, c->gear);
  if (c->v >= gm + gm / 20) return 0;            /* on the limiter */
  int32_t norm = (int32_t)((int64_t)c->v * 16 / gm);
  norm = clampi(norm, 0, 17);
  int64_t a = (int64_t)pg_vehicles[c->model].accel * FX / 8;
  return (int32_t)(a * TORQUE[norm] / 256 * PULL[c->gear - 1] / 256 * skill / 256);
}

/* air and rolling: none of the engine's push is left at the car's top speed in top gear */
static int32_t drag(const pg_car *c) {
  int32_t top = top_of(c);
  int64_t a = (int64_t)pg_vehicles[c->model].accel * FX / 8;
  int32_t at_top = (int32_t)(a * 250 / 256 * PULL[5] / 256);
  return DRAG0 + (int32_t)((int64_t)(at_top - DRAG0) * c->v / top * c->v / top);
}

static int32_t grip_of(const pg_race *r, const pg_track *t, const pg_car *c) {
  int32_t g = (int32_t)pg_vehicles[c->model].grip * ZONE_GRIP[pg_zone_at(t, c->s)] / 128;
  if (absi(c->x) > PG_ROAD + PG_KERB) g = g * 92 / 128;
  (void)r;
  return g;                                      /* 1/128 */
}

static void shift_auto(pg_car *c) {
  int red = pg_vehicles[c->model].redline;
  int32_t norm = (int32_t)((int64_t)c->v * 256 / gear_max(c, c->gear));
  int rpm = 1250 + (int)((int64_t)norm * (red - 1250) / 256);
  if (rpm > red - 250 && c->gear < 6) c->gear++;
  else if (c->gear > 1 && rpm < 3600 && c->v < gear_max(c, c->gear - 1) * 4 / 5) c->gear--;
}

/* the leader burns nitro faster, so nobody hoards it out in front */
static bool leading(const pg_race *r, int i) {
  if (r->mode == MODE_ENDLESS || r->rivals == 0) return false;
  for (int j = 0; j <= r->rivals; j++)
    if (j != i && !r->car[j].finish_ms && r->car[j].s > r->car[i].s) return false;
  return true;
}

/* a car close in front, in line: you're in its slipstream */
static bool drafting(const pg_race *r, const pg_track *t, int i) {
  const pg_car *c = &r->car[i];
  if (c->v < MS(30)) return false;
  for (int j = 0; j <= r->rivals; j++) {
    if (j == i) continue;
    int32_t ds = pg_wrap(t, r->car[j].s - c->s);
    if (ds > MS(4.5) && ds < MS(22) && absi(r->car[j].x - c->x) < MS(1.9)) return true;
  }
  return false;
}

/* where a car goes in a step: along the road (more of it on the inside of a bend), across it, and how the bend
   turns it */
static void move(pg_track *t, pg_car *c) {
  int32_t k = pg_seg_of(t, c->s);
  int32_t cv = t->cv[pg_slot(t, k)];
  int32_t step = c->v / PG_HZ;
  int32_t kx = (int32_t)((int64_t)cv * c->x * 6283 / (1000LL * t->seg));
  int32_t den = clampi(65536 - kx, 40000, 90000);
  int32_t ds = (int32_t)((int64_t)step * pg_cos(c->ang) / 32767 * 65536 / den);
  int32_t dx = (int32_t)((int64_t)step * pg_sin(c->ang) / 32767);
  c->s += ds;
  c->x += dx;
  c->ang = (int16_t)(c->ang - (int32_t)((int64_t)cv * ds / t->seg));
}

static void barriers(pg_race *r, pg_track *t, pg_car *c) {
  int32_t lim = PG_WALL - PG_HW;
  if (absi(c->x) > lim) {
    bool right = c->x > 0;
    c->x = right ? lim : -lim;
    int32_t a = c->ang;
    if ((right && a > 0) || (!right && a < 0)) {
      /* into it: the speed across is lost, a little more besides, and the car turns along it (the way it was
         going: forwards, or backwards) */
      c->v = (int32_t)((int64_t)c->v * absi(pg_cos(a)) / 32767 * 15 / 16);
      bool back = a > 16384 || a < -16384;
      int32_t rel = back ? a - (a > 0 ? 32768 : -32768) : a;
      c->ang = (int16_t)(back ? rel / 4 + (a > 0 ? 32768 : -32768) : rel / 4);
      c->bump = 12;
    }
    c->v -= c->v / 160;                          /* scraping along */
    c->drift = 0;
  }
  /* the endless road behind you isn't kept: it's a wall too */
  if (t->endless) {
    int32_t back = (t->first + 6) * t->seg;
    if (c->s < back) c->s = back, c->v = 0;
  }
  (void)r;
}

/* ------------------------------------------------------------------ your car */
static void drive(pg_race *r, pg_track *t, unsigned in, int shift) {
  pg_car *c = &r->car[0];
  const pg_vehicle *veh = &pg_vehicles[c->model];
  bool racing = r->phase == PH_RACE;
  if (!racing) in = r->phase == PH_GRID ? in & (IN_LEFT | IN_RIGHT) : 0;
  /* the wheel turns in quickly and comes back by itself */
  if ((in & IN_LEFT) && !(in & IN_RIGHT)) c->steer = (int16_t)clampi(c->steer - (c->steer > 0 ? 200 : 92), -1024, 1024);
  else if ((in & IN_RIGHT) && !(in & IN_LEFT)) c->steer = (int16_t)clampi(c->steer + (c->steer < 0 ? 200 : 92), -1024, 1024);
  else c->steer = (int16_t)(absi(c->steer) < 24 ? 0 : c->steer * 7 / 8);
  if (r->phase == PH_GRID) return;
  /* gears */
  if (r->manual) {
    if (shift > 0 && c->gear < 6) c->gear++;
    if (shift < 0 && c->gear > 1) c->gear--;
  } else {
    shift_auto(c);
  }
  /* nitro */
  bool lead = leading(r, 0);
  c->boost = (in & IN_NITRO) && c->nitro > 5;
  int burn = lead ? 9 : 5;
  if (c->boost) c->nitro = (uint16_t)(c->nitro > burn ? c->nitro - burn : 0);
  bool draft = drafting(r, t, 0);
  r->slipstream = draft ? (uint16_t)(r->slipstream + 1) : 0;
  /* a slide: braking while steering hard at speed */
  int32_t v = c->v;
  if ((in & IN_BRAKE) && absi(c->steer) >= 512 && v >= MS(22)) c->drift = 1;
  if (!(in & IN_BRAKE) || v < MS(15)) c->drift = 0;
  /* along */
  int32_t a = 0;
  if ((in & IN_GAS) || c->boost) {
    int32_t e = engine(c, 256);
    a += c->drift ? e / 2 : e;
    if (draft) a += DRAFT_A;
    if (c->boost) a += NITRO_A;
  }
  if (in & IN_BRAKE) a -= c->drift ? DRIFT_BRAKE : BRAKE;
  a -= drag(c);
  bool grass = absi(c->x) > PG_ROAD + PG_KERB;
  if (grass) a -= (int32_t)((MS(6) + (int64_t)v * v * 12 / 1000 / FX) * veh->offroad / 128);
  if (c->drift) a -= v / 25;                     /* the slide scrubs speed off */
  v += a / PG_HZ;
  int32_t top = top_of(c) + (c->boost ? NITRO_TOP : 0) + (draft ? DRAFT_TOP : 0);
  if (v > top) v = v - (v - top) / 8 > top ? v - (v - top) / 8 : top;
  if (v < 0) v = 0;
  c->v = v;
  /* turning: as far as the tyres hold */
  int32_t grip = grip_of(r, t, c);
  int32_t lat = A_LAT * grip / 128;
  int32_t hold = v > MS(1) ? (int32_t)((int64_t)lat * RAD / v) : TURN_MAX;   /* units a second */
  int32_t lock = hold + hold / 6 < TURN_MAX ? hold + hold / 6 : TURN_MAX;
  int32_t auth = v >= MS(12) ? 256 : v * 256 / MS(12);
  int32_t want = (int32_t)((int64_t)c->steer * lock / 1024 * auth / 256);
  int32_t can = c->drift ? hold * 13 / 10 : hold;
  int32_t turn = clampi(want, -can, can);
  if (want != turn) c->v -= c->v / 400;          /* asking more than the tyres give: they scrub */
  c->ang = (int16_t)(c->ang + turn / PG_HZ);
  /* the body in a slide: turned into the bend, by how much the car slides */
  int32_t slip_to = c->drift ? (int32_t)((int64_t)c->steer * 4700 / 1024 * veh->drift / 128) : 0;
  c->slip = (int16_t)(c->slip + (slip_to - c->slip) / 6);
  move(t, c);
  barriers(r, t, c);
  if (c->bump) c->bump--;
}

/* ------------------------------------------------------------------ the rivals */
static const int16_t LEVEL_CORNER[3] = {214, 234, 254};   /* how close to the limit they take bends (1/256) */
static const int16_t LEVEL_PULL[3] = {226, 242, 256};

static void rival(pg_race *r, pg_track *t, int i) {
  pg_car *c = &r->car[i];
  int lv = r->level <= LEVEL_HARD ? r->level : LEVEL_NORMAL;
  int skill_c = LEVEL_CORNER[lv] - (i == 0 ? 0 : (i - 1) * 3), skill_e = LEVEL_PULL[lv] - (i == 0 ? 0 : (i - 1) * 2);
  bool racing = r->phase == PH_RACE || (r->phase == PH_DONE && r->mode == MODE_RACE);
  int32_t top = top_of(c);
  /* a little help or a little holding back, away from you (not on Hard) */
  if (i && lv != LEVEL_HARD && r->car[0].finish_ms == 0) {
    int32_t gap = r->car[i].s - r->car[0].s;
    if (gap > MS(150)) top = top * 246 / 256, skill_e = skill_e * 240 / 256;
    else if (gap < -MS(120)) top = top * 262 / 256, skill_c += 6;
  }
  if (c->finish_ms) skill_c -= 16, top = top * 7 / 8;   /* a lap of honour */
  int32_t k0 = pg_seg_of(t, c->s);
  /* every few steps: the speed for what comes (the slowest bend ahead, braking from here) and the line */
  if (c->think == 0) {
    c->think = 4;
    int32_t grip = grip_of(r, t, c);
    int64_t aim2 = (int64_t)(top + (c->boost ? NITRO_TOP : 0)) * (top + (c->boost ? NITRO_TOP : 0));
    for (int k = 0; k < 24; k++) {
      int sl = pg_slot(t, k0 + k);
      int64_t vl = (int64_t)t->limit[sl] * FX;
      vl = vl * pg_isqrt((uint64_t)grip * 128) / 128 * skill_c / 256;   /* (speed grows with the root of grip) */
      int64_t d = (int64_t)(k0 + k) * t->seg - c->s;
      if (d < 0) d = 0;
      int64_t v2 = vl * vl + 2 * (int64_t)AI_BRAKE * d;
      if (v2 < aim2) aim2 = v2;
    }
    c->aim = (uint16_t)(pg_isqrt((uint64_t)aim2) / 8 > 65535 ? 65535 : pg_isqrt((uint64_t)aim2) / 8);
    /* the line: out before a bend, in at its middle, with each rival a little to its own side */
    int32_t near = 0, far = 0;
    for (int k = 0; k < 4; k++) near += t->cv[pg_slot(t, k0 + k)];
    for (int k = 4; k < 13; k++) far += t->cv[pg_slot(t, k0 + k)];
    int32_t line = near * 3 - far;              /* angle units */
    int32_t want = (int32_t)((int64_t)line * MS(6) / 3000) + ((i * 37) % 7 - 3) * MS(0.6);
    /* a slower car ahead in the way: go round it, on the side with room; none: follow it */
    int blocker = -1;
    int32_t best = MS(18);
    for (int j = 0; j <= r->rivals; j++) {
      if (j == i) continue;
      int32_t ds = pg_wrap(t, r->car[j].s - c->s);
      if (ds > 0 && ds < best && absi(r->car[j].x - c->x) < MS(2.8) && r->car[j].v < c->v + MS(1)) best = ds, blocker = j;
    }
    if (blocker >= 0) {
      int32_t bx = r->car[blocker].x;
      int32_t lft = bx - MS(3.4), rgt = bx + MS(3.4);
      bool lok = lft > -(PG_ROAD - MS(1.2)), rok = rgt < PG_ROAD - MS(1.2);
      for (int j = 0; j <= r->rivals; j++) {
        if (j == i || j == blocker) continue;
        int32_t ds = pg_wrap(t, r->car[j].s - c->s);
        if (ds < -MS(6) || ds > MS(24)) continue;
        if (absi(r->car[j].x - lft) < MS(2.6)) lok = false;
        if (absi(r->car[j].x - rgt) < MS(2.6)) rok = false;
      }
      if (lok && (!rok || absi(lft - c->x) <= absi(rgt - c->x))) want = lft;
      else if (rok) want = rgt;
      else if (c->aim > (r->car[blocker].v + MS(0.5)) / 8) c->aim = (uint16_t)((r->car[blocker].v + MS(0.5)) / 8);
    }
    /* an orb close to the line, when nitro runs low */
    if (c->nitro < 850 && r->mode == MODE_RACE) {
      for (int k = 1; k <= 5; k++) {
        int sl = pg_slot(t, k0 + k);
        if (t->orb[sl] == -128) continue;
        int id = t->orb_id[sl];
        if (id >= 0 && r->orb_wait[id]) break;
        int32_t ox = t->orb[sl] * FX / 8;
        if (absi(ox - c->x) < MS(5.2) && ((k0 + k) * 7 + i * 13) % 8 < 4 && blocker < 0) want = ox;
        break;
      }
    }
    c->lane = clampi(want, -(PG_ROAD - MS(1.4)), PG_ROAD - MS(1.4));
  }
  c->think--;
  /* nitro on the straights */
  int32_t aim = (int32_t)c->aim * 8;
  if (!racing) aim = 0;
  if (!c->boost) {
    if (racing && c->nitro >= 320 && aim >= top && c->v > MS(38)) c->boost = 1;
  } else if (c->nitro <= 5 || aim < top || !racing) {
    c->boost = 0;
  }
  if (c->boost) {
    int burn = leading(r, i) ? 9 : 5;
    c->nitro = (uint16_t)(c->nitro > burn ? c->nitro - burn : 0);
  }
  /* gas or brakes */
  if (c->gear < 1 || c->gear > 6) c->gear = 1;
  shift_auto(c);
  int32_t a = -drag(c);
  if (c->v > aim + MS(1)) a -= AI_BRAKE;
  else if (c->v < aim) {
    a += engine(c, skill_e);
    if (c->boost) a += NITRO_A;
    if (drafting(r, t, i)) a += DRAFT_A;
  }
  int32_t v = c->v + a / PG_HZ;
  int32_t cap = top + (c->boost ? NITRO_TOP : 0) + MS(3);
  if (absi(c->x) > PG_ROAD + PG_KERB) cap = cap * 3 / 4;
  c->v = clampi(v, 0, cap);
  /* across, towards the line, and pointing where it goes */
  int32_t dx = c->lane - c->x, vx_max = MS(5) / PG_HZ;
  int32_t vx = clampi(dx / 6, -vx_max, vx_max);
  if (c->v < MS(3)) vx = 0;
  int32_t head = c->v > MS(1) ? pg_atan2(vx * PG_HZ, c->v) : 0;
  c->ang = (int16_t)(head > 32767 ? head - 65536 : head);
  c->steer = (int16_t)clampi(c->ang * 8, -1024, 1024);
  /* along the road, as the road goes */
  int32_t keep = c->ang;
  move(t, c);
  c->ang = (int16_t)keep;
  c->slip = 0, c->drift = 0;
  barriers(r, t, c);
  if (c->bump) c->bump--;
}

/* ------------------------------------------------------------------ cars touching */
static void touch(pg_race *r, pg_track *t) {
  int n = r->mode == MODE_ENDLESS ? 1 : r->rivals + 1;
  for (int pass = 0; pass < 3; pass++)
    for (int a = 0; a < n; a++)
      for (int b = a + 1; b < n; b++) {
        pg_car *A = &r->car[a], *B = &r->car[b];
        int32_t ds = pg_wrap(t, B->s - A->s), dx = B->x - A->x;
        int32_t ps = 2 * PG_HL - absi(ds), px = 2 * PG_HW - absi(dx);
        if (ps <= 0 || px <= 0) continue;
        if ((int64_t)ps * PG_HW < (int64_t)px * PG_HL) {
          /* nose to tail: the car behind stops at the other's bumper and gives it a push */
          pg_car *back = ds > 0 ? A : B, *front = ds > 0 ? B : A;
          back->s -= ps * 3 / 4 + 1, front->s += ps - ps * 3 / 4;
          if (back->v > front->v) {
            int32_t d = back->v - front->v;
            back->v = front->v - d / 8 > 0 ? front->v - d / 8 : 0, front->v += d / 3;
          }
          back->bump = front->bump = 10;
        } else {
          /* side by side: pushed apart, both straightened */
          pg_car *left = dx > 0 ? A : B, *right = dx > 0 ? B : A;
          int32_t lw = -(PG_WALL - PG_HW), rw = PG_WALL - PG_HW;
          int32_t half = px / 2 + 1;
          int32_t lx = left->x - half, rx = right->x + (px - px / 2 + 1);
          if (lx < lw) rx += lw - lx, lx = lw;    /* against a barrier: the other takes all of it */
          if (rx > rw) lx -= rx - rw, rx = rw;
          if (lx < lw) lx = lw;
          left->x = lx, right->x = rx;
          if (left->ang > 0) left->ang = (int16_t)(left->ang / 2);
          if (right->ang < 0) right->ang = (int16_t)(right->ang / 2);
          left->v -= left->v / 64, right->v -= right->v / 64;
          left->bump = right->bump = 10;
          left->drift = right->drift = 0;
        }
      }
}

/* ------------------------------------------------------------------ orbs */
static void orbs(pg_race *r, pg_track *t, int i, int32_t s0) {
  pg_car *c = &r->car[i];
  int32_t k0 = pg_seg_of(t, s0), k1 = pg_seg_of(t, c->s);
  for (int32_t k = k0; k <= k1 && k <= k0 + 2; k++) {
    int sl = pg_slot(t, k);
    if (t->orb[sl] == -128) continue;
    int32_t mid = k * t->seg + t->seg / 2;
    if (!(s0 < mid && c->s >= mid)) continue;
    if (absi(t->orb[sl] * FX / 8 - c->x) >= MS(2.6)) continue;
    int gain;
    if (r->mode == MODE_ENDLESS) {
      if (k <= r->last_orb) continue;
      r->last_orb = k, gain = 350;
    } else {
      int id = t->orb_id[sl];
      if (id < 0 || r->orb_wait[id]) continue;
      r->orb_wait[id] = ORB_WAIT;
      gain = leading(r, i) ? 180 : 300;
    }
    c->nitro = (uint16_t)(c->nitro + gain > 1000 ? 1000 : c->nitro + gain);
    if (i == 0) r->flash = 25;
  }
}

/* ------------------------------------------------------------------ the race */
static void banner(pg_race *r, int b, int arg) { r->banner = (uint8_t)b, r->banner_arg = (uint8_t)arg, r->banner_t = 2 * PG_HZ; }

void pg_new(pg_race *r, pg_track *t, const pg_setup *s, uint32_t seed) {
  memset(r, 0, sizeof *r);
  r->mode = s->mode <= MODE_ENDLESS ? s->mode : MODE_RACE;
  r->track = s->track < PG_CIRCUITS ? s->track : 0;
  r->vehicle = s->vehicle < PG_VEHICLES ? s->vehicle : 0;
  r->laps = s->laps >= 1 && s->laps <= PG_MAX_LAPS ? s->laps : 3;
  r->rivals = r->mode == MODE_ENDLESS ? 0 : s->rivals <= PG_CARS - 1 ? s->rivals : 5;
  r->level = s->level <= LEVEL_HARD ? s->level : LEVEL_NORMAL;
  r->manual = s->manual ? 1 : 0;
  r->seed = seed ? seed : 1;
  r->phase = PH_GRID, r->countdown = COUNTDOWN, r->lap = 1;
  if (r->mode == MODE_ENDLESS) pg_endless(t, r->seed);
  else pg_build(t, r->track);
  uint32_t rng = r->seed;
  int n = r->rivals + 1;
  for (int i = 0; i < n; i++) {
    pg_car *c = &r->car[i];
    /* the grid: two by two behind the line, you at the back */
    int slot = i == 0 ? n - 1 : i - 1;
    c->s = -(MS(10) + (slot / 2) * GRID_GAP + (slot & 1) * GRID_GAP / 2);
    c->x = r->mode == MODE_ENDLESS ? 0 : (slot & 1) ? MS(3.2) : -MS(3.2);
    c->lane = c->x, c->gear = 1, c->nitro = 300;
    c->model = i == 0 ? r->vehicle : (uint8_t)((i + pg_rng(&rng) % 3) % 3);
    c->paint = (uint8_t)(i == 0 ? 0 : (i + 2) % 8);
    c->driven = i != 0, c->think = (uint16_t)(i % 4);
  }
  if (r->mode == MODE_ENDLESS) {
    r->car[0].s = MS(30), r->car[0].nitro = 1000;
    r->time_left = 30000, r->last_orb = -1;
  }
}

/* time a zone of the endless road gives: 8 s, a quarter of a second less each zone, 3 s at least */
int32_t pg_zone_bonus(int zone) {
  int32_t b = 8000 - 250 * (zone - 1);
  return b < 3000 ? 3000 : b;
}

void pg_step(pg_race *r, pg_track *t, unsigned input, int shift) {
  r->ticks++;
  if (r->mode == MODE_ENDLESS) pg_extend(t, pg_seg_of(t, r->car[0].s) + 48);
  if (r->phase == PH_GRID) {
    drive(r, t, input, 0);                       /* (the wheel turns; the car stays) */
    if (r->countdown) r->countdown--;
    if (!r->countdown) r->phase = PH_RACE;
    return;
  }
  if ((r->phase == PH_RACE || (r->phase == PH_DONE && r->mode == MODE_RACE)) && r->time_ms < 99u * 60000u) r->time_ms += PG_TICK;
  if (r->flash) r->flash--;
  int n = r->rivals + 1;
  int32_t before[PG_CARS];
  for (int i = 0; i < n; i++) before[i] = r->car[i].s;
  if (r->car[0].driven) rival(r, t, 0);
  else drive(r, t, input, shift);
  for (int i = 1; i < n; i++) rival(r, t, i);
  touch(r, t);
  for (int i = 0; i < n; i++) orbs(r, t, i, before[i]);
  for (int k = 0; k < PG_ORBS; k++)
    if (r->orb_wait[k]) r->orb_wait[k]--;
  if (r->banner_t && !--r->banner_t) r->banner = BANNER_NONE;
  pg_car *me = &r->car[0];
  /* wrong way: pointing back down the road and moving */
  bool back = me->ang > 18000 || me->ang < -18000;
  r->wrong = (uint8_t)(back && me->v > MS(4) && r->phase == PH_RACE ? (r->wrong < 255 ? r->wrong + 1 : 255) : 0);
  if (r->mode == MODE_ENDLESS) {
    int zone = (int)(pg_seg_of(t, me->s) / PG_ZONE);
    if (zone > r->zone && r->phase == PH_RACE) {
      r->zone = (uint8_t)(zone < 255 ? zone : 255);
      r->time_left += pg_zone_bonus(zone);
      banner(r, BANNER_ZONE, zone);
    }
    if (r->phase == PH_RACE) {
      r->time_left -= PG_TICK;
      if (r->time_left <= 0) r->time_left = 0, r->phase = PH_DONE, r->end = END_TIME, me->drift = 0;
    } else {
      r->done = (uint16_t)(r->done < 65535 ? r->done + 1 : 65535);
      me->v -= me->v / 20 + MS(0.2);             /* out of time: the car rolls to a stop */
      if (me->v < 0) me->v = 0;
    }
    return;
  }
  /* laps, the finish */
  for (int i = 0; i < n; i++) {
    pg_car *c = &r->car[i];
    int32_t L = t->length;
    if (c->s < -L) c->s += L;                    /* (a whole lap backwards: the same place) */
    if (c->finish_ms) {
      if (c->s >= (int32_t)(r->laps + 1) * L) c->s -= L;
      continue;
    }
    if (c->s >= (int32_t)r->laps * L) {
      c->finish_ms = r->time_ms ? r->time_ms : 1;
      c->place = (uint8_t)++r->finished;
      if (i == 0) {
        r->phase = PH_DONE, r->end = END_FINISH, c->driven = 1, c->drift = 0, c->boost = 0;
        uint32_t lap = r->time_ms - r->lap_at;
        r->last_lap_ms = lap;
        if (!r->best_lap_ms || lap < r->best_lap_ms) r->best_lap_ms = lap;
      }
    } else if (i == 0) {
      int lap = c->s < 0 ? 1 : (int)(c->s / L) + 1;
      if (lap > r->lap) {
        uint32_t ms = r->time_ms - r->lap_at;
        r->lap_at = r->time_ms, r->last_lap_ms = ms;
        bool best = !r->best_lap_ms || ms < r->best_lap_ms;
        if (best) r->best_lap_ms = ms;
        r->lap = (uint8_t)lap;
        banner(r, lap == r->laps ? BANNER_FINAL : BANNER_LAP, lap);
      }
    }
  }
  /* (99 minutes and still not over the line: out of time) */
  if (r->phase == PH_RACE && r->time_ms >= 99u * 60000u - PG_TICK) r->phase = PH_DONE, r->end = END_TIME, me->driven = 1;
  if (r->phase == PH_DONE) r->done = (uint16_t)(r->done < 65535 ? r->done + 1 : 65535);
  if (r->wrong > PG_HZ && r->banner != BANNER_WRONG) banner(r, BANNER_WRONG, 0);
}

int pg_rank(const pg_race *r, int car) {
  const pg_car *c = &r->car[car];
  if (c->place) return c->place;
  int rank = 1 + r->finished;
  for (int j = 0; j <= r->rivals; j++)
    if (j != car && !r->car[j].place && (r->car[j].s > c->s || (r->car[j].s == c->s && j < car))) rank++;
  return rank;
}

int32_t pg_lap_s(const pg_race *r, const pg_track *t) {
  int32_t s = r->car[0].s;
  if (t->endless) return s;
  int32_t L = t->length, m = s % L;
  return m < 0 ? m + L : m;
}

/* everything in range: what the game makes, and what a damaged save might hold */
bool pg_valid(const pg_race *r, const pg_track *t) {
  if (r->mode > MODE_ENDLESS || r->track >= PG_CIRCUITS || r->vehicle >= PG_VEHICLES) return false;
  if (r->laps < 1 || r->laps > PG_MAX_LAPS || r->rivals > PG_CARS - 1 || r->level > LEVEL_HARD) return false;
  if (r->manual > 1 || r->phase > PH_DONE || r->end > END_TIME || r->flash > 25) return false;
  if (r->mode == MODE_ENDLESS && r->rivals) return false;
  if (r->lap < 1 || r->lap > r->laps || r->finished > r->rivals + 1 || r->banner > BANNER_WRONG) return false;
  if (r->countdown > COUNTDOWN || (r->phase == PH_GRID) != (r->countdown > 0 && r->time_ms == 0)) return false;
  if (r->phase != PH_DONE && (r->end || r->done)) return false;
  if (r->phase == PH_DONE && !r->end) return false;
  if (r->banner_t > 2 * PG_HZ || r->lap_at > r->time_ms || r->best_lap_ms > r->time_ms || r->last_lap_ms > r->time_ms)
    return false;
  if (r->time_ms > 99 * 60000u || r->seed == 0) return false;
  for (int k = 0; k < PG_ORBS; k++)
    if (r->orb_wait[k] > ORB_WAIT) return false;
  if (r->mode == MODE_ENDLESS) {
    if (r->time_left < 0 || r->time_left > 30 * 60000 || r->last_orb < -1) return false;
    if (r->phase == PH_DONE && r->end != END_TIME) return false;
  } else if (r->end == END_TIME && r->car[0].finish_ms) {
    return false;
  } else if (r->time_left || r->last_orb || r->zone) {
    return false;
  }
  int32_t L = t && !t->endless ? t->length : MS(1800);
  int32_t lo = r->mode == MODE_ENDLESS ? 0 : -L - MS(100);
  int32_t hi = r->mode == MODE_ENDLESS ? MS(2000000) : (int32_t)(r->laps + 2) * L;
  for (int i = 0; i < PG_CARS; i++) {
    const pg_car *c = &r->car[i];
    if (i > r->rivals) {
      const uint8_t *b = (const uint8_t *)c;
      for (unsigned k = 0; k < sizeof *c; k++)
        if (b[k]) return false;
      continue;
    }
    if (c->s < lo || c->s > hi || c->x < -(PG_WALL - PG_HW) || c->x > PG_WALL - PG_HW) return false;
    if (c->lane < -PG_ROAD || c->lane > PG_ROAD || c->v < 0 || c->v > MS(130)) return false;
    if (c->steer < -1024 || c->steer > 1024 || c->nitro > 1000 || c->gear < 1 || c->gear > 6) return false;
    if (c->boost > 1 || c->drift > 1 || c->model >= PG_VEHICLES || c->paint > 7 || c->bump > 12 || c->driven > 1) return false;
    if (c->place > r->rivals + 1 || (c->place != 0) != (c->finish_ms != 0) || c->finish_ms > r->time_ms + 1) return false;
    if (c->think > 4 || (i == 0 && c->model != r->vehicle)) return false;
    if (i > 0 ? !c->driven : c->driven != (c->finish_ms || (r->mode == MODE_RACE && r->end == END_TIME) ? 1 : 0)) return false;
  }
  return true;
}
