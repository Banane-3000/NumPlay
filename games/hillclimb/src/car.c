/* A vehicle's physics (car.h). */
#include <math.h>
#include <string.h>
#include "art.h"
#include "car.h"
#include "stage.h"
#include "vehicle.h"

#define SUB 4                       /* steps a frame */
#define DT (1.0f / (CAR_HZ * SUB))
#define ITER 10
#define BETA 0.2f                   /* how much of an overlap is pushed out each step */
#define SLOP 0.004f
#define MAXC 64
#define SEG 0.2f                    /* the ground is straight between points this far apart */
#define HULL_R 0.05f
#define REACT 0.4f                  /* how much of the engine's twist the chassis feels (Bill leans into it) */

typedef struct {
  Body *b;
  float rx, ry, nx, ny, depth, mu, mn, mt, jn, jt, bias;
  int tree;                         /* a tree's index, or -1 */
  int wheel;                        /* the wheel's index, or -1 */
} Contact;

static Contact con[MAXC];
static int ncon;

static float cross(float ax, float ay, float bx, float by) { return ax * by - ay * bx; }
static void rot(float a, float x, float y, float *ox, float *oy) {
  float c = cosf(a), s = sinf(a);
  *ox = c * x - s * y, *oy = s * x + c * y;
}

float car_fuel_seconds(int veh, const uint8_t lv[4]) {
  const VehDef *d = &vehicles[veh];
  float f = d->fuel;
  for (int u = 0; u < 4; u++)
    if (d->up[u] == UP_FUEL) f *= 1 + 1.6f * (float)(lv[u] - 1) / (float)(d->max[u] - 1);
  return f;
}

void car_init(Car *c, int veh, const uint8_t lv[4], float x) {
  const VehDef *d = &vehicles[veh];
  const VehArt *a = &veh_art[veh];
  memset(c, 0, sizeof *c);
  c->veh = veh, c->nw = a->naxles, c->tree_broken = -1;
  float eng = 0, sus = 0, tir = 0, awd = d->awd, air = d->air, down = 0;
  for (int u = 0; u < 4; u++) {
    float f = (float)(lv[u] - 1) / (float)(d->max[u] - 1);
    switch (d->up[u]) {
      case UP_ENGINE: eng = f; break;
      case UP_SUSPENSION: sus = f; break;
      case UP_TIRES: case UP_TRACKS: tir = f; break;
      case UP_AWD: awd = f; break;
      case UP_AIR: air *= 1 + 2.4f * f; break;
      case UP_DOWNFORCE: down = d->downforce * f; break;
    }
  }
  c->torque = d->torque * (1 + 1.5f * eng);
  c->wmax = d->speed * (1 + 0.6f * eng) / a->wheel_r[0];
  c->brake = d->torque * 1.6f;
  c->grip = d->grip * (1 + 0.55f * tir);
  c->air = air, c->down = down;
  c->com[0] = d->com[0], c->com[1] = d->com[1] - 0.07f * sus;
  /* bodies */
  c->ch.im = 1 / d->mass;
  c->ch.ii = 1 / (d->mass * d->radius * d->radius);
  float wsum = 0;
  for (int i = 0; i < c->nw; i++) {
    float r = a->wheel_r[i];
    c->wr[i] = r;
    c->wh[i].im = 1 / d->wmass;
    c->wh[i].ii = 1 / (0.6f * d->wmass * r * r);
    /* the rear wheel always driven; the others by the 4WD's share */
    c->drive[i] = i == 0 ? 1 : awd;
    wsum += c->drive[i];
  }
  for (int i = 0; i < c->nw; i++) c->drive[i] *= (float)c->nw / (wsum > 0 ? wsum : 1) * 0.5f;
  if (c->nw > 2)   /* (the tank's five wheels share the engine) */
    for (int i = 0; i < c->nw; i++) c->drive[i] = 2.0f / (float)c->nw;
  /* springs: settled at `sag` under the chassis' share of the weight */
  float load = d->mass / (float)c->nw;
  float sag = d->sag * (1 - 0.25f * sus);
  c->k = load * 9.8f / sag;
  c->c = 2 * (d->zeta * (1 + 0.6f * sus)) * sqrtf(c->k * load);
  c->s0 = 2 * sag;
  c->smax = 2 * sag + d->droop;
  for (int i = 0; i < c->nw; i++) {
    c->anc[i][0] = a->axle[i][0] - c->com[0];
    c->anc[i][1] = a->axle[i][1] + sag - c->com[1];
  }
  c->nhull = a->nhull;
  for (int i = 0; i < a->nhull; i++) c->hull[i][0] = a->hull[i][0] - c->com[0], c->hull[i][1] = a->hull[i][1] - c->com[1];
  /* Bill's head over his hips, leaning */
  float nx, ny, hx, hy;
  rot(a->lean, BILL_NECK_X, BILL_NECK_Y, &nx, &ny);
  rot(a->lean, BILL_HEAD_X, BILL_HEAD_Y, &hx, &hy);
  c->head[0] = a->hips[0] + nx + hx - c->com[0];
  c->head[1] = a->hips[1] + ny + hy - c->com[1];
  /* on the ground at x, level */
  float gy = stage_floor(x);
  float lowest = 0;
  for (int i = 0; i < c->nw; i++)
    if (a->axle[i][1] - a->wheel_r[i] < lowest) lowest = a->axle[i][1] - a->wheel_r[i];
  c->ch.x = x + c->com[0], c->ch.y = gy - lowest + c->com[1] + 0.02f;
  for (int i = 0; i < c->nw; i++) {
    c->wh[i].x = c->ch.x + c->anc[i][0];
    c->wh[i].y = c->ch.y + c->anc[i][1] - sag;
  }
}

