/* The people, the dog, the power-ups and the boards (actors.h): boxes on a skeleton.
 *
 * A body faces +z (the camera mostly sees its back). Its parts hang from joints: the legs from the hips,
 * the shins from the knees, the arms from the shoulders; the chest leans forward from the waist and
 * carries the arms and the head. Each part is a box turned by the rotations above it. */
#include "actors.h"
#include "scene.h"

typedef struct {
  float m[9];
} M3;

static M3 mul(M3 a, M3 b) {
  M3 r;
  for (int i = 0; i < 3; i++)
    for (int j = 0; j < 3; j++) r.m[3 * i + j] = a.m[3 * i] * b.m[j] + a.m[3 * i + 1] * b.m[3 + j] + a.m[3 * i + 2] * b.m[6 + j];
  return r;
}
static V3 ap(M3 a, V3 v) {
  return (V3){a.m[0] * v.x + a.m[1] * v.y + a.m[2] * v.z, a.m[3] * v.x + a.m[4] * v.y + a.m[5] * v.z,
              a.m[6] * v.x + a.m[7] * v.y + a.m[8] * v.z};
}
static M3 rx(float a) {
  float s, c;
  r_sincos(a, &s, &c);
  return (M3){{1, 0, 0, 0, c, -s, 0, s, c}};
}
static M3 ry(float a) {
  float s, c;
  r_sincos(a, &s, &c);
  return (M3){{c, 0, s, 0, 1, 0, -s, 0, c}};
}
static M3 rz(float a) {
  float s, c;
  r_sincos(a, &s, &c);
  return (M3){{c, -s, 0, s, c, 0, 0, 0, 1}};
}
static const M3 ident = {{1, 0, 0, 0, 1, 0, 0, 0, 1}};
static V3 vadd(V3 a, V3 b) { return (V3){a.x + b.x, a.y + b.y, a.z + b.z}; }
static V3 vsub(V3 a, V3 b) { return (V3){a.x - b.x, a.y - b.y, a.z - b.z}; }
static V3 vscale(V3 a, float k) { return (V3){a.x * k, a.y * k, a.z * k}; }

/* the body being drawn: where its feet are, its turn, the point it turns about, its size, its light */
static V3 at, pivot;
static M3 root;
static float size, light;

static uint32_t shade(uint32_t c) {
  if (light >= 1) return c;
  return r_mix888(r_mix888(c, 0, (int)((1 - light) * 256)), 0x10203A, 24);
}
/* a part: the frame R at the joint j (body coordinates), its box centred at off in that frame */
static void part(M3 R, V3 j, V3 off, float hx, float hy, float hz, uint32_t col) {
  M3 W = mul(root, R);
  V3 c = vadd(j, ap(R, off));
  c = vadd(at, vadd(ap(root, vsub(vscale(c, size), pivot)), pivot));
  uint32_t k = shade(col), k6[6] = {k, k, k, k, k, k};
  r_box(c, ap(W, (V3){hx * size, 0, 0}), ap(W, (V3){0, hy * size, 0}), ap(W, (V3){0, 0, hz * size}), k6, 0);
}

/* ---------------------------------------------------------------- poses */
static float fsin(float a) {
  float s, c;
  r_sincos(a, &s, &c);
  return s;
}
static float fcos(float a) { return fsin(a + 1.5707963f); }
static float pos(float v) { return v > 0 ? v : 0; }

static void clear(Pose *p) {
  for (int i = 0; i < 2; i++) p->hip[i] = p->knee[i] = p->arm[i] = p->elbow[i] = p->out[i] = 0;
  p->lean = p->bob = p->spin = p->tilt = p->yaw = 0;
  p->squash = 1;
}

void pose_run(Pose *p, float ph) {
  clear(p);
  float s = fsin(ph), c = fcos(ph);
  p->hip[0] = 0.85f * s, p->hip[1] = -0.85f * s;
  p->knee[0] = 0.2f + 1.3f * pos(c), p->knee[1] = 0.2f + 1.3f * pos(-c);
  p->arm[0] = -0.75f * s, p->arm[1] = 0.75f * s;
  p->elbow[0] = p->elbow[1] = 1.35f;
  p->out[0] = p->out[1] = 0.12f;
  p->lean = 0.24f;
  p->bob = 0.07f * (c < 0 ? -c : c);
}

