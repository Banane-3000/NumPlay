/* The mart (game.c): the web game's GameState and Player, stepped 60 times a second.
 *
 * Everything happens by walking: the monkey picks up food standing by a plant, an animal or a machine, puts it on
 * the shelf for it, takes the money customers leave at the cash box, and pays for a place by standing on its pad.
 * Customers walk the game's path grid to the shelves they want and queue at the cash box; workers keep shelves
 * full, tire and sleep; animals and machines tire too; a troublemaker throws food off a shelf. The numbers, the
 * timers (in the game's seconds, here ticks) and the order things happen in are the game's. */
#ifndef MM_GAME_H
#define MM_GAME_H
#include <stdbool.h>
#include <stdint.h>
#include "data.h"

#define MM_TPS 60                   /* steps a second */
#define MM_ACTORS 17                /* you, 6 staff, the troublemaker, the manager, 8 customers */
#define MM_CARRY 12
#define MM_DROPPED 20               /* LIMIT_DROPPED */
#define MM_TIMERS 48
#define MM_PATH 96
#define MM_ALERTS 16
#define MM_FLY 15                   /* food flying to or from a monkey (foodAnimMax) */

/* actor slots */
enum { A_ME, A_CASHIER, A_ASSIST2, A_ASSIST3, A_ASSIST4, A_ASSIST5, A_ASSIST6, A_ROWDY, A_MANAGER, A_BUYER0 };
/* roles (global.lua) */
enum { R_BUYER, R_WORKER, R_CASHIER, R_STOCKMAN, R_ROWDY, R_MANAGER };
/* stageAction */
enum { S_IDLE, S_GO_BASKET, S_PICKING, S_GO_PAY, S_PACKING, S_EXIT, S_SLEEP, S_WAKEUP, S_CRAZY };
/* a stockman's brainState (Player.lua) */
enum { B_IDLE, B_GO_ZAVOD, B_PICKING, B_GO_SKLAD, B_DROPPING, B_NEXT_TASK, B_NOT_FOOD, B_NEXT_ZAVOD, B_GO_TRASH,
       B_GO_RELAX, B_GO_START, B_GO_SLEEP };
/* what an actor shows over its head */
enum { C_NONE, C_WISH, C_PAY, C_HAPPY, C_SLEEP, C_CRAZY, C_AWAY };
/* its body's animation (dodik.script): idle/run 1 empty-handed, 2 carrying, 3 with a basket, 4 leaving */
enum { AM_IDLE, AM_RUN, AM_SLEEP, AM_WAKE, AM_CRAZY, AM_THROW, AM_DILER };

typedef struct {
  uint8_t active, selling;
  uint16_t paid;
  uint8_t count, cap;               /* countFood, maxCount (upgrades raise it) */
  uint8_t build, tick;              /* zavodBuild (half seconds), zavodTick */
  uint8_t stamina, stamina_max;
  uint16_t busy;                    /* buyers' spots taken */
} mm_box;

typedef struct {
  uint8_t sleeping, eat;            /* eat: ticks of the eating animation left */
} mm_zstate;

typedef struct {
  int32_t x, y, lastx, lasty;       /* pos (the game floors it every frame), posLast */
  float vx, vy;                     /* velocity: a direction */
  int16_t speed, old_speed;         /* units a second */
  uint8_t role, alive, ready_buy, input_allowed, moving_on, follow_path;
  uint8_t sklad_place, exit_place, collecting, allow_collect, allow_drop, sleeping;
  uint8_t in_queue, need_basket, has_basket, wait_food, alert_sent, boost;
  uint8_t stage, brain;
  int8_t tick_wait;
  uint8_t num_place, num_offset;    /* numPlaceBox, numPlaceOffset (1-based, 0: none) */
  uint8_t stamina, stamina_max, max_food;
  uint8_t food[MM_CARRY], nfood;    /* listCollectedFood */
  uint8_t path[MM_PATH][2];
  uint8_t way, nway;                /* wayIndex (0-based), maxIndex - 1; nway 0 and way 0: straight to (fx, fy) */
  int32_t fx, fy;                   /* followingPoint */
  uint8_t cool;                     /* ticks before isCollecting goes false */
  /* a customer's list (wishFood) */
  uint8_t wish_type[6], wish_count[6], nwish, wtick, wtype, have, need;
  uint16_t cost;
  /* a stockman's round */
  uint8_t gswift, gidx, gtick;      /* swift (0-based), the goal, tick (1-based) */
  uint8_t skin, hat, flip, anim, cloud;
  uint8_t basket;                   /* a customer's carrying animation (suffixCollect): 2, or 3 with a basket */
  uint16_t anim_t;
  uint8_t crazy;                    /* the troublemaker's dance: 1 or 2 (crazy1, crazy2) */
} mm_actor;

typedef struct { int32_t x, y; uint8_t food, live; } mm_dropped;
typedef struct { uint32_t due; uint8_t kind, who; int16_t a; int32_t b, c; } mm_timer;
typedef struct { uint8_t t, n; int16_t x0, y0, x1, y1; uint16_t food; } mm_fly;   /* food 0: a note */

