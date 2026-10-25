/* Turbo Rush's screens: the title over a race of rivals, the race screen and the garage, the race's dials, and the
 * pause, results and standings over the race. Each has a line of plain keys at the bottom. */
#include <string.h>
#include "../../../launcher/src/gfx.h"
#include "draw.h"
#include "view.h"

#define WHITE 0xFFFF
#define INK HEX(0x0E1A26)
#define PANEL HEX(0x13212F)
#define GOLD HEX(0xFFD052)
#define MUTED HEX(0xA9BDC6)
#define DIM HEX(0x5F7480)
#define RED HEX(0xFF6F55)

/* the launcher's drawing (gfx.c) draws a strip of the screen at a time, and sends it here */
void plat_push(int x, int y, int w, int h, const uint16_t *px);
void np_push(int x, int y, int w, int h, const color_t *px) { plat_push(x, y, w, h, px); }
static color_t strip[SCREEN_W * STRIP_H];

const char *const tr_driver[TR_CARS] = {"You", "Rosa", "Kenji", "Marco", "Ines", "Theo", "Ava", "Luca"};
static const char *const MODES[3] = {"Grand Prix", "Single race", "Free Ride"};
static const char *const LEVELS[3] = {"Easy", "Normal", "Expert"};
static const char *const TRAFFIC[3] = {"Light", "Normal", "Busy"};
static const char *const MOODS[3] = {"Day", "Sunset", "Night"};
static const char *const UPGRADES[4] = {"Engine", "Tires", "Nitro", "Armor"};
static const char *const BODIES[2] = {"Coupe", "Supercar"};

static const tr_file *F;
static const tr_track *T;
static const tr_race *R, *RP;
static const tr_ui *U;
static int A;

/* ------------------------------------------------------------------ text */
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
static char *clock_of(char *p, uint32_t ms) {
  p = num(p, ms / 60000);
  *p++ = ':', *p++ = (char)('0' + ms / 10000 % 6), *p++ = (char)('0' + ms / 1000 % 10), *p++ = '.';
  *p++ = (char)('0' + ms / 100 % 10), *p = 0;
  return p;
}
static char *ordinal(char *p, int n) {
  p = num(p, (uint32_t)n);
  return cat(p, n == 1 ? "st" : n == 2 ? "nd" : n == 3 ? "rd" : "th");
}
static char *km(char *p, uint32_t fx) {
  uint32_t m = fx / FX;
  p = num(p, m / 1000);
  *p++ = '.', *p++ = (char)('0' + m / 100 % 10);
  return cat(p, " km");
}

static void text(const np_font_t *f, int x, int y, const char *s, color_t c) { gfx_text(f, x, y, s, c, 256); }
static void center(const np_font_t *f, int x, int y, const char *s, color_t c) { gfx_text_center(f, x, y, s, c, 256); }
static void right(const np_font_t *f, int x, int y, const char *s, color_t c) { gfx_text_right(f, x, y, s, c, 256); }

/* ------------------------------------------------------------------ pieces */
/* a line of keys: "Up Down=choose|OK=open", each key in gold, what it does after it */
static void key_line(int y, const char *s) {
  char buf[48];
  int w = 0;
  for (int pass = 0; pass < 2; pass++) {
    int x = 160 - w / 2;
    w = 0;
    const char *q = s;
    while (*q) {
      int k = 0;
      while (q[k] && q[k] != '=' && q[k] != '|' && k < (int)sizeof buf - 1) k++;
      memcpy(buf, q, (size_t)k), buf[k] = 0;
      bool key = q[k] == '=';
      int tw = gfx_text_width(&np_font_small, buf);
      if (pass) text(&np_font_small, x + w, y, buf, key ? GOLD : MUTED);
      w += tw + (key ? 4 : 0);
      q += k;
      if (*q == '=') q++;
      else if (*q == '|') q++, w += 11;
    }
  }
}

static void keys(const char *s) {
  gfx_fill(0, 222, 320, 18, INK);
  gfx_fill(0, 222, 320, 1, HEX(0x2A3D4C));
  key_line(235, s);
}

