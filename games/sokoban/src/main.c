/* Sokoban: push every box onto a goal.
 *
 * Hiroyuki Imabayashi's warehouse puzzle (Thinking Rabbit, 1982), with David W. Skinner's Microban:
 * 155 small levels, from the first push to The Dungeon. The keeper walks the floor and pushes the
 * boxes one at a time, never pulling them; every step can be undone. A level opens while fewer
 * than three levels are left unsolved before it, and the fewest moves each was solved in are kept.
 *
 * The levels are packed by tools/levels.py: only the floor, the goals, the boxes and the keeper (the
 * walls are the cells around the floor), compressed with DEFLATE and unpacked into the strip buffer
 * when a level opens. Only the rows a step changes are drawn again, in the launcher's strips
 * (launcher/src/gfx.c). Inside NumPlay it uses the launcher's shapes, fonts, files and inflater; on
 * its own it brings them along. */
#include <eadk.h>
#include "../../../launcher/src/gfx.h"
#include "../../../launcher/src/inflate.h"
#include "../../common/np_text.h"

#if NP_MODULE /* (np_file_read and np_file_write: NumPlay's, launcher/src/np.h) */
#define np_app_begin()
#define np_app_end() 0
#else
#include "../../common/epsilon_app.h"
#include "../../common/epsilon_files.h"
#define np_file_read ef_read
#define np_file_write ef_write
const char eadk_app_name[] __attribute__((section(".rodata.eadk_app_name"))) = "Sokoban";
const uint32_t eadk_api_level __attribute__((section(".rodata.eadk_api_level"))) = 0;
void np_push(int x, int y, int w, int h, const color_t *px) {
  eadk_display_push_rect((eadk_rect_t){(uint16_t)x, (uint16_t)y, (uint16_t)w, (uint16_t)h}, px);
}
#endif

#include "levels.inc" /* levels_z, LEVELS (inside NumPlay, fewer: see the Makefile), ALL_LEVELS, TEXT_BYTES, MAXW, MAXH */

/* the keys used, in 32 bits: those past the 32nd (4 5 6 2 EXE) come down by 22 */
#define KEY(k) (1u << ((k) - ((k) >= 32) * 22))
static uint32_t keys(void) {
  uint64_t s = eadk_keyboard_scan();
  return ((uint32_t)s & 0x800201FFu) | ((uint32_t)(s >> 32) & 0x00100870u) << 10;
}
#define HDR 22   /* the band on top */
#define STEPS 4  /* frames a step slides over */
#define HIST 1024
#define F_FLOOR 1
#define F_GOAL 2
#define F_BOX 4
#define F_WALL 8

#define WHITE 0xFFFF
#define BG HEX(0x2B2730) /* around the warehouse */
#define BAR HEX(0x1C1A20)
#define CREAM HEX(0xF2E6CF)
#define GOLD HEX(0xF2B53C)
#define GREEN HEX(0x5DAE45)

/* sokoban.sav: the level played last, and the fewest moves each level was solved in (0: not yet), for
   all of Microban whatever this build holds, so NumPlay and Sokoban.nwa share it */
static struct {
  uint8_t magic, version, flags, level; /* flags: 1 the keys were shown */
  uint16_t best[ALL_LEVELS];
} sv;
#define SAVE_NAME "sokoban.sav"

/* the level: a wall's width around its floor, MAXW cells a row (g has room for the neighbours of
   the first and last cells) */
static uint8_t g[MAXW * (MAXH + 2) + 2];
#define G (g + MAXW + 1)
static const int8_t off[4] = {-MAXW, 1, MAXW, -1}, ddx[4] = {0, 1, 0, -1}, ddy[4] = {-1, 0, 1, 0}; /* up, right... */
static uint8_t hist[HIST];                         /* the steps taken: the way, and 4 for a push */
static color_t strip[SCREEN_W * STRIP_H];
static int level, rows, size, ox, oy, keeper, face, left, moves, pushes, nh, anim, moved;
static int scr, ov, sel, pick, top, dlo, dhi;
enum { OV_NONE, OV_PAUSE, OV_QUIT, OV_HELP, OV_DONE };
static char line[64], msg[48]; /* any text; what the Solved! panel says */

