/* Dino Run — an original native endless runner for NumWorks / NumPlay.
 * Gameplay inspiration: offline dinosaur runners; no third-party source/sprites copied.
 * 320x240, 30 fps, RGB565; draws 16-row bands rather than using a full frame buffer.
 * Keyboard: OK/Up/Exe jump, Down duck, Back/Backspace pause, Home exit.
 */
#include <eadk.h>
#include <stdbool.h>
#include <stdint.h>
#include "../../common/epsilon_app.h"
#include "../../common/epsilon_files.h"

#ifdef __ELF__
const char eadk_app_name[] __attribute__((section(".rodata.eadk_app_name"))) = "Dino Run";
const uint32_t eadk_api_level __attribute__((section(".rodata.eadk_api_level"))) = 0;
#endif

#define W 320
#define H 240
#define GROUND 194
#define DX 34
#define DY (GROUND - 40)
#define BAND 16
#define KEY(k) (1ull << (k))
#define RGB(c) (uint16_t)((((c) >> 8) & 0xF800u) | (((c) >> 5) & 0x07E0u) | (((c) >> 3) & 0x1Fu))
#define JUMP (KEY(eadk_key_ok) | KEY(eadk_key_exe) | KEY(eadk_key_up) | KEY(eadk_key_eight))
#define DUCK (KEY(eadk_key_down) | KEY(eadk_key_two))
#define PAUSE (KEY(eadk_key_back) | KEY(eadk_key_backspace))
#define SAVE_NAME "dinorun.sav"

typedef uint16_t color;
typedef struct { int16_t x, y; uint8_t w, h, kind; } obstacle;
enum { TITLE, HELP, PLAY, PAUSED, DEAD };
enum { CACTUS, LARGE_CACTUS, BIRD };

static color pixels[W * BAND];
static int band_y, scene;
static color bg, ink, faint;
static obstacle obstacles[8];
static int count, dino_y, dino_vy, duck, score, high_score, speed, spawn_left;
static uint32_t rng=0xA319F4B5u, ticks, distance_px;
static bool new_record;

static uint32_t random32(void) {
  rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5;
  return rng;
}
static int randint(int n) { return (int)(random32() % (uint32_t)n); }

static void rect(int x, int y, int w, int h, color c) {
  if (w <= 0 || h <= 0) return;
  int x0=x<0?0:x, x1=x+w>W?W:x+w;
  int y0=y<band_y?band_y:y, y1=y+h>band_y+BAND?band_y+BAND:y+h;
  if (x0 >= x1 || y0 >= y1) return;
  for (int yy=y0; yy<y1; ++yy) {
    color *p=pixels + (yy-band_y)*W + x0;
    for (int xx=x0; xx<x1; ++xx) *p++=c;
  }
}

