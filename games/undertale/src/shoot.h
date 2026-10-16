/* Undertale: Hotland's shooting puzzles (shoot.c), on their own screen. */
#ifndef UT_SHOOT_H
#define UT_SHOOT_H
#include <stdint.h>
enum { SHOOT_RIGHT, SHOOT_LEFT, SHOOT_NORTH, SHOOT_SOUTH, SHOOT_SAGE, SHOOT_COUNT };   /* (the first floor's, the third's, the CORE's) */
void shoot_open(int id);      /* MODE_SHOOT until solved or left */
int shoot_result(void);       /* 1 solved, 0 left */
void shoot_update(uint32_t keys, uint32_t pressed);
void shoot_draw(void);
#endif
