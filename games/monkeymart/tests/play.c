/* Monkey Mart on a computer, for tests and pictures (make host, make test).
 *
 *   play test DIR       the rules against the web game's numbers (the spec), a player bot that runs the first mart
 *                       until the truck to the second is bought and then goes on in the second, a few hundred
 *                       sessions of the whole app with random keys (quitting with Home each time), saves written
 *                       and read back exactly, and damaged saves refused
 *   play shots DIR      pictures of the screens (DIR/<name>.ppm)
 *   play film DIR M N   N frames of the bot playing M minutes into the game, every other step (DIR/f_<ms>.raw)
 */
#include "../src/game.c"
#define main mm_app_main
#include "../src/main.c"
#undef main
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include "bot.h"

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
  clock_ms += 10;   /* the app's loop, faster than the game's 60 steps a second: some loops have none */
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
static void draw_ui(const mm_game *gm, uint8_t screen) {
  mm_ui u;
  memset(&u, 0, sizeof u);
  u.screen = screen;
  mm_draw(gm, &u, plat_push);
}

static int fails;
#define CHECK(c, ...) do { if (!(c)) { fails++; printf("FAIL %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

/* ---------------------------------------------------------------- the bot through the game */
static mm_game B;
/* runs the bot until done() or the time runs out; returns the steps taken */
static long bot_run(mm_game *gm, long limit, bool (*done)(const mm_game *), void (*each)(mm_game *, long)) {
  int dx = 0, dy = 0;
  long t = 0;
  for (; t < limit; t++) {
    if (done && done(gm)) break;
    if (t % 6 == 0) bot(gm, &dx, &dy);
    else bot_steer(gm, &dx, &dy);
    if (t % 60 == 0) bot_windows(gm);
    if (gm->want_marts) {
      gm->want_marts = 0;
      if (gm->mi == 0 && gm->p.have[0][gm->m->truck]) mm_travel(gm, 1);
    }
    mm_step(gm, dx, dy);
    if (each) each(gm, t);
  }
  return t;
}

/* a hash of the mart's state: the same run gives the same hash, here and in the CI */
static uint32_t state_hash(const mm_game *gm) {
  uint32_t h = mm_sum(&gm->p, sizeof gm->p);
  for (unsigned i = 0; i < MM_ACTORS; i++) h = (h ^ (uint32_t)gm->a[i].x) * 16777619u, h = (h ^ (uint32_t)gm->a[i].y) * 16777619u;
  for (unsigned i = 0; i < gm->m->count; i++) h = (h ^ gm->box[i].count) * 16777619u;
  return (h ^ gm->rng ^ gm->tick) * 16777619u;
}

/* nothing out of its bounds */
static void invariants(const mm_game *gm, const char *when) {
  const mm_mart *m = gm->m;
  for (unsigned i = 0; i < m->count; i++) {
    const mm_box *b = &gm->box[i];
    CHECK(b->count <= (b->cap > m->places[i].count0 ? b->cap : m->places[i].count0) + 2, "%s: place %u holds %u of %u", when, i, b->count, b->cap);
    CHECK(b->paid <= m->places[i].price, "%s: place %u paid %u", when, i, b->paid);
  }
  for (unsigned i = 0; i < MM_ACTORS; i++) {
    const mm_actor *a = &gm->a[i];
    if (!a->alive) continue;
    CHECK(a->nfood <= MM_CARRY && a->x > -2500 && a->x < 2500 && a->y > -1700 && a->y < 1700, "%s: actor %u at %d,%d carrying %u", when, i, a->x, a->y, a->nfood);
    if (i == A_ME) CHECK(a->nfood <= a->max_food, "%s: you carry %u of %u", when, a->nfood, a->max_food);
  }
  CHECK(gm->nqueue <= MM_BUYERS && gm->ndropped <= MM_DROPPED && gm->ntimers < MM_TIMERS && gm->nalerts <= MM_ALERTS, "%s: lists", when);
  CHECK(gm->cash >= 0, "%s: cash %d", when, gm->cash);
  CHECK(mm_progress_ok(&gm->p), "%s: progress", when);
}

