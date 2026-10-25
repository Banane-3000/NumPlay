/* PolyGT 3D's screens: the title over a car on a turntable, the race screen, how to play and the records; the race's
 * dials; the pause, results and questions over the race. Each has a line of plain keys at the bottom. */
#include <string.h>
#include "../../../launcher/src/gfx.h"
#include "draw.h"

#define WHITE 0xFFFF
#define INK HEX(0x0E0B24)
#define PANEL HEX(0x1C1745)
#define LINE HEX(0x3B3478)
#define CYAN HEX(0x00C3EB)
#define SUN HEX(0xEB6E50)
#define GOLD HEX(0xFFD052)
#define MUTED HEX(0xB2ADD6)
#define DIM HEX(0x6A6496)
#define RED HEX(0xFF5A4E)
#define STRIP 24

/* the launcher's drawing (gfx.c) sends strips here when asked; this game sends its own */
void plat_push(int x, int y, int w, int h, const uint16_t *px);
void np_push(int x, int y, int w, int h, const color_t *px) { plat_push(x, y, w, h, px); }
static color_t strip[SCREEN_W * STRIP];

const char *const pg_driver[PG_CARS] = {"You", "Aiko", "Bruno", "Dara", "Emil", "Farah", "Goro", "Hana"};
static const char *const MODES[2] = {"Circuit race", "Endless road"};
static const char *const LEVELS[3] = {"Easy", "Normal", "Hard"};
static const char *const VIEW_NAMES[VIEWS] = {"Behind", "Far behind", "Bumper"};

static const pg_file *F;
static const pg_track *T;
static const pg_race *R;
static const pg_ui *U;

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
  *p++ = (char)('0' + ms / 100 % 10), *p++ = (char)('0' + ms / 10 % 10), *p = 0;
  return p;
}
static char *ordinal(char *p, int n) {
  p = num(p, (uint32_t)n);
  return cat(p, n == 1 ? "st" : n == 2 ? "nd" : n == 3 ? "rd" : "th");
}
static char *km(char *p, uint32_t m) {
  p = num(p, m / 1000);
  *p++ = '.', *p++ = (char)('0' + m / 100 % 10);
  return cat(p, " km");
}

static void text(const np_font_t *f, int x, int y, const char *s, color_t c) { gfx_text(f, x, y, s, c, 256); }
static void center(const np_font_t *f, int x, int y, const char *s, color_t c) { gfx_text_center(f, x, y, s, c, 256); }
static void right(const np_font_t *f, int x, int y, const char *s, color_t c) { gfx_text_right(f, x, y, s, c, 256); }

/* ------------------------------------------------------------------ pieces */
/* a line of keys: "Up Down=choose|OK=open", each key in cyan, what it does after it */
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
      if (pass) text(&np_font_small, x + w, y, buf, key ? CYAN : MUTED);
      w += tw + (key ? 4 : 0);
      q += k;
      if (*q == '=') q++;
      else if (*q == '|') q++, w += 11;
    }
  }
}

static void keys(const char *s) {
  gfx_fill(0, 222, 320, 18, INK);
  gfx_fill(0, 222, 320, 1, LINE);
  key_line(235, s);
}

static void page(const char *title) {
  gfx_vgrad(0, 0, 320, 222, 0x120E30, 0x2A1F55);
  text(&np_font_title, 14, 30, title, WHITE);
}

/* a button: the chosen one bright and framed */
static void button(int x, int y, int w, const char *s, bool on) {
  if (on) {
    gfx_rrect(x - 2, y - 2, w + 4, 30, 8, WHITE, 256);
    gfx_rrect(x, y, w, 26, 6, CYAN, 256);
  } else {
    gfx_rrect(x, y, w, 26, 6, PANEL, 235);
    gfx_rrect_outline(x, y, w, 26, 6, 1, LINE, 256);
  }
  center(&np_font_body, x + w / 2, y + 18, s, on ? INK : WHITE);
}

static void dim(void) { gfx_fill_alpha(0, 0, 320, 240, HEX(0x05040F), 150); }

/* when the calculator's storage is full, say so above the keys */
static void not_saved(void) {
  if (!U->save_failed) return;
  gfx_rrect(40, 202, 240, 17, 5, HEX(0x5A1E1A), 240);
  center(&np_font_small, 160, 214, "Storage full: progress not saved", WHITE);
}

