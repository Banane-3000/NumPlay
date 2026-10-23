/* Additional games share one small working state and the original game's
 * colours, shapes, input, points and CPU levels. Included once by main.c. */
#ifndef TWOPLAYER_PARTY_H
#define TWOPLAYER_PARTY_H
typedef struct { float x,y,vx,vy; uint32_t ready,last; int aux; } PartyPlayer;
typedef struct { float x,y,vx,vy; int16_t life; int8_t own,kind; } PartyShot;
static struct {
  PartyPlayer p[2]; PartyShot shot[12];
  float x,y,vx,vy,value;
  uint32_t started,at,next,signal;
  uint16_t mark[2]; uint8_t flags[2],seq[8],index[2],n;
} party;
static float sq(float a) { return a*a; }
static float distance2(float x,float y,float a,float b) { return sq(x-a)+sq(y-b); }
static bool reached(uint32_t t) { return (int32_t)(now-t)>=0; }
static float seconds(void) { return ms*.001f; }
static void party_start(void) {
  memset(&party,0,sizeof party);party.started=party.at=now;party.p[0].x=52;party.p[1].x=268;
  for(int p=0;p<2;p++) party.p[p].y=190,party.mark[p]=3;
}
static void bot_to(float x,float y,float margin) {
  pad[1]=(party.p[1].x<x-margin?B_RIGHT:party.p[1].x>x+margin?B_LEFT:0)|
         (party.p[1].y<y-margin?B_DOWN:party.p[1].y>y+margin?B_UP:0);
  tap[1]=0;
}
static void actor2(int x,int feet,int p) {
  disc(x+2,feet+1,10,HEX(0x232944));box(x-7,feet-19,14,19,5,team[p]);
  disc(x,feet-25,7,light[p]);disc(x+(p?-2:2),feet-26,1,INK);
  box(x-8,feet-3,6,5,2,dark[p]);box(x+2,feet-3,6,5,2,dark[p]);
}
static void arena(color_t bg) {
  gfx_fill(0,22,320,218,INK);box(6,28,308,206,12,bg);
}
static void centre_text(int y,const char*s,color_t c) { say(&np_font_body,160,y,s,c); }
static void time_text(int y,uint32_t elapsed,uint32_t duration) {
  char s[16];char*e=num(s,elapsed<duration?(duration-elapsed+999)/1000:0);*e++='s';*e=0;centre_text(y,s,GOLD);
}
static int shot_slot(void) { for(int i=0;i<12;i++)if(party.shot[i].life<=0)return i;return -1; }
static void tie_dead(void) { if(!party.mark[0]||!party.mark[1])point(!party.mark[0]&&!party.mark[1]?2:party.mark[0]?0:1); }

/* ------------------------------------------------------------ Quick Draw */
static void quick_start(void) {
  party_start();party.signal=now+1800+rn(2800);
  party.next=party.signal+(cpu==1?700:cpu==2?500:350)+rn(cpu==1?400:cpu==2?300:200);
}
static void quick_step(void) {
  if(phase)return;
  if(cpu&&reached(party.next))tap[1]|=B_FIRE;
  int a=!!(tap[0]&B_FIRE),b=!!(tap[1]&B_FIRE);
  if(a||b)point(a&&b?2:reached(party.signal)?(a?0:1):(a?1:0));
}
static void quick_paint(void) {
  arena(HEX(0x262D52));bool go=reached(party.signal);
  for(int p=0;p<2;p++){actor2(74+p*172,197,p);box(32+p*172,81,84,48,12,go?team[p]:dark[p]);}
  centre_text(55,T("Quick Draw"),WHITE);centre_text(151,go?T("DRAW!"):T("Wait..."),go?GOLD:WHITE);
  if(!go)centre_text(222,T("Too soon loses the round"),HEX(0xB5BEDF));
}

/* ------------------------------------------------------------ Tug of War */
static void tug_start(void) { party_start();party.next=now+500; }
static void tug_step(void) {
  if(phase)return;
  if(cpu&&reached(party.next)){tap[1]|=B_FIRE;party.next=now+(cpu==1?500:cpu==2?350:250)+rn(cpu==1?150:cpu==2?120:90);}
  int pull[2]={0};
  for(int p=0;p<2;p++)if((tap[p]&B_FIRE)&&ago(party.p[p].last)>=100){pull[p]=1;party.p[p].last=now;}
  party.value+=(pull[1]-pull[0])*7.5f;
  if(party.value>0)party.value=clampf(party.value-seconds()*3,0,100);
  else party.value=clampf(party.value+seconds()*3,-100,0);
  if(party.value<-67)point(0);else if(party.value>67)point(1);
  else if(ago(party.started)>30000)point(party.value<-1?0:party.value>1?1:2);
}
static void tug_paint(void) {
  arena(HEX(0x262D52));centre_text(56,T("Tug of War"),WHITE);time_text(79,ago(party.started),30000);
  gfx_fill(91,101,2,100,team[0]);gfx_fill(227,101,2,100,team[1]);
  int x=160+(int)party.value;
  for(int p=0;p<2;p++){actor2(x+(p?85:-85),182,p);gfx_fill(x+(p?12:-80),141,68,4,HEX(0xC5CBE4));}
  box(x-6,131,12,19,3,GOLD);centre_text(219,T("Tap, release, pull!"),WHITE);
}

