/* Subway Surfers: little pictures for the screens over the game, drawn from discs and polygons (draw.h),
 * centred at (x, y), s pixels tall. */
#ifndef SS_ICONS_H
#define SS_ICONS_H
enum {
  ICON_COIN, ICON_KEY, ICON_BOARD, ICON_JETPACK, ICON_SNEAKERS, ICON_MAGNET, ICON_DOUBLE, ICON_MYSTERY,
  ICON_HEADSTART, ICON_BOOSTER, ICON_CHECK, ICON_LOCK, ICON_STAR, ICON_COUNT
};
void icon(int which, float x, float y, float s);
#endif