static void bar(int x, int y, int w, int h, int v, int max, color_t c) {
  gfx_rrect(x, y, w, h, h / 2, HEX(0x2A2456), 256);
  int f = v * w / max;
  if (f > 0) gfx_rrect(x, y, f < h ? h : f, h, h / 2, c, 256);
}

/* "POLYGT 3D", twice the title font, slanted, with a shadow */
static void logo(int x0, int y) {
  const np_font_t *f = &np_font_title;
  const char *s = "POLYGT 3D";
  for (int pass = 0; pass < 2; pass++) {
    int x = x0 + (pass ? 0 : 2), base = y + (pass ? 0 : 2);
    for (const char *q = s; *q; q++) {
      const np_glyph_t *g = &f->glyphs[(uint8_t)*q - f->first];
      const uint8_t *m = f->data + g->offset;
      color_t c = pass ? (q >= s + 7 ? SUN : WHITE) : HEX(0x0A0820);
      for (int gy = 0; gy < g->h; gy++) {
        int yy = base + (g->y + gy) * 2;
        if (yy + 2 <= gfx_y0 || yy >= gfx_y1) continue;
        for (int gx = 0; gx < g->w; gx++) {
          int k = gy * g->w + gx, cov = (m[k / 2] >> ((k & 1) * 4)) & 15;
          int lean = (g->h - gy) / 3;
          if (cov) gfx_fill_alpha(x + (g->x + gx) * 2 + lean, yy, 2, 2, c, cov * 17);
        }
      }
      x += g->advance * 2;
    }
  }
}

/* a circuit's outline: a soft edge, a light line, the start line */
static int16_t map_x[64], map_y[64];
static int map_of = -1;
static void map_setup(const pg_track *t, int w, int h) {
  int key = t->id * 1000 + w * 10 + h;
  if (map_of == key) return;
  pg_map(t, 64, map_x, map_y, w, h);
  map_of = key;
}
static void outline(int x, int y) {
  for (int pass = 0; pass < 2; pass++)
    for (int i = 0; i < 64; i++) {
      int j = (i + 1) % 64, x0 = x + map_x[i], y0 = y + map_y[i], x1 = x + map_x[j], y1 = y + map_y[j];
      int n = NP_MAX(NP_MAX(x1 - x0, x0 - x1), NP_MAX(y1 - y0, y0 - y1));
      for (int k = 0; k <= n; k++) {
        int px = x0 + (n ? (x1 - x0) * k / n : 0), py = y0 + (n ? (y1 - y0) * k / n : 0);
        if (pass) gfx_fill(px, py, 1, 1, HEX(0xE6E4F2));
        else gfx_fill(px - 1, py - 1, 3, 3, HEX(0x6E68A0));
      }
    }
  gfx_fill(x + map_x[0] - 1, y + map_y[0] - 3, 3, 6, WHITE);
}
/* where a distance is on the outline */
static void map_at(int32_t s, int *px, int *py) {
  int32_t L = T->length, m = s % L;
  if (m < 0) m += L;
  int64_t f = (int64_t)m * 64 / L;
  int i = (int)f, j = (i + 1) % 64;
  int32_t frac = (int32_t)((int64_t)m * 64 % L * 256 / L);
  *px = map_x[i] + (map_x[j] - map_x[i]) * frac / 256;
  *py = map_y[i] + (map_y[j] - map_y[i]) * frac / 256;
}