/* ---------------------------------------------------------------- the rules against the spec (spec/rules.md) */
static mm_game U;
static void fresh(unsigned mart) {
  mm_new_progress(&U.p);
  if (mart) U.p.have[0][mm_marts[0].truck] = 1, U.p.mart = 1;
  mm_start(&U, 7);
  U.tutorial = 0;
}
static void steps(mm_game *gm, int n, int dx, int dy) {
  for (int i = 0; i < n; i++) mm_step(gm, dx, dy);
}
static void stand(mm_game *gm, int32_t x, int32_t y) { gm->a[A_ME].x = x, gm->a[A_ME].y = y; }

static void units(void) {
  const mm_mart *m1 = &mm_marts[0], *m2 = &mm_marts[1];
  /* the marts' size and order (DataMart1, DataMart2) */
  CHECK(m1->count == 32 && m2->count == 43, "places %u %u", m1->count, m2->count);
  CHECK(m1->norder == 17 && m2->norder == 18, "order %u %u", m1->norder, m2->norder);
  static const uint16_t order1[17] = {10, 20, 5, 10, 20, 15, 20, 25, 40, 50, 50, 65, 200, 270, 300, 400, 1500};
  for (unsigned i = 0; i < 17; i++) CHECK(m1->places[m1->order[i]].price == order1[i], "mart 1 sale %u costs %u", i, m1->places[m1->order[i]].price);
  static const uint16_t order2[18] = {0, 20, 5, 15, 40, 25, 30, 80, 100, 90, 50, 60, 200, 300, 300, 400, 500, 600};
  for (unsigned i = 0; i < 18; i++) CHECK(m2->places[m2->order[i]].price == order2[i], "mart 2 sale %u costs %u", i, m2->places[m2->order[i]].price);
  unsigned total = 0;
  for (unsigned i = 0; i < m1->count; i++) total += m1->places[i].price;
  CHECK(total == 5290, "mart 1 costs %u in all", total);
  /* prices and how many of a food a customer wants at most (global.lua) */
  static const uint8_t price[15] = {0, 1, 1, 2, 2, 3, 3, 1, 1, 1, 2, 2, 2, 3, 4};
  static const uint8_t most[15] = {0, 4, 4, 2, 2, 2, 2, 3, 3, 3, 2, 2, 2, 2, 2};
  for (unsigned f = 1; f <= 14; f++) CHECK(mm_food_price[f] == price[f] && mm_food_most[f] == most[f], "food %u", f);
  static const uint16_t hat[21] = {0, 1, 300, 300, 500, 500, 2, 700, 700, 1000, 1000, 5, 1500, 2000, 8, 2500, 3000, 3500, 4000, 10, 5000};
  for (unsigned h = 0; h < 21; h++) CHECK(mm_hats[h].price == hat[h], "hat %u costs %u", h, mm_hats[h].price);
  CHECK(mm_hats[6].kind == HAT_ADS && mm_hats[11].kind == HAT_ADS && mm_hats[14].kind == HAT_ADS && mm_hats[19].kind == HAT_ADS &&
        mm_hats[1].kind == HAT_FREE && mm_hat_count() == 17, "the hats' kinds");
  /* the places: plants 2.5 s an item and never tire, animals and machines 6 s and 8 items; the shelves' room */
  unsigned plants = 0, machines = 0;
  for (unsigned n = 0; n < 2; n++)
    for (unsigned i = 0; i < mm_marts[n].count; i++) {
      const mm_place *p = &mm_marts[n].places[i];
      if (!(p->flags & PL_ZAVOD)) continue;
      if (p->feed == MM_NONE) {
        CHECK(p->build == 5 && p->stamina == 1, "plant %u/%u", n, i);
        plants++;
      } else {
        CHECK(p->build == 12 && p->stamina == 8 && p->cap0 == 4, "machine %u/%u", n, i);
        machines++;
      }
    }
  CHECK(plants == 10 + 15 && machines == 4 + 5, "plants %u, machines %u", plants, machines);
  static const uint8_t room1[6][2] = {{4, 15}, {5, 12}, {10, 9}, {13, 9}, {19, 12}, {22, 9}};
  for (unsigned i = 0; i < 6; i++) CHECK(m1->places[room1[i][0]].cap0 == room1[i][1], "shelf %u", room1[i][0]);
  /* upgrades (global.lua): the player's stack free, 100, 200; machines 3 2 3 levels */
  CHECK(m1->uprows[0].max[1] == 3 && m1->uprows[0].price[1][0] == 0 && m1->uprows[0].price[1][1] == 100 && m1->uprows[0].price[1][2] == 200, "your stack");
  CHECK(m1->uprows[5].max[0] == 3 && m1->uprows[5].max[1] == 2 && m1->uprows[5].price[0][0] == 40 && m1->uprows[9].max[2] == 3 && m1->uprows[9].price[2][2] == 500, "mart 1 rows");
  CHECK(m2->nuprow == 12 && m2->uprows[6].max[2] == 3 && m2->uprows[6].price[2][0] == 350 && m2->uprows[11].price[1][1] == 350, "mart 2 rows");

  /* a new game: 35 coins, the tutorial, the cash box for sale at 10 */
  mm_new_progress(&U.p);
  mm_start(&U, 7);
  CHECK(U.p.coins == 35 && U.tutorial && U.step == 0 && U.sale_next == m1->cash && U.box[m1->cash].selling, "a new game");
  CHECK(U.a[A_ME].x == -150 && U.a[A_ME].y == 300 && U.a[A_ME].max_food == 3 && U.a[A_ME].speed == 360, "you at the start");
  /* walking: 6 units a frame, diagonals 4.24 (floored: 4 up and right, 5 down and left) */
  int32_t x0 = U.a[A_ME].x;
  steps(&U, 10, 1, 0);
  CHECK(U.a[A_ME].x == x0 + 60, "right: %d", U.a[A_ME].x - x0);
  x0 = U.a[A_ME].x;
  int32_t y0 = U.a[A_ME].y;
  steps(&U, 1, 1, 1);
  CHECK(U.a[A_ME].x == x0 + 4 && U.a[A_ME].y == y0 + 4, "up right: %d %d", U.a[A_ME].x - x0, U.a[A_ME].y - y0);
  steps(&U, 1, -1, -1);
  CHECK(U.a[A_ME].x == x0 - 1 && U.a[A_ME].y == y0 - 1, "down left");
  /* paying at a pad: a tenth of what's left (rounded up) every 0.07 s, standing still */
  const mm_place *cb = &m1->places[m1->cash];
  stand(&U, cb->buy.x, cb->buy.y);
  int t = 0;
  while (!U.box[m1->cash].active && t < 600) mm_step(&U, 0, 0), t++;
  CHECK(U.box[m1->cash].active && U.p.coins == 25 && t >= 45 && t <= 55, "the cash box: %u coins left, %d steps", U.p.coins, t);
  CHECK(U.sale_next == 4 && U.box[4].selling && U.p.sales[0] == 1 && U.step == 1, "then the shelf");
  /* standing on a pad while walking pays nothing */
  stand(&U, m1->places[4].buy.x, m1->places[4].buy.y);
  steps(&U, 30, 1, 0);
  CHECK(U.box[4].paid == 0, "paid walking");

  /* plants: a banana every 2.5 s, up to 3 */
  fresh(0);
  set_bought(&U, 0);
  U.ntimers = 0;   /* (no customers: they'd take the bananas) */
  CHECK(U.box[0].count == 0, "a new palm is empty");
  steps(&U, 30 * 5 + 30, 0, 0);
  CHECK(U.box[0].count == 1, "a banana in 3 s: %u", U.box[0].count);
  steps(&U, 30 * 5 * 3, 0, 0);
  CHECK(U.box[0].count == 3, "3 at most: %u", U.box[0].count);
  /* picking up: one each 0.15 s, 3 in the arms */
  stand(&U, m1->places[0].work.x, m1->places[0].work.y);
  steps(&U, 1, 0, 0);
  CHECK(U.a[A_ME].nfood == 1 && U.box[0].count == 2, "picked up one");
  steps(&U, 8, 0, 0);
  CHECK(U.a[A_ME].nfood == 1, "not before 0.15 s");
  steps(&U, 1, 0, 0);
  CHECK(U.a[A_ME].nfood == 2, "the second after 0.15 s");
  /* onto its shelf */
  set_bought(&U, 4);
  stand(&U, m1->places[4].work.x, m1->places[4].work.y);
  steps(&U, 30, 0, 0);
  CHECK(U.a[A_ME].nfood == 0 && U.box[4].count == 2, "put on the shelf: %u", U.box[4].count);

  /* animals: an egg each 6 s from a feeder's corn, tired after 8 */
  fresh(0);
  set_bought(&U, 12);
  CHECK(U.box[11].active, "the chicken's dish comes with it");
  U.box[11].count = 4;
  steps(&U, 30 * 12 + 30, 0, 0);
  CHECK(U.box[12].count == 1 && U.box[11].count == 3 && U.box[12].stamina == 7, "an egg: %u, corn %u, stamina %u", U.box[12].count, U.box[11].count, U.box[12].stamina);
  for (int k = 0; k < 7; k++) {
    U.box[12].count = 0, U.box[11].count = 4;
    steps(&U, 30 * 12, 0, 0);
  }
  steps(&U, 40, 0, 0);
  CHECK(U.box[12].stamina == 0 && U.zs[1].sleeping && U.nalerts == 1, "asleep after 8 eggs: %u", U.box[12].stamina);
  U.box[12].count = 0;
  steps(&U, 30 * 30, 0, 0);
  CHECK(U.box[12].count == 0, "no eggs asleep");
  const mm_zavod *ch = &m1->zavods[1];
  stand(&U, ch->pos.x + 60, ch->pos.y - 30);
  steps(&U, 40, 0, 0);
  CHECK(!U.zs[1].sleeping && U.box[12].stamina == 8 && U.nalerts == 0, "woken up");

  /* the cash box: 5% a step (rounded up) */
  fresh(0);
  set_bought(&U, m1->cash);
  U.cash = 100;
  stand(&U, cb->pos.x + 100, cb->pos.y - 30);
  steps(&U, 1, 0, 0);
  CHECK(U.cash == 95 && U.p.coins == 40, "took 5: %d %u", U.cash, U.p.coins);
  steps(&U, 5, 0, 0);
  CHECK(U.cash == 90, "then 5 more: %d", U.cash);
  steps(&U, 600, 0, 0);
  CHECK(U.cash == 0 && U.p.coins == 135, "all of it: %u", U.p.coins);

  /* customers' lists (getWishList): every food sold */
  fresh(0);
  for (unsigned i = 0; i < m1->count; i++) U.box[i].active = 1;
  unsigned rows_seen[7] = {0};
  for (int k = 0; k < 4000; k++) {
    uint8_t ty[6], cn[6];
    unsigned n = wish_list(&U, ty, cn), sum = 0;
    CHECK(n >= 1 && n <= 6, "rows %u", n);
    for (unsigned i = 0; i < n; i++) {
      sum += cn[i];
      CHECK(cn[i] >= 1 && cn[i] <= 5 && ty[i] >= 1 && ty[i] <= 6, "item %u x %u", ty[i], cn[i]);
      for (unsigned j = 0; j < i; j++) CHECK(ty[i] != ty[j], "a food twice");
    }
    CHECK(sum <= 12, "%u items", sum);
    rows_seen[n]++;
  }
  for (unsigned r = 1; r <= 6; r++) CHECK(rows_seen[r] > 50, "%u rows seen %u times", r, rows_seen[r]);
  /* one banana only at first (the bananas' row: 1 row, 1 to 4 items) */
  fresh(0);
  U.box[0].active = 1;
  for (int k = 0; k < 500; k++) {
    uint8_t ty[6], cn[6];
    unsigned n = wish_list(&U, ty, cn);
    CHECK(n == 1 && ty[0] == 1 && cn[0] >= 1 && cn[0] <= 4, "bananas only");
  }

  /* a customer: walks in, takes what it wants, pays it all at the till, leaves */
  fresh(0);
  set_bought(&U, m1->cash), set_bought(&U, 4), set_bought(&U, 0);
  U.box[4].count = 15;
  stand(&U, cb->pos.x + 100, cb->pos.y - 30);
  unsigned waited = 0;
  while (!U.a[A_BUYER0].alive && waited < 200) mm_step(&U, 0, 0), waited++;
  CHECK(U.a[A_BUYER0].alive && waited >= 60 && waited <= 62, "the first customer after 1 s: %u", waited);
  uint16_t cost = U.a[A_BUYER0].cost;
  unsigned want = 0;
  for (unsigned i = 0; i < U.a[A_BUYER0].nwish; i++) want += U.a[A_BUYER0].wish_count[i];
  int32_t before = U.cash;
  uint32_t c0 = U.p.coins;
  for (int k = 0; k < 60 * 120 && U.a[A_BUYER0].alive; k++) {
    mm_step(&U, 0, 0);
    if (U.a[A_BUYER0].stage == S_GO_PAY) U.box[4].count = 15;
  }
  CHECK(!U.a[A_BUYER0].alive, "the customer left");
  CHECK(cost == want * 1 && (uint32_t)(U.cash - before) + (U.p.coins - c0) >= cost, "paid %u for %u bananas", cost, want);

  /* upgrades: your stack free, then 100 and 200; an assistant's speed; a machine's */
  fresh(0);
  U.p.coins = 1000;
  CHECK(mm_upgrade(&U, 0, 1) && U.a[A_ME].max_food == 4 && U.p.coins == 1000, "the free stack");
  CHECK(mm_upgrade(&U, 0, 1) && U.a[A_ME].max_food == 5 && U.p.coins == 900, "100");
  CHECK(mm_upgrade(&U, 0, 1) && U.a[A_ME].max_food == 6 && U.p.coins == 700, "200");
  CHECK(!mm_upgrade(&U, 0, 1) && mm_upgrade_price(&U, 0, 1) < 0 && !mm_upgrade(&U, 0, 0), "at its most");
  CHECK(!mm_upshow_open(&U, 1) && !mm_upgrade(&U, 1, 2), "a worker not hired");
  set_bought(&U, 27);
  CHECK(mm_upgrade(&U, 1, 2) && U.a[A_ASSIST3].speed == 180 && U.p.coins == 700 - 50, "assistant B: speed 5");
  set_bought(&U, 12);
  CHECK(mm_upgrade(&U, 6, 2) && U.box[12].build == 10 && mm_upgrade(&U, 6, 1) && U.box[12].cap == 6 && U.box[11].cap == 6, "the chicken");
  CHECK(mm_upgrade(&U, 6, 0) && U.box[12].stamina_max == 12, "the chicken's stamina");
  U.p.coins = 10;
  CHECK(!mm_upgrade(&U, 6, 2) && U.p.coins == 10 && U.notice_t, "not enough money");
  /* the player's row is the same in mart 2 */
  U.p.have[0][m1->truck] = 1;
  mm_travel(&U, 1);
  CHECK(U.a[A_ME].max_food == 6 && mm_upgrade_price(&U, 0, 1) < 0, "your stack in mart 2");

  /* hats */
  fresh(0);
  U.p.coins = 300;
  CHECK(mm_buy_hat(&U, 2) && U.p.coins == 0 && U.p.hat == 2 && U.a[A_ME].hat == 12, "a hat for 300");
  CHECK(mm_buy_hat(&U, 0) && U.p.hat == 0 && U.a[A_ME].hat == 0, "no hat");
  CHECK(!mm_buy_hat(&U, 6) && !mm_buy_hat(&U, 3) && mm_buy_hat(&U, 1) && U.p.hat == 1, "ads, too dear, free");

  /* mart 2: once the truck is bought; its first visit starts with the cash box and 25 in it */
  fresh(0);
  CHECK(!mm_mart_open(&U, 1), "mart 2 shut");
  U.p.have[0][m1->truck] = 1;
  CHECK(mm_mart_open(&U, 1), "mart 2 open");
  mm_travel(&U, 1);
  CHECK(U.mi == 1 && U.p.sales[1] == 1 && U.cash == 25 && U.box[m2->cash].active && U.sale_next == m2->order[1], "mart 2's start");
  CHECK(U.a[A_ME].x == 1000 && U.a[A_ME].y == -50, "you at mart 2's start");
  mm_travel(&U, 0);
  CHECK(U.mi == 0 && U.p.sales[1] == 1, "back to mart 1");

  /* loading a mart: shelves whose food is made get 0 .. room - 5 */
  mm_new_progress(&U.p);
  for (unsigned k = 0; k < 6; k++) U.p.have[0][m1->order[k]] = 1;
  U.p.sales[0] = 6;
  for (uint32_t seed = 1; seed < 60; seed++) {
    mm_start(&U, seed);
    CHECK(U.box[4].count <= 10 && U.box[5].count <= 7 && !U.tutorial, "filled shelves");
  }
}

