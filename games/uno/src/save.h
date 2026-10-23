/* What's kept (the new game screen's choices, whether how to play was read, the game going on), in uno.sav,
 * and a copy of it in a Python script, uno_saves.py (installing apps from the NumWorks website keeps only
 * scripts, so the save comes back from it). */
#ifndef UNO_SAVE_H
#define UNO_SAVE_H
#include "game.h"

#define UNO_SAVE_NAME "uno.sav"
#define UNO_COPY_NAME "uno_saves.py"
#define UNO_MAGIC 0x314F4E55u      /* "UNO1" */
#define UNO_VERSION 1

typedef struct {
  uno_rules setup;                 /* the new game screen's choices */
  uint8_t guide;                   /* how to play shown before the first game */
  uint8_t has_game;                /* a game to go on with */
  uint8_t spare[2];
  uno_game g;
} uno_progress;

typedef struct {
  uint32_t magic;
  uint16_t version, size;
  uno_progress p;
  uint32_t sum;                    /* FNV-1a of everything before it */
} uno_file;
_Static_assert(sizeof(uno_file) == 728, "uno.sav: 728 bytes");

void uno_fresh(uno_progress *p);
uint32_t uno_sum(const void *data, uint32_t n);
bool uno_file_ok(const uno_file *f);
bool save_load(uno_progress *p);   /* the save, its copy, or a fresh start; false: a fresh start */
bool save_write(const uno_progress *p);
#endif
