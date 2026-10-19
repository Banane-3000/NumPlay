/* Plays Plants vs. Zombies on a computer, and saves frames.
 *
 *   play [--frames N] [--keys "10-40:right,50:ok,..."] [--shots 30,60|every:N] [--out DIR] [--saves DIR]
 *        [--progress L] [--money M]
 *   play --sim L [--frames N] [--shots ...] [--out DIR] [--quiet]
 *
 * Keys: left right up down ok back shovel home almanac 0-9. "a-b:key" holds the key from tick a to b, "a:key" for
 * one tick (ticks are hundredths of a second; a frame is drawn every 3).
 * --progress L sets the adventure at level L with what it has found by then (then the menus play as scripted).
 * --sim L plays level L straight away, a robot planting (with all the sun it wants, so that every level's zombies
 * come and go): it prints how the level went and fails if it isn't won. Screenshots are DIR/shot_<tick>.ppm. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include "../src/draw.h"
#include "../src/game.h"
#include "../src/gfx.h"
#include "../src/res.h"
#include "../src/save.h"
#include "../src/screens.h"

extern uint32_t host_keys, host_time;
extern uint16_t host_fb[SCREEN_W * SCREEN_H];
void host_shot(const char *path);
void host_save_dir(const char *d);
extern const uint8_t pvz_data[];
extern const uint8_t *pvz_bin;
const uint8_t *pvz_bin;

typedef struct { int a, b; uint32_t k; } Hold;
static Hold holds[4096];
static int nholds;

static uint32_t key_named(const char *s) {
  static const struct { const char *n; uint32_t k; } t[] = {
    {"left", K_LEFT}, {"right", K_RIGHT}, {"up", K_UP}, {"down", K_DOWN}, {"ok", K_OK}, {"back", K_BACK},
    {"shovel", K_SHOVEL}, {"home", K_HOME}, {"almanac", K_ALMANAC},
  };
  for (unsigned i = 0; i < sizeof t / sizeof t[0]; i++)
    if (!strcmp(s, t[i].n)) return t[i].k;
  if (s[0] >= '0' && s[0] <= '9' && !s[1]) return K_DIGIT(s[0] == '0' ? 10 : s[0] - '0');
  fprintf(stderr, "unknown key %s\n", s);
  exit(2);
}

static void parse_keys(const char *spec) {
  char buf[65536];
  snprintf(buf, sizeof buf, "%s", spec);
  for (char *tok = strtok(buf, ","); tok; tok = strtok(NULL, ",")) {
    char *colon = strchr(tok, ':');
    if (!colon) continue;
    *colon = 0;
    int a, b;
    if (sscanf(tok, "%d-%d", &a, &b) != 2) b = a = atoi(tok);
    holds[nholds].a = a, holds[nholds].b = b, holds[nholds].k = key_named(colon + 1);
    if (nholds < 4095) nholds++;
  }
}

static int shots[1024], nshots_at, every;
static void parse_shots(const char *s) {
  if (!strncmp(s, "every:", 6)) {
    every = atoi(s + 6);
    return;
  }
  char buf[8192];
  snprintf(buf, sizeof buf, "%s", s);
  for (char *t = strtok(buf, ","); t && nshots_at < 1024; t = strtok(NULL, ",")) shots[nshots_at++] = atoi(t);
}
static bool shot_now(int tick) {
  if (every && tick % every == 0) return true;
  for (int i = 0; i < nshots_at; i++)
    if (shots[i] == tick) return true;
  return false;
}

/* ---------------------------------------------------------------- the robot */

static const int prefer[] = {P_SUNFLOWER, P_REPEATER, P_THREEPEATER, P_SNOWPEA, P_PEASHOOTER, P_MELONPULT, P_KERNELPULT,
                             P_CABBAGEPULT, P_CHERRYBOMB, P_CACTUS, P_SPLITPEA, P_TALLNUT, P_WALLNUT, P_LILYPAD,
                             P_FLOWERPOT, P_PLANTERN, P_PUMPKIN, P_STARFRUIT, P_FUMESHROOM, P_PUFFSHROOM, P_TORCHWOOD,
                             P_SQUASH, P_SPIKEWEED, P_GRAVEBUSTER, P_UMBRELLA, P_MAGNETSHROOM, P_COFFEEBEAN, P_JALAPENO};

