/* PolyGT 3D's picture: low-poly, flat-shaded triangles seen from behind your car.
 *
 * A frame is set up once (rd_race, rd_showroom): the road's segments ahead, the roadside, the orbs and the cars are
 * turned into the camera's space, cut at the near plane, projected, and kept as screen triangles with their depth
 * (1/z, which is flat across the screen). rd_draw then fills a strip of rows at a time: the sky, the hills and the
 * ground, then every triangle crossing the strip, nearest wins. Edges are walked in floats with a fill rule, so
 * neighbouring triangles never leave a gap or overlap. */
#include <string.h>
#include "../../../launcher/src/gfx.h"
#include "render.h"

#define CX 160.0f
#define CY 115.0f
#define FOCAL 205.0f
#define NEAR 1.0f
#define FAR 215.0f
#define FOG0 55.0f
#define MAX_TRIS 1400
#define DRAW_SEGS 34                /* segments ahead */
#define DETAIL_SEGS 14              /* ...with lines and stripes */
#define GUARD 1000.0f               /* screen triangles are cut to this far outside the screen */

typedef struct { float x, y, z; } v3;

typedef struct {
  int16_t x[3], y[3];               /* 1/16 pixel, top to bottom */
  uint16_t w[3];                    /* NEAR/z, 65535 = at the near plane */
  uint16_t c;
  uint8_t stip, pad;
  int16_t y0, y1;                   /* the rows it covers */
} tri;

static tri tris[MAX_TRIS];
static int ntris;
int rd_dropped, rd_tris;

/* the camera */
static int32_t cam_x, cam_y, cam_z;          /* where it is (FX) */
static float cy_c, cy_s, cp_c, cp_s, cr_c, cr_s;
static int32_t cam_yaw;
static int horizon;
/* the background */
static uint8_t sky_top[3], sky_bot[3], fog[3], ground[3], hill[3];
static uint8_t hills[SCREEN_W];

/* ------------------------------------------------------------------ colours */
typedef struct {
  uint8_t sky_top[3], sky_bot[3], ground[3], road[3], kerb[3], tree[3], wall[3];
  uint8_t lamps;
} theme;
static const theme THEMES[8] = {
  {{35, 25, 85}, {235, 110, 80}, {35, 85, 42}, {62, 64, 72}, {220, 35, 35}, {24, 95, 44}, {214, 214, 222}, 0},
  {{10, 12, 32}, {55, 25, 85}, {22, 24, 34}, {46, 50, 62}, {0, 195, 235}, {90, 220, 255}, {104, 110, 128}, 1},
  {{45, 35, 60}, {165, 130, 145}, {48, 62, 42}, {58, 56, 60}, {230, 165, 20}, {30, 72, 44}, {190, 190, 196}, 0},
  {{25, 60, 140}, {130, 190, 245}, {34, 98, 44}, {62, 65, 72}, {225, 40, 40}, {24, 95, 44}, {214, 214, 222}, 0},
  {{55, 20, 75}, {245, 115, 50}, {180, 98, 46}, {74, 62, 56}, {240, 185, 25}, {205, 120, 52}, {196, 170, 140}, 0},
  {{10, 12, 34}, {70, 25, 105}, {20, 26, 42}, {44, 48, 60}, {0, 215, 255}, {90, 220, 255}, {96, 104, 126}, 1},
  {{38, 82, 150}, {185, 225, 250}, {198, 215, 232}, {68, 76, 92}, {35, 115, 240}, {185, 220, 245}, {150, 160, 176}, 0},
  {{36, 16, 38}, {215, 65, 35}, {44, 36, 38}, {48, 44, 46}, {255, 95, 15}, {235, 85, 28}, {120, 110, 112}, 0},
};
const uint8_t rd_paint[8][3] = {{245, 195, 25}, {30, 205, 95}, {175, 55, 235}, {245, 110, 25},
                                          {50, 195, 240}, {215, 215, 225}, {235, 45, 135}, {40, 85, 220}};

uint16_t rd_rgb(const uint8_t c[3]) { return (uint16_t)((c[0] & 0xF8) << 8 | (c[1] & 0xFC) << 3 | c[2] >> 3); }
static uint8_t cl8(int v) { return (uint8_t)(v < 0 ? 0 : v > 255 ? 255 : v); }
static void mix3(uint8_t *o, const uint8_t *a, const uint8_t *b, int t256) {
  for (int i = 0; i < 3; i++) o[i] = (uint8_t)(a[i] + (b[i] - a[i]) * t256 / 256);
}

/* ------------------------------------------------------------------ small float maths */
static float fsinu(int32_t a) { return (float)pg_sin(a) * (1.0f / 32767.0f); }
static float fcosu(int32_t a) { return (float)pg_cos(a) * (1.0f / 32767.0f); }
static int iceil(float f) {
  int i = (int)f;
  return (float)i < f ? i + 1 : i;
}
static float clampf(float v, float lo, float hi) { return v < lo ? lo : v > hi ? hi : v; }

/* a point of the world (FX), in the camera's space */
static v3 to_cam_f(float dx, float dy, float dz) {
  float x1 = dx * cy_c - dz * cy_s, z1 = dx * cy_s + dz * cy_c;
  float y2 = dy * cp_c - z1 * cp_s, z2 = dy * cp_s + z1 * cp_c;
  v3 o = {x1 * cr_c - y2 * cr_s, x1 * cr_s + y2 * cr_c, z2};
  return o;
}
static v3 to_cam(int32_t wx, int32_t wy, int32_t wz) {
  return to_cam_f((float)(wx - cam_x) * (1.0f / FX), (float)(wy - cam_y) * (1.0f / FX), (float)(wz - cam_z) * (1.0f / FX));
}

/* ------------------------------------------------------------------ triangles */
typedef struct { float x, y, w; } sv;

static void store(const sv *a, const sv *b, const sv *c, uint16_t col, uint8_t stip) {
  if (ntris >= MAX_TRIS) {
    rd_dropped = 1;
    return;
  }
  const sv *p[3] = {a, b, c};
  /* top to bottom */
  for (int i = 0; i < 2; i++)
    for (int j = 0; j < 2 - i; j++)
      if (p[j]->y > p[j + 1]->y) {
        const sv *t = p[j];
        p[j] = p[j + 1], p[j + 1] = t;
      }
  int y0 = iceil(p[0]->y - 0.5f), y1 = iceil(p[2]->y - 0.5f);
  if (y0 < 0) y0 = 0;
  if (y1 > SCREEN_H) y1 = SCREEN_H;
  if (y1 <= y0) return;
  float xmin = p[0]->x, xmax = p[0]->x;
  for (int i = 1; i < 3; i++) {
    if (p[i]->x < xmin) xmin = p[i]->x;
    if (p[i]->x > xmax) xmax = p[i]->x;
  }
  if (xmax < 0 || xmin > SCREEN_W) return;
  tri *t = &tris[ntris++];
  for (int i = 0; i < 3; i++) {
    t->x[i] = (int16_t)(p[i]->x * 16.0f);
    t->y[i] = (int16_t)(p[i]->y * 16.0f);
    t->w[i] = (uint16_t)(clampf(p[i]->w, 0.0f, 1.0f) * 65535.0f);
  }
  t->c = col, t->stip = stip, t->y0 = (int16_t)y0, t->y1 = (int16_t)y1;
}

/* a convex polygon on the screen, cut to the guard band when it reaches far outside */
static void screen_poly(sv *v, int n, uint16_t col, uint8_t stip) {
  bool far_out = false;
  for (int i = 0; i < n; i++)
    if (v[i].x < -GUARD || v[i].x > SCREEN_W + GUARD || v[i].y < -GUARD || v[i].y > SCREEN_H + GUARD) far_out = true;
  if (far_out) {
    static const float LIM[4] = {-GUARD, SCREEN_W + GUARD, -GUARD, SCREEN_H + GUARD};
    sv buf[2][12];
    int m = n;
    memcpy(buf[0], v, sizeof(sv) * (size_t)n);
    int cur = 0;
    for (int e = 0; e < 4 && m; e++) {
      sv *in = buf[cur], *out = buf[cur ^ 1];
      int k = 0;
      for (int i = 0; i < m; i++) {
        sv *A = &in[i], *B = &in[(i + 1) % m];
        float a = e < 2 ? A->x : A->y, b = e < 2 ? B->x : B->y;
        bool ain = (e & 1) ? a <= LIM[e] : a >= LIM[e], bin = (e & 1) ? b <= LIM[e] : b >= LIM[e];
        if (ain) out[k++] = *A;
        if (ain != bin && k < 12) {
          float t = (LIM[e] - a) / (b - a);
          sv q = {A->x + (B->x - A->x) * t, A->y + (B->y - A->y) * t, A->w + (B->w - A->w) * t};
          out[k++] = q;
        }
      }
      m = k, cur ^= 1;
    }
    for (int i = 1; i + 1 < m; i++) store(&buf[cur][0], &buf[cur][i], &buf[cur][i + 1], col, stip);
    return;
  }
  for (int i = 1; i + 1 < n; i++) store(&v[0], &v[i], &v[i + 1], col, stip);
}