/* ------------------------------------------------------------------ the race's dials */
static void hud(void) {
  const pg_race *r = R;
  char s[40], *p;
  bool endless = r->mode == MODE_ENDLESS;
  gfx_rrect(6, 6, 98, 40, 6, INK, 215);
  if (endless) {
    text(&np_font_small, 14, 19, "TIME", MUTED);
    text(&np_font_small, 62, 19, "ZONE", MUTED);
    uint32_t left = (uint32_t)r->time_left;
    num(s, (left + 999) / 1000);
    text(&np_font_title, 14, 40, s, left < 10000 && ((U->clock / 250) & 1) ? RED : WHITE);
    num(s, (uint32_t)r->zone + 1);
    text(&np_font_body, 62, 40, s, WHITE);
  } else {
    text(&np_font_small, 14, 19, "POS", MUTED);
    text(&np_font_small, 62, 19, "LAP", MUTED);
    num(s, (uint32_t)pg_rank(r, 0));
    int w = gfx_text(&np_font_title, 14, 40, s, WHITE, 256);
    p = cat(s, "/");
    num(p, (uint32_t)r->rivals + 1);
    text(&np_font_body, 14 + w + 1, 40, s, MUTED);
    p = num(s, r->lap);
    p = cat(p, "/");
    num(p, r->laps);
    text(&np_font_body, 62, 40, s, WHITE);
  }
  /* the clock, or the distance */
  gfx_rrect(112, 6, 96, 40, 6, INK, 215);
  if (endless) {
    km(s, (uint32_t)(r->car[0].s / FX));
    center(&np_font_body, 160, 23, s, WHITE);
    if (F->p.best_m) {
      p = cat(s, "Best ");
      km(p, F->p.best_m);
      center(&np_font_small, 160, 38, s, MUTED);
    } else {
      center(&np_font_small, 160, 38, pg_zone_name[(pg_zone_at(T, r->car[0].s) - 3) % 5], MUTED);
    }
  } else {
    clock_of(s, r->time_ms);
    center(&np_font_body, 160, 23, s, WHITE);
    uint32_t best = r->best_lap_ms;
    if (best) {
      clock_of(cat(s, "Best lap "), best);
      center(&np_font_small, 160, 38, s, MUTED);
    } else {
      center(&np_font_small, 160, 38, pg_circuit_name[r->track], MUTED);
    }
  }
  /* the map */
  if (!endless) {
    map_setup(T, 70, 50);
    gfx_rrect(232, 6, 82, 62, 6, INK, 200);
    outline(238, 12);
    int sx, sy;
    for (int i = r->rivals; i >= 1; i--) {
      map_at(r->car[i].s, &sx, &sy);
      gfx_circle(238 + sx, 12 + sy, 2, rd_rgb(rd_paint[r->car[i].paint & 7]), 256);
    }
    map_at(r->car[0].s, &sx, &sy);
    gfx_circle(238 + sx, 12 + sy, 4, WHITE, 256);
    gfx_circle(238 + sx, 12 + sy, 3, SUN, 256);
  }
  /* speed | gear | nitro, then the keys */
  const pg_car *c = &r->car[0];
  gfx_fill_alpha(0, 202, 320, 25, INK, 230);
  gfx_fill(0, 202, 320, 1, LINE);
  gfx_fill(0, 227, 320, 13, HEX(0x08061A));
  gfx_fill(92, 207, 1, 16, LINE), gfx_fill(140, 207, 1, 16, LINE);
  num(s, (uint32_t)pg_kmh(c->v));
  right(&np_font_title, 56, 224, s, c->boost ? CYAN : WHITE);
  text(&np_font_small, 60, 224, "km/h", MUTED);
  text(&np_font_small, 99, 213, "GEAR", MUTED);
  s[0] = (char)('0' + c->gear), s[1] = 0;
  bool red = pg_rpm(r) > pg_vehicles[c->model].redline - 350;
  text(&np_font_body, 100, 225, s, red && r->manual ? RED : r->manual ? GOLD : WHITE);
  text(&np_font_small, 113, 225, r->manual ? "MT" : "AT", DIM);
  const char *nl = r->flash ? "NITRO +" : r->slipstream > 10 ? "SLIPSTREAM" : c->drift ? "SLIDE" : "NITRO";
  text(&np_font_small, 146, 213, nl, r->flash || r->slipstream > 10 ? GOLD : c->drift ? SUN : MUTED);
  bar(146, 216, 166, 7, c->nitro, 1000, c->boost ? HEX(0xA8F2FF) : CYAN);
  key_line(237, r->manual ? "Up=gas|Down=brake|OK=nitro|+ -=gears|Back=pause" : "Up=gas|Down=brake|OK=nitro|Back=pause");
  /* the start, a lap, the finish */
  if (r->phase == PH_GRID || (U->hold && U->screen == V_RACE)) {
    int n = r->phase == PH_GRID ? (r->countdown + PG_HZ - 1) / PG_HZ : U->hold;
    gfx_rrect(100, 58, 120, 48, 8, INK, 225);
    if (n > 3) {
      center(&np_font_body, 160, 78, endless ? "Endless road" : pg_circuit_name[r->track], WHITE);
      center(&np_font_small, 160, 96, "Get ready", MUTED);
    } else {
      s[0] = (char)('0' + n), s[1] = 0;
      center(&np_font_title, 160, 90, s, GOLD);
    }
  } else if (r->phase == PH_RACE && r->time_ms < 1000 && !r->banner_t) {
    gfx_rrect(110, 58, 100, 48, 8, INK, 225);
    center(&np_font_title, 160, 90, "GO!", GOLD);
  } else if (r->phase == PH_DONE) {
    gfx_rrect(90, 58, 140, 54, 8, INK, 225);
    if (r->end == END_FINISH) {
      center(&np_font_title, 160, 86, "Finish!", GOLD);
      ordinal(s, r->car[0].place);
      center(&np_font_body, 160, 105, s, WHITE);
    } else {
      center(&np_font_title, 160, 86, "Time up", RED);
      if (endless) km(s, (uint32_t)(r->car[0].s / FX)), center(&np_font_body, 160, 105, s, WHITE);
    }
  } else if (r->banner_t) {
    const char *big = "";
    char t[24];
    t[0] = 0;
    color_t col = GOLD;
    if (r->banner == BANNER_LAP || r->banner == BANNER_FINAL) {
      if (r->banner == BANNER_FINAL) big = "Final lap";
      else p = cat(s, "Lap "), num(p, r->banner_arg), big = s;
      char *e = clock_of(t, r->last_lap_ms);
      if (r->banner_arg >= 3 && r->last_lap_ms == r->best_lap_ms) cat(e, "  best");
    } else if (r->banner == BANNER_ZONE) {
      p = cat(s, "Zone "), num(p, (uint32_t)r->banner_arg + 1), big = s;
      uint32_t b = (uint32_t)pg_zone_bonus(r->banner_arg);
      p = cat(t, "+");
      p = num(p, b / 1000);
      if (b % 1000) *p++ = '.', *p++ = (char)('0' + b / 100 % 10), *p = 0;
      cat(p, " s");
    } else if (r->banner == BANNER_WRONG) {
      big = "Wrong way", col = RED;
      cat(t, "Turn round");
    }
    if (big[0]) {
      gfx_rrect(90, 58, 140, t[0] ? 54 : 40, 8, INK, 225);
      center(&np_font_title, 160, 86, big, col);
      if (t[0]) center(&np_font_body, 160, 105, t, WHITE);
    }
  }
}

