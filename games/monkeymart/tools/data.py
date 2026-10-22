#!/usr/bin/env python3
"""src/data.c: Monkey Mart's two marts, from the web game (version 2.1) as it was taken apart in WEB.

    python3 -I tools/data.py WEB

WEB holds the game's resources, decoded (WEB/decoded: collections and game objects) and its Lua sources as text
(WEB/lua). Nothing in WEB is run: the Lua is read (tools/lua.py) and the few functions that hold data are read
statement by statement. The positions are the game's world units (y up); see src/data.h for the fields.

    python3 -I tools/data.py WEB --check

makes them again elsewhere and fails if the four files differ from src/ (make test ORIGINAL=WEB); --dump prints
the tables read, as JSON (for the spec)."""
import json
import re
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import lua  # noqa: E402

ROOT = Path(__file__).resolve().parents[1]
SRC = ROOT
WEB = Path(sys.argv[1]).expanduser() if len(sys.argv) > 1 else None

FOODS = ['', 'banan', 'corn', 'egg', 'milk', 'popcorn', 'yogurt', 'cocoa', 'coffeebean', 'peanut', 'chocolate',
         'coffeeground', 'peanutbutter', 'cappuccino', 'chocolatebar']


# ---------------------------------------------------------------- Defold protobufs (collections, game objects)
def varint(b, i):
    r = s = 0
    while True:
        c = b[i]
        i += 1
        r |= (c & 0x7f) << s
        s += 7
        if not c & 0x80:
            return r, i


def pb(buf):
    i = 0
    while i < len(buf):
        key, i = varint(buf, i)
        f, wt = key >> 3, key & 7
        if wt == 0:
            v, i = varint(buf, i)
        elif wt == 1:
            v, i = buf[i:i + 8], i + 8
        elif wt == 2:
            n, i = varint(buf, i)
            v, i = buf[i:i + n], i + n
        elif wt == 5:
            v, i = buf[i:i + 4], i + 4
        else:
            raise ValueError('wire type %d' % wt)
        yield f, wt, v


def vec(buf):
    v = {1: 0.0, 2: 0.0, 3: 0.0, 4: 0.0}
    for f, w, x in pb(buf):
        if w == 5:
            v[f] = struct.unpack('<f', x)[0]
    return v


def collection(path):
    """instance id -> (local position, children, prototype, rotation quaternion)"""
    out = {}
    for f, w, v in pb(path.read_bytes()):
        if f != 2:
            continue
        o = {'children': []}
        for f2, w2, v2 in pb(v):
            if f2 == 1:
                o['id'] = v2.decode()
            elif f2 == 2:
                o['proto'] = v2.decode()
            elif f2 == 3:
                o['children'].append(v2.decode())
            elif f2 == 4:
                p = vec(v2)
                o['pos'] = (round(p[1]), round(p[2]))
            elif f2 == 5:
                q = vec(v2)
                o['rot'] = (q[1], q[2], q[3], q[4])
        out[o['id']] = o
    return out


def sprites_of(proto):
    """a game object's sprite components: id -> (default animation, local offset)"""
    out = {}
    for f, w, v in pb((WEB / 'decoded' / proto.lstrip('/')).read_bytes()):
        if f != 1:
            continue
        c = {f2: v2 for f2, w2, v2 in pb(v)}
        res = c[2].decode()
        if res.endswith('.spritec'):
            s = {f2: v2 for f2, w2, v2 in pb((WEB / 'decoded' / res.lstrip('/')).read_bytes())}
            p = vec(c[3]) if 3 in c else {1: 0, 2: 0}
            out[c[1].decode()] = (s[2].decode(), (round(p[1]), round(p[2])))
    return out


# ---------------------------------------------------------------- the Lua sources
def lua_src(name):
    return (WEB / 'lua' / name).read_text(encoding='utf-8', errors='replace').replace('\r', '')


GS = None   # GameState.lua
GLOBAL = None


def gamestate_env(name):
    """the GameState fields the mart tables refer to"""
    known = {'gameState.radiusSklad': 18000, 'gameState.defaultStamina': 8, 'gameState.buildTimePlants': 5,
             'gameState.buildTimeZavods': 12}
    return known.get(name, name)


def mart_tables(n):
    src = lua_src('main/gamecore/DataMart%d.luac.lua' % n)
    boxes = lua.value_after(src, 'this.listPlaceBoxes', gamestate_env)
    get = lambda k: lua.value_after(src, 'this.' + k, gamestate_env)
    m = {'src': src, 'boxes': boxes}
    for k in ('posDodik', 'limitMoveX', 'limitMoveY', 'posExit', 'posStart', 'posGateExit', 'posManager', 'posTrash',
              'posBasket', 'placesCamera', 'aimPlaces', 'pricePlaces', 'map_width', 'map_height', 'world',
              'NUM_SALE_CRAZY'):
        m[k] = get(k)
    m['listSalesMain'] = get('listSalesMain')
    for b in boxes:
        b.setdefault('offsetY', 0)
    m['index'] = {b['link']: i for i, b in enumerate(boxes)}    # 0-based
    return m


def place_id(m, s):
    """'this.PLACES["x"].id' or a number (1-based in Lua) -> our 0-based index"""
    s = s.strip()
    mm = re.match(r'this\.PLACES\["(\w+)"\]\.id', s)
    if mm:
        return m['index'][mm.group(1)]
    return int(s) - 1


ID = r'(?:this\.PLACES\["\w+"\]\.id|\d+)'


def blocks(body, key):
    """`if/elseif <key> == X then ... ` pieces: (X, text)"""
    parts = re.split(r'(?:else)?if\s*\(?\s*' + re.escape(key) + r'\s*==\s*', body)
    out = []
    for p in parts[1:]:
        mm = re.match(r"\s*(" + ID + r"|'[^']*')\s*\)?\s*then", p)
        out.append((mm.group(1), p[mm.end():]))
    return out


# ---------------------------------------------------------------- places
def places(m, col):
    out = []
    for i, b in enumerate(m['boxes']):
        inst = col['/' + b['link']]
        pos = inst['pos']
        spr = sprites_of(inst['proto'])
        poly = []
        if b.get('isPhysics'):
            for c in ('_pA', '_pB', '_pC', '_pD'):
                q = col['/' + b['link'] + c]['pos']
                poly.append((pos[0] + q[0], pos[1] + q[1]))
        price = m['pricePlaces'][i] if i < len(m['pricePlaces']) else 0
        ow, ob = b.get('offsetWork', {'x': 0, 'y': 0}), b.get('offsetBuy', {'x': 0, 'y': 0})
        og = b.get('offsetGraphic', {'x': 0, 'y': 0})
        p = {
            'link': b['link'], 'pos': pos, 'work': (pos[0] + ow['x'], pos[1] + ow['y']),
            'buy': (pos[0] + ob['x'], pos[1] + ob['y']), 'zy': pos[1] - b['offsetY'],
            'radius': b.get('radius', 0), 'price': price, 'food': b.get('typeFood', 0),
            'count0': b.get('countFood', 0), 'cap0': b.get('maxCount', 0),
            'build': b.get('zavodBuild', 0) if b.get('isZavod') else 0,
            'stamina': b.get('stamina_max', 0) if b.get('isZavod') else 0,
            'zavod': bool(b.get('isZavod')), 'sklad': bool(b.get('isSklad')), 'buyer': bool(b.get('isForBuyer')),
            'solid': bool(b.get('isPhysics')), 'hide': bool(b.get('isHideSprite')), 'poly': poly,
            'pic': spr.get('sprite', ('', (0, 0)))[0], 'pic2': spr['sprite2'][0] if b.get('isSprite2') else '',
            'pos2': (pos[0] - og['x'], pos[1] - og['y']), 'init': b.get('callInit', ''),
            'feed': None, 'feed2': None, 'offsetSklad': b.get('offsetSklad', {'x': 0, 'y': 0}),
            'offsetPlaha': b.get('offsetPlaha', {'x': 0, 'y': 0}), 'offsetBoost': b.get('offsetBoost', {'x': 0, 'y': 0}),
        }
        if b.get('linkSklad'):
            p['feed'] = b['linkSklad'] - 1
        out.append(p)
    return out


