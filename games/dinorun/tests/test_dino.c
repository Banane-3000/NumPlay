/* Host gameplay and rendering test of the ACTUAL native source. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#define main dino_app_main
#include "../src/main.c"
#undef main
static uint16_t screen[W*H];
uint64_t eadk_keyboard_scan(void) {return 0;}
uint32_t eadk_timing_millis(void) {return 0;}
void eadk_timing_msleep(uint32_t ms) {(void)ms;}
void eadk_display_push_rect(eadk_rect_t r,const uint16_t *px) {
  assert(r.x+r.width<=W && r.y+r.height<=H);
  for(int y=0;y<r.height;y++) for(int x=0;x<r.width;x++)
    screen[(y+r.y)*W+x+r.x]=px[y*r.width+x];
}
static void screenshot(const char *f) {
  FILE *fp=fopen(f,"wb"); assert(fp);
  fprintf(fp,"P6\n%d %d\n255\n",W,H);
  for(int i=0;i<W*H;i++) {
    uint16_t c=screen[i];
    unsigned char rgb[3]={(unsigned char)(((c>>11)&31)*255/31),
                          (unsigned char)(((c>>5)&63)*255/63),
                          (unsigned char)((c&31)*255/31)};
    fwrite(rgb,1,3,fp);
  }
  fclose(fp);
}
int main(int argc,char **argv) {
  scene=TITLE;dino_y=DY*256;score=0;
  frame();
  if(argc>=2)screenshot(argv[1]);
  start_game();assert(scene==PLAY && score==0 && count==0);
  int y=dino_y;
  update(true,false);assert(dino_y<y && dino_vy<0);
  for(int t=0;t<80;t++)update(false,false);
  assert(dino_y==DY*256);
  update(false,true);assert(duck);
  update(false,false);assert(!duck);
  /* Force a collision, test the game-over state and restart. */
  count=1;obstacles[0]=(obstacle){DX+12,GROUND-32,19,32,CACTUS};
  update(false,false);assert(scene==DEAD && high_score==score);
  start_game();assert(scene==PLAY && score==0 && count==0);
  for(int i=0;i<160;i++){
    if(scene==DEAD)start_game();
    update((i%31)==1,false);
  }
  start_game();
  score=328; ticks=1312;distance_px=2100;
  count=3;
  obstacles[0]=(obstacle){244,GROUND-32,19,32,CACTUS};
  obstacles[1]=(obstacle){304,GROUND-43,24,43,LARGE_CACTUS};
  obstacles[2]=(obstacle){152,GROUND-75,33,22,BIRD};
  frame();
  if(argc>=3)screenshot(argv[2]);
  printf("Host test passed: jump, crouch, collision, restart, rendering.\n");
  return 0;
}
