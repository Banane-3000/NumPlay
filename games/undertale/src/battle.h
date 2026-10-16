/* Undertale: battles.
 *
 * The original's battle is 640 x 480; here it is halved, as everything else:
 * the box, the buttons and the bullets at half their size, the monsters at the
 * size the game draws them (it doubles its sprites). Positions on the bullet
 * board are kept in 1/16 pixels, so bullets move as finely as there.
 *
 * A battle is a formation (FORM_*: who is fought, where, its music none) of
 * up to three monsters (MonDef: stats, texts, ACTs, attacks). A turn: the
 * player picks FIGHT, ACT, ITEM or MERCY; the monsters talk in their bubbles;
 * the box shrinks to the bullet board and the monsters attack the SOUL. */
#ifndef UT_BATTLE_H
#define UT_BATTLE_H
#include <stdbool.h>
#include <stdint.h>
#include "text.h"

enum { BR_WON, BR_SPARED, BR_FLED, BR_DIED, BR_SCRIPTED };

#define FX 16                /* fixed point: 1/16 pixel */
#define MAX_MON 3
#define MAX_BULLETS 96

/* the box: the menu's, wide; the board's for attacks, by default */
#define BOX_X0 16
#define BOX_Y0 125
#define BOX_X1 303
#define BOX_Y1 195
#define BOARD_W 77
#define BOARD_H 65

struct Battle;
struct Monster;

typedef struct MonDef {
  const char *name;
  int16_t hp, at, df, exp, gold;
  int16_t spare_gold;               /* the gold it gives when spared */
  uint16_t spr_idle, spr_hurt;      /* SPR_*: idle animation (its frames), hurt */
  uint8_t idle_speed;               /* frames between idle frames */
  int8_t dx, dy;                    /* where it stands, from its slot */
  const char *check;                /* after "* NAME - ATK n DEF n\n" */
  const char *acts[5];              /* besides Check */
  uint8_t nacts;
  const char *encounter;            /* the first turn's text, alone */
  const char *neutral[6];           /* flavour texts, at random */
  const char *quotes[6];            /* what it says, at random */
  const char *low_hp, *spareable;   /* flavour texts then */
  uint8_t flags;                    /* MF_* */
  /* its own ways: an ACT (fills text, may set m->spare), its talk this turn, its attack this turn */
  void (*act)(struct Battle *b, struct Monster *m, int act, char *text, int cap);
  const char *(*talk)(struct Battle *b, struct Monster *m);
  int (*attack)(struct Battle *b, struct Monster *m);
  const char *(*flavor)(struct Battle *b, struct Monster *m);
  bool (*on_hit)(struct Battle *b, struct Monster *m, int dmg);   /* true: its own way (no death) */
  void (*draw)(struct Battle *b, struct Monster *m, int x, int y);
  bool (*ends)(struct Battle *b, struct Monster *m);   /* after its talk: true, it leaves the battle */
  void (*on_spare)(struct Battle *b, struct Monster *m);   /* spared while it isn't spareable (Toriel counts) */
  /* an item used: true if it answers (the dogs and the Stick), its text in text */
  bool (*on_item)(struct Battle *b, struct Monster *m, int item, char *text, int cap);
} MonDef;
enum {
  MF_ONE_HIT = 1,      /* dies in one hit (Whimsun) */
  MF_BOSS = 2,         /* no fleeing */
  MF_NO_CHECK = 4,
  MF_SPARE_START = 8,  /* spareable from the start */
  MF_NOT_KILLABLE = 16,
  MF_MISS_IS_SPARE = 32  /* a missed FIGHT counts as a spare (Toriel) */
};

typedef struct Monster {
  const MonDef *def;
  int16_t hp, maxhp, at, df, exp, gold;
  int16_t x, y;                /* its feet's middle */
  bool alive, gone, spare;     /* gone: killed, spared or ran */
  bool killed;
  uint8_t fade, dust;          /* fading out (spared or ran) / turning to dust: frames */
  uint8_t hurt;                /* frames showing hurt */
  int16_t shake;               /* hurt: its shake */
  int16_t acts[6];             /* each ACT, how often */
  int16_t state, state2, turns;
  const char *say;             /* its bubble this turn */
  int16_t dmg_shown, dmg_t;    /* the damage number over it */
  int16_t bar_shown;           /* its HP before the hit (the bar's fall) */
  int16_t spare_gold;          /* the gold it gives when spared */
  int16_t spr_override;        /* a sprite its ways show instead, 0 none */
} Monster;

/* the bullets: a pool, moved by kind each frame; the attack's code makes them and steers them */
enum { BK_NONE, BK_LINEAR, BK_GRAVITY, BK_HOMING, BK_ORBIT, BK_SCRIPT };
enum { BC_WHITE, BC_BLUE, BC_ORANGE, BC_GREEN, BC_YELLOW };
typedef struct {
  uint8_t kind, color, frame, flags;
  uint16_t spr;
  int32_t x, y, vx, vy, ax, ay;    /* 1/16 pixels (and per frame, per frame squared) */
  int16_t r;                       /* its hit radius (pixels), a box if w/h */
  int16_t w, h;
  int16_t t, life;                 /* frames alive; dies at life (0: never) */
  int16_t a, b, c, d;              /* the attack's own */
  uint8_t anim, alpha;
  int8_t dmg;                      /* 0: the monster's AT */
  int16_t angle;                   /* drawn turned (1/256 turns) */
  uint8_t owner;                   /* the monster that made it */
} Bullet;
enum { BF_NOCLIP = 1, BF_ONCE = 2, BF_HIDDEN = 4, BF_BOX = 8, BF_FRONT = 16, BF_HARMLESS = 32, BF_SHIELD = 64,
       BF_PLAYER = 128 };   /* (shield: the green SOUL's blocks it; player: the yellow SOUL's own shot) */

