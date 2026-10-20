/* SPDX-License-Identifier: GPL-3.0-only */
#include "doodle.h"
#include <string.h>
static int32_t clamp(int32_t x,int32_t low,int32_t high) {
  return x<low?low:x>high?high:x;
}
static int32_t distance(int32_t a,int32_t b) {
  int32_t d=a-b;
  return d<0?-d:d;
}
static void add(uint32_t *v) {if(*v<UINT32_MAX)++*v;}
static uint32_t random32(dj_game *g) {
  uint32_t x=g->rng;x^=x<<13;x^=x>>17;x^=x<<5;return g->rng=x;
}
static int wave(uint32_t tick,unsigned phase) {
  unsigned t=(tick+phase)&127;
  return t<64?(int)t-32:95-(int)t;
}
int32_t dj_platform_x(const dj_platform *p,uint32_t tick) {
  int half=(p->width+1)/2;
  return clamp((int32_t)p->x*DJ_Q+(p->type==DJ_MOVING?wave(tick,p->phase)*128:0),half*DJ_Q,(DJ_WIDTH-half)*DJ_Q);
}
int32_t dj_enemy_x(const dj_enemy *e,uint32_t tick) {
  return clamp((int32_t)e->x*DJ_Q+wave(tick,e->phase)*96,DJ_LEFT*DJ_Q,DJ_RIGHT*DJ_Q);
}
uint32_t dj_height(const dj_game *g) {
  return g->peak>20*DJ_Q?(uint32_t)(g->peak/DJ_Q-20):0;
}
static dj_platform *slot(dj_game *g) {
  for(unsigned i=0;i<DJ_PLATFORMS;++i)
    if(!g->platform[i].active||g->platform[i].y<g->camera-32*DJ_Q)return &g->platform[i];
  return NULL;
}
static void generate(dj_game *g) {
  /* Random short/medium/long hops, not a fixed sweep. Reflect proposals at
   * the walls instead of piling them onto an edge. A brief inward bias after
   * four ledges in one half prevents prolonged one-sided climbs. */
  dj_platform *p=slot(g);
  if(!p)return;
  uint32_t r=random32(g);unsigned level=dj_height(g)/600;
  if(level>5)level=5;
  unsigned half_run=g->route_phase&7,run=(g->route_phase>>3)&7;
  bool before_right=(g->route_phase&64)!=0,right=((r>>16)&1)!=0;
  unsigned roll=r%10;
  int step=roll==0?(int)((r>>8)%9):roll<4?12+(int)((r>>8)%21):32+(int)((r>>8)%37);
  if(run>=3)right=!before_right;
  if(half_run>=4)right=g->route_x<144,step=48+(int)((r>>8)%21);
  int next=g->route_x+(right?step:-step);
  if(next<24)next=48-next;
  if(next>264)next=528-next;
  /* Imported routes can begin outside the new centre range. Keep the first
   * generated hop bounded too, without rearranging already visible ledges. */
  next=(int)clamp(next,(int)g->route_x-68,(int)g->route_x+68);
  if(next!=g->route_x)right=next>g->route_x;
  run=next==g->route_x?0:right==before_right?run+1:1;
  if(run>7)run=7;
  half_run=(next<144)==(g->route_x<144)?half_run+1:1;
  if(half_run>6)half_run=6;
  g->route_phase=(uint8_t)(half_run|(run<<3)|(right?64:0));
  g->route_x=(uint16_t)next;++g->rows;
  unsigned type=DJ_SOLID;
  if(g->rows>8&&(r>>8)%5==0)type=DJ_MOVING;
  else if(g->rows>16&&(r>>8)%7==0)type=DJ_FADING;
  unsigned item=g->rows%23==0?DJ_ROCKET:g->rows%7==0?DJ_SPRING:0;
  *p=(dj_platform){g->next_y,(uint16_t)next,(uint8_t)(58-level*3),
                    (uint8_t)type,(uint8_t)item,(uint8_t)(r>>16),1};
  if(g->rows>8&&g->rows%3==0) {
    dj_platform *bonus=slot(g);
    if(bonus) {
      int bx=next<144?next+100:next-100;
      *bonus=(dj_platform){g->next_y-13*DJ_Q,(uint16_t)bx,39,DJ_BRITTLE,0,0,1};
    }
  }
  if(g->rows>12&&g->rows%11==0) {
    for(unsigned i=0;i<DJ_ENEMIES;++i)if(!g->enemy[i].active||g->enemy[i].y<g->camera-32*DJ_Q) {
      unsigned ex=(unsigned)(next+110)%DJ_WIDTH;
      g->enemy[i]=(dj_enemy){g->next_y+20*DJ_Q,(uint16_t)ex,(uint8_t)r,1};break;
    }
  }
  g->next_y+=(int32_t)(36+(r>>24)%19)*DJ_Q;
}
void dj_new(dj_game *g,uint32_t seed) {
  memset(g,0,sizeof *g);g->rng=seed?seed:0x6D2B79F5u;
  g->x=144*DJ_Q;g->y=g->peak=20*DJ_Q;g->vy=1440;
  g->phase=DJ_WAIT;g->present=1;g->face=1;g->route_x=144;
  g->platform[0]=(dj_platform){20*DJ_Q,144,68,DJ_SOLID,0,0,1};
  g->next_y=62*DJ_Q;
  while(g->next_y<230*DJ_Q)generate(g);
}
void dj_start(dj_game *g,dj_stats *s) {
  if(!g->present||g->phase==DJ_OVER)return;
  if(!g->started)add(&s->runs),g->started=1;
  g->phase=DJ_RUNNING;
}
void dj_migrate(dj_game *g) {
  if(!g->present)return;
  int32_t bounded=clamp(g->x,DJ_LEFT*DJ_Q,DJ_RIGHT*DJ_Q);
  if(bounded!=g->x)g->x=bounded,g->vx=0;
  /* Keep an imported flight's height, items and records. Put its nearest
   * supporting route ledge under the player, then repair only the route
   * above it: an old edge-to-edge jump must not require wrapping anymore. */
  int anchor=-1;int32_t floor=INT32_MIN;
  for(unsigned i=0;i<DJ_PLATFORMS;++i) {
    dj_platform *p=&g->platform[i];
    if(p->active&&p->type!=DJ_BRITTLE&&p->y<=g->y&&p->y>floor)anchor=(int)i,floor=p->y;
  }
  int x=g->x/DJ_Q;
  if(anchor>=0)g->platform[anchor].x=(uint16_t)x;
  else floor=g->y;
  for(unsigned n=0;n<DJ_PLATFORMS;++n) {
    int best=-1;int32_t lowest=INT32_MAX;
    for(unsigned i=0;i<DJ_PLATFORMS;++i) {
      dj_platform *p=&g->platform[i];
      if(p->active&&p->type!=DJ_BRITTLE&&p->y>floor&&p->y<lowest)best=(int)i,lowest=p->y;
    }
    if(best<0)break;
    dj_platform *p=&g->platform[best];x=(int)clamp(p->x,x-68,x+68);
    p->x=(uint16_t)x;floor=p->y;
  }
  g->route_x=(uint16_t)x;g->route_phase=0;
}
bool dj_fire(dj_game *g) {
  if(g->phase!=DJ_RUNNING||g->cooldown)return false;
  for(unsigned i=0;i<DJ_SHOTS;++i)if(!g->shot[i].active) {
    g->shot[i]=(dj_shot){g->x,g->y+18*DJ_Q,1};g->cooldown=12;return true;
  }
  return false;
}
static void bounce(dj_game *g,dj_stats *s,int32_t velocity) {
  g->vy=velocity;g->bounce=9;add(&g->jumps);add(&s->jumps);
}
void dj_step(dj_game *g,dj_stats *s,int steering) {
  if(g->phase!=DJ_RUNNING)return;
  ++g->tick;
  if(g->cooldown)--g->cooldown;
  if(g->bounce)--g->bounce;
  if(steering) {
    g->vx+=steering>0?96:-96;g->face=(uint8_t)(steering>0);
    if(g->vx>672)g->vx=672;
    if(g->vx<-672)g->vx=-672;
  } else {
    g->vx=g->vx*3/4;
    if(g->vx<12&&g->vx>-12)g->vx=0;
  }
  int32_t old_x=g->x,old_y=g->y;
  g->x=clamp(g->x+g->vx,DJ_LEFT*DJ_Q,DJ_RIGHT*DJ_Q);
  if((g->x==DJ_LEFT*DJ_Q&&g->vx<0)||(g->x==DJ_RIGHT*DJ_Q&&g->vx>0))g->vx=0;
  bool powered=g->boost!=0;
  if(g->boost) {--g->boost;g->vy=1536;}
  else {
    g->vy-=47;
    if(g->vy<-2304)g->vy=-2304;
  }
  g->y+=g->vy;
  /* Descending feet cross the highest intersected platform first. Fractional
   * time-of-impact steering follows the actual movement, including wall stops.
   */
  int found=-1;int32_t highest=INT32_MIN;
  if(g->vy<=0)for(unsigned i=0;i<DJ_PLATFORMS;++i) {
    dj_platform *p=&g->platform[i];
    if(!p->active||old_y<p->y||g->y>p->y||p->y<highest)continue;
    int32_t fraction=old_y==g->y?0:(old_y-p->y)*DJ_Q/(old_y-g->y);
    int32_t hit_x=old_x+(g->x-old_x)*fraction/DJ_Q;
    int32_t before=dj_platform_x(p,g->tick-1),after=dj_platform_x(p,g->tick);
    int32_t px=before+(after-before)*fraction/DJ_Q;
    if(distance(hit_x,px)<((int32_t)p->width/2+7)*DJ_Q)found=(int)i,highest=p->y;
  }
  if(found>=0) {
    dj_platform *p=&g->platform[found];
    if(p->type==DJ_BRITTLE)p->active=0;
    else {
      g->y=p->y;bounce(g,s,1440);
      bool centre=distance(g->x,dj_platform_x(p,g->tick))<17*DJ_Q;
      if(centre&&p->item==DJ_SPRING)g->vy=2432,add(&s->springs);
      if(centre&&p->item==DJ_ROCKET)g->boost=80,g->vy=1536,add(&s->rockets),p->item=0;
      if(p->type==DJ_FADING)p->active=0;
    }
  }
  for(unsigned i=0;i<DJ_SHOTS;++i) {
    dj_shot *b=&g->shot[i];
    if(!b->active)continue;
    int32_t from=b->y;b->y+=9*DJ_Q;
    if(b->y>g->camera+245*DJ_Q)b->active=0;
    for(unsigned j=0;j<DJ_ENEMIES&&b->active;++j) {
      dj_enemy *e=&g->enemy[j];
      if(e->active&&distance(b->x,dj_enemy_x(e,g->tick))<14*DJ_Q&&from<e->y+12*DJ_Q&&b->y>e->y-10*DJ_Q)
        e->active=b->active=0,add(&s->monsters);
    }
  }
  for(unsigned i=0;i<DJ_ENEMIES;++i) {
    dj_enemy *e=&g->enemy[i];
    if(!e->active)continue;
    if(distance(g->x,dj_enemy_x(e,g->tick))<19*DJ_Q&&g->y<e->y+12*DJ_Q&&g->y+18*DJ_Q>e->y-9*DJ_Q) {
      if(powered||g->boost||(g->vy<=0&&old_y>=e->y+7*DJ_Q)) {
        e->active=0;add(&s->monsters);
        if(!powered&&!g->boost)g->y=e->y+12*DJ_Q,bounce(g,s,1690);
      } else g->phase=DJ_OVER,g->reason=DJ_HIT;
    }
    if(e->y<g->camera-32*DJ_Q)e->active=0;
  }
  if(g->y>DJ_LIMIT*DJ_Q)g->y=DJ_LIMIT*DJ_Q,g->vy=-47,g->boost=0;
  if(g->y>g->peak)g->peak=g->y;
  if(g->y-g->camera>124*DJ_Q)g->camera=g->y-124*DJ_Q;
  uint32_t height=dj_height(g);
  if(height>s->best)s->best=height;
  if(g->y<g->camera-24*DJ_Q)g->phase=DJ_OVER,g->reason=DJ_FELL;
  while(g->next_y<g->camera+232*DJ_Q) {
    int32_t before=g->next_y;generate(g);
    if(before==g->next_y)break;
  }
}
bool dj_valid(const dj_game *g,const dj_stats *s) {
  if(s->motion>1||s->best>DJ_LIMIT||g->present>1)return false;
  if(!g->present)return true;
  if(!g->rng||g->phase>DJ_OVER||g->started>1||g->reason>DJ_HIT||g->face>1||
     g->boost>80||g->cooldown>12||g->bounce>9||g->route_x>=DJ_WIDTH||g->route_phase>127||
     (g->route_phase&&(!(g->route_phase&7)||(g->route_phase&7)>6))||
     g->x<0||g->x>=DJ_WIDTH*DJ_Q||g->vx<-672||g->vx>672||g->vy<-2304||g->vy>2432||
     g->camera<0||g->camera>DJ_LIMIT*DJ_Q||g->peak<20*DJ_Q||g->peak>DJ_LIMIT*DJ_Q||
     g->y<g->camera-40*DJ_Q||g->y>g->camera+250*DJ_Q||g->peak<g->y||
     g->next_y<g->camera||g->next_y>g->camera+300*DJ_Q||
     g->rows>DJ_LIMIT/36+20||(!g->started&&g->phase!=DJ_WAIT)||
     (g->phase==DJ_OVER&&!g->reason)||(g->phase!=DJ_OVER&&g->reason))return false;
  unsigned live=0;
  for(unsigned i=0;i<DJ_PLATFORMS;++i) {
    const dj_platform *p=&g->platform[i];
    if(p->active>1)return false;
    if(!p->active)continue;
    ++live;
    if(p->x>=DJ_WIDTH||p->width<26||p->width>68||p->type>DJ_FADING||p->item>DJ_ROCKET||
       p->y<0||p->y>g->camera+300*DJ_Q)return false;
  }
  if(!live)return false;
  for(unsigned i=0;i<DJ_ENEMIES;++i) {
    const dj_enemy *e=&g->enemy[i];
    if(e->active>1)return false;
    if(e->active&&(e->x>=DJ_WIDTH||e->y<0||e->y>g->camera+330*DJ_Q))return false;
  }
  for(unsigned i=0;i<DJ_SHOTS;++i) {
    const dj_shot *b=&g->shot[i];
    if(b->active>1)return false;
    if(b->active&&(b->x<0||b->x>=DJ_WIDTH*DJ_Q||b->y<g->camera-40*DJ_Q||b->y>g->camera+260*DJ_Q))return false;
  }
  return true;
}
