#ifndef RENDER_H
#define RENDER_H
#include "game.h"
typedef void (*Push)(int,int,int,int,const uint16_t*);
void render(const Game*,const Stats*,unsigned,unsigned,bool,bool,uint32_t,Push);
void render_invalidate(void);
#endif
