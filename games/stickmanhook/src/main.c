#include <eadk.h>
#include "render.h"
#include "../../common/epsilon_app.h"
#include "../../common/epsilon_files.h"
#ifdef __ELF__
const char eadk_app_name[] __attribute__((section(".rodata.eadk_app_name")))="Stickman Hook";
const uint32_t eadk_api_level __attribute__((section(".rodata.eadk_api_level")))=0;
#endif
#define KEY(k) (UINT64_C(1)<<(k))
static sh_game game;
static sh_stats stats;
static uint8_t save_data[SH_SAVE_BYTES];
static bool have_save,save_failed;
static uint32_t last_hash;
uint32_t perf_frames,perf_max,perf_slow;
unsigned current_view;
static void push(int x,int y,int w,int h,const uint16_t *p){eadk_display_push_rect((eadk_rect_t){x,y,w,h},p);}
static bool save(void) {
  sh_encode(&stats,save_data);uint32_t h=save_data[36]|(uint32_t)save_data[37]<<8|(uint32_t)save_data[38]<<16|(uint32_t)save_data[39]<<24;
  if(have_save&&h==last_hash&&!save_failed)return true;
  save_failed=!ef_write("stickmanhook.sav",save_data,SH_SAVE_BYTES);
  if(!save_failed)have_save=true,last_hash=h;
  return !save_failed;
}
static void fresh(void){if(stats.runs<UINT32_MAX)++stats.runs;sh_new(&game,stats.current,stats.seed);}
static void completed(void){
  sh_finish(&stats,&game);save();
}
int main(void) {
  np_app_begin();uint32_t length=0;const uint8_t *data=ef_read("stickmanhook.sav",&length);
  stats=(sh_stats){.seed=0x484f4f4bu};
  if(!sh_decode(&stats,data,length))stats.seed^=(uint32_t)eadk_random();
  if(!stats.seed)stats.seed=1;
  sh_new(&game,stats.current,stats.seed);
  bool ongoing=false;unsigned view=SH_TITLE,selected=0,error_return=SH_TITLE,error_selection=0;
  uint64_t previous=0,keys=eadk_keyboard_scan(),stale=keys;
  uint32_t now=(uint32_t)eadk_timing_millis(),last=now,draw_at=now,accum=0;
  sh_invalidate();
  for(;;){
    now=(uint32_t)eadk_timing_millis();uint32_t elapsed=now-last;last=now;
    keys=eadk_keyboard_scan();stale&=keys;uint64_t active=keys&~stale,hit=active&~previous;previous=active;
    bool ok=(hit&(KEY(eadk_key_ok)|KEY(eadk_key_exe)))!=0,back=(hit&KEY(eadk_key_back))!=0;
    bool next=(hit&KEY(eadk_key_down))!=0,prev=(hit&KEY(eadk_key_up))!=0;
    bool right=(hit&KEY(eadk_key_right))!=0,left=(hit&KEY(eadk_key_left))!=0;
    if(view!=SH_SAVE_ERROR&&(keys&(KEY(eadk_key_home)|KEY(eadk_key_on_off)))){
      if(save())break;
      error_return=view==SH_PLAY?SH_PAUSE:view;error_selection=selected;view=SH_SAVE_ERROR;ok=back=next=prev=right=left=false;stale=keys;
    }
    if(view==SH_PLAY){
      if(back&&!game.completed)view=SH_PAUSE,selected=0,accum=0,save();
      else {
        if((hit&KEY(eadk_key_zero))&&!game.completed)sh_retry(&game);
        if(elapsed>100)elapsed=100;
        accum+=elapsed*60;
        bool held=(active&(KEY(eadk_key_ok)|KEY(eadk_key_exe)|KEY(eadk_key_shift)))!=0;
        while(accum>=1000){bool was_complete=game.completed;sh_step(&game,held);accum-=1000;
        if(!was_complete&&game.completed)completed();}
        if(game.ready)view=SH_RESULT,selected=0,ongoing=false;
      }
    }else{
      accum=0;
      if(view==SH_TITLE){
        if(next||right)selected=(selected+1)%3;
        if(prev||left)selected=(selected+2)%3;
        if(back){if(save())break;error_return=view;error_selection=selected;view=SH_SAVE_ERROR;}
        else if(hit&KEY(eadk_key_zero))view=SH_HELP;
        else if(ok){
          if(selected==0){if(!ongoing)fresh();ongoing=true;view=SH_PLAY;stale|=keys;}
          else if(selected==1)view=SH_LEVEL_MENU,selected=stats.current;
          else view=SH_SKIN_MENU,selected=stats.skin;
        }
      }else if(view==SH_HELP){if(ok||back)view=SH_TITLE,selected=0;}
      else if(view==SH_PAUSE){
        if(next||right)selected=(selected+1)%3;
        if(prev||left)selected=(selected+2)%3;
        if(back||(ok&&selected==0))view=SH_PLAY,stale|=keys;
        else if(ok&&selected==1)view=SH_CONFIRM,selected=0;
        else if(ok&&selected==2)view=SH_TITLE,selected=0;
      }else if(view==SH_CONFIRM){
        if(next||prev||right||left)selected=!selected;
        if(back||(ok&&selected==0))view=SH_PAUSE,selected=1;
        else if(ok)sh_retry(&game),view=SH_PLAY,selected=0,stale|=keys;
      }else if(view==SH_SKIN_MENU){
        if(right)selected=(selected+1)%SH_SKINS;
        if(left)selected=(selected+SH_SKINS-1)%SH_SKINS;
        if(next)selected=(selected+4)%SH_SKINS;
        if(prev)selected=(selected+SH_SKINS-4)%SH_SKINS;
        if(ok&&sh_skin_open(&stats,selected))stats.skin=(uint8_t)selected,save();
        if(back)view=SH_TITLE,selected=2;
      }else if(view==SH_LEVEL_MENU){
        unsigned limit=stats.unlocked<SH_LEVEL_COUNT?stats.unlocked:SH_MAX_LEVEL;
        if(right&&selected<limit)++selected;
        if(left&&selected)--selected;
        if(next)selected=selected+10>limit?limit:selected+10;
        if(prev)selected=selected<10?0:selected-10;
        if(back)view=SH_TITLE,selected=1;
        else if(ok){stats.current=selected;fresh();ongoing=true;view=SH_PLAY;selected=0;stale|=keys;save();}
      }else if(view==SH_RESULT){
        if(next||prev||right||left)selected=!selected;
        if(back||(ok&&selected==1))view=SH_TITLE,selected=0;
        else if(ok)fresh(),ongoing=true,view=SH_PLAY,selected=0,stale|=keys;
      }else if(view==SH_SAVE_ERROR){
        if(back)break;
        if(ok&&save())break;
        if(hit&KEY(eadk_key_one))view=error_return,selected=error_selection;
      }
    }
    current_view=view;
    if(now-draw_at>=33||hit||!perf_frames){
      uint32_t start=(uint32_t)eadk_timing_millis();sh_render(&game,&stats,view,selected,ongoing,save_failed,now,push);
      uint32_t spent=(uint32_t)eadk_timing_millis()-start;
      if(spent>perf_max)perf_max=spent;
      if(spent>33)++perf_slow;
      ++perf_frames;draw_at=now;
    }
    eadk_timing_msleep(2);
  }
  // Do not pass held Home/Back/Power to the launcher after returning.
  uint32_t released=(uint32_t)eadk_timing_millis();
  while(eadk_keyboard_scan()&&((uint32_t)eadk_timing_millis()-released)<2000)eadk_timing_msleep(10);
  return np_app_end();
}
