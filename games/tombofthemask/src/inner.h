/* What world.c, player.c, things.c and game.c share. */
#ifndef TM_INNER_H
#define TM_INNER_H
#include <string.h>
#include "game.h"

#define SEC(s) ((int32_t)((s) * 60.0f + 0.5f))              /* seconds to steps */
#define UNITS(u) ((int32_t)((u) / 0.12f * TT + 0.5f))        /* the original's units to ours */
#define HALF (TT / 2)
#define LAVA_BELOW (TT * 21 / 2)                             /* 10.5 tiles */

static inline int32_t iabs(int32_t v) { return v < 0 ? -v : v; }

/* Random.value as 0..65535; base + Random.value * var */
int32_t tm_value16(tm_game *g);
int32_t tm_vary(tm_game *g, int32_t base, int32_t var);

/* the world (world.c) */
int tm_index(const tm_game *g, int x, int y);                  /* -1 outside */
void tm_set(tm_game *g, int x, int y, uint8_t t);
bool tm_is_cannon(uint8_t t);
bool tm_is_portal(uint8_t t);
bool tm_is_tramplin(uint8_t t);
bool tm_is_rotor_part(uint8_t t);
void tm_build_arcade_start(tm_game *g);
void tm_build_more(tm_game *g);                                /* sections up to 20 tiles above the player */
void tm_load_stage(tm_game *g, int s);
void tm_set_zone(tm_game *g, int z);
void tm_change_zone(tm_game *g);
void tm_bonus_zone(tm_game *g, int door_x, int door_y);
void tm_close_bricks(tm_game *g, int x, int y, int n);
void tm_end_bonus_zone(tm_game *g, int exit_x, int exit_y);
void tm_change_colors(tm_game *g);
int tm_lane_centre(const tm_game *g);                          /* the view's x for the lane, units */

/* things (things.c) */
tm_actor *tm_spawn(tm_game *g, uint8_t type, int32_t x, int32_t y, int dx, int dy);
void tm_fx(tm_game *g, uint8_t kind, int32_t x, int32_t y, int dx, int dy);
void tm_things_step(tm_game *g);
void tm_things_touch(tm_game *g);                              /* hazards against the player */
void tm_add_monkeys(tm_game *g, int count, int32_t x);
void tm_add_snake_lines(tm_game *g, const tm_snake_line *lines, int n);
void tm_snake_lines_here(tm_game *g);                         /* the arcade's snake lines (UpdateSnake) */
void tm_rotate(tm_game *g);                                    /* every 2.1 s: the rotors turn */
void tm_activate_timer_wall(tm_game *g, int x, int y);
void tm_arm_trap(tm_game *g, int x, int y, int dx, int dy);
void tm_wake_ball(tm_game *g, int x, int y);
void tm_remove_monkeys(tm_game *g);
void tm_remove_snakes(tm_game *g);
bool tm_platform_at(const tm_game *g, int32_t x, int32_t y, int *which);
void tm_ball_checkpoint(tm_game *g);

/* the player (player.c) */
void tm_player_step(tm_game *g);
void tm_player_triggers(tm_game *g);
void tm_swipe(tm_game *g, int dx, int dy);
void tm_hurt(tm_game *g, int32_t kx, int32_t ky);              /* a hit from something at (kx, ky) */
void tm_stop_at(tm_game *g, int32_t x, int32_t y, bool save);
void tm_land(tm_game *g);
void tm_set_dir(tm_game *g, int dx, int dy);

/* game.c */
void tm_activate_powerup(tm_game *g, int which, bool on, int32_t y);
void tm_lava_activate(tm_game *g, bool on);
void tm_reward(tm_game *g, int additional, int coins);
void tm_save_prev(tm_game *g);
#endif
