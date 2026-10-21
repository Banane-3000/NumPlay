/* Plants vs. Zombies: the lawn and everything on it.
 *
 * The simulation is the game's own, at its own scale: positions in the pixels of
 * its 800 x 600 screen (the lawn's left at 40, its top at 80, cells 80 wide and
 * 100 high, 85 in the backyard and on the roof), time in ticks of a hundredth of
 * a second, health, damage, speeds and timers as the game counts them. Drawing
 * scales it all by 0.4 to the calculator's screen. */
#ifndef PVZ_GAME_H
#define PVZ_GAME_H
#include <stdbool.h>
#include <stdint.h>

#define COLS 9
#define MAX_ROWS 6
#define BOARD_LEFT 40
#define BOARD_TOP 80
#define CELL_W 80

enum Plant_ {
  P_PEASHOOTER, P_SUNFLOWER, P_CHERRYBOMB, P_WALLNUT, P_POTATOMINE, P_SNOWPEA, P_CHOMPER, P_REPEATER,
  P_PUFFSHROOM, P_SUNSHROOM, P_FUMESHROOM, P_GRAVEBUSTER, P_HYPNOSHROOM, P_SCAREDYSHROOM, P_ICESHROOM, P_DOOMSHROOM,
  P_LILYPAD, P_SQUASH, P_THREEPEATER, P_TANGLEKELP, P_JALAPENO, P_SPIKEWEED, P_TORCHWOOD, P_TALLNUT,
  P_SEASHROOM, P_PLANTERN, P_CACTUS, P_BLOVER, P_SPLITPEA, P_STARFRUIT, P_PUMPKIN, P_MAGNETSHROOM,
  P_CABBAGEPULT, P_FLOWERPOT, P_KERNELPULT, P_COFFEEBEAN, P_GARLIC, P_UMBRELLA, P_MARIGOLD, P_MELONPULT,
  P_EXPLODENUT,   /* the red wall-nut of Wall-nut Bowling */
  N_PLANTS,
  N_SEEDS = P_EXPLODENUT
};
enum Zombie_ {
  Z_NORMAL, Z_FLAG, Z_CONE, Z_POLE, Z_BUCKET, Z_NEWSPAPER, Z_DOOR, Z_FOOTBALL, Z_DANCER, Z_BACKUP, Z_DUCKY,
  Z_SNORKEL, Z_ZOMBONI, Z_BOBSLED, Z_DOLPHIN, Z_JACK, Z_BALLOON, Z_DIGGER, Z_POGO, Z_BUNGEE, Z_LADDER, Z_CATAPULT,
  Z_GARGANTUAR, Z_IMP,
  N_ZOMBIES
};
enum { ENV_DAY, ENV_NIGHT, ENV_POOL, ENV_FOG, ENV_ROOF };
enum { MODE_LAWN, MODE_BOWLING, MODE_CONVEYOR, MODE_WHACK, MODE_VASE };
#define N_LEVELS 50

/* ---------------------------------------------------------------- facts (defs.c) */
typedef struct {
  const char *name;
  uint16_t cost;
  uint16_t cooldown;      /* ticks */
  int16_t health;
  uint8_t mushroom;       /* sleeps in the day */
  uint8_t anim;           /* (index into plant_anims) */
} PlantDef;
extern const PlantDef plant_defs[N_PLANTS];
extern const char *const plant_blurbs[N_SEEDS];     /* the Almanac's */
extern const char *const plant_stats[N_SEEDS];

typedef struct {
  const char *name;
  uint8_t value, starting, first_wave;
  uint16_t weight;
  uint64_t allowed;       /* bit l: allowed in level l + 1 */
} ZombieDef;
extern const ZombieDef zombie_defs[N_ZOMBIES];
extern const char *const zombie_blurbs[N_ZOMBIES];
extern const char *const zombie_stats[N_ZOMBIES];

typedef struct {
  uint8_t env, mode, rows;    /* rows: a bit for each row zombies come in */
  uint16_t sun;               /* sun to start with */
  uint8_t waves;
  int8_t reward;              /* the plant found at the end, or REWARD_* */
  uint8_t nseeds;
  uint8_t seeds[10];          /* the seed packets of a level without a choice, or the conveyor's plants */
  uint8_t weights[10];        /* the conveyor's */
} LevelDef;
enum { REWARD_NONE = -1, REWARD_SHOVEL = -2, REWARD_ALMANAC = -3, REWARD_KEYS = -4, REWARD_MONEY = -5,
       REWARD_TROPHY = -6 };
