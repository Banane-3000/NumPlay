#include <eadk.h>
#include "render.h"
#include "../../common/epsilon_app.h"
#include "../../common/epsilon_files.h"
#ifdef __ELF__
const char eadk_app_name[] __attribute__((section(".rodata.eadk_app_name")))="Tomb of the Mask";
const uint32_t eadk_api_level __attribute__((section(".rodata.eadk_api_level")))=0;
#endif
#define KEY(k) (UINT64_C(1)<<(k))
static Game game;
static uint8_t save_bytes[SAVE_BYTES];
static uint32_t saved_crc;
static bool saved,failed;
uint32_t current_view,perf_frames,perf_max,perf_slow;
static void push(int x,int y,int w,int h,const uint16_t*p){eadk_display_push_rect((eadk_rect_t){x,y,w,h},p);}
static bool save(void){save_encode(&game.p,save_bytes);uint32_t crc=0;for(unsigned i=SAVE_BYTES-4;i<SAVE_BYTES;i++)crc=(crc<<8)|save_bytes[i];if(saved&&!failed&&crc==saved_crc)return true;failed=!ef_write("tombmask.sav",save_bytes,SAVE_BYTES);if(!failed)saved=true,saved_crc=crc,game.dirty=0;return !failed;}
int main(void){np_app_begin();game_new(&game,(uint32_t)eadk_random());uint32_t size=0;const uint8_t*data=ef_read("tombmask.sav",&size);if(save_decode(&game.p,data,size))saved=true;
 unsigned view=TITLE,selected=0,return_view=TITLE,error_view=TITLE,error_selection=0;uint64_t previous=0,keys=eadk_keyboard_scan(),stale=keys;uint32_t now=(uint32_t)eadk_timing_millis(),last=now,draw_at=now,accum=0,autosave=now;bool exiting=false,pending_action=false;int pending_direction=-1;render_invalidate();
 while(!exiting){now=(uint32_t)eadk_timing_millis();uint32_t elapsed=now-last;last=now;keys=eadk_keyboard_scan();stale&=keys;uint64_t active=keys&~stale,hit=active&~previous;previous=active;bool ok=(hit&(KEY(eadk_key_ok)|KEY(eadk_key_exe)))!=0,back=(hit&KEY(eadk_key_back))!=0,left=(hit&KEY(eadk_key_left))!=0,right=(hit&KEY(eadk_key_right))!=0,up=(hit&KEY(eadk_key_up))!=0,down=(hit&KEY(eadk_key_down))!=0;
  if(view!=SAVE_ERROR&&(keys&(KEY(eadk_key_home)|KEY(eadk_key_on_off)))){if(view==PLAY||view==PAUSE||view==QUIT)game_finish(&game);if(save())break;error_view=(view==PLAY||view==PAUSE||view==QUIT)?TITLE:view;error_selection=selected;view=SAVE_ERROR;stale=keys;ok=back=left=right=up=down=false;}
  if(view==PLAY){if(back){view=PAUSE;selected=0;save();}else{int direction=left?0:right?1:up?2:down?3:-1;if(direction>=0)pending_direction=direction;pending_action|=ok;if(elapsed>100)elapsed=100;accum+=elapsed*60;while(accum>=1000){game_step(&game,pending_direction,pending_action);pending_direction=-1;pending_action=false;accum-=1000;}if(game.dead||game.won){game_finish(&game);save();view=game.won?RESULT:DEAD;selected=0;stale|=keys;}if(game.dirty&&now-autosave>=30000){save();autosave=now;}}}
  else{accum=0;pending_direction=-1;pending_action=false;if(view==TITLE||view==MASKS||view==CHEST)game.tick=now/16;
   if(view==TITLE){if(left||right)selected^=1;if(up||down)selected^=2;if(hit&KEY(eadk_key_one))view=MISSIONS,selected=0;else if(hit&KEY(eadk_key_two))view=CHEST,game.reward_kind=0;else if(hit&KEY(eadk_key_zero))return_view=TITLE,view=HELP;else if(back)return_view=TITLE,view=QUIT,selected=0;else if(ok){if(selected==0)view=STAGES,selected=game.p.unlocked-1;else if(selected==1){game_start(&game,0,true);view=PLAY;stale|=keys;}else if(selected==2)view=MASKS,selected=game.p.mask;else view=SHOP,selected=0;}}
   else if(view==STAGES){if(left&&selected)selected--;if(right&&selected<199)selected++;if(up)selected=selected>=5?selected-5:0;if(down)selected=selected+5<200?selected+5:199;if(back)view=TITLE,selected=0;else if(ok&&selected<game.p.unlocked&&game_start(&game,selected,false))view=PLAY,stale|=keys;}
   else if(view==PAUSE){if(up)selected=(selected+3)%4;if(down)selected=(selected+1)%4;if(back||(ok&&selected==0))view=PLAY,stale|=keys;else if(ok){if(selected==1){game_finish(&game);game_start(&game,game.level,game.arcade);view=PLAY;stale|=keys;}else if(selected==2){game_finish(&game);save();view=game.arcade?TITLE:STAGES;selected=game.arcade?1:game.level;}else return_view=PAUSE,view=QUIT,selected=0;}}
   else if(view==QUIT){if(up||down||left||right)selected^=1;if(back||(ok&&selected==0))view=return_view,selected=0;else if(ok){if(return_view==PAUSE)game_finish(&game);if(save())exiting=true;else error_view=QUIT,error_selection=1,view=SAVE_ERROR;}}
   else if(view==DEAD){if(up||down)selected^=1;if(back||(ok&&selected==1))view=game.arcade?TITLE:STAGES,selected=game.arcade?1:game.level;else if(ok){game_start(&game,game.level,game.arcade);view=PLAY;stale|=keys;}}
   else if(view==RESULT){if(back||(ok&&game.level==199))view=STAGES,selected=game.level;else if(ok){game_start(&game,game.level+1,false);view=PLAY;stale|=keys;}}
   else if(view==MASKS){if(left||up)selected=(selected+15)%16;if(right||down)selected=(selected+1)%16;if(ok&&game_mask(&game,selected))save();if(back)view=TITLE,selected=2;}
   else if(view==SHOP){if(up)selected=(selected+4)%5;if(down)selected=(selected+1)%5;if(ok&&game_shop(&game,selected))save();if(back)view=TITLE,selected=3;}
   else if(view==MISSIONS){if(right||down)selected=(selected+1)%8;if(left||up)selected=(selected+7)%8;if(ok&&game_mission_claim(&game,selected))save();if(back)view=TITLE,selected=0;}
   else if(view==CHEST){if(ok&&!game.combo&&game_chest(&game))save();if(back)view=TITLE,selected=0,game.combo=0;}
   else if(view==HELP){if(back||ok)view=return_view,selected=0;}
   else if(view==SAVE_ERROR){if(back)exiting=true;if(ok&&save())exiting=true;if(hit&KEY(eadk_key_one))view=error_view,selected=error_selection;}
  }
  current_view=view;if(now-draw_at>=33||hit||!perf_frames){uint32_t start=(uint32_t)eadk_timing_millis();render(&game,view,selected,failed,push);if(view==CHEST&&game.combo)--game.combo;uint32_t spent=(uint32_t)eadk_timing_millis()-start;if(spent>perf_max)perf_max=spent;if(spent>33)++perf_slow;++perf_frames;draw_at=now;}eadk_timing_msleep(2);
 }
 uint32_t release=(uint32_t)eadk_timing_millis();while(eadk_keyboard_scan()&&(uint32_t)eadk_timing_millis()-release<2000)eadk_timing_msleep(10);return np_app_end();
}
