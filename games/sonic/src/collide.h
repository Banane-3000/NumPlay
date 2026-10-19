/* Collision with the level (collide.c). Distances are in pixels from the point to the surface
 * (negative: inside it); bit is $D for floors you can stand on from above, $E for solid all round;
 * eor flips the block's reading ($800 for walls to the left, $1000 for ceilings); step goes to the
 * next block ($10, or -$10 the other way). */
#ifndef S1_COLLIDE_H
#define S1_COLLIDE_H
#include "s1.h"
const uint16_t *FindNearestTile(Obj *o, int16_t y, int16_t x);
extern uint16_t col_tile;   /* the block word found last */
int16_t FindFloor(Obj *o, int16_t y, int16_t x, int bit, uint16_t eor, int16_t step, uint8_t *angle, uint16_t *d4);
int16_t FindWall(Obj *o, int16_t y, int16_t x, int bit, uint16_t eor, int16_t step, uint8_t *angle, uint16_t *d4);
int16_t ObjFloorDist(Obj *o, uint8_t *angle);
int16_t ObjFloorDist2(Obj *o, int16_t x, uint8_t *angle);
int16_t ObjHitWallRight(Obj *o, int16_t dx, uint8_t *angle);
int16_t ObjHitWallLeft(Obj *o, int16_t dx, uint8_t *angle);
int16_t ObjHitCeiling(Obj *o, uint8_t *angle);
#endif
