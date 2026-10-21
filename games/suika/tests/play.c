/* Suika Game on a computer, for tests and pictures (make host, make test).
 *
 *   play test             the original's numbers (a grape's fall, the cloud's speed, no bounce, the next
 *                         fruit on touching, the points of every merge, two watermelons vanishing, the
 *                         game ending after 47 steps above the line); a bot plays 300 games with every
 *                         step checked against the rules (merges scored right, fruit in the box, drops
 *                         from the first five); a settled pile must sleep without a pixel of jitter; the
 *                         whole game run 120 times with random keys; a save comes back, a damaged one
 *                         starts afresh
 *   play film DIR N [SEED [SKIP]]   N frames of the bot's game after SKIP steps (DIR/f_NNNN.ppm)
 *   play screens DIR      the menus over a filled box (DIR/ov_N.ppm), and that game's save (DIR/suika.sav)
 *   play icon FILE        the app's icon (the top left 55 x 56 of FILE) */
#define NP_MODULE 1 /* (its drawing pushed to fb, its saves kept in disk, below) */
#define main sk_main
#define gfx_render test_render
#include "../src/main.c"
#undef gfx_render
#undef main
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void gfx_render(gfx_scene_t scene, void *ctx, int y0, int y1);
static uint16_t fb[SCREEN_H][SCREEN_W];
static uint8_t disk[sizeof sv + 64];
static uint32_t disk_len;
void np_push(int x, int y, int w, int h, const color_t *p) {
  for (int j = 0; j < h; j++)
    for (int i = 0; i < w; i++) fb[y + j][x + i] = p[j * w + i];
}
static int render_every = 1, render_n;
void test_render(gfx_scene_t scene, void *ctx, int y0, int y1) {
  if (++render_n % render_every == 0) gfx_render(scene, ctx, y0, y1);
}

/* the calculator, for the game run whole: a clock, random keys, Home after a while */
static uint32_t clock_ms, frames_left, key_hold;
static uint64_t keys_now;
static int fails, checking;
#define CHECK(c, ...) do { if (!(c)) { printf(__VA_ARGS__); printf("\n"); fails++; } } while (0)
static uint32_t lcg = 7;
static uint32_t rand32(void) { return lcg = lcg * 1664525u + 1013904223u; }
static void invariants(const char *where);
uint64_t eadk_keyboard_scan(void) {
  if (!frames_left) return 0;
  if (--frames_left == 0) return KEY(eadk_key_home);
  if (!key_hold) { /* a new key (or none), held for a while */
    static const int pick[] = {-1, -1, -1, eadk_key_left, eadk_key_right, eadk_key_four, eadk_key_six, eadk_key_ok, eadk_key_exe,
                               eadk_key_back, eadk_key_up, eadk_key_down, eadk_key_ok, eadk_key_left, eadk_key_right};
    int k = pick[(rand32() >> 8) % (sizeof pick / sizeof pick[0])];
    keys_now = k < 0 ? 0 : KEY(k), key_hold = 1 + (rand32() >> 8) % 12;
  }
  key_hold--;
  return keys_now;
}
uint64_t eadk_timing_millis(void) { return clock_ms; }
void eadk_timing_msleep(uint32_t ms) { clock_ms += ms; }
bool eadk_display_wait_for_vblank(void) {
  if (checking) invariants("in play");
  return true;
}
void *np_alloc(uint32_t size) { return malloc(size); }
uint32_t eadk_random(void) { return rand32(); }
const uint8_t *np_file_read(const char *name, uint32_t *len) { *len = disk_len; return disk_len ? disk : 0; }
bool np_file_write(const char *name, const void *d, uint32_t len) {
  if (len > sizeof disk) return false;
  memcpy(disk, d, disk_len = len);
  return true;
}

