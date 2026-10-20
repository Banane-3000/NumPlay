/* The items, by Rebirth's numbers and names, the text the game shows when
 * one is picked up, the rooms each comes from (its pools) and what it does;
 * the active items' charges; pills, cards and familiars. */
#include <string.h>
#include "game.h"
#include "gfx.h"
#include "res.h"

enum { T = 1 << POOL_TREASURE, S = 1 << POOL_SHOP, BO = 1 << POOL_BOSS, D = 1 << POOL_DEVIL, A = 1 << POOL_ANGEL,
       X = 1 << POOL_SECRET, L = 1 << POOL_LIBRARY, C = 1 << POOL_CURSE };

/* id, name, pickup text, pools, charge (0: passive, 255: once), quality (0 to 4) */
static const ItemInfo items[] = {
  {1, "THE SAD ONION", "TEARS UP", T, 0, 3},
  {2, "THE INNER EYE", "TRIPLE SHOT", T, 0, 2},
  {3, "SPOON BENDER", "PSYCHIC SHOT", T | X, 0, 3},
  {4, "CRICKET'S HEAD", "DMG UP", T | X, 0, 4},
  {5, "MY REFLECTION", "BOOMERANG TEARS", T, 0, 1},
  {6, "NUMBER ONE", "TEARS UP", T, 0, 1},
  {7, "BLOOD OF THE MARTYR", "DMG UP", T, 0, 3},
  {8, "BROTHER BOBBY", "FRIENDS 'TILL THE END", T | A, 0, 1},
  {10, "HALO OF FLIES", "PROJECTILE PROTECTION", T, 0, 2},
  {11, "1UP!", "EXTRA LIFE", T | X, 0, 2},
  {12, "MAGIC MUSHROOM", "ALL STATS UP!", T, 0, 4},
  {13, "THE VIRUS", "POISON TOUCH", T, 0, 1},
  {14, "ROID RAGE", "SPEED AND RANGE UP", T, 0, 1},
  {15, "<3", "HP UP", T | BO, 0, 1},
  {16, "RAW LIVER", "HP UP", BO, 0, 1},
  {17, "SKELETON KEY", "99 KEYS", T | X, 0, 2},
  {18, "A DOLLAR", "$$$", T | X, 0, 2},
  {19, "BOOM!", "10 BOMBS", T, 0, 1},
  {20, "TRANSCENDENCE", "WE ALL FLOAT DOWN HERE...", T, 0, 2},
  {21, "THE COMPASS", "REVEALS ICONS", T | S, 0, 1},
  {22, "LUNCH", "HP UP", BO, 0, 0},
  {23, "DINNER", "HP UP", BO, 0, 0},
  {24, "DESSERT", "HP UP", BO, 0, 0},
  {25, "BREAKFAST", "HP UP", BO, 0, 0},
  {26, "ROTTEN MEAT", "HP UP", BO, 0, 0},
  {27, "WOODEN SPOON", "SPEED UP", BO, 0, 1},
  {28, "THE BELT", "SPEED UP", BO, 0, 1},
  {29, "MOM'S UNDERWEAR", "RANGE UP", BO, 0, 1},
  {30, "MOM'S HEELS", "RANGE UP", BO, 0, 1},
  {31, "MOM'S LIPSTICK", "RANGE UP", BO, 0, 1},
  {32, "WIRE COAT HANGER", "TEARS UP", T, 0, 2},
  {33, "THE BIBLE", "TEMPORARY FLIGHT", L | A | S, 4, 2},
  {34, "THE BOOK OF BELIAL", "TEMPORARY DMG UP", L | D, 3, 2},
  {35, "THE NECRONOMICON", "MEGA BLAST", L | D, 3, 2},
  {36, "THE POOP", "PLOP!", T, 1, 0},
  {37, "MR. BOOM", "REUSABLE BOMBS", T, 2, 1},
  {38, "TAMMY'S HEAD", "REUSABLE TEAR BURST", T, 1, 2},
  {39, "MOM'S BRA", "MASS PETRIFY", T, 3, 1},
  {41, "MOM'S PAD", "MASS FEAR", T, 3, 0},
  {42, "BOB'S ROTTEN HEAD", "REUSABLE RANGED BOMB", T, 3, 1},
  {44, "TELEPORT!", "TELEPORT", T | S, 2, 1},
  {45, "YUM HEART", "REUSABLE REGENERATION", T | S, 4, 2},
  {46, "LUCKY FOOT", "LUCK UP", T | S, 0, 2},
  {47, "DOCTOR'S REMOTE", "AIR STRIKE", T, 2, 1},
  {48, "CUPID'S ARROW", "PIERCING SHOTS", T, 0, 2},
  {49, "SHOOP DA WHOOP!", "BLLLARRRRGGG!", T, 2, 2},
  {50, "STEVEN", "DMG UP", BO, 0, 3},
  {51, "PENTAGRAM", "DMG UP", T | D, 0, 3},
  {52, "DR. FETUS", "WHAT DOES IT MEAN?", T, 0, 3},
  {53, "MAGNETO", "ITEM SNATCHER", T | S, 0, 1},
  {54, "TREASURE MAP", "FULL MAP", T | S, 0, 1},
  {55, "MOM'S EYE", "EYE IN THE BACK OF YOUR HEAD", T, 0, 1},
  {56, "LEMON MISHAP", "OOPS...", T, 2, 0},
  {57, "DISTANT ADMIRATION", "ATTACK FLY", T, 0, 1},
  {58, "BOOK OF SHADOWS", "TEMPORARY INVINCIBILITY", L | A | T, 3, 2},
  {60, "THE LADDER", "BUILDING BRIDGES", T | S, 0, 1},
  {62, "CHARM OF THE VAMPIRE", "KILLS HEAL", T, 0, 2},
  {63, "THE BATTERY", "STORES ENERGY", S, 0, 2},
  {64, "STEAM SALE", "50% OFF", S, 0, 2},
  {65, "ANARCHIST COOKBOOK", "SUMMON BOMBS", T, 3, 1},
  {66, "THE HOURGLASS", "TEMPORARY ENEMY SLOWDOWN", T, 2, 1},
  {67, "SISTER MAGGY", "FRIENDS 'TILL THE END", T, 0, 1},
  {68, "TECHNOLOGY", "LASER TEARS", T, 0, 3},
  {69, "CHOCOLATE MILK", "CHARGED TEARS", T, 0, 3},
  {70, "GROWTH HORMONES", "SPEED + DMG UP", BO, 0, 2},
  {71, "MINI MUSH", "SPEED + RANGE UP", T, 0, 1},
  {72, "ROSARY", "FAITH UP", A | T, 0, 2},
  {73, "CUBE OF MEAT", "GOTTA MEAT 'EM ALL", T, 0, 2},
  {74, "A QUARTER", "+25 COINS", T | X, 0, 1},
  {75, "PHD", "BETTER PILLS", T | S, 0, 2},
  {76, "X-RAY VISION", "I CAN SEE FOREVER!", T | S, 0, 1},
  {77, "MY LITTLE UNICORN", "TEMPORARY BADASS", T, 6, 2},
  {78, "BOOK OF REVELATIONS", "REUSABLE SOUL PROTECTION", L | A, 6, 2},
  {79, "THE MARK", "DMG UP + SPEED UP", D, 0, 3},
  {80, "THE PACT", "DMG + TEARS UP", D, 0, 3},
  {81, "DEAD CAT", "9 LIVES", D | T, 0, 2},
  {82, "LORD OF THE PIT", "DEMON WINGS", D, 0, 3},
  {83, "THE NAIL", "TEMPORARY DEMON FORM", D, 6, 1},
  {84, "WE NEED TO GO DEEPER!", "REUSABLE TRAPDOOR", T | S, 6, 2},
  {85, "DECK OF CARDS", "REUSABLE CARD GENERATOR", T | S, 6, 2},
  {86, "MONSTRO'S TOOTH", "SUMMON MONSTRO", T, 3, 1},
  {87, "LOKI'S HORNS", "CROSS TEARS", T, 0, 1},
  {88, "LITTLE CHUBBY", "ATTACK BUDDY", T, 0, 2},
  {89, "SPIDER BITE", "SLOW EFFECT", T, 0, 1},
  {90, "THE SMALL ROCK", "DMG UP", X, 0, 2},
  {91, "SPELUNKER HAT", "SEE THROUGH WALLS", T | S, 0, 1},
  {92, "SUPER BANDAGE", "+2 SOUL HEARTS", BO, 0, 2},
  {94, "SACK OF PENNIES", "GIVES MONEY", T, 0, 1},
  {95, "ROBO-BABY", "FRIENDS 'TILL THE END", T, 0, 1},
  {96, "LITTLE C.H.A.D.", "GIVES KISSES", T, 0, 1},
  {97, "THE BOOK OF SIN", "REUSABLE ITEM GENERATOR", L | D, 4, 2},
  {98, "THE RELIC", "SOUL GENERATOR", A, 0, 2},
  {99, "LITTLE GISH", "STICKY FRIEND", T, 0, 1},
  {100, "LITTLE STEVEN", "PSYCHIC FRIEND", T, 0, 1},
  {101, "THE HALO", "ALL STATS UP", A, 0, 3},
  {102, "MOM'S BOTTLE OF PILLS", "REUSABLE PILL GENERATOR", T | S, 6, 1},
  {103, "THE COMMON COLD", "POISON DAMAGE", T, 0, 1},
  {104, "THE PARASITE", "SPLIT SHOT", T, 0, 2},
  {105, "THE D6", "REROLL YOUR DESTINY", T | X, 6, 4},
  {106, "MR. MEGA", "BLAST DAMAGE", T, 0, 2},
  {108, "THE WAFER", "DAMAGE RESISTANCE", T | A, 0, 3},
  {109, "MONEY = POWER", "$ = DMG", T | S, 0, 1},
  {110, "MOM'S CONTACTS", "FREEZE EFFECT", T, 0, 1},
  {111, "THE BEAN", "TOOT ON COMMAND", T, 1, 0},
  {112, "GUARDIAN ANGEL", "ANGEL SHIELD", A, 0, 2},
  {113, "DEMON BABY", "AUTO TURRET", D | T, 0, 2},
  {114, "MOM'S KNIFE", "STAB STAB STAB", D | T, 0, 4},
  {115, "OUIJA BOARD", "SPECTRAL TEARS", T, 0, 1},
  {117, "DEAD BIRD", "PROTECTIVE FRIEND", T, 0, 1},
  {118, "BRIMSTONE", "BLOOD LASER BARRAGE", D, 0, 4},
  {119, "BLOOD BAG", "HP UP", T, 0, 1},
  {120, "ODD MUSHROOM", "SPEED + TEARS UP", T, 0, 1},
  {121, "ODD MUSHROOM", "DMG + RANGE + HP UP", T, 0, 2},
  {122, "WHORE OF BABYLON", "CURSE UP", D, 0, 2},
  {123, "MONSTER MANUAL", "TEMPORARY BUDDY", T | L, 3, 1},
  {124, "DEAD SEA SCROLLS", "IT'S A MYSTERY", T | L, 2, 1},
  {126, "RAZOR BLADE", "FEEL MY PAIN", D, 1, 1},
  {127, "FORGET ME NOW", "REROLL YOUR FLOOR", X | D, 255, 1},
  {128, "FOREVER ALONE", "ATTACK FLY", T, 0, 1},
  {129, "BUCKET OF LARD", "HP UP", BO, 0, 1},
  {130, "A PONY", "FLIGHT + DASH ATTACK", T, 2, 2},
  {131, "BOMB BAG", "GIVES BOMBS", T, 0, 1},
  {132, "A LUMP OF COAL", "MY XMAS PRESENT", T, 0, 2},
  {133, "GUPPY'S PAW", "SOUL CONVERTER", D, 1, 2},
  {134, "GUPPY'S TAIL", "CURSED?", D, 0, 1},
  {135, "IV BAG", "PORTABLE BLOOD BANK", S, 254, 1},
  {136, "BEST FRIEND", "REUSABLE DECOY", T | S, 3, 0},
  {137, "REMOTE DETONATOR", "REMOTE BOMB DETONATION", T | S, 1, 1},
  {138, "STIGMATA", "DMG + HP UP", T, 0, 2},
  {140, "BOB'S CURSE", "POISON BOMBS", T, 0, 1},
  {142, "SCAPULAR", "PROTECTION", A, 0, 2},
  {143, "SPEED BALL", "SPEED + SHOT SPEED UP", T, 0, 1},
  {144, "BUM FRIEND", "HE'S GREEDY", T | S, 0, 1},
  {145, "GUPPY'S HEAD", "REUSABLE FLY HIVE", D, 1, 2},
  {146, "PRAYER CARD", "REUSABLE ETERNITY", A, 6, 2},
  {147, "NOTCHED AXE", "ROCKS DON'T STAND A CHANCE", S | T, 0, 1},
  {149, "IPECAC", "EXPLOSIVE SHOTS", T, 0, 3},
  {150, "TOUGH LOVE", "TOOTH SHOT", T, 0, 2},
  {152, "TECHNOLOGY 2", "LASER", T, 0, 2},
  {153, "MUTANT SPIDER", "QUAD SHOT", T, 0, 3},
  {154, "CHEMICAL PEEL", "DMG UP", T, 0, 1},
  {155, "THE PEEPER", "PLOP!", T, 0, 1},
  {157, "BLOODY LUST", "RAGE", T, 0, 2},
  {158, "CRYSTAL BALL", "FORTUNE TELLER", S, 6, 1},
  {159, "SPIRIT OF THE NIGHT", "SCARY", D, 0, 2},
  {160, "CRACK THE SKY", "HOLY WHITE DEATH", A, 4, 2},
  {161, "ANKH", "ETERNAL LIFE?", T | X, 0, 2},
  {162, "CELTIC CROSS", "YOU FEEL BLESSED", T, 0, 1},
  {163, "GHOST BABY", "SPECTRAL BUDDY", T, 0, 1},
  {164, "THE CANDLE", "BURN", S, 2, 1},
  {165, "CAT-O-NINE-TAILS", "SHOT SPEED + DMG UP", T, 0, 1},
  {166, "D20", "REROLL THE BASICS", S, 6, 2},
  {167, "HARLEQUIN BABY", "DOUBLE SPREAD BUDDY", T, 0, 2},
  {169, "POLYPHEMUS", "MEGA TEARS", T, 0, 3},
  {170, "DADDY LONGLEGS", "DADDY'S LOVE", T, 3, 1},
  {171, "SPIDER BUTT", "BUG ZAPPER", T, 2, 1},
  {173, "MITRE", "BLESSING UP", A, 0, 1},
  {174, "RAINBOW BABY", "RANDOM BUDDY", T, 0, 1},
  {175, "DAD'S KEY", "OPEN SESAME", S, 2, 1},
  {176, "STEM CELLS", "HP UP + SHOT SPEED UP", BO, 0, 1},
  {177, "PORTABLE SLOT", "GAMBLE 24/7", S, 254, 1},
  {178, "HOLY WATER", "SPLASH!", A, 0, 1},
  {179, "FATE", "FLIGHT ETERNAL", A, 0, 3},
  {182, "SACRED HEART", "HOMING SHOTS + DMG UP", A, 0, 4},
  {183, "TOOTH PICKS", "TEARS + SHOT SPEED UP", T, 0, 1},
  {184, "HOLY GRAIL", "FLIGHT + HP UP", A, 0, 3},
  {185, "DEAD DOVE", "FLIGHT + SPECTRAL TEARS", A, 0, 2},
  {186, "BLOOD RIGHTS", "BLOODY RAGE", D, 3, 1},
  {188, "ABEL", "MIRRORED BUDDY", T, 0, 1},
  {189, "SMB SUPER FAN", "ALL STATS UP", T, 0, 3},
  {190, "PYRO", "99 BOMBS", T | X, 0, 2},
  {192, "TELEPATHY FOR DUMMIES", "TEMPORARY PSYCHIC", T | L, 2, 1},
  {193, "MEAT!", "DMG + HP UP", BO, 0, 2},
  {194, "MAGIC 8 BALL", "SHOT SPEED UP", T, 0, 1},
  {196, "SQUEEZY", "SOUL HEARTS", T, 0, 1},
  {197, "JESUS JUICE", "DMG + RANGE UP", T, 0, 2},
  {199, "MOM'S KEY", "ITEM DROP UP + 2 KEYS", T, 0, 1},
  {201, "IRON BAR", "DMG UP + CONCUSSIVE TEARS", T, 0, 2},
  {245, "20/20", "DOUBLE SHOT", T, 0, 3},
  {261, "PROPTOSIS", "SHORT RANGE MEGA TEARS", T, 0, 3},
};
#define NITEMS (int)(sizeof items / sizeof items[0])

