/* Text typed a letter at a time, and the dialogue box (see text.h). */
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include "gfx.h"
#include "res.h"
#include "text.h"
#include "world.h"

char g_name[8] = "Frisk";

/* ---------------------------------------------------------------- faces */
/* each expression: its sprite (2 frames: the second one talks), drawn at (x, y) in the box; Toriel's
 * heads sit on her shoulders */
typedef struct {
  uint16_t spr;
  int8_t x, y;
} Expr;
static const Expr toriel_faces[] = {
  {SPR_FACE_TORIELTALK, 9, 0},       {SPR_FACE_TORIELTALKSIDE, 9, 0}, {SPR_FACE_TORIELHAPPYTALK, 9, 0},
  {SPR_FACE_TORIELWHAT, 9, 0},       {SPR_FACE_TORIELWHATSIDE, 9, 0}, {SPR_FACE_TORIELMAD, 9, 0},
  {SPR_FACE_TORIELCOLD, 9, 0},       {SPR_FACE_TORIELSURPRISED, 9, 0}, {SPR_FACE_TORIELEMBARRASSED, 9, 0},
  {SPR_FACE_TORIELGLASSES, 9, 0},    {SPR_FACE_TORIELREVENGETALK, 9, 0}, {SPR_FACE_TORIELSCARED, 9, 0},
  {SPR_FACE_TORIELBLINK, 9, 0},      {SPR_FACE_TORIELHAPPYBLINK, 9, 0},
};
static const Expr flowey_faces[] = {
  {SPR_FLOWEYNICE, 4, -2}, {SPR_FLOWEYNICESIDE, 4, -2}, {SPR_FLOWEYSASSY, 4, -2}, {SPR_FLOWEYEVIL, 4, -2},
  {SPR_FLOWEYGRIN, 4, -2}, {SPR_FLOWEYPLAIN, 4, -2},    {SPR_FLOWEYPISSED, 4, -2}, {SPR_FLOWEYWINK, 4, -2},
  {SPR_FLOWEYLAUGH, 4, -2}, {SPR_FLOWEYHURT, 4, -2},    {SPR_FLOWEYTORIEL, 4, -2}, {SPR_FLOWEYTORIEL2, 4, -2},
  {SPR_FLOWEYSIDESHOCK, 4, -2},
};
static const Expr sans_faces[] = {{SPR_FACE_SANS, 4, 0}, {SPR_FACE_SANSCHUCKLE, 4, 0}, {SPR_FACE_SANSWINK, 4, 0}};
static const Expr papyrus_faces[] = {
  {SPR_FACE_PAPYRUS, 4, -4}, {SPR_FACE_PAPYRUSEVIL, 4, -4}, {SPR_FACE_PAPYRUSLAUGH, 4, -4},
  {SPR_FACE_PAPYRUSMAD, 4, -4}, {SPR_FACE_PAPYRUSSIDE, 4, -4},
};
static const struct {
  const Expr *e;
  int n;
} faces[FACE_MAX] = {
  [FACE_TORIEL] = {toriel_faces, sizeof toriel_faces / sizeof toriel_faces[0]},
  [FACE_FLOWEY] = {flowey_faces, sizeof flowey_faces / sizeof flowey_faces[0]},
  [FACE_SANS] = {sans_faces, sizeof sans_faces / sizeof sans_faces[0]},
  [FACE_PAPYRUS] = {papyrus_faces, sizeof papyrus_faces / sizeof papyrus_faces[0]},
};

/* ---------------------------------------------------------------- the writer */
#define MAX_LINES 8
static uint16_t breaks[MAX_LINES];   /* where the page's lines wrap (src offsets), the line after indented */
static int nbreaks;
static int wrap_width = 270;

static int page_len(const char *s) {
  int n = 0;
  while (s[n] && s[n] != '|') n++;
  return n;
}

/* the width of a letter, 0 for the marks */
static int code_len(const char *s) {
  switch (s[0]) {
    case '^': case '`': case '#': return s[1] ? 2 : 1;
    case '~': return 1;
    default: return 0;
  }
}

