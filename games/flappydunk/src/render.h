/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef FD_RENDER_H
#define FD_RENDER_H
#include "dunk.h"
enum { FD_TITLE,FD_FLIGHT,FD_READY,FD_PAUSE,FD_GUIDE,FD_LOCKER,FD_RESULT,FD_CONFIRM,FD_SAVE_ERROR };
typedef void (*fd_push)(int x,int y,int w,int h,const uint16_t *pixels);
void fd_invalidate(void);
void fd_render(const fd_game *g,const fd_stats *s,unsigned view,unsigned selected,
               uint32_t clock_ms,bool save_failed,fd_push push);
#endif
