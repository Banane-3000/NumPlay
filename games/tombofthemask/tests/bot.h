/* A player for the tests. A stage: breadth first over the places the player can stand (every dash, and waiting
 * a third of a second, tried on copies of the game with the real rules, hazards and lava included) until a way
 * reaches the exit; the game is deterministic, so that way, replayed, clears the stage. The arcade: the same
 * search a few moves deep, taking the way that climbs highest without dying. */
#ifndef TM_BOT_H
#define TM_BOT_H
#include <stdlib.h>
#include <string.h>
#include "game.h"

#define BOT_WAIT 21                      /* a wait: steps (6 of them: the rotors' 2.1 s) */

#define BOT_WAITS 12                     /* waits in a row at one place, at most */
#define BOT_PHASES 6                     /* the search tells apart arrivals at a place by the time, in waits */

typedef struct {
  tm_game g;
  int parent, move;                      /* move: 0..3 a dash, 4 a wait */
  int waits;                             /* waits in a row up to here */
} bot_node;

static bot_node *bot_q;
static int bot_cap;

/* one move from a standing player: a dash until it stands again (or dies, or wins), or a short wait */
static bool bot_move(tm_game *g, int m) {
  if (m == 4) {
    for (int i = 0; i < BOT_WAIT && g->alive && !g->ev_won; i++) tm_step(g, -1, false);
    return g->alive && (tm_standing(g) || g->ev_won);
  }
  int32_t x = g->x, y = g->y;
  int face = g->face;
  tm_step(g, m, false);
  if (tm_standing(g) && g->x == x && g->y == y && g->face == face) return false;   /* it could not go that way */
  for (int i = 0; i < 900 && g->alive && !g->ev_won && !tm_standing(g); i++) tm_step(g, -1, false);
  return g->alive && (tm_standing(g) || g->ev_won);
}

static void bot_room(int n) {
  if (bot_cap < n) bot_q = realloc(bot_q, sizeof(bot_node) * (size_t)n), bot_cap = n;
}

/* where and when a standing player is, and what moves round it (timer walls, platforms, rotors, things
 * awake), for the search's "seen" marks: a 64-bit hash */
static uint64_t bot_mix(uint64_t h, uint64_t v) { return (h ^ v) * 0x100000001B3ull; }
static uint64_t bot_key(const tm_game *g) {
  uint64_t h = 0xCBF29CE484222325ull;
  h = bot_mix(h, (uint64_t)(tm_cell(g->x) & 0xFFFF) << 24 | (uint64_t)(tm_cell(g->y) & 0xFFFF) << 4 | g->face);
  h = bot_mix(h, g->tick / BOT_WAIT % BOT_PHASES);
  for (int i = 0; i < TM_TIMER_WALLS; i++)
    if (g->tw[i].left > 0) h = bot_mix(h, (uint64_t)(g->tw[i].x * 977 + g->tw[i].y) << 8 | (unsigned)(g->tw[i].left / BOT_WAIT));
  for (int i = 0; i < TM_PLATFORMS; i++)
    if (g->plat[i].active) h = bot_mix(h, (uint64_t)(tm_cell(g->plat[i].x) * 977 + tm_cell(g->plat[i].y)) << 4 | (unsigned)(g->plat[i].dx + 2) << 2 | (unsigned)(g->plat[i].dy + 2));
  for (int i = 0; i < TM_ROTORS; i++)
    if (g->rotor[i].active) h = bot_mix(h, (uint64_t)(uint8_t)g->rotor[i].px[0] << 8 | (uint8_t)g->rotor[i].py[0]);
  for (int i = 0; i < TM_SNAKES; i++) h = bot_mix(h, g->snake[i].active);
  for (int i = 0; i < TM_BALLS; i++) h = bot_mix(h, g->ball[i].awake);
  h = bot_mix(h, g->ride);
  return h ? h : 1;
}

