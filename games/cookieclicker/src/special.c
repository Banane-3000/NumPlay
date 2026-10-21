/* Santa and Krumblor, the cookie dragon (the game's special tabs): Santa evolves through 15 levels
 * for cookies; the dragon's egg hatches, then trains auras for buildings sacrificed; its auras. */
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "game.h"

const char *const santa_levels[15] = {"Festive test tube", "Festive ornament", "Festive wreath", "Festive tree",
                                      "Festive present", "Festive elf fetus", "Elf toddler", "Elfling", "Young elf",
                                      "Bulky elf", "Nick", "Santa Claus", "Elder Santa", "True Santa", "Final Claus"};
const char *const aura_names[N_AURAS] = {
  "No aura", "Breath of Milk", "Dragon Cursor", "Elder Battalion", "Reaper of Fields", "Earth Shatterer",
  "Master of the Armory", "Fierce Hoarder", "Dragon God", "Arcane Aura", "Dragonflight", "Ancestral Metamorphosis",
  "Unholy Dominion", "Epoch Manipulator", "Mind Over Matter", "Radiant Appetite", "Dragon's Fortune", "Dragon's Curve",
  "Reality Bending", "Dragon Orbs", "Supreme Intellect", "Dragon Guts"};
const char *const aura_descs[N_AURAS] = {
  "Select an aura from those your dragon knows.", "Kittens are \x01" "5%\x02 more effective.",
  "Clicking is \x01" "5%\x02 more powerful.", "Grandmas gain \x01+1% CpS\x02 for each non-grandma building.",
  "Golden cookies may trigger a \x01" "Dragon Harvest\x02.", "Buildings sell back for \x01" "50%\x02 instead of 25%.",
  "All upgrades are \x01" "2% cheaper\x02.", "All buildings are \x01" "2% cheaper\x02.",
  "\x01+5%\x02 prestige level effect on CpS.", "Golden cookies appear \x01" "5%\x02 more often.",
  "Golden cookies may trigger a \x01" "Dragonflight\x02.", "Golden cookies give \x01" "10%\x02 more cookies.",
  "Wrath cookies give \x01" "10%\x02 more cookies.", "Golden cookie effects last \x01" "5%\x02 longer.",
  "Random drops are \x01" "25% more common\x02.", "All cookie production \x01multiplied by 2\x02.",
  "\x01+123% CpS\x02 per golden cookie on-screen, multiplicative.",
  "\x01+5%\x02 sugar lump growth. Sugar lumps are \x01twice as likely\x02 to be unusual.",
  "\x01One tenth\x02 of every other dragon aura, \x01" "combined\x02.",
  "With no buffs and no golden cookies on screen, selling your most powerful building has \x01" "10% chance to "
  "summon one\x02.",
  "Confers various powers to your minigames while active.\nSee the bottom of each minigame for more details.",
  "You can attract \x01" "2 more wrinklers\x02.\nWrinklers digest \x01" "20% more cookies\x02.\nWrinklers explode into "
  "\x01" "20% more cookies\x02."};
const int16_t aura_icons[N_AURAS] = {ICON(0, 7), ICON(18, 25), ICON(0, 25), ICON(1, 25), ICON(2, 25), ICON(3, 25),
                                     ICON(4, 25), ICON(15, 25), ICON(16, 25), ICON(17, 25), ICON(5, 25), ICON(6, 25),
                                     ICON(7, 25), ICON(8, 25), ICON(13, 25), ICON(14, 25), ICON(19, 25), ICON(20, 25),
                                     ICON(32, 25), ICON(33, 25), ICON(34, 25), ICON(35, 25)};
#define DRAGON_LEVELS 28
int dragon_level_count(void) { return DRAGON_LEVELS; }

float dragon_aura_mult(int aura) {
  float n = 0;
  if (S.dragon_aura == aura || S.dragon_aura2 == aura) n = 1;
  if ((S.dragon_aura == AURA_REALITY_BENDING || S.dragon_aura2 == AURA_REALITY_BENDING) && S.dragon_level >= aura + 4)
    n += 0.1f;
  return n;
}

/* the dragon's name and its pictures' frame at each level */
const char *dragon_name(void) {
  int l = S.dragon_level;
  return l < 3 ? "Dragon egg" : l == 3 ? "Shivering dragon egg" : l < 8 ? "Krumblor, cookie hatchling"
                                                                        : "Krumblor, cookie dragon";
}
int dragon_frame(void) {
  int l = S.dragon_level;
  return l < 4 ? l : l < 8 ? 4 : l < 25 ? 5 : l == 25 ? 6 : l == 26 ? 7 : 8;
}
/* what the next level does and costs; whether it can be paid */
void dragon_action(char *action, int asize, char *cost, int csize, bool *can) {
  int l = S.dragon_level;
  *can = false;
  cost[0] = 0;
  if (l < 4) {
    double c = 1000000.0 * (1 << l);
    snprintf(action, asize, l < 3 ? "Chip it" : "Hatch it");
    cookies_str(c, 0, cost);
    *can = S.cookies >= c;
  } else if (l < 25) {
    snprintf(action, asize, "Train %s\nAura: %s", aura_names[l - 3], aura_descs[l - 3]);
    if (l == 4) {
      cookies_str(16000000, 0, cost);
      *can = S.cookies >= 16000000;
    } else {
      int b = l - 5;
      snprintf(cost, csize, "100 %s", res_text(building_rec(b)->plural));
      *can = S.buildings[b].amount >= 100;
    }
  } else if (l == 25) {
    snprintf(action, asize, "Bake dragon cookie\nDelicious!");
    snprintf(cost, csize, "50 of every building");
    *can = true;
    for (int b = 0; b < NB; b++)
      if (S.buildings[b].amount < 50) *can = false;
  } else if (l == 26) {
    snprintf(action, asize, "Train secondary aura\nLets you use two dragon auras simultaneously");
    snprintf(cost, csize, "200 of every building");
    *can = true;
    for (int b = 0; b < NB; b++)
      if (S.buildings[b].amount < 200) *can = false;
  } else
    snprintf(action, asize, "Your dragon is fully trained.");
}

