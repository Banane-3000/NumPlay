import argparse, subprocess, os
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
def main():
    ap=argparse.ArgumentParser();ap.add_argument('--cc',default='cc');a=ap.parse_args();out=ROOT/'output';out.mkdir(exist_ok=True)
    for name,sources in [('engine',['tests/test_engine.c','src/uno.c']),('ui',['tests/test_ui.c','src/uno.c','../../launcher/src/gfx.c','../../launcher/src/font_data.c','../../launcher/src/xfont.c'])]:
        exe=out/('test-'+name+('.exe'if os.name=='nt'else''))
        subprocess.run([a.cc,'-std=c11','-O2','-Wall','-Wextra','-Werror','-Wno-misleading-indentation','-Wno-unused-parameter',
                        '-ffunction-sections','-fdata-sections','-Itests',*sources,'-Wl,--gc-sections','-lm','-o',str(exe)],cwd=ROOT,check=True)
        subprocess.run([str(exe)],cwd=out,check=True)
if __name__=='__main__':main()
