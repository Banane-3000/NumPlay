/* Ascension: prestige levels and heavenly chips (the game's HowMuchPrestige: a level for every
 * trillion cookies, cubed), the ascension and its crumbling cookie, the heavenly upgrades' tree,
 * permanent upgrade slots, the challenge mode Born again, and reincarnating (Game.Reset); the
 * shimmering veil. */
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "game.h"

#define HC_FACTOR 3
#define ASCEND_DURATION (FPS * 5)
#define ASCEND_BREAKPOINT (ASCEND_DURATION / 2)
#define REINCARNATE_DURATION (FPS * 1)

double how_much_prestige(double cookies) { return pow(cookies / 1e12, 1.0 / HC_FACTOR); }
double how_many_cookies_reset(double chips) { return pow(chips, HC_FACTOR) * 1e12; }

double heavenly_multiplier(void) {
  double m = 0;
  if (has(U_HEAVENLY_CHIP_SECRET)) m += 0.05;
  if (has(U_HEAVENLY_COOKIE_STAND)) m += 0.20;
  if (has(U_HEAVENLY_BAKERY)) m += 0.25;
  if (has(U_HEAVENLY_CONFECTIONERY)) m += 0.25;
  if (has(U_HEAVENLY_KEY)) m += 0.25;
  m *= 1 + aura_mult(AURA_DRAGON_GOD) * 0.05;
  if (has(U_LUCKY_DIGIT)) m *= 1.01;
  if (has(U_LUCKY_NUMBER)) m *= 1.01;
  if (has(U_LUCKY_PAYOUT)) m *= 1.01;
  static const float creation[4] = {1, 0.7f, 0.8f, 0.9f};
  m *= creation[hasgod(GOD_CREATION)];
  return m;
}

/* ------------------------------------------------------------------ the shimmering veil */
static double veil_defense(void) {
  double n = 0;
  if (has(U_REINFORCED_MEMBRANE)) n += 0.1;
  if (has(U_DELICATE_TOUCH)) n += 0.1;
  if (has(U_STEADFAST_MURMUR)) n += 0.1;
  if (has(U_GLITTERING_EDGE)) n += 0.1;
  return n;
}
double veil_boost(void) {
  double n = 0.5;
  if (has(U_REINFORCED_MEMBRANE)) n += 0.1;
  if (has(U_DELICATE_TOUCH)) n += 0.05;
  if (has(U_STEADFAST_MURMUR)) n += 0.05;
  if (has(U_GLITTERING_EDGE)) n += 0.05;
  return n;
}
void lose_shimmering_veil(int why) {
  (void)why;
  if (!has(U_SHIMMERING_VEIL)) return;
  if (!has(U_SHIMMERING_VEIL_OFF) && has(U_SHIMMERING_VEIL_ON)) return;
  if (has(U_REINFORCED_MEMBRANE) && rnd() < veil_defense()) {
    notify("The reinforced membrane protects the shimmering veil.", "", ICON(7, 10), 0);
    win(A_THICK_SKINNED);
    return;
  }
  bit_set(S.upgrades_bought, U_SHIMMERING_VEIL_ON, true);
  lock(U_SHIMMERING_VEIL_OFF), unlock(U_SHIMMERING_VEIL_OFF);
  notify("The shimmering veil disappears...", "", ICON(9, 10), 0);
  G.upgrades_dirty = true, G.recalculate = true;
}

