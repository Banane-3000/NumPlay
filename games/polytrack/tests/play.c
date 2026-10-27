/* PolyTrack on a computer, for tests and pictures (make host, make test).
 *
 *   play test DIR            every official track driven to the finish by its recorded run, to the same frame and
 *                            hash on every computer; the checkpoints in order, respawns, restarts; the whole app
 *                            through its keys, all 17 tracks one after the other; saves written and read back,
 *                            damaged saves, a full storage, the ghosts; 300 sessions of random keys
 *   play shots DIR [MS]      pictures of the screens, the race MS ms into Summer 1's run (DIR/<name>.ppm)
 *   play view DIR T MS [Q]   a picture of track T after MS ms of its recorded run, at quality Q (DIR/view.ppm)
 *   play film DIR T MS STEP  MS ms of track T's recorded run, a frame every STEP ms (DIR/<frame>_<ms>.raw, for
 *                            tools/record.py --frames)
 *   play probe DIR T MS [K]  the car's state after MS ms of the run, then a second of keys K (IN_ bits)
 *   play cam DIR T X Y Z TX TY TZ   track T seen from (X, Y, Z) looking at (TX, TY, TZ) (DIR/cam.ppm)
 * A run found by tests/route.c can be looked at before it goes into routes.h: ROUTE_FILE=its output. */
#define main pt_app_main
#include "../src/main.c"
#undef main
#include <stdio.h>
#include <stdlib.h>
#include "routes.h"
#include "../../../launcher/src/gfx.h"

/* ---------------------------------------------------------------- the calculator, on a computer */
static uint16_t fb[240][320];
void plat_push(int x, int y, int w, int h, const uint16_t *px) {
  for (int j = 0; j < h; j++)
    if (y + j < 240)
      for (int i = 0; i < w; i++) fb[y + j][x + i] = px[j * w + i];
}
void *np_alloc(uint32_t n) { (void)n; return 0; }
static uint32_t clock_ms, tick = 5, nap = 1;   /* ms a look at the keys takes; a sleep's share of what it asks */
static uint32_t (*script)(uint32_t n);
static uint32_t calls, quit_at;
uint32_t plat_keys(void) {
  clock_ms += tick;
  calls++;
  if (quit_at && calls >= quit_at) return PK_HOME;
  return script ? script(calls) : 0;
}
uint32_t plat_millis(void) { return clock_ms; }
void plat_sleep(uint32_t ms) { clock_ms += ms * nap; }
void plat_vblank(void) {}
bool plat_slow(void) { return false; }
void plat_begin(void) {}
int plat_end(void) { return 0; }

#define NFILES 6
static struct { char name[32]; uint8_t data[16384]; uint32_t len; bool on; } files[NFILES];
static bool storage_full;
static int writes;
bool plat_save(const char *name, const void *data, uint32_t len) {
  if (storage_full) return false;
  int k = -1;
  for (int i = 0; i < NFILES; i++)
    if (files[i].on && !strcmp(files[i].name, name)) k = i;
  for (int i = 0; i < NFILES && k < 0; i++)
    if (!files[i].on) k = i;
  if (k < 0 || len > sizeof files[k].data) return false;
  strcpy(files[k].name, name), memcpy(files[k].data, data, len), files[k].len = len, files[k].on = true;
  writes++;
  return true;
}
const uint8_t *plat_load(const char *name, uint32_t *len) {
  for (int i = 0; i < NFILES; i++)
    if (files[i].on && !strcmp(files[i].name, name)) return *len = files[i].len, files[i].data;
  return 0;
}
static uint8_t *file_data(const char *name, uint32_t *len) {
  for (int i = 0; i < NFILES; i++)
    if (files[i].on && !strcmp(files[i].name, name)) return *len = files[i].len, files[i].data;
  return 0;
}
static void forget(const char *name) {
  for (int i = 0; i < NFILES; i++)
    if (files[i].on && !strcmp(files[i].name, name)) files[i].on = false;
}
static void forget_all(void) {
  for (int i = 0; i < NFILES; i++) files[i].on = false;
}

static void shot(const char *dir, const char *name) {
  char path[512];
  snprintf(path, sizeof path, "%s/%s.ppm", dir, name);
  FILE *f = fopen(path, "wb");
  if (!f) exit(2);
  fprintf(f, "P6 320 240 255\n");
  for (int y = 0; y < 240; y++)
    for (int x = 0; x < 320; x++) {
      uint16_t c = fb[y][x];
      fputc((c >> 11) * 255 / 31, f), fputc((c >> 5 & 63) * 255 / 63, f), fputc((c & 31) * 255 / 31, f);
    }
  fclose(f);
}

static int fails, checks;
#define CHECK(c, ...) do { checks++; if (!(c)) { fails++; printf("FAIL %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

