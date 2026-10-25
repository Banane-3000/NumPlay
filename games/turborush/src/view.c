/* The view from behind your car, the classic way of the arcade racers: the road ahead is cut in segments, each
 * projected from near to far; a segment only fills the screen rows the nearer ones left (a crest hides what's
 * behind it), which gives each row of the screen one piece of road. The roadside and the cars are pictures scaled
 * to their distance, drawn far to near, each cut off below the crest in front of it.
 *
 * The frame is worked out once (view_frame), then drawn strip by strip (view_draw). */
#include <math.h>
#include <string.h>
#include "../../../launcher/src/gfx.h"
#include "art.h"
#include "view.h"

#define FOCAL 256.f                 /* pixels per metre at a metre */
#define CAM_H 3.6f                  /* the camera's height over the road */
#define CAM_D 9.f                   /* ...and how far behind your car */
#define NEAR 1.f
#define DRAW 170                    /* segments ahead */
#define SCENERY_SEGS 112            /* segments ahead with things beside the road */
#define CAR_W 3.f                   /* metres the width of a car picture covers */
#define MAX_SPRITES 128
#define SEG ((float)TR_SEG)
#define ROADW (TR_ROAD / (float)FX)

enum { ROW_SKY, ROW_ROAD, ROW_FAR };
typedef struct {
  float cx, hw;                     /* the road's middle and half width (pixels) */
  int32_t z;                        /* the distance along the race there (FX) */
  uint16_t depth;                   /* metres from the camera */
  uint8_t kind;
} vrow;

typedef struct {
  const art_img *img;
  const color_t *pal;
  int x0, top, w, h;                /* where on the screen, and the size drawn */
  int clip;                         /* rows from here down are behind a crest */
  float depth;
  uint8_t flip, fog;                /* fog: 0..32 */
  uint8_t shadow;
} vsprite;

static vrow rows[SCREEN_H];
static vsprite spr[MAX_SPRITES];
static int nspr, pan, stars_on, rain_on, frame_ticks, boost_style = -1;
static float seg_x[DRAW + 1], seg_dx[DRAW + 1], seg_dz[DRAW + 1], seg_y[DRAW + 2];
static int16_t seg_clip[DRAW + 1];

/* colours for the circuit and the time of day */
static color_t sky_pal[64], scene_pal[SC_KINDS][64], car_pal[ART_PAINTS][64];
static color_t grass[2], grass_avg, road[2], road_avg, curb[2], curb_avg, line_c, haze;
static const uint8_t *sky_px;
static uint64_t sky_mask;           /* the backdrop's colours that are sky, where stars may shine at night */
static int32_t track_len;           /* for the start line */
int16_t view_map_x[VIEW_MAP], view_map_y[VIEW_MAP];

/* ------------------------------------------------------------------ colours */
static color_t rgb(uint32_t c) { return HEX(c); }
static color_t mood(color_t c, int m, int rain, int kind) {
  /* kind: 0 ground and things, 1 the sky, 2 a light (barely dimmed at night) */
  if (m == MOOD_SUNSET) c = gfx_mix(c, rgb(0xFF9466), kind == 1 ? 9 : 6), c = gfx_mix(c, 0, kind == 1 ? 0 : 3);
  if (m == MOOD_NIGHT) c = gfx_mix(c, rgb(0x081226), kind == 2 ? 4 : kind == 1 ? 23 : 20);
  if (rain) c = gfx_mix(c, rgb(0x6E7884), kind == 2 ? 2 : kind == 1 ? 11 : 6);
  return c;
}

uint16_t view_paint(int paint) { return art_car_pal[(paint & 7) * 64 + 14]; }

