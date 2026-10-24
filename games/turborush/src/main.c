/* Turbo Rush, an arcade racer for the NumWorks calculator, by Bobydeluxe: six circuits, a Grand Prix, single
 * races and Free Ride, rivals at three levels and a garage. The rules are in race.c and track.c, the view in
 * view.c, the screens in draw.c and the save in save.c.
 *
 * Keys: Up accelerates, Down brakes, Left and Right steer, OK (or EXE) burns nitro, + and - change gear with the
 * manual gearbox, Back pauses; Home saves and quits, from anywhere. */
#include <string.h>
#include "draw.h"
#include "game.h"
#include "plat.h"
#include "save.h"
#include "view.h"

static tr_file sv;                  /* what's kept: your profile, a Grand Prix, the race going on */
static tr_track track;              /* the circuit built (the race's, or the title's) */
static tr_race prev, demo, demo_prev;
static tr_ui ui;
static uint32_t now, saved_at, hold_at, demo_at;
static int32_t accum;
static int pending_shift;
static int shown_track = -1, shown_mood = -1, shown_rain = -1;
static uint32_t frame_gap;          /* the least ms between frames (the tests draw fewer) */
/* frames drawn, the slowest (ms) and those over 33 ms: read by tools/emu.py */
#ifndef HOST
__attribute__((used, externally_visible))
#endif
uint32_t perf_frames, perf_max, perf_slow;

static const uint16_t PRIZE[TR_CARS] = {500, 350, 250, 190, 140, 100, 75, 50};
static const uint8_t POINTS[TR_CARS] = {15, 12, 10, 8, 6, 4, 2, 1};

static char *cat(char *p, const char *s) {
  while (*s) *p++ = *s++;
  *p = 0;
  return p;
}
static char *num(char *p, uint32_t v) {
  char t[12];
  int n = 0;
  do t[n++] = (char)('0' + v % 10); while ((v /= 10) && n < 11);
  while (n) *p++ = t[--n];
  *p = 0;
  return p;
}

static void save(void) {
  ui.save_failed = !save_write(&sv);
  saved_at = now;
}

/* the circuit and its colours, when they change */
static void show(const tr_race *r) {
  if (r->track != shown_track || track.id != r->track || track.n == 0) {
    tr_build(&track, r->track);
    shown_track = r->track, shown_mood = -1;
  }
  if (r->mood != shown_mood || r->rain != shown_rain) {
    view_setup(&track, r->mood, r->rain);
    shown_mood = r->mood, shown_rain = r->rain;
  }
}

static tr_setup setup_of(int mode, int id) {
  tr_setup s;
  memset(&s, 0, sizeof s);
  s.track = (uint8_t)id, s.mode = (uint8_t)mode;
  s.level = mode == MODE_FREE ? sv.p.traffic : sv.p.level;
  s.mood = sv.p.mood, s.rain = sv.p.rain, s.manual = sv.p.manual, s.paint = sv.p.paint, s.style = sv.p.style;
  memcpy(s.up, sv.p.up, 4);
  s.skill = mode == MODE_GP ? sv.gp.skill : NULL;
  return s;
}

/* ------------------------------------------------------------------ the title's race */
static void demo_start(void) {
  tr_setup s = setup_of(MODE_RACE, sv.p.track);
  s.level = LEVEL_EXPERT;
  tr_new(&demo, &track, &s, 0x7A5Eu ^ now);
  shown_track = demo.track, shown_mood = -1;
  for (int i = 0; i < TR_CARS; i++) {
    tr_car *c = &demo.car[i];
    c->z = (40 + i * 22) * FX;
    c->x = c->lane = (i % 3 - 1) * TR_LANE;
    c->v = (44 + i) * FX;
  }
  demo.phase = PH_DONE, demo.end = END_FINISH, demo.countdown = 0, demo.time_ms = 2000;
  demo_prev = demo;
  demo_at = now;
  show(&demo);
}

static void title_menu(void) {
  int n = 0;
  if (sv.has_race || sv.gp.on) ui.menu[n++] = MENU_CONTINUE;
  ui.menu[n++] = MENU_PLAY, ui.menu[n++] = MENU_GARAGE, ui.menu[n++] = MENU_RECORDS, ui.menu[n++] = MENU_HELP;
  ui.menu_n = (uint8_t)n;
}

static void to_title(void) {
  ui.screen = V_TITLE, ui.sel = 0;
  title_menu();
  demo_start();
}

