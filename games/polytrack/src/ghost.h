/* The ghost of your best run (ghost.c): where the car was every 100 ms, about six bytes a sample (positions in 1/16 m
 * as a change from a straight-on guess, the turn as its three smallest parts), played back smoothly. */
#ifndef PT_GHOST_H
#define PT_GHOST_H
#include "pt.h"
#include "save.h"

#define GHOST_STEP 100              /* frames (ms) between samples */

typedef struct {
  uint8_t buf[PT_GHOST_MAX];
  uint16_t len;
  uint8_t full;                     /* ran out of room: this run won't be kept */
  int32_t q[2][3];                  /* the last two positions, in 1/16 m */
  uint32_t n;
} ghost_rec;

typedef struct {
  const uint8_t *data;
  uint16_t len, at;
  int32_t q[2][3];
  uint32_t n;                       /* samples decoded */
  v3 pos[4];                        /* samples first-1 .. first+2 */
  qt rot[4];
  int32_t first;
  bool on, ended;
  uint32_t ms;
} ghost_play;

void ghost_rec_start(ghost_rec *r);
void ghost_rec_add(ghost_rec *r, v3 pos, qt rot);
void ghost_play_start(ghost_play *g, const uint8_t *data, uint16_t len, uint32_t ms);
/* the ghost at frame f (1 ms frames from the start); false when it has none there */
bool ghost_at(ghost_play *g, uint32_t f, v3 *pos, qt *rot);
#endif
