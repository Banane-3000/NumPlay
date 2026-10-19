/* The menus over the game: the menu (Back), Options, Statistics (the general ones, then the
 * upgrades and achievements as the game's crates, with their tooltips), Info, the keys, and the
 * special tabs: Santa's progress and Krumblor the cookie dragon (with its auras). */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ui.h"

extern bool game_save(void);
extern bool can_lumps(void);
extern void bakery_name(char *out, int size);
extern double heavenly_multiplier(void);
extern double how_much_prestige(double cookies);
extern const char *const santa_levels[15];
extern const char *const aura_names[N_AURAS];
extern const char *const aura_descs[N_AURAS];
extern const int16_t aura_icons[N_AURAS];
extern const char *dragon_name(void);
extern int dragon_frame(void);
extern void dragon_action(char *action, int asize, char *cost, int csize, bool *can);
extern void upgrade_dragon(void);
extern void set_dragon_aura(int slot, int aura);
extern void pet_dragon(void);
extern void upgrade_santa(void);
extern bool g_quit;

#define PANEL_Y NEWS_H
#define PANEL_H (SCREEN_H - NEWS_H)
#define PAD 10

static void panel(const char *title) {
  gfx_pattern(IM_DARK_NOISE, 0, PANEL_Y, SCREEN_W, PANEL_H, 128, 128, 0, 0, 256);
  gfx_image_scaled_fast(IM_BORDER_SOFT, 0, PANEL_Y, SCREEN_W, PANEL_H, 256);
  for (int x = 0; x < SCREEN_W; x += 128) gfx_image(IM_PANEL_H, x, PANEL_Y, 256);
  if (title) gfx_text_outline_center(F_KAVOON, SCREEN_W / 2, PANEL_Y + 20, title, WHITE, 256);
}

/* a button of the game's menus (".option"), selected or not */
static void button(int x, int y, int w, const char *s, bool sel, bool on) {
  gfx_fill_alpha(x, y, w, 15, on ? RGB(255, 255, 255) : BLACK, on ? 40 : 110);
  gfx_rect(x, y, w, 15, sel ? WHITE : on ? RGB(200, 200, 200) : RGB(110, 110, 110), 256);
  if (sel) gfx_rect(x - 1, y - 1, w + 2, 17, RGB(255, 238, 200), 140);
  gfx_text_center(F_NAME_SMALL, x + w / 2, y + 11, s, sel ? WHITE : on ? RGB(230, 230, 230) : RGB(170, 170, 170), 256);
}

/* ------------------------------------------------------------------ the menu */
enum { M_OPTIONS, M_STATS, M_INFO, M_LEGACY, M_SANTA, M_DRAGON, M_KEYS, M_SAVE, M_QUIT, N_M };
static int menu_items[N_M], nmenu;
static void menu_build(void) {
  nmenu = 0;
  menu_items[nmenu++] = M_OPTIONS;
  menu_items[nmenu++] = M_STATS;
  menu_items[nmenu++] = M_INFO;
  if (S.resets > 0 || how_much_prestige(S.cookies_reset + S.cookies_earned) >= 1) menu_items[nmenu++] = M_LEGACY;
  if (has(U_A_FESTIVE_HAT)) menu_items[nmenu++] = M_SANTA;
  if (has(U_A_CRUMBLY_EGG)) menu_items[nmenu++] = M_DRAGON;
  menu_items[nmenu++] = M_KEYS;
  menu_items[nmenu++] = M_SAVE;
  menu_items[nmenu++] = M_QUIT;
}
static const char *const menu_names[N_M] = {"Options", "Statistics", "Info", "Legacy", "Santa's progress",
                                            "Krumblor", "Keys", "Save", "Save & Quit"};

static void draw_menu(void) {
  menu_build();
  char t[64];
  bakery_name(t, sizeof t);
  int w = 150, h = 34 + nmenu * 18 + 6, x = (SCREEN_W - w) / 2, y = PANEL_Y + (PANEL_H - h) / 2;
  gfx_fill_alpha(0, PANEL_Y, SCREEN_W, PANEL_H, BLACK, 110);
  draw_frame_box(x, y, w, h);
  gfx_text_outline_center(F_KAVOON, SCREEN_W / 2, y + 19, "Menu", WHITE, 256);
  gfx_text_center(F_SMALL, SCREEN_W / 2, y + 30, t, RGB(204, 204, 204), 256);
  for (int i = 0; i < nmenu; i++) button(x + 12, y + 36 + i * 18, w - 24, menu_names[menu_items[i]], U.menu_sel == i, true);
}

static void menu_choose(int item) {
  switch (item) {
  case M_OPTIONS: U.overlay = OV_OPTIONS, U.list_sel = 0, U.list_top = 0; break;
  case M_STATS: U.overlay = OV_STATS, U.list_sel = 0, U.list_top = 0, U.stats_tab = 0; break;
  case M_INFO: U.overlay = OV_INFO, U.list_top = 0; break;
  case M_LEGACY: prompt_open(PROMPT_ASCEND, 0); break;
  case M_SANTA: U.overlay = OV_SPECIAL, U.special = 0, U.list_sel = 0; break;
  case M_DRAGON: U.overlay = OV_SPECIAL, U.special = 1, U.list_sel = 0; break;
  case M_KEYS: U.overlay = OV_KEYS; break;
  case M_SAVE:
    game_save();
    notify("Game saved", "", -1, 1);
    U.overlay = OV_NONE;
    break;
  case M_QUIT: g_quit = true; break;
  }
}

