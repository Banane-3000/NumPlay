/* Stickman Hook on a computer, for tests and pictures (make host, make test).
 *
 *   play bot DIR    (build/bot) a bot clears each of the 39 levels made by hand, and 40 random courses, holding
 *                   and letting go of OK only (a search over hold/let go every 6 steps); DIR/replays.bin
 *   play test DIR   the rules against the spec, the bot's replays played again (and their steps written to
 *                   DIR/cases.json for tests/oracle.cjs), the whole app through its screens with scripted keys,
 *                   saves written and read back, damaged saves refused, and a few hundred sessions of random keys
 *   play shots DIR  pictures of the screens (DIR/<name>.ppm)
 *   play film DIR LEVELS N   N frames of the bot through LEVELS (0-based, comma separated), 20 a second
 */
#define main sh_app_main
#include "../src/main.c"
#undef main
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "art.h"

/* ---------------------------------------------------------------- the calculator, on a computer */
static uint16_t fb[240][320];
void plat_push(int x, int y, int w, int h, const uint16_t *px) {
  for (int j = 0; j < h; j++) memcpy(&fb[y + j][x], px + j * w, (size_t)w * 2);
}
static uint32_t clock_ms;
static uint32_t (*script)(uint32_t frame);
static uint32_t frame_no, quit_at, quit_key = SK_HOME;
uint32_t plat_keys(void) {
  clock_ms += 50;   /* the app's loop at 20 a second: 3 steps a frame, always */
  frame_no++;
  if (frame_no >= quit_at) return quit_key;
  return script ? script(frame_no) : 0;
}
uint32_t plat_millis(void) { return clock_ms; }
void plat_sleep(uint32_t ms) { (void)ms; }
uint32_t plat_random(void) { return 0x9E3779B9u ^ frame_no * 2654435761u; }
void plat_begin(void) {}
int plat_end(void) { return 0; }

static struct { char name[32]; uint8_t data[1024]; uint32_t len; bool on; } files[4];
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
#define CHECK(c, ...) do { if (!(c)) { fails++; printf("FAIL %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)
static bool near(float a, float b, float e) { return fabsf(a - b) <= e; }

/* ---------------------------------------------------------------- the bot: a beam search over hold / let go */
#define BEAM 600   /* tried with 96, then wider if that's not enough */
#define DECISIONS 520
#define STRIDE 6   /* steps a decision lasts */
typedef struct { sh_game g; float score; uint8_t path[DECISIONS]; } node;
static node beam[BEAM], cand[BEAM * 2];
static int by_score(const void *a, const void *b) {
  float x = ((const node *)a)->score, y = ((const node *)b)->score;
  return x < y ? 1 : x > y ? -1 : 0;
}
static bool same(const sh_game *a, const sh_game *b) {
  return a->held == b->held && a->hooked == b->hooked && (int)(a->x * 45) == (int)(b->x * 45) &&
         (int)(a->y * 35) == (int)(b->y * 35) && (int)(a->vx * 8) == (int)(b->vx * 8) && (int)(a->vy * 8) == (int)(b->vy * 8);
}
/* the decisions that reach the line without a death, or 0; the first is always "let go" (the level has just
 * started: OK is up) */
static int solve_with(uint32_t level, const sh_course *c, uint8_t *out, int width) {
  uint32_t r = 1;
  memset(&beam[0], 0, sizeof beam[0]);
  sh_new(&beam[0].g, level, c);
  int count = 1;
  for (int step = 0; step < DECISIONS; step++) {
    int n = 0;
    for (int j = 0; j < count; j++)
      for (int a = 0; a < (step ? 2 : 1); a++) {
        node *k = &cand[n];
        *k = beam[j];
        k->path[step] = (uint8_t)a;
        for (int t = 0; t < STRIDE && !k->g.completed; t++) sh_step(&k->g, a != 0, &r);
        if (k->g.attempt != 1) continue;
        if (k->g.completed) {
          memcpy(out, k->path, (size_t)step + 1);
          return step + 1;
        }
        k->score = k->g.x - 0.09f * fabsf(k->g.y - 0.48f) + 0.09f * k->g.vx - 0.025f * fmaxf(k->g.vy, 0);
        n++;
      }
    qsort(cand, (size_t)n, sizeof(node), by_score);
    count = 0;
    for (int i = 0; i < n && count < width; i++) {
      bool dup = false;
      for (int j = 0; j < count && !dup; j++) dup = same(&cand[i].g, &beam[j].g);
      if (!dup) beam[count++] = cand[i];
    }
    if (!count) break;
  }
  return 0;
}
static int solve(uint32_t level, const sh_course *c, uint8_t *out) {
  int n = solve_with(level, c, out, 96);
  return n ? n : solve_with(level, c, out, BEAM);
}

