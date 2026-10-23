"""Run the actual Cortex-M7 app with Epsilon/Upsilon storage and exit checks."""
import argparse,io,json,shlex,struct,tempfile,types
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];REPO=ROOT.parents[1]
emu_path=REPO/'tools/emu.py';emu=types.ModuleType('uno_emu');emu.__file__=str(emu_path);emu.io=io
source=emu_path.read_text().replace('ELFFile(open(elf, "rb"))','ELFFile(io.BytesIO(open(elf, "rb").read()))')
source=source.replace('cmd = [nwlink, "nwa-elf",','cmd = nwlink + ["nwa-elf",')
source=source.replace('chunk = CPU_HZ // 1000','chunk = CPU_HZ // 10000').replace('self.now_ms += 1\n','self.now_ms += 0.1\n')
source=source.replace('        next_frame = self.now_ms if frame_ms else None','        self.checkpoint = until_ms\n        next_frame = self.now_ms if frame_ms else None')
source=source.replace('            self.slept_ms += r0\n','            self.slept_ms += r0\n            if self.now_ms >= self.checkpoint: uc.emu_stop()\n')
exec(compile(source,str(emu_path),'exec'),emu.__dict__)
def storage(path,items):
    data=bytearray(emu.STORAGE_SIZE);at=0
    for name,value in items:
        r=name.encode()+b'\0'+value;n=2+len(r);struct.pack_into('<H',data,at,n);data[at+2:at+n]=r;at+=n
    assert at+2<=len(data);path.write_bytes(data)
def records(data):
    result={};at=0
    while at+2<=len(data):
        n=struct.unpack_from('<H',data,at)[0]
        if not n:break
        assert n>=3 and at+n<=len(data);name,value=data[at+2:at+n].split(b'\0',1);result[name.decode()]=value;at+=n
    return result