/* ------------------------------------------------------------ Football */
static void football_start(void) {
  party_start();party.p[0].x=66;party.p[1].x=254;party.p[0].y=party.p[1].y=131;
  party.x=160;party.y=131;
}
static void football_step(void) {
  if(phase)return;
  float dt=seconds();
  if(cpu){bot_input();if(bot_due()){
    float target=party.x+15,ty=party.y;
    if(party.vx>130&&party.x>190)target=282,ty=clampf(party.y+party.vy*.15f,60,202);
    bot_to(clampf(target,20,300),ty,3);
    if(distance2(party.x,party.y,party.p[1].x,party.p[1].y)<sq(cpu==1?19:27)&&reached(party.p[1].ready))tap[1]|=B_FIRE;
  bot.pad=pad[1];}}
  for(int p=0;p<2;p++){
    PartyPlayer*a=&party.p[p];float speed=is_cpu(p)?(cpu==1?70:cpu==2?85:100):116;
    float dx=dirx(p),dy=diry(p);if(dx&&dy)dx*=.7071f,dy*=.7071f;
    a->vx=dx*speed;a->vy=dy*speed;a->x=clampf(a->x+a->vx*dt,20,300);a->y=clampf(a->y+a->vy*dt,43,220);
    float x=party.x-a->x,y=party.y-a->y,d=len(x,y);
    if(d<17){
      if(d<.1f){x=p?-1:1;y=0;d=1;}
      x/=d;y/=d;
      party.x=a->x+x*17;party.y=a->y+y*17;
      float v=(party.vx-a->vx)*x+(party.vy-a->vy)*y;
      if(v<0)party.vx-=1.5f*v*x,party.vy-=1.5f*v*y;
      party.vx+=a->vx*.13f;party.vy+=a->vy*.13f;
    }
    if((tap[p]&B_FIRE)&&reached(a->ready)){a->ready=now+(is_cpu(p)?1100-cpu*180:350);
      if(d<30){float gx=p?5:315,gy=131;
        if(is_cpu(p))gy=131+(cpu==1?rn(70)-35:(party.p[0].y>=131?-19:19)+rn(cpu==2?18:6)-(cpu==2?9:3));
        float vx=gx-party.x,vy=gy-party.y,n=len(vx,vy);
        party.vx=vx/n*260;party.vy=vy/n*260;}}
  }
  party.x+=party.vx*dt;party.y+=party.vy*dt;party.vx*=1-dt*.55f;party.vy*=1-dt*.55f;
  float v=len(party.vx,party.vy);if(v>310)party.vx*=310/v,party.vy*=310/v;
  if(party.y<39)party.y=39,party.vy=__builtin_fabsf(party.vy)*.85f;
  if(party.y>223)party.y=223,party.vy=-__builtin_fabsf(party.vy)*.85f;
  if(party.y>106&&party.y<156){if(party.x<10)point(1);if(party.x>310)point(0);}
  else{if(party.x<18)party.x=18,party.vx=__builtin_fabsf(party.vx)*.85f;if(party.x>302)party.x=302,party.vx=-__builtin_fabsf(party.vx)*.85f;}
  if(ago(party.started)>45000)point(2);
}
static void football_paint(void) {
  arena(HEX(0x254E46));gfx_rrect_outline(14,35,292,194,8,2,HEX(0x9CC4AE),256);
  gfx_fill(159,35,2,194,HEX(0x9CC4AE));gfx_rrect_outline(127,99,66,66,33,2,HEX(0x9CC4AE),256);
  gfx_fill(4,103,12,56,dark[0]);gfx_fill(304,103,12,56,dark[1]);
  for(int p=0;p<2;p++){int x=party.p[p].x,y=party.p[p].y;disc(x+1,y+2,11,HEX(0x18352F));disc(x,y,11,team[p]);disc(x-2,y-3,4,light[p]);}
  disc(party.x+1,party.y+2,7,HEX(0x18352F));disc(party.x,party.y,7,WHITE);disc(party.x,party.y,3,INK);
  time_text(51,ago(party.started),45000);
}

