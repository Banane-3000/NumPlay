/* The buildings' rows (the middle of the game's screen), shown with alpha in place of the store:
 * each building owned on its background, as many of its pictures as fit, set the game's way (rows,
 * random shifts, grandmas of every type they have become); its level; and the minigames. */
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "ui.h"

#define ROW_H 48
#define ROW_GAP 4
#define ROWS_X STORE_X
#define ROWS_W STORE_W
#define SC 0.375f
extern bool can_lumps(void);
extern void draw_minigame(int b);
extern void minigame_keys(int b, uint32_t pressed);
extern bool minigame_available(int b);
extern const char *minigame_name(int b);
extern bool level_up(int b);

/* the art of each building's row: random shifts, spacing, rows, offsets, frames (the game's) */
typedef struct { int8_t xv, yv, w, rows, x, y, frames; } Art;
static const Art arts[NB] = {
  {0, 0, 0, 0, 0, 0, 0},      {8, 8, 32, 3, 0, 16, 1},    {8, 8, 64, 2, 0, 16, 1},   {16, 16, 64, 2, 0, 24, 1},
  {8, 0, 64, 1, 0, -22, 1},   {8, 4, 56, 1, 0, 13, 1},    {8, 4, 72, 2, 0, -5, 1},   {16, 16, 48, 2, 0, 20, 1},
  {16, 16, 64, 1, 0, 0, 1},   {16, 16, 64, 2, 0, 16, 1},  {32, 32, 64, 2, 0, 0, 1},  {32, 32, 64, 1, 0, 0, 1},
  {0, 64, 64, 1, 0, 0, 1},    {16, 4, 64, 1, 0, 20, 1},   {8, 64, 64, 2, 0, 0, 1},   {8, 64, 64, 1, 0, 0, 1},
  {8, 64, 14, 1, 8, -32, 2},  {8, 96, 48, 2, 0, 0, 4},    {8, 96, 48, 1, 0, 0, 4},   {0, 0, 64, 2, 0, 0, 1},
};
static const int16_t row_bg[NB] = {-1, IM_ROW_BG_GRANDMA, IM_ROW_BG_FARM, IM_ROW_BG_MINE, IM_ROW_BG_FACTORY,
                                   IM_ROW_BG_BANK, IM_ROW_BG_TEMPLE, IM_ROW_BG_WIZARDTOWER, IM_ROW_BG_SHIPMENT,
                                   IM_ROW_BG_ALCHEMYLAB, IM_ROW_BG_PORTAL, IM_ROW_BG_TIMEMACHINE,
                                   IM_ROW_BG_ANTIMATTERCONDENSER, IM_ROW_BG_PRISM, IM_ROW_BG_CHANCEMAKER,
                                   IM_ROW_BG_FRACTALENGINE, IM_ROW_BG_JAVASCRIPTCONSOLE, IM_ROW_BG_IDLEVERSE,
                                   IM_ROW_BG_CORTEX, IM_ROW_BG_YOU};
static const int16_t row_pic[NB] = {-1, IM_ROW_GRANDMA, IM_ROW_FARM, IM_ROW_MINE, IM_ROW_FACTORY, IM_ROW_BANK,
                                    IM_ROW_TEMPLE, IM_ROW_WIZARDTOWER, IM_ROW_SHIPMENT, IM_ROW_ALCHEMYLAB,
                                    IM_ROW_PORTAL, IM_ROW_TIMEMACHINE, IM_ROW_ANTIMATTERCONDENSER, IM_ROW_PRISM,
                                    IM_ROW_CHANCEMAKER, IM_ROW_FRACTALENGINE, IM_ROW_JAVASCRIPTCONSOLE0,
                                    IM_ROW_IDLEVERSE0, IM_ROW_CORTEX0, IM_ROW_YOU};
/* the grandmas' types, in the order of their pictures (IM_ROW_FARMERGRANDMA...) */
static const uint16_t grandma_types[18] = {
  U_FARMER_GRANDMAS, U_WORKER_GRANDMAS, U_MINER_GRANDMAS, U_COSMIC_GRANDMAS, U_TRANSMUTED_GRANDMAS,
  U_ALTERED_GRANDMAS, U_GRANDMAS_GRANDMAS, U_ANTIGRANDMAS, U_RAINBOW_GRANDMAS, U_BANKER_GRANDMAS,
  U_PRIESTESS_GRANDMAS, U_WITCH_GRANDMAS, U_LUCKY_GRANDMAS, U_METAGRANDMAS, U_SCRIPT_GRANNIES,
  U_ALTERNATE_GRANDMAS, U_BRAINY_GRANDMAS, U_CLONE_GRANDMAS};
