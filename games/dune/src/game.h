#ifndef GAME_H
#define GAME_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#define SAVE_BYTES 40
#define BALL_COUNT 7
#define LANDSCAPE_COUNT 1
#define COIN_COUNT 32
#define WORLD_SCALE (240.f/665.f)
enum {TITLE,PLAY,PAUSE,RESULT,SHOP,HELP,SAVE_ERROR,BUY};
enum {RUNNING,CRASHED};
typedef struct {uint32_t best,coins,unlocked,scenes,runs,seed;uint8_t ball,landscape;} Stats;
typedef struct {float raw_width,width,height,divisor,x,contact[9];uint8_t flat,slid;} Terrain;
typedef struct {float x,y;uint8_t active;} Coin;
typedef struct {
 float x,y,vx,vy,camera_x,camera_y,zoom,rotation,slide_start;
 uint32_t rng,tick,score,earned,terrains;
 uint16_t airborne,phase_age,popup_age;
 uint8_t phase,grounded,sliding,slide_terrain,above_line,popup,ready;
 uint8_t order[8],spare;
 Terrain terrain[9];Coin coins[COIN_COUNT];
 float trail_scale,trail_angle;
} Game;
void game_new(Game*,uint32_t seed);
void game_step(Game*,bool held);
void game_input(Game*,bool held);
void terrain_shape(Terrain*,float,float,float,bool);
bool game_valid(const Game*);
float terrain_surface(const Terrain*,float x);
unsigned game_terrain(const Game*,float x);
float terrain_u(const Terrain*,float x);
void game_finish(Stats*,const Game*);
bool shop_owned(const Stats*,bool scene,unsigned index);
bool shop_buy(Stats*,bool scene,unsigned index);
void save_encode(const Stats*,uint8_t*);
bool save_decode(Stats*,const uint8_t*,size_t);
#endif
