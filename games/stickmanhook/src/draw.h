/* The screens (draw.c), drawn 8 rows at a time into a small strip and pushed to the screen. */
#ifndef SH_DRAW_H
#define SH_DRAW_H
#include "game.h"

enum { S_TITLE, S_PLAY, S_PAUSE, S_QUIT, S_LEVELS, S_HELP, S_VICTORY, S_UNLOCKED, S_SKINS };
enum { SH_PAUSE_RESUME, SH_PAUSE_RESTART, SH_PAUSE_LEVELS, SH_PAUSE_HELP, SH_PAUSE_QUIT, SH_PAUSE_ITEMS };

typedef struct {
  uint8_t screen, sel, skin, new_skin, first_run;
  uint32_t clock;                /* milliseconds, for what moves on the menus */
  uint32_t done, current;        /* levels finished (the furthest), the level Play starts */
  uint32_t level_sel, level_top; /* the level grid: the chosen level, the first row shown */
  const sh_game *game;
  const sh_swing *swing;         /* the title's stickman */
} sh_view;

typedef void (*sh_push)(int x, int y, int w, int h, const uint16_t *px);
void sh_draw(const sh_view *v, sh_push push);
void sh_redraw(void);            /* push every row again next time */
#define SH_LEVEL_COLS 8
#define SH_LEVEL_ROWS 5
#endif
