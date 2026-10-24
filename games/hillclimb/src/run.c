/* The run being driven, by the game's rules (its GameLayer and GameContentLayer): pedals, fuel, coins, flips, air
 * time, levels, the end; the camera and the picture of it all. */
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "art.h"
#include "game.h"
#include "gfx.h"

Run run;

#define SX 124      /* where the camera's point is on the screen */
#define SY 138
#define PI 3.14159265f
#define DT (1.0f / CAR_HZ)

/* ------------------------------------------------------------------ popups and particles */
static void popup_later(const char *text, uint32_t value, int big, int wait) {
  int slot = 0;
  for (int i = 0; i < NPOPUPS; i++)
    if (run.pop[i].t > run.pop[slot].t) slot = i;
  for (int i = 0; i < NPOPUPS; i++)
    if (!run.pop[i].text[0]) {
      slot = i;
      break;
    }
  Popup *p = &run.pop[slot];
  snprintf(p->text, sizeof p->text, "%s", text);
  p->value = value, p->t = 0, p->big = (uint8_t)big, p->seq = ++run.pop_seq, p->wait = (uint8_t)wait;
}
static void popup(const char *text, uint32_t value, int big) { popup_later(text, value, big, 0); }

/* the same popup again (air time going up): its value and its time renewed */
static void popup_update(const char *text, uint32_t value, int big) {
  for (int i = 0; i < NPOPUPS; i++)
    if (run.pop[i].text[0] && (!strcmp(run.pop[i].text, "AIR TIME") || !strcmp(run.pop[i].text, "BIG AIR TIME") ||
                               !strcmp(run.pop[i].text, "INSANE AIR TIME"))) {
      snprintf(run.pop[i].text, sizeof run.pop[i].text, "%s", text);
      run.pop[i].value = value, run.pop[i].big = (uint8_t)big;
      if (run.pop[i].t > 8) run.pop[i].t = 8;
      return;
    }
  popup(text, value, big);
}

static void particle(float x, float y, float vx, float vy, float life, float size, int kind) {
  Particle *p;
  if (run.npart < NPARTICLES) p = &run.part[run.npart++];
  else p = &run.part[run.frame % NPARTICLES];
  p->x = x, p->y = y, p->vx = vx, p->vy = vy, p->life = life, p->size = size, p->kind = (uint8_t)kind;
}

static uint32_t rnd_state = 1;
static float rndf(void) {
  rnd_state = rnd_state * 1664525u + 1013904223u;
  return (float)(rnd_state >> 8) * (1.0f / 16777216.0f);
}

/* ------------------------------------------------------------------ the run */
void run_start(int vehicle, int stg) {
  memset(&run, 0, sizeof run);
  stage_set(stg);
  stage_trees_reset();
  run.vehicle = vehicle, run.stage = stg;
  car_init(&run.car, vehicle, sv.level[vehicle]);
  run.fuel = 100;
  run.burn = 2.5f * car_fuel_rate(vehicle, sv.level[vehicle]);
  run.start_x = 0;
  run.best = sv.best[stg][vehicle];
  run.cam_x = run.car.ch.x, run.cam_y = run.car.ch.y, run.zoom = 40, run.cam_s = 22;
  rnd_state = 12345u + (uint32_t)stg;
  /* the stage's level goes on from the last run (StageInfo's level), and the run opens with its goal */
  run.level = sv.stage_level[stg];
  char t[32];
  snprintf(t, sizeof t, "LEVEL %d: REACH %um", run.level + 1, (unsigned)stage_level_goal(run.level));
  popup(t, 0, 0);
}

uint32_t run_total(void) { return run.coins + run.bonus; }

static void end_run(int why) {
  if (run.over) return;
  run.over = true, run.end = why, run.over_t = 0;
}

void run_finish(void) {
  car_free();
  if (run.dist > (float)sv.best[run.stage][run.vehicle]) {
    sv.best[run.stage][run.vehicle] = (uint16_t)(run.dist < 65535 ? run.dist : 65535);
    run.new_best = true;
  }
  uint32_t t = run_total();
  sv.coins = sv.coins + t < sv.coins ? 0xFFFFFFFFu : sv.coins + t;
  sv.coins_earned += t;
  sv.runs++;
  sv.meters += (uint32_t)run.dist;
  sv.flips += run.flips;
  if (run.air_best_ms > sv.best_air_ms) sv.best_air_ms = run.air_best_ms;
  save_write();
}

