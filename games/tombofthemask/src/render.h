#ifndef TOTM_RENDER_H
#define TOTM_RENDER_H
#include "game.h"
typedef void (*Push)(int,int,int,int,const uint16_t*);
void render(Game*,unsigned,unsigned,bool,Push);
void render_invalidate(void);
#endif
