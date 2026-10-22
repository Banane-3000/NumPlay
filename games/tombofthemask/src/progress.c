/* What a player keeps between runs: the original's settings (RSettings.InitDefaults), the shop, the masks
 * (BlockMaskController), the missions (DataManager's mission functions) and the chest's wheel
 * (RouletteController). */
#include <string.h>
#include "game.h"

const uint32_t tm_level_xp[20] = {0, 1000, 3600, 8670, 17458, 31738, 54015, 87802, 138000, 211415, 317460, 469104,
                                  684163, 987038, 1411063, 2001669, 2820643, 3951851, 5508926, 7645579};
const uint16_t tm_upgrade_cost[9] = {200, 400, 800, 1000, 1500, 2000, 3000, 4000, 5000};
/* by upgrade level: coin addict (dots), freeze, magnet, score booster (seconds) */
const uint8_t tm_powerup_secs[4][10] = {{10, 12, 15, 18, 21, 24, 27, 30, 35, 40}, {5, 6, 7, 8, 9, 10, 11, 12, 13, 14},
                                        {5, 6, 8, 10, 12, 14, 16, 18, 19, 20}, {5, 6, 8, 10, 12, 14, 16, 18, 19, 20}};
const uint16_t tm_mask_cost[16] = {0, 200, 500, 1000, 1000, 1000, 2000, 3000, 4000, 5000, 5000, 5000, 5000, 5000,
                                   5000, 5000};
const uint8_t tm_mask_level[16] = {0, 2, 2, 3, 4, 5, 6, 6, 8, 9, 10, 12, 14, 16, 18, 20};
const char *const tm_mask_name[16] = {"Travelboy", "Cyclop", "Crow", "Dog", "Kitty", "Pirate", "Rabbit", "Shapito",
                                      "Skull", "Voodoo", "Koala", "Piggy", "Robot", "Froggy", "Knight", "Agent"};
/* MaskFeatures0..15: (feature, value): 1 more coins, 2..5 more coin addicts, freezes, magnets, boosters, 6 more
 * bonus zones, 7 no snakes, 8 no monkeys */
const uint8_t tm_mask_feature[16][3][2] = {
  {{0, 0}}, {{1, 20}}, {{6, 20}}, {{1, 20}, {7, 0}}, {{4, 10}, {1, 20}}, {{1, 20}, {8, 0}}, {{1, 30}, {3, 10}},
  {{6, 10}, {3, 10}, {4, 10}}, {{1, 35}, {3, 15}, {2, 15}}, {{1, 40}, {2, 20}, {3, 20}}, {{6, 35}}, {{1, 50}},
  {{1, 30}, {2, 20}}, {{5, 40}}, {{1, 20}, {5, 20}}, {{1, 40}, {4, 20}, {3, 20}}};
/* MissionsInfo0..7 and MissionsRewards */
const uint16_t tm_mission_goal[8][7] = {{2, 7, 12, 17, 22, 37, 49}, {1, 5, 10, 20, 30, 40, 50},
                                        {500, 1000, 2000, 3000, 5000, 7000, 10000}, {5, 10, 15, 25, 35, 50, 0},
                                        {5, 10, 15, 20, 25, 35, 45}, {2, 3, 5, 10, 15, 0, 0},
                                        {200, 500, 1000, 1500, 2000, 4000, 7000}, {3, 5, 10, 15, 20, 25, 30}};
const uint8_t tm_mission_levels[8] = {7, 7, 7, 6, 7, 5, 7, 7};
const uint8_t tm_mission_reward[7] = {25, 50, 50, 75, 75, 100, 100};
/* mainColors and subColors of the GameController prefab, as RGB565 */
const uint16_t tm_colors[2][13] = {
  {0xD01F, 0x601F, 0xE001, 0x0358, 0xCBA0, 0xD807, 0x5BC0, 0x4A34, 0x8159, 0x7AE5, 0x1412, 0x9185, 0x0505},
  {0xD01A, 0x381F, 0xE120, 0x04DF, 0xB280, 0xB007, 0x52C0, 0x4334, 0x5138, 0x7224, 0x0BB1, 0x7142, 0x050A}};

