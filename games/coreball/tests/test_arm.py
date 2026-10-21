#!/usr/bin/env python3
"""Actual ARM .nwa tests: three calculator profiles, physics, save/reload and safe exit.
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
emu = types.ModuleType('arcade_emu');emu.__file__=str(emu_path);emu.io=io
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
    ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('--nwlink',default='nwlink');a=ap.parse_args()
    nw=['node',a.nwlink] if Path(a.nwlink).is_file() and not a.nwlink.endswith(('.exe','.cmd')) else shlex.split(a.nwlink)
    slug=ROOT.name;dune=slug=='dune';out=ROOT/'output/arm';out.mkdir(parents=True,exist_ok=True);tempfile.tempdir=str(out)
    host=json.loads((ROOT/'output/host-report.json').read_text());size=host['game_bytes'];stats_size=host['stats_bytes']
    nwa=str(ROOT/f'output/{slug}-debug.nwa');report=[];sentinel=b'print(42)\n';save_size=40 if dune else 24
    profiles=[('n0120','n0120','epsilon',153676),('n0110','n0110','epsilon',148928),('n0115','n0110','epsilon',133652),('upsilon','n0110','upsilon',107674)]
    for profile,model,system,ram in profiles:
        emu.configure(model,system,ram);st=out/(profile+'.bin');storage(st,[('keep.py',sentinel)])
        c=emu.Calculator(nwa,storage_file=str(st),nwlink=nw);c.enable_checks()
        c.keys=presses((300,'ok',80),(500,'ok',180),(850,'back',80),(1750,'back',80),(2400,'home',180))
        c.run(250);c.save_png(str(out/(profile+'-title.png')))
        c.run(1200);assert uint(c,'current_view')==2,('Pause menu did not open',uint(c,'current_view'))
        c.save_png(str(out/(profile+'-pause.png')));before=bytes(c.uc.mem_read(c.symbols['game'],size));c.run(1600)
        assert bytes(c.uc.mem_read(c.symbols['game'],size))==before,'Pause advanced physics'
        c.run(2100);c.save_png(str(out/(profile+'-play.png')))
        frames=uint(c,'perf_frames');slowest=uint(c,'perf_max');slow=uint(c,'perf_slow')
        c.run(3000);check_exit(c,system=='upsilon');r=records(st.read_bytes())
        assert r['keep.py']==sentinel and len(r[slug+'.sav'])==save_size
        expected=r[slug+'.sav'][4:4+(26 if dune else 16)]
        d=emu.Calculator(nwa,storage_file=str(st),nwlink=nw);d.enable_checks();d.keys=presses((400,'onoff',150));d.run(250)
        assert bytes(d.uc.mem_read(d.symbols['stats'],len(expected)))==expected,'Saved progress was not restored'
        d.run(900);check_exit(d,system=='upsilon');assert records(st.read_bytes())['keep.py']==sentinel
        report.append(dict(profile=profile,pause_save_reload_exit='pass',stack_bytes=max(c.stack_used(),d.stack_used()),frames=frames,slowest_draw_ms=slowest,frames_over_33ms=slow));print(profile,report[-1],flush=True)
    emu.configure();c=emu.Calculator(nwa,nwlink=nw);c.enable_checks();c.keys=presses((200,'ok',60));c.run(400)
    for i in range(26 if dune else 60):
        fixture=(ROOT/('output/stress.bin' if dune else f'output/fixture{i}.bin')).read_bytes();assert len(fixture)==size
        c.uc.mem_write(c.symbols['game'],fixture)
        if dune:c.uc.mem_write(c.symbols['stats'],struct.pack('<IIIIIIBBxx',100,50000,(1<<26)-1,127,1,1234,i,i%7))
        c.run(c.now_ms+75);assert not c.violations,sorted(set(c.violations))
        if i in (0,3,6,12,25,49,59):c.save_png(str(out/f'scene{i}.png'))
    if not dune:
        c.uc.mem_write(c.symbols['game'],(ROOT/'output/stress.bin').read_bytes());c.run(c.now_ms+180);assert not c.violations
    report.append(dict(scenes='26 balls and all 7 landscapes' if dune else '60 original levels',slowest_draw_ms=uint(c,'perf_max'),stack_bytes=c.stack_used()))
    c.keys=presses((c.now_ms+100,'home',180));c.run(c.now_ms+700);check_exit(c)
    # Last level completes at 60/60 and replays level 60 rather than creating 61.
    c=emu.Calculator(nwa,nwlink=nw);c.enable_checks();c.keys=presses((200,'ok',60));c.run(400)
    fixture=bytearray((ROOT/'output/fixture59.bin').read_bytes());fixture[host['remaining_offset']]=0
    c.uc.mem_write(c.symbols['game'],bytes(fixture));c.keys=[];c.run(c.now_ms+1400)
    assert uint(c,'current_view')==3 and struct.unpack('<II',bytes(c.uc.mem_read(c.symbols['stats'],8)))==(59,60)
    c.save_png(str(out/'complete.png'));now=c.now_ms;c.keys=presses((now+100,'ok',80),(now+500,'home',180));c.run(now+350)
    assert bytes(c.uc.mem_read(c.symbols['game']+host['level_offset'],1))==bytes([59]);c.run(now+1000);check_exit(c)
    report.append(dict(ending='60/60 saved, last level replayed without level 61'))
    st=out/'full.bin';payload=b'x'*(emu.STORAGE_SIZE-2-2-len('keep.py')-1);storage(st,[('keep.py',payload)])
    c=emu.Calculator(nwa,storage_file=str(st),nwlink=nw);c.enable_checks();c.keys=presses((400,'home',180),(1100,'1',80),(1600,'home',100),(2200,'back',80))
    c.run(900);assert not c.exited and uint(c,'current_view')==6;c.save_png(str(out/'storage-full.png'));c.run(1400);assert not c.exited;c.run(2800);check_exit(c)
    assert records(st.read_bytes())=={'keep.py':payload};report.append(dict(full_storage='continue and exit preserve other files'))
    c=emu.Calculator(nwa,nwlink=nw);c.enable_checks();c.keys=presses((0,'ok',600),(850,'home',180));c.run(700);assert uint(c,'current_view')==0;c.run(1500);check_exit(c)
    (out/'report.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))
if __name__=='__main__':main()
