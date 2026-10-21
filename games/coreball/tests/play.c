/* Core Ball on a computer, for tests and pictures (make host, make test).
 *
 *   play test          the 60 levels against the original (spec.h): pins, shots, the core's angle frame by
 *                      frame; the collision at every tenth of a degree; a shot's flight, two in one frame, the
 *                      clear with a ball still flying, the miss, the screens' timings; a bot clears all 60
 *                      levels; 300 random sessions through the menus and play (quitting and opening again)
 *                      never break a rule; saves come back, damaged ones reset
 *   play shot DIR      pictures (DIR/NAME.ppm): start, play, clear, miss, levels, menu, help, end */
#define NP_MODULE 1 /* (its drawing pushed to fb, its saves kept in disk, below) */
#define main cb_main
#include "../src/main.c"
#undef main
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "spec.h"

static uint16_t fb[SCREEN_H][SCREEN_W];
static uint8_t disk[64];
static uint32_t disk_len;
void np_push(int x, int y, int w, int h, const color_t *p) {
  for (int j = 0; j < h; j++)
    for (int i = 0; i < w; i++) fb[y + j][x + i] = p[j * w + i];
}
bool eadk_display_wait_for_vblank(void) { return true; }
uint64_t eadk_keyboard_scan(void) { return 0; }
uint64_t eadk_timing_millis(void) { return 0; }
void eadk_timing_msleep(uint32_t ms) { (void)ms; }
void *np_alloc(uint32_t size) { return malloc(size); }
const uint8_t *np_file_read(const char *name, uint32_t *len) { *len = disk_len; return disk_len ? disk : 0; }
bool np_file_write(const char *name, const void *d, uint32_t len) {
  if (len > sizeof disk) return false;
  memcpy(disk, d, disk_len = len);
  return true;
}

static int fails;
#define CHECK(c, ...) do { if (!(c)) { printf(__VA_ARGS__); printf("\n"); fails++; } } while (0)
static uint32_t seed = 1;
static uint32_t rnd(void) { return seed = seed * 1664525u + 1013904223u; }

static void fresh(int level) { /* a level from its first frame */
  memset(&g, 0, sizeof g);
  g.level = (uint8_t)level;
  begin();
}
static void ticks(int n) {
  while (n--) tick();
}

/* ---------------------------------------------------------------- the levels against the original */
static void levels_vs_spec(void) {
  for (int l = 1; l <= LAST; l++) {
    fresh(l);
    CHECK(g.n == spec[l - 1].pins && g.shots == spec[l - 1].shots, "level %d: %d pins %d shots", l, g.n, g.shots);
    for (int i = 0; i < g.n; i++)
      CHECK(g.ang[g.n - 1 - i] == spec[l - 1].preset[i] * 10, "level %d preset %d at %d", l, i, g.ang[g.n - 1 - i]);
    /* the core's turns: no balls, a click before every 137th frame */
    g.shots = 0;
    uint32_t h = 0x811C9DC5u;
    for (int f = 1; f <= SPEC_FRAMES; f++) {
      if (f % SPEC_TAP == 0) g.pending++;
      tick();
      h = (h ^ (g.angle & 255)) * 0x01000193u, h = (h ^ (uint32_t)(g.angle >> 8)) * 0x01000193u;
      if (f % SPEC_EVERY == 0)
        CHECK(g.angle == spec[l - 1].every[f / SPEC_EVERY - 1], "level %d frame %d: angle %d, the original %d", l, f,
              (int)g.angle, spec[l - 1].every[f / SPEC_EVERY - 1]);
    }
    CHECK(h == spec[l - 1].hash, "level %d: the core's turns differ from the original's", l);
  }
  /* a pin at every tenth of a degree, a ball landing under the core: a miss exactly as the original's
     floor(sqrt(floor(dx^2) + floor(dy^2))) <= 26 */
  for (int a = 0; a < 3600; a++) {
    fresh(1);
    g.n = 1, g.ang[0] = (int16_t)a, g.shots = 1, g.v = 0, g.pending = 1;
    ticks(4);
    bool miss = a >= SPEC_HIT_LO && a <= SPEC_HIT_HI;
    CHECK(g.state == (miss ? ST_MISS : ST_CLEAR), "a pin at %d tenths: state %d", a, g.state);
  }
}

