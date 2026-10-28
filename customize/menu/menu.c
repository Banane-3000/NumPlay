/* Menu de Multijeux. Compilé une fois par GitHub Actions (menu.yml) en menu.o.
 * Les noms des jeux viennent de la table mj_table, générée par nwa.html :
 * octet 0 = nombre de jeux, puis 8 noms de 24 octets. Les jeux sont game_1_main... game_8_main.
 * Les versions faibles ci-dessous sont remplacées par celles des jeux au moment de la liaison. */
#include "eadk.h"
#include <stddef.h>
#include <stdint.h>

const uint32_t eadk_api_level __attribute__((section(".rodata.eadk_api_level"))) = 0;

typedef int (*entry_t)(int, char **);
#define STUB(n) __attribute__((weak)) int game_##n##_main(int argc, char **argv) { (void)argc; (void)argv; return 0; }
STUB(1) STUB(2) STUB(3) STUB(4) STUB(5) STUB(6) STUB(7) STUB(8)

extern const uint8_t mj_table[];
#define NAMELEN 24

static const entry_t entries[8] = { game_1_main, game_2_main, game_3_main, game_4_main,
                                    game_5_main, game_6_main, game_7_main, game_8_main };

static int count(void) { int n = mj_table[0]; return n > 8 ? 8 : n; }
static const char *name(int i) { return (const char *)(mj_table + 4 + i * NAMELEN); }

static void wait_release(void) { while (eadk_keyboard_scan() != 0) { eadk_timing_msleep(20); } }

static void draw_menu(int sel) {
  eadk_display_push_rect_uniform((eadk_rect_t){0, 0, 320, 240}, eadk_color_white);
  eadk_display_draw_string("Choisis un jeu", (eadk_point_t){10, 8}, true, eadk_color_black, eadk_color_white);
  for (int i = 0; i < count(); i++) {
    uint16_t y = (uint16_t)(36 + i * 22);
    eadk_color_t bg = (i == sel) ? 0x001F : eadk_color_white;
    eadk_color_t fg = (i == sel) ? eadk_color_white : eadk_color_black;
    eadk_display_push_rect_uniform((eadk_rect_t){0, y, 320, 20}, bg);
    eadk_display_draw_string(name(i), (eadk_point_t){12, (uint16_t)(y + 2)}, false, fg, bg);
  }
}

int main(int argc, char *argv[]) {
  (void)argc; (void)argv;
  int sel = 0;
  draw_menu(sel);
  while (1) {
    eadk_keyboard_state_t k = eadk_keyboard_scan();
    if (eadk_keyboard_key_down(k, eadk_key_down) && sel < count() - 1) { sel++; draw_menu(sel); wait_release(); }
    else if (eadk_keyboard_key_down(k, eadk_key_up) && sel > 0) { sel--; draw_menu(sel); wait_release(); }
    else if (eadk_keyboard_key_down(k, eadk_key_ok) || eadk_keyboard_key_down(k, eadk_key_exe)) {
      wait_release();
      entries[sel](0, NULL);
      wait_release();
      draw_menu(sel);
    }
    eadk_timing_msleep(20);
  }
  return 0;
}
