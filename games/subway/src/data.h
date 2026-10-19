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
extern const Image im_key;
extern const Image im_double;
extern const Image im_magnet;
extern const Image im_jetpack;
extern const Image im_mystery;
extern const Image im_board;
extern const Image im_headstart;
extern const Image im_booster;
extern const Image im_sneakers;
extern const Image im_coin_s;
extern const Image im_key_s;
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
};
#endif

