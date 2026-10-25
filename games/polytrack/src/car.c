/* The car (car.h): what polytrack_physics.wasm's updateCarModel does each 1 ms frame, then Bullet's step: gravity,
 * damping, the chassis' contacts (a sequential impulse solver with split impulses), the integration, and the
 * btRaycastVehicle. The numbers are PolyTrack's (see the spec); Bullet's vehicle code is followed line by line. */
#include <string.h>
#include "car.h"

#define DT 0.001f
#define GRAVITY -9.82f
#define MASS 400.0f
#define INV_MASS (1.0f / 400.0f)
#define DAMP_LIN 0.999894619f       /* (1 - 0.1)^0.001 */
#define DAMP_ANG_LOW 0.999894619f   /* angular 0.1 */
#define DAMP_ANG_HIGH 0.999084115f  /* angular 0.6 (in the air, Down held) */
#define REST 0.12f
#define RADIUS 0.331f
#define TRAVEL 10.0f                /* maxSuspensionTravelCm 1000 */
#define DAMP_COMP 200.0f
#define DAMP_RELAX 1.35f
#define FRICTION_SLIP 3.0f
#define ROLL_INFLUENCE 0.5f
#define MAX_SUSP_FORCE 1e6f
#define ENGINE 4000.0f
#define MARGIN 0.03f                /* the hull's 0.02 and the meshes' 0.01 */
#define BREAKING 0.02f              /* Bullet's contact breaking threshold */
#define MAX_CONTACTS 12

const v3 car_wheel_pos[4] = {{0.627909f, 0.27f, 1.3478f}, {-0.627909f, 0.27f, 1.3478f},
                             {0.720832f, 0.27f, -1.52686f}, {-0.720832f, 0.27f, -1.52686f}};
static const float RESET_LEN[4] = {0.0780950100f, 0.0780950100f, 0.0781289680f, 0.0781289680f};

static v3 inv_inertia_local;
static v3 hull[PT_HULL_N];          /* body space: the model's hull moved up by the mass offset */
static v3 hull_lo, hull_hi;
static float hull_plane[16][4];     /* the hull's face planes, outwards */
static int nhull_plane;
static wtri cache[PT_CACHE];

/* the checkpoints of the track */
#define MAX_DET 64
static detector_t det[MAX_DET];
static int ndet, ncp_det;
static uint16_t orders[MAX_DET];
static int norders;

int car_checkpoints(void) { return norders; }

static void hull_setup(void) {
  for (int i = 0; i < PT_HULL_N; i++) hull[i] = V3(pt_hull[i][0], pt_hull[i][1] + CAR_MASS_OFFSET, pt_hull[i][2]);
  hull_lo = hull_hi = hull[0];
  for (int i = 1; i < PT_HULL_N; i++) {
    hull_lo = V3(pt_minf(hull_lo.x, hull[i].x), pt_minf(hull_lo.y, hull[i].y), pt_minf(hull_lo.z, hull[i].z));
    hull_hi = V3(pt_maxf(hull_hi.x, hull[i].x), pt_maxf(hull_hi.y, hull[i].y), pt_maxf(hull_hi.z, hull[i].z));
  }
  /* inertia: Bullet's box from the cached AABB (support points +- 0.04), its margin 0.02 added twice */
  float lx = 2.0f * ((hull_hi.x - hull_lo.x) * 0.5f + 0.04f + 0.02f + 0.02f);
  float ly = 2.0f * ((hull_hi.y - hull_lo.y) * 0.5f + 0.04f + 0.02f + 0.02f);
  float lz = 2.0f * ((hull_hi.z - hull_lo.z) * 0.5f + 0.04f + 0.02f + 0.02f);
  float m = MASS * 0.08333333f;
  inv_inertia_local = V3(1.0f / (m * (ly * ly + lz * lz)), 1.0f / (m * (lx * lx + lz * lz)), 1.0f / (m * (lx * lx + ly * ly)));
  /* face planes: every plane through three hull points with all the others behind it */
  nhull_plane = 0;
  for (int a = 0; a < PT_HULL_N; a++)
    for (int b = a + 1; b < PT_HULL_N; b++)
      for (int c = b + 1; c < PT_HULL_N; c++) {
        v3 n = vcross(vsub(hull[b], hull[a]), vsub(hull[c], hull[a]));
        float l = vlen(n);
        if (l < 1e-6f) continue;
        n = vmul(n, 1.0f / l);
        float d = vdot(n, hull[a]);
        int pos = 0, neg = 0;
        for (int k = 0; k < PT_HULL_N; k++) {
          float s = vdot(n, hull[k]) - d;
          if (s > 1e-5f) pos++;
          if (s < -1e-5f) neg++;
        }
        if (pos && neg) continue;
        if (pos) n = vneg(n), d = -d;
        bool dup = false;
        for (int k = 0; k < nhull_plane; k++)
          if (pt_absf(hull_plane[k][0] - n.x) + pt_absf(hull_plane[k][1] - n.y) + pt_absf(hull_plane[k][2] - n.z) < 1e-4f) dup = true;
        if (dup || nhull_plane >= 16) continue;
        hull_plane[nhull_plane][0] = n.x, hull_plane[nhull_plane][1] = n.y, hull_plane[nhull_plane][2] = n.z;
        hull_plane[nhull_plane][3] = d;
        nhull_plane++;
      }
}

