/* Doom on the calculator: what the platform's parts say to each other. */
#ifndef DOOM_NW_H
#define DOOM_NW_H
#include "doomtype.h"

/* the zone on a computer: as much as the calculator's own software leaves */
#define NW_HOST_ZONE (60 * 1024)

/* i_system.c: Home, On/Off and Quit end the main loop; I_Error and I_Quit jump back to main
 * (__builtin_setjmp and __builtin_longjmp on nw_exit) */
extern boolean nw_quit;
extern void *nw_exit[5];

/* i_video.c: an error message on the screen, until a key is pressed */
void nw_show_error(const char *msg);

/* i_input.c: Home or On/Off is pressed (the moment is kept, then the app quits) */
extern boolean nw_home;
void nw_input_init(void);

/* saves.c: the save slots (doomsav0.dsg to doomsav5.dsg), the moment kept on Home (slot NW_RESUME_SLOT,
 * read once on the next start) and the copy of them all in doom_saves.py */
#define NW_RESUME_SLOT 6
#define NW_SLOTS 7
void nw_saves_restore(void);       /* from doom_saves.py, when the files themselves are gone */
boolean nw_resume_ready(void);
void nw_resume_save(void);          /* the game as it is now, if there is one to keep */
void nw_resume_done(void);          /* the moment was read back: it goes */
void nw_settings_load(void);
void nw_settings_save(void);
#endif
