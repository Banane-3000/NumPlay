/* The player (the original's PlayerController): dashes at 5 units a second until something stops it, a raycast
 * from its leading edge each step; then what it touches, as the original's trigger colliders do (the player is a
 * circle of radius 0.375 tile; dots and coins 0.17, tiles' boxes 0.5, tramplins 0.33). */
#include "inner.h"

#define R_DOT 2219                    /* 0.375 + 0.167 tile: dots, coins, powerups, the bonus door */
#define R_BOX 3584                    /* 0.375 + 0.5: stars, the exit, portals, timer walls, close bricks... */
#define R_TRAMP 2901                  /* 0.375 + 0.333: tramplins */
#define R_TRAP 3243                   /* 0.375 + 0.417: the trap spikes' triggers, the last landing's marker */

bool tm_standing(const tm_game *g) { return !g->dx && !g->dy; }

void tm_set_dir(tm_game *g, int dx, int dy) {
  g->dx = (int8_t)dx, g->dy = (int8_t)dy;
  if (dx || dy) {
    g->pstate = ST_JUMP, g->anim = 0, g->anim_timer = 0, g->trail = 0;
  }
}

/* CanJumpWithDirection: not into the wall it rests against */
static bool can_jump(const tm_game *g, int dx, int dy) {
  if (!dx && !dy) return false;
  if (dy == 1 && g->face == 2) return false;
  if (dx == -1 && g->face == 3) return false;
  if (dx == 1 && g->face == 1) return false;
  if (dy == -1 && g->face == 0) return false;
  return true;
}

/* JumpWithDirection */
static bool jump(tm_game *g, int dx, int dy) {
  if (!can_jump(g, dx, dy)) return false;
  g->ride = 0;
  g->rest_on = 0;
  tm_fx(g, FX_JUMP, g->x, g->y, dx, dy);
  tm_set_dir(g, dx, dy);
  return true;
}

/* ProcessSwipe */
void tm_swipe(tm_game *g, int dx, int dy) {
  if (!g->alive) return;
  if (g->running && dy) dx = dy = 0;
  if (tm_standing(g)) {
    if (jump(g, dx, dy)) tm_lava_activate(g, true);
  } else {
    g->buf_dx = (int8_t)dx, g->buf_dy = (int8_t)dy, g->buf_timer = 6;   /* 0.1 s */
  }
}

/* GameController.PlayerLandedWithDirection */
void tm_land(tm_game *g) {
  g->cam_ease = 0;
  g->shake = 2;
  if (g->zone_changed && g->mode == MODE_ARCADE) {
    g->zone_changed = 0;
    if (!g->bonus) tm_close_bricks(g, tm_cell(g->cam_x) - 5, tm_cell(g->y) - 7, 11);
  }
}

/* StopByObjectTransform's end: standing, facing the wall, a pending dash goes */
static void stand(tm_game *g, int odx, int ody) {
  g->face = (uint8_t)(odx > 0 ? 1 : odx < 0 ? 3 : ody > 0 ? 2 : 0);
  g->pstate = g->running ? ST_RUN : ST_IDLE;
  g->anim = 0, g->anim_timer = 0;
  g->trail = 0;
  g->dx = g->dy = 0;
  if (g->running) g->run_go = 1;
  if (g->buf_timer > 0) {
    g->buf_timer = 0;
    jump(g, g->buf_dx, g->buf_dy);
  }
}

void tm_stop_at(tm_game *g, int32_t x, int32_t y, bool save) {
  int odx = g->dx, ody = g->dy;
  g->x = x;
  if (!g->running) g->y = y;
  tm_land(g);
  if (save) {
    tm_save_prev(g);
    tm_fx(g, FX_LANDING, g->x, g->y, odx, ody);
  }
  g->bounce = 0;
  stand(g, odx, ody);
}

/* DamageByObject from something at (kx, ky): a shield takes it (and the player flies back to its last landing),
 * else death */