static void page(const char *title) {
  gfx_vgrad(0, 0, 320, 222, 0x0E1C29, 0x1D3747);
  text(&np_font_title, 14, 30, title, WHITE);
  char s[24];
  cat(num(s, F->p.credits), " credits");
  right(&np_font_small, 306, 28, s, GOLD);
}

static void button(int x, int y, int w, const char *s, bool on) {
  gfx_rrect(x, y, w, 26, 6, on ? GOLD : PANEL, 256);
  if (!on) gfx_rrect_outline(x, y, w, 26, 6, 1, HEX(0x2E4556), 256);
  center(&np_font_body, x + w / 2, y + 18, s, on ? INK : WHITE);
}

static void dim(void) { gfx_fill_alpha(0, 0, 320, 240, HEX(0x050A10), 150); }

/* when the calculator's storage is full, say so above the keys */
static void not_saved(void) {
  if (!U->save_failed) return;
  gfx_rrect(40, 204, 240, 17, 5, HEX(0x5A1E1A), 240);
  center(&np_font_small, 160, 216, "Storage full: progress not saved", WHITE);
}

static void bar(int x, int y, int w, int h, int v, int max, color_t c) {
  gfx_rrect(x, y, w, h, h / 2, HEX(0x24384A), 256);
  int f = v * w / max;
  if (f > 0) gfx_rrect(x, y, f < h ? h : f, h, h / 2, c, 256);
}

static void logo(int y) {
  const np_font_t *f = &np_font_title;
  const char *s = "TURBO RUSH";
  int w = gfx_text_width(f, s) * 2, x0 = (320 - w) / 2;
  for (int pass = 0; pass < 2; pass++) {
    int x = x0 + (pass ? 0 : 2), base = y + (pass ? 0 : 2);
    color_t c = pass ? GOLD : HEX(0x7A2A22);
    for (const char *q = s; *q; q++) {
      const np_glyph_t *g = &f->glyphs[(uint8_t)*q - f->first];
      const uint8_t *m = f->data + g->offset;
      for (int gy = 0; gy < g->h; gy++) {
        int yy = base + (g->y + gy) * 2;
        if (yy + 2 <= gfx_y0 || yy >= gfx_y1) continue;
        for (int gx = 0; gx < g->w; gx++) {
          int k = gy * g->w + gx, cov = (m[k / 2] >> ((k & 1) * 4)) & 15;
          /* a little slant: the top leans right */
          int lean = (g->h - gy) / 3;
          if (cov) gfx_fill_alpha(x + (g->x + gx) * 2 + lean, yy, 2, 2, c, cov * 17);
        }
      }
      x += g->advance * 2;
    }
  }
}

/* a circuit's outline: a soft edge, a light line, the start line */
static void outline(int x, int y, const int16_t *mx, const int16_t *my) {
  for (int pass = 0; pass < 2; pass++)
    for (int i = 0; i < VIEW_MAP; i++) {
      int j = (i + 1) % VIEW_MAP, x0 = x + mx[i], y0 = y + my[i], x1 = x + mx[j], y1 = y + my[j];
      int n = NP_MAX(NP_MAX(x1 - x0, x0 - x1), NP_MAX(y1 - y0, y0 - y1));
      for (int k = 0; k <= n; k++) {
        int px = x0 + (n ? (x1 - x0) * k / n : 0), py = y0 + (n ? (y1 - y0) * k / n : 0);
        if (pass) gfx_fill(px, py, 1, 1, HEX(0xE6ECEF));
        else gfx_fill(px - 1, py - 1, 3, 3, HEX(0x8195A0));
      }
    }
  gfx_fill(x + mx[0] - 1, y + my[0] - 3, 2, 6, WHITE);
}

static void map(int x, int y, const tr_race *r, bool dots) {
  outline(x, y, view_map_x, view_map_y);
  int sx, sy;
  if (!dots || !r) return;
  for (int i = TR_CARS - 1; i >= 0; i--) {
    if (i && i > tr_traffic(r)) continue;
    view_map_at(T, r->car[i].z, &sx, &sy);
    if (i) gfx_circle(x + sx, y + sy, 2, view_paint(r->car[i].paint), 256);
  }
  view_map_at(T, r->car[0].z, &sx, &sy);
  gfx_circle(x + sx, y + sy, 4, WHITE, 256);
  gfx_circle(x + sx, y + sy, 3, view_paint(r->car[0].paint), 256);
}

