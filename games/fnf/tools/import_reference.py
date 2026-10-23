"""Convert the pinned, locally extracted official assets. No audio is read."""
from pathlib import Path
import argparse,json,zlib,hashlib,math,struct,re
import numpy as np
from PIL import Image,ImageDraw
from reference_art import ReferenceArt,read,bounds,flatten,multiply,IDENTITY

ROOT=Path(__file__).resolve().parents[1]
def main():
    ap=argparse.ArgumentParser();ap.add_argument('--reference',type=Path,required=True);ap.add_argument('--quick',action='store_true');args=ap.parse_args()
    ref=args.reference.resolve();source=ref/'original/funkin.assets';a=ROOT/'assets';a.mkdir(exist_ok=True);(ROOT/'output').mkdir(exist_ok=True);art=ReferenceArt(source)
    chartfiles=sorted((source/'preload/data/songs').rglob('*-chart*.json'));tracks=[];kinds={''};char_names=set();stage_names=set()
    for p in chartfiles:
        d=read(p);m=read(p.with_name(p.name.replace('-chart','-metadata')));song=p.parent.name;variant=p.stem[len(song+'-chart'):].lstrip('-')or'default'
        tracks.append(dict(id=song,variation=variant,metadata=m,chart=d));kinds.update(n.get('k','')for ns in d['notes'].values()for n in ns);stage_names.add(m['playData']['stage'])
        char_names.update(v for k,v in m['playData']['characters'].items()if k in ['player','opponent','girlfriend']and isinstance(v,str))
    char_names.update(p.stem for p in (source/'preload/data/characters').glob('*.json'));char_names=sorted(char_names);stage_names=sorted(stage_names)
    levels=[dict(id=key,**read(source/'preload/data/levels'/(key+'.json')))for key in ['tutorial']+['week'+str(i)for i in range(1,8)]+['weekend1','sserafim']]
    model=dict(refs=read(ref/'refs.json'),tracks=tracks,levels=levels,kinds=['']+sorted(kinds-{''}),characters=[],stages=[],ui={})
    frames=[];palettes=[];blob=bytearray();sprite_names={};dedup={}
    def sprite(name,im,ox=0,oy=0):
        im=im.convert('RGBA');mask=np.asarray(im.getchannel('A'));bbox=im.getbbox()
        if bbox:
            im=im.crop(bbox);ox+=bbox[0];oy+=bbox[1]
        else:im=Image.new('RGBA',(1,1))
        assert im.width<=320 and im.height<=240,(name,im.size)
        rgb=im.convert('RGB');q=rgb.quantize(colors=15,method=Image.Quantize.MEDIANCUT,dither=Image.Dither.NONE);pal=q.getpalette();pal.extend([0]*max(0,45-len(pal)));palette=[0]+[((pal[i*3]>>3)<<11)|((pal[i*3+1]>>2)<<5)|(pal[i*3+2]>>3)for i in range(15)]
        if palette not in palettes:palettes.append(palette)
        palette_id=palettes.index(palette);pix=np.asarray(q).astype(np.uint8)+1;pix[np.asarray(im.getchannel('A'))<96]=0
        data=bytearray();offsets=[]
        for row in pix:
            offsets.append(len(data));at=0
            while at<len(row):
                opaque=bool(row[at]);end=at+1
                while end<len(row)and bool(row[end])==opaque and end-at<127:end+=1
                data.append(((end-at)<<1)|int(opaque))
                if opaque:
                    for x in range(at,end,2):data.append(int(row[x])|((int(row[x+1])if x+1<end else 0)<<4))
                at=end
        compressed=name.startswith('stage/')or name=='menu'
        if compressed:
            flat=pix.reshape(-1);assert len(flat)%2==0;raw=bytes(int(flat[i])|(int(flat[i+1])<<4)for i in range(0,len(flat),2));data=bytearray(zlib.compress(raw,9,wbits=-15));offsets=[]
        key=(im.width,im.height,palette_id,bytes(data),tuple(offsets),ox,oy)
        if key in dedup:idx=dedup[key]
        else:
            idx=len(frames);offset=len(blob);blob.extend(struct.pack('<'+'H'*len(offsets),*offsets));blob.extend(data)
            frames.append(dict(name=name,w=im.width,h=im.height,palette=palette_id,offset=offset,ox=int(ox),oy=int(oy),compressed=compressed,packed_size=len(data)));dedup[key]=idx
        sprite_names[name]=idx;return idx
    def resize(im,w,h=None,pixel=False):return im.resize((w,h if h else max(1,round(im.height*w/im.width))),Image.Resampling.NEAREST if pixel else Image.Resampling.LANCZOS)
    anim_fail=[]
    for cid,name in enumerate(char_names):
        config=read(source/'preload/data/characters'/(name+'.json'));path=art.path(config['assetPath']);anims={};leaves_by_anim={}
        for anim in config['animations']:
            n=anim['name']
            if any(w in n.lower()for w in ['death','cutscene','-hold']):continue
            count=4 if n in ['idle','danceLeft','danceRight']else 1 if'miss'in n else 2
            try:leaves_by_anim[n]=art.frames(art.path(anim['assetPath'])if anim.get('assetPath')else path,anim,count)
            except (KeyError,FileNotFoundError,ValueError)as e:anim_fail.append((name,n,str(e)))
        assert leaves_by_anim,(name,'no usable animation')
        idle='idle'if'idle'in leaves_by_anim else'danceLeft'if'danceLeft'in leaves_by_anim else next(iter(leaves_by_anim));bb=bounds(leaves_by_anim[idle][0]);height=max(1,bb[3]-bb[1]);cx=(bb[0]+bb[2])*.5;foot=bb[3]
        target=64 if name.startswith(('bf','pico'))else 58 if name.startswith(('gf','otis'))else 76 if name.startswith(('nene','spooky','darnell'))else 84
        if name.startswith('sserafim'):target=76
        scale=target/height
        allboxes=[bounds(x)for v in leaves_by_anim.values()for x in v];scale=min(scale,126/max(b[2]-b[0]for b in allboxes),104/max(b[3]-b[1]for b in allboxes))
        for n,leaves_set in leaves_by_anim.items():
            ids=[]
            for i,leaves in enumerate(leaves_set):
                im,x,y=flatten(leaves,scale);ox=x-round(cx*scale);oy=y-round(foot*scale)
                if config.get('flipX'):im=im.transpose(Image.Transpose.FLIP_LEFT_RIGHT);ox=-ox-im.width
                if n==idle and i==0:im.save(ROOT/'output'/('character-'+name+'.png'))
                ids.append(sprite('char/'+name+'/'+n+'/'+str(i),im,ox,oy))
            anims[n]=ids
        if'danceLeft'in anims and'danceRight'in anims:anims['idle']=anims['danceLeft'][:2]+anims['danceRight'][:2]
        elif'idle'not in anims:anims['idle']=anims[idle]
        model['characters'].append(dict(id=name,anims=anims,icon=config.get('healthIcon',{}).get('id',name.split('-')[0]),singTime=config.get('singTime',4),isPixel=config.get('isPixel',False)))
        print('Character',name,len(anims),'animations;',len(blob),'art bytes',flush=True)
        if args.quick and cid==2:break
    for name in stage_names:
        config=read(source/'preload/data/stages'/(name+'.json'));chars=config.get('characters',{});feet=[chars.get(k,{}).get('position',[x,885])for k,x in [('bf',990),('dad',335)]];span=max(1440,abs(feet[0][0]-feet[1][0])+650);s=320/span;left=(feet[0][0]+feet[1][0])*.5-span*.5;top=max(v[1]for v in feet)-190/s
        im=Image.new('RGBA',(160,120),(28,23,42,255));missing=[]
        for prop in sorted(config.get('props',[]),key=lambda x:x.get('zIndex',0)):
            path=prop.get('assetPath','')
            if not path or path.startswith('#')or prop.get('alpha',1)==0:continue
            # These are enabled by the desktop intro script only, not during play.
            if name=='sserafim'and prop.get('name')in ['backTablesCutscene','burgerCutscene','truckDoor']:continue
            try:
                anim=(prop.get('animations')or[dict(prefix='')])[0];leaves=art.frames(art.path(path,config.get('directory','shared')),anim,1)[0]
                scale=prop.get('scale',[1,1]);scale=[scale,scale]if isinstance(scale,(int,float))else scale;pos=prop.get('position',[0,0]);m=(scale[0],0,0,scale[1],pos[0],pos[1]);leaves=[(part,multiply(m,mat),opacity*prop.get('alpha',1))for part,mat,opacity in leaves]
                layer,_,_=flatten(leaves,s*.5,(left,top,left+span,top+240/s));im.alpha_composite(layer,(0,0))
            except (FileNotFoundError,KeyError,ValueError)as e:missing.append((path,str(e)))
        bg=sprite('stage/'+name,im);model['stages'].append(dict(id=name,sprite=bg,missing=missing))
        (ROOT/'output'/('stage-'+name+'.png')).write_bytes(b'')if False else None
        im.save(ROOT/'output'/('stage-'+name+'.png'))
    # Native notes and receptor artwork, extracted without rescaling at runtime.
    for direction,label in enumerate(['Left','Down','Up','Right']):
        note=art.frames(art.path('notes'),dict(prefix='note'+label),1)[0];im,_,_=flatten(note);model['ui']['note'+str(direction)]=sprite('note'+str(direction),resize(im,22,22))
        for state,prefix in [('static','static'),('press','press'),('confirm','confirm')]:
            frame=art.frames(art.path('noteStrumline'),dict(prefix=prefix+label+'0'),1)[0];im,_,_=flatten(frame);model['ui'][state+str(direction)]=sprite(state+str(direction),resize(im,23,23))
    for n,name in [('logo','logoBumpin'),('menu','menuDesat')]:
        p=art.path(name,'preload');leaves=art.frames(p,dict(prefix=''),1)[0];im,_,_=flatten(leaves)
        if n=='logo':im.thumbnail((240,104),Image.Resampling.LANCZOS)
        else:im=resize(im,160,120)
        model['ui'][n]=sprite(n,im)
    icons={}
    for p in sorted((source/'preload/images/icons').glob('icon-*.png')):
        im=Image.open(p).convert('RGBA');size=im.height;ids=[]
        for i in range(min(3,im.width//size)):ids.append(sprite('icon/'+p.stem+'/'+str(i),resize(im.crop((i*size,0,(i+1)*size,size)),20,20,im.height<50)))
        icons[p.stem[5:]]=ids
    model['icons']=icons
    (a/'model.json.zlib').write_bytes(zlib.compress(json.dumps(model,separators=(',',':')).encode(),9));(a/'art.bin').write_bytes(blob)
    (a/'art.json').write_text(json.dumps(dict(sprites=frames,palettes=palettes,names=sprite_names),separators=(',',':'))+'\n',encoding='utf-8',newline='\n')
    report=dict(chars=len(model['characters']),stages=len(model['stages']),sprites=len(frames),art_bytes=len(blob),animation_failures=anim_fail,missing_props={s['id']:s['missing']for s in model['stages']if s['missing']})
    (ROOT/'output/import-report.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    print('Imported',len(tracks),'variations;',report,flush=True)
    for item in ['Funkin','funkin.art','funkin.assets']:
        target=ROOT/'LICENSES';target.mkdir(exist_ok=True);(target/(item+'.md')).write_bytes((ref/(item+'.license.md')).read_bytes())

if __name__=='__main__':main()