void tm_hurt(tm_game *g, int32_t kx, int32_t ky) {
  if (!g->alive || g->ev_won) return;
  if (g->pw_time[PW_SHIELD] > 0) {
    tm_activate_powerup(g, PW_SHIELD, false, 0);
    g->lava_back = 1;
    if (!tm_standing(g)) {
      g->bounce = 1;
      int ndx = -g->dx, ndy = -g->dy;
      tm_set_dir(g, ndx, ndy);
      if (!ndx) g->y = ky + ndy * TT;
      else g->x = kx + ndx * TT;
    }
    return;
  }
  g->alive = 0;
  g->ev_dead = 1;
  g->dx = g->dy = 0;
  g->shake = 12;
  tm_lava_activate(g, false);
  for (int i = 0; i < PW_COUNT; i++)
    if (g->pw_time[i] > 0) tm_activate_powerup(g, i, false, 0);
  tm_fx(g, FX_DEATH, g->x, g->y, 0, 0);
}

/* is (px, py) the rotor part of some rotor? */
static int rotor_of(const tm_game *g, int px, int py) {
  for (int r = 0; r < TM_ROTORS; r++) {
    const tm_rotor *ro = &g->rotor[r];
    if (!ro->active || ro->turning) continue;
    if (ro->x == px && ro->y == py) return r;
    for (int i = 0; i < ro->count; i++)
      if (ro->x + ro->px[i] == px && ro->y + ro->py[i] == py) return r;
  }
  return -1;
}

/* what the player's dash runs into (its raycast's layers: walls, ice, cannons, fish, rotors, timer walls while
 * closed; platform bricks apart) */
static bool blocks_player(const tm_game *g, int x, int y) { return tm_solid(g, x, y); }

/* PlayerController.FixedUpdate */
void tm_player_step(tm_game *g) {
  if (!g->alive || g->ev_won) return;
  if (g->skip) {
    g->skip = 0;
    return;
  }
  if (!tm_standing(g)) {
    g->step_acc += 25600;
    int32_t step = (int32_t)(g->step_acc / 9);
    g->step_acc %= 9;
    g->trail += step;
    int dx = g->dx, dy = g->dy;
    int32_t pos = dx ? g->x : g->y, lat = dx ? g->y : g->x;
    int32_t le = pos + (dx + dy) * HALF;            /* the leading edge */
    int lane_c = tm_cell(lat);
    /* the first cell whose near face is within [le, le + step] along the way */
    int32_t best = step + 1;
    int bx = 0, by = 0, kind = 0;                   /* kind 1 a cell, 2 a platform brick */
    int bplat = -1;
    int c0 = dx + dy > 0 ? (int)((le + HALF + TT - 1) >> 12) : (int)((le - HALF) >> 12);
    for (int k = 0; k < 3; k++) {
      int c = c0 + (dx + dy) * k;
      int32_t face = c * TT - (dx + dy) * HALF;
      int32_t d = (face - le) * (dx + dy);
      if (d < 0) continue;
      if (d > step) break;
      int cx = dx ? c : lane_c, cy = dx ? lane_c : c;
      if (blocks_player(g, cx, cy)) {
        best = d, bx = cx, by = cy, kind = 1;
        break;
      }
    }
    for (int p = 0; p < TM_PLATFORMS; p++) {
      const tm_platform *pl = &g->plat[p];
      if (!pl->active || g->ride == p + 1) continue;
      for (int i = 0; i < pl->count; i++) {
        int32_t ux = pl->x + pl->bx[i] * TT, uy = pl->y + pl->by[i] * TT;
        int32_t upos = dx ? ux : uy, ulat = dx ? uy : ux;
        if (iabs(ulat - lat) >= HALF) continue;
        int32_t d = (upos - (dx + dy) * HALF - le) * (dx + dy);
        if (d >= 0 && d < best) best = d, kind = 2, bplat = p, bx = tm_cell(ux), by = tm_cell(uy);
      }
    }
    if (kind == 1) {
      uint8_t t = tm_tile(g, bx, by);
      int32_t sx = (bx - dx) * TT, sy = (by - dy) * TT;
      if (t == T_ICE) {
        /* IceController: broken on the way, 1 + the multiplier, a coin if its roll won; the dash goes on */
        uint8_t lk = tm_look(g, bx, by);
        tm_set(g, bx, by, T_EMPTY);
        tm_reward(g, 1, (lk & 32) ? 1 : 0);
        tm_fx(g, FX_ICE, bx * TT, by * TT, 0, 0);
        g->shake = 3;
        g->x += dx * step, g->y += dy * step;
      } else if (t == T_SPIKES) {
        if (dx) g->x = sx;
        else g->y = sy;
        tm_hurt(g, bx * TT, by * TT);
      } else {
        int r = rotor_of(g, bx, by);
        tm_stop_at(g, sx, sy, true);
        if (r >= 0) g->ride = (int8_t)-(r + 1);
        if (t == T_TIMER_WALL) g->rest_on = 1, g->rest_x = (int16_t)bx, g->rest_y = (int16_t)by;
      }
    } else if (kind == 2) {
      /* StopByPlatformBrick: beside the brick, riding the platform */
      const tm_platform *pl = &g->plat[bplat];
      int32_t ux = 0, uy = 0;
      for (int i = 0; i < pl->count; i++)
        if (tm_cell(pl->x + pl->bx[i] * TT) == bx && tm_cell(pl->y + pl->by[i] * TT) == by)
          ux = pl->x + pl->bx[i] * TT, uy = pl->y + pl->by[i] * TT;
      tm_stop_at(g, ux - dx * TT, uy - dy * TT, true);
      g->ride = (int8_t)(bplat + 1);
    } else {
      g->x += dx * step, g->y += dy * step;
    }
  }
  if (g->running && g->run_go && g->alive) g->y += 1138;   /* the bonus run: 2 units a second up */
}

