/* PolyTrack for the NumWorks calculator: Kodub's low-poly time trial racer, its 17 official tracks, its car and its
 * physics, made for NumPlay. The physics is in car.c (1 ms frames, as the game), the track in world.c, the picture in
 * render.c, the screens in draw.c, the saves in save.c and the ghost in ghost.c.
 *
 * Keys: Up accelerates, Down brakes (and reverses), Left and Right steer, OK (or EXE) goes back to the last
 * checkpoint, Backspace starts over, Back pauses; Home saves and quits, from anywhere. */
#include <string.h>
#include "car.h"
#include "draw.h"
#include "ghost.h"
#include "plat.h"
#include "render.h"
#include "save.h"
#include "world.h"

static pt_file sv;                  /* what's kept */
static car_t car;
static car_look look, ghost_look;
static chase_t chase;
static camera_t cam;
static ghost_rec rec;
static ghost_play gplay;
static pt_ui ui;
static uint32_t now, saved_at;
static int32_t accum;
static uint16_t cp_seen;
static uint32_t frame_gap;          /* the least ms between frames (the tests draw fewer) */
/* frames drawn, the slowest (ms) and those over 33 ms: read by tools/emu.py */
#ifndef HOST
__attribute__((used, externally_visible))
#endif
uint32_t perf_frames, perf_max, perf_slow;
#define MAX_STEPS 120               /* frames of physics at most between two pictures (else the game slows) */

static void save(void) {
  ui.save_failed = !save_write(&sv);
  saved_at = now;
}

/* ------------------------------------------------------------------ races */
static void race_begin(void) {
  world_load(sv.p.track);
  car_reset(&car);
  look = car_look_of(&car);
  look.paint = sv.p.paint;
  chase_reset(&chase, car.pos, car.rot);
  chase_update(&chase, &cam, 0.0f, car.pos, car.rot, 0.0f);
  ghost_rec_start(&rec);
  ghost_rec_add(&rec, car.pos, car.rot);
  ghost_entry g;
  if (ghosts_find(sv.p.track, &g)) ghost_play_start(&gplay, g.data, g.len, g.ms);
  else ghost_play_start(&gplay, NULL, 0, 0);
  ui.has_ghost = gplay.on;
  ui.screen = V_RACE, ui.sel = 0;
  ui.started = 0, ui.new_record = 0;
  ui.split_at = 0, ui.finish_at = 0, ui.still_ms = 0;
  accum = 0;
  cp_seen = 0;
}

static void start_or_guide(void) {
  if (!sv.p.guide) {
    ui.screen = V_GUIDE, ui.page = 0;
    return;
  }
  race_begin();
}

static void finished(void) {
  uint32_t t = car.finish;
  int id = sv.p.track;
  uint32_t best = sv.p.best[id];
  ui.finish_at = ui.clock ? ui.clock : 1;
  ui.finish_diff = best ? (int32_t)t - (int32_t)best : 0;
  if (sv.p.finishes < 999999) sv.p.finishes++;
  if (!best || t < best) {
    sv.p.best[id] = t;
    int n = car_checkpoints();
    for (int k = 0; k < PT_SPLITS; k++) sv.p.split[id][k] = k < n && k < 64 ? car.cp_frames[k] : 0;
    ui.new_record = 1;
    /* its ghost: the run's last sample, then kept with the others */
    ghost_rec_add(&rec, car.pos, car.rot);
    if (!rec.full && rec.len) {
      ghosts_put(id, t, rec.buf, rec.len);
      ghost_entry g;
      if (ghosts_find(id, &g)) ghost_play_start(&gplay, g.data, g.len, g.ms);
    }
  }
  save();
}

static void step_race(uint32_t keys, uint32_t hit, uint32_t dt) {
  unsigned in = 0;
  if (keys & PK_UP) in |= IN_UP;
  if (keys & PK_DOWN) in |= IN_DOWN;
  if (keys & PK_LEFT) in |= IN_LEFT;
  if (keys & PK_RIGHT) in |= IN_RIGHT;
  if (keys & PK_OK) in |= IN_RESET;
  if (hit & PK_OK) {
    if (!car.has_respawn) {
      /* (no checkpoint yet: back to the start) */
      race_begin();
      return;
    }
    chase_reset(&chase, car.respawn.pos, car.respawn.rot);
  }
  if (!ui.started) {
    /* the car waits at the start until a driving key: the clock starts with it */
    if (!(in & (IN_UP | IN_DOWN | IN_LEFT | IN_RIGHT))) return;
    ui.started = 1;
    accum = 0;
  }
  accum += (int32_t)dt;
  int steps = 0;
  bool was_done = car_finished(&car);
  while (accum >= 1 && steps < MAX_STEPS) {
    car_step(&car, in);              /* (a held OK is one reset: the car looks for it pressed after released) */
    accum--, steps++;
    if (!car.finish && car.frames % GHOST_STEP == 0) ghost_rec_add(&rec, car.pos, car.rot);
    if (car.next_cp > cp_seen) {
      int k = car.next_cp - 1;
      ui.split_at = ui.clock ? ui.clock : 1;
      ui.split_cp = (uint16_t)k;
      uint32_t bs = k < PT_SPLITS ? sv.p.split[sv.p.track][k] : 0;
      ui.split_diff = bs ? (int32_t)car.cp_frames[k] - (int32_t)bs : 0x7FFFFFFF;
    }
    cp_seen = car.next_cp;
    if (!was_done && car_finished(&car)) {
      finished();
      was_done = true;
    }
  }
  if (accum > 100) accum = 100;
  if (pt_absf(car.speed) < 50.0f) ui.still_ms += dt;
  else ui.still_ms = 0;
}

