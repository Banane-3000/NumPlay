"""Audit source provenance, complete mart data and native asset reproducibility."""
from pathlib import Path
import hashlib,json,subprocess,sys
from PIL import Image
ROOT=Path(__file__).resolve().parents[1]
def digest(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def main():
    assets=ROOT/'assets';provenance=json.loads((assets/'provenance.json').read_text())
    assert provenance['commit']=='29006b0f11904704c390b457f7055c9fa1e53484'
    for name,expected in provenance['converted_sha256'].items():
        p=(assets/name).resolve();assert p.is_relative_to(assets.resolve())
        assert digest(p)==expected,('Converted reference asset changed',name)
    ref=json.loads((assets/'reference.json').read_text());assert ref['version']=='2.1'
    assert [len(m['listPlaceBoxes'])for m in ref['marts']]==[32,43]
    assert [len(m['listSalesMain'])for m in ref['marts']]==[17,18]
    assert len(ref['global']['data_hats'])==21
    foods=set()
    for m in ref['marts']:
        links=[b['link']for b in m['listPlaceBoxes']];assert len(links)==len(set(links))
        for b in m['listPlaceBoxes']:
            if b.get('typeFood'):foods.add(b['typeFood'])
        assert all(1<=v<=len(links)for row in m['unlocks']for v in row)
    assert foods==set(range(1,15))
    assert ref['marts'][1]['listPlaceBoxes'][25]['link']=='placebox_f9_6'
    actors=json.loads((assets/'actors.json').read_text());assert len(actors)==204
    assert {a['skin']for a in actors}=={'monkey1','monkey2','monkey3','monkey4','monkey5','monkey6','monkey7','monkey9','cow1','chicken1'}
    for actor in actors:
        im=Image.open(assets/'sprites'/(actor['name']+'.png'));assert im.mode=='RGBA'
        assert 0<im.width<=85 and 0<im.height<=85
    fonts=json.loads((assets/'ui-fonts.json').read_text());assert [f['size']for f in fonts]==[13,22]
    for font in fonts:
        assert len(font['glyphs'])==95 and len(font['pixels'])%2==0
        for offset,w,h,_,_,advance in font['glyphs']:
            assert offset+(w*h+1)//2<=len(font['pixels'])//2 and advance>0
    generated=[ROOT/'src'/n for n in ['assets.c','data.c','sprite-ids.h']]
    before=[digest(p)for p in generated]
    subprocess.run([sys.executable,str(ROOT/'tools/assets.py')],check=True)
    assert [digest(p)for p in generated]==before,'Asset generation is not reproducible'
    print('PASS: original reference hashes, both complete layouts, all 14 products, 21 hats, 204 baked frames, native fonts and reproducible packed data')
if __name__=='__main__':main()
