#!/usr/bin/env python3
"""Reproducible compact assets; no web engine is shipped in the application."""
import json
from pathlib import Path
from PIL import Image
ROOT=Path(__file__).resolve().parents[1]
def rgb(c):return ((c[0]>>3)<<11)|((c[1]>>2)<<5)|(c[2]>>3)
def arr(f,typ,name,values):
    f.write('const '+typ+' '+name+'[]={\n')
    for i in range(0,len(values),24):f.write(','.join(str(x) for x in values[i:i+24])+',\n')
    f.write('};\n')
def image(f,name,im):
    im=im.convert('RGBA'); colors=im.convert('RGB').quantize(colors=15,method=Image.Quantize.MEDIANCUT)
    pal=colors.getpalette()+[0]*768; palette=[0]+[rgb(pal[i*3:i*3+3]) for i in range(15)]
    p=[0 if a<96 else c+1 for c,(_,_,_,a) in zip(colors.getdata(),im.getdata())]
    arr(f,'uint16_t',name+'_pal',palette)
    arr(f,'uint8_t',name+'_pix',[(p[i]<<4)|(p[i+1] if i+1<len(p) else 0) for i in range(0,len(p),2)])
    f.write('const Image '+name+'={'+str(im.width)+','+str(im.height)+','+name+'_pix,'+name+'_pal};\n')
def font(f):
    fonts=json.loads((ROOT/'assets/ui-fonts.json').read_text(encoding='utf8'))
    for i,font in enumerate(fonts):
        arr(f,'uint8_t',f'font{i}_pixels',list(bytes.fromhex(font['pixels'])))
        f.write(f'const Glyph font{i}_glyphs[]={{\n')
        for glyph in font['glyphs']:f.write('{'+','.join(map(str,glyph))+'},\n')
        f.write('};\n')
    f.write('const Font fonts[]={'+','.join('{font'+str(i)+'_glyphs,font'+str(i)+'_pixels}' for i in range(3))+'};\n')
def main():
    with (ROOT/'src/assets.c').open('w',encoding='utf8',newline='\n') as f:
        f.write('#include "assets.h"\n');font(f)
        catalog=json.loads((ROOT/'assets/catalog.json').read_text())
        entries=[]
        for i,b in enumerate(catalog['balls']):
            name='ball'+str(i);image(f,name,Image.open(ROOT/'assets/sprites'/('B_'+b['name'].replace(' ','_')+'.png')));entries.append('&'+name)
        f.write('const Image *const balls[]={'+','.join(entries)+'};\n')
        for name in ('PLAY','Shop','Score_Coin','crown','Pause','Balls'):
            image(f,'PlayIcon' if name=='PLAY' else name,Image.open(ROOT/'assets/sprites'/(name+'.png')))
        f.write('const BallStyle ball_styles[]={\n')
        for b in catalog['balls']:
            t=b['trailData'];cols=[rgb(tuple(bytes.fromhex(c[1:]))) for c in t['colors']]
            f.write('{.name='+json.dumps(b['name'].replace('_',' '))+',.price='+str(b['price'])+',.colors={'+','.join(map(str,cols))+'},.rotates='+str(int(b['rotates']))+',.type='+str(t['gradient_type'])+',.count='+str(len(cols))+',.height='+str(t.get('height',56))+',.offset_x='+str(t.get('gradient_offsetX',0))+',.offset_y='+str(round(t.get('offsetY',0)*100))+'},\n')
        f.write('};\nconst Landscape landscapes[]={\n')
        for b in catalog['landscapes']:
            f.write('{'+json.dumps(b['name'])+','+str(b['price'])+','+','.join(str(rgb(tuple(bytes.fromhex(b[k][1:])))) for k in ['surfaceColor','lineColor','groundColor'])+'},\n')
        f.write('};\n')
        for i,scene in enumerate(catalog['landscapes']):
            original=scene['original_id']
            image(f,'earth'+str(i),Image.open(ROOT/f'assets/earth{original}.png'))
            im=Image.open(ROOT/f'assets/background{original}.png').convert('RGB').quantize(colors=32,method=Image.Quantize.MEDIANCUT,dither=Image.Dither.NONE)
            pal=im.getpalette()+[0]*768;arr(f,'uint16_t',f'bg{i}_pal',[rgb(pal[j*3:j*3+3]) for j in range(32)])
            data=[];offsets=[];px=list(im.getdata())
            for y in range(240):
                offsets.append(len(data));row=px[y*460:(y+1)*460];x=0
                while x<460:
                    end=x+1
                    while end<460 and row[end]==row[x] and end-x<128:end+=1
                    if end-x>=3:data.extend([127+end-x,row[x]]);x=end
                    else:
                        start=x;x=end
                        while x<460 and x-start<128:
                            if x+2<460 and row[x]==row[x+1]==row[x+2]:break
                            x+=1
                        data.extend([x-start-1]+row[start:x])
            arr(f,'uint32_t',f'bg{i}_offset',offsets);arr(f,'uint8_t',f'bg{i}_rle',data)
        f.write('const Image *const earths[]={'+','.join('&earth'+str(i) for i in range(len(catalog['landscapes'])))+'};\n')
        f.write('const Background backgrounds[]={'+','.join('{bg'+str(i)+'_rle,bg'+str(i)+'_pal,bg'+str(i)+'_offset}' for i in range(len(catalog['landscapes'])))+'};\n')
        legacy=json.loads((ROOT/'assets/legacy-shop.json').read_text())
        arr(f,'uint8_t','original_ball_ids',[b['original_id'] for b in catalog['balls']])
        arr(f,'uint16_t','legacy_ball_prices',legacy['ball_prices']);arr(f,'uint16_t','legacy_scene_prices',legacy['scene_prices'])
if __name__=='__main__':main()
