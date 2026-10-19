/* Level Devil on a computer, for tests and pictures (make host, make test).
 *
 *   play solve            every room played through with its solutions (tests/solutions.txt): each
 *                         must reach the door without dying, as it starts and again after one, two
 *                         and three deaths (some rooms change)
 *   play search L R [D]   finds a way through a room (after D deaths), for tests/solutions.txt
 *   play trace L R [D]    where the runner goes with the solution
 *   play rooms DIR        a picture of every room as it starts (DIR/room_LL_R.ppm)
 *   play film L R DIR [D [KEYS]]  the solution of a room (after D deaths), or these keys, a picture
 *                         every 2 steps (DIR/f_NNNN.ppm)
 */
#define main ld_main
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

/* tests/solutions.txt: a line a room, "LEVEL-ROOM: keys", with " | " between the keys for each
   number of deaths when the room changes (as it starts, after 1, 2, 3); "R40 RJ12 R30": the keys
   held, L R J or N for none, for so many steps */
static char *solutions[ROOM_COUNT][4];
static void read_solutions(void) {
  static char text[1 << 16];
  FILE *f = fopen("tests/solutions.txt", "r");
  size_t n = f ? fread(text, 1, sizeof text - 1, f) : 0;
  if (f) fclose(f);
  text[n] = 0;
  for (char *line = strtok(text, "\n"); line; line = strtok(0, "\n")) {
    int l, r, at;
    if (line[0] == '#' || sscanf(line, "%d-%d:%n", &l, &r, &at) < 2) continue;
    int i = (l - 1) * ROOMS + r - 1;
    if (i < 0 || i >= ROOM_COUNT) continue;
    char *p = line + at;
    for (int t = 0; t < 4; t++) {
      solutions[i][t] = p;
      char *bar = strchr(p, '|');
      if (bar) *bar = 0, p = bar + 1;
    }
  }
  for (int i = 0; i < ROOM_COUNT; i++)
    for (int t = 1; t < 4; t++)
      if (!solutions[i][t] || solutions[i][t] == solutions[i][t - 1]) solutions[i][t] = solutions[i][t - 1];
  for (int i = 0; i < ROOM_COUNT; i++)
    for (int t = 0; t < 4; t++)
      if (!solutions[i][t]) solutions[i][t] = "";
}

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
static void draw(void) { gfx_render(play_scene, 0, 0, SCREEN_H); }

static void setup(int l, int r, int deaths) {
  static int ready;
  if (!ready) {
    gfx_buf = strip, seed = 1, ready = 1;
    if (np_inflate(rooms, ROOM_BYTES, rooms_z, sizeof rooms_z, (np_inflate_tables_t *)(void *)strip) != ROOM_BYTES)
      exit(3);
  }
  scr = SC_PLAY, ov = 0, level = l, rnum = r;
  enter();
  tries = deaths;
  reset();
  curtain = 0;
}

/* plays a solution ("R30 RJ12 N8": the keys held, L R J or N for none, for so many steps);
   1 the door reached, 0 dead, -1 neither */
static int run(const char *s, const char *film) {
  int frame = 0;
  char path[256];
  for (;;) {
    while (*s == ' ') s++;
    int in = 0, n = 240; /* (after the last keys, nothing for a while) */
    for (; *s >= 'A' && *s <= 'Z'; s++) in |= *s == 'L' ? I_LEFT : *s == 'R' ? I_RIGHT : *s == 'J' ? I_JUMP : 0;
    if (*s) {
      char *e;
      n = (int)strtol(s, &e, 10);
      if (e == s) return -1; /* (not keys) */
      s = e;
    }
    while (n--) {
      step(in);
      if (film && !(frame++ & 1)) {
        draw();
        snprintf(path, sizeof path, "%s/f_%04d.ppm", film, frame / 2);
        picture(path);
      }
      if ((won || dead) && film) /* the runner in pieces, or the door closing, a little longer */
        for (int k = 0; k < 44; k++) {
          step(0);
          if (!(frame++ & 1)) {
            draw();
            snprintf(path, sizeof path, "%s/f_%04d.ppm", film, frame / 2);
            picture(path);
          }
        }
      if (won || dead) return won ? 1 : 0;
    }
    if (!*s) return -1;
  }
}

/* ---- finding a solution: a best-first search over the keys held 4 steps at a time, from the room
   as it starts, through states not seen yet, the nearest to the door first */