/* ------------------------------------------------------------------ screens */
static void title(void) {
  for (int y = 0; y < 84; y += 4) gfx_fill_alpha(0, y, 320, 4, HEX(0x05040F), 160 - y * 160 / 84);
  logo(16, 46);
  text(&np_font_small, 18, 66, "by Boris-TH, made for NumPlay", WHITE);
  static const char *const NAMES[4] = {"Continue", "Play", "Records", "How to play"};
  int n = U->menu_n, y0 = 92;
  for (int i = 0; i < n; i++) {
    int item = U->menu[i];
    const char *s = NAMES[item];
    if (item == MENU_CONTINUE) s = F->race.mode == MODE_ENDLESS ? "Continue drive" : "Continue race";
    button(16, y0 + i * 31, 136, s, U->sel == i);
  }
  /* the car on the turntable (or, when the storage is full, that) */
  const pg_vehicle *v = &pg_vehicles[F->p.vehicle];
  if (U->save_failed) {
    gfx_rrect(164, 186, 150, 30, 6, HEX(0x5A1E1A), 240);
    center(&np_font_small, 239, 199, "Storage full:", WHITE);
    center(&np_font_small, 239, 211, "progress not saved", WHITE);
  } else {
    gfx_rrect(164, 186, 150, 30, 6, INK, 200);
    center(&np_font_body, 239, 200, v->name, WHITE);
    center(&np_font_small, 239, 212, v->kind, MUTED);
  }
  keys("Up Down=choose|OK=open|Back=quit");
}

static void stat_row(int y, const char *name, int v, int max) {
  text(&np_font_small, 212, y, name, MUTED);
  bar(212, y + 4, 90, 6, v < 0 ? 0 : v, max, CYAN);
}