static void update_inertia(car_t *c) {
  qmat(c->rot, &c->basis);
  const m3 *b = &c->basis;
  m3 *I = &c->inv_inertia;
  for (int i = 0; i < 3; i++) {
    v3 r = b->r[i];
    v3 s = V3(r.x * inv_inertia_local.x, r.y * inv_inertia_local.y, r.z * inv_inertia_local.z);
    I->r[i] = V3(vdot(s, b->r[0]), vdot(s, b->r[1]), vdot(s, b->r[2]));
  }
}

static void wheels_reset(car_t *c) {
  for (int i = 0; i < 4; i++) {
    wheel_t *w = &c->w[i];
    w->clipped = 1.0f, w->relvel = 0.0f;
    w->susp = REST;
    w->normal = vneg(w->dir);
  }
  for (int i = 0; i < 4; i++) c->w[i].susp = RESET_LEN[i];
  c->w[0].steering = c->w[1].steering = 0.0f;
}

static void update_wheel_ws(car_t *c, wheel_t *w, int i) {
  w->contact = 0;
  w->hard = vadd(c->pos, mrow(&c->basis, car_wheel_pos[i]));
  w->dir = mrow(&c->basis, V3(0.0f, -1.0f, 0.0f));
  w->axle = mrow(&c->basis, V3(-1.0f, 0.0f, 0.0f));
}

void car_reset(car_t *c) {
  static int setup;
  if (!setup) hull_setup(), setup = 1;
  memset(c, 0, sizeof *c);
  world_start(&c->pos, &c->rot);
  update_inertia(c);
  for (int i = 0; i < 4; i++) {
    wheel_t *w = &c->w[i];
    update_wheel_ws(c, w, i);
    w->stiffness = 50.0f;
  }
  wheels_reset(c);
  c->ang_damp = DAMP_ANG_LOW;
  c->ground_on = c->pos.y < 4.0f;
  /* the track's detectors, and its checkpoint orders */
  const pt_track *t = W.t;
  ndet = t->ncp + t->nfin;
  if (ndet > MAX_DET) ndet = MAX_DET;
  ncp_det = t->ncp;
  norders = 0;
  for (int k = 0; k < ndet; k++) {
    world_detector(k, &det[k]);
    if (k < ncp_det && (!norders || orders[norders - 1] != det[k].order)) orders[norders++] = det[k].order;
  }
}

bool car_finished(const car_t *c) { return c->finish != 0; }

/* ------------------------------------------------------------------ the triangles round the car */
static void cache_refresh(car_t *c) {
  /* as the game re-picks the parts round the car when it moved more than 1.5 m: here the triangles within 5 m */
  if (c->cache_ok && pt_absf(c->pos.x - c->cache_at.x) + pt_absf(c->pos.y - c->cache_at.y) + pt_absf(c->pos.z - c->cache_at.z) <= 1.5f)
    return;
  c->cache_at = c->pos;
  v3 h = V3(5.0f, 5.0f, 5.0f);
  c->ncache = world_gather(vsub(c->pos, h), vadd(c->pos, h), cache, PT_CACHE);
  c->cache_ok = 1;
}

/* Bullet's btTriangleRaycastCallback, closest hit: fraction, normal facing the ray's start */
static bool ray_tri(const wtri *t, v3 from, v3 to, float *frac, v3 *normal) {
  v3 v10 = vsub(t->b, t->a), v20 = vsub(t->c, t->a);
  v3 tn = vcross(v10, v20);
  float dist = vdot(t->a, tn);
  float da = vdot(tn, from) - dist, db = vdot(tn, to) - dist;
  if (da * db >= 0.0f) return false;
  float dd = da / (da - db);
  if (dd >= *frac) return false;
  float tol = vlen2(tn) * -0.0001f;
  v3 p = vadd(vmul(from, 1.0f - dd), vmul(to, dd));
  v3 v0p = vsub(t->a, p), v1p = vsub(t->b, p), v2p = vsub(t->c, p);
  if (vdot(vcross(v0p, v1p), tn) < tol) return false;
  if (vdot(vcross(v1p, v2p), tn) < tol) return false;
  if (vdot(vcross(v2p, v0p), tn) < tol) return false;
  v3 n = vnorm(tn);
  *normal = da <= 0.0f ? vneg(n) : n;
  *frac = dd;
  return true;
}

