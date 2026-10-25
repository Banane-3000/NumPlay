/* Angry Birds on a computer: the tests (make test).
 *
 *   play test DIR          everything below, quickly (the bot on some levels), pictures in DIR
 *   play levels [kb]       every level loads, settles and takes shots without a fault, in kb of arena
 *   play bot [first last every]  the bot plays levels (each shot the best of many it tries)
 *   play fuzz [n]          random keys through the whole game, n sessions
 *   play saves             a save comes back as it was; a damaged one starts afresh
 *   play same              the same level played twice gives the very same world (no randomness leaks in)
 *   play shots DIR         pictures of the screens
 *   play level N [angle]   one level, with what happens
 */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "draw.h"
#include "game.h"
#include "plat.h"
#include "save.h"
#include "ui.h"
#include "world.h"

#define ARENA_BYTES (512 * 1024)
static uint8_t arena_mem[ARENA_BYTES] __attribute__((aligned(8)));

/* ------------------------------------------------------------------ the platform, simulated */
static uint32_t keys_now, clock_ms;
static uint16_t screen[240][320];
static struct { char name[32]; uint8_t data[8192]; uint32_t len; bool used; } files[4];
static bool storage_full;

uint32_t plat_keys(void) { return keys_now; }
uint32_t plat_millis(void) { return clock_ms; }
void plat_sleep(uint32_t ms) { clock_ms += ms; }
void plat_push(int x, int y, int w, int h, const uint16_t *px) {
  for (int j = 0; j < h; j++)
    for (int i = 0; i < w; i++)
      if (y + j >= 0 && y + j < 240 && x + i >= 0 && x + i < 320) screen[y + j][x + i] = px[j * w + i];
}
void np_push(int x, int y, int w, int h, const color_t *px) { plat_push(x, y, w, h, px); }
void *np_alloc(uint32_t size) { (void)size; return NULL; }   /* (gfx_init: not used) */
bool plat_save(const char *name, const void *data, uint32_t len) {
  if (storage_full || len > sizeof files[0].data) return false;
  int k = 0;
  while (k < 4 && files[k].used && strcmp(files[k].name, name)) k++;
  if (k == 4) return false;
  snprintf(files[k].name, sizeof files[k].name, "%s", name);
  memcpy(files[k].data, data, len);
  files[k].len = len, files[k].used = true;
  return true;
}
const uint8_t *plat_load(const char *name, uint32_t *len) {
  for (int k = 0; k < 4; k++)
    if (files[k].used && !strcmp(files[k].name, name)) {
      *len = files[k].len;
      return files[k].data;
    }
  return NULL;
}
void *plat_heap(int *size) {
  *size = ARENA_BYTES;
  return arena_mem;
}
bool plat_slow(void) { return false; }
void plat_begin(void) {}
int plat_end(void) { return 0; }

static void save_ppm(const char *path) {
  FILE *f = fopen(path, "wb");
  if (!f) return;
  fprintf(f, "P6\n320 240\n255\n");
  for (int y = 0; y < 240; y++)
    for (int x = 0; x < 320; x++) {
      uint16_t c = screen[y][x];
      uint8_t rgb[3] = {(uint8_t)((c >> 11) * 255 / 31), (uint8_t)(((c >> 5) & 63) * 255 / 63),
                        (uint8_t)((c & 31) * 255 / 31)};
      fwrite(rgb, 1, 3, f);
    }
  fclose(f);
}

static int failures;
#define CHECK(c, ...)                                                                \
  do {                                                                               \
    if (!(c)) {                                                                      \
      printf("FAIL %s:%d: ", __FILE__, __LINE__), printf(__VA_ARGS__), printf("\n"); \
      failures++;                                                                    \
    }                                                                                \
  } while (0)

static bool finite_world(void) {
  for (int i = 0; i < g.count; i++) {
    if (!(g.obj[i].flags & O_ALIVE) || (g.obj[i].flags & O_SCENERY)) continue;
    WBody b;
    world_get(i, &b);
    if (!isfinite(b.x) || !isfinite(b.y) || !isfinite(b.angle) || !isfinite(b.vx) || !isfinite(b.vy) || !isfinite(b.w))
      return false;
  }
  return true;
}

