/* The renderer: a draw list rasterised in bands of GFX_BAND rows.
 *
 * Each command keeps its screen bounds (clipped); a band walks the commands
 * that cross it, in order. Sprites are runs of palette indices (res.h), so a
 * band only reads the rows it shows. Alpha has 33 steps (0..32), applied with
 * the usual RGB565 trick. */
#include <string.h>
#include "gfx.h"
#include "res.h"

enum { I_RECT, I_SPRITE, I_BACKDROP, I_TEXT, I_ELLIPSE, I_DARK };

typedef struct {
  uint8_t kind, flags, alpha, amount;
  int16_t x0, y0, x1, y1;      /* screen bounds, clipped: [x0, x1) x [y0, y1) */
  int16_t x, y;                /* sprite: its top-left (scaled); text: its top-left; ellipse: centre */
  int16_t w, h;                /* sprite: its size on screen; ellipse: radii */
  int16_t ref, sub;            /* sprite and frame / text: font and pool offset / backdrop */
  uint16_t color, scale;       /* text: its scale; sprite: x scale */
  uint16_t scale_y;
} Item;

#ifdef HOST
static uint16_t host_band[SCREEN_W * GFX_BAND];
static uint16_t *band = host_band;
#else
static uint16_t *band;
#endif
void gfx_set_band_memory(uint16_t *px) {
#ifdef HOST
  band = px ? px : host_band;
#else
  band = px;
#endif
}

static Item items[GFX_MAX_ITEMS];
static int nitems;
static char pool[GFX_TEXT_POOL];
static int npool;
static int clip_x0, clip_y0, clip_x1 = SCREEN_W, clip_y1 = SCREEN_H;
int gfx_dropped;
static int dropped_now;

uint16_t gfx_mix(uint16_t f, uint16_t b, int a) {
  uint32_t x = (f | (uint32_t)f << 16) & 0x07E0F81F, y = (b | (uint32_t)b << 16) & 0x07E0F81F;
  y = (y + ((x - y) * (uint32_t)a >> 5)) & 0x07E0F81F;
  return (uint16_t)(y | y >> 16);
}

void gfx_begin(void) {
  nitems = 0, npool = 0, dropped_now = 0;
  gfx_clip_all();
}

void gfx_clip(int x, int y, int w, int h) {
  clip_x0 = x < 0 ? 0 : x, clip_y0 = y < 0 ? 0 : y;
  clip_x1 = x + w > SCREEN_W ? SCREEN_W : x + w, clip_y1 = y + h > SCREEN_H ? SCREEN_H : y + h;
}
void gfx_clip_all(void) { clip_x0 = 0, clip_y0 = 0, clip_x1 = SCREEN_W, clip_y1 = SCREEN_H; }

/* a new command over [x0, x1) x [y0, y1), clipped; NULL if it shows nothing */
static Item *add(int kind, int x0, int y0, int x1, int y1) {
  if (x0 < clip_x0) x0 = clip_x0;
  if (y0 < clip_y0) y0 = clip_y0;
  if (x1 > clip_x1) x1 = clip_x1;
  if (y1 > clip_y1) y1 = clip_y1;
  if (x0 >= x1 || y0 >= y1) return NULL;
  if (nitems >= GFX_MAX_ITEMS) {
    dropped_now++;
    return NULL;
  }
  Item *it = &items[nitems++];
  memset(it, 0, sizeof *it);
  it->kind = (uint8_t)kind;
  it->x0 = (int16_t)x0, it->y0 = (int16_t)y0, it->x1 = (int16_t)x1, it->y1 = (int16_t)y1;
  it->alpha = 32;
  return it;
}

static int clamp_alpha(int a) { return a < 0 ? 0 : a > 32 ? 32 : a; }

void gfx_backdrop(int b) {
  Item *it = add(I_BACKDROP, 0, 0, SCREEN_W, SCREEN_H);
  if (it) it->ref = (int16_t)b;
}

void gfx_rect(int x, int y, int w, int h, uint16_t color, int alpha) {
  alpha = clamp_alpha(alpha);
  if (!alpha) return;
  Item *it = add(I_RECT, x, y, x + w, y + h);
  if (it) it->color = color, it->alpha = (uint8_t)alpha;
}

