/* The store, as the game's: its title and buy / sell, 1 / 10 / 100 buttons; upgrades as icons in
 * their frames (all of them shown while one is chosen, a row otherwise), switches and research;
 * then the buildings on their wooden tiles, with their picture, name, price and how many are owned.
 * The tooltip of what is chosen shows over the left column, as the game's does beside the store. */
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "ui.h"

#define SECTION_GAP 5
extern void text_beautify(const char *src, char *dst, int size);
extern void upgrade_description(int u, char *out, int size);
extern bool can_lumps(void);

typedef struct {
  int sec, idx, id;          /* idx: within its section; id: the upgrade or building */
  int x, y, w, h;            /* (y in the store's content, before scrolling) */
} Item;
static Item items[N_UPGRADES + NB + 8];
static int nitems, content_h;
static int sec_first[N_SEC], sec_count[N_SEC], sec_top[N_SEC], sec_h[N_SEC];
static int buildings_shown;

/* the buildings the store lists: those unlocked, and the next two (as ???) */
static int building_state(int b) {   /* 0 hidden, 1 ???, 2 shown */
  int last_locked = 0;
  for (int i = 0; i <= b; i++) {
    bool unlocked = S.cookies_earned >= building_rec(i)->price || S.buildings[i].bought > 0;
    if (unlocked) last_locked = 0;
    else last_locked++;
    if (i == B_YOU && unlocked) win(A_COOKIE_CLICKER);
  }
  return last_locked == 0 ? 2 : last_locked <= 2 ? 1 : 0;
}

void ui_store_layout(void) {
  nitems = 0;
  int y = 0;
  int counts[3] = {G.nstore, G.nstore_toggle, G.nstore_tech};
  int base = 0;
  for (int s = 0; s < 3; s++) {
    sec_first[s] = nitems, sec_count[s] = counts[s], sec_top[s] = y;
    if (!counts[s]) {
      sec_h[s] = 0;
      base += counts[s];
      continue;
    }
    bool open = U.sec == s || s > 0;
    int rows = (counts[s] + CRATES_PER_ROW - 1) / CRATES_PER_ROW;
    if (!open) rows = 1;
    int top = y + 3;
    for (int i = 0; i < counts[s]; i++) {
      int r = i / CRATES_PER_ROW;
      Item *it = &items[nitems++];
      it->sec = s, it->idx = i, it->id = G.store[base + i];
      it->x = STORE_X + (i % CRATES_PER_ROW) * CRATE, it->y = top + r * CRATE, it->w = CRATE, it->h = CRATE;
      if (r >= rows) it->h = 0;   /* (folded away) */
    }
    y = top + rows * CRATE + 2;
    sec_h[s] = y - sec_top[s];
    y += SECTION_GAP;
    base += counts[s];
  }
  sec_first[SEC_BUILDINGS] = nitems, sec_top[SEC_BUILDINGS] = y;
  int n = 0;
  for (int b = 0; b < NB; b++) {
    if (!building_state(b)) break;
    Item *it = &items[nitems++];
    it->sec = SEC_BUILDINGS, it->idx = n++, it->id = b;
    it->x = STORE_X, it->y = y, it->w = STORE_W, it->h = PRODUCT_H;
    y += PRODUCT_H;
  }
  sec_count[SEC_BUILDINGS] = n, sec_h[SEC_BUILDINGS] = y - sec_top[SEC_BUILDINGS];
  buildings_shown = n;
  content_h = y;
  /* keep the selection on something that is there */
  if (U.sec < SEC_BUILDINGS && sec_count[U.sec] == 0) U.sec = SEC_BUILDINGS, U.idx = 0;
  if (U.idx >= sec_count[U.sec]) U.idx = sec_count[U.sec] - 1;
  if (U.idx < 0) U.idx = 0;
}

static Item *selected_item(void) {
  if (sec_count[U.sec] == 0) return NULL;
  return &items[sec_first[U.sec] + U.idx];
}
int store_selected_upgrade(void) {
  Item *it = selected_item();
  return it && it->sec < SEC_BUILDINGS ? it->id : -1;
}
int store_selected_building(void) {
  Item *it = selected_item();
  return it && it->sec == SEC_BUILDINGS ? it->id : -1;
}

int store_selected_y(int *h) {
  Item *it = selected_item();
  *h = it ? it->h : 0;
  return it ? it->y : 0;
}
int store_content_h(void) { return content_h; }
int store_buildings_top(void) { return sec_top[SEC_BUILDINGS]; }

