/* What the game needs from the calculator (plat_eadk.c), or from a computer for the tests. */
#ifndef TR_PLAT_H
#define TR_PLAT_H
#include <stdbool.h>
#include <stdint.h>

/* (TK_: NumPlay's np.h has K_ keys of its own) */
enum {
  TK_OK = 1, TK_UP = 2, TK_DOWN = 4, TK_LEFT = 8, TK_RIGHT = 16, TK_BACK = 32, TK_HOME = 64, TK_NITRO = 128,
  TK_GEAR_UP = 256, TK_GEAR_DOWN = 512
};
uint32_t plat_keys(void);
uint32_t plat_millis(void);
void plat_sleep(uint32_t ms);
void plat_push(int x, int y, int w, int h, const uint16_t *px);
void plat_vblank(void);
bool plat_save(const char *name, const void *data, uint32_t len);
const uint8_t *plat_load(const char *name, uint32_t *len);
uint32_t plat_random(void);
bool plat_slow(void);               /* an N0110 or N0115 */
void plat_begin(void);
int plat_end(void);
#endif
