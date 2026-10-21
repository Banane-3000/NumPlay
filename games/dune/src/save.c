#include "game.h"
#include "assets.h"
#include <string.h>
static void put(uint8_t*p,uint32_t v){for(int i=0;i<4;i++)p[i]=(uint8_t)(v>>(i*8));}
static uint32_t get(const uint8_t*p){return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
static uint32_t checksum(const uint8_t*p){uint32_t h=2166136261u;for(int i=0;i<36;i++)h=(h^p[i])*16777619u;return h;}
void save_encode(const Stats*s,uint8_t*out){
 memset(out,0,SAVE_BYTES);memcpy(out,"DUN2",4);
 put(out+4,s->best);put(out+8,s->coins);put(out+12,s->unlocked);put(out+16,s->scenes);
 put(out+20,s->runs);put(out+24,s->seed);out[28]=s->ball;out[29]=s->landscape;
 put(out+36,checksum(out));
}
bool save_decode(Stats*s,const uint8_t*p,size_t n){
 if(!p||n!=SAVE_BYTES||get(p+36)!=checksum(p))return false;
 bool legacy=memcmp(p,"DUN1",4)==0;
 if(!legacy&&memcmp(p,"DUN2",4))return false;
 Stats t={get(p+4),get(p+8),get(p+12),get(p+16),get(p+20),get(p+24),p[28],p[29]};
 if(!t.seed||!(t.unlocked&1)||!(t.scenes&1))return false;
 if(legacy){
  if(t.ball>=26||t.landscape>=7||t.unlocked>>26||t.scenes>>7||
     !(t.unlocked&(1u<<t.ball))||!(t.scenes&(1u<<t.landscape)))return false;
  uint32_t retained=0,new_mask=1,refund=0;uint8_t selected=0;
  for(unsigned i=0;i<BALL_COUNT;i++){
   unsigned original=original_ball_ids[i];retained|=1u<<original;
   if(t.unlocked&(1u<<original))new_mask|=1u<<i;
   if(t.ball==original)selected=(uint8_t)i;
  }
  /* Keep the value of removed purchases. Mountains is now free for everyone. */
  for(unsigned i=1;i<26;i++)if((t.unlocked&(1u<<i))&&!(retained&(1u<<i)))refund+=legacy_ball_prices[i];
  for(unsigned i=1;i<7;i++)if(t.scenes&(1u<<i))refund+=legacy_scene_prices[i];
  t.coins=UINT32_MAX-t.coins<refund?UINT32_MAX:t.coins+refund;
  t.unlocked=new_mask;t.ball=selected;t.scenes=1;t.landscape=0;
 }else if(t.ball>=BALL_COUNT||t.landscape||t.scenes!=1||t.unlocked>>BALL_COUNT||
           !(t.unlocked&(1u<<t.ball)))return false;
 *s=t;return true;
}
