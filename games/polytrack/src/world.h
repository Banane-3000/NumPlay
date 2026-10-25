/* The track as the physics and the picture see it (world.c): parts placed on the 5 m grid, their triangles near a
 * point, the start, the checkpoints and the finish, the ring of mountains. */
#ifndef PT_WORLD_H
#define PT_WORLD_H
#include "data.h"
#include "pt.h"

#define PT_PART_SIZE 5.0f
#define PT_CACHE 256            /* physics triangles kept around the car */

enum { ENV_SUMMER, ENV_WINTER, ENV_DESERT };

/* a triangle of the world, with its plane */
typedef struct {
  v3 a, b, c;
  v3 n;                         /* unit normal (front face: a, b, c counter-clockwise) */
  float d;                      /* n . a */
} wtri;

typedef struct {
  const pt_track *t;
  int id;
  /* the mountains: a ring round the track (main.bundle.js, createMountainVertices) */
  float mx, mz, mr;             /* centre and inner radius */
  int mseg;                     /* segments round */
  uint8_t mh[160][8];           /* heights, x 100/255 m */
  int nmh;
} world_t;

extern world_t W;

void world_load(int track);
const pt_inst *world_inst(int i);                  /* i < W.t->ninst */
/* an instance's orientation (3x3, whole numbers) and its position (metres) */
const int8_t *world_orient(const pt_inst *in);
v3 world_origin(const pt_inst *in);
v3 world_apply(const pt_inst *in, v3 local);       /* part space -> world */
void world_box(const pt_inst *in, v3 *lo, v3 *hi); /* the instance's world box */
/* calls f for every instance whose box meets [lo, hi] */
void world_query(v3 lo, v3 hi, void (*f)(int i, void *ctx), void *ctx);
/* physics triangles meeting the box (and the mountains near it); returns how many (at most max) */
int world_gather(v3 lo, v3 hi, wtri *out, int max);

/* the start: where the car is put and how it points */
void world_start(v3 *pos, qt *rot);
/* checkpoint detectors: n = pt_track.ncp, then nfin finishes */
typedef struct {
  v3 lo, hi;                    /* world box */
  uint16_t order;               /* 0xFFFF: a finish */
  uint8_t rot, axis, wide;
  v3 part;                      /* the part's position (metres) */
} detector_t;
void world_detector(int k, detector_t *d);
qt world_quat(int rot, int axis);
float mountain_height(float x, float z);           /* (for the picture) */
#endif