/* a polygon in the camera's space (3 or 4 corners, convex), cut at the near plane, fogged, projected */
static void poly(const v3 *p, int n, const uint8_t *rgb, uint8_t stip) {
  v3 c[6];
  int m = 0;
  bool any_far = true;
  for (int i = 0; i < n; i++) {
    const v3 *A = &p[i], *B = &p[(i + 1) % n];
    bool ain = A->z >= NEAR, bin = B->z >= NEAR;
    if (A->z < FAR) any_far = false;
    if (ain) c[m++] = *A;
    if (ain != bin) {
      float t = (NEAR - A->z) / (B->z - A->z);
      v3 q = {A->x + (B->x - A->x) * t, A->y + (B->y - A->y) * t, NEAR};
      c[m++] = q;
    }
  }
  if (m < 3 || any_far) return;
  /* fog towards the horizon's colour */
  float zs = 0;
  for (int i = 0; i < n; i++) zs += p[i].z;
  zs /= (float)n;
  uint8_t col[3] = {rgb[0], rgb[1], rgb[2]};
  if (zs > FOG0) {
    int t = (int)(clampf((zs - FOG0) * (1.0f / (FAR - FOG0)), 0.0f, 0.9f) * 256.0f);
    mix3(col, rgb, fog, t);
  }
  sv s[6];
  for (int i = 0; i < m; i++) {
    float inv = 1.0f / c[i].z;
    s[i].x = CX + c[i].x * FOCAL * inv;
    s[i].y = CY - c[i].y * FOCAL * inv;
    s[i].w = NEAR * inv;
  }
  screen_poly(s, m, rd_rgb(col), stip);
}

static void quad(v3 a, v3 b, v3 c, v3 d, const uint8_t *rgb) {
  v3 p[4] = {a, b, c, d};
  poly(p, 4, rgb, 0);
}
static void tri3(v3 a, v3 b, v3 c, const uint8_t *rgb, uint8_t stip) {
  v3 p[3] = {a, b, c};
  poly(p, 3, rgb, stip);
}

/* a face of a solid object: only when it faces the camera (seen from outside its centre o) */
static bool facing(const v3 *p, int n, v3 o) {
  v3 u = {p[1].x - p[0].x, p[1].y - p[0].y, p[1].z - p[0].z}, v = {p[n - 1].x - p[0].x, p[n - 1].y - p[0].y, p[n - 1].z - p[0].z};
  v3 nn = {u.y * v.z - u.z * v.y, u.z * v.x - u.x * v.z, u.x * v.y - u.y * v.x};
  float fx = 0, fy = 0, fz = 0;
  for (int i = 0; i < n; i++) fx += p[i].x, fy += p[i].y, fz += p[i].z;
  fx /= (float)n, fy /= (float)n, fz /= (float)n;
  float out = nn.x * (fx - o.x) + nn.y * (fy - o.y) + nn.z * (fz - o.z);
  float view = nn.x * fx + nn.y * fy + nn.z * fz;
  return out * view < 0;
}

/* ------------------------------------------------------------------ objects: a frame of axes in the camera's space */
typedef struct {
  v3 o, r, u, f;                    /* origin, right, up, forward */
} frame;

static v3 at(const frame *F, float x, float y, float z) {
  v3 p = {F->o.x + F->r.x * x + F->u.x * y + F->f.x * z, F->o.y + F->r.y * x + F->u.y * y + F->f.y * z,
          F->o.z + F->r.z * x + F->u.z * y + F->f.z * z};
  return p;
}

/* an object standing at a world point, pointing at heading h (angle units); roll and pitch in radians */
static frame frame_at(int32_t wx, int32_t wy, int32_t wz, int32_t h, float roll, float pitch) {
  frame F;
  F.o = to_cam(wx, wy, wz);
  float s = fsinu(h), c = fcosu(h);
  v3 zero = to_cam_f(0, 0, 0);
  v3 R = to_cam_f(c, 0, -s), U = to_cam_f(0, 1, 0), Fw = to_cam_f(s, 0, c);
  R.x -= zero.x, R.y -= zero.y, R.z -= zero.z;
  U.x -= zero.x, U.y -= zero.y, U.z -= zero.z;
  Fw.x -= zero.x, Fw.y -= zero.y, Fw.z -= zero.z;
  /* small tilts: the body leans in a bend and pitches under braking */
  F.r = (v3){R.x + U.x * roll, R.y + U.y * roll, R.z + U.z * roll};
  F.u = (v3){U.x - R.x * roll + Fw.x * pitch, U.y - R.y * roll + Fw.y * pitch, U.z - R.z * roll + Fw.z * pitch};
  F.f = (v3){Fw.x - U.x * pitch, Fw.y - U.y * pitch, Fw.z - U.z * pitch};
  return F;
}

static void lquad(const frame *F, const float *a, const float *b, const float *c, const float *d, const uint8_t *rgb,
                  bool solid, v3 centre) {
  v3 p[4] = {at(F, a[0], a[1], a[2]), at(F, b[0], b[1], b[2]), at(F, c[0], c[1], c[2]), at(F, d[0], d[1], d[2])};
  if (solid && !facing(p, 4, centre)) return;
  poly(p, 4, rgb, 0);
}

/* ------------------------------------------------------------------ cars */
static const float CAR_V[18][3] = {
  {-1.15f, 0.28f, -2.20f}, {1.15f, 0.28f, -2.20f}, {-1.08f, 0.82f, -2.12f}, {1.08f, 0.82f, -2.12f},
  {-1.20f, 0.24f, 0.20f}, {1.20f, 0.24f, 0.20f}, {-1.12f, 0.80f, 0.55f}, {1.12f, 0.80f, 0.55f},
  {-1.10f, 0.24f, 2.25f}, {1.10f, 0.24f, 2.25f}, {-1.02f, 0.64f, 2.15f}, {1.02f, 0.64f, 2.15f},
  {-1.04f, 0.84f, -1.30f}, {1.04f, 0.84f, -1.30f}, {-0.82f, 1.24f, -0.95f}, {0.82f, 1.24f, -0.95f},
  {-0.78f, 1.20f, 0.15f}, {0.78f, 1.20f, 0.15f}};
/* the body's faces: corners, and which colour (0 body, 1 side, 2 dark, 3 glass, 4 roof) */
static const uint8_t CAR_F[15][5] = {
  {0, 1, 3, 2, 2}, {2, 3, 13, 12, 0}, {12, 13, 15, 14, 3}, {14, 15, 17, 16, 4}, {16, 17, 7, 6, 3}, {6, 7, 11, 10, 0},
  {10, 11, 9, 8, 2}, {0, 4, 6, 2, 1}, {4, 8, 10, 6, 1}, {12, 6, 16, 14, 3}, {5, 1, 3, 7, 1}, {9, 5, 7, 11, 1},
  {7, 13, 15, 17, 3}, {2, 6, 12, 2, 1}, {3, 13, 7, 3, 1}};

typedef struct {
  int32_t wx, wy, wz, head;
  float roll, pitch, steer;
  int model;
  const uint8_t *paint;
  bool brake, flame, boost, smoke, ghost, near;
} car_pose;

static void horse(const frame *F, const car_pose *P, uint32_t clock);

