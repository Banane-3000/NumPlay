/* The screen and the keys: what each key does where, the store's scrolling and its tooltip, the
 * news ticker at the top (with the sugar lump at its left, as the game has it), and the prompts
 * (the game's "are you sure?" boxes). */
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "ui.h"

Ui U;
extern void click_lump(void);
extern bool can_lumps(void);
extern void lump_icons(int *icon, int *icon2, float *opacity);
extern void wrinkler_hit(void);
extern void news_next(char *out, int size);
extern void menus_keys(uint32_t pressed);
extern void rows_keys(uint32_t pressed);
extern void ascend_keys(uint32_t pressed);
extern bool level_up(int b);
extern void ascend_start(void);
extern void game_save(void);

#define TIP_TIME (FPS * 6)

void ui_init(void) {
  memset(&U, 0, sizeof U);
  U.view = VIEW_STORE;
  U.sec = SEC_BUILDINGS;
  U.prompt_sel = 0;
  news_next(U.news[0], sizeof U.news[0]);
  U.news_t = FPS * 2;
  if (!S.seen_keys) U.overlay = OV_KEYS;
  ui_store_layout();
}

/* ------------------------------------------------------------------ prompts */
void prompt_open(int kind, int arg) {
  U.prompt_kind = kind, U.prompt_arg = arg, U.prompt_sel = 0;
  U.overlay = OV_PROMPT;
}

static void prompt_answer(bool yes) {
  int kind = U.prompt_kind, arg = U.prompt_arg;
  U.overlay = OV_NONE;
  if (!yes) return;
  if (kind == PROMPT_UPGRADE) upgrade_buy(arg, true);
  else if (kind == PROMPT_ASCEND) ascend_start();
  else if (kind == PROMPT_LEVEL) level_up(arg);
  else if (kind == PROMPT_SELL_ALL) building_sell(arg, -1);
  else if (kind == PROMPT_WIPE) {
    extern void game_wipe(void);
    game_wipe();
  } else if (kind == PROMPT_REINCARNATE) {
    extern void reincarnate(int mode);
    reincarnate(arg);
  }
  G.store_dirty = true;
}

/* what a prompt says, its title, and its two answers */
void prompt_text(char *title, int ts, char *body, int bs, const char **yes, const char **no) {
  int arg = U.prompt_arg;
  *yes = "Yes", *no = "No";
  switch (U.prompt_kind) {
  case PROMPT_UPGRADE: {
    const UpgradeRec *r = upgrade_rec(arg);
    snprintf(title, ts, "%s", res_text(r->name));
    if (arg == U_ELDER_COVENANT || arg == U_REVOKE_ELDER_COVENANT || arg == U_ELDER_PLEDGE)
      snprintf(body, bs, "Are you sure you want to buy this?");
    else if (arg == U_ONE_MIND)
      snprintf(body, bs, "\x01Warning:\x02 purchasing this will have unexpected, and potentially undesirable "
                         "results!\nIt's all downhill from here. You have been warned!\nPurchase anyway?");
    else if (arg == U_COMMUNAL_BRAINSWEEP || arg == U_ELDER_PACT)
      snprintf(body, bs, "\x01Warning:\x02 this will awaken the elders further.\nPurchase anyway?");
    else
      snprintf(body, bs, "Are you sure you want to buy this?");
    *yes = "Purchase";
    break;
  }
  case PROMPT_ASCEND: {
    extern double how_much_prestige(double cookies);
    char a[48];
    double gain = floor(how_much_prestige(S.cookies_reset + S.cookies_earned)) - S.prestige;
    beautify(gain > 0 ? gain : 0, 0, a);
    snprintf(title, ts, "Ascend");
    snprintf(body, bs,
             "Do you REALLY want to ascend?\nYou will lose your progress and start over from scratch.\nAll your "
             "cookies will be converted into prestige and heavenly chips.\nYou will keep your achievements.\x05"
             "Ascending now gives \x01%s prestige level%s\x02.",
             a, gain == 1 ? "" : "s");
    *yes = "Ascend";
    break;
  }
  case PROMPT_LEVEL: {
    const BuildingRec *r = building_rec(arg);
    int lvl = S.buildings[arg].level;
    snprintf(title, ts, "Level up %s", res_text(r->plural));
    snprintf(body, bs, "Level up your %s for \x01%d sugar lump%s\x02?\nEach level gives \x01+1%% CpS\x02 to them%s.",
             res_text(r->plural), lvl + 1, lvl ? "s" : "",
             arg == B_FARM || arg == B_TEMPLE || arg == B_WIZARD_TOWER || arg == B_BANK
               ? ", and the first unlocks their minigame" : "");
    *yes = "Level up";
    break;
  }
  case PROMPT_SELL_ALL:
    snprintf(title, ts, "Sell all");
    snprintf(body, bs, "Do you really want to sell your %d %s?", S.buildings[arg].amount,
             res_text(building_rec(arg)->plural));
    *yes = "Sell";
    break;
  case PROMPT_WIPE:
    snprintf(title, ts, "Wipe save");
    snprintf(body, bs, "Do you REALLY want to wipe your save?\nYou will lose your progress, your achievements, "
                       "and your heavenly chips!");
    *yes = "Wipe";
    break;
  case PROMPT_REINCARNATE:
    snprintf(title, ts, "Reincarnate");
    snprintf(body, bs, arg == 1 ? "Are you ready to return to the mortal world?\nYou will be \x01" "Born again\x02: "
                                  "your heavenly upgrades and prestige will not count this run."
                                : "Are you ready to return to the mortal world?");
    *yes = "Reincarnate";
    break;
  default:
    title[0] = body[0] = 0;
  }
}