/* ------------------------------------------------------------------ races */
static void race_begin(int mode, int id) {
  tr_setup s = setup_of(mode, id);
  tr_new(&sv.race, &track, &s, plat_random() ^ now);
  shown_track = sv.race.track, shown_mood = -1;
  show(&sv.race);
  prev = sv.race;
  sv.has_race = 1;
  ui.screen = V_RACE, ui.sel = 0, ui.hold = 0;
  accum = 0, pending_shift = 0;
  save();
}

static void gp_new(void) {
  memset(&sv.gp, 0, sizeof sv.gp);
  sv.gp.on = 1;
  uint32_t seed = plat_random() ^ now ^ 0x9E37u;
  for (int i = 0; i < TR_CARS - 1; i++) sv.gp.skill[i] = (uint8_t)i;
  for (int i = TR_CARS - 2; i > 0; i--) {
    int j = (int)(tr_rng(&seed) % (uint32_t)(i + 1));
    uint8_t x = sv.gp.skill[i];
    sv.gp.skill[i] = sv.gp.skill[j], sv.gp.skill[j] = x;
  }
}

/* what you picked on the race screen */
static void start_new(void) {
  sv.has_race = 0;
  memset(&sv.gp, 0, sizeof sv.gp);
  if (sv.p.mode == MODE_GP) gp_new(), race_begin(MODE_GP, 0);
  else race_begin(sv.p.mode, sv.p.track);
}

static void start_or_guide(void) {
  if (!sv.p.guide) {
    ui.screen = V_GUIDE, ui.page = 0;
    return;
  }
  start_new();
}

static void resume_saved(void) {
  show(&sv.race);
  prev = sv.race;
  ui.screen = V_RACE, ui.hold = 3, accum = 0, pending_shift = 0;
  hold_at = now + 500;
}

static void finish_race(void) {
  tr_race *r = &sv.race;
  tr_profile *p = &sv.p;
  bool fin = r->end == END_FINISH;
  int rank = tr_rank(r, 0);
  ui.result_rank = (uint8_t)rank;
  ui.prize = fin ? PRIZE[rank - 1] : 25;
  p->credits = p->credits + ui.prize > TR_MAX_CREDITS ? TR_MAX_CREDITS : p->credits + ui.prize;
  if (p->races < 999999) p->races++;
  if (fin && rank == 1) p->wins++;
  ui.new_best = ui.new_lap = 0;
  int id = r->track;
  if (fin && (!p->best_ms[id] || r->car[0].finish_ms < p->best_ms[id])) p->best_ms[id] = r->car[0].finish_ms, ui.new_best = 1;
  if (r->best_lap_ms && (!p->best_lap_ms[id] || r->best_lap_ms < p->best_lap_ms[id]))
    p->best_lap_ms[id] = r->best_lap_ms, ui.new_lap = 1;
  memset(ui.gained, 0, sizeof ui.gained);
  if (r->mode == MODE_GP && sv.gp.on) {
    uint8_t order[TR_CARS];
    tr_order(r, order);
    int k = 0;
    for (int i = 0; i < TR_CARS; i++) {
      int car = order[i];
      if (car == 0 && !fin) continue;   /* out of the race: no points, the others move up */
      ui.gained[car] = POINTS[k++];
    }
    for (int i = 0; i < TR_CARS; i++) sv.gp.points[i] = (uint16_t)(sv.gp.points[i] + ui.gained[i]);
    memcpy(ui.points, sv.gp.points, sizeof ui.points);
    ui.round_done = (uint8_t)(sv.gp.round + 1);
    if (ui.round_done >= TR_TRACKS) {
      int place = 1;
      for (int i = 1; i < TR_CARS; i++)
        if (sv.gp.points[i] > sv.gp.points[0]) place++;
      ui.final_rank = (uint8_t)place;
      ui.bonus = (uint32_t)(9 - place) * 200;
      p->credits = p->credits + ui.bonus > TR_MAX_CREDITS ? TR_MAX_CREDITS : p->credits + ui.bonus;
      if (place == 1 && p->titles < p->races) p->titles++;
      memset(&sv.gp, 0, sizeof sv.gp);
    } else {
      sv.gp.round++;
    }
  }
  sv.has_race = 0;
  ui.screen = V_RESULT, ui.sel = 0;
  save();
}

