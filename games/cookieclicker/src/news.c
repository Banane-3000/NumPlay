/* The news ticker (the game's getNewTicker): what the bakery's fame has reached, news of the
 * buildings, the grandmas' mutterings and the elders' invasion. */
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "game.h"

static const char *const lore[][2] = {
  {"You feel like making cookies. But nobody wants to eat your cookies.", NULL},
  {"Your first batch goes to the trash. The neighborhood raccoon barely touches it.", NULL},
  {"Your family agrees to try some of your cookies.", NULL},
  {"Your cookies are popular in the neighborhood.", "People are starting to talk about your cookies."},
  {"Your cookies are talked about for miles around.", "Your cookies are renowned in the whole town!"},
  {"Your cookies bring all the boys to the yard.", "Your cookies now have their own website!"},
  {"Your cookies are worth a lot of money.", "Your cookies sell very well in distant countries."},
  {"People come from very far away to get a taste of your cookies.",
   "Kings and queens from all over the world are enjoying your cookies."},
  {"There are now museums dedicated to your cookies.", "A national day has been created in honor of your cookies."},
  {"Your cookies have been named a part of the world wonders.",
   "History books now include a whole chapter about your cookies."},
  {"Your cookies have been placed under government surveillance.", "The whole planet is enjoying your cookies!"},
  {"Strange creatures from neighboring planets wish to try your cookies.",
   "Elder gods from the whole cosmos have awoken to taste your cookies."},
  {"Beings from other dimensions lapse into existence just to get a taste of your cookies.",
   "Your cookies have achieved sentience."},
  {"The universe has now turned into cookie dough, to the molecular level.",
   "Your cookies are rewriting the fundamental laws of the universe."},
  {"A local news station runs a 10-minute segment about your cookies. Success! (you win a cookie)",
   "it's time to stop playing"},
};

uint32_t ticker_n;
void news_next(char *out, int size) {
  ticker_n++;
  int lore_progress = S.cookies_earned >= 10 ? (int)(log10(S.cookies_earned / 10) + 1) : 0;
  if (lore_progress < 0) lore_progress = 0;
  if (lore_progress > 14) lore_progress = 14;
  const char *const *l = lore[lore_progress];
  snprintf(out, size, "%s", l[1] && rnd() < 0.5 ? l[1] : l[0]);
}
