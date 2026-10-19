/* Cookie Clicker: the game's state and rules (logic.c and the files beside it), as the game's own
 * main.js has them (version 2.058). Times are in frames of 1/30 of a second, as the game counts. */
#ifndef CC_GAME_H
#define CC_GAME_H
#include <stdbool.h>
#include <stdint.h>
#include "plat.h"
#include "res.h"

#define FPS 30
#define NB N_BUILDINGS
#define MAX_SHIMMERS 48
#define MAX_BUFFS 12
#define MAX_PARTICLES 48
#define MAX_TEXT_PARTICLES 12
#define MAX_NOTES 4
#define MAX_POPUPS 4
#define MAX_WRINKLERS 14
#define UBITS ((N_UPGRADES + 7) / 8)
#define ABITS ((N_ACHIEVS + 7) / 8)

/* the dragon's auras and the Pantheon's gods, in the game's order */
enum {
  AURA_NONE, AURA_BREATH_OF_MILK, AURA_DRAGON_CURSOR, AURA_ELDER_BATTALION, AURA_REAPER_OF_FIELDS,
  AURA_EARTH_SHATTERER, AURA_MASTER_OF_THE_ARMORY, AURA_FIERCE_HOARDER, AURA_DRAGON_GOD, AURA_ARCANE_AURA,
  AURA_DRAGONFLIGHT, AURA_ANCESTRAL_METAMORPHOSIS, AURA_UNHOLY_DOMINION, AURA_EPOCH_MANIPULATOR,
  AURA_MIND_OVER_MATTER, AURA_RADIANT_APPETITE, AURA_DRAGONS_FORTUNE, AURA_DRAGONS_CURVE, AURA_REALITY_BENDING,
  AURA_DRAGON_ORBS, AURA_SUPREME_INTELLECT, AURA_DRAGON_GUTS, N_AURAS
};
enum {
  GOD_ASCETICISM, GOD_DECADENCE, GOD_RUIN, GOD_AGES, GOD_SEASONS, GOD_CREATION, GOD_LABOR, GOD_INDUSTRY,
  GOD_MOTHER, GOD_SCORN, GOD_ORDER, N_GODS
};

/* the buffs golden cookies, spells and the rest give (the game's buffTypes) */
enum {
  BUFF_FRENZY, BUFF_ELDER_FRENZY, BUFF_CLOT, BUFF_DRAGON_HARVEST, BUFF_EVERYTHING_MUST_GO, BUFF_CURSED_FINGER,
  BUFF_CLICK_FRENZY, BUFF_DRAGONFLIGHT, BUFF_COOKIE_STORM, BUFF_BUILDING_BUFF, BUFF_BUILDING_DEBUFF,
  BUFF_SUGAR_BLESSING, BUFF_HAGGLER_LUCK, BUFF_HAGGLER_MISERY, BUFF_PIXIE_LUCK, BUFF_PIXIE_MISERY, BUFF_MAGIC_ADEPT,
  BUFF_MAGIC_INEPT, BUFF_DEVASTATION, BUFF_SUGAR_FRENZY, BUFF_LOAN1, BUFF_LOAN1_INTEREST, BUFF_LOAN2,
  BUFF_LOAN2_INTEREST, BUFF_LOAN3, BUFF_LOAN3_INTEREST, N_BUFF_TYPES
};
typedef struct {
  uint8_t type, building, used, pad;
  int32_t time, max_time;
  float arg1;
  double power, mult_cps, mult_click;   /* (1 when the buff has none) */
} Buff;

/* the golden cookie's effects, and its forced kinds */
enum {
  GC_NONE, GC_FRENZY, GC_LUCKY, GC_RUIN, GC_BLOOD_FRENZY, GC_CLOT, GC_CLICK_FRENZY, GC_CURSED_FINGER, GC_CHAIN,
  GC_STORM, GC_BUILDING_SPECIAL, GC_DRAGON_HARVEST, GC_DRAGONFLIGHT, GC_SUGAR_LUMP, GC_BLAB, GC_STORM_DROP,
  GC_EVERYTHING_MUST_GO, N_GC
};
enum { SHIMMER_GOLDEN, SHIMMER_REINDEER };
typedef struct {
  uint8_t type, wrath, spawn_lead, force, used, pad[3];
  int32_t id, life, max_life;
  float x, y, dur, size_mult;
} Shimmer;

