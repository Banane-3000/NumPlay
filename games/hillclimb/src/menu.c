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
      if (ask == 1 && pay(vehicles[shown].price)) sv.vehicles |= (uint16_t)(1 << shown), sv.vehicle = (uint8_t)shown;
      if (ask == 2 && pay(stages[shown].price)) sv.stages |= (uint16_t)(1 << shown), sv.stage = (uint8_t)shown;
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
        if (pressed & K_LEFT) shown = (shown + NVEHICLES - 1) % NVEHICLES, t = 0;
        if (pressed & K_RIGHT) shown = (shown + 1) % NVEHICLES, t = 0;
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
  gfx_text(FONT_M, 272, 132, "Right, OK or 6", C_WHITE, C_BLACK, 1);
  gfx_text(FONT_S, 272, 146, "gas", rgb565(0xC8CDD4), C_BLACK, 1);
  gfx_text(FONT_M, 160, 70, "In the air:", rgb565(0xFFE14A), C_BLACK, 1);
  gfx_text(FONT_S, 160, 86, "gas tilts you back,", C_WHITE, C_BLACK, 1);
  gfx_text(FONT_S, 160, 100, "brake tilts you forward", C_WHITE, C_BLACK, 1);
  gfx_text(FONT_S, 160, 172, "Pick up fuel cans before the tank runs dry.", C_WHITE, C_BLACK, 1);
  gfx_text(FONT_S, 160, 186, "Bill's neck can't take a landing on his head!", C_WHITE, C_BLACK, 1);
  gfx_text(FONT_S, 160, 200, "Air time and flips earn coins. Back: pause.", C_WHITE, C_BLACK, 1);
  if ((t / 25) % 2 == 0) gfx_text(FONT_M, 160, 224, sv.seen_help ? "OK" : "OK: DRIVE!", rgb565(0xFFE14A), C_BLACK, 1);
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
  /* the vehicle, on a turntable of light */
  gfx_circle(160, 118, 66, rgb565(0x5A6F80), 60);
  gfx_image(v->icon, 160, 92 + (row == 0 && !(t & 32) ? 0 : 0), owned ? 1.15f : 1.0f, 0, owned ? 255 : 150);
  if (!owned) gfx_image(IMG_LOCK, 160, 92, 1, 0, 255);
  if (row == 0) arrows(108);
  gfx_text(FONT_L, 160, 154, v->name, row == 0 ? rgb565(0xFFE14A) : C_WHITE, C_BLACK, 1);
  char tx[48];
  if (!owned) {
    price_tag(160, 176, v->price, sv.coins >= v->price);
    gfx_text(FONT_S, 160, 196, v->about, rgb565(0xC8CDD4), C_BLACK, 1);
    gfx_text(FONT_S, 160, 230, sv.coins >= v->price ? "OK: buy" : "Not enough coins yet", flash ? rgb565(0xFF5A4A) : C_WHITE,
             C_BLACK, 1);
    return;
  }
  /* its four upgrades */
  for (int u = 0; u < 4; u++) {
    int x = 6 + u * 78, y = 164;
    bool on = row == 1 && u == up_sel;
    int lv = sv.level[shown][u], max = v->max[u];
    gfx_rrect(x, y, 74, 70, 7, on ? rgb565(0xF2B531) : rgb565(0x2A2D33), on ? 255 : 220);
    gfx_image(upgrade_icons[v->up[u]], (float)x + 20, (float)y + 21, 0.85f, 0, 255);
    snprintf(tx, sizeof tx, "%d/%d", lv, max);
    gfx_text(FONT_S, x + 70, y + 14, tx, on ? C_BLACK : C_WHITE, on ? rgb565(0xF2B531) : C_BLACK, 2);
    gfx_text(FONT_S, x + 37, y + 49, upgrade_names[v->up[u]], on ? C_BLACK : C_WHITE, on ? rgb565(0xF2B531) : C_BLACK, 1);
    level_bar(x + 6, y + 52, 62, lv, max);
    if (lv >= max) gfx_text(FONT_S, x + 37, y + 68, "MAX", rgb565(0x7FD63A), C_BLACK, 1);
    else {
      char n[20];
      format_number(n, upgrade_cost(shown, u, lv + 1));
      gfx_text(FONT_S, x + 37, y + 68, n, sv.coins >= upgrade_cost(shown, u, lv + 1) ? rgb565(0xFFE14A) : rgb565(0xFF7A6A),
               C_BLACK, 1);
    }
  }
  if (row == 0) gfx_text(FONT_S, 160, 236 - 74, "", C_WHITE, C_BLACK, 1);
  const char *hint = row == 0 ? "OK: choose a stage    Down: upgrades" : flash ? "Not enough coins!" : "OK: upgrade    Up: back";
  gfx_text(FONT_S, 312, 44, hint, flash ? rgb565(0xFF5A4A) : rgb565(0xC8CDD4), C_BLACK, 2);
}

static void stages_draw(void) {
  backdrop("STAGE");
  const Stage *s = &stages[shown];
  bool owned = sv.stages >> shown & 1;
  gfx_rrect(42, 46, 236, 122, 10, C_BLACK, 160);
  gfx_image_box(s->icon, 46, 50, 228, 114, 255);
  if (!owned) gfx_image_box(IMG_STAGE_LOCKED, 46, 50, 228, 114, 210);
  arrows(118);
  gfx_text(FONT_L, 160, 192, s->name, rgb565(0xFFE14A), C_BLACK, 1);
  char tx[48];
  if (owned) {
    snprintf(tx, sizeof tx, "Best with the %s: %dm", vehicles[sv.vehicle].name, sv.best[shown][sv.vehicle]);
    gfx_text(FONT_S, 160, 210, tx, rgb565(0xC8CDD4), C_BLACK, 1);
    if ((t / 25) % 2 == 0) gfx_text(FONT_M, 160, 232, "OK: START", C_WHITE, C_BLACK, 1);
  } else {
    price_tag(160, 212, s->price, sv.coins >= s->price);
    gfx_text(FONT_S, 160, 232, sv.coins >= s->price ? "OK: buy" : "Not enough coins yet", flash ? rgb565(0xFF5A4A) : C_WHITE,
             C_BLACK, 1);
  }
  /* where this stage is in the list */
  for (int i = 0; i < NSTAGES; i++)
    gfx_circle(160 - (NSTAGES - 1) * 5 + i * 10.0f, 40, i == shown ? 3 : 2, i == shown ? C_WHITE : rgb565(0x7A828C), 255);
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
