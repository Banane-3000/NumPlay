#!/usr/bin/env python3
"""Build indexed, row-addressable sprites from the supplied Halfbrick artwork.

Run with Pillow after changing assets/source. No image decoder or heap is used
on the calculator. The generated file is checked in, so normal builds need
neither Pillow nor access to the source archive.
"""
from pathlib import Path
from PIL import Image
import argparse
import hashlib
import io
import json

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / 'assets/source'
OUT = ROOT / 'src/assets.h'
NN = Image.Resampling.NEAREST
LZ = Image.Resampling.LANCZOS
images = []


def source(name):
    return Image.open(SOURCE / (name + '.png')).convert('RGBA')


def add(name, image, colors=32, palette=None):
    images.append((name, image, colors, palette))


def crop_cell(im, i, size=32):
    x = i % (im.width // size) * size
    y = i // (im.width // size) * size
    return im.crop((x, y, x + size, y + size))


def rgb565(rgb):
    r, g, b = rgb
    return (r >> 3) << 11 | (g >> 2) << 5 | (b >> 3)


def runs(row, bits):
    """PackBits: high bit = repeat, otherwise literal; count minus one."""
    out = bytearray()
    x = 0
    while x < len(row):
        n = 1
        while x + n < len(row) and row[x + n] == row[x] and n < 128:
            n += 1
        if n >= 3:
            out.extend((0x80 | (n - 1), row[x]))
            x += n
        else:
            start = x
            x += n
            while x < len(row) and x - start < 128:
                if x + 2 < len(row) and row[x] == row[x + 1] == row[x + 2]:
                    break
                x += 1
            out.append(x - start - 1)
            literal = row[start:x]
            if bits == 4:
                out.extend((literal[k] << 4) | (literal[k + 1] if k + 1 < len(literal) else 0)
                           for k in range(0, len(literal), 2))
            else:
                out.extend(literal)
    return out


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--check', action='store_true', help='verify deterministic generated files without writing')
    args = ap.parse_args()
    images.clear()
    manifest = json.loads((ROOT / 'assets/source-manifest.json').read_text())
    for name, entry in manifest['files'].items():
        assert hashlib.sha256((SOURCE / (name + '.png')).read_bytes()).hexdigest() == entry['sha256'], name
    # One low-resolution tile renders at 2x: 256x192 pixels of the real lab.
    lab = source('laboratory').crop((0, 128, 1024, 752)).resize((128, 96), LZ)
    add('lab', lab, 32)
    add('floor', source('laboratory').crop((0, 752, 1024, 880)).resize((128, 12), LZ), 16)
    add('jetpack_word', source('jetpack_word').resize((205, 59), LZ), 16)
    add('joyride_word', source('joyride_word').resize((210, 55), LZ), 16)
    hero = source('barry_hero')
    add('hero', hero.crop(hero.getbbox()).resize((72, 104), LZ), 48)
    # Body/head sheets share aligned 32-pixel cells. Ground frames 0..3,
    # airborne frames 4..7. Composite before reducing to calculator scale.
    frames = []
    body, head, pack = source('barry_body'), source('barry_head'), source('machinegun')
    pack = pack.crop(pack.getbbox()).resize((16, 16), NN)
    for i in range(8):
        im = Image.new('RGBA', (40, 48))
        im.alpha_composite(pack, (0, 22))
        im.alpha_composite(crop_cell(body, i), (7, 16))
        im.alpha_composite(crop_cell(head, i), (7, 0))
        frames.append(im.resize((28, 40), NN))
    atlas = Image.new('RGB', (28 * 8, 40), '#111017')
    for i, im in enumerate(frames):
        atlas.paste(im.convert('RGB'), (i * 28, 0))
    pal = atlas.quantize(colors=31, method=Image.Quantize.MEDIANCUT, dither=Image.Dither.NONE)
    barry_colors = [tuple(pal.getpalette()[i:i + 3]) for i in range(0, 93, 3)]
    for i, im in enumerate(frames):
        add('barry_' + str(i), im, 32, barry_colors)
    coins = source('coins')
    for i in range(8):
        add('coin_' + str(i), crop_cell(coins, i).resize((11, 11), LZ), 16)
    add('shield', source('shield').resize((18, 18), LZ), 24)
    add('missile', crop_cell(source('missiles'), 0).resize((28, 23), NN), 24)
    add('zapper_cap', crop_cell(source('zapper_caps'), 0, 128).resize((15, 15), LZ), 24)
    electricity = source('electricity')
    for i in range(3):
        beam = electricity.crop((0, i * 64, 1024, i * 64 + 64))
        add('beam_h_' + str(i), beam.resize((58, 10), LZ), 16)
        add('beam_v_' + str(i), beam.resize((70, 10), LZ).transpose(Image.Transpose.ROTATE_90), 16)
        add('beam_c_' + str(i), beam.resize((100, 10), LZ).transpose(Image.Transpose.ROTATE_90), 16)
    flash = source('muzzle_flash')
    for i in range(2):
        add('flash_' + str(i), crop_cell(flash, i, 64).resize((14, 16), NN), 16)
    scientists = source('scientists')
    for i in range(4):
        # Whole person per cell, as opposed to Barry's separate body/head.
        cell = crop_cell(scientists, i)
        add('scientist_' + str(i), cell.resize((17, 21), NN), 24)
    warning = source('warnings').crop((0, 0, 64, 64))
    add('warning', warning.resize((18, 18), NN), 16)

    lines = ['/* Generated by tools/assets.py. Artwork: Halfbrick; see assets/README.md. */',
             '#ifndef JJ_ASSETS_H', '#define JJ_ASSETS_H', '#include <stdint.h>',
             'typedef struct { uint16_t w,h; const uint16_t *palette,*rows; const uint8_t *data; uint8_t bits; } jj_sprite;']
    total = 0
    report = {}
    def array(kind, name, values, fmt=str):
        lines.append('static const ' + kind + ' ' + name + '[] = {')
        for k in range(0, len(values), 24):
            lines.append('  ' + ','.join(fmt(v) for v in values[k:k + 24]) + ',')
        lines.append('};')

    for name, im, colors, shared in images:
        rgb = im.convert('RGB')
        if shared:
            palette_im = Image.new('P', (1, 1))
            flat = [v for c in shared for v in c]
            palette_im.putpalette(flat + flat[-3:] * (256 - len(shared)))
            indexed = rgb.quantize(palette=palette_im, dither=Image.Dither.NONE)
            palette_rgb = shared
        else:
            indexed = rgb.quantize(colors=colors - 1, method=Image.Quantize.MEDIANCUT, dither=Image.Dither.NONE)
            packed_palette = indexed.getpalette()
            palette_rgb = [tuple(packed_palette[i:i + 3]) for i in range(0, len(packed_palette), 3)]
        # Threshold alpha before encoding: index zero is transparent.
        indices = [v + 1 if a >= 96 else 0 for v, a in zip(indexed.getdata(), im.getchannel('A').getdata())]
        palette = [0] + [rgb565(c) for c in palette_rgb]
        bits = 4 if max(indices) < 16 else 8
        offsets = [0]
        data = bytearray()
        decoded = []
        for y in range(im.height):
            row = indices[y * im.width:(y + 1) * im.width]
            packed = runs(row, bits)
            # Round-trip each row and bound every index/offset at generation.
            check = []
            pos = 0
            while pos < len(packed):
                tag = packed[pos]; pos += 1
                count = (tag & 127) + 1
                if tag & 128:
                    check.extend([packed[pos]] * count); pos += 1
                else:
                    if bits == 4:
                        check.extend((packed[pos + k // 2] >> (0 if k & 1 else 4)) & 15 for k in range(count))
                        pos += (count + 1) // 2
                    else:
                        check.extend(packed[pos:pos + count]); pos += count
            assert row == check and all(v < len(palette) for v in check)
            data.extend(packed); offsets.append(len(data)); decoded.extend(check)
        assert len(data) < 65536
        if shared:
            palette_name = 'jj_barry_palette_0'
        else:
            palette_name = 'jj_' + name + '_palette'
            array('uint16_t', palette_name, palette, hex)
        array('uint16_t', 'jj_' + name + '_rows', offsets)
        array('uint8_t', 'jj_' + name + '_data', list(data))
        lines.append(f'static const jj_sprite jj_{name} = {{{im.width},{im.height},{palette_name},jj_{name}_rows,jj_{name}_data,{bits}}};')
        cost = len(data) + len(offsets) * 2 + (0 if shared else len(palette) * 2)
        total += cost
        report[name] = dict(width=im.width, height=im.height, bits=bits, bytes=cost)
    # Shared shirt palette: preserve warm skin/hair; tint the cool suit tones.
    palette_lines = []
    for suit, target in enumerate([(1, 1, 1), (0.82, 1.08, 1.24), (1.45, 0.63, 1.04), (0.83, 1.32, 0.60)]):
        colors = [(0, 0, 0)] + barry_colors
        if suit:
            remapped = [colors[0]]
            for r, g, b in colors[1:]:
                if b > r * 1.05 and b > g * 0.95:
                    light = max(r, g, b)
                    if suit == 1:
                        light = min(235, light + 65)
                    r, g, b = [min(255, int(light * t)) for t in target]
                remapped.append((r, g, b))
            colors = remapped
        palette_lines.append('static const uint16_t jj_barry_palette_' + str(suit) + '[] = {' + ','.join(hex(rgb565(c)) for c in colors) + '};')
    lines[5:5] = palette_lines
    for group, count in [('barry', 8), ('coin', 8), ('beam_h', 3), ('beam_v', 3), ('beam_c', 3), ('flash', 2), ('scientist', 4)]:
        lines.append('static const jj_sprite *const jj_' + group + '[] = {' + ','.join('&jj_' + group + '_' + str(i) for i in range(count)) + '};')
    lines.append('static const uint16_t *const jj_suit_palettes[] = {jj_barry_palette_0,jj_barry_palette_1,jj_barry_palette_2,jj_barry_palette_3};')
    lines.append('#endif\n')
    header = '\n'.join(lines).encode('utf-8')
    report['total_encoded_bytes'] = total + 256
    report_data = (json.dumps(report, indent=2) + '\n').encode('utf-8')
    # The calculator's icon uses the supplied character artwork as well.
    icon = Image.new('RGBA', (55, 56), '#17283d')
    hero = source('barry_hero'); hero.thumbnail((39, 55), LZ)
    icon.alpha_composite(hero, (9, 0))
    icon_data = io.BytesIO()
    icon.convert('RGB').quantize(colors=24, dither=Image.Dither.NONE).convert('RGB').save(icon_data, format='PNG', optimize=True)
    outputs = {OUT: header, ROOT / 'assets/packed.json': report_data, ROOT / 'src/icon.png': icon_data.getvalue()}
    for path, data in outputs.items():
        if args.check:
            assert path.read_bytes() == data, 'Regenerate ' + str(path)
        else:
            path.write_bytes(data)
    print(json.dumps({'sprites': len(images), 'encoded_bytes': total + 256, 'check': args.check}, indent=2))


if __name__ == '__main__':
    main()
