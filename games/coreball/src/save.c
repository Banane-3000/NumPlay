#include "game.h"
#include <string.h>
static void put(uint8_t*p,uint32_t v){for(int i=0;i<4;i++)p[i]=(uint8_t)(v>>(i*8));}
static uint32_t get(const uint8_t*p){return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
static uint32_t checksum(const uint8_t*p){uint32_t h=2166136261u;for(int i=0;i<SAVE_BYTES-4;i++)h=(h^p[i])*16777619u;return h;}
void save_encode(const Stats*s,uint8_t*out){memset(out,0,SAVE_BYTES);memcpy(out,"CRB1",4);put(out+4,s->current);put(out+8,s->unlocked);put(out+12,s->runs);put(out+16,s->taps);put(out+20,checksum(out));}
bool save_decode(Stats*s,const uint8_t*p,size_t n){if(!p||n!=SAVE_BYTES||memcmp(p,"CRB1",4)||get(p+20)!=checksum(p)||get(p+4)>=60||get(p+8)>60||get(p+4)>get(p+8))return false;*s=(Stats){get(p+4),get(p+8),get(p+12),get(p+16)};return true;}
