#include "game.h"
#include <string.h>
#include <math.h>
#include <limits.h>
#include <stdlib.h>
#define NONE 255
#define BIT(n) (UINT64_C(1)<<(n))
#define NAV_W 47
#define NAV_H 32
#define NAV_N (NAV_W*NAV_H)
#define CELL 80
/* One reusable reverse-search workspace, never a heap or per-actor map. */
static uint16_t nav_queue[NAV_N],nav_parent[NAV_N];
static uint8_t nav_blocked[NAV_N];
static uint8_t nav_edges[NAV_N];
static bool segment(const Game*,float,float,float,float);
#ifdef MM_TEST
uint32_t production_counts[15];
#endif
static const MartDef* def(const Game*g){return &marts[g->p.market];}
static MartSave* state(Game*g){return &g->p.mart[g->p.market];}
static const MartSave* cstate(const Game*g){return &g->p.mart[g->p.market];}
static unsigned machine_upgrade(const Game*,unsigned);
static uint32_t random32(Game*g){uint32_t x=g->p.rng;x^=x<<13;x^=x>>17;x^=x<<5;return g->p.rng=x?x:1;}
static unsigned randomn(Game*g,unsigned n){return n?random32(g)%n:0;}
static unsigned wish_count(Game*g,unsigned food,unsigned available){unsigned maximum=food_max[food];if(available<=maximum)return 1+randomn(g,available);
 const uint8_t raw_weights[5]={2,3,3,2,1},processed_weights[3]={2,3,1};bool raw=food==1||food==2||food==7||food==8||food==9;const uint8_t*w=raw?raw_weights:processed_weights;unsigned r=randomn(g,raw?11:6),n=1;
 while(r>=w[n-1])r-=w[n++-1];return n>available?available:n;
}
static float distance(float x,float y,float a,float b){float dx=x-a,dy=y-b;return dx*dx+dy*dy;}
static bool active(const Game*g,unsigned n){return n<def(g)->count&&(cstate(g)->active&BIT(n));}
static unsigned level(const Game*g,unsigned u,unsigned k){return u<12?g->p.mart[u?g->p.market:0].upgrade[u][k]:0;}
static unsigned actor_upgrade(const Game*g,const Actor*a){if(a==g->actors)return 0;unsigned id=(unsigned)(a-g->actors)-1;return id<def(g)->worker_count?def(g)->workers[id].upgrade:255;}
unsigned game_capacity(const Game*g,const Actor*a){return a->role==BUYER?CARRY_MAX:3+level(g,actor_upgrade(g,a),1);}
unsigned game_stock_capacity(const Game*g,unsigned n){const PlaceDef*b=&def(g)->places[n];unsigned cap=b->capacity;
 if(b->inputs[0]!=NONE){for(unsigned u=0;u<def(g)->upgrade_count;u++)if(def(g)->upgrade_places[u]!=NONE&&(def(g)->upgrade_places[u]==n||def(g)->upgrade_places[u]==b->inputs[0])){unsigned l=level(g,u,1);if(l)cap=4+2*l;}}
 else for(unsigned j=0;j<def(g)->count;j++)if(def(g)->places[j].inputs[0]==n||def(g)->places[j].inputs[1]==n){unsigned l=level(g,machine_upgrade(g,j),1);if(l)cap=4+2*l;}
 return cap;
}
void game_note(Game*g,const char*s){unsigned n=0;for(;s[n]&&n<sizeof(g->notice)-1;n++)g->notice[n]=s[n];g->notice[n]=0;g->notice_time=150;}
bool game_blocked(const Game*g,float x,float y){const MartDef*m=def(g);if(x<m->minx+12||x>m->maxx-12||y<m->miny+12||y>m->maxy-12)return true;
 for(unsigned i=0;i<m->count;i++){const PlaceDef*b=&m->places[i];if(!active(g,i)||b->poly_count!=4)continue;bool positive=false,negative=false;
  for(unsigned j=0;j<4;j++){Point a=b->poly[j],c=b->poly[(j+1)%4];float cross=(c.x-a.x)*(y-a.y)-(c.y-a.y)*(x-a.x);if(cross>0)positive=true;if(cross<0)negative=true;}
  if(!(positive&&negative))return true;
 }return false;
}
static void move(Game*g,Actor*a,float dx,float dy){float x=a->x+dx,y=a->y+dy;a->moving=0;if(!game_blocked(g,x,a->y)){a->moving|=x!=a->x;a->x=x;}if(!game_blocked(g,a->x,y)){a->moving|=y!=a->y;a->y=y;}if(dx<-.01f)a->flip=1;if(dx>.01f)a->flip=0;}
static Point node_point(int n){return (Point){-1840+(n%NAV_W)*CELL,-1240+(n/NAV_W)*CELL};}
static int node(float x,float y){int xx=(int)floorf((x+1840)/CELL+.5f),yy=(int)floorf((y+1240)/CELL+.5f);if(xx<0)xx=0;if(xx>=NAV_W)xx=NAV_W-1;if(yy<0)yy=0;if(yy>=NAV_H)yy=NAV_H-1;return yy*NAV_W+xx;}
static void map_update(Game*g){memset(nav_edges,0,sizeof(nav_edges));for(unsigned n=0;n<NAV_N;n++){Point p=node_point(n);nav_blocked[n]=game_blocked(g,p.x,p.y);}
 for(unsigned n=0;n<NAV_N;n++)if(!nav_blocked[n]){Point p=node_point(n);if(n%NAV_W+1<NAV_W&&!nav_blocked[n+1]){Point q=node_point(n+1);if(segment(g,p.x,p.y,q.x,q.y))nav_edges[n]|=2,nav_edges[n+1]|=1;}
  if(n+NAV_W<NAV_N&&!nav_blocked[n+NAV_W]){Point q=node_point(n+NAV_W);if(segment(g,p.x,p.y,q.x,q.y))nav_edges[n]|=8,nav_edges[n+NAV_W]|=4;}}
 for(unsigned i=1;i<ACTOR_COUNT;i++)g->actors[i].nav_age=0;}
