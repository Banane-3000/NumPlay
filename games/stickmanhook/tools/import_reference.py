#!/usr/bin/env python3
"""Import the supplied, pinned browser assets, without retaining its browser runtime.

First run extract_reference.cjs on bundle.js, then pass its directory here.
Only resized artwork and canonical level/shape data enter the calculator build.
"""
import argparse
import base64
import hashlib
import json
from pathlib import Path
from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
COMMIT = '29006b0f11904704c390b457f7055c9fa1e53484'
POSES = ['spawn', 'hooked0', 'hooked1', 'hooked2', 'ball', 'victory']

def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('reference', type=Path)
    a = ap.parse_args()
    raw = (a.reference/'bundle.js').read_bytes()
    assert hashlib.sha1(b'blob '+str(len(raw)).encode()+b'\0'+raw).hexdigest() == '65da83d4a8d305683fda19aab8ad0b8433cf6c3c'
    ref = json.loads((a.reference/'reference.json').read_text())
    res = ref['resources']
    sprites = []
    def texture(key):
        v = res[key]
        if v.startswith('data:'):
            import io
            return Image.open(io.BytesIO(base64.b64decode(v.split(',')[1]))).convert('RGBA')
        return Image.open(a.reference/v).convert('RGBA')
    def add(name, im, scale, anchor=(.5,.5)):
        size = tuple(max(1, round(s*scale)) for s in im.size)
        im = im.resize(size, Image.Resampling.LANCZOS)
        # Palette transparency is binary, matching the RGB565 LCD.
        if name!='finishlight':
            alpha = im.getchannel('A').point(lambda x: 255 if x>=100 else 0)
            im.putalpha(alpha)
        box = im.getbbox() or (0,0,1,1)
        ox,oy = box[0]-round(size[0]*anchor[0]),box[1]-round(size[1]*anchor[1])
        im = im.crop(box)
        sprites.append((name, im, ox, oy))
    def frame(tex, atlas, name):
        d = atlas['frames'][name]; f = d['frame']; s = d['spriteSourceSize']
        w,h = (f['h'],f['w']) if d.get('rotated') else (f['w'],f['h'])
        im = tex.crop((f['x'],f['y'],f['x']+w,f['y']+h))
        if d.get('rotated'): im = im.transpose(Image.Transpose.ROTATE_90)
        assert im.size == (s['w'],s['h']), (name,im.size,s)
        canvas = Image.new('RGBA',(d['sourceSize']['w'],d['sourceSize']['h']))
        canvas.paste(im,(s['x'],s['y']))
        return canvas
    for key,scale in [('title',.34),('hook',.12),('hook_near',.12),('hook_active',.12),('arrow_right',.24)]:
        add(key,texture(key),scale)
    for key in ref['shapes']: add(key,texture(key),.12)
    btex=texture('bumper_texture');batlas=res['bumper_json']
    for i,name in enumerate(batlas['animations']['AN_Grap1_BumperLarge']):
        add('bumper'+str(i),frame(btex,batlas,name),.12,(.49,.635))
    add('finishlight',texture('finishlight'),.18,(.5,.4))
    tex=texture('finishplatform_texture');atlas=res['finishplatform_json']
    for i,name in enumerate(atlas['animations']['AN_Grap1_FinishPlatform']):
        add('platform'+str(i),frame(tex,atlas,name),.12,(.5,-.2))
    skins=[]; reward=0
    for i,s in enumerate(ref['skins']):
        atlas=res[s['atlas']];tex=texture(s['texture']);anim=atlas['animations'][s['id']]
        first=len(sprites)
        for pose in POSES: add('skin'+str(i)+'_'+pose,frame(tex,atlas,anim[s['frames'][pose]]),.06)
        dance=atlas['animations'].get('dance',[])
        for j,name in enumerate(dance):add('skin'+str(i)+'_dance'+str(j),frame(tex,atlas,name),.06)
        unlock=s.get('condition',{}).get('level',0)
        if s['type']=='rewarded':reward+=1;unlock=reward*8
        skins.append(dict(name='Hot Dog' if s['id']=='CHAR_HotDog' else s['name'],unlock=unlock,first=first,dance=len(dance),original_id=s['id']))
    # Store small, independent PNGs. Rebuilding never downloads anything.
    folder=ROOT/'assets/sprites';folder.mkdir(exist_ok=True)
    desc=[]
    for name,im,x,y in sprites:
        im.save(folder/(name+'.png'),optimize=True)
        desc.append(dict(name=name,x=x,y=y,w=im.width,h=im.height))
    assets=ROOT/'assets'
    for name,data in [('sprites',desc),('skins',skins),('levels',dict(maps=ref['maps'],recipes=ref['recipes'])),('geometry',ref['shapes'])]:
        (assets/(name+'.json')).write_text(json.dumps(data,separators=(',',':'))+'\n')
    backgrounds=[]
    for key in ['background_0','background_1']:
        im=texture(key).convert('RGB').resize((1,240),Image.Resampling.LANCZOS)
        bg_pixels=im.tobytes();backgrounds.append([list(bg_pixels[i:i+3]) for i in range(0,len(bg_pixels),3)])
    (assets/'backgrounds.json').write_text(json.dumps(backgrounds,separators=(',',':'))+'\n')
    icon=texture('hook_active').resize((55,55),Image.Resampling.LANCZOS)
    bg=Image.new('RGBA',(55,55),(83,111,187,255));bg.alpha_composite(icon);bg.convert('RGB').resize((55,56)).save(ROOT/'src/icon.png')
    manifest=dict(repository='https://github.com/Gr4ys0n/Gr4ys0n.github.io',commit=COMMIT,bundle_git_blob='65da83d4a8d305683fda19aab8ad0b8433cf6c3c',bundle_sha256=hashlib.sha256(raw).hexdigest(),sprites=len(sprites),skins=len(skins))
    (assets/'provenance.json').write_text(json.dumps(manifest,indent=2)+'\n')
    print(json.dumps(manifest,indent=2))
if __name__=='__main__':main()
