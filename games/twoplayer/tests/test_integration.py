"""Build the real NumPlay launcher with only this module for integration testing.

This deliberately small test catalogue is not a release of NumPlay. Production
builds still read the complete games/games.json and use the existing twoplayer ID.
"""
import argparse
import json
import shlex
import subprocess
import sys
from elftools.elf.elffile import ELFFile
from pathlib import Path
import test_arm as arm

ROOT = Path(__file__).resolve().parents[1]
REPO = ROOT.parents[1]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--nwlink', default='nwlink')
    args = ap.parse_args()
    nw = shlex.split(args.nwlink)
    out = ROOT / 'output/integration'
    out.mkdir(parents=True, exist_ok=True)
    arm.tempfile.tempdir = str(out)

    def run(*cmd):
        subprocess.run([str(x) for x in cmd], cwd=REPO, check=True)

    def py(tool, *args):
        run(sys.executable, REPO / 'tools' / tool, *args)

    device = shlex.split(subprocess.check_output(nw + ['eadk-cflags-device'], text=True))
    cc = ['arm-none-eabi-gcc', *device, '-std=gnu11', '-Os', '-Wall', '-Wextra',
          '-Wno-unused-parameter', '-fno-math-errno', '-ffunction-sections', '-fdata-sections',
          '-fno-tree-loop-distribute-patterns', '-DNP_VERSION="local-2players"',
          '-I' + str(REPO / 'launcher/src')]
    games = json.loads((REPO / 'games/games.json').read_text())
    index = next(i for i, g in enumerate(games, 1) if g['id'] == 'twoplayer')
    game = games[index - 1]
    assert game['app'] == 'TwoPlayers.nwa' and game['records'] == ['twoplayer.sav']
    run('arm-none-eabi-ld', '-r', '-d', '-z', 'noexecstack', '--force-group-allocation', '-T', REPO / 'tools/module.ld',
        ROOT / 'output/module.o', '-o', out / 'stage1.o')
    run('arm-none-eabi-objcopy', '--keep-global-symbol=main',
        '--redefine-sym', 'eadk_keyboard_scan=np_keyboard_scan',
        '--redefine-sym', 'eadk_event_get=np_event_get',
        '--redefine-sym', 'eadk_display_draw_string=np_display_draw_string',
        out / 'stage1.o', out / 'stage2.o')
    py('npmodule.py', out / 'stage2.o', out / 'twoplayer.o', '--game', 'twoplayer',
       '--index', index, '--json', out / 'twoplayer.json')
    py('gen_games.py', REPO / 'games/games.json', out, out, '--only', 'twoplayer')
    py('shots.py', '--id', 'twoplayer', '--index', index, '--colors', 16,
       out / 'shots.c', ROOT / 'docs/shot.png')
    objects = [out / 'twoplayer.o']
    entry = out / 'entry_twoplayer.c' if (out / 'entry_twoplayer.c').exists() else out / 'gametable.c'
    sources = sorted((REPO / 'launcher/src').glob('*.c')) + [entry, out / 'shots.c']
    for src in sources:
        obj = out / ('launcher-' + src.stem + '.o')
        run(*cc, '-c', src, '-o', obj)
        objects.append(obj)
    marks = 'marks_twoplayer' if (out / 'marks_twoplayer.s').exists() else 'marks'
    for stem in ['arena', marks]:
        obj = out / (stem + '.o')
        run(*cc, '-c', out / (stem + '.s'), '-o', obj)
        objects.append(obj)
    run(*cc, '-DNP_APP_NAME="NumPlay integration test"', '-c', REPO / 'launcher/app_name.c', '-o', out / 'name.o')
    run(*nw, 'png-icon-o', REPO / 'launcher/assets/icon.png', out / 'icon.o')
    app = out / 'NumPlay-test.nwa'
    run(*cc, '-nostartfiles', '--specs=nano.specs', '-Wl,--relocatable', '-Wl,--gc-sections',
        '-Wl,-e,main', '-Wl,-u,eadk_app_name', '-Wl,-u,eadk_app_icon', '-Wl,-u,eadk_api_level',
        '-Wl,-T,' + str(out / 'numplay.ld'), *objects, out / 'name.o', out / 'icon.o', '-lm', '-o', app)
    report = []
    for profile, model, system, ram in [('n0120', 'n0120', 'epsilon', 153676),
                                      ('upsilon', 'n0110', 'upsilon', 107674)]:
        arm.emu.configure(model, system, ram)
        st = out / (profile + '.storage')
        arm.storage(st, [('keep.py', b'print(42)\n')])
        c = arm.emu.Calculator(str(app), storage_file=str(st), nwlink=nw)
        # npmodule deliberately removes the module's private symbols. Recover
        # their RAM offsets from its input solely for these debugger assertions.
        with (out / 'stage1.o').open('rb') as f:
            elf = ELFFile(f)
            for sym in elf.get_section_by_name('.symtab').iter_symbols():
                if sym.name in ('screen', 'game', 'phase'):
                    section = elf.get_section(sym['st_shndx']).name
                    assert section == '.bss'
                    c.symbols[sym.name] = c.symbols['np_twoplayer_bss'] + sym['st_value']
        c.enable_checks()
        c.run(1100)
        c.save_png(str(out / (profile + '-launcher.png')))

        def key(k, wait=200):
            t = int(c.now_ms) + 20
            c.keys += arm.presses((t, k, 70))
            c.run(t + wait)

        key('ok', 650)  # Actual carousel -> existing 2 Players module.
        assert arm.uint(c, 'screen') == 0
        key('9'); key('ok'); key('ok')
        assert arm.uint(c, 'screen') == 2 and arm.uint(c, 'game') == 4
        key('back'); key('down'); key('down'); key('ok')
        assert arm.uint(c, 'screen') == 0
        key('back'); key('down'); key('ok', 800)  # Module Quit -> carousel.
        assert not c.exited
        c.save_png(str(out / (profile + '-returned.png')))
        key('ok', 650)  # Re-enter: BSS reset, selection restored from real storage.
        assert arm.uint(c, 'screen') == 0 and arm.uint(c, 'game') == 4
        key('home', 600)
        arm.exited(c, system == 'upsilon')
        records = arm.records(st.read_bytes())
        assert len(records['twoplayer.sav']) == 128 and records['keep.py'] == b'print(42)\n'
        assert b'twoplayer.sav' in records['numplay_saves.py']
        report.append(dict(profile=profile, launch_return_reenter=True, backup=True,
                           stack_bytes=c.stack_used()))
    (out / 'report.json').write_text(json.dumps(report, indent=2) + '\n')
    print('PASS: existing NumPlay module launch, return, re-entry, shared saves and backup.')


if __name__ == '__main__':
    main()
