#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC optimize("Os")
#endif
/* Little pictures (icons.h). Each is drawn on a 32-unit square scaled to s pixels. */
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
  k = s / 32, ox = x - s / 2, oy = y - s / 2;
  switch (which) {
    case ICON_COIN:
      disc(16, 16, 14, 0xC98A00);
      disc(16, 15, 13, 0xFFC21F);
      disc(16, 15, 9, 0xE8A400);
      disc(16, 14.5f, 8, 0xFFD84D);
      line(13, 9, 11, 14, 2, 0xFFF4C0);
      break;
    case ICON_KEY:
      d_ring(X(11), Y(11), 7 * k, 4.5f * k, C(0x1F6FB8), 255);
      d_ring(X(11), Y(10.5f), 6.5f * k, 3.5f * k, C(0x3FA9F5), 255);
      line(15, 15, 27, 27, 5, 0x1F6FB8);
      line(15, 14.5f, 27, 26.5f, 3.4f, 0x3FA9F5);
      line(22, 22, 25, 19, 3.2f, 0x3FA9F5);
      line(25, 25, 28, 22, 3.2f, 0x3FA9F5);
      break;
    case ICON_BOARD: {
      float b[16] = {6, 26, 2, 22, 22, 4, 28, 3, 30, 6, 28, 12, 10, 28, 7, 28};
      poly(b, 8, 0x173E73);
      float t[16] = {6, 24, 3, 21, 22, 3, 27, 2, 29, 5, 27, 10, 10, 26, 7, 26};
      poly(t, 8, 0x2E7BE0);
      line(7, 22, 26, 5, 2.5f, 0xF2C230);
      break;
    }
    case ICON_JETPACK:
      rrect(5, 6, 15, 24, 4, 0xB9C0C8);
      rrect(17, 6, 27, 24, 4, 0xB9C0C8);
      rect(5, 6, 15, 10, 0xD8302A);
      rect(17, 6, 27, 10, 0xD8302A);
      rect(12, 12, 20, 18, 0x3A3A40);
      {
        float f1[6] = {6, 24, 14, 24, 10, 31}, f2[6] = {18, 24, 26, 24, 22, 31};
        poly(f1, 3, 0xFFA020);
        poly(f2, 3, 0xFFA020);
      }
      break;
    case ICON_SNEAKERS: {
      float sole[8] = {3, 24, 30, 24, 30, 28, 3, 28};
      poly(sole, 4, 0xF4F4F4);
      float up[12] = {4, 24, 5, 12, 13, 10, 17, 17, 29, 19, 30, 24};
      poly(up, 6, 0x3BD16F);
      line(8, 16, 14, 16, 1.6f, 0xF4F4F4);
      line(9, 19, 16, 19, 1.6f, 0xF4F4F4);
      float wing[6] = {4, 12, 0, 6, 9, 11};
      poly(wing, 3, 0xF2C230);
      break;
    }
    case ICON_MAGNET:
      /* the bend of the U, in short pieces */
      for (int i = 0; i < 8; i++) {
        float s0, c0, s1, c1;
        r_sincos(3.14159f * (float)i / 8, &s0, &c0);
        r_sincos(3.14159f * (float)(i + 1) / 8, &s1, &c1);
        line(16 + 6.5f * c0, 15 + 6.5f * s0, 16 + 6.5f * c1, 15 + 6.5f * s1, 7.5f, 0xE0302A);
      }
      rect(6, 4, 13, 16, 0xE0302A);
      rect(19, 4, 26, 16, 0xE0302A);
      rect(6, 2, 13, 7, 0xE8ECF0);
      rect(19, 2, 26, 7, 0xE8ECF0);
      break;
    case ICON_DOUBLE:
      star(16, 17, 15, 0x7A2BBF);
      star(16, 16, 14, 0xA855F7);
      d_label(&font_small, (int)X(16), (int)Y(20), "2x", C(0xFFE040), C(0x4A1580), 1, 0);
      break;
    case ICON_MYSTERY:
      rrect(4, 6, 28, 30, 4, 0x5A2BB0);
      rrect(5, 6, 27, 28, 4, 0x7B3FE4);
      rect(14, 6, 18, 28, 0xF2C230);
      rect(5, 13, 27, 16, 0xF2C230);
      d_label(&font_mid, (int)X(16), (int)Y(26), "?", C(0xFFFFFF), C(0x3A1580), 1, 0);
      break;
    case ICON_HEADSTART: {
      float body[12] = {16, 2, 22, 10, 22, 22, 10, 22, 10, 10, 16, 2};
      poly(body, 6, 0xE8ECF0);
      disc(16, 13, 3, 0x3FA9F5);
      float fl[6] = {10, 16, 4, 26, 10, 22}, fr[6] = {22, 16, 28, 26, 22, 22};
      poly(fl, 3, 0xD8302A);
      poly(fr, 3, 0xD8302A);
      float fire[6] = {11, 22, 21, 22, 16, 31};
      poly(fire, 3, 0xFFA020);
      break;
    }
    case ICON_BOOSTER:
      star(16, 16, 14, 0xF5A300);
      star(16, 15, 12, 0xFFD23F);
      d_label(&font_small, (int)X(16), (int)Y(20), "+5", C(0xFFFFFF), C(0xA05A00), 1, 0);
      break;
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