static const char *const grandma_names[] = {
  "Granny", "Gusher", "Ethel", "Edna", "Doris", "Maud", "Hilda", "Gladys", "Michelle", "Michele", "Phyllis",
  "Millicent", "Muriel", "Myrtle", "Mildred", "Mavis", "Helen", "Gloria", "Sheila", "Betty", "Gertrude", "Agatha",
  "Beryl", "Agnes", "Pearl", "Precious", "Ruby", "Vera", "Bonnie", "Ada", "Bunny", "Cookie", "Darling", "Gaga",
  "GamGam", "Memaw", "Mimsy", "Peanut", "Nana", "Nan", "Tootsie", "Warty", "Stinky", "Heinous"};

/* the same numbers each time for the same picture (the game seeds its random with it) */
static uint32_t hash3(uint32_t a, uint32_t b, uint32_t c) {
  uint32_t h = a * 0x9E3779B1u ^ b * 0x85EBCA77u ^ c * 0xC2B2AE3Du;
  h ^= h >> 15, h *= 0x2C1B3C6Du, h ^= h >> 12, h *= 0x297A2D39u, h ^= h >> 15;
  return h;
}
static float hrand(uint32_t *h) {
  *h = *h * 1664525u + 1013904223u;
  return (float)(*h >> 8) / 16777216.f;
}

/* the rows shown: buildings owned past the cursor */
static int rows[NB], nrows;
static void rows_list(void) {
  nrows = 0;
  for (int b = 1; b < NB; b++)
    if (S.buildings[b].amount > 0) rows[nrows++] = b;
  if (U.rows_sel >= nrows) U.rows_sel = nrows ? nrows - 1 : 0;
}

static int grandma_pic(uint32_t *h) {
  int list[21], n = 0;
  list[n++] = IM_ROW_GRANDMA;
  for (int i = 0; i < 18; i++)
    if (has(grandma_types[i])) list[n++] = IM_ROW_FARMERGRANDMA + i;
  if (S.season == SEASON_CHRISTMAS) list[n++] = IM_ROW_ELFGRANDMA;
  if (S.season == SEASON_EASTER) list[n++] = IM_ROW_BUNNYGRANDMA;
  return list[(int)(hrand(h) * n) % n];
}

