/* Undertale: the game's state and its modes (title, overworld, battle, menus).
 *
 * Inspired by Undertale by Toby Fox, not affiliated with him.
 * Made by Mason Chen as part of NumPlay. */
#ifndef UT_GAME_H
#define UT_GAME_H
#include <stdbool.h>
#include <stdint.h>
#include "flags.h"

static inline int abs_i(int v) { return v < 0 ? -v : v; }
static inline int clamp_i(int v, int a, int b) { return v < a ? a : v > b ? b : v; }
static inline int min_i(int a, int b) { return a < b ? a : b; }
static inline int max_i(int a, int b) { return a > b ? a : b; }

enum { MODE_TITLE, MODE_NAME, MODE_WORLD, MODE_BATTLE, MODE_MENU, MODE_SAVE, MODE_GAMEOVER, MODE_INTRO, MODE_END };

#define NUM_ITEMS 8
typedef struct {
  /* what a SAVE keeps */
  char name[8];
  uint8_t lv;
  uint32_t exp;
  uint16_t gold;
  int16_t hp, maxhp;
  uint8_t at, df;              /* the game's own: 10 at LV 1, shown less 10 */
  uint8_t weapon, armor;       /* ITEM_* */
  uint8_t items[NUM_ITEMS];    /* ITEM_*, 0 none */
  uint8_t phone[8];            /* CELL: who can be called */
  uint16_t kills;
  uint32_t time;               /* frames played */
  int16_t room, x, y;          /* where it was saved */
  uint8_t flags[FLAG_COUNT];
} Save;
extern Save sv;                /* the game now */
extern int g_mode;
extern uint32_t g_frame;
extern bool g_running;

static inline int flag(int f) { return sv.flags[f]; }
static inline void set_flag(int f, int v) { sv.flags[f] = (uint8_t)v; }

void game_init(void);
void game_frame(uint32_t keys);
void game_draw(void);
void game_quit_save(void);     /* Home: the moment is kept, to come back to */
void game_open_menu(void);
void room_setup(int room, int from);   /* rooms.c: the room's objects */
void encounter_step(void);             /* rooms.c: a step towards a random encounter */
uint32_t rnd(void);
int rnd_n(int n);               /* 0 .. n - 1 */

/* stats */
int lv_maxhp(int lv);
int lv_at(int lv);
int lv_df(int lv);
int lv_exp_for(int lv);         /* the EXP to reach LV lv */
bool gain_exp(int exp);         /* true if LOVE went up */
int weapon_at(void);
int armor_df(void);

/* items: inventory */
bool item_add(int item);
void item_remove_at(int slot);
int item_count(void);
#endif