#define RANDOM_COURSES 100
typedef struct { uint32_t level; sh_course c; uint16_t n; uint8_t path[DECISIONS]; } replay;
static replay replays[SH_FIXED + RANDOM_COURSES];
static int nreplays;

static int bot(const char *dir) {
  int fixed = 0, random = 0;
  uint32_t rng = 2024;
  clock_t t0 = clock();
  for (uint32_t i = 0; i < SH_FIXED + RANDOM_COURSES; i++) {
    replay *p = &replays[nreplays];
    p->level = i < SH_FIXED ? i : SH_FIXED + (i - SH_FIXED) * 7;
    sh_pick(p->level, &rng, &p->c);
    int n = solve(p->level, &p->c, p->path);
    if (i < SH_FIXED) {
      CHECK(n > 0, "level %u: the bot found no way through", i + 1);
      fixed += n > 0;
    } else {
      random += n > 0;
      if (!n) printf("random course %u-%u-%u: not solved\n", p->c.part[0], p->c.part[1], p->c.part[2]);
    }
    if (n) p->n = (uint16_t)n, nreplays++;
  }
  CHECK(random >= RANDOM_COURSES * 98 / 100, "only %d of %d random courses solved", random, RANDOM_COURSES);
  char path[512];
  snprintf(path, sizeof path, "%s/replays.bin", dir);
  FILE *f = fopen(path, "wb");
  if (!f) return 2;
  fwrite(replays, sizeof replays[0], (size_t)nreplays, f);
  fclose(f);
  printf("bot: %d / %d levels made by hand and %d / %d random courses cleared, holding and letting go only (%.0f s)\n",
         fixed, SH_FIXED, random, RANDOM_COURSES, (double)(clock() - t0) / CLOCKS_PER_SEC);
  return fails != 0;
}

__attribute__((unused)) static bool load_replays(const char *dir) {
  char path[512];
  snprintf(path, sizeof path, "%s/replays.bin", dir);
  FILE *f = fopen(path, "rb");
  if (!f) return false;
  nreplays = (int)fread(replays, sizeof replays[0], SH_FIXED + RANDOM_COURSES, f);
  fclose(f);
  return nreplays > 0;
}

