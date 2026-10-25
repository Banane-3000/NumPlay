/* Angry Birds: the screens and what the keys do on each (ui.c). */
#ifndef AB_UI_H
#define AB_UI_H
#include <stdbool.h>
#include <stdint.h>

enum {
  S_TITLE, S_EPISODES, S_LEVELS, S_STORY, S_PLAY, S_PAUSE, S_RESULT, S_HELP, S_QUIT, S_SAVE_ERROR, S_GUIDE, S_NOMEM,
  S_COUNT
};

/* arena: the physics' memory; strip: the work buffer (world.h), the screen's strip and the save's scratch memory */
void ui_init(void *arena, int arena_size, uint16_t *strip);
/* the keys held now (K_*), the time it is (ms); false: quit */
bool ui_frame(uint32_t keys, uint32_t now);
void ui_draw(void);
bool ui_dirty(void);
int ui_screen(void);
bool ui_save(void);
#endif