/* ------------------------------------------------------------------ keys, screen by screen */
static int tab_row_of(int track) {
  for (int t = 0; t < 3; t++)
    for (int r = 0; r < tab_rows(t); r++)
      if (track_of_tab(t, r) == track) return r;
  return 0;
}

static bool keys_menu(uint32_t hit) {
  switch (ui.screen) {
    case V_TITLE:
      if (hit & PK_UP) ui.sel = (uint8_t)((ui.sel + MENU_ROWS - 1) % MENU_ROWS);
      if (hit & PK_DOWN) ui.sel = (uint8_t)((ui.sel + 1) % MENU_ROWS);
      if (hit & PK_BACK) ui.screen = V_QUIT, ui.sel = 0;
      else if (hit & PK_OK) switch (ui.sel) {
          case MENU_PLAY:
            ui.screen = V_TRACKS;
            ui.tab = (uint8_t)pt_tracks[sv.p.track].env;
            ui.sel = (uint8_t)tab_row_of(sv.p.track);
            break;
          case MENU_CAR: ui.screen = V_CAR; break;
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
    case V_TRACKS: {
      int n = tab_rows(ui.tab);
      if (hit & PK_UP) ui.sel = (uint8_t)((ui.sel + n - 1) % n);
      if (hit & PK_DOWN) ui.sel = (uint8_t)((ui.sel + 1) % n);
      if (hit & (PK_LEFT | PK_RIGHT)) {
        ui.tab = (uint8_t)((ui.tab + ((hit & PK_RIGHT) ? 1 : 2)) % 3);
        int m = tab_rows(ui.tab);
        if (ui.sel >= m) ui.sel = (uint8_t)(m - 1);
      }
      if (hit & PK_BACK) ui.screen = V_TITLE, ui.sel = MENU_PLAY;
      else if (hit & PK_OK) {
        sv.p.track = (uint8_t)track_of_tab(ui.tab, ui.sel);
        save();
        start_or_guide();
      }
      break;
    }
    case V_CAR:
      if (hit & PK_LEFT) sv.p.paint = (uint8_t)((sv.p.paint + PT_PAINTS - 1) % PT_PAINTS);
      if (hit & PK_RIGHT) sv.p.paint = (uint8_t)((sv.p.paint + 1) % PT_PAINTS);
      if (hit & (PK_UP | PK_DOWN)) sv.p.view ^= 1;
      if (hit & (PK_BACK | PK_OK)) save(), ui.screen = V_TITLE, ui.sel = MENU_CAR;
      break;
    case V_GUIDE:
      if ((hit & PK_RIGHT) && ui.page + 1 < PT_HELP_PAGES) ui.page++;
      if ((hit & PK_LEFT) && ui.page) ui.page--;
      if (hit & PK_BACK) ui.screen = V_TRACKS;
      else if (hit & PK_OK) {
        if (ui.page + 1 < PT_HELP_PAGES) ui.page++;
        else sv.p.guide = 1, save(), race_begin();
      }
      break;
    case V_HELP:
      if ((hit & PK_RIGHT) && ui.page + 1 < PT_HELP_PAGES) ui.page++;
      if ((hit & PK_LEFT) && ui.page) ui.page--;
      if (hit & (PK_BACK | PK_OK)) {
        if (ui.under == V_PAUSE) ui.screen = V_PAUSE, ui.sel = PAUSE_HELP;
        else ui.screen = V_TITLE, ui.sel = MENU_HELP;
      }
      break;
    case V_PAUSE:
      if (hit & PK_UP) ui.sel = (uint8_t)((ui.sel + PAUSE_ROWS - 1) % PAUSE_ROWS);
      if (hit & PK_DOWN) ui.sel = (uint8_t)((ui.sel + 1) % PAUSE_ROWS);
      if (hit & PK_BACK) ui.screen = V_RACE;
      else if (hit & PK_OK) switch (ui.sel) {
          case PAUSE_RESUME: ui.screen = V_RACE; break;
          case PAUSE_RESTART: race_begin(); break;
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
        else if (ui.sel == 1) {
          ui.screen = V_TRACKS;
          ui.tab = (uint8_t)pt_tracks[sv.p.track].env;
          ui.sel = (uint8_t)tab_row_of(sv.p.track);
        } else return true;
      }
      break;
    case V_RESULT:
      if (hit & PK_UP) ui.sel = (uint8_t)((ui.sel + RES_ROWS - 1) % RES_ROWS);
      if (hit & PK_DOWN) ui.sel = (uint8_t)((ui.sel + 1) % RES_ROWS);
      if (hit & PK_RESTART) race_begin();
      else if (hit & PK_OK) {
        if (ui.sel == RES_AGAIN) race_begin();
        else if (ui.sel == RES_NEXT) sv.p.track = (uint8_t)((sv.p.track + 1) % PT_NTRACKS), save(), race_begin();
        else {
          ui.screen = V_TRACKS;
          ui.tab = (uint8_t)pt_tracks[sv.p.track].env;
          ui.sel = (uint8_t)tab_row_of(sv.p.track);
        }
      }
      break;
  }
  return false;
}

/* ------------------------------------------------------------------ the loop */
int main(void) {
  plat_begin();
  memset(&ui, 0, sizeof ui);
  now = plat_millis();
  save_load(&sv);
  ghosts_load();
  world_load(sv.p.track);
  car_reset(&car);
  ui.screen = V_TITLE;
  uint32_t stale = plat_keys(), prev_keys = stale, rep_at = 0, last = now, frame_at = 0, spin0 = now;
  uint8_t shown = 0xFF;
  bool dirty = true;
  uint32_t min_frame = frame_gap ? frame_gap : plat_slow() ? 33 : 25;
  uint32_t slow_frames = 0, quick_frames = 0;
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
    if (hit && ui.screen != V_RACE) dirty = true;
    bool quit = false;
    ui.clock = now - spin0;
    if (ui.screen == V_RACE) {
      if (hit & PK_BACK) {
        ui.screen = V_PAUSE, ui.sel = PAUSE_RESUME;
      } else if (hit & PK_RESTART) {
        race_begin();
      } else {
        if (hit & PK_VIEW) sv.p.view ^= 1;
        step_race(live, hit, dt);
        if (car.finish && ui.finish_at && ui.clock - ui.finish_at > 2000 && ui.screen == V_RACE)
          ui.screen = V_RESULT, ui.sel = RES_AGAIN;
      }
    } else if (ui.screen == V_RESULT) {
      /* (the car rolls on behind the results) */
      step_race(0, 0, dt);
      quit = keys_menu(hit);
    } else {
      quit = keys_menu(hit);
    }
    if (quit) break;
    if (ui.screen != shown) {
      stale = keys, prev_keys = 0;  /* a new screen starts with all keys up */
      shown = ui.screen;
      dirty = true;
    }
    bool moving = ui.screen == V_RACE || ui.screen == V_RESULT || ui.screen == V_TITLE || ui.screen == V_QUIT ||
                  ui.screen == V_CAR;
    if (dirty || (moving && now - frame_at >= min_frame)) {
      uint32_t t0 = plat_millis();
      float fdt = (float)(now - frame_at) / 1000.0f;
      if (fdt > 0.1f) fdt = 0.1f;
      look = car_look_of(&car);
      look.paint = sv.p.paint;
      if (sv.p.view) cockpit_camera(&cam, look.pos, look.rot, car.speed);
      else chase_update(&chase, &cam, fdt, look.pos, look.rot, car.speed);
      ghost_look.ghost = 0;
      v3 gp;
      qt gr;
      if (ui.started && ghost_at(&gplay, car.frames, &gp, &gr)) {
        memset(&ghost_look, 0, sizeof ghost_look);
        ghost_look.pos = gp, ghost_look.rot = gr, ghost_look.ghost = 1, ghost_look.paint = sv.p.paint;
        for (int i = 0; i < 4; i++) ghost_look.susp[i] = 0.078f;
      }
      plat_vblank();
      pt_render(&sv, &car, &cam, &look, ghost_look.ghost ? &ghost_look : NULL, &ui);
      uint32_t spent = plat_millis() - t0;
      perf_frames++;
      if (spent > perf_max) perf_max = spent;
      if (spent > 33) perf_slow++;
      /* the picture's detail follows the time it takes: down at once when slow, up slowly */
      if (ui.screen == V_RACE) {
        if (spent > 30) slow_frames++, quick_frames = 0;
        else if (spent < 18) quick_frames++, slow_frames = 0;
        if (slow_frames >= 2 && rd_detail > 0) rd_detail--, slow_frames = 0;
        if (quick_frames >= 30 && rd_detail < 3) rd_detail++, quick_frames = 0;
      }
      frame_at = now;
      dirty = false;
    } else {
      plat_sleep(moving ? 1 : 10);
    }
  }
  save();
  return plat_end();
}