static void setup(void) {
  page("RACE");
  const pg_profile *p = &F->p;
  static const char *const NAMES[SET_ROWS] = {"Mode", "Circuit", "Car", "Laps", "Rivals", "Level", "Gearbox", "View", ""};
  bool endless = p->mode == MODE_ENDLESS;
  char s[24];
  for (int i = 0; i < SET_START; i++) {
    int y = 38 + i * 19;
    bool on = U->sel == i;
    bool off = endless && (i == SET_TRACK || i == SET_LAPS || i == SET_RIVALS || i == SET_LEVEL);
    if (on) gfx_rrect(8, y - 1, 194, 19, 5, WHITE, 256);
    gfx_rrect(10, y, 190, 17, 4, on ? CYAN : PANEL, 256);
    color_t c = on ? INK : off ? DIM : WHITE;
    text(&np_font_small, 18, y + 13, NAMES[i], c);
    const char *v = "";
    switch (i) {
      case SET_MODE: v = MODES[p->mode]; break;
      case SET_TRACK: v = endless ? "-" : pg_circuit_name[p->track]; break;
      case SET_CAR: v = pg_vehicles[p->vehicle].name; break;
      case SET_LAPS:
        if (endless) v = "-";
        else num(s, p->laps), v = s;
        break;
      case SET_RIVALS:
        if (endless) v = "-";
        else if (!p->rivals) v = "None";
        else num(s, p->rivals), v = s;
        break;
      case SET_LEVEL: v = endless ? "-" : LEVELS[p->level]; break;
      case SET_GEARS: v = p->manual ? "Manual" : "Automatic"; break;
      case SET_VIEW: v = VIEW_NAMES[p->view]; break;
    }
    right(&np_font_small, off ? 192 : 180, y + 13, v, c);
    if (!off) text(&np_font_small, 184, y + 13, ">", on ? INK : DIM);
  }
  /* the right: the car when choosing it, else the circuit or the road */
  gfx_rrect(206, 38, 104, 150, 6, PANEL, 256);
  if (U->sel == SET_CAR) {
    const pg_vehicle *v = &pg_vehicles[p->vehicle];
    center(&np_font_small, 258, 54, v->name, WHITE);
    stat_row(74, "Top speed", v->top / 8 - 60, 25);
    stat_row(98, "Pick-up", v->accel / 8 - 30, 18);
    stat_row(122, "Grip", v->grip - 110, 50);
    stat_row(146, "On grass", 160 - v->offroad, 100);
    num(s, (uint32_t)(v->top / 8) * 36 / 10);
    cat(s + strlen(s), " km/h");
    center(&np_font_small, 258, 178, s, GOLD);
  } else if (endless) {
    center(&np_font_small, 258, 56, "Endless road", WHITE);
    static const char *const L[6] = {"Reach each zone", "before the time", "runs out.", "Every zone adds", "time; the road", "never ends."};
    for (int i = 0; i < 6; i++) center(&np_font_small, 258, 80 + i * 15, L[i], MUTED);
    if (p->best_m) {
      km(cat(s, "Best "), p->best_m);
      center(&np_font_small, 258, 178, s, GOLD);
    }
  } else {
    center(&np_font_small, 258, 54, "Circuit", MUTED);
    map_setup(T, 84, 66);
    outline(216, 62);
    center(&np_font_small, 258, 146, pg_circuit_name[p->track], WHITE);
    center(&np_font_small, 258, 160, pg_circuit_note[p->track], MUTED);
    if (p->best_ms[p->track]) {
      clock_of(cat(s, "Best "), p->best_ms[p->track]);
      center(&np_font_small, 258, 178, s, GOLD);
    }
  }
  button(10, 192, 300, endless ? "Start the drive" : "Start race", U->sel == SET_START);
  keys(U->sel == SET_START ? "OK=start|Up Down=choose|Back=menu" : "Left Right=change|Up Down=choose|Back=menu");
}

static void confirm_box(const char *q, const char *note, int n, const char *const *labels) {
  dim();
  int h = 70 + n * 32;
  gfx_rrect(40, 120 - h / 2, 240, h, 8, INK, 256);
  gfx_rrect_outline(40, 120 - h / 2, 240, h, 8, 1, LINE, 256);
  center(&np_font_body, 160, 120 - h / 2 + 28, q, WHITE);
  if (note) center(&np_font_small, 160, 120 - h / 2 + 46, note, MUTED);
  for (int i = 0; i < n; i++) button(60, 120 - h / 2 + 58 + i * 32, 200, labels[i], U->sel == i);
}