/* ------------------------------------------------------------------ keys, screen by screen */
static void set_change(int row, int d) {
  tr_profile *p = &sv.p;
  switch (row) {
    case SET_MODE: p->mode = (uint8_t)((p->mode + 3 + d) % 3); break;
    case SET_TRACK:
      if (p->mode != MODE_GP) p->track = (uint8_t)((p->track + TR_TRACKS + d) % TR_TRACKS);
      break;
    case SET_RIVALS:
      if (p->mode == MODE_FREE) p->traffic = (uint8_t)((p->traffic + 3 + d) % 3);
      else p->level = (uint8_t)((p->level + 3 + d) % 3);
      break;
    case SET_TIME: p->mood = (uint8_t)((p->mood + 3 + d) % 3); break;
    case SET_WEATHER: p->rain ^= 1; break;
    case SET_GEARS: p->manual ^= 1; break;
  }
}

static void garage_buy(int item) {
  tr_profile *p = &sv.p;
  static const char *const NAMES[4] = {"Engine", "Tires", "Nitro", "Armor"};
  unsigned price = tr_price(p, item);
  char *q = ui.note;
  if (!price) {
    cat(q, "Already at its best");
  } else if (p->credits < price) {
    q = cat(q, "You need ");
    q = num(q, price - p->credits);
    cat(q, " more credits");
  } else {
    p->credits -= price;
    p->up[item]++;
    q = cat(q, NAMES[item]);
    q = cat(q, " level ");
    num(q, p->up[item]);
  }
}

