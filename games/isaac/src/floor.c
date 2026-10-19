/* A floor: its rooms laid out as the game lays them, and going from room to room.
 *
 * The layout is the original's. From the starting room, each room just
 * added tries each of its four sides, one time in two; a new room may touch
 * no room but the one it grows from, so the floor is a tree, and the floor
 * stops at min(20, 5 + 0 or 1 + 2.6 x the floor's number) rooms. Rooms that
 * grew nothing are its dead ends: the last one is the boss's, others become
 * the treasure room, the shop and the other special rooms. The secret room
 * goes in an empty cell next to at least three rooms (or two), none of them
 * special; the super secret room next to a single one. */
#include <string.h>
#include "game.h"
#include "gfx.h"
#include "res.h"

Floor fl;

int chapter_of(int stage) { return stage <= 8 ? (stage + 1) / 2 : stage <= 10 ? 5 : 6; }

const char *stage_name(int stage, bool alt) {
  static const char *const names[][2] = {
    {"BASEMENT I", "CELLAR I"}, {"BASEMENT II", "CELLAR II"}, {"THE CAVES I", "CATACOMBS I"},
    {"THE CAVES II", "CATACOMBS II"}, {"THE DEPTHS I", "NECROPOLIS I"}, {"THE DEPTHS II", "NECROPOLIS II"},
    {"THE WOMB I", "UTERO I"}, {"THE WOMB II", "UTERO II"}, {"SHEOL", "CATHEDRAL"}};
  if (stage < 1) stage = 1;
  if (stage > 9) stage = 9;
  return names[stage - 1][alt ? 1 : 0];
}

int room_backdrop(void) {
  Room *r = room_cur();
  switch (r->kind) {
    case ROOM_SHOP: return BD_SHOP;
    case ROOM_SECRET: case ROOM_SUPERSECRET: return BD_SECRET;
    case ROOM_ARCADE: return BD_ARCADE;
    case ROOM_LIBRARY: return BD_LIBRARY;
    case ROOM_SACRIFICE: return BD_SACRIFICE;
    case ROOM_CURSE: return BD_BARREN;
    case ROOM_DEVIL: return BD_SHEOL;
    case ROOM_ANGEL: return BD_CATHEDRAL;
    default: break;
  }
  static const uint8_t bd[5][2] = {{BD_BASEMENT, BD_CELLAR}, {BD_CAVES, BD_CATACOMBS}, {BD_DEPTHS, BD_NECROPOLIS},
                                   {BD_WOMB, BD_UTERO}, {BD_SHEOL, BD_CATHEDRAL}};
  int c = chapter_of(fl.stage);
  if (c > 5) c = 5;
  return bd[c - 1][fl.alt ? 1 : 0];
}

Room *room_cur(void) { return &fl.rooms[fl.cur]; }

/* ---------------------------------------------------------------- the layout (the original's, on 10 x 8 cells) */
#define GW 10
#define GH 8
#define START 35
static int8_t cell[GW * GH];
static int ends[GW * GH], nends;
static int act[GW * GH], nact, act2[GW * GH], nact2;
static int added, target;

static int cellv(int i) { return i >= 0 && i < GW * GH ? cell[i] : 0; }
static void try_add(int i) {
  if (i < 0 || i >= GW * GH) return;
  int around = cellv(i + 1) + cellv(i - 1) + cellv(i + GW) + cellv(i - GW);
  if ((cell[i] == 0 && around < 2 && added < target) || i == START) {
    if (cell[i]) return;
    cell[i] = 1;
    act2[nact2++] = i;
    if (i != START) added++;
  }
}
static int neighbours(int i) {
  return (i % GW < GW - 1 && cellv(i + 1) > 0) + (i % GW > 0 && cellv(i - 1) > 0) + (cellv(i + GW) > 0) + (cellv(i - GW) > 0);
}
static int take_end(void) {
  if (!nends) return -1;
  int k = rnd(nends), v = ends[k];
  ends[k] = ends[--nends];
  return v;
}