/* ------------------------------------------------------------------ every level */
static int levels(int arena, bool quiet) {
  int bad = 0, worst = 0, worst_level = 0;
  for (int l = 0; l < LEVEL_COUNT; l++) {
    if (!game_load(l, arena_mem, arena)) {
      printf("level %d: does not load (%d bytes, %d failures)\n", l, world_arena_peak(), world_arena_failures());
      bad++;
      continue;
    }
    int built = world_arena_used();
    /* settles, then every bird is fired as soon as it is ready, its power used */
    int launched = 0;
    for (int s = 0; s < 3000 && g.state != ST_WON && g.state != ST_LOST; s++) {
      if (game_can_launch()) {
        game_aim(0.25f + 0.15f * (float)(launched % 4), 1.0f);
        launched += game_launch();
      }
      if (s % 37 == 30) game_power();
      game_step();
    }
    bool ok = finite_world() && !world_arena_failures() && (g.state == ST_WON || g.state == ST_LOST);
    if (!ok) {
      printf("level %d: %s%s, %d failures\n", l, finite_world() ? "" : "not finite ",
             g.state == ST_WON || g.state == ST_LOST ? "" : "never ends", world_arena_failures());
      bad++;
    }
    if (world_arena_peak() > worst) worst = world_arena_peak(), worst_level = l;
    if (!quiet)
      printf("%3d objects %3d built %6d peak %6d contacts %4d score %7u pigs %d state %d\n", l, g.count, built,
             world_arena_peak(), world_contacts(), (unsigned)g.score, g.pigs, g.state);
  }
  printf("levels: %d, worst peak %d bytes (level %d), %d bad\n", LEVEL_COUNT, worst, worst_level, bad);
  return bad;
}

/* ------------------------------------------------------------------ the bot */
static const char *record_dir;
static int record_n;
static View vw;
static void scene_world(void *ctx);
static void record_step(void) {
  game_step();
  if (!record_dir || g.steps % (getenv("EVERY") ? atoi(getenv("EVERY")) : 15)) return;
  gfx_buf = (uint16_t *)malloc(320 * 16 * 2);
  float x0 = fminf(g.cam[0], g.cam[4]), x1 = fmaxf(g.cam[2], g.cam[6]), y0 = fminf(g.cam[1], g.cam[5]), y1 = fmaxf(g.cam[3], g.cam[7]);
  if (getenv("CASTLE")) x0 = g.cam[4], x1 = g.cam[6], y0 = g.cam[5], y1 = g.cam[7];
  vw.zoom = fminf(320 / (x1 - x0), 240 / (y1 - y0));
  vw.cx = (x0 + x1) / 2, vw.cy = (y0 + y1) / 2;
  gfx_render(scene_world, NULL, 0, 240);
  free(gfx_buf);
  char path[256];
  snprintf(path, sizeof path, "%s/f%05d.ppm", record_dir, record_n++);
  save_ppm(path);
}
static uint8_t *snap;
static Game gsnap;
static void keep(void) {
  world_snapshot_save(snap, arena_mem, ARENA_BYTES);
  gsnap = g;
}
static void back(void) {
  world_snapshot_load(snap, arena_mem, ARENA_BYTES);
  g = gsnap;
}

static bool shot_over(int s) {
  return s > 45 && g.flying < 0 && g.follow < 0 && g.eagle < 0 && (game_can_launch() || g.out_of_birds) &&
         g.settle_ms <= 0;
}

/* the horizontal distance from the flying bird to the nearest pig ahead of it (1e9: none) */
static float pig_ahead(void) {
  if (g.flying < 0) return 1e9f;
  float bx, by, best = 1e9f;
  game_object_pos(g.flying, &bx, &by, NULL);
  for (int i = 0; i < g.count; i++) {
    if (!(g.obj[i].flags & O_ALIVE) || g.obj[i].cls != C_PIG) continue;
    float x, y;
    game_object_pos(i, &x, &y, NULL);
    if (x - bx > -1 && x - bx < best) best = x - bx;
  }
  return best;
}

/* a shot tried from the kept state: how good it is. Its power: after power_at steps, or (power_at < -1) when the
   bird comes within near meters of a pig */
