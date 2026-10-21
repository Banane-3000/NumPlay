/* Ultimate Tic-Tac-Toe's rules and the computer's search, apart from the screen (tests/check.c
 * plays them on a computer).
 *
 * A position is each player's marks in each small board, 9 bits a board (bit 3 row + column),
 * the small boards each has won and the ones that are over, won or full. The cell you take sends
 * the other player to the small board in the same place; if that one is over, they play anywhere.
 * Three small boards in a row win; all nine over without that is a draw.
 *
 * The includer defines search_stop(), true when the search must end now (out of time, or Home),
 * and rnd(), random numbers. */
#include <stdbool.h>
#include <stdint.h>

typedef struct {
  uint16_t mark[2][9];
  uint16_t won[2], over;
  int8_t next;   /* the small board to play in, -1 for any */
  uint8_t turn;  /* 0 X, 1 O */
} pos_t;

static const uint16_t LINES[8] = {0007, 0070, 0700, 0111, 0222, 0444, 0421, 0124};
/* the value of a place, the middle above the corners above the edges */
static const int8_t PLACE[9] = {3, 2, 3, 2, 4, 2, 3, 2, 3};

static bool three(unsigned m) {
  for (int i = 0; i < 8; i++)
    if ((m & LINES[i]) == LINES[i]) return true;
  return false;
}

/* the cells the player to move may take, as a mask per small board; how many */
static int legal(const pos_t *p, uint16_t *mv) {
  int n = 0;
  for (int b = 0; b < 9; b++) {
    mv[b] = (p->over >> b & 1) || (p->next >= 0 && p->next != b) ? 0 : ~(p->mark[0][b] | p->mark[1][b]) & 0777;
    n += __builtin_popcount(mv[b]);
  }
  return n;
}

static void play(pos_t *p, int m) {
  int t = p->turn, b = m / 9, c = m % 9;
  uint16_t *k = &p->mark[t][b];
  *k |= (uint16_t)(1 << c);
  if (three(*k)) p->won[t] |= (uint16_t)(1 << b);
  if (three(*k) || (p->mark[0][b] | p->mark[1][b]) == 0777) p->over |= (uint16_t)(1 << b);
  p->next = (int8_t)(p->over >> c & 1 ? -1 : c);
  p->turn ^= 1;
}

/* 0 the game goes on, 1 X has won, 2 O has, 3 a draw */
static int result(const pos_t *p) {
  return three(p->won[0]) ? 1 : three(p->won[1]) ? 2 : p->over == 0777 ? 3 : 0;
}

/* ------------------------------------------------------------ the computer */
/* X's lines still open on a board (x, o: the marks or boards won, dead: the places neither can use),
   the more of a line taken the better, less O's */
static int lines(unsigned x, unsigned o, unsigned dead, int k) {
  int s = 0;
  for (int i = 0; i < 8; i++) {
    unsigned L = LINES[i];
    int nx = __builtin_popcount(x & L), no = __builtin_popcount(o & L);
    if (!((o | dead) & L)) s += nx * nx * k;
    if (!((x | dead) & L)) s -= no * no * k;
  }
  return s;
}
/* How good the position looks for the player to move: small boards won, in good places, the
   lines still open on the big board and in each small one, and a free move. */
static int eval(const pos_t *p) {
  int s = lines(p->won[0], p->won[1], p->over & ~(p->won[0] | p->won[1]), 24);
  for (int b = 0; b < 9; b++) {
    int w = PLACE[b];
    if (p->won[0] >> b & 1) s += 30 * w;
    else if (p->won[1] >> b & 1) s -= 30 * w;
    else if (!(p->over >> b & 1)) s += lines(p->mark[0][b], p->mark[1][b], 0, w);
  }
  if (p->turn) s = -s;
  return p->next < 0 ? s + 20 : s;
}

#define WIN 100000
static bool stopped;
static int nodes, noise, best_move;

/* Negamax with alpha-beta: what the position is worth to the player to move, depth moves ahead.
   At the root (ply 0) the boards are tried from a random one, so that moves as good as each other
   take turns, and with noise every move is weighed exactly and given up to that much either way. */
static int search(const pos_t *p, int depth, int alpha, int beta, int ply) {
  int r = result(p);
  if (r) return r == 3 ? 0 : ply - WIN; /* only the player who just moved can have won */
  if (!depth) return eval(p);
  if (!(++nodes & 511) && search_stop()) stopped = true;
  if (stopped) return 0;
  uint16_t mv[9];
  legal(p, mv);
  for (int i = 0, b0 = ply ? 0 : (int)(rnd() % 9); i < 9; i++) {
    int b = (b0 + i) % 9;
    for (unsigned m = mv[b]; m; m &= m - 1) {
      pos_t q = *p;
      int k = b * 9 + __builtin_ctz(m);
      play(&q, k);
      int v = -search(&q, depth - 1, -beta, ply || !noise ? -alpha : 2 * WIN, ply + 1);
      if (!ply && noise) v += (int)(rnd() % (unsigned)(2 * noise + 1)) - noise;
      if (v > alpha) {
        alpha = v;
        if (!ply) best_move = k;
        if (alpha >= beta) return alpha;
      }
    }
  }
  return alpha;
}

/* The computer's move: searches 1, 2... up to `depth` moves ahead while search_stop() allows */
static int think(const pos_t *p, int depth, int n) {
  int best = 0;
  noise = n, stopped = false, nodes = 0;
  for (int d = 1; d <= depth; d++) {
    int v = search(p, d, -2 * WIN, 2 * WIN, 0);
    if (stopped && d > 1) break; /* (a depth not searched to the end: the one before stands) */
    best = best_move;
    if (v > WIN / 2 || v < -WIN / 2) break;
  }
  return best;
}