/* what the player keeps (the game's datamanager): coins, hats, the mart, each mart's places, cash box and upgrades */
typedef struct {
  uint32_t coins;
  uint32_t hats;                    /* have_hats: bit n */
  uint8_t hat, mart;                /* selectedHat, numMarket - 1 */
  uint8_t sales[MM_MARTS];          /* numSale, numSale2 */
  uint32_t cash[MM_MARTS];          /* amount_cashbox, amount_cashbox2 */
  uint8_t have[MM_MARTS][MM_PLACES];        /* have_places: bought */
  uint16_t paid[MM_MARTS][MM_PLACES];       /* have_places: paid so far */
  uint8_t level[MM_MARTS][12][3];           /* upgrades_level: 1 .. (row 0 of mart 0 is the player's, in both) */
  uint8_t spare[2];                         /* (0: no padding in the file) */
} mm_progress;

typedef struct {
  mm_progress p;
  const mm_mart *m;
  uint8_t mi;
  uint32_t tick, rng;
  mm_box box[MM_PLACES];
  mm_zstate zs[8];
  mm_actor a[MM_ACTORS];
  uint8_t grid[90];                 /* walkable cells (this mart's copy, closed as places are bought) */
  mm_dropped dropped[MM_DROPPED];
  uint8_t ndropped;
  mm_timer timers[MM_TIMERS];
  uint8_t ntimers;
  uint32_t timer_seq;
  uint8_t queue[MM_BUYERS], nqueue; /* queueCashbox */
  uint8_t alerts[MM_ALERTS], nalerts;     /* listAlert: 1..8 zavod object + 1, 16+k worker k, 64+shelf a troublemaker */
  int32_t cash;                     /* amountCashBox */
  uint8_t sale_next;                /* placeNumForSale */
  uint8_t tutorial, step;           /* isTutorial, stepTutorial */
  uint8_t have_cashier, near_cash, can_check_queue, allow_rowdy, allow_next, allow_show_mart;
  uint16_t tick_crazy;
  int32_t truck_x, truck_y;         /* posNextMarket */
  uint8_t want_marts;               /* walking to the truck opened the marts window */
  uint8_t force_marts;              /* the first time (isHintTime): the marts window stays until you go */
  uint8_t badge;                    /* an upgrade is affordable (checkAvailableUpgrade) */
  /* the camera: follows the monkey (lerp 0.25 a frame), or shows a pad for a while */
  float camx, camy;
  int32_t pan_x, pan_y;
  uint16_t pan_t;                   /* ticks of the pan left (the monkey waits) */
  uint8_t slow;                     /* ticks of the slower follow after it */
  uint8_t control;                  /* the player moves the monkey (controlId) */
  mm_fly fly[MM_FLY];
  uint8_t fly_next;
  uint8_t hint;                     /* a sleeping animal, machine or worker to point at (+1), or 0 */
  int32_t hint_x, hint_y;
  uint8_t fx_kind[8];               /* small effects (poof, alarm, repair, broom): kind + 1, frame tick, place */
  uint16_t fx_t[8];
  int32_t fx_x[8], fx_y[8];
  uint32_t coins_display;           /* the money counter's lerp (x 256) */
  char notice[40];                  /* a short message (popup_message) */
  uint16_t notice_t;
} mm_game;

/* effects */
enum { FXK_EXIT = 1, FXK_ALARM, FXK_REPAIR, FXK_BROOM };

void mm_new_progress(mm_progress *p);
void mm_start(mm_game *g, uint32_t seed);              /* the mart in g->p.mart, from the progress */
void mm_step(mm_game *g, int dx, int dy);              /* dx, dy: the arrows (-1, 0, 1; y up) */
bool mm_upgrade(mm_game *g, unsigned show, unsigned k); /* the upgrades window: row `show`, column k (0..2) */
int mm_upgrade_price(const mm_game *g, unsigned row, unsigned k);   /* -1: none or at its most; 0: free */
unsigned mm_upgrade_level(const mm_game *g, unsigned row, unsigned k);
bool mm_upshow_open(const mm_game *g, unsigned show);  /* its place is bought (or it's always there) */
bool mm_buy_hat(mm_game *g, unsigned h);                   /* buy or put on hat h */
bool mm_mart_open(const mm_game *g, unsigned mart);    /* the marts window */
void mm_travel(mm_game *g, unsigned mart);
void mm_window(mm_game *g, int which, bool open);      /* the tutorial follows the windows: 0 upgrades, 1 hats */
uint32_t mm_rand(mm_game *g);
bool mm_progress_ok(const mm_progress *p);
#ifdef HOST
extern uint32_t mm_made[MM_FOODS + 1];                 /* foods made, sold (for the tests) */
extern uint32_t mm_sold[MM_FOODS + 1];
#endif
#endif
