#include <eadk.h>
#include <string.h>
#include "render.h"
#include "save.h"
#include "../../common/epsilon_app.h"
#include "../../common/epsilon_files.h"
#ifdef __ELF__
const char eadk_app_name[] __attribute__((section(".rodata.eadk_app_name")))="Friday Night Funkin'";
const uint32_t eadk_api_level __attribute__((section(".rodata.eadk_api_level")))=0;
#endif
#define KEY(k) (UINT64_C(1)<<(k))
static Game game;
static Menu menu;
static uint8_t save_bytes[SAVE_BYTES];
static char backup[BACKUP_BYTES];
static uint32_t saved_crc;
static bool saved;
static uint64_t stale,previous,pending,keys;
typedef struct { uint32_t at; uint8_t lanes; } Input;
static Input inputs[64];
static unsigned input_first,input_count;
static uint8_t last_lanes;
static bool queue_fault;
static uint32_t origin;
uint32_t current_view,perf_frames,perf_max,perf_input_max,input_sample_at,perf_queue_max,perf_updates;
uint32_t perf_play_draw_max,perf_play_input_max;
static void poll(void){
    uint32_t at=(uint32_t)eadk_timing_millis();if(input_sample_at&&at-input_sample_at>perf_input_max)perf_input_max=at-input_sample_at;if(menu.view==PLAY&&input_sample_at&&at-input_sample_at>perf_play_input_max)perf_play_input_max=at-input_sample_at;input_sample_at=at;
    keys=eadk_keyboard_scan();stale&=keys;uint64_t active=keys&~stale;pending|=active&~previous;previous=active;
    uint8_t lanes=game_lanes(active);if(menu.view==PLAY&&lanes!=last_lanes){if(input_count<64){inputs[(input_first+input_count)%64]=(Input){at*16,lanes};input_count++;if(input_count>perf_queue_max)perf_queue_max=input_count;}else queue_fault=true;}last_lanes=lanes;
}
static void push(int x,int y,int w,int h,const uint16_t*p){eadk_display_push_rect((eadk_rect_t){x,y,w,h},p);}
static bool save(void){
    save_encode(&game.p,save_bytes);uint32_t crc;memcpy(&crc,save_bytes+SAVE_BYTES-4,4);if(saved&&saved_crc==crc)return true;
    unsigned n=save_backup(save_bytes,backup);bool a=ef_write("fnf.sav",save_bytes,SAVE_BYTES),b=ef_write("fnf_save.py",backup,n);
    if(a&&b){saved=true;saved_crc=crc;game.dirty=0;}else saved=false;return a&&b;
}
static void begin(unsigned chart){
    if(!game_start(&game,chart)){menu.view=RESULT;menu.selected=0;return;}
    render_prepare(stage_sprites[tracks[charts[chart].track].stage]);origin=(uint32_t)eadk_timing_millis()*16-(uint32_t)game.time;
    input_first=input_count=0;queue_fault=false;menu.view=PLAY;menu.selected=0;stale|=keys;last_lanes=0;game_set_held(&game,0);input_sample_at=(uint32_t)eadk_timing_millis();render_invalidate();
}
static void normalize(void){
    if(game_chart(menu.song,menu.variant,menu.difficulty)!=NONE)return;
    for(unsigned d=0;d<5;d++)if(game_chart(menu.song,menu.variant,d)!=NONE){menu.difficulty=d;return;}
    menu.variant=0;menu.difficulty=1;
}
static void change_difficulty(int step){
    for(unsigned i=0;i<5;i++){menu.difficulty=(menu.difficulty+5+step)%5;if(game_chart(menu.song,menu.variant,menu.difficulty)!=NONE)return;}
}
static void select_songs(void){menu.view=menu.story?STORY:FREEPLAY;menu.selected=0;input_count=0;stale|=keys;}
int main(void){
    np_app_begin();game_init(&game);menu=(Menu){.view=TITLE,.difficulty=1};uint32_t n=0;const uint8_t*p=ef_read("fnf.sav",&n);bool loaded=save_decode(&game.p,p,n);
    if(!loaded){p=ef_read("fnf_save.py",&n);save_restore(&game.p,p,n);}
    keys=eadk_keyboard_scan();stale=keys;previous=pending=0;unsigned painted=999;bool exiting=false;uint32_t draw_at=0;unsigned quit_return=TITLE;
    while(!exiting){
        poll();uint32_t now=(uint32_t)eadk_timing_millis();uint64_t hit=pending;pending=0;
        bool ok=hit&(KEY(eadk_key_ok)|KEY(eadk_key_exe)),back=hit&KEY(eadk_key_back),up=hit&KEY(eadk_key_up),down=hit&KEY(eadk_key_down),left=hit&KEY(eadk_key_left),right=hit&KEY(eadk_key_right);
        if(menu.view!=SAVE_ERROR&&(keys&(KEY(eadk_key_home)|KEY(eadk_key_on_off)))){if(save())break;menu.view=SAVE_ERROR;menu.selected=0;stale|=keys;hit=0;ok=back=up=down=left=right=false;}
        if(menu.view==PLAY){
            /* Process actual timestamps sampled between LCD strips. Rendering never
             * advances the song, and a slow frame never changes the judgement window. */
            while(input_count){Input in=inputs[input_first];input_first=(input_first+1)%64;input_count--;int32_t time=(int32_t)(in.at-origin);if(time>=game.time)game_advance(&game,time,in.lanes);}
            game_advance(&game,(int32_t)(now*16-origin),last_lanes);perf_updates++;
            if(queue_fault){game.fault=game.lost=1;}
            if(game.won||game.lost){menu.view=RESULT;menu.selected=0;
                if(game.won&&menu.story&&menu.story_song+1==levels[menu.week].count){game.p.weeks|=1u<<menu.week;game.dirty=1;}
                if(game.dirty&&!save())menu.view=SAVE_ERROR;stale|=keys;
            }else if(back){menu.view=PAUSE;menu.selected=0;input_count=0;stale|=keys;}
        }else if(menu.view==TITLE){
            if(up)menu.selected=(menu.selected+2)%3;if(down)menu.selected=(menu.selected+1)%3;
            if(hit&KEY(eadk_key_zero))menu.view=CREDITS;
            else if(back){quit_return=TITLE;menu.view=QUIT;menu.selected=0;}
            else if(ok){if(menu.selected==0){menu.view=STORY;menu.difficulty=1;}else if(menu.selected==1){menu.view=FREEPLAY;normalize();}else {menu.view=OPTIONS;menu.selected=0;}}
        }else if(menu.view==STORY){
            if(up)menu.week=(menu.week+LEVEL_COUNT-1)%LEVEL_COUNT;if(down)menu.week=(menu.week+1)%LEVEL_COUNT;
            if(left)menu.difficulty=(menu.difficulty+2)%3;if(right)menu.difficulty=(menu.difficulty+1)%3;
            if(back){menu.view=TITLE;menu.selected=0;}else if(ok){menu.story=true;menu.story_song=0;menu.song=levels[menu.week].song[0];menu.variant=0;begin(game_chart(menu.song,0,menu.difficulty));}
        }else if(menu.view==FREEPLAY){
            if(up){menu.song=(menu.song+SONG_COUNT-1)%SONG_COUNT;menu.variant=0;normalize();}if(down){menu.song=(menu.song+1)%SONG_COUNT;menu.variant=0;normalize();}
            if(left)change_difficulty(-1);if(right)change_difficulty(1);
            if(hit&KEY(eadk_key_one)){for(unsigned i=0;i<4;i++){menu.variant=(menu.variant+1)%4;bool found=false;for(unsigned d=0;d<5;d++)if(game_chart(menu.song,menu.variant,d)!=NONE)found=true;if(found)break;}normalize();}
            if(back){menu.view=TITLE;menu.selected=1;}else if(ok){menu.story=false;begin(game_chart(menu.song,menu.variant,menu.difficulty));}
        }else if(menu.view==OPTIONS){
            if(up)menu.selected=(menu.selected+2)%3;if(down)menu.selected=(menu.selected+1)%3;
            if(ok||left||right){uint8_t*v=menu.selected==0?&game.p.downscroll:menu.selected==1?&game.p.ghost:&game.p.beat;*v^=1;game.dirty=1;}
            if(back){menu.view=TITLE;menu.selected=2;}
        }else if(menu.view==CREDITS){if(ok||back){menu.view=TITLE;menu.selected=0;}}
        else if(menu.view==PAUSE){
            if(up)menu.selected=(menu.selected+3)%4;if(down)menu.selected=(menu.selected+1)%4;
            if(back||(ok&&menu.selected==0)){menu.view=PLAY;input_count=0;queue_fault=false;origin=now*16-(uint32_t)game.time;game_set_held(&game,last_lanes);stale|=keys&(KEY(eadk_key_ok)|KEY(eadk_key_exe)|KEY(eadk_key_back));}
            else if(ok&&menu.selected==1)begin(game.chart);
            else if(ok&&menu.selected==2)select_songs();
            else if(ok){quit_return=PAUSE;menu.view=QUIT;menu.selected=0;}
        }else if(menu.view==RESULT){
            if(up||down||left||right)menu.selected^=1;
            if(back||(ok&&menu.selected==1))select_songs();else if(ok){if(game.won&&menu.story){menu.story_song++;if(menu.story_song>=levels[menu.week].count)select_songs();else{menu.song=levels[menu.week].song[menu.story_song];begin(game_chart(menu.song,0,menu.difficulty));}}else begin(game.chart);}
        }else if(menu.view==QUIT){
            if(up||down||left||right)menu.selected^=1;if(back||(ok&&menu.selected==0)){menu.view=quit_return;menu.selected=0;}else if(ok){if(save())exiting=true;else menu.view=SAVE_ERROR;}
        }else if(menu.view==SAVE_ERROR){if(ok&&save())exiting=true;if(back)exiting=true;if(hit&KEY(eadk_key_one)){menu.view=TITLE;menu.selected=0;}}
        current_view=menu.view;
        if(painted!=menu.view||hit||!perf_frames||(menu.view==PLAY&&now-draw_at>=16)){
            if(painted!=menu.view)render_invalidate();uint32_t at=(uint32_t)eadk_timing_millis();render(&game,&menu,push,poll);uint32_t used=(uint32_t)eadk_timing_millis()-at;if(used>perf_max)perf_max=used;if(menu.view==PLAY&&used>perf_play_draw_max)perf_play_draw_max=used;perf_frames++;draw_at=now;painted=menu.view;
        }
        eadk_timing_msleep(1);
    }
    uint32_t release=(uint32_t)eadk_timing_millis();while(eadk_keyboard_scan()&&(uint32_t)eadk_timing_millis()-release<2000)eadk_timing_msleep(10);return np_app_end();
}
