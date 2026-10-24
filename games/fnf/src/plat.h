/* What the game needs from the calculator (plat_eadk.c), or from a computer for the tests. */
#ifndef FNF_PLAT_H
#define FNF_PLAT_H
#include <stdbool.h>
#include <stdint.h>

/* the four lanes: the arrows, or 7 8 ( ) */
enum { FK_LEFT = 1, FK_DOWN = 2, FK_UP = 4, FK_RIGHT = 8, FK_7 = 16, FK_8 = 32, FK_LPAR = 64, FK_RPAR = 128,
       FK_OK = 256, FK_BACK = 512, FK_HOME = 1024, FK_SHIFT = 2048 };
uint32_t plat_keys(void);
uint32_t plat_millis(void);
void plat_sleep(uint32_t ms);
void plat_push(int x, int y, int w, int h, const uint16_t *px);
bool plat_save(const char *name, const void *data, uint32_t len);
const uint8_t *plat_load(const char *name, uint32_t *len);
void plat_begin(void);
int plat_end(void);
#endif
