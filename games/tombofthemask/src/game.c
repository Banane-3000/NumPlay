/* A run, step by step (the original's GameController: StartGame, FixedUpdate, UpdateLava, UpdateTimers,
 * ActivatePowerup, SaveGame, SaveGameResults, StageCompleted). 60 steps a second, as its fixed step. */
#include "inner.h"
#include "data.h"

/* ------------------------------------------------------------------ random numbers (no libc rand) */
uint32_t tm_rand(tm_game *g) {
  uint32_t x = g->rng;
  x ^= x << 13, x ^= x >> 17, x ^= x << 5;
  return g->rng = x ? x : 0x9E3779B9u;
}
int tm_range(tm_game *g, int lo, int hi) { return hi > lo ? lo + (int)(tm_rand(g) % (uint32_t)(hi - lo)) : lo; }

/* ------------------------------------------------------------------ rewards, landings */
/* DotController.AplyReward: the points (the multiplier, plus 1 for ice) and the coins, kept at once */
void tm_reward(tm_game *g, int additional, int coins) {
  g->score += (uint32_t)(additional + g->mult);
  if (coins) {
    g->coins = (uint16_t)(g->coins + coins);
    g->p.coins += (uint32_t)coins;
  }
}

/* SavePrevPosition: the last landing (revives and shield bounces come back to it); light balls follow it */
void tm_save_prev(tm_game *g) {
  g->prev_x = g->x, g->prev_y = g->y, g->prev_face = g->face;
  tm_ball_checkpoint(g);
}

/* ActivateLava: off until the first dash; never in a bonus zone; the cannons and fishes wait a period more */
void tm_lava_activate(tm_game *g, bool on) {
  if (on && g->bonus) return;
  if (g->lava_on != on) {
    g->lava_on = on;
    g->t_cannon = (int16_t)(g->t_cannon + SEC(1.1f));
    g->t_fish = (int16_t)(g->t_fish + SEC(2.1f));
  }
}

/* ChangeDotsToCoinsFromPosition: the dots from half a tile below the pickup up turn into coins, the lowest first */
static void dots_to_coins(tm_game *g, int32_t y) {
  int y0 = tm_cell(y - HALF + TT - 1);
  int x0 = g->mode == MODE_STAGE ? 0 : g->lane_x, x1 = g->mode == MODE_STAGE ? g->w : g->lane_x + TM_LANE;
  int y1 = g->mode == MODE_STAGE ? g->h : g->ring_top;
  for (int j = y0; j < y1 && g->addict_left > 0; j++)
    for (int i = x0; i < x1 && g->addict_left > 0; i++)
      if (tm_tile(g, i, j) == T_DOT) {
        tm_set(g, i, j, T_COIN);
        g->addict_left--;
      }
}

/* ActivatePowerup: on with its time (coin addict: a count of dots), or off */
void tm_activate_powerup(tm_game *g, int which, bool on, int32_t y) {
  unsigned up = which >= PW_ADDICT ? g->p.upgrades[which - 1] : 0;
  if (on) {
    g->ev_power = (uint8_t)(which + 1);
    switch (which) {
      case PW_ADDICT:
        g->addict_left = tm_powerup_secs[0][up];
        dots_to_coins(g, y);
        return;
      case PW_FREEZE:
        g->snake_mode = 0;
        tm_remove_monkeys(g);
        break;
      case PW_BOOST: g->mult = (uint8_t)(tm_level(g->p.xp) + 5); break;
      default: break;
    }
    int32_t t = which == PW_SHIELD ? SEC(30) : SEC(tm_powerup_secs[which - 1][up]);
    g->pw_time[which] = g->pw_full[which] = t;
  } else {
    if (which == PW_BOOST) g->mult = (uint8_t)tm_level(g->p.xp);
    g->pw_time[which] = 0;
  }
}

/* the shield key (ProcessDblTap): one from the inventory, or a blink when there is none */
static void use_shield(tm_game *g) {
  if (!g->alive || g->ev_won || g->pw_time[PW_SHIELD] > 0) return;
  if (g->p.shields >= 1) {
    g->p.shields--;
    tm_activate_powerup(g, PW_SHIELD, true, 0);
  } else {
    g->blink = 30;
  }
}