static bool cast_ray(car_t *c, v3 from, v3 to, float *frac, v3 *normal) {
  bool hit = false;
  float lx = pt_minf(from.x, to.x), hx = pt_maxf(from.x, to.x);
  float ly = pt_minf(from.y, to.y), hy = pt_maxf(from.y, to.y);
  float lz = pt_minf(from.z, to.z), hz = pt_maxf(from.z, to.z);
  *frac = 1.0f;
  for (int i = 0; i < c->ncache; i++) {
    const wtri *t = &cache[i];
    if (pt_maxf(t->a.x, pt_maxf(t->b.x, t->c.x)) < lx || pt_minf(t->a.x, pt_minf(t->b.x, t->c.x)) > hx) continue;
    if (pt_maxf(t->a.y, pt_maxf(t->b.y, t->c.y)) < ly || pt_minf(t->a.y, pt_minf(t->b.y, t->c.y)) > hy) continue;
    if (pt_maxf(t->a.z, pt_maxf(t->b.z, t->c.z)) < lz || pt_minf(t->a.z, pt_minf(t->b.z, t->c.z)) > hz) continue;
    if (ray_tri(t, from, to, frac, normal)) hit = true;
  }
  /* the ground plane, y = 0 (in the world only while the car is low) */
  if (c->ground_on && (from.y > 0.0f) != (to.y > 0.0f)) {
    float dd = from.y / (from.y - to.y);
    if (dd < *frac) {
      *frac = dd;
      *normal = from.y > 0.0f ? V3(0.0f, 1.0f, 0.0f) : V3(0.0f, -1.0f, 0.0f);
      hit = true;
    }
  }
  return hit;
}

/* ------------------------------------------------------------------ the chassis' contacts */
typedef struct {
  v3 n;                     /* from the world to the car */
  v3 r;                     /* contact point - centre of mass */
  float dist;               /* < 0: inside */
  float jinv, tjinv;
  v3 t;                     /* friction direction */
  float rhs, rhs_pen, trhs;
  float imp, timp, pimp;
} contact_t;

static int find_contacts(car_t *c, contact_t *out) {
  int n = 0;
  v3 wh[PT_HULL_N];
  v3 lo = V3(1e30f, 1e30f, 1e30f), hi = V3(-1e30f, -1e30f, -1e30f);
  for (int i = 0; i < PT_HULL_N; i++) {
    wh[i] = vadd(c->pos, mrow(&c->basis, hull[i]));
    lo = V3(pt_minf(lo.x, wh[i].x), pt_minf(lo.y, wh[i].y), pt_minf(lo.z, wh[i].z));
    hi = V3(pt_maxf(hi.x, wh[i].x), pt_maxf(hi.y, wh[i].y), pt_maxf(hi.z, wh[i].z));
  }
  float g = MARGIN + BREAKING;
  lo = vsub(lo, V3(g, g, g)), hi = vadd(hi, V3(g, g, g));
  contact_t cand[48];
  int nc = 0;
  for (int i = 0; i < c->ncache && nc < 48; i++) {
    const wtri *t = &cache[i];
    if (pt_maxf(t->a.x, pt_maxf(t->b.x, t->c.x)) < lo.x || pt_minf(t->a.x, pt_minf(t->b.x, t->c.x)) > hi.x) continue;
    if (pt_maxf(t->a.y, pt_maxf(t->b.y, t->c.y)) < lo.y || pt_minf(t->a.y, pt_minf(t->b.y, t->c.y)) > hi.y) continue;
    if (pt_maxf(t->a.z, pt_maxf(t->b.z, t->c.z)) < lo.z || pt_minf(t->a.z, pt_minf(t->b.z, t->c.z)) > hi.z) continue;
    /* the hull's corners against the triangle's face (either side, the side the car's centre is on) */
    float side = vdot(t->n, c->pos) - t->d;
    v3 nn = side >= 0.0f ? t->n : vneg(t->n);
    float dd = side >= 0.0f ? t->d : -t->d;
    v3 e0 = vsub(t->b, t->a), e1 = vsub(t->c, t->b), e2 = vsub(t->a, t->c);
    for (int k = 0; k < PT_HULL_N && nc < 48; k++) {
      float s = vdot(nn, wh[k]) - dd;
      if (s >= g || s < -0.5f) continue;
      if (vdot(vcross(e0, vsub(wh[k], t->a)), t->n) < 0.0f) continue;
      if (vdot(vcross(e1, vsub(wh[k], t->b)), t->n) < 0.0f) continue;
      if (vdot(vcross(e2, vsub(wh[k], t->c)), t->n) < 0.0f) continue;
      contact_t *ct = &cand[nc++];
      ct->n = nn;
      ct->r = vsub(wh[k], c->pos);
      ct->dist = s - MARGIN;
    }
    /* the triangle's corners inside the hull */
    const v3 *tv[3] = {&t->a, &t->b, &t->c};
    for (int k = 0; k < 3 && nc < 48; k++) {
      v3 p = mcol(&c->basis, vsub(*tv[k], c->pos));
      float best = -1e30f;
      int bi = -1;
      for (int j = 0; j < nhull_plane; j++) {
        float s = hull_plane[j][0] * p.x + hull_plane[j][1] * p.y + hull_plane[j][2] * p.z - hull_plane[j][3];
        if (s > g) {
          bi = -2;
          break;
        }
        if (s > best) best = s, bi = j;
      }
      if (bi < 0) continue;
      contact_t *ct = &cand[nc++];
      ct->n = vneg(mrow(&c->basis, V3(hull_plane[bi][0], hull_plane[bi][1], hull_plane[bi][2])));
      ct->r = vsub(*tv[k], c->pos);
      ct->dist = best - MARGIN;
    }
  }
  /* the ground plane */
  if (c->ground_on)
    for (int k = 0; k < PT_HULL_N && nc < 48; k++)
      if (wh[k].y < g && wh[k].y > -0.5f) {
        contact_t *ct = &cand[nc++];
        ct->n = V3(0.0f, 1.0f, 0.0f);
        ct->r = vsub(wh[k], c->pos);
        ct->dist = wh[k].y - MARGIN;
      }
  /* the deepest ones */
  for (int i = 0; i < nc && n < MAX_CONTACTS; i++) {
    int b = i;
    for (int j = i + 1; j < nc; j++)
      if (cand[j].dist < cand[b].dist) b = j;
    contact_t tmp = cand[i];
    cand[i] = cand[b], cand[b] = tmp;
    out[n++] = cand[i];
  }
  return n;
}

