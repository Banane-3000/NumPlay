/* The world: the original's tiles in a grid. A stage is its map (bottom row first, like the original's cache);
 * the arcade is a ring of TM_H rows of TM_W columns: two 13-wide lanes with 19 columns of void between them, the
 * x wrapping round, so a bonus zone's hall leads from one lane to the other the way the original's goes 127 tiles
 * sideways. Sections are chosen and filled as the original's GameController.GetNextSectionIndex and LevelLoader. */
#include "inner.h"
#include "data.h"
#include "../../common/np_lz.h"

int32_t tm_value16(tm_game *g) { return (int32_t)(tm_rand(g) >> 16); }
int32_t tm_vary(tm_game *g, int32_t base, int32_t var) { return base + (int32_t)(((int64_t)var * tm_value16(g)) >> 16); }

int tm_cell(int32_t u) { return (int)((u + HALF) >> 12); }

static int lane_of(int x) {
  x &= TM_W - 1;
  if (x < TM_LANE) return 0;
  if (x >= TM_LANE + TM_GAP && x < 2 * TM_LANE + TM_GAP) return 1;
  return -1;
}

int tm_index(const tm_game *g, int x, int y) {
  if (g->mode == MODE_STAGE) return (x < 0 || y < 0 || x >= g->w || y >= g->h) ? -1 : y * g->w + x;
  if (y >= g->ring_top || y < g->ring_top - TM_H) return -1;
  return (y & (TM_H - 1)) * TM_W + (x & (TM_W - 1));
}

uint8_t tm_tile(const tm_game *g, int x, int y) {
  if (g->mode == MODE_STAGE) {
    int i = tm_index(g, x, y);
    return i < 0 ? T_WALL : g->tile[i];
  }
  if (y >= g->ring_top) return T_EMPTY;
  if (y < g->ring_top - TM_H) return T_WALL;
  int l = lane_of(x);
  if (l >= 0 && y < g->lane_base[l]) return T_WALL;
  return g->tile[(y & (TM_H - 1)) * TM_W + (x & (TM_W - 1))];
}

uint8_t tm_look(const tm_game *g, int x, int y) {
  int i = tm_index(g, x, y);
  return i < 0 ? 0 : g->look[i];
}

void tm_set(tm_game *g, int x, int y, uint8_t t) {
  int i = tm_index(g, x, y);
  if (i >= 0) g->tile[i] = t;
}

bool tm_wallish(uint8_t t) {
  return t == T_WALL || t == T_INNER || t == T_SPIKES || t == T_MSPIKES || t == T_BONUS_WALL || t == T_BRICK;
}
bool tm_is_cannon(uint8_t t) { return t == T_CANNON_L || t == T_CANNON_R || t == T_CANNON_U || t == T_CANNON_D; }
bool tm_is_portal(uint8_t t) { return (t >= 49 && t <= 55) || (t >= 61 && t <= 67); }
bool tm_is_tramplin(uint8_t t) { return t == T_TRAMP_DR || t == T_TRAMP_DL || t == T_TRAMP_UR || t == T_TRAMP_UL; }
/* what turns with a rotor (LevelLoader.AddRotors keeps these in its copy): walls, spikes, trap spikes, portals 1..6,
 * tramplins, and the left, right and up cannons */
bool tm_is_rotor_part(uint8_t t) {
  return t == T_WALL || t == T_SPIKES || t == T_MSPIKES || (t >= 49 && t <= 54) || (t >= 61 && t <= 66) ||
         tm_is_tramplin(t) || t == T_CANNON_L || t == T_CANNON_R || t == T_CANNON_U;
}

/* what stops a dash (the raycast's layers: walls, ice, cannons, fish, rotors, active timer walls) */
bool tm_solid(const tm_game *g, int x, int y) {
  uint8_t t = tm_tile(g, x, y);
  return tm_wallish(t) || t == T_ICE || tm_is_cannon(t) || t == T_FISH || t == T_ROTOR_CW || t == T_ROTOR_CCW ||
         (t == T_TIMER_WALL && tm_timer_wall_on(g, x, y));
}

