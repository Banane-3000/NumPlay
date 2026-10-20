/* SPDX-License-Identifier: GPL-3.0-only
 * NumWorks game implementation by Bobydeluxe. Artwork credits: assets/README.md.
 */
#ifndef JETPACK_H
#define JETPACK_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#define JJ_Q 256
#define JJ_TICK_MS 16
#define JJ_SAVE_BYTES 260
#define JJ_HAZARDS 4
#define JJ_PICKUPS 18
#define JJ_LIMIT 10000000u
enum { JJ_WAIT,JJ_RUNNING,JJ_OVER };
enum { JJ_HORIZONTAL,JJ_VERTICAL,JJ_MISSILE,JJ_COLUMN,JJ_ROTATING };
enum { JJ_COIN,JJ_SHIELD };
typedef struct { int32_t x; uint16_t y; uint8_t kind,timer,active; } jj_hazard;
typedef struct { int32_t x; uint16_t y; uint8_t kind,active; } jj_pickup;
typedef struct {
  uint32_t best,runs,coins,dodged,wallet;
  uint8_t owned,suit;
} jj_stats;
typedef struct {
  int32_t y,vy,next;
  uint32_t rng,tick,distance;
  uint16_t coins,passed,events;
  uint8_t phase,present,started,thrust,shield,invuln,reason,flash;
  jj_hazard hazard[JJ_HAZARDS];
  jj_pickup pickup[JJ_PICKUPS];
} jj_game;
void jj_new(jj_game *g,uint32_t seed);
void jj_start(jj_game *g,jj_stats *s);
void jj_step(jj_game *g,jj_stats *s,bool thrust);
uint32_t jj_distance(const jj_game *g);
int jj_speed(const jj_game *g);
void jj_bounds(unsigned kind,int *w,int *h);
void jj_rotor(const jj_hazard *p,uint32_t tick,int *dx,int *dy);
bool jj_rotor_hit(const jj_hazard *p,uint32_t tick,int32_t y);
unsigned jj_price(unsigned suit);
bool jj_equip(jj_stats *s,unsigned suit);
bool jj_valid(const jj_game *g,const jj_stats *s);
void jj_encode(const jj_game *g,const jj_stats *s,uint8_t out[JJ_SAVE_BYTES]);
bool jj_decode(jj_game *g,jj_stats *s,const uint8_t *p,size_t n);
#endif
