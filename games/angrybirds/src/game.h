/* Angry Birds: a level as the game plays it (game.c): its objects on the physics (world.h), the slingshot, the
 * birds and their powers, damage and points, explosions, the Mighty Eagle, and when the level is won or lost.
 *
 * Time goes in steps of 33 ms (W_STEP), a physics step and the game's rules each: the original runs its physics in
 * 33 ms steps too, and its rules with the time each frame took. */
#ifndef AB_GAME_H
#define AB_GAME_H
#include <stdbool.h>
#include <stdint.h>
#include "res.h"

#define STEP_MS 33
#define OBJ_CAP (MAX_OBJECTS + 2 + 40)  /* the ground, the slingshot, the level's, and what joins it */
#define BIRD_CAP 32                      /* birds of all kinds at once (split blue birds, eggs, the eagle) */
#define BLAST_CAP 10
#define TIMER_CAP 48
#define TRAIL_CAP 96
#define PUFF_CAP 24
#define PI_F 3.14159265f

/* object flags */
enum {
  O_ALIVE = 1,
  O_SCENERY = 2,      /* static ground drawn on its own; its body is the level's merged rectangles */
  O_WAITING = 4,      /* a bird in the queue (the game's s) */
  O_ON_SLING = 8,     /* the bird on the slingshot (o) */
  O_DONE = 16,        /* to be removed (H) */
  O_USED = 32,        /* a bird's power was used (q) */
  O_HIT = 64,         /* the black bird's fuse is lit */
  O_MOVED = 128       /* it moved in the last step (the game's J and A differ) */
};

typedef struct {
  uint8_t type, cls;
  uint16_t flags;
  int32_t health, max_health;   /* E and F: whole points */
  float jx, jy, ja;             /* where it was after the last step (the game's A and z) */
} Obj;

typedef struct {
  int16_t obj;                  /* -1: free */
  int16_t rest;                 /* ms it has been still (p) */
  int16_t after_hit;            /* ms it stays steerable after its first damage (b, 50) */
  int16_t fuse, fuse_at;        /* the black bird: ms burnt, and when it goes off (i, j) */
  float steer_x, steer_y;       /* the green bird's turn: the speed it goes back to (S, T); 1e9: none */
  float home_x;                 /* a waiting bird stays here (G) */
  int16_t hop, spin;            /* a waiting bird's hop (n, k) */
  int16_t yell;                 /* (R) */
} Bird;

typedef struct { int8_t kind; int16_t wait; float x, y; } Blast;        /* explosions (mGb) */
typedef struct { int16_t trigger, target; int32_t at, ms; bool on; } Timer;   /* destroy B after A */
typedef struct { float x, y; uint8_t size; } TrailDot;
typedef struct { float x, y; int16_t t; int32_t points; uint8_t kind; } Puff;

enum { PUFF_SMOKE, PUFF_POINTS, PUFF_BOOM, PUFF_FEATHERS };
enum { ST_PLAY, ST_BONUS, ST_WON, ST_LOST };

typedef struct {
  int level;
  Obj obj[OBJ_CAP];
  int count;                    /* objects 0 .. count-1 */
  int first_block, first_bird, birds;
  Bird bird[BIRD_CAP];
  Blast blast[BLAST_CAP];
  Timer timer[TIMER_CAP];
  int timers;
  TrailDot trail[TRAIL_CAP];
  int trail_count, trail_last;
  float trail_x, trail_y;       /* the last dot of the flying bird */
  Puff puff[PUFF_CAP];
  uint32_t rng;
  /* the slingshot (gUb, mUb): the bird on it, the pocket's pull from the slingshot's top, the loading jump */
  float sling_x, sling_y;
  int loaded;                   /* the object on the slingshot, or -1 */
  int load_ms;                  /* the jump onto the slingshot: 1200 down to 0 */
  float load_x, load_y;         /* where it jumped from */
  bool ready;                   /* it sits in the pocket */
  int next_wait;                /* ms until the next bird jumps up (1000 after a launch) */
  bool out_of_birds;
  float angle, power;           /* the player's aim: radians up from level, 0..1 of the 5 m pull */
  float pull_x, pull_y;         /* the pocket, from the slingshot's top */
  int flying;                   /* the launched bird the player can still use, or -1 (the game's o) */
  int power_request;            /* 1: use the flying bird's power at the next step (I) */
  int follow;                   /* what the camera follows: a split blue bird, the egg... or -1 */
  /* the level's clocks (the game's p, f, t) */
  int32_t settle_ms;            /* after a launch: 30 s, kept to 5 s by damage; at 0 everything counts as still */
  int frames;                   /* steps since the start, up to 8 (no damage before) */
  /* the Mighty Eagle */
  bool eagle_armed;             /* the sardine is on the slingshot */
  bool eagle_used;              /* in this try */
  int eagle_ms;                 /* 2400: waiting for the sardine; 0.. after it lands (t) */
  bool eagle_out;               /* the eagle is flying (s) */
  bool eagle_turned, eagle_shook;
  int eagle;                    /* its object, or -1 */
  float sardine_spin;           /* (B) */
  /* points and the end */
  uint32_t score;
  int pigs, birds_left;
  int state;
  int end_ms;                   /* the 1 s checks; the waits before bonus and before failing */
  int check_ms;
  int bonus_ms;
  bool golden;                  /* a golden egg was broken in this try */
  int golden_level;             /* the Chrome Dimension level it opened, or -1 */
  int easter_special;           /* the hidden egg's special level, or -1 */
  float left, right;            /* past these an object is gone (the level's views, 5 m more) */
  float cam[8];
  uint32_t steps;
} Game;

extern Game g;

/* the level, its physics in the arena */
bool game_load(int level, void *arena, int arena_size);
void game_step(void);
/* the player */
void game_aim(float angle, float power);
bool game_can_launch(void);
bool game_launch(void);
bool game_power(void);
bool game_eagle_arm(void);      /* the next shot is the sardine */
bool game_in_flight(void);      /* a bird flies and its power can still be used */
int game_bird_queue(int *out, int max);   /* the waiting birds, next first */
float game_launch_speed(int obj);
/* the stars of a score in the level (0 when lost), the eagle's percent */
int game_stars(int level, uint32_t score);
int game_eagle_percent(int level, uint32_t score);
uint32_t game_rand(void);
void game_object_pos(int i, float *x, float *y, float *angle);
#endif
