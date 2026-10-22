/* Tomb of the Mask (Playgendary) on the NumWorks calculator, from its web version.
 *
 * Dash through the tomb: the player flies until a wall stops it. The arcade is an endless climb ahead of the
 * rising lava, built from the original's 517 sections; the 200 stages each have an exit and three stars. Dots
 * score, coins buy shields, powerup upgrades, masks and spins of the chest's wheel. The tomb steps 60 times a
 * second with the original's numbers (game.c and the rest); the screens are its own (draw.c).
 *
 * Keys: the arrows dash; OK uses a shield; Back pauses; Home saves and quits. */
#include <string.h>
#include "draw.h"
#include "game.h"
#include "plat.h"
#include "save.h"

static tm_game g;
static tm_ui ui;
static int stage_now = -1;          /* the run's stage (-1: the arcade) */
static uint32_t rng;
static uint32_t ended_at;           /* the step the player died or reached the exit (0: neither) */
/* frames drawn, the slowest (ms) and those over 33 ms: read by tools/emu.py */
#ifndef HOST
__attribute__((used, externally_visible))
#endif
uint32_t perf_frames, perf_max, perf_slow;

static uint32_t next_rand(void) {
  rng ^= rng << 13, rng ^= rng >> 17, rng ^= rng << 5;
  return rng ? rng : (rng = 0x6C8E9CF5u);
}

static void save(void) { ui.save_failed = !save_write(&g.p); }

static bool in_run(void) {
  return ui.screen == V_PLAY || ui.screen == V_PAUSE || ui.screen == V_QUIT || ui.screen == V_REVIVE ||
         (ui.screen == V_HELP && ui.under == V_PAUSE);
}

static void begin(int stage) {
  stage_now = stage;
  ui.run_coins = g.p.coins;
  tm_start(&g, stage, next_rand());
  ended_at = 0;
  ui.screen = V_PLAY;
}

/* a run asked for: the guide first, the first time */
static void play(int stage) {
  uint8_t bit = stage < 0 ? 1 : 2;
  if (!(g.p.guide & bit)) {
    g.p.guide |= bit;
    stage_now = stage;
    ui.first_run = 1, ui.page = stage < 0 ? 0 : 2, ui.under = V_TITLE, ui.screen = V_HELP;
    return;
  }
  begin(stage);
}

static void finish(void) {
  tm_end_run(&g);
  save();
  ui.screen = V_RESULTS;
}

static void open_pause(void) { ui.screen = V_PAUSE, ui.sel = PAUSE_RESUME; }

