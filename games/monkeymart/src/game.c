/* The mart (game.h): GameState.lua and Player.lua of the web game, ported function by function. Names in the
 * comments are the game's. Lua's timers become ticks (60 a second); its math.random is our xorshift. */
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include "game.h"
#include "art.h"

#ifdef HOST
uint32_t mm_made[MM_FOODS + 1], mm_sold[MM_FOODS + 1];
#endif

#define NONE MM_NONE
#define TIME_COLLECT 9      /* 0.15 s */
#define TIME_BUYING 5       /* 0.07 s: the frame after 70 ms */
#define TIME_SHOW_CRAZY 70  /* s */

enum { TM_BUYER, TM_WAKE1, TM_WAKE2, TM_PACK_EXIT, TM_PACK_PAY, TM_THROW, TM_DROP, TM_ZSLEEP, TM_ZWAKE, TM_PAN,
       TM_PAN_END };

/* ------------------------------------------------------------------ random (math.random) */
uint32_t mm_rand(mm_game *g) {
  uint32_t x = g->rng;
  x ^= x << 13, x ^= x >> 17, x ^= x << 5;
  return g->rng = x ? x : 0x9E3779B9u;
}
static int rnd(mm_game *g, int n) { return n > 0 ? 1 + (int)(mm_rand(g) % (uint32_t)n) : 1; }   /* math.random(n) */
static int rnd2(mm_game *g, int a, int b) { return a + (int)(mm_rand(g) % (uint32_t)(b - a + 1)); }
static float frnd(mm_game *g) { return (float)(mm_rand(g) >> 8) * (1.0f / 16777216.0f); }   /* math.random() */

/* ------------------------------------------------------------------ small helpers */
static int32_t dist2(int32_t x, int32_t y, int32_t a, int32_t b) {   /* lume.distance(..., true) */
  int32_t dx = x - a, dy = y - b;
  return dx * dx + dy * dy;
}
static const mm_place *P(const mm_game *g, unsigned i) { return &g->m->places[i]; }
static bool active(const mm_game *g, unsigned i) { return i < g->m->count && g->box[i].active; }
static bool in_order(const mm_game *g, unsigned id) {
  for (unsigned i = 0; i < g->m->norder; i++)
    if (g->m->order[i] == id) return true;
  return false;
}

static void timer(mm_game *g, uint32_t ticks, uint8_t kind, uint8_t who, int16_t a, int32_t b, int32_t c) {
  if (g->ntimers >= MM_TIMERS) return;
  mm_timer *t = &g->timers[g->ntimers++];
  t->due = g->tick + (ticks ? ticks : 1), t->kind = kind, t->who = who, t->a = a, t->b = b, t->c = c;
}
static uint32_t secs(float s) { return (uint32_t)ceilf(s * MM_TPS - 0.001f); }

static void notice(mm_game *g, const char *s) {
  unsigned n = 0;
  for (; s[n] && n + 1 < sizeof g->notice; n++) g->notice[n] = s[n];
  g->notice[n] = 0;
  g->notice_t = 150;
}

static void effect(mm_game *g, uint8_t kind, int32_t x, int32_t y) {
  unsigned best = 0;
  for (unsigned i = 0; i < 8; i++)
    if (!g->fx_kind[i]) { best = i; break; } else if (g->fx_t[i] > g->fx_t[best]) best = i;
  g->fx_kind[best] = kind, g->fx_t[best] = 0, g->fx_x[best] = x, g->fx_y[best] = y;
}

/* food flying from one spot to another (showFoodAnim): pic 0 food, 1 cash */
static void fly(mm_game *g, uint16_t food, int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint8_t ticks) {
  mm_fly *f = &g->fly[g->fly_next];
  g->fly_next = (uint8_t)((g->fly_next + 1) % MM_FLY);
  f->food = food, f->t = 0, f->n = ticks, f->x0 = (int16_t)x0, f->y0 = (int16_t)y0, f->x1 = (int16_t)x1, f->y1 = (int16_t)y1;
}

/* ------------------------------------------------------------------ alerts (updateAlert) */
enum { AL_ZAVOD = 1, AL_WORKER = 16, AL_ROWDY = 64 };
static void alert(mm_game *g, bool add, uint8_t v) {
  if (add) {
    if (g->nalerts < MM_ALERTS) g->alerts[g->nalerts++] = v;
    return;
  }
  for (int i = g->nalerts - 1; i >= 0; i--)
    if (g->alerts[i] == v) {
      memmove(&g->alerts[i], &g->alerts[i + 1], (size_t)(g->nalerts - 1 - i));
      g->nalerts--;
      return;
    }
}

/* ------------------------------------------------------------------ the path grid (initPath, findPath) */
static bool walkable(const mm_game *g, int x, int y) {
  if (x < 0 || y < 0 || x >= g->m->w || y >= g->m->h) return false;
  unsigned i = (unsigned)(y * g->m->w + x);
  return g->grid[i >> 3] >> (i & 7) & 1;
}
static void close_cell(mm_game *g, unsigned i) { g->grid[i >> 3] &= (uint8_t)~(1u << (i & 7)); }

/* updateMap: a bought place closes its cells */
static void update_map(mm_game *g, unsigned place) {
  for (unsigned i = 0; i < g->m->nblock; i++)
    if (g->m->blocks[i][0] == place) close_cell(g, g->m->blocks[i][1]);
}

static void tile_from_pos(int32_t x, int32_t y, int *tx, int *ty) {   /* getTileFromPos */
  *tx = 17 + (int)floorf((float)x / 80.0f);
  *ty = 9 + (int)floorf((float)(y - 40) / 80.0f);
}
static void pos_from_tile(int tx, int ty, int32_t *x, int32_t *y) {   /* getPosFromTile */
  *x = -1300 + tx * 80;
  *y = -600 + ty * 80 - 40;
}

/* A* on the grid (the game's astar extension): eight directions, costs 1 and 1.41 (as 100 and 141), the octile
 * estimate; ties go to the smaller estimate, then the earlier. The path starts at the start cell. */
static uint16_t a_cost[700], a_from[700], a_heap[700];
static uint8_t a_state[700];
static uint32_t a_key[700];
static unsigned a_n;
static int a_h(int x, int y, int ex, int ey) {
  int dx = abs(x - ex), dy = abs(y - ey);
  return dx > dy ? 100 * (dx - dy) + 141 * dy : 100 * (dy - dx) + 141 * dx;
}
static bool a_less(unsigned a, unsigned b) { return a_key[a] < a_key[b] || (a_key[a] == a_key[b] && a < b); }
static void a_push(unsigned c) {
  unsigned i = a_n++;
  a_heap[i] = (uint16_t)c;
  while (i && a_less(a_heap[i], a_heap[(i - 1) / 2])) {
    uint16_t t = a_heap[i];
    a_heap[i] = a_heap[(i - 1) / 2], a_heap[(i - 1) / 2] = t, i = (i - 1) / 2;
  }
}
static unsigned a_pop(void) {
  unsigned top = a_heap[0], i = 0;
  a_heap[0] = a_heap[--a_n];
  for (;;) {
    unsigned l = 2 * i + 1, r = l + 1, s = i;
    if (l < a_n && a_less(a_heap[l], a_heap[s])) s = l;
    if (r < a_n && a_less(a_heap[r], a_heap[s])) s = r;
    if (s == i) break;
    uint16_t t = a_heap[i];
    a_heap[i] = a_heap[s], a_heap[s] = t, i = s;
  }
  return top;
}
static unsigned find_path(mm_game *g, int sx, int sy, int ex, int ey, uint8_t out[][2]) {
  const int W = g->m->w, H = g->m->h;
  if (sx < 0 || sy < 0 || sx >= W || sy >= H || !walkable(g, ex, ey)) return 0;   /* NO_SOLUTION */
  if (sx == ex && sy == ey) return 0;                                            /* START_END_SAME */
  memset(a_state, 0, (size_t)(W * H));
  a_n = 0;
  unsigned s = (unsigned)(sy * W + sx), e = (unsigned)(ey * W + ex);
  a_cost[s] = 0, a_from[s] = (uint16_t)s, a_state[s] = 1;
  a_key[s] = (uint32_t)a_h(sx, sy, ex, ey) << 12;
  a_push(s);
  static const int8_t dirs[8][3] = {{1, 0, 100}, {0, 1, 100}, {-1, 0, 100}, {0, -1, 100},
                                    {1, 1, 141}, {-1, 1, 141}, {-1, -1, 141}, {1, -1, 141}};
  while (a_n) {
    unsigned c = a_pop();
    if (a_state[c] == 2) continue;
    a_state[c] = 2;
    if (c == e) break;
    int cx = (int)c % W, cy = (int)c / W;
    for (int d = 0; d < 8; d++) {
      int nx = cx + dirs[d][0], ny = cy + dirs[d][1];
      if (!walkable(g, nx, ny)) continue;
      unsigned n = (unsigned)(ny * W + nx);
      unsigned cost = a_cost[c] + (unsigned)dirs[d][2];
      if (a_state[n] == 2 || (a_state[n] == 1 && cost >= a_cost[n])) continue;
      a_cost[n] = (uint16_t)cost, a_from[n] = (uint16_t)c, a_state[n] = 1;
      unsigned h = (unsigned)a_h(nx, ny, ex, ey);
      a_key[n] = ((cost + h) << 12) | (h & 0xFFF);
      a_push(n);
    }
  }
  if (a_state[e] != 2) return 0;
  unsigned len = 1;
  for (unsigned c = e; c != s; c = a_from[c]) len++;
  if (len > MM_PATH) return 0;
  unsigned i = len;
  for (unsigned c = e;; c = a_from[c]) {
    i--;
    out[i][0] = (uint8_t)(c % (unsigned)W), out[i][1] = (uint8_t)(c / (unsigned)W);
    if (c == s) break;
  }
  return len;
}