/* ---------------------------------------------------------------- the recorded runs (routes.h) */
static uint8_t slots[60000];        /* the keys, 10 ms each */
static int nslots;
static void route_decode(const char *s) {
  nslots = 0;
  while (*s && nslots < (int)sizeof slots) {
    char c = *s++;
    uint8_t k;
    if (c >= 'a' && c <= 'p') k = (uint8_t)(c - 'a');
    else if (c >= 'A' && c <= 'P') k = (uint8_t)(c - 'A' + IN_RESET);
    else continue;
    int r = 0;
    while (*s >= '0' && *s <= '9') r = r * 10 + (*s++ - '0');
    if (!r) r = 1;
    while (r-- && nslots < (int)sizeof slots) slots[nslots++] = k;
  }
}
static char file_route[400000];
static void route_load(int t) {
  const char *path = getenv("ROUTE_FILE");
  FILE *f = path ? fopen(path, "r") : NULL;
  if (!f) {
    route_decode(pt_routes[t]);
    return;
  }
  /* (the letters between quotes) */
  int n = 0, ch, in = 0;
  while ((ch = fgetc(f)) != EOF && n < (int)sizeof file_route - 1) {
    if (ch == '"') in = !in;
    else if (in) file_route[n++] = (char)ch;
  }
  file_route[n] = 0;
  fclose(f);
  route_decode(file_route);
}
static unsigned route_keys(uint32_t frame) { return frame / 10 < (uint32_t)nslots ? slots[frame / 10] : 0; }

static car_t c1;                    /* (the tests' car) */
static void render_race(const car_t *c, chase_t *ch, float dt) {
  look = car_look_of(c);
  look.paint = sv.p.paint;
  chase_update(ch, &cam, dt, look.pos, look.rot, c->speed);
  ui.screen = V_RACE, ui.started = 1;
  pt_render(&sv, c, &cam, &look, NULL, &ui);
}

/* ---------------------------------------------------------------- the tests */
/* the runs: to the finish, at the frame and with the hash recorded; the checkpoints in order on the way */
static void test_runs(void) {
  for (int t = 0; t < PT_NTRACKS; t++) {
    world_load(t);
    route_load(t);
    car_reset(&c1);
    int n = car_checkpoints();
    uint16_t last = 0;
    bool order_ok = true;
    for (uint32_t f = 0; f < 600000 && !c1.finish; f++) {
      car_step(&c1, route_keys(f));
      if (c1.next_cp != last) {
        if (c1.next_cp != last + 1 || (last < 64 && c1.cp_frames[last] != c1.frames)) order_ok = false;
        last = c1.next_cp;
      }
    }
    uint32_t h = car_hash(&c1);
    printf("%-9s %s %6u ms, hash %08x\n", pt_tracks[t].name, c1.finish ? "finished in" : "NOT FINISHED", c1.finish, h);
    CHECK(c1.finish, "%s: not finished", pt_tracks[t].name);
    CHECK(c1.next_cp == n, "%s: finished with %d of %d checkpoints", pt_tracks[t].name, c1.next_cp, n);
    CHECK(order_ok, "%s: checkpoints out of order", pt_tracks[t].name);
    CHECK(nslots && (slots[0] & (IN_UP | IN_DOWN | IN_LEFT | IN_RIGHT)), "%s: the run doesn't start with a driving key",
          pt_tracks[t].name);
    CHECK(c1.finish == pt_route_ms[t], "%s: finished in %u ms, recorded %u", pt_tracks[t].name, c1.finish, pt_route_ms[t]);
    CHECK(h == pt_route_hash[t], "%s: hash %08x, recorded %08x (the physics isn't the same here)", pt_tracks[t].name, h,
          pt_route_hash[t]);
  }
}

/* puts the car at rest in a detector, and takes a frame */
static void put_in(car_t *c, int k) {
  detector_t d;
  world_detector(k, &d);
  c->pos = vmul(vadd(d.lo, d.hi), 0.5f);
  c->vel = c->ang = V3(0.0f, 0.0f, 0.0f);
  c->cache_ok = 0;
  car_step(c, 0);
}

/* the detector of the o-th checkpoint order (smallest first) */
static int det_of_order(int o) {
  const pt_track *tr = W.t;
  uint16_t got[64];
  int ng = 0;
  detector_t d;
  for (int j = 0; j < tr->ncp; j++) {
    world_detector(j, &d);
    bool dup = false;
    for (int m = 0; m < ng; m++) dup |= got[m] == d.order;
    if (!dup && ng < 64) got[ng++] = d.order;
  }
  for (int i = 0; i < tr->ncp; i++) {
    world_detector(i, &d);
    int below = 0;
    for (int m = 0; m < ng; m++) below += got[m] < d.order;
    if (below == o) return i;
  }
  return -1;
}

