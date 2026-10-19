/* A room's grid: rocks, poop, pits, spikes and fire; walking around them, blowing them up, and the doors. */
#include <string.h>
#include "game.h"
#include "gfx.h"
#include "res.h"

#define GRID_RUBBLE 100   /* (a rock blown up: rubble on the floor) */

static uint8_t g(int c, int r) {
  if (c < 0 || r < 0 || c >= COLS || r >= ROWS) return CELL_BLOCK;
  return room_cur()->grid[r][c];
}
static uint8_t ghp(int c, int r) { return room_cur()->gridhp[r][c]; }

static bool is_poop(int k) { return k == CELL_POOP || k == CELL_CORNY_POOP || k == CELL_RED_POOP || k == CELL_GOLDEN_POOP; }

bool grid_solid(int c, int r, bool flying) {
  int k = g(c, r);
  switch (k) {
    case CELL_ROCK: case CELL_BLOCK: case CELL_TINTED: case CELL_BOMB_ROCK: case CELL_TNT: case CELL_KEY_BLOCK:
      return !flying || k == CELL_BLOCK || k == CELL_KEY_BLOCK;
    case CELL_PIT: return !flying;
    case CELL_FIRE: case CELL_RED_FIRE: return ghp(c, r) > 0 && !flying;
    default: return is_poop(k) && ghp(c, r) > 0 && !flying;
  }
}

bool grid_blocks_tears(int c, int r) {
  int k = g(c, r);
  switch (k) {
    case CELL_ROCK: case CELL_BLOCK: case CELL_TINTED: case CELL_BOMB_ROCK: case CELL_TNT: case CELL_KEY_BLOCK: return true;
    case CELL_FIRE: case CELL_RED_FIRE: return ghp(c, r) > 0;
    default: return is_poop(k) && ghp(c, r) > 0;
  }
}

bool grid_pit(int c, int r) { return g(c, r) == CELL_PIT; }

static int col_of(float x) { return (int)((x - ROOM_X) / TILE + 100) - 100; }
static int row_of(float y) { return (int)((y - ROOM_Y) / TILE + 100) - 100; }

bool solid_at(float x, float y, bool flying) {
  if (x < ROOM_X || x >= ROOM_R || y < ROOM_Y || y >= ROOM_B) return true;
  return grid_solid(col_of(x), row_of(y), flying);
}

int grid_spikes_at(float x, float y) {
  int k = g(col_of(x), row_of(y));
  return k == CELL_SPIKES;
}

/* moves a body of radius r by its speed, sliding along the walls and the grid (as a square: simple and fair) */
void move_ent(float *x, float *y, float *vx, float *vy, float r, bool flying, bool nogrid) {
  float nx = *x + *vx;
  float lo = ROOM_X + r, hi = ROOM_R - r;
  if (nx < lo) nx = lo, *vx = 0;
  if (nx > hi) nx = hi, *vx = 0;
  if (!nogrid) {
    int r0 = row_of(*y - r + 0.5f), r1 = row_of(*y + r - 0.5f);
    if (*vx > 0) {
      int c = col_of(nx + r);
      for (int rr = r0; rr <= r1; rr++)
        if (grid_solid(c, rr, flying)) {
          nx = ROOM_X + c * TILE - r - 0.01f, *vx = 0;
          break;
        }
    } else if (*vx < 0) {
      int c = col_of(nx - r);
      for (int rr = r0; rr <= r1; rr++)
        if (grid_solid(c, rr, flying)) {
          nx = ROOM_X + (c + 1) * TILE + r + 0.01f, *vx = 0;
          break;
        }
    }
  }
  *x = nx;
  float ny = *y + *vy;
  lo = ROOM_Y + r, hi = ROOM_B - r;
  if (ny < lo) ny = lo, *vy = 0;
  if (ny > hi) ny = hi, *vy = 0;
  if (!nogrid) {
    int c0 = col_of(*x - r + 0.5f), c1 = col_of(*x + r - 0.5f);
    if (*vy > 0) {
      int rr = row_of(ny + r);
      for (int c = c0; c <= c1; c++)
        if (grid_solid(c, rr, flying)) {
          ny = ROOM_Y + rr * TILE - r - 0.01f, *vy = 0;
          break;
        }
    } else if (*vy < 0) {
      int rr = row_of(ny - r);
      for (int c = c0; c <= c1; c++)
        if (grid_solid(c, rr, flying)) {
          ny = ROOM_Y + (rr + 1) * TILE + r + 0.01f, *vy = 0;
          break;
        }
    }
  }
  *y = ny;
}

