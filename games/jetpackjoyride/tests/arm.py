#!/usr/bin/env python3
"""Execute the compiled app on all four NumPlay ARM emulator profiles.
Build first and run tests/test.c to produce save fixtures. Requires Unicorn,
Pillow and pyelftools. --nwlink accepts a command wrapper on Windows.
"""
import argparse
import io
import json
from pathlib import Path
import struct
import tempfile
import types

ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'output'
emu_path=ROOT.parents[1]/'tools/emu.py'
emu=types.ModuleType('jetpack_emu');emu.__file__=str(emu_path);emu.io=io
source=emu_path.read_text().replace('ELFFile(open(elf, "rb"))','ELFFile(io.BytesIO(open(elf, "rb").read()))')
source=source.replace('chunk = CPU_HZ // 1000','chunk = CPU_HZ // 10000')
source=source.replace('self.now_ms += 1\n','self.now_ms += 0.1\n')
# A static menu can execute many sleeps in a single instruction slice. Stop
# at a completed sleep when a checkpoint is reached; CPU/game state is intact.
source=source.replace('        next_frame = self.now_ms if frame_ms else None',
    '        self.checkpoint = until_ms\n        next_frame = self.now_ms if frame_ms else None')
source=source.replace('            self.slept_ms += r0\n',
    '            self.slept_ms += r0\n            if self.now_ms >= self.checkpoint: uc.emu_stop()\n')
exec(compile(source,str(emu_path),'exec'),emu.__dict__)

def record(name,value):
    data=name.encode()+b'\0'+value
    return struct.pack('<H',len(data)+2)+data

def records(data):
    result={};pos=0
    while True:
        n,=struct.unpack_from('<H',data,pos)
        if not n:return result
        assert n>=4 and pos+n<=len(data)
        end=data.index(0,pos+2,pos+n)
        result[data[pos+2:end].decode()]=data[end+1:pos+n];pos+=n

def presses(*items):return [(t,t+d,emu.KEYS[k]) for t,k,d in items]
def state(c):return bytes(c.uc.mem_read(c.symbols['game'],232))
def u32(c,s):return struct.unpack('<I',c.uc.mem_read(c.symbols[s],4))[0]
def safe(c,exited=True):
    assert c.exited==exited
    assert not c.violations,c.violations
    assert not c.firmware_ram_changes(),c.firmware_ram_changes()
    if exited:assert c.app_ram_clean() and c.locks==0
def performance(c):
    result={k:u32(c,k) for k in ('perf_frames','perf_max','perf_slow')}
    assert result['perf_max']<=32 and result['perf_slow']==0,result
    return result

def controller(g):
    y,vy=struct.unpack_from('<2i',g);nearest=1000000;target=120
    for i in range(4):
        x,yy,kind,timer,active=struct.unpack_from('<iH3B',g,40+i*12)
        width=84 if kind==4 else 58 if kind==0 else 24 if kind==2 else 10
        if active and x+width*128>=54*256 and x<nearest:
            nearest=x
            if kind==2:target=120 if timer>35 else 165 if yy<120 else 75
            elif kind==4:target=160 if yy<120 else 80
            elif kind==3:target=150 if yy<120 else 90
            elif kind==0:target=150 if yy>175 else 95 if yy<70 else 80
            else:
                target=180 if yy<120 else 60
                for j in range(4):
                    xx,y2,k2,t2,a2=struct.unpack_from('<iH3B',g,40+j*12)
                    if j!=i and a2 and xx==x and k2==1:target=(yy+y2)//2
    if nearest==1000000:
        for i in range(18):
            x,yy,kind,active=struct.unpack_from('<iH2B',g,88+i*8)
            if active and 44*256<x<nearest:nearest=x;target=yy
    return y+vy*10>target*256

