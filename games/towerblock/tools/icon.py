#!/usr/bin/env python3
"""Original isometric tower icon; generate with Pillow."""
from pathlib import Path
from PIL import Image,ImageDraw
S=4
im=Image.new('RGB',(55*S,56*S),'#091b2b');d=ImageDraw.Draw(im)
def poly(points,color):
    d.polygon([(int(x*S),int(y*S)) for x,y in points],fill=color)
def slab(cx,cy,w,h,color,left,right):
    a,b,c,e=(cx,cy-h),(cx+w,cy),(cx,cy+h),(cx-w,cy)
    poly([e,c,(c[0],c[1]+4),(e[0],e[1]+4)],left)
    poly([b,c,(c[0],c[1]+4),(b[0],b[1]+4)],right)
    poly([a,b,c,e],color)
    d.line([(int(e[0]*S),int(e[1]*S)),(int(a[0]*S),int(a[1]*S)),(int(b[0]*S),int(b[1]*S))],fill='#c7eee7',width=S)
for i,color in enumerate(['#f0acad','#c29ceb','#7ba6ed','#69debf']):
    slab(27,39-i*6,19,9,color,'#36627c','#25475b')
slab(34,11,15,7,'#95f0d2','#438d8b','#29636f')
im.resize((55,56),Image.Resampling.LANCZOS).save(Path(__file__).resolve().parents[1]/'src/icon.png')