/* ------------------------------------------------------------------ options */
typedef struct { const char *name, *label; uint8_t *pref; bool invert; } Opt;
enum { O_SAVE, O_WIPE, O_FIRST_PREF };
static Opt opts[] = {
  {"Save", "save manually (the game autosaves every 60 seconds)", NULL, false},
  {"Wipe save", "delete all your progress, including your achievements", NULL, false},
  {"Fancy graphics", "visual improvements; disabling may improve performance", &S.prefs.fancy, false},
  {"Particles", "cookies falling down, etc; disabling may improve performance", &S.prefs.particles, false},
  {"Numbers", "numbers that pop up when clicking the cookie", &S.prefs.numbers, false},
  {"Milk", "only appears with enough achievements", &S.prefs.milk, false},
  {"Cursors", "visual display of your cursors", &S.prefs.cursors, false},
  {"Wobbly cookie", "your cookie will react when you click it", &S.prefs.wobbly, false},
  {"Short numbers", "shorten big numbers", &S.prefs.format_long, true},
  {"Fast notes", "notifications disappear much faster", &S.prefs.notes_on, false},
  {"Scary stuff", NULL, &S.prefs.not_scary, true},
};
#define N_OPTS (int)(sizeof opts / sizeof opts[0])

/* where each option is (from the top of the page's content), and its label's lines */
static int opt_y[N_OPTS + 1];
static void options_layout(void) {
  int y = 16;
  for (int i = 0; i < N_OPTS; i++) {
    if (i == O_FIRST_PREF) y += 20;
    opt_y[i] = y;
    char t[64], l[96];
    snprintf(t, sizeof t, "%s OFF", opts[i].name);
    int lx = PAD + gfx_text_width(F_NAME_SMALL, t) + 20;
    int h = 15;
    if (opts[i].label) {
      snprintf(l, sizeof l, "(%s)", opts[i].label);
      int lh = gfx_para(&PS_SMALL, lx, 0, SCREEN_W - lx - 6, l, false);
      if (lh + 2 > h) h = lh + 2;
    }
    y += h + 4;
  }
  opt_y[N_OPTS] = y;
}

static void draw_options(void) {
  panel("Options");
  options_layout();
  int top = PANEL_Y + 30 - U.list_top;
  gfx_clip(0, PANEL_Y + 26, SCREEN_W, PANEL_H - 28);
  gfx_text_outline(F_NAME, PAD, top + 10, "General", RGB(246, 218, 184), 256);
  gfx_text_outline(F_NAME, PAD, top + opt_y[O_FIRST_PREF] - 6, "Settings", RGB(246, 218, 184), 256);
  for (int i = 0; i < N_OPTS; i++) {
    int y = top + opt_y[i];
    char t[64], w[64];
    bool on = true;
    if (opts[i].pref) {
      on = (*opts[i].pref != 0) != opts[i].invert;
      snprintf(t, sizeof t, "%s %s", opts[i].name, on ? "ON" : "OFF");
    } else
      snprintf(t, sizeof t, "%s", opts[i].name);
    snprintf(w, sizeof w, "%s OFF", opts[i].name);
    int bw = gfx_text_width(F_NAME_SMALL, opts[i].pref ? w : t) + 14;
    if (!gfx_visible(0, y - 2, SCREEN_W, opt_y[i + 1] - opt_y[i] + 4)) continue;
    button(PAD, y, bw, t, U.list_sel == i, on);
    if (opts[i].label) {
      char l[96];
      snprintf(l, sizeof l, "(%s)", opts[i].label);
      int lx = PAD + gfx_text_width(F_NAME_SMALL, w) + 20;
      ParaStyle st = PS_SMALL;
      st.color = RGB(180, 180, 180);
      int lh = gfx_para(&st, lx, 0, SCREEN_W - lx - 6, l, false);
      gfx_para(&st, lx, y + (15 - lh) / 2 - (lh > 15 ? 0 : 1), SCREEN_W - lx - 6, l, true);
    }
  }
  gfx_unclip();
}

static void options_keys(uint32_t p) {
  if (p & K_UP && U.list_sel > 0) U.list_sel--;
  if (p & K_DOWN && U.list_sel < N_OPTS - 1) U.list_sel++;
  options_layout();
  int y = opt_y[U.list_sel], view = PANEL_H - 30;
  if (y - U.list_top > view - 26) U.list_top = y - view + 26;
  if (y - U.list_top < 20) U.list_top = y - 20 < 0 ? 0 : y - 20;
  if (U.list_sel == 0) U.list_top = 0;
  if (p & (K_OK | K_EXE)) {
    Opt *o = &opts[U.list_sel];
    if (U.list_sel == O_SAVE) {
      game_save();
      notify("Game saved", "", -1, 1);
    } else if (U.list_sel == O_WIPE)
      prompt_open(PROMPT_WIPE, 0);
    else if (o->pref) {
      *o->pref = !*o->pref;
      G.store_dirty = true;
    }
  }
}

/* ------------------------------------------------------------------ statistics */
/* the tabs: General, Upgrades, Achievements */
enum { TAB_GENERAL, TAB_UPGRADES, TAB_ACHIEVS, N_TABS };
static const char *const tab_names[N_TABS] = {"General", "Upgrades", "Achievements"};
#define GRID_COLS 10
#define GRID_X ((SCREEN_W - GRID_COLS * CRATE) / 2)