bool tm_timer_wall_on(const tm_game *g, int x, int y) {
  for (int i = 0; i < TM_TIMER_WALLS; i++)
    if (g->tw[i].left > 0 && g->tw[i].x == x && g->tw[i].y == y) return true;
  return false;
}

int tm_lane_centre(const tm_game *g) { return (g->lane_x + 6) * TT; }

/* ------------------------------------------------------------------ making things from tiles (LevelLoader) */
/* a section (or a stage) being filled: its cells, with its place in the world */
typedef struct {
  uint8_t *t;                          /* w x h cells, bottom row first */
  int w, h, x0, y0;
  uint8_t style, rules;                /* rules: the look bits for walls (seams) */
  bool bonus, arcade;
} sect;

static uint8_t at(const sect *s, int x, int y) { return (x < 0 || y < 0 || x >= s->w || y >= s->h) ? 0 : s->t[y * s->w + x]; }

static uint8_t seen[TM_CELLS];         /* the floods' marks (static: the stack stays small) */

/* the rotors and what they hold (AddRotors, CountRotorParts): joined to the rotor orthogonally */
static void make_rotors(tm_game *g, sect *s) {
  for (int y = 0; y < s->h; y++)
    for (int x = 0; x < s->w; x++) {
      uint8_t t = at(s, x, y);
      if (t != T_ROTOR_CW && t != T_ROTOR_CCW) continue;
      tm_rotor *r = NULL;
      for (int i = 0; i < TM_ROTORS; i++)
        if (!g->rotor[i].active) { r = &g->rotor[i]; break; }
      if (!r) continue;
      memset(r, 0, sizeof *r);
      r->active = 1, r->ccw = t == T_ROTOR_CCW, r->x = s->x0 + x, r->y = s->y0 + y;
      /* flood: a queue of cells (offsets), only rotor parts */
      memset(seen, 0, (size_t)(s->w * s->h));
      seen[y * s->w + x] = 1;
      int qx[TM_ROTOR_PARTS + 4], qy[TM_ROTOR_PARTS + 4], head = 0, tail = 0;
      qx[tail] = x, qy[tail] = y, tail++;
      while (head < tail) {
        int cx = qx[head], cy = qy[head];
        head++;
        static const int8_t d4[4][2] = {{-1, 0}, {1, 0}, {0, -1}, {0, 1}};
        for (int k = 0; k < 4; k++) {
          int nx = cx + d4[k][0], ny = cy + d4[k][1];
          if (nx < 0 || ny < 0 || nx >= s->w || ny >= s->h || seen[ny * s->w + nx]) continue;
          if (!tm_is_rotor_part(at(s, nx, ny))) continue;
          seen[ny * s->w + nx] = 1;
          if (r->count >= TM_ROTOR_PARTS) continue;
          r->px[r->count] = (int8_t)(nx - x), r->py[r->count] = (int8_t)(ny - y);
          r->tile[r->count] = at(s, nx, ny);
          r->count++;
          if (tail < TM_ROTOR_PARTS + 4) qx[tail] = nx, qy[tail] = ny, tail++;
        }
      }
    }
}

/* platforms (AddPlatforms): the box round a joined group of one platform tile; every platform tile in it is a
 * brick; it moves the first tile's way */