void gfx_ellipse(int cx, int cy, int rx, int ry, uint16_t color, int alpha) {
  alpha = clamp_alpha(alpha);
  if (!alpha || rx <= 0 || ry <= 0) return;
  Item *it = add(I_ELLIPSE, cx - rx, cy - ry, cx + rx + 1, cy + ry + 1);
  if (it) it->x = (int16_t)cx, it->y = (int16_t)cy, it->w = (int16_t)rx, it->h = (int16_t)ry, it->color = color, it->alpha = (uint8_t)alpha;
}

void gfx_dark(int x, int y, int r, int alpha) {
  Item *it = add(I_DARK, 0, 0, SCREEN_W, SCREEN_H);
  if (it) it->x = (int16_t)x, it->y = (int16_t)y, it->w = (int16_t)r, it->alpha = (uint8_t)clamp_alpha(alpha);
}

void gfx_sprite_scaled(int s, int i, int x, int y, int sx, int sy, int flags, uint16_t color, int amount, int alpha) {
  Frame f;
  alpha = clamp_alpha(alpha);
  if (!alpha || sx <= 0 || sy <= 0 || !frame_get(s, i, &f)) return;
  int w = (f.w * sx + 128) >> 8, h = (f.h * sy + 128) >> 8;
  if (w <= 0 || h <= 0) return;
  int ox = (f.ox * sx) >> 8, oy = (f.oy * sy) >> 8;
  int tx = x - ((flags & GFX_FLIPX) ? w - 1 - ox : ox), ty = y - ((flags & GFX_FLIPY) ? h - 1 - oy : oy);
  Item *it = add(I_SPRITE, tx, ty, tx + w, ty + h);
  if (!it) return;
  it->x = (int16_t)tx, it->y = (int16_t)ty, it->w = (int16_t)w, it->h = (int16_t)h;
  it->ref = (int16_t)s, it->sub = (int16_t)i, it->flags = (uint8_t)flags, it->alpha = (uint8_t)alpha;
  it->color = color, it->amount = (uint8_t)clamp_alpha(amount);
  it->scale = (uint16_t)sx, it->scale_y = (uint16_t)sy;
}
void gfx_sprite(int s, int i, int x, int y, int flags, int alpha) { gfx_sprite_scaled(s, i, x, y, 256, 256, flags, 0, 0, alpha); }
void gfx_sprite_tint(int s, int i, int x, int y, int flags, uint16_t color, int amount, int alpha) {
  gfx_sprite_scaled(s, i, x, y, 256, 256, flags, color, amount, alpha);
}

int gfx_text_width(int font, const char *s, int scale) {
  int w = 0, best = 0;
  Glyph g;
  for (; *s; s++) {
    if (*s == '\n') {
      if (w > best) best = w;
      w = 0;
      continue;
    }
    if (glyph_get(font, (uint8_t)*s, &g)) w += g.adv * scale;
  }
  return w > best ? w : best;
}

int gfx_text(int font, int x, int y, const char *s, uint16_t color, int scale, bool outline) {
  int n = (int)strlen(s), w = gfx_text_width(font, s, scale), lines = 1;
  for (const char *p = s; *p; p++) lines += *p == '\n';
  if (npool + n + 1 > GFX_TEXT_POOL) return w;
  int pad = outline ? 1 : 0;
  Item *it = add(I_TEXT, x - pad, y - pad - 4 * scale, x + w + pad, y + lines * font_height(font) * scale + pad + 2 * scale);
  if (!it) return w;
  memcpy(pool + npool, s, (size_t)n + 1);
  it->x = (int16_t)x, it->y = (int16_t)y, it->ref = (int16_t)font, it->sub = (int16_t)npool;
  it->color = color, it->scale = (uint16_t)scale, it->flags = outline;
  npool += n + 1;
  return w;
}
void gfx_text_c(int font, int x, int y, const char *s, uint16_t color, int scale, bool outline) {
  gfx_text(font, x - gfx_text_width(font, s, scale) / 2, y, s, color, scale, outline);
}

