/* Tomb of the Mask's rules: the original's numbers, from its web build (see README.md and the spec kept with the
 * reference). world.c builds the stages and the arcade's climb, player.c moves the player, things.c the rest,
 * game.c steps it all 60 times a second (the original's fixed step), progress.c is the shop, masks, missions and
 * the wheel. A tile is TT units; y goes up. */
#ifndef TM_GAME_H
#define TM_GAME_H
#include <stdbool.h>
#include <stdint.h>

#define TT 4096                  /* units in a tile (0.12 of the original's units) */
#define TM_W 64                  /* the arcade's world: two 13-wide lanes 19 void columns apart, wrapping round */
#define TM_H 64                  /* ...and the rows it keeps (a ring) */
#define TM_CELLS 4096            /* also holds the biggest stage (67 x 83 = 1720 cells) */
#define TM_ACTORS 64
#define TM_ROTORS 8
#define TM_ROTOR_PARTS 40
#define TM_PLATFORMS 8
#define TM_PLAT_BRICKS 12
#define TM_SNAKES 3
#define TM_SNAKE_LINES 3
#define TM_SNAKE_LEN 24
#define TM_BALLS 6
#define TM_BALL_PATH 20
#define TM_TIMER_WALLS 12
#define TM_FX 16
#define TM_LANE 13               /* a section's width */
#define TM_GAP 19                /* the void between the arcade's lanes */

/* tiles (the original's LevelTileType) */
enum {
  T_EMPTY = 0, T_ENTER = 1, T_EXIT = 2, T_WALL = 3, T_STAR = 4, T_INNER = 5, T_DOOR = 6, T_ICE = 7, T_SPIKES = 8,
  T_CLOSE = 12, T_MONKEYS = 13, T_COIN = 19, T_BALL_TRIGGER = 23, T_BALL = 24, T_DOT = 31, T_ZONE_EXIT = 43,
  T_PORTAL_H = 49, T_TRAMP_DR = 56, T_TRAMP_DL = 57, T_PORTAL_V = 61, T_TRAMP_UR = 68, T_TRAMP_UL = 69,
  T_CANNON_L = 73, T_CANNON_R = 74, T_BAT_R = 76, T_BAT_L = 77, T_PLAT_R = 78, T_PLAT_L = 79, T_SNAKE_ENTER = 80,
  T_CANNON_U = 85, T_BAT_U = 88, T_BAT_D = 89, T_PLAT_U = 90, T_PLAT_D = 91, T_CANNON_D = 97, T_MSPIKES = 98,
  T_FISH = 102, T_ROTOR_CW = 109, T_ROTOR_CCW = 110, T_TEETH_R = 113, T_BONUS_WALL = 114, T_TEETH_L = 115,
  T_TIMER_WALL = 121, T_ADDICT = 145, T_FREEZE = 146, T_MAGNET = 147, T_BOOST = 148,
  T_BRICK = 200,                 /* a wall made later (close bricks, the zone's bricks): drawn on its own */
  T_TRIGGER = 201                /* a monkey or snake trigger's cell once its coin is taken (nothing to draw) */
};

enum { PW_SHIELD, PW_ADDICT, PW_FREEZE, PW_MAGNET, PW_BOOST, PW_COUNT, PW_NONE = 6 };
enum { MODE_ARCADE, MODE_STAGE };
enum { ST_ARRIVE = 1, ST_IDLE, ST_JUMP, ST_LONG, ST_RUN };   /* the player's look (PlayerState) */
enum { BONUS_NONE, BONUS_RUN, BONUS_PIPES };
enum { A_NONE, A_BAT, A_BULLET, A_ROCK, A_MONKEY, A_SPIKESM, A_MTRIGGER, A_STRIGGER, A_PULL };   /* A_PULL: a dot or
                                                                                          coin the magnet pulls */
enum { FX_LANDING = 1, FX_DOT, FX_COIN, FX_POWER, FX_ICE, FX_PORTAL, FX_BANG, FX_DEATH, FX_JUMP, FX_BRICK };

