/* Text and boxes over the picture (ui.c), and the screens that use them (menus.c). */
#ifndef S1_UI_H
#define S1_UI_H
#include <stdbool.h>
#include <stdint.h>

#define UI_WHITE 0xFFFF
#define UI_YELLOW 0xFFE0
#define UI_BLUE 0x0010
#define UI_DARK 0x0841

extern bool ui_active;
void ui_clear(void);
void ui_box(int x, int y, int w, int h, uint16_t c);
void ui_text(int x, int y, uint16_t c, int scale, const char *s);
void ui_text_centred(int y, uint16_t c, int scale, const char *s);
int ui_width(const char *s, int scale);
void ui_draw_band(uint16_t *band, int y0);

/* menus.c */
void keys_screen(void);              /* how to play, the first time */
void continue_menu_draw(int sel);     /* on the title: Continue (0) or New game (1) */
#endif