static void car(const car_pose *P, uint32_t clock) {
  frame F = frame_at(P->wx, P->wy, P->wz, P->head, P->roll, P->pitch);
  if (F.o.z < -6.0f || F.o.z > FAR) return;
  if (F.o.z > 0 && (F.o.x > F.o.z * 1.2f + 6 || F.o.x < -F.o.z * 1.2f - 6)) return;   /* out to the side */
  if (pg_vehicles[P->model].horse) {
    horse(&F, P, clock);
    return;
  }
  static const uint8_t GHOST[3] = {70, 220, 250};
  uint8_t b[3];
  for (int i = 0; i < 3; i++) b[i] = P->ghost ? GHOST[i] : P->paint[i];
  uint8_t col[5][3];
  for (int i = 0; i < 3; i++) {
    col[0][i] = b[i];
    col[1][i] = (uint8_t)(b[i] * 5 / 8);
    col[2][i] = (uint8_t)(b[i] * 3 / 4);
    col[3][i] = P->ghost ? (uint8_t)(b[i] * 3 / 4) : (uint8_t)(i == 0 ? 32 : i == 1 ? 48 : 75);
    col[4][i] = P->model == 1 && !P->ghost ? (uint8_t)(i == 2 ? 40 : 30) : b[i];
  }
  v3 centre = at(&F, 0, 0.6f, 0);
  float roof = P->model == 1 ? 1.18f : P->model == 2 ? 1.32f : 1.24f;
  v3 V[18];
  for (int i = 0; i < 18; i++) V[i] = at(&F, CAR_V[i][0], i >= 14 ? (i >= 16 ? roof - 0.04f : roof) : CAR_V[i][1], CAR_V[i][2]);
  bool far_away = F.o.z > 70.0f;
  uint8_t stip = P->ghost ? 1 : 0;
  if (!P->ghost) {   /* the shadow */
    static const uint8_t SH[3] = {14, 14, 20};
    v3 s[4] = {at(&F, -1.25f, 0.04f, -2.3f), at(&F, 1.25f, 0.04f, -2.3f), at(&F, 1.25f, 0.04f, 2.3f), at(&F, -1.25f, 0.04f, 2.3f)};
    poly(s, 4, SH, 0);
  }
  for (int k = 0; k < 15; k++) {
    const uint8_t *f = CAR_F[k];
    if (far_away && (k == 9 || k >= 12)) continue;
    v3 p[4] = {V[f[0]], V[f[1]], V[f[2]], V[f[3]]};
    int n = f[3] == f[0] ? 3 : 4;
    if (!facing(p, n, centre)) continue;
    poly(p, n, col[f[4]], stip);
  }
  if (far_away) return;
  /* the wing, the lights, the plate, the wheels */
  static const uint8_t WING[3] = {35, 35, 42}, TYRE[3] = {24, 24, 28};
  float wl0[3] = {-1.14f, 1.08f, -2.08f}, wr0[3] = {1.14f, 1.08f, -2.08f}, wr1[3] = {1.14f, 1.02f, -1.78f},
        wl1[3] = {-1.14f, 1.02f, -1.78f};
  if (!P->ghost) lquad(&F, wl0, wr0, wr1, wl1, WING, false, centre);
  if (P->ghost) return;
  const pg_vehicle *veh = &pg_vehicles[P->model];
  uint8_t tl[3] = {(uint8_t)(P->brake ? 255 : 185), (uint8_t)(P->brake ? 55 : 15), (uint8_t)(P->brake ? 40 : 20)};
  float a0[3] = {-1.02f, 0.56f, -2.23f}, a1[3] = {-0.42f, 0.56f, -2.23f}, a2[3] = {-0.42f, 0.74f, -2.18f}, a3[3] = {-1.02f, 0.74f, -2.18f};
  lquad(&F, a0, a1, a2, a3, tl, true, centre);
  float b0[3] = {0.42f, 0.56f, -2.23f}, b1[3] = {1.02f, 0.56f, -2.23f}, b2[3] = {1.02f, 0.74f, -2.18f}, b3[3] = {0.42f, 0.74f, -2.18f};
  lquad(&F, b0, b1, b2, b3, tl, true, centre);
  float p0[3] = {-0.24f, 0.36f, -2.23f}, p1[3] = {0.24f, 0.36f, -2.23f}, p2[3] = {0.24f, 0.54f, -2.21f}, p3[3] = {-0.24f, 0.54f, -2.21f};
  lquad(&F, p0, p1, p2, p3, veh->accent, true, centre);
  static const float WX[4] = {-1.18f, 1.18f, -1.16f, 1.16f}, WZ[4] = {-1.38f, -1.38f, 1.38f, 1.38f};
  uint8_t hub[3] = {(uint8_t)(veh->accent[0] / 2), (uint8_t)(veh->accent[1] / 2), (uint8_t)(veh->accent[2] / 2)};
  for (int w = 0; w < 4; w++) {
    float xin = WX[w] > 0 ? WX[w] - 0.22f : WX[w] + 0.22f, xo = WX[w];
    float q0[3] = {xin, 0.05f, WZ[w] - 0.36f}, q1[3] = {xo, 0.05f, WZ[w] - 0.36f}, q2[3] = {xo, 0.58f, WZ[w] - 0.36f},
          q3[3] = {xin, 0.58f, WZ[w] - 0.36f};
    lquad(&F, q0, q1, q2, q3, TYRE, false, centre);
    float s0[3] = {xo, 0.08f, WZ[w] - 0.34f}, s1[3] = {xo, 0.08f, WZ[w] + 0.34f}, s2[3] = {xo, 0.56f, WZ[w] + 0.34f},
          s3[3] = {xo, 0.56f, WZ[w] - 0.34f};
    lquad(&F, s0, s1, s2, s3, hub, true, centre);
  }
  if (P->boost || P->flame) {   /* nitro (blue) or a shift (orange), out of the exhausts */
    static const uint8_t FLAME[2][3] = {{255, 195, 40}, {40, 240, 255}};
    float len = P->boost ? ((clock / 40) & 1 ? -3.45f : -3.1f) : -2.9f;
    for (int side = -1; side <= 1; side += 2) {
      tri3(at(&F, side * 0.76f, 0.30f, -2.24f), at(&F, side * 0.36f, 0.30f, -2.24f), at(&F, side * 0.56f, 0.36f, len),
           FLAME[P->boost ? 1 : 0], 0);
    }
  }
  if (P->smoke) {
    static const uint8_t SMOKE[3] = {215, 215, 225};
    float j = (clock / 40) & 1 ? 0.25f : -0.25f;
    for (int side = -1; side <= 1; side += 2)
      tri3(at(&F, side * 1.25f + j, 0.10f, -2.1f), at(&F, side * 0.65f + j, 0.10f, -2.6f), at(&F, side * 0.95f, 0.75f, -2.5f),
           SMOKE, 1);
  }
}

/* Boris-TH's horses: a galloping body, head turned with the steering */
static void hq(const frame *F, float ax, float ay, float az, float bx, float by, float bz, float cx, float cy, float cz,
               float dx, float dy, float dz, const uint8_t *rgb, uint8_t stip) {
  v3 p[4] = {at(F, ax, ay, az), at(F, bx, by, bz), at(F, cx, cy, cz), at(F, dx, dy, dz)};
  poly(p, 4, rgb, stip);
}
static void ht(const frame *F, float ax, float ay, float az, float bx, float by, float bz, float cx, float cy, float cz,
               const uint8_t *rgb, uint8_t stip) {
  tri3(at(F, ax, ay, az), at(F, bx, by, bz), at(F, cx, cy, cz), rgb, stip);
}

static float fabsf_(float x) { return x < 0 ? -x : x; }
static float fsin(float x) {   /* x in radians */
  int32_t a = (int32_t)(x * 10430.378f);
  return fsinu(a);
}

