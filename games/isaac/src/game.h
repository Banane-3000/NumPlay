/* The Binding of Isaac: what the whole game shares.
 *
 * The game runs as the original does, 30 times a second. A room is 13 x 7
 * cells of 20 pixels on the screen (the game's 26), its floor from (30, 50)
 * to (290, 190); positions are screen pixels. The game's own distances (40
 * units a cell) are half a pixel each here. */
#ifndef IS_GAME_H
#define IS_GAME_H
#include <stdbool.h>
#include <stdint.h>
#include "data.h"
#include "plat.h"

#define TILE 20
#define COLS 13
#define ROWS 7
#define ROOM_X 30            /* the floor's top-left on the screen */
#define ROOM_Y 50
#define ROOM_R (ROOM_X + COLS * TILE)
#define ROOM_B (ROOM_Y + ROWS * TILE)
#define FPS 30

/* ---------------------------------------------------------------- random numbers */
uint32_t rng_next(uint32_t *s);
int rnd(int n);                  /* 0 .. n-1, the game's own */
float rndf(void);                /* 0 .. 1 */
bool chance(float p);
extern uint32_t g_rng;

/* ---------------------------------------------------------------- the player */
enum { DIR_DOWN, DIR_LEFT, DIR_UP, DIR_RIGHT, DIR_NONE };
extern const int8_t DIR_X[4], DIR_Y[4];

/* how tears behave (items give them) */
enum {
  TF_HOMING = 1 << 0, TF_PIERCE = 1 << 1, TF_SPECTRAL = 1 << 2, TF_POISON = 1 << 3, TF_SLOW = 1 << 4,
  TF_FREEZE = 1 << 5, TF_SPLIT = 1 << 6, TF_EXPLODE = 1 << 7, TF_BOOMERANG = 1 << 8, TF_CHARM = 1 << 9,
  TF_FEAR = 1 << 10, TF_CONFUSE = 1 << 11, TF_TOOTH = 1 << 12, TF_BLOOD = 1 << 13, TF_KNOCK = 1 << 14,
  TF_ENEMY = 1 << 15, TF_BOMB = 1 << 16, TF_ARC = 1 << 17, TF_COAL = 1 << 18, TF_FAMILIAR = 1 << 19,
  TF_BONE = 1 << 20, TF_BIG = 1 << 21, TF_PROPTOSIS = 1 << 22, TF_HOLY = 1 << 23, TF_FIRE = 1 << 24
};

#define ITEM_MAX 262
typedef struct {
  float x, y, vx, vy;
  int8_t face, shoot_dir;          /* where he looks; the way he cries (DIR_NONE: not crying) */
  uint8_t walk;                    /* the walk's frame counter */
  /* stats, as the game shows them */
  float damage, tear_delay, range, shot_speed, speed, luck;
  float tears_ups, damage_ups;     /* (what the items add, before the game's formulas) */
  uint32_t tear_flags;
  uint8_t shots;                   /* tears at once (The Inner Eye: 3, Mutant Spider: 4, 20/20: 2) */
  float fire_cd;                   /* frames until he can cry again */
  int16_t charge;                  /* Chocolate Milk, Brimstone, Mom's Knife: how long he has charged */
  uint8_t tear_eye;                /* which eye cries next */
  /* health: red containers and red hearts, in halves; soul hearts in halves, black ones a bit each */
  uint8_t containers, red, soul, eternal;
  uint16_t black;                  /* bit i: soul heart i (whole hearts, from the left) is black */
  uint8_t coins, bombs, keys;
  int16_t active, active_charge;   /* the active item (0: none) and its charge */
  uint8_t pocket_kind, pocket;     /* a pill or a card */
  uint8_t has[ITEM_MAX];           /* the items he has, how many of each */
  uint8_t order[64], norder;       /* the order he found them in */
  bool flight;
  int16_t invuln;                  /* frames he can't be hurt */
  int16_t hurt_anim, hold_anim;    /* hurt flash; holding an item up */
  int16_t hold_item;
  int16_t belial, bible, shadows, unicorn, hourglass, telepathy, nail, pony, shoop, lemon;   /* active items' room effects */
  float room_damage;               /* damage only for this room (Book of Belial, Blood Rights) */
  int16_t lust;                    /* Bloody Lust's hits this room */
  uint8_t lives;                   /* 1up!, Dead Cat, Ankh, Lazarus... extra lives */
  bool dead;
  int16_t vampire;                 /* kills toward Charm of the Vampire's heal */
  uint8_t pill_known[16];          /* which pill colours he knows (their effect + 1) */
  uint8_t guppy, lotf;             /* transformations: Guppy, Lord of the Flies (items counted) */
  uint8_t damage_taken_floor;      /* red damage this floor (the Devil's room) */
  int16_t creep;                   /* frames of creep left under him (Lemon Mishap) */
  uint8_t pill_effect[14];         /* what each pill colour does this run */
  float pill_tears, pill_range, pill_speed, pill_luck;   /* what pills did to his stats */
  uint8_t devil_deals;             /* deals taken (no angels after one) */
  uint8_t character;               /* CH_* */
} Player;
extern Player pl;