static bool segment(const Game*g,float x,float y,float a,float b){float dx=a-x,dy=b-y;unsigned steps=(unsigned)((fabsf(dx)+fabsf(dy))/15)+1;for(unsigned i=1;i<=steps;i++)if(game_blocked(g,x+dx*i/steps,y+dy*i/steps))return false;return true;}
static int nearby_node(const Game*g,float x,float y){int best=-1;float score=1e30f;int start=node(x,y);for(int yy=-2;yy<=2;yy++)for(int xx=-2;xx<=2;xx++){int n=start+yy*NAV_W+xx;if(n<0||n>=NAV_N||abs((n%NAV_W)-(start%NAV_W))>2||nav_blocked[n])continue;Point p=node_point(n);float d=distance(x,y,p.x,p.y);if(d<score&&segment(g,x,y,p.x,p.y))score=d,best=n;}return best;}
static int find_next(Game*g,Actor*a){int start=nearby_node(g,a->x,a->y),end=nearby_node(g,a->gx,a->gy);if(start<0||end<0)return -1;if(start==end)return start;
 for(unsigned i=0;i<NAV_N;i++)nav_parent[i]=UINT16_MAX;unsigned head=0,tail=0;nav_queue[tail++]=(uint16_t)end;nav_parent[end]=(uint16_t)end;
 while(head<tail&&nav_parent[start]==UINT16_MAX){unsigned n=nav_queue[head++];const int offsets[4]={-1,1,-NAV_W,NAV_W};for(unsigned k=0;k<4;k++){if(!(nav_edges[n]&(1u<<k)))continue;unsigned j=n+offsets[k];if(nav_parent[j]!=UINT16_MAX)continue;nav_parent[j]=(uint16_t)n;nav_queue[tail++]=(uint16_t)j;}}
 if(nav_parent[start]==UINT16_MAX)return -1;Point p=node_point(nav_parent[start]);return segment(g,a->x,a->y,p.x,p.y)?nav_parent[start]:start;
}
static void target(Game*g,Actor*a,float x,float y){/* Find a walkable approach to an interaction point. */
 if(game_blocked(g,x,y)){float best=1e30f,bx=x,by=y;for(int r=20;r<=180;r+=20)for(int i=0;i<16;i++){float angle=i*(6.2831853f/16),xx=x+cosf(angle)*r,yy=y+sinf(angle)*r;if(game_blocked(g,xx,yy))continue;float d=r*r+distance(a->x,a->y,xx,yy)*.02f;if(d<best)best=d,bx=xx,by=yy;}x=bx;y=by;}
 if(distance(a->gx,a->gy,x,y)>100)a->nav_age=0;a->gx=x;a->gy=y;
}
static void place_target(Game*g,Actor*a,unsigned n){const PlaceDef*b=&def(g)->places[n];Point p=b->work;if(b->flags&RETAIL){p=b->pos;p.y+=50;}target(g,a,p.x,p.y);a->target=(uint8_t)n;}
static bool arrive(Game*g,Actor*a,float speed){a->moving=0;float dx=a->gx-a->x,dy=a->gy-a->y,d=dx*dx+dy*dy;if(d<144)return true;
 float tx=a->gx,ty=a->gy;
 /* Purchased fixtures are static. Reuse clear routes and grid waypoints until
    a goal changes or a purchase invalidates the map; avoid tracing long rays
    through every fixture for every employee on every simulation tick. */
 if(!a->nav_age){a->nav=segment(g,a->x,a->y,tx,ty)?-2:(int16_t)find_next(g,a);a->nav_age=60;}
 if(a->nav!=-2){if(a->nav<0){--a->nav_age;a->failures++;return false;}Point p=node_point(a->nav);tx=p.x;ty=p.y;if(distance(a->x,a->y,tx,ty)<100){a->nav_age=0;return false;}}
 dx=tx-a->x;dy=ty-a->y;float len=sqrtf(dx*dx+dy*dy),step=speed/TICKS;if(len<step)step=len;move(g,a,dx/len*step,dy/len*step);if(!a->moving)a->nav_age=0;else if(a->nav_age)--a->nav_age;return false;
}
static void nudge(Game*g,Actor*a){if(!game_blocked(g,a->x,a->y))return;float best=1e30f,x=a->x,y=a->y;for(int r=12;r<260;r+=12)for(int k=0;k<16;k++){float xx=a->x+r*cosf(k*.392699f),yy=a->y+r*sinf(k*.392699f);if(!game_blocked(g,xx,yy)&&r<best)best=r,x=xx,y=yy;}a->x=x;a->y=y;}
void game_unlock(Game*g){const MartDef*m=def(g);MartSave*s=state(g);g->offered=0;s->progress=0;for(unsigned i=0;i<m->order_count;i++){if(active(g,m->order[i]))s->progress++;else{g->offered|=BIT(m->order[i]);break;}}
 for(unsigned n=0;n<m->count;n++)if(active(g,n))g->offered|=m->places[n].unlock;g->offered&=~s->active;
}
static void activate(Game*g,unsigned n){MartSave*s=state(g);if(n>=def(g)->count||active(g,n))return;const PlaceDef*b=&def(g)->places[n];s->active|=BIT(n);s->stamina[n]=b->stamina;s->stock[n]=(b->flags&PRODUCER)&&b->inputs[0]==NONE?b->capacity:0;s->paid[n]=b->price;if(b->activate!=NONE)activate(g,b->activate);}
bool game_buy(Game*g,unsigned n){if(n>=def(g)->count||!(g->offered&BIT(n)))return false;MartSave*s=state(g);unsigned price=def(g)->places[n].price,remain=price-s->paid[n];if(remain&&!g->p.coins)return false;unsigned pay=(remain+9)/10;if(pay>g->p.coins)pay=g->p.coins;g->p.coins-=pay;s->paid[n]+=pay;g->dirty=1;
 if(s->paid[n]==price){activate(g,n);game_unlock(g);map_update(g);for(unsigned a=0;a<ACTOR_COUNT;a++)if(g->actors[a].live)nudge(g,&g->actors[a]);game_note(g,(def(g)->places[n].flags&EMPLOYEE)?"New employee hired!":"Your mart is growing!");}return true;
}
static unsigned machine_upgrade(const Game*g,unsigned n){const PlaceDef*b=&def(g)->places[n];for(unsigned u=0;u<def(g)->upgrade_count;u++)if(def(g)->upgrade_places[u]!=NONE&&(def(g)->upgrade_places[u]==n||def(g)->upgrade_places[u]==b->inputs[0]))return u;return 255;}
static unsigned machine_stamina(const Game*g,unsigned n){unsigned u=machine_upgrade(g,n),l=level(g,u,0);return l?4+8*l:def(g)->places[n].stamina;}
Point game_repair_position(const Game*g,unsigned n){const PlaceDef*b=&def(g)->places[n];Point p=def(g)->places[b->inputs[0]].pos;if(b->food==3)p.x+=43,p.y+=58;else if(b->food==4)p.x+=77,p.y+=50;else p.y+=10;return p;}
static unsigned stamina_max(const Game*g,const Actor*a){return 6+2*level(g,actor_upgrade(g,a),0);}
static void start_market(Game*g,unsigned market){g->p.market=(uint8_t)market;MartSave*s=state(g);const MartDef*m=def(g);memset(g->actors,0,sizeof(g->actors));
 if(!s->started){s->started=1;for(unsigned i=0;i<MAX_WORKERS;i++)s->worker_stamina[i]=6;if(market){activate(g,m->cash);s->cash=25;}}
 Actor*a=g->actors;*a=(Actor){.x=m->spawn.x,.y=m->spawn.y,.role=PLAYER,.live=1,.skin=0,.hat=g->p.hat,.nav=-1,.target=NONE};
 if(g->p.x||g->p.y)a->x=g->p.x,a->y=g->p.y;a->count=g->p.count;memcpy(a->carry,g->p.carry,CARRY_MAX);g->camera_x=a->x;g->camera_y=a->y;g->customer_clock=60;g->rowdy_clock=70*TICKS;g->selected_place=NONE;g->market_armed=1;memset(g->dropped,0,sizeof(g->dropped));game_unlock(g);map_update(g);nudge(g,a);
 for(unsigned i=0;i<m->worker_count;i++){Actor*w=&g->actors[i+1];*w=(Actor){.x=m->places[m->workers[i].place].pos.x,.y=m->places[m->workers[i].place].pos.y,.role=i+1u==m->worker_count?MANAGER:WORKER,.target=NONE,.destination=NONE,.hat=m->workers[i].hat,.skin=7,.stamina=s->worker_stamina[i],.nav=-1};}
}
void game_new(Game*g,uint32_t seed){memset(g,0,sizeof(*g));g->p.rng=seed?seed:1;g->p.coins=35;g->p.hats=1;start_market(g,0);game_note(g,"Walk onto the $10 checkout pad.");}
void game_restore(Game*g){Progress p=g->p;memset(g,0,sizeof(*g));g->p=p;start_market(g,p.market);}
void game_snapshot(Game*g){Actor*a=g->actors;g->p.x=(int16_t)a->x;g->p.y=(int16_t)a->y;g->p.count=a->count;memcpy(g->p.carry,a->carry,CARRY_MAX);for(unsigned i=0;i<def(g)->worker_count;i++)state(g)->worker_stamina[i]=g->actors[i+1].stamina;}
bool game_market(Game*g,unsigned m){if(m>=2||(m==1&&!g->p.mart[0].started)|| (m==1&&!(g->p.mart[0].active&BIT(31))))return false;if(m==g->p.market)return true;game_snapshot(g);g->p.x=g->p.y=0;/* Carry is retained, never cloned into two saves. */start_market(g,m);g->dirty=1;return true;}
static bool take(Game*g,Actor*a,unsigned n){MartSave*s=state(g);const PlaceDef*b=&def(g)->places[n];if(!active(g,n)||!s->stock[n]||a->count>=game_capacity(g,a)||!b->food)return false;
 --s->stock[n];a->carry[a->count++]=b->food;g->dirty=1;return true;}
