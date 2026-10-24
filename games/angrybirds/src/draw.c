/* Drawing (draw.h): the game's pictures into the launcher's strip (gfx_buf, rows gfx_y0 to gfx_y1). */
#include <math.h>
#include <stdlib.h>
#include "draw.h"
#include "game.h"
#include "world.h"

#define PIX_PER_M 8.0f   /* the object pictures' scale (tools/art.py) */

static inline int clampi(int v, int lo, int hi) { return v < lo ? lo : v > hi ? hi : v; }

void draw_pic(int pic, int x, int y) {
  const Pic *p = res_pic(pic);
  int y0 = y > gfx_y0 ? y : gfx_y0, y1 = y + p->h < gfx_y1 ? y + p->h : gfx_y1;
  int x0 = x > 0 ? x : 0, x1 = x + p->w < SCREEN_W ? x + p->w : SCREEN_W;
  if (y0 >= y1 || x0 >= x1) return;
  const uint16_t *pal = res_pic_palette(p);
  const uint8_t *px = res_pic_pixels(p);
  bool opaque = p->flags & 1;
  for (int yy = y0; yy < y1; yy++) {
    uint32_t row = (uint32_t)(yy - y) * p->w;
    color_t *d = gfx_buf + (yy - gfx_y0) * SCREEN_W;
    for (int xx = x0; xx < x1; xx++) {
      uint32_t at = row + (uint32_t)(xx - x);
      unsigned k = (px[at >> 1] >> ((at & 1) * 4)) & 15;
      if (k || opaque) d[xx] = pal[k];
    }
  }
}

void draw_pic_scaled(int pic, int x, int y, int w, int h) {
  if (w <= 0 || h <= 0) return;
  const Pic *p = res_pic(pic);
  int y0 = y > gfx_y0 ? y : gfx_y0, y1 = y + h < gfx_y1 ? y + h : gfx_y1;
  int x0 = x > 0 ? x : 0, x1 = x + w < SCREEN_W ? x + w : SCREEN_W;
  if (y0 >= y1 || x0 >= x1) return;
  const uint16_t *pal = res_pic_palette(p);
  const uint8_t *px = res_pic_pixels(p);
  bool opaque = p->flags & 1;
  uint32_t du = ((uint32_t)p->w << 16) / (uint32_t)w, dv = ((uint32_t)p->h << 16) / (uint32_t)h;
  for (int yy = y0; yy < y1; yy++) {
    uint32_t v = ((uint32_t)(yy - y) * dv) >> 16;
    if (v >= p->h) v = p->h - 1;
    uint32_t row = v * p->w, u = (uint32_t)(x0 - x) * du;
    color_t *d = gfx_buf + (yy - gfx_y0) * SCREEN_W;
    for (int xx = x0; xx < x1; xx++, u += du) {
      uint32_t at = row + (u >> 16);
      unsigned k = (px[at >> 1] >> ((at & 1) * 4)) & 15;
      if (k || opaque) d[xx] = pal[k];
    }
  }
}

/* nearest neighbour: each screen pixel takes the picture's pixel under it (turned back) */
void draw_pic_at(int pic, float sx, float sy, float scale, float sn, float cs) {
  if (pic < 0 || pic >= PICTURE_COUNT || scale < 0.02f) return;
  const Pic *p = res_pic(pic);
  float px = p->px4 / 4.0f, py = p->py4 / 4.0f;
  /* the box the turned picture takes */
  float ex = fmaxf(px, p->w - px), ey = fmaxf(py, p->h - py);
  float r = sqrtf(ex * ex + ey * ey) * scale + 1;
  int y0 = (int)floorf(sy - r), y1 = (int)ceilf(sy + r), x0 = (int)floorf(sx - r), x1 = (int)ceilf(sx + r);
  if (y1 <= gfx_y0 || y0 >= gfx_y1 || x1 <= 0 || x0 >= SCREEN_W) return;
  y0 = clampi(y0, gfx_y0, gfx_y1), y1 = clampi(y1, gfx_y0, gfx_y1);
  x0 = clampi(x0, 0, SCREEN_W), x1 = clampi(x1, 0, SCREEN_W);
  const uint16_t *pal = res_pic_palette(p);
  const uint8_t *pix = res_pic_pixels(p);
  float inv = 1 / scale;
  /* picture coordinates (16.16) at (x0 + 0.5, y + 0.5), and their steps along x and y */
  int32_t dux = (int32_t)lroundf(cs * inv * 65536), dvx = (int32_t)lroundf(-sn * inv * 65536);
  int32_t duy = (int32_t)lroundf(sn * inv * 65536), dvy = (int32_t)lroundf(cs * inv * 65536);
  float fx = x0 + 0.5f - sx, fy = y0 + 0.5f - sy;
  int32_t u0 = (int32_t)lroundf(((fx * cs + fy * sn) * inv + px) * 65536);
  int32_t v0 = (int32_t)lroundf(((-fx * sn + fy * cs) * inv + py) * 65536);
  uint32_t w = p->w, h = p->h;
  for (int yy = y0; yy < y1; yy++, u0 += duy, v0 += dvy) {
    color_t *d = gfx_buf + (yy - gfx_y0) * SCREEN_W;
    int32_t u = u0, v = v0;
    for (int xx = x0; xx < x1; xx++, u += dux, v += dvx) {
      uint32_t iu = (uint32_t)u >> 16, iv = (uint32_t)v >> 16;
      if (iu >= w || iv >= h) continue;
      uint32_t at = iv * w + iu;
      unsigned k = (pix[at >> 1] >> ((at & 1) * 4)) & 15;
      if (k) d[xx] = pal[k];
    }
  }
}

