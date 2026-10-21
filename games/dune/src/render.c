#include "render.h"
#include "canvas.h"
#include <math.h>
#define WHITE 65535
#define GOLD RGB(255,222,38)
#define DARK RGB(24,28,73)
static uint8_t decoded[460];
static int16_t surface_y[320],texture_x[320];
static int floor_int(float x){int i=(int)x;return x<i?i-1:i;}
static void background_row(unsigned scene,int row){const Background*b=&backgrounds[scene];const uint8_t*p=b->data+b->offset[row];unsigned x=0;while(x<460){unsigned token=*p++,n=token>=128?token-127:token+1;if(token>=128){uint8_t v=*p++;for(unsigned i=0;i<n;i++)decoded[x++]=v;}else for(unsigned i=0;i<n;i++)decoded[x++]=*p++;}}
static void background(unsigned scene,int offset,bool dark){const Background*b=&backgrounds[scene];offset%=460;if(offset<0)offset+=460;for(int y=band;y<band+8;y++){background_row(scene,y);for(int x=0;x<320;x++){int u=x+offset;if(u>=460)u-=460;uint16_t c=b->palette[decoded[u]];strip[(y-band)*320+x]=dark?blend(DARK,c,2):c;}}}
static void button(int x,int y,int w,const char*s,bool selected){rect(x,y,w,29,selected?GOLD:DARK);center(x+w/2,y+6,s,selected?DARK:WHITE,1);}
static unsigned digits(unsigned n){unsigned count=1;while(n>=10){n/=10;++count;}return count;}
static int digit_width(unsigned n,int size){return (int)digits(n)*text_font(size)->glyphs['0'-32].advance;}
static void wallet(unsigned coins){rect(6,6,29+digit_width(coins,1),22,DARK);image(&Score_Coin,10,10,14,14);number(29,11,coins,WHITE,1,false);}
static void ball_draw(unsigned style,int cx,int cy,int size,float angle){const Image*im=balls[style];if(!ball_styles[style].rotates){image(im,cx-size/2,cy-size/2,size,size);return;}float co=cosf(angle),si=sinf(angle);int r=size/2+2;for(int y=cy-r<band?band:cy-r;y<=cy+r&&y<band+8;y++)for(int x=cx-r;x<=cx+r;x++){float dx=(x-cx)*22.f/size,dy=(y-cy)*22.f/size;int sx=(int)(11+dx*co+dy*si),sy=(int)(11-dx*si+dy*co);if((unsigned)sx<22&&(unsigned)sy<22){unsigned i=sy*22+sx,v=im->pixels[i/2],n=i&1?v&15:v>>4;if(n)pixel(x,y,im->palette[n]);}}}
static uint16_t mix_color(uint16_t a,uint16_t b,float t){unsigned n=(unsigned)(t*256);if(n>256)n=256;unsigned r=((a>>11)*(256-n)+(b>>11)*n)>>8,g=(((a>>5)&63)*(256-n)+((b>>5)&63)*n)>>8,v=((a&31)*(256-n)+(b&31)*n)>>8;return (uint16_t)((r<<11)|(g<<5)|v);}
static void trail_draw(const Game*g,unsigned style,float scale){
 const BallStyle*b=&ball_styles[style];float h=b->height*scale,w=h*g->trail_scale;if(w<1||h<1)return;
 float co=cosf(g->trail_angle),si=sinf(g->trail_angle),cx=(g->x+g->vx/75-g->camera_x)*scale,cy=(g->y+g->vy/75-g->camera_y)*scale;
 float px[3]={cx-co*w,cx-si*h*.5f,cx+si*h*.5f},py[3]={cy-si*w,cy+co*h*.5f,cy-co*h*.5f};float minx=fminf(px[0],fminf(px[1],px[2])),maxx=fmaxf(px[0],fmaxf(px[1],px[2])),miny=fminf(py[0],fminf(py[1],py[2])),maxy=fmaxf(py[0],fmaxf(py[1],py[2]));
 int x0=(int)floorf(minx)-2,x1=(int)ceilf(maxx)+2,y0=(int)floorf(miny)-3,y1=(int)ceilf(maxy)+3;if(x0<0)x0=0;if(x1>319)x1=319;if(y0<band)y0=band;if(y1>band+7)y1=band+7;
 for(int y=y0;y<=y1;y++)for(int x=x0;x<=x1;x++){
  float dx=x-cx,dy=y-cy,u=1+(dx*co+dy*si)/w,v=(-dx*si+dy*co)/h+.5f+b->offset_y*.01f;
  if(u<0||u>1||fabsf(v-.5f)>u*.5f)continue;
  if(original_ball_ids[style]==13&&u>.5f&&fabsf(v-.5f)<(u-.5f)*.75f)continue;
  uint16_t color;
  if(b->type==2)color=b->colors[((int)(u*10)+(int)(v*10))%2];
  else if(b->type==4)color=b->colors[((unsigned)(u*123)+(unsigned)(v*57))%13==0?1:0];
  else{float t=b->type==1?u:v;if(b->type==3){float rx=u-(b->offset_x?.5f+b->offset_x/(float)b->height:0),ry=v-.5f;t=sqrtf(rx*rx+ry*ry)/.8f;}if(t<0)t=0;if(t>1)t=1;
   if(b->count==1){float blend=t<.45f?t/.45f:t>.55f?(1-t)/.45f:1;color=mix_color(WHITE,b->colors[0],blend);}
   else{float p=t*(b->count-1);unsigned i=(unsigned)p;color=i>=(unsigned)b->count-1?b->colors[b->count-1]:mix_color(b->colors[i],b->colors[i+1],p-i);}
  }pixel(x,y,color);
 }
}
/* One native-size pass: no enlarged bitmap and no displaced shadow. */
static void label(const char*s,int y,uint16_t c){center(160,y,s,c,1);}
void render_invalidate(void){invalidate();}
void render(const Game*g,const Stats*s,unsigned view,unsigned selected,bool reserved,bool failed,uint32_t now,Push push){
 (void)reserved;
 unsigned scene=0,ball=s->ball<BALL_COUNT?s->ball:0;float scale=WORLD_SCALE*g->zoom;int bx=(int)((g->x-g->camera_x)*scale),by=(int)((g->y-g->camera_y)*scale),radius=(int)(28*scale);if(radius<3)radius=3;
 if(view==PLAY){for(int x=0;x<320;x++){float wx=g->camera_x+x/scale;unsigned i=game_terrain(g,wx);float y=(terrain_surface(&g->terrain[i],wx)-g->camera_y)*scale;surface_y[x]=(int16_t)(y< -1000?-1000:y>1000?1000:y);int tx=floor_int(wx*.125f)%160;texture_x[x]=(int16_t)(tx<0?tx+160:tx);}}
 for(band=0;band<240;band+=8){
  background(scene,view==PLAY?floor_int(g->camera_x*WORLD_SCALE*.1f):(int)(now/100),view!=PLAY&&view!=TITLE);
  if(view==PLAY){
   const Image*earth=earths[scene];const Landscape*l=&landscapes[scene];
   int line_y=(int)((200-g->camera_y)*scale);for(int x=0;x<320;x+=8)rect(x,line_y,4,1,WHITE);
   for(int x=0;x<320;x++){int y=surface_y[x];for(int py=y<band?band:y;py<band+8;py++){uint16_t c=l->ground;if(py-y<3)c=py==y?l->line:l->surface;else if(py-y<37){int tx=texture_x[x],ty=(py-y-3)%34;c=image_color(earth,tx,ty);}pixel(x,py,c);}}
   trail_draw(g,ball,scale);
   for(unsigned i=0;i<COIN_COUNT;i++)if(g->coins[i].active){int x=(int)((g->coins[i].x-g->camera_x)*scale),y=(int)((g->coins[i].y-g->camera_y)*scale),sz=(int)(43*scale);if(sz<5)sz=5;if(x> -sz&&x<320+sz&&y>band-sz&&y<band+8+sz)image(&Score_Coin,x-sz/2,y-sz/2,sz,sz);}
   ball_draw(ball,bx,by,radius*2,g->rotation);if(g->phase==CRASHED)circle(bx,by,2,GOLD);
   wallet(s->coins>UINT32_MAX-g->earned?UINT32_MAX:s->coins+g->earned);int score_width=digit_width(g->score,2);rect(153-score_width/2,8,14+score_width,28,DARK);number(160,13,g->score,WHITE,2,true);image(&Pause,295,12,13,13);
   if(g->popup_age){const char*msg=g->popup==4?"PERFECT SLIDE! +3":g->popup==5?"NICE SLIDE! +1":g->popup==3?"AWESOME! +3":g->popup==2?"GREAT! +2":"+1";int w=text_width(msg,1);rect(154-w/2,69,w+12,22,DARK);label(msg,74,GOLD);}
   if(g->tick<120&&s->runs<=1)label("HOLD OK TO SPEED UP",205,WHITE);
  }else if(view==TITLE){
   wallet(s->coins);int w=digit_width(s->best,1);rect(279-w,6,w+35,22,DARK);image(&crown,284-w,11,15,10);number(305-w,11,s->best,WHITE,1,false);
   center(160,48+(int)(now/600%2),"DUNE",WHITE,3);
   if(selected==0)circle(120,154,35,GOLD);else circle(200,154,35,GOLD);
   image(&PlayIcon,90,124,60,60);image(&Shop,170,124,60,60);center(120,190,"PLAY",WHITE,1);center(200,190,"SHOP",WHITE,1);
   text(10,222,"By Bobydeluxe",WHITE,1);text(230,222,"0: HOW TO",WHITE,1);
  }else if(view==SHOP||view==BUY){
   wallet(s->coins);center(172,11,"BALLS",WHITE,1);image(&Balls,279,3,26,26);
   if(selected>=BALL_COUNT)selected=0;
   for(unsigned i=0;i<BALL_COUNT;i++){int x=12+(int)(i%4)*76,y=40+(int)(i/4)*57;bool owned=shop_owned(s,false,i);rect(x,y,68,49,DARK);if(i==selected)outline(x-1,y-1,70,51,GOLD);image(balls[i],x+22,y+3,24,24);if(owned)center(x+34,y+32,s->ball==i?"EQUIPPED":"OWNED",WHITE,1);else number(x+34,y+32,ball_styles[i].price,GOLD,1,true);}
   label(ball_styles[selected].name,159,WHITE);
   unsigned price=ball_styles[selected].price;
   button(75,180,170,shop_owned(s,false,selected)?"OK: EQUIP":s->coins<price?"NOT ENOUGH COINS":"OK: BUY",true);center(160,220,"ARROWS: SELECT    BACK: MENU",WHITE,1);
   if(view==BUY){rect(31,68,258,102,DARK);outline(31,68,258,102,GOLD);center(160,81,"BUY THIS ITEM?",WHITE,1);number(160,111,price,GOLD,1,true);center(160,144,"OK: BUY    BACK: CANCEL",WHITE,1);}
  }else if(view==HELP){label("HOW TO PLAY",25,GOLD);label("HOLD OK, EXE OR SHIFT",68,WHITE);label("Speed up downhill. Dive in the air.",96,WHITE);label("Release uphill to take off.",122,WHITE);label("Cross the line. Land on the downslope.",148,WHITE);label("Collect coins to buy new balls.",174,WHITE);label("BACK pauses. HOME saves and exits.",199,WHITE);label("OK / BACK: RETURN",222,GOLD);
  }else if(view==PAUSE){label("PAUSED",47,GOLD);button(86,94,148,"RESUME",selected==0);button(86,134,148,"RESTART",selected==1);button(86,174,148,"MENU",selected==2);
  }else if(view==RESULT){label("GAME OVER",34,GOLD);number(160,79,g->score,WHITE,3,true);label("BEST",130,WHITE);number(160,151,s->best,GOLD,1,true);wallet(s->coins);button(41,184,112,"RETRY",selected==0);button(167,184,112,"MENU",selected==1);
  }else{label("STORAGE FULL",42,GOLD);label("Progress could not be saved.",100,WHITE);label("OK: RETRY SAVE AND EXIT",146,WHITE);label("1: KEEP PLAYING",175,WHITE);label("BACK: EXIT WITHOUT SAVING",204,WHITE);}
  if(failed&&view!=SAVE_ERROR)text(104,222,"SAVE FAILED",GOLD,1);
  present(push);
 }
}