static void horse(const frame *F, const car_pose *P, uint32_t clock) {
  bool brown = P->model == 4, g = P->ghost;
  uint8_t stip = g ? 1 : 0;
  /* top, side, dark, mane: the ghost, the brown mustang, the black stallion */
  static const uint8_t COAT[3][4][3] = {{{70, 220, 250}, {55, 185, 225}, {40, 140, 180}, {90, 240, 255}},
                                        {{168, 98, 50}, {140, 78, 38}, {96, 52, 24}, {36, 22, 14}},
                                        {{38, 38, 48}, {24, 24, 32}, {14, 14, 20}, {10, 10, 14}}};
  int coat = g ? 0 : brown ? 1 : 2;
  const uint8_t *top = COAT[coat][0], *side = COAT[coat][1], *dark = COAT[coat][2], *mane = COAT[coat][3];
  float speed = P->near ? 1.0f : 0.8f;
  float phase = (float)clock * 0.0168f * speed;
  float bob = 0.06f * fabsf_(fsin(phase));
  float turn = clampf(P->steer * 0.45f, -0.45f, 0.45f);
  if (!g) {
    static const uint8_t SH[3] = {14, 14, 20};
    hq(F, -0.6f, 0.04f, -1.85f, 0.6f, 0.04f, -1.85f, 0.6f, 0.04f, 1.85f, -0.6f, 0.04f, 1.85f, SH, 0);
  }
  float y0 = 0.86f + bob, y1 = 1.44f + bob;
  /* the body */
  hq(F, -0.42f, y0, -1.38f, 0.42f, y0, -1.38f, 0.38f, y1, -1.28f, -0.38f, y1, -1.28f, dark, stip);
  hq(F, -0.38f, y1, -1.28f, 0.38f, y1, -1.28f, 0.40f, y1 + 0.02f, 0.08f, -0.40f, y1 + 0.02f, 0.08f, top, stip);
  hq(F, -0.40f, y1 + 0.02f, 0.08f, 0.40f, y1 + 0.02f, 0.08f, 0.36f, y1 + 0.08f, 1.18f, -0.36f, y1 + 0.08f, 1.18f, top, stip);
  hq(F, -0.36f, y1 + 0.08f, 1.18f, 0.36f, y1 + 0.08f, 1.18f, 0.42f, y0, 1.25f, -0.42f, y0, 1.25f, side, stip);
  hq(F, -0.42f, y0, -1.38f, -0.46f, y0 - 0.04f, 0.05f, -0.40f, y1 + 0.02f, 0.08f, -0.38f, y1, -1.28f, side, stip);
  hq(F, -0.46f, y0 - 0.04f, 0.05f, -0.42f, y0, 1.25f, -0.36f, y1 + 0.08f, 1.18f, -0.40f, y1 + 0.02f, 0.08f, side, stip);
  hq(F, 0.46f, y0 - 0.04f, 0.05f, 0.42f, y0, -1.38f, 0.38f, y1, -1.28f, 0.40f, y1 + 0.02f, 0.08f, side, stip);
  hq(F, 0.42f, y0, 1.25f, 0.46f, y0 - 0.04f, 0.05f, 0.40f, y1 + 0.02f, 0.08f, 0.36f, y1 + 0.08f, 1.18f, side, stip);
  if (!g) {   /* the saddle */
    uint8_t sd[3] = {(uint8_t)(brown ? 75 : 185), (uint8_t)(brown ? 42 : 28), (uint8_t)(brown ? 20 : 35)};
    hq(F, -0.42f, y1 + 0.04f, -0.48f, 0.42f, y1 + 0.04f, -0.48f, 0.42f, y1 + 0.05f, 0.42f, -0.42f, y1 + 0.05f, 0.42f, sd, 0);
  }
  /* the neck and head */
  float nt = turn * 0.32f;
  hq(F, -0.22f, y1 - 0.06f, 0.88f, 0.22f, y1 - 0.06f, 0.88f, nt + 0.18f, y1 + 0.76f, 1.38f, nt - 0.18f, y1 + 0.76f, 1.38f, top, stip);
  hq(F, -0.20f, y1 - 0.18f, 1.34f, -0.22f, y1 - 0.06f, 0.88f, nt - 0.18f, y1 + 0.76f, 1.38f, nt - 0.16f, y1 + 0.52f, 1.72f, side, stip);
  hq(F, 0.22f, y1 - 0.06f, 0.88f, 0.20f, y1 - 0.18f, 1.34f, nt + 0.16f, y1 + 0.52f, 1.72f, nt + 0.18f, y1 + 0.76f, 1.38f, side, stip);
  hq(F, -0.20f, y1 - 0.18f, 1.34f, 0.20f, y1 - 0.18f, 1.34f, nt + 0.16f, y1 + 0.52f, 1.72f, nt - 0.16f, y1 + 0.52f, 1.72f, dark, stip);
  ht(F, 0.0f, y1 + 0.06f, 0.65f, nt * 0.5f, y1 + 0.52f, 0.95f, nt, y1 + 0.86f, 1.35f, mane, stip);
  float sx = turn * 0.52f;
  static const uint8_t WHITE_BLAZE[3] = {235, 230, 220};
  uint8_t blaze[3];
  for (int i = 0; i < 3; i++) blaze[i] = brown && !g ? WHITE_BLAZE[i] : top[i];
  hq(F, nt - 0.18f, y1 + 0.76f, 1.38f, nt + 0.18f, y1 + 0.76f, 1.38f, sx + 0.12f, y1 + 0.45f, 2.18f, sx - 0.12f, y1 + 0.45f, 2.18f, blaze, stip);
  hq(F, nt - 0.16f, y1 + 0.52f, 1.72f, nt - 0.18f, y1 + 0.76f, 1.38f, sx - 0.12f, y1 + 0.45f, 2.18f, sx - 0.11f, y1 + 0.25f, 2.10f, side, stip);
  hq(F, nt + 0.18f, y1 + 0.76f, 1.38f, nt + 0.16f, y1 + 0.52f, 1.72f, sx + 0.11f, y1 + 0.25f, 2.10f, sx + 0.12f, y1 + 0.45f, 2.18f, side, stip);
  hq(F, sx - 0.12f, y1 + 0.45f, 2.18f, sx + 0.12f, y1 + 0.45f, 2.18f, sx + 0.11f, y1 + 0.25f, 2.10f, sx - 0.11f, y1 + 0.25f, 2.10f, dark, stip);
  ht(F, nt - 0.18f, y1 + 0.74f, 1.40f, nt - 0.08f, y1 + 0.74f, 1.46f, nt - 0.14f, y1 + 1.02f, 1.36f, side, stip);
  ht(F, nt + 0.08f, y1 + 0.74f, 1.46f, nt + 0.18f, y1 + 0.74f, 1.40f, nt + 0.14f, y1 + 1.02f, 1.36f, side, stip);
  /* the tail */
  float sway = fsin(phase * 1.3f) * 0.20f - turn * 0.25f, lift = P->boost ? 0.32f : 0.0f;
  hq(F, -0.14f, y1 - 0.06f, -1.34f, 0.14f, y1 - 0.06f, -1.34f, sway + 0.22f, 0.85f + lift, -1.85f, sway - 0.22f, 0.85f + lift, -1.85f, mane, stip);
  ht(F, sway - 0.22f, 0.85f + lift, -1.85f, sway + 0.22f, 0.85f + lift, -1.85f, sway * 1.3f, 0.32f + lift, -2.20f, mane, stip);
  /* four legs at a gallop */
  static const float LX[4] = {-0.32f, 0.32f, -0.30f, 0.30f}, LZ[4] = {-1.10f, -1.10f, 1.02f, 1.02f}, LP[4] = {0.0f, 2.7f, 1.4f, 4.1f};
  for (int l = 0; l < 4; l++) {
    float a = phase + LP[l];
    float swing = fsin(a) * 0.52f, up = clampf(fsin(a + 1.5708f) * 0.28f, 0.0f, 0.30f);
    float tx = LX[l], tz = LZ[l], bz = LZ[l] + swing, by = 0.04f + up;
    hq(F, tx - 0.09f, y0 + 0.08f, tz - 0.10f, tx + 0.09f, y0 + 0.08f, tz + 0.10f, tx + 0.07f, by, bz + 0.08f, tx - 0.07f, by, bz - 0.08f, dark, stip);
    hq(F, tx, y0 + 0.08f, tz - 0.12f, tx, y0 + 0.08f, tz + 0.12f, tx, by, bz + 0.10f, tx, by, bz - 0.10f, side, stip);
    if (!g) {
      uint8_t hf[3] = {(uint8_t)(P->brake ? 255 : P->boost ? 40 : brown ? 225 : 195),
                       (uint8_t)(P->brake ? 55 : P->boost ? 235 : brown ? 215 : 165),
                       (uint8_t)(P->brake ? 35 : P->boost ? 255 : brown ? 195 : 90)};
      hq(F, tx - 0.10f, by, bz - 0.11f, tx + 0.10f, by, bz - 0.11f, tx + 0.10f, by + 0.16f, bz + 0.11f, tx - 0.10f, by + 0.16f, bz + 0.11f, hf, 0);
    }
  }
}

/* ------------------------------------------------------------------ the road */
enum { O_OUTL, O_WALLL, O_VERGEL, O_KERBL, O_LINEL, O_CL, O_CR, O_LINER, O_KERBR, O_VERGER, O_WALLR, O_OUTR, O_TOPL, O_TOPR, O_N };
static const float OFF[12] = {-38.5f, -12.5f, -9.2f, -8.0f, -7.7f, -0.15f, 0.15f, 7.7f, 8.0f, 9.2f, 12.5f, 38.5f};
static v3 node_pts[DRAW_SEGS + 4][O_N];

static void node_points(const pg_track *t, int32_t k, v3 *out) {
  int i = pg_slot(t, k);
  float nx = fcosu(t->nh[i]), nz = -fsinu(t->nh[i]);
  float bx = (float)(t->px[i] - cam_x) * (1.0f / FX), by = (float)(t->py[i] - cam_y) * (1.0f / FX),
        bz = (float)(t->pz[i] - cam_z) * (1.0f / FX);
  for (int o = 0; o < 12; o++) out[o] = to_cam_f(bx + nx * OFF[o], by, bz + nz * OFF[o]);
  out[O_TOPL] = to_cam_f(bx + nx * OFF[O_WALLL], by + 0.9f, bz + nz * OFF[O_WALLL]);
  out[O_TOPR] = to_cam_f(bx + nx * OFF[O_WALLR], by + 0.9f, bz + nz * OFF[O_WALLR]);
}

