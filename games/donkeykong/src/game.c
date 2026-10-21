/* SPDX-License-Identifier: GPL-3.0-only
 * Native arcade adaptation. Integer physics, bounded entity pools, no heap.
 * Layouts are redrawn for 224x224; this does not emulate the arcade ROM. */
#include "game.h"
#include <stdlib.h>
#include <string.h>
static int clamp(int x,int a,int b) {return x<a?a:x>b?b:x;}
static uint32_t random32(dk_game *g) {
  uint32_t x=g->rng;x^=x<<13;x^=x>>17;x^=x<<5;return g->rng=x;
}
unsigned dk_difficulty(const dk_game *g) {return g->round>9?8:g->round-1;}
static void platform(dk_game *g,int a,int b,int y,int z,int type,int dir) {
  g->platform[g->platform_count++]=(dk_platform){a,b,y,z,type,dir};
}
static void ladder(dk_game *g,int x,int lower,int upper,int broken,int retract) {
  g->ladder[g->ladder_count++]=(dk_ladder){x,lower,upper,broken,retract};
}
int dk_surface(const dk_game *g,unsigned index,int x) {
  if(index>=g->platform_count)return -1;
  const dk_platform *p=&g->platform[index];
  if(x<p->x1||x>p->x2)return -1;
  if(g->board==3&&index>=1&&index<=4) {
    unsigned bit=(index-1)*2;
    if((g->rivets&(1u<<bit))&&abs(x-48)<4)return -1;
    if((g->rivets&(2u<<bit))&&abs(x-176)<4)return -1;
  }
  return p->y1+(p->y2-p->y1)*(x-p->x1)/(p->x2-p->x1);
}
bool dk_ladder_open(const dk_game *g,unsigned index) {
  if(index>=g->ladder_count||g->ladder[index].broken)return false;
  return !g->ladder[index].retract||g->age%240<150;
}
int dk_belt_direction(const dk_game *g,const dk_platform *p) {
  return p->dir*((g->age/360)&1?-1:1);
}
static dk_enemy *spawn_enemy(dk_game *g,int type,int row,int x,int dir) {
  for(unsigned i=0;i<DK_ENEMIES;++i)if(!g->enemy[i].active) {
    dk_enemy *e=&g->enemy[i];memset(e,0,sizeof *e);
    e->active=1;e->type=type;e->row=row;e->x=x*DK_Q;e->dir=dir;e->ladder=-1;
    int y=dk_surface(g,(unsigned)row,x);e->y=(y<0?176:y)*DK_Q;e->old_x=e->x;
    return e;
  }
  return NULL;
}
static void load_board(dk_game *g) {
  g->age=0;g->bonus_ticks=0;g->spawn=100;g->throws=0;g->rivets=0;
  g->platform_count=g->ladder_count=0;g->phase=DK_READY;g->timer=65;
  g->bonus=(uint16_t)(g->round<5?4000+g->round*1000:8000);
  memset(g->enemy,0,sizeof g->enemy);memset(g->item,0,sizeof g->item);
  memset(&g->player,0,sizeof g->player);
  dk_player *p=&g->player;p->x=18*DK_Q;p->face=1;p->ladder=-1;
  p->grounded=1;p->support=0;p->jump_id=1;
  if(g->board==0) {
    platform(g,8,216,214,207,0,0);platform(g,8,216,177,184,0,0);
    platform(g,8,216,154,147,0,0);platform(g,8,216,117,124,0,0);
    platform(g,8,216,94,87,0,0);platform(g,8,216,57,64,0,0);
    platform(g,104,184,25,25,0,0);
    ladder(g,196,0,1,0,0);ladder(g,78,0,1,1,0);
    ladder(g,32,1,2,0,0);ladder(g,126,1,2,1,0);
    ladder(g,188,2,3,0,0);ladder(g,100,2,3,0,0);
    ladder(g,36,3,4,0,0);ladder(g,152,3,4,1,0);
    ladder(g,184,4,5,0,0);ladder(g,84,4,5,0,0);ladder(g,138,5,6,0,0);
    g->item[0]=(dk_item){58,dk_surface(g,1,58)-10,1,1};
    g->item[1]=(dk_item){160,dk_surface(g,3,160)-10,1,1};
  } else if(g->board==1) {
    platform(g,8,216,212,212,0,0);
    platform(g,8,98,176,176,1,1);platform(g,126,216,176,176,1,1);
    platform(g,8,216,140,140,0,0);
    platform(g,8,98,104,104,1,-1);platform(g,126,216,104,104,1,-1);
    platform(g,8,216,68,68,1,1);platform(g,128,208,28,28,0,0);
    ladder(g,24,0,1,0,0);ladder(g,200,0,2,0,0);
    ladder(g,56,1,3,0,0);ladder(g,166,2,3,0,0);
    ladder(g,28,3,4,0,0);ladder(g,196,3,5,0,0);
    ladder(g,54,4,6,0,1);ladder(g,172,5,6,0,1);ladder(g,184,6,7,0,0);
    g->item[0]=(dk_item){76,166,1,1};g->item[1]=(dk_item){150,94,1,1};
    g->item[2]=(dk_item){204,203,2,1};g->item[3]=(dk_item){100,131,3,1};
    g->item[4]=(dk_item){18,59,4,1};
  } else if(g->board==2) {
    platform(g,8,46,212,212,0,0);platform(g,146,216,212,212,0,0);
    platform(g,140,216,172,172,0,0);platform(g,140,216,132,132,0,0);
    platform(g,140,216,94,94,0,0);platform(g,8,82,55,55,0,0);
    platform(g,136,216,55,55,0,0);
    platform(g,8,42,168,168,0,0);platform(g,8,42,126,126,0,0);platform(g,8,42,84,84,0,0);
    platform(g,144,214,24,24,0,0);
    for(int i=0;i<3;++i)platform(g,56,80,212-i*52,212-i*52,2,-1);
    for(int i=0;i<3;++i)platform(g,104,128,56+i*52,56+i*52,2,1);
    ladder(g,200,1,2,0,0);ladder(g,160,2,3,0,0);ladder(g,198,3,4,0,0);
    ladder(g,156,4,6,0,0);ladder(g,188,6,10,0,0);
    g->item[0]=(dk_item){22,159,2,1};g->item[1]=(dk_item){200,123,3,1};
    g->item[2]=(dk_item){150,85,4,1};g->spawn=80;
  } else {
    for(int i=0;i<5;++i)platform(g,16,208,210-i*36,210-i*36,0,0);
    platform(g,144,208,28,28,0,0);
    for(int i=0;i<4;++i){ladder(g,28,i,i+1,0,0);ladder(g,196,i,i+1,0,0);}
    ladder(g,112,0,1,0,0);ladder(g,112,2,3,0,0);
    g->item[0]=(dk_item){150,164,1,1};g->item[1]=(dk_item){74,92,1,1};
    g->item[2]=(dk_item){84,201,2,1};g->item[3]=(dk_item){200,129,3,1};
    g->item[4]=(dk_item){60,57,4,1};
    spawn_enemy(g,DK_FIRE,1,142,-1);spawn_enemy(g,DK_FIRE,3,82,1);
    g->spawn=300;
  }
  p->y=dk_surface(g,0,p->x/DK_Q)*DK_Q;p->fall_from=p->y;
}
void dk_new(dk_game *g,uint32_t seed) {
  memset(g,0,sizeof *g);g->rng=seed?seed:0x1981a11u;g->round=1;g->lives=3;
  load_board(g);
}
static void points(dk_game *g,dk_stats *s,unsigned score,int x,int y) {
  if(score>DK_MAX_SCORE-g->score)g->score=DK_MAX_SCORE;else g->score+=score;
  if(g->score>s->best)s->best=g->score;
  if(!g->extra_life&&g->score>=7000){g->extra_life=1;if(g->lives<9)++g->lives;}
  if(x>=0){g->popup_x=(int16_t)x;g->popup_y=(int16_t)y;g->popup_time=50;g->popup_value=(uint16_t)score;}
}
static void die(dk_game *g,unsigned reason) {
  if(g->phase!=DK_RUNNING)return;
  g->phase=DK_DYING;g->timer=70;g->reason=(uint8_t)reason;g->player.hammer=0;
}
static void clear(dk_game *g,dk_stats *s) {
  points(g,s,g->bonus,-1,0);g->phase=DK_CLEAR;g->timer=125;
  if(g->board==3&&s->rescues<UINT32_MAX)++s->rescues;
}
static void update_lifts(dk_game *g) {
  if(g->board!=2)return;
  for(unsigned i=11;i<17;++i) {
    dk_platform *p=&g->platform[i];int old=p->y1;
    unsigned travel=(g->age*112u/DK_Q+(i-11)%3*52)%156;
    p->y1=p->y2=(int16_t)(p->dir<0?212-(int)travel:56+(int)travel);
    if(g->player.grounded&&g->player.support==(int)i) {
      int delta=p->y1-old;
      if(abs(delta)>4)g->player.grounded=0,g->player.support=-1;
      else g->player.y+=delta*DK_Q,g->player.fall_from+=delta*DK_Q;
    }
  }
}
static void move_player(dk_game *g,unsigned input) {
  dk_player *p=&g->player;int steer=!!(input&DK_RIGHT)-!!(input&DK_LEFT);
  int vertical=!!(input&DK_DOWN)-!!(input&DK_UP);
  if(p->hammer)--p->hammer;
  if((input&DK_JUMP)&&!(g->previous_input&DK_JUMP))p->jump_buffer=7;
  else if(p->jump_buffer)--p->jump_buffer;
  if(p->grounded)p->coyote=6;else if(p->coyote)--p->coyote;
  if(!p->hammer&&vertical&&p->ladder<0) {
    for(unsigned i=0;i<g->ladder_count;++i)if(dk_ladder_open(g,i)) {
      dk_ladder *l=&g->ladder[i];int top=dk_surface(g,l->upper,l->x),bottom=dk_surface(g,l->lower,l->x);
      if(top<0||bottom<0||abs(p->x-l->x*DK_Q)>5*DK_Q)continue;
      if((vertical<0&&abs(p->y-bottom*DK_Q)<=3*DK_Q)||
         (vertical>0&&abs(p->y-top*DK_Q)<=3*DK_Q)) {
        p->ladder=(int8_t)i;p->x=l->x*DK_Q;p->grounded=0;p->vy=0;p->jump_buffer=0;break;
      }
    }
  }
  if(p->ladder>=0) {
    dk_ladder *l=&g->ladder[(unsigned)p->ladder];
    int top=dk_surface(g,l->upper,l->x),bottom=dk_surface(g,l->lower,l->x);
    p->y+=vertical*200;
    if(p->y<=top*DK_Q){p->y=top*DK_Q;p->grounded=1;p->support=l->upper;p->ladder=-1;}
    else if(p->y>=bottom*DK_Q){p->y=bottom*DK_Q;p->grounded=1;p->support=l->lower;p->ladder=-1;}
    p->fall_from=p->y;return;
  }
  if(steer)p->face=(int8_t)steer;
  int belt=0;
  if(p->grounded&&p->support>=0) {
    dk_platform *floor=&g->platform[(unsigned)p->support];
    if(floor->type==DK_BELT)belt=dk_belt_direction(g,floor)*74;
  }
  p->x=clamp(p->x+steer*272+belt,6*DK_Q,218*DK_Q);
  if(p->jump_buffer&&p->coyote&&!p->hammer) {
    p->vy=-620;p->grounded=0;p->support=-1;p->jump_buffer=0;p->coyote=0;
    p->fall_from=p->y;p->air_points=0;if(++p->jump_id==0)++p->jump_id;
  }
  if(p->grounded) {
    int ground=dk_surface(g,(unsigned)p->support,p->x/DK_Q);
    if(ground>=0){p->y=ground*DK_Q;p->fall_from=p->y;p->vy=0;return;}
    p->grounded=0;p->support=-1;p->vy=0;
  }
  int old=p->y;p->vy=clamp(p->vy+40,-2000,1400);p->y+=p->vy;
  if(p->vy>=0) {
    int best=DK_H+32,index=-1;
    for(unsigned i=0;i<g->platform_count;++i) {
      int y=dk_surface(g,i,p->x/DK_Q);
      if(y>=0&&y<best&&old<=(y+2)*DK_Q&&p->y>=y*DK_Q)best=y,index=(int)i;
    }
    if(index>=0) {
      p->y=best*DK_Q;p->vy=0;p->support=(int8_t)index;p->grounded=1;
      if(p->y-p->fall_from>28*DK_Q)die(g,DK_FALL);
      p->fall_from=p->y;
    }
  }
  if(p->y>(DK_H+14)*DK_Q)die(g,DK_FALL);
}
static void barrel(dk_game *g,dk_enemy *e) {
  unsigned difficulty=dk_difficulty(g);
  if(e->mode) {
    e->vy=clamp(e->vy+48,0,1300);e->y+=e->vy;
    int target=dk_surface(g,(unsigned)e->row,clamp(e->x/DK_Q,8,216));
    if(target>=0&&e->y>=target*DK_Q){e->y=target*DK_Q;e->vy=0;e->mode=0;}
    return;
  }
  e->x+=e->dir*(260+(int)difficulty*14);
  dk_platform *p=&g->platform[(unsigned)e->row];
  if(e->x<=p->x1*DK_Q||e->x>=p->x2*DK_Q) {
    e->x=clamp(e->x,p->x1*DK_Q,p->x2*DK_Q);
    if(e->row==0) {
      if(e->blue)spawn_enemy(g,DK_FIRE,0,30,1);
      e->active=0;return;
    }
    --e->row;e->dir=-e->dir;e->mode=1;e->vy=0;return;
  }
  e->y=dk_surface(g,(unsigned)e->row,e->x/DK_Q)*DK_Q;
  if(!e->timer) {
    for(unsigned i=0;i<g->ladder_count;++i) {
      dk_ladder *l=&g->ladder[i];
      if(l->upper==e->row&&abs(e->x-l->x*DK_Q)<2*DK_Q&&
         (random32(g)%8<(e->blue?4:1+difficulty/3))) {
        e->row=l->lower;e->x=l->x*DK_Q;e->mode=1;e->vy=160;
        e->dir=e->row&1?1:-1;e->timer=30;break;
      }
    }
  }
}
static void fire(dk_game *g,dk_enemy *e) {
  int speed=58+(int)dk_difficulty(g)*8;
  if(e->ladder>=0) {
    dk_ladder *l=&g->ladder[(unsigned)e->ladder];
    int y=dk_surface(g,(unsigned)e->row,l->x);
    e->y+=(y*DK_Q>e->y?1:-1)*100;
    if(abs(e->y-y*DK_Q)<=100){e->y=y*DK_Q;e->ladder=-1;e->timer=50;}
    return;
  }
  if(!e->timer) {
    e->dir=g->player.x>e->x?1:-1;e->timer=(int16_t)(70+random32(g)%90);
  }
  int next=e->x+e->dir*speed;
  const dk_platform *p=&g->platform[(unsigned)e->row];
  if(next<(p->x1+6)*DK_Q||next>(p->x2-6)*DK_Q||dk_surface(g,(unsigned)e->row,next/DK_Q)<0)e->dir=-e->dir;
  else e->x=next;
  int floor=dk_surface(g,(unsigned)e->row,e->x/DK_Q);
  if(floor>=0)e->y=floor*DK_Q;
  if(e->timer<40)for(unsigned i=0;i<g->ladder_count;++i)if(dk_ladder_open(g,i)) {
    dk_ladder *l=&g->ladder[i];int target=-1;
    if(l->lower==e->row&&g->player.y<e->y-10*DK_Q)target=l->upper;
    if(l->upper==e->row&&g->player.y>e->y+10*DK_Q)target=l->lower;
    if(target>=0&&abs(e->x-l->x*DK_Q)<3*DK_Q) {
      e->x=l->x*DK_Q;e->row=(int8_t)target;e->ladder=(int8_t)i;break;
    }
  }
}
static void generate(dk_game *g) {
  if(g->spawn){--g->spawn;return;}
  unsigned d=dk_difficulty(g);
  if(g->board==0) {
    dk_enemy *e=spawn_enemy(g,DK_BARREL,5,62,1);
    if(e)e->blue=(++g->throws%5)==0;
    g->spawn=(uint16_t)(130-d*7+random32(g)%35);
  } else if(g->board==1) {
    int row=(g->throws++&1)?4:2;
    dk_platform *p=&g->platform[row];int dir=dk_belt_direction(g,p);
    spawn_enemy(g,DK_TUB,row,dir>0?p->x1+8:p->x2-8,dir);
    /* Both sides get traffic; the centre gap is crossed by climbing around it. */
    row=row==4?5:1;p=&g->platform[row];dir=dk_belt_direction(g,p);
    spawn_enemy(g,DK_TUB,row,dir>0?p->x1+8:p->x2-8,dir);
    if(g->throws%3==0)spawn_enemy(g,DK_FIRE,3,112,(random32(g)&1)?1:-1);
    g->spawn=(uint16_t)(210-d*10);
  } else if(g->board==2) {
    spawn_enemy(g,DK_SPRING,5,45,1);g->spawn=(uint16_t)(115-d*5+random32(g)%25);
  } else {
    unsigned count=0;for(unsigned i=0;i<DK_ENEMIES;++i)count+=g->enemy[i].active;
    if(count<5) {
      int row=1+(int)(random32(g)%4),dir=random32(g)&1?1:-1;
      spawn_enemy(g,DK_FIRE,row,dir>0?24:200,dir);
    }
    g->spawn=(uint16_t)(290-d*15);
  }
}
static void items(dk_game *g,dk_stats *s) {
  dk_player *p=&g->player;
  for(unsigned i=0;i<5;++i) {
    dk_item *v=&g->item[i];if(!v->active)continue;
    if(abs(p->x/DK_Q-v->x)<9&&p->y/DK_Q>v->y-3&&p->y/DK_Q<v->y+21) {
      if(v->type==1){if(p->ladder>=0)continue;p->hammer=480;}
      else points(g,s,300,v->x,v->y-8);
      v->active=0;
    }
  }
  if(g->board==3&&p->grounded&&p->support>=1&&p->support<=4) {
    int left=abs(p->x/DK_Q-48),right=abs(p->x/DK_Q-176);
    if(left<=3||right<=3) {
      unsigned bit=(unsigned)(p->support-1)*2+(right<left);
      if(!(g->rivets&(1u<<bit))) {
        g->rivets|=(uint8_t)(1u<<bit);points(g,s,100,p->x/DK_Q,p->y/DK_Q-22);
        if(g->rivets==255)clear(g,s);
      }
    }
  }
}
static void enemies(dk_game *g,dk_stats *s,int old_player_x) {
  dk_player *p=&g->player;
  for(unsigned i=0;i<DK_ENEMIES&&g->phase==DK_RUNNING;++i) {
    dk_enemy *e=&g->enemy[i];if(!e->active)continue;
    e->old_x=e->x;if(e->timer>0)--e->timer;
    if(e->type==DK_BARREL)barrel(g,e);
    else if(e->type==DK_FIRE)fire(g,e);
    else if(e->type==DK_TUB) {
      e->dir=(int8_t)dk_belt_direction(g,&g->platform[(unsigned)e->row]);
      e->x+=e->dir*(145+(int)dk_difficulty(g)*10);
      dk_platform *floor=&g->platform[(unsigned)e->row];
      if(e->x<(floor->x1+3)*DK_Q||e->x>(floor->x2-3)*DK_Q)e->active=0;
    } else {
      if(!e->mode) {
        e->x+=640+(int)dk_difficulty(g)*12;
        unsigned phase=(g->age+i*7)%30;
        int bump=(int)(phase*(30-phase))/15;
        e->y=(55-bump)*DK_Q;
        if(e->x>=207*DK_Q)e->mode=1,e->vy=0;
      } else e->vy+=60,e->y+=e->vy;
      if(e->y>235*DK_Q)e->active=0;
    }
    if(!e->active)continue;
    int dx=(e->x-p->x)/DK_Q,dy=(e->y-p->y)/DK_Q;
    if(p->hammer&&e->type!=DK_SPRING&&dx*p->face>=-4&&dx*p->face<19&&dy>-14&&dy<20) {
      e->active=0;points(g,s,300,e->x/DK_Q,e->y/DK_Q-16);continue;
    }
    if(!p->grounded&&p->ladder<0&&!p->hammer&&e->jump_id!=p->jump_id&&
       dy>=7&&dy<=30&&((old_player_x-e->old_x<0&&p->x-e->x>=0)||
                         (old_player_x-e->old_x>0&&p->x-e->x<=0))) {
      e->jump_id=p->jump_id;if(p->air_points<3)++p->air_points;
    }
    /* Body collision, with a two-pixel forgiving margin on the sprite edges. */
    int height=e->type==DK_FIRE?11:8;
    if(abs(dx)<8&&dy>-height+2&&dy<12)die(g,DK_CRASH);
  }
}
void dk_step(dk_game *g,dk_stats *s,uint8_t input) {
  if(g->phase==DK_OVER)return;
  ++g->tick;if(g->popup_time)--g->popup_time;
  if(g->round>s->best_round)s->best_round=g->round;
  if(g->phase!=DK_RUNNING) {
    if(g->timer)--g->timer;
    if(!g->timer) {
      if(g->phase==DK_READY)g->phase=DK_RUNNING;
      else if(g->phase==DK_DYING) {
        if(g->lives)--g->lives;
        if(!g->lives)g->phase=DK_OVER;else load_board(g);
      } else if(g->phase==DK_CLEAR) {
        if(++g->board==4){g->board=0;if(g->round<999)++g->round;}
        load_board(g);
      }
    }
    g->previous_input=input;return;
  }
  ++g->age;update_lifts(g);
  int old_x=g->player.x;uint8_t was_grounded=g->player.grounded;
  move_player(g,input);
  if(g->phase==DK_RUNNING) {
    items(g,s);
    if(g->phase==DK_RUNNING){generate(g);enemies(g,s,old_x);}
    if(!was_grounded&&g->player.grounded&&g->player.air_points) {
      points(g,s,g->player.air_points==1?100:g->player.air_points==2?300:500,
             g->player.x/DK_Q,g->player.y/DK_Q-21);g->player.air_points=0;
    }
    if(g->phase==DK_RUNNING) {
      int x=g->player.x/DK_Q,y=g->player.y/DK_Q;
      if(g->board!=3&&y<=29&&x>=132&&g->player.grounded)clear(g,s);
      if(g->board==1&&abs(x-112)<10&&y>153&&y<184)die(g,DK_CRASH);
      /* Kong rides harmlessly on the conveyor board, as in the arcade game. */
      if(g->board!=1) {
        int kx=g->board==3?112:34,ky=g->board==0?58:g->board==2?55:66;
        if(abs(x-kx)<15&&y>ky-26&&y<ky+9)die(g,DK_CRASH);
      }
      if(++g->bonus_ticks>=90){g->bonus_ticks=0;if(g->bonus>=100)g->bonus-=100;if(!g->bonus)die(g,DK_TIME);}
    }
  }
  g->previous_input=input;
}
bool dk_valid(const dk_game *g) {
  if(!g->round||g->round>999||g->board>3||g->phase>DK_OVER||g->lives>9||
     g->score>DK_MAX_SCORE||g->platform_count>DK_PLATFORMS||g->ladder_count>DK_LADDERS||
     g->player.x<0||g->player.x>DK_W*DK_Q||g->player.y< -64*DK_Q||g->player.y>512*DK_Q)return false;
  if(g->player.support< -1||g->player.support>=g->platform_count||
     g->player.ladder< -1||g->player.ladder>=g->ladder_count)return false;
  for(unsigned i=0;i<DK_ENEMIES;++i)if(g->enemy[i].active&&
      (g->enemy[i].type<1||g->enemy[i].type>4||g->enemy[i].row<0||g->enemy[i].row>=g->platform_count))return false;
  return true;
}
