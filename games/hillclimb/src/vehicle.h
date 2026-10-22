/* Hill Climb Racing: the vehicles and their upgrades (how they drive: car.cpp). */
#ifndef HC_VEHICLE_H
#define HC_VEHICLE_H
#include <stdbool.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

/* (the ids are the save's: new vehicles go at the end; the garage shows them in the game's order) */
enum { V_JEEP, V_BIKE, V_MONSTER, V_QUAD, V_RACE, V_RALLY, V_BUGGY, V_TANK, V_TRACTOR, V_BUS, V_DIESEL, V_SNOWMOBILE,
       V_TRUCK, NVEHICLES };
enum { UP_ENGINE, UP_SUSPENSION, UP_TIRES, UP_AWD, UP_AIR, UP_DOWNFORCE, UP_FUEL, UP_TRACKS, UP_STABILITY, NUPKINDS };

typedef struct {
  const char *name, *about;
  uint32_t price;
  uint8_t icon;
  uint8_t up[4], max[4];       /* its four upgrades and their last levels */
  uint16_t cost[4];            /* level L costs cost x L x L coins */
} VehDef;

extern const VehDef vehicles[NVEHICLES];
extern const uint8_t vehicle_order[NVEHICLES];   /* the garage's order, the game's */
extern const char *const upgrade_names[NUPKINDS];
extern const uint8_t upgrade_icons[NUPKINDS];
uint32_t upgrade_cost(int v, int u, int level);   /* the price of reaching `level` (2 and up) */
#ifdef __cplusplus
}
#endif
#endif