/* flips (PlayerCar::handleFlips, GameLayer::onFlip...) and air time (GameLayer's frame): paid as they happen */
static void tricks(void) {
  Car *c = &run.car;
  if (c->flip == FLIP_FRONT || c->flip == FLIP_BACK) {
    run.flips++;
    run.bonus += 1000;
    popup(c->flip == FLIP_BACK ? "BACK FLIP" : "FLIP", 1000, 1);
  } else if (c->flip == FLIP_NECK) {
    run.bonus += 2500;
    popup("NECK FLIP", 2500, 1);
  }
  /* air time counts from 0.75 s up, a step every 0.2 s: 25 each up to the 4th, 100 to the 8th, then 500;
   * not in the run's first 3 seconds */
  if (run.t < 3) return;
  float t = (float)c->air / CAR_HZ;
  if (t < 0.75f) {
    if (run.air_value) {
      uint32_t ms = (uint32_t)(run.air_best * 1000);
      if (ms > run.air_best_ms) run.air_best_ms = ms;
    }
    run.air_best = 0, run.air_step = 0, run.air_value = 0;
    return;
  }
  if (t > run.air_best) run.air_best = t;
  int n = (int)floorf(1 + (run.air_best - 0.75f) * 5);
  if (n == run.air_step) return;
  uint32_t v = n > 8 ? 500 : n > 4 ? 100 : 25;
  run.bonus += v;
  run.air_value += v;
  run.air_step = n;
  popup_update(n > 8 ? "INSANE AIR TIME" : n > 4 ? "BIG AIR TIME" : "AIR TIME", run.air_value, n > 4);
}

/* coins and fuel cans are picked up by touching them (GameContentLayer::PreSolve) */
static void pickups(void) {
  Car *c = &run.car;
  float half = COIN_SIZE * 0.5f, xmin = c->ch.x - 5, xmax = c->ch.x + 5;
  int k0 = (int)((xmin - 30) / COIN_SPACING), k1 = (int)(xmax / COIN_SPACING) + 1;
  for (int k = k0 < 1 ? 1 : k0; k <= k1 && k < COIN_GROUPS; k++) {
    float cx[14], cy[14];
    int val[14];
    int m = stage_coin_group(k, cx, cy, val);
    for (int i = 0; i < m; i++) {
      if (run.got[k] >> i & 1 || cx[i] < xmin || cx[i] > xmax) continue;
      if (car_touches_box(c, cx[i], cy[i], half)) {
        run.got[k] |= (uint16_t)(1 << i);
        run.coins += (uint32_t)val[i];
        for (int s = 0; s < 3; s++) {
          float vx = rndf() * 2 - 1, vy = rndf() * 2 + 1;   /* (one at a time: arguments have no order) */
          particle(cx[i], cy[i], vx, vy, 0.4f, 0.5f, 2);
        }
      }
    }
  }
  for (int k = stage_fuel_first(xmin); k < 512; k++) {
    float fx, fy;
    if (!stage_fuel(k, &fx, &fy) || fx > xmax) break;
    if (fx < xmin || run.fuel_got[k >> 3] >> (k & 7) & 1) continue;
    if (car_touches_box(c, fx, fy, half)) {
      run.fuel_got[k >> 3] |= (uint8_t)(1 << (k & 7));
      run.fuel = 100;
      popup("FUEL", 0, 0);
    }
  }
}