bool grid_line_clear(float x0, float y0, float x1, float y1) {
  float dx = x1 - x0, dy = y1 - y0, d = fsqrt(dx * dx + dy * dy);
  int n = (int)(d / 6) + 1;
  for (int i = 1; i < n; i++) {
    float x = x0 + dx * i / n, y = y0 + dy * i / n;
    if (grid_blocks_tears(col_of(x), row_of(y)) || grid_pit(col_of(x), row_of(y))) return false;
  }
  return true;
}

/* ---------------------------------------------------------------- the way to the player (a flow field) */
static uint8_t flow[ROWS][COLS];
static int flow_tc = -1, flow_tr = -1;
static uint32_t flow_frame = 0xFFFFFFFF;

static void flow_build(int tc, int tr) {
  memset(flow, 255, sizeof flow);
  if (tc < 0 || tr < 0 || tc >= COLS || tr >= ROWS) return;
  static uint8_t qc[ROWS * COLS], qr[ROWS * COLS];
  int h = 0, t = 0;
  flow[tr][tc] = 0, qc[t] = (uint8_t)tc, qr[t++] = (uint8_t)tr;
  while (h < t) {
    int c = qc[h], r = qr[h++];
    for (int d = 0; d < 4; d++) {
      int nc = c + DIR_X[d], nr = r + DIR_Y[d];
      if (nc < 0 || nr < 0 || nc >= COLS || nr >= ROWS || flow[nr][nc] != 255 || grid_solid(nc, nr, false)) continue;
      flow[nr][nc] = (uint8_t)(flow[r][c] + 1);
      qc[t] = (uint8_t)nc, qr[t++] = (uint8_t)nr;
    }
  }
}

/* the way a walker goes from (x, y) to (tx, ty): straight when nothing is in the way, else along the floor's
 * shortest path; returns 0 when there is no way */
int path_dir(float x, float y, float tx, float ty, bool flying, float *dx, float *dy) {
  float ex = tx - x, ey = ty - y, d = fsqrt(ex * ex + ey * ey);
  if (d < 0.01f) {
    *dx = *dy = 0;
    return 0;
  }
  if (flying || grid_line_clear(x, y, tx, ty)) {
    *dx = ex / d, *dy = ey / d;
    return 1;
  }
  int tc = col_of(tx), tr = row_of(ty);
  if (flow_frame != g_frame || tc != flow_tc || tr != flow_tr) {
    flow_build(tc, tr);
    flow_frame = g_frame, flow_tc = tc, flow_tr = tr;
  }
  int c = col_of(x), r = row_of(y), best = 255, bc = c, br = r;
  if (c < 0 || r < 0 || c >= COLS || r >= ROWS) {
    *dx = ex / d, *dy = ey / d;
    return 1;
  }
  for (int k = 0; k < 4; k++) {
    int nc = c + DIR_X[k], nr = r + DIR_Y[k];
    if (nc < 0 || nr < 0 || nc >= COLS || nr >= ROWS) continue;
    if (flow[nr][nc] < best) best = flow[nr][nc], bc = nc, br = nr;
  }
  if (best == 255) {
    *dx = ex / d, *dy = ey / d;
    return 0;
  }
  float gx = ROOM_X + bc * TILE + TILE / 2 - x, gy = ROOM_Y + br * TILE + TILE / 2 - y, gd = fsqrt(gx * gx + gy * gy);
  if (gd < 0.01f) gd = 1;
  *dx = gx / gd, *dy = gy / gd;
  return 1;
}

/* ---------------------------------------------------------------- breaking things */
static void cell_center(int c, int r, float *x, float *y) { *x = ROOM_X + c * TILE + TILE / 2, *y = ROOM_Y + r * TILE + TILE / 2; }

