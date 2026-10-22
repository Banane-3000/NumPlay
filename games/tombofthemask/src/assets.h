#ifndef TOTM_ASSETS_H
#define TOTM_ASSETS_H
#include <stdint.h>
typedef struct {uint32_t offset;uint16_t w,h;} Sprite;
typedef struct {uint32_t offset;uint16_t size;uint8_t w,h,zone,part,format;} Map;
typedef struct {uint16_t offset;uint8_t w,h,advance;int8_t x,y;} Glyph;
extern const Sprite sprites[];
extern const uint8_t sprite_bits[],sprite_ink[],map_bytes[],font_bits[];
extern const Map maps[717];
extern const Glyph font_glyphs[96];
extern const uint16_t theme_colors[13];
extern const uint8_t mask_levels[16];
extern const char *const mask_names[16];
extern const uint32_t xp_targets[20];
extern const uint16_t upgrade_prices[9],mask_prices[16];
extern const uint8_t powerup_effects[4][10],mask_features[16][9];
extern const uint16_t mission_goals[8][7],mission_rewards[7];
extern const uint16_t hero_sprites[16][4][10];
int map_decode(unsigned map,uint8_t *out,unsigned capacity);
#include "sprite-ids.h"
#endif
