/* Plays Undertale on a computer, with scripted keys, and saves frames.
 *
 *   play [--frames N] [--keys "10-40:right,50:z,..."] [--shots 30,60|all] [--out DIR] [--saves DIR]
 *        [--room N --at X,Y] [--name NAME] [--flag F=V]... [--gold N] [--battle ID] [--trace]
 *
 * --path "x,y;x,y;z;w30;..." steers Frisk to each point in turn (holding arrows), presses Z ("z"), X ("x"),
 * waits ("wN": N frames) or mashes Z through talks ("talk": until no text or event is left), when the scripted
 * keys don't hold anything.
 * Keys: left right up down z x c home. "a-b:key" holds the key from frame a to b, "a:key" for one frame.
 * --room starts a new game in room N (its ROOM_* number) with Frisk's top-left at (X, Y), past the title.
 * Screenshots are DIR/shot_<frame>.ppm. */
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include "../src/battle.h"
#include "../src/blk.h"
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
extern bool g_no_encounters;
static char *path_at;
static int path_wait, path_stuck, path_last_x, path_last_y, path_room = -1;

/* ---------------------------------------------------------------- "go:x,y": a way there (Frisk's top-left
 * on a 4-pixel grid, over the floor and around the solid things), found once, then walked */
#define GO_R 400                      /* cells each way: 1600 pixels */
#define GO_N (2 * GO_R + 1)
static int16_t go_wp[512][2];
static int go_n, go_i;
static bool go_free(int x, int y) { return world_walkable(x + 3, y + 20, 14, 9, -1); }
static bool go_plan(int tx, int ty) {
  static int16_t prev[GO_N * GO_N];
  static int queue[GO_N * GO_N];
  int ox = pl.x, oy = pl.y;
  for (int i = 0; i < GO_N * GO_N; i++) prev[i] = -2;
  int s = GO_R * GO_N + GO_R, qh = 0, qt = 0, best = s, bestd = 1 << 30;
  prev[s] = -1, queue[qt++] = s;
  while (qh < qt) {
    int c = queue[qh++], cx = c % GO_N - GO_R, cy = c / GO_N - GO_R;
    int d = abs(ox + cx * 4 - tx) + abs(oy + cy * 4 - ty);
    if (d < bestd) bestd = d, best = c;
    if (d <= 3) break;
    static const int dx[4] = {1, -1, 0, 0}, dy[4] = {0, 0, 1, -1};
    for (int k = 0; k < 4; k++) {
      int nx = cx + dx[k], ny = cy + dy[k];
      if (nx < -GO_R || nx > GO_R || ny < -GO_R || ny > GO_R) continue;
      int n = (ny + GO_R) * GO_N + nx + GO_R;
      if (prev[n] != -2 || !go_free(ox + nx * 4, oy + ny * 4)) continue;
      prev[n] = (int16_t)k, queue[qt++] = n;
    }
  }
  /* back from the end, a waypoint where the way turns */
  int16_t rev[GO_N * 2][2];
  int nr = 0, c = best, last = -1;
  while (prev[c] >= 0 && nr < GO_N * 2) {
    int k = prev[c], cx = c % GO_N - GO_R, cy = c / GO_N - GO_R;
    if (k != last) rev[nr][0] = (int16_t)(ox + cx * 4), rev[nr][1] = (int16_t)(oy + cy * 4), nr++, last = k;
    static const int dx[4] = {1, -1, 0, 0}, dy[4] = {0, 0, 1, -1};
    c = (cy - dy[k] + GO_R) * GO_N + cx - dx[k] + GO_R;
  }
  go_n = 0;
  for (int i = nr - 1; i >= 0 && go_n < 512; i--) go_wp[go_n][0] = rev[i][0], go_wp[go_n][1] = rev[i][1], go_n++;
  if (nr && go_n < 512) {
    /* (the last turn's waypoint is where the turn starts: the end too) */
    int ex = ox + (best % GO_N - GO_R) * 4, ey = oy + (best / GO_N - GO_R) * 4;
    go_wp[go_n][0] = (int16_t)ex, go_wp[go_n][1] = (int16_t)ey, go_n++;
  }
  go_i = 0;
  if (bestd > 3) fprintf(stderr, "go: no way to %d,%d, as near as %d px\n", tx, ty, bestd);
  if (getenv("GO_DEBUG"))
    for (int i = 0; i < go_n; i++) fprintf(stderr, "go %d: %d,%d\n", i, go_wp[i][0], go_wp[i][1]);
  return go_n > 0;
}