static long try_shot(float angle, float power, int power_at, float near) {
  back();
  uint32_t score0 = g.score;
  int pigs0 = g.pigs;
  game_aim(angle, power);
  if (!game_launch()) return -1000000000L;
  for (int s = 0; s < 400; s++) {
    if (s == power_at || (power_at < -1 && g.flying >= 0 && pig_ahead() <= near)) game_power();
    game_step();
    if (g.state != ST_PLAY || shot_over(s)) break;
  }
  return (long)(pigs0 - g.pigs) * 1000000L + (long)(g.score - score0) + (g.state == ST_WON ? 100000000L : 0);
}

typedef struct { long v; float a, p; int at; float near; } Shot;
static void keep_best(Shot *top, int n, Shot s) {
  int k = n;
  while (k > 0 && s.v > top[k - 1].v) k--;
  if (k == n) return;
  for (int j = n - 1; j > k; j--) top[j] = top[j - 1];
  top[k] = s;
}

static bool bot_level(int l, int *score) {
  if (!game_load(l, arena_mem, ARENA_BYTES)) return false;
  for (int guard = 0; guard < 6000 && g.state == ST_PLAY; guard++) {
    if (!game_can_launch() || g.flying >= 0) {
      game_step();
      continue;
    }
    keep();
    int kind = g.obj[g.loaded].cls;
    bool has_power = kind != C_RED && kind != C_REDBIG && kind != C_SARDINE;
    /* the power when near a pig: the white bird drops its egg over it, the black bird goes off by it, the blue
       bird splits and the yellow bird speeds up a little before it */
    float near = kind == C_WHITE ? 0.3f : kind == C_BLACK ? 1.0f : kind == C_BLUE ? 5.0f : 9.0f;
    bool by_pig = has_power && kind != C_GREEN;
    Shot top[4], none = {-2000000000L, 0.5f, 1, -1, 0};
    for (int k = 0; k < 4; k++) top[k] = none;
    /* every 2 degrees at two pulls, with the power by a pig too; then the best four again a degree either side
       and a little more or less pulled; then the best two with the power at different times */
    for (int deg = -12; deg <= 78; deg += 2)
      for (int pw = 0; pw < 2; pw++)
        for (int m = 0; m < (by_pig ? 2 : 1); m++) {
          Shot s = {0, (float)deg * PI_F / 180, pw ? 0.8f : 1.0f, m ? -2 : -1, near};
          s.v = try_shot(s.a, s.p, s.at, s.near);
          keep_best(top, 4, s);
        }
    Shot best[2] = {top[0], top[1]};
    for (int c = 0; c < 4; c++)
      for (int d = -1; d <= 1; d++)
        for (int q = -1; q <= 1; q++) {
          Shot s = top[c];
          s.a += (float)d * (PI_F / 180), s.p += (float)q * 0.05f;
          if (s.p > 1 || (d == 0 && q == 0)) continue;
          s.v = try_shot(s.a, s.p, s.at, s.near);
          keep_best(best, 2, s);
        }
    Shot pick = best[0];
    if (has_power)
      for (int c = 0; c < 2; c++)
        for (int t = 4; t <= 60; t += 4) {
          Shot s = best[c];
          s.at = t;
          s.v = try_shot(s.a, s.p, s.at, s.near);
          if (s.v > pick.v) pick = s;
        }
    back();
    game_aim(pick.a, pick.p);
    game_launch();
    if (record_dir) printf("shot: %.1f deg, %.0f%%, power at %d, expect %ld\n", pick.a * 180 / PI_F, pick.p * 100, pick.at, pick.v);
    for (int s = 0; s < 400 && g.state == ST_PLAY; s++) {
      if (s == pick.at || (pick.at < -1 && g.flying >= 0 && pig_ahead() <= pick.near)) game_power();
      record_step();
      if (shot_over(s)) break;
    }
  }
  for (int s = 0; s < 3000 && g.state != ST_WON && g.state != ST_LOST; s++) game_step();
  *score = (int)g.score;
  return g.state == ST_WON;
}

