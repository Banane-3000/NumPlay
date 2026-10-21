/* The grandmapocalypse and its wrinklers (the game's UpdateGrandmapocalypse, UpdateWrinklers): the
 * elders' wrath grows with the research, the pledge holds it back for a while, the covenant for good;
 * wrinklers come to the big cookie and eat a twentieth of its production each, and give it back,
 * and more, when popped. Var hits them (three times to pop one), the fullest first. */
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "game.h"

#define WRINKLER_HP 2.1f
#define WRINKLER_LIMIT 14
float elder_wrath_d = 1;

int wrinklers_max(void) {
  int n = 10;
  if (has(U_ELDER_SPICE)) n += 2;
  n += (int)roundf(aura_mult(AURA_DRAGON_GUTS) * 2);
  return n < WRINKLER_LIMIT ? n : WRINKLER_LIMIT;
}
void collect_wrinklers(void) {
  for (int i = 0; i < MAX_WRINKLERS; i++) S.wrinklers[i].hp = 0;
}
float wrinkler_eat_eff(void) {
  extern float minigame_eff(int which);
  return minigame_eff(1);
}

void update_grandmapocalypse(void) {
  if (has(U_ELDER_COVENANT) || S.buildings[B_GRANDMA].amount == 0) S.elder_wrath = 0;
  else if (S.pledge_t > 0) {
    S.pledge_t--;
    if (S.pledge_t == 0) lock(U_ELDER_PLEDGE), unlock(U_ELDER_PLEDGE), S.elder_wrath = 1;
  } else {
    if (has(U_ONE_MIND) && S.elder_wrath == 0) S.elder_wrath = 1;
    if (rnd() < 0.001 && S.elder_wrath < has(U_ONE_MIND) + has(U_COMMUNAL_BRAINSWEEP) + has(U_ELDER_PACT)) S.elder_wrath++;
    if (has(U_ELDER_PACT) && !has_unlocked(U_ELDER_PLEDGE)) lock(U_ELDER_PLEDGE), unlock(U_ELDER_PLEDGE);
  }
  static int old = -1;
  elder_wrath_d += ((S.elder_wrath + 1) - elder_wrath_d) * 0.001f;
  if (S.elder_wrath != old) G.store_dirty = true;
  old = S.elder_wrath;
}

static void spawn(Wrinkler *w) {
  w->phase = 1, w->hp = WRINKLER_HP, w->type = 0, w->clicks = 0;
  if (rnd() < 0.0001) w->type = 1;   /* a shiny one */
}

/* the Grimoire's Resurrect Abomination, and its backfire */
bool spawn_wrinkler(void) {
  int max = wrinklers_max(), n = 0;
  for (int i = 0; i < MAX_WRINKLERS; i++) n += S.wrinklers[i].phase > 0;
  for (int i = 0; i < MAX_WRINKLERS && i < max; i++)
    if (S.wrinklers[i].phase == 0 && S.elder_wrath > 0 && n < max) {
      spawn(&S.wrinklers[i]);
      return true;
    }
  return false;
}
bool pop_random_wrinkler(void) {
  Wrinkler *list[MAX_WRINKLERS];
  int n = 0;
  for (int i = 0; i < MAX_WRINKLERS; i++)
    if (S.wrinklers[i].phase > 0 && S.wrinklers[i].hp > 0) list[n++] = &S.wrinklers[i];
  if (!n) return false;
  list[rnd_int(n)]->hp = -10;
  return true;
}

/* var: a click on the wrinkler that ate the most */
void wrinkler_hit(void) {
  Wrinkler *best = NULL;
  for (int i = 0; i < MAX_WRINKLERS; i++) {
    Wrinkler *w = &S.wrinklers[i];
    if (w->phase > 0 && w->hp > 0 && (!best || w->sucked > best->sucked)) best = w;
  }
  if (!best) return;
  best->clicks++;
  if (best->clicks >= 50) win(A_WRINKLER_POKER);
  best->hurt = 1;
  best->hp -= 0.75f;
  for (int i = 0; i < MAX_WRINKLERS; i++) S.wrinklers[i].selected = &S.wrinklers[i] == best;
}