/* the crates the tabs show, as the game lists them (sorted by their order) */
static uint16_t grid[N_UPGRADES > N_ACHIEVS ? N_UPGRADES : N_ACHIEVS];
static int ngrid, grid_break[3];   /* where the cookies / prestige (upgrades) or shadow (achievements) start */

static void sort_by_order(uint16_t *ids, int n, bool achiev) {
  for (int i = 1; i < n; i++) {
    uint16_t v = ids[i];
    float o = achiev ? achiev_rec(v)->order : upgrade_rec(v)->order;
    int j = i - 1;
    while (j >= 0 && (achiev ? achiev_rec(ids[j])->order : upgrade_rec(ids[j])->order) > o) ids[j + 1] = ids[j], j--;
    ids[j + 1] = v;
  }
}

static void grid_build(int tab) {
  ngrid = 0;
  if (tab == TAB_UPGRADES) {
    /* prestige, then the regular ones, then cookies: those bought */
    for (int pass = 0; pass < 3; pass++) {
      grid_break[pass] = ngrid;
      int start = ngrid;
      for (int u = 0; u < N_UPGRADES; u++) {
        const UpgradeRec *r = upgrade_rec(u);
        if (!bit_get(S.upgrades_bought, u) || r->pool == POOL_DEBUG) continue;
        int want = r->pool == POOL_PRESTIGE ? 0 : r->pool == POOL_COOKIE ? 2 : 1;
        if (want != pass) continue;
        grid[ngrid++] = (uint16_t)u;
      }
      sort_by_order(grid + start, ngrid - start, false);
    }
  } else if (tab == TAB_ACHIEVS) {
    for (int pass = 0; pass < 2; pass++) {
      grid_break[pass] = ngrid;
      int start = ngrid;
      for (int a = 0; a < N_ACHIEVS; a++) {
        const AchievRec *r = achiev_rec(a);
        if (r->pool == APOOL_DUNGEON) continue;
        if ((r->pool == APOOL_SHADOW) != (pass == 1)) continue;
        if (r->pool == APOOL_SHADOW && !has_achiev(a)) continue;
        grid[ngrid++] = (uint16_t)a;
      }
      sort_by_order(grid + start, ngrid - start, true);
    }
    grid_break[2] = ngrid;
  }
}

static void stat_line(int *y, const char *s, bool draw) {
  *y += gfx_para(&PS_TIP, PAD, *y, SCREEN_W - 2 * PAD, s, draw) + 2;
}
static void stat_title(int *y, const char *s, bool draw) {
  *y += 4;
  if (draw) {
    gfx_text_outline(F_KAVOON, PAD, *y + 14, s, WHITE, 256);
    tip_line(PAD, *y + 18, SCREEN_W - 2 * PAD);
  }
  *y += 22;
}

