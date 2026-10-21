/* Sudoku on a computer, for tests and pictures (make host, make test).
 *
 *   play check        puzzles of every level made and checked: one solution, symmetric, the
 *                     right number of clues, solvable the way the level says; then a game played
 *                     with notes, mistakes, undo and hints, through to the end
 *   play film DIR     a game played with the keys, the screen saved as it goes (DIR/f_NNNN.ppm),
 *                     and the screens of the docs (DIR/shot_*.ppm)
 */
#define main sudoku_main
#include "../src/main.c"
#undef main
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static uint16_t fb[SCREEN_H][SCREEN_W];
void eadk_display_push_rect(eadk_rect_t r, const eadk_color_t *p) {
  for (int j = 0; j < r.height; j++)
    for (int i = 0; i < r.width; i++) fb[r.y + j][r.x + i] = p[j * r.width + i];
}
void eadk_display_push_rect_uniform(eadk_rect_t r, eadk_color_t c) {
  for (int j = 0; j < r.height; j++)
    for (int i = 0; i < r.width; i++) fb[r.y + j][r.x + i] = c;
}
bool eadk_display_wait_for_vblank(void) { return true; }
void *np_alloc(uint32_t size) { return malloc(size); }

/* the keys: a script of presses ("t key", one a line), each held 60 ms; the clock moves on as the
   game sleeps */
static uint64_t clock_ms;
static struct { uint64_t t; int key; } script[512];
static int nscript, nshots, frame;
static uint64_t shots[64];
static const char *dir;
uint64_t eadk_timing_millis(void) { return clock_ms; }
uint32_t eadk_random(void) { return 0x9E3779B9u; }
static void ppm(const char *path) {
  FILE *f = fopen(path, "wb");
  if (!f) return;
  fprintf(f, "P6 %d %d 255\n", SCREEN_W, SCREEN_H);
  for (int y = 0; y < SCREEN_H; y++)
    for (int x = 0; x < SCREEN_W; x++) {
      uint16_t c = fb[y][x];
      fputc((c >> 11) * 255 / 31, f), fputc((c >> 5 & 63) * 255 / 63, f), fputc((c & 31) * 255 / 31, f);
    }
  fclose(f);
}
void eadk_timing_msleep(uint32_t ms) {
  for (uint32_t i = 0; i < ms; i++) {
    clock_ms++;
    char path[512];
    for (int s = 0; s < nshots; s++)
      if (shots[s] == clock_ms) snprintf(path, sizeof path, "%s/shot_%d.ppm", dir, s), ppm(path);
    if (dir && clock_ms % 40 == 0) snprintf(path, sizeof path, "%s/f_%04d.ppm", dir, frame++), ppm(path);
  }
}
uint64_t eadk_keyboard_scan(void) {
  uint64_t k = 0;
  for (int i = 0; i < nscript; i++)
    if (clock_ms >= script[i].t && clock_ms < script[i].t + 60) k |= 1ull << script[i].key;
  return k;
}

static int fails;
#define CHECK(c, ...) do { if (!(c) && fails++ < 12) { printf("FAIL: " __VA_ARGS__); printf("\n"); } } while (0)

static bool valid_solution(const uint8_t *s) {
  for (int u = 0; u < 27; u++) {
    int m = 0;
    for (int i = 0; i < 9; i++) m |= 1 << s[at(u, i)];
    if (m != 0x3FE) return false;
  }
  return true;
}

static void check_puzzles(void) {
  static const int count[4] = {40, 40, 20, 12};
  for (int diff = 0; diff < 4; diff++) {
    int lo = 81, hi = 0, levels_seen[4] = {0};
    clock_t t0 = clock();
    for (int n = 0; n < count[diff]; n++) {
      seed = 0x1234567u * (uint32_t)(n + 1) + (uint32_t)diff * 7919u;
      sv.diff = (uint8_t)diff;
      generate();
      uint8_t p[81];
      int clues = 0;
      for (int c = 0; c < 81; c++) {
        p[c] = sv.val[c] & 15;
        clues += p[c] != 0;
        CHECK(!p[c] || sv.val[c] > 15, "a clue not marked as one");
        CHECK(!p[c] || p[c] == sv.sol[c], "a clue that isn't the solution's");
      }
      for (int c = 0; c < 81; c++) CHECK(!p[c] == !p[80 - c], "not symmetric (level %d, puzzle %d)", diff, n);
      CHECK(valid_solution(sv.sol), "the solution isn't one");
      CHECK(solutions(p) == 1, "not one solution (level %d, puzzle %d)", diff, n);
      int lv = logic(p);
      levels_seen[lv]++;
      if (diff < 2) CHECK(lv == 1, "level %d puzzle %d needs more than singles", diff, n);
      if (diff == 0) CHECK(clues >= 36, "too few clues for Easy (%d)", clues);
      if (diff == 2) CHECK(lv == 2, "Hard puzzle %d isn't Hard (%d)", n, lv);
      lo = clues < lo ? clues : lo, hi = clues > hi ? clues : hi;
    }
    printf("%-6s %2d puzzles, %d to %d clues, by singles %d, with pointing %d, beyond %d, %.0f ms each\n",
           LEVEL(diff), count[diff], lo, hi, levels_seen[1], levels_seen[2], levels_seen[3],
           (double)(clock() - t0) * 1000 / CLOCKS_PER_SEC / count[diff]);
    if (diff == 3) CHECK(levels_seen[3] * 2 >= count[3], "most Expert puzzles should need more than pointing");
  }
}