static bool layout(int stage) {
  target = 5 + rnd(2) + (int)(stage * 2.6f);
  if (target > 20) target = 20;
  for (int tries = 0; tries < 1000; tries++) {
    memset(cell, 0, sizeof cell);
    nends = 0, added = 0, nact2 = 0;
    try_add(START);
    for (float e = 0; e < 300; e++) {
      nact = nact2;
      memcpy(act, act2, sizeof(int) * (size_t)nact);
      nact2 = 0;
      for (int a = 0; a < nact; a++) {
        int v = act[a], before = nact2;
        if (rnd(2) == 0 && v % GW < 9) try_add(v + 1);
        if (rnd(2) == 0 && v % GW > 1) try_add(v - 1);
        if (rnd(2) == 0 && v < 70) try_add(v + GW);
        if (rnd(2) == 0 && v > 10) try_add(v - GW);
        if (nact2 == before && v != START) ends[nends++] = v;
      }
      if (nact2 < 2) e -= 0.5f;
      if (!nact2) break;
    }
    if (added < target || nends < 3) continue;
    /* the boss: the last dead end found, and never right by the start */
    int boss = ends[nends - 1];
    if (boss == START + 1 || boss == START - 1 || boss == START + GW || boss == START - GW) continue;
    return true;
  }
  return false;
}

/* ---------------------------------------------------------------- the rooms */
static int pick_template(int kind, int chapter_bit) {
  int total = 0;
  for (int i = 0; i < room_count(); i++) {
    const RoomTemplate *t = room_get(i);
    if (t->kind == kind && (t->chapters & chapter_bit)) total += t->weight;
  }
  if (!total) return -1;
  int k = rnd(total);
  for (int i = 0; i < room_count(); i++) {
    const RoomTemplate *t = room_get(i);
    if (t->kind != kind || !(t->chapters & chapter_bit)) continue;
    if ((k -= t->weight) < 0) return i;
  }
  return -1;
}

static const uint8_t groups[GROUP_COUNT][5] = GROUP_TABLE;
static int pick_group(int tmpl, int chapter_bit) {
  const RoomTemplate *t = room_get(tmpl);
  bool slot[4] = {false};
  for (int r = 0; r < ROWS; r++)
    for (int c = 0; c < COLS; c++)
      if (t->cell[r][c] >= CELL_SLOT1 && t->cell[r][c] <= CELL_SLOT4) slot[t->cell[r][c] - CELL_SLOT1] = true;
  int ok[GROUP_COUNT], n = 0;
  for (int g = 0; g < GROUP_COUNT; g++) {
    if (!(groups[g][0] & chapter_bit)) continue;
    bool any = false;
    for (int s = 0; s < 4; s++) any |= slot[s] && groups[g][1 + s] != MON_COUNT;
    if (any) ok[n++] = g;
  }
  return n ? ok[rnd(n)] : 0;
}

static int add_room(int mx, int my, int kind) {
  if (fl.nrooms >= MAX_ROOMS) return -1;
  int i = fl.nrooms++;
  Room *r = &fl.rooms[i];
  memset(r, 0, sizeof *r);
  r->kind = (uint8_t)kind, r->mx = (uint8_t)mx, r->my = (uint8_t)my;
  if (mx >= 0 && my >= 0 && mx < MAP_W && my < MAP_H) fl.map[my][mx] = (int8_t)i;
  int cb = 1 << (chapter_of(fl.stage) - 1);
  if (cb > 16) cb = 16;
  int t = pick_template(kind, cb);
  if (t < 0) t = pick_template(ROOM_NORMAL, cb);
  r->tmpl = (uint8_t)(t < 0 ? 0 : t);
  if (kind == ROOM_NORMAL || kind == ROOM_CURSE) r->group = (uint8_t)pick_group(r->tmpl, cb);
  return i;
}

