#include "game.h"
#include <math.h>
#include <string.h>
#define DT (1.0f/60.0f)
static float clamp(float x,float a,float b){return x<a?a:x>b?b:x;}
static uint32_t random32(uint32_t *s){uint32_t x=*s;x^=x<<13;x^=x>>17;x^=x<<5;return *s=x;}
bool sh_skin_open(const sh_stats *s,unsigned skin){return skin<SH_SKINS&&s->unlocked>=sh_unlocks[skin];}
void sh_finish(sh_stats *s,const sh_game *g) {
  if(!g->completed||g->level>=SH_LEVEL_COUNT)return;
  if(g->level==s->unlocked&&s->unlocked<SH_LEVEL_COUNT)++s->unlocked;
  s->current=g->level<SH_MAX_LEVEL?g->level+1:SH_MAX_LEVEL;
}
bool sh_closest(const sh_game *g,sh_vec *out) {
  float best=1e30f;bool found=false;
  for(unsigned j=0;j<g->section_count;++j){const sh_section *sc=&g->sections[j];const sh_part *p=&sh_parts[sc->part];
    for(unsigned i=0;i<p->hooks;++i){sh_vec h=sh_hooks[p->hook+i];h.x+=sc->x;
      float x=h.x-g->x,y=h.y-g->y,d=x*x+y*y;
      if(d<best)best=d,*out=h,found=true;}}
  return found;
}
void sh_hook(sh_game *g,sh_vec h) {
  float dx=g->x-h.x,dy=g->y-h.y;float r=sqrtf(dx*dx+dy*dy);
  if(r<.001f)return; // Avoid division by zero if the centre exactly meets a hook.
  g->theta=atan2f(dx,dy);g->r=r;g->omega=(cosf(g->theta)*g->vx-sinf(g->theta)*g->vy)/r*1.075f;
  g->hooked=1;g->hook_x=h.x;g->hook_y=h.y;g->pull=.2f;g->pose=SH_HOOKED2;
}
void sh_release(sh_game *g) {
  if(!g->hooked)return;
  g->vx*=1.2f;g->vy*=1.2f;g->spin=20*g->vx;g->hooked=0;g->pull=0;g->pose=SH_BALL;g->facing=0;
}
void sh_retry(sh_game *g) {
  g->x=g->spawn_x;g->y=g->spawn_y;g->vx=g->vy=0;g->theta=g->omega=g->r=g->pull=g->rotation=g->spin=0;
  g->hooked=g->completed=g->ready=0;g->pose=SH_SPAWN;g->progress=g->complete_time=0;
  g->trail_count=g->trail_cursor=0;g->facing=0;
  if(g->attempt<UINT32_MAX)++g->attempt;
  g->camera_offset=0;g->camera_x=fmaxf(0,g->x-2.0f/3);
}
void sh_new(sh_game *g,uint32_t level,uint32_t seed) {
  memset(g,0,sizeof(*g));g->level=level>SH_MAX_LEVEL?SH_MAX_LEVEL:level;g->seed=seed?seed:0x484f4f4bu;
  uint32_t rng=g->seed^(g->level*0x9e3779b9u);
  if(!rng)rng=1;
  for(unsigned i=0;i<3&&sh_courses[g->level][i]!=255;++i)
    g->sections[g->section_count++].part=sh_courses[g->level][i];
  for(unsigned i=0;i<g->section_count;++i){g->sections[i].x=g->finish;g->finish+=sh_parts[g->sections[i].part].finish;}
  const sh_part *p=&sh_parts[g->sections[0].part];g->spawn_x=p->spawn_x;g->spawn_y=p->spawn_y;
  g->background=g->level<9?p->background:(uint8_t)(random32(&rng)&1);sh_retry(g);
}
static void hit(sh_game *g) {g->pull=0;
if(!g->hooked)g->pose=SH_BALL,g->spin=20*g->vx;}
static void bumper(sh_game *g,const sh_bumper *item,float offset,bool *ignore) {
  const sh_bumper_profile *b=&sh_bumper_profiles[item->profile];
  float dx=g->x-item->x-offset,dy=g->y-item->y;
  float radius=SH_RADIUS+b->w;
  if(dx*dx+dy*dy>radius*radius)return;
  float x=dx*b->c+dy*b->s,y=-dx*b->s+dy*b->c;
  if(fabsf(x)>b->w*.5f+SH_RADIUS||fabsf(y)>b->h*.5f+SH_RADIUS)return;
  hit(g);++g->bumper_hits;
  if(g->hooked){if(*ignore)return;*ignore=true;g->theta-=g->omega/60;
    if(b->force)g->omega=b->force*(g->omega>0?-1:1)/g->r;
    else{g->omega*=-b->bounce;
    if(g->omega*g->r>3)g->omega=3/g->r;}return;}
  float vx=g->vx*b->c+g->vy*b->s,vy=-g->vx*b->s+g->vy*b->c;
  bool vertical=fabsf(y)-b->h*.5f>fabsf(x)-b->w*.5f;
  float *v=vertical?&vy:&vx,*pos=vertical?&y:&x,extent=vertical?b->h:b->w;
  float boundary=(*pos>0?1:-1)*(extent*.5f+SH_RADIUS);
  *v=b->force?b->force*(*v>0?-1:1):-*v*b->bounce;
  *v=clamp(fabsf(*v),.1f,3.0f)*(*v>0?1:-1);*pos=2*boundary-*pos;
  g->vx=vx*b->c-vy*b->s;g->vy=vx*b->s+vy*b->c;
  if(b->angled){float speed=hypotf(g->vx,g->vy);g->vx=speed*cosf(b->angle);g->vy=speed*sinf(b->angle);}
  g->x=item->x+offset+x*b->c-y*b->s;g->y=item->y+x*b->s+y*b->c;
}
static void obstacle(sh_game *g,const sh_obstacle *o,float offset,bool *ignore) {
  float dx=g->x-o->x-offset,dy=g->y-o->y;const sh_shape *shape=&sh_shapes[o->type];
  sh_vec rotation=sh_rotations[o->rotation];float cs=rotation.x,sn=rotation.y;
  float radius=shape->radius+SH_RADIUS;
  if(dx*dx+dy*dy>radius*radius)return;
  float x=dx*cs+dy*sn,y=-dx*sn+dy*cs;
  for(unsigned i=0;i<shape->count;++i){const sh_edge *e=&sh_edges[shape->first+i];
    float ax=x-e->x,ay=y-e->y,t;
    // The browser tests endpoints before the segment interior. Preserve that
    // order: a nearest-point approximation changes rebounds at wall corners.
    if(ax*ax+ay*ay<=SH_RADIUS*SH_RADIUS)t=0;
    else if((ax-e->dx)*(ax-e->dx)+(ay-e->dy)*(ay-e->dy)<=SH_RADIUS*SH_RADIUS)t=1;
    else {t=(ax*e->dx+ay*e->dy)*e->inv;if(t<0||t>1)continue;}
    float qx=e->x+e->dx*t,qy=e->y+e->dy*t,rx=x-qx,ry=y-qy;
    float d=rx*rx+ry*ry;
    if(d>SH_RADIUS*SH_RADIUS)continue;
    float nx=e->nx,ny=e->ny;
    if(t==0||t==1){float length=sqrtf(d);
    if(length>.000001f)nx=-rx/length,ny=-ry/length;}
    bool pulling=g->pull>0;hit(g);++g->wall_hits;
    if(g->hooked){*ignore=true;g->theta-=g->omega/60;
    if(pulling)g->r+=.01f;else g->omega*= -.1f;}
    else {
      x=qx-nx*(SH_RADIUS+.0001f);y=qy-ny*(SH_RADIUS+.0001f);
      float vx=g->vx*cs+g->vy*sn,vy=-g->vx*sn+g->vy*cs;
      float normal=vx*nx+vy*ny;vx-=1.1f*normal*nx;vy-=1.1f*normal*ny;
      g->vx=vx*cs-vy*sn;g->vy=vx*sn+vy*cs;
      g->x=o->x+offset+x*cs-y*sn;g->y=o->y+x*sn+y*cs;g->spin=20*g->vx;
    }return;
  }
}
void sh_step(sh_game *g,bool held) {
  if(!g->completed){if(held&&!g->held){sh_vec h;
  if(sh_closest(g,&h))sh_hook(g,h);}
    if(!held&&g->held)sh_release(g);}
  g->held=held;bool ignore=false;float dt=DT*(g->completed?.2f:1)/4;
  for(unsigned step=0;step<4;++step){
    if(g->hooked){
      if(g->pull>0){g->r-=.3f*fminf(dt,g->pull);
      if(g->r<.05f){g->r=.05f;g->pull=0;}g->pull-=dt;}
      g->omega+=-sinf(g->theta)/g->r*dt;g->theta+=g->omega*dt;
      float sn=sinf(g->theta),cs=cosf(g->theta);
      g->x=g->hook_x+g->r*sn;g->y=g->hook_y+g->r*cs;g->rotation=-g->theta;
      g->vx=g->r*g->omega*cs;g->vy=-g->r*g->omega*sn;g->facing=g->omega<=0;
      g->pose=fabsf(g->omega)<.5f?SH_HOOKED0:((g->theta<0&&g->omega<0)||(g->theta>0&&g->omega>0))?SH_HOOKED1:SH_HOOKED2;
    }else {if(!g->completed)g->vy+=dt;g->x+=g->vx*dt;g->y+=g->vy*dt;g->rotation+=g->spin*dt;}
    for(unsigned j=0;j<g->section_count;++j){const sh_section *sc=&g->sections[j];const sh_part *p=&sh_parts[sc->part];
      for(unsigned i=0;i<p->bumpers;++i)bumper(g,&sh_bumpers[p->bumper+i],sc->x,&ignore);}
    for(unsigned j=0;j<g->section_count;++j){const sh_section *sc=&g->sections[j];const sh_part *p=&sh_parts[sc->part];
      for(unsigned i=0;i<p->obstacles;++i)obstacle(g,&sh_obstacles[p->obstacle+i],sc->x,&ignore);}
  }
  if(!g->completed&&g->x>g->finish){g->completed=1;sh_release(g);g->spin=0;g->pose=SH_VICTORY;g->rotation=atan2f(g->vy,g->vx)+1.5707963268f;}
  if(!g->completed&&(g->y>1.15f||g->x<0)&&!g->hooked&&g->vy>0)sh_retry(g);
  if(g->completed){g->complete_time+=DT;
  if(g->complete_time>=2)g->ready=1;}
  else {float progress=(g->x-g->spawn_x)/(g->finish-g->spawn_x);
  if(progress>g->progress)g->progress=fminf(1,progress);}
  float direction=g->vx>.1f?.2f:g->vx<-.1f?-.2f:0;
  g->camera_offset=clamp(g->camera_offset+direction*DT,-.1f,.1f);
  g->camera_x=fmaxf(0,g->x+g->camera_offset-2.0f/3);
  if(!(g->ticks&1)){g->trail[g->trail_cursor]=(sh_vec){g->x,g->y};g->trail_cursor=(g->trail_cursor+1)%20;
  if(g->trail_count<20)++g->trail_count;}
  ++g->ticks;
}
bool sh_valid(const sh_game *g) {
  if(g->section_count<1||g->section_count>3||g->level>SH_MAX_LEVEL||g->trail_cursor>=20||g->trail_count>20)return false;
  for(unsigned i=0;i<g->section_count;++i)if(g->sections[i].part>=SH_PART_COUNT)return false;
  return isfinite(g->x)&&isfinite(g->y)&&isfinite(g->vx)&&isfinite(g->vy)&&isfinite(g->omega)&&isfinite(g->rotation)&&g->r>=0;
}
