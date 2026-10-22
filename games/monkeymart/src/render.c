#include "render.h"
#include "assets.h"
#include "canvas.h"
#include <math.h>
#define INK RGB(54,35,47)
#define CREAM RGB(255,252,223)
#define GREEN RGB(42,154,87)
#define GOLD RGB(255,203,44)
#define MUTED RGB(116,103,108)
#define BIT(n) (UINT64_C(1)<<(n))
typedef struct {int16_t x,y,z;uint16_t sprite,w,h;uint8_t flip;} Draw;
static Draw draws[230];static unsigned draw_count;
static int cx,cy;
static int screenx(float x){return 160+(int)floorf((x-cx)/3);}
static int screeny(float y){return 133-(int)floorf((y-cy)/3);}
static void sprite(unsigned id,int x,int y,bool flip){if(!id)return;const Image*im=sprites[id];if(!flip){image(im,x,y,im->w,im->h);return;}int y0=y<band?band:y,y1=y+im->h>band+8?band+8:y+im->h;for(int py=y0;py<y1;py++)for(unsigned xx=0;xx<im->w;xx++){int px=x+xx;if((unsigned)px>=320)continue;unsigned i=(py-y)*im->w+im->w-1-xx,v=im->pixels[i/2],n=i&1?v&15:v>>4;if(n)pixel(px,py,im->palette[n]);}}
static void queue(unsigned id,int x,int y,int z,bool flip){if(!id||draw_count>=230)return;const Image*im=sprites[id];if(x+im->w<0||x>=320||y+im->h<0||y>=240)return;draws[draw_count++]=(Draw){x,y,z,(uint16_t)id,im->w,im->h,(uint8_t)flip};}
static void object(unsigned id,Point p,int z){if(!id)return;const Image*im=sprites[id];queue(id,screenx(p.x)-im->w/2,screeny(p.y)-im->h/2,z,false);}
static void floor_band(unsigned market){const Background*b=&backgrounds[market];int ox=160-cx/3-b->w/2-(market?3:0),oy=133+cy/3-b->h/2-(market?27:0);
 rect(0,band,320,8,RGB(0,190,105));for(int y=band;y<band+8;y++){int sy=y-oy;if(sy<0||sy>=b->h)continue;const uint8_t*p=b->data+b->offset[sy];int x=ox;while(x<320&&x<ox+b->w){unsigned n=*p++;if(n>=128){unsigned run=n-127,c=*p++;if(x+(int)run>0)rect(x,y,run,1,b->palette[c]);x+=run;}else{unsigned run=n+1;for(unsigned i=0;i<run;i++,x++){unsigned c=(i&1)?p[i/2]&15:p[i/2]>>4;if((unsigned)x<320)pixel(x,y,b->palette[c]);}p+=(run+1)/2;}}}
}
static void world_queue(const Game*g){const MartDef*m=&marts[g->p.market];const MartSave*s=&g->p.mart[g->p.market];draw_count=0;
 for(unsigned i=0;i<m->decor_count;i++){const Decor*d=&m->decor[i];if(d->when&&!(s->active&BIT(d->when-1)))continue;unsigned before=draw_count;object(d->sprite,d->pos,d->depth);if(d->scale!=100&&draw_count>before){Draw*a=&draws[draw_count-1];unsigned w=a->w*d->scale/100,h=a->h*d->scale/100;a->x-=(int)(w-a->w)/2;a->y-=(int)(h-a->h)/2;a->w=(uint16_t)w;a->h=(uint16_t)h;}}
 object(S_GAME__TRASH1,m->trash,-m->trash.y);
 for(unsigned n=0;n<m->count;n++){const PlaceDef*b=&m->places[n];if(!(s->active&BIT(n)))continue;Point p=b->pos;int z=-p.y;if(!(b->flags&HIDDEN)&&!(b->flags&EMPLOYEE))object(b->sprite,p,z);
  if(b->sprite2)object(b->sprite2,b->second,z+75);
  if(b->inputs[0]!=255){unsigned f=b->food;if(f==3||f==4){Point a=game_repair_position(g,n);const Pose*pose=&animals[f==4][s->stamina[n]?((g->tick/30)&1):2][(g->tick/30)&1];queue(pose->sprite,screenx(a.x)+pose->x,screeny(a.y)+pose->y,-a.y,false);}}
  if(b->food&&s->stock[n]){unsigned count=s->stock[n];if(count>8)count=8;for(unsigned i=0;i<count;i++){Point q={p.x-35+(i%2)*50+(i/2)*10,p.y-10+(i/2)*30};if(b->flags&PRODUCER&&b->inputs[0]==255){q.x=p.x+(i%2)*25-12;q.y=p.y+30+(i/2)*22;}unsigned id=food_icons[b->food];object(id,q,z+2+(int)i);}}
 }
 for(unsigned i=0;i<20;i++)if(g->dropped[i].food)object(food_icons[g->dropped[i].food],g->dropped[i].pos,-g->dropped[i].pos.y);
 for(unsigned ai=0;ai<ACTOR_COUNT;ai++){const Actor*a=&g->actors[ai];if(!a->live)continue;unsigned anim=a->phase==RESTING?4:a->role==ROWDY&&a->phase!=LEAVING?5:a->count?(a->moving?3:2):(a->moving?1:0),frame=anim==1||anim==3?(g->tick/4)%8:(g->tick/30)%2;const Pose*p=&poses[a->skin%8][anim][frame];int x=screenx(a->x),y=screeny(a->y),z=-(int)a->y;
  int ox=a->flip?-p->x-sprites[p->sprite]->w:p->x;queue(p->sprite,x+ox,y+p->y,z,a->flip);
  unsigned hat=0;if(ai==0){if(a->hat)hat=hats[a->hat+5];}else if(a->role==WORKER||a->role==MANAGER)hat=hats[a->hat];else if(a->hat>=12&&a->hat<=20)hat=hats[a->hat-5];
  if(hat){const Image*im=sprites[hat];queue(hat,x-im->w/2,y-32-im->h/2,z+1,a->flip);}
  for(unsigned i=0;i<a->count;i++){unsigned food=a->carry[i],id=food_icons[food];if(!id)continue;const Image*im=sprites[id];queue(id,x+8-im->w/2,y-12-i*5-im->h,z+2+i,a->flip);}
 }
 for(unsigned i=1;i<draw_count;i++){Draw a=draws[i];unsigned j=i;while(j&&draws[j-1].z>a.z){draws[j]=draws[j-1];j--;}draws[j]=a;}
}
static void pill(int x,int y,int w,int h,uint16_t c){rect(x+5,y,w-10,h,c);rect(x,y+5,w,h-10,c);circle(x+5,y+5,5,c);circle(x+w-6,y+5,5,c);circle(x+5,y+h-6,5,c);circle(x+w-6,y+h-6,5,c);}
static void amount(int x,int y,uint32_t n,uint16_t color){number(x,y,n,color,1,false);}
static void pads(const Game*g){const MartDef*m=&marts[g->p.market];const MartSave*s=&g->p.mart[g->p.market];for(unsigned i=0;i<m->count;i++){
 const PlaceDef*b=&m->places[i];if(g->offered&BIT(i)){int x=screenx(b->buy.x),y=screeny(b->buy.y);if(x<-40||x>360||y<-20||y>260)continue;uint16_t c=g->p.coins>=(unsigned)(b->price-s->paid[i])?RGB(245,255,232):CREAM;outline(x-18,y-10,36,20,c);rect(x-15,y-8,30,1,c);number(x,y-4,b->price-s->paid[i],INK,1,true);}
 if((s->active&BIT(i))&&b->inputs[0]!=255&&!s->stamina[i]){Point p=game_repair_position(g,i);int x=screenx(p.x),y=screeny(p.y)-35;pill(x-12,y,24,18,CREAM);center(x,y+2,b->food<5?"Zz":"!",INK,1);}
 }for(unsigned i=1;i<ACTOR_COUNT;i++){const Actor*a=&g->actors[i];if(!a->live)continue;int x=screenx(a->x),y=screeny(a->y)-40;if(a->phase==RESTING){pill(x-12,y,24,18,CREAM);center(x,y+2,"Zz",INK,1);}else if(a->role==BUYER&&a->phase==SHOPPING){for(unsigned f=1;f<=14;f++)if(a->wish[f]){pill(x-18,y,36,22,CREAM);const Image*im=sprites[food_icons[f]];sprite(food_icons[f],x-12-im->w/2,y+2,false);number(x+7,y+5,a->wish[f],INK,1,false);break;}}}
 if(s->cash){const PlaceDef*b=&m->places[m->cash];int x=screenx(b->pos.x),y=screeny(b->pos.y)-35;pill(x-21,y,42,18,GOLD);number(x,y+2,s->cash,INK,1,true);}
}
static void hud(const Game*g,bool failed){const MartSave*s=&g->p.mart[g->p.market];pill(7,6,90,24,CREAM);sprite(S_GAME__CASH,11,10,false);amount(37,12,g->p.coins,INK);pill(238,6,75,24,CREAM);text(249,12,"Mart",INK,1);number(288,12,g->p.market+1,INK,1,false);
 if(g->notice_time){pill(6,216,308,20,CREAM);center(160,220,g->notice,INK,1);}else if(g->selected_place<marts[g->p.market].count){const PlaceDef*b=&marts[g->p.market].places[g->selected_place];if(b->food){pill(65,217,190,19,CREAM);center(160,220,food_names[b->food],INK,1);}}
 if(failed){pill(91,33,138,18,CREAM);center(160,36,"Save needs attention",RGB(181,57,43),1);}
 if(s->progress==marts[g->p.market].order_count&&!g->notice_time){pill(87,33,146,19,CREAM);center(160,37,g->p.market?"Mart complete!":"New mart unlocked!",INK,1);}
}
static void button(int x,int y,int w,const char*label,bool selected){pill(x,y,w,26,selected?GOLD:CREAM);center(x+w/2,y+7,label,INK,1);if(selected)outline(x+2,y+2,w-4,22,RGB(201,151,32));}
static void panel(const char*title){pill(13,16,294,208,CREAM);center(160,28,title,INK,2);rect(28,56,264,1,RGB(220,210,178));}
static void footer(const char*s){center(160,208,s,MUTED,1);}
static void overview(const Game*g){const MartDef*m=&marts[g->p.market];const MartSave*s=&g->p.mart[g->p.market];panel("YOUR MART");for(unsigned n=0;n<m->count;n++){const PlaceDef*b=&m->places[n];if(!(s->active&BIT(n))&&!(g->offered&BIT(n)))continue;int x=160+b->pos.x/13,y=130-b->pos.y/13;circle(x,y,3,s->active&BIT(n)?GREEN:GOLD);}circle(160+(int)g->actors[0].x/13,130-(int)g->actors[0].y/13,3,INK);footer("0 / BACK   Close map");}
void render_invalidate(void){invalidate();}
void render(const Game*g,unsigned view,unsigned selected,unsigned column,bool failed,Push push){cx=(int)g->camera_x;cy=(int)g->camera_y;if(view==TITLE)cx=-100,cy=50;world_queue(g);
 for(band=0;band<240;band+=8){floor_band(g->p.market);for(unsigned i=0;i<draw_count;i++){Draw*d=&draws[i];const Image*im=sprites[d->sprite];if(d->w!=im->w||d->h!=im->h)image(im,d->x,d->y,d->w,d->h);else sprite(d->sprite,d->x,d->y,d->flip);}pads(g);
  if(view==PLAY){hud(g,failed);if(g->overview)overview(g);}
  else if(view==TITLE){pill(30,25,260,178,CREAM);sprite(S_MENU_LOGO,65,38,false);center(160,117,"Grow your own grocery empire",INK,1);button(82,145,156,"CONTINUE",selected==0);center(160,186,"OK  Play     0  Controls",MUTED,1);text(8,222,"By Bobydeluxe",INK,1);}
  else if(view==PAUSE){panel("MONKEY MART");static const char*const labels[]={"Continue","Upgrades","Hats","Markets","Boosts","Controls","Save and exit","Reset progress"};for(unsigned i=0;i<8;i++){int y=64+i*19;if(i==selected)rect(28,y-2,264,19,GOLD);center(160,y+1,labels[i],INK,1);}}
  else if(view==UPGRADES){panel("UPGRADES");const MartDef*m=&marts[g->p.market];unsigned first=selected>1?selected-1:0;if(first+3>m->upgrade_count)first=m->upgrade_count-3;
   for(unsigned row=first;row<first+3;row++){int y=66+(row-first)*44;bool unlocked=row==0||(g->p.mart[g->p.market].active&BIT(m->upgrade_places[row]));text(29,y,upgrade_names[g->p.market][row],unlocked?INK:MUTED,1);
    for(unsigned k=0;k<3;k++){int x=29+k*88;uint16_t price=game_upgrade_price(g,row,k);const char*name=k==0?"Energy":k==1?"Carry":"Speed";if(row==selected&&k==column)rect(x-1,y+15,85,23,GOLD);text(x+2,y+19,name,unlocked?INK:MUTED,1);if(!unlocked)text(x+48,y+19,"--",MUTED,1);else if(price==UINT16_MAX)text(x+48,y+19,"MAX",MUTED,1);else number(x+48,y+19,price,INK,1,false);}}
   footer("Arrows  Select    OK  Buy    BACK");}
  else if(view==HATS){panel("HATS");unsigned first=(selected/7)*7;for(unsigned j=0;j<7;j++){unsigned h=first+j;int x=37+(j%4)*72,y=69+(j/4)*59;bool owned=(g->p.hats&(1u<<h))!=0;if(h==selected)pill(x-8,y-4,64,53,GOLD);unsigned id=h?hats[h+5]:S_UI_MAIN__HAT0;if(id){const Image*im=sprites[id];sprite(id,x+22-im->w/2,y+2,false);}if(g->p.hat==h)center(x+22,y+31,"Equipped",INK,1);else if(owned)center(x+22,y+31,"Owned",INK,1);else if(hat_prices[h])number(x+22,y+31,hat_prices[h],INK,1,true);else if(!hat_tokens[h])center(x+22,y+31,"Free",INK,1);else{number(x-1,y+31,hat_tokens[h],INK,1,false);text(x+12,y+31,"tokens",INK,1);}}
   text(29,191,"Coins",INK,1);number(70,191,g->p.coins,INK,1,false);text(168,191,"Tokens",INK,1);number(218,191,g->p.tokens,INK,1,false);footer("LEFT / RIGHT  Browse    OK  Equip");}
  else if(view==MARKETS){panel("MARKETS");for(unsigned i=0;i<2;i++){button(40,72+i*50,240,i==0?"Mart 1: Farm fresh":"Mart 2: Sweet treats",selected==i);const MartSave*s=&g->p.mart[i];number(139,104+i*50,s->progress,INK,1,false);text(158,104+i*50,i==0?"/ 17":"/ 18",INK,1);}if(!(g->p.mart[0].active&BIT(31)))center(160,181,"Build the delivery truck to unlock Mart 2",MUTED,1);footer("OK  Visit    BACK  Return");}
  else if(view==BOOSTS){panel("BOOSTS");static const char*const labels[]={"Double earnings - 120s","Energized workers - 120s","Tireless machines - 120s","Cash delivery + 1 token"};for(unsigned i=0;i<4;i++)button(36,66+i*31,248,labels[i],selected==i);if(g->p.reward_cooldown){text(60,194,"Next reward in",MUTED,1);number(168,194,g->p.reward_cooldown,MUTED,1,false);}else center(160,194,"Offline reward ready",GREEN,1);footer("OK  Claim    BACK  Return");}
  else if(view==HELP){panel("HOW TO PLAY");static const char*const lines[]={"ARROWS    Move your monkey","Stand near crops to harvest.","Carry goods to shelves and machines.","Stay by the till to serve customers.","Walk onto price pads to build or hire.","Wake workers and repair tired machines.","BACK   Menu       0   Map","Progress saves when you leave."};for(unsigned i=0;i<8;i++)text(28,68+i*16,lines[i],INK,1);}
  else if(view==RESET_CONFIRM){panel("RESET PROGRESS?");center(160,72,"Clear both marts, coins and upgrades?",INK,1);center(160,94,"Your hats will also be removed.",MUTED,1);button(42,122,236,"Keep progress",selected==0);button(42,157,236,"Reset all progress",selected==1);footer("OK  Confirm    BACK  Cancel");}
  else if(view==SAVE_ERROR){panel("STORAGE FULL");center(160,75,"Your progress could not be saved.",INK,1);center(160,100,"Free space in calculator storage.",MUTED,1);button(42,129,236,"OK   Retry save and exit",selected==0);center(160,167,"1   Continue without saving",INK,1);center(160,191,"BACK   Exit without saving",MUTED,1);}
  present(push);
 }
}