/* the arrows in the store */
static void move(int dx, int dy) {
  ui_store_layout();
  if (dy) {
    if (U.sec < SEC_BUILDINGS && sec_count[U.sec]) {
      int col = U.idx % CRATES_PER_ROW, row = U.idx / CRATES_PER_ROW;
      int rows = (sec_count[U.sec] + CRATES_PER_ROW - 1) / CRATES_PER_ROW;
      if (row + dy >= 0 && row + dy < rows) {
        int ni = (row + dy) * CRATES_PER_ROW + col;
        U.idx = ni < sec_count[U.sec] ? ni : sec_count[U.sec] - 1;
        ui_store_layout();
        return;
      }
      int s = U.sec + dy;
      while (s >= 0 && s < N_SEC && sec_count[s] == 0) s += dy;
      if (s < 0 || s >= N_SEC) return;
      U.sec = s;
      if (s == SEC_BUILDINGS) U.idx = dy > 0 ? 0 : sec_count[s] - 1;
      else {
        int r2 = dy > 0 ? 0 : (sec_count[s] - 1) / CRATES_PER_ROW;
        int ni = r2 * CRATES_PER_ROW + col;
        U.idx = ni < sec_count[s] ? ni : sec_count[s] - 1;
      }
      return;
    }
    int ni = U.idx + dy;
    if (ni >= 0 && ni < sec_count[SEC_BUILDINGS]) {
      U.idx = ni;
      return;
    }
    if (ni < 0) {
      int s = SEC_BUILDINGS - 1;
      while (s >= 0 && sec_count[s] == 0) s--;
      if (s < 0) return;
      U.sec = s;
      int rows = (sec_count[s] + CRATES_PER_ROW - 1) / CRATES_PER_ROW;
      U.idx = (rows - 1) * CRATES_PER_ROW;
    }
    return;
  }
  if (dx && U.sec < SEC_BUILDINGS) {
    int ni = U.idx + dx;
    if (ni >= 0 && ni < sec_count[U.sec]) U.idx = ni;
  }
}

void store_move(int dx, int dy) {
  move(dx, dy);
  ui_store_layout();   /* (the upgrades open while one is chosen) */
}

/* ------------------------------------------------------------------ drawing */
void draw_crate(int icon, int x, int y, int state, bool shadow) {
  gfx_fill_alpha(x + 3, y + 3, CRATE - 6, CRATE - 6, BLACK, 64);
  gfx_icon(icon, x + 2, y + 2, 256);
  gfx_image_part(IM_UPGRADE_FRAME, state * 29, shadow ? 29 : 0, 29, 29, x, y, 256);
}

void draw_price(int x, int y, double price, bool can, int money_im) {
  char t[48];
  beautify(round(price), 0, t);
  gfx_image(money_im, x, y - 8, 256);
  gfx_text_outline(F_SMALL, x + 12, y, t, can ? RGB(102, 255, 102) : RGB(255, 102, 102), 256);
}