/* ------------------------------------------------------------------ the ground's contacts */
static Contact *new_contact(Body *b, float px, float py, float nx, float ny, float depth, float mu, int wheel) {
  if (ncon >= MAXC) return NULL;
  Contact *k = &con[ncon++];
  k->b = b, k->rx = px - b->x, k->ry = py - b->y, k->nx = nx, k->ny = ny, k->depth = depth, k->mu = mu;
  k->jn = k->jt = 0, k->tree = -1, k->wheel = wheel;
  return k;
}

/* a circle against a line through points of f, all of them above (floor) or below (ceiling) it: its deepest touch */
static bool circle_line(float cx, float cy, float r, bool ceiling, float *px, float *py, float *nx, float *ny, float *depth) {
  float best = 1e9f, bx = 0, by = 0;
  int i0 = (int)floorf((cx - r) / SEG) - 1, i1 = (int)ceilf((cx + r) / SEG) + 1;
  float ya = 0, yb;
  for (int i = i0; i <= i1; i++) {
    float xb = (float)i * SEG;
    if (ceiling) {
      if (!stage_ceiling(xb, &yb)) {
        if (i == i0) ya = 1e9f;
        else ya = 1e9f;
        continue;
      }
    } else {
      yb = stage_floor(xb);
    }
    if (i > i0 && ya < 1e8f) {
      float xa = xb - SEG, dx = SEG, dy = yb - ya;
      float t = ((cx - xa) * dx + (cy - ya) * dy) / (dx * dx + dy * dy);
      t = t < 0 ? 0 : t > 1 ? 1 : t;
      float qx = xa + t * dx, qy = ya + t * dy;
      float ex = cx - qx, ey = cy - qy, d = sqrtf(ex * ex + ey * ey);
      /* inside the ground: the distance counts as negative */
      float side = cross(dx, dy, cx - xa, cy - ya);
      if (ceiling ? side > 0 : side < 0) d = -d;
      if (d < best) best = d, bx = qx, by = qy;
    }
    ya = yb;
  }
  if (best >= r) return false;
  *px = bx, *py = by;
  float ex = cx - bx, ey = cy - by, l = sqrtf(ex * ex + ey * ey);
  if (l > 1e-5f && best > 0) {
    *nx = ex / l, *ny = ey / l;
  } else {   /* deep inside: straight out of the ground's slope */
    float x0 = cx - 0.1f, x1 = cx + 0.1f, y0, y1;
    if (ceiling) {
      stage_ceiling(x0, &y0), stage_ceiling(x1, &y1);
    } else {
      y0 = stage_floor(x0), y1 = stage_floor(x1);
    }
    float tx = x1 - x0, ty = y1 - y0, tl = sqrtf(tx * tx + ty * ty);
    *nx = -ty / tl, *ny = tx / tl;
    if (ceiling) *nx = -*nx, *ny = -*ny;
  }
  *depth = r - best;
  return true;
}