static void destroy_cell(int c, int r) {
  Room *rm = room_cur();
  int k = rm->grid[r][c];
  float x, y;
  cell_center(c, r, &x, &y);
  switch (k) {
    case CELL_ROCK: case CELL_BOMB_ROCK:
      rm->grid[r][c] = GRID_RUBBLE;
      fx_spawn(FX_POOF, x, y, 10);
      if (k == CELL_ROCK && rnd(30) == 0) pickup_drop_random(x, y);
      break;
    case CELL_TINTED: {
      rm->grid[r][c] = GRID_RUBBLE;
      fx_spawn(FX_POOF, x, y, 10);
      /* Rebirth's: a soul heart, bombs, keys or a chest; The Small Rock now and then */
      int roll = rnd(100);
      if (roll < 40) pickup_spawn(PK_SOUL, 0, x, y, true);
      else if (roll < 60) pickup_spawn(PK_BOMB, 0, x, y, true), pickup_spawn(PK_BOMB, 0, x, y, true);
      else if (roll < 75) pickup_spawn(PK_KEY, 0, x, y, true);
      else if (roll < 90) pickup_spawn(PK_CHEST, 0, x, y, true);
      else if (!pl.has[90]) pedestal_spawn(90, x, y, 0, false);
      else pickup_spawn(PK_SOUL, 0, x, y, true);
      break;
    }
    case CELL_TNT:
      rm->grid[r][c] = GRID_RUBBLE;
      explode(x, y, 30, 10, true, 0);
      break;
    case CELL_KEY_BLOCK: case CELL_COBWEB:
      rm->grid[r][c] = CELL_EMPTY;
      break;
    default:
      if (is_poop(k) && rm->gridhp[r][c]) {
        rm->gridhp[r][c] = 0;
        fx_spawn(FX_POOF, x, y, 8);
        if (k == CELL_GOLDEN_POOP) for (int i = 0; i < 3; i++) pickup_spawn(PK_PENNY, 0, x, y, true);
        else if (rnd(10) == 0) pickup_drop_random(x, y);
        if (k == CELL_CORNY_POOP && rnd(3) == 0) mon_spawn(MON_FLY, x, y);
      }
      if ((k == CELL_FIRE || k == CELL_RED_FIRE) && rm->gridhp[r][c]) {
        rm->gridhp[r][c] = 0;
        fx_spawn(FX_POOF, x, y - 4, 8);
        if (rnd(4) == 0) pickup_drop_random(x, y);
      }
      break;
  }
}

void grid_hit(int c, int r, float dmg) {
  if (c < 0 || r < 0 || c >= COLS || r >= ROWS) return;
  Room *rm = room_cur();
  int k = rm->grid[r][c];
  if (is_poop(k) || k == CELL_FIRE || k == CELL_RED_FIRE) {
    if (!rm->gridhp[r][c]) return;
    /* a hit or more (big tears take more) */
    int hits = dmg >= 10 ? 2 : 1;
    if (rm->gridhp[r][c] <= hits) destroy_cell(c, r);
    else rm->gridhp[r][c] = (uint8_t)(rm->gridhp[r][c] - hits);
  } else if (k == CELL_TNT) {
    if (++rm->gridhp[r][c] >= 4) destroy_cell(c, r);
  }
}

void door_bombed(float x, float y);
void grid_explode(float x, float y, float radius) {
  for (int r = 0; r < ROWS; r++)
    for (int c = 0; c < COLS; c++) {
      float cx, cy;
      cell_center(c, r, &cx, &cy);
      float dx = cx - x, dy = cy - y;
      if (dx * dx + dy * dy > (radius + 10) * (radius + 10)) continue;
      int k = g(c, r);
      if (k == CELL_ROCK || k == CELL_TINTED || k == CELL_BOMB_ROCK || k == CELL_TNT || is_poop(k) || k == CELL_FIRE
          || k == CELL_RED_FIRE || k == CELL_COBWEB)
        destroy_cell(c, r);
      if (k == CELL_ROCK && pl.has[147]) destroy_cell(c, r);
    }
  door_bombed(x, y);
}

/* ---------------------------------------------------------------- drawing */
static int rock_sprite(void) {
  Room *r = room_cur();
  if (r->kind == ROOM_SECRET || r->kind == ROOM_SUPERSECRET) return SPR_ROCKS_SECRETROOM;
  switch (chapter_of(fl.stage)) {
    case 1: return fl.alt ? SPR_ROCKS_CELLAR : SPR_ROCKS_GENERIC;
    case 2: return fl.alt ? SPR_ROCKS_CATACOMBS : SPR_ROCKS_CAVES;
    case 3: return SPR_ROCKS_DEPTHS;
    case 4: return fl.alt ? SPR_ROCKS_SCARREDWOMB : SPR_ROCKS_WOMB;
    default: return fl.alt ? SPR_ROCKS_CATHEDRAL : SPR_ROCKS_SHEOL;
  }
}

/* the flat things (pits, spikes, rubble, webs), under everything */
void room_draw_floor(void) {
  gfx_backdrop(room_backdrop());
  Room *rm = room_cur();
  int rs = rock_sprite();
  for (int r = 0; r < ROWS; r++)
    for (int c = 0; c < COLS; c++) {
      int k = rm->grid[r][c];
      float x, y;
      cell_center(c, r, &x, &y);
      switch (k) {
        case CELL_PIT: gfx_sprite(SPR_PIT, 0, (int)x, (int)y, 0, 32); break;
        case CELL_SPIKES: gfx_sprite(SPR_SPIKES, 0, (int)x, (int)y, 0, 32); break;
        case CELL_COBWEB: gfx_sprite(SPR_COBWEB, 0, (int)x, (int)y, 0, 28); break;
        case GRID_RUBBLE: gfx_sprite(rs, 7, (int)x, (int)y, 0, 32); break;
        default: break;
      }
    }
}

