/* Donkey Kong on a computer, for tests and pictures (make host, make test).
 *
 *   play test           the rules against the arcade's numbers (the spec), a bot through the four boards on
 *                       three levels and through a whole game, saves written and read back, damaged saves
 *                       refused, and a few hundred sessions of the whole app with random keys, quitting with Home
 *   play shots DIR      pictures of the screens and of each board (DIR/<name>.ppm)
 *   play film DIR N     N frames of the bot's game, every other frame (DIR/f_NNNN.ppm)
 */
#define main dk_app_main
#include "../src/main.c"
#undef main
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* ---------------------------------------------------------------- the calculator, on a computer */
static uint16_t fb[SCREEN_H][SCREEN_W];
void np_push(int x, int y, int w, int h, const color_t *px) {
  for (int j = 0; j < h; j++)
    for (int i = 0; i < w; i++) fb[y + j][x + i] = px[j * w + i];
}
void *np_alloc(uint32_t size) { return malloc(size); }
static double clock_ms;
static uint32_t (*script)(uint32_t frame);
static uint32_t frame_no, quit_at;
uint32_t plat_keys(void) {
  clock_ms += 1000.0 / 30;
  frame_no++;
  if (frame_no >= quit_at) return JK_HOME;
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
  for (int y = 0; y < SCREEN_H; y++)
    for (int x = 0; x < SCREEN_W; x++) {
      uint16_t c = fb[y][x];
      fputc((c >> 11) * 255 / 31, f), fputc((c >> 5 & 63) * 255 / 63, f), fputc((c & 31) * 255 / 31, f);
    }
  fclose(f);
}

static int fails;
#define CHECK(c, ...) do { if (!(c)) { fails++; printf("FAIL %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)
#define M(s, a) DK_RAM(s, a)

/* ---------------------------------------------------------------- helpers */

/* a new game that starts at a place in the boards' order (0x3A65...), on a given level, past the intro */
static void start_on(dk_state *s, uint32_t seed, int level, uint16_t order) {
  dk_new_game(s, seed);
  M(s, 0x6041) = (uint8_t)level;
  M(s, 0x6042) = (uint8_t)order, M(s, 0x6043) = (uint8_t)(order >> 8);
  M(s, 0x6044) = 0; /* (the intro is for a new game) */
}
static int run_until_mode(dk_state *s, int mode, int max) {
  for (int i = 0; i < max; i++) {
    if (dk_mode(s) == mode) return i;
    dk_tick(s, 0);
  }
  return -1;
}
static uint16_t order_of(int screen) { return (uint16_t)(0x3A6A + screen - 1); } /* level 3: 25 50 75 100 */
static void play_on(dk_state *s, uint32_t seed, int level, int screen) {
  start_on(s, seed, level, order_of(screen));
  run_until_mode(s, DK_MODE_PLAY, 4000);
}
static void calm(dk_state *s) { /* no barrels, fires, springs or pies */
  for (int i = 0; i < 10; i++) M(s, 0x6700 + i * 0x20) = 0, M(s, 0x6500 + i * 0x10) = 0, M(s, 0x6980 + i * 4) = 0;
  for (int i = 0; i < 5; i++) M(s, 0x6400 + i * 0x20) = 0, M(s, 0x69D0 + i * 4) = 0;
  for (int i = 0; i < 6; i++) M(s, 0x65A0 + i * 0x10) = 0;
  M(s, 0x6392) = M(s, 0x6393) = 0, M(s, 0x62B9) = 0, M(s, 0x63A0) = 0, M(s, 0x639A) = 0, M(s, 0x6396) = 0;
}
static void tick_calm(dk_state *s, uint8_t joy, int n) {
  for (int i = 0; i < n; i++) calm(s), dk_tick(s, joy), M(s, 0x6393) = 1; /* (Kong keeps his barrel) */
}