/* ------------------------------------------------------------------ the masks' effects (StartGame) */
static void apply_mask(tm_game *g) {
  int64_t coin = 3277, bonus = 19661;   /* CoinChance 0.05, BonusCoinChance 0.3, x 65536 */
  int64_t pw = SEC(5), pwv = SEC(5), bz = SEC(40), bzv = SEC(20);
  int w[5] = {0, 10, 10, 10, 10};
  g->no_snakes = g->no_monkeys = 0;
  for (int k = 0; k < 3; k++) {
    int f = tm_mask_feature[g->p.mask][k][0], v = tm_mask_feature[g->p.mask][k][1];
    switch (f) {
      case 1: coin = coin * (100 + v) / 100, bonus = bonus * (100 + v) / 100; break;
      case 2: case 3: case 4: case 5:
        pw = pw * (100 - v) / 100, pwv = pwv * (100 - v) / 100, w[f - 1] = w[f - 1] * (100 + v) / 100;
        break;
      case 6: bz = bz * (100 - v) / 100, bzv = bzv * (100 - v) / 100; break;
      case 7: g->no_snakes = 1; break;
      case 8: g->no_monkeys = 1; break;
      default: break;
    }
  }
  g->coin_chance = (int32_t)coin, g->bonus_coin_chance = (int32_t)bonus;
  g->pw_cool = (int32_t)pw, g->pw_cool_var = (int32_t)pwv, g->bonus_cool = (int32_t)bz, g->bonus_cool_var = (int32_t)bzv;
  for (int i = 0; i < 5; i++) g->pw_weight[i] = (int16_t)w[i];
}

/* ------------------------------------------------------------------ a new run */
void tm_start(tm_game *g, int stage, uint32_t seed) {
  tm_progress keep = g->p;
  memset(g, 0, sizeof *g);
  g->p = keep;
  g->rng = seed ? seed : 0x2545F491u;
  g->mode = stage < 0 ? MODE_ARCADE : MODE_STAGE;
  g->stage = (int16_t)stage;
  g->alive = 1;
  g->pstate = ST_ARRIVE;
  g->pw_next = PW_NONE;
  g->mult = (uint8_t)tm_level(g->p.xp);
  g->t_cannon = SEC(1.1f), g->t_fish = SEC(2.1f), g->t_rotor = SEC(2.1f - 0.55f);
  g->lava_y = -LAVA_BELOW;
  apply_mask(g);
  g->pw_cooldown = tm_vary(g, g->pw_cool, g->pw_cool_var);
  g->bonus_left = 2;
  g->cd_bonus = tm_vary(g, g->bonus_cool, g->bonus_cool_var);
  tm_mission_new_run(&g->p);
  if (g->mode == MODE_ARCADE) {
    g->lava_speed = (int32_t)(0.19 * (1 << 24)), g->lava_max = (int32_t)(0.38 * (1 << 24));
    for (int i = 0; i < 4; i++) g->styles[i] = (uint8_t)(i + 1);
    for (int i = 0; i < 13; i++) g->color_order[i] = (uint8_t)i;
    g->style = 0, g->colors = 0;
    tm_set_zone(g, 0);
    g->zone_len = 10;
    tm_mission_add(&g->p, 1, 1);
    tm_build_arcade_start(g);
    g->x = 6 * TT, g->y = 0;
    g->cam_target_x = tm_lane_centre(g);
  } else {
    int lava = tm_stages[stage].extra;
    g->lava_speed = g->lava_max = (int32_t)((int64_t)lava * (1 << 24) / 100);
    g->colors = (uint8_t)((stage / 5) % 13);
    tm_load_stage(g, stage);
  }
  g->cam_x = g->x, g->cam_y = g->y;
  tm_save_prev(g);
}