/* followPath */
static void follow_path(mm_actor *a, unsigned len) {
  a->input_allowed = 0, a->follow_path = 1;
  a->way = 1, a->nway = (uint8_t)(len - 1);
  pos_from_tile(a->path[1][0], a->path[1][1], &a->fx, &a->fy);
}
/* followTile */
static void follow_tile(mm_game *g, mm_actor *a, int tx, int ty) {
  int sx, sy;
  tile_from_pos(a->x, a->y, &sx, &sy);
  unsigned len = find_path(g, sx, sy, tx, ty, a->path);
  if (len) follow_path(a, len);
}
/* followPlace: to a named spot of the grid; a shelf's spot or the exit say what to do once there */
static void follow_place(mm_game *g, mm_actor *a, unsigned name) {
  const mm_tile *t = &g->m->tiles[name];
  if (name == T_HOOLIGAN) {
    int tx, ty;
    tile_from_pos(g->a[A_ROWDY].x, g->a[A_ROWDY].y - 40, &tx, &ty);
    follow_tile(g, a, tx, ty);
    a->sklad_place = 0, a->exit_place = 0;
    return;
  }
  if (t->tx == 255) return;
  follow_tile(g, a, t->tx, t->ty);
  a->sklad_place = (t->flags & TL_SKLAD) != 0;
  a->exit_place = (t->flags & TL_EXIT) != 0;
  if (t->place != NONE) a->num_place = t->place;
}
/* followPointPlace, followPointForce: straight to a point */
static void follow_point(mm_actor *a, int32_t x, int32_t y) {
  a->input_allowed = 0, a->follow_path = 1;
  a->fx = x, a->fy = y;
}

/* ------------------------------------------------------------------ places (getPlaceOffset, clearOffsetPlace) */
static void place_offset(mm_game *g, unsigned box, int32_t *x, int32_t *y, uint8_t *r) {
  const mm_place *p = P(g, box);
  mm_box *b = &g->box[box];
  unsigned k = 0;
  for (; k < p->nspot && k < 10; k++)
    if (!(b->busy >> k & 1)) break;
  if (k >= p->nspot || k >= 10) k = (unsigned)rnd2(g, 1, 6) - 1;
  if (k >= p->nspot) k = 0;
  const mm_pt *s = &g->m->spots[p->spot + k];
  *x = p->pos.x + s->x, *y = p->pos.y + s->y;
  b->busy |= (uint16_t)(1u << k);
  *r = (uint8_t)(k + 1);
}
static void clear_offset(mm_game *g, unsigned box, unsigned r) {
  if (box < g->m->count && r) g->box[box].busy &= (uint16_t)~(1u << (r - 1));
}

static int32_t exit_x(const mm_game *g, int r) { return g->m->exit.x + 30 * r; }   /* getExitPoint */
static int32_t exit_y(const mm_game *g, int r) { return g->m->exit.y - 5 * r; }

/* ------------------------------------------------------------------ the money */
static uint32_t *coins(mm_game *g) { return &g->p.coins; }
static void cash_add(mm_game *g, int32_t v) {   /* updateCashboxAmount (no doubling: that was an ad's reward) */
  g->cash += v;
  g->p.cash[g->mi] = (uint32_t)g->cash;
}

/* ------------------------------------------------------------------ upgrades (upgradeObject) */
static uint8_t *level(mm_game *g, unsigned row, unsigned k) {
  return row == 0 ? &g->p.level[0][0][k] : &g->p.level[g->mi][row][k];
}
unsigned mm_upgrade_level(const mm_game *g, unsigned row, unsigned k) {
  unsigned v = row == 0 ? g->p.level[0][0][k] : g->p.level[g->mi][row][k];
  return v ? v : 1;
}
static void apply_upgrade(mm_game *g, unsigned row, unsigned k) {
  const mm_uprow *u = &g->m->uprows[row];
  unsigned lvl = mm_upgrade_level(g, row, k);
  if (lvl < 2) return;
  if (u->kind == UP_PLAYER) {
    if (k == 1) g->a[A_ME].max_food = (uint8_t)(lvl + 2);          /* 4, 5, 6 */
  } else if (u->kind == UP_STAFF) {
    mm_actor *a = &g->a[A_CASHIER + u->target];
    if (u->target == 6) a = &g->a[A_MANAGER];
    if (k == 0) a->stamina_max = a->stamina = (uint8_t)(2 * lvl + 4);   /* 8, 10, 12 */
    if (k == 1) a->max_food = (uint8_t)(lvl + 2);                   /* 4, 5, 6 */
    if (k == 2) a->speed = a->old_speed = (int16_t)(36 * (lvl + 3));    /* 36 x 5, 6, 7 */
  } else {
    const uint8_t *z = g->m->upzavod[u->target];
    if (k == 0) g->box[z[0]].stamina_max = (uint8_t)(lvl == 2 ? 12 : lvl == 3 ? 20 : 28);
    if (k == 1 && lvl <= 3) {
      uint8_t cap = (uint8_t)(2 * lvl + 2);                          /* 6, 8 */
      g->box[z[1]].cap = cap, g->box[z[0]].cap = cap;
      if (z[2] != NONE) g->box[z[2]].cap = cap;
    }
    if (k == 2) g->box[z[0]].build = (uint8_t)(14 - 2 * lvl);         /* 10, 8, 6 */
  }
}

int mm_upgrade_price(const mm_game *g, unsigned row, unsigned k) {
  const mm_mart *m = row == 0 ? &mm_marts[0] : g->m;
  const mm_uprow *u = &m->uprows[row];
  unsigned lvl = mm_upgrade_level(g, row, k);
  if (lvl > u->max[k]) return -1;
  return u->price[k][lvl - 1];
}

/* checkAvailableUpgrade: the "!" on the upgrades key */
static void check_badge(mm_game *g) {
  bool ok = false;
  if (g->tutorial) return;
  unsigned lvl = mm_upgrade_level(g, 0, 1);
  if (lvl <= 3) {
    int pr = mm_marts[0].uprows[0].price[1][lvl - 1];
    if (pr == 0 || *coins(g) >= (uint32_t)pr) ok = true;
  }
  for (unsigned i = 1; i < g->m->nuprow && !ok; i++)
    for (unsigned k = 0; k < 3; k++) {
      const mm_uprow *u = &g->m->uprows[i];
      unsigned l = mm_upgrade_level(g, i, k);
      if (l <= u->max[k] && active(g, u->badge) && *coins(g) >= u->price[k][l - 1]) {
        ok = true;
        break;
      }
    }
  g->badge = ok;
}

/* ------------------------------------------------------------------ the tutorial (updateTutorial) */
static void tutorial(mm_game *g, unsigned step) {
  g->step = (uint8_t)step;
  if (step == 11) {
    g->tutorial = 0;
    notice(g, "The tutorial is completed. Have fun!");
  }
}

/* ------------------------------------------------------------------ actors: animation and clouds */
static void set_cloud(mm_actor *a, uint8_t c) { a->cloud = c; }

static void finish_drop(mm_game *g, unsigned id);
static void update_bot(mm_game *g, unsigned id);
static void follow_exit(mm_game *g, unsigned id);

static void update_wish(mm_game *g, unsigned id);
/* updateAction */
static void update_action(mm_game *g, unsigned id, uint8_t stage) {
  mm_actor *a = &g->a[id];
  a->stage = stage;
  switch (stage) {
    case S_IDLE: set_cloud(a, C_NONE); break;
    case S_PICKING: update_wish(g, id); break;
    case S_GO_PAY: set_cloud(a, C_PAY); break;
    case S_EXIT:
      set_cloud(a, C_HAPPY);
      if (a->num_offset > 0) clear_offset(g, a->num_place, a->num_offset);
      a->nfood = 0;
      a->in_queue = 0;
      follow_exit(g, id);
      /* shiftQueueCashbox */
      g->can_check_queue = 1;
      for (unsigned i = 0; i < g->nqueue; i++) {
        mm_actor *q = &g->a[g->queue[i]];
        if (q->stage == S_GO_PAY && q->in_queue) {
          clear_offset(g, q->num_place, q->num_offset);
          q->num_offset = 0, q->follow_path = 1, q->sklad_place = 1;
          int32_t x, y;
          uint8_t r;
          place_offset(g, q->num_place, &x, &y, &r);
          q->fx = x, q->fy = y, q->num_offset = r, q->sklad_place = 0;
        }
      }
      break;
    case S_SLEEP: set_cloud(a, C_SLEEP), a->anim = AM_SLEEP; break;
    case S_WAKEUP: a->anim = AM_WAKE, set_cloud(a, C_NONE); break;
    case S_CRAZY: set_cloud(a, C_CRAZY); break;
  }
}
static void follow_exit(mm_game *g, unsigned id) { follow_place(g, &g->a[id], T_EXIT); }

/* updateWishFood */
static void update_wish(mm_game *g, unsigned id) {
  mm_actor *a = &g->a[id];
  if (a->wtick == a->nwish) {
    update_action(g, id, S_GO_PAY);
    return;
  }
  a->wtype = a->wish_type[a->wtick], a->need = a->wish_count[a->wtick], a->have = 0;
  a->wtick++;
  set_cloud(a, C_WISH);
  a->wait_food = 0;
}

/* updatePicking */
static void finish_picking(mm_game *g, unsigned id);
static void update_picking(mm_game *g, unsigned id) {
  mm_actor *a = &g->a[id];
  a->have = 0;
  for (unsigned i = 0; i < a->nfood; i++)
    if (a->food[i] == a->wtype) a->have++;
  if (a->role == R_BUYER) {
    if (a->have >= a->need) update_wish(g, id);
  } else if (a->role == R_STOCKMAN) {
    if (a->have == a->need) finish_picking(g, id);
  }
}
static void finish_picking(mm_game *g, unsigned id) {
  mm_actor *a = &g->a[id];
  if (a->role == R_STOCKMAN && a->brain > 0) {
    a->allow_collect = 0, a->brain = B_GO_SKLAD;
    update_bot(g, id);
  }
}
static void finish_drop(mm_game *g, unsigned id) {
  mm_actor *a = &g->a[id];
  if (a->role == R_STOCKMAN && a->brain > 0) {
    a->allow_collect = 0, a->brain = B_NEXT_TASK;
    update_bot(g, id);
  }
}