static void draw_product(const Item *it, int y, bool sel) {
  int b = it->id, state = building_state(b);
  const BuildingRec *r = building_rec(b);
  int tile = it->idx % 4 == 0 ? 1 : it->idx % 4 == 1 ? 2 : it->idx % 4 == 2 ? 3 : 0;
  gfx_image(IM_STORE_TILE0 + tile, STORE_X, y, 256);
  double price;
  int n;
  building_bulk(b, &price, &n);
  bool enabled = G.buy_mode == 1 ? S.cookies >= price : S.buildings[b].amount > 0;
  bool unlocked = state == 2;
  int icon = IM_BUILD_CURSOR + 2 * b;   /* (each building: its picture, then its silhouette) */
  if (unlocked) gfx_image(icon, STORE_X, y, 256);
  else gfx_image(icon + 1, STORE_X, y, 256);
  const char *name = unlocked ? res_text(r->name) : "???";
  int font = gfx_text_width(F_NAME, name) > STORE_W - 70 ? F_NAME_SMALL : F_NAME;
  gfx_text_outline(F_NAME == font ? F_NAME : F_NAME_SMALL, STORE_X + 34, y + 13, name, WHITE, 256);
  draw_price(STORE_X + 34, y + 27, price, enabled, IM_MONEY);
  if (G.buy_mode == -1 || G.buy_bulk != 1) {
    char t[16];
    snprintf(t, sizeof t, "x%d", n);
    int px = STORE_X + 34 + 12 + gfx_text_width(F_SMALL, "0") + 4;
    char pt[48];
    beautify(round(price), 0, pt);
    px = STORE_X + 34 + 12 + gfx_text_width(F_SMALL, pt) + 4;
    gfx_text_outline(F_SMALL, px, y + 27, t, RGB(255, 255, 204), 256);
  }
  if (unlocked && S.buildings[b].amount > 0) {
    char t[24];
    beautify(S.buildings[b].amount, 0, t);
    int x = STORE_X + STORE_W - 5;
    gfx_text_right(F_OWNED, x, y + 26, t, WHITE, 40);
    gfx_text_right(F_OWNED, x, y + 25, t, BLACK, 70);
  }
  if (unlocked && S.buildings[b].level > 0) {
    char t[16];
    snprintf(t, sizeof t, "lvl %d", S.buildings[b].level);
    gfx_text_outline(F_SMALL, STORE_X + STORE_W - 4 - gfx_text_width(F_SMALL, t), y + 9, t, RGB(255, 255, 204), 200);
  }
  if (!(unlocked && enabled)) gfx_fill_alpha(STORE_X, y, STORE_W, PRODUCT_H, BLACK, 102);
  if (G.buy_mode == -1) {
    gfx_hgrad(STORE_X, y, 8, PRODUCT_H, RGB(204, 0, 0), 150, 0);
    gfx_hgrad(STORE_X + STORE_W - 8, y, 8, PRODUCT_H, RGB(204, 0, 0), 0, 150);
  }
  if (sel) {   /* the game's hover: a warm glow inside, brighter */
    gfx_fill_alpha(STORE_X, y, STORE_W, PRODUCT_H, WHITE, 26);
    for (int k = 0; k < 6; k++) gfx_rect(STORE_X + k, y + k, STORE_W - 2 * k, PRODUCT_H - 2 * k, RGB(255, 238, 221), 110 - k * 18);
  }
}

void draw_store(void) {
  if (!gfx_visible(STORE_X - 4, NEWS_H, STORE_W + 4, SCREEN_H - NEWS_H)) return;
  /* the dark column between the cookie and the store */
  for (int y = NEWS_H; y < SCREEN_H; y += 128) gfx_image(IM_PANEL_V, LEFT_W, y, 256);
  gfx_clip(STORE_X, NEWS_H, STORE_W, SCREEN_H - NEWS_H);
  gfx_pattern(IM_DARK_NOISE, STORE_X, NEWS_H, STORE_W, SCREEN_H - NEWS_H, 128, 128, 0, 0, 256);
  gfx_fill_alpha(STORE_X, NEWS_H, STORE_W, SCREEN_H - NEWS_H, BLACK, 128);
  int scroll = (int)U.scroll;
  int base = STORE_TOP - scroll;
  gfx_clip(STORE_X, STORE_TOP, STORE_W, SCREEN_H - STORE_TOP);
  for (int s = 1; s < 3; s++) {   /* (switches and research: under a line, as the game has them) */
    if (!sec_count[s]) continue;
    int y = base + sec_top[s];
    for (int x = STORE_X; x < SCREEN_W; x += 128) gfx_image(IM_PANEL_H, x, y - 2, 256);
  }
  for (int i = 0; i < nitems; i++) {
    const Item *it = &items[i];
    if (it->h == 0) continue;
    int y = base + it->y;
    if (!gfx_visible(it->x, y, it->w, it->h)) continue;
    bool sel = it->sec == U.sec && it->idx == U.idx;
    if (it->sec < SEC_BUILDINGS) {
      bool can = upgrade_can_buy(it->id) || bit_get(S.upgrades_bought, it->id);
      draw_crate(upgrade_rec(it->id)->icon, it->x, y, sel ? 1 : can ? 0 : 2, false);
      if (bit_get(S.upgrades_bought, it->id)) gfx_fill_alpha(it->x + 2, y + 2, 25, 25, RGB(255, 255, 255), 30);
    } else
      draw_product(it, y, sel);
  }
  if (sec_count[SEC_BUILDINGS])
    for (int x = STORE_X; x < SCREEN_W; x += 128) gfx_image(IM_PANEL_H, x, base + sec_top[SEC_BUILDINGS] - 3, 256);
  gfx_unclip();
  /* the title: Store, buy / sell, 1 / 10 / 100 */
  gfx_pattern(IM_DARK_NOISE, STORE_X, NEWS_H, STORE_W, 14, 128, 128, 0, 0, 256);
  gfx_vgrad(STORE_X, NEWS_H, STORE_W, 14, BLACK, 60, 160);
  gfx_text_outline(F_NAME, STORE_X + 3, NEWS_H + 11, "Store", WHITE, 256);
  const char *opts[5] = {"Buy", "Sell", "1", "10", "100"};
  int x = SCREEN_W - 3;
  for (int i = 4; i >= 0; i--) {
    bool on = i == 0 ? G.buy_mode == 1 : i == 1 ? G.buy_mode == -1
                                                : (G.buy_bulk == (i == 2 ? 1 : i == 3 ? 10 : 100)) ||
                                                      (i == 4 && G.buy_bulk == -1);
    if (i == 4 && G.buy_mode == -1 && G.buy_bulk == -1) opts[4] = "all";
    int w = gfx_text_width(F_SMALL, opts[i]);
    x -= w + 4;
    if (on) gfx_round_rect(x - 2, NEWS_H + 2, w + 4, 11, 3, RGB(255, 255, 255), 40);
    gfx_text(F_SMALL, x, NEWS_H + 10, opts[i], on ? WHITE : RGB(140, 140, 140), 256);
    if (i == 2) x -= 5;
  }
  gfx_unclip();
}