void view_setup(const tr_track *t, int m, int rain) {
  static const uint32_t GROUND[6][2] = {{0x5A9A4E, 0x50904A}, {0x3F7F44, 0x39773F}, {0xD3A464, 0xC8995A},
                                        {0x7D878C, 0x758085}, {0xE4EBF2, 0xD5DFE9}, {0x5F676E, 0x58606A}};
  static const uint32_t ROAD[6][2] = {{0x62676C, 0x5D6267}, {0x5C6166, 0x575C61}, {0x6E6863, 0x68625E},
                                      {0x575C62, 0x52575D}, {0x50565C, 0x4B5157}, {0x4C5157, 0x474C52}};
  int th = t->theme < 6 ? t->theme : 0;
  for (int i = 0; i < 2; i++) {
    grass[i] = mood(rgb(GROUND[th][i]), m, rain, 0);
    road[i] = mood(rgb(ROAD[th][i]), m, rain, 0);
    curb[i] = mood(rgb(i ? 0xE8E6DC : 0xC8403A), m, rain, 0);
  }
  grass_avg = gfx_mix(grass[0], grass[1], 16), road_avg = gfx_mix(road[0], road[1], 16);
  curb_avg = gfx_mix(curb[0], curb[1], 16);
  line_c = mood(rgb(0xEDEDE4), m, rain, m == MOOD_NIGHT ? 2 : 0);
  haze = m == MOOD_NIGHT ? rgb(0x121C30) : m == MOOD_SUNSET ? rgb(0xE6A890) : rgb(0xBCD0DC);
  if (rain) haze = gfx_mix(haze, rgb(0x8A949E), 14);
  int sky = t->sky & 3;
  sky_px = art_sky[sky];
  sky_mask = 0;
  for (int i = 0; i < 64; i++) {
    color_t c = art_sky_pal[sky * 64 + i];
    int r = c >> 11 << 3, g = (c >> 5 & 63) << 2, b = (c & 31) << 3;
    if (b > 180 && b > r + 50 && b > g) sky_mask |= (uint64_t)1 << i;
    sky_pal[i] = mood(c, m, rain, 1);
  }
  for (int k = 0; k < SC_KINDS; k++) {
    for (int i = 0; i < 64; i++) {
      color_t c = art_scenery_pal[k * 64 + i];
      if (k == SC_ROCK && th == TH_ALPINE) {   /* grey granite in the Alps, not red sandstone */
        int r = c >> 11 << 3, g = (c >> 5 & 63) << 2, b = (c & 31) << 3, y = (r * 3 + g * 6 + b) / 10;
        c = RGB(y * 9 / 10, y * 19 / 20, y);
      }
      bool lit = m == MOOD_NIGHT && (art_scenery_glow[k] >> i & 1);
      scene_pal[k][i] = mood(c, m, rain, lit ? 2 : 0);
    }
  }
  for (int p = 0; p < ART_PAINTS; p++)
    for (int i = 0; i < 64; i++)
      car_pal[p][i] = mood(art_car_pal[p * 64 + i], m, rain, i >= 24 && i < 32 ? 2 : 0);
  stars_on = m == MOOD_NIGHT && !rain;
  rain_on = rain;
  track_len = t->length;
  tr_map(t->id, VIEW_MAP, view_map_x, view_map_y, 72, 52);
}

void view_map_at(const tr_track *t, int32_t z, int *x, int *y) {
  int32_t per = t->length / VIEW_MAP;
  int32_t w = z % t->length;
  if (w < 0) w += t->length;
  int i = (int)(w / per) % VIEW_MAP, j = (i + 1) % VIEW_MAP;
  int32_t f = (w - i * per) * 256 / per;
  *x = (view_map_x[i] * (256 - f) + view_map_x[j] * f) / 256;
  *y = (view_map_y[i] * (256 - f) + view_map_y[j] * f) / 256;
}

/* ------------------------------------------------------------------ working out the frame */
static int32_t lerp_z(const tr_track *t, int32_t a, int32_t b, int alpha, bool wrap) {
  int32_t d = wrap ? tr_wrap(t, b - a) : b - a;
  if (d > 20 * FX || d < -20 * FX) return b;          /* a jump (Free Ride traffic coming back): no sweep */
  return a + d * alpha / 256;
}

