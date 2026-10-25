/* What PolyTrack keeps (save.c): in polytrack.sav your best time on each track and its checkpoint times, the car's
 * colour, the view and whether you read the guide; a copy goes into a Python script, polytrack_saves.py (installing
 * apps from the NumWorks website keeps only scripts, so the save comes back from it). The ghosts of your best runs
 * go into polytrack_ghosts.sav, as many as fit in 12 KB, the last ones raced first.
 * Each file: a magic, a version, its size and a checksum; anything else is not read, and the game starts afresh. */
#ifndef PT_SAVE_H
#define PT_SAVE_H
#include <stdbool.h>
#include <stdint.h>
#include "data.h"

#define PT_SAVE_NAME "polytrack.sav"
#define PT_COPY_NAME "polytrack_saves.py"
#define PT_GHOST_NAME "polytrack_ghosts.sav"
#define PT_MAGIC 0x314B5450u        /* "PTK1" */
#define PT_GMAGIC 0x31475450u       /* "PTG1" */
#define PT_VERSION 1
#define PT_SPLITS 10                /* checkpoint orders on a track, at most */
#define PT_MAX_TIME 5999998u        /* (the game's own limit, 1 ms frames) */
#define PT_PAINTS 8
#define PT_GHOST_BYTES 12288        /* the ghosts' file */
#define PT_GHOST_MAX 8192           /* one ghost */

typedef struct {
  uint32_t best[PT_NTRACKS];        /* ms, 0: not finished yet */
  uint32_t split[PT_NTRACKS][PT_SPLITS];   /* the best run's checkpoint times */
  uint32_t finishes;                /* runs to the line */
  uint8_t track, paint, view, guide;
} pt_profile;

typedef struct {
  uint32_t magic;
  uint16_t version, size;
  pt_profile p;
  uint32_t sum;                     /* FNV-1a of everything before it */
} pt_file;
_Static_assert(sizeof(pt_file) == 4 + 4 + PT_NTRACKS * 4 * (1 + PT_SPLITS) + 8 + 4, "polytrack.sav's size");

void pt_fresh(pt_profile *p);
bool pt_file_ok(const pt_file *f);
bool save_load(pt_file *f);         /* the save, its copy, or a fresh start (false) */
bool save_write(const pt_file *f);  /* with the magic, size and sum */

/* ghosts: a track's samples, encoded (ghost.c) */
typedef struct {
  uint8_t track;
  uint16_t len;                     /* bytes */
  uint32_t ms;                      /* the run's time */
  const uint8_t *data;
} ghost_entry;
/* the ghost kept for a track (data points into a buffer of save.c), or false */
bool ghosts_find(int track, ghost_entry *g);
/* keeps this one first, then the others while they fit */
bool ghosts_put(int track, uint32_t ms, const uint8_t *data, uint16_t len);
void ghosts_load(void);
uint32_t pt_sum(const void *data, uint32_t n);
#endif
