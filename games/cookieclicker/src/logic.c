/* Cookie Clicker's rules: cookies, buildings, upgrades, achievements, the store, as the game's
 * main.js (2.058) has them: its Game.CalculateGains, Game.mouseCps, the buildings' cps
 * functions, Game.Upgrade's prices and purchases, Game.RebuildUpgrades, Game.Logic and its
 * checks every 5 seconds. */
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "game.h"

GameState G;

/* ------------------------------------------------------------------ random numbers */
double rnd(void) {
  G.seed ^= G.seed << 13;
  G.seed ^= G.seed >> 17;
  G.seed ^= G.seed << 5;
  return (G.seed >> 8) / 16777216.0;
}
int rnd_int(int n) { return n > 0 ? (int)(rnd() * n) : 0; }

/* ------------------------------------------------------------------ upgrades and achievements */
bool counts_as_upgrade_owned(int u) {
  int p = upgrade_rec(u)->pool;
  return p == POOL_NORMAL || p == POOL_COOKIE || p == POOL_TECH;
}
bool has(int u) {
  if ((unsigned)u >= N_UPGRADES) return false;
  if (S.ascension_mode == 1) {
    const UpgradeRec *r = upgrade_rec(u);
    if (r->pool == POOL_PRESTIGE || r->tier == TIER_FORTUNE) return false;
  }
  return bit_get(S.upgrades_bought, u);
}
void unlock(int u) {
  if ((unsigned)u >= N_UPGRADES || has_unlocked(u)) return;
  bit_set(S.upgrades_unlocked, u, true);
  G.upgrades_dirty = true, G.recalculate = true;
}
void lock(int u) {
  if ((unsigned)u >= N_UPGRADES) return;
  bit_set(S.upgrades_unlocked, u, false);
  G.upgrades_dirty = true;
  if (bit_get(S.upgrades_bought, u) && counts_as_upgrade_owned(u)) G.upgrades_owned--;
  bit_set(S.upgrades_bought, u, false);
  G.recalculate = true;
}
static void upgrade_bought_effects(int u);
void earn_upgrade(int u) {
  bit_set(S.upgrades_unlocked, u, true);
  if (bit_get(S.upgrades_bought, u)) return;
  bit_set(S.upgrades_bought, u, true);
  upgrade_bought_effects(u);
  G.upgrades_dirty = true, G.recalculate = true;
  if (counts_as_upgrade_owned(u)) G.upgrades_owned++;
}
void win(int a) {
  if ((unsigned)a >= N_ACHIEVS || has_achiev(a)) return;
  bit_set(S.achievements, a, true);
  const AchievRec *r = achiev_rec(a);
  notify("Achievement unlocked", res_text(r->name), r->icon, 0);
  if (r->pool == APOOL_NORMAL) G.achievements_owned++;
  G.recalculate = true;
}

/* ------------------------------------------------------------------ buffs (the buffs' effects on prices etc.) */
Buff *find_buff(int type) {
  for (int i = 0; i < MAX_BUFFS; i++)
    if (S.buffs[i].used && S.buffs[i].type == type && S.buffs[i].time > 0) return &S.buffs[i];
  return NULL;
}
bool has_buff(int type) { return find_buff(type) != NULL; }

/* the Pantheon and the dragon: their effects, once their parts are in */
extern float dragon_aura_mult(int aura);
float aura_mult(int aura) { return dragon_aura_mult(aura); }
int hasgod(int god) {
  for (int i = 0; i < 3; i++)
    if (S.gods[i] == god + 1 && G.s.buildings[B_TEMPLE].level > 0 && S.ascension_mode != 1)
      return aura_mult(AURA_SUPREME_INTELLECT) > 0 ? (i > 1 ? i : 1) : i + 1;   /* (Supreme Intellect: a slot up) */
  return 0;
}


