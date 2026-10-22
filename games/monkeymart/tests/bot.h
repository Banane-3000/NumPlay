/* A player for the tests (tests/play.c): it runs the shop as a person would, with the arrows only, and buys
 * every place in order until the truck to the second mart is bought. It reads the game's state to decide, and
 * walks the game's own path grid. Included by play.c after game.c. */
static int32_t bot_tx, bot_ty;          /* where it's going */
static uint8_t bot_path[MM_PATH][2], bot_np, bot_wi;
static int32_t bot_lastx, bot_lasty, bot_stuck, bot_wiggle, bot_wdx, bot_wdy, bot_best, bot_since;
static int bot_task, bot_lastwi;                    /* what it's doing (for the log) */
static uint32_t bot_rng = 12345;
static uint32_t bot_r(void) { bot_rng ^= bot_rng << 13, bot_rng ^= bot_rng >> 17, bot_rng ^= bot_rng << 5; return bot_rng; }

/* a free point within reach r2 of (x, y), nearest the monkey: things to wake or reach can stand inside a counter */
static void bot_reach(const mm_game *g, int32_t *x, int32_t *y, int32_t r2) {
  int32_t nx, ny, bx = *x, by = *y, bd = 0x7FFFFFFF;
  int tx, ty;
  tile_from_pos(*x, *y, &tx, &ty);
  if (!collide(g, *x, *y, &nx, &ny) && walkable(g, tx, ty)) return;
  for (int r = 30; r * r < r2; r += 20)
    for (int k = 0; k < 16; k++) {
      static const int8_t c16[16][2] = {{10, 0}, {9, 4}, {7, 7}, {4, 9}, {0, 10}, {-4, 9}, {-7, 7}, {-9, 4},
                                        {-10, 0}, {-9, -4}, {-7, -7}, {-4, -9}, {0, -10}, {4, -9}, {7, -7}, {9, -4}};
      int32_t px = *x + c16[k][0] * r / 10, py = *y + c16[k][1] * r / 10;
      tile_from_pos(px, py, &tx, &ty);
      if (collide(g, px, py, &nx, &ny) || !walkable(g, tx, ty) || dist2(px, py, *x, *y) >= r2) continue;
      int32_t d = dist2(px, py, g->a[A_ME].x, g->a[A_ME].y) + r * r * 4;
      if (d < bd) bd = d, bx = px, by = py;
    }
  *x = bx, *y = by;
}

static void bot_goto(mm_game *g, int32_t x, int32_t y) {
  if (x == bot_tx && y == bot_ty && bot_np) return;
  bot_tx = x, bot_ty = y;
  int sx, sy, ex, ey;
  tile_from_pos(g->a[A_ME].x, g->a[A_ME].y, &sx, &sy);
  tile_from_pos(x, y, &ex, &ey);
  bot_np = (uint8_t)find_path(g, sx, sy, ex, ey, bot_path);
  bot_wi = 1;
  bot_best = 0x7FFFFFFF, bot_since = 0;
}
/* the arrows towards the target, along the grid's path */
static void bot_steer(mm_game *g, int *dx, int *dy) {
  mm_actor *a = &g->a[A_ME];
  int32_t wx = bot_tx, wy = bot_ty;
  if (bot_np && bot_wi < bot_np) {
    int32_t px, py;
    pos_from_tile(bot_path[bot_wi][0], bot_path[bot_wi][1], &px, &py);
    px -= 20;                          /* (a tile's middle) */
    if (abs(px - a->x) < 30 && abs(py - a->y) < 30) bot_wi++;
    if (bot_wi < bot_np) {
      if (bot_wi != bot_lastwi) bot_lastwi = bot_wi, bot_best = 0x7FFFFFFF, bot_since = 0;
      wx = px, wy = py;
    }
  }
  *dx = wx > a->x + 4 ? 1 : wx < a->x - 4 ? -1 : 0;
  *dy = wy > a->y + 4 ? 1 : wy < a->y - 4 ? -1 : 0;
  int32_t d = dist2(a->x, a->y, wx, wy);
  if (d < bot_best - 400) bot_best = d, bot_since = 0;
  else if (++bot_since > 90) bot_since = 0, bot_best = d, bot_wiggle = 15 + (int)(bot_r() % 25),
       bot_wdx = (int)(bot_r() % 3) - 1, bot_wdy = (int)(bot_r() % 3) - 1;
  if (bot_wiggle > 0) {
    bot_wiggle--;
    *dx = bot_wdx, *dy = bot_wdy;
  } else if (*dx || *dy) {
    if (a->x == bot_lastx && a->y == bot_lasty) {
      if (++bot_stuck > 12) {
        bot_stuck = 0, bot_wiggle = 10 + (int)(bot_r() % 20);
        bot_wdx = (int)(bot_r() % 3) - 1, bot_wdy = (int)(bot_r() % 3) - 1;
        if (!bot_wdx && !bot_wdy) bot_wdy = 1;
      }
    } else {
      bot_stuck = 0;
    }
  }
  bot_lastx = a->x, bot_lasty = a->y;
}
static bool bot_near(const mm_game *g, int32_t x, int32_t y, int32_t r2) { return dist2(g->a[A_ME].x, g->a[A_ME].y, x, y) < r2; }

