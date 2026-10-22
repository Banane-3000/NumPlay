#!/usr/bin/env python3
"""src/art.c and src/art.h: Tomb of the Mask's pictures and its pixel font, from the web game taken apart.

    python3 -I tools/art.py REF

REF is the reference kept outside the repository: `art/sprites/<name>_<id>.png` (every Sprite of the game as a
PNG, taken out with UnityPy), `art/tex/hm_pxl_01-Regular_copy_<id>.png` and `art/fonts/font_hm_pxl_01-Regular_*.json`
(the game's pixel font and its glyph table). The game's sprites are white and black on transparent and tinted when
drawn; each one keeps two planes of bits here: drawn or not, and white (tinted) or black. Nothing in REF is run.
"""
import glob
import json
import os
import re
import sys

from PIL import Image

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

# (C name, sprite names): every sprite the game draws, in the order of art.h's ids
GROUPS = []


def group(cname, names):
    GROUPS.append((cname, names))


def frames(prefix, n, first=1):
    return ['%s_%d' % (prefix, i) for i in range(first, first + n)]


for m in range(16):
    group('CHAR%d_ARRIVE' % m, frames('%d_Char_arrive' % m, 8))
    group('CHAR%d_IDLE' % m, frames('%d_Char_idle' % m, 6))
    group('CHAR%d_LONG' % m, frames('%d_Char_long_jump' % m, 5))
    group('CHAR%d_RUN' % m, frames('%d_Char_run' % m, 10))
group('FLIGHT', frames('Hero_flight', 3))
group('TRAIL', ['Hero_jump_trail_1', 'Hero_jump_trail'])
group('JUMP_FX', frames('Hero_jump', 2))
group('LANDING', frames('Landing', 6))
group('DEATH', frames('Hero_death', 10))
group('SHIELD_FX', frames('Shield_fx', 2))
group('RUN_FX', frames('Run_particles_1', 4))
# walls: five styles, then the bonus walls, the bonus wheels, spikes and trap spikes; per set: corners UL UR DL DR,
# inner corners UL UR DL DR, edges U L R D (each a list of variants)
group('SECRET', ['Secret_Corner_u_l', 'Secret_Corner_u_r', 'Secret_Corner_d_l', 'Secret_Corner_d_r',
                 'Secret_Inner_u_l', 'Secret_Inner_u_r', 'Secret_Inner_d_l', 'Secret_Inner_u_r',
                 'Secret_u', 'Secret_l', 'Secret_r', 'Secret_d'])
group('WHEEL', ['Wheel_1'])
group('SPIKES', ['Corner_spikes_u_l', 'Corner_spikes_u_r', 'Corner_spikes_d_l', 'Corner_spikes_d_r',
                 'Spikes_u', 'Spikes_l', 'Spikes_r', 'Spikes_d'])
group('MSPIKES', ['Corner_M_spikes_u_l', 'Corner_M_spikes_u_r', 'Corner_M_spikes_d_l', 'Corner_M_spikes_d_r',
                  'M_Spikes_u', 'M_Spikes_l', 'M_Spikes_r', 'M_Spikes_d'])