/* ---------------------------------------------------------------- the rules against the spec */
static void units(void) {
  static dk_state s;
  /* the boards' order (0x3A65): level 1 25 100, 2 25 75 100, 3 25 50 75 100, 4 25 50 25 75 100, then
     25 50 25 75 25 100 again and again */
  {
    static const char want[] = "14" "134" "1234" "12134" "121314" "121314";
    char got[32] = {0};
    int n = 0;
    start_on(&s, 5, 1, 0x3A65);
    run_until_mode(&s, DK_MODE_PLAY, 4000);
    while (n < (int)sizeof want - 1) {
      got[n++] = (char)('0' + dk_screen(&s));
      /* clear the board at once: Pauline reached, or the rivets all gone */
      if (dk_screen(&s) == 4) M(&s, 0x6290) = 0;
      else M(&s, 0x6205) = 0x30, M(&s, 0x6216) = 0;
      dk_tick(&s, 0);
      CHECK(dk_mode(&s) == DK_MODE_CLEAR, "board %d not cleared", n);
      run_until_mode(&s, DK_MODE_HOWHIGH, 3000);
      run_until_mode(&s, DK_MODE_PLAY, 4000);
    }
    CHECK(!strcmp(got, want), "the boards came in this order: %s (not %s)", got, want);
    CHECK(dk_level(&s) == 7, "level %d after six levels", dk_level(&s));
  }
  /* the bonus: 5000, 6000, 7000, 8000 from level 4 on, losing 100 every 120, 100, 80, 60 frames (not 25m);
     level 22 wraps to 400: the kill screen */
  for (int level = 1; level <= 5; level++) {
    play_on(&s, 7, level, 3);
    int want = level < 4 ? 40 + level * 10 : 80;
    CHECK(M(&s, 0x62B0) == want && M(&s, 0x638C) == (want / 10) << 4, "level %d bonus %d (%02X)", level, M(&s, 0x62B0),
          M(&s, 0x638C));
    CHECK(M(&s, 0x62B3) == 220 - 2 * want, "level %d bonus clock %d", level, M(&s, 0x62B3));
    uint8_t b0 = M(&s, 0x638C);
    int f = 0;
    while (M(&s, 0x638C) == b0 && f < 400) tick_calm(&s, 0, 1), f++;
    int g = 0;
    b0 = M(&s, 0x638C);
    while (M(&s, 0x638C) == b0 && g < 400) tick_calm(&s, 0, 1), g++;
    CHECK(g == 220 - 2 * want, "level %d: 100 off the bonus every %d frames", level, g);
  }
  play_on(&s, 7, 22, 1);
  CHECK(M(&s, 0x62B0) == 4, "level 22 bonus %d", M(&s, 0x62B0));
  /* 25m: 100 off the bonus for each barrel Kong takes */
  {
    play_on(&s, 9, 1, 1);
    int barrels = 0, b = M(&s, 0x62B1);
    for (int f = 0; f < 600 && dk_alive(&s); f++) {
      bool deploying = M(&s, 0x6393);
      dk_tick(&s, 0);
      if (!deploying && M(&s, 0x6393)) barrels++;
    }
    CHECK(b - M(&s, 0x62B1) == barrels && barrels >= 3, "%d barrels, bonus %d to %d", barrels, b, M(&s, 0x62B1));
  }
  /* the difficulty: the level at the start of a board, +1 every 2048 frames, never above 5 */
  {
    play_on(&s, 11, 2, 4);
    CHECK(M(&s, 0x6380) == 2, "difficulty %d at the start of level 2", M(&s, 0x6380));
    M(&s, 0x600A) = DK_MODE_OVER, s.over = 1; /* (the clock runs between vblanks, whatever the game does) */
    int seen[4] = {0}, k = 0;
    for (int f = 0; f < 2048 * 4 + 300; f++) {
      dk_tick(&s, 0);
      if (f % 2048 == 100 && k < 4) seen[k++] = M(&s, 0x6380);
    }
    CHECK(seen[0] == 2 && seen[1] == 3 && seen[2] == 4 && seen[3] == 5 && M(&s, 0x6380) == 5,
          "difficulty %d %d %d %d then %d", seen[0], seen[1], seen[2], seen[3], M(&s, 0x6380));
  }
  /* Mario: walks 2 px in 3 frames, jumps 14 px for 41 frames (top at frame 20), climbs 2 px every 5 frames up
     and every 4 down */
  {
    play_on(&s, 13, 1, 4);
    int x = M(&s, 0x6203);
    tick_calm(&s, DK_RIGHT, 30);
    CHECK(M(&s, 0x6203) - x == 20, "walked %d px in 30 frames", M(&s, 0x6203) - x);
    tick_calm(&s, 0, 2);
    int y0 = M(&s, 0x6205), top = 255, at = 0, land = 0;
    tick_calm(&s, DK_JUMP, 1);
    for (int f = 1; f < 60; f++) {
      tick_calm(&s, 0, 1);
      if (M(&s, 0x6205) < top) top = M(&s, 0x6205), at = f;
      if (!land && !M(&s, 0x6216)) land = f;
    }
    CHECK(y0 - top == 14 && at >= 19 && at <= 21 && (land == 41 || land == 42), "jump: %d px, top at %d, down at %d", y0 - top, at, land);
    play_on(&s, 13, 1, 4);
    while (M(&s, 0x6203) > 0x1B) tick_calm(&s, DK_LEFT, 1);
    int y = M(&s, 0x6205);
    tick_calm(&s, DK_UP, 50);
    CHECK(y - M(&s, 0x6205) == 20, "climbed %d px in 50 frames", y - M(&s, 0x6205));
    y = M(&s, 0x6205);
    tick_calm(&s, DK_DOWN, 40);
    CHECK(M(&s, 0x6205) - y == 20, "went down %d px in 40 frames", M(&s, 0x6205) - y);
  }
  /* a fall of more than 15 px kills: off the middle of 75m into a shaft (40 px), but not a jump down to the next
     step (8 px) */
  {
    play_on(&s, 15, 1, 3);
    M(&s, 0x6203) = 0x60, M(&s, 0x6205) = 0xC8; /* on the girder at 0xD0, between the shafts */
    for (int f = 0; f < 120 && dk_mode(&s) == DK_MODE_PLAY; f++) {
      for (int e = 0; e < 6; e++) M(&s, 0x6600 + e * 0x10) = 0; /* (no elevator to land on) */
      tick_calm(&s, DK_RIGHT, 1);
    }
    CHECK(dk_mode(&s) == DK_MODE_DEAD, "a 40 px fall didn't kill");
    play_on(&s, 15, 1, 3);
    M(&s, 0x6203) = 0xE2, M(&s, 0x6205) = 0x60; /* on the girder at 0x68, a jump left onto the one at 0x70 */
    tick_calm(&s, DK_JUMP | DK_LEFT, 1);
    tick_calm(&s, 0, 70);
    CHECK(dk_mode(&s) == DK_MODE_PLAY && dk_alive(&s) && M(&s, 0x6205) == 0x68, "an 8 px drop: y %02X", M(&s, 0x6205));
  }
  /* points: a barrel jumped 100, two at once 300; a barrel smashed 300; an item 300, 500, 800 by level; a
     rivet 100; the extra life at 7000 */
  {
    for (int two = 0; two < 2; two++) {
      bool scored = false;
      for (int wait = 0; wait < 60 && !scored; wait++) {
        play_on(&s, 17, 1, 1);
        calm(&s);
        uint8_t x = M(&s, 0x6203), y = M(&s, 0x6205);
        for (int b = 0; b <= two; b++) { /* barrels rolling left toward Mario */
          uint16_t ix = (uint16_t)(0x6700 + b * 0x20);
          M(&s, ix) = 1, M(&s, ix + 1) = 0, M(&s, ix + 2) = 4, M(&s, ix + 3) = (uint8_t)(x + 40 + b * 6);
          M(&s, ix + 5) = (uint8_t)(y + 3), M(&s, ix + 7) = 0x15, M(&s, ix + 8) = 0x0B, M(&s, ix + 0x0F) = 4;
          M(&s, ix + 0x15) = 0;
        }
        uint32_t sc = dk_score(&s);
        for (int f = 0; f < wait; f++) dk_tick(&s, 0), M(&s, 0x6393) = 1;
        dk_tick(&s, DK_JUMP), M(&s, 0x6393) = 1;
        for (int f = 0; f < 70 && dk_alive(&s); f++) dk_tick(&s, 0), M(&s, 0x6393) = 1;
        if (dk_alive(&s) && dk_score(&s) != sc) {
          scored = true;
          CHECK(dk_score(&s) - sc == (two ? 300u : 100u), "%d barrel(s) jumped: %u points", two + 1, dk_score(&s) - sc);
        }
      }
      CHECK(scored, "never scored jumping %d barrel(s)", two + 1);
    }
    play_on(&s, 19, 1, 1);
    calm(&s);
    M(&s, 0x6217) = 1, M(&s, 0x6681) = 1; /* the hammer */
    uint8_t x = M(&s, 0x6203), y = M(&s, 0x6205);
    M(&s, 0x6700) = 1, M(&s, 0x6702) = 4, M(&s, 0x6703) = (uint8_t)(x + 30), M(&s, 0x6705) = (uint8_t)(y + 3);
    M(&s, 0x6707) = 0x15, M(&s, 0x6715) = 0, M(&s, 0x670F) = 4;
    uint32_t sc = dk_score(&s);
    for (int f = 0; f < 200; f++) dk_tick(&s, 0), M(&s, 0x6393) = 1;
    CHECK(dk_score(&s) - sc == 300, "a barrel smashed: %u points", dk_score(&s) - sc);
    static const uint32_t item_pts[4] = {0, 300, 500, 800};
    for (int level = 1; level <= 3; level++) {
      start_on(&s, 21, level, 0x3A6B); /* 50m, whatever the level */
      run_until_mode(&s, DK_MODE_PLAY, 4000);
      M(&s, 0x6203) = 0x50, M(&s, 0x6205) = 0xA0; /* beside the hat (0x53, 0xA0) */
      sc = dk_score(&s);
      tick_calm(&s, DK_RIGHT, 12);
      CHECK(dk_score(&s) - sc == item_pts[level], "level %d item: %u points", level, dk_score(&s) - sc);
    }
    play_on(&s, 23, 1, 4);
    M(&s, 0x6203) = 0x44, M(&s, 0x6205) = 0xC8; /* by the lowest left rivet */
    sc = dk_score(&s);
    tick_calm(&s, DK_RIGHT, 20);
    CHECK(dk_score(&s) - sc == 100 && M(&s, 0x6290) == 7, "a rivet: %u points, %d left", dk_score(&s) - sc, M(&s, 0x6290));
    tick_calm(&s, DK_LEFT, 20); /* back over its hole: down he goes */
    CHECK(M(&s, 0x6216) || !dk_alive(&s) || dk_mode(&s) != DK_MODE_PLAY, "walked back over a rivet's hole");
    play_on(&s, 25, 1, 4);
    M(&s, 0x60B2) = 0x00, M(&s, 0x60B3) = 0x69, M(&s, 0x60B4) = 0x00; /* 6900 */
    int lives = dk_lives(&s);
    M(&s, 0x6203) = 0x44, M(&s, 0x6205) = 0xC8;
    tick_calm(&s, DK_RIGHT, 20); /* + 100 */
    CHECK(dk_score(&s) == 7000 && dk_lives(&s) == lives + 1, "at %u, lives %d to %d", dk_score(&s), lives, dk_lives(&s));
  }
  /* the hammer lasts 512 frames */
  {
    play_on(&s, 27, 1, 1);
    M(&s, 0x6217) = 1, M(&s, 0x6681) = 1;
    int f = 0;
    while (M(&s, 0x6217) && f < 1000) tick_calm(&s, 0, 1), f++;
    CHECK(f == 512, "the hammer lasted %d frames", f);
  }
  /* the first barrel of a board is wild and blue, and lights the drum (on level 1 it falls straight down onto
     where Mario starts: he steps aside) */
  {
    play_on(&s, 29, 1, 1);
    int f = 0;
    while (!(M(&s, 0x6700) & 1) && f < 300) dk_tick(&s, DK_RIGHT), f++;
    CHECK(M(&s, 0x6701) == 1 && M(&s, 0x6715) == 1, "first barrel: wild %d blue %d", M(&s, 0x6701), M(&s, 0x6715));
    for (f = 0; f < 900 && !M(&s, 0x6348); f++) {
      for (int i = 1; i < 10; i++) M(&s, 0x6700 + i * 0x20) = 0; /* (only the first barrel) */
      dk_tick(&s, f < 60 ? DK_RIGHT : 0);
    }
    CHECK(M(&s, 0x6348) == 1, "the drum wasn't lit");
  }
  /* fires move every other frame at difficulty 1, 7 frames in 8 at 5 */
  {
    int moved[2] = {0};
    for (int k = 0; k < 2; k++) {
      play_on(&s, 31, k ? 5 : 1, 3); /* 75m starts with two fires */
      for (int f = 0; f < 256; f++) {
        uint8_t fx = M(&s, 0x640E), fy = M(&s, 0x640F), keep_x = M(&s, 0x6203);
        dk_tick(&s, 0);
        M(&s, 0x6203) = keep_x;
        moved[k] += M(&s, 0x640E) != fx || M(&s, 0x640F) != fy;
      }
    }
    CHECK(moved[0] < moved[1], "fires moved %d times at difficulty 1, %d at 5", moved[0], moved[1]);
  }
  /* a broken state is refused */
  {
    play_on(&s, 33, 1, 1);
    CHECK(dk_valid(&s), "a fresh game is refused");
    static dk_state b;
    b = s, b.ram[0x00A] = 0x42;
    CHECK(!dk_valid(&b), "GameMode2 0x42 accepted");
    b = s, b.ram[0x22A] = 0;
    CHECK(!dk_valid(&b), "a broken boards' order accepted");
  }
}