# ---------------------------------------------------------------- shelves: where their food shows, where buyers stand
def sklad_views(init, x, y, food, offset_sklad):
    """GameState:initSklad*(vX, vY, vType, vList, vPlaces): the food positions (x, y, depth, flipped) and the
    buyers' spots, from the function's own arithmetic"""
    name = init.split('.')[-1]
    body = lua.function_body(GS, name)
    env = {'vX': x, 'vY': y, 'vType': food, 'CELL_SIZE': 80, 'vOffsetSklad.x': offset_sklad['x'],
           'vOffsetSklad.y': offset_sklad['y']}

    def posz(arg):
        return 'Z:' + str(lua.Parser(lua.tokens(arg), env).expr())

    flip = 'flipX' in body
    if name == 'initViewSkladFood':
        # the generic one: its per-type settings
        settings = re.search(r'\(vType == %d\) then(.*?)(?:elseif|end)' % food, body, re.S)
        flip = False
        text = settings.group(1) if settings else ''
        defaults = re.search(r'local posX = vX(.*?)if \(vType', body, re.S).group(0)
        for src in (defaults, text):
            for k, v in re.findall(r'(?:local\s+)?(\w+)\s*=\s*([^\n]+)', src):
                v = v.strip()
                if k == 'isFlip':
                    flip = v == 'true'
                elif k == 'posZ':
                    env[k] = lua.Parser(lua.tokens(re.search(r'getPosZ\((.*)\)', v).group(1)), env).expr()
                elif k not in ('if',):
                    try:
                        env[k] = lua.Parser(lua.tokens(v), env).expr()
                    except Exception:
                        pass
    else:
        for k, v in re.findall(r'local\s+(\w+)\s*=\s*([^\n]+)', body.split('local arPos')[0]):
            if k.startswith('posZ'):
                env[k] = lua.Parser(lua.tokens(re.search(r'getPosZ\((.*)\)', v).group(1)), env).expr()
            else:
                env[k] = lua.Parser(lua.tokens(v), env).expr()
    ar = body[body.index('local arPos'):]
    ar = ar[:ar.index('local boxed_food') if 'local boxed_food' in ar else ar.index('for k,v')]
    items = re.findall(r'\{\s*x\s*=\s*([^,]+),\s*y\s*=\s*([^,]+),\s*z\s*=\s*([^}]+)\}', ar)
    out = []
    for k, (ex, ey, ez) in enumerate(items, 1):
        fx = lua.Parser(lua.tokens(ex), env).expr()
        fy = lua.Parser(lua.tokens(ey), env).expr()
        # depth: the item's z is getPosZ(base) + its own z (+ k steps); as a y: base - z * 10000
        zt = ez.strip()
        zm = re.match(r'(posZ\d?)\s*\+\s*([\d.]+)', zt)
        if zm:
            base, dz = env[zm.group(1)], float(zm.group(2))
        else:
            base = env.get('posZ', y)
            dz = float(zt)
        step = 1e-6 if '0.000001' in body else 1e-5
        if 'z = v.z})' in body or 'z = v.z}' in body:
            step = 0
        depth = base - (dz + k * step) * 10000
        out.append((round(fx - x), round(fy - y), round(depth - y, 2), flip))
    spots = []
    for ex, ey in re.findall(r'table\.insert\(vPlaces,\s*\{isBusy = false,\s*x\s*=\s*([^,]+),\s*y\s*=\s*([^}]+)\}', body):
        spots.append((lua.Parser(lua.tokens(ex), env).expr(), lua.Parser(lua.tokens(ey), env).expr()))
    return out, spots


def cash_views():
    """the cash box: 10 notes on a stack's top (moved up by 16 a stack) and up to 10 stacks of 10"""
    body = lua.function_body(GS, 'initSkladCash')
    spots = []
    for ex, ey in re.findall(r'table\.insert\(vPlaces,\s*\{isBusy = false,\s*x\s*=\s*([^,]+),\s*y\s*=\s*([^}]+)\}', body):
        spots.append((lua.Parser(lua.tokens(ex), {'w': 30, 'h': 40}).expr(), lua.Parser(lua.tokens(ey), {'w': 30, 'h': 40}).expr()))
    return spots


# ---------------------------------------------------------------- the data functions of a mart
def tile_places(m):
    """getTilePlace: name -> (tile x, tile y, place for a shelf or -1, is a shelf, is the exit)"""
    body = lua.function_body(m['src'], 'getTilePlace')
    out = {}
    for name, text in blocks(body, 'vNamePlace'):
        name = name.strip("'")
        if name == 'hooligan':
            continue
        t = re.search(r'tile\s*=\s*vmath\.vector3\((\d+),\s*(\d+)', text)
        if t:
            tx, ty = int(t.group(1)), int(t.group(2))
        else:
            pp = re.search(r'this\.PLACES\["(\w+)"\]\.pos', text).group(1)
            pos = m['places'][m['index'][pp]]['pos']
            g = re.search(r'getTileFromPos\((.*)\)\s*$', text, re.M).group(1)
            ax, ay = g.split(',')
            env = {'placePos.x': pos[0], 'placePos.y': pos[1], 'skladOffset': 180, 'CELL_SIZE': 80}
            wx = lua.Parser(lua.tokens(ax), env).expr()
            wy = lua.Parser(lua.tokens(ay), env).expr()
            tx, ty = tile_from_pos(wx, wy)
        num = re.search(r'num\s*=\s*(' + ID + r')', text)
        out[name] = (tx, ty, place_id(m, num.group(1)) if num else -1, 'isSkladPlace = true' in text,
                     'isExitPlace = true' in text)
    return out


def tile_from_pos(x, y):
    import math
    return 17 + math.floor(x / 80), 9 + math.floor((y - 40) / 80)


