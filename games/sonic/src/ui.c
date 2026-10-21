/* What is drawn over the game's picture: the pause menu, the title's Continue / New game, the keys
 * screen. Text is the original's level select font (8x8: digits, capitals, $ - = >, and a few signs
 * drawn here in its style), at 1x or 2x, on boxes; it goes into each 16-line band of the picture as
 * vdp_render() pushes it, so nothing flickers. */
#include "s1.h"
#include "ui.h"

#define NBOX 6
#define NTEXT 28
static struct {
  int16_t x, y, w, h;
  uint16_t c;
} boxes[NBOX];
static struct {
  int16_t x, y;
  uint16_t c;
  uint8_t scale;
  const char *s;
} texts[NTEXT];
static int nbox, ntext;
bool ui_active;

void ui_clear(void) { nbox = ntext = 0, ui_active = false; }
void ui_box(int x, int y, int w, int h, uint16_t c) {
  if (nbox < NBOX) boxes[nbox].x = (int16_t)x, boxes[nbox].y = (int16_t)y, boxes[nbox].w = (int16_t)w,
                   boxes[nbox].h = (int16_t)h, boxes[nbox].c = c, nbox++;
  ui_active = true;
}
void ui_text(int x, int y, uint16_t c, int scale, const char *s) {
  if (ntext < NTEXT) texts[ntext].x = (int16_t)x, texts[ntext].y = (int16_t)y, texts[ntext].c = c,
                     texts[ntext].scale = (uint8_t)scale, texts[ntext].s = s, ntext++;
  ui_active = true;
}
int ui_width(const char *s, int scale) { return (int)strlen(s) * 8 * scale; }
void ui_text_centred(int y, uint16_t c, int scale, const char *s) { ui_text((320 - ui_width(s, scale)) / 2, y, c, scale, s); }

/* signs the font lacks, one byte a row (the first pixel the high bit) */
static const struct {
  char ch;
  uint8_t rows[8];
} extra[] = {
  {'.', {0, 0, 0, 0, 0, 0, 0x60, 0x60}},  {',', {0, 0, 0, 0, 0, 0x30, 0x30, 0x60}},
  {':', {0, 0, 0x30, 0x30, 0, 0x60, 0x60, 0}}, {'!', {0, 0x0C, 0x18, 0x18, 0x30, 0x30, 0, 0x60}},
  {'?', {0, 0x3C, 0x66, 0x0C, 0x18, 0x30, 0, 0x30}}, {'/', {0, 0x06, 0x0C, 0x18, 0x30, 0x60, 0xC0, 0}},
  {'\'', {0, 0x18, 0x18, 0x30, 0, 0, 0, 0}}, {'+', {0, 0, 0x18, 0x18, 0x7E, 0x30, 0x30, 0}},
};

/* the glyph's row r as 8 bits (the first pixel the high bit) */
static uint8_t glyph_row(char ch, int r) {
  int t = -1;
  if (ch >= '0' && ch <= '9') t = ch - '0';
  else if (ch == '$') t = 0xA;
  else if (ch == '-') t = 0xB;
  else if (ch == '=') t = 0xC;
  else if (ch == '>') t = 0xD;
  else if (ch == 'Y' || ch == 'Z') t = 0xF + ch - 'Y';
  else if (ch >= 'A' && ch <= 'X') t = 0x11 + ch - 'A';
  else if (ch >= 'a' && ch <= 'z') return glyph_row((char)(ch - 32), r);
  if (t < 0) {
    for (unsigned i = 0; i < sizeof extra / sizeof extra[0]; i++)
      if (extra[i].ch == ch) return extra[i].rows[r];
    return 0;
  }
  const uint8_t *p = (const uint8_t *)DATA(D_TILES) + ((uint32_t)DATAW(D_ARTS)[ART_Art_Text * 2] + t) * 32 + r * 4;
  uint8_t b = 0;
  for (int i = 0; i < 4; i++) {
    if (p[i] >> 4) b |= (uint8_t)(0x80 >> (i * 2));
    if (p[i] & 15) b |= (uint8_t)(0x40 >> (i * 2));
  }
  return b;
}

static void text_band(uint16_t *band, int y0, int x, int y, uint16_t c, int scale, const char *s, bool shadow) {
  int h = 8 * scale;
  if (y + h <= y0 || y >= y0 + 16) return;
  for (; *s; s++, x += 8 * scale) {
    if (*s == ' ') continue;
    for (int py = 0; py < h; py++) {
      int sy = y + py;
      if (sy < y0 || sy >= y0 + 16) continue;
      uint8_t bits = glyph_row(*s, py / scale);
      if (!bits) continue;
      uint16_t *row = band + (sy - y0) * 320;
      for (int px = 0; px < 8 * scale; px++) {
        if (!(bits & (0x80 >> (px / scale)))) continue;
        int sx = x + px;
        if (shadow) {
          if (sx + 1 < 320 && sy + 1 < y0 + 16) row[320 + sx + 1] = 0;
          continue;
        }
        if ((unsigned)sx < 320) row[sx] = c;
      }
    }
  }
}

/* into the band of the picture's lines y0..y0+15 */
void ui_draw_band(uint16_t *band, int y0) {
  for (int i = 0; i < nbox; i++) {
    int ya = boxes[i].y > y0 ? boxes[i].y : y0, yb = boxes[i].y + boxes[i].h < y0 + 16 ? boxes[i].y + boxes[i].h : y0 + 16;
    for (int y = ya; y < yb; y++)
      for (int x = boxes[i].x; x < boxes[i].x + boxes[i].w; x++)
        if ((unsigned)x < 320) band[(y - y0) * 320 + x] = boxes[i].c;
  }
  for (int i = 0; i < ntext; i++)
    text_band(band, y0, texts[i].x, texts[i].y, 0, texts[i].scale, texts[i].s, true);
  for (int i = 0; i < ntext; i++)
    text_band(band, y0, texts[i].x, texts[i].y, texts[i].c, texts[i].scale, texts[i].s, false);
}
