/* What Turbo Rush keeps, in turborush.sav: your credits, car and records, the choices on the race screen, a Grand
 * Prix under way and the race you left. A copy goes into a Python script, turborush_saves.py (installing apps from
 * the NumWorks website keeps only scripts, so the save comes back from it). One format: a magic, a version, the
 * size and a checksum; anything else is not read, and the game starts afresh. */
#ifndef TR_SAVE_H
#define TR_SAVE_H
#include "game.h"

#define TR_SAVE_NAME "turborush.sav"
#define TR_COPY_NAME "turborush_saves.py"
#define TR_MAGIC 0x31555254u        /* "TRU1" */
#define TR_VERSION 1
#define TR_MAX_CREDITS 9999999u

typedef struct {
  uint32_t credits, races, wins, titles;
  uint32_t best_ms[TR_TRACKS], best_lap_ms[TR_TRACKS];
  uint8_t up[4];
  uint8_t paint, style, mode, track, level, traffic, mood, rain, manual, guide, spare[2];
} tr_profile;

typedef struct {
  uint8_t on, round;                /* round: the race to drive next, 0..5 */
  uint8_t skill[TR_CARS - 1];       /* each rival's skill, the same all the way */
  uint8_t spare;
  uint16_t points[TR_CARS];
} tr_gp;

typedef struct {
  uint32_t magic;
  uint16_t version, size;
  tr_profile p;
  tr_gp gp;
  uint8_t has_race, spare[5];
  tr_race race;
  uint32_t sum;                     /* FNV-1a of everything before it */
} tr_file;
_Static_assert(sizeof(tr_file) == 492, "turborush.sav: 492 bytes");

void tr_fresh(tr_profile *p, tr_gp *gp);
unsigned tr_price(const tr_profile *p, int item);   /* the next level of an upgrade (0: none left) */
uint32_t tr_sum(const void *data, uint32_t n);
bool tr_file_ok(const tr_file *f);
bool save_load(tr_file *f);         /* the save, its copy, or a fresh start (false) */
bool save_write(const tr_file *f);  /* with the magic, size and sum */
#endif