static void make_platforms(tm_game *g, sect *s) {
  for (int y = 0; y < s->h; y++)
    for (int x = 0; x < s->w; x++) {
      uint8_t t = at(s, x, y);
      if (t != T_PLAT_R && t != T_PLAT_L && t != T_PLAT_U && t != T_PLAT_D) continue;
      int minx = x, maxx = x, miny = y, maxy = y;
      int qx[64], qy[64], head = 0, tail = 0;
      memset(seen, 0, (size_t)(s->w * s->h));
      qx[tail] = x, qy[tail] = y, tail++, seen[y * s->w + x] = 1;
      while (head < tail) {
        int cx = qx[head], cy = qy[head];
        head++;
        static const int8_t d4[4][2] = {{-1, 0}, {1, 0}, {0, -1}, {0, 1}};
        for (int k = 0; k < 4; k++) {
          int nx = cx + d4[k][0], ny = cy + d4[k][1];
          if (nx < 0 || ny < 0 || nx >= s->w || ny >= s->h || seen[ny * s->w + nx] || at(s, nx, ny) != t) continue;
          seen[ny * s->w + nx] = 1;
          if (nx < minx) minx = nx;
          if (nx > maxx) maxx = nx;
          if (ny < miny) miny = ny;
          if (ny > maxy) maxy = ny;
          if (tail < 64) qx[tail] = nx, qy[tail] = ny, tail++;
        }
      }
      tm_platform *p = NULL;
      for (int i = 0; i < TM_PLATFORMS; i++)
        if (!g->plat[i].active) { p = &g->plat[i]; break; }
      for (int by = miny; by <= maxy; by++)
        for (int bx = minx; bx <= maxx; bx++) {
          uint8_t u = at(s, bx, by);
          if (u != T_PLAT_R && u != T_PLAT_L && u != T_PLAT_U && u != T_PLAT_D) continue;
          s->t[by * s->w + bx] = T_EMPTY;
          if (p && p->count < TM_PLAT_BRICKS) p->bx[p->count] = (int8_t)(bx - minx), p->by[p->count] = (int8_t)(by - miny), p->count++;
        }
      if (!p) continue;
      p->active = 1, p->x = (s->x0 + minx) * TT, p->y = (s->y0 + miny) * TT, p->wait = 0, p->frame = 0;
      p->dx = (int8_t)(t == T_PLAT_R ? 1 : t == T_PLAT_L ? -1 : 0), p->dy = (int8_t)(t == T_PLAT_U ? 1 : t == T_PLAT_D ? -1 : 0);
    }
}

/* light balls and their triggers (AddLightBalls): ball n and trigger n of the same section */
static void make_balls(tm_game *g, sect *s) {
  int bx[6], by[6], tx[6], ty[6];
  for (int i = 0; i < 6; i++) bx[i] = tx[i] = -1;
  for (int y = 0; y < s->h; y++)
    for (int x = 0; x < s->w; x++) {
      uint8_t t = at(s, x, y);
      for (int k = 0; k < 6; k++) {
        if (t == T_BALL + 12 * k) bx[k] = x, by[k] = y, s->t[y * s->w + x] = T_EMPTY;
        if (t == T_BALL_TRIGGER + 12 * k) tx[k] = x, ty[k] = y;
      }
    }
  for (int k = 0; k < 6; k++) {
    if (bx[k] < 0 || tx[k] < 0) {
      if (tx[k] >= 0) s->t[ty[k] * s->w + tx[k]] = T_EMPTY;
      continue;
    }
    for (int i = 0; i < TM_BALLS; i++)
      if (!g->ball[i].active) {
        tm_ball *b = &g->ball[i];
        memset(b, 0, sizeof *b);
        b->active = 1;
        b->home_x = b->x = (s->x0 + bx[k]) * TT, b->home_y = b->y = (s->y0 + by[k]) * TT;
        b->trig_x = s->x0 + tx[k], b->trig_y = s->y0 + ty[k];
        break;
      }
  }
}

/* things that move: bats */
static void make_actors(tm_game *g, sect *s) {
  for (int y = 0; y < s->h; y++)
    for (int x = 0; x < s->w; x++) {
      uint8_t t = at(s, x, y);
      int dx = t == T_BAT_R ? 1 : t == T_BAT_L ? -1 : 0, dy = t == T_BAT_U ? 1 : t == T_BAT_D ? -1 : 0;
      if (!dx && !dy) continue;
      s->t[y * s->w + x] = T_EMPTY;
      tm_spawn(g, A_BAT, (s->x0 + x) * TT, (s->y0 + y) * TT, dx, dy);
    }
}

/* the bonus door's hall (AddBonusHall): walls above and below its row, through the void to the other lane */
static void make_hall(tm_game *g, int x, int y, int dir, uint8_t style) {
  for (int b = 1; b <= TM_GAP + 1; b++) {
    int cx = x + b * dir;
    if (lane_of(cx) >= 0 && b > 1) break;
    int o = tm_index(g, cx, y);
    if (o >= 0 && lane_of(cx) < 0) g->look[o] = 128;   /* the hall itself: open (the void elsewhere is drawn as wall) */
    for (int k = -1; k <= 1; k += 2) {
      int i = tm_index(g, cx, y + k);
      if (i < 0) continue;
      g->tile[i] = T_WALL;
      g->look[i] = (uint8_t)(style | 8 | 16);   /* replicate rule: the corridor's edges only */
    }
  }
}