/* a game: notes, a mistake, undo, then hints to the end */
static void check_play(void) {
  seed = 99, sv.diff = 1, sv.flags = 1, ucount = 0, sel = 0;
  generate();
  int c = 0;
  while (sv.val[c]) c++;
  int wrong = sv.sol[c] % 9 + 1, other = 80;
  while (sv.val[other] || !sees(c, other) || other == c) other--;
  sv.notes[other] = (uint16_t)(1 << wrong | 1 << sv.sol[c]);
  write(c, wrong);
  CHECK(sv.val[c] == wrong && sv.mistakes == 1, "a wrong digit isn't counted");
  CHECK(!(sv.notes[other] >> wrong & 1), "the digit's note stays in a cell that sees it");
  undo_move();
  CHECK(sv.val[c] == 0 && sv.notes[other] >> wrong & 1, "undo doesn't bring back the cell and the note");
  sel = c;
  for (int i = 0; i < 81 && (sv.flags & 1); i++) hint();
  int right = 0;
  for (int k = 0; k < 81; k++) right += (sv.val[k] & 15) == sv.sol[k];
  CHECK(right == 81 && !(sv.flags & 1) && ov == 4, "hints don't finish the game");
  CHECK(sv.best[1] == sv.secs, "the best time isn't kept");
}

/* A film of a game: how to play, the levels, then a Medium game played with the keys: notes in
   two cells, a digit that takes a note away, a mistake undone, hints, the menu. The puzzle is the
   one the game will make (the same seed), so the keys can go where they need to. */
static uint64_t ft;
static int fsel;
static void press(int key, int wait) { script[nscript].t = ft, script[nscript++].key = key, ft += (uint64_t)wait; }
static int digit_code(int d) { return 42 - 6 * ((d - 1) / 3) + (d - 1) % 3; }
static void go(int c) { /* the arrows to cell c */
  while (fsel % 9 < c % 9) press(eadk_key_right, 160), fsel++;
  while (fsel % 9 > c % 9) press(eadk_key_left, 160), fsel--;
  while (fsel / 9 < c / 9) press(eadk_key_down, 160), fsel += 9;
  while (fsel / 9 > c / 9) press(eadk_key_up, 160), fsel -= 9;
  ft += 200;
}
static void film(void) {
  seed = 0x9E3779B9u, sv.diff = 1;
  generate();
  uint8_t sol[81], val[81];
  memcpy(sol, sv.sol, 81), memcpy(val, sv.val, 81);
  memset(&sv, 0, sizeof sv);
  /* two empty cells in the same box, near the middle */
  int a = -1, b = -1;
  for (int i = 0; i < 81 && b < 0; i++) {
    int c = (40 + i) % 81;
    if (val[c]) continue;
    if (a < 0) a = c;
    else if (unit[c][2] == unit[a][2] && sol[c] != sol[a]) b = c;
  }
  int other = sol[a] % 9 + 1 == sol[b] ? (sol[a] + 1) % 9 + 1 : sol[a] % 9 + 1; /* a note that isn't b's */
  ft = 1500, fsel = 40;
  shots[nshots++] = 1400; /* how to play */
  press(eadk_key_ok, 400);
  press(eadk_key_down, 300);
  shots[nshots++] = ft;   /* the levels */
  press(eadk_key_ok, 900);
  go(a);
  press(eadk_key_ok, 300); /* notes */
  press(digit_code(sol[a]), 250), press(digit_code(other), 250), press(digit_code(sol[b]), 300);
  go(b);
  press(digit_code(sol[a]), 250), press(digit_code(sol[b]), 300);
  press(eadk_key_ok, 400); /* notes off */
  shots[nshots++] = ft;   /* notes */
  press(digit_code(sol[b] % 9 + 1), 900); /* a mistake */
  shots[nshots++] = ft - 200;
  press(eadk_key_backspace, 500);        /* undone */
  press(digit_code(sol[b]), 900);         /* right: b's digit leaves a's notes */
  go(a);
  press(eadk_key_shift, 700);             /* a hint */
  press(eadk_key_shift, 900);
  shots[nshots++] = ft - 200;             /* the game */
  press(eadk_key_back, 1200);
  shots[nshots++] = ft - 300;             /* the menu */
  press(eadk_key_back, 800);
  press(eadk_key_home, 300);
  sudoku_main();
}

