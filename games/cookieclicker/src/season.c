/* Seasons (the Season switcher's biscuits: Christmas, Halloween, Valentine's day, Business day,
 * Easter), their drops: eggs, Santa's gifts. The game's own calendar seasons need a date the
 * calculator doesn't have: here a season is one the switcher triggers, for 24 hours of play. */
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "game.h"

#define SEASON_TRIGGER_BASE_PRICE 1000000000.0
static const struct { uint16_t trigger; const char *name, *start, *over; } seasons[6] = {
  {0, "", "", ""},
  {U_FESTIVE_BISCUIT, "Christmas", "Christmas season has started!", "Christmas season is over."},
  {U_GHOSTLY_BISCUIT, "Halloween", "Halloween has started!", "Halloween is over."},
  {U_LOVESICK_BISCUIT, "Valentine's day", "Valentine's day has started!", "Valentine's day is over."},
  {U_BUNNY_BISCUIT, "Easter", "Easter season has started!", "Easter season is over."},
  {U_FOOLS_BISCUIT, "Business day", "Business day has started!", "Business day is over."},
};
const char *season_name(int s) { return seasons[s].name; }
static int season_of_trigger(int u) {
  for (int s = 1; s < 6; s++)
    if (seasons[s].trigger == u) return s;
  return 0;
}
static int32_t season_duration(void) { return FPS * 60 * 60 * 24; }

double season_price(int u) {
  (void)u;
  static const float m[4] = {1, 2, 1.5f, 1.25f};
  return SEASON_TRIGGER_BASE_PRICE + G.unbuffed_cps * 60 * pow(1.5, S.season_uses) * m[hasgod(GOD_SEASONS)];
}

void season_trigger(int u) {
  int s = season_of_trigger(u);
  S.season_uses++;
  for (int k = 1; k < 6; k++)
    if (seasons[k].trigger != u) lock(seasons[k].trigger), unlock(seasons[k].trigger);
  if (S.season && S.season != s) notify(seasons[S.season].over, "", upgrade_rec(seasons[S.season].trigger)->icon, 4);
  S.season = (uint8_t)s;
  S.season_t = season_duration();
  G.store_dirty = true, G.upgrades_dirty = true;
  notify(seasons[s].start, "", upgrade_rec(u)->icon, 4);
}
/* a season's biscuit, bought again: the season stops */
bool season_cancel(int u) {
  int s = season_of_trigger(u);
  if (!s || !bit_get(S.upgrades_bought, u) || S.season != s) return false;
  lock(u);
  notify(seasons[s].over, "", upgrade_rec(u)->icon, 0);
  if (has(U_SEASON_SWITCHER)) unlock(u), bit_set(S.upgrades_bought, u, false);
  G.upgrades_dirty = true, G.recalculate = true;
  S.season = 0, S.season_t = -1;
  return true;
}

void season_logic(void) {
  if (S.season_t > 0) S.season_t--;
  if (S.season_t <= 0 && S.season && !has(U_ETERNAL_SEASONS)) {
    int u = seasons[S.season].trigger;
    notify(seasons[S.season].over, "", upgrade_rec(u)->icon, 0);
    if (has(U_SEASON_SWITCHER)) unlock(u), bit_set(S.upgrades_bought, u, false);
    S.season = 0, S.season_t = -1;
    G.upgrades_dirty = true;
  }
}

/* ------------------------------------------------------------------ Easter */
int how_many_eggs(void) {
  int n = 0;
  for (int i = 0; i < N_EASTER_EGGS; i++) n += has(LIST(EASTER_EGGS)[i]);
  return n;
}
void drop_egg(double fail) {
  if (S.season != SEASON_EASTER) return;
  if (has_achiev(A_HIDE_AND_SEEK_CHAMPION)) fail *= 0.8;
  if (has(U_OMELETTE)) fail *= 0.9;
  if (has(U_STARSPAWN)) fail *= 0.9;
  static const float sg[4] = {1, 0.9f, 0.95f, 0.97f};
  fail *= sg[hasgod(GOD_SEASONS)];
  fail = pow(fail, drop_rate_mult());
  if (rnd() <= fail) return;
  /* the first 12 eggs are the common ones, the last 8 the rare ones */
  int drop = rnd() < 0.1 ? LIST(EASTER_EGGS)[12 + rnd_int(8)] : LIST(EASTER_EGGS)[rnd_int(12)];
  if (has(drop) || has_unlocked(drop)) drop = rnd() < 0.1 ? LIST(EASTER_EGGS)[12 + rnd_int(8)] : LIST(EASTER_EGGS)[rnd_int(12)];
  if (has(drop) || has_unlocked(drop)) return;
  unlock(drop);
  char d[64];
  snprintf(d, sizeof d, "\x01%s\x02", res_text(upgrade_rec(drop)->name));
  notify("You found an egg!", d, upgrade_rec(drop)->icon, 0);
}

/* ------------------------------------------------------------------ Christmas: Santa's gifts */
void santa_drop(void) {
  int drop = LIST(SANTA_DROPS)[rnd_int(N_SANTA_DROPS)];
  unlock(drop);
  char d[96];
  snprintf(d, sizeof d, "a festive test tube\nand \x01%s\x02.", res_text(upgrade_rec(drop)->name));
  notify("In the festive hat, you find...", d, upgrade_rec(drop)->icon, 0);
}
