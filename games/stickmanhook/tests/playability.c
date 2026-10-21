/* Search real one-button input sequences; no teleporting, physics changes or cheats. */
#include "game.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define BEAM 96
#define DECISIONS 520
#define STRIDE 6
typedef struct {sh_game g;float score;uint8_t path[DECISIONS];} node;
static node beam[BEAM],candidates[BEAM*2];
static int order(const void *a,const void *b){float x=((const node *)a)->score,y=((const node *)b)->score;return x<y?1:x>y?-1:0;}
static bool equivalent(const sh_game *a,const sh_game *b){return a->held==b->held&&a->hooked==b->hooked&&
  (int)(a->x*45)==(int)(b->x*45)&&(int)(a->y*35)==(int)(b->y*35)&&
  (int)(a->vx*8)==(int)(b->vx*8)&&(int)(a->vy*8)==(int)(b->vy*8);}
int main(void){
  unsigned pass=0;
  for(unsigned level=0;level<SH_LEVEL_COUNT;++level){
    memset(beam,0,sizeof beam);sh_new(&beam[0].g,level,1234);unsigned count=1;bool won=false;
    for(unsigned step=0;step<DECISIONS&&!won;++step){
      unsigned n=0;
      for(unsigned j=0;j<count&&!won;++j)for(unsigned action=0;action<2&&!won;++action){
        node *c=&candidates[n];*c=beam[j];c->path[step]=(uint8_t)action;
        for(unsigned k=0;k<STRIDE&&!c->g.completed;++k)sh_step(&c->g,action!=0);
        if(c->g.attempt!=1||!sh_valid(&c->g))continue;
        if(c->g.completed){
          char name[64];snprintf(name,sizeof name,"replay-%02u.bin",level);FILE *f=fopen(name,"wb");if(!f)return 2;
          fwrite(c->path,1,step+1,f);fclose(f);++pass;won=true;break;}
        c->score=c->g.x-.09f*fabsf(c->g.y-.48f)+.09f*c->g.vx-.025f*fmaxf(c->g.vy,0);++n;
      }
      qsort(candidates,n,sizeof(node),order);count=0;
      for(unsigned i=0;i<n&&count<BEAM;++i){bool duplicate=false;for(unsigned j=0;j<count;++j)if(equivalent(&candidates[i].g,&beam[j].g)){duplicate=true;break;}
        if(!duplicate)beam[count++]=candidates[i];}
      if(!count)break;
    }
    printf("level %u: %s\n",level+1,won?"PASS":"UNSOLVED");fflush(stdout);
  }
  printf("%u / 50 courses completed using only hold/release\n",pass);return pass==SH_LEVEL_COUNT?0:1;
}
