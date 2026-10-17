/* Dino Run — C gameplay for NumWorks / NumPlay with geometry converted from
 * the Python Chrome Dino sprites. See dino_sprites.h for source/rights note.
 * 320x240, RGB565, 16-row bands; physics follows the Python frame steps.
 * Keyboard: OK jump, Down duck, Clear/Backspace pause, Back exits.
 * Pixel steps follow the original Python logic; rendering uses a paced game clock.
 */
#include <eadk.h>
#include <stdbool.h>
#include <stdint.h>
#include "../../common/epsilon_app.h"
#include "../../common/epsilon_files.h"
#include "dino_sprites.h"

#ifdef __ELF__
const char eadk_app_name[] __attribute__((section(".rodata.eadk_app_name"))) = "Chrome Dino";
const uint32_t eadk_api_level __attribute__((section(".rodata.eadk_api_level"))) = 0;
#endif

#define W 320
#define H 240
#define GROUND 195
#define DX 20
/* In the original Python sprites (1 px per art pixel), standing is drawn
 * around (20,190): 12 px above this anchor, 9 px below it. */
#define DY 190
#define BAND 16
#define KEY(k) (1ull << (k))
#define RGB(c) (uint16_t)((((c) >> 8) & 0xF800u) | (((c) >> 5) & 0x07E0u) | (((c) >> 3) & 0x1Fu))
/* Unlike the old test build, Back returns immediately to NumPlay / Home. */
#define JUMP KEY(eadk_key_ok)
#define DUCK (KEY(eadk_key_down) | KEY(eadk_key_two))
#define PAUSE KEY(eadk_key_backspace)
#define EXIT (KEY(eadk_key_back) | KEY(eadk_key_home) | KEY(eadk_key_on_off))
/* The Python original sleeps 30 ms *after* slow Kandinsky drawing. Its effective
 * frame period is much longer than 30 ms; cap native speed at ~10 fps so the
 * same 10-pixel/frame obstacles do not scroll 3x too fast. This is a timing
 * approximation: only a real calculator comparison can measure Python FPS. */
#define GAME_FRAME_MS 100
#define SAVE_NAME "dinorun.sav"

typedef uint16_t color;
typedef struct { int16_t x, y; uint8_t w, h, kind; } obstacle;
typedef struct { int16_t x, y; } decoration;
enum { TITLE, HELP, PLAY, PAUSED, DEAD };
enum { CACTUS, LARGE_CACTUS, BIRD };

static color pixels[W * BAND];
static int band_y, scene;
static color bg, ink, faint;
static obstacle obstacles[12];
static decoration clouds[3], dust[5], pebbles[2];
static int cloud_count, dust_count, pebble_count;
static int cloud_clock, cloud_delay, dust_clock, dust_delay, pebble_clock, pebble_delay;
static int count, dino_y, dino_vy, duck, score, high_score, speed;
/* Python values: H=-13 for jump, gravity ad=2 (+7 each duck frame),
 * cacti move Q=10..14 px/frame, birds move T=5..14 px/frame.
 * The simulation uses one step per frame, just like the source. */
static int gravity_units, cactus_meter, bird_meter;
static int bird_speed;
static uint32_t rng=0xA319F4B5u, ticks, distance_px;
static bool new_record;

static uint32_t random32(void) {
  rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5;
  return rng;
}
static int randint(int n) { return n>0 ? (int)(random32() % (uint32_t)n) : 0; }
/* MicroPython randint(a,b) is inclusive. */
static int randrange(int lo, int hi) { return lo + randint(hi-lo+1); }
static int imax(int a,int b) { return a>b?a:b; }

static void rect(int x, int y, int w, int h, color c) {
  if (w <= 0 || h <= 0) return;
  int x0=x<0?0:x, x1=x+w>W?W:x+w;
  int y0=y<band_y?band_y:y, y1=y+h>band_y+BAND?band_y+BAND:y+h;
  if (x0 >= x1 || y0 >= y1) return;
  for (int yy=y0; yy<y1; ++yy) {
    color *p=pixels + (yy-band_y)*W + x0;
    for (int xx=x0; xx<x1; ++xx) *p++=c;
  }
}

