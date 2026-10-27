#!/usr/bin/env python3
"""src/art.h: Halfbrick's own pictures, at the calculator's size (2/3 of the game's points).

    python3 -I tools/art.py FB_DIR APK_DIR src/art.h

FB_DIR: the PNGs of archive.org's "Jetpack Joyride facebook assets" (jetpack joyride images.rar, unpacked):
Barry, the lab, coins, zappers, missiles, lasers, the vehicle box, the Profit Bird, the Teleporter, the Gravity
Suit, the scientists, the title. APK_DIR: the 1.3.5 Android package unzipped, for what that archive lacks: the
Lil' Stomper and the Bad As Hog, the spin token, the HUD's font (New Athletic M54) and the Final Spin's pictures.
Nothing of either is run; the pictures are only read (Pillow).

Each sheet is a few frames sharing a palette; a frame is cut to its pixels (x, y: where its top left goes from
the anchor) and stored row by row in runs, so the drawing reads only the rows of the strip it draws. Index 0 is
see-through. Literal runs pack two indices to a byte when the palette has 16 colours or fewer.
"""
import os
import struct
import sys
from PIL import Image

LZ = Image.Resampling.LANCZOS
BOX = Image.Resampling.BOX
S = 2 / 3

FB_FILES = {
    'body': '21420928-1f48-42b3-af14-a9d19b8d8020.png',
    'head': 'c26be092-2285-4590-b144-df89a4dbf0c0.png',
    'flash': '0ae2fd6b-7484-4bd8-9184-f3bf9fac5d08.png',
    'lab': '1a3e77c5-b730-4ae6-9506-35e2644094c2.png',
    'coins': '5f7ee621-7ab3-41ef-bbd7-db48931e8f42.png',
    'missile': '970298ce-a6e3-411a-9ab3-5f6d62ad9f9e.png',
    'fire': '8ac9adb9-68f1-4d7a-b10b-de042fc9eca1.png',
    'boom': '9806e286-afa7-4496-8d7d-536ff8f2c8a6.png',
    'bird': '2a7a539e-4167-4abb-bf55-ba8119abc77b.png',
    'cft': 'bade5fc4-2ec2-42f7-957d-675ee1ac9aed.png',
    'grav': '915d9cc3-781c-430e-9ac8-0cb8435aabea.png',
    'worker1': '02dfee8e-274f-412f-a1af-ae35a526b3c2.png',
    'worker1_head': 'c1751fe8-911a-464b-b16a-de5692b8db11.png',
    'worker2': '391e301b-50de-4770-8db5-6bcd97d8e117.png',
    'worker2_head': '14344b46-2a22-43e2-b7bd-7cf092ab0fc3.png',
    'jetpack': 'ad9d62ec-8e4e-416e-826e-f577fe609103.png',
    'joyride': '4f8632f9-cd71-4b2f-898a-2f47c0344b7f.png',
    'hero': 'ee031bfe-32a8-487d-9f68-f0c66a3acf55.png',
    'orb': '5e3d78f3-b063-4e7b-bb8f-b319dfc6855e.png',
    'orb_rot': 'f2e7c59a-6915-4461-8d7b-4d2e58ff9059.png',
    'box': 'e763b7a7-db2f-462c-b910-e2da9b8d71ce.png',
    'laser': '4a2e43c1-5af8-4c01-8e71-e9e2e7815986.png',
    'warning': '89bfda72-a412-49ec-b87f-a4b09ff275f7.png',
    # the other rooms: the glass tube under the sea, the lava cave, the greenhouse (their ends have the lab's doors)
    'tube_in': '2c032eb0-5d37-44ae-89d3-6115c948539f.png',
    'tube_out': '13d94ae4-369d-47a2-8e6e-34b1ffe1b4be.png',
    'sea': '21cb3c74-6319-4e99-9227-8f4135b4d391.png',
    'coral1': '8b2f5933-fbcf-4ab3-a13b-d76c1ec6724e.png',
    'coral2': 'f20e09b1-5fe1-4ab3-868f-8b70df989dac.png',
    'cave_in': '828c2c84-2158-481a-a332-325861be1e5a.png',
    'cave': '75abe928-ba54-4755-b6e2-5541f69ecaab.png',
    'cave_out': '3c9d663e-8746-4e0e-9a3a-1de3af640951.png',
    'lava': '7ad87cd7-f077-4f2a-916b-fa7663d124e7.png',
    'green_in': 'cdf5238d-9e92-4c7a-a432-f51c1bf62b7b.png',
    'green': '283169e7-ed6c-4601-9ac8-98da3ef7f588.png',
    'green_out': '55cde58e-fed9-4fdc-b2a0-9ac6f3914cbc.png',
    'trees': '76bfac6c-f63e-4114-a804-03608b017ef5.png',
    'trees_far': '4293db94-f1da-4362-b672-490e500a46d9.png',
}