static char *cat(char *o, const char *s) { /* s at o; returns its end */
  while ((*o = *s++)) o++;
  return o;
}
static char *put(char *o, const char *s, uint32_t v) { /* a text and a number at o; returns its end */
  char t[10];
  int n = 0;
  o = cat(o, s);
  do t[n++] = (char)('0' + v % 10); while (v /= 10);
  while (n) *o++ = t[--n];
  *o = 0;
  return o;
}
static void save(void) {
  sv.magic = 'S', sv.version = 1;
  np_file_write(SAVE_NAME, &sv, sizeof sv);
}
static bool unlocked(int i) {
  int n = 0;
  for (int j = 0; j < LEVELS; j++) n += !sv.best[j];
  return sv.best[i] || i < LEVELS - n + 3;
}
static void mark(int a, int b) { /* rows a to b to draw again */
  if (a < dlo) dlo = a;
  if (b > dhi) dhi = b;
}
static void mark_cell(int i) { mark(oy + (i / MAXW - 1) * size, oy + (i / MAXW + 2) * size); }

/* ------------------------------------------------------------ the level */
static void open_level(int n) {
  uint32_t tables[816]; /* np_inflate's two Huffman tables, 2 x 1632 bytes */
  const uint8_t *t = (const uint8_t *)strip;
  np_inflate((uint8_t *)strip, TEXT_BYTES, levels_z, sizeof levels_z, (np_inflate_tables_t *)(void *)tables);
  for (int k = n; k; t++)
    if (t[0] == '\n' && t[1] == '\n') k--, t++;
  for (unsigned i = 0; i < sizeof g; i++) g[i] = 0;
  int x = 1, y = 1, w = 0;
  for (left = 0; *t != '\n' || x > 1; t++) {
    int c = *t, i = y * MAXW + x++;
    if (c == '\n') y++, x = 1;
    else if (c != ' ') {
      for (int k = 0; k < 6; k++) /* floor, goal, box, box on a goal, the keeper, on a goal */
        if (c == "-.$*@+"[k]) G[i] = (uint8_t)"\1\3\5\7\1\3"[k], keeper = k > 3 ? i : keeper;
      left += c == '$';
    }
    if (x > w) w = x;
  }
  rows = y + 1;
  for (int i = 0; i < rows * MAXW; i++) /* walls on every cell next to the floor */
    for (int d = 0; d < 9; d++)
      if (!(G[i] & F_FLOOR) && G[i + (d / 3 - 1) * MAXW + d % 3 - 1] & F_FLOOR) G[i] = F_WALL;
  size = (SCREEN_W - 8) / (w + 1);
  if ((SCREEN_H - HDR - 8) / rows < size) size = (SCREEN_H - HDR - 8) / rows;
  if (size > 24) size = 24;
  ox = (SCREEN_W - (w + 1) * size) / 2, oy = HDR + (SCREEN_H - HDR - rows * size) / 2;
  level = sv.level = (uint8_t)n, moves = pushes = nh = anim = ov = 0, face = 2, scr = 1;
  mark(0, SCREEN_H);
  save();
}

