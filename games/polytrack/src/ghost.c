/* The ghost (ghost.h). A sample: per axis a byte, the change from the guess (twice the last position less the one
 * before), or 0x80 and the position itself (two bytes); then the turn in three bytes. */
#include <string.h>
#include "ghost.h"

static int32_t qpos(float v) { return (int32_t)(v < 0.0f ? v * 16.0f - 0.5f : v * 16.0f + 0.5f); }

void ghost_rec_start(ghost_rec *r) {
  r->len = 0, r->full = 0, r->n = 0;
  memset(r->q, 0, sizeof r->q);
}

static void put(ghost_rec *r, uint8_t b) {
  if (r->len >= PT_GHOST_MAX) {
    r->full = 1;
    return;
  }
  r->buf[r->len++] = b;
}

static int8_t qpart(float v) {
  int i = (int)(v * 89.8026f + (v < 0.0f ? -0.5f : 0.5f));   /* 1/sqrt(2) -> 63.5 */
  return (int8_t)(i < -63 ? -63 : i > 63 ? 63 : i);
}

void ghost_rec_add(ghost_rec *r, v3 pos, qt rot) {
  if (r->full) return;
  int32_t q[3] = {qpos(pos.x), qpos(pos.y), qpos(pos.z)};
  for (int k = 0; k < 3; k++) {
    int32_t guess = r->n == 0 ? 0 : r->n == 1 ? r->q[1][k] : 2 * r->q[1][k] - r->q[0][k];
    int32_t d = q[k] - guess;
    if (r->n > 0 && d >= -127 && d <= 127) {
      put(r, (uint8_t)(int8_t)d);
    } else {
      int32_t v = q[k] < -32767 ? -32767 : q[k] > 32767 ? 32767 : q[k];
      q[k] = v;
      put(r, 0x80);
      put(r, (uint8_t)(v & 255));
      put(r, (uint8_t)((v >> 8) & 255));
    }
  }
  for (int k = 0; k < 3; k++) r->q[0][k] = r->q[1][k], r->q[1][k] = q[k];
  /* the turn: the biggest part left out (made positive), the other three in 7 bits */
  float c[4] = {rot.x, rot.y, rot.z, rot.w};
  int big = 0;
  for (int k = 1; k < 4; k++)
    if (pt_absf(c[k]) > pt_absf(c[big])) big = k;
  float s = c[big] < 0.0f ? -1.0f : 1.0f;
  uint32_t bits = (uint32_t)big;
  int sh = 2;
  for (int k = 0; k < 4; k++) {
    if (k == big) continue;
    bits |= (uint32_t)(qpart(c[k] * s) + 64) << sh;
    sh += 7;
  }
  put(r, (uint8_t)bits), put(r, (uint8_t)(bits >> 8)), put(r, (uint8_t)(bits >> 16));
  r->n++;
}

void ghost_play_start(ghost_play *g, const uint8_t *data, uint16_t len, uint32_t ms) {
  memset(g, 0, sizeof *g);
  g->data = data, g->len = len, g->ms = ms;
  g->on = data && len;
  g->first = -1000;
}

