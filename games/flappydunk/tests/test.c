/* SPDX-License-Identifier: GPL-3.0-only */
#include "dunk.h"
#include "render.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

static uint16_t screen[320*240],reference[320*240];
static unsigned pushes;
static void push(int x,int y,int w,int h,const uint16_t *p) {
  assert(x>=0&&y>=0&&w>0&&h>0&&x+w<=320&&y+h<=240&&w*h<=320*8);
  for(int j=0;j<h;++j)memcpy(screen+(y+j)*320+x,p+j*w,(size_t)w*2);
  ++pushes;
}
static void shot(const char *name) {
  FILE *f=fopen(name,"wb");assert(f);fprintf(f,"P6\n320 240\n255\n");
  for(unsigned i=0;i<320*240;++i) {
    unsigned c=screen[i];fputc(((c>>11)&31)*255/31,f);
    fputc(((c>>5)&63)*255/63,f);fputc((c&31)*255/31,f);
  }
  fclose(f);
}
static void rehash(uint8_t *data) {
  uint32_t h=2166136261u;
  for(unsigned i=0;i<FD_SAVE_BYTES-4;++i)h=(h^data[i])*16777619u;
  for(unsigned i=0;i<4;++i)data[FD_SAVE_BYTES-4+i]=(uint8_t)(h>>(8*i));
}
static void saves(void) {
  fd_game g,h;fd_stats s={0},t;uint8_t data[FD_SAVE_BYTES],copy[FD_SAVE_BYTES];
  fd_new(&g,73);fd_encode(&g,&s,data);
  FILE *fixture=fopen("flight-start.bin","wb");assert(fixture);
  assert(fwrite(data,1,sizeof data,fixture)==sizeof data);fclose(fixture);
  fd_flap(&g,&s);
  for(unsigned i=0;i<21;++i)fd_step(&g,&s);
  fd_encode(&g,&s,data);assert(fd_decode(&h,&t,data,sizeof data));
  assert(h.phase==FD_RUNNING&&t.runs==1&&h.rng==g.rng);
  fd_encode(&h,&t,copy);assert(!memcmp(data,copy,sizeof data));
  fd_game before=h;fd_stats prior=t;
  for(unsigned i=0;i<FD_SAVE_BYTES;++i) {
    data[i]^=128;assert(!fd_decode(&h,&t,data,sizeof data));data[i]^=128;
    assert(!memcmp(&before,&h,sizeof h)&&!memcmp(&prior,&t,sizeof t));
  }
  assert(!fd_decode(&h,&t,NULL,0));assert(!fd_decode(&h,&t,data,sizeof data-1));
  data[70]=255;rehash(data);assert(!fd_decode(&h,&t,data,sizeof data));
  fd_encode(&g,&s,data);data[30]=5;rehash(data);assert(!fd_decode(&h,&t,data,sizeof data));
  fd_encode(&g,&s,data);data[87]=3;rehash(data);assert(!fd_decode(&h,&t,data,sizeof data));
  g=(fd_game){0};s=(fd_stats){0};fd_encode(&g,&s,data);
  assert(fd_decode(&h,&t,data,sizeof data)&&!h.present);
}
static void setup(fd_game *g,fd_stats *s) {
  *s=(fd_stats){0};fd_new(g,42);fd_flap(g,s);
  g->hoop[0]=(fd_hoop){83,140,0,0,0,0};
  g->y=138;g->vy=2;
}
static void scoring(void) {
  fd_game g;fd_stats s;setup(&g,&s);
  fd_step(&g,&s);assert(g.hoop[0].state==1&&g.baskets==0);
  for(unsigned i=0;i<7;++i)fd_step(&g,&s);
  assert(g.baskets==1&&s.baskets==1&&g.swishes==1&&g.score==2&&s.best==2);
  for(unsigned i=0;i<3;++i)fd_step(&g,&s);
  assert(g.baskets==1); /* Never score one hoop twice. */
  setup(&g,&s);g.y=145;g.vy=-3;
  for(unsigned i=0;i<5;++i)fd_step(&g,&s);
  assert(g.baskets==0&&g.hoop[0].state==0); /* Upward passes do not count. */
  setup(&g,&s);g.hoop[0].touched=1;g.streak=g.peak=4;
  for(unsigned i=0;i<8;++i)fd_step(&g,&s);
  assert(g.baskets==1&&g.streak==0&&g.score==1);
  setup(&g,&s);g.y=126;g.vy=5;g.hoop[0].x=109;
  for(unsigned i=0;i<5;++i)fd_step(&g,&s);
  assert(g.hoop[0].touched&&g.baskets==0); /* Fast rim contact, no tunnelling. */
  setup(&g,&s);g.hoop[0].x=30;fd_step(&g,&s);
  assert(g.phase==FD_OVER&&g.reason==FD_MISSED);
  fd_game frozen=g;fd_step(&g,&s);assert(!memcmp(&g,&frozen,sizeof g));assert(!fd_flap(&g,&s));
  setup(&g,&s);g.y=202;g.vy=5;fd_step(&g,&s);assert(g.reason==FD_FLOOR_HIT);
  setup(&g,&s);g.y=48;g.vy=-3;fd_step(&g,&s);assert(g.reason==FD_CEILING_HIT);
  /* New cosmetics require the higher score, with no alternative currency. */
  for(unsigned i=1;i<FD_SKINS;++i) {
    s=(fd_stats){0};s.best=fd_skin_targets[i]-1;
    assert(!(fd_unlocked(&s)&(1u<<i)));
    s.best++;assert(fd_unlocked(&s)&(1u<<i));
    s.unlocked=fd_unlocked(&s);s.best=0;assert(fd_unlocked(&s)&(1u<<i));
  }
  setup(&g,&s);g.score=s.best=13;
  for(unsigned i=0;i<8;++i)fd_step(&g,&s);
  assert(g.score==15&&s.best==15&&(g.unlocks&2)&&(s.unlocked&2));
  setup(&g,&s);s.baskets=UINT32_MAX;s.swishes=UINT32_MAX;s.best=UINT32_MAX;
  for(unsigned i=0;i<8;++i)fd_step(&g,&s);
  assert(s.baskets==UINT32_MAX&&s.swishes==UINT32_MAX&&s.best==UINT32_MAX);
}
static void stress(void) {
  uint8_t data[FD_SAVE_BYTES];unsigned ticks=0;
  for(unsigned seed=1;seed<=40;++seed) {
    fd_game g,h;fd_stats s={0},t;fd_new(&g,seed);fd_flap(&g,&s);
    for(unsigned i=0;i<3000;++i) {
      if(i%(8+seed%41)==0)fd_flap(&g,&s);
      fd_step(&g,&s);assert(fd_valid(&g,&s));++ticks;
      if(i%19==0) {fd_encode(&g,&s,data);assert(fd_decode(&h,&t,data,sizeof data));}
      if(g.phase>=FD_OVER) {fd_new(&g,seed*713+i);fd_flap(&g,&s);}
    }
  }
  printf("Stress: %u endless ticks, save validation every 19 ticks.\n",ticks);
}
static void screenshots(void) {
  fd_game g;fd_stats s={0};fd_new(&g,42);
  fd_invalidate();fd_render(&g,&s,FD_TITLE,0,390,false,push);shot("title.ppm");
  fd_render(&g,&s,FD_LOCKER,5,390,false,push);shot("locker.ppm");
  fd_render(&g,&s,FD_READY,0,390,false,push);shot("ready.ppm");
  fd_render(&g,&s,FD_GUIDE,0,390,false,push);shot("guide.ppm");
  fd_render(&g,&s,FD_PAUSE,0,390,false,push);shot("pause.ppm");
  setup(&g,&s);
  for(unsigned i=0;i<8;++i)fd_step(&g,&s);
  fd_render(&g,&s,FD_FLIGHT,0,390,false,push);shot("game.ppm");
  for(unsigned i=0;i<140;++i) {
    if(g.phase>=FD_OVER) {fd_new(&g,42);fd_flap(&g,&s);}
    if(i%30==0)fd_flap(&g,&s);
    fd_step(&g,&s);fd_render(&g,&s,FD_FLIGHT,0,i*32,false,push);
    memcpy(reference,screen,sizeof screen);
    fd_invalidate();fd_render(&g,&s,FD_FLIGHT,0,i*32,false,push);
    assert(!memcmp(reference,screen,sizeof screen)); /* Dirty redraw equals full. */
  }
  pushes=0;fd_render(&g,&s,FD_FLIGHT,0,139*32,false,push);assert(pushes==0);
  g.phase=FD_OVER;g.reason=FD_MISSED;
  fd_render(&g,&s,FD_RESULT,0,390,false,push);shot("result.ppm");
  fd_render(&g,&s,FD_SAVE_ERROR,0,390,true,push);shot("storage.ppm");
}
static void migration(void) {
  fd_game g,h;fd_stats s,t;uint8_t data[FD_SAVE_BYTES],copy[FD_SAVE_BYTES];
  const char *names[]={"../tests/legacy-save.bin","../tests/legacy-challenge-save.bin"};
  for(unsigned i=0;i<2;++i) {
    FILE *f=fopen(names[i],"rb");assert(f);assert(fread(data,1,sizeof data,f)==sizeof data);fclose(f);
    assert(!memcmp(data,"FDK1",4)&&fd_decode(&g,&s,data,sizeof data));
    assert(fd_valid(&g,&s)&&s.best==30&&s.skin==5&&fd_unlocked(&s)==63);
    assert(i?(g.phase==FD_WAITING&&!g.started&&g.score==0):g.phase==FD_RUNNING);
    fd_encode(&g,&s,copy);assert(!memcmp(copy,"FDK2",4));
    assert(copy[28]==0&&copy[29]==0&&copy[70]==0&&copy[31]==63);
    assert(fd_decode(&h,&t,copy,sizeof copy)&&t.skin==5&&fd_unlocked(&t)==63);
    fd_encode(&h,&t,data);assert(!memcmp(data,copy,sizeof data));
  }
}
int main(void) {
  saves();scoring();stress();migration();screenshots();
  puts("Save migration, endless physics, score unlocks and pixel-exact redraw tests passed.");return 0;
}
