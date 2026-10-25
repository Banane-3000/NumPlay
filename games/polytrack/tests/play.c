/* PolyGT 3D on a computer, for tests and pictures (make host, make test).
 *
 *   play test DIR        the roads, laps, physics, crashes, the rivals finishing every circuit at every level, a
 *                        driver finishing races, the endless road, the whole app through its keys, saves written and
 *                        read back, damaged saves, a full storage, a few hundred sessions of random keys, and the same
 *                        race twice, to the same hash on every computer
 *   play shots DIR       pictures of the screens (DIR/<name>.ppm)
 *   play docs DIR        the README's pictures (DIR/<name>.ppm)
 *   play film DIR MS T [VIEW CAR STEP]   MS milliseconds of a race on circuit T, a driver at the wheel, a frame every
 *                        STEP ms (DIR/<frame>_<ms>.raw, for tools/record.py --frames)
 */
#define main pg_app_main
#include "../src/main.c"
#undef main
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

/* ---------------------------------------------------------------- the calculator, on a computer */
static uint16_t fb[240][320];
void plat_push(int x, int y, int w, int h, const uint16_t *px) {
  for (int j = 0; j < h; j++)
    if (y + j < 240)
      for (int i = 0; i < w; i++) fb[y + j][x + i] = px[j * w + i];
}
void *np_alloc(uint32_t n) { (void)n; return 0; }
static double clock_ms;
static uint32_t (*script)(uint32_t frame);
static uint32_t frame_no, quit_at;
static void (*watch)(void);
uint32_t plat_keys(void) {
  clock_ms += 5;
  frame_no++;
  if (watch) watch();
  if (frame_no >= quit_at) return PK_HOME;
  return script ? script(frame_no) : 0;
}
uint32_t plat_millis(void) { return (uint32_t)clock_ms; }
void plat_sleep(uint32_t ms) { clock_ms += ms; }
void plat_vblank(void) {}
static uint32_t rnd_state = 1;
uint32_t plat_random(void) { return pg_rng(&rnd_state); }
bool plat_slow(void) { return false; }
void plat_begin(void) {}
int plat_end(void) { return 0; }

static struct { char name[32]; uint8_t data[4096]; uint32_t len; bool on; } files[4];
static bool storage_full;
bool plat_save(const char *name, const void *data, uint32_t len) {
  if (storage_full) return false;
  int k = -1;
  for (int i = 0; i < 4; i++)
    if (files[i].on && !strcmp(files[i].name, name)) k = i;
  for (int i = 0; i < 4 && k < 0; i++)
    if (!files[i].on) k = i;
  if (k < 0 || len > sizeof files[k].data) return false;
  strcpy(files[k].name, name), memcpy(files[k].data, data, len), files[k].len = len, files[k].on = true;
  return true;
}
const uint8_t *plat_load(const char *name, uint32_t *len) {
  for (int i = 0; i < 4; i++)
    if (files[i].on && !strcmp(files[i].name, name)) return *len = files[i].len, files[i].data;
  return 0;
}
static void forget(const char *name) {
  for (int i = 0; i < 4; i++)
    if (files[i].on && !strcmp(files[i].name, name)) files[i].on = false;
}
static uint8_t *file_data(const char *name, uint32_t *len) {
  for (int i = 0; i < 4; i++)
    if (files[i].on && !strcmp(files[i].name, name)) return *len = files[i].len, files[i].data;
  return 0;
}

static void shot(const char *dir, const char *name) {
  char path[512];
  snprintf(path, sizeof path, "%s/%s.ppm", dir, name);
  FILE *f = fopen(path, "wb");
  if (!f) exit(2);
  fprintf(f, "P6 320 240 255\n");
  for (int y = 0; y < 240; y++)
    for (int x = 0; x < 320; x++) {
      uint16_t c = fb[y][x];
      fputc((c >> 11) * 255 / 31, f), fputc((c >> 5 & 63) * 255 / 63, f), fputc((c & 31) * 255 / 31, f);
    }
  fclose(f);
}

static int fails;
static const char *const LEVELS[3] = {"Easy", "Normal", "Hard"};
#define RACE_HASH 0x8932C86Au   /* (made on a Mac with clang; Linux with GCC must agree) */
#define CHECK(c, ...) do { if (!(c)) { fails++; printf("FAIL %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

/* ---------------------------------------------------------------- a driver */
/* it drives as a good player would: the rivals' line (out, in, out), steering for the bend ahead with taps of the
   keys, braking for the slowest bend coming up with the tyres it has, round slower cars, nitro on the straights;
   skill 0..100 (how much of the grip it dares) */
static int bot_skill = 100;
static int32_t bot_line;
static unsigned bot(const pg_race *r, const pg_track *t) {
  const pg_car *c = &r->car[0];
  int32_t v = c->v, k0 = pg_seg_of(t, c->s);
  int32_t grip = (int32_t)pg_vehicles[c->model].grip * (t->zone[pg_slot(t, k0)] == 2 ? 118 : 128) / 128;
  if (abs(c->x) > PG_ROAD + PG_KERB) grip = grip * 92 / 128;
  int64_t aim2 = (int64_t)200 * FX * 200 * FX;
  for (int k = 0; k < 30; k++) {
    int sl = pg_slot(t, k0 + k);
    int64_t vl = (int64_t)t->limit[sl] * FX;
    vl = vl * pg_isqrt((uint64_t)grip * 128) / 128 * (84 + bot_skill * 18 / 100) / 100;
    int64_t d = (int64_t)(k0 + k) * t->seg - c->s - 4 * FX;
    if (d < 0) d = 0;
    int64_t v2 = vl * vl + 2LL * 24 * FX * d;
    if (v2 < aim2) aim2 = v2;
  }
  int32_t aim = (int32_t)pg_isqrt((uint64_t)aim2);
  /* the line: out before a bend, in at its middle */
  int32_t near = 0, far = 0;
  for (int k = 0; k < 4; k++) near += t->cv[pg_slot(t, k0 + k)];
  for (int k = 4; k < 13; k++) far += t->cv[pg_slot(t, k0 + k)];
  int32_t line = (int32_t)((int64_t)(near * 3 - far) * 6 * FX / 3000);
  /* a slower car close ahead in the way: go round it */
  for (int j = 1; j <= r->rivals; j++) {
    int32_t ds = pg_wrap(t, r->car[j].s - c->s);
    if (ds > 0 && ds < 25 * FX && abs(r->car[j].x - c->x) < 3 * FX && r->car[j].v < v + FX)
      bot_line = r->car[j].x > 0 ? r->car[j].x - 4 * FX : r->car[j].x + 4 * FX;
  }
  bot_line -= bot_line / 32;
  int32_t target = line + bot_line;
  if (target > 6 * FX) target = 6 * FX;
  if (target < -6 * FX) target = -6 * FX;
  unsigned in = 0;
  if (v > aim + FX / 2) in |= IN_BRAKE;
  else in |= IN_GAS;
  /* steering: what the bend needs at this speed, plus a heading back towards the line */
  int32_t cv = t->cv[pg_slot(t, k0)];
  int32_t want_ang = (target - c->x) / 16;
  if (want_ang > 2500) want_ang = 2500;
  if (want_ang < -2500) want_ang = -2500;
  int64_t road = (int64_t)cv * v / t->seg;                       /* angle units a second */
  int64_t turn = road + (int64_t)(want_ang - c->ang) * 4;        /* ...and the correction, over a quarter second */
  int32_t lat = 24 * FX * grip / 128;
  int32_t hold = v > FX ? (int32_t)((int64_t)lat * 10430 / v) : 14394;
  int32_t lock = hold + hold / 6 < 14394 ? hold + hold / 6 : 14394;
  int32_t auth = v >= 12 * FX ? 256 : v * 256 / (12 * FX);
  int32_t full = (int32_t)((int64_t)lock * auth / 256);
  int32_t steer = full > 0 ? (int32_t)(turn * 1024 / full) : 0;
  if (steer > 1024) steer = 1024;
  if (steer < -1024) steer = -1024;
  if (c->steer < steer - 60) in |= IN_RIGHT;
  else if (c->steer > steer + 60) in |= IN_LEFT;
  if (!(in & IN_BRAKE) && aim > v + 20 * FX && c->nitro > 300 && abs(c->ang) < 1500) in |= IN_NITRO;
  return in;
}

