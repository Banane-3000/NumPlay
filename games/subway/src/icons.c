#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC optimize("Os")
#endif
/* Little pictures (icons.h): the game's own (data.c), at the size nearest. */
#include "data.h"
#include "draw.h"
#include "icons.h"

void icon(int which, float x, float y, float s) {
  static const Image *const pics[2][ICON_COUNT] = {
    {&im_coin, &im_key, &im_board, &im_jetpack, &im_sneakers, &im_magnet, &im_double, &im_mystery, &im_headstart, &im_booster,
     &im_check, &im_lock, &im_star, &im_trophy, &im_gift, &im_timer},
    {&im_coin_s, &im_key_s, &im_board_s, &im_jetpack_s, &im_sneakers_s, &im_magnet_s, &im_double_s, &im_mystery_s, &im_headstart_s,
     &im_booster_s, &im_check_s, &im_lock_s, &im_star_s, &im_trophy_s, &im_gift_s, &im_timer_s},
  };
  const Image *im = pics[s < 19][which];
  d_image((int)(x - (float)im->w / 2 + 0.5f), (int)(y - (float)im->h / 2 + 0.5f), im);
}