static void pop(Wrinkler *w) {
  S.wrinklers_popped++;
  G.recalculate = true;
  w->phase = 0, w->close = 0, w->hurt = 0;
  w->hp = WRINKLER_HP * (w->type == 1 ? 3 : 1);
  double to_suck = 1.1;
  if (has(U_SACRILEGIOUS_CORRUPTION)) to_suck *= 1.05;
  to_suck *= 1 + aura_mult(AURA_DRAGON_GUTS) * 0.2;
  if (w->type == 1) to_suck *= 3;
  w->sucked *= to_suck;
  if (has(U_WRINKLERSPAWN)) w->sucked *= 1.05;
  static const float scorn[4] = {1, 1.15f, 1.1f, 1.05f};
  w->sucked *= scorn[hasgod(GOD_SCORN)];
  if (w->sucked > 0.5) {
    char t[48], d[96];
    cookies_str(w->sucked, 0, t);
    snprintf(d, sizeof d, "Found \x01%s\x02!", t);
    notify(w->type == 1 ? "Exploded a shiny wrinkler" : "Exploded a wrinkler", d, ICON(19, 8), 6);
    snprintf(d, sizeof d, "+%s!", t);
    popup(d, w->x, w->y);
    if (!has_unlocked(U_WRINKLER_AMBERGRIS) && rnd() < 1.0 / (w->type == 1 ? 1000 : 10000)) {
      unlock(U_WRINKLER_AMBERGRIS);
      snprintf(d, sizeof d, "You also found \x01%s\x02!", res_text(upgrade_rec(U_WRINKLER_AMBERGRIS)->name));
      notify(res_text(upgrade_rec(U_WRINKLER_AMBERGRIS)->name), d, upgrade_rec(U_WRINKLER_AMBERGRIS)->icon, 0);
    }
    if (S.season == SEASON_HALLOWEEN) {
      double fail = 0.95;
      if (has_achiev(A_SPOOKY_COOKIES)) fail *= 0.8;
      if (has(U_STARTERROR)) fail *= 0.9;
      static const float sg[4] = {1, 0.9f, 0.95f, 0.97f};
      fail *= sg[hasgod(GOD_SEASONS)];
      if (w->type == 1) fail *= 0.9;
      fail = pow(fail, drop_rate_mult());
      if (rnd() > fail) {
        static const uint16_t c[7] = {U_SKULL_COOKIES, U_GHOST_COOKIES, U_BAT_COOKIES, U_SLIME_COOKIES,
                                      U_PUMPKIN_COOKIES, U_EYEBALL_COOKIES, U_SPIDER_COOKIES};
        int u = c[rnd_int(7)];
        if (!has_unlocked(u) && !has(u)) {
          unlock(u);
          snprintf(d, sizeof d, "You also found \x01%s\x02!", res_text(upgrade_rec(u)->name));
          notify(res_text(upgrade_rec(u)->name), d, upgrade_rec(u)->icon, 0);
        }
      }
    }
    extern void drop_egg(double failrate);
    drop_egg(0.98);
  }
  if (w->type == 1) win(A_LAST_CHANCE_TO_SEE);
  earn(w->sucked);
  if (S.prefs.particles && w->sucked > 0)
    for (int k = 0; k < 5; k++)
      particle_add(w->x, w->y, (float)(rnd() * 4 - 2), (float)(rnd() * -2 - 2), (float)(rnd() * 0.5 + 0.75), 1, 2);
  w->sucked = 0;
}

void wrinklers_update(void) {
  float T = G.real_t;
  int max = wrinklers_max(), n = 0;
  for (int i = 0; i < MAX_WRINKLERS; i++) n += S.wrinklers[i].phase > 0;
  for (int i = 0; i < MAX_WRINKLERS; i++) {
    Wrinkler *w = &S.wrinklers[i];
    if (w->phase == 0 && S.elder_wrath > 0 && n < max && i < max) {
      double chance = 0.00001 * S.elder_wrath;
      extern float minigame_eff(int which);
      chance *= minigame_eff(2);
      if (has(U_UNHOLY_BAIT)) chance *= 5;
      static const float scorn[4] = {1, 2.5f, 2, 1.5f};
      chance *= scorn[hasgod(GOD_SCORN)];
      if (has(U_WRINKLER_DOORMAT)) chance = 0.1;
      if (rnd() < chance) spawn(w), n++;
    }
    if (w->phase > 0) {
      if (w->close < 1) w->close += 1.f / FPS / 10;
      if (w->close > 1) w->close = 1;
      if (i >= max) w->hp = 0;
    } else
      w->close = 0;
    if (w->close == 1 && w->phase == 1) w->phase = 2, G.recalculate = true;
    if (w->phase == 2) w->sucked += G.cookies_ps / FPS * G.cps_sucked;
    if (w->phase > 0) {
      float top = WRINKLER_HP * (w->type == 1 ? 3 : 1);
      if (w->hp < top) w->hp += 0.04f;
      if (w->hp > top) w->hp = top;
      float d = 128 * 0.37f * (2 - w->close);
      if (S.prefs.fancy) d += cosf(T * 0.05f + i) * 4 * 0.37f;
      w->r = (float)i / max * 360;
      if (S.prefs.fancy) w->r += sinf(T * 0.05f + i) * 4;
      w->x = COOKIE_X + sinf(w->r * (float)M_PI / 180) * d;
      w->y = COOKIE_Y + cosf(w->r * (float)M_PI / 180) * d;
      if (S.prefs.fancy) w->r += sinf(T * 0.09f + i) * 4;
      if (rnd() < 0.01 && !S.prefs.not_scary) w->hurt = w->hurt > (float)rnd() ? w->hurt : (float)rnd();
    }
    if (w->hurt > 0) {
      w->hurt -= 5.f / FPS;
      w->r += sinf(T) * w->hurt * 18;
    }
    if (w->hp <= 0.5f && w->phase > 0) pop(w);
  }
}