/* ------------------------------------------------------------------ prices */
static double modify_building_price(int b, double price) {
  if (has(U_SEASON_SAVINGS)) price *= 0.99;
  if (has(U_SANTAS_DOMINION)) price *= 0.99;
  if (has(U_FABERGE_EGG)) price *= 0.99;
  if (has(U_DIVINE_DISCOUNT)) price *= 0.99;
  if (has(U_FORTUNE_100)) price *= 0.99;
  price *= 1 - aura_mult(AURA_FIERCE_HOARDER) * 0.02;
  if (has_buff(BUFF_EVERYTHING_MUST_GO)) price *= 0.95;
  if (has_buff(BUFF_PIXIE_LUCK)) price *= 0.98;
  if (has_buff(BUFF_PIXIE_MISERY)) price *= 1.02;
  uint16_t f = building_links(b)->fortune;
  if (f != NONE && has(f)) price *= 0.93;
  price *= eff(EFF_BUILDING_COST);
  int god = hasgod(GOD_CREATION);
  if (god == 1) price *= 0.93;
  else if (god == 2) price *= 0.95;
  else if (god == 3) price *= 0.98;
  return price;
}
double building_price(int b) {
  const Building *m = &S.buildings[b];
  int n = m->amount - m->free;
  double price = building_rec(b)->price * pow(1.15, n > 0 ? n : 0);
  return ceil(modify_building_price(b, price));
}
double building_sum_price(int b, int amount) {
  const Building *m = &S.buildings[b];
  double price = 0, base = building_rec(b)->price;
  int from = m->amount > 0 ? m->amount : 0;
  for (int i = from; i < from + amount; i++) price += base * pow(1.15, i - m->free > 0 ? i - m->free : 0);
  return ceil(modify_building_price(b, price));
}
static double sell_multiplier(void) { return 0.25 * (1 + aura_mult(AURA_EARTH_SHATTERER)); }
double building_reverse_sum_price(int b, int amount) {
  const Building *m = &S.buildings[b];
  double price = 0, base = building_rec(b)->price;
  int from = m->amount - amount > 0 ? m->amount - amount : 0;
  for (int i = from; i < (m->amount > 0 ? m->amount : 0); i++) price += base * pow(1.15, i - m->free > 0 ? i - m->free : 0);
  price = modify_building_price(b, price) * sell_multiplier();
  return ceil(price);
}
void building_bulk(int b, double *price, int *n) {
  int bulk = G.buy_bulk;
  if (G.buy_mode == 1) {
    if (bulk < 1) bulk = 1;
    *n = bulk, *price = building_sum_price(b, bulk);
  } else {
    int have = S.buildings[b].amount;
    int k = bulk == -1 ? have : bulk < have ? bulk : have;
    *n = k, *price = building_reverse_sum_price(b, k);
  }
}

double upgrade_price(int u) {
  const UpgradeRec *r = upgrade_rec(u);
  double price = r->price;
  switch (u) {   /* the upgrades whose price the game works out */
    case U_ELDER_PLEDGE: price = pow(8, S.pledges + 2 < 14 ? S.pledges + 2 : 14); break;
    case U_GOLDEN_SWITCH_OFF: case U_GOLDEN_SWITCH_ON: price = G.cookies_ps * 60 * 60; break;
    case U_SHIMMERING_VEIL_OFF: price = G.unbuffed_cps * 60 * 60 * 24; break;
    default: break;
  }
  if (r->tier == TIER_SYNERGY1 || r->tier == TIER_SYNERGY2)
    price = (building_rec(r->building)->price * 10 + building_rec(r->building2)->price) * tier_rec(r->tier)->price *
            (has(U_CHIMERA) ? 0.98 : 1);
  extern double special_upgrade_price(int u, double price);
  price = special_upgrade_price(u, price);
  if (price == 0) return 0;
  if (r->pool != POOL_PRESTIGE) {
    if (has(U_TOY_WORKSHOP)) price *= 0.95;
    if (has(U_FIVE_FINGER_DISCOUNT)) price *= pow(0.99, S.buildings[B_CURSOR].amount / 100.0);
    if (has(U_SANTAS_DOMINION)) price *= 0.98;
    if (has(U_FABERGE_EGG)) price *= 0.99;
    if (has(U_DIVINE_SALES)) price *= 0.99;
    if (has(U_FORTUNE_100)) price *= 0.99;
    if (has(U_WRINKLER_AMBERGRIS)) price *= 0.99;
    if (r->kitten && has(U_KITTEN_WAGES)) price *= 0.9;
    if (has_buff(BUFF_HAGGLER_LUCK)) price *= 0.98;
    if (has_buff(BUFF_HAGGLER_MISERY)) price *= 1.02;
    price *= 1 - aura_mult(AURA_MASTER_OF_THE_ARMORY) * 0.02;
    price *= eff(EFF_UPGRADE_COST);
    if (r->pool == POOL_COOKIE && has(U_DIVINE_BAKERIES)) price /= 5;
  }
  return ceil(price);
}
static int upgrade_price_lumps(int u) {
  extern int upgrade_lumps(int u);
  return upgrade_lumps(u);
}
bool upgrade_can_buy(int u) {
  extern int upgrade_can_buy_special(int u);   /* -1: the usual rule */
  int s = upgrade_can_buy_special(u);
  if (s >= 0) return s;
  int lumps = upgrade_price_lumps(u);
  if (lumps > 0) return S.lumps >= lumps;
  return S.cookies >= upgrade_price(u);
}
bool upgrade_needs_confirm(int u) {
  for (int i = 0; i < N_CONFIRM; i++)
    if (LIST(CONFIRM)[i] == u) return true;
  return false;
}

