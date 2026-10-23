/* UNO's rules: see game.h, and the instruction sheet's numbers in the spec kept with the reference. */
#include <string.h>
#include "game.h"

static uint32_t next_rand(uint32_t *s) {
  uint32_t x = *s;
  x ^= x << 13, x ^= x >> 17, x ^= x << 5;
  return *s = x ? x : 0x6C8E9CF5u;
}

int uno_points(int c) {
  if (c >= 52) return 50;
  int v = uno_value(c);
  return v >= 10 ? 20 : v;
}

int uno_next(const uno_game *g, int p) {
  int n = g->r.players;
  return (p + n + g->dir) % n;
}

static void event(uno_game *g, int e, int who, int card, int n) {
  g->ev = (uint8_t)e, g->ev_who = (uint8_t)who, g->ev_card = (uint8_t)card, g->ev_n = (uint8_t)n;
  g->ev_victim = g->ev_by = UNO_NONE;
}

void uno_fill_deck(uint8_t *d) {
  int n = 0;
  for (int c = 0; c < 52; c++) {
    d[n++] = (uint8_t)c;
    if (c % 13) d[n++] = (uint8_t)c;   /* one 0 of each colour, two of everything else */
  }
  for (int i = 0; i < 4; i++) d[n++] = C_WILD, d[n++] = C_WILD4;
}

static void shuffle(uno_game *g, uint8_t *a, int n) {
  for (int i = n - 1; i > 0; i--) {
    int j = (int)(next_rand(&g->rng) % (uint32_t)(i + 1));
    uint8_t t = a[i];
    a[i] = a[j], a[j] = t;
  }
}

/* one card from the draw pile into p's hand; an empty pile is made again from the discards under the top one */
static bool take_one(uno_game *g, int p) {
  if (!g->dn && g->pn > 1) {
    uint8_t top = g->pile[g->pn - 1];
    g->dn = (uint8_t)(g->pn - 1);
    memcpy(g->deck, g->pile, g->dn);
    g->pile[0] = top, g->pn = 1;
    shuffle(g, g->deck, g->dn);
  }
  if (!g->dn) return false;
  g->hand[p][g->count[p]++] = g->deck[--g->dn];
  return true;
}

static int take(uno_game *g, int p, int n) {
  int k = 0;
  while (k < n && take_one(g, p)) k++;
  if (n > 1) g->lacks[p] = 0;   /* new cards: what the table knew of that hand is gone */
  return k;
}

/* the player whose turn it is starts doing something: too late to catch whoever forgot UNO */
static void begin(uno_game *g) { g->uno_open = g->uno_said = UNO_NONE; }

static void hand_over(uno_game *g, int w) {
  int pts = 0;
  for (int p = 0; p < g->r.players; p++)
    if (p != w)
      for (int i = 0; i < g->count[p]; i++) pts += uno_points(g->hand[p][i]);
  g->winner = (uint8_t)w, g->gained = (uint16_t)pts;
  g->score[w] = (uint16_t)(g->score[w] + pts > 60000 ? 60000 : g->score[w] + pts);
  g->out = g->uno_open = g->uno_said = UNO_NONE;
  g->pending = 0;
  g->phase = g->r.one_hand || g->score[w] >= UNO_GOAL ? P_GAME_OVER : P_HAND_OVER;
}

