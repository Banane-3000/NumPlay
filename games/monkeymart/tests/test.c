#include "game.h"
#include "render.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <stddef.h>
#define BIT(n) (UINT64_C(1)<<(n))
static Game g;
extern uint32_t production_counts[15];
static uint16_t framebuffer[320*240];
static void unlock_all(unsigned market){if(market){g.p.mart[0].active|=BIT(31);assert(game_market(&g,1));}g.p.coins=1000000;const MartDef*m=&marts[market];for(unsigned round=0;round<100;round++){bool changed=false;for(unsigned i=0;i<m->count;i++)if(g.offered&BIT(i)){while(g.offered&BIT(i))assert(game_buy(&g,i));changed=true;}if(!changed)break;}assert(g.p.mart[market].progress==m->order_count);for(unsigned i=0;i<m->count;i++){if(!(g.p.mart[market].active&BIT(i)))fprintf(stderr,"MART%u missing%u offered%llu\n",market,i+1,(unsigned long long)g.offered);assert(g.p.mart[market].active&BIT(i));}}
static void save_test(void){uint8_t bytes[SAVE_BYTES];Progress out;game_snapshot(&g);save_encode(&g.p,bytes);assert(save_decode(&out,bytes,sizeof(bytes)));assert(!memcmp(&out,&g.p,sizeof(out)));for(unsigned i=0;i<SAVE_BYTES;i++){bytes[i]^=1;assert(!save_decode(&out,bytes,sizeof(bytes)));bytes[i]^=1;}assert(!save_decode(&out,bytes,sizeof(bytes)-1));assert(!save_decode(&out,NULL,0));}
static void push(int x,int y,int w,int h,const uint16_t*p){assert(x>=0&&y>=0&&x+w<=320&&y+h<=240);assert(p);for(int j=0;j<h;j++)memcpy(&framebuffer[(y+j)*320+x],p+j*w,w*2);}
static void screenshot(const char*name,unsigned view,unsigned selected){render_invalidate();render(&g,view,selected,1,false,push);FILE*f=fopen(name,"wb");assert(f);fprintf(f,"P6\n320 240\n255\n");for(unsigned i=0;i<320*240;i++){uint16_t p=framebuffer[i];uint8_t rgb[3]={(uint8_t)(((p>>11)*255)/31),(uint8_t)((((p>>5)&63)*255)/63),(uint8_t)(((p&31)*255)/31)};fwrite(rgb,1,3,f);}fclose(f);}
int main(void){game_new(&g,123);screenshot("title.ppm",TITLE,0);screenshot("new-game.ppm",PLAY,0);assert(g.p.coins==35);assert(g.offered==BIT(6));assert(!game_market(&g,1));
 /* The starting balance must build the till, first shelf and first crop. */
 for(unsigned round=0;round<3;round++){unsigned n=marts[0].order[round];while(g.offered&BIT(n))assert(game_buy(&g,n));}
 assert(g.p.coins==0);assert(g.p.mart[0].active&BIT(0));
 /* Real harvest -> shelf -> customer -> till -> wallet, without a money grant. */
 const PlaceDef*crop=&marts[0].places[0];g.actors[0].x=crop->work.x;g.actors[0].y=crop->work.y;for(unsigned i=0;i<50;i++)game_step(&g,0,0);assert(g.actors[0].count==3);
 const PlaceDef*shelf=&marts[0].places[4];g.actors[0].x=shelf->work.x;g.actors[0].y=shelf->work.y-35;for(unsigned i=0;i<100;i++)game_step(&g,0,0);assert(g.actors[0].count==0);
 const PlaceDef*till=&marts[0].places[6];g.actors[0].x=till->pos.x;g.actors[0].y=till->pos.y+120;for(unsigned i=0;i<4000;i++)game_step(&g,0,0);
 if(!g.p.coins){for(unsigned i=8;i<14;i++){Actor*a=&g.actors[i];fprintf(stderr,"buyer%u live%u phase%u pos%.1f %.1f goal%.1f %.1f target%u nav%d wish1%u bill%u failures%u\n",i,a->live,a->phase,a->x,a->y,a->gx,a->gy,a->target,a->nav,a->wish[1],a->bill,a->failures);}fprintf(stderr,"stock %u cash %lu\n",g.p.mart[0].stock[4],(unsigned long)g.p.mart[0].cash);}
 assert(g.p.coins>0);assert(g.p.total_sales>0);save_test();
 for(unsigned m=0;m<2;m++){unlock_all(m);for(unsigned u=0;u<marts[m].upgrade_count;u++)for(unsigned k=0;k<3;k++)while(game_upgrade(&g,u,k)){}save_test();
  /* A player wakes/repairs at the visible animal or machine, independently of
     the remote output tray. This also verifies all original repair locations. */
  for(unsigned n=0;n<marts[m].count;n++)if(marts[m].places[n].inputs[0]!=255){Point p=game_repair_position(&g,n);g.p.mart[m].stamina[n]=0;g.actors[0].x=p.x;g.actors[0].y=p.y;g.actors[0].wait=0;game_step(&g,0,0);assert(g.p.mart[m].stamina[n]>0);}
  /* All production chains can complete, including the two-input chocolate bar. */
  for(unsigned n=0;n<marts[m].count;n++){const PlaceDef*b=&marts[m].places[n];if(b->inputs[0]==255)continue;g.p.mart[m].stock[n]=0;g.p.mart[m].stamina[n]=8;g.p.mart[m].stock[b->inputs[0]]=4;if(b->inputs[1]!=255)g.p.mart[m].stock[b->inputs[1]]=4;for(unsigned t=0;t<721;t++)game_step(&g,0,0);bool produced=g.p.mart[m].stock[n]>0;for(unsigned a=1;a<ACTOR_COUNT;a++)for(unsigned c=0;c<g.actors[a].count;c++)if(g.actors[a].carry[c]==b->food)produced=true;assert(produced);}
  memset(production_counts,0,15*sizeof(uint32_t));
  for(unsigned i=0;i<36000;i++){int dx=(int)((i/600)%3)-1,dy=(int)((i/900)%3)-1;game_step(&g,dx,dy);assert(isfinite(g.actors[0].x)&&isfinite(g.actors[0].y));if(i%1800==0)save_test();}
  for(unsigned f=m?7:1;f<=(m?14:6);f++){fprintf(stderr,"mart%u food%u produced%lu\n",m+1,f,(unsigned long)production_counts[f]);if(!production_counts[f]){for(unsigned k=1;k<=marts[m].worker_count;k++){Actor*a=&g.actors[k];fprintf(stderr,"w%u xy%.0f %.0f goal%.0f %.0f dest%u target%u carry%u f%u stamina%u phase%u fails%u nav%d\n",k,a->x,a->y,a->gx,a->gy,a->destination,a->target,a->count,a->carry[0],a->stamina,a->phase,a->failures,a->nav);}for(unsigned n=0;n<marts[m].count;n++)fprintf(stderr,"%u:%u ",n+1,g.p.mart[m].stock[n]);fputc('\n',stderr);}assert(production_counts[f]>0);}
  for(unsigned view=0;view<=SAVE_ERROR;view++)render(&g,view,0,1,false,push);
  g.camera_x=g.actors[0].x=g.p.market?300:-400;g.camera_y=g.actors[0].y=g.p.market?400:150;
  screenshot(m?"mart2.ppm":"mart1.ppm",PLAY,0);FILE*f=fopen(m?"mart2.bin":"mart1.bin","wb");assert(f);fwrite(&g,1,sizeof(g),f);fclose(f);game_snapshot(&g);uint8_t save[SAVE_BYTES];save_encode(&g.p,save);f=fopen(m?"mart2.sav":"mart1.sav","wb");assert(f);fwrite(save,1,SAVE_BYTES,f);fclose(f);
 }
 for(unsigned h=0;h<21;h++){g.p.tokens=255;assert(game_hat(&g,h));}assert((g.p.hats&0x1fffff)==0x1fffff);save_test();
 assert(game_reward(&g,0));assert(!game_reward(&g,1));for(unsigned i=0;i<40*TICKS;i++)game_step(&g,0,0);assert(game_reward(&g,1));
 screenshot("upgrades.ppm",UPGRADES,9);screenshot("hats.ppm",HATS,19);screenshot("markets.ppm",MARKETS,1);screenshot("help.ppm",HELP,0);screenshot("boosts.ppm",BOOSTS,0);
 FILE*f=fopen("host-report.json","w");assert(f);fprintf(f,"{\"game_bytes\":%u,\"progress_bytes\":%u,\"simulation_ticks\":%u,\"markets\":2,\"places\":75,\"hats\":21,\"coins_offset\":%u,\"sales_offset\":%u,\"tick_offset\":%u}\n",(unsigned)sizeof(g),(unsigned)sizeof(g.p),g.tick,(unsigned)offsetof(Progress,coins),(unsigned)offsetof(Progress,total_sales),(unsigned)offsetof(Game,tick));fclose(f);
 printf("Both marts complete; all 75 places, production chains, upgrades, hats, economy, saves and rendering passed. RAM Game=%zu Progress=%zu\n",sizeof(Game),sizeof(Progress));return 0;
}
