#include "game.h"
#include "assets.h"
#include "render.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
static uint16_t frame[320*240];
static void push(int x,int y,int w,int h,const uint16_t*p){assert(x>=0&&y>=0&&x+w<=320&&y+h<=240);for(int j=0;j<h;j++)memcpy(frame+(y+j)*320+x,p+j*w,w*2);}
static void screenshot(const char*name,Game*g,Stats*s,unsigned view,unsigned selected,bool scenes){render_invalidate();render(g,s,view,selected,scenes,false,400,push);FILE*f=fopen(name,"wb");assert(f);fprintf(f,"P6\n320 240\n255\n");for(int i=0;i<320*240;i++){uint16_t c=frame[i];uint8_t p[3]={(c>>11)*255/31,((c>>5)&63)*255/63,(c&31)*255/31};fwrite(p,1,3,f);}fclose(f);}
static Stats fresh_stats(void){return (Stats){.unlocked=1,.scenes=1,.seed=1234567};}
static void legacy_save(const Stats*s,uint8_t*out){save_encode(s,out);memcpy(out,"DUN1",4);uint32_t hash=2166136261u;for(unsigned i=0;i<36;i++)hash=(hash^out[i])*16777619u;for(unsigned i=0;i<4;i++)out[36+i]=(uint8_t)(hash>>(i*8));}
int main(void){
 Stats s=fresh_stats(),decoded={0};uint8_t bytes[SAVE_BYTES];save_encode(&s,bytes);assert(save_decode(&decoded,bytes,sizeof bytes));for(unsigned i=0;i<sizeof bytes;i++){bytes[i]^=1;assert(!save_decode(&decoded,bytes,sizeof bytes));bytes[i]^=1;}assert(!save_decode(&decoded,NULL,0));
 assert(!save_decode(&decoded,bytes,SAVE_BYTES-1));Stats invalid=s;invalid.ball=26;save_encode(&invalid,bytes);assert(!save_decode(&decoded,bytes,sizeof bytes));invalid=s;invalid.ball=1;save_encode(&invalid,bytes);assert(!save_decode(&decoded,bytes,sizeof bytes));invalid=s;invalid.landscape=1;save_encode(&invalid,bytes);assert(!save_decode(&decoded,bytes,sizeof bytes));invalid=s;invalid.seed=0;save_encode(&invalid,bytes);assert(!save_decode(&decoded,bytes,sizeof bytes));
 assert(!shop_buy(&s,false,1));s.coins=50000;for(unsigned i=0;i<BALL_COUNT;i++){unsigned before=s.coins;assert(shop_buy(&s,false,i));assert(shop_owned(&s,false,i));assert(s.coins==before-ball_styles[i].price);assert(shop_buy(&s,false,i)&&s.coins==before-ball_styles[i].price);}assert(shop_buy(&s,true,0));assert(!shop_buy(&s,false,BALL_COUNT)&&!shop_buy(&s,true,1));save_encode(&s,bytes);assert(save_decode(&decoded,bytes,sizeof bytes));assert(decoded.ball==6&&decoded.landscape==0);
 uint32_t refund=0;for(unsigned i=1;i<26;i++){bool retained=false;for(unsigned j=0;j<BALL_COUNT;j++)retained|=original_ball_ids[j]==i;if(!retained)refund+=legacy_ball_prices[i];}for(unsigned i=1;i<7;i++)refund+=legacy_scene_prices[i];
 for(unsigned original=0;original<26;original++){
  Stats old={.best=987,.coins=123,.unlocked=(1u<<26)-1,.scenes=127,.runs=42,.seed=1234,.ball=original,.landscape=6};legacy_save(&old,bytes);assert(save_decode(&decoded,bytes,sizeof bytes));unsigned equipped=0;for(unsigned i=0;i<BALL_COUNT;i++)if(original_ball_ids[i]==original)equipped=i;
  assert(decoded.coins==123+refund&&decoded.best==987&&decoded.runs==42&&decoded.seed==1234&&decoded.ball==equipped&&decoded.unlocked==127&&decoded.landscape==0&&decoded.scenes==1);
  save_encode(&decoded,bytes);assert(!memcmp(bytes,"DUN2",4));Stats again;assert(save_decode(&again,bytes,sizeof bytes)&&again.coins==decoded.coins);
 }
 Stats old={.best=987,.coins=123,.unlocked=(1u<<26)-1,.scenes=127,.runs=42,.seed=1234,.ball=23,.landscape=6};legacy_save(&old,bytes);FILE*legacy=fopen("legacy-save.bin","wb");fwrite(bytes,1,sizeof bytes,legacy);fclose(legacy);
 old.coins=UINT32_MAX-1;legacy_save(&old,bytes);assert(save_decode(&decoded,bytes,sizeof bytes)&&decoded.coins==UINT32_MAX);
 Game g;game_new(&g,1234567);for(unsigned i=0;i<120;i++)game_step(&g,false);assert(fabsf(g.x-700)<.01f&&g.grounded&&game_valid(&g));float atrest=g.x;for(unsigned i=0;i<60;i++)game_step(&g,true);assert(g.x>atrest+100);
 unsigned deaths=0,score=0,coins=0,frames=0,longest=0,maxterrain=0;float maxspeed=0;
 for(unsigned seed=1;seed<=120;seed++){
  game_new(&g,seed*98765);unsigned t;
  for(t=0;t<9000&&g.phase==RUNNING;t++){
   unsigned id=game_terrain(&g,g.x);float u=terrain_u(&g.terrain[id],g.x);bool held=seed%3==0?true:seed%3==1?g.vy>0||g.vx<300:g.vy>100||(g.grounded&&u<.45f)||g.vx<200;
   game_step(&g,held);assert(game_valid(&g));if(g.vx>maxspeed)maxspeed=g.vx;++frames;
   if(seed==2&&t==220){FILE*f=fopen("fixture.bin","wb");fwrite(&g,1,sizeof g,f);fclose(f);Stats st=fresh_stats();screenshot("gameplay.ppm",&g,&st,PLAY,0,false);}
  }
  if(g.phase==CRASHED)++deaths;
  if(t>longest)longest=t;
  if(g.score>score)score=g.score;
  if(g.earned>coins)coins=g.earned;
  if(g.terrains>maxterrain)maxterrain=g.terrains;
 }
 printf("playability: deaths %u score %u coins %u frames %u longest %u terrain %u speed %.1f\n",deaths,score,coins,frames,longest,maxterrain,maxspeed);
 assert(deaths>10&&score>5&&coins>0&&maxterrain>3);
 game_new(&g,1234);g.coins[0]=(Coin){g.x,g.y,1};game_step(&g,false);assert(g.earned==1);game_finish(&s,&g);assert(s.best>=g.score);
 game_new(&g,1234);g.x+=100000;for(unsigned i=0;i<9;i++)g.terrain[i].x+=100000;for(unsigned i=0;i<COIN_COUNT;i++)g.coins[i].x+=100000;game_step(&g,false);assert(g.x<100000&&game_valid(&g));
 game_new(&g,1234);g.x=1300;g.y=300;g.zoom=1;g.camera_x=g.x-300;g.camera_y=100;g.phase=CRASHED;g.trail_scale=4.5f;g.trail_angle=-.6f;g.vx=4000;g.vy=1000;for(unsigned i=0;i<COIN_COUNT;i++)g.coins[i]=(Coin){g.camera_x+40+(i%8)*95.f,110+(i/8)*125.f,1};FILE*stress=fopen("stress.bin","wb");fwrite(&g,1,sizeof g,stress);fclose(stress);
 s=fresh_stats();game_new(&g,s.seed);screenshot("title.ppm",&g,&s,TITLE,0,false);screenshot("shop.ppm",&g,&s,SHOP,5,false);screenshot("help.ppm",&g,&s,HELP,0,false);
 FILE*f=fopen("host-report.json","w");fprintf(f,"{\"simulation_frames\":%u,\"seeds\":120,\"deaths\":%u,\"max_score\":%u,\"max_coins\":%u,\"recycled_terrains\":%u,\"longest_frames\":%u,\"game_bytes\":%u,\"stats_bytes\":%u,\"legacy_refund\":%u,\"retained_balls\":7,\"retained_original_landscape\":2}\n",frames,deaths,score,coins,maxterrain,longest,(unsigned)sizeof g,(unsigned)sizeof s,refund);fclose(f);puts("PASS: physics, seven balls, Mountains, original save migration and no duplicate refunds");return 0;
}