static void shade3(uint8_t *o, const uint8_t *c, int d) {
  for (int i = 0; i < 3; i++) o[i] = cl8(c[i] + d);
}

static void road_segment(const pg_track *t, int32_t k, const v3 *A, const v3 *B, int dist) {
  const theme *th = &THEMES[t->zone[pg_slot(t, k)] & 7];
  bool odd = k & 1;
  uint8_t road[3], kerb[3], white[3] = {225, 225, 230}, verge[3], out[3], wall[3];
  shade3(road, th->road, odd ? 0 : 6);
  shade3(verge, th->ground, odd ? 0 : 10);
  shade3(out, th->ground, odd ? -6 : -2);
  for (int i = 0; i < 3; i++) kerb[i] = odd ? white[i] : th->kerb[i];
  shade3(wall, th->wall, odd ? 0 : -24);
  bool detail = dist < DETAIL_SEGS;
  /* the start line: a band of squares */
  if (!t->endless && pg_slot(t, k) == 0) {
    static const uint8_t CHECK[2][3] = {{245, 245, 245}, {25, 25, 30}};
    for (int c = 0; c < 8; c++) {
      float f0 = (float)c / 8, f1 = (float)(c + 1) / 8;
      for (int row = 0; row < 2; row++) {
        float g0 = row * 0.5f, g1 = g0 + 0.5f;
        v3 la = {A[O_KERBL].x + (A[O_KERBR].x - A[O_KERBL].x) * f0, A[O_KERBL].y + (A[O_KERBR].y - A[O_KERBL].y) * f0, A[O_KERBL].z + (A[O_KERBR].z - A[O_KERBL].z) * f0};
        v3 lb = {A[O_KERBL].x + (A[O_KERBR].x - A[O_KERBL].x) * f1, A[O_KERBL].y + (A[O_KERBR].y - A[O_KERBL].y) * f1, A[O_KERBL].z + (A[O_KERBR].z - A[O_KERBL].z) * f1};
        v3 ma = {B[O_KERBL].x + (B[O_KERBR].x - B[O_KERBL].x) * f0, B[O_KERBL].y + (B[O_KERBR].y - B[O_KERBL].y) * f0, B[O_KERBL].z + (B[O_KERBR].z - B[O_KERBL].z) * f0};
        v3 mb = {B[O_KERBL].x + (B[O_KERBR].x - B[O_KERBL].x) * f1, B[O_KERBL].y + (B[O_KERBR].y - B[O_KERBL].y) * f1, B[O_KERBL].z + (B[O_KERBR].z - B[O_KERBL].z) * f1};
        v3 p0 = {la.x + (ma.x - la.x) * g0 * 0.4f, la.y + (ma.y - la.y) * g0 * 0.4f, la.z + (ma.z - la.z) * g0 * 0.4f};
        v3 p1 = {lb.x + (mb.x - lb.x) * g0 * 0.4f, lb.y + (mb.y - lb.y) * g0 * 0.4f, lb.z + (mb.z - lb.z) * g0 * 0.4f};
        v3 p2 = {lb.x + (mb.x - lb.x) * g1 * 0.4f, lb.y + (mb.y - lb.y) * g1 * 0.4f, lb.z + (mb.z - lb.z) * g1 * 0.4f};
        v3 p3 = {la.x + (ma.x - la.x) * g1 * 0.4f, la.y + (ma.y - la.y) * g1 * 0.4f, la.z + (ma.z - la.z) * g1 * 0.4f};
        quad(p0, p1, p2, p3, CHECK[(c + row) & 1]);
      }
      /* the rest of the segment: road */
    }
    v3 m0 = {A[O_KERBL].x + (B[O_KERBL].x - A[O_KERBL].x) * 0.4f, A[O_KERBL].y + (B[O_KERBL].y - A[O_KERBL].y) * 0.4f, A[O_KERBL].z + (B[O_KERBL].z - A[O_KERBL].z) * 0.4f};
    v3 m1 = {A[O_KERBR].x + (B[O_KERBR].x - A[O_KERBR].x) * 0.4f, A[O_KERBR].y + (B[O_KERBR].y - A[O_KERBR].y) * 0.4f, A[O_KERBR].z + (B[O_KERBR].z - A[O_KERBR].z) * 0.4f};
    quad(m0, m1, B[O_KERBR], B[O_KERBL], road);
  } else if (detail) {
    quad(A[O_KERBL], A[O_LINEL], B[O_LINEL], B[O_KERBL], white);
    quad(A[O_LINER], A[O_KERBR], B[O_KERBR], B[O_LINER], white);
    if (!odd) {
      static const uint8_t YEL[3] = {235, 205, 70};
      quad(A[O_LINEL], A[O_CL], B[O_CL], B[O_LINEL], road);
      quad(A[O_CL], A[O_CR], B[O_CR], B[O_CL], YEL);
      quad(A[O_CR], A[O_LINER], B[O_LINER], B[O_CR], road);
    } else {
      quad(A[O_LINEL], A[O_LINER], B[O_LINER], B[O_LINEL], road);
    }
  } else {
    quad(A[O_KERBL], A[O_KERBR], B[O_KERBR], B[O_KERBL], road);
  }
  quad(A[O_VERGEL], A[O_KERBL], B[O_KERBL], B[O_VERGEL], kerb);
  quad(A[O_KERBR], A[O_VERGER], B[O_VERGER], B[O_KERBR], kerb);
  quad(A[O_WALLL], A[O_VERGEL], B[O_VERGEL], B[O_WALLL], verge);
  quad(A[O_VERGER], A[O_WALLR], B[O_WALLR], B[O_VERGER], verge);
  if (dist < 26) {
    quad(A[O_OUTL], A[O_WALLL], B[O_WALLL], B[O_OUTL], out);
    quad(A[O_WALLR], A[O_OUTR], B[O_OUTR], B[O_WALLR], out);
  }
  if (dist < 24) {   /* the barriers: their side facing the road */
    quad(A[O_WALLL], B[O_WALLL], B[O_TOPL], A[O_TOPL], wall);
    quad(B[O_WALLR], A[O_WALLR], A[O_TOPR], B[O_TOPR], wall);
  }
}