/* The launcher's card (192 x 144): the game's screen drawn small and sharp rather than shrunk, so
   it packs into little flash: a game of the level asked as it starts, a cell picked. */
static int card_at(int i) { return 3 + 2 + 15 * i + i / 3; }
static int card_pad = 1; /* the keypad's digits on the card too */
static void card_scene(void *ctx) {
  char s[8];
  int pick = sel < 0 ? 0 : sv.val[sel] & 15;
  gfx_fill(0, 0, SCREEN_W, SCREEN_H, WHITE);
  gfx_fill(3, 3, 140, 140, NAVY);
  for (int b = 0; b < 9; b++) gfx_fill(card_at(b % 3 * 3), card_at(b / 3 * 3), 44, 44, HEX(0xBEC6D4));
  for (int c = 0; c < 81; c++) {
    int x = card_at(c % 9), y = card_at(c / 9), v = sv.val[c] & 15;
    gfx_fill(x, y, 14, 14, sel < 0 ? WHITE : c == sel ? PICK : pick && v == pick ? SAME : sees(c, sel) ? PALE : WHITE);
    if (v) s[0] = (char)('0' + v), s[1] = 0, gfx_text_center(&np_font_body, x + 7, y + 12, s, sv.val[c] > 15 ? NAVY : BLUE, 256);
  }
  gfx_text_center(&np_font_small, 168, 14, LEVEL(sv.diff), GREY, 256);
  gfx_text_center(&np_font_body, 168, 30, "4:05", NAVY, 256);
  for (int d = 1; d <= 9 && card_pad; d++) {
    int x = 148 + (d - 1) % 3 * 14, y = 40 + (2 - (d - 1) / 3) * 16;
    gfx_rrect(x, y, 13, 15, 3, d == pick ? BLUE : TILE, 256);
    s[0] = (char)('0' + d), s[1] = 0;
    gfx_text_center(&np_font_small, x + 7, y + 11, s, d == pick ? WHITE : NAVY, 256);
  }
}
static void card(const char *path, int diff, int pick, int written, uint32_t from) {
  seed = from, sv.diff = (uint8_t)diff, sv.flags = 1 | 2, sv.secs = 0, sel = pick, card_pad = !(written & 2);
  generate();
  for (int c = 0; c < 81 && written & 1; c++)  /* a few digits already written */
    if (!sv.val[c] && (c * 7) % 5 == 0) sv.val[c] = sv.sol[c];
  gfx_buf = strip;
  gfx_render(card_scene, 0, 0, SCREEN_H);
  FILE *f = fopen(path, "wb");
  if (!f) return;
  fprintf(f, "P6 192 144 255\n");
  for (int y = 0; y < 144; y++)
    for (int x = 0; x < 192; x++) {
      uint16_t c = fb[y][x];
      fputc((c >> 11) * 255 / 31, f), fputc((c >> 5 & 63) * 255 / 63, f), fputc((c & 31) * 255 / 31, f);
    }
  fclose(f);
}

int main(int argc, char **argv) {
  units();
  if (argc > 5 && !strcmp(argv[1], "card")) { /* card PATH LEVEL CELL FLAGS [SEED] */
    card(argv[2], atoi(argv[3]), atoi(argv[4]), atoi(argv[5]), argc > 6 ? (uint32_t)atol(argv[6]) : 4242);
    return 0;
  }
  if (argc > 2 && !strcmp(argv[1], "film")) {
    dir = argv[2];
    film();
    return 0;
  }
  check_puzzles();
  check_play();
  printf(fails ? "%d checks failed\n" : "all checks passed\n", fails);
  return fails != 0;
}