static const uint16_t sprite_ids[ITEM_SPRITE_COUNT] = ITEM_ID_TABLE;

const ItemInfo *item_info(int id) {
  for (int i = 0; i < NITEMS; i++)
    if (items[i].id == id) return &items[i];
  return NULL;
}
int item_sprite(int id) {
  for (int i = 0; i < ITEM_SPRITE_COUNT; i++)
    if (sprite_ids[i] == id) return SPR_ITEM_1 + i;
  return SPR_ITEM_1;
}
bool item_is_active(int id) {
  const ItemInfo *ii = item_info(id);
  return ii && ii->charge;
}

/* ---------------------------------------------------------------- pools: each item once a run */
uint8_t item_taken[ITEM_MAX];
#define taken item_taken
void items_new_run(void) { memset(taken, 0, sizeof taken); }

int item_from_pool(int pool) {
  int cand[NITEMS], n = 0;
  for (int i = 0; i < NITEMS; i++)
    if ((items[i].pools & (1 << pool)) && !taken[items[i].id] && !pl.has[items[i].id]) cand[n++] = i;
  if (!n)   /* the pool emptied: the treasure room's then, or Breakfast, as the game does */
    for (int i = 0; i < NITEMS; i++)
      if ((items[i].pools & T) && !taken[items[i].id] && !pl.has[items[i].id]) cand[n++] = i;
  if (!n) return 25;
  int id = items[cand[rnd(n)]].id;
  taken[id] = 1;
  return id;
}

