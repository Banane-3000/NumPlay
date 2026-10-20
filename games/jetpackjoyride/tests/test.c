/* SPDX-License-Identifier: GPL-3.0-only */
#include "jetpack.h"
#include "render.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
typedef char layout_checked[(sizeof(jj_game)==232&&offsetof(jj_game,hazard)==40&&offsetof(jj_game,pickup)==88)?1:-1];
static uint16_t screen[320*240],reference[320*240];
static unsigned pushes;
static void push(int x,int y,int w,int h,const uint16_t *p) {
  assert(x>=0&&y>=0&&w>0&&h>0&&x+w<=320&&y+h<=240&&w*h<=2560);
  for(int j=0;j<h;++j)memcpy(screen+(y+j)*320+x,p+j*w,(size_t)w*2);
  ++pushes;
}
static void shot(const char *name) {
  FILE *f=fopen(name,"wb");assert(f);fprintf(f,"P6\n320 240\n255\n");
  for(unsigned i=0;i<320*240;++i) {
    unsigned c=screen[i];fputc(((c>>11)&31)*255/31,f);fputc(((c>>5)&63)*255/63,f);fputc((c&31)*255/31,f);
  }
  fclose(f);
}
static void fixture(const char *name,const jj_game *g,const jj_stats *s) {
  assert(jj_valid(g,s));uint8_t data[JJ_SAVE_BYTES];jj_encode(g,s,data);
  FILE *f=fopen(name,"wb");assert(f);assert(fwrite(data,1,sizeof data,f)==sizeof data);fclose(f);
}
static void setup(jj_game *g,jj_stats *s) {*s=(jj_stats){0};jj_new(g,42);jj_start(g,s);}
static bool controller(const jj_game *g);
static void collisions(void) {
  jj_game g; jj_stats s;setup(&g,&s);
  for(int i=0;i<200;++i)jj_step(&g,&s,true);
  assert(g.y==42*JJ_Q&&g.vy==0&&g.phase==JJ_RUNNING);
  memset(g.hazard,0,sizeof g.hazard);g.next=600*JJ_Q;
  for(int i=0;i<100;++i)jj_step(&g,&s,false);
  assert(g.y==198*JJ_Q&&g.vy==0&&s.runs==1);
  jj_start(&g,&s);assert(s.runs==1);
  for(unsigned kind=0;kind<=JJ_ROTATING;++kind) {
    setup(&g,&s);g.y=120*JJ_Q;
    g.hazard[0]=(jj_hazard){67*JJ_Q,120,(uint8_t)kind,0,1};jj_step(&g,&s,false);
    assert(g.phase==JJ_OVER&&g.reason==(kind==JJ_MISSILE?2:1));
    jj_game frozen=g;jj_stats book=s;jj_step(&g,&s,true);
    assert(!memcmp(&g,&frozen,sizeof g)&&!memcmp(&s,&book,sizeof s));
    setup(&g,&s);g.y=120*JJ_Q;g.shield=1;
    g.hazard[0]=(jj_hazard){67*JJ_Q,120,(uint8_t)kind,0,1};jj_step(&g,&s,false);
    assert(g.phase==JJ_RUNNING&&!g.shield&&g.invuln==80&&!g.hazard[0].active);
  }
  setup(&g,&s);g.y=120*JJ_Q;g.pickup[0]=(jj_pickup){65*JJ_Q,120,JJ_COIN,1};jj_step(&g,&s,false);
  assert(g.coins==1&&s.coins==1&&s.wallet==1&&!g.pickup[0].active);jj_step(&g,&s,false);assert(g.coins==1);
  g.pickup[0]=(jj_pickup){65*JJ_Q,120,JJ_SHIELD,1};g.hazard[0]=(jj_hazard){65*JJ_Q,120,JJ_HORIZONTAL,0,1};
  jj_step(&g,&s,false);assert(g.invuln==80&&!g.shield&&g.phase==JJ_RUNNING);
  setup(&g,&s);g.y=120*JJ_Q;g.hazard[0]=(jj_hazard){356*JJ_Q,65,JJ_MISSILE,36,1};
  jj_step(&g,&s,false);assert(g.hazard[0].y==120&&g.hazard[0].timer==35);
  for(int i=0;i<35;++i)jj_step(&g,&s,true);
  assert(g.hazard[0].y==120&&!g.hazard[0].timer&&g.hazard[0].x==356*JJ_Q);
  jj_step(&g,&s,true);assert(g.hazard[0].x<356*JJ_Q&&g.phase==JJ_RUNNING);
  setup(&g,&s);g.hazard[0]=(jj_hazard){28*JJ_Q,90,JJ_HORIZONTAL,0,1};
  jj_step(&g,&s,false);assert(g.passed==1&&s.dodged==1);
  jj_step(&g,&s,false);assert(g.passed==1);
  setup(&g,&s);g.distance=JJ_LIMIT*JJ_Q-1;g.tick=4000000;jj_step(&g,&s,false);
  assert(g.phase==JJ_OVER&&g.reason==3&&jj_valid(&g,&s));
  s.runs=s.coins=s.dodged=UINT32_MAX;jj_new(&g,1);jj_start(&g,&s);assert(s.runs==UINT32_MAX);
}
static void hash(uint8_t *p) {
  uint32_t h=2166136261u;for(unsigned i=0;i<256;++i)h=(h^p[i])*16777619u;
  for(int i=0;i<4;++i)p[256+i]=(uint8_t)(h>>(8*i));
}
static void saves(void) {
  jj_game g,a; jj_stats s,b;setup(&g,&s);uint8_t data[JJ_SAVE_BYTES],bad[JJ_SAVE_BYTES];
  for(int i=0;i<300;++i)jj_step(&g,&s,i%80<35);
  jj_encode(&g,&s,data);assert(jj_decode(&a,&b,data,sizeof data));
  assert(!memcmp(&g,&a,sizeof g)&&!memcmp(&s,&b,sizeof s));
  for(unsigned i=0;i<sizeof data;++i) {
    memcpy(bad,data,sizeof bad);bad[i]^=1;jj_game keep=a;jj_stats book=b;
    assert(!jj_decode(&a,&b,bad,sizeof bad)&&!memcmp(&a,&keep,sizeof a)&&!memcmp(&b,&book,sizeof b));
  }
  assert(!jj_decode(&a,&b,NULL,0)&&!jj_decode(&a,&b,data,sizeof data-1));
  static const unsigned offsets[]={35,39,43,50,51,52,53,54,55,56,57,61,62,63,70,71,72,118,119};
  for(unsigned i=0;i<sizeof offsets/sizeof *offsets;++i) {
    memcpy(bad,data,sizeof bad);bad[offsets[i]]=255;hash(bad);assert(!jj_decode(&a,&b,bad,sizeof bad));
  }
  setup(&g,&s);
  for(int i=0;i<200;++i)jj_step(&g,&s,controller(&g));
  jj_game clone; jj_stats book;jj_encode(&g,&s,data);assert(jj_decode(&clone,&book,data,sizeof data));
  for(int i=0;i<1000;++i) {
    bool thrust=controller(&g);jj_step(&g,&s,thrust);jj_step(&clone,&book,thrust);
    assert(!memcmp(&g,&clone,sizeof g)&&!memcmp(&s,&book,sizeof s));
  }
  assert(g.phase==JJ_RUNNING&&jj_distance(&g)>100);
  memset(&g,0,sizeof g);memset(&s,0,sizeof s);jj_encode(&g,&s,data);assert(jj_decode(&a,&b,data,sizeof data));
  FILE *f=fopen("../tests/legacy-save.bin","rb");assert(f);assert(fread(data,1,sizeof data,f)==sizeof data);fclose(f);
  assert(!memcmp(data,"JJN1",4)&&jj_decode(&g,&s,data,sizeof data));
  assert(s.wallet==s.coins&&!s.owned&&!s.suit);jj_encode(&g,&s,data);
  assert(!memcmp(data,"JJN2",4)&&jj_decode(&a,&b,data,sizeof data)&&!memcmp(&g,&a,sizeof g)&&!memcmp(&s,&b,sizeof s));
}
static bool controller(const jj_game *g) {
  int target=120;int nearest=1000000;
  for(unsigned i=0;i<JJ_HAZARDS;++i) {
    const jj_hazard *p=&g->hazard[i];
    int w,h;jj_bounds(p->kind,&w,&h);
    if(!p->active||p->x+w*JJ_Q/2<54*JJ_Q)continue;
    if(p->x<nearest) {
      nearest=p->x;
      if(p->kind==JJ_MISSILE)target=p->timer>35?120:p->y<120?165:75;
      else if(p->kind==JJ_ROTATING)target=p->y<120?160:80;
      else if(p->kind==JJ_COLUMN)target=p->y<120?150:90;
      else if(p->kind==JJ_HORIZONTAL)target=p->y>175?150:p->y<70?95:80;
      else {
        target=p->y<120?180:60;
        for(unsigned j=0;j<JJ_HAZARDS;++j) {
          const jj_hazard *q=&g->hazard[j];
          if(j!=i&&q->active&&q->x==p->x&&q->kind==JJ_VERTICAL)target=(p->y+q->y)/2;
        }
      }
    }
  }
  if(nearest==1000000)for(unsigned i=0;i<JJ_PICKUPS;++i) {
    const jj_pickup *p=&g->pickup[i];
    if(p->active&&p->x>44*JJ_Q&&p->x<nearest)target=p->y,nearest=p->x;
  }
  return g->y+g->vy*10>target*JJ_Q;
}
static void courses(void) {
  unsigned successes=0,seen=0,shields=0,max_active=0;unsigned long steps=0;
  for(unsigned seed=1;seed<=80;++seed) {
    jj_game g;jj_stats s={0};jj_new(&g,seed);
    if(seed==1)fixture("start-save.bin",&g,&s);
    jj_start(&g,&s);
    for(unsigned i=0;i<20000&&g.phase==JJ_RUNNING&&jj_distance(&g)<3000;++i) {
      jj_step(&g,&s,controller(&g));assert(jj_valid(&g,&s));++steps;
      unsigned active=0;
      for(unsigned j=0;j<JJ_HAZARDS;++j)if(g.hazard[j].active)seen|=1u<<g.hazard[j].kind,++active;
      if(active>max_active)max_active=active;
      if(g.shield)++shields;
      if(seed==1&&jj_distance(&g)>=500&&jj_distance(&g)<=501)fixture("flight-save.bin",&g,&s);
      if(i%113==0) {
        jj_game copy; jj_stats book;uint8_t data[JJ_SAVE_BYTES];jj_encode(&g,&s,data);
        assert(jj_decode(&copy,&book,data,sizeof data)&&!memcmp(&copy,&g,sizeof g));
      }
    }
    if(jj_distance(&g)>=3000)++successes;
    else printf("Seed %u ended at %u m, cause %u\n",seed,(unsigned)jj_distance(&g),g.reason);
  }
  printf("Courses: %u/80 reached 3000 m; hazard kinds %u; shield frames %u; %lu steps\n",successes,seen,shields,steps);
  assert(successes>=70&&seen==31&&shields&&max_active==4);
  jj_game g; jj_stats s;setup(&g,&s);uint32_t r=0x452341u;
  for(unsigned i=0;i<200000;++i) {
    r^=r<<13;r^=r>>17;r^=r<<5;
    if(g.phase==JJ_OVER)jj_new(&g,r),jj_start(&g,&s);
    jj_step(&g,&s,(r&31)<16);assert(jj_valid(&g,&s));
  }
  setup(&g,&s);g.y=110*JJ_Q;g.shield=1;g.thrust=1;g.distance=5000*JJ_Q;
  for(unsigned i=0;i<JJ_HAZARDS;++i)g.hazard[i]=(jj_hazard){(int32_t)(125+i*62)*JJ_Q,(uint16_t)(i&1?140:87),(uint8_t)(i%3),i==2?30:0,1};
  for(unsigned i=0;i<JJ_PICKUPS;++i)g.pickup[i]=(jj_pickup){(int32_t)(70+i*14)*JJ_Q,(uint16_t)(i&1?60:190),(uint8_t)(i%8==0),1};
  fixture("stress-save.bin",&g,&s);
}
static void rendering(void) {
  jj_game g; jj_stats s;setup(&g,&s);g.y=120*JJ_Q;g.shield=1;
  g.hazard[0]=(jj_hazard){190*JJ_Q,90,JJ_VERTICAL,0,1};
  g.hazard[1]=(jj_hazard){356*JJ_Q,151,JJ_MISSILE,50,1};
  for(int i=0;i<7;++i)g.pickup[i]=(jj_pickup){(145+i*22)*JJ_Q,190,JJ_COIN,1};
  for(unsigned view=0;view<=JJ_BUY;++view)for(unsigned selected=0;selected<(view==JJ_BUY?8:4);++selected) {
    jj_invalidate();jj_render(&g,&s,view,selected,false,push);
    if(!selected) {if(view==JJ_TITLE)shot("title.ppm");if(view==JJ_GUIDE)shot("guide.ppm");if(view==JJ_SHOP)shot("shop.ppm");if(view==JJ_PLAY)shot("gameplay.ppm");}
    for(unsigned frame=0;frame<24;++frame) {
      if(view==JJ_PLAY)jj_step(&g,&s,controller(&g));
      jj_render(&g,&s,view,selected,false,push);memcpy(reference,screen,sizeof screen);
      jj_invalidate();jj_render(&g,&s,view,selected,false,push);assert(!memcmp(screen,reference,sizeof screen));
    }
    unsigned before=pushes;jj_render(&g,&s,view,selected,false,push);assert(pushes==before);
  }
  /* Screen boundaries, the longest exhaust plume, shield and invulnerability
   * blinking must never leave pixels on a later partial redraw. */
  setup(&g,&s);g.next=600*JJ_Q;g.shield=1;
  for(int i=0;i<240;++i) {
    jj_step(&g,&s,i<100||i>215);
    jj_render(&g,&s,JJ_PLAY,0,false,push);memcpy(reference,screen,sizeof screen);
    jj_invalidate();jj_render(&g,&s,JJ_PLAY,0,false,push);assert(!memcmp(screen,reference,sizeof screen));
  }
  jj_game frozen=g;jj_stats book=s;
  for(unsigned frame=0;frame<96;++frame) {
    jj_menu_time(frame*80);jj_render(&g,&s,JJ_TITLE,0,false,push);
    memcpy(reference,screen,sizeof screen);
    jj_invalidate();jj_render(&g,&s,JJ_TITLE,0,false,push);assert(!memcmp(screen,reference,sizeof screen));
    unsigned before=pushes;jj_render(&g,&s,JJ_TITLE,0,false,push);assert(before==pushes);
  }
  assert(!memcmp(&g,&frozen,sizeof g)&&!memcmp(&s,&book,sizeof s));
}
static void camping(void) {
  unsigned latest[4]={0};
  for(unsigned seed=1;seed<=200;++seed)for(unsigned mode=0;mode<4;++mode) {
    jj_game g;jj_stats s={0};jj_new(&g,seed);jj_start(&g,&s);g.shield=(uint8_t)(mode==3);
    for(unsigned tick=0;tick<1600&&g.phase==JJ_RUNNING;++tick)
      jj_step(&g,&s,mode==1||(mode==2&&g.y+g.vy*10>120*JJ_Q));
    assert(g.phase==JJ_OVER&&g.reason==1&&jj_distance(&g)<150);
    if(jj_distance(&g)>latest[mode])latest[mode]=jj_distance(&g);
  }
  printf("Camping (floor / ceiling / center / floor with shield): dead by %u / %u / %u / %u meters, 200 seeds each\n",latest[0],latest[1],latest[2],latest[3]);
}
static void purchases(void) {
  jj_game g;jj_stats s;setup(&g,&s);s.coins=200;s.wallet=200;
  fixture("shop-save.bin",&g,&s);
  assert(jj_price(1)==50&&jj_price(2)==120&&jj_price(3)==200);
  jj_stats before=s;s.wallet=49;before=s;assert(!jj_equip(&s,1)&&!memcmp(&s,&before,sizeof s));
  s.wallet=50;assert(jj_equip(&s,1)&&!s.wallet&&s.suit==1&&s.owned==2);
  assert(jj_equip(&s,1)&&!s.wallet&&jj_equip(&s,0)&&!s.wallet&&s.suit==0);
  before=s;assert(!jj_equip(&s,3)&&!jj_equip(&s,4)&&!memcmp(&s,&before,sizeof s));
  s.wallet=200;assert(jj_equip(&s,3)&&!s.wallet&&s.suit==3&&s.owned==10);
  uint8_t raw[JJ_SAVE_BYTES];jj_game a;jj_stats b;jj_encode(&g,&s,raw);
  assert(jj_decode(&a,&b,raw,sizeof raw)&&b.owned==10&&b.suit==3&&!b.wallet);
  /* Outfits must not change flight mechanics. */
  setup(&g,&s);a=g;b=s;b.suit=2;b.owned=4;
  for(int i=0;i<1200;++i) {
    bool thrust=controller(&g);jj_step(&g,&s,thrust);jj_step(&a,&b,thrust);
    assert(!memcmp(&g,&a,sizeof g)&&s.wallet==b.wallet&&s.coins==b.coins);
  }
}
static void rotators(void) {
  jj_hazard p={100*JJ_Q,120,JJ_ROTATING,0,1};int dx,dy;
  jj_rotor(&p,0,&dx,&dy);assert(dx==36*JJ_Q&&!dy&&jj_rotor_hit(&p,0,120*JJ_Q));
  jj_rotor(&p,32,&dx,&dy);assert(!dx&&dy==36*JJ_Q&&!jj_rotor_hit(&p,32,120*JJ_Q));
  /* A diagonal segment misses the empty corner of its enclosing square. */
  p.x=90*JJ_Q;assert(!jj_rotor_hit(&p,16,146*JJ_Q));
  assert(jj_rotor_hit(&p,16,94*JJ_Q));
  for(unsigned phase=0;phase<128;++phase) {
    p.timer=(uint8_t)phase;p.x=64*JJ_Q;
    jj_rotor(&p,37,&dx,&dy);
    assert(dx*dx+dy*dy>=35*35*JJ_Q*JJ_Q&&dx*dx+dy*dy<=37*37*JJ_Q*JJ_Q);
    assert(jj_rotor_hit(&p,37,120*JJ_Q));
    assert(!jj_rotor_hit(&p,37,60*JJ_Q));
    jj_game g;jj_stats s;setup(&g,&s);g.hazard[0]=p;g.hazard[0].x=210*JJ_Q;
    uint8_t raw[JJ_SAVE_BYTES];jj_game a;jj_stats b;jj_encode(&g,&s,raw);
    assert(jj_decode(&a,&b,raw,sizeof raw)&&!memcmp(&g,&a,sizeof g));
  }
  jj_game g;jj_stats s;setup(&g,&s);g.y=120*JJ_Q;g.hazard[0]=p;
  g.hazard[0].timer=65;jj_step(&g,&s,false);assert(g.phase==JJ_OVER&&g.reason==1);
  setup(&g,&s);g.y=120*JJ_Q;g.hazard[0]=p;g.hazard[0].timer=65;g.shield=1;
  jj_step(&g,&s,false);assert(g.phase==JJ_RUNNING&&g.invuln==80&&!g.hazard[0].active);
  setup(&g,&s);g.y=120*JJ_Q;g.hazard[0]=(jj_hazard){154*JJ_Q,90,JJ_ROTATING,70,1};
  g.hazard[1]=(jj_hazard){281*JJ_Q,150,JJ_ROTATING,23,1};
  fixture("rotor-save.bin",&g,&s);
  for(int frame=0;frame<128;++frame) {
    ++g.tick;jj_render(&g,&s,JJ_PLAY,0,false,push);memcpy(reference,screen,sizeof screen);
    jj_invalidate();jj_render(&g,&s,JJ_PLAY,0,false,push);assert(!memcmp(screen,reference,sizeof screen));
  }
  g.shield=1;
  for(unsigned i=0;i<4;++i)g.hazard[i]=(jj_hazard){(int32_t)(110+i*54)*JJ_Q,(uint16_t)(i&1?150:90),JJ_ROTATING,(uint8_t)(i*31),1};
  fixture("rotor-stress-save.bin",&g,&s);
  puts("PASS: rotating beam angles, empty corners, both directions, phase saves, shield hits and 128 redraws");
}
int main(void) {
  collisions();rotators();saves();camping();purchases();courses();rendering();puts("PASS: physics, patterns, anti-camping, shop, migration, saves, redraw equivalence");return 0;
}