/* ------------------------------------------------------------------ the view (FixedUpdate) */
static void camera(tm_game *g) {
  if (g->mode == MODE_STAGE) {
    g->cam_x += (g->x - g->cam_x) / 6;   /* Vector2.Lerp(camera, player, dt * 10) */
    g->cam_y += (g->y - g->cam_y) / 6;
    return;
  }
  if (g->cam_slide) {
    /* to the bonus lane: from 1 unit a second, +10 each second, at most 20 */
    g->cam_slide_speed += UNITS(10.0f) / 60;
    if (g->cam_slide_speed > UNITS(20.0f)) g->cam_slide_speed = UNITS(20.0f);
    int32_t d = g->cam_slide_speed / 60, left = g->cam_target_x - g->cam_x;
    if (iabs(left) <= d) g->cam_x = g->cam_target_x, g->cam_slide = 0;
    else g->cam_x += left > 0 ? d : -d;
  } else {
    g->cam_x += (g->cam_target_x - g->cam_x) / 3;   /* Lerp(x, target, dt * 20) */
  }
  if (g->running) {
    /* the bonus run: the view leads the player by up to 0.6 units */
    if (g->run_go) {
      g->cam_lead += UNITS(1.0f) / 60;
      if (g->cam_lead > UNITS(0.6f)) g->cam_lead = UNITS(0.6f);
    } else if (g->cam_lead > 0) {
      g->cam_lead -= UNITS(2.0f) / 60;
      if (g->cam_lead < 0) g->cam_lead = 0;
    }
    g->cam_y = g->y + g->cam_lead;
  } else {
    /* after a landing the view eases in: Lerp(y, player, 0.15 + 0.85 t^2), t up to 1 in a second */
    if (g->cam_ease < 1024) g->cam_ease = g->cam_ease + 17 > 1024 ? 1024 : g->cam_ease + 17;
    int32_t p = 154 + (int32_t)((int64_t)870 * g->cam_ease * g->cam_ease >> 20);
    g->cam_y += (int32_t)((int64_t)(g->y - g->cam_y) * p >> 10);
  }
}

/* ------------------------------------------------------------------ the lava (UpdateLava) */
#define LAVA_FALL ((int32_t)(3.0 / 0.12 * TT / 60 + 0.5))   /* 3 units a second */

int32_t tm_lava_shown(const tm_game *g) {
  /* drawn (and deadly) at whole hundredths of a unit: 1024/3 units */
  int32_t q = g->lava_y * 3 / 1024;
  return q * 1024 / 3;
}

static void lava(tm_game *g) {
  if (!g->lava_max) return;
  if (g->reviving == 1) {
    /* ResetLavaAndResurectPlayer: down to 10.5 tiles below (and no lower than at the start) */
    int32_t floor = g->y - LAVA_BELOW;
    if (floor < -LAVA_BELOW) floor = -LAVA_BELOW;
    g->lava_sub = 0;
    if (g->lava_y > floor) g->lava_y -= LAVA_FALL;
    if (g->lava_y <= floor) g->lava_y = floor, g->reviving = 2;
    return;
  }
  if (g->lava_back) {
    /* InvertLava: down to 10.5 tiles below the player (17 in a bonus zone) */
    int32_t floor = g->y - (g->bonus ? 17 * TT : LAVA_BELOW);
    g->lava_sub = 0;
    if (g->lava_y > floor) g->lava_y -= LAVA_FALL;
    if (g->lava_y <= floor) g->lava_back = 0;
    return;
  }
  if (!g->lava_on) return;
  int32_t gap = g->y - g->lava_y;
  int32_t mult = g->pw_time[PW_FREEZE] > 0 ? 0 : gap < 3 * TT ? 2048 + (int32_t)((int64_t)2048 * gap / (3 * TT)) : 4096;
  /* the rise: speed x dt x mult, in 256ths of a unit */
  int64_t rise = (int64_t)g->lava_speed * 5 / 576 * mult / 4096;
  int64_t sub = (int64_t)g->lava_y * 256 + g->lava_sub + rise;
  g->lava_y = (int32_t)(sub >> 8), g->lava_sub = (int32_t)(sub & 255);
  if (g->lava_speed < g->lava_max) {
    g->lava_speed += (int32_t)((239075 - g->lava_speed / 30) / 60);   /* + dt (0.01425 - speed / 30) */
    if (g->lava_speed > g->lava_max) g->lava_speed = g->lava_max;
  }
  if (g->y - g->lava_y > 17 * TT) g->lava_y = g->y - 17 * TT, g->lava_sub = 0;
}