static bool level_has(int kind) {
  for (int w = 0; w < B.nwaves && w < 50; w++)
    for (int i = 0; i < 24 && B.waves[w][i] != 0xFF; i++)
      if (B.waves[w][i] == kind) return true;
  return false;
}

static void robot_seeds(int level) {
  if (B.nseeds || level_defs[level - 1].nseeds) return;
  uint8_t s[10];
  int n = 0;
  bool has_repeater = plant_unlocked(P_REPEATER), pult = plant_unlocked(P_CABBAGEPULT);
  for (unsigned i = 0; i < sizeof prefer / sizeof prefer[0] && n < 10; i++) {
    int k = prefer[i];
    if (!plant_unlocked(k)) continue;
    if (k == P_PEASHOOTER && has_repeater) continue;
    if ((k == P_REPEATER || k == P_THREEPEATER || k == P_SNOWPEA || k == P_PEASHOOTER || k == P_SPLITPEA ||
         k == P_TORCHWOOD) && B.env == ENV_ROOF)
      continue;
    if (k == P_CACTUS && !level_has(Z_BALLOON)) continue;
    if (k == P_SPLITPEA && !level_has(Z_DIGGER)) continue;
    if (k == P_UMBRELLA && !level_has(Z_CATAPULT) && !level_has(Z_BUNGEE)) continue;
    if ((k == P_FUMESHROOM || k == P_PUFFSHROOM) && (pult || !is_night())) continue;
    if ((k == P_LILYPAD) && !is_pool()) continue;
    if (k == P_FLOWERPOT && B.env != ENV_ROOF) continue;
    if (k == P_GRAVEBUSTER && !B.ngraves) continue;
    if ((k == P_SPIKEWEED || k == P_GRAVEBUSTER) && B.env == ENV_ROOF) continue;
    if (k == P_PLANTERN && B.env != ENV_FOG) continue;
    if (k == P_COFFEEBEAN && is_night()) continue;
    s[n++] = (uint8_t)k;
  }
  board_set_seeds(s, n);
}

/* what the robot would like in a cell, in order (the first it can plant) */
static int want(int col, int row, int *kinds) {
  int n = 0;
  bool shooter_ok = B.env != ENV_ROOF;
  if (water_row(row)) kinds[n++] = P_LILYPAD;
  if (B.env == ENV_ROOF) kinds[n++] = P_FLOWERPOT;
  if (col == 0) kinds[n++] = P_SUNFLOWER, kinds[n++] = P_SUNSHROOM;
  if (col == 1) kinds[n++] = P_SPLITPEA, kinds[n++] = P_CACTUS;
  if (col >= 1 && col <= 4) {
    if (col == 4) kinds[n++] = P_CACTUS;
    if (shooter_ok) kinds[n++] = P_REPEATER, kinds[n++] = P_THREEPEATER, kinds[n++] = P_SNOWPEA, kinds[n++] = P_PEASHOOTER;
    kinds[n++] = P_MELONPULT, kinds[n++] = P_KERNELPULT, kinds[n++] = P_CABBAGEPULT;
    kinds[n++] = P_FUMESHROOM, kinds[n++] = P_STARFRUIT, kinds[n++] = P_CACTUS, kinds[n++] = P_PUFFSHROOM;
    kinds[n++] = P_SCAREDYSHROOM, kinds[n++] = P_SEASHROOM;
  }
  if (col == 5) kinds[n++] = P_TORCHWOOD, kinds[n++] = P_PLANTERN, kinds[n++] = P_UMBRELLA;
  if (col == 6) kinds[n++] = P_TALLNUT, kinds[n++] = P_WALLNUT, kinds[n++] = P_PUMPKIN;
  if (col == 7) kinds[n++] = P_SPIKEWEED, kinds[n++] = P_GRAVEBUSTER;
  kinds[n++] = P_COFFEEBEAN, kinds[n++] = P_PUMPKIN;
  return n;
}

