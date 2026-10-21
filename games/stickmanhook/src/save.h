/* What's kept (save.c): the levels finished, the level Play starts and its course, the skin, in
 * stickmanhook.sav, and a copy of it in a Python script, stickmanhook_saves.py (installing apps from the NumWorks
 * website keeps only scripts, so the save comes back from it). */
#ifndef SH_SAVE_H
#define SH_SAVE_H
#include "game.h"

typedef struct {
  uint32_t done;          /* levels finished: the furthest (the game's own counter) */
  uint32_t current;       /* the level Play starts (done, or one picked in Levels) */
  uint8_t skin, guide_seen, has_course, unused;
  sh_course course;       /* the course of `current`, while it's being played */
} sh_profile;

#define SH_SAVE_NAME "stickmanhook.sav"
#define SH_COPY_NAME "stickmanhook_saves.py"
#define SH_MAGIC 0x334B4853u   /* "SHK3" */
#define SH_VERSION 1

typedef struct {
  uint32_t magic;
  uint16_t version, size;
  sh_profile p;
  uint32_t sum;           /* FNV-1a of everything before it */
} sh_file;

bool sh_profile_ok(const sh_profile *p);
bool sh_file_ok(const sh_file *f);
void save_load(sh_profile *p);   /* the save, its copy, or a fresh start */
bool save_write(const sh_profile *p);
#endif