def tex(path):
    """Halfbrick .tex (Android): log2 w, log2 h, format, ?, u16 w, u16 h, u32, then RGBA8888 rows."""
    b = open(path, 'rb').read()
    w, h = struct.unpack_from('<HH', b, 4)
    return Image.frombytes('RGBA', (w, h), b[12:12 + w * h * 4])


def cell(im, i, cw, ch, cols=None):
    cols = cols or im.width // cw
    x, y = i % cols * cw, i // cols * ch
    return im.crop((x, y, x + cw, y + ch))


def scale(im, k, f=LZ):
    return im.resize((max(1, round(im.width * k)), max(1, round(im.height * k))), f)


def rgb565(c):
    r, g, b = c[:3]
    return (r >> 3) << 11 | (g >> 2) << 5 | (b >> 3)


def runs(row, bits):
    """PackBits: high bit set = a run of one index, else literals; the count minus one in the low bits."""
    out = bytearray()
    x = 0
    while x < len(row):
        n = 1
        while x + n < len(row) and row[x + n] == row[x] and n < 128:
            n += 1
        if n >= 3 or (row[x] == 0 and n >= 2):
            out += bytes((0x80 | (n - 1), row[x]))
            x += n
            continue
        start = x
        x += n
        while x < len(row) and x - start < 128:
            if x + 2 < len(row) and row[x] == row[x + 1] == row[x + 2]:
                break
            if row[x] == 0 and x + 1 < len(row) and row[x + 1] == 0:
                break
            x += 1
        lit = row[start:x]
        out.append(len(lit) - 1)
        if bits == 4:
            out += bytes((lit[k] << 4) | (lit[k + 1] if k + 1 < len(lit) else 0) for k in range(0, len(lit), 2))
        else:
            out += bytes(lit)
    return out


class Sheet:
    def __init__(self, name, colors=16):
        self.name, self.colors, self.frames = name, colors, []

    def add(self, im, ax=0, ay=0, trim=True):
        """A frame; (ax, ay) is the anchor inside it (where the game puts the thing's position)."""
        im = im.convert('RGBA')
        box = im.getchannel('A').point(lambda a: 255 if a >= 128 else 0).getbbox() if trim else (0, 0, im.width, im.height)
        if not box:
            box = (0, 0, 1, 1)
        self.frames.append((im.crop(box), box[0] - ax, box[1] - ay))
        return self


