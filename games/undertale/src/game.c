/* The game's state, its modes and its frame (game.h). */
#include <stdio.h>
#include <string.h>
#include "battle.h"
#include "event.h"
#include "game.h"
#include "gfx.h"
#include "items.h"
#include "menu.h"
#include "res.h"
#include "save.h"
#include "text.h"
#include "world.h"

Save sv;
int g_mode;
uint32_t g_frame;
bool g_running = true;
static uint32_t prev_keys;

static uint32_t seed = 0x2545F491u;
uint32_t rnd(void) {
  seed ^= seed << 13, seed ^= seed >> 17, seed ^= seed << 5;
  return seed;
}
int rnd_n(int n) { return n > 0 ? (int)(rnd() % (uint32_t)n) : 0; }

/* ---------------------------------------------------------------- stats (LOVE) */
static const uint32_t exp_table[] = {0, 0, 10, 30, 70, 120, 200, 300, 500, 800, 1200, 1700, 2500, 3500, 5000, 7000,
                                     10000, 15000, 25000, 50000, 99999};
int lv_exp_for(int lv) { return lv >= 1 && lv <= 20 ? (int)exp_table[lv] : 99999; }
int lv_maxhp(int lv) { return lv >= 20 ? 99 : 16 + 4 * lv; }
int lv_at(int lv) { return 8 + 2 * lv; }
int lv_df(int lv) { return 10 + (lv - 1) / 4; }

bool gain_exp(int exp) {
  sv.exp = (uint32_t)min_i((int)sv.exp + exp, 99999);
  bool up = false;
  while (sv.lv < 20 && (int)sv.exp >= lv_exp_for(sv.lv + 1)) {
    sv.lv++;
    int old = sv.maxhp;
    sv.maxhp = (int16_t)lv_maxhp(sv.lv);
    sv.hp = (int16_t)(sv.hp + sv.maxhp - old);
    sv.at = (uint8_t)lv_at(sv.lv), sv.df = (uint8_t)lv_df(sv.lv);
    up = true;
  }
  return up;
}
int weapon_at(void) { return item_info(sv.weapon)->at; }
int armor_df(void) { return item_info(sv.armor)->df; }

bool item_add(int item) {
  for (int i = 0; i < NUM_ITEMS; i++)
    if (!sv.items[i]) {
      sv.items[i] = (uint8_t)item;
      return true;
    }
  return false;
}
void item_remove_at(int slot) {
  for (int i = slot; i < NUM_ITEMS - 1; i++) sv.items[i] = sv.items[i + 1];
  sv.items[NUM_ITEMS - 1] = 0;
}
int item_count(void) {
  int n = 0;
  while (n < NUM_ITEMS && sv.items[n]) n++;
  return n;
}

/* ---------------------------------------------------------------- a new game */
void game_new(const char *name) {
  char n[sizeof sv.name];
  snprintf(n, sizeof n, "%s", name);   /* (it can be sv.name) */
  memset(&sv, 0, sizeof sv);
  memcpy(sv.name, n, sizeof n);
  memcpy(g_name, sv.name, sizeof g_name);
  sv.lv = 1, sv.maxhp = sv.hp = 20, sv.at = (uint8_t)lv_at(1), sv.df = (uint8_t)lv_df(1);
  sv.weapon = ITEM_STICK, sv.armor = ITEM_BANDAGE;
  g_mode = MODE_WORLD;
  world_enter(ROOM_AREA1, 120, 6010, DIR_DOWN);
}

void game_init(void) {
  save_restore_copy();
  /* back where Home left it, or the intro */
  if (!save_resume_load()) menu_title_open();
}

void game_open_menu(void) { menu_open(); }

void game_quit_save(void) { save_resume_write(); }

/* ---------------------------------------------------------------- the frame */
void game_frame(uint32_t keys) {
  uint32_t pressed = keys & ~prev_keys;
  prev_keys = keys;
  g_frame++;
  if (keys & K_HOME) {
    if (g_mode == MODE_WORLD || g_mode == MODE_MENU || g_mode == MODE_BATTLE) game_quit_save();
    g_running = false;
    return;
  }
  if (g_mode == MODE_WORLD || g_mode == MODE_BATTLE || g_mode == MODE_MENU) sv.time++;
  switch (g_mode) {
    case MODE_WORLD:
      world_update(keys, pressed);
      event_update();
      text_update(pressed);
      break;
    case MODE_BATTLE:
      battle_update(keys, pressed);
      event_update();
      break;
    default:
      menu_update(keys, pressed);
      if (g_mode == MODE_WORLD) event_update();
      break;
  }
}

void game_draw(void) {
  gfx_begin();
  switch (g_mode) {
    case MODE_WORLD:
    case MODE_MENU:
    case MODE_SAVE:
      world_draw();
      if (g_fade) gfx_rect(0, 0, SCREEN_W, SCREEN_H, C_BLACK, g_fade);
      text_draw();
      if (g_mode != MODE_WORLD) menu_draw();
      break;
    case MODE_BATTLE:
      battle_draw();
      break;
    default:
      menu_draw();
      break;
  }
  gfx_end();
}
