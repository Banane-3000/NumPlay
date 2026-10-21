/* Dune! (Team Lava) on the NumWorks calculator, from its web version (Inlogic Software's Dune Rider).
 *
 * Hold to roll faster down the dunes and to dive in the air, let go to fly off the top of the next one. Land on
 * a slope going down to keep your speed; a hard landing on one going up ends the run. Flying over the line
 * scores 1 to 3, a long fast slide 1 or 3; coins buy balls and landscapes in the shop. The ride steps 60 times
 * a second with the game's numbers and its physics engine's (game.c); the screens are its scenes (draw.c).
 *
 * Keys: OK, EXE or Shift held speed up and dive; Back pauses; Home saves and quits. */
#include <string.h>
#include "draw.h"
#include "game.h"
#include "plat.h"
#include "save.h"
#include "art.h"

static dn_run run;
static dn_profile prof;
static dn_view view;
static uint32_t rng;
/* frames drawn, the slowest (ms) and those over 33 ms: read by tools/emu.py */
#ifndef HOST
__attribute__((used, externally_visible))
#endif
uint32_t perf_frames, perf_max, perf_slow;

/* a new world behind the title: the nine dunes made again (as a page load), the ball dropped at the start */
static void fresh_world(void) {
  dn_new(&run, dn_rand(&rng), prof.ball, prof.land);
  for (int i = 0; i < 90; i++) dn_step(&run, false, false);
}

static void to_title(void) { view.screen = V_TITLE, view.sel = TITLE_PLAY; }

static void play(void) {
  dn_start(&run);
  if (prof.runs < 0xFFFFFFFFu) prof.runs++;
  view.screen = V_PLAY;
}

static bool in_run(void) {
  return run.alive && (view.screen == V_PLAY || view.screen == V_PAUSE || view.screen == V_QUIT ||
                       ((view.screen == V_SHOP || view.screen == V_BUY || view.screen == V_HELP) && view.under == V_PAUSE));
}

static void save(void) { save_write(&prof, in_run() ? &run : 0); }

static bool ball_owned(int i) { return prof.balls >> i & 1; }
static bool land_owned(int i) { return prof.lands >> i & 1; }

static void open_shop(uint8_t under) {
  view.under = under, view.screen = V_SHOP;
  view.shop_cat = SHOP_BALLS, view.shop_side = 0, view.shop_sel = prof.ball;
}

static void sync_view(uint32_t now) {
  view.ball = prof.ball, view.land = prof.land;
  view.owned_balls = prof.balls, view.owned_lands = prof.lands;
  view.best = prof.best, view.coins = prof.coins;
  view.clock = now, view.run = &run;
}

/* the shop's keys: the side bar (categories) or the cards */
static void shop_keys(uint32_t hit) {
  if (view.shop_side) {
    if (hit & (K_UP | K_DOWN)) {
      view.shop_cat ^= 1;
      view.shop_sel = view.shop_cat == SHOP_BALLS ? prof.ball : prof.land;
    }
    if (hit & (K_RIGHT | K_OK)) view.shop_side = 0;
    return;
  }
  int s = view.shop_sel;
  if (view.shop_cat == SHOP_BALLS) {
    int c = s % 13, r = s / 13;
    if (hit & K_LEFT) {
      if (c == 0) view.shop_side = 1;
      else s--;
    }
    if ((hit & K_RIGHT) && c < 12) s++;
    if (hit & (K_UP | K_DOWN)) s = (r ^ 1) * 13 + c;
  } else {
    if (hit & K_LEFT) {
      if (s == 0) view.shop_side = 1;
      else s--;
    }
    if ((hit & K_RIGHT) && s < DN_LANDS - 1) s++;
    if (hit & (K_UP | K_DOWN)) view.shop_side = 1;
  }
  view.shop_sel = (uint8_t)s;
  if (!(hit & K_OK) || view.shop_side) return;
  /* a card: in use already, put to use, or offered for its price (OnBallClicked, OnLandscapeClicked) */
  bool balls = view.shop_cat == SHOP_BALLS;
  if (balls ? ball_owned(s) : land_owned(s)) {
    if (balls) prof.ball = (uint8_t)s;
    else prof.land = (uint8_t)s;
    dn_style(&run, prof.ball, prof.land);
    save();
  } else if (prof.coins >= (balls ? art_ball_info_[s].price : art_land_info_[s].price)) {
    view.screen = V_BUY, view.buy_yes = 1;
  }
}

static void buy(void) {
  int s = view.shop_sel;
  if (view.shop_cat == SHOP_BALLS) {
    prof.coins -= art_ball_info_[s].price, prof.balls |= 1u << s, prof.ball = (uint8_t)s;
  } else {
    prof.coins -= art_land_info_[s].price, prof.lands |= (uint8_t)(1u << s), prof.land = (uint8_t)s;
  }
  dn_style(&run, prof.ball, prof.land);
  save();
}