typedef struct {
  int amount, bought, highest, free, level, muted;   /* (int: 32 bits, here and on computers) */
  double total_cookies;
  /* computed */
  double stored_cps, stored_total_cps;
  bool locked;
} Building;

typedef struct {
  float x, y, xd, yd, r, size;
  int16_t life, dur, pic, kind;
} Particle;
typedef struct {
  float x, y, yd;
  int16_t life, dur;
  char text[24];
} TextParticle;

/* the game's notes (bottom of the screen) and popups (texts that float up) */
typedef struct {
  char title[48];
  char desc[96];
  int16_t icon;      /* ICON(x, y), or -1 */
  int32_t life;      /* frames left, or -1 to stay until closed */
  int32_t age;
} Note;
typedef struct {
  char text[64];
  float x, y;
  int32_t life;
} Popup;

typedef struct {
  uint8_t phase, type, selected, pad;   /* phase: 0 none, 1 coming, 2 eating; type: 1 shiny */
  uint16_t clicks, pad2;
  float close, hp, hurt, r, x, y;       /* how near the cookie it is (0..1), where (computed) */
  double sucked;                        /* the cookies it ate */
} Wrinkler;

/* the preferences (Options) */
typedef struct {
  uint8_t particles, numbers, milk, fancy, cursors, wobbly, not_scary, format_long, monospace, notes_on;
  uint8_t short_numbers, extra_buttons, focus, warn, pad[2];
} Prefs;

/* everything that is saved */
typedef struct {
  uint32_t magic, version;
  double cookies, cookies_earned, cookies_reset, cookies_sucked, handmade_cookies, cookie_clicks;
  double prestige, heavenly_chips, heavenly_chips_spent, cookies_ps_raw_highest;
  unsigned golden_clicks, golden_clicks_local, missed_golden_clicks, reindeer_clicked, wrinklers_popped, resets;
  unsigned ticker_clicks, season_uses, pledges, spells_cast, spells_cast_total;
  int32_t elder_wrath, pledge_t, research_t, next_research, season_t, golden_time, golden_min, golden_max;
  int32_t reindeer_time, reindeer_min, reindeer_max, golden_chain;
  double golden_chain_total;
  uint8_t season, ascension_mode, dragon_level, dragon_aura, dragon_aura2, santa_level, golden_last, golden_spawned;
  uint8_t reindeer_spawned, milk_type, bg_type, bakery_name1, bakery_name2, seen_keys, seen_ascend, on_ascend;
  uint16_t permanent[5], pad2;
  double lumps, lumps_total;
  double lump_t, lump_refill;   /* play time (ms) the current lump started, the refill was used */
  uint8_t lump_type, pad3[7];
  double start_date, full_date, play_time;   /* play time (ms): this ascension began, the game began, now */
  Building buildings[NB];
  uint8_t upgrades_unlocked[UBITS], upgrades_bought[UBITS], upgrades_vaulted[UBITS], achievements[ABITS];
  Buff buffs[MAX_BUFFS];
  Wrinkler wrinklers[MAX_WRINKLERS];
  Prefs prefs;
  /* the minigames */
  float grimoire_magic;
  uint8_t on_minigame, pad5[3];        /* the minigame shown (its building + 1), 0 none */
  uint8_t gods[3], swaps;
  int32_t swap_t;
  uint8_t garden[6][6][2], garden_soil, garden_freeze;
  uint16_t garden_seeds[3];
  double garden_next_step, garden_next_soil;
  uint32_t garden_harvests, garden_harvests_total;
} SaveState;

typedef struct {
  SaveState s;
  /* the rest is computed, or lives only while the game runs */
  double cookies_ps, global_cps_mult, computed_mouse_cps, unbuffed_cps, building_cps, cookies_ps_raw;
  double cps_sucked, cookies_d;           /* the cookies shown, catching up with the real ones */
  float milk_progress, milk_h, milk_hd;
  int achievements_owned, upgrades_owned, buildings_owned;
  uint32_t T, draw_t;                     /* logic frames, drawn frames */
  float real_t;
  bool recalculate, store_dirty, upgrades_dirty;
  /* the store's upgrades, sorted as the game sorts them */
  uint16_t store[N_UPGRADES];
  int nstore, nstore_toggle, nstore_tech;
  int buy_mode, buy_bulk;                 /* 1 buy / -1 sell; 1, 10, 100, -1 all */
  /* the big cookie */
  int big_state;                          /* 0 normal, 1 pressed, 2 released */
  float big_size, big_size_d, big_size_t;
  int32_t click_t;                        /* frames since the last click */
  Shimmer shimmers[MAX_SHIMMERS];
  int nshimmers_golden, nshimmers_reindeer;
  int32_t shimmers_id;
  Particle particles[MAX_PARTICLES];
  TextParticle texts[MAX_TEXT_PARTICLES];
  Note notes[MAX_NOTES];
  Popup popups[MAX_POPUPS];
  float effs[16];                         /* the minigames' effects (EFF_*) */
  /* ascending */
  bool on_ascend;
  int32_t ascend_timer, reincarnate_timer;
  uint32_t seed;
} GameState;
extern GameState G;
#define S (G.s)

