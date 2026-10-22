/* Drawing (draw.c): the mart as the game's camera sees it, a third of a unit a pixel, its clouds and pads, the
 * money and the alerts, the key line, and the windows. */
#ifndef MM_DRAW_H
#define MM_DRAW_H
#include <stdbool.h>
#include <stdint.h>
#include "game.h"

enum { V_TITLE, V_PLAY, V_PAUSE, V_QUIT, V_HELP, V_UPGRADES, V_HATS, V_MARTS };
enum { PAUSE_RESUME, PAUSE_MARTS, PAUSE_HELP, PAUSE_QUIT };

typedef struct {
  uint8_t screen, under;        /* under: the screen how to play shows over */
  uint8_t sel;                  /* the pause menu's or the question's choice */
  uint8_t items[4], nitems;     /* the pause menu's lines (PAUSE_*) */
  uint8_t up_row, up_col, up_top;
  uint8_t hat_sel, hat_top;
  uint8_t mart_sel;
  uint8_t page;                 /* how to play */
  uint8_t save_failed;
  uint32_t clock;               /* milliseconds */
} mm_ui;

typedef void (*mm_push_fn)(int x, int y, int w, int h, const uint16_t *px);
void mm_draw(const mm_game *g, const mm_ui *v, mm_push_fn push);
unsigned mm_hat_count(void);                 /* the hats the shop lists (not the ones the game gave for ads) */
unsigned mm_hat_at(unsigned i);
#define MM_HELP_PAGES 3
#ifdef HOST
extern int mm_clipped;                       /* text that ran off the screen (the tests look) */
extern const char *mm_clipped_text;
#endif
#endif