/* a seed from the conveyor belt, where it does the most */
static bool robot_packet(int i) {
  int kind = B.packets[i].kind;
  if (B.mode == MODE_BOWLING) {
    for (int r = 0; r < B.nrows; r++) {
      int rr = (r + (int)(B.ticks / 25)) % B.nrows;
      if (!plant_issue(kind, 2, rr) && board_plant(i, 2, rr)) return true;
    }
    return false;
  }
  /* Dr. Zomboss: ice for his fire ball (and his head), fire for his ice ball */
  if (B.has_boss && (kind == P_ICESHROOM || kind == P_JALAPENO)) {
    bool fire = boss.ball.on && boss.ball.kind == 0, ice = boss.ball.on && boss.ball.kind == 1;
    if (kind == P_ICESHROOM && (fire || boss_vulnerable()))
      for (int r = 0; r < B.nrows; r++)
        for (int c = 0; c < 6; c++)
          if (!plant_issue(kind, c, r) && board_plant(i, c, r)) return true;
    if (kind == P_JALAPENO && ice)
      for (int c = 0; c < 6; c++)
        if (!plant_issue(kind, c, boss.ball.row) && board_plant(i, c, boss.ball.row)) return true;
    /* (or on the zombies, when they are close) */
    for (int k = 0; k < B.nzombies; k++) {
      const Zombie *z = &B.zombies[k];
      if (z->health > 0 && z->x < (kind == P_JALAPENO ? 400 : 250) && !plant_issue(kind, col_at(z->x), z->row))
        return board_plant(i, col_at(z->x), z->row);
    }
    return false;
  }
  /* the instant ones: on the zombie nearest the house */
  if (kind == P_CHERRYBOMB || kind == P_JALAPENO || kind == P_SQUASH || kind == P_TANGLEKELP || kind == P_ICESHROOM ||
      kind == P_DOOMSHROOM) {
    const Zombie *near = NULL;
    for (int k = 0; k < B.nzombies; k++) {
      const Zombie *z = &B.zombies[k];
      if (z->health > 0 && !(z->flags & ZF_HYPNO) && z->x < 700 && (!near || z->x < near->x) &&
          !plant_issue(kind, col_at(z->x), z->row))
        near = z;
    }
    return near && board_plant(i, col_at(near->x), near->row);
  }
  /* the others where they'd go, the rows in turn */
  for (int r = 0; r < B.nrows; r++) {
    int rr = (r + (int)(B.ticks / 25)) % B.nrows;
    for (int c = 0; c < COLS - 2; c++) {
      int kinds[24], n = want(c, rr, kinds);
      for (int k = 0; k < n; k++)
        if (kinds[k] == kind && !plant_issue(kind, c, rr) && board_plant(i, c, rr)) return true;
    }
  }
  return false;
}