void run_frame(uint32_t keys) {
  Car *c = &run.car;
  run.frame++;
  run.t += DT;
  /* the brake wins over the gas (GameContentLayer::preFrameUpdate) */
  bool brake = !run.over && (keys & K_LEFT), gas = !run.over && !brake && (keys & (K_RIGHT | K_OK));
  run.gas = gas, run.brake = brake;
  car_step(c, gas ? 1 : brake ? -1 : 0, run.fuel > 0);
  if (c->tree_broken >= 0) {
    float tx, ty;
    if (stage_tree(c->tree_broken, &tx, &ty))
      for (int s = 0; s < 14; s++) {
        float py = ty + 0.3f + rndf() * 2.0f, vx = c->vx * 0.5f + rndf() * 4 - 1, vy = rndf() * 4, sz = 0.3f + rndf() * 0.2f;
        particle(tx, py, vx, vy, 0.9f, sz, 0);
      }
  }
  /* fuel: 2.5 a second (and the vehicle's rate) out of 100, and 0.02 a step for each part on fire (lava); empty,
   * the run ends 4 s later */
  if (run.fuel > 0) {
    run.fuel -= run.burn * DT + 0.02f * (float)c->burning;
    if (run.fuel <= 0) run.fuel = 0;
    run.empty_t = 0;
  } else if (!run.over && (run.empty_t += DT) > 4) {
    end_run(END_OUT_OF_FUEL);
  }
  /* Bill down: the run ends 4 s later */
  if (c->dead && !run.over && (run.down_t += DT) > 4) end_run(END_DRIVER_DOWN);
  /* distance and levels (GameLayer::setScore): reaching the goal completes the level and pays its bonus; the
   * next one is kept for the next runs */
  float d = c->ch.x - run.start_x;
  if (d > run.dist) run.dist = d;
  if (!run.over && run.level < 255 && run.dist >= (float)stage_level_goal(run.level)) {
    uint32_t r = stage_level_bonus(run.level);
    run.bonus += r;
    run.levels_done++;
    char t[32];
    snprintf(t, sizeof t, "LEVEL %d COMPLETE", run.level + 1);
    popup(t, 0, 1);
    popup_later("LEVEL BONUS", r, 1, 60);   /* (the game's: a second later, then the next goal) */
    run.level++;
    sv.stage_level[run.stage] = (uint8_t)run.level;
    snprintf(t, sizeof t, "LEVEL %d: REACH %um", run.level + 1, (unsigned)stage_level_goal(run.level));
    popup_later(t, 0, 0, 120);
  }
  if (!run.over) {
    tricks();
    pickups();
  }
  if (run.over) run.over_t++;
  /* dust from the wheels that spin on the ground, smoke from the exhaust */
  for (int i = 0; i < c->nw; i++) {
    float sp = fabsf(c->vx);
    if (c->ground[i] && (sp > 4 || gas) && (run.frame + (uint32_t)i) % 5 == 0) {
      float vx = -c->vx * 0.2f - rndf() * 2, vy = 1 + rndf() * 2, sz = 0.22f + rndf() * 0.12f;
      particle(c->wh[i].x - c->wr[i] * 0.6f, c->wh[i].y - c->wr[i] * 0.8f, vx, vy, 0.5f, sz, 0);
    }
  }
  if (gas && run.fuel > 0 && !c->dead && run.frame % 9 == 0 && car_speed(c) < 6) {
    float ex, ey;
    car_point(c, -1.5f, 0.1f, &ex, &ey);
    particle(ex, ey, -1.0f + c->vx * 0.5f, 0.5f, 0.6f, 0.16f, 1);
  }
  for (int i = 0; i < run.npart; i++) {
    Particle *p = &run.part[i];
    if (p->life <= 0) continue;
    p->life -= DT;
    p->x += p->vx * DT, p->y += p->vy * DT;
    if (p->kind == 0) p->vy -= 6.0f * DT;
    else if (p->kind == 1) p->size += 0.5f * DT;
  }
  for (int i = 0; i < NPOPUPS; i++)
    if (run.pop[i].text[0] && !(run.pop[i].wait ? run.pop[i].wait-- : 0) && ++run.pop[i].t > 110)
      run.pop[i].text[0] = 0;
  /* the camera (GameContentLayer::updateCamera): half a second ahead of the vehicle, zooming out with its speed,
   * a quarter second once Bill is down */
  float sp = car_speed(c);
  if (sp > 20) sp = 20;
  if (c->dead) sp *= 0.5f;
  float z = 40 - 0.75f * sp;
  if (c->dead) z *= 1.5f;
  run.zoom += (z - run.zoom) * 0.05f;
  run.cam_x += (c->ch.x + (c->dead ? 0.25f : 0.5f) * c->vx - run.cam_x) * (3 * DT);
  run.cam_y += (c->ch.y - run.cam_y) * (3 * DT);
  run.cam_s = 22 * run.zoom / 40;
}

/* ------------------------------------------------------------------ drawing */
static float scr_x(float wx) { return SX + (wx - run.cam_x) * run.cam_s; }
static float scr_y(float wy) { return SY - (wy - run.cam_y) * run.cam_s; }
static float wld_x(float sx) { return run.cam_x + (sx - SX) / run.cam_s; }

static uint16_t sky_rows[SCREEN_H];
static int sky_of = -1;

static void make_sky(void) {
  const Img *im = img(stage->sky);
  const uint32_t *pal = img_pal(im);
  const uint8_t *px = img_px(im);
  for (int y = 0; y < SCREEN_H; y++) {
    float t = (float)y * (float)(im->h - 1) / (SCREEN_H - 1);
    int i = (int)t;
    float f = t - (float)i;
    uint32_t a = pal[px[i]], b = pal[px[i + 1 < im->h ? i + 1 : i]];
    int r = (int)((a & 255) * (1 - f) + (b & 255) * f), g = (int)((a >> 8 & 255) * (1 - f) + (b >> 8 & 255) * f),
        bl = (int)((a >> 16 & 255) * (1 - f) + (b >> 16 & 255) * f);
    sky_rows[y] = rgb565((uint32_t)r << 16 | (uint32_t)g << 8 | (uint32_t)bl);
  }
  sky_of = run.stage;
}

static uint32_t hash2(uint32_t a) {
  a ^= a >> 16;
  a *= 0x7FEB352Du;
  a ^= a >> 15;
  a *= 0x846CA68Bu;
  a ^= a >> 16;
  return a;
}