void uno_deal(uno_game *g) {
  int n = g->r.players;
  memset(g->hand, 0, sizeof g->hand);
  memset(g->count, 0, sizeof g->count);
  memset(g->lacks, 0, sizeof g->lacks);
  g->pn = 0, g->dir = 1, g->pending = 0, g->challengeable = g->guilty = 0, g->opening = 0;
  g->offender = g->drawn = g->uno_open = g->uno_said = g->out = g->winner = UNO_NONE;
  g->gained = 0;
  /* seven cards each, one at a time, from the dealer's left */
  for (int k = 0; k < 7; k++)
    for (int i = 1; i <= n; i++) take_one(g, (g->dealer + i) % n);
  /* the first card: a Wild Draw Four goes back into the deck, which is shuffled, and another is turned */
  for (int tries = 0; g->deck[g->dn - 1] == C_WILD4; tries++) {
    if (tries < 32) {
      shuffle(g, g->deck, g->dn);
    } else {
      int i = g->dn - 1;
      while (g->deck[i] == C_WILD4) i--;
      g->deck[g->dn - 1] = g->deck[i], g->deck[i] = C_WILD4;
    }
  }
  int c = g->deck[--g->dn], left = uno_next(g, g->dealer), v = uno_value(c);
  g->pile[g->pn++] = (uint8_t)c;
  g->color = (uint8_t)uno_color(c), g->turn = (uint8_t)left, g->phase = P_PLAY;
  event(g, E_DEAL, g->dealer, c, 0);
  if (c == C_WILD) {
    g->phase = P_COLOR, g->opening = 1;       /* the player on the dealer's left calls the colour, then plays */
  } else if (v == V_SKIP) {
    g->turn = (uint8_t)uno_next(g, left);
  } else if (v == V_REVERSE) {
    if (n > 2) g->dir = -1;                    /* the dealer starts, and play goes to the right */
    g->turn = g->dealer;
  } else if (v == V_DRAW2) {
    g->ev_n = (uint8_t)take(g, left, 2), g->ev_victim = (uint8_t)left;
    g->turn = (uint8_t)uno_next(g, left);
  }
}

void uno_next_hand(uno_game *g) {
  if (g->hands) g->dealer = (uint8_t)((g->dealer + 1) % g->r.players);   /* the deal passes to the left */
  g->hands++;
  uno_fill_deck(g->deck);
  g->dn = UNO_DECK;
  shuffle(g, g->deck, UNO_DECK);
  uno_deal(g);
}

void uno_new_game(uno_game *g, uno_rules r, uint32_t seed) {
  memset(g, 0, sizeof *g);
  g->r.players = (uint8_t)(r.players < 2 ? 2 : r.players > 4 ? 4 : r.players);
  g->r.level = (uint8_t)(r.level > LEVEL_HARD ? LEVEL_NORMAL : r.level);
  g->r.one_hand = r.one_hand ? 1 : 0;
  g->r.house = (uint8_t)(r.house & H_ALL);
  g->rng = seed ? seed : 1;
  g->brng = seed * 2654435761u + 0x9E3779B9u;
  if (!g->brng) g->brng = 1;
  g->dir = 1;
  /* the dealer: everyone picks a card, the highest number deals (a symbol counts as zero), the tied pick again */
  int n = g->r.players, at = 0;
  uint8_t pool[UNO_DECK];
  uno_fill_deck(pool);
  shuffle(g, pool, UNO_DECK);
  unsigned in = (1u << n) - 1;
  while ((in & (in - 1)) && at + n <= UNO_DECK) {
    int best = -1;
    unsigned tied = 0;
    for (int p = 0; p < n; p++) {
      if (!(in >> p & 1)) continue;
      int v = uno_value(pool[at++]);
      if (v > 9) v = 0;
      if (v > best) best = v, tied = 1u << p;
      else if (v == best) tied |= 1u << p;
    }
    in = tied;
  }
  for (int p = n - 1; p >= 0; p--)
    if (in >> p & 1) g->dealer = (uint8_t)p;
  uno_next_hand(g);
}

bool uno_matches(const uno_game *g, int c) {
  int top = g->pile[g->pn - 1];
  return c >= 52 || uno_color(c) == g->color || (top < 52 && uno_value(c) == uno_value(top));
}

static bool stacks(const uno_game *g, int c) {
  if (g->out != UNO_NONE) return false;    /* no stacking on a last card: the hand is over */
  if (g->pending_kind == V_DRAW2) return (g->r.house & H_STACK2) && uno_value(c) == V_DRAW2;
  return (g->r.house & H_STACK4) && c == C_WILD4;
}