static void cooldown(mm_actor *a, uint8_t t) { a->collecting = 1, a->cool = t; }

/* collectFood */
static void collect_food(mm_game *g, unsigned id, unsigned box) {
  mm_actor *a = &g->a[id];
  if (a->collecting) return;
  cooldown(a, TIME_COLLECT);
  mm_box *b = &g->box[box];
  const mm_place *p = P(g, box);
  b->count--;
  if (a->nfood < MM_CARRY) a->food[a->nfood++] = p->food;
  fly(g, p->food, p->pos.x, p->pos.y, a->x, a->y + 10 + a->nfood * 25, TIME_COLLECT);
  if (a->role == R_BUYER || a->role == R_STOCKMAN) update_picking(g, id);
  if (g->tutorial && p->food == 1 && g->step == 2) tutorial(g, 3);
}

/* dropFood: onto a shelf (box), or anywhere (to the trash, at the till): the last thing carried */
static void drop_food(mm_game *g, unsigned id, unsigned box, bool anywhere, int32_t ax, int32_t ay) {
  mm_actor *a = &g->a[id];
  if (a->collecting) return;
  if (anywhere) {
    unsigned i = a->nfood;
    fly(g, a->food[i - 1], a->x, a->y + 10 + (int32_t)i * 25, ax, ay + 45, TIME_COLLECT);
    a->nfood--;
    a->collecting = 1;
  } else {
    mm_box *b = &g->box[box];
    const mm_place *p = P(g, box);
    if (b->count < b->cap) {
      int idx = -1;
      for (int i = (int)a->nfood - 1; i >= 0; i--)
        if (a->food[i] == p->food) { idx = i; break; }
      if (idx >= 0) {
        memmove(&a->food[idx], &a->food[idx + 1], (size_t)(a->nfood - 1 - (unsigned)idx));
        a->nfood--;
        a->collecting = 1;
        fly(g, p->food, a->x, a->y + 10 + idx * 25, p->pos.x, p->pos.y, TIME_COLLECT);
        b->count++;
        if (g->tutorial && p->food == 1 && g->step == 3) tutorial(g, 4);
      }
    }
  }
  if (a->collecting) a->cool = TIME_COLLECT;
  if (a->role == R_STOCKMAN && a->nfood == 0) finish_drop(g, id);
}

/* clearFood: the trash, or the till */
static void clear_food(mm_game *g, unsigned id, int32_t x, int32_t y) {
  if (g->a[id].nfood > 0) drop_food(g, id, 0, true, x, y);
}

/* ------------------------------------------------------------------ buying places */
static void hooks(mm_game *g, unsigned place, bool loading);

/* placeSetForSale */
static void set_for_sale(mm_game *g, unsigned box) {
  if (g->box[box].active) return;
  g->box[box].selling = 1;
  if (g->tutorial) {
    if (box == 4) tutorial(g, 1);
    else if (box == 0) tutorial(g, 2);
    else if (box == 1) g->box[0].count = 2;
  }
}

/* checkNextPlaceSale */
static void next_sale(mm_game *g, int prev, bool loading) {
  if (prev >= 0) {
    bool main = in_order(g, (unsigned)prev);
    for (unsigned i = 0; i < g->m->nunlock; i++)
      if (g->m->unlocks[i][0] == prev) set_for_sale(g, g->m->unlocks[i][1]);
    if (!main || loading) return;
  }
  unsigned n = g->p.sales[g->mi];
  if (n < g->m->norder) {
    unsigned next = g->m->order[n];
    set_for_sale(g, next);
    g->sale_next = (uint8_t)next;
  } else {
    g->sale_next = NONE;
  }
}

/* followCameraPlace / moveCamera: after 0.6 s the camera glides to a pad for 1.5 s, the monkey waiting */
static void show_pad(mm_game *g, unsigned box) {
  if (box >= g->m->count) return;
  const mm_place *p = P(g, box);
  timer(g, secs(0.6f), TM_PAN, 0, 0, p->buy.x, p->buy.y);
}

/* placeSetBought */
static void set_bought(mm_game *g, unsigned box) {
  mm_box *b = &g->box[box];
  const mm_place *p = P(g, box);
  b->active = 1, b->selling = 0;
  if (p->flags & PL_SOLID) update_map(g, box);
  effect(g, FXK_EXIT, p->pos.x, p->pos.y);
  hooks(g, box, false);
  g->p.have[g->mi][box] = 1;
  if (in_order(g, box)) g->p.sales[g->mi]++;
  for (unsigned i = 0; i < g->m->ncamera; i++)
    if (g->m->camera[i] == box) {
      unsigned n = g->p.sales[g->mi];
      if (n < g->m->norder) show_pad(g, g->m->order[n]);
    }
  if (box == g->m->last) show_pad(g, g->m->truck);
}

/* placeCheckBuy: a tenth of what's left a step, while there's money */
static void check_buy(mm_game *g, unsigned id, unsigned box) {
  mm_actor *a = &g->a[id];
  if (a->collecting) return;
  cooldown(a, TIME_BUYING);
  if (*coins(g) == 0) return;
  mm_box *b = &g->box[box];
  const mm_place *p = P(g, box);
  uint32_t left = (uint32_t)(p->price - b->paid), take = (left + 9) / 10;   /* math.ceil(left * 0.1) */
  if (take > *coins(g)) take = *coins(g);
  *coins(g) -= take;
  b->paid = (uint16_t)(b->paid + take);
  g->p.paid[g->mi][box] = b->paid;
  fly(g, 0, a->x, a->y + 30, p->buy.x, p->buy.y, TIME_COLLECT);
  if (b->paid == p->price) {
    set_bought(g, box);
    next_sale(g, (int)box, false);
  }
}

/* ------------------------------------------------------------------ customers and the troublemaker */
/* getWishList */
static unsigned wish_list(mm_game *g, uint8_t type[6], uint8_t count[6]) {
  uint8_t types[MM_FOODS];
  unsigned nt = 0, max_rows = 1, max_food = 1;
  for (unsigned i = 0; i < g->m->nwish; i++)
    if (active(g, g->m->wish[i][0])) {
      types[nt++] = g->m->wish[i][1];
      max_rows = g->m->wish[i][2], max_food = g->m->wish[i][3];
    }
  if (g->m->wish_counts[0]) {       /* mart 2 */
    max_rows = nt;
    max_food = nt ? g->m->wish_counts[nt - 1] : 1;
    if (max_rows > 6) max_rows = 6;
  }
  if (!nt) return 0;
  unsigned rows = (unsigned)rnd(g, (int)max_rows);
  if (g->tutorial) types[0] = 1, nt = 1, max_food = 1, rows = 1;
  for (unsigned i = nt; i >= 1; i--) {       /* utils.shuffle */
    unsigned j = (unsigned)rnd(g, (int)i);
    uint8_t t = types[i - 1];
    types[i - 1] = types[j - 1], types[j - 1] = t;
  }
  if (rows > nt) rows = nt;                  /* (the game indexes past the list there; its order never lets it) */
  int left = rnd2(g, (int)rows, (int)max_food);
  unsigned n = 0;
  for (unsigned i = 0; i + 1 < rows; i++)
    if (left > 0) {
      unsigned t = types[i], most = mm_food_most[t];
      int c;
      if ((unsigned)left <= most) {
        c = rnd(g, left);
      } else {
        /* lume.weightedchoice: 1..5 weighted 2 3 3 2 1 for the raw foods, 1..3 weighted 2 3 1 for the rest */
        static const uint8_t raw[5] = {2, 3, 3, 2, 1}, made[3] = {2, 3, 1};
        bool r = t == 1 || t == 2 || t == 7 || t == 8 || t == 9;
        const uint8_t *w = r ? raw : made;
        unsigned nw = r ? 5 : 3, sum = r ? 11 : 6;
        float x = frnd(g) * (float)sum;
        c = (int)nw;
        for (unsigned k = 0; k < nw; k++) {
          if (x < w[k]) { c = (int)k + 1; break; }
          x -= w[k];
        }
      }
      type[n] = (uint8_t)t, count[n] = (uint8_t)c, n++;
      left -= c;
    }
  if (left > 0) {
    unsigned t = types[rows - 1], most = mm_food_most[t];
    if ((unsigned)left > most) left = (int)most;
    type[n] = (uint8_t)t, count[n] = (uint8_t)left, n++;
  }
  return n;
}

/* initAction */
static void init_action(mm_game *g, unsigned id) {
  mm_actor *a = &g->a[id];
  update_action(g, id, S_IDLE);
  a->input_allowed = 0, a->follow_path = 1;
  a->fx = g->m->door.x, a->fy = g->m->door.y;
}

static const uint8_t buyer_hats[10] = {0, 12, 13, 14, 15, 16, 17, 18, 19, 20};

