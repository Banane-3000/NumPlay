/* The calculator: EADK display, keyboard, time and files. */
#include <eadk.h>
#include "../../../common/epsilon_app.h"
#include "../../../common/epsilon_files.h"
#include "plat.h"

uint64_t plat_keys(void) { return eadk_keyboard_scan(); }
uint32_t plat_millis(void) { return (uint32_t)eadk_timing_millis(); }
uint32_t nw_slept_ms; /* (the time spent asleep: what is left is the work, nw/i_video.c) */
void plat_sleep(uint32_t ms) {
    nw_slept_ms += ms;
    eadk_timing_msleep(ms);
}
void plat_push(int x, int y, int w, int h, const uint16_t *px) {
  eadk_display_push_rect((eadk_rect_t){(uint16_t)x, (uint16_t)y, (uint16_t)w, (uint16_t)h}, px);
}
bool plat_vblank(void) { return eadk_display_wait_for_vblank(); }
void plat_pull(int x, int y, int w, int h, uint16_t *px) {
  eadk_display_pull_rect((eadk_rect_t){(uint16_t)x, (uint16_t)y, (uint16_t)w, (uint16_t)h}, px);
}
void plat_fill(int x, int y, int w, int h, uint16_t c) {
  if (w > 0 && h > 0) eadk_display_push_rect_uniform((eadk_rect_t){(uint16_t)x, (uint16_t)y, (uint16_t)w, (uint16_t)h}, c);
}
bool plat_save(const char *name, const void *data, uint32_t len) { return ef_write(name, data, len); }
const uint8_t *plat_load(const char *name, uint32_t *len) { return ef_read(name, len); }
uint8_t *plat_reserve(const char *name, uint32_t len) {
  uint32_t n;
  return ef_write(name, NULL, len) ? (uint8_t *)(uintptr_t)ef_read(name, &n) : NULL;   /* (the storage is RAM) */
}
void plat_reserved(const char *name, uint32_t len) { (void)name, (void)len; }
bool plat_remove(const char *name) { return ef_remove(name); }
void plat_begin(void) { np_app_begin(); }
int plat_end(void) { return np_app_end(); }
bool plat_slow(void) { return epsilon_slow_model(); }
