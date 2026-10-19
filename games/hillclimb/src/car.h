/* Hill Climb Racing: a vehicle's physics.
 *
 * The chassis (with Bill) and each wheel are rigid bodies in the plane. A wheel hangs from the chassis on a
 * spring and damper along the chassis' own up and down, held there by a joint; the engine turns the driven wheels
 * against the chassis, so it rears up when you floor it and turns in the air. The ground pushes on the wheels,
 * on the chassis' outline and, at the end, on Bill's head. Impulses are solved in turn (sequential impulses) at
 * 240 steps a second. Meters, seconds, kilograms; y up. */
#ifndef HC_CAR_H
#define HC_CAR_H
#include <stdbool.h>
#include <stdint.h>

#define CAR_MAXW 6
#define CAR_HZ 60          /* car_step's rate */

typedef struct { float x, y, a, vx, vy, w, im, ii; } Body;   /* centre of mass, angle; inverse mass and inertia */

typedef struct {
  int veh, nw;
  Body ch, wh[CAR_MAXW];
  float wr[CAR_MAXW], spin[CAR_MAXW];
  float anc[CAR_MAXW][2];   /* the tops of the springs, from the centre of mass (chassis frame) */
  float s0, smax;           /* the springs' length at rest unloaded, the most they let a wheel hang */
  float k, c;               /* stiffness, damping */
  float torque, wmax, brake, drive[CAR_MAXW];
  float grip, air, down;
  float com[2];             /* the centre of mass from the picture's origin */
  float hull[16][2];
  int nhull;
  float head[2];            /* Bill's head's centre, from the centre of mass */
  bool ground[CAR_MAXW], touch, head_hit;
  float wheel_load[CAR_MAXW];
  float rpm;                /* 0 to 1, for the meter */
  float wob[2], wobv[2];    /* Bill's head nodding on its neck (chassis frame) */
  float last_vx, last_vy;
  int tree_broken;          /* a tree knocked down this step, or -1 */
  int tree_hurt_id;         /* the tree being pushed on, and how hard so far (N s) */
  float tree_hurt;
} Car;

/* levels: the vehicle's four upgrades (1 and up) */
void car_init(Car *c, int veh, const uint8_t levels[4], float x);
/* one 1/60 s: gas and brake from 0 to 1; no fuel left: the engine is off */
void car_step(Car *c, float gas, float brake, bool fuel);
/* the picture's origin and angle: where to draw it */
void car_origin(const Car *c, float *x, float *y);
/* a point of the picture's frame (meters, y up) in the world */
void car_point(const Car *c, float lx, float ly, float *wx, float *wy);
void car_head(const Car *c, float *x, float *y);   /* Bill's head's centre in the world (nodding) */
float car_speed(const Car *c);                      /* along the ground, m/s */
bool car_airborne(const Car *c);
float car_fuel_seconds(int veh, const uint8_t levels[4]);
#endif
