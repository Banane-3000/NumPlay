/* Subway Surfers: what tools/pack.py makes (data.c): models and pictures. */
#ifndef SS_MODELS_H
#define SS_MODELS_H
#include <stdint.h>
#include "r3d.h"

/* a model's corner, at rest: position (mm), normal (x127), the bones it follows (b1 with weight w/256),
 * its place in its part's picture (1/4 texels) */
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

typedef struct {
  const MPart *parts;
  const MV *v;
  const Tri *t;               /* corners counted from the part's first */
  const uint16_t *pal;        /* the colours of the triangles that have no picture (RGB565) */
  const int16_t (*joints)[3]; /* each bone's joint at rest (mm) */
  const int8_t *parent;       /* each bone's parent (-1: none) */
  uint8_t nparts, nbones;
} Model;

/* a picture for the screens: rows of runs of (count, alpha 0..7 << 5 | colour) */
typedef struct {
  uint8_t w, h;
  const uint16_t *pal;
  const uint16_t *rows;       /* where each row's runs start (h + 1) */
  const uint8_t *runs;
} Image;
#endif
