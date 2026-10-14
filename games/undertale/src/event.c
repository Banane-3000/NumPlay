/* The event that runs now (event.h), and Frisk walking for a cutscene. */
#include <string.h>
#include "event.h"
#include "events.h"
#include "game.h"

static Ev cur;
static bool running;

void event_start(int id, Obj *self) {
  if (running || !id) return;
  memset(&cur, 0, sizeof cur);
  cur.id = id, cur.self = self;
  running = true;
  if (id != EV_ENCOUNTER) event_update();   /* (it starts this frame: a talk opens at once) */
}

bool event_running(void) { return running; }
bool event_blocking(void) { return running && !cur.free; }

void event_cancel(void) { running = false; }
void event_cur_arg(int v) { cur.v[0] = v; }
int g_exclaim;

void event_update(void) {
  if (!running) return;
  if (event_run(&cur)) running = false;
}

/* ---------------------------------------------------------------- Frisk walked */
static struct {
  int x, y, sp;
  bool on;
} walk;

void pl_walk_to(int x, int y, int sp) { walk.x = x, walk.y = y, walk.sp = sp ? sp : 3, walk.on = true; }
bool pl_walking(void) { return walk.on; }

/* (world.c calls it each frame while a cutscene runs) */
void pl_walk_step(void) {
  if (!walk.on) {
    pl.moving = 0;
    return;
  }
  int dx = walk.x - pl.x, dy = walk.y - pl.y;
  if (dx) pl.dir = dx < 0 ? DIR_LEFT : DIR_RIGHT;
  if (abs_i(dy) > abs_i(dx)) pl.dir = dy < 0 ? DIR_UP : DIR_DOWN;
  pl.x = (int16_t)(pl.x + clamp_i(dx, -walk.sp, walk.sp));
  pl.y = (int16_t)(pl.y + clamp_i(dy, -walk.sp, walk.sp));
  pl.anim++;
  pl.frame = (uint8_t)((1 + pl.anim / 6) % (pl.dir < 2 ? 4 : 2));
  pl.moving = 1;
  if (pl.x == walk.x && pl.y == walk.y) walk.on = false, pl.frame = 0, pl.anim = 0, pl.moving = 0;
}