def goals(m):
    """getDataPlace: a shelf or feeder a worker keeps full -> what to bring from where"""
    body = lua.function_body(m['src'], 'getDataPlace')
    out = {}
    for pid, text in blocks(body, 'vPlaceId'):
        g = {'offsetX': 0}
        for k, v in re.findall(r'wishFood\.(\w+)\s*=\s*(.*?)\n\s*(?=wishFood|$|--|elseif|end)', text + '\n', re.S):
            v = v.strip()
            if k == 'places_id':
                g[k] = [place_id(m, s) for s in re.findall(ID, v)]
            elif k in ('skladName', 'zavodName'):
                g[k] = v.strip("'")
            elif k == 'type':
                mm = re.match(r'this\.PLACES\["(\w+)"\]\.typeFood', v)
                g[k] = m['places'][m['index'][mm.group(1)]]['food'] if mm else int(v)
            else:
                g[k] = int(v)
        out[place_id(m, pid)] = g
    return out


def staff(m):
    """setupDodiki: hats, carry limits, the shelves each worker keeps full and where they may work"""
    body = lua.function_body(m['src'], 'setupDodiki')
    names = ['dodikAssist1', 'dodikAssist2', 'dodikAssist3', 'dodikAssist4', 'dodikAssist5', 'dodikAssist6',
             'dodikManager']
    out = []
    for n in names:
        s = {'hat': 0, 'maxFood': 3, 'goals': [], 'allow': [], 'man': 0}
        h = re.search(r'gameState\.%s:updateHat\((\d+)\)' % n, body)
        if h:
            s['hat'] = int(h.group(1))
        mf = re.search(r'gameState\.%s:setOptions\(\{maxFood = (\d+)\}\)' % n, body)
        if mf:
            s['maxFood'] = int(mf.group(1))
        st = re.search(r'gameState\.%s:setStockman\((\d+),\s*\{(.*?)\}\)' % n, body, re.S)
        if st:
            s['man'] = int(st.group(1))
            s['goals'] = [place_id(m, x) for x in re.findall(ID, st.group(2))]
        al = re.search(r'gameState\.%s\.allowWorkPlaces\s*=\s*\{(.*?)\}' % n, body, re.S)
        if al:
            s['allow'] = [place_id(m, x) for x in re.findall(ID, al.group(1))]
        out.append(s)
    return out


def next_sales(m):
    """checkNextPlaceSale: buying a place puts others up for sale (besides the main order)"""
    body = lua.function_body(m['src'], 'checkNextPlaceSale')
    out = {}
    for pid, text in blocks(body, 'vPrevPlaceId'):
        got = [int(x) - 1 for x in re.findall(r'listPlaceBoxes\[(\d+)\]', text)]
        got += [m['index'][x] for x in re.findall(r'table\.insert\(listNum,\s*"(\w+)"\)', text)]
        out[place_id(m, pid)] = got
    return out


def footprints(m):
    """getIndexes: the path grid's cells a bought place blocks (Lua's 1-based indexes)"""
    body = lua.function_body(m['src'], 'getIndexes')
    out = {}
    for pid, text in blocks(body, 'vNumPlaceBox'):
        cells = re.search(r'indexes\s*=\s*\{([^}]*)\}', text).group(1)
        out[place_id(m, pid)] = [int(c) - 1 for c in re.findall(r'\d+', cells)]
    return out


def bought_hooks(m):
    """placeCheckBought: what buying a place sets going: places it switches on, customers who come (after a
    delay), a worker, the truck to the other mart"""
    body = lua.function_body(m['src'], 'placeCheckBought')
    out = {}
    for pid, text in blocks(body, 'vBoxId'):
        h = {'on': [], 'buyers': [], 'staff': -1, 'truck': False, 'decor': None}
        env_num = {}
        for var, val in re.findall(r'local\s+(num)\s*=\s*(' + ID + r'|\d+)', text):
            env_num.setdefault('nums', []).append(place_id(m, val))
        # "this.listPlaceBoxes[num].isActive = true" after each "local num = ..."
        nums = iter(env_num.get('nums', []))
        for line in text.split('\n'):
            mm = re.search(r'local\s+num\s*=\s*(' + ID + r')', line)
            if mm:
                cur = place_id(m, mm.group(1))
            if re.search(r'listPlaceBoxes\[num\]\.isActive\s*=\s*true', line):
                h['on'].append(cur)
        for d, c in re.findall(r'timer\.delay\(([\d.]+),\s*false,\s*function\(\)\s*gameState:addNewBuyer\(gameState\.dodikCustomer(\d)\)', text):
            h['buyers'].append((int(c) - 1, float(d)))
        w = re.search(r'gameState\.(dodikAssist(\d)|dodikManager):updateVisible\(true\)', text)
        if w:
            h['staff'] = int(w.group(2)) - 1 if w.group(2) else 6
        if 'isAllowNextMart = true' in text:
            h['truck'] = True
            dx = re.search(r'posNextMarket\.x\s*=\s*boxNextMart\.x\s*([-+]\s*\d+)?', text).group(1)
            dy = re.search(r'posNextMarket\.y\s*=\s*boxNextMart\.y\s*([-+]\s*\d+)?', text).group(1)
            h['truck_off'] = (int((dx or '0').replace(' ', '')), int((dy or '0').replace(' ', '')))
            if 'boxNextMart.isActive = true' in text:
                h['on'].append(m['index']['placebox_nextmart'])
        dec = re.search(r"addDecorObject\('(\w+)',\s*vmath\.vector3\((-?\d+),\s*(-?\d+),\s*0\),\s*(-?\d+)\)", text)
        if dec and not h['truck']:
            h['decor'] = (dec.group(1), int(dec.group(2)), int(dec.group(3)), int(dec.group(4)))
        out[place_id(m, pid)] = h
    return out


def wish_sources(m):
    """getWishList: the foods customers may want (each once its place is bought), and how many"""
    body = lua.function_body(m['src'], 'getWishList')
    out = []
    for cond, text in re.findall(r'if\s*\((' + r'this\.listPlaceBoxes\[\d+\]\.isActive|this\.PLACES\["\w+"\]\.isActive' + r')\)\s*then(.*?)end', body, re.S):
        mm = re.match(r'this\.listPlaceBoxes\[(\d+)\]', cond)
        pid = int(mm.group(1)) - 1 if mm else m['index'][re.match(r'this\.PLACES\["(\w+)"\]', cond).group(1)]
        food = int(re.search(r'table\.insert\(typesFood,\s*(\d+)\)', text).group(1))
        wl = re.search(r'maxWishList\s*=\s*(\d+)', text)
        mf = re.search(r'maxFood\s*=\s*(\d+)', text)
        out.append((pid, food, int(wl.group(1)) if wl else 0, int(mf.group(1)) if mf else 0))
    counts = re.search(r'listCountFood\s*=\s*\{([^}]*)\}', body)
    return out, [int(x) for x in re.findall(r'\d+', counts.group(1))] if counts else None


def empty_checks(m):
    """checkEmptySklad: a machine's shelf needs nothing while the machine and its feeder are empty"""
    body = lua.function_body(m['src'], 'checkEmptySklad')
    out = {}
    for pid, text in blocks(body, 'vId'):
        refs = re.findall(r'listPlaceBoxes\[(\d+)\]\.countFood', text)
        refs = [int(r) - 1 for r in refs] + [m['index'][x] for x in re.findall(r'this\.PLACES\["(\w+)"\]\.countFood', text)]
        out[place_id(m, pid)] = refs
    return out