static float jac_inv(const car_t *c, v3 r, v3 n) {
  v3 rn = vcross(r, n);
  return 1.0f / (INV_MASS + vdot(rn, mrow(&c->inv_inertia, rn)));
}

/* the solver: contacts with friction 1.0, ten iterations, split impulses (Bullet's defaults) */
static void solve(car_t *c, contact_t *ct, int n, v3 *push, v3 *turn) {
  v3 v = vadd(c->vel, V3(0.0f, GRAVITY * DT, 0.0f));   /* with the gravity impulse */
  v3 w = c->ang;
  v3 dv = V3(0.0f, 0.0f, 0.0f), dw = dv, pv = dv, pw = dv;
  for (int i = 0; i < n; i++) {
    contact_t *k = &ct[i];
    k->jinv = jac_inv(c, k->r, k->n);
    v3 vel = vadd(v, vcross(w, k->r));
    float rel = vdot(k->n, vel);
    float pen = k->dist, perr = 0.0f, verr = -rel;
    float erp = pen < -0.04f ? 0.8f : 0.2f;
    if (pen > 0.0f) verr -= pen / DT;
    else perr = -pen * erp / DT;
    if (pen > -0.04f) k->rhs = (perr + verr) * k->jinv, k->rhs_pen = 0.0f;
    else k->rhs = verr * k->jinv, k->rhs_pen = perr * k->jinv;
    /* one friction direction: along the sliding */
    v3 lat = vsub(vel, vmul(k->n, rel));
    float l2 = vlen2(lat);
    if (l2 > 1.1920929e-07f) {
      k->t = vmul(lat, 1.0f / pt_sqrtf(l2));
    } else {
      v3 a = pt_absf(k->n.x) > 0.7071f ? V3(-k->n.y, k->n.x, 0.0f) : V3(0.0f, -k->n.z, k->n.y);
      k->t = vnorm(a);
    }
    k->tjinv = jac_inv(c, k->r, k->t);
    k->trhs = -vdot(k->t, vel) * k->tjinv;
    k->imp = k->timp = k->pimp = 0.0f;
  }
  for (int it = 0; it < 10; it++) {
    for (int i = 0; i < n; i++) {
      contact_t *k = &ct[i];
      float dvn = vdot(k->n, dv) + vdot(vcross(k->r, k->n), dw);
      float d = k->rhs - dvn * k->jinv;
      float sum = k->imp + d;
      if (sum < 0.0f) d = -k->imp, sum = 0.0f;
      k->imp = sum;
      dv = vadd(dv, vmul(k->n, d * INV_MASS));
      dw = vadd(dw, mrow(&c->inv_inertia, vmul(vcross(k->r, k->n), d)));
    }
    for (int i = 0; i < n; i++) {
      contact_t *k = &ct[i];
      float lim = 1.0f * k->imp;
      float dvt = vdot(k->t, dv) + vdot(vcross(k->r, k->t), dw);
      float d = k->trhs - dvt * k->tjinv;
      float sum = k->timp + d;
      if (sum < -lim) sum = -lim;
      if (sum > lim) sum = lim;
      d = sum - k->timp;
      k->timp = sum;
      dv = vadd(dv, vmul(k->t, d * INV_MASS));
      dw = vadd(dw, mrow(&c->inv_inertia, vmul(vcross(k->r, k->t), d)));
    }
  }
  for (int it = 0; it < 10; it++)
    for (int i = 0; i < n; i++) {
      contact_t *k = &ct[i];
      if (k->rhs_pen == 0.0f) continue;
      float dvn = vdot(k->n, pv) + vdot(vcross(k->r, k->n), pw);
      float d = k->rhs_pen - dvn * k->jinv;
      float sum = k->pimp + d;
      if (sum < 0.0f) d = -k->pimp, sum = 0.0f;
      k->pimp = sum;
      pv = vadd(pv, vmul(k->n, d * INV_MASS));
      pw = vadd(pw, mrow(&c->inv_inertia, vmul(vcross(k->r, k->n), d)));
    }
  c->vel = vadd(v, dv);
  c->ang = vadd(w, dw);
  *push = pv, *turn = pw;
  for (int i = 0; i < n; i++)
    if (ct[i].imp > c->impulse) c->impulse = ct[i].imp;
}

