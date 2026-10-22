/* What moves besides the player, with the original's numbers (its controllers' prefab values and constructors):
 * bats, bullets, monkeys and their rocks, trap spikes, platforms, rotors, snakes, light balls, timer walls. */
#include "inner.h"

#define FROZEN(g) ((g)->pw_time[PW_FREEZE] > 0)

tm_actor *tm_spawn(tm_game *g, uint8_t type, int32_t x, int32_t y, int dx, int dy) {
  for (int i = 0; i < TM_ACTORS; i++)
    if (!g->a[i].type) {
      tm_actor *a = &g->a[i];
      memset(a, 0, sizeof *a);
      a->type = type, a->x = x, a->y = y, a->dx = (int8_t)dx, a->dy = (int8_t)dy;
      return a;
    }
  return NULL;
}

void tm_fx(tm_game *g, uint8_t kind, int32_t x, int32_t y, int dx, int dy) {
  tm_actor *best = &g->fx[0];
  for (int i = 0; i < TM_FX; i++) {
    if (!g->fx[i].type) { best = &g->fx[i]; break; }
    if (g->fx[i].timer > best->timer) best = &g->fx[i];
  }
  memset(best, 0, sizeof *best);
  best->type = kind, best->x = x, best->y = y, best->dx = (int8_t)dx, best->dy = (int8_t)dy;
}

/* what stops bats, bullets and platforms: walls (and ice, cannons, fish, rotors: the walls' layer) and active
 * timer walls */
static bool blocks_things(const tm_game *g, int x, int y) {
  uint8_t t = tm_tile(g, x, y);
  return tm_wallish(t) || t == T_ICE || tm_is_cannon(t) || t == T_FISH || t == T_ROTOR_CW || t == T_ROTOR_CCW ||
         (t == T_TIMER_WALL && tm_timer_wall_on(g, x, y));
}

/* the first blocking cell met by something at (x, y) moving (dx, dy) by step; its distance, or -1 */
static int32_t ray(const tm_game *g, int32_t x, int32_t y, int dx, int dy, int32_t step, int *hx, int *hy) {
  int32_t pos = dx ? x : y, lat = dx ? y : x;
  int32_t le = pos + (dx + dy) * HALF;
  int lc = tm_cell(lat);
  int c0 = dx + dy > 0 ? (int)((le + HALF + TT - 1) >> 12) : (int)((le - HALF) >> 12);
  for (int k = 0; k < 3; k++) {
    int c = c0 + (dx + dy) * k;
    int32_t d = (c * TT - (dx + dy) * HALF - le) * (dx + dy);
    if (d < 0) continue;
    if (d > step) break;
    int cx = dx ? c : lc, cy = dx ? lc : c;
    if (blocks_things(g, cx, cy)) {
      *hx = cx, *hy = cy;
      return d;
    }
  }
  return -1;
}

/* ------------------------------------------------------------------ bats (BatController: 0.56 units a second,
 * a 0.5 s rest at each wall) */
static void bat_step(tm_game *g, tm_actor *a) {
  a->frozen = FROZEN(g);
  if (a->frozen) return;
  if (++a->timer >= 6) a->timer = 0, a->frame = (uint8_t)((a->frame + 1) % 5);
  if (a->wait > 0) {
    a->wait--;
    return;
  }
  a->v += 81556;                       /* 318.58 units a step, x 256 */
  int32_t step = a->v >> 8;
  a->v &= 255;
  int hx, hy;
  if (ray(g, a->x, a->y, a->dx, a->dy, step, &hx, &hy) >= 0) {
    a->x = (hx - a->dx) * TT, a->y = (hy - a->dy) * TT;
    a->dx = (int8_t)-a->dx, a->dy = (int8_t)-a->dy;
    a->wait = 30;
  } else {
    a->x += a->dx * step, a->y += a->dy * step;
  }
}

