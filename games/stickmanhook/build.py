#!/usr/bin/env python3
"""Build a standalone Stickman Hook .nwa, without changing the NumPlay launcher.

python build.py --arm-bin /path/to/arm/bin --nwlink /path/to/nwlink
NWLINK may also name the nwlink JavaScript entry point (run with Node).
"""
import argparse
import json
import os
from pathlib import Path
import re
import shlex
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parent
REPO = ROOT.parents[1]
OUT = ROOT/'output'

def run(args):
    args = [str(x) for x in args]
    subprocess.run(args, cwd=ROOT, check=True)

def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--arm-bin', default=os.environ.get('ARM_BIN',''))
    ap.add_argument('--nwlink', default=os.environ.get('NWLINK','nwlink'))
    a = ap.parse_args()
    OUT.mkdir(exist_ok=True)
    arm = lambda name: str(Path(a.arm_bin)/('arm-none-eabi-'+name)) if a.arm_bin else 'arm-none-eabi-'+name
    if Path(a.nwlink).is_file():
        nw = [a.nwlink] if a.nwlink.endswith(('.exe','.cmd')) else ['node',a.nwlink]
    else:
        nw = ([v.replace('"','') for v in re.findall(r'(?:[^\s"]|"[^"]*")+',a.nwlink)]
              if os.name=='nt' else shlex.split(a.nwlink))
    flags = subprocess.check_output(nw+['eadk-cflags-device'],cwd=ROOT,text=True).strip()
    # nwlink quotes the include directory. Preserve backslashes inside Windows paths.
    target = ([v.replace('"','') for v in re.findall(r'(?:[^\s"]|"[^"]*")+',flags)]
              if os.name=='nt' else shlex.split(flags))
    opts = ['-std=c11',*target,'-Os','-Wall','-Wextra','-Werror','-ffunction-sections',
            '-fdata-sections','-fstack-usage','-fno-tree-loop-distribute-patterns',
            '-flto','-fno-fat-lto-objects','-fvisibility=internal']
    run([sys.executable,'tools/assets.py'])
    objects = []
    for name in ('main','game','save','render','levels','assets'):
        obj = OUT/(name+'.o')
        run([arm('gcc'),*opts,'-c','src/'+name+'.c','-o',obj]);objects.append(obj)
    packer = ROOT/'tools/compact_nwa.py'
    run([*nw,'png-icon-o','src/icon.png',OUT/'icon.o'])
    run([sys.executable,packer,'icon',OUT/'icon.o',OUT/'icon-compact.o','--objcopy',arm('objcopy')])
    run([arm('gcc'),*opts,'-nostartfiles','--specs=nano.specs',
         '-Wl,--relocatable,--gc-sections,-e,main','-flinker-output=nolto-rel',
         '-Wl,-u,eadk_app_name,-u,eadk_app_icon,-u,eadk_api_level',
         *objects,OUT/'icon-compact.o','-lm','-o',OUT/'standalone.o'])
    run([sys.executable,packer,'app',OUT/'standalone.o',OUT/'stickmanhook-debug.nwa','--ld',arm('ld')])
    run([sys.executable,packer,'release',OUT/'stickmanhook-debug.nwa',OUT/'stickmanhook.nwa','--objcopy',arm('objcopy')])
    for name in ('stickmanhook','stickmanhook-debug'):
        run([*nw,'nwa-bin',OUT/(name+'.nwa'),OUT/(name+'.bin'),'--ram-length','107674'])
    installed = (OUT/'stickmanhook.bin').read_bytes()
    assert installed == (OUT/'stickmanhook-debug.bin').read_bytes(), 'Release changes installed bytes'
    apps = REPO/'build/apps';apps.mkdir(parents=True,exist_ok=True)
    shutil.copyfile(OUT/'stickmanhook.nwa',apps/'StickmanHook.nwa')
    report = dict(nwa_bytes=(OUT/'stickmanhook.nwa').stat().st_size, installed_bytes=len(installed),
                  release_installed_matches_debug=True, artifact=str(apps/'StickmanHook.nwa'))
    (OUT/'build-report.json').write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps(report,indent=2))

if __name__=='__main__':
    main()
