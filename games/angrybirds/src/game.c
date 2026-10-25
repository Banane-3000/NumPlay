/* A level (game.h), with the original's rules (gamesrc/angrybirds/spec/ has them with their numbers):
 *
 * - Damage. When two objects start touching, each hurts the other: the momentum of one minus the other's (mass
 *   times velocity), over 10, times what its material does to the other's. Points: 10 for each point of health
 *   taken from an object worth points, and its score when it breaks. An object's defense is taken off each blow
 *   (a blow smaller than it does nothing); unbreakable materials have none. Nothing is hurt in the first 8 steps,
 *   nor when both are still. Birds lose health but never break; something that breaks under a bird lets it go on
 *   with part of its speed.
 * - Health: the object's own, or its area times its material's strength (less for long thin ones).
 * - Explosions push everything near them away from their centre and hurt what is nearer still.
 * - The level ends when everything is still, or 5 s after the last damage (30 s after a launch at most): won with
 *   no pig left (then 10000 points for each bird not used, one a second), lost with no bird left. */
#include <math.h>
#include <string.h>
#ifdef DEBUG_DAMAGE
#include <stdio.h>
#endif
#include "game.h"
#include "inflate.h"
#include "world.h"

Game g;

#define NONE_F 1e9f
#define AT_REST_LINEAR 0.1f                   /* the game's sleep tolerances: 0.1 m/s and 2 degrees a second */
#define AT_REST_ANGULAR 0.03490658849477768f

uint32_t game_rand(void) {
  uint32_t x = g.rng;
  x ^= x << 13, x ^= x >> 17, x ^= x << 5;
  return g.rng = x ? x : 0x2545F491u;
}
static float frand(void) { return (game_rand() >> 8) * (1.0f / 16777216.0f); }

/* ------------------------------------------------------------------ what the object is */
static inline const Type *tp(int i) { return res_type(g.obj[i].type); }
static inline const Material *mat(int i) { return res_material(tp(i)->material); }
static inline int cls(int i) { return g.obj[i].cls; }
static inline bool alive(int i) { return i >= 0 && i < g.count && (g.obj[i].flags & O_ALIVE); }
static inline bool bird(int i) { return cls(i) >= C_RED && cls(i) <= C_EAGLE; }
static inline bool pig(int i) { return cls(i) == C_PIG; }
static inline bool waiting(int i) { return (g.obj[i].flags & O_WAITING) != 0; }
static inline bool on_sling(int i) { return (g.obj[i].flags & O_ON_SLING) != 0; }

static Bird *bird_state(int i) {
  for (int k = 0; k < BIRD_CAP; k++)
    if (g.bird[k].obj == i) return &g.bird[k];
  return NULL;
}
static Bird *bird_new(int i) {
  for (int k = 0; k < BIRD_CAP; k++)
    if (g.bird[k].obj < 0) {
      Bird *b = &g.bird[k];
      memset(b, 0, sizeof *b);
      b->obj = (int16_t)i, b->after_hit = 50, b->steer_x = b->steer_y = NONE_F;
      return b;
    }
  return NULL;
}

/* the original's yes/no questions about an object, by its class (Mrb and the classes over it) */
static bool q_ae(int i) { return bird(i); }                      /* birds don't hurt birds */
static bool q_fe(int i) { return cls(i) != C_BLUE; }             /* split blue birds pass through each other */
static bool q_ge(int i) { return cls(i) != C_EAGLE; }            /* a golden egg only meets the eagle */
static bool q_ie(int i) { return cls(i) != C_GOLDEN; }           /* the eagle takes all but the golden egg */
static bool q_ke(int i) { return !bird(i); }                     /* the slingshot lets birds through */
static bool q_le(int i) { return cls(i) != C_EGG; }              /* the white bird and its egg don't meet */
static bool q_me(int i) { return cls(i) != C_WHITE; }
static bool q_hf(int i) { return cls(i) == C_EGG; }
static bool q_gf(int i) { return cls(i) == C_TRAMPOLINE; }
static bool q_cf(int i) { return cls(i) == C_SARDINE; }
static bool q_kf(int i) { return cls(i) == C_BALLOON || cls(i) == C_BOMBBALLOON; }
static int min_damage(int i) { return bird(i) || cls(i) == C_EGG ? 2 : 0; }   /* (_e) */

static float defense(int i) { return mat(i)->defense; }

/* still: (Ne) with the game's tolerance times `mult`; `strict` counts jointed things as still */
static bool still(int i, float mult, bool strict) {
  if (cls(i) == C_BLACK) return waiting(i);
  if (cls(i) == C_EGG) return false;
  if (world_is_static(i) || waiting(i)) return true;
  if (strict && world_jointed(i)) return true;
  if (!world_awake(i) && !q_cf(i)) return true;
  WBody b;
  world_get(i, &b);
  if (fabsf(b.vx) < AT_REST_LINEAR * mult && fabsf(b.vy) < AT_REST_LINEAR * mult && fabsf(b.w) < AT_REST_ANGULAR * mult)
    return true;
  return !(g.obj[i].flags & O_MOVED);   /* unmoved over the last step */
}

/* ------------------------------------------------------------------ points, damage, breaking */
static void puff_add(int kind, float x, float y, int32_t points) {
  int best = 0;
  for (int k = 0; k < PUFF_CAP; k++) {
    if (g.puff[k].t <= 0) { best = k; break; }
    if (g.puff[k].t < g.puff[best].t) best = k;
  }
  Puff *p = &g.puff[best];
  p->x = x, p->y = y, p->points = points, p->kind = (uint8_t)kind;
  p->t = kind == PUFF_POINTS ? 1000 : kind == PUFF_BOOM ? 400 : 500;
}

void game_object_pos(int i, float *x, float *y, float *angle) {
  WBody b;
  world_get(i, &b);
  *x = b.x, *y = b.y;
  if (angle) *angle = b.angle;
}

