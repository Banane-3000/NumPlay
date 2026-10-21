/* What's kept, in donkeykong.sav: the best score and a few totals, and a game left with Home (the arcade's
 * RAM and tile map, packed), with a copy in a Python script, donkeykong_saves.py (installing apps from the
 * NumWorks website keeps only scripts, so the save comes back from it). */
#ifndef DK_SAVE_H
#define DK_SAVE_H
#include "dk.h"

typedef struct {
  uint32_t best, games, boards;
  uint16_t best_level;
  uint8_t guide_seen, has_run;
} dk_profile;

#define DK_SAVE_NAME "donkeykong.sav"
#define DK_COPY_NAME "donkeykong_saves.py"
#define DK_MAGIC 0x31474B44u /* "DKG1" */
#define DK_VERSION 1
#define DK_SNAP 0xB00        /* the RAM kept: 0x6000-0x6AFF (the rest is the arcade's stack) */
#define DK_SAVE_MAX 4096

uint32_t dk_sum(const void *data, uint32_t n);
uint32_t dk_pack(const dk_state *s, uint8_t *out, uint32_t room);           /* 0 if it doesn't fit */
bool dk_unpack(dk_state *s, const uint8_t *in, uint32_t n);
bool dk_file_ok(const uint8_t *d, uint32_t n, dk_profile *p, dk_state *s); /* p, s filled when true */
void save_load(dk_profile *p, dk_state *s);   /* the save, its copy, or a fresh start */
bool save_write(const dk_profile *p, const dk_state *s);
#endif