/* the cells into the world, with things made */
static void place(tm_game *g, sect *s) {
  make_rotors(g, s);
  make_platforms(g, s);
  make_balls(g, s);
  make_actors(g, s);
  for (int y = 0; y < s->h; y++)
    for (int x = 0; x < s->w; x++) {
      int i = tm_index(g, s->x0 + x, s->y0 + y);
      if (i < 0) continue;
      uint8_t t = s->t[y * s->w + x];
      uint8_t lk = s->style;
      if (s->arcade) {
        if (y == 0) lk |= 8;
        if (y == s->h - 1) lk |= 16;
        if (!s->bonus) lk |= 64;
      }
      if ((t == T_ICE || t == T_CLOSE) && tm_value16(g) < g->coin_chance) lk |= 32;   /* the ice's coin roll */
      g->tile[i] = t;
      g->look[i] = lk;
      if (t == T_COIN || t == T_DOT) g->dots_all++;
      if (t == T_STAR) g->stars_all++;
    }
  for (int y = 0; y < s->h; y++)
    for (int x = 0; x < s->w; x++)
      if (s->t[y * s->w + x] == T_DOOR) make_hall(g, s->x0 + x, s->y0 + y, x == 0 ? -1 : 1, s->style);
}

/* ------------------------------------------------------------------ stages */
/* ProcessMonkeysAndSnakes: monkey triggers keep a coin and wake 1..3 monkeys; snake n's enter and exit become walls
 * and its trigger (a coin) sends it out of the enter wall toward the exit */
static void stage_triggers(tm_game *g, sect *s) {
  for (int y = 0; y < s->h; y++)
    for (int x = 0; x < s->w; x++) {
      uint8_t t = at(s, x, y);
      if (t >= T_MONKEYS && t <= T_MONKEYS + 2) {
        s->t[y * s->w + x] = T_COIN;
        tm_actor *a = tm_spawn(g, A_MTRIGGER, x * TT, y * TT, 0, 0);
        if (a) a->aux = t - T_MONKEYS + 1;
      }
    }
  for (int k = 0; k < 6; k++) {
    int ex = -1, ey = 0, xx = -1, xy = 0, tx = -1, ty = 0;
    for (int y = 0; y < s->h; y++)
      for (int x = 0; x < s->w; x++) {
        uint8_t t = at(s, x, y);
        if (t == T_SNAKE_ENTER + 12 * k) ex = x, ey = y;
        if (t == T_SNAKE_ENTER + 1 + 12 * k) xx = x, xy = y;
        if (t == T_SNAKE_ENTER + 2 + 12 * k) tx = x, ty = y;
      }
    int found = 0;
    if (tx >= 0) s->t[ty * s->w + tx] = T_COIN;
    if (ex >= 0) s->t[ey * s->w + ex] = T_WALL, found++;
    if (xx >= 0) s->t[xy * s->w + xx] = T_WALL, found++;
    if (tx < 0 || found < 2 || (ex == xx && ey == xy)) continue;
    tm_snake_line l;
    memset(&l, 0, sizeof l);
    l.x = ex, l.y = ey;
    int ddx = xx - ex, ddy = xy - ey;
    l.dx = (int8_t)(ddx > 0 ? 1 : ddx < 0 ? -1 : 0), l.dy = (int8_t)(ddx ? 0 : ddy > 0 ? 1 : ddy < 0 ? -1 : 0);
    int dist = ddx ? iabs(ddx) : iabs(ddy);
    for (int i = 1; i < dist && l.n < TM_SNAKE_LEN; i++) {
      uint8_t u = at(s, ex + l.dx * i, ey + l.dy * i);
      l.open[l.n++] = !tm_wallish(u);
    }
    while (l.n && !l.open[l.n - 1]) l.n--;
    if (!l.n) continue;
    tm_actor *a = tm_spawn(g, A_STRIGGER, tx * TT, ty * TT, 0, 0);
    if (!a) continue;
    g->sline[k] = l;                   /* the line waits for its trigger */
    a->aux = k;
  }
}