/* ------------------------------------------------------------ Volley Ball */
static void volley_start(void) {
  party_start();for(int p=0;p<2;p++)party.p[p].x=75+p*170,party.p[p].y=208;
  int serve=winner<2?1-winner:rn(2);party.x=serve?242:78;party.y=75;party.vx=serve?-60:60;
}
static void volley_step(void) {
  if(phase)return;
  float dt=seconds();
  if(cpu){bot_input();if(bot_due()){
    /* Get behind the ball to return it over the net, with time to react. */
    float target=party.x>152?party.x+14+party.vx*(cpu==1?0:cpu==2?.06f:.12f):240;
    bot_to(clampf(target,180,292),208,5);pad[1]&=B_LEFT|B_RIGHT;
    if(party.x>161&&party.y>120&&party.y<183&&__builtin_fabsf(party.x-party.p[1].x)<30)tap[1]|=B_FIRE;
    bot.pad=pad[1];
  }}
  for(int p=0;p<2;p++){
    PartyPlayer*a=&party.p[p];a->x=clampf(a->x+dirx(p)*(is_cpu(p)?60+cpu*15:125)*dt,p?178:20,p?300:142);
    if((tap[p]&(B_UP|B_FIRE))&&a->y>=207)a->vy=-232;
    a->vy+=570*dt;a->y+=a->vy*dt;if(a->y>208)a->y=208,a->vy=0;
    float dx=party.x-a->x,dy=party.y-(a->y-12),d=len(dx,dy);
    if(d<24&&d>.01f){party.x=a->x+dx/d*24;party.y=a->y-12+dy/d*24;float v=party.vx*dx/d+(party.vy-a->vy)*dy/d;
      if(v<0){party.vx-=1.75f*v*dx/d;party.vy-=1.75f*v*dy/d;party.vy-=30;}}
  }
  float oldx=party.x,oldy=party.y;party.vy+=360*dt;party.x+=party.vx*dt;party.y+=party.vy*dt;
  party.vx=clampf(party.vx,-255,255);party.vy=clampf(party.vy,-285,360);
  if(party.x<18)party.x=18,party.vx=__builtin_fabsf(party.vx);
  if(party.x>302)party.x=302,party.vx=-__builtin_fabsf(party.vx);
  if(party.y<39)party.y=39,party.vy=__builtin_fabsf(party.vy);
  if(party.x>149&&party.x<171){
    if(oldy<=136&&party.y>=136){party.y=136;party.vy=-__builtin_fabsf(party.vy)*.85f;}
    else if(party.y>136){party.x=oldx<160?149:171;party.vx=oldx<160?-__builtin_fabsf(party.vx):__builtin_fabsf(party.vx);}
  }
  if(party.y>=211)point(party.x<160?1:0);
}
static void volley_paint(void) {
  arena(HEX(0x27385D));gfx_fill(12,221,296,9,HEX(0xB6BFE0));gfx_fill(157,145,6,76,WHITE);disc(160,145,4,WHITE);
  for(int p=0;p<2;p++){int x=party.p[p].x,y=party.p[p].y;disc(x+1,y+3,15,dark[p]);disc(x,y-3,15,team[p]);gfx_fill(x-15,y+1,30,18,team[p]);disc(x+(p?-4:4),y-7,2,WHITE);}
  disc(party.x+1,party.y+2,9,INK);disc(party.x,party.y,9,GOLD);disc(party.x-3,party.y-3,3,WHITE);
}

/* ------------------------------------------------------------ Archery */
#define ARROW_GRAVITY 240
#define BOW_Y 190
static float archery_power(unsigned held) { return 140+clampi(held,0,1000)*.14f; }
static void archery_plan(void) {
  PartyPlayer*a=&party.p[1];int angle=6+rn(5),spread=cpu==1?260:cpu==2?180:110;
  float power=__builtin_sqrtf(ARROW_GRAVITY*223.0f*127*127/(2*cos64(angle)*sin64(angle)));
  a->vx=angle;a->aux=clampi((int)((power-140)/.14f)+rn(spread*2+1)-spread,180,1000);
}
static void archery_start(void) {
  party_start();party.p[0].x=40;party.p[1].x=280;
  for(int p=0;p<2;p++){party.p[p].vx=8;party.p[p].y=0;}
  if(cpu){archery_plan();party.p[1].ready=now+(cpu==1?1600:cpu==2?1300:1000);}
}
static void archery_step(void) {
  if(phase)return;
  float dt=seconds();
  if(cpu){
    PartyPlayer*a=&party.p[1];pad[1]=tap[1]=0;
    if(reached(a->ready)&&(!party.flags[1]||ago(a->last)<(uint32_t)a->aux))pad[1]=B_FIRE;
  }
  for(int p=0;p<2;p++){
    PartyPlayer*a=&party.p[p];a->vx=clampf(a->vx-diry(p)*dt*6,2,14);
    a->y=clampf(a->y-dt,0,1); /* short hit flash, independent of the reload timer */
    if((pad[p]&B_FIRE)&&reached(a->ready)&&!party.flags[p])party.flags[p]=1,a->last=now;
    if(party.flags[p]&&!(pad[p]&B_FIRE)){
      int slot=shot_slot(),angle=(int)a->vx;float power=archery_power(ago(a->last));
      if(slot>=0)party.shot[slot]=(PartyShot){a->x+(p?-17:17),BOW_Y,
        (p?-1:1)*cos64(angle)*power/127,-sin64(angle)*power/127,4000,(int8_t)p,0};
      a->ready=now+(is_cpu(p)?(cpu==1?2000:cpu==2?1650:1350)+rn(400):600);
      party.flags[p]=0;
      if(is_cpu(p))archery_plan();
    }
  }
  for(int i=0;i<12;i++){
    PartyShot*s=&party.shot[i];if(s->life<=0)continue;
    s->life-=ms;s->vy+=ARROW_GRAVITY*dt;s->x+=s->vx*dt;s->y+=s->vy*dt;
    if(s->x<-12||s->x>332||s->y>208)s->life=0;
    int p=1-s->own;
    if(s->life>0&&__builtin_fabsf(s->x-party.p[p].x)<13&&s->y>165&&s->y<208){
      s->life=0;if(party.mark[p])party.mark[p]--;party.p[p].y=.25f;
    }
  }
  tie_dead();if(ago(party.started)>30000)point(party.mark[0]==party.mark[1]?2:party.mark[0]>party.mark[1]?0:1);
}
/* Small integer line segments keep bows, fletching and arrowheads readable
 * without a texture atlas. Only pixels in the current LCD strip are drawn. */
