/* Classic 108-card UNO. The rules engine has no graphics, clock or heap. */
#ifndef UNO_H
#define UNO_H
#include <stdint.h>
#include <stdbool.h>
#define UNO_CARDS 108
#define UNO_NONE 255
enum { UNO_SKIP=10, UNO_REVERSE=11, UNO_DRAW2=12, UNO_WILD=52, UNO_DRAW4=53 };
enum { U_TURN, U_COLOR, U_CHALLENGE, U_DRAWN, U_ROUND, U_MATCH };
enum { R_STACK2=1, R_STACK4=2, R_DRAW_UNTIL=4, R_FORCE=8, R_AUTO_UNO=16 };
enum { E_START, E_PLAY, E_DRAW, E_PASS, E_COLOR, E_PENALTY, E_CHALLENGE_WON,
       E_CHALLENGE_LOST, E_UNO, E_CAUGHT, E_ROUND, E_MATCH, E_STALEMATE };
typedef struct { uint8_t players, difficulty, flags; } UnoRules;
typedef struct {
  UnoRules rules;
  uint8_t hands[4][UNO_CARDS], count[4];
  uint8_t deck[UNO_CARDS], discard[UNO_CARDS], dn, xn;
  uint16_t round, turns;
  uint32_t rng;
  uint8_t dealer, turn, color, phase, pending, penalty_kind, offender;
  uint8_t guilty, old_color, drawn, candidate, winner, uno_player, uno_armed;
  uint8_t stalls, event, actor, card, amount;
  int8_t direction;
} Uno;
static inline int uno_color(int c) { return c<52?c/13:4; }
static inline int uno_value(int c) { return c<52?c%13:c; }
void uno_new(Uno*, UnoRules, uint32_t);
void uno_next_round(Uno*);
bool uno_can_play(const Uno*,int);
bool uno_play(Uno*,int,bool);
bool uno_choose_color(Uno*,int);
bool uno_draw(Uno*);
bool uno_keep(Uno*);
bool uno_accept(Uno*);
bool uno_challenge(Uno*);
bool uno_call(Uno*,int);
bool uno_catch(Uno*,int);
int uno_bot_card(const Uno*);
int uno_bot_color(const Uno*,int);
void uno_bot(Uno*);
bool uno_valid(const Uno*);
#endif