void pose_stand(Pose *p, float t) {
  clear(p);
  p->arm[0] = p->arm[1] = 0.08f;
  p->elbow[0] = p->elbow[1] = 0.25f;
  p->out[0] = p->out[1] = 0.1f;
  p->bob = 0.012f * fsin(t * 2.4f);
  p->knee[0] = p->knee[1] = 0.05f;
}

void pose_jump(Pose *p, float t) {
  clear(p);
  p->hip[0] = 1.0f, p->knee[0] = 1.5f;
  p->hip[1] = -0.35f, p->knee[1] = 1.0f;
  p->arm[0] = -0.8f, p->arm[1] = 1.4f + 0.2f * fsin(t * 6);
  p->elbow[0] = 0.9f, p->elbow[1] = 0.6f;
  p->out[0] = p->out[1] = 0.35f;
  p->lean = 0.12f;
}

void pose_roll(Pose *p, float t) {
  clear(p);
  p->hip[0] = p->hip[1] = 1.7f;
  p->knee[0] = p->knee[1] = 2.4f;
  p->arm[0] = p->arm[1] = 1.3f;
  p->elbow[0] = p->elbow[1] = 1.7f;
  p->lean = 0.7f;
  p->spin = 6.2831853f * t;
  p->squash = 0.78f;
  p->bob = -0.15f;
}

void pose_hover(Pose *p, float t) {
  clear(p);
  p->hip[0] = 0.4f, p->hip[1] = -0.35f;
  p->knee[0] = 0.55f, p->knee[1] = 0.6f;
  p->arm[0] = 0.3f, p->arm[1] = -0.2f;
  p->elbow[0] = p->elbow[1] = 0.5f;
  p->out[0] = p->out[1] = 0.9f + 0.1f * fsin(t * 3);
  p->lean = 0.18f;
  p->yaw = 0.3f;
  p->bob = 0.03f * fsin(t * 4);
}

void pose_fly(Pose *p, float t) {
  clear(p);
  p->hip[0] = 0.2f, p->hip[1] = -0.3f;
  p->knee[0] = 0.7f, p->knee[1] = 1.0f;
  p->arm[0] = p->arm[1] = -0.5f;
  p->elbow[0] = p->elbow[1] = 0.3f;
  p->out[0] = p->out[1] = 0.4f;
  p->lean = 0.55f + 0.05f * fsin(t * 5);
}

void pose_spray(Pose *p, float t) {
  clear(p);
  p->arm[1] = 2.0f + 0.25f * fsin(t * 9), p->elbow[1] = 0.25f;
  p->out[1] = 0.2f;
  p->arm[0] = 0.1f, p->elbow[0] = 0.3f;
  p->yaw = 1.1f;
  p->knee[0] = p->knee[1] = 0.1f;
  p->hip[0] = 0.15f;
}

void pose_fall(Pose *p, float t) {
  clear(p);
  float k = t > 1 ? 1 : t;
  p->spin = -1.35f * k;
  p->arm[0] = p->arm[1] = 2.4f * k;
  p->out[0] = p->out[1] = 0.6f;
  p->hip[0] = 0.5f * k, p->hip[1] = 0.2f * k;
  p->knee[0] = 0.6f * k, p->knee[1] = 0.3f;
}

/* ---------------------------------------------------------------- the people */
typedef struct {
  uint32_t skin, hair, hat, hat2, top, vest, sleeve, legs, shoes, sole;
  uint8_t head;       /* 0 a cap, 1 a beanie and a ponytail, 2 a flat top and headphones, 3 a mohawk, 4 the Inspector's cap */
  float width;        /* of the chest */
} Look;