void draw_prompt(void) {
  char title[96], body[600];
  const char *yes, *no;
  prompt_text(title, sizeof title, body, sizeof body, &yes, &no);
  int w = 236, x = (SCREEN_W - w) / 2, pad = 8;
  ParaStyle st = PS_TIP;
  st.align = 1;
  int bh = gfx_para(&st, x + pad, 0, w - 2 * pad, body, false);
  int h = pad + 18 + bh + 8 + 18 + pad;
  int y = (SCREEN_H - h) / 2;
  if (!gfx_visible(x, y, w, h)) return;
  gfx_fill_alpha(0, 0, SCREEN_W, SCREEN_H, BLACK, 100);
  draw_frame_box(x, y, w, h);
  gfx_text_outline_center(F_TITLE, SCREEN_W / 2, y + pad + 11, title, WHITE, 256);
  gfx_para(&st, x + pad, y + pad + 18, w - 2 * pad, body, true);
  int by = y + h - pad - 16;
  const char *opts[2] = {yes, no};
  for (int i = 0; i < 2; i++) {
    int bw = 80, bx = SCREEN_W / 2 + (i ? 6 : -6 - bw);
    bool on = U.prompt_sel == i;
    gfx_round_rect(bx, by, bw, 16, 3, on ? RGB(255, 255, 255) : BLACK, on ? 60 : 120);
    gfx_rect(bx, by, bw, 16, on ? WHITE : RGB(120, 120, 120), on ? 256 : 160);
    gfx_text_center(F_NAME_SMALL, bx + bw / 2, by + 12, opts[i], on ? WHITE : RGB(200, 200, 200), 256);
  }
}

/* ------------------------------------------------------------------ the news */
/* where the news starts: past the sugar lump and its count, once there are lumps */
#define NEWS_X (can_lumps() ? 38 : 4)
void ui_news_next(void) {
  char t[sizeof U.news[0]];
  news_next(t, sizeof t);
  U.news_t = 0;
  if (!t[0]) return;   /* (nothing to say: the last news stays) */
  U.news_cur ^= 1;
  memcpy(U.news[U.news_cur], t, sizeof t);
  U.news_fade = FPS / 2;
}

static int news_lines(const char *s, char lines[2][200], int w) {
  /* wrapped in two lines at most; 0 if it doesn't fit */
  int n = (int)strlen(s), start = 0, k = 0;
  while (start < n) {
    if (k == 2) return 0;
    int end = start, last_space = -1;
    while (end < n && gfx_text_n_width(F_SMALL, s + start, end - start + 1) <= w) {
      if (s[end] == ' ') last_space = end;
      end++;
    }
    if (end < n && last_space > start) end = last_space;
    memcpy(lines[k], s + start, (size_t)(end - start));
    lines[k][end - start] = 0;
    k++;
    start = end;
    while (start < n && s[start] == ' ') start++;
  }
  return k;
}

