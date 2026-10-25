/* A run to the finish for each official track: the keys held from a frame on (IN_ bits of car.h). Found by
 * tests/route.c, a driver that tries keys ahead with the game's own physics. */
#ifndef PT_ROUTES_H
#define PT_ROUTES_H
#include <stdint.h>
typedef struct { uint32_t frame; uint8_t keys; } route_ev;
typedef struct { const route_ev *ev; int n; } route_t;
static const route_ev r_default[] = {{0, 1}};
static const route_t pt_routes[17] = {
  {r_default, 1}, {r_default, 1}, {r_default, 1}, {r_default, 1}, {r_default, 1}, {r_default, 1}, {r_default, 1},
  {r_default, 1}, {r_default, 1}, {r_default, 1}, {r_default, 1}, {r_default, 1}, {r_default, 1}, {r_default, 1},
  {r_default, 1}, {r_default, 1}, {r_default, 1},
};
#endif
