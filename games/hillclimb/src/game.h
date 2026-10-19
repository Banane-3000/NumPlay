/* Hill Climb Racing: the game's screens and the run being driven. */
#ifndef HC_GAME_H
#define HC_GAME_H
#include <stdbool.h>
#include <stdint.h>
#include "car.h"
#include "stage.h"
#include "vehicle.h"

enum { MODE_TITLE, MODE_HELP, MODE_GARAGE, MODE_STAGES, MODE_RUN, MODE_PAUSE, MODE_END };

/* what is kept (save.c) */
typedef struct {
  uint32_t coins;
  uint16_t vehicles, stages;            /* bits: owned */
  uint8_t level[NVEHICLES][4];          /* each upgrade's level (1 and up) */
  uint16_t best[NSTAGES][NVEHICLES];    /* meters */
  uint8_t vehicle, stage, seen_help, pad;
  uint32_t runs, meters, flips, best_air_ms, coins_earned;
} Save;
extern Save sv;

enum { END_DRIVER_DOWN, END_OUT_OF_FUEL, END_QUIT };

typedef struct {
  char text[24];
  uint32_t value;
  uint16_t t;           /* frames shown so far */
  uint8_t big;
} Popup;

typedef struct {
  float x, y, vx, vy, life, size;
  uint8_t kind;          /* 0 the stage's dirt, 1 smoke, 2 a coin's sparkle */
} Particle;

#define NPOPUPS 4
#define NPARTICLES 40

typedef struct {
  Car car;
  int vehicle, stage;
  float fuel, fuel_max;          /* seconds */
  uint32_t coins, bonus;         /* collected, and from tricks and levels */
  float start_x, dist, best;     /* meters: the farthest reached this run, the best before it */
  int level;                     /* levels done */
  float air_t, air_rot;          /* in the air: for how long, turned how far (radians) */
  int air_flips, neck_ready;
  uint32_t air_value;            /* what the air time is worth so far */
  bool over;
  int end;                       /* END_* */
  int over_t;                    /* frames since */
  int empty_t;                   /* frames stopped without fuel */
  bool new_best;
  float cam_x, cam_y, cam_s;
  uint32_t frame;
  float gas, brake;              /* the pedals, eased */
  uint16_t got[COIN_GROUPS];     /* coins taken: a bit each */
  uint8_t fuel_got[512];         /* fuel cans taken */
  Popup pop[NPOPUPS];
  Particle part[NPARTICLES];
  int npart;
  uint32_t flips;
  uint32_t air_best_ms;
} Run;
extern Run run;

extern int g_mode;
extern bool g_running;
extern uint32_t g_frame;

void game_init(void);
void game_frame(uint32_t keys);
void game_draw(void);
void game_quit(void);               /* Home or On/Off: save and stop */

void run_start(int vehicle, int stg);
void run_frame(uint32_t keys);
void run_draw(bool hud);
void run_finish(void);              /* bank the run's coins and best */
uint32_t run_total(void);           /* coins collected and earned */

/* menus (menu.c) */
void menu_frame(uint32_t keys, uint32_t pressed);
void menu_draw(void);
void menu_enter(int mode);

/* saves (save.c) */
void save_load(void);
bool save_write(void);
void save_restore_copy(void);

/* drawing helpers shared by the screens */
void draw_coins_counter(int x, int y, uint32_t n, int align);
void format_number(char *out, uint32_t n);   /* 1,234,567 */
#endif