static void draw_sky(void) {
  if (sky_of != run.stage) make_sky();
  if (run.stage == ST_ALIEN) gfx_vgrad(0, 0, SCREEN_W, SCREEN_H, 0x0B2A22, 0x5A8A5E);   /* (a green night) */
  else gfx_rows(sky_rows, 0, SCREEN_H);
  if (stage->stars) {   /* the Moon's and the alien planet's night: stars, and the Earth or a planet */
    for (uint32_t i = 0; i < 46; i++) {
      uint32_t h = hash2(i * 77 + (uint32_t)run.stage);
      float x = (float)(h % 340) - fmodf(run.cam_x * 0.6f, 340.0f), y = (float)(h >> 9 & 127);
      if (x < -10) x += 340;
      gfx_fill((int)x, (int)y, 1 + (h >> 20 & 1), 1 + (h >> 20 & 1), C_WHITE, 120 + (int)(h >> 24 & 127));
    }
    float ex = 250 - fmodf(run.cam_x * 0.05f, 400.0f);
    if (ex < -40) ex += 400;
    if (run.stage == ST_MOON) {
      gfx_circle(ex, 44, 17, rgb565(0x2F6FD6), 255);
      gfx_circle(ex - 5, 40, 7, rgb565(0x3FA34D), 255);
      gfx_circle(ex + 6, 50, 5, rgb565(0x3FA34D), 255);
      gfx_circle(ex + 3, 35, 4, rgb565(0xEAF2FF), 200);
    } else {
      gfx_circle(ex, 48, 22, rgb565(0xD8C96A), 255);
      gfx_circle(ex + 7, 42, 22, rgb565(0x2B3A2A), 230);
    }
  }
  if (stage->clouds) {
    for (int i = -1; i < 4; i++) {
      float base = run.cam_x * 0.12f;
      int k = (int)floorf(base / 130.0f) + i;
      uint32_t h = hash2((uint32_t)k * 31u + 7u);
      float x = (float)k * 130.0f - base + (float)(h % 60) + 20;
      float y = 18 + (float)(h >> 8 & 31) - (run.cam_y - 2) * 0.6f;
      gfx_image(IMG_CLOUD, x, y, 1, 0, 255);
    }
  }
}

static void draw_ground(void) {
  Ground *g = &gfx_ground;
  float s = run.cam_s;
  g->surf = stage->surf, g->ground = stage->ground, g->csurf = stage->csurf, g->cground = stage->ground;
  g->ceiling = stage->ceiling != 0;
  /* one sample every 2 columns, in between halfway */
  float fl[SCREEN_W / 2 + 2], ce[SCREEN_W / 2 + 2];
  for (int i = 0; i <= SCREEN_W / 2 + 1; i++) {
    float wx = wld_x((float)(i * 2));
    fl[i] = scr_y(stage_ground(wx));
    float c;
    ce[i] = g->ceiling && stage_roof(wx, &c) ? scr_y(c) : -4000;
  }
  for (int x = 0; x < SCREEN_W; x++) {
    int i = x >> 1;
    float f = (x & 1) ? (fl[i] + fl[i + 1]) * 0.5f : fl[i], c = (x & 1) ? (ce[i] + ce[i + 1]) * 0.5f : ce[i];
    if (ce[i] < -3000 || ce[i + 1] < -3000) c = ce[i];
    g->floor[x] = (int16_t)(f > 4000 ? 32000 : f < -4000 ? -32000 : f * 8);
    g->ceil[x] = (int16_t)(c > 4000 ? 32000 : c < -4000 ? -32000 : c * 8);
    float wx = wld_x((float)x + 0.5f);
    int32_t u = (int32_t)floorf(wx * 32.0f);
    g->su[x] = (uint8_t)(u & 255), g->gu[x] = (uint8_t)(u & 127);
  }
  float top = run.cam_y + SY / s;   /* the world's height at the screen's top */
  g->gv0 = (int32_t)(-top * 32.0f * 65536.0f);
  g->gdv = (int32_t)(32.0f / s * 65536.0f);
  g->above = (int16_t)(0.26f * s + 0.5f), g->below = (int16_t)(0.26f * s + 0.5f);
  gfx_ground_draw();
}

static void draw_icicles(void) {
  if (stage->feature != FEAT_ICICLES) return;
  float s = run.cam_s, x0 = floorf(wld_x(-20) / 3.0f) * 3.0f, x1 = wld_x(SCREEN_W + 20);
  for (float x = x0; x < x1; x += 3.0f) {
    uint32_t h = hash2((uint32_t)(int32_t)(x * 10) + 99u);
    if (h % 3 == 0) continue;
    float px = x + (float)(h >> 8 & 255) / 256.0f * 2.5f, c;
    if (!stage_roof(px, &c)) continue;
    gfx_image(IMG_ICICLE, scr_x(px), scr_y(c) - 2, s / (img(IMG_ICICLE)->ppm4 / 4.0f) * (0.6f + (float)(h >> 16 & 7) * 0.06f),
              0, 255);
  }
}

/* the game's rope bridges: planks on joints (where the car is near, as they move) */
static void plank(float x, float y, float a, float s) {
  gfx_image(IMG_PLANK, scr_x(x), scr_y(y), s / (img(IMG_PLANK)->ppm4 / 4.0f), a, 255);
}

