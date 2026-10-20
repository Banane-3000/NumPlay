/* The game's frame (Game.Logic) and its checks every 5 seconds, the buildings' buyFunctions, a new
 * game. */
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "game.h"

#define SPECIAL_GRANDMA_UNLOCK 15

static void unlock_tiered(int b) {
  const BuildingLinks *l = building_links(b);
  int amount = S.buildings[b].amount;
  for (int t = 0; t < N_TIERS; t++) {
    const TierRec *tr = tier_rec(t);
    if (l->tiered[t] != NONE && tr->unlock != -1 && amount >= tr->unlock) unlock(l->tiered[t]);
    if (l->tiered_ach[t] != NONE && amount >= tr->achiev_unlock) win(l->tiered_ach[t]);
  }
  for (int i = 0; i < 6 && l->synergies[i] != NONE; i++) {
    const UpgradeRec *s = upgrade_rec(l->synergies[i]);
    const TierRec *tr = tier_rec(s->tier);
    if (tr->req != NONE && has(tr->req) && S.buildings[s->building].amount >= tr->unlock &&
        S.buildings[s->building2].amount >= tr->unlock)
      unlock(l->synergies[i]);
  }
}

void building_bought(int b) {
  unlock_tiered(b);
  int n = S.buildings[b].amount;
  if (b == B_CURSOR) {
    static const struct { int16_t at; uint16_t u; } ups[] = {
      {1, U_REINFORCED_INDEX_FINGER}, {1, U_CARPAL_TUNNEL_PREVENTION_CREAM}, {10, U_AMBIDEXTROUS},
      {25, U_THOUSAND_FINGERS}, {50, U_MILLION_FINGERS}, {100, U_BILLION_FINGERS}, {150, U_TRILLION_FINGERS},
      {200, U_QUADRILLION_FINGERS}, {250, U_QUINTILLION_FINGERS}, {300, U_SEXTILLION_FINGERS},
      {350, U_SEPTILLION_FINGERS}, {400, U_OCTILLION_FINGERS}, {450, U_NONILLION_FINGERS},
      {500, U_DECILLION_FINGERS}, {550, U_UNDECILLION_FINGERS}};
    for (unsigned i = 0; i < sizeof ups / sizeof ups[0]; i++)
      if (n >= ups[i].at) unlock(ups[i].u);
    static const struct { int16_t at; uint16_t a; } wins[] = {
      {1, A_CLICK}, {2, A_DOUBLE_CLICK}, {50, A_MOUSE_WHEEL}, {100, A_OF_MICE_AND_MEN}, {200, A_THE_DIGITAL},
      {300, A_EXTREME_POLYDACTYLY}, {400, A_DR_T}, {500, A_THUMBS_PHALANGES_METACARPALS},
      {600, A_WITH_HER_FINGER_AND_HER_THUMB}, {700, A_GOTTA_HAND_IT_TO_YOU}, {800, A_THE_DEVILS_WORKSHOP},
      {900, A_ALL_ON_DECK}, {1000, A_A_ROUND_OF_APPLAUSE}};
    for (unsigned i = 0; i < sizeof wins / sizeof wins[0]; i++)
      if (n >= wins[i].at) win(wins[i].a);
  } else if (b != B_GRANDMA) {
    uint16_t g = building_links(b)->grandma;
    if (n >= SPECIAL_GRANDMA_UNLOCK && S.buildings[B_GRANDMA].amount > 0 && g != NONE) unlock(g);
  }
}

/* ------------------------------------------------------------------ a new game */
extern void shimmers_reset(void);
extern void lumps_new(void);
void game_new(void) {
  uint32_t seed = G.seed;
  memset(&G, 0, sizeof G);
  G.seed = seed ? seed : 0x2545F491u;
  S.magic = 0x314B4343u, S.version = 1;
  S.prefs = (Prefs){.particles = 1, .numbers = 1, .milk = 1, .fancy = 1, .cursors = 1, .wobbly = 1, .notes_on = 0};
  S.research_t = -1, S.season_t = -1;
  S.swaps = 3;
  S.bakery_name1 = (uint8_t)rnd_int(256), S.bakery_name2 = (uint8_t)rnd_int(256);
  for (int i = 0; i < MAX_BUFFS; i++) S.buffs[i].used = 0;
  for (int i = 0; i < 16; i++) G.effs[i] = 1;
  shimmers_reset();
  lumps_new();
  G.buy_mode = 1, G.buy_bulk = 1;
  G.recalculate = true, G.upgrades_dirty = true, G.store_dirty = true;
  G.big_size = 1, G.big_size_t = 1;
  for (int i = 0; i < MAX_PARTICLES; i++) G.particles[i].life = -1;
  for (int i = 0; i < MAX_TEXT_PARTICLES; i++) G.texts[i].life = -1;
}

