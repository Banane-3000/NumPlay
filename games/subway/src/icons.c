#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC optimize("Os")
#endif
/* Little pictures (icons.h). Each is drawn on a 32-unit square scaled to s pixels. */
#include "data.h"
#include "draw.h"
#include "icons.h"

static float ox, oy, k;   /* the picture's top-left and its scale */
#define X(v) (ox + (v) * k)
#define Y(v) (oy + (v) * k)

static void disc(float x, float y, float r, uint32_t c) { d_disc(X(x), Y(y), r * k, C(c), 255); }
static void poly(const float *p, int n, uint32_t c) {
  float q[16];
  for (int i = 0; i < n; i++) q[2 * i] = X(p[2 * i]), q[2 * i + 1] = Y(p[2 * i + 1]);
  d_poly(q, n, C(c), 255);
}
static void rect(float x0, float y0, float x1, float y1, uint32_t c) {
  float p[8] = {x0, y0, x1, y0, x1, y1, x0, y1};
  poly(p, 4, c);
}
static void rrect(float x0, float y0, float x1, float y1, float r, uint32_t c) {
  d_rrect((int)X(x0), (int)Y(y0), (int)((x1 - x0) * k + 0.5f), (int)((y1 - y0) * k + 0.5f), (int)(r * k + 0.5f), C(c), 255);
}
static void line(float x0, float y0, float x1, float y1, float t, uint32_t c) { d_line(X(x0), Y(y0), X(x1), Y(y1), t * k, C(c), 255); }

static void star(float cx, float cy, float r, uint32_t c) {
  /* five points: a triangle each, and the middle */
  static const float u[10][2] = {{0, -1}, {0.225f, -0.309f}, {0.951f, -0.309f}, {0.363f, 0.118f}, {0.588f, 0.809f},
                                 {0, 0.382f}, {-0.588f, 0.809f}, {-0.363f, 0.118f}, {-0.951f, -0.309f}, {-0.225f, -0.309f}};
  float mid[10];
  for (int i = 0; i < 5; i++) {
    const float *a = u[(2 * i + 9) % 10], *b = u[2 * i], *c2 = u[2 * i + 1];
    float t[6] = {cx + a[0] * r, cy + a[1] * r, cx + b[0] * r, cy + b[1] * r, cx + c2[0] * r, cy + c2[1] * r};
    poly(t, 3, c);
    mid[2 * i] = cx + u[2 * i + 1][0] * r, mid[2 * i + 1] = cy + u[2 * i + 1][1] * r;
  }
  poly(mid, 5, c);
}

void icon(int which, float x, float y, float s) {
  /* the game's own pictures (the Subway Surfers Wiki's, data.c), at the size nearest */
  static const Image *const pics[2][ICON_BOOSTER + 1] = {
    {&im_coin, &im_key, &im_board, &im_jetpack, &im_sneakers, &im_magnet, &im_double, &im_mystery, &im_headstart, &im_booster},
    {&im_coin_s, &im_key_s, &im_board_s, &im_jetpack_s, &im_sneakers_s, &im_magnet_s, &im_double_s, &im_mystery_s,
     &im_headstart_s, &im_booster_s},
  };
  if (which <= ICON_BOOSTER) {
    const Image *im = pics[s < 19][which];
    d_image((int)(x - (float)im->w / 2 + 0.5f), (int)(y - (float)im->h / 2 + 0.5f), im);
    return;
  }
  k = s / 32, ox = x - s / 2, oy = y - s / 2;
  switch (which) {
    case ICON_CHECK:
      disc(16, 16, 14, 0x2E9E3E);
      disc(16, 16, 12, 0x3BD16F);
      line(9, 16, 14, 21, 3.5f, 0xFFFFFF);
      line(14, 21, 23, 10, 3.5f, 0xFFFFFF);
      break;
    case ICON_LOCK:
      d_ring(X(16), Y(12), 7 * k, 3.5f * k, C(0x9AA3AD), 255);
      rrect(7, 13, 25, 29, 3, 0xF2C230);
      disc(16, 20, 2.5f, 0x7A5A00);
      rect(15, 20, 17, 25, 0x7A5A00);
      break;
    case ICON_STAR:
      star(16, 17, 15, 0xA05A00);
      star(16, 16, 14, 0xFFD23F);
      break;
  }
}
