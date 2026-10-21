/* Donkey Kong (Nintendo, 1981) on the NumWorks calculator.
 *
 * Kong has taken Pauline to the top of a building site: Mario (Jumpman) climbs after her over the 25m
 * girders and their barrels, the 50m conveyor belts and pies, the 75m elevators and springs, and the 100m
 * rivets that bring Kong down. The arcade's program is ported routine by routine (sim.c) and runs 60 times a
 * second, as the arcade did; the pictures are new (art.h).
 *
 * Keys: the arrows walk and climb, OK or EXE jump, Back pauses, Home saves and quits. */
#include <string.h>
#include "../../../launcher/src/gfx.h"
#include "dk.h"
#include "draw.h"
#include "plat.h"
#include "save.h"

static dk_state run;
static dk_profile prof;
static dk_view view;
static uint32_t rng;
static uint8_t back_to, last_dir, shown_mode;
static color_t strip[SCREEN_W * STRIP_H];
/* frames drawn, the slowest (ms), those over 33 ms (read by tools/emu.py), and the game's 60 Hz steps */
#ifndef HOST
__attribute__((used, externally_visible))
#endif
uint32_t perf_frames, perf_max, perf_slow, perf_ticks;

static uint32_t next_rng(void) {
  rng ^= rng << 13, rng ^= rng >> 17, rng ^= rng << 5;
  return rng;
}

static void best_of_run(void) {
  uint32_t s = dk_score(&run);
  view.new_best = s > prof.best;
  if (s > prof.best) prof.best = s;
}

static void new_game(void) {
  dk_new_game(&run, next_rng());
  uint32_t b = prof.best > 7650 ? prof.best : 7650; /* the arcade's table starts at 7650 */
  for (int i = 0; i < 3; i++, b /= 100) DK_RAM(&run, 0x60B8 + i) = (uint8_t)((b % 100 / 10) << 4 | b % 10);
  prof.has_run = 1;
  if (prof.games < 0xFFFFFFFFu) prof.games++;
  view.screen = DK_SCREEN_PLAY;
  shown_mode = 0;
}

static void end_game(void) {
  best_of_run();
  prof.has_run = 0;
  save_write(&prof, 0);
}

/* the joystick: four ways like the arcade's, the latest arrow pressed winning */
static uint8_t joystick(uint32_t live, uint32_t hit) {
  static const uint32_t keys[4] = {JK_RIGHT, JK_LEFT, JK_UP, JK_DOWN};
  for (int i = 0; i < 4; i++)
    if (hit & keys[i]) last_dir = (uint8_t)(1 << i);
  bool held = false;
  for (int i = 0; i < 4; i++)
    if (last_dir == 1 << i && (live & keys[i])) held = true;
  if (!held) {
    last_dir = 0;
    for (int i = 0; i < 4 && !last_dir; i++)
      if (live & keys[i]) last_dir = (uint8_t)(1 << i);
  }
  return (uint8_t)(last_dir | ((live & JK_OK) ? DK_JUMP : 0));
}

static void track(void) { /* totals: boards cleared, the highest level */
  int m = dk_mode(&run);
  if (m == DK_MODE_CLEAR && shown_mode != DK_MODE_CLEAR && prof.boards < 0xFFFFFFFFu) prof.boards++;
  shown_mode = (uint8_t)m;
  if (dk_level(&run) > prof.best_level) prof.best_level = (uint16_t)dk_level(&run);
}

static void sync_view(void) {
  view.best = prof.best, view.games = prof.games, view.best_level = prof.best_level, view.has_run = prof.has_run;
}