typedef struct Battle {
  int form;
  int phase, t;
  Monster mon[MAX_MON];
  int nmon;
  int greens;                  /* green (healing) bullets the SOUL caught this battle */
  int button, target, act, item, mercy, page;
  Writer w;                    /* the box's text */
  char text[600];
  /* the box now, and where it is going */
  int16_t x0, y0, x1, y1, tx0, ty0, tx1, ty1;
  /* the SOUL */
  int32_t sx, sy;              /* its middle, 1/16 pixels */
  int soul_mode;
  int shield;                  /* green: the way its shield faces (DIR_*: down, up, left, right) */
  int inv;                     /* invincible for frames */
  int speed;                   /* 1/16 pixels a frame */
  /* the attack */
  int atk, atk_t, atk_len;
  int atk_owner;
  int turn;
  int bar_x, bar_dir;          /* FIGHT: the bar, 1/16 px */
  int strike_t, strike_dmg;
  bool flee_ok, scripted, no_flee, no_buttons;
  bool captures;               /* no death: at 0 HP the battle ends, HP 1 (Papyrus captures you) */
  int result;
  int exp_won, gold_won;
  int kr;                      /* (for later) */
  int df_loss;                 /* DEFENSE lost this battle (Vulkin's hugs) */
  int16_t state[8];            /* the formation's own */
  int32_t as[16];              /* the attack's own */
  int talk_i;                  /* whose bubble shows */
  Writer bw;                   /* the bubble's text */
  int hp_shown;
  int last_action;             /* 0 FIGHT, 1 ACT, 2 ITEM, 3 Spare, 4 Flee */
} Battle;
extern Battle bt;
extern Bullet bullets[MAX_BULLETS];

void battle_start(int form);
bool battle_over(void);
int battle_result(void);
/* what the battle waits for (for the tests): a button, a target, an ACT, an item, MERCY's choice or nothing */
enum { BUI_NONE, BUI_MENU, BUI_TARGET, BUI_ACTS, BUI_ITEMS, BUI_MERCY, BUI_BAR };
int battle_ui(void);
void battle_update(uint32_t keys, uint32_t pressed);
void battle_draw(void);

/* for the attacks (bullets.c) */
Bullet *bullet_new(uint16_t spr, int x, int y, int vx, int vy);   /* x, y in 1/16 px */
void bullets_clear(void);
void bullets_update(void);
void bullets_draw(void);
bool soul_hit_test(void);              /* bullets touching the SOUL hurt it */
void soul_update(uint32_t keys);
/* (the heart's frame too): green stays put, its shield turns; purple keeps to three strings across the board
 * (up and down hop between them); yellow shoots up (Z) */
enum { SOUL_RED, SOUL_BLUE, SOUL_GREEN, SOUL_PURPLE, SOUL_YELLOW };
#define NSTRINGS 3
int soul_string_y(int i);              /* the purple SOUL's strings: where string i is now (pixels) */
extern int32_t soul_strings_shift;     /* how far they've been pulled down (1/16 px; they come round from the top) */
void soul_set_mode(int mode);          /* blue: it falls, up jumps */
void gfx_bone(int cx, int cy, int w, int h, uint16_t col, uint8_t a);
void soul_hurt(int dmg, int color);
void board_set(int w, int h);          /* the board, centred, as the game resizes it */
void board_set_rect(int x0, int y0, int x1, int y1);
int board_cx(void), board_cy(void);    /* its middle (pixels) */
int soul_x(void), soul_y(void);        /* (pixels) */
int damage_to_player(int at);
int count_alive(void);
/* for the battles that play their own way (their draw) */
void battle_draw_hud(void);
void battle_draw_buttons(void);
/* those battles (flowey.c, htshows.c): set up, each frame (bt.result = BR_SCRIPTED ends it), drawn */
void flowey_battle_start(void);
void flowey_battle_update(uint32_t keys, uint32_t pressed);
void flowey_battle_draw(void);
void quiz_start(void);
void quiz_update(uint32_t keys, uint32_t pressed);
void quiz_draw(void);
void cook_start(void);
void cook_update(uint32_t keys, uint32_t pressed);
void cook_draw(void);
int cook_won(void);          /* the substitute reached in time */
void lasers_start(void);
void lasers_update(uint32_t keys, uint32_t pressed);
void lasers_draw(void);

/* the monsters and formations (monsters.c) */
const MonDef *mon_def(int id);
int form_setup(int form, Battle *b);   /* places the monsters; returns the encounter text's id */
const char *form_text(int form);
void attack_run(Battle *b, int atk, int t);   /* one frame of attack atk (ATK_*) */
void attack_draw(Battle *b, int atk, int t);  /* what it draws besides its bullets */
int attack_length(int atk);
#endif