/* ------------------------------------------------------------------ the race's dials */
static void hud(void) {
  const tr_race *r = R;
  char s[40], *p;
  bool free = r->mode == MODE_FREE;
  /* place and lap, or distance and cars passed */
  gfx_rrect(6, 6, 98, 40, 6, INK, 215);
  if (free) {
    text(&np_font_small, 14, 19, "DISTANCE", MUTED);
    km(s, r->odometer);
    text(&np_font_body, 14, 38, s, WHITE);
  } else {
    text(&np_font_small, 14, 19, "POS", MUTED);
    text(&np_font_small, 62, 19, "LAP", MUTED);
    num(s, (uint32_t)tr_rank(r, 0));
    int w = gfx_text(&np_font_title, 14, 40, s, WHITE, 256);
    text(&np_font_body, 14 + w + 1, 40, "/8", MUTED);
    p = num(s, r->lap);
    cat(cat(p, "/"), "3");
    text(&np_font_body, 62, 40, s, WHITE);
  }
  /* the clock */
  gfx_rrect(112, 6, 96, free ? 26 : 40, 6, INK, 215);
  if (free) {
    p = cat(s, "Passed ");
    num(p, r->passes);
    center(&np_font_body, 160, 24, s, WHITE);
  } else {
    clock_of(s, r->time_ms);
    center(&np_font_body, 160, 23, s, WHITE);
    if (r->best_lap_ms) {
      p = cat(s, "BEST ");
      clock_of(p, r->best_lap_ms);
      center(&np_font_small, 160, 38, s, MUTED);
    } else {
      center(&np_font_small, 160, 38, tr_track_name[r->track], MUTED);
    }
  }
  /* the map */
  gfx_rrect(232, 6, 82, 62, 6, INK, 200);
  map(237, 11, r, true);
  /* the dials at the bottom: speed | gear | nitro | damage, then the keys */
  gfx_fill_alpha(0, 202, 320, 25, INK, 230);
  gfx_fill(0, 202, 320, 1, HEX(0x2E4556));
  gfx_fill(0, 227, 320, 13, HEX(0x08111A));
  for (int k = 0; k < 3; k++) gfx_fill((int[]){88, 124, 218}[k], 207, 1, 16, HEX(0x2E4556));
  num(s, (uint32_t)tr_kmh(r->car[0].v));
  right(&np_font_title, 52, 224, s, WHITE);
  text(&np_font_small, 56, 224, "km/h", MUTED);
  text(&np_font_small, 94, 213, "GEAR", MUTED);
  s[0] = (char)('0' + r->gear), s[1] = 0;
  center(&np_font_body, 106, 225, s, r->manual ? GOLD : WHITE);
  text(&np_font_small, 130, 213, "NITRO", MUTED);
  if (r->nitro_lock) right(&np_font_small, 212, 213, "refilling", DIM);
  bar(130, 216, 82, 7, r->nitro, 1000, r->boost ? HEX(0x9BE7FF) : r->nitro_lock ? HEX(0x47708C) : HEX(0x3FB6F0));
  if (free) {
    text(&np_font_small, 224, 213, "TRAFFIC", MUTED);
    center(&np_font_body, 268, 225, TRAFFIC[r->level], WHITE);
  } else {
    text(&np_font_small, 224, 213, "DAMAGE", MUTED);
    p = num(s, r->damage / 10u);
    cat(p, "%");
    right(&np_font_small, 312, 213, s, r->impact ? RED : WHITE);
    bar(224, 216, 88, 7, r->damage, 1000, r->damage > 700 ? RED : HEX(0xF0A94E));
  }
  key_line(237, r->manual ? "Up=gas|Down=brake|OK=nitro|+ -=gears|Back=pause" : "Up=gas|Down=brake|OK=nitro|Back=pause");
  /* the start, a lap, the finish */
  if (r->phase == PH_GRID || (U->hold && U->screen == V_RACE)) {
    int n = r->phase == PH_GRID ? (r->countdown + TR_HZ - 1) / TR_HZ : U->hold;
    gfx_rrect(110, 54, 100, 48, 8, INK, 225);
    if (n > 3) {
      center(&np_font_body, 160, 74, tr_track_name[r->track], WHITE);
      center(&np_font_small, 160, 92, "Get ready", MUTED);
    } else {
      s[0] = (char)('0' + n), s[1] = 0;
      center(&np_font_title, 160, 86, s, GOLD);
    }
  } else if (!free && r->phase == PH_RACE && r->time_ms < 1000) {
    gfx_rrect(110, 54, 100, 48, 8, INK, 225);
    center(&np_font_title, 160, 86, "GO!", GOLD);
  } else if (r->banner_t || r->phase == PH_DONE) {
    int b = r->banner_t ? r->banner : r->end == END_FINISH ? BANNER_FINISH : r->end == END_WRECK ? BANNER_WRECK : BANNER_TIME;
    const char *big = "";
    char t[24];
    t[0] = 0;
    if (b == BANNER_LAP || b == BANNER_FINAL) {
      if (b == BANNER_FINAL) big = "Final lap";
      else p = cat(s, "Lap "), num(p, r->banner_arg), big = s;
      clock_of(t, r->last_lap_ms);
    } else if (b == BANNER_FINISH) {
      big = "Finish!";
      ordinal(t, r->car[0].place);
    } else if (b == BANNER_WRECK) {
      big = "Wrecked";
      cat(t, "Too much damage");
    } else {
      big = "Time up";
    }
    gfx_rrect(90, 54, 140, t[0] ? 54 : 40, 8, INK, 225);
    center(&np_font_title, 160, 82, big, b == BANNER_WRECK || b == BANNER_TIME ? RED : GOLD);
    if (t[0]) center(&np_font_body, 160, 101, t, WHITE);
  }
}

