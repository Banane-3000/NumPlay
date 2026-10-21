#!/usr/bin/env python3
"""Verify the sole retained Mountains background, catalog and packed rows."""
from pathlib import Path
import re,json
from PIL import Image,ImageChops,ImageStat
ROOT=Path(__file__).resolve().parents[1]
source=(ROOT/'src/assets.c').read_text()
def array(name):return list(map(int,re.search(r'\b'+name+r'\[\]=\{([^}]+)\}',source,re.S)[1].replace('\n','').strip(',').split(',')))
def rgb(c):return ((c[0]>>3)<<11)|((c[1]>>2)<<5)|(c[2]>>3)
errors=[]
catalog=json.loads((ROOT/'assets/catalog.json').read_text())
assert len(catalog['balls'])==7 and len(catalog['landscapes'])==1
assert catalog['landscapes'][0]['original_id']==1 and catalog['landscapes'][0]['name']=='Mountains'
assert len(list((ROOT/'assets').glob('background*.png')))==1
for scene in range(1):
    im=Image.open(ROOT/'assets/background1.png').convert('RGB')
    expected=im.quantize(colors=32,method=Image.Quantize.MEDIANCUT,dither=Image.Dither.NONE)
    data=array(f'bg{scene}_rle');offsets=array(f'bg{scene}_offset');pal=array(f'bg{scene}_pal');pixels=[]
    for y in range(240):
        i=offsets[y];row=[]
        while len(row)<460:
            token=data[i];i+=1
            if token>=128:row.extend([data[i]]*(token-127));i+=1
            else:row.extend(data[i:i+token+1]);i+=token+1
        assert len(row)==460 and max(row)<32
        if y<239:assert i==offsets[y+1]
        else:assert i==len(data)
        pixels.extend(row)
    assert pixels==list(expected.getdata())
    raw=expected.getpalette();assert pal==[rgb(raw[j*3:j*3+3]) for j in range(32)]
    baseline=im.quantize(colors=64,method=Image.Quantize.MEDIANCUT,dither=Image.Dither.NONE).convert('RGB')
    error=ImageStat.Stat(ImageChops.difference(expected.convert('RGB'),baseline)).mean
    errors.append(sum(error)/3)
print('PASS: 110400 Mountains pixels, seven balls, bounded RLE rows, exact RGB565 palettes')
print('Mean RGB channel difference vs 64-colour baseline:',round(sum(errors),3),'/ 255')