/* what stands at a segment */
static void prop(const pg_track *t, int32_t k, int dist, uint32_t clock) {
  int i = pg_slot(t, k);
  int p = t->prop[i] & 7, left = t->prop[i] & 8;
  const theme *th = &THEMES[t->zone[i] & 7];
  float nx = fcosu(t->nh[i]), nz = -fsinu(t->nh[i]), fx = fsinu(t->nh[i]), fz = fcosu(t->nh[i]);
  float bx = (float)(t->px[i] - cam_x) * (1.0f / FX), by = (float)(t->py[i] - cam_y) * (1.0f / FX),
        bz = (float)(t->pz[i] - cam_z) * (1.0f / FX);
  (void)clock;
  if (p == PROP_ARCH || p == PROP_GANTRY) {
    float aw = 13.2f, ah = p == PROP_ARCH ? 7.0f : 6.2f;
    static const uint8_t PIL[3] = {200, 200, 215}, PIL2[3] = {150, 150, 165};
    for (int s = -1; s <= 1; s += 2) {
      float px = bx + nx * aw * (float)s, pz = bz + nz * aw * (float)s;
      float w = 0.45f;
      v3 a = to_cam_f(px - fx * w, by, pz - fz * w), b = to_cam_f(px + fx * w, by, pz + fz * w);
      v3 c = to_cam_f(px + fx * w, by + ah, pz + fz * w), d = to_cam_f(px - fx * w, by + ah, pz - fz * w);
      quad(a, b, c, d, PIL);
      v3 e = to_cam_f(px - nx * w * (float)s - fx * w, by, pz - nz * w * (float)s - fz * w),
         f = to_cam_f(px - nx * w * (float)s - fx * w, by + ah, pz - nz * w * (float)s - fz * w);
      quad(a, e, f, d, PIL2);
    }
    if (p == PROP_ARCH) {
      bool start = !t->endless && i == 0;
      float y0 = by + ah - 1.4f, y1 = by + ah;
      int parts = start ? 8 : 1;
      static const uint8_t CHECK[2][3] = {{245, 245, 245}, {25, 25, 30}};
      for (int c = 0; c < parts; c++) {
        float s0 = -aw + 2 * aw * (float)c / (float)parts, s1 = -aw + 2 * aw * (float)(c + 1) / (float)parts;
        v3 a = to_cam_f(bx + nx * s0, y0, bz + nz * s0), b = to_cam_f(bx + nx * s1, y0, bz + nz * s1);
        v3 cc = to_cam_f(bx + nx * s1, y1, bz + nz * s1), d = to_cam_f(bx + nx * s0, y1, bz + nz * s0);
        if (start) quad(a, b, cc, d, CHECK[c & 1]);
        else quad(a, b, cc, d, th->kerb);
      }
    } else {
      static const uint8_t BEAM[3] = {70, 74, 90}, LIGHT[3] = {255, 240, 190};
      float y0 = by + ah - 0.5f, y1 = by + ah;
      v3 a = to_cam_f(bx - nx * aw, y0, bz - nz * aw), b = to_cam_f(bx + nx * aw, y0, bz + nz * aw);
      v3 c = to_cam_f(bx + nx * aw, y1, bz + nz * aw), d = to_cam_f(bx - nx * aw, y1, bz - nz * aw);
      quad(a, b, c, d, BEAM);
      if (dist < 20)
        for (int l = -1; l <= 1; l++) {
          float c0 = (float)l * 5.0f - 0.9f, c1 = c0 + 1.8f, ly = y0 - 0.02f;
          v3 p0 = to_cam_f(bx + nx * c0, ly, bz + nz * c0), p1 = to_cam_f(bx + nx * c1, ly, bz + nz * c1);
          v3 p2 = to_cam_f(bx + nx * c1 + fx * 0.6f, ly, bz + nz * c1 + fz * 0.6f), p3 = to_cam_f(bx + nx * c0 + fx * 0.6f, ly, bz + nz * c0 + fz * 0.6f);
          quad(p0, p1, p2, p3, LIGHT);
        }
    }
    return;
  }
  float side = left ? -1.0f : 1.0f;
  if (p == PROP_TREE) {
    if (dist > 26) return;
    float d = side * (12.5f + 3.6f);
    float tx = bx + nx * d, tz = bz + nz * d;
    if (th->lamps) {
      static const uint8_t POLE[3] = {70, 80, 100};
      float w = 0.16f;
      v3 a = to_cam_f(tx - fx * w, by, tz - fz * w), b = to_cam_f(tx + fx * w, by, tz + fz * w);
      v3 c = to_cam_f(tx + fx * w, by + 7.0f, tz + fz * w), dd = to_cam_f(tx - fx * w, by + 7.0f, tz - fz * w);
      quad(a, b, c, dd, POLE);
      v3 e = to_cam_f(tx - nx * w, by, tz - nz * w), f = to_cam_f(tx + nx * w, by, tz + nz * w);
      v3 g = to_cam_f(tx + nx * w, by + 7.0f, tz + nz * w), h = to_cam_f(tx - nx * w, by + 7.0f, tz - nz * w);
      quad(e, f, g, h, POLE);
      /* the lamp, leaning over the road */
      float hx = tx - nx * side * 2.2f, hz = tz - nz * side * 2.2f;
      v3 l0 = to_cam_f(tx, by + 7.0f, tz), l1 = to_cam_f(hx, by + 7.2f, hz);
      v3 l2 = to_cam_f(hx + fx * 0.5f, by + 7.0f, hz + fz * 0.5f);
      tri3(l0, l1, l2, POLE, 0);
      v3 q0 = to_cam_f(hx - fx * 0.5f, by + 6.95f, hz - fz * 0.5f), q1 = to_cam_f(hx + fx * 0.5f, by + 6.95f, hz + fz * 0.5f);
      v3 q2 = to_cam_f(hx + fx * 0.5f + nx * 0.6f, by + 6.95f, hz + fz * 0.5f + nz * 0.6f), q3 = to_cam_f(hx - fx * 0.5f + nx * 0.6f, by + 6.95f, hz - fz * 0.5f + nz * 0.6f);
      quad(q0, q1, q2, q3, th->tree);
      return;
    }
    /* a cone of five sides on a short trunk, lit from the left */
    static const uint8_t TRUNK[3] = {70, 48, 30};
    float r = 2.1f, h = 6.8f;
    v3 apex = to_cam_f(tx, by + h, tz);
    v3 base[5];
    for (int s = 0; s < 5; s++) {
      int32_t a = s * 13107 + (int32_t)(k * 977);
      base[s] = to_cam_f(tx + fsinu(a) * r, by + 0.7f, tz + fcosu(a) * r);
    }
    v3 cen = to_cam_f(tx, by + 2.5f, tz);
    for (int s = 0; s < 5; s++) {
      v3 p[3] = {base[s], base[(s + 1) % 5], apex};
      if (!facing(p, 3, cen)) continue;
      int32_t a = s * 13107 + 6553 + (int32_t)(k * 977) - (int32_t)t->nh[i];
      uint8_t c[3];
      shade3(c, th->tree, (int)(fsinu(a) * 22.0f));
      poly(p, 3, c, 0);
    }
    float w = 0.25f;
    v3 a = to_cam_f(tx - fx * w, by, tz - fz * w), b = to_cam_f(tx + fx * w, by, tz + fz * w);
    v3 c = to_cam_f(tx + fx * w, by + 0.8f, tz + fz * w), dd = to_cam_f(tx - fx * w, by + 0.8f, tz - fz * w);
    quad(a, b, c, dd, TRUNK);
    return;
  }
  if (p == PROP_CHEVRON) {
    if (dist > 22) return;
    float d = side * 13.0f;
    float sx = bx + nx * d, sz = bz + nz * d;
    static const uint8_t YEL[3] = {240, 185, 20}, BLK[3] = {25, 25, 30}, POST[3] = {90, 90, 100};
    v3 a = to_cam_f(sx - nx * 1.3f, by + 0.6f, sz - nz * 1.3f), b = to_cam_f(sx + nx * 1.3f, by + 0.6f, sz + nz * 1.3f);
    v3 c = to_cam_f(sx + nx * 1.3f, by + 2.2f, sz + nz * 1.3f), dd = to_cam_f(sx - nx * 1.3f, by + 2.2f, sz - nz * 1.3f);
    quad(a, b, c, dd, YEL);
    v3 p0 = to_cam_f(sx - nx * 0.15f, by, sz - nz * 0.15f), p1 = to_cam_f(sx + nx * 0.15f, by, sz + nz * 0.15f);
    v3 p2 = to_cam_f(sx + nx * 0.15f, by + 0.6f, sz + nz * 0.15f), p3 = to_cam_f(sx - nx * 0.15f, by + 0.6f, sz - nz * 0.15f);
    quad(p0, p1, p2, p3, POST);
    /* the arrow points into the bend (away from the side it stands on) */
    float dir = -side, e = -0.05f;
    for (int c2 = -1; c2 <= 1; c2 += 2) {
      float o = (float)c2 * 0.55f;
      v3 t0 = to_cam_f(sx + nx * (o - dir * 0.35f) + fx * e, by + 0.8f, sz + nz * (o - dir * 0.35f) + fz * e);
      v3 t1 = to_cam_f(sx + nx * (o + dir * 0.35f) + fx * e, by + 1.4f, sz + nz * (o + dir * 0.35f) + fz * e);
      v3 t2 = to_cam_f(sx + nx * (o - dir * 0.35f) + fx * e, by + 2.0f, sz + nz * (o - dir * 0.35f) + fz * e);
      tri3(t0, t1, t2, BLK, 0);
    }
  }
}

