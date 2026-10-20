#!/usr/bin/env python3
"""Run the actual Cortex-M7 application on NumPlay's four emulator profiles.

python tests/arm.py --nwlink /path/to/nwlink
Requires pyelftools, Unicorn and Pillow. Run the build and host test first
(the host test creates the reference input and save fixtures).
"""
import argparse
import io
import json
from pathlib import Path
import struct
import tempfile
import types

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'output'
# The shared emulator leaves its ELF stream open; use an in-memory stream so
# Windows can remove its temporary directory. Do not change the shared tool.
emu_path = ROOT.parents[1] / 'tools/emu.py'
emu = types.ModuleType('doodlejump_emu')
emu.__file__ = str(emu_path)
emu.io = io
source = emu_path.read_text().replace('ELFFile(open(elf, "rb"))',
     'ELFFile(io.BytesIO(open(elf, "rb").read()))')
# Short CPU slices keep checkpoints within a few milliseconds even on a static
# menu where thousands of sleep calls can otherwise fit in a single slice.
source = source.replace('chunk = CPU_HZ // 1000', 'chunk = CPU_HZ // 10000')
source = source.replace('self.now_ms += 1\n', 'self.now_ms += 0.1\n')
# Keep static-menu checkpoints bounded at completed sleeps.
source = source.replace('        next_frame = self.now_ms if frame_ms else None',
    '        self.checkpoint = until_ms\n        next_frame = self.now_ms if frame_ms else None')
source = source.replace('            self.slept_ms += r0\n',
    '            self.slept_ms += r0\n            if self.now_ms >= self.checkpoint: uc.emu_stop()\n')
exec(compile(source, str(emu_path), 'exec'), emu.__dict__)

def record(name, value):
    data = name.encode() + b'\0' + value
    return struct.pack('<H', len(data)+2) + data

def records(data):
    result = {}
    pos = 0
    while True:
        n, = struct.unpack_from('<H', data, pos)
        if not n:
            return result
        assert n >= 4 and pos+n <= len(data)
        end = data.index(0, pos+2, pos+n)
        result[data[pos+2:end].decode()] = data[end+1:pos+n]
        pos += n

def presses(*items):
    return [(t,t+d,emu.KEYS[k]) for t,k,d in items]

def u32(c, name):
    return struct.unpack('<I', c.uc.mem_read(c.symbols[name], 4))[0]

def safe(c, exited=True):
    assert c.exited == exited
    assert not c.violations, c.violations
    assert not c.firmware_ram_changes(), c.firmware_ram_changes()
    if exited:
        assert c.app_ram_clean() and c.locks == 0

def state(c):
    # Layout checked by offsetof assertions in the host/ARM C tests.
    return bytes(c.uc.mem_read(c.symbols['game'],308))

def performance(c):
    result={k:u32(c,k) for k in ('perf_frames','perf_max','perf_slow')}
    assert result['perf_max']<=32 and result['perf_slow']==0,result
    return result

