/* Hill Climb Racing: the stages, their ground and what lies along it, as the game makes them.
 *
 * A stage's ground is the game's own formula (its Stage*::getHeightAtX: sums of sines that grow with the distance),
 * sampled every 0.5 m like the game's terrain. Rope bridges cross canyons every 70 to 210 m on most stages (the road
 * resumes after one at the height it had before it); caves have a ceiling over the ground; the Forest has pine trees;
 * the Mudpool and the Volcano have pits of mud and lava (the road resumes after one as after a bridge).
 * Coins come in a chunk every 30 m, worth more the further you go, fuel cans every few chunks, all as the game's
 * GameContentLayer places them. Each stage has levels, kept from run to run, each with its bonus. */
#ifndef HC_STAGE_H
#define HC_STAGE_H
#include <stdbool.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

enum { ST_COUNTRYSIDE, ST_DESERT, ST_ARCTIC, ST_HIGHWAY, ST_CAVE, ST_MOON, ST_MARS, ST_ALIEN, ST_ARCTIC_CAVE,
       ST_FOREST, ST_MOUNTAIN, ST_MUDPOOL, ST_VOLCANO, NSTAGES };
enum { FEAT_NONE, FEAT_BRIDGES, FEAT_TREES, FEAT_ICICLES };
enum { PIT_NONE, PIT_MUD, PIT_LAVA };

typedef struct {
  const char *name;
  uint32_t price;
  float gravity, friction;      /* the game's: g (m/s2), the ground's friction */
  float coin_mult, coin_off;    /* a coin chunk at x is worth x * coin_mult; coins' rows from coin_off */
  uint8_t sky, surf, ground, csurf, icon, particle;
  uint8_t ceiling;              /* a cave: a ceiling all along */
  uint8_t feature, clouds, stars;
  uint8_t bridges;              /* rope bridges */
  float bridge_k, bridge_d;     /* the next bridge comes bridge_d (2 + sin(n / bridge_k)) m later */
  uint16_t lvl_base, lvl_step;  /* the game's levels (StageInfo): level n (from 0) ends at base + step n */
  uint16_t lvl_mult;            /* and pays mult x that + 2500 */
  uint32_t surf_color;          /* the surface's colour (particles, the map) */
  uint8_t pits;                 /* PIT_*: pits of mud or lava (then no bridges: they share their memory) */
} Stage;

extern const Stage stages[NSTAGES];
extern const Stage *stage;      /* the stage being driven */
extern int stage_id;
void stage_set(int s);          /* (also forgets what was worked out for the last one) */

/* the ground under x (the game's GameContentLayer::getTerrainHeightAt): under a bridge, the canyon */
float stage_floor(float x);
/* the ground as it is built: a point every 0.5 m (k: x = -40 + k / 2), lines between them */
float stage_vertex(int k);
float stage_ground(float x);
/* the ground, a bridge's deck or a pit's surface under x (getHeightWithBridgeOrPit) */
float stage_deck(float x);
/* a cave's ceiling over x (GameContentLayer::getCeilingHeightAt), and as it is built (points every 0.5 m) */
bool stage_ceiling(float x, float *y);
float stage_roof_vertex(int k);
bool stage_roof(float x, float *y);

/* bridges: the k-th one (from 0) starts at x0, is len meters long, at height y; false if none */
bool stage_bridge_k(int k, float *x0, float *len, float *y);
int stage_bridge_first(float x);   /* the first bridge that ends after x */

/* pits (Mudpool, Volcano): the k-th one (from 0) starts at x0, is len meters long, its surface at y; false if
 * none. Their depth (Pit::getDepth) */
bool stage_pit_k(int k, float *x0, float *len, float *y);
int stage_pit_first(float x);      /* the first pit that ends after x */
float stage_pit_depth(void);

/* trees (the Forest): the k-th one's foot; fallen ones are kept in a bit set */
bool stage_tree(int k, float *x, float *y);
int stage_tree_first(float x);     /* the first tree at or after x */
#define TREES_MAX 3200
void stage_trees_reset(void);      /* all standing again (a new run) */
bool stage_tree_standing(int k);
void stage_tree_fall(int k);
#define TREE_H 3.4f

/* coins: chunk k (from 1, at 30 k m): at most 14 coins, their places and values; how many */
#define COIN_GROUPS 2048
#define COIN_SPACING 30.0f
#define COIN_SIZE 1.5f             /* a coin's side (and a fuel can's): the car picks it up by touching it */
int stage_coin_group(int k, float *x, float *y, int *value);
/* fuel cans: the k-th one's place, k from 0; false past the last */
bool stage_fuel(int k, float *x, float *y);
int stage_fuel_first(float x);     /* the first can at or after x */
/* levels (StageInfo::getLevelGoal, getLevelBonus): where level n (0, 1...) ends, and its bonus */
uint32_t stage_level_goal(int n);
uint32_t stage_level_bonus(int n);

#ifdef __cplusplus
}
#endif
#endif
