/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef JETPACK_RENDER_H
#define JETPACK_RENDER_H
#include "jetpack.h"
enum { JJ_TITLE,JJ_PLAY,JJ_READY,JJ_PAUSE,JJ_SHOP,JJ_GUIDE,JJ_CONFIRM,JJ_RESULT,JJ_SAVE_ERROR,JJ_BUY };
typedef void (*jj_push)(int x,int y,int w,int h,const uint16_t *pixels);
void jj_invalidate(void);
void jj_menu_time(uint32_t ms);
void jj_render(const jj_game *g,const jj_stats *s,unsigned page,unsigned selected,bool failed,jj_push push);
#endif
