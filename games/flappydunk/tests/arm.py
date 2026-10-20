#!/usr/bin/env python3
"""Run the actual Cortex-M7 application on NumPlay's four emulator profiles.

python tests/arm.py --nwlink /path/to/nwlink
Requires pyelftools, Unicorn and Pillow. Run the build, host test and
playability test first (they create the reference input and save fixture).
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
emu = types.ModuleType('flappydunk_emu')
emu.__file__ = str(emu_path)
emu.io = io
source = emu_path.read_text().replace('ELFFile(open(elf, "rb"))',
     'ELFFile(io.BytesIO(open(elf, "rb").read()))')
# Short CPU slices keep checkpoints within a few milliseconds even on a static
# menu where thousands of sleep calls can otherwise fit in a single slice.
source = source.replace('chunk = CPU_HZ // 1000', 'chunk = CPU_HZ // 10000')
source = source.replace('self.now_ms += 1\n', 'self.now_ms += 0.1\n')
# Stop after a completed sleep at each requested checkpoint.
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

def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--nwlink', default='nwlink')
    a = ap.parse_args()
    (OUT/'tmp').mkdir(exist_ok=True)
    tempfile.tempdir = str(OUT/'tmp')
    nwa = str(OUT/'flappydunk-debug.nwa')
    report = []
    for name,model,system,ram in [('n0110','n0110','epsilon',148928),
        ('n0115-custom','n0110','epsilon',133652), ('n0120','n0120','epsilon',153676),
        ('upsilon','n0110','upsilon',107674)]:
        emu.configure(model,system,ram)
        storage = OUT/(name+'-storage.bin')
        sentinel = record('untouched.py',b'print(42)\n')
        storage.write_bytes(sentinel+b'\0\0')
        c = emu.Calculator(nwa,storage_file=str(storage),nwlink=a.nwlink)
        c.enable_checks()
        # Held launcher OK is ignored. A new Up press makes exactly one flap.
        c.keys = presses((0,'ok',300),(600,'ok',70),(1000,'up',400),(1450,'back',80),
                         (2600,'ok',70),(2850,'exe',70),(3200,'home',120))
        c.run(550);c.save_png(str(OUT/(name+'-title.png')))
        c.run(1900)
        saved = records(c.storage_bytes())['flappydunk.sav']
        assert len(saved)==160 and saved[:4]==b'FDK2'
        assert struct.unpack_from('<I',saved,44)[0]==1
        assert saved[71]==1 and saved[74]==1
        frame = bytes(c.screen)
        c.run(2400)
        assert bytes(c.screen)==frame
        assert records(c.storage_bytes())['flappydunk.sav']==saved
        c.run(3120)
        performance = {k:u32(c,k) for k in ('perf_frames','perf_max','perf_slow')}
        c.run(3500);safe(c)
        saved = records(c.storage_bytes())['flappydunk.sav']
        assert struct.unpack_from('<I',saved,44)[0]==2
        assert records(c.storage_bytes())['untouched.py']==b'print(42)\n'
        stack = c.stack_used()
        # A record after our save survives replacement and a second launch.
        after = record('after.py',b'x=123\n')
        raw = c.storage_bytes()
        end = len(sentinel)+2+len('flappydunk.sav')+1+160
        storage.write_bytes(raw[:end]+after+b'\0\0')
        d = emu.Calculator(nwa,storage_file=str(storage),nwlink=a.nwlink)
        d.enable_checks()
        d.keys = presses((500,'ok',80),(1000,'ok',80),(1250,'back',80),(1550,'onoff',120))
        d.run(1800);safe(d)
        loaded = records(d.storage_bytes())
        assert loaded['after.py']==b'x=123\n' and loaded['untouched.py']==b'print(42)\n'
        assert struct.unpack_from('<I',loaded['flappydunk.sav'],44)[0]==3
        report.append(dict(profile=name,stack_bytes=max(stack,d.stack_used()),**performance))
        print(json.dumps(report[-1]),flush=True)
    # Replay ordinary input on the slow model: three actual baskets, swish
    # particles and multipliers. The reference planner is an offline test only.
    emu.configure('n0110','epsilon',148928)
    storage = OUT/'flight-arm.bin'
    storage.write_bytes(record('flappydunk.sav',(OUT/'flight-start.bin').read_bytes())+b'\0\0')
    c = emu.Calculator(nwa,storage_file=str(storage),nwlink=a.nwlink)
    c.enable_checks()
    taps = [int(t) for t in (OUT/'flight-taps.txt').read_text().splitlines() if int(t)<420]
    c.keys = presses((600,'ok',80),
                    *((1000+t*16+(12 if t else 0),'up',24) for t in taps),
                    (7200,'home',120))
    c.run(3000);c.save_png(str(OUT/'arm-swish.png'))
    c.run(7000)
    flight_perf = {k:u32(c,k) for k in ('perf_frames','perf_max','perf_slow')}
    assert flight_perf['perf_slow']==0, flight_perf
    c.run(7500);safe(c)
    flight = records(c.storage_bytes())['flappydunk.sav']
    assert struct.unpack_from('<I',flight,36)[0]>=3
    assert struct.unpack_from('<I',flight,40)[0]>=1
    print('ARM gameplay: three baskets and swish effects; '+json.dumps(flight_perf),flush=True)
    # Migrate both old endless and discontinued-mode saves, preserving all
    # previously earned balls without retaining medal counters or mode data.
    for filename,phase in [('legacy-save.bin',1),('legacy-challenge-save.bin',0)]:
        old=(ROOT/'tests'/filename).read_bytes();storage=OUT/('arm-'+filename)
        storage.write_bytes(record('flappydunk.sav',old)+b'\0\0')
        c=emu.Calculator(nwa,storage_file=str(storage),nwlink=a.nwlink);c.enable_checks()
        c.keys=presses((500,'home',100));c.run(1000);safe(c)
        saved=records(c.storage_bytes())['flappydunk.sav']
        assert saved[:4]==b'FDK2' and saved[4:28]==old[4:28]
        assert saved[28:30]==b'\0\0' and saved[30:32]==b'\x05\x3f' and saved[70]==0 and saved[71]==phase
    # Three-item title navigation, locked Cosmic ball at the new 200-point
    # requirement, guide access and wrapping back to the Play entry.
    emu.configure('n0110','epsilon',148928)
    storage=OUT/'menus-storage.bin';storage.write_bytes(b'\0\0')
    c=emu.Calculator(nwa,storage_file=str(storage),nwlink=a.nwlink);c.enable_checks()
    c.keys=presses((500,'down',70),(800,'ok',70),(1300,'up',70),(1600,'right',70),(1900,'right',70),
                   (2500,'ok',70),(2800,'back',70),(3100,'down',70),(3400,'ok',70),
                   (4000,'back',70),(4300,'up',70),(4600,'down',70),(4900,'ok',70),
                   (5200,'up',70),(5450,'back',70),(5750,'down',70),(6050,'ok',70),
                   (6350,'down',70),(6650,'ok',70),(6950,'back',70),(7250,'home',100))
    c.run(2200);c.save_png(str(OUT/'arm-locker.png'))
    c.run(3700);c.save_png(str(OUT/'arm-guide.png'))
    c.run(6800);c.save_png(str(OUT/'arm-new-flight.png'))
    c.run(7700);safe(c)
    saved=records(c.storage_bytes())['flappydunk.sav']
    assert saved[30]==0 and saved[31]==1 and saved[71]==0 and saved[74]==0
    # Full storage: failed exit remains interactive, and Back explicitly exits.
    emu.configure('n0120','epsilon',153676)
    storage = OUT/'full-storage.bin'
    full = record('large.py',b'#'*(emu.STORAGE_SIZE-16))
    storage.write_bytes(full+b'\0\0')
    c = emu.Calculator(nwa,storage_file=str(storage),nwlink=a.nwlink)
    c.enable_checks();c.keys=presses((500,'home',300))
    c.run(1200);safe(c,False);c.save_png(str(OUT/'arm-storage-full.png'))
    assert c.storage_bytes()[:len(full)]==full
    c.keys+=presses((1300,'ok',80),(1600,'back',80))
    c.run(1900);safe(c)
    (OUT/'arm-validation.json').write_text(json.dumps(dict(profiles=report,flight=flight_perf),indent=2)+'\n')
    print('All ARM profiles: pause, fresh input, persistence, safe exits and full storage passed. Menu navigation, score-only locker and legacy migration passed.')

if __name__ == '__main__':
    main()
