/* The ride: the dunes, the ball and its physics, the score and the coins (game.c), stepped 60 times a second
 * as the web game's Phaser scene and its p2 physics world. Distances are the game's points (a 900 x 665 screen,
 * y down); the calculator shows 320/900 of a point per pixel. */
#ifndef DN_GAME_H
#define DN_GAME_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define DN_SEGS 9          /* the dune segments, made once: 0 the flat one, 1..7 in play, 8 set aside */
#define DN_PTS 64          /* curve points n = 0..63 */
#define DN_COINS 80
#define DN_W 900.0f        /* the game's width on a 4:3 screen */
#define DN_H 665.0f
#define DN_TY 392.0f       /* the segments' top: centerY (665 >> 1) + 60 */
#define DN_BALLS 26
#define DN_LANDS 7

typedef struct {
  float x, d, w;           /* left edge; the width before the cut (x(n) = d/90 n); the width (0.7 d) */
  float h, m;              /* the curve's height and divisor: y(n) = sin(n/20) h n / m */
  uint8_t flat, sticky;    /* sticky: no material, so p2's default one (friction 0.3, stiffness 1e6) */
} dn_seg;

/* made from a segment by dn_shape (not saved) */
typedef struct {
  float y[DN_PTS];         /* the curve at n */
  float lx[9], ly[9];      /* collision points: n = 0, 8 .. 56 and (w, 0), 15 down */
  float line[8][6];        /* p2's Line shapes between them: ends (from the segment's top left) and direction */
} dn_curve;

typedef struct {
  float x, y;
  uint8_t on, shrink;      /* shrink: frames left of the pick-up (100 ms) */
  uint8_t touch;           /* the ball overlaps its sensor (p2 keeps this for the body, wherever it goes) */
} dn_coin;

/* the landscapes' moving parts, in screen points (SceneBackground.update) */
typedef struct {
  float alpha;             /* the moon's or the sun's */
  float tx, ty, tx2, ty2;  /* the stars' or the falling snow's tile offsets */
  float start[7], scale[7], y[7], ax[7];
  uint32_t rng;
} dn_bg;

enum { DN_POP_NONE, DN_POP_PERFECT, DN_POP_NICE };

typedef struct {
  dn_seg seg[DN_SEGS];
  uint8_t order[8];        /* the game's terrains[]: order[0] is always the flat segment */
  uint8_t spare;           /* unusedTerrains[0] */
  uint8_t cur;             /* currentTerrain */
  float x, y, vx, vy, w, angle;   /* the ball (w: its spin, rad/s) */
  float damping;
  float zoom, cam_y, zoom_from;
  float trail_sx, trail_rot;
  float last_vx, last_vy, slide_start;
  uint32_t score, picked, spawned, frame, rng;
  uint32_t rebased;        /* times everything moved back 65536 points (so the numbers keep their precision) */
  uint32_t ground[DN_SEGS], slider[DN_SEGS];   /* lines touched by the ground sensor and the slide sensor */
  uint16_t airborn, start_wait;
  uint8_t hist, polys, slide_lines, touching, above;
  uint8_t sliding, can_slide, running, alive, valid, moving, guide, tick;
  uint8_t ball, rotates, land; /* the ball's style (fixed rotation unless it turns) and the landscape */
  /* effects, in frames */
  uint8_t line_t, line_n, slide_t, slide_kind, slide_hold, flash_t, shake_t, crash_t;
  float slide_y0, slide_y1;
  dn_coin coin[DN_COINS];
  dn_bg bg;
  dn_curve curve[DN_SEGS]; /* (last: everything before it is what a save keeps) */
} dn_run;
#define DN_RUN_SAVED ((uint32_t)offsetof(dn_run, curve))

/* xorshift32, as Math.random() */
uint32_t dn_rand(uint32_t *s);
float dn_frand(uint32_t *s);

void dn_shape(const dn_seg *s, dn_curve *c);    /* the curve and collision points from d, h, m */
void dn_shapes(dn_run *r);                      /* all of them (after loading a run) */
void dn_make_seg(dn_seg *s, float r0, float r1, float r2, bool flat);
void dn_new(dn_run *r, uint32_t seed, uint8_t ball, uint8_t land);   /* the nine segments, the ball at rest */
void dn_reset(dn_run *r);                       /* ResetLevel */
void dn_start(dn_run *r);                       /* StartGame: the level runs 300 ms later */
void dn_menu(dn_run *r);                        /* ScenePause.OnPressedMenu: EndGame and ResetLevel */
void dn_step(dn_run *r, bool held, bool pressed);
void dn_style(dn_run *r, uint8_t ball, uint8_t land);
bool dn_valid(const dn_run *r);
float dn_curve_at(const dn_run *r, int seg, float x);   /* the drawn surface's y at world x (from DN_TY) */
int dn_seg_at(const dn_run *r, float x);        /* the segment drawn at world x, or -1 */
static inline bool dn_crashed(const dn_run *r) { return !r->alive; }
/* the ball's distance from the start, as the backgrounds scroll by it */
static inline float dn_world_x(const dn_run *r) { return r->x + r->rebased * 65536.0f; }
#ifdef HOST
extern void (*dn_trace)(const dn_run *r, int after);
#endif
#endif