/* Compact 5 x 7 uppercase font. Each byte holds one row's five bits. */
typedef struct { char ch; uint8_t row[7]; } glyph;
static const glyph FONT[] = {
 {'A',{14,17,17,31,17,17,17}}, {'B',{30,17,17,30,17,17,30}},
 {'C',{14,17,16,16,16,17,14}}, {'D',{30,17,17,17,17,17,30}},
 {'E',{31,16,16,30,16,16,31}}, {'F',{31,16,16,30,16,16,16}},
 {'G',{14,17,16,23,17,17,14}}, {'H',{17,17,17,31,17,17,17}},
 {'I',{31,4,4,4,4,4,31}}, {'J',{7,2,2,2,18,18,12}},
 {'K',{17,18,20,24,20,18,17}}, {'L',{16,16,16,16,16,16,31}},
 {'M',{17,27,21,21,17,17,17}}, {'N',{17,25,21,19,17,17,17}},
 {'O',{14,17,17,17,17,17,14}}, {'P',{30,17,17,30,16,16,16}},
 {'Q',{14,17,17,17,21,18,13}}, {'R',{30,17,17,30,20,18,17}},
 {'S',{15,16,16,14,1,1,30}}, {'T',{31,4,4,4,4,4,4}},
 {'U',{17,17,17,17,17,17,14}}, {'V',{17,17,17,17,17,10,4}},
 {'W',{17,17,17,21,21,21,10}}, {'X',{17,17,10,4,10,17,17}},
 {'Y',{17,17,10,4,4,4,4}}, {'Z',{31,1,2,4,8,16,31}},
 {'0',{14,17,19,21,25,17,14}}, {'1',{4,12,4,4,4,4,14}},
 {'2',{14,17,1,2,4,8,31}}, {'3',{30,1,1,14,1,1,30}},
 {'4',{2,6,10,18,31,2,2}}, {'5',{31,16,30,1,1,17,14}},
 {'6',{6,8,16,30,17,17,14}}, {'7',{31,1,2,4,8,8,8}},
 {'8',{14,17,17,14,17,17,14}}, {'9',{14,17,17,15,1,2,12}},
 {':',{0,4,4,0,4,4,0}}, {'/',{1,1,2,4,8,16,16}},
 {'!',{4,4,4,4,4,0,4}}, {'-',{0,0,0,31,0,0,0}},
 {'+',{0,4,4,31,4,4,0}}, {'.',{0,0,0,0,0,12,12}}
};
static const uint8_t *glyph_rows(char c) {
  for (unsigned i=0;i<sizeof FONT/sizeof FONT[0];i++) if(FONT[i].ch==c) return FONT[i].row;
  return 0;
}
static int text_width(const char *s, int scale) { int n=0; while (*s++) n++; return n*6*scale; }
static void text_at(int x,int y,const char *s,int scale,color c) {
  for (;*s;s++,x+=6*scale) {
    const uint8_t *r=glyph_rows(*s);
    if (!r) continue;
    for (int yy=0;yy<7;yy++) for (int xx=0;xx<5;xx++)
      if (r[yy] & (1u << (4-xx))) rect(x+xx*scale,y+yy*scale,scale,scale,c);
  }
}
static void centered(int y,const char *s,int scale,color c) { text_at((W-text_width(s,scale))/2,y,s,scale,c); }
static void number(int x,int y,unsigned n,int min_digits,int scale,color c) {
  char s[12]; int i=0;
  do {s[i++]=(char)('0'+n%10);n/=10;} while (n && i<10);
  while(i<min_digits && i<10) s[i++]='0';
  for(int j=0;j<i/2;j++) {char t=s[j];s[j]=s[i-1-j];s[i-1-j]=t;}
  s[i]=0;text_at(x,y,s,scale,c);
}

