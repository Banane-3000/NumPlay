#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#define NP_MODULE 1
#define main twoplayer_appmain
#include "../src/main.c"
#undef main

static uint16_t pixels[320*240];
static uint8_t saved[SAVE_BYTES];
static unsigned saves;
uint64_t eadk_timing_millis(void){return now;}
uint32_t eadk_random(void){return 1;}
uint64_t eadk_keyboard_scan(void){return 0;}
void eadk_display_wait_for_vblank(void){}
void eadk_timing_msleep(uint32_t n){(void)n;}
void*np_alloc(uint32_t n){return malloc(n);}
bool np_file_write(const char*name,const void*d,uint32_t n){assert(!strcmp(name,SAVE_NAME)&&n==SAVE_BYTES);memcpy(saved,d,n);saves++;return true;}
const uint8_t*np_file_read(const char*name,uint32_t*n){*n=0;return NULL;}
void np_push(int x,int y,int w,int h,const color_t*d){assert(x>=0&&y>=0&&x+w<=320&&y+h<=240);for(int i=0;i<h;i++)memcpy(pixels+(y+i)*320+x,d+i*w,w*2);}
static void shot(const char*name){FILE*f=fopen(name,"wb");assert(f);fprintf(f,"P6\n320 240\n255\n");for(unsigned i=0;i<320*240;i++){unsigned c=pixels[i];unsigned char p[3]={(c>>11)*255/31,((c>>5)&63)*255/63,(c&31)*255/31};fwrite(p,1,3,f);}fclose(f);}
static void init(int g,int level,unsigned random){
  game=g;cpu=level;seed=random?random:1;now=1000;ms=8;pad[0]=pad[1]=tap[0]=tap[1]=0;
  new_match();
}
static void tick(unsigned a,unsigned b){pad[0]=tap[0]=a;pad[1]=tap[1]=b;now+=8;ms=8;match_step();}
static void step(unsigned duration,unsigned a,unsigned b){for(unsigned i=0;i<duration&&screen==S_PLAY;i+=8){pad[0]=a;pad[1]=b;tap[0]=tap[1]=0;now+=8;ms=8;match_step();}}
static void saves_test(void){
  uint8_t legacy[36]={'P',1,3,2};for(unsigned i=0;i<16;i++){legacy[4+i*2]=(uint8_t)(i+50);legacy[5+i*2]=1;}
  assert(save_decode(legacy,sizeof legacy));assert(sv.game==3&&sv.cpu==2);
  for(unsigned g=0;g<4;g++)for(unsigned m=0;m<2;m++)for(unsigned p=0;p<2;p++)assert(sv.wins[g][m][p]==g*4+m*2+p+306);
  for(int g=4;g<NG;g++)for(int a=0;a<2;a++)for(int p=0;p<2;p++)assert(!sv.wins[g][a][p]);
  uint8_t v2[136]={'P',2,15,1};
  for(unsigned i=0;i<64;i++){unsigned n=1000+i;v2[4+i*2]=(uint8_t)n;v2[5+i*2]=(uint8_t)(n>>8);}
  uint32_t crc=save_crc(v2,132);for(int i=0;i<4;i++)v2[132+i]=(uint8_t)(crc>>(i*8));
  assert(save_decode(v2,sizeof v2)&&sv.game==MEMORY);
  for(int g=0;g<NG;g++)for(int m=0;m<2;m++)for(int p=0;p<2;p++)assert(sv.wins[g][m][p]==1000+(g==MEMORY?15:g)*4+m*2+p);
  FILE*old=fopen("v2.sav","wb");assert(old);fwrite(v2,1,sizeof v2,old);fclose(old);
  v2[2]=14;crc=save_crc(v2,132);for(int i=0;i<4;i++)v2[132+i]=(uint8_t)(crc>>(i*8));
  assert(save_decode(v2,sizeof v2)&&sv.game==PONG);
  game=MEMORY;cpu=3;uint8_t bytes[SAVE_BYTES];save_encode(bytes);assert(save_decode(bytes,sizeof bytes));assert(sv.game==MEMORY&&sv.cpu==3);
  for(unsigned i=0;i<SAVE_BYTES;i++)for(unsigned bit=0;bit<8;bit++){
    uint8_t bad[SAVE_BYTES];memcpy(bad,bytes,sizeof bad);bad[i]^=1u<<bit;
    unsigned char before[sizeof sv];memcpy(before,&sv,sizeof sv);assert(!save_decode(bad,sizeof bad));assert(!memcmp(before,&sv,sizeof sv));
  }
  assert(!save_decode(legacy,sizeof legacy-1));assert(!save_decode(NULL,0));
  FILE*f=fopen("legacy.sav","wb");assert(f);fwrite(legacy,1,sizeof legacy,f);fclose(f);
}
static void mechanics(void){
  init(QUICK,0,1);tick(B_FIRE,0);assert(phase==1&&winner==1);
  init(QUICK,0,1);now=party.signal;tick(B_FIRE,B_FIRE);assert(winner==2&&score[0]==0&&score[1]==0);
  init(QUICK,0,1);now=party.signal;tick(0,B_FIRE);assert(winner==1);
  init(QUICK,1,1);now=party.signal;tick(B_FIRE,B_FIRE);assert(winner==0); /* solo EXE alias */
  init(TUG,0,1);tick(B_FIRE,B_FIRE);assert(fabsf(party.value)<.001f);tick(B_FIRE,0);assert(fabsf(party.value)<.001f);step(120,0,0);tick(B_FIRE,0);assert(party.value<0);
  init(FOOTBALL,0,1);party.x=9;party.y=131;tick(0,0);assert(winner==1&&phase==1);
  init(FOOTBALL,0,1);party.x=17;party.y=70;party.vx=-100;tick(0,0);assert(!phase&&party.vx>0);
  init(FOOTBALL,0,1);party.x=party.p[0].x+14;party.y=party.p[0].y;party.vx=-100;tick(0,0);assert(party.vx>0);
  init(VOLLEY,0,1);party.x=35;party.y=212;tick(0,0);assert(winner==1&&phase==1);
  init(VOLLEY,0,1);party.x=160;party.y=135;party.vy=180;tick(0,0);assert(!phase&&party.vy<0);
  init(ARCHERY,0,1);tick(B_FIRE,0);step(320,B_FIRE,0);tick(0,0);assert(party.shot[0].life>0&&party.shot[0].vx>0&&party.shot[0].vy<0);
  init(ARCHERY,0,1);party.mark[1]=1;party.shot[0]=(PartyShot){party.p[1].x,185,0,0,1000,0,0};tick(0,0);assert(winner==0&&phase==1);
  init(DODGE,0,1);party.mark[0]=party.mark[1]=1;for(int p=0;p<2;p++)party.shot[p]=(PartyShot){party.p[p].x,party.p[p].y,0,0,1000,1-p,0};tick(0,0);assert(winner==2);
  init(METEORS,0,1);party.shot[0]=(PartyShot){80,207,0,0,1,0,10};tick(0,0);assert(winner==2&&party.mark[0]==0&&party.mark[1]==0);
  init(ROCKETS,0,1);party.p[0].y=222;party.p[1].y=120;tick(0,0);assert(winner==1);
  init(SUMO,0,1);party.p[0].x=10;tick(0,0);assert(winner==1);
  init(SUMO,0,1);now+=20000;assert(ring_radius()<55);tick(B_FIRE,0);assert(party.p[0].ready>now);
  init(FISHING,0,1);party.p[0].y=160;step(80,B_FIRE,0);assert(party.p[0].y>160);step(80,0,0);assert(party.p[0].y<160+8);
  init(FISHING,0,1);party.p[0].x=party.p[1].x=100;party.p[0].y=party.p[1].y=150;party.shot[0]=(PartyShot){100,150,0,0,1,-1,0};tick(B_FIRE,B_FIRE);assert(winner==2&&phase==1);
  init(MEMORY,0,1);tick(MEMORY_BIT[(party.seq[0]+1)%4],0);assert(!phase&&!party.index[0]);now=party.signal;
  for(int i=0;i<party.n;i++){tick(MEMORY_BIT[party.seq[i]],0);assert(!party.flags[0]);}assert(phase==1&&winner==0);
  init(MEMORY,0,1);now=party.signal;tick(MEMORY_BIT[(party.seq[0]+1)%4],MEMORY_BIT[(party.seq[0]+1)%4]);assert(winner==2);
}
static void balance_test(void){
  assert(KEYS[0][4]==eadk_key_ok&&KEYS[1][4]==eadk_key_exe);
  for(int g=0;g<NG;g++){game=g;for(int d=-1;d<=1;d+=2){int n=page_game(d);assert(n>=0&&n<NG);}}
  unsigned quick_wins[3]={0},archery_wins[3]={0};
  for(int level=1;level<=3;level++){
    init(PONG,level,1);int y=py[1];step(160,0,0);assert(py[1]==y); /* no instant reaction */
    init(FOOTBALL,level,1);float x=party.p[1].x;step(160,0,0);assert(party.p[1].x==x);
    init(QUICK,level,1);assert(party.next-party.signal>=(uint32_t)(level==1?700:level==2?500:350));
    init(TUG,level,1);for(int t=0;t<16000&&!phase;t+=8)tick(t%200==0?B_FIRE:0,0);assert(phase&&winner==0);
    for(int run=0;run<20;run++){
      init(QUICK,level,run+750);for(int t=0;t<300000&&screen==S_PLAY;t+=8)tick(!phase&&reached(party.signal+420)?B_FIRE:0,0);
      assert(screen==S_OVER);if(score[0]>score[1])quick_wins[level-1]++;
      init(ARCHERY,level,run+900);
      for(int t=0;t<300000&&screen==S_PLAY;t+=8)tick(!phase&&ago(party.started)%1800<650?B_FIRE:0,0);
      assert(screen==S_OVER);if(score[0]>score[1])archery_wins[level-1]++;
    }
    assert(quick_wins[level-1]>=(level<3?19:3));assert(archery_wins[level-1]>=12);
  }
  FILE*f=fopen("balance-report.json","w");assert(f);
  fprintf(f,"{\"matches_per_difficulty\":20,\"quick_human_420ms_wins\":[%u,%u,%u],\"archery_human_650ms_charge_wins\":[%u,%u,%u]}\n",quick_wins[0],quick_wins[1],quick_wins[2],archery_wins[0],archery_wins[1],archery_wins[2]);fclose(f);
}
static void validate(void){
  assert(game>=0&&game<NG&&score[0]>=0&&score[1]>=0&&score[0]<=TARGET[game]&&score[1]<=TARGET[game]);
  if(game>=QUICK){
    assert(isfinite(party.x)&&isfinite(party.y)&&isfinite(party.value));
    for(int p=0;p<2;p++)assert(isfinite(party.p[p].x)&&isfinite(party.p[p].y)&&isfinite(party.p[p].vx)&&isfinite(party.p[p].vy));
    for(int i=0;i<12;i++)assert(isfinite(party.shot[i].x)&&isfinite(party.shot[i].y)&&isfinite(party.shot[i].vx)&&isfinite(party.shot[i].vy));
  }
}
static void playtests(void){
  FILE*f=fopen("host-report.json","w");assert(f);fprintf(f,"{\"games\":%d,\"party_ram_bytes\":%u,\"save_bytes\":%u,\"ai_runs\":[",NG,(unsigned)sizeof party,SAVE_BYTES);
  unsigned runs=0;
  for(int g=QUICK;g<NG;g++)for(int level=1;level<=3;level++){
    init(g,level,1300+g*19+level);unsigned t=0;
    for(;t<480000&&screen==S_PLAY;t+=8){tick(0,0);validate();}
    fprintf(f,"%s{\"game\":%d,\"cpu\":%d,\"elapsed_ms\":%u,\"score\":[%d,%d],\"complete\":%s}",runs++?",":"",g,level,t,score[0],score[1],screen==S_OVER?"true":"false");
    if(screen!=S_OVER){fprintf(stderr,"CPU game %d level%d stalled, score %d-%d at %u\n",g,level,score[0],score[1],t);abort();}
    assert(saves&&saved[0]=='P'&&saved[1]==3);
  }
  fprintf(f,"],\"random_runs\":150,\"screen\":%u,\"game\":%u,\"cpu\":%u,\"now\":%u,\"phase\":%u,\"score\":%u,\"sv\":%u,\"sv_size\":%u}\n",(unsigned)sizeof screen,(unsigned)sizeof game,(unsigned)sizeof cpu,(unsigned)sizeof now,(unsigned)sizeof phase,(unsigned)sizeof score,(unsigned)sizeof sv,(unsigned)sizeof sv);fclose(f);
  for(int g=0;g<NG;g++)for(int run=0;run<10;run++){
    init(g,run%4,g*1001+run+1);unsigned p0=0,p1=0;
    for(int t=0;t<12000&&screen==S_PLAY;t+=8){unsigned a=p0,b=p1;if(t%88==0)p0=rnd()&31,p1=rnd()&31;pad[0]=p0;pad[1]=p1;tap[0]=p0&~a;tap[1]=p1&~b;now+=8;ms=8;match_step();validate();}
  }
}
static void renders(void){
  memset(sv.wins,0,sizeof sv.wins);
  for(int g=0;g<NG;g++){
    init(g,0,g+1);char path[80];screen=S_MENU;gfx_render(scene,0,0,240);if(g%4==0){sprintf(path,"menu-%d.ppm",g/4+1);shot(path);}
    screen=S_RULES;gfx_render(scene,0,0,240);sprintf(path,"rules-%02d.ppm",g);shot(path);
    int end=gfx_paragraph(&np_font_small,160,52,284,11,RULES[g],WHITE,256);if(end>100){fprintf(stderr,"rules too tall %d: %d\n",g,end);abort();}
    screen=S_PLAY;step(g==ARCHERY?650:850,g==ARCHERY?B_FIRE:0,0);gfx_render(scene,0,0,240);sprintf(path,"game-%02d.ppm",g);shot(path);
    for(int s=S_PAUSE;s<=S_SAVE_ERROR;s++){screen=s;sel=0;gfx_render(scene,0,0,240);}
  }
}
int main(void){
  assert(offsetof(__typeof__(party),signal)==328); /* ARM debugger fixture */
  gfx_buf=strip;static const uint32_t colours[2]={0xEF4136,0x2E7CF6};
  for(int p=0;p<2;p++)team[p]=gfx_rgb(colours[p]),light[p]=gfx_rgb(gfx_lerp888(colours[p],0xFFFFFF,120)),dark[p]=gfx_rgb(gfx_lerp888(colours[p],0,110));
  saves_test();mechanics();balance_test();playtests();renders();
  printf("PASS: fifteen games; 33 complete AI matches, 150 random runs, mechanics, legacy/CRC saves and all screens.\n");return 0;
}