static uint32_t hash_race(const pg_race *r, uint32_t h) { return h ^ pg_sum(r, sizeof *r) * 16777619u; }

/* ---------------------------------------------------------------- the roads */
static pg_track TT;
static pg_setup plain(int mode, int id, int vehicle, int rivals, int level) {
  pg_setup s;
  memset(&s, 0, sizeof s);
  s.mode = (uint8_t)mode, s.track = (uint8_t)id, s.vehicle = (uint8_t)vehicle, s.laps = 3, s.rivals = (uint8_t)rivals;
  s.level = (uint8_t)level;
  return s;
}

static int64_t dist2(const pg_track *t, int a, int b) {
  int64_t dx = (t->px[a] - t->px[b]) / FX, dz = (t->pz[a] - t->pz[b]) / FX;
  return dx * dx + dz * dz;
}

static void roads(void) {
  for (int id = 0; id < PG_CIRCUITS; id++) {
    pg_build(&TT, id);
    int n = TT.n;
    CHECK(n > 200 && n <= PG_MAXN && TT.length == n * TT.seg, "%s: %d segments", pg_circuit_name[id], n);
    int64_t turn = 0;
    int jerk = 0, steep = 0, close = 0, gaps = 0;
    for (int k = 0; k < n; k++) {
      turn += TT.cv[k];
      if (abs(TT.cv[(k + 1) % n] - TT.cv[k]) > 1200) jerk++;
      if (abs(TT.py[(k + 1) % n] - TT.py[k]) > FX * 9 / 10) steep++;   /* no more than 15% */
      int64_t d2 = dist2(&TT, k, (k + 1) % n);
      if (d2 < 5 * 5 || d2 > 7 * 7) gaps++;
      for (int j = k + 8; j < n && j < k + n - 8; j++)
        if (dist2(&TT, k, j) < 32 * 32) close++;
    }
    CHECK(turn == ANG_TURN, "%s: turns %lld, not one turn round", pg_circuit_name[id], (long long)turn);
    CHECK(!jerk && !steep && !gaps, "%s: %d sudden bends, %d steep, %d uneven segments", pg_circuit_name[id], jerk, steep, gaps);
    CHECK(!close, "%s: the road comes within 32 m of itself %d times", pg_circuit_name[id], close);
    int lo = 255;
    for (int k = 0; k < n; k++)
      if (TT.limit[k] < lo) lo = TT.limit[k];
    CHECK(lo >= 15 && TT.orbs >= 10, "%s: tightest bend %d m/s, %d orbs", pg_circuit_name[id], lo, TT.orbs);
    printf("  %-17s %3d segments, %.2f km, tightest bend %d km/h, %d orbs\n", pg_circuit_name[id], n, n * TT.seg / 256000.0,
           lo * 36 / 10, TT.orbs);
  }
  /* the endless road: laid on and on, always joined up, never too steep, within its heights */
  pg_endless(&TT, 777);
  int bad = 0, zones[5] = {0};
  int32_t ylo = 0, yhi = 0;
  for (int32_t k = 0; k < 20000; k += 7) {
    pg_extend(&TT, k + 48);
    for (int32_t j = k; j < k + 40; j++) {
      int a = pg_slot(&TT, j), b = pg_slot(&TT, j + 1);
      int64_t d2 = dist2(&TT, a, b);
      if (d2 < 5 * 5 || d2 > 7 * 7) bad++;
      if (abs(TT.py[b] - TT.py[a]) > FX * 3 / 4) bad++;
      if (TT.py[a] < ylo) ylo = TT.py[a];
      if (TT.py[a] > yhi) yhi = TT.py[a];
      if (TT.zone[a] < 3 || TT.zone[a] > 7) bad++;
      else zones[TT.zone[a] - 3]++;
    }
  }
  CHECK(!bad && ylo > -100 * FX && yhi < 100 * FX, "the endless road: %d bad joins, heights %d..%d m", bad, ylo / FX, yhi / FX);
  CHECK(zones[0] && zones[1] && zones[2] && zones[3] && zones[4], "the endless road: every scenery");
  printf("  the endless road: 120 km laid, heights %d..%d m\n", ylo / FX, yhi / FX);
}

/* a whole race driven by the bot; returns the steps it took (0: not finished in 11 minutes) */
static int drive_race(pg_race *r, const pg_setup *s, uint32_t seed, int skill, uint32_t *hash) {
  pg_new(r, &TT, s, seed);
  bot_skill = skill, bot_line = 0;
  int steps = 0;
  while (r->phase != PH_DONE && steps < 11 * 60 * PG_HZ) {
    pg_step(r, &TT, bot(r, &TT), 0);
    steps++;
    if (hash) *hash = hash_race(r, *hash);
    if (!pg_valid(r, &TT)) {
      CHECK(0, "race not valid at step %d on %s", steps, pg_circuit_name[s->track]);
      break;
    }
  }
  return r->end == END_FINISH ? steps : 0;
}

