/* SPDX-License-Identifier: GPL-3.0-only */
#include "dunk.h"
#include <string.h>
static void wr16(uint8_t *p,uint32_t v) { p[0]=(uint8_t)v;p[1]=(uint8_t)(v>>8); }
static unsigned rd16(const uint8_t *p) { return p[0]|(unsigned)p[1]<<8; }
static void wr32(uint8_t *p,uint32_t v) { wr16(p,v);wr16(p+2,v>>16); }
static uint32_t rd32(const uint8_t *p) { return rd16(p)|(uint32_t)rd16(p+2)<<16; }
static float fixed(const uint8_t *p,float unit) { return (int16_t)rd16(p)/unit; }
static uint32_t hash(const uint8_t *p) {
  uint32_t h=2166136261u;for(unsigned i=0;i<FD_SAVE_BYTES-4;++i)h=(h^p[i])*16777619u;return h;
}
void fd_encode(const fd_game *g,const fd_stats *s,uint8_t out[FD_SAVE_BYTES]) {
  memset(out,0,FD_SAVE_BYTES);memcpy(out,"FDK2",4);
  wr32(out+4,s->best);wr32(out+8,s->baskets);wr32(out+12,s->swishes);
  wr32(out+16,s->runs);wr32(out+20,s->flaps);wr32(out+24,s->best_streak);
  out[30]=s->skin;out[31]=fd_unlocked(s);
  wr32(out+32,g->score);wr32(out+36,g->baskets);wr32(out+40,g->swishes);
  wr32(out+44,g->flaps);wr32(out+48,g->rng);wr32(out+52,g->tick);
  wr16(out+56,(int)(g->x*128));wr16(out+58,(int)(g->y*128));
  wr16(out+60,(int)(g->vx*256));wr16(out+62,(int)(g->vy*256));wr16(out+64,(int)(g->scroll*256));
  wr16(out+66,g->streak);wr16(out+68,g->peak);
  out[71]=g->phase;out[72]=g->reason;out[73]=g->present;
  out[74]=g->started;out[76]=g->unlocks;
  for(unsigned i=0;i<FD_HOOPS;++i) {
    const fd_hoop *h=&g->hoop[i];uint8_t *p=out+80+i*16;
    wr16(p,(int)(h->x*32));wr16(p+2,(int)(h->y*128));
    p[7]=h->state;p[8]=h->touched;p[9]=h->clean;p[10]=h->wobble;
  }
  wr32(out+FD_SAVE_BYTES-4,hash(out));
}
bool fd_decode(fd_game *g,fd_stats *s,const uint8_t *p,size_t length) {
  if(!p||length!=FD_SAVE_BYTES||(memcmp(p,"FDK1",4)&&memcmp(p,"FDK2",4))||rd32(p+length-4)!=hash(p))return false;
  bool legacy=p[3]=='1';
  fd_game a;fd_stats b;memset(&a,0,sizeof a);memset(&b,0,sizeof b);
  b.best=rd32(p+4);b.baskets=rd32(p+8);b.swishes=rd32(p+12);
  b.runs=rd32(p+16);b.flaps=rd32(p+20);b.best_streak=rd32(p+24);
  b.skin=p[30];b.unlocked=p[31];
  a.score=rd32(p+32);a.baskets=rd32(p+36);a.swishes=rd32(p+40);
  a.flaps=rd32(p+44);a.rng=rd32(p+48);a.tick=rd32(p+52);
  a.x=fixed(p+56,128);a.y=fixed(p+58,128);a.vx=fixed(p+60,256);a.vy=fixed(p+62,256);a.scroll=fixed(p+64,256);
  a.streak=(uint16_t)rd16(p+66);a.peak=(uint16_t)rd16(p+68);
  a.phase=p[71];a.reason=p[72];a.present=p[73];a.started=p[74];a.unlocks=p[76];
  if(legacy) {
    /* Import earned cosmetics, then discard the removed mode/progress data. */
    unsigned old_medals=rd16(p+28),earned=0;
    if(old_medals>=4096||p[70]>12||a.phase>3||a.reason>4)return false;
    for(unsigned i=0;i<12;++i)earned+=(old_medals>>i)&1;
    static const uint8_t old_targets[]={0,5,15,30,60,100};
    b.unlocked=1;
    for(unsigned i=1;i<FD_SKINS;++i)
      if(b.best>=old_targets[i]||earned>=i*2)b.unlocked|=(uint8_t)(1u<<i);
    if(a.phase==3) {
      if(!p[70]||!(old_medals&(1u<<(p[70]-1))))return false;
      a.phase=FD_OVER;a.reason=FD_MISSED;
    } else if(a.reason==4)a.reason=FD_FLOOR_HIT;
  } else if(p[28]||p[29]||p[70]||!(b.unlocked&1))return false;
  for(unsigned i=0;i<FD_HOOPS;++i) {
    const uint8_t *v=p+80+i*16;fd_hoop *h=&a.hoop[i];
    h->x=fixed(v,32);h->y=fixed(v+2,128);
    if(legacy&&(fixed(v+4,128)<103||fixed(v+4,128)>172||v[6]>31)&&a.present)return false;
    h->state=v[7];h->touched=v[8];h->clean=v[9];h->wobble=v[10];
  }
  if(!fd_valid(&a,&b))return false;
  /* A discontinued course becomes a fresh endless flight, preserving totals,
   * the personal best and every already-earned ball. Normal runs resume. */
  if(legacy&&p[70])fd_new(&a,a.rng);
  *g=a;*s=b;return true;
}
