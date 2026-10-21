#include "game.h"
#include "assets.h"
#include <math.h>
#include <string.h>
#define DEG .01745329251994329577f
void game_new(Game*g,unsigned level){memset(g,0,sizeof(*g));if(level>=60)level=59;g->level=(uint8_t)level;const Level*l=&levels[level];g->count=l->pins;g->remaining=l->shots;g->motion=l->motion;for(unsigned i=0;i<g->count;i++)g->pins[i]=preset_angle[preset_offset[l->pins]+i];}
void pin_position(const Game*g,unsigned i,float*x,float*y){float a=(g->pins[i]+g->angle)*DEG,r=64.285714f;if(g->phase==CLEARED)r+=25*g->phase_age;*x=160+cosf(a)*r;*y=85.714286f+sinf(a)*r;}
/* Test in the reference's 560-pixel world. Rounding radii after shrinking
 * to 240 pixels artificially closes the narrow gaps in the last levels. */
bool game_collision(const Game*g,float x,float y){for(unsigned i=0;i<g->count;i++){float px,py;pin_position(g,i,&px,&py);float dx=(x-px)*(7.f/3),dy=(y-py)*(7.f/3);int distance=(int)sqrtf(floorf(dx*dx)+floorf(dy*dy));if(distance<=26)return true;}return false;}
void game_step(Game*g,bool fire){
 if(g->ready)return;
 ++g->tick;
 if(g->phase!=RUNNING){if(++g->phase_age>=60)g->ready=1;return;}
 unsigned m=g->motion;
 if(fire&&g->remaining&&m>=10)g->direction^=1;
 if(m==4||m==5){unsigned interval=m==4?180:120;if(++g->motion_age>interval){g->motion_age=0;g->direction^=1;}}
 else if((m>=6&&m<=9)||m==11){unsigned interval=m==11?60:m<8?72:102;if(++g->motion_age>interval){g->motion_age=0;g->speed_phase^=1;}}
 float speed=m<2?1.5f:m<4?2.5f:m==4?2.2f:m==5?3.5f:m<8||m>=10?(g->speed_phase?.3f:2.f):(g->speed_phase?.5f:4.f);
 if(m==1||m==3||m==7||m==9)speed=-speed;
 if(g->direction)speed=-speed;
 g->speed=speed;g->angle=fmodf(g->angle+speed+360.f,360.f);
 for(unsigned i=0;i<6;i++)if(g->flights[i]){
   if(--g->flights[i]==0){
     if(game_collision(g,160,156.f)){g->phase=FAILED;g->phase_age=0;return;}
     if(g->count<MAX_PINS)g->pins[g->count++]=90-g->angle;
   }
 }
 if(fire&&g->remaining){for(unsigned i=0;i<6;i++)if(!g->flights[i]){g->flights[i]=3;--g->remaining;break;}}
 bool flying=false;for(unsigned i=0;i<6;i++)flying|=g->flights[i]!=0;
 if(!g->remaining&&!flying){g->phase=CLEARED;g->phase_age=0;}
}
void game_finish(Stats*s,const Game*g){if(g->phase==CLEARED){unsigned next=g->level+1;if(next>s->unlocked)s->unlocked=next;s->current=next<60?next:59;}}
bool game_valid(const Game*g){return g->level<60&&g->count<=MAX_PINS&&g->remaining<=26&&g->motion<12&&g->phase<=CLEARED&&isfinite(g->angle)&&g->angle>=0&&g->angle<360;}