/* btTransformUtil::integrateTransform */
static void integrate(car_t *c, v3 lin, v3 ang) {
  c->pos = vadd(c->pos, vmul(lin, DT));
  float a2 = vlen2(ang), a = 0.0f;
  if (a2 > 1.1920929e-07f) a = pt_sqrtf(a2);
  if (a * DT > 0.785398163f) a = 0.785398163f / DT;
  v3 axis;
  if (a < 0.001f) axis = vmul(ang, 0.5f * DT - (DT * DT * DT) * 0.020833333333f * a * a);
  else axis = vmul(ang, pt_sinf(0.5f * a * DT) / a);
  qt d = {axis.x, axis.y, axis.z, pt_cosf(a * DT * 0.5f)};
  qt q = qmul(d, c->rot);
  float l = pt_sqrtf(q.x * q.x + q.y * q.y + q.z * q.z + q.w * q.w);
  if (l > 1.1920929e-07f) q.x /= l, q.y /= l, q.z /= l, q.w /= l;
  c->rot = q;
  update_inertia(c);
}

/* ------------------------------------------------------------------ btRaycastVehicle */
static void apply_impulse(car_t *c, v3 imp, v3 rel) {
  c->vel = vadd(c->vel, vmul(imp, INV_MASS));
  c->ang = vadd(c->ang, mrow(&c->inv_inertia, vcross(rel, imp)));
}
static v3 vel_at(const car_t *c, v3 rel) { return vadd(c->vel, vcross(c->ang, rel)); }

static void ray_cast(car_t *c, wheel_t *w, int i) {
  update_wheel_ws(c, w, i);
  float raylen = REST + RADIUS;
  v3 to = vadd(w->hard, vmul(w->dir, raylen));
  w->point = to;
  float frac;
  v3 normal;
  w->ground = 0;
  if (cast_ray(c, w->hard, to, &frac, &normal)) {
    w->normal = normal;
    w->contact = 1;
    w->ground = 1;
    float hit = frac * raylen;
    w->susp = hit - RADIUS;
    float lo = REST - TRAVEL, hi = REST + TRAVEL;
    if (w->susp < lo) w->susp = lo;
    if (w->susp > hi) w->susp = hi;
    w->point = vadd(vmul(w->hard, 1.0f - frac), vmul(to, frac));
    float den = vdot(w->normal, w->dir);
    v3 vel = vel_at(c, vsub(w->point, c->pos));
    float proj = vdot(w->normal, vel);
    if (den >= -0.1f) {
      w->relvel = 0.0f;
      w->clipped = 1.0f / 0.1f;
    } else {
      float inv = -1.0f / den;
      w->relvel = proj * inv;
      w->clipped = inv;
    }
  } else {
    w->susp = REST;
    w->relvel = 0.0f;
    w->normal = vneg(w->dir);
    w->clipped = 1.0f;
  }
}

/* the wheel's axle turned by its steering (updateWheelTransform: steeringMat * right) */
static v3 wheel_axle(const wheel_t *w) {
  v3 up = vneg(w->dir), right = w->axle;
  float s = pt_sinf(w->steering), co = pt_cosf(w->steering);
  /* Rodrigues: right cos + (up x right) sin + up (up . right)(1 - cos) */
  return vadd(vadd(vmul(right, co), vmul(vcross(up, right), s)), vmul(up, vdot(up, right) * (1.0f - co)));
}

