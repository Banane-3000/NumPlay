/* The screens (draw.c), drawn with NumPlay's launcher graphics and fonts, in strips. */
#ifndef UNO_DRAW_H
#define UNO_DRAW_H
#include "game.h"

enum { V_TITLE, V_SETUP, V_HELP, V_PLAY, V_REVEAL, V_PAUSE, V_QUIT };
enum { MENU_CONTINUE, MENU_NEW, MENU_HELP };
enum { PAUSE_RESUME, PAUSE_NEW, PAUSE_HELP, PAUSE_QUIT, PAUSE_COUNT };
enum { ROW_PLAYERS, ROW_LEVEL, ROW_LENGTH, ROW_HOUSE, ROW_DEAL = ROW_HOUSE + 5, SETUP_ROWS };
enum { PEN_TAKE, PEN_CHALLENGE, PEN_STACK };
#define UNO_HELP_PAGES 5

typedef struct {
  uint8_t screen, under;  /* under: what a pause or how to play sits on */
  uint8_t sel;            /* the menu's or the panel's choice */
  uint8_t page;           /* how to play */
  uint8_t first_run;      /* how to play before the first game: OK deals */
  uint8_t pick;           /* the card chosen in your hand */
  uint8_t has_game;
  uint8_t show_end;       /* the hand's end: the scores are up */
  uno_rules setup;
  /* a challenge you made: the hand you were shown */
  uint8_t reveal[UNO_DECK], reveal_n, reveal_who, reveal_color, reveal_guilty;
  char msg[72];
  uint32_t clock;
} uno_ui;

const char *uno_name(int p);
void uno_card_name(int card, char *out);
const char *uno_keys(const uno_game *g, const uno_ui *v);
void uno_render(const uno_game *g, const uno_ui *v);
int uno_keyline_width(const char *keys);   /* (pixels) */
int uno_text_width(const char *s);         /* the small font's */
int uno_help_bottom(int page);
/* the menus' items: how many there are, and the n-th */
int uno_menu_count(const uno_ui *v);
int uno_menu_item(const uno_ui *v, int n);
/* the +4 panel's choices: PEN_TAKE, PEN_CHALLENGE and, stacking, PEN_STACK */
int uno_pen_count(const uno_game *g);
#endif