/* ------------------------------------------------------------------ the timers (UpdateTimers) */
static void cannons_shoot(tm_game *g) {
  int x0, x1, y0, y1;
  if (g->mode == MODE_STAGE) x0 = 0, x1 = g->w, y0 = 0, y1 = g->h;
  else x0 = g->lane_x, x1 = g->lane_x + TM_LANE, y0 = g->ring_top - TM_H, y1 = g->ring_top;
  int cy = tm_cell(g->y);
  if (y0 < cy - 24) y0 = cy - 24;
  if (y1 > cy + 25) y1 = cy + 25;
  for (int j = y0; j < y1; j++)
    for (int i = x0; i < x1; i++) {
      uint8_t t = tm_tile(g, i, j);
      if (!tm_is_cannon(t)) continue;
      int dx = t == T_CANNON_R ? 1 : t == T_CANNON_L ? -1 : 0, dy = t == T_CANNON_U ? 1 : t == T_CANNON_D ? -1 : 0;
      tm_spawn(g, A_BULLET, (i + dx) * TT, (j + dy) * TT, dx, dy);
    }
  g->cannon_flash = 12;
}

/* AddPowerup: a type by the masks' weights, put on the next section's coin spot */
static void add_powerup(tm_game *g) {
  int total = 0;
  for (int i = 1; i < 5; i++) total += g->pw_weight[i];
  if (total <= 0) return;
  int r = tm_range(g, 0, total), k = 1;
  while (k < 4 && r >= g->pw_weight[k]) r -= g->pw_weight[k++];
  g->pw_next = (uint8_t)k;
}

static void timers(tm_game *g) {
  bool frozen = g->pw_time[PW_FREEZE] > 0;
  if (!g->zone_changed)
    for (int i = 0; i < PW_COUNT; i++)
      if (g->pw_time[i] > 0 && --g->pw_time[i] <= 0) tm_activate_powerup(g, i, false, 0);
  if (--g->t_rotor <= 0) {
    g->t_rotor = (int16_t)(g->t_rotor + SEC(2.1f));
    tm_rotate(g);
  }
  bool shoot = g->lava_on && !frozen;
  if (--g->t_cannon <= 0) {
    g->t_cannon = (int16_t)(g->t_cannon + SEC(1.1f));
    if (shoot) cannons_shoot(g);
  }
  if (--g->t_fish <= 0) {
    g->t_fish = (int16_t)(g->t_fish + SEC(2.1f));
    if (shoot) g->fish_puff = SEC(0.8f), g->fish_frame_timer = 0;
  }
  if (g->mode != MODE_ARCADE || !g->lava_on || g->bonus || g->zone_changed) return;
  if (g->cd_bonus > 0 && --g->cd_bonus <= 0) g->want_bonus = 1;
  if (g->cd_platforms > 0 && --g->cd_platforms <= 0) g->want_platforms = 1;
  if (g->cd_tramplins > 0 && --g->cd_tramplins <= 0) g->want_tramplins = 1;
  if (g->cd_portals > 0 && --g->cd_portals <= 0) g->want_portals = 1;
  if (g->pw_cooldown > 0 && !frozen && g->pw_time[PW_MAGNET] <= 0 && g->pw_time[PW_BOOST] <= 0 && --g->pw_cooldown <= 0)
    add_powerup(g);
  if (!g->no_monkeys && g->cd_monkeys > 0 && g->in_zone < g->zone_len && !frozen && --g->cd_monkeys <= 0) {
    static const uint8_t cd[4] = {0, 20, 10, 5}, var[4] = {0, 10, 10, 10};
    int z = g->zone > 3 ? 3 : g->zone;
    g->cd_monkeys = tm_vary(g, SEC(cd[z]), SEC(var[z]));
    tm_add_monkeys(g, 0, g->cam_x);
  }
  if (!g->no_snakes && !g->snake_mode && g->cd_snakes > 0 && !frozen && --g->cd_snakes <= 0) {
    g->snake_mode = 1;
    g->snake_y = g->build_y * TT;
    g->snakes_left = 1;
  }
}

/* a powerup left behind (more than 6 units below) is lost: the cooldown starts again */
static void powerup_lost(tm_game *g) {
  if (!g->pw_spot) return;
  bool there = false;
  for (int i = 0; i < TM_LANE && !there; i++) {
    uint8_t t = tm_tile(g, g->lane_x + i, g->pw_spot_y);
    there = t >= T_ADDICT && t <= T_BOOST;
  }
  if (!there) {
    g->pw_spot = 0;   /* taken (the pickup restarts the cooldown) or gone */
    if (g->pw_cooldown <= 0) g->pw_cooldown = tm_vary(g, g->pw_cool, g->pw_cool_var);
  } else if (g->y - g->pw_spot_y * TT > UNITS(6.0f)) {
    for (int i = 0; i < TM_LANE; i++) {
      uint8_t t = tm_tile(g, g->lane_x + i, g->pw_spot_y);
      if (t >= T_ADDICT && t <= T_BOOST) tm_set(g, g->lane_x + i, g->pw_spot_y, T_EMPTY);
    }
    g->pw_spot = 0;
    g->pw_cooldown = tm_vary(g, g->pw_cool, g->pw_cool_var);
  }
}