static void vehicle_update(car_t *c) {
  v3 axle_ws[4];
  for (int i = 0; i < 4; i++) {
    update_wheel_ws(c, &c->w[i], i);
    axle_ws[i] = wheel_axle(&c->w[i]);
  }
  v3 fwd = mcolumn(&c->basis, 2);
  c->speed = 3.6f * vlen(c->vel);
  if (vdot(fwd, c->vel) < 0.0f) c->speed = -c->speed;
  for (int i = 0; i < 4; i++) ray_cast(c, &c->w[i], i);
  /* the springs */
  for (int i = 0; i < 4; i++) {
    wheel_t *w = &c->w[i];
    if (w->contact) {
      float force = w->stiffness * (REST - w->susp) * w->clipped;
      float rv = w->relvel;
      force -= (rv < 0.0f ? DAMP_COMP : DAMP_RELAX) * rv;
      w->force = force * MASS;
      if (w->force < 0.0f) w->force = 0.0f;
    } else {
      w->force = 0.0f;
    }
  }
  for (int i = 0; i < 4; i++) {
    wheel_t *w = &c->w[i];
    float f = w->force > MAX_SUSP_FORCE ? MAX_SUSP_FORCE : w->force;
    apply_impulse(c, vmul(w->normal, f * DT), vsub(w->point, c->pos));
  }
  /* updateFriction */
  int on = 0;
  float side[4] = {0}, fwdimp[4] = {0};
  v3 axle[4], fw[4];
  for (int i = 0; i < 4; i++) on += c->w[i].ground;
  for (int i = 0; i < 4; i++) {
    wheel_t *w = &c->w[i];
    if (!w->ground) continue;
    v3 a = axle_ws[i];
    a = vsub(a, vmul(w->normal, vdot(a, w->normal)));
    a = vnorm(a);
    axle[i] = a;
    fw[i] = vnorm(vcross(w->normal, a));
    /* resolveSingleBilateral against the fixed world */
    v3 rel = vsub(w->point, c->pos);
    float nl = vlen2(a);
    if (nl > 1.1f) {
      side[i] = 0.0f;
    } else {
      float relv = vdot(a, vel_at(c, rel));
      side[i] = -0.2f * relv * jac_inv(c, rel, a);
    }
  }
  bool sliding = false;
  for (int i = 0; i < 4; i++) {
    wheel_t *w = &c->w[i];
    float roll = 0.0f;
    if (w->ground) {
      if (w->engine != 0.0f) {
        roll = w->engine * DT;
      } else {
        float maximp = w->brake != 0.0f ? w->brake : 0.0f;
        v3 rel = vsub(w->point, c->pos);
        float jinv = jac_inv(c, rel, fw[i]);
        float vrel = vdot(fw[i], vel_at(c, rel));
        float j = -vrel * jinv / (float)on;
        if (j > maximp) j = maximp;
        if (j < -maximp) j = -maximp;
        roll = j;
      }
    }
    fwdimp[i] = 0.0f;
    w->skid = 1.0f;
    if (w->ground) {
      float maximp = w->force * DT * FRICTION_SLIP;
      float maxsq = maximp * maximp;
      fwdimp[i] = roll;
      float x = roll * 0.5f, y = side[i];
      float sq = x * x + y * y;
      if (sq > maxsq) {
        sliding = true;
        w->skid *= maximp / pt_sqrtf(sq);
      }
    }
  }
  if (sliding)
    for (int i = 0; i < 4; i++)
      if (side[i] != 0.0f && c->w[i].skid < 1.0f) fwdimp[i] *= c->w[i].skid, side[i] *= c->w[i].skid;
  v3 up = mcolumn(&c->basis, 1);
  for (int i = 0; i < 4; i++) {
    wheel_t *w = &c->w[i];
    v3 rel = vsub(w->point, c->pos);
    if (fwdimp[i] != 0.0f) apply_impulse(c, vmul(fw[i], fwdimp[i]), rel);
    if (side[i] != 0.0f) {
      v3 imp = vmul(axle[i], side[i]);
      /* (the roll influence along the car's up, ROLLING_INFLUENCE_FIX) */
      v3 r2 = vsub(rel, vmul(up, vdot(up, rel) * (1.0f - ROLL_INFLUENCE)));
      apply_impulse(c, imp, r2);
    }
  }
  /* the wheels' turning (for the picture) */
  for (int i = 0; i < 4; i++) {
    wheel_t *w = &c->w[i];
    if (w->contact) {
      v3 f = mcolumn(&c->basis, 2);
      f = vsub(f, vmul(w->normal, vdot(f, w->normal)));
      v3 vel = vel_at(c, vsub(w->hard, c->pos));
      w->drot = vdot(f, vel) * DT / RADIUS;
    }
    w->rotation += w->drot;
    if (w->rotation > 6.2831853f) w->rotation -= 6.2831853f;
    if (w->rotation < -6.2831853f) w->rotation += 6.2831853f;
    w->drot *= 0.99f;
  }
}

/* ------------------------------------------------------------------ checkpoints: the car's detector box against a
   detector's (OBB against AABB, 15 axes) */
