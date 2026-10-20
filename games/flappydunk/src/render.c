/* SPDX-License-Identifier: GPL-3.0-only
 * Original procedural artwork. No sprites, framebuffer, or heap allocations.
 */
#include "render.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
#define C(v) ((uint16_t)((((v) >> 8) & 0xF800) | (((v) >> 5) & 0x07E0) | (((v) >> 3) & 0x1F)))
#define PAPER C(0xF5EEE3)
#define INK C(0x354650)
#define MUTED C(0x9A958C)
#define GREEN C(0x3F8B87)
#define RED C(0xD76150)
static uint16_t band[320 * 8];
static int x0, y0, x1, y1;
static int16_t dirty_lo[30], dirty_hi[30];
static bool initialized;
static const fd_game *world;
static const fd_stats *stats;
static uint32_t clock_ms;
static unsigned page, selection;
static bool failed;

/* Five columns, seven rows. One tiny alphabet for every page of the game. */
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
static void mark(int x, int y, int w, int h) {
  if (x < 0) w += x, x = 0;
  if (y < 0) h += y, y = 0;
  if (x + w > 320) w = 320 - x;
  if (y + h > 240) h = 240 - y;
  if (w <= 0 || h <= 0) return;
  for (int row = y / 8; row <= (y + h - 1) / 8; ++row) {
    if (x < dirty_lo[row]) dirty_lo[row] = (int16_t)x;
    if (x + w > dirty_hi[row]) dirty_hi[row] = (int16_t)(x + w);
  }
}
void fd_invalidate(void) { mark(0, 0, 320, 240); initialized = false; }
static void pixel(int x, int y, uint16_t c) {
  if (x >= x0 && x < x1 && y >= y0 && y < y1) band[(y - y0) * (x1 - x0) + x - x0] = c;
}
static void rect(int x, int y, int w, int h, uint16_t c) {
  int l = x < x0 ? x0 : x, t = y < y0 ? y0 : y;
  int r = x + w > x1 ? x1 : x + w, b = y + h > y1 ? y1 : y + h;
  if (l >= r || t >= b) return;
  /* Clip once per span instead of branching on every individual pixel. */
  for (int j = t; j < b; ++j) {
    uint16_t *p = band + (j-y0)*(x1-x0) + l-x0;
    for (int i = l; i < r; ++i) *p++ = c;
  }
}
static void roundbox(int x, int y, int w, int h, int radius, uint16_t c) {
  if (y >= y1 || y + h <= y0 || x >= x1 || x + w <= x0) return;
  for (int j = y > y0 ? y : y0; j < y + h && j < y1; ++j) {
    int dy = j < y + radius ? y + radius - j : j >= y + h - radius ? j - (y + h - radius - 1) : 0;
    int inset = dy ? radius - (int)sqrtf((float)(radius * radius - dy * dy)) : 0;
    rect(x + inset, j, w - 2 * inset, 1, c);
  }
}
static void line(int a, int b, int c, int d, uint16_t ink) {
  int dx = abs(c - a), sx = a < c ? 1 : -1, dy = -abs(d - b), sy = b < d ? 1 : -1, err = dx + dy;
  for (;;) {
    pixel(a, b, ink);
    if (a == c && b == d) break;
    int e = 2 * err;
    if (e >= dy) err += dy, a += sx;
    if (e <= dx) err += dx, b += sy;
  }
}
static void circle(int x, int y, int r, uint16_t c, bool outline) {
  if (y + r < y0 || y - r >= y1 || x + r < x0 || x - r >= x1) return;
  for (int j = y - r > y0 ? y - r : y0; j <= y + r && j < y1; ++j) {
    int half = (int)sqrtf((float)(r * r - (j - y) * (j - y)));
    if (outline) {
      pixel(x - half, j, c); pixel(x + half, j, c);
      if (abs(j - y) >= r - 1) rect(x - half, j, 2 * half + 1, 1, c);
    } else rect(x - half, j, 2 * half + 1, 1, c);
  }
}
static void text(int x, int y, const char *s, int scale, uint16_t ink) {
  if (y >= y1 || y + 7 * scale <= y0) return;
  for (; *s; ++s, x += 6 * scale) {
    unsigned ch = (unsigned char)*s, i;
    if (ch >= 'a' && ch <= 'z') ch -= 32;
    if (ch >= 'A' && ch <= 'Z') i = ch - 'A';
    else if (ch >= '0' && ch <= '9') i = ch - '0' + 26;
    else {
      static const char punctuation[] = "+-.:/!<>|";
      const char *p = strchr(punctuation, (int)ch);
      if (!p || ch == ' ') continue;
      i = 36 + (unsigned)(p - punctuation);
    }
    for (int col = 0; col < 5; ++col) for (int row = 0; row < 7; ++row)
      if (alphabet[i][col] & (1 << row)) rect(x + col * scale, y + row * scale, scale, scale, ink);
  }
}
static int width(const char *s, int scale) { return (int)strlen(s) * 6 * scale - scale; }
static void centered(int cx, int y, const char *s, int scale, uint16_t ink) {
  text(cx - width(s, scale) / 2, y, s, scale, ink);
}
static void number(int x, int y, uint32_t value, int scale, uint16_t ink) {
  char s[11]; int n = 0;
  do { s[n++] = (char)('0' + value % 10); value /= 10; } while (value);
  for (int i = 0; i < n / 2; ++i) { char c = s[i]; s[i] = s[n - i - 1]; s[n - i - 1] = c; }
  s[n] = 0;
  text(x, y, s, scale, ink);
}

