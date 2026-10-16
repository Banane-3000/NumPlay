/* Undertale: shops and the Dimensional Box (shop.c). */
#ifndef UT_SHOP_H
#define UT_SHOP_H
#include <stdint.h>
enum { SHOP_SNOWDIN, SHOP_COUNT };
void shop_open(int id);   /* MODE_SHOP until Exit */
void box_open(void);
void shop_update(uint32_t keys, uint32_t pressed);
void shop_draw(void);
#endif
