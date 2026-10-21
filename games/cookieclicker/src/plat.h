/* Cookie Clicker: the platform (the calculator, or a computer for tests).
 *
 * plat_eadk.c: the calculator's EADK display, keyboard, time and files.
 * plat_host.c: a frame buffer that tests save as images, scripted keys and
 * simulated time (tests/play.c). */
#ifndef CC_PLAT_H
#define CC_PLAT_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define SCREEN_W 320
#define SCREEN_H 240

/* keys held now, as bits */
enum {
  K_LEFT = 1 << 0, K_RIGHT = 1 << 1, K_UP = 1 << 2, K_DOWN = 1 << 3,
  K_OK = 1 << 4,        /* OK: click the big cookie, or choose */
  K_EXE = 1 << 5,       /* EXE: buy (or sell) what is chosen in the store */
  K_BACK = 1 << 6,      /* Back: the menu, or back */
  K_SHIFT = 1 << 7,     /* shift: click the golden cookie (or another shimmer) */
  K_HOME = 1 << 8,      /* Home, On/Off: save and quit */
  K_ALPHA = 1 << 9,     /* alpha: the buildings' rows, and their minigames */
  K_MUL = 1 << 10,      /* x: buy 1, 10 or 100 at a time */
  K_DIV = 1 << 11,      /* the division key: buy or sell */
  K_TOOLBOX = 1 << 12,  /* the toolbox key: the statistics */
  K_VAR = 1 << 13,      /* var: pop the wrinklers */
  K_DEL = 1 << 14,      /* the delete key: close the notes */
  K_D1 = 1 << 15,       /* the digits 1 to 9, then 0: K_D1 << (n - 1), 0 is the tenth */
  K_XNT = 1 << 25,      /* x,n,t: harvest the sugar lump */
  K_ANY = 1u << 31
};
#define K_DIGIT(n) (K_D1 << ((n) - 1))
#define K_DIGITS (0x3FF * K_D1)
uint32_t plat_keys(void);
uint32_t plat_millis(void);
void plat_sleep(uint32_t ms);
void plat_push(int x, int y, int w, int h, const uint16_t *px);
bool plat_save(const char *name, const void *data, uint32_t len);
const uint8_t *plat_load(const char *name, uint32_t *len);   /* NULL if absent */
/* a file of len bytes to write in place (the calculator's storage is RAM): plat_reserve gives its
 * bytes, plat_reserved says they are written; NULL if there is no room */
uint8_t *plat_reserve(const char *name, uint32_t len);
void plat_reserved(const char *name, uint32_t len);
bool plat_remove(const char *name);
uint32_t plat_random(void);
void plat_begin(void);
int plat_end(void);
/* an N0110 or N0115 (216 MHz, the N0120 has 550) */
bool plat_slow(void);
#endif