bool uno_can_play(const uno_game *g, int i) {
  if (g->phase == P_COLOR || g->phase >= P_HAND_OVER || i < 0 || i >= g->count[g->turn]) return false;
  int c = g->hand[g->turn][i];
  if (g->phase == P_PENALTY) return stacks(g, c);
  if (g->phase == P_DRAWN && i != g->drawn) return false;
  return uno_matches(g, c);
}

bool uno_can_stack(const uno_game *g) {
  if (g->phase != P_PENALTY) return false;
  for (int i = 0; i < g->count[g->turn]; i++)
    if (stacks(g, g->hand[g->turn][i])) return true;
  return false;
}

bool uno_legal_wild4(const uno_game *g, int p) {
  for (int i = 0; i < g->count[p]; i++)
    if (uno_color(g->hand[p][i]) == g->color) return false;
  return true;
}

/* a card's effect, once its colour is known (p played it) */
static void effect(uno_game *g, int p, int c) {
  int v = uno_value(c);
  g->phase = P_PLAY;
  if (c == C_WILD4) {
    g->pending = (uint8_t)(g->pending + 4), g->pending_kind = C_WILD4, g->offender = (uint8_t)p;
    g->turn = (uint8_t)uno_next(g, p), g->phase = P_PENALTY;
    return;
  }
  if (v == V_DRAW2) {
    int victim = uno_next(g, p);
    if ((g->r.house & H_STACK2) && g->out == UNO_NONE) {
      g->pending = (uint8_t)(g->pending + 2), g->pending_kind = V_DRAW2, g->offender = (uint8_t)p;
      g->challengeable = 0, g->turn = (uint8_t)victim, g->phase = P_PENALTY;
      return;
    }
    g->ev_n = (uint8_t)take(g, victim, g->pending + 2), g->ev_victim = (uint8_t)victim;
    g->pending = 0;
    g->turn = (uint8_t)uno_next(g, victim);
  } else if (v == V_SKIP) {
    g->turn = (uint8_t)uno_next(g, uno_next(g, p));
  } else if (v == V_REVERSE) {
    if (g->r.players == 2) {
      g->turn = (uint8_t)p;                    /* with two players, a Reverse is a Skip */
    } else {
      g->dir = (int8_t)-g->dir;
      g->turn = (uint8_t)uno_next(g, p);
    }
  } else {
    g->turn = (uint8_t)uno_next(g, p);
  }
  if (g->out != UNO_NONE) hand_over(g, g->out);
}

bool uno_play(uno_game *g, int i, bool say_uno) {
  if (!uno_can_play(g, i)) return false;
  int p = g->turn, c = g->hand[p][i];
  bool stacking = g->phase == P_PENALTY;
  bool said = say_uno || g->uno_said == p || (g->r.house & H_AUTO_UNO);
  if (c == C_WILD4) {
    /* a Wild Draw Four may be played with a card of the colour in play: that's a bluff, which only a challenge
       finds out */
    g->guilty = !uno_legal_wild4(g, p);
    g->prev_color = g->color;
    g->challengeable = !stacking;
  }
  begin(g);
  memmove(&g->hand[p][i], &g->hand[p][i + 1], (size_t)(g->count[p] - i - 1));
  g->count[p]--;
  g->pile[g->pn++] = (uint8_t)c;
  g->drawn = UNO_NONE;
  if (c < 52) g->lacks[p] &= (uint8_t)~(1u << uno_color(c));
  event(g, E_PLAY, p, c, 0);
  if (g->count[p] == 1) {
    if (said) g->ev_by = (uint8_t)p;           /* (with UNO!) */
    else g->uno_open = (uint8_t)p;
  }
  if (!g->count[p]) g->out = (uint8_t)p;
  if (c >= 52) {
    if (c == C_WILD && g->out == p) {          /* out on a Wild: no colour to call */
      hand_over(g, p);
      return true;
    }
    g->phase = P_COLOR, g->opening = 0, g->color = NO_COLOR;
    return true;
  }
  g->color = (uint8_t)uno_color(c);
  effect(g, p, c);
  return true;
}

