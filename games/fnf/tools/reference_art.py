"""Flatten the reference's Sparrow/Adobe Animate atlases offline, never on device."""
from pathlib import Path
import json,math,xml.etree.ElementTree as ET
from functools import lru_cache
from PIL import Image,ImageEnhance,ImageChops

IDENTITY=(1,0,0,1,0,0)
def read(p):return json.loads(Path(p).read_text(encoding='utf-8-sig'))
def matrix(d):
    if 'MX'in d:return tuple(d['MX'])
    if 'M3D'in d:
        a=d['M3D'];return(a[0],a[1],a[4],a[5],a[12],a[13])
    return IDENTITY
def multiply(a,b):return(a[0]*b[0]+a[2]*b[1],a[1]*b[0]+a[3]*b[1],a[0]*b[2]+a[2]*b[3],a[1]*b[2]+a[3]*b[3],a[0]*b[4]+a[2]*b[5]+a[4],a[1]*b[4]+a[3]*b[5]+a[5])
def point(m,x,y):return(m[0]*x+m[2]*y+m[4],m[1]*x+m[3]*y+m[5])
def bounds(leaves):
    if not leaves:return(0,0,1,1)
    pts=[]
    for im,m,a in leaves:
        b=im.getbbox()
        if b and a>0:pts.extend(point(m,x,y)for x,y in [(b[0],b[1]),(b[2],b[1]),(b[2],b[3]),(b[0],b[3])])
    if not pts:return(0,0,1,1)
    return(min(p[0]for p in pts),min(p[1]for p in pts),max(p[0]for p in pts),max(p[1]for p in pts))
def flatten(leaves,scale=1,box=None):
    box=box or bounds(leaves);x0,y0=math.floor(box[0]*scale),math.floor(box[1]*scale);x1,y1=math.ceil(box[2]*scale),math.ceil(box[3]*scale)
    out=Image.new('RGBA',(max(1,x1-x0),max(1,y1-y0)))
    for im,m,alpha in leaves:
        a,b,c,d,tx,ty=m;a*=scale;b*=scale;c*=scale;d*=scale;tx=tx*scale-x0;ty=ty*scale-y0;det=a*d-b*c
        if abs(det)<1e-8:continue
        inv=(d/det,-c/det,(c*ty-d*tx)/det,-b/det,a/det,(b*tx-a*ty)/det)
        layer=im.transform(out.size,Image.Transform.AFFINE,inv,Image.Resampling.BICUBIC)
        if alpha<.999:layer.putalpha(layer.getchannel('A').point(lambda x:round(x*alpha)))
        out.alpha_composite(layer)
    return out,x0,y0

class Atlas:
    def __init__(self,path):
        self.path=Path(path);self.d=read(self.path/'Animation.json');self.symbols={x['SN']:x['TL']for x in self.d.get('SD',{}).get('S',[])};self.textures={};self.labels={}
        for p in sorted(self.path.glob('spritemap*.json')):
            im=Image.open(p.with_suffix('.png')).convert('RGBA');data=read(p)
            for item in data['ATLAS']['SPRITES']:
                s=item['SPRITE'];sprite=im.crop((s['x'],s['y'],s['x']+s['w'],s['y']+s['h']))
                if s.get('rotated'):sprite=sprite.transpose(Image.Transpose.ROTATE_90)
                self.textures[str(s['name'])]=sprite
        for layer in self.d['AN']['TL']['L']:
            for f in layer['FR']:
                if f.get('N'):self.labels[f['N']]=(f['I'],f['DU'])
    def duration(self,tl):return max((f['I']+f['DU']for l in tl['L']for f in l['FR']),default=1)
    def sequence(self,prefix,symbol=False):
        match=lambda keys:next((k for k in keys if k.lower()==prefix.lower()),next((k for k in keys if k.lower().startswith(prefix.lower())),None))
        if symbol:
            k=match(self.symbols)
            if k:return self.symbols[k],0,self.duration(self.symbols[k])
            if self.d['AN'].get('SN','').lower().startswith(prefix.lower()):return self.d['AN']['TL'],0,self.duration(self.d['AN']['TL'])
        k=match(self.labels)
        if k:return self.d['AN']['TL'],*self.labels[k]
        k=match(self.symbols)
        if k:return self.symbols[k],0,self.duration(self.symbols[k])
        raise KeyError((self.path.name,prefix))
    def walk(self,tl,frame,transform=IDENTITY,alpha=1,depth=0):
        if depth>40:raise ValueError('Cyclic symbol hierarchy')
        result=[]
        for l in reversed(tl['L']):
            if l.get('LT')=='Clp':continue
            f=next((f for f in l['FR']if f['I']<=frame<f['I']+f['DU']),None)
            if not f:continue
            leaves=[]
            for e in f.get('E',[]):
                if 'ASI'in e:
                    d=e['ASI'];im=self.textures.get(str(d['N']))
                    if im is not None:leaves.append((im,multiply(transform,matrix(d)),alpha))
                elif 'SI'in e:
                    d=e['SI'];child=self.symbols.get(d['SN'])
                    if child is None:continue
                    t=d.get('FF',0);loop=d.get('LP','LP')
                    if loop!='SF':t+=frame-f['I']
                    length=self.duration(child);t=t%length if loop=='LP'else min(t,length-1)
                    a=alpha*d.get('C',{}).get('AM',1)
                    leaves.extend(self.walk(child,t,multiply(transform,matrix(d)),a,depth+1))
            if l.get('Clpb')and leaves:
                mask=next((x for x in tl['L']if x.get('LN')==l['Clpb']),None)
                if mask:
                    masks=self.walk({'L':[{'LN':mask['LN'],'FR':mask['FR']}]},frame,transform,alpha,depth+1)
                    box=bounds(leaves);im,x,y=flatten(leaves,box=box);matte,_,_=flatten(masks,box=box)
                    im.putalpha(ImageChops.multiply(im.getchannel('A'),matte.getchannel('A')))
                    b=im.getbbox()
                    leaves=[(im.crop(b),(1,0,0,1,x+b[0],y+b[1]),1)]if b else[]
            result.extend(leaves)
        return result
    def frames(self,animation,count=2):
        tl,start,duration=self.sequence(animation['prefix'],animation.get('animType')=='symbol');indices=animation.get('frameIndices')
        ids=indices or list(range(duration));samples=[ids[min(len(ids)-1,round(i*(len(ids)-1)/max(1,count-1)))]for i in range(count)]
        ox,oy=animation.get('offsets',[0,0]);transform=(1,0,0,1,-ox,-oy)
        return[self.walk(tl,start+i,transform)for i in samples]

