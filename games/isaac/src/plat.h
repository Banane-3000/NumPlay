/* The Binding of Isaac: the platform (the calculator, or a computer for tests).
 *
 * plat_eadk.c: the calculator's EADK display, keyboard, time and files.
 * plat_host.c: a frame buffer that tests save as images, scripted keys and
 * simulated time (tests/play.c). */
#ifndef IS_PLAT_H
#define IS_PLAT_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define SCREEN_W 320
#define SCREEN_H 240

/* keys held now, as bits. The arrows walk, 8 4 6 2 (and 5) cry tears the
 * way they point, like the game's arrow keys; 9 drops a bomb, 7 or OK uses
 * the active item, 1 or 3 the pill or card, the toolbox key opens the map. */
enum {
  K_LEFT = 1 << 0, K_RIGHT = 1 << 1, K_UP = 1 << 2, K_DOWN = 1 << 3,
  K_SLEFT = 1 << 4, K_SRIGHT = 1 << 5, K_SUP = 1 << 6, K_SDOWN = 1 << 7,
  K_OK = 1 << 8,        /* OK, EXE: confirm in menus; the active item in a room */
  K_BACK = 1 << 9,      /* Back: pause, cancel */
  K_BOMB = 1 << 10,     /* 9, shift */
  K_ITEM = 1 << 11,     /* 7 (OK too, in a room) */
  K_CARD = 1 << 12,     /* 1, 3: the pill or card */
  K_MAP = 1 << 13,      /* toolbox, var: the whole map */
  K_HOME = 1 << 14,     /* Home, On/Off: keep the run and quit */
  K_ANY = 1 << 15
};
#define K_MOVE (K_LEFT | K_RIGHT | K_UP | K_DOWN)
#define K_SHOOT (K_SLEFT | K_SRIGHT | K_SUP | K_SDOWN)

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
void plat_begin(void);
int plat_end(void);
bool plat_slow(void);   /* an N0110 or N0115 (216 MHz, the N0120 has 550) */
#endif
