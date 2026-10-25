/* What the game needs from the calculator (plat_eadk.c), or from a computer for the tests. */
#ifndef PT_PLAT_H
#define PT_PLAT_H
#include <stdbool.h>
#include <stdint.h>

/* (PK_: NumPlay's np.h has K_ keys of its own) */
enum {
  PK_OK = 1, PK_UP = 2, PK_DOWN = 4, PK_LEFT = 8, PK_RIGHT = 16, PK_BACK = 32, PK_HOME = 64, PK_RESTART = 128,
  PK_VIEW = 256
};
uint32_t plat_keys(void);
uint32_t plat_millis(void);
void plat_sleep(uint32_t ms);
void plat_push(int x, int y, int w, int h, const uint16_t *px);
void plat_vblank(void);
bool plat_save(const char *name, const void *data, uint32_t len);
const uint8_t *plat_load(const char *name, uint32_t *len);
bool plat_slow(void);               /* an N0110 or N0115 */
void plat_begin(void);
int plat_end(void);
#endif