/* the magnet (DotController): dots and coins within 2.5 tiles fly to the player, 30% of the way each step */
static void magnet(tm_game *g) {
  if (g->alive && g->pw_time[PW_MAGNET] > 0) {
    int cx = tm_cell(g->x), cy = tm_cell(g->y);
    for (int j = cy - 3; j <= cy + 3; j++)
      for (int i = cx - 3; i <= cx + 3; i++) {
        uint8_t t = tm_tile(g, i, j);
        if (t != T_DOT && t != T_COIN) continue;
        int64_t dx = g->x - i * TT, dy = g->y - j * TT;
        if (dx * dx + dy * dy > (int64_t)(5 * TT / 2) * (5 * TT / 2)) continue;
        tm_actor *a = tm_spawn(g, A_PULL, i * TT, j * TT, 0, 0);
        if (!a) return;
        a->aux = t;
        tm_set(g, i, j, T_EMPTY);
      }
  }
  for (int k = 0; k < TM_ACTORS; k++) {
    tm_actor *a = &g->a[k];
    if (a->type != A_PULL) continue;
    a->x += (int32_t)((int64_t)(g->x - a->x) * 3 / 10);
    a->y += (int32_t)((int64_t)(g->y - a->y) * 3 / 10);
    int64_t dx = g->x - a->x, dy = g->y - a->y;
    if (g->alive && dx * dx + dy * dy < (int64_t)2219 * 2219) {
      g->dots++;
      tm_reward(g, 0, a->aux == T_COIN);
      tm_fx(g, a->aux == T_COIN ? FX_COIN : FX_DOT, a->x, a->y, 0, 0);
      a->type = A_NONE;
    }
  }
}

/* the shared animations: coins, fishes, the cannons' flash, the gates */
static void animate(tm_game *g) {
  if (g->cannon_flash > 0) g->cannon_flash--;
  if (g->pw_time[PW_FREEZE] > 0 && g->fish_puff) g->fish_puff = 0;
  if (++g->fish_frame_timer >= 6) {
    g->fish_frame_timer = 0;
    if (g->fish_puff > 0) {
      if (g->fish_frame < 3) g->fish_frame++;
    } else if (g->fish_frame > 0) {
      g->fish_frame--;
    }
  }
  if (g->fish_puff > 0 && --g->fish_puff == 0 && g->fish_frame > 2) g->fish_frame = 2;
  if (g->gate_frame < 13 && g->tick % 6 == 0) g->gate_frame++;
  if (g->blink > 0) g->blink--;
  if (g->shake > 0 && g->tick % 2 == 0) g->shake--;
  if (g->color_wait > 0 && --g->color_wait == 0) tm_change_colors(g);
  /* the player's frames: 10 a second (20 running) */
  if (++g->anim_timer >= (g->pstate == ST_RUN ? 3 : 6)) {
    g->anim_timer = 0;
    g->anim++;
    if (g->pstate == ST_ARRIVE && g->anim >= 8) g->pstate = ST_IDLE, g->anim = 0;
  }
}

/* ------------------------------------------------------------------ a step */
void tm_step(tm_game *g, int dir, bool shield) {
  static const int8_t dirs[4][2] = {{-1, 0}, {1, 0}, {0, 1}, {0, -1}};
  g->tick++;
  g->ev_power = 0;
  if (shield) use_shield(g);
  if (g->buf_timer > 0) g->buf_timer--;
  if (dir >= 0 && dir < 4 && g->alive && !g->reviving) tm_swipe(g, dirs[dir][0], dirs[dir][1]);
  tm_player_step(g);
  tm_things_step(g);
  tm_player_triggers(g);
  tm_things_touch(g);
  magnet(g);
  tm_build_more(g);
  camera(g);
  lava(g);
  if (g->alive && !g->ev_won && g->lava_max && tm_lava_shown(g) > g->y - HALF) tm_hurt(g, g->x, g->lava_y);
  if (g->reviving == 2) {
    /* PlayerController.Resurect: back at the last landing */
    g->reviving = 0;
    g->alive = 1, g->ev_dead = 0;
    g->x = g->prev_x, g->y = g->prev_y, g->face = g->prev_face;
    g->dx = g->dy = 0, g->bounce = 0, g->buf_timer = 0, g->ride = 0;
    g->pstate = g->running ? ST_RUN : ST_IDLE, g->anim = 0;
  }
  timers(g);
  if (g->mode == MODE_ARCADE) {
    tm_snake_lines_here(g);
    powerup_lost(g);
  }
  animate(g);
}

