/* UNO for NumPlay. Fixed-size state, shared sprites, strip rendering. */
#include <eadk.h>
#include <string.h>
#include "../../../launcher/src/gfx.h"
#include "uno.h"
#include "save.h"
#include "assets.h"
#if NP_MODULE
#define np_app_begin()
#define np_app_end() 0
#else
#include "../../common/epsilon_app.h"
#include "../../common/epsilon_files.h"
#define np_file_read ef_read
#define np_file_write ef_write
const char eadk_app_name[] __attribute__((section(".rodata.eadk_app_name")))="UNO";
const uint32_t eadk_api_level __attribute__((section(".rodata.eadk_api_level")))=0;
void np_push(int x,int y,int w,int h,const color_t*p){eadk_display_push_rect((eadk_rect_t){x,y,w,h},p);}
#endif
#define WHITE 0xffff
#define INK HEX(0x0B2925)
#define GOLD HEX(0xFFD75B)
#define MUTED HEX(0xBED2C7)
#define SAVE_NAME "uno.sav"
enum { S_MENU,S_SETTINGS,S_PLAY,S_PAUSE,S_HELP,S_CREDITS,S_REPLACE,S_EXIT,S_SAVE_ERROR,S_EVIDENCE,S_TABLE_INFO };
static UnoSave sv;
#define G (&sv.game)
static color_t strip[SCREEN_W*STRIP_H];
static int screen,sel,selected,action,color_sel,help_page,return_to,replace_to;
static int setup_page;
static uint32_t clock_ms,bot_at,last_action;
static bool redraw,dirty,reported,exit_after_save;
static bool attempted_uno;
static char message[84];
static uint8_t evidence[108],evidence_count,evidence_color;
static uint32_t ui_random=1;
static const char*const WHO[4]={"You","Bot 1","Bot 2","Bot 3"};
static const char*const COLORS[4]={"Red","Yellow","Green","Blue"};
static const color_t COLOR[4]={HEX(0xE84D50),HEX(0xEAC643),HEX(0x33B878),HEX(0x4D9DE0)};
static bool action_available(int i);
static uint32_t random_ui(void){ui_random^=ui_random<<13;ui_random^=ui_random>>17;return ui_random^=ui_random<<5;}
static char* number(char*p,unsigned v){char a[10];int n=0;do{a[n++]='0'+v%10;}while((v/=10));while(n)*p++=a[--n];*p=0;return p;}
static char* text(char*p,const char*s){while(*s)*p++=*s++;*p=0;return p;}
static void box(int x,int y,int w,int h,color_t c){gfx_rrect(x,y,w,h,7,c,256);}
static void label(int x,int y,const char*s,color_t c){gfx_text(&np_font_small,x,y,s,c,256);}
static void center(int x,int y,const char*s,color_t c){gfx_text_center(&np_font_body,x,y,s,c,256);}
static void title(int x,int y,const char*s){gfx_text_center(&np_font_title,x,y,s,WHITE,256);}
static void background(void){
  for(int y=gfx_y0;y<gfx_y1;y++){
    int sy=y/10,ny=NP_MIN(23,sy+1),fy=y%10*32/10;color_t row[32];
    for(int x=0;x<32;x++)row[x]=gfx_mix(table_pixels[sy*32+x],table_pixels[ny*32+x],fy);
    color_t*out=gfx_buf+(y-gfx_y0)*320;
    for(int x=0;x<31;x++)for(int i=0;i<10;i++)*out++=gfx_mix(row[x],row[x+1],i*32/10);
    for(int i=0;i<10;i++)*out++=row[31];
  }
}
/* Row RLE is decoded only for the current LCD strip. One 44-byte row is
 * enough, even for the enlarged menu cards; transparency is palette 31. */
