/* The screens: the arcade's play area 1:1 (224 wide, its rows 16 to 255) right of a 96 px panel. */
#ifndef DK_DRAW_H
#define DK_DRAW_H
#include "dk.h"

enum { DK_SCREEN_TITLE, DK_SCREEN_GUIDE, DK_SCREEN_PLAY, DK_SCREEN_PAUSE, DK_SCREEN_CONFIRM, DK_SCREEN_OVER };

typedef struct {
  uint8_t screen, sel, confirm; /* confirm: 0 new game, 1 quit */
  uint8_t new_best, has_run;
  uint32_t best, games, clock;
  uint16_t best_level;
} dk_view;

void dk_draw(const dk_state *s, const dk_view *v);
void dk_draw_invalidate(void);
#endif