static void news_text(const char *s, int alpha, int dy) {
  char lines[2][200];
  int w = SCREEN_W - NEWS_X - 6;
  int n = news_lines(s, lines, w);
  uint16_t c = RGB(255, 255, 255);
  if (n == 1) gfx_text_outline_center(F_SMALL, NEWS_X + w / 2 + 2, 13 + dy, lines[0], c, alpha);
  else if (n == 2) {
    gfx_text_outline_center(F_SMALL, NEWS_X + w / 2 + 2, 8 + dy, lines[0], c, alpha);   /* (both lines above the */
    gfx_text_outline_center(F_SMALL, NEWS_X + w / 2 + 2, 16 + dy, lines[1], c, alpha);  /* bar's edge) */
  } else {   /* too long: it goes by */
    U.news_long = true;
    int tw = gfx_text_width(F_SMALL, s);
    int x = SCREEN_W - (int)(U.news_t * 1.2f) % (tw + SCREEN_W - NEWS_X);
    gfx_clip(NEWS_X, 0, SCREEN_W - NEWS_X, NEWS_H);
    gfx_text_outline(F_SMALL, x, 13 + dy, s, c, alpha);
    gfx_unclip();
  }
}

void draw_news(void) {
  if (!gfx_visible(0, 0, SCREEN_W, NEWS_H)) return;
  U.news_long = false;
  gfx_pattern(IM_DARK_NOISE, 0, 0, SCREEN_W, NEWS_H, 128, 128, 0, 0, 256);
  gfx_vgrad(0, 0, SCREEN_W, NEWS_H, BLACK, 140, 60);
  for (int x = 0; x < SCREEN_W; x += 128) gfx_image(IM_PANEL_H, x, NEWS_H - 3, 256);
  if (U.news_fade > 0) {
    float k = (float)U.news_fade / (FPS / 2);
    news_text(U.news[U.news_cur ^ 1], (int)(k * 256), (int)((1 - k) * -8));
    news_text(U.news[U.news_cur], (int)((1 - k) * 256), (int)(k * 8));
  } else
    news_text(U.news[U.news_cur], 256, 0);
  if (U.on_ticker) {   /* (chosen: EXE presses it) */
    gfx_rect(NEWS_X - 2, 0, SCREEN_W - NEWS_X + 2, NEWS_H - 2, RGB(255, 238, 200), 200);
    gfx_fill_alpha(NEWS_X - 1, 1, SCREEN_W - NEWS_X, NEWS_H - 4, WHITE, 20);
  }
  /* the sugar lump, growing, and how many there are */
  if (can_lumps()) {
    int icon, icon2;
    float o;
    lump_icons(&icon, &icon2, &o);
    gfx_icon_scaled(icon, 0, -1, 20, 256);
    if (o > 0 && o < 1) gfx_icon_scaled(icon2, 0, -1, 20, (int)(o * 256));
    char t[16];
    snprintf(t, sizeof t, "%.0f", S.lumps);
    gfx_text_outline(F_NAME_SMALL, 20, 13, t, WHITE, 256);
  }
}

/* ------------------------------------------------------------------ the keys */
static uint32_t repeat_held;
static int repeat_t;

static void store_buy(void) {
  int u = store_selected_upgrade(), b = store_selected_building();
  if (u >= 0) {
    if (bit_get(S.upgrades_bought, u) && upgrade_rec(u)->pool != POOL_TOGGLE) return;
    if (upgrade_needs_confirm(u) && upgrade_can_buy(u)) prompt_open(PROMPT_UPGRADE, u);
    else upgrade_buy(u, false);
  } else if (b >= 0) {
    if (G.buy_mode == 1) building_buy(b, G.buy_bulk);
    else if (G.buy_bulk == -1 && S.buildings[b].amount > 0) prompt_open(PROMPT_SELL_ALL, b);
    else building_sell(b, G.buy_bulk);
  }
  G.store_dirty = true;
}

