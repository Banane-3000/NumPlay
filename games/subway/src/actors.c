/* The people, the dog, the power-ups and the boards (actors.h).
 *
 * The people are meshes fitted to the game's own models (data.c, from tools/models.py), on a skeleton:
 * each corner follows a bone, or two at a joint (blended, so knees and elbows bend smoothly). A pose turns
 * each bone from where it is at rest, the T pose; the animations are keyframes of poses played in a loop
 * (running, standing, on the board, flying) or once over a move (a jump, a roll, a fall), blended softly
 * from key to key. The dog has a skeleton of its own. */
#include "actors.h"
#include "data.h"
#include "scene.h"

/* ---------------------------------------------------------------- 3x4 matrices */
typedef struct {
  float m[12];   /* x' = m0 x + m1 y + m2 z + m3, y' = m4 x ..., z' = m8 x ... */
} M34;

static M34 mul(const M34 *a, const M34 *b) {
  M34 r;
  for (int i = 0; i < 3; i++) {
    const float *A = a->m + 4 * i;
    for (int j = 0; j < 4; j++) r.m[4 * i + j] = A[0] * b->m[j] + A[1] * b->m[4 + j] + A[2] * b->m[8 + j] + (j == 3 ? A[3] : 0);
  }
  return r;
}
static V3 ap(const M34 *a, V3 v) {
  const float *m = a->m;
  return (V3){m[0] * v.x + m[1] * v.y + m[2] * v.z + m[3], m[4] * v.x + m[5] * v.y + m[6] * v.z + m[7],
              m[8] * v.x + m[9] * v.y + m[10] * v.z + m[11]};
}
static float fsin(float a) {
  float s, c;
  r_sincos(a, &s, &c);
  return s;
}
static float fcos(float a) { return fsin(a + 1.5707963f); }

/* turned about x, then y, then z (R = Rx Ry Rz), about the point j */
static M34 rot_about(const float *a, V3 j) {
  float sx, cx, sy, cy, sz, cz;
  r_sincos(a[0], &sx, &cx);
  r_sincos(a[1], &sy, &cy);
  r_sincos(a[2], &sz, &cz);
  /* Ry Rz */
  float b0 = cy * cz, b1 = -cy * sz, b2 = sy;
  float b4 = sz, b5 = cz, b6 = 0;
  float b8 = -sy * cz, b9 = sy * sz, b10 = cy;
  M34 r = {{b0, b1, b2, 0, cx * b4 - sx * b8, cx * b5 - sx * b9, cx * b6 - sx * b10, 0, sx * b4 + cx * b8,
            sx * b5 + cx * b9, sx * b6 + cx * b10, 0}};
  /* about j: R (p - j) + j */
  r.m[3] = j.x - (r.m[0] * j.x + r.m[1] * j.y + r.m[2] * j.z);
  r.m[7] = j.y - (r.m[4] * j.x + r.m[5] * j.y + r.m[6] * j.z);
  r.m[11] = j.z - (r.m[8] * j.x + r.m[9] * j.y + r.m[10] * j.z);
  return r;
}
static M34 translate(float x, float y, float z) { return (M34){{1, 0, 0, x, 0, 1, 0, y, 0, 0, 1, z}}; }
static M34 rot_y(float a) {
  float s, c;
  r_sincos(a, &s, &c);
  return (M34){{c, 0, s, 0, 0, 1, 0, 0, -s, 0, c, 0}};
}

/* the camera as a matrix: world to camera space (r_view) */
static M34 view(void) {
  return (M34){{1, 0, 0, -cam.x, 0, cam.cp, cam.sp, -cam.y * cam.cp - cam.z * cam.sp, 0, -cam.sp, cam.cp,
                cam.y * cam.sp - cam.z * cam.cp}};
}

/* ---------------------------------------------------------------- drawing a model */
#define MAXB 12
static M34 bone_w[MAXB];   /* the bones of the model just drawn, in the world */

/* where the light comes from (in the world): above, to the left, from the camera's side */
static const V3 sun = {-0.42f, 0.78f, -0.47f};

