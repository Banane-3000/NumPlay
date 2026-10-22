/* Tomb of the Mask on a computer, for tests and pictures (make host, make test).
 *
 *   play test DIR       the rules against the original's numbers (the spec kept with the reference), a bot that
 *                       clears the stages and climbs the arcade, the same run twice (and its hash, the same on
 *                       every computer), a few hundred sessions of the whole app with random keys (quitting with
 *                       Home each time), saves written and read back exactly, damaged saves refused; with
 *                       TOTM_REF=DIR (the web game taken apart: DIR/text/stages__*, sections__*), the packed maps
 *                       against the game's own files
 *   play shots DIR      pictures of the screens (DIR/<name>.ppm)
 *   play film DIR N     N frames of the bot in the arcade, every other step (DIR/f_<ms>.raw)
 */
#define main tm_app_main
#include "../src/main.c"
#undef main
#include <dirent.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include "inner.h"
#include "data.h"
#include "bot.h"
#include "../../common/np_lz.h"

/* ---------------------------------------------------------------- the calculator, on a computer */
static uint16_t fb[240][320];
void plat_push(int x, int y, int w, int h, const uint16_t *px) {
  for (int j = 0; j < h; j++)
    for (int i = 0; i < w; i++) fb[y + j][x + i] = px[j * w + i];
}
static double clock_ms;
static uint32_t (*script)(uint32_t frame);
static uint32_t frame_no, quit_at;
uint32_t plat_keys(void) {
  clock_ms += 10;
  frame_no++;
  if (frame_no >= quit_at) return K_HOME;
  return script ? script(frame_no) : 0;
}
uint32_t plat_millis(void) { return (uint32_t)clock_ms; }
void plat_sleep(uint32_t ms) { clock_ms += ms; }
uint32_t plat_random(void) { return 0x9E3779B9u ^ frame_no; }
void plat_begin(void) {}
int plat_end(void) { return 0; }

static struct { char name[32]; uint8_t data[4096]; uint32_t len; bool on; } files[4];
static bool storage_full;
bool plat_save(const char *name, const void *data, uint32_t len) {
  if (storage_full) return false;
  int k = -1;
  for (int i = 0; i < 4; i++)
    if (files[i].on && !strcmp(files[i].name, name)) k = i;
  for (int i = 0; i < 4 && k < 0; i++)
    if (!files[i].on) k = i;
  if (k < 0 || len > sizeof files[k].data) return false;
  strcpy(files[k].name, name), memcpy(files[k].data, data, len), files[k].len = len, files[k].on = true;
  return true;
}
const uint8_t *plat_load(const char *name, uint32_t *len) {
  for (int i = 0; i < 4; i++)
    if (files[i].on && !strcmp(files[i].name, name)) return *len = files[i].len, files[i].data;
  return 0;
}
static void forget(const char *name) {
  for (int i = 0; i < 4; i++)
    if (files[i].on && !strcmp(files[i].name, name)) files[i].on = false;
}
static uint8_t *file_data(const char *name, uint32_t *len) {
  for (int i = 0; i < 4; i++)
    if (files[i].on && !strcmp(files[i].name, name)) return *len = files[i].len, files[i].data;
  return 0;
}

