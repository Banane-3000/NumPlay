/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef DOODLE_RENDER_H
#define DOODLE_RENDER_H
#include "doodle.h"
enum { DJ_TITLE,DJ_PLAY,DJ_READY,DJ_PAUSE,DJ_JOURNAL,DJ_GUIDE,DJ_CONFIRM,DJ_RESULT,DJ_SAVE_ERROR };
typedef void (*dj_push)(int x,int y,int w,int h,const uint16_t *pixels);
void dj_invalidate(void);
void dj_render(const dj_game *g,const dj_stats *s,unsigned page,unsigned selected,uint32_t ms,bool failed,dj_push push);
#endif