/* where the page's lines wrap: at the last space before the box's edge */
static void layout(Writer *w) {
  nbreaks = 0;
  const char *s = w->src + w->page;
  int x = 0, last_space = -1, start = 0;
  for (int i = 0; i < w->len; i++) {
    int c = code_len(s + i);
    if (c) {
      i += c - 1;
      continue;
    }
    if (s[i] == '@') break;
    if (s[i] == '\n') {
      x = 0, last_space = -1, start = i + 1;
      continue;
    }
    if (s[i] == ' ') last_space = i;
    int adv = s[i] == '$' ? 8 * (int)strlen(g_name) : 8;
    x += adv;
    if (x > wrap_width && last_space > start && nbreaks < MAX_LINES) {
      breaks[nbreaks++] = (uint16_t)(w->page + last_space);
      /* the line after: indented under the first's words when the page starts with "* " */
      x = (s[0] == '*' ? 16 : 0);
      for (int k = last_space + 1; k <= i; k++)
        if (!code_len(s + k)) x += 8;
      start = last_space + 1;
      last_space = -1;
    }
  }
}

static uint8_t expr_of(char c) { return (uint8_t)(c >= 'a' ? 10 + c - 'a' : c - '0'); }

static void page_begin(Writer *w) {
  w->len = page_len(w->src + w->page);
  w->shown = 0, w->pause = 0, w->t = 0, w->done = false, w->auto_close = false;
  w->choices = 0, w->choice = 0;
  for (int i = 0; i < w->len; i++)
    if (w->src[w->page + i] == '@') {
      w->choice_at = w->page + i;
      w->choices = 2;
      break;
    }
  layout(w);
}

void writer_start(Writer *w, const char *s, int x, int y, int font) {
  memset(w, 0, sizeof *w);
  w->src = s, w->x = x, w->y = y, w->font = font;
  w->line_h = 18, w->speed = 1;
  page_begin(w);
}

static void show_all(Writer *w) {
  const char *s = w->src + w->page;
  while (w->shown < w->len && s[w->shown] != '@') {
    int c = code_len(s + w->shown);
    if (c && s[w->shown] == '`') w->face_expr = expr_of(s[w->shown + 1]);
    if (c && s[w->shown] == '~') w->auto_close = true;
    w->shown += c ? c : 1;
  }
  w->done = true;
}

bool writer_update(Writer *w, uint32_t pressed) {
  if (w->finished) return false;
  const char *s = w->src + w->page;
  if (!w->done) {
    if (pressed & K_X) {
      show_all(w);
    } else if (w->pause > 0) {
      w->pause--;
    } else if (++w->t >= w->speed) {
      w->t = 0;
      /* the marks take no time; the next letter shows */
      for (;;) {
        if (w->shown >= w->len || s[w->shown] == '@') {
          w->done = true;
          break;
        }
        int c = code_len(s + w->shown);
        if (!c) {
          w->shown++;
          break;
        }
        if (s[w->shown] == '^') w->pause = (s[w->shown + 1] - '0') * 5;
        if (s[w->shown] == '`') w->face_expr = expr_of(s[w->shown + 1]);
        if (s[w->shown] == '~') w->auto_close = true;
        w->shown += c;
        if (w->pause) break;
      }
      if (w->shown >= w->len) w->done = true;
    }
    return true;
  }
  if (w->choices) {
    if (pressed & K_LEFT) w->choice = 0;
    if (pressed & K_RIGHT) w->choice = 1;
  }
  if ((pressed & K_Z) || w->auto_close) {
    const char *next = s + w->len;
    if (*next == '|') {
      w->page += w->len + 1;
      page_begin(w);
    } else {
      w->finished = true;
      return false;
    }
  }
  return true;
}

int writer_talking(const Writer *w) { return !w->done && !w->pause; }

static uint16_t color_of(char c) {
  switch (c) {
    case 'Y': return rgb565(0xFFFF00);
    case 'R': return rgb565(0xFF0000);
    case 'B': return rgb565(0x0000FF);
    case 'O': return rgb565(0xFFA040);
    case 'G': return rgb565(0x00C000);
    case 'P': return rgb565(0xD535D9);
    case 'L': return rgb565(0x0EC0FD);
    default: return C_WHITE;
  }
}