/* the floor's boss, from its chapter's (Rebirth's) */
static int pick_boss(int stage) {
  static const uint8_t ch1[] = {MON_MONSTRO, MON_LARRY, MON_DUKE, MON_GEMINI, MON_GURDY_JR, MON_FAMINE, MON_STEVEN,
                                MON_WIDOW, MON_PIN, MON_HAUNT, MON_DINGLE, MON_BLIGHTED_OVUM};
  static const uint8_t ch2[] = {MON_CHUB, MON_GURDY, MON_PEEP, MON_PESTILENCE, MON_CARRION_QUEEN, MON_MEGA_FATTY,
                                MON_POLYCEPHALUS, MON_HUSK};
  static const uint8_t ch3[] = {MON_MONSTRO2, MON_LOKI, MON_WAR, MON_MASK_OF_INFAMY, MON_GISH, MON_BLOAT, MON_WRETCHED,
                                MON_DARK_ONE};
  static const uint8_t ch4[] = {MON_BLASTOCYST, MON_DADDY_LONG_LEGS, MON_SCOLEX, MON_DEATH, MON_LOKII, MON_CONQUEST,
                                MON_MAMA_GURDY, MON_MR_FRED, MON_TRIACHNID, MON_TERATOMA};
  if (stage == 6) return MON_MOM;
  if (stage == 8) return MON_MOMS_HEART;
  if (stage == 9) return fl.alt ? MON_ISAAC : MON_SATAN;
  const uint8_t *list;
  int n;
  switch (chapter_of(stage)) {
    case 1: list = ch1, n = sizeof ch1; break;
    case 2: list = ch2, n = sizeof ch2; break;
    case 3: list = ch3, n = sizeof ch3; break;
    default: list = ch4, n = sizeof ch4; break;
  }
  return list[rnd(n)];
}

void floor_new(int stage) {
  ents_clear();
  tears_clear();
  memset(&fl, 0, sizeof fl);
  memset(fl.map, -1, sizeof fl.map);
  fl.stage = (uint8_t)stage;
  fl.seed = g_rng;
  fl.alt = stage <= 8 ? (uint8_t)rnd(2) : fl.alt;
  fl.devil_room = -1;
  if (stage > 1 && stage < 9 && rnd(4) == 0) {
    static const uint8_t curses[] = {CURSE_DARKNESS, CURSE_LOST, CURSE_UNKNOWN, CURSE_BLIND, CURSE_MAZE};
    fl.curse = curses[rnd(sizeof curses)];
  }
  if (!layout(stage)) {   /* (never, in practice): a short line of rooms */
    memset(cell, 0, sizeof cell);
    for (int i = 0; i < 6; i++) cell[START + i - 2] = 1;
    nends = 0;
    ends[nends++] = START - 2, ends[nends++] = START + 3;
  }
  /* special rooms on the dead ends (the original's order and chances) */
  int kind[GW * GH];
  for (int i = 0; i < GW * GH; i++) kind[i] = cell[i] ? ROOM_NORMAL : -1;
  kind[START] = ROOM_START;
  int boss = ends[--nends];
  kind[boss] = ROOM_BOSS;
  bool deep = stage >= 9;
  int e;
  if (!deep && (e = take_end()) >= 0) kind[e] = ROOM_TREASURE;
  if (!deep && (e = take_end()) >= 0) kind[e] = ROOM_SHOP;
  if (nends && (rnd(7) == 0 || (pl.red >= pl.containers * 2 && rnd(4) == 0)) && (e = take_end()) >= 0) kind[e] = ROOM_SACRIFICE;
  if (nends && rnd(20) == 0 && (e = take_end()) >= 0) kind[e] = ROOM_LIBRARY;
  if (nends && rnd(2) == 0 && stage > 1 && (e = take_end()) >= 0) kind[e] = ROOM_CURSE;
  if (nends && (rnd(4) == 0 || (stage == 1 && rnd(3) == 0)) && stage > 1 && (e = take_end()) >= 0) kind[e] = ROOM_MINIBOSS;
  if (nends && pl.red >= pl.containers * 2 && stage > 1 && rnd(2) == 0 && (e = take_end()) >= 0) kind[e] = ROOM_CHALLENGE;
  if (nends && pl.coins >= 5 && stage % 2 == 0 && (e = take_end()) >= 0) kind[e] = ROOM_ARCADE;
  /* the map: the original's 10 x 8 on our 13 x 9, one cell in */
  for (int i = 0; i < GW * GH; i++) {
    if (kind[i] < 0) continue;
    int idx = add_room(i % GW + 1, i / GW + 1, kind[i]);
    if (i == START) fl.start = (int8_t)idx;
    if (i == boss) fl.boss_room = (int8_t)idx;
  }
  /* the secret room: the empty cell with the most rooms around (three, else two), none special */
  int best = -1, bestn = 1;
  for (int i = 0; i < GW * GH; i++) {
    if (cell[i] || i % GW < 1 || i % GW > 8) continue;
    int n = neighbours(i);
    bool ok = true;
    const int d[4] = {1, -1, GW, -GW};
    for (int k = 0; k < 4; k++) {
      int j = i + d[k];
      if (j < 0 || j >= GW * GH || (k < 2 && (j / GW != i / GW))) continue;
      if (cell[j] && kind[j] != ROOM_NORMAL && kind[j] != ROOM_START) ok = false;
    }
    if (ok && (n > bestn || (n == bestn && rnd(3) == 0))) best = i, bestn = n;
  }
  if (best >= 0) {
    cell[best] = 2;
    add_room(best % GW + 1, best / GW + 1, ROOM_SECRET);
  }
  /* the super secret room: next to a single normal room */
  for (int tries = 0; tries < 200; tries++) {
    int i = rnd(GW * GH);
    if (cell[i] || i % GW < 1 || i % GW > 8 || neighbours(i) != 1) continue;
    const int d[4] = {1, -1, GW, -GW};
    bool ok = true;
    for (int k = 0; k < 4; k++) {
      int j = i + d[k];
      if (j >= 0 && j < GW * GH && cell[j] && kind[j] != ROOM_NORMAL) ok = false;
    }
    if (!ok) continue;
    cell[i] = 3;
    add_room(i % GW + 1, i / GW + 1, ROOM_SUPERSECRET);
    break;
  }
  fl.boss = (uint8_t)pick_boss(stage);
  fl.rooms[fl.boss_room].boss = fl.boss;
  fl.cur = fl.start;
}

