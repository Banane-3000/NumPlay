/* Monkey Mart (TinyDobbins) on the NumWorks calculator, from its web version 2.1.
 *
 * Run a grocery mart as a monkey: pick up bananas, corn, eggs and milk, put them on their shelves, take the
 * customers' money at the cash register, and pay for every new shelf, plant, animal, machine and worker by
 * standing on its pad, up to the truck to the second mart. The mart steps 60 times a second with the game's
 * numbers (game.c); the screens are its views (draw.c).
 *
 * Keys: the arrows walk; OK opens the upgrades, Shift the hats; Back pauses; Home saves and quits. */
#include <string.h>
#include "draw.h"
#include "game.h"
#include "plat.h"
#include "save.h"

static mm_game g;
static mm_ui ui;
static mm_progress kept;            /* what the save holds (the game saves nothing during its tutorial) */
/* frames drawn, the slowest (ms) and those over 33 ms: read by tools/emu.py */
#ifndef HOST
__attribute__((used, externally_visible))
#endif
uint32_t perf_frames, perf_max, perf_slow;

static bool save(void) {
  if (!g.tutorial) kept = g.p;
  bool ok = save_write(&kept);
  ui.save_failed = !ok;
  return ok;
}

/* the upgrades window: rows and columns that can be bought from (or shown at their most) */
static bool up_cell(unsigned s, unsigned k) {
  if (s >= g.m->nupshow || k > 2 || !mm_upshow_open(&g, s)) return false;
  const mm_upshow *u = &g.m->upshow[s];
  const mm_uprow *r = &(u->row == 0 ? &mm_marts[0] : g.m)->uprows[u->row];
  return r->max[k] > 0;
}
static void up_fix(void) {
  if (!up_cell(ui.up_row, ui.up_col))
    for (unsigned k = 0; k < 3; k++)
      if (up_cell(ui.up_row, k)) { ui.up_col = (uint8_t)k; break; }
  if (ui.up_row < ui.up_top) ui.up_top = ui.up_row;
  if (ui.up_row >= ui.up_top + 3) ui.up_top = (uint8_t)(ui.up_row - 2);
}
static void up_move(int dr, int dc) {
  if (dc) {
    for (int k = ui.up_col + dc; k >= 0 && k <= 2; k += dc)
      if (up_cell(ui.up_row, (unsigned)k)) { ui.up_col = (uint8_t)k; break; }
  }
  if (dr) {
    for (int r = ui.up_row + dr; r >= 0 && r < g.m->nupshow; r += dr) {
      bool any = false;
      for (unsigned k = 0; k < 3; k++) any |= up_cell((unsigned)r, k);
      if (any) { ui.up_row = (uint8_t)r; break; }
    }
  }
  up_fix();
}
static void open_upgrades(void) {
  ui.screen = V_UPGRADES;
  ui.up_row = 0, ui.up_col = 1, ui.up_top = 0;   /* your stack first: the tutorial's free one */
  up_fix();
  mm_window(&g, 0, true);
}
static void open_hats(void) {
  ui.screen = V_HATS;
  unsigned n = mm_hat_count();
  ui.hat_sel = 0;
  for (unsigned k = 0; k < n; k++)
    if (mm_hat_at(k) == g.p.hat) ui.hat_sel = (uint8_t)k;
  if (g.tutorial) ui.hat_sel = 1;                /* the free one */
  ui.hat_top = (uint8_t)(ui.hat_sel / 3 > 1 ? ui.hat_sel / 3 - 1 : 0);
  mm_window(&g, 1, true);
}
static void open_pause(void) {
  ui.screen = V_PAUSE, ui.sel = 0, ui.nitems = 0;
  ui.items[ui.nitems++] = PAUSE_RESUME;
  if (mm_mart_open(&g, 1)) ui.items[ui.nitems++] = PAUSE_MARTS;
  ui.items[ui.nitems++] = PAUSE_HELP;
  ui.items[ui.nitems++] = PAUSE_QUIT;
}
static void open_marts(void) {
  ui.screen = V_MARTS;
  ui.mart_sel = (uint8_t)(g.mi ^ 1);
}

