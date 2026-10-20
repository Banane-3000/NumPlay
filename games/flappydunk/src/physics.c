/* SPDX-License-Identifier: GPL-3.0-only */
#include "dunk.h"
#include <math.h>
#include <string.h>

const char *const fd_skin_names[] = {"CLASSIC","MIDNIGHT","MINT","CANDY","GOLDEN","COSMIC"};
const uint16_t fd_skin_targets[] = {0,15,35,70,120,200};
static const int8_t wave[32] = {0,6,12,18,23,27,30,31,32,31,30,27,23,18,12,6,
                               0,-6,-12,-18,-23,-27,-30,-31,-32,-31,-30,-27,-23,-18,-12,-6};
static float clamp(float v,float lo,float hi) { return v<lo?lo:v>hi?hi:v; }
static void add(uint32_t *value,uint32_t n) { *value=UINT32_MAX-*value<n?UINT32_MAX:*value+n; }
static uint32_t random32(fd_game *g) {
  uint32_t x=g->rng; x^=x<<13;x^=x>>17;x^=x<<5;return g->rng=x;
}
uint8_t fd_unlocked(const fd_stats *s) {
  uint8_t mask=(uint8_t)(s->unlocked|1);
  for(unsigned i=1;i<FD_SKINS;++i)if(s->best>=fd_skin_targets[i])mask|=(uint8_t)(1u<<i);
  return mask;
}
float fd_half_width(const fd_game *g) {
  return g->baskets<5?29.0f:g->baskets<20?27.0f:25.0f;
}
float fd_speed(const fd_game *g) {
  float n=g->baskets<80?(float)g->baskets:80.0f;
  return 1.10f+n*0.0035f;
}
static void generate(fd_game *g,fd_hoop *h,float x,float previous_y) {
  int delta=(int)(random32(g)%65)-32;
  float next_y=previous_y+delta;
  *h=(fd_hoop){x,clamp(next_y,103.0f,172.0f),0,0,0,0};
}
void fd_new(fd_game *g,uint32_t seed) {
  memset(g,0,sizeof *g);
  g->rng=seed?seed:0x6C8E9CF5u;
  g->phase=FD_WAITING;g->present=1;
  g->x=80;g->y=111;
  g->hoop[0]=(fd_hoop){191,148,0,0,0,0};
  for(unsigned i=1;i<FD_HOOPS;++i) {
    float next_x=g->hoop[i-1].x+129;
    generate(g,&g->hoop[i],next_x,g->hoop[i-1].y);
  }
}
static void finish(fd_game *g,unsigned reason) { g->phase=FD_OVER;g->reason=(uint8_t)reason; }
bool fd_flap(fd_game *g,fd_stats *s) {
  if(!g->present||g->phase>=FD_OVER)return false;
  if(!g->started) { g->started=1;add(&s->runs,1); }
  g->phase=FD_RUNNING;g->vy=-2.85f;g->flap_anim=12;
  add(&g->flaps,1);add(&s->flaps,1);return true;
}
static void burst(fd_game *g,float x,float y,bool clean) {
  /* Cosmetic particles never consume the course generator's random stream. */
  for(unsigned i=0;i<FD_PARTICLES;++i) {
    unsigned a=(i*7)&31;
    g->particle[i]=(fd_particle){x,y,wave[a]*0.045f,wave[(a+8)&31]*0.045f-0.7f,
                                (uint8_t)(22+i%9),(uint8_t)(clean?i%3:0)};
  }
}
static void score(fd_game *g,fd_stats *s,fd_hoop *h) {
  uint8_t before=fd_unlocked(s);
  bool clean=h->clean&&!h->touched;
  h->state=2;h->wobble=20;
  add(&g->baskets,1);add(&s->baskets,1);
  if(clean) {
    if(g->streak<999)++g->streak;
    if(g->streak>g->peak)g->peak=g->streak;
    if(g->streak>s->best_streak)s->best_streak=g->streak;
    add(&g->swishes,1);add(&s->swishes,1);
  } else g->streak=0;
  unsigned award=clean?(g->streak<4?g->streak+1:5):1;
  add(&g->score,award);
  if(g->score>s->best)s->best=g->score;
  s->unlocked=fd_unlocked(s);
  g->award=(uint8_t)award;g->message=clean?2:1;g->message_timer=65;
  burst(g,h->x,h->y,clean);
  g->unlocks|=(uint8_t)(fd_unlocked(s)&~before);
}
static void rim_contact(fd_game *g,fd_hoop *h,float rim_x,float speed) {
  float dx=g->x-rim_x,dy=g->y-h->y,rr=FD_BALL_R+2.2f;
  if(fabsf(dx)>=rr||fabsf(dy)>=rr)return;
  float d2=dx*dx+dy*dy;
  if(d2>=rr*rr)return;
  float distance=sqrtf(d2),nx,ny;
  if(distance<0.0001f)nx=0,ny=-1;
  else nx=dx/distance,ny=dy/distance;
  g->x+=nx*(rr-distance+0.01f);g->y+=ny*(rr-distance+0.01f);
  float relative=(g->vx+speed)*nx+g->vy*ny;
  if(relative<0) { g->vx-=relative*1.28f*nx;g->vy-=relative*1.28f*ny; }
  g->vx=clamp(g->vx,-3.0f,3.0f);g->vy=clamp(g->vy,-5.0f,5.0f);
  if(h->state!=2)h->touched=1;
  h->wobble=8;
}
void fd_step(fd_game *g,fd_stats *s) {
  if(g->phase!=FD_RUNNING)return;
  ++g->tick;
  if(g->flap_anim)--g->flap_anim;
  if(g->message_timer)--g->message_timer;
  float speed=fd_speed(g),half=fd_half_width(g);
  g->scroll+=speed/4;
  if(g->scroll>=64)g->scroll-=64;
  g->vx=(g->vx+(80-g->x)*0.006f)*0.955f;
  g->vy+=0.155f;
  g->vx=clamp(g->vx,-3.0f,3.0f);
  g->vy=clamp(g->vy,-5.0f,5.0f);
  for(unsigned i=0;i<FD_HOOPS;++i)if(g->hoop[i].wobble)--g->hoop[i].wobble;
  /* Swept plane crossing and four short substeps prevent a fast ball from
   * skipping the rim. A hoop scores only after a complete downward pass. */
  for(unsigned sub=0;sub<4&&g->phase==FD_RUNNING;++sub) {
    float old_x=g->x,old_y=g->y;
    g->x+=g->vx*0.25f;g->y+=g->vy*0.25f;
    for(unsigned i=0;i<FD_HOOPS&&g->phase==FD_RUNNING;++i) {
      fd_hoop *h=&g->hoop[i];
      float old_hx=h->x,old_hy=h->y;
      h->x-=speed*0.25f;
      float left=h->x-half,right=h->x+half;
      rim_contact(g,h,left,speed);
      rim_contact(g,h,right,speed);
      if(h->state==0&&old_y<old_hy&&g->y>=h->y&&g->vy>0) {
        float denominator=(g->y-h->y)-(old_y-old_hy);
        float t=denominator>0?(old_hy-old_y)/denominator:0;
        t=clamp(t,0.0f,1.0f);
        float x=old_x-old_hx+((g->x-h->x)-(old_x-old_hx))*t;
        if(fabsf(x)<=half-FD_BALL_R-1) {
          h->state=1;h->clean=(uint8_t)(fabsf(x)<=half-FD_BALL_R-5);
        }
      }
      if(h->state==1) {
        if(g->y-FD_BALL_R>h->y+2&&g->vy>0)score(g,s,h);
        else if(g->y+FD_BALL_R<h->y)h->state=0,h->clean=0;
      }
      if(h->state!=2&&h->x+half<g->x-FD_BALL_R-2&&g->phase==FD_RUNNING)finish(g,FD_MISSED);
    }
    if(g->phase==FD_RUNNING) {
      if(g->y+FD_BALL_R>=FD_FLOOR)
        finish(g,FD_FLOOR_HIT);
      else if(g->y-FD_BALL_R<=FD_TOP)finish(g,FD_CEILING_HIT);
    }
    /* Prevent a rare double-rim impulse from escaping the view/snapshot range. */
    g->x=clamp(g->x,20.0f,148.0f);g->y=clamp(g->y,(float)(FD_TOP+FD_BALL_R),(float)(FD_FLOOR-FD_BALL_R));
  }
  for(unsigned i=0;i<FD_HOOPS;++i) {
    fd_hoop *h=&g->hoop[i];
    if(h->x+half>=-4)continue;
    unsigned last=0;
    for(unsigned j=1;j<FD_HOOPS;++j)if(g->hoop[j].x>g->hoop[last].x)last=j;
    float next_x=g->hoop[last].x+126+(random32(g)%16);
    generate(g,h,next_x,g->hoop[last].y);
  }
  for(unsigned i=0;i<FD_PARTICLES;++i) {
    fd_particle *p=&g->particle[i];if(!p->life)continue;
    --p->life;p->x+=p->vx-speed;p->y+=p->vy;p->vy+=0.045f;
  }
}
bool fd_valid(const fd_game *g,const fd_stats *s) {
  if(s->skin>=FD_SKINS||s->unlocked>=1u<<FD_SKINS||s->best_streak>999||
     s->swishes>s->baskets||s->best_streak>s->swishes||
     !(fd_unlocked(s)&(1u<<s->skin)))return false;
  if(g->phase>FD_OVER||g->reason>FD_MISSED||
     g->present>1||g->started>1||g->streak>999||g->peak>999||g->peak<g->streak||
     g->unlocks>=1u<<FD_SKINS)return false;
  if(!g->present)return g->phase==FD_WAITING&&!g->started;
  if((g->phase==FD_WAITING)==(g->started!=0)||g->baskets>s->baskets||
     g->swishes>s->swishes||g->flaps>s->flaps||g->peak>g->swishes||
     (g->phase==FD_OVER)!=(g->reason!=FD_NO_REASON))return false;
  if(!g->rng||!isfinite(g->x)||!isfinite(g->y)||!isfinite(g->vx)||!isfinite(g->vy)||
     !isfinite(g->scroll)||g->x<20||g->x>148||g->y<FD_TOP+FD_BALL_R-0.1f||
     g->y>FD_FLOOR-FD_BALL_R+0.1f||fabsf(g->vx)>3.01f||fabsf(g->vy)>5.01f||
     g->scroll<0||g->scroll>=64||g->swishes>g->baskets)return false;
  for(unsigned i=0;i<FD_HOOPS;++i) {
    const fd_hoop *h=&g->hoop[i];
    if(!isfinite(h->x)||!isfinite(h->y)||h->x<-80||h->x>620||
       h->y<90||h->y>183||h->state>2||
       h->touched>1||h->clean>1||h->wobble>20)return false;
  }
  return true;
}