extern const LevelDef level_defs[N_LEVELS];
extern const uint8_t grave_columns[N_LEVELS][6][2];

/* ---------------------------------------------------------------- the lawn */
enum {
  PF_AWAKE = 1,       /* a mushroom woken by coffee */
  PF_SCARED = 2,      /* a scaredy-shroom hiding */
  PF_TALL = 4,        /* a cactus stretched up for balloons */
  PF_GROWN = 8,       /* a sun-shroom grown up */
};
typedef struct {
  uint16_t id;
  uint8_t kind, row, col, flags;
  uint8_t state;          /* chomper: CH_*; squash: jumping; kelp: holding */
  uint8_t pad;
  int16_t health;
  int16_t action;         /* ticks to its next action */
  int16_t aux;            /* chomper, squash, kelp: ticks left; split pea: its rear pea; potato mine: armed */
  uint16_t target;        /* the zombie a squash or kelp is after */
  float tx;               /* where a squash lands */
  uint32_t born, shot;    /* ticks: planted, last shot */
} Plant;
enum { CH_READY, CH_BITE, CH_DIGEST, CH_RECOVER };

enum {
  ZF_HYPNO = 1, ZF_MINI = 2, ZF_FLYING = 4, ZF_DOLPHIN = 8, ZF_POGO = 16, ZF_UNDERGROUND = 32, ZF_BACKWARDS = 64,
  ZF_METAL_GONE = 128, ZF_POLE = 256, ZF_EATING = 512, ZF_OPENING = 1024, ZF_IN_WATER = 2048, ZF_HEADLESS = 4096,
  ZF_PAPER_GONE = 8192, ZF_SPAWNED = 16384
};
enum { DP_INTRO, DP_POINT, DP_HOLD, DP_DANCE };            /* the dancer */
enum { BP_DOWN, BP_WAIT, BP_UP };                          /* the bungee */
enum { CP_WALK, CP_SHOOT, CP_RELOAD };                     /* the catapult */
enum { GP_WALK, GP_SMASH, GP_THROW };                      /* the gargantuar */
typedef struct {
  uint16_t id;
  uint8_t kind, row;
  uint16_t flags;
  uint8_t wave;
  int8_t bite;            /* ticks to its next bite */
  float x, speed;
  float walk;             /* pixels walked (its walk's animation) */
  float anim;             /* ticks of its other animations */
  int16_t health, armor, shield;
  int16_t chill, frozen, butter, rise;
  float vault, rage;      /* ticks left of a pole vault, a newspaper's rage */
  int16_t garlic, ladder_in, emerge, landing, pop, blown, crash;
  uint8_t garlic_row, phase;
  uint32_t hit_at;        /* tick of the last hit: a flash */
  union {
    struct { int16_t in; } jack;
    struct { uint16_t leader; uint8_t slot, pad; int16_t dance_in, summon_in; } dance;
    struct { uint8_t column, drop; int16_t left; } bungee;
    struct { uint8_t ammo, pad; int16_t left; } catapult;
    struct { uint8_t thrown, pad; int16_t pad2; float left; } giant;
    struct { int16_t left, duration; float sx, ex; } flight;
    struct { uint16_t sled, leader; } sled;
  } u;
} Zombie;

enum { S_PEA, S_ICE, S_FIRE, S_PUFF, S_SPIKE, S_STAR, S_CABBAGE, S_KERNEL, S_BUTTER, S_MELON, S_BASKETBALL };
enum { SF_HIGH = 1, SF_LONG = 2, SF_DONE = 4, SF_ROOFCHECK = 8 };
typedef struct {
  uint16_t id;
  uint8_t kind, row, flags;
  uint8_t torch;          /* the column of the torchwood that lit it, 0xFF none */
  int16_t age;
  float x, y, vx, vy;     /* (y only for stars, lobs, and the roof's straight shots: their start's height) */
  /* lobbed: */
  uint16_t target;
  int16_t duration;
  float sx, ex, sy, ey;
} Shot;

