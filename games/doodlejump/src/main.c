/* SPDX-License-Identifier: GPL-3.0-only */
#include <eadk.h>
#include "doodle.h"
#include "render.h"
#include "../../common/epsilon_app.h"
#include "../../common/epsilon_files.h"
#ifdef __ELF__
const char eadk_app_name[] __attribute__((section(".rodata.eadk_app_name")))="Doodle Jump";
const uint32_t eadk_api_level __attribute__((section(".rodata.eadk_api_level")))=0;
#endif
#define KEY(k) (UINT64_C(1)<<(k))
static dj_game game;
static dj_stats stats;
static uint8_t save_data[DJ_SAVE_BYTES];
static bool have_save,save_failed;
static uint32_t last_checksum;
uint32_t perf_frames,perf_max,perf_slow;
static void push(int x,int y,int w,int h,const uint16_t *p) {
  eadk_display_push_rect((eadk_rect_t){x,y,w,h},p);
}
static bool save(void) {
  dj_encode(&game,&stats,save_data);
  unsigned n=DJ_SAVE_BYTES-4;
  uint32_t hash=save_data[n]|(uint32_t)save_data[n+1]<<8|(uint32_t)save_data[n+2]<<16|(uint32_t)save_data[n+3]<<24;
  if(have_save&&hash==last_checksum&&!save_failed)return true;
  save_failed=!ef_write("doodlejump.sav",save_data,DJ_SAVE_BYTES);
  if(!save_failed)have_save=true,last_checksum=hash;
  return !save_failed;
}
static void fresh(uint32_t seed) {dj_new(&game,seed);}
int main(void) {
  np_app_begin();stats.motion=1;
  uint32_t length=0;const uint8_t *data=ef_read("doodlejump.sav",&length);
  dj_decode(&game,&stats,data,length);
  unsigned view=DJ_TITLE,selected=0,help_return=DJ_TITLE,help_selection=2;
  unsigned error_return=DJ_TITLE,error_selection=0;
  uint64_t previous=eadk_keyboard_scan(),stale=previous;
  uint32_t now=(uint32_t)eadk_timing_millis(),tick_at=now,draw_at=now,save_at=now;
  dj_invalidate();
  for(;;) {
    now=(uint32_t)eadk_timing_millis();uint64_t keys=eadk_keyboard_scan();stale&=keys;
    uint64_t active=keys&~stale,hit=active&~previous;previous=active;
    bool ok=(hit&(KEY(eadk_key_ok)|KEY(eadk_key_exe)))!=0;
    bool back=(hit&KEY(eadk_key_back))!=0;
    bool next=(hit&(KEY(eadk_key_right)|KEY(eadk_key_down)))!=0;
    bool prev=(hit&(KEY(eadk_key_left)|KEY(eadk_key_up)))!=0;
    if(view!=DJ_SAVE_ERROR&&(keys&(KEY(eadk_key_home)|KEY(eadk_key_on_off)))) {
      if(save())break;
      error_return=view==DJ_PLAY?DJ_READY:view;error_selection=selected;
      view=DJ_SAVE_ERROR;hit=0;ok=back=next=prev=false;stale=keys;
    }
    if(view==DJ_PLAY) {
      if(back)view=DJ_PAUSE,selected=0,save();
      else {
        if(hit&(KEY(eadk_key_ok)|KEY(eadk_key_exe)|KEY(eadk_key_up)))dj_fire(&game);
        int steer=(active&(KEY(eadk_key_right)|KEY(eadk_key_six))?1:0)-
                  (active&(KEY(eadk_key_left)|KEY(eadk_key_four))?1:0);
        if(now-tick_at>4*DJ_TICK_MS)tick_at=now-4*DJ_TICK_MS;
        while(now-tick_at>=DJ_TICK_MS&&game.phase==DJ_RUNNING) {
          dj_step(&game,&stats,steer);tick_at+=DJ_TICK_MS;
        }
        if(game.phase==DJ_OVER)view=DJ_RESULT,selected=0,save();
        if(now-save_at>=5000)save(),save_at=now;
      }
    } else {
      tick_at=now;
      if(view==DJ_TITLE) {
        if(next)selected=(selected+1)%3;
        if(prev)selected=(selected+2)%3;
        if(back) {
          if(save())break;
          error_return=DJ_TITLE;error_selection=selected;view=DJ_SAVE_ERROR;
        } else if(ok) {
          if(selected==0) {
            if(!game.present||game.phase==DJ_OVER)fresh(now^(stats.runs*0x9E3779B9u));
            view=DJ_READY;selected=0;
          } else if(selected==1)view=DJ_JOURNAL,selected=0;
          else help_return=DJ_TITLE,help_selection=2,view=DJ_GUIDE;
        }
      } else if(view==DJ_READY) {
        if(back)view=DJ_TITLE,selected=0;
        else if(ok)dj_start(&game,&stats),view=DJ_PLAY,tick_at=now;
      } else if(view==DJ_PAUSE) {
        if(next)selected=(selected+1)%4;
        if(prev)selected=(selected+3)%4;
        if(back||(ok&&selected==0))view=DJ_READY;
        else if(ok&&selected==1)view=DJ_CONFIRM,selected=0;
        else if(ok&&selected==2)help_return=DJ_PAUSE,help_selection=2,view=DJ_GUIDE;
        else if(ok&&selected==3)view=DJ_TITLE,selected=0;
      } else if(view==DJ_CONFIRM) {
        if(next||prev)selected=!selected;
        if(back||(ok&&selected==0))view=DJ_PAUSE,selected=1;
        else if(ok)fresh(now^(stats.runs*0x9E3779B9u)),view=DJ_READY,selected=0;
      } else if(view==DJ_RESULT) {
        if(next||prev)selected=!selected;
        if(back||(ok&&selected))view=DJ_TITLE,selected=0;
        else if(ok)fresh(now^(stats.runs*0x9E3779B9u)),view=DJ_READY,selected=0;
      } else if(view==DJ_JOURNAL) {
        if(back)view=DJ_TITLE,selected=1;
        else if(ok||next||prev)stats.motion=!stats.motion,save();
      } else if(view==DJ_GUIDE) {
        if(back||ok)view=help_return,selected=help_selection;
      } else if(view==DJ_SAVE_ERROR) {
        if(back)break;
        if(ok&&save())break;
        if(hit&KEY(eadk_key_one))view=error_return,selected=error_selection;
      }
    }
    if(now-draw_at>=32||hit||!perf_frames) {
      uint32_t start=(uint32_t)eadk_timing_millis();
      dj_render(&game,&stats,view,selected,now,save_failed,push);
      uint32_t spent=(uint32_t)eadk_timing_millis()-start;
      if(spent>perf_max)perf_max=spent;
      if(spent>32)++perf_slow;
      ++perf_frames;draw_at=now;
    }
    eadk_timing_msleep(4);
  }
  return np_app_end();
}
