#include "game.h"
#include <math.h>
#include <string.h>
#include <stdlib.h>

uint32_t game_varint(uint32_t *at,uint32_t end,bool *ok){
    uint32_t v=0;for(unsigned shift=0;shift<28;shift+=7){if(*at>=end){*ok=false;return 0;}unsigned b=note_bytes[(*at)++];v|=(b&127u)<<shift;if(!(b&128))return v;}*ok=false;return 0;
}
/* Alias before edge detection: using both keys for one lane never double-taps. */
uint8_t game_lanes(uint64_t k){return ((k&((UINT64_C(1)<<0)|(UINT64_C(1)<<30)))?1:0)|((k&((UINT64_C(1)<<2)|(UINT64_C(1)<<31)))?2:0)|((k&((UINT64_C(1)<<1)|(UINT64_C(1)<<33)))?4:0)|((k&((UINT64_C(1)<<3)|(UINT64_C(1)<<34)))?8:0);}
void game_init(Game*g){memset(g,0,sizeof(*g));g->p.beat=1;}
unsigned game_chart(unsigned song,unsigned variant,unsigned difficulty){
    if(song>=SONG_COUNT)return NONE;const Song*s=&songs[song];
    for(unsigned t=s->first;t<s->first+s->count;t++)if(tracks[t].variant==variant){const Track*r=&tracks[t];for(unsigned c=r->first;c<r->first+r->count;c++)if(charts[c].difficulty==difficulty)return c;}return NONE;
}
static void animate(Game*g,unsigned actor,unsigned animation,int32_t until){
    if(actor>=6||g->actors[actor].character==NONE||animation>=ANIMATION_COUNT)return;
    g->actors[actor].animation=animation;g->actors[actor].since=g->time;g->actors[actor].until=until;
}
static void sing(Game*g,const Note*n,bool miss){
    if(n->kind==KIND_NOANIM)return;
    for(unsigned a=0;a<6;a++){Actor*r=&g->actors[a];if(r->character==NONE||r->side!=(n->direction<4))continue;
        const Character*c=&characters[r->character];unsigned anim=miss?c->miss[n->direction&3]:kind_anim[r->character][n->kind][n->direction&3];
        int32_t duration=(int32_t)(60000.0f/g->bpm*c->sing_time*.25f*16);int32_t until=g->time+duration;if(!miss&&n->end>until)until=n->end;animate(g,a,anim,until);
    }
}
static void fill(Game*g){
    while(g->read<g->end&&g->stream_time<=g->time+40000){
        unsigned slot=0;for(;slot<NOTE_CAP;slot++)if(g->notes[slot].state==FINISHED)break;
        if(slot==NOTE_CAP){g->fault=g->lost=1;return;}
        bool ok=true;uint32_t at=g->read;uint32_t delta=game_varint(&at,g->end,&ok),info=game_varint(&at,g->end,&ok);uint32_t length=(info&1)?game_varint(&at,g->end,&ok):0;
        if(!ok||delta>10000000||length>10000000||(info>>4)>=KIND_COUNT){g->fault=g->lost=1;return;}
        g->stream_time+=(int32_t)delta;Note*n=&g->notes[slot];*n=(Note){g->stream_time,g->stream_time+(int32_t)length,(info>>1)&7,info>>4,WAITING};g->read=at;g->decoded++;g->count++;if(g->count>g->peak)g->peak=g->count;
    }
}
bool game_start(Game*g,unsigned id){
    if(id>=CHART_COUNT)return false;Progress p=g->p;memset(g,0,sizeof(*g));g->p=p;g->chart=id;
    const Chart*c=&charts[id];const Track*t=&tracks[c->track];g->read=c->offset;g->end=c->offset+c->bytes;g->tempo=t->tempo_first;g->bpm=tempos[g->tempo].bpm;g->time=-(int32_t)(240000/g->bpm*16);g->last_hit=g->judgement_at=-10000000;
    g->health=1;g->speed=g->speed_from=g->speed_to=c->speed;g->zoom=1;g->bop=1;g->event=t->event_first;
    for(unsigned i=0;i<NOTE_CAP;i++)g->notes[i].state=FINISHED;
    for(unsigned a=0;a<6;a++){Actor*r=&g->actors[a];r->character=t->character[a];r->visible=1;r->side=a==0;if(r->character!=NONE){const Character*q=&characters[r->character];r->animation=q->idle;memcpy(r->icon,q->icon,sizeof(r->icon));}}
    if(t->character[3]!=NONE){g->actors[2].side=0;g->actors[3].side=1;g->actors[0].side=0;g->actors[4].visible=g->actors[5].visible=0;}
    fill(g);return !g->fault;
}
int game_judge(int32_t error){int32_t e=abs(error);return e<=45*16?SICK:e<=90*16?GOOD:e<=135*16?BAD:e<=160*16?SHIT:MISS;}
int game_points(int32_t error){float e=abs(error)/16.0f;if(e>160)return -100;if(e<5)return 500;return (int)(500*(1-1/(1+expf(-.080f*(e-54.99f))))+9);}
static void retire(Game*g,Note*n){n->state=FINISHED;if(g->count)g->count--;}
static void miss_note(Game*g,Note*n){
    n->state=MISSED;sing(g,n,true);if(n->kind==KIND_NON_SCOREABLE)return;
    g->judged[MISS]++;g->combo=0;g->health-=.08f;g->score_milli-=100000;g->judgement=MISS;g->judgement_at=g->time;
    if(n->end-g->time>WINDOW)g->score_milli-=(int64_t)(n->end-g->time)*125000/16000;
}
static void hit_note(Game*g,Note*n){
    sing(g,n,false);n->state=n->end>n->time?HOLDING:FINISHED;if(n->state==FINISHED&&g->count)g->count--;
    if(n->kind==KIND_NON_SCOREABLE)return;
    int j=game_judge(g->time-n->time);g->judged[j]++;g->score_milli+=(int64_t)game_points(g->time-n->time)*1000;g->health+=j==SICK?.03f:j==GOOD?.015f:j==BAD?0:-.02f;
    if(j<=GOOD){g->combo++;if(g->combo>g->max_combo)g->max_combo=g->combo;}else g->combo=0;
    g->judgement=(uint8_t)j;g->judgement_at=g->last_hit=g->time;
}
static unsigned find_animation(unsigned character,unsigned name){
    const Character*c=&characters[character];for(unsigned a=c->first;a<c->first+c->count;a++)if(animations[a].name==name)return a;return NONE;
}
static void event(Game*g,const Event*e){
    switch(e->kind){
    case EVENT_FOCUSCAMERA:g->focus=(uint16_t)e->a;break;
    case EVENT_ZOOMCAMERA:g->zoom=fminf(1.15f,fmaxf(.8f,e->value));break;
    case EVENT_SETCAMERABOP:g->bop=e->value;break;
    case EVENT_PLAYANIMATION:if((unsigned)e->b<6&&g->actors[e->b].character!=NONE)animate(g,e->b,find_animation(g->actors[e->b].character,(uint16_t)e->a),g->time+(int32_t)(960000/g->bpm));break;
    case EVENT_SCROLLSPEED:g->speed_from=g->speed;g->speed_to=e->a?e->value:charts[g->chart].speed*e->value;g->speed_start=g->time;g->speed_end=g->time+(int32_t)(e->duration*960000/g->bpm);break;
    case EVENT_SETHEALTHICON:if((unsigned)e->a<2){g->actors[e->a].icon[0]=(uint16_t)e->b;g->actors[e->a].icon[1]=(uint16_t)e->c;}break;
    case EVENT_SSERAFIMSING:{static const uint8_t map[6]={3,1,4,5,0,2};for(unsigned i=0;i<6;i++)g->actors[map[i]].side=(e->a>>i)&1;break;}
    case EVENT_SSERAFIMSHOW:{static const uint8_t map[5]={3,1,4,5,0};for(unsigned i=0;i<5;i++)g->actors[map[i]].visible=(e->a>>i)&1;break;}
    case EVENT_SSERAFIMDARK:g->dark=e->value;break;
    case EVENT_SSERAFIMFLASH:g->flash_until=g->time+(int32_t)(e->duration*16000);break;
    case EVENT_SSERAFIMPULSELIGHTS:g->pulse=e->a!=0;break;
    case EVENT_SSERAFIMBEAUTIFUL:g->beautiful=e->a!=0;break;
    case EVENT_SSERAFIMCOVER:g->cover=e->a!=0;break;
    case EVENT_SSERAFIMLIGHTS:g->bop=e->value;break;
    case EVENT_SSERAFIMKICK:case EVENT_SSERAFIMGUITARVIBRATION:g->flash_until=g->time+4000;break;
    case EVENT_ENABLEMASK:case EVENT_SSERAFIMEND:break;
    }
}
unsigned game_completion(const Game*g){int n=(int)g->judged[SICK]+g->judged[GOOD]-g->judged[MISS];return n>0?(unsigned)((uint32_t)n*10000/charts[g->chart].players):0;}
const char*game_rank(const Game*g){unsigned c=game_completion(g);if(g->judged[SICK]==charts[g->chart].players)return "PERFECT GOLD";return c==10000?"PERFECT":c>=9000?"EXCELLENT":c>=8000?"GREAT":c>=6000?"GOOD":"SHIT";}
static void clear(Game*g){
    g->won=1;Record*r=&g->p.record[g->chart];int32_t s=(int32_t)(g->score_milli/1000);unsigned c=game_completion(g);
    if(s>r->score)r->score=s;if(c>r->completion)r->completion=(uint16_t)c;if(g->max_combo>r->combo)r->combo=g->max_combo;g->dirty=1;
}
static void tick(Game*g,int32_t time,uint8_t held){
    int32_t previous=g->time;g->time=time;const Track*t=&tracks[charts[g->chart].track];
    while(g->tempo+1<t->tempo_first+t->tempo_count&&tempos[g->tempo+1].time<=time)g->tempo++;
    const Tempo*tempo=&tempos[g->tempo];g->bpm=tempo->bpm;g->beat=tempo->beat+(time-tempo->time)*g->bpm/960000;
    while(g->event<t->event_first+t->event_count&&events[g->event].time<=time)event(g,&events[g->event++]);
    if(time>=g->speed_end)g->speed=g->speed_to;else if(time>=g->speed_start&&g->speed_end>g->speed_start){float x=(float)(time-g->speed_start)/(g->speed_end-g->speed_start);x=x*x*(3-2*x);g->speed=g->speed_from+(g->speed_to-g->speed_from)*x;}
    fill(g);if(g->fault)return;
    bool near=false,active_hold=false;uint8_t pressed=held&~g->held;
    for(unsigned i=0;i<NOTE_CAP;i++){Note*n=&g->notes[i];if(n->state==FINISHED)continue;
        if(n->direction>=4){if(n->state==WAITING&&time>=n->time){sing(g,n,false);n->state=HOLDING;}if(time>=n->end)retire(g,n);continue;}
        if(n->state==WAITING&&abs(time-n->time)<=WINDOW)near=true;
        if(n->state==HOLDING){active_hold=true;int32_t start=previous>n->time?previous:n->time,end=time<n->end?time:n->end;
            if((g->held&(1u<<n->direction))&&end>start&&n->kind!=KIND_NON_SCOREABLE){g->score_milli+=(int64_t)(end-start)*250000/16000;g->health+=(end-start)*(.12f/16000);}
            if(time>=n->end){retire(g,n);continue;}
            if(!(held&(1u<<n->direction))){if(n->end-time>WINDOW&&n->kind!=KIND_NON_SCOREABLE){g->score_milli-=(int64_t)(n->end-time)*125000/16000;g->combo=0;sing(g,n,true);}n->state=MISSED;}
        }
        if(n->state==WAITING&&time>n->time+WINDOW)miss_note(g,n);
        if(n->state==MISSED){if(n->end>time)active_hold=true;if(time>n->end+WINDOW)retire(g,n);}
    }
    for(unsigned lane=0;lane<4;lane++)if(pressed&(1u<<lane)){
        Note*best=NULL;for(unsigned i=0;i<NOTE_CAP;i++){Note*n=&g->notes[i];if(n->direction==lane&&n->state==WAITING&&abs(time-n->time)<=WINDOW&&(!best||n->time<best->time))best=n;}
        if(best){int32_t head=best->time;hit_note(g,best);/* Same-lane stacked notes are one press, as in the original. */
            for(unsigned i=0;i<NOTE_CAP;i++){Note*n=&g->notes[i];if(n->direction==lane&&n->state==WAITING&&abs(n->time-head)<16)hit_note(g,n);}}
        else if(!g->p.ghost||near||active_hold||time-g->last_hit<6000){if(time>=0){g->score_milli-=10000;g->health-=.08f;g->judgement=MISS;g->judgement_at=time;Note fake={.direction=(uint8_t)lane};sing(g,&fake,true);}}
    }
    g->held=held;if(g->health>2)g->health=2;
    if(g->health<=0){g->health=0;g->lost=1;return;}
    if(time>=charts[g->chart].end)clear(g);
}
void game_advance(Game*g,int32_t time,uint8_t held){
    if(time<g->time||g->won||g->lost)return;
    while(time-g->time>256&&!g->won&&!g->lost)tick(g,g->time+256,g->held);
    if(!g->won&&!g->lost)tick(g,time,held);
}
void game_set_held(Game*g,uint8_t held){g->held=held;}
