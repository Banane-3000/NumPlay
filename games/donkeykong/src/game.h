/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef DK_GAME_H
#define DK_GAME_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#define DK_Q 256
#define DK_W 224
#define DK_H 224
#define DK_PLATFORMS 20
#define DK_LADDERS 18
#define DK_ENEMIES 16
#define DK_SAVE_BYTES 32
#define DK_MAX_SCORE 999999u
enum { DK_LEFT=1, DK_RIGHT=2, DK_UP=4, DK_DOWN=8, DK_JUMP=16 };
enum { DK_READY, DK_RUNNING, DK_DYING, DK_CLEAR, DK_OVER };
enum { DK_GIRDER, DK_BELT, DK_LIFT };
enum { DK_BARREL=1, DK_FIRE, DK_TUB, DK_SPRING };
enum { DK_CRASH, DK_FALL, DK_TIME };
typedef struct { int16_t x1,x2,y1,y2; int8_t type,dir; } dk_platform;
typedef struct { int16_t x; uint8_t lower,upper,broken,retract; } dk_ladder;
typedef struct {
  int32_t x,y,vy,fall_from;
  int8_t support,ladder,face;
  uint8_t grounded,coyote,jump_buffer,air_points;
  uint16_t hammer,jump_id;
} dk_player;
typedef struct {
  int32_t x,y,old_x,vy;
  int16_t timer;
  uint16_t jump_id;
  int8_t row,dir,ladder;
  uint8_t type,mode,blue,active;
} dk_enemy;
typedef struct { int16_t x,y; uint8_t type,active; } dk_item;
typedef struct {
  uint32_t best,runs,rescues;
  uint16_t best_round;
} dk_stats;
typedef struct {
  uint32_t score,rng,tick,age;
  uint16_t round,bonus,timer,bonus_ticks,spawn,throws;
  uint16_t popup_time,popup_value;
  int16_t popup_x,popup_y;
  uint8_t board,phase,lives,extra_life,rivets,previous_input,reason;
  uint8_t platform_count,ladder_count;
  dk_player player;
  dk_platform platform[DK_PLATFORMS];
  dk_ladder ladder[DK_LADDERS];
  dk_enemy enemy[DK_ENEMIES];
  dk_item item[5];
} dk_game;
void dk_new(dk_game *,uint32_t seed);
void dk_step(dk_game *,dk_stats *,uint8_t input);
int dk_surface(const dk_game *,unsigned platform,int x);
bool dk_ladder_open(const dk_game *,unsigned ladder);
int dk_belt_direction(const dk_game *,const dk_platform *);
unsigned dk_difficulty(const dk_game *);
bool dk_valid(const dk_game *);
void dk_encode_stats(const dk_stats *,uint8_t out[DK_SAVE_BYTES]);
bool dk_decode_stats(dk_stats *,const uint8_t *,size_t);
#endif