void tm_load_stage(tm_game *g, int st) {
  const tm_map *m = &tm_stages[st];
  g->w = m->w, g->h = m->h;
  memset(g->tile, 0, sizeof g->tile);
  memset(g->look, 0, sizeof g->look);
  static uint8_t cells[TM_CELLS];   /* (static: the stack stays small) */
  np_lz_unpack(tm_maps + m->at, m->size, cells, TM_CELLS);
  int n = g->w * g->h;
  sect s = {cells, g->w, g->h, 0, 0, (uint8_t)((st / 10) % 5), 0, false, false};
  /* ProcessPointsAndCoins first, then the triggers (whose cells become coins), then the rest */
  for (int i = 0; i < n; i++)
    if (cells[i] == T_COIN && tm_value16(g) >= g->coin_chance) cells[i] = T_DOT;
  stage_triggers(g, &s);
  place(g, &s);
  for (int i = 0; i < n; i++)
    if (g->tile[i] == T_ENTER) {
      g->x = (i % g->w) * TT, g->y = (i / g->w) * TT;
      g->tile[i] = T_EMPTY;
    }
}

/* ------------------------------------------------------------------ the arcade's sections */
enum { SEC_BEGIN, SEC_EASY, SEC_SPIKES, SEC_BATS, SEC_CANNONS, SEC_FISHES, SEC_PLATFORMS, SEC_TRAMPLINS, SEC_HARD,
       SEC_PORTALS, SEC_BONUS_ENTER, SEC_BONUS_RUN, SEC_SNAKE, SEC_TUTORIAL, SEC_BONUS_PIPES };

typedef struct { int type, index; bool inverted, no_enemies; } sec_pick;

/* the "stack" pick (f_zqkc): a random one of the first part of the type's list, moved to its end */
static sec_pick stack_pick(tm_game *g, int type, int min, int percent, bool inverted) {
  int n = tm_section_count[type];
  int hi = n - n * percent / 100;      /* n - (int)(n * fraction) */
  int i = tm_range(g, min, hi);
  uint16_t *o = g->order + tm_section_first[type];
  uint16_t id = o[i];
  memmove(o + i, o + i + 1, (size_t)(n - 1 - i) * sizeof *o);
  o[n - 1] = id;
  sec_pick p = {type, n - 1, inverted, false};
  return p;
}

static sec_pick fixed(int type, int index, bool inverted) {
  sec_pick p = {type, index, inverted, false};
  return p;
}

/* GameController.GetNextSectionIndex */
static sec_pick next_section(tm_game *g) {
  if (g->bonus_active) {
    int t = g->bonus == BONUS_RUN ? SEC_BONUS_RUN : SEC_BONUS_PIPES;
    if (g->bonus_count == 0) return fixed(t, 0, false);
    if (g->bonus_count < g->bonus_len) {
      if (g->bonus == BONUS_PIPES) return stack_pick(g, t, 2, 50, tm_value16(g) > 32768);
      if (g->bonus_count != g->bonus_len - 1 && g->bonus_count != 1) return stack_pick(g, t, 3, 50, tm_value16(g) > 32768);
      return fixed(t, 2, false);
    }
    if (g->bonus_count == g->bonus_len) return fixed(t, 1, false);
    return fixed(SEC_BEGIN, 0, false);
  }
  if (g->in_zone == 0) return stack_pick(g, SEC_BEGIN, 2, 0, tm_value16(g) > 32768);
  if (g->in_zone >= g->zone_len) {
    if (g->in_zone != g->zone_len) return fixed(SEC_BEGIN, 0, false);
    return fixed(SEC_BEGIN, 1, tm_range(g, 0, 2) == 0);
  }
  bool d = tm_value16(g) > 32768;
  if (g->snake_mode) return stack_pick(g, SEC_SNAKE, 0, 30, d);
  if (g->want_platforms) {
    g->want_platforms = 0;
    g->cd_platforms = tm_vary(g, g->zone_platforms ? SEC(g->zone >= 1 ? 30 : 0) : 0, SEC(g->zone >= 1 ? 10 : 0));
    return stack_pick(g, SEC_PLATFORMS, 0, 30, d);
  }
  if (g->want_tramplins) {
    g->want_tramplins = 0;
    g->cd_tramplins = tm_vary(g, SEC(30), SEC(10));
    return stack_pick(g, SEC_TRAMPLINS, 0, 30, d);
  }
  if (g->want_portals) {
    g->want_portals = 0;
    g->cd_portals = tm_vary(g, SEC(40), SEC(20));
    return stack_pick(g, SEC_PORTALS, 0, 30, d);
  }
  if (g->want_bonus && g->in_zone < g->zone_len - 3 && g->in_zone >= 4) {
    g->want_bonus = 0;
    return stack_pick(g, SEC_BONUS_ENTER, 0, 20, d);
  }
  int total = 0;
  for (int i = 0; i < 6; i++) total += g->chances[i];
  int r = tm_range(g, 0, total), slot = 0;
  while (slot < 5 && r >= g->chances[slot]) r -= g->chances[slot++];
  sec_pick p = stack_pick(g, g->slot_type[slot], 0, 30, d);
  p.no_enemies = tm_range(g, 0, 10) == 0;
  return p;
}

