/* UNO on a computer, for tests and pictures (make host, make test).
 *
 *   play test DIR    the rules against Mattel's instruction sheet (every card, two players, the challenge, UNO and
 *                    its penalty, going out, scoring to 500, the house rules), the bots playing whole games to 500
 *                    at every level (and the same game twice, to the same hash on every computer), a few hundred
 *                    sessions of the whole app with random keys, saves written and read back, damaged saves
 *   play shots DIR   pictures of the screens (DIR/<name>.ppm)
 *   play film DIR MS SEED   MS milliseconds of a game, a bot playing your cards too (DIR/<frame>_<ms>.raw, for
 *                    tools/record.py --frames)
 */
#define main uno_app_main
#include "../src/main.c"
#undef main
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

/* ---------------------------------------------------------------- the calculator, on a computer */
static uint16_t fb[240][320];
static void (*on_frame)(void);
void plat_push(int x, int y, int w, int h, const uint16_t *px) {
  for (int j = 0; j < h; j++)
    for (int i = 0; i < w; i++) fb[y + j][x + i] = px[j * w + i];
  if (on_frame && y + h == 240) on_frame();
}
void *np_alloc(uint32_t n) { (void)n; return 0; }
static double clock_ms;
static uint32_t (*script)(uint32_t frame);
static uint32_t frame_no, quit_at;
static void (*watch)(void);
uint32_t plat_keys(void) {
  clock_ms += 10;
  frame_no++;
  if (watch) watch();
  if (frame_no >= quit_at) return K_HOME;
  return script ? script(frame_no) : 0;
}
uint32_t plat_millis(void) { return (uint32_t)clock_ms; }
void plat_sleep(uint32_t ms) { clock_ms += ms; }
uint32_t plat_random(void) { return 0x9E3779B9u ^ frame_no; }
void plat_begin(void) {}
int plat_end(void) { return 0; }

static struct { char name[32]; uint8_t data[4096]; uint32_t len; bool on; } files[4];
static bool storage_full;
bool plat_save(const char *name, const void *data, uint32_t len) {
  if (storage_full) return false;
  int k = -1;
  for (int i = 0; i < 4; i++)
    if (files[i].on && !strcmp(files[i].name, name)) k = i;
  for (int i = 0; i < 4 && k < 0; i++)
    if (!files[i].on) k = i;
  if (k < 0 || len > sizeof files[k].data) return false;
  strcpy(files[k].name, name), memcpy(files[k].data, data, len), files[k].len = len, files[k].on = true;
  return true;
}
const uint8_t *plat_load(const char *name, uint32_t *len) {
  for (int i = 0; i < 4; i++)
    if (files[i].on && !strcmp(files[i].name, name)) return *len = files[i].len, files[i].data;
  return 0;
}
static void forget(const char *name) {
  for (int i = 0; i < 4; i++)
    if (files[i].on && !strcmp(files[i].name, name)) files[i].on = false;
}
static uint8_t *file_data(const char *name, uint32_t *len) {
  for (int i = 0; i < 4; i++)
    if (files[i].on && !strcmp(files[i].name, name)) return *len = files[i].len, files[i].data;
  return 0;
}

static void shot(const char *dir, const char *name) {
  char path[512];
  snprintf(path, sizeof path, "%s/%s.ppm", dir, name);
  FILE *f = fopen(path, "wb");
  if (!f) exit(2);
  fprintf(f, "P6 320 240 255\n");
  for (int y = 0; y < 240; y++)
    for (int x = 0; x < 320; x++) {
      uint16_t c = fb[y][x];
      fputc((c >> 11) * 255 / 31, f), fputc((c >> 5 & 63) * 255 / 63, f), fputc((c & 31) * 255 / 31, f);
    }
  fclose(f);
}

static int fails;
#define CHECK(c, ...) do { if (!(c)) { fails++; printf("FAIL %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

/* ---------------------------------------------------------------- tables made to order */
#define R(c, v) ((c) * 13 + (v))
static uno_game T;

/* n players, the house rules, every card in the draw pile (in order, its top the last), nothing dealt; then
   top(card) turns a card, give(p, card) hands one over */
static void blank(uno_game *g, int n, int house) {
  uno_new_game(g, (uno_rules){(uint8_t)n, LEVEL_NORMAL, 0, (uint8_t)house}, 7);
  uno_fill_deck(g->deck);
  g->dn = UNO_DECK, g->pn = 0;
  memset(g->count, 0, sizeof g->count);
  memset(g->hand, 0, sizeof g->hand);
  g->turn = 0, g->dealer = (uint8_t)(n - 1), g->dir = 1, g->phase = P_PLAY, g->pending = 0, g->opening = 0;
  g->out = g->uno_open = g->uno_said = g->winner = g->offender = g->drawn = UNO_NONE;
}
static void pull(uno_game *g, int card) {
  for (int i = g->dn - 1; i >= 0; i--)
    if (g->deck[i] == card) {
      memmove(&g->deck[i], &g->deck[i + 1], (size_t)(g->dn - i - 1));
      g->dn--;
      return;
    }
  printf("no %d left in the deck\n", card);
  exit(1);
}
static void top(uno_game *g, int card) {
  pull(g, card);
  g->pile[g->pn++] = (uint8_t)card;
  g->color = (uint8_t)uno_color(card);
}
static void give(uno_game *g, int p, int card) {
  pull(g, card);
  g->hand[p][g->count[p]++] = (uint8_t)card;
}
/* the draw pile's next card */
static void next_draw(uno_game *g, int card) {
  pull(g, card);
  g->deck[g->dn++] = (uint8_t)card;
}
static int find(const uno_game *g, int p, int card) {
  for (int i = 0; i < g->count[p]; i++)
    if (g->hand[p][i] == card) return i;
  return -1;
}

/* ---------------------------------------------------------------- the rules */
static void deck_and_points(void) {
  uint8_t d[UNO_DECK];
  uno_fill_deck(d);
  int per[5] = {0}, zero = 0, skip = 0, rev = 0, d2 = 0, wild = 0, w4 = 0;
  for (int i = 0; i < UNO_DECK; i++) {
    int c = d[i], v = uno_value(c);
    per[uno_color(c)]++;
    zero += c < 52 && v == 0, skip += c < 52 && v == V_SKIP, rev += c < 52 && v == V_REVERSE, d2 += c < 52 && v == V_DRAW2;
    wild += c == C_WILD, w4 += c == C_WILD4;
  }
  CHECK(per[0] == 25 && per[1] == 25 && per[2] == 25 && per[3] == 25 && per[4] == 8, "25 cards a colour, 8 Wilds");
  CHECK(zero == 4 && skip == 8 && rev == 8 && d2 == 8 && wild == 4 && w4 == 4, "the action cards");
  int numbers = 0;
  for (int i = 0; i < UNO_DECK; i++) numbers += d[i] < 52 && uno_value(d[i]) < 10;
  CHECK(numbers == 76, "19 number cards a colour");
  CHECK(uno_points(R(RED, 0)) == 0 && uno_points(R(BLUE, 7)) == 7 && uno_points(R(GREEN, 9)) == 9, "numbers: face value");
  CHECK(uno_points(R(RED, V_SKIP)) == 20 && uno_points(R(RED, V_REVERSE)) == 20 && uno_points(R(RED, V_DRAW2)) == 20,
        "Skip, Reverse, Draw Two: 20");
  CHECK(uno_points(C_WILD) == 50 && uno_points(C_WILD4) == 50, "Wilds: 50");
}

