#include "game.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
static FILE*f;
static float read_float(void){float v;assert(fread(&v,4,1,f)==1);return v;}
int main(void){f=fopen("reference.bin","rb");assert(f);float worst=0;for(unsigned i=0;i<100;i++){Terrain t={0};float a=read_float(),b=read_float(),c=read_float(),flat=read_float(),width=read_float();terrain_shape(&t,a,b,c,flat!=0);assert(fabsf(t.width-width)<.001f);for(unsigned n=0;n<=63;n++){float expected=read_float();float x=t.raw_width*n/90.f,y=terrain_surface(&t,x)-392.5f,error=fabsf(y-expected);if(error>worst)worst=error;assert(error<.001f);if(n%8==0)assert(fabsf(t.contact[n/8]-407.5f-expected)<.001f);}}
 for(unsigned i=0;i<1000;i++){Game g={0};g.vx=read_float();g.vy=read_float();g.grounded=read_float()!=0;bool held=read_float()!=0;float x=read_float(),y=read_float();game_input(&g,held);assert(fabsf(g.vx-x)<.001f&&fabsf(g.vy-y)<.001f);}fclose(f);printf("PASS: 100 original terrain curves + contact meshes (error %.7f), 1000 original control samples\n",worst);return 0;}