/* ---------------------------------------------------------------- the rules against the spec */
__attribute__((unused)) static void units(void) {
  sh_course c;
  uint32_t r = 7;
  /* the courses: the introductory levels, then the recipes (LEVEL_009 is START006, MIDDLE002, FINISH005) */
  for (uint32_t l = 0; l < SH_INTRO; l++) {
    sh_pick(l, &r, &c);
    CHECK(c.part[0] == l && c.part[1] == SH_NO_PART && c.background == sh_parts[l].background && sh_course_ok(l, &c),
          "level %u", l + 1);
  }
  sh_pick(9, &r, &c);
  CHECK(c.part[0] == 15 && c.part[1] == 42 && c.part[2] == 88, "level 10: %u %u %u", c.part[0], c.part[1], c.part[2]);
  sh_pick(38, &r, &c);
  CHECK(c.part[0] == 16 && c.part[1] == 72 && c.part[2] == 113, "level 39");
  /* then any start, middle and end, evenly, and either background */
  int seen[SH_PARTS] = {0}, light = 0;
  for (int i = 0; i < 60000; i++) {
    sh_pick(39 + (uint32_t)i, &r, &c);
    CHECK(sh_course_ok(39 + (uint32_t)i, &c), "random course");
    for (int k = 0; k < 3; k++) seen[c.part[k]]++;
    light += c.background;
  }
  for (int p = SH_FIRST_START; p < SH_PARTS; p++) {
    int n = p < SH_FIRST_MIDDLE ? SH_STARTS : p < SH_FIRST_END ? SH_MIDDLES : SH_ENDS;
    CHECK(seen[p] > 60000 / n * 8 / 10 && seen[p] < 60000 / n * 12 / 10, "section %d drawn %d times", p, seen[p]);
  }
  CHECK(light > 28500 && light < 31500, "backgrounds %d", light);
  /* a course from another level, or broken, is refused */
  sh_pick(9, &r, &c);
  CHECK(!sh_course_ok(10, &c) && !sh_course_ok(3, &c), "a recipe elsewhere");
  c.part[0] = SH_FIRST_MIDDLE;
  CHECK(!sh_course_ok(50, &c), "a middle as a start");
  /* the goal shown: the next multiple of 5 */
  static const uint32_t goal[][2] = {{0, 5}, {3, 5}, {4, 10}, {8, 10}, {9, 15}, {39, 45}, {98, 100}, {99, 105}};
  for (unsigned i = 0; i < sizeof goal / sizeof goal[0]; i++)
    CHECK(sh_fake_max(goal[i][0]) == goal[i][1], "fakeMaxLevel(%u) = %u", goal[i][0], sh_fake_max(goal[i][0]));
  /* skins: 15 of 23 from the start; Plant on reaching level 5, SuperStick on reaching 150 */
  int open = 0;
  for (int i = 0; i < SH_SKINS; i++) open += sh_skin_open(0, i);
  CHECK(open == 15, "%d skins open at first", open);
  CHECK(!sh_skin_open(3, 2) && sh_skin_open(4, 2), "Plant");
  CHECK(!sh_skin_open(148, 9) && sh_skin_open(149, 9), "SuperStick");
  static const uint8_t levels[SH_SKINS] = {0, 0, 5, 10, 20, 30, 50, 75, 100, 150};
  CHECK(!memcmp(sh_unlock_level, levels, sizeof levels), "the skins' levels");

  /* falling from rest for a second (240 steps of 1/240), nothing around: v = 1, y += t^2 n (n + 1) / 2 */
  sh_game g;
  sh_pick(0, &r, &c);
  sh_new(&g, 0, &c);
  g.sections = 0, g.finish = 10;
  float y0 = g.y;
  for (int i = 0; i < 60; i++) sh_step(&g, false, &r);
  CHECK(g.attempt == 1 && near(g.vy, 1, 1e-4f) && near(g.y - y0, 240.0f * 241 / 2 / 57600, 1e-4f) && g.vx == 0,
        "free fall: vy %f, %f", g.vy, g.y - y0);
  /* hooking (level 1's single hook), the rope pulling in 0.3 * 0.2 = 0.06, letting go at 1.2 times the speed */
  sh_new(&g, 0, &c);
  sh_step(&g, false, &r);
  CHECK(g.near == 0 && near(g.near_scale, 0.4f + 5.0f / 60, 1e-5f), "the closest hook's ring %d %f", g.near, g.near_scale);
  sh_vec h = sh_hook_at(&g, 0);
  float vx = g.vx, vy = g.vy, dx = g.x - h.x, dy = g.y - h.y, r0 = sqrtf(dx * dx + dy * dy);
  float th = atan2f(dx, dy), om = (cosf(th) * vx - sinf(th) * vy) / r0 * 1.075f;
  sh_game k = g;
  sh_step(&k, true, &r);
  CHECK(k.hooked && k.hook_x == h.x && k.pull > 0, "hooked");
  {
    /* the same step by hand */
    float tt = 1.0f / 240, rr = r0, pull = 0.2f, the = th, ome = om;
    for (int s = 0; s < 4; s++) {
      rr -= 0.3f * fminf(tt, pull), pull -= tt;
      ome += -sinf(the) / rr * tt, the += ome * tt;
    }
    CHECK(near(k.r, rr, 1e-6f) && near(k.theta, the, 1e-6f) && near(k.omega, ome, 1e-5f), "hooked step");
  }
  for (int i = 0; i < 12; i++) sh_step(&k, true, &r);
  CHECK(near(k.r, r0 - 0.06f, 1e-4f) || k.r == 0.05f, "pulled in to %f from %f", k.r, r0);
  CHECK(k.pull < 0, "the pull ends just under 0 (%g), as in the game", k.pull);
  vx = k.vx, vy = k.vy;
  sh_step(&k, false, &r);
  CHECK(!k.hooked && k.pose == SH_BALL && near(k.spin, 20 * vx * 1.2f, 1e-3f), "let go");
  /* holding on never moves to another hook */
  k = g;
  for (int i = 0; i < 90 && !k.completed && k.attempt == 1; i++) {
    sh_step(&k, true, &r);
    if (!k.completed) CHECK(k.hooked && k.hook_x == h.x, "the same hook");
  }
  /* a bumper bounces a falling stickman up at 1.2 times its speed (at most 3), and plays its bounce */
  sh_new(&g, 0, &c);
  const sh_bumper *b0 = &sh_bumpers[sh_parts[0].bumper];
  const sh_bumper_kind *kd = &sh_bumper_kinds[b0->kind];
  bool tested = false;
  for (int i = 0; i < sh_parts[0].bumpers && !tested; i++) {
    const sh_bumper *b = &sh_bumpers[sh_parts[0].bumper + i];
    kd = &sh_bumper_kinds[b->kind];
    if (kd->angle != 0 || kd->force || kd->bounce != 1.2f) continue;
    sh_new(&g, 0, &c);
    g.x = b->x, g.y = b->y - 0.1f, g.vy = 0.5f, g.near = -1;
    for (int s = 0; s < 30 && g.vy > 0; s++) sh_step(&g, false, &r);
    float hit = sqrtf(0.25f + 2 * (0.1f - 0.02f - SH_RADIUS));   /* the speed on touching it */
    CHECK(g.vy < 0 && near(-g.vy, 1.2f * hit, 0.02f) && g.bumper_anim[i], "bumper: %f, not %f", g.vy, -1.2f * hit);
    tested = true;
  }
  CHECK(tested, "no plain bumper on level 1");
  /* a death: back at the start, the attempt counted, the camera's lead kept */
  sh_new(&g, 0, &c);
  g.cam_offset = 0.07f, g.x = 0.5f, g.y = 1.2f, g.vy = 0.1f;
  sh_step(&g, false, &r);
  CHECK(g.attempt == 2 && g.x == g.spawn_x && g.y == g.spawn_y && g.cam_offset == 0.07f && g.progress == 0, "death");
  /* the finish: slow motion, no gravity, a zoom to 4 in 0.6 s, the victory 2 s after */
  sh_new(&g, 0, &c);
  g.x = g.finish - 0.001f, g.vx = 1, g.vy = 0.2f;
  sh_step(&g, false, &r);
  CHECK(g.completed && g.pose == SH_VICTORY && g.progress == 1 && g.trail_max == 60, "finish");
  float vy1 = g.vy;
  int ticks = 1;
  while (!g.ready && ticks < 500) sh_step(&g, false, &r), ticks++;
  CHECK(ticks == 120, "victory after %d steps", ticks);
  CHECK(g.vy == vy1 && near(g.zoom, 4, 0.01f), "after the finish: vy %f, zoom %f", g.vy, g.zoom);
  CHECK(near(g.cam_y, -g.y * g.zoom + 0.5f, 1e-5f), "the camera on the stickman");
  printf("rules: courses, goals, skins, falling, hooking, bumpers, deaths and the finish as in the spec\n");
}