/* ------------------------------------------------------------------ screens */
static void title(void) {
  gfx_fill_alpha(0, 0, 320, 240, HEX(0x050A10), 70);
  for (int y = 0; y < 84; y += 4) gfx_fill_alpha(0, y, 320, 4, HEX(0x050A10), 150 - y * 150 / 84);
  logo(44);
  center(&np_font_small, 160, 64, "A game by Bobydeluxe", WHITE);
  static const char *const NAMES[5] = {"Continue", "Play", "Garage", "Records", "How to play"};
  int n = U->menu_n, y0 = 132 - n * 15;
  for (int i = 0; i < n; i++) {
    int item = U->menu[i];
    const char *s = NAMES[item];
    if (item == MENU_CONTINUE)
      s = !F->has_race ? "Next Grand Prix race" : F->race.mode == MODE_GP ? "Continue Grand Prix"
                       : F->race.mode == MODE_FREE ? "Continue ride" : "Continue race";
    button(70, y0 + i * 30, 180, s, U->sel == i);
  }
  not_saved();
  keys("Up Down=choose|OK=open|Back=quit");
}

static void setup(void) {
  page("RACE");
  const tr_profile *p = &F->p;
  static const char *const NAMES[SET_ROWS] = {"Mode", "Circuit", "Rivals", "Time of day", "Weather", "Gearbox", ""};
  for (int i = 0; i < SET_START; i++) {
    int y = 40 + i * 24;
    bool on = U->sel == i, off = i == SET_TRACK && p->mode == MODE_GP;
    gfx_rrect(10, y, 186, 21, 5, on ? GOLD : PANEL, 256);
    color_t c = on ? INK : off ? DIM : WHITE;
    const char *name = i == SET_RIVALS && p->mode == MODE_FREE ? "Traffic" : NAMES[i];
    text(&np_font_small, 18, y + 15, name, c);
    const char *v = "";
    switch (i) {
      case SET_MODE: v = MODES[p->mode]; break;
      case SET_TRACK: v = p->mode == MODE_GP ? "All six" : tr_track_name[p->track]; break;
      case SET_RIVALS: v = p->mode == MODE_FREE ? TRAFFIC[p->traffic] : LEVELS[p->level]; break;
      case SET_TIME: v = MOODS[p->mood]; break;
      case SET_WEATHER: v = p->rain ? "Rain" : "Clear"; break;
      case SET_GEARS: v = p->manual ? "Manual" : "Automatic"; break;
    }
    right(&np_font_small, off ? 188 : 180, y + 15, v, c);
    if (!off) text(&np_font_small, 184, y + 15, ">", on ? INK : DIM);
  }
  /* the circuit */
  gfx_rrect(204, 40, 106, 141, 6, PANEL, 256);
  int track = p->mode == MODE_GP ? 0 : p->track;
  static int16_t mx[VIEW_MAP], my[VIEW_MAP];
  static int shown = -1;
  if (shown != track) tr_map(track, VIEW_MAP, mx, my, 72, 52), shown = track;
  center(&np_font_small, 257, 56, p->mode == MODE_GP ? "First race" : "Circuit", MUTED);
  outline(222, 66, mx, my);
  center(&np_font_small, 257, 136, tr_track_name[track], WHITE);
  center(&np_font_small, 257, 151, tr_track_note[track], MUTED);
  char s[24];
  if (F->p.best_ms[track]) {
    clock_of(cat(s, "Best "), F->p.best_ms[track]);
    center(&np_font_small, 257, 170, s, GOLD);
  }
  const char *go = p->mode == MODE_GP ? "Start Grand Prix" : p->mode == MODE_FREE ? "Start Free Ride" : "Start race";
  button(10, 190, 300, go, U->sel == SET_START);
  keys(U->sel == SET_START ? "OK=start|Up Down=choose|Back=menu" : "Left Right=change|Up Down=choose|Back=menu");
}

