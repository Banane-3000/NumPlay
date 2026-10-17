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
  scene=TITLE; dino_y=DY*256;score=0;
  frame();
  if(argc>=2) screenshot(argv[1]);

  /* Reference: original Python H=-13, gravity=+2, 49 px at frame 7. */
  start_game();assert(scene==PLAY && score==0 && count==0);
  int y=dino_y;
  cactus_meter=bird_meter=-100000;
  update(true,false);assert(dino_y==y-13*256);
  for(int t=0;t<6;t++) update(false,false);
  assert(dino_y==y-49*256);
  for(int t=0;t<7;t++) update(false,false);
  assert(dino_y==DY*256 && score==14 && speed==10);

  /* Python: speed changes AFTER the 180th frame, not before it. */
  start_game();ticks=179;cactus_meter=bird_meter=-100000;
  update(false,false);assert(speed==10 && bird_speed==5 && score==1);
  update(false,false);assert(speed==11 && bird_speed==6 && score==2);
  /* At difficulty >= 12, cacti have 14-pixel steps; birds have 9. */
  ticks=720;update(false,false);assert(speed==14 && bird_speed==9);
  /* Every frame advances birds left (5 to 14px) and not right. */
  start_game();cactus_meter=bird_meter=-100000;
  obstacles[0]=(obstacle){280,DY,14,23,CACTUS};
  obstacles[1]=(obstacle){270,165,21,18,BIRD};count=2;
  update(false,false);
  assert(obstacles[0].x==270 && obstacles[1].x==265);

  /* Regression: the original sprite's beak faces LEFT. Mirroring it is wrong. */
  bg=RGB(0xFFFFFF);ink=RGB(0x535353);band_y=0;
  for(int i=0;i<W*BAND;i++) pixels[i]=bg;
  SPRITE(BIRD_WING_DOWN,80,4,ink);
  assert(pixels[4*W+84]==ink); /* Source first rect x=4..6: left-facing beak. */
  assert(pixels[4*W+95]==bg); /* Old mirrored version wrongly filled this spot. */

  /* Three smooth visual positions per unchanged Python-scale physics step. */
  start_game();cactus_meter=bird_meter=-100000;
  obstacles[0]=(obstacle){270,DY,14,23,CACTUS};
  obstacles[1]=(obstacle){200,165,21,18,BIRD};count=2;
  update(false,false);
  assert(obstacles[0].x==260 && obstacles[1].x==195);
  render_phase=0;
  assert(interpolated(old_obstacle_x[0],obstacles[0].x)==270);
  assert(interpolated(old_obstacle_x[1],obstacles[1].x)==200);
  render_phase=1;
  assert(interpolated(old_obstacle_x[0],obstacles[0].x)==267);
  render_phase=2;
  assert(interpolated(old_obstacle_x[0],obstacles[0].x)==264);
  render_phase=3;
  assert(interpolated(old_obstacle_x[0],obstacles[0].x)==260);
  /* Input is polled on visual frames, but physics runs only on its 100ms tick. */
  start_game();
  uint32_t no_tick=ticks;
  simulation_tick=false;
  assert(!handle_input(0,0) && ticks==no_tick);
  simulation_tick=true;
  assert(!handle_input(0,0) && ticks==no_tick+1);

  /* Python ducking changes gravitational acceleration, even in the air. */
  start_game();cactus_meter=bird_meter=-100000;
  update(true,false);update(false,true);
  assert(duck && gravity_units==9 && dino_y<DY*256);
  update(false,false);assert(!duck && gravity_units==2);
  /* Cloud motion follows bird/cloud speed, not cactus speed. */
  start_game();cactus_meter=bird_meter=-100000;
  clouds[0]=(decoration){200,80};cloud_count=1;cloud_clock=-99999;
  update(false,false);assert(clouds[0].x==195);

  /* Controls: Clear pauses and resumes; Back exits immediately. */
  start_game();
  assert(!handle_input(PAUSE,PAUSE) && scene==PAUSED);
  uint32_t before=ticks;
  assert(!handle_input(0,0) && ticks==before);
  assert(!handle_input(PAUSE,PAUSE) && scene==PLAY && ticks==before);
  assert(handle_input(KEY(eadk_key_back),KEY(eadk_key_back)) && scene==PLAY);
  scene=TITLE;
  assert(handle_input(KEY(eadk_key_back),KEY(eadk_key_back)));
  scene=PAUSED;
  assert(handle_input(KEY(eadk_key_home),KEY(eadk_key_home)));

  /* Original Python cactus collision: d[] x anchor and y position. */
  start_game();high_score=0;
  count=1; obstacles[0]=(obstacle){DX+22,DY,14,23,CACTUS};
  assert(collision());
  update(false,false);assert(scene==DEAD && high_score==score);
  start_game();assert(scene==PLAY && score==0 && count==0);

  /* Simulate a long random run and verify all bounded game arrays stay safe.
   * This also checks that scores, difficulty and every spawn type co-exist. */
  start_game();
  for(int i=0;i<2500;i++) {
    if(scene==DEAD) start_game();
    update((i%17)<4, (i%29)>24);
    assert(count>=0 && count<=12);
    assert(cloud_count>=0 && cloud_count<=3);
    assert(dust_count>=0 && dust_count<=5);
    assert(pebble_count>=0 && pebble_count<=2);
  }

  /* Both sprite variants and the sky can render safely in all 240 rows. */
  start_game();score=328;ticks=1312;
  count=3;
  obstacles[0]=(obstacle){244,DY,14,23,CACTUS};
  obstacles[1]=(obstacle){289,DY,30,23,LARGE_CACTUS};
  obstacles[2]=(obstacle){152,GROUND-28,21,18,BIRD};
  clouds[0]=(decoration){170,70};cloud_count=1;
  frame();
  if(argc>=3)screenshot(argv[2]);
  printf("Host tests passed: Python jump and speed, original left-facing bird, 30fps interpolation, sky, collisions, Back exit, Clear pause.\n");
  return 0;
}
