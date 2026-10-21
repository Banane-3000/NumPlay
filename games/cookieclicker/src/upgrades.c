/* The upgrades that do more than be had: their buyFunctions, the prices the game works out, the
 * switches, research, the elder pledge and covenant, the seasons' biscuits, and the cookies'
 * powers. */
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "game.h"

static bool in_list(const uint16_t *l, int n, int u) {
  for (int i = 0; i < n; i++)
    if (l[i] == u) return true;
  return false;
}

/* research: the bingo center finds the next upgrade in 30 minutes (3 with Persistent memory, 5 seconds
 * with Ultrascience) */
#define BASE_RESEARCH_TIME (FPS * 60 * 30)
void set_research(int u) {
  if (has(u)) return;
  S.research_t = BASE_RESEARCH_TIME;
  if (has(U_PERSISTENT_MEMORY)) S.research_t = (BASE_RESEARCH_TIME + 9) / 10;
  if (has(U_ULTRASCIENCE)) S.research_t = FPS * 5;
  S.next_research = u;
  notify("Research has begun", "Your bingo center/research facility is conducting experiments.", ICON(9, 0), 0);
}
int32_t pledge_duration(void) { return FPS * 60 * (has(U_SACRIFICIAL_ROLLING_PINS) ? 60 : 30); }
extern void collect_wrinklers(void);
extern void season_trigger(int u);
extern void santa_drop(void);

void upgrade_buy_function(int u) {
  switch (u) {
    case U_BINGO_CENTER_RESEARCH_FACILITY: set_research(U_SPECIALIZED_CHOCOLATE_CHIPS); break;
    case U_SPECIALIZED_CHOCOLATE_CHIPS: set_research(U_DESIGNER_COCOA_BEANS); break;
    case U_DESIGNER_COCOA_BEANS: set_research(U_RITUAL_ROLLING_PINS); break;
    case U_RITUAL_ROLLING_PINS: set_research(U_UNDERWORLD_OVENS); break;
    case U_UNDERWORLD_OVENS: set_research(U_ONE_MIND); break;
    case U_ONE_MIND: S.elder_wrath = 1, set_research(U_EXOTIC_NUTS), G.store_dirty = true; break;
    case U_EXOTIC_NUTS: set_research(U_COMMUNAL_BRAINSWEEP); break;
    case U_COMMUNAL_BRAINSWEEP: S.elder_wrath = 2, set_research(U_ARCANE_SUGAR), G.store_dirty = true; break;
    case U_ARCANE_SUGAR: set_research(U_ELDER_PACT); break;
    case U_ELDER_PACT: S.elder_wrath = 3, G.store_dirty = true; break;
    case U_ELDER_PLEDGE:
      S.elder_wrath = 0, S.pledges++, S.pledge_t = pledge_duration();
      unlock(U_ELDER_COVENANT);
      collect_wrinklers();
      G.store_dirty = true;
      break;
    case U_ELDER_COVENANT:
      S.pledge_t = 0;
      lock(U_REVOKE_ELDER_COVENANT), unlock(U_REVOKE_ELDER_COVENANT), lock(U_ELDER_PLEDGE);
      win(A_ELDER_CALM);
      collect_wrinklers();
      G.store_dirty = true;
      break;
    case U_REVOKE_ELDER_COVENANT: lock(U_ELDER_COVENANT), unlock(U_ELDER_COVENANT); break;
    case U_A_FESTIVE_HAT: santa_drop(); break;
    case U_SEASON_SWITCHER:
      unlock(U_FESTIVE_BISCUIT), unlock(U_GHOSTLY_BISCUIT), unlock(U_LOVESICK_BISCUIT), unlock(U_FOOLS_BISCUIT),
          unlock(U_BUNNY_BISCUIT);
      break;
    case U_FESTIVE_BISCUIT: case U_GHOSTLY_BISCUIT: case U_LOVESICK_BISCUIT: case U_FOOLS_BISCUIT:
    case U_BUNNY_BISCUIT: season_trigger(u); break;
    case U_CHOCOLATE_EGG: {
      double c = S.cookies * 0.05;
      char t[64], d[96];
      beautify(c, 0, t);
      snprintf(d, sizeof d, "The egg bursts into \x01%s\x02 cookies!", t);
      notify("Chocolate egg", d, upgrade_rec(U_CHOCOLATE_EGG)->icon, 0);
      earn(c);
      break;
    }
    case U_SUGAR_FRENZY:
      gain_buff(BUFF_SUGAR_FRENZY, 60 * 60, 3, 0);
      notify("Sugar frenzy!", "CpS x3 for 1 hour!", ICON(29, 14), 0);
      break;
    case U_TOY_WORKSHOP: case U_FIVE_FINGER_DISCOUNT: case U_WRINKLER_AMBERGRIS: G.upgrades_dirty = true; break;
    default: break;
  }
}

