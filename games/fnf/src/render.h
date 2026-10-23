#ifndef FNF_RENDER_H
#define FNF_RENDER_H
#include "game.h"
typedef struct { unsigned view,selected,song,week,variant,difficulty,story_song; bool story; } Menu;
typedef void (*DisplayPush)(int,int,int,int,const uint16_t*);
void render_invalidate(void);
bool render_prepare(unsigned);
void render(const Game*,const Menu*,DisplayPush,void (*poll)(void));
#endif