enum { CH_ISAAC, CH_MAGDALENE, CH_CAIN, CH_JUDAS, CH_BLUEBABY, CH_EVE, CH_SAMSON, CH_AZAZEL, CH_COUNT };
void player_reset(int character);
const char *character_name(int c);
bool character_unlocked(int c);
const char *character_unlock_hint(int c);
int character_head(int c);                   /* its head's sprite (its body's is the next one) */
void player_stats(void);                     /* recomputes stats from the items */
void player_starting_items(void);
void player_update(uint32_t keys);
void player_draw_shadow(void);
void player_draw(void);
bool player_hurt(int halves, int source);   /* true if it took */
void player_heal(int halves);
void player_add_soul(int halves, bool black);
void player_add_containers(int n);
int player_hearts_total(void);               /* halves of every kind */
void player_kill(int source);

/* ---------------------------------------------------------------- tears, shots, lasers, bombs */
typedef struct {
  float x, y, z, vx, vy, vz;       /* z: height over the floor (negative is up) */
  float dmg, size, dist, range;
  uint32_t flags;
  uint8_t active;
  int8_t lasthit;                  /* the last monster it went through (piercing) */
  int16_t life;
  float ang;                       /* homing, orbiting */
  uint16_t color;
} Tear;
#define MAX_TEARS 96
extern Tear tears[MAX_TEARS];
Tear *tear_fire(float x, float y, float vx, float vy, float dmg, uint32_t flags);
Tear *enemy_shot(float x, float y, float vx, float vy, int kind);   /* kind: 0 blood, 1 bone, 2 fire */
void enemy_shots_ring(float x, float y, int n, float speed, float a0);
void enemy_shot_at(float x, float y, float speed);
void tears_update(void);
void tears_draw(void);
void tears_clear(void);

typedef struct {
  float x, y;
  int8_t dir;                      /* DIR_*, or -1: from (x, y) to (x2, y2) */
  float x2, y2;
  float width, dmg;
  int16_t life, max;
  uint8_t active, enemy, kind;     /* kind: 0 blood (Brimstone), 1 tech, 2 holy, 3 Shoop */
  float len;                       /* how far it reaches (0: to the wall) */
} Laser;
#define MAX_LASERS 8
extern Laser lasers[MAX_LASERS];
Laser *laser_fire(float x, float y, int dir, float width, float dmg, int life, int kind, bool enemy);
void lasers_update(void);
void lasers_draw(void);

void bomb_place(float x, float y, int kind);      /* kind: 0 Isaac's, 1 troll, 2 big (Mr. Boom), 3 Dr. Fetus */
void explode(float x, float y, float radius, float dmg, bool hurt_player, int kind);

