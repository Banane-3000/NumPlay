/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef TOWER_RENDER_H
#define TOWER_RENDER_H
#include "tower.h"
enum { TB_TITLE,TB_PLAY,TB_READY,TB_PAUSE,TB_STUDIO,TB_GUIDE,TB_RESULT,TB_CONFIRM,TB_SAVE_ERROR };
typedef void (*tb_push)(int x,int y,int w,int h,const uint16_t *pixels);
void tb_invalidate(void);
void tb_render(const tb_game *g,const tb_stats *s,unsigned view,unsigned selected,
               uint32_t ms,bool save_failed,tb_push push);
#endif