group('COIN', frames('Coin', 4))
group('DOT', ['Dot_game'])
group('STAR', frames('Star_anim', 4))
group('EXIT', frames('Exit', 3))
group('ICE', ['Ice'])
group('ADDICT', frames('Coin_addict', 4))
group('FREEZE', frames('Freeze', 4))
group('MAGNET', frames('Magnet', 4))
group('BOOST', frames('Scores', 4))
group('TRAMPLIN', frames('Tramplin', 3))
group('PORTAL', frames('Portal', 11))
group('CANNON', frames('Cannon', 2))
group('CANNON_FX', frames('Cannon_attack', 4))
group('BULLET', ['Cannon_bolid'])
group('BANG', frames('Cannon_bang', 4))
group('BAT', frames('Bat', 5))
group('FISH', frames('fish_hedgehog', 4))
group('FISH_ATTACK', frames('fish_hedgehog_attack', 5))
group('TEETH', frames('Teeth', 6))
group('SPIKESM', frames('M_Spikes_attack', 3))
group('PLATFORM', frames('Platform', 3))
group('ROCK', ['Rock'])
group('MONKEY', frames('Monkey_idle', 8))
group('MONKEY_EMPTY', frames('Monkey_no_rock', 8))
group('TIMERWALL', ['Timer_wall_%d' % i for i in range(11)])
group('ROTOR', ['Rotor_1'])
group('LIGHTBALL', ['lightning', 'lightning_1', 'lightning_2', 'lightning_3'])
group('SNAKE_OUT', frames('Snake_exit', 6))
group('SNAKE_OUT_UP', frames('Snake_exit_up', 6))
group('DOOR', ['Secret_room_1'])
group('GATES', frames('Gates', 14))
group('TORCH', frames('Torch', 4))
group('EOLO', frames('eolo', 16))
group('DOT_FX', frames('Dot_pick', 3))
group('COIN_FX', frames('Coin_pick', 4))
group('POWER_FX', frames('Power_pick', 5))
group('ICE_FX', frames('Ice_dest', 4))
group('PORTAL_FX', frames('Portal_enter', 7))
group('LOGO', ['Logo_reveal_13'])
group('STAR_BIG', frames('Star_big', 2))
group('STAR_SMALL', ['Star_small'])
group('LOCK', ['Lock_icon'])
group('COIN_ICON', ['icon_coin'])
group('CHEST', ['Chest'])
group('CHESTS', ['Chests'])
group('WHEEL_MIDDLE', ['Middle'])
group('WHEEL_PRIZE', ['Wheel_prize'])
group('SHIELD_ICON', ['Shield'])
group('SHIELD_BIG', ['Shield_big'])
group('FREEZE_BIG', ['Freeze_big'])
group('MAGNET_BIG', ['Magnet_big'])
group('BOOST_BIG', ['Score_booster'])
group('ADDICT_BIG', ['Coin_addict_big'])
group('MISSION_ICONS', ['Mission_1_Map_1', 'Mission_2_Arcade_1', 'Mission_3_Dots', 'Mission_4_Stars',
                        'Mission_5_Map_2', 'Mission_6_Map_3', 'Mission_7_Arcade_2', 'Chest'])


def wall_sets(sprites):
    """Each wall style's 12 parts (corners, inner corners, edges), each with its variants."""
    out = []
    for s in range(1, 6):
        parts = []
        for part in ['Corner_u_l', 'Corner_u_r', 'Corner_d_l', 'Corner_d_r', 'Inner_u_l', 'Inner_u_r', 'Inner_d_l',
                     'Inner_d_r', 'Wall_u', 'Wall_l', 'Wall_r', 'Wall_d']:
            base = '%d_%s' % (s, part)
            names = sorted([n for n in sprites if re.fullmatch(re.escape(base) + r'(_\d+)?', n)],
                           key=lambda n: int(n.rsplit('_', 1)[1]) if n.rsplit('_', 1)[1].isdigit() and n != base else 0)
            assert names, base
            parts.append(names)
        out.append(parts)
    return out


# the menus' pictures are drawn in colour in the game (not white masks): every pixel not black is ink here
UI = {'LOGO', 'STAR_BIG', 'STAR_SMALL', 'LOCK', 'COIN_ICON', 'CHEST', 'CHESTS', 'WHEEL_MIDDLE', 'WHEEL_PRIZE',
      'SHIELD_ICON', 'SHIELD_BIG', 'FREEZE_BIG', 'MAGNET_BIG', 'BOOST_BIG', 'ADDICT_BIG', 'MISSION_ICONS'}


def bits(img, ui=False):
    """Two planes, row by row: drawn (alpha >= 128) and white (luminance >= 128; for the menus' colour pictures,
    any channel >= 64)."""
    a = img.getchannel('A').load()
    l = img.convert('L').load()
    rgb = img.convert('RGB').load()
    w, h = img.size
    cover, ink = [], []
    for y in range(h):
        for x in range(w):
            cover.append(a[x, y] >= 128)
            bright = max(rgb[x, y]) >= 64 if ui else l[x, y] >= 128
            ink.append(a[x, y] >= 128 and bright)
    if ui and not any(ink):
        ink = list(cover)   # a black silhouette (the game colours it): all of it is ink
    return cover, ink


