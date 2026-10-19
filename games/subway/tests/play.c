/* Plays Subway Surfers on a computer, with scripted keys, and saves frames.
 *
 *   play [--frames N] [--keys "10-40:right,50:ok,..."] [--shots 30,60|all] [--out DIR] [--saves DIR]
 *        [--seed N] [--bot FROM] [--coins N] [--keys-owned N] [--draw-every N] [--power FRAME:KIND] [--god] [--when roof|tunnel] [--stats]
 *
 * Keys: left right up down ok back one three home. "a-b:key" holds the key from frame a to b, "a:key" for one
 * frame (frames are the game's steps, 60 a second). --bot plays the run from frame FROM on: it reads the
 * tracks ahead and changes lanes, jumps and rolls (it is no expert: it stumbles and crashes now and then).
 * Screenshots are DIR/shot_<frame>.ppm. --raw DIR:FROM-TO keeps the frames drawn from FROM to TO as raw RGB565
 * files for tools/record.py --frames (GIFs for the documentation). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include "../src/app.h"
#include "../src/game.h"
#include "../src/r3d.h"
#include "../src/save.h"

extern uint32_t host_keys, host_time, host_seed;
extern uint16_t host_fb[SCREEN_W * SCREEN_H];
void host_shot(const char *path);
void host_save_dir(const char *d);

typedef struct { int a, b; uint32_t k; } Hold;
static int power_at[8], power_kind[8], npower;
static Hold holds[4096];
static int nholds;

static uint32_t key_of(const char *s) {
  static const struct { const char *n; uint32_t k; } names[] = {
    {"left", K_LEFT}, {"right", K_RIGHT}, {"up", K_UP}, {"down", K_DOWN}, {"ok", K_OK}, {"back", K_BACK},
    {"one", K_ONE}, {"three", K_THREE}, {"home", K_HOME},
  };
  for (unsigned i = 0; i < sizeof names / sizeof names[0]; i++)
    if (!strcmp(s, names[i].n)) return names[i].k;
  fprintf(stderr, "play: unknown key %s\n", s);
  exit(2);
}

static void parse_keys(const char *spec) {
  char buf[65536];
  snprintf(buf, sizeof buf, "%s", spec);
  for (char *t = strtok(buf, ","); t && nholds < 4096; t = strtok(NULL, ",")) {
    char *colon = strchr(t, ':');
    if (!colon) continue;
    *colon = 0;
    int a = atoi(t), b = a + 1;
    char *dash = strchr(t, '-');
    if (dash) b = atoi(dash + 1) + 1;
    holds[nholds++] = (Hold){a, b, key_of(colon + 1)};
  }
}

/* ---------------------------------------------------------------- the bot */
/* a ramp up to this train? */
static bool ramped(const Obstacle *t) {
  for (int i = 0; i < nobs; i++)
    if (obs[i].type == O_RAMP && obs[i].lane == t->lane && obs[i].z1 > t->z0 - 0.5f && obs[i].z1 < t->z0 + 0.5f) return true;
  return false;
}

static bool blocked(int lane, float z0, float z1, float y) {
  for (int i = 0; i < nobs; i++) {
    const Obstacle *o = &obs[i];
    if (o->type == O_TRAIN && ramped(o) && o->z0 > z0 + 1) continue;
    /* (a train coming this way: where it will be by then) */
    float ahead = o->type == O_MOVING ? o->v * (z1 - z0) / pl.speed : 0;
    if (o->lane != lane || o->z1 < z0 || o->z0 - ahead > z1) continue;
    if (o->type == O_RAMP || o->type == O_HURDLE || o->type == O_HIGHBAR) continue;
    if ((o->type == O_TRAIN || o->type == O_MOVING) && y >= TRAIN_H - 0.1f) continue;
    return true;
  }
  return false;
}