/* ---------------------------------------------------------------- rasterising */
static int by0, by1;   /* the band: rows [by0, by1) */
static inline uint16_t *px_at(int x, int y) { return band + (y - by0) * SCREEN_W + x; }

static void r_rect(const Item *it, int y0, int y1) {
  for (int y = y0; y < y1; y++) {
    uint16_t *p = px_at(it->x0, y);
    int n = it->x1 - it->x0;
    if (it->alpha >= 32)
      for (int i = 0; i < n; i++) p[i] = it->color;
    else
      for (int i = 0; i < n; i++) p[i] = gfx_mix(it->color, p[i], it->alpha);
  }
}

static void r_ellipse(const Item *it, int y0, int y1) {
  int rx = it->w, ry = it->h;
  for (int y = y0; y < y1; y++) {
    int dy = y - it->y;
    /* the half width at this row: rx * sqrt(1 - (dy / ry)^2) */
    int32_t t = (int32_t)ry * ry - dy * dy;
    if (t < 0) continue;
    int hw = 0;
    while ((hw + 1) * (hw + 1) * ry * ry <= t * rx * rx) hw++;
    int a = it->x - hw, b = it->x + hw + 1;
    if (a < it->x0) a = it->x0;
    if (b > it->x1) b = it->x1;
    uint16_t *p = px_at(0, y);
    for (int x = a; x < b; x++) p[x] = gfx_mix(it->color, p[x], it->alpha);
  }
}

static void r_dark(const Item *it, int y0, int y1) {
  int r2 = it->w * it->w, soft = (it->w + 24) * (it->w + 24);
  for (int y = y0; y < y1; y++) {
    uint16_t *p = px_at(0, y);
    int dy = y - it->y;
    for (int x = 0; x < SCREEN_W; x++) {
      int dx = x - it->x, d = dx * dx + dy * dy;
      if (d <= r2) continue;
      int a = d >= soft ? it->alpha : it->alpha * (d - r2) / (soft - r2);
      p[x] = gfx_mix(0, p[x], a);
    }
  }
}

static void r_backdrop(const Item *it, int y0, int y1) {
  Backdrop bd;
  if (!backdrop_get(it->ref, &bd)) return;
  /* the room 2 w x 2 h, its quarter mirrored; the screen shows it from x = w - 160 */
  int off = bd.w * 2 - SCREEN_W;   /* (40: 20 each side) */
  for (int y = y0; y < y1; y++) {
    int qy = y < bd.h ? y : 2 * bd.h - 1 - y;
    if (qy < 0) qy = 0;
    const uint8_t *row = bd.px + qy * bd.w;
    uint16_t *p = px_at(0, y);
    int x = 0, half = bd.w - off / 2;   /* screen columns of the left half */
    for (; x < half; x++) p[x] = bd.pal[row[x + off / 2]];
    for (; x < SCREEN_W; x++) p[x] = bd.pal[row[2 * bd.w - 1 - (x + off / 2)]];
  }
}

/* a sprite's row decoded into indices (255: nothing) */
static uint8_t line[512];
static void decode_row(const Frame *f, int r, int w) {
  memset(line, 255, (size_t)w);
  const uint8_t *p = f->rows + f->rowoff[r];
  int n = *p++, x = 0;
  while (n--) {
    x += *p++;
    int len = *p++;
    for (int i = 0; i < len && x < w; i++) line[x++] = *p++;
    if (x >= w) break;
  }
}