static void dealing(void) {
  /* a new game: a dealer, seven cards each, a first card that isn't a Wild Draw Four */
  int dealers[4] = {0};
  for (uint32_t s = 1; s <= 400; s++) {
    uno_new_game(&T, (uno_rules){4, LEVEL_NORMAL, 0, 0}, s);
    CHECK(uno_valid(&T), "a valid deal (seed %u)", s);
    CHECK(T.pn == 1 && T.pile[0] != C_WILD4, "no Wild Draw Four first (seed %u)", s);
    int dealt = 0;
    for (int p = 0; p < 4; p++) dealt += T.count[p];
    CHECK(dealt >= 28 && dealt <= 30 && T.dn == UNO_DECK - dealt - 1, "seven cards each (seed %u)", s);
    dealers[T.dealer]++;
  }
  CHECK(dealers[0] > 60 && dealers[1] > 60 && dealers[2] > 60 && dealers[3] > 60, "anyone can deal: %d %d %d %d",
        dealers[0], dealers[1], dealers[2], dealers[3]);
  /* the first card: on a deck laid out so that it comes up after the deal */
  struct { int card, n, turn, dir, phase, victim_cards; } first[] = {
      {R(RED, 5), 4, 0, 1, P_PLAY, 7},             /* the dealer (3)'s left (0) starts */
      {R(RED, V_SKIP), 4, 1, 1, P_PLAY, 7},        /* the dealer's left is skipped */
      {R(RED, V_REVERSE), 4, 3, -1, P_PLAY, 7},    /* the dealer starts, to the right */
      {R(RED, V_REVERSE), 2, 1, 1, P_PLAY, 7},     /* two players: the dealer starts */
      {R(RED, V_DRAW2), 4, 1, 1, P_PLAY, 9},       /* the dealer's left draws 2 and is skipped */
      {C_WILD, 4, 0, 1, P_COLOR, 7},               /* the dealer's left calls the colour, then plays */
  };
  for (unsigned k = 0; k < sizeof first / sizeof first[0]; k++) {
    int n = first[k].n;
    blank(&T, n, 0);
    T.dealer = (uint8_t)(n - 1);
    pull(&T, first[k].card);
    memmove(&T.deck[UNO_DECK - 7 * n], &T.deck[UNO_DECK - 7 * n - 1], (size_t)(7 * n));
    T.deck[UNO_DECK - 7 * n - 1] = (uint8_t)first[k].card;
    T.dn = UNO_DECK;
    uno_deal(&T);
    CHECK(T.pile[0] == first[k].card && T.turn == first[k].turn && T.dir == first[k].dir && T.phase == first[k].phase,
          "first card %d: turn %d dir %d phase %d", first[k].card, T.turn, T.dir, T.phase);
    CHECK(T.count[0] == first[k].victim_cards, "first card %d: the dealer's left has %d cards", first[k].card, T.count[0]);
    CHECK(uno_valid(&T), "first card %d: valid", first[k].card);
  }
  /* a Wild first: the first player calls the colour, then plays */
  CHECK(uno_choose(&T, BLUE) && T.phase == P_PLAY && T.turn == 0 && T.color == BLUE, "the first Wild's colour");
  /* a Wild Draw Four turned first goes back in the deck and another is turned */
  blank(&T, 3, 0);
  T.dealer = 2;
  pull(&T, C_WILD4);
  memmove(&T.deck[UNO_DECK - 21], &T.deck[UNO_DECK - 22], 21);
  T.deck[UNO_DECK - 22] = C_WILD4;
  T.dn = UNO_DECK;
  uno_deal(&T);
  CHECK(T.pile[0] != C_WILD4 && T.pn == 1 && uno_valid(&T), "a first Wild Draw Four is turned back");
  /* the deal goes to the left */
  uno_new_game(&T, (uno_rules){3, LEVEL_NORMAL, 0, 0}, 99);
  int d0 = T.dealer;
  uno_next_hand(&T);
  CHECK(T.dealer == (d0 + 1) % 3 && T.hands == 2, "the deal passes to the left");
}

static void action_cards(void) {
  /* matching */
  blank(&T, 3, 0);
  top(&T, R(RED, 5));
  give(&T, 0, R(RED, 2)), give(&T, 0, R(BLUE, 5)), give(&T, 0, R(BLUE, 7)), give(&T, 0, C_WILD), give(&T, 0, C_WILD4);
  CHECK(uno_can_play(&T, 0) && uno_can_play(&T, 1) && !uno_can_play(&T, 2) && uno_can_play(&T, 3) && uno_can_play(&T, 4),
        "colour, number, Wilds");
  CHECK(!uno_play(&T, 2, false) && T.count[0] == 5, "a card that doesn't go");
  /* a number: the next player */
  CHECK(uno_play(&T, 0, false) && T.turn == 1 && T.color == RED && T.pile[T.pn - 1] == R(RED, 2), "a number");
  /* symbols match symbols */
  blank(&T, 3, 0);
  top(&T, R(RED, V_SKIP));
  give(&T, 0, R(GREEN, V_SKIP)), give(&T, 0, R(GREEN, 1));
  CHECK(uno_can_play(&T, 0) && !uno_can_play(&T, 1), "a Skip on a Skip");
  /* Skip */
  CHECK(uno_play(&T, 0, false) && T.turn == 2 && T.color == GREEN, "Skip: player 1 misses a turn");
  /* Reverse, three and four players */
  for (int n = 3; n <= 4; n++) {
    blank(&T, n, 0);
    top(&T, R(RED, 1));
    give(&T, 0, R(RED, V_REVERSE)), give(&T, 0, R(RED, 3));
    CHECK(uno_play(&T, 0, false) && T.dir == -1 && T.turn == n - 1, "Reverse (%d players): to the right", n);
  }
  /* Draw Two */
  blank(&T, 3, 0);
  top(&T, R(BLUE, 1));
  give(&T, 0, R(BLUE, V_DRAW2)), give(&T, 0, R(RED, 3));
  CHECK(uno_play(&T, 0, false) && T.count[1] == 2 && T.turn == 2 && T.phase == P_PLAY, "Draw Two: 2 cards, skipped");
  CHECK(T.ev_victim == 1 && T.ev_n == 2, "Draw Two: the message");
  /* Wild */
  blank(&T, 3, 0);
  top(&T, R(BLUE, 1));
  give(&T, 0, C_WILD), give(&T, 0, R(RED, 3));
  CHECK(uno_play(&T, 0, false) && T.phase == P_COLOR && T.turn == 0 && T.color == NO_COLOR, "Wild: call a colour");
  CHECK(uno_valid(&T), "valid while calling");
  CHECK(!uno_choose(&T, 4) && uno_choose(&T, GREEN) && T.color == GREEN && T.turn == 1 && T.phase == P_PLAY, "Wild: green");
  /* Wild Draw Four, taken */
  blank(&T, 3, 0);
  top(&T, R(BLUE, 1));
  give(&T, 0, C_WILD4), give(&T, 0, R(RED, 3));
  CHECK(uno_play(&T, 0, false) && T.phase == P_COLOR && uno_choose(&T, RED), "Wild Draw Four: a colour");
  CHECK(T.phase == P_PENALTY && T.turn == 1 && T.pending == 4 && T.challengeable && !T.guilty, "next player: 4 waiting");
  CHECK(uno_valid(&T), "valid with a penalty waiting");
  CHECK(!uno_draw(&T) && uno_take(&T) && T.count[1] == 4 && T.turn == 2 && T.color == RED, "taken: 4 cards, skipped");
}

