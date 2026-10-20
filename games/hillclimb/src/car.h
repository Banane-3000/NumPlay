/* Hill Climb Racing: a vehicle and its world, as the game builds them (car.cpp).
 *
 * The game runs on Box2D 2.2.1, and so does this: the same bodies, fixtures and joints with the game's numbers
 * (its PlayerCar classes, read from its Android package), stepped 60 times a second with 8 velocity and 3 position
 * iterations. The chassis and the wheels hang on wheel joints whose motors drive them; Bill is a torso and a head
 * on revolute joints and springs; the ground is a chain of 0.5 m edges, rope bridges are planks on revolute joints.
 * Meters, seconds, radians; y up. car_step is one 1/60 s. */
#ifndef HC_CAR_H
#define HC_CAR_H
#include <stdbool.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

#define CAR_MAXW 6
#define CAR_HZ 60          /* car_step's rate */

typedef struct { float x, y, a; } CarBody;   /* a body's origin and angle */

enum { FLIP_NONE, FLIP_FRONT, FLIP_BACK, FLIP_NECK };

typedef struct {
  int veh, nw;
  CarBody ch, wh[CAR_MAXW], torso, head, cannon;
  float wr[CAR_MAXW];       /* the wheels' radii */
  float vx, vy, w;          /* the chassis' velocity and spin (rad/s, counterclockwise) */
  int air;                  /* steps since a wheel or the chassis last touched anything */
  bool dead;                /* Bill's head touched the ground: driver down */
  int flip;                 /* a flip completed this step (FLIP_*) */
  float rev, turbo;         /* the engine's revs and the turbo's pressure, for the meters */
  int tree_broken;          /* a tree knocked over this step, or -1 */
  bool ground[CAR_MAXW];    /* the wheel touched something this step */
} Car;

/* a new world for the stage being driven (stage.h), and the vehicle in it at the start */
void car_init(Car *c, int veh, const uint8_t levels[4]);
/* one 1/60 s: gas (1), brake (-1) or neither (0); engine: there is fuel and Bill is alive */
void car_step(Car *c, int pedal, bool engine);
/* the world's memory back (a run ends) */
void car_free(void);
/* a picture frame point (meters, the chassis' frame) in the world */
void car_point(const Car *c, float lx, float ly, float *wx, float *wy);
float car_speed(const Car *c);
bool car_airborne(const Car *c);
/* does a square (centre, half its side) touch the vehicle or Bill: coins and fuel cans */
bool car_touches_box(const Car *c, float x, float y, float half);
/* the fuel's use (1, or by the Fuel upgrade) */
float car_fuel_rate(int veh, const uint8_t levels[4]);
/* a bridge's planks, for drawing: how many, and each one's centre and angle */
int car_planks(CarBody *out, int most);
/* the trees near the car (Forest): their feet and angles, and which trees they are */
int car_trees(CarBody *out, int *which, int most);

#ifdef __cplusplus
}
#endif
#endif