/* the rows from ring_top up to y (excluded) start empty */
static void grow_ring(tm_game *g, int32_t y) {
  while (g->ring_top < y) {
    uint8_t *row = g->tile + (g->ring_top & (TM_H - 1)) * TM_W;
    memset(row, 0, TM_W);
    memset(g->look + (g->ring_top & (TM_H - 1)) * TM_W, 0, TM_W);
    g->ring_top++;
  }
}

/* InvertSectionLines: mirrored, with the things that face sideways turned round */
static uint8_t mirror(uint8_t t) {
  switch (t) {
    case 68: return 69; case 69: return 68; case 73: return 74; case 74: return 73;
    case 76: return 77; case 77: return 76; case 78: return 79; case 79: return 78;
    case 56: return 57; case 57: return 56; case 113: return 115; case 115: return 113;
    default: return t;
  }
}

/* RemoveEnemies: cannons, spikes, trap spikes and fish become walls; bats go */
static uint8_t tame(uint8_t t) {
  if (t == T_CANNON_L || t == T_CANNON_R || t == T_CANNON_U || t == T_CANNON_D || t == T_SPIKES || t == T_MSPIKES ||
      t == T_FISH)
    return T_WALL;
  if (t == T_BAT_R || t == T_BAT_L || t == T_BAT_U || t == T_BAT_D) return T_EMPTY;
  return t;
}

/* GameController.AddGameSection and LevelLoader.AddTilesRoutine */
static void add_section(tm_game *g, sec_pick pk) {
  int id = g->order[tm_section_first[pk.type] + pk.index];
  const tm_map *m = &tm_sections[id];
  uint8_t raw[16 * 13], cells[16 * 13];
  np_lz_unpack(tm_maps + m->at, m->size, raw, sizeof raw);
  int w = m->w, h = m->h;
  for (int y = 0; y < h; y++)
    for (int x = 0; x < w; x++) {
      uint8_t t = raw[y * w + (pk.inverted ? w - 1 - x : x)];
      if (pk.inverted) t = mirror(t);
      if (pk.no_enemies) t = tame(t);
      cells[y * w + x] = t;
    }
  bool bonus = pk.type == SEC_BONUS_RUN || pk.type == SEC_BONUS_PIPES;
  bool pipes_walls = pk.type == SEC_BONUS_PIPES && g->bonus_len - g->bonus_count >= 1;
  /* ProcessPointsAndCoins */
  int32_t k = bonus ? g->bonus_coin_chance : g->coin_chance;
  int16_t spots[16 * 13];
  int ns = 0;
  for (int i = 0; i < w * h; i++) {
    if (cells[i] == T_DOT) {
      if (g->addict_left >= 1) cells[i] = T_COIN, g->addict_left--;
    } else if (cells[i] == T_COIN) {
      spots[ns++] = (int16_t)i;
      if (g->addict_left >= 1) g->addict_left--;
      else if (tm_value16(g) >= k) cells[i] = T_DOT;
    }
  }
  if (ns > 0 && g->pw_next != PW_NONE) {
    int at = spots[tm_range(g, 0, ns)];
    cells[at] = (uint8_t)(T_ADDICT + g->pw_next - 1);
    g->pw_next = PW_NONE;
    g->pw_spot = 1, g->pw_spot_y = g->build_y + at / w;
  }
  grow_ring(g, g->build_y + h);
  sect s = {cells, w, h, g->lane_x, g->build_y, (uint8_t)(pipes_walls ? 5 : g->style), 0, bonus, true};
  place(g, &s);
  g->build_y += h;
  /* the counters */
  if (g->bonus_active) {
    g->bonus_count++;
    if (g->bonus_count > g->bonus_len) {
      g->snake_mode = 0;
      g->bonus_active = 0;
      if (g->in_zone >= g->zone_len - 1) g->in_zone = (uint8_t)(g->zone_len - 2);
    }
  } else {
    g->in_zone++;
    if (g->in_zone >= g->zone_len + 20) g->in_zone = 0, g->zone_len = (uint8_t)(10 + 2 * g->zone);
  }
}

