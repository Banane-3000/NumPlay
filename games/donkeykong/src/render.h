/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef DK_RENDER_H
#define DK_RENDER_H
#include "game.h"
enum { DK_TITLE, DK_PLAY, DK_PAUSE, DK_HELP, DK_CONFIRM, DK_RESULT, DK_SAVE_ERROR };
typedef void (*dk_push)(int,int,int,int,const uint16_t *);
void dk_render(const dk_game *,const dk_stats *,unsigned view,unsigned selection,
               bool continuing,bool save_failed,uint32_t ms,dk_push);
void dk_invalidate(void);
#endif