/* ---------------------------------------------------------------- the bot's replays, and the steps for the oracle */
__attribute__((unused)) static void replays_check(const char *dir) {
  char path[512];
  snprintf(path, sizeof path, "%s/cases.json", dir);
  FILE *f = fopen(path, "w");
  if (!f) exit(2);
  /* the courses' sections, as the web game's level files say them (from levels.c) */
  fprintf(f, "{\"parts\":[");
  for (int p = 0; p < SH_PARTS; p++) {
    const sh_part *q = &sh_parts[p];
    fprintf(f, "%s{\"hooks\":[", p ? "," : "");
    for (int i = 0; i < q->hooks; i++) fprintf(f, "%s[%.9g,%.9g]", i ? "," : "", sh_hooks[q->hook + i].x, sh_hooks[q->hook + i].y);
    fprintf(f, "],\"bumpers\":[");
    for (int i = 0; i < q->bumpers; i++) {
      const sh_bumper *b = &sh_bumpers[q->bumper + i];
      const sh_bumper_kind *k = &sh_bumper_kinds[b->kind];
      fprintf(f, "%s[%.9g,%.9g,%.9g,%.9g,%.9g,%.9g,%d]", i ? "," : "", b->x, b->y, k->angle, k->bounce, k->force, k->aim, k->aimed);
    }
    fprintf(f, "],\"walls\":[");
    for (int i = 0; i < q->walls; i++) {
      const sh_wall *o = &sh_walls[q->wall + i];
      fprintf(f, "%s[%d,%.9g,%.9g,%.9g]", i ? "," : "", o->shape, o->x, o->y, sh_turn_angles[o->turn]);
    }
    fprintf(f, "]}");
  }
  fprintf(f, "],\"cases\":[");
  int cases = 0, done = 0;
  uint32_t r = 3;
  for (int i = 0; i < nreplays; i++) {
    const replay *p = &replays[i];
    sh_game g;
    sh_new(&g, p->level, &p->c);
    for (int d = 0; d < p->n && !g.completed; d++)
      for (int t = 0; t < STRIDE && !g.completed; t++) {
        sh_game before = g;
        bool held = p->path[d] != 0;
        sh_step(&g, held, &r);
        CHECK(sh_valid(&g), "replay %d: a broken state", i);
        if (g.attempt != before.attempt || (held && !before.held && before.near < 0)) continue;
        const sh_game *s[2] = {&before, &g};
        fprintf(f, "%s{\"held\":%d", cases++ ? "," : "", held);
        for (int k = 0; k < 2; k++) {
          const sh_game *q = s[k];
          fprintf(f, ",\"%s\":{\"x\":%.9g,\"y\":%.9g,\"vx\":%.9g,\"vy\":%.9g,\"theta\":%.9g,\"omega\":%.9g,\"r\":%.9g,"
                     "\"pull\":%.9g,\"rotation\":%.9g,\"spin\":%.9g,\"hooked\":%d,\"hx\":%.9g,\"hy\":%.9g,\"held\":%d,"
                     "\"completed\":%d,\"finish\":%.9g,\"sections\":[",
                  k ? "after" : "before", q->x, q->y, q->vx, q->vy, q->theta, q->omega, q->r, q->pull, q->rotation, q->spin,
                  q->hooked, q->hook_x, q->hook_y, q->held, q->completed, q->finish);
          for (int j = 0; j < q->sections; j++) fprintf(f, "%s[%d,%.9g]", j ? "," : "", q->course.part[j], q->offset[j]);
          fprintf(f, "]}");
        }
        fprintf(f, "}");
      }
    CHECK(g.completed && g.attempt == 1, "replay of level %u: not through again", p->level + 1);
    done += g.completed;
  }
  fprintf(f, "]}\n");
  fclose(f);
  printf("replays: %d of %d through again, %d steps written for the oracle\n", done, nreplays, cases);
}

