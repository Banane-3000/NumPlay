#include "game.h"
#include <string.h>
#include <stdlib.h>
static void hurt(Game*);
static const int8_t dirs[4][2]={{-1,0},{1,0},{0,-1},{0,1}};
static uint32_t random32(Game*g){uint32_t x=g->rng;x^=x<<13;x^=x>>17;x^=x<<5;return g->rng=x?x:1;}
uint8_t game_tile(const Game*g,int x,int y){return x<0||y<0||x>=g->width||y>=g->height?3:g->tiles[y*g->width+x];}
bool game_solid(unsigned t){return t==109||t==110||t==3||t==5||t==7||t==114||t==121||t==73||t==74||t==85||t==97;}
static void set(Game*g,int x,int y,unsigned t){if(x>=0&&y>=0&&x<g->width&&y<g->height)g->tiles[y*g->width+x]=(uint8_t)t;}
unsigned game_player_level(const Progress*p){unsigned n=1;while(n<20&&p->xp>=xp_targets[n])++n;return n;}
bool game_mask(Game*g,unsigned m){if(m>=16||game_player_level(&g->p)<mask_levels[m])return false;if(!(g->p.owned&(1u<<m))){if(g->p.coins<mask_prices[m])return false;g->p.coins-=mask_prices[m];g->p.owned|=1u<<m;}g->p.mask=m;g->dirty=1;return true;}
static void coins_for_area(Game*g,int y0,int y1){unsigned chance=500*(100+mask_features[g->p.mask][1])/100;for(int y=y0;y<y1;y++)for(unsigned x=0;x<g->width;x++)if(g->tiles[y*g->width+x]==19&&random32(g)%10000>=chance)g->tiles[y*g->width+x]=31;}
static Actor*spawn(Game*g,unsigned type,int x,int y,int vx,int vy){for(unsigned i=0;i<ACTORS;i++)if(!g->actors[i].active){Actor*a=&g->actors[i];*a=(Actor){.x=x,.y=y,.home_x=x,.home_y=y,.vx=vx,.vy=vy,.type=type,.active=1};return a;}return NULL;}
static void add_actors(Game*g,int y0,int y1){for(int y=y0;y<y1;y++)for(unsigned x=0;x<g->width;x++){unsigned t=game_tile(g,x,y);int d=-1;if(t==76||t==78)d=1;if(t==77||t==79)d=0;if(t==88||t==90)d=2;if(t==89||t==91)d=3;if(d>=0){spawn(g,t,x*Q,y*Q,dirs[d][0]*20,dirs[d][1]*20);set(g,x,y,0);}else if(t==102||t==109||t==110)spawn(g,t,x*Q,y*Q,0,0);}}
static void decode_into(Game*g,unsigned id,int top){uint8_t temp[13*15];const Map*m=&maps[id];if(m->w!=13||m->h>15||map_decode(id,temp,sizeof(temp))<0)return;for(unsigned y=0;y<m->h;y++)for(unsigned x=0;x<13;x++)if(top+(int)y>=0&&top+(int)y<g->height)set(g,x,top+y,temp[y*13+x]);}
static unsigned choose_chunk(Game*g,unsigned zone){unsigned found=0,pick=200;for(unsigned i=200;i<717;i++)if(maps[i].zone==zone){if(random32(g)%++found==0)pick=i;}return pick;}
static void fill_arcade(Game*g,int top,int bottom){unsigned zone=1+(g->height_score/100)%14;if(g->bonus==2)zone=12;else if(random32(g)%1000<20u*(100+mask_features[g->p.mask][6])/100)zone=10;int y=bottom;while(y>top){unsigned id=choose_chunk(g,zone);uint8_t temp[195];
 for(unsigned attempt=0;attempt<60;attempt++){const Map*m=&maps[id];map_decode(id,temp,sizeof(temp));bool connected=y>=g->height;
  for(int x=1;x<12;x++)if(!game_solid(temp[(m->h-1)*13+x])&&!game_solid(game_tile(g,x,y)))connected=true;
  if(connected)break;id=choose_chunk(g,zone);}
 y-=maps[id].h;decode_into(g,id,y);}
 for(int j=top;j<bottom;j++){set(g,0,j,3);set(g,12,j,3);}}