/* ------------------------------------------------------------ the rules, checked */
static void invariants(const char *where) {
  CHECK(sv.cur >= 1 && sv.cur <= 5 && sv.next >= 1 && sv.next <= 5, "%s: drops come from the first five (%d %d)", where, sv.cur, sv.next);
  CHECK(ncon[cb] <= MAXC && ov >= 0 && ov <= OV_OVER, "%s: state", where);
  for (int i = 0; i < MAXF; i++) {
    fruit_t *a = &f[i];
    if (!a->k) continue;
    float r = RAD(a->k);
    CHECK(a->k <= 11, "%s: kind %d", where, a->k);
    CHECK(isfinite(a->x) && isfinite(a->y) && isfinite(a->vx) && isfinite(a->vy) && isfinite(a->w), "%s: not a number", where);
    CHECK(a->x >= r - 1 && a->x <= W - r + 1 && a->y <= FLOOR - r + 1, "%s: fruit %d (kind %d) out of the box at %.1f %.1f", where, i,
          a->k, a->x, a->y);
    CHECK(a->over < OVER_STEPS || over, "%s: above the line too long without the end", where);
  }
}
static void count(int n[13]) {
  memset(n, 0, 13 * sizeof n[0]);
  for (int i = 0; i < MAXF; i++) n[f[i].k]++;
}
static int touching(int d) {
  for (int i = 0; i < ncon[cb]; i++)
    if (con[cb][i].a == d || con[cb][i].b == d) return 1;
  return 0;
}
static long steps_checked, merges_seen[13];
static int most; /* fruit in the box at once */
/* a step, and what it did against the rules: merges from the changes in each kind's count, each worth
   T(k); the next fruit only once the dropped one touched something */
static void checked_sim(int dir) {
  int n0[13], n1[13], d = sv.drop - 1;
  uint32_t s0 = sv.score;
  bool was_held = sv.flags & HELD, was_over = over;
  count(n0);
  sim(dir);
  count(n1);
  long m = 0, pts = 0, carry = 0;
  for (int k = 1; k <= 11; k++) {
    long diff = n1[k] - n0[k]; /* = merges of the kind below - 2 x merges of this kind */
    long mk = carry - diff;
    CHECK(mk >= 0 && mk % 2 == 0, "merge counts don't add up (kind %d)", k);
    mk /= 2, m += mk, pts += mk * k * (k + 1) / 2, merges_seen[k] += mk, carry = mk;
  }
  CHECK(sv.score - s0 == pts, "score %u -> %u, the merges were worth %ld", s0, sv.score, pts);
  if (!was_held && d >= 0) {
    if (sv.flags & HELD) CHECK(!f[d].k || f[d].land, "the next fruit came before the dropped one touched anything");
    else CHECK(!touching(d), "the dropped fruit touched something, and no next fruit");
  }
  CHECK(was_held || !(sv.flags & HELD) || !sv.drop, "drop state");
  if (over && !was_over) {
    bool why = false;
    for (int i = 0; i < MAXF; i++) why |= f[i].k && f[i].land && f[i].y < RAD(f[i].k) && f[i].over >= OVER_STEPS;
    CHECK(why, "the game ended with no fruit above the line");
  }
  invariants("bot");
  steps_checked++;
  int many = 0;
  for (int i = 0; i < MAXF; i++) many += f[i].k > 0;
  most = many > most ? many : most;
}

/* ------------------------------------------------------------ the bot */
/* where a fruit dropped at x would stop first, and on what */
static float landing(float x, int k, int *on) {
  float r = RAD(k), best = FLOOR - r;
  *on = 0;
  for (int i = 0; i < MAXF; i++) {
    if (!f[i].k) continue;
    float rs = r + RAD(f[i].k), dx = f[i].x - x;
    if (dx * dx >= rs * rs) continue;
    float y = f[i].y - sqrtf(rs * rs - dx * dx);
    if (y < best) best = y, *on = f[i].k;
  }
  return best;
}
static float target;
static void choose(void) {
  float r = RAD(sv.cur), bestv = -1e9f;
  for (int n = 0; n <= 32; n++) {
    float x = r + (W - 2 * r) * (float)n / 32;
    int on;
    float v = landing(x, sv.cur, &on) + (on == sv.cur ? 160 : 0) - (on && on < sv.cur ? 60 : 0) + (float)(rand32() % 40);
    if (v > bestv) bestv = v, target = x;
  }
}
/* one bot move: to its spot, then drop; false when the game is over */
static bool bot_move(bool random_play) {
  if (over) return false;
  int dir = 0;
  if (sv.flags & HELD) {
    if (random_play) {
      if (rand32() % 20 == 0) drop_fruit();
      dir = (int)(rand32() % 3) - 1;
    } else if (fabsf(target - sv.aim) <= CLOUD / 2 + 0.01f) {
      drop_fruit();
    } else
      dir = target > sv.aim ? 1 : -1;
  }
  bool was_held = sv.flags & HELD;
  checked_sim(dir);
  if (!was_held && sv.flags & HELD && !random_play) choose();
  return !over;
}
static void bot_game(uint32_t seed, int max_steps, bool random_play) {
  sv.rng = seed ? seed : 1;
  new_game();
  choose();
  for (int s = 0; s < max_steps && bot_move(random_play); s++) now += 20;
}

