/* Dr. Zomboss's Zombot, drawn as the game draws it: parts moved, turned and scaled by the tracks of its .reanim,
 * one of every three frames kept and the others in between. Each part is a 4-bit picture drawn turned: every
 * screen pixel it covers looks up the part's pixel under it (boss_art.h). */
#include <string.h>
#include "boss_art.h"
#include "draw.h"
#include "game.h"
#include "gfx.h"
#include "res.h"

#define MAX_DRAWN 80
#define PART_K 1.25f   /* the parts' pictures are 0.32 of the game's size; the screen is 0.4 */

enum { RIG_BODY, RIG_DRIVER, RIG_FIRE, RIG_ICE };
typedef struct { int16_t x, y, sx, sy, kx, ky; uint8_t part, alpha; } Pose;
typedef struct {
  float a, b, c, d, tx, ty;     /* screen = (a c; b d) (u v) + t, u v the part's pixels */
  float ia, ib, ic, id;         /* the inverse */
  int16_t x0, y0, x1, y1;       /* what it covers on the screen */
  uint8_t part, alpha;
} Drawn;
/* (in gfx_scratch: no background is unpacked while a frame is drawn) */
#define drawn ((Drawn *)(void *)gfx_scratch)
_Static_assert(MAX_DRAWN * sizeof(Drawn) <= sizeof gfx_scratch, "the parts drawn");
static int ndrawn, tint;

static const uint8_t *base(void) { return table(T_BOSS); }

typedef struct { uint16_t w, h; const uint16_t *pal; const uint8_t *px; } Part;
static Part part(int i) {
  const uint8_t *b = base(), *t = b + rd32(b), *e = t + 4 + 12 * i;
  Part p = {rd16(e), rd16(e + 2), pal_colors(rd16(e + 4), NULL), b + rd32(e + 8)};
  return p;
}

typedef struct { uint16_t ntracks, kept, step, head; const uint8_t *flags, *offs; } Rig;
static Rig rig(int r) {
  const uint8_t *b = base(), *h = b + rd32(b + 4 + 4 * r);
  Rig g = {rd16(h), rd16(h + 2), rd16(h + 4), rd16(h + 6), h + 8, NULL};
  g.offs = h + 8 + ((g.ntracks + 3) & ~3);
  return g;
}

/* track i's pose in kept frame f, if it shows */
static bool pose(const Rig *g, int f, int i, Pose *p) {
  const uint8_t *fr = base() + rd32(g->offs + 4 * f), *mask = fr;
  if (!(mask[i >> 3] >> (i & 7) & 1)) return false;
  int n = 0;
  for (int k = 0; k < i; k++) n += mask[k >> 3] >> (k & 7) & 1;
  const uint8_t *q = fr + (g->ntracks + 7) / 8 + 14 * n;
  p->x = rds16(q), p->y = rds16(q + 2), p->sx = rds16(q + 4), p->sy = rds16(q + 6), p->kx = rds16(q + 8);
  p->ky = rds16(q + 10), p->part = q[12], p->alpha = q[13];
  return true;
}

static float fsin(float deg) {
  /* (sine of degrees, by its Taylor series around the nearest quarter turn) */
  float t = deg / 360.0f;
  t -= (float)(int)t;
  if (t < 0) t += 1;
  float x = t * 6.2831853f;
  if (x > 3.14159265f) x -= 6.2831853f;
  float x2 = x * x;
  return x * (1 - x2 / 6 * (1 - x2 / 20 * (1 - x2 / 42 * (1 - x2 / 72))));
}
static float fcos(float deg) { return fsin(deg + 90); }

