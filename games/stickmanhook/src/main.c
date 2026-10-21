/* Stickman Hook (Madbox, 2018) on the NumWorks calculator.
 *
 * Swing from hook to hook to the checkered line: hold to grab the nearest hook, let go to fly. The 39 levels
 * made by hand come first; after them every level is a start, a middle and an end drawn at random, as in the
 * web game, without end. The level steps 60 times a second with the original's numbers (game.c); the
 * screens are its scenes and interface (draw.c).
 *
 * Keys: OK or EXE held hooks, Back pauses, Home saves and quits. */
#include <string.h>
#include "draw.h"
#include "game.h"
#include "plat.h"
#include "save.h"

static sh_game game;
static sh_swing swing;
static sh_view view;
static sh_profile prof;
static uint32_t rng;
static uint8_t back_to;   /* where Back goes from Skins, Levels and How to play */
/* frames drawn, the slowest (ms) and those over 33 ms: read by tools/emu.py */
#ifndef HOST
__attribute__((used, externally_visible))
#endif
uint32_t perf_frames, perf_max, perf_slow;

/* the title's stickman, hooked to the logo's red dot at (0.4 * ratio, hook + 0.2), as the web game's title */
static void title(void) {
  view.screen = S_TITLE, view.sel = 0;
  sh_swing_start(&swing, 168.27f / 240, 60.92f / 240, 0.4f * SH_RATIO, 60.92f / 240 + 0.2f);
}

/* a level begins: its course drawn as the game does (again for a random one), unless it's the one left
 * unfinished last time */
static void play(uint32_t level) {
  if (!(prof.has_course && prof.current == level && sh_course_ok(level, &prof.course))) sh_pick(level, &rng, &prof.course);
  prof.current = level, prof.has_course = 1;
  sh_new(&game, level, &prof.course);
  view.screen = S_PLAY;
  save_write(&prof);
}

/* the checkered line crossed: the level counts (the web game's `level++`), maybe a skin opens */
static void finished(void) {
  uint32_t k = game.level;
  view.new_skin = 0xFF;
  if (k == prof.done) {
    prof.done++;
    for (int i = 0; i < SH_SKINS; i++)
      if (sh_unlock_level[i] == prof.done + 1) view.new_skin = (uint8_t)i;
  }
  prof.current = k + 1, prof.has_course = 0;
  save_write(&prof);
}

static void sync_view(uint32_t now) {
  view.clock = now, view.done = prof.done, view.current = prof.current, view.skin = prof.skin;
  view.first_run = !prof.guide_seen, view.game = &game, view.swing = &swing;
}

static void level_grid(uint32_t level) {
  view.level_sel = level;
  uint32_t row = level / SH_LEVEL_COLS;
  if (row < view.level_top) view.level_top = row;
  if (row >= view.level_top + SH_LEVEL_ROWS) view.level_top = row - SH_LEVEL_ROWS + 1;
}