static void draw_model(const Model *m, const float (*a)[3], const M34 *root, float light) {
  int nb = m->nbones;
  M34 cm[MAXB], vw = view();
  V3 lb[MAXB];
  for (int i = 0; i < nb; i++) {
    V3 j = {m->joints[i][0] * 0.001f, m->joints[i][1] * 0.001f, m->joints[i][2] * 0.001f};
    M34 local = rot_about(a[i], j);
    bone_w[i] = m->parent[i] < 0 ? mul(root, &local) : mul(&bone_w[(int)m->parent[i]], &local);
    cm[i] = mul(&vw, &bone_w[i]);
    /* the light in the bone's own frame (normals are kept at rest): the rotation's transpose */
    const float *w = bone_w[i].m;
    lb[i] = (V3){(w[0] * sun.x + w[4] * sun.y + w[8] * sun.z) * (1 / 127.0f),
                 (w[1] * sun.x + w[5] * sun.y + w[9] * sun.z) * (1 / 127.0f),
                 (w[2] * sun.x + w[6] * sun.y + w[10] * sun.z) * (1 / 127.0f)};
  }
  /* (nothing to do far away or behind) */
  V3 c = ap(&cm[0], (V3){m->joints[0][0] * 0.001f, m->joints[0][1] * 0.001f, m->joints[0][2] * 0.001f});
  if (c.z < -2.5f || c.z > 140) return;
  float amb = 0.6f * light * 255, dif = 0.45f * light * 255;
  RV rv[256];
  for (int pi = 0; pi < m->nparts; pi++) {
    const MPart *p = &m->parts[pi];
    const MV *q = m->v + p->v0;
    for (int k = 0; k < p->nv; k++, q++) {
      V3 r = {q->x * 0.001f, q->y * 0.001f, q->z * 0.001f};
      const M34 *A = &cm[q->b0];
      V3 v = ap(A, r);
      V3 L = lb[q->b0];
      float d = q->nx * L.x + q->ny * L.y + q->nz * L.z;
      if (q->w) {
        float t = q->w * (1 / 256.0f);
        V3 v2 = ap(&cm[q->b1], r);
        V3 L2 = lb[q->b1];
        float d2 = q->nx * L2.x + q->ny * L2.y + q->nz * L2.z;
        v.x += (v2.x - v.x) * t, v.y += (v2.y - v.y) * t, v.z += (v2.z - v.z) * t;
        d += (d2 - d) * t;
      }
      float s = amb + dif * (d > 0 ? d : 0);
      rv[k] = (RV){v.x, v.y, v.z, q->u, q->v, (uint8_t)(s > 255 ? 255 : s), {0, 0, 0}};
    }
    r_mesh(rv, p->nv, m->t + p->t0, p->nt, m->pal, p->tex);
  }
}

/* ---------------------------------------------------------------- keyframes */
/* a key: each bone's angles (degrees) and the body raised (cm) */
typedef struct {
  int8_t a[NBONES][3];
  int8_t y;
} Key;
typedef struct {
  const Key *k;
  uint8_t n, loop;
} Anim;

/* (the arms are lowered from the T pose about z, the left one with +, the right with -; an elbow bends
 * forward about y, the same way; a leg swings forward with - and spreads with its own sign) */
#define AL(sw, dn, tw) {sw, tw, dn}
#define AR(sw, dn, tw) {sw, -(tw), -(dn)}
#define FL(b) {0, b, 0}
#define FR(b) {0, -(b), 0}
#define LL(sw, out) {sw, 0, -(out)}
#define LR(sw, out) {sw, 0, out}
#define SH(b) {b, 0, 0}

