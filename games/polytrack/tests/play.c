/* PolyTrack on a computer, for tests and pictures (make host, make test).
 *
 *   play test DIR        every official track driven to the finish by its recorded run, to the same frame and hash
 *                        on every computer; the checkpoints in order, respawns, restarts; the whole app through its
 *                        keys; saves written and read back, damaged saves, a full storage, the ghosts; a few hundred
 *                        sessions of random keys
 *   play shots DIR       pictures of the screens (DIR/<name>.ppm)
 *   play view DIR T MS   a picture of track T after MS ms of its recorded run (DIR/view.ppm)
 *   play film DIR T MS STEP   MS ms of track T's recorded run, a frame every STEP ms (DIR/<frame>_<ms>.raw, for
 *                        tools/record.py --frames)
 */
#define main pt_app_main
#include "../src/main.c"
#undef main
#include <stdio.h>
#include <stdlib.h>
#include "routes.h"

/* ---------------------------------------------------------------- the calculator, on a computer */
static uint16_t fb[240][320];
void plat_push(int x, int y, int w, int h, const uint16_t *px) {
  for (int j = 0; j < h; j++)
    if (y + j < 240)
      for (int i = 0; i < w; i++) fb[y + j][x + i] = px[j * w + i];
}
void *np_alloc(uint32_t n) { (void)n; return 0; }
static double clock_ms;
static uint32_t (*script)(uint32_t frame);
static uint32_t frame_no, quit_at;
static void (*watch)(void);
uint32_t plat_keys(void) {
  clock_ms += 5;
  frame_no++;
  if (watch) watch();
  if (frame_no >= quit_at) return PK_HOME;
  return script ? script(frame_no) : 0;
}
uint32_t plat_millis(void) { return (uint32_t)clock_ms; }
void plat_sleep(uint32_t ms) { clock_ms += ms; }
void plat_vblank(void) {}
bool plat_slow(void) { return false; }
void plat_begin(void) {}
int plat_end(void) { return 0; }

#define NFILES 6
static struct { char name[32]; uint8_t data[16384]; uint32_t len; bool on; } files[NFILES];
static bool storage_full;
bool plat_save(const char *name, const void *data, uint32_t len) {
  if (storage_full) return false;
  int k = -1;
  for (int i = 0; i < NFILES; i++)
    if (files[i].on && !strcmp(files[i].name, name)) k = i;
  for (int i = 0; i < NFILES && k < 0; i++)
    if (!files[i].on) k = i;
  if (k < 0 || len > sizeof files[k].data) return false;
  strcpy(files[k].name, name), memcpy(files[k].data, data, len), files[k].len = len, files[k].on = true;
  return true;
}
const uint8_t *plat_load(const char *name, uint32_t *len) {
  for (int i = 0; i < NFILES; i++)
    if (files[i].on && !strcmp(files[i].name, name)) return *len = files[i].len, files[i].data;
  return 0;
}
static void forget(const char *name) {
  for (int i = 0; i < NFILES; i++)
    if (files[i].on && !strcmp(files[i].name, name)) files[i].on = false;
}
static uint8_t *file_data(const char *name, uint32_t *len) {
  for (int i = 0; i < NFILES; i++)
    if (files[i].on && !strcmp(files[i].name, name)) return *len = files[i].len, files[i].data;
  return 0;
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

static int fails;
#define CHECK(c, ...) do { if (!(c)) { fails++; printf("FAIL %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

/* ---------------------------------------------------------------- a recorded run: the keys held from a frame on */
static unsigned route_input(const route_t *r, uint32_t frame) {
  unsigned in = 0;
  for (int i = 0; i < r->n; i++) {
    if (r->ev[i].frame > frame) break;
    in = r->ev[i].keys;
  }
  return in;
}

/* drives track t with its route; returns the finish frame (0: not finished by max) */
static uint32_t drive(int t, const route_t *r, uint32_t max, uint32_t *hash) {
  world_load(t);
  static car_t c;
  car_reset(&c);
  for (uint32_t f = 0; f < max && !c.finish; f++) car_step(&c, route_input(r, f));
  if (hash) *hash = car_hash(&c);
  return c.finish;
}

static void render_race(const car_t *c, chase_t *ch, float dt) {
  look = car_look_of(c);
  look.paint = sv.p.paint;
  chase_update(ch, &cam, dt, look.pos, look.rot, c->speed);
  ui.screen = V_RACE, ui.started = 1;
  pt_render(&sv, c, &cam, &look, NULL, &ui);
}

int main(int argc, char **argv) {
  if (argc < 3) {
    printf("play test|shots|view|film DIR ...\n");
    return 2;
  }
  const char *dir = argv[2];
  if (!strcmp(argv[1], "view")) {
    int t = argc > 3 ? atoi(argv[3]) : 0;
    uint32_t ms = argc > 4 ? (uint32_t)atoi(argv[4]) : 0;
    world_load(t);
    car_reset(&car);
    chase_reset(&chase, car.pos, car.rot);
    for (uint32_t f = 0; f < ms; f++) {
      car_step(&car, route_input(&pt_routes[t], f));
      if (f % 33 == 0) chase_update(&chase, &cam, 0.033f, car.pos, car.rot, car.speed);
    }
    sv.p.track = (uint8_t)t;
    render_race(&car, &chase, 0.0f);
    printf("track %s frame %u pos %.2f %.2f %.2f speed %.1f cp %d tris %d%s\n", pt_tracks[t].name, car.frames,
           (double)car.pos.x, (double)car.pos.y, (double)car.pos.z, (double)car.speed, car.next_cp, rd_tris,
           rd_dropped ? " (dropped)" : "");
    shot(dir, "view");
    return 0;
  }
  if (!strcmp(argv[1], "film")) {
    int t = argc > 3 ? atoi(argv[3]) : 0;
    uint32_t ms = argc > 4 ? (uint32_t)atoi(argv[4]) : 10000, step = argc > 5 ? (uint32_t)atoi(argv[5]) : 40;
    world_load(t);
    car_reset(&car);
    chase_reset(&chase, car.pos, car.rot);
    sv.p.track = (uint8_t)t;
    int n = 0;
    for (uint32_t f = 0; f < ms; f++) {
      car_step(&car, route_input(&pt_routes[t], f));
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
    for (int t = 0; t < PT_NTRACKS; t++) {
      uint32_t h;
      uint32_t fin = drive(t, &pt_routes[t], 600000, &h);
      printf("%-9s %s %u ms, hash %08x\n", pt_tracks[t].name, fin ? "finished in" : "NOT FINISHED", fin, h);
      CHECK(fin, "%s not finished", pt_tracks[t].name);
    }
    printf(fails ? "%d FAILED\n" : "all good\n", fails);
    return fails ? 1 : 0;
  }
  (void)forget, (void)file_data, (void)storage_full, (void)quit_at, (void)script, (void)watch;
  return 2;
}