static int32_t iabs32(int32_t v) { return v < 0 ? -v : v; }
static uint32_t next_rand(uint32_t *s) {
  uint32_t x = *s ? *s : 0x9E3779B9u;
  x ^= x << 13, x ^= x >> 17, x ^= x << 5;
  return *s = x;
}
static int range(uint32_t *s, int lo, int hi) { return hi > lo ? lo + (int)(next_rand(s) % (uint32_t)(hi - lo)) : lo; }

void tm_new_progress(tm_progress *p) {
  memset(p, 0, sizeof *p);
  p->masks = 1;
  p->chest_free = 1;
  p->mission_type[0] = 0, p->mission_type[1] = 1, p->mission_type[2] = 2;
}

bool tm_progress_ok(const tm_progress *p) {
  if (p->reached >= 200 || p->mask >= 16 || !(p->masks & 1) || !(p->masks >> p->mask & 1) || p->shields > 10)
    return false;
  for (int i = 0; i < 4; i++)
    if (p->upgrades[i] > 9) return false;
  for (int i = 0; i < 200; i++)
    if (p->stars[i] > 4) return false;
  for (int i = 0; i < 3; i++) {
    if (p->mission_type[i] >= 8 || p->mission_index[i] >= tm_mission_levels[p->mission_type[i]]) return false;
    for (int j = 0; j < i; j++)
      if (p->mission_type[j] == p->mission_type[i]) return false;
  }
  for (int t = 0; t < 8; t++)
    if (p->missions_done[t] >> tm_mission_levels[t]) return false;
  return p->chest_free <= 1 && p->guide <= 3 && p->chest_wait <= TM_CHEST_WAIT;
}

unsigned tm_level(uint32_t xp) {
  unsigned n = 1;
  while (n < 20 && xp >= tm_level_xp[n]) n++;
  return n;
}

/* ------------------------------------------------------------------ the shop */
bool tm_buy_shield(tm_progress *p) {
  if (p->shields >= 10 || p->coins < 200) return false;
  p->coins -= 200;
  p->shields++;
  return true;
}

bool tm_buy_upgrade(tm_progress *p, int which) {
  if (which < 0 || which > 3 || p->upgrades[which] >= 9 || p->coins < tm_upgrade_cost[p->upgrades[which]]) return false;
  p->coins -= tm_upgrade_cost[p->upgrades[which]];
  p->upgrades[which]++;
  return true;
}

/* ------------------------------------------------------------------ masks: each needs its level, then its price */
bool tm_mask_open(const tm_progress *p, int m) { return tm_level(p->xp) >= tm_mask_level[m]; }

bool tm_buy_mask(tm_progress *p, int m) {
  if (m < 0 || m >= 16) return false;
  if (!(p->masks >> m & 1)) {
    if (!tm_mask_open(p, m) || p->coins < tm_mask_cost[m]) return false;
    p->coins -= tm_mask_cost[m];
    p->masks |= (uint16_t)(1 << m);
  }
  p->mask = (uint8_t)m;
  return true;
}

/* ------------------------------------------------------------------ missions */
/* the kind by type: complete stage N and stage N without a revive are exact, points in one run at least */
int tm_mission_kind(int type) { return type == 0 || type == 4 ? 2 : type == 6 ? 1 : 0; }

uint32_t tm_mission_goal_of(const tm_progress *p, int slot) {
  return tm_mission_goal[p->mission_type[slot]][p->mission_index[slot]];
}

bool tm_mission_ready(const tm_progress *p, int slot) {
  uint32_t goal = tm_mission_goal_of(p, slot), v = p->mission_progress[slot];
  return tm_mission_kind(p->mission_type[slot]) == 2 ? v == goal : v >= goal;
}