typedef struct {
  obj_t ob[28];
  int no, face, ground, coyote, buffer, held_in, anim, mode, warped, sprung;
  int32_t px, py, vx, vy;
  uint32_t found;
} snap_t;
static void take(snap_t *s) {
  memcpy(s->ob, ob, sizeof ob);
  s->no = no, s->face = face, s->ground = ground, s->coyote = coyote, s->buffer = buffer, s->held_in = held_in;
  s->anim = anim, s->mode = mode, s->warped = warped, s->sprung = sprung, s->px = px, s->py = py, s->vx = vx, s->vy = vy;
  s->found = found;
}
static void give(const snap_t *s) {
  memcpy(ob, s->ob, sizeof ob);
  no = s->no, face = s->face, ground = s->ground, coyote = s->coyote, buffer = s->buffer, held_in = s->held_in;
  anim = s->anim, mode = s->mode, warped = s->warped, sprung = s->sprung, px = s->px, py = s->py, vx = s->vx, vy = s->vy;
  found = s->found, dead = won = 0;
}
static uint64_t key_of(void) { /* the state, roughly: the runner and every object */
  uint64_t h = (uint64_t)(px >> 8) * 1000003u ^ (uint64_t)(py >> 8) * 9176u ^ (uint64_t)((vx >> 7) + 64) * 131u ^
               (uint64_t)((vy >> 7) + 64) * 7u ^ (uint64_t)ground << 40 ^ (uint64_t)mode << 44 ^
               (uint64_t)(held_in & 4) << 50;
  for (int k = 0; k < no; k++)
    h = h * 1099511628211u ^ (uint64_t)(ob[k].st | ob[k].on << 2 | (ob[k].x >> 4) << 3 | (uint64_t)(ob[k].y >> 4) << 20);
  return h ? h : 1;
}
static int door_dist(void) { /* from the runner to the nearest real door, in 16ths of a pixel */
  int best = 1 << 30;
  for (int k = 0; k < no; k++)
    if ((ob[k].r[0] & 15) == K_DOOR && !(ob[k].r[0] & F_GHOST)) {
      int dx = ob[k].x + 88 - (int)(px >> 4) - 40, dy = ob[k].y + 200 - (int)(py >> 4) - 56;
      int d = (int)__builtin_sqrtf((float)(dx * dx + dy * dy));
      if (d < best) best = d;
    }
  return best;
}
#define SEEN (1 << 22)
#define NODES (1 << 20)
static uint64_t *seen;
static struct node {
  snap_t s;
  int parent, depth, f;
  uint8_t in;
} *nodes;
static int *heap, nheap;
static void push(int i) {
  int k = nheap++;
  for (; k && nodes[heap[(k - 1) / 2]].f > nodes[i].f; k = (k - 1) / 2) heap[k] = heap[(k - 1) / 2];
  heap[k] = i;
}
static int pop(void) {
  int top = heap[0], last = heap[--nheap], k = 0;
  for (;;) {
    int c = 2 * k + 1;
    if (c >= nheap) break;
    if (c + 1 < nheap && nodes[heap[c + 1]].f < nodes[heap[c]].f) c++;
    if (nodes[heap[c]].f >= nodes[last].f) break;
    heap[k] = heap[c], k = c;
  }
  heap[k] = last;
  return top;
}
static const uint8_t moves[6] = {I_RIGHT, I_RIGHT | I_JUMP, 0, I_LEFT, I_LEFT | I_JUMP, I_JUMP};
static int search(int i, int deaths) {
  if (!seen) seen = calloc(SEEN, 8), nodes = malloc(sizeof *nodes * NODES), heap = malloc(sizeof *heap * NODES);
  memset(seen, 0, (size_t)SEEN * 8);
  setup(i / ROOMS, i % ROOMS, deaths);
  int n = 1;
  take(&nodes[0].s), nodes[0].parent = -1, nodes[0].depth = 0, nodes[0].f = 0, nheap = 0;
  push(0);
  while (nheap && n < NODES - 8) {
    int at = pop();
    for (int m = 0; m < 6; m++) {
      give(&nodes[at].s);
      for (int k = 0; k < 4 && !dead && !won; k++) step(moves[m]);
      if (dead) continue;
      if (won) { /* the keys, back from here */
        static uint8_t ins[8192];
        int c = 0;
        ins[c++] = moves[m];
        for (int q = at; q > 0 && c < 8192; q = nodes[q].parent) ins[c++] = nodes[q].in;
        char out[16384], *o = out;
        for (int a = c - 1; a >= 0;) {
          int b = a;
          while (b >= 0 && ins[b] == ins[a]) b--;
          int in = ins[a];
          o += sprintf(o, "%s%s%s%s%d ", in & I_LEFT ? "L" : "", in & I_RIGHT ? "R" : "", in & I_JUMP ? "J" : "",
                       in ? "" : "N", 4 * (a - b));
          a = b;
        }
        o[-1] = 0;
        printf("%s\n", out);
        return 1;
      }
      uint64_t k = key_of();
      uint32_t h = (uint32_t)(k % SEEN);
      while (seen[h] && seen[h] != k) h = (h + 1) % SEEN;
      if (seen[h]) continue;
      seen[h] = k;
      struct node *nd = &nodes[n];
      take(&nd->s), nd->parent = at, nd->in = moves[m], nd->depth = nodes[at].depth + 1;
      nd->f = nd->depth + 3 * door_dist() / 70;
      push(n++);
    }
  }
  printf("no way found (%d states)\n", n);
  return 0;
}