int main(void) {
  plat_begin();
  save_load(&prof);
  rng = plat_random() ^ prof.done * 0x9E3779B9u;
  if (!rng) rng = 1;
  title();
  sh_redraw();
  uint32_t stale = plat_keys(), prev = stale, last = plat_millis(), acc = 0, drawn = 0;
  uint8_t shown = 0xFF;
  for (;;) {
    uint32_t now = plat_millis(), keys = plat_keys();
    stale &= keys;                      /* keys still down from the screen before don't count */
    uint32_t live = keys & ~stale, hit = live & ~prev;
    prev = live;
    if (keys & SK_HOME) break;          /* Home: saved below, then out */
    uint32_t dt = now - last;
    last = now;
    if (dt > 100) dt = 100;
    acc += dt * 60;                     /* 60ths of a second gone by: that many steps, 4 at most */
    int ticks = 0;
    while (acc >= 1000 && ticks < 4) acc -= 1000, ticks++;
    if (acc >= 1000) acc = 0;           /* too slow: the game slows down rather than skip */
    bool moved = false, quit = false;
    switch (view.screen) {
      case S_TITLE:
        for (int i = 0; i < ticks; i++) sh_swing_step(&swing, 1.0f / 60);
        moved = ticks > 0;
        if (hit & (SK_LEFT | SK_RIGHT | SK_UP | SK_DOWN)) view.sel ^= 1;
        if (hit & SK_BACK) quit = true;
        else if (hit & SK_OK) {
          if (view.sel == 1) back_to = S_TITLE, view.screen = S_SKINS, view.sel = prof.skin;
          else if (!prof.guide_seen) back_to = S_PLAY, view.screen = S_HELP;
          else play(prof.current);
        }
        break;
      case S_PLAY:
        if ((hit & SK_BACK) && !game.completed) {
          view.screen = S_PAUSE, view.sel = 0;
          break;
        }
        for (int i = 0; i < ticks; i++) {
          bool was = game.completed;
          sh_step(&game, (live & SK_OK) != 0, &rng);
          if (!was && game.completed) finished();
        }
        moved = ticks > 0;
        if (game.ready) view.screen = view.new_skin < SH_SKINS ? S_UNLOCKED : S_VICTORY, view.sel = 0;
        break;
      case S_PAUSE:
        if (hit & SK_UP) view.sel = (uint8_t)((view.sel + SH_PAUSE_ITEMS - 1) % SH_PAUSE_ITEMS);
        if (hit & SK_DOWN) view.sel = (uint8_t)((view.sel + 1) % SH_PAUSE_ITEMS);
        if (hit & SK_BACK) view.screen = S_PLAY;
        else if (hit & SK_OK) switch (view.sel) {
          case SH_PAUSE_RESUME: view.screen = S_PLAY; break;
          case SH_PAUSE_RESTART: sh_spawn(&game), view.screen = S_PLAY; break;
          case SH_PAUSE_LEVELS: back_to = S_PAUSE, view.screen = S_LEVELS, level_grid(game.level); break;
          case SH_PAUSE_HELP: back_to = S_PAUSE, view.screen = S_HELP; break;
          default: view.screen = S_QUIT, view.sel = 0; break;
        }
        break;
      case S_QUIT:
        if (hit & (SK_LEFT | SK_RIGHT | SK_UP | SK_DOWN)) view.sel ^= 1;
        if (hit & SK_BACK) view.screen = S_PAUSE, view.sel = SH_PAUSE_QUIT;
        else if (hit & SK_OK) {
          if (view.sel) quit = true;
          else view.screen = S_PAUSE, view.sel = SH_PAUSE_QUIT;
        }
        break;
      case S_LEVELS: {
        uint32_t s = view.level_sel;
        if ((hit & SK_LEFT) && s > 0) s--;
        if ((hit & SK_RIGHT) && s < prof.done) s++;
        if (hit & SK_UP) s = s >= SH_LEVEL_COLS ? s - SH_LEVEL_COLS : s;
        if (hit & SK_DOWN) s = s + SH_LEVEL_COLS <= prof.done ? s + SH_LEVEL_COLS : s;
        level_grid(s);
        if (hit & SK_BACK) view.screen = back_to, view.sel = SH_PAUSE_LEVELS;
        else if (hit & SK_OK) play(s);
        break;
      }
      case S_HELP:
        moved = true;
        for (int i = 0; i < ticks; i++) sh_swing_step(&swing, 1.0f / 60);
        if (hit & (SK_OK | SK_BACK)) {
          if (back_to == S_PLAY) {
            prof.guide_seen = 1;
            play(prof.current);
          } else {
            view.screen = back_to, view.sel = SH_PAUSE_HELP;
          }
        }
        break;
      case S_VICTORY:
        moved = true;
        if (hit & (SK_LEFT | SK_RIGHT | SK_UP | SK_DOWN)) view.sel ^= 1;
        if (hit & SK_BACK) title();
        else if (hit & SK_OK) {
          if (view.sel == 0) play(prof.current);
          else back_to = S_VICTORY, view.screen = S_SKINS, view.sel = prof.skin;
        }
        break;
      case S_UNLOCKED:
        moved = true;
        if (hit & SK_BACK) view.screen = S_VICTORY, view.sel = 0;
        else if (hit & SK_OK) {
          prof.skin = view.new_skin;
          save_write(&prof);
          back_to = S_VICTORY, view.screen = S_SKINS, view.sel = prof.skin;
        }
        break;
      case S_SKINS: {
        moved = true;
        int s = view.sel;
        if ((hit & SK_LEFT) && s > 0) s--;
        if ((hit & SK_RIGHT) && s < SH_SKINS - 1) s++;
        if ((hit & SK_UP) && s >= 8) s -= 8;
        if ((hit & SK_DOWN) && s + 8 < SH_SKINS) s += 8;
        else if ((hit & SK_DOWN) && s / 8 < (SH_SKINS - 1) / 8) s = SH_SKINS - 1;
        view.sel = (uint8_t)s;
        if (hit & SK_BACK) {
          if (back_to == S_VICTORY) view.screen = S_VICTORY, view.sel = 1;
          else title(), view.sel = 1;
        } else if ((hit & SK_OK) && sh_skin_open(prof.done, s)) {
          if (s == prof.skin) play(prof.current);   /* the equipped skin again: on to the level, as in the shop */
          else prof.skin = (uint8_t)s, save_write(&prof);
        }
        break;
      }
    }
    if (quit) break;
    if (view.screen != shown) {
      stale = keys, prev = 0;           /* a new screen starts with all keys up */
      shown = view.screen;
      moved = true;
    }
    if ((moved && now - drawn >= (view.screen == S_PLAY ? 0u : 33u)) || hit) {
      sync_view(now);
      uint32_t t0 = plat_millis();
      sh_draw(&view, plat_push);
      uint32_t spent = plat_millis() - t0;
      drawn = now;
      if (view.screen == S_PLAY) {
        perf_frames++;
        if (spent > perf_max) perf_max = spent;
        if (spent > 33) perf_slow++;
      }
    } else {
      plat_sleep(2);
    }
  }
  save_write(&prof);
  return plat_end();
}