static int general(int y0, bool draw) {
  char a[64], b[64], c[64], l[300];
  int y = y0;
  stat_title(&y, "General", draw);
  beautify(S.cookies, 0, a);
  snprintf(l, sizeof l, "\x01" "Cookies in bank:\x02 %s", a), stat_line(&y, l, draw);
  beautify(S.cookies_earned, 0, a);
  snprintf(l, sizeof l, "\x01" "Cookies baked (this ascension):\x02 %s", a), stat_line(&y, l, draw);
  beautify(S.cookies_earned + S.cookies_reset, 0, a);
  snprintf(l, sizeof l, "\x01" "Cookies baked (all time):\x02 %s", a), stat_line(&y, l, draw);
  if (S.cookies_reset > 0) {
    beautify(S.cookies_reset, 0, a);
    snprintf(l, sizeof l, "\x01" "Cookies forfeited by ascending:\x02 %s", a), stat_line(&y, l, draw);
  }
  if (S.resets) {
    say_time((S.play_time - S.full_date) / 1000 * FPS, -1, a);
    snprintf(l, sizeof l, "\x01Legacy started:\x02 %s%s, with %u ascension%s", a[0] ? a : "just now", a[0] ? " ago" : "",
             S.resets, S.resets == 1 ? "" : "s");
    stat_line(&y, l, draw);
  }
  say_time((S.play_time - S.start_date) / 1000 * FPS, -1, a);
  snprintf(l, sizeof l, "\x01Run started:\x02 %s%s", a[0] ? a : "just now", a[0] ? " ago" : "");
  stat_line(&y, l, draw);
  snprintf(l, sizeof l, "\x01" "Buildings owned:\x02 %d", G.buildings_owned), stat_line(&y, l, draw);
  beautify(G.cookies_ps, 1, a);
  beautify(round(G.global_cps_mult * 100), 1, b);
  beautify(round(G.cps_sucked * 100), 1, c);
  snprintf(l, sizeof l, "\x01" "Cookies per second:\x02 %s (multiplier: %s%%)%s%s%s", a, b, G.cps_sucked > 0 ? " (withered: " : "",
           G.cps_sucked > 0 ? c : "", G.cps_sucked > 0 ? "%)" : "");
  stat_line(&y, l, draw);
  beautify(G.cookies_ps_raw, 1, a);
  beautify(S.cookies_ps_raw_highest, 1, b);
  snprintf(l, sizeof l, "\x01Raw cookies per second:\x02 %s (highest this ascension: %s)", a, b), stat_line(&y, l, draw);
  beautify(G.computed_mouse_cps, 1, a);
  snprintf(l, sizeof l, "\x01" "Cookies per click:\x02 %s", a), stat_line(&y, l, draw);
  beautify(S.cookie_clicks, 0, a);
  snprintf(l, sizeof l, "\x01" "Cookie clicks:\x02 %s", a), stat_line(&y, l, draw);
  beautify(S.handmade_cookies, 0, a);
  snprintf(l, sizeof l, "\x01Hand-made cookies:\x02 %s", a), stat_line(&y, l, draw);
  snprintf(l, sizeof l, "\x01Golden cookie clicks:\x02 %u (all time: %u)", S.golden_clicks_local, S.golden_clicks);
  stat_line(&y, l, draw);
  double drop = drop_rate_mult();
  if (drop != 1) {
    beautify(drop, 2, a);
    snprintf(l, sizeof l, "\x01Random drop multiplier:\x02 x%s", a), stat_line(&y, l, draw);
  }
  stat_line(&y, "\x01Running version:\x02 2.058", draw);

  /* special */
  static const char *const season_names[6] = {"", "Christmas", "Halloween", "Valentine's day", "Business day",
                                              "Easter"};
  const char *wrath = S.elder_wrath == 1 ? "awoken" : S.elder_wrath == 2 ? "displeased" : S.elder_wrath == 3 ? "angered"
                      : S.pledges > 0 ? "appeased" : NULL;
  if (S.resets > 0 || S.season || S.research_t > 0 || wrath || S.pledge_t > 0 || can_lumps() || S.wrinklers_popped ||
      S.reindeer_clicked || has(U_A_FESTIVE_HAT) || has(U_A_CRUMBLY_EGG)) {
    stat_title(&y, "Special", draw);
    if (S.resets > 0) {
      snprintf(l, sizeof l, "\x01" "Challenge mode:\x02 %s", S.ascension_mode == 1 ? "Born again" : "None");
      stat_line(&y, l, draw);
    }
    if (S.season) {
      say_time(S.season_t, -1, a);
      int sn = S.season == SEASON_CHRISTMAS ? 1 : S.season == SEASON_HALLOWEEN ? 2 : S.season == SEASON_VALENTINES ? 3
               : S.season == SEASON_FOOLS ? 4 : 5;
      snprintf(l, sizeof l, "\x01Seasonal event:\x02 %s%s%s%s", season_names[sn], a[0] ? " (" : "", a,
               a[0] ? " remaining)" : "");
      stat_line(&y, l, draw);
    }
    if (S.research_t > 0) {
      say_time(S.research_t, -1, a);
      snprintf(l, sizeof l, "\x01Research:\x02 %s remaining", a), stat_line(&y, l, draw);
    }
    if (wrath) snprintf(l, sizeof l, "\x01Grandmatriarchs status:\x02 %s", wrath), stat_line(&y, l, draw);
    if (S.pledge_t > 0) {
      say_time(S.pledge_t, -1, a);
      snprintf(l, sizeof l, "\x01Pledge:\x02 %s remaining", a), stat_line(&y, l, draw);
    }
    if (S.wrinklers_popped) snprintf(l, sizeof l, "\x01Wrinklers popped:\x02 %u", S.wrinklers_popped), stat_line(&y, l, draw);
    if (can_lumps() && S.lumps_total > -1) {
      beautify(S.lumps_total, 0, a);
      snprintf(l, sizeof l, "\x01Sugar lumps harvested:\x02 %s", a), stat_line(&y, l, draw);
    }
    if (S.reindeer_clicked) snprintf(l, sizeof l, "\x01Reindeer found:\x02 %u", S.reindeer_clicked), stat_line(&y, l, draw);
    if (has(U_A_FESTIVE_HAT)) {
      snprintf(l, sizeof l, "\x01Santa stages unlocked:\x02 %d/15 (%s)", S.santa_level + 1, santa_levels[S.santa_level]);
      stat_line(&y, l, draw);
    }
    if (has(U_A_CRUMBLY_EGG)) snprintf(l, sizeof l, "\x01" "Dragon training:\x02 %s", dragon_name()), stat_line(&y, l, draw);
  }
  if (S.prestige > 0) {
    stat_title(&y, "Prestige", draw);
    double hm = heavenly_multiplier();
    beautify(S.prestige, 0, a);
    beautify(hm * 100, 1, b);
    beautify(S.prestige * hm, 1, c);
    if (draw) gfx_icon(ICON(19, 7), PAD, y, 256);
    snprintf(l, sizeof l, "\x01Prestige level: %s\x02 at %s%% of its potential \x01(+%s%% CpS)\x02", a, b, c);
    int h = gfx_para(&PS_TIP, PAD + 28, y, SCREEN_W - 2 * PAD - 28, l, draw);
    beautify(S.heavenly_chips, 0, a);
    snprintf(l, sizeof l, "Heavenly chips: \x01%s\x02", a);
    h += gfx_para(&PS_TIP, PAD + 28, y + h, SCREEN_W - 2 * PAD - 28, l, draw);
    y += (h > 26 ? h : 26) + 2;
  }
  return y - y0;
}

static int achiev_count(int *other) {
  int n = 0;
  *other = 0;
  for (int a = 0; a < N_ACHIEVS; a++)
    if (has_achiev(a)) {
      if (achiev_rec(a)->pool == APOOL_NORMAL) n++;
      else (*other)++;
    }
  return n;
}

