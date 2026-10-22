"""src/art.c and src/art.h: Monkey Mart's pictures, from the web game's atlases (cut in WEB/sprites), its monkeys
and animals (baked from its Spine skeleton in WEB/baked), its floors and its font (Riffic, in WEB/font).

Called by tools/data.py, which knows the pictures the marts use. Pictures are 4 bits a pixel with their own 15
colours (0 is see-through), at a third of the world's scale (the screen's), cut to what they show; the floors are
runs of 16 colours. Pillow does the scaling and the colours."""
import json
import re
from pathlib import Path
from PIL import Image

# pictures the game draws besides the marts' (draw.c): "atlas:name[~][@angle][/percent]", 100% = the world's 1/3
UI = [
    'ui_game:cloud1', 'ui_game:cloud2', 'ui_game:cloud5', 'ui_game:aim1', 'ui_game:cash', 'ui_game:terminal/86',
    'ui_game:smile/86', 'ui_game:smile_sleepy/86', 'ui_game:smile_crazy/86', 'ui_game:smile_exit/86',
    'ui_game:smile_repair/86', 'ui_game:icon_cashier2', 'ui_game:icon_assistant2', 'ui_game:icon_farmer2',
    'ui_game:icon_chef2', 'ui_game:icon_manager2', 'ui_game:icon_next_mart', 'ui_game:tutorial_arrow',
    'ui_game:ui_cash', 'ui_game:icon_attention', 'game:cash', 'game:cash_list', 'game:aim1',
    'ui_game:icon_assistant2/90', 'ui_game:icon_farmer2/90', 'ui_game:icon_chef2/90', 'ui_game:cloud2/55',
    'ui_main:icon_max/60', 'ui_main:cash/60', 'ui_main:icon_marts/100', 'ui_main:icon_upgrades/80',
    'ui_main:icon_shop/80', 'ui_game:tutorial_keys/60', 'ui_main:icon_player/75',
] + ['ui_main:hat%d' % h for h in [0] + list(range(11, 31))]
# effects: (atlas name, keep every n-th frame)
EFFECTS = [('ui_game:effect_exit', 2), ('ui_game:effect_sleep', 6), ('ui_game:effect_alarm', 4),
           ('ui_game:effect_repair', 4), ('ui_game:effect_broom', 2), ('ui_game:effect_broken', 5),
           ('game:effect_dust', 4)]


def rgb565(c):
    return ((c[0] >> 3) << 11) | ((c[1] >> 2) << 5) | (c[2] >> 3)


def symbol(name):
    name = name.replace('~', '_flip').replace('@-', '_turn_minus').replace('@', '_turn').replace('/', '_size')
    return 'P_' + re.sub(r'[^A-Za-z0-9]+', '_', name).upper().strip('_')