/* ------------------------------------------------------------ pictures */
static void picture(const char *path) {
  FILE *fp = fopen(path, "wb");
  if (!fp) exit(2);
  fprintf(fp, "P6 320 240 255\n");
  for (int y = 0; y < SCREEN_H; y++)
    for (int x = 0; x < SCREEN_W; x++) {
      uint16_t c = fb[y][x];
      fputc((c >> 11) * 255 / 31, fp), fputc((c >> 5 & 63) * 255 / 63, fp), fputc((c & 31) * 255 / 31, fp);
    }
  fclose(fp);
}
static void render(void) {
  gfx_buf = strip;
  gfx_render(scene, 0, 0, SCREEN_H);
}

/* a fruit put somewhere by hand */
static int place(int k, float x, float y, bool land) {
  for (int i = 0; i < MAXF; i++)
    if (!f[i].k) {
      f[i] = (fruit_t){x, y, 0, 0, 1, 0, 0, (uint8_t)k, land, 0, 0, 0};
      return i;
    }
  return -1;
}
static void empty_box(void) {
  new_game();
  over = false;
}

/* the app's icon: a watermelon and a cherry on the box's honey */
static void icon_scene(void *ctx) {
  gfx_vgrad(0, 0, 55, 56, 0xF6DC8C, 0xF9EFC8);
  fruit(11, 26, 32, 20.5f, 0.97f, -0.24f);
  fruit(1, 45, 15, 6.5f, 1, 0);
}