static void draw_bridges(void) {
  if (!stage->bridges) return;
  float s = run.cam_s, x0w = wld_x(-30), x1w = wld_x(SCREEN_W + 30);
  CarBody pl[40];
  int np = car_planks(pl, 40);
  for (int k = stage_bridge_first(x0w);; k++) {
    float b0, len, by;
    if (!stage_bridge_k(k, &b0, &len, &by) || b0 > x1w) break;
    float b1 = b0 + len, w = len / (len - 1);
    /* the posts at each end */
    for (int e = 0; e < 2; e++) {
      float px = e ? b1 + w * 0.5f : b0 + w * 0.5f;
      gfx_tube(scr_x(px), scr_y(by - 0.3f), scr_x(px), scr_y(by + 1.3f), 0.12f * s, rgb565(0x6B4423), 255);
    }
    bool moving = false;
    for (int i = 0; i < np; i++) moving = moving || (pl[i].x > b0 && pl[i].x < b1 + 1);
    float lx = scr_x(b0 + w * 0.5f), ly = scr_y(by + 1.0f);
    int n = (int)len;
    for (int i = 0; i < n; i++) {
      float px = b0 + w + (float)i, py = by - 0.15f, a = 0;
      if (moving) {
        for (int j = 0; j < np; j++)
          if (fabsf(pl[j].x - px) < 0.6f) px = pl[j].x, py = pl[j].y, a = pl[j].a;
      }
      plank(px, py, a, s);
      /* the rope over the planks */
      float rx = scr_x(px), ry = scr_y(py + 1.15f);
      gfx_tube(lx, ly, rx, ry, 0.035f * s, rgb565(0x8A6A3A), 255);
      lx = rx, ly = ry;
    }
    gfx_tube(lx, ly, scr_x(b1 + w * 0.5f), scr_y(by + 1.0f), 0.035f * s, rgb565(0x8A6A3A), 255);
  }
}

/* a pine: the trunk, then three tiers of branches, each smaller, from its foot turned by a */
static void tree(float tx, float ty, float a, float s) {
  float sc = s / (img(IMG_TREE_TOP)->ppm4 / 4.0f), ux = -sinf(a), uy = cosf(a);
  gfx_image(IMG_TREE_TRUNK, scr_x(tx), scr_y(ty) + 2, sc, a, 255);
  for (int i = 0; i < 3; i++) {
    float h = 0.75f + 0.55f * (float)i;
    gfx_image(IMG_TREE_TOP, scr_x(tx + ux * h), scr_y(ty + uy * h), sc * (1 - 0.18f * (float)i), a, 255);
  }
}

static void draw_trees(void) {
  if (stage->feature != FEAT_TREES) return;
  float x0w = wld_x(-40), x1w = wld_x(SCREEN_W + 40), s = run.cam_s;
  CarBody tb[12];
  int which[12], nt = car_trees(tb, which, 12);
  for (int k = stage_tree_first(x0w);; k++) {
    float tx, ty;
    if (k >= TREES_MAX) break;
    if (!stage_tree(k, &tx, &ty)) {
      if (stage_tree_first(x1w) <= k) break;
      continue;
    }
    if (tx > x1w) break;
    bool body = false;
    for (int i = 0; i < nt; i++) body = body || which[i] == k;
    if (!body && stage_tree_standing(k)) tree(tx, ty, 0, s);
  }
  for (int i = 0; i < nt; i++) tree(tb[i].x, tb[i].y, tb[i].a, s);
}

static void draw_things(void) {
  float s = run.cam_s, x0 = wld_x(-30), x1 = wld_x(SCREEN_W + 30);
  /* the flag at the best distance */
  if (run.best > 1) {
    float fx = run.start_x + run.best, fy = stage_deck(fx);
    if (fx > x0 && fx < x1) {
      gfx_tube(scr_x(fx), scr_y(fy), scr_x(fx), scr_y(fy + 2.4f), 0.05f * s, rgb565(0x3A3A3A), 255);
      gfx_image(IMG_FLAG, scr_x(fx), scr_y(fy + 2.4f), s / (img(IMG_FLAG)->ppm4 / 4.0f), 0, 255);
    }
  }
  /* fuel cans */
  for (int k = stage_fuel_first(x0); k < 512; k++) {
    float fx, fy;
    if (!stage_fuel(k, &fx, &fy) || fx > x1) break;
    if (fx < x0 || run.fuel_got[k >> 3] >> (k & 7) & 1) continue;
    gfx_image(IMG_FUELCAN, scr_x(fx), scr_y(fy), s / (img(IMG_FUELCAN)->ppm4 / 4.0f), 0, 255);
  }
  /* coins */
  int k0 = (int)((x0 - 20) / COIN_SPACING), k1 = (int)(x1 / COIN_SPACING) + 1;
  for (int k = k0 < 1 ? 1 : k0; k <= k1 && k < COIN_GROUPS; k++) {
    float cx[14], cy[14];
    int val[14];
    int m = stage_coin_group(k, cx, cy, val);
    for (int i = 0; i < m; i++) {
      if (run.got[k] >> i & 1) continue;
      int id = val[i] == 5 ? IMG_COIN5 : val[i] == 25 ? IMG_COIN25 : val[i] == 100 ? IMG_COIN100 : IMG_COIN500;
      gfx_image(id, scr_x(cx[i]), scr_y(cy[i]), s / (img(id)->ppm4 / 4.0f), 0, 255);
    }
  }
}

/* a picture on a body: at (x, y), turned by a */
static void body_image(int id, float x, float y, float a) {
  gfx_image(id, scr_x(x), scr_y(y), run.cam_s / (img(id)->ppm4 / 4.0f), a, 255);
}

/* a picture at a point of the chassis' frame, turned with it */
static void car_image(int id, float lx, float ly, float extra) {
  const Car *c = &run.car;
  float wx, wy;
  car_point(c, lx, ly, &wx, &wy);
  body_image(id, wx, wy, c->ch.a + extra);
}

