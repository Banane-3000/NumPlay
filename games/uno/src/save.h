#ifndef UNO_SAVE_H
#define UNO_SAVE_H
#include "uno.h"
#define UNO_SAVE_BYTES 768
typedef struct { Uno game; UnoRules prefs; uint16_t played, wins; uint8_t active; } UnoSave;
static uint32_t save_crc(const uint8_t*p,unsigned n){
  uint32_t c=~0u;while(n--){c^=*p++;for(int j=0;j<8;j++)c=(c>>1)^(0xEDB88320u&-(c&1));}return ~c;
}
static void save_encode(const UnoSave*s,uint8_t*b){
  memset(b,0,UNO_SAVE_BYTES);uint8_t*p=b;
  *p++='U';*p++='N';*p++='O';*p++=2;*p++=s->active;
  *p++=s->prefs.players;*p++=s->prefs.difficulty;*p++=s->prefs.flags;p++; /* reserved */
#define PUT16(v) do {unsigned t=(v);*p++=t;*p++=t>>8;}while(0)
#define PUT32(v) do {uint32_t t=(v);for(int k=0;k<4;k++)*p++=t>>(k*8);}while(0)
  PUT16(s->played);PUT16(s->wins);
  const Uno*g=&s->game;
  *p++=g->rules.players;*p++=g->rules.difficulty;*p++=g->rules.flags;p++;
  memcpy(p,g->hands,sizeof g->hands);p+=sizeof g->hands;memcpy(p,g->count,4);p+=4;
  memcpy(p,g->deck,108);p+=108;memcpy(p,g->discard,108);p+=108;
  *p++=g->dn;*p++=g->xn;
  p+=8; /* Former v1 point totals: always zero in v2. */
  PUT16(g->round);p+=2;PUT16(g->turns);PUT32(g->rng);
#define WRITE(f) *p++=g->f
  WRITE(dealer);WRITE(turn);WRITE(color);WRITE(phase);WRITE(pending);WRITE(penalty_kind);WRITE(offender);
  WRITE(guilty);WRITE(old_color);WRITE(drawn);WRITE(candidate);WRITE(winner);WRITE(uno_player);WRITE(uno_armed);
  WRITE(stalls);WRITE(event);WRITE(actor);WRITE(card);WRITE(amount);WRITE(direction);
#undef WRITE
  p=b+UNO_SAVE_BYTES-4;PUT32(save_crc(b,UNO_SAVE_BYTES-4));
#undef PUT16
#undef PUT32
}
static bool save_decode(UnoSave*s,const uint8_t*b,unsigned n){
  if(!b||n!=UNO_SAVE_BYTES||b[0]!='U'||b[1]!='N'||b[2]!='O'||(b[3]!=1&&b[3]!=2))return false;
  unsigned at=UNO_SAVE_BYTES-4;uint32_t crc=0;for(int k=0;k<4;k++)crc|=(uint32_t)b[at+k]<<(k*8);
  if(crc!=save_crc(b,at))return false;
  UnoSave t={0};const uint8_t*p=b+4;t.active=*p++;
  t.prefs.players=*p++;t.prefs.difficulty=*p++;t.prefs.flags=*p++;p++;
  if(t.active>1||t.prefs.players<2||t.prefs.players>4||t.prefs.difficulty>2||t.prefs.flags>31)return false;
#define GET16(v) do{(v)=(uint16_t)(p[0]|p[1]<<8);p+=2;}while(0)
#define GET32(v) do{(v)=0;for(int k=0;k<4;k++)(v)|=(uint32_t)*p++<<(k*8);}while(0)
  GET16(t.played);GET16(t.wins);Uno*g=&t.game;
  g->rules.players=*p++;g->rules.difficulty=*p++;g->rules.flags=*p++;p++;
  memcpy(g->hands,p,sizeof g->hands);p+=sizeof g->hands;memcpy(g->count,p,4);p+=4;
  memcpy(g->deck,p,108);p+=108;memcpy(g->discard,p,108);p+=108;g->dn=*p++;g->xn=*p++;
  p+=8;GET16(g->round);p+=2;GET16(g->turns);GET32(g->rng);
#define READ(f) g->f=*p++
  READ(dealer);READ(turn);READ(color);READ(phase);READ(pending);READ(penalty_kind);READ(offender);
  READ(guilty);READ(old_color);READ(drawn);READ(candidate);READ(winner);READ(uno_player);READ(uno_armed);
  READ(stalls);READ(event);READ(actor);READ(card);READ(amount);READ(direction);
#undef READ
#undef GET16
#undef GET32
  /* A won v1 round now ends the game. In-progress cards and rules survive;
     old point totals and target settings are intentionally discarded. */
  if(b[3]==1&&g->phase==U_ROUND&&g->winner!=UNO_NONE){g->phase=U_MATCH;g->event=E_MATCH;}
  if(t.active&&!uno_valid(g))return false;
  *s=t;return true;
}
#endif