int main(void) {
  plat_begin();
  rng = plat_random() | 1;
  save_load(&g.p);
  ui.screen = V_TITLE;
  uint32_t stale = plat_keys(), prev = stale, last = plat_millis(), acc = 0, drawn = 0, played_ms = 0;
  uint8_t shown = 0xFF;
  int want_dir = -1;
  bool want_shield = false;
  for (;;) {
    uint32_t now = plat_millis(), keys = plat_keys();
    stale &= keys;                      /* keys still down from the screen before don't count */
    uint32_t live = keys & ~stale, hit = live & ~prev;
    prev = live;
    if (keys & K_HOME) break;           /* Home: saved below, then out */
    uint32_t dt = now - last;
    last = now;
    if (dt > 100) dt = 100;
    acc += dt * 60;                     /* 60ths of a second gone by: that many steps, 4 at most */
    int ticks = 0;
    while (acc >= 1000 && ticks < 4) acc -= 1000, ticks++;
    if (acc >= 1000) acc = 0;           /* too slow: the game slows down rather than skip */
    bool quit = false;
    int steps = 0;
    if (ui.screen == V_PLAY) {
      /* a dash on each arrow pressed (the original's swipes), the shield on OK; kept for the next step */
      if (hit & (K_LEFT | K_RIGHT | K_UP | K_DOWN))
        want_dir = hit & K_LEFT ? 0 : hit & K_RIGHT ? 1 : hit & K_UP ? 2 : 3;
      if (hit & K_OK) want_shield = true;
      for (; steps < ticks; steps++) {
        tm_step(&g, want_dir, want_shield);
        want_dir = -1, want_shield = false;
        if (!ended_at && (g.ev_dead || g.ev_won)) ended_at = g.tick;
      }
      played_ms += dt;
      while (played_ms >= 1000) {
        played_ms -= 1000;
        if (g.p.chest_wait) g.p.chest_wait--;
      }
    } else if (ui.screen == V_CHEST && ui.wheel.spinning) {
      for (; steps < ticks; steps++) tm_wheel_step(&g.p, &ui.wheel, &rng);
      if (ui.wheel.done) save();
    }
    switch (ui.screen) {
      case V_TITLE:
        if (hit & K_UP) ui.sel = (uint8_t)((ui.sel + M_COUNT - 1) % M_COUNT);
        if (hit & K_DOWN) ui.sel = (uint8_t)((ui.sel + 1) % M_COUNT);
        if (hit & K_BACK) quit = true;
        else if (hit & K_OK) switch (ui.sel) {
          case M_ARCADE: play(-1); break;
          case M_STAGES: ui.screen = V_STAGES, ui.stage_sel = g.p.reached; break;
          case M_SHOP: ui.screen = V_SHOP, ui.shop_sel = 0; break;
          case M_MASKS: ui.screen = V_MASKS, ui.mask_sel = g.p.mask; break;
          case M_MISSIONS: ui.screen = V_MISSIONS, ui.mission_sel = 0; break;
          default: ui.screen = V_CHEST, memset(&ui.wheel, 0, sizeof ui.wheel); break;
        }
        break;
      case V_STAGES: {
        int s = ui.stage_sel;
        if ((hit & K_LEFT) && s > 0) s--;
        if ((hit & K_RIGHT) && s < 199) s++;
        if ((hit & K_UP) && s >= 8) s -= 8;
        if ((hit & K_DOWN) && s + 8 < 200) s += 8;
        ui.stage_sel = (uint8_t)s;
        if (hit & K_BACK) ui.screen = V_TITLE, ui.sel = M_STAGES;
        else if ((hit & K_OK) && s <= g.p.reached) play(s);
        break;
      }
      case V_PLAY:
        if (ended_at && g.tick - ended_at >= (g.ev_won ? 60u : 120u)) {
          /* the original's 1 s after the exit, 2 s after a death */
          if (g.ev_dead && tm_revives_left(&g) > 0) ui.screen = V_REVIVE, ui.revive_left = 5000;
          else finish();
        } else if (hit & K_BACK) {
          open_pause();
          save();
        }
        break;
      case V_REVIVE:
        ui.revive_left = ui.revive_left > dt ? ui.revive_left - dt : 0;
        if ((hit & K_OK) && tm_revive(&g)) {
          ended_at = 0;
          ui.screen = V_PLAY;
          save();
        } else if ((hit & K_BACK) || !ui.revive_left) {
          finish();
        }
        break;
      case V_RESULTS:
        if (hit & K_OK) {
          if (stage_now >= 0 && g.ev_won && stage_now < 199) begin(stage_now + 1);
          else begin(stage_now);
        } else if (hit & K_BACK) {
          if (stage_now >= 0) ui.screen = V_STAGES, ui.stage_sel = (uint8_t)(g.p.reached < 199 ? g.p.reached : 199);
          else ui.screen = V_TITLE, ui.sel = M_ARCADE;
        }
        break;
      case V_PAUSE:
        if (hit & K_UP) ui.sel = (uint8_t)((ui.sel + PAUSE_COUNT - 1) % PAUSE_COUNT);
        if (hit & K_DOWN) ui.sel = (uint8_t)((ui.sel + 1) % PAUSE_COUNT);
        if (hit & K_BACK) ui.screen = V_PLAY, acc = 0;
        else if (hit & K_OK) switch (ui.sel) {
          case PAUSE_RESUME: ui.screen = V_PLAY, acc = 0; break;
          case PAUSE_RESTART: begin(stage_now); break;
          case PAUSE_HELP: ui.first_run = 0, ui.under = V_PAUSE, ui.screen = V_HELP, ui.page = 0; break;
          default: ui.screen = V_QUIT, ui.sel = 0; break;
        }
        break;
      case V_QUIT:
        if (hit & (K_LEFT | K_RIGHT | K_UP | K_DOWN)) ui.sel ^= 1;
        if (hit & K_BACK) open_pause(), ui.sel = PAUSE_QUIT;
        else if (hit & K_OK) {
          if (ui.sel) quit = true;      /* the run's results are kept (below) */
          else open_pause(), ui.sel = PAUSE_QUIT;
        }
        break;
      case V_HELP:
        if (ui.first_run && (hit & (K_OK | K_BACK))) {
          /* the guide before a first run: OK plays, Back goes back */
          ui.first_run = 0;
          if (hit & K_OK) begin(stage_now);
          else ui.screen = stage_now >= 0 ? V_STAGES : V_TITLE;
        } else if ((hit & (K_RIGHT | K_OK)) && ui.page + 1 < TM_HELP_PAGES) {
          ui.page++;
        } else if ((hit & K_LEFT) && ui.page) {
          ui.page--;
        } else if (hit & (K_BACK | K_OK)) {
          open_pause(), ui.sel = PAUSE_HELP;
        }
        break;
      case V_SHOP:
        if (hit & K_UP) ui.shop_sel = (uint8_t)((ui.shop_sel + 4) % 5);
        if (hit & K_DOWN) ui.shop_sel = (uint8_t)((ui.shop_sel + 1) % 5);
        if (hit & K_BACK) ui.screen = V_TITLE, ui.sel = M_SHOP;
        else if ((hit & K_OK) && (ui.shop_sel ? tm_buy_upgrade(&g.p, ui.shop_sel - 1) : tm_buy_shield(&g.p))) save();
        break;
      case V_MASKS: {
        int m = ui.mask_sel;
        if ((hit & K_LEFT) && m % 8) m--;
        if ((hit & K_RIGHT) && m % 8 < 7) m++;
        if (hit & (K_UP | K_DOWN)) m ^= 8;
        ui.mask_sel = (uint8_t)m;
        if (hit & K_BACK) ui.screen = V_TITLE, ui.sel = M_MASKS;
        else if ((hit & K_OK) && tm_buy_mask(&g.p, m)) save();
        break;
      }
      case V_MISSIONS:
        if (hit & K_UP) ui.mission_sel = (uint8_t)((ui.mission_sel + 2) % 3);
        if (hit & K_DOWN) ui.mission_sel = (uint8_t)((ui.mission_sel + 1) % 3);
        if (hit & K_BACK) ui.screen = V_TITLE, ui.sel = M_MISSIONS;
        else if ((hit & K_OK) && tm_claim_mission(&g.p, ui.mission_sel, &rng)) save();
        break;
      case V_CHEST:
        if (ui.wheel.spinning) break;
        if (hit & K_BACK) ui.screen = V_TITLE, ui.sel = M_CHEST;
        else if ((hit & K_OK) && tm_wheel_start(&g.p, &ui.wheel, &rng)) save();
        break;
    }
    if (quit) break;
    if (ui.screen != shown) {
      stale = keys, prev = 0;            /* a new screen starts with all keys up */
      want_dir = -1, want_shield = false;
      shown = ui.screen;
      drawn = now - 1000;
    }
    if (steps > 0 || hit || now - drawn >= 250) {
      ui.clock = now;
      uint32_t t0 = plat_millis();
      tm_draw(&g, &ui, plat_push);
      uint32_t spent = plat_millis() - t0;
      drawn = now;
      if (ui.screen == V_PLAY) {
        perf_frames++;
        if (spent > perf_max) perf_max = spent;
        if (spent > 33) perf_slow++;
      }
    } else {
      plat_sleep(2);
    }
  }
  if (in_run()) tm_end_run(&g);          /* Home or quitting mid-run: the run counts as played */
  save();
  return plat_end();
}