/* ------------------------------------------------------------------ the end of a run */
int tm_revives_left(const tm_game *g) { return (g->mode == MODE_STAGE ? 1 : 3) - g->revives; }

/* SaveGame: 200 coins; bats and light balls near the player and teeth at its height go, monkeys and snakes too;
 * the lava falls back, then the player comes back at its last landing */
bool tm_revive(tm_game *g) {
  if (g->alive || tm_revives_left(g) <= 0 || g->p.coins < 200) return false;
  g->p.coins -= 200;
  g->revives++, g->used_revive = 1;
  for (int i = 0; i < TM_ACTORS; i++) {
    tm_actor *a = &g->a[i];
    int64_t dx = a->x - g->x, dy = a->y - g->y;
    if (a->type == A_BAT && dx * dx + dy * dy < (int64_t)(2 * TT) * (2 * TT)) tm_fx(g, FX_BANG, a->x, a->y, 0, 0), a->type = A_NONE;
  }
  for (int b = 0; b < TM_BALLS; b++) {
    tm_ball *bl = &g->ball[b];
    int64_t dx = bl->x - g->x, dy = bl->y - g->y;
    if (bl->active && dx * dx + dy * dy < (int64_t)(2 * TT) * (2 * TT))
      bl->x = bl->home_x, bl->y = bl->home_y, bl->awake = 0, bl->next = bl->n = 0;
  }
  int cy = tm_cell(g->y);
  int x0 = g->mode == MODE_STAGE ? 0 : g->lane_x, x1 = g->mode == MODE_STAGE ? g->w : g->lane_x + TM_LANE;
  for (int j = cy - 1; j <= cy + 1; j++)
    for (int i = x0; i < x1; i++) {
      uint8_t t = tm_tile(g, i, j);
      if (t == T_TEETH_R || t == T_TEETH_L) tm_set(g, i, j, T_EMPTY), tm_fx(g, FX_BANG, i * TT, j * TT, 0, 0);
    }
  tm_remove_monkeys(g);
  tm_remove_snakes(g);
  g->lava_back = 0;
  g->reviving = g->lava_max ? 1 : 2;   /* (no lava: nothing to wait for) */
  return true;
}

/* SaveGameResults (and StageCompleted for a cleared stage): the best, the points into the level, the missions,
 * a stage's stars and its all-dots reward */
void tm_end_run(tm_game *g) {
  if (g->played) return;
  g->played = 1;
  tm_progress *p = &g->p;
  if (g->mode == MODE_ARCADE) {
    if (g->score > p->best) p->best = g->score;
    tm_mission_add(p, 6, g->score);
  }
  p->xp += g->score;
  tm_mission_add(p, 2, g->dots);
  tm_mission_add(p, 3, g->stars);
  if (g->mode == MODE_STAGE && g->ev_won) {
    int s = g->stage;
    if (g->stars > p->stars[s]) p->stars[s] = (uint8_t)g->stars;
    if (s + 1 > p->reached && s + 1 < 200) p->reached = (uint8_t)(s + 1);
    bool all = g->dots >= g->dots_all;
    if (all && !(p->dots_paid[s >> 3] >> (s & 7) & 1)) {
      p->dots_paid[s >> 3] |= (uint8_t)(1 << (s & 7));
      if (s % 3 == 2) {
        if (p->shields < 10) p->shields++;
      } else {
        p->coins += 25;
      }
      g->ev_reward = (uint8_t)(s % 3 == 2 ? 2 : 1);
    }
    if (all) tm_mission_add(p, 5, 1);
    tm_mission_add(p, 0, (uint32_t)s + 1);
    if (!g->used_revive) tm_mission_add(p, 4, (uint32_t)s + 1);
  }
}
