/* The track (world.h): the official track's parts from data.c, placed and turned on the 5 m grid. Every position is
 * a whole number of 1/256 m plus a multiple of 5 m, so the physics triangles come out exact. */
#include <string.h>
#include "world.h"

world_t W;

static int floordiv(int a, int b) { return a >= 0 ? a / b : -((-a + b - 1) / b); }
static int ffloor(float f) {
  int i = (int)f;
  return (float)i > f ? i - 1 : i;
}

void world_load(int track) {
  memset(&W, 0, sizeof W);
  W.id = track;
  W.t = &pt_tracks[track];
  const pt_track *t = W.t;
  /* the mountains (createMountainVertices): radius from the track's extent in tiles */
  float dx = (float)(t->bx1 - t->bx0) * PT_PART_SIZE / 2.0f * 1.41421356f;
  float dz = (float)(t->bz1 - t->bz0) * PT_PART_SIZE / 2.0f * 1.41421356f;
  float r = pt_maxf(200.0f, 160.0f + pt_maxf(dx, dz));
  W.mr = r;
  W.mx = ((float)t->bx0 + (float)(t->bx1 - t->bx0) / 2.0f) * PT_PART_SIZE;
  W.mz = ((float)t->bz0 + (float)(t->bz1 - t->bz0) / 2.0f) * PT_PART_SIZE;
  int seg = (int)(r / 10.0f);
  if (seg > 160) seg = 160;
  W.mseg = seg;
  int k = 0;
#define NEXT() (k = (k + 1) % PT_MOUNTAIN_N, pt_mountain[k])
  for (int e = 0; e < seg; e++)
    for (int n = 0; n < 8; n++) {
      float h;
      if (n == 0 || n == 7) h = 0.0f;
      else if (n == 1 && NEXT() < 0.5f) h = 0.0f;
      else h = NEXT();
      W.mh[e][n] = (uint8_t)(h * 255.0f + 0.5f);
    }
#undef NEXT
}

const pt_inst *world_inst(int i) { return &pt_insts[W.t->inst + i]; }
const int8_t *world_orient(const pt_inst *in) { return &pt_orient[(in->orient >> 2) * 36 + (in->orient & 3) * 9]; }
v3 world_origin(const pt_inst *in) {
  return V3((float)in->x * PT_PART_SIZE, (float)in->y * PT_PART_SIZE, (float)in->z * PT_PART_SIZE);
}
static v3 orient_apply(const int8_t *o, v3 p) {
  return V3((float)o[0] * p.x + (float)o[1] * p.y + (float)o[2] * p.z, (float)o[3] * p.x + (float)o[4] * p.y + (float)o[5] * p.z,
            (float)o[6] * p.x + (float)o[7] * p.y + (float)o[8] * p.z);
}
static v3 orient_back(const int8_t *o, v3 p) {
  return V3((float)o[0] * p.x + (float)o[3] * p.y + (float)o[6] * p.z, (float)o[1] * p.x + (float)o[4] * p.y + (float)o[7] * p.z,
            (float)o[2] * p.x + (float)o[5] * p.y + (float)o[8] * p.z);
}
v3 world_apply(const pt_inst *in, v3 local) { return vadd(orient_apply(world_orient(in), local), world_origin(in)); }

static void box_apply(const int8_t *o, v3 o0, v3 lo, v3 hi, v3 *wlo, v3 *whi) {
  v3 c = vmul(vadd(lo, hi), 0.5f), h = vmul(vsub(hi, lo), 0.5f);
  v3 wc = vadd(orient_apply(o, c), o0);
  v3 wh = V3(pt_absf((float)o[0]) * h.x + pt_absf((float)o[1]) * h.y + pt_absf((float)o[2]) * h.z,
             pt_absf((float)o[3]) * h.x + pt_absf((float)o[4]) * h.y + pt_absf((float)o[5]) * h.z,
             pt_absf((float)o[6]) * h.x + pt_absf((float)o[7]) * h.y + pt_absf((float)o[8]) * h.z);
  *wlo = vsub(wc, wh), *whi = vadd(wc, wh);
}

void world_box(const pt_inst *in, v3 *lo, v3 *hi) {
  const pt_part *p = &pt_parts[in->part];
  v3 l = V3((float)p->lo[0] / PT_FIX, (float)p->lo[1] / PT_FIX, (float)p->lo[2] / PT_FIX);
  v3 h = V3((float)p->hi[0] / PT_FIX, (float)p->hi[1] / PT_FIX, (float)p->hi[2] / PT_FIX);
  box_apply(world_orient(in), world_origin(in), l, h, lo, hi);
}

