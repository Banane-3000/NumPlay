#!/usr/bin/env python3
"""Check the port against the original game physics, using its actual JS functions."""
import argparse
import ctypes as C
import json
import os
from pathlib import Path
import subprocess
ROOT=Path(__file__).resolve().parents[1]
FLOATS='x y vx vy theta omega r pull rotation spin hook_x hook_y finish spawn_x spawn_y progress camera_offset camera_x complete_time'.split()
UINTS='level seed attempt ticks bumper_hits wall_hits'.split()
BYTES='section_count hooked held completed ready background pose facing trail_count trail_cursor'.split()
class Vec(C.Structure):_fields_=[('x',C.c_float),('y',C.c_float)]
class Section(C.Structure):_fields_=[('part',C.c_uint8),('x',C.c_float)]
class Game(C.Structure):_fields_=[*( (x,C.c_float) for x in FLOATS),*((x,C.c_uint32) for x in UINTS),('sections',Section*3),('trail',Vec*20),*((x,C.c_uint8) for x in BYTES)]
def state(g):return {**{k:getattr(g,k) for k in FLOATS+UINTS+BYTES},'sections':[dict(part=s.part,x=s.x) for s in g.sections[:g.section_count]]}
def main():
    ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('--cc',default=os.environ.get('HOST_CC','cc'));a=ap.parse_args()
    out=ROOT/'output';out.mkdir(exist_ok=True);lib=out/('physics.dll' if os.name=='nt' else 'physics.so')
    cmd=[a.cc,'-std=c11','-O2','-Wall','-Wextra','-Werror','-shared']
    if os.name!='nt':cmd+=['-fPIC']
    subprocess.run(cmd+['src/game.c','src/levels.c','-lm','-o',str(lib)],cwd=ROOT,check=True)
    dll=C.CDLL(str(lib));dll.sh_new.argtypes=[C.POINTER(Game),C.c_uint32,C.c_uint32];dll.sh_step.argtypes=[C.POINTER(Game),C.c_bool]
    assert C.sizeof(Game)==296
    cases=[]
    for level in range(50):
        g=Game();dll.sh_new(C.byref(g),level,1234);replay=(out/f'replay-{level:02}.bin').read_bytes()
        for action in replay:
            for _ in range(6):
                before=state(g);dll.sh_step(C.byref(g),bool(action));after=state(g)
                if before['attempt']==after['attempt']:cases.append(dict(before=before,after=after,held=bool(action)))
                if g.completed:break
            if g.completed:break
        assert g.completed,level
    fixture=out/'fidelity-cases.json';fixture.write_text(json.dumps(cases,separators=(',',':')))
    subprocess.run(['node',str(ROOT/'tests/oracle.cjs'),str(fixture)],check=True)
if __name__=='__main__':main()