void draw_line(int x0, int y0, int x1, int y1, int t, color_t c) {
  /* a thick line as a run of small squares: short lines only (the slingshot's bands) */
  int dx = x1 - x0, dy = y1 - y0, n = abs(dx) > abs(dy) ? abs(dx) : abs(dy);
  if (n > 400) return;
  for (int i = 0; i <= n; i++) {
    int x = x0 + (n ? dx * i / n : 0), y = y0 + (n ? dy * i / n : 0);
    gfx_fill(x - t / 2, y - t / 2, t, t, c);
  }
}

float draw_sx(const View *v, float x) { return SCREEN_W / 2 + (x - v->cx) * v->zoom; }
float draw_sy(const View *v, float y) { return SCREEN_H / 2 + (y - v->cy) * v->zoom; }

/* ------------------------------------------------------------------ the level */
/* a layer of the theme (the game's YDb): its bottom at height y of the game's pixels above the ground, moved across
 * at its parallax, repeated across or not, as big as the world is drawn */
static void layer(const View *v, const Layer *L) {
  const Pic *p = res_pic(L->pic);
  float s = v->zoom / 20 / L->q;
  int w = (int)lroundf(p->w * s), h = (int)lroundf(p->h * s);
  if (w <= 0 || h <= 0) return;
  int bottom = (int)lroundf(draw_sy(v, -L->y / 20));
  int top = bottom - h;
  if (bottom <= gfx_y0 || top >= gfx_y1) return;
  float left = SCREEN_W / 2 + (L->x / 20 - v->cx * L->parallax) * v->zoom;
  if (!(L->flags & 2)) {
    draw_pic_scaled(L->pic, (int)lroundf(left), top, w, h);
    return;
  }
  int x = (int)lroundf(fmodf(left, (float)w));
  if (x > 0) x -= w;
  for (; x < SCREEN_W; x += w) draw_pic_scaled(L->pic, x, top, w, h);
}

static void background(const View *v) {
  const Background *bg = res_background(res_level(g.level)->theme);
  gfx_fill(0, gfx_y0, SCREEN_W, gfx_y1 - gfx_y0, bg->sky);
  for (int k = 0; k < bg->layers; k++)
    if (!(bg->layer[k].flags & 1)) layer(v, &bg->layer[k]);
}

static void foreground(const View *v) {
  const Background *bg = res_background(res_level(g.level)->theme);
  int ground = (int)lroundf(draw_sy(v, 0));
  if (ground < gfx_y1) {
    int y = ground > gfx_y0 ? ground : gfx_y0;
    gfx_fill(0, y, SCREEN_W, gfx_y1 - y, bg->ground);
  }
  for (int k = 0; k < bg->layers; k++)
    if (bg->layer[k].flags & 1) layer(v, &bg->layer[k]);
}

static int frame_of(int i) {
  const Obj *o = &g.obj[i];
  const TypeArt *a = res_type_art(o->type);
  if (a->first == 0xFFFF) return -1;
  bool is_bird = o->cls >= C_RED && o->cls <= C_EAGLE;
  if (is_bird) {
    bool launched = !(o->flags & (O_WAITING | O_ON_SLING));
    if (launched && (o->flags & O_USED) && a->power != 0xFFFF) return a->power;
    if (o->health < o->max_health && a->frames > 1) return a->first + 1;
    if (launched && a->flying != 0xFFFF) return a->flying;
    return a->first;
  }
  if (a->frames > 1 && o->health < o->max_health && o->max_health > 0) {
    int f = (int)((1.0f - (float)o->health / (float)o->max_health) * a->frames);
    if (f >= a->frames) f = a->frames - 1;
    if (f < 1) f = 1;
    return a->first + f;
  }
  return a->first;
}

