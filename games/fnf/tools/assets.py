"""Pack all pinned charts and converted artwork into immutable native ROM tables."""
from pathlib import Path
import json,zlib,struct,math,hashlib,re
ROOT=Path(__file__).resolve().parents[1]
def ident(s): return re.sub('[^A-Z0-9]+','_',s.upper()).strip('_')
def q(t):return round(float(t)*16)
def leb(v):
    assert v>=0
    b=bytearray()
    while v>=128:b.append((v&127)|128);v>>=7
    b.append(v);return b
def f(x):
    x=float(x)
    assert math.isfinite(x)
    return f'{x:.8f}f'
def arr(x):return '{'+','.join(arr(y) if isinstance(y,(list,tuple)) else str(y) for y in x)+'}'
def main():
    model=json.loads(zlib.decompress((ROOT/'assets/model.json.zlib').read_bytes()));art=json.loads((ROOT/'assets/art.json').read_text());src=ROOT/'src';src.mkdir(exist_ok=True)
    kinds=model['kinds']; chars=model['characters']; charmap={c['id']:i for i,c in enumerate(chars)}; stage={s['id']:i for i,s in enumerate(model['stages'])}
    order=[s for l in model['levels'] for s in l['songs']]; diffs=['easy','normal','hard','erect','nightmare']; variants=['default','erect','pico','bf']
    tt=sorted(model['tracks'],key=lambda t:(order.index(t['id']),variants.index(t['variation'])))
    animnames=sorted({a for c in chars for a in c['anims']}); anims=[]; charrows=[]; kindrows=[]; allmaps=[]
    for c in chars:
        first=len(anims);am={}
        for name,frames in c['anims'].items():
            am[name]=len(anims);anims.append([frames+[frames[-1]]*(4-len(frames)),animnames.index(name),len(frames)])
        allmaps.append(am);idle=am.get('idle',am.get('danceLeft',first));dance=am.get('danceRight',idle)
        sing=[am.get('sing'+d,idle) for d in ['LEFT','DOWN','UP','RIGHT']];miss=[am.get('sing'+d+'miss',sing[i]) for i,d in enumerate(['LEFT','DOWN','UP','RIGHT'])]
        icons=model['icons'].get(c['icon'],model['icons']['face']);charrows.append([json.dumps(c['id']),first,len(am),idle,dance,sing,miss,[icons[0],icons[min(1,len(icons)-1)]],int(c['singTime'])])
        km=[]
        weekend={'blockhigh':'block','blocklow':'block','blockspin':'block','cockgun':'cock','dodgehigh':'dodge','dodgelow':'duck','fakeout':'fakeout','firegun':'shoot','hithigh':'hitHigh','hitlow':'hitLow','idle':'idle','kickcan':'kickCan','kneecan':'kneeCan','lightcan':'lightCan','picouppercut':'uppercut','picouppercutprep':'uppercutPrep','darnelluppercut':'uppercut','darnelluppercutprep':'uppercutPrep','punchhigh':'punchHigh1','punchhighblocked':'punchHigh2','punchhighspin':'punchHigh2','punchlow':'punchLow1','punchlowblocked':'punchLow2','punchlowdodged':'punchLow2','punchlowspin':'punchLow2','taunt':'taunt'}
        for k in kinds:
            row=[]
            for di,d in enumerate(['LEFT','DOWN','UP','RIGHT']):
                name='sing'+d
                if k=='mom':name+='-alt'
                elif k.startswith('sakura-'):name+='-'+k[7:]
                elif k=='censor':name+='-censor'
                elif k in ['ugh','hehPrettyGood']:name=k
                elif k.startswith('weekend-1-'):name=weekend.get(k[10:],name)
                row.append(65535 if k=='noanim' else am.get(name,sing[di]))
            km.append(row)
        kindrows.append(km)
    beautiful=[]
    for c,am in zip(chars,allmaps):
        for name in c['anims']:beautiful.append(am.get(name+'-beautiful',am[name]))
    eventnames=sorted({e['e'] for t in tt for e in t['chart'].get('events',[])})
    ev=[];tempos=[];trackrows=[];chartrows=[];songrows=[];notes=bytearray();streams={};manifest=[];aux=0
    for si,song in enumerate(order):
        firsttrack=len(trackrows);songtracks=[t for t in tt if t['id']==song]
        for t in songtracks:
            ti=len(trackrows);meta=t['metadata'];pd=meta['playData'];ch=t['chart'];tc=meta['timeChanges'];tf=len(tempos);beat=0;last=0;lastbpm=float(tc[0]['bpm'])
            for change in tc:
                time=float(change['t']);beat=float(change.get('b',beat+(time-last)*lastbpm/60000));last=time;lastbpm=float(change['bpm']);tempos.append([q(time),f(lastbpm),f(beat)])
            ef=len(ev)
            for e in sorted(ch.get('events',[]),key=lambda e:e['t']):
                v=e.get('v');kind=e['e'];v=v if isinstance(v,dict)else {'char':v or 0};a=b=c=0;value=0;duration=float(v.get('duration',0));
                if kind=='FocusCamera':a=int(v.get('char',0));b=round(float(v.get('x',0))/8);c=round(float(v.get('y',0))/8)
                elif kind=='ZoomCamera':value=float(v.get('zoom',1))
                elif kind=='SetCameraBop':value=float(v.get('intensity',1));a=int(v.get('rate',4))
                elif kind=='ScrollSpeed':value=float(v.get('scroll',1));a=int(v.get('absolute',False))
                elif kind=='PlayAnimation':a=animnames.index(v['anim']) if v.get('anim')in animnames else 65535;b={'boyfriend':0,'bf':0,'dad':1,'opponent':1,'girlfriend':2,'gf':2}.get(v.get('target'),0)
                elif kind=='SetHealthIcon':a=int(v.get('char',0));ic=model['icons'].get(v.get('id'),model['icons']['face']);b=ic[0];c=ic[min(1,len(ic)-1)]
                elif kind in ['sserafimSing','sserafimShow']:a=sum(1<<i for i,x in enumerate(v.get('singing',v.get('visible',[]))) if x)
                elif kind in ['sserafimDark','sserafimLights']:value=float(v.get('amount',0))
                elif kind=='sserafimBeautiful':a=int(v.get('beautiful',False))
                elif kind=='sserafimCover':a=int(v.get('visible',False))
                elif kind=='sserafimKick':a=int(v.get('final',False))
                elif kind=='sserafimPulseLights':a=int(v.get('enabled',False))
                ev.append([q(e['t']),f(value),f(duration),a,b,c,eventnames.index(kind)])
            cf=len(chartrows)
            for diff in pd['difficulties']:
                ns=ch['notes'].get(diff,[])
                if diff not in diffs or not ns:continue
                data=bytearray();prev=0;players=0
                for n in sorted(ns,key=lambda n:n['t']):
                    time=q(n['t']);length=q(n.get('l',0));kind=kinds.index(n.get('k',''));data+=leb(time-prev)+leb((kind<<4)|(int(n['d'])<<1)|bool(length));prev=time
                    if length:data+=leb(length)
                    if n['d']<4 and n.get('k','')!='non_scoreable':players+=1
                key=bytes(data)
                if key not in streams:streams[key]=len(notes);notes+=data
                off=streams[key];end=max([n['t']+n.get('l',0) for n in ns]+[e['t'] for e in ch.get('events',[])])+1200
                speed=ch.get('scrollSpeed',{}).get(diff,ch.get('scrollSpeed',{}).get('default',1))
                chartrows.append([off,len(data),q(end),len(ns),players,ti,diffs.index(diff),f(speed)])
                manifest.append(dict(chart=len(chartrows)-1,song=song,variation=t['variation'],difficulty=diff,notes=len(ns),player_notes=players,sha256=hashlib.sha256(data).hexdigest()))
            aux+=sum(len(ns) for d,ns in ch['notes'].items() if d=='picospeaker')
            ids=pd['characters'];actors=[charmap.get(ids.get(k,''),65535) for k in ['player','opponent','girlfriend']]
            actors += [charmap.get('sserafim-yunjin',65535),charmap.get('sserafim-chaewon',65535),charmap.get('sserafim-eunchae',65535)] if song=='spaghetti' else [65535]*3
            trackrows.append([si,cf,len(chartrows)-cf,stage[pd['stage']],actors,tf,len(tc),ef,len(ev)-ef,variants.index(t['variation'])])
        name=songtracks[0]['metadata']['songName'];name='SPAGHETTI' if song=='spaghetti' else name
        songrows.append([json.dumps(name),firsttrack,len(songtracks)])
    levelrows=[[json.dumps(l['name']),[order.index(s) for s in l['songs']]+[65535]*(4-len(l['songs'])),len(l['songs'])] for l in model['levels']]
    counts=dict(SPRITE_COUNT=len(art['sprites']),PALETTE_COUNT=len(art['palettes']),ART_BYTES=(ROOT/'assets/art.bin').stat().st_size,NOTE_BYTES=len(notes),ANIMATION_COUNT=len(anims),CHARACTER_COUNT=len(chars),KIND_COUNT=len(kinds),TEMPO_COUNT=len(tempos),EVENT_COUNT=len(ev),CHART_COUNT=len(chartrows),TRACK_COUNT=len(trackrows),SONG_COUNT=len(songrows),LEVEL_COUNT=len(levelrows),STAGE_COUNT=len(stage),UI_LOGO=model['ui']['logo'],UI_MENU=model['ui']['menu'])
    h=['/* Generated by tools/assets.py. */','#ifndef FNF_IDS_H','#define FNF_IDS_H']+[f'#define {k} {v}' for k,v in counts.items()]
    h += [f'#define KIND_{ident(k or "NORMAL")} {i}' for i,k in enumerate(kinds)] + [f'#define EVENT_{ident(k)} {i}' for i,k in enumerate(eventnames)]
    h += [f'#define CHAR_{ident(c["id"])} {i}' for i,c in enumerate(chars)]+['#endif','']
    (src/'ids.h').write_text('\n'.join(h),newline='\n')
    lines=['/* Generated from the pinned official assets; see ASSETS.md. */','#include "data.h"']
    def table(typ,name,rows,suffix=''):
        lines.append(f'const {typ} {name}[{len(rows)}]{suffix}={{\n'+',\n'.join(arr(r) if isinstance(r,(list,tuple))else str(r) for r in rows)+'\n};')
    table('Sprite','sprites',[[s['offset'],s['packed_size'],s['w'],s['h'],s['palette'],s['ox'],s['oy'],int(s['compressed'])]for s in art['sprites']])
    table('uint16_t','palettes',art['palettes'],'[16]');table('uint16_t','stage_sprites',[s['sprite']for s in model['stages']]);table('uint16_t','ui_notes',[[model['ui'][s+str(i)]for i in range(4)] for s in ['note','static','press','confirm']],'[4]')
    table('Animation','animations',anims);table('uint16_t','beautiful_animation',beautiful);table('Character','characters',charrows);table('uint16_t','kind_anim',kindrows,f'[{len(kinds)}][4]');table('Tempo','tempos',tempos);table('Event','events',ev);table('Chart','charts',chartrows);table('Track','tracks',trackrows);table('Song','songs',songrows);table('Level','levels',levelrows)
    lines.append('const char *const difficulties[5]={"EASY","NORMAL","HARD","ERECT","NIGHTMARE"};\nconst char *const variants[4]={"ORIGINAL","ERECT","PICO","BOYFRIEND"};')
    for name,data in [('art_pixels',(ROOT/'assets/art.bin').read_bytes()),('note_bytes',notes)]:
        lines.append(f'const uint8_t {name}[{len(data)}]={{\n'+',\n'.join(','.join(str(x)for x in data[i:i+32])for i in range(0,len(data),32))+'\n};')
    (src/'data.c').write_text('\n'.join(lines)+'\n',newline='\n')
    report=dict(counts,auxiliary_npc_notes=aux,charts=manifest,refs=model['refs']);(ROOT/'assets/chart-manifest.json').write_text(json.dumps(report,indent=2)+'\n',newline='\n')
    print(json.dumps({k:v for k,v in report.items()if k not in ['charts','refs']},indent=2))
if __name__=='__main__':main()