/* ------------------------------------------------------------------ the tooltip */
const ParaStyle PS_TIP = {F_BODY, F_BOLD, F_ITALIC, 0xCE79 /* #ccc */, WHITE, WHITE, 12, 0};
const ParaStyle PS_QUOTE = {F_ITALIC, F_ITALIC, F_ITALIC, 0x8410, 0x8410, 0x8410, 12, 0};
const ParaStyle PS_SMALL = {F_BODY, F_BOLD, F_ITALIC, 0xCE79, WHITE, WHITE, 11, 0};

void draw_frame_box(int x, int y, int w, int h) {
  gfx_pattern(IM_DARK_NOISE, x, y, w, h, 128, 128, 0, 0, 256);
  gfx_image_scaled(IM_BORDER_SOFT, (float)x, (float)y, (float)w, (float)h, 256);
  /* the gold frame (frameBorder.png's colours) */
  gfx_rect(x, y, w, h, 0xEF16 /* #ece2b6 */, 256);
  gfx_fill(x + 1, y + h - 2, w - 2, 1, 0x7144);
  gfx_fill(x + w - 2, y + 1, 1, h - 2, 0x8AA4);
  gfx_fill(x + 1, y + 1, w - 2, 1, 0xDDF3);
  gfx_fill(x + 1, y + 1, 1, h - 2, 0xDDF3);
  gfx_rect(x + 2, y + 2, w - 4, h - 4, BLACK, 160);
}