static void card(int id,int dx,int dy,int w,int h){
  if(id<0||id>54||dy>=gfx_y1||dy+h<=gfx_y0||dx>=320||dx+w<=0)return;
  const uint8_t*p=card_pixels+card_offsets[id];uint8_t row[CARD_W];
  int first=NP_MAX(dy,gfx_y0),last=NP_MIN(dy+h,gfx_y1),sy=-1;
  for(int y=first;y<last;y++){
    int target=(y-dy)*CARD_H/h;
    while(sy<target){int n=*p++;sy++;if(sy==target){int x=0;for(int i=0;i<n;i++){int t=p[i],run=(t>>5)+1;while(run--)row[x++]=t&31;}}p+=n;}
    for(int x=NP_MAX(0,dx);x<NP_MIN(320,dx+w);x++){
      int v=row[(x-dx)*CARD_W/w];if(v!=31)gfx_buf[(y-gfx_y0)*320+x]=card_palette[v];
    }
  }
}
static void button(int x,int y,int w,const char*s,bool focus){
  box(x,y,w,26,focus?GOLD:INK);gfx_text_center(&np_font_body,x+w/2,y+18,s,focus?INK:WHITE,256);
}
static void name_card(int c,char*s){
  char*p=s;if(c<52){p=text(p,COLORS[c/13]);p=text(p," ");}
  int v=uno_value(c);
  if(v<10)number(p,v);else text(p,v==UNO_SKIP?"Skip":v==UNO_REVERSE?"Reverse":v==UNO_DRAW2?"+2":c==UNO_WILD?"Wild":"Wild +4");
}
static void set_message(const char*s){text(message,s);}
static bool save(void){
  uint8_t b[UNO_SAVE_BYTES];save_encode(&sv,b);bool ok=np_file_write(SAVE_NAME,b,sizeof b);if(ok)dirty=false;return ok;
}
static void notice(void){
  Uno*g=G;char*p=message;int e=g->event;
  if(e==E_PLAY)attempted_uno=false;
  if(e==E_START){text(p,"Seven cards. Match a color or symbol.");}
  else if(e==E_STALEMATE)text(p,"No cards can move. Redeal this round.");
  else{
    p=text(p,WHO[g->actor%4]);
    if(e==E_PLAY){p=text(p," played ");name_card(g->card,p);}
    else if(e==E_COLOR){p=text(p," chose ");text(p,COLORS[g->color&3]);}
    else if(e==E_DRAW||e==E_PENALTY){p=text(p," drew ");p=number(p,g->amount);text(p,g->amount==1?" card":" cards");}
    else if(e==E_PASS)text(p," kept the card");
    else if(e==E_UNO)text(p,g->count[g->actor]==2?": UNO call ready":" called UNO!");
    else if(e==E_CAUGHT)text(p," forgot UNO: draw 2");
    else if(e==E_CHALLENGE_WON)text(p,": challenge won! Offender draws 4");
    else if(e==E_CHALLENGE_LOST)text(p,": challenge lost! Draw 6");
    else text(p," won the game!");
  }
  dirty=redraw=true;last_action=clock_ms;bot_at=clock_ms+1300;
  if(g->uno_player==0)bot_at=clock_ms+2200;
  selected=NP_CLAMP(selected,0,(int)g->count[0]-1);if(selected<0)selected=0;
  if(g->turn==0&&g->phase==U_DRAWN){selected=g->drawn;action=-1;}
  if(action>=0&&!action_available(action))action=-1;
  if(g->phase>=U_ROUND&&!reported){
    reported=true;sel=0;if(g->phase==U_MATCH){if(sv.played<65535)sv.played++;if(g->winner==0&&sv.wins<65535)sv.wins++;}
  }
}
static void start_game(UnoRules rules){
  uno_new(G,rules,random_ui());sv.active=1;reported=false;attempted_uno=false;screen=S_PLAY;selected=0;action=-1;color_sel=0;notice();
}
static void new_game(void){start_game(sv.prefs);}
static void next_round(void){uno_next_round(G);reported=false;selected=0;action=-1;color_sel=0;notice();}
static void panel(const char*heading){gfx_fill_alpha(0,0,320,240,0,140);box(15,18,290,204,INK);title(160,48,heading);}
static const char*const HELP_TITLE[7]={"Your turn","Action cards","Wild +4 challenge","Call UNO!","Winning","House rules","Controls"};
static const char*const HELP[7]={
  "Match the top card's color, number or symbol. Wilds choose a color. You may draw instead of playing. After drawing, only that new card can be played; you may also keep it. Seven cards each.",
  "+2: the next player draws two and loses a turn. Skip: skip the next player. Reverse: change direction. With two players, Reverse acts like Skip. Official rules never stack penalties.",
  "A +4 is legal only if you have no card of the active color. You may bluff. Its target may challenge: a guilty offender draws 4; an innocent offender makes the challenger draw 6. The hand is revealed on a challenge.",
  "Press 0 to call UNO before playing your next-to-last card, or right after. Forget it and another player can catch you for two cards, until the next player draws or plays. Press 0 to catch a bot that forgot.",
  "The first player with no cards left wins the game. A final +2 or +4 is still resolved, including a possible +4 challenge. After the result, choose Play again for another game with the same table rules, or return to the menu.",
  "Options are separate from classic rules. Stack only +2 on +2, or +4 on +4. Stacked +4 has no challenge. No stacking after a last card. You may also enable drawing until playable, forced play, or automatic UNO.",
  "Left / Right: select a card. OK / EXE: play. Up: action buttons; Down: hand. 1: draw or accept penalty. 2: keep drawn card. 0: call UNO or catch. Back: pause. Home / Power: save and quit."
};
static const char*const VARIANT[5]={"Stack +2","Stack +4","Draw until playable","Force drawn play","Automatic UNO"};
static const char*const VARIANT_HELP[6]={"Restore classic card rules; all variants off.",
  "Pass accumulated +2 cards to the next player.","Pass +4 cards on. Challenges are disabled.",
  "Keep drawing until a playable card appears.","A playable drawn card must be played.","Call UNO automatically for every player."};
