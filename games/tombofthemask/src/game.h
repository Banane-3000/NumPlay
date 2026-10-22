#ifndef TOTM_GAME_H
#define TOTM_GAME_H
#include "assets.h"
#include <stdbool.h>
#define GRID_W 72
#define GRID_H 128
/* Largest original stage: 1,720 cells; arcade window: 13 * 128. */
#define CELLS 1792
#define ACTORS 96
#define SAVE_BYTES 320
#define Q 256
enum {TITLE,STAGES,PLAY,PAUSE,RESULT,DEAD,SHOP,MASKS,HELP,QUIT,SAVE_ERROR,MISSIONS,CHEST};
typedef struct {uint32_t coins,best,xp,runs,mission_counts[8],playtime,next_chest;uint16_t unlocked,owned,chests;uint8_t free_chest,mission_step[8],mask,shields,upgrades[4],stars[200],dots[25];} Progress;
typedef struct {int32_t x,y,home_x,home_y;int16_t vx,vy;uint16_t timer;uint8_t type,active;} Actor;
typedef struct {uint8_t type;int8_t x,y;} Brick;
typedef struct {int16_t x,y;uint8_t count,direction;Brick bricks[64];} Rotor;
typedef struct {Progress p;uint8_t tiles[CELLS];Actor actors[ACTORS];Rotor rotors[8];uint32_t rng,tick,score,height_score;int32_t x,y,previous_x,previous_y,lava;uint16_t level,width,height,coins,dots,total_dots,stars,shield,freeze,magnet,boost,invincible,portal_cooldown,dead,won,ride,addict,powerup_wait;int8_t dx,dy,queued;uint8_t arcade,dirty,bonus,done,reward_kind;uint16_t combo,phase,reward_value;} Game;
void game_new(Game*,uint32_t);
bool game_start(Game*,unsigned,bool);
void game_step(Game*,int,bool);
void game_finish(Game*);
bool game_shop(Game*,unsigned);
unsigned game_player_level(const Progress*);
bool game_mask(Game*,unsigned);
bool game_mission_claim(Game*,unsigned);
bool game_chest(Game*);
uint8_t game_tile(const Game*,int,int);
bool game_solid(unsigned);
void game_test_touch(Game*);
void save_encode(const Progress*,uint8_t*);
bool save_decode(Progress*,const uint8_t*,unsigned);
#endif
