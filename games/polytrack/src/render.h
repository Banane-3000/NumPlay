/* The 3D picture (render.c): PolyTrack's sky, ground and mountains, the track's parts lit by its sun, the car, its
 * shadow and the ghost, as flat triangles drawn with a depth buffer a strip of the screen at a time. */
#ifndef PT_RENDER_H
#define PT_RENDER_H
#include "car.h"
#include "pt.h"

typedef struct {
  v3 pos;
  v3 right, up, fwd;            /* the camera's axes in the world */
  float fov;                    /* vertical, degrees */
} camera_t;

/* the chase camera (main.bundle.js): a point trailing 5.5 m behind the car on a rope, 2 m up the car's (eased) up */
typedef struct {
  v3 anchor, up;
} chase_t;
void chase_reset(chase_t *ch, v3 pos, qt rot);
void chase_update(chase_t *ch, camera_t *cam, float dt, v3 pos, qt rot, float kmh);
void cockpit_camera(camera_t *cam, v3 pos, qt rot, float kmh);

/* what a car looks like this frame */
typedef struct {
  v3 pos;
  qt rot;
  float susp[4], steer, spin[4];
  uint8_t brake, ghost, paint;
} car_look;
car_look car_look_of(const car_t *c);
car_look car_look_mix(const car_look *a, const car_look *b, float t);

extern const uint32_t rd_paints[8];   /* the car colours */
extern int rd_detail;                  /* 0..3: how far it draws; lowered when a frame is slow */
extern bool rd_half;                   /* half the rows (each drawn twice), when even 0 is too slow */
void rd_adapt(uint32_t spent_ms);      /* after each race frame: the quality for the next */
#define RD_MAX_TRIS 1700              /* triangles in a frame, at most */
extern int rd_tris, rd_dropped, rd_mtris, rd_chunks;   /* the last frame: triangles kept, some left out, of the mountains; chunks */

void rd_begin(const camera_t *cam, int env, int sun, bool sky_only);
void rd_track(void);
void rd_car(const car_look *c);
void rd_shadow(const car_look *c, const car_t *phys);
void rd_draw(void);                    /* the strip gfx_y0..gfx_y1 of gfx_buf */
/* a car alone on a turntable (the menus) */
void rd_showroom(int paint, float angle, int env);
#endif
