/* SPDX-License-Identifier: GPL-3.0-only */
#include <eadk.h>
#include "dunk.h"
#include "render.h"
#include "../../common/epsilon_app.h"
#include "../../common/epsilon_files.h"

#ifdef __ELF__
const char eadk_app_name[] __attribute__((section(".rodata.eadk_app_name"))) = "Flappy Dunk";
const uint32_t eadk_api_level __attribute__((section(".rodata.eadk_api_level"))) = 0;
#endif
#define KEY(k) (UINT64_C(1) << (k))
#define SAVE_NAME "flappydunk.sav"
static fd_game game;
static fd_stats stats;
static uint8_t save_data[FD_SAVE_BYTES];
static uint32_t last_checksum;
static bool save_failed;
uint32_t perf_frames, perf_max, perf_slow;

static void push(int x,int y,int w,int h,const uint16_t *p) {
  eadk_display_push_rect((eadk_rect_t){x,y,w,h},p);
}
static bool save(void) {
  fd_encode(&game,&stats,save_data);
  unsigned n=FD_SAVE_BYTES-4;
  uint32_t checksum=save_data[n]|(uint32_t)save_data[n+1]<<8|
                    (uint32_t)save_data[n+2]<<16|(uint32_t)save_data[n+3]<<24;
  if(checksum==last_checksum&&!save_failed)return true;
  save_failed=!ef_write(SAVE_NAME,save_data,FD_SAVE_BYTES);
  if(!save_failed)last_checksum=checksum;
  return !save_failed;
}
static unsigned grid(unsigned n,uint64_t hit,unsigned count) {
  if(hit&KEY(eadk_key_left))n=n%3?n-1:n+2;
  if(hit&KEY(eadk_key_right))n=n%3<2?n+1:n-2;
  if(hit&KEY(eadk_key_up))n=(n+count-3)%count;
  if(hit&KEY(eadk_key_down))n=(n+3)%count;
  return n;
}
int main(void) {
  np_app_begin();
  uint32_t len=0;
  const uint8_t *p=ef_read(SAVE_NAME,&len);
  fd_decode(&game,&stats,p,len);
  unsigned view=FD_TITLE,selected=0,help_return=FD_TITLE;
  unsigned confirm_return=FD_PAUSE,confirm_selection=0;
  unsigned error_return=FD_TITLE;
  uint64_t previous=eadk_keyboard_scan(),stale=previous;
  uint32_t now=(uint32_t)eadk_timing_millis(),tick_at=now,draw_at=now,save_at=now;
  fd_invalidate();
  for(;;) {
    now=(uint32_t)eadk_timing_millis();
    uint64_t keys=eadk_keyboard_scan();stale&=keys;
    uint64_t active=keys&~stale,hit=active&~previous;previous=active;
    bool ok=(hit&(KEY(eadk_key_ok)|KEY(eadk_key_exe)))!=0;
    bool back=(hit&KEY(eadk_key_back))!=0;
    bool flap=ok||(hit&KEY(eadk_key_up));
    if(view!=FD_SAVE_ERROR&&(keys&(KEY(eadk_key_home)|KEY(eadk_key_on_off)))) {
      if(save())break;
      error_return=view==FD_FLIGHT?FD_READY:view;
      view=FD_SAVE_ERROR;selected=0;stale=keys;hit=0;ok=back=flap=false;
    }
    if(view==FD_FLIGHT) {
      if(back) {view=FD_PAUSE;selected=0;save();}
      else {
        if(flap)fd_flap(&game,&stats);
        if(now-tick_at>4*FD_TICK_MS)tick_at=now-4*FD_TICK_MS;
        while(now-tick_at>=FD_TICK_MS&&game.phase==FD_RUNNING) {
          fd_step(&game,&stats);tick_at+=FD_TICK_MS;
        }
        if(game.phase>=FD_OVER) {view=FD_RESULT;selected=0;save();}
        if(now-save_at>=5000) {save();save_at=now;}
      }
    } else {
      tick_at=now;
      if(view==FD_TITLE) {
        if(hit&KEY(eadk_key_down))selected=(selected+1)%3;
        if(hit&KEY(eadk_key_up))selected=(selected+2)%3;
        if(back) {
          if(save())break;
          view=FD_SAVE_ERROR;error_return=FD_TITLE;selected=0;
        } else if(ok) {
          if(selected==0) {
            if(!game.present||game.phase>=FD_OVER)fd_new(&game,eadk_random()^now);
            view=FD_READY;
          } else if(selected==1)view=FD_LOCKER;
          else {view=FD_GUIDE;help_return=FD_TITLE;}
          selected=view==FD_LOCKER?stats.skin:0;
        }
      } else if(view==FD_READY) {
        if(back)view=FD_TITLE,selected=0;
        else if(flap) {fd_flap(&game,&stats);view=FD_FLIGHT;tick_at=now;}
      } else if(view==FD_PAUSE) {
        if(hit&KEY(eadk_key_down))selected=(selected+1)%4;
        if(hit&KEY(eadk_key_up))selected=(selected+3)%4;
        if(back||(ok&&selected==0))view=FD_READY;
        else if(ok&&selected==1) {
          confirm_return=FD_PAUSE;confirm_selection=1;
          view=FD_CONFIRM;selected=0;
        } else if(ok&&selected==2)help_return=FD_PAUSE,view=FD_GUIDE;
        else if(ok&&selected==3)view=FD_TITLE,selected=0;
      } else if(view==FD_LOCKER) {
        selected=grid(selected,hit,FD_SKINS);
        if(back)view=FD_TITLE,selected=1;
        else if(ok&&(fd_unlocked(&stats)&(1u<<selected))) {stats.skin=(uint8_t)selected;save();}
      } else if(view==FD_GUIDE) {
        if(back||ok)view=help_return,selected=0;
      } else if(view==FD_CONFIRM) {
        if(hit&(KEY(eadk_key_up)|KEY(eadk_key_down)))selected=!selected;
        if(back||(ok&&selected==0))view=confirm_return,selected=confirm_selection;
        else if(ok) {fd_new(&game,eadk_random()^now);view=FD_READY;selected=0;}
      } else if(view==FD_RESULT) {
        if(hit&(KEY(eadk_key_up)|KEY(eadk_key_down)))selected=!selected;
        if(back||(ok&&selected==1))view=FD_TITLE,selected=0;
        else if(ok) {fd_new(&game,eadk_random()^now);view=FD_READY;selected=0;}
      } else if(view==FD_SAVE_ERROR) {
        if(back)break;
        if(ok&&save())break;
        if(hit&KEY(eadk_key_one))view=error_return,selected=0;
      }
    }
    if(now-draw_at>=32||hit||perf_frames==0) {
      uint32_t begin=(uint32_t)eadk_timing_millis();
      fd_render(&game,&stats,view,selected,now,save_failed,push);
      uint32_t spent=(uint32_t)eadk_timing_millis()-begin;
      if(spent>perf_max)perf_max=spent;
      if(spent>32)++perf_slow;
      ++perf_frames;draw_at=now;
    }
    eadk_timing_msleep(4);
  }
  return np_app_end();
}