static void find_rotors(Game*g);
static void refill(Game*g){if(g->y>=40*Q)return;memmove(g->tiles+64*13,g->tiles,64*13);memset(g->tiles,0,64*13);g->y+=64*Q;g->previous_y+=64*Q;g->lava+=64*Q;g->height_score+=64;for(unsigned r=0;r<8;r++){g->rotors[r].y+=64;if(g->rotors[r].y>=128)g->rotors[r].count=0;}for(unsigned i=0;i<ACTORS;i++)if(g->actors[i].active){Actor*a=&g->actors[i];a->y+=64*Q;a->home_y+=64*Q;if(a->y>=128*Q)a->active=0;}fill_arcade(g,0,64);coins_for_area(g,0,64);add_actors(g,0,64);find_rotors(g);}
static void find_rotors(Game*g){for(unsigned y=0;y<g->height;y++)for(unsigned x=0;x<g->width;x++){unsigned type=game_tile(g,x,y);if(type!=109&&type!=110)continue;bool exists=false;for(unsigned r=0;r<8;r++)if(g->rotors[r].count&&g->rotors[r].x==(int)x&&g->rotors[r].y==(int)y)exists=true;if(exists)continue;unsigned slot=0;while(slot<8&&g->rotors[slot].count)slot++;if(slot==8)return;Rotor*r=&g->rotors[slot];r->x=x;r->y=y;r->direction=type==109;r->count=1;r->bricks[0]=(Brick){type,0,0};bool overflow=false;
 for(unsigned head=0;head<r->count&&!overflow;head++){Brick b=r->bricks[head];for(unsigned d=0;d<4;d++){int nx=b.x+dirs[d][0],ny=b.y+dirs[d][1];unsigned t=game_tile(g,x+nx,y+ny);if(abs(nx)>16||abs(ny)>16||x+nx>=g->width||y+ny>=g->height||!(t==3||t==8||t==98||t==7||t==73||t==74||t==85||t==97||(t>=49&&t<=55)||(t>=61&&t<=67)))continue;bool exists=false;for(unsigned j=0;j<r->count;j++)if(r->bricks[j].x==nx&&r->bricks[j].y==ny)exists=true;if(exists)continue;if(r->count==64){overflow=true;break;}r->bricks[r->count++]=(Brick){t,nx,ny};}}
 if(overflow)r->count=0;}}
static void rotate_all(Game*g){for(unsigned n=0;n<8;n++){Rotor*r=&g->rotors[n];if(!r->count)continue;bool carry=false;for(unsigned i=0;i<r->count;i++){Brick b=r->bricks[i];if(!g->dx&&!g->dy&&abs(g->x-(r->x+b.x)*Q)+abs(g->y-(r->y+b.y)*Q)<=Q+8)carry=true;set(g,r->x+b.x,r->y+b.y,0);}
 for(unsigned i=0;i<r->count;i++){Brick*b=&r->bricks[i];int x=b->x,y=b->y;b->x=r->direction?-y:y;b->y=r->direction?x:-x;set(g,r->x+b->x,r->y+b->y,b->type);}
 if(carry){int x=g->x-r->x*Q,y=g->y-r->y*Q;g->x=r->x*Q+(r->direction?-y:y);g->y=r->y*Q+(r->direction?x:-x);if(game_solid(game_tile(g,(g->x+Q/2)/Q,(g->y+Q/2)/Q)))hurt(g);}}}