/* addNewBuyer */
static void add_buyer(mm_game *g, unsigned id) {
  mm_actor *a = &g->a[id];
  a->ready_buy = 1;
  a->alive = 1;
  effect(g, FXK_EXIT, a->x, a->y + 40);
  init_action(g, id);
  /* initWishFood */
  a->nwish = (uint8_t)wish_list(g, a->wish_type, a->wish_count);
  unsigned total = 0;
  a->cost = 0;
  for (unsigned i = 0; i < a->nwish; i++) {
    total += a->wish_count[i];
    a->cost = (uint16_t)(a->cost + mm_food_price[a->wish_type[i]] * a->wish_count[i]);
  }
  a->wtick = 0, a->brain = 1, a->need_basket = 0;
  if (total > 4) a->need_basket = 1, a->brain = B_IDLE;
  a->basket = total > 4 ? 3 : 2;      /* updateSuffix: the basket's animations for more than four */
  a->num_offset = 0, a->num_place = NONE, a->has_basket = 0, a->wait_food = 0, a->in_queue = 0;
  a->skin = (uint8_t)(1 + rnd(g, 6));
  a->hat = buyer_hats[rnd(g, 10) - 1];
  a->nfood = 0;
}

/* addHooligan */
static void add_rowdy(mm_game *g) {
  mm_actor *a = &g->a[A_ROWDY];
  if (a->alive || !active(g, g->m->min)) return;
  g->allow_rowdy = 0;
  a->alive = 1;
  effect(g, FXK_EXIT, a->x, a->y + 40);
  init_action(g, A_ROWDY);
  a->brain = 0, a->speed = 144;       /* startHooligan */
  a->skin = (uint8_t)(1 + rnd(g, 6));
  a->hat = buyer_hats[rnd(g, 10) - 1];
}

/* getAvailableSklad: a shelf with more than one thing on it, at random */
static unsigned available_shelf(mm_game *g) {
  uint8_t list[8];
  unsigned n = 0, first = NONE;
  for (unsigned i = 0; i < g->m->count; i++) {
    const mm_place *p = P(g, i);
    if (!(p->flags & PL_BUYER)) continue;
    if (first == NONE) first = i;
    if (g->box[i].count > 1 && n < 8) list[n++] = (uint8_t)i;
  }
  if (!n) return first;
  for (unsigned i = n; i >= 1; i--) {
    unsigned j = (unsigned)rnd(g, (int)i);
    uint8_t t = list[i - 1];
    list[i - 1] = list[j - 1], list[j - 1] = t;
  }
  return list[0];
}
static unsigned shelf_tile(const mm_game *g, unsigned shelf) {   /* a shelf's name in the grid ('banan', ...) */
  for (unsigned t = 0; t < T_COUNT; t++)
    if (g->m->tiles[t].tx != 255 && g->m->tiles[t].place == shelf && (g->m->tiles[t].flags & TL_SKLAD)) return t;
  return T_EXIT;
}

/* throwFromPlace: a thing off the shelf, onto the floor in front of it */
static void throw_food(mm_game *g, unsigned box) {
  mm_box *b = &g->box[box];
  const mm_place *p = P(g, box);
  if (!b->count) return;
  b->count--;
  float rx = -10.0f + frnd(g) * 20.0f, ry = frnd(g) * 8.0f;   /* lume.random(-10, 10), lume.random(0, 8) */
  int32_t x = p->pos.x + (int32_t)(rx * 15.0f), y = p->pos.y - 160 - (int32_t)(ry * 10.0f);
  fly(g, p->food, p->pos.x, p->pos.y, x, y, 28);
  timer(g, secs(0.47f), TM_DROP, 0, p->food, x, y);
}

/* goHooliganAway */
static void rowdy_away(mm_game *g) {
  mm_actor *a = &g->a[A_ROWDY];
  if (a->stage == S_EXIT || !g->allow_rowdy) return;
  a->speed = 36 * 7;
  a->brain = 3;
  a->moving_on = 1, a->anim = AM_IDLE;
  a->stage = S_EXIT;
  follow_exit(g, A_ROWDY);
  set_cloud(a, C_AWAY);
  effect(g, FXK_BROOM, a->x + 5, a->y + 60);
  alert(g, false, (uint8_t)(AL_ROWDY + a->num_place));
}

/* ------------------------------------------------------------------ animals and machines */
static int zavod_of(const mm_game *g, unsigned box) {
  for (unsigned i = 0; i < g->m->nzavod; i++)
    if (g->m->zavods[i].zavod == box) return (int)i;
  return -1;
}
/* setZavodSleep */
static void zavod_sleep(mm_game *g, unsigned z, bool on) {
  const mm_zavod *zv = &g->m->zavods[z];
  if (on) {
    if (!g->hint) g->hint = 1, g->hint_x = zv->pos.x, g->hint_y = zv->pos.y;   /* (the game makes its arrow once) */
  } else {
    g->box[zv->zavod].stamina = g->box[zv->zavod].stamina_max;
    if (g->hint == 1) g->hint = 2;
  }
  g->zs[z].sleeping = on;
  alert(g, on, (uint8_t)(AL_ZAVOD + z));
}
/* zavodWakeUp */
static void zavod_wake(mm_game *g, unsigned z) {
  const mm_zavod *zv = &g->m->zavods[z];
  effect(g, zv->kind == ZV_MACHINE ? FXK_REPAIR : FXK_ALARM, zv->pos.x + 40, zv->pos.y + 50);
  g->zs[z].sleeping = 0;
  timer(g, secs(0.5f), TM_ZWAKE, (uint8_t)z, 0, 0, 0);
}
/* updateStamina */
static void zavod_tire(mm_game *g, unsigned box) {
  mm_box *b = &g->box[box];
  b->stamina--;
  int z = zavod_of(g, box);
  if (b->stamina == 0 && P(g, box)->feed != NONE && z >= 0) timer(g, secs(0.5f), TM_ZSLEEP, (uint8_t)z, 0, 0, 0);
}

/* updateZavod: an item made */
static void zavod_make(mm_game *g, unsigned box) {
  mm_box *b = &g->box[box];
  const mm_place *p = P(g, box);
  if (b->stamina == 0 || !b->active || b->count >= b->cap) return;
  if (p->feed != NONE) {
    mm_box *f1 = &g->box[p->feed], *f2 = p->feed2 != NONE ? &g->box[p->feed2] : 0;
    if (f1->count == 0 || (f2 && f2->count == 0)) return;
    f1->count--;
    if (f2) f2->count--;
    b->count++;
    zavod_tire(g, box);
  } else {
    b->count++;
  }
#ifdef HOST
  mm_made[p->food]++;
#endif
}

/* updateAllZavods: every half second */
static void all_zavods(mm_game *g) {
  for (unsigned i = 0; i < g->m->count; i++) {
    const mm_place *p = P(g, i);
    mm_box *b = &g->box[i];
    if (!(p->flags & PL_ZAVOD) || b->count >= b->cap || b->stamina == 0) continue;
    if (p->feed != NONE) {
      bool ok = g->box[p->feed].count > 0 && (p->feed2 == NONE || g->box[p->feed2].count > 0);
      if (ok) b->tick++;
    } else {
      b->tick++;
    }
    if (b->tick >= b->build) {
      zavod_make(g, i);
      b->tick = 0;
    } else {
      int z = zavod_of(g, i);      /* updateZavodAnim: an animal eats while its feeder has food */
      if (z >= 0 && p->feed != NONE && g->box[p->feed].count > 0) g->zs[z].eat = 40;
    }
  }
}

/* ------------------------------------------------------------------ the queue at the cash box */
/* updatePacking */
static void packing(mm_game *g, unsigned id) {
  mm_actor *a = &g->a[id];
  if (a->stage != S_GO_PAY) return;
  float t1 = 0.15f * (float)a->nfood + 1.0f, t2 = 0.15f * (float)a->nfood + 0.5f;
  a->stage = S_PACKING;      /* goPacking */
  timer(g, secs(t1), TM_PACK_EXIT, (uint8_t)id, 0, 0, 0);
  mm_actor *c = &g->a[A_CASHIER];     /* workCashier */
  if (c->alive && !c->follow_path && c->brain >= 2) c->moving_on = 0, c->anim_t = 0, c->anim = AM_DILER;
  timer(g, secs(t2), TM_PACK_PAY, (uint8_t)id, 0, a->cost, 0);
  if (g->tutorial && g->step == 4) tutorial(g, 5);
}
/* updateQueueCashbox: every second */
static void update_queue(mm_game *g) {
  if (!g->have_cashier && !g->near_cash) return;
  if (g->can_check_queue && g->nqueue) {
    unsigned id = g->queue[0];
    if (!g->a[id].follow_path) {
      memmove(g->queue, g->queue + 1, --g->nqueue);
      packing(g, id);
      g->can_check_queue = 0;
    }
  }
}

/* checkFollowingPlace */
static void check_following_place(mm_game *g, unsigned id) {
  mm_actor *a = &g->a[id];
  if (a->sklad_place) {
    if (a->num_place != NONE) {
      int32_t x, y;
      uint8_t r;
      if (a->role == R_ROWDY) {   /* getPlaceOffsetHooligan */
        static const uint8_t pick[4] = {2, 3, 5, 6};
        r = pick[rnd(g, 4) - 1];
        const mm_place *p = P(g, a->num_place);
        unsigned k = r - 1u < p->nspot ? r - 1u : 0;
        x = p->pos.x + g->m->spots[p->spot + k].x, y = p->pos.y + g->m->spots[p->spot + k].y - 15;
      } else {
        place_offset(g, a->num_place, &x, &y, &r);
      }
      a->fx = x, a->fy = y, a->num_offset = r;
    } else {
      a->stage = S_EXIT;
      follow_exit(g, id);
      set_cloud(a, C_NONE);
    }
    a->sklad_place = 0;
    if (a->num_place == g->m->cash && !a->in_queue) {
      if (g->nqueue < MM_BUYERS) g->queue[g->nqueue++] = (uint8_t)id;
      a->in_queue = 1;
    }
  } else if (a->exit_place) {
    a->exit_place = 0;
    int r = rnd2(g, 0, 8);
    a->fx = exit_x(g, r), a->fy = exit_y(g, r);
  }
}

