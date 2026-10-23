#include "game.h"
#include "save.h"
#include "render.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <math.h>
static Game g;
static bool dense_fixture,crowded_fixture;
typedef struct { int32_t time; uint8_t lane,on; } Press;
static Press presses[10000];
static uint16_t screen[320*240];
static unsigned poll_count,push_count;
static void sample_input(void){poll_count++;}
static void display(int x,int y,int w,int h,const uint16_t*p){push_count++;assert(x>=0&&y>=0&&x+w<=320&&y+h<=240);for(int j=0;j<h;j++)memcpy(screen+(y+j)*320+x,p+j*w,w*2);}
static void shot(const char*path){FILE*f=fopen(path,"wb");assert(f);fprintf(f,"P6\n320 240\n255\n");for(unsigned i=0;i<320*240;i++){uint16_t c=screen[i];unsigned char b[3]={(unsigned char)((c>>11)*255/31),(unsigned char)(((c>>5)&63)*255/63),(unsigned char)((c&31)*255/31)};fwrite(b,1,3,f);}fclose(f);}
static int compare(const void*a,const void*b){const Press*x=a,*y=b;return x->time!=y->time?(x->time>y->time?1:-1):(int)x->on-y->on;}
static unsigned decode(unsigned id,Note*out){const Chart*c=&charts[id];uint32_t at=c->offset,end=at+c->bytes;int32_t time=0;unsigned n=0;bool ok=true;while(at<end){time+=(int32_t)game_varint(&at,end,&ok);unsigned info=game_varint(&at,end,&ok),length=info&1?game_varint(&at,end,&ok):0;out[n++]=(Note){time,time+(int32_t)length,(info>>1)&7,info>>4,0};}assert(ok&&n==c->count);return n;}
static unsigned perfect(unsigned id){
    static Note notes[4000];unsigned n=decode(id,notes),np=0;int32_t last_release[4]={-1,-1,-1,-1};
    for(unsigned i=0;i<n;i++){Note*p=&notes[i];if(p->direction>=4)continue;unsigned d=p->direction;int32_t end=p->end+1;
        for(unsigned j=i+1;j<n;j++)if(notes[j].direction==d&&notes[j].time>p->time){if(end>=notes[j].time)end=notes[j].time-1;break;}
        if(p->time==last_release[d])continue;
        presses[np++]=(Press){p->time,(uint8_t)d,1};presses[np++]=(Press){end,(uint8_t)d,0};last_release[d]=p->time;
    }
    qsort(presses,np,sizeof(*presses),compare);assert(game_start(&g,id));uint8_t held=0;
    for(unsigned p=0;p<np;){int32_t at=presses[p].time;do{unsigned bit=1u<<presses[p].lane;if(presses[p].on)held|=bit;else held&=~bit;p++;}while(p<np&&presses[p].time==at);unsigned misses=g.judged[MISS];game_advance(&g,at,held);if(g.judged[MISS]!=misses)fprintf(stderr,"MISS chart%u at%f held%u\n",id,at/16.0,held);if(g.lost){fprintf(stderr,"chart %u lost at %.3f health %f fault %u peak %u\n",id,g.time/16.0,g.health,g.fault,g.peak);abort();}
        const char*fixture=NULL;
        if(!dense_fixture&&g.peak>=47){fixture="dense-chart.game";dense_fixture=true;}
        if(!crowded_fixture&&id==CHART_COUNT-1&&g.time>480000&&g.actors[4].visible&&g.actors[5].visible){fixture="crowded-scene.game";crowded_fixture=true;}
        if(fixture){FILE*file=fopen(fixture,"wb");assert(file);assert(fwrite(&g,1,sizeof(g),file)==sizeof(g));fclose(file);}
    }
    game_advance(&g,charts[id].end+1,0);unsigned total=0;for(unsigned i=0;i<5;i++)total+=g.judged[i];
    if(!g.won||g.lost||g.fault||g.judged[MISS]||total!=charts[id].players||g.judged[SICK]!=charts[id].players){fprintf(stderr,"chart %u: won%u lost%u fault%u judged%u expected%u sick%u miss%u decoded%lu/%u\n",id,g.won,g.lost,g.fault,total,charts[id].players,g.judged[SICK],g.judged[MISS],(unsigned long)g.decoded,charts[id].count);abort();}
    assert(game_completion(&g)==10000);assert(!strcmp(game_rank(&g),"PERFECT GOLD"));assert(g.p.record[id].completion==10000);return g.peak;
}
static void test_save(void){static uint8_t b[SAVE_BYTES],bad[SAVE_BYTES];static char backup[BACKUP_BYTES];Progress p={0},r={0};p.beat=1;p.record[0]=(Record){30000,10000,charts[0].players};save_encode(&p,b);assert(save_decode(&r,b,sizeof(b)));assert(!memcmp(&p,&r,sizeof(p)));unsigned n=save_backup(b,backup);assert(n<sizeof(backup));assert(save_restore(&r,(uint8_t*)backup,n));
    for(unsigned i=0;i<SAVE_BYTES;i++)for(unsigned j=0;j<8;j++){memcpy(bad,b,sizeof(b));bad[i]^=1u<<j;Progress before=r;assert(!save_decode(&r,bad,sizeof(bad)));assert(!memcmp(&r,&before,sizeof(r)));}
    backup[45]='x';assert(!save_restore(&r,(uint8_t*)backup,n));assert(!save_decode(&r,b,sizeof(b)-1));
}
static void test_score(void){assert(game_judge(45*16)==SICK);assert(game_judge(45*16+1)==GOOD);assert(game_judge(90*16)==GOOD);assert(game_judge(135*16)==BAD);assert(game_judge(160*16)==SHIT);assert(game_judge(160*16+1)==MISS);assert(game_points(0)==500);assert(game_points(161*16)==-100);
    for(int i=0;i<160*16;i++){assert(game_points(i)==game_points(-i));assert(game_points(i)>=game_points(i+1));}
    assert(game_lanes(UINT64_C(1)<<0)==1);assert(game_lanes(UINT64_C(1)<<2)==2);assert(game_lanes(UINT64_C(1)<<1)==4);assert(game_lanes(UINT64_C(1)<<3)==8);assert(game_lanes((UINT64_C(1)<<30)|(UINT64_C(1)<<0))==1);assert(game_lanes((UINT64_C(1)<<30)|(UINT64_C(1)<<31)|(UINT64_C(1)<<33)|(UINT64_C(1)<<34))==15);
    assert(game_start(&g,0));game_advance(&g,0,1);assert(g.score_milli==-10000);assert(g.health<1);g.p.ghost=1;assert(game_start(&g,0));game_advance(&g,0,1);assert(g.score_milli==0);g.p.ghost=0;
}
static void test_holds(void){
    for(unsigned test=0;test<2;test++){
        assert(game_start(&g,0));g.read=g.end;g.time=0;g.count=1;for(unsigned i=0;i<NOTE_CAP;i++)g.notes[i].state=FINISHED;
        g.notes[0]=(Note){16000,32000,0,KIND_NORMAL,WAITING};game_advance(&g,16000,1);assert(g.judged[SICK]==1&&g.combo==1&&g.score_milli==500000);
        int32_t release=test?30400:24000;game_advance(&g,release,1);int64_t before=g.score_milli;float hp=g.health;game_advance(&g,release,0);
        if(test){assert(g.combo==1&&g.score_milli==725000);}else{assert(g.combo==0&&before-g.score_milli==62500);}
        assert(fabsf(g.health-hp)<.00001f);int64_t dropped=g.score_milli;game_advance(&g,40000,0);assert(g.score_milli==dropped);
    }
    assert(game_start(&g,0));g.combo=3;game_advance(&g,0,1);assert(g.combo==3);/* Empty taps subtract score/health, not a chart judgement. */
}
static void test_render(void){
    Menu m={.view=TITLE,.difficulty=1};render_invalidate();poll_count=push_count=0;render(&g,&m,display,sample_input);assert(poll_count==30&&push_count==30);poll_count=push_count=0;render(&g,&m,display,sample_input);assert(poll_count==30&&!push_count);shot("title.ppm");
    for(unsigned v=0;v<=SAVE_ERROR;v++){m.view=v;for(unsigned sel=0;sel<4;sel++){m.selected=sel;render(&g,&m,display,NULL);}}
    for(unsigned s=0;s<SONG_COUNT;s++)for(unsigned v=0;v<4;v++)for(unsigned d=0;d<5;d++){unsigned c=game_chart(s,v,d);if(c==NONE)continue;assert(game_start(&g,c));m=(Menu){.view=FREEPLAY,.song=s,.variant=v,.difficulty=d};render(&g,&m,display,NULL);}
    for(unsigned w=0;w<LEVEL_COUNT;w++){m=(Menu){.view=STORY,.week=w,.difficulty=1};render(&g,&m,display,NULL);}shot("story.ppm");
    for(unsigned t=0;t<TRACK_COUNT;t++){assert(game_start(&g,tracks[t].first));m=(Menu){.view=PLAY};g.time=50000;g.beat=6.5f;g.combo=g.max_combo=9999;g.score_milli=999999000;render(&g,&m,display,NULL);char path[64];snprintf(path,sizeof(path),"stage-%u.ppm",tracks[t].stage);shot(path);}
    assert(game_start(&g,0));m=(Menu){.view=FREEPLAY,.song=0,.difficulty=1};render(&g,&m,display,NULL);shot("freeplay.ppm");
}
int main(void){
    game_init(&g);test_score();test_holds();test_save();unsigned peak=0,total=0;
    FILE*f=fopen("autoplay-report.json","w");assert(f);fprintf(f,"{\"charts\":[");
    for(unsigned i=0;i<CHART_COUNT;i++){unsigned p=perfect(i);if(p>peak)peak=p;total+=charts[i].count;fprintf(f,"%s{\"chart\":%u,\"peak_notes\":%u,\"sick\":%u,\"score\":%ld}",i?",":"",i,p,g.judged[SICK],(long)(g.score_milli/1000));}
    fprintf(f,"],\"total_notes\":%u,\"peak_notes\":%u}\n",total,peak);fclose(f);assert(dense_fixture&&crowded_fixture);
    for(unsigned i=0;i<CHART_COUNT;i++){assert(game_start(&g,i));game_advance(&g,charts[i].end+1,0);assert(g.lost&&!g.fault);assert(g.health==0);}
    for(unsigned i=0;i<100;i++){assert(game_start(&g,i%CHART_COUNT));uint32_t seed=i+1;for(int32_t t=g.time;t<charts[g.chart].end&&!g.won&&!g.lost;t+=123){seed=seed*1664525u+1013904223u;game_advance(&g,t,(seed>>24)&15);assert(g.count<=NOTE_CAP&&g.health>=0&&g.health<=2);}}
    test_render();f=fopen("host-report.json","w");assert(f);fprintf(f,"{\"charts\":%u,\"notes\":%u,\"peak_queue\":%u,\"game_size\":%u,\"progress_size\":%u,\"save_bytes\":%u,\"time\":%u,\"held\":%u,\"health\":%u,\"chart\":%u,\"won\":%u,\"lost\":%u,\"score\":%u,\"judged\":%u}\n",CHART_COUNT,total,peak,(unsigned)sizeof(Game),(unsigned)sizeof(Progress),SAVE_BYTES,(unsigned)offsetof(Game,time),(unsigned)offsetof(Game,held),(unsigned)offsetof(Game,health),(unsigned)offsetof(Game,chart),(unsigned)offsetof(Game,won),(unsigned)offsetof(Game,lost),(unsigned)offsetof(Game,score_milli),(unsigned)offsetof(Game,judged));fclose(f);
    printf("All %u charts: perfect autoplay; %u notes, peak queue %u/%u. Save, random-input and render bounds passed.\n",CHART_COUNT,total,peak,NOTE_CAP);return 0;
}