/* what the save keeps (save.c): the original's PlayerPrefs that matter */
typedef struct {
  uint32_t coins, best, xp;            /* xp: the original's allScore (all the points ever made) */
  uint32_t chest_wait;                 /* seconds of play before the next free chest */
  uint8_t stars[200];                  /* best stars per stage */
  uint8_t dots_paid[25];               /* a stage's all-dots reward taken, a bit per stage */
  uint8_t reached;                     /* the last stage unlocked (0..199) */
  uint8_t mask, shields, upgrades[4];  /* upgrades: coin addict, freeze, magnet, score booster */
  uint16_t masks;                      /* masks bought, a bit each (Travelboy always) */
  uint8_t mission_type[3], mission_index[3];
  uint32_t mission_progress[3];
  uint8_t missions_done[8];            /* a bit per level done, per mission type */
  uint16_t wheel_plays;
  uint8_t chest_free;                  /* the first chest is free */
  uint8_t max_zone;
  uint8_t guide;                       /* first-run guides read: 1 the arcade, 2 the stages */
  uint8_t spare[3];
} tm_progress;

typedef struct { int32_t x, y; } tm_pt;

typedef struct {
  uint8_t type, frozen, state, frame;
  int8_t dx, dy;                       /* direction */
  int16_t timer, wait;
  int32_t x, y;                        /* centre, in units */
  int32_t v;                           /* rocks: falling speed (the original's units a second, x 65536);
                                          monkeys: their sway; bats: a step's fraction */
  int32_t aux;                         /* monkeys: height above the view; spikes M: its trap's side */
} tm_actor;

typedef struct {
  uint8_t active, ccw, count, turning;
  int16_t timer;                       /* steps into a turn (it takes 7) */
  int32_t x, y;                        /* its cell */
  int8_t px[TM_ROTOR_PARTS], py[TM_ROTOR_PARTS];
  uint8_t tile[TM_ROTOR_PARTS], look[TM_ROTOR_PARTS];
} tm_rotor;

typedef struct {
  uint8_t active, count, frame, frame_timer;
  int8_t dx, dy;
  int16_t wait;
  int32_t x, y;                        /* its origin in units; bricks at origin + (bx, by) cells */
  int8_t bx[TM_PLAT_BRICKS], by[TM_PLAT_BRICKS];
} tm_platform;

typedef struct {
  int32_t x, y;                        /* the wall it comes out of */
  int8_t dx, dy;
  uint8_t n;                           /* open[i]: the cell i+1 from the wall is open */
  uint8_t open[TM_SNAKE_LEN];
} tm_snake_line;

typedef struct {
  uint8_t active, frozen, line, lines, frame, frame_timer;
  int16_t delay;                       /* warning steps before it moves */
  int32_t q;                           /* how far its head has gone along the line, units */
  int32_t end;                         /* how far it goes */
  tm_snake_line l[TM_SNAKE_LINES];
} tm_snake;

typedef struct {
  uint8_t active, awake, n, next;
  int16_t wait;
  int32_t speed;                       /* units a step */
  int32_t x, y, home_x, home_y, trig_x, trig_y;
  tm_pt path[TM_BALL_PATH];
} tm_ball;

typedef struct { int32_t x, y; int16_t left; } tm_timer_wall;