/* ---------------------------------------------------------------- shots, clears, misses, screens */
static void rules(void) {
  /* level 1: 45 degrees in, a shot lands 3 frames later straight down, numbered 8 */
  fresh(1);
  ticks(29);
  g.pending = 1;
  tick();
  int t0 = (int)g.t;
  CHECK(g.popped == 1 && g.landed == 0, "the ball leaves the queue");
  ticks(2);
  CHECK(g.landed == 0, "in flight for 3 frames");
  tick();
  CHECK(g.landed == 1 && g.n == 5 && g.state == ST_RUN, "lands on the 3rd frame (frame %d)", t0 + 3);
  CHECK(g.ang[4] == (900 - (int)g.angle + 3600) % 3600 && g.num[4] == 8 && g.r[4] == 164, "where it lands");
  tick();
  CHECK(g.r[4] == 150, "then on the circle");
  /* two balls in one frame land together: the second hits the first */
  fresh(9);
  g.pending = 2;
  ticks(4);
  CHECK(g.state == ST_MISS && g.n == 2, "two in one frame");
  /* the last ball in clears the level even with one still flying (the original checks the queue) */
  fresh(9);
  g.shots = 2;
  g.pending = 1, tick();
  g.pending = 1, tick();
  ticks(2);
  CHECK(g.state == ST_CLEAR && g.landed == 1 && g.popped == 2, "cleared with a ball in flight");
  /* the clear: pins fly 25 a frame for 61 frames, the gray panel, the start screen with the next level */
  sv = (typeof(sv)){0}, sv.level = sv.best = 9;
  fresh(9);
  g.shots = 1, g.pending = 1, ticks(4);
  CHECK(g.state == ST_CLEAR && sv.level == 10 && sv.best == 10, "level 9 cleared: %d", sv.level);
  ticks(61);
  CHECK(g.r[0] == 164 + 25 * 61, "spokes %d", g.r[0]);
  ticks(11);
  CHECK(g.state == ST_CLEAR, "the panel still sliding");
  tick();
  CHECK(g.state == ST_START && g.level == 10, "the start screen, level 10");
  /* the miss: the ball swells 25, 15, 20, 15 (frames 4, 7, 10, 13), same level after the panel */
  sv.level = sv.best = 1;
  fresh(1);
  g.pending = 1, ticks(4);
  CHECK(g.state == ST_MISS && sv.level == 1, "level 1 missed at once (the pin at 90 degrees)");
  ticks(72);
  CHECK(g.state == ST_MISS, "the panel still sliding");
  tick();
  CHECK(g.state == ST_START && g.level == 1, "the start screen, same level");
  /* the start: the black panel for 12 frames; after level 60, "to be continued..." */
  start(5);
  ticks(11);
  CHECK(g.state == ST_IN, "panel sliding in");
  tick();
  CHECK(g.state == ST_RUN && g.level == 5 && g.t == 0, "level 5 starts");
  start(LAST + 1);
  ticks(12);
  CHECK(g.state == ST_END, "to be continued");
  /* an E level turns back on every shot, even with no ball left */
  fresh(41);
  g.shots = 0, g.pending = 1;
  tick();
  CHECK(g.angle == 3600 - 20, "E1 turned back: %d", (int)g.angle);
}