void game_new(Game*g,uint32_t seed){memset(g,0,sizeof(*g));g->rng=seed?seed:1;g->p.unlocked=1;g->p.owned=1;g->p.shields=1;g->p.free_chest=1;g->queued=-1;g->dirty=1;}
bool game_start(Game*g,unsigned level,bool arcade){if(level>=200)return false;Progress p=g->p;uint32_t seed=g->rng;memset(g,0,sizeof(*g));g->p=p;g->rng=seed?seed:1;g->level=level;g->queued=-1;g->arcade=arcade;g->powerup_wait=300+random32(g)%301;g->x=6*Q;g->y=122*Q;
 if(arcade){g->width=13;g->height=128;fill_arcade(g,0,128);for(int y=120;y<127;y++)for(int x=1;x<12;x++)set(g,x,y,0);for(int x=0;x<13;x++)set(g,x,127,3);g->lava=138*Q;}
 else{const Map*m=&maps[level];g->width=m->w;g->height=m->h;if(g->width>GRID_W||g->height>GRID_H||g->width*g->height>CELLS||map_decode(level,g->tiles,CELLS)<0)return false;for(unsigned y=0;y<g->height;y++)for(unsigned x=0;x<g->width;x++){unsigned t=game_tile(g,x,y);if(t==1)g->x=x*Q,g->y=y*Q;}}
 for(unsigned i=0;i<g->width*g->height;i++){unsigned t=g->tiles[i];if(t==19||t==31||(t>=145&&t<=148))++g->total_dots;}
 coins_for_area(g,0,g->height);add_actors(g,0,g->height);find_rotors(g);g->previous_x=g->x;g->previous_y=g->y;return true;}