/* ---------------------------------------------------------------- a bot: beam search through the real game
 * Each board has a route: the ladders to take, the platforms to reach, the rivets to cross. Every 4 frames
 * each kept game is tried with each move held for 4 frames; the games where Mario lives and gets furthest
 * along the route are kept, at most a few per place Mario is in. A jump that can't end well is dropped at
 * once. The game is deterministic, so the moves found replay exactly. */
/* kind: 0 a ladder's foot, 1 its top, 2 pass going right, 3 left, 4 ride an elevator down past y */
typedef struct { uint8_t x, y, kind; } waypoint;
static const waypoint route25[] = {{0xCB, 0xEB, 0}, {0xCB, 0xD3, 1}, {0x73, 0xCE, 0}, {0x73, 0xAE, 1}, {0x83, 0xAD, 0},
                                   {0x83, 0x8D, 1}, {0x5B, 0x8A, 0}, {0x5B, 0x6E, 1}, {0xCB, 0x67, 0}, {0xCB, 0x4F, 1},
                                   {0x93, 0x4C, 0}, {0x93, 0x30, 1}};
static const waypoint route50[] = {{0x63, 0xF0, 0}, {0x63, 0xC8, 1}, {0x53, 0xC8, 0}, {0x53, 0xA0, 1},
                                   {0x63, 0xA0, 0}, {0x63, 0x78, 1}, {0x23, 0x78, 0}, {0x23, 0x50, 1}};