static void challenge(void) {
  /* guilty: it had a card of the colour in play */
  blank(&T, 3, 0);
  top(&T, R(RED, 5));
  give(&T, 0, C_WILD4), give(&T, 0, R(RED, 8)), give(&T, 0, R(BLUE, 2));
  give(&T, 1, R(GREEN, 1));
  CHECK(!uno_legal_wild4(&T, 0), "a red card: not allowed");
  CHECK(uno_play(&T, 0, false) && uno_choose(&T, BLUE) && T.guilty && T.prev_color == RED, "a bluff");
  CHECK(uno_challenge(&T) && T.count[0] == 6 && T.count[1] == 1 && T.turn == 1 && T.phase == P_PLAY,
        "challenge won: the bluffer draws 4, the challenger plays");
  CHECK(T.color == BLUE && T.ev == E_CHALLENGE_WON, "the colour called stays");
  /* not guilty: other numbers and symbols, and Wilds, don't count */
  blank(&T, 3, 0);
  top(&T, R(RED, 5));
  give(&T, 0, C_WILD4), give(&T, 0, R(BLUE, 5)), give(&T, 0, C_WILD), give(&T, 0, R(GREEN, V_SKIP));
  give(&T, 1, R(GREEN, 1));
  CHECK(uno_legal_wild4(&T, 0), "no red card: allowed");
  CHECK(uno_play(&T, 0, false) && uno_choose(&T, BLUE) && !T.guilty, "honest");
  CHECK(uno_challenge(&T) && T.count[1] == 7 && T.turn == 2 && T.count[0] == 3, "challenge lost: 6 cards, skipped");
  /* only a waiting Wild Draw Four can be challenged */
  blank(&T, 3, 0);
  top(&T, R(RED, 5));
  give(&T, 0, R(RED, V_DRAW2)), give(&T, 0, R(RED, 1));
  uno_play(&T, 0, false);
  CHECK(!uno_challenge(&T), "no challenge on a Draw Two");
  /* two players: after a Wild Draw Four taken, the same player again */
  blank(&T, 2, 0);
  top(&T, R(RED, 5));
  give(&T, 0, C_WILD4), give(&T, 0, R(BLUE, 1));
  uno_play(&T, 0, false), uno_choose(&T, BLUE);
  CHECK(uno_take(&T) && T.turn == 0 && T.count[1] == 4, "two players: +4 taken, the same player plays");
}

static void two_players(void) {
  blank(&T, 2, 0);
  top(&T, R(RED, 5));
  give(&T, 0, R(RED, V_REVERSE)), give(&T, 0, R(RED, V_SKIP)), give(&T, 0, R(RED, V_DRAW2)), give(&T, 0, R(RED, 1));
  CHECK(uno_play(&T, 0, false) && T.turn == 0 && T.dir == 1, "Reverse is a Skip: play again");
  CHECK(uno_play(&T, 0, false) && T.turn == 0, "Skip: play again");
  CHECK(uno_play(&T, 0, false) && T.turn == 0 && T.count[1] == 2, "Draw Two: 2 cards, play again");
}

static void drawing(void) {
  /* a card that goes: it may be played, and only it */
  blank(&T, 3, 0);
  top(&T, R(RED, 5));
  give(&T, 0, R(BLUE, 1)), give(&T, 0, R(RED, 2));
  next_draw(&T, R(RED, 9));
  CHECK(uno_draw(&T) && T.phase == P_DRAWN && T.count[0] == 3 && T.drawn == 2 && T.turn == 0, "drew a card that goes");
  CHECK(!uno_can_play(&T, 1) && uno_can_play(&T, 2), "only the card drawn");
  CHECK(uno_play(&T, 2, false) && T.turn == 1 && T.pile[T.pn - 1] == R(RED, 9), "played at once");
  /* or kept */
  blank(&T, 3, 0);
  top(&T, R(RED, 5));
  give(&T, 0, R(BLUE, 1));
  next_draw(&T, R(RED, 9));
  CHECK(uno_draw(&T) && uno_keep(&T) && T.turn == 1 && T.count[0] == 2, "kept");
  /* one that doesn't go: the turn passes */
  blank(&T, 3, 0);
  top(&T, R(RED, 5));
  give(&T, 0, R(BLUE, 1));
  next_draw(&T, R(GREEN, 9));
  CHECK(uno_draw(&T) && T.turn == 1 && T.phase == P_PLAY && T.lacks[0] == 1 << RED, "no luck: next player");
  /* allowed with a card that goes */
  blank(&T, 3, 0);
  top(&T, R(RED, 5));
  give(&T, 0, R(RED, 1));
  CHECK(uno_draw(&T), "drawing instead of playing");
  /* the draw pile runs out: the discards but the top one are shuffled into it */
  blank(&T, 3, 0);
  top(&T, R(RED, 5));
  give(&T, 0, R(BLUE, 1));
  while (T.dn > 1) T.pile[T.pn++] = T.deck[--T.dn];
  int last = T.deck[0], topc = T.pile[T.pn - 1], before = T.pn;
  T.color = (uint8_t)(uno_color(topc) < 4 ? uno_color(topc) : RED);
  T.dn = 0, T.pile[T.pn++] = (uint8_t)last;
  T.color = (uint8_t)(uno_color(last) < 4 ? uno_color(last) : RED);
  CHECK(uno_valid(&T), "an empty draw pile: valid");
  uno_draw(&T);
  CHECK(T.pn == 1 && T.pile[0] == last && T.dn == before - 1 && uno_valid(&T), "a new draw pile, the top card stays");
}