def warehouses(m):
    """fillWarehouses: on loading, a shelf whose food is made gets a random 0 .. capacity-5"""
    body = lua.function_body(m['src'], 'fillWarehouses')
    out = []
    for cond, text in re.findall(r'if\s*\(?(this\.listPlaceBoxes\[\d+\]\.isActive|this\.PLACES\["\w+"\]\.isActive)\)?\s*then(.*?)end', body, re.S):
        mm = re.match(r'this\.listPlaceBoxes\[(\d+)\]', cond)
        pid = int(mm.group(1)) - 1 if mm else m['index'][re.match(r'this\.PLACES\["(\w+)"\]', cond).group(1)]
        n = re.search(r'num\s*=\s*(' + ID + r')', text).group(1)
        out.append((pid, place_id(m, n)))
    return out


def zavods(m):
    """addZavodViews: the animals and machines (cow, chicken, popcorn machine...), each with its feeders"""
    body = lua.function_body(m['src'], 'addZavodViews')
    for z, f in re.findall(r'this\.PLACES\["(\w+)"\]\.linkSklad\s*=\s*this\.PLACES\["(\w+)"\]\.id', body):
        m['places'][m['index'][z]]['feed'] = m['index'][f]
    for z, f in re.findall(r'this\.PLACES\["(\w+)"\]\.linkSklad2\s*=\s*this\.PLACES\["(\w+)"\]\.id', body):
        m['places'][m['index'][z]]['feed2'] = m['index'][f]
    out = []
    for args in re.findall(r"gameState:addZavodObject\(([^)]*)\)", body):
        parts = [a.strip() for a in args.split(',')]
        kind = parts[0].strip("'")
        ids = [place_id(m, p) for p in parts[1:]]
        out.append((kind, ids))
    return out


def upgrades_rows(n, m):
    """the upgrade rows (global.lua's tables, the mart's listUpgradesId and placesUpgrade), in the order the
    upgrades window lists them (popup_upgrades.gui_script)"""
    if n == 1:
        ids = lua.value_after(m['src'], 'this.listUpgradesId')
        pu = lua.value_after(m['src'], 'this.placesUpgrade')
        ids = [i - 1 if i else -1 for i in ids]
        pu = [[x - 1 for x in p] for p in pu]
    else:
        body = lua.function_body(m['src'], 'updateDataMain')
        lst = re.search(r'this\.listUpgradesId\s*=\s*\{(.*?)\}', body, re.S).group(1)
        ids = [-1] + [m['index'][x] for x in re.findall(r'this\.PLACES\["(\w+)"\]', lst)]
        pu = []
        for grp in re.findall(r'\{(this\.PLACES[^{}]*)\}', re.search(r'this\.placesUpgrade\s*=\s*\{(.*)\}\s*$', body, re.S).group(1)):
            pu.append([m['index'][x] for x in re.findall(r'this\.PLACES\["(\w+)"\]', grp)])
    suffix = '' if n == 1 else '2'
    maxes = lua.value_after(GLOBAL, 'upgrades_max' + suffix)
    prices = lua.value_after(GLOBAL, 'upgrades_price' + suffix)
    popup = lua_src('main/gui/popup_upgrades.gui_scriptc.lua')
    body = lua.function_body(popup.replace('local function initUpgrades%d(self)' % n,
                                           'function this:initUpgrades%d(self)' % n), 'initUpgrades%d' % n)
    order = []
    for blk in re.findall(r'table\.insert\(self\.data_set,\s*\{(.*?)\}\)', body, re.S):
        v = int(re.search(r'value\s*=\s*(\d+)', blk).group(1))
        icon = re.search(r"icon\s*=\s*'(\w+)'", blk).group(1)
        text = re.search(r'textId\s*=\s*"(\w+)"', blk).group(1)
        small = [x for x in re.findall(r"icon[123]\s*=\s*'(\w+)'", blk)]
        pid = re.search(r'placeId\s*=\s*(\d+|gameState\.dataMart\.listUpgradesId\[(\d+)\])', blk)
        place = (int(pid.group(1)) - 1 if pid.group(1) != '0' else -1) if not pid.group(2) else ids[int(pid.group(2)) - 1]
        order.append({'value': v, 'icon': icon, 'text': text, 'small': small, 'place': place})
    return {'ids': ids, 'places_upgrade': pu, 'max': maxes, 'price': prices, 'order': order}


def dodik_for_upgrade(m):
    """getUpgradeDodik: upgrade row (1-based value) -> worker (0 cashier .. 5 assist6, 6 manager)"""
    body = lua.function_body(m['src'], 'getUpgradeDodik')
    out = {}
    for v, text in blocks(body, 'vNum'):
        w = re.search(r'gameState\.(dodikAssist(\d)|dodikManager)', text)
        out[int(v)] = int(w.group(2)) - 1 if w.group(2) else 6
    return out


def zavod_for_upgrade(m):
    body = lua.function_body(m['src'], 'getUpgradeZavod')
    out = {}
    for v, text in blocks(body, 'vNum'):
        out[int(v)] = int(re.search(r'placesUpgrade\[(\d+)\]', text).group(1)) - 1
    return out


