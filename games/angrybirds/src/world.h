/* Angry Birds: the physics, Box2D 2.2.1 set up as the game sets it up (world.cpp).
 *
 * Every object of the game is a body of one fixture, numbered as the game numbers its objects. Box2D's memory is
 * an arena the game gives it (world_begin); when it runs out, Box2D's calls fail softly (see b2BlockAllocator.h). */
#ifndef AB_WORLD_H
#define AB_WORLD_H
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define W_STEP 0.033f             /* the game's step: 33 ms, 10 velocity and 10 position iterations */

enum { W_BULLET = 1, W_FIXED_ROTATION = 2, W_ASLEEP = 4 };
enum { W_DISTANCE = 1, W_WELD = 2, W_REVOLUTE = 3 };

typedef struct {
  float x, y, angle, vx, vy, w;
} WBody;

/* a fresh world of gravity (0, 20) in mem (8-byte aligned), for the level's shapes (its bodies and ground
 * rectangles) */
bool world_begin(void *mem, int size, int shapes);
void world_end(void);
void world_grow(int size);          /* the arena given back its top, now `size` bytes */
bool world_body(int obj, int type, float x, float y, float angle, int flags);
/* a static, unbreakable rectangle of the ground, belonging to object `obj` */
bool world_rect(int obj, int material, float x0, float y0, float x1, float y1);
/* a joint between objects a and b: anchors (local to a, local to b), as the game makes them */
bool world_joint(int kind, int a, int b, bool collide, float x1, float y1, float x2, float y2, bool motor, bool limit,
                 float speed, float torque, float lower, float upper);
void world_break_joints(void);     /* the Mighty Eagle's arrival */
void world_step(void);
void world_destroy(int obj);
bool world_has(int obj);
void world_get(int obj, WBody *b);
/* where it is and how it is turned (its sine and cosine), cheaply: for drawing */
bool world_xf(int obj, float *x, float *y, float *s, float *c);
bool world_is_static(int obj);
bool world_awake(int obj);
bool world_jointed(int obj);
float world_mass(int obj);
void world_set_velocity(int obj, float vx, float vy);
void world_set_angular(int obj, float w);
void world_set_transform(int obj, float x, float y, float angle);
void world_set_awake(int obj, bool awake);
void world_free_rotation(int obj);
void world_impulse(int obj, float ix, float iy, float px, float py);
void world_angular_impulse(int obj, float i);
/* the objects whose boxes meet the box: f(obj, ctx) */
void world_query(float x0, float y0, float x1, float y1, void (*f)(int obj, void *ctx), void *ctx);
#ifdef HOST
/* (tests) the whole world saved and put back, its arena included: a bot tries shots */
int world_snapshot_size(int arena_size);
void world_snapshot_save(void *to, const void *arena, int arena_size);
void world_snapshot_load(const void *from, void *arena, int arena_size);
#endif
int world_bodies(void);
int world_contacts(void);
int world_arena_used(void);
int world_arena_peak(void);
int world_arena_failures(void);
int world_side_peak(void);          /* the most of the step's buffer used */
/* the step's memory for its islands and their constraints, taken first from a buffer on the stack the screen's
   strip shares (the screen is drawn apart): WORK_BYTES, more than the strip */
#define WORK_BYTES (22 * 1024)
void world_work(void *mem, int size);

/* the game's side (game.c): a contact begins (once, as the game hears it), and whether a contact holds */
void game_contact_begin(int a, int b);
bool game_contact_enabled(int a, int b);

#ifdef __cplusplus
}
#endif
#endif