int item_count_in_run(void) { return pl.norder; }

/* ---------------------------------------------------------------- picking one up */
static bool is_familiar(int id) {
  switch (id) {
    case 8: case 10: case 57: case 67: case 73: case 88: case 94: case 95: case 96: case 98: case 99: case 100: case 112:
    case 113: case 128: case 131: case 144: case 155: case 163: case 167: case 174: case 188:
      return true;
    default: return false;
  }
}

void item_give(int id) {
  const ItemInfo *ii = item_info(id);
  if (!ii) return;
  if (pl.has[id] < 255) pl.has[id]++;
  if (pl.norder < 64) pl.order[pl.norder++] = (uint8_t)(id < 256 ? id : 0);
  taken[id] = 1;
  hud_banner(ii->name, ii->text);
  if (ii->charge) {
    pl.active = (int16_t)id;
    pl.active_charge = ii->charge >= 254 ? (ii->charge == 255) : ii->charge;
  }
  switch (id) {
    case 11: pl.lives++; break;
    case 81: pl.lives += 9; pl.containers = 1; pl.red = 2; if (pl.soul > 2) pl.soul = 2; break;
    case 161: pl.lives++; break;
    case 12: case 15: case 22: case 23: case 24: case 25: case 26: case 121: case 138: case 176: case 184: case 193:
      player_add_containers(1), player_heal(id == 15 || id == 12 ? 24 : 2);
      break;
    case 101: player_add_containers(1), player_heal(2); break;
    case 182: player_add_containers(1), player_heal(24); break;
    case 16: player_add_containers(2), player_heal(4); break;
    case 92: player_add_containers(1), player_heal(2), player_add_soul(4, false); break;
    case 119: player_add_containers(1), player_heal(10); break;
    case 129: player_add_containers(2), player_heal(1); break;
    case 72: player_add_soul(6, false); break;
    case 80: player_add_soul(4, false); break;
    case 79: player_add_soul(2, true); break;
    case 196: player_add_soul(4, false); break;
    case 173: player_add_soul(2, false); break;
    case 179: pl.eternal = 1; break;
    case 17: pl.keys = 99; break;
    case 18: pl.coins = 99; break;
    case 74: pl.coins = (uint8_t)(pl.coins + 25 > 99 ? 99 : pl.coins + 25); break;
    case 19: pl.bombs = (uint8_t)(pl.bombs + 10 > 99 ? 99 : pl.bombs + 10); break;
    case 190: pl.bombs = 99; break;
    case 106: case 140: pl.bombs = (uint8_t)(pl.bombs + 5 > 99 ? 99 : pl.bombs + 5); break;
    case 199: pl.keys = (uint8_t)(pl.keys + 2 > 99 ? 99 : pl.keys + 2); break;
    case 75: player_heal(4); break;
    case 21: floor_reveal(2); break;
    case 54: floor_reveal(1); break;
    case 76: floor_reveal(4); break;
    case 194: pickup_spawn(PK_CARD, 0, pl.x, pl.y + 14, true); break;
    case 116: break;
    default: break;
  }
  if (is_familiar(id)) familiars_spawn();
  player_stats();
}