/* the tutorial as the bot plays it: the steps in order, the save after the last */
static void tutorial_run(void) {
  mm_new_progress(&B.p);
  mm_start(&B, 3);
  uint8_t last = 0;
  bool order = true;
  long t = 0;
  int dx = 0, dy = 0;
  for (; t < 60L * 600 && B.tutorial; t++) {
    if (t % 6 == 0) bot(&B, &dx, &dy);
    else bot_steer(&B, &dx, &dy);
    if (t % 60 == 0) bot_windows(&B);
    mm_step(&B, dx, dy);
    if (B.step < last) order = false;
    last = B.step;
  }
  CHECK(!B.tutorial && B.step == 11 && order, "the tutorial in %ld s (step %u)", t / 60, B.step);
  CHECK(B.p.hat == 1 && B.a[A_ME].max_food == 4 && B.p.sales[0] >= 3, "its upgrade and hat");
}

static void bot_each(mm_game *gm, long t) {
  if (t % 600 == 0) invariants(gm, gm->mi ? "mart 2" : "mart 1");
}
static bool truck_bought(const mm_game *gm) { return gm->p.have[0][mm_marts[0].truck]; }
static bool mart2_sales(const mm_game *gm) { return gm->mi == 1 && gm->p.sales[1] >= 10; }