/* checkpoints count in their order only, the finish after all of them */
static void test_checkpoints(void) {
  for (int t = 0; t < PT_NTRACKS; t++) {
    world_load(t);
    car_reset(&c1);
    const pt_track *tr = W.t;
    int n = car_checkpoints();
    put_in(&c1, tr->ncp);
    CHECK(!c1.finish && c1.next_cp == 0, "%s: the finish counted before the checkpoints", tr->name);
    if (n >= 2) {
      put_in(&c1, det_of_order(n - 1));
      CHECK(c1.next_cp == 0, "%s: the last checkpoint counted first", tr->name);
    }
    for (int o = 0; o < n; o++) {
      int k = det_of_order(o);
      CHECK(k >= 0, "%s: no checkpoint of order %d", tr->name, o);
      if (k < 0) break;
      put_in(&c1, k);
      CHECK(c1.next_cp == o + 1, "%s: checkpoint %d not taken", tr->name, o);
      CHECK(!c1.finish, "%s: finished early", tr->name);
    }
    put_in(&c1, tr->ncp);
    CHECK(c1.finish, "%s: the finish didn't count after every checkpoint", tr->name);
  }
}

/* respawns: back to the last flat checkpoint, standing, its count kept; a held key is one respawn; none before */
static void test_respawn(void) {
  for (int t = 0; t < PT_NTRACKS; t++) {
    world_load(t);
    route_load(t);
    car_reset(&c1);
    car_step(&c1, IN_RESET | IN_UP);
    v3 s0;
    qt q0;
    world_start(&s0, &q0);
    CHECK(!c1.has_respawn && vlen(vsub(c1.pos, s0)) < 0.5f, "%s: a respawn before any checkpoint", pt_tracks[t].name);
    uint32_t f = 1;
    while (f < 600000 && !c1.has_respawn && !c1.finish) car_step(&c1, route_keys(f++));
    if (!c1.has_respawn) continue;  /* (only checkpoints on walls: none to go back to) */
    for (int i = 0; i < 1500 && !c1.finish; i++) car_step(&c1, route_keys(f++));
    if (c1.finish) continue;
    uint16_t cp = c1.next_cp;
    uint32_t frames = c1.frames;
    car_step(&c1, IN_RESET);
    CHECK(vlen(vsub(c1.pos, c1.respawn.pos)) < 0.05f, "%s: not back at the checkpoint", pt_tracks[t].name);
    CHECK(c1.next_cp == c1.respawn.next && c1.next_cp <= cp, "%s: the checkpoints' count changed", pt_tracks[t].name);
    CHECK(c1.frames == frames + 1, "%s: the clock stopped", pt_tracks[t].name);
    CHECK(vlen(c1.vel) < 0.1f, "%s: still moving after a respawn", pt_tracks[t].name);
    v3 at = c1.pos;
    for (int i = 0; i < 300; i++) car_step(&c1, IN_RESET);
    CHECK(vlen(vsub(c1.pos, at)) < 1.0f, "%s: a held key respawned again", pt_tracks[t].name);
    /* (and it drives off: 30 km/h within two seconds of gas) */
    float top = 0.0f;
    for (int i = 0; i < 2000; i++) car_step(&c1, IN_UP), top = pt_maxf(top, c1.speed);
    CHECK(top > 30.0f, "%s: stuck after a respawn (%.0f km/h at most)", pt_tracks[t].name, (double)top);
  }
}

/* ---------------------------------------------------------------- the whole app, through its keys */
static int app_phase;
static uint32_t to_pk(unsigned in) {
  uint32_t k = 0;
  if (in & IN_UP) k |= PK_UP;
  if (in & IN_DOWN) k |= PK_DOWN;
  if (in & IN_LEFT) k |= PK_LEFT;
  if (in & IN_RIGHT) k |= PK_RIGHT;
  if (in & IN_RESET) k |= PK_OK;
  return k;
}
static int loaded = -1;
/* Title: Play; Tracks: OK; the guide: OK three times; each track with its run, then Next track; after the last,
   Tracks, Back, Back, the quit question: Down, OK */
static uint32_t script_all(uint32_t n) {
  if (ui.screen == V_RACE) {
    if (loaded != sv.p.track) route_load(sv.p.track), loaded = sv.p.track;
    return to_pk(route_keys(car.frames));
  }
  if (n & 1) return 0;              /* (keys go up between presses) */
  switch (ui.screen) {
    case V_TITLE: return app_phase ? PK_BACK : PK_OK;
    case V_TRACKS: return app_phase ? PK_BACK : PK_OK;
    case V_GUIDE: return PK_OK;
    case V_QUIT: return ui.sel ? PK_OK : PK_DOWN;
    case V_RESULT:
      if (sv.p.track == PT_NTRACKS - 1) {
        app_phase = 1;
        return ui.sel == RES_TRACKS ? PK_OK : PK_DOWN;
      }
      return ui.sel == RES_NEXT ? PK_OK : PK_DOWN;
    default: return PK_BACK;
  }
}
static void reset_app(void) {
  memset(&ui, 0, sizeof ui);
  memset(&sv, 0, sizeof sv);
  memset(&car, 0, sizeof car);
  rd_detail = RD_LEVELS - 1;
  calls = 0, quit_at = 0;
  app_phase = 0, loaded = -1;
}