class ReferenceArt:
    def __init__(self,root):self.root=Path(root);self.atlases={};self.sparrows={}
    def path(self,name,library='shared'):
        if ':'in name:library,name=name.split(':',1)
        choices=[self.root/lib/'images'/name for lib in dict.fromkeys([library,'shared','preload'])]
        return next((p for p in choices if p.is_dir()or p.with_suffix('.png').exists()),choices[0])
    def atlas(self,path):
        path=Path(path)
        if path not in self.atlases:self.atlases[path]=Atlas(path)
        return self.atlases[path]
    def sparrow(self,path):
        path=Path(path)
        if path not in self.sparrows:
            xml=ET.parse(path.with_suffix('.xml')).getroot();im=Image.open(path.with_suffix('.png')).convert('RGBA');frames=[]
            for item in xml:
                d=item.attrib;x,y,w,h=[int(d[k])for k in ['x','y','width','height']];sprite=im.crop((x,y,x+w,y+h))
                if d.get('rotated')=='true':sprite=sprite.transpose(Image.Transpose.ROTATE_90)
                frames.append((d['name'],sprite,(1,0,0,1,-int(d.get('frameX',0)),-int(d.get('frameY',0)))))
            self.sparrows[path]=frames
        return self.sparrows[path]
    def frames(self,path,animation,count=2):
        path=Path(path)
        if path.with_suffix('.xml').exists():
            allframes=[f for f in self.sparrow(path)if f[0].startswith(animation.get('prefix',''))]
            if not allframes:raise KeyError((str(path),animation))
            ids=animation.get('frameIndices')or list(range(len(allframes)));ox,oy=animation.get('offsets',[0,0]);result=[]
            for i in range(count):
                n=ids[min(len(ids)-1,round(i*(len(ids)-1)/max(1,count-1)))];_,im,m=allframes[n%len(allframes)];m=multiply((1,0,0,1,-ox,-oy),m);result.append([(im,m,1)])
            return result
        if path.with_suffix('.txt').exists():
            import re
            im=Image.open(path.with_suffix('.png')).convert('RGBA');items=[]
            for line in path.with_suffix('.txt').read_text().splitlines():
                m=re.match(r'(.+?)\s*=\s*(\d+)\s+(\d+)\s+(\d+)\s+(\d+)',line)
                if m and m[1].startswith(animation.get('prefix','')):
                    x,y,w,h=map(int,m.groups()[1:]);items.append((m[1],im.crop((x,y,x+w,y+h))))
            items.sort(key=lambda x:x[0]);ids=animation.get('frameIndices')or list(range(len(items)))
            if not items:raise KeyError((path,animation))
            ox,oy=animation.get('offsets',[0,0]);return[[(items[ids[min(len(ids)-1,round(i*(len(ids)-1)/max(1,count-1))) ]%len(items)][1],(1,0,0,1,-ox,-oy),1)]for i in range(count)]
        if path.is_dir():
            candidates=sorted(path.rglob('Animation.json'))
            for p in candidates:
                try:return self.atlas(p.parent).frames(animation,count)
                except KeyError:pass
            raise KeyError((str(path),animation))
        im=Image.open(path.with_suffix('.png')).convert('RGBA');return[[(im,IDENTITY,1)]]*count