/* ---------------------------------------------------------------- the app, through its screens */
static const replay *driving;   /* a level played by its replay, as keys */
typedef struct { uint8_t screen; uint32_t keys; } step;   /* on that screen, press those keys */
static const step *steps;
static int nsteps, at_step;
static uint32_t since, gap;
static uint32_t level_keys(void) {
  if (view.screen != S_PLAY || !driving || game.level != driving->level || game.completed) return 0;
  uint32_t d = game.ticks / STRIDE;
  return d < driving->n && driving->path[d] ? SK_OK : 0;
}
static uint32_t step_keys(uint32_t f) {
  if (at_step < nsteps && view.screen == steps[at_step].screen && f > 2) {
    if (++since >= 3) {
      since = 0;
      return steps[at_step++].keys;
    }
  } else {
    since = 0;
  }
  if (at_step == nsteps && gap++ > 4) return SK_HOME;   /* all done: Home */
  return level_keys();
}
/* the steps, in order; true if all were done before Home */
static bool session(const step *s, int n, const replay *r, uint32_t frames) {
  steps = s, nsteps = n, at_step = 0, since = 0, gap = 0, driving = r;
  script = step_keys, frame_no = 0, quit_at = frames, quit_key = SK_HOME;
  memset(&view, 0, sizeof view);
  sh_app_main();
  return at_step == n;
}
static sh_profile saved(void) {
  sh_profile p;
  save_load(&p);
  return p;
}
static const replay *find_replay(uint32_t level) {
  for (int i = 0; i < nreplays; i++)
    if (replays[i].level == level) return &replays[i];
  return 0;
}

