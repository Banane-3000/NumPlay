/* Hill Climb Racing: the stages, their ground and what lies along it.
 *
 * A stage's ground is the same on every run: a sum of smooth noises (each a Catmull-Rom curve through hashed
 * heights) whose size grows with the distance, so the hills get steeper and bumpier the further you go. Caves
 * have a ceiling; the Desert has rope bridges over canyons, the Forest trees to break through. Coins come every
 * 30 m, fuel cans at the stage's own spacing, and each stretch of the road is a level with its reward. */
#ifndef HC_STAGE_H
#define HC_STAGE_H
#include <stdbool.h>
#include <stdint.h>

enum { ST_COUNTRYSIDE, ST_DESERT, ST_ARCTIC, ST_HIGHWAY, ST_CAVE, ST_MOON, ST_MARS, ST_ALIEN, ST_ARCTIC_CAVE,
       ST_FOREST, NSTAGES };
enum { FEAT_NONE, FEAT_BRIDGES, FEAT_TREES, FEAT_ICICLES };

typedef struct {
  const char *name;
  uint32_t price;
  float gravity, friction;
  uint8_t sky, surf, ground, csurf, icon, particle;
  uint8_t ceiling;              /* 0 none, 1 all along (a cave), 2 tunnels now and then */
  uint8_t feature, clouds, stars;
  uint32_t seed;
  float len[4], amp0[4], grow[4], amax[4];   /* the noises: wavelength, size at the start, + per km, at most */
  float clear, clear_var;       /* a cave's height over its floor */
  float fuel_first, fuel_gap, fuel_grow, fuel_max;   /* the first can, the gap, its growth per can, at most */
  float coin25, coin100, coin500, all500;            /* where coins of each value start */
  float lvl_len, lvl_inc;       /* a level's length and how much longer each next one is */
  uint32_t lvl_reward, lvl_reward_inc;
  uint32_t surf_color;          /* the surface's colour (particles, the map) */
} Stage;

extern const Stage stages[NSTAGES];
extern const Stage *stage;      /* the stage being driven */
void stage_set(int s);

/* the ground under x (meters), what wheels drive on: bridges count */
float stage_floor(float x);
/* the ground drawn under x: below a bridge, the canyon's floor */
float stage_ground(float x);
/* the ceiling over x, if there is one there */
bool stage_ceiling(float x, float *y);
/* the start: a wall behind it */
#define STAGE_WALL -6.0f

/* bridges: the one over x (its ends), or false */
bool stage_bridge(float x, float *x0, float *x1);
/* how the bridge under the vehicle gives way: its weight at x (set each frame) */
void stage_bridge_load(float x, float depth);

/* trees: the k-th tree's place (false past the stage's end); broken ones are kept in a bit set */
bool stage_tree(int k, float *x);
int stage_tree_first(float x);   /* the first tree at or after x */
#define TREES_MAX 4800
void stage_trees_reset(void);    /* all standing again (a new run) */
bool stage_tree_standing(int k);
void stage_tree_fall(int k);
#define TREE_H 2.4f
#define TREE_R 0.16f

/* coins: group k's coins (at most 14); value of a coin; positions */
#define COIN_GROUPS 2048
#define COIN_SPACING 30.0f
int stage_coin_group(int k, float *x, float *y, int *value);   /* how many, their places and values */
/* fuel cans: the k-th one's x, k from 0; false past the last */
bool stage_fuel(int k, float *x);
/* levels: where level n (1, 2...) ends and its reward */
float stage_level_end(int n);
uint32_t stage_level_reward(int n);
#endif