/* bones: hips, chest, head, upper arm, forearm (left, right), thigh, shin (left, right) */
static const Key k_run[4] = {
  /* the left foot lands ahead, the right one pushes off behind, the arms the other way round */
  {{{0, 8, 0}, {14, -10, 0}, {-8, 6, 0}, AL(38, 72, 0), FL(80), AR(-48, 72, 0), FR(105), LL(-50, 3), SH(18), LR(32, 3), SH(58)}, -2},
  /* passing: the right knee comes through high */
  {{{0, 0, 0}, {16, 0, 0}, {-8, 0, 0}, AL(4, 72, 0), FL(95), AR(-6, 72, 0), FR(95), LL(12, 3), SH(16), LR(-58, 3), SH(112)}, 6},
  {{{0, -8, 0}, {14, 10, 0}, {-8, -6, 0}, AL(-48, 72, 0), FL(105), AR(38, 72, 0), FR(80), LL(32, 3), SH(58), LR(-50, 3), SH(18)}, -2},
  {{{0, 0, 0}, {16, 0, 0}, {-8, 0, 0}, AL(-6, 72, 0), FL(95), AR(4, 72, 0), FR(95), LL(-58, 3), SH(112), LR(12, 3), SH(16)}, 6},
};
static const Key k_stand[2] = {
  {{{0, 0, 0}, {2, 0, 0}, {0, 0, 0}, AL(4, 77, 0), FL(14), AR(4, 77, 0), FR(14), LL(0, 4), SH(2), LR(0, 4), SH(2)}, 0},
  {{{0, 4, 0}, {4, 3, 0}, {3, -5, 0}, AL(7, 75, 0), FL(18), AR(2, 79, 0), FR(10), LL(-2, 4), SH(4), LR(1, 4), SH(2)}, -1},
};
static const Key k_jump[3] = {
  /* off the ground, arms swinging up, then tucked up high, then reaching down to land */
  {{{0, 0, 0}, {10, 0, 0}, {-6, 0, 0}, AL(-40, 50, 0), FL(60), AR(-60, 40, 0), FR(50), LL(-20, 4), SH(30), LR(25, 4), SH(40)}, 0},
  {{{0, 0, 0}, {6, 0, 0}, {-4, 0, 0}, AL(-20, 15, 0), FL(40), AR(-105, 45, 0), FR(30), LL(-82, 6), SH(115), LR(-32, 6), SH(122)}, 8},
  {{{0, 0, 0}, {14, 0, 0}, {-6, 0, 0}, AL(10, 55, 0), FL(50), AR(-22, 50, 0), FR(60), LL(-36, 4), SH(36), LR(-10, 4), SH(46)}, 0},
};
static const Key k_roll[3] = {
  /* down into a ball, round once, and up again into the run */
  {{{0, 0, 0}, {48, 0, 0}, {28, 0, 0}, AL(-60, 60, 0), FL(100), AR(-60, 60, 0), FR(100), LL(-98, 8), SH(125), LR(-98, 8), SH(125)}, -30},
  {{{0, 0, 0}, {62, 0, 0}, {40, 0, 0}, AL(-70, 62, 0), FL(115), AR(-70, 62, 0), FR(115), LL(-118, 6), SH(145), LR(-118, 6), SH(145)}, -38},
  {{{0, 0, 0}, {20, 0, 0}, {0, 0, 0}, AL(10, 72, 0), FL(90), AR(-30, 72, 0), FR(90), LL(-40, 3), SH(40), LR(20, 3), SH(60)}, 0},
};
static const Key k_hover[2] = {
  /* on the board: side on, knees bent, arms out for balance */
  {{{0, 28, 0}, {12, -10, 0}, {-5, -22, 0}, AL(0, 35, 0), FL(30), AR(10, 42, 0), FR(25), LL(-25, 12), SH(45), LR(15, 12), SH(40)}, -8},
  {{{0, 28, 0}, {15, -8, 0}, {-5, -18, 0}, AL(6, 28, 0), FL(36), AR(4, 48, 0), FR(30), LL(-28, 12), SH(50), LR(18, 12), SH(46)}, -10},
};
static const Key k_fly[2] = {
  /* the jetpack: arms forward, legs trailing */
  {{{0, 0, 0}, {10, 0, 0}, {-28, 0, 0}, AL(-32, 45, 0), FL(70), AR(-32, 45, 0), FR(70), LL(14, 6), SH(40), LR(30, 6), SH(62)}, 0},
  {{{0, 0, 0}, {10, 4, 0}, {-26, 0, 0}, AL(-26, 50, 0), FL(64), AR(-38, 40, 0), FR(76), LL(26, 6), SH(56), LR(16, 6), SH(40)}, 0},
};
static const Key k_fall[3] = {
  /* hit: thrown back, arms up; falling; flat on the ground */
  {{{0, 0, 0}, {-16, 0, 0}, {-22, 0, 0}, AL(-30, 22, 0), FL(20), AR(-35, 18, 0), FR(20), LL(-22, 10), SH(20), LR(10, 10), SH(22)}, 0},
  {{{0, 0, 0}, {-10, 0, 0}, {-12, 0, 0}, AL(-60, 30, 0), FL(30), AR(-72, 20, 0), FR(30), LL(-50, 15), SH(40), LR(-30, 15), SH(30)}, 0},
  {{{0, 0, 0}, {0, 0, 0}, {12, 0, 0}, AL(0, 5, 0), FL(10), AR(0, -8, 0), FR(10), LL(-10, 15), SH(12), LR(0, 15), SH(22)}, 0},
};
static const Key k_spray[2] = {
  /* the can up at the train, shaken */
  {{{0, 0, 0}, {4, 10, 0}, {-4, 15, 0}, AL(4, 75, 0), FL(20), AR(-95, 20, 0), FR(8), LL(-4, 6), SH(4), LR(6, 6), SH(6)}, 0},
  {{{0, 0, 0}, {5, 12, 0}, {-6, 12, 0}, AL(6, 74, 0), FL(24), AR(-105, 26, 0), FR(16), LL(-4, 6), SH(4), LR(6, 6), SH(6)}, 0},
};
static const Key k_catch[2] = {
  /* (arms reaching forward: lowered, then swung up in front) */
  {{{0, 0, 0}, {8, 0, 0}, {6, 0, 0}, AL(-70, 80, 0), FL(30), AR(-82, 80, 0), FR(25), LL(-10, 6), SH(8), LR(8, 6), SH(10)}, 0},
  {{{0, 0, 0}, {10, 0, 0}, {10, 0, 0}, AL(-85, 80, 0), FL(40), AR(-68, 80, 0), FR(30), LL(-10, 6), SH(8), LR(8, 6), SH(10)}, 0},
};
static const Key k_trip = {
  /* a stumble: arms flung up, the body twisted */
  {{0, 0, 0}, {-12, 22, 14}, {-16, -20, 10}, AL(-70, 15, 0), FL(30), AR(-30, 5, 0), FR(25), LL(-30, 6), SH(40), LR(20, 6), SH(50)}, 0};