static void uno_calls(void) {
  /* forgot: caught, 2 cards */
  blank(&T, 3, 0);
  top(&T, R(RED, 5));
  give(&T, 0, R(RED, 1)), give(&T, 0, R(BLUE, 2));
  CHECK(uno_play(&T, 0, false) && T.uno_open == 0, "one card left, no UNO");
  CHECK(!uno_catch(&T, 0), "you can't catch yourself");
  CHECK(uno_catch(&T, 2) && T.count[0] == 3 && T.uno_open == UNO_NONE && T.ev == E_CAUGHT, "caught: 2 cards");
  /* said in time, after the card: safe */
  blank(&T, 3, 0);
  top(&T, R(RED, 5));
  give(&T, 0, R(RED, 1)), give(&T, 0, R(BLUE, 2));
  uno_play(&T, 0, false);
  CHECK(uno_call(&T, 0) && T.uno_open == UNO_NONE && !uno_catch(&T, 1), "said before anyone caught it");
  /* said with the next-to-last card */
  blank(&T, 3, 0);
  top(&T, R(RED, 5));
  give(&T, 0, R(RED, 1)), give(&T, 0, R(BLUE, 2));
  CHECK(uno_call(&T, 0) && uno_play(&T, 0, false) && T.uno_open == UNO_NONE, "said first");
  blank(&T, 3, 0);
  top(&T, R(RED, 5));
  give(&T, 0, R(RED, 1)), give(&T, 0, R(BLUE, 2));
  CHECK(uno_play(&T, 0, true) && T.uno_open == UNO_NONE && T.ev_by == 0, "said with the card");
  /* too late once the next player has started */
  blank(&T, 3, 0);
  top(&T, R(RED, 5));
  give(&T, 0, R(RED, 1)), give(&T, 0, R(BLUE, 2));
  give(&T, 1, R(RED, 3)), give(&T, 1, R(RED, 4));
  uno_play(&T, 0, false);
  CHECK(uno_play(&T, 0, true) && !uno_catch(&T, 2) && T.count[0] == 1, "too late after the next card");
  blank(&T, 3, 0);
  top(&T, R(RED, 5));
  give(&T, 0, R(RED, 1)), give(&T, 0, R(BLUE, 2));
  uno_play(&T, 0, false);
  CHECK(uno_draw(&T) && !uno_catch(&T, 2), "too late after the next player draws");
  /* not with more cards */
  blank(&T, 3, 0);
  top(&T, R(RED, 5));
  give(&T, 0, R(RED, 1)), give(&T, 0, R(BLUE, 2)), give(&T, 0, R(BLUE, 3));
  CHECK(!uno_call(&T, 0), "UNO with three cards");
  /* the window stays open while a Wild's colour is called */
  blank(&T, 3, 0);
  top(&T, R(RED, 5));
  give(&T, 0, C_WILD), give(&T, 0, R(BLUE, 2));
  uno_play(&T, 0, false);
  CHECK(T.uno_open == 0 && uno_choose(&T, BLUE) && T.uno_open == 0 && uno_catch(&T, 1) && T.count[0] == 3,
        "caught after the colour");
  /* house rule: UNO said for everyone */
  blank(&T, 3, H_AUTO_UNO);
  top(&T, R(RED, 5));
  give(&T, 0, R(RED, 1)), give(&T, 0, R(BLUE, 2));
  CHECK(uno_play(&T, 0, false) && T.uno_open == UNO_NONE, "automatic UNO");
}

static void going_out(void) {
  /* a number: the hand is over, the others' cards scored */
  blank(&T, 3, 0);
  top(&T, R(RED, 5));
  give(&T, 0, R(RED, 1));
  give(&T, 1, R(BLUE, 7)), give(&T, 1, C_WILD), give(&T, 2, R(GREEN, V_SKIP)), give(&T, 2, R(GREEN, 3));
  CHECK(uno_play(&T, 0, false) && T.phase == P_HAND_OVER && T.winner == 0 && T.gained == 7 + 50 + 20 + 3 &&
            T.score[0] == 80, "out: 80 points");
  CHECK(uno_valid(&T), "valid after the hand");
  /* a Draw Two last: the next player draws and it counts */
  blank(&T, 3, 0);
  top(&T, R(RED, 5));
  give(&T, 0, R(RED, V_DRAW2));
  give(&T, 1, R(BLUE, 7)), give(&T, 2, R(GREEN, 3));
  next_draw(&T, R(YELLOW, 9)), next_draw(&T, R(YELLOW, 8));
  CHECK(uno_play(&T, 0, false) && T.phase == P_HAND_OVER && T.count[1] == 3 && T.gained == 7 + 8 + 9 + 3,
        "a last Draw Two counts: %d", T.gained);
  /* a Wild last: no colour to call */
  blank(&T, 3, 0);
  top(&T, R(RED, 5));
  give(&T, 0, C_WILD), give(&T, 1, R(BLUE, 7)), give(&T, 2, R(GREEN, 3));
  CHECK(uno_play(&T, 0, false) && T.phase == P_HAND_OVER && T.gained == 10, "out on a Wild");
  /* a Wild Draw Four last, taken: counts */
  blank(&T, 3, 0);
  top(&T, R(RED, 5));
  give(&T, 0, C_WILD4), give(&T, 1, R(BLUE, 7)), give(&T, 2, R(GREEN, 3));
  next_draw(&T, R(YELLOW, 1)), next_draw(&T, R(YELLOW, 2)), next_draw(&T, R(YELLOW, 3)), next_draw(&T, R(YELLOW, 4));
  CHECK(uno_play(&T, 0, false) && T.phase == P_COLOR && uno_choose(&T, GREEN) && T.phase == P_PENALTY, "a last +4");
  CHECK(uno_valid(&T), "valid with the last +4 waiting");
  CHECK(uno_take(&T) && T.phase == P_HAND_OVER && T.gained == 7 + 10 + 3, "a last +4 taken counts: %d", T.gained);
  /* a Wild Draw Four last is always allowed: challenged, the challenger draws 6 and it counts */
  blank(&T, 3, 0);
  top(&T, R(RED, 5));
  give(&T, 0, C_WILD4), give(&T, 1, R(BLUE, 7)), give(&T, 2, R(GREEN, 3));
  uno_play(&T, 0, false), uno_choose(&T, GREEN);
  CHECK(!T.guilty && uno_challenge(&T) && T.phase == P_HAND_OVER && T.count[1] == 7, "a last +4 challenged: 6 cards");
  /* to 500 */
  blank(&T, 2, 0);
  top(&T, R(RED, 5));
  give(&T, 0, R(RED, 1)), give(&T, 1, C_WILD);
  T.score[0] = 460, T.score[1] = 499;
  CHECK(uno_play(&T, 0, false) && T.phase == P_GAME_OVER && T.winner == 0 && T.score[0] == 510, "500: the game is won");
  blank(&T, 2, 0);
  top(&T, R(RED, 5));
  give(&T, 0, R(RED, 1)), give(&T, 1, R(BLUE, 2));
  T.score[0] = 400;
  CHECK(uno_play(&T, 0, false) && T.phase == P_HAND_OVER && T.score[0] == 402, "short of 500: another hand");
  /* a single hand */
  blank(&T, 2, 0);
  T.r.one_hand = 1;
  top(&T, R(RED, 5));
  give(&T, 0, R(RED, 1)), give(&T, 1, R(BLUE, 2));
  CHECK(uno_play(&T, 0, false) && T.phase == P_GAME_OVER && T.winner == 0, "one hand: over");
}