/* goSleep */
static void go_sleep(mm_game *g, unsigned id) {
  mm_actor *a = &g->a[id];
  a->follow_path = 0, a->sleeping = 1, a->vx = a->vy = 0;
  update_action(g, id, S_SLEEP);
  a->moving_on = 0;
  alert(g, true, (uint8_t)(AL_WORKER + id));
  if (!g->hint) g->hint = 1, g->hint_x = a->x, g->hint_y = a->y;
}

/* finishFollowing */
static void finish_following(mm_game *g, unsigned id) {
  mm_actor *a = &g->a[id];
  a->follow_path = 0, a->input_allowed = 1, a->sklad_place = 0, a->exit_place = 0;
  if (a->stage == S_IDLE) {
    update_bot(g, id);
  } else if (a->stage == S_PICKING) {
    if (a->need_basket) a->need_basket = 0, a->has_basket = 1;   /* showBasket */
  } else if (a->stage == S_EXIT) {
    a->alive = 0;
    effect(g, FXK_EXIT, a->x, a->y + 40);
  } else if (a->stage == S_SLEEP) {
    go_sleep(g, id);
  }
  if (a->role == R_CASHIER && a->brain == 2) a->flip = 1;
}

/* updateFollowing: towards the point, along the path */
static void update_following(mm_game *g, unsigned id) {
  mm_actor *a = &g->a[id];
  int32_t dx = a->x - a->fx, dy = a->y - a->fy, d = dx * dx + dy * dy;
  if (d < 20000 && a->way == a->nway) check_following_place(g, id);
  if (d > 100) {
    a->vx = a->vy = 0;
    if (abs(dx) > 5) a->vx = dx > 0 ? -1.0f : 1.0f;
    if (abs(dy) > 5) a->vy = dy > 0 ? -1.0f : 1.0f;
  } else if (a->way == a->nway) {
    a->vx = a->vy = 0;
    finish_following(g, id);
  } else {
    a->way++;
    pos_from_tile(a->path[a->way][0], a->path[a->way][1], &a->fx, &a->fy);
  }
}

/* ------------------------------------------------------------------ the bots' minds (updateBot) */
/* getSkladInfo */
static unsigned shelf_need(mm_game *g, unsigned shelf, unsigned most) {
  mm_box *b = &g->box[shelf];
  unsigned need = b->cap > b->count ? (unsigned)(b->cap - b->count) : 0;
  if (need > most) need = most;
  for (unsigned i = 0; i < g->m->nempty; i++)     /* checkEmptySklad */
    if (g->m->empties[i][0] == shelf) {
      const uint8_t *e = g->m->empties[i];
      bool empty = g->box[e[1]].count == 0 && (g->box[e[2]].count == 0 || (e[3] != NONE && g->box[e[3]].count == 0));
      if (empty) need = 0;
    }
  return need;
}

static unsigned relax_tile(unsigned man) { return man == 1 ? T_RELAX_PLACE1 : man == 2 ? T_RELAX_PLACE2 : man == 3 ? T_RELAX_PLACE3 : man == 4 ? T_RELAX_PLACE4 : man == 5 ? T_RELAX_PLACE5 : T_RELAX_PLACE_M; }

static void bot_stockman(mm_game *g, unsigned id) {
  mm_actor *a = &g->a[id];
  const mm_staff *st = &g->m->staff[id - A_CASHIER];
  const mm_goal *gl = st->ngoal ? &g->m->goals[st->goal[a->gswift < st->ngoal ? a->gswift : 0]] : 0;
  switch (a->brain) {
    case B_IDLE: {
      a->allow_drop = 0, a->allow_collect = 0;
      if (!st->ngoal) return;
      gl = &g->m->goals[st->goal[a->gswift]];
      a->gidx = st->goal[a->gswift], a->have = 0, a->need = 0, a->gtick = 0, a->wtype = gl->food;
      if (active(g, gl->place)) {
        unsigned need = shelf_need(g, gl->place, a->max_food);
        if (need == 0) {
          bool done = true;
          for (unsigned i = 0; i < st->ngoal; i++)
            if (shelf_need(g, g->m->goals[st->goal[i]].place, a->max_food) > 0) done = false;
          if (done) {
            a->brain = B_GO_RELAX;
            follow_place(g, a, relax_tile(st->rest));
          }
        } else {
          a->brain = B_GO_ZAVOD;
          follow_place(g, a, gl->from);
          a->need = (uint8_t)need, a->gtick = 1;
        }
      }
      if (++a->gswift >= st->ngoal) a->gswift = 0;
      break;
    }
    case B_GO_ZAVOD: {
      const mm_goal *q = &g->m->goals[a->gidx];
      if (!q->nsrc) return;
      unsigned num = q->src[a->gtick - 1];
      a->allow_collect = 1;
      follow_point(a, P(g, num)->pos.x + q->offx, P(g, num)->pos.y - q->offy);
      unsigned t = a->gtick;
      bool next = t < 6 && q->nsrc >= t + 1 && active(g, q->src[t]);
      if (next) a->brain = B_NEXT_ZAVOD, a->gtick = (uint8_t)(t + 1);
      else a->brain = B_PICKING, a->tick_wait = 2;
      break;
    }
    case B_PICKING:
      a->tick_wait--;
      if (a->tick_wait < 0) {
        if (a->have > 0) finish_picking(g, id);
        else a->allow_collect = 0, a->brain = B_NOT_FOOD;
      }
      break;
    case B_GO_SKLAD:
      follow_place(g, a, g->m->goals[a->gidx].to);
      a->brain = B_DROPPING, a->tick_wait = 3;
      break;
    case B_DROPPING:
      a->allow_drop = 1;
      a->tick_wait--;
      if (a->tick_wait < 0) {
        follow_place(g, a, T_TRASH);
        a->brain = B_GO_TRASH;
      }
      break;
    case B_NEXT_TASK:
      a->allow_drop = 0;
      if (!a->boost) a->stamina--;
      if (a->stamina == 0 || a->stamina > 200) {
        a->stamina = 0;
        a->brain = B_GO_SLEEP, a->stage = S_SLEEP;
        follow_place(g, a, relax_tile(st->rest));
        return;
      }
      a->brain = B_IDLE;
      break;
    case B_GO_SLEEP: break;
    case B_GO_RELAX: a->allow_drop = 0, a->brain = B_IDLE; break;
    case B_NOT_FOOD: a->brain = B_IDLE; break;
    case B_NEXT_ZAVOD: a->brain = B_GO_ZAVOD; break;
    case B_GO_TRASH: follow_point(a, g->m->trash.x, g->m->trash.y - 30); break;
    case B_GO_START:
      a->allow_drop = 0, a->allow_collect = 0;
      follow_place(g, a, relax_tile(st->rest));
      a->brain = B_GO_RELAX;
      break;
  }
  (void)gl;
}

/* checkAlertPlaces: where the manager goes first */
static int alert_place(const mm_game *g) {
  if (!g->nalerts) return -1;
  uint8_t v = g->alerts[0];
  if (v >= AL_ROWDY) return T_HOOLIGAN;
  if (v >= AL_WORKER) return (int)relax_tile(g->m->staff[v - AL_WORKER - A_CASHIER].rest);
  return g->m->zavods[v - AL_ZAVOD].tile;
}

static void update_bot(mm_game *g, unsigned id) {
  mm_actor *a = &g->a[id];
  if (!a->alive || a->follow_path) return;
  switch (a->role) {
    case R_BUYER:
      if (a->brain == B_IDLE) {
        a->brain = 1;
        follow_place(g, a, T_BASKET);
      } else if (a->brain == 1) {
        if (a->stage == S_PICKING) {
          if (!a->wait_food) {
            if (a->num_offset > 0) clear_offset(g, a->num_place, a->num_offset);
            /* followPlaceByFood */
            for (unsigned t = 0; t < T_COUNT; t++) {
              const mm_tile *tl = &g->m->tiles[t];
              if (tl->tx != 255 && (tl->flags & TL_SKLAD) && tl->place != NONE && tl->place != g->m->cash &&
                  P(g, tl->place)->food == a->wtype && (P(g, tl->place)->flags & PL_BUYER)) {
                follow_place(g, a, t);
                break;
              }
            }
            a->wait_food = 1;
          }
        } else if (a->stage == S_GO_PAY) {
          if (a->num_offset > 0) clear_offset(g, a->num_place, a->num_offset);
          update_action(g, id, S_GO_PAY);
          follow_place(g, a, T_CASHBOX);
          a->brain = 2;
        }
      }
      if (a->stage == S_IDLE) {
        a->brain = 1;
        update_action(g, id, S_PICKING);
      }
      break;
    case R_ROWDY:
      if (a->brain == 0) {
        a->brain = 1;
        unsigned shelf = available_shelf(g);
        follow_place(g, a, shelf_tile(g, shelf));
        a->alert_sent = 0;
      } else if (a->brain == 1) {
        if (a->allow_drop) {
          a->brain = 2, a->tick_wait = 3;
          g->allow_rowdy = 1;
          update_action(g, id, S_CRAZY);
          a->moving_on = 0;
          a->anim = AM_CRAZY, a->crazy = (uint8_t)rnd(g, 2), a->anim_t = 0;
          if (!a->alert_sent) {
            a->alert_sent = 1;
            alert(g, true, (uint8_t)(AL_ROWDY + a->num_place));
          }
        }
      } else if (a->brain == 2) {
        if (a->allow_drop) {
          a->tick_wait--;
          if (a->tick_wait <= 0 && a->num_place != NONE && g->box[a->num_place].count > 0) {
            a->brain = 1;
            a->anim = AM_THROW, a->anim_t = 0;
            a->allow_drop = 0;
            timer(g, secs(1.0f), TM_THROW, (uint8_t)id, 0, 0, 0);
          }
        }
      }
      break;
    case R_CASHIER:
      if (a->brain == B_IDLE) {
        a->brain = 1;
        follow_place(g, a, T_CASHIER);
      } else if (a->brain == 1) {
        a->brain = 2;
        follow_point(a, P(g, g->m->cash)->pos.x - 85, P(g, g->m->cash)->pos.y - 4);
      }
      break;
    case R_STOCKMAN: bot_stockman(g, id); break;
    case R_MANAGER:
      if (a->brain == 0) {
        int t = alert_place(g);
        if (t >= 0) {
          follow_place(g, a, (unsigned)t);
          a->brain = t == T_HOOLIGAN ? 2 : 1;
        } else {
          follow_place(g, a, T_RELAX_PLACE_M);
          a->brain = 3;
        }
      } else if (a->brain == 1) {
        a->brain = 0;
      } else if (a->brain == 2) {
        if (g->a[A_ROWDY].stage == S_CRAZY) follow_point(a, g->a[A_ROWDY].x, g->a[A_ROWDY].y);
        a->brain = 1;
      } else if (a->brain == 3) {
        follow_point(a, g->m->manager.x, g->m->manager.y);
        a->brain = 0;
      }
      break;
  }
}

