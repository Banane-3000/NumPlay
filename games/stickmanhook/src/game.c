/* The game: Stickman Hook's level scene (bundle `Ms`), its stickman (`Aa`), bumpers (`vs`) and walls, the
 * camera and the finish, step for step. Numbers and order as in the web game; see the spec. */
#include <math.h>
#include <string.h>
#include "game.h"

#define DT (1.0f / 60.0f)
#define SUBSTEPS 4        /* Rs */
#define SLOW 0.2f         /* Ps: the scene's time after the finish */
#define PULL 0.2f         /* seconds the rope pulls in after hooking */
#define TEX 0.0005f       /* Sc.SCALE: a texture pixel */

static float clampf(float x, float a, float b) { return x < a ? a : x > b ? b : x; }

uint32_t sh_rand(uint32_t *rng) {
  uint32_t x = *rng ? *rng : 0x9E3779B9u;
  x ^= x << 13, x ^= x >> 17, x ^= x << 5;
  return *rng = x;
}
float sh_random(uint32_t *rng) { return (float)(sh_rand(rng) >> 8) * (1.0f / 16777216.0f); }
static uint8_t draw_one(uint32_t *rng, int first, int n) {
  int k = (int)(sh_random(rng) * (float)n);
  return (uint8_t)(first + (k < n ? k : n - 1));
}

uint32_t sh_fake_max(uint32_t level) { return 5 * ((level + 1) / 5 + 1); }
/* a skin opens on reaching its level: as(), `condition.level <= level + 1`; the others are free */
bool sh_skin_open(uint32_t levels_done, int skin) {
  return skin >= 0 && skin < SH_SKINS && sh_unlock_level[skin] <= levels_done + 1;
}

/* Ac(): the introductory levels, then the 30 recipes, then a start, a middle and an end at random */
void sh_pick(uint32_t level, uint32_t *rng, sh_course *c) {
  if (level < SH_INTRO) {
    c->part[0] = (uint8_t)level, c->part[1] = c->part[2] = SH_NO_PART;
    c->background = sh_parts[level].background;
    return;
  }
  if (level < SH_FIXED) {
    memcpy(c->part, sh_recipes[level - SH_INTRO], 3);
  } else {
    c->part[0] = draw_one(rng, SH_FIRST_START, SH_STARTS);
    c->part[1] = draw_one(rng, SH_FIRST_MIDDLE, SH_MIDDLES);
    c->part[2] = draw_one(rng, SH_FIRST_END, SH_ENDS);
  }
  c->background = sh_random(rng) > 0.5f;   /* sc(): background_1 or background_0, half and half */
}

bool sh_course_ok(uint32_t level, const sh_course *c) {
  if (c->background > 1) return false;
  if (level < SH_INTRO)
    return c->part[0] == level && c->part[1] == SH_NO_PART && c->part[2] == SH_NO_PART &&
           c->background == sh_parts[level].background;
  if (level < SH_FIXED) return !memcmp(c->part, sh_recipes[level - SH_INTRO], 3);
  return c->part[0] >= SH_FIRST_START && c->part[0] < SH_FIRST_START + SH_STARTS &&
         c->part[1] >= SH_FIRST_MIDDLE && c->part[1] < SH_FIRST_MIDDLE + SH_MIDDLES &&
         c->part[2] >= SH_FIRST_END && c->part[2] < SH_FIRST_END + SH_ENDS;
}

int sh_hook_count(const sh_game *g) {
  int n = 0;
  for (int j = 0; j < g->sections; j++) n += sh_parts[g->course.part[j]].hooks;
  return n;
}
sh_vec sh_hook_at(const sh_game *g, int i) {
  for (int j = 0; j < g->sections; j++) {
    const sh_part *p = &sh_parts[g->course.part[j]];
    if (i < p->hooks) {
      sh_vec h = sh_hooks[p->hook + i];
      h.x += g->offset[j];
      return h;
    }
    i -= p->hooks;
  }
  return (sh_vec){0, 0};
}
int sh_bumper_count(const sh_game *g) {
  int n = 0;
  for (int j = 0; j < g->sections; j++) n += sh_parts[g->course.part[j]].bumpers;
  return n;
}