static mm_progress saved[4];
static unsigned nsaved;

static uint32_t bot_through(bool check) {
  mm_new_progress(&B.p);
  mm_start(&B, 1234);
  bot_rng = 12345, bot_np = 0, bot_tx = bot_ty = 0, bot_wiggle = bot_stuck = 0, bot_since = 0;
  long t = bot_run(&B, 60L * 3600 * 5, truck_bought, check ? bot_each : 0);
  if (check) {
    CHECK(truck_bought(&B), "the bot didn't buy the truck in %ld s (sales %u)", t / 60, B.p.sales[0]);
    printf("the bot bought the truck to mart 2 after %ld minutes; %u coins\n", t / 3600, B.p.coins);
    saved[nsaved++] = B.p;
  }
  long t2 = bot_run(&B, 60L * 3600 * 3, mart2_sales, check ? bot_each : 0);
  if (check) {
    CHECK(B.mi == 1 && B.p.sales[1] >= 10, "in mart 2: %u sales after %ld minutes", B.p.sales[1], t2 / 3600);
    printf("in mart 2, %u sales %ld minutes later\n", B.p.sales[1], t2 / 3600);
    saved[nsaved++] = B.p;
    for (unsigned f = 1; f <= 11; f++)
      if (f != 9 && f != 10) CHECK(mm_made[f] > 0 && mm_sold[f] > 0, "food %u made %u, sold %u", f, mm_made[f], mm_sold[f]);
  }
  return state_hash(&B);
}