int main(void) {
  plat_begin();
  save_load(&g.p);
  kept = g.p;
  mm_start(&g, plat_random());
  ui.screen = V_TITLE;
  uint32_t stale = plat_keys(), prev = stale, last = plat_millis(), acc = 0, drawn = 0, saved_at = last;
  uint8_t shown = 0xFF;
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
    /* the mart goes on behind its windows (the game's popups), not behind the pause menu or the title */
    bool running = ui.screen == V_PLAY || ui.screen == V_UPGRADES || ui.screen == V_HATS || ui.screen == V_MARTS;
    int steps = 0;
    if (running)
      for (; steps < ticks; steps++) {
        int dx = 0, dy = 0;
        if (ui.screen == V_PLAY) {
          dx = (live & K_RIGHT ? 1 : 0) - (live & K_LEFT ? 1 : 0);
          dy = (live & K_UP ? 1 : 0) - (live & K_DOWN ? 1 : 0);
        }
        mm_step(&g, dx, dy);
      }
    switch (ui.screen) {
      case V_TITLE:
        if (hit & K_OK) ui.screen = V_PLAY;
        else if (hit & K_BACK) quit = true;
        break;
      case V_PLAY:
        if (g.want_marts) {
          g.want_marts = 0;
          open_marts();
        } else if (hit & K_BACK) {
          open_pause();
          save(), saved_at = now;
        } else if ((hit & K_OK) && (!g.tutorial || (g.step >= 5 && g.step <= 7))) {
          open_upgrades();
        } else if ((hit & K_SHIFT) && (!g.tutorial || (g.step >= 8 && g.step <= 10))) {
          open_hats();
        } else if (now - saved_at > 30000) {
          save(), saved_at = now;
        }
        break;
      case V_UPGRADES:
        if (hit & K_UP) up_move(-1, 0);
        if (hit & K_DOWN) up_move(1, 0);
        if (hit & K_LEFT) up_move(0, -1);
        if (hit & K_RIGHT) up_move(0, 1);
        if (hit & K_OK) {
          if (up_cell(ui.up_row, ui.up_col) && mm_upgrade(&g, ui.up_row, ui.up_col) && !g.tutorial) save();
        } else if ((hit & K_BACK) && !(g.tutorial && g.step < 7)) {
          mm_window(&g, 0, false);
          ui.screen = V_PLAY;
        }
        break;
      case V_HATS: {
        unsigned n = mm_hat_count();
        int s = ui.hat_sel;
        if ((hit & K_LEFT) && s % 3) s--;
        if ((hit & K_RIGHT) && s % 3 < 2 && s + 1 < (int)n) s++;
        if ((hit & K_UP) && s >= 3) s -= 3;
        if ((hit & K_DOWN) && s + 3 < (int)n) s += 3;
        ui.hat_sel = (uint8_t)s;
        if (s / 3 < ui.hat_top) ui.hat_top = (uint8_t)(s / 3);
        if (s / 3 >= ui.hat_top + 3) ui.hat_top = (uint8_t)(s / 3 - 2);
        if (hit & K_OK) {
          if (mm_buy_hat(&g, mm_hat_at(ui.hat_sel)) && !g.tutorial) save();
        } else if ((hit & K_BACK) && !(g.tutorial && g.step < 10)) {
          mm_window(&g, 1, false);
          ui.screen = V_PLAY;
          if (!g.tutorial) save();      /* the tutorial's last step saves */
        }
        break;
      }
      case V_MARTS:
        if (hit & (K_LEFT | K_RIGHT)) ui.mart_sel ^= 1;
        if (hit & K_OK) {
          if (ui.mart_sel != g.mi && mm_mart_open(&g, ui.mart_sel)) {
            mm_travel(&g, ui.mart_sel);
            ui.screen = V_PLAY;
            save(), saved_at = now;
          }
        } else if ((hit & K_BACK) && !g.force_marts) {
          ui.screen = V_PLAY;
        }
        break;
      case V_PAUSE:
        if (hit & K_UP) ui.sel = (uint8_t)((ui.sel + ui.nitems - 1) % ui.nitems);
        if (hit & K_DOWN) ui.sel = (uint8_t)((ui.sel + 1) % ui.nitems);
        if (hit & K_BACK) ui.screen = V_PLAY, acc = 0;
        else if (hit & K_OK) switch (ui.items[ui.sel]) {
          case PAUSE_RESUME: ui.screen = V_PLAY, acc = 0; break;
          case PAUSE_MARTS: open_marts(); break;
          case PAUSE_HELP: ui.under = V_PAUSE, ui.screen = V_HELP, ui.page = 0; break;
          default: ui.screen = V_QUIT, ui.sel = 0; break;
        }
        break;
      case V_QUIT:
        if (hit & (K_LEFT | K_RIGHT | K_UP | K_DOWN)) ui.sel ^= 1;
        if (hit & K_BACK) open_pause(), ui.sel = (uint8_t)(ui.nitems - 1);
        else if (hit & K_OK) {
          if (ui.sel) quit = true;
          else open_pause(), ui.sel = (uint8_t)(ui.nitems - 1);
        }
        break;
      case V_HELP:
        if ((hit & (K_RIGHT | K_OK)) && ui.page + 1 < MM_HELP_PAGES) ui.page++;
        else if ((hit & K_LEFT) && ui.page) ui.page--;
        else if (hit & (K_BACK | K_OK)) {
          open_pause();
          for (unsigned i = 0; i < ui.nitems; i++)
            if (ui.items[i] == PAUSE_HELP) ui.sel = (uint8_t)i;
        }
        break;
    }
    if (quit) break;
    if (ui.screen != shown) {
      stale = keys, prev = 0;            /* a new screen starts with all keys up */
      shown = ui.screen;
      drawn = now - 1000;
    }
    if (steps > 0 || hit || now - drawn >= 500) {
      ui.clock = now;
      uint32_t t0 = plat_millis();
      mm_draw(&g, &ui, plat_push);
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
  save();
  return plat_end();
}
