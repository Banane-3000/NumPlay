/* Subway Surfers: the people and the dog (the game's skinned models playing the game's animation clips), the
 * power-ups and coins, the boards. */
#ifndef SS_ACTORS_H
#define SS_ACTORS_H
#include "game.h"
#include "models.h"

/* what a model is playing: a clip and its time, and the clip it is fading from */
typedef struct {
  const Clip *a, *b;
  float ta, tb;
  float fade, rate;   /* how much of a (0..1), how fast it comes in (per second) */
} Anim;
/* plays c from time t (s), faded in over fade seconds (0: at once); the same clip again just goes on */
void anim_play(Anim *an, const Clip *c, float t, float fade);
void anim_step(Anim *an, float dt);
float anim_length(const Clip *c);
bool anim_done(const Anim *an);   /* a clip that does not loop is at its end */

enum { WHO_JAKE, WHO_TRICKY, WHO_FRESH, WHO_SPIKE, WHO_INSPECTOR, WHO_DOG };
/* a model standing at `at` (its feet), turned by yaw about the upright (0: facing ahead, +z), leaning forward by
 * lean (radians, about its hips); light: 1 outside, less in a tunnel */
void actor_draw(int who, const Anim *an, V3 at, float yaw, float lean, float light);
void board_draw(V3 at, int board, float yaw, float t, float light);
void jetpack_draw(float t, float light);   /* on the back of the person just drawn */
void shadow_draw(float x, float z, float ground, float height);
void actors_pickup(const Item *it, float t);   /* a power-up, a key or a mystery box, turning */
void actors_coin(const Item *it, float t, float light);
#endif
