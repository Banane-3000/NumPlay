/* Drawing (draw.c): the tomb as the original's camera sees it, 12 pixels a tile with its own sprites and colours,
 * the bar on top, the key line at the bottom, and the menus. */
#ifndef TM_DRAW_H
#define TM_DRAW_H
#include <stdbool.h>
#include <stdint.h>
#include "game.h"

enum { V_TITLE, V_STAGES, V_PLAY, V_PAUSE, V_QUIT, V_HELP, V_REVIVE, V_RESULTS, V_SHOP, V_MASKS, V_MISSIONS, V_CHEST };
enum { M_ARCADE, M_STAGES, M_SHOP, M_MASKS, M_MISSIONS, M_CHEST, M_COUNT };
enum { PAUSE_RESUME, PAUSE_RESTART, PAUSE_HELP, PAUSE_QUIT, PAUSE_COUNT };

typedef struct {
  uint8_t screen, under;        /* under: the screen how to play shows over (and goes back to) */
  uint8_t sel;                  /* the title's, the pause menu's or the question's line */
  uint8_t page;                 /* how to play */
  uint8_t stage_sel;            /* the stage map */
  uint8_t shop_sel, mask_sel, mission_sel;
  uint8_t first_run;            /* how to play shown before a first run (OK starts it) */
  uint8_t save_failed;
  uint32_t revive_left;         /* the revive question's milliseconds left */
  uint32_t clock;               /* milliseconds */
  uint32_t run_coins;           /* the coins before the run (for the results) */
  tm_wheel wheel;
} tm_ui;

typedef void (*tm_push_fn)(int x, int y, int w, int h, const uint16_t *px);
void tm_draw(const tm_game *g, const tm_ui *v, tm_push_fn push);
#define TM_HELP_PAGES 3
#ifdef HOST
extern int tm_clipped;                       /* text that ran off the screen or out of its box (the tests look) */
extern const char *tm_clipped_text;
#endif
#endif
