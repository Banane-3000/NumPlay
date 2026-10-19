/* Subway Surfers: the people and the dog (meshes on skeletons, keyframed), the power-ups, the boards. */
#ifndef SS_ACTORS_H
#define SS_ACTORS_H
#include "game.h"

#define NBONES 11
enum { BN_HIPS, BN_CHEST, BN_HEAD, BN_ARM_L, BN_FORE_L, BN_ARM_R, BN_FORE_R, BN_LEG_L, BN_SHIN_L, BN_LEG_R, BN_SHIN_R };

/* how a body is held: each bone turned from the T pose the models are made in (radians: about x, then
 * y, then z; arms are lowered about z, everything swings forward about -x), and the whole body */
typedef struct {
  float a[NBONES][3];
  float y;            /* raised (m) */
  float lean, tilt;   /* forward, sideways (about the hips, or the middle of a roll) */
  float yaw;          /* turned about the upright axis */
  float spin;         /* head over heels, about the middle of the body curled up (a roll) */
} Pose;

enum { WHO_JAKE, WHO_TRICKY, WHO_FRESH, WHO_SPIKE, WHO_INSPECTOR };
void pose_run(Pose *p, float phase);
void pose_stand(Pose *p, float t);
void pose_jump(Pose *p, float t);    /* t from 0 (leaving the ground) to 1 (landing) */
void pose_roll(Pose *p, float t);    /* t from 0 to 1 */
void pose_hover(Pose *p, float t);
void pose_fly(Pose *p, float t);
void pose_spray(Pose *p, float t);
void pose_fall(Pose *p, float t);    /* t from 0 (the crash) to 1 (lying down) */
void pose_catch(Pose *p, float t);   /* the Inspector, his hands on the runner */
void pose_stumble(Pose *p, float k); /* a trip, k from 1 (just now) to 0, over the pose */
/* a person at (x, y, z), its feet on y; light: 1 outside, less in a tunnel */
void actor_draw(int who, V3 at, const Pose *p, float light);
void dog_draw(V3 at, float phase, float light);
void dog_draw_turned(V3 at, float phase, float light, float yaw);
void board_draw(V3 at, int board, float yaw, float t, float light);
void jetpack_draw(V3 at, const Pose *p, float t, float light);   /* on the back of the person just drawn */
void shadow_draw(float x, float z, float ground, float height);
void actors_pickup(const Item *it, float t);   /* a power-up, a key or a mystery box, turning */
#endif