#define DEG 0.0174533f
static float smooth(float t) { return t * t * (3 - 2 * t); }

static void set_key(Pose *p, const Key *a, const Key *b, float t) {
  t = smooth(t);
  for (int i = 0; i < NBONES; i++)
    for (int k = 0; k < 3; k++) p->a[i][k] = ((float)a->a[i][k] + (float)(b->a[i][k] - a->a[i][k]) * t) * DEG;
  p->y = ((float)a->y + (float)(b->y - a->y) * t) * 0.01f;
  p->lean = p->tilt = p->yaw = p->spin = 0;
}

/* plays an animation: a loop over t (one round per 1), or once from t = 0 to 1 */
static void play(Pose *p, const Key *k, int n, bool loop, float t) {
  if (loop) {
    t -= (float)(int)t;
    if (t < 0) t += 1;
    float f = t * (float)n;
    int i = (int)f;
    if (i >= n) i = n - 1;
    set_key(p, &k[i], &k[(i + 1) % n], f - (float)i);
  } else {
    if (t < 0) t = 0;
    if (t > 1) t = 1;
    float f = t * (float)(n - 1);
    int i = (int)f;
    if (i >= n - 1) i = n - 2;
    set_key(p, &k[i], &k[i + 1], f - (float)i);
  }
}

void pose_run(Pose *p, float ph) { play(p, k_run, 4, true, ph * (1 / 6.2831853f)); }
void pose_stand(Pose *p, float t) { play(p, k_stand, 2, true, t * 0.3f); }
void pose_jump(Pose *p, float t) { play(p, k_jump, 3, false, t); }
void pose_hover(Pose *p, float t) { play(p, k_hover, 2, true, t * 0.6f); }
void pose_spray(Pose *p, float t) { play(p, k_spray, 2, true, t * 3.0f); }
void pose_catch(Pose *p, float t) { play(p, k_catch, 2, true, t * 1.2f); }

void pose_roll(Pose *p, float t) {
  play(p, k_roll, 3, false, t < 0.2f ? t * 2.5f : t < 0.75f ? 0.5f : 0.5f + (t - 0.75f) * 2);
  /* round once, head over heels */
  float s = (t - 0.08f) / 0.75f;
  p->spin = 6.2831853f * smooth(s < 0 ? 0 : s > 1 ? 1 : s);
}