/* ---------------------------------------------------------------- active items */
static int last_dir(void) { return pl.shoot_dir != DIR_NONE ? pl.shoot_dir : pl.face; }

static void hurt_all(float dmg) {
  for (int i = 0; i < MAX_ENTS; i++)
    if (ents[i].kind == EK_MON && !(ents[i].flags & EF_FRIEND)) ent_damage(&ents[i], dmg, 0, 0, 0);
}

static void blue_flies(int n) {
  for (int i = 0; i < n; i++) mon_spawn(MON_BLUE_FLY, pl.x + rnd(10) - 5, pl.y - 6);
}

static void random_room_teleport(int kind) {
  int cand[MAX_ROOMS], n = 0;
  for (int i = 0; i < fl.nrooms; i++)
    if (i != fl.cur && (kind < 0 ? fl.rooms[i].kind != ROOM_SUPERSECRET && fl.rooms[i].kind != ROOM_SECRET && i != fl.devil_room : fl.rooms[i].kind == kind)) cand[n++] = i;
  if (n) room_teleport(cand[rnd(n)]);
}

static bool use_effect(int id) {
  switch (id) {
    case 33:   /* The Bible: flight; and Mom or her heart dies of it */
      pl.bible = 1;
      for (int i = 0; i < MAX_ENTS; i++)
        if (ents[i].kind == EK_MON && (ents[i].type == MON_MOM || ents[i].type == MON_MOMS_HEART) && !ents[i].sub)
          ent_damage(&ents[i], 99999, 0, 0, 0);
      break;
    case 34: pl.belial = 1; break;
    case 35: hurt_all(40); g_shake = 10; break;
    case 36: {
      int c = (int)((pl.x - ROOM_X) / TILE), r = (int)((pl.y - ROOM_Y) / TILE);
      Room *rm = room_cur();
      if (c >= 0 && r >= 0 && c < COLS && r < ROWS && rm->grid[r][c] == CELL_EMPTY) rm->grid[r][c] = CELL_POOP, rm->gridhp[r][c] = 4;
      else return false;
      break;
    }
    case 37: bomb_place(pl.x, pl.y, 2); break;
    case 38: {
      float dmg = pl.damage + 25;
      for (int i = 0; i < 10; i++) {
        float a = i * 0.6283f;
        tear_fire(pl.x, pl.y, fcos(a) * 5, fsin(a) * 5, dmg, pl.tear_flags);
      }
      break;
    }
    case 39: for (int i = 0; i < MAX_ENTS; i++) if (ents[i].kind == EK_MON) ents[i].freeze = 120; break;
    case 41: for (int i = 0; i < MAX_ENTS; i++) if (ents[i].kind == EK_MON) ents[i].fear = 150; break;
    case 42: {
      int d = last_dir();
      explode(pl.x + DIR_X[d] * 60, pl.y + DIR_Y[d] * 50, 36, 70, false, 2);
      break;
    }
    case 44: random_room_teleport(-1); break;
    case 45: player_heal(2); break;
    case 47: {
      Ent *e = ent_nearest_monster(pl.x, pl.y, 999);
      if (e) explode(e->x, e->y, 40, 50, false, 0);
      else return false;
      break;
    }
    case 49: laser_fire(pl.x, pl.y - 10, last_dir(), 16, pl.damage * 0.9f, 20, 3, false); break;
    case 56: creep_spawn(pl.x, pl.y, 26, 300, false, 2); break;
    case 58: pl.shadows = 300; break;
    case 65:
      for (int i = 0; i < 6; i++) {
        float a = i * 1.047f;
        bomb_place(clampf(pl.x + fcos(a) * 45, ROOM_X + 8, ROOM_R - 8), clampf(pl.y + fsin(a) * 35, ROOM_Y + 8, ROOM_B - 8), 1);
      }
      break;
    case 66: pl.hourglass = 240; break;
    case 77: pl.unicorn = 180; player_stats(); break;
    case 78: player_add_soul(2, false); break;
    case 83: pl.nail = 1, player_add_soul(2, true), pl.room_damage += 0.7f; player_stats(); break;
    case 84: {
      Ent *t = ent_new(EK_TRAPDOOR, pl.x, pl.y + 2);
      if (t) t->spr = SPR_TRAPDOOR, t->t = 0;
      break;
    }
    case 85: pickup_spawn(PK_CARD, 0, pl.x, pl.y + 12, true); break;
    case 86: {
      Ent *e = ent_nearest_monster(pl.x, pl.y, 999);
      if (!e) return false;
      fx_spawn(FX_SHOCKWAVE, e->x, e->y, 14);
      g_shake = 10;
      for (int i = 0; i < MAX_ENTS; i++)
        if (ents[i].kind == EK_MON && dist2(ents[i].x, ents[i].y, e->x, e->y) < 40 * 40) ent_damage(&ents[i], 120, 0, 0, 0);
      break;
    }
    case 97: {
      static const uint8_t k[] = {PK_HEART, PK_PENNY, PK_BOMB, PK_KEY, PK_PILL, PK_CARD, PK_SOUL, PK_CHEST};
      pickup_spawn(k[rnd(sizeof k)], 0, pl.x, pl.y + 12, true);
      break;
    }
    case 102: pickup_spawn(PK_PILL, 0, pl.x, pl.y + 12, true); break;
    case 105: item_reroll_room(); break;
    case 111:
      for (int i = 0; i < MAX_ENTS; i++) {
        Ent *e = &ents[i];
        if (e->kind != EK_MON || dist2(e->x, e->y, pl.x, pl.y) > 50 * 50) continue;
        e->poison = 60, e->poison_dmg = 3;
        float dx = e->x - pl.x, dy = e->y - pl.y, d = fsqrt(dx * dx + dy * dy) + 0.1f;
        e->vx += dx / d * 4, e->vy += dy / d * 4;
      }
      fx_spawn(FX_POOF, pl.x, pl.y, 12);
      break;
    case 123: blue_flies(3); break;
    case 124: {
      static const int16_t eff[] = {35, 39, 41, 45, 58, 65, 66, 77, 111};
      return use_effect(eff[rnd(sizeof eff / sizeof eff[0])]);
    }
    case 126: if (!player_hurt(2, -8)) return false; pl.room_damage += 1.2f; player_stats(); break;
    case 127: floor_new(fl.stage); floor_enter_room(fl.start, DIR_NONE); break;
    case 130: pl.pony = 10; break;
    case 133:
      if (!pl.containers) return false;
      player_add_containers(-1), player_add_soul(6, false);
      break;
    case 135:
      if (!player_hurt(1, -5)) return false;
      pl.coins = (uint8_t)(pl.coins + 1 + rnd(2) > 99 ? 99 : pl.coins + 1 + rnd(2));
      break;
    case 136: bomb_place(pl.x, pl.y, 0); break;
    case 137: for (int i = 0; i < MAX_ENTS; i++) if (ents[i].kind == EK_BOMB) ents[i].t = 1; break;
    case 145: blue_flies(2 + rnd(3)); break;
    case 146: pl.eternal = 1; break;
    case 158: floor_reveal(7); player_add_soul(2, false); break;
    case 160: {
      for (int k = 0; k < 4; k++) {
        Ent *e = NULL;
        for (int t = 0; t < 10 && !e; t++) {
          Ent *c = &ents[rnd(MAX_ENTS)];
          if (c->kind == EK_MON && !(c->flags & EF_FRIEND)) e = c;
        }
        float x = e ? e->x : ROOM_X + 20 + rnd(220), y = e ? e->y : ROOM_Y + 20 + rnd(100);
        laser_fire(x, y, -1, 14, pl.damage * 0.5f + 1, 25, 2, false);
      }
      break;
    }
    case 164: {
      int d = last_dir();
      Tear *t = tear_fire(pl.x, pl.y, DIR_X[d] * 4, DIR_Y[d] * 4, 23, TF_PIERCE | TF_SPECTRAL);
      if (t) t->size = 1.8f, t->flags |= TF_BLOOD;
      break;
    }
    case 166:   /* D20: the room's pickups rolled again */
      for (int i = 0; i < MAX_ENTS; i++)
        if (ents[i].kind == EK_PICKUP && !(ents[i].flags & EF_SHOP)) {
          float x = ents[i].x, y = ents[i].y;
          ents[i].kind = EK_NONE;
          pickup_spawn(pickup_random_type(), 0, x, y, false);
        }
      break;
    case 170: {
      Ent *e = ent_nearest_monster(pl.x, pl.y, 999);
      if (e) ent_damage(e, 40, 0, 0, 0), fx_spawn(FX_SHOCKWAVE, e->x, e->y, 12);
      break;
    }
    case 171: for (int i = 0; i < MAX_ENTS; i++) if (ents[i].kind == EK_MON) ent_damage(&ents[i], 10, 0, 0, 0), ents[i].slow = 120; break;
    case 175: {
      Room *r = room_cur();
      for (int d = 0; d < 4; d++) r->doors_open |= (uint8_t)(1 << d);
      break;
    }
    case 177: {
      if (!pl.coins) return false;
      pl.coins--;
      int k = rnd(100);
      if (k < 15) pickup_spawn(PK_PENNY, 0, pl.x, pl.y + 12, true), pickup_spawn(PK_PENNY, 0, pl.x, pl.y + 12, true);
      else if (k < 22) pickup_spawn(PK_HEART, 0, pl.x, pl.y + 12, true);
      else if (k < 27) pickup_spawn(PK_BOMB, 0, pl.x, pl.y + 12, true);
      else if (k < 32) pickup_spawn(PK_KEY, 0, pl.x, pl.y + 12, true);
      else if (k < 34) mon_spawn(MON_FLY, pl.x, pl.y + 20);
      break;
    }
    case 186: if (!player_hurt(2, -8)) return false; hurt_all(40); break;
    case 192: pl.telepathy = 1; player_stats(); break;
    default: return false;
  }
  return true;
}