/* what buying an upgrade does besides having it (the upgrades' buyFunctions) */
extern void upgrade_buy_function(int u);
static void upgrade_bought_effects(int u) { upgrade_buy_function(u); }

bool upgrade_buy(int u, bool confirmed) {
  const UpgradeRec *r = upgrade_rec(u);
  if (bit_get(S.upgrades_bought, u)) {
    extern bool season_cancel(int u);
    return season_cancel(u);
  }
  if (!confirmed && upgrade_needs_confirm(u)) return false;
  if (r->pool == POOL_PRESTIGE) {
    double price = upgrade_price(u);
    if (S.heavenly_chips < price) return false;
    S.heavenly_chips -= price, S.heavenly_chips_spent += price;
    bit_set(S.upgrades_unlocked, u, true);
    bit_set(S.upgrades_bought, u, true);
    upgrade_bought_effects(u);
    return true;
  }
  if (!upgrade_can_buy(u)) return false;
  int lumps = upgrade_price_lumps(u);
  if (lumps > 0) S.lumps -= lumps;
  else spend(upgrade_price(u));
  bit_set(S.upgrades_bought, u, true);
  upgrade_bought_effects(u);
  extern int upgrade_toggle_into(int u);
  int into = upgrade_toggle_into(u);
  if (into >= 0) lock(into), unlock(into);
  G.upgrades_dirty = true, G.recalculate = true;
  if (counts_as_upgrade_owned(u)) G.upgrades_owned++;
  return true;
}

/* ------------------------------------------------------------------ cookies */
void earn(double v) {
  if (G.on_ascend) return;
  S.cookies += v, S.cookies_earned += v;
}
void spend(double v) {
  if (G.on_ascend) return;
  S.cookies -= v;
}
void dissolve(double v) {
  if (G.on_ascend) return;
  S.cookies -= v, S.cookies_earned -= v;
  if (S.cookies < 0) S.cookies = 0;
  if (S.cookies_earned < 0) S.cookies_earned = 0;
}

static double compute_cps(double base, int mult, double bonus) { return base * pow(2, mult) + bonus; }

/* the cursor's thousand fingers and their multipliers */
static double fingers(void) {
  static const struct { uint16_t u; float m; } f[] = {
    {U_MILLION_FINGERS, 5}, {U_BILLION_FINGERS, 10}, {U_TRILLION_FINGERS, 20}, {U_QUADRILLION_FINGERS, 20},
    {U_QUINTILLION_FINGERS, 20}, {U_SEXTILLION_FINGERS, 20}, {U_SEPTILLION_FINGERS, 20}, {U_OCTILLION_FINGERS, 20},
    {U_NONILLION_FINGERS, 20}, {U_DECILLION_FINGERS, 20}, {U_UNDECILLION_FINGERS, 20}, {U_UNSHACKLED_CURSORS, 25}};
  double add = 0;
  if (has(U_THOUSAND_FINGERS)) add += 0.1;
  for (unsigned i = 0; i < sizeof f / sizeof f[0]; i++)
    if (has(f[i].u)) add *= f[i].m;
  return add;
}

double tiered_cps_mult(int b) {
  const BuildingLinks *l = building_links(b);
  double mult = 1;
  uint16_t unshackle = l->unshackle;
  for (int t = 0; t < N_TIERS; t++) {
    uint16_t u = l->tiered[t];
    if (u == NONE || tier_rec(t)->special || !has(u)) continue;
    double tm = 2;
    uint16_t tu = LIST(TIER_UNSHACKLE)[t];
    if (S.ascension_mode != 1 && unshackle != NONE && has(unshackle) && tu != NONE && has(tu))
      tm += b == 1 ? 0.5 : (20 - b) * 0.1;
    mult *= tm;
  }
  for (int i = 0; i < 6 && l->synergies[i] != NONE; i++) {
    const UpgradeRec *s = upgrade_rec(l->synergies[i]);
    if (!has(l->synergies[i])) continue;
    if (s->building == b) mult *= 1 + 0.05 * S.buildings[s->building2].amount;
    else if (s->building2 == b) mult *= 1 + 0.001 * S.buildings[s->building].amount;
  }
  if (l->fortune != NONE && has(l->fortune)) mult *= 1.07;
  if (l->grandma != NONE && has(l->grandma)) mult *= 1 + S.buildings[B_GRANDMA].amount * 0.01 * (1.0 / (b - 1));
  return mult;
}