static void test_app(void) {
  forget_all();
  storage_full = false;
  reset_app();
  tick = 1, nap = 0, frame_gap = 2000;   /* (a physics frame each time round: the runs' keys land as recorded) */
  script = script_all;
  quit_at = 30000000;
  pt_app_main();
  CHECK(calls < quit_at, "the app didn't quit through its menus (screen %d, track %d, %u ms, checkpoint %d, at %.0f %.0f %.0f)",
        ui.screen, sv.p.track, car.frames, car.next_cp, (double)car.pos.x, (double)car.pos.y, (double)car.pos.z);
  pt_file f;
  bool ok = save_load(&f);
  CHECK(ok, "no save after the races");
  for (int t = 0; t < PT_NTRACKS && ok; t++)
    CHECK(f.p.best[t] == pt_route_ms[t], "app: %s's best %u, its run %u", pt_tracks[t].name, f.p.best[t], pt_route_ms[t]);
  CHECK(ok && f.p.finishes == PT_NTRACKS, "app: %u finishes", ok ? f.p.finishes : 0);
  CHECK(ok && f.p.guide == 1, "app: the guide wasn't marked read");
  /* ghosts: the last ones raced, as many as fit */
  ghosts_load();
  int kept = 0;
  ghost_entry g;
  for (int t = 0; t < PT_NTRACKS; t++)
    if (ghosts_find(t, &g)) {
      kept++;
      CHECK(g.ms == pt_route_ms[t], "app: %s's ghost has %u ms", pt_tracks[t].name, g.ms);
    }
  uint32_t glen;
  CHECK(file_data(PT_GHOST_NAME, &glen) && glen <= PT_GHOST_BYTES, "app: the ghosts' file");
  CHECK(ghosts_find(PT_NTRACKS - 1, &g) || pt_route_ms[PT_NTRACKS - 1] > 130000, "app: the last track's ghost is missing");
  printf("app: 17 tracks through the menus, %d ghosts kept (%u bytes)\n", kept, glen);
}

/* the ghost follows the run it was made from */
static void test_ghost(void) {
  for (int t = 0; t < PT_NTRACKS; t++) {
    world_load(t);
    route_load(t);
    car_reset(&c1);
    ghost_rec_start(&rec);
    ghost_rec_add(&rec, c1.pos, c1.rot);
    static v3 path[6000];
    int np = 0;
    path[np++] = c1.pos;
    for (uint32_t f = 0; f < 600000 && !c1.finish; f++) {
      car_step(&c1, route_keys(f));
      if (!c1.finish && c1.frames % GHOST_STEP == 0) {
        ghost_rec_add(&rec, c1.pos, c1.rot);
        if (np < 6000) path[np++] = c1.pos;
      }
    }
    if (rec.full) {
      printf("%-9s ghost: %u ms is too long for a ghost\n", pt_tracks[t].name, c1.finish);
      CHECK(c1.finish > 100000, "%s: a ghost of %u ms didn't fit", pt_tracks[t].name, c1.finish);
      continue;
    }
    ghost_play_start(&gplay, rec.buf, rec.len, c1.finish);
    float worst = 0.0f;
    for (int i = 0; i + 1 < np; i++) {
      v3 p;
      qt q;
      bool on = ghost_at(&gplay, (uint32_t)i * GHOST_STEP, &p, &q);
      CHECK(on, "%s: no ghost at %d ms", pt_tracks[t].name, i * GHOST_STEP);
      if (!on) break;
      float e = vlen(vsub(p, path[i]));
      if (e > worst) worst = e;
    }
    CHECK(worst < 0.1f, "%s: the ghost is %.2f m off", pt_tracks[t].name, (double)worst);
  }
}

/* keys pressed one at a time (O OK, U D L R arrows, B Back, X Backspace, G gas for a second), then Home once
   the screen is reached */
static const char *seq;
static uint8_t target;
static uint32_t gas_until;
static uint32_t script_seq(uint32_t n) {
  if (n < gas_until) return PK_UP;
  if (n & 1) return 0;
  if (!*seq && ui.screen == target) return PK_HOME;
  char c = *seq ? *seq++ : 0;
  switch (c) {
    case 'O': return PK_OK;
    case 'U': return PK_UP;
    case 'D': return PK_DOWN;
    case 'L': return PK_LEFT;
    case 'R': return PK_RIGHT;
    case 'B': return PK_BACK;
    case 'X': return PK_RESTART;
    case 'G': gas_until = n + 200; return PK_UP;
    default: return 0;
  }
}
static void run_seq(const char *keys, uint8_t screen) {
  reset_app();
  tick = 5, nap = 1, frame_gap = 200;
  seq = keys, target = screen, gas_until = 0;
  script = script_seq;
  quit_at = 5000;
  pt_app_main();
}