static void scroll_to_selection(void) {
  extern int store_selected_y(int *h);
  int h, y = store_selected_y(&h);
  extern int store_content_h(void);
  int view = SCREEN_H - STORE_TOP;
  float t = U.scroll_t;
  if (y - 4 < t) t = (float)(y - 4);
  if (y + h + 4 > t + view) t = (float)(y + h + 4 - view);
  if (U.sec == SEC_BUILDINGS && U.idx == 0) {
    /* the upgrades above stay in sight while the first buildings are chosen */
    extern int store_buildings_top(void);
    if (store_buildings_top() < view - PRODUCT_H * 2) t = 0;
  }
  int max = store_content_h() - view;
  if (t > max) t = (float)max;
  if (t < 0) t = 0;
  U.scroll_t = t;
}

void ui_keys(uint32_t pressed, uint32_t held) {
  if (pressed) U.dirty = 2;
  /* the arrows repeat while held */
  uint32_t arrows = held & (K_LEFT | K_RIGHT | K_UP | K_DOWN);
  if (arrows && arrows == repeat_held) {
    if (++repeat_t >= 9 && (repeat_t - 9) % 3 == 0) pressed |= arrows;
  } else
    repeat_t = 0;
  repeat_held = arrows;

  if (U.overlay == OV_PROMPT) {
    if (pressed & (K_LEFT | K_RIGHT | K_UP | K_DOWN)) U.prompt_sel ^= 1;
    if (pressed & (K_OK | K_EXE)) prompt_answer(U.prompt_sel == 0);
    else if (pressed & K_BACK) prompt_answer(false);
    return;
  }
  if (G.on_ascend || G.ascend_timer > 0) {
    if (G.on_ascend) ascend_keys(pressed);
    return;
  }
  if (U.overlay != OV_NONE) {
    menus_keys(pressed);
    return;
  }
  /* the big cookie, whatever else is shown */
  if (pressed & K_OK) {
    click_cookie();
    U.tip_t = 0;
  }
  G.big_state = (held & K_OK) ? 1 : G.click_t < FPS / 3 ? 2 : 0;
  if (pressed & K_SHIFT) {
    int i = shimmer_next();
    if (i >= 0) shimmer_pop(&G.shimmers[i]);
  }
  if (pressed & K_VAR) wrinkler_hit();
  if (pressed & K_XNT) click_lump();
  if (pressed & K_DEL) {   /* the notes, oldest first */
    for (int i = MAX_NOTES - 1; i >= 0; i--)
      if (G.notes[i].title[0]) {
        G.notes[i].title[0] = 0;
        break;
      }
  }
  if (pressed & K_BACK && !(U.view == VIEW_ROWS && S.on_minigame)) {
    U.overlay = OV_MENU, U.menu_sel = 0, U.from_menu = false;
    return;
  }
  if (pressed & K_TOOLBOX) {
    U.overlay = OV_STATS, U.list_sel = 0, U.list_top = 0, U.from_menu = false;
    return;
  }
  if (pressed & K_ALPHA) {
    U.view = U.view == VIEW_STORE ? VIEW_ROWS : VIEW_STORE;
    if (U.view == VIEW_STORE) S.on_minigame = 0;
    U.tip_t = 0;
    return;
  }
  if (pressed & K_DIV) {
    G.buy_mode = -G.buy_mode;
    if (G.buy_mode == 1 && G.buy_bulk == -1) G.buy_bulk = 100;
    G.store_dirty = true;
  }
  if (pressed & K_MUL) {
    G.buy_bulk = G.buy_bulk == 1 ? 10 : G.buy_bulk == 10 ? 100 : G.buy_bulk == 100 && G.buy_mode == -1 ? -1 : 1;
    G.store_dirty = true;
  }
  if (U.view == VIEW_ROWS) {
    rows_keys(pressed);
    return;
  }
  if (U.on_ticker) {   /* the news ticker, above the store */
    if (pressed & K_DOWN) U.on_ticker = false, U.tip_t = TIP_TIME;
    if (pressed & K_EXE) {
      extern void ticker_click(void);
      ticker_click();
      ui_news_next();
    }
    return;
  }
  if (pressed & (K_LEFT | K_RIGHT | K_UP | K_DOWN)) {
    int sec = U.sec, idx = U.idx;
    store_move(pressed & K_LEFT ? -1 : pressed & K_RIGHT ? 1 : 0, pressed & K_UP ? -1 : pressed & K_DOWN ? 1 : 0);
    if (pressed & K_UP && sec == U.sec && idx == U.idx) U.on_ticker = true, U.tip_t = 0;   /* (up past the top) */
    else U.tip_t = TIP_TIME;
    scroll_to_selection();
  }
  if (pressed & K_EXE) {
    store_buy();
    U.tip_t = TIP_TIME;
    ui_store_layout();
    scroll_to_selection();
  }
}