void tm_build_more(tm_game *g) {
  if (g->mode != MODE_ARCADE) return;
  if (g->build_y * TT - g->y < UNITS(2.4f)) add_section(g, next_section(g));
}

/* ------------------------------------------------------------------ zones (SetZoneIndex, ChangeZone) */
static const char *const zone_chances[7] = {"100000", "211000", "433000", "111100", "355502", NULL, NULL};
static const uint8_t zone_chances_big[2][6] = {{10, 23, 23, 24, 0, 20}, {5, 22, 22, 21, 0, 30}};

void tm_set_zone(tm_game *g, int z) {
  g->zone = (uint8_t)z;
  if (z < 5) {
    for (int i = 0; i < 6; i++) g->chances[i] = (uint8_t)(zone_chances[z][i] - '0');
  } else {
    for (int i = 0; i < 6; i++) g->chances[i] = zone_chances_big[z == 5 ? 0 : 1][i];
  }
  g->slot_type[0] = SEC_EASY, g->slot_type[5] = SEC_HARD;
  uint8_t pool[4] = {SEC_SPIKES, SEC_BATS, SEC_CANNONS, SEC_FISHES};
  for (int b = 0; b < 4; b++) {
    int e = tm_range(g, b, 4);
    g->slot_type[b + 1] = pool[e];
    pool[e] = pool[b];
  }
  uint8_t on[3] = {1, 1, 1};   /* monkeys, platforms, snakes */
  if (z <= 1) {
    on[0] = on[1] = on[2] = 0;
    on[tm_range(g, 0, 3)] = 1;
  }
  static const uint8_t monkey_cd[4] = {0, 20, 10, 5}, monkey_var[4] = {0, 10, 10, 10};
  int zi = z > 3 ? 3 : z, zs = z > 1 ? 1 : z;
  g->zone_monkeys = on[0], g->zone_platforms = on[1], g->zone_snakes = on[2];
  if (g->cd_monkeys <= 0 && on[0]) g->cd_monkeys = tm_vary(g, SEC(monkey_cd[zi]), SEC(monkey_var[zi]));
  if (g->cd_platforms <= 0 && on[1]) g->cd_platforms = tm_vary(g, SEC(zs ? 30 : 0), SEC(zs ? 10 : 0));
  if (g->cd_snakes <= 0 && on[2]) g->cd_snakes = tm_vary(g, SEC(zs ? 30 : 0), SEC(zs ? 10 : 0));
  if (g->cd_tramplins == 0) g->cd_tramplins = tm_vary(g, SEC(30), SEC(10));
  if (g->cd_portals == 0) g->cd_portals = tm_vary(g, SEC(40), SEC(20));
  if (z + 1 > g->p.max_zone) g->p.max_zone = (uint8_t)(z + 1 > 255 ? 255 : z + 1);
}

/* ChangeActiveColorSet: a pair from the first two thirds of the list, moved to its end */
void tm_change_colors(tm_game *g) {
  int f = tm_range(g, 1, 8);           /* (int)(13 * 0.66) */
  uint8_t c = g->color_order[f];
  memmove(g->color_order + f, g->color_order + f + 1, (size_t)(12 - f));
  g->color_order[12] = c;
  g->colors = c;
}