static void archery_line(int x,int y,int xx,int yy,color_t c) {
  int dx=xx-x,dy=yy-y,sx=dx<0?-1:1,sy=dy<0?-1:1;
  if(dx<0)dx=-dx;
  if(dy<0)dy=-dy;
  int error=dx-dy;
  for(;;){
    if(y>=gfx_y0&&y<gfx_y1)gfx_fill(x,y,1,1,c);
    if(x==xx&&y==yy)break;
    int e=error*2;if(e>-dy)error-=dy,x+=sx;if(e<dx)error+=dx,y+=sy;
  }
}
static void archery_arrow(int x,int y,float vx,float vy,color_t c) {
  float norm=len(vx,vy);if(norm<1)norm=1;
  int dx=(int)(vx/norm*10),dy=(int)(vy/norm*10);
  archery_line(x-dx,y-dy,x+dx,y+dy,WHITE);
  archery_line(x+dx,y+dy,x+dx-dx/2+dy/3,y+dy-dy/2-dx/3,c);
  archery_line(x+dx,y+dy,x+dx-dx/2-dy/3,y+dy-dy/2+dx/3,c);
  archery_line(x-dx,y-dy,x-dx+dy/3,y-dy-dx/3,c);
}
static void hearts(int x,int y,int p,int remaining) {
  for(int j=0;j<3;j++){
    int hx=x+j*14;color_t h=j<remaining?team[p]:HEX(0x404867);
    disc(hx-2,y,3,h);disc(hx+2,y,3,h);
    for(int k=0;k<5;k++)gfx_fill(hx-4+k,y+2+k,9-k*2,1,h);
  }
}
static void archery_paint(void) {
  arena(HEX(0x262D52));gfx_fill(12,209,296,4,HEX(0x7A86A8));
  for(int p=0;p<2;p++){
    PartyPlayer*a=&party.p[p];int x=a->x,d=p?-1:1,angle=(int)a->vx,ax=x+d*17;
    float power=archery_power(party.flags[p]?ago(a->last):650);
    float vx=d*cos64(angle)*power/127,vy=-sin64(angle)*power/127;
    for(int j=1;j<36;j++){
      float t=j*.06f,xx=ax+vx*t,yy=BOW_Y+vy*t+ARROW_GRAVITY*t*t*.5f;
      if(xx<17||xx>303||yy>208)break;
      if(yy>98)disc(xx,yy,1,gfx_mix(INK,light[p],24));
    }
    actor2(x,206,p);if(a->y>0)disc(x,181,8,WHITE);
    archery_line(x+d*4,190,ax,BOW_Y,light[p]);disc(ax,BOW_Y,2,light[p]);
    /* Draw the curved bow and its taut string in the actual aiming direction. */
    int cx=cos64(angle),sn=sin64(angle),ux=d*cx,uy=-sn,px=d*sn,py=cx;
    int lastx=ax-px*15/127,lasty=BOW_Y-py*15/127;
    for(int j=-2;j<=2;j++){
      int side=j*7,bulge=(4-j*j)*2;
      int xx=ax+(px*side+ux*bulge)/127,yy=BOW_Y+(py*side+uy*bulge)/127;
      archery_line(lastx,lasty,xx,yy,GOLD);lastx=xx;lasty=yy;
    }
    archery_line(ax-px*14/127,BOW_Y-py*14/127,ax+px*14/127,BOW_Y+py*14/127,light[p]);
    if(!party.flags[p]||now%180<130)archery_arrow(ax,BOW_Y,vx,vy,team[p]);
    hearts(26+p*240,48,p,party.mark[p]);
    int bx=p?226:18;box(bx,73,76,8,4,INK);
    int charge=party.flags[p]?clampi(ago(a->last),0,1000)*72/1000:0;
    if(charge)box(bx+2,75,charge,4,2,GOLD);
    gfx_text_center(&np_font_small,bx+38,68,T("Power"),light[p],256);
    char text[16];char*e=num(text,(unsigned)(angle*360/64));
    *e++=' ';*e++='d';*e++='e';*e++='g';*e=0;
    gfx_text_center(&np_font_small,bx+38,96,text,WHITE,256);
  }
  for(int i=0;i<12;i++){PartyShot*s=&party.shot[i];if(s->life>0)archery_arrow(s->x,s->y,s->vx,s->vy,light[s->own]);}
  /* HUD is painted last, so a high arrow cannot obscure its text. */
  time_text(51,ago(party.started),30000);
  centre_text(226,T("Hold OK / EXE, release to shoot"),WHITE);
}