uint32_t tm_mission_shown(const tm_progress *p, int slot) {
  uint32_t goal = tm_mission_goal_of(p, slot), v = p->mission_progress[slot];
  return v > goal ? goal : v;
}

/* IncrementMissionProgress */
void tm_mission_add(tm_progress *p, int type, uint32_t amount) {
  for (int i = 0; i < 3; i++) {
    if (p->mission_type[i] != type) continue;
    int kind = tm_mission_kind(type);
    uint32_t goal = tm_mission_goal_of(p, i);
    if ((kind == 1 && p->mission_progress[i] >= goal) || (kind == 2 && p->mission_progress[i] == goal)) continue;
    uint32_t v = p->mission_progress[i] + amount;
    p->mission_progress[i] = v < p->mission_progress[i] ? 0xFFFFFFFFu : v;
  }
}

/* ResetOneRunMissionsProgress */
void tm_mission_new_run(tm_progress *p) {
  for (int i = 0; i < 3; i++) {
    int kind = tm_mission_kind(p->mission_type[i]);
    uint32_t goal = tm_mission_goal_of(p, i);
    if ((kind == 1 && p->mission_progress[i] < goal) || (kind == 2 && p->mission_progress[i] != goal))
      p->mission_progress[i] = 0;
  }
}

/* the reward, and the slot refilled: the lowest level not done among the types not shown (one at random), or when
 * every mission is done, a repeatable type (MissionsToRepeat) at its last level */
bool tm_claim_mission(tm_progress *p, int slot, uint32_t *rng) {
  if (slot < 0 || slot > 2 || !tm_mission_ready(p, slot)) return false;
  int t = p->mission_type[slot], k = p->mission_index[slot];
  p->coins += tm_mission_reward[k];
  p->missions_done[t] |= (uint8_t)(1 << k);
  int best = 99, cand[8], n = 0;
  for (int u = 0; u < 8; u++) {
    if (u == p->mission_type[(slot + 1) % 3] || u == p->mission_type[(slot + 2) % 3]) continue;
    int lv = 0;
    while (lv < tm_mission_levels[u] && (p->missions_done[u] >> lv & 1)) lv++;
    if (lv >= tm_mission_levels[u]) continue;
    if (lv < best) best = lv, n = 0;
    if (lv == best) cand[n++] = u;
  }
  int nt, ni;
  if (n) {
    nt = cand[range(rng, 0, n)], ni = best;
  } else {
    static const uint8_t repeat[6] = {1, 2, 3, 5, 6, 7};
    for (int i = 0; i < 6; i++)
      if (repeat[i] != p->mission_type[(slot + 1) % 3] && repeat[i] != p->mission_type[(slot + 2) % 3]) cand[n++] = repeat[i];
    nt = cand[range(rng, 0, n)], ni = tm_mission_levels[nt] - 1;
  }
  p->mission_type[slot] = (uint8_t)nt, p->mission_index[slot] = (uint8_t)ni, p->mission_progress[slot] = 0;
  return true;
}

/* ------------------------------------------------------------------ the chest's wheel */
bool tm_chest_free(const tm_progress *p) { return p->chest_free || p->chest_wait == 0; }

/* RouletteContent: (kind, value): 0 coins x 25, 1 a box, 3 a shield */
static const uint8_t wheel_content[8][2] = {{0, 1}, {0, 8}, {0, 1}, {1, 1}, {0, 1}, {0, 10}, {0, 1}, {3, 1}};

/* SpinWheel: 13 degrees a step; it starts slowing after 1.48 s and a bit (RouletteWinChances: the first spin lands
 * a big prize, the second one half the time) */