static uint32_t bot(void) {
  static int wait;
  if (wait > 0) {
    wait--;
    return 0;
  }
  if (pl.dead) return 0;
  float look = pl.speed * 0.75f, z = pl.z;
  /* the nearest thing in this lane */
  const Obstacle *near = NULL;
  for (int i = 0; i < nobs; i++) {
    const Obstacle *o = &obs[i];
    float ahead = o->type == O_MOVING ? o->v * look / pl.speed : 0;
    if (o->lane != pl.lane || o->z1 < z || o->z0 - ahead > z + look) continue;
    if (o->type == O_RAMP) continue;
    if ((o->type == O_TRAIN || o->type == O_MOVING) && pl.y >= TRAIN_H - 0.1f) continue;
    if (!near || o->z0 < near->z0) near = o;
  }
  if (!near) return 0;
  /* on a roof, the next train along: jump the gap */
  if (pl.on_roof && near->type == O_TRAIN && near->z0 - z < pl.speed * 0.3f) {
    wait = 10;
    return K_UP;
  }
  float d = near->z0 - z - (near->type == O_MOVING ? near->v * (near->z0 - z) / (pl.speed + near->v) : 0);
  if (near->type == O_HURDLE && d < pl.speed * 0.25f && pl.state != ST_AIR) {
    wait = 8;
    return K_UP;
  }
  if (near->type == O_HIGHBAR && d < pl.speed * 0.22f && pl.roll <= 0) {
    wait = 8;
    return K_DOWN;
  }
  if (near->type == O_HURDLE || near->type == O_HIGHBAR) return 0;
  /* a train or a big barrier: another lane */
  int best = -1;
  for (int k = 0; k < 3; k++) {
    int l = pl.lane + (k == 0 ? -1 : k == 1 ? 1 : 0);
    if (k == 2 || l < 0 || l > 2) continue;
    if (!blocked(l, z - 1, z + look, pl.y)) {
      best = l;
      break;
    }
  }
  if (best < 0) {
    /* two lanes over? */
    int l = pl.lane == 0 ? 2 : pl.lane == 2 ? 0 : -1;
    if (l >= 0 && !blocked(1, z - 1, z + 3, pl.y) && !blocked(l, z - 1, z + look, pl.y)) best = pl.lane == 0 ? 1 : 1;
  }
  wait = 6;
  if (best < 0) {
    if (d < pl.speed * 0.3f && pl.state != ST_AIR) return K_UP;
    return 0;
  }
  return best < pl.lane ? K_LEFT : K_RIGHT;
}