void sh_new(sh_game *g, uint32_t level, const sh_course *c) {
  memset(g, 0, sizeof *g);
  g->level = level, g->course = *c;
  for (int j = 0; j < 3 && c->part[j] != SH_NO_PART; j++) {
    g->offset[j] = g->finish;
    g->finish += sh_parts[c->part[j]].finish;
    g->sections++;
  }
  g->spawn_x = sh_parts[c->part[0]].spawn_x, g->spawn_y = sh_parts[c->part[0]].spawn_y;
  g->zoom = 1;
  sh_spawn(g);
}

/* spawn(): a new stickman at the start (the camera's lead is kept) */
void sh_spawn(sh_game *g) {
  g->x = g->spawn_x, g->y = g->spawn_y;
  g->vx = g->vy = g->theta = g->omega = g->r = g->pull = g->rotation = g->spin = 0;
  g->hooked = g->completed = g->ready = 0, g->pose = SH_SPAWN, g->facing = 0;
  g->progress = 0, g->complete_time = 0, g->zoom = 1, g->cam_y = 0;
  g->trail_count = 0, g->trail_max = 20, g->near = -1, g->spawn_fx = 1;
  if (g->attempt < UINT32_MAX) g->attempt++;
  g->cam_x = fminf(0, -(g->x + g->cam_offset) + SH_RATIO / 2);
}

static void hook(sh_game *g, sh_vec h) {
  float dx = g->x - h.x, dy = g->y - h.y, r = sqrtf(dx * dx + dy * dy);
  if (r < 0.001f) return;   /* (the game would divide by zero: on the hook itself, wait for gravity) */
  g->theta = atan2f(dx, dy), g->r = r;
  g->omega = (cosf(g->theta) * g->vx - sinf(g->theta) * g->vy) / r * 1.075f;
  g->hooked = 1, g->hook_x = h.x, g->hook_y = h.y, g->pull = PULL, g->pose = SH_HOOKED2;
}

static void unhook(sh_game *g) {
  if (!g->hooked) return;
  g->vx *= 1.2f, g->vy *= 1.2f, g->spin = 20 * g->vx;
  g->hooked = 0, g->pull = 0, g->pose = SH_BALL, g->facing = 0;
}

/* onHit: the pull stops; flying free, the stickman rolls into a ball */
static void hit(sh_game *g) {
  g->pull = 0;
  if (!g->hooked) g->pose = SH_BALL, g->spin = 20 * g->vx;
}

static void move(sh_game *g, float t) {
  if (g->hooked) {
    if (g->pull > 0) {
      g->r -= 0.3f * fminf(t, g->pull);
      if (g->r < 0.05f) g->r = 0.05f, g->pull = 0;
      g->pull -= t;   /* (ends just under 0: still "pulling" for the walls, as in the game) */
    }
    g->omega += -sinf(g->theta) / g->r * t;
    g->theta += g->omega * t;
    float sn = sinf(g->theta), cs = cosf(g->theta);
    g->x = g->hook_x + g->r * sn, g->y = g->hook_y + g->r * cs;
    g->rotation = -g->theta;
    g->vx = g->r * g->omega * cs, g->vy = -g->r * g->omega * sn;
    g->facing = g->omega <= 0;
    g->pose = fabsf(g->omega) < 0.5f ? SH_HOOKED0
              : (g->theta < 0 && g->omega < 0) || (g->theta > 0 && g->omega > 0) ? SH_HOOKED1 : SH_HOOKED2;
  } else {
    if (!g->completed) g->vy += t;   /* gravity (0, 1); none after the finish */
    g->x += g->vx * t, g->y += g->vy * t;
    g->rotation += g->spin * t;
  }
}

