/* The vehicles (vehicle.h). Their prices, upgrades, the upgrades' last levels and prices are the game's (Hill
 * Climb Racing Wiki); how they drive is tuned to match it: the Hill Climber light and tippy, rear-wheel driven
 * until its 4WD is upgraded; the bike quick to flip; the Monster Truck's big soft wheels; the Race Car fast and low;
 * the Tank slow, heavy and gripping everything. */
#include "data.h"
#include "vehicle.h"

const char *const upgrade_names[NUPKINDS] = {"Engine", "Suspension", "Tires", "4WD", "Air Control", "Downforce",
                                             "Fuel", "Tracks"};
const uint8_t upgrade_icons[NUPKINDS] = {IMG_UP_ENGINE, IMG_UP_SUSPENSION, IMG_UP_TIRES, IMG_UP_AWD, IMG_UP_AIR,
                                         IMG_UP_DOWNFORCE, IMG_UP_FUEL, IMG_UP_TIRES};

const VehDef vehicles[NVEHICLES] = {
  {"Hill Climber", "Bill's trusty jeep. Upgrade its 4WD to climb anything.", 0, IMG_ICON_JEEP,
   {UP_ENGINE, UP_SUSPENSION, UP_TIRES, UP_AWD}, {13, 14, 16, 10}, {1000, 1000, 1000, 1000},
   300, 24, {0.02f, 0.38f}, 0.95f, 0.14f, 0.16f, 0.55f, 430, 11.0f, 0, 0.95f, 760, 40, 0},
  {"Motocross Bike", "Race the hills with this impressive turboed motocross bike.", 75000, IMG_ICON_BIKE,
   {UP_ENGINE, UP_SUSPENSION, UP_TIRES, UP_AIR}, {12, 14, 16, 10}, {1500, 1000, 1000, 3000},
   150, 14, {-0.05f, 0.55f}, 0.62f, 0.13f, 0.18f, 0.5f, 230, 13.0f, 0, 1.0f, 160, 40, 0},
  {"Monster Truck", "With large wheels and a supercharged engine this car really gives a new experience for Hill "
   "Climbing!", 100000, IMG_ICON_MONSTER,
   {UP_ENGINE, UP_SUSPENSION, UP_TIRES, UP_AWD}, {13, 14, 15, 10}, {1500, 1200, 1200, 1500},
   340, 48, {0.0f, 0.45f}, 1.05f, 0.26f, 0.3f, 0.45f, 820, 11.5f, 0, 1.0f, 1100, 40, 0},
  {"Quad Bike", "4x4 quad bike with almost the same agility as the bike offers you a different ride.", 175000,
   IMG_ICON_QUAD, {UP_ENGINE, UP_SUSPENSION, UP_TIRES, UP_AIR}, {13, 14, 16, 10}, {1500, 1200, 1200, 2500},
   190, 20, {0.0f, 0.5f}, 0.7f, 0.18f, 0.22f, 0.5f, 300, 12, 1.0f, 1.0f, 260, 40, 0},
  {"Race Car", "High power and aerodynamic ultimate hill climbing vehicle. Upgrade downforce to make this car "
   "follow the road like a magnet.", 250000, IMG_ICON_RACE,
   {UP_ENGINE, UP_SUSPENSION, UP_TIRES, UP_DOWNFORCE}, {12, 14, 16, 12}, {1500, 1500, 1500, 2500},
   270, 18, {0.05f, 0.18f}, 1.05f, 0.06f, 0.08f, 0.6f, 430, 17, 0, 1.05f, 900, 40, 9.0f},
  {"Rally Car", "Powerful engine and downforce upgraded provide the most ultimate combination for hill climbing.",
   750000, IMG_ICON_RALLY, {UP_ENGINE, UP_SUSPENSION, UP_TIRES, UP_DOWNFORCE}, {14, 14, 17, 10},
   {2000, 2000, 2000, 3000},
   320, 22, {0.05f, 0.3f}, 1.0f, 0.11f, 0.13f, 0.6f, 560, 16, 0.6f, 1.05f, 1000, 40, 8.0f},
  {"Dune Buggy", "Quick, good traction and handy hill climbing vehicle!", 800000, IMG_ICON_BUGGY,
   {UP_ENGINE, UP_SUSPENSION, UP_TIRES, UP_FUEL}, {15, 15, 15, 20}, {3000, 3000, 3000, 3000},
   260, 22, {0.1f, 0.32f}, 0.95f, 0.17f, 0.22f, 0.5f, 520, 15, 0.8f, 1.15f, 700, 52, 0},
  {"Tank", "It's a tank!", 1000000, IMG_ICON_TANK,
   {UP_ENGINE, UP_SUSPENSION, UP_TRACKS, UP_FUEL}, {15, 15, 15, 20}, {3000, 3000, 3000, 3000},
   900, 30, {0.0f, 0.42f}, 1.15f, 0.08f, 0.1f, 0.7f, 520, 8.0f, 1.0f, 1.45f, 2600, 40, 0},
};

uint32_t upgrade_cost(int v, int u, int level) { return (uint32_t)vehicles[v].cost[u] * (uint32_t)(level * level); }