/* ---------------------------------------------------------------- the room's things */
enum { EK_NONE, EK_MON, EK_PICKUP, EK_BOMB, EK_FX, EK_FAMILIAR, EK_PEDESTAL, EK_MACHINE, EK_TRAPDOOR, EK_CREEP,
       EK_FIRE, EK_DECAL };
/* pickups */
enum { PK_HEART, PK_HALF_HEART, PK_SOUL, PK_BLACK, PK_ETERNAL, PK_DOUBLE_HEART, PK_HALF_SOUL, PK_PENNY, PK_NICKEL,
       PK_DIME, PK_KEY, PK_GOLDEN_KEY, PK_KEY_RING, PK_BOMB, PK_DOUBLE_BOMB, PK_TROLL_BOMB, PK_CHEST, PK_LOCKED_CHEST,
       PK_RED_CHEST, PK_PILL, PK_CARD, PK_BATTERY, PK_GRAB_BAG, PK_SPIKED_CHEST, PK_GOLDEN_BOMB, PK_COUNT };
/* machines and beggars */
enum { MA_SLOT, MA_BLOOD, MA_FORTUNE, MA_BEGGAR, MA_DEVIL_BEGGAR, MA_KEY_MASTER, MA_BOMB_BUM, MA_SHOPKEEPER,
       MA_DEVIL_STATUE, MA_ANGEL_STATUE };
/* effects */
enum { FX_POOF, FX_BLOOD, FX_EXPLOSION, FX_SPLASH, FX_GIB, FX_TEXT, FX_SPARK, FX_FLAME, FX_LIGHT, FX_SHOCKWAVE,
       FX_STOMP };

enum {
  EF_FLYING = 1 << 0, EF_BOSS = 1 << 1, EF_INVULN = 1 << 2, EF_NOHIT = 1 << 3, EF_CHAMPION = 1 << 4,
  EF_SPAWNED = 1 << 5, EF_NOGRID = 1 << 6, EF_HIDDEN = 1 << 7, EF_SEGMENT = 1 << 8, EF_FRIEND = 1 << 9,
  EF_SHOP = 1 << 10, EF_DEVIL = 1 << 11, EF_NOCOUNT = 1 << 12, EF_ANGEL = 1 << 13, EF_NOKNOCK = 1 << 14,
  EF_FLIP = 1 << 15
};

typedef struct {
  uint8_t kind, type, sub, state;
  uint16_t flags;
  int16_t spr, frame;
  float x, y, z, vx, vy, vz;
  float r;                          /* radius */
  float hp, maxhp;
  int16_t t, t2, t3, price;
  float a, b, c;
  int8_t parent, child;
  uint8_t flash, poison, slow, freeze, charm, fear, confuse;
  float poison_dmg;
  uint8_t alpha;                    /* 32 opaque */
} Ent;
#define MAX_ENTS 112
extern Ent ents[MAX_ENTS];
Ent *ent_new(int kind, float x, float y);
void ents_clear(void);
int ents_alive_monsters(void);              /* monsters that keep the doors shut */
Ent *ent_nearest_monster(float x, float y, float maxd);
void ents_update(void);
void ents_draw(void);                        /* everything, sorted by depth, the player among them */
void ent_damage(Ent *e, float dmg, uint32_t flags, float kx, float ky);
void ent_kill(Ent *e);

/* monsters (monsters.c), bosses (bosses.c) */
Ent *mon_spawn(int type, float x, float y);
void mon_update(Ent *e);
void mon_draw(Ent *e);
bool mon_on_death(Ent *e);                    /* false: it isn't dead after all (a Globin) */
const char *mon_name(int type);
bool boss_update(Ent *e);                    /* false if e isn't a boss's */
void boss_spawn(int type, float x, float y);
bool boss_on_death(Ent *e);
void boss_draw(Ent *e);
float bosses_hp(float *max);                 /* the boss bar */
const char *boss_name(int type);
int boss_sprite(int type);
void mon_set_sprite(Ent *e);