/* the keys the path holds this frame (0: it's done or waiting) */
static uint32_t path_keys(int f) {
  if (!path_at || !*path_at) return 0;
  /* a battle or a choice that came on the way to a point: the step after it is for it, if it says so */
  {
    char *e = strchr(path_at, ';');
    if (e) {
      const char *n = e + 1;
      bool nb = !strncmp(n, "fight", 5) || !strncmp(n, "spare", 5) || !strncmp(n, "bt", 2) || !strncmp(n, "act:", 4) ||
                !strncmp(n, "flee", 4) || !strncmp(n, "seq:", 4);
      bool hb = !strncmp(path_at, "fight", 5) || !strncmp(path_at, "spare", 5) || !strncmp(path_at, "bt", 2) ||
                !strncmp(path_at, "act:", 4) || !strncmp(path_at, "flee", 4) || !strncmp(path_at, "pick:", 5) ||
                !strncmp(path_at, "seq:", 4);
      if (!hb && ((g_mode == MODE_BATTLE && nb) || (g_mode == MODE_WORLD && text_menu_sel() >= 0 && !strncmp(n, "pick:", 5))))
        path_at = e + 1;
    }
  }
  /* a choice up while Frisk still walks: a "pick:" a few steps on (past "talk"s and points) is for it */
  if (g_mode == MODE_WORLD && text_menu_sel() >= 0 && strncmp(path_at, "pick:", 5)) {
    char *n = strchr(path_at, ';');
    for (int k = 0; n && k < 3 && (!strncmp(n + 1, "talk", 4) || !strncmp(n + 1, "go:", 3) || isdigit((unsigned char)n[1])); k++)
      n = strchr(n + 1, ';');
    if (n && !strncmp(n + 1, "pick:", 5)) path_at = n + 1;
  }
  /* "pick:N": the option N of the next list or choice */
  if (!strncmp(path_at, "pick:", 5) && g_mode == MODE_WORLD) {
    int n = atoi(path_at + 5), sel = text_menu_sel();
    if (f & 1) return 0;
    if (sel < 0) return text_active() ? K_Z : 0;
    if (sel >= 100) sel -= 100;
    if (sel == n) {
      char *e = strchr(path_at, ';');
      path_at = e ? e + 1 : path_at + strlen(path_at);
      return K_Z;
    }
    return sel < n ? (text_menu_sel() >= 100 ? K_RIGHT : K_DOWN) : (text_menu_sel() >= 100 ? K_LEFT : K_UP);
  }
  /* a text in the way: read on (Z every other frame) */
  if (g_mode == MODE_WORLD && text_active()) return (f & 1) ? K_Z : 0;
  /* in a battle: "fight" FIGHTs, "spare" spares, "act:N" picks the ACT N (1: the first after Check) each turn,
   * "bt" (a scripted battle) reads on; Z on whatever else comes (the bar, the texts). A battle the path didn't
   * expect: Frisk flees it */
  bool btok = !strncmp(path_at, "fight", 5) || !strncmp(path_at, "spare", 5) || !strncmp(path_at, "bt", 2) ||
              !strncmp(path_at, "act:", 4) || !strncmp(path_at, "flee", 4) || !strncmp(path_at, "seq:", 4);
  /* "seq:a,b@2,...": an ACT a turn (b@2: on the second monster), MERCY once one can be spared or they're done */
  static char *seq_for;
  static int seq_i;
  int seq_act = 0, seq_target = 0;
  bool seq = !strncmp(path_at, "seq:", 4);
  if (seq) {
    if (seq_for != path_at) seq_for = path_at, seq_i = 0;
    const char *q = path_at + 4;
    for (int k = 0; k < seq_i && q && *q && *q != ';'; k++) {
      q = strchr(q, ',');
      if (q) q++;
    }
    if (q && *q && *q != ';') {
      seq_act = atoi(q);
      const char *at = strchr(q, '@'), *comma = strchr(q, ','), *semi = strchr(q, ';');
      if (at && (!comma || at < comma) && (!semi || at < semi)) seq_target = atoi(at + 1) - 1;
    }
    bool can = false;
    for (int i = 0; i < bt.nmon; i++) can |= bt.mon[i].alive && !bt.mon[i].gone && bt.mon[i].spare;
    if (can) seq_act = 0;
  }
  static int idle;
  if (g_mode == MODE_BATTLE) {
    idle = 0;
    if (battle_over()) return 0;
    if (f & 1) return 0;
    if (bt.scripted) return K_Z;   /* (Flowey's: read on) */
    int want = !btok || !strncmp(path_at, "flee", 4) || !strncmp(path_at, "spare", 5) ? 3 : !strncmp(path_at, "act:", 4) ? 1 : 0;
    if (seq) want = seq_act ? 1 : 3;
    int mercy = (!btok || !strncmp(path_at, "flee", 4)) && !bt.no_flee ? 1 : 0;
    switch (battle_ui()) {
      case BUI_MENU: return bt.button < want ? K_RIGHT : bt.button > want ? K_LEFT : K_Z;
      case BUI_TARGET:
        if (seq && bt.button == 1) return bt.target < seq_target ? K_DOWN : bt.target > seq_target ? K_UP : K_Z;
        return K_Z;
      case BUI_ACTS: {
        int n = seq ? seq_act : atoi(path_at + 4);
        if (seq && bt.act == n) seq_i++;
        return bt.act + 2 <= n ? K_DOWN : bt.act < n ? K_RIGHT : bt.act - 2 >= n ? K_UP : bt.act > n ? K_LEFT : K_Z;
      }
      case BUI_MERCY: return bt.mercy < mercy ? K_DOWN : bt.mercy > mercy ? K_UP : K_Z;
      case BUI_BAR: return abs(bt.bar_x / FX - 140) < 8 ? K_Z : 0;   /* (the middle: the best hit) */
      default: return K_Z;
    }
  }
  if (btok) {
    /* (it waits for the battle: done when the world has been quiet a while) */
    if (g_mode == MODE_WORLD && !event_blocking() && !text_active() && ++idle > 30) {
      char *e = strchr(path_at, ';');
      path_at = e ? e + 1 : path_at + strlen(path_at);
      idle = 0;
    }
    return (f & 1) ? K_Z : 0;
  }
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
  /* u d l r: a key's press (to face something) */
  if (!tok[1] && strchr("udlr", tok[0])) {
    path_at = next;
    path_wait = 1;
    return tok[0] == 'u' ? K_UP : tok[0] == 'd' ? K_DOWN : tok[0] == 'l' ? K_LEFT : K_RIGHT;
  }
  if (tok[0] == 'w') {
    path_wait = atoi(tok + 1);
    path_at = next;
    return 0;
  }
  if (!strcmp(tok, "talk")) {
    if (!text_active() && !event_blocking() && g_mode == MODE_WORLD && !world_fading()) {
      path_at = next;
      return 0;
    }
    return (f & 1) ? K_Z : 0;
  }
  if (!strncmp(tok, "go:", 3)) {
    static char *planned;
    if (g_mode != MODE_WORLD || text_active() || event_blocking() || world_fading()) return 0;
    int gx, gy;
    sscanf(tok + 3, "%d,%d", &gx, &gy);
    static int planned_room;
    if (planned == path_at && planned_room != g_room) {
      /* a door to another area on the way: the point was there, so it is reached */
      RoomInfo a, b;
      room_info(planned_room, &a), room_info(g_room, &b);
      if (a.area != b.area) {
        path_at = next, planned = NULL;
        return 0;
      }
    }
    if (planned != path_at || planned_room != g_room) {
      planned = path_at, planned_room = g_room;
      if (!go_plan(gx, gy)) {
        path_at = next, planned = NULL;
        return 0;
      }
    }
    while (go_i < go_n && abs(go_wp[go_i][0] - pl.x) <= 1 && abs(go_wp[go_i][1] - pl.y) <= 1) go_i++;
    /* (a waypoint on the axis it's walking: it may step over it by a pixel or two) */
    if (go_i >= go_n) {
      path_at = next, planned = NULL;
      return 0;
    }
    if (pl.x == path_last_x && pl.y == path_last_y) {
      if (++path_stuck > 30) {
        planned = NULL, path_stuck = 0;   /* (something moved in the way: plan again) */
        return 0;
      }
    } else
      path_stuck = 0;
    path_last_x = pl.x, path_last_y = pl.y;
    /* one axis at a time, as the way was found */
    int ddx = go_wp[go_i][0] - pl.x, ddy = go_wp[go_i][1] - pl.y;
    if (abs(ddx) > 1) return ddx > 0 ? K_RIGHT : K_LEFT;
    return ddy > 0 ? K_DOWN : K_UP;
  }
  int x, y;
  if (sscanf(tok, "%d,%d", &x, &y) != 2) {
    path_at = next;
    return 0;
  }
  if (g_mode != MODE_WORLD || text_active() || event_blocking() || world_fading()) return 0;
  /* a door took him to another room on the way to this point: it is reached */
  static char *room_step;
  if (room_step != path_at) room_step = path_at, path_room = g_room;
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
  int nflags = 0, fl[64][2], gold = -1;
  bool trace = false, coll = false, god = false;
  for (int i = 1; i < argc; i++) {
    if (!strcmp(argv[i], "--frames") && i + 1 < argc) frames = atoi(argv[++i]);
    else if (!strcmp(argv[i], "--keys") && i + 1 < argc) parse_keys(argv[++i]);
    else if (!strcmp(argv[i], "--path") && i + 1 < argc) snprintf(path_buf, sizeof path_buf, "%s", argv[++i]), path_at = path_buf;
    else if (!strcmp(argv[i], "--path-file") && i + 1 < argc) {
      /* a path one step a line; # starts a comment */
      FILE *pf = fopen(argv[++i], "r");
      if (!pf) fprintf(stderr, "no %s\n", argv[i]), exit(2);
      char line[256];
      size_t n = 0;
      while (fgets(line, sizeof line, pf)) {
        char *h = strchr(line, '#');
        if (h) *h = 0;
        char *a = line, *e = line + strlen(line);
        while (*a == ' ' || *a == '\t') a++;
        while (e > a && (e[-1] == '\n' || e[-1] == '\r' || e[-1] == ' ' || e[-1] == '\t')) *--e = 0;
        if (!*a) continue;
        n += (size_t)snprintf(path_buf + n, sizeof path_buf - n, "%s%s", n ? ";" : "", a);
      }
      fclose(pf);
      path_at = path_buf;
    }
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
    else if (!strcmp(argv[i], "--noenc")) g_no_encounters = true;
    else if (!strcmp(argv[i], "--god")) god = true;
    else if (!strcmp(argv[i], "--gold") && i + 1 < argc) gold = atoi(argv[++i]);
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
  if (room >= 0 || name || battle >= 0) {
    game_new(name ? name : "Frisk");
    for (int i = 0; i < nflags; i++) set_flag(fl[i][0], fl[i][1]);   /* (before the room is set up) */
    if (room < 0 && battle >= 0) room = 0;   /* (a battle comes back to a room) */
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
  if (gold >= 0) sv.gold = (uint16_t)gold;
  if (battle >= 0) battle_start(battle);
  bool all = !strcmp(shots, "all");
  for (int f = 0; f < frames && g_running; f++) {
    uint32_t k = 0;
    for (int i = 0; i < nholds; i++)
      if (f >= holds[i].a && f <= holds[i].b) k |= holds[i].k;
    if (!k) k = path_keys(f);
    host_keys = k;
    if (god && g_mode == MODE_BATTLE && sv.hp < sv.maxhp) sv.hp = sv.maxhp;   /* (--god: the SOUL can't be hurt) */
    host_time += 33;
    game_frame(k);
    game_draw();
    if (trace) printf("%d mode %d room %d frisk %d,%d cam %d,%d hp %d ui %d btn %d keys %x path %.12s\n", f, g_mode, g_room, pl.x, pl.y, cam_x, cam_y, sv.hp, battle_ui(), bt.button, k, path_at ? path_at : "");
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
  if (getenv("BLK_STATS")) fprintf(stderr, "blocks unpacked %u, at most %u in a frame\n", blk_unpacked, blk_most_in_frame);
  printf("END mode %d room %d at %d,%d hp %d lv %d gold %d kills %d path %s\nFLAGS", g_mode, g_room, pl.x, pl.y, sv.hp,
         sv.lv, sv.gold, sv.kills, path_at && *path_at ? "left" : "done");
  for (int i = 0; i < FLAG_COUNT; i++)
    if (sv.flags[i]) printf(" %d=%d", i, sv.flags[i]);
  printf("\nITEMS");
  for (int i = 0; i < NUM_ITEMS; i++) printf(" %d", sv.items[i]);
  printf("\n");
  return 0;
}