/* bullets (BulletController: 2.22 units a second; gone at a wall, or when frozen) */
static void bullet_step(tm_game *g, tm_actor *a) {
  if (FROZEN(g)) {
    tm_fx(g, FX_BANG, a->x, a->y, a->dx, a->dy);
    a->type = A_NONE;
    return;
  }
  if (!a->timer) {
    a->timer = 1;                      /* its first step is skipped */
    return;
  }
  int32_t step = 1263;
  int hx, hy;
  int32_t d = ray(g, a->x, a->y, a->dx, a->dy, step, &hx, &hy);
  if (d >= 0) {
    tm_fx(g, FX_BANG, (hx - a->dx) * TT + a->dx * HALF, (hy - a->dy) * TT + a->dy * HALF, a->dx, a->dy);
    a->type = A_NONE;
  } else {
    a->x += a->dx * step, a->y += a->dy * step;
  }
}

/* rocks (RockController: from 0.36 units a second, +3 a second each second, through everything) */
static void rock_step(tm_game *g, tm_actor *a) {
  if (FROZEN(g)) {
    a->type = A_NONE;
    return;
  }
  a->v += 3277;                        /* the speed in units a second x 65536: + 3 units a second each second */
  int32_t dy = (int32_t)((int64_t)a->v * TT / 471859);   /* to our units a step: / (0.12 x 65536 x 60) */
  a->y -= dy;
  if (g->y - a->y > UNITS(6.0f)) a->type = A_NONE;
}

/* monkeys (MonkeyController): hanging from the view's top; aux is their height above the view's centre (units),
 * state 0 waiting, 1 coming down, 3 swaying, 4 thrown, 2 going back up */
static void monkey_step(tm_game *g, tm_actor *a) {
  const int32_t top = UNITS(1.6f), low = UNITS(0.75f), speed = UNITS(1.0f) / 60;
  if (++a->timer >= 6) a->timer = 0, a->frame = (uint8_t)((a->frame + 1) % 8);
  switch (a->state) {
    case 0:
      if (--a->wait <= 0) a->state = 1;
      break;
    case 1:
      a->aux -= speed;
      if (a->aux <= low) a->aux = low, a->state = 3, a->dy = -1, a->v = 0, a->wait = 90;
      break;
    case 3:
    case 4: {
      /* the sway: aTime += dt * dir, bounced at 0.15; its height 0.75 + aTime * 0.2 */
      a->v += a->dy * 1092;            /* aTime x 65536: dt = 1/60 */
      if (iabs(a->v) > 9830) a->v = a->dy * 6554, a->dy = (int8_t)-a->dy;
      a->aux = low + (int32_t)((int64_t)a->v * UNITS(0.2f) / 65536);
      if (--a->wait <= 0) {
        if (a->state == 3) {
          a->state = 4, a->wait = 60;
          tm_actor *r = tm_spawn(g, A_ROCK, a->x, g->cam_y + a->aux - UNITS(0.05f), 0, -1);
          if (r) r->v = 23593;              /* 0.36 units a second, x 65536 */
        } else {
          a->state = 2;
        }
      }
      break;
    }
    case 2:
      a->aux += speed;
      if (a->aux >= top) a->type = A_NONE;
      break;
  }
  a->y = g->cam_y + a->aux;
}

/* the monkeys' enter delays (GameController's table, by group variant and place) */
static const uint8_t monkey_delay[5][3] = {{0, 0, 0}, {0, 30, 0}, {30, 0, 30}, {0, 30, 60}, {60, 30, 0}};

/* AddMonkeys: count (0: the zone's random count) round x */
void tm_add_monkeys(tm_game *g, int count, int32_t x) {
  static const uint8_t maxc[4] = {1, 1, 2, 3};
  if (!count) count = 1 + tm_range(g, 0, maxc[g->zone > 3 ? 3 : g->zone] - 1 + 1);
  int k = tm_range(g, 0, (count == 2 ? 8 : 4) - 2) + 3, l, m;
  if (count == 1) l = tm_range(g, 0, 9), m = 0;
  else l = tm_range(g, 0, 9 - (count - 1) * k), m = tm_range(g, 0, count == 2 ? 3 : 5);
  for (int h = 0; h < count; h++) {
    tm_actor *a = tm_spawn(g, A_MONKEY, x + (h * k + l - 4) * TT, g->cam_y + UNITS(1.6f), 0, 0);
    if (!a) return;
    a->aux = UNITS(1.6f);
    a->wait = monkey_delay[m][h < 3 ? h : 2];
    a->state = a->wait ? 0 : 1;
  }
}