static void bumper(sh_game *g, int index, const sh_bumper *b, float offset, bool *ignore) {
  const sh_bumper_kind *k = &sh_bumper_kinds[b->kind];
  const float w = 0.15f, h = 0.04f;
  float dx = g->x - b->x - offset, dy = g->y - b->y, reach = SH_RADIUS + w;   /* its radius: sqrt(w * w) */
  if (dx * dx + dy * dy > reach * reach) return;
  float x = dx * k->c + dy * k->s, y = -dx * k->s + dy * k->c;
  if (fabsf(x) > w / 2 + SH_RADIUS || fabsf(y) > h / 2 + SH_RADIUS) return;
  hit(g);
  if (!g->bumper_anim[index]) g->bumper_anim[index] = 1;
  if (g->hooked) {
    if (*ignore) return;
    *ignore = true;
    g->theta -= g->omega / 60;
    if (k->force) g->omega = k->force * (g->omega > 0 ? -1 : 1) / g->r;
    else {
      g->omega *= -k->bounce;
      if (g->omega * g->r > 3) g->omega = 3 / g->r;
    }
    return;
  }
  float vx = g->vx * k->c + g->vy * k->s, vy = -g->vx * k->s + g->vy * k->c;
  bool across = fabsf(y) - h / 2 > fabsf(x) - w / 2;
  float *v = across ? &vy : &vx, *p = across ? &y : &x, half = (across ? h : w) / 2 + SH_RADIUS;
  float edge = *p > 0 ? half : -half;
  *v = k->force ? k->force * (*v > 0 ? -1 : 1) : -*v * k->bounce;
  *v = clampf(fabsf(*v), 0.1f, 3) * (*v > 0 ? 1 : -1);
  *p = 2 * edge - *p;
  g->vx = vx * k->c - vy * k->s, g->vy = vx * k->s + vy * k->c;
  if (k->aimed) {
    float speed = hypotf(g->vx, g->vy);
    g->vx = speed * cosf(k->aim), g->vy = speed * sinf(k->aim);
  }
  g->x = b->x + offset + x * k->c - y * k->s, g->y = b->y + x * k->s + y * k->c;
}

static void wall(sh_game *g, const sh_wall *o, float offset, bool *ignore) {
  const sh_shape *shape = &sh_shapes[o->shape];
  float dx = g->x - o->x - offset, dy = g->y - o->y, reach = shape->radius + SH_RADIUS;
  if (dx * dx + dy * dy > reach * reach) return;
  float cs = sh_turns[o->turn].x, sn = sh_turns[o->turn].y;
  float x = dx * cs + dy * sn, y = -dx * sn + dy * cs;
  for (int i = 0; i < shape->count; i++) {
    const sh_edge *e = &sh_edges[shape->first + i];
    float ax = x - e->x, ay = y - e->y, t;
    /* ws(): the segment's two ends first, then its inside */
    if (ax * ax + ay * ay <= SH_RADIUS * SH_RADIUS) t = 0;
    else if ((ax - e->dx) * (ax - e->dx) + (ay - e->dy) * (ay - e->dy) <= SH_RADIUS * SH_RADIUS) t = 1;
    else {
      t = (ax * e->dx + ay * e->dy) * e->inv;
      if (t < 0 || t > 1) continue;
    }
    float qx = e->x + e->dx * t, qy = e->y + e->dy * t, rx = x - qx, ry = y - qy, d = rx * rx + ry * ry;
    if (d > SH_RADIUS * SH_RADIUS) continue;
    float nx = e->nx, ny = e->ny;
    if (t == 0 || t == 1) {
      float l = sqrtf(d);
      if (l > 0.000001f) nx = -rx / l, ny = -ry / l;
    }
    bool pulling = g->pull != 0;   /* `!!hookPullRemaining`, before onHit clears it */
    hit(g);
    if (g->hooked) {
      *ignore = true;
      g->theta -= g->omega / 60;
      if (pulling) g->r += 0.01f;
      else g->omega *= -0.1f;
    } else {
      x = qx - nx * (SH_RADIUS + 0.0001f), y = qy - ny * (SH_RADIUS + 0.0001f);
      float vx = g->vx * cs + g->vy * sn, vy = -g->vx * sn + g->vy * cs, across = vx * nx + vy * ny;
      vx -= 1.1f * across * nx, vy -= 1.1f * across * ny;   /* the speed across the wall times -0.1 */
      g->vx = vx * cs - vy * sn, g->vy = vx * sn + vy * cs;
      g->x = o->x + offset + x * cs - y * sn, g->y = o->y + x * sn + y * cs;
      g->spin = 20 * g->vx;
    }
    return;
  }
}

