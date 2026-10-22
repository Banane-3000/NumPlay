/* The menus: the title, how to drive, the garage (vehicles and their upgrades) and the stages. */
#include <stdio.h>
#include <string.h>
#include "game.h"
#include "gfx.h"

static int row;            /* the garage: 0 the vehicle, 1 its upgrades */
static int up_sel;         /* the upgrade chosen */
static int ask;            /* buying: 0 not asked, 1 the vehicle, 2 the stage */
static int shown;          /* the vehicle or stage on show (it may not be owned) */
static int flash;          /* frames left of a "not enough coins" */
static int bought;         /* frames left of the sparkle of something bought */
static int t;              /* frames on this screen */

/* the garage goes through the vehicles in the game's order */
static int vehicle_index(int v) {
  for (int i = 0; i < NVEHICLES; i++)
    if (vehicle_order[i] == v) return i;
  return 0;
}
static int vehicle_step(int v, int d) { return vehicle_order[(vehicle_index(v) + d + NVEHICLES) % NVEHICLES]; }

void menu_enter(int mode) {
  g_mode = mode, ask = 0, t = 0, flash = 0;
  if (mode == MODE_GARAGE) shown = sv.vehicle, row = 0;
  if (mode == MODE_STAGES) shown = sv.stage;
}

static void begin(void) {
  if (!sv.seen_help) {
    menu_enter(MODE_HELP);
    return;
  }
  run_start(sv.vehicle, sv.stage);
  g_mode = MODE_RUN;
}

static bool pay(uint32_t price) {
  if (sv.coins < price) {
    flash = 40;
    return false;
  }
  sv.coins -= price;
  bought = 30;
  save_write();
  return true;
}

void menu_frame(uint32_t keys, uint32_t pressed) {
  (void)keys;
  t++;
  if (flash) flash--;
  if (bought) bought--;
  if (ask) {
    if (pressed & K_OK) {
      if (ask == 1 && pay(vehicles[shown].price)) sv.vehicles |= (uint16_t)(1u << shown), sv.vehicle = (uint8_t)shown;
      if (ask == 2 && pay(stages[shown].price)) sv.stages |= (uint16_t)(1u << shown), sv.stage = (uint8_t)shown;
      save_write();
      ask = 0;
    } else if (pressed & K_BACK) {
      ask = 0;
    }
    return;
  }
  switch (g_mode) {
    case MODE_TITLE:
      if (pressed & K_OK && t > 10) menu_enter(MODE_GARAGE);
      else if (pressed & K_UP) menu_enter(MODE_HELP);
      else if (pressed & K_BACK && t > 10) game_quit();
      break;
    case MODE_HELP:
      if (pressed & (K_OK | K_BACK) && t > 15) {
        bool first = !sv.seen_help;
        sv.seen_help = 1;
        save_write();
        if (first && pressed & K_OK) begin();
        else menu_enter(MODE_TITLE);
      }
      break;
    case MODE_GARAGE: {
      bool owned = sv.vehicles >> shown & 1;
      if (row == 0) {
        if (pressed & K_LEFT) shown = vehicle_step(shown, -1), t = 0;
        if (pressed & K_RIGHT) shown = vehicle_step(shown, 1), t = 0;
        owned = sv.vehicles >> shown & 1;
        if (owned) sv.vehicle = (uint8_t)shown;
        if (pressed & K_DOWN && owned) row = 1;
        if (pressed & K_OK) {
          if (owned) menu_enter(MODE_STAGES);
          else if (sv.coins >= vehicles[shown].price) ask = 1;
          else flash = 40;
        }
        if (pressed & K_BACK) {
          if (!owned) shown = sv.vehicle;
          menu_enter(MODE_TITLE);
        }
      } else {
        if (pressed & K_LEFT) up_sel = (up_sel + 3) % 4;
        if (pressed & K_RIGHT) up_sel = (up_sel + 1) % 4;
        if (pressed & (K_UP | K_BACK)) row = 0;
        if (pressed & K_OK) {
          uint8_t *lv = &sv.level[shown][up_sel];
          if (*lv < vehicles[shown].max[up_sel] && pay(upgrade_cost(shown, up_sel, *lv + 1))) {
            (*lv)++;
            save_write();
          }
        }
      }
      break;
    }
    case MODE_STAGES: {
      if (pressed & K_LEFT) shown = (shown + NSTAGES - 1) % NSTAGES, t = 0;
      if (pressed & K_RIGHT) shown = (shown + 1) % NSTAGES, t = 0;
      bool owned = sv.stages >> shown & 1;
      if (owned) sv.stage = (uint8_t)shown;
      if (pressed & K_OK) {
        if (owned) begin();
        else if (sv.coins >= stages[shown].price) ask = 2;
        else flash = 40;
      }
      if (pressed & K_BACK) {
        shown = sv.stage;
        menu_enter(MODE_GARAGE);
      }
      break;
    }
  }
}

