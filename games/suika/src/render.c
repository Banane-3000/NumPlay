/* SPDX-License-Identifier: GPL-3.0-only
 * Original procedural artwork. No sprites, framebuffer, or heap allocations.
 */
#include "render.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
#define C(v) ((uint16_t)((((v) >> 8) & 0xF800) | (((v) >> 5) & 0x07E0) | (((v) >> 3) & 0x1F)))
#define PAPER C(0xFCF4DF)
#define INK C(0x354C34)
#define MUTED C(0x8D8867)
#define GREEN C(0x477842)
#define RED C(0xC64F49)
#define BX 88
#define BY 52
static uint16_t band[320 * 8];
static int x0, y0, x1, y1;
static int16_t dirty_lo[30], dirty_hi[30];
static bool initialized;
static const sk_game *world;
static unsigned page, selection;
static bool failed;
static bool basket_clip;

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
void sk_invalidate(void) { mark(0, 0, 320, 240); initialized = false; }
static void pixel(int x, int y, uint16_t c) {
  if (basket_clip && (x < BX || x >= BX + SK_WIDTH || y < BY-32 || y >= BY + SK_HEIGHT)) return;
  if (x >= x0 && x < x1 && y >= y0 && y < y1) band[(y - y0) * (x1 - x0) + x - x0] = c;
}
static void rect(int x, int y, int w, int h, uint16_t c) {
  int l = x < x0 ? x0 : x, t = y < y0 ? y0 : y;
  int r = x + w > x1 ? x1 : x + w, b = y + h > y1 ? y1 : y + h;
  if (basket_clip) {
    if (l < BX) l = BX;
    if (r > BX + SK_WIDTH) r = BX + SK_WIDTH;
    if (t < BY-32) t = BY-32;
    if (b > BY + SK_HEIGHT) b = BY + SK_HEIGHT;
  }
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
/* Relative scale and expressions adapted from the publisher's classic skin. */
static const uint16_t colors[]={0,C(0xF21F28),C(0xFF655F),C(0xA34CF5),C(0xFFB517),C(0xFF8E24),
 C(0xF21D2D),C(0xFFF28B),C(0xFFB6BF),C(0xFFE84F),C(0xD7ED76),C(0x43B60C)};
static const uint16_t rims[]={0,C(0xB91B24),C(0xD94047),C(0x792FD4),C(0xE59A08),C(0xE16D0D),
 C(0xB61020),C(0xD5AD2E),C(0xDD6B85),C(0xCFAB20),C(0x96B538),C(0x146509)};
static void leaf(int x,int y,int size) {
  line(x-size,y,x+size,y,GREEN);line(x,y-size,x,y+size,GREEN);
  line(x-size,y-size/2,x+size,y+size/2,GREEN);
}
static void fruit(int k,int x,int y,int r,bool happy) {
  if(y+r+9<y0||y-r-9>=y1||x+r+3<x0||x-r-3>=x1)return;
  if(k==3) {
    int a=r/2;if(a<2)a=2;
    static const int8_t xx[]={-1,0,1,-1,1,0},yy[]={0,-1,0,1,1,2};
    for(unsigned i=0;i<6;++i)circle(x+xx[i]*a,y+(yy[i]-1)*a*2/3,a+1,rims[k],false);
    for(unsigned i=0;i<6;++i)circle(x+xx[i]*a,y+(yy[i]-1)*a*2/3,a,colors[k],false);
    leaf(x-r+2,y-r+3,2);
  } else if(k==2) {
    /* A round shoulder tapers to the strawberry's tip. */
    int top=y-r>y0?y-r:y0,bottom=y+r<y1?y+r:y1-1;
    for(int j=top;j<=bottom;++j) {
      int dy=j-y,half=(int)sqrtf((float)(r*r-dy*dy));
      if(dy>0)half=half*(2*r-dy)/(2*r);
      rect(x-half,j,2*half+1,1,rims[k]);
      if(half>0)rect(x-half+1,j,2*half-1,1,colors[k]);
    }
  } else {
    circle(x,y,r,rims[k],false);circle(x,y,r-1,colors[k],false);
  }
  if(k==4) {circle(x,y-r+1,r/3+1,rims[k],false);circle(x,y-r+1,r/3,colors[k],false);}
  if(k==2||k==5)leaf(x,y-r+2,r/3+1);
  if(k==1||k==6||k==7) {
    line(x-1,y-r+2,x+1,y-r-3,C(0x8A5C24));
    if(k==1)leaf(x+3,y-r-3,2);
  }
  if(k==9) {
    for(int i=-2;i<=2;++i)line(x,y-r+2,x+i*3,y-r-5-((i&1)?0:3),GREEN);
  }
  if(k==8&&r>7)line(x-r/5,y-r+3,x-r/3,y-r/2,rims[k]);
  int top=y-r+2>y0?y-r+2:y0,bottom=y+r-2<y1?y+r-2:y1-1;
  if(k>=9)for(int j=top;j<=bottom;++j) {
    int dy=j-y,half=(int)sqrtf((float)((r-1)*(r-1)-dy*dy));
    for(int i=-half+1;i<half;++i) {
      if(k==11) {
        if((i+dy/3+r*3)%18<6)pixel(x+i,j,C(0x217A08));
      } else if((i+dy+r*4)%9==0||(i-dy+r*4)%9==0)
        pixel(x+i,j,k==10?C(0xECF5AC):C(0xE1BC29));
    }
  }
  if(k==2)for(int j=-r/2;j<r/2;j+=4)for(int i=-r/2;i<=r/2;i+=4)
    pixel(x+i+(j&1),y+j,C(0xFFE7A8));
  if(r>7) {
    circle(x-r/3,y-r/2,r/7+1,k==11?C(0xFFFFFF):C(0xFFF7D6),false);
    if(k>=6)circle(x-r/2,y-r/6,1,C(0xFFF7D6),false);
  }
  if(r<4)return;
  int ex=r>14?r/4:2,ey=y+r/10,eye=r>=17?2:1;
  uint16_t face=k==11?C(0x124902):k==6?C(0x8D1019):k==8?C(0xAA243E):C(0x8E6727);
  if(k==11||happy) {
    line(x-ex-eye,ey-eye,x-ex+eye,ey,face);line(x-ex+eye,ey,x-ex-eye,ey+eye,face);
    circle(x+ex,ey,eye,face,false);
  } else {
    circle(x-ex,ey,eye,face,false);circle(x+ex,ey,eye,face,false);
    if(k==3||k==4) {pixel(x-ex,ey,C(0xFFFFFF));pixel(x+ex,ey,C(0xFFFFFF));}
  }
  int mouth=ey+(r>12?5:3);
  if(k==6) {line(x-1,mouth-1,x+2,mouth-1,face);line(x+2,mouth-1,x,mouth+2,face);}
  else if(k==8) {
    line(x-3,mouth-1,x-2,mouth+2,face);line(x-2,mouth+2,x,mouth,face);
    line(x,mouth,x+2,mouth+2,face);line(x+2,mouth+2,x+3,mouth-1,face);
  } else if(k==3)circle(x,mouth,1,face,true);
  else {line(x-2,mouth,x,mouth+1,face);line(x,mouth+1,x+2,mouth-1,face);}
  if(r>=10&&k!=11) {pixel(x-ex-3,ey+3,C(0xEE8275));pixel(x+ex+3,ey+3,C(0xEE8275));}
}
static void button(int x, int y, int w, const char *label, bool active) {
  roundbox(x,y+2,w,23,6,C(0xDDD2AB));
  roundbox(x,y,w,23,6,active?GREEN:C(0xF4E9CE));
  centered(x+w/2,y+8,label,1,active?C(0xFFF9E9):INK);
}
static void card(int x, int y, int w, int h) {
  roundbox(x,y+3,w,h,7,C(0xE5D7B5)); roundbox(x,y,w,h,7,C(0xFFFBEF));
}
static void backdrop(void) {
  rect(0,0,320,110,C(0xCF9859));rect(0,110,320,23,C(0xB9812D));
  rect(0,133,320,7,C(0xE6CA81));rect(0,140,320,100,C(0xF4DF92));
  for(int y=140;y<240;y+=25)for(int x=0;x<320;x+=40)
    if(((x/40+y/25)&1)==0)rect(x,y,40,25,C(0xFFF0BB));
}
static void bubble(int x,int y,int r) {
  circle(x,y,r,C(0xF5D999),false);circle(x,y,r-2,C(0xE9BD7F),false);
  circle(x,y,r-5,C(0xDDA86D),false);
  circle(x-r/2,y-r/2,r/8+1,C(0xFFF4CE),false);
  circle(x-r/2-3,y-r/2+8,1,C(0xFFF9DC),false);
  line(x+r/3,y+r-6,x+r/2,y+r-9,C(0xFBE8B1));
}
static void cloud(int x,int y) {
  const uint16_t rim=C(0xEAC751),fill=C(0xFFF19B),face=C(0x8C6950);
  circle(x-10,y+3,9,rim,false);circle(x,y-3,12,rim,false);circle(x+11,y+3,9,rim,false);
  rect(x-10,y+3,23,10,rim);
  circle(x-10,y+3,7,fill,false);circle(x,y-3,10,fill,false);circle(x+11,y+3,7,fill,false);
  rect(x-10,y+2,23,9,fill);
  circle(x-5,y+1,1,face,false);circle(x+5,y+1,1,face,false);
  line(x-1,y+4,x,y+5,face);line(x,y+5,x+2,y+4,face);
  rect(x-11,y+5,3,2,C(0xF0B5A0));rect(x+9,y+5,3,2,C(0xF0B5A0));
}
static void board(void) {
  const sk_game *g=world;
  rect(BX,BY,SK_WIDTH,SK_HEIGHT,C(0xFFF0C0));
  rect(BX+2,BY,8,SK_HEIGHT,C(0xFFF5D3));rect(BX+SK_WIDTH-10,BY,8,SK_HEIGHT,C(0xEDDCAC));
  rect(BX+8,BY-9,SK_WIDTH-16,9,C(0xE9D79A));
  line(BX-2,BY,BX+7,BY-10,C(0xFFE7A1));line(BX+7,BY-10,BX+SK_WIDTH-7,BY-10,C(0xFFE7A1));
  line(BX+SK_WIDTH-7,BY-10,BX+SK_WIDTH+2,BY,C(0xFFE7A1));
  basket_clip=true;
  for(unsigned i=0;i<SK_CAPACITY;++i) {
    const sk_fruit *b=&g->fruit[i];if(!b->kind)continue;
    fruit(b->kind,BX+(int)b->x,BY+(int)b->y,sk_radius[b->kind],b->pop>0);
  }
  basket_clip=false;
  rect(BX-3,BY,3,SK_HEIGHT+3,C(0xFFE39A));rect(BX+SK_WIDTH,BY,3,SK_HEIGHT+3,C(0xEBCD7B));
  rect(BX-3,BY+SK_HEIGHT,SK_WIDTH+6,4,C(0xFFE39A));rect(BX-3,BY,SK_WIDTH+6,2,C(0xFFE39A));
  if(g->danger)for(int x=BX+2;x<BX+SK_WIDTH;x+=7)rect(x,BY+SK_DANGER,4,1,RED);
  if(!g->over) {
    int ax=BX+(int)g->aim;cloud(ax,18);
    if(!g->cooldown)fruit(g->current,ax,BY+SK_DROP_Y,sk_radius[g->current],false);
  }
}
static void center_number(int x,int y,uint32_t value,int scale,uint16_t ink) {
  unsigned digits=1;for(uint32_t n=value;n>=10;n/=10)++digits;
  number(x-((int)digits*6-1)*scale/2,y,value,scale,ink);
}
static void evolution(void) {
  static const int8_t xx[]={12,26,32,27,14,-4,-20,-30,-30,-21,-4};
  static const int8_t yy[]={-30,-18,0,17,29,32,25,10,-9,-25,-32};
  circle(278,170,37,C(0xF6ECA6),false);circle(278,170,26,C(0xEBDCA0),false);
  centered(278,109,"CIRCLE OF",1,INK);centered(278,120,"EVOLUTION",1,INK);
  for(unsigned i=0;i<SK_RANKS;++i)fruit((int)i+1,278+xx[i],170+yy[i],2+(int)i/2,false);
  line(290,133,294,135,GREEN);line(294,135,290,137,GREEN);
}
static void gameplay(void) {
  const sk_game *g=world;backdrop();
  bubble(38,59,35);centered(38,24,"SCORE",1,INK);
  center_number(38,43,g->score,g->score<10000?2:1,C(0xFFF7CC));
  centered(38,66,"BEST",1,INK);center_number(38,78,g->best,1,C(0xFFF7CC));
  bubble(279,58,30);centered(279,26,"NEXT",1,INK);
  fruit(g->next,279,63,sk_radius[g->next],false);
  evolution();board();
}
static void title(void) {
  text(22,18,"NUMPLAY / SUIKA",1,MUTED);
  text(21,42,"SUIKA",4,INK);
  text(23,82,"A LITTLE FRUIT. A BIG MERGE.",1,GREEN);
  fruit(11,272,52,28,true);
  roundbox(16,112,143,105,10,C(0xEAD5AD));
  roundbox(20,113,135,98,8,C(0xFFFBEF));
  fruit(11,67,169,34,false); fruit(8,119,187,20,false);
  fruit(5,111,151,14,false); fruit(3,124,125,10,false); fruit(1,88,128,6,false);
  text(177,111,"BEST SCORE",1,MUTED); number(177,124,world->best,2,INK);
  button(176,151,128,world->started&&!world->over?"CONTINUE":"PLAY",selection==0);
  button(176,181,128,"HOW TO PLAY",selection==1);
  text(178,218,"OK SELECT",1,MUTED);
  text(22,230,"BY BOBYDELUXE",1,GREEN);
}
static void modal(void) {
  /* Deterministic stipple dims without a second framebuffer. */
  for(int y=y0;y<y1;++y)for(int x=x0;x<x1;++x)if((x+y)&1)pixel(x,y,C(0xD5C9A6));
  card(40,35,240,174);
  if(page==SK_HELP) {
    centered(160,48,"HOW TO PLAY",2,INK);
    text(54,72,"MATCH TWO FRUITS TO GROW.",1,INK);
    text(54,86,"KEEP FRUIT INSIDE THE BOX.",1,INK);
    text(54,107,"< >   MOVE THE DROPPER",1,MUTED);
    text(54,121,"OK    DROP ONE FRUIT",1,MUTED);
    text(54,135,"ONLY THE NEXT FRUIT IS SHOWN.",1,MUTED);
    text(54,149,"FIVE SMALL FRUITS CAN DROP.",1,MUTED);
    text(54,163,"BACK  PAUSE / RESUME",1,MUTED);
    button(91,181,138,"OK GOT IT",true);
  } else if(page==SK_CONFIRM) {
    centered(160,50,"START AGAIN?",2,INK);
    centered(160,86,"YOUR CURRENT RUN WILL BE LOST.",1,MUTED);
    centered(160,102,"YOUR BEST SCORE WILL STAY.",1,MUTED);
    button(66,133,188,"KEEP PLAYING",selection==0);
    button(66,166,188,"NEW GAME",selection==1);
  } else if(page==SK_RESULT) {
    centered(160,49,"BASKET FULL",2,INK);
    centered(160,78,world->score && world->score==world->best?"A NEW PERSONAL BEST!":"A FRESH BASKET AWAITS.",1,GREEN);
    text(67,103,"SCORE",1,MUTED); number(149,99,world->score,2,INK);
    text(67,122,"MERGES",1,MUTED); number(149,122,world->merges,1,INK);
    button(66,145,188,"PLAY AGAIN",selection==0);
    button(66,175,188,"TITLE SCREEN",selection==1);
  } else if(page==SK_SAVE_ERROR) {
    centered(160,52,"SAVE FAILED",2,RED);
    centered(160,85,"CALCULATOR STORAGE IS FULL",1,INK);
    centered(160,102,"OR THE SAVE CANNOT BE WRITTEN.",1,MUTED);
    centered(160,124,"YOUR RUN IS STILL IN MEMORY.",1,MUTED);
    centered(160,138,"1 RETURN TO THE GAME",1,GREEN);
    button(66,158,188,"OK RETRY SAVE",true);
    centered(160,193,"BACK EXIT WITHOUT SAVING",1,RED);
  } else {
    centered(160,49,"TAKE A BREATHER",2,INK);
    centered(160,77,failed?"STORAGE FULL - SAVE FAILED":"YOUR BASKET IS SAVED.",1,failed?RED:MUTED);
    button(66,90,188,"RESUME",selection==0);
    button(66,119,188,"NEW GAME",selection==1);
    button(66,148,188,"HOW TO PLAY",selection==2);
    button(66,177,188,"TITLE SCREEN",selection==3);
  }
}
typedef struct {int16_t x,y;uint8_t kind,pop;} projection;
static projection previous[SK_CAPACITY];
static int old_aim,old_current,old_danger;
static uint32_t old_score,old_best;
static unsigned old_page=99,old_selection;
static uint8_t old_next,old_ready,old_over;
static bool old_failed;
void sk_render(const sk_game *g,unsigned view,unsigned selected,bool save_failed,sk_push push) {
  world=g;page=view;selection=selected;failed=save_failed;
  bool full=!initialized||page!=old_page||selection!=old_selection||failed!=old_failed;
  if(full) mark(0,0,320,240);
  int aim=(int)g->aim;
  for(unsigned i=0;i<SK_CAPACITY;++i) {
    const sk_fruit *b=&g->fruit[i]; projection p={(int16_t)b->x,(int16_t)b->y,b->kind,b->pop};
    if(memcmp(&p,&previous[i],sizeof p)) {
      projection q=previous[i];
      if(q.kind)mark(BX+q.x-sk_radius[q.kind]-9,BY+q.y-sk_radius[q.kind]-12,2*sk_radius[q.kind]+22,2*sk_radius[q.kind]+24);
      if(p.kind)mark(BX+p.x-sk_radius[p.kind]-9,BY+p.y-sk_radius[p.kind]-12,2*sk_radius[p.kind]+22,2*sk_radius[p.kind]+24);
      previous[i]=p;
    }
  }
  if(aim!=old_aim||g->current!=old_current||(g->cooldown==0)!=old_ready||g->over!=old_over) {
    mark(BX+old_aim-25,0,51,BY+26);mark(BX+aim-25,0,51,BY+26);
  }
  if((g->danger>0)!=(old_danger>0))mark(BX-3,BY,SK_WIDTH+6,8);
  if(g->score!=old_score||g->best!=old_best)mark(0,18,78,80);
  if(g->next!=old_next)mark(246,24,72,68);
  old_aim=aim;old_current=g->current;old_danger=g->danger;
  old_score=g->score;old_best=g->best;old_next=g->next;
  old_page=page;old_selection=selection;old_failed=failed;
  old_ready=g->cooldown==0;old_over=g->over;
  for(int row=0;row<30;++row) {
    if(dirty_hi[row]<=dirty_lo[row])continue;
    x0=dirty_lo[row];x1=dirty_hi[row];y0=row*8;y1=y0+8;
    rect(x0,y0,x1-x0,8,PAPER);
    if(page==SK_TITLE)title();
    else { gameplay();if(page!=SK_PLAY)modal(); }
    push(x0,y0,x1-x0,8,band);
    dirty_lo[row]=320;dirty_hi[row]=0;
  }
  initialized=true;
}
