/* SPDX-License-Identifier: GPL-3.0-only */
#include "game.h"
#include <string.h>
static void put32(uint8_t *p,uint32_t x){for(unsigned i=0;i<4;++i)p[i]=(uint8_t)(x>>(i*8));}
static uint32_t get32(const uint8_t *p){return p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
static uint32_t hash(const uint8_t *p){uint32_t h=2166136261u;for(unsigned i=0;i<28;++i)h=(h^p[i])*16777619u;return h;}
void dk_encode_stats(const dk_stats *s,uint8_t out[DK_SAVE_BYTES]) {
  memset(out,0,DK_SAVE_BYTES);memcpy(out,"DKA1",4);put32(out+4,s->best);put32(out+8,s->runs);
  put32(out+12,s->rescues);put32(out+16,s->best_round);put32(out+28,hash(out));
}
bool dk_decode_stats(dk_stats *s,const uint8_t *data,size_t size) {
  if(!data||size!=DK_SAVE_BYTES||memcmp(data,"DKA1",4)||get32(data+28)!=hash(data))return false;
  dk_stats next={get32(data+4),get32(data+8),get32(data+12),(uint16_t)get32(data+16)};
  if(next.best>DK_MAX_SCORE||get32(data+16)>999)return false;
  *s=next;return true;
}