int main(int argc, char **argv) {
  int frames = 600, draw_every = 1, bot_from = -1;
  const char *out = "build/shots", *shots = "", *saves = "build/host-saves";
  long coins = -1, keys_owned = -1;
  bool stats = false, god = false;
  const char *raw = NULL, *when = NULL;   /* when: a screenshot the first time the runner is on a roof, in a tunnel */
  int when_n = 0;
  int raw_from = 0, raw_to = -1;   /* god: nothing hurts (long runs for tests) */
  for (int i = 1; i < argc; i++) {
    const char *a = argv[i], *v = i + 1 < argc ? argv[i + 1] : "";
    if (!strcmp(a, "--frames")) frames = atoi(v), i++;
    else if (!strcmp(a, "--keys")) parse_keys(v), i++;
    else if (!strcmp(a, "--shots")) shots = v, i++;
    else if (!strcmp(a, "--out")) out = v, i++;
    else if (!strcmp(a, "--saves")) saves = v, i++;
    else if (!strcmp(a, "--seed")) host_seed = (uint32_t)strtoul(v, NULL, 0), i++;
    else if (!strcmp(a, "--bot")) bot_from = atoi(v), i++;
    else if (!strcmp(a, "--coins")) coins = atol(v), i++;
    else if (!strcmp(a, "--keys-owned")) keys_owned = atol(v), i++;
    else if (!strcmp(a, "--draw-every")) draw_every = atoi(v), i++;
    else if (!strcmp(a, "--stats")) stats = true;
    else if (!strcmp(a, "--god")) god = true;
    else if (!strcmp(a, "--when")) when = v, i++;
    else if (!strcmp(a, "--raw")) {
      static char dir[256];
      snprintf(dir, sizeof dir, "%s", v);
      char *c = strrchr(dir, ':');
      if (c) *c = 0, sscanf(c + 1, "%d-%d", &raw_from, &raw_to);
      raw = dir, i++;
    }
    else if (!strcmp(a, "--power") && npower < 8) {
      /* FRAME:KIND, KIND a power-up (0 jetpack, 1 sneakers, 2 magnet, 3 2x) */
      power_at[npower] = atoi(v), power_kind[npower] = atoi(strchr(v, ':') ? strchr(v, ':') + 1 : "0"), npower++, i++;
    }
    else {
      fprintf(stderr, "play: unknown option %s\n", a);
      return 2;
    }
  }
  mkdir("build", 0755);
  mkdir(out, 0755);
  mkdir(saves, 0755);
  host_save_dir(saves);
  static uint16_t band[SCREEN_W * BAND_H];
  r_set_band_memory(band);
  app_init();
  if (coins >= 0) pg.coins = (uint32_t)coins;
  if (keys_owned >= 0) pg.keys = (uint32_t)keys_owned;
  bool all = !strcmp(shots, "all");
  uint32_t prev = 0;
  int max_polys = 0, f = 0;
  extern uint32_t perf_polys;
  for (f = 0; f < frames && app_running; f++) {
    uint32_t k = 0;
    for (int i = 0; i < nholds; i++)
      if (f >= holds[i].a && f < holds[i].b) k |= holds[i].k;
    if (bot_from >= 0 && f >= bot_from) k |= bot();
    host_keys = k;
    for (int i = 0; i < npower; i++)
      if (f == power_at[i]) {
        pl.power[power_kind[i]] = power_time(power_kind[i]);
        if (power_kind[i] == PW_JETPACK) pl.fly = 8, world_sky_coins(pl.z + 12, pl.z + 12 + pl.speed * 9);
      }
    if (god && pl.safe < 0.1f) pl.safe = 0.1f;
    bool was_dead = pl.dead;
    app_step(k, k & ~prev);
    prev = k;
    if (stats && pl.dead && !was_dead) {
      /* what the runner ran into */
      printf("dead at frame %d: z %.1f x %.2f y %.2f lane %d from %d state %d roll %.2f chase %.2f\n", f, pl.z, pl.x, pl.y,
             pl.lane, pl.from_lane, pl.state, pl.roll, pl.chase);
      for (int i = 0; i < nobs; i++)
        if (obs[i].z1 > pl.z - 3 && obs[i].z0 < pl.z + 6)
          printf("  obstacle type %d lane %d z %.1f..%.1f v %.1f\n", obs[i].type, obs[i].lane, obs[i].z0, obs[i].z1, obs[i].v);
    }
    host_time += (f % 3 == 2) ? 17 : 16;
    char name[64];
    snprintf(name, sizeof name, ",%d,", f);
    char list[8192];
    snprintf(list, sizeof list, ",%s,", shots);
    bool shot = all || strstr(list, name);
    if (when && !pl.dead) {
      bool now = (!strcmp(when, "roof") && pl.on_roof) || (!strcmp(when, "tunnel") && world_tunnel(pl.z - 2) && world_tunnel(pl.z + 30));
      when_n = now ? when_n + 1 : 0;
      if (when_n == 25) shot = true;
    }
    bool keep = raw && f >= raw_from && f <= raw_to && f % 4 == 0;   /* (15 frames a second) */
    if (shot || keep || f % draw_every == 0) {
      app_draw();
      if ((int)perf_polys > max_polys) max_polys = (int)perf_polys;
    }
    if (keep) {
      char p[512];
      mkdir(raw, 0755);
      snprintf(p, sizeof p, "%s/%06d_%06d.raw", raw, f, (f - raw_from) * 50 / 3);
      FILE *o = fopen(p, "wb");
      if (o) fwrite(host_fb, 2, SCREEN_W * SCREEN_H, o), fclose(o);
    }
    if (shot) {
      char p[512];
      snprintf(p, sizeof p, "%s/shot_%05d.ppm", out, f);
      host_shot(p);
    }
  }
  if (stats)
    printf("frames %d running %d dead %d dist %.0f score %u coins %u multiplier %d polys %d total_coins %u "
           "best %u runs %u mset %u jumps %d rolls %d stumbles %d\n",
           f, app_running, pl.dead, run.dist, run.score, run.coins, run.multiplier, max_polys, pg.coins, pg.best,
           pg.runs, pg.mset, run.jumps, run.rolls, run.stumbles);
  return 0;
}
