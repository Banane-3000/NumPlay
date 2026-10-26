/* The frame rate on the calculator's processor, for tools/armperf.py: a track's recorded run (routes.h) driven
 * with its exact keys, drawn as the game draws a race (the picture's detail going down when a frame is slow and up
 * when there is time, as in main.c), each frame's work timed. tools/emu.py counts time by instructions, as fast as
 * an N0110: the slowest calculators. */
#include <eadk.h>
#include <string.h>
#include "../src/car.h"
#include "../src/draw.h"
#include "../src/render.h"
#include "../src/save.h"
#include "routes.h"

const char eadk_app_name[] __attribute__((section(".rodata.eadk_app_name"))) = "PTPerf";
const uint32_t eadk_api_level __attribute__((section(".rodata.eadk_api_level"))) = 0;

/* in: the track; out: frames, work (ms) in all, the slowest, frames over 33 ms, detail levels used, the finish */
__attribute__((used, externally_visible)) volatile uint32_t perf_track;
__attribute__((used, externally_visible)) uint32_t perf_out[8 + 4], perf_done;

void plat_push(int x, int y, int w, int h, const uint16_t *px) {
  eadk_display_push_rect((eadk_rect_t){(uint16_t)x, (uint16_t)y, (uint16_t)w, (uint16_t)h}, (const eadk_color_t *)px);
}

static car_t car;
static pt_file sv;
static pt_ui ui;
static chase_t chase;
static camera_t cam;
static uint8_t slots[60000];

int main(void) {
  int t = (int)perf_track;
  int n = 0;
  for (const char *s = pt_routes[t]; *s && n < (int)sizeof slots;) {
    char ch = *s++;
    if (ch < 'a' || ch > 'p') continue;
    int r = 0;
    while (*s >= '0' && *s <= '9') r = r * 10 + (*s++ - '0');
    for (r = r ? r : 1; r-- && n < (int)sizeof slots;) slots[n++] = (uint8_t)(ch - 'a');
  }
  memset(&sv, 0, sizeof sv);
  sv.p.track = (uint8_t)t;
  world_load(t);
  car_reset(&car);
  chase_reset(&chase, car.pos, car.rot);
  memset(&ui, 0, sizeof ui);
  ui.screen = V_RACE, ui.started = 1;
  rd_detail = 3;
  uint32_t dt = 33, slow = 0, quick = 0, f = 0;
  while (!car.finish && f < 600000) {
    uint32_t t0 = (uint32_t)eadk_timing_millis();
    for (uint32_t i = 0; i < dt && !car.finish; i++, f++) car_step(&car, f / 10 < (uint32_t)n ? slots[f / 10] : 0);
    car_look look = car_look_of(&car);
    chase_update(&chase, &cam, (float)dt / 1000.0f, look.pos, look.rot, car.speed);
    ui.clock += dt;
    pt_render(&sv, &car, &cam, &look, NULL, &ui);
    uint32_t spent = (uint32_t)eadk_timing_millis() - t0;
    perf_out[0]++, perf_out[1] += spent;
    if (spent > perf_out[2]) perf_out[2] = spent;
    if (spent > 33) perf_out[3]++;
    perf_out[4 + (rd_detail & 3)]++;
    if (spent > 30 || rd_dropped) slow++, quick = 0;
    else if (spent < 18 && rd_tris < RD_MAX_TRIS * 3 / 4) quick++, slow = 0;
    if (slow >= 2 && rd_detail > 0) rd_detail--, slow = 0;
    if (quick >= 30 && rd_detail < 3) rd_detail++, quick = 0;
    dt = spent > 33 ? spent : 33;     /* (the game's frames: 33 ms at least on these calculators) */
  }
  perf_out[8] = car.finish;
  perf_done = 1;
  return 0;
}