# ---------------------------------------------------------------- pictures placed around the mart
def decor(n, m, col):
    """addDecorBack, addGraphicsPlaces and the zavod machines: (picture, x, y, depth y, flipped, scale, shown
    once this place is bought or -1)"""
    out = []
    src = m['src']
    P = m['places']

    def env(extra):
        def look(name):
            mm = re.match(r'this\.listPlaceBoxes\[(\w+)\]\.(x|y)$', name)
            if mm:
                k = mm.group(1)
                i = extra[k] if k in extra else int(k) - 1
                return P[i]['pos'][0 if mm.group(2) == 'x' else 1]
            mm = re.match(r'box\.(x|y)$', name)
            if mm:
                return P[extra['box']]['pos'][0 if mm.group(1) == 'x' else 1]
            mm = re.match(r'this\.posGateExit\.(x|y)$', name)
            if mm:
                return m['posGateExit'][mm.group(1)]
            if name in extra:
                return extra[name]
            raise KeyError(name)
        return look

    body = lua.function_body(src, 'addGraphicsPlaces')
    objs = {}       # variable -> entry
    cur = {}
    for st in re.split(r'\n', body):
        st = st.strip()
        mm = re.match(r'(?:local\s+)?num\s*=\s*this\.PLACES\["(\w+)"\]\.id', st)
        if mm:
            cur['num'] = m['index'][mm.group(1)]
            continue
        mm = re.match(r'(?:local\s+)?box\s*=\s*this\.listPlaceBoxes\[num\]', st)
        if mm:
            cur['box'] = cur['num']
            continue
        mm = re.match(r'(?:local\s+)?(\w+)\s*=\s*addGraphic\(\{sprite="(\w+)",\s*x=(.*?),\s*y=(.*?),\s*z=DEEP_BACKGROUND\}\)', st)
        if mm:
            x = lua.Parser(lua.tokens(mm.group(3)), env(cur)).expr()
            y = lua.Parser(lua.tokens(mm.group(4)), env(cur)).expr()
            e = [mm.group(2), x, y, None, False, 100, -1]
            objs[mm.group(1)] = e
            out.append(e)
            continue
        mm = re.match(r'(?:local\s+)?(\w+)\s*=\s*gameState:addDecorObject\(\'(\w+)\',\s*vmath\.vector3\((.*)\),\s*(-?\d+)\)', st)
        if mm:
            args = mm.group(3).rsplit(',', 1)[0]
            ax, ay = split2(args)
            x = lua.Parser(lua.tokens(ax), env(cur)).expr()
            y = lua.Parser(lua.tokens(ay), env(cur)).expr()
            e = [mm.group(2), x, y + int(mm.group(4)), y, False, 100, -1]
            objs[mm.group(1)] = e
            out.append(e)
            continue
        mm = re.match(r'addGraphic\(\{sprite="(\w+)",\s*x=(.*?),\s*y=(.*?),\s*z=DEEP_BACKGROUND\}\)', st)
        if mm:
            x = lua.Parser(lua.tokens(mm.group(2)), env(cur)).expr()
            y = lua.Parser(lua.tokens(mm.group(3)), env(cur)).expr()
            out.append([mm.group(1), x, y, None, False, 100, -1])
            continue
        mm = re.match(r'msg\.post\((\w+),\s*"updateScale",\s*\{value\s*=\s*([\d.]+)\}\)', st)
        if mm and mm.group(1) in objs:
            objs[mm.group(1)][5] = round(float(mm.group(2)) * 100)
            continue
        mm = re.match(r'this\.listPlaceBoxes\[(\w+)\]\.(carpet2?|iconSklad|tree)\s*=\s*(\w+)', st) or \
            re.match(r'(box)\.(carpet2?|iconSklad|tree)\s*=\s*(\w+)', st)
        if mm:
            k = mm.group(1)
            i = cur['box'] if k == 'box' else (cur[k] if k in cur else int(k) - 1)
            # a machine ("tree") shows with its zavod; carpets and shelf signs with their place
            objs[mm.group(3)][6] = i
    body = lua.function_body(src, 'addDecorBack')
    if True:
        for spr, args, off in re.findall(r"^\s*gameState:addDecorObject\('(\w+)',\s*vmath\.vector3\((.*?)\),\s*(-?\d+)\)", body, re.M):
            ax, ay, _ = split3(args)
            x = lua.Parser(lua.tokens(ax), env({})).expr()
            y = lua.Parser(lua.tokens(ay), env({})).expr()
            out.append([spr, x, y + int(off), y, False, 100, -1])
    if n == 2:
        offsets = {k: int(v) for k, v in re.findall(r'offsets\[hash\("(\w+)"\)\]\s*=\s*(\d+)', body)}
        for i in range(1, 200):
            inst = col.get('/dec%d' % i)
            if not inst:
                break
            spr = sprites_of(inst['proto'])['sprite'][0]
            q = inst.get('rot', (0, 0, 0, 1))
            flip = abs(q[1]) > 0.7     # a half turn about y: drawn mirrored
            x, y = inst['pos']
            off = offsets.get(spr, 0)
            # addDecorObject2(hash, pos with y - offY, offY): drawn at pos, sorted at pos.y - offY
            out.append([spr, x, y, y - off, flip, 100, -1])
    return out


def split2(s):
    depth, parts, cur = 0, [], ''
    for ch in s:
        if ch == '(':
            depth += 1
        elif ch == ')':
            depth -= 1
        if ch == ',' and depth == 0 and not parts:
            parts.append(cur)
            cur = ''
        else:
            cur += ch
    parts.append(cur)
    return parts[0], parts[1]


def split3(s):
    a, rest = split2(s)
    b, c = split2(rest)
    return a, b, c


# ---------------------------------------------------------------- C output
def c_arr(v):
    if isinstance(v, (list, tuple)):
        return '{' + ','.join(c_arr(x) for x in v) + '}'
    if isinstance(v, bool):
        return '1' if v else '0'
    if isinstance(v, float):
        return str(round(v))
    return str(v)


def main():
    global GS, GLOBAL
    if WEB is None:
        sys.exit(__doc__)
    GS = lua_src('main/gamecore/GameState.luac.lua')
    GLOBAL = lua_src('main/global.luac.lua')
    marts = []
    for n in (1, 2):
        m = mart_tables(n)
        col = collection(WEB / ('decoded/main/screen_game/mart%d.collectionc' % n))
        m['places'] = places(m, col)
        m['zavods'] = zavods(m)
        m['tiles'] = tile_places(m)
        m['goals'] = goals(m)
        m['staff'] = staff(m)
        m['next'] = next_sales(m)
        m['blocks'] = footprints(m)
        m['hooks'] = bought_hooks(m)
        m['wish'], m['wish_counts'] = wish_sources(m)
        m['empty'] = empty_checks(m)
        m['fill'] = warehouses(m)
        m['up'] = upgrades_rows(n, m)
        m['up_dodik'] = dodik_for_upgrade(m)
        m['up_zavod'] = zavod_for_upgrade(m)
        m['decor'] = decor(n, m, col)
        for p in m['places']:
            p['views'], p['spots'] = ([], [])
            if p['sklad'] and p['init'] and 'Cash' not in p['init']:
                p['views'], p['spots'] = sklad_views(p['init'], p['pos'][0], p['pos'][1], p['food'], p['offsetSklad'])
        marts.append(m)
    if '--dump' in sys.argv:
        dump = []
        for m in marts:
            dump.append({k: v for k, v in m.items() if k not in ('src', 'boxes', 'index')})
        print(json.dumps(dump, indent=1, default=str))
        return
    global ROOT
    check = '--check' in sys.argv
    if check:
        import tempfile, shutil
        tmp = Path(tempfile.mkdtemp())
        (tmp / 'src').mkdir()
        (tmp / 'tools').mkdir()
        shutil.copy(SRC / 'tools' / 'data.h.in', tmp / 'tools')
        ROOT = tmp
    pictures = write_c(marts)
    import art
    art.write(WEB, ROOT, pictures)
    if check:
        bad = [f for f in ('data.c', 'data.h', 'art.c', 'art.h') if (ROOT / 'src' / f).read_bytes() != (SRC / 'src' / f).read_bytes()]
        shutil.rmtree(ROOT)
        if bad:
            sys.exit('the web game gives other ' + ', '.join(bad) + ' than src/ holds: make data WEB=...')
        print('src/data.c, data.h, art.c and art.h are the web game\'s')


# the pictures data.c names (art.py makes them): "atlas:name", optionally "@angle" (turned) and "/percent" (scale,
# where 100 is the world's 1/3)
PICTURES = []


def pic(name):
    if not name:
        return 'P_NONE'
    if ':' not in name:
        name = 'game:' + name
    if name not in PICTURES:
        PICTURES.append(name)
    return pic_symbol(name)


def pic_symbol(name):
    name = name.replace('~', '_flip').replace('@-', '_turn_minus').replace('@', '_turn').replace('/', '_size')
    return 'P_' + re.sub(r'[^A-Za-z0-9]+', '_', name).upper().strip('_')