static void add_points(int32_t p) { g.score += (uint32_t)p; }

/* points for damage, and the level's clock kept going (Orb) */
static void scored(int i, bool count, int taken, bool show) {
  Obj *o = &g.obj[i];
  if (o->health < 0) o->health = 0;
  if (!count) return;
  if (g.settle_ms < 5000) g.settle_ms = 5000;
  int32_t p = tp(i)->score > 0 ? taken * 10 : 0;
  add_points(p);
  if (show && p > 0) {
    float x, y;
    game_object_pos(i, &x, &y, NULL);
    puff_add(PUFF_POINTS, x, y - 1, p);
  }
}

static void black_fuse(int i) {   /* (cCb.Af) */
  Bird *b = bird_state(i);
  if (b && b->fuse_at <= 0 && !(g.obj[i].flags & O_USED)) {
    b->fuse_at = 2000, b->fuse = 0;
    g.obj[i].flags |= O_USED | O_HIT;
  }
}

/* an object takes `a` damage (isb); count: points; full: no defense taken off (the eagle); its health after */
static int32_t hurt(int i, float a, bool count, bool show, bool full) {
  Obj *o = &g.obj[i];
  if (cls(i) == C_EAGLE) return o->health;
  if (a < (float)min_damage(i)) a = (float)min_damage(i);
  int32_t e = (int32_t)lroundf(a);
  float c = defense(i);
  if (a >= c && e < 1) e = 1;
  if (c < 0 && !full) return o->health;
  if (waiting(i)) return o->health;
  if ((float)e < c) return o->health;
  if (!full) e = (int32_t)((float)e - c);
  if (e > o->health) e = o->health;
  o->health -= e;
  if (bird(i) && !full) {   /* (nBb.Re) birds don't break */
    if (o->health < 1) o->health = 1;
    if (cls(i) == C_BLACK) black_fuse(i);
  } else {
    scored(i, count, e, show);
  }
  return o->health;
}

/* pass-through: what breaks under an object lets it go on with part of its speed, after the step (P) */
static struct { int16_t obj; float vx, vy; } pass[16];
static int passes;

/* a hurts b as they meet (Nrb) */
static bool enabled(int a, int b, bool strict);
static void attack(int a, int b) {
  if (!enabled(a, b, true)) return;
  int ma = tp(a)->material, mb = tp(b)->material;
  float gmul = res_damage(ma, mb), vmul = res_velocity(ma, mb);
  WBody A, B;
  world_get(a, &A);
  world_get(b, &B);
  float massa = world_mass(a), massb = world_mass(b);
  float ex = massa * A.vx - massb * B.vx, ey = massa * A.vy - massb * B.vy;
  float d = gmul * sqrtf(ex * ex + ey * ey) / 10;
  int32_t before = g.obj[b].health > 0 ? g.obj[b].health : 0;
  if ((q_hf(a) && q_gf(b)) || (q_gf(a) && q_hf(b))) d = 0;
  int32_t k = hurt(b, d, true, bird(a), false);
#ifdef DEBUG_DAMAGE
  printf("step %u: %d(cls %d m %.2f v %.1f,%.1f) hits %d(cls %d m %.2f v %.1f,%.1f): d %.1f -> health %d/%d\n", g.steps, a, cls(a),
         massa, A.vx, A.vy, b, cls(b), massb, B.vx, B.vy, d, g.obj[b].health, g.obj[b].max_health);
#endif
  if (k <= 0 && d > 0 && passes < 16) {
    float keep = (d - (float)before) / d * vmul;
    if (keep > 1) keep = 1;
    pass[passes].obj = (int16_t)a, pass[passes].vx = A.vx * keep, pass[passes].vy = A.vy * keep;
    passes++;
  }
}

/* ------------------------------------------------------------------ contacts (the game's listener) */
static bool rest_test(int a, int b, bool strict) {   /* ($rb) */
  if (strict) {
    if (g.frames < 8) return false;
    if (still(a, 10, false) && still(b, 10, false)) return false;
  }
  return true;
}
static bool bird_test(int a, int b, bool strict) {   /* (pBb) */
  if (on_sling(a)) return false;
  if (cls(b) == C_SLINGSHOT) return false;
  if (waiting(b)) return false;
  if (strict && q_ae(b)) return false;
  return rest_test(a, b, strict);
}
/* whether a acts on b (tf, by a's class) */
static bool enabled(int a, int b, bool strict) {
  switch (cls(a)) {
    case C_BLUE:
      if (!q_fe(b) && g.obj[a].health == g.obj[a].max_health && g.obj[b].health == g.obj[b].max_health) return false;
      return bird_test(a, b, strict);
    case C_WHITE:
      if (!q_le(b)) return false;
      return bird_test(a, b, strict);
    case C_EAGLE:
      if (!q_ie(b)) return false;
      return bird_test(a, b, strict);
    case C_RED: case C_YELLOW: case C_BLACK: case C_GREEN: case C_REDBIG: case C_SARDINE:
      return bird_test(a, b, strict);
    case C_SLINGSHOT:
      if (!q_ke(b)) return false;
      return rest_test(a, b, strict);
    case C_EGG:
      if (!q_me(b)) return false;
      return rest_test(a, b, strict);
    case C_GOLDEN:
      if (q_ge(b)) return false;
      return rest_test(a, b, strict);
    default:
      return rest_test(a, b, strict);
  }
}

static void eagle_takes(int i) {   /* (FCb.Ee) */
  if (alive(i) && q_ie(i)) hurt(i, (float)g.obj[i].max_health * 2, true, true, true);
}

/* a meets b (Ee, by a's class) */
static void meet(int a, int b) {
  if (!alive(a) || !alive(b)) return;
  if (cls(a) == C_EAGLE) {
    eagle_takes(b);
    return;
  }
  if (cls(a) == C_SARDINE && g.eagle_ms == 2400 && !waiting(a) && b != 1) g.eagle_ms = 0;
  attack(a, b);
}