static int bot(int first, int last, int every, int *tried_out) {
  snap = malloc((size_t)world_snapshot_size(ARENA_BYTES));
  int won = 0, tried = 0, stars = 0;
  for (int l = first; l <= last && l < LEVEL_COUNT; l += every) {
    int score = 0;
    bool w = bot_level(l, &score);
    tried++;
    won += w;
    int st = w ? game_stars(l, (uint32_t)score) : 0;
    stars += st;
    printf("bot: level %3d (%s %d) %s, %d points, %d stars\n", l, episode_names[res_episode_of(l)],
           l - res_episode_first(res_episode_of(l)) + 1, w ? "cleared" : "failed", score, st);
    fflush(stdout);
  }
  printf("bot: cleared %d of %d levels (%d%%), %d stars\n", won, tried, tried ? 100 * won / tried : 0, stars);
  free(snap);
  if (tried_out) *tried_out = tried;
  return won;
}

/* ------------------------------------------------------------------ the whole game with random keys */
/* the screen's strip and the physics' step memory, one buffer (as main.c has it on the stack) */
static uint64_t work[WORK_BYTES / 8];
#define strip ((uint16_t *)work)
static int fuzz(int sessions, unsigned seed) {
  int problems = 0;
  for (int k = 0; k < sessions; k++) {
    memset(files, 0, sizeof files);
    clock_ms = 1000;
    ui_init(arena_mem, ARENA_BYTES, strip);
    uint32_t x = seed + (unsigned)k * 7919u;
    for (int f = 0; f < 6000; f++) {
      x ^= x << 13, x ^= x >> 17, x ^= x << 5;
      uint32_t r = x % 100;
      /* mostly OK and the arrows, Back now and then; Home rarely (it ends the session) */
      keys_now = r < 30   ? KB_OK
                 : r < 40 ? KB_UP
                 : r < 50 ? KB_DOWN
                 : r < 58 ? KB_LEFT
                 : r < 66 ? KB_RIGHT
                 : r < 70 ? KB_BACK
                 : r < 72 ? KB_SHIFT
                 : r < 73 ? KB_EAGLE
                          : 0;
      if (f % 997 == 996) keys_now = KB_HOME;
      clock_ms += 17;
      if (!ui_frame(keys_now, clock_ms)) break;
      if (ui_dirty() && f % 4 == 0) ui_draw();
      if (ui_screen() == S_PLAY && !finite_world()) {
        printf("fuzz %d: the world went wrong at frame %d\n", k, f);
        problems++;
        break;
      }
    }
  }
  printf("fuzz: %d sessions, %d problems\n", sessions, problems);
  return problems;
}

/* ------------------------------------------------------------------ saves */
static void saves(void) {
  memset(files, 0, sizeof files);
  Progress p, q, z;
  save_fresh(&p);
  save_fresh(&z);
  for (int l = 0; l < LEVEL_COUNT; l++) {
    p.best[l] = (uint32_t)l * 977u % 200000u;
    p.stars[l] = (uint8_t)(l % 4);
    p.eagle[l] = (uint8_t)(l % 101);
    p.flags[l] = (uint8_t)(l % 8);
  }
  p.last = 123, p.guide = 1, p.eagle_lock_on = 1, p.eagle_level = 77, p.eagle_since = 4242, p.played = 99999;
  CHECK(save_write(&p), "a save is written");
  CHECK(save_load(&q) && !memcmp(&p, &q, sizeof p), "the save comes back as it was");
  for (int k = 0; k < 4; k++)
    if (!strcmp(files[k].name, AB_SAVE_NAME)) files[k].used = false;
  memset(&q, 0, sizeof q);
  CHECK(save_load(&q) && !memcmp(&p, &q, sizeof p), "the copy brings it back when the save is gone");
  int n = 0;
  for (int k = 0; k < 4; k++) {
    if (strcmp(files[k].name, AB_SAVE_NAME)) continue;
    for (uint32_t i = 0; i < files[k].len; i += 5) {
      files[k].data[i] ^= 0x5A;
      CHECK(save_load(&q) && !memcmp(&p, &q, sizeof p), "a damaged save (byte %u) comes back from its copy", i);
      files[k].data[i] ^= 0x5A;
      n++;
    }
  }
  for (int k = 0; k < 4; k++) {
    if (!files[k].used) continue;
    files[k].data[files[k].len / 2] ^= 0xFF;
    files[k].len -= 3;
  }
  CHECK(!save_load(&q) && !memcmp(&q, &z, sizeof q), "a save and its copy both damaged start afresh");
  memset(files, 0, sizeof files);
  CHECK(!save_load(&q) && !memcmp(&q, &z, sizeof q), "no save: a fresh start");
  storage_full = true;
  CHECK(!save_write(&p), "a full storage says so");
  storage_full = false;
  printf("saves: round trip, the copy, %d damaged saves, both damaged, none, a full storage\n", n);
}

