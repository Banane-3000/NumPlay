/* Dune on a computer, for tests and pictures (make host, make test).
 *
 *   play test DIR       the rules against the original's numbers (the spec), a rider's runs (no broken state; the
 *                       line, both slides, coins, crashes, every landscape and the sticky dune met), saves written
 *                       and read back exactly, damaged saves refused, and a few hundred sessions of the whole app
 *                       with random keys, quitting with Home each time. DIR/cases.json gets the rider's steps for
 *                       tests/oracle.cjs, which plays them again with the original's own code.
 *   play shots DIR      pictures of the screens (DIR/<name>.ppm)
 *   play film DIR S A N N frames of a ride (seed S), after A steps, every other step (DIR/f_<ms>.raw)
 */
#include "../src/game.c"
#define main dn_app_main
#include "../src/main.c"
#undef main
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

/* ---------------------------------------------------------------- the calculator, on a computer */
static uint16_t fb[240][320];
void plat_push(int x, int y, int w, int h, const uint16_t *px) {
  for (int j = 0; j < h; j++)
    for (int i = 0; i < w; i++) fb[y + j][x + i] = px[j * w + i];
}
static double clock_ms;
static uint32_t (*script)(uint32_t frame);
static uint32_t frame_no, quit_at;
uint32_t plat_keys(void) {
  clock_ms += 10;   /* the app's loop, faster than the game's 60 steps a second: some loops have none */
  frame_no++;
  if (frame_no >= quit_at) return K_HOME;
  return script ? script(frame_no) : 0;
}
uint32_t plat_millis(void) { return (uint32_t)clock_ms; }
void plat_sleep(uint32_t ms) { clock_ms += ms; }
uint32_t plat_random(void) { return 0x9E3779B9u ^ frame_no; }
void plat_begin(void) {}
int plat_end(void) { return 0; }