static void help_page(int k, bool guide) {
  page(guide ? "BEFORE YOU RACE" : "HOW TO PLAY");
  static const char *const TITLES[PG_HELP_PAGES] = {"Keys", "Racing", "Endless road"};
  center(&np_font_body, 160, 56, TITLES[k], GOLD);
  if (k == 0) {
    static const char *const K[7][2] = {{"Up", "Accelerate"}, {"Down", "Brake"}, {"Left Right", "Steer"},
                                        {"OK or EXE", "Nitro"}, {"+ and -", "Gears, if manual"},
                                        {"Back", "Pause"}, {"Home", "Save and quit"}};
    for (int i = 0; i < 7; i++) {
      int y = 66 + i * 20;
      gfx_rrect(40, y, 240, 18, 4, PANEL, 256);
      text(&np_font_small, 50, y + 13, K[i][0], CYAN);
      text(&np_font_small, 140, y + 13, K[i][1], WHITE);
    }
  } else {
    static const char *const L[2][7] = {
        {"Finish the laps ahead of the rivals.", "Brake before tight bends, or you", "run wide. Brake while steering hard",
         "to slide round a hairpin. Orbs on the", "road fill your nitro; driving close", "behind a car pulls you along.",
         "Your best lap comes back as a ghost."},
        {"Reach the next zone before the", "clock runs out: each one adds time,", "a little less each time.",
         "There are no rivals, just you,", "the orbs and the road, which goes", "on and changes scenery.", ""}};
    for (int i = 0; i < 7; i++) center(&np_font_small, 160, 82 + i * 19, L[k - 1][i], WHITE);
  }
  for (int i = 0; i < PG_HELP_PAGES; i++) gfx_circle(148 + i * 12, 212, 3, i == k ? CYAN : DIM, 256);
  keys(guide ? "OK=start|Left Right=page|Back=return" : "Left Right=page|Back=return");
}

static void records(void) {
  page("RECORDS");
  text(&np_font_small, 172, 50, "Race", MUTED);
  text(&np_font_small, 246, 50, "Lap", MUTED);
  char s[32];
  for (int i = 0; i < PG_CIRCUITS; i++) {
    int y = 56 + i * 24;
    gfx_rrect(10, y, 300, 21, 5, PANEL, 256);
    text(&np_font_small, 18, y + 15, pg_circuit_name[i], WHITE);
    if (F->p.best_ms[i]) clock_of(s, F->p.best_ms[i]);
    else cat(s, "-");
    text(&np_font_small, 172, y + 15, s, F->p.best_ms[i] ? GOLD : DIM);
    if (F->p.best_lap_ms[i]) clock_of(s, F->p.best_lap_ms[i]);
    else cat(s, "-");
    text(&np_font_small, 246, y + 15, s, F->p.best_lap_ms[i] ? GOLD : DIM);
  }
  int y = 56 + 3 * 24;
  gfx_rrect(10, y, 300, 21, 5, PANEL, 256);
  text(&np_font_small, 18, y + 15, "Endless road", WHITE);
  if (F->p.best_m) {
    char *q = km(s, F->p.best_m);
    q = cat(q, ", zone ");
    num(q, F->p.best_zone);
  } else {
    cat(s, "-");
  }
  text(&np_font_small, 172, y + 15, s, F->p.best_m ? GOLD : DIM);
  char t[64], *q = cat(t, "Races ");
  q = num(q, F->p.races);
  q = cat(q, "     Wins ");
  num(q, F->p.wins);
  center(&np_font_small, 160, 196, t, MUTED);
  keys("Back=menu");
}