static double building_cps(int b) {
  const BuildingRec *r = building_rec(b);
  if (b == B_CURSOR) {
    double add = fingers();
    int num = 0;
    for (int i = 0; i < NB; i++)
      if (i != B_CURSOR) num += S.buildings[i].amount;
    add *= num;
    double mult = tiered_cps_mult(b);
    mult *= eff(EFF_CURSOR_CPS);
    return compute_cps(0.1, has(U_REINFORCED_INDEX_FINGER) + has(U_CARPAL_TUNNEL_PREVENTION_CREAM) + has(U_AMBIDEXTROUS),
                       add) * mult;
  }
  if (b == B_GRANDMA) {
    double mult = 1;
    for (int i = 0; i < N_GRANDMA_SYNERGIES; i++)
      if (has(LIST(GRANDMA_SYNERGIES)[i])) mult *= 2;
    if (has(U_BINGO_CENTER_RESEARCH_FACILITY)) mult *= 4;
    if (has(U_RITUAL_ROLLING_PINS)) mult *= 2;
    if (has(U_NAUGHTY_LIST)) mult *= 2;
    if (has(U_ELDERWORT_BISCUITS)) mult *= 1.02;
    mult *= eff(EFF_GRANDMA_CPS);
    if (has(U_CAT_LADIES))
      for (int i = 0; i < N_KITTENS; i++)
        if (has(LIST(KITTENS)[i])) mult *= 1.29;
    mult *= tiered_cps_mult(b);
    double add = 0;
    if (has(U_ONE_MIND)) add += S.buildings[B_GRANDMA].amount * 0.02;
    if (has(U_COMMUNAL_BRAINSWEEP)) add += S.buildings[B_GRANDMA].amount * 0.02;
    if (has(U_ELDER_PACT)) add += S.buildings[B_PORTAL].amount * 0.05;
    int num = 0;
    for (int i = 0; i < NB; i++)
      if (i != B_GRANDMA) num += S.buildings[i].amount;
    mult *= 1 + aura_mult(AURA_ELDER_BATTALION) * 0.01 * num;
    return (r->cps + add) * mult;
  }
  return r->cps * tiered_cps_mult(b);
}

double mouse_cps(void) {
  double add = fingers();
  int num = 0;
  for (int i = 0; i < NB; i++) num += S.buildings[i].amount;
  num -= S.buildings[B_CURSOR].amount;
  add *= num;
  static const uint16_t mice[] = {U_PLASTIC_MOUSE, U_IRON_MOUSE, U_TITANIUM_MOUSE, U_ADAMANTIUM_MOUSE,
                                  U_UNOBTAINIUM_MOUSE, U_ELUDIUM_MOUSE, U_WISHALLOY_MOUSE, U_FANTASTEEL_MOUSE,
                                  U_NEVERCRACK_MOUSE, U_ARMYTHRIL_MOUSE, U_TECHNOBSIDIAN_MOUSE, U_PLASMARBLE_MOUSE,
                                  U_MIRACULITE_MOUSE, U_AETHERICE_MOUSE, U_OMNIPLAST_MOUSE, U_FORTUNE_104};
  for (unsigned i = 0; i < sizeof mice / sizeof mice[0]; i++)
    if (has(mice[i])) add += G.cookies_ps * 0.01;
  double mult = 1;
  if (has(U_SANTAS_HELPERS)) mult *= 1.1;
  if (has(U_COOKIE_EGG)) mult *= 1.1;
  if (has(U_HALO_GLOVES)) mult *= 1.1;
  if (has(U_DRAGON_CLAW)) mult *= 1.03;
  if (has(U_AURA_GLOVES)) {
    int lv = S.buildings[B_CURSOR].level, cap = has(U_LUMINOUS_GLOVES) ? 20 : 10;
    mult *= 1 + 0.05 * (lv < cap ? lv : cap);
  }
  mult *= eff(EFF_CLICK);
  int god = hasgod(GOD_LABOR);
  if (god == 1) mult *= 1.15;
  else if (god == 2) mult *= 1.1;
  else if (god == 3) mult *= 1.05;
  for (int i = 0; i < MAX_BUFFS; i++)
    if (S.buffs[i].used && S.buffs[i].time > 0) mult *= S.buffs[i].mult_click;
  mult *= 1 + aura_mult(AURA_DRAGON_CURSOR) * 0.05;
  double out = mult * compute_cps(1, has(U_REINFORCED_INDEX_FINGER) + has(U_CARPAL_TUNNEL_PREVENTION_CREAM) +
                                         has(U_AMBIDEXTROUS), add);
  Buff *cf = find_buff(BUFF_CURSED_FINGER);
  if (cf) out = cf->power;
  return out;
}

