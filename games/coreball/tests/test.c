#include "game.h"
#include "assets.h"
#include "render.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
static uint16_t frame[320*240];
static void push(int x,int y,int w,int h,const uint16_t*p){assert(x>=0&&y>=0&&x+w<=320&&y+h<=240);for(int j=0;j<h;j++)memcpy(frame+(y+j)*320+x,p+j*w,w*2);}
static void screenshot(const char*name,Game*g,Stats*s,unsigned view,unsigned selected){render_invalidate();render(g,s,view,selected,false,false,400,push);FILE*f=fopen(name,"wb");assert(f);fprintf(f,"P6\n320 240\n255\n");for(int i=0;i<320*240;i++){uint16_t c=frame[i];uint8_t p[3]={(c>>11)*255/31,((c>>5)&63)*255/63,(c&31)*255/31};fwrite(p,1,3,f);}fclose(f);}
static uint32_t rng=1234567;
static unsigned random_u(void){rng^=rng<<13;rng^=rng>>17;rng^=rng<<5;return rng;}
int main(void){
 Stats s={0},decoded={0};uint8_t bytes[SAVE_BYTES];save_encode(&s,bytes);assert(save_decode(&decoded,bytes,sizeof bytes));for(unsigned i=0;i<sizeof bytes;i++){bytes[i]^=1;assert(!save_decode(&decoded,bytes,sizeof bytes));bytes[i]^=1;}assert(!save_decode(&decoded,NULL,0));
 assert(!save_decode(&decoded,bytes,SAVE_BYTES-1));Stats invalid={.current=60,.unlocked=60};save_encode(&invalid,bytes);assert(!save_decode(&decoded,bytes,sizeof bytes));invalid=(Stats){.current=3,.unlocked=2};save_encode(&invalid,bytes);assert(!save_decode(&decoded,bytes,sizeof bytes));
 unsigned total=0,maxattempts=0;
 for(unsigned level=0;level<60;level++){
  Game g;bool won=false;
  for(unsigned attempt=0;attempt<160&&!won;attempt++){
   game_new(&g,level);unsigned idle=0;
   for(unsigned t=0;t<18000&&g.phase==RUNNING;t++){
    bool fire=false;
    if(g.remaining){Game preview=g;game_step(&preview,true);for(int j=0;j<3;j++)game_step(&preview,false);if(preview.phase!=FAILED&&(random_u()%13)<6){fire=true;idle=0;}else ++idle;}
    game_step(&g,fire);assert(game_valid(&g));++total;if(idle>1500)break;
   }
   if(g.phase==CLEARED){won=true;if(attempt+1>maxattempts)maxattempts=attempt+1;game_finish(&s,&g);assert(s.unlocked==level+1);}
  }
  if(!won){printf("Unsolved level %u, pins %u remaining %u\n",level+1,g.count,g.remaining);return 1;}
  if(level==59){Game clear=g;clear.phase_age=40;FILE*f=fopen("stress.bin","wb");fwrite(&clear,1,sizeof clear,f);fclose(f);}
  game_new(&g,level);for(unsigned t=0;t<45;t++)game_step(&g,false);char path[64];snprintf(path,sizeof path,"fixture%u.bin",level);FILE*f=fopen(path,"wb");fwrite(&g,1,sizeof g,f);fclose(f);
 }
 assert(s.unlocked==60&&s.current==59);
 Game g;game_new(&g,0);g.angle=45;game_step(&g,true);assert(g.remaining==7&&g.count==4);game_step(&g,false);game_step(&g,false);game_step(&g,false);assert(g.count==5);
 game_new(&g,0);g.angle=90;assert(game_collision(&g,160,150));assert(!game_collision(&g,160,180));
 game_new(&g,59);uint8_t direction=g.direction;game_step(&g,true);assert(g.direction!=direction);
 for(unsigned m=0;m<12;m++){game_new(&g,0);g.motion=m;for(unsigned t=0;t<360000;t++)game_step(&g,false);assert(game_valid(&g));}
 s=(Stats){.unlocked=60,.current=0};game_new(&g,0);screenshot("title.ppm",&g,&s,TITLE,0);screenshot("gameplay.ppm",&g,&s,PLAY,0);screenshot("levels.ppm",&g,&s,LEVELS,48);screenshot("help.ppm",&g,&s,HELP,0);
 FILE*f=fopen("host-report.json","w");fprintf(f,"{\"levels_solved\":60,\"simulation_frames\":%u,\"max_solver_attempts\":%u,\"game_bytes\":%u,\"stats_bytes\":%u,\"remaining_offset\":%u,\"level_offset\":%u,\"rotation_soak_frames\":4320000}\n",total,maxattempts,(unsigned)sizeof g,(unsigned)sizeof s,(unsigned)offsetof(Game,remaining),(unsigned)offsetof(Game,level));fclose(f);printf("PASS: 60 levels solved, %u frames, max %u attempts; game %u bytes\n",total,maxattempts,(unsigned)sizeof g);return 0;
}