/* pickups (pickups.c) */
Ent *pickup_spawn(int type, int sub, float x, float y, bool pop);
void pickup_update(Ent *e);
int pickup_random_type(void);
void pickup_drop_random(float x, float y);
void room_clear_reward(void);
Ent *pedestal_spawn(int item, float x, float y, int price, bool devil);
Ent *machine_spawn(int type, float x, float y);
void machine_update(Ent *e);
void fx_spawn(int type, float x, float y, int life);
Ent *fx_text(float x, float y, const char *s);
void decal_blood(float x, float y, float size);
Ent *creep_spawn(float x, float y, float r, int life, bool enemy, int color);

/* ---------------------------------------------------------------- items (items.c) */
enum { POOL_TREASURE, POOL_SHOP, POOL_BOSS, POOL_DEVIL, POOL_ANGEL, POOL_SECRET, POOL_LIBRARY, POOL_CURSE,
       POOL_COUNT };
typedef struct {
  uint16_t id;
  const char *name, *text;
  uint8_t pools;                   /* POOL_* bits */
  uint8_t charge;                  /* an active item's charges (0: passive; 255: used once) */
  uint8_t quality;
} ItemInfo;
const ItemInfo *item_info(int id);
int item_sprite(int id);
int item_from_pool(int pool);                  /* takes it out of the pool */
void item_give(int id);                        /* picking it up: its effect, its text */
void item_use_active(void);
void item_room_cleared(void);
void items_new_run(void);
bool item_is_active(int id);
int item_count_in_run(void);
void pill_use(int color);
const char *pill_name(int color, bool known);
void card_use(int card);
const char *card_name(int card);
int familiars_count(void);
void familiars_spawn(void);
void familiar_update(Ent *e);
void familiar_draw(Ent *e);
void item_reroll_room(void);                    /* The D6 */

/* ---------------------------------------------------------------- the floor and its rooms (floor.c) */
#define MAP_W 13
#define MAP_H 9
#define MAX_ROOMS 28
#define ROOM_SAVED 12
enum { RS_VISITED = 1, RS_CLEARED = 2, RS_SEEN = 4, RS_FOUND = 8, RS_PAID = 16, RS_SPIKED = 32 };
typedef struct {
  uint8_t kind, type, sub, flags;
  uint16_t val;                                  /* a pedestal's item, a pickup's price */
  int16_t x, y;
} Saved;                                         /* a thing left lying in a room */
typedef struct {
  uint8_t kind, tmpl, group, state;
  uint8_t mx, my;                                /* its cell on the map */
  uint8_t grid[ROWS][COLS];
  uint8_t gridhp[ROWS][COLS];                     /* poop's state, fire's */
  uint8_t nsaved, boss, variant;
  uint8_t doors_open;                             /* bit d: the door that way is open for good (keys, bombs) */
  Saved saved[ROOM_SAVED];
} Room;
typedef struct {
  uint8_t stage;                                 /* 1: Basement I .. 8: Utero II, 9: Sheol, 10: Cathedral... */
  uint8_t alt;                                   /* Cellar, Catacombs, Necropolis, Utero; Cathedral */
  uint8_t curse;
  uint8_t nrooms;
  int8_t map[MAP_H][MAP_W];                      /* room index, or -1 */
  int8_t cur, start, boss_room, devil_room;
  uint8_t boss;                                  /* the boss's MON_* */
  bool devil_open, devil_angel, boss_beaten, trapdoor;
  int8_t devil_side;                             /* the wall of the boss's room it opens in */
  bool golden_key, golden_bomb;                  /* found on this floor: keys and bombs for nothing */
  uint32_t seed;
  Room rooms[MAX_ROOMS];
} Floor;
extern Floor fl;
enum { CURSE_DARKNESS = 1, CURSE_LOST = 2, CURSE_UNKNOWN = 4, CURSE_MAZE = 8, CURSE_BLIND = 16, CURSE_LABYRINTH = 32 };

