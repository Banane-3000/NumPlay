/* Plants vs. Zombies: the screens, from the title to the lawn and back (screens.c). */
#ifndef PVZ_SCREENS_H
#define PVZ_SCREENS_H
#include <stdbool.h>
#include <stdint.h>

void game_init(void);
void game_frame(uint32_t keys);   /* a hundredth of a second */
void game_draw(void);
extern bool g_running;
extern int g_state;
enum {
  ST_TITLE, ST_MENU, ST_LEVELS, ST_DAVE, ST_PAN, ST_CHOOSE, ST_READY, ST_PLAY, ST_PAUSE, ST_WON, ST_AWARD, ST_LOST,
  ST_ALMANAC, ST_SHOP, ST_HELP
};
#endif