def zavod_views(p):
    """GameState:addPlaces: how a plant, an animal's or a machine's food shows (offsets from the place, depth as a
    y offset, picture)"""
    f, out = p['food'], []
    zy = p['zy'] - p['pos'][1]          # the place's own depth, relative to pos.y

    def put(dx, dy, dz, name):
        out.append((dx, dy, round(zy - dz * 10000, 2), name))
    if f == 1:      # one bunch: banana1..3 by the count
        put(4, 18, 0, 'banana%d')
    elif f == 2:
        put(0, 58, 0, 'corn1')
    elif f == 3:
        for dx, dy, dz in ((-21, 10, 22), (21, 10, 22), (-14, 30, 21), (28, 30, 21), (-21, 40, 22), (21, 40, 22),
                           (-14, 60, 21), (28, 60, 21)):
            put(dx, dy, dz * 1e-5, 'food3')
    elif f == 4:
        for dx, dy, dz in ((-28, 10, 22), (15, 4, 22), (-15, 33, 21), (28, 27, 21), (-28, 43, 22), (15, 37, 22),
                           (-15, 66, 21), (28, 60, 21)):
            put(dx, dy, dz * 1e-5, 'food4')
    elif f == 5:
        for dx, dy, dz in ((-22, 10, 22), (22, 4, 22), (-7, 38, 21), (37, 32, 21), (-22, 42, 22), (22, 36, 22),
                           (-7, 70, 21), (37, 64, 21)):
            put(dx, dy, dz * 1e-5, 'food5')
    elif f == 6:
        for dx, dy, dz in ((-25, 10, 22), (25, 4, 22), (-8, 42, 21), (42, 36, 21), (-25, 43, 22), (25, 37, 22),
                           (-8, 75, 21), (42, 69, 21)):
            put(dx, dy, dz * 1e-5, 'food6')
    elif f == 7:
        put(-6, -6, 0, 'cocoa1@60')
        put(29, 4, -1e-4, 'cocoa1@100')
    elif f == 8:
        put(10, 66, 21e-5, 'coffee_bean1@100')
        put(28, 32, 21e-5, 'coffee_bean1@65')
        put(-25, 42, 21e-5, 'coffee_bean2@-75')
    elif f == 9:
        pass        # the plant itself (shown while it has a peanut)
    else:
        w, h, lvl, dx1, dx2, dy1, dy2 = {10: (23, 31, 15, -5, 12, 2, -5), 11: (23, 30, 30, -8, 9, 4, -2),
                                         12: (22, 30, 37, 0, 17, 10, 4), 13: (22, 29, 39, -4, 13, 12, 6),
                                         14: (25, 30, 16, -10, 9, -12, -19)}[f]
        if f == 11:
            zy = -40
        for dx, dy, dz in ((-w + dx1, dy1, 23), (w + dx1, dy2, 24), (-w + dx2, dy1 + h, 21), (w + dx2, dy2 + h, 22),
                           (-w + dx1, dy1 + lvl, 27), (w + dx1, dy2 + lvl, 28), (-w + dx2, dy1 + h + lvl, 25),
                           (w + dx2, dy2 + h + lvl, 26)):
            put(dx, dy, dz * 1e-5, 'food%d' % f)
    return out


def ground(p):
    """the patches of earth addPlaces puts under the plants (always there)"""
    x, y = p['pos']
    if p['zavod'] and p['food'] == 1:
        return [('place1', x - 8, y - 26)]
    if p['zavod'] and p['food'] == 7:
        return [('place_corn3', x + 5, y - 57)]
    if p['zavod'] and p['food'] == 8:
        return [('place1', x, y - 23)]
    return []