/* the counts kept from the saved bits */
void game_recount(void) {
  G.achievements_owned = G.upgrades_owned = G.buildings_owned = 0;
  for (int a = 0; a < N_ACHIEVS; a++)
    if (has_achiev(a) && achiev_rec(a)->pool == APOOL_NORMAL) G.achievements_owned++;
  for (int u = 0; u < N_UPGRADES; u++)
    if (bit_get(S.upgrades_bought, u) && counts_as_upgrade_owned(u)) G.upgrades_owned++;
  for (int b = 0; b < NB; b++) G.buildings_owned += S.buildings[b].amount;
  G.milk_progress = G.achievements_owned / 25.f;
}

/* ------------------------------------------------------------------ the checks every 5 seconds */
void game_check(void) {
  if (isnan(S.cookies)) S.cookies = 0, S.cookies_earned = 0, G.recalculate = true;
  double time_played = S.play_time - S.start_date;
  if (S.play_time - S.full_date >= 365.0 * 24 * 60 * 60 * 1000) win(A_SO_MUCH_TO_DO_SO_MUCH_TO_SEE);
  if (S.cookies_earned >= 1000000 && (S.ascension_mode == 1 || S.resets == 0)) {
    if (time_played <= 1000 * 60 * 35) win(A_SPEED_BAKING_I);
    if (time_played <= 1000 * 60 * 25) win(A_SPEED_BAKING_II);
    if (time_played <= 1000 * 60 * 15) win(A_SPEED_BAKING_III);
    if (S.cookie_clicks <= 15) win(A_NEVERCLICK);
    if (S.cookie_clicks <= 0) win(A_TRUE_NEVERCLICK);
    if (S.cookies_earned >= 1000000000 && G.upgrades_owned == 0) win(A_HARDCORE);
  }
  for (int i = 0; i < N_UNLOCK_AT; i++) {
    const UnlockAtRec *u = unlock_at(i);
    if (S.cookies_earned < u->cookies) continue;
    if (u->require_up != NONE && !has(u->require_up)) continue;
    if (u->require_ach != NONE && !has_achiev(u->require_ach)) continue;
    if (u->season && S.season != u->season) continue;
    if (u->up != NONE) unlock(u->up);
    if (u->ach != NONE) win(u->ach);
  }
  if (has(U_GOLDEN_SWITCH)) unlock(U_GOLDEN_SWITCH_OFF);
  if (has(U_SHIMMERING_VEIL) && !has(U_SHIMMERING_VEIL_OFF) && !has(U_SHIMMERING_VEIL_ON))
    unlock(U_SHIMMERING_VEIL_ON), earn_upgrade(U_SHIMMERING_VEIL_OFF);
  if (has(U_SUGAR_CRAVING)) unlock(U_SUGAR_FRENZY);
  if (has(U_CLASSIC_DAIRY_SELECTION)) unlock(U_MILK_SELECTOR);
  if (has(U_BASIC_WALLPAPER_ASSORTMENT)) unlock(U_BACKGROUND_SELECTOR);
  if (has(U_PRISM_HEART_BISCUITS)) win(A_LOVELY_COOKIES);
  if (S.season == SEASON_EASTER) {
    int eggs = 0;
    for (int i = 0; i < N_EASTER_EGGS; i++) eggs += has_unlocked(LIST(EASTER_EGGS)[i]);
    if (eggs >= 1) win(A_THE_HUNT_IS_ON);
    if (eggs >= 7) win(A_EGGING_ON);
    if (eggs >= 14) win(A_MASS_EASTERIA);
    if (eggs >= N_EASTER_EGGS) win(A_HIDE_AND_SEEK_CHAMPION);
  }
  if (has(U_FORTUNE_COOKIES)) {
    int f = 0;
    for (int i = 0; i < N_FORTUNES; i++) f += has(LIST(FORTUNES)[i]);
    if (f >= N_FORTUNES) win(A_O_FORTUNA);
  }
  if (has(U_LEGACY) && S.ascension_mode != 1) {
    unlock(U_HEAVENLY_CHIP_SECRET);
    if (has(U_HEAVENLY_CHIP_SECRET)) unlock(U_HEAVENLY_COOKIE_STAND);
    if (has(U_HEAVENLY_COOKIE_STAND)) unlock(U_HEAVENLY_BAKERY);
    if (has(U_HEAVENLY_BAKERY)) unlock(U_HEAVENLY_CONFECTIONERY);
    if (has(U_HEAVENLY_CONFECTIONERY)) unlock(U_HEAVENLY_KEY);
    if (has(U_HEAVENLY_KEY)) win(A_WHOLESOME);
  }
  for (int i = 0; i < N_BANK_ACHIEVS; i++)
    if (S.cookies_earned >= bank_achiev(i)->threshold) win(bank_achiev(i)->id);
  if (S.elder_wrath >= 3) win(A_GRANDMAPOCALYPSE);

  int owned = 0, min_amount = 100000;
  bool mathematician = true, base10 = true;
  for (int b = 0; b < NB; b++) {
    int a = S.buildings[b].amount;
    owned += a;
    if (a < min_amount) min_amount = a;
    int need = 1 << (NB - b - 1);
    if (a < (need < 128 ? need : 128)) mathematician = false;
    if (a < (NB - b) * 10) base10 = false;
  }
  if (min_amount >= 1) win(A_ONE_WITH_EVERYTHING);
  if (mathematician) win(A_MATHEMATICIAN);
  if (base10) win(A_BASE_10);
  static const struct { int16_t at; uint16_t a, u; } centennials[] = {
    {100, A_CENTENNIAL, U_MILK_CHOCOLATE_BUTTER_BISCUIT},
    {150, A_CENTENNIAL_AND_A_HALF, U_DARK_CHOCOLATE_BUTTER_BISCUIT},
    {200, A_BICENTENNIAL, U_WHITE_CHOCOLATE_BUTTER_BISCUIT},
    {250, A_BICENTENNIAL_AND_A_HALF, U_RUBY_CHOCOLATE_BUTTER_BISCUIT},
    {300, A_TRICENTENNIAL, U_LAVENDER_CHOCOLATE_BUTTER_BISCUIT},
    {350, A_TRICENTENNIAL_AND_A_HALF, U_SYNTHETIC_CHOCOLATE_GREEN_HONEY_BUTTER_BISCUIT},
    {400, A_QUADRICENTENNIAL, U_ROYAL_RASPBERRY_CHOCOLATE_BUTTER_BISCUIT},
    {450, A_QUADRICENTENNIAL_AND_A_HALF, U_ULTRA_CONCENTRATED_HIGH_ENERGY_CHOCOLATE_BUTTER_BISCUIT},
    {500, A_QUINCENTENNIAL, U_PURE_PITCH_BLACK_CHOCOLATE_BUTTER_BISCUIT},
    {550, A_QUINCENTENNIAL_AND_A_HALF, U_COSMIC_CHOCOLATE_BUTTER_BISCUIT},
    {600, A_SEXCENTENNIAL, U_BUTTER_BISCUIT_WITH_BUTTER}, {650, A_SEXCENTENNIAL_AND_A_HALF, U_EVERYBUTTER_BISCUIT},
    {700, A_SEPTCENTENNIAL, U_PERSONAL_BISCUIT}};
  for (unsigned i = 0; i < sizeof centennials / sizeof centennials[0]; i++)
    if (min_amount >= centennials[i].at) win(centennials[i].a), unlock(centennials[i].u);
  static const struct { double at; uint16_t a, u; } clicks[] = {
    {1e3, A_CLICKTASTIC, U_PLASTIC_MOUSE}, {1e5, A_CLICKATHLON, U_IRON_MOUSE}, {1e7, A_CLICKOLYMPICS, U_TITANIUM_MOUSE},
    {1e9, A_CLICKORAMA, U_ADAMANTIUM_MOUSE}, {1e11, A_CLICKASMIC, U_UNOBTAINIUM_MOUSE},
    {1e13, A_CLICKAGEDDON, U_ELUDIUM_MOUSE}, {1e15, A_CLICKNAROK, U_WISHALLOY_MOUSE},
    {1e17, A_CLICKASTROPHE, U_FANTASTEEL_MOUSE}, {1e19, A_CLICKATACLYSM, U_NEVERCRACK_MOUSE},
    {1e21, A_THE_ULTIMATE_CLICKDOWN, U_ARMYTHRIL_MOUSE},
    {1e23, A_ALL_THE_OTHER_KIDS_WITH_THE_PUMPED_UP_CLICKS, U_TECHNOBSIDIAN_MOUSE},
    {1e25, A_ONE_MORE_CLICK, U_PLASMARBLE_MOUSE}, {1e27, A_CLICKETY_SPLIT, U_MIRACULITE_MOUSE},
    {1e29, A_AINT_THAT_A_CLICK_IN_THE_HEAD, U_AETHERICE_MOUSE}, {1e31, A_WHATS_NOT_CLICKING, U_OMNIPLAST_MOUSE}};
  for (unsigned i = 0; i < sizeof clicks / sizeof clicks[0]; i++)
    if (S.handmade_cookies >= clicks[i].at) win(clicks[i].a), unlock(clicks[i].u);
  if (S.cookies_earned < S.cookies) win(A_CHEATED_COOKIES_TASTE_AWFUL);
  if (has(U_SKULL_COOKIES) && has(U_GHOST_COOKIES) && has(U_BAT_COOKIES) && has(U_SLIME_COOKIES) &&
      has(U_PUMPKIN_COOKIES) && has(U_EYEBALL_COOKIES) && has(U_SPIDER_COOKIES))
    win(A_SPOOKY_COOKIES);
  if (S.wrinklers_popped >= 1) win(A_ITCHSCRATCHER);
  if (S.wrinklers_popped >= 50) win(A_WRINKLESQUISHER);
  if (S.wrinklers_popped >= 200) win(A_MOISTBURSTER);
  if (S.cookies_earned >= 1000000 && has(U_HOW_TO_BAKE_YOUR_DRAGON)) unlock(U_A_CRUMBLY_EGG);
  if (S.cookies_earned >= 25 && S.season == SEASON_CHRISTMAS) unlock(U_A_FESTIVE_HAT);
  if (has(U_CHRISTMAS_TREE_BISCUITS) && has(U_SNOWFLAKE_BISCUITS) && has(U_SNOWMAN_BISCUITS) &&
      has(U_HOLLY_BISCUITS) && has(U_CANDY_CANE_BISCUITS) && has(U_BELL_BISCUITS) && has(U_PRESENT_BISCUITS))
    win(A_LET_IT_SNOW);
  if (S.reindeer_clicked >= 1) win(A_OH_DEER);
  if (S.reindeer_clicked >= 50) win(A_SLEIGH_OF_HAND);
  if (S.reindeer_clicked >= 200) win(A_REINDEER_SLEIGHER);
  static const struct { int16_t at; uint16_t a; } builds[] = {
    {100, A_BUILDER}, {500, A_ARCHITECT}, {1000, A_ENGINEER}, {2500, A_LORD_OF_CONSTRUCTS}, {5000, A_GRAND_DESIGN},
    {7500, A_ECUMENOPOLIS}, {10000, A_MYRIAD}};
  for (unsigned i = 0; i < sizeof builds / sizeof builds[0]; i++)
    if (owned >= builds[i].at) win(builds[i].a);
  static const struct { int16_t at; uint16_t a; } ups[] = {
    {20, A_ENHANCER}, {50, A_AUGMENTER}, {100, A_UPGRADER}, {200, A_LORD_OF_PROGRESS}, {300, A_THE_FULL_PICTURE},
    {400, A_WHEN_THERES_NOTHING_LEFT_TO_ADD}, {500, A_KAIZEN}, {600, A_BEYOND_QUALITY},
    {700, A_OFT_WE_MAR_WHATS_WELL}};
  for (unsigned i = 0; i < sizeof ups / sizeof ups[0]; i++)
    if (G.upgrades_owned >= ups[i].at) win(ups[i].a);
  if (owned >= 4000 && G.upgrades_owned >= 300) win(A_POLYMATH);
  if (owned >= 8000 && G.upgrades_owned >= 400) win(A_RENAISSANCE_BAKER);
  if (!has_achiev(A_JELLICLES)) {
    int k = 0;
    for (int i = 0; i < N_KITTENS; i++) k += has(LIST(KITTENS)[i]);
    if (k >= 10) win(A_JELLICLES);
  }
  if (S.cookies_earned >= 1e14 && !has_achiev(A_YOU_WIN_A_COOKIE)) win(A_YOU_WIN_A_COOKIE), earn(1);
  if (G.nshimmers_golden >= 4) win(A_FOUR_LEAF_COOKIE);
  int grandmas = 0;
  for (int i = 0; i < N_GRANDMA_SYNERGIES; i++) grandmas += has(LIST(GRANDMA_SYNERGIES)[i]);
  if (!has_achiev(A_ELDER) && grandmas >= 7) win(A_ELDER);
  if (!has_achiev(A_VETERAN) && grandmas >= 14) win(A_VETERAN);
  if (S.buildings[B_GRANDMA].amount >= 6 && !has(U_BINGO_CENTER_RESEARCH_FACILITY) && has_achiev(A_ELDER))
    unlock(U_BINGO_CENTER_RESEARCH_FACILITY);
  if (S.pledges > 0) win(A_ELDER_NAP);
  if (S.pledges >= 5) win(A_ELDER_SLUMBER);
  if (S.pledges >= 10) unlock(U_SACRIFICIAL_ROLLING_PINS);
  if (S.buildings[B_CURSOR].amount + S.buildings[B_GRANDMA].amount >= 777) win(A_THE_ELDER_SCROLLS);
  for (int b = 0; b < NB; b++) {
    const BuildingLinks *l = building_links(b);
    for (int i = 0; i < 3; i++)
      if (l->prod_ach[i] != NONE && S.buildings[b].total_cookies >= l->prod_pow[i]) win(l->prod_ach[i]);
  }
  /* the big cookie dips into the milk */
  float panel = SCREEN_H - NEWS_H;
  if (!has_achiev(A_COOKIE_DUNKER) && G.milk_progress > 0.1f &&
      panel * 0.4f + 256 * 0.37f / 2 - 16 * 0.37f > (1 - G.milk_hd) * panel)
    win(A_COOKIE_DUNKER);
}

