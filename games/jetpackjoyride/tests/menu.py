#!/usr/bin/env python3
"""Focused animated-title check on the actual ARM application."""
import argparse
import hashlib
import json
import tempfile
from PIL import Image
from arm import OUT,emu,performance,presses,record,records,safe,state

def main():
    ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('--nwlink',default='nwlink');a=ap.parse_args()
    (OUT/'tmp').mkdir(exist_ok=True);tempfile.tempdir=str(OUT/'tmp');report=[]
    for name,model,system,ram in [('n0110','n0110','epsilon',148928),('n0115-custom','n0110','epsilon',133652),('n0120','n0120','epsilon',153676),('upsilon','n0110','upsilon',107674)]:
        emu.configure(model,system,ram);storage=OUT/(name+'-menu-storage.bin')
        storage.write_bytes(record('keep.py',b'menu sentinel\n')+b'\0\0')
        c=emu.Calculator(str(OUT/'jetpackjoyride-debug.nwa'),storage_file=str(storage),nwlink=a.nwlink);c.enable_checks()
        hashes=set();frames=[]
        for frame in range(16):
            c.run(200+frame*160);hashes.add(hashlib.sha256(c.screen).digest())
            assert state(c)==bytes(232),'Menu animation changed the simulation'
            if name=='n0110':
                path=OUT/f'animated-title-{frame}.png';c.save_png(str(path));frames.append(Image.open(path).copy())
        assert len(hashes)==16,'Title background did not animate'
        perf=performance(c)
        c.keys=presses((2900,'home',100));c.run(3300);safe(c)
        assert records(c.storage_bytes())['keep.py']==b'menu sentinel\n'
        report.append(dict(profile=name,unique_frames=len(hashes),stack=c.stack_used(),**perf))
        if frames:frames[0].save(OUT/'menu-animation.gif',save_all=True,append_images=frames[1:],duration=160,loop=0)
    (OUT/'menu-validation.json').write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps(report,indent=2))

if __name__=='__main__':main()