static bool box_hit(const car_t *c, const detector_t *d) {
  v3 cen = vadd(c->pos, mrow(&c->basis, V3(0.0f, 0.48f, -0.15f)));
  float he[3] = {0.445f, 0.11f, 0.9f};
  v3 ax[3] = {mcolumn(&c->basis, 0), mcolumn(&c->basis, 1), mcolumn(&c->basis, 2)};
  v3 bc = vmul(vadd(d->lo, d->hi), 0.5f), bh = vmul(vsub(d->hi, d->lo), 0.5f);
  float bhe[3] = {bh.x, bh.y, bh.z};
  v3 t = vsub(cen, bc);
  float R[3][3], AR[3][3];
  for (int i = 0; i < 3; i++) {
    float a[3] = {ax[i].x, ax[i].y, ax[i].z};
    for (int j = 0; j < 3; j++) R[i][j] = a[j], AR[i][j] = pt_absf(a[j]) + 1.19e-07f;
  }
  float tt[3] = {t.x, t.y, t.z};
  /* the box's axes (world) */
  for (int j = 0; j < 3; j++) {
    float ra = he[0] * AR[0][j] + he[1] * AR[1][j] + he[2] * AR[2][j];
    if (pt_absf(tt[j]) > ra + bhe[j]) return false;
  }
  /* the car's axes */
  for (int i = 0; i < 3; i++) {
    float rb = bhe[0] * AR[i][0] + bhe[1] * AR[i][1] + bhe[2] * AR[i][2];
    float p = tt[0] * R[i][0] + tt[1] * R[i][1] + tt[2] * R[i][2];
    if (pt_absf(p) > he[i] + rb) return false;
  }
  /* cross products: car axis i x world axis j */
  for (int i = 0; i < 3; i++)
    for (int j = 0; j < 3; j++) {
      int i1 = (i + 1) % 3, i2 = (i + 2) % 3, j1 = (j + 1) % 3, j2 = (j + 2) % 3;
      float ra = he[i1] * AR[i2][j] + he[i2] * AR[i1][j];
      float rb = bhe[j1] * AR[i][j2] + bhe[j2] * AR[i][j1];
      float p = tt[j2] * R[i][j1] - tt[j1] * R[i][j2];
      if (pt_absf(p) > ra + rb) return false;
    }
  return true;
}

static void checkpoints(car_t *c) {
  if (c->next_cp >= norders) {
    for (int k = ncp_det; k < ndet; k++)
      if (box_hit(c, &det[k])) {
        c->finish = c->frames;
        return;
      }
    return;
  }
  uint16_t want = orders[c->next_cp];
  for (int k = 0; k < ncp_det; k++) {
    const detector_t *d = &det[k];
    if (d->order != want || !box_hit(c, d)) continue;
    if (c->next_cp < 64) c->cp_frames[c->next_cp] = c->frames;
    c->next_cp++;
    if (d->axis != 0) return;       /* (no respawn on a checkpoint on a wall) */
    float a = (float)d->rot * PT_HALF_PI;
    float vz = c->vel.z, vx = c->vel.x;
    bool back = (d->rot == 0 && vz < 0.0f) || (d->rot == 1 && vx < 0.0f) || (d->rot == 2 && vz > 0.0f) ||
                (d->rot == 3 && vx > 0.0f);
    float b = back ? a + PT_PI : a;
    qt q = {0.0f, pt_sinf(b * 0.5f), 0.0f, pt_cosf(b * 0.5f)};
    v3 off = qrot(q, V3(0.0f, 0.35f, -1.35f));
    if (d->wide) {
      float s = pt_sinf(a), co = pt_cosf(a);
      /* the part's turn of (10, 0, 0) about y */
      off = vadd(off, V3(10.0f * co, 0.0f, -10.0f * s));
    }
    c->respawn.pos = vadd(d->part, off);
    c->respawn.rot = q;
    c->respawn.next = c->next_cp;
    c->has_respawn = 1;
    return;
  }
}