static const waypoint route75[] = {{0x1B, 0xE0, 0}, {0x1B, 0xB8, 1}, {0x23, 0xB8, 0}, {0x23, 0x80, 1}, {0x4C, 0x80, 2},
                                   {0x77, 0xC4, 4}, {0x8C, 0xE0, 2}, {0xAE, 0xD8, 2}, {0xC4, 0xD0, 2}, {0xE3, 0xC8, 0}, {0xE3, 0xB0, 1},
                                   {0xCB, 0xA8, 0}, {0xCB, 0x88, 1}, {0xE3, 0x88, 0}, {0xE3, 0x60, 1}, {0xD2, 0x68, 3},
                                   {0xB3, 0x70, 0}, {0xB3, 0x50, 1}, {0x93, 0x50, 0}, {0x93, 0x30, 1}};
/* (Kong stands in the middle of the top floor: its two rivets are reached from each side) */
static const waypoint route100[] = {{0x1B, 0xF0, 0}, {0x1B, 0xC8, 1}, {0x4E, 0xC8, 2}, {0xB6, 0xC8, 2}, {0xDB, 0xC8, 0},
                                    {0xDB, 0xA0, 1}, {0xB0, 0xA0, 3}, {0x48, 0xA0, 3}, {0x2B, 0xA0, 0}, {0x2B, 0x78, 1},
                                    {0x4E, 0x78, 2}, {0xB6, 0x78, 2}, {0xCB, 0x78, 0}, {0xCB, 0x50, 1}, {0xB0, 0x50, 3},
                                    {0xCB, 0x50, 0}, {0xCB, 0x78, 1}, {0x53, 0x78, 0}, {0x53, 0x50, 1}, {0x48, 0x50, 3}};
static const waypoint *const routes[5] = {0, route25, route50, route75, route100};
static const int route_len[5] = {0, 12, 8, 20, 20};

