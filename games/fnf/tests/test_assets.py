"""Verify all imported chart data, lossless note streams and bounded artwork."""
from pathlib import Path
import json,zlib,re,hashlib,collections
ROOT=Path(__file__).resolve().parents[1]
def main():
    model=json.loads(zlib.decompress((ROOT/'assets/model.json.zlib').read_bytes()));art=json.loads((ROOT/'assets/art.json').read_text());manifest=json.loads((ROOT/'assets/chart-manifest.json').read_text());c=(ROOT/'src/data.c').read_text()
    data=bytes(map(int,re.search(r'const uint8_t note_bytes\[\d+\]=\{(.*?)\};',c,re.S)[1].replace('\n','').split(',')))
    rows=re.search(r'const Chart charts\[\d+\]=\{(.*?)\n\};',c,re.S)[1].strip().splitlines();rows=[r.strip('{},').split(',')for r in rows]
    def var(at,end):
        v=0;shift=0
        while True:
            assert at<end and shift<28;b=data[at];at+=1;v|=(b&127)<<shift;shift+=7
            if not b&128:return v,at
    total=0;maxerror=0;peak=0;key_schedules=[]
    for entry,row in zip(manifest['charts'],rows,strict=True):
        track=next(t for t in model['tracks'] if t['id']==entry['song']and t['variation']==entry['variation']);original=sorted(track['chart']['notes'][entry['difficulty']],key=lambda n:n['t'])
        at=int(row[0]);end=at+int(row[1]);assert hashlib.sha256(data[at:end]).hexdigest()==entry['sha256'];time=0;intervals=[]
        schedule=[]
        for n in original:
            dt,at=var(at,end);info,at=var(at,end);length=0
            if info&1:length,at=var(at,end)
            time+=dt;assert time==round(n['t']*16);assert length==round(n.get('l',0)*16);assert(info>>1)&7==n['d'];assert model['kinds'][info>>4]==n.get('k','');maxerror=max(maxerror,abs(time/16-n['t']),abs(length/16-n.get('l',0)))
            intervals.extend([(time-40000,1),(time+length+2561,-1)])
            if n['d']<4:schedule.append(dict(time=time,end=time+length,lane=n['d']))
        assert at==end and len(original)==entry['notes'];total+=len(original)
        # Even retaining every missed hold until its end must fit, without a life-bar cutoff.
        concurrent=0
        for _,change in sorted(intervals):concurrent+=change;peak=max(peak,concurrent+1)
        if entry['variation']=='default'and entry['difficulty']=='normal'and entry['song']in ['tutorial','bopeebo']:key_schedules.append(dict(chart=entry['chart'],song=entry['song'],end=int(row[2]),notes=schedule))
    capacity=int(re.search(r'#define NOTE_CAP (\d+)',(ROOT/'src/game.h').read_text())[1])
    assert len(rows)==len(manifest['charts'])==166 and total==90437 and peak<capacity and maxerror<=.03125001
    blob=(ROOT/'assets/art.bin').read_bytes()
    for sprite in art['sprites']:
        off=sprite['offset'];w=sprite['w'];h=sprite['h'];assert sprite['palette']<len(art['palettes'])
        if sprite['compressed']:
            assert(w,h)==(160,120);raw=zlib.decompress(blob[off:off+sprite['packed_size']],-15);assert len(raw)==9600
        else:
            start=off+h*2;end=start+sprite['packed_size'];assert end<=len(blob)
            for y in range(h):
                at=start+int.from_bytes(blob[off+y*2:off+y*2+2],'little');x=0
                while x<w:
                    assert at<end;token=blob[at];at+=1;length=token>>1;assert length and x+length<=w
                    if token&1:at+=(length+1)//2
                    x+=length
                assert at<=end
    ids=range(len(art['sprites']))
    for ch in model['characters']:
        for frames in ch['anims'].values():assert 1<=len(frames)<=4 and all(i in ids for i in frames)
    for stage in model['stages']:assert not stage['missing'] and stage['sprite']in ids
    out=ROOT/'output';out.mkdir(exist_ok=True);(out/'key-schedules.json').write_text(json.dumps(key_schedules)+'\n')
    inputs=['model.json.zlib','art.json','art.bin'];report=dict(charts=166,songs=27,notes=total,max_time_quantization_ms=maxerror,worst_case_queue_bound=peak,sprites=len(art['sprites']),input_sha256={p:hashlib.sha256((ROOT/'assets'/p).read_bytes()).hexdigest()for p in inputs})
    (out/'asset-report.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))
if __name__=='__main__':main()
