/* Hotland's shooting puzzles (the Reactivated Door Puzzles), on their own screen (MODE_SHOOT).
 *
 * A grid of blocks stands between Frisk's ship, below, and the target, above. Each arrow pushes every dark block one
 * cell that way, where there's room inside the grid; the light blocks never move. Z shoots straight up: a shot
 * breaks the first dark block it meets, a light one stops it, and a clear way hits the target. Two shots a try on
 * the first floor, one on the third: out of them, it starts over. (The rules are worked out from the solutions the Undertale Wiki gives for them; the
 * grids are the game's, as its screenshots show them.) */
#include <string.h>
#include "game.h"
#include "gfx.h"
#include "plat.h"
#include "res.h"
#include "shoot.h"

typedef struct {
  uint8_t w, h, ship, shots;
  const char *rows[6];   /* '#' a light block, 'o' a dark one, ' ' nothing */
} Puzzle;
static const Puzzle puzzles[SHOOT_COUNT] = {
  [SHOOT_RIGHT] = {3, 3, 1, 2, {"#o#", "   ", "#o#"}},
  [SHOOT_LEFT] = {4, 4, 2, 2, {"#  #", "ooo ", " #o#", "#oo "}},
  [SHOOT_NORTH] = {5, 5, 2, 1, {"## o#", " o # ", " #o o", " o o#", "## ##"}},
  [SHOOT_SOUTH] = {5, 4, 3, 1, {"###o ", "   o#", "###o ", "###o#"}},
};
#define CELL 24

static int id, shots, result, t, shot_y, fail_t;
static char grid[6][6];
static bool won;

static void reset(void) {
  const Puzzle *p = &puzzles[id];
  for (int y = 0; y < p->h; y++)
    for (int x = 0; x < p->w; x++) grid[y][x] = p->rows[y][x];
  shots = p->shots, shot_y = -1, fail_t = 0, won = false;
}

void shoot_open(int which) {
  id = which, result = 0, t = 0;
  reset();
  g_mode = MODE_SHOOT;
}
int shoot_result(void) { return result; }

static void push(int dx, int dy) {
  const Puzzle *p = &puzzles[id];
  /* one cell a press; the dark blocks nearest the way they go move first, so a line of them moves together */
  for (int y = 0; y < p->h; y++)
    for (int x = 0; x < p->w; x++) {
      int sx = dx > 0 ? p->w - 1 - x : x, sy = dy > 0 ? p->h - 1 - y : y;
      if (grid[sy][sx] != 'o') continue;
      int nx = sx + dx, ny = sy + dy;
      if (nx < 0 || ny < 0 || nx >= p->w || ny >= p->h || grid[ny][nx] != ' ') continue;
      grid[ny][nx] = 'm', grid[sy][sx] = ' ';   /* (marked: moved once) */
    }
  for (int y = 0; y < p->h; y++)
    for (int x = 0; x < p->w; x++)
      if (grid[y][x] == 'm') grid[y][x] = 'o';
}

void shoot_update(uint32_t keys, uint32_t pressed) {
  (void)keys;
  const Puzzle *p = &puzzles[id];
  t++;
  if (won) {
    if (++fail_t > 40) result = 1, g_mode = MODE_WORLD;
    return;
  }
  if (fail_t) {
    if (++fail_t > 45) reset();
    return;
  }
  if (shot_y >= 0) {
    /* the shot flies up its column, a cell every few frames */
    if (t % 3) return;
    shot_y--;
    if (shot_y < 0) {
      won = true, fail_t = 1;
      return;
    }
    char *c = &grid[shot_y][p->ship];
    if (*c == ' ') return;
    if (*c == 'o') *c = ' ';
    shot_y = -1;
    if (!shots) fail_t = 1;
    return;
  }
  if (pressed & K_X) {
    result = 0, g_mode = MODE_WORLD;
    return;
  }
  if (pressed & K_UP) push(0, -1);
  else if (pressed & K_DOWN) push(0, 1);
  else if (pressed & K_LEFT) push(-1, 0);
  else if (pressed & K_RIGHT) push(1, 0);
  else if ((pressed & K_Z) && shots) shots--, shot_y = p->h, t = 0;
}

void shoot_draw(void) {
  const Puzzle *p = &puzzles[id];
  gfx_rect(0, 0, SCREEN_W, SCREEN_H, C_BLACK, 255);
  int gw = p->w * CELL, gh = p->h * CELL;
  int gx = (SCREEN_W - gw) / 2, gy = 104 - gh / 2;
  uint16_t grey = rgb565(0x909090), dim = rgb565(0x606060);
  /* the frame, the target, the ship */
  gfx_rect(gx - 40, 16, gw + 80, 1, grey, 255);
  gfx_rect(gx - 40, 190, gw + 80, 1, grey, 255);
  gfx_rect(gx - 40, 16, 1, 175, grey, 255);
  gfx_rect(gx + gw + 39, 16, 1, 175, grey, 255);
  int cx = gx + p->ship * CELL + CELL / 2;
  if (!won || (t / 3) % 2)
    for (int k = 0; k < 8; k++) gfx_rect(cx - 8 + k, 22 + k, 16 - 2 * k, 1, grey, 255);
  for (int k = 0; k < 9; k++) gfx_rect(cx - k, 172 + k, 2 * k + 1, 1, rgb565(0xA0A030), 255);
  /* the grid */
  gfx_rect(gx - 2, gy - 2, gw + 4, 1, dim, 255);
  gfx_rect(gx - 2, gy + gh + 1, gw + 4, 1, dim, 255);
  gfx_rect(gx - 2, gy - 2, 1, gh + 4, dim, 255);
  gfx_rect(gx + gw + 1, gy - 2, 1, gh + 4, dim, 255);
  for (int y = 0; y < p->h; y++)
    for (int x = 0; x < p->w; x++) {
      int bx = gx + x * CELL + 2, by = gy + y * CELL + 2;
      if (grid[y][x] == '#') {
        gfx_rect(bx, by, CELL - 4, CELL - 4, grey, 255);
      } else if (grid[y][x] == 'o') {
        gfx_rect(bx, by, CELL - 4, CELL - 4, grey, 255);
        gfx_rect(bx + 2, by + 2, CELL - 8, CELL - 8, C_BLACK, 255);
      }
    }
  if (shot_y >= 0) gfx_rect(cx - 1, gy + shot_y * CELL + CELL / 2 - 4, 3, 8, C_YELLOW, 255);
  /* the shots left, and how it plays */
  for (int k = 0; k < shots; k++) gfx_rect(gx + gw + 50 + k * 8, 176, 4, 8, C_YELLOW, 255);
  if (fail_t && !won) gfx_text(FONT_MAIN, 120, 200, "RESTART...", C_WHITE, 255);
  else if (won) gfx_text(FONT_MAIN, 128, 200, "SUCCESS!", C_YELLOW, 255);
  else gfx_text(FONT_SMALL, 40, 204, "ARROWS: MOVE BLOCKS   Z: SHOOT   X: LEAVE", dim, 255);
}
