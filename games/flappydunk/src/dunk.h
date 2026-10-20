/* SPDX-License-Identifier: GPL-3.0-only
 * Flappy Dunk: an original NumWorks implementation by Bobydeluxe.
 */
#ifndef FLAPPY_DUNK_H
#define FLAPPY_DUNK_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define FD_TICK_MS 16
#define FD_SAVE_BYTES 160
#define FD_HOOPS 3
#define FD_PARTICLES 16
#define FD_SKINS 6
#define FD_BALL_R 9
#define FD_TOP 38
#define FD_FLOOR 212

enum { FD_WAITING, FD_RUNNING, FD_OVER };
enum { FD_NO_REASON, FD_FLOOR_HIT, FD_CEILING_HIT, FD_MISSED };
extern const char *const fd_skin_names[FD_SKINS];
extern const uint16_t fd_skin_targets[FD_SKINS];

typedef struct {
  float x, y;
  uint8_t state, touched, clean, wobble;
} fd_hoop;
typedef struct { float x,y,vx,vy; uint8_t life,kind; } fd_particle;
typedef struct {
  uint32_t best, baskets, swishes, runs, flaps, best_streak;
  uint8_t skin, unlocked;
} fd_stats;
typedef struct {
  float x,y,vx,vy,scroll;
  fd_hoop hoop[FD_HOOPS];
  fd_particle particle[FD_PARTICLES];
  uint32_t rng,tick,score,baskets,swishes,flaps;
  uint16_t streak,peak;
  uint8_t phase,reason,present,started;
  uint8_t flap_anim,message,message_timer,award,unlocks;
} fd_game;

void fd_new(fd_game *g,uint32_t seed);
bool fd_flap(fd_game *g,fd_stats *s);
void fd_step(fd_game *g,fd_stats *s);
float fd_half_width(const fd_game *g);
float fd_speed(const fd_game *g);
uint8_t fd_unlocked(const fd_stats *s);
bool fd_valid(const fd_game *g,const fd_stats *s);
void fd_encode(const fd_game *g,const fd_stats *s,uint8_t out[FD_SAVE_BYTES]);
bool fd_decode(fd_game *g,fd_stats *s,const uint8_t *data,size_t length);
#endif
