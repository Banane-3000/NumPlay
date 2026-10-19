/* Sokoban on a computer, for tests and pictures (make host, make test).
 *
 *   play solve FILE        every level played through with its moves (tests/solutions.txt, from
 *                          tools/solve.py): each step must go, each push push, the level must end
 *                          solved; then every step is undone, back to the level as it started
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

static int solve(const char *file) {
  static char text[1 << 18];
  FILE *f = fopen(file, "r");
  size_t n = f ? fread(text, 1, sizeof text - 1, f) : 0;
  if (f) fclose(f);
  text[n] = 0;
  int done = 0, bad = 0;
  for (char *line = strtok(text, "\n"); line; line = strtok(0, "\n")) {
    int l, at;
    if (line[0] == '#' || sscanf(line, "%d:%n", &l, &at) < 1) continue;
    open_level(l - 1);
    uint8_t start[sizeof g];
    memcpy(start, g, sizeof g);
    int boxes = 0, goals = 0, start_left = left, start_keeper = keeper, problem = 0;
    for (int i = 0; i < rows * MAXW; i++) boxes += !!(G[i] & F_BOX), goals += !!(G[i] & F_GOAL);
    if (boxes != goals || !boxes || !left || !(G[keeper] & F_FLOOR) || G[keeper] & F_BOX) problem = 1;
    for (const char *p = line + at; *p && !problem; p++) {
      int d = way(*p), m = moves, q = pushes;
      if (d < 0) continue;
      step(d);
      anim = 0;
      if (moves != m + 1 || pushes != q + (*p < 'a')) problem = 2;
    }
    if (!problem && left) problem = 3;
    if (!problem && moves != nh) problem = 4;
    while (!problem && nh) undo();
    if (!problem && (memcmp(start, g, sizeof g) || left != start_left || keeper != start_keeper || moves || pushes))
      problem = 5;
    if (problem) {
      printf("level %d: %s\n", l,
             (const char *[]){"", "not a level", "a step didn't go", "not solved", "steps lost", "undo"}[problem]);
      bad++;
    }
    done++;
  }
  printf("%d levels played, %d solved and undone%s\n", done, done - bad, done == LEVELS ? "" : ": some are missing");
  return bad || done != LEVELS;
}

int main(int argc, char **argv) {
  gfx_buf = strip;
  if (argc >= 3 && !strcmp(argv[1], "solve")) return solve(argv[2]);
  if (argc >= 4 && !strcmp(argv[1], "shot")) {
    open_level(atoi(argv[2]) - 1);
    const char *m = argc > 4 ? argv[4] : "";
    for (const char *p = m; *p; p++)
      if (way(*p) >= 0) step(way(*p)), anim = p[1] ? 0 : STEPS / 2;
    if (argc > 5) { /* and a panel on top: 1 paused, 2 quit, 3 how to play, 4 solved */
      ov = atoi(argv[5]);
      if (ov == OV_DONE) put(cat(put(msg, T("Moves "), (uint32_t)moves), "   "), T("Your best: "), (uint32_t)moves);
    }
    gfx_render(play_scene, 0, 0, SCREEN_H);
    picture(argv[3]);
    return 0;
  }
  if (argc >= 5 && !strcmp(argv[1], "film")) { /* film L DIR MOVES: two pictures a step, then Solved! */
    char path[256];
    int n = 0;
    open_level(atoi(argv[2]) - 1);
    gfx_render(play_scene, 0, 0, SCREEN_H);
    snprintf(path, sizeof path, "%s/f_%04d.ppm", argv[3], n++);
    picture(path);
    for (const char *p = argv[4]; *p; p++) {
      if (way(*p) < 0) continue;
      step(way(*p));
      for (int a = STEPS / 2; a >= 0; a -= STEPS / 2) {
        anim = a;
        gfx_render(play_scene, 0, 0, SCREEN_H);
        snprintf(path, sizeof path, "%s/f_%04d.ppm", argv[3], n++);
        picture(path);
      }
    }
    if (!left) {
      put(cat(put(msg, T("Moves "), (uint32_t)moves), "   "), T("Your best: "), (uint32_t)moves);
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
  fprintf(stderr, "play solve FILE | shot L OUT [MOVES] | hub OUT [SOLVED]\n");
  return 2;
}
