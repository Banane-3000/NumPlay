/* Menu de Multijeux (jusqu'a 100 jeux, avec defilement). Compile une fois par GitHub Actions (menu.yml) en menu.o.
 * mj_table, generee par nwa.html : octet 0 = nombre de jeux, puis 100 noms de 24 octets (debut a l'octet 4).
 * Les jeux sont game_1_main ... game_100_main. Les versions faibles ci-dessous sont remplacees a la liaison. */
#include "eadk.h"
#include <stddef.h>
#include <stdint.h>

const uint32_t eadk_api_level __attribute__((section(".rodata.eadk_api_level"))) = 0;

#define MAXG 100
#define NAMELEN 24
#define ROWS 9

typedef int (*entry_t)(int, char **);
#define STUB(n) __attribute__((weak)) int game_##n##_main(int argc, char **argv) { (void)argc; (void)argv; return 0; }
STUB(1) STUB(2) STUB(3) STUB(4) STUB(5) STUB(6) STUB(7) STUB(8) STUB(9) STUB(10)
STUB(11) STUB(12) STUB(13) STUB(14) STUB(15) STUB(16) STUB(17) STUB(18) STUB(19) STUB(20)
STUB(21) STUB(22) STUB(23) STUB(24) STUB(25) STUB(26) STUB(27) STUB(28) STUB(29) STUB(30)
STUB(31) STUB(32) STUB(33) STUB(34) STUB(35) STUB(36) STUB(37) STUB(38) STUB(39) STUB(40)
STUB(41) STUB(42) STUB(43) STUB(44) STUB(45) STUB(46) STUB(47) STUB(48) STUB(49) STUB(50)
STUB(51) STUB(52) STUB(53) STUB(54) STUB(55) STUB(56) STUB(57) STUB(58) STUB(59) STUB(60)
STUB(61) STUB(62) STUB(63) STUB(64) STUB(65) STUB(66) STUB(67) STUB(68) STUB(69) STUB(70)
STUB(71) STUB(72) STUB(73) STUB(74) STUB(75) STUB(76) STUB(77) STUB(78) STUB(79) STUB(80)
STUB(81) STUB(82) STUB(83) STUB(84) STUB(85) STUB(86) STUB(87) STUB(88) STUB(89) STUB(90)
STUB(91) STUB(92) STUB(93) STUB(94) STUB(95) STUB(96) STUB(97) STUB(98) STUB(99) STUB(100)

extern const uint8_t mj_table[];

static const entry_t entries[MAXG] = {
  game_1_main, game_2_main, game_3_main, game_4_main, game_5_main, game_6_main, game_7_main, game_8_main,
  game_9_main, game_10_main, game_11_main, game_12_main, game_13_main, game_14_main, game_15_main, game_16_main,
  game_17_main, game_18_main, game_19_main, game_20_main, game_21_main, game_22_main, game_23_main, game_24_main,
  game_25_main, game_26_main, game_27_main, game_28_main, game_29_main, game_30_main, game_31_main, game_32_main,
  game_33_main, game_34_main, game_35_main, game_36_main, game_37_main, game_38_main, game_39_main, game_40_main,
  game_41_main, game_42_main, game_43_main, game_44_main, game_45_main, game_46_main, game_47_main, game_48_main,
  game_49_main, game_50_main, game_51_main, game_52_main, game_53_main, game_54_main, game_55_main, game_56_main,
  game_57_main, game_58_main, game_59_main, game_60_main, game_61_main, game_62_main, game_63_main, game_64_main,
  game_65_main, game_66_main, game_67_main, game_68_main, game_69_main, game_70_main, game_71_main, game_72_main,
  game_73_main, game_74_main, game_75_main, game_76_main, game_77_main, game_78_main, game_79_main, game_80_main,
  game_81_main, game_82_main, game_83_main, game_84_main, game_85_main, game_86_main, game_87_main, game_88_main,
  game_89_main, game_90_main, game_91_main, game_92_main, game_93_main, game_94_main, game_95_main, game_96_main,
  game_97_main, game_98_main, game_99_main, game_100_main
};

static int count(void) { int n = mj_table[0]; return n > MAXG ? MAXG : n; }
static const char *name(int i) { return (const char *)(mj_table + 4 + i * NAMELEN); }

static void wait_release(void) { while (eadk_keyboard_scan() != 0) { eadk_timing_msleep(20); } }

static void num(char *s, int v) { /* v -> texte */
  char t[8]; int n = 0; if (v == 0) t[n++] = '0';
  while (v > 0 && n < 7) { t[n++] = (char)('0' + v % 10); v /= 10; }
  while (n > 0) *s++ = t[--n];
  *s = 0;
}

static void draw_menu(int sel, int top) {
  char head[24]; char a[8], b[8]; int p = 0;
  eadk_display_push_rect_uniform((eadk_rect_t){0, 0, 320, 240}, eadk_color_white);
  num(a, sel + 1); num(b, count());
  const char *t = "Choisis un jeu  ";
  while (t[p]) { head[p] = t[p]; p++; }
  for (int i = 0; a[i]; i++) head[p++] = a[i];
  head[p++] = '/';
  for (int i = 0; b[i]; i++) head[p++] = b[i];
  head[p] = 0;
  eadk_display_draw_string(head, (eadk_point_t){10, 8}, true, eadk_color_black, eadk_color_white);
  for (int r = 0; r < ROWS; r++) {
    int i = top + r;
    if (i >= count()) break;
    uint16_t y = (uint16_t)(36 + r * 22);
    eadk_color_t bg = (i == sel) ? 0x001F : eadk_color_white;
    eadk_color_t fg = (i == sel) ? eadk_color_white : eadk_color_black;
    eadk_display_push_rect_uniform((eadk_rect_t){0, y, 320, 20}, bg);
    eadk_display_draw_string(name(i), (eadk_point_t){12, (uint16_t)(y + 2)}, false, fg, bg);
  }
}

int main(int argc, char *argv[]) {
  (void)argc; (void)argv;
  int sel = 0, top = 0;
  draw_menu(sel, top);
  while (1) {
    eadk_keyboard_state_t k = eadk_keyboard_scan();
    if (eadk_keyboard_key_down(k, eadk_key_down) && sel < count() - 1) {
      sel++; if (sel >= top + ROWS) top = sel - ROWS + 1;
      draw_menu(sel, top); wait_release();
    } else if (eadk_keyboard_key_down(k, eadk_key_up) && sel > 0) {
      sel--; if (sel < top) top = sel;
      draw_menu(sel, top); wait_release();
    } else if (eadk_keyboard_key_down(k, eadk_key_ok) || eadk_keyboard_key_down(k, eadk_key_exe)) {
      wait_release();
      entries[sel](0, NULL);
      wait_release();
      draw_menu(sel, top);
    }
    eadk_timing_msleep(20);
  }
  return 0;
}