static void hurt(Game*g){if(g->dead||g->won||g->invincible)return;if(g->shield){g->shield=0;g->invincible=90;g->dx=g->dy=0;g->queued=-1;}else{g->dead=1;g->dx=g->dy=0;}}
static void pickup(Game*g,int x,int y){unsigned t=game_tile(g,x,y);if(t==19||t==31){++g->dots;g->score+=game_player_level(&g->p)+(g->boost?5:0);if(t==19||g->bonus){++g->coins;if(g->bonus==1&&g->addict&&!--g->addict)g->bonus=0;}set(g,x,y,0);}else if(t==4){++g->stars;g->score+=100;set(g,x,y,0);}else if(t>=145&&t<=148){++g->dots;g->score+=game_player_level(&g->p);unsigned duration=powerup_effects[t-145][g->p.upgrades[t-145]]*60;if(t==145)g->bonus=1,g->addict=powerup_effects[0][g->p.upgrades[0]];else if(t==146)g->freeze=duration;else if(t==147)g->magnet=duration;else g->boost=duration;set(g,x,y,0);}else if(t==2&&!g->arcade){g->won=1;g->dx=g->dy=0;}
 if(t==12){set(g,x,y,0);spawn(g,155,x*Q,y*Q,0,0);}
 if(t==8||((t==113||t==115)&&g->tick%180>=90)||(t==98&&(g->tick%150>=75)&&!g->freeze))hurt(g);
 if((t>=49&&t<=55)||(t>=61&&t<=67)){if(!g->portal_cooldown){unsigned pair=t>=61?t-61:t-49;for(unsigned i=0;i<g->width*g->height;i++){unsigned v=g->tiles[i];if((v==49+pair||v==61+pair)&&(int)i!=y*g->width+x){g->x=(i%g->width)*Q;g->y=(i/g->width)*Q;
 if(v>=61&&g->dx)g->dy=g->dx,g->dx=0;else if(v<61&&g->dy)g->dx=g->dy,g->dy=0;
 if(game_solid(game_tile(g,g->x/Q+g->dx,g->y/Q+g->dy)))g->dx=-g->dx,g->dy=-g->dy;
 g->queued=-1;g->portal_cooldown=30;break;}}}}

 if(t==56||t==57||t==68||t==69){int dx=g->dx,dy=g->dy;if(t==68||t==57){g->dx=-dy;g->dy=-dx;}else{g->dx=dy;g->dy=dx;}}
 if(t==23||t==35||t==47||t==59||t==71||t==83){unsigned target=t+1;for(unsigned i=0;i<g->width*g->height;i++)if(g->tiles[i]==target)spawn(g,150,(i%g->width)*Q,(i/g->width)*Q,0,0);set(g,x,y,0);}
 if(!mask_features[g->p.mask][8]&&(t==13||t==14||t==15)){Actor*a=spawn(g,151,x*Q,y*Q,0,0);if(a)a->timer=90;set(g,x,y,0);}
 if(!mask_features[g->p.mask][7]&&t>=82&&t<=142&&(t-82)%12==0){unsigned enter=t-2,exit=t-1;int ex=-1,ey=-1;for(unsigned i=0;i<g->width*g->height;i++)if(g->tiles[i]==exit)ex=i%g->width,ey=i/g->width;for(unsigned i=0;i<g->width*g->height;i++)if(g->tiles[i]==enter){Actor*a=spawn(g,152,(i%g->width)*Q,(i/g->width)*Q,0,0);if(a&&ex>=0)a->home_x=ex*Q,a->home_y=ey*Q;}set(g,x,y,0);}
 if(t==43&&g->arcade){g->score+=100;g->bonus=0;set(g,x,y,0);}if(t==6){g->bonus=2;set(g,x,y,0);}
}
void game_test_touch(Game*g){pickup(g,g->x/Q,g->y/Q);}
static void shots(Game*g){for(unsigned y=0;y<g->height;y++)for(unsigned x=0;x<g->width;x++){unsigned t=game_tile(g,x,y);int d=t==73?0:t==74?1:t==85?2:t==97?3:-1;if(d>=0)spawn(g,153,(int)x*Q+dirs[d][0]*Q,(int)y*Q+dirs[d][1]*Q,dirs[d][0]*42,dirs[d][1]*42);}}
static void actors(Game*g){for(unsigned i=0;i<ACTORS;i++){Actor*a=&g->actors[i];if(!a->active)continue;if(a->type==154||a->type==155)continue;++a->timer;if(g->freeze&&a->type!=78&&a->type!=79&&a->type!=90&&a->type!=91)continue;int32_t oldx=a->x,oldy=a->y;bool platform=a->type==78||a->type==79||a->type==90||a->type==91;
 if(a->type==150||a->type==151||a->type==152){if(a->type==151&&a->timer<120)continue;int tx=a->type==152?a->home_x:g->x,ty=a->type==152?a->home_y:g->y;if(abs(tx-a->x)>abs(ty-a->y))a->vx=(tx>a->x?1:-1)*14,a->vy=0;else a->vy=(ty>a->y?1:-1)*14,a->vx=0;}
 if(a->type==102){int phase=a->timer%126;a->y=a->home_y-(phase<63?phase:126-phase)*18;}
 else if(a->type==109||a->type==110)continue;
 else{int nx=a->x+a->vx,ny=a->y+a->vy;int cx=(nx+Q/2)/Q,cy=(ny+Q/2)/Q;bool stop=game_solid(game_tile(g,cx,cy));if(stop){if(a->type==153)a->active=0;else if(a->type==152){a->active=0;}else a->vx=-a->vx,a->vy=-a->vy;}else a->x=nx,a->y=ny;}
 if(platform){if(!g->dx&&!g->dy&&g->ride==i+1){g->x+=a->x-oldx;g->y+=a->y-oldy;}}
 else if(a->active&&abs(a->x-g->x)<Q*2/3&&abs(a->y-g->y)<Q*2/3){hurt(g);if(a->type==153)a->active=0;}
 if(a->type==153&&(a->timer>240||a->x<0||a->y<0||a->x>=g->width*Q||a->y>=g->height*Q))a->active=0;
 }}
