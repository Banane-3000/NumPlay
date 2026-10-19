/* Plays Five Nights at Freddy's on a computer, with scripted keys (or a player of its own), and saves frames.
 *
 *   play [--frames N] [--keys "10-40:left,50:ok,..."] [--shots 30,60|all|every:N] [--out DIR] [--saves DIR]
 *        [--night N] [--ai F,B,C,X] [--power P] [--hour H] [--seed S] [--bot] [--resume]
 *
 * Keys: left right up down ok back home del 0 to 9. "a-b:key" holds the key from frame (step) a to b, "a:key" for
 * one step. --night starts that night (its card first); --ai, --power and --hour then set its AI levels, power
 * and hour. --bot plays the night itself (it knows where everyone is): doors when someone is at them, a look
 * at Pirate Cove now and then. Frames are steps (60 a second); the screen is drawn every second step.
 * Screenshots are DIR/shot_<frame>.ppm. At the end it prints the state: "END scene ... night ... hour ...". */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include "../src/game.h"
#include "../src/gfx.h"
#include "../src/res.h"

extern uint32_t host_keys, host_time;
extern uint16_t host_fb[SCREEN_W * SCREEN_H];
void host_shot(const char *path);
void host_save_dir(const char *d);
extern const uint8_t fnaf_data[];
const uint8_t *fnaf_bin;

typedef struct { int a, b; uint32_t k; } Hold;
static Hold holds[4096];
static int nholds;

static uint32_t key_of(const char *s) {
  static const struct { const char *n; uint32_t k; } names[] = {
    {"left", K_LEFT}, {"right", K_RIGHT}, {"up", K_UP}, {"down", K_DOWN}, {"ok", K_OK}, {"back", K_BACK},
    {"home", K_HOME}, {"del", K_DEL}};
  for (unsigned i = 0; i < sizeof names / sizeof names[0]; i++)
    if (!strcmp(s, names[i].n)) return names[i].k;
  if (s[0] >= '0' && s[0] <= '9' && !s[1]) return K_DIGIT(s[0] - '0');
  fprintf(stderr, "unknown key %s\n", s);
  exit(2);
}

static void parse_keys(char *s) {
  for (char *t = strtok(s, ","); t; t = strtok(NULL, ",")) {
    char *colon = strchr(t, ':');
    if (!colon) continue;
    *colon = 0;
    int a = atoi(t), b = a;
    char *dash = strchr(t, '-');
    if (dash) b = atoi(dash + 1);
    holds[nholds++] = (Hold){a, b, key_of(colon + 1)};
  }
}

/* ------------------------------------------------------------------ a player */
static uint32_t bot(int f) {
  Night *n = &g.n;
  if (g.scene != SC_NIGHT || g.paused) return 0;
  uint32_t k = 0;
  static int next_look, look_until;
  bool up = n->flip >= 1;
  bool bonnie = n->pos[BONNIE] == AT_DOOR, chica = n->pos[CHICA] == AT_DOOR;
  /* the left door for Bonnie at it or Foxy gone from his cove; the right one for Chica, or for Freddy at
     4B while the monitor is up (he only comes in then) */
  bool want_l = bonnie || n->foxy == 3;
  bool want_r = chica || (n->pos[FREDDY] == CAM_4B && (n->monitor || f >= next_look - 40));
  if (n->monitor) {   /* on the cameras: look at Pirate Cove, then come down */
    if (up && n->cam != CAM_1C && !(f & 1)) return K_DIGIT(1);
    if (up && f >= look_until && !(f & 1)) return K_OK;
    return 0;
  }
  if (n->flip > 0 || (f & 1)) return 0;
  if (n->door[LEFT] != want_l) return K_DIGIT(7);
  if (n->door[RIGHT] != want_r) return K_DIGIT(9);
  if (f >= next_look && !n->dark) {   /* a look at Pirate Cove: more often once Foxy peeks out */
    next_look = f + (n->foxy >= 1 ? 200 : 360) + (int)(rnd() % 200);
    look_until = f + 50;
    return K_OK;
  }
  return k;
}