extern double heavenly_multiplier(void);
extern double veil_boost(void);
extern double cookie_upgrade_power(int u);
extern void minigame_effs(void);
extern void compute_lump_times(void);

void calculate_gains(void) {
  double cps = 0, mult = 1;
  minigame_effs();
  if (S.ascension_mode != 1) mult += S.prestige * 0.01 * heavenly_multiplier();
  mult *= eff(EFF_CPS);
  /* (Heralds: the players with the game's support page open; none here) */
  for (int i = 0; i < N_COOKIE_UPGRADES; i++) {
    int u = LIST(COOKIE_UPGRADES)[i];
    if (has(u)) mult *= 1 + cookie_upgrade_power(u) * 0.01;
  }
  if (has(U_SPECIALIZED_CHOCOLATE_CHIPS)) mult *= 1.01;
  if (has(U_DESIGNER_COCOA_BEANS)) mult *= 1.02;
  if (has(U_UNDERWORLD_OVENS)) mult *= 1.03;
  if (has(U_EXOTIC_NUTS)) mult *= 1.04;
  if (has(U_ARCANE_SUGAR)) mult *= 1.05;
  if (has(U_INCREASED_MERRINESS)) mult *= 1.15;
  if (has(U_IMPROVED_JOLLINESS)) mult *= 1.15;
  if (has(U_A_LUMP_OF_COAL)) mult *= 1.01;
  if (has(U_AN_ITCHY_SWEATER)) mult *= 1.01;
  if (has(U_SANTAS_DOMINION)) mult *= 1.2;
  if (has(U_FORTUNE_100)) mult *= 1.01;
  if (has(U_FORTUNE_101)) mult *= 1.07;
  if (has(U_DRAGON_SCALE)) mult *= 1.03;
  if (has(U_WRINKLER_AMBERGRIS)) mult *= 1.06;
  double build_mult = 1;
  int god = hasgod(GOD_ASCETICISM);
  if (god == 1) mult *= 1.15;
  else if (god == 2) mult *= 1.1;
  else if (god == 3) mult *= 1.05;
  god = hasgod(GOD_AGES);
  if (god) {   /* (the game's real-time cycles: here the calculator's time played) */
    double period = god == 1 ? 3 : god == 2 ? 12 : 24;
    mult *= 1 + 0.15 * sin(S.play_time / 1000 / (60 * 60 * period) * M_PI * 2);
  }
  god = hasgod(GOD_DECADENCE);
  if (god == 1) build_mult *= 0.93;
  else if (god == 2) build_mult *= 0.95;
  else if (god == 3) build_mult *= 0.98;
  god = hasgod(GOD_INDUSTRY);
  if (god == 1) build_mult *= 1.1;
  else if (god == 2) build_mult *= 1.06;
  else if (god == 3) build_mult *= 1.03;
  god = hasgod(GOD_LABOR);
  if (god == 1) build_mult *= 0.97;
  else if (god == 2) build_mult *= 0.98;
  else if (god == 3) build_mult *= 0.99;
  if (has(U_SANTAS_LEGACY)) mult *= 1 + (S.santa_level + 1) * 0.03;

  G.milk_progress = G.achievements_owned / 25.f;
  double milk_mult = 1;
  if (has(U_SANTAS_MILK_AND_COOKIES)) milk_mult *= 1.05;
  milk_mult *= 1 + aura_mult(AURA_BREATH_OF_MILK) * 0.05;
  god = hasgod(GOD_MOTHER);
  if (god == 1) milk_mult *= 1.1;
  else if (god == 2) milk_mult *= 1.05;
  else if (god == 3) milk_mult *= 1.03;
  milk_mult *= eff(EFF_MILK);
  static const struct { uint16_t u; float k; } kittens[] = {
    {U_KITTEN_HELPERS, 0.1}, {U_KITTEN_WORKERS, 0.125}, {U_KITTEN_ENGINEERS, 0.15}, {U_KITTEN_OVERSEERS, 0.175},
    {U_KITTEN_MANAGERS, 0.2}, {U_KITTEN_ACCOUNTANTS, 0.2}, {U_KITTEN_SPECIALISTS, 0.2}, {U_KITTEN_EXPERTS, 0.2},
    {U_KITTEN_CONSULTANTS, 0.2}, {U_KITTEN_ASSISTANTS_TO_THE_REGIONAL_MANAGER, 0.175},
    {U_KITTEN_MARKETEERS, 0.15}, {U_KITTEN_ANALYSTS, 0.125}, {U_KITTEN_EXECUTIVES, 0.115},
    {U_KITTEN_ADMINS, 0.11}, {U_KITTEN_STRATEGISTS, 0.105}, {U_KITTEN_ANGELS, 0.1}, {U_FORTUNE_103, 0.05}};
  double cat_mult = 1;
  for (unsigned i = 0; i < sizeof kittens / sizeof kittens[0]; i++)
    if (has(kittens[i].u)) cat_mult *= 1 + G.milk_progress * kittens[i].k * milk_mult;

  for (int b = 0; b < NB; b++) {
    Building *m = &S.buildings[b];
    m->stored_cps = building_cps(b);
    if (S.ascension_mode != 1) m->stored_cps *= (1 + m->level * 0.01) * build_mult;
    if (b == B_GRANDMA && has(U_MILKHELPANDREG_LACTOSE_INTOLERANCE_RELIEF_TABLETS))
      m->stored_cps *= 1 + 0.05 * G.milk_progress * milk_mult;
    m->stored_total_cps = m->amount * m->stored_cps;
    cps += m->stored_total_cps;
  }
  G.building_cps = cps;
  if (has(U_EGG)) cps += 9;
  mult *= cat_mult;

  static const uint16_t eggs[] = {U_CHICKEN_EGG, U_DUCK_EGG, U_TURKEY_EGG, U_QUAIL_EGG, U_ROBIN_EGG, U_OSTRICH_EGG,
                                  U_CASSOWARY_EGG, U_SALMON_ROE, U_FROGSPAWN, U_SHARK_EGG, U_TURTLE_EGG, U_ANT_LARVA};
  double egg_mult = 1;
  for (unsigned i = 0; i < sizeof eggs / sizeof eggs[0]; i++)
    if (has(eggs[i])) egg_mult *= 1.01;
  if (has(U_CENTURY_EGG)) {   /* +10% at most, over 100 days of this ascension */
    double day = floor((S.play_time - S.start_date) / 1000 / 10) * 10 / 60 / 60 / 24;
    if (day < 0) day = 0;
    if (day > 100) day = 100;
    egg_mult *= 1 + (1 - pow(1 - day / 100, 3)) * 0.1;
  }
  mult *= egg_mult;
  if (has(U_SUGAR_BAKING)) mult *= 1 + (S.lumps < 100 ? S.lumps : 100) * 0.01;
  mult *= 1 + aura_mult(AURA_RADIANT_APPETITE);

  double raw = cps * mult;
  for (int i = 0; i < N_CPS_ACHIEVS; i++)
    if (raw >= cps_achiev(i)->threshold) win(cps_achiev(i)->id);
  G.cookies_ps_raw = raw;
  if (raw > S.cookies_ps_raw_highest) S.cookies_ps_raw_highest = raw;

  double fortune = aura_mult(AURA_DRAGONS_FORTUNE);
  for (int i = 0; i < G.nshimmers_golden; i++) mult *= 1 + fortune * 1.23;

  int sucking = 0;
  for (int i = 0; i < MAX_WRINKLERS; i++)
    if (S.wrinklers[i].phase == 2) sucking++;
  double suck = 1.0 / 20;
  extern float wrinkler_eat_eff(void);
  suck *= wrinkler_eat_eff();
  suck *= 1 + aura_mult(AURA_DRAGON_GUTS) * 0.2;
  G.cps_sucked = sucking * suck < 1 ? sucking * suck : 1;

  if (has(U_ELDER_COVENANT)) mult *= 0.95;
  if (has(U_GOLDEN_SWITCH_OFF)) {
    double gs = 1.5;
    if (has(U_RESIDUAL_LUCK))
      for (int i = 0; i < N_GOLDEN_UPGRADES; i++)
        if (has(LIST(GOLDEN_UPGRADES)[i])) gs += 0.1;
    mult *= gs;
  }
  if (has(U_SHIMMERING_VEIL_OFF)) mult *= 1 + veil_boost();
  if (has(U_MAGIC_SHENANIGANS)) mult *= 1000;
  if (has(U_OCCULT_OBSTRUCTION)) mult *= 0;

  G.unbuffed_cps = cps * mult;
  for (int i = 0; i < MAX_BUFFS; i++)
    if (S.buffs[i].used && S.buffs[i].time > 0) mult *= S.buffs[i].mult_cps;
  G.global_cps_mult = mult;
  G.cookies_ps = cps * mult;
  G.computed_mouse_cps = mouse_cps();
  compute_lump_times();
  G.recalculate = false;
}

