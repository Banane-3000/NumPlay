/* Drawing (draw.c): the landscape, the dunes, the ball and its trail, the coins, the score, and the menus. */
#ifndef DN_DRAW_H
#define DN_DRAW_H
#include <stdbool.h>
#include <stdint.h>
#include "game.h"

enum { V_TITLE, V_PLAY, V_PAUSE, V_QUIT, V_RESULT, V_SHOP, V_BUY, V_HELP };
enum { PAUSE_RESUME, PAUSE_RESTART, PAUSE_SHOP, PAUSE_HELP, PAUSE_QUIT, PAUSE_ITEMS };
enum { TITLE_HELP, TITLE_SHOP, TITLE_PLAY };
enum { SHOP_BALLS, SHOP_LANDS };

typedef struct {
  uint8_t screen, sel, under;   /* under: the screen the shop, how to play or a question shows over */
  uint8_t shop_cat, shop_side;  /* the shop's category; the focus on the side bar (the categories) */
  uint8_t shop_sel, buy_yes;
  uint8_t ball, land;           /* equipped */
  uint32_t owned_balls, owned_lands, best, coins;
  uint32_t clock;               /* milliseconds, for the menus' motion */
  const dn_run *run;
} dn_view;

typedef void (*dn_push_fn)(int x, int y, int w, int h, const uint16_t *px);
void dn_draw(const dn_view *v, dn_push_fn push);
void dn_redraw(void);           /* the next frame pushes every strip */
#ifdef HOST
extern int dn_clipped;          /* text drawn past the screen's edge (the tests look) */
#endif
#endif
