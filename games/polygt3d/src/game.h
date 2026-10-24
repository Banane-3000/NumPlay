/* PolyGT 3D's rules: the roads (track.c) and the race (race.c), in whole numbers only, so that a race comes out the
 * same on the calculator and on every computer. The picture (render.c) is drawn from them with floats.
 *
 * Distances are in 1/256 metre (FX), speeds in 1/256 metre a second, angles in 1/65536 of a turn; the race moves on
 * in steps of 20 ms. A car is placed along the road (s, from the start line) and across it (x, + to the right), and
 * points at `ang` from the road's direction; the road turns right where its heading grows. */
#ifndef PG_GAME_H
#define PG_GAME_H
#include <stdbool.h>
#include <stdint.h>

#define FX 256
#define PG_TICK 20                  /* ms per step */
#define PG_HZ 50
#define PG_CARS 8                   /* you and up to seven rivals */
#define PG_CIRCUITS 3
#define PG_VEHICLES 5
#define PG_MAXN 288                 /* segments in a circuit, at most */
#define PG_RING 128                 /* the endless road: the segments kept around you (a power of two) */
#define PG_SEG (6 * FX)             /* the endless road's segments; circuits come close to it */
#define PG_ROAD (8 * FX)            /* half the road */
#define PG_KERB (6 * FX / 5)
#define PG_WALL (25 * FX / 2)       /* the barriers, either side */
#define PG_HL (22 * FX / 10)        /* half a car's length... */
#define PG_HW (115 * FX / 100)      /* ...and half its width */
#define PG_ZONE 70                  /* the endless road changes scenery every 70 segments (420 m) */
#define PG_ORBS 24                  /* nitro orbs on a circuit, at most */
#define PG_MAX_LAPS 9
#define PG_GHOST 480                /* samples of a lap, every 200 ms: 96 seconds */
#define ANG_TURN 65536

enum { MODE_RACE, MODE_ENDLESS };
enum { LEVEL_EASY, LEVEL_NORMAL, LEVEL_HARD };
enum { VIEW_CHASE, VIEW_FAR, VIEW_BUMPER, VIEWS };
enum { PH_GRID, PH_RACE, PH_DONE };
enum { END_NONE, END_FINISH, END_TIME };
enum { IN_GAS = 1, IN_BRAKE = 2, IN_LEFT = 4, IN_RIGHT = 8, IN_NITRO = 16 };
enum { BANNER_NONE, BANNER_LAP, BANNER_FINAL, BANNER_ZONE, BANNER_WRONG };
/* what stands at a segment */
enum { PROP_NONE, PROP_TREE, PROP_ARCH, PROP_GANTRY, PROP_CHEVRON };

/* ------------------------------------------------------------------ fixed-point helpers (track.c) */
int32_t pg_sin(int32_t a);          /* 32767 = 1 */
int32_t pg_cos(int32_t a);
int32_t pg_atan2(int32_t y, int32_t x);   /* the angle of (x, y), x along 0 */
uint32_t pg_isqrt(uint64_t v);
uint32_t pg_rng(uint32_t *s);       /* xorshift32; s must not be 0 */

/* ------------------------------------------------------------------ the roads (track.c) */
typedef struct {
  uint32_t rng;
  int32_t head;                     /* heading of the segment being laid */
  int32_t turn, turn_to;            /* its bend, and the bend it eases into (1/16 angle unit a segment) */
  int32_t slope, slope_to;          /* climb a segment (FX) */
  int16_t turn_hold, slope_hold;
  uint8_t zone, spare;
  int32_t count;                    /* segments laid */
  int32_t x, y, z;                  /* the end of the road so far */
  int32_t prev_head;                /* the heading of the segment before */
} pg_gen;

typedef struct {
  uint8_t endless, id;              /* id: the circuit, or the scenery the endless road starts in */
  uint16_t n;                       /* circuit: segments in a lap */
  int32_t seg;                      /* a segment's length */
  int32_t length;                   /* circuit: a lap, n * seg */
  int32_t px[PG_MAXN], py[PG_MAXN], pz[PG_MAXN];   /* where each segment starts (its node) */
  uint16_t nh[PG_MAXN];             /* the road's heading at the node */
  int16_t cv[PG_MAXN];              /* how much it turns over the segment */
  uint8_t limit[PG_MAXN];           /* the fastest a car with grip 1 takes the segment's bend (m/s) */
  uint8_t prop[PG_MAXN];            /* PROP_..., + 8 on the left */
  uint8_t zone[PG_MAXN];            /* the scenery (endless: changes along the road) */
  int8_t orb[PG_MAXN];              /* a nitro orb at the segment's middle: its x in 1/8 metre, or -128 */
  int8_t orb_id[PG_MAXN];           /* circuit: the orb's number, for its cooldown */
  uint8_t orbs;                     /* circuit: orbs on a lap */
  int32_t first, last;              /* endless: the segments kept, by number from the start */
  pg_gen gen;
} pg_track;