static bool reached(const dk_state *s, const waypoint *w) {
  int x = M(s, 0x6203), dy = M(s, 0x6205) - w->y;
  if (w->kind == 4) return M(s, 0x6398) && x >= 0x6C && x < 0x83 && dy >= 0;
  if (M(s, 0x6216)) return false;
  switch (w->kind) {
  case 0: return ((x | 3) & ~4) == w->x && dy == 0;
  case 1: return dy == 0 && !M(s, 0x6215);
  case 2: return x >= w->x && dy >= -6 && dy <= 6;
  default: return x <= w->x && dy >= -6 && dy <= 6;
  }
}

static int route_score(const dk_state *s, int screen, uint8_t *wp) {
  const waypoint *r = routes[screen];
  while (*wp < route_len[screen] && reached(s, &r[*wp])) (*wp)++;
  int score = *wp * 1000;
  if (*wp < route_len[screen]) {
    int y = M(s, 0x6216) && !M(s, 0x621F) ? M(s, 0x620E) : M(s, 0x6205); /* (a jump on its way up is no higher) */
    int dx = abs(M(s, 0x6203) - r[*wp].x), dy = abs(y - r[*wp].y);
    int k = r[*wp].kind;
    score -= k == 2 || k == 3 ? 2 * dx + dy / 4 : dx + 2 * dy; /* to a platform: across first */
  }
  if (screen == 4) score += (8 - M(s, 0x6290)) * 3000;
  return score;
}

/* in the air nothing can be changed: does this jump (or fall) end well? */
static bool lands_safely(const dk_state *s) {
  static dk_state t;
  t = *s;
  for (int f = 0; f < 64 && M(&t, 0x6216); f++) {
    dk_tick(&t, 0);
    if (dk_mode(&t) == DK_MODE_CLEAR) return true;
    if (dk_mode(&t) != DK_MODE_PLAY || !dk_alive(&t) || M(&t, 0x6220)) return false;
  }
  for (int f = 0; f < 8; f++) dk_tick(&t, 0);
  return dk_mode(&t) == DK_MODE_CLEAR || (dk_mode(&t) == DK_MODE_PLAY && dk_alive(&t));
}

static bool same_place(const dk_state *a, const dk_state *b) {
  return M(a, 0x6203) == M(b, 0x6203) && M(a, 0x6205) == M(b, 0x6205) && M(a, 0x6216) == M(b, 0x6216) &&
         M(a, 0x6215) == M(b, 0x6215) && M(a, 0x620F) == M(b, 0x620F) && M(a, 0x6222) == M(b, 0x6222) &&
         M(a, 0x6398) == M(b, 0x6398) && M(a, 0x621E) == M(b, 0x621E) &&
         (!M(a, 0x6216) || (M(a, 0x6214) == M(b, 0x6214) && M(a, 0x6210) == M(b, 0x6210)));
}

#define BEAM 72
#define MOVES 8
#define MAX_STEPS 1200
static const uint8_t moves[MOVES] = {0, DK_RIGHT, DK_LEFT, DK_UP, DK_DOWN, DK_JUMP, DK_JUMP | DK_RIGHT, DK_JUMP | DK_LEFT};
typedef struct { dk_state s; int score; uint8_t wp, parent, move; uint32_t hazards; } bot_node;
static bot_node beam[BEAM], kids[BEAM * MOVES];
static uint8_t hist_parent[MAX_STEPS + 1][BEAM], hist_move[MAX_STEPS + 1][BEAM];
static uint8_t bot_path[MAX_STEPS];
static int cmp_node(const void *a, const void *b) { return ((const bot_node *)b)->score - ((const bot_node *)a)->score; }
static void hold(dk_state *s, uint8_t move) {
  for (int f = 0; f < 4; f++) dk_tick(s, f ? move & ~DK_JUMP : move);
}

/* the moves (one per 4 frames) that clear the board from here, into bot_path: their count, or -1 */
static int bot_board(const dk_state *start) {
  int screen = dk_screen(start), n = 1;
  beam[0].s = *start, beam[0].wp = 0, beam[0].score = 0;
  for (int step = 0; step < MAX_STEPS; step++) {
    int m = 0;
    for (int i = 0; i < n; i++)
      for (int k = 0; k < MOVES; k++) {
        bot_node *c = &kids[m];
        c->s = beam[i].s, c->wp = beam[i].wp, c->parent = (uint8_t)i, c->move = moves[k];
        hold(&c->s, moves[k]);
        int mode = dk_mode(&c->s);
        if (mode == DK_MODE_CLEAR) {
          bot_path[step] = moves[k];
          for (int t = step, at = i; t > 0; t--) bot_path[t - 1] = hist_move[t][at], at = hist_parent[t][at];
          return step + 1;
        }
        if (mode != DK_MODE_PLAY || !dk_alive(&c->s) || M(&c->s, 0x6220)) continue;
        if (M(&c->s, 0x6216) && !lands_safely(&c->s)) continue;
        c->score = route_score(&c->s, screen, &c->wp) * 16 + (int)((uint32_t)(i * 7 + k * 13) % 16);
        c->hazards = dk_sum(&M(&c->s, 0x6400), 0x500);
        m++;
      }
    if (!m) {
      if (getenv("BOTDBG"))
        for (int i = 0; i < n && i < 6; i++) {
          const dk_state *a = &beam[i].s;
          printf("   dead end %d: wp %d mario %02X,%02X jump %d lad %d ride %d |", i, beam[i].wp, M(a, 0x6203), M(a, 0x6205),
                 M(a, 0x6216), M(a, 0x6215), M(a, 0x6398));
          for (int e = 0; e < 5; e++) if (M(a, 0x6400 + e * 32)) printf(" f%02X,%02X", M(a, 0x6403 + e * 32), M(a, 0x6405 + e * 32));
          for (int e = 0; e < 10; e++) if (M(a, 0x6500 + e * 16) & 1) printf(" s%02X,%02X", M(a, 0x6503 + e * 16), M(a, 0x6505 + e * 16));
          printf("\n");
        }
      return -1;
    }
    qsort(kids, (size_t)m, sizeof kids[0], cmp_node);
    n = 0;
    for (int i = 0; i < m && n < BEAM; i++) { /* a few games per place Mario is in, with different hazards */
      bool skip = false;
      int alike = 0;
      for (int j = 0; j < n && !skip; j++)
        if (same_place(&beam[j].s, &kids[i].s)) alike++, skip = alike >= 4 || beam[j].hazards == kids[i].hazards;
      if (skip) continue;
      hist_parent[step + 1][n] = kids[i].parent, hist_move[step + 1][n] = kids[i].move;
      beam[n++] = kids[i];
    }
    if (getenv("BOTDBG") && step % 100 == 99) {
      printf("  step %d:", step + 1);
      for (int i = 0; i < n && i < 5; i++)
        printf(" [wp%d %02X,%02X j%d l%d r%d 620F %d 621A %d]", beam[i].wp, M(&beam[i].s, 0x6203), M(&beam[i].s, 0x6205),
               M(&beam[i].s, 0x6216), M(&beam[i].s, 0x6215), M(&beam[i].s, 0x6398), M(&beam[i].s, 0x620F), M(&beam[i].s, 0x621A));
      printf("\n");
    }
  }
  return -1;
}