static bool keys_menu(uint32_t hit) {
  int n;
  switch (ui.screen) {
    case V_TITLE:
      n = ui.menu_n;
      if (hit & TK_UP) ui.sel = (uint8_t)((ui.sel + n - 1) % n);
      if (hit & TK_DOWN) ui.sel = (uint8_t)((ui.sel + 1) % n);
      if (hit & TK_BACK) ui.screen = V_QUIT, ui.sel = 0;
      else if (hit & TK_OK) switch (ui.menu[ui.sel % n]) {
          case MENU_CONTINUE:
            if (sv.has_race) resume_saved();
            else race_begin(MODE_GP, sv.gp.round);
            break;
          case MENU_PLAY: ui.screen = V_SETUP, ui.sel = SET_START; break;
          case MENU_GARAGE: ui.screen = V_GARAGE, ui.sel = 0, ui.under = V_TITLE, ui.note[0] = 0; break;
          case MENU_RECORDS: ui.screen = V_RECORDS; break;
          default: ui.screen = V_HELP, ui.page = 0, ui.under = V_TITLE; break;
        }
      break;
    case V_QUIT:
      if (hit & (TK_UP | TK_DOWN)) ui.sel ^= 1;
      if (hit & TK_BACK) ui.screen = V_TITLE, ui.sel = 0;
      else if (hit & TK_OK) {
        if (ui.sel) return true;
        ui.screen = V_TITLE, ui.sel = 0;
      }
      break;
    case V_SETUP: {
      int d = (hit & TK_DOWN) ? 1 : (hit & TK_UP) ? -1 : 0;
      if (d) {
        ui.sel = (uint8_t)((ui.sel + SET_ROWS + d) % SET_ROWS);
        if (ui.sel == SET_TRACK && sv.p.mode == MODE_GP) ui.sel = (uint8_t)((ui.sel + SET_ROWS + d) % SET_ROWS);
      }
      int dx = (hit & TK_RIGHT) ? 1 : (hit & TK_LEFT) ? -1 : 0;
      if (dx && ui.sel < SET_START) set_change(ui.sel, dx);
      if (hit & TK_BACK) {
        save();
        to_title();
        ui.sel = (uint8_t)(ui.menu_n - 4);
      } else if (hit & TK_OK) {
        if (ui.sel < SET_START) set_change(ui.sel, 1);
        else if (sv.has_race || sv.gp.on) ui.screen = V_NEWRUN, ui.sel = 0;
        else start_or_guide();
      }
      break;
    }
    case V_NEWRUN:
      if (hit & (TK_UP | TK_DOWN)) ui.sel ^= 1;
      if ((hit & TK_BACK) || ((hit & TK_OK) && !ui.sel)) ui.screen = V_SETUP, ui.sel = SET_START;
      else if (hit & TK_OK) {
        sv.has_race = 0;
        memset(&sv.gp, 0, sizeof sv.gp);
        start_or_guide();
      }
      break;
    case V_GUIDE:
      if ((hit & TK_RIGHT) && ui.page + 1 < TR_HELP_PAGES) ui.page++;
      if ((hit & TK_LEFT) && ui.page) ui.page--;
      if (hit & TK_BACK) ui.screen = V_SETUP, ui.sel = SET_START;
      else if (hit & TK_OK) sv.p.guide = 1, start_new();
      break;
    case V_HELP:
      if ((hit & TK_RIGHT) && ui.page + 1 < TR_HELP_PAGES) ui.page++;
      if ((hit & TK_LEFT) && ui.page) ui.page--;
      if (hit & (TK_BACK | TK_OK)) {
        if (ui.under == V_PAUSE) ui.screen = V_PAUSE, ui.sel = PAUSE_HELP;
        else to_title(), ui.sel = (uint8_t)(ui.menu_n - 1);
      }
      break;
    case V_RECORDS:
      if (hit & (TK_BACK | TK_OK)) to_title(), ui.sel = (uint8_t)(ui.menu_n - 2);
      break;
    case V_GARAGE: {
      int d = (hit & TK_DOWN) ? 1 : (hit & TK_UP) ? -1 : 0;
      int dx = (hit & TK_RIGHT) ? 1 : (hit & TK_LEFT) ? -1 : 0;
      if (d || dx) ui.note[0] = 0;
      if (d) ui.sel = (uint8_t)((ui.sel + GAR_ROWS + d) % GAR_ROWS);
      if (ui.sel == GAR_PAINT && (dx || (hit & TK_OK))) sv.p.paint = (uint8_t)((sv.p.paint + 8 + (dx ? dx : 1)) % 8);
      if (ui.sel == GAR_BODY && (dx || (hit & TK_OK))) sv.p.style ^= 1;
      if (ui.sel < 4 && (hit & TK_OK)) garage_buy(ui.sel);
      if (hit & TK_BACK) {
        save();
        if (ui.under == V_RESULT || ui.under == V_STANDINGS) ui.screen = ui.under, ui.sel = 1;
        else to_title(), ui.sel = (uint8_t)(ui.menu_n - 3);
      }
      break;
    }
    case V_PAUSE:
      if (hit & TK_UP) ui.sel = (uint8_t)((ui.sel + PAUSE_ROWS - 1) % PAUSE_ROWS);
      if (hit & TK_DOWN) ui.sel = (uint8_t)((ui.sel + 1) % PAUSE_ROWS);
      if (hit & TK_BACK) ui.screen = V_RACE;
      else if (hit & TK_OK) switch (ui.sel) {
          case PAUSE_RESUME: ui.screen = V_RACE; break;
          case PAUSE_RESTART: race_begin(sv.race.mode, sv.race.track); break;
          case PAUSE_HELP: ui.screen = V_HELP, ui.page = 0, ui.under = V_PAUSE; break;
          default: ui.screen = V_LEAVE, ui.sel = 0; break;
        }
      break;
    case V_LEAVE:
      if (hit & TK_UP) ui.sel = (uint8_t)((ui.sel + 2) % 3);
      if (hit & TK_DOWN) ui.sel = (uint8_t)((ui.sel + 1) % 3);
      if (hit & TK_BACK) ui.screen = V_PAUSE, ui.sel = PAUSE_QUIT;
      else if (hit & TK_OK) {
        if (ui.sel == 0) ui.screen = V_RACE;
        else if (ui.sel == 1) save(), to_title();
        else return true;
      }
      break;
    case V_RESULT:
      if (hit & TK_UP) ui.sel = (uint8_t)((ui.sel + 2) % 3);
      if (hit & TK_DOWN) ui.sel = (uint8_t)((ui.sel + 1) % 3);
      if (hit & TK_OK) {
        if (ui.sel == 0) {
          if (sv.race.mode == MODE_GP) ui.screen = V_STANDINGS, ui.sel = 0;
          else race_begin(sv.race.mode, sv.race.track);
        } else if (ui.sel == 1) {
          ui.screen = V_GARAGE, ui.under = V_RESULT, ui.sel = 0, ui.note[0] = 0;
        } else {
          to_title();
        }
      }
      break;
    case V_STANDINGS: {
      bool final = ui.round_done >= TR_TRACKS;
      int nb = final ? 2 : 3;
      if (hit & (TK_LEFT | TK_UP)) ui.sel = (uint8_t)((ui.sel + nb - 1) % nb);
      if (hit & (TK_RIGHT | TK_DOWN)) ui.sel = (uint8_t)((ui.sel + 1) % nb);
      if (hit & TK_OK) {
        int b = ui.sel % nb;
        if (b == 1) ui.screen = V_GARAGE, ui.under = V_STANDINGS, ui.sel = 0, ui.note[0] = 0;
        else if (!final && b == 0 && sv.gp.on) race_begin(MODE_GP, sv.gp.round);
        else to_title();
      }
      break;
    }
  }
  return false;
}

