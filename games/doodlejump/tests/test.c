/* SPDX-License-Identifier: GPL-3.0-only */
#include "doodle.h"
#include "render.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef char checked_layout[(offsetof(dj_game,shot)==272 && sizeof(dj_game)==308)?1:-1];
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
static void setup(dj_game *g,dj_stats *s) {
  *s=(dj_stats){0};s->motion=1;dj_new(g,42);dj_start(g,s);assert(dj_valid(g,s));
}
static void landing(dj_game *g,dj_stats *s,unsigned type,unsigned item) {
  setup(g,s);memset(g->platform,0,sizeof g->platform);
  g->platform[0]=(dj_platform){20*DJ_Q,144,58,(uint8_t)type,(uint8_t)item,0,1};
  g->y=g->peak=24*DJ_Q;g->vy=-1400;
}
static void collisions(void) {
  dj_game g;dj_stats s;
  landing(&g,&s,DJ_SOLID,0);dj_step(&g,&s,0);assert(g.y==20*DJ_Q&&g.vy==1440&&g.jumps==1&&s.jumps==1);
  landing(&g,&s,DJ_SOLID,0);g.y=18*DJ_Q;g.vy=1000;dj_step(&g,&s,0);assert(g.jumps==0&&g.vy>0);
  landing(&g,&s,DJ_BRITTLE,0);dj_step(&g,&s,0);assert(!g.platform[0].active&&g.vy<0&&g.jumps==0);
  landing(&g,&s,DJ_FADING,0);dj_step(&g,&s,0);assert(!g.platform[0].active&&g.vy==1440);
  landing(&g,&s,DJ_MOVING,0);g.platform[0].phase=32;dj_step(&g,&s,0);assert(g.vy==1440);
  landing(&g,&s,DJ_SOLID,DJ_SPRING);dj_step(&g,&s,0);assert(g.vy==2432&&s.springs==1);
  landing(&g,&s,DJ_SOLID,DJ_SPRING);g.x+=25*DJ_Q;dj_step(&g,&s,0);assert(g.vy==1440&&!s.springs);
  landing(&g,&s,DJ_SOLID,DJ_ROCKET);dj_step(&g,&s,0);assert(g.boost==80&&s.rockets==1&&!g.platform[0].item);
  landing(&g,&s,DJ_SOLID,DJ_ROCKET);g.enemy[0]=(dj_enemy){25*DJ_Q,144,32,1};
  dj_step(&g,&s,0);assert(g.boost==80&&g.phase==DJ_RUNNING&&!g.enemy[0].active);
  setup(&g,&s);g.x=DJ_LEFT*DJ_Q+16;g.vx=-672;dj_step(&g,&s,-1);assert(g.x==DJ_LEFT*DJ_Q&&!g.vx);
  dj_step(&g,&s,1);assert(g.x>DJ_LEFT*DJ_Q&&g.vx>0);
  g.x=DJ_RIGHT*DJ_Q-16;g.vx=672;dj_step(&g,&s,1);assert(g.x==DJ_RIGHT*DJ_Q&&!g.vx);
  dj_step(&g,&s,-1);assert(g.x<DJ_RIGHT*DJ_Q&&g.vx<0);
  /* A platform across the old seam cannot catch the player anymore. */
  landing(&g,&s,DJ_SOLID,0);g.x=DJ_LEFT*DJ_Q;g.platform[0].x=DJ_WIDTH-1;
  dj_step(&g,&s,0);assert(!g.jumps&&g.vy<0);
  setup(&g,&s);g.enemy[0]=(dj_enemy){50*DJ_Q,144,32,1};assert(dj_fire(&g));assert(!dj_fire(&g));
  dj_step(&g,&s,0);assert(!g.enemy[0].active&&s.monsters==1);
  setup(&g,&s);g.y=g.peak=35*DJ_Q;g.vy=-2304;g.enemy[0]=(dj_enemy){20*DJ_Q,144,32,1};
  dj_step(&g,&s,0);assert(!g.enemy[0].active&&g.vy==1690&&g.phase==DJ_RUNNING);
  setup(&g,&s);g.enemy[0]=(dj_enemy){25*DJ_Q,144,32,1};dj_step(&g,&s,0);assert(g.phase==DJ_OVER&&g.reason==DJ_HIT);
  setup(&g,&s);g.boost=1;g.enemy[0]=(dj_enemy){25*DJ_Q,144,32,1};dj_step(&g,&s,0);
  assert(g.phase==DJ_RUNNING&&!g.enemy[0].active&&!g.boost);
  setup(&g,&s);g.y=-23*DJ_Q;g.vy=-2304;dj_step(&g,&s,0);assert(g.phase==DJ_OVER&&g.reason==DJ_FELL);
  dj_game frozen=g;dj_stats record=s;dj_step(&g,&s,1);assert(!memcmp(&g,&frozen,sizeof g)&&!memcmp(&s,&record,sizeof s));
}
static int delta(int target,int x) {
  return target-x;
}
static int controller(const dj_game *g) {
  int best=-1,best_y=-1,ticks=0;
  if(g->vy>0&&!g->boost)for(unsigned i=0;i<DJ_ENEMIES;++i) {
    const dj_enemy *e=&g->enemy[i];int dy=e->y-g->y;
    int dx=delta(dj_enemy_x(e,g->tick),g->x);
    if(e->active&&dy>-15*DJ_Q&&dy<80*DJ_Q&&abs(dx)<30*DJ_Q) {
      int avoid=dx>=0?-1:1;
      if((avoid<0&&g->x<(DJ_LEFT+28)*DJ_Q)||(avoid>0&&g->x>(DJ_RIGHT-28)*DJ_Q))return 0;
      return avoid;
    }
  }
  for(unsigned i=0;i<DJ_PLATFORMS;++i) {
    const dj_platform *p=&g->platform[i];if(!p->active||p->type==DJ_BRITTLE||p->y<g->camera-12*DJ_Q)continue;
    int y=g->y,v=g->vy,boost=g->boost;
    for(int t=1;t<200;++t) {
      int old=y;if(boost)--boost,v=1536;else v-=47;
      if(v<-2304)v=-2304;
      y+=v;
      if(v<0&&old>=p->y&&y<=p->y) {
        int dx=delta(dj_platform_x(p,g->tick+(unsigned)t),g->x);
        if(abs(dx)<(t>7?(t-7)*672:0)+((int)p->width/2+5)*DJ_Q&&p->y>best_y)
          best=(int)i,best_y=p->y,ticks=t;
        break;
      }
      if(y<g->camera-24*DJ_Q)break;
    }
  }
  if(best<0)return 0;
  int aim=dj_platform_x(&g->platform[best],g->tick+(unsigned)ticks);
  int dx=delta(aim,g->x+g->vx*3);
  return abs(dx)<3*DJ_Q?0:dx>0?1:-1;
}
static void saves(void) {
  dj_game g,a;dj_stats s,b;setup(&g,&s);
  uint8_t raw[DJ_SAVE_BYTES],again[DJ_SAVE_BYTES];
  dj_encode(&g,&s,raw);assert(dj_decode(&a,&b,raw,sizeof raw));dj_encode(&a,&b,again);assert(!memcmp(raw,again,sizeof raw));
  for(unsigned i=0;i<sizeof raw;++i) {
    raw[i]^=1;dj_game before=a;dj_stats previous=b;
    assert(!dj_decode(&a,&b,raw,sizeof raw));assert(!memcmp(&a,&before,sizeof a)&&!memcmp(&b,&previous,sizeof b));raw[i]^=1;
  }
  assert(!dj_decode(&a,&b,NULL,0)&&!dj_decode(&a,&b,raw,sizeof raw-1));
  g.vx=INT32_MAX;dj_encode(&g,&s,raw);assert(!dj_decode(&a,&b,raw,sizeof raw));
  setup(&g,&s);g.camera=INT32_MAX;dj_encode(&g,&s,raw);assert(!dj_decode(&a,&b,raw,sizeof raw));
  setup(&g,&s);g.platform[0].width=255;dj_encode(&g,&s,raw);assert(!dj_decode(&a,&b,raw,sizeof raw));
  setup(&g,&s);g.enemy[0].active=2;dj_encode(&g,&s,raw);assert(!dj_decode(&a,&b,raw,sizeof raw));
  setup(&g,&s);g.route_phase=128;dj_encode(&g,&s,raw);assert(!dj_decode(&a,&b,raw,sizeof raw));
  setup(&g,&s);g.route_phase=7;dj_encode(&g,&s,raw);assert(!dj_decode(&a,&b,raw,sizeof raw));
  setup(&g,&s);g.shot[0]=(dj_shot){INT32_MIN,1,1};dj_encode(&g,&s,raw);assert(!dj_decode(&a,&b,raw,sizeof raw));
  landing(&g,&s,DJ_SOLID,0);s.jumps=UINT32_MAX;dj_step(&g,&s,0);assert(s.jumps==UINT32_MAX);
  FILE *legacy=fopen("../tests/legacy-save.bin","rb");assert(legacy);
  assert(fread(raw,1,sizeof raw,legacy)==sizeof raw);fclose(legacy);
  assert(dj_decode(&g,&s,raw,sizeof raw)&&!g.route_phase);
  dj_encode(&g,&s,again);assert(!memcmp(again,"DJN3",4));
  assert(dj_decode(&a,&b,again,sizeof again)&&!memcmp(&g,&a,sizeof g)&&!memcmp(&s,&b,sizeof s));
  uint32_t runs=s.runs,best=s.best;
  for(unsigned i=0;i<120;++i){g.boost=80;dj_step(&g,&s,0);assert(dj_valid(&g,&s));}
  assert(g.route_phase>0&&g.route_phase<=127&&s.runs==runs&&s.best>=best);
  legacy=fopen("../tests/wrapping-save.bin","rb");assert(legacy);
  assert(fread(raw,1,sizeof raw,legacy)==sizeof raw);fclose(legacy);
  assert(!memcmp(raw,"DJN1",4)&&dj_decode(&g,&s,raw,sizeof raw));
  assert(g.x>=DJ_LEFT*DJ_Q&&g.x<=DJ_RIGHT*DJ_Q);
  dj_encode(&g,&s,again);assert(!memcmp(raw+4,again+4,25)); /* Lifetime records. */
  assert(!memcmp(raw+36,again+36,4)&&!memcmp(raw+48,again+48,8)); /* Height/camera/peak. */
  legacy=fopen("../tests/bounded-save.bin","rb");assert(legacy);
  assert(fread(raw,1,sizeof raw,legacy)==sizeof raw);fclose(legacy);
  assert(!memcmp(raw,"DJN2",4)&&dj_decode(&g,&s,raw,sizeof raw)&&!g.route_phase);
  dj_encode(&g,&s,again);
  assert(!memcmp(raw+4,again+4,82)&&!memcmp(raw+87,again+87,DJ_SAVE_BYTES-91));
  setup(&g,&s);dj_new(&g,1);dj_start(&g,&s);a=g;b=s;
  for(unsigned i=0;i<1200;++i) {
    int steer=controller(&g);
    if(i%16==0){dj_fire(&g);dj_fire(&a);}
    dj_step(&g,&s,steer);dj_step(&a,&b,steer);
    if(i%37==0) {
      dj_encode(&g,&s,raw);dj_encode(&a,&b,again);assert(!memcmp(raw,again,sizeof raw));
      assert(dj_decode(&a,&b,raw,sizeof raw));
    }
  }
}
static void distribution(void) {
  unsigned samples=0,all_counts[6]={0},all_turns=0;
  for(unsigned seed=1;seed<=120;++seed) {
    dj_game g;dj_stats s;setup(&g,&s);dj_new(&g,seed);dj_start(&g,&s);
    unsigned counts[6]={0},n=0,streak=0,side=2,short_hops=0,long_hops=0,turns=0,runs=0;
    int previous=g.route_x,last_direction=0;
    for(unsigned tick=0;n<240&&tick<4000;++tick) {
      uint32_t rows=g.rows;g.boost=80;dj_step(&g,&s,0);assert(dj_valid(&g,&s));
      if(g.rows==rows)continue;
      assert(g.rows==rows+1);
      int hop=g.route_x-previous;assert(abs(hop)<=68);
      assert(g.route_x>=24&&g.route_x<=264);
      if(abs(hop)<16)++short_hops;
      if(abs(hop)>=48)++long_hops;
      int direction=hop>0?1:hop<0?-1:0;
      if(direction&&last_direction) {
        if(direction!=last_direction&&previous>68&&previous<220)++turns;
        if(direction==last_direction)++runs;
      }
      if(direction)last_direction=direction;
      previous=g.route_x;
      unsigned band=g.route_x/48;assert(band<6);++counts[band];
      unsigned now=band/3;streak=now==side?streak+1:1;side=now;assert(streak<=6);
      assert(g.x>=DJ_LEFT*DJ_Q&&g.x<=DJ_RIGHT*DJ_Q);
      for(unsigned i=0;i<DJ_PLATFORMS;++i)if(g.platform[i].active) {
        const dj_platform *p=&g.platform[i];int x=dj_platform_x(p,g.tick),half=(p->width+1)/2;
        assert(x>=half*DJ_Q&&x<=(DJ_WIDTH-half)*DJ_Q);
      }
      ++n;
    }
    assert(n==240&&short_hops>0&&long_hops>0&&turns>10&&runs>10);
    unsigned left=counts[0]+counts[1]+counts[2];assert(left>70&&left<170);
    for(unsigned i=0;i<6;++i){if(i>0&&i<5)assert(counts[i]>0);all_counts[i]+=counts[i];}
    samples+=n;all_turns+=turns;
  }
  for(unsigned i=0;i<6;++i)assert(all_counts[i]>100);
  printf("Random generation: %u ledges, direct hops <=68 px, half streak <=6; %u interior turns\n",samples,all_turns);
  printf("Zone counts: %u / %u / %u / %u / %u / %u\n",all_counts[0],all_counts[1],all_counts[2],all_counts[3],all_counts[4],all_counts[5]);
}
static void long_runs(void) {
  uint32_t successes=0,highest=0;unsigned observed=0,falls=0,hits=0;
  FILE *trace=fopen("climb-input.bin","wb");assert(trace);
  for(unsigned seed=1;seed<=80;++seed) {
    dj_game g;dj_stats s;setup(&g,&s);dj_new(&g,seed);
    if(seed==1) {
      uint8_t initial[DJ_SAVE_BYTES];dj_encode(&g,&s,initial);
      FILE *f=fopen("start-save.bin","wb");assert(f);fwrite(initial,1,sizeof initial,f);fclose(f);
    }
    dj_start(&g,&s);
    for(unsigned n=0;n<30000&&g.phase==DJ_RUNNING&&dj_height(&g)<6000;++n) {
      int steer=controller(&g);bool fire=n%16==0;
      if(seed==1){fputc(steer+1,trace);fputc(fire,trace);}
      if(fire)dj_fire(&g);
      dj_step(&g,&s,steer);assert(dj_valid(&g,&s));
      if(n%97==0) {
        uint8_t raw[DJ_SAVE_BYTES];dj_game a;dj_stats b;dj_encode(&g,&s,raw);assert(dj_decode(&a,&b,raw,sizeof raw));
      }
      for(unsigned i=0;i<DJ_PLATFORMS;++i)if(g.platform[i].active)observed|=1u<<g.platform[i].type;
      if(seed==1&&dj_height(&g)>1100&&dj_height(&g)<1200) {
        uint8_t raw[DJ_SAVE_BYTES];dj_encode(&g,&s,raw);FILE *f=fopen("climb-save.bin","wb");assert(f);fwrite(raw,1,sizeof raw,f);fclose(f);
      }
    }
    if(dj_height(&g)>=6000)++successes;
    else if(g.reason==DJ_HIT)++hits;
    else ++falls;
    if(dj_height(&g)>highest)highest=dj_height(&g);
    if(seed==1)printf("Reference: %u height, %u bounces, %u springs, %u rockets, %u monsters\n",(unsigned)dj_height(&g),(unsigned)s.jumps,(unsigned)s.springs,(unsigned)s.rockets,(unsigned)s.monsters);
  }
  fclose(trace);
  dj_game fixture;dj_stats book;setup(&fixture,&book);
  fixture.camera=900*DJ_Q;fixture.y=fixture.peak=1024*DJ_Q;fixture.next_y=1160*DJ_Q;fixture.boost=70;
  for(unsigned i=0;i<DJ_PLATFORMS;++i)fixture.platform[i]=(dj_platform){(int32_t)(914+i*12)*DJ_Q,(uint16_t)(30+i*15),43,(uint8_t)(i%4),(uint8_t)(i%3),0,1};
  for(unsigned i=0;i<DJ_ENEMIES;++i)fixture.enemy[i]=(dj_enemy){(int32_t)(950+i*50)*DJ_Q,(uint16_t)(40+i*90),32,1};
  for(unsigned i=0;i<DJ_SHOTS;++i)fixture.shot[i]=(dj_shot){(int32_t)(45+i*70)*DJ_Q,(int32_t)(940+i*60)*DJ_Q,1};
  assert(dj_valid(&fixture,&book));uint8_t data[DJ_SAVE_BYTES];dj_encode(&fixture,&book,data);
  FILE *stress=fopen("stress-save.bin","wb");assert(stress);fwrite(data,1,sizeof data,stress);fclose(stress);
  printf("Controller: %u/80 reached 6000; highest %u; platform kinds %u\n",(unsigned)successes,(unsigned)highest,observed);
  printf("Other runs: %u monster contacts, %u falls\n",hits,falls);
  /* The controller can lose a run; require broad successful play through
   * the new random layouts, alongside the separate route invariants. */
  assert(successes>=70&&highest>=6000&&observed==15);
  dj_game g;dj_stats s;setup(&g,&s);uint32_t r=0x124578u;
  for(unsigned i=0;i<120000;++i) {
    if(g.phase==DJ_OVER)dj_new(&g,r),dj_start(&g,&s);
    r^=r<<13;r^=r>>17;r^=r<<5;
    if(r%31==0)dj_fire(&g);
    dj_step(&g,&s,(int)(r%3)-1);assert(dj_valid(&g,&s));
  }
}
static void walls(void) {
  dj_game g;dj_stats s;setup(&g,&s);g.boost=80;
  uint8_t raw[DJ_SAVE_BYTES];dj_encode(&g,&s,raw);
  FILE *f=fopen("edge-save.bin","wb");assert(f);fwrite(raw,1,sizeof raw,f);fclose(f);
  for(int side=-1;side<=1;side+=2) {
    setup(&g,&s);g.boost=80;
    for(unsigned i=0;i<72;++i) {
      int32_t before=g.x;dj_step(&g,&s,side);
      assert(g.x>=DJ_LEFT*DJ_Q&&g.x<=DJ_RIGHT*DJ_Q&&abs(g.x-before)<=672);
    }
    assert(g.x==(side<0?DJ_LEFT:DJ_RIGHT)*DJ_Q&&!g.vx);
    dj_step(&g,&s,-side);assert(side<0?g.vx>0:g.vx<0);
  }
  puts("PASS: bounded movement, sustained wall contact and immediate reversal");
}
static void rendering(void) {
  dj_game g;dj_stats s;setup(&g,&s);
  g.x=DJ_LEFT*DJ_Q;g.vx=-600;
  g.platform[1]=(dj_platform){62*DJ_Q,30,58,DJ_MOVING,DJ_SPRING,0,1};
  g.enemy[0]=(dj_enemy){140*DJ_Q,1,32,1};g.boost=12;
  for(unsigned view=0;view<=DJ_SAVE_ERROR;++view) {
    dj_invalidate();dj_render(&g,&s,view,0,0,false,push);
    if(view==DJ_TITLE)shot("title.ppm");
    if(view==DJ_GUIDE)shot("guide.ppm");
    if(view==DJ_JOURNAL)shot("journal.ppm");
    for(unsigned frame=1;frame<=36;++frame) {
      if(view==DJ_PLAY){int steer=controller(&g);dj_step(&g,&s,steer);}
      dj_render(&g,&s,view,0,frame*64,false,push);memcpy(reference,screen,sizeof screen);
      dj_invalidate();dj_render(&g,&s,view,0,frame*64,false,push);assert(!memcmp(screen,reference,sizeof screen));
    }
    unsigned before=pushes;dj_render(&g,&s,view,0,36*64,false,push);assert(pushes==before);
  }
  uint8_t data[DJ_SAVE_BYTES];FILE *f=fopen("climb-save.bin","rb");assert(f);assert(fread(data,1,sizeof data,f)==sizeof data);fclose(f);
  assert(dj_decode(&g,&s,data,sizeof data));
  dj_invalidate();dj_render(&g,&s,DJ_PLAY,0,0,false,push);shot("gameplay.ppm");
}
int main(void) {
  collisions();walls();saves();distribution();long_runs();rendering();puts("PASS: collisions, saves, distribution, generated courses, stress, partial/full rendering");return 0;
}