static const ParaStyle PS_HEAD = {F_BODY, F_BOLD, F_ITALIC, 0xCE79, WHITE, WHITE, 12, 0};
static int grid_head(int tab, int y, bool draw) {
  char l[200];
  int h = 0;
  if (tab == TAB_UPGRADES) {
    int total = 0, owned = 0;
    for (int u = 0; u < N_UPGRADES; u++) {
      const UpgradeRec *r = upgrade_rec(u);
      if (r->pool == POOL_PRESTIGE || r->pool == POOL_DEBUG || r->pool == POOL_TOGGLE) continue;
      total++;
      if (bit_get(S.upgrades_bought, u)) owned++;
    }
    snprintf(l, sizeof l, "\x01Upgrades unlocked:\x02 %d/%d (%d%%)", owned, total, total ? owned * 100 / total : 0);
  } else {
    int other, n = achiev_count(&other), total = 0;
    for (int a = 0; a < N_ACHIEVS; a++) total += achiev_rec(a)->pool == APOOL_NORMAL;
    int rank = (int)floorf(G.milk_progress);
    if (rank > N_MILKS - 1) rank = N_MILKS - 1;
    char o[32] = "";
    if (other) snprintf(o, sizeof o, " (+%d)", other);
    snprintf(l, sizeof l, "\x01" "Achievements unlocked:\x02 %d/%d (%d%%)%s\n\x01Milk:\x02 %s", n, total,
             total ? n * 100 / total : 0, o, res_text(milk_rec(rank)->name));
  }
  h = gfx_para(&PS_HEAD, PAD, y, SCREEN_W - 2 * PAD, l, draw);
  return h + 4;
}

/* where crate i of the grid is (y in the tab's content) */
static int grid_top(void) { return 30; }
static void crate_pos(int tab, int i, int head, int *x, int *y) {
  int sec = 0;
  for (int k = 1; k < 3; k++)
    if (i >= grid_break[k] && grid_break[k] > grid_break[k - 1]) sec = k;
  /* each section starts on a new row, with a gap */
  int yy = grid_top() + head, row0 = 0;
  for (int k = 0; k < sec; k++) {
    int n = grid_break[k + 1] - grid_break[k];
    if (tab == TAB_ACHIEVS && k == 1) n = ngrid - grid_break[1];
    if (n > 0) yy += ((n + GRID_COLS - 1) / GRID_COLS) * CRATE + 14;
  }
  row0 = grid_break[sec];
  int j = i - row0;
  *x = GRID_X + (j % GRID_COLS) * CRATE;
  *y = yy + 12 + (j / GRID_COLS) * CRATE;
}

static void draw_grid(int tab) {
  int head = grid_head(tab, 0, false);
  int y0 = PANEL_Y - U.list_top;
  grid_head(tab, y0 + grid_top(), true);
  static const char *const up_secs[3] = {"Prestige", "Upgrades", "Cookies"};
  int last_sec = -1;
  for (int i = 0; i < ngrid; i++) {
    int x, y;
    crate_pos(tab, i, head, &x, &y);
    int sec = 0;
    for (int k = 1; k < 3; k++)
      if (i >= grid_break[k]) sec = k;
    if (sec != last_sec) {
      last_sec = sec;
      const char *t = tab == TAB_UPGRADES ? up_secs[sec] : sec == 1 ? "Shadow achievements" : NULL;
      if (t && gfx_visible(0, y0 + y - 13, SCREEN_W, 13))
        gfx_text_outline(F_BOLD, GRID_X, y0 + y - 3, t, WHITE, 256);
    }
    if (!gfx_visible(x, y0 + y, CRATE, CRATE)) continue;
    bool sel = U.list_sel == i;
    /* (as the game's statistics show them: the icons alone, the heavenly ones in their slots) */
    int icon;
    if (tab == TAB_UPGRADES) {
      const UpgradeRec *r = upgrade_rec(grid[i]);
      icon = r->icon;
      if (r->pool == POOL_PRESTIGE) gfx_image_scaled(IM_ASCEND_SLOT0, x - 3, y0 + y - 3, 35, 35, 256);
    } else {
      int a = grid[i];
      icon = has_achiev(a) ? achiev_rec(a)->icon : ICON(0, 7);
    }
    gfx_icon(icon, x + 2, y0 + y + 2 - sel, tab == TAB_ACHIEVS && !has_achiev(grid[i]) ? 140 : 256);
    if (sel) gfx_rect(x, y0 + y, CRATE - 1, CRATE - 1, WHITE, 200);
  }
}

static void draw_stats(void) {
  panel(NULL);
  /* the tabs */
  int x = PAD;
  for (int t = 0; t < N_TABS; t++) {
    int w = gfx_text_width(F_NAME, tab_names[t]) + 12;
    bool on = U.stats_tab == t;
    gfx_round_rect(x, PANEL_Y + 6, w, 16, 3, on ? RGB(255, 255, 255) : BLACK, on ? 50 : 100);
    gfx_text_outline(F_NAME, x + 6, PANEL_Y + 18, tab_names[t], on ? WHITE : RGB(150, 150, 150), 256);
    x += w + 4;
  }
  gfx_text_right(F_SMALL, SCREEN_W - PAD, PANEL_Y + 17, "< >", RGB(150, 150, 150), 256);
  gfx_clip(0, PANEL_Y + 26, SCREEN_W, PANEL_H - 26);
  if (U.stats_tab == TAB_GENERAL) general(PANEL_Y + 26 - U.list_top, true);
  else {
    grid_build(U.stats_tab);
    draw_grid(U.stats_tab);
  }
  gfx_unclip();
  /* the tooltip of the crate chosen */
  if (U.stats_tab != TAB_GENERAL && ngrid > 0 && U.list_sel < ngrid) {
    int w = 220, pad = 5, type = U.stats_tab == TAB_UPGRADES ? 0 : 1;
    int h = crate_tip(type, grid[U.list_sel], CTX_STATS, 0, 0, w - 2 * pad, false) + 2 * pad;
    int head = grid_head(U.stats_tab, 0, false), cx, cy;
    crate_pos(U.stats_tab, U.list_sel, head, &cx, &cy);
    cy += PANEL_Y - U.list_top;
    int ty = cy + CRATE + 2;
    if (ty + h > SCREEN_H - 2) ty = cy - h - 2;
    if (ty < NEWS_H + 2) ty = NEWS_H + 2;
    int tx = cx + CRATE / 2 - w / 2;
    if (tx < 2) tx = 2;
    if (tx + w > SCREEN_W - 2) tx = SCREEN_W - 2 - w;
    if (gfx_visible(tx, ty, w, h)) {
      draw_frame_box(tx, ty, w, h);
      crate_tip(type, grid[U.list_sel], CTX_STATS, tx + pad, ty + pad, w - 2 * pad, true);
    }
  }
}

