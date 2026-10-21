/* The vehicles (vehicle.h). Their prices, upgrades, the upgrades' last levels and prices are the game's (Hill
 * Climb Racing Wiki); the Quad Bike's fourth upgrade is its 4WD, as the game's own saves name it (quadbike_awd), at
 * the game's price. How they drive is the game's own, in car.cpp. */
#include "data.h"
#include "vehicle.h"

const char *const upgrade_names[NUPKINDS] = {"Engine", "Suspension", "Tires", "4WD", "Air Control", "Downforce",
                                             "Fuel", "Tracks"};
const uint8_t upgrade_icons[NUPKINDS] = {IMG_UP_ENGINE, IMG_UP_SUSPENSION, IMG_UP_TIRES, IMG_UP_AWD, IMG_UP_AIR,
                                         IMG_UP_DOWNFORCE, IMG_UP_FUEL, IMG_UP_TIRES};

const VehDef vehicles[NVEHICLES] = {
  {"Hill Climber", "Bill's trusty jeep. Upgrade its 4WD to climb anything.", 0, IMG_ICON_JEEP,
   {UP_ENGINE, UP_SUSPENSION, UP_TIRES, UP_AWD}, {13, 14, 16, 10}, {1000, 1000, 1000, 1000}},
  {"Motocross Bike", "Race the hills with this impressive turboed motocross bike.", 75000, IMG_ICON_BIKE,
   {UP_ENGINE, UP_SUSPENSION, UP_TIRES, UP_AIR}, {12, 14, 16, 10}, {1500, 1000, 1000, 3000}},
  {"Monster Truck", "With large wheels and a supercharged engine this car really gives a new experience for Hill "
   "Climbing!", 100000, IMG_ICON_MONSTER,
   {UP_ENGINE, UP_SUSPENSION, UP_TIRES, UP_AWD}, {13, 14, 15, 10}, {1500, 1200, 1200, 1500}},
  {"Quad Bike", "4x4 quad bike with almost the same agility as the bike offers you a different ride.", 175000,
   IMG_ICON_QUAD, {UP_ENGINE, UP_SUSPENSION, UP_TIRES, UP_AWD}, {13, 14, 16, 10}, {1500, 1200, 1200, 1200}},
  {"Race Car", "High power and aerodynamic ultimate hill climbing vehicle. Upgrade downforce to make this car "
   "follow the road like a magnet.", 250000, IMG_ICON_RACE,
   {UP_ENGINE, UP_SUSPENSION, UP_TIRES, UP_DOWNFORCE}, {12, 14, 16, 12}, {1500, 1500, 1500, 2500}},
  {"Rally Car", "Powerful engine and downforce upgraded provide the most ultimate combination for hill climbing.",
   750000, IMG_ICON_RALLY, {UP_ENGINE, UP_SUSPENSION, UP_TIRES, UP_DOWNFORCE}, {14, 14, 17, 10},
   {2000, 2000, 2000, 3000}},
  {"Dune Buggy", "Quick, good traction and handy hill climbing vehicle!", 800000, IMG_ICON_BUGGY,
   {UP_ENGINE, UP_SUSPENSION, UP_TIRES, UP_FUEL}, {15, 15, 15, 20}, {3000, 3000, 3000, 3000}},
  {"Tank", "It's a tank!", 1000000, IMG_ICON_TANK,
   {UP_ENGINE, UP_SUSPENSION, UP_TRACKS, UP_FUEL}, {15, 15, 15, 20}, {3000, 3000, 3000, 3000}},
};

uint32_t upgrade_cost(int v, int u, int level) { return (uint32_t)vehicles[v].cost[u] * (uint32_t)(level * level); }
