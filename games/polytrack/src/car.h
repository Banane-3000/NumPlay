/* The car (car.c): PolyTrack's physics, a port of what its polytrack_physics.wasm does each 1 ms frame. A Bullet
 * rigid body of 400 kg with a convex hull, a btRaycastVehicle with four wheels, the downforce and the steering PolyTrack
 * adds, the checkpoints and the finish. Floats, rounded the same on every machine (see pt.h). */
#ifndef PT_CAR_H
#define PT_CAR_H
#include "pt.h"
#include "world.h"

#define PT_HZ 1000
#define PT_MAX_FRAMES 5999998u

enum { IN_UP = 1, IN_RIGHT = 2, IN_DOWN = 4, IN_LEFT = 8, IN_RESET = 16 };

typedef struct {
  /* raycast info */
  v3 normal, point, hard, dir, axle;
  float susp;                       /* suspension length */
  uint8_t contact, ground;
  /* the rest of btWheelInfo that changes */
  float steering, rotation, drot;
  float engine, brake, stiffness;
  float clipped, relvel, force, skid;
} wheel_t;

typedef struct {
  v3 pos;
  qt rot;
  uint16_t next;                    /* the next checkpoint (index into the sorted checkpoints) */
} respawn_t;

typedef struct {
  /* the rigid body */
  v3 pos, vel, ang;
  qt rot;
  m3 basis, inv_inertia;            /* world */
  float ang_damp;
  wheel_t w[4];
  float speed;                      /* km/h, minus going backwards (btRaycastVehicle) */
  float steer_in;                   /* -1 .. 1, eased */
  float air, lock;                  /* seconds in the air, engine lock after a long jump */
  uint32_t frames;                  /* 1 ms frames since the start */
  uint32_t finish;                  /* frames at the finish, 0 while racing */
  uint16_t next_cp;
  uint8_t has_respawn, reset_held, brake_light, ground_on;
  respawn_t respawn;
  uint32_t cp_frames[64];           /* when each checkpoint was taken (for the splits) */
  /* the physics triangles round the car */
  v3 cache_at;
  int ncache;
  uint8_t cache_ok;
  float impulse;                    /* the biggest contact impulse this frame (a bump) */
} car_t;

extern const v3 car_wheel_pos[4];
#define CAR_MASS_OFFSET 0.6f
#define CAR_RADIUS 0.331f

void car_reset(car_t *c);           /* at the start, as before the first frame */
void car_step(car_t *c, unsigned input);
bool car_finished(const car_t *c);
uint32_t car_hash(const car_t *c);  /* the state's bits, for the tests */
void car_cache_rebuild(car_t *c);   /* after putting back a saved state */
int car_checkpoints(void);          /* checkpoints on the track (orders) */
#endif