static const Look looks[5] = {
  /* Jake: the white cap with its red peak, a white hoodie under a denim vest, light jeans, green shoes */
  {0xF1C7A3, 0x6B3E1F, 0xF4F4F4, 0xD8302A, 0xF2F2F2, 0x5E95CF, 0xF2F2F2, 0x86AEDB, 0x4CAF50, 0xD8302A, 0, 1.0f},
  /* Tricky: red hair in a ponytail under a red beanie, a pink jacket, dark trousers */
  {0xF5D0B5, 0xD9502B, 0xD8302A, 0xF4F4F4, 0xE5476A, 0xE5476A, 0xF4F4F4, 0x2E3440, 0xF4F4F4, 0xE5476A, 1, 0.95f},
  /* Fresh: a flat top, big yellow headphones, a red tracksuit */
  {0x8D5524, 0x161616, 0x161616, 0xF2C230, 0xD8302A, 0xD8302A, 0xD8302A, 0x2A2A30, 0xF4F4F4, 0x2A2A30, 2, 1.0f},
  /* Spike: a green mohawk, a black leather jacket, tartan trousers, boots */
  {0xF1C7A3, 0x3BD16F, 0x3BD16F, 0x3BD16F, 0x3A3A40, 0x26262B, 0x26262B, 0x8E2433, 0x1E1E22, 0x55555C, 3, 1.02f},
  /* the Inspector: a navy uniform and cap with a gold band, a round belly */
  {0xF0B48C, 0xB5B5B5, 0x23375E, 0xE0B13A, 0x23375E, 0x23375E, 0x23375E, 0x1F2D4D, 0x141414, 0x141414, 4, 1.45f},
};

static void head(const Look *k, M3 T, V3 neck) {
  part(T, neck, (V3){0, 0.15f, 0}, 0.15f, 0.15f, 0.15f, k->skin);
  V3 h = vadd(neck, ap(T, (V3){0, 0.15f, 0}));
  /* the face: eyes, brows, a smile (only seen from the front) */
  for (int s = -1; s <= 1; s += 2) {
    part(T, h, (V3){0.065f * (float)s, 0.01f, 0.152f}, 0.03f, 0.04f, 0.004f, 0xFFFFFF);
    part(T, h, (V3){0.06f * (float)s, 0.0f, 0.156f}, 0.018f, 0.026f, 0.004f, 0x1D1D2B);
    part(T, h, (V3){0.065f * (float)s, 0.07f, 0.152f}, 0.04f, 0.012f, 0.006f, k->head == 4 ? 0x9A9A9A : k->hair);
  }
  if (k->head == 4) part(T, h, (V3){0, -0.1f, 0.14f}, 0.14f, 0.07f, 0.03f, 0xC8C8C8);   /* the Inspector's beard */
  else part(T, h, (V3){0, -0.075f, 0.152f}, 0.045f, 0.012f, 0.004f, 0x8A2A2A);
  switch (k->head) {
    case 0:
      part(T, h, (V3){0, 0.03f, -0.12f}, 0.155f, 0.1f, 0.04f, k->hair);
      part(T, h, (V3){0, 0.13f, 0}, 0.165f, 0.065f, 0.165f, k->hat);
      part(T, h, (V3){0, 0.08f, 0.21f}, 0.15f, 0.015f, 0.08f, k->hat2);
      part(T, h, (V3){0, 0.1f, -0.166f}, 0.1f, 0.03f, 0.005f, k->hat2);
      break;
    case 1:
      part(T, h, (V3){0, 0.15f, -0.01f}, 0.168f, 0.08f, 0.168f, k->hat);
      part(T, h, (V3){0, 0.08f, -0.01f}, 0.17f, 0.025f, 0.17f, k->hat2);
      part(T, h, (V3){0, -0.04f, -0.2f}, 0.06f, 0.13f, 0.06f, k->hair);
      part(T, h, (V3){0, 0.02f, -0.13f}, 0.158f, 0.08f, 0.03f, k->hair);
      break;
    case 2:
      part(T, h, (V3){0, 0.2f, 0}, 0.15f, 0.07f, 0.15f, k->hair);
      part(T, h, (V3){0, 0.03f, -0.125f}, 0.152f, 0.1f, 0.03f, k->hair);
      part(T, h, (V3){-0.175f, 0, 0}, 0.04f, 0.08f, 0.07f, k->hat2);
      part(T, h, (V3){0.175f, 0, 0}, 0.04f, 0.08f, 0.07f, k->hat2);
      part(T, h, (V3){0, 0.29f, 0}, 0.19f, 0.02f, 0.03f, 0x2A2A30);
      break;
    case 3:
      part(T, h, (V3){0, 0.25f, 0}, 0.035f, 0.13f, 0.16f, k->hair);
      part(T, h, (V3){0, 0.1f, -0.13f}, 0.035f, 0.18f, 0.03f, k->hair);
      break;
    default:
      part(T, h, (V3){0, 0.14f, 0}, 0.175f, 0.06f, 0.19f, k->hat);
      part(T, h, (V3){0, 0.09f, 0}, 0.18f, 0.025f, 0.195f, k->hat2);
      part(T, h, (V3){0, 0.07f, 0.24f}, 0.16f, 0.015f, 0.07f, 0x141414);
      part(T, h, (V3){0, -0.05f, -0.1f}, 0.15f, 0.06f, 0.06f, k->hair);
      break;
  }
}