static void bots(void) {
  static dk_state s;
  static const char *const names[5] = {"", "25m", "50m", "75m", "100m"};
  for (int level = 1; level <= 5; level += 2)
    for (int b = 1; b <= 4; b++) {
      int steps = -1;
      for (uint32_t seed = 1; seed <= 4 && steps < 0; seed++) {
        play_on(&s, seed * 2654435761u + (uint32_t)(b * 31 + level), level, b);
        steps = bot_board(&s);
      }
      CHECK(steps > 0, "the bot never cleared %s on level %d", names[b], level);
      if (steps > 0) printf("bot: level %d %s cleared in %d frames\n", level, names[b], steps * 4);
    }
}

/* the bot plays a whole game from the start, through the intro and the scenes between the boards */
static int game_boards, game_levels;
static void bot_game(dk_state *s, uint32_t seed, int boards, void (*each)(const dk_state *)) {
  dk_new_game(s, seed);
  game_boards = 0;
  while (game_boards < boards && !s->over) {
    while (dk_mode(s) != DK_MODE_PLAY && !s->over) {
      dk_tick(s, 0);
      if (each) each(s);
    }
    if (s->over) break;
    int steps = bot_board(s);
    for (int wait = 0; steps < 0 && wait < 3 && dk_mode(s) == DK_MODE_PLAY; wait++) { /* no way: wait a little */
      static dk_state t;
      t = *s;
      for (int f = 0; f < 24 && dk_mode(&t) == DK_MODE_PLAY && dk_alive(&t); f++) dk_tick(&t, 0);
      if (dk_mode(&t) != DK_MODE_PLAY || !dk_alive(&t)) break;
      *s = t;
      steps = bot_board(s);
    }
    if (steps < 0) { /* no way through from here: Mario loses this life */
      while (dk_mode(s) == DK_MODE_PLAY) dk_tick(s, 0);
      continue;
    }
    for (int i = 0; i < steps; i++)
      for (int f = 0; f < 4; f++) {
        dk_tick(s, f ? bot_path[i] & ~DK_JUMP : bot_path[i]);
        if (each) each(s);
      }
    CHECK(dk_mode(s) == DK_MODE_CLEAR, "the bot's moves didn't replay (mode %X)", dk_mode(s));
    game_boards++;
  }
  game_levels = dk_level(s);
}

static void whole_game(void) {
  static dk_state s;
  bot_game(&s, 4242, 9, 0); /* level 1 (2 boards), 2 (3), 3 (4) */
  run_until_mode(&s, DK_MODE_PLAY, 4000);
  game_levels = dk_level(&s);
  CHECK(game_boards == 9 && game_levels == 4, "the bot's game: %d boards, level %d", game_boards, game_levels);
  CHECK(dk_score(&s) > 20000, "the bot's game: %u points", dk_score(&s));
  printf("bot: a whole game, 9 boards to level %d, %u points, %d lives left\n", game_levels, dk_score(&s), dk_lives(&s));
}

