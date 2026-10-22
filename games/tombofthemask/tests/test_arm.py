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
    host=json.loads((ROOT/'output/host-report.json').read_text());nwa=str(ROOT/'output/tombofthemask-debug.nwa');report=[];sentinel=b'print(42)\n'
    for profile,model,system,ram in [('n0120','n0120','epsilon',153676),('n0110','n0110','epsilon',148928),('n0115','n0110','epsilon',133652),('upsilon','n0110','upsilon',107674)]:
        emu.configure(model,system,ram);st=out/(profile+'.bin');storage(st,[('keep.py',sentinel)])
        c=emu.Calculator(nwa,storage_file=str(st),nwlink=nw);c.enable_checks();c.keys=presses((400,'ok',80),(650,'ok',80),(900,'right',80),(1250,'up',80),(1700,'left',80),(2200,'back',80),(2700,'back',80),(3200,'home',120))
        c.run(300);c.save_png(str(out/(profile+'-title.png')));c.run(2050);assert uint(c,'current_view')==2
        c.save_png(str(out/(profile+'-play.png')));c.run(2350);assert uint(c,'current_view')==3
        before=bytes(c.uc.mem_read(c.symbols['game'],host['game_bytes']));c.run(2550);assert before==bytes(c.uc.mem_read(c.symbols['game'],host['game_bytes']))
        c.run(3000);draw=uint(c,'perf_max');frames=uint(c,'perf_frames');c.run(3800);check_exit(c,system=='upsilon')
        saved=records(st.read_bytes());assert saved['keep.py']==sentinel and len(saved['tombmask.sav'])==320
        d=emu.Calculator(nwa,storage_file=str(st),nwlink=nw);d.enable_checks();d.keys=presses((500,'onoff',150));d.run(1000);check_exit(d,system=='upsilon')
        assert records(st.read_bytes())['keep.py']==sentinel
        report.append(dict(profile=profile,move_pause_save_reload_exit='pass',frames=frames,slowest_draw_ms=draw,stack_bytes=max(c.stack_used(),d.stack_used())));print(report[-1],flush=True)
    emu.configure();c=emu.Calculator(nwa,nwlink=nw);c.enable_checks();c.keys=presses((0,'ok',1000),(1300,'home',120));c.run(500);assert uint(c,'current_view')==0;c.run(1900);check_exit(c)
    # Visit every secondary menu using only real calculator key events.
    c=emu.Calculator(nwa,nwlink=nw);c.enable_checks();c.keys=presses((300,'0',80),(700,'back',80),(1000,'down',80),(1300,'ok',80),(1650,'right',80),(1900,'back',80),(2200,'right',80),(2500,'ok',80),(2800,'back',80),(3200,'home',120))
    c.run(500);assert uint(c,'current_view')==8;c.save_png(str(out/'help.png'));c.run(1500);assert uint(c,'current_view')==7;c.save_png(str(out/'masks.png'));c.run(2700);assert uint(c,'current_view')==6;c.save_png(str(out/'shop.png'));c.run(3800);check_exit(c)
    # Arcade starts from the same menu and uses its own rising hazard.
    c=emu.Calculator(nwa,nwlink=nw);c.enable_checks();c.keys=presses((300,'right',80),(550,'ok',80),(900,'up',80),(1600,'right',80),(2300,'up',80),(5000,'home',120));c.run(4500);c.save_png(str(out/'arcade.png'));c.run(5700);check_exit(c)
    st=out/'full.bin';filler=b'x'*(emu.STORAGE_SIZE-24);storage(st,[('full.py',filler)])
    c=emu.Calculator(nwa,storage_file=str(st),nwlink=nw);c.enable_checks();c.keys=presses((400,'home',100),(1000,'ok',80),(1400,'1',80),(1800,'home',80),(2300,'back',80));c.run(700);assert uint(c,'current_view')==10;c.run(1200);assert uint(c,'current_view')==10;c.run(1600);assert uint(c,'current_view')==0;c.run(2900);check_exit(c);assert records(st.read_bytes())['full.py']==filler
    report.append(dict(launch_held_keys='pass',menus='pass',arcade='pass',full_storage_retry_continue_exit='pass'))
    # Complete stage 1 with all three stars, through real key events, then check the saved unlock.
    st=out/'complete-stage.bin';storage(st,[('keep.py',sentinel)]);route=json.loads((ROOT/'tests/first-stage-route.json').read_text())
    events=[(300,'ok',70),(550,'ok',70)]+[(900+i*600,k,70)for i,k in enumerate(route)]
    finish=900+len(route)*600;c=emu.Calculator(nwa,storage_file=str(st),nwlink=nw);c.enable_checks();c.keys=presses(*events,(finish+500,'home',120));c.run(finish)
    assert uint(c,'current_view')==4,'The complete stage route did not reach the results screen';c.save_png(str(out/'stage-clear.png'));c.run(finish+1100);check_exit(c)
    p=records(st.read_bytes())['tombmask.sav'];assert p[24]==2 and p[32]==3;report.append(dict(real_keys_stage_clear='Stage 1: all three stars, exit reached, stage 2 unlocked and saved'))
    st=out/'meta.bin';storage(st,[('keep.py',sentinel)]);c=emu.Calculator(nwa,storage_file=str(st),nwlink=nw);c.enable_checks();c.keys=presses((300,'1',80),(700,'right',80),(950,'back',80),(1250,'2',80),(1500,'ok',80),(3500,'back',80),(3800,'home',100));c.run(500);assert uint(c,'current_view')==11;c.save_png(str(out/'missions.png'));c.run(1450);assert uint(c,'current_view')==12;c.run(3300);c.save_png(str(out/'chest.png'));c.run(4400);check_exit(c);p=records(st.read_bytes())['tombmask.sav'];assert p[310]==0 and p[308]==1;report.append(dict(missions_chest_reward_save='pass'))
    for profile,model,ram in [('n0120','n0120',153676),('n0110','n0110',148928)]:
        emu.configure(model,'epsilon',ram);st=out/(profile+'-large-stage.bin');storage(st,[('keep.py',sentinel),('tombmask.sav',(ROOT/'output/stage190.sav').read_bytes())]);c=emu.Calculator(nwa,storage_file=str(st),nwlink=nw);c.enable_checks();c.keys=presses((300,'ok',70),(550,'ok',70),(800,'ok',70),(1100,'left',70),(1700,'up',70),(2300,'right',70),(2900,'down',70),(6500,'home',100));c.run(6000);r=dict(profile=profile,largest_stage=190,slowest_draw_ms=uint(c,'perf_max'),frames=uint(c,'perf_frames'),frames_over_33ms=uint(c,'perf_slow'));c.save_png(str(out/(profile+'-stage190.png')));c.run(7200);check_exit(c);r['stack_bytes']=c.stack_used();report.append(r);print(r,flush=True)
    (out/'report.json').write_text(json.dumps(report,indent=2)+'\n');print('ARM tests passed',flush=True)
if __name__=='__main__':main()
