/* Angry Birds: what the game needs from the calculator (plat_eadk.c), or from a computer for the tests. */
#ifndef AB_PLAT_H
#define AB_PLAT_H
#include <stdbool.h>
#include <stdint.h>

enum {
  KB_OK = 1, KB_UP = 2, KB_DOWN = 4, KB_LEFT = 8, KB_RIGHT = 16, KB_BACK = 32, KB_HOME = 64, KB_SHIFT = 128,
  KB_EAGLE = 256   /* 1 */
};
uint32_t plat_keys(void);
uint32_t plat_millis(void);
void plat_sleep(uint32_t ms);
void plat_push(int x, int y, int w, int h, const uint16_t *px);
bool plat_save(const char *name, const void *data, uint32_t len);
const uint8_t *plat_load(const char *name, uint32_t *len);
/* the RAM past the app's data, for the physics (the installer's heap), and its size */
void *plat_heap(int *size);
bool plat_slow(void);
void plat_begin(void);
int plat_end(void);
#endif
