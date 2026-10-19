/* Subway Surfers: the run, the world and what is kept between runs.
 *
 * Units are metres and seconds. x goes right across the three tracks (lanes 0, 1 and 2 at x = -LANE,
 * 0 and LANE), y up, z along the tracks, the way the runner goes. */
#ifndef SS_GAME_H
#define SS_GAME_H
#include <stdbool.h>
#include <stdint.h>
#include "r3d.h"

#define LANE 2.6f
#define LANE_X(l) (((l) - 1) * LANE)
#define TRAIN_W 1.18f      /* half its width */
#define TRAIN_H 3.5f       /* its roof */
#define CAR_LEN 11.0f
#define CAR_GAP 0.6f
#define RAMP_LEN 7.0f
#define DT (1.0f / 60)

/* ---------------------------------------------------------------- the world ahead */
enum {
  O_TRAIN,       /* standing on the tracks: a crash from the front, a stumble from the side, a roof to run on */
  O_MOVING,      /* a train coming the other way */
  O_RAMP,        /* up to a train's roof */
  O_HURDLE,      /* low: jump it */
  O_HIGHBAR,     /* a board up high: roll under it */
  O_BLOCK,       /* a big barrier: go round it, or over with Super Sneakers or the jetpack */
};
typedef struct {
  uint8_t type, lane, cars, look;
  float z0, z1;          /* from its near end to its far end */
  float v;               /* towards the runner (a moving train) */
} Obstacle;

/* coins, and the things to pick up */
enum { I_COIN, I_JETPACK, I_SNEAKERS, I_MAGNET, I_DOUBLE, I_MYSTERY, I_KEY };
typedef struct {
  float x, y, z;
  uint8_t kind, flying;  /* flying: drawn towards the runner by the magnet */
} Item;

#define MAXOBS 72
#define MAXITEMS 220
extern Obstacle obs[MAXOBS];
extern int nobs;
extern Item items[MAXITEMS];
extern int nitems;

void world_reset(uint32_t seed);
void world_title(void);         /* the title screen's trains */
void world_update(float pz, float speed);   /* makes the world ahead, forgets what is behind */
float world_ground(float x, float z, float y);   /* the height to stand on at (x, z), at most a little above y */
void world_clear(float z0, float z1);       /* no obstacles from z0 to z1 (a safe landing) */
void world_sky_coins(float z0, float z1);   /* coins high up, for the jetpack */
uint32_t world_hash(uint32_t a);
extern uint32_t world_seed;   /* the scenery of this run */
/* the scenery at z: a tunnel? */
bool world_tunnel(float z);

/* ---------------------------------------------------------------- the runner */
enum { ST_RUN, ST_AIR, ST_STUMBLE, ST_CRASH };
enum { PW_JETPACK, PW_SNEAKERS, PW_MAGNET, PW_DOUBLE, PW_COUNT };
typedef struct {
  float x, y, z, vy;
  int lane, from_lane;   /* where it is going, and where it came from (a stumble goes back there) */
  int state;
  float roll;            /* time left rolling */
  float speed, t;        /* forward speed; time running */
  float phase;           /* of the running legs */
  float stumble;         /* time left stumbling */
  float chase;           /* time left with the Inspector right behind */
  float ground;          /* the height under the runner */
  float power[PW_COUNT]; /* time left */
  float hover;           /* hoverboard time left */
  float safe;            /* after a broken board: nothing can hurt for this long */
  float fly;             /* the jetpack's or the headstart's height, 0 on foot */
  float headstart;       /* time left of a headstart */
  float crash_t;         /* since the crash */
  float lean;            /* sideways, while changing lanes */
  bool on_roof, dead;
} Runner;
extern Runner pl;

/* ---------------------------------------------------------------- the run */
typedef struct {
  uint32_t score, coins, keys;
  float dist, fscore;
  int multiplier;        /* the missions' multiplier (plus a score booster's 5) */
  int jumps, rolls, lanes, powerups, boards_used, stumbles, mystery, jetpacks, magnets;
  float roof;            /* metres run on top of trains */
  int saves;             /* times saved with keys this run */
  bool boosted;          /* a score booster used */
  float start_t;         /* the time since the run started (headstart and booster offers) */
  float toast_t;         /* a message on top: its time left */
  char toast[40];
} Run;
extern Run run;

/* the sparkle of a coin picked up */
typedef struct {
  float x, y, z, t;    /* t: its age, from 0 */
} Spark;
#define NSPARKS 12
extern Spark sparks[NSPARKS];

void run_start(void);
void run_step(uint32_t keys, uint32_t pressed);
bool run_over(void);           /* caught or crashed, and not saved */
void run_revive(void);         /* saved with keys */
int run_save_cost(void);       /* keys to be saved now */
void run_bank(void);           /* the coins and the progress go to the save (the run's end, or Home) */

/* ---------------------------------------------------------------- what is kept */
#define NCHARS 4
#define NBOARDS 3
#define NUPG 4
typedef struct {
  uint32_t coins, keys, best, runs;
  uint16_t boards, headstarts, boosters, mystery;   /* how many of each item */
  uint8_t upg[NUPG];             /* jetpack, sneakers, magnet, 2x: levels 0 to 5 */
  uint8_t chars, ownboards;      /* bits: unlocked */
  uint8_t character, board;
  uint8_t mset, mdone;           /* the mission set (the multiplier is 1 + mset) and its missions done */
  uint8_t help_seen, pad;
  uint32_t mbase[3];             /* the totals when each mission started */
  uint32_t total[16];            /* totals of everything the missions count */
} Progress;
extern Progress pg;

/* the missions' counts (Progress.total and the run's own) */
enum {
  CNT_COINS, CNT_JUMPS, CNT_ROLLS, CNT_POWERUPS, CNT_BOARDS, CNT_STUMBLES, CNT_JETPACKS, CNT_MAGNETS,
  CNT_MYSTERY, CNT_RUNS, CNT_DIST, CNT_COUNT
};
float power_time(int pw);      /* how long a power-up lasts, with its upgrades */
int upgrade_cost(int level);   /* to go up from this level, or 0 at the top */
extern const char *const char_names[NCHARS];
extern const uint32_t char_price[NCHARS];
extern const char *const board_names[NBOARDS];
extern const char *const board_power[NBOARDS];
extern const uint32_t board_price[NBOARDS];

/* missions */
typedef struct {
  uint8_t kind;          /* M_* */
  uint32_t target;
} Mission;
void mission_get(int set, int i, Mission *m);
void mission_text(const Mission *m, char *out, int n);
uint32_t mission_progress(int i);   /* how far along mission i of the current set is */
void missions_check(void);          /* marks the missions done, and moves to the next set */
int multiplier(void);
#endif