void pose_fly(Pose *p, float t) {
  play(p, k_fly, 2, true, t * 0.5f);
  p->lean = 0.55f + 0.05f * fsin(t * 5);
}

void pose_fall(Pose *p, float t) {
  play(p, k_fall, 3, false, t);
  float k = t > 1 ? 1 : t < 0 ? 0 : t;
  /* backwards onto the ground (about the feet) */
  p->lean = -1.5f * smooth(k);
}

void pose_stumble(Pose *p, float k) {
  if (k <= 0) return;
  if (k > 1) k = 1;
  k = smooth(k);
  for (int i = 1; i < NBONES; i++)
    for (int j = 0; j < 3; j++) p->a[i][j] += ((float)k_trip.a[i][j] * DEG - p->a[i][j]) * k * (i == BN_LEG_L || i >= BN_SHIN_L ? 0.4f : 1);
}

/* ---------------------------------------------------------------- the people */
static const Model *const people[2][5] = {
  {&model_jake, &model_tricky, &model_fresh, &model_spike, &model_inspector},
  /* (with fewer corners, for when they are small on the screen) */
  {&model_jake_lo, &model_tricky_lo, &model_fresh_lo, &model_spike_lo, &model_inspector_lo},
};
static const Model *drawn;   /* the person just drawn (for the jetpack) */

static M34 body_root(const Model *m, V3 at, const Pose *p) {
  /* the hips: where the body turns, and the middle of a roll */
  float hy = m->joints[0][1] * 0.001f;
  M34 r = translate(at.x, at.y + p->y, at.z), y = rot_y(p->yaw);
  r = mul(&r, &y);
  float a[3] = {p->lean + p->spin, 0, p->tilt};
  M34 lt = rot_about(a, p->spin != 0 ? (V3){0, hy, 0.1f} : (V3){0, p->lean < 0 ? 0.05f : hy, 0});
  return mul(&r, &lt);
}

void actor_draw(int who, V3 at, const Pose *p, float lt) {
  V3 v = r_view(at);
  const Model *m = people[v.z > 4.5f][who];
  drawn = m;
  M34 root = body_root(m, at, p);
  float a[NBONES][3];
  for (int i = 0; i < NBONES; i++) a[i][0] = p->a[i][0], a[i][1] = p->a[i][1], a[i][2] = p->a[i][2];
  if (who == WHO_FRESH) {
    /* his left hand up, holding the boombox on his shoulder */
    a[BN_ARM_L][0] = 0.2f, a[BN_ARM_L][1] = -0.3f, a[BN_ARM_L][2] = -0.35f;
    a[BN_FORE_L][0] = 0, a[BN_FORE_L][1] = 0.5f, a[BN_FORE_L][2] = -2.2f;
  }
  draw_model(m, (const float(*)[3])a, &root, lt);
}

/* ---------------------------------------------------------------- the dog */
void dog_draw(V3 at, float ph, float lt) { dog_draw_turned(at, ph, lt, 0); }

void dog_draw_turned(V3 at, float ph, float lt, float yaw) {
  const Model *m = &model_dog;
  /* a gallop: the front legs together, then the back ones; the body rocks */
  float s = fsin(ph), c = fcos(ph);
  float a[7][3] = {{0}};
  a[0][0] = 0.12f * c;               /* the body */
  a[1][0] = -0.1f * c - 0.1f;        /* the head, steady */
  a[2][0] = a[3][0] = -0.75f * s;    /* front legs */
  a[3][0] += 0.2f;
  a[4][0] = a[5][0] = 0.75f * s;     /* back legs */
  a[5][0] -= 0.2f;
  a[6][0] = -0.5f + 0.3f * s;        /* the tail */
  M34 root = translate(at.x, at.y + 0.05f * (s < 0 ? -s : s), at.z), ry = rot_y(yaw);
  root = mul(&root, &ry);
  draw_model(m, (const float(*)[3])a, &root, lt);
}

/* ---------------------------------------------------------------- boards, the jetpack, shadows */
static uint32_t lit(uint32_t c, float light) {
  if (light >= 1) return c;
  return r_mix888(r_mix888(c, 0, (int)((1 - light) * 256)), 0x10203A, 24);
}

