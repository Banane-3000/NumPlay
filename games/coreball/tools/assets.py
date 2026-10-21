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
        for name,file in [('logo','title.png'),('go','go.png')]:
            im=Image.open(ROOT/'assets'/file).convert('L');p=[(v+8)//17 for v in im.getdata()]
            arr(f,'uint8_t',name+'_pix',[(p[i]<<4)|(p[i+1] if i+1<len(p) else 0) for i in range(0,len(p),2)])
            f.write('const Mask '+name+'={'+str(im.width)+','+str(im.height)+','+name+'_pix};\n')
        levels=json.loads((ROOT/'assets/levels.json').read_text());motions=['A1','A2','B1','B2','C1','C2','D1','D2','D3','D4','E1','E2']
        f.write('const Level levels[60]={\n'+',\n'.join('{'+str(int(v[0]))+','+str(v[1])+','+str(motions.index(v[2]))+'}' for v in levels.values())+'};\n')
        presets={0:[],2:[0,180],3:[0,120,240],4:[0,90,180,270],5:[0,72,144,216,288],6:list(range(0,360,60)),7:[0,52,103,155,206,258,309],8:list(range(0,360,45)),9:list(range(0,360,40)),10:list(range(0,360,36)),11:[0,33,66,99,131,164,197,230,262,295,328],12:list(range(0,360,30)),13:[0,28,56,84,111,139,167,194,222,250,277,305,333],14:[0,26,52,78,103,129,155,180,206,232,258,283,309,335],15:list(range(0,360,24)),16:[0,23,45,68,90,113,135,158,180,203,225,248,270,293,315,338]}
        offsets=[];angles=[]
        for i in range(17):offsets.append(len(angles));angles+=presets.get(i,[])
        arr(f,'uint8_t','preset_offset',offsets);arr(f,'uint16_t','preset_angle',angles)
if __name__=='__main__':main()