/* ---------------------------------------------------------------- saves */
static void saves(void) {
  static dk_state s, back;
  dk_profile p = {12300, 7, 21, 3, 1, 1}, q;
  play_on(&s, 77, 2, 2);
  for (int i = 0; i < 700; i++) dk_tick(&s, (uint8_t)(i % 90 < 40 ? DK_RIGHT : i % 90 < 50 ? DK_JUMP : 0));
  forget(DK_SAVE_NAME), forget(DK_COPY_NAME);
  CHECK(save_write(&p, &s), "save failed");
  memset(&back, 0, sizeof back);
  save_load(&q, &back);
  CHECK(q.best == 12300 && q.games == 7 && q.boards == 21 && q.best_level == 3 && q.has_run == 1, "profile back");
  CHECK(!memcmp(back.ram, s.ram, DK_SNAP) && !memcmp(back.vram, s.vram, 0x400) && back.loop == s.loop &&
            back.palette == s.palette, "game back");
  for (int i = 0; i < 1200; i++) { /* the same game goes on the same way */
    uint8_t j = (uint8_t)(i % 70 < 30 ? DK_LEFT : i % 70 < 34 ? DK_JUMP : DK_UP);
    dk_tick(&s, j), dk_tick(&back, j);
  }
  CHECK(!memcmp(back.ram, s.ram, DK_SNAP) && !memcmp(back.vram, s.vram, 0x400), "the game went another way after loading");
  /* the file gone: the copy in the script brings it back */
  forget(DK_SAVE_NAME);
  memset(&q, 0, sizeof q);
  save_load(&q, &back);
  CHECK(q.best == 12300 && q.has_run, "from the copy");
  uint32_t n;
  CHECK(plat_load(DK_SAVE_NAME, &n) != 0, "the save restored from the copy");
  /* every byte damaged in turn: refused, never a crash */
  forget(DK_COPY_NAME);
  save_write(&p, &s);
  forget(DK_COPY_NAME);
  static uint8_t keep[DK_SAVE_MAX], bad[DK_SAVE_MAX];
  const uint8_t *d = plat_load(DK_SAVE_NAME, &n);
  memcpy(keep, d, n);
  int refused = 0;
  for (uint32_t i = 0; i < n; i++) {
    memcpy(bad, keep, n);
    bad[i] ^= (uint8_t)(1 + i % 255);
    plat_save(DK_SAVE_NAME, bad, n);
    forget(DK_COPY_NAME);
    memset(&q, 0xAB, sizeof q);
    save_load(&q, &back);
    refused += q.best == 0 && q.has_run == 0;
  }
  CHECK(refused == (int)n, "damaged saves refused: %d of %u", refused, n);
  plat_save(DK_SAVE_NAME, keep, n - 1), save_load(&q, &back);
  CHECK(q.best == 0, "a short save read");
  plat_save(DK_SAVE_NAME, keep, 3), save_load(&q, &back);
  CHECK(q.best == 0, "a 3-byte save read");
  /* garbage in the packed game, with a right checksum: refused by the unpacker or the checks */
  memcpy(bad, keep, n);
  for (uint32_t i = 24; i < n - 4; i++) bad[i] = (uint8_t)(i * 37);
  uint32_t h = dk_sum(bad, n - 4);
  for (int i = 0; i < 4; i++) bad[n - 4 + i] = (uint8_t)(h >> (8 * i));
  plat_save(DK_SAVE_NAME, bad, n), save_load(&q, &back);
  CHECK(!q.has_run, "a garbled game read");
  printf("saves: %u bytes (the game packed from %d), every damaged byte refused\n", n, DK_SNAP + 0x400 + 6);
}

/* ---------------------------------------------------------------- the whole app with random keys */
static uint32_t fuzz_rng = 77;
static uint32_t xr(void) {
  fuzz_rng ^= fuzz_rng << 13, fuzz_rng ^= fuzz_rng >> 17, fuzz_rng ^= fuzz_rng << 5;
  return fuzz_rng;
}
static uint32_t fuzz_keys(uint32_t f) {
  static uint32_t held, until;
  if (f == 1) until = 0;
  if (f >= until) {
    uint32_t x = xr();
    static const uint32_t pick[] = {0, JK_OK, JK_OK, JK_LEFT, JK_RIGHT, JK_UP, JK_DOWN, JK_BACK, JK_OK | JK_RIGHT,
                                    JK_OK | JK_LEFT, JK_UP | JK_RIGHT, JK_DOWN, JK_RIGHT, JK_LEFT, JK_UP, 0};
    held = pick[x % 16];
    until = f + 1 + (x >> 8) % 30;
  }
  return held;
}
static void fuzz(void) {
  int sessions = 300;
  for (int s = 0; s < sessions; s++) {
    if (s % 60 == 0) forget(DK_SAVE_NAME), forget(DK_COPY_NAME);
    memset(&run, 0, sizeof run), memset(&prof, 0, sizeof prof), memset(&view, 0, sizeof view);
    fuzz_rng = (uint32_t)s * 7919u + 1;
    script = fuzz_keys, frame_no = 0, quit_at = 100 + xr() % 3000;
    dk_app_main();
    uint32_t n;
    const uint8_t *d = plat_load(DK_SAVE_NAME, &n);
    CHECK(d != 0, "session %d: no save", s);
    if (d) {
      dk_profile p;
      static dk_state t;
      static uint8_t copy_of[DK_SAVE_MAX];
      memcpy(copy_of, d, n);
      CHECK(dk_file_ok(copy_of, n, &p, &t), "session %d: the save doesn't read back", s);
      if (p.has_run) CHECK(dk_valid(&t), "session %d: a broken game was saved", s);
    }
  }
  script = 0;
  printf("fuzz: %d sessions of random keys, each quit with Home and its save read back\n", sessions);
}

/* ---------------------------------------------------------------- pictures */
static void picture(const dk_state *s, const char *dir, const char *name) {
  char path[512];
  snprintf(path, sizeof path, "%s/%s.ppm", dir, name);
  view.best = 18200, view.best_level = 3;
  dk_draw_invalidate();
  dk_draw(s, &view);
  shot(path);
}