typedef struct {
  /* the world */
  uint8_t mode, lane, bonus;           /* bonus: BONUS_* while in a bonus zone */
  int16_t stage;
  int16_t w, h;                        /* a stage's size; the arcade: TM_W x TM_H, rows in a ring */
  int32_t ring_top;                    /* the arcade: one past the highest row made */
  int32_t lane_base[2];                /* the arcade: the lowest row made in each lane (below it: wall) */
  uint8_t tile[TM_CELLS];
  uint8_t look[TM_CELLS];              /* walls: style 0..6 (3 bits), section's bottom row 8, top row 16, the arcade's
                                          seam rule 64; ice and close bricks: their coin roll won 32 */
  /* the player */
  int32_t x, y;                        /* centre, in units */
  int8_t dx, dy;                       /* moving direction (0, 0: standing) */
  uint8_t face;                        /* the wall it rests against: 0 below, 1 right, 2 above, 3 left */
  uint8_t pstate, anim;                /* ST_*, frame */
  int16_t anim_timer;
  int8_t buf_dx, buf_dy, buf_timer;    /* a dash pressed while moving, for 6 steps */
  uint8_t skip, running, run_go, alive;
  int8_t ride;                         /* the platform it rides + 1, or -(rotor + 1) */
  uint32_t step_acc;
  int32_t prev_x, prev_y;              /* the last landing (revives and shield bounces) */
  uint8_t prev_face, bounce;           /* bounce: flying back to prev after a shield hit */
  int32_t trail;                       /* length of the dash trail, units */
  int16_t blink;                       /* the shield button's "none left" blink */
  int16_t in_x[4], in_y[4];            /* the close bricks and timer walls it is in (in_n of them) */
  uint8_t in_n;
  uint8_t rest_on;                     /* it stands against the closed timer wall at rest_x, rest_y */
  int16_t rest_x, rest_y;
  int32_t tramp_cell, portal_cell;     /* the tramplin/portal it is inside (index + 1), so it fires once */
  /* things */
  tm_actor a[TM_ACTORS];
  tm_rotor rotor[TM_ROTORS];
  tm_platform plat[TM_PLATFORMS];
  tm_snake snake[TM_SNAKES];
  tm_ball ball[TM_BALLS];
  tm_timer_wall tw[TM_TIMER_WALLS];
  tm_snake_line sline[6];              /* a stage's snakes, waiting for their triggers */
  /* timers (steps) */
  uint32_t tick;
  int16_t t_cannon, t_fish, t_rotor;
  int16_t fish_puff, fish_frame, fish_frame_timer;
  /* the lava */
  uint8_t lava_on, lava_back, reviving;
  int32_t lava_y;                      /* its top, units (and lava_sub 256ths) */
  int32_t lava_sub;
  int32_t lava_speed, lava_max;        /* the original's units a second, x 2^24 (0: no lava) */
  /* powerups */
  int32_t pw_time[PW_COUNT], pw_full[PW_COUNT];
  uint8_t pw_next;                     /* the next one to place (PW_NONE: none waiting) */
  int32_t pw_cooldown;                 /* steps; counts while > 0 */
  int16_t addict_left;                 /* coin addict: spots still to turn into coins */
  uint8_t pw_spot;                     /* a powerup was put in a section, at row pw_spot_y */
  int32_t pw_spot_y;
  uint8_t mult;                        /* the score multiplier: the level, +5 boosted */
  /* the run */
  uint32_t score;
  uint16_t coins, dots, dots_all, stars, stars_all;
  uint8_t revives, used_revive, played;
  /* the arcade's maker (GameController) */
  uint8_t zone, zone_len, in_zone, zone_changed;
  uint8_t style, styles[4];            /* wall style now; the order of styles 1..4 */
  uint8_t colors, color_order[13];     /* the colour pair now; the pairs' order */
  uint8_t slot_type[6], chances[6];    /* the zone's six section slots and their weights */
  uint8_t bonus_count, bonus_len, bonus_active, bonus_left;
  uint8_t want_platforms, want_tramplins, want_portals, want_bonus, snake_mode, snakes_left;
  uint8_t zone_monkeys, zone_platforms, zone_snakes;
  int32_t cd_platforms, cd_tramplins, cd_portals, cd_bonus, cd_monkeys, cd_snakes;   /* steps */
  int32_t snake_y;                     /* the snake lines start above this row (units) */
  int32_t build_y;                     /* the next section's bottom row */
  int16_t lane_x;                      /* the lane's first column */
  uint16_t order[517];                 /* each type's sections in their "stack" order (used ones go to the back) */
  /* the mask's effects */
  int32_t coin_chance, bonus_coin_chance;   /* x 65536 */
  int32_t pw_cool, pw_cool_var, bonus_cool, bonus_cool_var;   /* steps */
  int16_t pw_weight[5];
  uint8_t no_snakes, no_monkeys;
  /* the view */
  int32_t cam_x, cam_y;                /* the view's centre, units */
  int32_t cam_slide_speed, cam_target_x, cam_lead;
  uint8_t cam_slide;
  int32_t cam_ease;                    /* x 1024: grows to 1 in a second after each landing */
  int16_t shake;
  int16_t gate_frame;                  /* the arcade's gates: frames into their closing */
  int16_t color_wait;                  /* steps until the zone's new colours */
  int16_t cannon_flash;                /* steps the cannons show their shot */
  /* effects (drawn only) */
  tm_actor fx[TM_FX];
  /* what happened, for the screens */
  uint8_t ev_dead, ev_won, ev_zone, ev_power, ev_reward;
  uint32_t rng;
  tm_progress p;
} tm_game;

