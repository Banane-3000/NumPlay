/* The maths of pt.h: polynomial sine, cosine, arctangent and powers (the coefficients are Cephes' single precision
 * ones), using only operations IEEE 754 rounds the same everywhere, so a race comes out the same on the calculator
 * and on every computer. */
#include <string.h>
#include "pt.h"

v3 qrot(qt q, v3 v) {
  m3 m;
  qmat(q, &m);
  return mrow(&m, v);
}

qt mat_quat(const m3 *m) {
  float tr = m->r[0].x + m->r[1].y + m->r[2].z;
  float t[4];
  if (tr > 0.0f) {
    float s = pt_sqrtf(tr + 1.0f);
    t[3] = s * 0.5f;
    s = 0.5f / s;
    t[0] = (m->r[2].y - m->r[1].z) * s;
    t[1] = (m->r[0].z - m->r[2].x) * s;
    t[2] = (m->r[1].x - m->r[0].y) * s;
  } else {
    float d[3] = {m->r[0].x, m->r[1].y, m->r[2].z};
    float e[3][3] = {{m->r[0].x, m->r[0].y, m->r[0].z}, {m->r[1].x, m->r[1].y, m->r[1].z},
                     {m->r[2].x, m->r[2].y, m->r[2].z}};
    int i = d[0] < d[1] ? (d[1] < d[2] ? 2 : 1) : (d[0] < d[2] ? 2 : 0);
    int j = (i + 1) % 3, k = (i + 2) % 3;
    float s = pt_sqrtf(e[i][i] - e[j][j] - e[k][k] + 1.0f);
    t[i] = s * 0.5f;
    s = 0.5f / s;
    t[3] = (e[k][j] - e[j][k]) * s;
    t[j] = (e[j][i] + e[i][j]) * s;
    t[k] = (e[k][i] + e[i][k]) * s;
  }
  qt q = {t[0], t[1], t[2], t[3]};
  return q;
}

/* x reduced by multiples of pi/2: x = k pi/2 + r, |r| <= pi/4 (Cody-Waite, three parts of pi/2) */
static float reduce(float x, int *quadrant) {
  float k = x * 0.636619772f;
  int n = (int)(k < 0.0f ? k - 0.5f : k + 0.5f);
  float fn = (float)n;
  float r = ((x - fn * 1.5703125f) - fn * 4.83751297e-4f) - fn * 7.54978995e-8f;
  *quadrant = n & 3;
  return r;
}
static float sin_poly(float r) {
  float z = r * r;
  return ((-1.9515295891e-4f * z + 8.3321608736e-3f) * z - 1.6666654611e-1f) * z * r + r;
}
static float cos_poly(float r) {
  float z = r * r;
  return ((2.443315711809948e-5f * z - 1.388731625493765e-3f) * z + 4.166664568298827e-2f) * z * z - 0.5f * z + 1.0f;
}
float pt_sinf(float x) {
  int q;
  float r = reduce(x, &q);
  switch (q) {
    case 0: return sin_poly(r);
    case 1: return cos_poly(r);
    case 2: return -sin_poly(r);
    default: return -cos_poly(r);
  }
}
float pt_cosf(float x) {
  int q;
  float r = reduce(x, &q);
  switch (q) {
    case 0: return cos_poly(r);
    case 1: return -sin_poly(r);
    case 2: return -cos_poly(r);
    default: return sin_poly(r);
  }
}

static float atan_poly(float t) {
  /* t >= 0 */
  float y = 0.0f;
  if (t > 2.414213562f) {
    y = PT_HALF_PI;
    t = -1.0f / t;
  } else if (t > 0.414213562f) {
    y = 0.785398163f;
    t = (t - 1.0f) / (t + 1.0f);
  }
  float z = t * t;
  return y + (((8.05374449538e-2f * z - 1.38776856032e-1f) * z + 1.99777106478e-1f) * z - 3.33329491539e-1f) * z * t + t;
}
float pt_atan2f(float y, float x) {
  if (x == 0.0f) {
    if (y > 0.0f) return PT_HALF_PI;
    if (y < 0.0f) return -PT_HALF_PI;
    return 0.0f;
  }
  float a = atan_poly(pt_absf(y / x));
  if (x < 0.0f) a = PT_PI - a;
  return y < 0.0f ? -a : a;
}

static uint32_t fbits(float f) {
  uint32_t u;
  memcpy(&u, &f, 4);
  return u;
}
static float bitsf(uint32_t u) {
  float f;
  memcpy(&f, &u, 4);
  return f;
}
/* log2 for x > 0 (normal numbers) */
static float log2_(float x) {
  uint32_t u = fbits(x);
  int e = (int)((u >> 23) & 255) - 127;
  float m = bitsf((u & 0x007FFFFFu) | 0x3F800000u);   /* 1 <= m < 2 */
  if (m > 1.41421356f) m *= 0.5f, e++;
  float t = m - 1.0f;                                  /* -0.29 .. 0.41 */
  float z = t * t;
  float y = (((((((7.0376836292e-2f * t - 1.1514610310e-1f) * t + 1.1676998740e-1f) * t - 1.2420140846e-1f) * t +
                1.4249322787e-1f) * t - 1.6668057665e-1f) * t + 2.0000714765e-1f) * t - 2.4999993993e-1f) * t +
            3.3333331174e-1f;
  float ln = t - 0.5f * z + y * z * t;
  return (float)e + ln * 1.44269504f;
}
/* 2^x */
static float exp2_(float x) {
  if (x > 126.0f) x = 126.0f;
  if (x < -126.0f) return 0.0f;
  float fl = (float)(int)x;
  if (fl > x) fl -= 1.0f;
  float f = (x - fl) * 0.693147181f;                  /* 0 <= f < ln 2 */
  float p = ((((1.9875691500e-4f * f + 1.3981999507e-3f) * f + 8.3334519073e-3f) * f + 4.1665795894e-2f) * f +
             1.6666665459e-1f) * f + 5.0000001201e-1f;
  float ef = 1.0f + f + p * f * f;
  return ef * bitsf((uint32_t)((int)fl + 127) << 23);
}
float pt_powf(float x, float y) { return exp2_(y * log2_(x)); }
float pt_expf(float x) { return exp2_(x * 1.44269504f); }

uint32_t pt_hash(uint32_t h, const void *data, uint32_t n) {
  const uint8_t *p = data;
  for (uint32_t i = 0; i < n; i++) h = (h ^ p[i]) * 16777619u;
  return h;
}