void game_contact_begin(int a, int b) {
  meet(a, b);
  meet(b, a);
}
bool game_contact_enabled(int a, int b) { return alive(a) && alive(b) && enabled(a, b, false); }

/* ------------------------------------------------------------------ objects coming and going */
static int place_object(int type, float x, float y, float angle_rad, int flags) {
  if (g.count >= OBJ_CAP) return -1;
  int i = g.count;
  Obj *o = &g.obj[i];
  memset(o, 0, sizeof *o);
  o->type = (uint8_t)type, o->cls = res_type(type)->cls;
  o->flags = O_ALIVE;
  const Type *t = res_type(type);
  bool is_bird = o->cls >= C_RED && o->cls <= C_EAGLE;
  if (is_bird) {
    o->flags |= O_WAITING;   /* (a bird starts waiting) */
    flags |= W_BULLET | W_FIXED_ROTATION;
  }
  if (o->cls == C_SLINGSHOT) flags |= W_FIXED_ROTATION;
  if (!world_body(i, type, x, y, angle_rad, flags)) return -1;
  g.count++;
  /* health: its own, or its area (a static object's rough one) times its strength, less when long (esb, Urb) */
  float h = t->max_health;
  if (h <= 0) {
    const Shape *s = res_shape(t->shape);
    float area, ratio = 1;
    if (s->kind == 0) {
      area = PI_F * s->r * s->r;
    } else {
      float x0 = 0, x1 = 0, y0 = 0, y1 = 0;
      for (int k = 0; k < s->n; k++) {
        x0 = fminf(x0, s->v[2 * k]), x1 = fmaxf(x1, s->v[2 * k]);
        y0 = fminf(y0, s->v[2 * k + 1]), y1 = fmaxf(y1, s->v[2 * k + 1]);
      }
      ratio = (x1 - x0) / (y1 - y0);
      if (ratio < 1) ratio = 1 / ratio;
      if (world_is_static(i)) {
        area = (x1 - x0) * (y1 - y0);
        if (s->n == 3) area *= 0.5f;
        else if (s->n > 4) area *= 0.8f;
      } else {
        area = world_mass(i) / res_material(t->material)->density;
      }
    }
    float m = 1 - 0.5f * fminf(10, ratio - 1) / 10;
    h = area * m * res_material(t->material)->strength;
  }
  int32_t hp = (int32_t)lroundf(h);   /* (a whole number, 1 at least) */
  o->health = o->max_health = hp < 1 ? 1 : hp;
  WBody b;
  world_get(i, &b);
  o->jx = b.x, o->jy = b.y, o->ja = b.angle;
  if (is_bird) {
    Bird *bs = bird_new(i);
    if (bs) bs->home_x = x;
  }
  if (o->cls == C_BALLOON || o->cls == C_BOMBBALLOON) world_set_velocity(i, 0, -5);
  return i;
}

static void blast_add(int kind, float x, float y) {
  for (int k = 0; k < BLAST_CAP; k++)
    if (g.blast[k].kind < 0) {
      g.blast[k].kind = (int8_t)kind, g.blast[k].wait = 0, g.blast[k].x = x, g.blast[k].y = y;
      return;
    }
}

static void count_up(void) {
  g.pigs = 0, g.birds_left = 0;
  for (int i = 0; i < g.count; i++) {
    if (!alive(i)) continue;
    if (pig(i)) g.pigs++;
    if (bird(i)) g.birds_left++;
  }
}

/* the eggs that open levels (Xe of the golden egg and the Easter special eggs) */
static void golden_broken(void) {
  for (int k = 0; k < GOLDEN_COUNT; k++)
    if (res_golden(k) == g.level) {
      g.golden = true;
      g.golden_level = res_episode_first(5) + k;
    }
}

/* removes object i (YIb): effects and points when `effects` */
static void remove_object(int i, bool effects) {
  if (!alive(i)) return;
  float x, y;
  game_object_pos(i, &x, &y, NULL);
  if (effects) {
    switch (cls(i)) {
      case C_TNT: blast_add(2, x, y); break;
      case C_EGG: blast_add(0, x, y); break;
      case C_IMPLOSION: blast_add(3, x, y); break;
      case C_GOLDEN: golden_broken(); break;
      case C_BOMBBALLOON: blast_add(2, x, y); break;
      case C_EASTEREGG: g.easter_special = res_unlock(g.level); break;
      default: break;
    }
    if (!bird(i)) {   /* (nBb.we: no points for birds) */
      add_points(tp(i)->score);
      if (tp(i)->score > 0) puff_add(PUFF_POINTS, x, y - 1, tp(i)->score);
    }
    puff_add(bird(i) ? PUFF_FEATHERS : PUFF_SMOKE, x, y, 0);
  }
  world_destroy(i);
  for (int k = 0; k < g.timers; k++)
    if (g.timer[k].trigger == i) g.timer[k].on = true;
  Bird *b = bird_state(i);
  if (b) b->obj = -1;
  g.obj[i].flags = 0;
  if (g.follow == i) g.follow = -1;
  if (g.flying == i) g.flying = -1;
  if (g.eagle == i) g.eagle = -1;
  count_up();
}

/* ------------------------------------------------------------------ explosions (lGb) */
static const struct { float push_r, hurt_r, push, hurt; } BLASTS[4] = {
  {10, 8, 500, 400},     /* the white bird's egg */
  {15, 5, 2000, 300},    /* the black bird */
  {10, 5, 1500, 150},    /* TNT (birds are not pushed) */
  {10, 1, -2000, 1},     /* Chrome Dimension's implosion */
};
static void explode(const Blast *x) {
  puff_add(PUFF_BOOM, x->x, x->y, 0);
  for (int i = 0; i < g.count; i++) {
    if (!alive(i) || waiting(i) || (g.obj[i].flags & O_SCENERY)) continue;
    if (x->kind == 2 && bird(i)) continue;
    WBody b;
    world_get(i, &b);
    float dx = b.x - x->x, dy = b.y - x->y, e = sqrtf(dx * dx + dy * dy);
    float div = e < 1 ? 1 : floorf(e);
    if (e <= BLASTS[x->kind].push_r && e > 0) {
      float o = BLASTS[x->kind].push / div;
      world_impulse(i, o * dx / e, o * dy / e, x->x, x->y);
    }
    if (e <= BLASTS[x->kind].hurt_r) hurt(i, BLASTS[x->kind].hurt / div, false, false, false);
  }
}