void item_use_active(void) {
  const ItemInfo *ii = item_info(pl.active);
  if (!ii) return;
  int need = ii->charge == 255 ? 1 : ii->charge == 254 ? 0 : ii->charge;
  if (pl.active_charge < need && ii->charge != 0) return;
  if (!use_effect(pl.active)) return;
  if (ii->charge == 255) {   /* used once: gone */
    pl.active = 0, pl.active_charge = 0;
    return;
  }
  pl.active_charge = (int16_t)(pl.active_charge - need);
  if (pl.active_charge < 0) pl.active_charge = 0;
  player_stats();
}

/* a room cleared: a charge, and the familiars that bring things */
void item_room_cleared(void) {
  const ItemInfo *ii = item_info(pl.active);
  if (ii && ii->charge && ii->charge < 254) {
    int max = ii->charge * (pl.has[63] ? 2 : 1);
    pl.active_charge = (int16_t)(pl.active_charge + 1 + (pl.has[116] ? 1 : 0));
    if (pl.active_charge > max) pl.active_charge = (int16_t)max;
  }
  static uint8_t rooms;
  rooms++;
  if (pl.has[94] && rooms % 2 == 0) pickup_spawn(PK_PENNY, 0, pl.x, pl.y + 10, true);
  if (pl.has[96] && rooms % 3 == 0) pickup_spawn(PK_HALF_HEART, 0, pl.x, pl.y + 10, true);
  if (pl.has[131] && rooms % 3 == 1) pickup_spawn(PK_BOMB, 0, pl.x, pl.y + 10, true);
  if (pl.has[98] && rooms % 6 == 0) pickup_spawn(PK_SOUL, 0, pl.x, pl.y + 10, true);
}