void tm_remove_monkeys(tm_game *g) {
  for (int i = 0; i < TM_ACTORS; i++) {
    tm_actor *a = &g->a[i];
    if (a->type != A_MONKEY) continue;
    if (a->aux < UNITS(1.6f) && a->state) a->state = 2;   /* CancelAttack */
    else a->type = A_NONE;
  }
}

/* trap spikes (SpikesMController): frames 0.5, 0.06, 1.0 s, deadly from the second; then 0.03 s and gone */
static void spikesm_step(tm_game *g, tm_actor *a) {
  a->frozen = FROZEN(g);
  if (a->frozen) return;
  static const int16_t frame_steps[4] = {30, 4, 60, 2};
  if (--a->wait <= 0) {
    if (a->state) {
      a->type = A_NONE;
      return;
    }
    a->frame++;
    if (a->frame < 3) {
      a->wait = frame_steps[a->frame];
    } else {
      a->frame = 0, a->state = 1, a->wait = frame_steps[3];
    }
  }
}

void tm_arm_trap(tm_game *g, int x, int y, int dx, int dy) {
  if (FROZEN(g) || !g->lava_on) return;
  for (int i = 0; i < TM_ACTORS; i++)
    if (g->a[i].type == A_SPIKESM && tm_cell(g->a[i].x) == x && tm_cell(g->a[i].y) == y) return;
  tm_actor *a = tm_spawn(g, A_SPIKESM, x * TT, y * TT, dx, dy);
  if (a) a->wait = 30, a->frame = 0;
}

/* ------------------------------------------------------------------ platforms (PlatformController: 3.33 units
 * a second, a 0.5 s rest at each wall, then back; a standing player in the way is picked up) */
bool tm_platform_at(const tm_game *g, int32_t x, int32_t y, int *which) {
  for (int p = 0; p < TM_PLATFORMS; p++) {
    const tm_platform *pl = &g->plat[p];
    if (!pl->active) continue;
    for (int i = 0; i < pl->count; i++)
      if (iabs(pl->x + pl->bx[i] * TT - x) < HALF && iabs(pl->y + pl->by[i] * TT - y) < HALF) {
        if (which) *which = p;
        return true;
      }
  }
  return false;
}