static void r_sprite(const Item *it, int y0, int y1) {
  Frame f;
  if (!frame_get(it->ref, it->sub, &f)) return;
  bool scaled = it->scale != 256 || it->scale_y != 256;
  bool flipx = it->flags & GFX_FLIPX, flipy = it->flags & GFX_FLIPY;
  int alpha = it->alpha, amount = it->amount;
  uint16_t tint = it->color;
  for (int y = y0; y < y1; y++) {
    int sy = y - it->y;
    if (scaled) sy = sy * 256 / it->scale_y;
    if (flipy) sy = f.h - 1 - sy;
    if (sy < 0 || sy >= f.h) continue;
    uint16_t *p = px_at(0, y);
    if (!scaled) {
      const uint8_t *q = f.rows + f.rowoff[sy];
      int n = *q++, x = 0;
      while (n--) {
        x += *q++;
        int len = *q++;
        for (int i = 0; i < len; i++, x++) {
          uint8_t c = *q++;
          int dx = flipx ? it->x + f.w - 1 - x : it->x + x;
          if (dx < it->x0 || dx >= it->x1) continue;
          int a = f.alpha[c] * alpha >> 5;
          uint16_t col = f.pal[c];
          if (amount) col = gfx_mix(tint, col, amount);
          p[dx] = a >= 32 ? col : gfx_mix(col, p[dx], a);
        }
      }
    } else {
      decode_row(&f, sy, f.w);
      for (int dx = it->x0; dx < it->x1; dx++) {
        int sx = (dx - it->x) * 256 / it->scale;
        if (flipx) sx = f.w - 1 - sx;
        if (sx < 0 || sx >= f.w) continue;
        uint8_t c = line[sx];
        if (c == 255) continue;
        int a = f.alpha[c] * alpha >> 5;
        uint16_t col = f.pal[c];
        if (amount) col = gfx_mix(tint, col, amount);
        p[dx] = a >= 32 ? col : gfx_mix(col, p[dx], a);
      }
    }
  }
}

static void r_glyphs(const Item *it, int y0, int y1, int ox, int oy, uint16_t color) {
  const char *s = pool + it->sub;
  int sc = it->scale, x = it->x + ox, y = it->y + oy, fh = font_height(it->ref);
  Glyph g;
  for (; *s; s++) {
    if (*s == '\n') {
      x = it->x + ox, y += fh * sc;
      continue;
    }
    if (!glyph_get(it->ref, (uint8_t)*s, &g)) continue;
    int gx = x + g.ox * sc, gy = y + g.oy * sc, stride = (g.w + 7) >> 3;
    for (int r = 0; r < g.h; r++)
      for (int k = 0; k < sc; k++) {
        int yy = gy + r * sc + k;
        if (yy < y0 || yy >= y1) continue;
        uint16_t *p = px_at(0, yy);
        const uint8_t *bits = g.bits + r * stride;
        for (int c = 0; c < g.w; c++)
          if (bits[c >> 3] & (0x80 >> (c & 7)))
            for (int m = 0; m < sc; m++) {
              int xx = gx + c * sc + m;
              if (xx >= it->x0 && xx < it->x1) p[xx] = color;
            }
      }
    x += g.adv * sc;
  }
}
static void r_text(const Item *it, int y0, int y1) {
  if (it->flags) {
    static const int8_t d[8][2] = {{-1, 0}, {1, 0}, {0, -1}, {0, 1}, {-1, -1}, {1, -1}, {-1, 1}, {1, 1}};
    for (int k = 0; k < 8; k++) r_glyphs(it, y0, y1, d[k][0], d[k][1], C_BLACK);
  }
  r_glyphs(it, y0, y1, 0, 0, it->color);
}

void gfx_end(void) {
  gfx_dropped = dropped_now;
  for (by0 = 0; by0 < SCREEN_H; by0 += GFX_BAND) {
    by1 = by0 + GFX_BAND;
    for (int i = 0; i < nitems; i++) {
      const Item *it = &items[i];
      if (it->y1 <= by0 || it->y0 >= by1) continue;
      int y0 = it->y0 > by0 ? it->y0 : by0, y1 = it->y1 < by1 ? it->y1 : by1;
      switch (it->kind) {
        case I_RECT: r_rect(it, y0, y1); break;
        case I_SPRITE: r_sprite(it, y0, y1); break;
        case I_BACKDROP: r_backdrop(it, y0, y1); break;
        case I_TEXT: r_text(it, y0, y1); break;
        case I_ELLIPSE: r_ellipse(it, y0, y1); break;
        case I_DARK: r_dark(it, y0, y1); break;
      }
    }
    plat_push(0, by0, SCREEN_W, GFX_BAND, band);
  }
}