static void laps(void) {
  pg_race r;
  pg_setup s = plain(MODE_RACE, 0, 0, 0, LEVEL_NORMAL);
  pg_new(&r, &TT, &s, 5);
  CHECK(r.phase == PH_GRID && r.lap == 1 && r.car[0].s < 0, "the start: on the grid, behind the line, lap 1");
  int steps = 0, laps_seen = 1;
  while (r.phase == PH_GRID) pg_step(&r, &TT, IN_GAS, 0), steps++;
  CHECK(steps == 3 * PG_HZ + PG_HZ / 2 && r.car[0].v == 0, "the countdown took %d steps, the car held", steps);
  bot_line = 0;
  while (r.phase == PH_RACE && steps < 20000) {
    int before = r.lap;
    int32_t s0 = r.car[0].s;
    pg_step(&r, &TT, bot(&r, &TT), 0);
    steps++;
    if (r.lap != before) {
      CHECK(r.lap == before + 1, "lap %d to %d", before, r.lap);
      CHECK(s0 < before * TT.length && r.car[0].s >= before * TT.length, "the lap changed off the line");
      CHECK(r.banner == (r.lap == 3 ? BANNER_FINAL : BANNER_LAP), "the lap's banner");
      CHECK(r.last_lap_ms > 15000 && r.lap_at == r.time_ms, "the lap's time (%u)", r.last_lap_ms);
      laps_seen++;
    }
  }
  CHECK(laps_seen == 3 && r.end == END_FINISH && r.car[0].place == 1, "three laps, then the finish (%d laps, end %d)",
        laps_seen, r.end);
  CHECK(r.car[0].s >= 3 * TT.length && r.car[0].finish_ms == r.time_ms, "finished at the line");
  CHECK(r.best_lap_ms > 15000 && r.best_lap_ms * 3 <= r.time_ms + 6000, "a best lap (%u ms of %u)", r.best_lap_ms, r.time_ms);
  /* standing still, or going backwards over the line, counts no lap */
  pg_new(&r, &TT, &s, 5);
  for (int i = 0; i < 2000; i++) pg_step(&r, &TT, IN_BRAKE, 0);
  CHECK(r.lap == 1 && r.car[0].v == 0, "standing still: still lap 1");
  pg_new(&r, &TT, &s, 5);
  r.phase = PH_RACE, r.countdown = 0;
  r.car[0].s = 20 * FX, r.car[0].ang = 32767, r.car[0].v = 20 * FX;   /* pointing back down the road */
  int wrong = 0;
  for (int i = 0; i < 1500; i++) {
    pg_step(&r, &TT, IN_GAS, 0);
    if (r.banner == BANNER_WRONG) wrong = 1;
  }
  CHECK(r.lap == 1 && r.car[0].s < -100 * FX && wrong, "backwards: lap %d, at %d m, told %d", r.lap, r.car[0].s / FX, wrong);
}

static void physics(void) {
  pg_race r;
  /* flat out on Neo-Tokyo Wangan's long straight: the car's top speed, no more; nitro adds to it */
  for (int v = 0; v < PG_VEHICLES; v++) {
    pg_setup s = plain(MODE_RACE, 1, v, 0, LEVEL_NORMAL);
    pg_new(&r, &TT, &s, 3);
    r.phase = PH_RACE, r.countdown = 0;
    int32_t top = (int32_t)pg_vehicles[v].top * FX / 8, most = 0;
    int t100 = 0;
    for (int i = 0; i < 40 * PG_HZ; i++) {
      r.car[0].s = 10 * FX, r.car[0].x = 0, r.car[0].ang = 0;   /* (kept on the straight) */
      pg_step(&r, &TT, IN_GAS, 0);
      if (r.car[0].v > most) most = r.car[0].v;
      if (!t100 && pg_kmh(r.car[0].v) >= 100) t100 = i;
    }
    CHECK(most <= top + FX / 4 && most > top - 3 * FX, "%s: top %d km/h, its own %d", pg_vehicles[v].name, pg_kmh(most), pg_kmh(top));
    r.car[0].nitro = 1000;
    for (int i = 0; i < 3 * PG_HZ; i++) {
      r.car[0].s = 10 * FX, r.car[0].x = 0, r.car[0].ang = 0;
      pg_step(&r, &TT, IN_GAS | IN_NITRO, 0);
    }
    CHECK(r.car[0].v > top + 5 * FX, "%s: nitro adds speed", pg_vehicles[v].name);
    printf("  %-15s top %d km/h, 0-100 km/h in %.1f s\n", pg_vehicles[v].name, pg_kmh(most), t100 / 50.0);
  }
  /* the manual gearbox: first gear stops at its limiter, + takes the next */
  {
    pg_setup m = plain(MODE_RACE, 1, 0, 0, LEVEL_NORMAL);
    m.manual = 1;
    pg_new(&r, &TT, &m, 3);
    r.phase = PH_RACE, r.countdown = 0;
    for (int i = 0; i < 10 * PG_HZ; i++) {
      r.car[0].s = 10 * FX, r.car[0].x = 0, r.car[0].ang = 0;
      pg_step(&r, &TT, IN_GAS, 0);
    }
    int32_t first = r.car[0].v;
    CHECK(r.car[0].gear == 1 && pg_kmh(first) < 90, "manual: still in first (%d) at %d km/h", r.car[0].gear, pg_kmh(first));
    pg_step(&r, &TT, IN_GAS, 1);
    for (int i = 0; i < 3 * PG_HZ; i++) {
      r.car[0].s = 10 * FX, r.car[0].x = 0, r.car[0].ang = 0;
      pg_step(&r, &TT, IN_GAS, 0);
    }
    CHECK(r.car[0].gear == 2 && r.car[0].v > first + 5 * FX, "manual: second gear goes on (%d km/h)", pg_kmh(r.car[0].v));
    pg_step(&r, &TT, IN_GAS, -1), pg_step(&r, &TT, IN_GAS, -1);
    CHECK(r.car[0].gear == 1, "manual: - goes back down, not below first");
  }
  /* braking from 200 km/h: under 60 m */
  pg_setup s = plain(MODE_RACE, 1, 0, 0, LEVEL_NORMAL);
  pg_new(&r, &TT, &s, 3);
  r.phase = PH_RACE, r.countdown = 0, r.car[0].s = 10 * FX, r.car[0].v = 200 * FX * 10 / 36;
  int32_t s0 = r.car[0].s;
  while (r.car[0].v > 0) pg_step(&r, &TT, IN_BRAKE, 0);
  CHECK(r.car[0].s - s0 < 60 * FX && r.car[0].s - s0 > 30 * FX, "braking from 200 km/h: %d m", (r.car[0].s - s0) / FX);
  /* a bend not steered through carries you to its outside; steering keeps you on the road */
  pg_build(&TT, 2);
  int32_t kb = 0;
  for (int k = 0; k < TT.n; k++)
    if (TT.cv[k] > TT.cv[kb]) kb = k;
  s = plain(MODE_RACE, 2, 0, 0, LEVEL_NORMAL);
  pg_new(&r, &TT, &s, 3);
  r.phase = PH_RACE, r.countdown = 0, r.car[0].s = (kb - 4) * TT.seg, r.car[0].v = 15 * FX;
  for (int i = 0; i < 60; i++) pg_step(&r, &TT, IN_GAS, 0);
  CHECK(r.car[0].x < -2 * FX, "a right bend, unsteered: out to the left (%d)", r.car[0].x / FX);
  /* the tyres hold so much: full lock at speed turns no faster than the grip allows */
  pg_build(&TT, 1);
  s = plain(MODE_RACE, 1, 0, 0, LEVEL_NORMAL);
  pg_new(&r, &TT, &s, 3);
  r.phase = PH_RACE, r.countdown = 0, r.car[0].s = 10 * FX, r.car[0].v = 60 * FX;
  int32_t worst = 0;
  for (int i = 0; i < 30; i++) {
    int16_t a0 = r.car[0].ang;
    int32_t cv = TT.cv[pg_slot(&TT, pg_seg_of(&TT, r.car[0].s))];
    int32_t ds0 = r.car[0].s;
    pg_step(&r, &TT, IN_RIGHT, 0);
    int32_t turned = (int16_t)(r.car[0].ang - a0) + (int32_t)((int64_t)cv * (r.car[0].s - ds0) / TT.seg);
    int32_t lat = (int32_t)((int64_t)turned * PG_HZ * r.car[0].v / 10430);   /* v * turn rate: FX/s^2 */
    if (lat > worst) worst = lat;
  }
  int32_t hold = 24 * FX * pg_vehicles[0].grip / 128 * 133 / 128;
  CHECK(worst <= hold + FX / 2 && worst > hold * 3 / 4, "sideways at full lock: %d m/s^2, the tyres %d", worst / FX, hold / FX);
  /* braking hard while steering hard slides the car: a tighter turn, less braking */
  pg_new(&r, &TT, &s, 3);
  r.phase = PH_RACE, r.countdown = 0, r.car[0].s = 10 * FX, r.car[0].v = 40 * FX, r.car[0].steer = 1024;
  pg_race q = r;
  for (int i = 0; i < 25; i++) pg_step(&r, &TT, IN_BRAKE | IN_RIGHT, 0), pg_step(&q, &TT, IN_RIGHT, 0);
  CHECK(r.car[0].drift && r.car[0].slip > 1000 && r.car[0].ang > q.car[0].ang, "a slide: %d, body %d, heading %d vs %d",
        r.car[0].drift, r.car[0].slip, r.car[0].ang, q.car[0].ang);
  /* the grass is slow */
  pg_new(&r, &TT, &s, 3);
  r.phase = PH_RACE, r.countdown = 0;
  for (int i = 0; i < 30 * PG_HZ; i++) {
    r.car[0].s = 10 * FX, r.car[0].x = 11 * FX, r.car[0].ang = 0;
    pg_step(&r, &TT, IN_GAS, 0);
  }
  CHECK(pg_kmh(r.car[0].v) < 160, "on the grass: %d km/h", pg_kmh(r.car[0].v));
  /* the barriers: never through, and hitting one costs speed */
  pg_new(&r, &TT, &s, 3);
  r.phase = PH_RACE, r.countdown = 0, r.car[0].s = 10 * FX, r.car[0].v = 50 * FX, r.car[0].ang = 6000;
  int32_t far_x = 0, v0 = r.car[0].v;
  for (int i = 0; i < 100; i++) {
    pg_step(&r, &TT, IN_GAS, 0);
    if (r.car[0].x > far_x) far_x = r.car[0].x;
  }
  CHECK(far_x <= PG_WALL - PG_HW && r.car[0].v < v0, "into the barrier: %d cm in, %d km/h", (far_x - (PG_WALL - PG_HW)) * 100 / FX,
        pg_kmh(r.car[0].v));
}

