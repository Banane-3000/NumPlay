/* SPDX-License-Identifier: GPL-3.0-only */
#include <eadk.h>
#include "render.h"
#include "../../common/epsilon_app.h"
#include "../../common/epsilon_files.h"
#ifdef __ELF__
const char eadk_app_name[] __attribute__((section(".rodata.eadk_app_name")))="Donkey Kong";
const uint32_t eadk_api_level __attribute__((section(".rodata.eadk_api_level")))=0;
#endif
#define KEY(k) (UINT64_C(1)<<(k))
static dk_game game;
static dk_stats stats;
static uint8_t save_data[DK_SAVE_BYTES];
static bool have_save,save_failed;
static uint32_t last_checksum;
uint32_t perf_frames,perf_max,perf_slow;
static void push(int x,int y,int w,int h,const uint16_t *p) {
  eadk_display_push_rect((eadk_rect_t){x,y,w,h},p);
}
static bool save(void) {
  dk_encode_stats(&stats,save_data);
  uint32_t hash=save_data[28]|(uint32_t)save_data[29]<<8|(uint32_t)save_data[30]<<16|(uint32_t)save_data[31]<<24;
  if(have_save&&hash==last_checksum&&!save_failed)return true;
  save_failed=!ef_write("donkeykong.sav",save_data,DK_SAVE_BYTES);
  if(!save_failed)have_save=true,last_checksum=hash;
  return !save_failed;
}
static void fresh(uint32_t seed) {
  if(stats.runs<UINT32_MAX)++stats.runs;
  dk_new(&game,seed^(stats.runs*0x9E3779B9u));
}
int main(void) {
  np_app_begin();
  uint32_t length=0;const uint8_t *data=ef_read("donkeykong.sav",&length);
  dk_decode_stats(&stats,data,length);dk_new(&game,1981);
  bool ongoing=false;
  unsigned view=DK_TITLE,selected=0,error_return=DK_TITLE,error_selection=0;
  uint64_t previous=eadk_keyboard_scan(),stale=previous;
  uint32_t now=(uint32_t)eadk_timing_millis(),last=now,draw_at=now,save_at=now,accum=0;
  dk_invalidate();
  for(;;) {
    now=(uint32_t)eadk_timing_millis();uint32_t elapsed=now-last;last=now;
    uint64_t keys=eadk_keyboard_scan();stale&=keys;
    uint64_t active=keys&~stale,hit=active&~previous;previous=active;
    bool ok=(hit&(KEY(eadk_key_ok)|KEY(eadk_key_exe)))!=0;
    bool back=(hit&KEY(eadk_key_back))!=0;
    bool next=(hit&(KEY(eadk_key_down)|KEY(eadk_key_right)))!=0;
    bool prev=(hit&(KEY(eadk_key_up)|KEY(eadk_key_left)))!=0;
    if(view!=DK_SAVE_ERROR&&(keys&(KEY(eadk_key_home)|KEY(eadk_key_on_off)))) {
      if(save())break;
      error_return=view==DK_PLAY?DK_PAUSE:view;error_selection=selected;
      view=DK_SAVE_ERROR;ok=back=next=prev=false;stale=keys;
    }
    if(view==DK_PLAY) {
      if(back)view=DK_PAUSE,selected=0,accum=0,save();
      else {
        unsigned input=0;
        if(active&(KEY(eadk_key_left)|KEY(eadk_key_four)))input|=DK_LEFT;
        if(active&(KEY(eadk_key_right)|KEY(eadk_key_six)))input|=DK_RIGHT;
        if(active&(KEY(eadk_key_up)|KEY(eadk_key_eight)))input|=DK_UP;
        if(active&(KEY(eadk_key_down)|KEY(eadk_key_two)))input|=DK_DOWN;
        if(active&(KEY(eadk_key_ok)|KEY(eadk_key_exe)|KEY(eadk_key_shift)))input|=DK_JUMP;
        if(elapsed>66)elapsed=66;
        accum+=elapsed*60;
        while(accum>=1000)dk_step(&game,&stats,(uint8_t)input),accum-=1000;
        if(game.phase==DK_OVER)view=DK_RESULT,selected=0,ongoing=false,save();
        if(now-save_at>=5000)save(),save_at=now;
      }
    } else {
      accum=0;
      if(view==DK_TITLE) {
        if(next)selected=(selected+1)%3;
        if(prev)selected=(selected+2)%3;
        if(back||(ok&&selected==2)) {
          if(save())break;
          error_return=DK_TITLE;error_selection=selected;view=DK_SAVE_ERROR;
        } else if(ok&&selected==0) {
          if(!ongoing)fresh(now),ongoing=true;
          game.previous_input=DK_JUMP;stale|=keys;view=DK_PLAY;
        } else if(ok&&selected==1)view=DK_HELP;
      } else if(view==DK_HELP) {
        if(ok||back)view=DK_TITLE,selected=1;
      } else if(view==DK_PAUSE) {
        if(next)selected=(selected+1)%3;
        if(prev)selected=(selected+2)%3;
        if(back||(ok&&selected==0))view=DK_PLAY,stale|=keys;
        else if(ok&&selected==1)view=DK_CONFIRM,selected=0;
        else if(ok&&selected==2)view=DK_TITLE,selected=0;
      } else if(view==DK_CONFIRM) {
        if(next||prev)selected=!selected;
        if(back||(ok&&selected==0))view=DK_PAUSE,selected=1;
        else if(ok)fresh(now),view=DK_PLAY,selected=0,stale|=keys;
      } else if(view==DK_RESULT) {
        if(next||prev)selected=!selected;
        if(back||(ok&&selected))view=DK_TITLE,selected=0;
        else if(ok)fresh(now),view=DK_PLAY,selected=0,ongoing=true,stale|=keys;
      } else if(view==DK_SAVE_ERROR) {
        if(back)break;
        if(ok&&save())break;
        if(hit&KEY(eadk_key_one))view=error_return,selected=error_selection;
      }
    }
    if(now-draw_at>=16||hit||!perf_frames) {
      uint32_t start=(uint32_t)eadk_timing_millis();
      dk_render(&game,&stats,view,selected,ongoing,save_failed,now,push);
      uint32_t spent=(uint32_t)eadk_timing_millis()-start;
      if(spent>perf_max)perf_max=spent;
      if(spent>17)++perf_slow;
      ++perf_frames;draw_at=now;
    }
    eadk_timing_msleep(2);
  }
  return np_app_end();
}
