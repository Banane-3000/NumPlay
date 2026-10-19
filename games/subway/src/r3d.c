/* The 3D renderer (r3d.h): polygons in a list, drawn band by band with a depth buffer. */
#include "r3d.h"

#define NEAR 0.35f
#define FAR 190.0f
#define ZK 22000.0f      /* depths are ZK / z: 62857 at the near plane, larger is nearer */
#define MAXP 1500
#define MAXV 5200
#define MAXN 10          /* corners of a polygon cut at the near plane */

typedef struct {
  float a, b, c;         /* its depth at (sx, sy): a sx + b sy + c */
  uint16_t v0, color;
  uint8_t n, alpha;
  int16_t y0, y1;        /* the rows it covers */
} Poly;

uint16_t *r_col;
int r_y0, r_y1;
static uint16_t zbuf[SCREEN_W * BAND_H] __attribute__((aligned(8)));
Camera cam;
float r_haze0 = 60, r_haze1 = 150;
uint32_t r_haze_rgb = 0xD8F0FF;

static Poly polys[MAXP];
static uint16_t order[MAXP];   /* the see-through ones go last */
static float vx[MAXV], vy[MAXV];
static int npolys, nverts, nalpha;

void r_set_band_memory(uint16_t *band) { r_col = band; }

void r_camera(float x, float y, float z, float pitch, float f, float cx, float cy) {
  cam.x = x, cam.y = y, cam.z = z, cam.pitch = pitch, cam.f = f, cam.cx = cx, cam.cy = cy;
  r_sincos(pitch, &cam.sp, &cam.cp);
}

bool r_project(V3 p, float *sx, float *sy) {
  V3 v = r_view(p);
  if (v.z < NEAR) return false;
  float k = cam.f / v.z;
  *sx = cam.cx + v.x * k, *sy = cam.cy - v.y * k;
  return true;
}

void r_sincos(float a, float *s, float *c) {
  a -= (float)(int)(a / 6.2831853f) * 6.2831853f;
  if (a > 3.14159265f) a -= 6.2831853f;
  if (a < -3.14159265f) a += 6.2831853f;
  float a2 = a * a;
  *s = a * (1 - a2 / 6 * (1 - a2 / 20 * (1 - a2 / 42 * (1 - a2 / 72 * (1 - a2 / 110)))));
  *c = 1 - a2 / 2 * (1 - a2 / 12 * (1 - a2 / 30 * (1 - a2 / 56 * (1 - a2 / 90))));
}

void r_begin(void) { npolys = nverts = nalpha = 0; }
int r_count(void) { return npolys; }

uint32_t r_mix888(uint32_t a, uint32_t b, int t) {
  uint32_t rb = (a & 0xFF00FF) * (uint32_t)(256 - t) + (b & 0xFF00FF) * (uint32_t)t;
  uint32_t g = (a & 0x00FF00) * (uint32_t)(256 - t) + (b & 0x00FF00) * (uint32_t)t;
  return ((rb >> 8) & 0xFF00FF) | ((g >> 8) & 0x00FF00);
}

/* a bright cartoon light: tops full, the left a little darker, the right more, fronts in between */
uint32_t r_shade(uint32_t rgb, V3 n) {
  float s = 0.74f + 0.26f * n.y + 0.08f * -n.x - 0.13f * n.z;
  if (s > 1) s = 1;
  if (s < 0.3f) s = 0.3f;
  int k = (int)(s * 256);
  uint32_t r = ((rgb >> 16) & 255) * (uint32_t)k >> 8, g = ((rgb >> 8) & 255) * (uint32_t)k >> 8, b = (rgb & 255) * (uint32_t)k >> 8;
  return r << 16 | g << 8 | b;
}