/* ---------------------------------------------------------------- saves */
static void saves(void) {
  memset(files, 0, sizeof files);
  mm_progress p, q;
  CHECK(!save_load(&p) && p.coins == 35 && p.hats == 1, "nothing saved: a new game");
  p = saved[nsaved - 1];
  CHECK(save_write(&p), "written");
  CHECK(save_load(&q) && !memcmp(&p, &q, sizeof p), "read back exactly");
  uint32_t n, nc;
  uint8_t *f = file_data(MM_SAVE_NAME, &n), *c = file_data(MM_COPY_NAME, &nc);
  CHECK(f && n == sizeof(mm_file) && c && nc > 400, "the files: %u, %u bytes", n, nc);
  static uint8_t good[sizeof(mm_file)], goodc[1024];
  memcpy(good, f, n), memcpy(goodc, c, nc);
  /* damaged: from the copy (and put back); both damaged: a new game, never a crash */
  for (uint32_t i = 0; i < n; i++) {
    f[i] ^= (uint8_t)(1 + i % 255);
    CHECK(save_load(&q) && !memcmp(&p, &q, sizeof p), "byte %u damaged: from the copy", i);
    CHECK(!memcmp(file_data(MM_SAVE_NAME, &n), good, n), "put back");
  }
  for (uint32_t i = 0; i < n; i += 7) {
    memcpy(f, good, n), memcpy(c, goodc, nc);
    uint32_t at = (uint32_t)(strstr((char *)goodc + 1, "#>") - (char *)goodc) + 20;
    f[i] ^= 0x55, c[at + i % (nc - at - 2)] ^= 0x21;
    bool ok = save_load(&q);
    CHECK(!ok && q.coins == 35, "both damaged at %u", i);
  }
  memcpy(c, goodc, nc);
  files[0].len = n - 3;
  for (int i = 0; i < 4; i++) if (files[i].on && !strcmp(files[i].name, MM_SAVE_NAME)) files[i].len = n - 3;
  CHECK(save_load(&q) && !memcmp(&p, &q, sizeof p), "cut short: from the copy");
  forget(MM_SAVE_NAME);
  CHECK(save_load(&q) && !memcmp(&p, &q, sizeof p) && file_data(MM_SAVE_NAME, &n), "only the copy left (a reinstall)");
  /* a valid file with impossible numbers: refused */
  mm_file bad;
  memcpy(&bad, good, sizeof bad);
  bad.p.hat = 6;   /* a hat not owned */
  bad.sum = mm_sum(&bad, offsetof(mm_file, sum));
  forget(MM_COPY_NAME);
  plat_save(MM_SAVE_NAME, &bad, sizeof bad);
  CHECK(!save_load(&q), "a hat not owned");
  bad.p.hat = 0, bad.p.paid[0][5] = 999, bad.sum = mm_sum(&bad, offsetof(mm_file, sum));
  plat_save(MM_SAVE_NAME, &bad, sizeof bad);
  CHECK(!save_load(&q), "paid more than the price");
  memset(files, 0, sizeof files);
}