static const uint32_t board_cols[3][3] = {
  {0xE8402A, 0xF6B24A, 0x8A1A12},   /* the Hoverboard: red, its stripes orange */
  {0x3C9BE8, 0xF2E24A, 0x174A78},   /* the Bouncer */
  {0x7A3FD8, 0x58E0F0, 0x2E1460},   /* the Daredevil */
};

void board_draw(V3 p, int kind, float yaw, float t, float lt) {
  const uint32_t *c = board_cols[kind % 3];
  float sy, cy;
  r_sincos(yaw, &sy, &cy);
  float hw = 0.3f, hl = 0.85f, y = p.y + 0.16f + 0.03f * fsin(t * 6);
  /* a rounded deck, longer than wide, as the game's boards */
  static const float o[8][2] = {{0, 1}, {0.75f, 0.9f}, {1, 0.45f}, {1, -0.45f}, {0.75f, -0.9f}, {0, -1}, {-0.75f, -0.9f}, {-1, -0.45f}};
  static const float o8[2] = {-1, 0.45f};
  V3 top8[9], bot8[9];
  for (int i = 0; i < 9; i++) {
    const float *q = i < 8 ? o[i] : o8;
    float ox = q[0] * hw, oz = q[1] * hl, rx = ox * cy + oz * sy, rz = -ox * sy + oz * cy;
    top8[i] = (V3){p.x + rx, y + 0.07f, p.z + rz};
    bot8[i] = (V3){p.x + rx * 0.9f, y, p.z + rz * 0.9f};
  }
  V3 a[5] = {top8[0], top8[1], top8[2], top8[3], top8[4]}, b[6] = {top8[4], top8[5], top8[6], top8[7], top8[8], top8[0]};
  uint32_t face = lit(r_shade(c[0], (V3){0, 1, 0}), lt);
  r_poly(a, 5, face, 0, 0);
  r_poly(b, 6, face, 0, 0);
  /* stripes across */
  for (int i = -2; i <= 2; i++) {
    float zc = (float)i * 0.26f, w = 0.05f, x0 = -hw * 0.75f, x1 = hw * 0.75f;
    V3 st[4];
    float px[4] = {x0, x1, x1, x0}, pz[4] = {zc + w, zc + w, zc - w, zc - w};
    for (int k = 0; k < 4; k++) st[k] = (V3){p.x + px[k] * cy + pz[k] * sy, y + 0.071f, p.z - px[k] * sy + pz[k] * cy};
    r_poly(st, 4, lit(c[1], lt), P_DECAL | P_TWOSIDED, 0);
  }
  /* the rim, all round */
  for (int i = 0; i < 8; i++) {
    V3 r[4] = {top8[i + 1], top8[i], bot8[i], bot8[i + 1]};
    r_poly(r, 4, lit(0xF2C230, lt * 0.85f), P_TWOSIDED, 0);
  }
  (void)c[2];
  /* the glow under it */
  V3 g[4] = {{p.x - 0.55f, p.y + 0.03f, p.z + 0.95f}, {p.x + 0.55f, p.y + 0.03f, p.z + 0.95f},
             {p.x + 0.55f, p.y + 0.03f, p.z - 0.95f}, {p.x - 0.55f, p.y + 0.03f, p.z - 0.95f}};
  r_poly(g, 4, 0x7FE8FF, P_ALPHA | P_NOHAZE, 90);
}

/* a cylinder along the matrix's y from y0 to y1 (the jetpack's tanks) */
static void cylinder(const M34 *w, float x, float z, float r, float y0, float y1, uint32_t col, float lt) {
  V3 ring0[8], ring1[8];
  for (int i = 0; i < 8; i++) {
    float s, c;
    r_sincos((float)i * 0.785398f, &s, &c);
    ring0[i] = ap(w, (V3){x + r * s, y0, z + r * c});
    ring1[i] = ap(w, (V3){x + r * s, y1, z + r * c});
  }
  for (int i = 0; i < 8; i++) {
    int j = (i + 1) & 7;
    V3 q[4] = {ring1[i], ring1[j], ring0[j], ring0[i]};
    float s, c;
    r_sincos(((float)i + 0.5f) * 0.785398f, &s, &c);
    V3 n = ap(w, (V3){s, 0, c});
    V3 o = ap(w, (V3){0, 0, 0});
    n = (V3){n.x - o.x, n.y - o.y, n.z - o.z};
    r_poly(q, 4, lit(r_shade(col, n), lt), P_TWOSIDED, 0);
  }
  V3 cap[8];
  for (int i = 0; i < 8; i++) cap[i] = ring1[7 - i];
  r_poly(cap, 8, lit(col, lt), P_TWOSIDED, 0);
}