typedef struct { float x, y, ty; int16_t left; uint8_t value, flags; } Sun;      /* flags: 1 from the sky */
typedef struct { float x, y, ty; int16_t left; uint16_t value; } Coin;
typedef struct { uint16_t id; uint8_t kind, row; float x, y; int8_t dir; uint8_t nhit; uint16_t hit[6]; } Roller;
typedef struct { uint8_t row, col; } Cell;
typedef struct { uint8_t row, col; int16_t left; } Crater;
typedef struct { float x; int16_t left; } Ice;
typedef struct { float x; uint8_t state; } Mower;           /* 0 ready, 1 moving, 2 gone */
typedef struct { uint8_t kind; float x; } Packet;
typedef struct { uint8_t row, col, green, zombie, kind; } Vase;
typedef struct { uint8_t kind, row, col, times; int16_t left; } Drop;

/* what is only seen: the dead, explosions, splats */
enum { FX_CORPSE, FX_BOOM, FX_DOOM, FX_FIRE, FX_FREEZE, FX_SPLAT, FX_DIRT, FX_SPLASH, FX_SUNFLY, FX_COINFLY,
       FX_SMOKE, FX_TEXT, FX_HEAD, FX_POW, FX_ZZZ };
typedef struct {
  uint8_t kind, flags;    /* flags: GFX_FLIP, 2 mini, 4 burnt */
  uint16_t anim;          /* a corpse's animation */
  int16_t tint;
  float x, y;
  int16_t t, life;        /* ticks */
  int16_t v;              /* a row, a value */
} Fx;

#define MAX_PLANTS 150
#define MAX_ZOMBIES 100
#define MAX_SHOTS 120
#define MAX_SUNS 40
#define MAX_COINS 24
#define MAX_FX 72
#define MAX_ROLLERS 12

enum { PH_INTRO, PH_PLAY, PH_WON, PH_LOST };
typedef struct {
  /* the level */
  uint8_t level, env, mode, nrows, rows;       /* rows: lanes zombies use */
  uint8_t phase;
  uint32_t ticks;
  uint32_t rng;
  int32_t sun;
  uint32_t money;                              /* coins picked up this level */
  uint16_t next_id;
  /* seeds */
  uint8_t nseeds, seeds[10];
  int16_t cooldown[10];
  uint8_t npackets;                            /* the conveyor */
  Packet packets[10];
  int16_t conveyor_in;
  uint8_t last_conveyor;
  /* waves */
  uint8_t wave, nwaves;
  uint8_t waves[50][24];                       /* the zombies of each wave, 0xFF ends */
  int16_t wave_in, flag_warning;
  uint32_t wave_elapsed;
  int32_t wave_threshold;
  int16_t sun_in;
  uint16_t suns_fallen;
  int16_t grave_wave_in;
  uint8_t row_last[MAX_ROWS], row_second[MAX_ROWS];
  int8_t row_mowed[MAX_ROWS];
  uint8_t started;
  uint8_t tutorial;                            /* TUT_* */
  uint8_t first_coin;
  int16_t storm_in;
  uint8_t storm_flash;
  int16_t fog_blown, pool_frozen;
  Mower mowers[MAX_ROWS];
  uint8_t ngraves, ncraters, nladders, nvases, ndrops;
  Cell graves[16];
  Crater craters[16];
  Cell ladders[24];
  Ice ice[MAX_ROWS];
  Vase vases[45];
  Drop drops[30];
  uint8_t vase_stage;
  /* whack-a-zombie */
  int16_t whack_spawn_in;
  uint8_t whack_final;
  int16_t hammer_t;
  /* stats */
  uint16_t kills, planted;
  /* the last zombie's place: where the reward drops */
  float last_x, last_y;
  uint8_t has_boss;
  struct { uint8_t on, row, col; int16_t age; } rake;   /* on: 1 waiting, 2 a zombie on its way */
  /* the things on the lawn (a level's save keeps what is before them, then them as many as there are) */
  int nplants, nzombies, nshots, nsuns, ncoins, nfx, nrollers;
  Plant plants[MAX_PLANTS];
  Zombie zombies[MAX_ZOMBIES];
  Shot shots[MAX_SHOTS];
  Sun suns[MAX_SUNS];
  Coin coins[MAX_COINS];
  Fx fx[MAX_FX];
  Roller rollers[MAX_ROLLERS];
} Board;