static void platform_step(tm_game *g, int p) {
  tm_platform *pl = &g->plat[p];
  if (++pl->frame_timer >= 6) {
    pl->frame_timer = 0;
    pl->frame = (uint8_t)((pl->frame + ((pl->dx > 0 || pl->dy > 0) ? 1 : 2)) % 3);
  }
  if (pl->wait > 0) {
    if (--pl->wait == 0) pl->dx = (int8_t)-pl->dx, pl->dy = (int8_t)-pl->dy;
    return;
  }
  int32_t step = 1894;
  bool riding = g->ride == p + 1;
  int32_t best = -1;
  int hx = 0, hy = 0, hb = 0;
  for (int i = 0; i < pl->count; i++) {
    int x, y;
    int32_t d = ray(g, pl->x + pl->bx[i] * TT, pl->y + pl->by[i] * TT, pl->dx, pl->dy, step, &x, &y);
    if (d >= 0 && (best < 0 || d < best)) best = d, hx = x, hy = y, hb = i;
  }
  if (riding) {
    int x, y;
    int32_t d = ray(g, g->x, g->y, pl->dx, pl->dy, step, &x, &y);
    if (d >= 0 && (best < 0 || d < best)) {
      best = d;
      /* stop so that the player rests against the wall */
      int32_t nx = (x - pl->dx) * TT, ny = (y - pl->dy) * TT;
      int32_t mx = pl->dx ? nx - g->x : 0, my = pl->dy ? ny - g->y : 0;
      pl->x += mx, pl->y += my, g->x += mx, g->y += my;
      pl->wait = 30;
      return;
    }
  } else if (g->alive && tm_standing(g) && !g->ride) {
    /* the player stands in the way: picked up (StopByPlatformBrick with the platform's direction) */
    for (int i = 0; i < pl->count; i++) {
      int32_t bx = pl->x + pl->bx[i] * TT, by = pl->y + pl->by[i] * TT;
      int32_t dpos = pl->dx ? (g->x - bx) * pl->dx : (g->y - by) * pl->dy;
      int32_t dlat = pl->dx ? g->y - by : g->x - bx;
      if (iabs(dlat) < HALF && dpos >= TT - 1 && dpos - TT <= step && (best < 0 || dpos - TT < best)) {
        g->x = pl->dx ? bx + pl->dx * TT : bx;
        g->y = pl->dy ? by + pl->dy * TT : by;
        g->ride = (int8_t)(p + 1);
        g->face = (uint8_t)(pl->dx > 0 ? 3 : pl->dx < 0 ? 1 : pl->dy > 0 ? 0 : 2);
        riding = true;
        break;
      }
    }
  }
  if (best >= 0) {
    /* StopWithPosition: the brick beside the wall */
    int32_t bx = pl->x + pl->bx[hb] * TT, by = pl->y + pl->by[hb] * TT;
    int32_t mx = pl->dx ? (hx - pl->dx) * TT - bx : 0, my = pl->dy ? (hy - pl->dy) * TT - by : 0;
    pl->x += mx, pl->y += my;
    if (riding) g->x += mx, g->y += my;
    pl->wait = 30;
  } else {
    pl->x += pl->dx * step, pl->y += pl->dy * step;
    if (riding) g->x += pl->dx * step, g->y += pl->dy * step;
  }
}

/* ------------------------------------------------------------------ rotors (RotorController: a quarter turn in
 * 0.12 s every 2.1 s; a player standing on its walls turns with it) */
static uint8_t turn_tile(uint8_t t, bool cw) {
  static const uint8_t tramp_cw[4][2] = {{T_TRAMP_UR, T_TRAMP_DR}, {T_TRAMP_DR, T_TRAMP_DL}, {T_TRAMP_DL, T_TRAMP_UL},
                                         {T_TRAMP_UL, T_TRAMP_UR}};
  static const uint8_t cannon_cw[4][2] = {{T_CANNON_R, T_CANNON_D}, {T_CANNON_D, T_CANNON_L}, {T_CANNON_L, T_CANNON_U},
                                          {T_CANNON_U, T_CANNON_R}};
  for (int i = 0; i < 4; i++) {
    if (cw && tramp_cw[i][0] == t) return tramp_cw[i][1];
    if (!cw && tramp_cw[i][1] == t) return tramp_cw[i][0];
    if (cw && cannon_cw[i][0] == t) return cannon_cw[i][1];
    if (!cw && cannon_cw[i][1] == t) return cannon_cw[i][0];
  }
  if (t >= 49 && t <= 55) return (uint8_t)(t + 12);
  if (t >= 61 && t <= 67) return (uint8_t)(t - 12);
  return t;
}

void tm_rotate(tm_game *g) {
  for (int r = 0; r < TM_ROTORS; r++) {
    tm_rotor *ro = &g->rotor[r];
    if (!ro->active || ro->turning) continue;
    ro->turning = 1, ro->timer = 0;
    for (int i = 0; i < ro->count; i++) {
      int idx = tm_index(g, ro->x + ro->px[i], ro->y + ro->py[i]);
      if (idx < 0) continue;
      ro->look[i] = g->look[idx];
      g->tile[idx] = T_EMPTY;
    }
  }
}