double drop_rate_mult(void) {
  double rate = 1;
  if (has(U_GREEN_YEAST_DIGESTIVES)) rate *= 1.03;
  if (has(U_DRAGON_TEDDY_BEAR)) rate *= 1.03;
  rate *= eff(EFF_ITEM_DROPS);
  rate *= 1 + aura_mult(AURA_MIND_OVER_MATTER) * 0.25;
  if (has(U_SANTAS_BOTTOMLESS_BAG)) rate *= 1.1;
  if (has(U_COSMIC_BEGINNERS_LUCK) && !has(U_HEAVENLY_CHIP_SECRET)) rate *= 5;
  return rate;
}

/* ------------------------------------------------------------------ buildings */
extern void building_bought(int b);   /* the buildings' buyFunctions: unlocks, achievements */
void building_buy(int b, int amount) {
  if (G.buy_mode == -1) {
    building_sell(b, G.buy_bulk);
    return;
  }
  Building *m = &S.buildings[b];
  if (amount == -1) amount = 1000;
  bool any = false;
  for (int i = 0; i < amount; i++) {
    double price = building_price(b);
    if (S.cookies < price) break;
    spend(price);
    m->amount++, m->bought++;
    building_bought(b);
    G.recalculate = true;
    if (m->amount > m->highest) m->highest = m->amount;
    G.buildings_owned++;
    any = true;
  }
  if (any) G.store_dirty = true;
}
extern void building_sold(int b, int n);   /* the Pantheon's Ruin, the dragon's orbs */
void building_sell(int b, int amount) {
  Building *m = &S.buildings[b];
  if (amount == -1) amount = m->amount;
  if (!amount) amount = 1;
  int sold = 0;
  for (int i = 0; i < amount; i++) {
    if (m->amount <= 0) break;
    double price = floor(building_price(b) * sell_multiplier());
    sold++;
    S.cookies += price;
    if (S.cookies > S.cookies_earned) S.cookies_earned = S.cookies;
    m->amount--;
    G.recalculate = true;
    G.buildings_owned--;
  }
  if (sold) building_sold(b, sold), G.store_dirty = true;
}