/* Compact 5 x 7 uppercase font. Each byte holds one row's five bits. */
typedef struct { char ch; uint8_t row[7]; } glyph;
static const glyph FONT[] = {
 {'A',{14,17,17,31,17,17,17}}, {'B',{30,17,17,30,17,17,30}},
 {'C',{14,17,16,16,16,17,14}}, {'D',{30,17,17,17,17,17,30}},
 {'E',{31,16,16,30,16,16,31}}, {'F',{31,16,16,30,16,16,16}},
 {'G',{14,17,16,23,17,17,14}}, {'H',{17,17,17,31,17,17,17}},
 {'I',{31,4,4,4,4,4,31}}, {'J',{7,2,2,2,18,18,12}},
 {'K',{17,18,20,24,20,18,17}}, {'L',{16,16,16,16,16,16,31}},
 {'M',{17,27,21,21,17,17,17}}, {'N',{17,25,21,19,17,17,17}},
 {'O',{14,17,17,17,17,17,14}}, {'P',{30,17,17,30,16,16,16}},
 {'Q',{14,17,17,17,21,18,13}}, {'R',{30,17,17,30,20,18,17}},
 {'S',{15,16,16,14,1,1,30}}, {'T',{31,4,4,4,4,4,4}},
 {'U',{17,17,17,17,17,17,14}}, {'V',{17,17,17,17,17,10,4}},
 {'W',{17,17,17,21,21,21,10}}, {'X',{17,17,10,4,10,17,17}},
 {'Y',{17,17,10,4,4,4,4}}, {'Z',{31,1,2,4,8,16,31}},
 {'0',{14,17,19,21,25,17,14}}, {'1',{4,12,4,4,4,4,14}},
 {'2',{14,17,1,2,4,8,31}}, {'3',{30,1,1,14,1,1,30}},
 {'4',{2,6,10,18,31,2,2}}, {'5',{31,16,30,1,1,17,14}},
 {'6',{6,8,16,30,17,17,14}}, {'7',{31,1,2,4,8,8,8}},
 {'8',{14,17,17,14,17,17,14}}, {'9',{14,17,17,15,1,2,12}},
 {':',{0,4,4,0,4,4,0}}, {'/',{1,1,2,4,8,16,16}},
 {'!',{4,4,4,4,4,0,4}}, {'-',{0,0,0,31,0,0,0}},
 {'+',{0,4,4,31,4,4,0}}, {'.',{0,0,0,0,0,12,12}}
};
static const uint8_t *glyph_rows(char c) {
  for (unsigned i=0;i<sizeof FONT/sizeof FONT[0];i++) if(FONT[i].ch==c) return FONT[i].row;
  return 0;
}
static int text_width(const char *s, int scale) { int n=0; while (*s++) n++; return n*6*scale; }
static void text_at(int x,int y,const char *s,int scale,color c) {
  for (;*s;s++,x+=6*scale) {
    const uint8_t *r=glyph_rows(*s);
    if (!r) continue;
    for (int yy=0;yy<7;yy++) for (int xx=0;xx<5;xx++)
      if (r[yy] & (1u << (4-xx))) rect(x+xx*scale,y+yy*scale,scale,scale,c);
  }
}
static void centered(int y,const char *s,int scale,color c) { text_at((W-text_width(s,scale))/2,y,s,scale,c); }
static void number(int x,int y,unsigned n,int min_digits,int scale,color c) {
  char s[12]; int i=0;
  do {s[i++]=(char)('0'+n%10);n/=10;} while (n && i<10);
  while(i<min_digits && i<10) s[i++]='0';
  for(int j=0;j<i/2;j++) {char t=s[j];s[j]=s[i-1-j];s[i-1-j]=t;}
  s[i]=0;text_at(x,y,s,scale,c);
}

/* Draw the original rectangle lists without approximation or antialiasing.
 * The existing banded framebuffer clips each individual rectangle for us. */
static void sprite(const dino_sprite_rect *pieces, unsigned n,
                   int x, int y, color c) {
  for (unsigned i=0; i<n; i++)
    rect(x+pieces[i].x, y+pieces[i].y,
         pieces[i].w, pieces[i].h, c);
}
#define SPRITE(name, x, y, c) sprite(name, COUNT_OF(name), x, y, c)

/* Mirrored about a 21-pixel bounding box. The imported aB/an bird images
 * face right even though the Python game moves them left; keeping each pixel
 * but reversing its X coordinate fixes the "flying backward" appearance. */