static void result(void) {
  dim();
  const pg_race *r = R;
  char s[48];
  bool endless = r->mode == MODE_ENDLESS;
  int h = endless ? 176 : 196, y0 = (222 - h) / 2;
  gfx_rrect(30, y0, 260, h, 8, INK, 256);
  gfx_rrect_outline(30, y0, 260, h, 8, 1, LINE, 256);
  int y = y0 + 30;
  if (endless) {
    km(s, (uint32_t)(r->car[0].s / FX));
    center(&np_font_title, 160, y, s, GOLD);
    char *q = cat(s, "Zone ");
    num(q, (uint32_t)r->zone + 1);
    center(&np_font_body, 160, y + 22, s, WHITE);
    if (U->new_far && !U->save_failed) center(&np_font_small, 160, y + 40, "Your furthest yet!", GOLD);
    else if (F->p.best_m) km(cat(s, "Best "), F->p.best_m), center(&np_font_small, 160, y + 40, s, MUTED);
  } else if (r->end == END_TIME) {
    center(&np_font_title, 160, y, "Time up", RED);
    center(&np_font_small, 160, y + 16, pg_circuit_name[r->track], MUTED);
  } else {
    char *q = ordinal(s, U->result_rank);
    cat(q, U->result_rank == 1 ? " place!" : " place");
    center(&np_font_title, 160, y, s, U->result_rank == 1 ? GOLD : WHITE);
    center(&np_font_small, 160, y + 16, pg_circuit_name[r->track], MUTED);
    clock_of(cat(s, "Time  "), r->car[0].finish_ms);
    center(&np_font_body, 160, y + 36, s, WHITE);
    if (r->best_lap_ms) {
      clock_of(cat(s, "Best lap  "), r->best_lap_ms);
      center(&np_font_small, 160, y + 52, s, WHITE);
    }
    if ((U->new_best || U->new_lap) && !U->save_failed)
      center(&np_font_small, 160, y + 68, U->new_best ? "New circuit record!" : "New lap record!", GOLD);
  }
  int by = y0 + h - 70;
  button(50, by, 220, endless ? "Drive again" : "Race again", U->sel == 0);
  button(50, by + 31, 220, "Main menu", U->sel == 1);
  if (U->save_failed) not_saved();
  keys("Up Down=choose|OK=select");
}

static void race_screen(void) {
  hud();
  if (U->screen == V_PAUSE) {
    static const char *const L[PAUSE_ROWS] = {"Resume", "Restart race", "How to play", "Quit game"};
    dim();
    gfx_rrect(60, 30, 200, 172, 8, INK, 256);
    gfx_rrect_outline(60, 30, 200, 172, 8, 1, LINE, 256);
    center(&np_font_title, 160, 60, "PAUSED", WHITE);
    for (int i = 0; i < PAUSE_ROWS; i++) {
      const char *s = i == PAUSE_RESTART && R->mode == MODE_ENDLESS ? "Restart drive" : L[i];
      button(80, 74 + i * 31, 160, s, U->sel == i);
    }
    keys("Up Down=choose|OK=select|Back=resume");
  } else if (U->screen == V_LEAVE) {
    static const char *const L[3] = {"Keep racing", "Main menu", "Quit PolyGT 3D"};
    confirm_box(R->mode == MODE_ENDLESS ? "Leave the drive?" : "Leave the race?", "It's kept: Continue picks it up.", 3, L);
    keys("Up Down=choose|OK=select|Back=pause");
  } else if (U->screen == V_RESULT) {
    result();
  }
}

static void scene(void) {
  switch (U->screen) {
    case V_SETUP: setup(); break;
    case V_NEWRUN: {
      setup();
      static const char *const L[2] = {"Keep it", "Start new"};
      confirm_box("Start something new?", F->race.mode == MODE_ENDLESS ? "The drive you left will end." : "The race you left will end.", 2, L);
      keys("Up Down=choose|OK=select|Back=return");
      break;
    }
    case V_GUIDE: help_page(U->page, true); break;
    case V_HELP: help_page(U->page, false); break;
    case V_RECORDS: records(); break;
    case V_TITLE:
    case V_QUIT:
      rd_draw();
      title();
      if (U->screen == V_QUIT) {
        static const char *const L[2] = {"Stay", "Quit"};
        confirm_box("Quit PolyGT 3D?", NULL, 2, L);
        keys("Up Down=choose|OK=select|Back=return");
      }
      break;
    default:
      rd_draw();
      race_screen();
      break;
  }
}

void pg_render(const pg_file *f, const pg_track *t, const pg_race *prev, const pg_race *race, int alpha,
               const pg_ui *ui, const rd_ghost *g) {
  F = f, T = t, R = race, U = ui;
  int s = ui->screen;
  if (s == V_RACE || s == V_PAUSE || s == V_LEAVE || s == V_RESULT)
    rd_race(t, prev, race, alpha, f->p.view, g, ui->clock);
  else if (s == V_TITLE || s == V_QUIT)
    rd_showroom(f->p.vehicle, (int32_t)(ui->clock * 12), f->p.track, ui->clock);
  color_t *old = gfx_buf;
  gfx_buf = strip;
  for (int y = 0; y < SCREEN_H; y += STRIP) {
    gfx_y0 = y, gfx_y1 = y + STRIP;
    scene();
    plat_push(0, y, SCREEN_W, STRIP, strip);
  }
  gfx_buf = old;
}