/* the food a place makes, if it's a source with food */
static int bot_source(const mm_game *g, unsigned food) {
  int best = -1;
  int32_t bd = 0x7FFFFFFF;
  for (unsigned i = 0; i < g->m->count; i++) {
    const mm_place *p = P(g, i);
    if (!(p->flags & PL_ZAVOD) || !g->box[i].active || p->food != food || !g->box[i].count) continue;
    int32_t d = dist2(g->a[A_ME].x, g->a[A_ME].y, p->work.x, p->work.y);
    if (d < bd) bd = d, best = (int)i;
  }
  return best;
}

static int bot_target_shelf = -1;
/* one decision; returns the arrows */
static void bot(mm_game *g, int *dx, int *dy) {
  mm_actor *me = &g->a[A_ME];
  const mm_mart *m = g->m;
  *dx = *dy = 0;
  if (!g->control) return;
  /* 1. a place we can pay for whole: its pad */
  int buy = -1;
  if (g->sale_next != NONE && !g->box[g->sale_next].active) buy = g->sale_next;
  if (buy >= 0 && g->p.coins + 0u < (unsigned)(P(g, (unsigned)buy)->price - g->box[buy].paid)) buy = -1;
  for (unsigned i = 0; i < m->count && buy < 0; i++)
    if (g->box[i].selling && !g->box[i].active && g->p.coins >= (unsigned)(P(g, i)->price - g->box[i].paid) &&
        P(g, i)->price - g->box[i].paid <= g->p.coins / 2 + 5)
      buy = (int)i;
  if (buy >= 0) {
    bot_task = 1;
    const mm_place *p = P(g, (unsigned)buy);
    if (bot_near(g, p->buy.x, p->buy.y, 600)) return;      /* standing: paying */
    bot_goto(g, p->buy.x, p->buy.y);
    bot_steer(g, dx, dy);
    if (bot_near(g, p->buy.x, p->buy.y, 1500)) *dx = *dy = 0, (void)0;
    return;
  }
  /* the second mart, once the truck is bought: to the truck's stop */
  if (g->mi == 0 && g->allow_next && active(g, m->truck)) {
    bot_task = 10;
    if (bot_near(g, g->truck_x, g->truck_y, 1500)) return;
    int32_t x = g->truck_x, y = g->truck_y;
    bot_reach(g, &x, &y, 3000);
    bot_goto(g, x, y);
    bot_steer(g, dx, dy);
    if (bot_near(g, g->truck_x, g->truck_y, 2500)) *dx = *dy = 0;
    return;
  }
  /* 2. the troublemaker */
  if (g->allow_rowdy && g->a[A_ROWDY].alive && g->a[A_ROWDY].stage == S_CRAZY) {
    bot_task = 2;
    bot_goto(g, g->a[A_ROWDY].x, g->a[A_ROWDY].y);
    bot_steer(g, dx, dy);
    return;
  }
  /* 3. sleepers */
  for (unsigned z = 0; z < m->nzavod; z++)
    if (g->zs[z].sleeping) {
      bot_task = 3;
      int32_t x = m->zavods[z].pos.x, y = m->zavods[z].pos.y;
      bot_reach(g, &x, &y, m->zavods[z].radius * 3 / 4);
      bot_goto(g, x, y);
      bot_steer(g, dx, dy);
      return;
    }
  for (unsigned i = A_ASSIST2; i <= A_ASSIST6; i++)
    if (g->a[i].alive && g->a[i].sleeping && !g->a[A_MANAGER].alive) {
      bot_task = 4;
      bot_goto(g, g->a[i].x, g->a[i].y);
      bot_steer(g, dx, dy);
      return;
    }
  /* 4. money at the till, or customers waiting with no cashier */
  const mm_place *c = P(g, m->cash);
  bool waiting = g->nqueue && !g->have_cashier;
  if ((g->cash >= 20 || waiting || (g->cash > 0 && me->nfood == 0 && g->p.coins < 30)) && active(g, m->cash)) {
    bot_task = 5;
    if (bot_near(g, c->pos.x, c->pos.y, 12000)) {
      if (g->cash > 0 || waiting) return;
    } else {
      bot_goto(g, c->pos.x + 100, c->pos.y - 30);
      bot_steer(g, dx, dy);
      return;
    }
  }
  /* 5. stock: carrying something -> where it goes; else pick a shelf or feeder short of food */
  if (me->nfood) {
    unsigned f = me->food[me->nfood - 1];
    int best = -1, bd = -1;
    for (unsigned i = 0; i < m->count; i++) {
      const mm_place *p = P(g, i);
      if (!(p->flags & PL_SKLAD) || i == m->cash || !g->box[i].active || p->food != f) continue;
      int gap = g->box[i].cap - g->box[i].count;
      if (gap > bd) bd = gap, best = (int)i;
    }
    if (best < 0 || bd <= 0) {
      bot_task = 6;
      bot_goto(g, m->trash.x, m->trash.y - 30);
      bot_steer(g, dx, dy);
      return;
    }
    const mm_place *p = P(g, (unsigned)best);
    bot_task = 7;
    /* full hands, or nothing more to fetch for it */
    int src = bot_source(g, f);
    if (me->nfood < me->max_food && src >= 0 && bd > me->nfood && !bot_near(g, p->work.x, p->work.y, p->radius)) {
      const mm_place *s = P(g, (unsigned)src);
      if (dist2(me->x, me->y, s->work.x, s->work.y) < 250000) {
        int32_t x = s->work.x, y = s->work.y;
        bot_reach(g, &x, &y, s->radius / 2);
        bot_goto(g, x, y);
        bot_steer(g, dx, dy);
        return;
      }
    }
    if (bot_near(g, p->work.x, p->work.y, p->radius * 3 / 4)) return;
    int32_t x = p->work.x, y = p->work.y;
    bot_reach(g, &x, &y, p->radius / 2);
    bot_goto(g, x, y);
    bot_steer(g, dx, dy);
    return;
  }
  int best = -1;
  float bscore = 0;
  for (unsigned i = 0; i < m->count; i++) {
    const mm_place *p = P(g, i);
    if (!(p->flags & PL_SKLAD) || i == m->cash || !g->box[i].active) continue;
    int gap = g->box[i].cap - g->box[i].count;
    if (gap <= 0 || bot_source(g, p->food) < 0) continue;
    float score = (float)gap / g->box[i].cap + ((p->flags & PL_BUYER) ? 0.3f : 0.5f);
    if (score > bscore) bscore = score, best = (int)i;
  }
  if (best >= 0) {
    bot_task = 8;
    int src = bot_source(g, P(g, (unsigned)best)->food);
    const mm_place *s = P(g, (unsigned)src);
    int32_t x = s->work.x, y = s->work.y;
    bot_reach(g, &x, &y, s->radius / 2);
    bot_goto(g, x, y);
    bot_steer(g, dx, dy);
    return;
  }
  /* 6. nothing to do: by the till */
  bot_task = 9;
  if (!bot_near(g, c->pos.x, c->pos.y, 12000)) {
    bot_goto(g, c->pos.x + 100, c->pos.y - 30);
    bot_steer(g, dx, dy);
  }
}