static void house_rules(void) {
  /* stacking Draw Twos */
  blank(&T, 3, H_STACK2);
  top(&T, R(RED, 5));
  give(&T, 0, R(RED, V_DRAW2)), give(&T, 0, R(RED, 1));
  give(&T, 1, R(BLUE, V_DRAW2)), give(&T, 1, R(BLUE, 1));
  give(&T, 2, R(GREEN, 1));
  CHECK(uno_play(&T, 0, false) && T.phase == P_PENALTY && T.turn == 1 && T.pending == 2 && !T.challengeable, "+2 waits");
  CHECK(uno_can_stack(&T) && !uno_can_play(&T, 1) && uno_play(&T, 0, false) && T.turn == 2 && T.pending == 4, "stacked");
  CHECK(!uno_can_stack(&T) && uno_take(&T) && T.count[2] == 5 && T.turn == 0, "4 taken");
  /* stacking Wild Draw Fours: a stacked one can't be challenged */
  blank(&T, 3, H_STACK4);
  top(&T, R(RED, 5));
  give(&T, 0, C_WILD4), give(&T, 0, R(GREEN, 1)), give(&T, 1, C_WILD4), give(&T, 1, R(BLUE, 1)), give(&T, 2, R(GREEN, 2));
  uno_play(&T, 0, false), uno_choose(&T, BLUE);
  CHECK(T.challengeable && uno_can_stack(&T) && uno_play(&T, 0, false) && uno_choose(&T, GREEN), "+4 on +4");
  CHECK(T.turn == 2 && T.pending == 8 && !T.challengeable && !uno_challenge(&T) && uno_take(&T) && T.count[2] == 9,
        "8 taken, no challenge");
  /* no stacking on a last card */
  blank(&T, 3, H_STACK2);
  top(&T, R(RED, 5));
  give(&T, 0, R(RED, V_DRAW2)), give(&T, 1, R(BLUE, V_DRAW2)), give(&T, 2, R(GREEN, 1));
  CHECK(uno_play(&T, 0, false) && T.phase == P_HAND_OVER && T.count[1] == 3, "a last Draw Two is taken at once");
  /* without stacking a Draw Two can't answer a waiting Wild Draw Four */
  blank(&T, 3, 0);
  top(&T, R(RED, 5));
  give(&T, 0, C_WILD4), give(&T, 0, R(GREEN, 1)), give(&T, 1, R(BLUE, V_DRAW2)), give(&T, 1, C_WILD4);
  uno_play(&T, 0, false), uno_choose(&T, BLUE);
  CHECK(!uno_can_stack(&T) && !uno_can_play(&T, 0) && !uno_can_play(&T, 1), "official: no stacking");
  /* draw until you can play */
  blank(&T, 3, H_DRAW_UNTIL);
  top(&T, R(RED, 5));
  give(&T, 0, R(BLUE, 1));
  next_draw(&T, R(RED, 9)), next_draw(&T, R(GREEN, 2)), next_draw(&T, R(GREEN, 3));
  CHECK(uno_draw(&T) && T.count[0] == 4 && T.phase == P_DRAWN && T.ev_n == 3, "drew 3 until one goes");
  /* a playable drawn card must be played */
  blank(&T, 3, H_MUST_PLAY);
  top(&T, R(RED, 5));
  give(&T, 0, R(BLUE, 1));
  next_draw(&T, R(RED, 9));
  CHECK(uno_draw(&T) && !uno_keep(&T) && uno_play(&T, 1, false), "must play it");
}

static void validity(void) {
  uno_new_game(&T, (uno_rules){4, LEVEL_HARD, 0, H_ALL}, 5);
  CHECK(uno_valid(&T), "a fresh game");
  uno_game b = T;
  b.hand[1][0] = b.hand[2][0];
  CHECK(!uno_valid(&b), "a card twice");
  b = T, b.count[0]++;
  CHECK(!uno_valid(&b), "a card too many");
  b = T, b.turn = 4;
  CHECK(!uno_valid(&b), "a turn past the table");
  b = T, b.phase = 9;
  CHECK(!uno_valid(&b), "a phase that isn't");
  b = T, b.color = NO_COLOR;
  CHECK(!uno_valid(&b), "no colour outside a Wild");
}

/* ---------------------------------------------------------------- the bots */
static uint32_t hash_game(const uno_game *g, uint32_t h) {
  return uno_sum(g, sizeof *g) ^ (h * 16777619u);
}

/* a whole game, bots in every seat (their levels), catches included; the hash of every step */
static int bot_game(uno_rules r, uint32_t seed, const int *levels, uint32_t *hash, int *hands_out) {
  uno_new_game(&T, r, seed);
  uint32_t h = 0;
  int steps = 0, gained = 0, hands = 1;
  while (T.phase != P_GAME_OVER && steps < 400000) {
    if (T.phase == P_HAND_OVER) {
      gained += T.gained;
      uno_next_hand(&T);
      hands++;
      CHECK(uno_valid(&T), "a new hand: valid");
      continue;
    }
    int p = T.turn;
    uno_move m = bot_move(&T, levels[p]);
    if (!bot_do(&T, m)) {
      CHECK(0, "seed %u step %d: the bot's move %d (%d) was refused (phase %d)", seed, steps, m.act, m.arg, T.phase);
      return -1;
    }
    if (T.uno_open != UNO_NONE)
      for (int b = 0; b < r.players; b++) {
        uint32_t ms;
        if (b != T.uno_open && bot_catches(&T, b, levels[b], &ms)) {
          uno_catch(&T, b);
          break;
        }
      }
    if (!uno_valid(&T)) {
      CHECK(0, "seed %u step %d: not valid after move %d", seed, steps, m.act);
      return -1;
    }
    h = hash_game(&T, h);
    steps++;
  }
  CHECK(T.phase == P_GAME_OVER, "seed %u: the game ends (%d steps)", seed, steps);
  gained += T.gained;
  int total = 0;
  for (int p = 0; p < r.players; p++) total += T.score[p];
  CHECK(total == gained, "seed %u: the scores add up (%d, %d)", seed, total, gained);
  CHECK(r.one_hand || T.score[T.winner] >= UNO_GOAL, "seed %u: 500 reached", seed);
  if (hash) *hash = h;
  if (hands_out) *hands_out = hands;
  return T.winner;
}