/* ------------------------------------------------------------------ a frame */
extern void update_grandmapocalypse(void);
extern void update_special(void);
extern void do_lumps(void);
extern void minigames_logic(void);
extern void wrinklers_update(void);
extern void season_logic(void);
extern void ascend_logic(void);

void game_logic(void) {
  if (G.on_ascend || G.ascend_timer > 0) {
    ascend_logic();
    G.T++;
    G.real_t += 1;
    return;
  }
  update_special();
  update_grandmapocalypse();

  /* the big cookie's size: it wobbles when clicked */
  if (S.prefs.wobbly) {
    G.big_size_t = G.big_state == 1 ? 0.98f : G.big_state == 2 ? 1.05f : 1;
    G.big_size_d += (G.big_size_t - G.big_size) * 0.75f;
    G.big_size_d *= 0.75f;
    G.big_size += G.big_size_d;
    if (G.big_size < 0.1f) G.big_size = 0.1f;
  } else
    G.big_size += ((G.big_state == 1 ? 0.98f : G.big_state == 2 ? 1.05f : 1) - G.big_size) * 0.5f;

  /* particles */
  for (int i = 0; i < MAX_PARTICLES; i++) {
    Particle *p = &G.particles[i];
    if (p->life < 0) continue;
    p->yd += 0.25f * 0.37f;
    p->x += p->xd, p->y += p->yd;
    if (++p->life >= FPS * p->dur) p->life = -1;
  }
  for (int i = 0; i < MAX_TEXT_PARTICLES; i++) {
    TextParticle *t = &G.texts[i];
    if (t->life < 0) continue;
    t->y += t->yd;
    if (++t->life >= t->dur) t->life = -1;
  }
  for (int i = 0; i < MAX_POPUPS; i++)
    if (G.popups[i].life > 0) G.popups[i].life--;

  /* milk */
  G.milk_progress = G.achievements_owned / 25.f;
  static const uint16_t kittens[] = {U_KITTEN_HELPERS, U_KITTEN_WORKERS, U_KITTEN_ENGINEERS, U_KITTEN_OVERSEERS,
                                     U_KITTEN_MANAGERS, U_KITTEN_ACCOUNTANTS, U_KITTEN_SPECIALISTS, U_KITTEN_EXPERTS,
                                     U_KITTEN_CONSULTANTS, U_KITTEN_ASSISTANTS_TO_THE_REGIONAL_MANAGER,
                                     U_KITTEN_MARKETEERS, U_KITTEN_ANALYSTS, U_KITTEN_EXECUTIVES, U_KITTEN_ADMINS,
                                     U_KITTEN_STRATEGISTS};
  if (G.milk_progress >= 0.5f) unlock(kittens[0]);
  for (int i = 1; i < 15; i++)
    if (G.milk_progress >= i) unlock(kittens[i]);
  G.milk_h = (G.milk_progress < 1 ? G.milk_progress : 1) * 0.35f;
  G.milk_hd += (G.milk_h - G.milk_hd) * 0.02f;

  /* research */
  if (S.research_t > 0) S.research_t--;
  if (S.research_t == 0 && S.next_research) {
    if (!has(S.next_research)) {
      unlock(S.next_research);
      char d[96];
      snprintf(d, sizeof d, "You have discovered: \x01%s\x02.", res_text(upgrade_rec(S.next_research)->name));
      notify("Research complete", d, upgrade_rec(S.next_research)->icon, 0);
    }
    S.next_research = 0, S.research_t = -1, G.recalculate = true;
  }
  season_logic();

  /* cookies */
  if (G.recalculate) calculate_gains();
  earn(G.cookies_ps / FPS);
  do_lumps();
  minigames_logic();
  if (G.cps_sucked > 0) {
    double d = G.cookies_ps / FPS * G.cps_sucked;
    dissolve(d);
    S.cookies_sucked += d;
  }
  for (int b = 0; b < NB; b++) S.buildings[b].total_cookies += S.buildings[b].stored_total_cps * G.global_cps_mult / FPS;
  /* the cookies that fall behind the big one */
  if (S.prefs.particles && S.cookies > 0) {
    double c = G.cookies_ps < 10 ? G.cookies_ps : 10;
    if (c > 0 && G.T % (uint32_t)ceil(FPS / c) == 0) particle_add(0, 0, 0, 0, 1, 2, 0);
  }
  if (G.T % (FPS * 10) == 0) G.recalculate = true;

  if (G.T % FPS == 0 && rnd() < 1.0 / 1000000) win(A_JUST_PLAIN_LUCKY);
  if (G.T % (FPS * 5) == 0) game_check();

  if (isnan(G.cookies_d)) G.cookies_d = 0;
  if (G.cookies_d == G.cookies_d + (S.cookies - G.cookies_d) * 0.3) G.cookies_d = S.cookies;
  else G.cookies_d += (S.cookies - G.cookies_d) * 0.3;

  if (G.upgrades_dirty) game_rebuild_upgrades();
  shimmers_update();
  buffs_update();
  wrinklers_update();

  /* notes that go away by themselves */
  for (int i = 0; i < MAX_NOTES; i++) {
    Note *n = &G.notes[i];
    if (!n->title[0]) continue;
    n->age++;
    if (n->life > 0 && --n->life == 0) n->title[0] = 0;
  }
  G.click_t++;
  G.T++;
  G.real_t += 1;
  S.play_time += 1000.0 / FPS;
}
