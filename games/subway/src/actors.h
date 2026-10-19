/* Subway Surfers: the people and the dog, the power-ups and the boards, as boxes. */
#ifndef SS_ACTORS_H
#define SS_ACTORS_H
#include "game.h"

/* how a body is held: angles in radians */
typedef struct {
  float hip[2], knee[2];      /* legs, left and right: forward swing, knee bend */
  float arm[2], elbow[2];     /* arms: forward swing, elbow bend (forward) */
  float out[2];               /* arms raised sideways */
  float lean;                 /* the body forward */
  float bob;                  /* up and down */
  float spin;                 /* head over heels (a roll) */
  float tilt;                 /* sideways */
  float yaw;                  /* turned about the upright axis */
  float squash;               /* 1: as tall as it is; less while rolling */
} Pose;

enum { WHO_JAKE, WHO_TRICKY, WHO_FRESH, WHO_SPIKE, WHO_INSPECTOR };
void pose_run(Pose *p, float phase);
void pose_stand(Pose *p, float t);
void pose_jump(Pose *p, float t);
void pose_roll(Pose *p, float t);   /* t from 0 to 1 */
void pose_hover(Pose *p, float t);
void pose_fly(Pose *p, float t);
void pose_spray(Pose *p, float t);
void pose_fall(Pose *p, float t);
/* a person at (x, y, z), its feet on y; light: 1 outside, less in a tunnel */
void actor_draw(int who, V3 at, const Pose *p, float light);
void dog_draw(V3 at, float phase, float light);
void board_draw(V3 at, int board, float yaw, float t, float light);
void jetpack_draw(V3 at, const Pose *p, float t, float light);
void shadow_draw(float x, float z, float ground, float height);
void actors_pickup(const Item *it, float t);   /* a power-up, a key or a mystery box, turning */
#endif