/* ------------------------------------------------------------------ the loop */
static unsigned race_input(uint32_t keys) {
  unsigned in = 0;
  if (keys & TK_UP) in |= IN_GAS;
  if (keys & TK_DOWN) in |= IN_BRAKE;
  if (keys & TK_LEFT) in |= IN_LEFT;
  if (keys & TK_RIGHT) in |= IN_RIGHT;
  if (keys & TK_OK) in |= IN_NITRO;
  return in;
}

int main(void) {
  plat_begin();
  memset(&ui, 0, sizeof ui);
  now = plat_millis();
  save_load(&sv);
  to_title();
  uint32_t stale = plat_keys(), prev_keys = stale, rep_at = 0, last = now, frame_at = 0;
  uint8_t shown = 0xFF;
  bool dirty = true;
  uint32_t min_frame = frame_gap ? frame_gap : plat_slow() ? 30 : 18;
  for (;;) {
    now = plat_millis();
    uint32_t dt = now - last;
    if (dt > 100) dt = 100;
    last = now;
    uint32_t keys = plat_keys();
    stale &= keys;                  /* keys still held from the screen before don't count */
    uint32_t live = keys & ~stale, hit = live & ~prev_keys;
    if (ui.screen != V_RACE) {      /* in menus, a held arrow repeats */
      uint32_t arrows = live & (TK_UP | TK_DOWN | TK_LEFT | TK_RIGHT);
      if (hit & arrows) rep_at = now + 350;
      else if (arrows && (int32_t)(now - rep_at) >= 0) hit |= arrows, rep_at = now + 110;
    }
    prev_keys = live;
    if (keys & TK_HOME) break;      /* Home (or On/Off): saved below, then out */
    if (hit) dirty = true;
    bool quit = false;
    if (ui.screen == V_RACE) {
      if (hit & TK_BACK) {
        ui.screen = V_PAUSE, ui.sel = PAUSE_RESUME;
        save();
      } else if (ui.hold) {
        if ((int32_t)(now - hold_at) >= 0) ui.hold--, hold_at = now + 500, dirty = true;
        prev = sv.race;
      } else {
        if (hit & TK_GEAR_UP) pending_shift = 1;
        if (hit & TK_GEAR_DOWN) pending_shift = -1;
        accum += (int32_t)dt;
        unsigned in = race_input(live);
        while (accum >= TR_TICK) {
          prev = sv.race;
          tr_step(&sv.race, &track, in, pending_shift);
          pending_shift = 0;
          accum -= TR_TICK;
          if (sv.race.mode != MODE_FREE && sv.race.phase == PH_DONE && sv.race.done >= 3 * TR_HZ) {
            finish_race();
            break;
          }
        }
        if (ui.screen == V_RACE && (int32_t)(now - saved_at) >= 30000) save();
      }
    } else {
      quit = keys_menu(hit);
    }
    if (quit) break;
    if (ui.screen == V_TITLE || ui.screen == V_QUIT) {
      accum += (int32_t)dt;
      while (accum >= TR_TICK) {
        demo_prev = demo;
        tr_step(&demo, &track, 0, 0);
        accum -= TR_TICK;
      }
      if ((int32_t)(now - demo_at) >= 90000 || demo.finished == TR_CARS) demo_start();
    }
    if (ui.screen != shown) {
      stale = keys, prev_keys = 0;  /* a new screen starts with all keys up */
      if (ui.screen != V_RACE && ui.screen != V_TITLE && ui.screen != V_QUIT) accum = 0;
      shown = ui.screen;
      dirty = true;
    }
    bool moving = ui.screen == V_RACE || ui.screen == V_TITLE || ui.screen == V_QUIT;
    if (dirty || (moving && now - frame_at >= min_frame)) {
      uint32_t t0 = plat_millis();
      ui.clock = now;
      int alpha = accum >= TR_TICK ? 256 : accum * 256 / TR_TICK;
      bool title = ui.screen == V_TITLE || ui.screen == V_QUIT;
      if (title) show(&demo);
      else if (ui.screen == V_RACE || ui.screen == V_PAUSE || ui.screen == V_LEAVE || ui.screen == V_RESULT ||
               ui.screen == V_STANDINGS)
        show(&sv.race);
      if ((ui.screen != V_RACE && !title) || ui.hold) alpha = 256;
      plat_vblank();
      tr_render(&sv, &track, title ? &demo_prev : &prev, title ? &demo : &sv.race, alpha, &ui);
      uint32_t spent = plat_millis() - t0;
      perf_frames++;
      if (spent > perf_max) perf_max = spent;
      if (spent > 33) perf_slow++;
      frame_at = now;
      dirty = false;
    } else {
      plat_sleep(moving ? 2 : 10);
    }
  }
  save();
  return plat_end();
}