static void bots(void) {
  int games = 0, hands = 0;
  for (int n = 2; n <= 4; n++)
    for (int level = 0; level < 3; level++)
      for (int house = 0; house < 4; house++) {
        static const uint8_t mix[4] = {0, H_STACK2 | H_STACK4, H_DRAW_UNTIL | H_MUST_PLAY, H_ALL};
        int levels[4] = {level, level, level, level};
        for (uint32_t s = 1; s <= 4; s++) {
          int k = 0;
          bot_game((uno_rules){(uint8_t)n, (uint8_t)level, 0, mix[house]}, s * 7919 + (uint32_t)(n * 100 + level * 10 + house),
                   levels, 0, &k);
          games++, hands += k;
        }
      }
  printf("bots: %d games to 500 (%d hands), every step valid\n", games, hands);
  /* the levels: who wins, two players to 500 */
  struct { int a, b, min; } duel[] = {{LEVEL_HARD, LEVEL_EASY, 60}, {LEVEL_NORMAL, LEVEL_EASY, 55}, {LEVEL_HARD, LEVEL_NORMAL, 50}};
  for (int d = 0; d < 3; d++) {
    int wins = 0, n = 200;
    for (int s = 0; s < n; s++) {
      /* each seat in turn, so the dealer's luck evens out */
      int levels[4] = {duel[d].a, duel[d].b, 0, 0};
      if (s & 1) levels[0] = duel[d].b, levels[1] = duel[d].a;
      int w = bot_game((uno_rules){2, LEVEL_NORMAL, 0, 0}, 1000u + (uint32_t)s, levels, 0, 0);
      wins += w == (s & 1 ? 1 : 0);
    }
    printf("bots: %s beat %s in %d of %d games\n", (const char *[]){"Easy", "Normal", "Hard"}[duel[d].a],
           (const char *[]){"Easy", "Normal", "Hard"}[duel[d].b], wins, n);
    CHECK(wins * 100 >= duel[d].min * n, "level %d against %d: %d of %d", duel[d].a, duel[d].b, wins, n);
  }
  /* four at the table, every level, many hands: the same game twice, to the same hash on every computer */
  uint32_t h1, h2;
  int levels[4] = {LEVEL_HARD, LEVEL_EASY, LEVEL_NORMAL, LEVEL_HARD};
  bot_game((uno_rules){4, LEVEL_NORMAL, 0, 0}, 2026, levels, &h1, 0);
  bot_game((uno_rules){4, LEVEL_NORMAL, 0, 0}, 2026, levels, &h2, 0);
  printf("bots: a game's hash %08X\n", h1);
  CHECK(h1 == h2 && h1 == 0x6205C6AFu, "the same game twice, and as on every computer: %08X", h1);
}

/* ---------------------------------------------------------------- saves */
static void saves(void) {
  memset(files, 0, sizeof files);
  uno_progress a, b;
  uno_fresh(&a);
  CHECK(!save_load(&b) && b.setup.players == 4 && b.setup.level == LEVEL_NORMAL && !b.has_game, "no save: a fresh start");
  a.setup = (uno_rules){3, LEVEL_HARD, 1, H_STACK2 | H_AUTO_UNO};
  a.guide = 1, a.has_game = 1;
  uno_new_game(&a.g, a.setup, 31337);
  for (int i = 0; i < 40 && a.g.phase < P_HAND_OVER; i++) bot_do(&a.g, bot_move(&a.g, LEVEL_NORMAL));
  CHECK(save_write(&a), "written");
  uint32_t n;
  uint8_t *d = file_data(UNO_SAVE_NAME, &n);
  CHECK(d && n == sizeof(uno_file) && n == 728 && !memcmp(d, "UNO1", 4), "uno.sav: %u bytes", n);
  CHECK(save_load(&b) && !memcmp(&a, &b, sizeof a), "read back exactly");
  /* damaged: the copy in the script brings it back */
  d[100] ^= 0x55;
  CHECK(save_load(&b) && !memcmp(&a, &b, sizeof a), "a damaged save: back from the copy");
  d = file_data(UNO_SAVE_NAME, &n);
  CHECK(d && !memcmp(d + 4, (uint8_t[]){1, 0, 216, 2}, 4), "and written again");
  /* cut short */
  files[0].len = 300;
  CHECK(save_load(&b) && !memcmp(&a, &b, sizeof a), "a save cut short: back from the copy");
  /* both damaged: a clean fresh start */
  d = file_data(UNO_SAVE_NAME, &n);
  d[200] ^= 1;
  uint8_t *c = file_data(UNO_COPY_NAME, &n);
  c[n - 10] = c[n - 10] == 'A' ? 'B' : 'A';
  CHECK(!save_load(&b) && !b.has_game && b.setup.players == 4, "both damaged: a fresh start");
  /* the script gone too */
  forget(UNO_COPY_NAME);
  CHECK(!save_load(&b) && !b.has_game, "damaged, no copy: a fresh start");
  /* the right checksum over a wrong game: refused */
  save_write(&a);
  d = file_data(UNO_SAVE_NAME, &n);
  uno_file f;
  memcpy(&f, d, sizeof f);
  f.p.g.hand[0][0] = f.p.g.hand[1][0];
  f.sum = uno_sum(&f, offsetof(uno_file, sum));
  memcpy(d, &f, sizeof f);
  forget(UNO_COPY_NAME);
  CHECK(!save_load(&b) && !b.has_game, "a card twice, summed right: refused");
  /* another version: refused */
  save_write(&a);
  d = file_data(UNO_SAVE_NAME, &n);
  memcpy(&f, d, sizeof f);
  f.version = 2;
  f.sum = uno_sum(&f, offsetof(uno_file, sum));
  memcpy(d, &f, sizeof f);
  forget(UNO_COPY_NAME);
  CHECK(!save_load(&b), "another version: refused");
  /* where the emulator test looks: has_game, the players, the score */
  CHECK(offsetof(uno_file, p.has_game) == 13 && offsetof(uno_file, p.g.r.players) == 16 &&
            offsetof(uno_file, p.g.score) == 674 && offsetof(uno_file, p.g.hands) == 682,
        "offsets %zu %zu %zu %zu", offsetof(uno_file, p.has_game), offsetof(uno_file, p.g.r.players),
        offsetof(uno_file, p.g.score), offsetof(uno_file, p.g.hands));
}

