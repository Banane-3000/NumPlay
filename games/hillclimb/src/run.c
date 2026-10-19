/* The run being driven: pedals, fuel, coins, tricks, levels, the end; the camera and the picture of it all. */
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "art.h"
#include "game.h"
#include "gfx.h"

Run run;

#define SX 118      /* where the camera's point is on the screen */
#define SY 138
#define PI 3.14159265f
#define FLIP_TURN (1.5f * PI)   /* a flip counts once upside down and the other wheel on top: 270 degrees */

static const uint32_t air_values[] = {25, 50, 75, 100, 200, 300, 400, 500};

/* ------------------------------------------------------------------ popups and particles */
static void popup(const char *text, uint32_t value, int big) {
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
  p->value = value, p->t = 0, p->big = (uint8_t)big;
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
  car_init(&run.car, vehicle, sv.level[vehicle], 0);
  run.fuel = run.fuel_max = car_fuel_seconds(vehicle, sv.level[vehicle]);
  run.start_x = run.car.ch.x;
  run.best = sv.best[stg][vehicle];
  run.cam_x = run.car.ch.x, run.cam_y = run.car.ch.y + 0.4f, run.cam_s = 30;
  rnd_state = 12345u + (uint32_t)stg;
}

uint32_t run_total(void) { return run.coins + run.bonus; }

static void end_run(int why) {
  if (run.over) return;
  run.over = true, run.end = why, run.over_t = 0;
}

