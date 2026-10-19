/* Sugar lumps (the game's computeLumpTimes, doLumps, harvestLumps, computeLumpType): once a billion
 * cookies are baked in all, a lump grows; it matures after 20 hours, ripens after 23 and falls after 24,
 * and may be bifurcated, golden, meaty or caramelized. Lumps level up buildings (a building's level
 * costs that many lumps and gives +1% of its production; level 1 of the farm, bank, temple and wizard
 * tower opens their minigames).
 *
 * The calculator can't tell how long it was off, so lumps grow while the game is open, 24 times as
 * fast as the game's clock (an hour of play for a day). */
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "game.h"

#define LUMP_SPEED 24.0
#define HOUR (1000.0 * 60 * 60)
double lump_mature_age = HOUR * 20, lump_ripe_age = HOUR * 23, lump_overripe_age = HOUR * 24;

static double lump_now(void) { return S.play_time * LUMP_SPEED; }
double lump_age(void) { return lump_now() - S.lump_t; }

bool can_lumps(void) {
  return S.lumps_total > -1 || (S.ascension_mode != 1 && S.cookies_earned + S.cookies_reset >= 1000000000);
}

void compute_lump_times(void) {
  lump_mature_age = HOUR * 20, lump_ripe_age = HOUR * 23;
  if (has(U_STEVIA_CAELESTIS)) lump_ripe_age -= HOUR;
  if (has(U_DIABETICA_DAEMONICUS)) lump_mature_age -= HOUR;
  if (has(U_ICHOR_SYRUP)) lump_mature_age -= 1000 * 60 * 7;
  if (has(U_SUGAR_AGING_PROCESS)) {
    int g = S.buildings[B_GRANDMA].amount;
    lump_ripe_age -= 6000.0 * (g < 600 ? g : 600);
  }
  if (G.buildings_owned % 10 == 0) {
    int god = hasgod(GOD_ORDER);
    if (god == 1) lump_ripe_age -= HOUR;
    else if (god == 2) lump_ripe_age -= HOUR / 3 * 2;
    else if (god == 3) lump_ripe_age -= HOUR / 3;
  }
  double curve = 1 + aura_mult(AURA_DRAGONS_CURVE) * 0.05;
  lump_mature_age /= curve, lump_ripe_age /= curve;
  lump_overripe_age = lump_ripe_age + HOUR;
  if (has(U_GLUCOSE_CHARGED_AIR)) lump_mature_age /= 2000, lump_ripe_age /= 2000, lump_overripe_age /= 2000;
}

void gain_lumps(int total) {
  if (S.lumps_total == -1) S.lumps_total = 0, S.lumps = 0;
  S.lumps += total, S.lumps_total += total;
  if (S.lumps_total >= 7) win(A_DUDE_SWEET);
  if (S.lumps_total >= 30) win(A_SUGAR_RUSH);
  if (S.lumps_total >= 365) win(A_YEARS_WORTH_OF_CAVITIES);
}

static void compute_lump_type(void) {
  int types[1 + 4 * 3], n = 0;
  types[n++] = 0;
  double loop = 1 + aura_mult(AURA_DRAGONS_CURVE);
  int l = (int)floor(loop);
  if (rnd() < loop - l) l++;
  for (int i = 0; i < l; i++) {
    if (rnd() < (has(U_SUCRALOSIA_INUTILIS) ? 0.15 : 0.1)) types[n++] = 1;
    if (rnd() < 3.0 / 1000) types[n++] = 2;
    if (rnd() < 0.1 * S.elder_wrath) types[n++] = 3;
    if (rnd() < 1.0 / 50) types[n++] = 4;
  }
  S.lump_type = (uint8_t)types[rnd_int(n)];
}

