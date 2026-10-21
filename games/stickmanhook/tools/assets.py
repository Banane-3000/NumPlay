#!/usr/bin/env python3
"""Generate ROM tables and packed original artwork; no network or runtime decoding."""
import json
import math
from pathlib import Path
from PIL import Image
ROOT=Path(__file__).resolve().parents[1]
A=ROOT/'assets'
def read(n):return json.loads((A/(n+'.json')).read_text())
def cf(x):
    s=format(x,'.9g')
    if '.' not in s and 'e' not in s:s+='.0'
    return s+'f'
def row(v):return '{'+','.join(cf(x) if isinstance(x,float) else str(x) for x in v)+'}'
def rgb(v):return (v[0]>>3)<<11|(v[1]>>2)<<5|(v[2]>>3)
def main():
    d=read('levels');shapes=read('geometry');names=list(shapes)
    assert [len(d['maps'][k]) for k in ['165','175','207','251']]==[9,31,43,31]
    assert len(d['recipes'])==30 and len(names)==6
    assert sum(max(len(p['hooks'])+len(p['bumpers'])+len(p.get('obstacles',[])) for p in d['maps'][k]) for k in ['175','207','251'])+4<=192
    campaign=read('campaign')
    assert len(campaign)==50 and all(len(c) in (1,3) for c in campaign)
    used={name for c in campaign for name in c}
    hooks=[];bumpers=[];obstacles=[];parts=[];lookup={};profiles=[];rotations=[]
    profile_ids={};rotation_ids={}
    for family in ['165','175','207','251']:
        for p in d['maps'][family]:
            if p['file'] not in used:continue
            lookup[p['file']]=len(parts)
            parts.append(row([float(p['spawnPoint']['x']),float(p['spawnPoint']['y']),float(p['finishLine']),len(hooks),len(bumpers),len(obstacles),len(p['hooks']),len(p['bumpers']),len(p.get('obstacles',[])),int(p.get('background','background_0')[-1])]))
            hooks += [row([float(v['x']),float(v['y'])]) for v in p['hooks']]
            for v in p['bumpers']:
                b=v.get('bounce') or {};r=v.get('rotation',0)
                profile=row([float(v.get('w',.15)),float(v.get('h',.04)),math.cos(r),math.sin(r),float(b.get('bounce') or 1.2),float(b.get('force',0)),float(b.get('angle',0)),int('angle' in b)])
                if profile not in profile_ids:profile_ids[profile]=len(profiles);profiles.append(profile)
                bumpers.append(row([float(v['x']),float(v['y']),profile_ids[profile]]))
            for v in p.get('obstacles',[]):
                r=v.get('rotation') or 0;rotation=row([math.cos(r),math.sin(r)])
                if rotation not in rotation_ids:rotation_ids[rotation]=len(rotations);rotations.append(rotation)
                obstacles.append(row([float(v['x']),float(v['y']),rotation_ids[rotation],names.index(v['name'])]))
    assert len(parts)<255 and len(profiles)<256 and len(rotations)<256
    assert len(hooks)<65536 and len(bumpers)<65536 and len(obstacles)<65536
    (ROOT/'src/levels_meta.h').write_text('/* Generated reachable course data. */\n#define SH_PART_COUNT '+str(len(parts))+'u\n')
    (ROOT/'src/levels_index.json').write_text(json.dumps(list(lookup))+'\n')
    edges=[];shape=[]
    for name in names:
        first=len(edges);radius=0
        for poly in shapes[name]:
            pts=[((poly['shape'][i]-256)*.0005,(poly['shape'][i+1]-256)*.0005) for i in range(0,len(poly['shape']),2)]
            for i,(x,y) in enumerate(pts):
                x2,y2=pts[(i+1)%len(pts)];dx=x2-x;dy=y2-y;l=math.hypot(dx,dy)
                if l:edges.append(row([x,y,dx,dy,-dy/l,dx/l,1/(l*l)]))
                radius=max(radius,math.hypot(x,y))
        shape.append(row([first,len(edges)-first,radius]))
    recipes=[row([lookup[name] for name in course]+[255]*(3-len(course))) for course in campaign]
    skins=read('skins')
    out=['/* Generated from the supplied game; do not edit. */','#include "game.h"']
    for typ,name,vals in [('sh_vec','sh_hooks',hooks),('sh_bumper','sh_bumpers',bumpers),('sh_bumper_profile','sh_bumper_profiles',profiles),('sh_obstacle','sh_obstacles',obstacles),('sh_vec','sh_rotations',rotations),('sh_part','sh_parts',parts),('sh_edge','sh_edges',edges),('sh_shape','sh_shapes',shape)]:
        out.append('const '+typ+' '+name+'[]={\n'+',\n'.join(vals)+'\n};')
    out.append('const uint8_t sh_courses[SH_LEVEL_COUNT][3]={'+','.join(recipes)+'};')
    unlocks=read('unlocks');assert len(unlocks)==23 and max(unlocks)<=50 and unlocks[0]==0
    out.append('const uint16_t sh_unlocks[SH_SKINS]={'+','.join(str(u) for u in unlocks)+'};')
    out.append('const char sh_skin_names[SH_SKINS][16]={'+','.join(json.dumps(s['name']) for s in skins)+'};')
    (ROOT/'src/levels.c').write_text('\n'.join(out)+'\n')
    # One 16-colour RGB565 palette per sprite; index zero is transparent.
    pixels=bytearray();palettes=[];descs=[];meta=read('sprites');palette_offsets={};pixel_offsets={}
    for s in meta:
        im=Image.open(A/'sprites'/(s['name']+'.png')).convert('RGBA');opaque=im.convert('RGB').quantize(colors=15,method=Image.Quantize.MEDIANCUT,dither=Image.Dither.NONE)
        assert 0<im.width<256 and 0<im.height<256 and -128<=s['x']<=127 and -128<=s['y']<=127
        pal=opaque.getpalette();pal += [0]*(45-len(pal));palette=[0]+[rgb(pal[i:i+3]) for i in range(0,45,3)]
        values=[v+1 if a>=100 else 0 for v,a in zip(opaque.tobytes(),im.getchannel('A').tobytes())]
        if s['name']=='finishlight':
            palette=[0]+[65535]*15
            values=[(a*15+127)//255 for a in im.getchannel('A').tobytes()]
        if s['name']!='finishlight':
            colors=sorted({palette[v] for v in values if v})
            remap={color:i+1 for i,color in enumerate(colors)}
            values=[remap[palette[v]] if v else 0 for v in values]
            palette=[0]+colors
        key=tuple(palette)
        if key not in palette_offsets:palette_offsets[key]=len(palettes);palettes+=palette
        packed=bytes(values[i]<<4|(values[i+1] if i+1<len(values) else 0) for i in range(0,len(values),2))
        if packed not in pixel_offsets:pixel_offsets[packed]=len(pixels);pixels+=packed
        descs.append(row([pixel_offsets[packed],palette_offsets[key],im.width,im.height,s['x'],s['y']]))
    lines=['/* Generated packed artwork from assets/sprites. */','#ifndef SH_ASSETS_H','#define SH_ASSETS_H',
           'typedef struct {uint32_t offset;uint16_t palette;uint8_t w,h;int8_t x,y;} sh_sprite;']
    for i,s in enumerate(meta):lines.append('#define SPR_'+s['name'].upper()+' '+str(i))
    lines+=['extern const sh_sprite sh_sprites[];','extern const uint8_t sh_pixels[];','extern const uint16_t sh_palettes[];','extern const uint16_t sh_backgrounds[2][240];','extern const uint16_t sh_skin_first[SH_SKINS];','extern const uint8_t sh_skin_dance[SH_SKINS];','#endif']
    (ROOT/'src/assets.h').write_text('\n'.join(lines)+'\n')
    out=['#include "game.h"','#include "assets.h"','const sh_sprite sh_sprites[]={'+','.join(descs)+'};']
    for typ,name,vals in [('uint8_t','sh_pixels',pixels),('uint16_t','sh_palettes',palettes)]:
        out.append('const '+typ+' '+name+'[]={\n'+',\n'.join(','.join(str(v) for v in vals[i:i+32]) for i in range(0,len(vals),32))+'\n};')
    out.append('const uint16_t sh_backgrounds[2][240]={'+','.join(row([rgb(c) for c in bg]) for bg in read('backgrounds'))+'};')
    out.append('const uint16_t sh_skin_first[SH_SKINS]='+row([s['first'] for s in skins])+';')
    out.append('const uint8_t sh_skin_dance[SH_SKINS]='+row([s['dance'] for s in skins])+';')
    (ROOT/'src/assets.c').write_text('\n'.join(out)+'\n')
    im=Image.open(A/'font.png');rows=[];mapping=[];glyphs={}
    for i in range(95):
        x=(i%16)*16;y=(i//16)*16
        glyph=tuple(sum(((im.getpixel((x+c,y+r))*3+127)//255)<<(c*2) for c in range(16)) for r in range(16))
        if glyph not in glyphs:glyphs[glyph]=len(rows);rows.append(row(glyph))
        mapping.append(glyphs[glyph])
    (ROOT/'src/font.h').write_text('/* JUNEGULL glyphs from the supplied game, 14px. */\nstatic const uint32_t font_rows['+str(len(rows))+'][16]={'+','.join(rows)+'};\nstatic const uint8_t font_map[95]='+row(mapping)+';\nstatic const uint8_t font_widths[95]='+row(read('font-widths'))+';\n')
    print(f'{len(parts)} parts, {len(recipes)} recipes, {len(meta)} sprites, {len(pixels)} packed pixel bytes')
if __name__=='__main__':main()
