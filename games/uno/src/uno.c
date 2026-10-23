#include "uno.h"
#include <string.h>
static uint32_t random32(Uno*g){uint32_t x=g->rng;x^=x<<13;x^=x>>17;x^=x<<5;return g->rng=x?x:1;}
static int next(const Uno*g,int p){return (p+g->rules.players+g->direction)%g->rules.players;}
static void event(Uno*g,int e,int p,int c,int n){g->event=e;g->actor=p;g->card=c;g->amount=n;}
static void shuffle(Uno*g){for(int i=g->dn-1;i>0;i--){int j=random32(g)%(i+1),c=g->deck[i];g->deck[i]=g->deck[j];g->deck[j]=c;}}
static bool take(Uno*g,int p){
  if(!g->dn&&g->xn>1){int c=g->discard[g->xn-1];g->dn=g->xn-1;memcpy(g->deck,g->discard,g->dn);g->discard[0]=c;g->xn=1;shuffle(g);}
  if(!g->dn||g->count[p]>=UNO_CARDS)return false;
  g->hands[p][g->count[p]++]=g->deck[--g->dn];return true;
}
static int take_n(Uno*g,int p,int n){int got=0;while(got<n&&take(g,p))got++;return got;}
static void begin_action(Uno*g){g->uno_player=UNO_NONE;g->uno_armed=UNO_NONE;g->turns++;}
static bool matching(const Uno*g,int c){int top=g->discard[g->xn-1];return c>=52||uno_color(c)==g->color||uno_value(c)==uno_value(top);}
static void finish(Uno*g,int w){
  g->winner=w;g->candidate=UNO_NONE;g->phase=U_MATCH;
  event(g,E_MATCH,w,0,0);
}
static void check_out(Uno*g){if(g->candidate!=UNO_NONE&&!g->pending)finish(g,g->candidate);}
void uno_next_round(Uno*g){
  if(g->round)g->dealer=(g->dealer+1)%g->rules.players;
  g->round++;g->direction=1;g->phase=U_TURN;g->dn=0;g->xn=0;g->pending=0;g->turns=0;g->stalls=0;
  g->winner=g->candidate=g->uno_player=g->uno_armed=g->drawn=UNO_NONE;
  memset(g->count,0,sizeof g->count);
  for(int c=0;c<52;c++){g->deck[g->dn++]=c;if(c%13)g->deck[g->dn++]=c;}
  for(int i=0;i<4;i++){g->deck[g->dn++]=UNO_WILD;g->deck[g->dn++]=UNO_DRAW4;}
  shuffle(g);
  for(int n=0;n<7;n++)for(int p=0;p<g->rules.players;p++)take(g,(g->dealer+1+p)%g->rules.players);
  /* Opening +4 goes back in the deck; no random-retry loop. */
  int at=g->dn-1;while(at>=0&&g->deck[at]==UNO_DRAW4)at--;
  bool rejected=at!=g->dn-1;
  int c=g->deck[at];g->deck[at]=g->deck[g->dn-1];g->dn--;g->discard[g->xn++]=c;
  if(rejected)shuffle(g);
  g->color=uno_color(c);g->turn=next(g,g->dealer);
  int v=uno_value(c);
  if(v==UNO_REVERSE){g->direction=-1;g->turn=g->dealer;}
  else if(v==UNO_SKIP)g->turn=next(g,g->turn);
  else if(v==UNO_DRAW2){take_n(g,g->turn,2);g->turn=next(g,g->turn);}
  else if(c==UNO_WILD)g->phase=U_COLOR;
  event(g,E_START,g->turn,c,0);g->offender=UNO_NONE;
}
void uno_new(Uno*g,UnoRules r,uint32_t seed){
  memset(g,0,sizeof *g);g->rules=r;g->rules.players=r.players<2?2:r.players>4?4:r.players;
  g->rules.difficulty=r.difficulty%3;g->rules.flags=r.flags&31;g->rng=seed?seed:1;g->direction=1;
  /* Official dealer selection: each player draws, highest number (actions=0).
   * Ties redraw only among tied players, from the same finite shuffled deck. */
  uint8_t pool[UNO_CARDS];int n=0;
  for(int c=0;c<52;c++){pool[n++]=c;if(c%13)pool[n++]=c;}
  for(int i=0;i<4;i++){pool[n++]=UNO_WILD;pool[n++]=UNO_DRAW4;}
  for(int i=n-1;i>0;i--){int j=random32(g)%(i+1),c=pool[i];pool[i]=pool[j];pool[j]=c;}
  unsigned eligible=(1u<<g->rules.players)-1;int index=0;
  while((eligible&(eligible-1))&&index+g->rules.players<=n){
    int best=-1;unsigned tied=0;
    for(int p=0;p<g->rules.players;p++)if(eligible&(1u<<p)){
      int v=uno_value(pool[index++]);if(v>9)v=0;
      if(v>best){best=v;tied=1u<<p;}else if(v==best)tied|=1u<<p;
    }eligible=tied;
  }
  for(int p=0;p<g->rules.players;p++)if(eligible&(1u<<p)){g->dealer=p;break;}
  uno_next_round(g);
}
bool uno_can_play(const Uno*g,int i){
  if(i<0||i>=g->count[g->turn]||g->phase>=U_ROUND||g->phase==U_COLOR)return false;
  if(g->phase==U_DRAWN&&i!=g->drawn)return false;
  int c=g->hands[g->turn][i];
  if(g->pending){
    if(g->candidate!=UNO_NONE)return false;
    return (g->penalty_kind==2&&(g->rules.flags&R_STACK2)&&uno_value(c)==UNO_DRAW2)||
           (g->penalty_kind==4&&(g->rules.flags&R_STACK4)&&c==UNO_DRAW4);
  }
  return g->phase!=U_CHALLENGE&&matching(g,c);
}
static void apply(Uno*g,int c){
  int p=g->turn,v=uno_value(c);g->phase=U_TURN;
  if(v==UNO_DRAW2||c==UNO_DRAW4){
    int n=c==UNO_DRAW4?4:2;g->pending+=n;g->penalty_kind=n;g->offender=p;g->turn=next(g,p);
    if(c==UNO_DRAW4&&!(g->rules.flags&R_STACK4))g->phase=U_CHALLENGE;
    /* Last-card penalties cannot be stacked: the round has ended. +4 may
       still be challenged before the penalty and win are settled. */
    else if(g->candidate!=UNO_NONE)uno_accept(g);
  }else{
    if(v==UNO_REVERSE&&g->rules.players>2)g->direction=-g->direction;
    g->turn=next(g,p);
    if(v==UNO_SKIP||(v==UNO_REVERSE&&g->rules.players==2))g->turn=next(g,g->turn);
    check_out(g);
  }
}
bool uno_play(Uno*g,int i,bool announce){
  if(!uno_can_play(g,i))return false;
  int p=g->turn,c=g->hands[p][i];bool armed=g->uno_armed==p;
  /* A +4 may be bluffed; only a challenge consults this saved evidence. */
  g->old_color=g->color;g->guilty=0;
  if(c==UNO_DRAW4)for(int j=0;j<g->count[p];j++)if(j!=i&&uno_color(g->hands[p][j])==g->color)g->guilty=1;
  begin_action(g);g->stalls=0;
  memmove(&g->hands[p][i],&g->hands[p][i+1],g->count[p]-i-1);g->count[p]--;
  g->discard[g->xn++]=c;g->drawn=UNO_NONE;
  if(g->count[p]==1){if(!(announce||armed||(g->rules.flags&R_AUTO_UNO)))g->uno_player=p;}
  if(!g->count[p])g->candidate=p;
  event(g,E_PLAY,p,c,0);
  if(c>=52){g->phase=U_COLOR;g->offender=p;}
  else{g->color=uno_color(c);apply(g,c);}
  return true;
}
bool uno_choose_color(Uno*g,int color){
  if(g->phase!=U_COLOR||color<0||color>3)return false;
  g->color=color;
  if(g->offender==UNO_NONE){g->phase=U_TURN;event(g,E_COLOR,g->turn,0,color);}
  else {int c=g->discard[g->xn-1];event(g,E_COLOR,g->turn,c,color);apply(g,c);}
  return true;
}
bool uno_accept(Uno*g){
  if(!g->pending||g->phase==U_COLOR||g->phase>=U_ROUND)return false;
  int p=g->turn,n=g->pending;begin_action(g);int got=take_n(g,p,n);g->pending=0;g->phase=U_TURN;
  g->turn=next(g,p);event(g,E_PENALTY,p,0,got);check_out(g);return true;
}
bool uno_challenge(Uno*g){
  if(g->phase!=U_CHALLENGE)return false;
  int p=g->turn,o=g->offender;begin_action(g);g->phase=U_TURN;g->pending=0;
  if(g->guilty){int n=take_n(g,o,4);if(g->candidate==o)g->candidate=UNO_NONE;event(g,E_CHALLENGE_WON,p,0,n);}
  else{int n=take_n(g,p,6);g->turn=next(g,p);event(g,E_CHALLENGE_LOST,p,0,n);}
  check_out(g);return true;
}
bool uno_draw(Uno*g){
  if(g->phase!=U_TURN)return false;
  if(g->pending)return uno_accept(g);
  int p=g->turn;begin_action(g);int n=0;
  do {if(!take(g,p))break;n++;g->drawn=g->count[p]-1;}
  while((g->rules.flags&R_DRAW_UNTIL)&&!matching(g,g->hands[p][g->drawn]));
  event(g,E_DRAW,p,0,n);
  if(n&&matching(g,g->hands[p][g->drawn])){
    g->phase=U_DRAWN;g->stalls=0;
    if((g->rules.flags&R_FORCE)&&g->hands[p][g->drawn]<52)uno_play(g,g->drawn,false);
    /* Wild still needs the player's colour decision. */
  }else{
    g->turn=next(g,p);g->drawn=UNO_NONE;
    if(!n)g->stalls++;else g->stalls=0;
    if(g->stalls>=g->rules.players){g->phase=U_ROUND;g->winner=UNO_NONE;event(g,E_STALEMATE,p,0,0);}
  }
  return true;
}
bool uno_keep(Uno*g){
  if(g->phase!=U_DRAWN||(g->rules.flags&R_FORCE))return false;
  int p=g->turn;g->drawn=UNO_NONE;g->phase=U_TURN;g->turn=next(g,p);event(g,E_PASS,p,0,0);return true;
}
bool uno_call(Uno*g,int p){
  if(g->phase>=U_ROUND||p<0||p>=g->rules.players)return false;
  if(g->uno_player==p){g->uno_player=UNO_NONE;event(g,E_UNO,p,0,0);return true;}
  if(g->count[p]==2&&p==g->turn&&(g->phase==U_TURN||g->phase==U_DRAWN)){g->uno_armed=p;event(g,E_UNO,p,0,0);return true;}
  return false;
}
bool uno_catch(Uno*g,int p){
  int o=g->uno_player;if(o==UNO_NONE||p==o||p<0||p>=g->rules.players||g->phase>=U_ROUND)return false;
  int got=take_n(g,o,2);g->uno_player=UNO_NONE;event(g,E_CAUGHT,o,0,got);return true;
}
int uno_bot_color(const Uno*g,int p){
  int counts[4]={0},best=0;for(int i=0;i<g->count[p];i++){int c=g->hands[p][i];if(c<52)counts[c/13]++;}
  for(int c=1;c<4;c++)if(counts[c]>counts[best])best=c;return best;
}
int uno_bot_card(const Uno*g){
  int best=-1,rank=-10000,p=g->turn,counts[4]={0};
  for(int i=0;i<g->count[p];i++)if(g->hands[p][i]<52)counts[g->hands[p][i]/13]++;
  for(int i=0;i<g->count[p];i++)if(uno_can_play(g,i)){
    /* Card-selection priorities are private strategy, not a player score. */
    int c=g->hands[p][i],v=uno_value(c),r=c>=52?5:v>=10?20:v;
    if(c<52)r+=counts[c/13]*(g->rules.difficulty==2?7:3);
    else if(c==UNO_DRAW4&&counts[g->color])r-=150;
    if(g->rules.difficulty==2&&g->count[next(g,p)]<=2&&(v>=10&&v<=12))r+=55;
    if(g->rules.difficulty==2&&v==UNO_REVERSE&&g->rules.players>2&&g->count[(p+g->rules.players-g->direction)%g->rules.players]<=2)r-=45;
    if(g->rules.difficulty==0)r=(int)((g->rng^(unsigned)c*2654435761u)%(r< -100?10:100));
    if(r>rank){rank=r;best=i;}
  }return best;
}
void uno_bot(Uno*g){
  if(g->phase>=U_ROUND)return;
  int p=g->turn;
  if(g->phase==U_COLOR){uno_choose_color(g,uno_bot_color(g,p));return;}
  if(g->phase==U_CHALLENGE){
    /* Decision uses RNG, difficulty and public hand sizes, never guilty or
       any other player's hidden cards. The engine reveals evidence after. */
    unsigned chance=g->rules.difficulty==0?20:g->rules.difficulty==1?12:8;
    if(random32(g)%100<chance)uno_challenge(g);else uno_accept(g);return;
  }
  int i=uno_bot_card(g);
  if(i>=0){
    if(g->phase==U_DRAWN&&!(g->rules.flags&R_FORCE)&&g->rules.difficulty==0&&random32(g)%12==0){uno_keep(g);return;}
    bool call=random32(g)%100<(g->rules.difficulty==0?85:97);
    uno_play(g,i,call);
  }else if(g->phase==U_DRAWN)uno_keep(g);
  else uno_draw(g);
}
bool uno_valid(const Uno*g){
  if(g->rules.players<2||g->rules.players>4||g->rules.difficulty>2||g->rules.flags>31||!g->rng||
     g->turn>=g->rules.players||g->dealer>=g->rules.players||g->phase>U_MATCH||!g->xn||g->xn>108||g->dn>108||
     (g->direction!=1&&g->direction!=-1)||g->color>4||g->pending>104||g->event>E_STALEMATE)return false;
  if(g->color==4&&!(g->phase==U_COLOR&&g->offender==UNO_NONE))return false;
  const uint8_t ids[]={g->offender,g->candidate,g->winner,g->uno_player,g->uno_armed};
  for(unsigned i=0;i<sizeof ids;i++)if(ids[i]!=UNO_NONE&&ids[i]>=g->rules.players)return false;
  if(g->phase==U_DRAWN&&(g->drawn>=g->count[g->turn]||g->pending))return false;
  if(g->phase==U_CHALLENGE&&(!g->pending||g->penalty_kind!=4||g->offender==UNO_NONE))return false;
  if(g->pending&&g->penalty_kind!=2&&g->penalty_kind!=4)return false;
  if(g->phase==U_COLOR&&g->discard[g->xn-1]<52)return false;
  if(g->uno_player!=UNO_NONE&&g->count[g->uno_player]!=1)return false;
  if(g->candidate!=UNO_NONE&&g->count[g->candidate])return false;
  if(g->phase==U_MATCH&&(g->winner==UNO_NONE||g->count[g->winner]))return false;
  if(g->phase==U_ROUND&&g->winner!=UNO_NONE)return false;
  uint8_t hist[54]={0};int total=g->dn+g->xn;
  for(int p=0;p<4;p++){
    if(g->count[p]>108||(p>=g->rules.players&&g->count[p]))return false;
    total+=g->count[p];for(int i=0;i<g->count[p];i++){int c=g->hands[p][i];if(c>53)return false;hist[c]++;}
  }
  if(total!=108)return false;
  for(int i=0;i<g->dn;i++){if(g->deck[i]>53)return false;hist[g->deck[i]]++;}
  for(int i=0;i<g->xn;i++){if(g->discard[i]>53)return false;hist[g->discard[i]]++;}
  for(int c=0;c<54;c++)if(hist[c]!=(c>=52?4:c%13?2:1))return false;
  return true;
}