/* ------------------------------------------------------------------ loading */
static int32_t rd32le(const uint8_t *p) { return (int32_t)(p[0] | p[1] << 8 | p[2] << 16 | (uint32_t)p[3] << 24); }
static uint16_t rd16le(const uint8_t *p) { return (uint16_t)(p[0] | p[1] << 8); }
static float rdf(const uint8_t *p) {
  uint32_t v = (uint32_t)rd32le(p);
  float f;
  memcpy(&f, &v, 4);
  return f;
}
static float milli(const uint8_t *planes, int n, int i) {
  int32_t v = (int32_t)(planes[i] | planes[n + i] << 8 | planes[2 * n + i] << 16 | (uint32_t)planes[3 * n + i] << 24);
  return (float)v / 1000.0f;
}

bool game_load(int level, void *arena, int arena_size) {
  memset(&g, 0, sizeof g);
  g.level = level;
  g.rng = 0x9E3779B9u ^ (uint32_t)level * 2654435761u;
  for (int k = 0; k < BIRD_CAP; k++) g.bird[k].obj = -1;
  for (int k = 0; k < BLAST_CAP; k++) g.blast[k].kind = -1;
  g.loaded = g.flying = g.follow = g.eagle = -1;
  g.golden_level = g.easter_special = -1;
  g.eagle_ms = 2400;
  g.sardine_spin = 0.07f;
  g.angle = 0.7853982f, g.power = 1.0f;
  const LevelInfo *L = res_level(level);
  /* the level unpacked at the arena's top, out of the physics' way while it is built from it */
  int top = ((int)L->unpacked + 7) & ~7;
  if (L->unpacked > MAX_UNPACKED || top + 4096 > arena_size) return false;
  uint8_t *scratch = (uint8_t *)arena + arena_size - top;
  if (!inflate_raw(res_level_stream(level), L->packed, scratch, L->unpacked)) return false;
  const uint8_t *p = scratch;
  int n = rd16le(p), nj = rd16le(p + 2), nr = rd16le(p + 4), nb = p[6];
  if (n > MAX_OBJECTS || nj > MAX_JOINTS || nr > MAX_RECTS || nb > n) return false;
  const uint8_t *types = p + 8, *flags = types + n, *planes = flags + n;
  const uint8_t *joints = planes + 12 * n, *rects = joints + 42 * nj;
  if ((uint32_t)(rects + 20 * nr - scratch) != L->unpacked) return false;
  memcpy(g.cam, L->cam, sizeof g.cam);
  int shapes = 2 + nr;   /* the ground, the slingshot, the rectangles, and the objects that aren't scenery */
  for (int k = 0; k < n; k++) shapes += !(flags[k] & 1);
  if (!world_begin(arena, arena_size - top, shapes)) return false;
  /* the ground, the slingshot (above the first bird, as high as it sits on it), then blocks and birds */
  int bird0 = n - nb;
  float b0x = milli(planes, n, bird0), b0y = milli(planes + 4 * n, n, bird0);
  int bt = types[bird0];
  const Shape *bs = res_shape(res_type(bt)->shape);
  float q = bs->r;
  if (bt == T_BIRD_WHITE) q += 0.2f;
  if (bt == T_BIRD_GREEN || bt == T_BIRD_SARDINE) q += 0.45f;
  if (place_object(T_GROUND_HILLS, 0, 0, 0, 0) != 0) return false;
  g.sling_x = b0x, g.sling_y = b0y - 8.5f + q;
  if (place_object(T_SLINGSHOT, g.sling_x, g.sling_y, 0, 0) != 1) return false;
  g.first_block = 2;
  int scenery_owner[MATERIAL_COUNT];
  for (int k = 0; k < MATERIAL_COUNT; k++) scenery_owner[k] = -1;
  for (int k = 0; k < n; k++) {
    float x = milli(planes, n, k), y = milli(planes + 4 * n, n, k), a = milli(planes + 8 * n, n, k);
    float rad = (360.0f - fmodf(360.0f - fmodf(a, 360.0f), 360.0f)) / 57.29577951308232f;
    if (k >= bird0) rad = 0;
    if (flags[k] & 1) {
      /* scenery: drawn as placed, its body in the merged rectangles below */
      Obj *o = &g.obj[g.count];
      memset(o, 0, sizeof *o);
      o->type = types[k], o->cls = res_type(types[k])->cls;
      o->flags = O_ALIVE | O_SCENERY;
      o->jx = x, o->jy = y, o->ja = rad;
      o->health = o->max_health = 1;
      int m = res_type(types[k])->material;
      if (scenery_owner[m] < 0) scenery_owner[m] = g.count;
      g.count++;
      continue;
    }
    if (place_object(types[k], x, y, rad, 0) < 0) return false;
  }
  g.first_bird = g.first_block + bird0;
  g.birds = nb;
  for (int k = 0; k < nr; k++) {
    const uint8_t *r = rects + 20 * k;
    int m = r[0];
    if (m >= MATERIAL_COUNT || scenery_owner[m] < 0) return false;
    if (!world_rect(scenery_owner[m], m, rdf(r + 4), rdf(r + 8), rdf(r + 12), rdf(r + 16))) return false;
  }
  for (int k = 0; k < nj; k++) {
    const uint8_t *j = joints + 42 * k;
    int kind = j[0], f = j[1], a = g.first_block + rd16le(j + 2), b = g.first_block + rd16le(j + 4);
    if (a >= g.first_bird || b >= g.first_bird) continue;
    if (kind == 5) {
      if (g.timers < TIMER_CAP) {
        Timer *t = &g.timer[g.timers++];
        t->trigger = (int16_t)a, t->target = (int16_t)b, t->ms = (int32_t)rd32le(j + 38), t->at = 0, t->on = false;
      }
      continue;
    }
    if (!world_has(a) || !world_has(b)) continue;
    if (!world_joint(kind, a, b, f & 1, rdf(j + 6), rdf(j + 10), rdf(j + 14), rdf(j + 18), (f >> 1) & 1, (f >> 2) & 1,
                     rdf(j + 22), rdf(j + 26), rdf(j + 30), rdf(j + 34)))
      return false;
  }
  /* past these an object is gone (Wrb, LFb): 10 m past the views' sides, and 5 m past the level's objects */
  g.left = fminf(g.cam[0], g.cam[4]) - 10;
  g.right = fmaxf(g.cam[2], g.cam[6]) + 10;
  for (int k = 0; k < n; k++) {
    float x = milli(planes, n, k);
    g.left = fminf(g.left, x - 5), g.right = fmaxf(g.right, x + 5);
  }
  count_up();
  g.next_wait = 1;
  if (world_arena_failures()) return false;
  world_grow(arena_size);   /* (the unpacked level is no longer needed) */
  return true;
}