static void bird_sprite(const dino_sprite_rect *pieces, unsigned n,
                        int x,int y,color c) {
  for(unsigned i=0;i<n;i++)
    rect(x+21-pieces[i].x-pieces[i].w,
         y+pieces[i].y,pieces[i].w,pieces[i].h,c);
}
static void dino(int x,int y,bool crouch,int step) {
  /* Original J%5: 2 frames of pose 1, 3 frames of pose 2. */
  bool first=(step%5)<2;
  if(crouch) {
    /* The original Python draws aj (21 rects), NOT V (aj + 7 fills).
     * V is preserved in the header but use only its aj prefix here. */
    if(first) sprite(DINO_DUCK_1,COUNT_OF(DINO_DUCK_1)-7,x,y,ink);
    else      SPRITE(DINO_DUCK_2,x,y,ink);
  } else if(y<DY) {
    SPRITE(DINO_JUMP,x,y,ink);
  } else if(first) {
    SPRITE(DINO_RUN_1,x,y,ink);
  } else {
    SPRITE(DINO_RUN_2,x,y,ink);
  }
}
static void cactus(obstacle *o) {
  /* o->x is the ORIGINAL Python d[] anchor; sprite S extends 20px left. */
  SPRITE(CACTUS_ORIGINAL,o->x,DY,ink);
  if(o->kind==LARGE_CACTUS) SPRITE(CACTUS_ORIGINAL,o->x+20,DY,ink);
}
static void bird(obstacle *o) {
  /* Exact aB/an rectangles and original two/three-frame flap cadence,
   * mirrored only so its beak points in its direction of travel (left). */
  if(((ticks? ticks-1:0)%5)<2) bird_sprite(BIRD_WING_DOWN,COUNT_OF(BIRD_WING_DOWN),o->x,o->y,ink);
  else            bird_sprite(BIRD_WING_UP,COUNT_OF(BIRD_WING_UP),o->x,o->y,ink);
}
static void scenery(void) {
  rect(0,GROUND,W,1,ink);
  if((score/300)&1) {
    /* Fixed moon and star placements match the original nighttime scene. */
    SPRITE(MOON_ORIGINAL,50,90,faint);
    static const int16_t stars[][3] = {
      {100,100,1},{20,110,0},{60,40,1},{140,20,0},
      {130,75,1},{60,140,0},{260,120,1},{180,90,0},
      {220,70,1},{280,60,0},{250,20,1},{160,110,0}
    };
    for(unsigned i=0;i<sizeof(stars)/sizeof(stars[0]);i++) {
      if(stars[i][2]) SPRITE(STAR_ORIGINAL_1,stars[i][0],stars[i][1],faint);
      else            SPRITE(STAR_ORIGINAL_2,stars[i][0],stars[i][1],faint);
    }
  }
  /* Clouds at 5 px/frame, like Python's j[] list, visible in both palettes. */
  for(int i=0;i<cloud_count;i++)
    SPRITE(CLOUD_ORIGINAL,clouds[i].x,clouds[i].y,faint);
  /* Moving ground specks and tufts use the original W/av sprite lists. */
  for(int i=0;i<dust_count;i++) rect(dust[i].x,dust[i].y,2,1,ink);
  for(int i=0;i<pebble_count;i++)
    SPRITE(GROUND_DETAIL_1,pebbles[i].x,DY,ink);
}
/* Match the original Python aM, ay and ap collision predicates, which operate
 * on sprite anchor positions (not on the old approximated C bounding boxes). */
