/* Undertale: saves (the game's SAVE, the moment kept on Home), and their copy in a Python script. */
#ifndef UT_SAVE_H
#define UT_SAVE_H
#include <stdbool.h>
#include "game.h"
void save_restore_copy(void);
bool save_write(void);
bool save_exists(void);
bool save_load(void);
bool save_peek(Save *s);   /* the SAVE's facts, without loading it */
void save_resume_write(void);
bool save_resume_load(void);
#endif