static void test_restart_home(void) {
  /* Backspace in a race: the start again, the clock not running */
  forget_all();
  run_seq("OOOOOGX", V_RACE);
  v3 s0;
  qt q0;
  world_start(&s0, &q0);
  CHECK(ui.screen == V_RACE && car.frames == 0 && !ui.started, "restart: the race didn't start over (screen %d, %u frames)",
        ui.screen, car.frames);
  CHECK(vlen(vsub(car.pos, s0)) < 0.5f, "restart: the car isn't at the start");
  /* OK before the first checkpoint: the start again too */
  forget_all();
  run_seq("OOOOOGO", V_RACE);
  CHECK(ui.screen == V_RACE && car.frames == 0, "OK before a checkpoint didn't start over");
  /* Home: saved and out, from each screen */
  static const struct { const char *keys; uint8_t screen; } H[] = {
    {"", V_TITLE}, {"O", V_TRACKS}, {"DOR", V_CAR}, {"DDO", V_HELP}, {"OO", V_GUIDE}, {"OOOOOG", V_RACE},
    {"OOOOOGB", V_PAUSE}, {"OOOOOGBUO", V_LEAVE}, {"B", V_QUIT}, {"OOOOOGBDO", V_RACE},
  };
  for (unsigned i = 0; i < sizeof H / sizeof H[0]; i++) {
    forget_all();
    run_seq(H[i].keys, H[i].screen);
    pt_file f;
    CHECK(calls < quit_at && ui.screen == H[i].screen, "Home: \"%s\" didn't reach screen %d (%d)", H[i].keys, H[i].screen,
          ui.screen);
    CHECK(save_load(&f), "Home on screen %d didn't save", H[i].screen);
    if (H[i].screen == V_CAR) CHECK(f.p.paint == 1, "Home: the car's colour wasn't kept");
  }
  /* Quit game asks first: Back, Up (Quit game), OK, then Keep racing goes back */
  forget_all();
  run_seq("OOOOOGBUOO", V_RACE);
  CHECK(ui.screen == V_RACE, "Keep racing didn't go back to the race");
}

/* the saves: kept and read back, damaged files, the copy in the script, a full storage */
static void test_saves(void) {
  forget_all();
  storage_full = false;
  pt_file a, b;
  CHECK(!save_load(&a), "a save without files");
  pt_fresh(&a.p);
  for (int t = 0; t < PT_NTRACKS; t++) {
    a.p.best[t] = 30000u + (uint32_t)t * 1111u;
    for (int k = 0; k < 3; k++) a.p.split[t][k] = 5000u * (uint32_t)(k + 1) + (uint32_t)t;
  }
  a.p.finishes = 1234, a.p.track = 7, a.p.paint = 3, a.p.view = 1, a.p.guide = 1;
  CHECK(save_write(&a), "the save wasn't written");
  CHECK(save_load(&b) && !memcmp(&a.p, &b.p, sizeof a.p), "the save didn't read back");
  uint32_t n;
  uint8_t *d = file_data(PT_SAVE_NAME, &n);
  CHECK(d && n == sizeof(pt_file) && n == 768, "polytrack.sav is %u bytes", n);
  static uint8_t good[1024], copy[2048];
  memcpy(good, d, n);
  uint32_t cn;
  uint8_t *cp = file_data(PT_COPY_NAME, &cn);
  CHECK(cp && cn < sizeof copy, "no copy in polytrack_saves.py");
  memcpy(copy, cp, cn);
  /* every byte damaged in turn: the copy brings it back */
  int bad = 0;
  for (uint32_t i = 0; i < n; i++) {
    for (int bit = 0; bit < 8; bit += 3) {
      d = file_data(PT_SAVE_NAME, &n);
      memcpy(d, good, n);
      d[i] ^= (uint8_t)(1 << bit);
      memset(&b, 0, sizeof b);
      bool ok = save_load(&b);
      if (!ok || memcmp(&a.p, &b.p, sizeof a.p)) bad++;
      d = file_data(PT_SAVE_NAME, &n);
      if (memcmp(d, good, n)) bad++;   /* (put back) */
    }
  }
  CHECK(!bad, "%d damaged saves weren't mended from the copy", bad);
  /* both damaged: a fresh start, never wrong numbers */
  int wrong = 0;
  for (uint32_t i = 0; i < cn; i += 3) {
    d = file_data(PT_SAVE_NAME, &n);
    memcpy(d, good, n);
    d[100] ^= 0x10;
    uint8_t *c = file_data(PT_COPY_NAME, &cn);
    memcpy(c, copy, cn);
    c[i] ^= 0x04;
    memset(&b, 0, sizeof b);
    bool ok = save_load(&b);
    if (ok && memcmp(&a.p, &b.p, sizeof a.p)) wrong++;
    pt_profile z;
    pt_fresh(&z);
    if (!ok && memcmp(&b.p, &z, sizeof z)) wrong++;
  }
  CHECK(!wrong, "%d damaged saves read as something else", wrong);
  /* cut short, too long, another version, numbers out of range */
  forget_all();
  save_write(&a);
  forget(PT_COPY_NAME);
  d = file_data(PT_SAVE_NAME, &n);
  files[0].len = n - 1;
  CHECK(!save_load(&b), "a save cut short was read");
  forget_all();
  pt_file e = a;
  e.p.best[3] = PT_MAX_TIME + 1;
  save_write(&e);
  CHECK(!save_load(&b), "a time out of range was read");
  e = a;
  e.p.split[2][1] = 1;              /* (before the one before it) */
  save_write(&e);
  CHECK(!save_load(&b), "checkpoint times out of order were read");
  e = a;
  e.p.paint = PT_PAINTS;
  save_write(&e);
  CHECK(!save_load(&b), "a paint out of range was read");
  /* only the copy: the save comes back from it */
  forget_all();
  save_write(&a);
  forget(PT_SAVE_NAME);
  CHECK(save_load(&b) && !memcmp(&a.p, &b.p, sizeof a.p), "the copy didn't bring the save back");
  CHECK(file_data(PT_SAVE_NAME, &n), "the save wasn't put back from its copy");
  /* a full storage: the write says so, nothing breaks */
  forget_all();
  storage_full = true;
  CHECK(!save_write(&a), "a write to a full storage said it worked");
  CHECK(!ghosts_put(0, 1000, good, 100), "a ghost written to a full storage said it worked");
  storage_full = false;
  /* the app with a full storage: it races, finishes, says it couldn't save */
  forget_all();
  reset_app();
  storage_full = true;
  tick = 1, nap = 0, frame_gap = 2000;
  script = script_all;
  sv.p.track = 0;
  quit_at = 120000;
  pt_app_main();
  CHECK(ui.save_failed, "the app didn't see the full storage");
  storage_full = false;
}

