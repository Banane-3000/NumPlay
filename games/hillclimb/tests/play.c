/* Plays Hill Climb Racing on a computer, with scripted keys, and saves frames.
 *
 *   play [--frames N] [--keys "10-40:right,50:ok,..."] [--shots 30,60|every:N] [--out DIR] [--saves DIR]
 *        [--run STAGE,VEHICLE] [--levels E,S,T,A] [--coins N] [--auto] [--trace N]
 *
 * Keys: left right up down ok back home. "a-b:key" holds the key from frame a to b, "a:key" for one frame.
 * --run starts a run on that stage with that vehicle, past the menus; --auto then drives it like a player would
 * (gas, and the pedals to land on the wheels). Screenshots are DIR/shot_<frame>.ppm.
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
static uint32_t autopilot(void) {
  const Car *c = &run.car;
  if (run.over) return 0;
  float x = c->ch.x, slope = atan2f(stage_floor(x + 1.5f) - stage_floor(x - 1.5f), 3.0f);
  float a = c->ch.a;
  while (a > 3.14159f) a -= 6.28318f;
  while (a < -3.14159f) a += 6.28318f;
  float d = a - slope;
  if (car_airborne(c)) {
    if (flipper && run.air_rot < 2.4f) {   /* time left in the air against the turn still to make */
      float h = c->ch.y - stage_floor(c->ch.x), g = stage->gravity, vy = c->ch.vy;
      float left = (vy + sqrtf(vy * vy + 2 * g * (h > 0 ? h : 0))) / g, w = c->ch.w > 2 ? c->ch.w : 2;
      if (left > (6.28f - run.air_rot) / w + 0.25f) return K_RIGHT;
    }
    if (d > 0.25f || (c->ch.w > 1.2f && d > -0.9f)) return K_LEFT;
    if (d < -0.35f) return K_RIGHT;
    return 0;
  }
  if (d > 0.6f) return K_LEFT;        /* rearing up: a touch of brake brings the nose down */
  if (d > 0.3f && c->ch.w > 0.6f) return 0;   /* coming up fast: ease off */
  if (car_speed(c) < 0.3f && run.fuel <= 0) return 0;
  return K_RIGHT;
}

int main(int argc, char **argv) {
  int frames = 600, run_stage = -1, run_vehicle = 0, trace = 0;
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
    else if (!strcmp(argv[i], "--things")) {   /* where the first bridges, tunnels and fuel cans of a stage are */
      stage_set(atoi(argv[++i]));
      float b0, b1, c, f;
      for (float x = 0; x < 2000; x += 1) {
        if (stage_bridge(x, &b0, &b1)) printf("bridge %.0f to %.0f\n", b0, b1), x = b1;
      }
      bool in = false;
      for (float x = 0; x < 2000; x += 1)
        if (stage_ceiling(x, &c) != in) in = !in, printf("ceiling %s at %.0f\n", in ? "starts" : "ends", x);
      for (int k = 0; k < 8 && stage_fuel(k, &f); k++) printf("fuel %.0f\n", f);
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
  if (run_stage >= 0) {
    sv.vehicles |= (uint16_t)(1 << run_vehicle), sv.stages |= (uint16_t)(1 << run_stage);
    for (int u = 0; u < 4; u++) sv.level[run_vehicle][u] = (uint8_t)levels[u];
    sv.vehicle = (uint8_t)run_vehicle, sv.stage = (uint8_t)run_stage, sv.seen_help = 1;
    run_start(run_vehicle, run_stage);
    g_mode = MODE_RUN;
  }
  for (int f = 0; f < frames && g_running; f++) {
    uint32_t k = 0;
    for (int i = 0; i < nholds; i++)
      if (f >= holds[i].a && f <= holds[i].b) k |= holds[i].k;
    if (autop && g_mode == MODE_RUN) k |= autopilot();
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
      printf("f %d x %.1f y %.1f a %.2f v %.1f fuel %.1f air %d head %d dist %.0f coins %u\n", f, run.car.ch.x,
             run.car.ch.y, run.car.ch.a, car_speed(&run.car), run.fuel, car_airborne(&run.car), run.car.head_hit,
             run.dist, (unsigned)run_total());
  }
  printf("END mode %d dist %d coins %u total %u over %d end %d level %d flips %u saved %u running %d\n", g_mode,
         (int)run.dist, (unsigned)run.coins, (unsigned)run_total(), run.over, run.end, run.level, (unsigned)run.flips,
         (unsigned)sv.coins, g_running);
  return 0;
}