static void add_sprite(const art_sprite *s, const color_t *pal, float cx, float bottom, float w, float h, int clip,
                       float depth, bool flip, bool shadow) {
  /* (the last place is kept for your car) */
  if (nspr >= MAX_SPRITES - (depth > 0 ? 1 : 0) || w < 1.5f || h < 1.5f) return;
  int iw = (int)(w + 0.5f), ih = (int)(h + 0.5f), x0 = (int)floorf(cx - w / 2 + 0.5f);
  int top = (int)floorf(bottom - h + 0.5f);
  if (x0 >= SCREEN_W || x0 + iw <= 0 || top >= clip || top >= SCREEN_H || iw > 4000 || ih > 4000) return;
  int level = 0;
  while (level < 2 && iw * 2 <= s->mip[level].w) level++;
  vsprite *p = &spr[nspr++];
  p->img = &s->mip[level], p->pal = pal;
  p->x0 = x0, p->top = top, p->w = iw, p->h = ih, p->clip = clip, p->depth = depth, p->flip = flip;
  p->shadow = shadow;
  float f = (depth - 140.f) / 26.f;
  p->fog = (uint8_t)(f < 0 ? 0 : f > 18 ? 18 : f);
}

static int fog_of(float depth) {
  float f = (depth - 120.f) / 22.f;
  return f < 0 ? 0 : f > 20 ? 20 : (int)f;
}

