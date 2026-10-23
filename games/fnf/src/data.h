#ifndef FNF_DATA_H
#define FNF_DATA_H
#include <stdint.h>
#include "ids.h"
#define NONE 65535u
typedef struct { uint32_t offset,size; uint16_t w,h,palette; int16_t ox,oy; uint8_t compressed; } Sprite;
typedef struct { uint16_t frame[4],name; uint8_t count; } Animation;
typedef struct { const char *name; uint16_t first,count,idle,dance,sing[4],miss[4],icon[2],sing_time; } Character;
typedef struct { int32_t time; float bpm,beat; } Tempo;
typedef struct { int32_t time; float value,duration; int16_t a,b,c; uint16_t kind; } Event;
typedef struct { uint32_t offset,bytes; int32_t end; uint16_t count,players,track; uint8_t difficulty; float speed; } Chart;
typedef struct { uint16_t song,first,count,stage,character[6],tempo_first,tempo_count,event_first,event_count; uint8_t variant; } Track;
typedef struct { const char *name; uint16_t first,count; } Song;
typedef struct { const char *name; uint16_t song[4]; uint8_t count; } Level;
extern const Sprite sprites[SPRITE_COUNT];
extern const uint16_t palettes[PALETTE_COUNT][16],stage_sprites[STAGE_COUNT],ui_notes[4][4];
extern const uint8_t art_pixels[ART_BYTES],note_bytes[NOTE_BYTES];
extern const Animation animations[ANIMATION_COUNT];
extern const uint16_t beautiful_animation[ANIMATION_COUNT];
extern const Character characters[CHARACTER_COUNT];
extern const uint16_t kind_anim[CHARACTER_COUNT][KIND_COUNT][4];
extern const Tempo tempos[TEMPO_COUNT];
extern const Event events[EVENT_COUNT];
extern const Chart charts[CHART_COUNT];
extern const Track tracks[TRACK_COUNT];
extern const Song songs[SONG_COUNT];
extern const Level levels[LEVEL_COUNT];
extern const char *const difficulties[5],*const variants[4];
#endif
