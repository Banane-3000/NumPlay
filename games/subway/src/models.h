/* Subway Surfers: what tools/apk.py makes from the game's package (data.c): skinned models and their animation
 * clips, the baked faces of trains, barriers and the city, pictures for the screens. */
#ifndef SS_MODELS_H
#define SS_MODELS_H
#include <stdint.h>
#include "r3d.h"

/* a model's corner where the skeleton is at rest: position (mm), normal (x127), the bones it follows (b1 with
 * weight w/256), its place in its part's picture (1/4 texels) */
typedef struct {
  int16_t x, y, z;
  int8_t nx, ny, nz;
  uint8_t w;
  uint16_t u, v;
  uint8_t b0, b1;
} MV;

/* triangles that share a picture (tex, or -1) and at most 255 corners */
typedef struct {
  uint16_t v0, nv, t0, nt;
  int16_t tex;
} MPart;

/* a skinned model: its corners follow skin bones; each skin bone is a bone of the rig, and ibind takes a corner
 * from where it is at rest into that bone's own space (metres) */
typedef struct {
  const MPart *parts;
  const MV *v;
  const Tri *t;                /* corners counted from the part's first */
  const uint16_t *pal;         /* the colours of the triangles that have no picture (RGB565) */
  const float (*ibind)[12];
  const uint8_t *bone;
  uint8_t nparts, nbones;
} Model;

/* an animation clip of a rig: each frame, every bone's turn from its parent (a quaternion x127); its place from
 * its parent (mm): t for the bones that stay put, mt every frame for the moving ones (move lists them) */
typedef struct {
  uint8_t nbones, nframes, loop, nmove, fps;
  const int8_t *parent;
  const int8_t (*q)[4];
  const int16_t (*t)[3];
  const uint8_t *move;
  const int16_t (*mt)[3];
} Clip;

/* a baked face's corner: where it is (cm) and its place in the model's texture (1/4 texels) */
typedef struct {
  int16_t x, y, z;
  uint16_t u, v;
} BCorner;
/* a baked face: a convex polygon of n corners (clockwise seen from the front) from corner first, and its picture's
 * average colour when it has few holes (0: none; far away it is drawn plain) */
typedef struct {
  uint16_t first;
  uint8_t n;
  uint16_t avg;
} BFace;

/* a model baked into faces, the most worth seeing first; box: its extent (cm) x0 y0 z0 x1 y1 z1 */
typedef struct {
  const BFace *f;
  const BCorner *c;
  uint16_t n;
  uint8_t tex, pad;
  int16_t box[6];
} BModel;

/* a picture for the screens: rows of runs of (count, alpha 0..7 << 5 | colour) */
typedef struct {
  uint8_t w, h;
  const uint16_t *pal;
  const uint16_t *rows;        /* where each row's runs start (h + 1) */
  const uint8_t *runs;
} Image;
#endif