void tm_change_zone(tm_game *g) {
  tm_remove_monkeys(g);
  tm_remove_snakes(g);
  g->snake_mode = 0;
  g->zone_changed = 1;
  tm_set_zone(g, g->zone + 1);
  int c = tm_range(g, 0, 3);
  uint8_t s = g->styles[c];
  g->styles[c] = g->styles[3];
  g->styles[3] = s;
  g->style = s;
  g->color_wait = 18;                  /* ChangeActiveColorSet, 0.3 s later */
  g->ev_zone = 1;
}

/* AddExtraBricks: bricks in a row (style 1, drawn on their own) */
void tm_close_bricks(tm_game *g, int x, int y, int n) {
  for (int i = 0; i < n; i++) {
    int idx = tm_index(g, x + i, y);
    if (idx < 0) continue;
    g->tile[idx] = T_BRICK;
    g->look[idx] = 0;
    tm_fx(g, FX_BRICK, (x + i) * TT, y * TT, 0, 0);
  }
}

/* CreateBonusZone: the other lane, through the door's hall */
void tm_bonus_zone(tm_game *g, int door_x, int door_y) {
  if (g->bonus) return;
  g->snake_mode = 0;
  tm_remove_monkeys(g);
  tm_remove_snakes(g);
  g->zone_changed = 1;
  bool right = door_x * TT > g->cam_x;
  int s = right ? 1 : -1;
  int type = tm_range(g, 0, 2) == 0 ? BONUS_RUN : BONUS_PIPES;
  g->cam_slide = 1, g->cam_slide_speed = UNITS(1.0f), g->cam_lead = 0;
  g->cam_target_x = g->cam_x + s * (TM_LANE + TM_GAP) * TT;
  g->lane_x = (int16_t)(g->lane_x + s * (TM_LANE + TM_GAP));
  g->lane = (uint8_t)(g->lane ^ 1);
  g->bonus = (uint8_t)type;
  g->build_y = door_y - (type == BONUS_RUN ? 3 : 6);
  g->lane_base[g->lane] = g->build_y;
  /* the lane starts clean */
  for (int y = g->ring_top - TM_H; y < g->ring_top; y++)
    for (int x = 0; x < TM_LANE; x++) {
      int i = tm_index(g, g->lane_x + x, y);
      if (i >= 0) g->tile[i] = T_EMPTY, g->look[i] = 0;
    }
  if (type == BONUS_RUN) {
    g->running = 1;
    g->pstate = ST_LONG;
  }
  g->lava_on = 0, g->lava_back = 1;
  g->bonus_active = 1, g->bonus_count = 0, g->bonus_len = type == BONUS_RUN ? 30 : 6;
  sec_pick p = next_section(g);
  p.inverted = !right;
  add_section(g, p);
}

/* the bonus zone's exit: close it behind (CloseZoneWithBricks) and climb on from there (DeactivateBonusZone) */
void tm_end_bonus_zone(tm_game *g, int exit_x, int exit_y) {
  int cx = tm_cell(g->cam_x);
  if (g->bonus == BONUS_RUN) tm_close_bricks(g, cx - 1, exit_y, 3);
  else tm_close_bricks(g, cx, exit_y, 1);
  (void)exit_x;
  g->bonus = BONUS_NONE;
  g->running = 0;
  tm_lava_activate(g, true);
  if (g->bonus_left) g->bonus_left--;
  if (g->bonus_left > 0) g->cd_bonus = tm_vary(g, g->bonus_cool, g->bonus_cool_var);
}

/* the arcade's beginning: three empty rooms, then the zone's first sections (GenerateFirstSections) */
void tm_build_arcade_start(tm_game *g) {
  for (int t = 0; t < 15; t++)
    for (int i = 0; i < tm_section_count[t]; i++) g->order[tm_section_first[t] + i] = (uint16_t)(tm_section_first[t] + i);
  g->lane = 0, g->lane_x = 0;
  g->ring_top = -24;
  g->lane_base[0] = g->lane_base[1] = -24;
  g->build_y = -24;
  for (int i = 0; i < 3; i++) add_section(g, fixed(SEC_BEGIN, 0, false));
  g->in_zone = 0;
  for (int i = 0; i < 7 - 3; i++) add_section(g, next_section(g));
}