static int inside_count(const pg_race *r, const pg_track *t) {
  int n = 0;
  for (int a = 0; a <= r->rivals; a++)
    for (int b = a + 1; b <= r->rivals; b++) {
      int32_t ds = pg_wrap(t, r->car[b].s - r->car[a].s), dx = r->car[b].x - r->car[a].x;
      if (abs(ds) < 2 * PG_HL - FX / 4 && abs(dx) < 2 * PG_HW - FX / 4) n++;
    }
  return n;
}

static void crashes(void) {
  pg_race r;
  /* random keys among a full field: never a car inside another, everything in range */
  uint32_t lcg = 77;
  int inside = 0, bad = 0;
  for (int id = 0; id < PG_CIRCUITS; id++) {
    pg_setup s = plain(MODE_RACE, id, id, 7, LEVEL_HARD);
    pg_new(&r, &TT, &s, (uint32_t)id + 1);
    for (int i = 0; i < 9000 && r.phase != PH_DONE; i++) {
      lcg = lcg * 1103515245u + 12345u;
      unsigned in = (lcg >> 16) & 31;
      pg_step(&r, &TT, in | IN_GAS, 0);
      inside += inside_count(&r, &TT);
      if (!pg_valid(&r, &TT)) bad++;
    }
  }
  CHECK(!inside && !bad, "cars inside each other %d times, %d bad steps", inside, bad);
  /* ramming the car in front: you stop at its bumper, it takes the push */
  pg_setup s = plain(MODE_RACE, 1, 0, 1, LEVEL_EASY);
  pg_new(&r, &TT, &s, 3);
  r.phase = PH_RACE, r.countdown = 0;
  r.car[0].s = 100 * FX, r.car[0].x = 0, r.car[0].v = 60 * FX;
  r.car[1].s = 112 * FX, r.car[1].x = 0, r.car[1].lane = 0, r.car[1].v = 20 * FX;
  bool through = false;
  int32_t v1 = r.car[1].v;
  for (int i = 0; i < 30; i++) {
    r.car[1].lane = 0, r.car[1].think = 2;
    pg_step(&r, &TT, IN_GAS, 0);
    if (r.car[0].s > r.car[1].s && abs(r.car[0].x - r.car[1].x) < 2 * PG_HW) through = true;
  }
  CHECK(!through && r.car[1].v > v1, "drove through the car in front (%d), it was pushed (%d)", through, r.car[1].v > v1);
}

static void rivals(void) {
  uint32_t h = 0;
  for (int id = 0; id < PG_CIRCUITS; id++) {
    uint32_t mean[3] = {0};
    for (int level = 0; level < 3; level++) {
      pg_race r;
      pg_setup s = plain(MODE_RACE, id, 0, 7, level);
      pg_new(&r, &TT, &s, (uint32_t)(id * 3 + level + 1));
      /* you stay on the grid: the rivals race on their own */
      int steps = 0, inside = 0;
      while (r.finished < 7 && steps < 11 * 60 * PG_HZ) {
        pg_step(&r, &TT, 0, 0), steps++;
        h = hash_race(&r, h);
        inside += inside_count(&r, &TT);
      }
      CHECK(r.finished == 7 && !inside, "%s, %s: %d of 7 rivals finished, %d overlaps", pg_circuit_name[id], LEVELS[level],
            r.finished, inside);
      uint32_t first = 0, last = 0;
      for (int i = 1; i < PG_CARS; i++) {
        mean[level] += r.car[i].finish_ms / 7;
        if (r.car[i].place == 1) first = r.car[i].finish_ms;
        if (r.car[i].place == 7) last = r.car[i].finish_ms;
      }
      printf("  %-17s %-6s rivals %u:%04.1f to %u:%04.1f\n", pg_circuit_name[id], LEVELS[level], first / 60000,
             first % 60000 / 1000.0, last / 60000, last % 60000 / 1000.0);
      CHECK(last - first < 30000, "%s %s: the field spread over %u ms", pg_circuit_name[id], LEVELS[level], last - first);
    }
    CHECK(mean[0] > mean[1] && mean[1] > mean[2], "%s: Easy %u, Normal %u, Hard %u ms", pg_circuit_name[id], mean[0], mean[1], mean[2]);
  }
  printf("  rivals' hash %08x\n", h);
}

