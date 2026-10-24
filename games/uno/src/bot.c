/* The bots. Each one sees what a player at the table sees: its own hand, the discard pile, how many cards the
 * others hold, who drew on which colour. Easy plays any card that goes and often forgets UNO; Normal keeps its
 * Wilds, plays its high cards and its biggest colour, and stops a player about to go out; Hard also plays to the
 * colours the next player lacks, gets rid of its points when someone is about to win, challenges by counting
 * the cards it has seen, and may bluff a Wild Draw Four to stop the last card. */
#include "game.h"

static uint32_t brand(uno_game *g) {
  uint32_t x = g->brng;
  x ^= x << 13, x ^= x >> 17, x ^= x << 5;
  return g->brng = x ? x : 0x2545F491u;
}
static bool chance(uno_game *g, int pct) { return (int)(brand(g) % 100) < pct; }

static int colour_count(const uno_game *g, int p, int col) {
  int n = 0;
  for (int i = 0; i < g->count[p]; i++) n += uno_color(g->hand[p][i]) == col;
  return n;
}

int bot_best_color(const uno_game *g, int p, int level) {
  int s[4] = {0, 0, 0, 0};
  for (int i = 0; i < g->count[p]; i++) {
    int c = g->hand[p][i];
    if (c < 52) s[uno_color(c)] += 10 + uno_points(c) / 4;   /* the colour it holds most, then the points */
  }
  if (level == LEVEL_HARD) {
    int nx = uno_next(g, p);
    for (int col = 0; col < 4; col++)
      if (g->lacks[nx] >> col & 1) s[col] += g->count[nx] <= 3 ? 14 : 4;
  }
  int best = 0;
  for (int col = 1; col < 4; col++)
    if (s[col] > s[best]) best = col;
  return best;
}

static bool says_uno(uno_game *g, int p, int level) {
  if (g->count[p] != 2) return false;
  return level == LEVEL_HARD ? true : !chance(g, level == LEVEL_EASY ? 25 : 8);
}

static bool attacks(const uno_game *g, int c) {
  int v = uno_value(c);
  return v == V_SKIP || v == V_DRAW2 || c == C_WILD4 || (v == V_REVERSE && g->r.players == 2);
}

/* Hard only: an illegal Wild Draw Four, when the next player is down to one card and nothing else stops them */
static bool bluff_ok(const uno_game *g, int p) {
  if (g->count[uno_next(g, p)] != 1) return false;
  for (int i = 0; i < g->count[p]; i++) {
    int c = g->hand[p][i];
    if (c != C_WILD4 && attacks(g, c) && uno_matches(g, c)) return false;
  }
  return true;
}

static int score(const uno_game *g, int p, int i, int level) {
  int c = g->hand[p][i], v = uno_value(c), col = uno_color(c), n = g->r.players, nx = uno_next(g, p);
  bool threat = g->count[nx] <= 2;
  int s;
  if (c >= 52) {
    s = g->count[p] <= 2 ? 60 : -40;           /* Wilds are kept for when nothing else goes */
    if (c == C_WILD4 && !threat) s -= 20;
  } else {
    s = uno_points(c) + 6 * (colour_count(g, p, col) - 1);   /* high cards first, staying in a colour it holds */
  }
  if (threat && attacks(g, c)) s += level == LEVEL_HARD ? 120 : 70;
  if (v == V_REVERSE && n > 2) {
    int back = (p + n - g->dir) % n;           /* who plays next after it */
    if (g->count[back] <= 2 && !threat) s -= 60;
    else if (threat && g->count[back] > 2) s += 50;
  }
  if (level == LEVEL_HARD) {
    if (col < 4 && (g->lacks[nx] >> col & 1)) s += 15;
    bool close = false;
    for (int q = 0; q < n; q++) close |= q != p && g->count[q] <= 2;
    if (close && !g->r.one_hand) s += uno_points(c) / 2;   /* about to lose the hand: shed the points */
  }
  return s;
}

