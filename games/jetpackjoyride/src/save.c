/* SPDX-License-Identifier: GPL-3.0-only */
#include "jetpack.h"
#include <string.h>
static void w16(uint8_t *p,uint32_t v) {p[0]=(uint8_t)v;p[1]=(uint8_t)(v>>8);}
static unsigned r16(const uint8_t *p) {return p[0]|(unsigned)p[1]<<8;}
static void w32(uint8_t *p,uint32_t v) {w16(p,v);w16(p+2,v>>16);}
static uint32_t r32(const uint8_t *p) {return r16(p)|(uint32_t)r16(p+2)<<16;}
static uint32_t checksum(const uint8_t *p) {
  uint32_t h=2166136261u;for(unsigned i=0;i<JJ_SAVE_BYTES-4;++i)h=(h^p[i])*16777619u;return h;
}
void jj_encode(const jj_game *g,const jj_stats *s,uint8_t out[JJ_SAVE_BYTES]) {
  memset(out,0,JJ_SAVE_BYTES);memcpy(out,"JJN2",4);
  w32(out+4,s->best);w32(out+8,s->runs);w32(out+12,s->coins);w32(out+16,s->dodged);
  w32(out+20,g->rng);w32(out+24,g->tick);w32(out+28,g->distance);
  w32(out+32,g->y);w32(out+36,g->vy);w32(out+40,g->next);
  w16(out+44,g->coins);w16(out+46,g->passed);w16(out+48,g->events);
  out[50]=g->phase;out[51]=g->present;out[52]=g->started;out[53]=g->thrust;
  out[54]=g->shield;out[55]=g->invuln;out[56]=g->reason;out[57]=g->flash;
  w32(out+58,s->wallet);out[62]=s->owned;out[63]=s->suit;
  for(unsigned i=0;i<JJ_HAZARDS;++i) {
    const jj_hazard *p=&g->hazard[i];uint8_t *d=out+64+i*12;
    w32(d,p->x);w16(d+4,p->y);d[6]=p->kind;d[7]=p->timer;d[8]=p->active;
  }
  for(unsigned i=0;i<JJ_PICKUPS;++i) {
    const jj_pickup *p=&g->pickup[i];uint8_t *d=out+112+i*8;
    w32(d,p->x);w16(d+4,p->y);d[6]=p->kind;d[7]=p->active;
  }
  w32(out+256,checksum(out));
}
bool jj_decode(jj_game *g,jj_stats *s,const uint8_t *p,size_t n) {
  if(!p||n!=JJ_SAVE_BYTES||(memcmp(p,"JJN1",4)&&memcmp(p,"JJN2",4))||r32(p+256)!=checksum(p))return false;
  jj_game a;memset(&a,0,sizeof a);jj_stats b;memset(&b,0,sizeof b);
  b.best=r32(p+4);b.runs=r32(p+8);b.coins=r32(p+12);b.dodged=r32(p+16);
  b.wallet=p[3]=='1'?b.coins:r32(p+58);b.owned=p[3]=='1'?0:p[62];b.suit=p[3]=='1'?0:p[63];
  a.rng=r32(p+20);a.tick=r32(p+24);a.distance=r32(p+28);
  a.y=(int32_t)r32(p+32);a.vy=(int32_t)r32(p+36);a.next=(int32_t)r32(p+40);
  a.coins=(uint16_t)r16(p+44);a.passed=(uint16_t)r16(p+46);a.events=(uint16_t)r16(p+48);
  a.phase=p[50];a.present=p[51];a.started=p[52];a.thrust=p[53];
  a.shield=p[54];a.invuln=p[55];a.reason=p[56];a.flash=p[57];
  for(unsigned i=0;i<JJ_HAZARDS;++i) {
    const uint8_t *d=p+64+i*12;a.hazard[i]=(jj_hazard){(int32_t)r32(d),(uint16_t)r16(d+4),d[6],d[7],d[8]};
  }
  for(unsigned i=0;i<JJ_PICKUPS;++i) {
    const uint8_t *d=p+112+i*8;a.pickup[i]=(jj_pickup){(int32_t)r32(d),(uint16_t)r16(d+4),d[6],d[7]};
  }
  if(!jj_valid(&a,&b))return false;
  *g=a;*s=b;return true;
}
