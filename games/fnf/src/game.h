#ifndef FNF_GAME_H
#define FNF_GAME_H
#include <stdbool.h>
#include "data.h"
#define NOTE_CAP 64
#define WINDOW (160*16)
enum { TITLE,STORY,FREEPLAY,OPTIONS,CREDITS,PLAY,PAUSE,RESULT,QUIT,SAVE_ERROR };
enum { WAITING,HOLDING,FINISHED,MISSED };
enum { SICK,GOOD,BAD,SHIT,MISS };
typedef struct { int32_t score; uint16_t completion,combo; } Record;
typedef struct { Record record[CHART_COUNT]; uint16_t weeks; uint8_t downscroll,ghost,beat; } Progress;
typedef struct { int32_t time,end; uint8_t direction,kind,state; } Note;
typedef struct { uint16_t character,animation,icon[2]; int32_t since,until; uint8_t visible,side; } Actor;
typedef struct {
    Progress p;
    Note notes[NOTE_CAP];
    Actor actors[6];
    uint32_t read,end,decoded; int32_t stream_time,time,last_hit,judgement_at;
    int64_t score_milli;
    float bpm,beat,health,speed,speed_from,speed_to,zoom,dark,bop;
    int32_t speed_start,speed_end,flash_until;
    uint16_t chart,event,tempo,combo,max_combo,judged[5],peak;
    uint16_t count,focus; uint8_t held,judgement,won,lost,fault,dirty,beautiful,cover,pulse;
} Game;
void game_init(Game*);
bool game_start(Game*,unsigned);
void game_advance(Game*,int32_t,uint8_t);
void game_set_held(Game*,uint8_t);
int game_judge(int32_t);
int game_points(int32_t);
unsigned game_completion(const Game*);
const char* game_rank(const Game*);
unsigned game_chart(unsigned,unsigned,unsigned);
uint8_t game_lanes(uint64_t);
uint32_t game_varint(uint32_t*,uint32_t,bool*);
#endif