/* ------------------------------------------------------------------ a frame */
extern int minigame_tip(int b, int x, int y, int w, bool draw);
/* the tooltip of what the minigame's panel has chosen, over the left column */
static void draw_minigame_tip(void) {
  if (U.view != VIEW_ROWS || !S.on_minigame || U.tip_t <= 0 || U.overlay) return;
  int b = S.on_minigame - 1, w = LEFT_W + 6, x = 1, pad = 5;
  int h = minigame_tip(b, x + pad, 0, w - 2 * pad, false);
  if (h <= 0) return;
  h += 2 * pad;
  int y = NEWS_H + 30;
  if (y + h > SCREEN_H - 2) y = SCREEN_H - 2 - h;
  if (!gfx_visible(x, y, w, h)) return;
  draw_frame_box(x, y, w, h);
  minigame_tip(b, x + pad, y + pad, w - 2 * pad, true);
}

static void draw_all(void) {
  if (G.on_ascend) {
    draw_ascend();
    if (U.overlay == OV_PROMPT) draw_prompt();
    return;
  }
  draw_news();
  draw_left();
  if (U.view == VIEW_STORE) draw_store();
  else draw_rows();
  if (G.ascend_timer > 0) {   /* (all but the cookie fades to black) */
    int a = G.ascend_timer * 256 / FPS;
    a = a > 256 ? 256 : a;
    gfx_fill_alpha(LEFT_W, NEWS_H, SCREEN_W - LEFT_W, SCREEN_H - NEWS_H, BLACK, a);
    gfx_fill_alpha(0, 0, SCREEN_W, NEWS_H, BLACK, a);
    draw_notes();
    return;
  }
  draw_shimmers();
  draw_buffs();
  draw_popups();
  draw_notes();
  draw_tooltip();
  draw_minigame_tip();
  if (U.overlay == OV_PROMPT) draw_prompt();
  else if (U.overlay != OV_NONE) draw_overlay();
}

/* the screen's side of a logic frame: scrolling, the tooltip's time, the news */
void ui_tick(void) {
  extern void ascend_enter(void);
  extern void ascend_tick(void);
  static bool was_on_ascend;
  if (G.on_ascend && !was_on_ascend) ascend_enter(), U.overlay = OV_NONE;
  was_on_ascend = G.on_ascend;
  if (G.on_ascend) ascend_tick();
  if (G.store_dirty) ui_store_layout(), G.store_dirty = false;
  U.scroll += (U.scroll_t - U.scroll) * 0.4f;
  if (fabsf(U.scroll_t - U.scroll) < 0.5f) U.scroll = U.scroll_t;
  if (U.tip_t > 0) U.tip_t--;
  U.news_t++;
  if (U.news_fade > 0) U.news_fade--;
  if (U.news_t > FPS * 10) ui_news_next();
}

/* what is drawn: the left column every frame (it moves all the time), the rest of the screen when a key
 * was pressed and five times a second (prices, notes), the news while it changes; the pages that fill
 * the screen when pressed and three times a second */
void ui_frame(void) {
  ui_store_layout();
  bool page = U.overlay == OV_OPTIONS || U.overlay == OV_STATS || U.overlay == OV_INFO || U.overlay == OV_KEYS;
  bool reindeer = G.nshimmers_reindeer > 0;   /* (it crosses the whole screen) */
  bool full = U.dirty > 0 || G.on_ascend || G.ascend_timer > 0 || G.reincarnate_timer > 0 || reindeer ||
              G.draw_t % (page ? 10 : 6) == 0;
  if (U.dirty > 0) U.dirty--;
  if (full) gfx_render(0, 0, SCREEN_W, SCREEN_H, draw_all);
  else if (!page) {
    gfx_render(0, NEWS_H, LEFT_W, SCREEN_H - NEWS_H, draw_all);
    if (U.news_fade > 0 || U.news_long || G.draw_t % 3 == 0) gfx_render(0, 0, SCREEN_W, NEWS_H, draw_all);
  }
  G.draw_t++;
}
