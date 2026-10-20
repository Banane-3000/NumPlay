/* SPDX-License-Identifier: GPL-3.0-only
 * Original procedural artwork. No sprites, framebuffer, or heap allocations.
 */
#include "render.h"
#include <stdlib.h>
#include <string.h>
#define C(v) ((uint16_t)((((v) >> 8) & 0xF800) | (((v) >> 5) & 0x07E0) | (((v) >> 3) & 0x1F)))
#define PAPER C(0x081927)
#define INK C(0xEDF4EF)
#define MUTED C(0x839FAE)
#define GREEN C(0x74E8C3)
#define RED C(0xFFA58F)
static uint16_t band[320 * 8];
static int x0, y0, x1, y1;
static int16_t dirty_lo[30], dirty_hi[30];
static bool initialized;
static const tb_game *world;
static const tb_stats *stats;
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
void tb_invalidate(void) { mark(0, 0, 320, 240); initialized = false; }
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
static void line(int a, int b, int c, int d, uint16_t ink) {
  if((a<x0&&c<x0)||(a>=x1&&c>=x1)||(b<y0&&d<y0)||(b>=y1&&d>=y1))return;
  int dx = abs(c - a), sx = a < c ? 1 : -1, dy = -abs(d - b), sy = b < d ? 1 : -1, err = dx + dy;
  for (;;) {
    pixel(a, b, ink);
    if (a == c && b == d) break;
    int e = 2 * err;
    if (e >= dy) err += dy, a += sx;
    if (e <= dx) err += dx, b += sy;
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

static uint16_t blend(uint16_t a,uint16_t b,unsigned t) {
  uint32_t x=(a|(uint32_t)a<<16)&0x07E0F81F,z=(b|(uint32_t)b<<16)&0x07E0F81F;
  x=((x*(32-t)+z*t)>>5)&0x07E0F81F;return (uint16_t)(x|x>>16);
}
static uint16_t accent(void) {
  static const uint16_t colors[]={GREEN,C(0x68DED6),C(0xFFC487),C(0xC5DDE5)};
  return colors[stats->palette];
}
static uint16_t material(unsigned tone) {
  static const uint16_t colors[TB_PALETTES][4]={
    {C(0x69DEBF),C(0x7BA6ED),C(0xC29CEB),C(0xF0ACAD)},
    {C(0x42CFCE),C(0x81DF9B),C(0x608DE9),C(0xB1ACF2)},
    {C(0xF2CB83),C(0xED9B79),C(0xCA799C),C(0xDEAA86)},
    {C(0xBBD5DE),C(0x83AAB7),C(0xD4E5DF),C(0x7096A9)}
  };
  tone%=96;return blend(colors[stats->palette][tone/24],colors[stats->palette][(tone/24+1)%4],tone%24*32/24);
}
static void background(void) {
  for(int y=y0;y<y1;++y)rect(0,y,320,1,blend(C(0x071524),C(0x204757),(unsigned)(y*21/240)));
}
typedef struct {int x,y;} point;
static void polygon(const point p[4],uint16_t color) {
  int top=p[0].y,bottom=top,left=p[0].x,right=left;
  for(unsigned i=1;i<4;++i) {
    if(p[i].y<top)top=p[i].y;
    if(p[i].y>bottom)bottom=p[i].y;
    if(p[i].x<left)left=p[i].x;
    if(p[i].x>right)right=p[i].x;
  }
  if(top>=y1||bottom<y0||left>=x1||right<x0)return;
  if(top<y0)top=y0;
  if(bottom>=y1)bottom=y1-1;
  for(int y=top;y<=bottom;++y) {
    int lo=10000,hi=-10000;
    for(unsigned i=0;i<4;++i) {
      point a=p[i],b=p[(i+1)&3];
      if(a.y>b.y) {point t=a;a=b;b=t;}
      if(y<a.y||y>b.y)continue;
      if(a.y==b.y) {
        if(a.x<lo)lo=a.x;
        if(b.x<lo)lo=b.x;
        if(a.x>hi)hi=a.x;
        if(b.x>hi)hi=b.x;
      } else {
        int x=a.x+(b.x-a.x)*(y-a.y)/(b.y-a.y);
        if(x<lo)lo=x;
        if(x>hi)hi=x;
      }
    }
    if(hi>=lo)rect(lo,y,hi-lo+1,1,color);
  }
}
static int scene_x,scene_y,zoom;
static int32_t camera_x,camera_z;
static point project(int32_t x,int32_t z,int offset) {
  return (point){scene_x+(int)((x-z-camera_x+camera_z)*zoom/(TB_Q*256)),
                scene_y+(int)((x+z-camera_x-camera_z)*zoom/(TB_Q*512))+offset};
}
static void slab(int32_t x,int32_t z,int w,int d,int offset,unsigned tone,unsigned fade,bool wire) {
  point p[4]={project(x,z,offset),project(x+w,z,offset),project(x+w,z+d,offset),project(x,z+d,offset)};
  int min_y=p[0].y,max_y=p[0].y,min_x=p[0].x,max_x=p[0].x;
  for(unsigned i=1;i<4;++i) {
    if(p[i].y<min_y)min_y=p[i].y;
    if(p[i].y>max_y)max_y=p[i].y;
    if(p[i].x<min_x)min_x=p[i].x;
    if(p[i].x>max_x)max_x=p[i].x;
  }
  if(min_y>=y1||max_y+8*zoom/256<y0||min_x>=x1||max_x<x0)return;
  uint16_t top=blend(material(tone),C(0x183441),fade);
  if(!wire) {
    int h=8*zoom/256;if(h<2)h=2;
    point left[4]={p[3],p[2],{p[2].x,p[2].y+h},{p[3].x,p[3].y+h}};
    point right[4]={p[1],p[2],{p[2].x,p[2].y+h},{p[1].x,p[1].y+h}};
    polygon(left,blend(top,C(0x021322),12));polygon(right,blend(top,C(0x021322),22));polygon(p,top);
    uint16_t seam=blend(top,PAPER,25);
    line(p[3].x,p[3].y+h,p[2].x,p[2].y+h,seam);
    line(p[1].x,p[1].y+h,p[2].x,p[2].y+h,seam);
    line(p[0].x,p[0].y,p[1].x,p[1].y,blend(top,INK,9));
    line(p[0].x,p[0].y,p[3].x,p[3].y,blend(top,INK,6));
  } else for(unsigned i=0;i<4;++i)line(p[i].x,p[i].y,p[(i+1)&3].x,p[(i+1)&3].y,blend(accent(),PAPER,15));
}
static void scene(void) {
  const tb_game *g=world;
  bool side=page==TB_PAUSE||page==TB_RESULT||page==TB_CONFIRM||page==TB_SAVE_ERROR;
  scene_x=side?232:160;scene_y=188-(int)(g->height<8?g->height:8)*8;zoom=side?160:256;
  camera_x=g->cam_x;camera_z=g->cam_z;
  for(unsigned i=0;i<g->count;++i) {
    const tb_layer *p=&g->layer[i];unsigned depth=g->count-1-i;
    int offset=(int)depth*8+g->camera/TB_Q;
    if(i+1u==(unsigned)g->count&&g->phase==TB_SETTLING&&g->result!=TB_MISS)offset+=g->drop/TB_Q;
    slab(p->x,p->z,p->w,p->d,offset,p->tone,depth>6?(depth-6>12?12:depth-6):0,false);
  }
  if(g->present&&g->phase==TB_MOVING) {
    const tb_layer *p=&g->layer[g->count-1];
    int32_t x=p->x+(g->axis?0:g->offset),z=p->z+(g->axis?g->offset:0);
    if(stats->guide) {
      slab(p->x,p->z,p->w,p->d,-8,p->tone,0,true);
      point target=project(p->x+p->w/2,p->z+p->d/2,-8);
      point moving=project(x+p->w/2,z+p->d/2,-12);
      for(int i=0;i<12;++i)if(i&1)pixel(target.x+(moving.x-target.x)*i/12,target.y+(moving.y-target.y)*i/12,accent());
    }
    slab(x,z,p->w,p->d,-12,(g->height%96+1)%96,0,false);
  }
  if(stats->motion)for(unsigned i=0;i<TB_SHARDS;++i) {
    const tb_shard *p=&g->shard[i];if(!p->life)continue;
    int relative=g->height>=p->level?(int)(g->height-p->level):-1;
    slab(p->x,p->z,p->w,p->d,relative*8+p->fall/TB_Q+g->camera/TB_Q,p->tone,52-p->life>32?12:0,false);
  }
}
static void demo(bool title_view) {
  scene_x=title_view?244:250;scene_y=185;zoom=title_view?152:120;camera_x=camera_z=0;
  for(int i=0;i<11;++i) {
    int n=i*TB_Q;
    slab(-TB_SIZE/2+n/3,-TB_SIZE/2+n/5,TB_SIZE-n,TB_SIZE-n/2,-i*6,(unsigned)i*5,0,false);
  }
  int n=stats->motion?(int)((clock_ms/32)%96):24;if(n>48)n=96-n;
  slab(-TB_SIZE/2+(n-24)*TB_Q,-TB_SIZE/2,TB_SIZE-10*TB_Q,TB_SIZE-5*TB_Q,-72,57,0,false);
}
static unsigned digit_scale(uint32_t n) {return n<10000?3:n<1000000?2:1;}
static void title(void) {
  background();demo(true);
  text(17,29,"TOWER",4,INK);text(17,63,"BLOCK",4,accent());
  text(18,112,"BEST",1,MUTED);number(18,128,stats->best,stats->best<1000000?2:1,INK);
  text(18,188,"BY BOBYDELUXE",1,MUTED);
  rect(0,209,320,31,PAPER);
  static const char *const names[]={"BUILD","STUDIO","GUIDE"};
  for(unsigned i=0;i<3;++i) {
    int x=20+(int)i*105;
    const char *name=i==0&&world->present&&world->phase<TB_OVER&&world->started?"RESUME":names[i];
    centered(x+35,220,name,1,i==selection?INK:MUTED);
    if(i==selection)rect(x+5,234,60,2,accent());
  }
}
static void play(void) {
  const tb_game *g=world;background();scene();
  rect(0,0,320,52,PAPER);
  text(13,9,"HEIGHT",1,MUTED);number(13,23,g->height,digit_scale(g->height),INK);
  text(259,10,"BEST",1,MUTED);number(259,26,stats->best,1,accent());
  if(g->phase==TB_SETTLING&&g->message&&g->result>=TB_PERFECT&&g->result!=TB_MISS) {
    if(g->result==TB_GROW)centered(160,64,"WIDTH RESTORED",1,accent());
    else if(g->streak>1) {
      text(128,64,"PERFECT",1,accent());text(178,64,"X",1,MUTED);number(190,64,g->streak,1,accent());
    } else centered(160,64,"PERFECT",1,accent());
  }
  rect(0,219,320,21,PAPER);
  text(13,230,"OK DROP",1,MUTED);text(247,230,"BACK PAUSE",1,MUTED);
}
static const char *const palettes[]={"PRISM","AURORA","EMBER","MONO"};
static void studio(void) {
  background();demo(false);
  text(14,14,"STUDIO",3,INK);
  static const char *const labels[]={"PALETTE","ALIGNMENT GUIDE","ANIMATIONS"};
  for(unsigned i=0;i<3;++i) {
    int y=74+(int)i*39;
    text(15,y,labels[i],1,i==selection?accent():MUTED);
    if(i==selection)rect(9,y,2,25,accent());
    const char *value=i==0?palettes[stats->palette]:i==1?(stats->guide?"ON":"OFF"):(stats->motion?"FULL":"REDUCED");
    text(15,y+14,"<",1,MUTED);text(31,y+14,value,1,INK);text(119,y+14,">",1,MUTED);
  }
  text(15,230,"ARROWS CHANGE",1,MUTED);text(253,230,"BACK",1,MUTED);
}
static void guide(void) {
  background();text(15,15,"HOW TO PLAY",2,INK);
  static const char *const steps[]={"DROP","STACK","PERFECT","GAME OVER"};
  static const char *const body[]={"OK / EXE PLACES THE BLOCK.","OVERHANGS ARE CUT AWAY.","THREE IN A ROW RESTORE WIDTH.","A MISSED BLOCK ENDS THE RUN."};
  for(unsigned i=0;i<4;++i) {
    int y=51+(int)i*35;text(15,y,steps[i],1,accent());text(15,y+12,body[i],1,MUTED);
  }
  text(15,205,"BACK PAUSE  |  HOME SAVE AND EXIT",1,MUTED);
  text(15,230,"OK / BACK RETURN",1,INK);
}
static void choice(int x,int y,const char *name,unsigned index) {
  if(selection==index) {rect(x-7,y-5,123,23,accent());text(x,y+3,name,1,PAPER);}
  else text(x,y+3,name,1,MUTED);
}
static void overlay(void) {
  if(page==TB_READY) {
    rect(0,191,320,49,PAPER);
    text(14,200,world->started?"RESUME":"READY",2,INK);
    text(15,226,world->started?"OK RESUME":"OK START",1,accent());text(264,226,"BACK",1,MUTED);
    return;
  }
  if(page==TB_GUIDE) {guide();return;}
  rect(0,0,147,240,PAPER);rect(146,12,1,216,accent());
  if(page==TB_PAUSE) {
    text(14,22,"PAUSED",2,INK);text(15,48,failed?"SAVE FAILED":"TOWER BLOCK",1,failed?RED:MUTED);
    choice(18,82,"RESUME",0);choice(18,111,"NEW BUILD",1);choice(18,140,"GUIDE",2);choice(18,169,"TITLE",3);
    text(15,228,"OK SELECT",1,MUTED);
  } else if(page==TB_RESULT) {
    text(14,21,"GAME OVER",2,RED);
    text(15,71,"FINAL HEIGHT",1,MUTED);number(15,87,world->height,digit_scale(world->height),INK);
    text(15,120,"PERFECT",1,MUTED);number(82,120,world->perfect,1,accent());
    if(world->height&&world->height==stats->best)text(15,137,"PERSONAL BEST",1,accent());
    choice(18,169,"BUILD AGAIN",0);choice(18,199,"TITLE",1);
    text(15,229,"OK SELECT",1,MUTED);
  } else if(page==TB_CONFIRM) {
    text(14,21,"NEW",3,INK);text(14,49,"BUILD?",3,accent());
    text(15,92,"THIS TOWER WILL",1,MUTED);text(15,106,"BE LEFT BEHIND.",1,MUTED);
    text(15,129,"RECORDS STAY.",1,MUTED);
    choice(18,166,"KEEP TOWER",0);choice(18,197,"START NEW",1);
    text(15,229,"OK SELECT",1,MUTED);
  } else if(page==TB_SAVE_ERROR) {
    text(14,23,"SAVE",3,RED);text(14,52,"FAILED",3,RED);
    text(15,92,"STORAGE IS FULL",1,MUTED);text(15,106,"OR UNAVAILABLE.",1,MUTED);
    text(15,138,"OK RETRY SAVE",1,accent());text(15,163,"1 KEEP BUILDING",1,INK);
    text(15,196,"BACK EXIT",1,RED);text(15,210,"WITHOUT SAVING",1,RED);
  }
}
static unsigned old_page=99,old_selection,old_palette,old_guide,old_motion;
static uint32_t old_tick,old_clock,old_height;
static bool old_failed;
void tb_render(const tb_game *g,const tb_stats *s,unsigned view,unsigned selected,uint32_t ms,bool save_failed,tb_push push) {
  world=g;stats=s;page=view;selection=selected;clock_ms=ms;failed=save_failed;
  bool full=!initialized||old_page!=view||old_selection!=selected||old_palette!=s->palette||old_guide!=s->guide||
            old_motion!=s->motion||old_failed!=failed;
  if(full)mark(0,0,320,240);
  else if(view==TB_PLAY&&old_tick!=g->tick)mark(0,52,320,167);
  else if(s->motion&&(view==TB_TITLE||view==TB_STUDIO)&&old_clock!=ms/64)mark(165,70,155,133);
  if(g->height!=old_height)mark(0,0,320,52);
  old_page=view;old_selection=selected;old_palette=s->palette;old_guide=s->guide;old_motion=s->motion;
  old_tick=g->tick;old_clock=ms/64;old_height=g->height;old_failed=failed;
  for(int row=0;row<30;++row) {
    if(dirty_hi[row]<=dirty_lo[row])continue;
    x0=dirty_lo[row];x1=dirty_hi[row];y0=row*8;y1=y0+8;rect(x0,y0,x1-x0,8,PAPER);
    if(view==TB_TITLE)title();
    else if(view==TB_STUDIO)studio();
    else if(view==TB_GUIDE)guide();
    else if(view==TB_PLAY||view==TB_READY) {play();if(view==TB_READY)overlay();}
    else {background();scene();overlay();}
    push(x0,y0,x1-x0,8,band);dirty_lo[row]=320;dirty_hi[row]=0;
  }
  initialized=true;
}
