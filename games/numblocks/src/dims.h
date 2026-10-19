/* NumBlocks.nwa only (NB_DIMS): what the generators of the Nether and the End share with the game. */
#ifndef NB_DIMS_H
#define NB_DIMS_H
#include <stdbool.h>
#include <stdint.h>
#include "blocks.h"

enum { DIM_OVERWORLD = 0, DIM_NETHER = 1, DIM_END = 2 };

/* The generators write a byte a block: codes 67..82 (wool, which no generator makes) stand for the
 * blocks past 255 (world.c turns them back) */
#define GEN_CODE(k) (B_WOOL_WHITE + (k))
enum { GC_NETHERRACK, GC_SOUL_SAND, GC_QUARTZ_ORE, GC_NETHER_BRICK, GC_FENCE, GC_STAIRS_E, GC_STAIRS_W, GC_STAIRS_S,
       GC_STAIRS_N, GC_WART, GC_END_STONE, GC_FRAME, GC_FRAME_EYE, GC_N };
extern const uint16_t gen_code_block[GC_N];

/* gen.c, past the Overworld (gen_dims.h): the generator of each dimension */
void gen_dim(int d);   /* which one gen_slab, gen_biome and gen_top run (after gen_init) */

/* dims.c: what is kept of the dimensions with the world (save.c) */
typedef struct {
  float ow_x, ow_y, ow_z;   /* where the player was last in the Overworld */
} DimsState;
extern DimsState dst;
extern float portal_time;   /* 0..1: the portal's swirl over the picture */
bool dims_portal_light(int x, int y, int z);   /* fire there lights a portal? (then it is made) */
int dims_can_stay(int b, int x, int y, int z);   /* -1: not one of the dimensions' blocks */
void dims_player_tick(void);                   /* portals */
void dims_respawn(void);                       /* (before the player respawns) */
void player_to_spawn(void);
void dims_go(int to);   /* (tests) there, as through a portal */
void dragon_enter(void);
void dims_credits(void);
#endif
