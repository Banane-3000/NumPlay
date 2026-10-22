#include "render.h"
#include <stdlib.h>
#include <string.h>
#define YELLOW 0xffa0
#define GREEN 0x07fa
#define WHITE 0xffff
static uint16_t strip[320*8];
static uint32_t hashes[30];
static int band;
static void pixel(int x,int y,uint16_t c){if((unsigned)x<320&&(unsigned)(y-band)<8)strip[(y-band)*320+x]=c;}
static void rect(int x,int y,int w,int h,uint16_t c){int x0=x<0?0:x,x1=x+w>320?320:x+w,y0=y<band?band:y,y1=y+h>band+8?band+8:y+h;for(int j=y0;j<y1;j++)for(int i=x0;i<x1;i++)strip[(j-band)*320+i]=c;}
static void box(int x,int y,int w,int h,uint16_t c){rect(x,y,w,1,c);rect(x,y+h-1,w,1,c);rect(x,y,1,h,c);rect(x+w-1,y,1,h,c);}
static int width(const char*s,int scale){int w=0;for(;*s;s++){unsigned n=(unsigned char)*s;w+=font_glyphs[n>=32&&n<128?n-32:31].advance*scale;}return w;}
static void text(int x,int y,const char*s,uint16_t color,int scale){for(;*s;s++){unsigned ch=(unsigned char)*s;const Glyph*g=&font_glyphs[ch>=32&&ch<128?ch-32:31];for(unsigned py=0;py<g->h;py++){int yy=y+(g->y+(int)py)*scale;if(yy+scale<=band||yy>=band+8)continue;for(unsigned px=0;px<g->w;px++){unsigned bit=py*g->w+px;if(font_bits[g->offset+bit/8]&(128>>(bit%8)))rect(x+(g->x+(int)px)*scale,yy,scale,scale,color);}}x+=g->advance*scale;}}
static void center(int x,int y,const char*s,uint16_t c,int scale){text(x-width(s,scale)/2,y,s,c,scale);}
static void number(int x,int y,unsigned n,uint16_t c,bool centered){char b[12],rev[11];unsigned k=0;do{rev[k++]='0'+n%10;n/=10;}while(n&&k<10);for(unsigned i=0;i<k;i++)b[i]=rev[k-1-i];b[k]=0;if(centered)center(x,y,b,c,1);else text(x,y,b,c,1);}
static void sprite(unsigned id,int x,int y,uint16_t color,int scale,int rotation){const Sprite*s=&sprites[id];int w=rotation&1?s->h:s->w,h=rotation&1?s->w:s->h;int y0=y<band?band:y,y1=y+h*scale>band+8?band+8:y+h*scale;for(int py=y0;py<y1;py++)for(int px=x<0?0:x;px<x+w*scale&&px<320;px++){int a=(px-x)/scale,b=(py-y)/scale,sx=a,sy=b;switch(rotation&3){case 1:sx=b;sy=s->h-1-a;break;case 2:sx=s->w-1-a;sy=s->h-1-b;break;case 3:sx=s->w-1-b;sy=a;break;}if(rotation&4)sx=s->w-1-sx;unsigned bit=sy*s->w+sx;if(sprite_bits[s->offset+bit/8]&(128>>(bit%8)))pixel(px,py,(sprite_ink[s->offset+bit/8]&(128>>(bit%8)))?color:0);}}
static void at(unsigned id,int x,int y,uint16_t c,int scale,int rotation){const Sprite*s=&sprites[id];sprite(id,x-(rotation&1?s->h:s->w)*scale/2,y-(rotation&1?s->w:s->h)*scale/2,c,scale,rotation);}
static uint16_t dim(uint16_t c){return ((c&0xf800)>>2&0xf800)|((c&0x7e0)>>2&0x7e0)|((c&31)>>2);}
static void button(int x,int y,int w,const char*label,bool selected){rect(x,y,w,25,selected?YELLOW:0);box(x,y,w,25,YELLOW);center(x+w/2,y+6,label,selected?0:YELLOW,1);}
static void stars(int x,int y,unsigned count,uint16_t color){for(unsigned i=0;i<3;i++)at(S_STAR_ANIM_1,x+(int)i*16,y,i<count?color:dim(color),1,0);}
static void gameplay(Game*g){uint16_t c=g->freeze?0xbdf7:theme_colors[(g->arcade?g->height_score/100:g->level/15)%13];int camera_x=g->width*12<=320?(int)g->width*6-160:g->x*12/Q-160;int camera_y=g->y*12/Q-148;int miny=-14,maxy=(int)g->height*12-240;if(camera_y<miny)camera_y=miny;if(maxy<miny)maxy=miny;if(camera_y>maxy)camera_y=maxy;
 int row0=(band+camera_y)/12-1,row1=(band+8+camera_y)/12+1;if(row0<0)row0=0;if(row1>=g->height)row1=g->height-1;
 for(int y=row0;y<=row1;y++)for(unsigned x=0;x<g->width;x++){unsigned t=game_tile(g,x,y);int sx=x*12-camera_x,sy=y*12-camera_y;if(sx<-24||sx>344)continue;
 if(t==3||t==5||t==114){bool a=!game_solid(game_tile(g,x,y-1)),b=!game_solid(game_tile(g,x,y+1)),l=!game_solid(game_tile(g,x-1,y)),r=!game_solid(game_tile(g,x+1,y));if(a)rect(sx,sy,12,2,c);if(b)rect(sx,sy+10,12,2,c);if(l)rect(sx,sy,2,12,c);if(r)rect(sx+10,sy,2,12,c);if(a||b||l||r){unsigned hash=x*1103515245u+y*12345u;if(a)rect(sx+hash%9,sy+2,2,2,c);if(l)rect(sx+2,sy+(hash>>5)%9,2,2,c);if(b)rect(sx+(hash>>9)%9,sy+8,2,2,c);if(r)rect(sx+8,sy+(hash>>13)%9,2,2,c);}}
 else if(t==19||t==31){bool coin=t==19||g->bonus;if(coin)at(S_COIN_1,sx+6,sy+6,YELLOW,1,0);else rect(sx+5,sy+5,2,2,c);}
 else if(t==4)at(S_STAR_ANIM_1,sx+6,sy+6,c,1,0);
 else if(t==1)at(S_EXIT_1,sx+6,sy+6,dim(c),2,0);
 else if(t==2)at(S_EXIT_1,sx+6,sy+6,GREEN,1,0);
 else if(t==7)at(S_ICE,sx+6,sy+6,0x67ff,1,0);
 else if(t==8||t==98){bool active=t==8||(g->tick%150>=75&&!g->freeze);uint16_t col=active?c:dim(c);at(t==8?S_SPIKES_U:S_M_SPIKES_U,sx+6,sy+2,col,1,0);at(t==8?S_SPIKES_D:S_M_SPIKES_D,sx+6,sy+10,col,1,0);}
 else if(t==73||t==74||t==85||t==97)at(S_CANNON_1,sx+6,sy+6,c,1,t==74?0:t==73?2:t==85?3:1);
 else if((t>=49&&t<=55)||(t>=61&&t<=67))at(S_PORTAL_1,sx+6,sy+6,GREEN,1,t>=61);
 else if(t==56||t==57||t==68||t==69)at(S_TRAMPLIN_1,sx+6,sy+6,GREEN,1,t==56?1:t==57?2:t==68?0:3);
 else if(t==121){box(sx+1,sy+1,10,10,c);rect(sx+4,sy+4,4,4,c);}
 else if(t>=145&&t<=148)at(t==145?S_COIN_ADDICT_1:t==146?S_FREEZE_1:t==147?S_MAGNET_1:S_SCORES_1,sx+6,sy+6,YELLOW,1,0);
 else if(t==109||t==110){box(sx,sy,12,12,c);rect(sx+5,sy,2,12,c);rect(sx,sy+5,12,2,c);}
 else if(t==113||t==115){at(S_TEETH_1,sx+6,sy+6,g->tick%180>=90?c:dim(c),1,t==115?2:0);}
 else if(t==6||t==12){box(sx,sy,12,12,c);}
 else if(t==24||t==36||t==48||t==60||t==72||t==84){at(S_COIN_ADDICT_1,sx+6,sy+6,dim(c),1,0);}
 }
 for(unsigned i=0;i<ACTORS;i++){Actor*a=&g->actors[i];if(!a->active)continue;unsigned t=a->type,id=t==153?S_CANNON_BOLID:t==102?S_FISH_HEDGEHOG_1:t==150?S_COIN_ADDICT_1:t==151?S_MONKEY_IDLE_1:t==152?S_ROCK:t==78||t==79||t==90||t==91?S_PLATFORM_1:S_BAT_1;if(t==109||t==110||t==154)continue;at(id,a->x*12/Q+6-camera_x,a->y*12/Q+6-camera_y,c,1,0);}
 int px=g->x*12/Q+6-camera_x,py=g->y*12/Q+6-camera_y;if(g->dx||g->dy){rect((g->previous_x*12/Q+6-camera_x),py-1,2,2,dim(YELLOW));}
 if(!g->dead||g->tick%12<6){unsigned state=g->dx||g->dy?1:0;unsigned frame=(g->tick/6)%(state?5:6);at(hero_sprites[g->p.mask][state][frame],px,py,g->invincible&&g->tick%8<4?GREEN:YELLOW,1,g->dy>0?1:g->dy<0?3:g->dx<0?4:0);}
 if(g->shield){box(px-10,py-10,21,21,GREEN);}
 if(g->arcade){int sy=g->lava*12/Q-camera_y;if(sy<240){rect(0,sy,320,240-sy,0x0567);for(int x=0;x<320;x+=8)rect(x,sy-((x+g->tick/3)%4),6,2,GREEN);}}
 rect(0,0,320,22,0);if(g->arcade){text(9,5,"SCORE",YELLOW,1);number(65,5,g->score,YELLOW,false);}else{number(9,5,g->level+1,YELLOW,false);stars(143,11,g->stars,c);}at(S_COIN_1,259,11,YELLOW,1,0);number(270,5,g->coins,YELLOW,false);if(g->p.shields){box(7,218,16,16,YELLOW);center(15,219,"S",YELLOW,1);number(29,220,g->p.shields,YELLOW,false);}}