void actor_draw(int who, V3 where, const Pose *p, float lt) {
  const Look *k = &looks[who];
  bool big = who == WHO_INSPECTOR;
  at = where, light = lt, size = big ? 1.14f : 1.0f;
  pivot = (V3){0, 0.75f * size, 0};
  root = mul(ry(p->yaw), mul(rz(p->tilt), rx(p->spin)));
  if (p->squash != 1) {
    /* a roll: smaller, about the middle */
    size *= p->squash;
    pivot.y *= p->squash;
  }
  float w = k->width;
  V3 hip = {0, 0.86f + p->bob, 0};
  /* legs */
  for (int s = 0; s < 2; s++) {
    float sx = s ? 0.105f : -0.105f;
    M3 up = rx(-p->hip[s]), lo = mul(up, rx(p->knee[s]));
    V3 j = {sx * (big ? 1.25f : 1), hip.y, 0};
    float th = big ? 0.095f : 0.078f;
    part(up, j, (V3){0, -0.2f, 0}, th, 0.2f, th + 0.005f, k->legs);
    V3 knee = vadd(j, ap(up, (V3){0, -0.4f, 0}));
    part(lo, knee, (V3){0, -0.19f, 0}, th - 0.008f, 0.19f, th - 0.005f, k->legs);
    V3 ankle = vadd(knee, ap(lo, (V3){0, -0.38f, 0}));
    part(lo, ankle, (V3){0, -0.025f, 0.05f}, 0.08f, 0.055f, 0.15f, k->shoes);
    part(lo, ankle, (V3){0, -0.07f, 0.05f}, 0.082f, 0.015f, 0.152f, k->sole);
  }
  /* the hips, then the chest leaning from the waist */
  part(ident, hip, (V3){0, 0.04f, 0}, 0.18f * (big ? 1.3f : 1), 0.08f, 0.11f * (big ? 1.4f : 1), k->legs);
  M3 T = rx(p->lean);
  V3 waist = {0, hip.y + 0.08f, 0};
  float cw = 0.21f * w, cd = big ? 0.22f : 0.125f;
  part(T, waist, (V3){0, 0.27f, big ? 0.04f : 0}, cw, 0.27f, cd, k->top);
  if (k->vest != k->top) part(T, waist, (V3){0, 0.29f, 0}, cw + 0.012f, 0.22f, cd + 0.012f, k->vest);
  if (who == WHO_JAKE) part(T, waist, (V3){0, 0.5f, -0.13f}, 0.13f, 0.07f, 0.04f, k->sleeve);   /* the hood */
  if (who == WHO_SPIKE) part(T, waist, (V3){0, 0.3f, -0.14f}, 0.12f, 0.12f, 0.01f, 0xC8C8D0);  /* a patch */
  /* arms */
  for (int s = 0; s < 2; s++) {
    float sgn = s ? 1.0f : -1.0f;
    M3 up = mul(T, mul(rz(sgn * p->out[s]), rx(-p->arm[s]))), lo = mul(up, rx(-p->elbow[s]));
    V3 sh = vadd(waist, ap(T, (V3){sgn * (cw + 0.07f), 0.47f, 0}));
    float th = big ? 0.075f : 0.062f;
    part(up, sh, (V3){0, -0.14f, 0}, th, 0.15f, th, k->sleeve);
    V3 el = vadd(sh, ap(up, (V3){0, -0.29f, 0}));
    part(lo, el, (V3){0, -0.13f, 0}, th - 0.006f, 0.14f, th - 0.006f, k->sleeve);
    part(lo, el, (V3){0, -0.3f, 0}, 0.05f, 0.05f, 0.05f, k->skin);
  }
  V3 neck = vadd(waist, ap(T, (V3){0, 0.56f, 0}));
  head(k, T, neck);
}

