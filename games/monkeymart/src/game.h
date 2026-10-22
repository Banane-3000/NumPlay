#ifndef MM_GAME_H
#define MM_GAME_H
#include "data.h"
#define ACTOR_COUNT 17
#define CARRY_MAX 12
#define TICKS 60
#define SAVE_BYTES 768
enum {PLAYER,WORKER,BUYER,MANAGER,ROWDY};
enum {ENTER,SHOPPING,QUEUE,LEAVING,RESTING,PACKING};
typedef struct {uint64_t active;uint16_t paid[MAX_PLACES],production[MAX_PLACES];uint8_t stock[MAX_PLACES],stamina[MAX_PLACES],upgrade[12][3],worker_stamina[7];uint32_t cash;uint8_t progress,started;} MartSave;
typedef struct {MartSave mart[2];uint32_t coins,hats,rng,seconds,total_sales;uint16_t boosts[3],reward_cooldown;int16_t x,y;uint8_t market,hat,carry[CARRY_MAX],count,tokens;} Progress;
typedef struct {float x,y,gx,gy,vx,vy;uint16_t wait,nav_age;int16_t nav;uint16_t bill;uint8_t carry[CARRY_MAX],wish[FOOD_COUNT+1];uint8_t count,role,phase,target,destination,stamina,skin,hat,live,moving,flip,failures;} Actor;
typedef struct {Point pos;uint8_t food;} Dropped;
typedef struct {Progress p;Actor actors[ACTOR_COUNT];Dropped dropped[20];uint64_t offered;uint32_t tick;float camera_x,camera_y;uint16_t customer_clock,rowdy_clock,notice_time;uint8_t selected_place,overview,dirty,market_request,market_armed;char notice[48];} Game;
void game_new(Game*,uint32_t);
void game_restore(Game*);
void game_step(Game*,int,int);
bool game_buy(Game*,unsigned);
void game_unlock(Game*);
bool game_market(Game*,unsigned);
bool game_upgrade(Game*,unsigned,unsigned);
uint16_t game_upgrade_price(const Game*,unsigned,unsigned);
bool game_hat(Game*,unsigned);
bool game_reward(Game*,unsigned);
unsigned game_capacity(const Game*,const Actor*);
unsigned game_stock_capacity(const Game*,unsigned);
Point game_repair_position(const Game*,unsigned);
bool game_blocked(const Game*,float,float);
void game_note(Game*,const char*);
void game_snapshot(Game*);
bool save_decode(Progress*,const uint8_t*,uint32_t);
void save_encode(const Progress*,uint8_t[SAVE_BYTES]);
#endif