static void object(const View *v, int i) {
  const Obj *o = &g.obj[i];
  float x, y, s, c;
  if (o->flags & O_SCENERY) {
    x = o->jx, y = o->jy, s = sinf(o->ja), c = cosf(o->ja);
  } else if (!world_xf(i, &x, &y, &s, &c)) {
    return;
  }
  int pic = frame_of(i);
  if (pic < 0) return;
  draw_pic_at(pic, draw_sx(v, x), draw_sy(v, y), v->zoom / PIX_PER_M, s, c);
}

static void slingshot_back(const View *v) {
  const Pic *p = res_pic(P_SLINGSHOT_01_BACK);
  float scale = v->zoom / PIX_PER_M;
  float sx = draw_sx(v, g.sling_x - 0.55f), sy = draw_sy(v, g.sling_y + 8.5f);
  draw_pic_scaled(P_SLINGSHOT_01_BACK, (int)lroundf(sx - p->w * scale / 2), (int)lroundf(sy - p->h * scale),
                  (int)lroundf(p->w * scale), (int)lroundf(p->h * scale));
}
static void slingshot_front(const View *v) {
  const Pic *p = res_pic(P_SLINGSHOT_01_FRONT);
  float scale = v->zoom / PIX_PER_M;
  float sx = draw_sx(v, g.sling_x + 0.15f), sy = draw_sy(v, g.sling_y + 8.5f - 3.6f);
  draw_pic_scaled(P_SLINGSHOT_01_FRONT, (int)lroundf(sx - p->w * scale / 2), (int)lroundf(sy - p->h * scale),
                  (int)lroundf(p->w * scale), (int)lroundf(p->h * scale));
}
static void band(const View *v, float fx, float fy, color_t c) {
  float bx, by, a;
  if (g.loaded < 0) return;
  game_object_pos(g.loaded, &bx, &by, &a);
  int t = (int)(v->zoom * 0.25f) + 1;
  draw_line((int)draw_sx(v, fx), (int)draw_sy(v, fy), (int)draw_sx(v, bx - 0.5f), (int)draw_sy(v, by), t, c);
}

void draw_world(const View *v) {
  background(v);
  /* the last shot's path */
  for (int k = 0; k < g.trail_count; k++) {
    const TrailDot *t = &g.trail[k];
    int r = (int)(v->zoom * (0.12f + 0.08f * t->size)) + 1;
    gfx_circle((int)draw_sx(v, t->x), (int)draw_sy(v, t->y), r, COL_WHITE, 256);
  }
  slingshot_back(v);
  if (g.loaded >= 0 && g.ready) band(v, g.sling_x + 0.45f, g.sling_y + 0.1f, HEX(0x301808));
  for (int i = 0; i < g.count; i++)
    if ((g.obj[i].flags & O_SCENERY) && g.obj[i].cls != C_FRONT) object(v, i);
  for (int i = 2; i < g.count; i++)
    if ((g.obj[i].flags & O_ALIVE) && !(g.obj[i].flags & O_SCENERY) && g.obj[i].cls != C_FRONT) object(v, i);
  if (g.loaded >= 0 && g.ready) band(v, g.sling_x - 0.45f, g.sling_y + 0.1f, HEX(0x4A2810));
  slingshot_front(v);
  for (int i = 2; i < g.count; i++)
    if ((g.obj[i].flags & O_ALIVE) && g.obj[i].cls == C_FRONT) object(v, i);
  foreground(v);
  /* smoke, blasts and points */
  for (int k = 0; k < PUFF_CAP; k++) {
    const Puff *p = &g.puff[k];
    if (p->t <= 0) continue;
    float sx = draw_sx(v, p->x), sy = draw_sy(v, p->y);
    if (p->kind == PUFF_POINTS) {
      char t[12];
      int n = 0;
      uint32_t val = (uint32_t)p->points;
      char rev[12];
      do rev[n++] = (char)('0' + val % 10); while ((val /= 10) && n < 11);
      for (int j = 0; j < n; j++) t[j] = rev[n - 1 - j];
      t[n] = 0;
      float rise = (1000 - p->t) * 0.002f * v->zoom;
      gfx_text_center(&np_font_small, (int)sx, (int)(sy - rise), t, HEX(0xFFFFFF), 256);
    } else {
      int pic = p->kind == PUFF_BOOM ? P_EXPLOSION_3 : p->kind == PUFF_FEATHERS ? P_SMOKE_SMALL_1 : P_SMOKE_BIG_2;
      float grow = p->kind == PUFF_BOOM ? 1.0f + (400 - p->t) / 200.0f : 1.0f;
      draw_pic_at(pic, sx, sy, v->zoom / PIX_PER_M * grow, 0, 1);
    }
  }
}