static uint16_t mix(uint16_t a,uint16_t b,unsigned amount) {
  uint32_t x=((a|(uint32_t)a<<16)&0x07E0F81F),z=((b|(uint32_t)b<<16)&0x07E0F81F);
  x=((x*(32-amount)+z*amount)>>5)&0x07E0F81F;
  return (uint16_t)(x|x>>16);
}
static void ellipse(int x,int y,int rx,int ry,uint16_t color,bool outline) {
  if(ry<=0||rx<=0||y+ry<y0||y-ry>=y1||x+rx<x0||x-rx>=x1)return;
  for(int j=y-ry>y0?y-ry:y0;j<=y+ry&&j<y1;++j) {
    float t=(float)(j-y)/ry;int half=(int)(rx*sqrtf(1-t*t));
    if(!outline||abs(j-y)>=ry-1)rect(x-half,j,2*half+1,1,color);
    else {rect(x-half,j,2,1,color);rect(x+half-1,j,2,1,color);}
  }
}
static void stroke(int x,int y,int xx,int yy,int thick,uint16_t color) {
  for(int i=0;i<thick;++i)line(x,y+i,xx,yy+i,color);
}
static void star(int x,int y,int r,uint16_t color) {
  line(x-r,y,x+r,y,color);line(x,y-r,x,y+r,color);
  if(r>2) {line(x-r+1,y-r+1,x+r-1,y+r-1,color);line(x+r-1,y-r+1,x-r+1,y+r-1,color);}
}
static const uint16_t ball_colors[]={C(0xF6A24E),C(0x54748B),C(0x83C9B6),C(0xEAA5B5),C(0xE8C265),C(0xA595CA)};
static const uint16_t ball_rims[]={C(0xA96334),C(0x304B63),C(0x458C7F),C(0xAA667E),C(0xA08035),C(0x695991)};
static void ball(int x,int y,int r,unsigned skin,unsigned lift,bool happy) {
  if(y+r+3<y0||y-r-12>=y1||x+2*r+8<x0||x-2*r-8>=x1)return;
  int feather=r/3+1,spread=r/2+2,up=(int)lift*r/30;
  for(int side=-1;side<=1;side+=2) {
    for(int i=2;i>=0;--i) {
      int wx=x+side*(r+spread-i*spread/3),wy=y-2-up+i*feather/2;
      ellipse(wx,wy,feather+3,feather,C(0xB7B1A6),false);
      ellipse(wx,wy-1,feather+2,feather-1,C(0xFFFDF6),false);
    }
  }
  circle(x+1,y+2,r,C(0xD9CCB8),false);
  circle(x,y,r,ball_rims[skin],false);circle(x,y-1,r-1,ball_colors[skin],false);
  ellipse(x-r/3,y-r/3,r/4+1,r/5+1,mix(ball_colors[skin],C(0xFFF2CE),21),false);
  int top=y-r+1,bottom=y+r-1;
  stroke(x-r+2,y-1,x+r-2,y-1,r>20?2:1,ball_rims[skin]);
  /* Curved basketball seams, kept inside the ball. */
  for(int row=top;row<=bottom;++row) {
    float yy=(float)(row-y)/r;
    int offset=(int)(r*0.42f*(1-yy*yy));
    pixel(x-offset,row,ball_rims[skin]);pixel(x+offset,row,ball_rims[skin]);
  }
  if(skin==5) {star(x-r/3,y-r/2,r/8+1,C(0xFFF9E8));star(x+r/2,y+r/3,r/9+1,C(0xFFF9E8));}
  int ex=r>18?r/4:3,ey=y+r/5;
  if(happy) {
    line(x-ex-1,ey,x-ex,ey-1,INK);line(x-ex,ey-1,x-ex+1,ey,INK);
    line(x+ex-1,ey,x+ex,ey-1,INK);line(x+ex,ey-1,x+ex+1,ey,INK);
  } else {rect(x-ex,ey,r>25?2:1,r>20?3:2,INK);rect(x+ex,ey,r>25?2:1,r>20?3:2,INK);}
  line(x-2,ey+4,x,ey+5,INK);line(x,ey+5,x+2,ey+4,INK);
}
static void button(int x,int y,int w,const char *label,bool active) {
  roundbox(x,y+2,w,23,6,C(0xD9CEBD));roundbox(x,y,w,23,6,active?GREEN:C(0xEEE5D7));
  centered(x+w/2,y+8,label,1,active?C(0xFFFDF6):INK);
}
static void card(int x,int y,int w,int h) {
  roundbox(x,y+3,w,h,8,C(0xDCCFBB));roundbox(x,y,w,h,8,C(0xFFF9ED));
}
static void wall(int shift,bool court) {
  int top=court?FD_TOP:0,bottom=court?FD_FLOOR:240;
  for(int y=top>y0?top:y0;y<bottom&&y<y1;++y) {
    unsigned amount=(unsigned)((y-top)*9/(bottom-top));
    rect(0,y,320,1,mix(PAPER,C(0xE5DCCB),amount));
  }
  uint16_t mortar=C(0xEAE1D3);
  for(int y=top+9;y<bottom;y+=24) {
    rect(0,y,320,1,mortar);
    int offset=((y-top)/24&1)?32:0;
    for(int x=-64+offset-shift;x<320;x+=64)rect(x,y,1,24,mortar);
  }
  if(court) {
    rect(0,FD_TOP-2,320,2,C(0xD7C6AF));
    rect(0,FD_FLOOR,320,13,C(0xE2BE92));rect(0,FD_FLOOR,320,2,C(0xC59A6C));
    for(int x=-64-shift*2;x<320;x+=48)rect(x,FD_FLOOR+2,1,11,C(0xCFA77A));
  }
}
static void hoop_back(const fd_hoop *h,float half) {
  int x=(int)h->x,y=(int)h->y,w=(int)half;
  if(y+28<y0||y-11>=y1||x+w+4<x0||x-w-4>=x1)return;
  int sway=h->wobble?(h->wobble&4?2:-2):0;
  uint16_t rope=h->state==2?C(0xC0B7A4):C(0xBEB3A0);
  for(int i=0;i<5;++i) {
    int px=x-w+4+i*(2*w-8)/4,bx=x-w/2+i*w/4+sway;
    line(px,y+2,bx,y+21,rope);
    if(i<4) {int nx=x-w+4+(i+1)*(2*w-8)/4;line(px,y+2,(bx+nx)/2,y+12,rope);line(nx,y+2,(bx+nx)/2,y+12,rope);}
  }
  stroke(x-w/2+sway,y+21,x+w/2+sway,y+21,1,rope);
  ellipse(x,y,w+2,6,C(0xA4483D),false);
  ellipse(x,y,w,4,PAPER,false);
  for(int row=y-4;row<y;++row) {
    float a=(float)(row-y)/4;int ex=(int)(w*sqrtf(1-a*a));
    rect(x-ex,row,3,1,C(0xEF8C6D));rect(x+ex-2,row,3,1,C(0xEF8C6D));
  }
}
static void hoop_front(const fd_hoop *h,float half) {
  int x=(int)h->x,y=(int)h->y,w=(int)half;
  if(y+7<y0||y>=y1)return;
  for(int row=y;row<=y+6;++row) {
    float a=(float)(row-y)/6;int edge=(int)((w+2)*sqrtf(1-a*a));
    if(row<y+4) {
      float t=(float)(row-y)/4;int inner=(int)(w*sqrtf(1-t*t));
      rect(x-edge,row,edge-inner+1,1,RED);rect(x+inner,row,edge-inner+1,1,RED);
    } else rect(x-edge,row,2*edge+1,1,RED);
  }
  if(h->wobble>12)star(x-w-6,y-4,3,C(0xE6B853));
}
static void flight(void) {
  const fd_game *g=world;
  wall((int)g->scroll,true);
  text(12,9,"FLAPPY DUNK",1,INK);text(12,23,"ENDLESS",1,MUTED);
  number(147,10,g->score,g->score<10000?2:1,INK);
  text(252,9,"BEST",1,MUTED);number(252,22,stats->best,1,INK);
  float half=fd_half_width(g);
  unsigned next=FD_HOOPS;float nx=1000;
  for(unsigned i=0;i<FD_HOOPS;++i) {
    const fd_hoop *h=&g->hoop[i];hoop_back(h,half);
    if(h->state!=2&&h->x<nx)nx=h->x,next=i;
  }
  if(next<FD_HOOPS&&g->phase<FD_OVER) {
    const fd_hoop *h=&g->hoop[next];int x=(int)h->x,y=(int)h->y;
    line(x-3,y-13,x,y-10,GREEN);line(x,y-10,x+3,y-13,GREEN);
  }
  int shadow=12+(int)(g->y-FD_TOP)/12;
  ellipse((int)g->x,FD_FLOOR+5,shadow,2,C(0xCDA779),false);
  if(g->streak>=3) {
    for(int i=3;i>0;--i)circle((int)g->x-9-i*4,(int)g->y+i/2,4-i,C(0xEDB55C),false);
  }
  ball((int)g->x,(int)g->y,FD_BALL_R,stats->skin,g->flap_anim,g->message_timer&&g->message==2);
  for(unsigned i=0;i<FD_HOOPS;++i)hoop_front(&g->hoop[i],half);
  for(unsigned i=0;i<FD_PARTICLES;++i) {
    const fd_particle *p=&g->particle[i];if(!p->life)continue;
    if(p->kind==2)star((int)p->x,(int)p->y,p->life>14?2:1,C(0xD8B85F));
    else circle((int)p->x,(int)p->y,p->life>14?2:1,p->kind?GREEN:RED,false);
  }
  if(g->message_timer) {
    card(200,49,108,28);
    text(209,59,g->message==2?"SWISH!":"DUNK!",1,g->message==2?GREEN:RED);
    text(262,59,g->message==2?"X":"+",1,INK);number(274,59,g->award,1,INK);
  }
  text(12,231,"OK FLAP | BACK PAUSE",1,INK);
  if(g->streak>1) {text(213,231,"STREAK",1,GREEN);number(260,231,g->streak,1,GREEN);}
  else text(238,231,"HOME EXIT",1,MUTED);

}
static void title(void) {
  wall(0,false);
  text(17,35,"FLAPPY",3,INK);text(17,62,"DUNK",3,RED);
  ellipse(85,206,55,5,C(0xD9CCB7),false);
  fd_hoop h={86,187,0,0,0,0};hoop_back(&h,42);
  unsigned lift=6+((clock_ms/130)%4)*3;
  ball(84,147,32,stats->skin,lift,false);hoop_front(&h,42);
  card(190,32,114,48);text(202,41,"PERSONAL BEST",1,MUTED);
  number(202,56,stats->best,stats->best<100000?2:1,INK);
  bool resume=world->present&&world->started&&world->phase<FD_OVER;
  button(185,96,121,resume?"CONTINUE":"PLAY",selection==0);
  button(185,132,121,"LOCKER",selection==1);
  button(185,168,121,"HOW TO PLAY",selection==2);
  text(15,230,"BY BOBYDELUXE",1,MUTED);text(194,230,"OK SELECT",1,MUTED);
}
static void locker(void) {
  wall(0,false);text(12,12,"YOUR LOCKER",2,INK);text(12,35,"UNLOCK WITH YOUR BEST SCORE",1,MUTED);
  uint8_t unlocked=fd_unlocked(stats);
  for(unsigned i=0;i<FD_SKINS;++i) {
    int x=12+(int)(i%3)*102,y=57+(int)(i/3)*73;bool active=i==selection,open=unlocked&(1u<<i);
    roundbox(x-1,y-1,96,67,7,active?GREEN:C(0xDDD4C5));roundbox(x,y,94,65,6,open?C(0xFFF9ED):C(0xE7E0D6));
    ball(x+46,y+25,15,i,active?12:0,stats->skin==i);
    centered(x+47,y+49,fd_skin_names[i],1,open?INK:MUTED);
    if(stats->skin==i)star(x+82,y+10,3,GREEN);
    if(!open) {roundbox(x+68,y+7,14,11,2,C(0x9D998D));circle(x+75,y+6,4,C(0x9D998D),true);}
  }
  if(!(unlocked&(1u<<selection))) {
    text(12,213,"BEST",1,MUTED);number(43,213,fd_skin_targets[selection],1,INK);
    text(76,213,"POINTS TO UNLOCK",1,MUTED);
  } else text(12,213,selection==stats->skin?"EQUIPPED":"OK TO EQUIP",1,GREEN);
  text(12,231,"ARROWS CHOOSE",1,MUTED);text(264,231,"BACK",1,MUTED);
}
static void modal(void) {
  for(int y=y0;y<y1;++y)for(int x=x0;x<x1;++x)if((x+y)&1)pixel(x,y,C(0xD5CBBB));
  card(38,31,244,177);
  if(page==FD_READY) {
    centered(160,50,world->started?"BACK IN THE AIR":"READY TO DUNK?",2,INK);
    centered(160,81,"FLAP TO STAY UP.",1,MUTED);
    centered(160,97,"DROP THROUGH EVERY HOOP.",1,MUTED);
    fd_hoop h={160,139,0,0,0,0};hoop_back(&h,24);ball(160,119,12,stats->skin,12,false);hoop_front(&h,24);
    button(68,173,184,"OK / EXE / UP TO FLAP",true);
  } else if(page==FD_GUIDE) {
    centered(160,45,"HOW TO FLY",2,INK);
    text(52,70,"OK / EXE / UP  FLAP ONCE",1,INK);
    text(52,85,"BACK           PAUSE",1,INK);
    text(52,100,"HOME           SAVE AND EXIT",1,INK);
    text(52,124,"GO DOWN THROUGH EVERY HOOP.",1,MUTED);
    text(52,139,"CENTRE SHOTS BUILD A STREAK.",1,MUTED);
    text(52,154,"AVOID THE CEILING AND FLOOR.",1,MUTED);
    button(92,179,136,"OK GOT IT",true);
  } else if(page==FD_CONFIRM) {
    centered(160,50,"START AGAIN?",2,INK);
    centered(160,85,"YOUR CURRENT FLIGHT WILL END.",1,MUTED);
    centered(160,103,"RECORDS AND UNLOCKS WILL STAY.",1,MUTED);
    button(66,133,188,"KEEP PLAYING",selection==0);button(66,168,188,"NEW FLIGHT",selection==1);
  } else if(page==FD_SAVE_ERROR) {
    centered(160,48,"SAVE FAILED",2,RED);
    centered(160,83,"CALCULATOR STORAGE IS FULL",1,INK);
    centered(160,101,"OR THE SAVE CANNOT BE WRITTEN.",1,MUTED);
    centered(160,123,"1 RETURN TO YOUR FLIGHT",1,GREEN);
    button(66,151,188,"OK RETRY SAVE",true);
    centered(160,190,"BACK EXIT WITHOUT SAVING",1,RED);
  } else if(page==FD_RESULT) {
    centered(160,45,"KEEP FLYING!",2,INK);
    static const char *const why[]={"","THE FLOOR GOT YOU.","A LITTLE TOO HIGH.","YOU MISSED A HOOP."};
    centered(160,69,why[world->reason],1,MUTED);
    text(63,89,"SCORE",1,MUTED);number(119,85,world->score,world->score<100000?2:1,INK);
    text(205,89,"SWISH",1,MUTED);number(246,89,world->swishes,1,GREEN);
    text(63,111,world->unlocks?"NEW BALL IN YOUR LOCKER!":"YOUR NEXT SWISH IS WAITING.",1,world->unlocks?GREEN:MUTED);
    button(65,129,190,"PLAY AGAIN",selection==0);
    button(65,158,190,"TITLE SCREEN",selection==1);
  } else {
    centered(160,44,"TAKE A BREATHER",2,INK);
    centered(160,69,failed?"STORAGE FULL - SAVE FAILED":"YOUR FLIGHT IS SAVED.",1,failed?RED:MUTED);
    button(65,86,190,"RESUME",selection==0);button(65,115,190,"NEW FLIGHT",selection==1);
    button(65,144,190,"HOW TO PLAY",selection==2);button(65,173,190,"TITLE SCREEN",selection==3);
  }
}
static unsigned old_page=99,old_selection,old_skin;
static uint32_t old_tick,old_score,old_best,old_clock;
static bool old_failed;
void fd_render(const fd_game *g,const fd_stats *s,unsigned view,unsigned selected,uint32_t ms,bool save_failed,fd_push push) {
  world=g;stats=s;page=view;selection=selected;clock_ms=ms;failed=save_failed;
  bool full=!initialized||old_page!=view||old_selection!=selected||old_skin!=s->skin||old_failed!=failed;
  if(full)mark(0,0,320,240);
  else if(view==FD_FLIGHT&&old_tick!=g->tick)mark(0,FD_TOP,320,240-FD_TOP);
  else if(view==FD_TITLE&&old_clock!=ms/130)mark(12,106,153,110);
  if(s->best!=old_best||g->score!=old_score)mark(0,0,320,FD_TOP);
  old_page=view;old_selection=selected;old_skin=s->skin;old_tick=g->tick;old_score=g->score;
  old_best=s->best;old_clock=ms/130;old_failed=failed;
  for(int row=0;row<30;++row) {
    if(dirty_hi[row]<=dirty_lo[row])continue;
    x0=dirty_lo[row];x1=dirty_hi[row];y0=row*8;y1=y0+8;rect(x0,y0,x1-x0,8,PAPER);
    if(view==FD_TITLE)title();
    else if(view==FD_LOCKER)locker();
    else {flight();if(view!=FD_FLIGHT)modal();}
    push(x0,y0,x1-x0,8,band);dirty_lo[row]=320;dirty_hi[row]=0;
  }
  initialized=true;
}