/* the driver against the field, every circuit and level */
static void racing(void) {
  int wins[3] = {0}, finishes = 0, places[3] = {0};
  for (int level = 0; level < 3; level++)
    for (int id = 0; id < PG_CIRCUITS; id++) {
      pg_race r;
      pg_setup s = plain(MODE_RACE, id, 0, 7, level);
      int steps = drive_race(&r, &s, (uint32_t)(100 + id + level * 7), 100, NULL);
      CHECK(steps > 0, "%s %s: the driver didn't finish", pg_circuit_name[id], LEVELS[level]);
      if (steps) finishes++;
      if (r.car[0].place == 1) wins[level]++;
      places[level] += r.car[0].place;
      printf("  %-17s %-6s %u:%04.1f, %d%s, best lap %u:%04.1f\n", pg_circuit_name[id], LEVELS[level], r.time_ms / 60000,
             r.time_ms % 60000 / 1000.0, r.car[0].place, r.car[0].place == 1 ? "st" : r.car[0].place == 2 ? "nd" : r.car[0].place == 3 ? "rd" : "th",
             r.best_lap_ms / 60000, r.best_lap_ms % 60000 / 1000.0);
    }
  printf("  the driver won %d easy, %d normal, %d hard races of 3 each\n", wins[0], wins[1], wins[2]);
  CHECK(finishes == 9, "%d of 9 races finished", finishes);
  CHECK(places[0] <= places[1] && places[1] <= places[2], "harder levels should be harder (%d %d %d)", places[0], places[1], places[2]);
}

/* the endless road: zones add time, the clock runs out, orbs fill nitro */
static void endless(void) {
  pg_race r;
  pg_setup s = plain(MODE_ENDLESS, 0, 0, 0, LEVEL_NORMAL);
  for (int run = 0; run < 3; run++) {
    pg_new(&r, &TT, &s, 400 + (uint32_t)run);
    CHECK(r.rivals == 0 && r.time_left == 30000, "the endless road: alone, 30 s");
    bot_skill = 100 - run * 20, bot_line = 0;
    int steps = 0, orbs = 0, bad = 0;
    uint16_t n0 = r.car[0].nitro;
    while (r.phase != PH_DONE && steps < 30 * 60 * PG_HZ) {
      pg_step(&r, &TT, bot(&r, &TT), 0), steps++;
      if (r.car[0].nitro > n0) orbs++;
      n0 = r.car[0].nitro;
      if (!pg_valid(&r, &TT)) bad++;
    }
    CHECK(r.phase == PH_DONE && r.end == END_TIME && !bad, "the endless road ends when time runs out (%d bad)", bad);
    CHECK(r.zone >= 4 && orbs > 3, "the endless road: zone %d, %d orbs", r.zone + 1, orbs);
    printf("  endless, skill %d: %.1f km, zone %d, %.0f s, %d orbs\n", bot_skill, r.car[0].s / 256000.0, r.zone + 1,
           r.ticks / 50.0, orbs);
  }
}

/* the same race twice, to the same hash on every computer */
static void same_twice(void) {
  uint32_t h1 = 0, h2 = 0;
  pg_race a, b;
  pg_setup s = plain(MODE_RACE, 2, 1, 7, LEVEL_NORMAL);
  drive_race(&a, &s, 2026, 100, &h1);
  drive_race(&b, &s, 2026, 100, &h2);
  printf("  a race's hash %08X\n", h1);
  CHECK(h1 == h2 && h1 == RACE_HASH, "the same race twice, and as on every computer: %08X", h1);
}

/* ---------------------------------------------------------------- saves */
static void saves(void) {
  memset(files, 0, sizeof files);
  pg_file a, b;
  memset(&a, 0, sizeof a);
  pg_fresh(&a.p);
  a.p.races = 9, a.p.wins = 4, a.p.runs = 3, a.p.best_ms[1] = 160000, a.p.best_lap_ms[1] = 51000, a.p.best_lap_ms[2] = 47000;
  a.p.best_m = 12345, a.p.best_zone = 21, a.p.mode = MODE_RACE, a.p.track = 2, a.p.vehicle = 4, a.p.laps = 5;
  a.p.rivals = 6, a.p.level = LEVEL_HARD, a.p.manual = 1, a.p.view = VIEW_FAR, a.p.guide = 1;
  pg_setup s = plain(MODE_RACE, 2, 4, 6, LEVEL_HARD);
  pg_new(&a.race, &TT, &s, 4);
  bot_line = 0;
  for (int i = 0; i < 2500; i++) pg_step(&a.race, &TT, bot(&a.race, &TT), 0);
  a.has_race = 1;
  CHECK(save_write(&a), "saved");
  CHECK(save_load(&b) && !memcmp(&a.p, &b.p, sizeof a.p) && b.has_race && !memcmp(&a.race, &b.race, sizeof a.race),
        "read back as written");
  /* the copy in the script brings it back when the save is gone */
  forget(PG_SAVE_NAME);
  CHECK(save_load(&b) && b.p.races == 9 && b.has_race, "back from polygt3d_saves.py");
  CHECK(plat_load(PG_SAVE_NAME, &(uint32_t){0}) != NULL, "and put back in polygt3d.sav");
  /* every byte damaged in turn: never read (with the copy gone, a fresh start) */
  uint32_t n;
  uint8_t good[sizeof(pg_file)];
  memcpy(good, file_data(PG_SAVE_NAME, &n), sizeof good);
  forget(PG_COPY_NAME);
  int bad = 0;
  for (uint32_t i = 0; i < sizeof good; i++) {
    uint8_t *d = file_data(PG_SAVE_NAME, &n);
    memcpy(d, good, sizeof good);
    d[i] ^= (uint8_t)(1 + i % 255);
    if (save_load(&b) || b.p.races || b.has_race || b.p.laps != 3) bad++;
  }
  CHECK(!bad, "%d damaged saves read", bad);
  /* cut short, empty, another version: a fresh start */
  for (int k = 0; k < 4; k++)
    if (files[k].on && !strcmp(files[k].name, PG_SAVE_NAME)) memcpy(files[k].data, good, sizeof good), files[k].len = sizeof good - 1;
  CHECK(!save_load(&b) && b.p.rivals == 5, "a short save: fresh");
  for (int k = 0; k < 4; k++)
    if (files[k].on && !strcmp(files[k].name, PG_SAVE_NAME)) files[k].len = 0;
  CHECK(!save_load(&b), "an empty save: fresh");
  pg_file v = a;
  v.magic = PG_MAGIC, v.size = sizeof v, v.version = 2;
  v.sum = pg_sum(&v, offsetof(pg_file, sum));
  plat_save(PG_SAVE_NAME, &v, sizeof v);
  CHECK(!save_load(&b), "another version: fresh");
  /* a checksum that fits over values that don't: not read either; whatever is read races and draws safely */
  uint32_t lcg = 5;
  int read = 0;
  for (int k = 0; k < 400; k++) {
    pg_file x;
    memcpy(&x, good, sizeof x);
    uint8_t *p = (uint8_t *)&x;
    for (int j = 0; j < 3; j++) {
      lcg = lcg * 1103515245u + 12345u;
      uint32_t at = 8 + (lcg >> 8) % (offsetof(pg_file, sum) - 8);
      p[at] = (uint8_t)(lcg >> 24);
    }
    x.sum = pg_sum(&x, offsetof(pg_file, sum));
    plat_save(PG_SAVE_NAME, &x, sizeof x);
    if (save_load(&b)) {
      read++;
      if (b.has_race) {
        show_race(&b.race), built = -1;
        int went_bad = 0;
        for (int i = 0; i < 300; i++) {
          pg_step(&b.race, &track, (unsigned)i & 31, 0);
          if (!pg_valid(&b.race, NULL)) went_bad++;
        }
        CHECK(!went_bad, "an accepted race went bad");
      }
    }
  }
  printf("  saves: %d of 400 changed saves with a fitting checksum were still in range\n", read);
  /* odd but possible races (cars on top of each other, flat out, stopped, wide): read, then raced and drawn */
  int odd = 0, odd_bad = 0;
  for (int k = 0; k < 200; k++) {
    pg_file x;
    memcpy(&x, good, sizeof x);
    for (int i = 0; i <= x.race.rivals; i++) {
      pg_car *c = &x.race.car[i];
      lcg = lcg * 1103515245u + 12345u;
      c->s = (int32_t)(lcg >> 9) % (3 * 1600 * FX);
      lcg = lcg * 1103515245u + 12345u;
      c->x = ((int32_t)(lcg >> 8) % (2 * (PG_WALL - PG_HW))) - (PG_WALL - PG_HW);
      c->lane = ((int32_t)(lcg >> 20) % 3 - 1) * 4 * FX;
      lcg = lcg * 1103515245u + 12345u;
      c->v = (int32_t)(lcg >> 8) % (100 * FX);
      c->ang = (int16_t)(lcg >> 3);
    }
    x.race.car[1].s = x.race.car[2].s, x.race.car[1].x = x.race.car[2].x;   /* two cars in one place */
    x.sum = pg_sum(&x, offsetof(pg_file, sum));
    plat_save(PG_SAVE_NAME, &x, sizeof x);
    if (!save_load(&b) || !b.has_race) continue;
    odd++;
    sv = b, built = -1;
    show_race(&sv.race);
    for (int i = 0; i < 400; i++) {
      pg_step(&sv.race, &track, bot(&sv.race, &track), 0);
      if (!pg_valid(&sv.race, &track)) odd_bad++;
    }
    prev = sv.race, ui.screen = V_RACE;
    pg_render(&sv, &track, &prev, &sv.race, 256, &ui, &ghost_now);
  }
  CHECK(odd > 150 && !odd_bad, "odd races: %d read, %d steps gone bad", odd, odd_bad);
  /* storage full: the save fails, the game goes on */
  memset(files, 0, sizeof files);
  storage_full = true;
  CHECK(!save_write(&a), "a full storage fails the save");
  storage_full = false;
  memset(files, 0, sizeof files);
}