/* ------------------------------------------------------------------ the heavenly upgrades */
/* how many 7s the prestige level is written with (Lucky digit, number, payout) */
static int sevens(void) {
  double p = S.prestige;
  int n = 0;
  char t[40];
  snprintf(t, sizeof t, "%.0f", p);
  for (char *c = t; *c; c++) n += *c == '7';
  return n;
}
bool heavenly_show_if(int u) {
  if (u == U_LUCKY_DIGIT) return sevens() >= 1;
  if (u == U_LUCKY_NUMBER) return sevens() >= 2;
  if (u == U_LUCKY_PAYOUT) return sevens() >= 4;
  return true;
}
/* a heavenly upgrade can be bought once all its parents are */
bool heavenly_can_be_purchased(int u) {
  if (bit_get(S.upgrades_bought, u)) return true;
  if (!heavenly_show_if(u)) return false;
  const UpgradeRec *r = upgrade_rec(u);
  for (int i = 0; i < 6 && r->parents[i] != NONE; i++)
    if (!bit_get(S.upgrades_bought, r->parents[i])) return false;
  return true;
}
/* shown dimmed: one of its parents can be bought */
bool heavenly_ghosted(int u) {
  if (heavenly_can_be_purchased(u) || !heavenly_show_if(u)) return false;
  const UpgradeRec *r = upgrade_rec(u);
  for (int i = 0; i < 6 && r->parents[i] != NONE; i++)
    if (heavenly_can_be_purchased(r->parents[i])) return true;
  return false;
}
int permanent_slot_of(int u) {   /* which permanent upgrade slot (0..4) a heavenly upgrade is, or -1 */
  static const uint16_t slots[5] = {U_PERMANENT_UPGRADE_SLOT_I, U_PERMANENT_UPGRADE_SLOT_II,
                                    U_PERMANENT_UPGRADE_SLOT_III, U_PERMANENT_UPGRADE_SLOT_IV,
                                    U_PERMANENT_UPGRADE_SLOT_V};
  for (int i = 0; i < 5; i++)
    if (slots[i] == u) return i;
  return -1;
}
/* the upgrades a permanent slot can hold: bought last run, of the regular kind, not in another slot */
bool can_be_permanent(int u) {
  const UpgradeRec *r = upgrade_rec(u);
  if (!bit_get(S.upgrades_bought, u) || !has_unlocked(u)) return false;
  if (r->pool != POOL_NORMAL && r->pool != POOL_COOKIE) return false;
  for (int i = 0; i < N_NO_PERM; i++)
    if (LIST(NO_PERM)[i] == u) return false;
  for (int i = 0; i < 5; i++)
    if (S.permanent[i] == u + 1) return false;
  return true;
}

/* ------------------------------------------------------------------ ascending */
void earn_heavenly_chips(double forfeited, bool silent) {
  double p = floor(how_much_prestige(S.cookies_reset + forfeited));
  if (p < 0) p = 0;
  if (p != S.prestige) {
    double diff = p - S.prestige;
    extern double gained_prestige;
    gained_prestige = diff;
    S.heavenly_chips += diff;
    S.prestige = p;
    if (!silent && diff > 0) {
      char a[48], b[48], t[96], d[96];
      cookies_str(forfeited, 0, a);
      beautify(diff, 0, b);
      snprintf(t, sizeof t, "You forfeit your %s.", a);
      snprintf(d, sizeof d, "You gain \x01%s prestige level%s\x02!", b, diff == 1 ? "" : "s");
      notify(t, d, ICON(19, 7), 0);
    }
  }
}
double gained_prestige;

extern void reset_minigames(bool hard);
extern void lumps_reset(bool hard);
static bool in_list(const uint16_t *l, int n, int u) {
  for (int i = 0; i < n; i++)
    if (l[i] == u) return true;
  return false;
}

