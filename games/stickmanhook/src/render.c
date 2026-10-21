/* Original sprites and gradients, directly from flash. One 5 KiB LCD strip. */
#include "render.h"
#include "assets.h"
#include "font.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
#define C(v) ((uint16_t)((((v)>>8)&0xF800)|(((v)>>5)&0x07E0)|(((v)>>3)&31)))
#define WHITE C(0xffffff)
#define NAVY C(0x13234b)
#define BLUE C(0x0171a3)
#define LIME C(0x75edaf)
#define GOLD C(0xffdf69)
typedef struct {int16_t x,y,c,s,l,r,t,b;uint16_t sprite;uint8_t shadow;} command;
static command commands[192];
static unsigned command_count;
static uint16_t band[320*8];
static uint32_t hashes[30];
static bool valid;
static int top;
static unsigned mode,selection;
static const sh_game *g;
static const sh_stats *records;
static bool continuing,failed;
static uint32_t clock_ms;
static int rope_x,rope_y,man_x,man_y;
static bool rope;
void sh_invalidate(void){valid=false;}
static uint16_t half(uint16_t c){return (c>>1)&0x7bef;}
static uint16_t blend(uint16_t a,uint16_t b,unsigned opacity){
  unsigned r=((a>>11)*(3-opacity)+(b>>11)*opacity)/3;
  unsigned green=(((a>>5)&63)*(3-opacity)+((b>>5)&63)*opacity)/3;
  unsigned blue=((a&31)*(3-opacity)+(b&31)*opacity)/3;
  return (uint16_t)(r<<11|green<<5|blue);
}
static void pixel(int x,int y,uint16_t color){if((unsigned)x<320&&y>=top&&y<top+8)band[(y-top)*320+x]=color;}
static void rect(int x,int y,int w,int h,uint16_t c) {
  int l=x<0?0:x,r=x+w>320?320:x+w,a=y<top?top:y,b=y+h>top+8?top+8:y+h;
  if(l>=r||a>=b)return;
  for(int j=a;j<b;++j)for(int i=l;i<r;++i)band[(j-top)*320+i]=c;
}
static void line(int x,int y,int a,int b,uint16_t c) {
  if((y<top&&b<top)||(y>=top+8&&b>=top+8))return;
  // Game coordinates are clipped before they can produce unbounded raster work.
  if(abs(a-x)>2400||abs(b-y)>2400)return;
  int dx=abs(a-x),sx=x<a?1:-1,dy=-abs(b-y),sy=y<b?1:-1,e=dx+dy;
  for(;;){pixel(x,y,c);
  if(x==a&&y==b){break;}int v=2*e;
  if(v>=dy)e+=dy,x+=sx;
  if(v<=dx)e+=dx,y+=sy;}
}
static void circle(int x,int y,int radius,uint16_t c) {
  for(int j=top;j<top+8;++j){int dy=j-y;
  if(abs(dy)>radius)continue;
    int dx=(int)sqrtf((float)(radius*radius-dy*dy));rect(x-dx,j,2*dx+1,1,c);}
}
static int width(const char *s,int scale){int w=0;for(;*s;++s){unsigned c=(unsigned char)*s;
if(c>=32&&c<127)w+=(font_widths[c-32]+1)*scale;}return w;}
static void text(int x,int y,const char *s,int scale,uint16_t c) {
  if(y>=top+8||y+16*scale<=top)return;
  for(;*s;++s){unsigned ch=(unsigned char)*s;
  if(ch<32||ch>=127){continue;}ch-=32;
    for(int j=0;j<16;++j)if(y+j*scale<top+8&&y+(j+1)*scale>top)
      for(int i=0;i<16;++i){unsigned a=(font_rows[font_map[ch]][j]>>(i*2))&3;
        if(a==3)rect(x+i*scale,y+j*scale,scale,scale,c);
        else if(a)for(int dy=0;dy<scale;++dy)for(int dx=0;dx<scale;++dx){int xx=x+i*scale+dx,yy=y+j*scale+dy;
          if((unsigned)xx<320&&yy>=top&&yy<top+8){unsigned target=(unsigned)((yy-top)*320+xx);band[target]=blend(band[target],c,a);}}}
    x+=(font_widths[ch]+1)*scale;}
}
static void center(int x,int y,const char *s,int scale,uint16_t c){text(x-width(s,scale)/2,y,s,scale,c);}
static void label(int x,int y,const char *s){center(x,y+2,s,1,NAVY);center(x,y,s,1,WHITE);}
static void digits(char out[11],uint32_t v){unsigned n=0;do{out[n++]=(char)('0'+v%10);v/=10;}while(v);for(unsigned i=0;i<n/2;++i){char c=out[i];out[i]=out[n-i-1];out[n-i-1]=c;}out[n]=0;}
static void number(int x,int y,uint32_t v,uint16_t c){char s[11];digits(s,v);center(x,y,s,1,c);}
static void add(unsigned sprite,int x,int y,float angle,bool flip,bool shadow) {
  const sh_sprite *s=&sh_sprites[sprite];float sn=sinf(angle),cs=cosf(angle);
  bool character=sprite>=SPR_SKIN0_SPAWN;
  float scale=character?(mode==SH_RESULT?3:(mode==SH_SKIN_MENU||mode==SH_TITLE)?2:1):1;
  // Inverse rotation in Q10; reflection remains in the source coordinate system.
  command d={.x=(int16_t)x,.y=(int16_t)y,.c=(int16_t)lroundf(cs*1024/scale),.s=(int16_t)lroundf(sn*1024/scale),.sprite=(uint16_t)sprite,.shadow=shadow};
  if(flip)d.c=-d.c,d.s=-d.s; // Bit 7 records reflection, while c/s keep a valid rotation.
  if(flip)d.shadow|=128;
  int r=(int)((abs(s->x)+s->w+abs(s->y)+s->h)*scale);
  if(x+r<0||x-r>=320||y+r<0||y-r>=240)return;
  float left=10000,right=-10000,t=10000,b=-10000;
  for(int i=0;i<4;++i){float px=(float)(s->x+((i&1)?s->w:0)),py=(float)(s->y+((i&2)?s->h:0));
    if(flip)px=-px;
    float xx=(px*cs-py*sn)*scale,yy=(px*sn+py*cs)*scale;
    if(xx<left)left=xx;
    if(xx>right)right=xx;
    if(yy<t)t=yy;
    if(yy>b)b=yy;}
  d.l=(int16_t)(x+(int)floorf(left)-1);d.r=(int16_t)(x+(int)ceilf(right)+1);
  d.t=(int16_t)(y+(int)floorf(t)-1);d.b=(int16_t)(y+(int)ceilf(b)+1);
  if(d.r<0||d.l>=320||d.b<0||d.t>=240)return;
  if(command_count<sizeof commands/sizeof commands[0])commands[command_count++]=d;
}
static void draw(const command *d) {
  if(d->t>=top+8||d->b<top)return;
  const sh_sprite *s=&sh_sprites[d->sprite];int l=d->l<0?0:d->l,r=d->r>319?319:d->r;
  int a=d->t<top?top:d->t,b=d->b>top+7?top+7:d->b;
  bool flip=d->shadow&128;
  for(int y=a;y<=b;++y){int dx=l-d->x,dy=y-d->y;
    int u=dx*d->c+dy*d->s,v=flip?dx*d->s-dy*d->c:-dx*d->s+dy*d->c;
    for(int x=l;x<=r;++x,u+=d->c,v+=flip?d->s:-d->s){int xx=((u+512)>>10)-s->x,yy=((v+512)>>10)-s->y;
      if(xx<0||xx>=s->w||yy<0||yy>=s->h)continue;
      unsigned n=(unsigned)(yy*s->w+xx),p=sh_pixels[s->offset+n/2];p=(n&1)?p&15:p>>4;
      if(p){unsigned target=(unsigned)((y-top)*320+x);
        if(d->sprite==SPR_FINISHLIGHT){
          uint16_t bg=band[target];unsigned r=((bg>>11)*(15-p)+31*p)/15;
          unsigned green=(((bg>>5)&63)*(15-p)+63*p)/15,blue=((bg&31)*(15-p)+31*p)/15;
          band[target]=(uint16_t)(r<<11|green<<5|blue);
        }else band[target]=(d->shadow&1)?half(band[target]):sh_palettes[s->palette+p];}}
  }
}
static int wx(float x){return (int)lroundf((x-g->camera_x)*240);}
static int wy(float y){return (int)lroundf(y*240);}
static void prepare(void) {
  command_count=0;rope=false;
  if(mode==SH_PLAY||mode==SH_PAUSE||mode==SH_CONFIRM){
    for(unsigned j=0;j<g->section_count;++j){const sh_section *sc=&g->sections[j];const sh_part *p=&sh_parts[sc->part];
      for(unsigned i=0;i<p->bumpers;++i){const sh_bumper *b=&sh_bumpers[p->bumper+i];
        const sh_bumper_profile *profile=&sh_bumper_profiles[b->profile];
        add(SPR_BUMPER4,wx(b->x+sc->x),wy(b->y),atan2f(profile->s,profile->c),false,false);}
      for(unsigned i=0;i<p->obstacles;++i){const sh_obstacle *o=&sh_obstacles[p->obstacle+i];
        sh_vec rotation=sh_rotations[o->rotation];
        add(SPR_A_WALL45+o->type,wx(o->x+sc->x),wy(o->y),atan2f(rotation.y,rotation.x),false,false);}
      for(unsigned i=0;i<p->hooks;++i){sh_vec h=sh_hooks[p->hook+i];add(SPR_HOOK,wx(h.x+sc->x),wy(h.y),0,false,false);}
    }
    sh_vec closest;
    if(sh_closest(g,&closest))add(SPR_HOOK_NEAR,wx(closest.x),wy(closest.y),0,false,false);
    if(g->hooked){add(SPR_HOOK_ACTIVE,wx(g->hook_x),wy(g->hook_y),0,false,false);rope=true;rope_x=wx(g->hook_x);rope_y=wy(g->hook_y);}
    man_x=wx(g->x);man_y=wy(g->y);
    unsigned sprite=sh_skin_first[records->skin]+g->pose;
    add(sprite,man_x,man_y+3,g->rotation,g->facing,true);add(sprite,man_x,man_y,g->rotation,g->facing,false);
  }else if(mode==SH_TITLE){
    add(SPR_TITLE,106,72,0,false,false);
    float a=sinf(clock_ms*.0018f)*.85f;man_x=244+(int)(sinf(a)*44);man_y=68+(int)(cosf(a)*44);
    rope=true;rope_x=244;rope_y=68;add(SPR_HOOK_ACTIVE,244,68,0,false,false);
    add(sh_skin_first[records->skin]+SH_HOOKED2,man_x,man_y,-a,false,false);
  }else if(mode==SH_SKIN_MENU){
    unsigned page=selection/8;
    for(unsigned i=page*8;i<page*8+8&&i<SH_SKINS;++i){int col=i%4,row=(i%8)/4;
      add(sh_skin_first[i]+SH_SPAWN,40+80*col,66+59*row,0,false,false);}
  }else if(mode==SH_RESULT){
    unsigned n=sh_skin_dance[records->skin],sprite=sh_skin_first[records->skin]+(n?6+(clock_ms/130)%n:SH_VICTORY);
    add(SPR_FINISHLIGHT,160,100,0,false,false);
    add(SPR_PLATFORM0+(clock_ms/33)%11,160,100,0,false,false);
    add(sprite,160,100,0,false,false);
  }
}
static void button(int x,int y,int w,const char *s,bool active) {
  int h=24;rect(x+9,y+3,w-18,h,half(BLUE));circle(x+10,y+15,12,half(BLUE));circle(x+w-11,y+15,12,half(BLUE));
  uint16_t c=active?WHITE:BLUE;rect(x+10,y,w-20,h,c);circle(x+10,y+12,12,c);circle(x+w-11,y+12,12,c);
  center(x+w/2,y+6,s,1,active?BLUE:WHITE);
}
static void finish(void){
  int x=wx(g->finish);
  if(x< -14||x>320)return;
  for(int row=0;row<24;++row)for(int col=0;col<2;++col)rect(x+col*5,row*10,5,10,(row+col)&1?WHITE:NAVY);
}
static void trail(void){
  uint16_t color=half(WHITE);
  for(unsigned i=1;i<g->trail_count;++i){unsigned a=(g->trail_cursor+20-g->trail_count+i-1)%20,b=(a+1)%20;
    line(wx(g->trail[a].x),wy(g->trail[a].y),wx(g->trail[b].x),wy(g->trail[b].y),color);}
}
static void panel(void){rect(43,44,234,153,NAVY);rect(46,47,228,147,BLUE);}
static void overlay(void) {
  if(mode==SH_TITLE){
    label(241,151,records->unlocked==SH_LEVEL_COUNT?"COMPLETE":"LEVEL");number(241,171,records->current+1,WHITE);
    button(17,132,158,continuing?"RESUME":"PLAY",selection==0);button(17,164,158,"LEVELS",selection==1);
    button(17,196,158,"SKINS",selection==2);
    text(193,209,"0: HELP",1,WHITE); // No decorative copy or intrusive HUD.
    text(12,227,"By Bobydeluxe",1,WHITE);
  }else if(mode==SH_PLAY||mode==SH_PAUSE||mode==SH_CONFIRM){
    number(94,10,g->level+1,WHITE);number(228,10,g->level<SH_MAX_LEVEL?g->level+2:SH_LEVEL_COUNT,WHITE);rect(117,11,86,8,NAVY);rect(117,11,(int)(86*g->progress),8,WHITE);
    rect(11,9,3,12,WHITE);rect(18,9,3,12,WHITE);
    if(g->ticks<150&&g->attempt==1&&!g->completed){label(160,210,"HOLD OK TO HOOK");}
    if(g->completed)label(160,45,"LEVEL CLEARED!");
    if(mode==SH_PAUSE){panel();label(160,61,"PAUSED");button(77,90,166,"RESUME",selection==0);button(77,122,166,"RETRY",selection==1);button(77,154,166,"MENU",selection==2);}
    if(mode==SH_CONFIRM){panel();label(160,68,"RESTART LEVEL?");button(77,108,166,"KEEP PLAYING",selection==0);button(77,145,166,"RESTART",selection==1);}
  }else if(mode==SH_HELP){
    label(160,20,"HOW TO PLAY");
    center(160,61,"HOLD OK, EXE OR SHIFT",1,WHITE);center(160,84,"TO GRAB THE NEAREST HOOK",1,WHITE);
    center(160,112,"RELEASE TO FLY",1,GOLD);center(160,135,"USE BUMPERS TO GAIN SPEED",1,WHITE);
    center(160,165,"BACK: PAUSE   0: RETRY",1,WHITE);center(160,186,"HOME: SAVE AND EXIT",1,WHITE);center(160,219,"OK / BACK",1,WHITE);
  }else if(mode==SH_SKIN_MENU){
    label(160,10,"SKINS");unsigned page=selection/8;
    for(unsigned i=page*8;i<page*8+8&&i<SH_SKINS;++i){int x=40+80*(i%4),y=66+59*((i%8)/4);bool open=sh_skin_open(records,i);
      if(!open){rect(x-4,y+8,8,7,NAVY);rect(x-3,y+4,6,5,NAVY);}
      if(i==records->skin)rect(x-18,y+20,36,2,LIME);
      if(i==selection){rect(x-25,y-23,50,2,WHITE);rect(x-25,y+25,50,2,WHITE);rect(x-25,y-23,2,48,WHITE);rect(x+23,y-23,2,48,WHITE);}}
    label(160,161,sh_skin_names[selection]);
    if(sh_skin_open(records,selection))center(160,185,selection==records->skin?"EQUIPPED":"OK: EQUIP",1,LIME);
    else {center(136,185,"FINISH LEVEL",1,WHITE);number(226,185,sh_unlocks[selection],GOLD);}
    center(160,219,"ARROWS: SELECT     BACK: MENU",1,WHITE);
  }else if(mode==SH_LEVEL_MENU){
    label(160,20,"SELECT LEVEL");number(160,75,selection+1,GOLD);
    center(160,110,"<     LEFT / RIGHT     >",1,WHITE);center(160,137,"UP / DOWN: 10 LEVELS",1,WHITE);
    button(74,172,172,"OK: PLAY",true);center(160,219,"BACK: MENU",1,WHITE);
  }else if(mode==SH_RESULT){
    label(160,28,g->level==SH_MAX_LEVEL?"ALL 50 COMPLETE!":"LEVEL CLEARED!");number(160,59,g->level+1,WHITE);
    for(unsigned i=0;i<24;++i){int x=(int)((i*83u+clock_ms/17)%310)+5,y=(int)((i*43u+clock_ms/23)%123)+20;rect(x,y,3,2,i&1?GOLD:LIME);}
    button(69,149,182,g->level==SH_MAX_LEVEL?"REPLAY LEVEL":"NEXT LEVEL",selection==0);button(69,185,182,"MENU",selection==1);
  }else if(mode==SH_SAVE_ERROR){
    label(160,35,"SAVE UNAVAILABLE");center(160,78,"NOT ENOUGH FREE SPACE",1,WHITE);center(160,109,"OK: RETRY AND EXIT",1,WHITE);
    center(160,141,"1: CONTINUE PLAYING",1,WHITE);center(160,175,"BACK: EXIT WITHOUT SAVING",1,GOLD);
  }
  if(failed&&mode!=SH_SAVE_ERROR){rect(304,225,8,9,GOLD);text(305,224,"!",1,NAVY);}
}
void sh_render(const sh_game *game,const sh_stats *s,unsigned view,unsigned selected,bool ongoing,bool save_failed,uint32_t ms,sh_push push) {
  g=game;records=s;mode=view;selection=selected;continuing=ongoing;failed=save_failed;clock_ms=ms;prepare();
  unsigned background=(view==SH_PLAY||view==SH_PAUSE||view==SH_CONFIRM||view==SH_RESULT)?g->background:1;
  for(top=0;top<240;top+=8){
    for(int row=0;row<8;++row)for(unsigned x=0;x<320;++x)band[row*320+x]=sh_backgrounds[background][top+row];
    if(mode==SH_PLAY||mode==SH_PAUSE||mode==SH_CONFIRM){finish();trail();}
    if(rope)line(rope_x,rope_y,man_x,man_y,WHITE);
    for(unsigned i=0;i<command_count;++i)draw(&commands[i]);
    overlay();
    uint32_t hash=2166136261u;for(unsigned i=0;i<320*8;++i)hash=(hash^band[i])*16777619u;
    if(!valid||hash!=hashes[top/8])push(0,top,320,8,band);
    hashes[top/8]=hash;
  }valid=true;
}
