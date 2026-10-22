#include "game.h"
#include <string.h>
static uint32_t crc32(const uint8_t*p,unsigned n){uint32_t c=~0u;for(unsigned i=0;i<n;i++){c^=p[i];for(unsigned k=0;k<8;k++)c=(c>>1)^(0xedb88320u&-(int32_t)(c&1));}return ~c;}
static void put(uint8_t**p,uint32_t v,unsigned n){for(unsigned i=0;i<n;i++){*(*p)++=(uint8_t)v;v>>=8;}}
static uint32_t get(const uint8_t**p,unsigned n){uint32_t v=0;for(unsigned i=0;i<n;i++)v|=(uint32_t)*(*p)++<<(8*i);return v;}
void save_encode(const Progress*s,uint8_t out[SAVE_BYTES]){memset(out,0,SAVE_BYTES);memcpy(out,"MMR1",4);uint8_t*p=out+4;
 put(&p,s->coins,4);put(&p,s->hats,4);put(&p,s->rng,4);put(&p,s->seconds,4);put(&p,s->total_sales,4);put(&p,s->market,1);put(&p,s->hat,1);put(&p,(uint16_t)s->x,2);put(&p,(uint16_t)s->y,2);put(&p,s->count,1);memcpy(p,s->carry,CARRY_MAX);p+=CARRY_MAX;put(&p,s->tokens,1);put(&p,s->reward_cooldown,2);for(unsigned k=0;k<3;k++)put(&p,s->boosts[k],2);
 for(unsigned m=0;m<2;m++){const MartSave*v=&s->mart[m];put(&p,(uint32_t)v->active,4);put(&p,(uint32_t)(v->active>>32),4);put(&p,v->cash,4);put(&p,v->progress,1);put(&p,v->started,1);
  for(unsigned n=0;n<MAX_PLACES;n++){put(&p,v->paid[n],2);put(&p,v->production[n],2);put(&p,v->stock[n],1);put(&p,v->stamina[n],1);}
  for(unsigned u=0;u<12;u++)for(unsigned k=0;k<3;k++)put(&p,v->upgrade[u][k],1);
  for(unsigned i=0;i<7;i++)put(&p,v->worker_stamina[i],1);
 }p=out+SAVE_BYTES-4;put(&p,crc32(out,SAVE_BYTES-4),4);
}
bool save_decode(Progress*out,const uint8_t*data,uint32_t size){if(!data||size!=SAVE_BYTES||memcmp(data,"MMR1",4))return false;const uint8_t*tail=data+SAVE_BYTES-4;if(get(&tail,4)!=crc32(data,SAVE_BYTES-4))return false;Progress s={0};const uint8_t*p=data+4;
 s.coins=get(&p,4);s.hats=get(&p,4);s.rng=get(&p,4);s.seconds=get(&p,4);s.total_sales=get(&p,4);s.market=(uint8_t)get(&p,1);s.hat=(uint8_t)get(&p,1);s.x=(int16_t)get(&p,2);s.y=(int16_t)get(&p,2);s.count=(uint8_t)get(&p,1);memcpy(s.carry,p,CARRY_MAX);p+=CARRY_MAX;s.tokens=(uint8_t)get(&p,1);s.reward_cooldown=(uint16_t)get(&p,2);for(unsigned i=0;i<3;i++)s.boosts[i]=(uint16_t)get(&p,2);
 if(s.market>=2||s.hat>=21||s.count>CARRY_MAX||!s.rng||(s.hats&~0x1fffffu)||!(s.hats&1)||!(s.hats&(1u<<s.hat))||s.reward_cooldown>40)return false;
 for(unsigned i=0;i<s.count;i++)if(s.carry[i]<1||s.carry[i]>FOOD_COUNT)return false;
 for(unsigned i=0;i<3;i++)if(s.boosts[i]>120)return false;
 for(unsigned m=0;m<2;m++){MartSave*v=&s.mart[m];const MartDef*def=&marts[m];v->active=get(&p,4);v->active|=(uint64_t)get(&p,4)<<32;v->cash=get(&p,4);v->progress=(uint8_t)get(&p,1);v->started=(uint8_t)get(&p,1);if(v->active>>def->count||v->progress>def->order_count||v->started>1)return false;
  for(unsigned n=0;n<MAX_PLACES;n++){v->paid[n]=(uint16_t)get(&p,2);v->production[n]=(uint16_t)get(&p,2);v->stock[n]=(uint8_t)get(&p,1);v->stamina[n]=(uint8_t)get(&p,1);if(v->stock[n]>32||v->stamina[n]>28||v->production[n]>12*TICKS||(n<def->count&&v->paid[n]>def->places[n].price))return false;}
  for(unsigned u=0;u<12;u++)for(unsigned k=0;k<3;k++){v->upgrade[u][k]=(uint8_t)get(&p,1);if(v->upgrade[u][k]>(u<def->upgrade_count?def->upgrade_max[u][k]:0))return false;}
  for(unsigned i=0;i<7;i++){v->worker_stamina[i]=(uint8_t)get(&p,1);if(v->worker_stamina[i]>12)return false;}
 }
 const MartDef*m=&marts[s.market];if(!s.mart[0].started||!s.mart[s.market].started||s.x<m->minx||s.x>m->maxx||s.y<m->miny||s.y>m->maxy||(s.market==1&&!(s.mart[0].active&(UINT64_C(1)<<31))))return false;
 *out=s;return true;
}