/* ---------------------------------------------------------------- the whole app, with random keys */
static uint32_t fz;
static uint32_t fz_r(void) { fz ^= fz << 13, fz ^= fz >> 17, fz ^= fz << 5; return fz; }
static uint32_t fz_held, fz_until;
static uint32_t fuzz_keys(uint32_t frame) {
  if (frame >= fz_until) {
    static const uint32_t k[] = {K_OK, K_BACK, K_SHIFT, K_UP, K_DOWN, K_LEFT, K_RIGHT, K_UP | K_RIGHT, K_DOWN | K_LEFT, 0, 0};
    fz_held = k[fz_r() % (sizeof k / sizeof k[0])];
    fz_until = frame + 1 + fz_r() % 40;
  }
  return fz_held;
}
static void fuzz(unsigned sessions) {
  memset(files, 0, sizeof files);
  fz = 2024;
  for (unsigned s = 0; s < sessions; s++) {
    if (s % 50 == 0) memset(files, 0, sizeof files);
    if (s % 50 == 10 && nsaved) save_write(&saved[0]);
    if (s % 50 == 30 && nsaved > 1) save_write(&saved[1]);
    memset(&g, 0, sizeof g), memset(&ui, 0, sizeof ui);
    script = fuzz_keys, frame_no = 0, fz_until = 0, quit_at = 200 + fz_r() % 1400;
    storage_full = s % 97 == 13;
    mm_app_main();
    storage_full = false;
    mm_progress p;
    uint32_t n;
    bool any = file_data(MM_SAVE_NAME, &n) != 0;
    CHECK(!any || save_load(&p), "session %u left a bad save", s);
    CHECK(!mm_clipped, "session %u: text off the screen: %s", s, mm_clipped_text);
  }
  script = 0;
}

