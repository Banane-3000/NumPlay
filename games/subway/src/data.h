/* Subway Surfers: the game's models, animations and pictures (data.c, made by tools/apk.py). */
#ifndef SS_DATA_H
#define SS_DATA_H
#include "models.h"

#define RB_ROOT 1
#define RB_HIPS 2
#define RB_LOWERSPINE 3
#define RB_HEAD 4
#define RB_L_CLAVICLE 5
#define RB_L_SHOULDER 6
#define RB_L_ELBOW 7
#define RB_L_HAND 8
#define RB_R_CLAVICLE 9
#define RB_R_SHOULDER 10
#define RB_R_ELBOW 11
#define RB_R_HAND 12
#define RB_L_LEG 13
#define RB_L_KNEE 14
#define RB_L_FOOT 15
#define RB_R_LEG 16
#define RB_R_KNEE 17
#define RB_R_FOOT 18
#define RB_SPINE RB_LOWERSPINE
extern const Clip clip_runner_run;
extern const Clip clip_runner_jump;
extern const Clip clip_runner_hang;
extern const Clip clip_runner_land;
extern const Clip clip_runner_roll;
extern const Clip clip_runner_dodge_l;
extern const Clip clip_runner_dodge_r;
extern const Clip clip_runner_stumble_l;
extern const Clip clip_runner_stumble_r;
extern const Clip clip_runner_stumble;
extern const Clip clip_runner_death;
extern const Clip clip_runner_death_train;
extern const Clip clip_runner_board;
extern const Clip clip_runner_board_jump;
extern const Clip clip_runner_board_hang;
extern const Clip clip_runner_board_land;
extern const Clip clip_runner_board_roll;
extern const Clip clip_runner_board_l;
extern const Clip clip_runner_board_r;
extern const Clip clip_runner_fly;
extern const Clip clip_runner_fly_l;
extern const Clip clip_runner_fly_r;
extern const Clip clip_runner_idle;
extern const Clip clip_runner_intro;
extern const Clip clip_runner_super_run;
extern const Clip clip_runner_salto;
extern const Model model_jake;
extern const Model model_tricky;
extern const Model model_fresh;
extern const Model model_spike;
extern const Clip clip_inspector_run;
extern const Clip clip_inspector_jump;
extern const Clip clip_inspector_roll;
extern const Clip clip_inspector_dodge_l;
extern const Clip clip_inspector_dodge_r;
extern const Clip clip_inspector_catch;
extern const Clip clip_inspector_whistle;
extern const Clip clip_inspector_intro;
extern const Model model_inspector;
extern const Clip clip_dog_run;
extern const Clip clip_dog_jump;
extern const Clip clip_dog_dodge_l;
extern const Clip clip_dog_dodge_r;
extern const Clip clip_dog_catch;
extern const Clip clip_dog_intro;
extern const Model model_dog;
extern const Model model_p_jetpack;
extern const Model model_p_magnet;
extern const Model model_p_sneakers;
extern const Model model_p_double;
extern const Model model_p_mystery;
extern const Model model_p_key;
extern const Model model_p_board;
extern const Model model_p_bouncer;
extern const Model model_p_daredevil;
extern const Image im_coin;
extern const Image im_coin_s;
extern const Image im_key;
extern const Image im_key_s;
extern const Image im_board;
extern const Image im_board_s;
extern const Image im_jetpack;
extern const Image im_jetpack_s;
extern const Image im_mystery;
extern const Image im_mystery_s;
extern const Image im_headstart;
extern const Image im_headstart_s;
extern const Image im_booster;
extern const Image im_booster_s;
extern const Image im_sneakers;
extern const Image im_sneakers_s;
extern const Image im_magnet;
extern const Image im_magnet_s;
extern const Image im_double;
extern const Image im_double_s;
extern const Image im_logo;
extern const uint32_t ground_rgb[2 * 384];   /* between two sleepers, then on one */
extern const BModel bm_car_sub;
extern const BModel bm_car_sub_wagon;
extern const BModel bm_car_standard;
extern const BModel bm_car_standard_wagon;
extern const BModel bm_car_cargo1;
extern const BModel bm_car_cargo2;
extern const BModel bm_car_freight;
extern const BModel bm_car_ramp;
extern const BModel bm_barrier_jump;
extern const BModel bm_barrier_roll;
extern const BModel bm_barrier_standard;
extern const BModel bm_low_01_l;
extern const BModel bm_low_01_r;
extern const BModel bm_low_02_l;
extern const BModel bm_low_02_r;
extern const BModel bm_med_01_l;
extern const BModel bm_med_01_r;
extern const BModel bm_med_02_l;
extern const BModel bm_med_02_r;
extern const BModel bm_med_03_l;
extern const BModel bm_med_03_r;
extern const BModel bm_high_01_l;
extern const BModel bm_high_01_r;
extern const BModel bm_high_02_l;
extern const BModel bm_high_02_r;
extern const BModel bm_high_03_l;
extern const BModel bm_high_03_r;
extern const BModel bm_pillars_start;
extern const BModel bm_pillars_mid;
extern const BModel bm_pillars_end;
extern const BModel bm_station_start;
extern const BModel bm_station_mid;
extern const BModel bm_station_end;
extern const BModel bm_tube_start;
extern const BModel bm_tube_mid;
extern const BModel bm_tube_end;

enum {
  TX_JAKE = 0,
  TX_TRICKY = 1,
  TX_FRESH = 2,
  TX_SPIKE = 3,
  TX_INSPECTOR = 4,
  TX_PROPS = 5,
  TX_P_BOARD = 6,
  TX_P_BOUNCER = 7,
  TX_P_DAREDEVIL = 8,
  TX_COIN = 9,
  TX_CAR_SUB = 10,
  TX_CAR_SUB_WAGON = 11,
  TX_CAR_STANDARD = 12,
  TX_CAR_STANDARD_WAGON = 13,
  TX_CAR_CARGO1 = 14,
  TX_CAR_CARGO2 = 15,
  TX_CAR_FREIGHT = 16,
  TX_CAR_RAMP = 17,
  TX_BARRIER_JUMP = 18,
  TX_BARRIER_ROLL = 19,
  TX_BARRIER_STANDARD = 20,
  TX_LOW_01_L = 21,
  TX_LOW_01_R = 22,
  TX_LOW_02_L = 23,
  TX_LOW_02_R = 24,
  TX_MED_01_L = 25,
  TX_MED_01_R = 26,
  TX_MED_02_L = 27,
  TX_MED_02_R = 28,
  TX_MED_03_L = 29,
  TX_MED_03_R = 30,
  TX_HIGH_01_L = 31,
  TX_HIGH_01_R = 32,
  TX_HIGH_02_L = 33,
  TX_HIGH_02_R = 34,
  TX_HIGH_03_L = 35,
  TX_HIGH_03_R = 36,
  TX_PILLARS_START = 37,
  TX_PILLARS_MID = 38,
  TX_PILLARS_END = 39,
  TX_STATION_START = 40,
  TX_STATION_MID = 41,
  TX_STATION_END = 42,
  TX_TUBE_START = 43,
  TX_TUBE_MID = 44,
  TX_TUBE_END = 45,
};
#endif

