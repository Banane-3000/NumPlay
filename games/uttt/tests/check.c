/* Ultimate Tic-Tac-Toe's rules and computer, checked on a computer (make test):
 * - the rules on hand-made positions: where a move sends the other player, a small board won or full,
 *   the big board won, a draw;
 * - thousands of random games: every move legal, the game always ends, never more than 81 moves;
 * - the computer at every level only ever plays legal moves, takes a win in one, never hands one
 *   over, and the levels beat each other in order: hard over normal over easy over random. */
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

static long budget;
static long ticks;
static bool search_stop(void) { return ++ticks > budget; }
static uint32_t seed = 12345;
static uint32_t rnd(void) {
  seed ^= seed << 13;
  seed ^= seed >> 17;
  return seed ^= seed << 5;
}
#include "../src/rules.h"

static int fails;
#define CHECK(c, ...)                            \
  do {                                           \
    if (!(c)) {                                  \
      fails++;                                   \
      printf("FAIL %s:%d ", __FILE__, __LINE__); \
      printf(__VA_ARGS__);                       \
      printf("\n");                              \
    }                                            \
  } while (0)

static int random_move(const pos_t *p) {
  uint16_t mv[9];
  int n = legal(p, mv), k = (int)(rnd() % (unsigned)n);
  for (int b = 0; b < 9; b++)
    for (unsigned m = mv[b]; m; m &= m - 1)
      if (!k--) return b * 9 + __builtin_ctz(m);
  return -1;
}
static bool is_legal(const pos_t *p, int m) {
  uint16_t mv[9];
  legal(p, mv);
  return m >= 0 && m < 81 && (mv[m / 9] >> m % 9 & 1);
}
/* the computer's move at a level, as src/main.c sets them: 1 easy, 2 normal, 3 hard; the search
   may go on `calls` times 512 nodes */
static int cpu(const pos_t *p, int level, long calls) {
  static const int depth[4] = {0, 1, 3, 30}, noise[4] = {0, 90, 10, 0};
  ticks = 0, budget = calls;
  return think(p, depth[level], noise[level]);
}

static void rules(void) {
  pos_t p = {.next = -1};
  uint16_t mv[9];
  CHECK(legal(&p, mv) == 81, "81 moves to start");
  play(&p, 4 * 9 + 2); /* X in the middle board, top right cell: O goes to the top right board */
  CHECK(p.next == 2 && p.turn == 1, "sent to board 2");
  CHECK(legal(&p, mv) == 9 && mv[2] == 0777, "9 moves in board 2");
  /* O wins the top left board with its bottom row, X sending O back there each time */
  p = (pos_t){.next = -1};
  int seq[] = {0 * 9 + 0, 0 * 9 + 8, 8 * 9 + 0, 0 * 9 + 7, 7 * 9 + 0, 0 * 9 + 6};
  for (int i = 0; i < 6; i++) {
    CHECK(is_legal(&p, seq[i]), "move %d legal", i);
    play(&p, seq[i]);
  }
  CHECK(p.won[1] == 1 && p.won[0] == 0 && p.over == 1, "O has board 0: %o %o", p.won[0], p.won[1]);
  CHECK(p.next == 6 && !is_legal(&p, 0 * 9 + 1), "X sent to board 6, board 0 closed");
  p = (pos_t){.next = -1};
  p.mark[0][0] = 0003, p.turn = 0, p.next = 0;
  play(&p, 2);
  CHECK(p.won[0] == 1 && (p.over & 1), "X wins board 0");
  CHECK(p.next == 2, "sent to board 2: %d", p.next);
  /* sent to a board that is over: anywhere */
  p = (pos_t){.next = 3};
  p.over = 1, p.won[1] = 1, p.mark[1][0] = 0007;
  play(&p, 3 * 9 + 0);
  CHECK(p.next == -1, "board 0 over: a free move");
  CHECK(legal(&p, mv) == 81 - 9 - 1, "free move: every board but 0, %d", legal(&p, mv));
  /* a full board with no line is over but won by nobody */
  p = (pos_t){.next = 5, .turn = 1};
  p.mark[0][5] = 0b011100101, p.mark[1][5] = 0b100011000; /* O fills the last cell, 1: no line */
  int c = __builtin_ctz(~(p.mark[0][5] | p.mark[1][5]) & 0777);
  play(&p, 5 * 9 + c);
  CHECK((p.over >> 5 & 1) && !(p.won[0] >> 5 & 1) && !(p.won[1] >> 5 & 1), "board 5 full, nobody's: %o %o",
        p.mark[0][5], p.mark[1][5]);
  /* the big board: three in a row wins, a full one without is a draw */
  p = (pos_t){.next = -1};
  p.won[1] = 0124, p.over = 0124;
  CHECK(result(&p) == 2, "O wins on the diagonal");
  p.won[1] = 0, p.won[0] = 0, p.over = 0777;
  CHECK(result(&p) == 3, "draw");
}

