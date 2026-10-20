/* SPDX-License-Identifier: GPL-3.0-only
 * Halfbrick sprites adapted for a 5 KiB strip renderer; no framebuffer.
 */
#include "render.h"
#include "assets.h"
#include <stdlib.h>
#include <string.h>
#define C(v) ((uint16_t)((((v)>>8)&0xF800)|(((v)>>5)&0x07E0)|(((v)>>3)&31)))
#define INK C(0x080D18)
#define BG C(0x202B3E)
#define PANEL C(0x35445D)
#define EDGE C(0x748097)
#define MUTED C(0xBAC4D2)
#define WHITE C(0xFFF9E8)
#define CYAN C(0x70C9F4)
#define AMBER C(0xFFD04B)
#define RED C(0xF57862)
static uint16_t band[320*8];
static int x0,y0,x1,y1;
static int16_t lo[30],hi[30];
static bool initialized;
static const jj_game *world;
static const jj_stats *stats;
static unsigned page,selection;
static bool failed;
static uint32_t menu_frame,old_menu_frame;
void jj_menu_time(uint32_t ms) {menu_frame=ms/80;}
static const uint8_t alphabet[][5] = {
 {0x7e,0x11,0x11,0x11,0x7e},{0x7f,0x49,0x49,0x49,0x36},{0x3e,0x41,0x41,0x41,0x22},
 {0x7f,0x41,0x41,0x22,0x1c},{0x7f,0x49,0x49,0x49,0x41},{0x7f,0x09,0x09,0x09,0x01},
 {0x3e,0x41,0x49,0x49,0x7a},{0x7f,0x08,0x08,0x08,0x7f},{0x00,0x41,0x7f,0x41,0x00},
 {0x20,0x40,0x41,0x3f,0x01},{0x7f,0x08,0x14,0x22,0x41},{0x7f,0x40,0x40,0x40,0x40},
 {0x7f,0x02,0x0c,0x02,0x7f},{0x7f,0x04,0x08,0x10,0x7f},{0x3e,0x41,0x41,0x41,0x3e},
 {0x7f,0x09,0x09,0x09,0x06},{0x3e,0x41,0x51,0x21,0x5e},{0x7f,0x09,0x19,0x29,0x46},
 {0x46,0x49,0x49,0x49,0x31},{0x01,0x01,0x7f,0x01,0x01},{0x3f,0x40,0x40,0x40,0x3f},
 {0x1f,0x20,0x40,0x20,0x1f},{0x3f,0x40,0x38,0x40,0x3f},{0x63,0x14,0x08,0x14,0x63},
 {0x07,0x08,0x70,0x08,0x07},{0x61,0x51,0x49,0x45,0x43},
 {0x3e,0x51,0x49,0x45,0x3e},{0x00,0x42,0x7f,0x40,0x00},{0x62,0x51,0x49,0x49,0x46},
 {0x22,0x41,0x49,0x49,0x36},{0x18,0x14,0x12,0x7f,0x10},{0x27,0x45,0x45,0x45,0x39},
 {0x3c,0x4a,0x49,0x49,0x30},{0x01,0x71,0x09,0x05,0x03},{0x36,0x49,0x49,0x49,0x36},
 {0x06,0x49,0x49,0x29,0x1e},
 {0,0x08,0x3e,0x08,0},{0,0x08,0x08,0x08,0},{0,0x60,0x60,0,0},
 {0,0x36,0x36,0,0},{0x20,0x10,0x08,0x04,0x02},{0,0,0x5f,0,0},
 {0x08,0x14,0x22,0x41,0},{0x41,0x22,0x14,0x08,0},{0,0,0x7f,0,0}
};

