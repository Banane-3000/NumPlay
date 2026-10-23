"""Execute the actual ARM app, including firmware memory and clean-exit checks."""
import argparse,io,json,os,re,shlex,struct,tempfile,types
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];REPO=ROOT.parents[1]
emu_path=REPO/'tools/emu.py';emu=types.ModuleType('fnf_emu');emu.__file__=str(emu_path);emu.io=io
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
    ap=argparse.ArgumentParser();ap.add_argument('--nwlink',default='nwlink');ap.add_argument('--quick',action='store_true');ap.add_argument('--smoke',action='store_true',help='First 16 notes per profile instead of a full N0120 song');a=ap.parse_args()
    nw=['node',a.nwlink]if Path(a.nwlink).is_file()and not a.nwlink.endswith(('.exe','.cmd'))else shlex.split(a.nwlink)
    out=ROOT/'output/arm';out.mkdir(parents=True,exist_ok=True);tempfile.tempdir=str(out);app=str(ROOT/'output/fnf-debug.nwa');report=[];sentinel=b'print(42)\n'
    offsets=json.loads((ROOT/'output/host-report.json').read_text());schedules=json.loads((ROOT/'output/key-schedules.json').read_text());schedule=next(s for s in schedules if s['song']=='bopeebo')
    def field(c,key,fmt):return struct.unpack(fmt,bytes(c.uc.mem_read(c.symbols['game']+offsets[key],struct.calcsize(fmt))))[0]
    profiles=[('n0120','n0120','epsilon',153676),('n0110','n0110','epsilon',148928),('n0115','n0110','epsilon',133652),('upsilon','n0110','upsilon',107674)]
    for profile,model,system,ram in profiles[:1]if a.quick else profiles:
        emu.configure(model,system,ram);emu.CPU_HZ=216_000_000 if model=='n0120'else 86_400_000;st=out/(profile+'.bin');storage(st,[('keep.py',sentinel)])
        c=emu.Calculator(app,storage_file=str(st),nwlink=nw);c.enable_checks()
        # Title -> Freeplay -> Bopeebo (Normal), entirely through the keypad.
        c.keys=presses((400,'down',65),(650,'ok',65),(950,'down',65),(1250,'ok',65));c.run(300);c.save_png(str(out/(profile+'-title.png')));c.run(1150);assert uint(c,'current_view')==2;c.save_png(str(out/(profile+'-freeplay.png')));c.run(1500);assert uint(c,'current_view')==5;assert field(c,'chart','<H')==schedule['chart']
        origin=uint(c,'origin')/16;lane_keys=['left','down','up','right'] if profile in ['n0120','n0115']else ['7','8','lparen','rparen']
        full_song=profile=='n0120'and not a.smoke
        chosen=schedule['notes'] if full_song else schedule['notes'][:16]
        # Keep exact original head and tail times; avoid release collisions with the next head.
        for i,n in enumerate(chosen):
            start=origin+n['time']/16;end=max(start+16,origin+n['end']/16+1)
            for nxt in chosen[i+1:]:
                if nxt['lane']==n['lane']:
                    end=min(end,origin+nxt['time']/16-14);break
            c.keys.append((start,end,emu.KEYS[lane_keys[n['lane']]]))
        stop=origin+schedule['end']/16+250 if full_song else origin+chosen[-1]['time']/16+40
        # A held arrow plus its number alias is still exactly one input.
        n=chosen[0];c.keys.append((origin+n['time']/16+3,origin+n['time']/16+10,emu.KEYS['7'if n['lane']==0 else '8'if n['lane']==1 else 'lparen'if n['lane']==2 else 'rparen']))
        while c.now_ms<stop:
            c.run(min(stop,c.now_ms+5000))
        assert not field(c,'lost','<B'),(profile,'unexpected failure');assert field(c,'judged','<H')>=len(chosen)-1,(profile,field(c,'judged','<H'),len(chosen));c.save_png(str(out/(profile+'-play.png')))
        sick=field(c,'judged','<H')
        if full_song:assert field(c,'won','<B')and uint(c,'current_view')==7
        else:
            t=int(c.now_ms)+20;c.keys+=presses((t,'back',65));c.run(t+180);assert uint(c,'current_view')==6;frozen=field(c,'time','<i');c.run(t+680);assert field(c,'time','<i')==frozen
            c.keys+=presses((t+750,'back',65));c.run(t+950);assert uint(c,'current_view')==5
            c.keys+=presses((t+1000,'back',65),(t+1200,'down',65),(t+1400,'ok',65));c.run(t+1650);assert uint(c,'current_view')==5 and field(c,'time','<i')<0
        r=dict(profile=profile,real_key_song_clear=full_song,keys=lane_keys,frames=uint(c,'perf_frames'),draw_max_ms=uint(c,'perf_max'),play_draw_max_ms=uint(c,'perf_play_draw_max'),input_sample_max_ms=uint(c,'perf_input_max'),play_input_max_ms=uint(c,'perf_play_input_max'),queue_peak=uint(c,'perf_queue_max'),stack_bytes=c.stack_used(),sick=sick)
        t=int(c.now_ms)+40;c.keys+=presses((t,'home',150));c.run(t+900);exited(c,system=='upsilon');recs=records(st.read_bytes());assert recs['keep.py']==sentinel and len(recs['fnf.sav'])==offsets['save_bytes'] and b'FNF1:'in recs['fnf_save.py']
        # Reinstallation can erase .sav; restore from the inert Python comment backup.
        restored=out/(profile+'-backup.bin');storage(restored,[('keep.py',sentinel),('fnf_save.py',recs['fnf_save.py'])]);d=emu.Calculator(app,storage_file=str(restored),nwlink=nw);d.enable_checks();d.keys=presses((400,'onoff',100));d.run(1100);exited(d,system=='upsilon');assert records(restored.read_bytes())['fnf.sav']==recs['fnf.sav'];report.append(r);print(json.dumps(r),flush=True)
    if not a.quick:
        # Explicit host-engine fixtures exercise the largest on-screen cast and
        # densest working queue. These are rendering/performance tests, not clears.
        for profile,model,ram in [('n0120','n0120',153676),('n0110','n0110',148928)]:
            emu.configure(model,'epsilon',ram);emu.CPU_HZ=216_000_000 if model=='n0120'else 86_400_000
            for name in ['dense-chart','crowded-scene']:
                c=emu.Calculator(app,nwlink=nw);c.enable_checks();c.keys=presses((300,'down',65),(550,'ok',65),(850,'down',65),(1150,'ok',65));c.run(1450);assert uint(c,'current_view')==5
                raw=(ROOT/'output'/(name+'.game')).read_bytes();assert len(raw)==offsets['game_size'];c.uc.mem_write(c.symbols['game'],raw);clock=field(c,'time','<i');c.uc.mem_write(c.symbols['origin'],struct.pack('<I',(int(c.now_ms)*16-clock)&0xffffffff))
                for metric in ['perf_play_draw_max','perf_play_input_max']:c.uc.mem_write(c.symbols[metric],bytes(4))
                c.run(c.now_ms+140);assert uint(c,'current_view')==5;c.save_png(str(out/(profile+'-'+name+'.png')))
                report.append(dict(profile=profile,fixture=name,play_draw_max_ms=uint(c,'perf_play_draw_max'),play_input_max_ms=uint(c,'perf_play_input_max'),stack_bytes=c.stack_used()));t=int(c.now_ms)+20;c.keys+=presses((t,'home',120));c.run(t+800);exited(c)
        emu.configure();st=out/'full.bin';filler=b'x'*(emu.STORAGE_SIZE-24);storage(st,[('full.py',filler)]);c=emu.Calculator(app,storage_file=str(st),nwlink=nw);c.enable_checks();c.keys=presses((400,'home',65),(800,'ok',65),(1200,'1',65),(1600,'home',65),(2000,'back',65));c.run(700);assert uint(c,'current_view')==9;c.run(1000);assert uint(c,'current_view')==9;c.run(1400);assert uint(c,'current_view')==0;c.run(2600);exited(c);assert records(st.read_bytes())['full.py']==filler
        c=emu.Calculator(app,nwlink=nw);c.enable_checks();c.keys=presses((0,'ok',900),(1300,'home',120));c.run(500);assert uint(c,'current_view')==0;c.run(2000);exited(c)
        report.append(dict(full_storage_retry_menu_unsaved_exit='pass',launch_held_key='pass'))
    (out/('smoke-report.json'if a.smoke else'report.json')).write_text(json.dumps(report,indent=2)+'\n');print('ARM tests passed',flush=True)
if __name__=='__main__':main()
