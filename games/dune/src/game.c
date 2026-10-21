#include "game.h"
#include "assets.h"
#include <math.h>
#include <string.h>
static float clamp(float v,float a,float b){return v<a?a:v>b?b:v;}
static uint32_t random_u(Game*g){uint32_t x=g->rng;x^=x<<13;x^=x>>17;x^=x<<5;return g->rng=x;}
static float random_f(Game*g){return (random_u(g)>>8)*(1.f/16777216.f);}
static float curve(const Terrain*t,float n){return t->flat||n>=63?0:sinf(n/20.f)*t->height*n/t->divisor;}
void terrain_shape(Terrain*t,float r0,float r1,float r2,bool flat){t->raw_width=fminf(2048,700+1700*r0+(flat?1000:0));t->width=t->raw_width*.7f;t->height=t->raw_width/10+220*r1-50;t->divisor=42+15*r2;t->flat=flat;for(unsigned j=0;j<9;j++)t->contact[j]=curve(t,j<8?j*8:63)+407.5f;}
static void shape(Game*g,unsigned i,bool flat){float a=random_f(g),b=random_f(g),c=random_f(g);terrain_shape(&g->terrain[i],a,b,c,flat);}
float terrain_u(const Terrain*t,float x){return (x-t->x)/t->width;}
float terrain_surface(const Terrain*t,float x){float n=clamp((x-t->x)*90/t->raw_width,0,63);unsigned i=(unsigned)n;return 392.5f+curve(t,(float)i)*(1-(n-i))+curve(t,(float)i+1)*(n-i);}
unsigned game_terrain(const Game*g,float x){for(unsigned j=0;j<8;j++){unsigned i=g->order[j];if(x>=g->terrain[i].x&&x<=g->terrain[i].x+g->terrain[i].width)return i;}return x<g->terrain[g->order[0]].x?g->order[0]:g->order[7];}
static void add_coin(Game*g,float x,float y){for(unsigned i=0;i<COIN_COUNT;i++)if(!g->coins[i].active){g->coins[i]=(Coin){x,y,1};return;}}
static void populate(Game*g,Terrain*t){if(random_f(g)>=.6f)return;unsigned count=2+random_u(g)%3,start=8+random_u(g)%30;for(unsigned i=0;i<count;i++){float x=t->x+t->raw_width*(start+i*5)/90.f;add_coin(g,x,terrain_surface(t,x)-70);}}
void game_new(Game*g,uint32_t seed){memset(g,0,sizeof(*g));g->rng=seed?seed:0x44554e45;g->x=700;g->y=200;g->zoom=1;g->trail_scale=1;g->spare=8;g->slide_terrain=255;float x=-300;for(unsigned i=0;i<9;i++){shape(g,i,i==0);if(i<8){g->order[i]=i;g->terrain[i].x=x;x=floorf(x-1)+g->terrain[i].width;if(i>1)populate(g,&g->terrain[i]);}}}
typedef struct {float px,py,nx,ny,dist,u;unsigned terrain;} Contact;
static Contact nearest(const Game*g,float x,float y){Contact c={.dist=1e20f};for(unsigned j=0;j<8;j++){unsigned id=g->order[j];const Terrain*t=&g->terrain[id];if(x<t->x-100||x>t->x+t->width+100)continue;for(unsigned k=0;k<8;k++){float ax=t->x+t->raw_width*(k*8)/90.f,bx=t->x+t->raw_width*(k<7?(k+1)*8:63)/90.f,ay=t->contact[k],by=t->contact[k+1],dx=bx-ax,dy=by-ay,l2=dx*dx+dy*dy,u=clamp(((x-ax)*dx+(y-ay)*dy)/l2,0,1),px=ax+u*dx,py=ay+u*dy,vx=x-px,vy=y-py,d=vx*vx+vy*vy;if(d<c.dist){float inv=1/sqrtf(l2);c=(Contact){px,py,dy*inv,-dx*inv,d,terrain_u(t,x),id};}}}return c;}
static void score_add(Game*g,unsigned n,unsigned popup){g->score=UINT32_MAX-g->score<n?UINT32_MAX:g->score+n;g->popup=popup;g->popup_age=90;}
void game_input(Game*g,bool held){if(held){if(g->vx<2700){g->vx+=16;if(g->grounded){g->vx+=12;if(g->vx<250){g->vx+=30;g->vy-=30;}else g->vy+=g->vx/80;}}if(g->vy<2300&&!g->grounded){g->vy+=80;g->vy+=g->vx/50;}}}
void game_step(Game*g,bool held){
 if(g->ready)return;
 ++g->tick;
 if(g->popup_age)--g->popup_age;
 if(g->phase==CRASHED){if(++g->phase_age>=45)g->ready=1;return;}
 float previous_vx=g->vx,previous_vy=g->vy;unsigned airborne=g->airborne;
 Contact before=nearest(g,g->x,g->y);g->grounded=before.dist<=70*70;
 game_input(g,held);
 if(g->vx<1800)g->vx*=1.005f;
 if(g->y< -650&&g->vy<0){g->vx+=.15f*-g->vy;g->vy*=.8f;}else if(g->y< -300&&g->vy<0)g->vy+=30;
 bool landed=false;Contact c=before;
 for(unsigned sub=0;sub<4;sub++){
  g->vy+=1000.f/240;g->x+=g->vx/240;g->y+=g->vy/240;c=nearest(g,g->x,g->y);
  float signed_d=(g->x-c.px)*c.nx+(g->y-c.py)*c.ny;
  if(c.dist<42*42&&signed_d>=-20){float penetration=42-signed_d;g->x+=c.nx*penetration;g->y+=c.ny*penetration;float vn=g->vx*c.nx+g->vy*c.ny;if(vn<0){g->vx-=c.nx*vn;g->vy-=c.ny*vn;landed=true;}}
 }
 g->vx*=.9998325f;g->vy*=.9998325f;c=nearest(g,g->x,g->y);bool touching=c.dist<=70*70;
 if(g->grounded&&!touching&&held&&g->vx>0)g->vx*=1.03f,g->vy*=1.03f;
 g->grounded=touching;if(touching)g->airborne=0;else if(g->airborne<65535)++g->airborne;
 if(landed&&previous_vx>10&&previous_vy>30&&fabsf(previous_vx-g->vx)>70&&fabsf(previous_vy-g->vy)>70&&c.u>.7f&&airborne>20&&!g->terrain[c.terrain].flat){g->phase=CRASHED;g->phase_age=0;}
 if(landed&&g->vy>1000)g->vy=1000;
 Contact small=nearest(g,g->x,g->y+30);bool slide=small.dist<=35*35;
 if(slide&&!g->sliding&&small.u<.4f&&previous_vy>10&&!g->terrain[small.terrain].flat){g->slide_start=small.u;g->slide_terrain=small.terrain;}
 if(slide&&g->slide_terrain==small.terrain&&!g->terrain[small.terrain].slid&&small.u-g->slide_start>.4f&&g->slide_start<.45f&&hypotf(g->vx,g->vy)>1900){bool perfect=g->slide_start<.2f;score_add(g,perfect?3:1,perfect?4:5);float boost=perfect?1.2f:1.05f;g->vx*=boost;g->vy*=boost;g->terrain[small.terrain].slid=1;}
 if(!slide)g->slide_terrain=255;
 g->sliding=slide;
 if(!g->above_line&&g->y<222&&g->vy<0){unsigned n=g->vy> -800?1:g->vy> -1300?2:3;score_add(g,n,n);g->above_line=1;unsigned count=3+(unsigned)(2*random_f(g)+.999999f);for(unsigned i=0;i<count;i++){float x=g->x+i*.1f*g->vx,y=g->y+i*.1f*g->vy+i*i*i;if(y<=170)add_coin(g,x,y);}}
 if(g->above_line&&g->y>222)g->above_line=0;
 for(unsigned i=0;i<COIN_COUNT;i++)if(g->coins[i].active){float dx=g->x-g->coins[i].x,dy=g->y-g->coins[i].y;if(dx*dx+dy*dy<72.8f*72.8f){g->coins[i].active=0;if(g->earned<UINT32_MAX)++g->earned;}else if(dx>2200)g->coins[i].active=0;}
 while(g->terrain[g->order[1]].x+g->terrain[g->order[1]].width<g->x-2000){unsigned old=g->order[1],id=old;if(random_f(g)<.6f){id=g->spare;g->spare=old;}Terrain*t=&g->terrain[id],*last=&g->terrain[g->order[7]];t->x=floorf(last->x-1)+last->width;t->slid=0;for(unsigned i=1;i<7;i++)g->order[i]=g->order[i+1];g->order[7]=id;++g->terrains;populate(g,t);}
 float target=clamp((g->y+392.5f)/750,.4f,1),blend=.2f+clamp(-.0004f*g->vy,.2f,.8f);g->zoom+=(target-g->zoom)*blend;float cy=100*g->zoom+(g->zoom-1)*665;g->camera_y+=(cy-g->camera_y)*.4f;g->camera_x=g->x-300/g->zoom;g->rotation+=g->vx/8000;
 g->trail_scale+=(clamp((g->vx+g->vy)/(touching?400:160),.2f,4.5f)-g->trail_scale)*.03f;g->trail_angle+=(atan2f(g->vy,g->vx)-g->trail_angle)*.3f;
 if(g->x>100000){g->x-=90000;g->camera_x-=90000;for(unsigned i=0;i<9;i++)g->terrain[i].x-=90000;for(unsigned i=0;i<COIN_COUNT;i++)g->coins[i].x-=90000;}
 if(!isfinite(g->x)||!isfinite(g->y)||fabsf(g->y)>10000||g->vx< -1){g->phase=CRASHED;g->phase_age=0;}
}
void game_finish(Stats*s,const Game*g){if(g->score>s->best)s->best=g->score;s->coins=UINT32_MAX-s->coins<g->earned?UINT32_MAX:s->coins+g->earned;s->seed=g->rng;}
bool game_valid(const Game*g){if(!isfinite(g->x)||!isfinite(g->y)||!isfinite(g->vx)||!isfinite(g->vy)||g->zoom<.39f||g->zoom>1.01f||g->phase>CRASHED)return false;unsigned mask=1u<<g->spare;for(unsigned i=0;i<8;i++){if(g->order[i]>=9||mask&(1u<<g->order[i]))return false;mask|=1u<<g->order[i];}return mask==511;}
bool shop_owned(const Stats*s,bool scene,unsigned i){return i<(scene?LANDSCAPE_COUNT:BALL_COUNT)&&((scene?s->scenes:s->unlocked)&(1u<<i));}
bool shop_buy(Stats*s,bool scene,unsigned i){if(i>=(scene?LANDSCAPE_COUNT:BALL_COUNT))return false;unsigned price=scene?landscapes[i].price:ball_styles[i].price;if(!shop_owned(s,scene,i)){if(s->coins<price)return false;s->coins-=price;if(scene)s->scenes|=1u<<i;else s->unlocked|=1u<<i;}if(scene)s->landscape=i;else s->ball=i;return true;}