/* ------------------------------------------------------------------ drawing */
static void backdrop(const char *title) {
  gfx_image_box(IMG_MENU_BG, 0, 0, SCREEN_W, SCREEN_H, 255);
  if (title) {
    gfx_fill(0, 0, SCREEN_W, 30, rgb565(0x2B2E33), 235);
    gfx_fill(0, 30, SCREEN_W, 2, rgb565(0x15171A), 255);
    gfx_text(FONT_L, 10, 24, title, C_WHITE, C_BLACK, 0);
  }
  draw_coins_counter(312, 22, sv.coins, 2);
}

/* the strip over a picker: every vehicle or stage small, in order, the one on show in a bright frame, the ones not
 * owned dimmed; n of them, thumbs from first_img, which id each slot is, the one on show, owned bits */
static void strip(int n, int first_img, const uint8_t *ids, int on, uint32_t owned) {
  int slot = 24, x0 = (SCREEN_W - n * slot) / 2, y = 43;
  gfx_fill(0, 32, SCREEN_W, 22, C_BLACK, 120);
  for (int i = 0; i < n; i++) {
    int id = ids ? ids[i] : i, cx = x0 + i * slot + slot / 2;
    bool sel = id == on, have = owned >> id & 1;
    if (sel) {
      gfx_rrect(cx - 14, y - 11, 28, 22, 5, rgb565(0xFFD21F), 255);
      gfx_rrect(cx - 11, y - 8, 22, 16, 3, rgb565(0x23262B), 255);
    }
    const Img *im = img(first_img + id);
    float k = 21.0f / (float)im->w < 1 ? 21.0f / (float)im->w : 1;
    gfx_image(first_img + id, (float)cx, (float)y, k, 0, have || sel ? 255 : 80);
  }
}

/* the key line at the bottom: plain words */
static void keys(const char *s, bool warn) {
  gfx_text(FONT_S, 160, 236, s, warn ? rgb565(0xFF5A4A) : rgb565(0xD8DEE6), C_BLACK, 1);
}

static void arrows(int y) {
  int dx = (t / 8) % 2;
  gfx_text(FONT_XL, 14 - dx, y, "<", C_WHITE, C_BLACK, 1);
  gfx_text(FONT_XL, 306 + dx, y, ">", C_WHITE, C_BLACK, 1);
}

static void price_tag(int x, int y, uint32_t price, bool can) {
  char n[20];
  format_number(n, price);
  int w = gfx_text_width(FONT_M, n) + 22;
  gfx_image(IMG_COIN_ICON, (float)(x - w / 2 + 8), (float)(y - 5), 1, 0, 255);
  gfx_text(FONT_M, x - w / 2 + 20, y, n, can ? rgb565(0xFFE14A) : rgb565(0xFF5A4A), C_BLACK, 0);
}

static void ask_box(const char *what, uint32_t price) {
  char t2[48], n[20];
  gfx_fill(0, 0, SCREEN_W, SCREEN_H, C_BLACK, 140);
  gfx_rrect(56, 70, 208, 104, 10, rgb565(0x2A2D33), 245);
  snprintf(t2, sizeof t2, "Buy %s?", what);
  gfx_text(FONT_L, 160, 100, t2, C_WHITE, C_BLACK, 1);
  format_number(n, price);
  price_tag(160, 126, price, true);
  (void)n;
  gfx_rrect(76, 138, 80, 26, 6, rgb565(0x3DAA3D), 255);
  gfx_rrect(164, 138, 80, 26, 6, rgb565(0x8A2E2E), 255);
  gfx_text(FONT_M, 116, 157, "OK: BUY", C_WHITE, C_BLACK, 1);
  gfx_text(FONT_M, 204, 157, "BACK", C_WHITE, C_BLACK, 1);
}

