/* SPDX-License-Identifier: GPL-3.0-only
 * An ink-and-graph-paper field notebook. All artwork is procedural.
 * One 5,120-byte strip buffer; no framebuffer, sprites, heap or floating point.
 */
#include "render.h"
#include <stdlib.h>
#include <string.h>
#define C(v) ((uint16_t)((((v)>>8)&0xF800)|(((v)>>5)&0x07E0)|(((v)>>3)&31)))
#define PAPER C(0xFAF4DD)
#define INK C(0x294853)
#define GRID C(0xDBE5D8)
#define GREEN C(0x8DBD69)
#define BLUE C(0x71A6B5)
#define RED C(0xC76A55)
#define LIGHT C(0xFFFBEF)
#define PENCIL C(0x7B8D87)
static uint16_t band[320*8];
static int x0,y0,x1,y1;
static int16_t lo[30],hi[30];
static bool initialized;
static const dj_game *world;
static const dj_stats *stats;
static unsigned page,selection;
static uint32_t clock_ms;
static bool failed;
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
void dj_invalidate(void) {initialized=false;mark(0,0,320,240);}
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
static void arrow(int x,int y,int direction,uint16_t color) {
  line(x,y,x+direction*12,y,color);line(x+direction*12,y,x+direction*7,y-4,color);
  line(x+direction*12,y,x+direction*7,y+4,color);
}
static void paper(bool ruled) {
  rect(x0,y0,x1-x0,y1-y0,PAPER);
  if(ruled)for(int y=y0;y<y1;++y)if(y%24==0)rect(0,y,320,1,GRID);
}
static void hero(int x,int foot,int size,bool face,unsigned bounce,unsigned boost) {
  if(foot<y0||foot-35*size>=y1||x+20*size<x0||x-20*size>=x1)return;
  int top=foot-18*size+(bounce?size:0),nose=face?1:-1;
  if(boost) {
    int tail=(boost&2)?11:8;
    line(x-9*size,foot,x-11*size,foot+tail*size,RED);
    line(x-8*size,foot,x-7*size,foot+(tail-3)*size,C(0xD8AB45));
  }
  circle(x,top,10*size,INK);rect(x-10*size,top,20*size,12*size,INK);
  circle(x,top,9*size,GREEN);rect(x-9*size,top,18*size,11*size,GREEN);
  rect(x-9*size,top+11*size,18*size,size,INK);
  for(int k=-5;k<=5;k+=5)line(x+k*size,top+3*size,x+(k-1)*size,top+9*size,C(0x6B9955));
  rect(x-14*size,top+3*size,5*size,11*size,INK);
  rect(x-13*size,top+4*size,3*size,8*size,boost?RED:BLUE);
  rect(x+nose*8*size-(nose<0?7*size:0),top-size,9*size,6*size,INK);
  rect(x+nose*8*size-(nose<0?6*size:0),top,8*size,4*size,GREEN);
  for(int i=0;i<2;++i) {
    int ex=x+(face?2+i*4:-5-i*4)*size;
    rect(ex,top-5*size,3*size,5*size,LIGHT);rect(ex+size,top-3*size,size,2*size,INK);
  }
  line(x-6*size,top+12*size,x-7*size,foot,INK);line(x+5*size,top+12*size,x+7*size,foot,INK);
  line(x-9*size,foot,x-4*size,foot,INK);line(x+4*size,foot,x+10*size,foot,INK);
}
static void spring(int x,int y) {
  for(int k=0;k<4;++k)line(x+(k&1?4:-4),y-2-k*2,x+(k&1?-4:4),y-4-k*2,INK);
  line(x-5,y-10,x+5,y-10,RED);
}
static void rocket(int x,int y) {
  line(x-4,y,x-4,y-12,INK);line(x+4,y,x+4,y-12,INK);
  line(x-4,y-12,x,y-17,INK);line(x,y-17,x+4,y-12,INK);
  rect(x-3,y-11,7,10,RED);circle(x,y-9,2,LIGHT);
  line(x-4,y-2,x-7,y+2,INK);line(x+4,y-2,x+7,y+2,INK);
}
static void ledge(int x,int y,int w,unsigned type,unsigned item) {
  if(y+6<y0||y-18>=y1)return;
  uint16_t color=type==DJ_MOVING?BLUE:type==DJ_BRITTLE?C(0xB8936E):type==DJ_FADING?LIGHT:GREEN;
  int left=x-w/2;rect(left,y,w,6,INK);rect(left+1,y+1,w-2,4,color);
  line(left+3,y+2,left+w-5,y+2,LIGHT);
  if(type==DJ_BRITTLE){line(x-4,y,x+1,y+3,INK);line(x+1,y+3,x-2,y+6,INK);}
  if(type==DJ_MOVING){line(x-4,y+3,x+4,y+3,INK);pixel(x-4,y+2,INK);pixel(x+4,y+4,INK);}
  if(type==DJ_FADING)for(int k=left+6;k<left+w-3;k+=6)pixel(k,y+4,PENCIL);
  if(item==DJ_SPRING)spring(x,y);else if(item==DJ_ROCKET)rocket(x,y);
}
static void monster(int x,int y,unsigned tick) {
  if(y+12<y0||y-12>=y1)return;
  circle(x,y,11,INK);circle(x,y,10,C(0xD99890));
  for(int k=-8;k<=8;k+=8){line(x+k,y-7,x+k-3,y-14,INK);circle(x+k-3,y-14,1,INK);}
  circle(x-4,y-3,3,LIGHT);circle(x+4,y-3,3,LIGHT);
  circle(x-3,y-3,1,INK);circle(x+5,y-3,1,INK);
  line(x-4,y+4,x+4,y+4,INK);
  for(int k=-7;k<=7;k+=7)line(x+k,y+8,x+k+(tick&8?3:-3),y+13,INK);
}
static void flight(void) {
  const dj_game *g=world;
  paper(false);
  for(unsigned i=0;i<DJ_PLATFORMS;++i) {
    const dj_platform *p=&g->platform[i];
    if(!p->active)continue;
    int y=227-(int)((p->y-g->camera)/DJ_Q);
    if(y>=31&&y<=234) {
      int px=16+(int)(dj_platform_x(p,g->tick)/DJ_Q);
      ledge(px,y,p->width,p->type,p->item);
    }
  }
  for(unsigned i=0;i<DJ_ENEMIES;++i) {
    const dj_enemy *e=&g->enemy[i];
    if(!e->active)continue;
    int y=227-(int)((e->y-g->camera)/DJ_Q);
    if(y>=38&&y<=236) {
      int px=16+(int)(dj_enemy_x(e,g->tick)/DJ_Q);unsigned tick=stats->motion?g->tick:0;
      monster(px,y,tick);
    }
  }
  for(unsigned i=0;i<DJ_SHOTS;++i) {
    const dj_shot *b=&g->shot[i];
    if(!b->active)continue;
    int y=227-(int)((b->y-g->camera)/DJ_Q);
    if(y>=29&&y<228)circle(16+b->x/DJ_Q,y,2,INK);
  }
  int x=16+g->x/DJ_Q,y=227-(int)((g->y-g->camera)/DJ_Q);
  unsigned boost=g->boost?(stats->motion?g->boost:1):0;
  hero(x,y,1,g->face,stats->motion?g->bounce:0,boost);
  rect(0,0,320,26,PAPER);line(21,25,316,25,GRID);
  text(26,5,"HEIGHT",1,PENCIL);number(72,5,dj_height(g),2,INK);
  text(210,5,"BEST",1,PENCIL);number(241,5,stats->best,1,INK);
  if(g->boost){rect(211,17,g->boost,3,RED);text(146,16,"ROCKET",1,RED);}
  rect(0,228,320,12,PAPER);line(21,228,316,228,GRID);
  text(25,232,"< > MOVE",1,INK);text(101,232,"OK SHOOT",1,INK);text(229,232,"BACK PAUSE",1,PENCIL);
}
static void entry(int y,const char *label,bool active) {
  text(32,y,label,1,active?INK:PENCIL);
  if(active){arrow(207,y+4,1,RED);line(32,y+11,168,y+11,RED);}
}
static void title(void) {
  paper(true);
  text(31,33,"DOODLE",3,INK);text(32,61,"JUMP",3,INK);
  line(31,87,158,89,RED);
  text(32,125,"BEST",1,PENCIL);number(81,123,stats->best,2,INK);
  unsigned t=stats->motion?(clock_ms/100)%16:4;
  int lift=t<8?(int)t:15-(int)t;
  ledge(259,144,53,DJ_SOLID,DJ_SPRING);
  hero(256,126-lift*2,2,true,0,0);
  bool resume=world->present&&world->phase!=DJ_OVER;
  entry(163,resume?"CONTINUE CLIMB":"START CLIMBING",selection==0);
  entry(184,"FIELD NOTES",selection==1);entry(205,"HOW TO PLAY",selection==2);
  text(30,231,"BY BOBYDELUXE",1,PENCIL);text(215,231,"OK OPEN",1,PENCIL);
}
static void heading(const char *name) {
  paper(true);text(30,17,name,2,INK);line(30,38,300,39,RED);
}
static void journal(void) {
  heading("FIELD NOTES");
  const char *labels[]={"HIGHEST CLIMB","EXPEDITIONS","BOUNCES","SPRINGS USED","ROCKET RIDES","MONSTERS CLEARED"};
  uint32_t values[]={stats->best,stats->runs,stats->jumps,stats->springs,stats->rockets,stats->monsters};
  for(unsigned i=0;i<6;++i) {
    int y=71+(int)i*20;text(31,y,labels[i],1,INK);number(226,y,values[i],1,INK);line(31,y+12,296,y+12,GRID);
  }
  text(31,201,"ANIMATIONS",1,INK);text(226,201,stats->motion?"ON":"REDUCED",1,RED);
  text(31,217,"OK / LEFT / RIGHT TO CHANGE",1,PENCIL);text(263,231,"BACK",1,PENCIL);
}
static void guide(void) {
  heading("HOW TO CLIMB");
  text(31,69,"LEFT / RIGHT  STEER",1,INK);
  text(31,85,"OK / EXE / UP  SHOOT UPWARD",1,INK);
  text(31,101,"BACK  PAUSE    HOME  SAVE / EXIT",1,INK);
  ledge(57,128,43,DJ_SOLID,0);text(91,128,"SAFE LANDING",1,INK);
  ledge(57,147,43,DJ_MOVING,0);text(91,147,"MOVES SIDE TO SIDE",1,INK);
  ledge(57,166,43,DJ_BRITTLE,0);text(91,166,"BREAKS. NO BOUNCE!",1,RED);
  ledge(57,185,43,DJ_FADING,0);text(91,185,"ONE BOUNCE ONLY",1,INK);
  spring(47,214);rocket(74,217);text(96,207,"LAND ON CENTRE TO BOOST",1,INK);
  text(31,231,"STOMP OR SHOOT MONSTERS",1,PENCIL);text(263,231,"BACK",1,PENCIL);
}
static void sheet(void) {
  rect(31,40,254,155,LIGHT);
  line(31,40,285,40,INK);line(31,40,31,194,INK);line(285,40,285,194,INK);line(31,194,285,194,INK);
}
static void choice(int y,const char *label,unsigned id) {
  text(56,y,label,1,selection==id?INK:PENCIL);
  if(selection==id){arrow(40,y+3,1,RED);line(56,y+11,264,y+11,RED);}
}
static void modal(void) {
  sheet();
  if(page==DJ_READY) {
    center(158,57,"A LITTLE HIGHER?",2,INK);
    center(158,85,"BOUNCING IS AUTOMATIC.",1,PENCIL);
    center(158,101,"LEFT / RIGHT TO FIND A LEDGE.",1,INK);
    hero(157,151,1,true,0,0);ledge(157,153,59,DJ_SOLID,0);
    center(158,173,"OK TO BEGIN",1,RED);
  } else if(page==DJ_PAUSE) {
    center(158,55,"PENCIL DOWN.",2,INK);
    center(158,78,failed?"SAVE FAILED - STORAGE FULL":"YOUR CLIMB IS SAVED.",1,failed?RED:PENCIL);
    choice(99,"KEEP CLIMBING",0);choice(120,"NEW EXPEDITION",1);choice(141,"HOW TO PLAY",2);choice(162,"TITLE PAGE",3);
  } else if(page==DJ_CONFIRM) {
    center(158,57,"A FRESH PAGE?",2,INK);
    center(158,88,"YOUR CURRENT CLIMB WILL END.",1,PENCIL);
    center(158,104,"YOUR RECORDS WILL STAY.",1,PENCIL);
    choice(133,"KEEP THIS CLIMB",0);choice(162,"START NEW",1);
  } else if(page==DJ_SAVE_ERROR) {
    center(158,55,"SAVE FAILED",2,RED);
    center(158,84,"NOT ENOUGH FREE STORAGE",1,INK);
    center(158,101,"OR THE SAVE CANNOT BE WRITTEN.",1,PENCIL);
    center(158,125,"OK RETRY SAVE",1,INK);center(158,146,"1 RETURN TO YOUR CLIMB",1,INK);
    center(158,175,"BACK EXIT WITHOUT SAVING",1,RED);
  } else {
    center(158,55,"END OF THIS PAGE",2,INK);
    center(158,80,world->reason==DJ_HIT?"A MONSTER CAUGHT YOU.":"THE NEXT LEDGE IS WAITING.",1,PENCIL);
    text(59,104,"HEIGHT",1,PENCIL);number(112,99,dj_height(world),2,INK);
    text(212,104,"BEST",1,PENCIL);number(212,119,stats->best,1,INK);
    choice(142,"TRY AGAIN",0);choice(168,"TITLE PAGE",1);
  }
}
static unsigned old_page=99,old_selection,old_motion,old_boost;
static uint32_t old_tick,old_height,old_best,old_clock;
static bool old_failed;
void dj_render(const dj_game *g,const dj_stats *s,unsigned view,unsigned selected,uint32_t ms,bool save_failed,dj_push push) {
  world=g;stats=s;page=view;selection=selected;clock_ms=ms;failed=save_failed;
  bool full=!initialized||view!=old_page||selected!=old_selection||s->motion!=old_motion||failed!=old_failed;
  if(full)mark(0,0,320,240);
  else if(view==DJ_PLAY&&old_tick!=g->tick)mark(0,26,320,202);
  else if(view==DJ_TITLE&&s->motion&&old_clock!=ms/100)mark(221,54,79,100);
  if(view==DJ_PLAY&&(old_height!=dj_height(g)||old_best!=s->best||old_boost!=g->boost))mark(0,0,320,26);
  old_page=view;old_selection=selected;old_motion=s->motion;old_failed=failed;
  old_tick=g->tick;old_height=dj_height(g);old_best=s->best;old_boost=g->boost;old_clock=ms/100;
  for(int row=0;row<30;++row) {
    if(hi[row]<=lo[row])continue;
    x0=lo[row];x1=hi[row];y0=row*8;y1=y0+8;
    if(view==DJ_TITLE)title();else if(view==DJ_JOURNAL)journal();else if(view==DJ_GUIDE)guide();
    else {
      if(g->present)flight();else title();
      if(view!=DJ_PLAY)modal();
    }
    push(x0,y0,x1-x0,8,band);lo[row]=320;hi[row]=0;
  }
  initialized=true;
}