class Packer:
    def __init__(self):
        self.data = bytearray()
        self.pals = []
        self.pal_index = {}
        self.pics = []          # (symbol, ox, oy, w, h, pal, offset)
        self.seen = {}

    def palette(self, cols):
        key = tuple(cols)
        if key not in self.pal_index:
            self.pal_index[key] = len(self.pals)
            self.pals.append(key)
        return self.pal_index[key]

    def add(self, sym, im, ax, ay, colors=15):
        """im: RGBA; (ax, ay): the anchor (where the picture's position is) in im's pixels"""
        im = im.convert('RGBA')
        a = im.getchannel('A').point(lambda v: 255 if v >= 110 else 0)
        box = a.getbbox()
        if not box:
            box = (0, 0, 1, 1)
        im, a = im.crop(box), a.crop(box)
        rgb = Image.new('RGB', im.size, (0, 0, 0))
        rgb.paste(im.convert('RGB'), (0, 0), a)
        opaque = [p for p, v in zip(rgb.getdata(), a.getdata()) if v]
        if not opaque:
            opaque = [(0, 0, 0)]
        sample = Image.new('RGB', (len(opaque), 1))
        sample.putdata(opaque)
        q = sample.quantize(colors=colors, method=Image.Quantize.MEDIANCUT, dither=Image.Dither.NONE)
        pal = q.getpalette()[:colors * 3]
        pal += pal[:3] * (colors - len(pal) // 3)
        qi = rgb.quantize(palette=q, dither=Image.Dither.NONE)
        idx = [(v + 1) if t else 0 for v, t in zip(qi.getdata(), a.getdata())]
        cols = [0] + [rgb565(pal[i * 3:i * 3 + 3]) for i in range(colors)]
        cols += [0] * (16 - len(cols))
        w, h = im.size
        packed = bytes((idx[i] << 4) | (idx[i + 1] if i + 1 < len(idx) else 0) for i in range(0, len(idx), 2)) \
            if w % 2 == 0 else self.rows_packed(idx, w, h)
        key = (w, h, packed, tuple(cols))
        if key in self.seen:
            off, pi = self.seen[key]
        else:
            off, pi = len(self.data), self.palette(cols)
            self.data += packed
            self.seen[key] = (off, pi)
        self.pics.append((sym, box[0] - ax, box[1] - ay, w, h, pi, off))

    @staticmethod
    def rows_packed(idx, w, h):
        """odd widths: each row starts on a byte"""
        out = bytearray()
        for y in range(h):
            row = idx[y * w:(y + 1) * w] + [0]
            out += bytes((row[i] << 4) | row[i + 1] for i in range(0, w, 2))
        return bytes(out)


def load(web, spec):
    """a picture spec -> (RGBA image at the screen's scale, anchor x, y)"""
    m = re.match(r'(\w+):(\w+?)(~?)(?:@(-?\d+))?(?:/(\d+))?$', spec)
    atlas, name, flip, angle, pct = m.groups()
    p = web / 'sprites' / ('%s__%s.png' % (atlas, name))
    if not p.exists():
        p = web / 'sprites' / ('%s__%s_0.png' % (atlas, name))
    im = Image.open(p).convert('RGBA')
    if flip:
        im = im.transpose(Image.Transpose.FLIP_LEFT_RIGHT)
    if angle:
        im = im.rotate(int(angle), resample=Image.Resampling.BICUBIC, expand=True)
    scale = (int(pct) if pct else 100) / 300.0
    if atlas.startswith('background'):
        scale *= 2          # the floors are drawn at twice their size in the world
    w, h = max(1, round(im.width * scale)), max(1, round(im.height * scale))
    im = im.resize((w, h), Image.Resampling.LANCZOS)
    return im, w // 2, h // 2


def floor(web, name, scale):
    im = Image.open(web / 'sprites' / name).convert('RGB')
    im = im.resize((round(im.width * scale), round(im.height * scale)), Image.Resampling.LANCZOS)
    q = im.quantize(colors=16, method=Image.Quantize.MEDIANCUT, dither=Image.Dither.NONE)
    pal = q.getpalette()[:48]
    pal += pal[:3] * (16 - len(pal) // 3)
    px = q.load()
    W, H = q.size
    data, rows = bytearray(), []
    for y in range(H):
        rows.append(len(data))
        x = 0
        while x < W:
            e = x + 1
            while e < W and px[e, y] == px[x, y] and e - x < 128:
                e += 1
            if e - x >= 3:                      # a run: 128 + length - 1, colour
                data += bytes([127 + e - x, px[x, y]])
                x = e
                continue
            s = x                               # literals: length - 1, then two a byte
            while x < W and x - s < 128:
                if x + 2 < W and px[x, y] == px[x + 1, y] == px[x + 2, y]:
                    break
                x += 1
            vals = [px[i, y] for i in range(s, x)]
            data.append(len(vals) - 1)
            data += bytes((vals[i] << 4) | (vals[i + 1] if i + 1 < len(vals) else 0) for i in range(0, len(vals), 2))
    return W, H, [rgb565(pal[i * 3:i * 3 + 3]) for i in range(16)], data, rows


def font(web, cap):
    """Riffic's letters (the game's font: its glyph bank's face channel), scaled to a capital `cap` pixels high;
    4-bit coverage"""
    info = json.loads((web / 'font' / 'font.json').read_text())
    scale = cap / 17.0                      # 'B' and 'X' stand 17 pixels in the bank
    glyphs, data = [], bytearray()
    for c in range(32, 127):
        g = info['glyphs'].get(str(c))
        if not g or 'w' not in g:
            adv = (g or {}).get('advance', 6)
            glyphs.append((len(data), 0, 0, 0, 0, max(1, round(adv * scale))))
            continue
        face = Image.open(web / 'font' / ('g%03d.png' % c)).getchannel('B')   # the third channel: the letter
        w, h = max(1, round(face.width * scale)), max(1, round(face.height * scale))
        sm = face.resize((w, h), Image.Resampling.LANCZOS)
        box = sm.getbbox()
        top = round((17 - g['ascent'] - 1) * scale)     # the glyph's top below the capital line
        if not box:
            glyphs.append((len(data), 0, 0, 0, 0, max(1, round(g['advance'] * scale))))
            continue
        sm = sm.crop(box)
        px = [min(15, (v + 8) // 17) for v in sm.getdata()]
        off = len(data)
        data += bytes((px[i] << 4) | (px[i + 1] if i + 1 < len(px) else 0) for i in range(0, len(px), 2))
        glyphs.append((off, sm.width, sm.height, box[0] + round(g['bearing'] * scale), top + box[1],
                       max(1, round(g['advance'] * scale))))
    return glyphs, data


def write(web, root, pictures):
    web = Path(web)
    P = Packer()
    out_h = ['/* Monkey Mart\'s pictures and font (art.c, made by tools/art.py with tools/data.py). */',
             '#ifndef MM_ART_H', '#define MM_ART_H', '#include <stdint.h>', '',
             '/* a picture: its top left from where it stands, its size, its 16 colours, its pixels (4 bits each, 0 see-through;',
             ' * a row of an odd width starts on a byte) */',
             'typedef struct { int16_t ox, oy; uint16_t w, h; uint16_t pal; uint32_t px; } mm_pic;', '']
    world = [p for p in pictures if not p.startswith('background')]
    for spec in world + [u for u in UI if u not in world]:
        im, ax, ay = load(web, spec)
        P.add(symbol(spec), im, ax, ay)
    # effects
    effects = []
    for spec, step in EFFECTS:
        atlas, name = spec.split(':')
        info = json.loads((web / 'sprites' / (atlas + '.json')).read_text())[name]
        first = len(P.pics)
        frames = info['frames'][::step]
        for k, fr in enumerate(frames):
            im = Image.open(web / 'sprites' / fr['file']).convert('RGBA')
            w, h = max(1, round(im.width / 3)), max(1, round(im.height / 3))
            im = im.resize((w, h), Image.Resampling.LANCZOS)
            P.add('%s_%d' % (symbol(spec), k), im, w // 2, h // 2)
        fps = info['fps'] or 30
        effects.append((symbol(spec)[2:], first, len(frames), max(1, round(60 * step / fps))))
    # monkeys and animals
    A = json.loads((web / 'baked' / 'actors.json').read_text())
    anims, anchors = [], []
    for key, an in A['frames'].items():
        first = len(P.pics)
        for k, fr in enumerate(an['frames']):
            im = Image.open(web / 'baked' / fr['file']).convert('RGBA')
            P.add('P_A_%s_%d' % (key.upper(), k), im, -fr['x'], -fr['y'])
            anchors.append((fr['hat'], fr['food']))
        n = len(an['frames'])
        anims.append((key.upper(), first, n, max(1, round(an['length'] * 60 / n))))
    hats = {}
    for hname, off in A['hats'].items():
        im = Image.open(web / 'baked' / ('hat%s.png' % hname)).convert('RGBA')
        hats[int(hname)] = len(P.pics)
        P.add('P_HAT%s' % hname, im, -off[0], -off[1])
    foods = {}
    for fname, off in A['foods'].items():
        im = Image.open(web / 'baked' / ('food%s.png' % fname)).convert('RGBA')
        foods[int(fname)] = len(P.pics)
        P.add('P_CARRY%s' % fname, im, -off[0], -off[1])
    # the title's logo, from the loading picture
    logo = Image.open(web / 'web21' / 'bg_loading.png').convert('RGBA').crop((385, 30, 735, 205))
    # the sky and clouds around the sign go: what's light blue or white, reached from the edges
    px = logo.load()
    W, H = logo.size
    sky = lambda c: (c[2] > 150 and c[2] > c[0] + 20) or min(c[:3]) > 225
    todo = [(x, y) for x in range(W) for y in (0, H - 1)] + [(x, y) for y in range(H) for x in (0, W - 1)]
    seen = set()
    while todo:
        x, y = todo.pop()
        if (x, y) in seen or not (0 <= x < W and 0 <= y < H) or not sky(px[x, y]):
            continue
        seen.add((x, y))
        px[x, y] = (0, 0, 0, 0)
        todo += [(x + 1, y), (x - 1, y), (x, y + 1), (x, y - 1)]
    logo = logo.resize((round(logo.width * 0.5), round(logo.height * 0.5)), Image.Resampling.LANCZOS)
    P.add('P_LOGO', logo, logo.width // 2, logo.height // 2)

    syms = [p[0] if p[0].startswith('P_') else p[0] for p in P.pics]
    out_h.append('enum {')
    out_h.append('  P_NONE,')
    for s in syms:
        out_h.append('  %s,' % (s if s.startswith('P_') else 'P_' + s))
    out_h.append('  P_COUNT')
    out_h.append('};')
    out_h.append('')
    out_h.append('/* an animation: its first picture, frames, ticks a frame (60 a second) */')
    out_h.append('typedef struct { uint16_t first; uint8_t n, ticks; } mm_anim;')
    out_h.append('enum {')
    for name, first, n, t in anims:
        out_h.append('  AN_%s,' % name)
    out_h.append('  AN_COUNT')
    out_h.append('};')
    out_h.append('enum {')
    for name, first, n, t in effects:
        out_h.append('  FX_%s,' % name)
    out_h.append('  FX_COUNT')
    out_h.append('};')
    out_h.append('/* where an actor\'s hat and carried food go, a frame (from where it stands) */')
    out_h.append('typedef struct { int8_t hx, hy, fx[12], fy[12]; } mm_anchor;')
    out_h.append('typedef struct { uint16_t w, h; const uint16_t *pal; const uint8_t *data; const uint32_t *rows; } mm_floor;')
    out_h.append('typedef struct { uint16_t off; uint8_t w, h; int8_t x, y; uint8_t adv; } mm_glyph;')
    out_h.append('typedef struct { const mm_glyph *g; const uint8_t *px; uint8_t cap, line; } mm_font;')
    out_h.append('')
    out_h.append('extern const mm_pic mm_pics[P_COUNT];')
    out_h.append('extern const uint16_t mm_pals[][16];')
    out_h.append('extern const uint8_t mm_pixels[];')
    out_h.append('extern const mm_anim mm_anims[AN_COUNT], mm_effects[FX_COUNT];')
    out_h.append('extern const mm_anchor mm_anchors[];')
    out_h.append('extern const uint16_t mm_hat_pic[31], mm_carry_pic[15];')
    out_h.append('extern const mm_floor mm_floors[2];')
    out_h.append('extern const mm_font mm_font_small, mm_font_big;')
    out_h.append('#endif')

    c = ['/* Made by tools/art.py (see src/art.h) from the web game\'s pictures. */', '#include "art.h"', '']

    def arr(typ, name, vals, per=24):
        c.append('%s %s[] = {' % (typ, name))
        for i in range(0, len(vals), per):
            c.append(','.join(str(v) for v in vals[i:i + per]) + ',')
        c.append('};')
    arr('const uint8_t', 'mm_pixels', list(P.data), 40)
    c.append('const uint16_t mm_pals[][16] = {')
    for pal in P.pals:
        c.append('{%s},' % ','.join(str(v) for v in pal))
    c.append('};')
    c.append('const mm_pic mm_pics[P_COUNT] = {{0, 0, 0, 0, 0, 0},')
    for sym, ox, oy, w, h, pi, off in P.pics:
        c.append('{%d,%d,%d,%d,%d,%d},' % (ox, oy, w, h, pi, off))
    c.append('};')
    c.append('const mm_anim mm_anims[AN_COUNT] = {%s};' % ','.join('{%d,%d,%d}' % (f + 1, n, t) for _, f, n, t in anims))
    c.append('const mm_anim mm_effects[FX_COUNT] = {%s};' % ','.join('{%d,%d,%d}' % (f + 1, n, t) for _, f, n, t in effects))
    rows = []
    for (hx, hy), food in anchors:
        rows.append('{%d,%d,{%s},{%s}}' % (hx, hy, ','.join(str(f[0]) for f in food), ','.join(str(f[1]) for f in food)))
    c.append('const mm_anchor mm_anchors[] = {%s};' % ','.join(rows))
    c.append('const uint16_t mm_hat_pic[31] = {%s};' % ','.join(str(hats[i] + 1) if i in hats else '0' for i in range(31)))
    c.append('const uint16_t mm_carry_pic[15] = {%s};' % ','.join(str(foods[i] + 1) if i in foods else '0' for i in range(15)))
    fl = []
    for n, name in enumerate(['background__bg_mart_1.png', 'background2__bg_mart_2.png']):
        W, H, pal, data, rws = floor(web, name, 2 / 3)
        arr('static const uint8_t', 'floor%d_px' % n, list(data), 40)
        arr('static const uint32_t', 'floor%d_rows' % n, rws, 16)
        c.append('static const uint16_t floor%d_pal[16] = {%s};' % (n, ','.join(map(str, pal))))
        fl.append('{%d,%d,floor%d_pal,floor%d_px,floor%d_rows}' % (W, H, n, n, n))
    c.append('const mm_floor mm_floors[2] = {%s};' % ','.join(fl))
    for fname, cap, line in (('small', 8, 12), ('big', 12, 17)):
        glyphs, data = font(web, cap)
        arr('static const uint8_t', 'font_%s_px' % fname, list(data), 40)
        c.append('static const mm_glyph font_%s_g[95] = {%s};' % (fname, ','.join('{%d,%d,%d,%d,%d,%d}' % g for g in glyphs)))
        c.append('const mm_font mm_font_%s = {font_%s_g, font_%s_px, %d, %d};' % (fname, fname, fname, cap, line))
    (root / 'src' / 'art.h').write_text('\n'.join(out_h) + '\n')
    (root / 'src' / 'art.c').write_text('\n'.join(c) + '\n')
    print('art: %d pictures, %d bytes of pixels, %d palettes' % (len(P.pics), len(P.data), len(P.pals)))
