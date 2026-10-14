/* Plays Undertale on a computer, with scripted keys, and saves frames.
 *
 *   play [--frames N] [--keys "10-40:right,50:z,..."] [--shots 30,60|all] [--out DIR] [--saves DIR]
 *        [--room N --at X,Y] [--name NAME] [--flag F=V]... [--battle ID] [--trace]
 *
 * --path "x,y;x,y;z;w30;..." steers Frisk to each point in turn (holding arrows), presses Z ("z"), X ("x"),
 * waits ("wN": N frames) or mashes Z through talks ("talk": until no text or event is left), when the scripted
 * keys don't hold anything.
 * Keys: left right up down z x c home. "a-b:key" holds the key from frame a to b, "a:key" for one frame.
 * --room starts a new game in room N (its ROOM_* number) with Frisk's top-left at (X, Y), past the title.
 * Screenshots are DIR/shot_<frame>.ppm. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include "../src/battle.h"
#include "../src/game.h"
#include "../src/gfx.h"
#include "../src/menu.h"
#include "../src/res.h"
#include "../src/world.h"

extern uint32_t host_keys, host_time;
extern uint16_t host_fb[SCREEN_W * SCREEN_H];
void host_shot(const char *path);
void host_save_dir(const char *d);
extern const uint8_t ut_data[];

typedef struct { int a, b; uint32_t k; } Hold;
static Hold holds[4096];
static int nholds;

#include "../src/event.h"
#include "../src/text.h"
static char path_buf[65536];
static char *path_at;
static int path_wait, path_stuck, path_last_x, path_last_y, path_room = -1;

/* the keys the path holds this frame (0: it's done or waiting) */
static uint32_t path_keys(int f) {
  if (!path_at || !*path_at) return 0;
  if (path_wait > 0) {
    path_wait--;
    return 0;
  }
  char *end = strchr(path_at, ';');
  char tok[64];
  int n = end ? (int)(end - path_at) : (int)strlen(path_at);
  snprintf(tok, sizeof tok, "%.*s", n, path_at);
  char *next = end ? end + 1 : path_at + n;
  if (!strcmp(tok, "z") || !strcmp(tok, "x") || !strcmp(tok, "c")) {
    path_at = next;
    path_wait = 1;
    return tok[0] == 'z' ? K_Z : tok[0] == 'x' ? K_X : K_C;
  }
  if (tok[0] == 'w') {
    path_wait = atoi(tok + 1);
    path_at = next;
    return 0;
  }
  if (!strcmp(tok, "talk")) {
    if (!text_active() && !event_running() && g_mode == MODE_WORLD && !world_fading()) {
      path_at = next;
      return 0;
    }
    return (f & 1) ? K_Z : 0;
  }
  int x, y;
  if (sscanf(tok, "%d,%d", &x, &y) != 2) {
    path_at = next;
    return 0;
  }
  if (g_mode != MODE_WORLD || text_active() || event_running() || world_fading()) return 0;
  /* a door took him to another room: that point is reached */
  if (path_room >= 0 && g_room != path_room) {
    path_room = g_room;
    path_at = next;
    return 0;
  }
  path_room = g_room;
  int dx = x - pl.x, dy = y - pl.y;
  if (abs(dx) <= 1 && abs(dy) <= 1) {
    path_at = next;
    path_stuck = 0;
    return 0;
  }
  if (pl.x == path_last_x && pl.y == path_last_y) {
    if (++path_stuck > 60) {
      fprintf(stderr, "path: stuck at %d,%d on the way to %d,%d (room %d)\n", pl.x, pl.y, x, y, g_room);
      path_at = next, path_stuck = 0;
    }
  } else
    path_stuck = 0;
  path_last_x = pl.x, path_last_y = pl.y;
  uint32_t k = 0;
  if (dx > 1) k |= K_RIGHT;
  if (dx < -1) k |= K_LEFT;
  if (dy > 1) k |= K_DOWN;
  if (dy < -1) k |= K_UP;
  return k;
}

static uint32_t key_bit(const char *s) {
  static const struct { const char *n; uint32_t k; } keys[] = {
    {"left", K_LEFT}, {"right", K_RIGHT}, {"up", K_UP}, {"down", K_DOWN}, {"z", K_Z}, {"x", K_X}, {"c", K_C},
    {"home", K_HOME}};
  for (size_t i = 0; i < sizeof keys / sizeof keys[0]; i++)
    if (!strcmp(s, keys[i].n)) return keys[i].k;
  fprintf(stderr, "unknown key %s\n", s);
  exit(2);
}

static void parse_keys(const char *spec) {
  char buf[65536];
  snprintf(buf, sizeof buf, "%s", spec);
  for (char *t = strtok(buf, ","); t; t = strtok(NULL, ",")) {
    int a, b;
    char k[32];
    if (sscanf(t, "%d-%d:%31s", &a, &b, k) == 3) holds[nholds++] = (Hold){a, b, key_bit(k)};
    else if (sscanf(t, "%d:%31s", &a, k) == 2) holds[nholds++] = (Hold){a, a, key_bit(k)};
    else fprintf(stderr, "bad key %s\n", t), exit(2);
  }
}

