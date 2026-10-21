#ifndef GAME_H
#define GAME_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#define SAVE_BYTES 24
#define LEVEL_COUNT 60
#define MAX_PINS 32
enum {TITLE,PLAY,PAUSE,RESULT,LEVELS,HELP,SAVE_ERROR};
enum {RUNNING,FAILED,CLEARED};
typedef struct {uint32_t current,unlocked,runs,taps;} Stats;
typedef struct {
 float angle,pins[MAX_PINS],speed;
 uint32_t tick,phase_age,motion_age;
 uint8_t level,count,remaining,motion,phase,flights[6],direction,speed_phase,ready;
} Game;
void game_new(Game*,unsigned);
void game_step(Game*,bool fire);
void game_finish(Stats*,const Game*);
bool game_valid(const Game*);
void pin_position(const Game*,unsigned,float*,float*);
bool game_collision(const Game*,float,float);
void save_encode(const Stats*,uint8_t*);
bool save_decode(Stats*,const uint8_t*,size_t);
#endif