enum { EFF_CPS, EFF_CLICK, EFF_GRANDMA_CPS, EFF_CURSOR_CPS, EFF_MILK, EFF_GOLDEN_FREQ, EFF_GOLDEN_DUR, EFF_GOLDEN_EFF_DUR,
       EFF_GOLDEN_GAIN, EFF_WRATH_FREQ, EFF_WRATH_DUR, EFF_WRATH_EFF_DUR, EFF_WRATH_GAIN, EFF_BUILDING_COST,
       EFF_UPGRADE_COST, EFF_ITEM_DROPS };
static inline float eff(int i) { return G.effs[i]; }

/* the game's random numbers */
double rnd(void);                         /* 0 <= x < 1 */
int rnd_int(int n);

/* upgrades and achievements */
static inline bool bit_get(const uint8_t *b, int i) { return b[i >> 3] >> (i & 7) & 1; }
static inline void bit_set(uint8_t *b, int i, bool v) {
  if (v) b[i >> 3] |= (uint8_t)(1 << (i & 7));
  else b[i >> 3] &= (uint8_t)~(1 << (i & 7));
}
bool has(int u);                          /* bought (Game.Has) */
static inline bool has_unlocked(int u) { return bit_get(S.upgrades_unlocked, u); }
static inline bool has_achiev(int a) { return bit_get(S.achievements, a); }
void unlock(int u);
void lock(int u);
void win(int a);
void earn_upgrade(int u);                 /* bought without spending */
double upgrade_price(int u);
bool upgrade_can_buy(int u);
bool upgrade_buy(int u, bool confirmed);  /* false: not bought */
bool upgrade_needs_confirm(int u);
bool counts_as_upgrade_owned(int u);

/* buildings */
double building_price(int b);             /* the next one */
double building_sum_price(int b, int n);  /* n more */
double building_reverse_sum_price(int b, int n);
void building_buy(int b, int n);
void building_sell(int b, int n);
void building_bulk(int b, double *price, int *n);   /* what the store's mode would buy or sell, and for how much */

/* cookies */
void earn(double v);
void spend(double v);
void dissolve(double v);
void click_cookie(void);
void calculate_gains(void);
double tiered_cps_mult(int b);
int hasgod(int god);                      /* the Pantheon: 0, or the slot (1..3) a god is in */
float aura_mult(int aura);
bool has_buff(int type);
Buff *find_buff(int type);
Buff *gain_buff(int type, float time, double pow, int building);
void kill_buff(int type);
double drop_rate_mult(void);

/* the game */
void game_new(void);
void game_logic(void);                    /* a frame */
void game_rebuild_upgrades(void);
void game_check(void);                    /* the checks the game makes every 5 seconds */
void notify(const char *title, const char *desc, int icon, int seconds);
void popup(const char *text, float x, float y);
void particle_add(float x, float y, float xd, float yd, float size, int dur, int kind);
void text_particle_add(float x, float y, const char *text);

/* shimmers (shimmer.c) */
void shimmers_reset(void);
void shimmers_update(void);
Shimmer *shimmer_new(int type, int force, bool no_count);
void shimmer_pop(Shimmer *m);
int shimmer_next(void);                   /* the one shift pops (the oldest), -1 none */
int32_t golden_min_time(void), golden_max_time(void);
void buffs_update(void);

/* numbers, as the game writes them */
void beautify(double v, int floats, char *out);
void say_time(double frames, int detail, char *out);
/* "%1 cookie(s)" */
void cookies_str(double v, int floats, char *out);

/* where things are on the screen (ui.c) */
#define NEWS_H 20
#define LEFT_W 142
#define STORE_X 146
#define STORE_W (SCREEN_W - STORE_X)
#define COOKIE_X 71
#define COOKIE_Y 116
#endif
