/* PolyGT 3D, a low-poly 3D racer for the NumWorks calculator, by Boris-TH, made for NumPlay: three circuits against
 * up to seven rivals, the endless road against the clock, three cars and two horses, nitro orbs, the slipstream and a
 * ghost of your best lap. The rules are in race.c and track.c, the picture in render.c, the screens
 * in draw.c and the save in save.c.
 *
 * Keys: Up accelerates, Down brakes, Left and Right steer, OK (or EXE) burns nitro, + and - change gear with the
 * manual gearbox, Back pauses; Home saves and quits, from anywhere. */
#include <string.h>
#include "draw.h"
#include "game.h"
#include "plat.h"
#include "render.h"
#include "save.h"

static pg_file sv;                  /* what's kept: your choices and records, the race going on */
static pg_track track;              /* the road built (the race's, or the circuit on the race screen) */
static pg_race prev;
static pg_ui ui;
static uint32_t now, saved_at, hold_at;
static int32_t accum;
static int pending_shift;
static int built = -1;              /* which road is in `track`: a circuit, or 100: the endless road of seed... */
static uint32_t built_seed;         /* ...built_seed */
static uint32_t frame_gap;          /* the least ms between frames (the tests draw fewer) */
/* frames drawn, the slowest (ms) and those over 33 ms: read by tools/emu.py */
#ifndef HOST
__attribute__((used, externally_visible))
#endif
uint32_t perf_frames, perf_max, perf_slow;

/* ------------------------------------------------------------------ the ghost of your best lap on each circuit
   (kept until you quit): where you were every 200 ms */
typedef struct {
  uint16_t s;                       /* in the lap, 1/8 metre */
  int8_t x, a;                      /* across, 1/8 metre; heading from the road, 1/256 turn */
} ghost_pt;
typedef struct {
  uint16_t n;
  uint8_t model, on;
  uint32_t ms;
  ghost_pt p[PG_GHOST];
} ghost;
static ghost best_ghost[PG_CIRCUITS], rec;
static bool rec_ok;                 /* the lap being recorded was driven from its start */
static rd_ghost ghost_now;

static void save(void) {
  ui.save_failed = !save_write(&sv);
  saved_at = now;
}

/* the road for a race, built again when it isn't the one in `track` */
static void show_race(const pg_race *r) {
  if (r->mode == MODE_ENDLESS) {
    if (built != 100 || built_seed != r->seed || !track.endless) {
      pg_endless(&track, r->seed);
      built = 100, built_seed = r->seed;
    }
    pg_extend(&track, pg_seg_of(&track, r->car[0].s) + 48);
  } else if (built != r->track || track.endless) {
    pg_build(&track, r->track);
    built = r->track;
  }
}
static void show_circuit(int id) {
  if (built != id || track.endless) pg_build(&track, id), built = id;
}

static pg_setup setup_of(void) {
  pg_setup s;
  s.mode = sv.p.mode, s.track = sv.p.track, s.vehicle = sv.p.vehicle, s.laps = sv.p.laps, s.rivals = sv.p.rivals;
  s.level = sv.p.level, s.manual = sv.p.manual;
  return s;
}

static void title_menu(void) {
  int n = 0;
  if (sv.has_race) ui.menu[n++] = MENU_CONTINUE;
  ui.menu[n++] = MENU_PLAY, ui.menu[n++] = MENU_RECORDS, ui.menu[n++] = MENU_HELP;
  ui.menu_n = (uint8_t)n;
}
static void to_title(void) {
  ui.screen = V_TITLE, ui.sel = 0;
  title_menu();
}

/* ------------------------------------------------------------------ races */
/* the race screen's choices set to the race just driven (to drive it again) */
static void use_race_settings(void) {
  pg_race *r = &sv.race;
  sv.p.mode = r->mode, sv.p.vehicle = r->vehicle, sv.p.manual = r->manual;
  if (r->mode == MODE_RACE) sv.p.track = r->track, sv.p.laps = r->laps, sv.p.rivals = r->rivals, sv.p.level = r->level;
}