bool tm_wheel_start(tm_progress *p, tm_wheel *w, uint32_t *rng) {
  if (tm_chest_free(p)) {
    p->chest_free = 0;
    p->chest_wait = TM_CHEST_WAIT;
  } else if (p->coins >= TM_CHEST_COST) {
    p->coins -= TM_CHEST_COST;
  } else {
    return false;
  }
  memset(w, 0, sizeof *w);
  tm_mission_add(p, 7, 1);
  static const uint8_t win[3] = {100, 50, 0};
  w->speed = 13 << 16;
  if (range(rng, 0, 100) < win[p->wheel_plays > 2 ? 2 : p->wheel_plays])
    w->delay = (148 + 6 * (range(rng, 0, 2) ? 7 : 3)) * 60 / 100;
  else
    w->delay = (int)((1480 + (int64_t)480 * (next_rand(rng) >> 16) / 65536) * 60 / 1000);
  if (p->wheel_plays < 0xFFFF) p->wheel_plays++;
  w->spinning = true;
  return true;
}

/* the prize (ProcessPrize); a box holds a mask not yet had (2 in 10), coins (5) or a shield (3) */
static void prize(tm_progress *p, tm_wheel *w, uint32_t *rng) {
  int kind = wheel_content[w->sector][0], v = wheel_content[w->sector][1];
  if (kind == 1) {
    int locked[16], n = 0;
    for (int m = 1; m < 16; m++)
      if (!(p->masks >> m & 1)) locked[n++] = m;
    int chance_mask = n ? 2 : 0, r = range(rng, 0, chance_mask + 5 + 3);
    if (r < chance_mask) {
      int m = locked[range(rng, 0, n)];
      p->masks |= (uint16_t)(1 << m);
      p->mask = (uint8_t)m;
      w->kind = 1, w->amount = m;
      return;
    }
    r -= chance_mask;
    if (r < 3) kind = 3, v = 1;
    else kind = 0, v = range(rng, 8, 13);
  }
  w->kind = kind;
  if (kind == 3) {
    w->amount = 1;
    if (p->shields < 10) p->shields++;
    else p->coins += 200;   /* (a full inventory: its price instead) */
  } else {
    w->amount = 25 * v;
    p->coins += (uint32_t)w->amount;
  }
}

/* FixedUpdate and Update: the turn, the pegs every 45 degrees (a wheel stopping on one rolls back), the slowing
 * (x 0.98 a 60th of a second), and the sector under the pointer */
void tm_wheel_step(tm_progress *p, tm_wheel *w, uint32_t *rng) {
  if (!w->spinning) return;
  w->angle -= (int32_t)((int64_t)w->speed * 1024 >> 16);
  if (w->angle < -360 * 1024) w->angle += 360 * 1024;
  else if (w->angle >= 360 * 1024) w->angle -= 360 * 1024;
  int32_t g = -w->angle;
  while (g > 45 * 1024) g -= 45 * 1024;
  bool on_peg = g >= 0 && g <= 8 * 1024;
  if (on_peg) {
    w->peg_angle = g * 5;
    if (w->speed > 0) w->peg = 1;
  } else {
    w->peg = 0;
  }
  if (iabs32(w->speed) < 6554) {   /* 0.1 degree */
    if (w->peg) {
      w->peg = 0;
      w->speed = -(int32_t)(9830 + (int64_t)9830 * w->peg_angle / (40 * 1024));   /* -0.15 (1 + g / 40) */
    } else {
      w->spinning = false;
      int32_t z = w->angle < 0 ? w->angle + 360 * 1024 : w->angle;   /* the wheel's z angle, 0..360 */
      int32_t d = 360 * 1024 - z + 1024;
      if (d < 359 * 1024) d += 1024;
      int s = (d >> 10) / 45;
      w->sector = s >= 8 ? 0 : s;
      prize(p, w, rng);
      w->done = true;
      return;
    }
  }
  if (w->delay > 0) w->delay--;
  if (w->delay <= 0) w->speed = (int32_t)((int64_t)w->speed * 49 / 50);
}