/* wakeUp */
static void wake_up(mm_game *g, unsigned id) {
  mm_actor *a = &g->a[id];
  a->sleeping = 0;
  effect(g, FXK_ALARM, a->x + 35, a->y + 100);
  timer(g, secs(0.5f), TM_WAKE1, (uint8_t)id, 0, 0, 0);
  timer(g, secs(1.5f), TM_WAKE2, (uint8_t)id, 0, 0, 0);
  alert(g, false, (uint8_t)(AL_WORKER + id));
  if (g->hint == 1) g->hint = 2;
}

/* ------------------------------------------------------------------ what's around an actor (updateLogicCollect, updateLogicBuy) */
static void logic_collect(mm_game *g, unsigned id) {
  mm_actor *a = &g->a[id];
  const mm_mart *m = g->m;
  unsigned count = a->nfood;
  if (a->role == R_WORKER || a->role == R_STOCKMAN) {
    uint64_t allow = a->role == R_STOCKMAN ? m->staff[id - A_CASHIER].allow : ~(uint64_t)0;
    for (unsigned i = 0; i < m->count; i++) {
      const mm_place *p = P(g, i);
      mm_box *b = &g->box[i];
      if (!b->active || i == m->cash || dist2(a->x, a->y, p->work.x, p->work.y) >= p->radius) continue;
      if (!(allow >> i & 1)) continue;
      if ((p->flags & PL_ZAVOD) && b->count > 0 && count < a->max_food) {
        if (a->allow_collect) collect_food(g, id, i);
      } else if ((p->flags & PL_SKLAD) && count > 0) {
        if (a->allow_drop) drop_food(g, id, i, false, 0, 0);
      }
    }
    if (dist2(a->x, a->y, m->trash.x, m->trash.y) < 2600) clear_food(g, id, m->trash.x, m->trash.y);
  } else if (a->role == R_BUYER && !a->follow_path) {
    if (a->stage == S_PICKING) {
      for (unsigned i = 0; i < m->count; i++) {
        const mm_place *p = P(g, i);
        if (!(p->flags & PL_BUYER)) continue;
        if (dist2(a->x, a->y, p->work.x, p->work.y) < p->radius && (p->flags & PL_SKLAD) && g->box[i].count > 0 &&
            p->food == a->wtype) {
          bool before = a->collecting;
          collect_food(g, id, i);
#ifdef HOST
          if (!before) mm_sold[p->food]++;
#else
          (void)before;
#endif
        }
      }
    } else if (a->stage == S_PACKING) {
      const mm_place *c = P(g, m->cash);
      clear_food(g, id, c->pos.x + 40, c->pos.y - 10);
    }
  }
}

static void logic_buy(mm_game *g, unsigned id) {
  mm_actor *a = &g->a[id];
  const mm_mart *m = g->m;
  if (a->role == R_WORKER || a->role == R_MANAGER) {
    for (unsigned z = 0; z < m->nzavod; z++)
      if (g->zs[z].sleeping && dist2(a->x, a->y, m->zavods[z].pos.x, m->zavods[z].pos.y) < m->zavods[z].radius)
        zavod_wake(g, z);
    if (g->allow_rowdy && dist2(a->x, a->y, g->a[A_ROWDY].x, g->a[A_ROWDY].y) < 5000) rowdy_away(g);
  }
  if (a->role != R_WORKER) return;
  if (a->vx == 0 && a->vy == 0) {
    for (unsigned i = 0; i < m->count; i++) {
      const mm_place *p = P(g, i);
      if (!g->box[i].active && g->box[i].selling) {
        if (dist2(a->x, a->y, p->buy.x, p->buy.y) < 2000) check_buy(g, id, i);
      }
    }
  }
  /* things the troublemaker threw */
  for (unsigned i = 0; i < g->ndropped; i++) {
    mm_dropped *d = &g->dropped[i];
    if (d->live && a->nfood < a->max_food && !a->collecting && dist2(a->x, a->y, d->x, d->y) < 2200) {
      cooldown(a, TIME_COLLECT);    /* collectDroppedFood */
      d->live = 0;
      a->food[a->nfood++] = d->food;
      fly(g, d->food, d->x, d->y, a->x, a->y + 10 + a->nfood * 25, TIME_COLLECT);
    }
  }
  for (unsigned i = 0; i < g->ndropped;)
    if (!g->dropped[i].live) memmove(&g->dropped[i], &g->dropped[i + 1], (size_t)(--g->ndropped - i) * sizeof g->dropped[0]);
    else i++;
  /* the cash box */
  if (active(g, m->cash)) {
    const mm_place *c = P(g, m->cash);
    if (dist2(a->x, a->y, c->pos.x, c->pos.y) < c->radius) {
      if (!a->collecting && g->cash > 0) {     /* collectCashBox: 5% a step, rounded up */
        cooldown(a, TIME_BUYING);
        int32_t take = (g->cash * 5 + 99) / 100;
        g->cash -= take;
        g->p.cash[g->mi] = (uint32_t)g->cash;
        *coins(g) += (uint32_t)take;
        fly(g, 0, c->pos.x, c->pos.y, a->x, a->y + 30, TIME_COLLECT);
      }
      g->near_cash = 1;
    } else {
      g->near_cash = 0;
    }
  }
  /* (the zavods again, as the game does) */
  for (unsigned z = 0; z < m->nzavod; z++)
    if (g->zs[z].sleeping && dist2(a->x, a->y, m->zavods[z].pos.x, m->zavods[z].pos.y) < m->zavods[z].radius)
      zavod_wake(g, z);
  /* the truck to the other mart */
  bool moving = a->vx != 0 || a->vy != 0;
  if (!moving && g->allow_next && active(g, m->truck)) {
    if (dist2(a->x, a->y, g->truck_x, g->truck_y) < 4000) {
      if (g->allow_show_mart) g->want_marts = 1, g->allow_show_mart = 0;
    } else {
      g->allow_show_mart = 1;
    }
  }
}

/* ------------------------------------------------------------------ the player's walking (applyMoveInput) */
/* utils.pointInPolygon4V: in triangle a b c or a d c */
static int32_t sign3(int32_t px, int32_t py, mm_pt a, mm_pt b) {
  return (px - b.x) * (a.y - b.y) - (a.x - b.x) * (py - b.y);
}
static bool in_tri(int32_t px, int32_t py, mm_pt a, mm_pt b, mm_pt c) {
  int32_t d1 = sign3(px, py, a, b), d2 = sign3(px, py, b, c), d3 = sign3(px, py, c, a);
  bool neg = d1 < 0 || d2 < 0 || d3 < 0, pos = d1 > 0 || d2 > 0 || d3 > 0;
  return !(neg && pos);
}

/* checkCollision at a point: the trash's and the baskets' circles (radius 10, us 10) and the bought places'
 * polygons. The "near point" is the polygon's farthest corner (the game's nearPointPolygon4V keeps the largest
 * distance), or the world's middle when only a circle was hit. */
static bool collide(const mm_game *g, int32_t fx, int32_t fy, int32_t *nx, int32_t *ny) {
  const mm_mart *m = g->m;
  bool hit = false;
  *nx = *ny = 0;
  const mm_pt circles[2] = {m->trash, m->basket};
  for (unsigned i = 0; i < 2; i++)
    if (dist2(fx, fy, circles[i].x, circles[i].y) < 400) hit = true;
  for (unsigned i = 0; i < m->count; i++) {
    const mm_place *p = &m->places[i];
    if (!(p->flags & PL_SOLID) || !g->box[i].active || p->poly == NONE) continue;
    if (dist2(fx, fy, p->pos.x, p->pos.y) >= 90000) continue;
    const mm_pt *q = m->polys[p->poly];
    if (in_tri(fx, fy, q[0], q[1], q[2]) || in_tri(fx, fy, q[0], q[3], q[2])) {
      hit = true;
      int32_t best = -1;
      for (unsigned k = 0; k < 4; k++) {
        int32_t d = dist2(fx, fy, q[k].x, q[k].y);
        if (d > best) best = d, *nx = q[k].x, *ny = q[k].y;
      }
    }
  }
  return hit;
}

/* applyMoveInput: the arrows' direction, unless the point 5 ahead is outside the mart or in something. Then the
 * game slides: moving along x it goes along y (and the other way), at the sign of the angle from the near point.
 * On the keypad a diagonal press into a counter first tries its two arrows alone (the game, there, shuffled in
 * place) */