/* ------------------------------------------------------------------ the slingshot */
int game_bird_queue(int *out, int max) {
  int n = 0;
  for (int i = g.first_bird; i < g.count && n < max; i++)
    if (alive(i) && waiting(i) && i != g.loaded) out[n++] = i;
  return n;
}

static int next_bird(void) {   /* (hUb) the first waiting bird */
  for (int i = 0; i < g.count; i++)
    if (alive(i) && bird(i) && waiting(i)) return i;
  return -1;
}

static void load_next(void) {   /* (gUb) */
  int i = next_bird();
  if (i < 0) {
    g.loaded = -1;
    g.out_of_birds = true;
    return;
  }
  g.loaded = i;
  g.obj[i].flags |= O_ON_SLING;
  world_set_awake(i, false);
  g.load_ms = 1200;
  game_object_pos(i, &g.load_x, &g.load_y, NULL);
  g.ready = false;
  g.next_wait = 10000;
}

/* the pocket's pull for the aim, held off the slingshot's post as the game holds it (jUb) */
static void pull_for_aim(float *u, float *v) {
  float d = 5.0f * g.power;
  *u = -d * cosf(g.angle);
  *v = d * sinf(g.angle);
  if (g.loaded < 0) return;
  const Shape *s = res_shape(tp(g.loaded)->shape);
  float r = s->kind == 0 ? s->r : 0.5f;
  float pp = r * 0.9f, qq = s->oy4 / 4.0f / 20.0f;
  float j = 0.5f + pp;
  if (cls(g.loaded) == C_SARDINE) j *= 0.6f;
  float f = 3 - (pp + qq);
  if (fabsf(*u) < j && *v > f) {
    float gg = fabsf(*u) / j, ii = f / *v;
    gg = gg * (1 - ii) + ii;
    *v *= gg;
  }
}

void game_aim(float angle, float power) {
  if (angle < -1.3962634f) angle = -1.3962634f;   /* -80 to 80 degrees */
  if (angle > 1.3962634f) angle = 1.3962634f;
  if (power < 0.1f) power = 0.1f;
  if (power > 1.0f) power = 1.0f;
  g.angle = angle, g.power = power;
}

float game_launch_speed(int obj) { return cls(obj) == C_GREEN ? 50.0f : 46.25f; }

bool game_can_launch(void) { return g.state == ST_PLAY && g.loaded >= 0 && g.ready; }

static void trail_clear(void) {
  g.trail_count = 0, g.trail_last = 0;
}

bool game_launch(void) {   /* (mUb, released) */
  if (!game_can_launch()) return false;
  int i = g.loaded;
  float u, v;
  pull_for_aim(&u, &v);
  float d = sqrtf(u * u + v * v);
  if (d < 0.01f) return false;
  float speed = game_launch_speed(i) * d / 5;
  float vx = speed * -u / d, vy = speed * -v / d;
  float x = g.sling_x + u, y = g.sling_y + v;
  g.settle_ms = 30000;
  /* the bird is made anew where it is, now turning, and sent off */
  world_destroy(i);
  if (!world_body(i, g.obj[i].type, x, y, 0, W_BULLET | W_FIXED_ROTATION)) {
    g.obj[i].flags = 0;
    count_up();
    return false;
  }
  world_free_rotation(i);
  world_set_velocity(i, vx, vy);
  g.obj[i].flags &= (uint16_t)~(O_ON_SLING | O_WAITING);
  g.obj[i].jx = x, g.obj[i].jy = y, g.obj[i].ja = 0;
  g.obj[i].flags |= O_MOVED;
  Bird *b = bird_state(i);
  if (b) b->yell = 800;
  g.flying = i;
  g.power_request = 0;
  g.loaded = -1;
  g.ready = false;
  g.next_wait = 1000;
  g.follow = -1;
  trail_clear();
  g.trail_x = x, g.trail_y = y;
  return true;
}

/* the flying bird is still the player's (uf): unhurt, or hurt less than 50 ms ago; the black one until it goes off */
static bool usable(int i) {
  if (!alive(i) || !bird(i)) return false;
  if (cls(i) == C_BLACK) return true;
  Bird *b = bird_state(i);
  return g.obj[i].health == g.obj[i].max_health || (b && b->after_hit > 0);
}

bool game_in_flight(void) {
  int i = g.flying;
  if (!usable(i)) return false;
  return cls(i) == C_BLACK || !(g.obj[i].flags & O_USED);
}

bool game_power(void) {
  if (!game_in_flight()) return false;
  g.power_request = 1;
  return true;
}

