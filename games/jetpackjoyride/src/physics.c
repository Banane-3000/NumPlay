/* SPDX-License-Identifier: GPL-3.0-only */
#include "jetpack.h"
#include <string.h>
static uint32_t random(jj_game *g) {
  uint32_t x=g->rng;x^=x<<13;x^=x>>17;x^=x<<5;return g->rng=x;
}
static void increment(uint32_t *v) {if(*v<UINT32_MAX)++*v;}
uint32_t jj_distance(const jj_game *g) {return g->distance/(10*JJ_Q);}
int jj_speed(const jj_game *g) {
  unsigned extra=jj_distance(g)/4;
  return 576+(int)(extra>576?576:extra); /* 2.25 to 4.5 px per tick. */
}
void jj_bounds(unsigned kind,int *w,int *h) {
  *w=kind==JJ_ROTATING?84:kind==JJ_HORIZONTAL?58:kind==JJ_MISSILE?24:10;
  *h=kind==JJ_ROTATING?84:kind==JJ_VERTICAL?70:kind==JJ_COLUMN?100:10;
}
static int sine(unsigned phase) {
  static const uint16_t quarter[]={0,25,50,74,98,121,142,162,181,198,213,226,237,245,251,255,256};
  phase&=63;
  if(phase<=16)return quarter[phase];
  if(phase<=32)return quarter[32-phase];
  if(phase<=48)return -(int)quarter[phase-32];
  return -(int)quarter[64-phase];
}
void jj_rotor(const jj_hazard *p,uint32_t tick,int *dx,int *dy) {
  /* One revolution in 128 simulation ticks; bit 6 reverses rotation.
   * Phase lives in the existing timer byte, so saves and pools stay the same. */
  unsigned angle=(p->timer+((p->timer&64)?0u-tick/2:tick/2))&63;
  *dx=sine(angle+16)*36;*dy=sine(angle)*36;
}
static int magnitude(int v) {return v<0?-v:v;}
bool jj_rotor_hit(const jj_hazard *p,uint32_t tick,int32_t y) {
  int dx,dy;jj_rotor(p,tick,&dx,&dy);
  int x=64*JJ_Q-p->x,yy=y-p->y*JJ_Q;
  int ax=magnitude(dx),ay=magnitude(dy);
  /* Segment vs player box expanded by the beam's 3 px half-thickness.
   * Early bounds keep the cross products below signed 32-bit limits even
   * for off-screen objects. Unlike a square collider, corners remain safe. */
  if(magnitude(x)>ax+10*JJ_Q||magnitude(yy)>ay+11*JJ_Q)return false;
  return magnitude(x*dy-yy*dx)<=10*JJ_Q*ay+11*JJ_Q*ax;
}
unsigned jj_price(unsigned suit) {
  static const uint8_t prices[]={0,50,120,200};return suit<4?prices[suit]:0;
}
bool jj_equip(jj_stats *s,unsigned suit) {
  if(suit>=4)return false;
  if(suit&&!(s->owned&(1u<<suit))) {
    unsigned price=jj_price(suit);if(s->wallet<price)return false;
    s->wallet-=price;s->owned|=(uint8_t)(1u<<suit);
  }
  s->suit=(uint8_t)suit;return true;
}
void jj_new(jj_game *g,uint32_t seed) {
  memset(g,0,sizeof *g);g->y=198*JJ_Q;g->next=180*JJ_Q;
  g->rng=seed?seed:0x4A455450u;g->present=1;
}
void jj_start(jj_game *g,jj_stats *s) {
  if(!g->present||g->phase==JJ_OVER)return;
  if(!g->started)increment(&s->runs),g->started=1;
  g->phase=JJ_RUNNING;
}
static void pickup(jj_game *g,int x,int y,unsigned kind) {
  for(unsigned i=0;i<JJ_PICKUPS;++i)if(!g->pickup[i].active) {
    g->pickup[i]=(jj_pickup){x*JJ_Q,(uint16_t)y,(uint8_t)kind,1};return;
  }
}
static void hazard(jj_game *g,int x,int y,unsigned kind) {
  for(unsigned i=0;i<JJ_HAZARDS;++i)if(!g->hazard[i].active) {
    unsigned timer=kind==JJ_MISSILE?75:kind==JJ_ROTATING?random(g)&127:0;
    g->hazard[i]=(jj_hazard){x*JJ_Q,(uint16_t)y,(uint8_t)kind,(uint8_t)timer,1};return;
  }
}
static void trail(jj_game *g,int x,int y) {
  for(int i=0;i<4;++i)pickup(g,x-68+i*22,y,JJ_COIN);
}
static void spawn(jj_game *g) {
  int last=356,safe=120;
  unsigned pattern=!g->events?0:g->events==2?4:random(g)%5;
  if(g->events%5==4) {
    hazard(g,356,120,JJ_MISSILE);g->next=680*JJ_Q;
  } else {
    /* Authored patterns, not independently random hazards: each column leaves
     * a deliberate corridor. A 260 px step leaves >52 collision-free ticks at
     * top speed for a 60 px altitude change. The opening weave blocks both
     * floor camping and ceiling camping, independent of the random seed. */
    if(pattern==0) {
      bool reverse=g->events&&(random(g)&1);
      for(int i=0;i<3;++i) {
        bool low=((i&1)!=0)==reverse;
        safe=low?90:150;last=356+i*260;
        hazard(g,last,low?164:76,JJ_COLUMN);trail(g,last,safe);
      }
    } else if(pattern==1) {
      hazard(g,356,59,JJ_VERTICAL);hazard(g,356,181,JJ_VERTICAL);trail(g,356,120);
      last=616;hazard(g,last,120,JJ_HORIZONTAL);safe=random(g)&1?80:160;trail(g,last,safe);
    } else if(pattern==2) {
      bool reverse=(random(g)&1)!=0;
      for(int i=0;i<3;++i) {
        int y=i==1?130:((i==0)!=reverse?198:42);
        safe=y>175?150:95;last=356+i*260;
        hazard(g,last,y,JJ_HORIZONTAL);trail(g,last,safe);
      }
    } else if(pattern==3) {
      bool reverse=(random(g)&1)!=0;
      for(int i=0;i<2;++i) {
        int offset=((i==0)!=reverse)?-16:16;last=356+i*260;safe=120+offset;
        hazard(g,last,59+offset,JJ_VERTICAL);hazard(g,last,181+offset,JJ_VERTICAL);trail(g,last,safe);
      }
    } else {
      bool reverse=(random(g)&1)!=0;
      for(int i=0;i<2;++i) {
        bool low=((i&1)==0)!=reverse;last=356+i*300;safe=low?160:80;
        hazard(g,last,low?90:150,JJ_ROTATING);
        hazard(g,last,low?198:42,JJ_HORIZONTAL);trail(g,last,safe);
      }
    }
    if(g->events%4==3)pickup(g,last+60,safe,JJ_SHIELD);
    /* Start the next group only after the last hazard has left the screen. */
    g->next=(last-356+440+(int)(random(g)%65))*JJ_Q;
  }
  ++g->events;
}
static bool touching(int x,int y,int w,int h,int py,int radius) {
  /* Fixed-point inclusive rectangles. Small player hitbox forgives art edges.
   * The fastest missile moves <10 px/tick: narrower than either collider. */
  return x+w*JJ_Q/2>=(64-radius)*JJ_Q&&x-w*JJ_Q/2<=(64+radius)*JJ_Q&&
         y+h*JJ_Q/2>=py-8*JJ_Q&&y-h*JJ_Q/2<=py+8*JJ_Q;
}
void jj_step(jj_game *g,jj_stats *s,bool thrust) {
  if(g->phase!=JJ_RUNNING)return;
  int speed=jj_speed(g);++g->tick;g->thrust=(uint8_t)thrust;
  if(g->invuln)--g->invuln;
  if(g->flash)--g->flash;
  g->vy+=thrust?-46:38;
  if(g->vy<-1050)g->vy=-1050;
  if(g->vy>1150)g->vy=1150;
  g->y+=g->vy;
  if(g->y<42*JJ_Q)g->y=42*JJ_Q,g->vy=0;
  if(g->y>198*JJ_Q)g->y=198*JJ_Q,g->vy=0;
  g->distance+=(uint32_t)speed;g->next-=speed;
  if(g->next<=0)spawn(g);
  /* Pickups resolve first, so a shield collected on a hit can protect you. */
  for(unsigned i=0;i<JJ_PICKUPS;++i) {
    jj_pickup *p=&g->pickup[i];if(!p->active)continue;
    p->x-=speed;
    if(p->x<-16*JJ_Q)p->active=0;
    else if(touching(p->x,p->y*JJ_Q,10,10,g->y,10)) {
      p->active=0;g->flash=6;
      if(p->kind==JJ_SHIELD)g->shield=1;
      else {if(g->coins<65535)++g->coins;increment(&s->coins);increment(&s->wallet);}
    }
  }
  for(unsigned i=0;i<JJ_HAZARDS;++i) {
    jj_hazard *p=&g->hazard[i];if(!p->active)continue;
    if(p->kind==JJ_MISSILE&&p->timer) {
      if(p->timer>35)p->y=(uint16_t)(g->y/JJ_Q);
      --p->timer;continue;
    }
    int w,h;jj_bounds(p->kind,&w,&h);
    int old_right=p->x+w*JJ_Q/2;
    p->x-=speed+(p->kind==JJ_MISSILE?1152:0);
    if(old_right>55*JJ_Q&&p->x+w*JJ_Q/2<=55*JJ_Q) {
      if(g->passed<65535)++g->passed;
      increment(&s->dodged);
    }
    if(p->x<(p->kind==JJ_ROTATING?-48:-40)*JJ_Q)p->active=0;
    else if(!g->invuln&&(p->kind==JJ_ROTATING?jj_rotor_hit(p,g->tick,g->y):touching(p->x,p->y*JJ_Q,w,h,g->y,7))) {
      if(g->shield)g->shield=0,g->invuln=80,p->active=0;
      else g->phase=JJ_OVER,g->reason=(uint8_t)(p->kind==JJ_MISSILE?2:1);
    }
  }
  if(jj_distance(g)>s->best)s->best=jj_distance(g);
  if(g->distance>=JJ_LIMIT*JJ_Q)g->phase=JJ_OVER,g->reason=3;
}
bool jj_valid(const jj_game *g,const jj_stats *s) {
  if(s->best>JJ_LIMIT/10||s->suit>3||s->owned>15||s->wallet>s->coins||
     (s->suit&&!(s->owned&(1u<<s->suit)))||g->y<0||g->y>240*JJ_Q||g->vy<-1050||g->vy>1150||
     g->next<0||g->next>1100*JJ_Q||g->distance>JJ_LIMIT*JJ_Q+1200||g->tick>4500000||
     g->phase>JJ_OVER||g->present>1||g->started>1||g->thrust>1||g->shield>1||
     g->invuln>80||g->reason>3||g->flash>6||g->events>30000)return false;
  if(g->present&&(!g->rng||g->y<42*JJ_Q||g->y>198*JJ_Q))return false;
  if(g->phase!=JJ_WAIT&&(!g->present||!g->started))return false;
  for(unsigned i=0;i<JJ_HAZARDS;++i) {
    const jj_hazard *p=&g->hazard[i];
    if(p->active>1||p->kind>JJ_ROTATING||p->timer>(p->kind==JJ_ROTATING?127:75))return false;
    if(p->active&&(p->x<(p->kind==JJ_ROTATING?-48:-40)*JJ_Q||p->x>876*JJ_Q||p->y<42||p->y>198||
       (p->kind!=JJ_MISSILE&&p->kind!=JJ_ROTATING&&p->timer)))return false;
  }
  for(unsigned i=0;i<JJ_PICKUPS;++i) {
    const jj_pickup *p=&g->pickup[i];
    if(p->active>1||p->kind>JJ_SHIELD)return false;
    if(p->active&&(p->x<-16*JJ_Q||p->x>936*JJ_Q||p->y<42||p->y>198))return false;
  }
  return true;
}