int main(int argc, char **argv) {
  int frames = 600, night = 0, power = -1, hour = -1, ai[4] = {-1, -1, -1, -1}, gold = 0, stars = -1;
  int pos[3] = {-1, -1, -1};
  bool use_bot = false, all = false;
  int every = 0;
  const char *out = "build/shots", *saves = "build/host-saves";
  static int shots[4096];
  int nshots = 0;
  for (int i = 1; i < argc; i++) {
    const char *a = argv[i];
    if (!strcmp(a, "--frames")) frames = atoi(argv[++i]);
    else if (!strcmp(a, "--keys")) parse_keys(argv[++i]);
    else if (!strcmp(a, "--shots")) {
      char *s = argv[++i];
      if (!strcmp(s, "all")) all = true;
      else if (!strncmp(s, "every:", 6)) every = atoi(s + 6);
      else
        for (char *t = strtok(s, ","); t && nshots < 4096; t = strtok(NULL, ",")) shots[nshots++] = atoi(t);
    } else if (!strcmp(a, "--out")) out = argv[++i];
    else if (!strcmp(a, "--saves")) saves = argv[++i];
    else if (!strcmp(a, "--night")) night = atoi(argv[++i]);
    else if (!strcmp(a, "--power")) power = atoi(argv[++i]);
    else if (!strcmp(a, "--hour")) hour = atoi(argv[++i]);
    else if (!strcmp(a, "--ai")) sscanf(argv[++i], "%d,%d,%d,%d", &ai[0], &ai[1], &ai[2], &ai[3]);
    else if (!strcmp(a, "--seed")) {
      unsigned s = (unsigned)atoi(argv[++i]);
      for (unsigned k = 0; k < s % 1000; k++) rnd();
    } else if (!strcmp(a, "--bot")) use_bot = true;
    else if (!strcmp(a, "--gold")) gold = 1;
    else if (!strcmp(a, "--pos")) sscanf(argv[++i], "%d,%d,%d", &pos[0], &pos[1], &pos[2]);
    else if (!strcmp(a, "--stars")) stars = atoi(argv[++i]);
    else {
      fprintf(stderr, "unknown argument %s\n", a);
      return 2;
    }
  }
  mkdir(out, 0755);
  mkdir(saves, 0755);
  host_save_dir(saves);
  static uint16_t band[SCREEN_W * BAND_H];
  gfx_set_band_memory(band);
  fnaf_bin = fnaf_data;
  host_time = 1000;
  game_init();
  if (stars >= 0) g.stars = (uint8_t)stars;
  if (night) {
    if (night == 7)
      for (int k = 0; k < 4; k++) g.custom[k] = (int8_t)(ai[k] >= 0 ? ai[k] : 0);
    start_night(night);
  }
  bool set = false;
  int f;
  for (f = 0; f < frames && g.running; f++) {
    if (!set && g.scene == SC_NIGHT) {   /* the night's levels, power and hour, as asked */
      set = true;
      for (int k = 0; k < 4; k++)
        if (ai[k] >= 0) g.n.ai[k] = (int8_t)ai[k];
      if (power >= 0) g.n.power = (int16_t)power;
      if (hour >= 0) g.n.hour = (uint8_t)hour, g.n.t = hour * 89.0f + 1;
      if (gold) g.n.gold = 1;
      for (int k = 0; k < 3; k++)
        if (pos[k] >= 0) g.n.pos[k] = (uint8_t)pos[k];
    }
    uint32_t k = 0;
    for (int i = 0; i < nholds; i++)
      if (f >= holds[i].a && f <= holds[i].b) k |= holds[i].k;
    if (use_bot && !k) k = bot(f);
    host_keys = k;
    int was = g.scene;
    game_step(k);
    if (g.scene != was && (g.scene == SC_SIX || g.scene == SC_SCARE || g.scene == SC_GOLDEN))
      printf("EVENT %s night %d hour %d power %d t %.1f who %d foxy_pounds %d\n",
             g.scene == SC_SIX ? "six" : g.scene == SC_SCARE ? "scare" : "golden", g.n.night, g.n.hour, g.n.power,
             g.n.t, g.scare, g.n.foxy_pounds);
    host_time += (f % 3 == 2) ? 17 : 16;
    bool shot = all || (every && f % every == 0);
    for (int i = 0; i < nshots; i++) shot |= shots[i] == f;
    if (g.running && (shot || !(f & 1))) {
      game_draw();
      if (shot) {
        char p[512];
        snprintf(p, sizeof p, "%s/shot_%05d.ppm", out, f);
        host_shot(p);
      }
    }
  }
  printf("END scene %d night %d hour %d power %d t %.1f frames %d running %d paused %d reached %d stars %d "
         "pos %d,%d,%d foxy %d gold %d dark %d muted %d ai %d,%d,%d,%d doors %d,%d cam %d keys_seen %d pounds %d\n",
         g.scene, g.n.night, g.n.hour, g.n.power, g.n.t, f, g.running, g.paused, g.reached, g.stars, g.n.pos[0],
         g.n.pos[1], g.n.pos[2], g.n.foxy, g.n.gold, g.n.dark, g.n.call_muted, g.n.ai[0], g.n.ai[1], g.n.ai[2],
         g.n.ai[3], g.n.door[0], g.n.door[1], g.n.cam, g.keys_seen, g.n.foxy_pounds);
  return 0;
}
