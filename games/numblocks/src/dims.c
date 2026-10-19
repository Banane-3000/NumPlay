/* NumBlocks.nwa only: the Nether and the End, as Minecraft 1.8.8 has them, next to the Overworld.
 *
 * The player is in one dimension at a time (dim); the block cache, the generator and the edit log
 * follow it. Each dimension's changes have their own records (edits.c). */
#include "nb.h"
#if NB_DIMS
#include "gen.h"

int dim;

/* what the generators' codes stand for (nb.h) */
const uint16_t gen_code_block[GC_N] = {
    B_NETHERRACK, B_SOUL_SAND, B_QUARTZ_ORE, B_NETHER_BRICK, B_NETHER_BRICK_FENCE, B_NETHER_BRICK_STAIRS_E,
    B_NETHER_BRICK_STAIRS_W, B_NETHER_BRICK_STAIRS_S, B_NETHER_BRICK_STAIRS_N, B_NETHER_WART_0, B_END_STONE,
    B_END_PORTAL_FRAME, B_END_PORTAL_FRAME_EYE};

void dims_slab(int cx, int cz, int y0, int h, uint8_t *out) { gen_slab(cx, cz, y0, h, out); }
int dims_biome(int x, int z) { return gen_biome(x, z); }
int dims_top(int x, int z) { return gen_top(x, z); }
#endif