static void apply_move(mm_game *g, int dx, int dy) {
  mm_actor *a = &g->a[A_ME];
  if (!a->input_allowed) return;
  a->vx = (float)dx, a->vy = (float)dy;
  if (!dx && !dy) return;
  const mm_mart *m = g->m;
  int32_t fx = a->x + dx * 5, fy = a->y + dy * 5;      /* futurePoint = pos + velocity * 5 */
  if (fx > m->maxx || fx < m->minx) a->vx = 0;
  if (fy > m->maxy || fy < m->miny) a->vy = 0;
  int32_t nx, ny, t1, t2;
  if (!collide(g, fx, fy, &nx, &ny)) return;
  if (a->vx != 0 && a->vy != 0) {
    if (!collide(g, a->x + dx * 5, a->y, &t1, &t2)) { a->vy = 0; return; }
    if (!collide(g, a->x, a->y + dy * 5, &t1, &t2)) { a->vx = 0; return; }
  }
  int32_t ax = fx - nx, ay = fy - ny;
  /* sin(pi) and cos(+-pi/2) are a hair above 0 in floating point: that still moves a full step */
  if (a->vx != 0) {
    a->vx = 0;
    a->vy = ay > 0 ? 2.0f : ay < 0 ? -2.0f : ax < 0 ? 2.0f : 0.0f;
  } else if (a->vy != 0) {
    a->vx = ax > 0 ? 2.0f : ax < 0 ? -2.0f : 2.0f;
    a->vy = 0;
  }
}

/* Player.update's movement: speed along the velocity's direction, floored */
static void move(mm_actor *a) {
  if (a->vx == 0 && a->vy == 0) return;
  float step = (float)a->speed / (float)MM_TPS;
  float ux = a->vx, uy = a->vy;
  if (ux != 0 && uy != 0) ux = ux > 0 ? 0.70710678f : -0.70710678f, uy = uy > 0 ? 0.70710678f : -0.70710678f;
  else ux = ux > 0 ? 1.0f : ux < 0 ? -1.0f : 0.0f, uy = uy > 0 ? 1.0f : uy < 0 ? -1.0f : 0.0f;
  a->x = (int32_t)floorf((float)a->x + step * ux);
  a->y = (int32_t)floorf((float)a->y + step * uy);
}

static void actor_update(mm_game *g, unsigned id) {
  mm_actor *a = &g->a[id];
  if (!a->alive) return;
  if (a->moving_on) {
    if (a->follow_path) update_following(g, id);
    move(a);
  }
  logic_collect(g, id);
  logic_buy(g, id);
  if (a->sleeping) {
    const mm_actor *me = &g->a[A_ME], *mg = &g->a[A_MANAGER];
    /* (the manager wakes them from where he stands even before he's hired, as in the game: by the door) */
    if (dist2(a->x, a->y, me->x, me->y) < 4000 || dist2(a->x, a->y, mg->x, mg->y) < 4000) wake_up(g, id);
  }
}

/* ------------------------------------------------------------------ what buying a place sets going (placeCheckBought) */
static void hooks(mm_game *g, unsigned place, bool loading) {
  (void)loading;
  for (unsigned i = 0; i < g->m->nhook; i++) {
    const mm_hook *h = &g->m->hooks[i];
    if (h->place != place) continue;
    switch (h->kind) {
      case HK_ON: g->box[h->a].active = 1; break;
      case HK_BUYER: timer(g, (uint32_t)h->b, TM_BUYER, (uint8_t)(A_BUYER0 + h->a), 0, 0, 0); break;
      case HK_STAFF: {
        unsigned id = h->a == 6 ? A_MANAGER : A_CASHIER + (unsigned)h->a;
        g->a[id].alive = 1;
        effect(g, FXK_EXIT, g->a[id].x, g->a[id].y + 40);
        if (id == A_CASHIER) g->have_cashier = 1;
        break;
      }
      case HK_TRUCK: {
        const mm_place *t = P(g, g->m->truck);
        g->truck_x = t->pos.x + h->a, g->truck_y = t->pos.y + h->b;
        g->allow_next = 1;
        if (g->mi == 0 && g->p.sales[1] == 0) g->force_marts = 1;
        break;
      }
    }
  }
}

/* ------------------------------------------------------------------ timers */
static void run_timer(mm_game *g, const mm_timer *t) {
  mm_actor *a = &g->a[t->who];
  switch (t->kind) {
    case TM_BUYER: add_buyer(g, t->who); break;
    case TM_WAKE1: update_action(g, t->who, S_WAKEUP), a->stamina = a->stamina_max; break;
    case TM_WAKE2: a->moving_on = 1, a->brain = B_IDLE, a->anim = AM_IDLE, update_action(g, t->who, S_IDLE); break;
    case TM_PACK_EXIT: update_action(g, t->who, S_EXIT); break;
    case TM_PACK_PAY: cash_add(g, t->b); break;
    case TM_THROW:
      if (a->stage != S_EXIT && a->num_place != NONE) throw_food(g, a->num_place);
      a->allow_drop = 1;
      break;
    case TM_DROP:
      if (g->ndropped >= MM_DROPPED) memmove(g->dropped, g->dropped + 1, (size_t)--g->ndropped * sizeof g->dropped[0]);
      g->dropped[g->ndropped++] = (mm_dropped){t->b, t->c, (uint8_t)t->a, 1};
      break;
    case TM_ZSLEEP: zavod_sleep(g, t->who, true); break;
    case TM_ZWAKE: zavod_sleep(g, t->who, false); break;
    case TM_PAN:
      g->a[A_ME].vx = g->a[A_ME].vy = 0;
      g->control = 0, g->pan_x = t->b, g->pan_y = t->c, g->pan_t = (uint16_t)secs(1.5f);
      timer(g, secs(1.5f), TM_PAN_END, 0, 0, 0, 0);
      break;
    case TM_PAN_END: g->control = 1, g->pan_t = 0, g->slow = 24; break;
  }
}

static void run_timers(mm_game *g) {
  for (;;) {
    int best = -1;
    for (int i = 0; i < g->ntimers; i++)
      if (g->timers[i].due <= g->tick && (best < 0 || g->timers[i].due < g->timers[best].due)) best = i;
    if (best < 0) return;
    mm_timer t = g->timers[best];
    memmove(&g->timers[best], &g->timers[best + 1], (size_t)(--g->ntimers - best) * sizeof t);
    run_timer(g, &t);
  }
}

/* ------------------------------------------------------------------ the seconds */
static void each_second(mm_game *g) {
  /* updateBots */
  bool add = true;
  static const uint8_t order[MM_ACTORS] = {A_ME, A_CASHIER, A_ASSIST2, A_ASSIST3, A_ASSIST4, A_ASSIST5, A_ASSIST6,
                                           A_ROWDY, A_MANAGER, 9, 10, 11, 12, 13, 14, 15, 16};
  for (unsigned k = 1; k < MM_ACTORS; k++) {
    unsigned id = order[k];
    mm_actor *a = &g->a[id];
    if (a->alive) update_bot(g, id);
    else if (a->role == R_BUYER && a->ready_buy && add) add_buyer(g, id), add = false;
  }
  update_queue(g);
  /* checkEvents */
  if (!g->tutorial) {
    if (!g->a[A_ROWDY].alive) g->tick_crazy++;
    if (g->tick_crazy > TIME_SHOW_CRAZY && g->p.sales[g->mi] >= g->m->crazy) {
      add_rowdy(g);
      g->tick_crazy = 0;
    }
  }
  check_badge(g);
}

void mm_step(mm_game *g, int dx, int dy) {
  g->tick++;
  run_timers(g);
  for (unsigned i = 0; i < MM_ACTORS; i++) {
    mm_actor *a = &g->a[i];
    if (a->cool && --a->cool == 0) a->collecting = 0;
  }
  /* updateInput */
  if (g->control) apply_move(g, dx, dy);
  else if (g->a[A_ME].input_allowed) g->a[A_ME].vx = g->a[A_ME].vy = 0;
  for (unsigned i = 0; i < MM_ACTORS; i++) {
    mm_actor *a = &g->a[i];
    int32_t ox = a->x, oy = a->y;
    actor_update(g, i);
    /* dodik.script: facing and running from how the position changed, while it moves at all */
    if (a->moving_on) {
      if (a->x != ox) a->flip = a->x > ox;
      uint8_t an = (a->x != ox || a->y != oy) ? AM_RUN : AM_IDLE;
      if (an != a->anim) a->anim = an, a->anim_t = 0;
    } else if (a->anim == AM_THROW && a->anim_t > 114) {
      a->anim = AM_CRAZY, a->anim_t = 0;        /* crazy3 done: crazy2 */
    } else if (a->anim == AM_DILER && a->anim_t > 120) {
      a->anim = AM_IDLE, a->anim_t = 0;         /* diler1 done: idle */
    }
    a->anim_t++;
    a->lastx = ox, a->lasty = oy;
  }
  if (g->tick % 30 == 0) all_zavods(g);
  if (g->tick % 60 == 0) each_second(g);
  for (unsigned z = 0; z < g->m->nzavod; z++)
    if (g->zs[z].eat) g->zs[z].eat--;
  for (unsigned i = 0; i < MM_FLY; i++)
    if (g->fly[i].t < g->fly[i].n) g->fly[i].t++;
  for (unsigned i = 0; i < 8; i++)
    if (g->fx_kind[i] && ++g->fx_t[i] > 50) g->fx_kind[i] = 0;
  if (g->notice_t) g->notice_t--;
  if (g->hint == 2) g->hint = 3;    /* gone, and not made again (the game's listHelpArrow) */
  /* the camera (orthographic's follow, lerp 0.25 a frame; 0.1 towards and back from a pad) */
  float tx = (float)g->a[A_ME].x, ty = (float)g->a[A_ME].y, l = 0.25f;
  if (g->pan_t) tx = (float)g->pan_x, ty = (float)g->pan_y, l = 0.1f, g->pan_t--;
  else if (g->slow) l = 0.1f, g->slow--;
  g->camx += (tx - g->camx) * l;
  g->camy += (ty - g->camy) * l;
  float bx = (float)(g->m->maxx + 100) - 480.0f, by = (float)g->m->maxy - 360.0f;   /* the bounds: limitMove */
  if (g->camx > bx) g->camx = bx;
  if (g->camx < -bx) g->camx = -bx;
  if (g->camy > by) g->camy = by;
  if (g->camy < -by) g->camy = -by;
  /* the money counter (lume.lerp 0.1 a frame) */
  uint32_t want = *coins(g) << 8;
  if (g->coins_display != want) {
    int64_t d = (int64_t)want - (int64_t)g->coins_display;
    int64_t s = d / 10;
    if (s == 0) s = d > 0 ? 1 : -1;
    g->coins_display = (uint32_t)((int64_t)g->coins_display + s);
    if ((d > 0 && g->coins_display > want) || (d < 0 && g->coins_display < want) || (d < 256 && d > -256)) g->coins_display = want;
  }
}