static bool drop(Game*g,Actor*a,unsigned n){MartSave*s=state(g);const PlaceDef*b=&def(g)->places[n];if(!active(g,n)||s->stock[n]>=game_stock_capacity(g,n)||!b->food)return false;
 for(unsigned i=0;i<a->count;i++)if(a->carry[i]==b->food){--a->count;memmove(&a->carry[i],&a->carry[i+1],a->count-i);++s->stock[n];g->dirty=1;return true;}return false;
}
static void player_interact(Game*g){Actor*a=g->actors;const MartDef*m=def(g);MartSave*s=state(g);if(a->wait){--a->wait;return;}g->selected_place=NONE;
 if(a->moving)g->market_armed=1;
 for(unsigned n=0;n<m->count;n++)if(active(g,n)&&(m->places[n].flags&NEXT_MART)){Point p=m->places[n].pos;p.x-=g->p.market?0:200;p.y+=g->p.market?50:10;if(g->market_armed&&!a->moving&&distance(a->x,a->y,p.x,p.y)<4000)g->market_request=1,g->market_armed=0;}
 for(unsigned i=0;i<20;i++)if(g->dropped[i].food&&a->count<game_capacity(g,a)&&distance(a->x,a->y,g->dropped[i].pos.x,g->dropped[i].pos.y)<2200){a->carry[a->count++]=g->dropped[i].food;g->dropped[i].food=0;a->wait=9;g->dirty=1;return;}
 /* Checkout both packs customers and exposes earned cash; it is never passive income. */
 const PlaceDef*cash=&m->places[m->cash];if(active(g,m->cash)&&distance(a->x,a->y,cash->work.x,cash->work.y)<cash->radius){g->selected_place=m->cash;if(s->cash){unsigned take=s->cash>20?20:s->cash;s->cash-=take;g->p.coins+=take;a->wait=4;g->dirty=1;return;}}
 for(unsigned i=1;i<=m->worker_count;i++){Actor*w=&g->actors[i];if(w->live&&w->phase==RESTING&&distance(a->x,a->y,w->x,w->y)<9000){w->phase=ENTER;w->stamina=(uint8_t)stamina_max(g,w);w->wait=30;game_note(g,"Back to work!");}}
 Actor*r=&g->actors[ACTOR_COUNT-1];if(r->live&&distance(a->x,a->y,r->x,r->y)<10000){r->phase=LEAVING;target(g,r,m->entry.x,m->entry.y+250);game_note(g,"Troublemaker sent away.");}
 for(unsigned n=0;n<m->count;n++){const PlaceDef*b=&m->places[n];if(active(g,n)&&b->inputs[0]!=NONE&&!s->stamina[n]){Point p=game_repair_position(g,n);if(distance(a->x,a->y,p.x,p.y)<(b->food<5?7000:18000)){s->stamina[n]=(uint8_t)machine_stamina(g,n);game_note(g,b->food<5?"Animal woken up!":"Machine repaired!");a->wait=30;g->dirty=1;return;}}}
 for(unsigned n=0;n<m->count;n++){const PlaceDef*b=&m->places[n];if((g->offered&BIT(n))&&distance(a->x,a->y,b->buy.x,b->buy.y)<3600){g->selected_place=n;if(game_buy(g,n))a->wait=4;return;}}
 float closest=1e30f;int chosen=-1;unsigned action=0;
 for(unsigned n=0;n<m->count;n++){const PlaceDef*b=&m->places[n];if(!active(g,n)||b->flags&(CASHIER|EMPLOYEE|NEXT_MART))continue;float d=distance(a->x,a->y,b->work.x,b->work.y);if(d>b->radius)continue;unsigned act=0;
  if((b->flags&SHELF)&&s->stock[n]<game_stock_capacity(g,n))for(unsigned k=0;k<a->count;k++)if(a->carry[k]==b->food)act=2;
  if((b->flags&PRODUCER)&&s->stock[n]&&a->count<game_capacity(g,a))act=1;
  if(act&&d<closest)chosen=(int)n,closest=d,action=act;
 }
 if(chosen>=0){g->selected_place=(uint8_t)chosen;if(action==1)take(g,a,chosen);else if(action==2)drop(g,a,chosen);a->wait=9;}
 if(a->count&&distance(a->x,a->y,m->trash.x,m->trash.y)<4000){--a->count;g->dirty=1;a->wait=9;}
}
uint16_t game_upgrade_price(const Game*g,unsigned u,unsigned k){const MartDef*m=u?def(g):&marts[0];if(u>=m->upgrade_count||k>=3)return UINT16_MAX;unsigned l=level(g,u,k);if(l>=m->upgrade_max[u][k])return UINT16_MAX;return m->prices[u][k][l];}
bool game_upgrade(Game*g,unsigned u,unsigned k){const MartDef*m=def(g);if(u>=m->upgrade_count||k>=3||(u&& !active(g,m->upgrade_places[u])))return false;uint16_t price=game_upgrade_price(g,u,k);if(price==UINT16_MAX||g->p.coins<price)return false;g->p.coins-=price;++g->p.mart[u?g->p.market:0].upgrade[u][k];g->dirty=1;return true;}
bool game_hat(Game*g,unsigned h){if(h>=21)return false;if(!(g->p.hats&(1u<<h))){unsigned price=hat_prices[h];if(!price){unsigned cost=hat_tokens[h];if(g->p.tokens<cost)return false;g->p.tokens-=cost;}else if(g->p.coins<price)return false;else g->p.coins-=price;g->p.hats|=1u<<h;}g->p.hat=(uint8_t)h;g->actors[0].hat=(uint8_t)h;g->dirty=1;return true;}
bool game_reward(Game*g,unsigned type){if(type>3||g->p.reward_cooldown)return false;g->p.reward_cooldown=40;if(type<3)g->p.boosts[type]=120;else{unsigned progress=state(g)->progress;g->p.coins+=progress<8?10:progress<12?20:80;}if(g->p.tokens<255)++g->p.tokens;g->dirty=1;return true;}
static void produce(Game*g){MartSave*s=state(g);for(unsigned n=0;n<def(g)->count;n++){const PlaceDef*b=&def(g)->places[n];if(!(b->flags&PRODUCER)||!active(g,n)||s->stock[n]>=game_stock_capacity(g,n))continue;bool machine=b->inputs[0]!=NONE;if(machine&& !s->stamina[n]&&!g->p.boosts[2])continue;if(machine&&(!s->stock[b->inputs[0]]||(b->inputs[1]!=NONE&&!s->stock[b->inputs[1]])))continue;
  unsigned period=b->period;if(machine){unsigned speed=level(g,machine_upgrade(g,n),2);if(speed)period=12-2*speed;}
  if(++s->production[n]>=period*TICKS){s->production[n]=0;++s->stock[n];if(machine){--s->stock[b->inputs[0]];if(b->inputs[1]!=NONE)--s->stock[b->inputs[1]];if(s->stamina[n]&&!g->p.boosts[2])--s->stamina[n];}g->dirty=1;
#ifdef MM_TEST
   ++production_counts[b->food];
#endif
  }
 }}