extern Board B;

/* ---------------------------------------------------------------- board.c */
float row_y(int row, float x);               /* the ground's height in a row, at x */
float cell_x(int col);                        /* a cell's middle */
int row_at(float y, float x);
int col_at(float x);
float cell_h(void);
bool water_row(int row);
bool is_night(void);
bool is_pool(void);
uint32_t rnd(void);
float frand(void);                            /* 0 to 1 */
int irand(int n);                             /* 0 to n - 1 */
void board_start(int level);
void board_tick(void);
bool board_plant(int seed, int col, int row);   /* seed: the packet's index (or the conveyor's) */
int plant_issue(int kind, int col, int row);    /* 0 fine, else why not (ISSUE_*) */
enum { ISSUE_NONE, ISSUE_PLACE, ISSUE_OCCUPIED, ISSUE_SUN, ISSUE_RECHARGE, ISSUE_WATER, ISSUE_LILY, ISSUE_POT,
       ISSUE_ROOF, ISSUE_GRAVE, ISSUE_CRATER, ISSUE_COFFEE, ISSUE_BOWLING, ISSUE_ICE };
extern const char *const issue_text[];
bool board_dig(int col, int row);
Plant *dig_target(int col, int row);
int board_collect(float x, float y, float r);   /* picks up sun and coins near a point; returns how many */
bool board_whack(float x, float y);
void board_break_vase(int col, int row);
int plant_cost(int kind);
bool sleeping(const Plant *p);
uint16_t new_id(void);
Plant *plant_at(int col, int row, int kind);    /* -1: any but lily pads, pots and pumpkins */
Zombie *zombie_by_id(uint16_t id);
void fx_add(int kind, float x, float y, int life, int v);
Fx *fx_corpse(const Zombie *z, int anim, int life, int burnt);
void add_sun(float x, float y, float ty, int value, bool sky);
void add_coin(float x, float y, int value);
bool board_won(void);
int seed_kind(int slot);                        /* the plant of a packet */
bool has_conveyor(void);
int waves_flag_every(void);
bool is_flag_wave(int wave);
void board_drop_reward(void);
void board_set_seeds(const uint8_t *seeds, int n);
void board_next_vase_stage(void);
void whack_graves(int count);

/* ---------------------------------------------------------------- zombies.c */
Zombie *spawn_zombie(int kind, int row, float x, int wave);
void zombies_tick(void);
void damage_zombie(Zombie *z, int amount, bool through_shield);
bool zombie_flying(const Zombie *z);
bool zombie_submerged(const Zombie *z);
bool zombie_headless(const Zombie *z);
float walking_speed(const Zombie *z);
int zombie_total_health(const Zombie *z);
void kill_zombie(Zombie *z, int how);
enum { DIE_NORMAL, DIE_BURN, DIE_GONE, DIE_MOWED, DIE_SQUASH };
void drop_from_sky(int kind, int row, int wave);

/* ---------------------------------------------------------------- plants.c */
void plants_tick(void);
void shots_tick(void);
void plant_new(Plant *p, int kind, int col, int row);

/* ---------------------------------------------------------------- boss.c */
#define BOSS_HEALTH 40000
enum { BS_ENTER, BS_IDLE, BS_SPAWN, BS_STOMP, BS_BUNGEE, BS_BUNGEE_WAIT, BS_BUNGEE_LEAVE, BS_HEAD_ENTER, BS_HEAD_BEFORE,
       BS_SPIT, BS_RV, BS_HEAD_AFTER, BS_HEAD_LEAVE, BS_DEATH };
typedef struct {
  int32_t health;
  uint8_t phase, clip, mode, row, column, ball_kind;
  float elapsed, duration;
  uint8_t fps;
  int32_t age, anim;
  int16_t summon_in, head_in, stomp_in, bungee_in, chill, frozen;
  uint32_t hit_at;
  struct { uint8_t on, kind, row; float x; int16_t age; } ball;
} Boss;
extern Boss boss;
void boss_start(void);
void boss_tick(void);
bool boss_vulnerable(void);
void boss_damage(int amount);
void boss_draw(void);
#endif
