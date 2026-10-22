#include "game.h"
#include "render.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
static Game g;
static uint16_t screen[320*240];
static void push(int x,int y,int w,int h,const uint16_t*p){assert(x>=0&&y>=0&&x+w<=320&&y+h<=240);for(int j=0;j<h;j++)memcpy(screen+(y+j)*320+x,p+j*w,w*2);}
static void capture(const char*name,unsigned view,unsigned selected){render_invalidate();render(&g,view,selected,false,push);char path[100];snprintf(path,sizeof(path),"%s.ppm",name);FILE*f=fopen(path,"wb");assert(f);fprintf(f,"P6\n320 240\n255\n");for(unsigned i=0;i<320*240;i++){unsigned c=screen[i];unsigned char rgb[3]={(c>>11)*255/31,((c>>5)&63)*255/63,(c&31)*255/31};fwrite(rgb,1,3,f);}fclose(f);}
static void fixture(unsigned tile){game_start(&g,0,false);g.width=g.height=9;memset(g.tiles,3,sizeof(g.tiles));memset(g.actors,0,sizeof(g.actors));for(int y=1;y<8;y++)for(int x=1;x<8;x++)g.tiles[y*9+x]=0;g.x=4*Q;g.y=4*Q;g.tiles[4*9+6]=tile;}
static void settle(int dir,unsigned ticks){game_step(&g,dir,false);for(unsigned i=1;i<ticks;i++)game_step(&g,-1,false);}
int main(void){game_new(&g,1234);assert(g.p.unlocked==1);capture("title",TITLE,0);capture("map",STAGES,0);game_start(&g,0,false);capture("stage1",PLAY,0);printf("Start %d %d dimensions %u %u\n",g.x/Q,g.y/Q,g.width,g.height);assert(g.x==15*Q&&g.y==33*Q);settle(1,100);assert(g.x==18*Q&&g.y==33*Q&&!g.dx&&!g.dy);settle(2,100);assert(g.y==28*Q);assert(g.stars==0);settle(0,100);assert(g.stars==1);capture("stage1-star",PLAY,0);
 fixture(0);settle(1,60);assert(g.x==7*Q&&g.y==4*Q&&!g.dx);fixture(8);settle(1,60);assert(g.dead);fixture(8);g.p.shields=1;game_step(&g,1,true);settle(-1,60);assert(!g.dead&&!g.p.shields&&g.invincible);
 fixture(98);g.tiles[4*9+4]=98;settle(-1,100);assert(g.dead);
 fixture(7);settle(1,60);assert(g.tiles[4*9+6]==0&&!g.dead);fixture(121);settle(1,200);assert(g.tiles[4*9+6]==121);
 fixture(4);settle(1,60);assert(g.stars==1);fixture(2);settle(1,60);assert(g.won);g.total_dots=g.dots;game_finish(&g);unsigned coins=g.p.coins;game_finish(&g);assert(g.p.coins==coins);
 fixture(49);g.tiles[2*9+2]=61;settle(1,10);assert(g.portal_cooldown&&g.dx==0&&g.dy);
 fixture(145);settle(1,60);assert(g.bonus);fixture(146);settle(1,60);assert(g.freeze);fixture(147);settle(1,60);assert(g.magnet);fixture(148);settle(1,60);assert(g.boost);
 uint8_t save[SAVE_BYTES],bad[SAVE_BYTES];Progress p;save_encode(&g.p,save);assert(save_decode(&p,save,sizeof(save)));assert(!memcmp(&p,&g.p,sizeof(p)));for(unsigned i=0;i<SAVE_BYTES;i++){memcpy(bad,save,sizeof(bad));bad[i]^=0x40;assert(!save_decode(&p,bad,sizeof(bad)));}assert(!save_decode(&p,NULL,0));assert(!save_decode(&p,save,SAVE_BYTES-1));
 g.p.coins=100000;assert(game_shop(&g,0));for(unsigned i=1;i<=4;i++)for(unsigned j=0;j<9;j++)assert(game_shop(&g,i));for(unsigned i=1;i<=4;i++)assert(!game_shop(&g,i));assert(!game_shop(&g,99));assert(!game_mask(&g,15));g.p.xp=20000000;assert(game_mask(&g,15));
 for(unsigned type=0;type<8;type++){g.p.mission_counts[type]=mission_goals[type][0];unsigned before=g.p.coins;assert(game_mission_claim(&g,type));assert(g.p.coins==before+mission_rewards[0]);assert(!game_mission_claim(&g,type));}assert(!game_mission_claim(&g,8));
 g.p.coins=10000;assert(game_chest(&g));assert(!g.p.free_chest&&g.p.next_chest==g.p.playtime+14400);for(unsigned i=0;i<10;i++)assert(game_chest(&g));g.p.playtime=g.p.next_chest;g.p.coins=0;assert(game_chest(&g));save_encode(&g.p,save);assert(save_decode(&p,save,sizeof(save))&&!memcmp(&p,&g.p,sizeof(p)));capture("missions",MISSIONS,2);g.combo=0;capture("chest",CHEST,0);
 unsigned maps_ok=0,stages_ok=0;uint8_t cells[CELLS];for(unsigned m=0;m<717;m++){int n=map_decode(m,cells,sizeof(cells));assert(n==maps[m].w*maps[m].h);++maps_ok;if(m<200){assert(game_start(&g,m,false));unsigned start=0,exit=0,stars=0;for(int i=0;i<n;i++){start+=cells[i]==1;exit+=cells[i]==2;stars+=cells[i]==4;}assert(start==1&&exit==1&&stars>=3);++stages_ok;}}
 unsigned random=123,ticks=0;for(unsigned m=0;m<200;m++){game_start(&g,m,false);g.shield=65000;for(unsigned t=0;t<1500;t++){++ticks;random=random*1664525+1013904223;game_step(&g,(t%25==0)?(int)((random>>28)%4):-1,false);assert(g.x>=0&&g.y>=0&&g.x<g.width*Q&&g.y<g.height*Q);if(g.dead||g.won)break;}}
 game_start(&g,0,true);g.invincible=65000;for(unsigned t=0;t<12000;t++){if(t%70==0){g.y=30*Q;g.x=6*Q;}game_step(&g,t%20==0?(int)((t/20)%4):-1,false);assert(g.height==128&&g.width==13);}capture("arcade",PLAY,0);assert(g.height_score>0);
 game_start(&g,29,false);capture("stage30",PLAY,0);game_start(&g,99,false);capture("stage100",PLAY,0);capture("pause",PAUSE,0);capture("masks",MASKS,15);capture("shop",SHOP,2);capture("help",HELP,0);capture("save-error",SAVE_ERROR,0);
 Progress pf=g.p;pf.unlocked=190;pf.shields=10;save_encode(&pf,save);FILE*f=fopen("stage190.sav","wb");assert(f);fwrite(save,1,sizeof(save),f);fclose(f);
 f=fopen("host-report.json","w");assert(f);fprintf(f,"{\"game_bytes\":%zu,\"progress_bytes\":%zu,\"x_offset\":%zu,\"y_offset\":%zu,\"tick_offset\":%zu,\"maps\":%u,\"stages\":%u,\"random_ticks\":%u}",sizeof(Game),sizeof(Progress),offsetof(Game,x),offsetof(Game,y),offsetof(Game,tick),maps_ok,stages_ok,ticks);fclose(f);printf("PASS: %u maps, %u complete stage layouts, movement, traps, pickups, shop, save corruption and random-input soak. Game=%zu Progress=%zu\n",maps_ok,stages_ok,sizeof(Game),sizeof(Progress));}