static void stats_keys(uint32_t p) {
  if (p & (K_LEFT | K_RIGHT) && (U.stats_tab == TAB_GENERAL || p & K_SHIFT)) {
    U.stats_tab = (U.stats_tab + (p & K_LEFT ? N_TABS - 1 : 1)) % N_TABS;
    U.list_sel = 0, U.list_top = 0;
    return;
  }
  if (U.stats_tab == TAB_GENERAL) {
    int h = general(0, false) + 30, view = PANEL_H - 26;
    if (p & K_DOWN) U.list_top += 24;
    if (p & K_UP) U.list_top -= 24;
    if (U.list_top > h - view) U.list_top = h - view;
    if (U.list_top < 0) U.list_top = 0;
    return;
  }
  grid_build(U.stats_tab);
  if (!ngrid) {
    if (p & (K_LEFT | K_RIGHT)) U.stats_tab = (U.stats_tab + (p & K_LEFT ? N_TABS - 1 : 1)) % N_TABS, U.list_sel = 0;
    return;
  }
  int i = U.list_sel;
  int head = grid_head(U.stats_tab, 0, false), x, y;
  crate_pos(U.stats_tab, i, head, &x, &y);
  if (p & K_LEFT) {
    if (i > 0) i--;
    else U.stats_tab = (U.stats_tab + N_TABS - 1) % N_TABS, U.list_sel = 0, U.list_top = 0;
  }
  if (p & K_RIGHT) {
    if (i < ngrid - 1) i++;
    else U.stats_tab = (U.stats_tab + 1) % N_TABS, U.list_sel = 0, U.list_top = 0;
  }
  if (U.list_sel != 0 || i != 0 || !(p & (K_LEFT | K_RIGHT))) {
    if (p & (K_UP | K_DOWN)) {
      /* the crate above or below: the nearest in x */
      int best = -1, bd = 1 << 30;
      for (int k = 0; k < ngrid; k++) {
        int kx, ky;
        crate_pos(U.stats_tab, k, head, &kx, &ky);
        if (p & K_UP ? ky >= y : ky <= y) continue;
        int d = abs(ky - y) * 4 + abs(kx - x);
        if (d < bd) bd = d, best = k;
      }
      if (best >= 0) i = best;
    }
    U.list_sel = i;
  }
  crate_pos(U.stats_tab, U.list_sel, head, &x, &y);
  int view = PANEL_H;
  if (y - U.list_top < 30) U.list_top = y - 30;
  if (y + CRATE - U.list_top > view - 4) U.list_top = y + CRATE - view + 4;
  if (U.list_sel < GRID_COLS) U.list_top = 0;
  if (U.list_top < 0) U.list_top = 0;
}

/* ------------------------------------------------------------------ info, keys */
static const char info_text[] =
  "\x01" "Cookie Clicker\x02 by Orteil and Opti (DashNet), version 2.058, on the NumWorks calculator. Its pictures, "
  "texts and numbers are the game's own (from its public mirror), as are its rules; the fonts are Merriweather, "
  "Kavoon and Noto Sans (SIL Open Font License). Not affiliated with DashNet.\x05"
  "\x01What's different here:\x02 the game counts its time while it runs (the calculator has no clock), so it "
  "doesn't bake while closed, and sugar lumps grow 24 times faster to make up for it. There is no sound. The "
  "golden cookies come to the left column.\x05"
  "Made by Mason Chen as part of NumPlay.";

static const char keys_text[] =
  "\x01OK\x02  click the big cookie\n"
  "\x01" "Arrows\x02  choose in the store\n"
  "\x01" "EXE\x02  buy (or sell) what is chosen\n"
  "\x01" "÷\x02  buy / sell     \x01" "×\x02  1, 10, 100 (all)\n"
  "\x01shift\x02  click a golden cookie (or a reindeer)\n"
  "\x01var\x02  pop a wrinkler     \x01x,n,t\x02  harvest the sugar lump\n"
  "\x01" "alpha\x02  the buildings and their minigames\n"
  "\x01toolbox\x02  statistics     \x01" "delete\x02  close a note\n"
  "\x01" "Back\x02  the menu     \x01Home\x02  save and quit";

