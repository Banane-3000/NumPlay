/* SPDX-License-Identifier: GPL-3.0-only
 * Original pixel art, direct packed sprites, one 8-row RGB565 strip (5 KiB).
 * Only changed strips are sent to the LCD; no heap or full framebuffer. */
#include "render.h"
#include <stdlib.h>
#include <string.h>
#include "assets.h"
#include "font.h"
#define C(v) ((uint16_t)((((v)>>8)&0xF800)|(((v)>>5)&0x07E0)|(((v)>>3)&31)))
#define BLACK C(0x070B16)
#define PANEL C(0x111A2B)
#define DIM C(0x52617B)
#define STEEL C(0x29364F)
#define WHITE C(0xFFF0CD)
#define RED C(0xFF5545)
#define PINK C(0xF566AF)
#define GOLD C(0xFFE16B)
#define CYAN C(0x77DAEA)
static uint16_t band[320*8];
static uint32_t hashes[30];
static bool valid;
static int top;
static const dk_game *g;
static const dk_stats *s;
static unsigned view,selected;
static bool ongoing,failed;
static uint32_t clock_ms;
void dk_invalidate(void){valid=false;}
static void pixel(int x,int y,uint16_t color) {
  if(x>=0&&x<320&&y>=top&&y<top+8)band[(y-top)*320+x]=color;
}
static void rect(int x,int y,int w,int h,uint16_t color) {
  int l=x<0?0:x,r=x+w>320?320:x+w,t=y<top?top:y,b=y+h>top+8?top+8:y+h;
  if(l>=r||t>=b)return;
  for(int j=t;j<b;++j)for(int i=l;i<r;++i)band[(j-top)*320+i]=color;
}
static void line(int a,int b,int c,int d,uint16_t color) {
  if((b<top&&d<top)||(b>=top+8&&d>=top+8))return;
  int dx=abs(c-a),sx=a<c?1:-1,dy=-abs(d-b),sy=b<d?1:-1,e=dx+dy;
  for(;;){pixel(a,b,color);if(a==c&&b==d)break;int twice=e*2;
    if(twice>=dy)e+=dy,a+=sx;
    if(twice<=dx)e+=dx,b+=sy;}
}
static void frame(int x,int y,int w,int h,uint16_t color) {
  rect(x,y,w,1,color);rect(x,y+h-1,w,1,color);rect(x,y,1,h,color);rect(x+w-1,y,1,h,color);
}
static void text(int x,int y,const char *str,int scale,uint16_t color) {
  if(y>=top+8||y+7*scale<=top)return;
  for(;*str;++str,x+=6*scale) {
    unsigned ch=(unsigned char)*str,i;if(ch>='a'&&ch<='z')ch-=32;
    if(ch>='A'&&ch<='Z')i=ch-'A';
    else if(ch>='0'&&ch<='9')i=ch-'0'+26;
    else{static const char punctuation[]="+-.:/!<>|";const char *p=strchr(punctuation,(int)ch);
      if(!p||ch==' ')continue;
      i=36+(unsigned)(p-punctuation);}
    for(int col=0;col<5;++col)for(int row=0;row<7;++row)
      if(alphabet[i][col]&(1<<row))rect(x+col*scale,y+row*scale,scale,scale,color);
  }
}
static void center(int x,int y,const char *str,int scale,uint16_t color) {
  text(x-((int)strlen(str)*6*scale-scale)/2,y,str,scale,color);
}
static void number(int x,int y,uint32_t v,int scale,uint16_t color) {
  char str[11];unsigned n=0;do{str[n++]=(char)('0'+v%10);v/=10;}while(v);
  for(unsigned i=0;i<n/2;++i){char c=str[i];str[i]=str[n-i-1];str[n-i-1]=c;}str[n]=0;text(x,y,str,scale,color);
}
static void sprite(int x,int y,const dk_sprite *p,int face,int scale,bool blue) {
  if(y>=top+8||y+p->h*scale<=top)return;
  int first=(top-y)/scale;if(first<0)first=0;
  int last=(top+7-y)/scale;if(last>=p->h)last=p->h-1;
  for(int row=first;row<=last;++row)for(int col=0;col<p->w;++col) {
    unsigned i=(unsigned)(row*p->w+col),v=p->p[i/2];v=(i&1)?v&15:v>>4;
    if(v)rect(x+(face<0?p->w-col-1:col)*scale,y+row*scale,scale,scale,
      blue&&(v==12||v==7)?CYAN:palette[v]);
  }
}
static int wx(int x){return x+88;}
static int wy(int y){return y+8;}
static void heart(int x,int y) {
  rect(x,y,3,3,PINK);rect(x+5,y,3,3,PINK);rect(x-1,y+2,10,3,PINK);
  rect(x,y+5,8,1,PINK);rect(x+1,y+6,6,1,PINK);rect(x+2,y+7,4,1,PINK);rect(x+3,y+8,2,1,PINK);
}
static void mario(int x,int feet,int face,bool ladder,bool dead,int step,bool hammer) {
  if(dead) { // The silhouette tumbles once, then stays still during the death cue.
    sprite(x-6,feet-13,&spr_mario,face,1,false);line(x-6,feet-6,x+6,feet-1,WHITE);return;
  }
  sprite(x-6,feet-16,&spr_mario,face,1,false);
  if(ladder) {
    rect(x-6,feet-9,2,3,palette[2]);rect(x+5,feet-12,2,3,palette[2]);
    rect(x-4,feet-4,8,4,BLACK);rect(x-4,feet-(step?5:3),3,3,palette[3]);
    rect(x+2,feet-(step?3:5),3,3,palette[3]);
  } else if(step) {
    rect(x-6,feet-2,13,2,BLACK);rect(x-3,feet-2,4,2,palette[5]);rect(x+3,feet-4,4,2,palette[5]);
  }
  if(hammer) sprite(x+(face<0?-17:9),feet-23+(step?7:0),&spr_hammer,face,1,false);
}
static void kong(int x,int feet) {
  if(g->board==3&&g->phase==DK_CLEAR)feet+=(125-g->timer)/2;
  sprite(wx(x-13),wy(feet-25),&spr_kong,1,1,false);
  // Fists move with barrel throws; the tie is red, not another bitmap frame.
  if((g->tick/24)&1){rect(wx(x-15),wy(feet-17),4,5,palette[7]);rect(wx(x+12),wy(feet-17),4,5,palette[7]);}
  rect(wx(x-1),wy(feet-10),3,5,RED);rect(wx(x-2),wy(feet-6),5,3,RED);
}
static void girders(void) {
  for(unsigned i=0;i<g->platform_count;++i) {
    const dk_platform *p=&g->platform[i];
    int low=p->y1<p->y2?p->y1:p->y2,high=p->y1>p->y2?p->y1:p->y2;
    if(wy(low)>top+7||wy(high+5)<top)continue;
    uint16_t color=p->type==DK_LIFT?GOLD:PINK;
    for(int x=p->x1;x<=p->x2;++x) {
      int y=dk_surface(g,i,x);if(y<0)continue;
      pixel(wx(x),wy(y),color);pixel(wx(x),wy(y+4),color);
      if(p->type==DK_BELT) {
        pixel(wx(x),wy(y+2),STEEL);
        int offset=dk_belt_direction(g,p)*(int)(g->age/4);
        if(((x+offset)%8+8)%8<2)pixel(wx(x),wy(y+2),WHITE);
      } else {
        int k=(x-p->x1)%8;pixel(wx(x),wy(y+1+(k<4?k:7-k)),color);
      }
    }
    if(p->type==DK_BELT) {
      rect(wx(p->x1),wy(low),2,5,CYAN);rect(wx(p->x2-1),wy(high),2,5,CYAN);
    }
  }
  if(g->board==2)for(int x=60;x<=124;x+=56)line(wx(x),wy(57),wx(x),wy(216),STEEL);
}
static void ladders(void) {
  for(unsigned i=0;i<g->ladder_count;++i) {
    const dk_ladder *l=&g->ladder[i];
    int t=dk_surface(g,l->upper,l->x)+2,b=dk_surface(g,l->lower,l->x);
    bool open=dk_ladder_open(g,i);uint16_t color=open?CYAN:DIM;
    if(l->retract&&!open)t=b-7;
    for(int y=t;y<b;++y) {
      if(l->broken&&y>t+7&&y<b-7)continue;
      pixel(wx(l->x-3),wy(y),color);pixel(wx(l->x+3),wy(y),color);
      if((y-t)%4==0)rect(wx(l->x-3),wy(y),7,1,color);
    }
  }
}
static void world(void) {
  rect(87,7,226,226,BLACK);frame(86,6,228,228,STEEL);
  girders();ladders();
  for(unsigned i=0;i<5;++i)if(g->item[i].active) {
    const dk_item *v=&g->item[i];const dk_sprite *p=v->type==1?&spr_hammer:v->type==2?&spr_bag:v->type==3?&spr_hat:&spr_parasol;
    sprite(wx(v->x-p->w/2),wy(v->y-p->h/2),p,1,1,false);
  }
  if(g->board==3)for(int row=1;row<=4;++row)for(int side=0;side<2;++side)
    if(!(g->rivets&(1u<<((row-1)*2+side)))) {
      int x=side?176:48,y=g->platform[row].y1;
      rect(wx(x-2),wy(y-2),5,2,CYAN);rect(wx(x),wy(y),1,5,CYAN);
    }
  if(g->board==0||g->board==1) {
    int x=g->board==0?17:112,y=g->board==0?212:176;
    rect(wx(x-7),wy(y-13),14,13,palette[3]);rect(wx(x-8),wy(y-14),16,2,CYAN);
    if((g->age/8)&1)sprite(wx(x-5),wy(y-23),&spr_fire,1,1,true);
    center(wx(x),wy(y-8),"OIL",1,WHITE);
  }
  int kx=g->board==3?112:g->board==1?80+(int)(g->age/8%42):34;
  int ky=g->board==0?58:g->board==1?68:g->board==2?55:66;
  kong(kx,ky);
  int px=g->board==0?163:g->board==1?198:g->board==2?204:185;
  int py=g->board==0?25:g->board==1?28:g->board==2?24:28;
  sprite(wx(px-5),wy(py-18),&spr_pauline,1,1,false);
  if(g->phase==DK_CLEAR&&g->board==3)heart(wx(165),wy(5));
  else if((g->tick/45)&1)text(wx(px-36),wy(4),"HELP!",1,PINK);
  for(unsigned i=0;i<DK_ENEMIES;++i) {
    const dk_enemy *e=&g->enemy[i];if(!e->active)continue;
    int x=wx(e->x/DK_Q),y=wy(e->y/DK_Q);
    if(e->type==DK_SPRING) {
      rect(x-5,y-8,10,1,CYAN);rect(x-5,y-1,10,1,CYAN);
      for(int j=0;j<3;++j)line(x-5,y-7+j*2,x+4,y-5+j*2,WHITE);
    } else {
      const dk_sprite *p=e->type==DK_BARREL?&spr_barrel:e->type==DK_FIRE?&spr_fire:&spr_tub;
      sprite(x-p->w/2,y-p->h,p,e->dir,1,e->blue);
      if(e->type==DK_BARREL)line(x-3,y-5,x+3,y-5+(int)(g->age/6%3)-1,GOLD);
    }
  }
  const dk_player *p=&g->player;
  if(g->phase!=DK_DYING||(g->timer/7)&1)
    mario(wx(p->x/DK_Q),wy(p->y/DK_Q),p->face,p->ladder>=0,g->phase==DK_DYING,
          (int)(g->tick/7&1)&&((g->previous_input&(DK_LEFT|DK_RIGHT|DK_UP|DK_DOWN))!=0),p->hammer!=0);
  if(g->popup_time)number(wx(g->popup_x-8),wy(g->popup_y),g->popup_value,1,WHITE);
}
static void side(void) {
  text(8,12,"1UP",1,RED);number(8,24,g->score,1,WHITE);
  text(8,43,"HI SCORE",1,DIM);number(8,55,s->best,1,GOLD);
  rect(8,73,65,1,STEEL);
  text(8,84,"ROUND",1,DIM);number(48,84,g->round,1,WHITE);
  static const char *const stage[]={"25M","50M","75M","100M"};
  text(8,103,stage[g->board],2,CYAN);
  text(8,126,"BONUS",1,DIM);number(8,138,g->bonus,1,GOLD);
  text(8,160,"LIVES",1,DIM);
  for(unsigned i=0;i<g->lives&&i<4;++i)mario(14+(int)i*16,189,1,false,false,0,false);
  if(g->lives>4)number(61,181,g->lives,1,WHITE);
  if(g->player.hammer) { // eight seconds, shown without an extra HUD row
    rect(8,197,64,3,STEEL);rect(8,197,g->player.hammer*64/480,3,GOLD);
  }
  text(8,214,"BACK",1,DIM);text(8,226,"PAUSE",1,WHITE);
}
static void button(int x,int y,int w,const char *label,bool on) {
  rect(x,y,w,20,on?GOLD:PANEL);
  text(x+8,y+6,label,1,on?BLACK:WHITE);
  if(on)text(x+w-12,y+6,">",1,BLACK);
}
static void overlay(const char *title,int height) {
  int y=(240-height)/2;rect(94,y,210,height,PANEL);frame(94,y,210,height,GOLD);
  center(199,y+12,title,1,GOLD);
}
static void title(void) {
  // A vertical control panel next to a live-looking attract board.
  text(7,14,"DONKEY",2,RED);text(7,33,"KONG",2,GOLD);
  text(8,58,"ARCADE",1,CYAN);rect(8,74,65,1,STEEL);
  text(8,84,"BEST",1,DIM);number(8,96,s->best,1,WHITE);
  button(4,123,76,ongoing?"RESUME":"PLAY",selected==0);
  button(4,148,76,"HOW TO",selected==1);
  button(4,173,76,"EXIT",selected==2);
  text(8,211,"BY",1,DIM);text(8,224,"BOBYDELUXE",1,WHITE);
  overlay("HOW HIGH CAN YOU GET?",54);center(199,119,"FOUR BOARDS. ONE RESCUE.",1,WHITE);
  center(199,134,clock_ms/600%2?"PRESS OK":"",1,CYAN);
}
static void help(void) {
  rect(0,0,320,240,BLACK);rect(0,0,320,30,PANEL);text(12,10,"HOW TO PLAY",2,GOLD);
  text(12,43,"LEFT / RIGHT",1,CYAN);text(132,43,"MOVE",1,WHITE);
  text(12,59,"UP / DOWN",1,CYAN);text(132,59,"CLIMB LADDERS",1,WHITE);
  text(12,75,"OK / EXE / SHIFT",1,CYAN);text(132,75,"JUMP",1,WHITE);
  text(12,91,"BACK",1,CYAN);text(132,91,"PAUSE",1,WHITE);
  rect(12,107,296,1,STEEL);
  text(12,119,"25M  JUMP BARRELS. CLIMB TO PAULINE.",1,WHITE);
  text(12,135,"50M  WATCH THE BELTS AND OIL FIRE.",1,WHITE);
  text(12,151,"75M  RIDE LIFTS. DODGE THE SPRINGS.",1,WHITE);
  text(12,167,"100M REMOVE ALL EIGHT BLUE RIVETS.",1,WHITE);
  text(12,186,"HAMMER: NO JUMPING OR CLIMBING.",1,GOLD);
  text(12,201,"LONG FALLS HURT. EXTRA LIFE: 7000.",1,DIM);
  text(12,223,"ARCADE ADAPTATION / BY BOBYDELUXE",1,CYAN);
  text(12,213,"ORIGINAL GAME: NINTENDO / 1981",1,DIM);
  text(278,12,"BACK",1,WHITE);
}
static void scene(void) {
  rect(0,top,320,8,BLACK);
  if(view==DK_HELP){help();return;}
  side();world();
  if(view==DK_TITLE) {rect(0,0,84,240,BLACK);title();}
  else if(view==DK_PLAY) {
    if(g->phase==DK_READY) {overlay("GET READY",52);center(199,123,"SAVE PAULINE!",1,WHITE);}
    if(g->phase==DK_CLEAR) {overlay(g->board==3?"PAULINE IS SAFE!":"BOARD CLEAR",42);center(199,123,"BONUS ADDED",1,WHITE);}
    if(g->phase==DK_DYING)center(199,15,g->reason==DK_TIME?"TIME UP!":"TRY AGAIN!",1,RED);
  } else if(view==DK_PAUSE) {
    overlay("PAUSED",116);button(108,91,182,"RESUME",selected==0);
    button(108,117,182,"RESTART",selected==1);button(108,143,182,"TITLE",selected==2);
  } else if(view==DK_CONFIRM) {
    overlay("RESTART THIS RUN?",94);button(108,105,182,"KEEP PLAYING",selected==0);
    button(108,131,182,"RESTART",selected==1);
  } else if(view==DK_RESULT) {
    overlay("GAME OVER",122);center(199,82,"FINAL SCORE",1,DIM);number(170,97,g->score,2,WHITE);
    button(108,121,182,"PLAY AGAIN",selected==0);button(108,147,182,"TITLE",selected==1);
  } else if(view==DK_SAVE_ERROR) {
    overlay("STORAGE FULL",108);center(199,96,"RECORD COULD NOT BE SAVED",1,WHITE);
    center(199,114,"OK: RETRY AND EXIT",1,GOLD);center(199,132,"1: KEEP PLAYING",1,WHITE);
    center(199,150,"BACK: EXIT WITHOUT SAVING",1,DIM);
  }
  if(failed&&view!=DK_SAVE_ERROR)rect(77,3,4,4,RED);
}
void dk_render(const dk_game *game,const dk_stats *stats,unsigned page,unsigned selection,
               bool continuing,bool save_failed,uint32_t ms,dk_push push) {
  g=game;s=stats;view=page;selected=selection;ongoing=continuing;failed=save_failed;clock_ms=ms;
  for(unsigned strip=0;strip<30;++strip) {
    top=(int)strip*8;scene();uint32_t hash=2166136261u;
    for(unsigned i=0;i<320*8;++i)hash=(hash^band[i])*16777619u;
    if(!valid||hashes[strip]!=hash)push(0,top,320,8,band);
    hashes[strip]=hash;
  }
  valid=true;
}
