/* The game (game.c): a level's course and the stickman on it, stepped 60 times a second with the web
 * game's own rules and numbers (see the spec in the README's credits). World units: the screen's height
 * is 1, x grows to the right, y down. */
#ifndef SH_GAME_H
#define SH_GAME_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "levels.h"

#define SH_SKINS 23
#define SH_FIXED (SH_INTRO + SH_RECIPES)   /* 39 levels made by hand, then courses drawn at random */
#define SH_RADIUS 0.017f
#define SH_TRAIL 61                         /* the trail's points: 20 (60 after the finish) and the newest */
#define SH_CONFETTI 50
#define SH_RATIO (320.0f / 240.0f)          /* the screen's width, in heights */
#define SH_NO_PART 255

typedef struct { float x, y; } sh_vec;
typedef struct { float spawn_x, spawn_y, finish; uint16_t hook, bumper, wall; uint8_t hooks, bumpers, walls, background; } sh_part;
typedef struct { float x, y; uint8_t kind; } sh_bumper;
typedef struct { float angle, c, s, bounce, force, aim; uint8_t aimed; } sh_bumper_kind;
typedef struct { float x, y; uint8_t turn, shape; } sh_wall;
typedef struct { float x, y, dx, dy, nx, ny, inv; } sh_edge;
typedef struct { uint16_t first, count; float radius; } sh_shape;

extern const sh_part sh_parts[SH_PARTS];
extern const sh_vec sh_hooks[];
extern const sh_bumper sh_bumpers[];
extern const sh_bumper_kind sh_bumper_kinds[];
extern const sh_wall sh_walls[];
extern const sh_vec sh_turns[];
extern const float sh_turn_angles[];
extern const sh_edge sh_edges[];
extern const sh_shape sh_shapes[];
extern const uint8_t sh_recipes[SH_RECIPES][3];
extern const uint8_t sh_unlock_level[SH_SKINS];

/* a course: up to three sections (an introductory level is one) and its background */
typedef struct { uint8_t part[3], background; } sh_course;

typedef struct { float x, y, vx, vy, angle, w, h; uint16_t color; } sh_confetto;

enum { SH_SPAWN, SH_HOOKED0, SH_HOOKED1, SH_HOOKED2, SH_BALL, SH_VICTORY };

typedef struct {
  uint32_t level, attempt, ticks;
  sh_course course;
  float offset[3], finish, spawn_x, spawn_y;
  /* the stickman */
  float x, y, vx, vy, theta, omega, r, pull, rotation, spin, hook_x, hook_y;
  uint8_t hooked, held, completed, ready, pose, facing, sections, spawn_fx;
  /* what's seen: the camera (screen x = 240 * (x * zoom + cam_x)), the closest hook's ring */
  float progress, cam_offset, cam_x, cam_y, zoom, complete_time, near_scale;
  int16_t near;
  uint8_t trail_count, trail_max;
  sh_vec trail[SH_TRAIL];
  uint8_t bumper_anim[SH_MAX_BUMPERS];   /* a bumper's bounce: ticks since it was hit, plus one; 0 at rest */
  sh_confetto confetti[SH_CONFETTI];
} sh_game;

uint32_t sh_rand(uint32_t *rng);
float sh_random(uint32_t *rng);                       /* [0, 1), as Math.random() */
void sh_pick(uint32_t level, uint32_t *rng, sh_course *c);   /* the course the game makes for a level */
bool sh_course_ok(uint32_t level, const sh_course *c);
void sh_new(sh_game *g, uint32_t level, const sh_course *c);
void sh_spawn(sh_game *g);                            /* back to the start: a death, or Restart */
void sh_step(sh_game *g, bool held, uint32_t *rng);    /* a 60th of a second */
bool sh_valid(const sh_game *g);
int sh_hook_count(const sh_game *g);
sh_vec sh_hook_at(const sh_game *g, int i);
int sh_bumper_count(const sh_game *g);
uint32_t sh_fake_max(uint32_t level);                 /* the "N / M" goal: the next multiple of 5 */
bool sh_skin_open(uint32_t levels_done, int skin);

/* the title's stickman, hanging from the logo */
typedef struct { float hx, hy, theta, omega, r, pull, x, y; uint8_t pose, facing; } sh_swing;
void sh_swing_start(sh_swing *s, float hx, float hy, float x, float y);
void sh_swing_step(sh_swing *s, float dt);
#endif
