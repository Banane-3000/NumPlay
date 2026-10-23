"""Execute the actual ARM app, including firmware memory and clean-exit checks."""
import argparse,io,json,os,re,shlex,struct,tempfile,types
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];REPO=ROOT.parents[1]
emu_path=REPO/'tools/emu.py';emu=types.ModuleType('twoplayer_emu');emu.__file__=str(emu_path);emu.io=io
source=emu_path.read_text().replace('ELFFile(open(elf, "rb"))','ELFFile(io.BytesIO(open(elf, "rb").read()))')
source=source.replace('cmd = [nwlink, "nwa-elf",','cmd = nwlink + ["nwa-elf",')
source=source.replace('chunk = CPU_HZ // 1000','chunk = CPU_HZ // 10000').replace('self.now_ms += 1\n','self.now_ms += 0.1\n')
source=source.replace('        next_frame = self.now_ms if frame_ms else None','        self.checkpoint = until_ms\n        next_frame = self.now_ms if frame_ms else None')
source=source.replace('            self.slept_ms += r0\n','            self.slept_ms += r0\n            if self.now_ms >= self.checkpoint: uc.emu_stop()\n')
exec(compile(source,str(emu_path),'exec'),emu.__dict__)
def presses(*items):return[(t,t+d,emu.KEYS[k])for t,k,d in items]
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
def exited(c,upsilon=False):
    assert c.exited,'App did not exit';assert not c.violations,sorted(set(c.violations));assert not c.firmware_ram_changes();assert c.app_ram_clean();assert c.locks==0 and c.max_locks==(0 if upsilon else 1)