static void dino(int x,int y,bool crouch,int step) {
  if (crouch) {
    rect(x+7,y+15,24,12,ink); rect(x+20,y+10,16,10,ink);
    rect(x+32,y+17,7,5,ink); rect(x+3,y+19,9,4,ink);
    rect(x+25,y+13,2,2,bg); rect(x+10,y+27,7,3,ink); rect(x+26,y+27,7,3,ink);
  } else {
    rect(x+15,y+1,17,14,ink); rect(x+23,y+13,14,5,ink);
    rect(x+17,y+17,14,5,ink);rect(x+10,y+15,16,21,ink);
    rect(x+5,y+22,10,6,ink); rect(x+1,y+19,5,4,ink);
    rect(x+22,y+22,9,3,ink);rect(x+23,y+26,4,4,ink);
    rect(x+24,y+4,3,3,bg);rect(x+30,y+15,7,2,bg);
    if (step&1) {rect(x+12,y+35,5,6,ink);rect(x+25,y+34,6,3,ink);}
    else {rect(x+12,y+34,7,3,ink);rect(x+25,y+35,5,6,ink);}
  }
}
static void cactus(obstacle *o) {
  int x=o->x,y=o->y;
  if (o->kind==LARGE_CACTUS) {
    rect(x+8,y,10,o->h,ink);rect(x+4,y+12,6,6,ink);
    rect(x,y+5,5,15,ink);rect(x+16,y+17,7,5,ink);
    rect(x+19,y+8,5,18,ink);
  } else {
    rect(x+6,y,7,o->h,ink);rect(x+2,y+13,5,5,ink);
    rect(x,y+7,4,12,ink);rect(x+12,y+18,5,4,ink);
    rect(x+15,y+11,4,13,ink);
  }
}
static void bird(obstacle *o) {
  int x=o->x,y=o->y;
  rect(x+6,y+7,20,7,ink);rect(x+25,y+9,7,3,ink);rect(x+3,y+10,5,3,ink);
  rect(x+22,y+8,2,2,bg);
  if ((ticks/7)&1) {rect(x+13,y,8,8,ink);rect(x+9,y+3,5,4,ink);}
  else {rect(x+11,y+13,10,7,ink);rect(x+8,y+16,5,4,ink);}
}
static void scenery(void) {
  rect(0,GROUND+1,W,2,ink);
  /* Daytime clouds; tiny stars replace them in the night. */
  if ((score/300)&1) {
    for(int i=0;i<23;i++) {
      int x=(i*101+23 - (int)(distance_px/8)%570 +570)%570-80;
      int y=34+(i*47)%102;
      rect(x,y,2,2,faint);
    }
  } else {
    for(int i=0;i<4;i++) {
      int x=(i*143+330 - (int)(distance_px/5)%620 +620)%620-85;
      int y=49+(i*31)%70;
      rect(x+10,y,31,2,faint);rect(x+17,y-7,18,2,faint);
      rect(x+9,y-5,2,7,faint);rect(x+35,y-5,2,7,faint);
      rect(x+16,y-9,3,3,faint);
    }
  }
  for(int i=0;i<25;i++) {
    int x=(i*43+17 - (int)(distance_px%1075)+1075)%1075;
    if (x < W) rect(x,GROUND+9+(i*7)%21,3+(i%3),1,faint);
  }
}
static bool overlap(int x,int y,int w,int h,int xx,int yy,int ww,int hh) {
  return x<xx+ww && x+w>xx && y<yy+hh && y+h>yy;
}
static bool collision(void) {
  int y=(dino_y+255)/256;
  int x=DX+8, top=y+4, w=24,h=37;
  if (duck && y>=DY) {x=DX+7;top=GROUND-26;w=32;h=26;}
  for(int i=0;i<count;i++) {
    obstacle *o=&obstacles[i];
    int ox=o->x+2, oy=o->y+2, ow=o->w-4, oh=o->h-3;
    if (o->kind==BIRD) {oy=o->y+6;oh=10;ox=o->x+5;ow=22;}
    if(overlap(x,top,w,h,ox,oy,ow,oh))return true;
  }
  return false;
}
static void start_game(void) {
  scene=PLAY;count=0;score=0;speed=4;spawn_left=260;distance_px=0;
  duck=0;dino_y=DY*256;dino_vy=0;ticks=0;new_record=false;
}
static void spawn(void) {
  if(count>=8)return;
  obstacle *o=&obstacles[count++];
  o->x=W+5;
  if(score>350 && randint(5)==0) {
    o->kind=BIRD;o->w=33;o->h=22;
    o->y=randint(2)?GROUND-46:GROUND-75;
  } else {
    o->kind=randint(3)==0?LARGE_CACTUS:CACTUS;
    o->w=o->kind==LARGE_CACTUS?24:19;
    o->h=o->kind==LARGE_CACTUS?43:32;
    o->y=GROUND-o->h;
  }
}
static void lose(void) {
  scene=DEAD;
  if(score>high_score) {
    high_score=score;new_record=true;
    uint32_t data[2]={0x4F4E4944u,(uint32_t)high_score};
    ef_write(SAVE_NAME,data,sizeof data);
  }
}
static void update(bool jump,bool down) {
  ticks++;score=(int)(ticks/4);speed=4+(score/140);if(speed>10)speed=10;
  if(jump && dino_y>=DY*256 && !down) dino_vy=-2600;
  duck=down && dino_y>=DY*256;
  if(dino_y<DY*256 || dino_vy<0) {
    if(down)dino_vy+=220;
    dino_y+=dino_vy;dino_vy+=155;
    if(dino_y>=DY*256){dino_y=DY*256;dino_vy=0;}
  }
  distance_px+=(uint32_t)speed;
  for(int i=0;i<count;) {
    obstacles[i].x-=(int16_t)speed;
    if(obstacles[i].x+obstacles[i].w<0) {
      for(int j=i+1;j<count;j++) obstacles[j-1]=obstacles[j];
      count--;
    } else i++;
  }
  spawn_left-=speed;
  if(spawn_left<=0) {
    spawn();spawn_left=200+randint(95)+(speed-4)*13;
  }
  if(collision())lose();
}
static void frame(void) {
  bool night=((score/300)&1)!=0;
  bg=RGB(night?0x181D2A:0xF7F7F7);
  ink=RGB(night?0xF1F1F1:0x404040);
  faint=RGB(night?0x7A8193:0xBBBBBB);
  for(band_y=0;band_y<H;band_y+=BAND) {
    for(int i=0;i<W*BAND;i++)pixels[i]=bg;
    scenery();
    if(scene==PLAY || scene==PAUSED || scene==DEAD) {
      for(int i=0;i<count;i++) {
        if(obstacles[i].kind==BIRD)bird(&obstacles[i]);else cactus(&obstacles[i]);
      }
      dino(DX,(dino_y+128)/256,duck,ticks/5);
      text_at(11,11,"HI",2,faint);number(47,11,high_score,5,2,ink);
      number(245,11,(unsigned)score,5,2,ink);
    } else {
      dino(134,101,0,ticks/8);
      centered(40,"DINO RUN",3,ink);
      centered(170,scene==HELP?"OK OR UP JUMP":"PRESS OK TO PLAY",2,ink);
      centered(209,scene==HELP?"DOWN DUCK":"SHIFT FOR HELP",2,faint);
      if(scene==HELP) centered(230,"BACK PAUSE",1,faint);
      else {text_at(9,230,"BEST",1,faint);number(46,230,high_score,5,1,ink);}
    }
    if(scene==DEAD) {
      rect(46,61,228,90,bg);
      centered(73,"GAME OVER",3,ink);
      centered(108,new_record?"NEW RECORD!":"TRY AGAIN!",1,ink);
      centered(127,"OK TO RESTART",2,ink);
    } else if(scene==PAUSED) {
      rect(75,70,170,76,bg);
      centered(82,"PAUSED",3,ink);
      centered(119,"BACK TO RESUME",1,ink);
    }
    eadk_display_push_rect((eadk_rect_t){0,(uint16_t)band_y,W,BAND},pixels);
  }
}

