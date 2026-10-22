/* Plays Hill Climb Racing on a computer, with scripted keys, and saves frames.
 *
 *   play [--frames N] [--keys "10-40:right,50:ok,..."] [--shots 30,60|every:N] [--out DIR] [--saves DIR]
 *        [--run STAGE,VEHICLE] [--levels E,S,T,A] [--coins N] [--auto] [--trace N] [--fuzz SEED]
 *        [--numbers VEHICLE] [--ground STAGE]
 *
 * Keys: left right up down ok back home. "a-b:key" holds the key from frame a to b, "a:key" for one frame.
 * --run starts a run on that stage with that vehicle, past the menus; --auto then drives it like a player would
 * (gas, and the pedals to land on the wheels). Screenshots are DIR/shot_<frame>.ppm.
 * --fuzz presses random keys (its own generator, the same on every computer) through menus and runs; --numbers
 * prints the numbers a vehicle is built with (at --levels); --ground checks a stage's ground, bridges, pits and
 * coins over 6 km.
 * At the end it prints the state: END mode M dist D coins C total T over O end E level L flips F. */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include "../src/game.h"
#include "../src/gfx.h"

extern uint32_t host_keys, host_time;
void host_shot(const char *path);
void host_save_dir(const char *d);

typedef struct { int a, b; uint32_t k; } Hold;
static Hold holds[4096];
static int nholds;
static int shots[4096], nshots, every, every_from, every_to = 1 << 30;

static uint32_t key_of(const char *s) {
  static const char *const names[] = {"left", "right", "up", "down", "ok", "back", "home"};
  static const uint32_t bits[] = {K_LEFT, K_RIGHT, K_UP, K_DOWN, K_OK, K_BACK, K_HOME};
  for (int i = 0; i < 7; i++)
    if (!strcmp(s, names[i])) return bits[i];
  fprintf(stderr, "unknown key %s\n", s);
  exit(2);
}

static void parse_keys(const char *spec) {
  char buf[65536];
  snprintf(buf, sizeof buf, "%s", spec);
  for (char *tok = strtok(buf, ","); tok; tok = strtok(NULL, ",")) {
    int a, b;
    char k[16];
    if (sscanf(tok, "%d-%d:%15s", &a, &b, k) == 3) holds[nholds++] = (Hold){a, b, key_of(k)};
    else if (sscanf(tok, "%d:%15s", &a, k) == 2) holds[nholds++] = (Hold){a, a, key_of(k)};
  }
}

/* a driver: gas, and in the air the pedals to land level with the ground ahead */
static bool flipper;   /* --flips: backflips in the air when there is time */
static float air_rot, last_a, best_x;
static int stuck, backing, tries;
static uint32_t autopilot(void) {
  const Car *c = &run.car;
  if (run.over) return 0;
  /* stuck at the foot of a hill: back up the last one and take a run at it, as a player would */
  if (backing && c->ch.x > best_x - 4 - 3 * tries) return backing--, K_LEFT;
  backing = 0;
  if (c->ch.x > best_x + 1) best_x = c->ch.x, stuck = 0, tries = 0;
  else if (++stuck > 90 && run.fuel > 0) {   /* (further back each time) */
    stuck = 0, backing = 45 + 40 * (tries < 4 ? tries++ : tries);
    return K_LEFT;
  }
  air_rot = car_airborne(c) ? air_rot + (c->ch.a - last_a) : 0;
  last_a = c->ch.a;
  float x = c->ch.x, slope = atan2f(stage_deck(x + 1.5f) - stage_deck(x - 1.5f), 3.0f);
  float a = c->ch.a;
  while (a > 3.14159f) a -= 6.28318f;
  while (a < -3.14159f) a += 6.28318f;
  float d = a - slope;
  if (car_airborne(c)) {
    if (flipper && air_rot < 2.4f) {   /* time left in the air against the turn still to make */
      float h = c->ch.y - stage_deck(c->ch.x), g = stage->gravity, vy = c->vy;
      float left = (vy + sqrtf(vy * vy + 2 * g * (h > 0 ? h : 0))) / g, w = c->w > 2 ? c->w : 2;
      if (left > (6.28f - air_rot) / w + 0.25f) return K_RIGHT;
    }
    if (d > 0.25f || (c->w > 1.2f && d > -0.9f)) return K_LEFT;
    if (d < -0.35f || (c->w < -1.2f && d < 0.9f)) return K_RIGHT;
    return 0;
  }
  if (d > 0.6f) return K_LEFT;        /* rearing up: a touch of brake brings the nose down */
  if (d > 0.3f && c->w > 0.6f) return 0;   /* coming up fast: ease off */
  if (car_speed(c) < 0.3f && run.fuel <= 0) return 0;
  return K_RIGHT;
}

