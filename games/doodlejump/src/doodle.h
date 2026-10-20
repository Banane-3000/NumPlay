/* SPDX-License-Identifier: GPL-3.0-only
 * Doodle Jump: original NumWorks implementation by Bobydeluxe.
 */
#ifndef DOODLE_H
#define DOODLE_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#define DJ_Q 256
#define DJ_WIDTH 288
#define DJ_LEFT 18
#define DJ_RIGHT (DJ_WIDTH-18)
#define DJ_TICK_MS 16
#define DJ_PLATFORMS 16
#define DJ_ENEMIES 3
#define DJ_SHOTS 3
#define DJ_SAVE_BYTES 384
#define DJ_LIMIT 1000000
enum { DJ_WAIT, DJ_RUNNING, DJ_OVER };
enum { DJ_SOLID, DJ_MOVING, DJ_BRITTLE, DJ_FADING };
enum { DJ_SPRING=1, DJ_ROCKET=2 };
enum { DJ_FELL=1, DJ_HIT=2 };
typedef struct {
  int32_t y;
  uint16_t x;
  uint8_t width,type,item,phase,active;
} dj_platform;
typedef struct { int32_t y; uint16_t x; uint8_t phase,active; } dj_enemy;
typedef struct { int32_t x,y; uint8_t active; } dj_shot;
typedef struct {
  uint32_t best,runs,jumps,springs,rockets,monsters;
  uint8_t motion;
} dj_stats;
typedef struct {
  int32_t x,y,vx,vy,camera,peak,next_y;
  uint32_t rng,tick,rows,jumps;
  uint16_t route_x;
  uint8_t phase,present,started,reason,boost,cooldown,face,bounce;
  uint8_t route_phase; /* Half/run/direction history; zero starts a new history. */
  dj_platform platform[DJ_PLATFORMS];
  dj_enemy enemy[DJ_ENEMIES];
  dj_shot shot[DJ_SHOTS];
} dj_game;
void dj_new(dj_game *g,uint32_t seed);
void dj_start(dj_game *g,dj_stats *s);
void dj_migrate(dj_game *g);
void dj_step(dj_game *g,dj_stats *s,int steering);
bool dj_fire(dj_game *g);
uint32_t dj_height(const dj_game *g);
int32_t dj_platform_x(const dj_platform *p,uint32_t tick);
int32_t dj_enemy_x(const dj_enemy *e,uint32_t tick);
bool dj_valid(const dj_game *g,const dj_stats *s);
void dj_encode(const dj_game *g,const dj_stats *s,uint8_t out[DJ_SAVE_BYTES]);
bool dj_decode(dj_game *g,dj_stats *s,const uint8_t *p,size_t n);
#endif