static void confirm_box(const char *q, const char *note, int n, const char *const *labels) {
  dim();
  int h = 70 + n * 32;
  gfx_rrect(40, 120 - h / 2, 240, h, 8, INK, 256);
  center(&np_font_body, 160, 120 - h / 2 + 28, q, WHITE);
  if (note) center(&np_font_small, 160, 120 - h / 2 + 46, note, MUTED);
  for (int i = 0; i < n; i++) button(60, 120 - h / 2 + 58 + i * 32, 200, labels[i], U->sel == i);
}

static void help_page(int k, bool guide) {
  page(guide ? "BEFORE YOU RACE" : "HOW TO PLAY");
  static const char *const TITLES[TR_HELP_PAGES] = {"Keys", "Racing", "Grand Prix and garage"};
  center(&np_font_body, 160, 56, TITLES[k], GOLD);
  if (k == 0) {
    static const char *const K[7][2] = {{"Up", "Accelerate"}, {"Down", "Brake"}, {"Left Right", "Steer"},
                                        {"OK or EXE", "Nitro"}, {"+ and -", "Gears, if manual"},
                                        {"Back", "Pause"}, {"Home", "Save and quit"}};
    for (int i = 0; i < 7; i++) {
      int y = 66 + i * 20;
      gfx_rrect(40, y, 240, 18, 4, PANEL, 256);
      text(&np_font_small, 50, y + 13, K[i][0], GOLD);
      text(&np_font_small, 140, y + 13, K[i][1], WHITE);
    }
  } else {
    static const char *const L[2][6] = {
        {"Three laps against seven rivals.", "Brake before the sharp bends,", "or the bend pulls you off the road.",
         "Grass slows you down. Trees, lamps", "and buildings stop you and hurt the car.", "At 100% damage the race is over."},
        {"Six circuits, one after the other:", "points 15 12 10 8 6 4 2 1.", "Every race pays credits:",
         "spend them in the garage.", "Free Ride: just you and the traffic,", "as long as you like."}};
    for (int i = 0; i < 6; i++) center(&np_font_small, 160, 84 + i * 21, L[k - 1][i], WHITE);
  }
  for (int i = 0; i < TR_HELP_PAGES; i++) gfx_circle(148 + i * 12, 212, 3, i == k ? GOLD : DIM, 256);
  keys(guide ? "OK=start the race|Left Right=page|Back=return" : "Left Right=page|Back=return");
}