static void mark(int x,int y,int w,int h) {
  if(x<0)w+=x,x=0;
  if(y<0)h+=y,y=0;
  if(x+w>320)w=320-x;
  if(y+h>240)h=240-y;
  if(w<=0||h<=0)return;
  for(int row=y/8;row<=(y+h-1)/8;++row) {
    if(x<lo[row])lo[row]=(int16_t)x;
    if(x+w>hi[row])hi[row]=(int16_t)(x+w);
  }
}
void jj_invalidate(void) {initialized=false;mark(0,0,320,240);}
static void pixel(int x,int y,uint16_t color) {
  if(x>=x0&&x<x1&&y>=y0&&y<y1)band[(y-y0)*(x1-x0)+x-x0]=color;
}
static void rect(int x,int y,int w,int h,uint16_t color) {
  int l=x<x0?x0:x,t=y<y0?y0:y,r=x+w>x1?x1:x+w,b=y+h>y1?y1:y+h;
  if(l>=r||t>=b)return;
  for(int j=t;j<b;++j) {
    uint16_t *p=band+(j-y0)*(x1-x0)+l-x0;
    for(int i=l;i<r;++i)*p++=color;
  }
}
static void line(int a,int b,int c,int d,uint16_t color) {
  if((a<x0&&c<x0)||(a>=x1&&c>=x1)||(b<y0&&d<y0)||(b>=y1&&d>=y1))return;
  int dx=abs(c-a),sx=a<c?1:-1,dy=-abs(d-b),sy=b<d?1:-1,e=dx+dy;
  for(;;) {
    pixel(a,b,color);
    if(a==c&&b==d)break;
    int twice=e*2;
    if(twice>=dy)e+=dy,a+=sx;
    if(twice<=dx)e+=dx,b+=sy;
  }
}
static void circle(int x,int y,int r,uint16_t color) {
  if(y+r<y0||y-r>=y1||x+r<x0||x-r>=x1)return;
  int start=y-r>y0?y-r:y0,end=y+r<y1?y+r:y1-1;
  for(int j=start;j<=end;++j) {
    int dy=j-y,half=0;
    while((half+1)*(half+1)+dy*dy<=r*r)++half;
    rect(x-half,j,half*2+1,1,color);
  }
}
static void text(int x,int y,const char *s,int scale,uint16_t color) {
  if(y>=y1||y+7*scale<=y0)return;
  for(;*s;++s,x+=6*scale) {
    unsigned ch=(unsigned char)*s,i;
    if(ch>='a'&&ch<='z')ch-=32;
    if(ch>='A'&&ch<='Z')i=ch-'A';
    else if(ch>='0'&&ch<='9')i=ch-'0'+26;
    else {
      static const char punctuation[]="+-.:/!<>|";const char *p=strchr(punctuation,(int)ch);
      if(!p||ch==' ')continue;
      i=36+(unsigned)(p-punctuation);
    }
    for(int col=0;col<5;++col)for(int row=0;row<7;++row)
      if(alphabet[i][col]&(1<<row))rect(x+col*scale,y+row*scale,scale,scale,color);
  }
}
static void center(int x,int y,const char *s,int scale,uint16_t color) {
  text(x-((int)strlen(s)*6*scale-scale)/2,y,s,scale,color);
}
static void number(int x,int y,uint32_t value,int scale,uint16_t color) {
  char s[11];unsigned n=0;
  do{s[n++]=(char)('0'+value%10);value/=10;}while(value);
  for(unsigned i=0;i<n/2;++i){char c=s[i];s[i]=s[n-i-1];s[n-i-1]=c;}
  s[n]=0;text(x,y,s,scale,color);
}