extern const char *const pg_circuit_name[PG_CIRCUITS];
extern const char *const pg_circuit_note[PG_CIRCUITS];
extern const char *const pg_zone_name[5];
void pg_build(pg_track *t, int id);                 /* a circuit */
void pg_endless(pg_track *t, uint32_t seed);        /* the endless road, from its start */
void pg_extend(pg_track *t, int32_t upto);          /* the endless road laid on to segment `upto` (and kept from
                                                       there back), or nothing for a circuit */
int pg_slot(const pg_track *t, int32_t k);          /* the arrays' index of segment k (any lap) */
int32_t pg_seg_of(const pg_track *t, int32_t s);    /* the segment number under s (negative too) */
int32_t pg_heading_at(const pg_track *t, int32_t s);
int32_t pg_height_at(const pg_track *t, int32_t s);
void pg_world(const pg_track *t, int32_t s, int32_t x, int32_t *wx, int32_t *wy, int32_t *wz);
int32_t pg_wrap(const pg_track *t, int32_t ds);     /* a circuit gap brought within half a lap either way */
int pg_zone_at(const pg_track *t, int32_t s);
int pg_map(const pg_track *t, int n, int16_t *xs, int16_t *ys, int w, int h);  /* the outline, fit in w x h */

/* ------------------------------------------------------------------ cars (race.c) */
typedef struct {
  const char *name, *kind;
  uint16_t top;                     /* top speed, 1/8 m/s */
  uint16_t accel;                   /* 1/8 m/s^2 */
  uint8_t grip, drift;              /* 1/128 */
  uint8_t offroad;                  /* grass drag, 1/128 */
  uint8_t horse;
  uint16_t redline;
  uint8_t body[3], accent[3];
} pg_vehicle;
extern const pg_vehicle pg_vehicles[PG_VEHICLES];

typedef struct {
  int32_t s, x, v;                  /* along (unwound: laps add up), across, speed */
  int32_t lane;                     /* a rival: the x it heads for */
  int16_t ang;                      /* where it heads, from the road's direction */
  int16_t slip;                     /* the body turned from where it heads (a slide) */
  int16_t steer;                    /* -1024..1024 */
  uint16_t nitro;                   /* 0..1000 */
  uint32_t finish_ms;               /* at the line (0: not yet) */
  uint16_t think, aim;              /* a rival: steps to its next look ahead, the speed it aims for (FX / 8) */
  uint8_t gear, boost, drift, model, paint, place, bump;
  uint8_t driven;                   /* by the game: the rivals, and you after the line */
} pg_car;

typedef struct {
  uint8_t mode, track, vehicle, laps, rivals, level, manual, phase;
  uint8_t end, lap, finished, banner, banner_arg, zone, wrong, flash;   /* flash: an orb just taken */
  uint16_t countdown, done, banner_t, slipstream;
  uint32_t ticks, time_ms, lap_at, best_lap_ms, last_lap_ms, seed;
  int32_t time_left;                /* endless: ms left */
  int32_t last_orb;                 /* endless: the last orb taken (its segment) */
  uint16_t orb_wait[PG_ORBS];       /* circuit: steps before an orb is back */
  pg_car car[PG_CARS];              /* 0 is you */
} pg_race;

typedef struct {
  uint8_t mode, track, vehicle, laps, rivals, level, manual;
} pg_setup;

void pg_new(pg_race *r, pg_track *t, const pg_setup *s, uint32_t seed);
void pg_step(pg_race *r, pg_track *t, unsigned input, int shift);
bool pg_valid(const pg_race *r, const pg_track *t);
int pg_rank(const pg_race *r, int car);             /* 1.. : by finish, then by distance */
int pg_kmh(int32_t v);
int pg_rpm(const pg_race *r);
int32_t pg_lap_s(const pg_race *r, const pg_track *t);  /* where you are in the lap (or on the endless road) */
int32_t pg_zone_bonus(int zone);                    /* ms a zone of the endless road gives */
uint32_t pg_sum(const void *data, uint32_t n);
#endif