static struct { char name[32]; uint8_t data[8192]; uint32_t len; bool on; } files[4];
bool plat_save(const char *name, const void *data, uint32_t len) {
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

static void shot(const char *path) {
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
#define CHECK(c, ...) do { if (!(c)) { fails++; printf("FAIL %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)
#define NEAR(a, b, e) (fabs((double)(a) - (double)(b)) <= (e))

/* ---------------------------------------------------------------- a rider: plans (hold n, let go m) tried ahead */
static int plan_h, plan_r, plan_left, plan_t;
static float judge(const dn_run *r0, int hold, int rel, int horizon) {
  static dn_run r;
  r = *r0;
  void (*keep)(const dn_run *, int) = dn_trace;
  dn_trace = 0;   /* (the rider's look ahead isn't recorded) */
  for (int f = 0; f < horizon; f++) {
    int cyc = hold + rel, p = cyc ? f % cyc : 0;
    dn_step(&r, p < hold, p == 0 && hold > 0);
    if (!r.alive) return dn_trace = keep, (float)f;   /* a crash: the sooner the worse */
  }
  dn_trace = keep;
  return 1000 + 60.0f * (float)(r.score - r0->score) + (float)(r.picked - r0->picked) + (r.x - r0->x) / 300;
}
static bool rider(const dn_run *r) {
  if (plan_left <= 0) {
    static const int plans[][2] = {{1, 0}, {0, 1}, {20, 10}, {10, 20}, {30, 30}, {45, 15}, {15, 45}, {60, 20}, {6, 60}, {40, 80}};
    float best = -1;
    for (unsigned k = 0; k < sizeof plans / sizeof plans[0]; k++) {
      float v = judge(r, plans[k][0], plans[k][1], 120);
      if (v > best) best = v, plan_h = plans[k][0], plan_r = plans[k][1];
    }
    plan_left = 8, plan_t = 0;
  }
  plan_left--;
  int cyc = plan_h + plan_r, p = cyc ? plan_t % cyc : 0;
  plan_t++;
  return p < plan_h;
}
/* a run: the world at rest, the level started, its first press */
static void ride_start(dn_run *r, uint32_t seed, int ball, int land) {
  dn_new(r, seed, (uint8_t)ball, (uint8_t)land);
  for (int i = 0; i < 90; i++) dn_step(r, false, false);
  dn_start(r);
  for (int i = 0; i < 20; i++) dn_step(r, false, false);
  dn_step(r, true, true);
  plan_left = 0;
}

/* ---------------------------------------------------------------- the rules against the spec */
static void units(void) {
  /* the dunes: d = min(2048, 700 + 1700 r0 (+1000 flat)), h = d/10 + 220 r1 - 50, m = 42 + 15 r2, width 0.7 d */
  dn_seg s;
  dn_curve c;
  dn_make_seg(&s, 0.5f, 0.25f, 0.8f, false);
  dn_shape(&s, &c);
  CHECK(NEAR(s.d, 1550, 1e-3) && NEAR(s.h, 160, 1e-3) && NEAR(s.m, 54, 1e-4) && NEAR(s.w, 1085, 1e-2), "segment %f %f %f %f", s.d, s.h, s.m, s.w);
  CHECK(NEAR(c.y[40], sin(2.0) * 160 * 40 / 54, 1e-2) && c.y[63] == 0 && c.y[0] == 0, "curve %f", c.y[40]);
  CHECK(NEAR(c.lx[3], 1550.0 / 90 * 24, 1e-3) && NEAR(c.ly[3], c.y[24] + 15, 1e-4) && NEAR(c.lx[8], s.w, 1e-3) && c.ly[8] == 15, "lines");
  CHECK(NEAR(c.line[2][2] - c.line[2][0], (c.lx[3] - c.lx[2]) * (1 + 0.1 / hypot(c.lx[3] - c.lx[2], c.ly[3] - c.ly[2])), 1e-2), "line + 0.1");
  dn_make_seg(&s, 0.9f, 0, 0, true);
  CHECK(s.d == 2048 && NEAR(s.w, 2048 * 0.7, 1e-2) && s.flat, "flat %f", s.d);
  dn_make_seg(&s, 0, 0, 0, false);
  CHECK(s.d == 700 && NEAR(s.h, 20, 1e-4) && s.m == 42, "smallest");
  for (int n = 1; n < 63; n++) CHECK(sinf(n / 20.0f) >= 0, "the curve stays down to 62");
  CHECK(sinf(63 / 20.0f) < 0, "and ends at 63");

  /* the shop: the game's prices, in its order */
  static const uint16_t ball_price[26] = {0, 50, 50, 50, 50, 100, 100, 100, 250, 250, 250, 500, 500, 500, 500, 750, 750, 750,
                                          1000, 1000, 1000, 1500, 1500, 1500, 3000, 5000};
  static const uint16_t land_price[7] = {0, 100, 150, 300, 400, 500, 600};
  static const uint8_t turns[26] = {0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 0, 0, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 1, 1, 1};
  unsigned total = 0;
  for (int i = 0; i < 26; i++) {
    CHECK(art_ball_info_[i].price == ball_price[i], "ball %d price %u", i, art_ball_info_[i].price);
    CHECK(art_ball_info_[i].rotates == turns[i], "ball %d turns", i);
    total += art_ball_info_[i].price;
  }
  CHECK(total == 21000, "balls %u", total);
  total = 0;
  for (int i = 0; i < 7; i++) {
    CHECK(art_land_info_[i].price == land_price[i], "landscape %d", i);
    total += art_land_info_[i].price;
  }
  CHECK(total == 2050, "landscapes %u", total);
  CHECK(!strcmp(art_ball_info_[0].name, "Basic Ball") && !strcmp(art_land_info_[0].name, "Night Land") &&
            !strcmp(art_land_info_[1].name, "Mountains"), "names %s %s", art_ball_info_[0].name, art_land_info_[0].name);
  CHECK(art_ball_info_[14].type == 3 && art_ball_info_[14].cx == 0 && art_ball_info_[16].cx == 25 && art_ball_info_[19].cx == 6, "radial centres");
  CHECK(art_land_info_[1].surface == 0xCCF173 && art_land_info_[2].line == 0xC04700 && art_land_info_[5].ground == 0x624BFE, "colours");

  /* at rest: on the flat start, sunk by p2's skin (0.1) into the lines 15 under the surface */
  dn_run r;
  dn_new(&r, 7, 0, 0);
  for (int i = 0; i < 240; i++) dn_step(&r, false, false);
  CHECK(NEAR(r.y, DN_TY + 15 - 42 + 0.1, 0.05) && NEAR(r.vy, 0, 1) && r.x == 700, "rest %f %f", r.y, r.vy);
  CHECK(NEAR(r.cam_y, 100, 0.5) && r.zoom == 1, "camera %f", r.cam_y);
  dn_start(&r);
  for (int i = 0; i < 17; i++) dn_step(&r, false, false);
  CHECK(!r.running, "not before 300 ms");
  dn_step(&r, true, true);
  CHECK(r.running && !r.valid && r.guide, "a press before the level runs doesn't count");
  dn_step(&r, true, true);
  CHECK(r.valid && !r.guide, "the first press after");

  /* InputHandler.update */
  dn_run q = r;
  q.running = q.valid = 1, q.touching = 1, q.vx = 100, q.vy = 0;
  input(&q, true);
  CHECK(q.vx == 158 && q.vy == -30 && q.moving, "slow on the ground %f %f", q.vx, q.vy);
  q.vx = 1000, q.vy = 0, input(&q, true);
  CHECK(q.vx == 1028 && NEAR(q.vy, 1028 / 80.0, 1e-4), "fast on the ground %f %f", q.vx, q.vy);
  q.touching = 0, q.vx = 1000, q.vy = 0, input(&q, true);
  CHECK(q.vx == 1016 && NEAR(q.vy, 80 + 1016 / 50.0, 1e-4), "in the air %f %f", q.vx, q.vy);
  q.vx = 2700, q.vy = 2300, input(&q, true);
  CHECK(q.vx == 2700 && q.vy == 2300, "the limits");
  q.vx = 500, q.vy = 0, q.valid = 0, input(&q, true);
  CHECK(q.vx == 500 && !q.moving, "no press yet");

  /* one p2 step: damping 0.01, gravity 1000 */
  q = r;
  q.running = 0, q.x = 700, q.y = -200, q.vx = 100, q.vy = 0;
  physics(&q);
  CHECK(NEAR(q.vx, 100 * pow(0.99, 1 / 60.0), 1e-3) && NEAR(q.vy, 1000 / 60.0, 1e-3), "free %f %f", q.vx, q.vy);
  /* a landing 5 into the flat line: 1/17 of the speed stays a step, 4/17 of the overlap is pushed out */
  q = r;
  q.running = 0, q.x = 700, q.vx = 0, q.vy = 600;
  q.y = DN_TY + 15 - 42 + 5;
  float d = (float)pow(0.99, 1 / 60.0), vy0 = 600 * d, want = vy0 / 17 + (-5 + 0.1f) * (4 * 60 / 17.0f);
  physics(&q);
  CHECK(NEAR(q.vy, want, 0.05), "landing %f, p2 %f", q.vy, want);
  /* the first line under the ground sensor holds the fall to 1000; leaving the ground while held: 1.03 */
  q = r;
  memset(q.ground, 0, sizeof q.ground);
  q.running = 1, q.polys = 0, q.x = 700, q.y = DN_TY + 15 - 69, q.vx = 0, q.vy = 1500;
  physics(&q);
  CHECK(NEAR(q.vy, 1000 * 1 + 1000 / 60.0, 1e-3), "held to 1000: %f", q.vy);
  q = r;
  q.running = 1, q.moving = 1, q.x = 700, q.y = DN_TY - 200, q.vx = 1000, q.vy = -100;
  memset(q.ground, 0, sizeof q.ground), q.ground[0] = 1, q.polys = 1, q.touching = 1;
  physics(&q);
  CHECK(NEAR(q.vx, 1000 * d * 1.03, 1e-2) && !q.touching, "take-off %f", q.vx);
  /* the dune with p2's default material: friction 0.3 takes at most 5 a step */
  q = r;
  q.order[1] = 8, q.spare = 1, q.seg[8].x = q.seg[1].x;
  dn_shapes(&q);
  q.running = 0, q.rotates = 0, q.vx = 600, q.vy = 0, q.x = q.seg[8].x + 4;
  q.y = DN_TY + q.curve[8].ly[0] - 42 + 0.5f;
  q.seg[8].x = q.x - 2;
  float before = q.vx * d;
  physics(&q);
  CHECK(q.vx < before - 1 && q.vx >= before - 5.01f, "friction %f %f", q.vx, before);

  /* the line: +1 above -800, +2 above -1300, else +3 */
  static const float speeds[3] = {-700, -1000, -1500};
  for (int k = 0; k < 3; k++) {
    q = r;
    q.running = 1, q.above = 0, q.score = 0, q.x = 700, q.y = 221, q.vx = 900, q.vy = speeds[k];
    physics(&q);
    CHECK(q.score == (uint32_t)k + 1, "line %d: %u", k, q.score);
  }
  /* the coins in the air: 4 or 5 on the way up, then the arrow (7), or high up 3x3, the letters (38), 5x3, 3x6 */
  int seen[64] = {0};
  for (int k = 0; k < 2000; k++) {
    q = r;
    memset(q.coin, 0, sizeof q.coin);
    q.rng = 1000u + (uint32_t)k * 7919u;
    q.x = 5000, q.y = k & 1 ? 0 : 200, q.vx = 1500, q.vy = k & 1 ? -1200 : -600;
    air_coins(&q);
    int n = 0;
    for (int i = 0; i < DN_COINS; i++) n += q.coin[i].on;
    seen[n]++;
  }
  CHECK(seen[10] + seen[11] == 1000 && seen[10] > 400 && seen[11] > 400, "the arrow: %d %d", seen[10], seen[11]);
  CHECK(seen[13] + seen[14] > 150 && seen[42] + seen[43] > 80 && seen[19] + seen[20] > 200 && seen[22] + seen[23] > 200,
        "the shapes %d %d %d %d", seen[13] + seen[14], seen[42] + seen[43], seen[19] + seen[20], seen[22] + seen[23]);

  /* a crash: landing hard on the last 30% of a dune after 20 frames in the air */
  q = r;
  dn_seg *g1 = &q.seg[q.order[1]];
  q.running = 1, q.valid = 0, q.x = g1->x + 0.8f * g1->w, q.y = DN_TY - 100, q.cur = q.order[1];
  q.last_vx = 500, q.last_vy = 800, q.vx = 300, q.vy = 100, q.airborn = 30, q.hist = 0;
  dn_step(&q, false, false);
  CHECK(!q.alive && q.damping == 0.5f && q.vx != q.vx + 1, "crash");
  q = r;
  q.running = 1, q.valid = 0, q.x = g1->x + 0.5f * g1->w, q.y = DN_TY - 100, q.cur = q.order[1];
  q.last_vx = 500, q.last_vy = 800, q.vx = 300, q.vy = 100, q.airborn = 30, q.hist = 0;
  dn_step(&q, false, false);
  CHECK(q.alive, "no crash on the way down");

  /* a slide: perfect from under 0.2 (+3, 1.2), nice from under 0.45 (+1, 1.05) */
  for (int k = 0; k < 2; k++) {
    q = r;
    dn_seg *g = &q.seg[q.order[2]];
    q.running = 1, q.valid = 0, q.cur = q.order[2], q.x = g->x + 0.8f * g->w, q.y = 300;
    q.sliding = 1, q.can_slide = 1, q.slide_start = k ? 0.3f : 0.1f, q.score = 0, q.vx = 2000, q.vy = 300;
    dn_step(&q, false, false);
    CHECK(q.score == (k ? 1u : 3u) && q.slide_kind == (k ? DN_POP_NICE : DN_POP_PERFECT) && !q.can_slide, "slide %d: %u", k, q.score);
  }
  /* the zoom: 1 low down, 0.4 at the top */
  q = r;
  q.running = 1, q.y = -1500, q.vy = -100, q.zoom = 0.4f;
  dn_step(&q, false, false);
  CHECK(NEAR(q.zoom, 0.4, 1e-4), "zoom %f", q.zoom);
  /* a broken state is refused */
  q = r;
  CHECK(dn_valid(&q), "fresh run");
  q.vx = NAN;
  CHECK(!dn_valid(&q), "NaN accepted");
  q = r, q.order[3] = q.order[4];
  CHECK(!dn_valid(&q), "a dune twice");
}

/* ---------------------------------------------------------------- the rider's runs, and the steps for the oracle */
static FILE *cases;
static int ncases, case_every;
static float pre[6];
static int pre_n;
static void trace(const dn_run *r, int after) {
  if (!cases || ncases >= 6000) return;
  if (!after) {
    pre[0] = r->x, pre[1] = r->y, pre[2] = r->vx, pre[3] = r->vy, pre[4] = r->w, pre[5] = r->angle;
    pre_n = 1;
    return;
  }
  bool sticky = r->ground[8] != 0;   /* (every step on the dune with p2's default material, the others now and then) */
  if (!pre_n || (r->frame % (unsigned)case_every && !sticky) || (!sticky && ncases >= 4000)) return;
  pre_n = 0;
  /* the dunes near the ball: their place and collision points, sticky or not */
  fprintf(cases, "%s\n{\"b\":[%.9g,%.9g,%.9g,%.9g,%.9g],\"a\":[%.9g,%.9g,%.9g,%.9g,%.9g],\"turns\":%d,\"segs\":[", ncases ? "," : "",
          pre[0], pre[1], pre[2], pre[3], pre[4], r->x, r->y, r->vx, r->vy, r->w, r->rotates);
  int k = 0;
  for (int i = 0; i < DN_SEGS; i++) {
    const dn_seg *s = &r->seg[i];
    if (!(pre[0] > s->x - 130 && pre[0] < s->x + s->w + 130)) continue;
    fprintf(cases, "%s{\"x\":%.9g,\"sticky\":%d,\"p\":[", k++ ? "," : "", s->x, s->sticky);
    for (int j = 0; j < 9; j++) fprintf(cases, "%s%.9g,%.9g", j ? "," : "", r->curve[i].lx[j], r->curve[i].ly[j]);
    fprintf(cases, "]}");
  }
  fprintf(cases, "]}");
  ncases++;
}

/* the dunes, the input and the coins in the air, for the oracle */
static void oracle_cases(void) {
  if (!cases) return;
  uint32_t g = 99;
  fprintf(cases, "\n],\"terrain\":[");
  for (int i = 0; i < 100; i++) {
    float r0 = dn_frand(&g), r1 = dn_frand(&g), r2 = dn_frand(&g);
    bool flat = i % 11 == 0;
    dn_seg s;
    dn_curve c;
    dn_make_seg(&s, r0, r1, r2, flat);
    dn_shape(&s, &c);
    fprintf(cases, "%s\n{\"r\":[%.9g,%.9g,%.9g],\"flat\":%d,\"w\":%.9g,\"y\":[", i ? "," : "", r0, r1, r2, flat, s.w);
    for (int n = 0; n < 64; n++) fprintf(cases, "%s%.9g", n ? "," : "", c.y[n]);
    fprintf(cases, "],\"lines\":[");
    for (int k = 0; k < 8; k++) fprintf(cases, "%s%.9g,%.9g,%.9g,%.9g", k ? "," : "", c.line[k][0], c.line[k][1], c.line[k][2], c.line[k][3]);
    float px[64], py[64];
    int nc = coin_points(&s, &c, px, py, 64);
    fprintf(cases, "],\"coins\":[");
    for (int k = 0; k < nc; k++) fprintf(cases, "%s%.9g,%.9g", k ? "," : "", px[k], py[k]);
    fprintf(cases, "]}");
  }
  fprintf(cases, "],\"input\":[");
  for (int i = 0; i < 1000; i++) {
    dn_run q;
    memset(&q, 0, sizeof q);
    q.running = 1, q.valid = i % 7 != 0, q.touching = i % 2 == 0;
    q.vx = dn_frand(&g) * 4000, q.vy = dn_frand(&g) * 6000 - 2500;
    bool held = i % 5 != 0;
    float vx = q.vx, vy = q.vy;
    input(&q, held);
    fprintf(cases, "%s[%.9g,%.9g,%d,%d,%d,%.9g,%.9g]", i ? "," : "", vx, vy, q.touching, held, q.valid, q.vx, q.vy);
  }
  fprintf(cases, "],\"air\":[");
  for (int i = 0; i < 300; i++) {
    dn_run q;
    memset(&q, 0, sizeof q);
    q.rng = 777u + (uint32_t)i * 977u;
    q.x = 3000, q.y = 221 - dn_frand(&g) * 300, q.vx = 800 + dn_frand(&g) * 2000, q.vy = -300 - dn_frand(&g) * 1800;
    uint32_t peek = q.rng;
    float ra = dn_frand(&peek), rb = dn_frand(&peek);
    air_coins(&q);
    fprintf(cases, "%s\n{\"s\":[%.9g,%.9g,%.9g,%.9g],\"r\":[%.9g,%.9g],\"c\":[", i ? "," : "", q.x, q.y, q.vx, q.vy, ra, rb);
    int k = 0;
    for (int j = 0; j < DN_COINS; j++)
      if (q.coin[j].on) fprintf(cases, "%s%.9g,%.9g", k++ ? "," : "", q.coin[j].x, q.coin[j].y);
    fprintf(cases, "]}");
  }
}

static void rides(void) {
  int lines = 0, perfect = 0, nice = 0, crashes = 0, sticky = 0, recycled = 0;
  uint32_t coins = 0, best = 0, total = 0;
  dn_trace = trace, case_every = 5;
  for (int s = 1; s <= 21; s++) {
    dn_run r;
    ride_start(&r, (uint32_t)s * 2654435761u, s % 26, s % 7);
    uint8_t last_slide = 0, last_line = 0;
    for (int f = 0; f < 60 * 45 && r.alive; f++) {
      bool hold = rider(&r);
      dn_step(&r, hold, hold && f % 8 == 0);
      if (r.line_t && r.line_t > last_line) lines++;
      last_line = r.line_t;
      if (r.slide_t == 2 && last_slide != 2) r.slide_kind == DN_POP_PERFECT ? perfect++ : nice++;
      last_slide = r.slide_t;
      if (r.cur == 8) sticky++;
      if (f % 60 == 0) CHECK(dn_valid(&r), "seed %d: invalid at %d", s, f);
    }
    crashes += !r.alive;
    coins += r.picked, recycled += (int)r.spawned;
    total += r.score;
    if (r.score > best) best = r.score;
    for (int i = 0; i < 200; i++) dn_step(&r, false, false);
    CHECK(dn_valid(&r), "seed %d: invalid after", s);

  }
  CHECK(lines > 100 && perfect > 5 && nice > 5 && coins > 200 && recycled > 50 && sticky > 0 && best >= 40,
        "lines %d perfect %d nice %d coins %u recycled %d sticky %d best %u", lines, perfect, nice, coins, recycled, sticky, best);
  /* a crash happens: the rider holding all the way down a dune's far side */
  int crashed = 0;
  for (int s = 1; s <= 12 && !crashed; s++) {
    dn_run r;
    ride_start(&r, (uint32_t)s * 40503u, 0, 0);
    for (int f = 0; f < 3000 && r.alive; f++) dn_step(&r, (f / 50) % 2 == 0, f % 50 == 0);
    crashed += !r.alive;
  }
  CHECK(crashed, "no crash");
  printf("rider: 21 runs of up to 45 s, %u points (best %u), %d lines, %d perfect and %d nice slides, %u coins, %d crashes\n",
         total, best, lines, perfect, nice, coins, crashes);
}

/* ---------------------------------------------------------------- saves */
static void saves(void) {
  dn_run r, back;
  dn_profile p, q;
  dn_profile_new(&p);
  p.best = 77, p.coins = 1234, p.runs = 9, p.balls = 1 | 1u << 5 | 1u << 23, p.lands = 1 | 1 << 4, p.ball = 5, p.land = 4;
  ride_start(&r, 4242, 5, 4);
  for (int i = 0; i < 1500 && r.alive; i++) dn_step(&r, rider(&r), false);
  CHECK(r.alive, "the rider survives 25 s");
  forget(DN_SAVE_NAME), forget(DN_COPY_NAME);
  CHECK(save_write(&p, &r), "save failed");
  memset(&back, 0, sizeof back);
  bool has = save_load(&q, &back);
  CHECK(has && q.best == 77 && q.coins == 1234 && q.balls == p.balls && q.lands == p.lands && q.ball == 5 && q.land == 4, "profile back");
  CHECK(!memcmp(&back, &r, sizeof r), "run back");
  for (int i = 0; i < 600; i++) {
    bool h = rider(&r);
    dn_step(&r, h, false), dn_step(&back, h, false);
  }
  CHECK(!memcmp(&back, &r, DN_RUN_SAVED), "run diverged after loading");
  /* the file gone: the copy in the script brings it back */
  forget(DN_SAVE_NAME);
  memset(&q, 0, sizeof q);
  has = save_load(&q, &back);
  CHECK(has && q.coins == 1234, "copy back");
  uint32_t n;
  CHECK(plat_load(DN_SAVE_NAME, &n) != 0, "save restored from the copy");
  const uint8_t *cp = plat_load(DN_COPY_NAME, &n);
  printf("saves: %u bytes, its copy %u\n", (unsigned)sizeof(dn_file), n);
  (void)cp;
  /* every byte damaged in turn: refused, a clean start, never a crash */
  save_write(&p, &r);
  uint8_t keep[8192];
  const uint8_t *dd = plat_load(DN_SAVE_NAME, &n);
  memcpy(keep, dd, n);
  int refused = 0;
  for (uint32_t i = 0; i < n; i++) {
    uint8_t bad[8192];
    memcpy(bad, keep, n);
    bad[i] ^= (uint8_t)(1 + i % 255);
    plat_save(DN_SAVE_NAME, bad, n);
    forget(DN_COPY_NAME);
    memset(&q, 0xAB, sizeof q);
    has = save_load(&q, &back);
    refused += q.best == 0 && q.coins == 0 && q.balls == 1 && !has;
  }
  CHECK(refused == (int)n, "damaged saves refused: %d of %u", refused, n);
  plat_save(DN_SAVE_NAME, keep, n - 1), forget(DN_COPY_NAME);
  save_load(&q, &back);
  CHECK(q.coins == 0, "short save");
  /* a damaged copy too */
  save_write(&p, 0);
  forget(DN_SAVE_NAME);
  for (int i = 0; i < 4; i++)
    if (files[i].on && !strcmp(files[i].name, DN_COPY_NAME)) files[i].data[files[i].len - 9] ^= 0x15;
  save_load(&q, &back);
  CHECK(q.coins == 0 || q.coins == 1234, "damaged copy");
  printf("saves: every damaged byte refused\n");
}

/* ---------------------------------------------------------------- the whole app with random keys */
static uint32_t fuzz_rng = 77;
static uint32_t fuzz_keys(uint32_t f) {
  static uint32_t held, until;
  if (f >= until) {
    uint32_t x = dn_rand(&fuzz_rng);
    static const uint32_t pick[] = {0, K_OK, K_OK, K_OK, K_SHIFT, K_UP, K_DOWN, K_LEFT, K_RIGHT, K_BACK, K_OK, K_OK, 0};
    held = pick[x % 13];
    until = f + 1 + (x >> 8) % 40;
  }
  return held;
}
/* one short tap counts, even when the loop it falls in goes by without a step of the game */
static uint32_t tap_at;
static uint8_t tap_valid;
static uint32_t tap_keys(uint32_t f) {
  if (f == 60) return K_OK;                       /* Play */
  if (f == tap_at + 60) tap_valid = run.valid && !run.guide;
  return f == tap_at ? K_OK : 0;                  /* one key scan of 10 ms */
}
static void taps(void) {
  int ok = 0;
  for (uint32_t k = 0; k < 6; k++) {
    forget(DN_SAVE_NAME), forget(DN_COPY_NAME);
    memset(&run, 0, sizeof run), memset(&prof, 0, sizeof prof), memset(&view, 0, sizeof view);
    tap_at = 130 + k, tap_valid = 0;
    script = tap_keys, frame_no = 0, quit_at = 260;
    dn_app_main();
    ok += tap_valid;
  }
  CHECK(ok == 6, "short taps that counted: %d of 6", ok);
}

static void fuzz(void) {
  taps();
  int sessions = 300;
  dn_clipped = 0;
  for (int s = 0; s < sessions; s++) {
    if (s % 50 == 0) forget(DN_SAVE_NAME), forget(DN_COPY_NAME);
    memset(&run, 0, sizeof run), memset(&prof, 0, sizeof prof), memset(&view, 0, sizeof view);
    fuzz_rng = (uint32_t)s * 7919u + 1;
    script = fuzz_keys, frame_no = 0, quit_at = 150 + dn_rand(&fuzz_rng) % 2000;
    if (s % 10 == 0) prof.coins = 0;
    dn_app_main();
    uint32_t n;
    const uint8_t *d = plat_load(DN_SAVE_NAME, &n);
    CHECK(d && n == sizeof(dn_file), "session %d: no save", s);
    if (d) {
      static dn_file f;
      static dn_run scratch_;
      memcpy(&f, d, sizeof f);
      CHECK(dn_file_ok(&f, &scratch_), "session %d: the save doesn't read back", s);
    }
  }
  CHECK(dn_clipped == 0, "text past the screen's edge: %d", dn_clipped);
  printf("fuzz: %d sessions of random keys, each quit with Home and its save read back\n", sessions);
}

/* ---------------------------------------------------------------- pictures */
static void picture(const char *dir, const char *name) {
  char path[512];
  snprintf(path, sizeof path, "%s/%s.ppm", dir, name);
  sync_view(view.clock);
  dn_redraw();
  dn_draw(&view, plat_push);
  shot(path);
}

static void ride_until(uint32_t seed, int ball, int land, bool (*want)(const dn_run *), int max) {
  for (uint32_t s = seed;; s++) {
    ride_start(&run, s * 2654435761u, ball, land);
    for (int f = 0; f < max && run.alive; f++) {
      dn_step(&run, rider(&run), false);
      if (want(&run)) return;
    }
    if (s > seed + 40) return;
  }
}
static bool w_high(const dn_run *r) { return r->line_t > 30 && r->zoom < 0.6f; }
static bool w_slide(const dn_run *r) { return r->slide_t > 14 && r->slide_t < 20 && r->slide_kind == DN_POP_PERFECT; }
static bool w_down(const dn_run *r) { return r->touching && r->vx > 1500 && r->frame > 500 && r->zoom > 0.85f; }
static bool w_coins(const dn_run *r) {
  int n = 0;
  for (int i = 0; i < DN_COINS; i++) n += r->coin[i].on && r->coin[i].x > r->x;
  return n > 14 && r->vy < 0;
}

static void shots(const char *dir) {
  dn_profile_new(&prof);
  prof.best = 64, prof.coins = 820, prof.balls = 1 | 1 << 5 | 1 << 6 | 1 << 13, prof.lands = 3, prof.ball = 5, prof.land = 1;
  memset(&view, 0, sizeof view);
  view.clock = 1000;
  rng = 5;
  fresh_world();
  view.screen = V_TITLE, view.sel = TITLE_PLAY;
  picture(dir, "title");
  view.screen = V_PLAY;
  ride_until(3, 5, 1, w_down, 4000);
  picture(dir, "shot");
  ride_until(11, 13, 0, w_high, 4000);
  picture(dir, "high");
  ride_until(21, 23, 3, w_slide, 6000);
  picture(dir, "slide");
  ride_until(31, 6, 2, w_coins, 6000);
  picture(dir, "coins");
  static const int lands[7] = {0, 1, 2, 3, 4, 5, 6};
  for (int k = 0; k < 7; k++) {
    ride_until(41 + (uint32_t)k, k * 3 + 1, lands[k], w_down, 3000);
    char n[16];
    snprintf(n, sizeof n, "land%d", k);
    picture(dir, n);
  }
  ride_start(&run, 99, 0, 1);
  dn_new(&run, 99, 5, 1);
  for (int i = 0; i < 90; i++) dn_step(&run, false, false);
  dn_start(&run);
  for (int i = 0; i < 30; i++) dn_step(&run, false, false);
  view.screen = V_PLAY;
  picture(dir, "guide");
  ride_until(3, 5, 1, w_down, 4000);
  view.screen = V_PAUSE, view.sel = PAUSE_SHOP;
  picture(dir, "pause");
  view.screen = V_QUIT, view.sel = 0;
  picture(dir, "quit");
  view.screen = V_SHOP, view.under = V_TITLE, view.shop_cat = SHOP_BALLS, view.shop_sel = 6, view.shop_side = 0;
  dn_menu(&run);
  for (int i = 0; i < 90; i++) dn_step(&run, false, false);
  picture(dir, "shop");
  view.shop_sel = 20;
  picture(dir, "shop2");
  view.shop_cat = SHOP_LANDS, view.shop_sel = 2;
  picture(dir, "landscapes");
  view.screen = V_BUY, view.buy_yes = 1;
  picture(dir, "buy");
  view.shop_cat = SHOP_BALLS, view.shop_sel = 13;
  view.screen = V_BUY;
  picture(dir, "buyball");
  view.screen = V_HELP;
  picture(dir, "help");
  /* a crash */
  for (uint32_t s = 1; s < 50; s++) {
    ride_start(&run, s * 40503u, 5, 1);
    for (int f = 0; f < 3000 && run.alive; f++) dn_step(&run, (f / 50) % 2 == 0, f % 50 == 0);
    if (!run.alive) break;
  }
  run.score = 23;
  for (int i = 0; i < 20; i++) dn_step(&run, false, false);
  view.screen = V_RESULT;
  picture(dir, "result");
}

static void film(const char *dir, int seed, int start, int frames) {
  dn_profile_new(&prof);
  prof.best = 64, prof.coins = 820, prof.ball = 23, prof.land = 1;
  memset(&view, 0, sizeof view);
  view.screen = V_PLAY;
  ride_start(&run, (uint32_t)seed * 2654435761u, 23, 1);
  char path[512];
  for (int f = -start; f < frames; f++) {
    for (int k = 0; k < 2; k++) {
      if (!run.alive) break;
      dn_step(&run, rider(&run), false);
      prof.coins += run.picked, run.picked = 0;
    }
    if (f < 0) continue;
    snprintf(path, sizeof path, "%s/f_%06d.raw", dir, f * 1000 / 30);
    sync_view(0);
    view.coins = prof.coins;
    dn_redraw();
    dn_draw(&view, plat_push);
    FILE *o = fopen(path, "wb");
    if (!o) exit(2);
    fwrite(fb, 2, 320 * 240, o);
    fclose(o);
  }
}

/* how long a frame takes to draw on this computer, for each landscape (to compare them) */
static void bench(void) {
  dn_profile_new(&prof);
  memset(&view, 0, sizeof view);
  view.screen = V_PLAY;
  for (int land = 0; land < 7; land++) {
    ride_start(&run, 12345, 5, land);
    double t = 0;
    int n = 0;
    for (int f = 0; f < 1200 && run.alive; f++) {
      dn_step(&run, rider(&run), false);
      if (f % 2) continue;
      sync_view(0);
      clock_t c0 = clock();
      dn_draw(&view, plat_push);
      t += (double)(clock() - c0) / CLOCKS_PER_SEC, n++;
    }
    printf("landscape %d: %.0f us a frame\n", land, t / n * 1e6);
  }
}

int main(int argc, char **argv) {
  if (argc >= 2 && !strcmp(argv[1], "bench")) {
    bench();
    return 0;
  }
  if (argc >= 3 && !strcmp(argv[1], "test")) {
    char path[512];
    snprintf(path, sizeof path, "%s/cases.json", argv[2]);
    cases = fopen(path, "w");
    if (cases) fprintf(cases, "{\"steps\":[");
    clock_t c0 = clock();
    units();
    rides();
    oracle_cases();
    if (cases) fprintf(cases, "]}\n"), fclose(cases), cases = 0;
    clock_t c1 = clock();
    saves();
    fuzz();
    fprintf(stderr, "(%.0f s rules and rides, %.0f s saves and keys; %d oracle steps)\n", (double)(c1 - c0) / CLOCKS_PER_SEC,
            (double)(clock() - c1) / CLOCKS_PER_SEC, ncases);
    printf(fails ? "%d FAILED\n" : "all passed\n", fails);
    return fails != 0;
  }
  if (argc >= 3 && !strcmp(argv[1], "shots")) {
    shots(argv[2]);
    return fails != 0;
  }
  if (argc >= 6 && !strcmp(argv[1], "film")) {
    film(argv[2], atoi(argv[3]), atoi(argv[4]), atoi(argv[5]));
    return 0;
  }
  fprintf(stderr, "usage: play test DIR | shots DIR | film DIR SEED START FRAMES\n");
  return 2;
}