void render_invalidate(void){memset(hashes,0,sizeof(hashes));}
void render(Game*g,unsigned view,unsigned selected,bool failed,Push push){for(band=0;band<240;band+=8){memset(strip,0,sizeof(strip));
 if(view==PLAY||view==PAUSE||view==DEAD||view==RESULT||view==QUIT)gameplay(g);
 if(view==TITLE){for(int x=8;x<320;x+=24)for(int y=5;y<240;y+=24)if((x+y+g->tick/20)%7==0)rect(x,y,2,2,0x3006);at(S_LOGO_REVEAL_13,160,43,YELLOW,2,0);at(hero_sprites[g->p.mask][0][(g->tick/9)%6],160,97,YELLOW,3,0);button(32,136,120,"MAP",selected==0);button(168,136,120,"ARCADE",selected==1);button(32,170,120,"MASKS",selected==2);button(168,170,120,"SHOP",selected==3);center(160,207,"0 HELP   1 MISSIONS   2 CHEST",YELLOW,1);text(6,229,"BY BOBYDELUXE",GREEN,1);}
 else if(view==STAGES){text(12,7,"MAP",YELLOW,1);number(246,7,g->p.coins,YELLOW,false);unsigned page=selected/15,start=page*15;uint16_t c=theme_colors[(selected/15)%13];for(unsigned n=0;n<15&&start+n<200;n++){unsigned id=start+n;int col=n%5,row=n/5;if(row&1)col=4-col;int x=15+col*61,y=40+row*57;uint16_t k=id<g->p.unlocked?YELLOW:c;if(id==selected){box(x-3,y-3,52,42,GREEN);box(x-4,y-4,54,44,GREEN);}rect(x,y,46,30,k);if(id<g->p.unlocked)number(x+23,y+8,id+1,0,true);else {box(x+18,y+8,10,12,0);box(x+21,y+4,4,7,0);}stars(x+7,y+38,g->p.stars[id],k);if(n%5!=4)rect(x+(row&1?-15:46),y+14,15,2,c);}center(160,225,"ARROWS: CHOOSE    OK: PLAY",YELLOW,1);}
 else if(view==PAUSE||view==DEAD||view==RESULT||view==QUIT){rect(48,35,224,172,YELLOW);const char*heading=view==PAUSE?"PAUSED":view==DEAD?"TRY AGAIN":view==QUIT?"QUIT GAME?":"STAGE CLEAR";center(160,45,heading,0,1);rect(61,68,198,1,0);if(view==RESULT){stars(144,92,g->stars,0);text(65,115,"COINS",0,1);number(224,115,g->coins,0,true);center(160,140,g->dots==g->total_dots?"ALL DOTS +25 COINS":"FIND EVERY DOT",0,1);button(77,166,166,g->level==199?"MAP":"NEXT",true);box(77,166,166,25,0);}
 else{const char*items[4];unsigned n;if(view==PAUSE){items[0]="RESUME";items[1]="RESTART";items[2]="MAP / MENU";items[3]="QUIT GAME";n=4;}else if(view==QUIT){items[0]="KEEP PLAYING";items[1]="SAVE AND EXIT";n=2;}else{items[0]="RETRY";items[1]="MAP / MENU";n=2;number(160,83,g->score,0,true);}for(unsigned i=0;i<n;i++){int y=(view==PAUSE?81:125)+i*27;if(i==selected)rect(64,y,192,23,0);center(160,y+5,items[i],i==selected?YELLOW:0,1);}}}
 else if(view==MASKS){center(160,12,"MASKS",YELLOW,1);unsigned m=selected%16;center(160,44,mask_names[m],YELLOW,1);at(hero_sprites[m][0][(g->tick/9)%6],62,110,YELLOW,4,0);
 const char*features[]={"","COINS +","ADDICT +","FREEZE +","MAGNET +","BOOST +","BONUS +","NO SNAKES","NO MONKEYS"};unsigned row=0;for(unsigned f=1;f<9;f++)if(mask_features[m][f]){int y=82+row++*23;text(125,y,features[f],GREEN,1);if(f<7){int x=125+width(features[f],1)+3;number(x,y,mask_features[m][f],GREEN,false);text(x+22,y,"%",GREEN,1);}}if(!row)text(125,100,"THE ORIGINAL",GREEN,1);
 unsigned lv=game_player_level(&g->p);text(92,163,lv<mask_levels[m]?"NEEDS LVL":"YOUR LVL",GREEN,1);number(212,163,lv<mask_levels[m]?mask_levels[m]:lv,GREEN,false);
 if(!(g->p.owned&(1u<<m))){at(S_COIN_1,128,190,YELLOW,1,0);number(143,184,mask_prices[m],YELLOW,false);}else center(160,184,"OWNED",GREEN,1);
 button(69,207,182,g->p.mask==m?"SELECTED":lv<mask_levels[m]?"LOCKED":g->p.owned&(1u<<m)?"SELECT":"UNLOCK",g->p.mask==m);}

 else if(view==SHOP){center(160,9,"SHOP",YELLOW,1);at(S_COIN_1,124,35,YELLOW,1,0);number(140,29,g->p.coins,YELLOW,false);const char*names[]={"SHIELD","COIN ADDICT","FREEZE","MAGNET","SCORE BOOST"};for(unsigned i=0;i<5;i++){int y=54+i*30;if(selected==i)rect(17,y,286,25,YELLOW);text(25,y+6,names[i],selected==i?0:YELLOW,1);number(214,y+6,i?g->p.upgrades[i-1]+1:g->p.shields,selected==i?0:YELLOW,true);unsigned cost=i?upgrade_prices[g->p.upgrades[i-1]<9?g->p.upgrades[i-1]:8]:200;if(i&&g->p.upgrades[i-1]>=9)text(252,y+6,"MAX",selected==i?0:YELLOW,1);else number(261,y+6,cost,selected==i?0:YELLOW,true);}center(160,222,"OK: BUY    BACK: MENU",GREEN,1);}
 else if(view==MISSIONS){center(160,12,"MISSIONS",YELLOW,1);number(160,36,selected+1,GREEN,true);const char*lines[]={"COMPLETE STAGE","PLAY ARCADE","COLLECT DOTS","COLLECT STARS","STAGE WITHOUT REVIVE","ALL DOTS IN STAGES","ARCADE SCORE IN ONE RUN","OPEN CHESTS"};unsigned step=g->p.mission_step[selected];if(step>=7)center(160,116,"ALL COMPLETE",GREEN,1);else{unsigned target=mission_goals[selected][step],progress=g->p.mission_counts[selected];if(progress>target)progress=target;center(160,73,lines[selected],YELLOW,1);number(160,100,target,YELLOW,true);text(79,133,"PROGRESS",GREEN,1);number(200,133,progress,GREEN,false);text(79,161,"REWARD",YELLOW,1);number(200,161,mission_rewards[step],YELLOW,false);button(59,199,202,progress>=target?"CLAIM":"IN PROGRESS",progress>=target);}center(160,225,"LEFT / RIGHT: NEXT",GREEN,1);}
 else if(view==CHEST){center(160,12,"CHEST",YELLOW,1);at(S_COIN_1,125,39,YELLOW,1,0);number(141,33,g->p.coins,YELLOW,false);
 if(g->combo){const int8_t points[8][2]={{0,-1},{1,-1},{1,0},{1,1},{0,1},{-1,1},{-1,0},{-1,-1}};for(unsigned i=0;i<8;i++){int x=160+points[i][0]*56,y=124+points[i][1]*46;box(x-13,y-13,26,26,i==g->tick%8?GREEN:YELLOW);at(i==7?S_SHIELD_FX_1:i==3?S_COIN_ADDICT_1:S_COIN_1,x,y,i==g->tick%8?GREEN:YELLOW,1,0);}center(160,115,"...",GREEN,1);}
 else if(g->reward_kind){center(160,72,"YOU GOT",GREEN,1);if(g->reward_kind==3){at(hero_sprites[g->reward_value][0][0],160,116,YELLOW,3,0);center(160,154,mask_names[g->reward_value],YELLOW,1);}else{at(g->reward_kind==2?S_SHIELD_FX_1:S_COIN_1,160,113,YELLOW,3,0);number(160,151,g->reward_value,YELLOW,true);}}
 else{rect(120,82,80,64,YELLOW);rect(125,87,70,15,0);rect(125,107,70,34,0);rect(151,99,18,19,YELLOW);rect(156,104,8,9,0);center(160,162,"WHEEL OF FORTUNE",GREEN,1);}
 if(!g->combo){bool free=g->p.free_chest||(g->p.next_chest&&g->p.playtime>=g->p.next_chest);button(69,201,182,free?"OPEN FREE":"OPEN 200 COINS",true);}}
 else if(view==HELP){center(160,12,"TOMB OF THE MASK",YELLOW,1);const char*lines[]={"ARROWS: DASH TO A WALL","COLLECT DOTS AND 3 STARS","REACH THE GREEN EXIT","OK / EXE: USE A SHIELD","BACK: PAUSE","HOME / POWER: SAVE + EXIT","ARCADE: ESCAPE THE RISING TIDE","ORIGINAL: HAPPYMAGENTA","ADAPTATION: BOBYDELUXE"};for(unsigned i=0;i<9;i++)center(160,43+i*20,lines[i],i<7?YELLOW:GREEN,1);}
 else if(view==SAVE_ERROR){center(160,52,"STORAGE FULL",YELLOW,1);center(160,87,"OK: RETRY SAVE",YELLOW,1);center(160,117,"1: KEEP PLAYING",YELLOW,1);center(160,147,"BACK: EXIT UNSAVED",YELLOW,1);center(160,185,"OTHER FILES ARE KEPT",GREEN,1);}
 if(failed&&view!=SAVE_ERROR)rect(314,2,4,4,0xf800);
 uint32_t hash=2166136261u;for(unsigned i=0;i<320*8;i++)hash=(hash^strip[i])*16777619u;if(hash!=hashes[band/8]){hashes[band/8]=hash;push(0,band,320,8,strip);}}
}
