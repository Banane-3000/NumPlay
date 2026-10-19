/* Plays Sonic the Hedgehog on a computer, with scripted keys, and saves frames.
 *
 *   play [--frames N] [--keys "10-40:right,50:a,..."] [--shots 30,60|all|every:N] [--out DIR]
 *        [--saves DIR] [--level Z.A] [--at X,Y] [--lives N] [--trace] [--dump N] [--god]
 *
 * Keys: up down left right a b c start home. "a-b:key" holds the key from frame a to b, "a:key" for one
 * frame. --level starts the act straight away (zone 0-6, act 0-3), past the SEGA and title screens, --at
 * starts it at that place.
 * Screenshots are DIR/shot_<frame>.ppm; --trace prints Sonic's position and state every frame, --dump N
 * the objects at frame N; --god keeps Sonic invincible (pits still kill). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include "../src/game.h"
#include "../src/plat.h"
#include "../src/s1.h"

extern uint32_t host_keys, host_time;
void host_shot(const char *path);
void host_save_dir(const char *d);
int game_main(void);

typedef struct {
  int a, b;
  uint32_t k;
} Hold;
static Hold holds[4096];
static int nholds;
static int frames = 600, frame, every, trace, dump = -1, god;
static char out_dir[256] = "build/shots";
static bool shot_all;
static int shots[1024], nshots;
int host_level = -1, host_act, host_lives = -1, host_at_x, host_at_y;

static uint32_t key_bit(const char *k) {
  static const struct {
    const char *n;
    uint32_t b;
  } t[] = {{"up", K_UP}, {"down", K_DOWN}, {"left", K_LEFT}, {"right", K_RIGHT}, {"a", K_A}, {"b", K_B},
           {"c", K_C}, {"start", K_START}, {"home", K_HOME}};
  for (unsigned i = 0; i < sizeof t / sizeof t[0]; i++)
    if (!strcmp(k, t[i].n)) return t[i].b;
  fprintf(stderr, "unknown key %s\n", k);
  exit(2);
}

static void parse_keys(char *s) {
  for (char *tok = strtok(s, ","); tok; tok = strtok(NULL, ",")) {
    char *c = strchr(tok, ':');
    if (!c) continue;
    *c = 0;
    int a = atoi(tok), b = a;
    char *d = strchr(tok, '-');
    if (d) b = atoi(d + 1);
    holds[nholds].a = a, holds[nholds].b = b, holds[nholds].k = key_bit(c + 1);
    nholds++;
  }
}

/* called by WaitForVBlank each frame, after the picture */
void host_frame(void) {
  frame++;
  bool take = shot_all || (every && frame % every == 0);
  for (int i = 0; i < nshots; i++) take |= shots[i] == frame;
  if (take) {
    char p[512];
    snprintf(p, sizeof p, "%s/shot_%05d.ppm", out_dir, frame);
    host_shot(p);
  }
  if (trace)
    printf("f%d mode %02x zone %d.%d sonic %d,%d r%d st%02x an%02x v%d,%d in%d rings %d lives %d cam %d,%d water %d/%d\n", frame,
           V.gamemode, V.zone, V.act, player->x.w.i, player->y.w.i, player->routine, player->status, player->angle,
           player->vx, player->vy, player->inertia, V.rings, V.lives, V.cam[0].x.w.i, V.cam[0].y.w.i, V.waterpos1,
           V.hblank_line);
  if (frame == dump)
    for (int i = 0; i < NOBJ; i++)
      if (objs[i].id)
        printf("obj %d id %02x r%d.%d x %d y %d render %02x col %02x frame %d sub %02x\n", i, objs[i].id,
               objs[i].routine, objs[i].routine2, objs[i].x.w.i, objs[i].y.w.i, objs[i].render, objs[i].coltype,
               objs[i].frame, objs[i].s.b[0]);
  if (god && V.gamemode == id_Level) V.invinc = 1, player->s.w[(0x32 - 0x28) / 2] = 100;
  host_keys = 0;
  for (int i = 0; i < nholds; i++)
    if (frame + 1 >= holds[i].a && frame + 1 <= holds[i].b) host_keys |= holds[i].k;
  if (host_keys) host_keys |= K_ANY;
  if (frame >= frames) host_keys |= K_HOME;
}

int main(int argc, char **argv) {
  char saves[256] = "build/host-saves";
  for (int i = 1; i < argc; i++) {
    if (!strcmp(argv[i], "--frames") && i + 1 < argc) frames = atoi(argv[++i]);
    else if (!strcmp(argv[i], "--keys") && i + 1 < argc) parse_keys(argv[++i]);
    else if (!strcmp(argv[i], "--out") && i + 1 < argc) snprintf(out_dir, sizeof out_dir, "%s", argv[++i]);
    else if (!strcmp(argv[i], "--saves") && i + 1 < argc) snprintf(saves, sizeof saves, "%s", argv[++i]);
    else if (!strcmp(argv[i], "--trace")) trace = 1;
    else if (!strcmp(argv[i], "--god")) god = 1;
    else if (!strcmp(argv[i], "--dump") && i + 1 < argc) dump = atoi(argv[++i]);
    else if (!strcmp(argv[i], "--lives") && i + 1 < argc) host_lives = atoi(argv[++i]);
    else if (!strcmp(argv[i], "--at") && i + 1 < argc) {
      char *s = argv[++i];
      host_at_x = atoi(s);
      host_at_y = strchr(s, ',') ? atoi(strchr(s, ',') + 1) : 0;
    }
    else if (!strcmp(argv[i], "--level") && i + 1 < argc) {
      char *s = argv[++i];
      host_level = atoi(s);
      char *d = strchr(s, '.');
      host_act = d ? atoi(d + 1) : 0;
    } else if (!strcmp(argv[i], "--shots") && i + 1 < argc) {
      char *s = argv[++i];
      if (!strcmp(s, "all")) shot_all = true;
      else if (!strncmp(s, "every:", 6)) every = atoi(s + 6);
      else
        for (char *t = strtok(s, ","); t && nshots < 1024; t = strtok(NULL, ",")) shots[nshots++] = atoi(t);
    }
  }
  mkdir("build", 0755);
  mkdir(out_dir, 0755);
  mkdir(saves, 0755);
  host_save_dir(saves);
  int r = game_main();
  printf("frames %d, mode %02x, zone %d.%d, rings %d, lives %d, score %u\n", frame, V.gamemode, V.zone, V.act, V.rings,
         V.lives, (unsigned)V.score * 10);
  return r;
}