/* Bill: his torso and head are bodies of their own (upside down in the game's frame: their angle less pi) */
static void draw_bill(const VehArt *a) {
  const Car *c = &run.car;
  float ta = c->torso.a - PI, ha = c->head.a - PI;
  /* his hips: the bottom of his torso */
  float hx = c->torso.x + sinf(ta) * 0.3f, hy = c->torso.y - cosf(ta) * 0.3f;
  if (a->legs) body_image(IMG_BILL_LEG, hx, hy, c->ch.a);
  body_image(IMG_BILL_BODY, hx, hy, ta);
  body_image(IMG_BILL_HEAD, c->head.x, c->head.y, ha);
}

/* a rider on two bodies: a torso picture from his hips, a head picture on his head (the bus' passengers) */
static void draw_rider(const CarBody *torso, const CarBody *head, int head_id) {
  float ta = torso->a - PI;
  float hx = torso->x + sinf(ta) * 0.3f, hy = torso->y - cosf(ta) * 0.3f;
  body_image(IMG_BILL_BODY, hx, hy, ta);
  body_image(head_id, head->x, head->y, head->a - PI);
}

static void draw_wheel(const VehArt *a, int i, float size) {
  const Car *c = &run.car;
  gfx_image(a->wheel, scr_x(c->wh[i].x), scr_y(c->wh[i].y), run.cam_s / (img(a->wheel)->ppm4 / 4.0f) * size, c->wh[i].a,
            255);
}

static void draw_car(void) {
  const Car *c = &run.car;
  const VehArt *a = &veh_art[run.vehicle];
  float s = run.cam_s;
  bool rider = a->legs;
  for (int i = 0; i < a->nlayers; i++)
    if (!a->layer[i][3]) car_image((int)a->layer[i][0], a->layer[i][1], a->layer[i][2], 0);
  if (run.vehicle == V_TANK || run.vehicle == V_SNOWMOBILE) {   /* the track around the wheels */
    int n = run.vehicle == V_TANK ? c->nw : 5;
    for (int pass = 0; pass < 2; pass++)
      for (int i = 0; i < n; i++) {
        int j = (i + 1) % n;
        gfx_tube(scr_x(c->wh[i].x), scr_y(c->wh[i].y), scr_x(c->wh[j].x), scr_y(c->wh[j].y),
                 (c->wr[0] + (pass ? -0.02f : 0.07f)) * s, rgb565(pass ? 0x4A4A4A : 0x2A2A2A), 255);
      }
  }
  if (a->ntubes) {   /* the swingarm, behind */
    float tx, ty;
    car_point(c, a->tube[0][0], a->tube[0][1], &tx, &ty);
    int w = (int)a->tube[0][2];
    gfx_tube(scr_x(tx), scr_y(ty), scr_x(c->wh[w].x), scr_y(c->wh[w].y), a->tube[0][4] * s,
             rgb565((uint32_t)a->tube[0][3]), 255);
  }
  if (run.vehicle == V_TRACTOR && c->has_exhaust) body_image(IMG_EXHAUST, c->exhaust.x, c->exhaust.y, c->exhaust.a);
  if (run.vehicle == V_BUS)   /* the passengers on the upper deck, behind its side */
    for (int p = 0; p < CAR_PASS; p++) draw_rider(&c->pass[p][0], &c->pass[p][1], IMG_PASS1 + p);
  if (!rider) draw_bill(a);
  car_image(a->chassis, 0, 0, run.vehicle == V_TRUCK ? 0.025f : 0);   /* (the truck's picture is turned a little) */
  if (rider) draw_bill(a);
  if (run.vehicle == V_TRACTOR) {
    /* the soft tire: its picture as round as its balls are far out (the balls are 0.135 R, R 1.09) */
    float d = 0;
    for (int i = 0; i < CAR_SEGS; i++) d += hypotf(c->seg[i][0] - c->wh[0].x, c->seg[i][1] - c->wh[0].y);
    draw_wheel(a, 0, (d / CAR_SEGS + 1.09f * 0.135f) / a->wheel_r);
    draw_wheel(a, 1, c->wr[1] / a->wheel_r);
  } else if (run.vehicle == V_SNOWMOBILE) {
    for (int i = 0; i < 5; i++) draw_wheel(a, i, c->wr[i] / a->wheel_r);
    /* the ski across its two wheels (a multibody sprite), 0.1 ahead and 0.19 under their middle */
    float dx = c->wh[6].x - c->wh[5].x, dy = c->wh[6].y - c->wh[5].y, an = atan2f(dy, dx), cs = cosf(an), sn = sinf(an);
    float mx = (c->wh[5].x + c->wh[6].x) * 0.5f + 0.1f * cs + 0.19f * sn, my = (c->wh[5].y + c->wh[6].y) * 0.5f + 0.1f * sn - 0.19f * cs;
    body_image(IMG_SKI, mx, my, an);
  } else {
    for (int i = 0; i < c->nw; i++) draw_wheel(a, i, c->wr[i] / a->wheel_r);
  }
  for (int i = 1; i < a->ntubes; i++) {   /* the fork, in front */
    float tx, ty;
    car_point(c, a->tube[i][0], a->tube[i][1], &tx, &ty);
    int w = (int)a->tube[i][2];
    gfx_tube(scr_x(tx), scr_y(ty), scr_x(c->wh[w].x), scr_y(c->wh[w].y), a->tube[i][4] * s,
             rgb565((uint32_t)a->tube[i][3]), 255);
  }
  for (int i = 0; i < a->nlayers; i++)
    if (a->layer[i][3]) car_image((int)a->layer[i][0], a->layer[i][1], a->layer[i][2], 0);
}