/* ---------------------------------------------------------------- the whole app, with keys */
static int bad_frames;
static void check_state(void) {
  if (ui.screen >= V_SCREENS) bad_frames++;
  if (sv.has_race && !pg_valid(&sv.race, NULL)) bad_frames++;
  if (sv.p.wins > sv.p.races || sv.p.laps < 1 || sv.p.laps > PG_MAX_LAPS || sv.p.vehicle >= PG_VEHICLES) bad_frames++;
  if (rd_dropped) bad_frames++;
}

static uint32_t (*menu_keys)(uint32_t f);
static uint32_t drive_keys(uint32_t f) {
  if (ui.screen == V_RACE && !ui.hold && sv.race.phase != PH_DONE) {
    unsigned in = bot(&sv.race, &track);
    uint32_t k = 0;
    if (in & IN_GAS) k |= PK_UP;
    if (in & IN_BRAKE) k |= PK_DOWN;
    if (in & IN_LEFT) k |= PK_LEFT;
    if (in & IN_RIGHT) k |= PK_RIGHT;
    if (in & IN_NITRO) k |= PK_OK;
    return k;
  }
  return menu_keys ? menu_keys(f) : 0;
}

/* a race: Play, Start race, the guide, three laps driven, then the results' Main menu */
static int race_phase;
static uint32_t race_menu(uint32_t f) {
  if (f % 4) return 0;
  switch (ui.screen) {
    case V_TITLE:
      if (race_phase) return 0;
      return ui.menu[ui.sel] == MENU_PLAY ? PK_OK : PK_DOWN;
    case V_SETUP: return ui.sel == SET_START ? PK_OK : PK_DOWN;
    case V_GUIDE: return PK_OK;
    case V_RESULT: race_phase = 1; return ui.sel == 1 ? PK_OK : PK_DOWN;
    default: return 0;
  }
}
static bool saw_ghost;
static void watch_race(void) {
  check_state();
  if (ghost_now.on) saw_ghost = true;
  if (race_phase == 1 && ui.screen == V_TITLE) quit_at = frame_no + 2, race_phase = 2;
}
static void race_app(void) {
  memset(files, 0, sizeof files);
  memset(best_ghost, 0, sizeof best_ghost);
  frame_gap = 60;
  race_phase = 0, saw_ghost = false;
  script = drive_keys, menu_keys = race_menu, watch = watch_race;
  frame_no = 0, quit_at = 4000000, bad_frames = 0;
  bot_line = 0;
  pg_app_main();
  script = 0, menu_keys = 0, watch = 0;
  pg_file f;
  CHECK(save_load(&f), "the race saved");
  CHECK(f.p.races == 1 && !f.has_race && f.p.guide == 1 && f.p.best_ms[0] && f.p.best_lap_ms[0], "a race finished and its records kept (races %u)",
        f.p.races);
  CHECK(race_phase == 2 && saw_ghost, "back to the menu (%d), the ghost seen (%d)", race_phase, saw_ghost);
  CHECK(!bad_frames, "%d bad frames", bad_frames);
  printf("  a race through the keys: %u:%04.1f, %s, %u frames\n", f.p.best_ms[0] / 60000, f.p.best_ms[0] % 60000 / 1000.0,
         f.p.wins ? "won" : "not won", frame_no);
}

/* the endless road: chosen on the race screen, a minute of driving, pause, Quit game to the menu (kept), Continue,
   then Home: it's saved */
static int en_step;
static uint32_t en_menu(uint32_t f) {
  if (f % 4) return 0;
  switch (ui.screen) {
    case V_TITLE:
      if (en_step >= 2) return ui.menu[ui.sel] == MENU_CONTINUE ? PK_OK : PK_UP;
      return ui.menu[ui.sel] == MENU_PLAY ? PK_OK : PK_DOWN;
    case V_SETUP:
      if (sv.p.mode != MODE_ENDLESS) return ui.sel == SET_MODE ? PK_RIGHT : PK_UP;
      return ui.sel == SET_START ? PK_OK : PK_UP;
    case V_GUIDE: return PK_OK;
    case V_PAUSE: return ui.sel == PAUSE_QUIT ? PK_OK : PK_DOWN;
    case V_LEAVE: return ui.sel == 1 ? PK_OK : PK_DOWN;
    default: return 0;
  }
}
static uint32_t en_keys(uint32_t f) {
  if (ui.screen == V_RACE && en_step == 0 && sv.race.car[0].s > 1500 * FX) en_step = 1;
  if (ui.screen == V_RACE && en_step == 1) return f % 8 < 4 ? PK_BACK : 0;
  if (ui.screen == V_TITLE && en_step == 1) en_step = 2;
  if (ui.screen == V_RACE && en_step == 2 && !ui.hold) en_step = 3, quit_at = frame_no + 600;
  return drive_keys(f);
}
static void endless_app(void) {
  memset(files, 0, sizeof files);
  frame_gap = 60;
  en_step = 0;
  script = en_keys, menu_keys = en_menu, watch = check_state;
  frame_no = 0, quit_at = 400000, bad_frames = 0;
  bot_line = 0;
  pg_app_main();
  script = 0, menu_keys = 0, watch = 0;
  pg_file f;
  CHECK(en_step == 3, "the endless steps (%d)", en_step);
  CHECK(save_load(&f) && f.has_race && f.race.mode == MODE_ENDLESS && f.race.car[0].s > 1500 * FX,
        "the drive kept on Home (%d)", f.has_race);
  CHECK(!bad_frames, "%d bad frames", bad_frames);
  printf("  the endless road through the keys: %.1f km, kept on Home\n", f.race.car[0].s / 256000.0);
}

