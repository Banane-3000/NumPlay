/* SPDX-License-Identifier: GPL-3.0-only */
#include <eadk.h>
#include "tower.h"
#include "render.h"
#include "../../common/epsilon_app.h"
#include "../../common/epsilon_files.h"

#ifdef __ELF__
const char eadk_app_name[] __attribute__((section(".rodata.eadk_app_name"))) = "Tower Block";
const uint32_t eadk_api_level __attribute__((section(".rodata.eadk_api_level"))) = 0;
#endif
#define KEY(k) (UINT64_C(1) << (k))
#define SAVE_NAME "towerblock.sav"
static tb_game game;
static tb_stats stats;
static uint8_t save_data[TB_SAVE_BYTES];
static uint32_t last_checksum;
static bool have_save,save_failed;
uint32_t perf_frames,perf_max,perf_slow;

static void push(int x,int y,int w,int h,const uint16_t *p) {
  eadk_display_push_rect((eadk_rect_t){x,y,w,h},p);
}
static bool save(void) {
  tb_encode(&game,&stats,save_data);
  unsigned n=TB_SAVE_BYTES-4;
  uint32_t hash=save_data[n]|(uint32_t)save_data[n+1]<<8|
                (uint32_t)save_data[n+2]<<16|(uint32_t)save_data[n+3]<<24;
  if(have_save&&hash==last_checksum&&!save_failed)return true;
  save_failed=!ef_write(SAVE_NAME,save_data,TB_SAVE_BYTES);
  if(!save_failed)last_checksum=hash,have_save=true;
  return !save_failed;
}
int main(void) {
  np_app_begin();
  uint32_t len=0;const uint8_t *p=ef_read(SAVE_NAME,&len);
  if(!tb_decode(&game,&stats,p,len))stats.guide=stats.motion=1;
  unsigned view=TB_TITLE,selected=0,help_return=TB_TITLE,help_selection=2;
  unsigned error_return=TB_TITLE,error_selection=0;
  uint64_t previous=eadk_keyboard_scan(),stale=previous;
  uint32_t now=(uint32_t)eadk_timing_millis(),tick_at=now,draw_at=now,save_at=now;
  tb_invalidate();
  for(;;) {
    now=(uint32_t)eadk_timing_millis();
    uint64_t keys=eadk_keyboard_scan();stale&=keys;
    uint64_t active=keys&~stale,hit=active&~previous;previous=active;
    bool ok=(hit&(KEY(eadk_key_ok)|KEY(eadk_key_exe)))!=0;
    bool back=(hit&KEY(eadk_key_back))!=0;
    bool next=(hit&(KEY(eadk_key_right)|KEY(eadk_key_down)))!=0;
    bool prev=(hit&(KEY(eadk_key_left)|KEY(eadk_key_up)))!=0;
    if(view!=TB_SAVE_ERROR&&(keys&(KEY(eadk_key_home)|KEY(eadk_key_on_off)))) {
      if(save())break;
      error_return=view==TB_PLAY?TB_READY:view;error_selection=selected;
      view=TB_SAVE_ERROR;stale=keys;hit=0;ok=back=next=prev=false;
    }
    if(view==TB_PLAY) {
      if(back)view=TB_PAUSE,selected=0,save();
      else {
        if(ok)tb_place(&game,&stats);
        if(now-tick_at>4*TB_TICK_MS)tick_at=now-4*TB_TICK_MS;
        while(now-tick_at>=TB_TICK_MS&&game.phase!=TB_OVER) {
          tb_step(&game,&stats);tick_at+=TB_TICK_MS;
        }
        if(game.phase==TB_OVER)view=TB_RESULT,selected=0,save();
        if(now-save_at>=5000)save(),save_at=now;
      }
    } else {
      tick_at=now;
      if(view==TB_TITLE) {
        if(next)selected=(selected+1)%3;
        if(prev)selected=(selected+2)%3;
        if(back) {
          if(save())break;
          error_return=TB_TITLE;error_selection=selected;view=TB_SAVE_ERROR;
        } else if(ok) {
          if(selected==0) {
            if(!game.present||game.phase==TB_OVER)tb_new(&game);
            view=TB_READY;
          } else if(selected==1)view=TB_STUDIO,selected=0;
          else help_return=TB_TITLE,help_selection=2,view=TB_GUIDE;

        }
      } else if(view==TB_READY) {
        if(back)view=TB_TITLE,selected=0;
        else if(ok)tb_start(&game,&stats),view=TB_PLAY,tick_at=now;
      } else if(view==TB_PAUSE) {
        if(next)selected=(selected+1)%4;
        if(prev)selected=(selected+3)%4;
        if(back||(ok&&selected==0))view=TB_READY;
        else if(ok&&selected==1) {
          view=TB_CONFIRM;selected=0;
        } else if(ok&&selected==2)help_return=TB_PAUSE,help_selection=2,view=TB_GUIDE;
        else if(ok&&selected==3)view=TB_TITLE,selected=0;
      } else if(view==TB_CONFIRM) {
        if(next||prev)selected=!selected;
        if(back||(ok&&selected==0))view=TB_PAUSE,selected=1;
        else if(ok)tb_new(&game),view=TB_READY,selected=0;
      } else if(view==TB_RESULT) {
        if(next||prev)selected=!selected;
        if(back||(ok&&selected))view=TB_TITLE,selected=0;
        else if(ok)tb_new(&game),view=TB_READY,selected=0;
      } else if(view==TB_STUDIO) {
        if(hit&KEY(eadk_key_down))selected=(selected+1)%3;
        if(hit&KEY(eadk_key_up))selected=(selected+2)%3;
        if(back)view=TB_TITLE,selected=1;
        else if(ok||(hit&(KEY(eadk_key_left)|KEY(eadk_key_right)))) {
          if(selected==0)stats.palette=(uint8_t)((stats.palette+((hit&KEY(eadk_key_left))?3:1))%TB_PALETTES);
          else if(selected==1)stats.guide=!stats.guide;
          else stats.motion=!stats.motion;
          save();
        }
      } else if(view==TB_GUIDE) {
        if(ok||back)view=help_return,selected=help_selection;
      } else if(view==TB_SAVE_ERROR) {
        if(back)break;
        if(ok&&save())break;
        if(hit&KEY(eadk_key_one))view=error_return,selected=error_selection;
      }
    }
    if(now-draw_at>=32||hit||perf_frames==0) {
      uint32_t begin=(uint32_t)eadk_timing_millis();
      tb_render(&game,&stats,view,selected,now,save_failed,push);
      uint32_t spent=(uint32_t)eadk_timing_millis()-begin;
      if(spent>perf_max)perf_max=spent;
      if(spent>32)++perf_slow;
      ++perf_frames;draw_at=now;
    }
    eadk_timing_msleep(4);
  }
  return np_app_end();
}