static void random_games(void) {
  for (int g = 0; g < 20000; g++) {
    pos_t p = {.next = -1};
    int n = 0;
    while (!result(&p)) {
      int m = random_move(&p);
      CHECK(is_legal(&p, m), "random move legal");
      play(&p, m);
      CHECK(++n <= 81, "more than 81 moves");
      if (n > 81) break;
    }
    /* what result() says matches the marks */
    uint16_t won[2] = {0, 0};
    for (int b = 0; b < 9; b++)
      for (int t = 0; t < 2; t++)
        if (three(p.mark[t][b])) won[t] |= (uint16_t)(1 << b);
    CHECK((won[0] & won[1]) == 0, "a board won twice");
    CHECK(won[0] == p.won[0] && won[1] == p.won[1], "won boards kept right");
  }
}

/* a game between two levels (0: random), the hard one searching `calls` x 512 nodes a move; 1 if X
   wins, 2 if O does, 3 a draw */
static int match(int x, int o, long calls) {
  pos_t p = {.next = -1};
  while (!result(&p)) {
    int lv = p.turn ? o : x, m = lv ? cpu(&p, lv, lv == 3 ? calls : 1 << 30) : random_move(&p);
    CHECK(is_legal(&p, m), "level %d played an illegal move %d", lv, m);
    if (!is_legal(&p, m)) return 0;
    play(&p, m);
  }
  return result(&p);
}
/* the games a wins against b, playing X then O, `rounds` times */
static int duel(int a, int b, int rounds, long calls) {
  int w = 0;
  for (int i = 0; i < rounds; i++) w += (match(a, b, calls) == 1) + (match(b, a, calls) == 2);
  return w;
}

static void computer(void) {
  /* a win in one: X has two in the top row of board 0 and the boards 1 and 2 won */
  for (int lv = 1; lv <= 3; lv++) {
    pos_t p = {.next = 0};
    p.mark[0][0] = 0003, p.won[0] = 0006, p.over = 0006, p.mark[0][1] = 0007, p.mark[0][2] = 0007;
    p.mark[1][0] = 0300, p.mark[1][3] = 0001;
    int m = cpu(&p, lv, 100);
    CHECK(m == 2, "level %d misses the win: %d", lv, m);
  }
  /* no win in one handed over: O to play in board 0, where X threatens the top row and owns boards 1
     and 2. Sending X to a board that is over (1 or 2) or to board 0 would let X win. */
  for (int lv = 2; lv <= 3; lv++) {
    pos_t p = {.next = 0, .turn = 1};
    p.mark[0][0] = 0003, p.won[0] = 0006, p.over = 0006, p.mark[0][1] = 0007, p.mark[0][2] = 0007;
    p.mark[1][0] = 0100, p.mark[1][4] = 0001, p.mark[1][5] = 0020;
    int m = cpu(&p, lv, 100);
    CHECK(is_legal(&p, m), "level %d illegal: %d", lv, m);
    play(&p, m);
    uint16_t mv[9];
    legal(&p, mv);
    for (int b = 0; b < 9; b++)
      for (unsigned k = mv[b]; k; k &= k - 1) {
        pos_t q = p;
        play(&q, b * 9 + __builtin_ctz(k));
        CHECK(result(&q) != 1, "level %d plays %d and X wins with %d", lv, m, b * 9 + __builtin_ctz(k));
      }
  }
  /* each level beats the one below (hard with a small search, so that the check stays quick) */
  int er = duel(1, 0, 5, 0), nr = duel(2, 0, 5, 0), ne = duel(2, 1, 5, 0), hn = duel(3, 2, 3, 8);
  printf("out of 10: easy beat random %d, normal random %d, normal easy %d; out of 6: hard normal %d\n", er, nr, ne,
         hn);
  CHECK(er >= 8 && nr >= 9 && ne >= 8 && hn >= 5, "a level doesn't beat the one below");
  /* how fast: a hard move from the start, 200 x 512 nodes */
  clock_t t0 = clock();
  pos_t p = {.next = -1};
  cpu(&p, 3, 200);
  printf("%d nodes in %.2f s here\n", nodes, (double)(clock() - t0) / CLOCKS_PER_SEC);
}

int main(void) {
  rules();
  random_games();
  computer();
  printf(fails ? "%d FAILED\n" : "all checks passed\n", fails);
  return fails != 0;
}