/* the game's Reset: a new run (hard: everything goes) */
void game_reset(bool hard) {
  G.T = 0, G.real_t = 0;
  double forfeited = S.cookies_earned;
  if (!hard) {
    static const struct { double at; uint16_t a; } wins[] = {
      {1e6, A_SACRIFICE}, {1e9, A_OBLIVION}, {1e12, A_FROM_SCRATCH}, {1e15, A_NIHILISM}, {1e18, A_DEMATERIALIZE},
      {1e21, A_NIL_ZERO_ZILCH}, {1e24, A_TRANSCENDENCE}, {1e27, A_OBLITERATE}, {1e30, A_NEGATIVE_VOID},
      {1e33, A_TO_CRUMBS_YOU_SAY}, {1e36, A_YOU_GET_NOTHING}, {1e39, A_HUMBLE_REBEGINNINGS},
      {1e42, A_THE_END_OF_THE_WORLD}, {1e45, A_OH_YOURE_BACK}, {1e48, A_LAZARUS}, {1e51, A_SMURF_ACCOUNT},
      {1e54, A_IF_AT_FIRST_YOU_DONT_SUCCEED}, {1e57, A_NO_MORE_ROOM_IN_HELL}};
    for (unsigned i = 0; i < sizeof wins / sizeof wins[0]; i++)
      if (forfeited >= wins[i].at) win(wins[i].a);
    if (round(S.cookies) == 1e12) win(A_WHEN_THE_COOKIES_ASCEND_JUST_RIGHT);
    if (has_buff(BUFF_LOAN1) || has_buff(BUFF_LOAN2) || has_buff(BUFF_LOAN3)) win(A_DEBT_EVASION);
  }
  for (int i = 0; i < MAX_BUFFS; i++) S.buffs[i].used = 0;
  S.cookies_reset += S.cookies_earned;
  S.cookies = 0, S.cookies_earned = 0, S.cookie_clicks = 0, S.golden_clicks_local = 0, S.handmade_cookies = 0;
  S.cookies_ps_raw_highest = 0;
  if (hard) {
    S.bg_type = 0, S.milk_type = 0;
    memset(S.upgrades_vaulted, 0, sizeof S.upgrades_vaulted);
    S.golden_clicks = 0, S.missed_golden_clicks = 0, S.resets = 0;
    S.full_date = S.play_time;
    S.cookies_reset = 0, S.prestige = 0, gained_prestige = 0;
    S.heavenly_chips = 0, S.heavenly_chips_spent = 0;
    memset(S.permanent, 0, sizeof S.permanent);
    S.ascension_mode = 0;
    lumps_reset(true);
  }
  S.pledges = 0, S.pledge_t = 0, S.elder_wrath = 0;
  extern float elder_wrath_d;
  elder_wrath_d = 0;
  S.next_research = 0, S.research_t = 0;
  S.season_t = 0, S.season_uses = 0, S.season = 0;
  S.start_date = S.play_time;
  S.cookies_sucked = 0, S.wrinklers_popped = 0;
  memset(S.wrinklers, 0, sizeof S.wrinklers);
  S.santa_level = 0, S.reindeer_clicked = 0, S.fortune_gc = 0, S.fortune_cps = 0;
  S.dragon_level = 0, S.dragon_aura = 0, S.dragon_aura2 = 0;
  S.ticker_clicks = 0;
  if (!hard && gained_prestige > 0) S.resets++;
  gained_prestige = 0;
  for (int b = 0; b < NB; b++) {
    Building *m = &S.buildings[b];
    m->amount = m->bought = m->highest = m->free = 0;
    m->total_cookies = 0;
    if (hard) m->muted = 0, m->level = 0;
  }
  for (int u = 0; u < N_UPGRADES; u++) {
    const UpgradeRec *r = upgrade_rec(u);
    bool lasting = in_list(LIST(LASTING), N_LASTING, u);
    if (hard || r->pool != POOL_PRESTIGE) bit_set(S.upgrades_bought, u, false);
    if (hard) bit_set(S.upgrades_unlocked, u, false);
    if (r->pool != POOL_PRESTIGE && !lasting) {
      bool keep = false;
      if (has(U_KEEPSAKES) && (in_list(LIST(HEART_DROPS), N_HEART_DROPS, u) ||
                                in_list(LIST(HALLOWEEN_DROPS), N_HALLOWEEN_DROPS, u) ||
                                in_list(LIST(EASTER_EGGS), N_EASTER_EGGS, u) ||
                                in_list(LIST(SANTA_DROPS), N_SANTA_DROPS, u) ||
                                in_list(LIST(REINDEER_DROPS), N_REINDEER_DROPS, u)) && rnd() < 1.0 / 5)
        keep = true;
      else if (S.ascension_mode == 1 && has_achiev(A_O_FORTUNA) && r->tier == TIER_FORTUNE)
        keep = true;
      else if (has_achiev(A_O_FORTUNA) && r->tier == TIER_FORTUNE && rnd() < 0.4)
        keep = true;
      if (!keep) bit_set(S.upgrades_unlocked, u, false);
    }
  }
  extern void game_recount(void);
  game_recount();
  if (!hard && S.ascension_mode != 1) {
    for (int i = 0; i < 5; i++)
      if (S.permanent[i]) earn_upgrade(S.permanent[i] - 1);
    if (has(U_SEASON_SWITCHER))
      unlock(U_FESTIVE_BISCUIT), unlock(U_GHOSTLY_BISCUIT), unlock(U_LOVESICK_BISCUIT), unlock(U_FOOLS_BISCUIT),
          unlock(U_BUNNY_BISCUIT);
    if (has(U_STARTER_KIT)) S.buildings[B_CURSOR].amount += 10, S.buildings[B_CURSOR].bought += 10,
                            S.buildings[B_CURSOR].free += 10, S.buildings[B_CURSOR].highest = 10;
    if (has(U_STARTER_KITCHEN)) S.buildings[B_GRANDMA].amount += 5, S.buildings[B_GRANDMA].bought += 5,
                                S.buildings[B_GRANDMA].free += 5, S.buildings[B_GRANDMA].highest = 5;
  }
  game_recount();
  G.recalculate = true, G.store_dirty = true, G.upgrades_dirty = true;
  shimmers_reset();
  G.buy_bulk = 1, G.buy_mode = 1;
  reset_minigames(hard);
  game_rebuild_upgrades();
}