static void draw_row(int b, int y, bool sel) {
  const Art *a = &arts[b];
  gfx_clip(ROWS_X, y, ROWS_W, ROW_H);
  gfx_pattern(row_bg[b], ROWS_X, y, ROWS_W, ROW_H, ROW_H, ROW_H, ROWS_X, y, 256);
  /* the pictures: at the game's places (in a 128-pixel row), drawn at 0.375 */
  int w = a->w ? a->w : 48, nrow = a->rows ? a->rows : 1;
  int max_i = (int)(ROWS_W / SC / ((float)w / nrow)) + 1;
  int amount = S.buildings[b].amount < max_i ? S.buildings[b].amount : max_i;
  int prev = 0;
  /* drawn from the back (smaller y) to the front */
  static int16_t px[64], py[64], pp[64];
  int n = 0;
  for (int i = 0; i < amount && n < 64; i++) {
    uint32_t h = hash3(G.seed, (uint32_t)b, (uint32_t)i);
    float x, yy;
    if (nrow != 1) {
      x = floorf((float)(i / nrow)) * w + ((float)(i % nrow) / nrow) * w + floorf((hrand(&h) - 0.5f) * a->xv) + a->x;
      yy = 32 + floorf((hrand(&h) - 0.5f) * a->yv) + ((-nrow / 2.f) * 32 / 2 + (i % nrow) * 32 / 2.f) + a->y;
    } else {
      x = (float)i * w + floorf((hrand(&h) - 0.5f) * a->xv) + a->x;
      yy = 32 + floorf((hrand(&h) - 0.5f) * a->yv) + a->y;
    }
    int pic = row_pic[b];
    if (b == B_GRANDMA) pic = grandma_pic(&h);
    if (a->frames > 1) {
      int f = (prev + (int)(hrand(&h) * (a->frames - 1)) + 1) % a->frames;
      prev = f;
      pic += f;
    }
    px[n] = (int16_t)(ROWS_X + x * SC), py[n] = (int16_t)(y + yy * SC), pp[n] = (int16_t)pic;
    n++;
  }
  for (int k = 0; k < n; k++) {   /* (sorted by y, as the game sorts its sprites) */
    int best = k;
    for (int j = k + 1; j < n; j++)
      if (py[j] < py[best]) best = j;
    int16_t t;
    t = px[k], px[k] = px[best], px[best] = t;
    t = py[k], py[k] = py[best], py[best] = t;
    t = pp[k], pp[k] = pp[best], pp[best] = t;
    if (gfx_visible(px[k], py[k], 24, 48)) gfx_image(pp[k], px[k], py[k], 256);
  }
  /* a grandma's name when their row is chosen */
  if (sel && b == B_GRANDMA && n > 0) {
    int k = (int)(G.real_t / (FPS * 3)) % n;
    uint32_t h = hash3(G.seed, 77, (uint32_t)k + (uint32_t)(G.real_t / (FPS * 3)) * 31);
    char t[48];
    snprintf(t, sizeof t, "%s, age %d", grandma_names[(int)(hrand(&h) * 44) % 44],
             (int)(70 + hrand(&h) * 30 + 13 + S.buildings[b].level));
    int tw = gfx_text_width(F_NAME_SMALL, t) + 8;
    int tx = px[k] + 12 - tw / 2;
    if (tx < ROWS_X) tx = ROWS_X;
    if (tx + tw > SCREEN_W) tx = SCREEN_W - tw;
    gfx_line(px[k] + 12, py[k] + 12, tx + tw / 2, y + 12, BLACK, 190);
    gfx_fill_alpha(tx, y + 2, tw, 11, BLACK, 190);
    gfx_text(F_NAME_SMALL, tx + 4, y + 10, t, RGB(200, 200, 200), 256);
  }
  /* its level and minigame */
  char t[64];
  int ly = y + ROW_H - 4;
  if (can_lumps() || S.buildings[b].level > 0) {
    snprintf(t, sizeof t, "lvl %d", S.buildings[b].level);
    gfx_fill_alpha(ROWS_X, ly - 9, gfx_text_width(F_SMALL, t) + 16, 11, BLACK, 140);
    gfx_icon_scaled(ICON(29, 14), ROWS_X + 1, ly - 9, 11, 256);
    gfx_text_outline(F_SMALL, ROWS_X + 13, ly, t, WHITE, 256);
  }
  if (minigame_available(b)) {
    snprintf(t, sizeof t, "%s %s", S.on_minigame == b + 1 ? "Close" : "View", minigame_name(b));
    int tw = gfx_text_width(F_SMALL, t) + 6;
    gfx_round_rect(SCREEN_W - tw - 2, ly - 9, tw, 11, 2, BLACK, 150);
    gfx_text_outline(F_SMALL, SCREEN_W - tw + 1, ly, t, RGB(255, 238, 200), 256);
  }
  if (S.buildings[b].muted) gfx_fill_alpha(ROWS_X, y, ROWS_W, ROW_H, BLACK, 150);
  if (sel)
    for (int k = 0; k < 3; k++) gfx_rect(ROWS_X + k, y + k, ROWS_W - 2 * k, ROW_H - 2 * k, RGB(255, 238, 221), 160 - k * 50);
  gfx_unclip();
}

void draw_rows(void) {
  if (!gfx_visible(LEFT_W, NEWS_H, SCREEN_W - LEFT_W, SCREEN_H - NEWS_H)) return;
  for (int y = NEWS_H; y < SCREEN_H; y += 128) gfx_image(IM_PANEL_V, LEFT_W, y, 256);
  if (S.on_minigame) {
    draw_minigame(S.on_minigame - 1);
    return;
  }
  rows_list();
  gfx_clip(ROWS_X, NEWS_H, ROWS_W, SCREEN_H - NEWS_H);
  gfx_pattern(IM_DARK_NOISE, ROWS_X, NEWS_H, ROWS_W, SCREEN_H - NEWS_H, 128, 128, 0, 0, 256);
  if (!nrows) {
    ParaStyle st = PS_TIP;
    st.align = 1;
    gfx_para(&st, ROWS_X + 8, NEWS_H + 80, ROWS_W - 16, "Your buildings show here once you have some.\n"
             "\x01" "alpha\x02: back to the store", true);
  }
  int top = (int)U.rows_scroll;
  for (int i = 0; i < nrows; i++) {
    int y = NEWS_H + i * (ROW_H + ROW_GAP) - top;
    if (!gfx_visible(ROWS_X, y, ROWS_W, ROW_H + ROW_GAP)) continue;
    draw_row(rows[i], y, i == U.rows_sel);
    for (int x = ROWS_X; x < SCREEN_W; x += 128) gfx_image(IM_PANEL_H, x, y + ROW_H, 256);
  }
  gfx_unclip();
}

/* ------------------------------------------------------------------ the row's actions */
enum { RA_MINIGAME, RA_LEVEL, RA_MUTE };
static int actions[3], nactions;
static void row_actions(int b) {
  nactions = 0;
  if (minigame_available(b)) actions[nactions++] = RA_MINIGAME;
  if (can_lumps()) actions[nactions++] = RA_LEVEL;
  actions[nactions++] = RA_MUTE;
}

