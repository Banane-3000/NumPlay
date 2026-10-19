/* Subway Surfers: pictures and models (data.c, made by tools/pack.py). */
#ifndef SS_DATA_H
#define SS_DATA_H
#include "models.h"

extern const Model model_jake;
extern const Model model_jake_lo;
extern const Model model_tricky;
extern const Model model_tricky_lo;
extern const Model model_fresh;
extern const Model model_fresh_lo;
extern const Model model_spike;
extern const Model model_spike_lo;
extern const Model model_inspector;
extern const Model model_inspector_lo;
extern const Model model_dog;
extern const Image im_coin;
extern const Image im_coin_s;
extern const Image im_key;
extern const Image im_key_s;
extern const Image im_board;
extern const Image im_board_s;
extern const Image im_jetpack;
extern const Image im_jetpack_s;
extern const Image im_sneakers;
extern const Image im_sneakers_s;
extern const Image im_magnet;
extern const Image im_magnet_s;
extern const Image im_double;
extern const Image im_double_s;
extern const Image im_mystery;
extern const Image im_mystery_s;
extern const Image im_headstart;
extern const Image im_headstart_s;
extern const Image im_booster;
extern const Image im_booster_s;
extern const Image im_logo;

enum {
  TX_JAKE_BODY = 0,
  TX_JAKE_HEAD = 1,
  TX_TRICKY_BODY = 2,
  TX_TRICKY_HEAD = 3,
  TX_FRESH_BODY = 4,
  TX_FRESH_HEAD = 5,
  TX_SPIKE_BODY = 6,
  TX_SPIKE_HEAD = 7,
  TX_INSPECTOR_BODY = 8,
  TX_INSPECTOR_HEAD = 9,
  TX_COIN = 10,
  TX_KEY = 11,
  TX_MAGNET = 12,
  TX_JETPACK = 13,
  TX_DOUBLE = 14,
  TX_MYSTERY = 15,
  TX_SNEAKERS = 16,
  TX_TRAIN_A_SIDE = 17,
  TX_TRAIN_B_SIDE = 18,
  TX_TRAIN_C_SIDE = 19,
  TX_TRAIN_A_FRONT = 20,
  TX_TRAIN_B_FRONT = 21,
  TX_TRAIN_C_FRONT = 22,
  TX_BOXCAR_SIDE = 23,
  TX_BOXCAR_END = 24,
  TX_WAGON_SIDE = 25,
  TX_WAGON_END = 26,
  TX_CARGO = 27,
  TX_ROOF_A = 28,
  TX_ROOF_B = 29,
  TX_ROOF_C = 30,
  TX_ROOF_BOX = 31,
  TX_ARCH_WALL = 32,
  TX_CONCRETE = 33,
  TX_TREE = 34,
  TX_GRAFFITI_0 = 35,
  TX_GRAFFITI_1 = 36,
  TX_GRAFFITI_2 = 37,
  TX_GRAFFITI_3 = 38,
  TX_BARRIER = 39,
  TX_CHEVRONS = 40,
  TX_RAMP = 41,
  TX_TUNNEL = 42,
};
#endif