/* every screen, in the states the tests reach: no text off the screen */
static void screens(void) {
  static const uint8_t all[] = {V_TITLE, V_PLAY, V_PAUSE, V_QUIT, V_HELP, V_UPGRADES, V_HATS, V_MARTS};
  for (unsigned k = 0; k < nsaved; k++) {
    B.p = saved[k];
    mm_start(&B, 5);
    for (unsigned i = 0; i < sizeof all; i++) {
      mm_ui u;
      memset(&u, 0, sizeof u);
      u.screen = all[i];
      u.nitems = 4, u.items[0] = 0, u.items[1] = 1, u.items[2] = 2, u.items[3] = 3;
      for (u.up_top = 0; u.up_top + 3 <= B.m->nupshow; u.up_top++) {
        for (u.page = 0; u.page < MM_HELP_PAGES; u.page++) {
          mm_draw(&B, &u, plat_push);
          CHECK(!mm_clipped, "screen %u (save %u) has text off the screen", all[i], k);
        }
        if (all[i] != V_UPGRADES) break;
      }
    }
  }
}

static void test(void) {
  units();
  tutorial_run();
  uint32_t h1 = bot_through(true);
  uint32_t h2 = bot_through(false);
  CHECK(h1 == h2, "the bot's run is not the same twice: %08x %08x", h1, h2);
  printf("the bot's run: %08x\n", h1);
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
/* how much is on the screen: monkeys, carried food, places with food */
static int busy(const mm_game *gm) {
  int n = 0;
  for (unsigned i = 0; i < MM_ACTORS; i++) {
    const mm_actor *a = &gm->a[i];
    if (a->alive && fabsf((float)a->x - gm->camx) < 430 && fabsf((float)a->y - gm->camy) < 300) n += 3 + a->nfood;
  }
  for (unsigned i = 0; i < gm->m->count; i++) {
    const mm_place *p = &gm->m->places[i];
    if (gm->box[i].active && fabsf((float)p->pos.x - gm->camx) < 400 && fabsf((float)p->pos.y - gm->camy) < 280) n += 2 + gm->box[i].count / 3;
  }
  return n;
}
/* the busiest moment of the next few minutes of the bot, drawn */
static void busiest(const char *name, long ticks) {
  static mm_game best;
  int bscore = -1, dx = 0, dy = 0;
  for (long t = 0; t < ticks; t++) {
    if (t % 6 == 0) bot(&B, &dx, &dy);
    else bot_steer(&B, &dx, &dy);
    if (t % 60 == 0) bot_windows(&B);
    mm_step(&B, dx, dy);
    if (t % 15 == 0 && !B.pan_t && B.a[A_ME].vx == 0 && B.a[A_ME].vy == 0) {
      int b = busy(&B);
      if (b > bscore) bscore = b, best = B;
    }
  }
  draw_ui(&best, V_PLAY), save_shot(name);
}
static void shots(void) {
  mm_ui u;
  memset(&u, 0, sizeof u);
  mm_new_progress(&B.p);
  mm_start(&B, 99);
  draw_ui(&B, V_TITLE), save_shot("title");
  for (int i = 0; i < 30; i++) mm_step(&B, 0, 0);
  draw_ui(&B, V_PLAY), save_shot("guide");
  bot_run(&B, 60L * 60 * 32, 0, 0);
  busiest("shot", 60L * 60 * 4);
  u.screen = V_UPGRADES, u.up_row = 1, u.up_col = 2, u.up_top = 0;
  mm_draw(&B, &u, plat_push), save_shot("upgrades");
  u.screen = V_HATS, u.hat_sel = 4, u.hat_top = 0;
  mm_draw(&B, &u, plat_push), save_shot("hats");
  u.screen = V_PAUSE, u.nitems = 3, u.items[0] = PAUSE_RESUME, u.items[1] = PAUSE_HELP, u.items[2] = PAUSE_QUIT, u.sel = 0;
  mm_draw(&B, &u, plat_push), save_shot("pause");
  u.screen = V_HELP, mm_draw(&B, &u, plat_push), save_shot("help");
  bot_run(&B, 60L * 3600 * 5, truck_bought, 0);
  draw_ui(&B, V_PLAY), save_shot("truck");
  bot_run(&B, 60L * 3600 * 3, mart2_sales, 0);
  busiest("mart2", 60L * 60 * 6);
  u.screen = V_MARTS, u.mart_sel = 0;
  mm_draw(&B, &u, plat_push), save_shot("marts");
}

/* frames of the bot at work, for the GIF (tools/record.py --frames) */
static void film(long minutes, int n) {
  mm_new_progress(&B.p);
  mm_start(&B, 99);
  bot_run(&B, 60L * 60 * minutes, 0, 0);
  int dx = 0, dy = 0;
  for (int f = 0; f < n; f++) {
    for (int k = 0; k < 2; k++) {
      long t = B.tick;
      if (t % 6 == 0) bot(&B, &dx, &dy);
      else bot_steer(&B, &dx, &dy);
      mm_step(&B, dx, dy);
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
  else if (!strcmp(argv[1], "film") && argc > 4) film(atol(argv[3]), atoi(argv[4]));
  else if (!strcmp(argv[1], "save") && argc > 3) {   /* DIR/monkeymart.sav after M minutes of the bot */
    mm_new_progress(&B.p);
    mm_start(&B, 99);
    bot_run(&B, 60L * 60 * atol(argv[3]), 0, 0);
    memset(files, 0, sizeof files);
    save_write(&B.p);
    uint32_t n;
    uint8_t *d = file_data(MM_SAVE_NAME, &n);
    char p[300];
    snprintf(p, sizeof p, "%s/monkeymart.sav", outdir);
    FILE *o = fopen(p, "wb");
    if (!o || !d) return 2;
    fwrite(d, 1, n, o);
    fclose(o);
    printf("mart %u, sales %u %u, coins %u\n", B.p.mart + 1, B.p.sales[0], B.p.sales[1], B.p.coins);
  }
  return fails ? 1 : 0;
}