/* the speed made `add` faster, the same way (_rb) */
static void faster(int i, float add) {
  WBody b;
  world_get(i, &b);
  float d = sqrtf(b.vx * b.vx + b.vy * b.vy);
  if (d <= 0) return;
  float c = 1 + add / d;
  world_set_velocity(i, b.vx * c, b.vy * c);
}

/* the bird's power (wf, Bf) */
static void use_power(int i) {
  Obj *o = &g.obj[i];
  Bird *bs = bird_state(i);
  if ((o->flags & O_USED) && cls(i) != C_BLACK) return;
  WBody b;
  world_get(i, &b);
  switch (cls(i)) {
    case C_YELLOW: faster(i, 46.25f); break;
    case C_BLUE: {
      float a = sqrtf(b.vx * b.vx + b.vy * b.vy);
      if (a <= 0) break;
      float e = -b.vx / a, d = b.vy / a;
      for (int s = -1; s <= 1; s += 2) {
        int k = place_object(o->type, b.x + s * d, b.y + s * e, 0, W_BULLET | W_FIXED_ROTATION);
        if (k < 0) continue;
        g.obj[k].flags &= (uint16_t)~O_WAITING;
        g.obj[k].flags |= O_USED;
        world_free_rotation(k);
        world_set_velocity(k, b.vx + s * 7 * d, b.vy + s * 7 * e);
      }
      break;
    }
    case C_BLACK:
      if (bs && (bs->fuse_at <= 0 || bs->fuse_at > 100)) {
        bs->fuse = bs->fuse_at > 0 ? (int16_t)(100 * bs->fuse / bs->fuse_at) : 0;
        bs->fuse_at = 100;
      }
      break;
    case C_WHITE: {
      int k = place_object(T_MISC_FOOD_EGG, b.x, b.y + 1, 0, 0);
      if (k >= 0) world_set_velocity(k, 0, 100);
      world_impulse(i, 700, -1400, b.x - 0.5f, b.y);
      g.follow = i;
      if (k >= 0) g.flying = k;
      break;
    }
    case C_GREEN:
      if (bs) {
        bs->steer_x = fmaxf(-50, fminf(50, -b.vx * 1.5f));
        bs->steer_y = 1;
      }
      break;
    default: break;
  }
  o->flags |= O_USED;
}

/* ------------------------------------------------------------------ the Mighty Eagle */
bool game_eagle_arm(void) {   /* (EGb) every bird still waiting goes; the sardine jumps onto the slingshot */
  if (g.eagle_used || g.state != ST_PLAY) return false;
  for (int i = 0; i < g.count; i++)
    if (alive(i) && bird(i) && (waiting(i) || on_sling(i))) remove_object(i, true);
  int k = place_object(T_BIRD_SARDINE, g.sling_x, g.sling_y + 0.4f, 0, 0);
  if (k < 0) return false;
  g.eagle_used = true;
  g.eagle_armed = true;
  g.out_of_birds = false;
  g.loaded = -1;
  g.ready = false;
  g.next_wait = 1;
  for (int i = 0; i < g.count; i++)
    if (alive(i) && cls(i) == C_EASTEREGG) remove_object(i, false);
  return true;
}

static void eagle_query(int obj, void *ctx) {
  (void)ctx;
  eagle_takes(obj);
}

static void eagle_fly(int i, int dt) {   /* (FCb.Ue) */
  WBody b;
  world_get(i, &b);
  const Shape *s = res_shape(tp(i)->shape);
  world_query(b.x - s->r, b.y - s->r, b.x + s->r, b.y + s->r, eagle_query, NULL);
  g.eagle_ms += dt;
  float dir = 1;
  if (g.eagle_turned) {
    float d = fmodf(360.0f - fmodf(b.angle * 180.0f / PI_F, 360.0f), 360.0f);
    d -= 8.0f * dt * 360 / 1000;
    float rad = (360.0f - fmodf(d, 360.0f)) / 57.29577951308232f;
    world_set_transform(i, b.x, b.y, rad);
    world_get(i, &b);
    dir = -1;
    if (!g.eagle_shook) {
      g.eagle_shook = true;
      for (int k = 0; k < g.count; k++)
        if (alive(k) && pig(k)) world_set_velocity(k, 0, -18);
      for (int k = 0; k < g.count; k++)
        if (alive(k) && q_kf(k) && world_jointed(k)) remove_object(k, true);
      world_break_joints();
    }
    if (g.eagle_ms > 6000 && g.pigs) {
      for (int k = 0; k < g.count; k++)
        if (alive(k) && pig(k)) hurt(k, (float)g.obj[k].max_health * 2, true, true, true);
    }
  } else if (b.y >= -s->r) {
    g.eagle_turned = true;
  }
  /* 128 m/s across, sin(6.688) of that up or down */
  float step = dt * 0.128f;
  world_set_transform(i, b.x + step, b.y + dir * 0.39398f * step, b.angle);
}

static void sardine_tick(int i, int dt) {   /* (_Cb.We and sf) */
  if (g.eagle_out) return;
  if (g.obj[i].health == g.obj[i].max_health && !waiting(i) && !on_sling(i)) {
    if (g.sardine_spin < 0.308f) g.sardine_spin += dt * 0.0002f;
    world_angular_impulse(i, dt * g.sardine_spin);
  }
  if (g.eagle_ms < 2400) {
    int f = g.eagle_ms + dt;
    if (f >= 2400) {
      WBody b;
      world_get(i, &b);
      int k = place_object(T_BIRD_ME, b.x - 140, b.y - 140 * 0.39398f * 1.07f, -27.84f / 57.29577951308232f, 0);
      if (k >= 0) {
        g.obj[k].flags &= (uint16_t)~O_WAITING;
        g.eagle = k;
      }
      g.eagle_out = true;
    }
    g.eagle_ms = f;
  }
}