static void rotor_step(tm_game *g, int r) {
  tm_rotor *ro = &g->rotor[r];
  if (!ro->turning) return;
  bool cw = !ro->ccw;
  int32_t cx = ro->x * TT, cy = ro->y * TT;
  if (++ro->timer < 7) return;
  ro->turning = 0;
  for (int i = 0; i < ro->count; i++) {
    int8_t x = ro->px[i], y = ro->py[i];
    ro->px[i] = cw ? y : (int8_t)-y;
    ro->py[i] = cw ? (int8_t)-x : x;
    ro->tile[i] = turn_tile(ro->tile[i], cw);
    int idx = tm_index(g, ro->x + ro->px[i], ro->y + ro->py[i]);
    if (idx < 0) continue;
    g->tile[idx] = ro->tile[i];
    g->look[idx] = ro->look[i];
  }
  if (g->ride == -(r + 1) && g->alive) {
    int32_t x = g->x - cx, y = g->y - cy;
    g->x = cx + (cw ? y : -y), g->y = cy + (cw ? -x : x);
    g->face = (uint8_t)((g->face + (cw ? 3 : 1)) & 3);
    /* spikes turned onto the player */
    for (int i = 0; i < ro->count; i++)
      if (ro->tile[i] == T_SPIKES && iabs(cx + ro->px[i] * TT - g->x) < TT && iabs(cy + ro->py[i] * TT - g->y) < TT) {
        tm_hurt(g, cx + ro->px[i] * TT, cy + ro->py[i] * TT);
        break;
      }
  }
}

/* ------------------------------------------------------------------ snakes (SnakeController: a 23-tile body at
 * 1.67 units a second along its line after a 1 s warning) */
static void snake_line_start(tm_snake *s) {
  const tm_snake_line *l = &s->l[s->line];
  s->q = 0;
  s->delay = 60;
  s->end = (23 + l->n) * TT;
}

void tm_add_snake_lines(tm_game *g, const tm_snake_line *lines, int n) {
  for (int i = 0; i < TM_SNAKES; i++) {
    tm_snake *s = &g->snake[i];
    if (s->active) continue;
    memset(s, 0, sizeof *s);
    s->active = 1;
    s->lines = (uint8_t)(n > TM_SNAKE_LINES ? TM_SNAKE_LINES : n);
    for (int k = 0; k < s->lines; k++) s->l[k] = lines[k];
    snake_line_start(s);
    return;
  }
}

void tm_remove_snakes(tm_game *g) {
  bool any = false;
  for (int i = 0; i < TM_SNAKES; i++) any |= g->snake[i].active, g->snake[i].active = 0;
  g->snake_mode = 0;
  if (any) g->cd_snakes = tm_vary(g, SEC(g->zone >= 1 ? 30 : 0), SEC(g->zone >= 1 ? 10 : 0));
}

static void snake_step(tm_game *g, tm_snake *s) {
  const tm_snake_line *l = &s->l[s->line];
  if (g->y - l->y * TT > UNITS(2.0f)) {
    s->active = 0;
    return;
  }
  s->frozen = FROZEN(g);
  if (s->frozen) return;
  if (++s->frame_timer >= 6) s->frame_timer = 0, s->frame = (uint8_t)((s->frame + 1) % 3);
  if (s->delay > 0) {
    s->delay--;
    return;
  }
  s->q += 950;                         /* 1.67 units a second */
  if (s->q > s->end) {
    if (++s->line < s->lines) snake_line_start(s);
    else s->active = 0;
  }
}