def write_c(marts):
    out = []
    h = []
    views = []          # (dx, dy, dz10, picture)
    tile_names = []
    for m in marts:
        for k in m['tiles']:
            if k not in tile_names:
                tile_names.append(k)
    for k in ('hooligan',):
        tile_names.append(k)
    T = lambda name: 'T_' + name.upper()
    alert_names = []    # the zavod objects' names
    for m in marts:
        for kind, ids in m['zavods']:
            if kind not in alert_names:
                alert_names.append(kind)

    c = out.append
    c('/* Made by tools/data.py from the web game (version 2.1): see src/data.h. */')
    c('#include "data.h"')
    c('#include "art.h"')
    c('')
    mart_c = []
    for n, m in enumerate(marts, 1):
        P = m['places']
        idx = lambda i: 'MM_NONE' if i is None or i < 0 else str(i)
        polys, spots, places = [], [], []
        for i, p in enumerate(P):
            flags = []
            for k, f in (('zavod', 'PL_ZAVOD'), ('sklad', 'PL_SKLAD'), ('buyer', 'PL_BUYER'), ('solid', 'PL_SOLID'),
                         ('hide', 'PL_HIDE')):
                if p[k]:
                    flags.append(f)
            v0 = len(views)
            if p['sklad'] and p['views']:
                for dx, dy, dz, flip in p['views']:
                    views.append((dx, dy, round(dz * 10), pic('food%d%s' % (p['food'], '~' if flip else ''))))
            elif p['zavod']:
                for dx, dy, dz, name in zavod_views(p):
                    if '%d' in name:
                        for k in (1, 2, 3):
                            pic(name % k)
                        name = name % 1
                    views.append((dx, dy, round(dz * 10), pic(name)))
            nv = len(views) - v0
            s0 = len(spots)
            spots += [(round(x), round(y)) for x, y in p['spots']]
            if p['link'] == 'placebox_cashbox':
                spots += [(round(x), round(y)) for x, y in cash_views()]
            poly = 'MM_NONE'
            if p['poly']:
                poly = str(len(polys))
                polys.append(p['poly'])
            pic_name = p['pic']
            places.append('  {%s, %s, %s, %s, %d, %d, %d, %s, %d, %d, %d, %d, %d, %s, %s, %s, %s, %d, %d, %d, %d, %s},  /* %d %s */' % (
                c_arr(p['pos']), c_arr(p['work']), c_arr(p['buy']), c_arr(p['pos2']), p['zy'], p['radius'],
                p['price'], ' | '.join(flags) or '0', p['food'], p['count0'], p['cap0'], p['build'], p['stamina'],
                idx(p['feed']), idx(p['feed2']), pic(pic_name), pic(p['pic2']), v0, nv, s0, len(spots) - s0, poly,
                i, p['link']))
        c('static const mm_place places%d[] = {' % n)
        out.extend(places)
        c('};')
        c('static const mm_pt polys%d[][4] = {%s};' % (n, ','.join(c_arr(q) for q in polys)))
        c('static const mm_pt spots%d[] = {%s};' % (n, ','.join(c_arr(q) for q in spots)))
        order = [x - 1 for x in m['listSalesMain']]
        c('static const uint8_t order%d[] = %s;' % (n, c_arr(order)))
        nxt = [(k, v) for k in sorted(m['next'], key=int) for v in m['next'][k]]
        c('static const uint8_t unlocks%d[][2] = {%s};' % (n, ','.join(c_arr((int(k), v)) for k, v in nxt)))
        cells = [(int(k), v) for k in sorted(m['blocks'], key=int) for v in m['blocks'][k]]
        c('static const uint16_t blocks%d[][2] = {%s};' % (n, ','.join(c_arr(q) for q in cells)))
        hooks = []
        for k in sorted(m['hooks'], key=int):
            hk = m['hooks'][k]
            for o in hk['on']:
                hooks.append((int(k), 'HK_ON', o, 0))
            for cu, d in hk['buyers']:
                hooks.append((int(k), 'HK_BUYER', cu, round(d * 60)))
            if hk['staff'] >= 0:
                hooks.append((int(k), 'HK_STAFF', hk['staff'], 0))
            if hk['truck']:
                hooks.append((int(k), 'HK_TRUCK', hk['truck_off'][0], hk['truck_off'][1]))
            if hk['decor']:
                spr, x, y, off = hk['decor']
                m['decor'].append([spr, x, y + off, y, False, 100, int(k)])
        c('static const mm_hook hooks%d[] = {%s};' % (n, ','.join('{%d, %s, %d, %d}' % q for q in hooks)))
        # the world grid: 1 bit a cell, 1 where walkable (the game's tile kinds 1 and 2)
        w, hh = m['map_width'], m['map_height']
        bits = [0] * ((w * hh + 7) // 8)
        for i2, v in enumerate(m['world']):
            if v:
                bits[i2 >> 3] |= 1 << (i2 & 7)
        c('static const uint8_t grid%d[] = %s;' % (n, c_arr(bits)))
        tiles = []
        for name in tile_names:
            t = m['tiles'].get(name)
            if t:
                tiles.append('{%d, %d, %s, %s}' % (t[0], t[1], idx(t[2]), ('TL_SKLAD' if t[3] else '0') + (' | TL_EXIT' if t[4] else '')))
            else:
                tiles.append('{255, 255, MM_NONE, 0}')
        c('static const mm_tile tiles%d[] = {%s};' % (n, ','.join(tiles)))
        goals = []
        goal_index = {}
        for k in sorted(m['goals'], key=int):
            g = m['goals'][k]
            pl = g['places_id'] + [255] * (6 - len(g['places_id']))
            goal_index[int(k)] = len(goals)
            goals.append('{%d, %d, %d, %d, %s, %s, %s, %d}' % (int(k), g['type'], g['offsetX'], g['offsetY'],
                                                               T(g['skladName']), T(g['zavodName']), c_arr(pl),
                                                               len(g['places_id'])))
        c('static const mm_goal goals%d[] = {%s};' % (n, ','.join(goals)))
        staff = []
        for sidx, st in enumerate(m['staff']):
            gl = [goal_index[g] for g in st['goals']] + [255] * (3 - len(st['goals']))
            mask = sum(1 << a for a in st['allow'])
            staff.append('{%d, %d, %d, %d, %s, UINT64_C(0x%x)}' % (st['hat'], st['maxFood'], st['man'], len(st['goals']), c_arr(gl), mask))
        c('static const mm_staff staff%d[] = {%s};' % (n, ','.join(staff)))
        wl = ','.join('{%d, %d, %d, %d}' % q for q in m['wish'])
        c('static const uint8_t wish%d[][4] = {%s};' % (n, wl))
        emp = []
        for k in sorted(m['empty'], key=int):
            r = m['empty'][k] + [255] * (3 - len(m['empty'][k]))
            emp.append(c_arr([int(k)] + r))
        c('static const uint8_t empties%d[][4] = {%s};' % (n, ','.join(emp)))
        c('static const uint8_t fills%d[][2] = {%s};' % (n, ','.join(c_arr(q) for q in m['fill'])))
        zv = []
        for kind, ids in m['zavods']:
            z = ids[0]
            feeds = ids[1:] + [255]
            fp = P[feeds[0]]
            if kind == 'cow':
                opos = (fp['pos'][0] + 77, fp['pos'][1] + 50)
                panel = (fp['pos'][0] - 50, fp['pos'][1] + 90)
                panel2 = (0, 0)
                rad, k2 = 7000, 'ZV_COW'
            elif kind == 'chicken':
                opos = (fp['pos'][0] + 43, fp['pos'][1] + 58)
                panel = (fp['pos'][0] - 50, fp['pos'][1] + 90)
                panel2 = (0, 0)
                rad, k2 = 7000, 'ZV_CHICKEN'
            else:
                x, y = fp['pos'][0], fp['pos'][1] + 10
                opos = (x, y)
                op = fp['offsetPlaha']
                panel = (x - 140 + op['x'], y + 125 + op['y'])
                panel2 = (0, 0)
                if feeds[1] != 255:
                    op2 = P[feeds[1]]['offsetPlaha']
                    panel2 = (x - 140 + op2['x'], y + 125 + op2['y'])
                rad, k2 = 18000, 'ZV_MACHINE'
            appliance = {'cow': 'cow_animal', 'chicken': 'chicken_animal'}.get(kind, kind + '_appliance')
            zv.append('{%s, %d, %d, %d, %d, %s, %s, %s, %d, %s}' % (k2, alert_names.index(kind), z, feeds[0], feeds[1],
                                                                  c_arr(opos), c_arr(panel), c_arr(panel2), rad, T(appliance)))
        c('static const mm_zavod zavods%d[] = {%s};' % (n, ','.join(zv)))
        # upgrades: rows by the global tables' index (value - 1)
        up = m['up']
        rows = []
        for v in range(1, len(up['max']) + 1):
            if v == 1:
                kind, target = 'UP_PLAYER', 0
            elif v in m['up_dodik']:
                kind, target = 'UP_STAFF', m['up_dodik'][v]
            else:
                kind, target = 'UP_ZAVOD', m['up_zavod'][v]
            pr = [[0 if isinstance(x, str) else x for x in lv] for lv in up['price'][v - 1]]
            rows.append('{%s, %d, %s, %s, %s}' % (kind, target, idx(up['ids'][v - 1]), c_arr(up['max'][v - 1]), c_arr(pr)))
        c('static const mm_uprow uprows%d[] = {%s};' % (n, ','.join(rows)))
        shown = []
        for o in up['order']:
            small = (o['small'] + ['', '', ''])[:3]
            shown.append('{%d, %s, %s, %s, %s}' % (o['value'] - 1, idx(o['place']), pic('ui_main:' + o['icon'] + '/75'),
                                                 pic('ui_main:' + o['icon'] + '_tint/75') if o['place'] >= 0 else 'P_NONE',
                                                 c_arr([pic(('ui_game:' + s + '/85') if s.startswith('food') else ('ui_main:' + s + '/70')) if s else 'P_NONE' for s in small])))
            shown[-1] = shown[-1][:-1] + ', "%s"}' % upgrade_title(o['text'])
        c('static const mm_upshow upshow%d[] = {%s};' % (n, ','.join(shown)))
        zp = up['places_upgrade']
        c('static const uint8_t upzavod%d[][3] = {%s};' % (n, ','.join(c_arr((g + [255, 255])[:3]) for g in zp)))
        # pictures placed around the mart
        dec = []
        for p in P:
            for spr, x, y in ground(p):
                dec.append([spr, x, y, None, False, 100, -1])
        dec += m['decor']
        for nm, k in (('posTrash', 'trash1'), ('posBasket', 'baskets1')):
            t = m[nm]
            dec.append([k, t['x'], t['y'] + t['offsetY'], t['y'], False, 100, -1])
        rows = []
        for spr, x, y, zy, flip, scale, cond in dec:
            name = spr + ('~' if flip else '') + ('' if scale == 100 else '/%d' % scale)
            rows.append('{%s, %d, %d, %s, %s}' % (pic(name), round(x), round(y), 'MM_GROUND' if zy is None else round(zy), idx(cond)))
        c('static const mm_decor decor%d[] = {%s};' % (n, ','.join(rows)))
        pt = lambda d: '{%d, %d}' % (round(d['x']), round(d['y']))
        cam = [x - 1 for x in m['placesCamera']]
        c('static const uint8_t camera%d[] = %s;' % (n, c_arr(cam)))
        last = order[-1] if n == 2 else lua.value_after(m['src'], 'this.NUMPLACE_LAST') - 1
        if n == 1:
            zav, mn = lua.value_after(m['src'], 'this.NUMPLACE_ZAVOD') - 1, lua.value_after(m['src'], 'this.NUMPLACE_MIN') - 1
        else:
            body = lua.function_body(m['src'], 'updateDataMain')
            zav = m['index'][re.search(r'NUMPLACE_ZAVOD\s*=\s*this\.PLACES\["(\w+)"\]', body).group(1)]
            mn = m['index'][re.search(r'NUMPLACE_MIN\s*=\s*this\.PLACES\["(\w+)"\]', body).group(1)]
        mart_c.append(dict(n=n, count=len(P), order=len(order), unlocks=len(nxt), blocks=len(cells), hooks=len(hooks),
                           w=w, h=hh, goals=len(goals), wish=len(m['wish']), empties=len(emp), fills=len(m['fill']),
                           zavods=len(zv), uprows=len(up['max']), upshow=len(up['order']), decor=len(rows),
                           camera=len(cam), start=pt(m['posDodik']), lx=(m['limitMoveX']['min'], m['limitMoveX']['max']),
                           ly=(m['limitMoveY']['min'], m['limitMoveY']['max']), exit=pt(m['posExit']),
                           door=pt(m['posStart']), manager=pt(m['posManager']), trash=pt(m['posTrash']),
                           basket=pt(m['posBasket']), last=last, zav=zav, mn=mn, crazy=m['NUM_SALE_CRAZY'],
                           cash=m['index']['placebox_cashbox'], truck=m['index']['placebox_nextmart'],
                           wish_counts=m['wish_counts'], bg=str(n - 1)))
    c('')
    c('const mm_mart mm_marts[MM_MARTS] = {')
    for d in mart_c:
        n = d['n']
        wc = d['wish_counts'] or [0] * 8
        c('  {places%d, polys%d, spots%d, order%d, unlocks%d, blocks%d, hooks%d, grid%d, tiles%d, goals%d, staff%d, wish%d,' % ((n,) * 12))
        c('   empties%d, fills%d, zavods%d, uprows%d, upshow%d, upzavod%d, decor%d, camera%d,' % ((n,) * 8))
        c('   %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d,' % (
            d['count'], d['order'], d['unlocks'], d['blocks'], d['hooks'], d['w'], d['h'], d['goals'], d['wish'],
            d['empties'], d['fills'], d['zavods'], d['uprows'], d['upshow'], d['decor'], d['camera'], len(marts[n - 1]['up']['places_upgrade'])))
        c('   %s, %d, %d, %d, %d, %s, %s, %s, %s, %s,' % (d['start'], d['lx'][0], d['lx'][1], d['ly'][0], d['ly'][1],
                                                     d['exit'], d['door'], d['manager'], d['trash'], d['basket']))
        c('   %d, %d, %d, %d, %d, %d, %s, %s},' % (d['last'], d['zav'], d['mn'], d['crazy'], d['cash'], d['truck'], c_arr(wc), d['bg']))
    c('};')
    c('const mm_view mm_views[] = {%s};' % ','.join('{%d, %d, %d, %s}' % v for v in views))
    g = GLOBAL
    prices = lua.value_after(g, 'priceFood')
    maxes = lua.value_after(g, 'maxTypeFood')
    hats = lua.value_after(g, 'data_hats')
    c('const uint8_t mm_food_price[MM_FOODS + 1] = %s;' % c_arr([0] + prices))
    c('const uint8_t mm_food_most[MM_FOODS + 1] = %s;' % c_arr([0] + maxes))
    c('const mm_hat mm_hats[MM_HATS] = {%s};' % ','.join(
        '{%s, %d}' % ({'coins': 'HAT_COINS', 'free': 'HAT_FREE', 'ads': 'HAT_ADS'}[x['type']], x['amount']) for x in hats))
    names = ['Bananas', 'Corn', 'Eggs', 'Milk', 'Popcorn', 'Yogurt', 'Cocoa', 'Coffee beans', 'Peanuts', 'Chocolate',
             'Ground coffee', 'Peanut butter', 'Cappuccino', 'Chocolate bars']
    c('const char *const mm_food_name[MM_FOODS + 1] = {"", %s};' % ', '.join('"%s"' % x for x in names))
    c('const uint16_t mm_food_pic[MM_FOODS + 1] = {P_NONE, %s};' % ', '.join(pic('food%d' % f) for f in range(1, 15)))
    c('const uint16_t mm_food_icon[MM_FOODS + 1] = {P_NONE, %s};' % ', '.join(pic('ui_game:food%d_icon' % f) for f in range(1, 15)))
    c('const uint16_t mm_alert_icon[] = {%s};' % ', '.join(pic('ui_game:icon_%s2/90' % {'cow': 'cow', 'chicken': 'chicken', 'popcorn': 'microwave', 'yogurt': 'yogurt_machine', 'chocolate': 'chocolate_machine', 'coffeeground': 'coffee_grinder', 'peanutbutter': 'peanut_machine', 'cappuccino': 'coffee_machine', 'chocolatebar': 'chocolate_bar_machine'}[a]) for a in alert_names))
    (ROOT / 'src' / 'data.c').write_text('\n'.join(out) + '\n')

    # data.h: the types, then the names it uses
    tpl = (ROOT / 'tools' / 'data.h.in').read_text()
    tnames = ',\n'.join('  ' + T(x) for x in tile_names)
    tpl = tpl.replace('@TILES@', tnames).replace('@VIEWS@', str(len(views)))
    tpl = tpl.replace('@ALERTS@', ', '.join('AL_' + a.upper() for a in alert_names))
    (ROOT / 'src' / 'data.h').write_text(tpl)
    return PICTURES


def upgrade_title(text_id):
    t = {'PLAYER': 'You', 'ASSISTANT': 'Assistant', 'MANAGER': 'Manager', 'FARMER': 'Farmer', 'CHEF': 'Chef',
         'CHICKEN': 'Chicken', 'COW': 'Cow', 'MICROWAVE': 'Microwave', 'YOGURT_MAKER': 'Yogurt maker',
         'COFFEE_GRINDER': 'Grinder', 'COFFEE_MACHINE': 'Coffee maker', 'PEANUT_MACHINE': 'Peanut maker',
         'CHOCOLATE_MACHINE': 'Chocolate maker', 'CHOCOLATE_BAR_MACHINE': 'Chocolate bar maker'}
    return t[text_id]


if __name__ == '__main__':
    main()