static void garage(void) {
  page("GARAGE");
  const tr_profile *p = &F->p;
  gfx_vgrad(10, 38, 300, 58, 0x1A2E3C, 0x253F4F);
  gfx_fill(10, 88, 300, 8, HEX(0x1A2A35));
  view_car(160, 92, 96, p->paint, p->style, 1);
  char s[40], *q;
  for (int i = 0; i < 4; i++) {
    int y = 112 + i * 18;
    bool on = U->sel == i;
    gfx_rrect(10, y, 300, 16, 4, on ? GOLD : PANEL, 256);
    color_t c = on ? INK : WHITE;
    text(&np_font_small, 18, y + 12, UPGRADES[i], c);
    for (int k = 0; k < 4; k++)
      gfx_rrect(96 + k * 16, y + 5, 12, 7, 2, k < p->up[i] ? (on ? INK : GOLD) : (on ? HEX(0xB8952F) : HEX(0x3A5060)), 256);
    unsigned cost = tr_price(p, i);
    if (cost) cat(num(s, cost), " credits");
    else cat(s, "Full");
    right(&np_font_small, 302, y + 12, s, on ? INK : cost && cost > p->credits ? DIM : GOLD);
  }
  int y = 184;
  gfx_rrect(10, y, 300, 16, 4, U->sel == GAR_PAINT ? GOLD : PANEL, 256);
  text(&np_font_small, 18, y + 12, "Paint", U->sel == GAR_PAINT ? INK : WHITE);
  for (int i = 0; i < 8; i++) {
    int x = 96 + i * 26;
    if (i == p->paint) gfx_rrect(x - 2, y + 1, 22, 14, 3, U->sel == GAR_PAINT ? INK : WHITE, 256);
    gfx_rrect(x, y + 3, 18, 10, 2, view_paint(i), 256);
  }
  y = 202;
  gfx_rrect(10, y, 300, 16, 4, U->sel == GAR_BODY ? GOLD : PANEL, 256);
  text(&np_font_small, 18, y + 12, "Body", U->sel == GAR_BODY ? INK : WHITE);
  q = cat(s, "< ");
  q = cat(q, BODIES[p->style]);
  cat(q, " >");
  right(&np_font_small, 302, y + 12, s, U->sel == GAR_BODY ? INK : WHITE);
  /* what the chosen row does, or the garage's last word */
  const char *line = U->note;
  char d[48];
  if (!line[0]) {
    int i = U->sel, lv = i < 4 ? p->up[i] : 0;
    q = d;
    switch (i) {
      case GAR_ENGINE:
        q = cat(q, "Top speed ");
        q = num(q, (uint32_t)((61 + 3 * lv) * 36 / 10));
        if (lv < 4) q = cat(q, " > "), q = num(q, (uint32_t)((64 + 3 * lv) * 36 / 10));
        cat(q, " km/h");
        break;
      case GAR_TIRES:
        q = cat(q, "Grip in bends ");
        q = num(q, (uint32_t)(100 + 6 * lv));
        if (lv < 4) q = cat(q, "% > "), q = num(q, (uint32_t)(106 + 6 * lv));
        cat(q, "%");
        break;
      case GAR_NITRO:
        q = cat(q, "Refills in ");
        q = num(q, (uint32_t)(1000 / (2 + lv) / TR_HZ));
        if (lv < 4) q = cat(q, " s > "), q = num(q, (uint32_t)(1000 / (3 + lv) / TR_HZ));
        cat(q, " s");
        break;
      case GAR_ARMOR:
        q = cat(q, "Crash damage ");
        q = num(q, (uint32_t)((12 - 2 * lv) * 100 / 12));
        if (lv < 4) q = cat(q, "% > "), q = num(q, (uint32_t)((10 - 2 * lv) * 100 / 12));
        cat(q, "%");
        break;
      default: cat(q, "Free"); break;
    }
    line = d;
  }
  center(&np_font_small, 160, 107, line, U->note[0] ? GOLD : MUTED);
  keys(U->sel < 4 ? "OK=buy|Up Down=choose|Back=return" : "Left Right=change|Up Down=choose|Back=return");
}