void world_query(v3 lo, v3 hi, void (*f)(int i, void *ctx), void *ctx) {
  const pt_track *t = W.t;
  float reach = (float)t->reach;
  int cx0 = ffloor((lo.x - reach) / 40.0f) - t->gx0, cx1 = ffloor((hi.x + reach) / 40.0f) - t->gx0;
  int cz0 = ffloor((lo.z - reach) / 40.0f) - t->gz0, cz1 = ffloor((hi.z + reach) / 40.0f) - t->gz0;
  if (cx0 < 0) cx0 = 0;
  if (cz0 < 0) cz0 = 0;
  if (cx1 >= t->gw) cx1 = t->gw - 1;
  if (cz1 >= t->gh) cz1 = t->gh - 1;
  for (int cz = cz0; cz <= cz1; cz++)
    for (int cx = cx0; cx <= cx1; cx++) {
      int c = cz * t->gw + cx;
      for (int i = pt_tcells[t->cell + c]; i < pt_tcells[t->cell + c + 1]; i++) {
        v3 a, b;
        world_box(world_inst(i), &a, &b);
        if (a.x > hi.x || b.x < lo.x || a.y > hi.y || b.y < lo.y || a.z > hi.z || b.z < lo.z) continue;
        f(i, ctx);
      }
    }
}

/* ------------------------------------------------------------------ gathering physics triangles */
typedef struct {
  v3 lo, hi;
  wtri *out;
  int n, max;
} gather_t;
static uint8_t seen[(4096 + 7) / 8];

static void add_tri(gather_t *g, v3 a, v3 b, v3 c) {
  float x0 = pt_minf(a.x, pt_minf(b.x, c.x)), x1 = pt_maxf(a.x, pt_maxf(b.x, c.x));
  float y0 = pt_minf(a.y, pt_minf(b.y, c.y)), y1 = pt_maxf(a.y, pt_maxf(b.y, c.y));
  float z0 = pt_minf(a.z, pt_minf(b.z, c.z)), z1 = pt_maxf(a.z, pt_maxf(b.z, c.z));
  if (x0 > g->hi.x || x1 < g->lo.x || y0 > g->hi.y || y1 < g->lo.y || z0 > g->hi.z || z1 < g->lo.z) return;
  v3 n = vcross(vsub(b, a), vsub(c, a));
  float l = vlen(n);
  if (!(l > 1e-9f)) return;
  if (g->n >= g->max) return;
  wtri *w = &g->out[g->n++];
  w->a = a, w->b = b, w->c = c;
  w->n = vmul(n, 1.0f / l);
  w->d = vdot(w->n, a);
}

static void gather_inst(int i, void *ctx) {
  gather_t *g = ctx;
  const pt_inst *in = world_inst(i);
  const pt_part *p = &pt_parts[in->part];
  const int8_t *o = world_orient(in);
  v3 o0 = world_origin(in);
  /* the query box in the part's space */
  v3 a = orient_back(o, vsub(g->lo, o0)), b = orient_back(o, vsub(g->hi, o0));
  v3 l = V3(pt_minf(a.x, b.x), pt_minf(a.y, b.y), pt_minf(a.z, b.z));
  float px = (float)p->lo[0] / PT_FIX, pz = (float)p->lo[2] / PT_FIX;
  int gx0 = ffloor((l.x - px) / 5.0f), gx1 = ffloor((pt_maxf(a.x, b.x) - px) / 5.0f);
  int gz0 = ffloor((l.z - pz) / 5.0f), gz1 = ffloor((pt_maxf(a.z, b.z) - pz) / 5.0f);
  if (gx0 < 0) gx0 = 0;
  if (gz0 < 0) gz0 = 0;
  if (gx1 >= p->gx) gx1 = p->gx - 1;
  if (gz1 >= p->gz) gz1 = p->gz - 1;
  if (gx0 > gx1 || gz0 > gz1) return;
  uint32_t mask = p->simple ? pt_masks[W.t->mask + in->mask] : 0xFFFFFFFFu;
  memset(seen, 0, (size_t)(p->ncol + 7) / 8);
  const int16_t *pool = &pt_pool[p->vbase * 3];
  for (int gz = gz0; gz <= gz1; gz++)
    for (int gx = gx0; gx <= gx1; gx++) {
      int c = p->grid + gz * p->gx + gx;
      for (uint32_t j = pt_cgrid[c]; j < pt_cgrid[c + 1]; j++) {
        int ti = pt_cgidx[j];
        if (seen[ti >> 3] & (1 << (ti & 7))) continue;
        seen[ti >> 3] |= (uint8_t)(1 << (ti & 7));
        if (ti < 32 && !(mask & (1u << ti))) continue;
        const uint16_t *tr = &pt_ctri[(p->col + ti) * 3];
        v3 v[3];
        for (int k = 0; k < 3; k++) {
          const int16_t *q = &pool[tr[k] * 3];
          v3 lp = V3((float)q[0] / PT_FIX, (float)q[1] / PT_FIX, (float)q[2] / PT_FIX);
          v[k] = vadd(orient_apply(o, lp), o0);
        }
        add_tri(g, v[0], v[1], v[2]);
      }
    }
}

