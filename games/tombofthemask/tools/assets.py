"""Pack original monochrome sprites, bitmap font and all 717 original maps."""
from pathlib import Path
import json
from PIL import Image
ROOT=Path(__file__).resolve().parents[1]
def array(name,ctype,values):return 'const '+ctype+' '+name+'[]={'+','.join(map(str,values))+'};\n'
def pack(bits):
    data=bytearray((len(bits)+7)//8)
    for i,v in enumerate(bits):
        if v:data[i//8]|=128>>(i%8)
    return data
def main():
    a=ROOT/'assets';cat=json.loads((a/'sprites.json').read_text());sheet=Image.open(a/'sprites.png').convert('RGBA');bits=bytearray();ink=bytearray();sprites=[];ids=[];cache={}
    for i,s in enumerate(cat):
        im=sheet.crop((s['x'],s['y'],s['x']+s['w'],s['y']+s['h']));raw=pack([p>=128 for p in im.getchannel('A').getdata()]);colors=pack([p>=128 for p in im.convert('L').getdata()]);key=(im.size,bytes(raw),bytes(colors))
        if key not in cache:cache[key]=len(bits);bits+=raw;ink+=colors
        sprites.append('{%d,%d,%d}'%(cache[key],s['w'],s['h']));name='S_'+''.join(c.upper()if c.isalnum()else '_'for c in s['name']);ids.append('#define '+name+' '+str(i))
    (ROOT/'src/sprite-ids.h').write_text('\n'.join(ids)+'\n',encoding='utf-8',newline='\n')
    font=Image.open(a/'font.png').convert('RGBA').getchannel('A');ft=json.loads((a/'font.json').read_text());glyphs={};fb=bytearray()
    for g in ft['glyphs']:
        uv,v=g['uv'],g['vert'];x=round(uv['x']*font.width);y=round((1-uv['y'])*font.height);w=round(abs(uv['width'])*font.width);h=round(abs(uv['height'])*font.height)
        im=font.crop((x,y,x+w,y+h)).transpose(Image.Transpose.FLIP_TOP_BOTTOM);raw=pack([p>=128 for p in im.getdata()]);glyphs[g['index']]='{%d,%d,%d,%d,%d,%d}'%(len(fb),w,h,round(g['advance']),round(v['x']),12-round(v['y']));fb+=raw
    for code,g in json.loads((a/'font-extra.json').read_text()).items():
        if int(code)in glyphs:continue
        glyphs[int(code)]='{%d,%d,%d,%d,%d,%d}'%(len(fb),g['w'],g['h'],g['advance'],g['x'],g['y']);fb+=bytes.fromhex(g['bits'])
    fallback=glyphs.get(ord('?'),'{0,0,0,6,0,0}');gs=[glyphs.get(i,glyphs.get(i-32,fallback))for i in range(32,128)]
    s='#include "assets.h"\n'+array('sprite_bits','uint8_t',bits)+array('sprite_ink','uint8_t',ink)+'const Sprite sprites[]={'+','.join(sprites)+'};\n'+array('font_bits','uint8_t',fb)+'const Glyph font_glyphs[]={'+','.join(gs)+'};\n'
    ref=json.loads((a/'reference.json').read_text());colors=[(round(c['r']*31)<<11)|(round(c['g']*63)<<5)|round(c['b']*31)for c in ref['mainColors']]
    s+=array('theme_colors','uint16_t',colors)+array('mask_levels','uint8_t',[m['levelToUnlock']for m in ref['masks']])+'const char *const mask_names[]={'+','.join(json.dumps(m['name'].upper())for m in ref['masks'])+'};\n'
    lookup={v['name']:i for i,v in enumerate(cat)}
    heroes=[]
    for mask in range(16):
        states=[]
        for state,count in [('idle',6),('long_jump',5),('run',10),('arrive',8)]:states.append('{'+','.join(str(lookup[f'{mask}_Char_{state}_{i%count+1}'])for i in range(10))+'}')
        heroes.append('{'+','.join(states)+'}')
    s+='const uint16_t hero_sprites[16][4][10]={'+','.join(heroes)+'};\n'
    rules=json.loads((a/'rules.json').read_text());costs=list(map(int,rules['MaskUnlockCost'].split()))
    s+=array('xp_targets','uint32_t',rules['LevelsTargets'].split())+array('upgrade_prices','uint16_t',rules['UpgradeFreezeCost'].split())+array('mask_prices','uint16_t',[costs[min(i,len(costs)-1)]for i in range(16)])
    s+='const uint8_t powerup_effects[4][10]={'+','.join('{'+','.join(rules[k].split())+'}'for k in ['CoinAddictEffect','FreezeDuration','MagnetDuration','ScoreBoosterDuration'])+'};\n'
    features=[]
    for m in range(16):
        row=[0]*9
        for f in rules['MaskFeatures'+str(m)].split():
            parts=list(map(int,f.split(':')));row[parts[0]]=parts[1]if len(parts)>1 else 1
        features.append('{'+','.join(map(str,row))+'}')
    s+='const uint8_t mask_features[16][9]={'+','.join(features)+'};\n'
    goals=[]
    for i in range(8):
        row=list(map(int,rules['MissionsInfo'+str(i)].split(':')));row.extend([row[-1]]*(7-len(row)));goals.append('{'+','.join(map(str,row))+'}')
    s+='const uint16_t mission_goals[8][7]={'+','.join(goals)+'};\n'+array('mission_rewards','uint16_t',rules['MissionsRewards'].split())
    (ROOT/'src/assets.c').write_text(s,encoding='utf-8',newline='\n')
    maps=json.loads((a/'maps.json').read_text());rows=[]
    for m in maps:
        zone,part=map(int,m['name'].split('_'))if '_'in m['name']else (0,int(m['name']))
        rows.append('{%d,%d,%d,%d,%d,%d,%d}'%(m['offset'],m['size'],m['w'],m['h'],zone,part,m.get('format',0)))
    s='#include "assets.h"\n#include "../../common/np_lz.h"\n'+array('map_bytes','uint8_t',(a/'maps.bin').read_bytes())+'const Map maps[717]={'+','.join(rows)+'};\n'
    s+='''int map_decode(unsigned id,uint8_t*out,unsigned cap){
 static const uint8_t common[15]={3,19,0,8,98,4,7,1,2,31,121,73,74,97,85};
 if(id>=717)return -1;const Map*m=&maps[id];unsigned n=m->w*m->h,i=0,o=0;if(n>cap)return -1;
 if(m->format){int got=np_lz_unpack(map_bytes+m->offset,m->size,out,n);return got==(int)n?got:-1;}
 while(i<m->size){unsigned tag=map_bytes[m->offset+i++],v=tag&15,count=(tag>>4)+1;if(v==15){if(i>=m->size)return -1;v=map_bytes[m->offset+i++];}else v=common[v];if(o+count>n)return -1;while(count--)out[o++]=v;}return o==n?(int)o:-1;
}\n'''
    (ROOT/'src/data.c').write_text(s,encoding='utf-8',newline='\n')
    print('Sprites',len(cat),'unique',len(cache),'bits',len(bits),'font',len(fb),'maps',len((a/'maps.bin').read_bytes()))
if __name__=='__main__':main()