def pack(bitlist):
    out = bytearray((len(bitlist) + 7) // 8)
    for i, v in enumerate(bitlist):
        if v:
            out[i // 8] |= 128 >> (i % 8)
    return bytes(out)


def c_array(ctype, name, values, per=24):
    vals = [str(v) for v in values]
    lines = [','.join(vals[i:i + per]) for i in range(0, len(vals), per)]
    return 'const %s %s[%d] = {\n%s};\n' % (ctype, name, len(vals), ',\n'.join(lines))


def main():
    if len(sys.argv) != 2:
        raise SystemExit(__doc__)
    ref = sys.argv[1]
    sprites = {}
    for path in glob.glob(os.path.join(ref, 'art', 'sprites', '*.png')):
        name = os.path.basename(path).rsplit('_', 1)[0]
        sprites.setdefault(name, path)
    walls = wall_sets(sprites)
    entries = []           # (name, w, h, offset)
    blob_cover, blob_ink = bytearray(), bytearray()
    cache = {}
    ids = []

    def add(name, ui=False):
        img = Image.open(sprites[name]).convert('RGBA')
        cover, ink = bits(img, ui)
        key = (img.size, pack(cover), pack(ink))
        if key not in cache:
            cache[key] = len(blob_cover)
            blob_cover.extend(pack(cover))
            blob_ink.extend(pack(ink))
        entries.append((name, img.size[0], img.size[1], cache[key]))
        return len(entries) - 1

    for cname, names in GROUPS:
        first = len(entries)
        for n in names:
            add(n, cname in UI)
        ids.append((cname, first, len(names)))
    # wall styles: 12 parts, each a run of variants: tm_wall_part[style][part] = (first, count)
    wall_index = []
    for s, parts in enumerate(walls):
        row = []
        for names in parts:
            first = len(entries)
            for n in names:
                add(n)
            row.append((first, len(names)))
        wall_index.append(row)
    # the pixel font: 64 glyphs on a 64x128 texture (flipped), in a 10x10 box with 1 pixel of margin
    fjson = json.load(open(glob.glob(os.path.join(ref, 'art', 'fonts', 'font_hm_pxl_01-Regular_813.json'))[0]))
    ftex = Image.open(glob.glob(os.path.join(ref, 'art', 'tex', 'hm_pxl_01-Regular_copy_*.png'))[0]).convert('RGBA')
    fw, fh = ftex.size
    alpha = ftex.getchannel('A')
    glyphs = {}
    fbits = bytearray()
    for r in fjson['rects']:
        if r['index'] > 127:
            continue
        uv, vert = r['uv'], r['vert']
        x0 = round(uv['x'] * fw)
        y0 = round(uv['y'] * fh)
        w = round(abs(uv['width']) * fw)
        h = round(abs(uv['height']) * fh)
        # uv y counts from the texture's bottom; the glyphs are stored upside down in it
        top = fh - y0
        crop = alpha.crop((x0, top, x0 + w, top + h)).transpose(Image.Transpose.FLIP_TOP_BOTTOM)
        px = [crop.getpixel((x, y)) >= 128 for y in range(h) for x in range(w)]
        glyphs[r['index']] = (len(fbits), w, h, int(round(r['advance'])), int(round(vert['x'])), 10 - int(round(vert['y'])))
        fbits.extend(pack(px))
    # punctuation the font lacks, drawn in its 2-pixel strokes (5 x 8 grid at 2x would be too big: 8 rows, 1x)
    extra = {
        '.': ['00', '00', '00', '00', '00', '00', '11', '11'],
        ',': ['00', '00', '00', '00', '00', '00', '11', '11', '01'],
        ':': ['00', '00', '11', '11', '00', '00', '11', '11'],
        '-': ['0000', '0000', '0000', '1111', '1111', '0000', '0000', '0000'],
        '+': ['000000', '001100', '001100', '111111', '111111', '001100', '001100', '000000'],
        '/': ['000011', '000011', '001100', '001100', '110000', '110000', '000000', '000000'],
        '%': ['110011', '110011', '000110', '001100', '011000', '110011', '110011', '000000'],
        '?': ['011110', '110011', '000011', '000110', '001100', '000000', '001100', '001100'],
        "'": ['11', '11', '01', '00', '00', '00', '00', '00'],
        '(': ['0011', '0110', '1100', '1100', '1100', '1100', '0110', '0011'],
        ')': ['1100', '0110', '0011', '0011', '0011', '0011', '0110', '1100'],
    }
    for ch, rows in extra.items():
        if rows is None or ord(ch) in glyphs:
            continue
        w, h = len(rows[0]), len(rows)
        px = [rows[y][x] == '1' for y in range(h) for x in range(w)]
        glyphs[ord(ch)] = (len(fbits), w, h, w + 2, 0, 1)
        fbits.extend(pack(px))
    glyphs[32] = (glyphs[32][0], 0, 0, 5, 0, 0)   # the space: the font's is 2 pixels, too narrow for words
    glyph_rows = []
    for c in range(32, 128):
        g = glyphs.get(c) or glyphs.get(c - 32 if 97 <= c <= 122 else c) or glyphs[ord('?')]
        glyph_rows.append('{%d,%d,%d,%d,%d,%d}' % g)
    assert len(blob_cover) < 65536
    # header
    hlines = ['/* Generated by tools/art.py from the web game\'s own sprites and font: do not edit. */',
              '#ifndef TM_ART_H', '#define TM_ART_H', '#include <stdint.h>', '',
              '/* a sprite: size and where its two planes of bits start in tm_cover and tm_ink (row by row) */',
              'typedef struct { uint8_t w, h; uint16_t at; } tm_sprite;',
              '/* a glyph of the game\'s pixel font: bits, size, advance, offset */',
              'typedef struct { uint16_t at; uint8_t w, h, advance; int8_t x, y; } tm_glyph;',
              'extern const tm_sprite tm_sprites[];', 'extern const uint8_t tm_cover[], tm_ink[], tm_font_bits[];',
              'extern const tm_glyph tm_glyphs[96];',
              '/* a wall style\'s parts: corners UL UR DL DR, inner corners UL UR DL DR, edges U L R D */',
              'extern const uint16_t tm_wall_part[5][12][2];', '']
    for cname, first, n in ids:
        hlines.append('#define S_%s %d' % (cname, first))
        if n > 1:
            hlines.append('#define S_%s_N %d' % (cname, n))
    hlines += ['#define TM_SPRITES %d' % len(entries), '#endif', '']
    clines = ['/* Generated by tools/art.py from the web game\'s own sprites and font: do not edit. */',
              '#include "art.h"', '',
              'const tm_sprite tm_sprites[%d] = {%s};' % (len(entries), ','.join('{%d,%d,%d}' % (w, h, o) for _, w, h, o in entries)),
              c_array('uint8_t', 'tm_cover', blob_cover), c_array('uint8_t', 'tm_ink', blob_ink),
              c_array('uint8_t', 'tm_font_bits', fbits),
              'const tm_glyph tm_glyphs[96] = {%s};' % ','.join(glyph_rows),
              'const uint16_t tm_wall_part[5][12][2] = {%s};' % ','.join(
                  '{%s}' % ','.join('{%d,%d}' % p for p in row) for row in wall_index), '']
    open(os.path.join(ROOT, 'src', 'art.h'), 'w').write('\n'.join(hlines))
    open(os.path.join(ROOT, 'src', 'art.c'), 'w').write('\n'.join(clines))
    print('art: %d sprites, %d bytes of bits x2, font %d bytes' % (len(entries), len(blob_cover), len(fbits)))


if __name__ == '__main__':
    main()