static uint32_t lcg_keys;
static uint32_t rnd(void) { return lcg_keys = lcg_keys * 1103515245u + 12345u, lcg_keys >> 8; }
static uint32_t held_keys, held_until;
static uint32_t random_keys(uint32_t f) {
  if (f >= held_until) {
    static const uint32_t pick[] = {PK_OK, PK_OK, PK_OK, PK_LEFT, PK_RIGHT, PK_UP, PK_UP, PK_UP, PK_DOWN, PK_BACK,
                                    PK_GEAR_UP, PK_GEAR_DOWN, PK_UP | PK_OK, PK_UP | PK_LEFT, PK_DOWN | PK_RIGHT, 0, 0};
    held_keys = pick[rnd() % (sizeof pick / sizeof pick[0])];
    held_until = f + 2 + rnd() % 40;
  }
  return held_keys;
}

static void fuzz(void) {
  memset(files, 0, sizeof files);
  frame_gap = 90;
  int sessions = 0, kept = 0;
  for (int s = 0; s < 300; s++) {
    lcg_keys = (uint32_t)s * 2654435761u + 1;
    if (s % 40 == 0) memset(files, 0, sizeof files);
    storage_full = s % 37 == 36;
    script = random_keys, watch = check_state;
    frame_no = 0, quit_at = 2000 + rnd() % 6000, held_until = 0, bad_frames = 0;
    pg_app_main();
    sessions++;
    storage_full = false;
    pg_file f;
    if (save_load(&f)) kept += f.has_race;
    CHECK(!bad_frames, "session %d: %d bad frames", s, bad_frames);
  }
  script = 0, watch = 0;
  printf("  keys: %d sessions of random keys, quit with Home each time (%d left a race going)\n", sessions, kept);
}

static void test(const char *dir) {
  (void)dir;
  printf("the roads\n");
  roads();
  printf("laps\n");
  laps();
  printf("physics\n");
  physics();
  printf("crashes\n");
  crashes();
  printf("rivals\n");
  rivals();
  printf("racing\n");
  racing();
  printf("the endless road\n");
  endless();
  printf("the same race\n");
  same_twice();
  printf("saves\n");
  saves();
  printf("the app\n");
  race_app();
  endless_app();
  fuzz();
  printf(fails ? "%d FAILED\n" : "all good\n", fails);
}

/* ---------------------------------------------------------------- pictures */
static void picture(const char *dir, const char *name) {
  pg_render(&sv, &track, &prev, &sv.race, 256, &ui, &ghost_now);
  shot(dir, name);
}

static void race_at(int id, int vehicle, int rivals, float metres, int view) {
  memset(&sv, 0, sizeof sv);
  pg_fresh(&sv.p);
  sv.p.track = (uint8_t)id, sv.p.vehicle = (uint8_t)vehicle, sv.p.rivals = (uint8_t)rivals, sv.p.view = (uint8_t)view;
  sv.p.guide = 1;
  race_begin();
  bot_line = 0;
  while (sv.race.car[0].s < (int32_t)(metres * FX) && sv.race.phase != PH_DONE) {
    prev = sv.race;
    pg_step(&sv.race, &track, bot(&sv.race, &track), 0);
  }
  prev = sv.race;
  ui.screen = V_RACE;
}

static void endless_at(float metres, uint32_t seed) {
  memset(&sv, 0, sizeof sv);
  pg_fresh(&sv.p);
  sv.p.mode = MODE_ENDLESS, sv.p.guide = 1;
  rnd_state = seed;
  race_begin();
  bot_line = 0;
  while (sv.race.car[0].s < (int32_t)(metres * FX) && sv.race.phase != PH_DONE) {
    prev = sv.race;
    pg_step(&sv.race, &track, bot(&sv.race, &track), 0);
  }
  prev = sv.race;
  ui.screen = V_RACE;
}

static void shots(const char *dir) {
  memset(&sv, 0, sizeof sv);
  pg_fresh(&sv.p);
  now = 1000;
  to_title();
  ui.clock = 2600;
  picture(dir, "title");
  ui.screen = V_SETUP, ui.sel = SET_START;
  show_circuit(0);
  picture(dir, "setup");
  ui.sel = SET_CAR;
  picture(dir, "setup_car");
  ui.screen = V_RECORDS;
  picture(dir, "records");
  for (int k = 0; k < PG_HELP_PAGES; k++) {
    ui.screen = V_HELP, ui.page = (uint8_t)k;
    char n[16];
    snprintf(n, sizeof n, "help%d", k);
    picture(dir, n);
  }
  race_at(0, 0, 7, -1, VIEW_CHASE);
  picture(dir, "grid");
  static const float AT[3] = {420, 600, 500};
  for (int id = 0; id < PG_CIRCUITS; id++) {
    race_at(id, id, 7, AT[id], VIEW_CHASE);
    char n[16];
    snprintf(n, sizeof n, "track%d", id);
    picture(dir, n);
  }
  race_at(2, 3, 5, 700, VIEW_CHASE);
  picture(dir, "horse");
  race_at(1, 1, 5, 900, VIEW_FAR);
  picture(dir, "far");
  race_at(0, 2, 5, 300, VIEW_BUMPER);
  picture(dir, "bumper");
  ui.screen = V_PAUSE, ui.sel = 0;
  picture(dir, "pause");
  ui.screen = V_LEAVE, ui.sel = 1;
  picture(dir, "leave");
  for (int z = 0; z < 5; z++) {
    endless_at(400 + 430 * z, 7 + z);
    char n[16];
    snprintf(n, sizeof n, "endless%d", z);
    picture(dir, n);
  }
  /* a race to the end, then its results */
  race_at(0, 1, 5, 99999, VIEW_CHASE);
  while (!race_over(&sv.race)) prev = sv.race, pg_step(&sv.race, &track, 0, 0);
  prev = sv.race;
  finish_race();
  picture(dir, "result");
  /* the endless road, out of time, then its results */
  endless_at(1e6f, 3);
  while (!race_over(&sv.race)) prev = sv.race, pg_step(&sv.race, &track, 0, 0);
  ui.screen = V_RACE;
  prev = sv.race;
  picture(dir, "timeup");
  finish_race();
  picture(dir, "result_endless");
  /* the horses, close */
  for (int v = 3; v < 5; v++) {
    memset(&sv, 0, sizeof sv);
    pg_fresh(&sv.p);
    sv.p.vehicle = (uint8_t)v;
    to_title();
    ui.clock = 1800 + v * 900;
    char n[16];
    snprintf(n, sizeof n, "title_v%d", v);
    picture(dir, n);
  }
  race_at(1, 4, 3, 300, VIEW_CHASE);
  picture(dir, "mustang");
  /* the title with a race kept, the questions */
  sv.has_race = 1;
  to_title();
  ui.clock = 900;
  picture(dir, "title_continue");
  ui.screen = V_QUIT, ui.sel = 0;
  picture(dir, "quit");
  ui.screen = V_NEWRUN, ui.sel = 0;
  picture(dir, "newrun");
  ui.screen = V_GUIDE, ui.page = 0;
  picture(dir, "guide");
  sv.p.best_ms[0] = 101230, sv.p.best_lap_ms[0] = 32870, sv.p.best_ms[2] = 152110, sv.p.best_lap_ms[2] = 49010;
  sv.p.best_m = 9420, sv.p.best_zone = 23, sv.p.races = 12, sv.p.wins = 5;
  ui.screen = V_RECORDS;
  picture(dir, "records_full");
  ui.screen = V_TITLE, ui.sel = 0, ui.save_failed = true;
  picture(dir, "title_full");
  ui.save_failed = false;
}