/* the ghosts' file: the last raced first, within 12 KB; damaged: gone */
static void test_ghost_file(void) {
  forget_all();
  ghosts_load();
  ghost_entry g;
  CHECK(!ghosts_find(0, &g), "a ghost without a file");
  static uint8_t data[PT_GHOST_MAX];
  for (int i = 0; i < PT_GHOST_MAX; i++) data[i] = (uint8_t)(i * 7 + 3);
  for (int t = 0; t < PT_NTRACKS; t++) {
    uint16_t len = (uint16_t)(1500 + t * 300);
    CHECK(ghosts_put(t, 40000u + (uint32_t)t, data, len), "ghost %d not written", t);
    CHECK(ghosts_find(t, &g) && g.len == len && g.ms == 40000u + (uint32_t)t && !memcmp(g.data, data, len),
          "ghost %d didn't read back", t);
  }
  uint32_t n;
  uint8_t *d = file_data(PT_GHOST_NAME, &n);
  CHECK(d && n <= PT_GHOST_BYTES, "the ghosts' file is %u bytes", n);
  ghosts_load();
  CHECK(ghosts_find(PT_NTRACKS - 1, &g) && !ghosts_find(0, &g), "the ghosts kept aren't the last ones raced");
  /* a long ghost pushes the others out, a short one keeps them */
  CHECK(ghosts_put(5, 99999, data, PT_GHOST_MAX), "the longest ghost wasn't kept");
  CHECK(ghosts_find(5, &g) && g.len == PT_GHOST_MAX, "the longest ghost didn't read back");
  /* damaged: every byte in turn, no ghost read wrong */
  d = file_data(PT_GHOST_NAME, &n);
  static uint8_t good[PT_GHOST_BYTES];
  memcpy(good, d, n);
  int wrong = 0;
  for (uint32_t i = 0; i < n; i += 37) {
    d = file_data(PT_GHOST_NAME, &n);
    memcpy(d, good, n);
    d[i] ^= 0x21;
    ghosts_load();
    if (ghosts_find(5, &g)) wrong++;
  }
  CHECK(!wrong, "%d damaged ghost files were read", wrong);
  d = file_data(PT_GHOST_NAME, &n);
  memcpy(d, good, n);
  ghosts_load();
  CHECK(ghosts_find(5, &g), "the ghosts' file wasn't read back");
}

/* random keys: anything goes, nothing breaks, the save stays good */
static uint32_t rng = 12345;
static uint32_t rnd(void) { return rng = rng * 1103515245u + 12345u, rng >> 8; }
static uint32_t held;
static uint32_t script_random(uint32_t n) {
  if (n % 20 == 0) {
    static const uint32_t K[] = {PK_OK, PK_UP, PK_DOWN, PK_LEFT, PK_RIGHT, PK_BACK, PK_RESTART, PK_VIEW, PK_UP | PK_LEFT,
                                 PK_UP | PK_RIGHT, PK_UP, PK_UP, 0, 0};
    held = K[rnd() % (sizeof K / sizeof K[0])];
  }
  return held;
}
static void test_random(int sessions) {
  int bad = 0;
  for (int s = 0; s < sessions; s++) {
    if (s % 50 == 0) forget_all();
    reset_app();
    rng = 777u + (uint32_t)s * 7919u;
    tick = 5, nap = 1, frame_gap = 400;
    script = script_random;
    quit_at = 1500 + rnd() % 3000;
    pt_app_main();
    pt_file f;
    uint32_t n;
    if (file_data(PT_SAVE_NAME, &n) && !save_load(&f)) bad++;
  }
  CHECK(!bad, "%d random sessions left a bad save", bad);
  printf("random keys: %d sessions\n", sessions);
}