/* --fuzz: a key or two at random, changing every few frames (an LCG, not libc's rand: the same everywhere) */
static uint32_t fuzz_state;
static uint32_t fuzz_next(void) {
  fuzz_state = fuzz_state * 1103515245u + 12345u;
  return fuzz_state >> 8;
}
static uint32_t fuzz_keys(int f) {
  static uint32_t k;
  if (f % 6 == 0) {
    static const uint32_t pool[] = {K_RIGHT, K_RIGHT, K_RIGHT, K_OK, K_OK, K_LEFT, K_LEFT, K_UP, K_DOWN, K_BACK, 0, 0};
    uint32_t r = fuzz_next();
    k = pool[r % 12];
    if (r >> 12 & 1) k |= pool[(r >> 4) % 12];
    if ((r >> 16) % 500 == 0) k = K_HOME;   /* (now and then Home: the game saves and quits, and starts again) */
  }
  return k;
}

/* --ground: the stage's ground over 6 km: finite, its bridges and pits in order and apart, its coins and fuel cans
 * over the ground; prints a line OK or what is wrong */
static int ground_check(int s) {
  stage_set(s);
  float last = stage_floor(0);
  for (float x = -40; x < 6000; x += 0.5f) {
    float h = stage_floor(x);
    if (!(h == h) || h > 1e6f || h < -1e6f) return printf("GROUND bad height %f at %f\n", h, x), 1;
    float roof;
    if (stage_ceiling(x, &roof) && roof <= stage_deck(x)) return printf("GROUND ceiling under the ground at %f\n", x), 1;
    last = h;
  }
  (void)last;
  float prev = -1e9f;
  int nb = 0, np = 0;
  for (int k = 0;; k++) {
    float x0, len, y;
    if (!stage_bridge_k(k, &x0, &len, &y) || x0 > 6000) break;
    if (x0 < prev + 10 || len < 8 || len > 18) return printf("GROUND bridge %d at %f len %f\n", k, x0, len), 1;
    prev = x0 + len, nb++;
  }
  prev = -1e9f;
  for (int k = 0;; k++) {
    float x0, len, y;
    if (!stage_pit_k(k, &x0, &len, &y) || x0 > 6000) break;
    if (x0 < prev + 10 || len < 15 || len > 25) return printf("GROUND pit %d at %f len %f\n", k, x0, len), 1;
    if (stage_deck(x0 + len * 0.5f) != y || stage_floor(x0 + len * 0.5f) > y - 0.5f)
      return printf("GROUND pit %d has no dip\n", k), 1;
    prev = x0 + len, np++;
  }
  int coins = 0;
  for (int k = 1; k < 200; k++) {
    float cx[14], cy[14];
    int val[14];
    int m = stage_coin_group(k, cx, cy, val);
    for (int i = 0; i < m; i++, coins++)
      if (cy[i] < stage_floor(cx[i]) + 0.5f) return printf("GROUND coin under the ground at %f\n", cx[i]), 1;
  }
  int cans = 0;
  float fx, fy;
  while (stage_fuel(cans, &fx, &fy) && fx < 6000) {
    if (fy < stage_floor(fx) + 0.5f) return printf("GROUND fuel can under the ground at %f\n", fx), 1;
    cans++;
  }
  printf("GROUND ok bridges %d pits %d coins %d cans %d\n", nb, np, coins, cans);
  return 0;
}