/* adds the rig's parts at clip position pos (in the .reanim's frames, from the clip's start), moved by (ox, oy) */
static void add_rig(int r, int start, int len, float pos, bool loop, float ox, float oy, float scale,
                    float arm_dx, float arm_dy, Pose *head) {
  Rig g = rig(r);
  if (loop) {
    while (pos >= len) pos -= len;
  } else if (pos > len - 1)
    pos = (float)(len - 1);
  if (pos < 0) pos = 0;
  /* the kept frames inside the clip */
  int first = (start + g.step - 1) / g.step, last = (start + len - 1) / g.step;
  if (last >= g.kept) last = g.kept - 1;
  if (first > last) first = last;
  float at = (start + pos) / g.step;
  if (at < first) at = (float)first;
  if (at > last) at = (float)last;
  int f0 = (int)at, f1 = f0 + 1 > last ? (loop ? first : last) : f0 + 1;
  float k = at - f0;
  for (int i = 0; i < g.ntracks && ndrawn < MAX_DRAWN; i++) {
    Pose a, b;
    if (!pose(&g, f0, i, &a)) continue;
    if (!pose(&g, f1, i, &b) || b.part != a.part) b = a;
    float x = (a.x + (b.x - a.x) * k) / 8, y = (a.y + (b.y - a.y) * k) / 8;
    float sx = (a.sx + (b.sx - a.sx) * k) / 4096, sy = (a.sy + (b.sy - a.sy) * k) / 4096;
    float kx = (a.kx + (b.kx - a.kx) * k) / 16, ky = (a.ky + (b.ky - a.ky) * k) / 16;
    if (i == g.head && head) *head = a, head->x = (int16_t)(x * 8), head->y = (int16_t)(y * 8);
    if (g.flags[i]) x += arm_dx, y += arm_dy;
    Part p = part(a.part);
    Drawn *d = &drawn[ndrawn];
    /* the part's matrix in the game's pixels, then the screen's */
    float ma = fcos(kx) * sx, mb = fsin(kx) * sx, mc = -fsin(ky) * sy, md = fcos(ky) * sy;
    float s = scale * PART_K;
    d->a = ma * s, d->b = mb * s, d->c = mc * s, d->d = md * s;
    d->tx = (ox + x * scale - cam_x) * 0.4f, d->ty = (oy + y * scale) * 0.4f;
    float det = d->a * d->d - d->c * d->b;
    if (det < 1e-6f && det > -1e-6f) continue;
    d->ia = d->d / det, d->ib = -d->b / det, d->ic = -d->c / det, d->id = d->a / det;
    float xs[4] = {0, (float)p.w, 0, (float)p.w}, ys[4] = {0, 0, (float)p.h, (float)p.h};
    float x0 = 1e9f, y0 = 1e9f, x1 = -1e9f, y1 = -1e9f;
    for (int c = 0; c < 4; c++) {
      float X = d->a * xs[c] + d->c * ys[c] + d->tx, Y = d->b * xs[c] + d->d * ys[c] + d->ty;
      x0 = X < x0 ? X : x0, x1 = X > x1 ? X : x1, y0 = Y < y0 ? Y : y0, y1 = Y > y1 ? Y : y1;
    }
    if (x1 < 0 || y1 < 0 || x0 >= SCREEN_W || y0 >= SCREEN_H) continue;
    d->x0 = (int16_t)(x0 < 0 ? 0 : x0), d->y0 = (int16_t)(y0 < 0 ? 0 : y0);
    d->x1 = (int16_t)(x1 + 1 > SCREEN_W ? SCREEN_W : x1 + 1), d->y1 = (int16_t)(y1 + 1 > SCREEN_H ? SCREEN_H : y1 + 1);
    d->part = a.part, d->alpha = (uint8_t)(a.alpha + (b.alpha - a.alpha) * k);
    ndrawn++;
  }
}

static uint16_t boss_color(uint16_t c) {
  int r = c >> 11, g = (c >> 5) & 63, b = c & 31;
  if (tint == 1) r = r * 9 / 16, g = g * 13 / 16 + 6, b = b * 3 / 4 + 11;   /* frozen */
  else if (tint == 2) r = r + 6, g = g + 12, b = b + 6;                  /* hit */
  r = r > 31 ? 31 : r, g = g > 63 ? 63 : g, b = b > 31 ? 31 : b;
  return (uint16_t)(r << 11 | g << 5 | b);
}

