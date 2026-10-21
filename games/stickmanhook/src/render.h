#ifndef SH_RENDER_H
#define SH_RENDER_H
#include "game.h"
enum {SH_TITLE,SH_PLAY,SH_PAUSE,SH_HELP,SH_SKIN_MENU,SH_LEVEL_MENU,SH_CONFIRM,SH_RESULT,SH_SAVE_ERROR};
typedef void (*sh_push)(int,int,int,int,const uint16_t *);
void sh_render(const sh_game *,const sh_stats *,unsigned view,unsigned selection,bool ongoing,bool save_failed,uint32_t ms,sh_push);
void sh_invalidate(void);
#endif