int main(void) {
  plat_begin();
  gfx_buf = strip;
  save_load(&prof, &run);
  rng = plat_random() ^ prof.games * 0x9E3779B9u;
  if (!rng) rng = 1;
  view.screen = prof.has_run ? DK_SCREEN_PAUSE : DK_SCREEN_TITLE;
  uint32_t stale = plat_keys(), prev = stale, last = plat_millis(), acc = 0, saved_at = last;
  uint8_t shown = 0xff;
  bool redraw = true;
  dk_draw_invalidate();
  for (;;) {
    uint32_t now = plat_millis(), keys = plat_keys();
    stale &= keys; /* keys still down from the screen before don't count */
    uint32_t live = keys & ~stale, hit = live & ~prev;
    prev = live;
    if (keys & JK_HOME) {
      if (view.screen == DK_SCREEN_OVER || !prof.has_run) save_write(&prof, 0);
      else save_write(&prof, &run);
      break;
    }
    uint32_t dt = now - last;
    last = now;
    if (dt > 100) dt = 100;
    /* 60ths of a second gone by: the game steps that many times, 4 at most (slower rather than skipping) */
    acc += dt * 60;
    int ticks = 0;
    while (acc >= 1000 && ticks < 4) acc -= 1000, ticks++;
    if (acc >= 1000) acc = 0;
    int steps = 0;
    switch (view.screen) {
    case DK_SCREEN_TITLE:
      view.clock += (uint32_t)ticks;
      redraw |= ticks > 0;
      if (hit & JK_BACK) {
        save_write(&prof, 0);
        return plat_end();
      }
      if (hit & JK_OK) {
        if (!prof.guide_seen) back_to = DK_SCREEN_PLAY, view.screen = DK_SCREEN_GUIDE;
        else new_game();
      }
      break;
    case DK_SCREEN_GUIDE:
      if (hit & (JK_OK | JK_BACK)) {
        prof.guide_seen = 1;
        if (back_to == DK_SCREEN_PLAY) new_game();
        else view.screen = back_to, view.sel = 2;
      }
      break;
    case DK_SCREEN_PLAY:
      if (hit & JK_BACK) {
        view.screen = DK_SCREEN_PAUSE, view.sel = 0;
        save_write(&prof, &run), saved_at = now;
        break;
      }
      for (; steps < ticks; steps++) {
        dk_tick(&run, joystick(live, steps ? 0 : hit));
        perf_ticks++;
        track();
      }
      if (run.over) {
        end_game();
        view.screen = DK_SCREEN_OVER, view.sel = 0;
      } else if (now - saved_at > 10000) {
        save_write(&prof, &run), saved_at = now;
      }
      break;
    case DK_SCREEN_PAUSE:
      if (hit & JK_UP) view.sel = (uint8_t)((view.sel + 3) % 4);
      if (hit & JK_DOWN) view.sel = (uint8_t)((view.sel + 1) % 4);
      if (hit & JK_BACK) view.screen = DK_SCREEN_PLAY, acc = 0;
      else if (hit & JK_OK) {
        if (view.sel == 0) view.screen = DK_SCREEN_PLAY, acc = 0;
        else if (view.sel == 2) back_to = DK_SCREEN_PAUSE, view.screen = DK_SCREEN_GUIDE;
        else view.confirm = view.sel == 3, view.screen = DK_SCREEN_CONFIRM, view.sel = 0;
      }
      break;
    case DK_SCREEN_CONFIRM:
      if (hit & (JK_UP | JK_DOWN)) view.sel ^= 1;
      if ((hit & JK_BACK) || ((hit & JK_OK) && view.sel == 0)) {
        view.sel = view.confirm ? 3 : 1, view.screen = DK_SCREEN_PAUSE;
      } else if (hit & JK_OK) {
        end_game();
        if (view.confirm) view.screen = DK_SCREEN_TITLE, view.sel = 0;
        else new_game();
      }
      break;
    case DK_SCREEN_OVER:
      if (hit & (JK_UP | JK_DOWN)) view.sel ^= 1;
      if (hit & JK_BACK) view.screen = DK_SCREEN_TITLE;
      else if (hit & JK_OK) {
        if (view.sel == 0) new_game();
        else view.screen = DK_SCREEN_TITLE;
      }
      break;
    }
    if (view.screen != shown) {
      stale = keys, prev = 0; /* a new screen starts with all keys up */
      shown = view.screen, redraw = true;
      last_dir = 0;
    }
    if (steps || redraw || hit) {
      sync_view();
      uint32_t t0 = plat_millis();
      dk_draw(&run, &view);
      uint32_t spent = plat_millis() - t0;
      if (view.screen == DK_SCREEN_PLAY) {
        perf_frames++;
        if (spent > perf_max) perf_max = spent;
        if (spent > 33) perf_slow++;
      }
      redraw = false;
    } else {
      plat_sleep(2);
    }
  }
  return plat_end();
}