/* the bot's game, a picture of each board a while into it */
static const char *shots_dir;
static int shot_marks, board_frames, last_board, scene_frames;
static void shot_each(const dk_state *s) {
  if (dk_mode(s) == DK_MODE_CLEAR) { /* the scenes after 25m and 100m */
    scene_frames++;
    if (game_boards == 1 && scene_frames == 150 && !(shot_marks & 64)) picture(s, shots_dir, "clear25"), shot_marks |= 64;
    if (game_boards == 2 && M(s, 0x6388) == 5 && M(s, 0x62AF) == 0xB0 && !(shot_marks & 128))
      picture(s, shots_dir, "clear100"), shot_marks |= 128;
  } else {
    scene_frames = 0;
  }
  static const struct { int board, frames; const char *name; } when[] = {
      {1, 420, "25m"}, {2, 600, "100m"}, {4, 300, "75m"}, {7, 420, "50m"}, {6, 700, "shot"}};
  if (game_boards != last_board) last_board = game_boards, board_frames = 0;
  if (dk_mode(s) == DK_MODE_PLAY) board_frames++;
  for (unsigned i = 0; i < sizeof when / sizeof when[0]; i++)
    if (!(shot_marks >> i & 1) && game_boards == when[i].board - 1 && board_frames == when[i].frames) {
      picture(s, shots_dir, when[i].name);
      shot_marks |= 1 << i;
    }
}

static void shots(const char *dir) {
  static dk_state s;
  memset(&view, 0, sizeof view);
  view.screen = DK_SCREEN_TITLE, view.clock = 40;
  picture(&s, dir, "title");
  view.screen = DK_SCREEN_GUIDE;
  picture(&s, dir, "guide");
  view.screen = DK_SCREEN_PLAY;
  dk_new_game(&s, 12345);
  for (int i = 0; i < 2000 && dk_mode(&s) != DK_MODE_HOWHIGH; i++) {
    dk_tick(&s, 0);
    if (i == 200) picture(&s, dir, "intro1");
    if (i == 420) picture(&s, dir, "intro2");
  }
  picture(&s, dir, "intro3");
  for (int i = 0; i < 40; i++) dk_tick(&s, 0);
  picture(&s, dir, "howhigh");
  shots_dir = dir, shot_marks = 0, last_board = -1;
  bot_game(&s, 4242, 9, shot_each);
  play_on(&s, 9, 1, 1); /* a barrel, and Mario's death */
  for (int f = 0; f < 3000 && dk_mode(&s) != DK_MODE_DEAD; f++) dk_tick(&s, 0);
  for (int f = 0; f < 0x40 + 8 * 5; f++) dk_tick(&s, 0);
  picture(&s, dir, "death");
  for (int f = 0; f < 20000 && !s.over; f++) dk_tick(&s, 0); /* the other lives, and GAME OVER */
  view.screen = DK_SCREEN_OVER, view.sel = 0, view.new_best = 0;
  picture(&s, dir, "over");
  view.screen = DK_SCREEN_PAUSE, view.sel = 0;
  picture(&s, dir, "pause");
}

static const char *film_dir;
static int film_frames, film_done, film_tick;
static void film_each(const dk_state *s) {
  static char path[512];
  if (film_done >= film_frames || film_tick++ % 2) return;
  snprintf(path, sizeof path, "%s/f_%04d.ppm", film_dir, film_done++);
  dk_draw(s, &view);
  shot(path);
}
static void film(const char *dir, int frames) {
  static dk_state s;
  memset(&view, 0, sizeof view);
  view.screen = DK_SCREEN_PLAY;
  film_dir = dir, film_frames = frames, film_done = 0, film_tick = 0;
  dk_draw_invalidate();
  bot_game(&s, 4242, 9, film_each);
}

int main(int argc, char **argv) {
  gfx_buf = strip;
  if (argc >= 2 && !strcmp(argv[1], "test")) {
    clock_t c0 = clock();
    units();
    clock_t c1 = clock();
    bots();
    whole_game();
    clock_t c2 = clock();
    saves();
    fuzz();
    fprintf(stderr, "(%.0f s rules, %.0f s bot, %.0f s saves and keys)\n", (double)(c1 - c0) / CLOCKS_PER_SEC,
            (double)(c2 - c1) / CLOCKS_PER_SEC, (double)(clock() - c2) / CLOCKS_PER_SEC);
    printf(fails ? "%d FAILED\n" : "all passed\n", fails);
    return fails != 0;
  }
  if (argc >= 3 && !strcmp(argv[1], "bot1")) { /* one board: bot1 LEVEL SCREEN [SEED] */
    static dk_state s;
    play_on(&s, argc > 4 ? (uint32_t)atoi(argv[4]) : 1, atoi(argv[2]), atoi(argv[3]));
    int steps = bot_board(&s);
    printf("%d steps, best waypoint %d at %02X,%02X\n", steps, beam[0].wp, M(&beam[0].s, 0x6203), M(&beam[0].s, 0x6205));
    return steps < 0;
  }
  if (argc >= 3 && !strcmp(argv[1], "shots")) {
    shots(argv[2]);
    return fails != 0;
  }
  if (argc >= 4 && !strcmp(argv[1], "film")) {
    film(argv[2], atoi(argv[3]));
    return 0;
  }
  fprintf(stderr, "usage: play test | shots DIR | film DIR N\n");
  return 2;
}
