/* Subway Surfers: little pictures for the screens over the game, the game's own, centred at (x, y), about s
 * pixels tall (24 or 15). */
#ifndef SS_ICONS_H
#define SS_ICONS_H
enum {
  ICON_COIN, ICON_KEY, ICON_BOARD, ICON_JETPACK, ICON_SNEAKERS, ICON_MAGNET, ICON_DOUBLE, ICON_MYSTERY,
  ICON_HEADSTART, ICON_BOOSTER, ICON_CHECK, ICON_LOCK, ICON_STAR, ICON_TROPHY, ICON_GIFT, ICON_TIMER, ICON_COUNT
};
void icon(int which, float x, float y, float s);
#endif