static void race_begin(void) {
  pg_setup s = setup_of();
  pg_new(&sv.race, &track, &s, (plat_random() ^ now) | 1);
  built = s.mode == MODE_ENDLESS ? 100 : s.track, built_seed = sv.race.seed;
  prev = sv.race;
  sv.has_race = 1;
  ui.screen = V_RACE, ui.sel = 0, ui.hold = 0;
  accum = 0, pending_shift = 0;
  rec.n = 0, rec_ok = true;
  save();
}

static void start_or_guide(void) {
  if (!sv.p.guide) {
    ui.screen = V_GUIDE, ui.page = 0;
    return;
  }
  race_begin();
}

static void resume_saved(void) {
  show_race(&sv.race);
  prev = sv.race;
  ui.screen = V_RACE, ui.hold = 3, accum = 0, pending_shift = 0;
  hold_at = now + 500;
  rec.n = 0, rec_ok = false;
}

static void finish_race(void) {
  pg_race *r = &sv.race;
  pg_profile *p = &sv.p;
  ui.new_best = ui.new_lap = ui.new_far = 0;
  if (r->mode == MODE_ENDLESS) {
    uint32_t m = (uint32_t)(r->car[0].s / FX);
    if (m > PG_MAX_M) m = PG_MAX_M;
    if (p->runs < 999999) p->runs++;
    if (m > p->best_m) p->best_m = m, p->best_zone = (uint16_t)(r->zone + 1), ui.new_far = 1;
  } else {
    int rank = r->car[0].place ? r->car[0].place : pg_rank(r, 0);
    ui.result_rank = (uint8_t)rank;
    if (p->races < 999999) p->races++;
    if (rank == 1 && r->rivals) p->wins++;
    int id = r->track;
    uint32_t t = r->car[0].finish_ms;
    if (t && t <= PG_MAX_TIME && (!p->best_ms[id] || t < p->best_ms[id])) p->best_ms[id] = t, ui.new_best = 1;
    if (r->best_lap_ms && r->best_lap_ms <= PG_MAX_TIME && (!p->best_lap_ms[id] || r->best_lap_ms < p->best_lap_ms[id]))
      p->best_lap_ms[id] = r->best_lap_ms, ui.new_lap = 1;
    /* (a best race holds a lap at least as good) */
    if (p->best_ms[id] && (!p->best_lap_ms[id] || p->best_lap_ms[id] > p->best_ms[id])) p->best_lap_ms[id] = p->best_ms[id];
  }
  sv.has_race = 0;
  ui.screen = V_RESULT, ui.sel = 0;
  save();
}

/* the ghost: a sample every 200 ms of the lap; at the end of a lap, kept if it's the best on this circuit */
static void ghost_after_step(int lap_before) {
  pg_race *r = &sv.race;
  if (r->mode != MODE_RACE) return;
  ghost *g = &best_ghost[r->track];
  bool lap_done = r->lap != lap_before || (r->phase == PH_DONE && r->end == END_FINISH && r->car[0].driven && rec.n &&
                                           r->last_lap_ms && r->time_ms - r->lap_at == r->last_lap_ms);
  if (lap_done) {
    if (rec_ok && rec.n > 10 && r->last_lap_ms && (!g->on || r->last_lap_ms < g->ms)) {
      memcpy(g->p, rec.p, sizeof(ghost_pt) * rec.n);
      g->n = rec.n, g->ms = r->last_lap_ms, g->model = r->vehicle, g->on = 1;
    }
    rec.n = 0, rec_ok = r->phase == PH_RACE;
  }
  if (r->phase != PH_RACE || r->car[0].driven) return;
  uint32_t t = r->time_ms - r->lap_at;
  if (t % 200 || t / 200 != rec.n) {
    if (t / 200 > rec.n) rec_ok = false;
    return;
  }
  if (rec.n >= PG_GHOST) {
    rec_ok = false;
    return;
  }
  const pg_car *c = &r->car[0];
  ghost_pt *q = &rec.p[rec.n++];
  q->s = (uint16_t)(pg_lap_s(r, &track) / 32);
  q->x = (int8_t)(c->x / 32);
  q->a = (int8_t)((c->ang + c->slip) / 256);
}