static void title_draw(void) {
  gfx_image_box(IMG_MENU_BG, 0, 0, SCREEN_W, SCREEN_H, 255);
  gfx_image(IMG_LOGO, 160, 62, 1, 0, 255);
  gfx_image(IMG_ICON_JEEP, 160, 146, 1.1f, 0, 255);
  if ((t / 25) % 2 == 0) gfx_text(FONT_L, 160, 212, "PRESS OK TO PLAY", C_WHITE, C_BLACK, 1);
  gfx_text(FONT_S, 6, 234, "Up: how to drive", rgb565(0xB8C0C8), C_BLACK, 0);
  gfx_text(FONT_S, 314, 234, "Back: quit", rgb565(0xB8C0C8), C_BLACK, 2);
  draw_coins_counter(312, 20, sv.coins, 2);
}

static void help_draw(void) {
  backdrop("HOW TO DRIVE");
  gfx_rrect(10, 40, 300, 192, 10, rgb565(0x23262B), 225);
  gfx_image(IMG_PEDAL_BRAKE, 48, 86, 1, 0, 255);
  gfx_image(IMG_PEDAL_GAS, 272, 86, 1, 0, 255);
  gfx_text(FONT_M, 48, 132, "Left or 4", C_WHITE, C_BLACK, 1);
  gfx_text(FONT_S, 48, 146, "brake, reverse", rgb565(0xC8CDD4), C_BLACK, 1);
  gfx_text(FONT_M, 272, 132, "Right or OK", C_WHITE, C_BLACK, 1);
  gfx_text(FONT_S, 272, 146, "gas", rgb565(0xC8CDD4), C_BLACK, 1);
  gfx_text(FONT_M, 160, 70, "In the air:", rgb565(0xFFE14A), C_BLACK, 1);
  gfx_text(FONT_S, 160, 86, "gas tilts you back,", C_WHITE, C_BLACK, 1);
  gfx_text(FONT_S, 160, 100, "brake tilts you forward", C_WHITE, C_BLACK, 1);
  gfx_text(FONT_S, 160, 172, "Pick up fuel cans before the tank runs dry.", C_WHITE, C_BLACK, 1);
  gfx_text(FONT_S, 160, 186, "Bill's neck can't take a landing on his head!", C_WHITE, C_BLACK, 1);
  gfx_text(FONT_S, 160, 200, "Air time and flips earn coins. Back: pause.", C_WHITE, C_BLACK, 1);
  if ((t / 25) % 2 == 0) gfx_text(FONT_M, 160, 224, sv.seen_help ? "OK" : "OK: DRIVE!", rgb565(0xFFE14A), C_BLACK, 1);
}

/* a text in lines of at most width pixels, centred on cx, 12 pixels apart */
static void wrapped(int cx, int y, int width, const char *s) {
  char line[96];
  while (*s) {
    int n = 0, fit = 0;
    for (;;) {
      while (s[n] && s[n] != ' ') n++;
      if (n >= (int)sizeof line) break;
      memcpy(line, s, (size_t)n), line[n] = 0;
      if (gfx_text_width(FONT_S, line) > width && fit) break;
      fit = n;
      if (!s[n]) break;
      n++;
    }
    memcpy(line, s, (size_t)fit), line[fit] = 0;
    gfx_text(FONT_S, cx, y, line, rgb565(0xC8CDD4), C_BLACK, 1);
    s += fit;
    while (*s == ' ') s++;
    y += 12;
  }
}

static void level_bar(int x, int y, int w, int level, int max) {
  gfx_rrect(x, y, w, 7, 3, C_BLACK, 200);
  int f = (w - 2) * (level - 1) / (max - 1 > 0 ? max - 1 : 1);
  if (f > 0) gfx_rrect(x + 1, y + 1, f, 5, 2, rgb565(0x7FD63A), 255);
}