static void touch_circle(Body *b, float cx, float cy, float r, float mu, int wheel) {
  float px, py, nx, ny, d;
  if (circle_line(cx, cy, r, false, &px, &py, &nx, &ny, &d)) new_contact(b, cx - nx * r, cy - ny * r, nx, ny, d, mu, wheel);
  if (stage->ceiling && circle_line(cx, cy, r, true, &px, &py, &nx, &ny, &d))
    new_contact(b, cx - nx * r, cy - ny * r, nx, ny, d, mu, wheel);
  if (cx - r < STAGE_WALL) new_contact(b, cx - r, cy, 1, 0, STAGE_WALL - (cx - r), mu * 0.3f, wheel);
  if (stage->feature == FEAT_TREES) {
    for (int k = stage_tree_first(cx - 4);; k++) {
      float tx;
      if (!stage_tree(k, &tx)) {
        if (k % 6 == 0) break;
        continue;
      }
      if (tx > cx + 4) break;
      if (!stage_tree_standing(k) || tx < cx - 4) continue;
      float ty0 = stage_floor(tx), ty1 = ty0 + TREE_H;
      float qy = cy < ty0 ? ty0 : cy > ty1 ? ty1 : cy, ex = cx - tx, ey = cy - qy, l = sqrtf(ex * ex + ey * ey);
      if (l >= r + TREE_R || l < 1e-5f) continue;
      Contact *k2 = new_contact(b, cx - ex / l * r, cy - ey / l * r, ex / l, ey / l, r + TREE_R - l, 0.3f, wheel);
      if (k2) k2->tree = k;
    }
  }
}

static void find_contacts(Car *c) {
  ncon = 0;
  float mu = c->grip * stage->friction;
  for (int i = 0; i < c->nw; i++) {
    int before = ncon;
    touch_circle(&c->wh[i], c->wh[i].x, c->wh[i].y, c->wr[i], mu, i);
    c->ground[i] = c->ground[i] || ncon > before;
  }
  int before = ncon;
  float ca = cosf(c->ch.a), sa = sinf(c->ch.a);
  for (int i = 0; i < c->nhull; i++) {
    float px = c->ch.x + ca * c->hull[i][0] - sa * c->hull[i][1];
    float py = c->ch.y + sa * c->hull[i][0] + ca * c->hull[i][1];
    touch_circle(&c->ch, px, py, HULL_R, 0.6f * stage->friction, -1);
  }
  if (c->head_hit) {   /* (Bill down: his head lies on the ground too) */
    float hx = c->ch.x + ca * c->head[0] - sa * c->head[1], hy = c->ch.y + sa * c->head[0] + ca * c->head[1];
    touch_circle(&c->ch, hx, hy, BILL_HEAD_R, 0.6f, -1);
  }
  c->touch = c->touch || ncon > before;
}

/* ------------------------------------------------------------------ solving */
static void push(Body *b, float px, float py, float rx, float ry) {
  b->vx += b->im * px, b->vy += b->im * py;
  b->w += b->ii * cross(rx, ry, px, py);
}

