"""Check translation coverage, mechanics and all layouts using the real fonts."""
from pathlib import Path
import shutil,importlib.util,subprocess,sys,re
repo=Path(__file__).resolve().parents[3];out=repo/'games/twoplayer/output/languages';out.mkdir(parents=True,exist_ok=True)
def load(name,p):
 s=importlib.util.spec_from_file_location(name,p);m=importlib.util.module_from_spec(s);s.loader.exec_module(m);return m
lang=load('lang',repo/'tools/lang.py')
for code in ['fr','zh']:
 dst=out/code
 for sub in ['games/twoplayer/src','games/twoplayer/tests','games/common','launcher/src']:
  for p in (repo/sub).glob('*'):
   if p.is_file() and p.suffix in ['.c','.h']:
    target=dst/sub/p.name;target.parent.mkdir(parents=True,exist_ok=True);text=p.read_text(encoding='utf-8')
    if sub=='games/twoplayer/src':
     table=lang.load_table('twoplayer',code);missing=[s for s in lang.marked(p) if s not in table];assert not missing,missing
     text=lang.MARK.sub(lambda m:'T("'+table[m.group(2)]+'")',text)
    if p.name=='np_text.h':text=text.replace('#define NP_TEXT_EXTRA 0','#define NP_TEXT_EXTRA 1')
    target.write_text(text,encoding='utf-8')
 texts='\n'.join(lang.load_table('twoplayer',code).values());(dst/'texts.txt').write_text(texts,encoding='utf-8')
 extra=','.join(str(c)for c in sorted({ord(c)for c in texts if 0x80<=ord(c)<0x180}))
 subprocess.run([sys.executable,str(repo/'tools/fontgen.py'),str(dst/'launcher/src/font_data.c'),'--extra',extra],check=True)
 subprocess.run([sys.executable,str(repo/'tools/xfont.py'),'table',str(repo/'assets/fonts/fusion-pixel-12px.bin'),str(dst/'launcher/src/np_xfont.h'),str(dst/'texts.txt')],check=True)
 target=dst/'games/twoplayer';(target/'output').mkdir(exist_ok=True)
 subprocess.run(['gcc','-std=c11','-O2','-Wall','-Wextra','-Wno-unused-parameter','-Wno-misleading-indentation','-Itests','tests/test.c','../../launcher/src/gfx.c','../../launcher/src/font_data.c','../../launcher/src/xfont.c','-lm','-o','output/test.exe'],cwd=target,check=True)
 subprocess.run([str(target/'output/test.exe')],cwd=target/'output',check=True)
 print('Translations and layout pass:',code)