/* the windows a player opens: the tutorial's upgrade and hat, then upgrades it can afford */
static void bot_windows(mm_game *g) {
  if (g->tutorial) {
    if (g->step == 5) mm_window(g, 0, true);
    if (g->step == 6) mm_upgrade(g, 0, 1);
    if (g->step == 7) mm_window(g, 0, false);
    if (g->step == 8) mm_window(g, 1, true);
    if (g->step == 9) mm_buy_hat(g, 1);
    if (g->step == 10) mm_window(g, 1, false);
    return;
  }
  /* carry more first; then workers' speed and stack, the machines' speed, once the next place is paid for */
  int pr = mm_upgrade_price(g, 0, 1);
  if (pr >= 0 && g->p.coins >= (unsigned)pr + 60) mm_upgrade(g, 0, 1);
  if (g->sale_next == NONE || g->p.coins > 2u * (P(g, g->sale_next)->price - g->box[g->sale_next].paid) + 300)
    for (unsigned s = 1; s < g->m->nupshow; s++)
      for (unsigned k = 0; k < 3; k++) {
        int q = mm_upgrade_price(g, g->m->upshow[s].row, k);
        if (q > 0 && mm_upshow_open(g, s) && g->p.coins >= (unsigned)q + 300) mm_upgrade(g, s, k);
      }
}