static void ghost_pose(int alpha) {
  ghost_now.on = false;
  pg_race *r = &sv.race;
  if (r->mode != MODE_RACE || r->phase != PH_RACE || track.endless) return;
  const ghost *g = &best_ghost[r->track];
  if (!g->on) return;
  uint32_t t = r->time_ms - r->lap_at + (uint32_t)(alpha * PG_TICK / 256);
  uint32_t i = t / 200, f = t % 200;
  if (i + 1 >= g->n) return;
  const ghost_pt *a = &g->p[i], *b = &g->p[i + 1];
  int32_t L = track.length, sa = a->s * 32, sb = b->s * 32;
  if (sb - sa < -L / 2) sb += L;
  if (sb - sa > L / 2) sb -= L;
  ghost_now.s = sa + (sb - sa) * (int32_t)f / 200;
  ghost_now.x = (a->x * 32) + (b->x - a->x) * 32 * (int32_t)f / 200;
  ghost_now.ang = a->a * 256 + (int32_t)(int8_t)(b->a - a->a) * 256 * (int32_t)f / 200;
  ghost_now.model = g->model;
  ghost_now.on = true;
}

/* ------------------------------------------------------------------ keys, screen by screen */
static bool row_off(int row) {
  return sv.p.mode == MODE_ENDLESS && (row == SET_TRACK || row == SET_LAPS || row == SET_RIVALS || row == SET_LEVEL);
}

static void set_change(int row, int d) {
  pg_profile *p = &sv.p;
  switch (row) {
    case SET_MODE: p->mode ^= 1; break;
    case SET_TRACK: p->track = (uint8_t)((p->track + PG_CIRCUITS + d) % PG_CIRCUITS); break;
    case SET_CAR: p->vehicle = (uint8_t)((p->vehicle + PG_VEHICLES + d) % PG_VEHICLES); break;
    case SET_LAPS: p->laps = (uint8_t)((p->laps - 1 + PG_MAX_LAPS + d) % PG_MAX_LAPS + 1); break;
    case SET_RIVALS: p->rivals = (uint8_t)((p->rivals + PG_CARS + d) % PG_CARS); break;
    case SET_LEVEL: p->level = (uint8_t)((p->level + 3 + d) % 3); break;
    case SET_GEARS: p->manual ^= 1; break;
    case SET_VIEW: p->view = (uint8_t)((p->view + VIEWS + d) % VIEWS); break;
  }
}