def main():
    ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('--nwlink',default='nwlink');a=ap.parse_args()
    (OUT/'tmp').mkdir(exist_ok=True);tempfile.tempdir=str(OUT/'tmp');nwa=str(OUT/'jetpackjoyride-debug.nwa')
    report=[]
    for name,model,system,ram in [('n0110','n0110','epsilon',148928),('n0115-custom','n0110','epsilon',133652),('n0120','n0120','epsilon',153676),('upsilon','n0110','upsilon',107674)]:
        emu.configure(model,system,ram);storage=OUT/(name+'-storage.bin')
        storage.write_bytes(record('keep.py',b'print(42)\n')+b'\0\0')
        c=emu.Calculator(nwa,storage_file=str(storage),nwlink=a.nwlink);c.enable_checks()
        c.keys=presses((0,'ok',300),(600,'ok',70),(1000,'ok',70),(1150,'up',500),(1750,'back',70),(1950,'down',70),(2100,'ok',70),(2250,'back',70),(2400,'up',70),(2550,'ok',70),(2700,'ok',70),(3000,'home',120))
        c.run(550);assert not state(c)[31],'Held launch key entered a game';c.save_png(str(OUT/(name+'-title.png')))
        c.run(1640);g=state(c);assert g[30]==1 and g[32]==1 and g[33]==1
        c.save_png(str(OUT/(name+'-gameplay.png')))
        c.run(1900);saved=records(c.storage_bytes())['jetpackjoyride.sav'];tick=struct.unpack_from('<I',saved,24)[0]
        assert len(saved)==260 and saved[:4]==b'JJN2';c.save_png(str(OUT/(name+'-pause.png')))
        c.run(2380);assert records(c.storage_bytes())['jetpackjoyride.sav']==saved
        assert struct.unpack_from('<I',state(c),16)[0]==tick,'Pause/cancel changed world'
        c.run(2900);perf=performance(c);c.run(3300);safe(c)
        saved=records(c.storage_bytes())['jetpackjoyride.sav'];assert struct.unpack_from('<I',saved,8)[0]==1
        assert records(c.storage_bytes())['keep.py']==b'print(42)\n'
        stack=c.stack_used();recs=records(c.storage_bytes());recs['later.py']=b'x=123\n'
        storage.write_bytes(b''.join(record(k,v) for k,v in recs.items())+b'\0\0')
        c2=emu.Calculator(nwa,storage_file=str(storage),nwlink=a.nwlink);c2.enable_checks()
        c2.keys=presses((500,'ok',70),(900,'ok',70),(1150,'exe',160),(1450,'onoff',120))
        c2.run(800)
        assert state(c2)==saved[32:44]+saved[20:32]+saved[44:58]+b'\0\0'+saved[64:256],'Resume did not restore exact simulation state'
        c2.run(1800);safe(c2)
        recs=records(c2.storage_bytes());assert recs['later.py']==b'x=123\n' and recs['keep.py']==b'print(42)\n'
        assert struct.unpack_from('<I',recs['jetpackjoyride.sav'],8)[0]==1
        # Real shop navigation: cancel, buy with a held OK, cancel another
        # purchase, relaunch, equip owned suits and reject insufficient funds.
        storage=OUT/(name+'-shop-storage.bin');sentinel=record('keep.py',b'shop sentinel\n')
        storage.write_bytes(sentinel+record('jetpackjoyride.sav',(OUT/'shop-save.bin').read_bytes())+b'\0\0')
        c3=emu.Calculator(nwa,storage_file=str(storage),nwlink=a.nwlink);c3.enable_checks()
        c3.keys=presses((500,'down',70),(700,'ok',70),(1000,'down',70),(1200,'ok',70),(1400,'ok',70),(1600,'ok',70),(1800,'down',70),(2000,'ok',300),(2600,'down',70),(2800,'ok',70),(3000,'back',70),(3200,'back',70),(3500,'home',100))
        c3.run(1350);c3.save_png(str(OUT/(name+'-buy.png')))
        c3.run(2400);c3.save_png(str(OUT/(name+'-shop.png')));shop_perf=performance(c3)
        c3.run(3900);safe(c3);recs=records(c3.storage_bytes());data=recs['jetpackjoyride.sav']
        assert struct.unpack_from('<I',data,58)[0]==150 and data[62:64]==b'\x02\x01'
        assert recs['keep.py']==b'shop sentinel\n';stack=max(stack,c2.stack_used(),c3.stack_used())
        c4=emu.Calculator(nwa,storage_file=str(storage),nwlink=a.nwlink);c4.enable_checks()
        c4.keys=presses((500,'right',70),(800,'ok',70),(1100,'ok',70),(1400,'down',70),(1600,'ok',70),(1800,'down',70),(2000,'ok',70),(2200,'up',70),(2400,'down',70),(2600,'down',70),(2800,'ok',70),(3200,'home',100))
        c4.run(3000);c4.save_png(str(OUT/(name+'-insufficient.png')));c4.run(3600);safe(c4)
        data=records(c4.storage_bytes())['jetpackjoyride.sav']
        assert struct.unpack_from('<I',data,58)[0]==30 and data[62:64]==b'\x06\x02'
        report.append(dict(profile=name,stack=max(stack,c4.stack_used()),shop_max=shop_perf['perf_max'],**perf));print(json.dumps(report[-1]),flush=True)
    emu.configure('n0110','epsilon',148928);storage=OUT/'stress-storage.bin'
    storage.write_bytes(record('jetpackjoyride.sav',(OUT/'stress-save.bin').read_bytes())+b'\0\0')
    c=emu.Calculator(nwa,storage_file=str(storage),nwlink=a.nwlink);c.enable_checks()
    c.keys=presses((500,'ok',70),(900,'ok',300),(1500,'back',70),(1800,'home',100))
    c.run(1150);c.save_png(str(OUT/'arm-stress.png'));c.run(1700);stress=performance(c);c.run(2200);safe(c)
    # Real keyboard input only: controller observes state but never writes it.
    storage=OUT/'keyboard-storage.bin';storage.write_bytes(record('jetpackjoyride.sav',(OUT/'start-save.bin').read_bytes())+b'\0\0')
    c=emu.Calculator(nwa,storage_file=str(storage),nwlink=a.nwlink);c.enable_checks()
    c.keys=presses((500,'ok',70),(900,'ok',70));c.run(980);seen=0;snapped=False;rotor_snapped=False
    for _ in range(10000):
        g=state(c);distance=struct.unpack_from('<I',g,20)[0]//2560
        for i in range(4):
            if g[48+i*12]:seen|=1<<g[46+i*12]
        visible=any(g[48+i*12] and g[46+i*12]<2 and 140*256<struct.unpack_from('<i',g,40+i*12)[0]<240*256 for i in range(4))
        if not snapped and distance>70 and visible:
            c.save_png(str(OUT/'arm-flight.png'));snapped=True
        visible_rotor=any(g[48+i*12] and g[46+i*12]==4 and 120*256<struct.unpack_from('<i',g,40+i*12)[0]<260*256 for i in range(4))
        if not rotor_snapped and visible_rotor:
            c.save_png(str(OUT/'arm-rotor-flight.png'));rotor_snapped=True
        if (distance>=650 and seen==31) or g[30]==2:break
        key=emu.KEYS['up'] if controller(g) else 0
        c.keys=[(c.now_ms,c.now_ms+40,key)];c.run(c.now_ms+16)
    c.save_png(str(OUT/'arm-keyboard-last.png'));assert distance>=650 and seen==31,(distance,seen,g[36])
    keyboard=performance(c);keyboard.update(meters=distance,hazards=struct.unpack_from('<H',g,26)[0],coins=struct.unpack_from('<H',g,24)[0],stack=c.stack_used())
    c.keys=[(c.now_ms,c.now_ms+150,emu.KEYS['home'])];c.run(c.now_ms+500);safe(c)
    # Four rotating beams test the maximum renderer load. Ordinary authored
    # rotor groups are covered separately by the live-keyboard run above.
    storage=OUT/'rotor-storage.bin'
    storage.write_bytes(record('jetpackjoyride.sav',(OUT/'rotor-stress-save.bin').read_bytes())+b'\0\0')
    c=emu.Calculator(nwa,storage_file=str(storage),nwlink=a.nwlink);c.enable_checks()
    c.keys=presses((500,'ok',70),(900,'ok',70),(1500,'back',70),(1800,'home',100))
    c.run(1050);c.save_png(str(OUT/'arm-rotors.png'))
    c.run(1350);c.save_png(str(OUT/'arm-rotors-turned.png'));rotors=performance(c)
    c.run(2200);safe(c);rotors['stack']=c.stack_used()
    # Guide/shop navigation and title Back exit.
    storage=OUT/'menus-storage.bin';storage.write_bytes(b'\0\0')
    c=emu.Calculator(nwa,storage_file=str(storage),nwlink=a.nwlink);c.enable_checks()
    c.keys=presses((500,'down',70),(800,'ok',70),(1400,'back',70),(1700,'right',70),(2000,'ok',70),(2600,'back',70),(3000,'back',70))
    c.run(1100);c.save_png(str(OUT/'arm-shop.png'));c.run(2400);c.save_png(str(OUT/'arm-guide.png'));c.run(3300);safe(c)
    # Import an actual save produced by the previous app, without losing its
    # world or coins; migrate only when writing the new format on exit.
    legacy=(ROOT/'tests/legacy-save.bin').read_bytes();storage=OUT/'legacy-storage.bin'
    storage.write_bytes(record('jetpackjoyride.sav',legacy)+b'\0\0')
    c=emu.Calculator(nwa,storage_file=str(storage),nwlink=a.nwlink);c.enable_checks()
    c.keys=presses((500,'down',70),(800,'ok',70),(1200,'home',100));c.run(1600);safe(c)
    migrated=records(c.storage_bytes())['jetpackjoyride.sav']
    assert migrated[:4]==b'JJN2' and migrated[4:58]==legacy[4:58] and migrated[64:256]==legacy[64:256]
    assert migrated[58:62]==legacy[12:16] and migrated[62:64]==b'\0\0'
    # A normal collision, result screen, and restart must count two flights.
    storage=OUT/'collision-storage.bin';storage.write_bytes(record('jetpackjoyride.sav',(OUT/'start-save.bin').read_bytes())+b'\0\0')
    c=emu.Calculator(nwa,storage_file=str(storage),nwlink=a.nwlink);c.enable_checks()
    c.keys=presses((500,'ok',70),(900,'ok',70));c.run(30000)
    assert state(c)[30]==2,'No-input flight should eventually collide';c.save_png(str(OUT/'arm-result.png'))
    t=c.now_ms;c.keys=presses((t+100,'ok',70),(t+400,'ok',70),(t+800,'home',100));c.run(t+1200);safe(c)
    assert struct.unpack_from('<I',records(c.storage_bytes())['jetpackjoyride.sav'],8)[0]==2
    storage=OUT/'full-storage.bin';full=record('large.py',b'#'*(emu.STORAGE_SIZE-16));storage.write_bytes(full+b'\0\0')
    c=emu.Calculator(nwa,storage_file=str(storage),nwlink=a.nwlink);c.enable_checks();c.keys=presses((500,'home',300))
    c.run(1200);safe(c,False);c.save_png(str(OUT/'arm-storage-full.png'));assert c.storage_bytes()[:len(full)]==full
    c.keys+=presses((1400,'1',70),(1700,'home',100),(2100,'ok',70),(2500,'back',70));c.run(2900);safe(c)
    assert c.storage_bytes()[:len(full)]==full
    result=dict(profiles=report,stress=stress,keyboard=keyboard,rotors=rotors)
    (OUT/'arm-validation.json').write_text(json.dumps(result,indent=2)+'\n')
    print('PASS: all profiles, input, pause/cancel/resume, persistence, stress, live keyboard, full storage; '+json.dumps(result),flush=True)

if __name__=='__main__':main()