static bool decode(ghost_play *g, v3 *pos, qt *rot) {
  int32_t q[3];
  for (int k = 0; k < 3; k++) {
    if (g->at >= g->len) return false;
    uint8_t b = g->data[g->at++];
    if (b == 0x80) {
      if (g->at + 2 > g->len) return false;
      q[k] = (int16_t)(g->data[g->at] | g->data[g->at + 1] << 8);
      g->at += 2;
    } else {
      int32_t guess = g->n == 1 ? g->q[1][k] : 2 * g->q[1][k] - g->q[0][k];
      q[k] = guess + (int8_t)b;
    }
  }
  if (g->at + 3 > g->len) return false;
  uint32_t bits = (uint32_t)(g->data[g->at] | g->data[g->at + 1] << 8 | g->data[g->at + 2] << 16);
  g->at += 3;
  for (int k = 0; k < 3; k++) g->q[0][k] = g->q[1][k], g->q[1][k] = q[k];
  g->n++;
  *pos = V3((float)q[0] / 16.0f, (float)q[1] / 16.0f, (float)q[2] / 16.0f);
  int big = (int)(bits & 3);
  float c[4], sum = 0.0f;
  int sh = 2;
  for (int k = 0; k < 4; k++) {
    if (k == big) continue;
    c[k] = (float)((int)((bits >> sh) & 127) - 64) / 89.8026f;
    sum += c[k] * c[k];
    sh += 7;
  }
  c[big] = pt_sqrtf(pt_maxf(0.0f, 1.0f - sum));
  qt r = {c[0], c[1], c[2], c[3]};
  *rot = qnorm(r);
  return true;
}

bool ghost_at(ghost_play *g, uint32_t f, v3 *pos, qt *rot) {
  if (!g->on || g->ended) return false;
  int32_t i = (int32_t)(f / GHOST_STEP);
  if (i < g->first) {
    ghost_play_start(g, g->data, g->len, g->ms);
  }
  /* the window: samples i-1 .. i+2 */
  while (g->first < i) {
    if (g->first < -1) {
      /* (re)start: decode the first three samples */
      v3 p;
      qt q;
      if (!decode(g, &p, &q)) return g->ended = true, false;
      g->pos[0] = g->pos[1] = p, g->rot[0] = g->rot[1] = q;
      for (int k = 2; k < 4; k++) {
        if (!decode(g, &g->pos[k], &g->rot[k])) g->pos[k] = g->pos[k - 1], g->rot[k] = g->rot[k - 1];
      }
      g->first = 0;
      continue;
    }
    for (int k = 0; k < 3; k++) g->pos[k] = g->pos[k + 1], g->rot[k] = g->rot[k + 1];
    if (!decode(g, &g->pos[3], &g->rot[3])) {
      if (g->n < (uint32_t)i + 2) return g->ended = true, false;
      g->pos[3] = g->pos[2], g->rot[3] = g->rot[2];
    }
    g->first++;
  }
  if ((uint32_t)i + 1 >= g->n) return false;
  float t = (float)(f % GHOST_STEP) / (float)GHOST_STEP;
  /* Catmull-Rom between samples i and i+1 */
  v3 p0 = g->pos[0], p1 = g->pos[1], p2 = g->pos[2], p3 = g->pos[3];
  float t2 = t * t, t3 = t2 * t;
  float a = -0.5f * t3 + t2 - 0.5f * t, b = 1.5f * t3 - 2.5f * t2 + 1.0f, c = -1.5f * t3 + 2.0f * t2 + 0.5f * t,
        d = 0.5f * t3 - 0.5f * t2;
  /* (a teleport, a respawn: no curve across it) */
  if (vlen2(vsub(p2, p1)) > 400.0f) {
    *pos = t < 0.5f ? p1 : p2;
  } else {
    if (vlen2(vsub(p1, p0)) > 400.0f) p0 = p1;
    if (vlen2(vsub(p3, p2)) > 400.0f) p3 = p2;
    *pos = vadd(vadd(vmul(p0, a), vmul(p1, b)), vadd(vmul(p2, c), vmul(p3, d)));
  }
  qt q0 = g->rot[1], q1 = g->rot[2];
  if (q0.x * q1.x + q0.y * q1.y + q0.z * q1.z + q0.w * q1.w < 0.0f) q1.x = -q1.x, q1.y = -q1.y, q1.z = -q1.z, q1.w = -q1.w;
  qt q = {q0.x + (q1.x - q0.x) * t, q0.y + (q1.y - q0.y) * t, q0.z + (q1.z - q0.z) * t, q0.w + (q1.w - q0.w) * t};
  *rot = qnorm(q);
  return true;
}
