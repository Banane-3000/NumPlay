/* What's kept, in fnf.sav, and a copy of it in a Python script, fnf_saves.py (installing apps from the NumWorks
 * website keeps only scripts, so the save comes back from it): the options, how to play seen, the menus' last
 * choices, each chart's best score and rank, and each week's best score. */
#ifndef FNF_SAVE_H
#define FNF_SAVE_H
#include <stdbool.h>
#include "data.h"

#define FNF_SAVE_NAME "fnf.sav"
#define FNF_COPY_NAME "fnf_saves.py"
#define FNF_MAGIC 0x53464E46u      /* "FNFS" */
#define FNF_VERSION 1
#define FNF_OFFSET_MAX 300         /* the note offset: -300..300 ms */

typedef struct {
  int32_t score;                   /* the best score */
  uint16_t completion;             /* the best clear's completion (ten-thousandths) */
  uint8_t rank;                    /* its rank + 1 (fnf_rank), 0: never cleared */
  uint8_t spare;
} fnf_record_t;

typedef struct {
  uint8_t downscroll, guide;       /* the options; how to play shown before the first song */
  int16_t offset;                  /* the note offset (ms) */
  uint8_t week, week_diff, song, song_diff, list, spare[3];   /* the menus' last choices */
  int32_t week_score[FNF_LEVELS][FNF_DIFFS];
  fnf_record_t chart[FNF_CHARTS];
} fnf_progress_t;

typedef struct {
  uint32_t magic;
  uint16_t version, size;
  fnf_progress_t p;
  uint32_t sum;                    /* FNV-1a of everything before it */
} fnf_file_t;

void fnf_fresh(fnf_progress_t *p);
uint32_t fnf_sum(const void *data, uint32_t n);
bool fnf_file_ok(const fnf_file_t *f);
bool save_load(fnf_progress_t *p);     /* the save, its copy, or a fresh start; false: a fresh start */
bool save_write(const fnf_progress_t *p);
/* a song cleared: the best score and the best rank kept (Save.applySongRank) */
void fnf_record(fnf_progress_t *p, unsigned chart, int32_t score, int rank, int32_t completion);
#endif
