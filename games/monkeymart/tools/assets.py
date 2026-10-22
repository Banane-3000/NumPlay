"""Native 4-bit sprites, baked Spine frames and row-addressable floors."""
from pathlib import Path
import json
from PIL import Image
ROOT=Path(__file__).resolve().parents[1]
def rgb(c):return ((c[0]>>3)<<11)|((c[1]>>2)<<5)|(c[2]>>3)
def arr(f,typ,name,v):
 f.write('static const '+typ+' '+name+'[]={\n')
 for i in range(0,len(v),24):f.write(','.join(map(str,v[i:i+24]))+',\n')
 f.write('};\n')
def main():
 paths=sorted((ROOT/'assets/sprites').glob('*.png'));paths=[p for p in paths if not p.name.startswith('background')and not p.stem.startswith('ui_game__')and(not p.stem.startswith('ui_main__')or '__hat'in p.stem)]
 ids={p.stem:i+1 for i,p in enumerate(paths)};(ROOT/'assets/sprite-ids.json').write_text(json.dumps(ids,separators=(',',':'))+'\n',encoding='utf-8',newline='\n')
 (ROOT/'src/sprite-ids.h').write_text('\n'.join('#define S_'+k.upper()+' '+str(v)for k,v in ids.items())+'\n',encoding='utf-8',newline='\n')
 with (ROOT/'src/assets.c').open('w',newline='\n')as f:
  f.write('#include "assets.h"\n')
  fonts=json.loads((ROOT/'assets/ui-fonts.json').read_text())
  for i,font in enumerate(fonts[:2]):
   arr(f,'uint8_t',f'fp{i}',list(bytes.fromhex(font['pixels'])));f.write(f'static const Glyph fg{i}[]={{'+','.join('{'+','.join(map(str,g))+'}'for g in font['glyphs'])+'};\n')
  f.write('const Font fonts[]={{fg0,fp0},{fg1,fp1},{fg1,fp1}};\n')
  # Identical frames and flat-color palettes are shared automatically.
  intern={};entries=['0'];total=0
  for p in paths:
   im=Image.open(p).convert('RGBA')
   if not p.stem.startswith(('actor_','menu_logo')):
    scale=1/3
    if p.stem.startswith('ui_main__hat'):scale=.3
    im=im.resize((max(1,round(im.width*scale)),max(1,round(im.height*scale))),Image.Resampling.LANCZOS)
   q=im.convert('RGB').quantize(colors=15,method=Image.Quantize.MEDIANCUT,dither=Image.Dither.NONE);pal=q.getpalette()+[0]*768
   pixels=[0 if a<88 else v+1 for v,(_,_,_,a)in zip(q.getdata(),im.getdata())]
   pix=bytes((pixels[i]<<4)|(pixels[i+1]if i+1<len(pixels)else 0)for i in range(0,len(pixels),2));palette=tuple([0]+[rgb(pal[i*3:i*3+3])for i in range(15)])
   key=(im.size,pix,palette)
   if key not in intern:
    n='im'+str(ids[p.stem]);intern[key]=n;arr(f,'uint16_t',n+'p',palette);arr(f,'uint8_t',n+'b',pix);f.write(f'static const Image {n}={{'+f'{im.width},{im.height},{n}b,{n}p'+'};\n');total+=len(pix)+32
   entries.append('&'+intern[key])
  f.write('const Image *const sprites[]={'+','.join(entries)+'};\n')
  actors=json.loads((ROOT/'assets/actors.json').read_text());lookup={(a['skin'],a['anim'],a['frame']):a for a in actors}
  def pose(skin,anim,frame):
   a=lookup.get((skin,anim,frame),lookup[(skin,anim,0)]);return '{%d,%d,%d}'%(ids[a['name']],a['x'],a['y'])
  f.write('const Pose poses[8][6][8]={'+','.join('{'+','.join('{'+','.join(pose('monkey%d'%s,a,j%(8 if a.startswith('run')else 2))for j in range(8))+'}'for a in ['idle1','run1','idle2','run2','sleep1','crazy2'])+'}'for s in [1,2,3,4,5,6,7,9])+'};\n')
  f.write('const Pose animals[2][3][2]={'+','.join('{'+','.join('{'+','.join(pose(s,a,j)for j in range(2))+'}'for a in ['idle_animal','eat_animal','sleep_chicken1'if s=='chicken1'else'sleep_cow1'])+'}'for s in ['chicken1','cow1'])+'};\n')
  f.write('const uint16_t hats[26]={'+','.join(str(ids.get('ui_main__hat'+str(i),0))if i==0 or i>=11 else str(ids.get('workerhat'+str(i),0))for i in list(range(6))+list(range(11,31)))+'};\n')
  f.write('const uint16_t food_icons[15]={0,'+','.join(str(ids.get('game__food'+str(i),0))for i in range(1,15))+'};\n')
  machines={5:'microwave1',6:'yogurt_machine1',10:'chocolate_machine1',11:'coffee_grinder1',12:'peanut_machine1',13:'coffee_machine1',14:'chocolate_bar_machine1'}
  f.write('const uint16_t machine_sprites[15]={'+','.join(str(ids.get('game__'+machines.get(i,''),0))for i in range(15))+'};\n')
  bg=[]
  for mi in range(2):
   p=ROOT/'assets/sprites'/('background'+('2'if mi else'')+'__bg_mart_'+str(mi+1)+'.png')
   im=Image.open(p).convert('RGB');im=im.resize((round(im.width*2/3),round(im.height*2/3)),Image.Resampling.LANCZOS).quantize(colors=16,method=Image.Quantize.MEDIANCUT,dither=Image.Dither.NONE)
   palette=im.getpalette()+[0]*768;arr(f,'uint16_t',f'bp{mi}',[rgb(palette[i*3:i*3+3])for i in range(16)])
   data=[];offset=[];px=list(im.getdata())
   for y in range(im.height):
    offset.append(len(data));row=px[y*im.width:(y+1)*im.width];x=0
    while x<im.width:
     end=x+1
     while end<im.width and row[end]==row[x]and end-x<128:end+=1
     if end-x>=3:data.extend([127+end-x,row[x]]);x=end
     else:
      start=x;x=end
      while x<im.width and x-start<128:
       if x+2<im.width and row[x]==row[x+1]==row[x+2]:break
       x+=1
      values=row[start:x];data.append(len(values)-1);data.extend((values[j]<<4)|(values[j+1]if j+1<len(values)else 0)for j in range(0,len(values),2))
   arr(f,'uint8_t',f'bb{mi}',data);arr(f,'uint32_t',f'bo{mi}',offset);bg.append('{bb%d,bp%d,bo%d,%d,%d}'%(mi,mi,mi,im.width,im.height));total+=len(data)+len(offset)*4+64
  f.write('const Background backgrounds[]={'+','.join(bg)+'};\n')
 print('packed sprite and floor bytes',total,'sprites',len(paths),'unique',len(intern))
 import data;data.main()
if __name__=='__main__':main()