void r_poly(const V3 *w, int n, uint32_t rgb, int flags, int alpha) {
  if (npolys >= MAXP || nverts + MAXN > MAXV || n < 3 || n > MAXN - 2) return;
  V3 a[MAXN], b[MAXN];
  int behind = 0, far = 0;
  for (int i = 0; i < n; i++) {
    a[i] = r_view(w[i]);
    behind += a[i].z < NEAR;
    far += a[i].z > FAR;
  }
  if (behind == n || far == n) return;
  /* cut at the near plane */
  const V3 *p = a;
  if (behind) {
    int m = 0;
    for (int i = 0, j = n - 1; i < n; j = i++) {
      bool in_i = a[i].z >= NEAR, in_j = a[j].z >= NEAR;
      if (in_i != in_j) {
        float t = (NEAR - a[j].z) / (a[i].z - a[j].z);
        b[m++] = (V3){a[j].x + t * (a[i].x - a[j].x), a[j].y + t * (a[i].y - a[j].y), NEAR};
      }
      if (in_i) b[m++] = a[i];
    }
    if (m < 3) return;
    p = b, n = m;
  }
  /* the plane, by Newell's method (good for any convex polygon); depth = ZK (n . (xs, ys, 1)) / d */
  V3 nm = {0, 0, 0};
  for (int i = 0, j = n - 1; i < n; j = i++) {
    nm.x += (p[j].y - p[i].y) * (p[j].z + p[i].z);
    nm.y += (p[j].z - p[i].z) * (p[j].x + p[i].x);
    nm.z += (p[j].x - p[i].x) * (p[j].y + p[i].y);
  }
  float d = nm.x * p[0].x + nm.y * p[0].y + nm.z * p[0].z;
  if (d > -1e-6f && d < 1e-6f) return;   /* seen edge-on */
  float *px = vx + nverts, *py = vy + nverts;
  float x0 = 1e9f, x1 = -1e9f, y0 = 1e9f, y1 = -1e9f, zsum = 0;
  for (int i = 0; i < n; i++) {
    float k = cam.f / p[i].z;
    px[i] = cam.cx + p[i].x * k, py[i] = cam.cy - p[i].y * k;
    if (px[i] < x0) x0 = px[i];
    if (px[i] > x1) x1 = px[i];
    if (py[i] < y0) y0 = py[i];
    if (py[i] > y1) y1 = py[i];
    zsum += p[i].z;
  }
  if (x1 < 0 || x0 >= SCREEN_W || y1 < 0 || y0 >= SCREEN_H) return;
  float area = 0;
  for (int i = 0, j = n - 1; i < n; j = i++) area += px[j] * py[i] - px[i] * py[j];
  if (area < 0.01f && !(flags & P_TWOSIDED && area < -0.01f)) return;
  Poly *q = &polys[npolys];
  float s = ZK / d, f = cam.f;
  q->a = s * nm.x / f;
  q->b = -s * nm.y / f;
  q->c = s * (nm.z - nm.x * cam.cx / f + nm.y * cam.cy / f);
  if (flags & P_DECAL) q->c += 3;
  if (flags & P_DECAL2) q->c += 6;
  if (flags & P_DECAL3) q->c += 9;
  if (!(flags & P_NOHAZE)) {
    float zm = zsum / (float)n;
    if (zm > r_haze0) {
      float h = (zm - r_haze0) / (r_haze1 - r_haze0);
      rgb = r_mix888(rgb, r_haze_rgb, h >= 1 ? 240 : (int)(h * 240));
    }
  }
  q->color = r_565(rgb);
  q->alpha = flags & P_ALPHA ? (uint8_t)(alpha > 255 ? 255 : alpha < 0 ? 0 : alpha) : 255;
  q->v0 = (uint16_t)nverts, q->n = (uint8_t)n;
  q->y0 = (int16_t)(y0 < 0 ? 0 : (int)(y0 + 0.5f));
  q->y1 = (int16_t)(y1 >= SCREEN_H ? SCREEN_H : (int)(y1 + 0.5f));
  if (q->y0 >= q->y1) return;
  nverts += n;
  if (flags & P_ALPHA) nalpha++;
  order[npolys] = (uint16_t)npolys;
  npolys++;
}

static V3 add3(V3 a, V3 b, V3 c, V3 d) { return (V3){a.x + b.x + c.x + d.x, a.y + b.y + c.y + d.y, a.z + b.z + c.z + d.z}; }
static V3 neg(V3 a) { return (V3){-a.x, -a.y, -a.z}; }
static V3 unit(V3 a) {
  float l = a.x * a.x + a.y * a.y + a.z * a.z;
  if (l <= 0) return a;
  float r = 1.0f / __builtin_sqrtf(l);
  return (V3){a.x * r, a.y * r, a.z * r};
}

void r_box(V3 c, V3 ax, V3 ay, V3 az, const uint32_t *rgb6, int skip) {
  V3 nx = neg(ax), ny = neg(ay), nz = neg(az);
  /* each face seen from outside, clockwise: its four corners as signs along x, y, z */
  static const int8_t faces[6][4][3] = {
    {{-1, 1, 1}, {1, 1, 1}, {1, 1, -1}, {-1, 1, -1}},        /* top */
    {{1, -1, 1}, {-1, -1, 1}, {-1, -1, -1}, {1, -1, -1}},    /* bottom */
    {{-1, 1, 1}, {-1, 1, -1}, {-1, -1, -1}, {-1, -1, 1}},    /* -x */
    {{1, 1, -1}, {1, 1, 1}, {1, -1, 1}, {1, -1, -1}},        /* +x */
    {{-1, 1, -1}, {1, 1, -1}, {1, -1, -1}, {-1, -1, -1}},    /* front, -z */
    {{1, 1, 1}, {-1, 1, 1}, {-1, -1, 1}, {1, -1, 1}},        /* back, +z */
  };
  const V3 normals[6] = {ay, ny, nx, ax, nz, az};
  for (int f = 0; f < 6; f++) {
    if (skip & 1 << f) continue;
    /* only the faces turned towards the camera */
    V3 nf = normals[f], fc = (V3){c.x + nf.x, c.y + nf.y, c.z + nf.z};
    if ((fc.x - cam.x) * nf.x + (fc.y - cam.y) * nf.y + (fc.z - cam.z) * nf.z >= 0) continue;
    V3 v[4];
    for (int k = 0; k < 4; k++) {
      const int8_t *s = faces[f][k];
      V3 x = s[0] > 0 ? ax : nx, y = s[1] > 0 ? ay : ny, z = s[2] > 0 ? az : nz;
      v[k] = add3(c, x, y, z);
    }
    r_poly(v, 4, r_shade(rgb6[f], unit(nf)), 0, 0);
  }
}

