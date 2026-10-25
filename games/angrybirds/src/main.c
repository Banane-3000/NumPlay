/* Angry Birds for the NumWorks calculator.
 *
 * Inspired by Angry Birds by Rovio Entertainment, not affiliated with them. Made by Mason Chen as part of NumPlay,
 * from Bobydeluxe's adaptation; the physics is Box2D by Erin Catto. */
#include "draw.h"
#include "plat.h"
#include "ui.h"

#if defined(MEASURE) && !defined(HOST)
/* (a build for tools/emu.py: the physics' memory and time for the levels of MEASURE_LIST, each played with shots at
   set angles, in an arena of at most MEASURE_ARENA bytes, as the game's own arena is) */
#include "game.h"
#include "world.h"
static const int16_t measure_list[] = {MEASURE_LIST};
#define MEASURED (sizeof measure_list / sizeof *measure_list)
uint32_t measure_peak[MEASURED] __attribute__((used)), measure_ms[MEASURED] __attribute__((used));
uint32_t measure_step_max[MEASURED] __attribute__((used)), measure_side[MEASURED] __attribute__((used));
uint32_t measure_done __attribute__((used));
void np_push(int x, int y, int w, int h, const color_t *px) { plat_push(x, y, w, h, px); }
int main(void) {
  uint64_t work[WORK_BYTES / 8];
  world_work(work, (int)sizeof work);
  int size;
  void *heap = plat_heap(&size);
#ifdef MEASURE_ARENA
  if (size > MEASURE_ARENA) size = MEASURE_ARENA;
#endif
  for (unsigned k = 0; k < MEASURED; k++) {
    int l = measure_list[k];
    uint32_t t0 = plat_millis(), worst = 0;
    if (!game_load(l, heap, size)) {
      measure_peak[k] = 0xFFFFFFFFu;
      continue;
    }
    for (int s = 0; s < 700 && g.state != ST_WON && g.state != ST_LOST; s++) {
      if (game_can_launch()) {
        game_aim(0.15f + 0.12f * (float)(s % 5), 1.0f);
        game_launch();
      }
      if (s % 40 == 20) game_power();
      uint32_t a = plat_millis();
      game_step();
      uint32_t d = plat_millis() - a;
      if (d > worst) worst = d;
    }
    measure_peak[k] = (uint32_t)world_arena_peak() | (world_arena_failures() ? 0x80000000u : 0);
    measure_ms[k] = plat_millis() - t0;
    measure_step_max[k] = worst;
    measure_side[k] = (uint32_t)world_side_peak();
  }
  measure_done = 1;
  for (;;) plat_sleep(1000);
}
#elif !defined(HOST)
#include "world.h"
/* (read in tools/emu.py: frames drawn, the slowest drawing, frames slower than 33 ms, the slowest step of the
   game's time with its drawing) */
uint32_t perf_frames __attribute__((used));
uint32_t perf_max __attribute__((used));
uint32_t perf_slow __attribute__((used));
uint32_t perf_step_max __attribute__((used));

void np_push(int x, int y, int w, int h, const color_t *px) { plat_push(x, y, w, h, px); }

int main(void) {
  /* on main's stack, as the app's RAM goes to the physics: the strip the screen is drawn into, and while the game
     steps, the physics' memory for the step (world.h) */
  uint64_t work[WORK_BYTES / 8];
  plat_begin();
  int size;
  void *heap = plat_heap(&size);
  world_work(work, (int)sizeof work);
  ui_init(heap, size, (uint16_t *)work);
  for (;;) {
    uint32_t now = plat_millis();
    if (!ui_frame(plat_keys(), now)) break;
    if (ui_dirty()) {
      uint32_t t0 = plat_millis();
      ui_draw();
      uint32_t dt = plat_millis() - t0;
      if (dt > perf_max && perf_frames > 30) perf_max = dt;
      if (t0 - now + dt > 33) perf_slow++;
      if (t0 - now + dt > perf_step_max && perf_frames > 30) perf_step_max = t0 - now + dt;
      perf_frames++;
    } else {
      plat_sleep(5);
    }
  }
  return plat_end();
}
#endif
