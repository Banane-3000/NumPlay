#include <eadk.h>
#include "render.h"
#include "../../common/epsilon_app.h"
#include "../../common/epsilon_files.h"
#ifdef __ELF__
const char eadk_app_name[] __attribute__((section(".rodata.eadk_app_name")))="Monkey Mart";
const uint32_t eadk_api_level __attribute__((section(".rodata.eadk_api_level")))=0;
#endif
#define KEY(k) (UINT64_C(1)<<(k))
static Game game;
static uint8_t save_buffer[SAVE_BYTES];
static uint32_t saved_hash;
static bool have_save,save_failed;
uint32_t perf_frames,perf_max,perf_slow,current_view;
static void push(int x,int y,int w,int h,const uint16_t*p){eadk_display_push_rect((eadk_rect_t){x,y,w,h},p);}
static bool save(void){game_snapshot(&game);save_encode(&game.p,save_buffer);uint32_t hash=0;for(unsigned i=SAVE_BYTES-4;i<SAVE_BYTES;i++)hash=(hash<<8)|save_buffer[i];if(have_save&&!save_failed&&hash==saved_hash)return true;save_failed=!ef_write("monkeymart.sav",save_buffer,SAVE_BYTES);if(!save_failed)have_save=true,saved_hash=hash,game.dirty=0;return !save_failed;}
int main(void){np_app_begin();uint32_t size=0;const uint8_t*data=ef_read("monkeymart.sav",&size);if(save_decode(&game.p,data,size)){have_save=true;game_restore(&game);}else game_new(&game,(uint32_t)eadk_random());
 unsigned view=TITLE,selected=0,column=1,error_return=TITLE,error_selection=0,help_return=TITLE;uint64_t previous=0,keys=eadk_keyboard_scan(),stale=keys;
 uint32_t now=(uint32_t)eadk_timing_millis(),last=now,draw_at=now,accum=0,autosave=now;render_invalidate();
 for(;;){now=(uint32_t)eadk_timing_millis();uint32_t elapsed=now-last;last=now;keys=eadk_keyboard_scan();stale&=keys;uint64_t active=keys&~stale,hit=active&~previous;previous=active;
  bool ok=(hit&(KEY(eadk_key_ok)|KEY(eadk_key_exe)))!=0,back=(hit&KEY(eadk_key_back))!=0,left=(hit&KEY(eadk_key_left))!=0,right=(hit&KEY(eadk_key_right))!=0,up=(hit&KEY(eadk_key_up))!=0,down=(hit&KEY(eadk_key_down))!=0,zero=(hit&KEY(eadk_key_zero))!=0;
  if(view!=SAVE_ERROR&&(keys&(KEY(eadk_key_home)|KEY(eadk_key_on_off)))){if(save())break;error_return=view==PLAY?PAUSE:view;error_selection=selected;view=SAVE_ERROR;stale=keys;ok=back=left=right=up=down=zero=false;}
  if(view==PLAY){if(back){if(game.overview)game.overview=0;else view=PAUSE,selected=0,save();}else if(zero)game.overview^=1;
   else if(!game.overview){if(elapsed>100)elapsed=100;accum+=elapsed*60;int dx=((active&KEY(eadk_key_right))!=0)-((active&KEY(eadk_key_left))!=0),dy=((active&KEY(eadk_key_up))!=0)-((active&KEY(eadk_key_down))!=0);while(accum>=1000){game_step(&game,dx,dy);accum-=1000;}if(game.market_request){game.market_request=0;view=MARKETS;selected=1;}if(game.dirty&&now-autosave>=30000){save();autosave=now;}}
  }else{accum=0;
   if(view==TITLE){if(ok)view=PLAY,stale|=keys;else if(zero)view=HELP,help_return=TITLE;else if(back){if(save())break;view=SAVE_ERROR;}}
   else if(view==PAUSE){if(down)selected=(selected+1)%8;if(up)selected=(selected+7)%8;if(back)view=PLAY,stale|=keys;else if(ok){if(selected==0)view=PLAY,stale|=keys;else if(selected==1)view=UPGRADES,selected=0,column=1;else if(selected==2)view=HATS,selected=game.p.hat;else if(selected==3)view=MARKETS,selected=game.p.market;else if(selected==4)view=BOOSTS,selected=0;else if(selected==5)view=HELP,help_return=PAUSE;else if(selected==7)view=RESET_CONFIRM,selected=0;else{if(save())break;error_return=PAUSE;error_selection=6;view=SAVE_ERROR;}}}
   else if(view==UPGRADES){unsigned n=marts[game.p.market].upgrade_count;if(down)selected=(selected+1)%n;if(up)selected=(selected+n-1)%n;if(left)column=(column+2)%3;if(right)column=(column+1)%3;if(back)view=PAUSE,selected=1;else if(ok){if(game_upgrade(&game,selected,column))save();}}
   else if(view==HATS){if(right||down)selected=(selected+1)%21;if(left||up)selected=(selected+20)%21;if(back)view=PAUSE,selected=2;else if(ok){if(game_hat(&game,selected))save();}}
   else if(view==MARKETS){if(left||right||up||down)selected^=1;if(back)view=PAUSE,selected=3;else if(ok&&game_market(&game,selected))save(),view=PLAY,stale|=keys;}
   else if(view==BOOSTS){if(down||right)selected=(selected+1)%4;if(up||left)selected=(selected+3)%4;if(back)view=PAUSE,selected=4;else if(ok&&game_reward(&game,selected))save();}
   else if(view==HELP){if(back||ok)view=help_return,selected=0;}
   else if(view==RESET_CONFIRM){if(up||down||left||right)selected^=1;if(back||(ok&&selected==0))view=PAUSE,selected=7;else if(ok){game_new(&game,(uint32_t)eadk_random());save();view=TITLE;selected=0;}}
   else if(view==SAVE_ERROR){if(back)break;if(ok&&save())break;if(hit&KEY(eadk_key_one))view=error_return,selected=error_selection;}
  }
  current_view=view;if(now-draw_at>=33||hit||!perf_frames){uint32_t start=(uint32_t)eadk_timing_millis();render(&game,view,selected,column,save_failed,push);uint32_t spent=(uint32_t)eadk_timing_millis()-start;if(spent>perf_max)perf_max=spent;if(spent>33)++perf_slow;++perf_frames;draw_at=now;}eadk_timing_msleep(2);
 }
 uint32_t release=(uint32_t)eadk_timing_millis();while(eadk_keyboard_scan()&&(uint32_t)eadk_timing_millis()-release<2000)eadk_timing_msleep(10);return np_app_end();
}
