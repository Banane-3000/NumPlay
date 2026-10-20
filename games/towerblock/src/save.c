/* SPDX-License-Identifier: GPL-3.0-only */
#include "tower.h"
#include <string.h>
static void w16(uint8_t *p,uint32_t v) {p[0]=(uint8_t)v;p[1]=(uint8_t)(v>>8);}
static unsigned r16(const uint8_t *p) {return p[0]|(unsigned)p[1]<<8;}
static void w32(uint8_t *p,uint32_t v) {w16(p,v);w16(p+2,v>>16);}
static uint32_t r32(const uint8_t *p) {return r16(p)|(uint32_t)r16(p+2)<<16;}
static uint32_t checksum(const uint8_t *p) {
  uint32_t h=2166136261u;for(unsigned i=0;i<TB_SAVE_BYTES-4;++i)h=(h^p[i])*16777619u;return h;
}
void tb_encode(const tb_game *g,const tb_stats *s,uint8_t out[TB_SAVE_BYTES]) {
  memset(out,0,TB_SAVE_BYTES);memcpy(out,"TBK2",4);
  w32(out+4,s->best);w32(out+12,s->total);
  w32(out+16,s->perfect);w32(out+20,s->attempts);
  out[24]=s->palette;out[25]=s->guide;out[26]=s->motion;
  w32(out+28,g->height);w32(out+32,g->perfect);w32(out+36,g->misses);w32(out+40,g->tick);
  w32(out+44,g->cam_x);w32(out+48,g->cam_z);w32(out+52,g->offset);
  w16(out+56,g->camera);w16(out+58,g->drop);
  out[60]=g->count;out[62]=g->phase;out[63]=(uint8_t)g->direction;
  out[64]=g->started;out[65]=g->axis;w16(out+66,g->streak);w16(out+68,g->peak);
  out[70]=g->timer;out[71]=g->result;out[72]=g->message;out[73]=g->present;
  for(unsigned i=0;i<g->count&&i<TB_LAYERS;++i) {
    const tb_layer *b=&g->layer[i];uint8_t *p=out+80+i*10;
    w16(p,b->x);w16(p+2,b->z);w16(p+4,b->w);w16(p+6,b->d);p[8]=b->tone;
  }
  w32(out+TB_SAVE_BYTES-4,checksum(out));
}
bool tb_decode(tb_game *g,tb_stats *s,const uint8_t *p,size_t length) {
  if(!p||length!=TB_SAVE_BYTES||(memcmp(p,"TBK1",4)&&memcmp(p,"TBK2",4))||r32(p+TB_SAVE_BYTES-4)!=checksum(p))return false;
  bool legacy=p[3]=='1';
  if(p[61]>(legacy?1:0)||(!legacy&&r32(p+8)))return false;
  bool old_practice=legacy&&p[61]==1;
  tb_game a;tb_stats b;memset(&a,0,sizeof a);memset(&b,0,sizeof b);
  b.best=r32(p+4);b.total=r32(p+12);
  b.perfect=r32(p+16);b.attempts=r32(p+20);
  b.palette=p[24];b.guide=p[25];b.motion=p[26];
  a.height=r32(p+28);a.perfect=r32(p+32);a.misses=r32(p+36);a.tick=r32(p+40);
  a.cam_x=(int32_t)r32(p+44);a.cam_z=(int32_t)r32(p+48);a.offset=(int32_t)r32(p+52);
  a.camera=(int16_t)r16(p+56);a.drop=(int16_t)r16(p+58);
  a.count=p[60];a.phase=p[62];a.direction=(int8_t)p[63];
  a.started=p[64];a.axis=p[65];a.streak=(uint16_t)r16(p+66);a.peak=(uint16_t)r16(p+68);
  a.timer=p[70];a.result=p[71];a.message=p[72];a.present=p[73];
  if(a.count>TB_LAYERS)return false;
  for(unsigned i=0;i<a.count;++i) {
    const uint8_t *v=p+80+i*10;
    a.layer[i]=(tb_layer){(int16_t)r16(v),(int16_t)r16(v+2),(int16_t)r16(v+4),(int16_t)r16(v+6),v[8]};
  }
  /* Validate the old tower against its own record before discarding it.
   * Never promote a Practice score into the normal personal best. */
  tb_stats checked=b;
  if(old_practice) {
    if(a.phase>=TB_ENDING)return false;
    checked.best=r32(p+8);
  }
  if(!tb_valid(&a,&checked))return false;
  if(old_practice)tb_new(&a);
  *g=a;*s=b;return true;
}
