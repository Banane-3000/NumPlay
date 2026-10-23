#include "save.h"
#include <string.h>
static uint32_t crc(const uint8_t*p,unsigned n){uint32_t c=~0u;while(n--){c^=*p++;for(unsigned i=0;i<8;i++)c=(c>>1)^((c&1)?0xedb88320u:0);}return ~c;}
static void put(uint8_t*p,uint32_t v,unsigned n){while(n--){*p++=(uint8_t)v;v>>=8;}}
static uint32_t get(const uint8_t*p,unsigned n){uint32_t v=0;for(unsigned i=0;i<n;i++)v|=(uint32_t)p[i]<<(i*8);return v;}
void save_encode(const Progress*p,uint8_t*b){
    memset(b,0,SAVE_BYTES);memcpy(b,"FNF1",4);put(b+4,CHART_COUNT,2);put(b+6,p->weeks,2);b[8]=p->downscroll;b[9]=p->ghost;b[10]=p->beat;
    for(unsigned i=0;i<CHART_COUNT;i++){uint8_t*r=b+12+i*8;put(r,(uint32_t)p->record[i].score,4);put(r+4,p->record[i].completion,2);put(r+6,p->record[i].combo,2);}put(b+SAVE_BYTES-4,crc(b,SAVE_BYTES-4),4);
}
bool save_decode(Progress*p,const uint8_t*b,unsigned n){
    if(!b||n!=SAVE_BYTES||memcmp(b,"FNF1",4)||get(b+4,2)!=CHART_COUNT||get(b+SAVE_BYTES-4,4)!=crc(b,SAVE_BYTES-4)||b[8]>1||b[9]>1||b[10]>1)return false;
    for(unsigned i=0;i<CHART_COUNT;i++){const uint8_t*r=b+12+i*8;if(get(r+4,2)>10000||get(r+6,2)>charts[i].players||get(r,4)>5000000)return false;}
    memset(p,0,sizeof(*p));p->weeks=(uint16_t)get(b+6,2);p->downscroll=b[8];p->ghost=b[9];p->beat=b[10];for(unsigned i=0;i<CHART_COUNT;i++){const uint8_t*r=b+12+i*8;p->record[i]=(Record){(int32_t)get(r,4),(uint16_t)get(r+4,2),(uint16_t)get(r+6,2)};}return true;
}
unsigned save_backup(const uint8_t*b,char*out){static const char head[]="# Friday Night Funkin' save backup\n# FNF1:";static const char hex[]="0123456789abcdef";unsigned n=sizeof(head)-1;memcpy(out,head,n);for(unsigned i=0;i<SAVE_BYTES;i++){out[n++]=hex[b[i]>>4];out[n++]=hex[b[i]&15];}memcpy(out+n,"\npass\n",7);return n+7;}
static int digit(unsigned c){return c>='0'&&c<='9'?(int)c-'0':c>='a'&&c<='f'?(int)c-'a'+10:-1;}
bool save_restore(Progress*p,const uint8_t*b,unsigned n){
    static uint8_t raw[SAVE_BYTES];static const char head[]="# Friday Night Funkin' save backup\n# FNF1:";unsigned h=sizeof(head)-1;
    if(!b||n!=h+SAVE_BYTES*2+7||memcmp(b,head,h)||memcmp(b+h+SAVE_BYTES*2,"\npass\n",7))return false;
    for(unsigned i=0;i<SAVE_BYTES;i++){int a=digit(b[h+i*2]),c=digit(b[h+i*2+1]);if(a<0||c<0)return false;raw[i]=(uint8_t)(a*16+c);}return save_decode(p,raw,SAVE_BYTES);
}
