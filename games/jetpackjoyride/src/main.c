/* SPDX-License-Identifier: GPL-3.0-only */
#include <eadk.h>
#include "jetpack.h"
#include "render.h"
#include "../../common/epsilon_app.h"
#include "../../common/epsilon_files.h"
#ifdef __ELF__
const char eadk_app_name[] __attribute__((section(".rodata.eadk_app_name")))="Jetpack Joyride";
const uint32_t eadk_api_level __attribute__((section(".rodata.eadk_api_level")))=0;
#endif
#define KEY(k) (UINT64_C(1)<<(k))
static jj_game game;
static jj_stats stats;
static uint8_t save_data[JJ_SAVE_BYTES];
static bool have_save,save_failed;
static uint32_t last_checksum;
uint32_t perf_frames,perf_max,perf_slow;
static void push(int x,int y,int w,int h,const uint16_t *p) {
  eadk_display_push_rect((eadk_rect_t){x,y,w,h},p);
}
static bool save(void) {
  jj_encode(&game,&stats,save_data);
  unsigned n=JJ_SAVE_BYTES-4;
  uint32_t hash=save_data[n]|(uint32_t)save_data[n+1]<<8|(uint32_t)save_data[n+2]<<16|(uint32_t)save_data[n+3]<<24;
  if(have_save&&hash==last_checksum&&!save_failed)return true;
  save_failed=!ef_write("jetpackjoyride.sav",save_data,JJ_SAVE_BYTES);
  if(!save_failed)have_save=true,last_checksum=hash;
  return !save_failed;
}
static void fresh(uint32_t seed) {jj_new(&game,seed);}
int main(void) {
  np_app_begin();
  uint32_t length=0;const uint8_t *data=ef_read("jetpackjoyride.sav",&length);
  jj_decode(&game,&stats,data,length);
  unsigned view=JJ_TITLE,selected=0,help_return=JJ_TITLE,help_selection=2;
  unsigned error_return=JJ_TITLE,error_selection=0;
  uint64_t previous=eadk_keyboard_scan(),stale=previous;
  uint32_t now=(uint32_t)eadk_timing_millis(),tick_at=now,draw_at=now,save_at=now;
  jj_invalidate();
  for(;;) {
    now=(uint32_t)eadk_timing_millis();uint64_t keys=eadk_keyboard_scan();stale&=keys;
    uint64_t active=keys&~stale,hit=active&~previous;previous=active;
    bool ok=(hit&(KEY(eadk_key_ok)|KEY(eadk_key_exe)))!=0;
    bool back=(hit&KEY(eadk_key_back))!=0;
    bool next=(hit&(KEY(eadk_key_right)|KEY(eadk_key_down)))!=0;
    bool prev=(hit&(KEY(eadk_key_left)|KEY(eadk_key_up)))!=0;
    if(view!=JJ_SAVE_ERROR&&(keys&(KEY(eadk_key_home)|KEY(eadk_key_on_off)))) {
      if(save())break;
      error_return=view==JJ_PLAY?JJ_READY:view;error_selection=selected;
      view=JJ_SAVE_ERROR;hit=0;ok=back=next=prev=false;stale=keys;
    }
    if(view==JJ_PLAY) {
      if(back)view=JJ_PAUSE,selected=0,save();
      else {
        bool thrust=(active&(KEY(eadk_key_ok)|KEY(eadk_key_exe)|KEY(eadk_key_up)))!=0;
        if(now-tick_at>4*JJ_TICK_MS)tick_at=now-4*JJ_TICK_MS;
        while(now-tick_at>=JJ_TICK_MS&&game.phase==JJ_RUNNING) {
          jj_step(&game,&stats,thrust);tick_at+=JJ_TICK_MS;
        }
        if(game.phase==JJ_OVER)view=JJ_RESULT,selected=0,save();
        if(now-save_at>=5000)save(),save_at=now;
      }
    } else {
      tick_at=now;
      if(view==JJ_TITLE) {
        if(next)selected=(selected+1)%3;
        if(prev)selected=(selected+2)%3;
        if(back) {
          if(save())break;
          error_return=JJ_TITLE;error_selection=selected;view=JJ_SAVE_ERROR;
        } else if(ok) {
          if(selected==0) {
            if(!game.present||game.phase==JJ_OVER)fresh(now^(stats.runs*0x9E3779B9u));
            view=JJ_READY;selected=0;
          } else if(selected==1)view=JJ_SHOP,selected=stats.suit;
          else help_return=JJ_TITLE,help_selection=2,view=JJ_GUIDE;
        }
      } else if(view==JJ_READY) {
        if(back)view=JJ_TITLE,selected=0;
        else if(ok)jj_start(&game,&stats),view=JJ_PLAY,tick_at=now;
      } else if(view==JJ_PAUSE) {
        if(next)selected=(selected+1)%4;
        if(prev)selected=(selected+3)%4;
        if(back||(ok&&selected==0))view=JJ_READY;
        else if(ok&&selected==1)view=JJ_CONFIRM,selected=0;
        else if(ok&&selected==2)help_return=JJ_PAUSE,help_selection=2,view=JJ_GUIDE;
        else if(ok&&selected==3)view=JJ_TITLE,selected=0;
      } else if(view==JJ_CONFIRM) {
        if(next||prev)selected=!selected;
        if(back||(ok&&selected==0))view=JJ_PAUSE,selected=1;
        else if(ok)fresh(now^(stats.runs*0x9E3779B9u)),view=JJ_READY,selected=0;
      } else if(view==JJ_RESULT) {
        if(next||prev)selected=!selected;
        if(back||(ok&&selected))view=JJ_TITLE,selected=0;
        else if(ok)fresh(now^(stats.runs*0x9E3779B9u)),view=JJ_READY,selected=0;
      } else if(view==JJ_SHOP) {
        if(next)selected=(selected+1)%4;
        if(prev)selected=(selected+3)%4;
        if(back)view=JJ_TITLE,selected=1;
        else if(ok) {
          if(!selected||(stats.owned&(1u<<selected)))jj_equip(&stats,selected),save();
          else if(stats.wallet>=jj_price(selected))view=JJ_BUY,selected<<=1;
        }
      } else if(view==JJ_BUY) {
        if(next||prev)selected^=1;
        if(back||(ok&&!(selected&1)))view=JJ_SHOP,selected>>=1;
        else if(ok) {
          selected>>=1;jj_equip(&stats,selected);save();view=JJ_SHOP;
        }
      } else if(view==JJ_GUIDE) {
        if(back||ok)view=help_return,selected=help_selection;
      } else if(view==JJ_SAVE_ERROR) {
        if(back)break;
        if(ok&&save())break;
        if(hit&KEY(eadk_key_one))view=error_return,selected=error_selection;
      }
    }
    if(now-draw_at>=32||hit||!perf_frames) {
      uint32_t start=(uint32_t)eadk_timing_millis();
      jj_menu_time(now);
      jj_render(&game,&stats,view,selected,save_failed,push);
      uint32_t spent=(uint32_t)eadk_timing_millis()-start;
      if(spent>perf_max)perf_max=spent;
      if(spent>32)++perf_slow;
      ++perf_frames;draw_at=now;
    }
    eadk_timing_msleep(4);
  }
  return np_app_end();
}
