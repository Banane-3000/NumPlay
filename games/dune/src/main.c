#include <eadk.h>
#include "render.h"
#include "assets.h"
#include "../../common/epsilon_app.h"
#include "../../common/epsilon_files.h"
#ifdef __ELF__
const char eadk_app_name[] __attribute__((section(".rodata.eadk_app_name")))="Dune";
const uint32_t eadk_api_level __attribute__((section(".rodata.eadk_api_level")))=0;
#endif
#define KEY(k) (UINT64_C(1)<<(k))
static Game game;
static Stats stats;
static uint8_t save_data[SAVE_BYTES];
static bool saved,save_failed;
static uint32_t last_hash;
uint32_t perf_frames,perf_max,perf_slow,current_view;
static void push(int x,int y,int w,int h,const uint16_t*p){eadk_display_push_rect((eadk_rect_t){x,y,w,h},p);}
static void bank(void){game_finish(&stats,&game);game.earned=0;}
static bool save(void){bank();save_encode(&stats,save_data);uint32_t h=0;for(unsigned i=SAVE_BYTES-4;i<SAVE_BYTES;i++)h=(h<<8)|save_data[i];if(saved&&h==last_hash&&!save_failed)return true;save_failed=!ef_write("dune.sav",save_data,SAVE_BYTES);if(!save_failed)saved=true,last_hash=h;return !save_failed;}
static void fresh(void){if(stats.runs<UINT32_MAX)++stats.runs;uint32_t seed=stats.seed^(stats.runs*2654435761u);game_new(&game,seed);}
int main(void){
 np_app_begin();stats=(Stats){.unlocked=1,.scenes=1,.seed=0x44554e45};uint32_t n=0;const uint8_t*p=ef_read("dune.sav",&n);if(!save_decode(&stats,p,n))stats.seed^=(uint32_t)eadk_random();if(!stats.seed)stats.seed=1;game_new(&game,stats.seed);
 unsigned view=TITLE,selected=0,error_return=TITLE,error_selection=0;bool ongoing=false;
 uint64_t previous=0,keys=eadk_keyboard_scan(),stale=keys;uint32_t now=(uint32_t)eadk_timing_millis(),last=now,draw_at=now,accum=0;
 render_invalidate();
 for(;;){
  now=(uint32_t)eadk_timing_millis();uint32_t elapsed=now-last;last=now;keys=eadk_keyboard_scan();stale&=keys;uint64_t active=keys&~stale,hit=active&~previous;previous=active;
  bool ok=(hit&(KEY(eadk_key_ok)|KEY(eadk_key_exe)))!=0,back=(hit&KEY(eadk_key_back))!=0,right=(hit&KEY(eadk_key_right))!=0,left=(hit&KEY(eadk_key_left))!=0,down=(hit&KEY(eadk_key_down))!=0,up=(hit&KEY(eadk_key_up))!=0;
  if(view!=SAVE_ERROR&&(keys&(KEY(eadk_key_home)|KEY(eadk_key_on_off)))){if(save())break;error_return=view==PLAY?PAUSE:view;error_selection=selected;view=SAVE_ERROR;ok=back=right=left=up=down=false;stale=keys;}
  if(view==PLAY){
   if(back){view=PAUSE;selected=0;save();}
   else{if(elapsed>100)elapsed=100;accum+=elapsed*60;bool held=(active&(KEY(eadk_key_ok)|KEY(eadk_key_exe)|KEY(eadk_key_shift)))!=0;
    while(accum>=1000){unsigned phase=game.phase;game_step(&game,held);accum-=1000;if(phase==RUNNING&&game.phase==CRASHED)save();}
    if(game.ready)view=RESULT,selected=0,ongoing=false;
   }
  }else{
   accum=0;
   if(view==TITLE){if(right||down||left||up)selected^=1;if(back){if(save())break;error_return=TITLE;error_selection=selected;view=SAVE_ERROR;}
    else if(hit&KEY(eadk_key_zero))view=HELP;
    else if(ok){if(selected==0){if(!ongoing)fresh();ongoing=true;view=PLAY;stale|=keys;}else view=SHOP,selected=stats.ball;}}
   else if(view==PAUSE){if(down||right)selected=(selected+1)%3;if(up||left)selected=(selected+2)%3;if(back||(ok&&selected==0))view=PLAY,stale|=keys;else if(ok&&selected==1)save(),fresh(),view=PLAY,stale|=keys;else if(ok&&selected==2)view=TITLE,selected=0;}
   else if(view==RESULT){if(up||down||left||right)selected^=1;if(back||(ok&&selected==1))view=TITLE,selected=0;else if(ok){fresh();ongoing=true;view=PLAY;stale|=keys;}}
   else if(view==SHOP){
    if(right)selected=(selected+1)%BALL_COUNT;
    if(left)selected=(selected+BALL_COUNT-1)%BALL_COUNT;
    if(down||up)selected=selected>=4?selected-4:selected+4<BALL_COUNT?selected+4:BALL_COUNT-1;
    if(back)view=TITLE,selected=1;
    else if(ok){if(shop_owned(&stats,false,selected))shop_buy(&stats,false,selected),save();else if(stats.coins>=ball_styles[selected].price)view=BUY;}
   }else if(view==BUY){if(back)view=SHOP;else if(ok)shop_buy(&stats,false,selected),save(),view=SHOP;}
   else if(view==HELP){if(back||ok)view=TITLE,selected=0;}
   else if(view==SAVE_ERROR){if(back)break;if(ok&&save())break;if(hit&KEY(eadk_key_one))view=error_return,selected=error_selection;}
  }
  current_view=view;
  if(now-draw_at>=33||hit||!perf_frames){uint32_t start=(uint32_t)eadk_timing_millis();render(&game,&stats,view,selected,false,save_failed,now,push);uint32_t spent=(uint32_t)eadk_timing_millis()-start;if(spent>perf_max)perf_max=spent;if(spent>33)++perf_slow;++perf_frames;draw_at=now;}
  eadk_timing_msleep(2);
 }
 uint32_t released=(uint32_t)eadk_timing_millis();while(eadk_keyboard_scan()&&(uint32_t)eadk_timing_millis()-released<2000)eadk_timing_msleep(10);return np_app_end();
}