bool uno_choose(uno_game *g, int color) {
  if (g->phase != P_COLOR || color < 0 || color > 3) return false;
  int p = g->turn, c = g->pile[g->pn - 1];
  g->color = (uint8_t)color;
  event(g, E_COLOR, p, c, color);
  if (g->opening) {
    g->opening = 0, g->phase = P_PLAY;
    return true;
  }
  effect(g, p, c);
  return true;
}

bool uno_draw(uno_game *g) {
  if (g->phase != P_PLAY) return false;
  int p = g->turn, k = 0;
  begin(g);
  g->lacks[p] |= (uint8_t)(1u << g->color);
  do {
    if (!take_one(g, p)) break;
    k++;
  } while ((g->r.house & H_DRAW_UNTIL) && !uno_matches(g, g->hand[p][g->count[p] - 1]));
  event(g, E_DRAW, p, 0, k);
  if (k && uno_matches(g, g->hand[p][g->count[p] - 1])) {
    g->phase = P_DRAWN, g->drawn = (uint8_t)(g->count[p] - 1);
    return true;
  }
  /* (nothing to draw: every other card is in someone's hand, so someone can play) */
  g->turn = (uint8_t)uno_next(g, p);
  return true;
}

bool uno_keep(uno_game *g) {
  if (g->phase != P_DRAWN || (g->r.house & H_MUST_PLAY)) return false;
  int p = g->turn;
  g->drawn = UNO_NONE, g->phase = P_PLAY, g->turn = (uint8_t)uno_next(g, p);
  event(g, E_KEEP, p, 0, 0);
  return true;
}

bool uno_take(uno_game *g) {
  if (g->phase != P_PENALTY) return false;
  int p = g->turn;
  begin(g);
  int k = take(g, p, g->pending);
  event(g, E_TAKE, p, g->pending_kind, k);
  g->pending = 0, g->challengeable = 0, g->phase = P_PLAY, g->turn = (uint8_t)uno_next(g, p);
  if (g->out != UNO_NONE) hand_over(g, g->out);
  return true;
}

bool uno_challenge(uno_game *g) {
  if (g->phase != P_PENALTY || !g->challengeable) return false;
  int p = g->turn, o = g->offender;
  begin(g);
  g->phase = P_PLAY, g->challengeable = 0;
  if (g->guilty) {
    /* the bluffer draws the 4 instead, and the challenger plays (never someone out: a last card is always allowed) */
    int k = take(g, o, g->pending);
    g->pending = 0;
    event(g, E_CHALLENGE_WON, p, C_WILD4, k);
    g->ev_victim = (uint8_t)o;
  } else {
    /* the challenger draws 4 and 2 more, and loses the turn */
    int k = take(g, p, g->pending + 2);
    g->pending = 0;
    event(g, E_CHALLENGE_LOST, p, C_WILD4, k);
    g->ev_victim = (uint8_t)p;
    g->turn = (uint8_t)uno_next(g, p);
    if (g->out != UNO_NONE) hand_over(g, g->out);
  }
  return true;
}

bool uno_call(uno_game *g, int p) {
  if (g->phase >= P_HAND_OVER || p < 0 || p >= g->r.players) return false;
  if (g->uno_open == p) {                      /* caught yourself before anyone else did: safe */
    g->uno_open = UNO_NONE;
    event(g, E_UNO, p, 0, 0);
    return true;
  }
  if (p == g->turn && g->count[p] == 2 && g->uno_said != p &&
      (g->phase == P_PLAY || g->phase == P_DRAWN || g->phase == P_PENALTY)) {
    g->uno_said = (uint8_t)p;                  /* said with the next-to-last card */
    event(g, E_UNO, p, 0, 0);
    return true;
  }
  return false;
}