/* ---------------------------------------------------------------- doors */
static int room_at(int mx, int my) {
  if (mx < 0 || my < 0 || mx >= MAP_W || my >= MAP_H) return -1;
  return fl.map[my][mx];
}

bool room_door_exists(int d, int *kind) {
  Room *r = room_cur();
  if (fl.cur == fl.devil_room) {   /* the Devil's (or an angel's) room: its one door, back the way he came */
    if (kind) *kind = ROOM_BOSS;
    return d == ((fl.devil_side + 2) & 3);
  }
  if (r->mx == 0 && r->my == 0) return false;
  int j = room_at(r->mx + DIR_X[d], r->my + DIR_Y[d]);
  if (fl.cur == fl.boss_room && d == fl.devil_side && fl.devil_open && j < 0) {
    if (kind) *kind = fl.devil_angel ? ROOM_ANGEL : ROOM_DEVIL;
    return true;
  }
  if (j < 0) return false;
  int k = fl.rooms[j].kind;
  if (kind) *kind = k;
  /* a secret room's walls hide their doors until a bomb opens them; from inside they show */
  if ((k == ROOM_SECRET || k == ROOM_SUPERSECRET) && !(r->doors_open & (1 << d))) return false;
  if ((r->kind == ROOM_SECRET || r->kind == ROOM_SUPERSECRET) && k != ROOM_SECRET) {
    if (kind) *kind = ROOM_SECRET;
  }
  return true;
}

/* the kind of lock a door has: 0 none, 1 a key (treasure rooms and shops from the second floor), 2 two coins
 * (the arcade), 3 full health (a challenge room) */
static int door_lock(int d) {
  int k;
  if (!room_door_exists(d, &k)) return 0;
  if (room_cur()->doors_open & (1 << d)) return 0;
  if ((k == ROOM_TREASURE || k == ROOM_SHOP) && fl.stage > 1) return 1;
  if (k == ROOM_LIBRARY) return 1;
  if (k == ROOM_CHALLENGE && pl.red < pl.containers * 2 && !(room_cur()->doors_open & (1 << d))) return 3;
  return 0;
}

bool room_door_open(int d) {
  if (ents_alive_monsters()) return false;
  return door_lock(d) == 0;
}

void room_open_doors(void) {}