/* The D6: every item in the room rolled again, from the room's own pool */
void item_reroll_room(void) {
  int kind = room_cur()->kind;
  int pool = kind == ROOM_SHOP ? POOL_SHOP : kind == ROOM_BOSS ? POOL_BOSS : kind == ROOM_DEVIL ? POOL_DEVIL
             : kind == ROOM_ANGEL ? POOL_ANGEL : kind == ROOM_SECRET ? POOL_SECRET : kind == ROOM_LIBRARY ? POOL_LIBRARY : POOL_TREASURE;
  for (int i = 0; i < MAX_ENTS; i++)
    if (ents[i].kind == EK_PEDESTAL && ents[i].t3) {
      ents[i].t3 = (int16_t)item_from_pool(pool);
      fx_spawn(FX_POOF, ents[i].x, ents[i].y - 8, 10);
    }
}

/* ---------------------------------------------------------------- pills: 13 colours, each run's effects shuffled */
enum { PL_BAD_GAS, PL_BAD_TRIP, PL_BALLS, PL_BOMBS_KEYS, PL_DIARRHEA, PL_FULL_HEALTH, PL_HEALTH_DOWN, PL_HEALTH_UP,
       PL_PILLS, PL_PUBERTY, PL_PRETTY_FLY, PL_RANGE_DOWN, PL_RANGE_UP, PL_SPEED_DOWN, PL_SPEED_UP, PL_TEARS_DOWN,
       PL_TEARS_UP, PL_LUCK_DOWN, PL_LUCK_UP, PL_TELEPILLS, PL_48H, PL_HEMATEMESIS, PL_PARALYSIS, PL_SEE, PL_PHEROMONES,
       PL_AMNESIA, PL_LEMON, PL_COUNT };
static const char *const pill_names[PL_COUNT] = {
  "BAD GAS", "BAD TRIP", "BALLS OF STEEL", "BOMBS ARE KEY", "EXPLOSIVE DIARRHEA", "FULL HEALTH", "HEALTH DOWN",
  "HEALTH UP", "I FOUND PILLS", "PUBERTY", "PRETTY FLY", "RANGE DOWN", "RANGE UP", "SPEED DOWN", "SPEED UP", "TEARS DOWN",
  "TEARS UP", "LUCK DOWN", "LUCK UP", "TELEPILLS", "48 HOUR ENERGY", "HEMATEMESIS", "PARALYSIS", "I CAN SEE FOREVER!",
  "PHEROMONES", "AMNESIA", "LEMON PARTY"};
#define pill_effect pl.pill_effect
static bool pill_bad(int e) {
  return e == PL_BAD_TRIP || e == PL_HEALTH_DOWN || e == PL_RANGE_DOWN || e == PL_SPEED_DOWN || e == PL_TEARS_DOWN
         || e == PL_LUCK_DOWN || e == PL_AMNESIA || e == PL_PARALYSIS;
}
void pills_shuffle(void) {
  uint8_t all[PL_COUNT];
  for (int i = 0; i < PL_COUNT; i++) all[i] = (uint8_t)i;
  for (int i = PL_COUNT - 1; i > 0; i--) {
    int j = rnd(i + 1);
    uint8_t t = all[i];
    all[i] = all[j], all[j] = t;
  }
  for (int i = 0; i < 14; i++) pill_effect[i] = all[i];
}
const char *pill_name(int color, bool known) { return known ? pill_names[pill_effect[color % 14]] : "PILL"; }

void pill_use(int color) {
  int e = pill_effect[color % 14];
  /* PHD: the bad ones turn good */
  if (pl.has[75] && pill_bad(e)) {
    static const uint8_t good[] = {PL_BALLS, PL_HEALTH_UP, PL_RANGE_UP, PL_SPEED_UP, PL_TEARS_UP, PL_LUCK_UP, PL_FULL_HEALTH};
    e = good[e % sizeof good];
  }
  pl.pill_known[color % 16] = (uint8_t)(e + 1);
  hud_banner(pill_names[e], "");
  switch (e) {
    case PL_BAD_GAS: use_effect(111); break;
    case PL_BAD_TRIP: if (player_hearts_total() <= 2) player_add_soul(4, false), hud_banner("BALLS OF STEEL", ""); else player_hurt(2, -9); break;
    case PL_BALLS: player_add_soul(4, false); break;
    case PL_BOMBS_KEYS: { uint8_t t = pl.bombs; pl.bombs = pl.keys, pl.keys = t; break; }
    case PL_DIARRHEA: for (int i = 0; i < 5; i++) bomb_place(pl.x + rnd(20) - 10, pl.y + rnd(16) - 8, 0); break;
    case PL_FULL_HEALTH: player_heal(24); break;
    case PL_HEALTH_DOWN: if (pl.containers > 1) player_add_containers(-1); break;
    case PL_HEALTH_UP: player_add_containers(1); break;
    case PL_PRETTY_FLY: blue_flies(2); break;
    case PL_RANGE_DOWN: pl.pill_range -= 2; break;
    case PL_RANGE_UP: pl.pill_range += 2.5f; break;
    case PL_SPEED_DOWN: pl.pill_speed -= 0.12f; break;
    case PL_SPEED_UP: pl.pill_speed += 0.15f; break;
    case PL_TEARS_DOWN: pl.pill_tears -= 0.28f; break;
    case PL_TEARS_UP: pl.pill_tears += 0.35f; break;
    case PL_LUCK_DOWN: pl.pill_luck -= 1; break;
    case PL_LUCK_UP: pl.pill_luck += 1; break;
    case PL_TELEPILLS: random_room_teleport(-1); break;
    case PL_48H: { const ItemInfo *ii = item_info(pl.active); if (ii && ii->charge && ii->charge < 254) pl.active_charge = ii->charge; break; }
    case PL_HEMATEMESIS: { int n = pl.red / 2; pl.red = pl.containers ? 1 : 0; for (int i = 0; i < n && i < 5; i++) pickup_spawn(PK_HEART, 0, pl.x, pl.y, true); break; }
    case PL_PARALYSIS: pl.invuln = 0; pl.vx = pl.vy = 0; break;
    case PL_SEE: floor_reveal(4); break;
    case PL_PHEROMONES: for (int i = 0; i < MAX_ENTS; i++) if (ents[i].kind == EK_MON && !(ents[i].flags & EF_BOSS) && dist2(ents[i].x, ents[i].y, pl.x, pl.y) < 80 * 80) ents[i].flags |= EF_FRIEND; break;
    case PL_AMNESIA: for (int i = 0; i < fl.nrooms; i++) if (i != fl.cur) fl.rooms[i].state &= (uint8_t)~RS_SEEN; break;
    case PL_LEMON: creep_spawn(pl.x, pl.y, 30, 240, false, 2); break;
    default: break;
  }
  if (pl.has[75]) player_heal(1);
  player_stats();
}

