#ifndef ASSETS_H
#define ASSETS_H
#include <stdint.h>
typedef struct {uint16_t w,h;const uint8_t *pixels;const uint16_t *palette;} Image;
typedef struct {uint16_t offset;uint8_t w,h;int8_t xoff,yoff;uint8_t advance;} Glyph;
typedef struct {const Glyph*glyphs;const uint8_t*pixels;} Font;
extern const Font fonts[3];
typedef struct {uint16_t w,h;const uint8_t*pixels;} Mask;
extern const Mask logo,go;
typedef struct {uint8_t pins,shots,motion;} Level;
extern const Level levels[60];
extern const uint8_t preset_offset[];
extern const uint16_t preset_angle[];
#endif
