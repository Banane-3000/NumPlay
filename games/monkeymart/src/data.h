#ifndef MM_DATA_H
#define MM_DATA_H
#include <stdint.h>
#include <stdbool.h>
#define MART_COUNT 2
#define MAX_PLACES 43
#define MAX_WORKERS 7
#define MAX_CUSTOMERS 8
#define FOOD_COUNT 14
#define HAT_COUNT 21
enum {PRODUCER=1,SHELF=2,RETAIL=4,HIDDEN=8,CASHIER=16,EMPLOYEE=32,NEXT_MART=64};
typedef struct {int16_t x,y;} Point;
typedef struct {Point pos,work,buy,second,poly[4];uint64_t unlock;uint16_t price,radius,sprite,sprite2;uint8_t food,flags,capacity,period,inputs[2],activate,stamina,upgrade,poly_count;} PlaceDef;
typedef struct {Point pos;int16_t depth;uint16_t sprite;uint8_t when,scale;} Decor;
typedef struct {uint64_t destinations;uint8_t place,hat,upgrade;} WorkerDef;
typedef struct {const PlaceDef*places;const Decor*decor;const WorkerDef*workers;const uint8_t*order;const uint16_t(*prices)[3][3];const uint8_t(*upgrade_max)[3];const uint8_t*upgrade_places;Point spawn,entry,trash;int16_t minx,maxx,miny,maxy;uint8_t count,decor_count,worker_count,order_count,cash,upgrade_count;} MartDef;
extern const MartDef marts[2];
extern const uint16_t hat_prices[21];
extern const uint8_t hat_tokens[21];
extern const uint8_t food_prices[15],food_max[15];
extern const char*const food_names[15];
extern const char*const upgrade_names[2][12];
#endif
