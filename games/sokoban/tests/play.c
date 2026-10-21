/* Sokoban on a computer, for tests and pictures (make host, make test).
 *
 *   play check LEVELS SOLUTIONS
 *                          every level as the game opens it, against Microban as published
 *                          (levels/microban.txt): the same walls, goals, boxes and keeper; then played
 *                          through with its moves, when tools/solve.py found them (tests/solutions.txt),
 *                          and undone back to the start
 *   play shot L OUT [MOVES]  a picture of level L (from 1) after these moves (u r d l), with the
 *                          keeper half way through the last one
 *   play hub OUT [SOLVED]  a picture of the level select, the first SOLVED levels solved
 */
#define main sk_main
#include "../src/main.c"
#undef main
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
uint64_t eadk_keyboard_scan(void) { return 0; }
uint64_t eadk_timing_millis(void) { return 0; }
void eadk_timing_msleep(uint32_t ms) { (void)ms; }
uint32_t eadk_random(void) { return 0x9E3779B9u; }
void *np_alloc(uint32_t size) { return malloc(size); }

static void picture(const char *path) {
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

static int way(char c) {
  const char *p = strchr("urdl", c | 0x20);
  return p ? (int)(p - "urdl") : -1;
}

static char *slurp(const char *file) {
  static char texts[2][1 << 18];
  static int k;
  char *text = texts[k++ & 1];
  FILE *f = fopen(file, "r");
  size_t n = f ? fread(text, 1, sizeof texts[0] - 1, f) : 0;
  if (f) fclose(f);
  text[n] = 0;
  return text;
}

/* The level as the game opened it against Microban as published (its rows, the keeper on both): the
   same walls, goals, boxes and keeper, and floor only where Microban has no wall. */
static const char *differs(char pub[][40], int np) {
  int ky = -1, kx = 0;
  for (int y = 0; y < np; y++)
    for (int x = 0; pub[y][x]; x++)
      if (pub[y][x] == '@' || pub[y][x] == '+') ky = y, kx = x;
  if (ky < 0) return "no keeper in Microban";
  int dy = keeper / MAXW - ky, dx = keeper % MAXW - kx, boxes = 0, goals = 0;
  for (int i = 0; i < rows * MAXW; i++) { /* every cell of the game in Microban */
    int y = i / MAXW - dy, x = i % MAXW - dx, f = G[i];
    char c = y >= 0 && y < np && x >= 0 && x < (int)strlen(pub[y]) ? pub[y][x] : ' ';
    boxes += !!(f & F_BOX), goals += !!(f & F_GOAL);
    if (!!(f & F_WALL) != (c == '#')) return "a wall";
    if (c == '#' || !f) continue;
    if (!(f & F_FLOOR)) return "floor";
    if (!!(f & F_GOAL) != (c == '.' || c == '*' || c == '+') || !!(f & F_BOX) != (c == '$' || c == '*'))
      return "a box or a goal";
  }
  for (int y = 0; y < np; y++) /* and every wall, box and goal of Microban in the game */
    for (int x = 0; pub[y][x]; x++) {
      int gy = y + dy, gx = x + dx, f = gy >= 0 && gy < rows && gx >= 0 && gx < MAXW ? G[gy * MAXW + gx] : 0;
      if ((pub[y][x] == '#' && !(f & F_WALL)) || (strchr(".$*+", pub[y][x]) && !(f & F_FLOOR))) return "missing";
    }
  if (boxes != goals || !left || G[keeper] & F_BOX) return "boxes and goals";
  return 0;
}

/* plays a solution through: each step must go and each push push, the level must end solved; then
   every step is undone, back to the level as it began */
static const char *play(const char *m) {
  uint8_t start[sizeof g];
  memcpy(start, g, sizeof g);
  int start_left = left, start_keeper = keeper;
  for (const char *p = m; *p; p++) {
    int d = way(*p), n = moves, q = pushes;
    if (d < 0) continue;
    step(d);
    if (moves != n + 1 || pushes != q + (*p < 'a')) return "a step didn't go";
  }
  if (left) return "not solved";
  if (moves != nh) return "steps lost";
  while (nh) undo();
  if (memcmp(start, g, sizeof g) || left != start_left || keeper != start_keeper || moves || pushes) return "undo";
  return 0;
}

static int check(const char *levels_file, const char *solutions_file) {
  static char pub[MAXH + 8][40];
  static const char *solution[ALL_LEVELS + 1];
  char *sol = slurp(solutions_file), *text = slurp(levels_file);
  for (char *line = strtok(sol, "\n"); line; line = strtok(0, "\n")) {
    int l, at;
    if (sscanf(line, "%d:%n", &l, &at) >= 1 && l >= 1 && l <= ALL_LEVELS) solution[l] = line + at;
  }
  int level_n = 0, n = 0, bad = 0, solved = 0;
  for (char *line = text;; ) { /* the levels of microban.txt, an empty line after each */
    char *end = strchr(line, '\n');
    if (end) *end = 0;
    if (line[0] && line[0] != ';') {
      if (n < MAXH + 8) snprintf(pub[n++], sizeof pub[0], "%s", line);
    } else if (!line[0] && n) {
      open_level(level_n);
      const char *why = differs(pub, n);
      if (!why && solution[level_n + 1]) why = play(solution[level_n + 1]), solved += !why;
      if (why) printf("level %d: %s\n", level_n + 1, why), bad++;
      level_n++, n = 0;
    }
    if (!end) break;
    line = end + 1;
  }
  printf("%d levels as Microban has them; %d played through with their solutions and undone\n", level_n - bad, solved);
  return bad || level_n != LEVELS;
}

int main(int argc, char **argv) {
  gfx_buf = strip;
  if (argc >= 4 && !strcmp(argv[1], "check")) return check(argv[2], argv[3]);
  if (argc >= 4 && !strcmp(argv[1], "shot")) {
    open_level(atoi(argv[2]) - 1);
    const char *m = argc > 4 ? argv[4] : "";
    for (const char *p = m; *p; p++)
      if (way(*p) >= 0) step(way(*p));
    if (argc > 5) { /* and a panel on top: 1 paused, 2 quit, 3 how to play, 4 solved */
      ov = atoi(argv[5]);
      if (ov == OV_DONE) put(msg, T("Your best: "), (uint32_t)moves);
    }
    gfx_render(play_scene, 0, 0, SCREEN_H);
    picture(argv[3]);
    return 0;
  }
  if (argc >= 5 && !strcmp(argv[1], "film")) { /* film L DIR MOVES: a picture a step, then Solved! */
    char path[256];
    int n = 0;
    open_level(atoi(argv[2]) - 1);
    gfx_render(play_scene, 0, 0, SCREEN_H);
    snprintf(path, sizeof path, "%s/f_%04d.ppm", argv[3], n++);
    picture(path);
    for (const char *p = argv[4]; *p; p++) {
      if (way(*p) < 0) continue;
      step(way(*p));
      gfx_render(play_scene, 0, 0, SCREEN_H);
      snprintf(path, sizeof path, "%s/f_%04d.ppm", argv[3], n++);
      picture(path);
    }
    if (!left) {
      put(msg, T("Your best: "), (uint32_t)moves);
      ov = OV_DONE;
      gfx_render(play_scene, 0, 0, SCREEN_H);
      snprintf(path, sizeof path, "%s/f_%04d.ppm", argv[3], n++);
      picture(path);
    }
    printf("%d\n", n);
    return 0;
  }
  if (argc >= 3 && !strcmp(argv[1], "hub")) {
    for (int i = 0; i < (argc > 3 ? atoi(argv[3]) : 0); i++) sv.best[i] = (uint16_t)(20 + i);
    pick = argc > 3 && atoi(argv[3]) > 0 ? atoi(argv[3]) - 1 : 0;
    gfx_render(hub_scene, 0, 0, SCREEN_H);
    picture(argv[2]);
    return 0;
  }
  fprintf(stderr, "play check LEVELS SOLUTIONS | shot L OUT [MOVES [PANEL]] | film L DIR MOVES | hub OUT [SOLVED]\n");
  return 2;
}