/* ------------------------------------------------------------ Dodge Ball */
static void dodge_start(void) {party_start();party.p[0].x=64;party.p[1].x=256;party.p[0].y=party.p[1].y=130;}
static void dodge_step(void) {
  if(phase)return;
  float dt=seconds();
  if(cpu){bot_input();if(bot_due()){
    float ty=party.p[0].y,tx=254;
    for(int i=0;i<12;i++){PartyShot*s=&party.shot[i];if(s->life>0&&!s->own&&s->vx>0&&s->x>135&&__builtin_fabsf(s->y-party.p[1].y)<(cpu==1?15:28))ty=s->y<party.p[1].y?216:43;}
    bot_to(tx,ty,4);if(reached(party.p[1].ready))tap[1]=B_FIRE;
  bot.pad=pad[1];}}
  for(int p=0;p<2;p++){
    PartyPlayer*a=&party.p[p];float speed=is_cpu(p)?47+cpu*18:125;float dx=dirx(p),dy=diry(p);if(dx&&dy)dx*=.7071f,dy*=.7071f;
    a->x=clampf(a->x+dx*speed*dt,p?178:19,p?301:142);a->y=clampf(a->y+dy*speed*dt,44,218);
    if((tap[p]&B_FIRE)&&reached(a->ready)){
      int slot=shot_slot();float x=party.p[1-p].x-a->x,y=party.p[1-p].y-a->y;if(is_cpu(p))y+=rn(cpu==1?101:cpu==2?61:33)-(cpu==1?50:cpu==2?30:16);float d=len(x,y);
      if(slot>=0&&d>1)party.shot[slot]=(PartyShot){a->x,a->y,x/d*218,y/d*218,3500,(int8_t)p,0};
      a->ready=now+(is_cpu(p)?1950-cpu*300:520);
    }
  }
  for(int i=0;i<12;i++){PartyShot*s=&party.shot[i];if(s->life<=0)continue;s->life-=ms;s->x+=s->vx*dt;s->y+=s->vy*dt;
    if(s->y<37)s->y=37,s->vy=__builtin_fabsf(s->vy);
    if(s->y>225)s->y=225,s->vy=-__builtin_fabsf(s->vy);
    if(s->x<8||s->x>312)s->life=0;
    int p=1-s->own;if(s->life>0&&distance2(s->x,s->y,party.p[p].x,party.p[p].y)<sq(16)&&reached(party.p[p].last)){
      if(party.mark[p])party.mark[p]--;
      party.p[p].last=now+450;s->life=0;
    }
  }
  tie_dead();if(ago(party.started)>30000)point(party.mark[0]==party.mark[1]?2:party.mark[0]>party.mark[1]?0:1);
}
static void dodge_paint(void) {
  arena(HEX(0x293658));for(int y=33;y<230;y+=14)gfx_fill(159,y,2,8,HEX(0x7482AF));
  for(int p=0;p<2;p++){int x=party.p[p].x,y=party.p[p].y;
    if(reached(party.p[p].last)||now%160<100){disc(x+1,y+2,12,dark[p]);disc(x,y,11,team[p]);disc(x-3,y-3,4,light[p]);}
    hearts(26+p*240,40,p,party.mark[p]);
  }
  for(int i=0;i<12;i++){PartyShot*s=&party.shot[i];if(s->life>0){disc(s->x,s->y,5,team[s->own]);disc(s->x-1,s->y-1,2,WHITE);}}
  time_text(45,ago(party.started),30000);
}