void jetpack_draw(V3 at, const Pose *ps, float t, float lt) {
  (void)at, (void)ps;
  /* on the chest's back (the bones of the person just drawn) */
  const M34 *w = &bone_w[BN_CHEST];
  const Model *m = drawn ? drawn : &model_jake;
  float y0 = m->joints[BN_CHEST][1] * 0.001f;
  for (int s = -1; s <= 1; s += 2) {
    float x = 0.1f * (float)s;
    cylinder(w, x, -0.2f, 0.085f, y0 + 0.02f, y0 + 0.36f, 0x5BBF2A, lt);
    cylinder(w, x, -0.2f, 0.07f, y0 + 0.36f, y0 + 0.46f, 0xE8582A, lt);
    cylinder(w, x, -0.2f, 0.06f, y0 - 0.04f, y0 + 0.02f, 0x6A6E74, lt);
    /* the flame */
    float len = 0.4f + 0.14f * fsin(t * 37 + (float)s);
    V3 n = ap(w, (V3){x, y0 - 0.05f, -0.2f});
    V3 f[3] = {{n.x - 0.08f, n.y, n.z - 0.02f}, {n.x + 0.08f, n.y, n.z - 0.02f}, {n.x, n.y - len, n.z - 0.15f}};
    r_poly(f, 3, 0xFF9A1A, P_ALPHA | P_NOHAZE | P_TWOSIDED, 220);
    V3 f2[3] = {{n.x - 0.04f, n.y, n.z - 0.03f}, {n.x + 0.04f, n.y, n.z - 0.03f}, {n.x, n.y - len * 0.6f, n.z - 0.12f}};
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
/* the game's picture of it, turning round (as wide as the cosine of its turn) */
static void spinner(V3 c, float w, float h, float turn, int tex, float lt) {
  float s, co;
  r_sincos(turn, &s, &co);
  float hw = w * co;
  if (hw > -0.04f && hw < 0.04f) hw = hw < 0 ? -0.04f : 0.04f;
  V3 q[4] = {{c.x - hw, c.y + h, c.z}, {c.x + hw, c.y + h, c.z}, {c.x + hw, c.y - h, c.z}, {c.x - hw, c.y - h, c.z}};
  float uv[8] = {0, 0, 32, 0, 32, 32, 0, 32};
  int l = (int)(lt * 256 * (0.82f + 0.18f * (co < 0 ? -co : co)));
  r_poly_tex(q, uv, 4, tex, l, P_TWOSIDED | P_NOHAZE);
}

void actors_pickup(const Item *it, float t) {
  float bobv = 0.12f * fsin(t * 3 + it->z);
  float lt = scene_light(it->z);
  V3 c = {it->x, it->y + bobv, it->z};
  float turn = t * 2.2f + it->z;
  switch (it->kind) {
    case I_MAGNET: spinner(c, 0.45f, 0.48f, turn, TX_MAGNET, lt); break;
    case I_JETPACK: spinner(c, 0.45f, 0.5f, turn, TX_JETPACK, lt); break;
    case I_SNEAKERS: spinner(c, 0.45f, 0.45f, turn, TX_SNEAKERS, lt); break;
    case I_DOUBLE: spinner(c, 0.5f, 0.5f, turn, TX_DOUBLE, lt); break;
    case I_MYSTERY: spinner(c, 0.5f, 0.5f, turn * 0.5f, TX_MYSTERY, lt); break;
    case I_KEY: spinner(c, 0.36f, 0.45f, turn, TX_KEY, lt); break;
  }
  /* a soft glow on the ground under it */
  float gy = world_ground(it->x, it->z, it->y) + 0.02f;
  V3 g[4] = {{it->x - 0.6f, gy, it->z + 0.6f}, {it->x + 0.6f, gy, it->z + 0.6f}, {it->x + 0.6f, gy, it->z - 0.6f}, {it->x - 0.6f, gy, it->z - 0.6f}};
  r_poly(g, 4, it->kind == I_KEY ? 0x7FD0FF : 0xFFE680, P_ALPHA | P_NOHAZE, 70);
}