/* UpdateSnake: lines across the lane, 6..8 tiles above where the snake sections began, then 1..3 tiles apart */
void tm_snake_lines_here(tm_game *g) {
  if (!g->snake_mode || g->zone_changed || g->y < g->snake_y) return;
  for (int i = 0; i < TM_SNAKES; i++)
    if (g->snake[i].active) return;
  static const uint8_t cycles_max[4] = {1, 1, 2, 3};
  int z = g->zone > 3 ? 3 : g->zone;
  int dir = tm_value16(g) <= 32768 ? 1 : -1;
  int y = tm_cell(g->snake_y) + tm_range(g, 6, 9);
  int t = tm_range(g, 1, cycles_max[z] + 1), off = tm_range(g, 1, 4);
  tm_snake_line lines[TM_SNAKE_LINES];
  int n = 0;
  for (int h = 0; h < t && n < TM_SNAKE_LINES; h++) {
    /* RaycastAll along the row from outside the lane: the first wall followed by open cells starts the line */
    tm_snake_line l;
    memset(&l, 0, sizeof l);
    l.dx = (int8_t)dir, l.dy = 0, l.y = y;
    int x0 = dir > 0 ? g->lane_x : g->lane_x + TM_LANE - 1;
    bool started = false;
    for (int k = 0; k < TM_LANE; k++) {
      int x = x0 + dir * k;
      bool wall = tm_wallish(tm_tile(g, x, y));
      if (!started) {
        if (wall && !tm_wallish(tm_tile(g, x + dir, y))) started = true, l.x = x;
        continue;
      }
      if (l.n < TM_SNAKE_LEN) l.open[l.n++] = !wall;
    }
    while (l.n && !l.open[l.n - 1]) l.n--;
    if (started && l.n) lines[n++] = l;
    y += off;
    dir = -dir;
  }
  if (n) tm_add_snake_lines(g, lines, n);
  g->snake_y += (y - tm_cell(g->snake_y)) * TT + 10 * TT;
  if (g->snakes_left) g->snakes_left--;
  if (!g->snakes_left) {
    g->snake_mode = 0;
    g->cd_snakes = tm_vary(g, SEC(z >= 1 ? 30 : 0), SEC(z >= 1 ? 10 : 0));
  }
}

/* ------------------------------------------------------------------ light balls (LightBallController: 0.3 s after
 * its trigger it follows the player's landings, from 0.5 to 1 unit a second) */
void tm_wake_ball(tm_game *g, int x, int y) {
  for (int i = 0; i < TM_BALLS; i++) {
    tm_ball *b = &g->ball[i];
    if (!b->active || b->awake || b->trig_x != x || b->trig_y != y) continue;
    b->awake = 1, b->wait = 18, b->n = 0, b->next = 0, b->speed = 0;
    b->path[b->n++] = (tm_pt){x * TT, y * TT};
  }
}

void tm_ball_checkpoint(tm_game *g) {
  for (int i = 0; i < TM_BALLS; i++) {
    tm_ball *b = &g->ball[i];
    if (!b->active || !b->awake) continue;
    int k;
    for (k = b->n - 1; k >= b->next; k--)
      if (b->path[k].x == g->x && b->path[k].y == g->y) break;
    if (k >= b->next && k >= 0) b->n = (uint8_t)k;
    if (b->n < TM_BALL_PATH) b->path[b->n++] = (tm_pt){g->x, g->y};
  }
}

static void ball_step(tm_game *g, tm_ball *b) {
  if (!b->awake) return;
  if (b->wait > 0) {
    if (--b->wait == 0) b->speed = 284;    /* half its speed */
    return;
  }
  if (b->speed < 569) b->speed += 10;      /* +1 unit a second, each second */
  if (b->speed > 569) b->speed = 569;
  if (b->next >= b->n) return;
  int32_t tx = b->path[b->next].x, ty = b->path[b->next].y;
  int32_t dx = tx - b->x, dy = ty - b->y;
  if (!dx && !dy) {
    b->next++;
    return;
  }
  /* toward the next landing, a step at a time; corners are reached exactly */
  int32_t ax = iabs(dx), ay = iabs(dy), len = ax > ay ? ax + ay / 2 : ay + ax / 2;
  if (len <= b->speed) {
    b->x = tx, b->y = ty, b->next++;
  } else {
    int32_t mx = (int32_t)((int64_t)dx * b->speed / len), my = (int32_t)((int64_t)dy * b->speed / len);
    int cx = tm_cell(b->x + mx + (mx > 0 ? HALF : mx < 0 ? -HALF : 0)), cy = tm_cell(b->y + my + (my > 0 ? HALF : my < 0 ? -HALF : 0));
    if (tm_wallish(tm_tile(g, cx, cy)) && (cx != tm_cell(b->x) || cy != tm_cell(b->y))) {
      tm_fx(g, FX_BANG, b->x, b->y, 0, 0);
      b->active = 0;
      return;
    }
    b->x += mx, b->y += my;
  }
}