static bool collision(void) {
  int y=dino_y/256;
  for(int i=0;i<count;i++) {
    const obstacle *o=&obstacles[i];
    if(o->kind==BIRD) {
      if(DX<o->x+14 && DX+30>o->x && y<o->y+14 && y+20>o->y)
        return true;
    } else {
      int width=duck?26:14;
      if(o->x-20<DX+width && o->x-8>DX && DY-20<y && DY>y-(duck?12:20))
        return true;
      if(o->kind==LARGE_CACTUS &&
         o->x+20-20<DX+width && o->x+20-8>DX && DY-20<y && DY>y-(duck?12:20))
        return true;
    }
  }
  return false;
}
static void start_game(void) {
  scene=PLAY;count=0;score=0;speed=10;bird_speed=5;distance_px=0;
  cactus_meter=0;bird_meter=0;gravity_units=2;
  cloud_count=dust_count=pebble_count=0;
  cloud_clock=dust_clock=pebble_clock=0;
  cloud_delay=randrange(30,70);dust_delay=randrange(1,2);
  pebble_delay=randrange(30,50);
  duck=0;dino_y=DY*256;dino_vy=0;ticks=0;new_record=false;
}
/* The original Python's E/q counters and inclusive random thresholds. */
static void spawn(void) {
  int difficulty=(int)(ticks/60); /* B has not advanced yet this frame */
  int cactus_count=0, bird_count=0, last_cactus=-10000;
  for(int i=0;i<count;i++) {
    if(obstacles[i].kind==BIRD) bird_count++;
    else {cactus_count++;last_cactus=obstacles[i].x;}
  }
  int gap=130+difficulty*4+randrange(0,60);
  int min_threshold=imax(20,cactus_meter/2-difficulty);
  int max_threshold=min_threshold+40+randrange(0,20);
  if(cactus_count<3 && cactus_meter>=randrange(min_threshold,max_threshold)
     && randrange(1,4)==1 && (cactus_count==0 || last_cactus<350-gap)) {
    int amount=randrange(1,6)==6 ? randrange(2,3) : 1;
    for(int i=0;i<amount && count<12;i++) {
      int x=amount>1 ? 350+20*(i+1) : 350;
      obstacles[count++]=(obstacle){(int16_t)x,DY,14,23,CACTUS};
    }
    if(amount==1) cactus_meter=0; /* original does not reset E for cactus groups */
  }
  int p=imax(40,bird_meter/2);
  if(bird_count<1 && count<12 && bird_meter>=randrange(p,p+40)
     && score>300 && randrange(1,7)==2) {
    /* The Python also appends a cactus far behind the flying bird. */
    if(count<11) obstacles[count++]=(obstacle){650,DY,14,23,CACTUS};
    obstacles[count++]=(obstacle){400,(int16_t)randrange(150,180),21,18,BIRD};
    bird_meter=0;
  }
  if(cloud_count<3 && cloud_clock>=cloud_delay) {
    clouds[cloud_count++]=(decoration){320,(int16_t)randrange(50,120)};
    cloud_clock=0;
    cloud_delay=randrange(imax(1,20-difficulty),imax(1,60-difficulty));
  }
  if(dust_count<5 && dust_clock>=dust_delay) {
    dust[dust_count++]=(decoration){320,(int16_t)randrange(197,203)};
    dust_clock=0;
    dust_delay=randrange(1,3);
  }
  if(pebble_count<2 && pebble_clock>=pebble_delay) {
    pebbles[pebble_count++]=(decoration){500,0};
    pebble_clock=0;
    pebble_delay=randrange(imax(1,20-difficulty),imax(1,60-difficulty));
  }
}
static void advance_decorations(void) {
  for(int i=0;i<cloud_count;) {
    clouds[i].x-=bird_speed;
    if(clouds[i].x<=-50) {for(int j=i+1;j<cloud_count;j++)clouds[j-1]=clouds[j];cloud_count--;}
    else i++;
  }
  for(int i=0;i<dust_count;) {
    dust[i].x-=speed;
    if(dust[i].x<=-10) {for(int j=i+1;j<dust_count;j++)dust[j-1]=dust[j];dust_count--;}
    else i++;
  }
  for(int i=0;i<pebble_count;) {
    pebbles[i].x-=speed;
    if(pebbles[i].x<=-20) {for(int j=i+1;j<pebble_count;j++)pebbles[j-1]=pebbles[j];pebble_count--;}
    else i++;
  }
}
static void lose(void) {
  scene=DEAD;
  if(score>high_score) {
    high_score=score;new_record=true;
    uint32_t data[2]={0x4F4E4944u,(uint32_t)high_score};
    ef_write(SAVE_NAME,data,sizeof data);
  }
}
static void update(bool jump,bool down) {
  /* Python increments score at the start, then advances difficulty after
   * processing movement on every 60th frame. */
  score++;
  speed=10+(int)(ticks/180); if(speed>14)speed=14;
  bird_speed=5+(int)(ticks/180); if(bird_speed>14)bird_speed=14;
  spawn();
  /* The original Python uses H=-13, then applies H+=ad (ad starts at 2).
   * The C value is scaled by 256 only for consistency with the old tests. */
  if(jump && dino_y>=DY*256 && !down) dino_vy=-13*256;
  duck=down; /* The Python also renders its duck pose while airborne. */
  dino_y+=dino_vy;
  if(dino_y<DY*256) dino_vy+=gravity_units*256;
  else {dino_y=DY*256;dino_vy=0;}
  if(down)gravity_units+=7;
  else gravity_units=2;

  for(int i=0;i<count;) {
    int move=obstacles[i].kind==BIRD ? bird_speed : speed;
    obstacles[i].x-=(int16_t)move;
    bool gone=(obstacles[i].kind==BIRD?obstacles[i].x<=-30:obstacles[i].x<=-10);
    if(gone) {for(int j=i+1;j<count;j++)obstacles[j-1]=obstacles[j];count--;}
    else i++;
  }
  advance_decorations();
  if(collision())lose();
  distance_px+=(uint32_t)speed;
  int difficulty=(int)(ticks/60);
  cactus_meter+=randrange(1,4+difficulty/3);
  bird_meter++;cloud_clock++;dust_clock++;pebble_clock++;
  ticks++;
}
static void frame(void) {
  bool night=((score/300)&1)!=0;
  /* Exact daytime/nighttime palette from the Python version. */
  bg=RGB(night?0x000000:0xFFFFFF);
  ink=RGB(night?0xACACAC:0x535353);
  faint=RGB(night?0x252525:0xDADADA);
  for(band_y=0;band_y<H;band_y+=BAND) {
    for(int i=0;i<W*BAND;i++)pixels[i]=bg;
    scenery();
    if(scene==PLAY || scene==PAUSED || scene==DEAD) {
      for(int i=0;i<count;i++) {
        if(obstacles[i].kind==BIRD)bird(&obstacles[i]);else cactus(&obstacles[i]);
      }
      dino(DX,dino_y/256,duck,(int)(ticks? ticks-1:0));
      text_at(11,11,"HI",2,faint);number(47,11,high_score,5,2,ink);
      number(245,11,(unsigned)score,5,2,ink);
    } else {
      /* The original title character is the separately encoded 3x sprite. */
      SPRITE(DINO_MENU_LARGE,120,150,ink);
      centered(40,"CHROME DINO",3,ink);
      centered(202,scene==HELP?"OK TO JUMP":"PRESS OK TO PLAY",2,ink);
      centered(225,scene==HELP?"DOWN DUCK  CLEAR PAUSE":"SHIFT FOR HELP",1,faint);
      if(scene!=HELP) {text_at(9,230,"BEST",1,faint);number(46,230,high_score,5,1,ink);}
    }
    if(scene==DEAD) {
      rect(46,61,228,90,bg);
      centered(73,"GAME OVER",3,ink);
      centered(108,new_record?"NEW RECORD!":"TRY AGAIN!",1,ink);
      centered(127,"OK TO RESTART",2,ink);
    } else if(scene==PAUSED) {
      rect(75,70,170,76,bg);
      centered(82,"PAUSED",3,ink);
      centered(115,"CLEAR TO RESUME",1,ink);
      centered(129,"BACK TO EXIT",1,ink);
    }
    eadk_display_push_rect((eadk_rect_t){0,(uint16_t)band_y,W,BAND},pixels);
  }
}

