/* Angry Birds: the tables of data.bin (tools/pack.py): the materials and how they hurt each other, the shapes, the
 * objects, the levels, the episodes and the pictures. */
#ifndef AB_RES_H
#define AB_RES_H
#include <stdbool.h>
#include <stdint.h>
#include "data.h"

typedef struct {
  float density, friction, restitution, strength, defense;   /* defense < 0: never broken */
  uint8_t is_static, pad[3];
} Material;

typedef struct {
  uint8_t a, b, pad[2];   /* what a does to b: */
  float damage, velocity; /* its damage times this, and what it keeps of its speed when it breaks b */
} Pair;

typedef struct {
  uint8_t kind, n;        /* kind 0: a circle of radius r; 1: a polygon of n vertices */
  int16_t ox4, oy4, pad;  /* the picture's offset from the body (its pixels at 20 a meter, x 4) */
  float r;
  float v[12];            /* the polygon's vertices, meters */
} Shape;

typedef struct {
  uint16_t shape;
  uint8_t material, cls;  /* cls: C_* (data.h) */
  uint16_t score;         /* points when it is destroyed */
  uint8_t damage_frames;  /* pictures for its damage */
  uint8_t frames;         /* its pictures in all */
  float max_health;       /* 0: from its size and material */
} Type;

typedef struct {
  uint32_t off;
  uint16_t packed, unpacked;
  uint32_t silver, gold, eagle;   /* two stars, three stars, the Mighty Eagle's 100 % */
  uint8_t theme, episode, birds, pad;
  float cam[8];                   /* the slingshot's view and the castle's: left, top, right, bottom */
} LevelInfo;

typedef struct {
  uint32_t off;                   /* in the pixels: 16 colours, then 4 bits a pixel */
  uint16_t w, h;
  int16_t px4, py4;               /* its pivot x 4 */
  uint16_t flags, pad;            /* flags 1: opaque */
} Pic;

typedef struct {
  uint16_t pic, flags;            /* flags 1: in front of the world; 2: repeated across */
  float parallax, x, y;           /* x and y: the game's pixels (20 a meter), y up from the ground to its bottom */
  float q;                        /* its pixels for one of the game's */
} Layer;
typedef struct {
  uint16_t sky, ground, layers, pad;
  Layer layer[8];
} Background;

extern const uint8_t ab_data[];
const void *res_section(int id, uint32_t *size);

const Material *res_material(int i);
const Type *res_type(int i);
const Shape *res_shape(int i);
const LevelInfo *res_level(int i);
typedef struct {
  uint16_t first, frames;         /* its pictures: undamaged, then more damaged (first 0xFFFF: none) */
  uint16_t flying, power;         /* a bird's, in flight and using its power (0xFFFF: none) */
} TypeArt;
const TypeArt *res_type_art(int type);
float res_damage(int attacker_material, int victim_material);
float res_velocity(int attacker_material, int victim_material);
int res_episode_first(int e);
int res_episode_count(int e);
int res_episode_of(int level);
int res_story_before(int level);       /* a story picture index, or -1 */
int res_story_after(int level);
int res_golden(int i);                 /* the level of the i-th golden egg (it opens Chrome Dimension's i-th) */
int res_unlock(int level);             /* the level its hidden egg opens, or -1 */
int res_episode_pic(int e);
int res_story_pic(int s);
const Pic *res_pic(int i);
const uint16_t *res_pic_palette(const Pic *p);
const uint8_t *res_pic_pixels(const Pic *p);
const Background *res_background(int theme);
const uint8_t *res_level_stream(int level);
#endif
