/* UNO's rules (game.c) and its bots (bot.c), as Mattel's instruction sheet has them: the 108-card deck, the
 * action cards, the Wild Draw Four and its challenge, UNO calls and their penalty, and hands scored until
 * someone has 500 points. No screen, clock or files here: the tests drive it on a computer. */
#ifndef UNO_GAME_H
#define UNO_GAME_H
#include <stdbool.h>
#include <stdint.h>

#define UNO_DECK 108
#define UNO_NONE 0xFF
#define UNO_GOAL 500

/* A card: colour * 13 + value for the 100 coloured ones (0 to 9, then Skip, Reverse, Draw Two), then the Wild
 * and the Wild Draw Four. */
enum { V_SKIP = 10, V_REVERSE = 11, V_DRAW2 = 12, C_WILD = 52, C_WILD4 = 53, UNO_KINDS = 54 };
enum { RED, YELLOW, GREEN, BLUE, NO_COLOR = 4 };
static inline int uno_color(int c) { return c < 52 ? c / 13 : NO_COLOR; }
static inline int uno_value(int c) { return c < 52 ? c % 13 : c; }

/* house rules, none of them official: all off by default */
enum { H_STACK2 = 1, H_STACK4 = 2, H_DRAW_UNTIL = 4, H_MUST_PLAY = 8, H_AUTO_UNO = 16, H_ALL = 31 };
enum { LEVEL_EASY, LEVEL_NORMAL, LEVEL_HARD };

typedef struct {
  uint8_t players;    /* 2 to 4: you (player 0) and 1 to 3 bots */
  uint8_t level;      /* the bots' */
  uint8_t one_hand;   /* 1: a single hand; 0: hands until someone has 500 points */
  uint8_t house;
} uno_rules;

/* what the player whose turn it is has to do */
enum {
  P_PLAY,       /* play a card or draw one */
  P_DRAWN,      /* play the card just drawn (only that one), or keep it */
  P_COLOR,      /* call the colour of the Wild just played */
  P_PENALTY,    /* a Wild Draw Four (or, stacking, a Draw Two) waits: take it, challenge it, or stack on it */
  P_HAND_OVER,  /* the hand is over and scored */
  P_GAME_OVER,
};

/* what happened last, for the screen */
enum { E_NONE, E_DEAL, E_PLAY, E_DRAW, E_KEEP, E_COLOR, E_TAKE, E_CHALLENGE_WON, E_CHALLENGE_LOST, E_UNO, E_CAUGHT };

typedef struct {
  uno_rules r;
  uint8_t hand[4][UNO_DECK];
  uint8_t count[4];
  uint8_t deck[UNO_DECK];   /* the draw pile, its top the last */
  uint8_t pile[UNO_DECK];   /* the discard pile, its top the last */
  uint8_t dn, pn;
  uint16_t score[4];
  uint16_t hands;           /* hands dealt in this game */
  uint32_t rng;             /* the shuffles' */
  uint32_t brng;            /* the bots' choices */
  uint8_t dealer, turn, phase;
  int8_t dir;               /* 1: clockwise, to the left; -1 */
  uint8_t color;            /* the colour to match: the top card's, or the one called for a Wild */
  uint8_t opening;          /* P_COLOR: the first player calls the colour of the first card, then plays */
  uint8_t pending;          /* P_PENALTY: the cards to take */
  uint8_t pending_kind;     /* V_DRAW2 or C_WILD4 */
  uint8_t offender;         /* who played the Wild Draw Four waiting */
  uint8_t challengeable;    /* it can be challenged (not one stacked on another) */
  uint8_t guilty;           /* its player had a card of the colour before it */
  uint8_t prev_color;       /* that colour */
  uint8_t drawn;            /* P_DRAWN: the card's place in the hand */
  uint8_t uno_open;         /* someone down to one card who did not call UNO, until the next player starts */
  uint8_t uno_said;         /* the player whose turn it is called UNO before playing the next-to-last card */
  uint8_t out;              /* who played the last card, while its Draw Two or Wild Draw Four waits */
  uint8_t winner;           /* the hand's, or the game's at P_GAME_OVER */
  uint8_t spare0;
  uint16_t gained;          /* the points the hand's winner scored */
  uint8_t lacks[4];         /* colours a player drew on rather than play (what everyone at the table saw) */
  uint8_t ev, ev_who, ev_card, ev_n, ev_victim, ev_by;
  uint8_t spare[2];
} uno_game;
_Static_assert(sizeof(uno_game) == 708, "uno_game: packed bytes, no padding");

int uno_points(int card);
int uno_next(const uno_game *g, int p);
void uno_new_game(uno_game *g, uno_rules r, uint32_t seed);
void uno_next_hand(uno_game *g);
void uno_fill_deck(uint8_t *deck);          /* the 108 cards, in order */
void uno_deal(uno_game *g);                  /* deals from g->deck as it is (uno_next_hand shuffles first) */
bool uno_matches(const uno_game *g, int card);
bool uno_can_play(const uno_game *g, int i); /* card i of the hand of the player whose turn it is */
bool uno_legal_wild4(const uno_game *g, int p);   /* p holds no card of the colour in play */
bool uno_play(uno_game *g, int i, bool say_uno);
bool uno_choose(uno_game *g, int color);
bool uno_draw(uno_game *g);
bool uno_keep(uno_game *g);
bool uno_take(uno_game *g);
bool uno_challenge(uno_game *g);
bool uno_call(uno_game *g, int p);
bool uno_catch(uno_game *g, int by);
bool uno_can_stack(const uno_game *g);       /* P_PENALTY: the player has a card to stack */
bool uno_valid(const uno_game *g);           /* every card once, every field in range (a save to trust) */

/* the bots (bot.c): what the player whose turn it is does, at a level, from what that player can see */
enum { M_PLAY, M_DRAW, M_KEEP, M_TAKE, M_CHALLENGE, M_COLOR };
typedef struct {
  uint8_t act, arg;  /* arg: the card's place (M_PLAY), the colour (M_COLOR) */
  bool uno;          /* say UNO with this card */
} uno_move;
uno_move bot_move(uno_game *g, int level);
bool bot_do(uno_game *g, uno_move m);
/* whether bot `by` catches the player who forgot UNO, and how long it takes (ms) */
bool bot_catches(uno_game *g, int by, int level, uint32_t *ms);
int bot_best_color(const uno_game *g, int p, int level);
#endif