/* lava: fire on the parts that burn (the game's five counters: the back wheel, the front, Bill, the middle, the body) */
static void draw_fire(void) {
  const Car *c = &run.car;
  if (!c->burning) return;
  float px[5], py[5];
  px[0] = c->wh[0].x, py[0] = c->wh[0].y;
  px[1] = c->wh[c->nw > 2 && run.vehicle != V_SNOWMOBILE ? c->nw - 1 : 1].x;
  py[1] = c->wh[c->nw > 2 && run.vehicle != V_SNOWMOBILE ? c->nw - 1 : 1].y;
  px[2] = c->torso.x, py[2] = c->torso.y;
  car_point(c, 0.8f, 0.2f, &px[3], &py[3]);
  px[4] = c->ch.x, py[4] = c->ch.y;
  for (int k = 0; k < 5; k++) {
    if (!c->burn[k]) continue;
    float grow = c->burn[k] > 99 ? 1 : 0.4f + 0.6f * (float)c->burn[k] / 99.0f;
    for (int f = 0; f < 3; f++) {
      uint32_t h = hash2(run.frame * 3u + (uint32_t)(k * 7 + f));
      float fx = px[k] + ((float)(h & 255) / 255.0f - 0.5f) * 0.8f, fy = py[k] + (float)(h >> 8 & 255) / 255.0f * 0.9f;
      gfx_image(IMG_FIRE, scr_x(fx), scr_y(fy), run.cam_s / (float)img(IMG_FIRE)->w * 1.1f * grow, 0, 200);
    }
  }
}

static void draw_particles(void) {
  float s = run.cam_s;
  for (int i = 0; i < run.npart; i++) {
    const Particle *p = &run.part[i];
    if (p->life <= 0) continue;
    int a = (int)(p->life * 400);
    if (a > 255) a = 255;
    int id = p->kind == 1 ? IMG_SMOKE : p->kind == 2 ? IMG_COIN_ICON : stage->particle;
    const Img *im = img(id);
    gfx_image(id, scr_x(p->x), scr_y(p->y), p->size * s / (float)im->w * 2.0f, p->x * 3, p->kind == 1 ? a / 3 : a);
  }
}

/* ------------------------------------------------------------------ the dashboard */
void format_number(char *out, uint32_t n) {
  char t[16];
  int k = snprintf(t, sizeof t, "%lu", (unsigned long)n), j = 0;
  for (int i = 0; i < k; i++) {
    out[j++] = t[i];
    if ((k - 1 - i) % 3 == 0 && i < k - 1) out[j++] = ',';
  }
  out[j] = 0;
}
void draw_coins_counter(int x, int y, uint32_t n, int align) {
  char t[20];
  format_number(t, n);
  int w = gfx_text_width(FONT_M, t) + 22;
  int x0 = align == 2 ? x - w : align == 1 ? x - w / 2 : x;
  gfx_image(IMG_COIN_ICON, (float)x0 + 8, (float)y - 5, 1, 0, 255);
  gfx_text(FONT_M, x0 + 20, y, t, rgb565(0xFFE14A), C_BLACK, 0);
}

/* the meters' needles: the game turns them 23 degrees a unit of revs and 87 of turbo pressure, from 135 */
static float needle(float v, float per) {
  v = v * per / 270;
  return 2.2f - (v < 0 ? 0 : v > 1 ? 1 : v) * 4.4f;
}