void game_step(Game*g,int direction,bool shield){if(g->dead||g->won)return;++g->tick;if(g->tick%60==0)++g->p.playtime,g->dirty=1;if(g->shield)--g->shield;if(g->freeze)--g->freeze;if(g->magnet)--g->magnet;if(g->boost)--g->boost;if(g->invincible)--g->invincible;if(g->portal_cooldown)--g->portal_cooldown;if(shield&&!g->shield&&g->p.shields){--g->p.shields;g->shield=1800;g->dirty=1;}
 if(direction>=0&&direction<4){if(!g->dx&&!g->dy){g->x=((g->x+Q/2)/Q)*Q;g->y=((g->y+Q/2)/Q)*Q;g->ride=0;g->dx=dirs[direction][0];g->dy=dirs[direction][1];}else g->queued=direction;}
 g->previous_x=g->x;g->previous_y=g->y;
 if(g->dx||g->dy){int speed=177;g->phase+=7;if(g->phase>=9)g->phase-=9,++speed;while(speed>0&&!g->dead&&!g->won){
  int x=g->x/Q,y=g->y/Q;bool centered=(g->x%Q==0&&g->y%Q==0);
  if(centered){unsigned t=game_tile(g,x+g->dx,y+g->dy);if(game_solid(t)){
   if(t==7)set(g,x+g->dx,y+g->dy,0);
   else if(t==121){set(g,x+g->dx,y+g->dy,0);Actor*a=spawn(g,154,(x+g->dx)*Q,(y+g->dy)*Q,0,0);if(a)a->timer=0;}
   g->dx=g->dy=0;break;}}
  int pos=g->dx?g->x:g->y,sign=g->dx?g->dx:g->dy,mod=pos%Q;
  int distance=mod?(sign>0?Q-mod:mod):Q;int step=speed<distance?speed:distance;
  g->x+=g->dx*step;g->y+=g->dy*step;speed-=step;
  for(unsigned i=0;i<ACTORS;i++){Actor*a=&g->actors[i];if(!a->active||!(a->type==78||a->type==79||a->type==90||a->type==91))continue;
   int dx=a->x-g->x,dy=a->y-g->y;if((g->dx&&abs(dy)<Q/2&&dx*g->dx>=0&&abs(dx)<=Q)||(g->dy&&abs(dx)<Q/2&&dy*g->dy>=0&&abs(dy)<=Q)){g->x=a->x-g->dx*Q;g->y=a->y-g->dy*Q;g->ride=i+1;g->dx=g->dy=0;speed=0;break;}}

  if(step==distance){pickup(g,g->x/Q,g->y/Q);if(g->portal_cooldown==30)break;}
 }}
 for(unsigned i=0;i<ACTORS;i++)if(g->actors[i].active&&g->actors[i].type==155){Actor*a=&g->actors[i];if(abs(g->x-a->x)+abs(g->y-a->y)>Q/2){set(g,a->x/Q,a->y/Q,3);a->active=0;}}
 for(unsigned i=0;i<ACTORS;i++)if(g->actors[i].active&&g->actors[i].type==154){Actor*a=&g->actors[i];if(++a->timer>=120){int x=a->x/Q,y=a->y/Q;if(abs(g->x-a->x)<Q/2&&abs(g->y-a->y)<Q/2)hurt(g);set(g,x,y,121);a->active=0;}}
 if(!g->dx&&!g->dy&&g->queued>=0){int d=g->queued;g->queued=-1;int x=(g->x+Q/2)/Q,y=(g->y+Q/2)/Q;if(!game_solid(game_tile(g,x+dirs[d][0],y+dirs[d][1])))g->dx=dirs[d][0],g->dy=dirs[d][1];}
 if(g->powerup_wait)--g->powerup_wait;
 if(!g->powerup_wait){unsigned weights[4];for(unsigned t=0;t<4;t++)weights[t]=10*(100+mask_features[g->p.mask][2+t])/100;unsigned choice=random32(g)%100,type=0;while(type<4&&choice>=weights[type])choice-=weights[type++];
  if(type<4){unsigned candidate=0,found=0;int px=g->x/Q,py=g->y/Q;for(int y=py-8;y<=py+8;y++)for(int x=px-8;x<=px+8;x++)if(abs(x-px)+abs(y-py)>3&&game_tile(g,x,y)==31){if(random32(g)%++found==0)candidate=y*g->width+x;}if(found)g->tiles[candidate]=145+type;}
  g->powerup_wait=300+random32(g)%301;}
 if(g->tick%66==0&&!g->freeze)shots(g);if(g->tick%126==0&&!g->freeze)rotate_all(g);actors(g);
 if(g->magnet){int x=(g->x+Q/2)/Q,y=(g->y+Q/2)/Q;for(int dy=-2;dy<=2;dy++)for(int dx=-2;dx<=2;dx++)if(abs(dx)+abs(dy)<=2){unsigned t=game_tile(g,x+dx,y+dy);if(t==19||t==31)pickup(g,x+dx,y+dy);}}
 unsigned standing=game_tile(g,(g->x+Q/2)/Q,(g->y+Q/2)/Q);if(standing==8||(standing==98&&g->tick%150>=75&&!g->freeze)||((standing==113||standing==115)&&g->tick%180>=90))hurt(g);
 if(g->arcade){if(!g->freeze&&g->tick>180)g->lava-=7+(g->tick/3600<7?g->tick/3600:7);if(g->lava>g->y+28*Q)g->lava=g->y+28*Q;if(g->y>=g->lava)hurt(g);refill(g);unsigned h=g->height_score+(128-g->y/Q);if(h>g->score)g->score=h;}
}
void game_finish(Game*g){if(g->done||!g->width)return;g->done=1;uint32_t earned=g->coins+(g->won&&g->dots==g->total_dots?25:0);g->p.coins+=earned;g->p.xp+=g->score;if(g->arcade){if(g->score>g->p.best)g->p.best=g->score;}else if(g->won){unsigned s=g->stars>3?3:g->stars;if(s>g->p.stars[g->level])g->p.stars[g->level]=s;if(g->dots==g->total_dots)g->p.dots[g->level/8]|=1u<<(g->level%8);if(g->level+1==g->p.unlocked&&g->p.unlocked<200)++g->p.unlocked;}g->p.mission_counts[2]+=g->dots;g->p.mission_counts[3]+=g->stars;
 if(g->arcade){++g->p.mission_counts[1];if(g->score>g->p.mission_counts[6])g->p.mission_counts[6]=g->score;}
 else if(g->won){if(g->level+1u>g->p.mission_counts[0])g->p.mission_counts[0]=g->level+1;if(g->level+1u>g->p.mission_counts[4])g->p.mission_counts[4]=g->level+1;if(g->dots==g->total_dots)++g->p.mission_counts[5];}
 ++g->p.runs;g->dirty=1;}