void r_cube(float x0, float y0, float z0, float x1, float y1, float z1, uint32_t rgb, int skip) {
  float hx = (x1 - x0) / 2, hy = (y1 - y0) / 2, hz = (z1 - z0) / 2;
  uint32_t c[6] = {rgb, rgb, rgb, rgb, rgb, rgb};
  r_box((V3){x0 + hx, y0 + hy, z0 + hz}, (V3){hx, 0, 0}, (V3){0, hy, 0}, (V3){0, 0, hz}, c, skip);
}

/* ---------------------------------------------------------------- drawing a band */
static void raster(const Poly *q) {
  int ya = q->y0 > r_y0 ? q->y0 : r_y0, yb = q->y1 < r_y1 ? q->y1 : r_y1;
  if (ya >= yb) return;
  const float *px = vx + q->v0, *py = vy + q->v0;
  int n = q->n;
  /* the edges crossing the band: where they start and their slope */
  float ey0[MAXN], ey1[MAXN], ex[MAXN], es[MAXN];
  int ne = 0;
  for (int i = 0, j = n - 1; i < n; j = i++) {
    float a = py[j], b = py[i];
    if (a == b) continue;
    float xa = px[j], xb = px[i];
    if (a > b) {
      float t = a; a = b, b = t;
      t = xa; xa = xb, xb = t;
    }
    if (b <= ya || a >= yb) continue;
    ey0[ne] = a, ey1[ne] = b, es[ne] = (xb - xa) / (b - a), ex[ne] = xa - a * es[ne];
    ne++;
  }
  if (ne < 2) return;
  uint16_t col = q->color;
  int alpha = q->alpha >> 3;
  int32_t da = (int32_t)(q->a * 65536.0f);
  for (int y = ya; y < yb; y++) {
    float yc = (float)y + 0.5f, xl = 1e9f, xr = -1e9f;
    for (int e = 0; e < ne; e++) {
      if (yc < ey0[e] || yc >= ey1[e]) continue;
      float x = ex[e] + yc * es[e];
      if (x < xl) xl = x;
      if (x > xr) xr = x;
    }
    if (xl > xr) continue;
    int x0 = xl <= -1 ? 0 : (int)(xl + 0.5f), x1 = xr >= SCREEN_W ? SCREEN_W : (int)(xr + 0.5f);
    if (x0 < 0) x0 = 0;
    if (x0 >= x1) continue;
    float zf = q->a * ((float)x0 + 0.5f) + q->b * yc + q->c;
    if (zf < 0) zf = 0;
    if (zf > 65535) zf = 65535;
    uint32_t z = (uint32_t)(zf * 65536.0f);
    uint16_t *zb = zbuf + (y - r_y0) * SCREEN_W, *cb = r_col + (y - r_y0) * SCREEN_W;
    if (alpha >= 31) {
      for (int x = x0; x < x1; x++, z += (uint32_t)da) {
        uint16_t zz = (uint16_t)(z >> 16);
        if (zz > zb[x]) zb[x] = zz, cb[x] = col;
      }
    } else {
      for (int x = x0; x < x1; x++, z += (uint32_t)da)
        if ((uint16_t)(z >> 16) > zb[x]) cb[x] = r_mix565(cb[x], col, alpha);
    }
  }
}

void r_render(r_band_fn bg, r_band_fn overlay) {
  /* the see-through polygons after the others, in the order they came */
  if (nalpha) {
    int k = 0;
    for (int i = 0; i < npolys; i++)
      if (polys[i].alpha == 255) order[k++] = (uint16_t)i;
    for (int i = 0; i < npolys; i++)
      if (polys[i].alpha != 255) order[k++] = (uint16_t)i;
  }
  for (int band = 0; band < NBANDS; band++) {
    r_y0 = band * BAND_H, r_y1 = r_y0 + BAND_H;
    /* (cleared a word at a time: the C library's is a byte at a time) */
    for (uint32_t *p = (uint32_t *)(void *)zbuf, *e = p + sizeof zbuf / 4; p < e; p += 4) p[0] = p[1] = p[2] = p[3] = 0;
    if (bg) bg();
    else
      for (int i = 0; i < SCREEN_W * BAND_H; i++) r_col[i] = 0;
    for (int i = 0; i < npolys; i++) {
      const Poly *q = &polys[order[i]];
      if (q->y1 > r_y0 && q->y0 < r_y1) raster(q);
    }
    if (overlay) overlay();
    plat_push(0, r_y0, SCREEN_W, BAND_H, r_col);
  }
}
