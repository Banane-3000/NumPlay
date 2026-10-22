/* What's kept (coins, the best score, the level's points, stars, stages, masks, upgrades, shields, missions, the
 * chest), in tombofthemask.sav, and a copy of it in a Python script, tombofthemask_saves.py (installing apps from
 * the NumWorks website keeps only scripts, so the save comes back from it). */
#ifndef TM_SAVE_H
#define TM_SAVE_H
#include "game.h"

#define TM_SAVE_NAME "tombofthemask.sav"
#define TM_COPY_NAME "tombofthemask_saves.py"
#define TM_MAGIC 0x4D544F54u      /* "TOTM" */
#define TM_VERSION 1

typedef struct {
  uint32_t magic;
  uint16_t version, size;
  tm_progress p;
  uint32_t sum;                    /* FNV-1a of everything before it */
} tm_file;

uint32_t tm_sum(const void *data, uint32_t n);
bool tm_file_ok(const tm_file *f);
bool save_load(tm_progress *p);    /* the save, its copy, or a fresh start; false: a fresh start */
bool save_write(const tm_progress *p);
#endif
