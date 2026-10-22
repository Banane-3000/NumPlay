"""Offline Spine region animation baking; no skeleton runtime/RAM in the app."""
from pathlib import Path
from PIL import Image,ImageDraw
import numpy as np,json,math,argparse
parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--reference',type=Path,required=True);args=parser.parse_args();ROOT=args.reference.resolve();D=json.loads((ROOT/'skeleton.json').read_text());OUT=Path(__file__).resolve().parents[1]/'assets/sprites'
skins={s['name']:s['attachments']for s in D['skins']};cache={}
def timeline(keys,t,key,default=0):
 if not keys or t<keys[0].get('time',0):return default
 a=keys[0];b=a
 for k in keys:
  if k.get('time',0)<=t:a=k
  else:b=k;break
 else:b=a
 av=a.get(key,default);bv=b.get(key,default)
 if a==b or a.get('curve')=='stepped':return av
 return av+(bv-av)*(t-a.get('time',0))/(b.get('time',0)-a.get('time',0))
def last(keys,t,default=None):
 result=default
 for k in keys:
  if k.get('time',0)>t:break
  result=k
 return result
def mat(x,y,r,sx=1,sy=1):
 c=math.cos(math.radians(r));s=math.sin(math.radians(r))
 return np.array([[c*sx,-s*sy,x],[s*sx,c*sy,y],[0,0,1.]])
def pose(skin,anim,t):
 a=D['animations'][anim];bones={}
 for b in D['bones']:
  ts=a.get('bones',{}).get(b['name'],{})
  m=mat(b.get('x',0)+timeline(ts.get('translate'),t,'x'),b.get('y',0)+timeline(ts.get('translate'),t,'y'),b.get('rotation',0)+timeline(ts.get('rotate'),t,'value'),b.get('scaleX',1)*timeline(ts.get('scale'),t,'x',1),b.get('scaleY',1)*timeline(ts.get('scale'),t,'y',1))
  bones[b['name']]=bones.get(b.get('parent'),np.eye(3))@m
 canvas=Image.new('RGBA',(256,256));view=np.array([[1,0,128],[0,-1,190],[0,0,1.]])
 slots=list(D['slots']);order=last(a.get('drawOrder',[]),t)
 if order:
  offsets={x['slot']:x['offset']for x in order.get('offsets',[])};new=[None]*len(slots);unchanged=[]
  for i,s in enumerate(slots):
   if s['name']in offsets:new[i+offsets[s['name']]]=s
   else:unchanged.append(s)
  for i in range(len(new)):
   if new[i]is None:new[i]=unchanged.pop(0)
  slots=new
 for s in slots:
  sn=s['name'];attachment=s.get('attachment');ts=a.get('slots',{}).get(sn,{})
  k=last(ts.get('attachment',[]),t)
  if k is not None:attachment=k.get('name')
  if sn.startswith('food')or sn=='hat':continue
  att=skins[skin].get(sn,{}).get(attachment,skins['default'].get(sn,{}).get(attachment))
  if not att:continue
  name=att.get('path',att.get('name',attachment));p=ROOT/'sprites'/('dodik__'+name+'.png')
  if not p.exists():continue
  if name not in cache:cache[name]=Image.open(p).convert('RGBA')
  im=cache[name];w=att['width'];h=att['height']
  # Source pixels use a top-left origin; Spine uses y up and centered regions.
  m=view@bones[s['bone']]@mat(att.get('x',0),att.get('y',0),att.get('rotation',0),att.get('scaleX',1),att.get('scaleY',1))@np.array([[w/im.width,0,-w/2],[0,-h/im.height,h/2],[0,0,1.]])
  inv=np.linalg.inv(m);layer=im.transform(canvas.size,Image.Transform.AFFINE,tuple(inv[:2].ravel()),Image.Resampling.BICUBIC)
  if s.get('color','ffffffff')[-2:]=='00':continue
  color=last(ts.get('rgba',[]),t)
  if color and color.get('color','ffffffff')[-2:]=='00':continue
  canvas.alpha_composite(layer)
 return canvas.resize((85,85),Image.Resampling.LANCZOS)
entries=[]
for skin in ['monkey1','monkey2','monkey3','monkey4','monkey5','monkey6','monkey7','monkey9','chicken1','cow1']:
 anims=['idle1','run1','idle2','run2','sleep1','crazy2']if skin.startswith('monkey')else ['idle_animal','eat_animal', 'sleep_chicken1'if skin=='chicken1'else'sleep_cow1']
 for anim in anims:
  count=8 if anim.startswith('run')else 2
  for frame in range(count):
   im=pose(skin,anim,frame/count*(.5333 if anim.startswith('run')else 1));box=im.getbbox()
   if not box:continue
   im=im.crop(box);name=f'actor_{skin}_{anim}_{frame}';im.save(OUT/(name+'.png'));entries.append(dict(name=name,x=box[0]-43,y=box[1]-63,skin=skin,anim=anim,frame=frame))
(OUT.parent/'actors.json').write_text(json.dumps(entries,separators=(',',':'))+'\n')
sample=Image.new('RGB',(500,200),'#cbdfb0')
for i,e in enumerate([e for e in entries if e['frame']==0][:30]):
 im=Image.open(OUT/(e['name']+'.png'));sample.paste(im,(i%10*50,i//10*60),im)
sample.save(ROOT/'actors-contact.png')
print('baked',len(entries),'frames')
