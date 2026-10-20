/* SPDX-License-Identifier: GPL-3.0-only */
#include "tower.h"
#include <string.h>

static int absolute(int x) { return x<0?-x:x; }
static void add(uint32_t *v) { if(*v<UINT32_MAX)++*v; }
static void spawn(tb_game *g) {
  g->axis=(uint8_t)(g->height&1u);
  g->direction=(g->height&2u)?-1:1;
  g->offset=-g->direction*TB_TRAVEL;g->phase=TB_MOVING;g->timer=0;
}
void tb_new(tb_game *g) {
  memset(g,0,sizeof *g);g->present=1;g->count=1;
  g->layer[0]=(tb_layer){-TB_SIZE/2,-TB_SIZE/2,TB_SIZE,TB_SIZE,0};
  spawn(g);
}
void tb_start(tb_game *g,tb_stats *s) {
  if(!g->present||g->phase==TB_OVER)return;
  if(!g->started) { g->started=1;add(&s->attempts); }
}
unsigned tb_speed(const tb_game *g) {
  return g->height<140?220+(unsigned)g->height*3:640;
}
static void shard(tb_game *g,const tb_layer *b,int direction) {
  unsigned slot=0;
  for(unsigned i=0;i<TB_SHARDS;++i) {
    if(!g->shard[i].life) {slot=i;break;}
    if(g->shard[i].life<g->shard[slot].life)slot=i;
  }
  g->shard[slot]=(tb_shard){b->x,b->z,-4*TB_Q,g->height==UINT32_MAX?g->height:g->height+1,
      b->w,b->d,(int16_t)(g->axis?0:direction*100),(int16_t)(g->axis?direction*100:0),
      -90,52,b->tone};
}
static int16_t bounded_origin(int v,int size) {
  if(v<-TB_SIZE/2)v=-TB_SIZE/2;
  if(v>TB_SIZE/2-size)v=TB_SIZE/2-size;
  return (int16_t)v;
}
bool tb_place(tb_game *g,tb_stats *s) {
  if(!g->present||!g->started||g->phase!=TB_MOVING)return false;
  tb_layer b=g->layer[g->count-1],piece=b;
  int offset=g->offset,loss=absolute(offset);
  int size=g->axis?b.d:b.w;
  b.tone=piece.tone=(uint8_t)((g->height%96+1)%96);
  if(loss<=TB_SNAP) {
    if(g->streak<999)++g->streak;
    if(g->streak>g->peak)g->peak=g->streak;
    add(&g->perfect);add(&s->perfect);g->result=TB_PERFECT;
    if(g->streak%3==0&&size<TB_SIZE) {
      int grow=TB_SIZE-size<2*TB_Q?TB_SIZE-size:2*TB_Q;
      if(g->axis) { b.d=(int16_t)(b.d+grow);b.z=bounded_origin(b.z-grow/2,b.d); }
      else { b.w=(int16_t)(b.w+grow);b.x=bounded_origin(b.x-grow/2,b.w); }
      g->result=TB_GROW;
    }
  } else if(size-loss>=TB_MIN) {
    g->streak=0;g->result=TB_CUT;
    if(g->axis) {
      b.d=(int16_t)(size-loss);piece.d=(int16_t)loss;
      if(offset>0) {b.z=(int16_t)(b.z+offset);piece.z=(int16_t)(piece.z+size);}
      else piece.z=(int16_t)(piece.z+offset);
    } else {
      b.w=(int16_t)(size-loss);piece.w=(int16_t)loss;
      if(offset>0) {b.x=(int16_t)(b.x+offset);piece.x=(int16_t)(piece.x+size);}
      else piece.x=(int16_t)(piece.x+offset);
    }
    shard(g,&piece,offset>0?1:-1);
  } else {
    /* A completely missed slab ends the run. */
    if(g->axis)piece.z=(int16_t)(piece.z+offset);
    else piece.x=(int16_t)(piece.x+offset);
    shard(g,&piece,offset>0?1:-1);
    g->streak=0;add(&g->misses);g->result=TB_MISS;g->message=60;
    g->phase=TB_ENDING;g->timer=48;
    return true;
  }
  if(g->count==TB_LAYERS)memmove(g->layer,g->layer+1,(TB_LAYERS-1)*sizeof(tb_layer));
  else ++g->count;
  g->layer[g->count-1]=b;
  add(&g->height);add(&s->total);
  if(g->height>s->best)s->best=g->height;
  g->camera=g->height>8?-8*TB_Q:0;g->drop=-4*TB_Q;
  g->phase=TB_SETTLING;g->timer=16;g->message=62;
  return true;
}
static int approach(int value,int target) {
  int d=target-value;
  return absolute(d)<6?target:value+d/6;
}
void tb_step(tb_game *g,const tb_stats *s) {
  if(!g->present||!g->started||g->phase==TB_OVER)return;
  ++g->tick;
  if(g->message)--g->message;
  const tb_layer *top=&g->layer[g->count-1];
  int target_x=top->x+top->w/2,target_z=top->z+top->d/2;
  g->cam_x=s->motion?approach(g->cam_x,target_x):target_x;
  g->cam_z=s->motion?approach(g->cam_z,target_z):target_z;
  g->camera=s->motion?(int16_t)approach(g->camera,0):0;
  g->drop=s->motion?(int16_t)approach(g->drop,0):0;
  for(unsigned i=0;i<TB_SHARDS;++i) {
    tb_shard *p=&g->shard[i];if(!p->life)continue;
    --p->life;p->x+=p->vx;p->z+=p->vz;p->fall+=p->vy;p->vy+=48;
  }
  if(g->phase==TB_MOVING) {
    g->offset+=g->direction*(int)tb_speed(g);
    if(g->offset>TB_TRAVEL)g->offset=2*TB_TRAVEL-g->offset,g->direction=-1;
    else if(g->offset<-TB_TRAVEL)g->offset=-2*TB_TRAVEL-g->offset,g->direction=1;
  } else if(g->timer&&!--g->timer) {
    if(g->phase==TB_ENDING)g->phase=TB_OVER;
    else spawn(g);
  }
}
bool tb_valid(const tb_game *g,const tb_stats *s) {
  if(s->palette>=TB_PALETTES||s->guide>1||s->motion>1||s->perfect>s->total)return false;
  if(g->present>1||g->started>1||g->phase>TB_OVER||g->axis>1||
     g->result>TB_MISS||g->message>62||g->streak>999||g->peak>999||
     g->streak>g->peak||g->peak>g->perfect||g->perfect>g->height||g->height>s->total)return false;
  if(!g->present)return !g->started&&g->height==0;
  if(g->count!=(g->height<TB_LAYERS-1?g->height+1:TB_LAYERS)||
     (g->direction!=1&&g->direction!=-1)||g->offset<-TB_TRAVEL||g->offset>TB_TRAVEL||
     g->cam_x<-TB_SIZE/2||g->cam_x>TB_SIZE/2||g->cam_z<-TB_SIZE/2||g->cam_z>TB_SIZE/2||
     g->camera<-8*TB_Q||g->camera>0||g->drop<-4*TB_Q||g->drop>0||
     g->height>s->best||
     (g->height&&!g->started)||g->timer>48)return false;
  if(g->phase==TB_MOVING&&g->timer)return false;
  if((g->phase==TB_SETTLING||g->phase==TB_ENDING)&&(!g->started||!g->timer))return false;
  if((g->phase==TB_OVER||g->phase==TB_ENDING)&&g->result!=TB_MISS)return false;
  if(g->phase==TB_OVER&&g->timer)return false;
  for(unsigned i=0;i<g->count;++i) {
    const tb_layer *p=&g->layer[i];
    if(p->w<TB_MIN||p->w>TB_SIZE||p->d<TB_MIN||p->d>TB_SIZE||p->tone>=96||
       p->x<-TB_SIZE/2||p->z<-TB_SIZE/2||p->x+p->w>TB_SIZE/2||p->z+p->d>TB_SIZE/2)return false;
  }
  return true;
}
