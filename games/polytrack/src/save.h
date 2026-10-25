/* What PolyGT 3D keeps, in polygt3d.sav: your choices on the race screen, your records and the race you left. A copy
 * goes into a Python script, polygt3d_saves.py (installing apps from the NumWorks website keeps only scripts, so the
 * save comes back from it). One format: a magic, a version, the size and a checksum; anything else is not read, and
 * the game starts afresh. */
#ifndef PG_SAVE_H
#define PG_SAVE_H
#include "game.h"

#define PG_SAVE_NAME "polygt3d.sav"
#define PG_COPY_NAME "polygt3d_saves.py"
#define PG_MAGIC 0x31544750u        /* "PGT1" */
#define PG_VERSION 1
#define PG_MAX_TIME (99u * 60000u)
#define PG_MAX_M 2000000u           /* (2000 km) */

typedef struct {
  uint32_t races, wins, runs;       /* races finished, won; endless runs */
  uint32_t best_ms[PG_CIRCUITS], best_lap_ms[PG_CIRCUITS];
  uint32_t best_m;                  /* the endless road: the furthest (metres)... */
  uint16_t best_zone;               /* ...and zones */
  uint8_t mode, track, vehicle, laps, rivals, level, manual, view, guide, spare[3];
} pg_profile;

typedef struct {
  uint32_t magic;
  uint16_t version, size;
  pg_profile p;
  uint8_t has_race, spare[3];
  pg_race race;
  uint32_t sum;                     /* FNV-1a of everything before it */
} pg_file;
_Static_assert(sizeof(pg_file) == 496, "polygt3d.sav: 496 bytes");

void pg_fresh(pg_profile *p);
bool pg_file_ok(const pg_file *f);
bool save_load(pg_file *f);         /* the save, its copy, or a fresh start (false) */
bool save_write(const pg_file *f);  /* with the magic, size and sum */
#endif