def encode(sheets, out, extra):
    lines = ['/* Generated by tools/art.py. The pictures are Halfbrick\'s (Jetpack Joyride): archive.org\'s Facebook',
             ' * assets, and the 1.3.5 Android package for the Lil\' Stomper, the Bad As Hog, the spin token, the HUD font and',
             ' * the Final Spin. See the README. */',
             '#ifndef JJ_ART_H', '#define JJ_ART_H', '#include <stdint.h>',
             'typedef struct { uint16_t w, h; int16_t x, y; uint32_t data; } jj_frame;',
             'typedef struct { const uint16_t *pal; const uint8_t *data; const jj_frame *f; uint8_t n, bits; } jj_sheet;']
    total = 0
    report = []
    for sh in sheets:
        # one palette for the sheet: quantize all frames together
        strip_w = sum(f[0].width for f in sh.frames)
        strip_h = max(f[0].height for f in sh.frames)
        strip = Image.new('RGBA', (strip_w, strip_h))
        x = 0
        for im, _, _ in sh.frames:
            strip.paste(im, (x, 0))
            x += im.width
        opaque = Image.new('RGB', strip.size, (0, 0, 0))
        opaque.paste(strip.convert('RGB'), mask=strip.getchannel('A').point(lambda a: 255 if a >= 128 else 0))
        pixels = [p for p, a in zip(strip.convert('RGB').getdata(), strip.getchannel('A').getdata()) if a >= 128]
        sample = Image.new('RGB', (max(1, len(pixels)), 1))
        sample.putdata(pixels or [(0, 0, 0)])
        q = sample.quantize(colors=sh.colors - 1, method=Image.Quantize.MEDIANCUT, dither=Image.Dither.NONE)
        pal = q.getpalette()[:3 * (sh.colors - 1)]
        ncol = len(set(q.getdata()))
        pimg = Image.new('P', (1, 1))
        pimg.putpalette(pal + pal[-3:] * (256 - len(pal) // 3))
        bits = 4 if sh.colors <= 16 else 8
        data = bytearray()
        frames = []
        for im, fx, fy in sh.frames:
            idx = im.convert('RGB').quantize(palette=pimg, dither=Image.Dither.NONE)
            ind = [v + 1 if a >= 128 else 0 for v, a in zip(idx.getdata(), im.getchannel('A').getdata())]
            start = len(data)
            offs, body = [], bytearray()
            for y in range(im.height):
                offs.append(len(body))
                body += runs(ind[y * im.width:(y + 1) * im.width], bits)
            offs.append(len(body))
            assert len(body) + 2 * len(offs) < 65536
            head = bytearray()
            for o in offs:
                head += struct.pack('<H', o + 2 * len(offs))
            data += head + body
            frames.append((im.width, im.height, fx, fy, start))
        palette = [0] + [rgb565(pal[i:i + 3]) for i in range(0, len(pal), 3)]
        n = sh.name
        lines.append(f'static const uint16_t pic_{n}_pal[] = {{' + ','.join(str(v) for v in palette) + '};')
        lines.append(f'static const uint8_t pic_{n}_data[] = {{')
        for k in range(0, len(data), 32):
            lines.append('  ' + ','.join(str(v) for v in data[k:k + 32]) + ',')
        lines.append('};')
        lines.append(f'static const jj_frame pic_{n}_f[] = {{' + ','.join('{%d,%d,%d,%d,%d}' % f for f in frames) + '};')
        lines.append(f'static const jj_sheet pic_{n} = {{pic_{n}_pal, pic_{n}_data, pic_{n}_f, {len(frames)}, {bits}}};')
        size = len(data) + 2 * len(palette) + 12 * len(frames)
        total += size
        report.append((n, len(frames), size))
    lines += extra
    lines.append('#endif')
    open(out, 'w').write('\n'.join(lines) + '\n')
    for n, k, s in report:
        print(f'  {n:12} {k:3} frames {s:7} bytes')
    print(f'{out}: {total} bytes')


def main():
    fb, apk, out = sys.argv[1], sys.argv[2], sys.argv[3]
    F = lambda k: Image.open(os.path.join(fb, FB_FILES[k])).convert('RGBA')
    T = lambda p: tex(os.path.join(apk, 'assets', 'textures', p))
    sheets = []

    # ---- the lab: rows 230..951 of the 2x picture are the screen's 0..240 (ceiling 30, floor 290 in points)
    labpic = F('lab')
    lab = labpic.crop((0, 230, 1024, 951))
    sheets.append(Sheet('lab', 64).add(scale(lab, 1 / 3), trim=False))

    # ---- the other rooms, made whole: what shows through them behind (the sea and its corals, the lava, the
    # forest under a misty sky; the game scrolls these slower, here they're drawn in), and the lab where a piece's
    # end is open (a whole lab picture comes before each room and after it). Cut in bands of at most 120 columns.
    def tile(bg, w, h, k, y0=0):
        bg = scale(bg, k)
        im = Image.new('RGBA', (w, h))
        for x in range(0, w, bg.width):
            for y in range(y0, h, bg.height):
                im.alpha_composite(bg, (x, y))
        return im

    def sea(w, h):
        b = tile(F('sea'), w, h, 2)
        for k, x0 in (('coral1', 0), ('coral2', 520)):
            for x in range(x0, w, 1040):
                b.alpha_composite(F(k), (x, h - 572))
        return b

    def forest(w, h):
        b = Image.new('RGBA', (w, h))
        for y in range(h):   # the mist between the trees ([F] the forest pictures' own colours)
            k = min(1, max(0, (y - 230) / 600))
            b.paste((round(90 - 55 * k), round(170 - 75 * k), round(195 - 83 * k), 255), (0, y, w, y + 1))
        b.alpha_composite(tile(F('trees_far'), w, h, 1, 380))
        b.alpha_composite(tile(F('trees'), w, h, 1))
        return b

    def lava(w, h):
        return tile(F('lava'), w, h, 2)

    def room(name, key, behind, lab_left=0, lab_right=None):
        p = F(key)
        w = p.width
        b = behind(w, 1024)
        if lab_left:
            b.paste(labpic.crop((0, 0, lab_left, 1024)), (0, 0))
        if lab_right is not None:
            b.paste(labpic.crop((1024 - (w - lab_right), 0, 1024, 1024)), (lab_right, 0))
        b.alpha_composite(p)
        im = scale(b.crop((0, 230, w, 951)), 1 / 3)
        sh = Sheet('rm_' + name, 96)
        for x in range(0, im.width, 120):
            sh.add(im.crop((x, 0, min(im.width, x + 120), im.height)), trim=False)
        sheets.append(sh)

    room('tube_in', 'tube_in', sea, lab_left=192)
    room('tube_out', 'tube_out', sea, lab_right=842)
    room('cave_in', 'cave_in', lava, lab_left=116)
    room('cave', 'cave', lava)
    room('cave_out', 'cave_out', lava, lab_right=968)
    room('green_in', 'green_in', forest, lab_left=192)
    room('green', 'green', forest)
    room('green_out', 'green_out', forest, lab_right=894)

    # ---- Barry, put together as clothing.xml does: the body 3 points under his position, the head 9 over it, the
    # machine gun jetpack (the APK's, barrels down) 12 behind it. A 56 x 64 canvas, his feet at (28, 58): the body's
    # cell at (16, 28), the head's at (16, 16), the jetpack's 32 x 64 cell at (4, 9)
    body, head = F('body'), F('head')
    jet = T('characters/jetpacks/jetpackdefault.tex')
    barry = Sheet('barry', 32)
    #   run 0-3, fly 4-7, fall 8-9 (with the jetpack); zapped 10-11; sliding on the floor 12
    for i, j in [(0, 0), (1, 1), (2, 2), (3, 3), (4, 4), (5, 5), (6, 6), (7, 7), (8, 8), (10, 10), (16, -1), (20, -1),
                 (30, -1)]:
        im = Image.new('RGBA', (56, 64))
        if j >= 0:
            im.alpha_composite(cell(jet, j, 32, 64, 4), (4, 9))
        im.alpha_composite(cell(body, i, 32, 32), (16, 28))
        im.alpha_composite(cell(head, i, 32, 32), (16, 16))
        barry.add(scale(im, S), 28 * S, 58 * S)
    sheets.append(barry)
    # knocked out, in the air: the four death bounce poses, turned in 8 steps (the game spins him: settings.xml
    # MIN/MAX_ROTATION_SPEED), anchored at his position, 16 points over his feet
    tumble = Sheet('tumble', 32)
    for i in [22, 23, 25, 29]:
        im = Image.new('RGBA', (64, 64))
        im.alpha_composite(cell(body, i, 32, 32), (16, 19))
        im.alpha_composite(cell(head, i, 32, 32), (16, 7))
        for a in range(8):
            tumble.add(scale(im.rotate(-45 * a, resample=Image.Resampling.BICUBIC, center=(32, 32)), S), 32 * S, 32 * S)
    sheets.append(tumble)
    # the jetpack's fire (APK textures made for 2x, so at half size): its flash under the barrels, its bullets
    # (effect_smgbullet) turned 0 to 40 degrees from straight down towards the back, the shells it throws out, and
    # where a bullet hits the floor (the Facebook assets' ricochet)
    fl = Sheet('jflash', 16)
    for i in range(4):
        fl.add(scale(cell(T('effect_jetpackflash.tex'), i, 32, 32), S / 2), 16 * S / 2, 4 * S / 2)
    sheets.append(fl)
    bul = T('effect_smgbullet.tex')
    sh = Sheet('bullet', 16)
    for a in range(5):
        r = bul.rotate(-90 - 10 * a, resample=Image.Resampling.BICUBIC, expand=True)
        sh.add(scale(r, S / 2), r.width * S / 4, r.height * S / 4)
    sheets.append(sh)
    shell = T('effect_rocketmgshell.tex')
    sh = Sheet('shell', 8)
    for a in range(4):
        r = shell.rotate(-45 * a, resample=Image.Resampling.BICUBIC, expand=True)
        sh.add(scale(r, S / 2), r.width * S / 4, r.height * S / 4)
    sheets.append(sh)
    rico = F('flash')
    sh = Sheet('spark', 16)
    for i in range(4):
        sh.add(scale(cell(rico, i, 64, 64), 1 / 3), 32 / 3, 58 / 3)
    sheets.append(sh)

    # ---- coins (2x), spin token (APK, 1x), vehicle box (2x)
    coinpic = F('coins')
    coin = Sheet('coin', 16)
    for i in range(8):
        coin.add(scale(cell(coinpic, i, 32, 32), 1 / 3), 16 / 3, 16 / 3, trim=False)
    sheets.append(coin)
    tok = T('entities/slottoken.tex')
    token = Sheet('token', 16)
    for i in range(8):
        token.add(scale(cell(tok, i, 32, 32), S), 16 * S, 16 * S, trim=False)
    sheets.append(token)
    bx = F('box')
    box = Sheet('box', 32)
    for i in range(0, 8, 2):
        box.add(scale(cell(bx, i, 128, 128), 1 / 3), 64 / 3, 64 / 3, trim=False)
    sheets.append(box)

    # ---- zapper orbs (2x), missiles (1x), warnings (2x), laser emitters (4x), explosions (2x)
    for name, key in (('orb', 'orb'), ('orbr', 'orb_rot')):
        o = F(key)
        sh = Sheet(name, 16)
        for i in range(4):
            sh.add(scale(cell(o, i, 62, 42), 1 / 3), 31 / 3, 21 / 3, trim=False)
        sheets.append(sh)
    mis = F('missile')
    sh = Sheet('missile', 16)
    for i in range(4):
        sh.add(scale(cell(mis, i, 32, 32), S), 16 * S, 16 * S)
    fire = F('fire')
    for i in range(2):
        sh.add(scale(cell(fire, i, 16, 32, 4).rotate(90, expand=True), S), 0, 8 * S)
    sheets.append(sh)
    w = F('warning')
    sh = Sheet('warn', 16)
    for i in [0, 1, 2, 3]:
        sh.add(scale(cell(w, i, 64, 64), 1 / 2), 16, 16)
    sheets.append(sh)
    lz = F('laser')
    sh = Sheet('emitter', 16)
    for i in [0, 1, 2, 3]:
        sh.add(scale(cell(lz, i, 128, 128), 1 / 6), 64 / 6, 64 / 6, trim=False)
    sheets.append(sh)
    expl = F('boom')
    sh = Sheet('boom', 16)
    for i in [4, 0, 13, 14, 12, 2]:
        sh.add(scale(cell(expl, i, 128, 128), 1 / 3), 64 / 3, 64 / 3)
    sheets.append(sh)

    # ---- vehicles. Barry's head goes on the Stomper, the Hog and the Bird where the game's head data puts it
    def with_head(im, cx, cy, frame, hx, hy):
        im = im.copy()
        h = cell(head, frame - 1, 32, 32)
        im.alpha_composite(h, (round(cx + hx - 16), round(cy + hy - 16)))
        return im

    mech = T('characters/vehicles/vehiclemech.tex')
    mech_head = {1: (14, -8), 2: (16, -3), 3: (15, -6), 4: (14, -7), 5: (16, -3), 6: (15, -5), 8: (15, 10), 10: (12, 10),
                 12: (11, 7), 13: (11, 7), 14: (11, 7), 15: (10, 7), 17: (9, -16)}
    sh = Sheet('mech', 24)
    for fr in [1, 2, 4, 5, 8, 12, 13, 14, 15]:
        c = cell(mech, fr - 1, 85, 85, 6)
        hx, hy = mech_head[fr]
        c = with_head(c, 42, 42, 13 if fr == 17 else 5, hx, hy)
        sh.add(scale(c, S), 42 * S, 80 * S)
    sheets.append(sh)
    hog = T('characters/vehicles/vehiclehog.tex')
    hog_head = {1: (-11, -7, 5), 2: (-11, -7, 6), 3: (-11, -7, 7), 4: (-11, -7, 8), 5: (-16, 0, 11), 7: (-17, 1, 11)}
    sh = Sheet('hog', 32)
    for fr in [1, 2, 3, 4, 5, 7]:
        c = cell(hog, fr - 1, 64, 64, 4)
        hx, hy, hf = hog_head[fr]
        c = with_head(c, 32, 32, hf, hx, hy)
        sh.add(scale(c, S), 32 * S, 60 * S)
    sheets.append(sh)
    bird = F('bird')
    sh = Sheet('bird', 32)
    for i in [0, 6, 2, 1, 7]:
        c = Image.new('RGBA', (96, 72))
        c.alpha_composite(cell(bird, i, 78, 54, 3), (8, 14))
        c = with_head(c, 48, 40, 5, 4, -14)
        sh.add(scale(c, S), 48 * S, 66 * S)
    sheets.append(sh)
    tp = F('cft')
    sh = Sheet('cft', 32)
    for i in [0, 1, 4, 5, 2, 6, 3, 7]:
        sh.add(scale(cell(tp, i, 64, 64, 4), S), 32 * S, 32 * S)
    sh.add(scale(T('entities/teleportarrow.tex'), S), 8 * S, 8 * S)
    sheets.append(sh)
    gs = F('grav')
    sh = Sheet('grav', 32)
    for i in [0, 1, 3, 4, 2, 8, 9]:
        sh.add(scale(cell(gs, i, 42, 42, 6), S), 21 * S, 40 * S)
    sheets.append(sh)

    # ---- the scientists, in their hazmat suits: a body and a helmet each (the cells line up, the helmet 13 points
    # up). Walking 0-3, running 4-7, the death bounces 8-11, sliding dead on the floor 12-13, curled up 14, zapped
    # 15-16 (the frames of the game's worker sprite files)
    for name, bk, hk in (('sci1', 'worker1', 'worker1_head'), ('sci2', 'worker2', 'worker2_head')):
        wb, wh = F(bk), F(hk)
        sh = Sheet(name, 16)
        for i in [0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 14, 16, 18, 19]:
            im = Image.new('RGBA', (40, 56))
            im.alpha_composite(cell(wb, i, 32, 32), (4, 22))
            im.alpha_composite(cell(wh, i, 32, 32), (4, 9))
            sh.add(scale(im, S), 20 * S, 51 * S)
        sheets.append(sh)

    # ---- the title
    sheets.append(Sheet('wjet', 24).add(scale(F('jetpack'), 0.82), trim=False))
    sheets.append(Sheet('wjoy', 24).add(scale(F('joyride'), 0.82), trim=False))
    hero = F('hero')
    hero = hero.crop(hero.getbbox())
    sheets.append(Sheet('hero', 32).add(scale(hero, 0.25)))

    # ---- the HUD's numbers: New Athletic M54 (the game's vehicle_font), 0-9 and M, at half size
    fnt = open(os.path.join(apk, 'assets', 'fonts', 'vehicle_font.fnt'), encoding='latin1').read().split('\n')
    page = tex(os.path.join(apk, 'assets', 'fonts', 'vehicle_font.tex'))
    glyphs = {}
    for l in fnt:
        if l.startswith('char id='):
            kv = dict(x.split('=') for x in l.split() if '=' in x)
            glyphs[int(kv['id'])] = {k: int(v) for k, v in kv.items() if k != 'id' and v.lstrip('-').isdigit()}
    sh = Sheet('digits', 8)
    adv = []
    for ch in '0123456789M':
        g = glyphs[ord(ch)]
        im = page.crop((g['x'], g['y'], g['x'] + g['width'], g['y'] + g['height']))
        k = 0.5
        sh.add(scale(im, k), -g['xoffset'] * k, -g['yoffset'] * k)
        adv.append(round(g['xadvance'] * k))
    sheets.append(sh)

    # ---- the Final Spin: its window and prizes (APK, 2x)
    sh = Sheet('prize', 32)
    for p in ['slotcoins100', 'slotcoins500', 'slotcoins1000', 'slotexplosion', 'slotexplosion2', 'slotexplosion3',
              'slotrevive', 'slottokens', 'slotdoublecoins', 'slotstart500m']:
        sh.add(scale(T('menus/slotmachine/slotimages/' + p + '.tex'), 1 / 3), 64 / 3, 64 / 3, trim=False)
    sheets.append(sh)
    sheets.append(Sheet('stoken', 16).add(scale(T('menus/slotmachine/slottoken.tex'), 1 / 3)))

    encode(sheets, out, ['/* the HUD digits\' advances */', 'static const uint8_t pic_digit_adv[] = {' + ','.join(map(str, adv)) + '};'])


if __name__ == '__main__':
    main()