/* a box from one cell to the next */
static void shove(int from, int to) {
  G[from] ^= F_BOX, G[to] |= F_BOX, left += !(G[to] & F_GOAL) - !(G[from] & F_GOAL);
}
/* the keeper steps one way, pushing a box if there is one and it can go; or a step back */
static void step(int d) {
  int o = off[d], to = keeper + o, b = to + o;
  face = d, moved = -1;
  mark_cell(keeper);
  if (!(G[to] & F_FLOOR) || (G[to] & F_BOX && (!(G[b] & F_FLOOR) || G[b] & F_BOX))) return;
  if (nh == HIST) { /* the oldest steps can't be undone any more */
    for (int i = 0; i < HIST / 2; i++) hist[i] = hist[i + HIST / 2];
    nh = HIST / 2;
  }
  hist[nh++] = (uint8_t)d;
  if (G[to] & F_BOX) {
    shove(to, b);
    pushes++, hist[nh - 1] |= 4, moved = b;
  }
  moves++, keeper = to, anim = STEPS;
}
static void undo(void) {
  int h = hist[--nh], o = off[h & 3];
  if (h & 4) shove(keeper + o, keeper), pushes--;
  mark_cell(keeper);
  keeper -= o, face = h & 3, moves--;
}

/* ------------------------------------------------------------ drawing */
static void box(int i, int dx, int dy) {
  int s = size, x = ox + i % MAXW * s + dx, y = oy + i / MAXW * s + dy;
  color_t dark = G[i] & F_GOAL ? HEX(0x3F7A2C) : HEX(0x8A5422);
  gfx_rrect(x + 1, y + 1, s - 2, s - 2, s / 5, dark, 256);
  gfx_rrect(x + 3, y + 3, s - 6, s - 6, s / 5 - 1, G[i] & F_GOAL ? GREEN : HEX(0xD6943E), 256);
  gfx_fill(x + 3, y + s / 2 - 1, s - 6, 2, dark);
}
/* the panels in the middle (OV_PAUSE to OV_DONE): a title, a few lines, then the choices, the one
   picked in gold */
static const char *const panels[4][6] = {
  {T("Paused"), T("Resume"), T("Restart"), T("Levels"), T("How to play"), T("Quit game")},
  {T("Quit game?"), T("No"), T("Yes")},
  {T("How to play"), T("Play")},
  {T("Solved!"), T("Next level")},
};
static const uint8_t choices[4] = {5, 2, 1, 1}, lines[4] = {0, 0, 4, 1};
static void overlay(void) {
  int n = choices[ov - 1], h = 54 + 16 * lines[ov - 1] + 28 * n, y = (SCREEN_H - h) / 2;
  gfx_fill_alpha(0, 0, SCREEN_W, SCREEN_H, 0, 110);
  gfx_rrect(40, y, 240, h, 12, BAR, 256);
  gfx_text_center(&np_font_title, 160, y + 32, panels[ov - 1][0], GOLD, 256);
  y = gfx_paragraph(&np_font_body, 160, y + 54, 224, 16, ov == OV_HELP ? T("Push every box onto a goal: one at a time, and never pulled.\nArrows: walk and push\nBackspace: undo   Back: pause") : ov == OV_DONE ? msg : "", CREAM, 256) - 12;
  for (int i = 0; i < n; i++, y += 28) {
    if (sel == i) gfx_rrect(96, y, 128, 24, 12, GOLD, 256);
    gfx_text_center(&np_font_body, 160, y + 17, panels[ov - 1][i + 1], sel == i ? BAR : CREAM, 256);
  }
}