static void prepare_contacts(void) {
  for (int i = 0; i < ncon; i++) {
    Contact *k = &con[i];
    float rn = cross(k->rx, k->ry, k->nx, k->ny), rt = cross(k->rx, k->ry, -k->ny, k->nx);
    k->mn = 1 / (k->b->im + k->b->ii * rn * rn);
    k->mt = 1 / (k->b->im + k->b->ii * rt * rt);
    float pen = k->depth - SLOP;
    k->bias = pen > 0 ? BETA / DT * pen : 0;
    if (k->bias > 4) k->bias = 4;
  }
}

static void solve_contacts(void) {
  for (int i = 0; i < ncon; i++) {
    Contact *k = &con[i];
    Body *b = k->b;
    float vx = b->vx - b->w * k->ry, vy = b->vy + b->w * k->rx;
    /* along the normal: no going in */
    float vn = vx * k->nx + vy * k->ny;
    float j = k->mn * (k->bias - vn), old = k->jn;
    k->jn = old + j > 0 ? old + j : 0;
    j = k->jn - old;
    push(b, j * k->nx, j * k->ny, k->rx, k->ry);
    /* friction along the ground, up to mu times the push */
    vx = b->vx - b->w * k->ry, vy = b->vy + b->w * k->rx;
    float tx = -k->ny, ty = k->nx, vt = vx * tx + vy * ty;
    float jt = -k->mt * vt, lim = k->mu * k->jn;
    old = k->jt;
    k->jt = old + jt;
    if (k->jt > lim) k->jt = lim;
    if (k->jt < -lim) k->jt = -lim;
    jt = k->jt - old;
    push(b, jt * tx, jt * ty, k->rx, k->ry);
  }
}

/* the wheel joints: across the axis held, along it the bump stops; and the engine */
typedef struct {
  float px, py, sAy, my, bias;      /* across the axis, and the error there pushed out */
  float dx, dy, sAx, mx, s;         /* along it (down the chassis) */
  float jlo, jm, mmax, speed;       /* the stops' and the engine's impulses; the engine's most, its target */
} Joint;
static Joint jo[CAR_MAXW];

static void prepare_joints(Car *c, float gas, float brake, bool fuel) {
  Body *A = &c->ch;
  float ca = cosf(A->a), sa = sinf(A->a);
  /* the engine: forward is the wheels turning clockwise */
  float spin = 0;
  for (int i = 0; i < c->nw; i++) spin += c->wh[i].w - A->w;
  spin /= (float)c->nw;
  float speed = 0, tmax = 0;
  bool shared = true;
  if (gas > 0 && fuel) {
    speed = -c->wmax, tmax = c->torque * gas;
  } else if (brake > 0) {
    if (spin < -1.5f || !fuel) speed = 0, tmax = c->brake * brake, shared = false;   /* rolling forward: brake */
    else speed = c->wmax * 0.45f, tmax = c->torque * 0.7f * brake;                   /* stopped: reverse */
  } else {
    speed = 0, tmax = c->torque * 0.015f, shared = false;   /* coasting: the engine's own drag */
  }
  c->rpm = 0;
  for (int i = 0; i < c->nw; i++) {
    Joint *J = &jo[i];
    Body *B = &c->wh[i];
    float rax = ca * c->anc[i][0] - sa * c->anc[i][1], ray = sa * c->anc[i][0] + ca * c->anc[i][1];
    float dx = B->x - (A->x + rax), dy = B->y - (A->y + ray);
    J->dx = sa, J->dy = -ca;   /* down the chassis */
    J->px = ca, J->py = sa;    /* across */
    J->s = dx * J->dx + dy * J->dy;
    J->bias = -BETA / DT * (dx * J->px + dy * J->py);
    J->sAy = cross(dx + rax, dy + ray, J->px, J->py);
    J->sAx = cross(dx + rax, dy + ray, J->dx, J->dy);
    J->my = 1 / (A->im + B->im + A->ii * J->sAy * J->sAy);
    J->mx = 1 / (A->im + B->im + A->ii * J->sAx * J->sAx);
    J->jlo = J->jm = 0;
    J->speed = speed;
    J->mmax = (shared ? tmax * c->drive[i] : tmax) * DT;
    /* the spring and damper: once a step, as an impulse */
    float vs = (B->vx - A->vx) * J->dx + (B->vy - A->vy) * J->dy - J->sAx * A->w;
    float f = c->k * (c->s0 - J->s) - c->c * vs;
    c->wheel_load[i] = f > 0 ? f : 0;
    float j = f * DT;
    B->vx += B->im * j * J->dx, B->vy += B->im * j * J->dy;
    A->vx -= A->im * j * J->dx, A->vy -= A->im * j * J->dy, A->w -= A->ii * j * J->sAx;
    float r = -(B->w - A->w) / c->wmax;
    if (r > c->rpm) c->rpm = r;
  }
  if (c->rpm > 1) c->rpm = 1;
}