/* switches: one turns into the other */
int upgrade_toggle_into(int u) {
  switch (u) {
    case U_GOLDEN_SWITCH_OFF: return U_GOLDEN_SWITCH_ON;
    case U_GOLDEN_SWITCH_ON: return U_GOLDEN_SWITCH_OFF;
    case U_SHIMMERING_VEIL_OFF: return U_SHIMMERING_VEIL_ON;
    case U_SHIMMERING_VEIL_ON: return U_SHIMMERING_VEIL_OFF;
    default: return -1;
  }
}

/* the store shows these even once had: the pledge running out, the selectors */
bool upgrade_display_when_owned(int u) {
  return u == U_ELDER_PLEDGE || u == U_FESTIVE_BISCUIT || u == U_GHOSTLY_BISCUIT || u == U_LOVESICK_BISCUIT ||
         u == U_FOOLS_BISCUIT || u == U_BUNNY_BISCUIT;
}

int upgrade_lumps(int u) { return u == U_SUGAR_FRENZY ? 1 : 0; }
int upgrade_can_buy_special(int u) {
  (void)u;
  return -1;
}

extern int how_many_eggs(void);
extern double season_price(int u);
extern int dragon_level_count(void);
double special_upgrade_price(int u, double price) {
  const UpgradeRec *r = upgrade_rec(u);
  switch (u) {
    case U_INCREASED_MERRINESS: case U_IMPROVED_JOLLINESS: case U_A_LUMP_OF_COAL: case U_AN_ITCHY_SWEATER:
    case U_REINDEER_BAKING_GROUNDS: case U_WEIGHTED_SLEIGHS: case U_HO_HO_HO_FLAVORED_FROSTING:
    case U_SEASON_SAVINGS: case U_TOY_WORKSHOP: case U_NAUGHTY_LIST: case U_SANTAS_BOTTOMLESS_BAG:
    case U_SANTAS_HELPERS: case U_SANTAS_LEGACY: case U_SANTAS_MILK_AND_COOKIES:
      return pow(3, S.santa_level) * 2525;
    case U_CHICKEN_EGG: case U_DUCK_EGG: case U_TURKEY_EGG: case U_QUAIL_EGG: case U_ROBIN_EGG: case U_OSTRICH_EGG:
    case U_CASSOWARY_EGG: case U_SALMON_ROE: case U_FROGSPAWN: case U_SHARK_EGG: case U_TURTLE_EGG: case U_ANT_LARVA:
      return pow(2, how_many_eggs()) * 999;
    case U_GOLDEN_GOOSE_EGG: case U_FABERGE_EGG: case U_WRINKLERSPAWN: case U_COOKIE_EGG: case U_OMELETTE:
    case U_CHOCOLATE_EGG: case U_CENTURY_EGG: case U_EGG:
      return pow(3, how_many_eggs()) * 999;
    case U_FORTUNE_100: case U_FORTUNE_101: case U_FORTUNE_102: case U_FORTUNE_103: case U_FORTUNE_104: {
      double c = G.unbuffed_cps * 60 * 60 * 24;
      return r->price < c ? r->price : c;
    }
    case U_DRAGON_SCALE: case U_DRAGON_CLAW: case U_DRAGON_FANG: case U_DRAGON_TEDDY_BEAR:
      return G.unbuffed_cps * 60 * 30 * (S.dragon_level < dragon_level_count() - 1 ? 1 : 0.1);
    case U_FESTIVE_BISCUIT: case U_GHOSTLY_BISCUIT: case U_LOVESICK_BISCUIT: case U_FOOLS_BISCUIT:
    case U_BUNNY_BISCUIT: return season_price(u);
    default: break;
  }
  extern double garden_drop_price(int u, double price);
  return garden_drop_price(u, price);
}

/* a cookie upgrade's production bonus (%); the heart biscuits' grows with Starlove and the Pantheon */
double cookie_upgrade_power(int u) {
  if (in_list(LIST(HEART_DROPS), N_HEART_DROPS, u)) {
    double pow_ = 2;
    if (has(U_STARLOVE)) pow_ = 3;
    if (S.season == SEASON_VALENTINES) {
      int god = hasgod(GOD_SEASONS);
      if (god == 1) pow_ *= 1.3;
      else if (god == 2) pow_ *= 1.2;
      else if (god == 3) pow_ *= 1.1;
    }
    return pow_;
  }
  return upgrade_rec(u)->power;
}

/* what particles fall as: a cookie the bakery has, or the plain one */
int particle_icon(void) {
  if (rnd() < 1.0 / 10000) return ICON(17, 5);
  int n = 1;
  for (int i = 0; i < N_COOKIE_UPGRADES; i++) n += has(LIST(COOKIE_UPGRADES)[i]);
  int k = rnd_int(n);
  for (int i = 0; i < N_COOKIE_UPGRADES && k; i++)
    if (has(LIST(COOKIE_UPGRADES)[i]) && --k == 0) return upgrade_rec(LIST(COOKIE_UPGRADES)[i])->icon;
  return ICON(10, 0);
}