/* ------------------------------------------------------------------ timer walls (TimerWallController: 2 s) */
void tm_activate_timer_wall(tm_game *g, int x, int y) {
  tm_timer_wall *free_one = NULL;
  for (int i = 0; i < TM_TIMER_WALLS; i++) {
    tm_timer_wall *w = &g->tw[i];
    if (w->left > 0 && w->x == x && w->y == y) {
      w->left = 120;
      return;
    }
    if (w->left <= 0 && !free_one) free_one = w;
  }
  if (free_one) free_one->x = x, free_one->y = y, free_one->left = 120;
}

/* ------------------------------------------------------------------ all of them, a step */
void tm_things_step(tm_game *g) {
  for (int i = 0; i < TM_ACTORS; i++) {
    tm_actor *a = &g->a[i];
    switch (a->type) {
      case A_BAT: bat_step(g, a); break;
      case A_BULLET: bullet_step(g, a); break;
      case A_ROCK: rock_step(g, a); break;
      case A_MONKEY: monkey_step(g, a); break;
      case A_SPIKESM: spikesm_step(g, a); break;
      default: break;
    }
    /* things far below the player go (GameSuperObject.FixedUpdate: 6 units) */
    if (a->type && a->type != A_MONKEY && g->y - a->y > UNITS(6.0f)) a->type = A_NONE;
    if (a->type && g->mode == MODE_ARCADE && tm_cell(a->y) < g->ring_top - TM_H) a->type = A_NONE;
  }
  for (int p = 0; p < TM_PLATFORMS; p++)
    if (g->plat[p].active) {
      platform_step(g, p);
      if (g->mode == MODE_ARCADE && tm_cell(g->plat[p].y) < g->ring_top - TM_H) g->plat[p].active = 0;
    }
  for (int r = 0; r < TM_ROTORS; r++)
    if (g->rotor[r].active) {
      rotor_step(g, r);
      if (g->mode == MODE_ARCADE && g->rotor[r].y < g->ring_top - TM_H + 8) g->rotor[r].active = 0;
    }
  for (int s = 0; s < TM_SNAKES; s++)
    if (g->snake[s].active) snake_step(g, &g->snake[s]);
  for (int b = 0; b < TM_BALLS; b++)
    if (g->ball[b].active) {
      ball_step(g, &g->ball[b]);
      if (g->mode == MODE_ARCADE && tm_cell(g->ball[b].home_y) < g->ring_top - TM_H) g->ball[b].active = 0;
    }
  for (int i = 0; i < TM_TIMER_WALLS; i++) {
    tm_timer_wall *w = &g->tw[i];
    /* it stays closed while the player stands against it */
    if (w->left > 0 && --w->left == 0 && g->rest_on && g->rest_x == w->x && g->rest_y == w->y && tm_standing(g))
      w->left = 1;
  }
  for (int i = 0; i < TM_FX; i++)
    if (g->fx[i].type && ++g->fx[i].timer > 60) g->fx[i].type = 0;
}

/* ------------------------------------------------------------------ hazards against the player */
static bool box(const tm_game *g, int32_t x, int32_t y, int32_t r) { return iabs(g->x - x) < r && iabs(g->y - y) < r; }
static bool circle(const tm_game *g, int32_t x, int32_t y, int32_t r) {
  int64_t dx = g->x - x, dy = g->y - y;
  return dx * dx + dy * dy < (int64_t)r * r;
}

/* the hit with the shield's rule of each kind: 0 bounce back (DamageByObject), 1 the shield goes with the thing */
static void hit(tm_game *g, int32_t x, int32_t y, int rule, bool *gone) {
  if (g->pw_time[PW_SHIELD] > 0 && rule == 1) {
    tm_activate_powerup(g, PW_SHIELD, false, 0);
    *gone = true;
    return;
  }
  tm_hurt(g, x, y);
}