void floor_new(int stage);
void floor_enter_room(int index, int from_dir);   /* from_dir: the door he came through, DIR_NONE: the middle */
Room *room_cur(void);
int room_backdrop(void);
const char *stage_name(int stage, bool alt);
int chapter_of(int stage);                       /* 1 Basement .. 4 Womb, 5 Sheol */
void room_save_things(void);
void floor_reveal(int what);                     /* 1: the map, 2: special rooms, 4: secret rooms */
bool room_door_exists(int d, int *kind);         /* a door that way, and the room's kind behind it */
bool room_door_open(int d);
void room_open_doors(void);
void room_teleport(int index);
void floor_open_devil(void);

/* the grid (room.c) */
bool grid_solid(int c, int r, bool flying);      /* blocks walking (or flying) */
bool grid_blocks_tears(int c, int r);
bool grid_pit(int c, int r);
bool solid_at(float x, float y, bool flying);
void move_ent(float *x, float *y, float *vx, float *vy, float r, bool flying, bool nogrid);
void grid_hit(int c, int r, float dmg);          /* a tear hits a poop or a fire */
void grid_explode(float x, float y, float radius);
void grid_draw_row(int r);
void grid_update(void);
void grid_break_at(float x, float y);
extern bool g_ladder;                          /* fire burns, red fire shoots */
bool door_try_unlock(int d);
void door_bombed(float x, float y);
int floor_neighbour(int d);
void doors_draw(void);
bool grid_line_clear(float x0, float y0, float x1, float y1);
int grid_spikes_at(float x, float y);
void room_draw_floor(void);                      /* the walls and floor, decals, creep */
int path_dir(float x, float y, float tx, float ty, bool flying, float *dx, float *dy);   /* the way to walk */

/* ---------------------------------------------------------------- the HUD (hud.c) */
void hud_draw(void);
void hud_banner(const char *title, const char *sub);   /* an item's name, a floor's */
void map_draw_full(void);
extern int16_t banner_t;

/* ---------------------------------------------------------------- the game */
enum { MODE_TITLE, MODE_MENU, MODE_PLAY, MODE_PAUSE, MODE_MAP, MODE_DEAD, MODE_WIN, MODE_STAGE, MODE_HELP,
       MODE_CONFIRM, MODE_STATS, MODE_CHARSEL };
extern int g_mode;
extern bool g_running;
extern uint32_t g_frame;          /* frames since the run began */
extern uint32_t g_keys, g_pressed;
extern int g_room_frames;         /* frames since the room was entered */
extern int g_shake;               /* screen shake left */
extern int g_killer;              /* what killed him (MON_*, or -1..-n: other causes) */
extern bool g_room_was_cleared;
extern uint32_t g_run_seed;
extern int g_transition;          /* frames of the dip to black between rooms */
extern int g_boss_intro;          /* frames left of the boss's introduction (him VS it) */
extern uint8_t g_wins;            /* runs won: Mom, Mom's Heart, Satan, Isaac (progress kept) */
extern uint16_t g_deaths, g_runs;
extern uint8_t g_best_stage;
extern bool g_swap;              /* the number keys walk and the arrows cry (as WASD and the arrows) */

void game_init(void);
void game_frame(uint32_t keys);
void game_draw(void);
void game_new_run(int character);
extern uint8_t g_last_char;
void game_next_floor(void);
void game_won(int how);
void game_over(void);

/* saves (save.c) */
bool save_run(void);
bool load_run(void);
bool run_saved(void);
void save_remove_run(void);
void save_progress(void);
void load_progress(void);
void save_restore_copy(void);

static inline float clampf(float v, float a, float b) { return v < a ? a : v > b ? b : v; }
static inline float absf(float v) { return v < 0 ? -v : v; }
float fsqrt(float v);
float fatan2(float y, float x);
float fsin(float a);
float fcos(float a);
float dist2(float ax, float ay, float bx, float by);
#endif