/* ---------------------------------------------------------------- the whole app, with keys */
static uint32_t lcg;
static uint32_t rnd(void) { return lcg = lcg * 1103515245u + 12345u, lcg >> 8; }
static uint32_t held_keys, held_until;
static uint32_t random_keys(uint32_t f) {
  if (f >= held_until) {
    static const uint32_t pick[] = {K_OK, K_OK, K_OK, K_LEFT, K_RIGHT, K_RIGHT, K_UP, K_UP, K_DOWN, K_SHIFT, K_BACK, 0, 0, 0};
    held_keys = pick[rnd() % (sizeof pick / sizeof pick[0])];
    held_until = f + 2 + rnd() % 12;
  }
  return held_keys;
}
static int bad_frames;
static void check_state(void) {
  if (ui.has_game && !uno_valid(G)) bad_frames++;
  if (ui.screen > V_QUIT || (ui.screen == V_PLAY && !ui.has_game && G->phase != P_GAME_OVER)) bad_frames++;
}

static void fuzz(void) {
  memset(files, 0, sizeof files);
  int quits = 0, games = 0;
  for (int s = 0; s < 300; s++) {
    lcg = (uint32_t)s * 2654435761u + 1;
    if (s % 50 == 0) memset(files, 0, sizeof files);
    storage_full = s % 37 == 36;
    script = random_keys, watch = check_state;
    frame_no = 0, quit_at = 1500 + rnd() % 3000, held_until = 0;
    uno_app_main();
    quits++;
    storage_full = false;
    uno_progress p;
    if (save_load(&p)) games += p.has_game;
    CHECK(!bad_frames, "session %d: %d bad frames", s, bad_frames);
    bad_frames = 0;
  }
  script = 0, watch = 0;
  printf("keys: %d sessions of random keys, quit with Home each time (%d left a game going)\n", quits, games);
}

/* a game played through the app's own keys: the menu, how to play, a deal; your turns played by the Normal bot
   through the keys a player would press */
static bool game_done;
static uint32_t key_every = 2;               /* (frames from a press to the next) */
static uint32_t bot_keys(uint32_t f) {
  if (f % key_every || game_done) return 0;  /* keys up between presses */
  uno_game *g = G;
  if (ui.screen != V_PLAY) return K_OK;
  if (g->phase >= P_HAND_OVER) return ui.show_end ? K_OK : 0;
  if (g->turn != 0) return 0;
  if (g->uno_open == 0) return K_SHIFT;
  uno_move m = bot_move(g, LEVEL_NORMAL);
  switch (m.act) {
    case M_COLOR: return ui.sel == m.arg ? K_OK : K_RIGHT;
    case M_DRAW: return K_UP;
    case M_KEEP: return K_UP;
    case M_TAKE: return g->challengeable ? (ui.sel == PEN_TAKE ? K_OK : K_UP) : (uno_can_stack(g) ? K_UP : 0);
    case M_CHALLENGE: return ui.sel == PEN_CHALLENGE ? K_OK : K_DOWN;
    default:
      if (g->phase == P_PENALTY && g->challengeable) return ui.sel == PEN_STACK ? K_OK : K_DOWN;
      if (g->phase == P_DRAWN) return K_OK;
      if (m.uno && g->uno_said != 0) return K_SHIFT;
      return ui.pick == m.arg ? K_OK : K_RIGHT;
  }
}

static void until_won(void) {
  check_state();
  if (!game_done && ui.screen == V_PLAY && G->phase == P_GAME_OVER && ui.show_end) game_done = true, quit_at = frame_no + 2;
}
static void through_the_keys(void) {
  memset(files, 0, sizeof files);
  script = bot_keys, watch = until_won;
  frame_no = 0, quit_at = 2000000, game_done = false, clock_ms = 0;
  uno_app_main();
  uno_progress p;
  CHECK(save_load(&p) && p.guide, "how to play shown");
  CHECK(game_done && G->score[G->winner] >= UNO_GOAL, "a game to 500 through the keys");
  printf("keys: a game to 500 through the app's keys, %u hands in %.0f minutes, scores %u %u %u %u\n", G->hands,
         clock_ms / 60000, G->score[0], G->score[1], G->score[2], G->score[3]);
  CHECK(!bad_frames, "%d bad frames", bad_frames);
  script = 0, watch = 0;
}

/* every key line fits */
static void texts(void) {
  uno_ui v;
  memset(&v, 0, sizeof v);
  uno_new_game(&T, (uno_rules){4, LEVEL_NORMAL, 0, H_ALL}, 3);
  int widest = 0;
  for (int screen = 0; screen <= V_QUIT; screen++)
    for (int phase = 0; phase <= P_GAME_OVER; phase++)
      for (int sel = 0; sel < SETUP_ROWS; sel++)
        for (int k = 0; k < 8; k++) {
          v.screen = (uint8_t)screen, v.sel = (uint8_t)sel, v.first_run = k & 1, v.show_end = k >> 1 & 1;
          T.phase = (uint8_t)phase, T.turn = (uint8_t)(k >> 2 & 1), T.challengeable = k & 1, T.count[0] = (uint8_t)(1 + (k & 2));
          T.uno_open = k == 6 ? 1 : UNO_NONE, T.r.house = k & 4 ? H_MUST_PLAY : 0;
          int w = uno_keyline_width(uno_keys(&T, &v));
          if (w > widest) widest = w;
          CHECK(w <= 314, "key line too wide (%d): %s", w, uno_keys(&T, &v));
        }
  printf("texts: the widest key line is %d pixels\n", widest);
  for (int pg = 0; pg < UNO_HELP_PAGES; pg++)
    CHECK(uno_help_bottom(pg) <= 86 + 4 * 19, "how to play, page %d: more than four lines (%d)", pg + 1, uno_help_bottom(pg));
  /* the longest messages */
  static const char *const msgs[] = {"Bot 3 played Wild +4: Yellow, UNO!", "Bot 2 played Yellow +2: Bot 3 draws 2, UNO!",
                                     "Bot 1 challenged: Bot 3 bluffed, draws 4", "UNO is for your next-to-last card",
                                     "Bot 3 deals: Yellow +2, you draw 2", "Only the new card can be played"};
  for (unsigned i = 0; i < sizeof msgs / sizeof msgs[0]; i++)
    CHECK(uno_text_width(msgs[i]) <= 252, "message too wide: %s (%d)", msgs[i], uno_text_width(msgs[i]));
}

static void test(const char *dir) {
  (void)dir;
  deck_and_points();
  dealing();
  action_cards();
  challenge();
  two_players();
  drawing();
  uno_calls();
  going_out();
  house_rules();
  validity();
  printf("rules: checked\n");
  bots();
  saves();
  texts();
  through_the_keys();
  fuzz();
}

/* ---------------------------------------------------------------- pictures */
static void picture(const char *dir, const char *name) {
  uno_render(G, &ui);
  shot(dir, name);
}

/* the bots play on until it's your turn in phase `phase` (and `count` cards or fewer in your hand, when not 0) */
static bool until_mine(int phase, int most) {
  for (int i = 0; i < 20000; i++) {
    uno_game *g = G;
    if (g->phase >= P_HAND_OVER) uno_next_hand(g);
    if (g->turn == 0 && g->phase == phase && (!most || g->count[0] <= most)) return true;
    bot_do(g, bot_move(g, g->turn ? g->r.level : LEVEL_NORMAL));
    describe();
  }
  return false;
}