/* ---------------------------------------------------------------- cards */
static const char *const card_names[32] = {
  "", "0 - THE FOOL", "I - THE MAGICIAN", "II - THE HIGH PRIESTESS", "III - THE EMPRESS", "IV - THE EMPEROR",
  "V - THE HIEROPHANT", "VI - THE LOVERS", "VII - THE CHARIOT", "VIII - JUSTICE", "IX - THE HERMIT",
  "X - WHEEL OF FORTUNE", "XI - STRENGTH", "XII - THE HANGED MAN", "XIII - DEATH", "XIV - TEMPERANCE",
  "XV - THE DEVIL", "XVI - THE TOWER", "XVII - THE STARS", "XVIII - THE MOON", "XIX - THE SUN", "XX - JUDGEMENT",
  "XXI - THE WORLD", "2 OF CLUBS", "2 OF DIAMONDS", "2 OF SPADES", "2 OF HEARTS", "ACE OF CLUBS", "ACE OF DIAMONDS",
  "ACE OF SPADES", "ACE OF HEARTS", "JOKER"};
const char *card_name(int c) { return c > 0 && c < 32 ? card_names[c] : "CARD"; }

static void pickups_to(int type) {
  for (int i = 0; i < MAX_ENTS; i++)
    if (ents[i].kind == EK_PICKUP && !(ents[i].flags & EF_SHOP)) {
      float x = ents[i].x, y = ents[i].y;
      ents[i].kind = EK_NONE;
      pickup_spawn(type, 0, x, y, false);
    }
}

void card_use(int c) {
  hud_banner(card_name(c), "");
  switch (c) {
    case 1: room_teleport(fl.start); break;
    case 2: pl.telepathy = 1; break;
    case 3: use_effect(86); break;
    case 4: pl.room_damage += 1.5f; pl.unicorn = 0; break;
    case 5: room_teleport(fl.boss_room); break;
    case 6: player_add_soul(4, false); break;
    case 7: for (int i = 0; i < 2; i++) pickup_spawn(PK_HEART, 0, pl.x, pl.y + 10, true); break;
    case 8: pl.unicorn = 240; break;
    case 9: { static const uint8_t k[4] = {PK_PENNY, PK_BOMB, PK_KEY, PK_HEART}; for (int i = 0; i < 4; i++) pickup_spawn(k[i], 0, pl.x, pl.y + 10, true); break; }
    case 10: for (int i = 0; i < fl.nrooms; i++) if (fl.rooms[i].kind == ROOM_SHOP) { room_teleport(i); break; } break;
    case 11: machine_spawn(rnd(2) ? MA_SLOT : MA_FORTUNE, pl.x, pl.y - 30); break;
    case 12: pl.room_damage += 1.0f; player_add_containers(0); break;
    case 13: pl.bible = 1; break;
    case 14: hurt_all(40); break;
    case 15: machine_spawn(MA_BLOOD, pl.x, pl.y - 30); break;
    case 16: pl.belial = 1; break;
    case 17: use_effect(65); break;
    case 18: for (int i = 0; i < fl.nrooms; i++) if (fl.rooms[i].kind == ROOM_TREASURE) { room_teleport(i); break; } break;
    case 19: for (int i = 0; i < fl.nrooms; i++) if (fl.rooms[i].kind == ROOM_SECRET) { room_teleport(i); break; } break;
    case 20: player_heal(24); floor_reveal(1); hurt_all(100); break;
    case 21: machine_spawn(MA_BEGGAR, pl.x, pl.y - 30); break;
    case 22: floor_reveal(7); break;
    case 23: pl.bombs = (uint8_t)(pl.bombs * 2 > 99 ? 99 : pl.bombs * 2); break;
    case 24: pl.coins = (uint8_t)(pl.coins * 2 > 99 ? 99 : pl.coins * 2); break;
    case 25: pl.keys = (uint8_t)(pl.keys * 2 > 99 ? 99 : pl.keys * 2); break;
    case 26: player_heal(pl.red); break;
    case 27: pickups_to(PK_BOMB); break;
    case 28: pickups_to(PK_PENNY); break;
    case 29: pickups_to(PK_KEY); break;
    case 30: pickups_to(PK_HEART); break;
    case 31:   /* the Joker: to the Devil's (or an angel's) room */
      floor_open_devil();
      room_teleport(fl.devil_room);
      break;
    default: break;
  }
  player_stats();
}

/* ---------------------------------------------------------------- familiars */
static const uint8_t familiar_items[] = {8, 10, 57, 67, 73, 88, 95, 99, 100, 112, 113, 128, 144, 155, 163, 167, 174, 188};
int familiars_count(void) {
  int n = 0;
  for (unsigned i = 0; i < sizeof familiar_items; i++) n += pl.has[familiar_items[i]];
  return n;
}

void familiars_spawn(void) {
  for (int i = 0; i < MAX_ENTS; i++)
    if (ents[i].kind == EK_FAMILIAR) ents[i].kind = EK_NONE;
  int follow = 0, orbit = 0;
  for (unsigned k = 0; k < sizeof familiar_items; k++) {
    int id = familiar_items[k];
    for (int n = 0; n < pl.has[id]; n++) {
      int count = id == 10 ? 2 : 1;
      for (int j = 0; j < count; j++) {
        Ent *e = ent_new(EK_FAMILIAR, pl.x, pl.y + 10);
        if (!e) return;
        e->type = (uint8_t)(id & 0xFF), e->t3 = (int16_t)id, e->spr = (int16_t)item_sprite(id), e->r = 5;
        bool orbital = id == 10 || id == 57 || id == 73 || id == 112 || id == 128;
        e->state = orbital;
        e->sub = (uint8_t)(orbital ? orbit++ : follow++);
        e->t = (int16_t)(rnd(20));
      }
    }
  }
}