static void sacrifice(int b, int n) {
  Building *m = &S.buildings[b];
  for (int i = 0; i < n && m->amount > 0; i++) m->amount--, G.buildings_owned--;
  G.recalculate = true, G.store_dirty = true;
}

void upgrade_dragon(void) {
  char a[200], c[64];
  bool can;
  dragon_action(a, sizeof a, c, sizeof c, &can);
  int l = S.dragon_level;
  if (l >= DRAGON_LEVELS - 1 || !can) return;
  if (l < 5) spend(1000000.0 * (1 << l));
  else if (l < 25) sacrifice(l - 5, 100);
  else if (l == 25) {
    for (int b = 0; b < NB; b++) sacrifice(b, 50);
    unlock(U_DRAGON_COOKIE);
  } else if (l == 26)
    for (int b = 0; b < NB; b++) sacrifice(b, 200);
  S.dragon_level = (uint8_t)((l + 1) % DRAGON_LEVELS);
  if (S.dragon_level >= DRAGON_LEVELS - 1) win(A_HERE_BE_DRAGON);
  G.recalculate = true, G.upgrades_dirty = true;
}

/* choosing an aura: free with no buildings, else the most expensive building owned goes */
void set_dragon_aura(int slot, int aura) {
  int current = slot ? S.dragon_aura2 : S.dragon_aura;
  if (aura == current) return;
  int highest = -1;
  for (int b = 0; b < NB; b++)
    if (S.buildings[b].amount > 0) highest = b;
  if (highest >= 0) sacrifice(highest, 1);
  if (slot) S.dragon_aura2 = (uint8_t)aura;
  else S.dragon_aura = (uint8_t)aura;
  G.recalculate = true;
}

/* petting the dragon (Pet the dragon): at level 8 and up, one chance in 20 of a drop, the one for the
 * time (here: for the play time's minute) */
void pet_dragon(void) {
  if (S.dragon_level < 4 || !has(U_PET_THE_DRAGON)) return;
  if (S.dragon_level >= 8 && rnd() < 1.0 / 20) {
    static const uint16_t drops[4] = {U_DRAGON_SCALE, U_DRAGON_CLAW, U_DRAGON_FANG, U_DRAGON_TEDDY_BEAR};
    int minute = (int)(S.play_time / 60000) % 60;
    int d = drops[(minute * 4 / 60 + S.bakery_name1) % 4];
    if (!has(d) && !has_unlocked(d)) {
      unlock(d);
      notify(res_text(upgrade_rec(d)->name), "\x01Your dragon dropped something!\x02", upgrade_rec(d)->icon, 0);
    }
  }
}

/* Santa evolves */
void upgrade_santa(void) {
  double moni = pow(S.santa_level + 1, S.santa_level + 1);
  if (S.cookies <= moni || S.santa_level >= 14) return;
  spend(moni);
  S.santa_level = (uint8_t)((S.santa_level + 1) % 15);
  if (S.santa_level == 14) {
    unlock(U_SANTAS_DOMINION);
    char t[96];
    snprintf(t, sizeof t, "You are granted %s.", res_text(upgrade_rec(U_SANTAS_DOMINION)->name));
    notify(t, "", upgrade_rec(U_SANTAS_DOMINION)->icon, 0);
  }
  int cand[N_SANTA_DROPS], n = 0;
  for (int i = 0; i < N_SANTA_DROPS; i++)
    if (!has_unlocked(LIST(SANTA_DROPS)[i])) cand[n++] = LIST(SANTA_DROPS)[i];
  if (n) {
    int d = cand[rnd_int(n)];
    unlock(d);
    char t[96];
    snprintf(t, sizeof t, "You find a present which contains...\n\x01%s\x02!", res_text(upgrade_rec(d)->name));
    notify("Found a present!", t, upgrade_rec(d)->icon, 0);
  }
  if (S.santa_level >= 6) win(A_COMING_TO_TOWN);
  if (S.santa_level >= 14) win(A_ALL_HAIL_SANTA);
  G.recalculate = true, G.upgrades_dirty = true;
}

void update_special(void) {
  extern void reincarnate_logic(void);
  reincarnate_logic();
}

/* selling: Godzamok's Devastation, the Dragon Orbs */
void building_sold(int b, int sold) {
  int god = hasgod(GOD_RUIN);
  if (god) {
    static const float k[4] = {0, 0.01f, 0.005f, 0.0025f};
    Buff *old = find_buff(BUFF_DEVASTATION);
    if (old) old->mult_click += sold * k[god];
    else gain_buff(BUFF_DEVASTATION, 10, 1 + sold * k[god], 0);
  }
  if (G.nshimmers_golden <= 0 && aura_mult(AURA_DRAGON_ORBS) > 0) {
    int highest = -1;
    for (int i = 0; i < NB; i++)
      if (S.buildings[i].amount > 0) highest = i;
    if ((highest < 0 || highest <= b) && rnd() < aura_mult(AURA_DRAGON_ORBS) * 0.1) {
      int buffs = 0;
      for (int i = 0; i < MAX_BUFFS; i++) buffs += S.buffs[i].used;
      if (!buffs) {
        shimmer_new(SHIMMER_GOLDEN, GC_NONE, false);
        notify("Dragon Orbs!", "Wish granted. Golden cookie spawned.", ICON(33, 25), 0);
      }
    }
  }
}
