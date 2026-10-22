/* What's kept (the game's own save: coins, hats, the mart, each mart's places, cash box and upgrades), in
 * monkeymart.sav, and a copy of it in a Python script, monkeymart_saves.py (installing apps from the NumWorks
 * website keeps only scripts, so the save comes back from it). As in the game, a mart's shelves and what the
 * monkeys carry are not kept, and nothing is kept before the tutorial ends. */
#ifndef MM_SAVE_H
#define MM_SAVE_H
#include "game.h"

#define MM_SAVE_NAME "monkeymart.sav"
#define MM_COPY_NAME "monkeymart_saves.py"
#define MM_MAGIC 0x544D4D4Du      /* "MMMT" */
#define MM_VERSION 1

typedef struct {
  uint32_t magic;
  uint16_t version, size;
  mm_progress p;
  uint32_t sum;                    /* FNV-1a of everything before it */
} mm_file;

uint32_t mm_sum(const void *data, uint32_t n);
bool mm_file_ok(const mm_file *f);
bool save_load(mm_progress *p);    /* the save, its copy, or a fresh start; false: a fresh start */
bool save_write(const mm_progress *p);
#endif