/* ---------------------------------------------------------------- the dog */
void dog_draw(V3 where, float ph, float lt) {
  at = where, light = lt, size = 1, pivot = (V3){0, 0, 0}, root = ident;
  uint32_t fur = 0xA0612B, dark = 0x6E3F18, light_fur = 0xD9A066;
  float s = fsin(ph), bob = 0.04f * (s < 0 ? -s : s);
  part(ident, (V3){0, 0.44f + bob, 0}, (V3){0, 0, 0}, 0.13f, 0.12f, 0.3f, fur);
  part(ident, (V3){0, 0.38f + bob, 0}, (V3){0, 0, 0.02f}, 0.11f, 0.07f, 0.24f, light_fur);
  M3 H = rx(-0.15f);
  V3 neck = {0, 0.56f + bob, 0.3f};
  part(H, neck, (V3){0, 0.04f, 0.04f}, 0.11f, 0.1f, 0.11f, fur);
  part(H, neck, (V3){0, -0.01f, 0.18f}, 0.06f, 0.05f, 0.06f, light_fur);
  part(H, neck, (V3){-0.09f, 0.14f, 0.0f}, 0.03f, 0.06f, 0.035f, dark);
  part(H, neck, (V3){0.09f, 0.14f, 0.0f}, 0.03f, 0.06f, 0.035f, dark);
  for (int i = 0; i < 4; i++) {
    float lx = i & 1 ? 0.08f : -0.08f, lz = i < 2 ? 0.2f : -0.2f;
    float a = 0.7f * fsin(ph + (i < 2 ? 0 : 3.14159f) + (i & 1 ? 0.5f : 0));
    part(rx(-a), (V3){lx, 0.38f + bob, lz}, (V3){0, -0.17f, 0}, 0.035f, 0.17f, 0.035f, i < 2 ? fur : dark);
  }
  part(rx(-0.9f + 0.3f * s), (V3){0, 0.5f + bob, -0.3f}, (V3){0, 0, -0.1f}, 0.025f, 0.025f, 0.12f, fur);
}

/* ---------------------------------------------------------------- boards, the jetpack, shadows */
static const uint32_t board_cols[3][3] = {
  {0x2E7BE0, 0xF2C230, 0x173E73},   /* the Hoverboard */
  {0xF07F2D, 0x3BD16F, 0x7A3A10},   /* the Bouncer */
  {0xD8302A, 0xF2C230, 0x5A1010},   /* the Daredevil */
};

