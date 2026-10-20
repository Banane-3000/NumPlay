#!/usr/bin/env python3
"""Run the actual Cortex-M7 application on NumPlay's four emulator profiles.

python tests/arm.py --nwlink /path/to/nwlink
Requires pyelftools, Unicorn and Pillow. Run the build and host test first.
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
emu = types.ModuleType('towerblock_emu')
emu.__file__ = str(emu_path)
emu.io = io
source = emu_path.read_text().replace('ELFFile(open(elf, "rb"))',
     'ELFFile(io.BytesIO(open(elf, "rb").read()))')
# Short CPU slices keep checkpoints within a few milliseconds even on a static
# menu where thousands of sleep calls can otherwise fit in a single slice.
source = source.replace('chunk = CPU_HZ // 1000', 'chunk = CPU_HZ // 10000')
source = source.replace('self.now_ms += 1\n', 'self.now_ms += 0.1\n')
# Stop on a completed sleep so static screens cannot skip checkpoints.
source=source.replace('        next_frame = self.now_ms if frame_ms else None',
    '        self.checkpoint = until_ms\n        next_frame = self.now_ms if frame_ms else None')
source=source.replace('            self.slept_ms += r0\n',
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

def menu_and_migration(nwa,nwlink,name):
    """Three title entries; old Practice cannot inflate the personal best."""
    storage=OUT/(name+'-migration-storage.bin')
    for filename,height,started in [('legacy-practice-save.bin',0,0),('legacy-classic-save.bin',3,1)]:
        old=(ROOT/'tests'/filename).read_bytes()
        storage.write_bytes(record('towerblock.sav',old)+b'\0\0')
        c=emu.Calculator(nwa,storage_file=str(storage),nwlink=nwlink);c.enable_checks()
        c.keys=presses((500,'home',120));c.run(900);safe(c)
        saved=records(c.storage_bytes())['towerblock.sav']
        assert saved[:4]==b'TBK2' and saved[8:12]==b'\0'*4 and saved[61]==0
        assert struct.unpack_from('<I',saved,4)[0]==5
        assert struct.unpack_from('<I',saved,28)[0]==height and saved[64]==started
        assert saved[12:27]==old[12:27]
    # Resume an existing normal tower without resetting it.
    c=emu.Calculator(nwa,storage_file=str(storage),nwlink=nwlink);c.enable_checks()
    c.keys=presses((400,'ok',80),(750,'home',120));c.run(1100);safe(c)
    assert records(c.storage_bytes())['towerblock.sav']==saved
    # A single Right selects Studio; Guide is the third and last title entry.
    storage.write_bytes(b'\0\0')
    c=emu.Calculator(nwa,storage_file=str(storage),nwlink=nwlink);c.enable_checks()
    c.keys=presses((400,'right',80),(650,'ok',80),(950,'right',80),(1200,'back',80),
                   (1450,'right',80),(1700,'ok',80),(2200,'back',80),(2450,'right',80),
                   (2700,'ok',80),(3000,'ok',80),(3350,'back',80),(3650,'down',80),
                   (3900,'ok',80),(4200,'ok',80),(4500,'back',80),(4800,'home',120))
    c.run(1950);c.save_png(str(OUT/(name+'-guide.png')))
    c.run(4400);c.save_png(str(OUT/(name+'-pause.png')))
    c.run(5200);safe(c);saved=records(c.storage_bytes())['towerblock.sav']
    assert saved[24]==1 and saved[61]==0 and saved[64]==1
    assert struct.unpack_from('<I',saved,4)[0]==0
    assert struct.unpack_from('<I',saved,20)[0]==1
    print(name+': three-item menu, resume, restart cancellation and old-record protection passed.',flush=True)


def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--nwlink',default='nwlink')
    a=ap.parse_args()
    (OUT/'tmp').mkdir(exist_ok=True);tempfile.tempdir=str(OUT/'tmp')
    nwa=str(OUT/'towerblock-debug.nwa');report=[]
    sentinel=record('untouched.py',b'print(42)\n')
    for name,model,system,ram in [('n0110','n0110','epsilon',148928),
        ('n0115-custom','n0110','epsilon',133652),('n0120','n0120','epsilon',153676),
        ('upsilon','n0110','upsilon',107674)]:
        emu.configure(model,system,ram)
        menu_and_migration(nwa,a.nwlink,name)
        storage=OUT/(name+'-storage.bin');storage.write_bytes(sentinel+b'\0\0')
        c=emu.Calculator(nwa,storage_file=str(storage),nwlink=a.nwlink);c.enable_checks()
        c.keys=presses((0,'ok',300),(600,'ok',80),(1000,'ok',400),(1450,'back',90),
                       (2600,'ok',80),(2850,'exe',80),(3150,'ok',300),(3500,'home',120))
        c.run(500);c.save_png(str(OUT/(name+'-title.png')))
        c.run(1900);saved=records(c.storage_bytes())['towerblock.sav']
        assert len(saved)==384 and saved[:4]==b'TBK2' and saved[64]==1
        assert struct.unpack_from('<I',saved,28)[0]==0 # Held OK never auto-places.
        frame=bytes(c.screen);c.run(2400)
        assert bytes(c.screen)==frame and records(c.storage_bytes())['towerblock.sav']==saved
        c.run(3450);performance={k:u32(c,k) for k in ('perf_frames','perf_max','perf_slow')}
        c.run(3800);safe(c)
        saved=records(c.storage_bytes())['towerblock.sav']
        assert struct.unpack_from('<I',saved,28)[0]==1
        assert struct.unpack_from('<I',saved,20)[0]==1
        assert records(c.storage_bytes())['untouched.py']==b'print(42)\n'
        stack=c.stack_used()
        after=record('after.py',b'x=123\n');raw=c.storage_bytes()
        end=len(sentinel)+2+len('towerblock.sav')+1+384
        storage.write_bytes(raw[:end]+after+b'\0\0')
        d=emu.Calculator(nwa,storage_file=str(storage),nwlink=a.nwlink);d.enable_checks()
        d.keys=presses((500,'ok',80),(900,'ok',80),(1250,'onoff',120))
        d.run(1500);safe(d);loaded=records(d.storage_bytes())
        assert loaded['after.py']==b'x=123\n' and loaded['untouched.py']==b'print(42)\n'
        assert struct.unpack_from('<I',loaded['towerblock.sav'],28)[0]==1
        assert struct.unpack_from('<I',loaded['towerblock.sav'],20)[0]==1
        report.append(dict(profile=name,stack_bytes=max(stack,d.stack_used()),**performance))
        assert performance['perf_slow']==0,performance
        print(json.dumps(report[-1]),flush=True)
    # Real input through an entire reference flight; no test-only functions in
    # the binary. 24 ms holds allow sampling around the display transfers.
    emu.configure('n0110','epsilon',148928)
    c=emu.Calculator(nwa,nwlink=a.nwlink);c.enable_checks()
    taps=[int(t) for t in (OUT/'build-taps.txt').read_text().splitlines()[:12]]
    c.keys=presses((600,'ok',80),(1000,'ok',80),
                   *((1000+t*16+8,'exe',24) for t in taps))
    end=1000+taps[-1]*16+500
    c.run(end);c.save_png(str(OUT/'arm-gameplay.png'))
    gameplay_perf={k:u32(c,k) for k in ('perf_frames','perf_max','perf_slow')}
    c.keys+=presses((int(c.now_ms)+100,'home',120));c.run(c.now_ms+500);safe(c)
    flight=records(c.storage_bytes())['towerblock.sav']
    floors=struct.unpack_from('<I',flight,28)[0]
    assert floors>=8,(floors,flight[62])
    assert gameplay_perf['perf_slow']==0,gameplay_perf
    print('Keyboard gameplay: '+str(floors)+' floors; '+json.dumps(gameplay_perf),flush=True)
    # Maximum history, falling cut pieces and camera movement on a slow model.
    storage=OUT/'tall-storage.bin'
    storage.write_bytes(record('towerblock.sav',(OUT/'tall-start.bin').read_bytes())+b'\0\0')
    c=emu.Calculator(nwa,storage_file=str(storage),nwlink=a.nwlink);c.enable_checks()
    c.keys=presses((500,'ok',80),(900,'ok',80),(1800,'exe',80),(3500,'home',120))
    c.run(3200);tall_perf={k:u32(c,k) for k in ('perf_frames','perf_max','perf_slow')}
    c.save_png(str(OUT/'arm-tall.png'));c.run(3800);safe(c)
    assert tall_perf['perf_slow']==0,tall_perf
    print('Tall tower: '+json.dumps(tall_perf),flush=True)
    # Studio choices persist independently of a tower.
    storage=OUT/'studio-storage.bin';storage.write_bytes(b'\0\0')
    c=emu.Calculator(nwa,storage_file=str(storage),nwlink=a.nwlink);c.enable_checks()
    c.keys=presses((400,'right',80),(900,'ok',80),(1200,'right',80),
                   (1500,'down',80),(1700,'right',80),(1950,'down',80),(2150,'right',80),(2500,'home',120))
    c.run(2800);safe(c)
    assert records(c.storage_bytes())['towerblock.sav'][24:27]==bytes([1,0,0])
    # Refuse a failed exit cleanly and keep unrelated storage intact.
    storage=OUT/'full-storage.bin';full=record('large.py',b'#'*(emu.STORAGE_SIZE-16))
    storage.write_bytes(full+b'\0\0')
    c=emu.Calculator(nwa,storage_file=str(storage),nwlink=a.nwlink);c.enable_checks()
    c.keys=presses((500,'home',300),(1300,'ok',80),(1700,'back',80))
    c.run(1200);safe(c,False);c.save_png(str(OUT/'arm-storage-full.png'))
    assert c.storage_bytes()[:len(full)]==full
    c.run(2000);safe(c)
    (OUT/'arm-validation.json').write_text(json.dumps(dict(practice_removed=True,legacy_records=True,profiles=report,gameplay=gameplay_perf,
        gameplay_floors=floors,tall=tall_perf),indent=2)+'\n')
    print('ARM input, persistence, rendering, safe exits and full storage passed.')

if __name__=='__main__':
    main()