/* the standing things, each drawn in its row (by ents_draw) */
void grid_draw_row(int r) {
  Room *rm = room_cur();
  int rs = rock_sprite();
  for (int c = 0; c < COLS; c++) {
    int k = rm->grid[r][c], hp = rm->gridhp[r][c];
    float x, y;
    cell_center(c, r, &x, &y);
    int ix = (int)x, iy = (int)y;
    switch (k) {
      case CELL_ROCK: gfx_sprite(rs, (c * 7 + r * 13) % 3, ix, iy - 2, 0, 32); break;
      case CELL_BLOCK: gfx_sprite(rs, 3, ix, iy - 2, 0, 32); break;
      case CELL_TINTED: gfx_sprite(rs, 4, ix, iy - 2, 0, 32); break;
      case CELL_BOMB_ROCK: gfx_sprite(rs, 5, ix, iy - 2, 0, 32); break;
      case CELL_TNT: gfx_sprite(SPR_TNT, 0, ix, iy + 9, 0, 32); break;
      case CELL_KEY_BLOCK: gfx_sprite(SPR_KEY_BLOCK, 0, ix, iy + 9, 0, 32); break;
      case CELL_FIRE: case CELL_RED_FIRE: {
        int spr = k == CELL_FIRE ? SPR_FIREPLACE : SPR_RED_FIREPLACE;
        if (hp) {
          int sy = 256 + ((int)((g_frame + c * 5) % 6) - 3) * 8 - (4 - hp) * 30;
          gfx_sprite_scaled(spr, 0, ix, iy + 9, 256, sy, 0, 0, 0, 32);
        } else
          gfx_sprite_scaled(spr, 0, ix, iy + 9, 256, 90, 0, C_BLACK, 20, 32);
        break;
      }
      default:
        if (is_poop(k) && hp) {
          int spr = k == CELL_POOP ? SPR_POOP : k == CELL_CORNY_POOP ? SPR_CORNY_POOP : k == CELL_RED_POOP ? SPR_RED_POOP : SPR_GOLDEN_POOP;
          if (spr == SPR_POOP) gfx_sprite(spr, 4 - hp, ix, iy + 9, 0, 32);
          else gfx_sprite_scaled(spr, 0, ix, iy + 9, 256, 256 * hp / 4, 0, 0, 0, 32);
        }
        break;
    }
  }
}

/* ---------------------------------------------------------------- doors */
static const int door_x[4] = {160, ROOM_X, 160, ROOM_R}, door_y[4] = {ROOM_B, 120, ROOM_Y, 120};

static int door_frame_kind(int kind, bool here_kind_special, int here) {
  (void)here_kind_special;
  int k = kind;
  if (here == ROOM_BOSS || here == ROOM_TREASURE || here == ROOM_SHOP || here == ROOM_CURSE || here == ROOM_SECRET
      || here == ROOM_SUPERSECRET || here == ROOM_ARCADE || here == ROOM_DEVIL || here == ROOM_ANGEL)
    if (kind == ROOM_NORMAL || kind == ROOM_START) k = here;
  switch (k) {
    case ROOM_TREASURE: return 1;
    case ROOM_BOSS: return 2;
    case ROOM_SECRET: case ROOM_SUPERSECRET: return 3;
    case ROOM_SHOP: case ROOM_LIBRARY: return 4;
    case ROOM_CURSE: return 5;
    case ROOM_DEVIL: return 6;
    case ROOM_ANGEL: return 7;
    case ROOM_ARCADE: return 8;
    default: return 0;
  }
}

void doors_draw(void) {
  int here = room_cur()->kind;
  for (int d = 0; d < 4; d++) {
    int kind;
    if (!room_door_exists(d, &kind)) continue;
    int fk = door_frame_kind(kind, false, here);
    bool open = room_door_open(d);
    int f = fk * 2 + (open ? 0 : 1);
    if (fk == 3) f = 6;   /* (a hole stays a hole) */
    if (d == DIR_UP) gfx_sprite(SPR_DOOR, f, door_x[2], door_y[2], 0, 32);
    else if (d == DIR_DOWN) gfx_sprite(SPR_DOOR, f, door_x[0], door_y[0] + 30, GFX_FLIPY, 32);
    else {
      int spr = SPR_DOOR_SIDE_NORMAL + fk;
      if (d == DIR_LEFT) gfx_sprite(spr, open ? 0 : 1, door_x[1], door_y[1], 0, 32);
      else gfx_sprite(spr, open ? 0 : 1, door_x[3], door_y[3], GFX_FLIPX, 32);
    }
  }
}