void run_finish(void) {
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

/* the air time's worth after t seconds in the air: from a second on, a step every quarter second */
static uint32_t air_worth(float t) {
  if (t < 1.0f) return 0;
  int n = (int)((t - 1.0f) / 0.25f);
  if (n < 8) return air_values[n];
  uint32_t v = 1000 + 500 * (uint32_t)(n - 8);
  return v > 25000 ? 25000 : v;
}

static void tricks(void) {
  Car *c = &run.car;
  bool air = car_airborne(c) && !c->head_hit;
  static float last_a;
  float da = c->ch.a - last_a;
  last_a = c->ch.a;
  if (air) {
    run.air_t += 1.0f / CAR_HZ;
    run.air_rot += da;
    run.air_value = air_worth(run.air_t);
    /* a flip each time it has turned 270 degrees more (and 360 for each next one) */
    float turn = fabsf(run.air_rot) - FLIP_TURN - 2 * PI * (float)run.air_flips;
    if (turn >= 0) {
      run.air_flips++;
      run.flips++;
      run.bonus += 1000;
      popup(run.air_rot > 0 ? "BACK FLIP" : "FLIP", 1000, 1);
    }
  } else if (run.air_t > 0) {
    if (!c->head_hit && run.air_value) {
      run.bonus += run.air_value;
      popup(run.air_value >= 1000 ? "INSANE AIR TIME" : run.air_value >= 200 ? "BIG AIR TIME" : "AIR TIME",
            run.air_value, run.air_value >= 1000);
      uint32_t ms = (uint32_t)(run.air_t * 1000);
      if (ms > run.air_best_ms) run.air_best_ms = ms;
    }
    run.air_t = 0, run.air_rot = 0, run.air_flips = 0, run.air_value = 0;
  }
}

static void pickups(void) {
  Car *c = &run.car;
  /* the points of the vehicle that take things: its middle, wheels and Bill */
  float px[CAR_MAXW + 2], py[CAR_MAXW + 2];
  int n = 0;
  px[n] = c->ch.x, py[n] = c->ch.y, n++;
  car_head(c, &px[n], &py[n]), n++;
  for (int i = 0; i < c->nw; i++) px[n] = c->wh[i].x, py[n] = c->wh[i].y, n++;
  float xmin = c->ch.x - 3, xmax = c->ch.x + 3;
  /* coins */
  int k0 = (int)((xmin - 8) / COIN_SPACING), k1 = (int)((xmax + 8) / COIN_SPACING) + 1;
  for (int k = k0 < 1 ? 1 : k0; k <= k1 && k < COIN_GROUPS; k++) {
    float cx[14], cy[14];
    int val[14];
    int m = stage_coin_group(k, cx, cy, val);
    for (int i = 0; i < m; i++) {
      if (run.got[k] >> i & 1) continue;
      for (int j = 0; j < n; j++) {
        float dx = cx[i] - px[j], dy = cy[i] - py[j], r = j >= 2 ? c->wr[j - 2] + 0.35f : 1.1f;
        if (dx * dx + dy * dy < r * r) {
          run.got[k] |= (uint16_t)(1 << i);
          run.coins += (uint32_t)val[i];
          for (int s = 0; s < 3; s++) particle(cx[i], cy[i], rndf() * 2 - 1, rndf() * 2 + 1, 0.4f, 0.3f, 2);
          break;
        }
      }
    }
  }
  /* fuel */
  for (int k = 0; k < 4096; k++) {
    float fx;
    if (!stage_fuel(k, &fx) || fx > xmax + 2) break;
    if (fx < xmin - 2 || (k < 4096 && run.fuel_got[k >> 3] >> (k & 7) & 1)) continue;
    float fy = stage_floor(fx) + 0.45f;
    for (int j = 0; j < n; j++) {
      float dx = fx - px[j], dy = fy - py[j], r = j >= 2 ? c->wr[j - 2] + 0.45f : 1.2f;
      if (dx * dx + dy * dy < r * r) {
        run.fuel_got[k >> 3] |= (uint8_t)(1 << (k & 7));
        run.fuel = run.fuel_max;
        popup("FUEL", 0, 0);
        break;
      }
    }
  }
}

void run_frame(uint32_t keys) {
  Car *c = &run.car;
  run.frame++;
  bool gas = !run.over && (keys & (K_RIGHT | K_OK)), brake = !run.over && (keys & K_LEFT);
  /* the pedals go down quickly and come up a little slower, as feet do */
  run.gas += ((gas ? 1.0f : 0.0f) - run.gas) * (gas ? 0.5f : 0.35f);
  run.brake += ((brake ? 1.0f : 0.0f) - run.brake) * (brake ? 0.5f : 0.35f);
  if (run.over) run.gas = run.brake = 0;
  bool fuel = run.fuel > 0;
  /* the bridge under the vehicle sags with its weight */
  stage_bridge_load(c->ch.x, 0.35f * (vehicles[run.vehicle].mass / 300.0f));
  car_step(c, run.gas, run.brake, fuel && !c->head_hit);
  if (c->tree_broken >= 0) {
    float tx;
    if (stage_tree(c->tree_broken, &tx))
      for (int s = 0; s < 6; s++)
        particle(tx, stage_floor(tx) + 0.6f + rndf() * 1.4f, rndf() * 3, rndf() * 2, 0.7f, 0.35f, 0);
  }
  /* fuel burns while the run goes on */
  if (!run.over && run.fuel > 0) {
    run.fuel -= 1.0f / CAR_HZ;
    if (run.fuel < 0) run.fuel = 0;
  }
  /* distance and levels */
  float d = c->ch.x - run.start_x;
  if (d > run.dist) run.dist = d;
  if (!run.over)
    while (run.dist >= stage_level_end(run.level + 1)) {
      run.level++;
      uint32_t r = stage_level_reward(run.level);
      run.bonus += r;
      char t[24];
      snprintf(t, sizeof t, "LEVEL %d", run.level);
      popup(t, r, 1);
    }
  if (!run.over) {
    tricks();
    pickups();
  }
  /* Bill's neck */
  if (c->head_hit && !run.over) {
    if (fabsf(run.air_rot) >= PI * 0.75f || run.air_flips) {
      run.bonus += 2500;
      popup("NECK FLIP", 2500, 1);
    }
    end_run(END_DRIVER_DOWN);
  }
  /* out of fuel: when the vehicle has stopped */
  if (!run.over && run.fuel <= 0) {
    if (car_speed(c) < 0.6f) run.empty_t++;
    else run.empty_t = 0;
    if (run.empty_t > 90) end_run(END_OUT_OF_FUEL);
  }
  if (run.over) run.over_t++;
  /* dust from the wheels that spin on the ground, smoke from the exhaust */
  for (int i = 0; i < c->nw; i++) {
    float slip = fabsf(c->wh[i].w * c->wr[i] - (c->wh[i].vx)), sp = fabsf(c->wh[i].vx);
    if (c->ground[i] && (slip > 2.5f || sp > 6) && (run.frame + (uint32_t)i) % 3 == 0)
      particle(c->wh[i].x - c->wr[i] * 0.6f, c->wh[i].y - c->wr[i] * 0.8f, -c->wh[i].vx * 0.2f - rndf() * 2,
               1 + rndf() * 2, 0.5f, 0.22f + rndf() * 0.12f, 0);
  }
  if (run.gas > 0.5f && fuel && run.frame % 6 == 0) {
    float ex, ey;
    car_point(c, -1.4f, 0.2f, &ex, &ey);
    particle(ex, ey, -1.2f + c->ch.vx * 0.3f, 0.6f, 0.9f, 0.25f, 1);
  }
  for (int i = 0; i < run.npart; i++) {
    Particle *p = &run.part[i];
    if (p->life <= 0) continue;
    p->life -= 1.0f / CAR_HZ;
    p->x += p->vx / CAR_HZ, p->y += p->vy / CAR_HZ;
    if (p->kind == 0) p->vy -= 6.0f / CAR_HZ;
    else if (p->kind == 1) p->size += 0.5f / CAR_HZ;
  }
  for (int i = 0; i < NPOPUPS; i++)
    if (run.pop[i].text[0] && ++run.pop[i].t > 110) run.pop[i].text[0] = 0;
  /* the camera: ahead of the vehicle as it goes, zooming out with speed */
  float sp = car_speed(c), lead = c->ch.vx * 0.13f;
  if (lead > 2.6f) lead = 2.6f;
  if (lead < -1.5f) lead = -1.5f;
  float tx = c->ch.x + lead, ty = c->ch.y + 0.4f;
  run.cam_x += (tx - run.cam_x) * 0.12f;
  run.cam_y += (ty - run.cam_y) * 0.12f;
  float ts = 30 - (sp > 22 ? 22 : sp) * 0.42f;
  run.cam_s += (ts - run.cam_s) * 0.03f;
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
  gfx_rows(sky_rows, 0, SCREEN_H);
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
    ce[i] = g->ceiling && stage_ceiling(wx, &c) ? scr_y(c) : -4000;
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

static void draw_bridges(void) {
  if (stage->feature != FEAT_BRIDGES) return;
  float x0w = wld_x(-20), x1w = wld_x(SCREEN_W + 20);
  for (float x = x0w; x < x1w;) {
    float b0, b1;
    if (!stage_bridge(x, &b0, &b1)) {
      x += 2;
      continue;
    }
    float s = run.cam_s;
    /* the posts at each end, the ropes over the planks */
    for (int e = 0; e < 2; e++) {
      float px = e ? b1 : b0, py = stage_floor(px);
      gfx_tube(scr_x(px), scr_y(py - 0.3f), scr_x(px), scr_y(py + 1.3f), 0.12f * s, rgb565(0x6B4423), 255);
    }
    int n = (int)((b1 - b0) / 0.55f);
    float lx = 0, ly = 0;
    for (int i = 0; i <= n; i++) {
      float px = b0 + (b1 - b0) * (float)i / (float)n, py = stage_floor(px);
      float a = atan2f(stage_floor(px + 0.1f) - stage_floor(px - 0.1f), 0.2f);
      float sx = scr_x(px), sy = scr_y(py - 0.14f);
      gfx_image(IMG_PLANK, sx, sy, s / img(IMG_PLANK)->ppm4 * 4.0f * 0.5f, a, 255);
      float rx = scr_x(px), ry = scr_y(py + 1.0f);
      if (i) gfx_tube(lx, ly, rx, ry, 0.035f * s, rgb565(0x8A6A3A), 255);
      lx = rx, ly = ry;
    }
    x = b1 + 1;
  }
}

static void draw_trees(void) {
  if (stage->feature != FEAT_TREES) return;
  float x0w = wld_x(-40), x1w = wld_x(SCREEN_W + 40), s = run.cam_s;
  for (int k = stage_tree_first(x0w);; k++) {
    float tx;
    if (!stage_tree(k, &tx)) {
      if (k % 6 == 0 && k > stage_tree_first(x1w) + 6) break;
      continue;
    }
    if (tx > x1w) break;
    float ty = stage_floor(tx), sc = s / (img(IMG_TREE_TOP)->ppm4 / 4.0f);
    if (stage_tree_standing(k)) {
      gfx_image(IMG_TREE_TRUNK, scr_x(tx), scr_y(ty) + 2, sc, 0, 255);
      float th = img(IMG_TREE_TRUNK)->h * sc * 0.8f;
      gfx_image(IMG_TREE_TOP, scr_x(tx), scr_y(ty) - th + 4, sc, 0, 255);
    } else {   /* knocked down: lying ahead */
      gfx_image(IMG_TREE_TRUNK, scr_x(tx + 0.4f), scr_y(ty + 0.2f), sc, -1.35f, 255);
      gfx_image(IMG_TREE_TOP, scr_x(tx + 1.5f), scr_y(ty + 0.25f), sc, -1.45f, 255);
    }
  }
}

static void draw_icicles(void) {
  if (stage->feature != FEAT_ICICLES) return;
  float s = run.cam_s, x0 = floorf(wld_x(-20) / 3.0f) * 3.0f, x1 = wld_x(SCREEN_W + 20);
  for (float x = x0; x < x1; x += 3.0f) {
    uint32_t h = hash2((uint32_t)(int32_t)(x * 10) + 99u);
    if (h % 3 == 0) continue;
    float px = x + (float)(h >> 8 & 255) / 256.0f * 2.5f, c;
    if (!stage_ceiling(px, &c)) continue;
    gfx_image(IMG_ICICLE, scr_x(px), scr_y(c) - 2, s / (img(IMG_ICICLE)->ppm4 / 4.0f) * (0.6f + (float)(h >> 16 & 7) * 0.06f),
              0, 255);
  }
}

static void draw_things(void) {
  float s = run.cam_s, x0 = wld_x(-30), x1 = wld_x(SCREEN_W + 30);
  /* the flag at the best distance */
  if (run.best > 1) {
    float fx = run.start_x + run.best, fy = stage_floor(fx);
    if (fx > x0 && fx < x1) {
      gfx_tube(scr_x(fx), scr_y(fy), scr_x(fx), scr_y(fy + 2.4f), 0.05f * s, rgb565(0x3A3A3A), 255);
      gfx_image(IMG_FLAG, scr_x(fx), scr_y(fy + 2.4f), s / (img(IMG_FLAG)->ppm4 / 4.0f), 0, 255);
    }
  }
  /* fuel cans */
  for (int k = 0; k < 4096; k++) {
    float fx;
    if (!stage_fuel(k, &fx) || fx > x1) break;
    if (fx < x0 || run.fuel_got[k >> 3] >> (k & 7) & 1) continue;
    float bob = sinf((float)run.frame * 0.08f + fx) * 0.06f;
    gfx_image(IMG_FUELCAN, scr_x(fx), scr_y(stage_floor(fx) + bob), s / (img(IMG_FUELCAN)->ppm4 / 4.0f), 0, 255);
  }
  /* coins */
  int k0 = (int)((x0 - 8) / COIN_SPACING), k1 = (int)((x1 + 8) / COIN_SPACING) + 1;
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

/* a picture at a point of the vehicle's frame, turned with it */
static void car_image(int id, float lx, float ly, float extra) {
  const Car *c = &run.car;
  float wx, wy;
  car_point(c, lx, ly, &wx, &wy);
  gfx_image(id, scr_x(wx), scr_y(wy), run.cam_s / (img(id)->ppm4 / 4.0f), c->ch.a + extra, 255);
}

static void draw_bill(const VehArt *a) {
  const Car *c = &run.car;
  if (a->legs) car_image(IMG_BILL_LEG, a->hips[0] + 0.05f, a->hips[1] + 0.02f, a->lean * 0.3f);
  car_image(IMG_BILL_BODY, a->hips[0], a->hips[1], a->lean);
  /* the head on the neck, nodding; dropped to the side once Bill is down */
  float nx = BILL_NECK_X, ny = BILL_NECK_Y, ca = cosf(a->lean), sa = sinf(a->lean);
  float lx = a->hips[0] + ca * nx - sa * ny + c->wob[0], ly = a->hips[1] + sa * nx + ca * ny + c->wob[1];
  float tilt = c->head_hit ? -0.9f : -c->wob[0] * 2.0f;
  car_image(IMG_BILL_HEAD, lx, ly, a->lean + tilt);
}

static void draw_car(void) {
  const Car *c = &run.car;
  const VehArt *a = &veh_art[run.vehicle];
  float s = run.cam_s;
  bool rider = a->legs;
  /* behind: the tank's tracks, and the bikes' swingarm */
  for (int i = 0; i < a->nlayers; i++)
    if (!a->layer[i][3]) car_image((int)a->layer[i][0], a->layer[i][1], a->layer[i][2], 0);
  if (run.vehicle == V_TANK) {   /* the track around the wheels */
    float x0 = scr_x(c->wh[0].x), y0 = scr_y(c->wh[0].y), x1 = scr_x(c->wh[c->nw - 1].x), y1 = scr_y(c->wh[c->nw - 1].y);
    gfx_tube(x0, y0, x1, y1, (c->wr[0] + 0.07f) * s, rgb565(0x2A2A2A), 255);
    gfx_tube(x0, y0, x1, y1, (c->wr[0] - 0.02f) * s, rgb565(0x4A4A4A), 255);
  }
  if (a->ntubes) {
    int i = 0;
    float tx, ty;
    car_point(c, a->tube[i][0], a->tube[i][1], &tx, &ty);
    int w = (int)a->tube[i][2];
    gfx_tube(scr_x(tx), scr_y(ty), scr_x(c->wh[w].x), scr_y(c->wh[w].y), a->tube[i][4] * s,
             rgb565((uint32_t)a->tube[i][3]), 255);
  }
  if (!rider) draw_bill(a);
  car_image(a->chassis, 0, 0, 0);
  if (rider) draw_bill(a);
  for (int i = 0; i < c->nw; i++)
    gfx_image(a->wheel, scr_x(c->wh[i].x), scr_y(c->wh[i].y), s / (img(a->wheel)->ppm4 / 4.0f) * c->wr[i] / a->wheel_r[0],
              c->spin[i], 255);
  for (int i = 1; i < a->ntubes; i++) {
    float tx, ty;
    car_point(c, a->tube[i][0], a->tube[i][1], &tx, &ty);
    int w = (int)a->tube[i][2];
    gfx_tube(scr_x(tx), scr_y(ty), scr_x(c->wh[w].x), scr_y(c->wh[w].y), a->tube[i][4] * s,
             rgb565((uint32_t)a->tube[i][3]), 255);
  }
  for (int i = 0; i < a->nlayers; i++)
    if (a->layer[i][3]) car_image((int)a->layer[i][0], a->layer[i][1], a->layer[i][2], 0);
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
    gfx_image(id, scr_x(p->x), scr_y(p->y), p->size * s / (float)im->w * 2.0f, p->x * 3, p->kind == 1 ? a / 2 : a);
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

static void draw_hud(void) {
  const Car *c = &run.car;
  char t[40];
  /* fuel: the can, then the gauge */
  float f = run.fuel / run.fuel_max;
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
  float l0 = stage_level_end(run.level), l1 = stage_level_end(run.level + 1);
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
  float rpm = c->rpm * (run.fuel > 0 ? 1 : 0.1f), boost = car_speed(c) / (vehicles[run.vehicle].speed * 1.6f);
  if (boost > 1) boost = 1;
  for (int m = 0; m < 2; m++) {
    float mx = m ? 186 : 134, my = 216, v = m ? rpm : boost;
    gfx_image(m ? IMG_METER_RPM : IMG_METER_BOOST, mx, my, 1, 0, 255);
    gfx_image(IMG_NEEDLE, mx, my, 1, 2.2f - v * 4.4f, 255);
  }
  /* what was just earned */
  int row = 0;
  for (int i = 0; i < NPOPUPS; i++) {
    const Popup *q = &run.pop[i];
    if (!q->text[0]) continue;
    int a = q->t < 80 ? 255 : 255 - (q->t - 80) * 8;
    if (a <= 0) continue;
    char v[16];
    if (q->value) {
      format_number(v, q->value);
      snprintf(t, sizeof t, "%s +%s", q->text, v);
    } else {
      snprintf(t, sizeof t, "%s", q->text);
    }
    int y = 74 + row * 18 - (q->t < 8 ? 8 - q->t : 0);
    gfx_text(q->big ? FONT_L : FONT_M, 160, y, t, q->big ? rgb565(0xFFE14A) : C_WHITE, C_BLACK, 1);
    row++;
  }
  if (run.air_value && !run.over) {
    snprintf(t, sizeof t, "AIR TIME +%lu", (unsigned long)run.air_value);
    gfx_text(FONT_M, 160, 74 + row * 18, t, rgb565(0xBFF0FF), C_BLACK, 1);
  }
}

void run_draw(bool hud) {
  draw_sky();
  draw_ground();
  draw_icicles();
  draw_bridges();
  draw_trees();
  draw_things();
  draw_car();
  draw_particles();
  if (hud) draw_hud();
}