bool game_shop(Game*g,unsigned item){if(item>4)return false;if((!item&&g->p.shields>=10)||(item&&g->p.upgrades[item-1]>=9))return false;unsigned cost=item?upgrade_prices[g->p.upgrades[item-1]]:200;if(g->p.coins<cost)return false;g->p.coins-=cost;if(item)++g->p.upgrades[item-1];else ++g->p.shields;g->dirty=1;return true;}

bool game_mission_claim(Game*g,unsigned type){if(type>=8||g->p.mission_step[type]>=7)return false;unsigned step=g->p.mission_step[type];if(g->p.mission_counts[type]<mission_goals[type][step])return false;g->p.coins+=mission_rewards[step];if(type==0||type==4)++g->p.mission_step[type];else{if(step<6)++g->p.mission_step[type];g->p.mission_counts[type]=0;}g->dirty=1;return true;}
bool game_chest(Game*g){bool free=g->p.free_chest||(g->p.next_chest&&g->p.playtime>=g->p.next_chest);if(!free&&g->p.coins<200)return false;if(free)g->p.free_chest=0,g->p.next_chest=g->p.playtime+14400;else g->p.coins-=200;++g->p.chests;++g->p.mission_counts[7];unsigned slot=random32(g)%8,amount=25;g->reward_kind=1;
 if(slot==1)amount=200;else if(slot==5)amount=250;else if(slot==7){if(g->p.shields<10){++g->p.shields;g->reward_kind=2;amount=1;}else amount=200;}
 else if(slot==3){unsigned prize=random32(g)%10;if(prize<2){unsigned picked=0,count=0;for(unsigned m=1;m<16;m++)if(!(g->p.owned&(1u<<m))&&game_player_level(&g->p)>=mask_levels[m]&&random32(g)%++count==0)picked=m;if(count){g->p.owned|=1u<<picked;g->reward_kind=3;amount=picked;}else amount=25*(8+random32(g)%5);}else if(prize>=7&&g->p.shields<10){++g->p.shields;g->reward_kind=2;amount=1;}else amount=25*(8+random32(g)%5);}
 if(g->reward_kind==1)g->p.coins+=amount;g->reward_value=amount;g->combo=45;g->dirty=1;return true;}