void view_frame(const tr_track *t, const tr_race *a, const tr_race *b, int alpha) {
  bool wrap = b->mode == MODE_FREE;
  boost_style = -1;
  frame_ticks = (int)b->ticks;
  int32_t me_z = lerp_z(t, a->car[0].z, b->car[0].z, alpha, wrap);
  int32_t me_x = a->car[0].x + (b->car[0].x - a->car[0].x) * alpha / 256;
  float camx = (float)(a->cam_x + (b->cam_x - a->cam_x) * alpha / 256) / FX;
  int32_t cam_z = me_z - (int32_t)(CAM_D * FX);
  int32_t per = TR_SEG * FX;
  int32_t base = cam_z >= 0 ? cam_z / per : -((-cam_z + per - 1) / per);
  float frac = (float)(cam_z - base * per) / (float)per;
  int n = t->n;
  /* the camera rides at a fixed height over your car, so your car stays put on the screen over crests */
  float cam_y = (float)tr_height_at(t, me_z) / FX + CAM_H;
  /* the backdrop turns with the road */
  int s0 = (int)(((base % n) + n) % n);
  int32_t hd = t->heading[s0] + (int32_t)((float)(int16_t)(t->heading[s0 + 1] - t->heading[s0]) * frac);
  pan = (int)((((uint32_t)hd & 0xFFFF) * 800u) >> 16);
  /* the road */
  float x = 0, dx = -(float)t->curve[s0] * frac;
  int maxy = SCREEN_H;
  for (int k = 0; k <= DRAW; k++) {
    int s = (int)(((base + k) % n + n) % n);
    float z1 = (float)k * SEG - frac * SEG, z2 = z1 + SEG;
    float y1w = (float)t->y[s] / 100.f, y2w = (float)t->y[s + 1] / 100.f;
    float x1 = x / 100.f, x2 = (x + dx) / 100.f;
    seg_x[k] = x1, seg_dx[k] = dx, seg_dz[k] = z1, seg_y[k] = y1w, seg_y[k + 1] = y2w, seg_clip[k] = (int16_t)maxy;
    x += dx, dx += (float)t->curve[s];
    if (k == DRAW || z2 <= NEAR || maxy <= 0) continue;
    if (z1 < NEAR) {
      float u = (NEAR - z1) / SEG;
      x1 += (x2 - x1) * u, y1w += (y2w - y1w) * u, z1 = NEAR;
    }
    float p1 = FOCAL / z1, p2 = FOCAL / z2;
    float sx1 = 160.f + (x1 - camx) * p1, sy1 = VIEW_HZ + (cam_y - y1w) * p1, w1 = ROADW * p1;
    float sx2 = 160.f + (x2 - camx) * p2, sy2 = VIEW_HZ + (cam_y - y2w) * p2, w2 = ROADW * p2;
    if (sy2 >= sy1) continue;
    int r0 = (int)ceilf(sy2 - 0.5f), r1 = (int)ceilf(sy1 - 0.5f);
    if (r0 < 0) r0 = 0;
    if (r1 > maxy) r1 = maxy;
    for (int r = r0; r < r1; r++) {
      float u = (sy1 - ((float)r + 0.5f)) / (sy1 - sy2), iz = (1 - u) / z1 + u / z2, zr = 1.f / iz;
      vrow *w = &rows[r];
      w->cx = sx1 + (sx2 - sx1) * u, w->hw = w1 + (w2 - w1) * u;
      w->z = cam_z + (int32_t)(zr * FX), w->depth = (uint16_t)(zr < 60000 ? zr : 60000), w->kind = ROW_ROAD;
    }
    if (r0 < r1) maxy = r0;
  }
  for (int r = 0; r < maxy; r++) rows[r].kind = r < VIEW_HZ ? ROW_SKY : ROW_FAR;
  /* the cars first: they always get a place */
  nspr = 0;
  for (int i = 1; i < TR_CARS; i++) {
    if (i > tr_traffic(b)) continue;
    const tr_car *c = &b->car[i];
    int32_t cz = lerp_z(t, a->car[i].z, c->z, alpha, true);
    float dz = (float)tr_wrap(t, cz - cam_z) / FX;
    if (dz < 2.f || dz >= (DRAW - 1) * SEG) continue;
    int k = (int)floorf((dz + frac * SEG) / SEG);
    if (k < 0 || k >= DRAW) continue;
    float u = (dz - seg_dz[k]) / SEG, xr = seg_x[k] + seg_dx[k] * u / 100.f;
    float yr = seg_y[k] + (seg_y[k + 1] - seg_y[k]) * u, p = FOCAL / dz;
    float cx = (float)(a->car[i].x + (c->x - a->car[i].x) * alpha / 256) / FX;
    float sx = 160.f + (xr + cx - camx) * p, sy = VIEW_HZ + (cam_y - yr) * p, w = CAR_W * p;
    float yaw = seg_dx[k] / (100.f * SEG) + (float)c->vx / (float)(c->v > 5 * FX ? c->v : 5 * FX);
    int angle = yaw < -0.06f ? 0 : yaw > 0.06f ? 2 : 1;
    add_sprite(&art_cars[(c->style & 1) * 3 + angle], car_pal[c->paint & 7], sx, sy, w, w * 64 / 96, seg_clip[k], dz,
               false, true);
  }
  /* the roadside, near to far, while there's room */
  for (int k = 1; k < SCENERY_SEGS; k++) {
    float zc = seg_dz[k] + SEG / 2;
    if (zc < 2.5f) continue;
    int s = (int)(base + k);
    float xc = seg_x[k] + seg_dx[k] / 200.f, yc = (seg_y[k] + seg_y[k + 1]) / 2, p = FOCAL / zc;
    for (int side = -1; side <= 1; side += 2) {
      tr_thing o = tr_scenery(t, s, side);
      if (o.kind < 0) continue;
      const art_sprite *as = &art_scenery[o.kind];
      float h = (float)o.height / FX * p, w = h * as->mip[0].w / as->mip[0].h;
      float sx = 160.f + (xc + (float)o.x / FX - camx) * p, sy = VIEW_HZ + (cam_y - yc) * p;
      add_sprite(as, scene_pal[o.kind], sx, sy, w, h, seg_clip[k], zc, o.flip, false);
    }
  }
  /* far to near; the order stays as made on ties */
  for (int i = 1; i < nspr; i++) {
    vsprite v = spr[i];
    int j = i;
    while (j > 0 && spr[j - 1].depth < v.depth) spr[j] = spr[j - 1], j--;
    spr[j] = v;
  }
  /* you, last */
  {
    int k = (int)floorf((CAM_D + frac * SEG) / SEG);
    float u = (CAM_D - seg_dz[k]) / SEG, xr = seg_x[k] + seg_dx[k] * u / 100.f, yr = seg_y[k] + (seg_y[k + 1] - seg_y[k]) * u;
    float p = FOCAL / CAM_D, w = CAR_W * p;
    float sx = 160.f + (xr + (float)me_x / FX - camx) * p, sy = VIEW_HZ + (cam_y - yr) * p;
    int steer = a->steer + (b->steer - a->steer) * alpha / 256;
    if (b->phase == PH_DONE && b->end == END_FINISH) steer = (int)(b->car[0].vx / 4);
    int angle = steer < -350 ? 0 : steer > 350 ? 2 : 1;
    if (b->offroad && b->car[0].v > 3 * FX && (b->ticks & 2)) sy -= 1;
    if (b->impact) sx += (float)((b->impact * 7) % 5 - 2);
    add_sprite(&art_cars[(b->car[0].style & 1) * 3 + angle], car_pal[b->car[0].paint & 7], sx, sy, w, w * 64 / 96,
               SCREEN_H, 0, false, true);
    boost_style = b->boost && angle == 1 ? b->car[0].style & 1 : b->boost ? 2 : -1;
  }
}