static void sprite(const jj_sprite *im,int x,int y,int scale,const uint16_t *palette) {
  if(x+(int)im->w*scale<=x0||x>=x1||y+(int)im->h*scale<=y0||y>=y1)return;
  if(!palette)palette=im->palette;
  int first=y<y0?(y0-y)/scale:0,last=(y1-y+scale-1)/scale;
  if(last>im->h)last=im->h;
  for(int row=first;row<last;++row) {
    const uint8_t *p=im->data+im->rows[row],*end=im->data+im->rows[row+1];
    int xx=x,yy=y+row*scale;
    while(p<end) {
      unsigned tag=*p++,n=(tag&127)+1;
      if(tag&128) {
        unsigned color=*p++;
        if(color)rect(xx,yy,(int)n*scale,scale,palette[color]);
        xx+=(int)n*scale;
      } else {
        for(unsigned i=0;i<n;++i,xx+=scale) {
          unsigned color=im->bits==4?(p[i/2]>>(i&1?0:4))&15:p[i];
          if(color&&xx+scale>x0&&xx<x1)rect(xx,yy,scale,scale,palette[color]);
        }
        p+=im->bits==4?(n+1)/2:n;
      }
    }
  }
}
static void outlined(int x,int y,const char *s,int scale,uint16_t color) {
  text(x-1,y,s,scale,INK);text(x+1,y,s,scale,INK);
  text(x,y-1,s,scale,INK);text(x,y+1,s,scale,INK);text(x,y,s,scale,color);
}
static uint16_t suit_color(unsigned suit) {
  static const uint16_t colors[]={AMBER,CYAN,C(0xF58BB5),C(0xA8D864)};
  return colors[suit&3];
}
static void pilot(int x,int y,int scale,bool thrust,unsigned tick,unsigned suit) {
  unsigned frame=thrust?4+(tick/4)%4:(tick/5)%4;
  if(thrust) {
    /* Machine-gun exhaust: a real muzzle flash and a few deterministic shots. */
    sprite(jj_flash[(tick/3)&1],x-18*scale,y+7*scale,scale,0);
    for(unsigned i=0;i<3;++i) {
      int d=(int)((tick*5+i*13)%36);
      int yy=y+(14+d)*scale;
      if(yy<216)rect(x-12*scale+(int)(i&1)*3*scale,yy,scale,yy+3*scale>216?216-yy:3*scale,AMBER);
    }
  }
  sprite(jj_barry[frame],x-17*scale,y-24*scale,scale,jj_suit_palettes[suit&3]);
}
static void coin(int x,int y,unsigned tick) {
  sprite(jj_coin[(tick/3)&7],x-5,y-5,1,0);
}
static void shield(int x,int y) {
  sprite(&jj_shield,x-9,y-9,1,0);
}
static void cap(int x,int y) {sprite(&jj_zapper_cap,x-7,y-7,1,0);}
static void zapper(int x,int y,unsigned kind,unsigned tick) {
  int w,h;jj_bounds(kind,&w,&h);unsigned frame=(tick/3)%3;
  bool vertical=kind==JJ_VERTICAL||kind==JJ_COLUMN;
  const jj_sprite *im=kind==JJ_HORIZONTAL?jj_beam_h[frame]:kind==JJ_COLUMN?jj_beam_c[frame]:jj_beam_v[frame];
  sprite(im,x-w/2,y-h/2,1,0);
  if(vertical)cap(x,y-h/2),cap(x,y+h/2);
  else cap(x-w/2,y),cap(x+w/2,y);
}
static void rotor(const jj_hazard *p) {
  int dx,dy;jj_rotor(p,world->tick,&dx,&dy);
  dx/=JJ_Q;dy/=JJ_Q;int x=p->x/JJ_Q,y=p->y;
  if(x+45<x0||x-45>=x1||y+45<y0||y-45>=y1)return;
  bool horizontal=abs(dx)>=abs(dy);
  for(int k=-3;k<=3;++k)line(x-dx+(horizontal?0:k),y-dy+(horizontal?k:0),
    x+dx+(horizontal?0:k),y+dy+(horizontal?k:0),k==0?WHITE:k==1||k==-1?AMBER:C(0x906426));
  /* A small travelling arc follows the same finite beam as the collider. */
  int jitter=world->tick&2?2:-2;
  line(x-dx,y-dy,x+jitter,y-jitter,WHITE);
  line(x+jitter,y-jitter,x+dx,y+dy,WHITE);
  cap(x-dx,y-dy);cap(x+dx,y+dy);
}
static void floor_panel(unsigned shift) {
  for(int x=-(int)(shift%256);x<320;x+=256)sprite(&jj_floor,x,216,2,0);
}
static void lab(unsigned shift) {
  rect(x0,y0,x1-x0,y1-y0,INK);
  shift%=256;
  for(int x=-(int)shift;x<320;x+=256)sprite(&jj_lab,x,24,2,0);
  /* Continue the supplied floor/ceiling textures beyond the play area. */
  rect(0,0,320,24,C(0x252B36));rect(0,21,320,3,C(0xA0A7AD));
  floor_panel(shift);
}
static void flight(void) {
  unsigned shift=world->distance/(JJ_Q*2);
  lab(shift);
  for(unsigned i=0;i<3;++i) {
    int x=355-(int)((shift+i*137)%450);
    sprite(jj_scientist[(world->tick/7+i)&3],x,195,1,0);
  }
  for(unsigned i=0;i<JJ_PICKUPS;++i) {
    const jj_pickup *p=&world->pickup[i];if(!p->active)continue;
    int x=p->x/JJ_Q;
    if(p->kind==JJ_COIN)coin(x,p->y,world->tick);else shield(x,p->y);
  }
  for(unsigned i=0;i<JJ_HAZARDS;++i) {
    const jj_hazard *p=&world->hazard[i];if(!p->active)continue;
    int x=p->x/JJ_Q,y=p->y;
    if(p->kind==JJ_ROTATING)rotor(p);
    else if(p->kind!=JJ_MISSILE)zapper(x,y,p->kind,world->tick);
    else if(p->timer) {
      bool locked=p->timer<=35;
      if(locked||world->tick%16<11) {
        rect(286,y-12,32,25,INK);
        sprite(&jj_warning,290,y-9,1,0);
        text(310,y-3,"<",1,locked?RED:AMBER);
      }
    } else {
      sprite(jj_flash[(world->tick/3)&1],x+11,y-8,1,0);
      sprite(&jj_missile,x-14,y-11,1,0);
    }
  }
  int y=world->y/JJ_Q;
  if((world->shield||world->invuln)&&(world->shield||world->tick%8<5)) {
    circle(63,y-3,22,C(0x51C0F2));circle(63,y-3,20,C(0x2B5978));
    circle(63,y-3,18,C(0x203A57));
  }
  pilot(64,y,1,world->thrust,world->tick,stats->suit);
  /* Art below the floor is clipped by the foreground rail. */
  floor_panel(shift);
  rect(0,0,320,24,C(0x252B36));
  uint32_t meters=jj_distance(world),v=meters;unsigned digits=1;while(v>=10)v/=10,++digits;
  number(9,4,meters,2,WHITE);text(13+(int)digits*12,10,"M",1,WHITE);
  coin(183,11,0);number(194,7,world->coins,1,AMBER);
  if(world->shield)shield(274,11);
  rect(300,5,3,11,C(0xAFB8C3));rect(306,5,3,11,C(0xAFB8C3));
}
static void footer(const char *hint) {
  rect(0,219,320,21,INK);rect(0,219,320,1,EDGE);text(12,227,hint,1,MUTED);
}
static void heading(const char *label,const char *sub) {
  lab(64);
  rect(0,0,320,58,INK);rect(0,56,320,2,AMBER);
  outlined(12,12,label,2,WHITE);text(13,38,sub,1,MUTED);
}
static void button(int x,int y,int w,int h,const char *label,bool on) {
  rect(x,y+2,w,h,INK);rect(x,y,w,h-2,on?C(0x9A5E0F):C(0x3A4355));
  rect(x+1,y+1,w-2,h-5,on?C(0xF4B531):C(0x65718A));
  rect(x+2,y+2,w-4,(h-6)/2,on?C(0xFFDA67):C(0x7A879D));
  rect(x+2,y+h-5,w-4,1,on?C(0xF8D050):C(0xA4B0BC));
  center(x+w/2,y+(h-10)/2,label,1,on?INK:WHITE);
  if(on)text(x+8,y+(h-10)/2,">",1,INK);
}
static void title(void) {
  lab(menu_frame);
  sprite(&jj_jetpack_word,17,25,1,0);sprite(&jj_joyride_word,20,79,1,0);
  int bob=(int)((menu_frame/4)%12);if(bob>6)bob=12-bob;
  sprite(&jj_hero,238,40+bob,1,0);
  outlined(15,7,"BEST",1,MUTED);number(49,7,stats->best,1,WHITE);text(98,7,"M",1,MUTED);
  coin(243,10,0);number(254,7,stats->wallet,1,AMBER);
  button(52,148,216,33,world->started&&world->phase!=JJ_OVER?"RESUME RUN":"PLAY",selection==0);
  button(52,187,104,27,"THE STASH",selection==1);
  button(164,187,104,27,"HOW TO PLAY",selection==2);
  rect(0,222,320,18,INK);text(10,228,"BY BOBYDELUXE",1,MUTED);text(255,228,"OK  PLAY",1,WHITE);
}
static const char *const suits[]={"CLASSIC","ARCTIC","ROSE","LIME"};
static void shop(void) {
  heading("THE STASH","OUTFITS");
  coin(230,41,0);number(242,38,stats->wallet,1,AMBER);
  for(unsigned i=0;i<4;++i) {
    int x=9+(int)i*78;bool on=selection==i;
    rect(x-1,70,70,116,INK);rect(x,71,68,114,on?C(0x956320):C(0x4E5E74));
    rect(x+2,73,64,110,on?C(0x394355):C(0x243147));
    if(on)rect(x+2,73,64,2,AMBER),rect(x+2,181,64,2,AMBER);
    center(x+34,81,suits[i],1,on?AMBER:WHITE);
    pilot(x+34,132,1,true,12,i);
    const char *status=stats->suit==i?"EQUIPPED":!i||(stats->owned&(1u<<i))?"OWNED":0;
    if(status)center(x+34,165,status,1,stats->suit==i?CYAN:MUTED);
    else {coin(x+21,168,0);number(x+31,165,jj_price(i),1,AMBER);}
  }
  const char *hint=stats->suit==selection?"EQUIPPED":!selection||(stats->owned&(1u<<selection))?"OK  EQUIP OUTFIT":stats->wallet<jj_price(selection)?"COLLECT MORE COINS":"OK  BUY OUTFIT";
  center(160,202,hint,1,failed?RED:WHITE);
  footer(failed?"SAVE FAILED / HOME TO RETRY":"ARROWS  CHOOSE     BACK  RETURN");
}
static void guide(void) {
  heading("HOW TO PLAY","JETPACK JOYRIDE");
  rect(57,65,256,43,BG);rect(57,113,256,43,BG);rect(57,160,256,49,BG);
  pilot(31,91,1,true,0,stats->suit);
  text(63,73,"HOLD OK / EXE / UP TO RISE.",1,WHITE);
  text(63,89,"RELEASE TO FALL. NO FUEL LIMIT.",1,MUTED);
  zapper(31,139,JJ_VERTICAL,0);
  text(63,119,"DODGE ELECTRIC ZAPPERS.",1,WHITE);
  text(63,135,"SOME ROTATE. MISSILES LOCK ON.",1,MUTED);
  coin(23,185,0);shield(44,185);
  text(63,168,"COINS BUY OUTFITS IN THE STASH.",1,WHITE);
  text(63,184,"SHIELDS PROTECT AGAINST ONE HIT.",1,MUTED);
  text(63,200,"BACK PAUSES. HOME SAVES AND EXITS.",1,MUTED);
  footer("OK / BACK  RETURN");
}
static void choice(int y,const char *label,unsigned id) {
  bool on=selection==id;
  if(on)rect(29,y-5,262,20,C(0x806026)),rect(29,y-5,3,20,AMBER);
  text(42,y,label,1,on?WHITE:MUTED);
  if(on)text(270,y,">",1,AMBER);
}
static void modal(void) {
  rect(17,44,286,158,INK);rect(20,47,280,152,C(0x4D596C));
  rect(23,50,274,146,BG);rect(23,50,274,3,AMBER);
  if(page==JJ_READY) {
    center(160,65,world->started?"READY TO RESUME":"LET'S GO",2,WHITE);
    center(160,101,"HOLD TO RISE. RELEASE TO FALL.",1,MUTED);
    center(160,121,"WATCH OUT FOR ZAPPERS!",1,AMBER);
    center(160,157,world->started?"OK / EXE  RESUME":"OK / EXE  PLAY",1,WHITE);
    center(160,180,"BACK  MAIN MENU",1,MUTED);
  } else if(page==JJ_PAUSE) {
    text(34,61,"PAUSED",2,WHITE);
    text(35,84,failed?"SAVE FAILED - CHECK STORAGE":"PROGRESS SAVED",1,failed?RED:MUTED);
    choice(107,"RESUME",0);choice(129,"NEW RUN",1);
    choice(151,"HOW TO PLAY",2);choice(177,"MAIN MENU",3);
  } else if(page==JJ_CONFIRM) {
    center(160,65,"START A NEW RUN",2,WHITE);
    center(160,97,"THIS RUN WILL END.",1,MUTED);
    center(160,113,"YOUR COINS AND RECORD WILL STAY.",1,MUTED);
    choice(147,"KEEP PLAYING",0);choice(177,"START NEW",1);
  } else if(page==JJ_BUY) {
    unsigned item=selection>>1;
    center(160,65,"BUY OUTFIT",2,WHITE);
    text(35,99,suits[item],1,suit_color(item));number(170,99,jj_price(item),1,AMBER);text(200,99,"COINS",1,MUTED);
    center(160,119,"PERMANENT UNLOCK. NO GAMEPLAY CHANGE.",1,MUTED);
    unsigned saved=selection;selection&=1;
    choice(149,"CANCEL",0);choice(178,"BUY AND EQUIP",1);selection=saved;
  } else if(page==JJ_SAVE_ERROR) {
    center(160,65,"SAVE FAILED",2,RED);
    center(160,95,"FREE STORAGE MAY BE FULL.",1,MUTED);
    center(160,113,"YOUR RUN IS STILL IN MEMORY.",1,MUTED);
    text(35,140,"OK  RETRY SAVE AND EXIT",1,WHITE);
    text(35,162,"1   RETURN TO GAME",1,CYAN);
    text(35,183,"BACK  EXIT WITHOUT SAVING",1,AMBER);
  } else {
    text(34,62,"RUN COMPLETE",2,WHITE);
    text(35,87,world->reason==2?"MISSILE HIT":world->reason==3?"LAB END REACHED":"ZAPPED!",1,AMBER);
    number(35,106,jj_distance(world),3,WHITE);text(35,133,"METERS",1,MUTED);
    coin(235,116,0);number(247,113,world->coins,1,AMBER);
    choice(154,"TRY AGAIN",0);choice(180,"MAIN MENU",1);
  }
}