int main(void) {
  np_app_begin();
  uint32_t len=0;
  const uint8_t *save=ef_read(SAVE_NAME,&len);
  if(save && len==8 && save[0]=='D' && save[1]=='I' && save[2]=='N' && save[3]=='O') {
    uint32_t value=(uint32_t)save[4]|(uint32_t)save[5]<<8|(uint32_t)save[6]<<16|(uint32_t)save[7]<<24;
    if(value<100000000u)high_score=(int)value;
  }
  rng^=(uint32_t)eadk_timing_millis()*2654435761u;
  scene=TITLE; dino_y=DY*256;
  uint64_t held=eadk_keyboard_scan();
  for(;;) {
    uint32_t now=(uint32_t)eadk_timing_millis();
    uint64_t keys=eadk_keyboard_scan(),hit=keys & ~held;
    held=keys;
    if(keys&(KEY(eadk_key_home)|KEY(eadk_key_on_off)))break;
    if(scene==TITLE || scene==HELP) {
      if(hit&JUMP) start_game();
      else if(hit&KEY(eadk_key_shift))scene=scene==TITLE?HELP:TITLE;
    } else if(scene==PLAY) {
      if(hit&PAUSE)scene=PAUSED;
      else update((hit&JUMP)!=0,(keys&DUCK)!=0);
    } else if(scene==PAUSED) {
      if(hit&PAUSE)scene=PLAY;
    } else if(scene==DEAD) {
      if(hit&JUMP)start_game();
    }
    frame();
    uint32_t elapsed=(uint32_t)eadk_timing_millis()-now;
    if(elapsed<33)eadk_timing_msleep(33-elapsed);
  }
  return np_app_end();
}