/* what a tooltip holds, measured (draw false) or drawn */
static int tooltip_body(int x, int y, int w, bool draw) {
  int u = store_selected_upgrade(), b = store_selected_building();
  char buf[1400], line[200];
  int yy = y;
  if (u >= 0) return crate_tip(0, u, CTX_STORE, x, y, w, draw);
  if (b >= 0) {
    const BuildingRec *r = building_rec(b);
    bool locked = S.cookies_earned < r->price && S.buildings[b].bought == 0;
    const Building *m = &S.buildings[b];
    double price;
    int n;
    building_bulk(b, &price, &n);
    bool can = G.buy_mode == 1 ? S.cookies >= price : m->amount > 0;
    if (draw) {
      if (locked) gfx_icon(ICON(0, 7), x, yy, 256);
      else gfx_icon(ICON(r->column, 0), x, yy, 256);
      char t[48];
      beautify(round(price), 0, t);
      int pw = gfx_text_width(F_SMALL, t) + 12;
      gfx_image(IM_MONEY, x + w - pw, yy, 256);
      gfx_text_outline(F_SMALL, x + w - pw + 12, yy + 8, t, can ? RGB(102, 255, 102) : RGB(255, 102, 102), 256);
    }
    const char *name = locked ? "???" : res_text(r->name);
    if (draw) {
      gfx_text_outline(F_NAME, x + 27, yy + 11, name, WHITE, 256);
      char tag[32];
      snprintf(tag, sizeof tag, "owned: %d", m->amount);
      int tw = gfx_text_width(F_SMALL, tag) + 6;
      gfx_round_rect(x + 27, yy + 14, tw, 10, 2, RGB(204, 204, 204), 210);
      gfx_text(F_SMALL, x + 30, yy + 22, tag, BLACK, 256);
      if (m->free > 0) {
        snprintf(tag, sizeof tag, "free: %d!", m->free);
        int tw2 = gfx_text_width(F_SMALL, tag) + 6;
        gfx_round_rect(x + 29 + tw, yy + 14, tw2, 10, 2, RGB(204, 204, 204), 210);
        gfx_text(F_SMALL, x + 32 + tw, yy + 22, tag, BLACK, 256);
      }
    }
    yy += 27;
    if (draw) {
      gfx_hgrad(x, yy, w / 2, 1, BLACK, 0, 110);
      gfx_hgrad(x + w / 2, yy, w - w / 2, 1, BLACK, 110, 0);
    }
    yy += 4;
    snprintf(buf, sizeof buf, CH_LQUOTE "%s" CH_RQUOTE, locked ? "???" : res_text(r->desc));
    ParaStyle qs = PS_QUOTE;
    yy += gfx_para(&qs, x, yy, w, buf, draw) + 3;
    if (m->total_cookies > 0) {
      if (draw) {
        gfx_hgrad(x, yy, w / 2, 1, BLACK, 0, 110);
        gfx_hgrad(x + w / 2, yy, w - w / 2, 1, BLACK, 110, 0);
      }
      yy += 3;
      char a[48], c[48], d[48];
      if (m->amount > 0) {
        cookies_str(m->stored_total_cps / m->amount * G.global_cps_mult, 1, a);
        snprintf(line, sizeof line, CH_BULLET " each %s produces \x01%s\x02 per second", res_text(r->single), a);
        yy += gfx_para(&PS_SMALL, x, yy, w, line, draw) + 2;
      }
      beautify(m->amount, 0, a);
      cookies_str(m->stored_total_cps * G.global_cps_mult, 1, c);
      double pct = G.cookies_ps > 0 && m->amount > 0 ? m->stored_total_cps * G.global_cps_mult / G.cookies_ps * 100 : 0;
      beautify(pct, 1, d);
      snprintf(line, sizeof line, CH_BULLET " %s %s producing \x01%s\x02 per second (\x01%s%%\x02 of total CpS)", a,
               m->amount == 1 ? res_text(r->single) : res_text(r->plural), c, d);
      yy += gfx_para(&PS_SMALL, x, yy, w, line, draw) + 2;
      beautify(m->total_cookies, 0, a);
      snprintf(line, sizeof line, CH_BULLET " \x01%s\x02 %s %s so far", a,
               floor(m->total_cookies) == 1 ? "cookie" : "cookies", res_text(r->action));
      yy += gfx_para(&PS_SMALL, x, yy, w, line, draw) + 2;
    }
    {
      if (can_lumps()) {
        if (draw) {
          gfx_hgrad(x, yy, w / 2, 1, BLACK, 0, 110);
          gfx_hgrad(x + w / 2, yy, w - w / 2, 1, BLACK, 110, 0);
        }
        yy += 3;
        snprintf(line, sizeof line, "\x01Level %d %s\x02 (+%d%% CpS). [toolbox] levels up for %d sugar lump%s.",
                 m->level, res_text(r->plural), m->level, m->level + 1, m->level ? "s" : "");
        yy += gfx_para(&PS_SMALL, x, yy, w, line, draw) + 2;
      }
    }
    return yy - y;
  }
  return 0;
}

void draw_tooltip(void) {
  if (U.tip_t <= 0 || U.overlay || U.view != VIEW_STORE) return;
  Item *it = selected_item();
  if (!it) return;
  int w = LEFT_W + 6, x = 1, pad = 5;
  int h = tooltip_body(x + pad, 0, w - 2 * pad, false) + 2 * pad;
  int iy = STORE_TOP - (int)U.scroll + it->y;
  int y = iy + it->h / 2 - h / 2;
  if (y < NEWS_H + 2) y = NEWS_H + 2;
  if (y + h > SCREEN_H - 2) y = SCREEN_H - 2 - h;
  if (!gfx_visible(x, y, w, h)) return;
  draw_frame_box(x, y, w, h);
  tooltip_body(x + pad, y + pad, w - 2 * pad, true);
}
