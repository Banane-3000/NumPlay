/* Subway Surfers: the scenery and what stands on the tracks, as polygons (r3d.h), and the sky and the
 * ground behind them, drawn row by row. */
#ifndef SS_SCENE_H
#define SS_SCENE_H
#include "game.h"

void scene_init(void);
/* the camera behind and above the runner (or for the title screen), looking down by pitch (radians), with focal
 * length f (pixels) */
void scene_camera(float x, float y, float z, float pitch, float f);
void scene_draw(float t);      /* the world in front of the camera; t: the time, for spinning coins */
void scene_background(void);   /* r3d band callback: the sky and the ground */
/* how lit things are at z: 1 outside, darker in a tunnel */
float scene_light(float z);
uint32_t scene_lit(uint32_t rgb, float z);
#endif