static void draw_hud(void) {
  const Car *c = &run.car;
  char t[40];
  /* fuel: the can, then the gauge */
  float f = run.fuel / 100;
  gfx_image(IMG_FUEL_ICON, 14, 13, 1, 0, 255);
  gfx_rrect(26, 7, 64, 12, 3, C_BLACK, 200);
  uint32_t col = f > 0.5f ? 0x5BE33A : f > 0.25f ? 0xF2C230 : 0xE8392E;
  int fw = (int)(60 * f + 0.5f);
  if (fw > 0) gfx_rrect(28, 9, fw, 8, 2, rgb565(col), 255);
  if (f < 0.25f && !run.over && (run.frame / 20) % 2) gfx_image(IMG_FUEL_LOW, 160, 96, 1, 0, 230);
  draw_coins_counter(8, 38, run_total(), 0);
  /* distance and the level */
  snprintf(t, sizeof t, "%dm", (int)run.dist);
  gfx_text(FONT_L, 160, 24, t, C_WHITE, C_BLACK, 1);
  float l0 = run.level ? (float)stage_level_goal(run.level - 1) : 0, l1 = (float)stage_level_goal(run.level);
  float p = (run.dist - l0) / (l1 - l0);
  gfx_rrect(122, 30, 76, 6, 3, C_BLACK, 170);
  if (p > 0) gfx_rrect(123, 31, (int)(74 * (p > 1 ? 1 : p)), 4, 2, rgb565(0xFFC21A), 255);
  snprintf(t, sizeof t, "LEVEL %d", run.level + 1);
  gfx_text(FONT_S, 160, 46, t, C_WHITE, C_BLACK, 1);
  if (run.best > 1) {
    snprintf(t, sizeof t, "BEST %dm", (int)run.best);
    gfx_text(FONT_S, 312, 14, t, rgb565(0xDDE6EE), C_BLACK, 2);
  }
  /* the meters and the pedals */
  int pb = run.brake > 0.5f ? IMG_PEDAL_BRAKE_DOWN : IMG_PEDAL_BRAKE, pg = run.gas > 0.5f ? IMG_PEDAL_GAS_DOWN : IMG_PEDAL_GAS;
  gfx_image(pb, 23, 218, 1, 0, 255);
  gfx_image(pg, 297, 218, 1, 0, 255);
  /* (each vehicle's own scale: PlayerCar*::getEngineRev, getTurboPressure) */
  float rk, rb, tk;
  car_meters(run.vehicle, &rk, &rb, &tk);
  float rpm = run.fuel > 0 && !c->dead ? needle(c->rev * rk + rb, 23) : needle(0, 23);
  float boost = needle(c->turbo * tk, 87);
  for (int m = 0; m < 2; m++) {
    float mx = m ? 186 : 134, my = 216;
    gfx_image(m ? IMG_METER_RPM : IMG_METER_BOOST, mx, my, 1, 0, 255);
    gfx_image(IMG_NEEDLE, mx, my, 1, m ? rpm : boost, 255);
  }
  /* what was just earned, in the order it came, one under the other */
  int y = 52;
  uint16_t last = 0;
  for (int n = 0; n < NPOPUPS; n++) {
    const Popup *q = NULL;
    for (int i = 0; i < NPOPUPS; i++)
      if (run.pop[i].text[0] && run.pop[i].seq > last && (!q || run.pop[i].seq < q->seq)) q = &run.pop[i];
    if (!q) break;
    last = q->seq;
    int a = q->t < 80 ? 255 : 255 - (q->t - 80) * 8;
    if (a <= 0 || q->wait) continue;
    char v[16];
    if (q->value) {
      format_number(v, q->value);
      snprintf(t, sizeof t, "%s +%s", q->text, v);
    } else {
      snprintf(t, sizeof t, "%s", q->text);
    }
    int f = q->big && gfx_text_width(FONT_L, t) <= 300 ? FONT_L : FONT_M;
    y += f == FONT_L ? 22 : 15;   /* (down to its baseline) */
    gfx_text(f, 160, y - (q->t < 8 ? 8 - q->t : 0), t, q->big ? rgb565(0xFFE14A) : C_WHITE, C_BLACK, 1);
    y += 5;
  }
  if (c->dead && !run.over) gfx_text(FONT_XL, 160, 150, "DRIVER DOWN", rgb565(0xFF5A3C), C_BLACK, 1);
  else if (run.fuel <= 0 && !run.over) gfx_text(FONT_XL, 160, 150, "OUT OF FUEL", rgb565(0xFFC21A), C_BLACK, 1);
}

/* the pits' mud or lava, over the car as in the game (Pit::draw comes after the car's pictures) */
static void draw_pits(void) {
  if (!stage->pits) return;
  Ground *g = &gfx_ground;
  float x0w = wld_x(0), x1w = wld_x(SCREEN_W);
  bool any = false;
  for (int x = 0; x < SCREEN_W; x++) g->liquid[x] = 32000;
  for (int k = stage_pit_first(x0w);; k++) {
    float p0, len, py;
    if (!stage_pit_k(k, &p0, &len, &py) || p0 > x1w) break;
    int a = (int)ceilf(scr_x(p0)), b = (int)floorf(scr_x(p0 + len));
    if (a < 0) a = 0;
    if (b > SCREEN_W) b = SCREEN_W;
    float top = scr_y(py);
    for (int x = a; x < b; x++) g->liquid[x] = (int16_t)(top > 4000 ? 32000 : top < -4000 ? -32000 : top * 8);
    any = any || a < b;
  }
  if (!any) return;
  g->lpic = stage->pits == PIT_LAVA ? IMG_PIT_LAVA : IMG_PIT_MUD;
  g->ldv = (int32_t)((float)img(g->lpic)->h / (stage_pit_depth() * run.cam_s) * 65536.0f);
  gfx_liquid_draw();
}

void run_draw(bool hud) {
  draw_sky();
  draw_ground();
  draw_icicles();
  draw_bridges();
  draw_trees();
  draw_things();
  draw_car();
  draw_pits();
  draw_fire();
  draw_particles();
  if (hud) draw_hud();
}