/* ---------------------------------------------------------------- a bot clears all 60 levels */
static bool safe_now(void) { /* a shot now lands where it hits nothing (frame by frame, as the game) */
  typeof(g) keep = g;
  typeof(sv) keep_sv = sv;
  g.pending++;
  for (int i = 0; i < 4 && g.state == ST_RUN; i++) tick();
  bool ok = g.state != ST_MISS;
  g = keep, sv = keep_sv;
  return ok;
}
static void bot(void) {
  long frames = 0;
  int worst = 0, total = 0;
  sv.level = sv.best = 1;
  for (int l = 1; l <= LAST; l++) {
    int tries = 0;
    for (; tries < 300; tries++) {
      start(l);
      ticks(12);
      for (int f = 0; f < 60 * 120 && g.state == ST_RUN; f++, frames++) {
        if (g.landed == g.popped && rnd() % 5 == 0 && safe_now()) g.pending++;
        tick();
      }
      if (g.state == ST_CLEAR) break;
    }
    total += tries + 1, worst = tries + 1 > worst ? tries + 1 : worst;
    CHECK(g.state == ST_CLEAR, "the bot could not clear level %d", l);
    ticks(80);
    CHECK(g.state == ST_START && sv.level == l + 1 && sv.best == l + 1, "after level %d: level %d", l, sv.level);
  }
  printf("bot: all 60 levels in %d tries (at most %d for one), %ld frames\n", total, worst, frames);
  start(sv.level);
  ticks(12);
  CHECK(g.state == ST_END, "after level 60: to be continued");
}

/* ---------------------------------------------------------------- random sessions */
static void render(void) {
  gfx_buf = strip;
  gfx_render(scene, 0, 0, SCREEN_H);
}
static bool sane(int s, int f) {
  int before = fails;
  CHECK(g.n <= MAXP && g.landed <= g.popped && g.popped <= g.shots && g.shots <= 26, "session %d frame %d: balls", s, f);
  CHECK(g.angle >= 0 && g.angle < 3600 && g.state <= ST_END, "session %d frame %d: core", s, f);
  CHECK(g.level >= 1 && g.level <= LAST + 1 && sv.level >= 1 && sv.level <= sv.best && sv.best <= LAST + 1,
        "session %d frame %d: levels %d %d %d", s, f, g.level, sv.level, sv.best);
  CHECK(ov >= 0 && ov <= OV_LEVELS && sel >= 0 && sel < (ov == OV_LEVELS ? LAST : ov ? ov_count[ov] : 5),
        "session %d frame %d: menu %d %d", s, f, ov, sel);
  return fails == before;
}
static void sessions(void) {
  static const int keys[] = {eadk_key_ok, eadk_key_exe, eadk_key_back, eadk_key_up, eadk_key_down, eadk_key_left,
                             eadk_key_right, eadk_key_ok, eadk_key_ok, eadk_key_exe};
  int quits = 0;
  for (int s = 0; s < 300; s++) {
    /* what is on the calculator: nothing, a save, a damaged one */
    uint32_t k = rnd() % 4;
    if (k == 0) disk_len = 0;
    else {
      sv = (typeof(sv)){0}, sv.flags = (uint8_t)(rnd() & 2), sv.best = (uint8_t)(1 + rnd() % 61);
      sv.level = (uint8_t)(1 + rnd() % sv.best);
      save();
      if (k == 3) disk[rnd() % disk_len] ^= (uint8_t)(1 + rnd() % 255);
    }
    memset(&g, 0, sizeof g), ov = sel = 0;
    load();
    g.level = sv.level;
    if (!(sv.flags & SEEN)) ov = OV_HELP, sv.flags |= SEEN;
    for (int f = 0; f < 4000; f++) {
      uint64_t hit = rnd() % 3 ? 0 : KEY(keys[rnd() % 10]);
      if (press(hit)) { /* quit, saved, and opened again */
        quits++;
        save();
        memset(&g, 0, sizeof g), ov = sel = 0;
        load();
        g.level = sv.level;
        CHECK(sv.flags & SEEN, "session %d: the guide again", s);
      }
      if (!ov) ticks(1 + (int)(rnd() % 3 == 0));
      if (!sane(s, f)) break;
      if (f % 25 == 0) render();
    }
  }
  printf("sessions: 300 random ones of 4000 frames, quit through the menu and opened again %d times\n", quits);
}

