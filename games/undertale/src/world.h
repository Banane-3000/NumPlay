/* Undertale: the overworld. Rooms, the camera, Frisk, the room's objects.
 *
 * Positions are the area map's pixels (tools/rooms.py): the rooms of an area
 * keep their places on it, so walking out of a room's rectangle walks into the
 * next room, after a fade, as in the game. */
#ifndef UT_WORLD_H
#define UT_WORLD_H
#include <stdbool.h>
#include <stdint.h>
#include "res.h"

enum { DIR_DOWN, DIR_UP, DIR_LEFT, DIR_RIGHT };

/* an object of the room: a character, a SAVE Point, a sign, a trigger... */
enum {
  OBJ_NONE, OBJ_NPC, OBJ_SAVE, OBJ_SIGN, OBJ_TRIGGER, OBJ_DECO, OBJ_ROCK, OBJ_SPIKES, OBJ_SWITCH, OBJ_ITEM,
  OBJ_MARK
};
enum {
  OF_SOLID = 1,       /* Frisk can't walk through its box */
  OF_TALK = 2,        /* pressing Z facing it runs its event */
  OF_TOUCH = 4,       /* walking into its box runs its event */
  OF_HIDDEN = 8,      /* not drawn */
  OF_ANIM = 16,       /* its frames cycle (anim_speed) even standing */
  OF_WALKANIM = 32,   /* its frames cycle while it walks */
  OF_FRONT = 64,      /* drawn over everything else */
  OF_BACK = 128       /* drawn under everything else */
};
#define MAX_OBJS 48
typedef struct {
  uint8_t kind, dir;
  uint16_t flags;
  int16_t x, y;              /* its sprite's origin (the frame's top-left for the game's sprites) */
  int16_t bx, by, bw, bh;    /* its box, from (x, y) */
  uint16_t spr[4];           /* its sprite for each direction (spr[0] if it has one) */
  uint8_t frame, anim_speed, anim_t, depth_bias;
  uint16_t event;            /* EV_* run when talked to or touched */
  int16_t arg, arg2;         /* the kind's own (a sign's text, a flag...) */
  /* walking somewhere: (tx, ty) at speed pixels a frame */
  int16_t tx, ty;
  uint8_t speed, walking;
  uint8_t alpha;
  uint8_t name;              /* an ACTOR_* (scripts find it by it) */
  int16_t fx, fy;            /* fractions (1/16 pixel) for slow walks */
} Obj;
extern Obj objs[MAX_OBJS];

typedef struct {
  int16_t x, y;              /* Frisk's sprite's top-left */
  uint8_t dir, frame, anim, moving;
  bool frozen;               /* a cutscene has control */
  bool hidden;
  uint16_t steps;            /* towards an encounter */
} Player;
extern Player pl;

extern int g_room;            /* ROOM_* */
extern int cam_x, cam_y;      /* the screen's top-left on the map */
extern bool cam_locked;       /* a cutscene moves the camera */

const RoomInfo *world_room_info(void);
void world_enter(int room, int x, int y, int dir);   /* (x, y) Frisk's top-left; no fade */
void world_update(uint32_t keys, uint32_t pressed);
void world_draw(void);
bool world_walkable(int x, int y, int w, int h, int ignore);   /* a box on the map, objects too (not `ignore`) */
void world_camera(void);       /* follows Frisk, inside the room */
Obj *obj_new(int kind, int x, int y);
Obj *obj_find(int name);
void obj_clear_all(void);
/* the room fades out, then in: room changes in the middle */
void world_fade_to(int room, int x, int y, int dir);
bool world_fading(void);
extern uint8_t g_fade;         /* 0 clear, 255 black: drawn over the room */
#endif
