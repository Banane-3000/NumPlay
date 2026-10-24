/* The screens (draw.c): menus, the race's dials, results. */
#ifndef PG_DRAW_H
#define PG_DRAW_H
#include "game.h"
#include "render.h"
#include "save.h"

enum { V_TITLE, V_SETUP, V_NEWRUN, V_GUIDE, V_HELP, V_RECORDS, V_RACE, V_PAUSE, V_LEAVE, V_RESULT, V_QUIT, V_SCREENS };
enum { SET_MODE, SET_TRACK, SET_CAR, SET_LAPS, SET_RIVALS, SET_LEVEL, SET_GEARS, SET_VIEW, SET_START, SET_ROWS };
enum { PAUSE_RESUME, PAUSE_RESTART, PAUSE_HELP, PAUSE_QUIT, PAUSE_ROWS };
enum { MENU_CONTINUE, MENU_PLAY, MENU_RECORDS, MENU_HELP };
#define PG_HELP_PAGES 3

typedef struct {
  uint8_t screen, sel, page, under;           /* under: where Back goes from how to play */
  uint8_t menu[4], menu_n;                    /* the title's buttons */
  uint8_t hold;                               /* a race going on again: 3, 2, 1 */
  uint8_t result_rank, new_best, new_lap, new_far;
  uint32_t clock;                             /* ms, for what moves on the menus */
  bool save_failed;
} pg_ui;

extern const char *const pg_driver[PG_CARS];
/* the whole screen; race: the race shown (the one going, or the last one) */
void pg_render(const pg_file *f, const pg_track *t, const pg_race *prev, const pg_race *race, int alpha,
               const pg_ui *ui, const rd_ghost *g);
#endif