static bool keys_menu(uint32_t hit) {
  int n;
  switch (ui.screen) {
    case V_TITLE:
      n = ui.menu_n;
      if (hit & PK_UP) ui.sel = (uint8_t)((ui.sel + n - 1) % n);
      if (hit & PK_DOWN) ui.sel = (uint8_t)((ui.sel + 1) % n);
      if (hit & PK_BACK) ui.screen = V_QUIT, ui.sel = 0;
      else if (hit & PK_OK) switch (ui.menu[ui.sel % n]) {
          case MENU_CONTINUE: resume_saved(); break;
          case MENU_PLAY: ui.screen = V_SETUP, ui.sel = SET_START; break;
          case MENU_RECORDS: ui.screen = V_RECORDS; break;
          default: ui.screen = V_HELP, ui.page = 0, ui.under = V_TITLE; break;
        }
      break;
    case V_QUIT:
      if (hit & (PK_UP | PK_DOWN)) ui.sel ^= 1;
      if (hit & PK_BACK) ui.screen = V_TITLE, ui.sel = 0;
      else if (hit & PK_OK) {
        if (ui.sel) return true;
        ui.screen = V_TITLE, ui.sel = 0;
      }
      break;
    case V_SETUP: {
      int d = (hit & PK_DOWN) ? 1 : (hit & PK_UP) ? -1 : 0;
      if (d) {
        do ui.sel = (uint8_t)((ui.sel + SET_ROWS + d) % SET_ROWS);
        while (row_off(ui.sel));
      }
      int dx = (hit & PK_RIGHT) ? 1 : (hit & PK_LEFT) ? -1 : 0;
      if (dx && ui.sel < SET_START) set_change(ui.sel, dx);
      if (hit & PK_BACK) {
        save();
        to_title();
        ui.sel = (uint8_t)(ui.menu_n - 3);
      } else if (hit & PK_OK) {
        if (ui.sel < SET_START) set_change(ui.sel, 1);
        else if (sv.has_race) ui.screen = V_NEWRUN, ui.sel = 0;
        else start_or_guide();
      }
      if (ui.screen == V_SETUP && sv.p.mode == MODE_RACE) show_circuit(sv.p.track);
      break;
    }
    case V_NEWRUN:
      if (hit & (PK_UP | PK_DOWN)) ui.sel ^= 1;
      if ((hit & PK_BACK) || ((hit & PK_OK) && !ui.sel)) ui.screen = V_SETUP, ui.sel = SET_START;
      else if (hit & PK_OK) sv.has_race = 0, start_or_guide();
      break;
    case V_GUIDE:
      if ((hit & PK_RIGHT) && ui.page + 1 < PG_HELP_PAGES) ui.page++;
      if ((hit & PK_LEFT) && ui.page) ui.page--;
      if (hit & PK_BACK) ui.screen = V_SETUP, ui.sel = SET_START;
      else if (hit & PK_OK) sv.p.guide = 1, race_begin();
      break;
    case V_HELP:
      if ((hit & PK_RIGHT) && ui.page + 1 < PG_HELP_PAGES) ui.page++;
      if ((hit & PK_LEFT) && ui.page) ui.page--;
      if (hit & (PK_BACK | PK_OK)) {
        if (ui.under == V_PAUSE) ui.screen = V_PAUSE, ui.sel = PAUSE_HELP;
        else to_title(), ui.sel = (uint8_t)(ui.menu_n - 1);
      }
      break;
    case V_RECORDS:
      if (hit & (PK_BACK | PK_OK)) to_title(), ui.sel = (uint8_t)(ui.menu_n - 2);
      break;
    case V_PAUSE:
      if (hit & PK_UP) ui.sel = (uint8_t)((ui.sel + PAUSE_ROWS - 1) % PAUSE_ROWS);
      if (hit & PK_DOWN) ui.sel = (uint8_t)((ui.sel + 1) % PAUSE_ROWS);
      if (hit & PK_BACK) ui.screen = V_RACE;
      else if (hit & PK_OK) switch (ui.sel) {
          case PAUSE_RESUME: ui.screen = V_RACE; break;
          case PAUSE_RESTART: use_race_settings(), race_begin(); break;
          case PAUSE_HELP: ui.screen = V_HELP, ui.page = 0, ui.under = V_PAUSE; break;
          default: ui.screen = V_LEAVE, ui.sel = 0; break;
        }
      break;
    case V_LEAVE:
      if (hit & PK_UP) ui.sel = (uint8_t)((ui.sel + 2) % 3);
      if (hit & PK_DOWN) ui.sel = (uint8_t)((ui.sel + 1) % 3);
      if (hit & PK_BACK) ui.screen = V_PAUSE, ui.sel = PAUSE_QUIT;
      else if (hit & PK_OK) {
        if (ui.sel == 0) ui.screen = V_RACE;
        else if (ui.sel == 1) save(), to_title();
        else return true;
      }
      break;
    case V_RESULT:
      if (hit & (PK_UP | PK_DOWN)) ui.sel ^= 1;
      if (hit & PK_OK) {
        if (ui.sel == 0) {
          use_race_settings(), race_begin();
        } else {
          to_title();
        }
      }
      break;
  }
  return false;
}