static unsigned old_page=99,old_selection;
static uint32_t old_tick,old_distance,old_wallet;
static uint16_t old_coins;
static uint8_t old_shield,old_suit;
static bool old_failed;
void jj_render(const jj_game *g,const jj_stats *s,unsigned view,unsigned selected,bool save_failed,jj_push push) {
  world=g;stats=s;page=view;selection=selected;failed=save_failed;
  if(!initialized||view!=old_page||selected!=old_selection||failed!=old_failed||s->suit!=old_suit||
     (view!=JJ_PLAY&&old_wallet!=s->wallet))mark(0,0,320,240);
  else if(view==JJ_PLAY&&old_tick!=g->tick)mark(0,24,320,216);
  else if(view==JJ_TITLE&&menu_frame!=old_menu_frame)mark(0,0,320,240);
  if(view==JJ_PLAY&&(old_distance!=jj_distance(g)||old_coins!=g->coins||old_shield!=g->shield))mark(0,0,320,24);
  old_page=view;old_selection=selected;old_tick=g->tick;old_failed=failed;
  old_distance=jj_distance(g);old_coins=g->coins;old_shield=g->shield;
  old_wallet=s->wallet;old_suit=s->suit;
  old_menu_frame=menu_frame;
  for(int row=0;row<30;++row) {
    if(hi[row]<=lo[row])continue;
    x0=lo[row];x1=hi[row];y0=row*8;y1=y0+8;
    if(view==JJ_TITLE)title();else if(view==JJ_SHOP)shop();else if(view==JJ_GUIDE)guide();
    else if(view==JJ_BUY){rect(x0,y0,x1-x0,y1-y0,BG);modal();}
    else {if(g->present)flight();else title();if(view!=JJ_PLAY)modal();}
    push(x0,y0,x1-x0,8,band);lo[row]=320;hi[row]=0;
  }
  initialized=true;
}