static void garage_draw(void) {
  backdrop("VEHICLE");
  const VehDef *v = &vehicles[shown];
  bool owned = sv.vehicles >> shown & 1;
  strip(NVEHICLES, IMG_THUMB_V0, vehicle_order, shown, sv.vehicles);
  char tx[48];
  snprintf(tx, sizeof tx, "%d/%d", vehicle_index(shown) + 1, NVEHICLES);
  gfx_text(FONT_S, 314, 68, tx, rgb565(0xB8C0C8), C_BLACK, 2);
  /* the vehicle, on a turntable of light */
  gfx_circle(160, 106, 58, rgb565(0x5A6F80), 60);
  gfx_image(v->icon, 160, 99, owned ? 1.0f : 0.92f, 0, owned ? 255 : 150);
  if (!owned) gfx_image(IMG_LOCK, 160, 99, 1, 0, 255);
  if (row == 0) arrows(113);
  gfx_text(FONT_L, 160, 156, v->name, row == 0 ? rgb565(0xFFE14A) : C_WHITE, C_BLACK, 1);
  if (!owned) {
    price_tag(160, 174, v->price, sv.coins >= v->price);
    wrapped(160, 188, 308, v->about);
    keys(flash ? "Not enough coins yet" : sv.coins >= v->price ? "OK: buy    Left Right: choose    Back: title"
                                                                : "Left Right: choose    Back: title", flash);
    return;
  }
  /* its four upgrades */
  for (int u = 0; u < 4; u++) {
    int x = 6 + u * 78, y = 161;
    bool on = row == 1 && u == up_sel;
    int lv = sv.level[shown][u], max = v->max[u];
    gfx_rrect(x, y, 74, 64, 7, on ? rgb565(0xF2B531) : rgb565(0x2A2D33), on ? 255 : 220);
    gfx_image(upgrade_icons[v->up[u]], (float)x + 20, (float)y + 19, 0.8f, 0, 255);
    snprintf(tx, sizeof tx, "%d/%d", lv, max);
    gfx_text(FONT_S, x + 70, y + 13, tx, on ? C_BLACK : C_WHITE, on ? rgb565(0xF2B531) : C_BLACK, 2);
    gfx_text(FONT_S, x + 37, y + 44, upgrade_names[v->up[u]], on ? C_BLACK : C_WHITE, on ? rgb565(0xF2B531) : C_BLACK, 1);
    level_bar(x + 6, y + 47, 62, lv, max);
    if (lv >= max) gfx_text(FONT_S, x + 37, y + 62, "MAX", rgb565(0x7FD63A), C_BLACK, 1);
    else {
      char n[20];
      format_number(n, upgrade_cost(shown, u, lv + 1));
      bool can = sv.coins >= upgrade_cost(shown, u, lv + 1);
      gfx_text(FONT_S, x + 37, y + 62, n, on ? (can ? C_BLACK : rgb565(0xA01010)) : can ? rgb565(0xFFE14A) : rgb565(0xFF7A6A),
               on ? rgb565(0xF2B531) : C_BLACK, 1);
    }
  }
  if (row == 0) keys("OK: stages    Down: upgrades    Left Right: choose", false);
  else keys(flash ? "Not enough coins!" : "OK: upgrade    Left Right: choose    Up: back", flash);
}

static void stages_draw(void) {
  backdrop("STAGE");
  const Stage *s = &stages[shown];
  bool owned = sv.stages >> shown & 1;
  strip(NSTAGES, IMG_THUMB_S0, NULL, shown, sv.stages);
  char tx[48];
  snprintf(tx, sizeof tx, "%d/%d", shown + 1, NSTAGES);
  gfx_text(FONT_S, 314, 68, tx, rgb565(0xB8C0C8), C_BLACK, 2);
  gfx_rrect(50, 58, 220, 112, 10, C_BLACK, 160);
  gfx_image_box(s->icon, 54, 62, 212, 104, 255);
  if (!owned) gfx_image_box(IMG_STAGE_LOCKED, 54, 62, 212, 104, 210);
  arrows(126);
  gfx_text(FONT_L, 160, 194, s->name, rgb565(0xFFE14A), C_BLACK, 1);
  if (owned) {
    snprintf(tx, sizeof tx, "Best with the %s: %dm", vehicles[sv.vehicle].name, sv.best[shown][sv.vehicle]);
    gfx_text(FONT_S, 160, 213, tx, rgb565(0xC8CDD4), C_BLACK, 1);
    keys("OK: drive    Left Right: choose    Back: vehicle", false);
  } else {
    price_tag(160, 214, s->price, sv.coins >= s->price);
    keys(flash ? "Not enough coins yet" : sv.coins >= s->price ? "OK: buy    Left Right: choose    Back: vehicle"
                                                                : "Left Right: choose    Back: vehicle", flash);
  }
}

void menu_draw(void) {
  switch (g_mode) {
    case MODE_TITLE: title_draw(); break;
    case MODE_HELP: help_draw(); break;
    case MODE_GARAGE: garage_draw(); break;
    case MODE_STAGES: stages_draw(); break;
  }
  if (ask == 1) ask_box(vehicles[shown].name, vehicles[shown].price);
  if (ask == 2) ask_box(stages[shown].name, stages[shown].price);
  if (bought) {
    for (int i = 0; i < 12; i++) {
      float a = (float)i * 0.52f + (float)bought * 0.1f, r = (float)(30 - bought) * 3;
      gfx_circle(160 + r * __builtin_cosf(a), 110 + r * __builtin_sinf(a), 3, rgb565(0xFFE14A), bought * 8);
    }
  }
}