static void open_both(int d) {
  Room *r = room_cur();
  r->doors_open |= (uint8_t)(1 << d);
  int j = room_at(r->mx + DIR_X[d], r->my + DIR_Y[d]);
  if (j >= 0) fl.rooms[j].doors_open |= (uint8_t)(1 << ((d + 2) & 3));
}

/* he walks into a locked door: a key opens it */
bool door_try_unlock(int d) {
  if (door_lock(d) != 1) return false;
  if (pl.has[175] || fl.golden_key) {   /* (Dad's Key, a golden key: no key needed) */
    open_both(d);
    return true;
  }
  if (pl.keys == 0) return false;
  pl.keys--;
  open_both(d);
  return true;
}

/* a bomb by a wall: a secret room's door, if there's one behind */
void door_bombed(float x, float y) {
  Room *r = room_cur();
  static const float dx[4] = {160, ROOM_X, 160, ROOM_R}, dy[4] = {ROOM_B, 120, ROOM_Y, 120};
  for (int d = 0; d < 4; d++) {
    float ex = x - dx[d], ey = y - dy[d];
    if (ex * ex + ey * ey > 34 * 34) continue;
    int j = room_at(r->mx + DIR_X[d], r->my + DIR_Y[d]);
    if (j < 0) continue;
    int k = fl.rooms[j].kind;
    if ((k == ROOM_SECRET || k == ROOM_SUPERSECRET) && !(r->doors_open & (1 << d))) {
      open_both(d);
      fl.rooms[j].state |= RS_SEEN;
      fx_spawn(FX_POOF, dx[d], dy[d], 12);
    }
  }
}

/* ---------------------------------------------------------------- what a room holds */
static const uint8_t cell_grid[CELL_COUNT] = {
  [CELL_EMPTY] = CELL_EMPTY, [CELL_ROCK] = CELL_ROCK, [CELL_BLOCK] = CELL_BLOCK, [CELL_TINTED] = CELL_TINTED,
  [CELL_BOMB_ROCK] = CELL_BOMB_ROCK, [CELL_POOP] = CELL_POOP, [CELL_CORNY_POOP] = CELL_CORNY_POOP, [CELL_PIT] = CELL_PIT,
  [CELL_SPIKES] = CELL_SPIKES, [CELL_TNT] = CELL_TNT, [CELL_KEY_BLOCK] = CELL_KEY_BLOCK, [CELL_COBWEB] = CELL_COBWEB,
  [CELL_FIRE] = CELL_FIRE, [CELL_RED_FIRE] = CELL_RED_FIRE,
};

static float cell_x(int c) { return ROOM_X + c * TILE + TILE / 2; }
static float cell_y(int r) { return ROOM_Y + r * TILE + TILE / 2; }

static void room_build_grid(Room *r) {
  const RoomTemplate *t = room_get(r->tmpl);
  for (int y = 0; y < ROWS; y++)
    for (int x = 0; x < COLS; x++) {
      int c = t->cell[y][x];
      int g = c < CELL_COUNT ? cell_grid[c] : CELL_EMPTY;
      if (g == CELL_ROCK && rnd(30) == 0) g = CELL_TINTED;           /* a marked rock, now and then */
      if (g == CELL_POOP && rnd(12) == 0 && fl.stage > 2) g = CELL_RED_POOP;
      if (g == CELL_POOP && rnd(60) == 0) g = CELL_GOLDEN_POOP;
      r->grid[y][x] = (uint8_t)g;
      r->gridhp[y][x] = (g == CELL_POOP || g == CELL_CORNY_POOP || g == CELL_RED_POOP || g == CELL_GOLDEN_POOP) ? 4
                        : (g == CELL_FIRE || g == CELL_RED_FIRE) ? 4 : 0;
    }
}