/* ------------------------------------------------------------------ what it touches */
static bool near_box(const tm_game *g, int cx, int cy, int32_t r) {
  return iabs(g->x - cx * TT) < r && iabs(g->y - cy * TT) < r;
}
static bool near_dot(const tm_game *g, int cx, int cy) {
  int32_t dx = g->x - cx * TT, dy = g->y - cy * TT;
  return (int64_t)dx * dx + (int64_t)dy * dy < (int64_t)R_DOT * R_DOT;
}

/* the other portal of the pair, in the same section (the nearest one with the pair's number) */
static bool partner(const tm_game *g, int x, int y, int pair, int *ox, int *oy) {
  int best = 1 << 30;
  int x0 = g->mode == MODE_STAGE ? 0 : x - 13, x1 = g->mode == MODE_STAGE ? g->w - 1 : x + 13;
  int y0 = g->mode == MODE_STAGE ? 0 : y - 15, y1 = g->mode == MODE_STAGE ? g->h - 1 : y + 15;
  for (int j = y0; j <= y1; j++)
    for (int i = x0; i <= x1; i++) {
      if (i == x && j == y) continue;
      uint8_t t = tm_tile(g, i, j);
      if (!tm_is_portal(t) || (t >= 61 ? t - 61 : t - 49) != pair) continue;
      int d = (i - x) * (i - x) + (j - y) * (j - y);
      if (d < best) best = d, *ox = i, *oy = j;
    }
  return best < (1 << 30);
}

static void collect(tm_game *g, int x, int y, uint8_t t) {
  tm_set(g, x, y, T_EMPTY);
  if (t == T_DOT) {
    g->dots++;
    tm_reward(g, 0, 0);
    tm_fx(g, FX_DOT, x * TT, y * TT, 0, 0);
  } else {
    g->dots++;
    g->shake = 1;
    tm_reward(g, 0, 1);
    tm_fx(g, FX_COIN, x * TT, y * TT, 0, 0);
  }
}