/* ------------------------------------------------------------------ the loop */
static unsigned race_input(uint32_t keys) {
  unsigned in = 0;
  if (keys & PK_UP) in |= IN_GAS;
  if (keys & PK_DOWN) in |= IN_BRAKE;
  if (keys & PK_LEFT) in |= IN_LEFT;
  if (keys & PK_RIGHT) in |= IN_RIGHT;
  if (keys & PK_OK) in |= IN_NITRO;
  return in;
}

static bool race_over(const pg_race *r) {
  return r->phase == PH_DONE && r->done >= (r->mode == MODE_ENDLESS ? 2 * PG_HZ + PG_HZ / 2 : 3 * PG_HZ);
}

int main(void) {
  plat_begin();
  memset(&ui, 0, sizeof ui);
  now = plat_millis();
  save_load(&sv);
  if (sv.has_race) show_race(&sv.race);
  to_title();
  uint32_t stale = plat_keys(), prev_keys = stale, rep_at = 0, last = now, frame_at = 0, spin0 = now;
  uint8_t shown = 0xFF;
  bool dirty = true;
  uint32_t min_frame = frame_gap ? frame_gap : plat_slow() ? 33 : 20;
  for (;;) {
    now = plat_millis();
    uint32_t dt = now - last;
    if (dt > 100) dt = 100;
    last = now;
    uint32_t keys = plat_keys();
    stale &= keys;                  /* keys still held from the screen before don't count */
    uint32_t live = keys & ~stale, hit = live & ~prev_keys;
    if (ui.screen != V_RACE) {      /* in menus, a held arrow repeats */
      uint32_t arrows = live & (PK_UP | PK_DOWN | PK_LEFT | PK_RIGHT);
      if (hit & arrows) rep_at = now + 350;
      else if (arrows && (int32_t)(now - rep_at) >= 0) hit |= arrows, rep_at = now + 110;
    }
    prev_keys = live;
    if (keys & PK_HOME) break;      /* Home (or On/Off): saved below, then out */
    if (hit && ui.screen != V_RACE) dirty = true;   /* (a race is drawn at its own pace) */
    bool quit = false;
    if (ui.screen == V_RACE) {
      if (hit & PK_BACK) {
        ui.screen = V_PAUSE, ui.sel = PAUSE_RESUME;
        save();
      } else if (ui.hold) {
        if ((int32_t)(now - hold_at) >= 0) ui.hold--, hold_at = now + 500, dirty = true;
        prev = sv.race;
      } else {
        if (hit & PK_GEAR_UP) pending_shift = 1;
        if (hit & PK_GEAR_DOWN) pending_shift = -1;
        accum += (int32_t)dt;
        unsigned in = race_input(live);
        while (accum >= PG_TICK) {
          prev = sv.race;
          int lap = sv.race.lap;
          pg_step(&sv.race, &track, in, pending_shift);
          pending_shift = 0;
          accum -= PG_TICK;
          ghost_after_step(lap);
          if (race_over(&sv.race)) {
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
    if (ui.screen != shown) {
      stale = keys, prev_keys = 0;  /* a new screen starts with all keys up */
      if (ui.screen != V_RACE) accum = 0;
      if (ui.screen == V_SETUP && sv.p.mode == MODE_RACE) show_circuit(sv.p.track);
      if (ui.screen == V_RACE || ui.screen == V_PAUSE || ui.screen == V_RESULT) show_race(&sv.race);
      shown = ui.screen;
      dirty = true;
    }
    bool moving = ui.screen == V_RACE || ui.screen == V_TITLE || ui.screen == V_QUIT;
    if (dirty || (moving && now - frame_at >= min_frame)) {
      uint32_t t0 = plat_millis();
      ui.clock = now - spin0;
      int alpha = accum >= PG_TICK ? 256 : accum * 256 / PG_TICK;
      if (ui.screen != V_RACE || ui.hold) alpha = 256;
      ghost_pose(alpha);
      plat_vblank();
      pg_render(&sv, &track, &prev, &sv.race, alpha, &ui, &ghost_now);
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