/* ------------------------------------------------------------------ the step */
static bool gone(int i) {   /* (Wrb) */
  if (world_is_static(i)) return false;
  WBody b;
  world_get(i, &b);
  return b.x < g.left || b.x > g.right || b.y < -192 || b.y > 10;
}

/* a bird is done when it has been still for 2 s, or the level's clock has run out (nBb.Ce) */
static bool bird_done(int i, int dt) {
  if (on_sling(i) || waiting(i)) return false;
  if (cls(i) == C_SARDINE || cls(i) == C_EAGLE) return false;
  if (g.settle_ms <= 0) return true;
  Bird *b = bird_state(i);
  if (!b) return true;
  if (still(i, 5, true)) b->rest = (int16_t)(b->rest + dt > 30000 ? 30000 : b->rest + dt);
  else {
    b->rest = 0;
    return false;
  }
  if (b->fuse_at > 0) return false;
  return b->rest > 2000;
}

static void waiting_bird(int i, int dt) {   /* (nBb.Se) a waiting bird stays in place and hops now and then */
  Bird *b = bird_state(i);
  if (!b || on_sling(i)) return;
  WBody s;
  world_get(i, &s);
  if (!(g.obj[i].flags & O_MOVED)) world_set_awake(i, false);
  world_set_velocity(i, 0, s.vy);
  world_set_transform(i, b->home_x, s.y, s.angle);
  if (g.state == ST_PLAY && !world_awake(i) && frand() * 1000 < dt) {
    float m = world_mass(i);
    world_impulse(i, 0, -(6 + frand() * 2) * m, s.x, s.y);
  }
}

static void object_tick(int i, int dt) {   /* (vf, csb) */
  Obj *o = &g.obj[i];
  Bird *b = bird(i) ? bird_state(i) : NULL;
  if (gone(i)) {
    if (cls(i) == C_SARDINE) {
      if (g.eagle_ms == 2400) g.eagle_ms = 0;
    } else {
      o->flags |= O_DONE;
    }
  } else if (b && bird_done(i, dt)) {
    o->flags |= O_DONE;
  }
  if (b && b->fuse_at > 0) {
    b->fuse += (int16_t)dt;
    if (b->fuse >= b->fuse_at) {
      WBody s;
      world_get(i, &s);
      blast_add(1, s.x, s.y);
      o->flags |= O_DONE;
    }
  }
  if (b && o->health != o->max_health && b->after_hit > 0) b->after_hit = (int16_t)(b->after_hit - dt);
  if (o->flags & O_DONE) return;
  if (cls(i) == C_GREEN && o->health == o->max_health && !waiting(i) && !on_sling(i))
    world_angular_impulse(i, dt * ((o->flags & O_USED) ? 0.4f : 0.2f));
  if (cls(i) == C_EAGLE) eagle_fly(i, dt);
  if (cls(i) == C_SARDINE) {
    sardine_tick(i, dt);
    if (gone(i) && g.eagle_out) o->flags |= O_DONE;
  }
  if (b && waiting(i)) waiting_bird(i, dt);
  if (cls(i) == C_BALLOON || cls(i) == C_BOMBBALLOON) {
    WBody s;
    world_get(i, &s);
    if (s.vy > -5) {
      float m = world_mass(i);
      /* (the original pushes once each frame: twice a 33 ms step at 60 frames a second) */
      world_impulse(i, 0, 2 * -86 * m * W_STEP, s.x, s.y);
    }
  }
}

static void sling_tick(int dt) {   /* (mUb) */
  if (g.loaded < 0) {
    if (!g.out_of_birds) {
      if (g.next_wait > 0) g.next_wait -= dt;
      if (g.next_wait <= 0) load_next();
    }
    return;
  }
  int i = g.loaded;
  if (!alive(i)) {
    g.loaded = -1;
    return;
  }
  float u, v;
  pull_for_aim(&u, &v);
  float tx = g.sling_x - 0.1f, ty = g.sling_y + 0.01f;   /* the pocket at rest */
  if (g.ready) {
    tx = g.sling_x + u, ty = g.sling_y + v;
    world_set_transform(i, tx, ty, 0);
    return;
  }
  g.load_ms -= dt;
  if (g.load_ms < 0) g.load_ms = 0;
  float o = (float)g.load_ms;
  float x = tx + o * (g.load_x - tx) / 1200, y = ty + o * (g.load_y - ty) / 1200 - sinf(o / 1200 * PI_F) * 5;
  WBody s;
  world_get(i, &s);
  world_set_transform(i, x, y, s.angle);
  if (g.load_ms >= 1200 - dt) {
    world_set_transform(i, x, y, 0);
    world_set_angular(i, 15);
  }
  world_get(i, &s);
  if (s.angle >= 2 * PI_F) {
    world_set_transform(i, x, y, 0);
    world_set_angular(i, 0);
  }
  if (g.load_ms <= 0) {
    world_set_angular(i, 15);
    world_set_transform(i, tx, ty, 0);
    g.ready = true;
  }
}

static void trail_tick(void) {   /* (KIb, bf) dots behind the bird, a meter apart */
  int i = g.flying >= 0 ? g.flying : g.follow;
  if (!alive(i) || !bird(i) || g.obj[i].health != g.obj[i].max_health) return;
  WBody b;
  world_get(i, &b);
  float dx = b.x - g.trail_x, dy = b.y - g.trail_y;
  if (dx * dx + dy * dy < 1) return;
  g.trail_x = b.x, g.trail_y = b.y;
  if (g.trail_count < TRAIL_CAP) {
    TrailDot *t = &g.trail[g.trail_count++];
    t->x = b.x, t->y = b.y, t->size = (uint8_t)(g.trail_last = (g.trail_last + 1) % 3);
  }
}

