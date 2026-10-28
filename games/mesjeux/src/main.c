/* Mes jeux: the menu of your own .nwa games, inside NumPlay.
 *
 * A game like any other for NumPlay: it has its tile in the launcher. The customizer website (customize/app.js)
 * adds your .nwa objects to the link, their main renamed game_1_main ... game_100_main, and a table of their
 * names, mj_table: byte 0 is the count, then 24 bytes for each name from byte 4. This lists them (up to 100,
 * scrolling) and runs the one you pick. Back or Home leaves.
 * On its own (MesJeux.nwa) it has no games to show. */
#include <eadk.h>
#include <stddef.h>
#include <stdint.h>

#define MAXG 100
#define NAMELEN 24
#define ROWS 9
#define HOME_KEYS ((1ull << eadk_key_home) | (1ull << eadk_key_on_off))

typedef int (*entry_t)(int, char **);

#if NP_MODULE
/* (defined by your .nwa objects, and by the customizer's stand-ins for the numbers left over) */
#define G(n) extern __attribute__((visibility("default"))) int game_##n##_main(int, char **);
extern __attribute__((visibility("default"))) const uint8_t mj_table[];
#else
const char eadk_app_name[] __attribute__((section(".rodata.eadk_app_name"))) = "Mes jeux";
const uint32_t eadk_api_level __attribute__((section(".rodata.eadk_api_level"))) = 0;
#define G(n) static int game_##n##_main(int argc, char **argv) { (void)argc; (void)argv; return 0; }
static const uint8_t mj_table[4 + MAXG * NAMELEN];
#endif

G(1) G(2) G(3) G(4) G(5) G(6) G(7) G(8) G(9) G(10)
G(11) G(12) G(13) G(14) G(15) G(16) G(17) G(18) G(19) G(20)
G(21) G(22) G(23) G(24) G(25) G(26) G(27) G(28) G(29) G(30)
G(31) G(32) G(33) G(34) G(35) G(36) G(37) G(38) G(39) G(40)
G(41) G(42) G(43) G(44) G(45) G(46) G(47) G(48) G(49) G(50)
G(51) G(52) G(53) G(54) G(55) G(56) G(57) G(58) G(59) G(60)
G(61) G(62) G(63) G(64) G(65) G(66) G(67) G(68) G(69) G(70)
G(71) G(72) G(73) G(74) G(75) G(76) G(77) G(78) G(79) G(80)
G(81) G(82) G(83) G(84) G(85) G(86) G(87) G(88) G(89) G(90)
G(91) G(92) G(93) G(94) G(95) G(96) G(97) G(98) G(99) G(100)

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

static void wait_release(void) {
  while (eadk_keyboard_scan() != 0) eadk_timing_msleep(20);
}

static void num(char *s, int v) {
  char t[8];
  int n = 0;
  if (v == 0) t[n++] = '0';
  while (v > 0 && n < 7) { t[n++] = (char)('0' + v % 10); v /= 10; }
  while (n > 0) *s++ = t[--n];
  *s = 0;
}

static void draw_menu(int sel, int top) {
  char head[32], a[8], b[8];
  int p = 0;
  eadk_display_push_rect_uniform((eadk_rect_t){0, 0, 320, 240}, eadk_color_white);
  if (count() == 0) {
    eadk_display_draw_string("Mes jeux", (eadk_point_t){10, 8}, true, eadk_color_black, eadk_color_white);
    eadk_display_draw_string("Aucun jeu: ajoute des .nwa", (eadk_point_t){10, 40}, false, eadk_color_black, eadk_color_white);
    eadk_display_draw_string("dans NumPlay Studio.", (eadk_point_t){10, 60}, false, eadk_color_black, eadk_color_white);
    return;
  }
  num(a, sel + 1);
  num(b, count());
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

int main(void) {
  int sel = 0, top = 0;
  wait_release();
  draw_menu(sel, top);
  for (;;) {
    eadk_keyboard_state_t k = eadk_keyboard_scan();
    if ((k & HOME_KEYS) || eadk_keyboard_key_down(k, eadk_key_back)) break;
    if (count() > 0) {
      if (eadk_keyboard_key_down(k, eadk_key_down) && sel < count() - 1) {
        sel++;
        if (sel >= top + ROWS) top = sel - ROWS + 1;
        draw_menu(sel, top);
        wait_release();
      } else if (eadk_keyboard_key_down(k, eadk_key_up) && sel > 0) {
        sel--;
        if (sel < top) top = sel;
        draw_menu(sel, top);
        wait_release();
      } else if (eadk_keyboard_key_down(k, eadk_key_ok) || eadk_keyboard_key_down(k, eadk_key_exe)) {
        wait_release();
        entries[sel](0, NULL);
        if (eadk_keyboard_scan() & HOME_KEYS) break; /* the game ended on Home: NumPlay goes home too */
        wait_release();
        draw_menu(sel, top);
      }
    }
    eadk_timing_msleep(20);
  }
  return 0;
}
