"""Audit the lossless level conversion and generated asset reproducibility."""
from pathlib import Path
import hashlib,json,subprocess,sys
from PIL import Image
ROOT=Path(__file__).resolve().parents[1]
COMMON=[3,19,0,8,98,4,7,1,2,31,121,73,74,97,85]
def decode(m,stream):
    data=stream[m['offset']:m['offset']+m['size']];out=bytearray();i=0
    if m.get('format'):
        while i<len(data):
            flags=data[i];i+=1
            for bit in range(8):
                if i>=len(data):break
                if flags&(1<<bit):out.append(data[i]);i+=1
                else:
                    distance=(data[i]|((data[i+1]&15)<<8))+1;n=(data[i+1]>>4)+3;i+=2
                    assert distance<=len(out)
                    for _ in range(n):out.append(out[-distance])
    else:
        while i<len(data):
            tag=data[i];i+=1;v=tag&15
            if v==15:v=data[i];i+=1
            else:v=COMMON[v]
            out.extend([v]*((tag>>4)+1))
    assert len(out)==m['w']*m['h']
    return out
def main():
    a=ROOT/'assets';maps=json.loads((a/'maps.json').read_text());stream=(a/'maps.bin').read_bytes();assert len(maps)==717
    assert [m['name']for m in maps[:200]]==list(map(str,range(1,201)))
    for m in maps:assert hashlib.sha256(decode(m,stream)).hexdigest()==m['sha256']
    assert sum('_'in m['name']for m in maps)==517
    ref=json.loads((a/'reference.json').read_text());assert ref['commit']=='29006b0f11904704c390b457f7055c9fa1e53484';assert len(ref['masks'])==16
    provenance=json.loads((a/'provenance.json').read_text());assert provenance['stage_cache_comparisons_passed']==200
    for name,digest in provenance['converted_sha256'].items():assert hashlib.sha256((a/name).read_bytes()).hexdigest()==digest
    rules=json.loads((a/'rules.json').read_text());assert rules['PlayerSpeed']==5 and rules['ShieldCost']==200 and rules['ShieldDuration']==30
    assert len(rules['LevelsTargets'].split())==20 and len(rules['UpgradeFreezeCost'].split())==9
    sheet=Image.open(a/'sprites.png');sprites=json.loads((a/'sprites.json').read_text());assert len(sprites)==706
    assert sum(s['name'].startswith('0_Char_')for s in sprites)==29
    for s in sprites:assert s['x']+s['w']<=sheet.width and s['y']+s['h']<=sheet.height
    paths=[ROOT/'src'/n for n in ['assets.c','data.c','sprite-ids.h']];before=[p.read_bytes()for p in paths];subprocess.run([sys.executable,str(ROOT/'tools/assets.py')],check=True);assert before==[p.read_bytes()for p in paths]
    print('PASS: all 200 stages and 517 arcade sections match the original tile hashes; 16 masks, 706 sprites, reproducible packing')
if __name__=='__main__':main()