/* Separated out so native host tests can verify controls as well as physics.
 * Returns true when Back/Home requests leaving the app/NumPlay. */
static bool handle_input(uint64_t keys,uint64_t hit) {
  if(keys&EXIT) return true;
  if(scene==TITLE || scene==HELP) {
    if(hit&JUMP) start_game();
    else if(hit&KEY(eadk_key_shift)) scene=scene==TITLE?HELP:TITLE;
  } else if(scene==PLAY) {
    if(hit&PAUSE) scene=PAUSED;
    else update((keys&JUMP)!=0,(keys&DUCK)!=0);
  } else if(scene==PAUSED) {
    if(hit&PAUSE) scene=PLAY;
  } else if(scene==DEAD) {
    if(hit&JUMP) start_game();
  }
  return false;
}
int main(void) {
  np_app_begin();
  uint32_t len=0;
  const uint8_t *save=ef_read(SAVE_NAME,&len);
  if(save && len==8 && save[0]=='D' && save[1]=='I' && save[2]=='N' && save[3]=='O') {
    uint32_t value=(uint32_t)save[4]|(uint32_t)save[5]<<8|(uint32_t)save[6]<<16|(uint32_t)save[7]<<24;
    if(value<100000000u)high_score=(int)value;
  }
  rng^=(uint32_t)eadk_timing_millis()*2654435761u;
  scene=TITLE; dino_y=DY*256;
  uint64_t held=eadk_keyboard_scan();
  for(;;) {
    uint32_t now=(uint32_t)eadk_timing_millis();
    uint64_t keys=eadk_keyboard_scan(),hit=keys&~held;
    held=keys;
    if(handle_input(keys,hit)) break;
    frame();
    uint32_t elapsed=(uint32_t)eadk_timing_millis()-now;
    if(elapsed<GAME_FRAME_MS) eadk_timing_msleep(GAME_FRAME_MS-elapsed);
  }
  return np_app_end();
}
