/* Turbo Rush's rules: the circuits (track.c) and the race (race.c), in whole numbers only, so that a race comes out
 * the same on the calculator and on every computer.
 *
 * Distances are in 1/256 metre and speeds in 1/256 metre a second (FX); the race moves on in steps of 20 ms. */
#ifndef TR_GAME_H
#define TR_GAME_H
#include <stdbool.h>
#include <stdint.h>

#define FX 256
#define TR_TRACKS 6
#define TR_CARS 8
#define TR_LAPS 3
#define TR_SEG 5                    /* metres of road per segment */
#define TR_MAXSEG 600               /* segments in a lap, at most */
#define TR_ROAD (7 * FX)            /* half the road's width */
#define TR_LANE (14 * FX / 3)       /* the three lanes: -TR_LANE, 0 and TR_LANE */
#define TR_OUT (20 * FX)            /* how far from the road a car can go */
#define TR_CAR_LEN (21 * FX / 5)    /* cars touch nearer than this, front to back (4.2 m)... */
#define TR_CAR_WID (23 * FX / 10)   /* ...and side by side (2.3 m) */
#define TR_TICK 20                  /* ms per step */
#define TR_HZ 50

enum { MODE_GP, MODE_RACE, MODE_FREE };
enum { LEVEL_EASY, LEVEL_NORMAL, LEVEL_EXPERT };
enum { MOOD_DAY, MOOD_SUNSET, MOOD_NIGHT };
enum { PH_GRID, PH_RACE, PH_DONE };                     /* PH_DONE: you're over the line or out of the race */
enum { END_NONE, END_FINISH, END_WRECK, END_TIME };
enum { IN_GAS = 1, IN_BRAKE = 2, IN_LEFT = 4, IN_RIGHT = 8, IN_NITRO = 16 };
enum { UP_ENGINE, UP_TIRES, UP_NITRO, UP_ARMOR };
enum { SC_PALM, SC_PINE, SC_ROCK, SC_TOWER, SC_SIGN, SC_CACTUS, SC_VILLA, SC_LAMP, SC_KINDS };
enum { SKY_COAST, SKY_ALPS, SKY_DESERT, SKY_CITY };
enum { TH_COAST, TH_VALLEY, TH_DESERT, TH_HARBOR, TH_ALPINE, TH_METRO };

/* ------------------------------------------------------------------ circuits (track.c) */
typedef struct {
  uint16_t n;                       /* segments in a lap */
  int32_t length;                   /* n * TR_SEG * FX */
  uint8_t id, sky, theme;
  int8_t curve[TR_MAXSEG];          /* the bend: centimetres the road turns per segment, per segment (+ right) */
  int16_t y[TR_MAXSEG + 1];         /* height at the start of each segment, in centimetres */
  uint8_t limit[TR_MAXSEG];         /* the speed (m/s) a car with plain tyres can carry here, braking included */
  uint16_t heading[TR_MAXSEG + 1];  /* which way the road points, in 1/65536 of a turn */
} tr_track;

typedef struct {
  int8_t kind;                      /* SC_..., or -1: nothing */
  bool flip;
  int32_t x;                        /* across the road (FX) */
  int32_t half;                     /* half its width at the ground, for crashes (FX) */
  int32_t height;                   /* (FX) */
} tr_thing;

extern const char *const tr_track_name[TR_TRACKS];
extern const char *const tr_track_note[TR_TRACKS];
void tr_build(tr_track *t, int id);
int tr_seg_of(const tr_track *t, int32_t z);                      /* the segment under z (any lap, negative too) */
int tr_curve_at(const tr_track *t, int32_t z);
int32_t tr_height_at(const tr_track *t, int32_t z);              /* (FX) */
tr_thing tr_scenery(const tr_track *t, int seg, int side);       /* what stands at a segment, side -1 or 1 */
int tr_map(int id, int n, int16_t *xs, int16_t *ys, int w, int h);  /* the circuit's outline, fit in w x h */
int32_t tr_wrap(const tr_track *t, int32_t dz);                  /* a gap brought into half a lap either way */

/* ------------------------------------------------------------------ the race (race.c) */
typedef struct {
  int32_t z;                        /* distance from the start line (negative on the grid) */
  int32_t x;                        /* across the road, 0 the middle, + right */
  int32_t v;                        /* speed */
  int32_t vx;                       /* sideways speed */
  int32_t lane;                     /* a rival: where it wants to be */
  int32_t cruise;                   /* Free Ride traffic: the speed it keeps */
  uint32_t finish_ms;               /* race time at the finish line (0: not yet) */
  uint16_t think;                   /* a rival: steps to its next look round */
  uint16_t bump;                    /* steps before touching you hurts again */
  uint8_t paint, style, skill, place;   /* place: finishing place, 0 before the line */
} tr_car;

typedef struct {
  uint8_t track, mode, level, mood, rain, manual, laps, phase;
  uint8_t end, gear, lap, boost;    /* lap: yours, 1..laps; boost: nitro burning */
  uint8_t up[4], finished, offroad, banner, banner_arg;
  uint16_t countdown, nitro, damage, nitro_lock, impact, done, banner_t, spare;
  int16_t steer, cam_x;
  uint32_t ticks, time_ms, lap_ms, best_lap_ms, last_lap_ms, seed, odometer, passes;
  tr_car car[TR_CARS];              /* 0 is you */
  uint8_t ahead[TR_CARS];           /* Free Ride: that car is in front of you */
} tr_race;

enum { BANNER_NONE, BANNER_LAP, BANNER_FINAL, BANNER_FINISH, BANNER_WRECK, BANNER_TIME };

typedef struct {
  uint8_t track, mode, level, mood, rain, manual, paint, style;
  uint8_t up[4];
  const uint8_t *skill;             /* the rivals' skills (a Grand Prix keeps them), or NULL: shuffled */
} tr_setup;
void tr_new(tr_race *r, tr_track *t, const tr_setup *s, uint32_t seed);
void tr_step(tr_race *r, const tr_track *t, unsigned input, int shift);
bool tr_valid(const tr_race *r);
int tr_top_speed(const uint8_t up[4]);                     /* m/s */
int tr_kmh(int32_t v);
int tr_rank(const tr_race *r, int car);                     /* 1..8: by finish, then by distance */
void tr_order(const tr_race *r, uint8_t order[TR_CARS]);    /* the cars, first to last */
int tr_traffic(const tr_race *r);                           /* Free Ride: cars on the road with you */
uint32_t tr_rng(uint32_t *s);
#endif
