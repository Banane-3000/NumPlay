#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#define NP_MODULE 1
#define main uno_appmain
#include "../src/main.c"
#undef main
static uint16_t pixels[320*240];
uint64_t eadk_timing_millis(void){return clock_ms;}
uint32_t eadk_random(void){return 73;}
uint64_t eadk_keyboard_scan(void){return 0;}
void eadk_display_wait_for_vblank(void){}
void eadk_timing_msleep(uint32_t n){(void)n;}
void*np_alloc(uint32_t n){return malloc(n);}
bool np_file_write(const char*n,const void*d,uint32_t z){assert(!strcmp(n,SAVE_NAME)&&z==768);return true;}
const uint8_t*np_file_read(const char*n,uint32_t*z){*z=0;return NULL;}
void np_push(int x,int y,int w,int h,const color_t*d){assert(x>=0&&y>=0&&x+w<=320&&y+h<=240);for(int i=0;i<h;i++)memcpy(pixels+(y+i)*320+x,d+i*w,w*2);}
static void shot(const char*name){
  gfx_render(scene,NULL,0,240);FILE*f=fopen(name,"wb");assert(f);fprintf(f,"P6\n320 240\n255\n");
  for(unsigned i=0;i<320*240;i++){unsigned c=pixels[i];uint8_t rgb[3]={(c>>11)*255/31,((c>>5)&63)*255/63,(c&31)*255/31};fwrite(rgb,1,3,f);}fclose(f);
}
static void art(void){
  assert(sizeof(card_palette)/2==31&&sizeof(card_offsets)/2==56);
  for(int i=0;i<55;i++){
    const uint8_t*p=card_pixels+card_offsets[i];
    for(int y=0;y<64;y++){int n=*p++,x=0;assert(n>0&&n<=44);for(int j=0;j<n;j++)x+=(p[j]>>5)+1;assert(x==44);p+=n;}
    assert(p==card_pixels+card_offsets[i+1]);
  }
  for(int i=0;i<7;i++){
    /* Check the wrapped help really fits the panel, using the actual font. */
    const char*p=HELP[i];int lines=1,w=0;
    while(*p){const char*start=p;while(*p&&*p!=' ')p++;char word[80];unsigned n=p-start;assert(n<sizeof word);memcpy(word,start,n);word[n]=0;
      int ww=gfx_text_width(&np_font_body,word),space=gfx_text_width(&np_font_body," ");assert(ww<=266);
      if(w&&w+space+ww>266){lines++;w=ww;}else w+=ww+(w?space:0);if(*p)p++;
    }
    assert(72+(lines-1)*20+4<209);
  }
  for(int i=0;i<5;i++)assert(gfx_text_width(&np_font_small,VARIANT[i])<218);
  for(int i=0;i<6;i++)assert(gfx_text_width(&np_font_small,VARIANT_HELP[i])<296);
  assert(172+gfx_text_width(&np_font_small,"DISCARD")/2+6<=222-gfx_text_width(&np_font_small,"COLOR")/2);
  assert(gfx_text_width(&np_font_small,"1 Draw +16")<=64);
  assert(gfx_text_width(&np_font_small,"Back: pause")<=79);
  for(int c=0;c<54;c++){char s[60];name_card(c,s);assert(gfx_text_width(&np_font_small,s)<126);}
}
static void setup_navigation(void){
  screen=S_SETTINGS;setup_page=0;sel=0;Uno before=*G;
  for(int i=0;i<3;i++)setup_input(1,0,false,false);
  assert(sv.prefs.players==4&&setup_page==0&&sel==0);
  setup_input(0,1,false,false);
  for(int i=0;i<3;i++)setup_input(1,0,false,false);
  assert(sv.prefs.difficulty==1);
  setup_input(0,1,true,false);assert(setup_page==1&&sel==0&&screen==S_SETTINGS);
  for(int i=0;i<5;i++){setup_input(0,1,true,false);assert(sv.prefs.flags==(1u<<(i+1))-1);}
  assert(sv.prefs.flags==31&&custom_rules(&sv.prefs));
  setup_input(0,1,true,false);assert(setup_page==2&&screen==S_SETTINGS);
  setup_input(0,0,false,true);assert(setup_page==1&&sel==6);
  setup_input(0,1,true,false);assert(sel==0&&sv.prefs.flags==0);
  setup_input(0,0,false,true);assert(setup_page==0&&sel==2);
  setup_input(0,0,false,true);assert(screen==S_MENU);
  assert(!memcmp(&before,G,sizeof before));
}
int main(void){
  gfx_buf=strip;sv.prefs=(UnoRules){4,1,0};ui_random=5123;clock_ms=10000;last_action=0;screen=S_MENU;sel=1;
  art();shot("menu.ppm");
  setup_navigation();screen=S_SETTINGS;setup_page=0;sel=0;shot("settings.ppm");
  sv.prefs.players=2;shot("settings-two.ppm");sv.prefs.players=4;
  setup_page=1;sel=2;sv.prefs.flags=3;shot("variants.ppm");
  setup_page=2;sel=0;shot("review.ppm");sv.prefs.flags=0;shot("review-classic.ppm");
  for(int n=2;n<=4;n++){
    sv.prefs.players=n;new_game();while(G->phase==U_COLOR)uno_choose_color(G,0);G->turn=0;last_action=0;
    char s[40];sprintf(s,"table-%d.ppm",n);shot(s);G->turn=1;selected=3;sprintf(s,"bot-turn-%d.ppm",n);shot(s);
    int old=selected;play_input(1,0,false,false,false,false);assert(selected==(old+1)%G->count[0]);
  }
  for(int i=0;i<7;i++){screen=S_HELP;help_page=i;char s[40];sprintf(s,"help-%d.ppm",i);shot(s);}
  screen=S_CREDITS;shot("credits.ppm");screen=S_PAUSE;sel=0;shot("pause.ppm");screen=S_TABLE_INFO;shot("table-rules.ppm");
  screen=S_PLAY;G->phase=U_COLOR;G->turn=0;color_sel=2;shot("color.ppm");
  G->phase=U_CHALLENGE;G->pending=4;G->old_color=1;sel=1;shot("challenge.ppm");
  evidence_count=G->count[1];memcpy(evidence,G->hands[1],evidence_count);selected=0;screen=S_EVIDENCE;shot("evidence.ppm");
  screen=S_PLAY;G->phase=U_MATCH;G->winner=0;sel=0;shot("result.ppm");
  /* Play again retains the active table, even if future-game prefs differ. */
  UnoRules rules=G->rules;sv.prefs=(UnoRules){2,0,R_AUTO_UNO};
  play_input(0,0,true,false,false,false);assert(screen==S_PLAY&&G->phase<U_ROUND);
  assert(!memcmp(&rules,&G->rules,sizeof rules)&&sv.prefs.flags==R_AUTO_UNO);
  G->phase=U_MATCH;G->winner=1;shot("bot-wins.ppm");
  play_input(0,1,true,false,false,false);assert(screen==S_MENU&&!sv.active);
  screen=S_SAVE_ERROR;sel=0;exit_after_save=true;shot("save-error.ppm");
  /* Navigate every card even when the hand is bigger than one screen. */
  new_game();G->phase=U_TURN;G->turn=0;
  while(G->dn){G->hands[0][G->count[0]++]=G->deck[--G->dn];}
  selected=G->count[0]-1;last_action=0;shot("large-hand.ppm");assert(uno_valid(G));
  for(int j=0;j<G->count[0];j++)play_input(1,0,false,false,false,false);
  assert(selected==G->count[0]-1);
  /* Disabled actions cannot trap focus, and returning to the hand is explicit. */
  G->turn=1;G->uno_player=UNO_NONE;action=-1;play_input(0,-1,false,false,false,false);assert(action==2);
  play_input(1,0,false,false,false,false);assert(action==2);
  play_input(0,1,false,false,false,false);assert(action==-1);
  G->turn=0;G->phase=U_DRAWN;G->rules.flags=R_FORCE;G->drawn=selected;
  play_input(0,-1,false,false,false,false);assert(action==2);
  puts("Asset decoder, visual setup navigation, text layout, card browsing and UI screenshots passed.");return 0;
}