/* the monsters of a room's layout, in its group */
static void room_spawn_monsters(Room *r) {
  const RoomTemplate *t = room_get(r->tmpl);
  for (int y = 0; y < ROWS; y++)
    for (int x = 0; x < COLS; x++) {
      int c = t->cell[y][x], m = -1;
      if (c >= CELL_SLOT1 && c <= CELL_SLOT4) m = groups[r->group][1 + c - CELL_SLOT1];
      else if (c >= CELL_MON) m = c - CELL_MON;
      if (m < 0 || m >= MON_COUNT) continue;
      Ent *e = mon_spawn(m, cell_x(x), cell_y(y));
      /* a champion, one time in twenty (Champion Belt: one in five) */
      if (e && !(e->flags & EF_BOSS) && rnd(pl.has[208] ? 5 : 20) == 0) {
        e->flags |= EF_CHAMPION;
        e->sub = (uint8_t)rnd(4);
        e->hp = e->maxhp = e->maxhp * (e->sub == 0 ? 2.0f : 1.5f);
      }
    }
}

static void room_spawn_first(Room *r) {
  const RoomTemplate *t = room_get(r->tmpl);
  for (int y = 0; y < ROWS; y++)
    for (int x = 0; x < COLS; x++) {
      int c = t->cell[y][x];
      if (c == CELL_PICKUP) pickup_drop_random(cell_x(x), cell_y(y));
      else if (c == CELL_CHEST) pickup_spawn(r->kind == ROOM_CURSE ? PK_RED_CHEST : rnd(5) == 0 ? PK_LOCKED_CHEST : PK_CHEST,
                                             0, cell_x(x), cell_y(y), false);
      else if (c == CELL_ITEM) {
        int pool = r->kind == ROOM_SECRET ? POOL_SECRET : r->kind == ROOM_SUPERSECRET ? (rnd(2) ? POOL_DEVIL : POOL_ANGEL)
                   : r->kind == ROOM_CURSE ? POOL_CURSE : POOL_TREASURE;
        pedestal_spawn(item_from_pool(pool), cell_x(x), cell_y(y), 0, false);
      }
    }
  float mx = 160;
  switch (r->kind) {
    case ROOM_SHOP: {
      machine_spawn(MA_SHOPKEEPER, mx, ROOM_Y + 24);
      /* Rebirth's shop: an item, then pickups at their prices */
      static const float sx[5] = {80, 120, 160, 200, 240};
      int n = fl.stage <= 2 ? 3 : 5;
      int first = n == 3 ? 1 : 0;
      for (int i = 0; i < n; i++) {
        float x = sx[first + i], y = 135;
        if (i == 0 || (n == 5 && i == 4 && rnd(3) == 0)) {
          int it = item_from_pool(POOL_SHOP);
          pedestal_spawn(it, x, y, pl.has[64] ? 8 : 15, false);
        } else {
          static const uint8_t wares[] = {PK_HEART, PK_SOUL, PK_BOMB, PK_KEY, PK_PILL, PK_CARD, PK_DOUBLE_BOMB, PK_GRAB_BAG};
          int k = wares[rnd(sizeof wares)];
          int price = k == PK_HEART ? 3 : k == PK_GRAB_BAG ? 7 : 5;
          Ent *p = pickup_spawn(k, 0, x, y, false);
          if (p) p->price = (int16_t)(pl.has[64] ? (price + 1) / 2 : price), p->flags |= EF_SHOP;
        }
      }
      break;
    }
    case ROOM_ARCADE:
      machine_spawn(MA_SLOT, 100, 90);
      machine_spawn(MA_SLOT, 220, 90);
      machine_spawn(rnd(2) ? MA_BLOOD : MA_FORTUNE, 160, 80);
      machine_spawn(rnd(3) ? MA_BEGGAR : MA_BOMB_BUM, 100, 160);
      if (rnd(2)) machine_spawn(MA_SLOT, 220, 160);
      break;
    case ROOM_SACRIFICE:
      r->grid[3][6] = CELL_SPIKES;
      break;
    case ROOM_LIBRARY:
      pedestal_spawn(item_from_pool(POOL_LIBRARY), 120, 120, 0, false);
      pedestal_spawn(item_from_pool(POOL_LIBRARY), 200, 120, 0, false);
      break;
    case ROOM_DEVIL: {
      machine_spawn(MA_DEVIL_STATUE, mx, ROOM_Y + 22);
      int n = 1 + rnd(3);
      static const float px[3] = {110, 160, 210};
      for (int i = 0; i < n; i++) {
        int it = item_from_pool(POOL_DEVIL);
        pedestal_spawn(it, n == 1 ? 160 : n == 2 ? (i ? 190 : 130) : px[i], 125, 0, true);
      }
      break;
    }
    case ROOM_ANGEL:
      machine_spawn(MA_ANGEL_STATUE, mx, ROOM_Y + 22);
      pedestal_spawn(item_from_pool(POOL_ANGEL), 160, 125, 0, false);
      break;
    default: break;
  }
}