static void play_scene(void *ctx) {
  int s = size, sh = s * anim / STEPS, ex = -ddx[face] * sh, ey = -ddy[face] * sh;
  gfx_fill(0, gfx_y0, SCREEN_W, gfx_y1 - gfx_y0, BG);
  for (int i = 0; i < rows * MAXW; i++) {
    int x = ox + i % MAXW * s, y = oy + i / MAXW * s, f = G[i];
    if (y >= gfx_y1 || y + s <= gfx_y0) continue;
    if (f & F_WALL) {
      gfx_fill(x, y, s, s, HEX(0x5B3A26));
      gfx_fill(x, y, s - 1, s - 1, HEX(0xB57D55));
      gfx_fill(x + 1, y + 1, s - 2, s - 2, HEX(0x8C5A3A));
    } else if (f) {
      gfx_fill(x, y, s, s, (i ^ i / MAXW) & 1 ? HEX(0xE4D7BC) : HEX(0xDCCDAF));
      if (f & F_GOAL) gfx_circle(x + s / 2, y + s / 2, s / 5 + 1, HEX(0xD2452F), 256);
      if (f & F_BOX && (!anim || i != moved)) box(i, 0, 0);
    }
  }
  if (anim && moved >= 0) box(moved, ex, ey);
  /* the keeper, looking the way he goes */
  int cx = ox + keeper % MAXW * s + ex + s / 2, cy = oy + keeper / MAXW * s + ey + s / 2, r = s * 3 / 8;
  int lx = ddx[face] * s / 8, ly = ddy[face] * s / 8;
  gfx_circle(cx, cy, r, HEX(0x3C7BE8), 256);
  for (int e = -s / 7; e <= s / 7; e += 2 * (s / 7)) {
    gfx_circle(cx + e + lx, cy - s / 12 + ly, s / 8 + 1, WHITE, 256);
    gfx_circle(cx + e + lx * 3 / 2, cy - s / 12 + ly * 3 / 2, s / 16 + 1, BAR, 256);
  }
  gfx_fill(0, 0, SCREEN_W, HDR, BAR);
  put(line, T("Level "), (uint32_t)level + 1);
  gfx_text(&np_font_body, 8, 16, line, GOLD, 256);
  put(cat(put(line, T("Moves "), (uint32_t)moves), "   "), T("Pushes "), (uint32_t)pushes);
  gfx_text_right(&np_font_body, 312, 16, line, CREAM, 256);
  if (ov) overlay();
}

/* the levels, ten a row, six rows on the screen at a time */
static void hub_scene(void *ctx) {
  char s[8];
  int n = 0;
  gfx_fill(0, 0, SCREEN_W, SCREEN_H, BG);
  gfx_fill(0, 0, SCREEN_W, 36, BAR);
  gfx_text(&np_font_title, 12, 27, "SOKOBAN", GOLD, 256);
  for (int i = 0; i < LEVELS; i++) {
    int x = 12 + 30 * (i % 10), y = 46 + 27 * (i / 10 - top);
    n += sv.best[i] != 0;
    if (y < 40 || y > 190) continue;
    gfx_rrect(x, y, 26, 22, 6, sv.best[i] ? GREEN : unlocked(i) ? CREAM : HEX(0x57505E), 256);
    if (i == pick) gfx_rrect_outline(x - 3, y - 3, 32, 28, 8, 2, GOLD, 256);
    put(s, "", (uint32_t)i + 1);
    gfx_text_center(&np_font_small, x + 13, y + 15, s, sv.best[i] ? WHITE : BG, 256);
  }
  put(cat(put(line, "", (uint32_t)n), " / "), "", LEVELS);
  gfx_text_right(&np_font_body, 308, 24, line, CREAM, 256);
  gfx_fill(0, 214, SCREEN_W, 26, BAR);
  put(line, T("Solved in moves: "), sv.best[pick]);
  if (sv.best[pick]) gfx_text_center(&np_font_body, 160, 232, line, CREAM, 256);
  if (ov) overlay();
}

