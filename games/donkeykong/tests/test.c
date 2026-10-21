/* SPDX-License-Identifier: GPL-3.0-only */
#include "game.h"
#include "render.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
static dk_game game;
static dk_stats stats;
static uint16_t image[320*240];
static unsigned pushes;
static void push(int x,int y,int w,int h,const uint16_t *p) {
  assert(x>=0&&y>=0&&x+w<=320&&y+h<=240);++pushes;
  for(int j=0;j<h;++j)memcpy(image+(y+j)*320+x,p+j*w,(size_t)w*2);
}
static void shot(const char *name,unsigned view) {
  dk_invalidate();dk_render(&game,&stats,view,0,true,false,800,push);
  char path[100];snprintf(path,sizeof path,"%s.ppm",name);FILE *f=fopen(path,"wb");assert(f);
  fprintf(f,"P6\n320 240\n255\n");
  for(unsigned i=0;i<320*240;++i){unsigned c=image[i];unsigned char rgb[]={
    (unsigned char)((c>>11)*255/31),(unsigned char)(((c>>5)&63)*255/63),(unsigned char)((c&31)*255/31)};
    fwrite(rgb,1,3,f);}
  fclose(f);
}
static void step(unsigned input) {dk_step(&game,&stats,(uint8_t)input);assert(dk_valid(&game));}
static void frames(unsigned n,unsigned input) {while(n--)step(input);}
static void board(unsigned b) {
  dk_new(&game,1981);game.board=(uint8_t)b;game.phase=DK_CLEAR;game.timer=1;
  // Public transition is the only board loader. Choose the preceding board.
  game.board=(uint8_t)(b?b-1:3);step(0);
  if(!b)game.round=1;
  frames(65,0);assert(game.phase==DK_RUNNING&&game.board==b);
}
static void quiet(void) {memset(game.enemy,0,sizeof game.enemy);game.spawn=65535;}
static void place(unsigned floor,int x) {
  dk_player *p=&game.player;memset(p,0,sizeof *p);p->x=x*DK_Q;
  p->y=dk_surface(&game,floor,x)*DK_Q;p->grounded=1;p->support=(int8_t)floor;
  p->face=1;p->ladder=-1;p->fall_from=p->y;p->jump_id=1;
}
static void walk(int x) {
  unsigned limit=400;
  while(game.phase==DK_RUNNING&&abs(game.player.x/DK_Q-x)>1&&limit--) {
    unsigned dir=game.player.x/DK_Q<x?DK_RIGHT:DK_LEFT;
    unsigned input=dir;
    if(game.board==3&&game.player.support>=1&&game.player.support<=4&&
       (game.player.grounded||game.player.coyote)&&!game.player.hammer) {
      unsigned row=(unsigned)game.player.support-1;
      for(unsigned side=0;side<2;++side) {
        int gap=side?176:48,delta=gap-game.player.x/DK_Q;
        if((game.rivets&(1u<<(row*2+side)))&&abs(delta)<=7&&
           ((dir==DK_RIGHT&&delta>=0)||(dir==DK_LEFT&&delta<=0)))input|=DK_JUMP;
      }
    }
    step(input);
    if(game.phase!=DK_RUNNING&&game.phase!=DK_CLEAR)
      fprintf(stderr,"Walk failure board=%u age=%lu target=%d x=%ld y=%ld floor=%d reason=%u rivets=%u\n",
              game.board,(unsigned long)game.age,x,(long)game.player.x/DK_Q,(long)game.player.y/DK_Q,game.player.support,game.reason,game.rivets);
    assert(game.phase==DK_RUNNING||game.phase==DK_CLEAR);
  }
  assert(limit);
}
static void climb(unsigned index) {
  const dk_ladder *l=&game.ladder[index];walk(l->x);
  unsigned limit=200;
  while(game.player.support!=l->upper||!game.player.grounded) {
    if(game.player.hammer)step(0);else step(DK_UP);
    assert(game.phase==DK_RUNNING||game.phase==DK_CLEAR);assert(--limit);
  }
}
static void saves(void) {
  uint8_t bytes[32];dk_stats a={999999,1234,7,999},b={0};dk_encode_stats(&a,bytes);
  assert(dk_decode_stats(&b,bytes,32));assert(b.best==a.best&&b.runs==a.runs&&b.rescues==7&&b.best_round==999);
  for(unsigned i=0;i<32;++i){bytes[i]^=1;assert(!dk_decode_stats(&b,bytes,32));bytes[i]^=1;}
  assert(!dk_decode_stats(&b,bytes,31));assert(!dk_decode_stats(&b,NULL,0));
  assert(b.best==999999&&b.runs==1234);
}
static void mechanics(void) {
  board(0);quiet();place(0,196);climb(0);assert(game.player.support==1);
  place(0,78);frames(60,DK_UP);assert(game.player.support==0&&game.player.ladder<0);
  place(0,196);game.player.hammer=100;frames(10,DK_UP|DK_JUMP);
  assert(game.player.grounded&&game.player.ladder<0&&game.player.hammer==90);
  // Jump has an edge, holding the key cannot bounce automatically.
  place(0,100);step(0);frames(70,DK_JUMP);assert(game.player.grounded&&game.player.jump_id==2);
  place(5,8);frames(20,DK_LEFT);frames(100,0);assert(game.phase==DK_READY||game.phase==DK_DYING);
  board(0);quiet();game.bonus=100;game.bonus_ticks=89;step(0);
  assert(game.phase==DK_DYING&&game.reason==DK_TIME);frames(70,0);assert(game.lives==2);
  frames(65,0);quiet();game.lives=1;game.bonus=0;game.bonus_ticks=89;step(0);frames(70,0);assert(game.phase==DK_OVER);
  board(1);quiet();place(4,54);game.age=180;step(DK_UP);assert(game.player.ladder<0);
  game.age=240;step(DK_UP);assert(game.player.ladder==6);
  // A retracting ladder never drops a player already climbing it.
  game.age=150;frames(50,DK_UP);assert(game.player.support==6&&game.player.grounded);
  board(0);quiet();place(0,100);game.player.hammer=50;
  game.enemy[0]=(dk_enemy){.active=1,.type=DK_BARREL,.row=0,.x=110*DK_Q,
    .y=dk_surface(&game,0,110)*DK_Q,.dir=-1,.ladder=-1};
  step(0);assert(game.score==300&&!game.enemy[0].active);frames(10,0);assert(game.score==300);
  board(0);quiet();place(0,100);
  game.enemy[0]=(dk_enemy){.active=1,.type=DK_BARREL,.row=0,.x=104*DK_Q,
    .y=dk_surface(&game,0,104)*DK_Q,.dir=-1,.ladder=-1};
  step(0);assert(game.phase==DK_DYING&&game.reason==DK_CRASH);
  // Jumping across one barrel scores exactly once, upon a safe landing.
  board(0);quiet();place(0,110);game.previous_input=0;
  game.enemy[0]=(dk_enemy){.active=1,.type=DK_BARREL,.row=0,.x=132*DK_Q,
    .y=dk_surface(&game,0,132)*DK_Q,.dir=-1,.ladder=-1};
  step(DK_RIGHT|DK_JUMP);frames(34,DK_RIGHT);
  assert(game.phase==DK_RUNNING&&game.score==100&&game.player.grounded);
  frames(20,0);assert(game.score==100);
  // The extra life is granted once; scores saturate instead of wrapping.
  board(0);quiet();place(0,100);game.player.hammer=100;game.score=6990;
  game.enemy[0]=(dk_enemy){.active=1,.type=DK_BARREL,.row=0,.x=110*DK_Q,
    .y=dk_surface(&game,0,110)*DK_Q,.dir=-1,.ladder=-1};
  step(0);assert(game.lives==4&&game.extra_life);
  game.score=DK_MAX_SCORE-10;
  game.enemy[0]=(dk_enemy){.active=1,.type=DK_BARREL,.row=0,.x=110*DK_Q,
    .y=dk_surface(&game,0,110)*DK_Q,.dir=-1,.ladder=-1};
  step(0);assert(game.score==DK_MAX_SCORE&&game.lives==4);
  game.round=999;assert(dk_difficulty(&game)==8);
}
static void routes(void) {
  // Actual walking/climbing routes, hazards suppressed to test the geometry.
  board(0);quiet();memset(game.item,0,sizeof game.item);
  unsigned path[]={0,2,4,6,8,10};for(unsigned i=0;i<6;++i)climb(path[i]);
  assert(game.phase==DK_CLEAR&&game.score>0);frames(125,0);assert(game.board==1&&game.phase==DK_READY);
  board(1);quiet();memset(game.item,0,sizeof game.item);
  climb(0);climb(2);climb(4); // left half, safe from central oil
  while(!dk_ladder_open(&game,6))step(0);
  climb(6);walk(184);climb(8);assert(game.phase==DK_CLEAR);
  // Full elevator route: catch a lift, transfer via the top ledge, then
  // step down on the right-hand girders and use the final ladders.
  board(2);quiet();walk(40);frames(24,DK_JUMP|DK_RIGHT);
  unsigned wait=120;
  while(!game.player.grounded&&wait--)step(0);
  assert(wait&&game.player.support>=11&&game.player.support<=13);
  wait=500;
  while(game.player.y>71*DK_Q&&wait--)step(0);
  assert(wait);step(DK_JUMP);frames(40,0);
  assert(game.player.support==5&&game.player.grounded);walk(78);
  wait=400;
  while(wait--) {
    bool soon=false;
    for(unsigned i=14;i<17;++i)if(game.platform[i].y1>201)soon=true;
    if(soon)break;
    step(0);
  }
  assert(wait);frames(35,DK_JUMP|DK_RIGHT);wait=70;
  while(!game.player.grounded&&wait--)step(0);
  assert(wait&&game.player.support>=14&&game.player.support<=16);
  walk(154);assert(game.player.support==4);climb(3);climb(4);
  assert(game.phase==DK_CLEAR);
  // An entire rivet board with real movement and jumps over each open gap.
  board(3);quiet();memset(game.item,0,sizeof game.item);
  climb(0);walk(196);climb(3);walk(28);climb(4);walk(196);climb(7);walk(176);
  walk(196);
  wait=150;while(game.player.support!=3||!game.player.grounded){step(DK_DOWN);assert(--wait);}
  walk(28);climb(6);walk(48);assert(game.phase==DK_CLEAR);
  // Each rivet is consumed once; clearing all eight advances the round.
  board(3);quiet();memset(game.item,0,sizeof game.item);
  for(unsigned row=1;row<=4;++row)for(unsigned side=0;side<2;++side) {
    place(row,side?176:48);step(0);
    assert(game.rivets&(1u<<((row-1)*2+side)));
    assert(dk_surface(&game,row,side?176:48)==-1);
  }
  assert(game.phase==DK_CLEAR&&stats.rescues);frames(125,0);assert(game.board==0&&game.round==2);
  // Landing on an ascending lift carries the player; a wrap cannot teleport them.
  board(2);quiet();place(11,68);int before=game.player.y;
  frames(30,0);assert(game.player.grounded&&game.player.support==11&&game.player.y<before);
  game.age=355;step(0);assert(!game.player.grounded||game.player.support!=11);
}
static void soak(void) {
  uint32_t seed=0xabc123;
  for(unsigned b=0;b<4;++b)for(unsigned run=0;run<12;++run) {
    board(b);game.rng+=run*19;
    for(unsigned i=0;i<18000;++i) {
      seed^=seed<<13;seed^=seed>>17;seed^=seed<<5;
      unsigned input=(seed>>8)&31;if(i%12)input=game.previous_input;
      step(input);if(game.phase==DK_OVER)break;
    }
  }
}
int main(void) {
  saves();mechanics();routes();soak();
  memset(&stats,0,sizeof stats);stats.best=12000;
  for(unsigned b=0;b<4;++b) {board(b);frames(100,0);char name[20];snprintf(name,sizeof name,"board%u",b);shot(name,DK_PLAY);
    FILE *f;snprintf(name,sizeof name,"fixture%u.bin",b);f=fopen(name,"wb");assert(f);fwrite(&game,1,sizeof game,f);fclose(f);}
  board(0);shot("title",DK_TITLE);shot("pause",DK_PAUSE);shot("help",DK_HELP);
  unsigned p=pushes;dk_render(&game,&stats,DK_HELP,0,true,false,800,push);assert(p==pushes);
  printf("PASS: physics, collisions, routes, timers, saves, bounded soak, rendering. Game RAM %zu bytes.\n",sizeof game);
}
