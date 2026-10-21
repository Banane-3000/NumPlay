#ifndef ASSETS_H
#define ASSETS_H
#include <stdint.h>
typedef struct {uint16_t w,h;const uint8_t*pixels;const uint16_t*palette;} Image;
typedef struct {const uint8_t*data;const uint16_t*palette;const uint32_t*offset;} Background;
typedef struct {const char*name;uint16_t price,colors[18];uint8_t rotates,type,count,height;int8_t offset_x,offset_y;} BallStyle;
typedef struct {const char*name;uint16_t price,surface,line,ground;} Landscape;
typedef struct {uint16_t offset;uint8_t w,h;int8_t xoff,yoff;uint8_t advance;} Glyph;
typedef struct {const Glyph*glyphs;const uint8_t*pixels;} Font;
extern const Font fonts[3];
extern const Image PlayIcon,Shop,Score_Coin,crown,Pause,Balls;
extern const Image *const balls[],*const earths[];
extern const Background backgrounds[];
extern const BallStyle ball_styles[];
extern const Landscape landscapes[];
extern const uint8_t original_ball_ids[7];
extern const uint16_t legacy_ball_prices[26],legacy_scene_prices[7];
#endif