/* how likely the Wild Draw Four's player held the colour before it, from the cards this player has seen, in
 * 65536ths: their other cards drawn at random from the ones it hasn't seen */
static uint32_t guilt(const uno_game *g, int p) {
  int col = g->prev_color, unseen = UNO_DECK - g->count[p] - g->pn, left = 25 - colour_count(g, p, col);
  for (int i = 0; i < g->pn; i++) left -= uno_color(g->pile[i]) == col;
  uint32_t none = 65536;
  for (int k = 0; k < g->count[g->offender] && none; k++) {
    int a = unseen - left - k, b = unseen - k;
    if (b <= 0) break;
    none = a <= 0 ? 0 : (uint32_t)((uint64_t)none * (uint32_t)a / (uint32_t)b);
  }
  return 65536 - none;
}

static bool challenges(uno_game *g, int level) {
  if (level == LEVEL_EASY) return chance(g, 20);
  /* honest players play it only without the colour: worth it on strong evidence (62.5%, Normal 80%) */
  uint32_t need = level == LEVEL_HARD ? 40960 : 52429;
  return guilt(g, g->turn) >= need;
}

uno_move bot_move(uno_game *g, int level) {
  uno_move m = {M_DRAW, 0, false};
  int p = g->turn;
  if (g->phase == P_COLOR) {
    m.act = M_COLOR;
    m.arg = (uint8_t)(level == LEVEL_EASY && chance(g, 40) ? (int)(brand(g) % 4) : bot_best_color(g, p, level));
    return m;
  }
  if (g->phase == P_DRAWN) {
    int c = g->hand[p][g->drawn];
    bool play = true;
    if (!(g->r.house & H_MUST_PLAY)) {
      if (c == C_WILD4 && !uno_legal_wild4(g, p) && !(level == LEVEL_HARD && bluff_ok(g, p))) play = false;
      else if (level == LEVEL_EASY && chance(g, 15)) play = false;
    }
    m.act = play ? M_PLAY : M_KEEP, m.arg = g->drawn, m.uno = play && says_uno(g, p, level);
    return m;
  }
  /* P_PLAY, or P_PENALTY with a card to stack */
  int best = -1, best_s = -1000000;
  for (int i = 0; i < g->count[p]; i++) {
    if (!uno_can_play(g, i)) continue;
    int c = g->hand[p][i];
    if (c == C_WILD4 && g->phase == P_PLAY && !uno_legal_wild4(g, p) && !(level == LEVEL_HARD && bluff_ok(g, p))) continue;
    int s = level == LEVEL_EASY ? (int)(brand(g) % 1000) : score(g, p, i, level);
    if (s > best_s) best_s = s, best = i;
  }
  if (g->phase == P_PENALTY) {
    if (best >= 0 && (level != LEVEL_EASY || chance(g, 70))) {
      m.act = M_PLAY, m.arg = (uint8_t)best, m.uno = says_uno(g, p, level);
    } else {
      m.act = g->challengeable && challenges(g, level) ? M_CHALLENGE : M_TAKE;
    }
    return m;
  }
  if (best >= 0) m.act = M_PLAY, m.arg = (uint8_t)best, m.uno = says_uno(g, p, level);
  return m;
}

bool bot_do(uno_game *g, uno_move m) {
  switch (m.act) {
    case M_PLAY: return uno_play(g, m.arg, m.uno);
    case M_DRAW: return uno_draw(g);
    case M_KEEP: return uno_keep(g);
    case M_TAKE: return uno_take(g);
    case M_CHALLENGE: return uno_challenge(g);
    default: return uno_choose(g, m.arg);
  }
}

bool bot_catches(uno_game *g, int by, int level, uint32_t *ms) {
  static const uint8_t pct[3] = {35, 70, 95};
  static const uint16_t base[3] = {1500, 1000, 600}, spread[3] = {700, 500, 400};
  if (g->uno_open == UNO_NONE || by == g->uno_open) return false;
  bool yes = chance(g, pct[level]);
  *ms = base[level] + brand(g) % spread[level];
  return yes;
}
