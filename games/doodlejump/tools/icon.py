#!/usr/bin/env python3
"""Original flat-colour notebook icon: small both as PNG and as an NWI."""
from pathlib import Path
from PIL import Image, ImageDraw
im=Image.new('RGB',(55,56),'#faf4dd');d=ImageDraw.Draw(im)
for y in range(5,56,7):d.line((0,y,54,y),fill='#dbe5d8')
for x in range(9,55,7):d.line((x,0,x,55),fill='#dbe5d8')
d.rectangle((0,0,6,55),fill='#e6e1c9');d.line((10,0,10,55),fill='#c76a55')
ink='#294853';green='#8dbd69'
for y in (10,26,42):d.ellipse((3,y-2,7,y+2),fill=ink);d.line((0,y-2,6,y-2),fill=ink)
d.rectangle((16,45,48,49),fill=ink);d.rectangle((17,46,47,48),fill=green)
d.ellipse((20,12,42,34),fill=ink);d.ellipse((21,13,41,33),fill=green)
d.rectangle((20,22,42,35),fill=ink);d.rectangle((21,21,41,34),fill=green)
d.rectangle((16,23,21,35),fill=ink);d.rectangle((17,24,19,33),fill='#71a6b5')
d.rectangle((39,22,49,28),fill=ink);d.rectangle((40,23,48,27),fill=green)
for x in (29,35):d.rectangle((x,17,x+3,22),fill='#fffbef');d.rectangle((x+2,20,x+2,22),fill=ink)
d.line((25,35,24,42),fill=ink);d.line((36,35,39,42),fill=ink)
d.line((21,42,27,42),fill=ink);d.line((36,42,43,42),fill=ink)
im.save(Path(__file__).resolve().parents[1]/'src/icon.png',optimize=True)