__attribute__((unused)) static void flows(void) {
  forget(SH_SAVE_NAME), forget(SH_COPY_NAME);
  /* first run: the title, OK: how to play, OK: level 1, played through to the victory; Home */
  {
    step s[] = {{S_TITLE, SK_OK}, {S_HELP, SK_OK}, {S_VICTORY, 0}};
    CHECK(session(s, 3, find_replay(0), 2000), "first run: stuck at step %d (screen %d)", at_step, view.screen);
  }
  sh_profile p = saved();
  CHECK(p.done == 1 && p.current == 1 && p.guide_seen && !p.has_course, "level 1 finished: done %u", p.done);
  /* next time: Play is level 2; Back pauses, Restart, Back, Levels (1 and 2), Back, Quit game: No, then Yes */
  {
    step s[] = {{S_TITLE, SK_OK},   {S_PLAY, SK_BACK},  {S_PAUSE, SK_DOWN}, {S_PAUSE, SK_OK},   {S_PLAY, SK_BACK},
                {S_PAUSE, SK_DOWN}, {S_PAUSE, SK_DOWN}, {S_PAUSE, SK_OK},   {S_LEVELS, SK_LEFT}, {S_LEVELS, SK_BACK},
                {S_PAUSE, SK_DOWN}, {S_PAUSE, SK_OK},   {S_HELP, SK_BACK},  {S_PAUSE, SK_DOWN}, {S_PAUSE, SK_OK},
                {S_QUIT, SK_OK},    {S_PAUSE, SK_OK},   {S_QUIT, SK_RIGHT}, {S_QUIT, SK_OK}};
    int n = sizeof s / sizeof s[0];
    session(s, n, 0, 2000);
    CHECK(at_step == n && frame_no < 200, "pause menu: stuck at step %d (screen %d), frame %u", at_step, view.screen, frame_no);
  }
  p = saved();
  CHECK(p.done == 1 && p.current == 1 && p.has_course, "a level left: kept %u %u %u", p.done, p.current, p.has_course);
  /* Levels: level 1 again, through; the victory's Next is level 2 */
  {
    step s[] = {{S_TITLE, SK_OK}, {S_PLAY, SK_BACK}, {S_PAUSE, SK_DOWN}, {S_PAUSE, SK_DOWN}, {S_PAUSE, SK_OK},
                {S_LEVELS, SK_LEFT}, {S_LEVELS, SK_OK}, {S_VICTORY, SK_OK}, {S_PLAY, 0}};
    CHECK(session(s, sizeof s / sizeof s[0], find_replay(0), 3000), "a level replayed: stuck at step %d", at_step);
    CHECK(game.level == 1, "Next after a replay: level %u", game.level + 1);
  }
  p = saved();
  CHECK(p.done == 1 && p.current == 1, "a replay doesn't count twice: done %u", p.done);
  /* reaching level 5 opens Plant: done 3, level 4 through, the popup, Equip now, the shop, OK (equipped): level 5 */
  sh_profile q = {3, 3, 0, 1, 0, 0, {{0}, 0}};
  save_write(&q);
  {
    step s[] = {{S_TITLE, SK_OK}, {S_UNLOCKED, SK_OK}, {S_SKINS, SK_OK}, {S_PLAY, 0}};
    CHECK(session(s, 4, find_replay(3), 3000), "a skin opened: stuck at step %d (screen %d)", at_step, view.screen);
  }
  p = saved();
  CHECK(p.done == 4 && p.skin == 2 && game.level == 4, "Plant equipped after level 4: done %u, skin %u", p.done, p.skin);
  /* the shop from the title: Mummy (locked) can't be chosen; Cactus, below it, can; Back to the title */
  {
    step s[] = {{S_TITLE, SK_RIGHT}, {S_TITLE, SK_OK}, {S_SKINS, SK_RIGHT}, {S_SKINS, SK_OK}, {S_SKINS, SK_DOWN},
                {S_SKINS, SK_OK}, {S_SKINS, SK_BACK}, {S_TITLE, 0}};
    CHECK(session(s, sizeof s / sizeof s[0], 0, 400), "the shop: stuck at step %d", at_step);
  }
  p = saved();
  CHECK(p.skin == 11, "skins: Mummy refused, Cactus chosen: skin %u", p.skin);
  /* far on: a random level; after Home, the same course again */
  q = (sh_profile){120, 120, 5, 1, 0, 0, {{0}, 0}};
  save_write(&q);
  {
    step s[] = {{S_TITLE, SK_OK}, {S_PLAY, 0}};
    session(s, 2, 0, 400);
  }
  p = saved();
  CHECK(p.current == 120 && p.has_course && sh_course_ok(120, &p.course), "level 121's course kept");
  sh_course kept = p.course;
  {
    step s[] = {{S_TITLE, SK_OK}, {S_PLAY, 0}};
    session(s, 2, 0, 400);
  }
  CHECK(!memcmp(&game.course, &kept, sizeof kept), "the same course after coming back");
  printf("screens: first run, levels through to the victory, pause, Restart, Levels, How to play, quitting, a skin "
         "opened and equipped, the shop, a random level kept\n");
}

/* ---------------------------------------------------------------- saves */
__attribute__((unused)) static void saves(void) {
  sh_profile p = {57, 41, 6, 1, 1, 0, {{0}, 0}}, q;
  uint32_t r = 5;
  sh_pick(41, &r, &p.course);
  forget(SH_SAVE_NAME), forget(SH_COPY_NAME);
  CHECK(save_write(&p), "save failed");
  memset(&q, 0xAB, sizeof q);
  save_load(&q);
  CHECK(!memcmp(&p, &q, sizeof p), "the save read back");
  /* the file gone: the copy in the script brings it back */
  forget(SH_SAVE_NAME);
  memset(&q, 0, sizeof q);
  save_load(&q);
  CHECK(!memcmp(&p, &q, sizeof p), "the copy read back");
  uint32_t n;
  CHECK(plat_load(SH_SAVE_NAME, &n) != 0, "the save put back from the copy");
  /* every byte damaged in turn, or the file cut short: refused, a fresh start */
  const uint8_t *d = plat_load(SH_SAVE_NAME, &n);
  uint8_t keep[64];
  memcpy(keep, d, n);
  int refused = 0;
  for (uint32_t i = 0; i < n; i++)
    for (int bit = 0; bit < 8; bit++) {
      uint8_t bad[64];
      memcpy(bad, keep, n);
      bad[i] ^= (uint8_t)(1 << bit);
      plat_save(SH_SAVE_NAME, bad, n);
      forget(SH_COPY_NAME);
      memset(&q, 0xAB, sizeof q);
      save_load(&q);
      refused += q.done == 0 && q.current == 0 && q.skin == 0 && !q.has_course;
    }
  CHECK(refused == (int)n * 8, "damaged saves refused: %d of %u", refused, n * 8);
  plat_save(SH_SAVE_NAME, keep, n - 1), save_load(&q);
  CHECK(q.done == 0, "short save");
  /* well made but impossible: a locked skin, a course from elsewhere */
  sh_profile bad = p;
  bad.skin = 9;
  save_write(&bad), save_load(&q);
  CHECK(q.done == 0, "a locked skin accepted");
  bad = p, bad.course.part[0] = 3;
  save_write(&bad), save_load(&q);
  CHECK(q.done == 0, "a wrong course accepted");
  /* and the app, on a damaged save: a fresh start, nothing broken */
  plat_save(SH_SAVE_NAME, "garbage", 7), forget(SH_COPY_NAME);
  {
    step k[] = {{S_TITLE, SK_OK}, {S_HELP, SK_OK}, {S_PLAY, 0}};
    session(k, 3, 0, 100);
  }
  save_load(&q);
  CHECK(q.done == 0 && q.guide_seen && q.has_course && q.current == 0, "the app after a damaged save");
  printf("saves: %u bytes, read back, restored from the copy, every damaged bit refused, the app starts afresh\n", n);
}