void draw_row_menu(void) {
  rows_list();
  if (!nrows) return;
  int b = rows[U.rows_sel];
  row_actions(b);
  const BuildingRec *r = building_rec(b);
  int w = 200, x = (SCREEN_W - w) / 2, h = 36 + nactions * 18 + 30;
  int y = (SCREEN_H - h) / 2;
  gfx_fill_alpha(0, 0, SCREEN_W, SCREEN_H, BLACK, 100);
  draw_frame_box(x, y, w, h);
  gfx_icon(ICON(r->column, 0), x + 6, y + 6, 256);
  gfx_text_outline(F_TITLE, x + 34, y + 18, res_text(r->plural), WHITE, 256);
  char t[200];
  const char *extra = res_text(S.buildings[b].level == 1 ? r->extra : r->extras);
  char e[96];
  snprintf(e, sizeof e, "%s", extra);
  char *xp = strstr(e, "[X]");
  if (xp) {
    char rest[96];
    snprintf(rest, sizeof rest, "%s", xp + 3);
    snprintf(xp, sizeof e - (size_t)(xp - e), "%d%s", S.buildings[b].level, rest);
  }
  if (S.buildings[b].level > 0) snprintf(t, sizeof t, "Level %d: %s (+%d%% CpS)", S.buildings[b].level, e, S.buildings[b].level);
  else snprintf(t, sizeof t, "Level 0");
  ParaStyle st = PS_SMALL;
  st.align = 1;
  gfx_para(&st, x + 6, y + 30, w - 12, t, true);
  for (int i = 0; i < nactions; i++) {
    int by = y + 52 + i * 18, bw = w - 40, bx = x + 20;
    if (actions[i] == RA_MINIGAME) snprintf(t, sizeof t, "%s %s", S.on_minigame == b + 1 ? "Close" : "View", minigame_name(b));
    else if (actions[i] == RA_LEVEL) snprintf(t, sizeof t, "Level up for %d sugar lump%s", S.buildings[b].level + 1,
                                              S.buildings[b].level ? "s" : "");
    else snprintf(t, sizeof t, "%s", S.buildings[b].muted ? "Unmute" : "Mute");
    bool on = U.list_sel == i;
    bool can = actions[i] != RA_LEVEL || S.lumps >= S.buildings[b].level + 1;
    gfx_fill_alpha(bx, by, bw, 15, on ? WHITE : BLACK, on ? 40 : 110);
    gfx_rect(bx, by, bw, 15, on ? WHITE : RGB(110, 110, 110), 256);
    gfx_text_center(F_NAME_SMALL, bx + bw / 2, by + 11, t, can ? on ? WHITE : RGB(220, 220, 220) : RGB(255, 102, 102), 256);
  }
}

void row_menu_keys(uint32_t p) {
  rows_list();
  if (!nrows) {
    U.overlay = OV_NONE;
    return;
  }
  int b = rows[U.rows_sel];
  row_actions(b);
  if (p & K_UP && U.list_sel > 0) U.list_sel--;
  if (p & K_DOWN && U.list_sel < nactions - 1) U.list_sel++;
  if (p & K_BACK) U.overlay = OV_NONE;
  if (!(p & (K_OK | K_EXE))) return;
  U.overlay = OV_NONE;
  switch (actions[U.list_sel]) {
  case RA_MINIGAME: S.on_minigame = S.on_minigame == b + 1 ? 0 : (uint8_t)(b + 1); break;
  case RA_LEVEL:
    if (S.lumps >= S.buildings[b].level + 1) prompt_open(PROMPT_LEVEL, b);
    break;
  case RA_MUTE: S.buildings[b].muted = !S.buildings[b].muted; break;
  }
}

void rows_keys(uint32_t p) {
  if (S.on_minigame) {
    if (p & K_BACK) {
      S.on_minigame = 0;
      return;
    }
    minigame_keys(S.on_minigame - 1, p);
    return;
  }
  rows_list();
  if (p & K_UP && U.rows_sel > 0) U.rows_sel--;
  if (p & K_DOWN && U.rows_sel < nrows - 1) U.rows_sel++;
  int view = SCREEN_H - NEWS_H, y = U.rows_sel * (ROW_H + ROW_GAP);
  if (y - U.rows_scroll < 0) U.rows_scroll = (float)y;
  if (y + ROW_H - U.rows_scroll > view) U.rows_scroll = (float)(y + ROW_H - view);
  if (p & K_EXE && nrows) {
    U.overlay = OV_ROWMENU, U.list_sel = 0;
  }
}