/* the README's pictures: a race on each circuit, taken when the most rivals are close ahead (the race is run once to
   find that moment, then again to it) */
static void doc_race(const char *dir, const char *name, int id, int vehicle, int view, int after_ms) {
  int best_t = -1, best_n = 0;
  for (int pass = 0; pass < 2; pass++) {
    memset(&sv, 0, sizeof sv);
    pg_fresh(&sv.p);
    sv.p.track = (uint8_t)id, sv.p.vehicle = (uint8_t)vehicle, sv.p.view = (uint8_t)view, sv.p.rivals = 7, sv.p.guide = 1;
    now = 0, rnd_state = 1;
    race_begin();
    ui.screen = V_RACE;
    bot_line = 0, bot_skill = 100;
    for (int t = 0; t < 3 * 60000 && sv.race.phase != PH_DONE; t += PG_TICK) {
      prev = sv.race;
      int lap = sv.race.lap;
      pg_step(&sv.race, &track, bot(&sv.race, &track), 0);
      ghost_after_step(lap);
      if (pass == 1) {
        if (t == best_t) break;
        continue;
      }
      if (t < after_ms || sv.race.banner_t || abs(sv.race.car[0].x) > PG_ROAD - FX || abs(sv.race.car[0].ang) > 1500) continue;
      int close = 0;
      for (int i = 1; i <= sv.race.rivals; i++) {
        int32_t ds = pg_wrap(&track, sv.race.car[i].s - sv.race.car[0].s);
        if (ds > 9 * FX && ds < 70 * FX) close += ds < 35 * FX ? 2 : 1;
      }
      if (close > best_n) best_n = close, best_t = t;
    }
  }
  prev = sv.race;
  ui.clock = sv.race.time_ms;
  ghost_pose(256);
  picture(dir, name);
}

static void docs(const char *dir) {
  doc_race(dir, "shot", 0, 0, VIEW_CHASE, 6000);
  doc_race(dir, "wangan", 1, 1, VIEW_CHASE, 6000);
  doc_race(dir, "akagi", 2, 2, VIEW_CHASE, 9000);
  endless_at(2700, 21);
  picture(dir, "endless");
  memset(&sv, 0, sizeof sv);
  pg_fresh(&sv.p);
  now = 0;
  to_title();
  ui.clock = 2300;
  picture(dir, "title");
  sv.p.vehicle = 4;
  ui.clock = 4300;
  picture(dir, "horse");
  sv.p.vehicle = 0;
  sv.p.best_ms[0] = 101230;
  ui.screen = V_SETUP, ui.sel = SET_START;
  show_circuit(0);
  picture(dir, "setup");
}

/* raw frames of a race, the driver at the wheel, every `step` ms (for tools/record.py --frames) */
static void film(const char *dir, int ms, int id, int view, int vehicle, int step) {
  memset(&sv, 0, sizeof sv);
  pg_fresh(&sv.p);
  sv.p.track = (uint8_t)id, sv.p.view = (uint8_t)view, sv.p.vehicle = (uint8_t)vehicle, sv.p.rivals = 7, sv.p.guide = 1;
  sv.p.level = LEVEL_NORMAL;
  now = 0;
  race_begin();
  ui.screen = V_RACE;
  bot_line = 0, bot_skill = 100;
  int frame = 0;
  for (int t = 0; t < ms; t += PG_TICK) {
    prev = sv.race;
    int lap = sv.race.lap;
    pg_step(&sv.race, &track, bot(&sv.race, &track), 0);
    ghost_after_step(lap);
    if (t % step) continue;
    ui.clock = (uint32_t)t;
    ghost_pose(256);
    pg_render(&sv, &track, &prev, &sv.race, 256, &ui, &ghost_now);
    char path[512];
    snprintf(path, sizeof path, "%s/%06d_%06d.raw", dir, frame++, t);
    FILE *f = fopen(path, "wb");
    if (!f) exit(2);
    fwrite(fb, 2, 320 * 240, f);
    fclose(f);
  }
}

/* the busiest frames of whole races: triangles, for every circuit and view */
static void busy(void) {
  for (int view = 0; view < VIEWS; view++)
    for (int id = 0; id < PG_CIRCUITS; id++) {
      memset(&sv, 0, sizeof sv);
      pg_fresh(&sv.p);
      sv.p.track = (uint8_t)id, sv.p.rivals = 7, sv.p.view = (uint8_t)view, sv.p.guide = 1;
      race_begin();
      bot_line = 0;
      int most = 0, at = 0, drops = 0;
      ui.screen = V_RACE;
      for (int i = 0; i < 6000 && sv.race.phase != PH_DONE; i++) {
        prev = sv.race;
        pg_step(&sv.race, &track, bot(&sv.race, &track), 0);
        if (i % 3) continue;
        pg_render(&sv, &track, &prev, &sv.race, 256, &ui, &ghost_now);
        if (rd_tris > most) most = rd_tris, at = i;
        drops += rd_dropped;
      }
      printf("view %d %-17s most %d triangles at step %d, %d frames out of room\n", view, pg_circuit_name[id], most, at, drops);
    }
}

int main(int argc, char **argv) {
  if (argc >= 3 && !strcmp(argv[1], "docs")) {
    docs(argv[2]);
    return 0;
  }
  if (argc >= 5 && !strcmp(argv[1], "film")) {
    film(argv[2], atoi(argv[3]), atoi(argv[4]), argc > 5 ? atoi(argv[5]) : 0, argc > 6 ? atoi(argv[6]) : 0,
         argc > 7 ? atoi(argv[7]) : 60);
    return 0;
  }
  if (argc >= 2 && !strcmp(argv[1], "busy")) {
    busy();
    return 0;
  }
  if (argc >= 3 && !strcmp(argv[1], "test")) {
    test(argv[2]);
    return fails ? 1 : 0;
  }
  if (argc >= 3 && !strcmp(argv[1], "shots")) {
    shots(argv[2]);
    return 0;
  }
  fprintf(stderr, "usage: play test DIR | shots DIR | docs DIR | film DIR MS CIRCUIT\n");
  return 2;
}