/* the seen marks: an open-addressed set of keys */
#define BOT_SEEN (1 << 20)
static uint64_t *bot_seen;
static void bot_seen_clear(void) {
  if (!bot_seen) bot_seen = malloc(sizeof(uint64_t) * BOT_SEEN);
  memset(bot_seen, 0, sizeof(uint64_t) * BOT_SEEN);
}
static bool bot_seen_add(uint64_t k) {   /* false: already there */
  for (uint32_t i = (uint32_t)(k >> 40) & (BOT_SEEN - 1);; i = (i + 1) & (BOT_SEEN - 1)) {
    if (bot_seen[i] == k) return false;
    if (!bot_seen[i]) return bot_seen[i] = k, true;
  }
}

/* the moves to the exit from g (written to moves, at most max), or -1 when none is found within nodes */
static int bot_route(const tm_game *g, int nodes, int *moves, int max) {
  bot_seen_clear();
  bot_room(nodes);
  int head = 0, tail = 0;
  bot_q[tail].g = *g, bot_q[tail].parent = -1, bot_q[tail].move = -1, bot_q[tail].waits = 0, tail++;
  bot_seen_add(bot_key(g));
  while (head < tail) {
    int at = head++;
    for (int m = 0; m < 5 && tail < nodes; m++) {
      if (m == 4 && bot_q[at].waits >= BOT_WAITS) continue;
      bot_node *c = &bot_q[tail];
      c->g = bot_q[at].g;
      if (!bot_move(&c->g, m)) continue;
      c->parent = at, c->move = m, c->waits = m == 4 ? bot_q[at].waits + 1 : 0;
      if (c->g.ev_won) {
        int n = 0;
        for (int k = tail; bot_q[k].parent >= 0; k = bot_q[k].parent) n++;
        if (n > max) return -1;
        int i = n;
        for (int k = tail; bot_q[k].parent >= 0; k = bot_q[k].parent) moves[--i] = bot_q[k].move;
        return n;
      }
      if (!bot_seen_add(bot_key(&c->g))) continue;
      tail++;
    }
  }
  return -1;
}

/* plays moves from a standing player (a dash runs until it stands again) */
static void bot_play(tm_game *g, const int *moves, int n) {
  for (int i = 0; i < n && g->alive && !g->ev_won; i++) bot_move(g, moves[i]);
}

/* a stage cleared by the bot: true when its way reaches the exit */
static bool bot_play_stage(tm_game *g, int stage, uint32_t seed, int nodes) {
  static int moves[512];
  tm_start(g, stage, seed);
  int n = bot_route(g, nodes, moves, 512);
  if (n < 0) return false;
  bot_play(g, moves, n);
  return g->ev_won;
}

/* the arcade: the next move of the way that ends highest a few moves on (or -1: every way dies) */
static int bot_arcade_move(const tm_game *g, int nodes) {
  bot_seen_clear();
  bot_room(nodes);
  int head = 0, tail = 0, best = -1;
  int32_t best_y = -0x7FFFFFFF;
  bot_q[tail].g = *g, bot_q[tail].parent = -1, bot_q[tail].move = -1, bot_q[tail].waits = 0, tail++;
  while (head < tail) {
    int at = head++;
    for (int m = 0; m < 5 && tail < nodes; m++) {
      if (m == 4 && bot_q[at].waits >= 2) continue;
      bot_node *c = &bot_q[tail];
      c->g = bot_q[at].g;
      if (!bot_move(&c->g, m)) continue;
      c->parent = at, c->move = m, c->waits = m == 4 ? bot_q[at].waits + 1 : 0;
      if (!bot_seen_add(bot_key(&c->g))) continue;
      /* the height, less a little for each move (the shortest way up), and the lava's distance */
      int32_t score = c->g.y - (c->g.y - c->g.lava_y < 6 * TT ? 8 * TT : 0);
      if (score > best_y) {
        best_y = score;
        int k = tail;
        while (bot_q[k].parent > 0) k = bot_q[k].parent;
        best = bot_q[k].move;
      }
      tail++;
    }
  }
  return best;
}
#endif