def main():
    ap=argparse.ArgumentParser();ap.add_argument('--nwlink',default='nwlink');ap.add_argument('--quick',action='store_true');a=ap.parse_args()
    nw=['node',a.nwlink]if Path(a.nwlink).is_file()and not a.nwlink.endswith(('.exe','.cmd'))else shlex.split(a.nwlink)
    out=ROOT/'output/arm';out.mkdir(parents=True,exist_ok=True);tempfile.tempdir=str(out);app=str(ROOT/'output/twoplayer-debug.nwa');report=[]
    legacy=(ROOT/'output/legacy.sav').read_bytes();sentinel=b'print(42)\n'
    profiles=[('n0120','n0120','epsilon',153676),('n0110','n0110','epsilon',148928),('n0115','n0110','epsilon',133652),('upsilon','n0110','upsilon',107674)]
    def key(c,k,d=45):
        t=int(c.now_ms)+10;c.keys+=presses((t,k,d));c.run(t+d+55)
    def check(c):
        assert not c.violations,sorted(set(c.violations));assert not c.firmware_ram_changes()
    for profile,model,system,ram in profiles[:1]if a.quick else profiles:
        emu.configure(model,system,ram);emu.CPU_HZ=216_000_000 if model=='n0120'else 86_400_000
        st=out/(profile+'.bin');storage(st,[('keep.py',sentinel),('twoplayer.sav',legacy)])
        c=emu.Calculator(app,storage_file=str(st),nwlink=nw);c.enable_checks();c.run(160)
        assert uint(c,'screen')==0 and uint(c,'game')==3 and uint(c,'cpu')==2
        # Select two-player mode, return to game 0, and navigate every page through real keys.
        key(c,'left');key(c,'left')
        for _ in range(3):key(c,'up')
        assert uint(c,'cpu')==0 and uint(c,'game')==0
        c.save_png(str(out/(profile+'-menu.png')))
        for g in range(15):
            assert uint(c,'screen')==0 and uint(c,'game')==g,(profile,g)
            key(c,'ok');assert uint(c,'screen')==1
            key(c,'ok');assert uint(c,'screen')==2
            clock=uint(c,'now');c.run(c.now_ms+130);assert uint(c,'now')>clock
            if g==8:
                # New action keys really charge independently; Shift no longer acts.
                key(c,'shift')
                flags=c.symbols['party']+336
                assert bytes(c.uc.mem_read(flags,2))==b'\x00\x00'
                t=int(c.now_ms)+20;c.keys+=presses((t,'ok',650),(t,'exe',550))
                c.run(t+400);assert bytes(c.uc.mem_read(flags,2))==b'\x01\x01'
                c.save_png(str(out/(profile+'-archery-charge.png')))
                c.run(t+740);assert bytes(c.uc.mem_read(flags,2))==b'\x00\x00'
                assert any(struct.unpack('<h',bytes(c.uc.mem_read(c.symbols['party']+56+i*20+16,2)))[0]>0 for i in range(12))
                c.save_png(str(out/(profile+'-archery-shot.png')))
            # Both physical key sets are sampled together; pause stops only game time.
            t=int(c.now_ms)+10
            c.keys+=presses((t,'left',70),(t,'ok',60),(t,'minus',70),(t,'exe',60));c.run(t+100)
            key(c,'back');assert uint(c,'screen')==3
            frozen=uint(c,'now');c.run(c.now_ms+120);assert uint(c,'now')==frozen
            key(c,'back');assert uint(c,'screen')==2
            if g>=4:c.save_png(str(out/(profile+'-game-%02d.png'%g)))
            key(c,'back');key(c,'down');key(c,'down');key(c,'ok');assert uint(c,'screen')==0
            check(c)
            if g<14:key(c,'down')
        # Page jump wraps and retains the row; CPU mode remains independent.
        key(c,'9');assert uint(c,'game')==2
        key(c,'7');assert uint(c,'game')==14
        key(c,'right');assert uint(c,'cpu')==1
        key(c,'right');key(c,'right');assert uint(c,'cpu')==3
        key(c,'ok');key(c,'ok');key(c,'back');key(c,'down');key(c,'ok');assert uint(c,'screen')==2 and uint(c,'phase')==0
        # Real keys score for each human in Quick Draw; holding action does not
        # leak an extra shot into the following round.
        # 14 -> 2 -> 3 -> 4, then CPU hard -> two players.
        key(c,'back');key(c,'down');key(c,'down');key(c,'ok')
        key(c,'9');key(c,'down');key(c,'down');key(c,'right')
        assert uint(c,'game')==4 and uint(c,'cpu')==0
        key(c,'ok');key(c,'ok')
        for player,k in enumerate(['ok','exe']):
            if player:c.run(c.now_ms+1650)
            assert uint(c,'phase')==0
            # signal is a plain uint32 at the offset verified by the host.
            signal=struct.unpack('<I',bytes(c.uc.mem_read(c.symbols['party']+328,4)))[0]
            c.run(c.now_ms+max(0,signal-uint(c,'now'))+40)
            key(c,k)
            assert uint(c,'phase')==1 and uint(c,'winner')==player
            assert struct.unpack('<ii',bytes(c.uc.mem_read(c.symbols['score'],8)))==((1,0)if player==0 else(1,1))
        r=dict(profile=profile,games_opened=15,pause_freezes=True,both_human_keys_score=True,stack_bytes=c.stack_used())
        key(c,'home',90);c.run(c.now_ms+160);exited(c,system=='upsilon')
        recs=records(st.read_bytes());assert recs['keep.py']==sentinel
        saved=recs['twoplayer.sav'];assert len(saved)==128 and saved[:4]==b'P\x03\x04\x00'
        # All 16 legacy counters survive the expansion; none were won during these short tests.
        assert saved[4:36]==legacy[4:36] and not any(saved[36:124])
        d=emu.Calculator(app,storage_file=str(st),nwlink=nw);d.enable_checks();d.run(140);assert uint(d,'game')==4 and uint(d,'cpu')==0
        key(d,'onoff',90);d.run(d.now_ms+150);exited(d,system=='upsilon');assert records(st.read_bytes())['twoplayer.sav']==saved
        report.append(r);print(json.dumps(r),flush=True)
    emu.configure()
    import zlib
    for old_game,expected in [(15,14),(14,0)]:
        previous=bytearray((ROOT/'output/v2.sav').read_bytes());previous[2]=old_game
        struct.pack_into('<I',previous,132,zlib.crc32(previous[:132]))
        st=out/('migration-%d.bin'%old_game);storage(st,[('keep.py',sentinel),('twoplayer.sav',previous)])
        c=emu.Calculator(app,storage_file=str(st),nwlink=nw);c.enable_checks();c.run(160)
        assert uint(c,'game')==expected
        key(c,'home',90);c.run(c.now_ms+160);exited(c)
        migrated=records(st.read_bytes())['twoplayer.sav']
        assert len(migrated)==128 and migrated[:3]==bytes([80,3,expected])
        assert migrated[4:116]==previous[4:116] and migrated[116:124]==previous[124:132]
        assert records(st.read_bytes())['keep.py']==sentinel
    # No accidental actions when launching with OK held; a full file system stays intact.
    emu.configure();c=emu.Calculator(app,nwlink=nw);c.enable_checks();c.keys=presses((0,'ok',350));c.run(450);assert uint(c,'screen')==0
    key(c,'home',90);c.run(c.now_ms+150);exited(c)
    st=out/'full.bin';filler=b'x'*(emu.STORAGE_SIZE-24);storage(st,[('full.py',filler)])
    c=emu.Calculator(app,storage_file=str(st),nwlink=nw);c.enable_checks();c.run(150);key(c,'home');assert uint(c,'screen')==6
    key(c,'ok');assert uint(c,'screen')==6
    key(c,'down');key(c,'ok');c.run(c.now_ms+150);exited(c);assert records(st.read_bytes())=={'full.py':filler}
    report.append(dict(legacy_migration=True,full_storage_retry_and_unsaved_exit=True,held_launch_key=True))
    (out/'report.json').write_text(json.dumps(report,indent=2)+'\n');print('ARM tests passed',flush=True)
if __name__=='__main__':main()
