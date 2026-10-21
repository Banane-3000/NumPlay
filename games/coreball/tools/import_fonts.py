#!/usr/bin/env python3
"""Bake native-size antialiased glyphs; the build needs no installed fonts."""
import argparse
import json
import math
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parents[1]

def rasterize(path, size, chars):
    font = ImageFont.truetype(str(path), size)
    cap_top = font.getbbox('H')[1]
    data, glyphs = [], []
    for code in range(32, 127):
        ch = chr(code)
        if ch not in chars:
            glyphs.append(None)
            continue
        left, top, right, bottom = font.getbbox(ch)
        im = Image.new('L', (max(1, right-left), max(1, bottom-top)))
        ImageDraw.Draw(im).text((-left, -top), ch, font=font, fill=255)
        bounds = im.getbbox()
        advance = math.ceil(font.getlength(ch)) + (1 if size == 13 else 0)
        if not bounds:
            glyphs.append([len(data), 0, 0, 0, 0, advance])
            continue
        ink = im.crop(bounds)
        xoff, yoff = left+bounds[0], top-cap_top+bounds[1]
        pixels = [(p+8)//17 for p in ink.getdata()]
        glyphs.append([len(data), ink.width, ink.height, xoff, yoff, advance])
        data.extend((pixels[i]<<4)|(pixels[i+1] if i+1<len(pixels) else 0)
                    for i in range(0, len(pixels), 2))
    fallback = glyphs[ord('?')-32]
    glyphs = [g if g is not None else fallback for g in glyphs]
    return dict(size=size, source=Path(path).name, glyphs=glyphs, pixels=bytes(data).hex())

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--regular-font', default='C:/Windows/Fonts/segoeui.ttf')
    parser.add_argument('--bold-font', default='C:/Windows/Fonts/segoeuib.ttf')
    args=parser.parse_args()
    fonts=[rasterize(args.regular_font,13,''.join(map(chr,range(32,127)))),
           rasterize(args.bold_font,22,'ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789 !?+-.:/'),
           rasterize(args.bold_font,32,'0123456789? ')]
    (ROOT/'assets/ui-fonts.json').write_text(json.dumps(fonts,separators=(',',':'))+'\n',encoding='utf8')
    print(ROOT.name, 'native-size font pixels:',sum(len(f['pixels'])//2 for f in fonts))
if __name__=='__main__': main()