void board_draw(V3 p, int kind, float yaw, float t, float lt) {
  const uint32_t *c = board_cols[kind % 3];
  light = lt;
  float sy, cy;
  r_sincos(yaw, &sy, &cy);
  float hw = 0.3f, hl = 0.82f, y = p.y + 0.16f + 0.03f * fsin(t * 6);
  /* its outline: pointed at both ends, as the game's boards */
  static const float o[8][2] = {{0, 1}, {0.8f, 0.8f}, {1, 0.25f}, {1, -0.25f}, {0.8f, -0.8f}, {0, -1}, {-0.8f, -0.8f}, {-1, -0.25f}};
  static const float o8[2] = {-1, 0.25f};
  V3 top8[9], bot8[9];
  for (int i = 0; i < 9; i++) {
    const float *q = i < 8 ? o[i] : o8;
    float ox = q[0] * hw, oz = q[1] * hl, rx = ox * cy + oz * sy, rz = -ox * sy + oz * cy;
    top8[i] = (V3){p.x + rx, y + 0.07f, p.z + rz};
    bot8[i] = (V3){p.x + rx * 0.92f, y, p.z + rz * 0.92f};
  }
  /* (nine corners in all; r_poly takes eight at most, so the top is two halves) */
  V3 a[5] = {top8[0], top8[1], top8[2], top8[3], top8[4]}, b[6] = {top8[4], top8[5], top8[6], top8[7], top8[8], top8[0]};
  uint32_t face = shade(r_shade(c[0], (V3){0, 1, 0}));
  r_poly(a, 5, face, 0, 0);
  r_poly(b, 6, face, 0, 0);
  /* the stripe down the middle */
  float sx = 0.07f * cy, sz = -0.07f * sy, lx = hl * 0.8f * sy, lz = hl * 0.8f * cy;
  V3 st[4] = {{p.x - sx + lx, y + 0.071f, p.z - sz + lz}, {p.x + sx + lx, y + 0.071f, p.z + sz + lz},
              {p.x + sx - lx, y + 0.071f, p.z + sz - lz}, {p.x - sx - lx, y + 0.071f, p.z - sz - lz}};
  r_poly(st, 4, shade(c[1]), P_DECAL | P_TWOSIDED, 0);
  /* the rim, all round */
  for (int i = 0; i < 8; i++) {
    V3 r[4] = {top8[i + 1], top8[i], bot8[i], bot8[i + 1]};
    r_poly(r, 4, shade(c[2]), P_TWOSIDED, 0);
  }
  /* the glow under it */
  V3 g[4] = {{p.x - 0.55f, p.y + 0.03f, p.z + 0.95f}, {p.x + 0.55f, p.y + 0.03f, p.z + 0.95f},
             {p.x + 0.55f, p.y + 0.03f, p.z - 0.95f}, {p.x - 0.55f, p.y + 0.03f, p.z - 0.95f}};
  r_poly(g, 4, 0x7FE8FF, P_ALPHA | P_NOHAZE, 90);
}

void jetpack_draw(V3 p, const Pose *ps, float t, float lt) {
  at = p, light = lt, size = 1, pivot = (V3){0, 0.75f, 0};
  root = mul(ry(ps->yaw), mul(rz(ps->tilt), rx(ps->spin)));
  M3 T = rx(ps->lean);
  V3 waist = {0, 0.94f + ps->bob, 0};
  for (int s = -1; s <= 1; s += 2) {
    part(T, waist, (V3){0.11f * (float)s, 0.3f, -0.24f}, 0.085f, 0.2f, 0.085f, 0xC9CFD6);
    part(T, waist, (V3){0.11f * (float)s, 0.48f, -0.24f}, 0.07f, 0.03f, 0.07f, 0xD8302A);
    part(T, waist, (V3){0.11f * (float)s, 0.08f, -0.24f}, 0.06f, 0.03f, 0.06f, 0x3A3A40);
    /* the flame */
    float len = 0.35f + 0.12f * fsin(t * 37 + (float)s);
    V3 n = vadd(at, ap(root, vsub(vadd(waist, ap(T, (V3){0.11f * (float)s, 0.05f, -0.24f})), pivot)));
    n = vadd(n, pivot);
    V3 f[3] = {{n.x - 0.07f, n.y, n.z - 0.02f}, {n.x + 0.07f, n.y, n.z - 0.02f}, {n.x, n.y - len, n.z - 0.12f}};
    r_poly(f, 3, 0xFFB020, P_ALPHA | P_NOHAZE | P_TWOSIDED, 220);
    V3 f2[3] = {{n.x - 0.035f, n.y, n.z - 0.03f}, {n.x + 0.035f, n.y, n.z - 0.03f}, {n.x, n.y - len * 0.6f, n.z - 0.1f}};
    r_poly(f2, 3, 0xFFF4B0, P_ALPHA | P_NOHAZE | P_TWOSIDED, 230);
  }
}