bool uno_catch(uno_game *g, int by) {
  int o = g->uno_open;
  if (o == UNO_NONE || by == o || by < 0 || by >= g->r.players || g->phase >= P_HAND_OVER) return false;
  g->uno_open = UNO_NONE;
  int k = take(g, o, 2);
  event(g, E_CAUGHT, o, 0, k);
  g->ev_by = (uint8_t)by;
  return true;
}

bool uno_valid(const uno_game *g) {
  const uno_rules *r = &g->r;
  int n = r->players;
  if (n < 2 || n > 4 || r->level > LEVEL_HARD || r->one_hand > 1 || r->house > H_ALL) return false;
  if (!g->rng || !g->brng || g->phase > P_GAME_OVER || (g->dir != 1 && g->dir != -1)) return false;
  if (g->turn >= n || g->dealer >= n || !g->pn || g->pn > UNO_DECK || g->dn > UNO_DECK || g->opening > 1) return false;
  if (g->color > NO_COLOR || (g->color == NO_COLOR) != (g->phase == P_COLOR)) return false;
  if (g->challengeable > 1 || g->guilty > 1 || g->prev_color > 3 || g->pending > UNO_DECK) return false;
  if (g->phase != P_DRAWN && g->drawn != UNO_NONE) return false;
  const uint8_t who[] = {g->offender, g->uno_open, g->uno_said, g->out};
  for (unsigned i = 0; i < sizeof who; i++)
    if (who[i] != UNO_NONE && who[i] >= n) return false;
  if (g->winner != UNO_NONE && g->winner >= n) return false;
  for (int p = 0; p < 4; p++)
    if (g->score[p] > 60000 || (p >= n && (g->count[p] || g->score[p]))) return false;
  int top = g->pile[g->pn - 1];
  if (top >= UNO_KINDS) return false;
  switch (g->phase) {
    case P_PLAY: if (g->pending || g->out != UNO_NONE) return false; break;
    case P_DRAWN: if (g->pending || g->out != UNO_NONE || g->drawn >= g->count[g->turn]) return false; break;
    case P_COLOR: if (top < 52 || (g->opening && g->pn != 1)) return false; break;
    case P_PENALTY:
      if (!g->pending || g->pending & 1 || g->offender == UNO_NONE) return false;
      if (g->pending_kind != C_WILD4 && g->pending_kind != V_DRAW2) return false;
      if (g->challengeable && (g->pending_kind != C_WILD4 || g->pending != 4)) return false;
      break;
    case P_GAME_OVER: if (g->winner == UNO_NONE) return false; break;
    default: break;
  }
  if (g->out != UNO_NONE && (g->count[g->out] || (g->phase != P_COLOR && g->phase != P_PENALTY))) return false;
  if (g->uno_open != UNO_NONE && g->count[g->uno_open] != 1) return false;
  /* every card of the deck, once */
  uint8_t seen[UNO_KINDS] = {0};
  int total = g->dn + g->pn;
  for (int i = 0; i < g->dn; i++) {
    if (g->deck[i] >= UNO_KINDS) return false;
    seen[g->deck[i]]++;
  }
  for (int i = 0; i < g->pn; i++) {
    if (g->pile[i] >= UNO_KINDS) return false;
    seen[g->pile[i]]++;
  }
  for (int p = 0; p < 4; p++) {
    if (g->count[p] > UNO_DECK) return false;
    total += g->count[p];
    for (int i = 0; i < g->count[p]; i++) {
      if (g->hand[p][i] >= UNO_KINDS) return false;
      seen[g->hand[p][i]]++;
    }
  }
  if (total != UNO_DECK) return false;
  for (int c = 0; c < UNO_KINDS; c++)
    if (seen[c] != (c >= 52 ? 4 : c % 13 ? 2 : 1)) return false;
  return true;
}