static void records(void) {
  page("RECORDS");
  text(&np_font_small, 168, 50, "Race", MUTED);
  text(&np_font_small, 244, 50, "Lap", MUTED);
  char s[24];
  for (int i = 0; i < TR_TRACKS; i++) {
    int y = 56 + i * 24;
    gfx_rrect(10, y, 300, 21, 5, PANEL, 256);
    text(&np_font_small, 18, y + 15, tr_track_name[i], WHITE);
    if (F->p.best_ms[i]) clock_of(s, F->p.best_ms[i]);
    else cat(s, "-");
    text(&np_font_small, 168, y + 15, s, F->p.best_ms[i] ? GOLD : DIM);
    if (F->p.best_lap_ms[i]) clock_of(s, F->p.best_lap_ms[i]);
    else cat(s, "-");
    text(&np_font_small, 244, y + 15, s, F->p.best_lap_ms[i] ? GOLD : DIM);
  }
  char t[64], *q = cat(t, "Races ");
  q = num(q, F->p.races);
  q = cat(q, "     Wins ");
  q = num(q, F->p.wins);
  q = cat(q, "     Grand Prix won ");
  num(q, F->p.titles);
  center(&np_font_small, 160, 211, t, MUTED);
  keys("Back=menu");
}

static void result(void) {
  dim();
  gfx_rrect(30, 10, 260, 210, 8, INK, 256);
  char s[48], *q;
  const tr_race *r = R;
  if (r->end == END_FINISH) {
    q = ordinal(s, U->result_rank);
    cat(q, U->result_rank == 1 ? " place!" : " place");
  } else {
    cat(s, r->end == END_WRECK ? "Wrecked" : "Time up");
  }
  center(&np_font_title, 160, 38, s, r->end == END_FINISH ? GOLD : RED);
  center(&np_font_small, 160, 54, tr_track_name[r->track], MUTED);
  if (r->end == END_FINISH) {
    clock_of(cat(s, "Time  "), r->car[0].finish_ms);
    center(&np_font_body, 160, 74, s, WHITE);
  }
  if (r->best_lap_ms) {
    clock_of(cat(s, "Best lap  "), r->best_lap_ms);
    center(&np_font_small, 160, 90, s, WHITE);
  }
  q = cat(s, "+");
  q = num(q, U->prize);
  q = cat(q, " credits");
  if (r->mode == MODE_GP) {
    q = cat(q, "    +");
    q = num(q, U->gained[0]);
    cat(q, U->gained[0] == 1 ? " point" : " points");
  }
  center(&np_font_body, 160, 109, s, GOLD);
  if ((U->new_best || U->new_lap) && !U->save_failed) center(&np_font_small, 160, 126, U->new_best ? "New circuit record!" : "New lap record!", GOLD);
  const char *a = r->mode == MODE_GP ? "Standings" : "Race again";
  int by = U->save_failed ? 120 : 134, gap = U->save_failed ? 27 : 29;
  button(50, by, 220, a, U->sel == 0);
  button(50, by + gap, 220, "Garage", U->sel == 1);
  button(50, by + 2 * gap, 220, "Main menu", U->sel == 2);
  if (U->save_failed) not_saved();
  keys("Up Down=choose|OK=select");
}