/* ------------------------------------------------------------ Meteor Run */
static void meteors_start(void) {party_start();party.p[0].x=party.p[1].x=80;party.next=now+650;}
static void meteors_step(void) {
  if(phase)return;
  float dt=seconds();
  if(cpu){bot_input();if(bot_due()){float best=80;int danger[3]={0};
    for(int i=0;i<12;i++){PartyShot*s=&party.shot[i];if(s->life>0&&s->y>85&&s->y<226){for(int k=0;k<3;k++)if(__builtin_fabsf(s->x-(30+50*k))<29)danger[k]+=s->y>150?3:1;}}
    int choice=0;for(int k=1;k<3;k++)if(danger[k]<danger[choice]||(danger[k]==danger[choice]&&__builtin_fabsf(party.p[1].x-(30+50*k))<__builtin_fabsf(party.p[1].x-(30+50*choice))))choice=k;
    best=30+50*choice;if(cpu==1&&ago(party.started)%1600<550)best=80;
    bot_to(best,190,4);pad[1]&=B_LEFT|B_RIGHT;
  bot.pad=pad[1];}}
  for(int p=0;p<2;p++)if(party.mark[p])party.p[p].x=clampf(party.p[p].x+dirx(p)*(is_cpu(p)?55+cpu*20:145)*dt,23,137);
  if(reached(party.next)){
    int slot=shot_slot();if(slot>=0)party.shot[slot]=(PartyShot){25+rn(111),22,0,90+clampi(ago(party.started)/180,0,130),1,0,(int8_t)(8+rn(5))};
    party.next=now+(ago(party.started)>15000?450:650);
  }
  for(int i=0;i<12;i++){PartyShot*s=&party.shot[i];if(s->life<=0)continue;s->y+=s->vy*dt;if(s->y>250)s->life=0;
    for(int p=0;p<2;p++)if(party.mark[p]&&distance2(party.p[p].x,208,s->x,s->y)<sq(s->kind+9))party.mark[p]=0;
  }
  tie_dead();if(ago(party.started)>30000)point(2);
}
static void meteors_paint(void) {
  arena(HEX(0x1C2847));gfx_fill(158,30,4,200,HEX(0x56618C));
  for(int p=0;p<2;p++){
    for(int j=0;j<9;j++)disc(14+p*160+(j*37)%140,45+(j*29)%145,1,HEX(0x8592B7));
    if(party.mark[p]){int x=p*160+(int)party.p[p].x;box(x-9,200,18,14,5,team[p]);disc(x,199,6,light[p]);}
    for(int i=0;i<12;i++){PartyShot*s=&party.shot[i];if(s->life>0){int x=p*160+(int)s->x,y=s->y;box(x-3,y-18,6,18,3,HEX(0xBA714A));disc(x,y,s->kind,HEX(0x757F9D));disc(x-3,y-3,3,HEX(0xA3ACC5));}}
  }
  time_text(52,ago(party.started),30000);
}

/* ------------------------------------------------------------ Rocket Race */
static void rockets_start(void) {party_start();for(int p=0;p<2;p++)party.p[p].y=125;party.next=now+900;}
static void rockets_step(void) {
  if(phase)return;
  float dt=seconds();
  if(cpu){bot_input();if(bot_due()){float target=125,closest=1000;
    for(int i=0;i<12;i++){PartyShot*s=&party.shot[i];if(s->life>0&&s->x>4&&s->x<closest)closest=s->x,target=s->y;}
    if(party.p[1].y>target+(cpu==1?15:cpu==2?7:1)&&party.p[1].vy>-45&&ago(party.p[1].last)>150)tap[1]|=B_FIRE;
  bot.pad=pad[1];}}
  for(int p=0;p<2;p++)if(party.mark[p]){
    PartyPlayer*a=&party.p[p];if((tap[p]&(B_UP|B_FIRE))&&ago(a->last)>=100)a->vy=-123,a->last=now;
    a->vy=clampf(a->vy+225*dt,-160,190);a->y+=a->vy*dt;if(a->y<40||a->y>221)party.mark[p]=0;
  }
  if(reached(party.next)){int slot=shot_slot();if(slot>=0)party.shot[slot]=(PartyShot){172,88+rn(88),65+clampi(ago(party.started)/1000,0,24),0,1,0,0};party.next=now+1700;}
  for(int i=0;i<12;i++){PartyShot*s=&party.shot[i];if(s->life<=0)continue;s->x-=s->vx*dt;if(s->x<-22)s->life=0;
    if(s->x<66&&s->x+18>46)for(int p=0;p<2;p++)if(party.p[p].y<s->y-34||party.p[p].y>s->y+34)party.mark[p]=0;
  }
  tie_dead();if(ago(party.started)>30000)point(2);
}
static void rockets_paint(void) {
  arena(HEX(0x1B2441));gfx_fill(158,30,4,200,HEX(0x56618C));
  for(int p=0;p<2;p++){
    for(int i=0;i<12;i++){PartyShot*s=&party.shot[i];if(s->life>0){
      int left=clampi((int)s->x,12,148),right=clampi((int)s->x+18,12,148),g=s->y;
      if(right>left){int x=p*160+left;box(x,30,right-left,g-73,4,HEX(0x56618C));box(x,g+43,right-left,230-g-43,4,HEX(0x56618C));}
    }}
    if(party.mark[p]){
      int x=56+p*160,y=party.p[p].y;box(x-9,y-7,18,14,6,team[p]);disc(x+4,y-2,3,WHITE);
      int thrust=ago(party.p[p].last)<180?12:5;
      gfx_fill(x-8-thrust,y-3,thrust,6,GOLD);
    }
  }
  time_text(52,ago(party.started),30000);
}