int main(int argc, char **argv) {
  int frames = 600, run_stage = -1, run_vehicle = 0, trace = 0, numbers = -1;
  bool fuzz = false;
  bool autop = false;
  const char *out = "build/shots";
  int levels[4] = {1, 1, 1, 1};
  long coins = -1;
  for (int i = 1; i < argc; i++) {
    if (!strcmp(argv[i], "--frames")) frames = atoi(argv[++i]);
    else if (!strcmp(argv[i], "--keys")) parse_keys(argv[++i]);
    else if (!strcmp(argv[i], "--shots")) {
      const char *s = argv[++i];
      if (!strncmp(s, "every:", 6)) every = atoi(s + 6);
      else if (strchr(s, '-') && strchr(s, ':')) sscanf(s, "%d-%d:%d", &every_from, &every_to, &every);
      else
        for (const char *p = s; *p;) {
          shots[nshots++] = atoi(p);
          while (*p && *p != ',') p++;
          if (*p) p++;
        }
    } else if (!strcmp(argv[i], "--out")) out = argv[++i];
    else if (!strcmp(argv[i], "--saves")) host_save_dir(argv[++i]);
    else if (!strcmp(argv[i], "--run")) sscanf(argv[++i], "%d,%d", &run_stage, &run_vehicle);
    else if (!strcmp(argv[i], "--levels")) sscanf(argv[++i], "%d,%d,%d,%d", &levels[0], &levels[1], &levels[2], &levels[3]);
    else if (!strcmp(argv[i], "--coins")) coins = atol(argv[++i]);
    else if (!strcmp(argv[i], "--auto")) autop = true;
    else if (!strcmp(argv[i], "--nohud")) g_nohud = true;
    else if (!strcmp(argv[i], "--flips")) autop = flipper = true;
    else if (!strcmp(argv[i], "--trace")) trace = atoi(argv[++i]);
    else if (!strcmp(argv[i], "--fuzz")) fuzz = true, fuzz_state = (uint32_t)atol(argv[++i]);
    else if (!strcmp(argv[i], "--numbers")) numbers = atoi(argv[++i]);
    else if (!strcmp(argv[i], "--ground")) return ground_check(atoi(argv[++i]));
    else if (!strcmp(argv[i], "--things")) {   /* where the first bridges, tunnels and fuel cans of a stage are */
      stage_set(atoi(argv[++i]));
      float b0, len, by, f, fy;
      for (int k = 0; stage_bridge_k(k, &b0, &len, &by) && b0 < 2000; k++)
        printf("bridge %.0f to %.0f at %.1f\n", b0, b0 + len, by);
      for (int k = 0; k < 8 && stage_fuel(k, &f, &fy); k++) printf("fuel %.1f %.1f\n", f, fy);
      return 0;
    } else if (!strcmp(argv[i], "--slopes")) {   /* the steepest slope of each 100 m of a stage, in degrees */
      stage_set(atoi(argv[++i]));
      for (int s = 0; s < 4000; s += 100) {
        float most = 0, lo = 1e9f, hi = -1e9f;
        for (float x = (float)s; x < s + 100; x += 0.5f) {
          float a = atanf((stage_floor(x + 0.5f) - stage_floor(x)) / 0.5f) * 57.3f, h = stage_floor(x);
          if (fabsf(a) > fabsf(most)) most = a;
          if (h < lo) lo = h;
          if (h > hi) hi = h;
        }
        printf("%5d m: steepest %5.1f, heights %6.1f to %6.1f\n", s, most, lo, hi);
      }
      return 0;
    }
  }
  mkdir("build", 0755);
  mkdir(out, 0755);
  static uint16_t band[SCREEN_W * GFX_BAND];
  gfx_init(band);
  game_init();
  if (coins >= 0) sv.coins = (uint32_t)coins;
  if (numbers >= 0) {   /* the vehicle on the Countryside at those levels */
    uint8_t lv[4];
    for (int u = 0; u < 4; u++) lv[u] = (uint8_t)levels[u];
    stage_set(run_stage >= 0 ? run_stage : 0);
    Car car;
    car_init(&car, numbers, lv);
    float n[12];
    car_numbers(n);
    printf("NUMBERS");
    for (int k = 0; k < 12; k++) printf(" %.6g", n[k]);
    printf(" fuel %.6g\n", car_fuel_rate(numbers, lv));
    car_free();
    return 0;
  }
  if (run_stage >= 0) {
    sv.vehicles |= (uint16_t)(1 << run_vehicle), sv.stages |= (uint16_t)(1 << run_stage);
    for (int u = 0; u < 4; u++) sv.level[run_vehicle][u] = (uint8_t)levels[u];
    sv.vehicle = (uint8_t)run_vehicle, sv.stage = (uint8_t)run_stage, sv.seen_help = 1;
    run_start(run_vehicle, run_stage);
    g_mode = MODE_RUN;
  }
  for (int f = 0; f < frames && (g_running || fuzz); f++) {
    if (!g_running) {   /* (--fuzz: the game quit; started again, from its save) */
      g_running = true;
      game_init();
    }
    uint32_t k = 0;
    for (int i = 0; i < nholds; i++)
      if (f >= holds[i].a && f <= holds[i].b) k |= holds[i].k;
    if (autop && g_mode == MODE_RUN && !k) k = autopilot();   /* (--keys take over while they are held) */
    if (fuzz && !k) k = fuzz_keys(f);
    host_keys = k;
    game_frame(k);
    host_time += 17;
    bool shot = every && f >= every_from && f <= every_to ? (f - every_from) % every == 0 : false;
    for (int i = 0; i < nshots; i++) shot |= shots[i] == f;
    if (shot) {
      game_draw();
      char p[512];
      snprintf(p, sizeof p, "%s/shot_%06d.ppm", out, f);
      host_shot(p);
    }
    if (trace && f % trace == 0 && (g_mode == MODE_RUN || g_mode == MODE_END))
      printf("f %d x %.2f y %.2f a %.3f v %.2f fuel %.1f air %d dead %d dist %.0f coins %u\n", f, run.car.ch.x,
             run.car.ch.y, run.car.ch.a, car_speed(&run.car), run.fuel, run.car.air, run.car.dead,
             run.dist, (unsigned)run_total());
  }
  extern long b2_heap_peak;
  fprintf(stderr, "heap peak %ld\n", b2_heap_peak);
  printf("END mode %d dist %d coins %u total %u over %d end %d level %d flips %u saved %u running %d\n", g_mode,
         (int)run.dist, (unsigned)run.coins, (unsigned)run_total(), run.over, run.end, run.level, (unsigned)run.flips,
         (unsigned)sv.coins, g_running);
  return 0;
}