int main(int argc, char **argv) {
  char path[256];
  read_solutions();
  if (!strcmp(argc > 1 ? argv[1] : "", "check") && argc > 5) { /* do these keys clear the room? */
    int i = (atoi(argv[2]) - 1) * ROOMS + atoi(argv[3]) - 1;
    setup(i / ROOMS, i % ROOMS, atoi(argv[4]));
    int ok = run(argv[5], 0) == 1;
    printf(ok ? "ok\n" : "no\n");
    return !ok;
  }
  if (!strcmp(argc > 1 ? argv[1] : "", "search") && argc > 3)
    return !search((atoi(argv[2]) - 1) * ROOMS + atoi(argv[3]) - 1, argc > 4 ? atoi(argv[4]) : 0);
  if (!strcmp(argc > 1 ? argv[1] : "", "trace") && argc > 3) { /* where the runner goes with the solution */
    int i = (atoi(argv[2]) - 1) * ROOMS + atoi(argv[3]) - 1;
    setup(i / ROOMS, i % ROOMS, argc > 4 ? atoi(argv[4]) : 0);
    const char *s = solutions[i][argc > 4 ? atoi(argv[4]) & 3 : 0];
    for (int t = 0;; t++) {
      while (*s == ' ') s++;
      if (!*s) break;
      int in = 0;
      for (; *s >= 'A' && *s <= 'Z'; s++) in |= *s == 'L' ? I_LEFT : *s == 'R' ? I_RIGHT : *s == 'J' ? I_JUMP : 0;
      for (int n = (int)strtol(s, (char **)&s, 10); n--; t++) {
        step(in);
        if (!(t % 4) || dead || won) printf("%4d x %4.1f y %4.1f %s%s%s\n", t, px / 256.0, py / 256.0, ground ? "ground" : "",
                                            dead ? " DEAD" : "", won ? " DOOR" : "");
        if (dead || won) return 0;
      }
    }
    return 0;
  }
  if (argc < 2) return 2;
  if (!strcmp(argv[1], "screens") && argc > 2) { /* the map, the menus and the end, with some progress */
    static const struct {
      int scr, ov, sel;
      const char *name;
    } screens[] = {{SC_HUB, OV_NONE, 0, "map"},  {SC_HUB, OV_HELP, 0, "help"},      {SC_PLAY, OV_PAUSE, 1, "pause"},
                   {SC_PLAY, OV_QUIT, 1, "quit"}, {SC_HUB, OV_END, 0, "end"},       {SC_HUB, OV_NONE, 0, "banner"}};
    setup(15, 4, 0);
    sv.open = 12, sv.done = 0x7FF, sv.deaths = 173, sv.sel = 6, sv.run = 4;
    for (int i = 0; i < LEVELS; i++) sv.best[i] = (uint16_t)(i * 3 % 7);
    for (unsigned i = 0; i < sizeof screens / sizeof screens[0]; i++) {
      scr = screens[i].scr, ov = screens[i].ov, sel = screens[i].sel, banner = i == 5 ? 4 : 0;
      gfx_render(scr == SC_HUB ? hub_scene : play_scene, 0, 0, SCREEN_H);
      snprintf(path, sizeof path, "%s/%s.ppm", argv[2], screens[i].name);
      picture(path);
    }
    return 0;
  }
  if (!strcmp(argv[1], "rooms") && argc > 2) {
    for (int i = 0; i < ROOM_COUNT; i++) {
      setup(i / ROOMS, i % ROOMS, 0);
      draw();
      snprintf(path, sizeof path, "%s/room_%02d_%d.ppm", argv[2], i / ROOMS + 1, i % ROOMS + 1);
      picture(path);
    }
    return 0;
  }
  if (!strcmp(argv[1], "film") && argc > 4) {
    int i = (atoi(argv[2]) - 1) * ROOMS + atoi(argv[3]) - 1;
    if (i < 0 || i >= ROOM_COUNT) return 2;
    setup(i / ROOMS, i % ROOMS, argc > 5 ? atoi(argv[5]) : 0);
    return run(argc > 6 ? argv[6] : solutions[i][argc > 5 ? atoi(argv[5]) & 3 : 0], argv[4]) == 1 ? 0 : 1;
  }
  if (!strcmp(argv[1], "solve")) {
    int bad = 0;
    for (int i = 0; i < ROOM_COUNT; i++)
      for (int t = 0; t < 4; t++) { /* as it starts, then after 1, 2 and 3 deaths */
        setup(i / ROOMS, i % ROOMS, t);
        int got = run(solutions[i][t], 0);
        if (got != 1) {
          printf("level %d room %d (after %d deaths): %s at x %d y %d\n", i / ROOMS + 1, i % ROOMS + 1, t,
                 got ? "stuck" : "dead", (int)(px >> 8), (int)(py >> 8));
          bad = 1;
          break;
        }
      }
    if (bad) printf("some rooms fail\n");
    else printf("all %d rooms solved\n", ROOM_COUNT);
    return bad;
  }
  return 2;
}