/* ------------------------------------------------------------ Sumo */
static void sumo_start(void) {party_start();for(int p=0;p<2;p++)party.p[p].x=115+p*90,party.p[p].y=133;}
static float ring_radius(void) {return 96-clampf(((float)ago(party.started)-6000)*.003f,0,57);}
static void sumo_step(void) {
  if(phase)return;
  float dt=seconds(),radius=ring_radius();
  if(cpu){bot_input();if(bot_due()){float x=party.p[0].x,y=party.p[0].y;
    if(distance2(party.p[1].x,party.p[1].y,160,133)>sq(radius-18))x=160,y=133;
    bot_to(x,y,2);if(distance2(x,y,party.p[1].x,party.p[1].y)<sq(cpu==1?24:48)&&reached(party.p[1].ready))tap[1]|=B_FIRE;
  bot.pad=pad[1];}}
  for(int p=0;p<2;p++){
    PartyPlayer*a=&party.p[p];float dx=dirx(p),dy=diry(p),n=len(dx,dy);if(n>0)dx/=n,dy/=n;
    if((tap[p]&B_FIRE)&&reached(a->ready)){
      if(n==0){dx=party.p[1-p].x-a->x;dy=party.p[1-p].y-a->y;n=len(dx,dy);if(n>0)dx/=n,dy/=n;}
      a->vx+=dx*225;a->vy+=dy*225;a->ready=now+(is_cpu(p)?2200-cpu*300:1100);
    }
    a->vx+=dx*500*dt;a->vy+=dy*500*dt;float friction=1-dt*4.2f;a->vx*=friction;a->vy*=friction;
    a->x+=a->vx*dt;a->y+=a->vy*dt;
  }
  PartyPlayer*a=&party.p[0],*b=&party.p[1];float dx=b->x-a->x,dy=b->y-a->y,n=len(dx,dy);
  if(n<27){if(n<.01f)dx=1,dy=0,n=1;dx/=n;dy/=n;float shift=(27-n)*.5f;a->x-=dx*shift;a->y-=dy*shift;b->x+=dx*shift;b->y+=dy*shift;
    float v=(b->vx-a->vx)*dx+(b->vy-a->vy)*dy;if(v<0){float impulse=-v*.9f+12;a->vx-=dx*impulse;a->vy-=dy*impulse;b->vx+=dx*impulse;b->vy+=dy*impulse;}}
  for(int p=0;p<2;p++)if(distance2(party.p[p].x,party.p[p].y,160,133)>sq(radius+2))party.mark[p]=0;
  tie_dead();if(ago(party.started)>30000)point(2);
}
static void sumo_paint(void) {
  arena(HEX(0x202745));int r=ring_radius();disc(160,135,r+4,HEX(0x10162D));disc(160,133,r+3,GOLD);disc(160,133,r,HEX(0xD0D3DE));disc(160,133,4,HEX(0x8791AD));
  for(int p=0;p<2;p++){int x=party.p[p].x,y=party.p[p].y;disc(x+1,y+3,14,HEX(0x8992AA));disc(x,y,13,team[p]);disc(x-3,y-4,5,light[p]);if(reached(party.p[p].ready))disc(x+4,y+3,3,WHITE);}
}

/* ------------------------------------------------------------ Fishing */
static void fishing_start(void) {
  party_start();party.p[0].x=64;party.p[1].x=256;
  for(int p=0;p<2;p++)party.p[p].y=83,party.p[p].aux=-1;
  for(int i=0;i<7;i++)party.shot[i]=(PartyShot){25+rn(270),115+rn(90),(i&1?1:-1)*(28+rn(25)),0,1,-1,0};
}
static void fishing_step(void) {
  if(phase)return;
  float dt=seconds();
  if(cpu){bot_input();if(bot_due()){
    PartyPlayer*a=&party.p[1];float best=1e6f,target=250;int choice=0;
    for(int i=0;i<7;i++){PartyShot*s=&party.shot[i];if(s->own>=0)continue;float d=__builtin_fabsf(s->x-a->x)+(s->y-80)*.2f;if(d<best)best=d,target=clampf(s->x+s->vx*(s->y-a->y)/100,18,302),choice=i;}
    bot_to(target,190,5);pad[1]&=B_LEFT|B_RIGHT;
    if(a->aux<0&&a->y<party.shot[choice].y+9&&__builtin_fabsf(a->x-party.shot[choice].x)<(cpu==1?14:cpu==2?23:31))pad[1]|=B_FIRE;
    bot.pad=pad[1];
  }}
  for(int i=0;i<7;i++){PartyShot*s=&party.shot[i];if(s->own<0){s->x+=s->vx*dt;if(s->x<21)s->x=21,s->vx=__builtin_fabsf(s->vx);if(s->x>299)s->x=299,s->vx=-__builtin_fabsf(s->vx);}}
  bool caught[2]={false,false};
  for(int p=0;p<2;p++){
    PartyPlayer*a=&party.p[p];a->x=clampf(a->x+dirx(p)*(is_cpu(p)?60+cpu*15:115)*dt,18,302);bool lower=(pad[p]&B_FIRE)!=0;
    a->y=clampf(a->y+(lower?105:-145)*dt,83,219);
    if(a->aux>=0){PartyShot*s=&party.shot[a->aux];s->x=a->x;s->y=a->y+6;if(a->y<=83)caught[p]=true;}
  }
  for(int i=0;i<7;i++)if(party.shot[i].own<0){
    PartyShot*s=&party.shot[i];float d[2];for(int p=0;p<2;p++)d[p]=party.p[p].aux<0?distance2(party.p[p].x,party.p[p].y,s->x,s->y):1e6f;
    if(d[0]<144&&d[1]<144&&__builtin_fabsf(d[0]-d[1])<1){point(2);return;}
    int p=d[0]<d[1]?0:1;if(d[p]<144)party.p[p].aux=i,s->own=p;
  }
  if(caught[0]||caught[1])point(caught[0]&&caught[1]?2:caught[0]?0:1);
  if(ago(party.started)>30000)point(2);
}
static void fishing_paint(void) {
  arena(HEX(0x283D67));gfx_fill(12,82,296,147,HEX(0x244F75));gfx_fill(12,79,296,3,HEX(0x7FA6C3));
  for(int p=0;p<2;p++){int x=party.p[p].x,y=party.p[p].y;actor2(x,77,p);gfx_fill(x,82,1,y-82,light[p]);gfx_fill(x,y,7,2,team[p]);gfx_fill(x+6,y-5,2,7,team[p]);}
  for(int i=0;i<7;i++){PartyShot*s=&party.shot[i];int x=s->x,y=s->y;box(x-8,y-4,16,9,4,i&1?GOLD:HEX(0xA4D3DE));gfx_fill(x+(s->vx>0?-11:8),y-5,4,11,HEX(0x7B9DAF));disc(x+(s->vx>0?4:-4),y-1,1,INK);}
  for(int p=0;p<2;p++)if(!is_cpu(p))
    gfx_text_center(&np_font_small,80+p*160,47,party.p[p].aux>=0?T("Release!"):p?T("Hold EXE"):T("Hold OK"),GOLD,256);
  time_text(48,ago(party.started),30000);
}

