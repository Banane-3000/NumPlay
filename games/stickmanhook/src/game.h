/* Native calculator adaptation by Bobydeluxe. See assets/provenance.json. */
#ifndef SH_GAME_H
#define SH_GAME_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#define SH_SKINS 23
#define SH_LEVEL_COUNT 50u
#define SH_MAX_LEVEL (SH_LEVEL_COUNT-1)
#include "levels_meta.h"
#define SH_SAVE_BYTES 40
#define SH_RADIUS .017f
typedef struct {float x,y;} sh_vec;
typedef struct {float x,y;uint8_t profile;} sh_bumper;
typedef struct {float w,h,c,s,bounce,force,angle;uint8_t angled;} sh_bumper_profile;
typedef struct {float x,y;uint8_t rotation,type;} sh_obstacle;
typedef struct {float x,y,dx,dy,nx,ny,inv;} sh_edge;
typedef struct {uint16_t first,count;float radius;} sh_shape;
typedef struct {float spawn_x,spawn_y,finish;uint16_t hook,bumper,obstacle;uint8_t hooks,bumpers,obstacles,background;} sh_part;
typedef struct {uint8_t part;float x;} sh_section;
typedef struct {uint32_t current,unlocked,seed,runs;uint8_t skin;} sh_stats;
typedef struct {
  float x,y,vx,vy,theta,omega,r,pull,rotation,spin;
  float hook_x,hook_y,finish,spawn_x,spawn_y,progress,camera_offset,camera_x,complete_time;
  uint32_t level,seed,attempt,ticks,bumper_hits,wall_hits;
  sh_section sections[3];
  sh_vec trail[20];
  uint8_t section_count,hooked,held,completed,ready,background,pose,facing,trail_count,trail_cursor;
} sh_game;
enum {SH_SPAWN,SH_HOOKED0,SH_HOOKED1,SH_HOOKED2,SH_BALL,SH_VICTORY};
extern const sh_vec sh_hooks[];
extern const sh_bumper sh_bumpers[];
extern const sh_bumper_profile sh_bumper_profiles[];
extern const sh_obstacle sh_obstacles[];
extern const sh_vec sh_rotations[];
extern const sh_part sh_parts[];
extern const sh_edge sh_edges[];
extern const sh_shape sh_shapes[];
extern const uint8_t sh_courses[SH_LEVEL_COUNT][3];
extern const uint16_t sh_unlocks[SH_SKINS];
extern const char sh_skin_names[SH_SKINS][16];
void sh_new(sh_game *,uint32_t level,uint32_t seed);
void sh_retry(sh_game *);
void sh_step(sh_game *,bool held);
bool sh_closest(const sh_game *,sh_vec *);
void sh_hook(sh_game *,sh_vec);
void sh_release(sh_game *);
bool sh_skin_open(const sh_stats *,unsigned);
void sh_finish(sh_stats *,const sh_game *);
void sh_encode(const sh_stats *,uint8_t[SH_SAVE_BYTES]);
bool sh_decode(sh_stats *,const uint8_t *,size_t);
bool sh_valid(const sh_game *);
#endif
