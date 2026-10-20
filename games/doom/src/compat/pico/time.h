/* The Pico SDK's clock, from the calculator's (nw/i_system.c). */
#ifndef NW_PICO_TIME_H
#define NW_PICO_TIME_H
#include <stdint.h>
uint64_t time_us_64(void);
static inline uint32_t time_us_32(void) { return (uint32_t)time_us_64(); }
#endif