int main(int argc, char **argv) {
  if (argc >= 3 && !strcmp(argv[1], "icon")) {
    gfx_buf = strip;
    gfx_render(icon_scene, 0, 0, 64);
    picture(argv[2]);
    return 0;
  }
  if (argc >= 3 && !strcmp(argv[1], "screens")) { /* the overlays, over a game going on; and its save */
    sv.flags = SEEN, sv.rng = 26;
    new_game();
    choose();
    for (int s = 0; s < 20000 && bot_move(false); s++) { /* until the box is well filled */
      int many = 0;
      for (int i = 0; i < MAXF; i++) many += f[i].k > 0;
      if (many >= 30 && sv.flags & HELD) break;
      now += 20;
    }
    char path[512];
    for (int o = OV_MENU; o <= OV_OVER; o++) {
      ov = o, sel = 0;
      render();
      snprintf(path, sizeof path, "%s/ov_%d.ppm", argv[2], o);
      picture(path);
    }
    ov = 0, save();
    snprintf(path, sizeof path, "%s/suika.sav", argv[2]);
    FILE *fp = fopen(path, "wb");
    fwrite(disk, 1, disk_len, fp), fclose(fp);
    return 0;
  }
  if (argc >= 4 && !strcmp(argv[1], "film")) {
    int frames = atoi(argv[3]), skip = argc > 5 ? atoi(argv[5]) : 0, shot = 0;
    sv.flags = SEEN;
    sv.rng = argc > 4 ? (uint32_t)atoi(argv[4]) : 5;
    new_game();
    choose();
    now = 1000, sv.best = 2471; /* (someone's best from before) */
    char path[512];
    for (int s = 0; s < skip + frames && !over; s++) {
      bot_move(false);
      now += 20;
      if (s < skip || (s - skip) % 2) continue;
      render();
      snprintf(path, sizeof path, "%s/f_%04d.ppm", argv[2], shot++);
      picture(path);
    }
    printf("score %u, %s\n", sv.score, over ? "over" : "going");
    return 0;
  }

  /* ---- the original's numbers */
  sv.rng = 99;
  empty_box();
  sv.cur = 3, aim_at(341); /* a grape where the video's first one fell */
  drop_fruit();
  int d = sv.drop - 1, steps = 0;
  while (!(sv.flags & HELD) && steps < 200) checked_sim(0), steps++;
  printf("a grape falls to the floor in %d steps (%.2f s; the Switch: 0.87 s)\n", steps, steps * DT);
  CHECK(steps >= 42 && steps <= 46, "fall time");
  float rest = FLOOR - RAD(3), highest = 1e9f;
  for (int s = 0; s < 60; s++) {
    checked_sim(0);
    if (f[d].y < highest) highest = f[d].y;
  }
  printf("  no bounce: after landing it stays at least %.2f px into the floor, and rests %.2f px into it\n", highest - rest,
         f[d].y - rest);
  CHECK(fabsf(f[d].y - rest) < SLOP + 0.1f && highest > rest - 1, "it should land without bouncing");
  CHECK(f[d].zz, "a fruit at rest should sleep");
  float a0 = sv.aim;
  for (int s = 0; s < 25; s++) checked_sim(-1);
  CHECK(fabsf(a0 - sv.aim - 25 * CLOUD) < 0.01f, "the cloud moves 290 px/s");
  /* every merge's points, and the fruit it makes, at the middle of the two */
  for (int k = 1; k <= 11; k++) {
    empty_box();
    float r = RAD(k);
    int i = place(k, W / 2 - r + 1, FLOOR - r, true);
    place(k, W / 2 + r - 1, FLOOR - r, true);
    uint32_t s0 = sv.score;
    checked_sim(0);
    CHECK(sv.score - s0 == (uint32_t)(k * (k + 1) / 2), "two of kind %d: %u points", k, sv.score - s0);
    if (k < 11) CHECK(f[i].k == k + 1 && fabsf(f[i].x - W / 2) < 2, "two of kind %d should make kind %d in the middle", k, k + 1);
    else CHECK(!f[i].k && !f[i + 1].k, "two watermelons should both vanish");
  }
  /* the end: a fruit that has landed, above the line for 47 steps */
  empty_box();
  int top = place(5, 200, -60, true), loose = place(4, 60, -60, false);
  for (int s = 1; s <= OVER_STEPS && !over; s++) {
    f[top].y = f[loose].y = -60, f[top].vy = f[loose].vy = 0;
    sim(0);
    CHECK(over == (s == OVER_STEPS), "the game should end after exactly %d steps above the line (step %d)", OVER_STEPS, s);
  }
  CHECK(!(sv.flags & GAME), "a game over is not one to go on with");
  empty_box();
  loose = place(4, 60, -60, false);
  for (int s = 0; s < 100; s++) f[loose].y = -60, f[loose].vy = 0, sim(0);
  CHECK(!over, "a fruit still falling never ends the game");

  /* ---- the bot: 300 games, every step checked */
  long total = 0, drops = 0;
  uint32_t best = 0;
  int biggest = 0, ended = 0;
  for (int g = 0; g < 300; g++) {
    bot_game(1234 + g * 7919, 12000, false);
    total += sv.score, best = sv.score > best ? sv.score : best, ended += over;
    for (int i = 0; i < MAXF; i++) biggest = f[i].k > biggest ? f[i].k : biggest;
  }
  for (int k = 1; k <= 11; k++) biggest = merges_seen[k] ? (k + 1 > biggest ? k + 1 : biggest) : biggest;
  printf("bot: 300 games, %ld points on average, best %u, %d ended, biggest fruit %d, %d fruit at most, %ld steps checked\n",
         total / 300, best, ended, biggest > 11 ? 11 : biggest, most, steps_checked);
  printf("  merges by kind:");
  for (int k = 1; k <= 11; k++) printf(" %ld", merges_seen[k]);
  printf("\n");
  CHECK(best > 1500 && merges_seen[10] > 0, "the bot should make a watermelon at least once");
  (void)drops;
  /* random play */
  for (int g = 0; g < 100; g++) bot_game(77 + g, 6000, true);

  /* ---- a settled pile sleeps and never moves */
  sv.rng = 4242;
  new_game();
  choose();
  for (int s = 0; s < 20000 && bot_move(false); s++) {
    int many = 0;
    for (int i = 0; i < MAXF; i++) many += f[i].k > 0;
    if (many >= 24 && sv.flags & HELD) break;
  }
  over = false;
  sv.flags &= (uint8_t)~HELD; /* no more drops */
  sv.drop = 0;
  for (int s = 0; s < 400; s++) checked_sim(0);
  float snap[MAXF][2], moved = 0, overlap = 0;
  int awake = 0, n = 0;
  for (int i = 0; i < MAXF; i++) snap[i][0] = f[i].x, snap[i][1] = f[i].y;
  for (int s = 0; s < 200; s++) checked_sim(0);
  for (int i = 0; i < MAXF; i++) {
    if (!f[i].k) continue;
    n++, awake += !f[i].zz;
    float m = fabsf(f[i].x - snap[i][0]) + fabsf(f[i].y - snap[i][1]);
    moved = m > moved ? m : moved;
    for (int j = i + 1; j < MAXF; j++)
      if (f[j].k) {
        float dx = f[j].x - f[i].x, dy = f[j].y - f[i].y, o = RAD(f[i].k) + RAD(f[j].k) - sqrtf(dx * dx + dy * dy);
        overlap = o > overlap ? o : overlap;
      }
  }
  printf("a settled pile of %d: %d awake, moved %.4f px in 4 s, deepest overlap %.2f px (%.2f on screen)\n", n, awake, moved,
         overlap, overlap / 3);
  CHECK(n > 10 && awake == 0 && moved < 0.01f, "a settled pile should sleep, not jitter");
  CHECK(overlap < 6, "fruit sink into each other");

  /* ---- the whole game with random keys: menus, drops, quitting; the save after each */
  render_every = 25, checking = 1;
  for (int run = 0; run < 120; run++) {
    if (run % 30 == 0) disk_len = 0;
    clock_ms = 1000 + run * 7, frames_left = 300 + rand32() % 3000, key_hold = 0;
    memset(f, 0, sizeof f), ov = 0, over = false;
    sk_main();
    uint8_t copy[sizeof sv];
    memcpy(copy, &sv, sizeof sv);
    load();
    CHECK(!memcmp(copy, &sv, sizeof sv), "run %d: the save didn't come back as it was", run);
  }
  checking = 0;

  /* ---- the save: back as it was (the falling fruit too); damaged, a fresh start */
  bot_game(31337, 900, false);
  if (sv.flags & HELD) drop_fruit();
  for (int s = 0; s < 5; s++) sim(0);
  sv.best = 777;
  save();
  fruit_t keep[MAXF];
  memcpy(keep, f, sizeof keep);
  uint32_t score = sv.score;
  uint8_t drop = sv.drop, cur = sv.cur, next = sv.next;
  memset(&sv, 0, sizeof sv), memset(f, 0, sizeof f);
  load();
  bool same = sv.best == 777 && sv.score == score && sv.drop == drop && sv.cur == cur && sv.next == next && (sv.flags & GAME);
  for (int i = 0; i < MAXF; i++)
    same &= f[i].k == keep[i].k && (!f[i].k || (f[i].x == keep[i].x && f[i].y == keep[i].y && f[i].vy == keep[i].vy &&
                                                f[i].c == keep[i].c && f[i].land == keep[i].land));
  CHECK(same, "save not read back");
  disk[40] ^= 4;
  load();
  CHECK(sv.best == 0 && !(sv.flags & GAME), "a damaged file should start afresh");
  disk[40] ^= 4, disk_len -= 4;
  load();
  CHECK(sv.best == 0, "a short file should start afresh");
  render();
  printf(fails ? "FAILED (%d)\n" : "ok\n", fails);
  return fails != 0;
}