/* ------------------------------------------------------------------ the big cookie */
void click_cookie(void) {
  if (G.on_ascend || G.ascend_timer > 0 || G.T < 3) return;
  extern void lose_shimmering_veil(int why);
  lose_shimmering_veil(0);
  double amount = G.computed_mouse_cps;
  earn(amount);
  S.handmade_cookies += amount;
  if (S.prefs.particles) {
    particle_add(0, 0, 0, 0, 1, 2, 0);
    particle_add(COOKIE_X + (float)(rnd() * 40 - 20), COOKIE_Y + (float)(rnd() * 40 - 20), (float)(rnd() * 4 - 2),
                 (float)(rnd() * -2 - 2), (float)(rnd() * 0.5 + 0.75), 1, 2);
  }
  if (S.prefs.numbers) {
    char t[24] = "+";
    beautify(amount, 1, t + 1);
    text_particle_add(COOKIE_X + (float)(rnd() * 24 - 12), COOKIE_Y - 10 + (float)(rnd() * 8 - 4), t);
  }
  S.cookie_clicks++;
  G.click_t = 0;
}

/* ------------------------------------------------------------------ the store's upgrades */
static uint16_t sort_key_u[N_UPGRADES];
static double sort_key_p[N_UPGRADES];
void game_rebuild_upgrades(void) {
  G.upgrades_dirty = false;
  int n = 0;
  for (int u = 0; u < N_UPGRADES; u++) {
    const UpgradeRec *r = upgrade_rec(u);
    if (r->pool == POOL_DEBUG || r->pool == POOL_PRESTIGE) continue;
    bool bought = bit_get(S.upgrades_bought, u);
    extern bool upgrade_display_when_owned(int u);
    if (!bought) {
      if (S.ascension_mode == 1) {
        bool lasting = false;
        for (int i = 0; i < N_LASTING; i++)
          if (LIST(LASTING)[i] == u) lasting = true;
        if (lasting || r->tier == TIER_FORTUNE) continue;
      }
      if (!has_unlocked(u)) continue;
    } else if (!upgrade_display_when_owned(u))
      continue;
    sort_key_u[n] = (uint16_t)u;
    sort_key_p[n] = r->pool == POOL_TOGGLE ? r->order : upgrade_price(u);
    n++;
  }
  /* (insertion sort: stable, as the game's sort is in practice) */
  for (int i = 1; i < n; i++) {
    uint16_t u = sort_key_u[i];
    double p = sort_key_p[i];
    int j = i;
    while (j > 0 && sort_key_p[j - 1] > p) sort_key_u[j] = sort_key_u[j - 1], sort_key_p[j] = sort_key_p[j - 1], j--;
    sort_key_u[j] = u, sort_key_p[j] = p;
  }
  /* the store: upgrades, then switches, then research */
  G.nstore = G.nstore_toggle = G.nstore_tech = 0;
  for (int pass = 0; pass < 3; pass++)
    for (int i = 0; i < n; i++) {
      int pool = upgrade_rec(sort_key_u[i])->pool;
      int want = pool == POOL_TOGGLE ? 1 : pool == POOL_TECH ? 2 : 0;
      if (want != pass) continue;
      G.store[G.nstore + G.nstore_toggle + G.nstore_tech] = sort_key_u[i];
      if (pass == 0) G.nstore++;
      else if (pass == 1) G.nstore_toggle++;
      else G.nstore_tech++;
    }
  G.store_dirty = true;
}

