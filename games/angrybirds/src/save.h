/* Angry Birds: what is kept, in angrybirds.sav, and a copy of it in a Python script, angrybirds_saves.py
 * (installing apps from the NumWorks website keeps only scripts, so the save comes back from it).
 *
 * One format: a magic, a version, its size, then the progress, then a checksum of all before it. A file that is
 * damaged, cut short or from elsewhere is not read: the copy is tried, then a fresh start. */
#ifndef AB_SAVE_H
#define AB_SAVE_H
#include <stdbool.h>
#include <stdint.h>
#include "data.h"

#define AB_SAVE_NAME "angrybirds.sav"
#define AB_COPY_NAME "angrybirds_saves.py"
#define AB_MAGIC 0x31534241u     /* "ABS1" */
#define AB_VERSION 1

enum { LV_CLEARED = 1, LV_FEATHER = 2, LV_OPEN = 4 };

typedef struct {
  uint32_t best[LEVEL_COUNT];     /* best score without the Mighty Eagle */
  uint8_t stars[LEVEL_COUNT];     /* 0 to 3 */
  uint8_t eagle[LEVEL_COUNT];     /* the Mighty Eagle's best percent */
  uint8_t flags[LEVEL_COUNT];     /* LV_* */
  uint16_t last;                  /* the last level played */
  uint8_t guide;                  /* the first-run guide was shown */
  uint8_t eagle_lock_on;          /* the Mighty Eagle rests (an hour of play) but for the level it was used on */
  uint16_t eagle_level;
  uint16_t pad;
  uint32_t eagle_since;           /* play time (s) when it was used */
  uint32_t played;                /* seconds of play */
} Progress;

typedef struct {
  uint32_t magic;
  uint16_t version, size;
  Progress p;
  uint32_t sum;                   /* FNV-1a of everything before it */
} SaveFile;

/* the memory saving and loading work in (the work buffer main.c keeps on the stack, free while the game saves):
   at least SAVE_SCRATCH bytes */
#define SAVE_SCRATCH 8192
void save_scratch(void *mem);
void save_fresh(Progress *p);
uint32_t save_sum(const void *data, uint32_t n);
bool save_ok(const SaveFile *f);
bool save_load(Progress *p);      /* false: a fresh start */
bool save_write(const Progress *p);
#endif