/* ---------------------------------------------------------------- saves */
static void saves(void) {
  sv = (typeof(sv)){0}, sv.level = 12, sv.best = 30, sv.flags = SEEN;
  save();
  CHECK(disk_len == 12 && disk[0] == 'C' && disk[1] == 1, "save: %u bytes", disk_len);
  memset(&sv, 0, sizeof sv);
  load();
  CHECK(sv.level == 12 && sv.best == 30 && sv.flags == SEEN, "save not read back");
  uint8_t good[12];
  memcpy(good, disk, 12);
  for (int i = 0; i < 12; i++)
    for (int b = 0; b < 8; b++) {
      memcpy(disk, good, 12), disk[i] ^= (uint8_t)(1 << b);
      load();
      CHECK(sv.level == 1 && sv.best == 1 && sv.flags == 0, "byte %d bit %d damaged: not reset", i, b);
    }
  memcpy(disk, good, 12), disk_len = 5;
  load();
  CHECK(sv.level == 1 && sv.best == 1, "a short file should reset");
  /* checked but impossible: level 0, past the best, a best past 61 */
  static const uint8_t bad[3][2] = {{0, 5}, {9, 5}, {3, 62}};
  for (int i = 0; i < 3; i++) {
    sv = (typeof(sv)){0}, sv.level = bad[i][0], sv.best = bad[i][1];
    save();
    load();
    CHECK(sv.level == 1 && sv.best == 1, "impossible save %d kept", i);
  }
}

/* ---------------------------------------------------------------- pictures */
static void picture(const char *dir, const char *name) {
  char path[256];
  snprintf(path, sizeof path, "%s/%s.ppm", dir, name);
  render();
  FILE *f = fopen(path, "wb");
  if (!f) exit(2);
  fprintf(f, "P6 320 240 255\n");
  for (int y = 0; y < SCREEN_H; y++)
    for (int x = 0; x < SCREEN_W; x++) {
      uint16_t c = fb[y][x];
      fputc((c >> 11) * 255 / 31, f), fputc((c >> 5 & 63) * 255 / 63, f), fputc((c & 31) * 255 / 31, f);
    }
  fclose(f);
}
/* the bot plays level l until it has placed n balls, taking its time (the same moments every time) */
static void play_to(int l, int n) {
  seed = 11;
  for (;;) {
    start(l);
    ticks(12);
    while (g.state == ST_RUN && g.landed < n) {
      if (g.landed == g.popped && rnd() % 25 == 0 && safe_now()) g.pending++;
      tick();
    }
    if (g.state == ST_RUN) break;
  }
}
static void shots(const char *dir) {
  sv = (typeof(sv)){0}, sv.level = 2, sv.best = 23, sv.flags = SEEN;
  g.level = 2, g.state = ST_START;
  picture(dir, "start");
  play_to(2, 4);
  ticks(9);
  picture(dir, "play");
  ov = OV_MENU, sel = 0;
  picture(dir, "menu");
  ov = OV_SURE, sel = 0;
  picture(dir, "quit");
  ov = OV_HELP, sel = 0;
  picture(dir, "help");
  ov = OV_LEVELS, sel = 22;
  picture(dir, "levels");
  ov = 0;
  play_to(10, 9);
  while (safe_now()) tick();
  g.pending = 1, ticks(4 + 5);
  picture(dir, "miss");
  play_to(5, 7);
  while (g.state == ST_RUN) g.pending += g.landed == g.popped && safe_now(), tick();
  ticks(6);
  picture(dir, "clear");
  ticks(61);
  picture(dir, "slide");
  start(LAST + 1), ticks(12);
  picture(dir, "end");
}

int main(int argc, char **argv) {
  if (argc >= 3 && !strcmp(argv[1], "shot")) {
    shots(argv[2]);
    return 0;
  }
  levels_vs_spec();
  rules();
  bot();
  sessions();
  saves();
  render();
  printf(fails ? "FAILED (%d)\n" : "ok\n", fails);
  return fails != 0;
}