static void solve_joints(Car *c) {
  Body *A = &c->ch;
  for (int i = 0; i < c->nw; i++) {
    Joint *J = &jo[i];
    Body *B = &c->wh[i];
    /* the engine (or brake) between chassis and wheel */
    if (J->mmax > 0) {
      float rel = B->w - A->w - J->speed;
      float j = -rel / (REACT * A->ii + B->ii), old = J->jm;
      J->jm = old + j;
      if (J->jm > J->mmax) J->jm = J->mmax;
      if (J->jm < -J->mmax) J->jm = -J->mmax;
      j = J->jm - old;
      A->w -= REACT * A->ii * j, B->w += B->ii * j;
    }
    /* across the axis: the wheel stays on it */
    float v = (B->vx - A->vx) * J->px + (B->vy - A->vy) * J->py - J->sAy * A->w;
    float j = J->my * (J->bias - v);
    B->vx += B->im * j * J->px, B->vy += B->im * j * J->py;
    A->vx -= A->im * j * J->px, A->vy -= A->im * j * J->py, A->w -= A->ii * j * J->sAy;
    /* along it: the bump stop (s >= 0) and the end of its travel (s <= smax) */
    float vs = (B->vx - A->vx) * J->dx + (B->vy - A->vy) * J->dy - J->sAx * A->w;
    if (J->s < 0.02f) {
      float bias = J->s < 0 ? -BETA / DT * J->s : 0;
      float jl = J->mx * (bias - vs), old = J->jlo;
      J->jlo = old + jl > 0 ? old + jl : 0;
      jl = J->jlo - old;
      B->vx += B->im * jl * J->dx, B->vy += B->im * jl * J->dy;
      A->vx -= A->im * jl * J->dx, A->vy -= A->im * jl * J->dy, A->w -= A->ii * jl * J->sAx;
    } else if (J->s > c->smax - 0.02f) {
      float bias = J->s > c->smax ? -BETA / DT * (J->s - c->smax) : 0;
      float jl = J->mx * (bias - vs), old = J->jlo;
      J->jlo = old + jl < 0 ? old + jl : 0;
      jl = J->jlo - old;
      B->vx += B->im * jl * J->dx, B->vy += B->im * jl * J->dy;
      A->vx -= A->im * jl * J->dx, A->vy -= A->im * jl * J->dy, A->w -= A->ii * jl * J->sAx;
    }
  }
}

static void integrate(Body *b) {
  b->x += b->vx * DT, b->y += b->vy * DT, b->a += b->w * DT;
}