/* ------------------------------------------------------------------ drawing a strip */
static inline void fill(color_t *row, int x0, int x1, color_t c) {
  if (x0 < 0) x0 = 0;
  if (x1 > SCREEN_W) x1 = SCREEN_W;
  for (int x = x0; x < x1; x++) row[x] = c;
}

static void draw_sky(color_t *dst, int r) {
  int sr = r - (VIEW_HZ - 100);
  if (sr < 0) sr = 0;
  const uint8_t *src = sky_px + sr * 400;
  int u = pan;
  for (int x = 0; x < SCREEN_W; x++, u++) {
    if (u >= 800) u -= 800;
    dst[x] = sky_pal[src[u < 400 ? u : 799 - u]];
  }
  if (stars_on && sr < 46) {
    for (int i = 0; i < 40; i++) {
      uint32_t h = (uint32_t)i * 2654435761u;
      if ((int)(h >> 26) % 46 != sr) continue;
      int sx = (int)((h >> 8) % 800), x = sx - pan;
      if (x < 0) x += 800;
      if (x >= SCREEN_W) continue;
      int px = src[sx < 400 ? sx : 799 - sx];
      if (sky_mask >> px & 1) dst[x] = (h & 0x300) ? HEX(0xC8D4E4) : HEX(0x8A9AB4);
    }
  }
}