void harvest_lumps(int amount, bool silent) {
  if (!can_lumps()) return;
  S.lump_t = lump_now();
  double total = amount;
  static const int k1[2] = {1, 2}, k2[6] = {2, 3, 4, 5, 6, 7}, k3[5] = {0, 0, 1, 2, 2}, k4[3] = {1, 2, 3};
  if (S.lump_type == 1 && has(U_SUCRALOSIA_INUTILIS) && rnd() < 0.05) total *= 2;
  else if (S.lump_type == 1) total *= k1[rnd_int(2)];
  else if (S.lump_type == 2) {
    total *= k2[rnd_int(6)];
    gain_buff(BUFF_SUGAR_BLESSING, 24 * 60 * 60, 1, 0);
    double a = G.cookies_ps * 60 * 60 * 24, b = S.cookies;
    earn(a < b ? a : b);
    notify("Sugar blessing activated!", "Your cookies have been doubled.\n+10% golden cookies for the next 24 hours.",
           ICON(29, 16), 0);
  } else if (S.lump_type == 3)
    total *= k3[rnd_int(5)];
  else if (S.lump_type == 4) {
    total *= k4[rnd_int(3)];
    S.lump_refill = 0;
    notify("Sugar lump cooldowns cleared!", "", ICON(29, 27), 0);
  }
  int t = (int)floor(total);
  gain_lumps(t);
  if (S.lump_type == 1) win(A_SUGAR_SUGAR);
  else if (S.lump_type == 2) win(A_ALL_NATURAL_CANE_SUGAR);
  else if (S.lump_type == 3) win(A_SWEETMEATS);
  else if (S.lump_type == 4) win(A_MAILLARD_REACTION);
  if (!silent) {
    char d[48];
    if (t > 0) snprintf(d, sizeof d, "+%d sugar lump%s", t, t == 1 ? "" : "s");
    else snprintf(d, sizeof d, "Botched harvest!");
    popup(d, LEFT_W / 2, NEWS_H + 30);
  }
  compute_lump_times();
}

/* harvesting by hand: a mature lump has one chance in two to give something, a ripe one gives */
void click_lump(void) {
  if (!can_lumps()) return;
  double age = lump_age();
  if (age < lump_mature_age) return;
  if (age < lump_ripe_age) {
    int a = rnd_int(2);
    if (a) win(A_HAND_PICKED);
    harvest_lumps(a, false);
    compute_lump_type();
  } else if (age < lump_overripe_age) {
    harvest_lumps(1, false);
    compute_lump_type();
  }
}

void lumps_new(void) {
  S.lumps = -1, S.lumps_total = -1, S.lump_t = 0, S.lump_refill = 0, S.lump_type = 0;
}
void lumps_reset(bool hard) {
  if (hard) lumps_new();
}

void do_lumps(void) {
  if (S.lump_refill > 0) S.lump_refill--;
  if (!can_lumps()) return;
  if (S.lumps_total == -1) {
    S.lump_t = lump_now(), S.lumps_total = 0, S.lumps = 0;
    compute_lump_type();
    notify("Sugar lumps!",
           "Because you've baked a \x01billion cookies\x02 in total, you are now attracting \x01sugar lumps\x02. "
           "They coalesce quietly near the top of your screen.\nYou will be able to harvest them when they're "
           "ripe, after which you may spend them on all sorts of things!",
           ICON(23, 14), 0);
  }
  if (lump_age() > lump_overripe_age) {
    harvest_lumps(1, false);
    compute_lump_type();
  }
}

/* the lump's icon and the next stage's, and how far between them (0..1) */
void lump_icons(int *icon, int *icon2, float *opacity) {
  double age = lump_age();
  int phase = (int)floor(age / lump_overripe_age * 7), phase2 = phase + 1;
  if (phase > 6) phase = 6;
  if (phase2 > 6) phase2 = 6;
  int row = 14, row2 = 14, type = S.lump_type;
  if (type == 1) {
    if (phase2 >= 6) row2 = 15;
  } else if (type == 2) {
    if (phase >= 4) row = 16;
    if (phase2 >= 4) row2 = 16;
  } else if (type == 3) {
    if (phase >= 4) row = 17;
    if (phase2 >= 4) row2 = 17;
  } else if (type == 4) {
    if (phase >= 4) row = 27;
    if (phase2 >= 4) row2 = 27;
  }
  *icon = ICON(23 + (phase < 5 ? phase : 5), row);
  *icon2 = ICON(23 + phase2, row2);
  double o = age / lump_overripe_age * 7;
  if (o > 6) o = 6;
  *opacity = (float)(o - floor(o));
  if (phase >= 6) *opacity = 1;
}

/* the lump refill (minigames): 15 minutes between uses */
#define LUMP_REFILL_MAX (FPS * 60 * 15)
bool can_refill_lump(void) { return S.lump_refill <= 0; }
bool refill_lump(int n) {
  if (S.lumps < n || !can_refill_lump()) return false;
  S.lumps -= n;
  S.lump_refill = LUMP_REFILL_MAX;
  G.recalculate = true;
  return true;
}

/* a building's level: level + 1 lumps */
bool level_up(int b) {
  Building *m = &S.buildings[b];
  if (!can_lumps() || S.ascension_mode == 1 || S.lumps < m->level + 1) return false;
  S.lumps -= m->level + 1;
  m->level++;
  if (m->level >= 10 && building_links(b)->level10 != NONE) win(building_links(b)->level10);
  extern void building_levelled(int b);
  building_levelled(b);
  G.recalculate = true, G.store_dirty = true;
  return true;
}