/* the original's settings (RSettings.InitDefaults) */
extern const uint32_t tm_level_xp[20];
extern const uint16_t tm_upgrade_cost[9];
extern const uint8_t tm_powerup_secs[4][10];
extern const uint16_t tm_mask_cost[16];
extern const uint8_t tm_mask_level[16];
extern const char *const tm_mask_name[16];
extern const uint8_t tm_mask_feature[16][3][2];
extern const uint16_t tm_mission_goal[8][7];
extern const uint8_t tm_mission_levels[8];
extern const uint8_t tm_mission_reward[7];
extern const uint16_t tm_colors[2][13];

void tm_new_progress(tm_progress *p);
bool tm_progress_ok(const tm_progress *p);
unsigned tm_level(uint32_t xp);
uint32_t tm_rand(tm_game *g);
int tm_range(tm_game *g, int lo, int hi);             /* Random.Range(lo, hi): lo..hi-1 */

void tm_start(tm_game *g, int stage, uint32_t seed);  /* stage -1: the arcade */
void tm_step(tm_game *g, int dir, bool shield);        /* dir 0 left 1 right 2 up 3 down, -1 none */
void tm_end_run(tm_game *g);                           /* the run's results into the progress */
bool tm_revive(tm_game *g);                            /* 200 coins: back to the last landing */
int tm_revives_left(const tm_game *g);
uint8_t tm_tile(const tm_game *g, int x, int y);
uint8_t tm_look(const tm_game *g, int x, int y);
bool tm_wallish(uint8_t t);
bool tm_solid(const tm_game *g, int x, int y);
bool tm_timer_wall_on(const tm_game *g, int x, int y);
int tm_cell(int32_t u);                                /* the cell of a coordinate in units */
bool tm_standing(const tm_game *g);
int32_t tm_lava_shown(const tm_game *g);              /* the lava's top as drawn, units */

/* the shop, masks, missions and the wheel (progress.c) */
bool tm_buy_shield(tm_progress *p);
bool tm_buy_upgrade(tm_progress *p, int which);
bool tm_mask_open(const tm_progress *p, int m);
bool tm_buy_mask(tm_progress *p, int m);
bool tm_mission_ready(const tm_progress *p, int slot);
uint32_t tm_mission_goal_of(const tm_progress *p, int slot);
uint32_t tm_mission_shown(const tm_progress *p, int slot);
bool tm_claim_mission(tm_progress *p, int slot, uint32_t *rng);
void tm_mission_add(tm_progress *p, int type, uint32_t amount);
void tm_mission_new_run(tm_progress *p);
bool tm_chest_free(const tm_progress *p);
int tm_mission_kind(int type);                         /* 0 adds up, 1 one run (at least), 2 exactly */
typedef struct {
  int32_t angle;                       /* the wheel's turn, 1024ths of a degree (it turns the negative way) */
  int32_t speed;                       /* 65536ths of a degree a step */
  int delay;                           /* steps before it slows */
  int peg, peg_angle;                  /* stopping on a peg: it rolls back */
  int sector, kind, amount;            /* the result: sector 0..7; kind 0 coins, 1 a mask, 3 a shield */
  bool spinning, done;
} tm_wheel;
#define TM_CHEST_COST 200
#define TM_CHEST_WAIT 14400            /* seconds of play between free chests */
bool tm_wheel_start(tm_progress *p, tm_wheel *w, uint32_t *rng);   /* false: no free chest and too few coins */
void tm_wheel_step(tm_progress *p, tm_wheel *w, uint32_t *rng);
#endif
