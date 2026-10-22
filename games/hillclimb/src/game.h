/* Hill Climb Racing: the game's screens and the run being driven. */
#ifndef HC_GAME_H
#define HC_GAME_H
#include <stdbool.h>
#include <stdint.h>
#include "car.h"
#include "stage.h"
#include "vehicle.h"

enum { MODE_TITLE, MODE_HELP, MODE_GARAGE, MODE_STAGES, MODE_RUN, MODE_PAUSE, MODE_END };

/* what is kept (save.c): hillclimb.sav, version 3 (13 vehicles and 13 stages); versions 1 and 2 (8 vehicles, 10
 * stages) are read and carried over */
#define SAVE_VERSION 3
typedef struct {
  uint16_t version, pad0;
  uint32_t coins;
  uint16_t vehicles, stages;            /* bits: owned */
  uint8_t level[NVEHICLES][4];          /* each upgrade's level (1 and up) */
  uint16_t best[NSTAGES][NVEHICLES];    /* meters */
  uint8_t vehicle, stage, seen_help, pad;
  uint32_t runs, meters, flips, best_air_ms, coins_earned;
  uint8_t stage_level[NSTAGES];         /* each stage's level, from 0 (the game keeps them from run to run) */
  uint8_t pad2[3];
} Save;
extern Save sv;

enum { END_DRIVER_DOWN, END_OUT_OF_FUEL, END_QUIT };

typedef struct {
  char text[24];
  uint32_t value;
  uint16_t t;           /* frames shown so far */
  uint16_t seq;         /* the order they came in */
  uint8_t big, wait;    /* (frames before it shows) */
} Popup;

typedef struct {
  float x, y, vx, vy, life, size;
  uint8_t kind;          /* 0 the stage's dirt, 1 smoke, 2 a coin's sparkle */
} Particle;

#define NPOPUPS 5
#define NPARTICLES 40

typedef struct {
  Car car;
  int vehicle, stage;
  float fuel, burn;              /* the game's fuel: 100 full, burning `burn` a second */
  uint32_t coins, bonus;         /* collected, and from tricks and levels */
  float start_x, dist, best;     /* meters: the farthest reached this run, the best before it */
  int level;                     /* the stage's level (from 0), and how many this run completed */
  int levels_done;
  float t;                       /* seconds since the start */
  float air_best;                /* this time in the air: the longest it has been (seconds) */
  int air_step;                  /* the air time's step paid so far */
  uint32_t air_value;            /* what this air time has paid so far */
  float down_t, empty_t;         /* seconds since Bill went down, since the fuel ran out */
  bool over;
  int end;                       /* END_* */
  int over_t;                    /* frames since */
  bool new_best;
  float cam_x, cam_y, cam_s, zoom;
  uint32_t frame;
  float gas, brake;              /* the pedals (drawn pressed) */
  uint16_t got[COIN_GROUPS];     /* coins taken: a bit each */
  uint8_t fuel_got[64];          /* fuel cans taken */
  Popup pop[NPOPUPS];
  uint16_t pop_seq;
  Particle part[NPARTICLES];
  int npart;
  uint32_t flips;
  uint32_t air_best_ms;
} Run;
extern Run run;

extern int g_mode;
extern bool g_running, g_nohud;
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
