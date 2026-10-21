#include "render.h"
#include "canvas.h"
#include <math.h>
#define BLACK 0
#define WHITE 65535
#define GREY RGB(200,200,200)
static void tiny_digit(int x,int y,unsigned n,uint16_t c){const uint16_t digits[]={31599,11415,29671,29647,23497,31183,31215,29257,31727,31695};uint16_t bits=digits[n%10];for(int j=0;j<5;j++)for(int i=0;i<3;i++)if(bits&(1u<<(14-j*3-i)))pixel(x+i,y+j,c);}
static void tiny_number(int x,int y,unsigned n){if(n>=10){tiny_digit(x-3,y,n/10,BLACK);tiny_digit(x+1,y,n%10,BLACK);}else tiny_digit(x-1,y,n,BLACK);}
static void button(int x,int y,int w,int h,const char*label,bool on){if(on)rect(x,y,w,h,BLACK);outline(x,y,w,h,BLACK);center(x+w/2,y+(h-14)/2,label,on?WHITE:BLACK,1);}
static void round_rect(int x,int y,int w,int h,int r,uint16_t c){rect(x+r,y,w-2*r,h,c);rect(x,y+r,w,h-2*r,c);circle(x+r,y+r,r,c);circle(x+w-r-1,y+r,r,c);circle(x+r,y+h-r-1,r,c);circle(x+w-r-1,y+h-r-1,r,c);}
static void mask(const Mask*im,int x,int y){for(int py=y<band?band:y;py<y+im->h&&py<band+8;py++)for(int px=0;px<im->w;px++){unsigned i=(py-y)*im->w+px,v=im->pixels[i/2],a=i&1?v&15:v>>4;if(a&&(unsigned)(x+px)<320){uint16_t*p=&strip[(py-band)*320+x+px];*p=blend_alpha(BLACK,*p,a);}}}
void render_invalidate(void){invalidate();}
void render(const Game*g,const Stats*s,unsigned view,unsigned selected,bool ongoing,bool failed,uint32_t now,Push push){
 (void)ongoing;(void)now;
 for(band=0;band<240;band+=8){
  uint16_t bg=view==PLAY?(g->phase==FAILED?RGB(184,17,28):g->phase==CLEARED?RGB(28,176,26):BLACK):GREY;
  rect(0,band,320,8,bg);
  if(view==PLAY){
   for(unsigned i=0;i<g->count;i++){float x,y;pin_position(g,i,&x,&y);line(160,86,(int)lroundf(x),(int)lroundf(y),WHITE);circle((int)lroundf(x),(int)lroundf(y),5,WHITE);}
   circle(160,86,21,WHITE);number(160,78,g->level+1,BLACK,2,true);
   for(unsigned i=0;i<g->remaining&&i<5;i++){int y=171+(int)i*15;circle(160,y,5,WHITE);tiny_number(160,y-2,g->remaining-i);}
   for(unsigned i=0;i<6;i++)if(g->flights[i])circle(160,156+g->flights[i]*5,5,WHITE);
   if(g->phase==FAILED){unsigned radii[]={25,15,20,15};unsigned step=g->phase_age/3;if(step>3)step=3;line(160,86,160,150,WHITE);circle(160,150,(int)(radii[step]*(3.f/7)),WHITE);}
   text(10,8,"CORE BALL",WHITE,1);text(10,220,"BACK: PAUSE",RGB(160,160,160),1);
  }else if(view==TITLE){
   mask(&logo,70,23);round_rect(111,88,98,98,10,BLACK);round_rect(112,89,96,96,9,selected==0?RGB(230,230,230):GREY);mask(&go,120,101);text(129,156,"Level",BLACK,1);number(182,156,s->current+1,BLACK,1,true);
   button(12,112,82,32,"LEVELS",selected==1);button(226,112,82,32,"HOW TO",selected==2);
   text(10,222,"By Bobydeluxe",BLACK,1);if(s->unlocked==60)text(226,222,"60 / 60",BLACK,1);
  }else if(view==LEVELS){
   center(160,15,"SELECT LEVEL",BLACK,1);unsigned page=selected/20;
   for(unsigned i=page*20;i<page*20+20;i++){int x=22+(int)(i%5)*58,y=47+(int)((i%20)/5)*39;bool open=i<=s->unlocked;if(i==selected)rect(x,y,44,29,BLACK);else outline(x,y,44,29,open?BLACK:RGB(170,170,170));if(open)number(x+22,y+6,i+1,i==selected?WHITE:BLACK,1,true);else center(x+22,y+6,"-",RGB(150,150,150),1);}
   center(160,218,"ARROWS: SELECT    OK: PLAY    BACK: MENU",BLACK,1);
  }else if(view==HELP){
   center(160,26,"HOW TO PLAY",BLACK,2);center(160,83,"Press OK, EXE or SHIFT to launch a pin.",BLACK,1);
   center(160,113,"Place every pin on the rotating core.",BLACK,1);center(160,143,"Do not touch another pin.",BLACK,1);center(160,187,"60 original levels. Timing is everything.",BLACK,1);center(160,220,"OK / BACK: RETURN",BLACK,1);
  }else if(view==SAVE_ERROR){center(160,42,"STORAGE FULL",BLACK,2);center(160,104,"Progress could not be saved.",BLACK,1);center(160,145,"OK: RETRY SAVE AND EXIT",BLACK,1);center(160,174,"1: KEEP PLAYING",BLACK,1);center(160,203,"BACK: EXIT WITHOUT SAVING",BLACK,1);
  }else if(view==PAUSE){center(160,38,"PAUSED",BLACK,2);button(84,94,152,32,"RESUME",selected==0);button(84,136,152,32,"RESTART",selected==1);button(84,178,152,32,"MENU",selected==2);
  }else{center(160,26,g->phase==CLEARED?"WELL DONE!":"TRY AGAIN",BLACK,2);number(160,78,g->level+1,BLACK,2,true);center(160,125,g->level==59&&g->phase==CLEARED?"TO BE CONTINUED...":"Core Ball",BLACK,1);button(50,167,102,38,g->phase==CLEARED&&g->level<59?"NEXT":"RETRY",selected==0);button(168,167,102,38,"MENU",selected==1);}
  if(failed&&view!=SAVE_ERROR)text(8,8,"SAVE FAILED",RGB(184,17,28),1);
  present(push);
 }
}