/* ---------------------------------------------------------------- random keys */
static uint32_t fuzz_rng = 77;
static uint32_t fuzz_keys(uint32_t f) {
  static uint32_t held, until;
  if (f == 1) until = 0;
  if (f >= until) {
    uint32_t x = sh_rand(&fuzz_rng);
    static const uint32_t pick[] = {0, SK_OK, SK_OK, SK_OK, SK_UP, SK_DOWN, SK_LEFT, SK_RIGHT, SK_BACK, SK_OK, 0, SK_OK};
    held = pick[x % 12];
    until = f + 1 + (x >> 8) % 30;
  }
  return held;
}
__attribute__((unused)) static void fuzz(void) {
  int sessions = 300;
  uint32_t screens = 0;
  for (int s = 0; s < sessions; s++) {
    if (s % 40 == 0) forget(SH_SAVE_NAME), forget(SH_COPY_NAME);
    if (s % 40 == 20) {   /* far on, with skins open */
      sh_profile p = {(uint32_t)(s * 3), (uint32_t)(s * 3), 0, 1, 0, 0, {{0}, 0}};
      save_write(&p);
    }
    memset(&view, 0, sizeof view);
    fuzz_rng = (uint32_t)s * 7919u + 1;
    script = fuzz_keys, frame_no = 0, quit_at = 100 + sh_rand(&fuzz_rng) % 1500, quit_key = SK_HOME;
    sh_app_main();
    screens |= 1u << view.screen;
    CHECK(view.screen != S_PLAY || sh_valid(&game), "session %d: a broken level", s);
    uint32_t n;
    const uint8_t *d = plat_load(SH_SAVE_NAME, &n);
    CHECK(d && n == sizeof(sh_file), "session %d: no save", s);
    if (d) {
      sh_file f;
      memcpy(&f, d, sizeof f);
      CHECK(sh_file_ok(&f), "session %d: the save doesn't read back", s);
    }
  }
  printf("fuzz: %d sessions of random keys, each quit with Home and its save read back (screens seen: %x)\n", sessions,
         screens);
}