static unsigned source_for(Game*g,unsigned dest){unsigned food=def(g)->places[dest].food;int found=-1;unsigned best=0;for(unsigned n=0;n<def(g)->count;n++)if(active(g,n)&&(def(g)->places[n].flags&PRODUCER)&&def(g)->places[n].food==food&&state(g)->stock[n]>best)found=(int)n,best=state(g)->stock[n];return found<0?NONE:(unsigned)found;}
static unsigned worker_destination(Game*g,unsigned wi){uint64_t mask=def(g)->workers[wi].destinations;int best=-1;float score=-1;for(unsigned n=0;n<def(g)->count;n++)if((mask&BIT(n))&&active(g,n)){unsigned capacity=game_stock_capacity(g,n),have=state(g)->stock[n];if(have>=capacity||source_for(g,n)==NONE)continue;unsigned reserved=0;for(unsigned w=1;w<=def(g)->worker_count;w++)if(w!=wi+1&&g->actors[w].live&&g->actors[w].destination==n)reserved+=g->actors[w].count?g->actors[w].count:3;if(have+reserved>=capacity)continue;float priority=(float)(capacity-have-reserved)/capacity;if(priority>score)score=priority,best=(int)n;}return best<0?NONE:(unsigned)best;}
static void worker(Game*g,unsigned wi){Actor*a=&g->actors[wi+1];const WorkerDef*w=&def(g)->workers[wi];a->live=active(g,w->place);if(!a->live)return;
 if(a->phase==RESTING){a->moving=0;return;}if(a->wait){--a->wait;a->moving=0;return;}float speed=144+36*level(g,w->upgrade,2);if(g->p.boosts[1])speed=324;
 if(wi==0){/* A cashier stays at the till; packing still runs at its original rate. */const PlaceDef*b=&def(g)->places[def(g)->cash];target(g,a,b->pos.x-150,b->pos.y+10);arrive(g,a,speed);return;}
 if(a->role==MANAGER){Actor*r=&g->actors[ACTOR_COUNT-1];if(r->live&&r->phase!=LEAVING){target(g,a,r->x,r->y);if(arrive(g,a,speed)){r->phase=LEAVING;target(g,r,def(g)->entry.x,def(g)->entry.y+250);}return;}
  for(unsigned i=1;i<def(g)->worker_count;i++)if(g->actors[i].live&&g->actors[i].phase==RESTING){Actor*q=&g->actors[i];target(g,a,q->x,q->y);if(arrive(g,a,speed)){q->phase=ENTER;q->stamina=(uint8_t)stamina_max(g,q);q->wait=30;}return;}
  for(unsigned n=0;n<def(g)->count;n++)if(active(g,n)&&def(g)->places[n].inputs[0]!=NONE&&!state(g)->stamina[n]){Point p=game_repair_position(g,n);target(g,a,p.x,p.y);if(arrive(g,a,speed)){state(g)->stamina[n]=(uint8_t)machine_stamina(g,n);a->wait=30;}return;}
  Point p=def(g)->places[w->place].pos;target(g,a,p.x,p.y);arrive(g,a,speed);return;
 }
 if(a->destination==NONE){unsigned dest=NONE;if(a->count){for(unsigned n=0;n<def(g)->count;n++)if((w->destinations&BIT(n))&&active(g,n)&&def(g)->places[n].food==a->carry[0]&&state(g)->stock[n]<game_stock_capacity(g,n)){dest=n;break;}
   if(dest==NONE){target(g,a,def(g)->trash.x,def(g)->trash.y);if(arrive(g,a,speed)){--a->count;memmove(a->carry,a->carry+1,a->count);a->wait=9;}return;}
  }else dest=worker_destination(g,wi);if(dest==NONE){a->moving=0;return;}a->destination=(uint8_t)dest;a->target=NONE;}
 unsigned dest=a->destination;if(!active(g,dest)||state(g)->stock[dest]>=game_stock_capacity(g,dest)){a->destination=NONE;return;}
 if(a->count){place_target(g,a,dest);if(arrive(g,a,speed)){if(drop(g,a,dest))a->wait=9;else a->destination=NONE;if(!a->count){a->destination=NONE;if(a->stamina&&!g->p.boosts[1])--a->stamina;if(!a->stamina){a->phase=RESTING;a->moving=0;}}}return;}
 unsigned src=source_for(g,dest);if(src==NONE){a->destination=NONE;return;}place_target(g,a,src);if(arrive(g,a,speed)){unsigned cap=game_capacity(g,a),gap=game_stock_capacity(g,dest)-state(g)->stock[dest];if(cap>gap)cap=gap;while(a->count<cap&&take(g,a,src)){}a->wait=9;}
}
static void new_buyer(Game*g,Actor*a){const MartDef*m=def(g);uint8_t types[8];unsigned count=0;for(unsigned n=0;n<m->count;n++)if(active(g,n)&&(m->places[n].flags&RETAIL))types[count++]=m->places[n].food;if(!count)return;
 *a=(Actor){.x=m->entry.x,.y=m->entry.y+120,.role=BUYER,.phase=SHOPPING,.live=1,.skin=1+randomn(g,6),.target=NONE,.nav=-1};
 unsigned hat=randomn(g,10);a->hat=hat?(uint8_t)(hat+11):0;
 for(unsigned i=count;i>1;i--){unsigned k=randomn(g,i);uint8_t t=types[i-1];types[i-1]=types[k];types[k]=t;}unsigned maxwish=count>6?6:count,wishes=1+randomn(g,maxwish);const unsigned limits[]={4,5,6,8,10,12,12,12};unsigned remain=wishes+randomn(g,limits[count-1]-wishes+1);
 if(g->p.market==0&&!g->p.total_sales)wishes=1,remain=1;
 for(unsigned i=0;i<wishes&&remain;i++){unsigned max=food_max[types[i]];if(max>remain)max=remain;unsigned n=i+1==wishes?max:wish_count(g,types[i],remain);a->wish[types[i]]=(uint8_t)n;remain-=n;}
}
static unsigned shelf_for(const Game*g,unsigned food){for(unsigned n=0;n<def(g)->count;n++)if(active(g,n)&&(def(g)->places[n].flags&RETAIL)&&def(g)->places[n].food==food)return n;return NONE;}
static void buyer(Game*g,unsigned ai){Actor*a=&g->actors[ai];if(!a->live)return;
 if(a->phase==PACKING){if(a->wait)--a->wait;if(a->wait==30&&a->bill){state(g)->cash+=a->bill*(g->p.boosts[0]?2:1);g->p.total_sales+=a->bill;a->bill=0;g->dirty=1;}if(!a->wait)a->phase=LEAVING,a->count=0;return;}
 if(a->wait){--a->wait;return;}
 if(a->phase==SHOPPING){unsigned n=NONE;for(unsigned f=1;f<=14;f++)if(a->wish[f]){n=shelf_for(g,f);if(n!=NONE)break;}if(n==NONE){a->phase=QUEUE;a->target=NONE;return;}
  place_target(g,a,n);if(arrive(g,a,144)&&state(g)->stock[n]){unsigned f=def(g)->places[n].food;--state(g)->stock[n];--a->wish[f];if(a->count<CARRY_MAX)a->carry[a->count++]=(uint8_t)f;a->bill+=food_prices[f];a->wait=30;g->dirty=1;}return;
 }
 if(a->phase==QUEUE){unsigned rank=0;for(unsigned i=8;i<ai;i++)if(g->actors[i].live&&g->actors[i].phase==QUEUE)rank++;const PlaceDef*b=&def(g)->places[def(g)->cash];target(g,a,b->pos.x+70+(int)rank*50,b->pos.y+85-(int)rank*8);
  if(arrive(g,a,144)&&rank==0){Actor*player=g->actors;bool packed=active(g,def(g)->workers[0].place)||distance(player->x,player->y,b->work.x,b->work.y)<b->radius;for(unsigned i=8;i<ACTOR_COUNT-1;i++)if(g->actors[i].live&&g->actors[i].phase==PACKING)packed=false;if(packed){a->wait=(uint16_t)(9*a->count+60);a->phase=PACKING;g->dirty=1;}}return;
 }
 if(a->phase==LEAVING){target(g,a,def(g)->entry.x,def(g)->entry.y+200);if(arrive(g,a,144)){a->live=0;g->customer_clock=120;}}
}
static void rowdy(Game*g){Actor*a=&g->actors[ACTOR_COUNT-1];if(!a->live){if(state(g)->progress<10)return;if(g->rowdy_clock)--g->rowdy_clock;else{unsigned shelves[8],cnt=0;for(unsigned n=0;n<def(g)->count;n++)if(active(g,n)&&(def(g)->places[n].flags&RETAIL))shelves[cnt++]=n;if(!cnt)return;*a=(Actor){.x=def(g)->entry.x,.y=def(g)->entry.y,.live=1,.role=ROWDY,.skin=1+randomn(g,6),.target=(uint8_t)shelves[randomn(g,cnt)],.nav=-1};place_target(g,a,a->target);}return;}
 if(a->phase==LEAVING){if(arrive(g,a,200))a->live=0,g->rowdy_clock=70*TICKS;return;}
 if(arrive(g,a,144)){if(a->wait)--a->wait;else{unsigned n=a->target;if(state(g)->stock[n]){const PlaceDef*b=&def(g)->places[n];for(unsigned i=0;i<20;i++)if(!g->dropped[i].food){Point p={b->pos.x+(int)randomn(g,301)-150,b->pos.y-160-(int)randomn(g,81)};if(!game_blocked(g,p.x,p.y)){g->dropped[i]=(Dropped){p,b->food};--state(g)->stock[n];g->dirty=1;}break;}}a->wait=180;}}
}
void game_step(Game*g,int dx,int dy){++g->tick;if(g->notice_time)--g->notice_time;Actor*a=g->actors;float speed=360;if(dx&&dy)speed*=.70710678f;move(g,a,dx*speed/TICKS,dy*speed/TICKS);player_interact(g);produce(g);
 for(unsigned i=0;i<def(g)->worker_count;i++)worker(g,i);
 if(g->customer_clock)--g->customer_clock;else{uint8_t gates1[8]={0,0,0,7,12,15,255,255},gates2[8]={13,13,13,15,34,22,36,32};const uint8_t*gates=g->p.market?gates2:gates1;
  for(unsigned i=8;i<ACTOR_COUNT-1;i++)if(gates[i-8]!=255&&active(g,gates[i-8])&&!g->actors[i].live){new_buyer(g,&g->actors[i]);g->customer_clock=180;break;}}
 for(unsigned i=8;i<ACTOR_COUNT-1;i++)buyer(g,i);rowdy(g);
 if(g->tick%60==0){++g->p.seconds;for(unsigned i=0;i<3;i++)if(g->p.boosts[i])--g->p.boosts[i];if(g->p.reward_cooldown)--g->p.reward_cooldown;}
 /* Camera smoothing uses elapsed simulation ticks, so it is display-rate independent. */
 g->camera_x+=(a->x-g->camera_x)*.12f;g->camera_y+=(a->y-g->camera_y)*.12f;
}
