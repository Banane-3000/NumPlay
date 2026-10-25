/* The screens (ui.h): the title, the episodes, the levels, a level with its key line, the pause menu, the result,
 * how to play, the story pictures, quitting. Every screen says in its bottom line what the keys do. */
#include <math.h>
#include <string.h>
#include "draw.h"
#include "game.h"
#include "plat.h"
#include "save.h"
#include "ui.h"
#include "world.h"

#define EP_CHROME 5
#define EP_XMAS 7
#define EP_EASTER 8
#define EP_SUMMER 11

static Progress P;
static int screen = S_TITLE, back_to = S_TITLE;
static int sel, episode, level = -1;
static bool dirty = true;
static void *arena;
static int arena_size;
static uint32_t held, edge, now_ms, last_ms, tick_acc, played_ms;
static uint32_t rep_at[8];
static View view, target;
static int look_ms;            /* at the start, the castle is shown first */
static int after_ms;           /* after a shot: the castle stays a while */
static int story = -1, story_ms, story_next = S_PLAY;
static bool result_new_best;
static int result_eagle;       /* the Mighty Eagle's percent in the result, or -1 */

/* ------------------------------------------------------------------ text */
static char *put(char *p, const char *s) {
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

/* ------------------------------------------------------------------ progress */
static bool cleared(int l) { return (P.flags[l] & LV_CLEARED) != 0; }
static bool episode_done(int e) {
  int f = res_episode_first(e), n = res_episode_count(e);
  for (int l = f; l < f + n; l++)
    if (!cleared(l)) return false;
  return true;
}
static bool episode_open(int e) {
  if (e == 1) return episode_done(0);   /* Mighty Hoax after Poached Eggs, Danger Above after it */
  if (e == 2) return episode_done(1);
  return true;
}
static bool level_open(int l) {
  if (P.flags[l] & (LV_OPEN | LV_CLEARED)) return true;
  int e = res_episode_of(l), f = res_episode_first(e);
  if (!episode_open(e)) return false;
  if (e == EP_CHROME) return false;   /* each opened by its golden egg */
  if (e == EP_XMAS) {
    if (l < f + 25) return true;       /* the advent calendar: every day */
    for (int k = f; k < f + 25; k++)
      if (!cleared(k)) return false;
    return true;
  }
  if (e == EP_EASTER && l >= f + 18) return false;   /* the specials: by the hidden eggs */
  if (e == EP_SUMMER && l == f + 28) {
    for (int k = f; k < f + 28; k++)
      if (P.stars[k] < 3) return false;
    return true;
  }
  return l == f || cleared(l - 1);
}
static int episode_stars(int e) {
  int f = res_episode_first(e), n = res_episode_count(e), s = 0;
  for (int l = f; l < f + n; l++) s += P.stars[l];
  return s;
}
static uint32_t played_s(void) { return P.played + played_ms / 1000; }
static bool eagle_ready(void) {
  if (!P.eagle_lock_on || P.eagle_level == level) return true;
  return played_s() - P.eagle_since >= 3600;
}
static int eagle_minutes(void) { return (int)((3600 - (played_s() - P.eagle_since) + 59) / 60); }

bool ui_save(void) {
  P.played += played_ms / 1000;
  played_ms %= 1000;
  return save_write(&P);
}

/* the eggs broken in a try open their levels, won or not */
static void keep_eggs(void) {
  if (g.golden && g.golden_level >= 0) P.flags[g.golden_level] |= LV_OPEN;
  if (g.easter_special >= 0) P.flags[g.easter_special] |= LV_OPEN;
}

/* ------------------------------------------------------------------ keys */
static bool hit(uint32_t k) { return (edge & k) != 0; }
/* a held arrow: once, then again and again (faster the longer it is held); its steps */
static int repeat(int slot, uint32_t k) {
  if (hit(k)) {
    rep_at[slot] = now_ms;
    return 1;
  }
  if (!(held & k)) return 0;
  uint32_t t = now_ms - rep_at[slot];
  if (t < 300) return 0;
  return t > 1200 ? 3 : 1;
}

/* ------------------------------------------------------------------ the view */
static void fit(const float *r, View *v) {
  float w = r[2] - r[0], h = r[3] - r[1];
  if (w < 1) w = 1;
  if (h < 1) h = 1;
  v->zoom = fminf(SCREEN_W / w, SCREEN_H / h);
  v->cx = (r[0] + r[2]) / 2, v->cy = (r[1] + r[3]) / 2;
}

static void camera(int dt) {
  View sling, castle;
  fit(g.cam, &sling);
  fit(g.cam + 4, &castle);
  int follow = g.eagle >= 0 ? g.eagle : g.flying >= 0 ? g.flying : g.follow;
  if (look_ms > 0) look_ms -= dt;
  /* after a shot the castle stays in sight while things break, and a while after */
  static uint32_t last_score;
  if (g.flying >= 0 || g.follow >= 0 || g.score != last_score) after_ms = 1500;
  else if (after_ms > 0) after_ms -= dt;
  last_score = g.score;
  if (g.eagle >= 0) {
    target = castle;
    target.zoom = fminf(sling.zoom, castle.zoom) * 0.8f;
  } else if (follow >= 0 && (g.obj[follow].flags & O_ALIVE)) {
    /* the view goes from the slingshot's to the castle's as the bird does, and up with it when it flies higher */
    float x, y;
    game_object_pos(follow, &x, &y, NULL);
    float t = castle.cx > sling.cx ? (x - sling.cx) / (castle.cx - sling.cx) : 1;
    t = fmaxf(0, fminf(1, t));
    target.cx = sling.cx + (castle.cx - sling.cx) * t;
    target.cy = sling.cy + (castle.cy - sling.cy) * t;
    target.zoom = sling.zoom + (castle.zoom - sling.zoom) * t;
    target.cy = fminf(target.cy, y + SCREEN_H / 2 / target.zoom * 0.7f);
  } else if (look_ms > 0 || after_ms > 0 || (held & KB_SHIFT) || g.state != ST_PLAY) {
    target = castle;
  } else {
    target = sling;
  }
  float k = 0.12f;
  view.cx += (target.cx - view.cx) * k;
  view.cy += (target.cy - view.cy) * k;
  view.zoom += (target.zoom - view.zoom) * k;
}

/* ------------------------------------------------------------------ levels */
static bool start_level(int l) {
  level = l;
  P.last = (uint16_t)l;
  if (!game_load(l, arena, arena_size)) {
    world_end();
    screen = S_NOMEM;
    dirty = true;
    return false;
  }
  View c;
  fit(g.cam + 4, &c);
  view = c;
  look_ms = 1500;
  after_ms = 0;
  tick_acc = 0;
  screen = S_PLAY;
  dirty = true;
  return true;
}

static void open_level(int l) {
  int s = res_story_before(l);
  if (s >= 0 && !cleared(l)) {
    story = s, story_ms = 0, story_next = S_PLAY, level = l;
    screen = S_STORY;
    dirty = true;
    return;
  }
  if (!P.guide) {
    level = l;
    screen = S_GUIDE;
    dirty = true;
    return;
  }
  start_level(l);
}

static void finish_level(void) {
  keep_eggs();
  int l = level;
  result_new_best = false;
  result_eagle = -1;
  if (g.state == ST_WON) {
    if (g.eagle_used) {
      int pct = game_eagle_percent(l, g.score);
      result_eagle = pct;
      if (pct > P.eagle[l]) P.eagle[l] = (uint8_t)pct;
      if (pct >= 100) P.flags[l] |= LV_FEATHER;
    } else {
      int st = game_stars(l, g.score);
      if (g.score > P.best[l]) P.best[l] = g.score, result_new_best = true;
      if (st > P.stars[l]) P.stars[l] = (uint8_t)st;
      P.flags[l] |= LV_CLEARED;
    }
    /* (a win with the Mighty Eagle opens the next level too) */
    int e = res_episode_of(l);
    if (g.eagle_used && l + 1 < res_episode_first(e) + res_episode_count(e) && e != EP_CHROME) P.flags[l + 1] |= LV_OPEN;
  }
  ui_save();
  sel = g.state == ST_WON ? 2 : 0;
  screen = S_RESULT;
  dirty = true;
  int s = res_story_after(l);
  if (g.state == ST_WON && s >= 0 && !g.eagle_used) {
    story = s, story_ms = 0, story_next = S_RESULT;
    screen = S_STORY;
  }
}

static bool next_level_exists(void) {
  int e = res_episode_of(level);
  return level + 1 < res_episode_first(e) + res_episode_count(e) && level_open(level + 1);
}

/* ------------------------------------------------------------------ frame */
void ui_init(void *mem, int size, uint16_t *strip) {
  arena = mem, arena_size = size;
  gfx_buf = strip;
  save_scratch(strip);   /* (the work buffer: free while the game saves) */
  save_load(&P);
  screen = S_TITLE;
  sel = 0;
  dirty = true;
}

int ui_screen(void) { return screen; }
bool ui_dirty(void) { return dirty; }

static void go(int s) {
  screen = s;
  sel = 0;
  dirty = true;
}

static void play_frame(int dt_real) {
  played_ms += (uint32_t)dt_real;
  if (hit(KB_BACK)) {
    go(S_PAUSE);
    return;
  }
  int a = repeat(0, KB_UP) - repeat(1, KB_DOWN), p = repeat(3, KB_RIGHT) - repeat(2, KB_LEFT);
  if (a || p) {
    game_aim(g.angle + a * (PI_F / 180), g.power + p * 0.01f);
    look_ms = 0, after_ms = 0;
  }
  if (hit(KB_OK)) {
    if (game_in_flight()) game_power();
    else if (game_launch()) look_ms = 0;
  }
  if (hit(KB_EAGLE) && !g.eagle_used && eagle_ready() && g.state == ST_PLAY) {
    if (game_eagle_arm()) {
      P.eagle_lock_on = 1, P.eagle_level = (uint16_t)level, P.eagle_since = played_s();
    }
  }
  /* the game's steps for the time that passed (three at most a frame, as the game itself catches up) */
  tick_acc += (uint32_t)dt_real;
  int n = 0;
  while (tick_acc >= STEP_MS && n < 3) {
    tick_acc -= STEP_MS;
    game_step();
    camera(STEP_MS);
    n++;
    if (g.state == ST_WON || g.state == ST_LOST) break;
  }
  if (tick_acc > 3 * STEP_MS) tick_acc = 0;
  if (n) dirty = true;
  /* the physics ran out of memory (a crowded level on a calculator that gives games less): said, not played on */
  if (world_arena_failures()) {
    world_end();
    screen = S_NOMEM;
    dirty = true;
    return;
  }
  if (g.state == ST_WON || g.state == ST_LOST) finish_level();
}

static int grid_move(int s, int count, int cols) {
  if (hit(KB_LEFT) && s > 0) s--;
  if (hit(KB_RIGHT) && s + 1 < count) s++;
  if (hit(KB_UP) && s >= cols) s -= cols;
  if (hit(KB_DOWN) && s + cols < count) s += cols;
  return s;
}

bool ui_frame(uint32_t keys, uint32_t now) {
  edge = keys & ~held;
  held = keys;
  uint32_t dt = last_ms ? now - last_ms : 0;
  if (dt > 200) dt = 200;
  last_ms = now, now_ms = now;
  if (edge) dirty = true;
  if (keys & KB_HOME) {
    if (screen == S_PLAY || screen == S_PAUSE) keep_eggs();
    ui_save();
    return false;
  }
  switch (screen) {
    case S_TITLE:
      sel = (sel + (hit(KB_DOWN) ? 1 : 0) + (hit(KB_UP) ? 1 : 0)) % 2;
      if (hit(KB_OK)) {
        if (sel == 0) go(S_EPISODES), sel = episode;
        else back_to = S_TITLE, go(S_HELP);
      }
      if (hit(KB_BACK)) back_to = S_TITLE, go(S_QUIT);
      break;
    case S_EPISODES:
      sel = grid_move(sel, EPISODE_COUNT, 3);
      if (hit(KB_BACK)) go(S_TITLE);
      else if (hit(KB_OK) && episode_open(sel)) {
        episode = sel;
        int f = res_episode_first(episode), n = res_episode_count(episode), s = 0;
        for (int l = f; l < f + n; l++)
          if (level_open(l)) s = l - f;
        go(S_LEVELS);
        sel = P.last >= f && P.last < f + n ? P.last - f : s;
      }
      break;
    case S_LEVELS: {
      int f = res_episode_first(episode), n = res_episode_count(episode);
      sel = grid_move(sel, n, 5);
      if (hit(KB_BACK)) go(S_EPISODES), sel = episode;
      else if (hit(KB_OK) && level_open(f + sel)) open_level(f + sel);
      break;
    }
    case S_GUIDE:
      if (hit(KB_OK) || hit(KB_BACK)) {
        P.guide = 1;
        start_level(level);
      }
      break;
    case S_STORY: {
      story_ms += (int)dt;
      dirty = true;
      if (hit(KB_OK) || hit(KB_BACK) || story_ms > 16000) {
        if (story_next == S_PLAY) {
          if (!P.guide) go(S_GUIDE);
          else start_level(level);
        } else {
          go(story_next);
          sel = 2;
        }
      }
      break;
    }
    case S_PLAY:
      play_frame((int)dt);
      break;
    case S_PAUSE:
      if (hit(KB_UP)) sel = (sel + 4) % 5;
      if (hit(KB_DOWN)) sel = (sel + 1) % 5;
      if (hit(KB_BACK) || (hit(KB_OK) && sel == 0)) go(S_PLAY);
      else if (hit(KB_OK) && sel == 1) {
        keep_eggs();
        start_level(level);
      } else if (hit(KB_OK) && sel == 2) {
        keep_eggs();
        ui_save();
        world_end();
        go(S_LEVELS);
        sel = level - res_episode_first(episode);
      } else if (hit(KB_OK) && sel == 3) {
        back_to = S_PAUSE, go(S_HELP);
      } else if (hit(KB_OK) && sel == 4) {
        back_to = S_PAUSE, go(S_QUIT);
      }
      break;
    case S_RESULT:
      if (hit(KB_LEFT)) sel = (sel + 2) % 3;
      if (hit(KB_RIGHT)) sel = (sel + 1) % 3;
      if (hit(KB_BACK) || (hit(KB_OK) && sel == 1)) {
        world_end();
        go(S_LEVELS);
        sel = level - res_episode_first(episode);
      } else if (hit(KB_OK) && sel == 0) {
        start_level(level);
      } else if (hit(KB_OK) && sel == 2) {
        if (next_level_exists()) open_level(level + 1);
        else {
          world_end();
          go(S_LEVELS);
          sel = level - res_episode_first(episode);
        }
      }
      break;
    case S_HELP:
      if (hit(KB_OK) || hit(KB_BACK)) go(back_to), sel = back_to == S_PAUSE ? 3 : 1;
      break;
    case S_QUIT:
      if (hit(KB_UP) || hit(KB_DOWN) || hit(KB_LEFT) || hit(KB_RIGHT)) sel ^= 1;
      if (hit(KB_BACK) || (hit(KB_OK) && sel == 0)) go(back_to), sel = back_to == S_PAUSE ? 4 : 0;
      else if (hit(KB_OK) && sel == 1) {
        if (back_to == S_PAUSE) keep_eggs();
        if (ui_save()) return false;
        go(S_SAVE_ERROR);
      }
      break;
    case S_SAVE_ERROR:
      if (hit(KB_OK)) {
        if (ui_save()) return false;
      }
      if (hit(KB_BACK)) return false;
      break;
    case S_NOMEM:
      if (hit(KB_OK) || hit(KB_BACK)) go(S_LEVELS), sel = level - res_episode_first(episode);
      break;
  }
  return true;
}

/* ------------------------------------------------------------------ drawing */
static void key_line(const char *s) {
  gfx_fill(0, SCREEN_H - 18, SCREEN_W, 18, COL_PANEL);
  gfx_text_center(&np_font_small, SCREEN_W / 2, SCREEN_H - 5, s, COL_CREAM, 256);
}

static void button(int x, int y, int w, int h, const char *s, bool on) {
  gfx_rrect(x - 2, y - 2, w + 4, h + 4, 8, on ? COL_FOCUS : COL_INK, 256);
  gfx_rrect(x, y, w, h, 6, on ? HEX(0xFFE68A) : COL_CREAM, 256);
  gfx_text_center(&np_font_body, x + w / 2, y + h / 2 + 5, s, COL_INK, 256);
}

static void star(int cx, int cy, int r, bool on) {
  if (cy + r < gfx_y0 || cy - r >= gfx_y1) return;
  float px[10], py[10];
  for (int i = 0; i < 10; i++) {
    float a = i * PI_F / 5 - PI_F / 2, d = (i & 1) ? r * 0.45f : (float)r;
    px[i] = cx + cosf(a) * d, py[i] = cy + sinf(a) * d;
  }
  color_t fill = on ? HEX(0xFFD02A) : HEX(0x6B5A48), edge_c = on ? HEX(0x7A4A00) : HEX(0x3A2C20);
  for (int y = cy - r; y <= cy + r; y++) {
    if (y < gfx_y0 || y >= gfx_y1) continue;
    for (int x = cx - r; x <= cx + r; x++) {
      bool in = false;
      for (int i = 0, j = 9; i < 10; j = i++)
        if ((py[i] > y + 0.5f) != (py[j] > y + 0.5f) &&
            x + 0.5f < (px[j] - px[i]) * (y + 0.5f - py[i]) / (py[j] - py[i]) + px[i])
          in = !in;
      if (in) gfx_buf[(y - gfx_y0) * SCREEN_W + x] = fill;
    }
  }
  (void)edge_c;
}

static void scene_title(void *ctx) {
  (void)ctx;
  draw_pic(P_MENU_BG, 0, 0);
  const Pic *logo = res_pic(P_LOGO);
  draw_pic(P_LOGO, (SCREEN_W - logo->w) / 2, 14);
  button(100, 120, 120, 30, "Play", sel == 0);
  button(100, 160, 120, 30, "How to play", sel == 1);
  key_line("OK choose  -  Back quit");
}

static void scene_episodes(void *ctx) {
  (void)ctx;
  draw_pic(P_MENU_BG, 0, 0);
  gfx_fill(0, 0, SCREEN_W, 22, COL_PANEL);
  int page = sel / 6;
  char t[40], *p = t;
  p = put(p, "Episodes  ");
  p = num(p, (uint32_t)page + 1);
  p = put(p, " / 2");
  gfx_text_center(&np_font_body, SCREEN_W / 2, 15, t, COL_CREAM, 256);
  for (int k = 0; k < 6; k++) {
    int e = page * 6 + k;
    if (e >= EPISODE_COUNT) break;
    int x = 6 + (k % 3) * 104, y = 28 + (k / 3) * 94;
    bool on = e == sel, open = episode_open(e);
    gfx_rrect(x - 2, y - 2, 100, 90, 8, on ? COL_FOCUS : COL_INK, 256);
    gfx_rrect(x, y, 96, 86, 6, open ? COL_CREAM : HEX(0x9C8E7C), 256);
    draw_pic(res_episode_pic(e), x + 16, y + 2);
    gfx_text_center(&np_font_small, x + 48, y + 72, episode_names[e], COL_INK, 256);
    p = t;
    p = num(p, (uint32_t)episode_stars(e));
    p = put(p, " / ");
    num(p, (uint32_t)res_episode_count(e) * 3);
    star(x + 26, y + 79, 4, true);
    gfx_text(&np_font_small, x + 33, y + 83, t, COL_INK, 256);
    if (!open) draw_pic(P_LEVELSELECTION_LOCK, x + 39, y + 22);
  }
  key_line("Arrows choose  -  OK open  -  Back title");
}

static void scene_levels(void *ctx) {
  (void)ctx;
  draw_pic(P_MENU_BG, 0, 0);
  gfx_fill(0, 0, SCREEN_W, 22, COL_PANEL);
  int f = res_episode_first(episode), n = res_episode_count(episode), page = sel / 15, pages = (n + 14) / 15;
  char t[48], *p = t;
  p = put(p, episode_names[episode]);
  if (pages > 1) {
    p = put(p, "  ");
    p = num(p, (uint32_t)page + 1);
    p = put(p, " / ");
    num(p, (uint32_t)pages);
  }
  gfx_text_center(&np_font_body, SCREEN_W / 2, 15, t, COL_CREAM, 256);
  for (int k = 0; k < 15; k++) {
    int i = page * 15 + k;
    if (i >= n) break;
    int l = f + i, x = 12 + (k % 5) * 61, y = 28 + (k / 5) * 54;
    bool on = i == sel, open = level_open(l);
    gfx_rrect(x - 2, y - 2, 56, 50, 8, on ? COL_FOCUS : COL_INK, 256);
    gfx_rrect(x, y, 52, 46, 6, open ? COL_CREAM : HEX(0x9C8E7C), 256);
    if (open) {
      p = num(t, (uint32_t)i + 1);
      gfx_text_center(&np_font_body, x + 26, y + 20, t, COL_INK, 256);
      for (int s = 0; s < 3; s++) star(x + 12 + s * 14, y + 34, 6, P.stars[l] > s);
      if (P.flags[l] & LV_FEATHER) draw_pic(P_ASSET_FEATHER, x + 40, y + 2);
    } else {
      draw_pic(P_LEVELSELECTION_LOCK, x + 17, y + 12);
    }
  }
  int l = f + sel;
  gfx_fill(0, SCREEN_H - 40, SCREEN_W, 22, COL_PANEL_LIGHT);
  p = t;
  if (level_open(l)) {
    p = put(p, "Best ");
    p = num(p, P.best[l]);
    if (P.eagle[l]) {
      p = put(p, "    Mighty Eagle ");
      p = num(p, P.eagle[l]);
      put(p, "%");
    }
  } else {
    put(p, episode == EP_CHROME ? "A golden egg opens it" : "Locked");
  }
  gfx_text_center(&np_font_small, SCREEN_W / 2, SCREEN_H - 25, t, COL_CREAM, 256);
  key_line("Arrows choose  -  OK play  -  Back episodes");
}

static void hud(void) {
  char t[40], *p = t;
  p = put(t, "Score ");
  num(p, g.score);
  gfx_text_right(&np_font_body, SCREEN_W - 6, 15, t, 0, 140);
  gfx_text_right(&np_font_body, SCREEN_W - 7, 14, t, COL_WHITE, 256);
  p = put(t, episode_names[episode]);
  p = put(p, " ");
  num(p, (uint32_t)(level - res_episode_first(res_episode_of(level)) + 1));
  gfx_text(&np_font_small, 7, 13, t, 0, 140);
  gfx_text(&np_font_small, 6, 12, t, COL_WHITE, 256);
  if (!g.eagle_used && g.state == ST_PLAY) {
    draw_pic(P_BIRD_SARDINE, 6, 18);
    p = t;
    if (eagle_ready()) put(p, "1 Mighty Eagle");
    else {
      p = num(p, (uint32_t)eagle_minutes());
      put(p, " min");
    }
    gfx_text(&np_font_small, 29, 30, t, COL_WHITE, 256);
  }
  if (g.state == ST_BONUS || g.state == ST_WON) return;
  if (game_in_flight()) {
    key_line(g.obj[g.flying].cls == C_RED || g.obj[g.flying].cls == C_REDBIG || g.obj[g.flying].cls == C_SARDINE
                 ? "Back pause"
                 : "OK power  -  Back pause");
  } else if (game_can_launch()) {
    p = put(t, "");
    p = num(p, (uint32_t)lroundf(g.angle * 180 / PI_F < 0 ? -g.angle * 180 / PI_F : g.angle * 180 / PI_F));
    p = put(p, g.angle < 0 ? " down  " : " up  ");
    p = num(p, (uint32_t)lroundf(g.power * 100));
    put(p, "%");
    gfx_rrect(4, SCREEN_H - 36, gfx_text_width(&np_font_small, t) + 10, 16, 5, COL_PANEL, 200);
    gfx_text(&np_font_small, 9, SCREEN_H - 24, t, COL_CREAM, 256);
    key_line("OK fire  -  Arrows aim  -  Shift look  -  Back pause");
  } else {
    key_line("Shift look  -  Back pause");
  }
}

static void scene_play(void *ctx) {
  (void)ctx;
  draw_world(&view);
  hud();
}

static void panel(int x, int y, int w, int h) {
  gfx_rrect(x - 3, y - 3, w + 6, h + 6, 12, COL_INK, 230);
  gfx_rrect(x, y, w, h, 10, COL_PANEL, 245);
}

static void scene_pause(void *ctx) {
  (void)ctx;
  draw_world(&view);
  gfx_fill_alpha(0, 0, SCREEN_W, SCREEN_H, 0, 110);
  panel(70, 18, 180, 196);
  gfx_text_center(&np_font_title, SCREEN_W / 2, 44, "Paused", COL_CREAM, 256);
  static const char *const items[5] = {"Resume", "Restart", "Levels", "How to play", "Quit game"};
  for (int i = 0; i < 5; i++) button(92, 56 + i * 31, 136, 24, items[i], sel == i);
  key_line("Up Down choose  -  OK  -  Back resume");
}

static void scene_result(void *ctx) {
  (void)ctx;
  draw_world(&view);
  gfx_fill_alpha(0, 0, SCREEN_W, SCREEN_H, 0, 110);
  panel(40, 14, 240, 202);
  bool won = g.state == ST_WON;
  char t[40], *p;
  if (result_eagle >= 0) {
    gfx_text_center(&np_font_title, SCREEN_W / 2, 42, "Mighty Eagle", COL_CREAM, 256);
    p = num(t, (uint32_t)result_eagle);
    put(p, "% destruction");
    gfx_text_center(&np_font_body, SCREEN_W / 2, 80, t, COL_FOCUS, 256);
    if (result_eagle >= 100) draw_pic(P_LEVELEND_FEATHER, SCREEN_W / 2 - 15, 88);
  } else {
    gfx_text_center(&np_font_title, SCREEN_W / 2, 42, won ? "Level cleared!" : "Level failed", COL_CREAM, 256);
    int st = won ? game_stars(level, g.score) : 0;
    const Pic *sp = res_pic(P_LEVELOVERWIN_STARS_BIG_0 + st);
    draw_pic(P_LEVELOVERWIN_STARS_BIG_0 + st, (SCREEN_W - sp->w) / 2, 52);
  }
  p = put(t, "Score ");
  num(p, g.score);
  gfx_text_center(&np_font_body, SCREEN_W / 2, 132, t, COL_CREAM, 256);
  p = put(t, result_new_best ? "New best!" : "Best ");
  if (!result_new_best) num(p, P.best[level]);
  gfx_text_center(&np_font_small, SCREEN_W / 2, 150, t, result_new_best ? COL_FOCUS : COL_MUTED, 256);
  static const int pics[3] = {P_LEVELOVERWIN_BUTTON_RESTART, P_LEVELOVERWIN_BUTTON_MENU, P_LEVELOVERWIN_BUTTON_NEXTLEVEL};
  static const char *const names[3] = {"Restart", "Levels", "Next"};
  for (int i = 0; i < 3; i++) {
    int x = 76 + i * 64, y = 160;
    if (sel == i) gfx_rrect(x - 5, y - 4, 44, 52, 8, COL_FOCUS, 256);
    draw_pic(pics[i], x, y);
    gfx_text_center(&np_font_small, x + 17, y + 46, names[i], sel == i ? COL_INK : COL_CREAM, 256);
  }
  key_line("Left Right choose  -  OK");
}

static const char *const HELP[] = {
  "Knock out every pig with the birds.",
  "Up and Down aim, Left and Right pull.",
  "OK fires. OK again in flight: its power.",
  "Yellow speeds up, blue splits in three,",
  "black explodes, white drops an egg,",
  "green turns back like a boomerang.",
  "Birds you don't use: 10000 points each.",
  "1: the Mighty Eagle clears the level,",
  "once an hour of play (more on that level).",
  "Shift looks at the pigs. Back pauses.",
};

static void scene_help(void *ctx) {
  (void)ctx;
  draw_pic(P_MENU_BG, 0, 0);
  panel(10, 12, 300, 204);
  gfx_text_center(&np_font_title, SCREEN_W / 2, 40, "How to play", COL_CREAM, 256);
  for (unsigned i = 0; i < sizeof HELP / sizeof *HELP; i++)
    gfx_text(&np_font_small, 22, 62 + (int)i * 15, HELP[i], COL_CREAM, 256);
  key_line("OK back");
}

static void scene_guide(void *ctx) {
  (void)ctx;
  draw_pic(P_MENU_BG, 0, 0);
  panel(20, 30, 280, 170);
  gfx_text_center(&np_font_title, SCREEN_W / 2, 56, "Ready?", COL_CREAM, 256);
  gfx_text_center(&np_font_body, SCREEN_W / 2, 84, "Up and Down aim the slingshot,", COL_CREAM, 256);
  gfx_text_center(&np_font_body, SCREEN_W / 2, 102, "Left and Right pull it.", COL_CREAM, 256);
  gfx_text_center(&np_font_body, SCREEN_W / 2, 128, "OK fires the bird.", COL_FOCUS, 256);
  gfx_text_center(&np_font_body, SCREEN_W / 2, 146, "OK again while it flies:", COL_CREAM, 256);
  gfx_text_center(&np_font_body, SCREEN_W / 2, 164, "its power.", COL_CREAM, 256);
  gfx_text_center(&np_font_small, SCREEN_W / 2, 188, "Back pauses. Home saves and quits.", COL_MUTED, 256);
  key_line("OK start");
}

static void scene_story(void *ctx) {
  (void)ctx;
  gfx_fill(0, gfx_y0, SCREEN_W, gfx_y1 - gfx_y0, 0);
  int pic = res_story_pic(story);
  const Pic *p = res_pic(pic);
  /* the picture slides across, as the game's comic does */
  float k = story_ms < 2500 ? 0 : story_ms > 10500 ? 1 : (story_ms - 2500) / 8000.0f;
  int x = 0, y = (SCREEN_H - 18 - p->h) / 2;
  if (p->w > SCREEN_W) x = -(int)lroundf((p->w - SCREEN_W) * k);
  else x = (SCREEN_W - p->w) / 2;
  if (p->h > SCREEN_H - 18) y = -(int)lroundf((p->h - (SCREEN_H - 18)) * k);
  draw_pic(pic, x, y);
  key_line("OK continue");
}

static void scene_quit(void *ctx) {
  (void)ctx;
  if (back_to == S_PAUSE) {
    draw_world(&view);
    gfx_fill_alpha(0, 0, SCREEN_W, SCREEN_H, 0, 110);
  } else {
    draw_pic(P_MENU_BG, 0, 0);
  }
  panel(50, 50, 220, 130);
  gfx_text_center(&np_font_title, SCREEN_W / 2, 80, "Quit game?", COL_CREAM, 256);
  button(80, 96, 160, 28, "Keep playing", sel == 0);
  button(80, 134, 160, 28, "Quit", sel == 1);
  key_line("Up Down choose  -  OK");
}

static void scene_message(void *ctx) {
  (void)ctx;
  draw_pic(P_MENU_BG, 0, 0);
  panel(20, 50, 280, 130);
  if (screen == S_SAVE_ERROR) {
    gfx_text_center(&np_font_title, SCREEN_W / 2, 82, "Storage is full", COL_CREAM, 256);
    gfx_text_center(&np_font_body, SCREEN_W / 2, 112, "Delete a file to save your progress.", COL_CREAM, 256);
    key_line("OK try again  -  Back quit without saving");
  } else {
    gfx_text_center(&np_font_title, SCREEN_W / 2, 82, "Too big", COL_CREAM, 256);
    gfx_text_center(&np_font_body, SCREEN_W / 2, 112, "This level needs more memory", COL_CREAM, 256);
    gfx_text_center(&np_font_body, SCREEN_W / 2, 130, "than this calculator gives games.", COL_CREAM, 256);
    key_line("OK back");
  }
}

void ui_draw(void) {
  static const gfx_scene_t scenes[S_COUNT] = {
    scene_title, scene_episodes, scene_levels, scene_story, scene_play, scene_pause, scene_result, scene_help,
    scene_quit, scene_message, scene_guide, scene_message,
  };
  gfx_render(scenes[screen], NULL, 0, SCREEN_H);
  dirty = false;
}