void tm_things_touch(tm_game *g) {
  if (!g->alive || g->ev_won) return;
  bool frozen = FROZEN(g);
  for (int i = 0; i < TM_ACTORS && g->alive; i++) {
    tm_actor *a = &g->a[i];
    bool gone = false;
    switch (a->type) {
      case A_BAT:
        if (circle(g, a->x, a->y, 2901)) {
          if (frozen) gone = true, g->shake = 3;
          else hit(g, a->x, a->y, 0, &gone);
        }
        break;
      case A_BULLET:
        if (a->timer && circle(g, a->x + a->dx * UNITS(0.03f), a->y + a->dy * UNITS(0.03f), 2560)) hit(g, a->x, a->y, 1, &gone);
        break;
      case A_ROCK:
        if (circle(g, a->x, a->y, 3243)) {
          if (g->pw_time[PW_SHIELD] > 0) {
            tm_activate_powerup(g, PW_SHIELD, false, 0);
            g->lava_back = 1;
            gone = true;
          } else {
            tm_hurt(g, a->x, a->y);
          }
        }
        break;
      case A_SPIKESM:
        if (a->frame >= 1 && !a->state && box(g, a->x, a->y, 3584)) {
          if (frozen) gone = true;
          else hit(g, a->x, a->y, 0, &gone);
        }
        break;
      case A_MTRIGGER:
        if (box(g, a->x, a->y, 3584)) {
          tm_add_monkeys(g, a->aux, a->x);
          gone = true;
        }
        break;
      case A_STRIGGER:
        if (box(g, a->x, a->y, 3584)) {
          tm_add_snake_lines(g, &g->sline[a->aux], 1);
          gone = true;
        }
        break;
      default: break;
    }
    if (gone) {
      if (a->type == A_BAT || a->type == A_SPIKESM || a->type == A_BULLET) tm_fx(g, FX_BANG, a->x, a->y, 0, 0);
      a->type = A_NONE;
    }
  }
  /* snakes: every open cell their body covers */
  for (int s = 0; s < TM_SNAKES && g->alive; s++) {
    tm_snake *sn = &g->snake[s];
    if (!sn->active || sn->delay > 0) continue;
    const tm_snake_line *l = &sn->l[sn->line];
    for (int i = 0; i < l->n; i++) {
      if (!l->open[i]) continue;
      int32_t c = (2 * i + 1) * HALF;
      if (sn->q < c || sn->q - 23 * TT > c) continue;
      int32_t x = (l->x + l->dx * (i + 1)) * TT, y = (l->y + l->dy * (i + 1)) * TT;
      if (!box(g, x, y, 3584)) continue;
      bool gone = false;
      if (frozen) gone = true, g->shake = 3;
      else hit(g, x, y, 1, &gone);
      if (gone) sn->active = 0;
      break;
    }
  }
  /* light balls */
  for (int b = 0; b < TM_BALLS && g->alive; b++) {
    tm_ball *bl = &g->ball[b];
    if (!bl->active || !bl->awake || !circle(g, bl->x, bl->y, 2901)) continue;
    if (g->pw_time[PW_SHIELD] > 0) {
      tm_activate_powerup(g, PW_SHIELD, false, 0);
      bl->x = bl->home_x, bl->y = bl->home_y, bl->wait = 18, bl->next = 0, bl->speed = 0;
    } else {
      tm_hurt(g, bl->x, bl->y);
    }
  }
  /* teeth and the fishes' spikes (tiles) */
  int cx = tm_cell(g->x), cy = tm_cell(g->y);
  bool spikes_out = g->fish_puff > 0 && g->fish_frame >= 2;
  for (int j = cy - 2; j <= cy + 2 && g->alive; j++)
    for (int i = cx - 2; i <= cx + 2 && g->alive; i++) {
      uint8_t t = tm_tile(g, i, j);
      if ((t == T_TEETH_R || t == T_TEETH_L) && circle(g, i * TT, j * TT, 3243)) {
        bool gone = false;
        if (frozen) gone = true;
        else hit(g, i * TT, j * TT, 1, &gone);
        if (gone) tm_set(g, i, j, T_EMPTY), tm_fx(g, FX_BANG, i * TT, j * TT, 0, 0);
      } else if (t == T_FISH && spikes_out && !frozen && box(g, i * TT, j * TT, 5291)) {
        tm_hurt(g, i * TT, j * TT);
      }
    }
}