static bool custom_rules(const UnoRules*r){return r->flags!=0;}
static void setting_value(int row,char*s){
  const UnoRules*r=&sv.prefs;
  if(row==0){number(s,r->players);text(s+1," players");}
  else text(s,(const char*const[]){"Casual","Normal","Clever"}[r->difficulty]);
}
static void settings_change(int dx){
  UnoRules*r=&sv.prefs;
  if(setup_page==0){
    if(sel==0)r->players=2+(r->players-2+dx+3)%3;
    else if(sel==1)r->difficulty=(r->difficulty+dx+3)%3;
  }else if(setup_page==1){
    if(sel==0)r->flags=0;
    else if(sel<=5)r->flags^=1u<<(sel-1);
  }
  dirty=true;
}
static void setup_seats(int y){
  for(int p=0;p<4;p++){
    int x=12+p*76;bool active=p<sv.prefs.players;
    box(x,y,68,55,INK);
    if(active)gfx_rrect_outline(x,y,68,55,7,1,p?MUTED:GOLD,256);
    if(active&&p){card(54,x+24,y+5,16,23);card(54,x+29,y+7,16,23);}
    else if(active){gfx_circle(x+34,y+13,6,GOLD,256);gfx_rrect(x+23,y+20,22,10,5,GOLD,256);}
    else gfx_rrect_outline(x+25,y+8,18,24,4,1,MUTED,100);
    gfx_text_center(&np_font_small,x+34,y+45,active?WHO[p]:"Empty",active?WHITE:MUTED,active?256:150);
  }
}
static void toggle(int x,int y,bool on,bool focus){
  box(x,y,43,16,on?GOLD:HEX(0x37544B));
  gfx_circle(x+(on?34:9),y+8,5,on?INK:MUTED,256);
  label(x+(on?5:17),y+12,on?"ON":"OFF",on?INK:WHITE);
  if(focus)gfx_rrect_outline(x-2,y-2,47,20,8,1,WHITE,256);
}
static void setup_scene(void){
  char s[90];title(160,23,"Create table");
  const char*const steps[3]={"1 Players","2 Rules","3 Ready"};
  for(int i=0;i<3;i++){
    box(12+i*102,30,92,20,i==setup_page?GOLD:INK);
    gfx_text_center(&np_font_small,58+i*102,44,steps[i],i==setup_page?INK:MUTED,256);
  }
  if(setup_page==0){
    setup_seats(57);
    for(int i=0;i<2;i++){
      int y=116+i*28;bool focus=sel==i;color_t c=focus?INK:WHITE;
      box(12,y,296,23,focus?GOLD:INK);label(22,y+16,i?"Bot level":"Players",c);
      if(i==0){
        for(int n=2;n<=4;n++){
          int x=178+(n-2)*37;bool chosen=n==sv.prefs.players;
          box(x,y+2,30,19,chosen?INK:focus?GOLD:HEX(0x37544B));number(s,n);
          gfx_text_center(&np_font_small,x+15,y+16,s,chosen?GOLD:c,256);
        }
      }else{setting_value(i,s);gfx_text_right(&np_font_small,285,y+16,s,c,256);label(293,y+16,">",c);}
    }
    box(12,175,296,20,INK);gfx_text_center(&np_font_small,160,189,"First empty hand wins",WHITE,256);
    button(12,201,296,"Next: rules  >",sel==2);
  }else if(setup_page==1){
    box(12,57,296,25,sel==0?GOLD:INK);
    label(22,74,custom_rules(&sv.prefs)?"Custom rules":"Classic rules",sel==0?INK:WHITE);
    gfx_text_right(&np_font_small,296,74,"OK: restore classic",sel==0?INK:MUTED,256);
    for(int i=0;i<5;i++){
      int y=85+i*20;box(12,y,296,20,INK);
      if(sel==i+1)gfx_rrect_outline(12,y,296,20,7,1,GOLD,256);
      label(23,y+14,VARIANT[i],WHITE);toggle(253,y+2,sv.prefs.flags&(1u<<i),sel==i+1);
    }
    gfx_text_center(&np_font_small,160,196,sel<6?VARIANT_HELP[sel]:"Review your table before dealing the cards.",MUTED,256);
    button(12,201,296,"Next: review table  >",sel==6);
  }else{
    setup_seats(57);
    char*p=number(s,sv.prefs.players-1);p=text(p,sv.prefs.players==2?" bot  /  ":" bots  /  ");setting_value(1,p);
    gfx_text_center(&np_font_small,160,128,s,WHITE,256);
    gfx_text_center(&np_font_body,160,151,custom_rules(&sv.prefs)?"Custom table":"Classic UNO",GOLD,256);
    const char*const short_rule[5]={"+2 stacking","+4 stacking","Draw until match","Forced draw play","Auto UNO"};
    for(int i=0;i<5;i++){
      int x=12+(i%2)*152,y=158+(i/2)*15;bool on=sv.prefs.flags&(1u<<i);
      gfx_circle(x+5,y+6,3,on?GOLD:HEX(0x537366),256);label(x+13,y+10,short_rule[i],on?WHITE:MUTED);
      label(x+123,y+10,on?"On":"Off",on?GOLD:MUTED);
    }
    button(12,201,296,"Deal cards",true);
  }
  gfx_text_center(&np_font_small,160,238,setup_page==2?"OK: start   Back: edit rules":"Up / Down: select   OK: confirm   Back: return",MUTED,256);
}
static void bot_badge(int p,int x,int y,int w){
  bool current=G->turn==p&&G->phase<U_ROUND;
  box(x,y,w,39,INK);if(current)gfx_rrect_outline(x,y,w,39,7,2,GOLD,256);
  label(x+6,y+13,WHO[p],current?GOLD:WHITE);card(54,x+6,y+18,10,15);
  char s[24];char*z=number(s,G->count[p]);text(z," cards");label(x+21,y+30,s,WHITE);
  if(G->count[p]==1)label(x+w-28,y+13,"UNO",G->uno_player==p?GOLD:MUTED);
}
static bool draw_available(void){return !G->turn&&(G->phase==U_TURN||(G->phase==U_DRAWN&&!(G->rules.flags&R_FORCE)));}
static bool uno_available(void){return G->phase<U_ROUND&&(G->uno_player!=UNO_NONE||(!G->turn&&G->count[0]==2&&(G->phase==U_TURN||G->phase==U_DRAWN)));}
static bool action_available(int i){return i==2||(i==0?draw_available():uno_available());}
static void hand_card(int i,int x,bool focus){
  int c=G->hands[0][i],y=focus?160:172,w=focus?44:38,h=focus?64:55;
  if(focus)gfx_rrect_outline(x-2,y-2,w+4,h+4,7,2,G->turn?WHITE:GOLD,256);
  card(c,x,y,w,h);
  char s[8];int v=uno_value(c);if(v<10)number(s,v);else text(s,v==10?"S":v==11?"R":v==12?"+2":c==52?"W":"+4");
  box(x+2,y+2,23,15,c<52?COLOR[c/13]:INK);label(x+5,y+13,s,c<52&&c/13==1?INK:WHITE);
  if(G->turn==0&&uno_can_play(G,i))gfx_circle(x+8,y+h-7,3,GOLD,256);
}
static void hand(void){
  Uno*g=G;int n=g->count[0],visible=NP_MIN(n,8),first=NP_CLAMP(selected-3,0,n-visible);
  int spacing=visible>1?NP_MIN(38,260/(visible-1)):0,w=visible?44+(visible-1)*spacing:0,x0=(320-w)/2;
  gfx_fill_alpha(0,158,320,70,INK,110);
  for(int j=0;j<visible;j++)if(first+j!=selected)hand_card(first+j,x0+j*spacing+(first+j>selected?6:0),false);
  if(n)hand_card(selected,x0+(selected-first)*spacing,true);
  if(first)label(3,204,"<",GOLD);if(first+visible<n)label(309,204,">",GOLD);
  char s[60];gfx_fill(0,228,320,12,INK);
  if(n)name_card(g->hands[0][selected],s);else text(s,"No cards left");label(6,238,s,WHITE);
  char*p=number(s,n?selected+1:0);p=text(p,"/");number(p,n);gfx_text_center(&np_font_small,160,238,s,MUTED,256);
  const char*hint=g->turn?"Wait for your turn":n&&uno_can_play(g,selected)?"OK: play":"Not playable";
  if(!g->turn&&g->phase==U_DRAWN&&!uno_can_play(g,selected))hint="New card only";
  if(action>=0)hint="Down: your hand";
  gfx_text_right(&np_font_small,314,238,hint,!g->turn&&n&&uno_can_play(g,selected)?GOLD:MUTED,256);
}
static void table_action(int x,int w,const char*s,int id,bool enabled){
  bool focus=action==id;box(x,136,w,21,focus?GOLD:INK);
  gfx_text_center(&np_font_small,x+w/2,151,s,focus?INK:enabled?WHITE:MUTED,256);
}
static void table(void){
  Uno*g=G;char s[60];bool human=g->turn==0&&g->phase<U_ROUND;
  bool alert=g->uno_player!=UNO_NONE&&g->phase<U_ROUND;
  gfx_fill(0,0,320,20,human||alert?GOLD:INK);color_t bar=human||alert?INK:WHITE;
  label(7,14,custom_rules(&g->rules)?"Custom":"Classic",bar);char*p;
  p=text(s,g->phase>=U_ROUND?(g->phase==U_MATCH?"GAME OVER":"REDEAL"):human?"YOUR TURN":WHO[g->turn]);
  if(!human&&g->phase<U_ROUND)text(p,"'s turn");
  if(alert)text(s,g->uno_player==0?"0: CALL UNO!":"0: CATCH UNO!");
  gfx_text_center(&np_font_body,153,15,s,bar,256);
  p=number(s,g->rules.players);text(p," players");
  gfx_text_right(&np_font_small,313,14,s,bar,256);
  if(g->rules.players==2)bot_badge(1,119,22,82);
  else if(g->rules.players==3){bot_badge(1,5,77,65);bot_badge(2,250,77,65);}
  else{bot_badge(1,5,77,65);bot_badge(2,119,22,82);bot_badge(3,250,77,65);}
  if(g->rules.players==3){gfx_text_center(&np_font_small,160,39,custom_rules(&g->rules)?"Custom table":"Classic table",MUTED,256);}
  gfx_text_center(&np_font_small,99,70,"DRAW",MUTED,256);gfx_text_center(&np_font_small,172,70,"DISCARD",MUTED,256);
  card(54,80,75,38,55);card(g->discard[g->xn-1],151,72,42,61);
  box(85,111,28,15,INK);number(s,g->dn);gfx_text_center(&np_font_small,99,123,s,WHITE,256);
  gfx_text_center(&np_font_small,222,69,"COLOR",MUTED,256);
  if(g->color<4){box(201,74,42,36,COLOR[g->color]);gfx_text_center(&np_font_small,222,96,COLORS[g->color],g->color==1?INK:WHITE,256);}
  gfx_text_center(&np_font_small,222,128,g->rules.players==2?"2 players":g->direction==1?"Clockwise":"Counter-CW",MUTED,256);
  hand();
  box(5,136,64,21,human?GOLD:INK);p=text(s,"YOU ");number(p,g->count[0]);gfx_text_center(&np_font_small,37,151,s,human?INK:MUTED,256);
  if(g->phase==U_DRAWN)text(s,g->rules.flags&R_FORCE?"Must play":"2 Keep");
  else if(g->pending){p=text(s,"1 Draw +");number(p,g->pending);}else text(s,"1 Draw");
  table_action(74,72,s,0,draw_available());
  table_action(151,72,g->uno_player!=UNO_NONE&&g->uno_player!=0?"0 Catch!":g->uno_armed==0?"UNO ready":"0 UNO!",1,uno_available());
  table_action(228,87,"Back: pause",2,true);
  if(screen==S_PLAY&&human&&g->phase==U_COLOR){
    panel("Choose a color");
    for(int i=0;i<4;i++){int x=34+i*66;box(x,84,54,58,COLOR[i]);if(i==color_sel)gfx_rrect_outline(x-3,81,60,64,9,3,WHITE,256);gfx_text_center(&np_font_small,x+27,119,COLORS[i],i==1?INK:WHITE,256);}
    center(160,180,"Left / Right, then OK",MUTED);
  }else if(screen==S_PLAY&&human&&g->phase==U_CHALLENGE){
    panel("Wild +4");gfx_paragraph(&np_font_small,160,73,255,16,"Did the offender have a card of the previous color? Challenge to reveal their hand.",WHITE,256);
    button(36,127,248,"Accept: draw 4",sel==0);button(36,159,248,"Challenge: risk drawing 6",sel==1);
    gfx_text_center(&np_font_small,160,205,"Previous color",MUTED,256);gfx_circle(246,201,6,COLOR[g->old_color&3],256);
  }else if(screen==S_PLAY&&g->phase>=U_ROUND){
    if(g->winner==UNO_NONE){
      panel("Table blocked");center(160,106,"Nobody can play or draw.",WHITE);
      button(36,171,248,"Redeal cards",true);
    }else{
      p=text(s,g->winner?WHO[g->winner]:"You");text(p,g->winner?" wins!":" win!");panel(s);
      card(g->discard[g->xn-1],138,61,44,64);
      gfx_text_center(&np_font_small,160,146,"First empty hand. Game won!",GOLD,256);
      button(36,160,248,"Play again",sel==0);button(36,190,248,"Back to menu",sel==1);
    }
  }
}
static void scene(void*unused){
  (void)unused;background();char s[90];
  if(screen==S_MENU||screen==S_SETTINGS||screen==S_REPLACE||screen==S_EXIT||screen==S_SAVE_ERROR){
    if(screen==S_MENU||screen==S_EXIT||screen==S_SAVE_ERROR){
      gfx_text(&np_font_title,24,53,"UNO",WHITE,256);label(26,75,"The classic card game",MUTED);
      card(0,239,53,47,68);card(53,222,44,47,68);card(54,206,36,47,68);
      static const char*const items[4]={"Continue","New game","How to play","Credits"};
      for(int i=0;i<4;i++)button(23,91+i*31,274,i==0&&!sv.active?"Continue  -  no saved match":items[i],sel==i);
      label(24,233,"By Bobydeluxe",MUTED);gfx_text_right(&np_font_small,297,233,"Back: exit",MUTED,256);
    }else setup_scene();
  }else if(screen==S_HELP||screen==S_CREDITS){
    title(160,34,screen==S_HELP?HELP_TITLE[help_page]:"Credits");
    box(14,48,292,161,INK);
    gfx_paragraph(&np_font_body,160,72,266,20,screen==S_HELP?HELP[help_page]:"Game made by Bobydeluxe.\nCard and table art by AlexDer.\nUNO is a Mattel game.\nUnofficial fan adaptation.",WHITE,256);
    if(screen==S_HELP){char*p=number(s,help_page+1);p=text(p," / 7   Left / Right   Back");(void)p;}else text(s,"Back: return");
    gfx_text_center(&np_font_small,160,232,s,MUTED,256);
  }else if(sv.active)table();
  if(screen==S_PAUSE){
    panel("Paused");static const char*const items[4]={"Resume","Table rules","Save to menu","Save and exit"};
    for(int i=0;i<4;i++)button(36,62+i*33,248,items[i],sel==i);
  }else if(screen==S_REPLACE){
    panel("Replace this game?");gfx_paragraph(&np_font_body,160,83,254,21,"Starting a new game replaces your current game.",WHITE,256);
    button(36,132,248,"Keep current game",sel==0);button(36,166,248,"Start new game",sel==1);
  }else if(screen==S_EXIT){
    panel("Save and exit?");button(36,90,248,"Stay",sel==0);button(36,127,248,"Save and exit",sel==1);
  }else if(screen==S_SAVE_ERROR){
    panel("Save failed");gfx_paragraph(&np_font_small,160,76,256,17,"Calculator storage is full or unavailable. Your existing files were kept.",WHITE,256);
    button(36,119,248,"Retry saving",sel==0);button(36,153,248,exit_after_save?"Exit without saving":"Return without saving",sel==1);
  }else if(screen==S_TABLE_INFO){
    panel("Table rules");
    for(int i=0;i<5;i++){int y=66+i*24;label(30,y+13,VARIANT[i],WHITE);toggle(245,y,G->rules.flags&(1u<<i),false);}
    gfx_text_center(&np_font_small,160,195,"First empty hand wins",GOLD,256);
    gfx_text_center(&np_font_small,160,206,"OK: read rules   Back: pause",WHITE,256);
  }else if(screen==S_EVIDENCE){
    panel("Challenge evidence");char*p=text(s,"Cards: ");number(p,evidence_count);label(30,73,s,WHITE);
    label(174,73,"Color:",MUTED);gfx_circle(216,69,6,COLOR[evidence_color&3],256);label(230,73,COLORS[evidence_color&3],WHITE);
    int first=NP_CLAMP(selected-2,0,NP_MAX(0,evidence_count-5));
    for(int j=0;j<5&&first+j<evidence_count;j++)card(evidence[first+j],30+j*53,85,44,64);
    if(evidence_count){name_card(evidence[selected],s);gfx_text_center(&np_font_small,160,168,s,WHITE,256);}
    gfx_text_center(&np_font_small,160,190,G->event==E_CHALLENGE_WON?"Illegal +4: offender draws 4":"Legal +4: challenger draws 6",GOLD,256);
    gfx_text_center(&np_font_small,160,211,"Left / Right: cards    OK: continue",MUTED,256);
  }
}
static bool key(uint64_t k,int id){return (k>>id)&1;}
static void uno_action(void){
  if(G->uno_player!=UNO_NONE&&G->uno_player!=0){if(uno_catch(G,0))notice();}
  else if(uno_call(G,0))notice();
  else{set_message("Call UNO when you have two cards.");redraw=true;}
}
static void draw_action(void){
  if(G->turn)return;
  if(G->phase==U_DRAWN){if(uno_keep(G))notice();}
  else if(G->phase==U_TURN||G->phase==U_CHALLENGE){if(G->pending?uno_accept(G):uno_draw(G))notice();}
}
static void start_requested(void){
  if(sv.active){replace_to=S_SETTINGS;screen=S_REPLACE;sel=0;}else new_game();
}
static void setup_input(int dx,int dy,bool ok,bool back){
  if(back){
    if(setup_page){setup_page--;sel=setup_page?6:2;}
    else{screen=S_MENU;sel=1;}
    return;
  }
  int rows=setup_page==0?3:setup_page==1?7:1;
  if(dy)sel=(sel+dy+rows)%rows;
  if(dx)settings_change(dx);
  if(ok){
    if(setup_page==2)start_requested();
    else if(sel==rows-1){setup_page++;sel=0;}
    else settings_change(1);
  }
}
static void play_input(int dx,int dy,bool ok,bool draw,bool keep,bool call){
  Uno*g=G;
  if(call){uno_action();return;}
  if(g->phase>=U_ROUND){
    if(g->phase==U_MATCH){if(dy)sel^=1;if(ok){if(sel){sv.active=0;screen=S_MENU;sel=1;dirty=true;}else start_game(g->rules);}}
    else if(ok)next_round();
    return;
  }
  if(g->turn==0&&g->phase==U_COLOR){color_sel=(color_sel+dx+4)%4;if(ok&&uno_choose_color(g,color_sel)){action=-1;notice();}return;}
  if(g->turn==0&&g->phase==U_CHALLENGE){
    if(dy)sel^=1;
    if(draw||(ok&&!sel)){uno_accept(g);notice();}
    else if(ok){
      evidence_count=g->count[g->offender];memcpy(evidence,g->hands[g->offender],evidence_count);evidence_color=g->old_color;
      uno_challenge(g);notice();selected=0;screen=S_EVIDENCE;
    }return;
  }
  if(draw){draw_action();return;}
  if(keep&&g->turn==0&&uno_keep(g)){notice();return;}
  if(dy){action=dy<0?0:-1;while(action>=0&&!action_available(action))action++;}
  if(action>=0){
    action=(action+dx+3)%3;
    while(!action_available(action))action=(action+(dx<0?2:1))%3;
    if(ok){if(action==0)draw_action();else if(action==1)uno_action();else{screen=S_PAUSE;sel=0;}}
  }else{
    int n=g->count[0];if(dx&&n){selected=(selected+dx+n)%n;last_action=clock_ms-1800;}
    if(ok&&!g->turn&&uno_play(g,selected,false)){color_sel=uno_bot_color(g,0);sel=0;notice();}
    else if(ok&&!g->turn){set_message("Match the active color, number or symbol.");redraw=true;}
  }
}
static void cpu_tick(void){
  Uno*g=G;
  if(screen!=S_PLAY||g->phase>=U_ROUND||(int32_t)(clock_ms-bot_at)<0)return;
  if(g->uno_player==0&&!attempted_uno){attempted_uno=true;if(random_ui()%100<70){uno_catch(g,1);notice();}bot_at=clock_ms+700;return;}
  if(!g->turn)return;
  if(g->uno_player!=UNO_NONE&&g->uno_player!=g->turn&&random_ui()%100<45){uno_catch(g,g->turn);notice();return;}
  uno_bot(g);notice();color_sel=uno_bot_color(g,0);sel=0;
}
int main(void){
  np_app_begin();color_t*old_strip=gfx_buf;gfx_buf=strip;
  sv.prefs=(UnoRules){4,1,0};uint32_t n=0;const uint8_t*b=np_file_read(SAVE_NAME,&n);save_decode(&sv,b,n);
  uint32_t last=(uint32_t)eadk_timing_millis();ui_random=eadk_random()^last;if(!ui_random)ui_random=1;
  screen=S_MENU;sel=sv.active?0:1;selected=0;action=-1;clock_ms=0;redraw=true;dirty=false;
  uint64_t held=eadk_keyboard_scan(),stale=held;bool quit=false;uint32_t last_repeat=0;uint64_t repeat_keys=0;
  while(!quit){
    uint32_t now=(uint32_t)eadk_timing_millis(),dt=now-last;last=now;
    if(screen==S_PLAY){clock_ms+=NP_MIN(dt,100u);}
    uint64_t k=eadk_keyboard_scan();stale&=k;k&=~stale;uint64_t hit=k&~held;held=k;
    uint64_t arrows=(1ull<<eadk_key_left)|(1ull<<eadk_key_right)|(1ull<<eadk_key_up)|(1ull<<eadk_key_down);
    if((k&arrows)!=repeat_keys){repeat_keys=k&arrows;last_repeat=now+330;}
    else if(repeat_keys&&(int32_t)(now-last_repeat)>=0){hit|=repeat_keys;last_repeat=now+110;}
    bool ok=key(hit,eadk_key_ok)||key(hit,eadk_key_exe),back=key(hit,eadk_key_back);
    int dx=key(hit,eadk_key_right)-key(hit,eadk_key_left),dy=key(hit,eadk_key_down)-key(hit,eadk_key_up),before=screen;
    if(hit)redraw=true;
    if(screen!=S_SAVE_ERROR&&(key(k,eadk_key_home)||key(k,eadk_key_on_off))){
      if(save()){quit=true;continue;}exit_after_save=true;screen=S_SAVE_ERROR;sel=0;stale|=k;ok=back=false;dx=dy=0;redraw=true;
    }
    switch(screen){
      case S_MENU:
        if(dy)sel=(sel+dy+4)%4;
        if(ok){if(sel==0&&sv.active){screen=S_PLAY;reported=G->phase>=U_ROUND;bot_at=clock_ms+1300;}
          else if(sel==1){screen=S_SETTINGS;setup_page=0;sel=0;}
          else if(sel==2){return_to=S_MENU;screen=S_HELP;help_page=0;}
          else if(sel==3){screen=S_CREDITS;return_to=S_MENU;}}
        else if(back){screen=S_EXIT;sel=0;}break;
      case S_SETTINGS:
        setup_input(dx,dy,ok,back);break;
      case S_PLAY:
        if(back){screen=S_PAUSE;sel=0;}
        else play_input(dx,dy,ok,key(hit,eadk_key_one),key(hit,eadk_key_two),key(hit,eadk_key_zero));
        cpu_tick();break;
      case S_PAUSE:
        if(dy)sel=(sel+dy+4)%4;
        if(back||(ok&&!sel)){screen=S_PLAY;bot_at=clock_ms+800;}
        else if(ok&&sel==1){screen=S_TABLE_INFO;}
        else if(ok){exit_after_save=sel==3;if(save()){if(exit_after_save)quit=true;else screen=S_MENU,sel=0;}else screen=S_SAVE_ERROR,sel=0;}
        break;
      case S_HELP:
        help_page=(help_page+dx+7)%7;if(back||ok){screen=return_to;sel=return_to==S_PAUSE?1:2;}break;
      case S_CREDITS:if(back||ok){screen=S_MENU;sel=3;}break;
      case S_TABLE_INFO:
        if(back){screen=S_PAUSE;sel=1;}else if(ok){return_to=S_TABLE_INFO;screen=S_HELP;help_page=4;}break;
      case S_REPLACE:
        if(dy)sel^=1;if(back||(ok&&!sel)){screen=replace_to;setup_page=2;sel=0;}else if(ok)new_game();break;
      case S_EXIT:
        if(dy)sel^=1;if(back||(ok&&!sel)){screen=S_MENU;sel=1;}else if(ok){if(save())quit=true;else screen=S_SAVE_ERROR,sel=0,exit_after_save=true;}break;
      case S_SAVE_ERROR:
        if(dy)sel^=1;
        if(ok){if(sel||save()){if(exit_after_save)quit=true;else screen=S_MENU,sel=0;}}break;
      case S_EVIDENCE:
        if(dx&&evidence_count)selected=(selected+dx+evidence_count)%evidence_count;
        if(ok||back){screen=S_PLAY;selected=0;bot_at=clock_ms+1300;}break;
    }
    if(screen!=before){stale|=k;redraw=true;repeat_keys=0;last=(uint32_t)eadk_timing_millis();}
    if(redraw){eadk_display_wait_for_vblank();gfx_render(scene,0,0,240);redraw=false;}
    eadk_timing_msleep(8);
  }
  gfx_buf=old_strip;return np_app_end();
}