/* everything still (VIb); `birds`: and no launched bird left */
static bool all_still(bool birds_too) {
  if (g.settle_ms <= 0) return true;
  for (int i = 0; i < g.count; i++) {
    if (!alive(i) || (g.obj[i].flags & O_SCENERY)) continue;
    if (!still(i, 5, true)) return false;
    /* (a launched bird keeps the level going; the sardine and the eagle don't) */
    if (birds_too && bird(i) && !waiting(i) && cls(i) != C_SARDINE && cls(i) != C_EAGLE) return false;
  }
  return true;
}

static void end_tick(int dt) {   /* (the level screen's checks, once a second) */
  if (g.state == ST_WON || g.state == ST_LOST) return;
  if (g.state == ST_BONUS) {
    g.bonus_ms -= dt;
    if (g.bonus_ms <= 0) {
      /* (HIb, nBb.lf) a waiting bird hops up for 10000 points, one a second */
      int k = -1;
      for (int i = 0; i < g.count; i++)
        if (alive(i) && bird(i) && waiting(i)) { k = i; break; }
      if (k >= 0) {
        float x, y;
        game_object_pos(k, &x, &y, NULL);
        add_points(10000);
        puff_add(PUFF_POINTS, x, y - 2, 10000);
        g.obj[k].flags &= (uint16_t)~O_WAITING;
        g.obj[k].flags |= O_DONE;
        g.bonus_ms = 1000;
      } else {
        g.state = ST_WON;
      }
    }
    return;
  }
  if (g.end_ms > 0) g.end_ms += dt;
  g.check_ms += dt;
  if (g.check_ms <= 1000) return;
  g.check_ms = 0;
  bool still_all = all_still(true);
  bool quick = g.pigs == 0 && !g.eagle_out && all_still(false);
  if (!still_all && !quick) return;
  if (g.pigs == 0 && !(g.eagle_out && g.eagle_ms < 4400)) {
    if (!quick && g.end_ms < 1000) {
      if (g.end_ms == 0) g.end_ms = 1;
    } else {
      g.state = ST_BONUS;
      g.bonus_ms = 1000;
    }
  } else if (g.birds_left == 0 && g.loaded < 0) {
    if (g.end_ms < 2000) {
      if (g.end_ms == 0) g.end_ms = 1;
    } else {
      g.state = ST_LOST;
    }
  }
}

void game_step(void) {
  int dt = STEP_MS;
  g.steps++;
  passes = 0;
  world_step();
  /* after the step (cJb): broken things go, speeds kept through what broke, the green bird turns */
  if (g.frames < 8) g.frames++;
  for (int i = 0; i < g.count; i++) {
    if (!alive(i) || (g.obj[i].flags & O_SCENERY)) continue;
    Obj *o = &g.obj[i];
    if (o->health <= 0) {
      remove_object(i, true);
      continue;
    }
    for (int k = 0; k < passes; k++)
      if (pass[k].obj == i) world_set_velocity(i, pass[k].vx, pass[k].vy);
    Bird *b = cls(i) == C_GREEN ? bird_state(i) : NULL;
    if (b && o->health == o->max_health && (b->steer_x != NONE_F || b->steer_y != NONE_F)) {
      WBody s;
      world_get(i, &s);
      float vx = s.vx, vy = s.vy;
      if (b->steer_x != NONE_F) {
        if (b->steer_x < vx) vx = fmaxf(b->steer_x, vx - 2.4749999f);
        else if (b->steer_x > vx) vx = fminf(b->steer_x, vx + 2.4749999f);
      }
      if (b->steer_y != NONE_F) {
        float d = b->steer_y < 0 ? 2.4749999f : 0.495f;
        if (b->steer_y < vy) vy = fmaxf(b->steer_y, vy - d);
      }
      world_set_velocity(i, vx, vy);
    }
    /* where it is now, and whether it moved in the step (J <- A, A <- now) */
    WBody now;
    world_get(i, &now);
    if (now.x != o->jx || now.y != o->jy || now.angle != o->ja) o->flags |= O_MOVED;
    else o->flags &= (uint16_t)~O_MOVED;
    o->jx = now.x, o->jy = now.y, o->ja = now.angle;
  }
  /* the game's rules for this step's time (fJb) */
  for (int i = 0; i < g.count; i++) {
    if (!alive(i) || (g.obj[i].flags & O_SCENERY)) continue;
    object_tick(i, dt);
    if (g.obj[i].flags & O_DONE) remove_object(i, true);
  }
  /* the flying bird: its power, or done (hJb) */
  if (g.flying >= 0) {
    if (!usable(g.flying)) {
      g.flying = -1;
      g.power_request = 0;
    } else if (g.power_request == 1) {
      use_power(g.flying);
      g.power_request = 2;
    } else {
      trail_tick();
    }
  } else if (g.follow >= 0) {
    trail_tick();
  }
  for (int k = 0; k < BLAST_CAP; k++)
    if (g.blast[k].kind >= 0) {
      Blast x = g.blast[k];
      g.blast[k].kind = -1;
      explode(&x);
    }
  for (int k = 0; k < g.timers; k++) {
    Timer *t = &g.timer[k];
    if (!t->on) continue;
    t->at += dt;
    if (t->at >= t->ms) {
      t->on = false;
      if (alive(t->target)) remove_object(t->target, true);
    }
  }
  if (g.settle_ms > 0) g.settle_ms -= dt;
  sling_tick(dt);
  for (int k = 0; k < PUFF_CAP; k++)
    if (g.puff[k].t > 0) g.puff[k].t = (int16_t)(g.puff[k].t - dt);
  count_up();
  end_tick(dt);
}

/* ------------------------------------------------------------------ stars */
int game_stars(int level, uint32_t score) {
  const LevelInfo *L = res_level(level);
  return score >= L->gold ? 3 : score >= L->silver ? 2 : score > 0 ? 1 : 0;
}

int game_eagle_percent(int level, uint32_t score) {
  const LevelInfo *L = res_level(level);
  if (!L->eagle) return 100;
  int p = (int)lroundf((float)score / (float)L->eagle * 100);
  return p > 100 ? 100 : p;
}
