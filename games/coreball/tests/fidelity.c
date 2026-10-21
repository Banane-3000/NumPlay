#include "game.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
int main(void){FILE*f=fopen("reference.bin","rb");assert(f);float worst=0;for(unsigned level=0;level<60;level++){Game g;game_new(&g,level);for(unsigned frame=1;frame<=1100;frame++){g.count=0;g.remaining=26;for(unsigned i=0;i<6;i++)g.flights[i]=0;game_step(&g,frame%137==0);float expected;assert(fread(&expected,4,1,f)==1);float error=fabsf(remainderf(g.angle-expected,360));if(error>worst)worst=error;if(error>.03f){printf("Mismatch level %u frame %u: %.5f vs %.5f\n",level+1,frame,g.angle,expected);return 1;}}}fclose(f);printf("PASS: 66000 source motion samples; worst angular error %.5f degrees\n",worst);return 0;}
