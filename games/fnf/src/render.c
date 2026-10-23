#include "render.h"
#include "inflate.h"
#include "../../../launcher/src/font.h"
#include <stdio.h>
#include <string.h>
#include <math.h>
#ifdef FNF_TEST
#include <assert.h>
#endif
#define C(r,g,b) (((r)>>3)<<11|((g)>>2)<<5|((b)>>3))
enum { BLACK=0,WHITE=65535,INK=C(19,15,31),PINK=C(249,54,143),YELLOW=C(255,220,63),CYAN=C(52,223,243),GREY=C(134,140,160) };
static uint16_t strip[320*8];
static uint8_t background[9600];
static uint32_t hashes[30];
static unsigned loaded=NONE;static int top;static bool invalid=true;
static uint16_t ink=WHITE;
static uint16_t mix(uint16_t bg,uint16_t fg,unsigned a){return (uint16_t)(((((bg>>11)*(15-a)+(fg>>11)*a)/15)<<11)|(((((bg>>5)&63)*(15-a)+((fg>>5)&63)*a)/15)<<5)|(((bg&31)*(15-a)+(fg&31)*a)/15));}
void render_invalidate(void){invalid=true;}
bool render_prepare(unsigned id){if(id==loaded)return true;if(id>=SPRITE_COUNT||!sprites[id].compressed)return false;const Sprite*s=&sprites[id];if(!inflate_raw(art_pixels+s->offset,s->size,background,sizeof(background)))return false;loaded=id;render_invalidate();return true;}
static void rect(int x,int y,int w,int h,uint16_t c){int x0=x<0?0:x,x1=x+w>320?320:x+w,y0=y<top?top:y,y1=y+h>top+8?top+8:y+h;for(int yy=y0;yy<y1;yy++)for(int xx=x0;xx<x1;xx++)strip[(yy-top)*320+xx]=c;}
static void panel(int x,int y,int w,int h,uint16_t c){rect(x+2,y+2,w,h,BLACK);rect(x,y,w,h,c);}
static void shade(int y,int h,unsigned a){int y0=y<top?top:y,y1=y+h>top+8?top+8:y+h;for(int yy=y0;yy<y1;yy++)for(int x=0;x<320;x++){unsigned p=(yy-top)*320+x;strip[p]=mix(strip[p],INK,a);}}
static void image(unsigned id,int x,int y){
    if(id>=SPRITE_COUNT)return;const Sprite*s=&sprites[id];if(s->compressed||x+s->w<=0||x>=320||y+s->h<=top||y>=top+8)return;
    const uint8_t*base=art_pixels+s->offset,*data=base+s->h*2;const uint16_t*pal=palettes[s->palette];int start=top>y?top-y:0,end=top+8-y;if(end>s->h)end=s->h;
    for(int yy=start;yy<end;yy++){unsigned off=base[yy*2]|base[yy*2+1]<<8;const uint8_t*p=data+off;unsigned at=0;uint16_t*row=strip+(yy+y-top)*320;
        while(at<s->w){unsigned token=*p++,len=token>>1;if(!len)return;
            if(token&1){for(unsigned i=0;i<len;i++){int xx=x+(int)(at+i);if((unsigned)xx<320)row[xx]=pal[(p[i/2]>>((i&1)*4))&15];}p+=(len+1)/2;}at+=len;
        }
    }
}
static const np_glyph_t*glyph(unsigned c){return &np_font_body.glyphs[(c>=32&&c<=126?c:'?')-32];}
static int width(const char*s){int n=0;for(;*s;s++)n+=glyph((unsigned char)*s)->advance;return n;}
static void text(int x,int y,const char*s){
#ifdef FNF_TEST
    assert(x>=0&&x+width(s)<=320&&y>=0&&y+13<=240);
#endif
    if(y+13<=top||y>=top+8)return;
    for(;*s;s++){const np_glyph_t*g=glyph((unsigned char)*s);int xx=x+g->x,yy=y+np_font_body.ascent+g->y;
        for(unsigned j=0;j<g->h;j++){int row=yy+(int)j;if(row<top||row>=top+8)continue;for(unsigned i=0;i<g->w;i++){int col=xx+(int)i;if((unsigned)col>=320)continue;unsigned at=j*g->w+i,a=(np_font_body.data[g->offset+at/2]>>((at&1)*4))&15;if(a){unsigned p=(row-top)*320+col;strip[p]=a==15?ink:mix(strip[p],ink,a);}}}x+=g->advance;
    }
}
static void fit(int x,int y,int max,const char*s){char buf[128];unsigned n=0;int w=0;for(;s[n]&&n<sizeof(buf)-4;n++){int a=glyph((unsigned char)s[n])->advance;if(w+a>max-12)break;buf[n]=s[n];w+=a;}if(s[n]){buf[n++]='.';buf[n++]='.';buf[n++]='.';}buf[n]=0;text(x,y,buf);}
static void center(int y,const char*s){int w=width(s);if(w>304)fit(8,y,304,s);else text((320-w)/2,y,s);}
static void heading(const char*s){panel(0,0,320,30,INK);ink=PINK;fit(12,8,296,s);ink=WHITE;}
static void footer(const char*s){rect(0,218,320,22,INK);ink=WHITE;center(222,s);}
static void choice(int y,const char*s,bool active){panel(52,y,216,23,active?PINK:INK);ink=WHITE;center(y+5,s);}
static void draw_background(bool menu,float dark){
    if(loaded==NONE){rect(0,top,320,8,INK);return;}const uint16_t*pal=palettes[sprites[loaded].palette];uint16_t lut[16];unsigned a=menu?8:(unsigned)fminf(14,fmaxf(0,dark*15));
    for(unsigned i=0;i<16;i++)lut[i]=mix(pal[i],menu?C(246,170,38):BLACK,a);
    for(int yy=0;yy<8;yy++){unsigned row=(top+yy)/2*160;uint16_t*out=strip+yy*320;for(unsigned x=0;x<160;x++){unsigned at=row+x;uint16_t c=lut[(background[at/2]>>((at&1)*4))&15];out[x*2]=out[x*2+1]=c;}}
}
static void actor(const Game*g,unsigned i){const Actor*a=&g->actors[i];if(a->character==NONE||!a->visible)return;const Character*c=&characters[a->character];unsigned anim=a->animation;int32_t age=g->time-a->since;
    if(g->time>=a->until){anim=((int)floorf(g->beat)&1)?c->dance:c->idle;age=(int32_t)((g->beat-floorf(g->beat))*60000/g->bpm*16);}
    if(g->beautiful)anim=beautiful_animation[anim];const Animation*d=&animations[anim];unsigned frame=(unsigned)(age<0?0:age)/1600;if(frame>=d->count)frame=d->count-1;unsigned id=d->frame[frame];const Sprite*s=&sprites[id];
    static const int x[6]={252,64,160,34,114,214};static const int foot[6]={191,186,171,171,177,182};int px=x[i],py=foot[i];
    if(g->actors[3].character!=NONE){static const int xx[6]={265,88,170,28,144,209};px=xx[i];py=i==2?157:190;}
    int cam=g->focus==0?-2:g->focus==1?2:0;px+=cam;py-=(g->p.beat&&g->beat>=0&&g->beat-floorf(g->beat)<.12f)?(int)fminf(2,g->bop):0;
    image(id,px+s->ox,py+s->oy);
}
static int notey(const Game*g,int32_t time){float d=(time-g->time)/16.0f*.16f*g->speed;return g->p.downscroll?193-(int)d:43+(int)d;}
static void gameplay(const Game*g){
    actor(g,2);actor(g,3);actor(g,4);actor(g,5);actor(g,1);actor(g,0);
    if(g->cover)shade(70,135,9);
    if(g->pulse&&((int)g->beat&1)){rect(0,68,3,133,PINK);rect(317,68,3,133,CYAN);}
    shade(0,71,10);shade(204,36,14);const uint16_t colour[4]={C(198,75,230),CYAN,C(87,235,92),C(249,67,70)};int receptor=g->p.downscroll?193:43;
    for(unsigned i=0;i<NOTE_CAP;i++){const Note*n=&g->notes[i];if(n->state==FINISHED)continue;int x=(n->direction<4?194:20)+(n->direction&3)*34,y=notey(g,n->time),end=notey(g,n->end);
        if(n->end>n->time){int a=y<end?y:end,b=y>end?y:end;if(n->state==HOLDING){if(g->p.downscroll)b=receptor;else a=receptor;}if(a<26)a=26;if(b>204)b=204;if(b>a){rect(x-3,a,7,b-a,INK);rect(x-2,a,5,b-a,n->state==MISSED?GREY:colour[n->direction&3]);}}
        if(n->state==WAITING&&y>=24&&y<=205)image(ui_notes[0][n->direction&3],x-11,y-11);
    }
    for(unsigned side=0;side<2;side++)for(unsigned d=0;d<4;d++){unsigned state=1;if(side==1&&(g->held&(1u<<d)))state=2;for(unsigned i=0;i<NOTE_CAP;i++){const Note*n=&g->notes[i];if(n->state==HOLDING&&n->direction==d+(side?0:4))state=3;}image(ui_notes[state][d],(side?194:20)+d*34-11,receptor-11);}
    char buf[96];ink=WHITE;const Track*t=&tracks[charts[g->chart].track];fit(8,3,180,songs[t->song].name);ink=CYAN;text(210,3,difficulties[charts[g->chart].difficulty]);
    if(g->time>=0){rect(0,20,(int)((int64_t)g->time*320/charts[g->chart].end),2,CYAN);if(g->p.beat&&g->beat-floorf(g->beat)<.12f)rect(156,24,8,2,YELLOW);}
    int fill=(int)(g->health*106);rect(52,209,216,7,BLACK);rect(54,211,212,3,C(220,50,60));rect(266-fill,211,fill,3,C(88,226,72));
    image(g->actors[1].icon[g->health>1.6f?1:0],30,202);image(g->actors[0].icon[g->health<.4f?1:0],270,202);
    snprintf(buf,sizeof(buf),"Score: %ld   Misses: %u   Combo: %u",(long)(g->score_milli/1000),g->judged[MISS],g->combo);ink=WHITE;center(225,buf);
    if(g->time-g->judgement_at<6500){static const char*const names[]={"SICK!!","GOOD!","BAD","SHIT","MISS"};ink=g->judgement==SICK?CYAN:g->judgement==GOOD?YELLOW:PINK;center(85,names[g->judgement]);snprintf(buf,sizeof(buf),"%u",g->combo);center(100,buf);}
    if(g->time<0){int beat=(int)ceilf(-g->time*g->bpm/960000);const char*s=beat>=3?"READY?":beat==2?"SET!":"GO!";panel(106,94,108,28,INK);ink=beat==1?CYAN:YELLOW;center(102,s);}
    /* A bounded visual pulse replaces neither note timing nor input sampling. */
    if(g->flash_until>g->time){rect(0,72,2,131,WHITE);rect(318,72,2,131,WHITE);}
}
static void menus(const Game*g,const Menu*m){
    char b[112];
    if(m->view==TITLE){image(UI_LOGO,(320-sprites[UI_LOGO].w)/2,4);choice(116,"STORY MODE",m->selected==0);choice(144,"FREEPLAY",m->selected==1);choice(172,"OPTIONS",m->selected==2);ink=INK;text(9,201,"By Bobydeluxe");text(220,201,"0: Credits");footer("OK: Select   Back: Quit");}
    else if(m->view==STORY){heading("STORY MODE");const Level*l=&levels[m->week];snprintf(b,sizeof(b),"<  %s  >",difficulties[m->difficulty]);ink=CYAN;center(38,b);
        panel(12,60,296,117,INK);ink=PINK;snprintf(b,sizeof(b),"%02u / %02u",m->week+1,LEVEL_COUNT);text(23,69,b);ink=WHITE;fit(23,89,274,l->name);
        for(unsigned i=0;i<l->count;i++){ink=GREY;fit(25,109+i*15,270,songs[l->song[i]].name);}ink=INK;center(190,"Up / Down: Week   Left / Right: Difficulty");footer("OK: Start week   Back: Menu");
    }else if(m->view==FREEPLAY){heading("FREEPLAY");for(int row=-2;row<=2;row++){int song=(int)m->song+row,y=91+row*23;if(song<0||song>=SONG_COUNT)continue;panel(14,y,292,21,row==0?PINK:INK);ink=WHITE;fit(24,y+4,271,songs[song].name);}
        panel(14,167,292,45,INK);unsigned id=game_chart(m->song,m->variant,m->difficulty);ink=CYAN;snprintf(b,sizeof(b),"< %s >   1: %s",difficulties[m->difficulty],variants[m->variant]);center(172,b);ink=WHITE;if(id!=NONE){snprintf(b,sizeof(b),"Best: %ld   Completion: %u.%02u%%",(long)g->p.record[id].score,g->p.record[id].completion/100,g->p.record[id].completion%100);center(191,b);}footer("Up / Down: Song   OK: Play   Back: Menu");
    }else if(m->view==OPTIONS){heading("OPTIONS");const char*labels[]={"DOWNSCROLL","GHOST TAPPING","BEAT ANIMATION"};const unsigned values[]={g->p.downscroll,g->p.ghost,g->p.beat};for(unsigned i=0;i<3;i++){snprintf(b,sizeof(b),"%s: %s",labels[i],values[i]?"ON":"OFF");choice(46+i*32,b,m->selected==i);}panel(12,150,296,58,INK);ink=WHITE;center(158,"Left / Down / Up / Right");center(175,"or 7 / 8 / ( / )");ink=GREY;center(192,"All levels are played without audio.");footer("OK / Left / Right: Change   Back: Menu");
    }else if(m->view==CREDITS){heading("CREDITS");panel(12,44,296,164,INK);ink=PINK;center(53,"FRIDAY NIGHT FUNKIN'");ink=WHITE;center(77,"Original game by The Funkin' Crew");center(96,"Art and charts: original contributors");center(115,"Calculator adaptation by Bobydeluxe");ink=GREY;center(144,"27 songs / 166 charts / no audio");center(162,"Original art remains with its owners.");center(185,"See README and LICENSES for credits.");footer("OK / Back: Menu");
    }else if(m->view==PAUSE){shade(0,240,11);heading("PAUSED");choice(63,"RESUME",m->selected==0);choice(95,"RETRY",m->selected==1);choice(127,"SONG SELECT",m->selected==2);choice(159,"QUIT",m->selected==3);footer("OK: Select   Back: Resume");
    }else if(m->view==RESULT){shade(0,240,11);heading(g->won?"SONG COMPLETE":"GAME OVER");panel(26,42,268,99,INK);ink=g->won?CYAN:PINK;center(51,g->fault?"DATA ERROR":g->won?game_rank(g):"TRY AGAIN");ink=WHITE;snprintf(b,sizeof(b),"Score: %ld    Best combo: %u",(long)(g->score_milli/1000),g->max_combo);center(75,b);snprintf(b,sizeof(b),"Sick: %u   Good: %u   Misses: %u",g->judged[SICK],g->judged[GOOD],g->judged[MISS]);center(95,b);snprintf(b,sizeof(b),"Completion: %u.%02u%%",game_completion(g)/100,game_completion(g)%100);center(115,b);choice(151,g->won&&m->story?"NEXT SONG":"RETRY",m->selected==0);choice(181,"SONG SELECT",m->selected==1);footer("OK: Select   Back: Song select");
    }else if(m->view==QUIT){shade(0,240,11);heading("QUIT GAME?");choice(90,"KEEP PLAYING",m->selected==0);choice(125,"SAVE AND QUIT",m->selected==1);footer("OK: Select   Back: Menu");
    }else if(m->view==SAVE_ERROR){shade(0,240,12);heading("SAVE INCOMPLETE");panel(14,65,292,111,INK);ink=WHITE;center(75,"There is not enough free storage.");center(98,"OK: Retry saving");center(121,"1: Return to the menu");center(144,"Back: Exit without saving");footer("Free space in the calculator's Python app.");}
}
void render(const Game*g,const Menu*m,DisplayPush push,void (*poll)(void)){
    unsigned bg=(m->view==PLAY||m->view==PAUSE||m->view==RESULT)?stage_sprites[tracks[charts[g->chart].track].stage]:UI_MENU;
    if(!render_prepare(bg)){loaded=NONE;invalid=true;}
    bool play=m->view==PLAY||m->view==PAUSE||m->view==RESULT;
    for(top=0;top<240;top+=8){draw_background(!play,play?g->dark:0);if(play)gameplay(g);if(m->view!=PLAY)menus(g,m);
        uint32_t hash=2166136261u;for(unsigned i=0;i<320*8;i++)hash=(hash^strip[i])*16777619u;unsigned row=(unsigned)top/8;if(invalid||hash!=hashes[row]){push(0,top,320,8,strip);hashes[row]=hash;}if(poll)poll();
    }invalid=false;
}