def steering(g):
    x,y,vx,vy,camera=struct.unpack_from('<5i',g)
    tick=struct.unpack_from('<I',g,32)[0];boost=g[50];best=None
    def delta(a,b):
        return a-b
    def position(base,kind,phase,time,width):
        t=(time+phase)&127;wave=t-32 if t<64 else 95-t
        return max(((width+1)//2)*256,min((288-(width+1)//2)*256,base*256+(wave*128 if kind==1 else 0)))
    # Avoid rising into a monster's underside; descending onto it is safe.
    for i in range(3):
        ey,ex,phase,active=struct.unpack_from('<iH2B',g,248+i*8)
        t=(tick+phase)&127;wave=t-32 if t<64 else 95-t
        dx=delta(max(18*256,min(270*256,ex*256+wave*96)),x)
        if active and vy>0 and not boost and -15*256<ey-y<80*256 and abs(dx)<30*256:
            avoid=-1 if dx>=0 else 1
            return 0 if (avoid<0 and x<46*256) or (avoid>0 and x>242*256) else avoid
    for i in range(16):
        py,px,width,kind,item,phase,active=struct.unpack_from('<iH5B',g,56+i*12)
        if not active or kind==2 or py<camera-12*256:continue
        yy,vv,bb=y,vy,boost
        for t in range(1,200):
            old=yy
            if bb:bb-=1;vv=1536
            else:vv=max(-2304,vv-47)
            yy+=vv
            if vv<0 and old>=py and yy<=py:
                aim=position(px,kind,phase,tick+t,width)
                if abs(delta(aim,x))<max(0,t-7)*672+(width//2+5)*256 and (best is None or py>best[0]):best=(py,aim)
                break
            if yy<camera-24*256:break
    if best is None:return 0
    dx=delta(best[1],x+vx*3)
    return 0 if abs(dx)<3*256 else 1 if dx>0 else -1

def main():
    ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('--nwlink',default='nwlink');a=ap.parse_args()
    (OUT/'tmp').mkdir(exist_ok=True);tempfile.tempdir=str(OUT/'tmp')
    nwa=str(OUT/'doodlejump-debug.nwa');report=[]
    for name,model,system,ram in [('n0110','n0110','epsilon',148928),('n0115-custom','n0110','epsilon',133652),('n0120','n0120','epsilon',153676),('upsilon','n0110','upsilon',107674)]:
        emu.configure(model,system,ram);storage=OUT/(name+'-storage.bin')
        sentinel=record('untouched.py',b'print(42)\n');storage.write_bytes(sentinel+b'\0\0')
        c=emu.Calculator(nwa,storage_file=str(storage),nwlink=a.nwlink);c.enable_checks()
        c.keys=presses((0,'ok',300),(600,'ok',70),(1000,'ok',70),(1150,'up',500),(1750,'back',70),(1950,'down',70),(2100,'ok',70),(2250,'back',70),(2400,'up',70),(2550,'ok',70),(2700,'ok',70),(3000,'home',120))
        c.run(550);c.save_png(str(OUT/(name+'-title.png')))
        c.run(1280);g=state(c);assert g[46]==1 and sum(g[280+i*12] for i in range(3))==1
        c.run(1690);g=state(c);assert g[51]==0 and sum(g[280+i*12] for i in range(3))==0,'Held shoot key repeated'
        c.save_png(str(OUT/(name+'-gameplay.png')))
        c.run(1900);saved=records(c.storage_bytes())['doodlejump.sav']
        assert len(saved)==384 and saved[:4]==b'DJN3' and saved[80]==1
        frozen_tick=struct.unpack_from('<I',saved,64)[0];c.save_png(str(OUT/(name+'-pause.png')))
        c.run(2380);assert records(c.storage_bytes())['doodlejump.sav']==saved
        assert struct.unpack_from('<I',state(c),32)[0]==frozen_tick,'Pause/confirmation advanced the world'
        c.run(2900);perf=performance(c);c.run(3300);safe(c)
        saved=records(c.storage_bytes())['doodlejump.sav']
        assert struct.unpack_from('<I',saved,8)[0]==1,'Resume counted as a new expedition'
        assert records(c.storage_bytes())['untouched.py']==b'print(42)\n'
        stack=c.stack_used();recs=records(c.storage_bytes());recs['later.py']=b'x=123\n'
        storage.write_bytes(b''.join(record(k,v) for k,v in recs.items())+b'\0\0')
        c2=emu.Calculator(nwa,storage_file=str(storage),nwlink=a.nwlink);c2.enable_checks()
        c2.keys=presses((500,'ok',70),(900,'ok',70),(1150,'left',160),(1450,'onoff',120));c2.run(1800);safe(c2)
        recs=records(c2.storage_bytes());assert recs['later.py']==b'x=123\n' and recs['untouched.py']==b'print(42)\n'
        assert struct.unpack_from('<I',recs['doodlejump.sav'],8)[0]==1
        for direction,wall in [('left',18),('right',270)]:
            edge_storage=OUT/(name+'-'+direction+'-storage.bin')
            edge_storage.write_bytes(record('doodlejump.sav',(OUT/'edge-save.bin').read_bytes())+b'\0\0')
            edge=emu.Calculator(nwa,storage_file=str(edge_storage),nwlink=a.nwlink);edge.enable_checks()
            edge.keys=presses((500,'ok',70),(900,'ok',70),(1000,direction,1200),(2300,'home',100))
            edge.run(980);last=struct.unpack_from('<i',state(edge))[0]
            for t in range(1050,2151,50):
                edge.run(t);xx=struct.unpack_from('<i',state(edge))[0]
                assert 18*256<=xx<=270*256 and abs(xx-last)<24*256,'Player crossed a wall'
                last=xx
            assert last==wall*256 and struct.unpack_from('<i',state(edge),8)[0]==0
            edge.save_png(str(OUT/(name+'-'+direction+'-wall.png')))
            edge.run(2600);safe(edge);stack=max(stack,edge.stack_used())
        report.append(dict(profile=name,stack=max(stack,c2.stack_used()),**perf))
        print(name+': '+json.dumps(report[-1]),flush=True)
    # A packed 16-platform/3-monster/3-shot rocket scene tests full redraws.
    emu.configure('n0110','epsilon',148928);storage=OUT/'stress-storage.bin'
    storage.write_bytes(record('keep.py',b'hello\n')+record('doodlejump.sav',(OUT/'stress-save.bin').read_bytes())+b'\0\0')
    c=emu.Calculator(nwa,storage_file=str(storage),nwlink=a.nwlink);c.enable_checks()
    c.keys=presses((500,'ok',70),(900,'ok',70),(1250,'right',250),(1750,'up',70),(2300,'back',70),(2800,'home',100))
    c.run(1400);c.save_png(str(OUT/'arm-rocket.png'));c.run(2200);stress=performance(c);c.run(3100);safe(c)
    assert records(c.storage_bytes())['keep.py']==b'hello\n'
    storage=OUT/'climb-storage.bin'
    storage.write_bytes(record('doodlejump.sav',(OUT/'climb-save.bin').read_bytes())+b'\0\0')
    c=emu.Calculator(nwa,storage_file=str(storage),nwlink=a.nwlink);c.enable_checks()
    c.keys=presses((500,'ok',70),(900,'ok',70),(1600,'back',70),(2100,'home',100))
    c.run(1350);c.save_png(str(OUT/'arm-climb.png'));c.run(2400);safe(c)
    # A read-only controller plays through ordinary held/released key events.
    storage=OUT/'keyboard-climb-storage.bin'
    storage.write_bytes(record('doodlejump.sav',(OUT/'start-save.bin').read_bytes())+b'\0\0')
    c=emu.Calculator(nwa,storage_file=str(storage),nwlink=a.nwlink);c.enable_checks()
    c.keys=presses((500,'ok',70),(900,'ok',70));c.run(915)
    last_fire=-16
    for _ in range(5000):
        g=state(c);tick=struct.unpack_from('<I',g,32)[0];peak=struct.unpack_from('<i',g,20)[0]//256-20
        if peak>=1200 or g[46]==2:break
        steer=steering(g)
        if tick-last_fire>=16:last_fire=tick
        fire=tick-last_fire<4
        key=(emu.KEYS['left'] if steer<0 else emu.KEYS['right'] if steer>0 else 0)|(emu.KEYS['up'] if fire else 0)
        c.keys=[(c.now_ms,c.now_ms+20,key)];c.run(c.now_ms+8)
    c.save_png(str(OUT/'arm-keyboard-last.png'))
    assert peak>=1200,(peak,tick,g[46],g[49])
    c.save_png(str(OUT/'arm-keyboard-climb.png'));keyboard=performance(c)
    keyboard.update(height=peak,ticks=tick,jumps=struct.unpack_from('<I',g,40)[0])
    c.keys=[(c.now_ms,c.now_ms+150,emu.KEYS['home'])];c.run(c.now_ms+500);safe(c)
    # Options persist even without starting a game.
    emu.configure('n0120','epsilon',153676);storage=OUT/'journal-storage.bin';storage.write_bytes(b'\0\0')
    c=emu.Calculator(nwa,storage_file=str(storage),nwlink=a.nwlink);c.enable_checks()
    c.keys=presses((500,'down',70),(800,'ok',70),(1100,'ok',70),(1500,'back',70),(1750,'down',70),(2000,'ok',70),(2550,'back',70),(2900,'home',100))
    c.run(1400);c.save_png(str(OUT/'arm-journal.png'));c.run(2400);c.save_png(str(OUT/'arm-guide.png'));c.run(3300);safe(c)
    assert records(c.storage_bytes())['doodlejump.sav'][28]==0
    storage=OUT/'full-storage.bin';full=record('large.py',b'#'*(emu.STORAGE_SIZE-16));storage.write_bytes(full+b'\0\0')
    c=emu.Calculator(nwa,storage_file=str(storage),nwlink=a.nwlink);c.enable_checks();c.keys=presses((500,'home',300))
    c.run(1200);safe(c,False);c.save_png(str(OUT/'arm-storage-full.png'));assert c.storage_bytes()[:len(full)]==full
    c.keys+=presses((1400,'ok',70),(1800,'back',70));c.run(2200);safe(c);assert c.storage_bytes()[:len(full)]==full
    (OUT/'arm-validation.json').write_text(json.dumps(dict(profiles=report,stress=stress,keyboard=keyboard),indent=2)+'\n')
    print('PASS: four ARM profiles, held input, pause/cancel/resume, saves, full storage, rocket stress; '+json.dumps(stress),flush=True)

if __name__=='__main__':
    main()