void ascend_start(void) {
  notify("Ascending", "So long, cookies.", ICON(20, 7), 4);
  G.on_ascend = false;
  G.ascend_timer = 1;
  for (int i = 0; i < MAX_SHIMMERS; i++) G.shimmers[i].used = 0;
  shimmers_reset();
}

/* the animation: the cookie shakes, bursts, and the sky of heavenly upgrades opens */
void ascend_logic(void) {
  if (G.ascend_timer > 0) {
    G.ascend_timer++;
    if (G.ascend_timer > ASCEND_DURATION) {
      earn_heavenly_chips(S.cookies_earned, false);
      G.ascend_timer = 0;
      G.on_ascend = true;
      S.ascension_mode = 0;
    }
  }
}

/* back to the mortal world, in the challenge mode chosen (0 none, 1 Born again) */
void reincarnate(int mode) {
  S.ascension_mode = (uint8_t)mode;
  game_reset(false);
  if (has_achiev(A_REBIRTH)) notify("Reincarnated", "Hello, cookies!", ICON(10, 0), 4);
  if (S.resets >= 1000) win(A_ENDLESS_CYCLE);
  if (S.resets >= 100) win(A_REINCARNATION);
  if (S.resets >= 10) win(A_RESURRECTION);
  if (S.resets >= 1) win(A_REBIRTH);
  int owned = 0;
  for (int u = 0; u < N_UPGRADES; u++)
    if (bit_get(S.upgrades_bought, u) && upgrade_rec(u)->pool == POOL_PRESTIGE) owned++;
  if (owned >= 100) win(A_ALL_THE_STARS_IN_HEAVEN);
  G.on_ascend = false;
  G.reincarnate_timer = 1;
  G.big_size = 0;
}
void reincarnate_logic(void) {
  if (G.reincarnate_timer > 0 && ++G.reincarnate_timer > REINCARNATE_DURATION) G.reincarnate_timer = 0;
}