/* ------------------------------------------------------------ Memory Duel */
static const uint8_t MEMORY_BIT[4]={B_LEFT,B_UP,B_DOWN,B_RIGHT};
static void memory_start(void) {
  party_start();party.n=(uint8_t)(3+clampi(score[0]+score[1],0,4));for(int i=0;i<party.n;i++)party.seq[i]=rn(4);
  party.signal=now+700+party.n*650;party.next=party.signal+(cpu==1?1100:cpu==2?850:600);
  party.p[1].aux=rn(100)<(cpu==1?60:cpu==2?35:15)?rn(party.n):-1;
}
static void memory_step(void) {
  if(phase||!reached(party.signal))return;
  if(cpu&&reached(party.next)&&party.index[1]<party.n&&!party.flags[1]){
    int i=party.index[1],d=party.seq[i];if(i==party.p[1].aux)d=(d+1)%4;tap[1]=MEMORY_BIT[d];party.next=now+(cpu==1?1100:cpu==2?850:600);
  }
  for(int p=0;p<2;p++)if(!party.flags[p]&&party.index[p]<party.n){
    unsigned bits=tap[p]&15;if(!bits)continue;int d=0;while(!(bits&MEMORY_BIT[d]))d++;
    if((bits&(bits-1))||d!=party.seq[party.index[p]])party.flags[p]=1;
    else party.index[p]++;
  }
  if(party.flags[0]||party.flags[1])point(party.flags[0]&&party.flags[1]?2:party.flags[0]?1:0);
  else if(party.index[0]==party.n||party.index[1]==party.n)point(party.index[0]==party.n&&party.index[1]==party.n?2:party.index[0]==party.n?0:1);
  if(ago(party.signal)>15000&&reached(party.signal))point(2);
}
static void memory_arrow(int x,int y,int d,color_t c) {
  static const int8_t dx[4]={-1,0,0,1},dy[4]={0,-1,1,0};int xx=x+dx[d]*20,yy=y+dy[d]*20;
  if(dx[d]){gfx_fill(xx-7,yy-2,14,5,c);for(int i=0;i<7;i++)gfx_fill(xx+(dx[d]>0?7-i:-7+i),yy-i,1,2*i+1,c);}
  else{gfx_fill(xx-2,yy-7,5,14,c);for(int i=0;i<7;i++)gfx_fill(xx-i,yy+(dy[d]>0?7-i:-7+i),2*i+1,1,c);}
}
static void memory_paint(void) {
  arena(HEX(0x262D52));bool replay=reached(party.signal);centre_text(54,replay?T("Repeat the arrows!"):T("Watch the arrows"),WHITE);
  int elapsed=ago(party.started),shown=elapsed<700?-1:(elapsed-700)/650;
  for(int p=0;p<2;p++){
    int x=80+p*160;box(x-61,76,122,123,14,INK);
    for(int d=0;d<4;d++)memory_arrow(x,137,d,!replay&&shown<party.n&&shown>=0&&(elapsed-700)%650<470&&party.seq[shown]==d?GOLD:team[p]);
    for(int i=0;i<party.n;i++)disc(x-((party.n-1)*6)+i*12,217,4,replay?(i<party.index[p]?light[p]:dark[p]):(i==shown?GOLD:HEX(0x56618C)));
  }
}
#endif