/* ------------------------------------------------------------------ the same twice */
static uint32_t world_hash(void) {
  uint32_t h = 2166136261u;
  for (int i = 0; i < g.count; i++) {
    WBody b;
    world_get(i, &b);
    uint32_t v[6];
    memcpy(v, &b, sizeof v);
    for (int k = 0; k < 6; k++) h = (h ^ v[k]) * 16777619u;
  }
  return (h ^ g.score) * 16777619u;
}
static void same(void) {
  int checked = 0;
  for (int l = 0; l < LEVEL_COUNT; l += 37) {
    uint32_t hs[2];
    for (int r = 0; r < 2; r++) {
      game_load(l, arena_mem, ARENA_BYTES);
      for (int s = 0; s < 700 && g.state == ST_PLAY; s++) {
        if (game_can_launch()) {
          game_aim(0.3f, 1.0f);
          game_launch();
        }
        if (s % 41 == 25) game_power();
        game_step();
      }
      hs[r] = world_hash();
    }
    CHECK(hs[0] == hs[1], "level %d plays the same twice", l);
    checked++;
  }
  printf("same: %d levels played twice alike\n", checked);
}

/* ------------------------------------------------------------------ pictures */
static void shots(const char *dir) {
  char path[256];
  memset(files, 0, sizeof files);
  clock_ms = 1000;
  ui_init(arena_mem, ARENA_BYTES, strip);
  static const struct { uint32_t keys; int frames; const char *name; } script[] = {
    {0, 10, "title"},     {KB_OK, 1, NULL},   {0, 10, "episodes"}, {KB_OK, 1, NULL}, {0, 10, "levels"},
    {KB_OK, 1, NULL},     {0, 200, "story"},  {KB_OK, 1, NULL},    {0, 10, "guide"}, {KB_OK, 1, NULL},
    {0, 200, "aim"},      {KB_DOWN, 1, NULL}, {0, 20, NULL},       {KB_OK, 1, NULL}, {0, 40, "flight"},
    {KB_BACK, 1, NULL},   {0, 10, "pause"},   {KB_BACK, 1, NULL},  {0, 900, "end"},
  };
  for (unsigned k = 0; k < sizeof script / sizeof *script; k++) {
    for (int f = 0; f < script[k].frames; f++) {
      keys_now = f == 0 ? script[k].keys : 0;
      clock_ms += 17;
      ui_frame(keys_now, clock_ms);
      ui_draw();
    }
    if (script[k].name) {
      snprintf(path, sizeof path, "%s/%s.ppm", dir, script[k].name);
      save_ppm(path);
    }
  }
}

/* a level's world at given steps, the castle's view (or one fitting the whole level) */
static void scene_world(void *ctx) {
  (void)ctx;
  draw_world(&vw);
}
static void view_level(int l, int argc, char **argv, const char *dir) {
  game_load(l, arena_mem, ARENA_BYTES);
  gfx_buf = strip;
  float x0 = g.cam[4], x1 = g.cam[6], y0 = g.cam[5], y1 = g.cam[7];
  vw.zoom = fminf(320 / (x1 - x0), 240 / (y1 - y0));
  vw.cx = (x0 + x1) / 2, vw.cy = (y0 + y1) / 2;
  int s = 0;
  for (int k = 0; k < argc; k++) {
    int until = atoi(argv[k]);
    while (s < until) game_step(), s++;
    gfx_render(scene_world, NULL, 0, 240);
    char path[256];
    snprintf(path, sizeof path, "%s/level%d_%05d.ppm", dir, l, s);
    save_ppm(path);
  }
}

