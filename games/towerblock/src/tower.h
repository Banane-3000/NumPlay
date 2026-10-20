/* SPDX-License-Identifier: GPL-3.0-only
 * Original NumWorks implementation by Bobydeluxe.
 */
#ifndef TOWER_BLOCK_H
#define TOWER_BLOCK_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define TB_Q 256
#define TB_SIZE (72*TB_Q)
#define TB_TRAVEL (80*TB_Q)
#define TB_SNAP 384
#define TB_MIN 64
#define TB_LAYERS 24
#define TB_SHARDS 3
#define TB_SAVE_BYTES 384
#define TB_TICK_MS 16
#define TB_PALETTES 4

enum { TB_MOVING,TB_SETTLING,TB_ENDING,TB_OVER };
enum { TB_NONE,TB_CUT,TB_PERFECT,TB_GROW,TB_MISS };

typedef struct { int16_t x,z,w,d;uint8_t tone; } tb_layer;
typedef struct {
  int32_t x,z,fall;
  uint32_t level;
  int16_t w,d,vx,vz,vy;
  uint8_t life,tone;
} tb_shard;
typedef struct {
  uint32_t best,total,perfect,attempts;
  uint8_t palette,guide,motion;
} tb_stats;
typedef struct {
  tb_layer layer[TB_LAYERS];
  tb_shard shard[TB_SHARDS];
  uint32_t height,perfect,misses,tick;
  int32_t offset,cam_x,cam_z;
  int16_t camera,drop;
  uint16_t streak,peak;
  int8_t direction;
  uint8_t count,phase,present,started,axis,timer,result,message;
} tb_game;

void tb_new(tb_game *g);
void tb_start(tb_game *g,tb_stats *s);
bool tb_place(tb_game *g,tb_stats *s);
void tb_step(tb_game *g,const tb_stats *s);
unsigned tb_speed(const tb_game *g);
bool tb_valid(const tb_game *g,const tb_stats *s);
void tb_encode(const tb_game *g,const tb_stats *s,uint8_t out[TB_SAVE_BYTES]);
bool tb_decode(tb_game *g,tb_stats *s,const uint8_t *data,size_t length);
#endif