int main(int argc, char **argv) {
  const char *out = "build/play", *saves = NULL, *shots = "";
  int frames = 120, room = -1, ax = 0, ay = 0, battle = -1;
  const char *name = NULL;
  int nflags = 0, fl[64][2];
  bool trace = false, coll = false;
  for (int i = 1; i < argc; i++) {
    if (!strcmp(argv[i], "--frames") && i + 1 < argc) frames = atoi(argv[++i]);
    else if (!strcmp(argv[i], "--keys") && i + 1 < argc) parse_keys(argv[++i]);
    else if (!strcmp(argv[i], "--path") && i + 1 < argc) snprintf(path_buf, sizeof path_buf, "%s", argv[++i]), path_at = path_buf;
    else if (!strcmp(argv[i], "--shots") && i + 1 < argc) shots = argv[++i];
    else if (!strcmp(argv[i], "--out") && i + 1 < argc) out = argv[++i];
    else if (!strcmp(argv[i], "--saves") && i + 1 < argc) saves = argv[++i];
    else if (!strcmp(argv[i], "--room") && i + 1 < argc) room = atoi(argv[++i]);
    else if (!strcmp(argv[i], "--at") && i + 1 < argc) sscanf(argv[++i], "%d,%d", &ax, &ay);
    else if (!strcmp(argv[i], "--name") && i + 1 < argc) name = argv[++i];
    else if (!strcmp(argv[i], "--battle") && i + 1 < argc) battle = atoi(argv[++i]);
    else if (!strcmp(argv[i], "--flag") && i + 1 < argc && nflags < 64) {
      sscanf(argv[++i], "%d=%d", &fl[nflags][0], &fl[nflags][1]);
      nflags++;
    } else if (!strcmp(argv[i], "--trace")) trace = true;
    else if (!strcmp(argv[i], "--coll")) coll = true;
    else fprintf(stderr, "unknown option %s\n", argv[i]), exit(2);
  }
  mkdir(out, 0755);
  char sd[512];
  snprintf(sd, sizeof sd, "%s/saves", out);
  if (!saves) saves = sd;
  mkdir(saves, 0755);
  host_save_dir(saves);
  ut_bin = ut_data;
  game_init();
  if (room >= 0 || name) {
    game_new(name ? name : "Frisk");
    if (room >= 0) {
      if (!ax && !ay) {
        RoomInfo ri;
        room_info(room, &ri);
        ax = ri.x + ri.w / 2 - 10, ay = ri.y + ri.h / 2 - 15;
      }
      world_enter(room, ax, ay, DIR_DOWN);
    }
  }
  for (int i = 0; i < nflags; i++) set_flag(fl[i][0], fl[i][1]);
  if (battle >= 0) battle_start(battle);
  bool all = !strcmp(shots, "all");
  for (int f = 0; f < frames && g_running; f++) {
    uint32_t k = 0;
    for (int i = 0; i < nholds; i++)
      if (f >= holds[i].a && f <= holds[i].b) k |= holds[i].k;
    if (!k) k = path_keys(f);
    host_keys = k;
    host_time += 33;
    game_frame(k);
    game_draw();
    if (trace) printf("%d mode %d room %d frisk %d,%d cam %d,%d hp %d\n", f, g_mode, g_room, pl.x, pl.y, cam_x, cam_y, sv.hp);
    char want[16];
    snprintf(want, sizeof want, "%d", f);
    bool shoot = all;
    if (!shoot) {
      const char *p = shots;
      size_t n = strlen(want);
      while ((p = strstr(p, want))) {
        if ((p == shots || p[-1] == ',') && (p[n] == ',' || !p[n])) {
          shoot = true;
          break;
        }
        p += n;
      }
    }
    if (shoot && coll && world_room_info()) {
      /* the walkable cells tinted green, the others red */
      const RoomInfo *r = world_room_info();
      for (int y = 0; y < SCREEN_H; y++)
        for (int x = 0; x < SCREEN_W; x++) {
          int wx = x + cam_x, wy = y + cam_y, cx = (wx - r->x) >> 2, cy = (wy - r->y) >> 2;
          if (wx < r->x || wy < r->y || cx >= r->cw || cy >= r->ch) continue;
          bool w = r->coll[cy * ((r->cw + 7) >> 3) + (cx >> 3)] >> (cx & 7) & 1;
          uint16_t c = host_fb[y * SCREEN_W + x];
          host_fb[y * SCREEN_W + x] = w ? (uint16_t)((c >> 1 & 0x7BEF) | 0x0400) : (uint16_t)((c >> 1 & 0x7BEF) | 0x8000);
        }
    }
    if (shoot) {
      char path[600];
      snprintf(path, sizeof path, "%s/shot_%d.ppm", out, f);
      host_shot(path);
    }
  }
  if (gfx_dropped) printf("dropped %d draw commands\n", gfx_dropped);
  return 0;
}
