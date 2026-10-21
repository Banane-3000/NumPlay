/* The RP2040's pins: none here. */
#ifndef NW_HARDWARE_GPIO_H
#define NW_HARDWARE_GPIO_H
#include "../pico.h"
#define gpio_put(pin, v) ((void)0)
#define gpio_init(pin) ((void)0)
#define gpio_set_dir(pin, d) ((void)0)
#endif
