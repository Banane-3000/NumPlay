/* PolyTrack's shared pieces: vectors, quaternions and the few functions of maths the game needs, written out so that
 * they give the same bits everywhere (only + - * / and sqrt, which IEEE 754 rounds the same on every machine; the
 * files that use them are compiled with -ffp-contract=off, and every constant is a float). */
#ifndef PT_H
#define PT_H
#include <stdbool.h>
#include <stdint.h>

typedef struct { float x, y, z; } v3;
typedef struct { float x, y, z, w; } qt;
typedef struct { v3 r[3]; } m3;     /* rows */

#define PT_PI 3.14159265f
#define PT_HALF_PI 1.57079633f

static inline v3 V3(float x, float y, float z) { v3 v = {x, y, z}; return v; }
static inline v3 vadd(v3 a, v3 b) { return V3(a.x + b.x, a.y + b.y, a.z + b.z); }
static inline v3 vsub(v3 a, v3 b) { return V3(a.x - b.x, a.y - b.y, a.z - b.z); }
static inline v3 vmul(v3 a, float s) { return V3(a.x * s, a.y * s, a.z * s); }
static inline v3 vneg(v3 a) { return V3(-a.x, -a.y, -a.z); }
static inline float vdot(v3 a, v3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
static inline v3 vcross(v3 a, v3 b) { return V3(a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x); }
static inline float vlen2(v3 a) { return vdot(a, a); }
static inline float pt_sqrtf(float x) { return __builtin_sqrtf(x); }
static inline float vlen(v3 a) { return pt_sqrtf(vdot(a, a)); }
static inline v3 vnorm(v3 a) {
  float l = vlen(a);
  return l > 0.0f ? vmul(a, 1.0f / l) : a;
}
static inline float pt_absf(float x) { return x < 0.0f ? -x : x; }
static inline float pt_minf(float a, float b) { return a < b ? a : b; }
static inline float pt_maxf(float a, float b) { return a > b ? a : b; }
static inline float pt_clampf(float v, float lo, float hi) { return v < lo ? lo : v > hi ? hi : v; }

static inline v3 mrow(const m3 *m, v3 v) { return V3(vdot(m->r[0], v), vdot(m->r[1], v), vdot(m->r[2], v)); }
/* m^T v */
static inline v3 mcol(const m3 *m, v3 v) {
  return V3(m->r[0].x * v.x + m->r[1].x * v.y + m->r[2].x * v.z, m->r[0].y * v.x + m->r[1].y * v.y + m->r[2].y * v.z,
            m->r[0].z * v.x + m->r[1].z * v.y + m->r[2].z * v.z);
}
static inline v3 mcolumn(const m3 *m, int i) {
  return i == 0 ? V3(m->r[0].x, m->r[1].x, m->r[2].x) : i == 1 ? V3(m->r[0].y, m->r[1].y, m->r[2].y)
                                                                : V3(m->r[0].z, m->r[1].z, m->r[2].z);
}

/* Bullet's btMatrix3x3::setRotation */
static inline void qmat(qt q, m3 *m) {
  float d = q.x * q.x + q.y * q.y + q.z * q.z + q.w * q.w;
  float s = 2.0f / d;
  float xs = q.x * s, ys = q.y * s, zs = q.z * s;
  float wx = q.w * xs, wy = q.w * ys, wz = q.w * zs;
  float xx = q.x * xs, xy = q.x * ys, xz = q.x * zs;
  float yy = q.y * ys, yz = q.y * zs, zz = q.z * zs;
  m->r[0] = V3(1.0f - (yy + zz), xy - wz, xz + wy);
  m->r[1] = V3(xy + wz, 1.0f - (xx + zz), yz - wx);
  m->r[2] = V3(xz - wy, yz + wx, 1.0f - (xx + yy));
}
static inline qt qmul(qt a, qt b) {
  qt r = {a.w * b.x + a.x * b.w + a.y * b.z - a.z * b.y, a.w * b.y + a.y * b.w + a.z * b.x - a.x * b.z,
          a.w * b.z + a.z * b.w + a.x * b.y - a.y * b.x, a.w * b.w - a.x * b.x - a.y * b.y - a.z * b.z};
  return r;
}
static inline qt qnorm(qt q) {
  float l = pt_sqrtf(q.x * q.x + q.y * q.y + q.z * q.z + q.w * q.w);
  qt r = {q.x / l, q.y / l, q.z / l, q.w / l};
  return r;
}
v3 qrot(qt q, v3 v);
qt mat_quat(const m3 *m);          /* Bullet's btMatrix3x3::getRotation */

float pt_sinf(float x);
float pt_cosf(float x);
float pt_atan2f(float y, float x);
float pt_powf(float x, float y);   /* x > 0 */
float pt_expf(float x);

uint32_t pt_hash(uint32_t h, const void *data, uint32_t n);   /* FNV-1a */
#endif