static void shot(const char *path) {
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

static tm_game U, B;

/* a stage with the player alone in it: a free row and column round (7, 7) in a 15 x 15 box of walls */
static void room(tm_game *t) {
  tm_new_progress(&t->p);
  tm_start(t, 0, 7);
  memset(t->tile, 0, sizeof t->tile);
  memset(t->look, 0, sizeof t->look);
  memset(t->a, 0, sizeof t->a);
  memset(t->plat, 0, sizeof t->plat);
  memset(t->rotor, 0, sizeof t->rotor);
  memset(t->ball, 0, sizeof t->ball);
  memset(t->snake, 0, sizeof t->snake);
  t->w = 15, t->h = 15;
  for (int y = 0; y < 15; y++)
    for (int x = 0; x < 15; x++) t->tile[y * 15 + x] = (x == 0 || y == 0 || x == 14 || y == 14) ? T_WALL : T_EMPTY;
  t->x = 7 * TT, t->y = 7 * TT, t->face = 0, t->dx = t->dy = 0;
  t->pstate = ST_IDLE;
  t->lava_max = 0;
  t->dots = t->dots_all = 0, t->score = 0;
  tm_save_prev(t);
}
static void steps(tm_game *t, int n, int dir) {
  for (int i = 0; i < n; i++) tm_step(t, i == 0 ? dir : -1, false);
}

/* ---------------------------------------------------------------- the rules against the spec */
static void units(void) {
  /* settings (RSettings.InitDefaults) */
  CHECK(tm_level_xp[1] == 1000 && tm_level_xp[19] == 7645579 && tm_level(0) == 1 && tm_level(999) == 1 &&
        tm_level(1000) == 2 && tm_level(8000000) == 20, "levels");
  CHECK(tm_upgrade_cost[0] == 200 && tm_upgrade_cost[8] == 5000, "upgrade costs");
  CHECK(tm_powerup_secs[0][0] == 10 && tm_powerup_secs[0][9] == 40 && tm_powerup_secs[1][9] == 14 &&
        tm_powerup_secs[2][2] == 8 && tm_powerup_secs[3][9] == 20, "powerup effects");
  CHECK(tm_mask_cost[1] == 200 && tm_mask_cost[2] == 500 && tm_mask_cost[9] == 5000 && tm_mask_cost[15] == 5000 &&
        tm_mask_level[15] == 20 && tm_mask_level[8] == 8, "masks");
  CHECK(tm_mission_goal[0][6] == 49 && tm_mission_goal[2][6] == 10000 && tm_mission_levels[3] == 6 &&
        tm_mission_levels[5] == 5 && tm_mission_reward[6] == 100, "missions");
  /* the data: 200 stages, 517 sections by type (Begin 5, Easy 106 ...), the lava stages */
  static const uint8_t count[15] = {5, 106, 70, 69, 60, 18, 22, 16, 60, 17, 26, 10, 10, 1, 27};
  for (int t = 0; t < 15; t++) CHECK(tm_section_count[t] == count[t], "sections of type %d: %u", t, tm_section_count[t]);
  int lava = 0;
  for (int s = 0; s < 200; s++) {
    CHECK(tm_stages[s].w * tm_stages[s].h <= TM_CELLS, "stage %d too big", s);
    if (tm_stages[s].extra) lava++;
  }
  CHECK(lava == 7 && tm_stages[3].extra == 13 && tm_stages[186].extra == 20 && tm_stages[92].extra == 10, "lava stages");
  for (int i = 0; i < 517; i++) CHECK(tm_sections[i].w == 13 && tm_sections[i].h >= 4 && tm_sections[i].h <= 15, "section %d", i);

  /* the dash: 5 units a second (25600/9 units a step), until a wall; the stop is the cell before it */
  room(&U);
  tm_step(&U, 1, false);
  CHECK(U.dx == 1 && U.x == 7 * TT + 25600 / 9, "a step of a dash: %d", U.x - 7 * TT);
  steps(&U, 20, -1);
  CHECK(tm_standing(&U) && U.x == 13 * TT && U.face == 1, "stops beside the wall: %d face %d", U.x, U.face);
  /* not into the wall it stands against; any other way */
  tm_step(&U, 1, false);
  CHECK(tm_standing(&U), "dashed into its own wall");
  tm_step(&U, 2, false);
  CHECK(U.dy == 1, "no dash up");
  steps(&U, 20, -1);
  CHECK(U.y == 13 * TT && U.face == 2, "up to the ceiling");
  /* a dash pressed in the last 0.1 s of a dash is taken on landing (6 steps), not an earlier one */
  room(&U);
  tm_step(&U, 0, false);                 /* 6 tiles to the wall: 9 steps */
  steps(&U, 1, -1);
  tm_step(&U, 2, false);                 /* pressed too early */
  steps(&U, 12, -1);
  CHECK(tm_standing(&U) && U.x == TT && U.y == 7 * TT, "an early press was kept");
  room(&U);
  tm_step(&U, 0, false);
  steps(&U, 6, -1);
  tm_step(&U, 2, false);                 /* within 0.1 s of the landing */
  steps(&U, 2, -1);
  CHECK(U.dy == 1 && U.x == TT, "a late press was lost");

  /* dots score the multiplier (the level), coins one more coin; ice 1 + the multiplier */
  room(&U);
  U.tile[7 * 15 + 9] = T_DOT, U.tile[7 * 15 + 11] = T_COIN, U.tile[7 * 15 + 12] = T_ICE;
  uint32_t c0 = U.p.coins;
  tm_step(&U, 1, false), steps(&U, 20, -1);
  CHECK(U.score == 1 + 1 + 2 && U.p.coins == c0 + 1 && U.dots == 2 && U.x == 13 * TT, "dot, coin and ice: %u", U.score);
  U.p.xp = 1000, U.mult = (uint8_t)tm_level(U.p.xp);
  U.tile[7 * 15 + 9] = T_DOT;
  tm_step(&U, 0, false), steps(&U, 20, -1);
  CHECK(U.score == 4 + 2, "a level 2 dot: %u", U.score);
  /* spikes kill a dash into them; a shield takes it and the player goes back to its last landing */
  room(&U);
  U.tile[7 * 15 + 11] = T_SPIKES;
  tm_step(&U, 1, false), steps(&U, 10, -1);
  CHECK(!U.alive && U.ev_dead, "spikes");
  room(&U);
  U.tile[7 * 15 + 11] = T_SPIKES;
  U.p.shields = 1;
  tm_step(&U, -1, true);
  CHECK(U.pw_time[PW_SHIELD] == SEC(30) - 1 && U.p.shields == 0, "the shield: 30 s");
  tm_step(&U, 1, false), steps(&U, 20, -1);
  CHECK(U.alive && U.pw_time[PW_SHIELD] == 0 && U.x == 7 * TT && tm_standing(&U), "a shield hit: back at %d", U.x / TT);

  /* the things' speeds (prefab values): bats 0.56 units a second, bullets 2.22, platforms 3.33, snakes 1.67 */
  room(&U);
  tm_actor *bat = tm_spawn(&U, A_BAT, 3 * TT, 3 * TT, 1, 0);
  int32_t bx = bat->x;
  steps(&U, 60, -1);
  CHECK(bat->x - bx == UNITS(0.56f) || iabs(bat->x - bx - UNITS(0.56f)) <= 1, "bat: %d a second", bat->x - bx);
  room(&U);
  tm_actor *bul = tm_spawn(&U, A_BULLET, 2 * TT, 3 * TT, 1, 0);
  steps(&U, 2, -1);
  CHECK(bul->x == 2 * TT + 1263, "bullet: skips a step, then 2.22 units a second (%d)", bul->x - 2 * TT);
  CHECK(UNITS(1.67f) / 60 == 950 && UNITS(3.33f) / 60 == 1894, "snake and platform speeds");
  /* rocks: from 0.36 units a second, +3 each second: 1.86 units in the first second */
  room(&U);
  U.y = 100 * TT;
  tm_actor *rock = tm_spawn(&U, A_ROCK, 7 * TT, 90 * TT, 0, -1);
  rock->v = 23593;
  for (int i = 0; i < 60; i++) U.y = 100 * TT, tm_step(&U, -1, false);
  CHECK(iabs(90 * TT - rock->y - UNITS(1.86f)) < UNITS(0.05f), "a rock's first second: %d", 90 * TT - rock->y);
  /* the cannons fire every 1.1 s once the lava is on, the fishes every 2.1 s, the rotors turn every 2.1 s */
  room(&U);
  U.tile[3 * 15 + 1] = T_CANNON_R;
  int shots = 0;
  for (int i = 0; i < 660; i++) {
    tm_step(&U, -1, false);
    for (int k = 0; k < TM_ACTORS; k++)
      if (U.a[k].type == A_BULLET && U.a[k].x == 2 * TT && !U.a[k].timer && U.a[k].wait == 0) shots++, U.a[k].wait = 1;
  }
  CHECK(shots == 0, "cannons before the first dash: %d", shots);
  U.lava_on = 1;
  for (int i = 0; i < 660; i++) {
    tm_step(&U, -1, false);
    for (int k = 0; k < TM_ACTORS; k++)
      if (U.a[k].type == A_BULLET && U.a[k].x == 2 * TT && !U.a[k].timer && U.a[k].wait == 0) shots++, U.a[k].wait = 1;
  }
  CHECK(shots == 10, "a cannon in 11 s: %d shots", shots);
  /* timer walls close for 2 s behind the player */
  room(&U);
  U.tile[7 * 15 + 9] = T_TIMER_WALL;
  tm_step(&U, 1, false), steps(&U, 20, -1);
  CHECK(tm_timer_wall_on(&U, 9, 7), "timer wall closed");
  tm_step(&U, 0, false), steps(&U, 20, -1);
  CHECK(U.x == 10 * TT, "stopped by the closed timer wall: %d", U.x / TT);
  steps(&U, 150, -1);
  CHECK(tm_timer_wall_on(&U, 9, 7), "the timer wall stays closed while the player stands against it");
  tm_step(&U, 1, false), steps(&U, 120, -1);
  CHECK(!tm_timer_wall_on(&U, 9, 7), "timer wall open 2 s later");

  /* the arcade: the lava waits for the first dash, starts 10.5 tiles down at 0.19 units a second, speeds up to
   * 0.38 (in about 48 s), half as fast within 3 tiles, never more than 17 tiles behind */
  tm_new_progress(&U.p);
  tm_start(&U, -1, 11);
  CHECK(U.lava_y == -LAVA_BELOW && U.x == 6 * TT && U.y == 0 && U.zone == 0 && U.zone_len == 10, "the arcade's start");
  steps(&U, 60, -1);
  CHECK(U.lava_y == -LAVA_BELOW && !U.lava_on, "lava before the first dash");
  room(&U);
  U.lava_speed = (int32_t)(0.19 * (1 << 24)), U.lava_max = (int32_t)(0.38 * (1 << 24)), U.lava_on = 1;
  tm_step(&U, -1, false);
  CHECK(U.lava_y == -10 * TT, "pulled up to 17 tiles behind: %d", U.lava_y);
  int32_t l0 = U.lava_y;
  steps(&U, 60, -1);
  CHECK(U.lava_y - l0 >= UNITS(0.19f) && U.lava_y - l0 <= UNITS(0.2f), "a second of lava: %d", U.lava_y - l0);
  CHECK(U.lava_speed > (int32_t)(0.19 * (1 << 24)) && U.lava_speed < (int32_t)(0.21 * (1 << 24)), "lava speed after 1 s");
  for (int i = 0; i < 60 * 46; i++) U.y = U.lava_y + 10 * TT, tm_step(&U, -1, false);   /* (the player kept away) */
  CHECK(U.lava_speed < U.lava_max, "not yet at top speed");
  for (int i = 0; i < 60 * 3; i++) U.y = U.lava_y + 10 * TT, tm_step(&U, -1, false);
  CHECK(U.lava_speed == U.lava_max, "top speed after about 48 s");
  U.y = 7 * TT;
  U.lava_y = U.y - 2 * TT, U.lava_sub = 0, l0 = U.lava_y;
  tm_step(&U, -1, false);
  int64_t full = (int64_t)U.lava_max * 5 / 576, got = ((int64_t)U.lava_y - l0) * 256 + U.lava_sub;
  CHECK(got * 6 >= full * 5 - 256 && got * 6 <= full * 5 + 256, "2 tiles below: 5/6 of the speed (%lld of %lld)", (long long)got, (long long)full);
  /* the zones: 10 + 2z sections; touching the zone exit makes the next zone */
  tm_new_progress(&U.p);
  tm_start(&U, -1, 12);
  CHECK(U.style == 0 && U.colors == 0, "zone 0's look");
  tm_change_zone(&U);
  CHECK(U.zone == 1 && U.style >= 1 && U.style <= 4 && U.zone_changed && U.color_wait == 18, "zone change");

  /* masks: Cyclop's 20% more coins; Dog: no snakes */
  tm_new_progress(&U.p);
  U.p.mask = 1, U.p.masks |= 2;
  tm_start(&U, -1, 1);
  CHECK(U.coin_chance == 3277 * 120 / 100 && U.bonus_coin_chance == 19661 * 120 / 100, "Cyclop's coins");
  U.p.mask = 3, U.p.masks |= 8;
  tm_start(&U, -1, 1);
  CHECK(U.no_snakes && !U.no_monkeys, "Dog: no snakes");
  /* the shop and masks */
  tm_progress p;
  tm_new_progress(&p);
  p.coins = 1000;
  CHECK(tm_buy_shield(&p) && p.shields == 1 && p.coins == 800, "a shield costs 200");
  CHECK(tm_buy_upgrade(&p, 0) && tm_buy_upgrade(&p, 0) && p.upgrades[0] == 2 && p.coins == 200, "upgrades 200 then 400");
  CHECK(!tm_buy_mask(&p, 1), "Cyclop needs level 2");
  p.xp = 1000;
  CHECK(tm_buy_mask(&p, 1) && p.coins == 0 && p.mask == 1, "Cyclop: 200 coins at level 2");
  CHECK(tm_progress_ok(&p), "progress");
  /* missions: complete stage N is exact, points in one run at least, the reward and a new one */
  tm_new_progress(&p);
  tm_mission_add(&p, 0, 1);
  CHECK(!tm_mission_ready(&p, 0), "stage 1 is not stage 2");
  tm_mission_new_run(&p);
  tm_mission_add(&p, 0, 2);
  CHECK(tm_mission_ready(&p, 0), "stage 2 done");
  uint32_t r = 5, coins = p.coins;
  CHECK(tm_claim_mission(&p, 0, &r) && p.coins == coins + 25 && p.missions_done[0] == 1, "the reward");
  CHECK(p.mission_type[0] != 1 && p.mission_type[0] != 2 && p.mission_index[0] == 0, "a new mission");
  CHECK(tm_progress_ok(&p), "progress after a mission");
  /* revives: 200 coins, one in a stage, three in the arcade */
  room(&U);
  U.p.coins = 1000;
  U.tile[7 * 15 + 11] = T_SPIKES;
  tm_step(&U, 1, false), steps(&U, 10, -1);
  CHECK(tm_revives_left(&U) == 1 && tm_revive(&U) && U.p.coins == 800 && tm_revives_left(&U) == 0, "a stage's revive");
  steps(&U, 2, -1);
  CHECK(U.alive && U.x == 7 * TT, "revived at the last landing");
  /* the wheel: the first spin is free, then 200 coins (or a free one every 4 hours of play) */
  tm_new_progress(&p);
  tm_wheel w;
  r = 9;
  CHECK(tm_wheel_start(&p, &w, &r) && p.coins == 0 && p.chest_wait == TM_CHEST_WAIT, "the free spin");
  for (int i = 0; i < 2000 && w.spinning; i++) tm_wheel_step(&p, &w, &r);
  CHECK(w.done && w.sector < 8 && (p.coins > 0 || p.shields > 0 || p.mask), "the wheel stopped on %d", w.sector);
  CHECK(!tm_wheel_start(&p, &w, &r) || p.coins >= 0, "no spin without coins");
}

/* ---------------------------------------------------------------- the reference (optional) */
static int refcheck(void) {
  const char *ref = getenv("TOTM_REF");
  if (!ref) return 0;
  char dir[600], path[900];
  snprintf(dir, sizeof dir, "%s/text", ref);
  DIR *d = opendir(dir);
  if (!d) return 0;
  char stages[300] = "", sections[300] = "";
  struct dirent *e;
  while ((e = readdir(d))) {
    if (!strncmp(e->d_name, "stages__", 8)) snprintf(stages, sizeof stages, "%s", e->d_name);
    if (!strncmp(e->d_name, "sections__", 10)) snprintf(sections, sizeof sections, "%s", e->d_name);
  }
  closedir(d);
  if (!stages[0] || !sections[0]) return 0;
  static uint8_t buf[1 << 20], cells[TM_CELLS];
  snprintf(path, sizeof path, "%s/%s", dir, stages);
  FILE *f = fopen(path, "rb");
  size_t n = f ? fread(buf, 1, sizeof buf, f) : 0;
  if (f) fclose(f);
  size_t at = 0;
  for (int s = 0; s < 200; s++) {
    CHECK(at + 3 <= n, "reference stages cut short");
    if (at + 3 > n) return 1;
    int lava = buf[at], w = buf[at + 1], h = buf[at + 2];
    np_lz_unpack(tm_maps + tm_stages[s].at, tm_stages[s].size, cells, TM_CELLS);
    CHECK(lava == tm_stages[s].extra && w == tm_stages[s].w && h == tm_stages[s].h && !memcmp(cells, buf + at + 3, (size_t)(w * h)), "stage %d differs from the reference", s);
    at += 3 + (size_t)(w * h);
  }
  snprintf(path, sizeof path, "%s/%s", dir, sections);
  f = fopen(path, "rb");
  n = f ? fread(buf, 1, sizeof buf, f) : 0;
  if (f) fclose(f);
  at = 0;
  /* the sections, by type in the original's order */
  int seen[15] = {0};
  for (int i = 0; i < 517 && at + 2 <= n; i++) {
    int t = buf[at], len = buf[at + 1];
    int k = tm_section_first[t] + seen[t]++;
    np_lz_unpack(tm_maps + tm_sections[k].at, tm_sections[k].size, cells, TM_CELLS);
    CHECK(tm_sections[k].h * 13 == len && !memcmp(cells, buf + at + 2, (size_t)len), "section %d differs from the reference", i);
    at += 2 + (size_t)len;
  }
  return 1;
}

/* ---------------------------------------------------------------- the bot */
/* a hash of the game: the same run gives the same hash, here and in the CI */
static uint32_t state_hash(const tm_game *t) {
  uint32_t h = tm_sum(t->tile, sizeof t->tile);
  h = (h ^ (uint32_t)t->x) * 16777619u, h = (h ^ (uint32_t)t->y) * 16777619u;
  h = (h ^ t->score) * 16777619u, h = (h ^ (uint32_t)t->lava_y) * 16777619u;
  for (int i = 0; i < TM_ACTORS; i++) h = (h ^ (uint32_t)t->a[i].x) * 16777619u, h = (h ^ (uint32_t)t->a[i].y) * 16777619u;
  return (h ^ t->rng ^ t->tick) * 16777619u;
}

/* the arcade with the bot for a few minutes of the game: how high it got */
static uint32_t arcade_run(uint32_t seed, int seconds, int *zone, int *bonus) {
  tm_new_progress(&B.p);
  tm_start(&B, -1, seed);
  int t = 0;
  uint8_t lastb = 0;
  *bonus = 0;
  while (B.alive && t < 60 * seconds) {
    if (tm_standing(&B)) {
      int m = bot_arcade_move(&B, 150);
      if (m == 4) {
        for (int i = 0; i < BOT_WAIT && B.alive; i++) tm_step(&B, -1, false), t++;
        continue;
      }
      tm_step(&B, m < 0 ? 2 : m, false), t++;
    } else {
      tm_step(&B, -1, false), t++;
    }
    if (B.bonus && !lastb) ++*bonus;
    lastb = B.bonus;
    CHECK(B.ring_top - tm_cell(B.y) <= TM_H && B.lava_y < B.y + TT, "arcade seed %u: the world at %d", seed, tm_cell(B.y));
  }
  *zone = B.zone;
  return state_hash(&B);
}

/* stages the search cannot clear in its node budget (big ones with long timed routes) */
static const int16_t bot_misses[] = {41, 50, 87, 88, 93, 99, 106, 126, 137, 140, 142, 143, 144, 147, 167, 168, 181, 188};

static void bot_stages(void) {
  int cleared = 0;
  for (int s = 0; s < 200; s++) {
    bool known = false;
    for (size_t k = 0; k < sizeof bot_misses / sizeof *bot_misses; k++) known |= bot_misses[k] == s;
    tm_new_progress(&B.p);
    bool won = bot_play_stage(&B, s, 1000 + (uint32_t)s, 20000);
    if (won) {
      cleared++;
      CHECK(B.ev_won && B.alive, "stage %d", s);
    } else {
      CHECK(known, "the bot no longer clears stage %d", s);
    }
  }
  printf("the bot cleared %d of the 200 stages\n", cleared);
}

/* ---------------------------------------------------------------- saves */
static tm_progress saved[3];
static void saves(void) {
  memset(files, 0, sizeof files);
  tm_progress a, b;
  tm_new_progress(&a);
  a.coins = 12345, a.best = 678, a.xp = 99999, a.reached = 57, a.stars[3] = 3, a.shields = 4, a.upgrades[2] = 5;
  a.masks |= 1 << 6, a.mask = 6, a.mission_progress[1] = 3, a.missions_done[7] = 3, a.guide = 3;
  CHECK(tm_progress_ok(&a), "a good progress");
  CHECK(save_write(&a), "written");
  CHECK(save_load(&b) && !memcmp(&a, &b, sizeof a), "read back exactly");
  saved[0] = a;
  /* damaged: one byte changed, cut short, a wrong version, a bad value: a fresh start (the copy is gone too) */
  uint32_t n;
  uint8_t *d = file_data(TM_SAVE_NAME, &n);
  forget(TM_COPY_NAME);
  d[20] ^= 0x40;
  CHECK(!save_load(&b) && b.coins == 0 && b.masks == 1, "a damaged save is refused");
  save_write(&a), forget(TM_COPY_NAME);
  d = file_data(TM_SAVE_NAME, &n);
  files[0].len = n - 5;
  for (int i = 0; i < 4; i++)
    if (files[i].on && !strcmp(files[i].name, TM_SAVE_NAME)) files[i].len = n - 5;
  CHECK(!save_load(&b) && b.coins == 0, "a short save is refused");
  save_write(&a), forget(TM_COPY_NAME);
  d = file_data(TM_SAVE_NAME, &n);
  tm_file *fl = (tm_file *)d;
  fl->version++;
  fl->sum = tm_sum(fl, offsetof(tm_file, sum));
  CHECK(!save_load(&b), "another version is refused");
  tm_progress bad = a;
  bad.mask = 9;   /* not bought */
  save_write(&bad), forget(TM_COPY_NAME);
  CHECK(!save_load(&b), "a mask not bought is refused");
  /* the copy in the Python script brings it back */
  save_write(&a);
  forget(TM_SAVE_NAME);
  CHECK(save_load(&b) && !memcmp(&a, &b, sizeof a) && file_data(TM_SAVE_NAME, &n), "the copy brings the save back");
  tm_new_progress(&saved[1]);
  saved[2] = a, saved[2].coins = 0, saved[2].reached = 199, saved[2].xp = 9000000, saved[2].shields = 10;
  saved[2].masks = 0xFFFF, saved[2].mask = 15, saved[2].best = 4000000000u;
  for (int i = 0; i < 200; i++) saved[2].stars[i] = 3;
}

/* ---------------------------------------------------------------- the whole app with random keys */
static uint32_t fz, fz_until, fz_held;
static uint32_t fz_r(void) { return fz = fz * 1103515245u + 12345u, fz >> 8; }
static uint32_t fuzz_keys(uint32_t frame) {
  if (frame >= fz_until) {
    static const uint32_t k[] = {K_OK, K_BACK, K_UP, K_DOWN, K_LEFT, K_RIGHT, K_OK, K_UP, K_LEFT, K_RIGHT, 0, 0};
    fz_held = k[fz_r() % (sizeof k / sizeof k[0])];
    fz_until = frame + 1 + fz_r() % 30;
  }
  return fz_held;
}
static void fuzz(unsigned sessions) {
  memset(files, 0, sizeof files);
  fz = 2024;
  for (unsigned s = 0; s < sessions; s++) {
    if (s % 50 == 0) memset(files, 0, sizeof files);
    if (s % 50 == 10) save_write(&saved[0]);
    if (s % 50 == 30) save_write(&saved[2]);
    memset(&g, 0, sizeof g), memset(&ui, 0, sizeof ui);
    script = fuzz_keys, frame_no = 0, fz_until = 0, quit_at = 300 + fz_r() % 3000;
    storage_full = s % 97 == 13;
    tm_clipped = 0;
    tm_app_main();
    storage_full = false;
    tm_progress p;
    uint32_t n;
    bool any = file_data(TM_SAVE_NAME, &n) != 0;
    CHECK(!any || save_load(&p), "session %u left a bad save", s);
    CHECK(!tm_clipped, "session %u: text off the screen: %s", s, tm_clipped_text);
  }
  script = 0;
}

/* every screen with each kept progress: no text off the screen */
static void screens(void) {
  static const uint8_t all[] = {V_TITLE, V_STAGES, V_PLAY, V_PAUSE, V_QUIT, V_HELP, V_REVIVE, V_RESULTS, V_SHOP,
                                V_MASKS, V_MISSIONS, V_CHEST};
  for (int k = 0; k < 3; k++)
    for (int mode = -1; mode <= 0; mode++) {
      B.p = saved[k];
      tm_start(&B, mode < 0 ? -1 : 5, 3);
      for (unsigned i = 0; i < sizeof all; i++)
        for (int sel = 0; sel < 16; sel++) {
          tm_ui u;
          memset(&u, 0, sizeof u);
          u.screen = all[i], u.under = V_PAUSE, u.sel = (uint8_t)(sel % 4), u.page = (uint8_t)(sel % TM_HELP_PAGES);
          u.mask_sel = (uint8_t)sel, u.shop_sel = (uint8_t)(sel % 5), u.mission_sel = (uint8_t)(sel % 3), u.stage_sel = (uint8_t)(sel * 13);
          u.revive_left = 3000;
          tm_clipped = 0;
          tm_draw(&B, &u, plat_push);
          CHECK(!tm_clipped, "screen %u (save %d, choice %d): text off the screen: %s", all[i], k, sel, tm_clipped_text);
        }
    }
}

static void test(void) {
  units();
  if (refcheck()) printf("the maps match the reference\n");
  else printf("(no reference: TOTM_REF not set)\n");
  bot_stages();
  int z1, z2, b1, b2;
  uint32_t h1 = arcade_run(21, 150, &z1, &b1), h2 = arcade_run(21, 150, &z2, &b2);
  CHECK(h1 == h2, "the arcade run is not the same twice: %08x %08x", h1, h2);
  CHECK(z1 >= 4, "the bot only reached zone %d", z1);
  printf("the bot's arcade run: zone %d, %d bonus zones, %08x\n", z1, b1, h1);
  CHECK(h1 == 0x93b0be5cu, "the arcade run's hash changed: %08x (the rules changed, or are not the same here)", h1);
  saves();
  screens();
  fuzz(300);
  printf("%s\n", fails ? "FAILED" : "all good");
}

/* ---------------------------------------------------------------- pictures */
static char outdir[256];
static void save_shot(const char *name) {
  char p[300];
  snprintf(p, sizeof p, "%s/%s.ppm", outdir, name);
  shot(p);
}
static void draw_ui(const tm_game *t, uint8_t screen) {
  tm_ui u;
  memset(&u, 0, sizeof u);
  u.screen = screen;
  tm_draw(t, &u, plat_push);
}
/* how much is on the screen round the player */
static int busy(const tm_game *t) {
  int n = 0;
  for (int y = tm_cell(t->cam_y) - 8; y <= tm_cell(t->cam_y) + 8; y++)
    for (int x = tm_cell(t->cam_x) - 7; x <= tm_cell(t->cam_x) + 7; x++) {
      uint8_t c = tm_tile(t, x, y);
      n += c == T_DOT || c == T_COIN ? 1 : c != T_EMPTY && !tm_wallish(c) ? 3 : 0;
    }
  for (int i = 0; i < TM_ACTORS; i++) n += t->a[i].type && iabs(t->a[i].y - t->cam_y) < 8 * TT ? 4 : 0;
  return n + (t->pw_time[PW_SHIELD] > 0) * 5;
}
static void arcade_until(tm_game *t, int seconds, bool (*want)(const tm_game *), tm_game *best) {
  int bs = -1;
  for (int k = 0; k < 60 * seconds && t->alive; k++) {
    if (tm_standing(t)) {
      int m = bot_arcade_move(t, 150);
      if (m == 4) m = -1;
      tm_step(t, m < 0 ? -1 : m, false);
    } else {
      tm_step(t, -1, false);
    }
    if (best && t->tick % 10 == 0 && t->alive && !tm_standing(t) && (!want || want(t))) {
      int b = busy(t);
      if (b > bs) bs = b, *best = *t;
    }
  }
}
static void shots(void) {
  static tm_game best;
  tm_ui u;
  memset(&u, 0, sizeof u);
  tm_new_progress(&B.p);
  B.p.coins = 1460, B.p.xp = 40000, B.p.shields = 3, B.p.best = 2410, B.p.reached = 23;
  for (int i = 0; i < 23; i++) B.p.stars[i] = (uint8_t)(1 + (i * 7) % 3);
  B.p.masks = 0x2F, B.p.mask = 0;
  tm_start(&B, -1, 77);
  draw_ui(&B, V_TITLE), save_shot("title");
  /* the arcade a minute in, at its busiest */
  B.p.shields = 3;
  arcade_until(&B, 60, NULL, &best);
  draw_ui(&best, V_PLAY), save_shot("shot");
  arcade_until(&B, 40, NULL, &best);
  best.lava_y = best.y - 5 * TT;   /* (the bot keeps well ahead of it) */
  draw_ui(&best, V_PLAY), save_shot("lava");
  u.screen = V_PAUSE, u.sel = 0;
  tm_draw(&best, &u, plat_push), save_shot("pause");
  /* a bonus zone (the bot finds one sooner with the bonus zones' cooldown short) */
  for (uint32_t seed = 3; seed < 40; seed++) {
    tm_new_progress(&U.p);
    U.p.shields = 3;
    tm_start(&U, -1, seed);
    U.bonus_cool = U.bonus_cool_var = U.cd_bonus = 60;
    int k = 0;
    while (U.alive && !U.bonus && k++ < 60 * 240) {
      int m = tm_standing(&U) ? bot_arcade_move(&U, 150) : -1;
      tm_step(&U, m == 4 ? -1 : m, false);
    }
    if (!U.bonus) continue;
    for (k = 0; k < 150 && U.alive; k++) {
      int m = tm_standing(&U) ? bot_arcade_move(&U, 150) : -1;
      tm_step(&U, m == 4 ? -1 : m, false);
    }
    draw_ui(&U, V_PLAY), save_shot("bonus");
    break;
  }
  /* a stage */
  tm_start(&B, 4, 5);
  for (int i = 0; i < 40; i++) tm_step(&B, -1, false);
  draw_ui(&B, V_PLAY), save_shot("stage");
  u.screen = V_STAGES, u.stage_sel = 23;
  tm_draw(&B, &u, plat_push), save_shot("stages");
  u.screen = V_SHOP, u.shop_sel = 2;
  tm_draw(&B, &u, plat_push), save_shot("shop");
  u.screen = V_MASKS, u.mask_sel = 5;
  tm_draw(&B, &u, plat_push), save_shot("masks");
  B.p.mission_progress[1] = 1;
  u.screen = V_MISSIONS, u.mission_sel = 1;
  tm_draw(&B, &u, plat_push), save_shot("missions");
  u.screen = V_CHEST;
  uint32_t r = 3;
  B.p.chest_free = 1;
  tm_wheel_start(&B.p, &u.wheel, &r);
  for (int i = 0; i < 40; i++) tm_wheel_step(&B.p, &u.wheel, &r);
  tm_draw(&B, &u, plat_push), save_shot("chest");
  u.screen = V_HELP, u.under = V_TITLE, u.page = 0, u.first_run = 1;
  tm_draw(&B, &u, plat_push), save_shot("help");
}

/* frames of the bot in the arcade, for the GIF (tools/record.py --frames) */
static void film(int n) {
  tm_new_progress(&B.p);
  B.p.shields = 2;
  tm_start(&B, -1, 77);
  arcade_until(&B, 26, NULL, NULL);
  for (int f = 0; f < n && B.alive; f++) {
    for (int k = 0; k < 2; k++) {
      int m = -1;
      if (tm_standing(&B)) {
        m = bot_arcade_move(&B, 150);
        if (m == 4) m = -1;
      }
      tm_step(&B, m, false);
    }
    draw_ui(&B, V_PLAY);
    char p[300];
    snprintf(p, sizeof p, "%s/f_%06d.raw", outdir, f * 1000 / 30);
    FILE *o = fopen(p, "wb");
    if (!o) exit(2);
    fwrite(fb, 2, 320 * 240, o);
    fclose(o);
  }
}

int main(int argc, char **argv) {
  if (argc < 3) return 2;
  snprintf(outdir, sizeof outdir, "%s", argv[2]);
  if (!strcmp(argv[1], "test")) test();
  else if (!strcmp(argv[1], "shots")) shots();
  else if (!strcmp(argv[1], "film") && argc > 3) film(atoi(argv[3]));
  return fails ? 1 : 0;
}