def uint(c,name):return struct.unpack('<I',bytes(c.uc.mem_read(c.symbols[name],4)))[0]
def main():
    ap=argparse.ArgumentParser();ap.add_argument('--nwlink',default='nwlink');ap.add_argument('--quick',action='store_true');a=ap.parse_args()
    nw=['node',a.nwlink]if Path(a.nwlink).is_file()and not a.nwlink.endswith(('.exe','.cmd'))else shlex.split(a.nwlink)
    out=ROOT/'output/arm';out.mkdir(parents=True,exist_ok=True);tempfile.tempdir=str(out);app=str(ROOT/'output/uno-debug.nwa');report=[]
    def key(c,k,d=70):
        t=int(c.now_ms)+10;c.keys.append((t,t+d,emu.KEYS[k]));c.run(t+d+120)
    def check(c):assert not c.violations,sorted(set(c.violations));assert not c.firmware_ram_changes()
    def review(c):
        key(c,'up');assert uint(c,'sel')==2;key(c,'ok');assert uint(c,'setup_page')==1
        key(c,'down');key(c,'ok');key(c,'down');key(c,'ok')
        c.save_png(str(out/'rules-custom.png'))
        key(c,'up');key(c,'up');key(c,'ok') # restore classic before starting
        key(c,'up');assert uint(c,'sel')==6;key(c,'ok');assert uint(c,'setup_page')==2
    def exit_check(c,upsilon=False):
        assert c.exited;check(c);assert c.app_ram_clean();assert c.locks==0 and c.max_locks==(0 if upsilon else 1)
    sentinel=b'print(42)\n';saved_reference=None
    profiles=[('n0120','n0120','epsilon',153676),('n0110','n0110','epsilon',148928),('n0115','n0110','epsilon',133652),('upsilon','n0110','upsilon',107674)]
    for profile,model,system,ram in profiles[:1]if a.quick else profiles:
        emu.configure(model,system,ram);emu.CPU_HZ=216_000_000 if model=='n0120'else 86_400_000
        path=out/(profile+'.storage');storage(path,[('keep.py',sentinel)])
        c=emu.Calculator(app,storage_file=str(path),nwlink=nw);c.enable_checks();c.run(250)
        assert uint(c,'screen')==0 and uint(c,'sel')==1
        c.save_png(str(out/(profile+'-menu.png')))
        key(c,'ok');assert uint(c,'screen')==1
        # Players 4 -> 2 -> 3 -> 4; options change without creating a match.
        key(c,'right');key(c,'right');key(c,'right')
        c.save_png(str(out/(profile+'-setup.png')))
        review(c);c.save_png(str(out/(profile+'-review.png')));key(c,'ok');assert uint(c,'screen')==2
        c.run(c.now_ms+4500);check(c);c.save_png(str(out/(profile+'-table.png')))
        for _ in range(5):key(c,'right')
        key(c,'0');key(c,'1');c.run(c.now_ms+2000)
        key(c,'back');assert uint(c,'screen')==3
        frozen=uint(c,'clock_ms');c.run(c.now_ms+1200);assert uint(c,'clock_ms')==frozen
        c.save_png(str(out/(profile+'-pause.png')))
        key(c,'down');key(c,'ok');assert uint(c,'screen')==10
        key(c,'ok');assert uint(c,'screen')==4
        for _ in range(7):key(c,'right')
        key(c,'back');assert uint(c,'screen')==10
        key(c,'back');assert uint(c,'screen')==3
        key(c,'down');key(c,'ok');assert uint(c,'screen')==0
        saved=records(path.read_bytes())['uno.sav'];assert len(saved)==768 and saved[:5]==b'UNO\x02\x01'
        assert not saved[8] and not saved[16] and not any(saved[671:679]) and not any(saved[681:683])
        saved_reference=saved
        # Launch settings then cancel replacement, without changing the saved table.
        key(c,'down');key(c,'ok');assert uint(c,'screen')==1
        review(c);key(c,'ok');assert uint(c,'screen')==6
        for _ in range(4):key(c,'back')
        assert uint(c,'screen')==0
        key(c,'up');key(c,'ok');assert uint(c,'screen')==2
        key(c,'back');key(c,'home',90);c.run(c.now_ms+150);exit_check(c,system=='upsilon')
        assert records(path.read_bytes())['keep.py']==sentinel
        d=emu.Calculator(app,storage_file=str(path),nwlink=nw);d.enable_checks();d.run(250)
        assert uint(d,'screen')==0 and uint(d,'sel')==0
        key(d,'ok');assert uint(d,'screen')==2
        before_resume=records(path.read_bytes())['uno.sav']
        key(d,'back');key(d,'onoff',90);d.run(d.now_ms+150);exit_check(d,system=='upsilon')
        assert records(path.read_bytes())['uno.sav']==before_resume
        frames=c.svc_counts.get(19,0)//15
        r=dict(profile=profile,static_ram=json.loads((ROOT/'output/sizes.json').read_text())['profiles'][0]['static_ram'],peak_stack_bytes=max(c.stack_used(),d.stack_used()),
               average_render_work_ms=round((c.now_ms-c.slept_ms)/max(1,frames),2),
               gameplay=True,pause_freezes=True,save_resume=True,firmware_memory_preserved=True,clean_exit=True)
        report.append(r);print(json.dumps(r),flush=True)
    emu.configure();emu.CPU_HZ=216_000_000
    # Real key paths for play, manual UNO, legal/illegal challenges, draws,
    # wild colour choice and a completed match. Fixtures are engine-validated.
    def start_fixture(name):
        p=out/(name+'.storage');storage(p,[('keep.py',sentinel),('uno.sav',(ROOT/'output'/(name+'.sav')).read_bytes())])
        c=emu.Calculator(app,storage_file=str(p),nwlink=nw);c.enable_checks();c.run(200);key(c,'ok');assert uint(c,'screen')==2;return c,p
    def finish_fixture(c,p):
        key(c,'back');key(c,'home');c.run(c.now_ms+150);exit_check(c);r=records(p.read_bytes());assert r['keep.py']==sentinel;return r['uno.sav']
    c,p=start_fixture('play');key(c,'0');key(c,'ok');saved=finish_fixture(c,p)
    assert saved[449]==1 and saved[701]==255 # human called UNO and played one card
    c,p=start_fixture('draw');key(c,'1');key(c,'left');key(c,'ok');saved=finish_fixture(c,p)
    assert saved[449]==2 and saved[692]==3 # cannot play an old hand card after drawing
    c,p=start_fixture('draw');key(c,'1');key(c,'2');saved=finish_fixture(c,p)
    assert saved[449]==2 and saved[692]==0 and saved[690]==1
    c,p=start_fixture('wild');key(c,'ok');key(c,'right');key(c,'ok');saved=finish_fixture(c,p)
    assert saved[449]==1 and saved[691]==1 and saved[690]==1
    for fixture,guilty in [('challenge-guilty',True),('challenge-legal',False)]:
        c,p=start_fixture(fixture);key(c,'down');key(c,'ok');assert uint(c,'screen')==9
        c.save_png(str(out/(fixture+'.png')));key(c,'ok');saved=finish_fixture(c,p)
        assert saved[449]==(1 if guilty else 7) and saved[450]==(5 if guilty else 1)
        assert saved[690]==(0 if guilty else 1) and saved[693]==0
    c,p=start_fixture('win');key(c,'ok');c.save_png(str(out/'win.png'));saved=finish_fixture(c,p)
    assert saved[692]==5 and saved[700]==0 and not any(saved[671:679]) and struct.unpack_from('<HH',saved,9)==(1,1)
    # Real v1 save with accumulated points: preserve the table and discard totals.
    legacy=(ROOT/'tests/legacy-save.bin').read_bytes();p=out/'legacy.storage';storage(p,[('keep.py',sentinel),('uno.sav',legacy)])
    c=emu.Calculator(app,storage_file=str(p),nwlink=nw);c.enable_checks();c.run(250);key(c,'ok');assert uint(c,'screen')==2
    migrated=finish_fixture(c,p);assert migrated[3]==2 and migrated[17:671]==legacy[17:671]
    assert not migrated[8] and not migrated[16] and not any(migrated[671:679]) and not any(migrated[681:683])
    # From a finished game, OK starts a fresh single game; no extra round target.
    c,p=start_fixture('win');key(c,'ok');key(c,'ok');assert uint(c,'screen')==2
    fresh=finish_fixture(c,p);assert fresh[692]<4 and all(n>=7 for n in fresh[449:451])
    # Full storage: retry fails without damaging the existing record, exit explicit.
    p=out/'full.storage';filler=b'x'*(emu.STORAGE_SIZE-24);storage(p,[('full.py',filler)])
    c=emu.Calculator(app,storage_file=str(p),nwlink=nw);c.enable_checks();c.run(250);key(c,'home');assert uint(c,'screen')==8
    key(c,'ok');assert uint(c,'screen')==8;key(c,'down');key(c,'ok');c.run(c.now_ms+150);exit_check(c)
    assert records(p.read_bytes())=={'full.py':filler}
    # Invalid checksum is ignored and never interpreted as a match.
    bad=bytearray(saved_reference);bad[300]^=1;p=out/'bad.storage';storage(p,[('keep.py',sentinel),('uno.sav',bad)])
    c=emu.Calculator(app,storage_file=str(p),nwlink=nw);c.enable_checks();c.run(250);assert uint(c,'sel')==1
    key(c,'home');c.run(c.now_ms+150);exit_check(c);assert records(p.read_bytes())['keep.py']==sentinel
    # A launch key held by NumPlay cannot accidentally enter New game.
    c=emu.Calculator(app,nwlink=nw);c.enable_checks();c.keys=[(0,400,emu.KEYS['ok'])];c.run(550);assert uint(c,'screen')==0
    key(c,'home');c.run(c.now_ms+150);exit_check(c)
    report.append(dict(full_storage=True,corrupt_save_ignored=True,held_launch_key=True,real_keys_play_and_uno=True,real_keys_challenges=True,real_keys_draw_keep_and_wild=True,real_keys_win=True,legacy_points_removed=True,real_keys_play_again=True))
    (out/'report.json').write_text(json.dumps(report,indent=2)+'\n');print('ARM tests passed',flush=True)
if __name__=='__main__':main()