static void draw_road(color_t *dst, const vrow *w) {
  int depth = w->depth, fog = fog_of((float)depth);
  /* bands every two segments; far away they blend into one colour (no shimmer) */
  int band = (int)(((w->z / (2 * TR_SEG * FX)) % 2 + 2) % 2);
  int k = depth < 110 ? 32 : depth > 260 ? 0 : (260 - depth) * 32 / 150;
  color_t g = gfx_mix(gfx_mix(grass_avg, grass[band], k), haze, fog);
  color_t rd = gfx_mix(gfx_mix(road_avg, road[band], k), haze, fog);
  color_t cb = gfx_mix(gfx_mix(curb_avg, curb[band], k), haze, fog);
  color_t ln = gfx_mix(line_c, haze, fog);
  float hw = w->hw, cx = w->cx;
  int l = (int)floorf(cx - hw + 0.5f), r = (int)floorf(cx + hw + 0.5f);
  float cw = hw * 0.11f;
  int cl = (int)floorf(cx - hw - cw + 0.5f), cr = (int)floorf(cx + hw + cw + 0.5f);
  fill(dst, 0, cl, g);
  fill(dst, cr, SCREEN_W, g);
  if (cl >= SCREEN_W || cr <= 0) return;
  fill(dst, cl, l, cb);
  fill(dst, r, cr, cb);
  fill(dst, l, r, rd);
  if (hw < 3) return;
  int lw = (int)(hw * 0.035f + 0.5f);
  if (lw < 1) lw = 1;
  fill(dst, l + lw, l + 2 * lw, ln);
  fill(dst, r - 2 * lw, r - lw, ln);
  if (depth < 230 && ((w->z / (FX * 3)) & 3) < 2) {   /* dashes: 6 m painted, 6 m gap */
    int a = (int)floorf(cx - hw / 3 - lw / 2.f + 0.5f), b = (int)floorf(cx + hw / 3 - lw / 2.f + 0.5f);
    fill(dst, a, a + lw, ln);
    fill(dst, b, b + lw, ln);
  }
}

static void draw_start(color_t *dst, const vrow *w) {
  if (!track_len) return;
  int32_t m = w->z % track_len;
  if (m < 0) m += track_len;
  if (m >= 3 * FX) return;
  int fog = fog_of((float)w->depth);
  float hw = w->hw, cx = w->cx;
  int row = (int)(m * 2 / (3 * FX));
  for (int q = 0; q < 12; q++) {
    int a = (int)floorf(cx - hw + 2 * hw * q / 12 + 0.5f), b = (int)floorf(cx - hw + 2 * hw * (q + 1) / 12 + 0.5f);
    fill(dst, a, b, gfx_mix((q + row) & 1 ? HEX(0x15181C) : HEX(0xF0F0EA), haze, fog));
  }
}

static void draw_sprite(const vsprite *p) {
  int r0 = p->top > gfx_y0 ? p->top : gfx_y0;
  int r1 = p->top + p->h;
  if (r1 > gfx_y1) r1 = gfx_y1;
  if (r1 > p->clip) r1 = p->clip;
  if (r0 >= r1) return;
  const art_img *im = p->img;
  int sw = im->w, sh = im->h;
  uint32_t xs = ((uint32_t)sw << 16) / (uint32_t)p->w, ys = ((uint32_t)sh << 16) / (uint32_t)p->h;
  color_t fogged[64];
  const color_t *pal = p->pal;
  if (p->fog) {
    for (int i = 0; i < 63; i++) fogged[i] = gfx_mix(pal[i], haze, p->fog);
    pal = fogged;
  }
  for (int r = r0; r < r1; r++) {
    int sy = (int)(((uint32_t)(r - p->top) * ys + ys / 2) >> 16);
    if (sy >= sh) sy = sh - 1;
    const uint8_t *src = im->px + sy * sw;
    int a = im->span[sy * 2], b = im->span[sy * 2 + 1];
    if (a >= b) continue;
    if (p->flip) {
      int t = sw - b;
      b = sw - a, a = t;
    }
    int xa = p->x0 + (int)(((uint32_t)a * (uint32_t)p->w) / (uint32_t)sw);
    int xb = p->x0 + (int)(((uint32_t)b * (uint32_t)p->w + (uint32_t)sw - 1) / (uint32_t)sw);
    if (xa < 0) xa = 0;
    if (xb > SCREEN_W) xb = SCREEN_W;
    color_t *dst = gfx_buf + (r - gfx_y0) * SCREEN_W;
    uint32_t fx = (uint32_t)(xa - p->x0) * xs + xs / 2;
    for (int x = xa; x < xb; x++, fx += xs) {
      int sx = (int)(fx >> 16);
      if (sx >= sw) sx = sw - 1;
      if (p->flip) sx = sw - 1 - sx;
      uint8_t c = src[sx];
      if (c != ART_T) dst[x] = pal[c];
    }
  }
}