/* ------------------------------------------------------------------ the windows */
bool mm_upshow_open(const mm_game *g, unsigned show) {
  const mm_upshow *s = &g->m->upshow[show];
  return s->place == NONE || active(g, s->place);
}

bool mm_upgrade(mm_game *g, unsigned show, unsigned k) {
  const mm_upshow *s = &g->m->upshow[show];
  unsigned row = s->row;
  if (!mm_upshow_open(g, show)) return false;
  int price = mm_upgrade_price(g, row, k);
  if (price < 0) return false;
  if (price > 0) {
    if (*coins(g) < (uint32_t)price) {
      notice(g, "Not enough money");
      return false;
    }
    *coins(g) -= (uint32_t)price;
  } else if (g->tutorial && g->step == 6) {
    tutorial(g, 7);
  }
  uint8_t *l = level(g, row, k);
  if (!*l) *l = 1;
  (*l)++;
  apply_upgrade(g, row, k);
  check_badge(g);
  return true;
}

bool mm_buy_hat(mm_game *g, unsigned h) {
  if (h >= MM_HATS) return false;
  if (!(g->p.hats >> h & 1)) {
    const mm_hat *t = &mm_hats[h];
    if (t->kind == HAT_ADS) return false;
    if (t->kind == HAT_COINS) {
      if (*coins(g) < t->price) {
        notice(g, "Not enough money");
        return false;
      }
      *coins(g) -= t->price;
    } else if (g->tutorial && g->step == 9) {
      tutorial(g, 10);
    }
    g->p.hats |= 1u << h;
  }
  g->p.hat = (uint8_t)h;
  g->a[A_ME].hat = (uint8_t)(h ? h + 10 : 0);
  return true;
}

void mm_window(mm_game *g, int which, bool open) {
  if (!g->tutorial) return;
  if (which == 0 && open && g->step == 5) tutorial(g, 6);
  if (which == 0 && !open && g->step == 7) tutorial(g, 8);
  if (which == 1 && open && g->step == 8) tutorial(g, 9);
  if (which == 1 && !open && g->step == 10) tutorial(g, 11);
}

bool mm_mart_open(const mm_game *g, unsigned mart) {
  if (mart == 0) return true;
  return mart == 1 && g->p.have[0][mm_marts[0].truck];
}

void mm_travel(mm_game *g, unsigned mart) {
  g->p.mart = (uint8_t)mart;
  mm_start(g, mm_rand(g));
}

/* ------------------------------------------------------------------ a new game, a mart */
void mm_new_progress(mm_progress *p) {
  memset(p, 0, sizeof *p);
  p->coins = 35;                    /* global.amount_coins */
  p->hats = 1;                      /* have_hats[1] */
  for (unsigned m = 0; m < MM_MARTS; m++)
    for (unsigned r = 0; r < 12; r++)
      for (unsigned k = 0; k < 3; k++) p->level[m][r][k] = 1;
}

bool mm_progress_ok(const mm_progress *p) {
  if (p->mart >= MM_MARTS || p->hat >= MM_HATS || !(p->hats & 1) || p->hats >> MM_HATS || !(p->hats >> p->hat & 1))
    return false;
  if (p->coins > 1000000000u) return false;
  for (unsigned m = 0; m < MM_MARTS; m++) {
    const mm_mart *mt = &mm_marts[m];
    if (p->sales[m] > mt->norder || p->cash[m] > 100000000u) return false;
    for (unsigned i = 0; i < MM_PLACES; i++) {
      if (p->have[m][i] > 1) return false;
      if (i >= mt->count ? (p->have[m][i] || p->paid[m][i]) : p->paid[m][i] > mt->places[i].price) return false;
    }
    for (unsigned r = 0; r < 12; r++)
      for (unsigned k = 0; k < 3; k++) {
        const mm_mart *um = r == 0 ? &mm_marts[0] : mt;
        unsigned most = r < um->nuprow ? um->uprows[r].max[k] + 1u : 1u;
        if (p->level[m][r][k] < 1 || p->level[m][r][k] > most) return false;
      }
  }
  if (p->mart == 1 && !p->have[0][mm_marts[0].truck]) return false;
  return !p->spare[0] && !p->spare[1];
}

void mm_start(mm_game *g, uint32_t seed) {
  mm_progress p = g->p;
  memset(g, 0, sizeof *g);
  g->p = p;
  g->rng = seed ? seed : 1;
  g->mi = p.mart;
  const mm_mart *m = g->m = &mm_marts[g->mi];
  /* initSettingsGame */
  if (g->mi == 1 && g->p.sales[1] == 0) g->p.sales[1] = 1, g->p.cash[1] = 25;
  g->cash = (int32_t)g->p.cash[g->mi];
  g->can_check_queue = 1;
  g->control = 1;
  memcpy(g->grid, m->grid, (size_t)(m->w * m->h + 7) / 8);
  /* addMonkeys */
  for (unsigned i = 0; i < MM_ACTORS; i++) {
    mm_actor *a = &g->a[i];
    a->x = a->lastx = m->door.x, a->y = a->lasty = m->door.y;
    a->speed = a->old_speed = 144;
    a->stamina_max = 6, a->stamina = 6, a->max_food = 12;
    a->input_allowed = 1, a->moving_on = 1, a->allow_collect = 1, a->allow_drop = 1;
    a->num_place = NONE;
    a->basket = 2;
    /* monkey1 is you, 9 the staff, 7 the troublemaker; customers start as 2 .. 7 */
    a->skin = i == A_ME ? 1 : i <= A_ASSIST6 || i == A_MANAGER ? 9 : i == A_ROWDY ? 7 : (uint8_t)(i - A_BUYER0 < 5 ? 2 + i - A_BUYER0 : 7);
  }
  g->a[A_ME].x = m->start.x, g->a[A_ME].y = m->start.y;
  g->a[A_ME].speed = 360;
  g->a[A_ME].role = R_WORKER, g->a[A_ME].alive = 1, g->a[A_ME].max_food = 3;
  g->a[A_ME].hat = (uint8_t)(g->p.hat ? g->p.hat + 10 : 0);
  g->a[A_CASHIER].role = R_CASHIER;
  for (unsigned i = A_ASSIST2; i <= A_ASSIST6; i++) g->a[i].role = R_STOCKMAN;
  g->a[A_ROWDY].role = R_ROWDY;
  g->a[A_MANAGER].role = R_MANAGER;
  for (unsigned i = 0; i < MM_BUYERS; i++) {
    mm_actor *a = &g->a[A_BUYER0 + i];
    a->role = R_BUYER;
    a->x = exit_x(g, 5) + 5 * (int32_t)(i + 1), a->y = exit_y(g, 5);
  }
  /* checkFromSave */
  for (unsigned i = 0; i < m->count; i++) {
    mm_box *b = &g->box[i];
    const mm_place *pl = P(g, i);
    if (g->p.have[g->mi][i]) b->active = 1;
    else b->paid = g->p.paid[g->mi][i];
    b->cap = pl->cap0, b->build = pl->build, b->stamina = b->stamina_max = pl->stamina;
  }
  for (unsigned k = 0; k < m->norder && k < g->p.sales[g->mi]; k++) g->box[m->order[k]].active = 1;
  /* addPlaces: food on hand; the bought places close their grid cells */
  for (unsigned i = 0; i < m->count; i++) {
    const mm_place *pl = P(g, i);
    mm_box *b = &g->box[i];
    b->count = pl->count0;
    if ((pl->flags & PL_ZAVOD) && !b->active) b->count = 0;
    if (b->active && (pl->flags & PL_SOLID)) update_map(g, i);
  }
  /* fillWarehouses */
  for (unsigned i = 0; i < m->nfill; i++)
    if (g->box[m->fills[i][0]].active) {
      mm_box *b = &g->box[m->fills[i][1]];
      b->count = (uint8_t)rnd2(g, 0, b->cap - 5);
    }
  /* setupDodiki */
  for (unsigned s = 0; s < MM_STAFF; s++) {
    mm_actor *a = &g->a[s == 6 ? A_MANAGER : A_CASHIER + s];
    a->hat = m->staff[s].hat;
    a->max_food = m->staff[s].carry;
    if (a->role == R_STOCKMAN && m->staff[s].ngoal) a->brain = B_GO_START;
  }
  /* placeCheckBought for everything, then checkNextPlaceSale */
  for (unsigned i = 0; i < m->count; i++)
    if (g->box[i].active) hooks(g, i, true);
  for (unsigned i = 0; i < m->count; i++)
    if (g->box[i].active) next_sale(g, (int)i, true);
  /* checkTutorial */
  if (g->mi == 0 && g->p.sales[0] <= 1) g->tutorial = 1, g->step = 0;
  next_sale(g, -1, false);
  /* initUpgrades */
  for (unsigned r = 0; r < m->nuprow; r++)
    for (unsigned k = 0; k < 3; k++) apply_upgrade(g, r, k);
  check_badge(g);
  g->camx = (float)g->a[A_ME].x, g->camy = (float)g->a[A_ME].y;
  g->coins_display = g->p.coins << 8;
}
