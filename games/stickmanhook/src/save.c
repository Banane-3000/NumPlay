#include "game.h"
#include <string.h>
static void put(uint8_t *p,uint32_t v){for(unsigned i=0;i<4;++i)p[i]=(uint8_t)(v>>(8*i));}
static uint32_t get(const uint8_t *p){return p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
static uint32_t hash(const uint8_t *p){uint32_t h=2166136261u;for(unsigned i=0;i<36;++i)h=(h^p[i])*16777619u;return h;}
void sh_encode(const sh_stats *s,uint8_t out[SH_SAVE_BYTES]) {
  memset(out,0,SH_SAVE_BYTES);memcpy(out,"SHK2",4);put(out+4,s->current);put(out+8,s->unlocked);put(out+12,s->seed);put(out+16,s->runs);out[20]=s->skin;put(out+36,hash(out));
}
bool sh_decode(sh_stats *s,const uint8_t *p,size_t length) {
  if(!p||length!=SH_SAVE_BYTES||(memcmp(p,"SHK1",4)&&memcmp(p,"SHK2",4))||get(p+36)!=hash(p))return false;
  sh_stats next={get(p+4),get(p+8),get(p+12),get(p+16),p[20]};
  if(next.current>next.unlocked||!next.seed||next.skin>=SH_SKINS)return false;
  if(p[3]=='1') {
    if(next.unlocked>999999)return false;
    if(next.unlocked>SH_LEVEL_COUNT)next.unlocked=SH_LEVEL_COUNT;
    if(next.current>SH_MAX_LEVEL)next.current=SH_MAX_LEVEL;
    if(!sh_skin_open(&next,next.skin))next.skin=0;
  }else if(next.unlocked>SH_LEVEL_COUNT||next.current>SH_MAX_LEVEL||!sh_skin_open(&next,next.skin))return false;
  *s=next;return true;
}