void familiar_update(Ent *e) {
  int id = e->t3;
  if (e->state) {   /* orbitals: round and round him, stopping shots */
    float r = id == 57 || id == 128 ? 34 : id == 112 ? 26 : 18, sp = id == 57 || id == 128 ? 0.07f : 0.1f;
    float a = g_frame * sp + e->sub * 2.4f;
    e->x = pl.x + fcos(a) * r, e->y = pl.y - 4 + fsin(a) * r * 0.8f;
    for (int i = 0; i < MAX_TEARS; i++) {
      Tear *t = &tears[i];
      if (t->active && (t->flags & TF_ENEMY) && dist2(t->x, t->y, e->x, e->y) < 8 * 8) t->active = 0;
    }
    if (id == 57 || id == 128 || id == 73 || id == 112)
      for (int i = 0; i < MAX_ENTS; i++) {
        Ent *m = &ents[i];
        if (m->kind == EK_MON && !(m->flags & EF_FRIEND) && dist2(m->x, m->y, e->x, e->y) < (m->r + 6) * (m->r + 6) && (g_frame & 3) == 0)
          ent_damage(m, id == 57 ? 5 : id == 128 ? 2 : 3, 0, 0, 0);
      }
    return;
  }
  /* followers: a line behind him */
  float tx = pl.x - DIR_X[pl.face] * (14 + e->sub * 12), ty = pl.y - DIR_Y[pl.face] * (12 + e->sub * 10) + 2;
  if (id == 188) tx = 320 - pl.x, ty = 240 - pl.y + 0;   /* Abel: across the room */
  if (id == 155) {   /* The Peeper: bounces about, hurting */
    if (!e->vx && !e->vy) e->vx = 1.4f, e->vy = 1.1f;
    float ox = e->vx, oy = e->vy;
    move_ent(&e->x, &e->y, &e->vx, &e->vy, 5, true, true);
    if (!e->vx) e->vx = -ox;
    if (!e->vy) e->vy = -oy;
    for (int i = 0; i < MAX_ENTS; i++)
      if (ents[i].kind == EK_MON && !(ents[i].flags & EF_FRIEND) && dist2(ents[i].x, ents[i].y, e->x, e->y) < (ents[i].r + 6) * (ents[i].r + 6) && (g_frame & 3) == 0)
        ent_damage(&ents[i], 2, 0, 0, 0);
    return;
  }
  if (id == 88 && e->c > 0) {   /* Little Chubby: thrown at them */
    e->x += e->vx, e->y += e->vy, e->c -= 1;
    for (int i = 0; i < MAX_ENTS; i++)
      if (ents[i].kind == EK_MON && !(ents[i].flags & EF_FRIEND) && dist2(ents[i].x, ents[i].y, e->x, e->y) < (ents[i].r + 6) * (ents[i].r + 6))
        ent_damage(&ents[i], 1.5f, 0, 0, 0);
    if (e->x < ROOM_X || e->x > ROOM_R || e->y < ROOM_Y || e->y > ROOM_B) e->c = 0;
    return;
  }
  e->x += (tx - e->x) * 0.12f, e->y += (ty - e->y) * 0.12f;
  if (e->t > 0) {
    e->t--;
    return;
  }
  /* they cry when he does */
  int dir = pl.shoot_dir;
  if (id == 113) {   /* Demon Baby: at the nearest monster, on its own */
    Ent *m = ent_nearest_monster(e->x, e->y, 110);
    if (m) {
      float dx = m->x - e->x, dy = m->y - e->y, d = fsqrt(dx * dx + dy * dy) + 0.1f;
      tear_fire(e->x, e->y, dx / d * 5, dy / d * 5, 3, TF_FAMILIAR);
      e->t = 12;
    }
    return;
  }
  if (id == 144) {   /* Bum Friend: after the coins, and something back now and then */
    Ent *c = NULL;
    float bd = 1e9f;
    for (int i = 0; i < MAX_ENTS; i++)
      if (ents[i].kind == EK_PICKUP && !(ents[i].flags & EF_SHOP) && ents[i].type >= PK_PENNY && ents[i].type <= PK_DIME) {
        float d = dist2(ents[i].x, ents[i].y, e->x, e->y);
        if (d < bd) bd = d, c = &ents[i];
      }
    if (c) {
      e->x += (c->x - e->x) * 0.1f, e->y += (c->y - e->y) * 0.1f;
      if (bd < 8 * 8) {
        e->c += c->type == PK_PENNY ? 1 : c->type == PK_NICKEL ? 5 : 10;
        c->kind = EK_NONE;
        if (e->c >= 3 + rnd(4)) e->c = 0, pickup_drop_random(e->x, e->y);
      }
    }
    return;
  }
  if (dir == DIR_NONE) return;
  float sx = DIR_X[dir] * 5, sy = DIR_Y[dir] * 5;
  if (id == 188) sx = -sx, sy = -sy;
  switch (id) {
    case 8: tear_fire(e->x, e->y, sx, sy, 3.5f, TF_FAMILIAR); e->t = 22; break;
    case 67: tear_fire(e->x, e->y, sx, sy, 7, TF_FAMILIAR | TF_BLOOD); e->t = 22; break;
    case 100: tear_fire(e->x, e->y, sx, sy, 3.5f, TF_FAMILIAR | TF_HOMING); e->t = 22; break;
    case 163: tear_fire(e->x, e->y, sx, sy, 3.5f, TF_FAMILIAR | TF_SPECTRAL | TF_PIERCE); e->t = 22; break;
    case 99: tear_fire(e->x, e->y, sx, sy, 3.5f, TF_FAMILIAR | TF_SLOW); e->t = 22; break;
    case 95: laser_fire(e->x, e->y - 6, dir, 2, 3.5f, 3, 1, false); e->t = 30; break;
    case 167: for (int k = -1; k <= 1; k += 2) tear_fire(e->x, e->y, sx + DIR_Y[dir] * k, sy + DIR_X[dir] * k, 4, TF_FAMILIAR); e->t = 22; break;
    case 174: { static const uint32_t f[4] = {TF_HOMING, TF_PIERCE, TF_POISON, TF_SLOW}; tear_fire(e->x, e->y, sx, sy, 3.5f, TF_FAMILIAR | f[rnd(4)]); e->t = 22; break; }
    case 188: tear_fire(e->x, e->y, sx, sy, 3.5f, TF_FAMILIAR); e->t = 22; break;
    case 88: e->vx = sx * 1.2f, e->vy = sy * 1.2f, e->c = 40, e->t = 60; break;
    default: break;
  }
}

void familiar_draw(Ent *e) {
  int bob = (int)((g_frame / 5 + e->sub) % 2);
  if (e->t3 == 10) {   /* the halo's flies */
    gfx_sprite(SPR_FLY, 0, (int)e->x, (int)e->y + 3 - bob, 0, 32);
    return;
  }
  if (e->t3 == 57 || e->t3 == 128) {
    gfx_sprite_tint(SPR_ATTACK_FLY, 0, (int)e->x, (int)e->y + 3 - bob, 0, rgb565(0x80FF80), e->t3 == 57 ? 8 : 0, 32);
    return;
  }
  gfx_sprite_scaled(e->spr, 0, (int)e->x, (int)e->y + 4 - bob, 200, 200, 0, 0, 0, 32);
}