void writer_draw(const Writer *w, uint16_t color) {
  if (w->finished) return;
  char seg[80];
  int n = 0, x = w->x, y = w->y, sx = x;
  uint16_t col = color, segcol = color;
  const char *s = w->src + w->page;
  int b = 0;
#define FLUSH() do { if (n) { seg[n] = 0; gfx_text_ex(w->font, sx, y, seg, n, segcol, w->shake, (uint32_t)(y * 31 + sx), 0); n = 0; } } while (0)
  for (int i = 0; i < w->shown && i < w->len; i++) {
    if (s[i] == '@') break;
    int c = code_len(s + i);
    if (c) {
      if (s[i] == '#') {
        FLUSH();
        col = s[i + 1] == 'X' || s[i + 1] == 'W' ? color : color_of(s[i + 1]);
      }
      i += c - 1;
      continue;
    }
    if (s[i] == '\n' || (b < nbreaks && w->page + i == breaks[b])) {
      FLUSH();
      if (s[i] != '\n') b++;
      y += w->line_h;
      x = w->x + (s[i] != '\n' && s[0] == '*' ? 16 : 0);
      continue;
    }
    if (n == 0) sx = x, segcol = col;
    if (col != segcol) {
      FLUSH();
      sx = x, segcol = col;
    }
    if (s[i] == '$') {
      for (const char *p = g_name; *p && n < 70; p++) seg[n++] = *p, x += 8;
      continue;
    }
    seg[n++] = s[i];
    x += 8;
    if (n >= 70) FLUSH();
  }
  FLUSH();
#undef FLUSH
  if (w->choices && w->done) {
    /* the two options on the next line, the heart by the one selected */
    const char *a = w->src + w->choice_at + 1, *bb = strchr(a, '@');
    int la = bb ? (int)(bb - a) : 0, lb = bb ? page_len(bb + 1) : 0;
    int cy = y + w->line_h + 4;
    int ax = w->x + 56, bx = w->x + 168;
    gfx_text_ex(w->font, ax, cy, a, la, C_WHITE, 0, 0, 0);
    if (bb) gfx_text_ex(w->font, bx, cy, bb + 1, lb, C_WHITE, 0, 0, 0);
    gfx_sprite(SPR_HEART, 0, (w->choice ? bx : ax) - 12, cy + 6, 0, 255);
  }
}

/* ---------------------------------------------------------------- the dialogue box */
static Writer box;
static bool box_open;
static int box_face;
static int box_top = -1, box_at_top;
static int last_choice;
static char fmt_buf[512];

void text_set_top(int top) { box_top = top; }

void text_open(int face, const char *s) {
  box_face = face;
  int tx = face ? 76 : 22;
  wrap_width = face ? 300 - tx - 6 : 300 - tx - 8;
  box_at_top = box_top >= 0 ? box_top : (pl.y + 15 - cam_y > 130);
  writer_start(&box, s, tx, (box_at_top ? 5 : 160) + 11, FONT_MAIN);
  box_open = true;
}

void text_openf(int face, const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(fmt_buf, sizeof fmt_buf, fmt, ap);
  va_end(ap);
  text_open(face, fmt_buf);
}

bool text_active(void) { return box_open; }
int text_choice(void) { return last_choice; }

void text_update(uint32_t pressed) {
  if (!box_open) return;
  int ch = box.choice;
  bool had = box.choices && box.done;
  if (!writer_update(&box, pressed)) {
    box_open = false;
    if (had) last_choice = ch;
  }
}

void text_draw(void) {
  if (!box_open) return;
  int y = box_at_top ? 5 : 160;
  gfx_rect(16, y, 288, 76, C_WHITE, 255);
  gfx_rect(19, y + 3, 282, 70, C_BLACK, 255);
  if (box_face && faces[box_face].e) {
    int e = box.face_expr < faces[box_face].n ? box.face_expr : 0;
    const Expr *x = &faces[box_face].e[e];
    int fr = writer_talking(&box) ? (box.shown / 2) & 1 : 0;
    if (box_face == FACE_TORIEL) gfx_sprite(SPR_FACE_TORIELBODY, 0, 26, y + 13 + 32, 0, 255);
    gfx_sprite(x->spr, fr, 26 + x->x, y + 13 + x->y, 0, 255);
  }
  writer_draw(&box, C_WHITE);
}