static v3 mountain_point(int e, int k) {
  int s = W.mseg;
  float ang = (float)(e % s) / (float)s * 2.0f * PT_PI;
  float rad = W.mr + 100.0f * (float)k;
  float h = (float)W.mh[e % s][k] * (100.0f / 255.0f);
  return V3(W.mx + pt_cosf(ang) * rad, h, W.mz + pt_sinf(ang) * rad);
}

static void gather_mountains(gather_t *g) {
  float cx = (g->lo.x + g->hi.x) * 0.5f - W.mx, cz = (g->lo.z + g->hi.z) * 0.5f - W.mz;
  float half = pt_maxf(g->hi.x - g->lo.x, g->hi.z - g->lo.z) * 0.5f * 1.5f;
  float d = pt_sqrtf(cx * cx + cz * cz);
  if (d + half < W.mr || d - half > W.mr + 700.0f) return;
  float ang = pt_atan2f(cz, cx);
  if (ang < 0.0f) ang += 2.0f * PT_PI;
  int s = W.mseg;
  int e0 = (int)(ang / (2.0f * PT_PI) * (float)s);
  int k0 = ffloor((d - half - W.mr) / 100.0f), k1 = ffloor((d + half - W.mr) / 100.0f);
  if (k0 < 0) k0 = 0;
  if (k1 > 6) k1 = 6;
  for (int e = e0 - 1; e <= e0 + 1; e++) {
    int ee = (e + s) % s;
    for (int k = k0; k <= k1; k++) {
      v3 a = mountain_point(ee, k), b = mountain_point(ee + 1, k), c = mountain_point(ee + 1, k + 1),
         dd = mountain_point(ee, k + 1);
      add_tri(g, a, b, c);
      add_tri(g, a, c, dd);
    }
  }
}

int world_gather(v3 lo, v3 hi, wtri *out, int max) {
  gather_t g = {lo, hi, out, 0, max};
  world_query(lo, hi, gather_inst, &g);
  gather_mountains(&g);
  return g.n;
}

float mountain_height(float x, float z) {
  (void)x, (void)z;
  return 0.0f;
}

/* ------------------------------------------------------------------ start and detectors */
qt world_quat(int rot, int axis) {
  const int8_t *o = &pt_orient[axis * 36 + rot * 9];
  m3 m;
  for (int r = 0; r < 3; r++) m.r[r] = V3((float)o[r * 3], (float)o[r * 3 + 1], (float)o[r * 3 + 2]);
  return mat_quat(&m);
}

void world_start(v3 *pos, qt *rot) {
  const pt_inst *in = world_inst(W.t->start);
  const pt_part *p = &pt_parts[in->part];
  const int8_t *o = world_orient(in);
  /* the part's turn, then half a turn round y */
  m3 m;
  for (int r = 0; r < 3; r++) m.r[r] = V3(-(float)o[r * 3], (float)o[r * 3 + 1], -(float)o[r * 3 + 2]);
  *rot = mat_quat(&m);
  v3 off = mrow(&m, V3(p->start[0], p->start[1], p->start[2]));
  *pos = vadd(world_origin(in), off);
}

void world_detector(int k, detector_t *d) {
  const pt_spec *s = &pt_specs[W.t->spec + k];
  const pt_inst *in = world_inst(s->inst);
  const pt_part *p = &pt_parts[in->part];
  const int8_t *o = world_orient(in);
  v3 c = V3(p->dc[0], p->dc[1], p->dc[2]), h = V3(p->ds[0] * 0.5f, p->ds[1] * 0.5f, p->ds[2] * 0.5f);
  box_apply(o, world_origin(in), vsub(c, h), vadd(c, h), &d->lo, &d->hi);
  d->order = s->order;
  d->rot = in->orient & 3, d->axis = (uint8_t)(in->orient >> 2);
  d->wide = pt_part_id[in->part] == 65 || pt_part_id[in->part] == 77;
  d->part = world_origin(in);
  (void)floordiv;
}