static void orb(const pg_track *t, int32_t k, uint32_t clock) {
  int i = pg_slot(t, k);
  int32_t s = k * t->seg + t->seg / 2, wx, wy, wz;
  pg_world(t, s, t->orb[i] * FX / 8, &wx, &wy, &wz);
  float ox = (float)(wx - cam_x) * (1.0f / FX), oz = (float)(wz - cam_z) * (1.0f / FX), gy = (float)(wy - cam_y) * (1.0f / FX);
  float oy = gy + 1.25f + 0.25f * fsinu((int32_t)(clock * 6) + k * 3000);
  int32_t ang = (int32_t)(clock * 16) + k * 5000;
  float rx = fcosu(ang) * 1.1f, rz = fsinu(ang) * 1.1f;
  v3 top = to_cam_f(ox, oy + 1.05f, oz), bot = to_cam_f(ox, oy - 0.85f, oz);
  v3 l = to_cam_f(ox - rx, oy + 0.1f, oz - rz), r = to_cam_f(ox + rx, oy + 0.1f, oz + rz);
  v3 f = to_cam_f(ox - rz, oy + 0.1f, oz + rx), b = to_cam_f(ox + rz, oy + 0.1f, oz - rx);
  static const uint8_t C0[3] = {60, 255, 255}, C1[3] = {0, 210, 255}, C2[3] = {20, 150, 245}, C3[3] = {255, 230, 80},
                       PAD[3] = {0, 150, 210};
  v3 centre = to_cam_f(ox, oy, oz);
  v3 faces[8][3] = {{top, l, f}, {top, f, r}, {top, r, b}, {top, b, l}, {bot, f, l}, {bot, r, f}, {bot, b, r}, {bot, l, b}};
  const uint8_t *cols[8] = {C0, C1, C0, C1, C2, C3, C2, C3};
  for (int q = 0; q < 8; q++)
    if (facing(faces[q], 3, centre)) poly(faces[q], 3, cols[q], 0);
  float nx = fcosu(t->nh[i]), nz = -fsinu(t->nh[i]), fx = fsinu(t->nh[i]), fz = fcosu(t->nh[i]);
  v3 p0 = to_cam_f(ox - nx * 1.1f, gy + 0.05f, oz - nz * 1.1f), p1 = to_cam_f(ox + fx * 1.4f, gy + 0.05f, oz + fz * 1.4f);
  v3 p2 = to_cam_f(ox + nx * 1.1f, gy + 0.05f, oz + nz * 1.1f), p3 = to_cam_f(ox - fx * 1.4f, gy + 0.05f, oz - fz * 1.4f);
  quad(p0, p1, p2, p3, PAD);
}

/* ------------------------------------------------------------------ the background */
static void sky_for(int zone_a, int zone_b, int t256) {
  const theme *a = &THEMES[zone_a & 7], *b = &THEMES[zone_b & 7];
  mix3(sky_top, a->sky_top, b->sky_top, t256);
  mix3(sky_bot, a->sky_bot, b->sky_bot, t256);
  mix3(ground, a->ground, b->ground, t256);
  memcpy(fog, sky_bot, 3);
  for (int i = 0; i < 3; i++) hill[i] = (uint8_t)((sky_top[i] * 3 + sky_bot[i]) / 5);
}

static void setup_camera(int32_t x, int32_t y, int32_t z, int32_t yaw, float pitch, float roll) {
  cam_x = x, cam_y = y, cam_z = z, cam_yaw = yaw;
  cy_c = fcosu(yaw), cy_s = fsinu(yaw);
  float p2 = pitch * pitch, r2 = roll * roll;
  cp_c = 1 - p2 / 2 + p2 * p2 / 24, cp_s = pitch - pitch * p2 / 6;
  cr_c = 1 - r2 / 2, cr_s = roll - roll * r2 / 6;
  horizon = (int)(CY + FOCAL * cp_s / cp_c);
  /* the hills on the horizon turn with the camera: 1024 pixels a turn; three ranges laid over each other */
  int scroll = (int)(((uint32_t)yaw & 0xFFFF) >> 6);
  for (int x2 = 0; x2 < SCREEN_W; x2++) {
    int m = (x2 + scroll) & 1023;
    int a = (m % 128) - 64, b = ((m + 40) % 52) - 26, c = ((m + 11) % 20) - 10;
    int h = 26 - (a < 0 ? -a : a) * 14 / 64 - (b < 0 ? -b : b) * 7 / 26 - (c < 0 ? -c : c) * 3 / 10;
    hills[x2] = (uint8_t)(h < 1 ? 1 : h);
  }
  ntris = 0, rd_dropped = 0;
}

/* ------------------------------------------------------------------ frames */
static int32_t lerp(int32_t a, int32_t b, int alpha) { return a + (int32_t)((int64_t)(b - a) * alpha / 256); }

void rd_race(const pg_track *t, const pg_race *a, const pg_race *b, int alpha, int view, const rd_ghost *g,
             uint32_t clock) {
  const pg_car *ca = &a->car[0], *cb = &b->car[0];
  int32_t s = lerp(ca->s, cb->s, alpha), x = lerp(ca->x, cb->x, alpha);
  int32_t ang = ca->ang + (int32_t)((int64_t)(int16_t)(cb->ang - ca->ang) * alpha / 256);
  int32_t head = pg_heading_at(t, s) + ang;
  int32_t wx, wy, wz;
  pg_world(t, s, x, &wx, &wy, &wz);
  /* behind the car, a little above, looking where it goes */
  float dist = view == VIEW_FAR ? 9.5f : view == VIEW_BUMPER ? -0.6f : 6.8f;
  float high = view == VIEW_FAR ? 3.35f : view == VIEW_BUMPER ? 1.55f : 2.4f;
  bool horse_ = pg_vehicles[cb->model].horse;
  if (horse_ && view == VIEW_CHASE) dist = 6.6f, high = 2.5f;
  if (horse_ && view == VIEW_BUMPER) high = 2.6f, dist = -0.2f;
  int32_t sh = pg_sin(head), ch = pg_cos(head);
  int32_t cx = wx - (int32_t)(dist * (float)sh * (FX / 32767.0f)), cz = wz - (int32_t)(dist * (float)ch * (FX / 32767.0f));
  int32_t ground_y = wy;
  int32_t slope = pg_height_at(t, s + 12 * FX) - pg_height_at(t, s);
  float pitch = -0.11f + clampf((float)slope / (12.0f * FX) * 0.35f, -0.08f, 0.08f);
  float roll = -(float)cb->steer / 1024.0f * clampf((float)cb->v / (75.0f * FX), 0, 1) * 0.15f * 0.35f;
  setup_camera(cx, ground_y + (int32_t)(high * FX), cz, head, pitch, roll);
  /* the sky: the scenery here, blending into the one ahead */
  int32_t k0 = pg_seg_of(t, s);
  int za = t->zone[pg_slot(t, k0)], zb = t->zone[pg_slot(t, k0 + 14)], blend = 0;
  if (za != zb)
    for (int k = 1; k <= 14; k++) blend += t->zone[pg_slot(t, k0 + k)] == zb ? 18 : 0;
  sky_for(za, zb, blend > 256 ? 256 : blend);
  /* the road, from far to near (the depth buffer sorts it out, but the far pieces go first if room runs out) */
  int32_t kc = pg_seg_of(t, s - (int32_t)(dist > 0 ? dist * FX : 0)) - 1;
  int32_t kend = k0 + DRAW_SEGS;
  if (t->endless && kend > t->last) kend = t->last;
  if (t->endless && kc < t->first) kc = t->first;
  int nn = (int)(kend - kc + 1);
  if (nn > DRAW_SEGS + 4) nn = DRAW_SEGS + 4;
  for (int j = 0; j < nn; j++) node_points(t, kc + j, node_pts[j]);
  for (int j = nn - 2; j >= 0; j--) road_segment(t, kc + j, node_pts[j], node_pts[j + 1], (int)(kc + j - k0));
  for (int j = nn - 2; j >= 0; j--) {
    int32_t k = kc + j;
    int i = pg_slot(t, k);
    if (t->prop[i] & 7) prop(t, k, (int)(k - k0), clock);
    if (t->orb[i] != -128 && k - k0 < 28 && k >= k0 - 1) {
      bool here = t->endless ? k > b->last_orb : t->orb_id[i] >= 0 && !b->orb_wait[t->orb_id[i]];
      if (here) orb(t, k, clock);
    }
  }
  /* the rivals */
  for (int i = 1; i <= b->rivals; i++) {
    const pg_car *pa = &a->car[i], *pb = &b->car[i];
    int32_t rs = lerp(pa->s, pb->s, alpha);
    if (!t->endless) rs = s + pg_wrap(t, rs - s);
    if (rs < s - 30 * FX || rs > s + (DRAW_SEGS - 2) * t->seg) continue;
    car_pose P;
    memset(&P, 0, sizeof P);
    pg_world(t, rs, lerp(pa->x, pb->x, alpha), &P.wx, &P.wy, &P.wz);
    P.head = pg_heading_at(t, rs) + pb->ang;
    P.model = pb->model, P.paint = rd_paint[pb->paint & 7];
    P.brake = pb->v < pa->v - FX / 4, P.boost = pb->boost;
    car(&P, clock + (uint32_t)i * 97);
  }
  /* the ghost of your best lap */
  if (g && g->on) {
    int32_t gs = s + pg_wrap(t, g->s - pg_lap_s(b, t));
    if (gs > s - 20 * FX && gs < s + (DRAW_SEGS - 2) * t->seg) {
      car_pose P;
      memset(&P, 0, sizeof P);
      pg_world(t, gs, g->x, &P.wx, &P.wy, &P.wz);
      P.head = pg_heading_at(t, gs) + g->ang, P.model = g->model, P.ghost = true, P.near = true;
      P.paint = pg_vehicles[g->model].body;
      car(&P, clock);
    }
  }
  /* your car */
  if (view != VIEW_BUMPER) {
    car_pose P;
    memset(&P, 0, sizeof P);
    P.wx = wx, P.wy = wy, P.wz = wz;
    int32_t slip = ca->slip + (int32_t)((int64_t)(cb->slip - ca->slip) * alpha / 256);
    P.head = head + slip;
    P.model = cb->model, P.paint = pg_vehicles[cb->model].body;
    float spd = clampf((float)cb->v / (75.0f * FX), 0, 1);
    P.roll = clampf(-(float)cb->steer / 1024.0f * spd * 0.15f - (float)slip * (0.14f / 4700.0f), -0.2f, 0.2f);
    if (cb->bump || (cb->x > PG_ROAD || cb->x < -PG_ROAD)) P.roll += (clock / 40) & 1 ? 0.03f : -0.03f;
    P.pitch = cb->v < ca->v - FX / 6 ? -0.05f : cb->v > ca->v + FX / 8 ? 0.03f : 0.0f;
    P.steer = (float)cb->steer / 1024.0f;
    P.brake = cb->v < ca->v - FX / 6 || cb->drift;
    P.boost = cb->boost, P.flame = b->manual && pg_rpm(b) > pg_vehicles[cb->model].redline - 300;
    P.smoke = (cb->drift || cb->bump) && cb->v > 15 * FX;
    P.near = true;
    car(&P, clock);
  }
}