int main(void) {
  np_app_begin();
  color_t *their_strip = gfx_buf;
  gfx_buf = strip;
  uint32_t len = 0;
  const uint8_t *d = np_file_read(SAVE_NAME, &len);
  for (uint32_t i = 0; d && len == sizeof sv && d[0] == 'S' && d[1] == 1 && i < len; i++) ((uint8_t *)&sv)[i] = d[i];
  if (sv.level >= LEVELS) sv.level = 0;
  pick = sv.level, top = pick / 10 > 3 ? pick / 10 - 3 : 0;
  if (!(sv.flags & 1)) ov = OV_HELP;
  static const uint32_t ways[5] = {KEY(eadk_key_up) | KEY(eadk_key_eight), KEY(eadk_key_right) | KEY(eadk_key_six),
                                   KEY(eadk_key_down) | KEY(eadk_key_two), KEY(eadk_key_left) | KEY(eadk_key_four),
                                   KEY(eadk_key_backspace)};
  const uint32_t ok = KEY(eadk_key_ok) | KEY(eadk_key_exe) | KEY(eadk_key_five), back = KEY(eadk_key_back);
  uint32_t held = keys(), stale = held; /* keys still down from the launcher wait to be let go */
  uint32_t again = 0;
  mark(0, SCREEN_H);
  for (int quit = 0; !quit;) {
    uint32_t now = (uint32_t)eadk_timing_millis();
    uint32_t k = keys(), hit = k & ~held;
    stale &= k, held = k, k &= ~stale;
    if (k & (KEY(eadk_key_home) | KEY(eadk_key_on_off))) break;
    int way = -1, tap = -1; /* the way held, and pressed just now (0 up, 1 right, 2 down, 3 left, 4 undo) */
    for (int i = 0; i < 5; i++) {
      if (k & ways[i]) way = i;
      if (hit & ways[i]) tap = i;
    }
    if (hit && (ov || !scr)) mark(0, SCREEN_H);
    if (ov) {
      int n = choices[ov - 1], c = hit & ok ? sel : hit & back ? -1 : -2;
      if (tap == 0 || tap == 2) sel = (sel + tap - 1 + n) % n;
      if (c > -2) {
        int was = ov;
        ov = OV_NONE, sel = 0, sv.flags |= 1;
        if (was == OV_PAUSE && c == 1) open_level(level);
        if (was == OV_PAUSE && c == 2) scr = 0, pick = level;
        if (was == OV_PAUSE && c > 2) ov = c == 3 ? OV_HELP : OV_QUIT;
        if (was == OV_QUIT) c == 1 ? quit = 1 : scr ? (ov = OV_PAUSE, sel = 4) : 0;
        if (was == OV_DONE) c == 0 && level + 1 < LEVELS ? open_level(level + 1) : (void)(scr = 0, pick = level);
      }
    } else if (!scr) {
      if (tap >= 0 && tap < 4) pick += tap & 1 ? 2 - tap : 10 * (tap - 1);
      pick = pick < 0 ? 0 : pick >= LEVELS ? LEVELS - 1 : pick;
      top = pick / 10 < top ? pick / 10 : pick / 10 > top + 5 ? pick / 10 - 5 : top;
      if (hit & ok && unlocked(pick)) open_level(pick);
      if (hit & back) ov = OV_QUIT;
    } else if (hit & back) {
      ov = OV_PAUSE, sel = 0;
      mark(0, SCREEN_H);
    } else if (!anim && way >= 0 && (tap >= 0 || (int32_t)(now - again) >= 0)) { /* held, it goes on */
      if (way < 4) step(way);
      else if (nh) undo();
      again = now + (tap >= 0 ? 240 : 100);
    }
    if (anim) {
      anim--;
      if (moved >= 0) mark_cell(moved);
      mark_cell(keeper);
      mark(0, HDR);
      if (!anim && !left) { /* solved */
        if (!sv.best[level] || moves < sv.best[level]) sv.best[level] = (uint16_t)moves;
        put(cat(put(msg, T("Moves "), (uint32_t)moves), "   "), T("Your best: "), sv.best[level]);
        ov = OV_DONE;
        save();
        mark(0, SCREEN_H);
      }
    }
    if (dlo < dhi) {
      eadk_display_wait_for_vblank();
      gfx_render(scr ? play_scene : hub_scene, 0, dlo < 0 ? 0 : dlo, dhi > SCREEN_H ? SCREEN_H : dhi);
      dlo = SCREEN_H, dhi = 0;
    } else {
      eadk_timing_msleep(8);
    }
  }
  save();
  gfx_buf = their_strip;
  return np_app_end();
}