static void room_spawn_specials(Room *r) {
  if (r->kind == ROOM_BOSS && !(r->state & RS_CLEARED)) boss_spawn(fl.boss, 160, 105);
  if (r->kind == ROOM_MINIBOSS && !(r->state & RS_CLEARED)) {
    static const uint8_t sins[] = {MON_SLOTH, MON_LUST, MON_WRATH, MON_GLUTTONY, MON_GREED, MON_ENVY, MON_PRIDE};
    if (!r->boss) r->boss = sins[rnd(sizeof sins)];
    boss_spawn(r->boss, 160, 110);
  }
  if (r->kind == ROOM_CHALLENGE && !(r->state & RS_CLEARED)) {
    /* two waves of the floor's monsters, for the chest */
    int cb = 1 << (chapter_of(fl.stage) - 1);
    int g = pick_group(r->tmpl, cb > 16 ? 16 : cb);
    for (int i = 0; i < 4; i++) {
      int m = groups[g][1 + i];
      if (m != MON_COUNT) mon_spawn(m, i < 2 ? 70 + i * 180 : 120 + (i - 2) * 80, i < 2 ? 80 : 160);
    }
    r->variant = 1;
  }
}

/* ---------------------------------------------------------------- leaving and coming back */
void room_save_things(void) {
  Room *r = room_cur();
  r->nsaved = 0;
  for (int i = 0; i < MAX_ENTS && r->nsaved < ROOM_SAVED; i++) {
    Ent *e = &ents[i];
    if (e->kind != EK_PICKUP && e->kind != EK_PEDESTAL && e->kind != EK_MACHINE && e->kind != EK_TRAPDOOR) continue;
    if (e->kind == EK_MACHINE && e->state == 255) continue;   /* (blown up) */
    Saved *s = &r->saved[r->nsaved++];
    s->kind = e->kind, s->type = e->type, s->sub = e->sub, s->flags = 0;
    s->x = (int16_t)e->x, s->y = (int16_t)e->y;
    if (e->kind == EK_PICKUP) s->val = (uint16_t)e->price, s->flags = (e->flags & EF_SHOP) ? 1 : 0;
    if (e->kind == EK_PEDESTAL) s->val = (uint16_t)e->t3, s->sub = (uint8_t)e->price, s->flags = (e->flags & EF_DEVIL) ? 1 : 0;
    if (e->kind == EK_MACHINE) s->flags = e->state;
  }
}

static void room_restore_things(Room *r) {
  for (int i = 0; i < r->nsaved; i++) {
    Saved *s = &r->saved[i];
    if (s->kind == EK_PICKUP) {
      Ent *e = pickup_spawn(s->type, s->sub, s->x, s->y, false);
      if (e && (s->flags & 1)) e->price = (int16_t)s->val, e->flags |= EF_SHOP;
    } else if (s->kind == EK_PEDESTAL) {
      pedestal_spawn(s->val, s->x, s->y, s->sub, s->flags & 1);
    } else if (s->kind == EK_MACHINE) {
      Ent *e = machine_spawn(s->type, s->x, s->y);
      if (e) e->sub = s->sub, e->state = s->flags;
    } else if (s->kind == EK_TRAPDOOR) {
      Ent *e = ent_new(EK_TRAPDOOR, s->x, s->y);
      if (e) e->type = s->type, e->spr = SPR_TRAPDOOR;
    }
  }
}