int main(void) {
  plat_begin();
  bool resumed = save_load(&prof, &run);
  rng = plat_random() ^ prof.runs * 0x9E3779B9u;
  if (!rng) rng = 1;
  if (resumed) {
    view.screen = V_PAUSE, view.sel = PAUSE_RESUME;   /* the run left with Home, paused */
  } else {
    fresh_world();
    to_title();
  }
  dn_redraw();
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
    bool quit = false, frozen = view.screen == V_PAUSE || view.screen == V_QUIT || view.under == V_PAUSE;
    if (view.screen == V_TITLE || view.screen == V_PLAY || view.screen == V_RESULT) frozen = false;
    /* the world goes on behind the menus (the game's physics runs there too), not while paused */
    int steps = 0;
    if (!frozen)
      for (; steps < ticks; steps++) {
        bool play_ = view.screen == V_PLAY;
        dn_step(&run, play_ && (live & (K_OK | K_SHIFT)), play_ && steps == 0 && (hit & (K_OK | K_SHIFT)));
      }
    if (run.picked) prof.coins += run.picked, run.picked = 0;   /* coins count at once */
    if (run.score > prof.best) prof.best = run.score;
    switch (view.screen) {
      case V_TITLE:
        if (hit & K_LEFT) view.sel = view.sel == TITLE_PLAY ? TITLE_SHOP : TITLE_HELP;
        if (hit & K_RIGHT) view.sel = view.sel == TITLE_HELP ? TITLE_SHOP : TITLE_PLAY;
        if (hit & K_UP) view.sel = TITLE_HELP;
        if ((hit & K_DOWN) && view.sel == TITLE_HELP) view.sel = TITLE_SHOP;
        if (hit & K_BACK) quit = true;
        else if (hit & K_OK) {
          if (view.sel == TITLE_PLAY) play();
          else if (view.sel == TITLE_SHOP) open_shop(V_TITLE);
          else view.under = V_TITLE, view.screen = V_HELP;
        }
        break;
      case V_PLAY:
        if (!run.alive) {   /* the crash: "You Crashed!" at once */
          view.screen = V_RESULT;
          save();
        } else if (hit & K_BACK) {
          view.screen = V_PAUSE, view.sel = PAUSE_RESUME;
          save(), saved_at = now;
        } else if (now - saved_at > 10000) {
          save(), saved_at = now;
        }
        break;
      case V_PAUSE:
        if (hit & K_UP) view.sel = (uint8_t)((view.sel + PAUSE_ITEMS - 1) % PAUSE_ITEMS);
        if (hit & K_DOWN) view.sel = (uint8_t)((view.sel + 1) % PAUSE_ITEMS);
        if (hit & K_BACK) view.screen = V_PLAY, acc = 0;
        else if (hit & K_OK) switch (view.sel) {
          case PAUSE_RESUME: view.screen = V_PLAY, acc = 0; break;
          case PAUSE_RESTART: dn_menu(&run), play(); break;
          case PAUSE_SHOP: open_shop(V_PAUSE); break;
          case PAUSE_HELP: view.under = V_PAUSE, view.screen = V_HELP; break;
          default: view.screen = V_QUIT, view.sel = 0; break;
        }
        break;
      case V_QUIT:
        if (hit & (K_LEFT | K_RIGHT | K_UP | K_DOWN)) view.sel ^= 1;
        if (hit & K_BACK) view.screen = V_PAUSE, view.sel = PAUSE_QUIT;
        else if (hit & K_OK) {
          if (view.sel) {   /* the run ends here; coins and best are kept */
            run.alive = 0;
            quit = true;
          } else {
            view.screen = V_PAUSE, view.sel = PAUSE_QUIT;
          }
        }
        break;
      case V_RESULT:
        if (hit & K_OK) dn_reset(&run), play();
        else if (hit & K_BACK) dn_menu(&run), to_title();
        break;
      case V_SHOP:
        if (hit & K_BACK) {
          view.screen = view.under;
          view.sel = view.under == V_PAUSE ? PAUSE_SHOP : TITLE_SHOP;
          view.under = 0;
        } else {
          shop_keys(hit);
        }
        break;
      case V_BUY:
        if (hit & (K_LEFT | K_RIGHT | K_UP | K_DOWN)) view.buy_yes ^= 1;
        if (hit & K_BACK) view.screen = V_SHOP;
        else if (hit & K_OK) {
          if (view.buy_yes) buy();
          view.screen = V_SHOP;
        }
        break;
      case V_HELP:
        if (hit & (K_OK | K_BACK)) {
          view.screen = view.under;
          view.sel = view.under == V_PAUSE ? PAUSE_HELP : TITLE_HELP;
          view.under = 0;
        }
        break;
    }
    if (quit) break;
    if (view.screen != shown) {
      stale = keys, prev = 0;           /* a new screen starts with all keys up */
      shown = view.screen;
      drawn = now - 1000;
    }
    bool moving = steps > 0 || view.screen == V_TITLE;
    if ((moving && now - drawn >= (view.screen == V_PLAY ? 0u : 33u)) || hit || now - drawn >= 1000) {
      sync_view(now);
      uint32_t t0 = plat_millis();
      dn_draw(&view, plat_push);
      uint32_t spent = plat_millis() - t0;
      drawn = now;
      if (view.screen == V_PLAY) {
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
