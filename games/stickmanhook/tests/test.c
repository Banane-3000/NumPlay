#include "game.h"
#include "render.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
static uint16_t screen[240][320];
static void push(int x,int y,int w,int h,const uint16_t *p){assert(x>=0&&y>=0&&x+w<=320&&y+h<=240);for(int j=0;j<h;++j)memcpy(&screen[y+j][x],p+j*w,(size_t)w*2);}
static void shot(const char *name){FILE *f=fopen(name,"wb");assert(f);fprintf(f,"P6\n320 240\n255\n");for(unsigned y=0;y<240;++y)for(unsigned x=0;x<320;++x){uint16_t c=screen[y][x];uint8_t v[3]={(uint8_t)((c>>11)*255/31),(uint8_t)(((c>>5)&63)*255/63),(uint8_t)((c&31)*255/31)};fwrite(v,1,3,f);}fclose(f);}
static uint32_t rand32(uint32_t *seed){uint32_t s=*seed;s^=s<<13;s^=s>>17;s^=s<<5;return *seed=s;}
static void save_tests(void){
  sh_stats s={22,50,781349,128,9},t={0};uint8_t data[SH_SAVE_BYTES];sh_encode(&s,data);assert(sh_decode(&t,data,sizeof data));assert(s.current==t.current&&s.unlocked==t.unlocked&&s.seed==t.seed&&s.runs==t.runs&&s.skin==t.skin);
  for(unsigned i=0;i<SH_SAVE_BYTES;++i){uint8_t b=data[i];data[i]^=1;t=s;assert(!sh_decode(&t,data,sizeof data));assert(s.current==t.current&&s.unlocked==t.unlocked&&s.seed==t.seed&&s.runs==t.runs&&s.skin==t.skin);data[i]=b;}
  assert(!sh_decode(&t,NULL,SH_SAVE_BYTES));assert(!sh_decode(&t,data,SH_SAVE_BYTES-1));
  s.current=50;sh_encode(&s,data);assert(!sh_decode(&t,data,sizeof data));s.current=0;s.skin=23;sh_encode(&s,data);assert(!sh_decode(&t,data,sizeof data));
  // Import progress from the former endless version, including beyond level 50.
  s=(sh_stats){160,160,781349,128,9};sh_encode(&s,data);data[3]='1';
  uint32_t hash=2166136261u;for(unsigned i=0;i<36;++i)hash=(hash^data[i])*16777619u;
  for(unsigned i=0;i<4;++i)data[36+i]=(uint8_t)(hash>>(i*8));
  assert(sh_decode(&t,data,sizeof data)&&t.current==49&&t.unlocked==50&&t.seed==s.seed&&t.skin==9);
}
int main(void){
  printf("game=%zu stats=%zu\n",sizeof(sh_game),sizeof(sh_stats));save_tests();
  sh_game g,h;sh_stats stats={0,50,1234,4,0};sh_new(&g,0,1234);assert(g.section_count==1&&g.finish>.65f&&g.finish<.66f);
  sh_invalidate();sh_render(&g,&stats,SH_TITLE,0,false,false,1200,push);shot("title.ppm");
  sh_invalidate();sh_render(&g,&stats,SH_SKIN_MENU,0,false,false,1200,push);shot("skins.ppm");
  sh_invalidate();sh_render(&g,&stats,SH_HELP,0,false,false,1200,push);shot("help.ppm");
  sh_new(&g,0,1234);for(unsigned i=0;i<60;++i)sh_step(&g,i>=16&&i<83);
  sh_invalidate();sh_render(&g,&stats,SH_PLAY,0,true,false,1800,push);shot("play.ppm");
  sh_invalidate();sh_render(&g,&stats,SH_PAUSE,0,true,false,1800,push);shot("pause.ppm");
  sh_invalidate();sh_render(&g,&stats,SH_RESULT,0,false,false,1800,push);shot("result.ppm");
  // Continuous hold never chains to another anchor; release boosts velocity once.
  sh_new(&g,0,1234);for(unsigned i=0;i<20;++i)sh_step(&g,false);sh_step(&g,true);assert(g.hooked);
  float hx=g.hook_x,hy=g.hook_y;for(unsigned i=0;i<35&&!g.completed;++i){sh_step(&g,true);if(g.hooked)assert(g.hook_x==hx&&g.hook_y==hy);}
  float vx=g.vx,vy=g.vy;sh_release(&g);assert(fabsf(g.vx-vx*1.2f)<.00001f&&fabsf(g.vy-vy*1.2f)<.00001f);vx=g.vx;sh_release(&g);assert(g.vx==vx);
  for(unsigned level=0;level<300;++level){
    sh_new(&g,level%50,5489+level);sh_new(&h,level%50,5489+level);assert(!memcmp(&g,&h,sizeof g));assert(sh_valid(&g));
    uint32_t rng=level+1;bool held=false;
    for(unsigned t=0;t<2400;++t){if(!(t%7)&&rand32(&rng)%4==0)held=!held;sh_step(&g,held);assert(sh_valid(&g));
      if(g.completed){uint32_t attempt=g.attempt;for(unsigned i=0;i<125;++i)sh_step(&g,false);assert(g.ready&&g.attempt==attempt);break;}}
  }
  // Replays of earlier levels must not advance the furthest level.
  sh_new(&g,38,5);g.x=g.finish+.01f;sh_step(&g,false);assert(g.completed);
  sh_stats progress={38,49,5,1,0};sh_finish(&progress,&g);assert(progress.unlocked==49);
  sh_new(&g,49,5);g.x=g.finish+.01f;sh_step(&g,false);sh_finish(&progress,&g);
  assert(progress.unlocked==50&&progress.current==49);sh_finish(&progress,&g);assert(progress.unlocked==50);
  for(unsigned i=0;i<SH_SKINS;++i)assert(sh_skin_open(&progress,i));
  sh_invalidate();sh_render(&g,&progress,SH_RESULT,0,false,false,1800,push);shot("complete.ppm");
  sh_new(&g,50,5);assert(g.level==49);sh_new(&g,UINT32_MAX,5);assert(g.level==49);
  // A player exactly on a hook may wait for gravity; no NaN or zero-length rope.
  sh_new(&g,0,1);sh_vec v;assert(sh_closest(&g,&v));g.x=v.x;g.y=v.y;sh_hook(&g,v);assert(!g.hooked&&sh_valid(&g));
  FILE *f=fopen("state-size.txt","w");assert(f);fprintf(f,"%zu\n",sizeof(sh_game));fclose(f);
  for(unsigned i=0;i<SH_LEVEL_COUNT;++i){sh_new(&g,i,1234);char name[40];snprintf(name,sizeof name,"fixture%u.bin",i);f=fopen(name,"wb");assert(f);fwrite(&g,1,sizeof g,f);fclose(f);}
  puts("save corruption, native rendering, pendulum controls, 300-level soak: PASS");return 0;
}
