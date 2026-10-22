#ifndef MM_ASSETS_H
#define MM_ASSETS_H
#include <stdint.h>
typedef struct {uint16_t w,h;const uint8_t*pixels;const uint16_t*palette;} Image;
typedef struct {const uint8_t*data;const uint16_t*palette;const uint32_t*offset;uint16_t w,h;} Background;
typedef struct {uint16_t offset;uint8_t w,h;int8_t xoff,yoff;uint8_t advance;} Glyph;
typedef struct {const Glyph*glyphs;const uint8_t*pixels;} Font;
typedef struct {uint16_t sprite;int8_t x,y;} Pose;
extern const Font fonts[3];
extern const Image *const sprites[];
extern const Background backgrounds[2];
extern const Pose poses[8][6][8];
extern const Pose animals[2][3][2];
extern const uint16_t hats[26],food_icons[15],machine_sprites[15];
#include "sprite-ids.h"
#endif
