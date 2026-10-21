#!/usr/bin/env python3
"""Actual ARM .nwa smoke tests: menu/pause, all boards, save/reload and safe exit.
Requires the repository emulator's dependencies (Unicorn, pyelftools, Pillow).
"""
import argparse
import io
import json
import os
from pathlib import Path
import re
import shlex
import struct
import tempfile
import types

ROOT = Path(__file__).resolve().parents[1]
REPO = ROOT.parents[1]
emu_path = REPO/'tools/emu.py'
emu = types.ModuleType('dk_emu');emu.__file__=str(emu_path);emu.io=io
source = emu_path.read_text().replace('ELFFile(open(elf, "rb"))','ELFFile(io.BytesIO(open(elf, "rb").read()))')
source = source.replace('cmd = [nwlink, "nwa-elf",','cmd = nwlink + ["nwa-elf",')
source = source.replace('chunk = CPU_HZ // 1000','chunk = CPU_HZ // 10000')
source = source.replace('self.now_ms += 1\n','self.now_ms += 0.1\n')
source = source.replace('        next_frame = self.now_ms if frame_ms else None',
    '        self.checkpoint = until_ms\n        next_frame = self.now_ms if frame_ms else None')
source = source.replace('            self.slept_ms += r0\n',
    '            self.slept_ms += r0\n            if self.now_ms >= self.checkpoint: uc.emu_stop()\n')
exec(compile(source,str(emu_path),'exec'),emu.__dict__)

def presses(*items):
    return [(t,t+d,emu.KEYS[k]) for t,k,d in items]

def storage(path,items):
    data=bytearray(emu.STORAGE_SIZE);offset=0
    for name,value in items:
        record=name.encode()+b'\0'+value;size=2+len(record)
        struct.pack_into('<H',data,offset,size);data[offset+2:offset+size]=record;offset+=size
    assert offset+2<=len(data)
    path.write_bytes(data)

def records(data):
    result={};offset=0
    while offset+2<=len(data):
        n=struct.unpack_from('<H',data,offset)[0]
        if not n:break
        assert n>=3 and offset+n<=len(data)
        name,value=data[offset+2:offset+n].split(b'\0',1);result[name.decode()]=value;offset+=n
    return result

def check_exit(c,upsilon=False):
    assert c.exited,'App did not exit'
    assert not c.violations,sorted(set(c.violations))
    assert not c.firmware_ram_changes(),'Firmware RAM changed'
    assert c.app_ram_clean(),'App RAM not cleared'
    assert c.locks==0 and c.max_locks==(0 if upsilon else 1)
    assert c.stack_used()<4096,c.stack_used()

def uint(c,name):
    return struct.unpack('<I',bytes(c.uc.mem_read(c.symbols[name],4)))[0]

def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--nwlink',default='nwlink')
    a=ap.parse_args()
    if Path(a.nwlink).is_file():
        nw=[a.nwlink] if a.nwlink.endswith(('.exe','.cmd')) else ['node',a.nwlink]
    else:
        nw=([v.replace('"','') for v in re.findall(r'(?:[^\s"]|"[^"]*")+',a.nwlink)]
            if os.name=='nt' else shlex.split(a.nwlink))
    out=ROOT/'output/arm';out.mkdir(parents=True,exist_ok=True);tempfile.tempdir=str(out)
    nwa=str(ROOT/'output/donkeykong-debug.nwa');report=[];sentinel=b'print(42)\n'
    for profile,model,system,ram in [('n0120','n0120','epsilon',153676),('n0110','n0110','epsilon',148928),('upsilon','n0110','upsilon',107674)]:
        emu.configure(model,system,ram);st=out/(profile+'.bin');storage(st,[('keep.py',sentinel)])
        c=emu.Calculator(nwa,storage_file=str(st),nwlink=nw);c.enable_checks()
        c.keys=presses((300,'ok',80),(1600,'right',200),(1900,'ok',60),(2200,'back',80),
                      (3300,'back',80),(3800,'home',180))
        c.run(250);c.save_png(str(out/(profile+'-title.png')))
        c.run(2350);c.save_png(str(out/(profile+'-pause.png')))
        before=bytes(c.uc.mem_read(c.symbols['game'],864));c.run(3200)
        assert bytes(c.uc.mem_read(c.symbols['game'],864))==before,'Pause advanced the game'
        # Seed a known high score to exercise the real storage integration.
        c.uc.mem_write(c.symbols['stats'],struct.pack('<I',12345))
        c.run(4200);check_exit(c,system=='upsilon')
        r=records(st.read_bytes());assert r['keep.py']==sentinel
        saved=r['donkeykong.sav'];assert len(saved)==32 and struct.unpack_from('<I',saved,4)[0]==12345
        d=emu.Calculator(nwa,storage_file=str(st),nwlink=nw);d.enable_checks()
        d.keys=presses((400,'onoff',150));d.run(250)
        assert uint(d,'stats')==12345;d.run(900);check_exit(d,system=='upsilon')
        assert records(st.read_bytes())['keep.py']==sentinel
        report.append(dict(profile=profile,pause_save_reload_exit='pass',stack_bytes=max(c.stack_used(),d.stack_used())))
        print(profile,'PASS',flush=True)
    # Real ARM code for every board (host fixture sets a representative scene).
    emu.configure();c=emu.Calculator(nwa,nwlink=nw);c.enable_checks();c.keys=presses((200,'ok',60))
    c.run(400)
    for b in range(4):
        fixture=(ROOT/f'output/fixture{b}.bin').read_bytes();assert len(fixture)==864
        c.uc.mem_write(c.symbols['game'],fixture)
        end=c.now_ms+700;c.run(end);c.save_png(str(out/f'board{b}.png'))
        assert not c.violations,sorted(set(c.violations))
    report.append(dict(boards='four ARM scenes',frames=uint(c,'perf_frames'),slowest_draw_ms=uint(c,'perf_max'),
                       frames_over_17ms=uint(c,'perf_slow'),stack_bytes=c.stack_used()))
    # A completely full filesystem must never delete another file or trap Home.
    st=out/'full.bin';payload=b'x'*(emu.STORAGE_SIZE-2-2-len('keep.py')-1)
    storage(st,[('keep.py',payload)]);c=emu.Calculator(nwa,storage_file=str(st),nwlink=nw);c.enable_checks()
    c.keys=presses((400,'home',180),(1100,'1',80),(1600,'home',100),(2200,'back',80))
    c.run(900);assert not c.exited;c.save_png(str(out/'storage-full.png'))
    c.run(1400);assert not c.exited
    c.run(2600);check_exit(c);assert records(st.read_bytes())=={'keep.py':payload}
    report.append(dict(full_storage='continue, retry exit, leave without deleting files: pass'))
    (out/'report.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))

if __name__=='__main__':
    main()