static void robot(void) {
  if (B.phase != PH_PLAY) return;
  B.sun = 5000;
  /* sun and coins: picked up */
  board_collect(400, 300, 5000);
  if (B.mode == MODE_WHACK) {
    for (int i = 0; i < B.nzombies; i++)
      if (B.zombies[i].x < 700 && B.zombies[i].rise == 0 && (B.ticks + i) % 7 == 0)
        board_whack(B.zombies[i].x, row_y(B.zombies[i].row, B.zombies[i].x) - 40);
    for (int g = 0; g < B.ngraves; g++)
      for (int s = 0; s < B.nseeds; s++)
        if (B.seeds[s] == P_GRAVEBUSTER && !B.cooldown[s]) board_plant(s, B.graves[g].col, B.graves[g].row);
    return;
  }
  if (B.mode == MODE_VASE) {
    /* the seeds found: where a zombie is, or in the rows in turn */
    if (B.ndrops) {
      int kind = B.drops[0].kind, row = -1;
      for (int i = 0; i < B.nzombies; i++)
        if (B.zombies[i].health > 0 && (row < 0 || B.zombies[i].x < B.zombies[row].x)) row = i;
      row = row >= 0 ? B.zombies[row].row : (int)(B.ticks / 7) % B.nrows;
      if (kind == P_SQUASH || kind == P_CHERRYBOMB) {
        for (int i = 0; i < B.nzombies; i++)
          if (B.zombies[i].health > 0 && !plant_issue(kind, col_at(B.zombies[i].x), B.zombies[i].row)) {
            board_plant(0, col_at(B.zombies[i].x), B.zombies[i].row);
            break;
          }
      } else
        for (int c = 0; c < 5 && B.ndrops; c++)
          if (!plant_issue(kind, c, row) && board_plant(0, c, row)) break;
      if (B.ndrops && B.drops[0].left < 50) B.drops[0] = B.drops[--B.ndrops];
    } else if (B.nvases && B.ticks % 150 == 0) {
      /* a green one first, then from the far side, when the zombies out are few */
      int alive = 0, pick = 0;
      for (int i = 0; i < B.nzombies; i++) alive += B.zombies[i].health > 0;
      for (int i = 1; i < B.nvases; i++)
        if (B.vases[i].green > B.vases[pick].green ||
            (B.vases[i].green == B.vases[pick].green && B.vases[i].col > B.vases[pick].col))
          pick = i;
      if (alive < 2 || B.vases[pick].green) board_break_vase(B.vases[pick].col, B.vases[pick].row);
    }
    return;
  }
  if (has_conveyor()) {
    if (!B.npackets || B.ticks % 25) return;
    for (int i = 0; i < B.npackets; i++)
      if (robot_packet(i)) return;
    /* nowhere, the belt full: the first one dropped */
    if (B.npackets >= 8) {
      memmove(&B.packets[0], &B.packets[1], (size_t)(B.npackets - 1) * sizeof B.packets[0]);
      B.npackets--;
    }
    return;
  }
  /* plants: every cell, what it wants first */
  for (int r = 0; r < B.nrows; r++) {
    if (!(B.rows >> r & 1)) continue;
    for (int c = 0; c < 8; c++) {
      int kinds[24], n = want(c, r, kinds);
      for (int k = 0; k < n; k++)
        for (int s = 0; s < B.nseeds; s++)
          if (B.seeds[s] == kinds[k] && !B.cooldown[s] && !plant_issue(kinds[k], c, r)) {
            board_plant(s, c, r);
            goto next;
          }
    next:;
    }
  }
  /* a cherry bomb where the zombies are thickest */
  for (int s = 0; s < B.nseeds; s++) {
    if ((B.seeds[s] != P_CHERRYBOMB && B.seeds[s] != P_JALAPENO) || B.cooldown[s]) continue;
    for (int i = 0; i < B.nzombies; i++) {
      const Zombie *z = &B.zombies[i];
      if (z->x < 500 && z->x > 100 && !(z->flags & ZF_HYPNO)) {
        int c = col_at(z->x);
        if (!plant_issue(B.seeds[s], c, z->row)) board_plant(s, c, z->row);
        break;
      }
    }
  }
}

static int sim(int level, int frames, const char *out, bool quiet) {
  progress_load();
  progress.level = (uint8_t)level, progress.slots = 10, progress.items = 0x7F;
  board_start(level);
  robot_seeds(level);
  B.phase = PH_PLAY;
  cam_x = 0;
  int bg = B.env == ENV_NIGHT ? BG_NIGHT : B.env == ENV_POOL ? BG_POOL : B.env == ENV_FOG ? BG_FOG
         : B.env == ENV_ROOF ? BG_ROOF : BG_LAWN;
  gfx_load_back(bg, 88);
  int t;
  int most = 0;
  for (t = 0; t < frames && B.phase == PH_PLAY; t++) {
    robot();
    board_tick();
    vis_ticks++;
    if (B.nzombies > most) most = B.nzombies;
    if (getenv("PVZ_DEBUG") && t % 500 == 0) {
      int lob = 0, other = 0;
      for (int i = 0; i < B.nshots; i++) lob += B.shots[i].kind >= S_CABBAGE, other += B.shots[i].kind < S_CABBAGE;
      printf("t %d plants %d shots %d/%d:", t, B.nplants, lob, other);
      for (int i = 0; i < B.nzombies; i++)
        printf(" [k%d r%d x%.0f h%d a%d f%x]", B.zombies[i].kind, B.zombies[i].row, B.zombies[i].x, B.zombies[i].health,
               B.zombies[i].armor, B.zombies[i].flags);
      printf("\n");
    }
    if (shot_now(t)) {
      gfx_begin();
      draw_lawn(true);
      hud.col = -10;
      hud_draw(false);
      gfx_end();
      char p[512];
      snprintf(p, sizeof p, "%s/shot_%06d.ppm", out, t);
      host_shot(p);
    }
  }
  const char *how = B.phase == PH_WON ? "won" : B.phase == PH_LOST ? "LOST" : "UNFINISHED";
  if (!quiet || B.phase != PH_WON)
    printf("level %2d-%-2d %-10s ticks %6d waves %2d/%-2d kills %4d planted %3d most zombies %d\n", level_world(level),
           level_stage(level), how, t, B.wave, B.nwaves, B.kills, B.planted, most);
  if (B.has_boss && (!quiet || B.phase != PH_WON)) printf("Dr. Zomboss: %d of %d\n", (int)boss.health, BOSS_HEALTH);
  return B.phase == PH_WON ? 0 : 1;
}

