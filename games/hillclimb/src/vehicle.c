/* The vehicles (vehicle.h). Their prices, upgrades and the upgrades' last levels and prices are the game's own
 * (PlayerCarManager::initialize in its Android package, the same as the Hill Climb Racing Wiki's tables), and so
 * are the descriptions. How they drive is the game's own, in car.cpp. */
#include "data.h"
#include "vehicle.h"

const char *const upgrade_names[NUPKINDS] = {"Engine", "Suspension", "Tires", "4WD", "Air Control", "Downforce",
                                             "Fuel", "Tracks", "Stability"};
const uint8_t upgrade_icons[NUPKINDS] = {IMG_UP_ENGINE, IMG_UP_SUSPENSION, IMG_UP_TIRES, IMG_UP_AWD, IMG_UP_AIR,
                                         IMG_UP_DOWNFORCE, IMG_UP_FUEL, IMG_UP_TIRES, IMG_UP_DOWNFORCE};

const VehDef vehicles[NVEHICLES] = {
  {"Hill Climber", "Bill's trusty jeep. Upgrade its 4WD to climb anything.", 0, IMG_ICON_JEEP,
   {UP_ENGINE, UP_SUSPENSION, UP_TIRES, UP_AWD}, {13, 14, 16, 10}, {1000, 1000, 1000, 1000}},
  {"Motocross Bike", "Race the hills with this impressive turboed motocross bike.", 75000, IMG_ICON_BIKE,
   {UP_ENGINE, UP_SUSPENSION, UP_TIRES, UP_AIR}, {12, 14, 16, 10}, {1500, 1000, 1000, 3000}},
  {"Monster Truck", "With large wheels and a supercharged engine this car really gives a new experience for Hill "
   "Climbing!", 100000, IMG_ICON_MONSTER,
   {UP_ENGINE, UP_SUSPENSION, UP_TIRES, UP_AWD}, {14, 16, 18, 10}, {1000, 1000, 1000, 1000}},
  {"Quad Bike", "4x4 quad bike with almost the same agility as the bike offers you a different ride.", 175000,
   IMG_ICON_QUAD, {UP_ENGINE, UP_SUSPENSION, UP_TIRES, UP_AWD}, {12, 14, 16, 10}, {1500, 1100, 1100, 1200}},
  {"Race Car", "High power and aerodynamic ultimate hill climbing vehicle. Upgrade downforce to make this car "
   "follow the road like a magnet.", 250000, IMG_ICON_RACE,
   {UP_ENGINE, UP_SUSPENSION, UP_TIRES, UP_DOWNFORCE}, {12, 14, 16, 10}, {1500, 1000, 1000, 3000}},
  {"Rally Car", "Powerful engine and downforce upgraded provide the most ultimate combination for hill climbing.",
   750000, IMG_ICON_RALLY, {UP_ENGINE, UP_SUSPENSION, UP_TIRES, UP_DOWNFORCE}, {13, 14, 17, 10},
   {1800, 1250, 1250, 3000}},
  {"Dune Buggy", "Quick, good traction and a handy hill climbing vehicle!", 800000, IMG_ICON_BUGGY,
   {UP_ENGINE, UP_SUSPENSION, UP_TIRES, UP_FUEL}, {16, 15, 25, 20}, {2000, 1500, 1500, 1200}},
  {"Tank", "It's a tank!", 1000000, IMG_ICON_TANK,
   {UP_ENGINE, UP_SUSPENSION, UP_TRACKS, UP_FUEL}, {16, 15, 20, 20}, {1250, 1250, 1000, 1000}},
  {"Tractor", "The tractor is slow and heavy but has great traction and plenty of torque (and a tendency to roll "
   "over in steep terrain). Not recommended for caves.", 100000, IMG_ICON_TRACTOR,
   {UP_ENGINE, UP_TIRES, UP_FUEL, UP_STABILITY}, {16, 15, 20, 15}, {1000, 1000, 1000, 1000}},
  {"Tourist Bus", "Bill decided to take a shower and give people a ride while climbing hills! Not suitable for "
   "cave levels!", 200000, IMG_ICON_BUS,
   {UP_ENGINE, UP_SUSPENSION, UP_TIRES, UP_FUEL}, {15, 15, 20, 25}, {1400, 1250, 1000, 1000}},
  {"Super Diesel 4x4", "Ready for the super turbo whistle and black smoke? With powerful engine and large wheelbase "
   "this really is an unique experience!", 500000, IMG_ICON_DIESEL,
   {UP_ENGINE, UP_SUSPENSION, UP_TIRES, UP_FUEL}, {15, 15, 20, 20}, {1400, 1250, 1000, 1000}},
  {"Snow Mobile", "Great traction in the ice and snow. Fully upgraded it becomes a fun vehicle for other levels "
   "too!", 1000000, IMG_ICON_SNOWMOBILE,
   {UP_ENGINE, UP_SUSPENSION, UP_TRACKS, UP_FUEL}, {17, 15, 20, 20}, {1250, 1250, 1000, 1000}},
  {"Truck", "Weight, power, turbo whistle and attitude! And upgradeable very large fuel tank!", 1000000,
   IMG_ICON_TRUCK, {UP_ENGINE, UP_SUSPENSION, UP_TIRES, UP_FUEL}, {15, 15, 20, 25}, {1400, 1250, 1000, 1000}},
};

/* PlayerCarManager::initialize's order */
const uint8_t vehicle_order[NVEHICLES] = {V_JEEP,  V_BIKE, V_MONSTER, V_TRACTOR, V_QUAD,       V_BUS,  V_RACE,
                                          V_DIESEL, V_RALLY, V_TANK,  V_SNOWMOBILE, V_TRUCK, V_BUGGY};

uint32_t upgrade_cost(int v, int u, int level) { return (uint32_t)vehicles[v].cost[u] * (uint32_t)(level * level); }