/* ------------------------------------------------------------------ notes, popups, particles */
void notify(const char *title, const char *desc, int icon, int seconds) {
  for (int i = MAX_NOTES - 1; i > 0; i--) G.notes[i] = G.notes[i - 1];
  Note *n = &G.notes[0];
  snprintf(n->title, sizeof n->title, "%s", title);
  snprintf(n->desc, sizeof n->desc, "%s", desc ? desc : "");
  n->icon = (int16_t)(icon == NONE ? -1 : icon);
  if (S.prefs.notes_on) seconds = seconds > 0 && seconds < 6 ? seconds : 6;   /* (Fast notes) */
  n->life = seconds > 0 ? seconds * FPS : -1;
  n->age = 0;
}
void popup(const char *text, float x, float y) {
  int k = 0;
  for (int i = 0; i < MAX_POPUPS; i++)
    if (G.popups[i].life <= 0 || G.popups[i].life < G.popups[k].life) k = i;
  Popup *p = &G.popups[k];
  snprintf(p->text, sizeof p->text, "%s", text);
  p->x = x, p->y = y, p->life = FPS * 4;
}
void particle_add(float x, float y, float xd, float yd, float size, int dur, int kind) {
  int k = 0, oldest = -1;
  for (int i = 0; i < MAX_PARTICLES; i++) {
    if (G.particles[i].life < 0) {
      k = i;
      oldest = -2;
      break;
    }
    if (G.particles[i].life > oldest) oldest = G.particles[i].life, k = i;
  }
  Particle *p = &G.particles[k];
  bool auto_ = x != 0;
  p->life = 0;
  p->x = auto_ ? x : (float)(rnd() * LEFT_W);
  p->y = auto_ ? y : NEWS_H - 24;
  p->xd = auto_ ? xd : (float)(rnd() * 2 - 1) * 0.37f;
  p->yd = auto_ ? yd : (float)(4 + rnd() * 2) * 0.37f;
  if (auto_) p->xd *= 0.37f, p->yd *= 0.37f;
  p->size = size;
  p->dur = (int16_t)dur;
  p->kind = (int16_t)kind;
  p->r = (float)(rnd() * 360);
  /* a cookie the bakery has (its icon), or the plain one */
  extern int particle_icon(void);
  p->pic = (int16_t)particle_icon();
}
void text_particle_add(float x, float y, const char *text) {
  int k = 0;
  for (int i = 0; i < MAX_TEXT_PARTICLES; i++) {
    if (G.texts[i].life < 0) {
      k = i;
      break;
    }
    if (G.texts[i].life > G.texts[k].life) k = i;
  }
  TextParticle *t = &G.texts[k];
  t->x = x, t->y = y, t->yd = -0.75f, t->life = 0, t->dur = FPS;
  snprintf(t->text, sizeof t->text, "%s", text);
}
