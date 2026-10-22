#ifndef MM_RENDER_H
#define MM_RENDER_H
#include "game.h"
typedef void (*Push)(int,int,int,int,const uint16_t*);
enum {TITLE,PLAY,PAUSE,UPGRADES,HATS,MARKETS,BOOSTS,HELP,RESET_CONFIRM,SAVE_ERROR};
void render(const Game*,unsigned,unsigned,unsigned,bool,Push);
void render_invalidate(void);
#endif