void rd_showroom(int vehicle, int32_t angle, int zone, uint32_t clock) {
  /* the car on the right of the screen: the camera to its left */
  setup_camera(-(int32_t)(2.3f * FX), (int32_t)(2.2f * FX), -(int32_t)(7.0f * FX), 0, -0.15f, 0);
  sky_for(zone, zone, 0);
  float ox = 2.3f, oy = -2.2f, oz = 7.0f;   /* the turntable's middle, from the camera */
  for (int i = 0; i < 16; i++) {
    int32_t a0 = i * 65536 / 16 + angle / 2, a1 = (i + 1) * 65536 / 16 + angle / 2;
    v3 c = to_cam_f(ox, oy, oz), p = to_cam_f(ox + fcosu(a0) * 3.4f, oy, oz + fsinu(a0) * 3.4f),
       q = to_cam_f(ox + fcosu(a1) * 3.4f, oy, oz + fsinu(a1) * 3.4f);
    uint8_t col[3] = {(uint8_t)((i & 1) ? 40 : 58), (uint8_t)((i & 1) ? 36 : 52), (uint8_t)((i & 1) ? 70 : 92)};
    tri3(c, p, q, col, 0);
  }
  car_pose P;
  memset(&P, 0, sizeof P);
  P.head = angle, P.model = vehicle, P.paint = pg_vehicles[vehicle].body, P.brake = true, P.near = true, P.steer = 0.25f;
  car(&P, clock);
}

/* ------------------------------------------------------------------ drawing a strip */
static uint16_t zbuf[SCREEN_W * 24];
static const uint8_t BAYER[4][4] = {{0, 8, 2, 10}, {12, 4, 14, 6}, {3, 11, 1, 9}, {15, 7, 13, 5}};

static void row_fill(uint16_t *row, int y, const uint8_t *c) {
  uint16_t pat[4];
  for (int i = 0; i < 4; i++) {
    int d = BAYER[y & 3][i];
    int r = c[0] + (d >> 1), g = c[1] + (d >> 2), b = c[2] + (d >> 1);
    pat[i] = (uint16_t)((r > 255 ? 31 : r >> 3) << 11 | (g > 255 ? 63 : g >> 2) << 5 | (b > 255 ? 31 : b >> 3));
  }
  for (int x = 0; x < SCREEN_W; x += 4) row[x] = pat[0], row[x + 1] = pat[1], row[x + 2] = pat[2], row[x + 3] = pat[3];
}

static void raster(const tri *t, int ys, int ye) {
  float x0 = t->x[0] * (1.0f / 16), y0 = t->y[0] * (1.0f / 16), x1 = t->x[1] * (1.0f / 16), y1 = t->y[1] * (1.0f / 16),
        x2 = t->x[2] * (1.0f / 16), y2 = t->y[2] * (1.0f / 16);
  float w0 = t->w[0] * (1.0f / 65535), w1 = t->w[1] * (1.0f / 65535), w2 = t->w[2] * (1.0f / 65535);
  float dx1 = x1 - x0, dy1 = y1 - y0, dx2 = x2 - x0, dy2 = y2 - y0;
  float det = dx1 * dy2 - dx2 * dy1;
  if (det > -0.01f && det < 0.01f) return;
  float B = ((w1 - w0) * dy2 - (w2 - w0) * dy1) / det, C = (dx1 * (w2 - w0) - dx2 * (w1 - w0)) / det;
  float s02 = (x2 - x0) / (y2 - y0);
  float s01 = y1 > y0 ? (x1 - x0) / (y1 - y0) : 0, s12 = y2 > y1 ? (x2 - x1) / (y2 - y1) : 0;
  int ra = t->y0 > ys ? t->y0 : ys, rb = t->y1 < ye ? t->y1 : ye;
  uint16_t col = t->c;
  for (int r = ra; r < rb; r++) {
    float yc = (float)r + 0.5f;
    float xl = x0 + (yc - y0) * s02;
    float xs = yc < y1 ? x0 + (yc - y0) * s01 : x1 + (yc - y1) * s12;
    float xa = xl < xs ? xl : xs, xb = xl < xs ? xs : xl;
    xa = clampf(xa, -1.0f, SCREEN_W + 1.0f), xb = clampf(xb, -1.0f, SCREEN_W + 1.0f);
    int pa = iceil(xa - 0.5f), pb = iceil(xb - 0.5f);
    if (pa < 0) pa = 0;
    if (pb > SCREEN_W) pb = SCREEN_W;
    if (pb <= pa) continue;
    float wa = w0 + B * ((float)pa + 0.5f - x0) + C * (yc - y0);
    float wb = w0 + B * ((float)pb - 0.5f - x0) + C * (yc - y0);
    int32_t qa = (int32_t)(clampf(wa, 0, 1) * 1073741824.0f), qb = (int32_t)(clampf(wb, 0, 1) * 1073741824.0f);
    int n = pb - pa;
    int32_t dq = n > 1 ? (qb - qa) / (n - 1) : 0;
    uint16_t *z = zbuf + (r - gfx_y0) * SCREEN_W, *p = gfx_buf + (r - gfx_y0) * SCREEN_W;
    if (t->stip) {
      for (int x = pa; x < pb; x++, qa += dq) {
        if ((x + r) & 1) continue;
        uint16_t q = (uint16_t)(qa >> 14 > 65535 ? 65535 : qa >> 14);
        if (q > z[x]) z[x] = q, p[x] = col;
      }
    } else {
      for (int x = pa; x < pb; x++, qa += dq) {
        uint16_t q = (uint16_t)(qa >> 14 > 65535 ? 65535 : qa >> 14);
        if (q > z[x]) z[x] = q, p[x] = col;
      }
    }
  }
}

void rd_draw(void) {
  int ys = gfx_y0, ye = gfx_y1;
  rd_tris = ntris;
  for (int y = ys; y < ye; y++) {
    uint16_t *row = gfx_buf + (y - ys) * SCREEN_W;
    memset(zbuf + (y - ys) * SCREEN_W, 0, SCREEN_W * 2);
    uint8_t c[3];
    if (y < horizon) {
      int t = horizon > 0 ? y * 256 / horizon : 256;
      mix3(c, sky_top, sky_bot, t > 256 ? 256 : t);
      row_fill(row, y, c);
      uint16_t hc = rd_rgb(hill);
      int up = horizon - y;
      if (up <= 26)
        for (int x = 0; x < SCREEN_W; x++)
          if (hills[x] >= up) row[x] = hc;
    } else {
      int t = SCREEN_H > horizon ? (y - horizon) * 256 / (SCREEN_H - horizon) : 256;
      mix3(c, fog, ground, t > 256 ? 256 : t);
      row_fill(row, y, c);
    }
  }
  for (int i = 0; i < ntris; i++)
    if (tris[i].y1 > ys && tris[i].y0 < ye) raster(&tris[i], ys, ye);
}
