/* SPDX-License-Identifier: GPL-3.0-only */
#include "doodle.h"
#include <string.h>
static void w16(uint8_t *p,uint32_t v) {p[0]=(uint8_t)v;p[1]=(uint8_t)(v>>8);}
static unsigned r16(const uint8_t *p) {return p[0]|(unsigned)p[1]<<8;}
static void w32(uint8_t *p,uint32_t v) {w16(p,v);w16(p+2,v>>16);}
static uint32_t r32(const uint8_t *p) {return r16(p)|(uint32_t)r16(p+2)<<16;}
static uint32_t checksum(const uint8_t *p) {
  uint32_t h=2166136261u;for(unsigned i=0;i<DJ_SAVE_BYTES-4;++i)h=(h^p[i])*16777619u;return h;
}
void dj_encode(const dj_game *g,const dj_stats *s,uint8_t out[DJ_SAVE_BYTES]) {
  memset(out,0,DJ_SAVE_BYTES);memcpy(out,"DJN3",4);
  w32(out+4,s->best);w32(out+8,s->runs);w32(out+12,s->jumps);
  w32(out+16,s->springs);w32(out+20,s->rockets);w32(out+24,s->monsters);out[28]=s->motion;
  w32(out+32,g->x);w32(out+36,g->y);w32(out+40,g->vx);w32(out+44,g->vy);
  w32(out+48,g->camera);w32(out+52,g->peak);w32(out+56,g->next_y);
  w32(out+60,g->rng);w32(out+64,g->tick);w32(out+68,g->rows);w32(out+72,g->jumps);
  w16(out+76,g->route_x);out[78]=g->phase;out[79]=g->present;out[80]=g->started;
  out[81]=g->reason;out[82]=g->boost;out[83]=g->cooldown;out[84]=g->face;out[85]=g->bounce;
  out[86]=g->route_phase;
  for(unsigned i=0;i<DJ_PLATFORMS;++i) {
    const dj_platform *p=&g->platform[i];uint8_t *d=out+96+i*12;
    w32(d,p->y);w16(d+4,p->x);d[6]=p->width;d[7]=p->type;d[8]=p->item;d[9]=p->phase;d[10]=p->active;
  }
  for(unsigned i=0;i<DJ_ENEMIES;++i) {
    const dj_enemy *e=&g->enemy[i];uint8_t *d=out+288+i*8;
    w32(d,e->y);w16(d+4,e->x);d[6]=e->phase;d[7]=e->active;
  }
  for(unsigned i=0;i<DJ_SHOTS;++i) {
    const dj_shot *b=&g->shot[i];uint8_t *d=out+312+i*10;
    w32(d,b->x);w32(d+4,b->y);d[8]=b->active;
  }
  w32(out+DJ_SAVE_BYTES-4,checksum(out));
}
bool dj_decode(dj_game *g,dj_stats *s,const uint8_t *p,size_t n) {
  if(!p||n!=DJ_SAVE_BYTES||(memcmp(p,"DJN1",4)&&memcmp(p,"DJN2",4)&&memcmp(p,"DJN3",4))||r32(p+DJ_SAVE_BYTES-4)!=checksum(p))return false;
  dj_game a;dj_stats b;memset(&a,0,sizeof a);memset(&b,0,sizeof b);
  b.best=r32(p+4);b.runs=r32(p+8);b.jumps=r32(p+12);
  b.springs=r32(p+16);b.rockets=r32(p+20);b.monsters=r32(p+24);b.motion=p[28];
  a.x=(int32_t)r32(p+32);a.y=(int32_t)r32(p+36);a.vx=(int32_t)r32(p+40);a.vy=(int32_t)r32(p+44);
  a.camera=(int32_t)r32(p+48);a.peak=(int32_t)r32(p+52);a.next_y=(int32_t)r32(p+56);
  a.rng=r32(p+60);a.tick=r32(p+64);a.rows=r32(p+68);a.jumps=r32(p+72);
  a.route_x=(uint16_t)r16(p+76);a.phase=p[78];a.present=p[79];a.started=p[80];
  a.reason=p[81];a.boost=p[82];a.cooldown=p[83];a.face=p[84];a.bounce=p[85];
  a.route_phase=p[86];
  for(unsigned i=0;i<DJ_PLATFORMS;++i) {
    const uint8_t *d=p+96+i*12;
    a.platform[i]=(dj_platform){(int32_t)r32(d),(uint16_t)r16(d+4),d[6],d[7],d[8],d[9],d[10]};
  }
  for(unsigned i=0;i<DJ_ENEMIES;++i) {
    const uint8_t *d=p+288+i*8;
    a.enemy[i]=(dj_enemy){(int32_t)r32(d),(uint16_t)r16(d+4),d[6],d[7]};
  }
  for(unsigned i=0;i<DJ_SHOTS;++i) {
    const uint8_t *d=p+312+i*10;a.shot[i]=(dj_shot){(int32_t)r32(d),(int32_t)r32(d+4),d[8]};
  }
  uint8_t previous_phase=a.route_phase;
  if(p[3]!='3') {
    if(previous_phase>12)return false;
    a.route_phase=0;
  }
  if(!dj_valid(&a,&b))return false;
  if(p[3]=='1')dj_migrate(&a);
  else if(a.present&&(a.x<DJ_LEFT*DJ_Q||a.x>DJ_RIGHT*DJ_Q))return false;
  *g=a;*s=b;return true;
}