/* ONLY="saves ghost": the parts named, else all */
static bool want(const char *only, const char *name) {
  if (!only) return true;
  size_t n = strlen(name);
  for (const char *p = only; (p = strstr(p, name)); p += n)
    if ((p == only || p[-1] == ' ' || p[-1] == ',') && (p[n] == 0 || p[n] == ' ' || p[n] == ',')) return true;
  return false;
}

int main(int argc, char **argv) {
  if (argc < 3) {
    printf("play test|shots|view|film|probe DIR ...\n");
    return 2;
  }
  const char *dir = argv[2];
  if (!strcmp(argv[1], "view")) {
    int t = argc > 3 ? atoi(argv[3]) : 0;
    uint32_t ms = argc > 4 ? (uint32_t)atoi(argv[4]) : 0;
    world_load(t);
    route_load(t);
    car_reset(&car);
    chase_reset(&chase, car.pos, car.rot);
    for (uint32_t f = 0; f < ms; f++) {
      car_step(&car, route_keys(f));
      if (f % 33 == 0) chase_update(&chase, &cam, 0.033f, car.pos, car.rot, car.speed);
    }
    sv.p.track = (uint8_t)t;
    rd_detail = argc > 5 ? atoi(argv[5]) : RD_LEVELS - 1;
    render_race(&car, &chase, 0.0f);
    printf("track %s frame %u pos %.2f %.2f %.2f speed %.1f cp %d tris %d (mountains %d, chunks %d)%s\n", pt_tracks[t].name, car.frames,
           (double)car.pos.x, (double)car.pos.y, (double)car.pos.z, (double)car.speed, car.next_cp, rd_tris, rd_mtris, rd_chunks,
           rd_dropped ? " (dropped)" : "");
    shot(dir, "view");
    return 0;
  }
  if (!strcmp(argv[1], "probe")) {
    int t = argc > 3 ? atoi(argv[3]) : 0;
    uint32_t ms = argc > 4 ? (uint32_t)atoi(argv[4]) : 0;
    unsigned keys = argc > 5 ? (unsigned)atoi(argv[5]) : IN_UP;
    world_load(t);
    route_load(t);
    car_reset(&car);
    for (uint32_t f = 0; f < ms; f++) car_step(&car, route_keys(f));
    for (int f = 0; f <= 1000; f++) {
      if (f % 100 == 0) {
        printf("f %u pos %.2f %.2f %.2f v %.2f %.2f %.2f ang %.2f %.2f %.2f spd %.1f cp %d cache %d up %.2f %.2f %.2f\n",
               car.frames, (double)car.pos.x, (double)car.pos.y, (double)car.pos.z, (double)car.vel.x, (double)car.vel.y,
               (double)car.vel.z, (double)car.ang.x, (double)car.ang.y, (double)car.ang.z, (double)car.speed, car.next_cp,
               car.ncache, (double)car.basis.r[0].y, (double)car.basis.r[1].y, (double)car.basis.r[2].y);
        for (int i = 0; i < 4; i++)
          printf("   w%d contact %d susp %.3f force %.0f skid %.2f\n", i, car.w[i].contact, (double)car.w[i].susp,
                 (double)car.w[i].force, (double)car.w[i].skid);
      }
      car_step(&car, keys);
    }
    return 0;
  }
  if (!strcmp(argv[1], "shots")) {
    /* every screen: play shots DIR */
    memset(&sv, 0, sizeof sv);
    pt_fresh(&sv.p);
    sv.p.guide = 1;
    for (int t = 0; t < PT_NTRACKS; t++)
      if (t % 3 != 2) sv.p.best[t] = 31000u + (uint32_t)t * 2345u;
    rd_detail = RD_LEVELS - 1;
    world_load(0);
    car_reset(&car);
    look = car_look_of(&car);
    memset(&ui, 0, sizeof ui);
    static const struct { uint8_t screen, sel, page, tab; const char *name; } S[] = {
      {V_TITLE, 0, 0, 0, "title"}, {V_QUIT, 1, 0, 0, "quit"}, {V_TRACKS, 1, 0, 0, "tracks"},
      {V_TRACKS, 2, 0, 2, "tracks_desert"}, {V_CAR, 0, 0, 0, "car"}, {V_GUIDE, 0, 0, 0, "guide"},
      {V_HELP, 0, 1, 0, "help2"}, {V_HELP, 0, 2, 0, "help3"},
    };
    for (unsigned i = 0; i < sizeof S / sizeof S[0]; i++) {
      ui.screen = S[i].screen, ui.sel = S[i].sel, ui.page = S[i].page, ui.tab = S[i].tab;
      ui.clock = 7000;
      sv.p.paint = S[i].screen == V_CAR ? 1 : 0;
      look.paint = sv.p.paint;
      chase_update(&chase, &cam, 0.0f, look.pos, look.rot, 0.0f);
      pt_render(&sv, &car, &cam, &look, NULL, &ui);
      shot(dir, S[i].name);
    }
    /* the race: Summer 1, its run some way on, then the pause, leaving, the finish */
    world_load(0);
    route_load(0);
    car_reset(&car);
    chase_reset(&chase, car.pos, car.rot);
    memset(&ui, 0, sizeof ui);
    ui.screen = V_RACE;
    render_race(&car, &chase, 0.0f);
    ui.started = 0;
    pt_render(&sv, &car, &cam, &look, NULL, &ui);
    shot(dir, "start");
    uint32_t at = argc > 3 ? (uint32_t)atoi(argv[3]) : 12000;
    for (uint32_t f = 0; f < at; f++) {
      car_step(&car, route_keys(f));
      if (f % 33 == 0) chase_update(&chase, &cam, 0.033f, car.pos, car.rot, car.speed);
    }
    ui.clock = 100000, ui.split_at = ui.clock - 1000, ui.split_cp = 0, ui.split_diff = -1234;
    render_race(&car, &chase, 0.0f);
    shot(dir, "race");
    ui.screen = V_PAUSE, ui.sel = 0;
    pt_render(&sv, &car, &cam, &look, NULL, &ui);
    shot(dir, "pause");
    ui.screen = V_LEAVE, ui.sel = 0;
    pt_render(&sv, &car, &cam, &look, NULL, &ui);
    shot(dir, "leave");
    while (!car.finish && car.frames < 600000) car_step(&car, route_keys(car.frames));
    for (int i = 0; i < 60; i++) chase_update(&chase, &cam, 0.033f, car.pos, car.rot, car.speed);
    ui.screen = V_RESULT, ui.sel = 0, ui.new_record = 1, ui.finish_diff = -2345, ui.finish_at = 1, ui.split_at = 0;
    pt_render(&sv, &car, &cam, &look, NULL, &ui);
    shot(dir, "result");
    printf("shots in %s\n", dir);
    return 0;
  }
  if (!strcmp(argv[1], "cam")) {
    /* a picture from anywhere: play cam DIR T X Y Z TX TY TZ */
    int t = atoi(argv[3]);
    v3 p = V3((float)atof(argv[4]), (float)atof(argv[5]), (float)atof(argv[6]));
    v3 at = V3((float)atof(argv[7]), (float)atof(argv[8]), (float)atof(argv[9]));
    world_load(t);
    camera_t c;
    c.pos = p, c.fwd = vnorm(vsub(at, p));
    c.right = vnorm(vcross(c.fwd, V3(0.0f, 1.0f, 0.0f)));
    c.up = vcross(c.right, c.fwd);
    c.fov = 70.0f;
    rd_detail = RD_LEVELS - 1;
    rd_begin(&c, W.t->env, W.t->sun, false);
    rd_track();
    color_t *old = gfx_buf;
    static color_t st[320 * 24];
    gfx_buf = st;
    for (int y = 0; y < 240; y += 24) {
      gfx_y0 = y, gfx_y1 = y + 24;
      rd_draw();
      plat_push(0, y, 320, 24, st);
    }
    gfx_buf = old;
    shot(dir, "cam");
    return 0;
  }
  if (!strcmp(argv[1], "film")) {
    int t = argc > 3 ? atoi(argv[3]) : 0;
    uint32_t ms = argc > 4 ? (uint32_t)atoi(argv[4]) : 10000, step = argc > 5 ? (uint32_t)atoi(argv[5]) : 40;
    world_load(t);
    route_load(t);
    car_reset(&car);
    chase_reset(&chase, car.pos, car.rot);
    sv.p.track = (uint8_t)t;
    rd_detail = RD_LEVELS - 1;
    int n = 0;
    for (uint32_t f = 0; f < ms; f++) {
      car_step(&car, route_keys(f));
      if (f % step == 0) {
        render_race(&car, &chase, (float)step / 1000.0f);
        char path[512];
        snprintf(path, sizeof path, "%s/%06d_%06u.raw", dir, n++, f);
        FILE *o = fopen(path, "wb");
        if (!o) return 2;
        fwrite(fb, 1, sizeof fb, o);
        fclose(o);
      }
    }
    return 0;
  }
  if (!strcmp(argv[1], "test")) {
    const char *only = getenv("ONLY");
#define PART(name, call) if (want(only, name)) { call; fflush(stdout); }
    PART("runs", test_runs());
    PART("checkpoints", test_checkpoints());
    PART("respawn", test_respawn());
    PART("ghost", test_ghost());
    PART("saves", test_saves());
    PART("ghostfile", test_ghost_file());
    PART("restart", test_restart_home());
    PART("app", test_app());
    PART("random", test_random(300));
    if (fails) printf("%d of %d checks FAILED\n", fails, checks);
    else printf("all good (%d checks)\n", checks);
    return fails ? 1 : 0;
  }
  return 2;
}