static void text_page(const char *title, const char *s) {
  panel(title);
  char buf[1400];
  extern void text_beautify(const char *src, char *dst, int size);
  /* (× and ÷ as the fonts have them) */
  int o = 0;
  for (const unsigned char *c = (const unsigned char *)s; *c && o < (int)sizeof buf - 1; c++) {
    if (c[0] == 0xC3 && c[1] == 0x97) buf[o++] = (char)(0x80 + 18), c++;
    else if (c[0] == 0xC3 && c[1] == 0xB7) buf[o++] = (char)(0x80 + 19), c++;
    else buf[o++] = (char)*c;
  }
  buf[o] = 0;
  gfx_clip(0, PANEL_Y + 26, SCREEN_W, PANEL_H - 28);
  gfx_para(&PS_TIP, PAD + 4, PANEL_Y + 32 - U.list_top, SCREEN_W - 2 * PAD - 8, buf, true);
  gfx_unclip();
}

/* ------------------------------------------------------------------ Santa, the dragon */
static void draw_special(void) {
  int w = 236, x = (SCREEN_W - w) / 2, pad = 8;
  char l[400], a[200], c[64];
  int h;
  gfx_fill_alpha(0, PANEL_Y, SCREEN_W, PANEL_H, BLACK, 110);
  if (U.special == 0) {
    double cost = pow(S.santa_level + 1, S.santa_level + 1);
    beautify(cost, 0, a);
    bool can = S.cookies > cost && S.santa_level < 14;
    h = 150;
    int y = PANEL_Y + (PANEL_H - h) / 2;
    draw_frame_box(x, y, w, h);
    gfx_image(IM_SANTA0 + S.santa_level, x + w / 2 - 24, y + 8, 256);
    gfx_text_outline_center(F_TITLE, SCREEN_W / 2, y + 72, santa_levels[S.santa_level], WHITE, 256);
    tip_line(x + pad, y + 78, w - 2 * pad);
    if (S.santa_level < 14) {
      snprintf(l, sizeof l, "Evolve\n\x01" "cost:\x02 %s cookie%s", a, cost == 1 ? "" : "s");
      ParaStyle st = PS_TIP;
      st.align = 1;
      gfx_para(&st, x + pad, y + 84, w - 2 * pad, l, true);
      button(x + w / 2 - 50, y + h - 26, 100, "Evolve", U.list_sel == 0, can);
    } else {
      ParaStyle st = PS_TIP;
      st.align = 1;
      gfx_para(&st, x + pad, y + 86, w - 2 * pad, "Santa is fully grown!", true);
    }
    return;
  }
  /* the dragon */
  bool can;
  dragon_action(a, sizeof a, c, sizeof c, &can);
  int y = PANEL_Y + 6;
  h = PANEL_H - 12;
  draw_frame_box(x, y, w, h);
  float bob = sinf(G.real_t * 0.1f) * 2;
  gfx_image(IM_DRAGON0 + dragon_frame(), x + w / 2 - 24, y + 4 + (int)bob, 256);
  gfx_text_outline_center(F_TITLE, SCREEN_W / 2, y + 64, dragon_name(), WHITE, 256);
  tip_line(x + pad, y + 70, w - 2 * pad);
  ParaStyle st = PS_SMALL;
  st.align = 1;
  int yy = y + 74;
  if (c[0]) snprintf(l, sizeof l, "%s\n\x01" "Cost:\x02 %s", a, c);
  else snprintf(l, sizeof l, "%s", a);
  yy += gfx_para(&st, x + pad, yy, w - 2 * pad, l, true) + 4;
  int sel = 0;
  if (S.dragon_level < 27) button(x + w / 2 - 50, yy, 100, "Train", U.list_sel == sel, can), yy += 19;
  sel++;
  if (S.dragon_level >= 5) {
    for (int slot = 0; slot < (S.dragon_level >= 27 ? 2 : 1); slot++) {
      int aura = slot ? S.dragon_aura2 : S.dragon_aura;
      snprintf(l, sizeof l, "%s: %s", slot ? "Secondary aura" : "Dragon aura", aura_names[aura]);
      gfx_icon(aura_icons[aura], x + pad, yy - 4, 256);
      button(x + pad + 28, yy, w - 2 * pad - 28, l, U.list_sel == sel, true);
      yy += 23, sel++;
    }
  }
  if (S.dragon_level >= 4 && has(U_PET_THE_DRAGON)) button(x + w / 2 - 50, yy, 100, "Pet", U.list_sel == sel, true);
}

static int special_items(void) {
  if (U.special == 0) return 1;
  return 1 + (S.dragon_level >= 5 ? (S.dragon_level >= 27 ? 2 : 1) : 0) + (S.dragon_level >= 4 && has(U_PET_THE_DRAGON));
}

static void special_keys(uint32_t p) {
  int n = special_items();
  if (p & K_UP && U.list_sel > 0) U.list_sel--;
  if (p & K_DOWN && U.list_sel < n - 1) U.list_sel++;
  if (!(p & (K_OK | K_EXE))) return;
  if (U.special == 0) {
    upgrade_santa();
    return;
  }
  int auras = S.dragon_level >= 5 ? (S.dragon_level >= 27 ? 2 : 1) : 0;
  if (U.list_sel == 0) upgrade_dragon();
  else if (U.list_sel <= auras) {
    U.aura_slot = U.list_sel - 1;
    U.aura_sel = U.aura_slot ? S.dragon_aura2 : S.dragon_aura;
    U.overlay = OV_AURA;
  } else
    pet_dragon();
}

