/* Sonic the Hedgehog: the platform (the calculator, or a computer for tests).
 *
 * plat_eadk.c: the calculator's display, keyboard, time and files.
 * plat_host.c: a frame buffer that tests save as images, scripted keys and simulated time. */
#ifndef S1_PLAT_H
#define S1_PLAT_H
#include <stdbool.h>
#include <stdint.h>

#define SCREEN_W 320
#define SCREEN_H 240

/* keys held now, as the Mega Drive pad's buttons (and Home) */
enum {
  K_UP = 0x01, K_DOWN = 0x02, K_LEFT = 0x04, K_RIGHT = 0x08,
  K_B = 0x10, K_C = 0x20, K_A = 0x40, K_START = 0x80,
  K_HOME = 0x100,       /* Home, On/Off: save and quit */
  K_ANY = 0x200
};
uint32_t plat_keys(void);
uint32_t plat_millis(void);
void plat_sleep(uint32_t ms);
void plat_push(int x, int y, int w, int h, const uint16_t *px);
void plat_fill(int x, int y, int w, int h, uint16_t c);
bool plat_save(const char *name, const void *data, uint32_t len);
const uint8_t *plat_load(const char *name, uint32_t *len);   /* NULL if absent */
/* a file of len bytes to write in place: plat_reserve gives its bytes, plat_reserved says they are
 * written; NULL if there is no room */
uint8_t *plat_reserve(const char *name, uint32_t len);
void plat_reserved(const char *name, uint32_t len);
bool plat_remove(const char *name);
void plat_begin(void);
int plat_end(void);
bool plat_slow(void);    /* an N0110 or N0115 (216 MHz, the N0120 has 550) */
#endif