void floor_enter_room(int index, int from_dir) {
  if (index < 0 || index >= fl.nrooms) return;
  Room *old = room_cur();
  if (g_mode == MODE_PLAY && index != fl.cur) room_save_things();
  (void)old;
  fl.cur = (int8_t)index;
  Room *r = room_cur();
  ents_clear();
  tears_clear();
  bool first = !(r->state & RS_VISITED);
  r->state |= RS_VISITED | RS_SEEN;
  /* the rooms next door show on the map */
  for (int d = 0; d < 4; d++) {
    int j = room_at(r->mx + DIR_X[d], r->my + DIR_Y[d]);
    if (j >= 0 && fl.rooms[j].kind != ROOM_SECRET && fl.rooms[j].kind != ROOM_SUPERSECRET) fl.rooms[j].state |= RS_SEEN;
    /* the Spelunker Hat: what the rooms next door are, and the rooms past them */
    if (j >= 0 && pl.has[91]) {
      fl.rooms[j].state |= RS_SEEN | RS_FOUND;
      for (int e = 0; e < 4; e++) {
        int k = room_at(fl.rooms[j].mx + DIR_X[e], fl.rooms[j].my + DIR_Y[e]);
        if (k >= 0) fl.rooms[k].state |= RS_SEEN;
      }
    }
  }
  if (first) {
    room_build_grid(r);
    room_spawn_first(r);
  } else
    room_restore_things(r);
  if (!(r->state & RS_CLEARED)) {
    room_spawn_monsters(r);
    room_spawn_specials(r);
    if (!ents_alive_monsters()) r->state |= RS_CLEARED;
  }
  g_room_was_cleared = r->state & RS_CLEARED;
  /* where he comes in */
  static const float ex[5] = {160, ROOM_X + 14, 160, ROOM_R - 14, 160}, ey[5] = {ROOM_B - 12, 120, ROOM_Y + 14, 120, 140};
  int d = from_dir < 0 || from_dir > 4 ? 4 : from_dir;
  pl.x = ex[d], pl.y = ey[d], pl.vx = pl.vy = 0;
  if (r->kind == ROOM_START && d == 4) pl.x = 160, pl.y = 120;
  if (d == 4 && r->kind != ROOM_START) pl.y = ROOM_B - 14;   /* (sent here: by the bottom wall, clear of what's in the room) */
  familiars_spawn();
  g_room_frames = 0;
  /* per-room effects end */
  pl.belial = pl.bible = pl.shadows = pl.unicorn = pl.hourglass = pl.telepathy = pl.nail = pl.pony = 0;
  pl.room_damage = 0, pl.lust = 0;
  player_stats();
  /* the Curse of the Lost hides the map; Darkness, Unknown and Blind are drawn */
  if (fl.curse & CURSE_LOST)
    for (int i = 0; i < fl.nrooms; i++)
      if (i != fl.cur) fl.rooms[i].state &= (uint8_t)~RS_SEEN;
  /* a curse room hurts as he comes in (unless he flies) */
  if (r->kind == ROOM_CURSE && !pl.flight && d != 4) player_hurt(1, -3);
}

/* Teleport!, the Joker...: into another room, its middle */
void room_teleport(int index) {
  room_save_things();
  floor_enter_room(index, DIR_NONE);
}

void floor_open_devil(void) {
  if (fl.devil_room >= 0) return;
  int i = add_room(-1, -1, fl.devil_angel ? ROOM_ANGEL : ROOM_DEVIL);
  fl.devil_room = (int8_t)i;
}

void floor_reveal(int what) {
  for (int i = 0; i < fl.nrooms; i++) {
    Room *r = &fl.rooms[i];
    bool secret = r->kind == ROOM_SECRET || r->kind == ROOM_SUPERSECRET;
    if ((what & 1) && !secret) r->state |= RS_SEEN | RS_FOUND;
    if ((what & 2) && r->kind != ROOM_NORMAL && !secret) r->state |= RS_SEEN | RS_FOUND;
    if ((what & 4) && secret) r->state |= RS_SEEN | RS_FOUND;
  }
}

/* entering a door: through to the next room */
int floor_neighbour(int d) {
  Room *r = room_cur();
  if (fl.cur == fl.devil_room) return fl.boss_room;
  if (fl.cur == fl.boss_room && d == fl.devil_side && fl.devil_open && room_at(r->mx + DIR_X[d], r->my + DIR_Y[d]) < 0) {
    floor_open_devil();
    return fl.devil_room;
  }
  return room_at(r->mx + DIR_X[d], r->my + DIR_Y[d]);
}
