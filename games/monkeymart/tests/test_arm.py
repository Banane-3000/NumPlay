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
    nw=['node',a.nwlink]if Path(a.nwlink).is_file()and not a.nwlink.endswith(('.exe','.cmd'))else shlex.split(a.nwlink)
    out=ROOT/'output/arm';out.mkdir(parents=True,exist_ok=True);tempfile.tempdir=str(out)
    host=json.loads((ROOT/'output/host-report.json').read_text());size=host['game_bytes'];nwa=str(ROOT/'output/monkeymart-debug.nwa');report=[];sentinel=b'print(42)\n'
    for profile,model,system,ram in [('n0120','n0120','epsilon',153676),('n0110','n0110','epsilon',148928),('n0115','n0110','epsilon',133652),('upsilon','n0110','upsilon',107674)]:
        emu.configure(model,system,ram);st=out/(profile+'.bin');storage(st,[('keep.py',sentinel)])
        c=emu.Calculator(nwa,storage_file=str(st),nwlink=nw);c.enable_checks();c.keys=presses((400,'ok',80),(600,'right',200),(1000,'back',80),(2000,'back',80),(2500,'home',150))
        c.run(300);c.save_png(str(out/(profile+'-title.png')));c.run(1400)
        assert uint(c,'current_view')==2,('pause',uint(c,'current_view'));before=bytes(c.uc.mem_read(c.symbols['game'],size));c.run(1800);assert bytes(c.uc.mem_read(c.symbols['game'],size))==before
        c.run(2300);c.save_png(str(out/(profile+'-play.png')));draw=uint(c,'perf_max');frames=uint(c,'perf_frames');c.run(3100);check_exit(c,system=='upsilon');saved=records(st.read_bytes());assert saved['keep.py']==sentinel and len(saved['monkeymart.sav'])==768
        d=emu.Calculator(nwa,storage_file=str(st),nwlink=nw);d.enable_checks();d.keys=presses((450,'onoff',150));d.run(250);d.run(1000);check_exit(d,system=='upsilon');assert records(st.read_bytes())['keep.py']==sentinel
        report.append(dict(profile=profile,start_move_pause_save_reload_exit='pass',frames=frames,slowest_draw_ms=draw,stack_bytes=max(c.stack_used(),d.stack_used())));print(report[-1],flush=True)
    for profile,model,ram in [('n0120','n0120',153676),('n0110','n0110',148928),('n0115','n0110',133652)]:
        emu.configure(model,'epsilon',ram)
        for m in [1,2]:
            st=out/f'{profile}-mart{m}.bin';storage(st,[('keep.py',sentinel),('monkeymart.sav',(ROOT/f'output/mart{m}.sav').read_bytes())])
            c=emu.Calculator(nwa,storage_file=str(st),nwlink=nw);c.enable_checks();c.keys=presses((500,'ok',80),(10700,'home',180));c.run(10400)
            ticks=struct.unpack('<I',bytes(c.uc.mem_read(c.symbols['game']+host['tick_offset'],4)))[0];assert 560<=ticks<=605,(profile,m,ticks)
            c.save_png(str(out/f'{profile}-mart{m}.png'));r=dict(profile=profile,full_mart=m,simulation_ticks=ticks,frames=uint(c,'perf_frames'),slowest_draw_ms=uint(c,'perf_max'),frames_over_33ms=uint(c,'perf_slow'));c.run(11400);check_exit(c);assert records(st.read_bytes())['keep.py']==sentinel;r['stack_bytes']=c.stack_used();report.append(r);print(r,flush=True)
    emu.configure();c=emu.Calculator(nwa,nwlink=nw);c.enable_checks();c.keys=presses((300,'ok',80),(500,'left',400),(1000,'down',710),(4300,'home',150));c.run(4000)
    coins=struct.unpack('<I',bytes(c.uc.mem_read(c.symbols['game']+host['coins_offset'],4)))[0];assert coins==25,('Real keyboard checkout purchase',coins);c.save_png(str(out/'first-checkout.png'));c.run(5000);check_exit(c);report.append(dict(real_keyboard_purchase='Starting $35 -> checkout built -> $25; no injected state'))
    st=out/'reset.bin';storage(st,[('keep.py',sentinel),('monkeymart.sav',(ROOT/'output/mart1.sav').read_bytes())])
    c=emu.Calculator(nwa,storage_file=str(st),nwlink=nw);c.enable_checks();c.keys=presses((300,'ok',60),(600,'back',60),(800,'up',60),(1000,'ok',60),(1200,'back',60),(1400,'ok',60),(1600,'down',60),(1800,'ok',60),(2200,'home',100))
    c.run(1100);assert uint(c,'current_view')==8;c.save_png(str(out/'reset-confirm.png'));before=bytes(c.uc.mem_read(c.symbols['game'],host['progress_bytes']));c.run(1300);assert uint(c,'current_view')==2;assert bytes(c.uc.mem_read(c.symbols['game'],host['progress_bytes']))==before
    c.run(2000);assert uint(c,'current_view')==0;assert struct.unpack('<I',bytes(c.uc.mem_read(c.symbols['game']+host['coins_offset'],4)))[0]==35;c.run(2800);check_exit(c);r=records(st.read_bytes());assert r['keep.py']==sentinel;assert struct.unpack_from('<I',r['monkeymart.sav'],4)[0]==35;report.append(dict(reset='Cancel preserves progress; explicit reset saves a fresh $35 game and preserves other files'))
    st=out/'full.bin';payload=b'x'*(emu.STORAGE_SIZE-2-2-len('keep.py')-1);storage(st,[('keep.py',payload)])
    c=emu.Calculator(nwa,storage_file=str(st),nwlink=nw);c.enable_checks();c.keys=presses((400,'home',150),(1100,'1',80),(1700,'home',150),(2400,'back',80));c.run(900);assert not c.exited and uint(c,'current_view')==9;c.save_png(str(out/'storage-full.png'));c.run(1500);assert not c.exited;c.run(3000);check_exit(c);assert records(st.read_bytes())=={'keep.py':payload}
    report.append(dict(full_storage='retry, continue, safe exit; other records preserved'))
    c=emu.Calculator(nwa,nwlink=nw);c.enable_checks();c.keys=presses((0,'ok',600),(900,'home',150));c.run(700);assert uint(c,'current_view')==0;c.run(1600);check_exit(c)
    (out/'report.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))
if __name__=='__main__':main()