void tm_player_triggers(tm_game *g) {
  if (!g->alive || g->ev_won) return;
  int cx = tm_cell(g->x), cy = tm_cell(g->y);
  bool moving = !tm_standing(g);
  int16_t in_x[4], in_y[4];
  int in_n = 0;
  int tramp = 0, portal = 0;
  for (int j = cy - 1; j <= cy + 1 && g->alive; j++)
    for (int i = cx - 1; i <= cx + 1 && g->alive; i++) {
      uint8_t t = tm_tile(g, i, j);
      if (t == T_EMPTY || tm_wallish(t)) continue;
      int idx = tm_index(g, i, j) + 1;
      if (t == T_COIN || t == T_DOT) {
        if (near_dot(g, i, j)) collect(g, i, j, t);
      } else if (t >= T_ADDICT && t <= T_BOOST) {
        if (near_dot(g, i, j)) {
          tm_set(g, i, j, T_EMPTY);
          g->shake = 3;
          tm_fx(g, FX_POWER, i * TT, j * TT, 0, 0);
          tm_activate_powerup(g, t - T_ADDICT + 1, true, j * TT);
          g->pw_cooldown = tm_vary(g, g->pw_cool, g->pw_cool_var);   /* ResetPowerupTimer */
        }
      } else if (t == T_STAR) {
        if (near_box(g, i, j, R_BOX)) {
          tm_set(g, i, j, T_EMPTY);
          g->stars++;
          g->shake = 3;
          tm_fx(g, FX_POWER, i * TT, j * TT, 0, 0);
        }
      } else if (t == T_EXIT) {
        if (near_box(g, i, j, R_BOX) && g->mode == MODE_STAGE) {
          g->x = i * TT, g->y = j * TT, g->dx = g->dy = 0;
          g->ev_won = 1;
          tm_fx(g, FX_PORTAL, i * TT, j * TT, 0, 0);
          tm_lava_activate(g, false);
          for (int k = 0; k < PW_COUNT; k++)
            if (g->pw_time[k] > 0) tm_activate_powerup(g, k, false, 0);
          return;
        }
      } else if (tm_is_portal(t)) {
        if (near_box(g, i, j, R_BOX)) {
          portal = idx;
          int ox, oy;
          if (moving && g->portal_cell != idx && partner(g, i, j, t >= 61 ? t - 61 : t - 49, &ox, &oy)) {
            tm_fx(g, FX_PORTAL, i * TT, j * TT, 0, 0);
            tm_fx(g, FX_PORTAL, ox * TT, oy * TT, 0, 0);
            g->trail = 0;
            if (blocks_player(g, ox + g->dx, oy + g->dy)) {
              /* a wall right past the other portal: the next raycast starts inside it and stops the player on
               * the portal, facing that wall */
              g->x = ox * TT, g->y = oy * TT;
              tm_stop_at(g, ox * TT, oy * TT, true);
              g->portal_cell = tm_index(g, ox, oy) + 1;
              return;
            }
            g->x = (ox + g->dx) * TT, g->y = (oy + g->dy) * TT;
            g->skip = 1;
            g->portal_cell = 0;
            return;
          }
        }
      } else if (tm_is_tramplin(t)) {
        if (near_box(g, i, j, R_TRAMP)) {
          tramp = idx;
          if (moving && g->tramp_cell != idx) {
            int tx = (t == T_TRAMP_UR || t == T_TRAMP_DR) ? 1 : -1, ty = (t == T_TRAMP_UR || t == T_TRAMP_UL) ? 1 : -1;
            int ndx = g->dx ? 0 : tx, ndy = g->dx ? ty : 0;
            int odx = g->dx, ody = g->dy;
            g->x = i * TT;
            if (!g->running) g->y = j * TT;
            tm_land(g);
            stand(g, odx, ody);
            tm_set_dir(g, ndx, ndy);
            g->tramp_cell = idx;
            tm_fx(g, FX_JUMP, i * TT, j * TT, ndx, ndy);
          }
        }
      } else if (t == T_DOOR) {
        if (near_dot(g, i, j) && g->mode == MODE_ARCADE) {
          tm_set(g, i, j, T_EMPTY);
          g->pstate = ST_LONG;
          tm_bonus_zone(g, i, j);
        }
      } else if (t == T_ZONE_EXIT) {
        if (near_box(g, i, j, R_BOX) && g->mode == MODE_ARCADE) {
          if (!g->bonus) {
            if (!g->zone_changed) {
              g->pstate = ST_LONG;
              tm_change_zone(g);
            }
          } else {
            tm_fx(g, FX_JUMP, g->x, g->y, 0, 1);
            tm_set_dir(g, 0, 1);
            g->x = i * TT, g->y = (j + 1) * TT;
            for (int k = 0; k < TM_LANE; k++)
              if (tm_tile(g, g->lane_x + k, j) == T_ZONE_EXIT) tm_set(g, g->lane_x + k, j, T_EMPTY);
            tm_end_bonus_zone(g, i, j);
            return;
          }
        }
      } else if (t == T_TIMER_WALL || t == T_CLOSE) {
        if (near_box(g, i, j, R_BOX) && in_n < 4) in_x[in_n] = (int16_t)i, in_y[in_n] = (int16_t)j, in_n++;
      } else if (t >= T_BALL_TRIGGER && t <= 83 && (t - T_BALL_TRIGGER) % 12 == 0) {
        if (near_box(g, i, j, R_BOX)) {
          tm_set(g, i, j, T_EMPTY);
          tm_wake_ball(g, i, j);
        }
      }
    }
  if (!g->alive) return;
  /* OnTriggerExit: a close brick left becomes a wall, a timer wall left closes for 2 s */
  for (int k = 0; k < g->in_n; k++) {
    bool still = false;
    for (int m = 0; m < in_n; m++) still |= in_x[m] == g->in_x[k] && in_y[m] == g->in_y[k];
    if (still) continue;
    int x = g->in_x[k], y = g->in_y[k];
    uint8_t t = tm_tile(g, x, y);
    if (t == T_CLOSE) {
      int idx = tm_index(g, x, y);
      if (idx >= 0) g->tile[idx] = T_BRICK, g->look[idx] = (uint8_t)(g->look[idx] & 7);
      tm_fx(g, FX_BRICK, x * TT, y * TT, 0, 0);
    } else if (t == T_TIMER_WALL && !tm_timer_wall_on(g, x, y)) {
      tm_activate_timer_wall(g, x, y);
    }
  }
  g->in_n = (uint8_t)in_n;
  for (int m = 0; m < in_n; m++) g->in_x[m] = in_x[m], g->in_y[m] = in_y[m];
  g->tramp_cell = tramp;
  if (!portal) g->portal_cell = 0;
  /* trap spikes: the open cells round a trap-spikes block arm it */
  static const int8_t d4[4][2] = {{0, 1}, {-1, 0}, {1, 0}, {0, -1}};
  for (int j = cy - 1; j <= cy + 1; j++)
    for (int i = cx - 1; i <= cx + 1; i++) {
      if (tm_wallish(tm_tile(g, i, j)) || !near_box(g, i, j, R_TRAP)) continue;
      for (int k = 0; k < 4; k++)
        if (tm_tile(g, i - d4[k][0], j - d4[k][1]) == T_MSPIKES) tm_arm_trap(g, i, j, d4[k][0], d4[k][1]);
    }
  /* the shield's bounce stops at the last landing (the prevPos marker) */
  if (g->bounce && !tm_standing(g) && iabs(g->x - g->prev_x) < R_TRAP && iabs(g->y - g->prev_y) < R_TRAP) {
    g->x = g->prev_x, g->y = g->prev_y;
    g->face = g->prev_face;
    g->dx = g->dy = 0;
    g->bounce = 0;
    g->buf_timer = 0;
    g->pstate = g->running ? ST_RUN : ST_IDLE;
  }
}