/* ---------------------------------------------------------------- pictures */
static void draw_now(void) {
  sync_view(view.clock);
  sh_redraw();
  sh_draw(&view, plat_push);
}
__attribute__((unused)) static void shots(const char *dir) {
  memset(&prof, 0, sizeof prof);
  prof.done = 7, prof.current = 7, prof.guide_seen = 1;
  view.clock = 1300;
  title();
  for (int i = 0; i < 40; i++) sh_swing_step(&swing, 1.0f / 60);
  draw_now(), shot(dir, "title");
  view.sel = 1, draw_now(), shot(dir, "title_skins");
  uint32_t r = 11;
  /* level 1, swinging; a level made by hand; a random course; the finish */
  const replay *p = find_replay(0);
  sh_course c;
  sh_pick(0, &r, &c);
  sh_new(&game, 0, &c);
  view.screen = S_PLAY;
  for (int d = 0; p && d < p->n; d++) {
    for (int t = 0; t < STRIDE; t++) sh_step(&game, p->path[d], &r);
    if (game.hooked && game.ticks > 50) break;
  }
  draw_now(), shot(dir, "play");
  struct { uint32_t level; const char *name; int frac; } scenes[] = {
      {3, "level4", 20}, {6, "walls", 40}, {8, "level9", 35}, {12, "level13", 30}, {20, "level21", 50}, {27, "level28", 40},
      {SH_FIXED, "random", 45}, {SH_FIXED + 7, "random2", 30}, {SH_FIXED + 14, "random3", 55}};
  for (unsigned s = 0; s < sizeof scenes / sizeof scenes[0]; s++) {
    p = find_replay(scenes[s].level);
    if (!p) continue;
    sh_new(&game, p->level, &p->c);
    /* past that part of the replay, the first moment the stickman swings on a long rope */
    int stop = p->n * scenes[s].frac / 100 * STRIDE;
    for (int t = 0; t < p->n * STRIDE && !game.completed; t++) {
      if (t > stop && game.hooked && game.r > 0.12f && game.y < 0.85f && 240 * (game.x + game.cam_x) > 60) break;
      sh_step(&game, p->path[t / STRIDE], &r);
    }
    draw_now(), shot(dir, scenes[s].name);
  }
  view.screen = S_PAUSE, view.sel = 0, draw_now(), shot(dir, "pause");
  view.screen = S_QUIT, view.sel = 0, draw_now(), shot(dir, "quit");
  /* the finish, a third of a second in, then the victory */
  p = find_replay(2);
  sh_new(&game, p->level, &p->c);
  view.screen = S_PLAY;
  for (int d = 0; d < p->n && !game.completed; d++)
    for (int t = 0; t < STRIDE && !game.completed; t++) sh_step(&game, p->path[d], &r);
  for (int i = 0; i < 40; i++) sh_step(&game, false, &r);
  draw_now(), shot(dir, "finish");
  prof.current = 3;
  view.screen = S_VICTORY, view.sel = 0, draw_now(), shot(dir, "victory");
  view.screen = S_UNLOCKED, view.new_skin = 2, draw_now(), shot(dir, "unlocked");
  view.screen = S_SKINS, view.sel = 4, prof.skin = 0, prof.done = 12, prof.current = 12, draw_now(), shot(dir, "skins");
  prof.done = 37, view.screen = S_LEVELS, view.level_top = 0, view.level_sel = 21, draw_now(), shot(dir, "levels");
  view.screen = S_HELP, draw_now(), shot(dir, "help");
}

/* frames of the bot playing levels in a row, as the app shows them (20 a second: every third step), the finish
 * and the victory between them: DIR/f_NNNN.ppm */
static void film(const char *dir, const char *levels, int frames) {
  memset(&prof, 0, sizeof prof);
  prof.guide_seen = 1;
  uint32_t r = 5;
  int f = 0;
  char path[64];
  for (const char *l = levels; *l && f < frames;) {
    uint32_t level = (uint32_t)strtoul(l, (char **)&l, 10);
    if (*l == ',') l++;
    const replay *p = find_replay(level);
    if (!p) continue;
    prof.current = prof.done = level;
    sh_new(&game, level, &p->c);
    view.screen = S_PLAY;
    for (int t = 0; !game.ready && f < frames; t++) {
      sh_step(&game, t / STRIDE < p->n && p->path[t / STRIDE], &r);
      if (t % 3 == 2) view.clock = (uint32_t)f * 50, draw_now(), snprintf(path, sizeof path, "f_%04d", f++), shot(dir, path);
    }
    prof.current = prof.done = level + 1;
    view.screen = S_VICTORY, view.sel = 0;
    for (int k = 0; k < 30 && f < frames; k++)
      view.clock = (uint32_t)f * 50, draw_now(), snprintf(path, sizeof path, "f_%04d", f++), shot(dir, path);
  }
}

int main(int argc, char **argv) {
  if (argc >= 3 && !strcmp(argv[1], "bot")) return bot(argv[2]);
#ifndef BOT_ONLY
  if (argc >= 3 && !strcmp(argv[1], "test")) {
    clock_t t0 = clock();
    units();
    if (!load_replays(argv[2])) {
      printf("FAIL no %s/replays.bin: run the bot first\n", argv[2]);
      return 1;
    }
    replays_check(argv[2]);
    flows();
    saves();
    fuzz();
    extern int sh_clipped;
    CHECK(!sh_clipped, "text cut off %d times", sh_clipped);
    fprintf(stderr, "(%.0f s)\n", (double)(clock() - t0) / CLOCKS_PER_SEC);
    printf(fails ? "%d FAILED\n" : "all passed\n", fails);
    return fails != 0;
  }
  if (argc >= 5 && !strcmp(argv[1], "film")) {
    if (!load_replays("build")) return 2;
    film(argv[2], argv[3], atoi(argv[4]));
    return 0;
  }
  if (argc >= 3 && !strcmp(argv[1], "shots")) {
    if (!load_replays(argc >= 4 ? argv[3] : "build")) return 2;
    shots(argv[2]);
    extern int sh_clipped;
    CHECK(!sh_clipped, "text cut off %d times", sh_clipped);
    return fails != 0;
  }
#endif
  fprintf(stderr, "usage: play bot DIR | test DIR | shots DIR [REPLAYS_DIR] | film DIR LEVELS N\n");
  return 2;
}
