/* SPDX-License-Identifier: GPL-3.0-only */
#include "tower.h"
#include "render.h"
#include <assert.h>
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
static void setup(tb_game *g,tb_stats *s) {
  *s=(tb_stats){0};s->guide=s->motion=1;tb_new(g);tb_start(g,s);
}
static void settle(tb_game *g,const tb_stats *s) {
  for(unsigned i=0;g->phase==TB_SETTLING&&i<50;++i)tb_step(g,s);
  assert(g->phase==TB_MOVING);
}
static void geometry(void) {
  tb_game g;tb_stats s;setup(&g,&s);
  g.offset=10*TB_Q;assert(tb_place(&g,&s)&&g.height==1&&g.count==2);
  tb_layer p=g.layer[1];assert(p.x==-26*TB_Q&&p.w==62*TB_Q&&p.d==TB_SIZE);
  assert(g.shard[0].w==10*TB_Q&&g.shard[0].x==36*TB_Q&&g.axis==0);
  assert(!tb_place(&g,&s));settle(&g,&s);assert(g.axis==1);
  g.offset=-12*TB_Q;tb_place(&g,&s);p=g.layer[2];
  assert(p.z==-36*TB_Q&&p.d==60*TB_Q&&p.w==62*TB_Q);
  assert(g.shard[1].z==-48*TB_Q&&g.shard[1].d==12*TB_Q);assert(tb_valid(&g,&s));
  /* Snap keeps the complete slab; every third perfect restores two units. */
  for(unsigned i=0;i<3;++i) {
    settle(&g,&s);g.offset=i==1?-TB_SNAP:TB_SNAP;assert(tb_place(&g,&s));
  }
  assert(g.streak==3&&g.perfect==3&&g.result==TB_GROW&&s.perfect==3);
  assert(g.layer[g.count-1].w==64*TB_Q&&s.best==5);assert(tb_valid(&g,&s));
  settle(&g,&s);g.offset=TB_TRAVEL;tb_place(&g,&s);
  assert(g.phase==TB_ENDING&&g.height==5&&g.misses==1);
  for(unsigned i=0;i<48;++i)tb_step(&g,&s);
  assert(g.phase==TB_OVER&&!g.timer);tb_game frozen=g;
  tb_step(&g,&s);assert(!memcmp(&frozen,&g,sizeof g)&&!tb_place(&g,&s));
  /* Quarter-unit overlap remains playable; just below the minimum falls away. */
  setup(&g,&s);g.offset=TB_SIZE-TB_MIN;tb_place(&g,&s);
  assert(g.height==1&&g.layer[1].w==TB_MIN);assert(tb_valid(&g,&s));
  setup(&g,&s);g.offset=TB_SIZE-TB_MIN+1;tb_place(&g,&s);assert(!g.height&&g.phase==TB_ENDING);
}
static void rehash(uint8_t *p) {
  uint32_t h=2166136261u;
  for(unsigned i=0;i<TB_SAVE_BYTES-4;++i)h=(h^p[i])*16777619u;
  for(unsigned i=0;i<4;++i)p[TB_SAVE_BYTES-4+i]=(uint8_t)(h>>(8*i));
}
static void saves(void) {
  tb_game g,h;tb_stats s,t;uint8_t data[TB_SAVE_BYTES],copy[TB_SAVE_BYTES];
  setup(&g,&s);g.offset=7*TB_Q;tb_place(&g,&s);
  tb_encode(&g,&s,data);assert(tb_decode(&h,&t,data,sizeof data));
  assert(h.height==1&&h.phase==TB_SETTLING&&!h.shard[0].life);
  tb_encode(&h,&t,copy);assert(!memcmp(data,copy,sizeof data));
  tb_game before=h;tb_stats prior=t;
  for(unsigned i=0;i<TB_SAVE_BYTES;++i) {
    data[i]^=128;assert(!tb_decode(&h,&t,data,sizeof data));data[i]^=128;
    assert(!memcmp(&before,&h,sizeof h)&&!memcmp(&prior,&t,sizeof t));
  }
  assert(!tb_decode(&h,&t,NULL,0)&&!tb_decode(&h,&t,data,sizeof data-1));
  unsigned bad[]={24,60,61,62,65,88};
  for(unsigned i=0;i<sizeof bad/sizeof bad[0];++i) {
    memcpy(data,copy,sizeof data);data[bad[i]]=255;rehash(data);assert(!tb_decode(&h,&t,data,sizeof data));
  }
  memcpy(data,copy,sizeof data);data[63]=0;rehash(data);assert(!tb_decode(&h,&t,data,sizeof data));
  memcpy(data,copy,sizeof data);memset(data+44,0,4);data[47]=128;rehash(data);assert(!tb_decode(&h,&t,data,sizeof data));
  g=(tb_game){0};s=(tb_stats){0};s.guide=s.motion=1;
  tb_encode(&g,&s,data);assert(tb_decode(&h,&t,data,sizeof data)&&!h.present);
}
static void reachability(void) {
  tb_game g;tb_stats s,t;tb_game loaded;uint8_t data[TB_SAVE_BYTES];
  setup(&g,&s);unsigned ticks=0;
  FILE *f=fopen("build-taps.txt","w");assert(f);
  while(g.height<3000) {
    if(g.phase==TB_MOVING&&g.offset>=-TB_SNAP&&g.offset<=TB_SNAP) {
      if(g.height<25)fprintf(f,"%u\n",ticks);
      assert(tb_place(&g,&s));
    }
    tb_step(&g,&s);++ticks;
    assert(tb_valid(&g,&s));
    if(ticks%97==0) {tb_encode(&g,&s,data);assert(tb_decode(&loaded,&t,data,sizeof data));}
    assert(g.phase!=TB_ENDING&&g.phase!=TB_OVER);
  }
  fclose(f);assert(g.count==TB_LAYERS&&g.streak==999&&s.best==3000);
  tb_encode(&g,&s,data);f=fopen("tall-start.bin","wb");assert(f);
  assert(fwrite(data,1,sizeof data,f)==sizeof data);fclose(f);
  printf("Reached 3000 floors with normal input in %u ticks; fixed %u-layer history.\n",ticks,TB_LAYERS);
  /* Unpredictable timing remains bounded across both axes and directions. */
  for(unsigned run=0;run<40;++run) {
    setup(&g,&s);
    for(unsigned tick=0;tick<3000;++tick) {
      if(tick%(13+run)==0)tb_place(&g,&s);
      tb_step(&g,&s);assert(tb_valid(&g,&s));
      if(g.phase==TB_OVER)tb_new(&g),tb_start(&g,&s);
    }
  }
}
static void pictures(void) {
  tb_game g;tb_stats s;setup(&g,&s);
  tb_invalidate();tb_render(&g,&s,TB_TITLE,0,400,false,push);shot("title.ppm");
  for(unsigned n=0;n<15;++n) {
    g.offset=n%4==0?4*TB_Q:0;tb_place(&g,&s);settle(&g,&s);
  }
  g.offset=12*TB_Q;
  tb_render(&g,&s,TB_PLAY,0,400,false,push);shot("game.ppm");
  tb_render(&g,&s,TB_PAUSE,0,400,false,push);shot("pause.ppm");
  tb_render(&g,&s,TB_STUDIO,0,400,false,push);shot("studio.ppm");
  tb_render(&g,&s,TB_GUIDE,0,400,false,push);shot("guide.ppm");
  tb_render(&g,&s,TB_READY,0,400,false,push);shot("ready.ppm");
  g.offset=TB_TRAVEL;tb_place(&g,&s);
  for(unsigned i=0;i<48;++i)tb_step(&g,&s);
  tb_render(&g,&s,TB_RESULT,0,400,false,push);shot("result.ppm");
  tb_render(&g,&s,TB_CONFIRM,0,400,false,push);shot("confirm.ppm");
  tb_render(&g,&s,TB_SAVE_ERROR,0,400,true,push);shot("storage.ppm");
  for(unsigned palette=0;palette<TB_PALETTES;++palette) {
    setup(&g,&s);s.palette=(uint8_t)palette;tb_invalidate();
    for(unsigned i=0;i<340;++i) {
      if(g.phase==TB_OVER)tb_new(&g),tb_start(&g,&s);
      if(i%73==0)tb_place(&g,&s);
      tb_step(&g,&s);tb_render(&g,&s,TB_PLAY,0,i*32,false,push);
      memcpy(reference,screen,sizeof screen);tb_invalidate();tb_render(&g,&s,TB_PLAY,0,i*32,false,push);
      assert(!memcmp(reference,screen,sizeof screen));
    }
  }
  pushes=0;tb_render(&g,&s,TB_PLAY,0,339*32,false,push);assert(pushes==0);
  s.motion=0;
  for(unsigned view=0;view<2;++view) {
    unsigned page=view?TB_STUDIO:TB_TITLE;
    tb_render(&g,&s,page,0,400,false,push);pushes=0;
    tb_render(&g,&s,page,0,1400,false,push);assert(pushes==0);
  }
}
static void migration(void) {
  tb_game g,h;tb_stats s,t;uint8_t old[TB_SAVE_BYTES],data[TB_SAVE_BYTES],copy[TB_SAVE_BYTES];
  const char *names[]={"../tests/legacy-practice-save.bin","../tests/legacy-classic-save.bin"};
  for(unsigned i=0;i<2;++i) {
    FILE *f=fopen(names[i],"rb");assert(f);assert(fread(old,1,sizeof old,f)==sizeof old);fclose(f);
    assert(!memcmp(old,"TBK1",4)&&tb_decode(&g,&s,old,sizeof old));
    assert(tb_valid(&g,&s)&&s.best==5&&s.palette==2&&s.guide&&s.motion);
    if(i==0)assert(g.height==0&&g.count==1&&!g.started&&g.present);
    else assert(g.height==3&&g.started);
    tb_encode(&g,&s,data);assert(!memcmp(data,"TBK2",4));
    assert(data[8]==0&&data[9]==0&&data[10]==0&&data[11]==0&&data[61]==0);
    assert(!memcmp(data+12,old+12,15)); /* totals, attempts, preferences */
    assert(tb_decode(&h,&t,data,sizeof data)&&t.best==5);
    tb_encode(&h,&t,copy);assert(!memcmp(data,copy,sizeof data));
    if(i==0) {
      memset(old+4,0,4);rehash(old);assert(tb_decode(&g,&s,old,sizeof old));
      assert(s.best==0&&g.height==0); /* Practice must never become a personal best. */
    }
  }
}
int main(void) {
  geometry();saves();migration();reachability();pictures();
  puts("Geometry, saves, legacy record protection, playability and exact redraw tests passed.");return 0;
}