void shadow_draw(float x, float z, float ground, float h) {
  float k = 1 - h * 0.12f;
  if (k < 0.3f) k = 0.3f;
  static const float o[8][2] = {{0, 1}, {0.71f, 0.71f}, {1, 0}, {0.71f, -0.71f}, {0, -1}, {-0.71f, -0.71f}, {-1, 0}, {-0.71f, 0.71f}};
  V3 v[8];
  for (int i = 0; i < 8; i++) v[i] = (V3){x + o[i][0] * 0.42f * k, ground + 0.02f, z + o[i][1] * 0.4f * k};
  r_poly(v, 8, 0x000000, P_ALPHA | P_NOHAZE, (int)(110 * k));
}

/* ---------------------------------------------------------------- the things to pick up */
void actors_pickup(const Item *it, float t) {
  float bobv = 0.12f * fsin(t * 3 + it->z);
  at = (V3){it->x, it->y + bobv, it->z}, light = scene_light(it->z), size = 1, pivot = (V3){0, 0, 0};
  root = ry(t * 2.2f + it->z);
  M3 I = ident;
  V3 o = {0, 0, 0};
  switch (it->kind) {
    case I_MAGNET:
      part(I, o, (V3){-0.22f, 0.05f, 0}, 0.09f, 0.25f, 0.09f, 0xE0302A);
      part(I, o, (V3){0.22f, 0.05f, 0}, 0.09f, 0.25f, 0.09f, 0xE0302A);
      part(I, o, (V3){0, -0.25f, 0}, 0.31f, 0.09f, 0.09f, 0xE0302A);
      part(I, o, (V3){-0.22f, 0.36f, 0}, 0.092f, 0.07f, 0.092f, 0xE8ECF0);
      part(I, o, (V3){0.22f, 0.36f, 0}, 0.092f, 0.07f, 0.092f, 0xE8ECF0);
      break;
    case I_JETPACK:
      part(I, o, (V3){-0.13f, 0, 0}, 0.11f, 0.28f, 0.11f, 0xC9CFD6);
      part(I, o, (V3){0.13f, 0, 0}, 0.11f, 0.28f, 0.11f, 0xC9CFD6);
      part(I, o, (V3){-0.13f, 0.3f, 0}, 0.09f, 0.04f, 0.09f, 0xD8302A);
      part(I, o, (V3){0.13f, 0.3f, 0}, 0.09f, 0.04f, 0.09f, 0xD8302A);
      part(I, o, (V3){0, 0.05f, 0.08f}, 0.06f, 0.2f, 0.04f, 0x3A3A40);
      part(I, o, (V3){-0.13f, -0.32f, 0}, 0.07f, 0.05f, 0.07f, 0xF2C230);
      part(I, o, (V3){0.13f, -0.32f, 0}, 0.07f, 0.05f, 0.07f, 0xF2C230);
      break;
    case I_SNEAKERS:
      for (int s = -1; s <= 1; s += 2) {
        float x = 0.15f * (float)s;
        part(I, o, (V3){x, -0.15f, 0.05f}, 0.1f, 0.04f, 0.26f, 0xF4F4F4);
        part(I, o, (V3){x, -0.03f, -0.02f}, 0.095f, 0.09f, 0.19f, 0x3BD16F);
        part(I, o, (V3){x, 0.1f, -0.1f}, 0.09f, 0.08f, 0.09f, 0x3BD16F);
        part(I, o, (V3){x, -0.02f, 0.18f}, 0.06f, 0.06f, 0.05f, 0xF2C230);
      }
      break;
    case I_DOUBLE: {
      /* a purple star, 2x */
      V3 c = at;
      float s, co;
      r_sincos(t * 2.2f + it->z, &s, &co);
      for (int i = 0; i < 5; i++) {
        float a0 = (float)i * 1.2566f, a1 = a0 + 0.6283f, a2 = a0 - 0.6283f;
        float x0 = fsin(a0) * 0.45f, y0 = fcos(a0) * 0.45f, x1 = fsin(a1) * 0.2f, y1 = fcos(a1) * 0.2f, x2 = fsin(a2) * 0.2f,
              y2 = fcos(a2) * 0.2f;
        V3 tri[3] = {{c.x + co * x2, c.y + y2, c.z + s * x2}, {c.x + co * x0, c.y + y0, c.z + s * x0}, {c.x + co * x1, c.y + y1, c.z + s * x1}};
        r_poly(tri, 3, shade(0xA855F7), P_TWOSIDED, 0);
      }
      V3 pent[5];
      for (int i = 0; i < 5; i++) {
        float a = (float)i * 1.2566f + 0.6283f, x = fsin(a) * 0.205f, y = fcos(a) * 0.205f;
        pent[i] = (V3){c.x + co * x, c.y + y, c.z + s * x};
      }
      r_poly(pent, 5, shade(0xA855F7), P_TWOSIDED, 0);
      /* "2": two bars and a step, in yellow */
      float bars[3][4] = {{-0.09f, 0.08f, 0.09f, 0.12f}, {-0.09f, -0.12f, 0.09f, -0.08f}, {-0.04f, -0.1f, 0.05f, 0.1f}};
      for (int i = 0; i < 3; i++) {
        float x0 = bars[i][0], y0 = bars[i][1], x1 = bars[i][2], y1 = bars[i][3];
        if (i == 2) x0 = -0.02f, x1 = 0.02f;
        V3 q[4] = {{c.x + co * x0, c.y + y1, c.z + s * x0 - 0.01f}, {c.x + co * x1, c.y + y1, c.z + s * x1 - 0.01f},
                   {c.x + co * x1, c.y + y0, c.z + s * x1 - 0.01f}, {c.x + co * x0, c.y + y0, c.z + s * x0 - 0.01f}};
        r_poly(q, 4, shade(0xFFE040), P_TWOSIDED | P_DECAL, 0);
      }
      break;
    }
    case I_MYSTERY:
      part(I, o, (V3){0, 0, 0}, 0.3f, 0.3f, 0.3f, 0x7B3FE4);
      part(I, o, (V3){0, 0.31f, 0}, 0.31f, 0.02f, 0.06f, 0xF2C230);
      part(I, o, (V3){0, 0.31f, 0}, 0.06f, 0.02f, 0.31f, 0xF2C230);
      part(I, o, (V3){0, 0, -0.31f}, 0.06f, 0.3f, 0.01f, 0xF2C230);
      part(I, o, (V3){0, 0, 0.31f}, 0.06f, 0.3f, 0.01f, 0xF2C230);
      part(I, o, (V3){0, 0.36f, 0}, 0.1f, 0.05f, 0.04f, 0xF2C230);
      break;
    case I_KEY:
      part(I, o, (V3){0, 0.22f, 0}, 0.16f, 0.04f, 0.04f, 0x3FA9F5);
      part(I, o, (V3){0, 0.06f, 0}, 0.16f, 0.04f, 0.04f, 0x3FA9F5);
      part(I, o, (V3){-0.14f, 0.14f, 0}, 0.04f, 0.12f, 0.04f, 0x3FA9F5);
      part(I, o, (V3){0.14f, 0.14f, 0}, 0.04f, 0.12f, 0.04f, 0x3FA9F5);
      part(I, o, (V3){0, -0.2f, 0}, 0.04f, 0.24f, 0.04f, 0x3FA9F5);
      part(I, o, (V3){0.08f, -0.3f, 0}, 0.06f, 0.03f, 0.035f, 0x3FA9F5);
      part(I, o, (V3){0.07f, -0.4f, 0}, 0.05f, 0.03f, 0.035f, 0x3FA9F5);
      break;
  }
  /* a soft glow on the ground under it */
  float gy = world_ground(it->x, it->z, it->y) + 0.02f;
  V3 g[4] = {{it->x - 0.6f, gy, it->z + 0.6f}, {it->x + 0.6f, gy, it->z + 0.6f}, {it->x + 0.6f, gy, it->z - 0.6f}, {it->x - 0.6f, gy, it->z - 0.6f}};
  r_poly(g, 4, it->kind == I_KEY ? 0x7FD0FF : 0xFFE680, P_ALPHA | P_NOHAZE, 70);
}