int main(int argc, char **argv) {
  world_work(work, (int)sizeof work);
  save_scratch(work);
  if (argc > 2 && !strcmp(argv[1], "scan")) {   /* one shot at each angle: what it does */
    snap = malloc((size_t)world_snapshot_size(ARENA_BYTES));
    game_load(atoi(argv[2]), arena_mem, ARENA_BYTES);
    while (!game_can_launch()) game_step();
    keep();
    int d0 = argc > 3 ? atoi(argv[3]) : -10, d1 = argc > 4 ? atoi(argv[4]) : 70;
    for (int deg = d0; deg <= d1; deg += 2) {
      back();
      int pigs0 = g.pigs;
      game_aim((float)deg * PI_F / 180, 1);
      game_launch();
      record_dir = getenv("RECORD");
      for (int s = 0; s < 400 && g.state == ST_PLAY; s++) {
        record_step();
        if (shot_over(s)) break;
      }
      printf("%3d deg: pigs %d -> %d, %u points\n", deg, pigs0, g.pigs, (unsigned)g.score);
    }
    return 0;
  }
  if (argc > 3 && !strcmp(argv[1], "botview")) {
    record_dir = argv[2];
    snap = malloc((size_t)world_snapshot_size(ARENA_BYTES));
    int score = 0;
    bool w = bot_level(atoi(argv[3]), &score);
    printf("%s %d\n", w ? "cleared" : "failed", score);
    return 0;
  }
  if (argc > 3 && !strcmp(argv[1], "view")) return view_level(atoi(argv[3]), argc - 4, argv + 4, argv[2]), 0;
  if (argc > 1 && !strcmp(argv[1], "levels"))
    return levels(argc > 2 ? atoi(argv[2]) * 1024 : ARENA_BYTES, false) != 0;
  if (argc > 1 && !strcmp(argv[1], "bot")) {
    int first = argc > 2 ? atoi(argv[2]) : 0, last = argc > 3 ? atoi(argv[3]) : LEVEL_COUNT - 1;
    bot(first, last, argc > 4 ? atoi(argv[4]) : 1, NULL);
    return 0;
  }
  if (argc > 1 && !strcmp(argv[1], "fuzz")) return fuzz(argc > 2 ? atoi(argv[2]) : 100, 12345) != 0;
  if (argc > 1 && !strcmp(argv[1], "saves")) return saves(), failures != 0;
  if (argc > 1 && !strcmp(argv[1], "same")) return same(), failures != 0;
  if (argc > 2 && !strcmp(argv[1], "shots")) return shots(argv[2]), 0;
  if (argc > 2 && !strcmp(argv[1], "level")) {
    int l = atoi(argv[2]);
    if (!game_load(l, arena_mem, ARENA_BYTES)) return printf("no load\n"), 1;
    for (int s = 0; s < 3000 && g.state != ST_WON && g.state != ST_LOST; s++) {
      if (game_can_launch() && !game_in_flight() && g.flying < 0) {
        game_aim(argc > 3 ? (float)atof(argv[3]) : 0.19f, 1.0f);
        game_launch();
        printf("step %d: launch, score %u\n", s, (unsigned)g.score);
      }
      game_step();
      if (s % 100 == 0)
        printf("step %d: score %u pigs %d birds %d state %d peak %d\n", s, (unsigned)g.score, g.pigs, g.birds_left,
               g.state, world_arena_peak());
    }
    printf("end: score %u state %d\n", (unsigned)g.score, g.state);
    return 0;
  }
  if (argc > 2 && !strcmp(argv[1], "test")) {
    CHECK(levels(ARENA_BYTES, true) == 0, "every level loads, plays and ends");
    saves();
    same();
    CHECK(fuzz(60, 777) == 0, "random keys");
    int tried = 0, won = bot(0, LEVEL_COUNT - 1, 31, &tried);
    CHECK(won * 4 >= tried * 3, "the bot clears three levels in four it plays");
    shots(argv[2]);
    printf(failures ? "FAILED: %d\n" : "all tests passed\n", failures);
    return failures != 0;
  }
  printf("play test DIR | levels [kb] | bot [first last every] | fuzz [n] | saves | same | shots DIR | level N\n");
  return 2;
}