int main(int argc, char **argv) {
  int frames = 2000, simlevel = 0, prog = 0;
  long money = -1;
  bool quiet = false;
  const char *out = "build/shots", *saves = "build/host-saves";
  for (int i = 1; i < argc; i++) {
    if (!strcmp(argv[i], "--frames") && i + 1 < argc) frames = atoi(argv[++i]);
    else if (!strcmp(argv[i], "--keys") && i + 1 < argc) parse_keys(argv[++i]);
    else if (!strcmp(argv[i], "--shots") && i + 1 < argc) parse_shots(argv[++i]);
    else if (!strcmp(argv[i], "--out") && i + 1 < argc) out = argv[++i];
    else if (!strcmp(argv[i], "--saves") && i + 1 < argc) saves = argv[++i];
    else if (!strcmp(argv[i], "--sim") && i + 1 < argc) simlevel = atoi(argv[++i]);
    else if (!strcmp(argv[i], "--progress") && i + 1 < argc) prog = atoi(argv[++i]);
    else if (!strcmp(argv[i], "--money") && i + 1 < argc) money = atol(argv[++i]);
    else if (!strcmp(argv[i], "--quiet")) quiet = true;
  }
  mkdir(out, 0755);
  mkdir(saves, 0755);
  host_save_dir(saves);
  static uint16_t band[SCREEN_W * GFX_BAND];
  gfx_set_band_memory(band);
  pvz_bin = pvz_data;
  B.rng = 20251019;
  if (simlevel) {
    if (simlevel < 0) {
      int bad = 0;
      for (int l = 1; l <= N_LEVELS; l++) B.rng = (uint32_t)(l * 7919), bad += sim(l, frames, out, quiet);
      printf("%d of %d levels not won\n", bad, N_LEVELS);
      return bad ? 1 : 0;
    }
    return sim(simlevel, frames, out, quiet);
  }
  game_init();
  if (prog) {
    progress.level = (uint8_t)prog, progress.slots = 6;
    for (int l = 1; l < prog; l++) {
      int r = level_defs[l - 1].reward;
      if (r == REWARD_SHOVEL) progress.items |= ITEM_SHOVEL;
      if (r == REWARD_ALMANAC) progress.items |= ITEM_ALMANAC;
      if (r == REWARD_KEYS) progress.items |= ITEM_SHOP;
    }
  }
  if (money >= 0) progress.money = (uint32_t)money;
  for (int t = 0; t < frames && g_running; t++) {
    host_keys = 0;
    for (int i = 0; i < nholds; i++)
      if (t >= holds[i].a && t <= holds[i].b) host_keys |= holds[i].k;
    if (host_keys) host_keys |= K_ANY;
    game_frame(host_keys);
    host_time += 10;
    if (t % 3 == 0 || shot_now(t)) game_draw();
    if (shot_now(t)) {
      char p[512];
      snprintf(p, sizeof p, "%s/shot_%06d.ppm", out, t);
      host_shot(p);
    }
  }
  printf("state %d level %d phase %d sun %d zombies %d plants %d wave %d/%d\n", g_state, B.level, B.phase, (int)B.sun,
         B.nzombies, B.nplants, B.wave, B.nwaves);
  return 0;
}