static void draw_shadow(const vsprite *p) {
  int h = p->h / 9 + 1, y = p->top + p->h - h + 1, w = p->w * 7 / 8;
  if (y + h > p->clip) return;
  gfx_rrect(p->x0 + (p->w - w) / 2, y, w, h, h / 2, HEX(0x0A0E12), 120);
}

/* nitro: a blue flame from each exhaust of your car (the last sprite) */
static void draw_flames(void) {
  const vsprite *p = &spr[nspr - 1];
  static const uint8_t AT[3][2] = {{27, 34}, {25, 74}, {30, 70}};   /* % of the width: the coupe, the supercar, turning */
  int len = p->h / 6 + ((frame_ticks >> 1) & 1) * (p->h / 24 + 1), fw = p->w / 13 + 2;
  for (int k = 0; k < 2; k++) {
    int cx = p->x0 + p->w * AT[boost_style][k] / 100, y0 = p->top + p->h * 85 / 100;
    for (int j = 0; j < len; j++) {   /* a flame: wide at the pipe, a point at its end, white at its heart */
      int w = fw * (len - j) / len + 1, core = w / 2;
      gfx_fill_alpha(cx - w / 2, y0 + j, w, 1, HEX(0x3FA9F5), 200 - j * 120 / len);
      if (j < len * 2 / 3 && core) gfx_fill_alpha(cx - core / 2, y0 + j, core, 1, HEX(0xEAF9FF), 230 - j * 150 / len);
    }
  }
}

static void draw_rain(void) {
  for (int i = 0; i < 46; i++) {
    int x = (int)((uint32_t)(i * 97 + frame_ticks * 5) % 340u) - 10;
    int y = (int)((uint32_t)(i * 53 + frame_ticks * 13) % 260u) - 10;
    for (int j = 0; j < 8; j++) {
      int yy = y + j, xx = x - j / 3;
      if (yy < gfx_y0 || yy >= gfx_y1 || xx < 0 || xx >= SCREEN_W) continue;
      color_t *d = gfx_buf + (yy - gfx_y0) * SCREEN_W + xx;
      *d = gfx_mix(*d, HEX(0xC8D2DC), 11);
    }
  }
}

void view_draw(void) {
  for (int r = gfx_y0; r < gfx_y1; r++) {
    color_t *dst = gfx_buf + (r - gfx_y0) * SCREEN_W;
    const vrow *w = &rows[r];
    if (w->kind == ROW_SKY) draw_sky(dst, r);
    else if (w->kind == ROW_FAR) fill(dst, 0, SCREEN_W, gfx_mix(grass_avg, haze, 20));
    else draw_road(dst, w), draw_start(dst, w);
  }
  for (int i = 0; i < nspr; i++) {
    if (spr[i].shadow) draw_shadow(&spr[i]);
    draw_sprite(&spr[i]);
  }
  if (boost_style >= 0 && nspr) draw_flames();
  if (rain_on) draw_rain();
}

void view_car(int x, int bottom, int width, int paint, int style, int angle) {
  vsprite v;
  const art_sprite *s = &art_cars[(style & 1) * 3 + (angle < 0 ? 0 : angle > 2 ? 2 : angle)];
  int level = 0;
  while (level < 2 && width * 2 <= s->mip[level].w) level++;
  v.img = &s->mip[level], v.pal = art_car_pal + (paint & 7) * 64;
  v.w = width, v.h = width * 64 / 96, v.x0 = x - width / 2, v.top = bottom - v.h, v.clip = SCREEN_H;
  v.flip = 0, v.fog = 0, v.depth = 0, v.shadow = 1;
  draw_shadow(&v);
  draw_sprite(&v);
}