/* choosing an aura: those the dragon knows (one per level from 5) */
static int auras_known(void) {
  int n = S.dragon_level - 4;
  if (n > N_AURAS - 1) n = N_AURAS - 1;
  return n < 0 ? 0 : n;
}
static void draw_aura(void) {
  int n = auras_known() + 1;
  int w = 300, x = (SCREEN_W - w) / 2, pad = 8;
  int cols = 11, rows = (n + cols - 1) / cols;
  int h = 28 + rows * 26 + 74;
  int y = PANEL_Y + (PANEL_H - h) / 2;
  gfx_fill_alpha(0, PANEL_Y, SCREEN_W, PANEL_H, BLACK, 110);
  draw_frame_box(x, y, w, h);
  gfx_text_outline_center(F_TITLE, SCREEN_W / 2, y + 17, U.aura_slot ? "Set your secondary aura" : "Set your dragon's aura",
                          WHITE, 256);
  for (int i = 0; i < n; i++) {
    int cx = x + pad + (i % cols) * 26, cy = y + 24 + (i / cols) * 26;
    gfx_icon(aura_icons[i], cx, cy, (U.aura_slot ? S.dragon_aura == i : S.dragon_aura2 == i) && i ? 90 : 256);
    if (U.aura_sel == i) gfx_rect(cx - 1, cy - 1, 26, 26, WHITE, 256);
  }
  int yy = y + 28 + rows * 26;
  tip_line(x + pad, yy - 2, w - 2 * pad);
  gfx_text_outline_center(F_NAME, SCREEN_W / 2, yy + 10, aura_names[U.aura_sel], WHITE, 256);
  ParaStyle st = PS_SMALL;
  st.align = 1;
  char l[400];
  int highest = -1;
  for (int b = 0; b < NB; b++)
    if (S.buildings[b].amount > 0) highest = b;
  snprintf(l, sizeof l, "%s\n%s", aura_descs[U.aura_sel],
           highest >= 0 ? "Switching auras costs \x01one of your most expensive buildings\x02." : "");
  for (char *c = l; *c; c++)
    if (*c == '\n' && !c[1]) *c = 0;
  if (highest >= 0) {
    char t[96];
    snprintf(t, sizeof t, " (%s)", res_text(building_rec(highest)->single));
    strncat(l, t, sizeof l - strlen(l) - 1);
  }
  gfx_para(&st, x + pad, yy + 14, w - 2 * pad, l, true);
}
static void aura_keys(uint32_t p) {
  int n = auras_known() + 1;
  if (p & K_LEFT && U.aura_sel > 0) U.aura_sel--;
  if (p & K_RIGHT && U.aura_sel < n - 1) U.aura_sel++;
  if (p & K_UP && U.aura_sel >= 11) U.aura_sel -= 11;
  if (p & K_DOWN && U.aura_sel + 11 < n) U.aura_sel += 11;
  if (p & (K_OK | K_EXE)) {
    int other = U.aura_slot ? S.dragon_aura : S.dragon_aura2;
    if (U.aura_sel == 0 || U.aura_sel != other) set_dragon_aura(U.aura_slot, U.aura_sel);
    U.overlay = OV_SPECIAL;
  }
}

/* ------------------------------------------------------------------ dispatch */
void draw_overlay(void) {
  switch (U.overlay) {
  case OV_MENU: draw_menu(); break;
  case OV_OPTIONS: draw_options(); break;
  case OV_STATS: draw_stats(); break;
  case OV_INFO: text_page("Info", info_text); break;
  case OV_KEYS: text_page("Keys", keys_text); break;
  case OV_SPECIAL: draw_special(); break;
  case OV_AURA: draw_aura(); break;
  case OV_ROWMENU: {
    extern void draw_row_menu(void);
    draw_row_menu();
    break;
  }
  default: break;
  }
}

void menus_keys(uint32_t p) {
  if (p & K_BACK) {
    switch (U.overlay) {
    case OV_KEYS: S.seen_keys = 1, U.overlay = U.from_menu ? OV_MENU : OV_NONE; break;
    case OV_AURA: U.overlay = OV_SPECIAL; break;
    case OV_MENU: case OV_ROWMENU: U.overlay = OV_NONE; break;
    default: U.overlay = U.from_menu ? OV_MENU : OV_NONE;
    }
    return;
  }
  switch (U.overlay) {
  case OV_MENU:
    menu_build();
    if (p & K_UP) U.menu_sel = (U.menu_sel + nmenu - 1) % nmenu;
    if (p & K_DOWN) U.menu_sel = (U.menu_sel + 1) % nmenu;
    if (U.menu_sel >= nmenu) U.menu_sel = nmenu - 1;
    if (p & (K_OK | K_EXE)) U.from_menu = true, menu_choose(menu_items[U.menu_sel]);
    break;
  case OV_OPTIONS: options_keys(p); break;
  case OV_STATS: stats_keys(p); break;
  case OV_INFO:
  case OV_KEYS:
    if (p & K_DOWN) U.list_top += 24;
    if (p & K_UP) U.list_top -= 24;
    if (U.list_top < 0) U.list_top = 0;
    if (U.list_top > 120) U.list_top = 120;
    if (p & (K_OK | K_EXE) && U.overlay == OV_KEYS && !S.seen_keys) S.seen_keys = 1, U.overlay = OV_NONE;
    break;
  case OV_SPECIAL: special_keys(p); break;
  case OV_AURA: aura_keys(p); break;
  case OV_ROWMENU: {
    extern void row_menu_keys(uint32_t p);
    row_menu_keys(p);
    break;
  }
  default: U.overlay = OV_NONE;
  }
}