/* ------------------------------------------------------------------ a frame */
void car_step(car_t *c, unsigned in) {
  if (c->frames > PT_MAX_FRAMES) return;
  cache_refresh(c);
  c->impulse = 0.0f;
  /* the downforce: -up |v . forward| 0.05 */
  {
    v3 up = mcolumn(&c->basis, 1), fwd = mcolumn(&c->basis, 2);
    float s = pt_absf(vdot(c->vel, fwd));
    c->vel = vadd(c->vel, vmul(up, -s * 0.05f * INV_MASS));
  }
  if (c->finish) in = 0;
  c->frames++;
  bool up = in & IN_UP, down = in & IN_DOWN, left = in & IN_LEFT, right = in & IN_RIGHT;
  bool w0 = c->w[0].contact, w1 = c->w[1].contact, w2 = c->w[2].contact, w3 = c->w[3].contact;
  float lock_now = 0.0f;
  bool grounded;
  bool engine_off = false;
  if (w0 || w1 || w2 || w3) {
    c->ang_damp = DAMP_ANG_LOW;
    c->air = 0.0f;
    c->lock -= 0.001f;
    grounded = true;
    if (c->lock < 0.0f) c->lock = 0.0f;
    lock_now = c->lock;
  } else {
    c->ang_damp = down ? DAMP_ANG_HIGH : DAMP_ANG_LOW;
    grounded = false;
    c->air += 0.001f;
    if (c->air > 0.2f) {
      c->air = 0.2f, c->lock = 0.2f;
      engine_off = true;
    }
    lock_now = c->lock;
  }
  if (!(w0 || w1 || w2) && w3) c->ang_damp = DAMP_ANG_LOW;
  float engine = engine_off ? 0.0f : (up && lock_now == 0.0f ? ENGINE : 0.0f);
  c->w[2].engine = c->w[3].engine = engine;
  c->brake_light = 0;
  if (down) {
    if (!(grounded && !(c->speed > 1.0f))) {
      for (int i = 0; i < 4; i++) c->w[i].brake = 10.0f;
      c->brake_light = 1;
    } else {
      for (int i = 0; i < 4; i++) c->w[i].brake = 0.0f;
      float e = (c->speed / 100.0f + 1.0f) * -2000.0f;
      e = e > -2000.0f ? e : -2000.0f;
      e = e < 0.0f ? e : 0.0f;
      c->w[2].engine = c->w[3].engine = e;
    }
  } else {
    for (int i = 0; i < 4; i++) c->w[i].brake = 0.0f;
  }
  /* steering: where the car slides, while a front wheel touches, and the keys, less at speed */
  {
    v3 lv = mcol(&c->basis, c->vel);
    float lx = lv.x, lz = lv.z, l2 = lx * lx + lz * lz;
    if (l2 != 0.0f) {
      float l = pt_sqrtf(l2);
      lz = lz / l, lx = lx / l;
    }
    float k = pt_clampf(c->speed / 30.0f, 0.0f, 1.0f);
    float slip = PT_HALF_PI - (pt_atan2f(-lz, -lx) + PT_PI);
    float assist = (w0 ? k : (w1 ? k : 0.0f)) * slip;
    assist = pt_clampf(assist, -0.410258055f, 0.410258055f);
    float amount = 155.0f / pt_powf(pt_maxf(pt_absf(c->speed), 46.0f), 1.55f);
    float s = c->steer_in;
    if (left && !right) s = pt_minf(1.0f, s + 0.010000001f);
    else if (right && !left) s = pt_maxf(-1.0f, s - 0.010000001f);
    else if (s > 0.0f) s = pt_maxf(0.0f, s - 0.010000001f);
    else if (s < 0.0f) s = pt_minf(0.0f, s + 0.010000001f);
    c->steer_in = s;
    float key = amount * s, st;
    if (assist < 0.0f && key < 0.0f) st = assist > key ? key : assist;
    else if (assist > 0.0f && key > 0.0f) st = assist < key ? key : assist;
    else st = assist + key;
    c->w[0].steering = c->w[1].steering = st;
  }
  for (int i = 0; i < 4; i++) c->w[i].stiffness = c->w[i].susp < -0.2f ? 200.0f : 50.0f;
  /* Reset: back to the last checkpoint */
  bool reset = in & IN_RESET;
  if (reset && !c->reset_held && c->has_respawn) {
    c->pos = c->respawn.pos;
    c->rot = c->respawn.rot;
    update_inertia(c);
    c->vel = c->ang = V3(0.0f, 0.0f, 0.0f);
    for (int i = 0; i < 4; i++) update_wheel_ws(c, &c->w[i], i);
    wheels_reset(c);
    c->next_cp = c->respawn.next;
    c->cache_ok = 0;
    cache_refresh(c);
  }
  c->reset_held = reset;
  /* Bullet's step */
  if (c->pos.y < 4.0f) c->ground_on = 1;
  else if (c->pos.y > 5.0f) c->ground_on = 0;
  c->vel = vmul(c->vel, DAMP_LIN);
  c->ang = vmul(c->ang, c->ang_damp);
  contact_t ct[MAX_CONTACTS];
  int n = find_contacts(c, ct);
  v3 push = V3(0.0f, 0.0f, 0.0f), turn = push;
  if (n) {
    solve(c, ct, n, &push, &turn);
    if (vlen2(push) != 0.0f || vlen2(turn) != 0.0f) integrate(c, push, vmul(turn, 0.1f));
  } else {
    c->vel = vadd(c->vel, V3(0.0f, GRAVITY * DT, 0.0f));
  }
  integrate(c, c->vel, c->ang);
  vehicle_update(c);
  if (!c->finish) checkpoints(c);
}

uint32_t car_hash(const car_t *c) {
  uint32_t h = 2166136261u;
  h = pt_hash(h, &c->pos, sizeof c->pos);
  h = pt_hash(h, &c->rot, sizeof c->rot);
  h = pt_hash(h, &c->vel, sizeof c->vel);
  h = pt_hash(h, &c->ang, sizeof c->ang);
  h = pt_hash(h, &c->frames, sizeof c->frames);
  h = pt_hash(h, &c->next_cp, sizeof c->next_cp);
  h = pt_hash(h, &c->finish, sizeof c->finish);
  for (int i = 0; i < 4; i++) h = pt_hash(h, &c->w[i].susp, sizeof(float));
  return h;
}
