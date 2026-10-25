/* The screens (draw.c): menus, the race's dials, results. */
#ifndef TR_DRAW_H
#define TR_DRAW_H
#include "game.h"
#include "save.h"

enum {
  V_TITLE, V_SETUP, V_NEWRUN, V_GUIDE, V_GARAGE, V_RECORDS, V_HELP, V_RACE, V_PAUSE, V_LEAVE, V_RESULT, V_STANDINGS,
  V_QUIT, V_SCREENS
};
enum { SET_MODE, SET_TRACK, SET_RIVALS, SET_TIME, SET_WEATHER, SET_GEARS, SET_START, SET_ROWS };
enum { GAR_ENGINE, GAR_TIRES, GAR_NITRO, GAR_ARMOR, GAR_PAINT, GAR_BODY, GAR_ROWS };
enum { PAUSE_RESUME, PAUSE_RESTART, PAUSE_HELP, PAUSE_QUIT, PAUSE_ROWS };
enum { MENU_CONTINUE, MENU_PLAY, MENU_GARAGE, MENU_RECORDS, MENU_HELP };
#define TR_HELP_PAGES 3

typedef struct {
  uint8_t screen, sel, page, under;           /* under: where Back goes from the garage or how to play */
  uint8_t menu[5], menu_n;                    /* the title's buttons */
  uint8_t result_rank, new_best, new_lap, final_rank;
  uint8_t gained[TR_CARS];                    /* Grand Prix points from the last race */
  uint16_t points[TR_CARS];                   /* the standings shown */
  uint8_t round_done;                         /* races of the Grand Prix driven, for the standings */
  uint8_t hold;                               /* a race going on again: 3, 2, 1 */
  uint32_t prize, bonus, clock;
  char note[48];                              /* the garage's last word */
  bool save_failed;
} tr_ui;

extern const char *const tr_driver[TR_CARS];
/* the whole screen; race: the race shown (the one going, the title's demo, or the last one) */
void tr_render(const tr_file *f, const tr_track *t, const tr_race *prev, const tr_race *race, int alpha,
               const tr_ui *ui);
#endif
