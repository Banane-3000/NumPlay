#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../src/uno.h"
#include "../src/save.h"
static Uno g;
static void fixture(int players,int flags,int top,int color){
  memset(&g,0,sizeof g);g.rules=(UnoRules){players,1,flags};g.direction=1;g.color=color;g.rng=12345;g.round=1;
  g.candidate=g.winner=g.offender=g.uno_player=g.uno_armed=g.drawn=UNO_NONE;
  for(int c=0;c<52;c++){g.deck[g.dn++]=c;if(c%13)g.deck[g.dn++]=c;}
  for(int i=0;i<4;i++){g.deck[g.dn++]=52;g.deck[g.dn++]=53;}
  for(int i=0;i<g.dn;i++)if(g.deck[i]==top){g.deck[i]=g.deck[--g.dn];break;}
  g.discard[g.xn++]=top;
}
static void give(int p,int c){
  int i;for(i=0;i<g.dn;i++)if(g.deck[i]==c)break;assert(i<g.dn);
  g.deck[i]=g.deck[--g.dn];g.hands[p][g.count[p]++]=c;
}
static void on_top(int c){int i;for(i=0;i<g.dn;i++)if(g.deck[i]==c)break;assert(i<g.dn);int t=g.deck[g.dn-1];g.deck[g.dn-1]=c;g.deck[i]=t;}
static void snapshot(const char*name){
  UnoSave s={0};s.prefs=g.rules;s.active=1;s.game=g;assert(uno_valid(&g));
  uint8_t b[UNO_SAVE_BYTES];save_encode(&s,b);FILE*f=fopen(name,"wb");assert(f);fwrite(b,1,sizeof b,f);fclose(f);
}
static void basic(void){
  fixture(4,0,3,0);give(0,4);give(0,16);give(0,27);give(0,52);give(0,53);
  assert(uno_can_play(&g,0)&&uno_can_play(&g,1)&&!uno_can_play(&g,2)&&uno_can_play(&g,3)&&uno_can_play(&g,4));
  assert(!uno_play(&g,-1,false)&&!uno_play(&g,9,false));assert(uno_play(&g,1,false));assert(g.turn==1&&g.color==1&&g.count[0]==4);assert(uno_valid(&g));
  for(int n=2;n<=4;n++){
    fixture(n,0,3,0);give(0,UNO_REVERSE);give(0,4);assert(uno_play(&g,0,false));assert(g.turn==(n==2?0:n-1));assert(g.direction==(n==2?1:-1));
    fixture(n,0,3,0);give(0,UNO_SKIP);give(0,4);assert(uno_play(&g,0,false));assert(g.turn==2%n);
  }
  fixture(4,0,3,0);give(0,12);give(0,4);give(1,25);give(2,30);
  assert(uno_play(&g,0,false));assert(g.turn==1&&g.pending==2&&!uno_can_play(&g,0));
  assert(uno_accept(&g));assert(g.count[1]==3&&g.turn==2&&g.pending==0&&uno_valid(&g));
}
static void draws(void){
  fixture(2,0,3,0);give(0,4);on_top(5);assert(uno_draw(&g));assert(g.phase==U_DRAWN&&g.drawn==1);
  assert(!uno_can_play(&g,0)&&uno_can_play(&g,1));assert(!uno_play(&g,0,false));assert(uno_keep(&g)&&g.turn==1&&g.count[0]==2);
  fixture(2,0,3,0);give(0,4);on_top(27);assert(uno_draw(&g)&&g.phase==U_TURN&&g.turn==1&&g.count[0]==2);
  fixture(2,R_DRAW_UNTIL,3,0);give(0,4);on_top(27);
  for(int i=0;i<g.dn-1;i++)if(g.deck[i]==5){int c=g.deck[g.dn-2];g.deck[g.dn-2]=5;g.deck[i]=c;break;}
  assert(uno_draw(&g)&&g.phase==U_DRAWN&&g.count[0]==3&&g.drawn==2);assert(uno_play(&g,2,false));assert(g.turn==1);
  fixture(2,R_FORCE,3,0);give(0,4);on_top(5);assert(uno_draw(&g)&&g.turn==1&&g.count[0]==1);assert(!uno_keep(&g));
  fixture(2,R_FORCE,3,0);give(0,4);on_top(52);assert(uno_draw(&g)&&g.phase==U_DRAWN);assert(!uno_keep(&g));assert(uno_play(&g,g.drawn,false));assert(uno_choose_color(&g,2));assert(g.turn==1&&g.color==2);
  /* Every non-top discard gets recycled; the top card and hand never do. */
  fixture(4,0,3,0);while(g.dn>2)give(3,g.deck[0]);
  g.discard[g.xn++]=g.deck[--g.dn];g.discard[g.xn++]=g.deck[--g.dn];g.color=uno_color(g.discard[g.xn-1]);if(g.color==4)g.color=0;
  int top=g.discard[g.xn-1];assert(uno_draw(&g));assert(g.discard[0]==top&&g.xn==1&&uno_valid(&g));
}
static void challenge(void){
  fixture(4,0,3,0);give(0,53);give(0,4);give(1,20);
  assert(uno_play(&g,0,false)&&g.phase==U_COLOR&&g.guilty);assert(uno_choose_color(&g,1)&&g.phase==U_CHALLENGE&&g.turn==1);
  assert(uno_challenge(&g));assert(g.count[0]==5&&g.count[1]==1&&g.turn==1&&g.event==E_CHALLENGE_WON&&uno_valid(&g));
  fixture(4,0,3,0);give(0,53);give(0,16);give(1,20);
  assert(uno_play(&g,0,false)&&!g.guilty);assert(uno_choose_color(&g,2));assert(uno_challenge(&g));
  assert(g.count[1]==7&&g.turn==2&&g.event==E_CHALLENGE_LOST&&uno_valid(&g));
  fixture(2,0,3,0);give(0,53);give(0,52);assert(uno_play(&g,0,false)&&!g.guilty);
  assert(uno_choose_color(&g,3));assert(uno_accept(&g)&&g.count[1]==4&&g.turn==0);
  /* A different-colour matching symbol is allowed alongside a legal +4. */
  fixture(2,0,3,0);give(0,53);give(0,16);assert(uno_play(&g,0,false)&&!g.guilty);
}
static void variants(void){
  fixture(4,R_STACK2,3,0);give(0,12);give(0,4);give(1,25);give(1,20);give(2,38);give(2,30);
  assert(uno_play(&g,0,true)&&g.pending==2&&uno_can_play(&g,0));assert(uno_play(&g,0,true)&&g.pending==4);
  assert(uno_play(&g,0,true)&&g.pending==6&&g.turn==3);assert(uno_accept(&g)&&g.count[3]==6&&g.turn==0&&uno_valid(&g));
  fixture(4,R_STACK4,3,0);give(0,53);give(0,4);give(1,53);give(1,20);
  assert(uno_play(&g,0,true)&&uno_choose_color(&g,2));assert(g.phase==U_TURN&&g.pending==4&&!uno_challenge(&g));
  assert(uno_play(&g,0,true)&&uno_choose_color(&g,1));assert(g.pending==8);assert(uno_accept(&g)&&g.count[2]==8&&uno_valid(&g));
  fixture(4,R_AUTO_UNO,3,0);give(0,4);give(0,5);assert(uno_play(&g,0,false)&&g.uno_player==UNO_NONE);
}
static void uno_calls(void){
  fixture(4,0,3,0);give(0,4);give(0,5);give(1,6);give(1,7);
  give(2,19);give(3,32);snapshot("play.sav");
  assert(uno_play(&g,0,false)&&g.uno_player==0);assert(uno_call(&g,0)&&g.uno_player==UNO_NONE);assert(!uno_catch(&g,1));
  fixture(4,0,3,0);give(0,4);give(0,5);assert(uno_call(&g,0)&&g.uno_armed==0);assert(uno_play(&g,0,false)&&g.uno_player==UNO_NONE);
  fixture(4,0,3,0);give(0,4);give(0,5);assert(uno_play(&g,0,false));assert(uno_catch(&g,1));assert(g.count[0]==3&&!uno_catch(&g,2)&&g.turn==1);
  fixture(4,0,3,0);give(0,4);give(0,5);give(1,6);give(1,7);assert(uno_play(&g,0,false));assert(uno_play(&g,0,true));assert(!uno_catch(&g,1)&&g.count[0]==1);
}
static void wins(void){
  fixture(2,0,3,0);give(0,4);give(1,50);give(1,53);snapshot("win.sav");assert(uno_play(&g,0,true));assert(g.phase==U_MATCH&&g.winner==0&&uno_valid(&g));
  fixture(2,0,3,0);give(0,12);give(1,20);
  assert(uno_play(&g,0,true));assert(g.phase==U_MATCH&&g.count[1]==3&&g.winner==0&&uno_valid(&g));
  fixture(2,0,3,0);give(0,53);give(1,20);assert(uno_play(&g,0,true)&&uno_choose_color(&g,2));assert(g.phase==U_CHALLENGE&&g.candidate==0);
  assert(uno_challenge(&g)&&g.phase==U_MATCH&&g.count[1]==7&&g.winner==0&&uno_valid(&g));
  fixture(2,R_STACK2,3,0);give(0,12);give(1,25);assert(uno_play(&g,0,true));assert(g.phase==U_MATCH&&g.count[1]==3);
  fixture(2,0,3,0);give(0,4);give(1,0);assert(uno_play(&g,0,true)&&g.phase==U_MATCH&&g.winner==0); /* Even opponents holding only zero-valued cards. */
  fixture(4,0,3,0);give(0,4);give(1,5);assert(uno_play(&g,0,true)&&g.phase==U_MATCH);g.direction=-1;g.dealer=0;uno_next_round(&g);assert(g.dealer==1&&g.direction==1&&uno_valid(&g));
  fixture(2,0,3,0);give(0,50);give(1,4);g.turn=1;assert(uno_play(&g,0,true)&&g.phase==U_MATCH&&g.winner==1);
}
static void arm_fixtures(void){
  fixture(4,0,3,0);give(0,4);give(1,53);give(1,5);give(2,30);give(3,43);
  g.turn=1;assert(uno_play(&g,0,true)&&uno_choose_color(&g,2));g.turn=0;snapshot("challenge-guilty.sav");
  fixture(4,0,3,0);give(0,4);give(1,53);give(1,16);give(2,30);give(3,43);
  g.turn=1;assert(uno_play(&g,0,true)&&uno_choose_color(&g,2));g.turn=0;snapshot("challenge-legal.sav");
  fixture(2,0,3,0);give(0,4);give(1,6);on_top(5);snapshot("draw.sav");
  fixture(2,0,3,0);give(0,52);give(0,4);give(1,6);snapshot("wild.sav");
}
static void saves(void){
  UnoSave a={0},b={0};a.prefs=(UnoRules){4,1,0};a.active=1;a.played=19;a.wins=7;uno_new(&a.game,a.prefs,98765);
  uint8_t data[UNO_SAVE_BYTES];save_encode(&a,data);assert(save_decode(&b,data,sizeof data));assert(!memcmp(&a,&b,sizeof a));
  for(unsigned i=0;i<sizeof data;i++)for(int bit=0;bit<8;bit++){
    data[i]^=1u<<bit;UnoSave before=b;assert(!save_decode(&b,data,sizeof data));assert(!memcmp(&before,&b,sizeof b));data[i]^=1u<<bit;
  }
  assert(!save_decode(&b,data,20)&&!save_decode(&b,NULL,0));
  FILE*f=fopen("reference.sav","wb");assert(f);fwrite(data,1,sizeof data,f);fclose(f);
  a.game.turn=9;save_encode(&a,data);assert(!save_decode(&b,data,sizeof data));
  /* Real v1 save from the previous build: preserve cards, drop old scores. */
  f=fopen("../tests/legacy-save.bin","rb");assert(f);assert(fread(data,1,sizeof data,f)==sizeof data);fclose(f);
  assert(data[3]==1&&data[8]==3&&data[16]==3&&data[671]);
  assert(save_decode(&b,data,sizeof data)&&uno_valid(&b.game));
  uint8_t migrated[UNO_SAVE_BYTES];save_encode(&b,migrated);assert(migrated[3]==2&&!migrated[8]&&!migrated[16]);
  assert(!memcmp(data+17,migrated+17,654));for(int i=671;i<679;i++)assert(!migrated[i]);
  for(int i=681;i<683;i++)assert(!migrated[i]);
  f=fopen("migrated.sav","wb");assert(f);fwrite(migrated,1,sizeof migrated,f);fclose(f);
  /* A formerly completed round becomes a finished game, not another deal. */
  fixture(2,0,3,0);give(0,4);give(1,50);assert(uno_play(&g,0,true));a.game=g;a.prefs=g.rules;save_encode(&a,data);
  data[3]=1;data[8]=data[16]=3;data[692]=U_ROUND;data[704]=E_ROUND;
  uint32_t crc=save_crc(data,UNO_SAVE_BYTES-4);for(int i=0;i<4;i++)data[UNO_SAVE_BYTES-4+i]=crc>>(i*8);
  assert(save_decode(&b,data,sizeof data)&&b.game.phase==U_MATCH&&b.game.winner==0&&uno_valid(&b.game));
}
static void simulations(void){
  unsigned moves=0,max_hand=0,rounds=0;bool opening[54]={0};
  for(unsigned seed=1;seed<=12000;seed++){
    UnoRules r={(seed%3)+2,seed%3,0};uno_new(&g,r,seed);assert(uno_valid(&g));
    int c=g.discard[0];opening[c]=true;assert(c!=53);
    for(int p=0;p<r.players;p++)assert(g.count[p]==7||g.count[p]==9);
  }
  assert(opening[52]&&opening[12]&&opening[11]&&opening[10]);
  for(int flags=0;flags<32;flags++)for(int players=2;players<=4;players++)for(int difficulty=0;difficulty<3;difficulty++)for(int s=1;s<=4;s++){
    UnoRules r={players,difficulty,flags};uno_new(&g,r,(s*13457u+flags*17+players*13));
    unsigned step=0;
    while(g.phase!=U_MATCH&&step++<150000){
      assert(uno_valid(&g));
      for(int p=0;p<players;p++)if(g.count[p]>max_hand)max_hand=g.count[p];
      if(g.phase==U_ROUND){uno_next_round(&g);rounds++;continue;}
      if(g.uno_player!=UNO_NONE){int p=(g.uno_player+1)%players;if(step%3==0)uno_catch(&g,p);else uno_call(&g,g.uno_player);}
      uno_bot(&g);moves++;
    }
    assert(g.phase==U_MATCH&&uno_valid(&g));
  }
  printf("1,152 complete games, %u actions, %u blocked-table redeals; largest hand %u cards.\n",moves,rounds,max_hand);
}
int main(void){basic();draws();challenge();variants();uno_calls();wins();saves();arm_fixtures();simulations();puts("Engine, card rules, variants, direct wins, legacy migration and saves passed.");return 0;}
