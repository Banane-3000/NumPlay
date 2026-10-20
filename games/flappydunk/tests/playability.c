/* SPDX-License-Identifier: GPL-3.0-only
 * Offline reachability search. This test controller is never shipped in the app.
 * It uses only the real flap/step API, favouring clear approaches to the rim.
 */
#include "dunk.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

#define BEAM 4096
#define HORIZON 180
typedef struct { fd_game g;fd_stats s;uint8_t taps[HORIZON]; } node;
static node layer[2][BEAM];
static int seen[320*44*2];
static uint8_t taps[HORIZON];
static unsigned chosen_ticks;
static bool next(fd_game *g,fd_stats *s) {
  layer[0][0].g=*g;layer[0][0].s=*s;
  memset(layer[0][0].taps,0,HORIZON);
  unsigned count=1;
  for(unsigned tick=0;tick<HORIZON&&count;++tick) {
    unsigned n=0;int best=-1;
    memset(seen,255,sizeof seen);
    for(unsigned i=0;i<count;++i)for(unsigned tap=0;tap<2;++tap) {
      node a=layer[tick&1][i];
      if(tap&&(a.g.flap_anim>4||!fd_flap(&a.g,&a.s)))continue;
      a.taps[tick]=(uint8_t)tap;fd_step(&a.g,&a.s);
      if(a.g.phase==FD_OVER)continue;
      bool touch=false;unsigned nearest=0;float x=1000;
      for(unsigned j=0;j<FD_HOOPS;++j) {
        if(a.g.hoop[j].state!=2&&a.g.hoop[j].touched)touch=true;
        if(a.g.hoop[j].state!=2&&a.g.hoop[j].x<x)x=a.g.hoop[j].x,nearest=j;
      }
      if(touch)continue;
      if(a.g.baskets>g->baskets) {
        if(best<0||a.g.flaps<layer[(tick+1)&1][best].g.flaps) {
          if(best<0) {if(n==BEAM)continue;best=(int)n++;}
          layer[(tick+1)&1][best]=a;
        }
        continue;
      }
      int yy=(int)((a.g.y-47)*2),vy=(int)((a.g.vy+5)*4);
      if(yy<0||yy>=320||vy<0||vy>=44)continue;
      unsigned cell=(unsigned)((yy*44+vy)*2+(a.g.hoop[nearest].state==1));
      int old=seen[cell];
      if(old>=0&&layer[(tick+1)&1][old].g.flaps<=a.g.flaps)continue;
      if(old<0) {if(n==BEAM)continue;old=(int)n++;seen[cell]=old;}
      layer[(tick+1)&1][old]=a;
    }
    if(best>=0) {
      node *a=&layer[(tick+1)&1][best];
      *g=a->g;*s=a->s;chosen_ticks=tick+1;memcpy(taps,a->taps,chosen_ticks);
      return true;
    }
    count=n;
  }
  return false;
}
int main(void) {
  fd_game g;fd_stats s={0};fd_new(&g,73);fd_flap(&g,&s);
  FILE *f=fopen("flight-taps.txt","w");assert(f);fprintf(f,"0\n");
  unsigned elapsed=0;
  while(g.phase==FD_RUNNING&&g.baskets<50) {
    unsigned basket=g.baskets;
    bool reachable=next(&g,&s);
    if(!reachable)printf("FAILED basket %u y %.1f flaps %u\n",basket,g.y,(unsigned)g.flaps);
    assert(reachable&&fd_valid(&g,&s));
    for(unsigned i=0;i<chosen_ticks;++i)if(taps[i])fprintf(f,"%u\n",elapsed+i);
    elapsed+=chosen_ticks;
  }
  fclose(f);
  assert(g.baskets==50&&g.phase==FD_RUNNING);
  printf("Endless: %u baskets, %u swishes, score %u, %u flaps, %.1f seconds. Normal flap input only.\n",
         (unsigned)g.baskets,(unsigned)g.swishes,(unsigned)g.score,(unsigned)g.flaps,elapsed*.016);
  return 0;
}