void car_step(Car *c, float gas, float brake, bool fuel) {
  float g = stage->gravity;
  for (int i = 0; i < c->nw; i++) c->ground[i] = false;
  c->touch = false;
  c->tree_broken = -1;
  c->last_vx = c->ch.vx, c->last_vy = c->ch.vy;
  for (int sub = 0; sub < SUB; sub++) {
    /* forces: gravity, downforce, turning in the air */
    c->ch.vy -= g * DT;
    for (int i = 0; i < c->nw; i++) c->wh[i].vy -= g * DT;
    bool air = true;
    for (int i = 0; i < c->nw; i++) air = air && !c->ground[i];
    if (c->down > 0) {
      float v2 = c->ch.vx * c->ch.vx + c->ch.vy * c->ch.vy, f = c->down * v2 * c->ch.im * DT;
      if (f > g * 1.2f * DT) f = g * 1.2f * DT;
      c->ch.vx += sinf(c->ch.a) * f, c->ch.vy -= cosf(c->ch.a) * f;
    }
    if (air && !c->touch && !c->head_hit) c->ch.w += c->air * (gas - brake) * c->ch.ii * DT;
    /* a little air: nothing spins forever */
    c->ch.w *= 1 - 0.15f * DT;
    find_contacts(c);
    prepare_contacts();
    prepare_joints(c, gas, brake, fuel);
    for (int it = 0; it < ITER; it++) {
      solve_joints(c);
      solve_contacts();
    }
    /* trees that took too hard a knock fall */
    for (int i = 0; i < ncon; i++)
      if (con[i].tree >= 0 && con[i].jn > 55.0f) {
        stage_tree_fall(con[i].tree);
        c->tree_broken = con[i].tree;
      }
    integrate(&c->ch);
    for (int i = 0; i < c->nw; i++) {
      integrate(&c->wh[i]);
      c->spin[i] += c->wh[i].w * DT;
    }
  }
  /* Bill's head: does it touch the ground (or a cave's roof)? */
  if (!c->head_hit) {
    float hx, hy, px, py, nx, ny, d;
    float ca = cosf(c->ch.a), sa = sinf(c->ch.a);
    hx = c->ch.x + ca * c->head[0] - sa * c->head[1], hy = c->ch.y + sa * c->head[0] + ca * c->head[1];
    if (circle_line(hx, hy, BILL_HEAD_R * 0.92f, false, &px, &py, &nx, &ny, &d) ||
        (stage->ceiling && circle_line(hx, hy, BILL_HEAD_R * 0.92f, true, &px, &py, &nx, &ny, &d)))
      c->head_hit = true;
  }
  /* his head nods on its neck: a spring pulled by how the chassis speeds up */
  float ax = (c->ch.vx - c->last_vx) * CAR_HZ, ay = (c->ch.vy - c->last_vy) * CAR_HZ + g;
  float ca = cosf(-c->ch.a), sa = sinf(-c->ch.a), lx = ca * ax - sa * ay, ly = sa * ax + ca * ay;
  for (int k = 0; k < 2; k++) {
    float f = -160 * c->wob[k] - 14 * c->wobv[k] - (k ? ly - g : lx) * 0.6f;
    c->wobv[k] += f / CAR_HZ;
    c->wob[k] += c->wobv[k] / CAR_HZ;
    if (c->wob[k] > 0.12f) c->wob[k] = 0.12f, c->wobv[k] = 0;
    if (c->wob[k] < -0.12f) c->wob[k] = -0.12f, c->wobv[k] = 0;
  }
}

void car_origin(const Car *c, float *x, float *y) { car_point(c, 0, 0, x, y); }

void car_point(const Car *c, float lx, float ly, float *wx, float *wy) {
  float ox, oy;
  rot(c->ch.a, lx - c->com[0], ly - c->com[1], &ox, &oy);
  *wx = c->ch.x + ox, *wy = c->ch.y + oy;
}

void car_head(const Car *c, float *x, float *y) {
  float ox, oy;
  rot(c->ch.a, c->head[0] + c->wob[0], c->head[1] + c->wob[1], &ox, &oy);
  *x = c->ch.x + ox, *y = c->ch.y + oy;
}

float car_speed(const Car *c) { return sqrtf(c->ch.vx * c->ch.vx + c->ch.vy * c->ch.vy); }

bool car_airborne(const Car *c) {
  for (int i = 0; i < c->nw; i++)
    if (c->ground[i]) return false;
  return !c->touch;
}