static void band_fn(uint16_t *band, int y0, int y1, void *ctx) {
  (void)ctx;
  for (int i = 0; i < ndrawn; i++) {
    const Drawn *d = &drawn[i];
    if (d->y1 <= y0 || d->y0 >= y1) continue;
    Part p = part(d->part);
    uint16_t pal[16];
    for (int c = 0; c < 16; c++) pal[c] = boss_color(p.pal[c]);
    int stride = (p.w + 1) / 2;
    bool half = d->alpha < 190;
    if (d->alpha < 40) continue;
    /* 16.16 fixed point across each row */
    int32_t du = (int32_t)(d->ia * 65536), dv = (int32_t)(d->ib * 65536);
    int ya = d->y0 > y0 ? d->y0 : y0, yb = d->y1 < y1 ? d->y1 : y1;
    for (int y = ya; y < yb; y++) {
      uint16_t *out = band + (y % GFX_BAND) * SCREEN_W;
      float fx = d->x0 + 0.5f - d->tx, fy = y + 0.5f - d->ty;
      int32_t u = (int32_t)((d->ia * fx + d->ic * fy) * 65536), v = (int32_t)((d->ib * fx + d->id * fy) * 65536);
      for (int x = d->x0; x < d->x1; x++, u += du, v += dv) {
        uint32_t iu = (uint32_t)(u >> 16), iv = (uint32_t)(v >> 16);
        if (u < 0 || v < 0 || iu >= p.w || iv >= p.h) continue;
        int c = p.px[iv * stride + (iu >> 1)] >> ((iu & 1) * 4) & 15;
        if (!c) continue;
        out[x] = half ? mix565(pal[c], out[x], 16) : pal[c];
      }
    }
  }
}

void boss_draw(void) {
  ndrawn = 0;
  tint = boss.frozen > 0 ? 1 : B.ticks - boss.hit_at < 10 ? 2 : 0;
  const BossClip *c = &boss_clips[boss.clip];
  bool timed = boss.phase == BS_IDLE || boss.phase == BS_HEAD_BEFORE || boss.phase == BS_HEAD_AFTER;
  float elapsed = boss.phase == BS_BUNGEE_WAIT ? 1000 : boss.elapsed;
  float pos = elapsed * boss.fps * (boss.chill > 0 && timed ? 0.5f : 1) / 100;
  float adx = 0, ady = 0;
  if (boss.phase == BS_RV) adx = boss.column * 80.0f, ady = (boss.row - 1) * 85.0f - boss.column * 20.0f;
  else if (boss.phase == BS_BUNGEE || boss.phase == BS_BUNGEE_WAIT || boss.phase == BS_BUNGEE_LEAVE) adx = boss.column * 80.0f - 23;
  Pose head;
  memset(&head, 0, sizeof head);
  head.part = 0xFF;
  add_rig(RIG_BODY, c->start, c->len, pos, timed, 0, 0, 1, adx, ady, &head);
  /* the driver, in the Zombot's head */
  if (head.part != 0xFF) {
    static const uint16_t clip[3][2] = {{9, 15}, {24, 25}, {78, 86}};
    int k = boss.phase == BS_DEATH ? 2 : boss.phase == BS_SPIT ? 1 : 0;
    float dpos = k == 2 ? boss.elapsed * 18 / 100 : boss.anim * (k == 1 ? 36 : 18) / 100.0f;
    add_rig(RIG_DRIVER, clip[k][0], clip[k][1], dpos, k != 2, head.x / 8.0f + 28, head.y / 8.0f - 84, 1.2f, 0, 0, NULL);
  }
  /* the fire or ice ball */
  if (boss.ball.on) {
    bool ice = boss.ball.kind == 1;
    int start = boss.ball.age < 100 ? (ice ? 1 : 0) : (ice ? 17 : 16), len = boss.ball.age < 100 ? 16 : (ice ? 59 : 57);
    float bpos = boss.ball.age < 100 ? boss.ball.age * 16 / 100.0f : (boss.ball.age - 100) * 2 / 100.0f;
    float bx = boss.ball.x, by = row_y(boss.ball.row, boss.ball.x);
    add_rig(ice ? RIG_ICE : RIG_FIRE, start, len, bpos, boss.ball.age >= 100, bx - 75, by - 120, 1, 0, 0, NULL);
  }
  gfx_custom(band_fn, NULL);
}
