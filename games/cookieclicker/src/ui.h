/* Cookie Clicker's screen: the left column (the big cookie), the store, the buildings' rows and the
 * minigames, the menus, ascension; and the keys. */
#ifndef CC_UI_H
#define CC_UI_H
#include <stdbool.h>
#include <stdint.h>
#include "game.h"
#include "gfx.h"

/* the fonts' characters past ASCII (tools/pack.py's CHARS) */
#define CH_BULLET "\x95"
#define CH_LQUOTE "\x99"
#define CH_RQUOTE "\x9a"
#define CH_TIMES "\x92"

/* what the right of the screen shows */
enum { VIEW_STORE, VIEW_ROWS };
/* the screens over the game */
enum { OV_NONE, OV_MENU, OV_STATS, OV_OPTIONS, OV_INFO, OV_PROMPT, OV_SPECIAL, OV_AURA, OV_PERMANENT, OV_KEYS,
       OV_ROWMENU };

/* the store's sections, for the choice of what is selected */
enum { SEC_UPGRADES, SEC_SWITCHES, SEC_RESEARCH, SEC_BUILDINGS, N_SEC };
#define CRATE 29             /* an upgrade's crate in the store */
#define CRATES_PER_ROW 6
#define PRODUCT_H 32         /* a building's row in the store */
#define STORE_TOP (NEWS_H + 14)

typedef struct {
  int view, overlay;
  int sec, idx;              /* the store's selection */
  float scroll, scroll_t;    /* the store's scroll (pixels) and where it heads */
  int32_t tip_t;             /* frames the tooltip still shows */
  int menu_sel, prompt_sel, list_sel, list_top, stats_tab;
  bool from_menu;            /* the page was opened from the menu (Back goes back to it) */
  int prompt_kind, prompt_arg;
  int rows_sel;              /* the buildings' rows: the one chosen */
  float rows_scroll;
  int special;               /* 0 santa, 1 dragon */
  int aura_slot, aura_sel;
  int perm_slot, perm_sel;
  /* the news */
  char news[2][200];
  int news_cur;
  int32_t news_t, news_fade;
  int asc_mode;              /* ascension: the challenge mode chosen */
  float asc_x, asc_y, asc_tx, asc_ty;   /* where the heavens are (and are going) */
  bool store_dirty;
} Ui;
extern Ui U;

/* the prompts */
enum { PROMPT_UPGRADE = 1, PROMPT_ASCEND, PROMPT_LEVEL, PROMPT_SELL_ALL, PROMPT_WIPE, PROMPT_REINCARNATE };
void prompt_open(int kind, int arg);
void draw_prompt(void);

void ui_init(void);
void ui_tick(void);          /* a logic frame */
void ui_keys(uint32_t pressed, uint32_t held);
void ui_frame(void);         /* draws the screen */
void ui_store_layout(void);
void ui_news_next(void);

/* drawing parts */
void draw_left(void);
void draw_shimmers(void);
void draw_buffs(void);
void draw_notes(void);
void draw_popups(void);
void draw_store(void);
void draw_tooltip(void);
void draw_news(void);
void draw_rows(void);
void draw_overlay(void);
void draw_ascend(void);
void draw_frame_box(int x, int y, int w, int h);   /* the game's framed box */
void draw_crate(int icon, int x, int y, int state, bool shadow);   /* state: 0 can buy, 1 selected, 2 can't */
void draw_price(int x, int y, double price, bool can, int money_im);
void store_move(int dx, int dy);
int store_selected_y(int *h);        /* where the selection is in the store's content */
int store_content_h(void);
int store_buildings_top(void);
int store_selected_upgrade(void);    /* -1 if the selection is a building */
int store_selected_building(void);

extern const ParaStyle PS_TIP, PS_QUOTE, PS_SMALL;

/* an upgrade's (type 0) or achievement's (type 1) tooltip, as the store, the statistics or the
 * ascension show it: measured (draw false) or drawn at (x, y) w wide; returns its height */
enum { CTX_STORE, CTX_STATS, CTX_ASCEND };
int crate_tip(int type, int id, int context, int x, int y, int w, bool draw);
void tip_line(int x, int y, int w);   /* the game's faded line across */
int tip_tag(int x, int y, const char *s, uint16_t bg);   /* returns its width */
#endif
