#!/usr/bin/env python3
"""Generate the original Flappy Dunk launcher icon (Pillow)."""
from pathlib import Path
from PIL import Image, ImageDraw

S = 4
im = Image.new('RGB', (55*S, 56*S), '#f5eee3')
d = ImageDraw.Draw(im)
def oval(box, color, outline=None):
    d.ellipse(tuple(int(v*S) for v in box), fill=color, outline=outline, width=S)
def line(points, color, width=1):
    d.line([(int(x*S), int(y*S)) for x,y in points], fill=color, width=width*S)

for y in (8, 24, 40):
    line([(0,y),(55,y)], '#eae1d3')
for side in (-1,1):
    for i in range(3):
        cx=27+side*(21-i*3)
        oval((cx-6,14+i*3,cx+6,20+i*3),'#fffdf6','#b7b1a6')
oval((12,9,43,40),'#a96334')
oval((13,9,42,38),'#f6a24e')
oval((18,13,25,17),'#ffe0a0')
line([(13,23),(42,23)],'#a96334')
for side in (-1,1):
    line([(27,10),(27+side*7,18),(27+side*7,29),(27,38)],'#a96334')
oval((21,25,23,28),'#354650')
oval((32,25,34,28),'#354650')
line([(25,30),(27,32),(29,30)],'#354650')
for x in (9,18,27,36,45):
    line([(x,43),(18+(x-9)*.5,54)],'#b4a895')
oval((6,38,49,47),'#ad493d')
oval((9,40,46,44),'#f5eee3')
d.arc((6*S,38*S,49*S,47*S),0,180,fill='#d76150',width=2*S)
im.resize((55,56),Image.Resampling.LANCZOS).save(Path(__file__).resolve().parents[1]/'src/icon.png')