void uno_render_cards(void);
void uno_render_icon(uint16_t bg);
static void shots(const char *dir) {
  uno_render_cards();
  shot(dir, "cards");
  uno_render_icon(0);
  shot(dir, "icon_black");
  uno_render_icon(0xFFFF);
  shot(dir, "icon_white");
  memset(files, 0, sizeof files);
  memset(&ui, 0, sizeof ui);
  sv.guide = 1;
  ui.setup = (uno_rules){4, LEVEL_NORMAL, 0, 0};
  ui.has_game = 1;
  /* the table: your turn */
  uno_new_game(G, ui.setup, 12);
  for (int i = 0; i < 9; i++) bot_do(G, bot_move(G, LEVEL_NORMAL)), describe();
  until_mine(P_PLAY, 0);
  ui.screen = V_PLAY, ui.pick = 2;
  for (int i = 0; i < G->count[0]; i++)
    if (uno_can_play(G, i)) { ui.pick = (uint8_t)i; break; }
  picture(dir, "shot");
  /* a Wild: calling the colour */
  uno_new_game(G, (uno_rules){3, LEVEL_HARD, 0, 0}, 77);
  for (int k = 0; k < 400; k++) {
    until_mine(P_PLAY, 0);
    int w = find(G, 0, C_WILD);
    if (w >= 0 && uno_can_play(G, w)) { uno_play(G, w, false), describe(); break; }
    bot_do(G, bot_move(G, LEVEL_NORMAL)), describe();
  }
  ui.sel = (uint8_t)bot_best_color(G, 0, LEVEL_NORMAL), ui.pick = 0;
  picture(dir, "colour");
  /* a Wild Draw Four played on you */
  uno_new_game(G, (uno_rules){2, LEVEL_HARD, 0, 0}, 5);
  for (int k = 0; k < 100000 && !(G->turn == 0 && G->phase == P_PENALTY && G->challengeable); k++) {
    if (G->phase >= P_HAND_OVER) uno_next_hand(G);
    bot_do(G, bot_move(G, LEVEL_NORMAL)), describe();
  }
  ui.sel = 1;
  picture(dir, "challenge");
  /* the hand you were shown */
  int o = G->offender;
  memcpy(ui.reveal, G->hand[o], G->count[o]);
  ui.reveal_n = G->count[o], ui.reveal_who = (uint8_t)o, ui.reveal_color = G->prev_color, ui.reveal_guilty = G->guilty;
  uno_challenge(G), describe();
  ui.screen = V_REVEAL;
  picture(dir, "reveal");
  /* UNO! */
  uno_new_game(G, (uno_rules){4, LEVEL_NORMAL, 0, 0}, 41);
  ui.screen = V_PLAY;
  for (int k = 0; k < 400000; k++) {
    if (G->phase >= P_HAND_OVER) uno_next_hand(G);
    if (G->turn == 0 && G->phase == P_PLAY && G->count[0] == 2 && uno_can_play(G, 0)) break;
    bot_do(G, bot_move(G, LEVEL_NORMAL)), describe();
  }
  ui.pick = 0;
  picture(dir, "uno");
  /* the end of a hand */
  uno_new_game(G, (uno_rules){4, LEVEL_NORMAL, 0, 0}, 8);
  G->score[1] = 120, G->score[2] = 245, G->score[3] = 60;
  for (int k = 0; k < 100000 && G->phase < P_HAND_OVER; k++) bot_do(G, bot_move(G, LEVEL_HARD)), describe();
  ui.show_end = 1, ui.sel = 0;
  picture(dir, "scores");
  /* the end of a game */
  G->phase = P_GAME_OVER, G->winner = 0, G->score[0] = 512, G->gained = 96;
  picture(dir, "won");
  /* two players */
  uno_new_game(G, (uno_rules){2, LEVEL_EASY, 0, 0}, 3);
  until_mine(P_PLAY, 0);
  ui.show_end = 0, ui.pick = 1;
  picture(dir, "duel");
  /* the menus */
  ui.screen = V_PAUSE, ui.sel = 0;
  picture(dir, "pause");
  ui.screen = V_QUIT, ui.sel = 0;
  picture(dir, "quit");
  ui.screen = V_TITLE, ui.sel = 0, ui.has_game = 1;
  picture(dir, "title");
  ui.screen = V_SETUP, ui.sel = ROW_DEAL, ui.setup = (uno_rules){4, LEVEL_NORMAL, 0, 0};
  picture(dir, "setup");
  ui.sel = ROW_HOUSE, ui.setup.house = H_STACK2;
  picture(dir, "setup_house");
  for (int pg = 0; pg < UNO_HELP_PAGES; pg++) {
    char name[16];
    snprintf(name, sizeof name, "help%d", pg + 1);
    ui.screen = V_HELP, ui.page = (uint8_t)pg, ui.first_run = 0;
    picture(dir, name);
  }
}

/* a game, your cards played by a bot, for the GIF: every frame drawn at the table, for ms milliseconds */
static const char *film_dir;
static uint32_t film_ms, film_k;
static double film_t0 = -1;
static void film_frame(void) {
  if (ui.screen != V_PLAY) return;
  if (film_t0 < 0) film_t0 = clock_ms;
  char path[512];
  snprintf(path, sizeof path, "%s/%06u_%06u.raw", film_dir, film_k++, (unsigned)(clock_ms - film_t0));
  FILE *f = fopen(path, "wb");
  if (!f) exit(2);
  fwrite(fb, 2, 320 * 240, f);
  fclose(f);
  if (clock_ms - film_t0 >= film_ms) quit_at = frame_no + 1;
}

int main(int argc, char **argv) {
  if (argc > 2 && !strcmp(argv[1], "test")) {
    test(argv[2]);
    if (fails) printf("%d FAILED\n", fails);
    else printf("all passed\n");
    return fails ? 1 : 0;
  }
  if (argc > 2 && !strcmp(argv[1], "shots")) {
    shots(argv[2]);
    return 0;
  }
  if (argc > 4 && !strcmp(argv[1], "film")) {
    /* a table already dealt (how to play read), from seed argv[4] */
    memset(files, 0, sizeof files);
    uno_progress p;
    uno_fresh(&p);
    p.guide = 1, p.has_game = 1;
    uno_new_game(&p.g, p.setup, (uint32_t)atoi(argv[4]));
    save_write(&p);
    film_dir = argv[2], film_ms = (uint32_t)atoi(argv[3]);
    script = bot_keys, on_frame = film_frame, key_every = 24;
    frame_no = 0, quit_at = 10000000;
    uno_app_main();
    return 0;
  }
  printf("usage: play test DIR | shots DIR | film DIR MS SEED\n");
  return 2;
}
