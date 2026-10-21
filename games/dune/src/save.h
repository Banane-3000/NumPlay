/* What's kept: the best, the coins, the balls and landscapes bought and in use, and a run left with Home, in
 * dune.sav, and a copy of it in a Python script, dune_saves.py (installing apps from the NumWorks website keeps
 * only scripts, so the save comes back from it). */
#ifndef DN_SAVE_H
#define DN_SAVE_H
#include "game.h"

typedef struct {
  uint32_t best, coins, runs;
  uint32_t balls;              /* bought (bit 0, the Basic Ball, from the start) */
  uint8_t lands;               /* bought (bit 0, Night Land, from the start) */
  uint8_t ball, land, has_run;
} dn_profile;

#define DN_SAVE_NAME "dune.sav"
#define DN_COPY_NAME "dune_saves.py"
#define DN_MAGIC 0x454E5544u   /* "DUNE" */
#define DN_VERSION 1

typedef struct {
  uint32_t magic;
  uint16_t version, size;
  dn_profile p;
  uint8_t run[DN_RUN_SAVED];   /* the run, when has_run */
  uint32_t sum;                /* FNV-1a of everything before it */
} dn_file;

uint32_t dn_sum(const void *data, uint32_t n);
bool dn_file_ok(const dn_file *f, dn_run *scratch);
void dn_profile_new(dn_profile *p);
bool save_load(dn_profile *p, dn_run *r);    /* the save, its copy, or a fresh start; true if a run came back */
bool save_write(const dn_profile *p, const dn_run *r);
#endif