static int closest(const sh_game *g) {
  float best = INFINITY;
  int found = -1, n = 0;
  for (int j = 0; j < g->sections; j++) {
    const sh_part *p = &sh_parts[g->course.part[j]];
    for (int i = 0; i < p->hooks; i++, n++) {
      float x = sh_hooks[p->hook + i].x + g->offset[j] - g->x, y = sh_hooks[p->hook + i].y - g->y, d = x * x + y * y;
      if (d < best) best = d, found = n;
    }
  }
  return found;
}

/* onComplete(): slow motion, no gravity, the victory pose, a longer trail, confetti */
static void finish(sh_game *g, uint32_t *rng) {
  g->completed = 1;
  unhook(g);
  g->spin = 0, g->pose = SH_VICTORY;
  g->rotation = atan2f(g->vy, g->vx) + 1.5707963f;
  g->trail_max = 60;
  for (int e = 0; e < SH_CONFETTI; e++) {
    sh_confetto *c = &g->confetti[e];
    float k = 50.0f * (float)(50 - e) * TEX;   /* texture pixels a second, in a container at scale SCALE */
    c->x = g->finish, c->y = g->y;
    c->vx = (g->vx + 0.3f * (sh_random(rng) - 0.5f)) * k;
    c->vy = (g->vy + 0.3f * (sh_random(rng) - 0.5f)) * k;
    c->w = 0.2f * sh_random(rng) * 126 * TEX, c->h = 0.2f * sh_random(rng) * 89 * TEX;   /* the confetti picture's size */
    c->angle = sh_random(rng);
    uint32_t tint = (uint32_t)(8947848.0f * sh_random(rng)) + 7829367u;
    c->color = (uint16_t)((tint >> 8 & 0xF800) | (tint >> 5 & 0x07E0) | (tint >> 3 & 0x1F));
  }
}

static float ease(float t) {   /* Cs(): ease out cubic over 0.6 s */
  float e = t / 0.6f - 1;
  return e * e * e + 1;
}