static void standings(void) {
  dim();
  gfx_rrect(16, 8, 288, 210, 8, INK, 256);
  char s[48], *q;
  bool final = U->round_done >= TR_TRACKS;
  if (final) {
    q = cat(s, "Grand Prix: ");
    ordinal(q, U->final_rank);
  } else {
    q = cat(s, "Grand Prix, race ");
    q = num(q, U->round_done);
    cat(q, " of 6");
  }
  center(&np_font_body, 160, 28, s, final && U->final_rank == 1 ? GOLD : WHITE);
  uint8_t order[TR_CARS];
  for (int i = 0; i < TR_CARS; i++) order[i] = (uint8_t)i;
  for (int i = 1; i < TR_CARS; i++) {   /* by points; on a tie, the order of the table */
    uint8_t v = order[i];
    int j = i;
    while (j > 0 && U->points[order[j - 1]] < U->points[v]) order[j] = order[j - 1], j--;
    order[j] = v;
  }
  for (int k = 0; k < TR_CARS; k++) {
    int i = order[k], y = 36 + k * 17;
    bool you = i == 0;
    gfx_rrect(26, y, 268, 15, 3, you ? HEX(0x2C4A5E) : PANEL, 256);
    num(s, (uint32_t)k + 1);
    text(&np_font_small, 32, y + 11, s, MUTED);
    int paint = you ? F->p.paint : (F->p.paint + i) & 7;
    gfx_rrect(48, y + 4, 12, 7, 2, view_paint(paint), 256);
    text(&np_font_small, 66, y + 11, tr_driver[i], you ? GOLD : WHITE);
    if (U->gained[i]) {
      q = cat(s, "+");
      num(q, U->gained[i]);
      right(&np_font_small, 236, y + 11, s, MUTED);
    }
    num(s, U->points[i]);
    right(&np_font_small, 284, y + 11, s, you ? GOLD : WHITE);
  }
  if (final) {
    q = cat(s, "+");
    q = num(q, U->bonus);
    cat(q, " credits");
    center(&np_font_small, 160, 181, s, GOLD);
  }
  int nb = final ? 2 : 3;
  static const char *const MID[3] = {"Next race", "Garage", "Menu"};
  static const char *const END[2] = {"Main menu", "Garage"};
  for (int i = 0; i < nb; i++) {
    int w = nb == 3 ? 88 : 132, x = 26 + i * (w + (nb == 3 ? 2 : 4));
    button(x, 188, w, final ? END[i] : MID[i], U->sel == i);
  }
  keys("Left Right=choose|OK=select");
}

static void race_screen(void) {
  hud();
  if (U->screen == V_PAUSE) {
    static const char *const L[PAUSE_ROWS] = {"Resume", "Restart race", "How to play", "Quit game"};
    dim();
    gfx_rrect(60, 34, 200, 168, 8, INK, 256);
    center(&np_font_title, 160, 62, "PAUSED", WHITE);
    for (int i = 0; i < PAUSE_ROWS; i++) {
      const char *s = i == PAUSE_RESTART && R->mode == MODE_FREE ? "Restart ride" : L[i];
      button(80, 76 + i * 30, 160, s, U->sel == i);
    }
    keys("Up Down=choose|OK=select|Back=resume");
  } else if (U->screen == V_LEAVE) {
    static const char *const L[3] = {"Keep racing", "Main menu", "Quit Turbo Rush"};
    confirm_box(R->mode == MODE_FREE ? "Leave the ride?" : "Leave the race?", "It's kept: Continue picks it up.", 3, L);
    keys("Up Down=choose|OK=select|Back=pause");
  } else if (U->screen == V_RESULT) {
    result();
  } else if (U->screen == V_STANDINGS) {
    standings();
  }
}

static void scene(void *ctx) {
  (void)ctx;
  switch (U->screen) {
    case V_SETUP: setup(); break;
    case V_NEWRUN: {
      setup();
      static const char *const L[2] = {"Keep it", "Start new"};
      confirm_box("Start something new?", F->gp.on ? "The Grand Prix under way will end." : "The race you left will end.",
                  2, L);
      keys("Up Down=choose|OK=select|Back=return");
      break;
    }
    case V_GUIDE: help_page(U->page, true); break;
    case V_HELP: help_page(U->page, false); break;
    case V_GARAGE: garage(); break;
    case V_RECORDS: records(); break;
    case V_TITLE:
    case V_QUIT:
      view_draw();
      title();
      if (U->screen == V_QUIT) {
        static const char *const L[2] = {"Stay", "Quit"};
        confirm_box("Quit Turbo Rush?", NULL, 2, L);
        keys("Up Down=choose|OK=select|Back=return");
      }
      break;
    default:
      view_draw();
      race_screen();
      break;
  }
}

void tr_render(const tr_file *f, const tr_track *t, const tr_race *prev, const tr_race *race, int alpha, const tr_ui *ui) {
  F = f, T = t, R = race, RP = prev, U = ui, A = alpha;
  int s = ui->screen;
  if (s == V_TITLE || s == V_QUIT || s == V_RACE || s == V_PAUSE || s == V_LEAVE || s == V_RESULT || s == V_STANDINGS)
    view_frame(t, prev, race, alpha);
  color_t *old = gfx_buf;
  gfx_buf = strip;
  gfx_render(scene, 0, 0, 240);
  gfx_buf = old;
}