void sh_step(sh_game *g, bool held, uint32_t *rng) {
  if (!g->completed) {
    if (held && !g->held && !g->hooked && g->near >= 0) hook(g, sh_hook_at(g, g->near));
    if (!held && g->held) unhook(g);
  }
  g->held = held;
  float e = g->completed ? DT * SLOW : DT, t = e / SUBSTEPS;
  bool ignore = false;
  for (int s = 0; s < SUBSTEPS; s++) {
    move(g, t);
    for (int j = 0, n = 0; j < g->sections; j++) {
      const sh_part *p = &sh_parts[g->course.part[j]];
      for (int i = 0; i < p->bumpers; i++, n++) bumper(g, n, &sh_bumpers[p->bumper + i], g->offset[j], &ignore);
    }
    for (int j = 0; j < g->sections; j++) {
      const sh_part *p = &sh_parts[g->course.part[j]];
      for (int i = 0; i < p->walls; i++) wall(g, &sh_walls[p->wall + i], g->offset[j], &ignore);
    }
  }
  int near = closest(g);
  if (near != g->near) g->near = (int16_t)near, g->near_scale = 0.4f;
  g->near_scale = fminf(g->near_scale + 5 * e, 1);
  if (!g->completed && g->x > g->finish) finish(g, rng);
  if (!g->completed && (g->y > 1.15f || g->x < 0) && !g->hooked && g->vy > 0) sh_spawn(g);   /* onDead() */
  {
    /* updateCamera() */
    if (g->completed) {
      g->complete_time += DT;
      if (g->complete_time < 0.6f) {
        float k = ease(g->complete_time), from = -(g->zoom = 1 + 3 * k) / 2 + 0.5f;
        g->cam_y = (-g->y * g->zoom + 0.5f - from) * k + from;
      } else {
        g->cam_y = -g->y * g->zoom + 0.5f;
      }
      g->cam_x = fminf(0, (-g->x - 0.05f / g->zoom) * g->zoom + SH_RATIO / 2);
      if (g->complete_time >= 1.9999f) g->ready = 1;   /* gotoVictory, 2 s after */
      for (int i = 0; i < SH_CONFETTI; i++) g->confetti[i].x += g->confetti[i].vx * e, g->confetti[i].y += g->confetti[i].vy * e;
    } else {
      float lead = g->vx > 0.1f ? 0.2f : g->vx < -0.1f ? -0.2f : 0;
      g->cam_offset = clampf(g->cam_offset + lead * DT, -0.1f, 0.1f);
      g->cam_x = fminf(0, -(g->x + g->cam_offset) + SH_RATIO / 2);
    }
    float progress = (g->x - g->spawn_x) / (g->finish - g->spawn_x);
    if (progress > g->progress) g->progress = fminf(1, progress);
    /* the trail: a point a frame, the last trail_max kept (and the newest) */
    if (g->trail_count == g->trail_max + 1) {
      memmove(g->trail, g->trail + 1, sizeof g->trail[0] * g->trail_max);
      g->trail_count--;
    }
    g->trail[g->trail_count++] = (sh_vec){g->x, g->y};
  }
  /* the bumpers' bounce (5 frames at half a frame a tick) and the spawn burst (12 frames, the same) */
  for (int i = 0; i < SH_MAX_BUMPERS; i++)
    if (g->bumper_anim[i] && ++g->bumper_anim[i] > 10) g->bumper_anim[i] = 0;
  if (g->spawn_fx && ++g->spawn_fx > 24) g->spawn_fx = 0;
  g->ticks++;
}

bool sh_valid(const sh_game *g) {
  if (!sh_course_ok(g->level, &g->course) || g->sections < 1 || g->sections > 3) return false;
  if (g->trail_count > g->trail_max + 1 || g->trail_max > 60 || g->near >= sh_hook_count(g)) return false;
  const float f[] = {g->x, g->y, g->vx, g->vy, g->theta, g->omega, g->r, g->pull, g->rotation, g->spin, g->cam_x, g->cam_y};
  for (unsigned i = 0; i < sizeof f / sizeof f[0]; i++)
    if (!isfinite(f[i])) return false;
  return g->r >= 0 && g->zoom >= 1 && g->zoom <= 4.01f;
}

void sh_swing_start(sh_swing *s, float hx, float hy, float x, float y) {
  memset(s, 0, sizeof *s);
  s->hx = hx, s->hy = hy, s->x = x, s->y = y;
  s->theta = atan2f(x - hx, y - hy), s->r = sqrtf((x - hx) * (x - hx) + (y - hy) * (y - hy));
  s->pull = PULL, s->pose = SH_HOOKED2;
}

void sh_swing_step(sh_swing *s, float t) {
  if (s->pull > 0) {
    s->r -= 0.3f * fminf(t, s->pull);
    if (s->r < 0.05f) s->r = 0.05f, s->pull = 0;
    s->pull -= t;
  }
  s->omega += -sinf(s->theta) / s->r * t;
  s->theta += s->omega * t;
  s->x = s->hx + s->r * sinf(s->theta), s->y = s->hy + s->r * cosf(s->theta);
  s->facing = s->omega <= 0;
  s->pose = fabsf(s->omega) < 0.5f ? SH_HOOKED0
            : (s->theta < 0 && s->omega < 0) || (s->theta > 0 && s->omega > 0) ? SH_HOOKED1 : SH_HOOKED2;
}
