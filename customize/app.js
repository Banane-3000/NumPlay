/* NumPlay Studio: pick games, add Retro-Go games, and link a NumPlay.nwa of your own in the
 * browser. The kit (kit/, made by `make kit`, tools/kit.py) holds the objects the Makefile links
 * NumPlay.nwa from; GNU ld and objcopy, built as WebAssembly (ld/), link them here the same way. */
'use strict';
(() => {
  const KIT = 'kit/';
  const MiB = 1048576;
  const XL_SPACE = Math.round(6.6 * MiB);  // Enlarged Extra Apps (Nwagra), as issue #57 promised
  const SYS = {
    1: { tag: 'GB', name: 'Game Boy', color: '#6d8a0c', shell: '#c9c9cf' },
    2: { tag: 'GBC', name: 'Game Boy Color', color: '#6a3fb5', shell: '#b9a6d8' },
    3: { tag: 'NES', name: 'NES', color: '#c43a2f', shell: '#d9d9de' },
    4: { tag: 'SMS', name: 'Master System', color: '#2f62c4', shell: '#cfd6e4' },
    5: { tag: 'GG', name: 'Game Gear', color: '#3b3b3b', shell: '#c8c8cc' },
  };
  const EXT = { gb: 1, gbc: 2, nes: 3, sms: 4, gg: 5 };
  const ROM_ENTRY = 52, ROM_HEAD = 12;
  const VARIANT_NAMES = { 'NumPlay': 'NumPlay', 'NumPlay-Invisible': 'Invisible', 'NumPlay-Matrices': 'Matrices' };
  const W = 320, H = 222;  // the screen below its status bar, in the calculator's pixels

  const $ = (s) => document.querySelector(s);
  const el = (tag, cls, attrs) => {
    const e = document.createElement(tag);
    if (cls) e.className = cls;
    if (attrs) for (const [k, v] of Object.entries(attrs)) e.setAttribute(k, v);
    return e;
  };
  const align4 = (n) => (n + 3) & ~3;
  const hex = (s) => '#' + String(s).replace(/^0x/i, '').padStart(6, '0');
  const mix = (h, t) => {
    const n = parseInt(h.slice(1), 16), c = [n >> 16, (n >> 8) & 255, n & 255];
    return 'rgb(' + c.map((v) => Math.round(t > 0 ? v + (255 - v) * t : v * (1 + t))).join(',') + ')';
  };
  const size = (b) => b >= MiB ? (b / MiB).toFixed(2) + ' MB' : Math.max(1, Math.round(b / 1024)) + ' KB';

  let kit = null, rg = null;
  const st = { picked: new Set(), roms: [], cap: 'std', variant: 'NumPlay' };
  let romKey = 0;

  // ------------------------------------------------------------------ sizes
  const capacity = () => st.cap === 'xl' ? XL_SPACE : kit.appSpace;
  const romBytes = (r) => ROM_ENTRY + align4(r.bytes.length);
  function items() {
    const out = [{ key: '_core', name: 'NumPlay', bytes: kit.base, color: '#3a3a3c', icon: KIT + 'variants/' + st.variant + '.png' }];
    for (const g of kit.games)
      if (st.picked.has(g.id) && g !== rg)
        out.push({ key: g.id, id: g.id, name: g.title, bytes: g.flash, color: hex(g.top), icon: g.icon ? KIT + 'icons/' + g.id + '.png' : null });
    if (rg && st.roms.length) {
      out.push({ key: rg.id, id: rg.id, name: rg.title, bytes: rg.flash + ROM_HEAD, color: hex(rg.top), icon: rg.icon ? KIT + 'icons/' + rg.id + '.png' : null });
      for (const r of st.roms)
        out.push({ key: 'rom' + r.key, rom: r, name: r.name, bytes: romBytes(r), color: SYS[r.sys].color, tag: SYS[r.sys].tag });
    }
    return out;
  }
  const usedBytes = () => items().reduce((s, it) => s + it.bytes, 0);

  // ------------------------------------------------------------------ the squarified treemap
  function treemap(list, total) {
    const scale = W * H / total;
    let rest = list.slice().sort((a, b) => b.bytes - a.bytes).map((it) => ({ ...it, a: it.bytes * scale }));
    const worst = (row, side) => {
      const s = row.reduce((a, r) => a + r.a, 0), mx = Math.max(...row.map((r) => r.a)), mn = Math.min(...row.map((r) => r.a));
      return Math.max(side * side * mx / (s * s), (s * s) / (side * side * mn));
    };
    let rx = 0, ry = 0, rw = W, rh = H;
    const out = [];
    while (rest.length) {
      const side = Math.min(rw, rh);
      let row = [rest[0]], best = worst(row, side), i = 1;
      while (i < rest.length) {
        const next = row.concat([rest[i]]), w = worst(next, side);
        if (w > best) break;
        row = next; best = w; i++;
      }
      const sum = row.reduce((a, r) => a + r.a, 0);
      if (rw >= rh) {
        const cw = rh ? sum / rh : 0; let cy = ry;
        for (const r of row) { const ch = cw ? r.a / cw : 0; out.push({ ...r, x: rx, y: cy, w: cw, h: ch }); cy += ch; }
        rx += cw; rw -= cw;
      } else {
        const ch = rw ? sum / rw : 0; let cx = rx;
        for (const r of row) { const cw = ch ? r.a / ch : 0; out.push({ ...r, x: cx, y: ry, w: cw, h: ch }); cx += cw; }
        ry += ch; rh -= ch;
      }
      rest = rest.slice(row.length);
    }
    return out;
  }

  // ------------------------------------------------------------------ drawing
  const tiles = new Map(), segs = new Map(), buttons = new Map();
  let shownUsed = 0, tween = 0;

  function render() {
    const list = items(), cap = capacity();
    const used = list.reduce((s, it) => s + it.bytes, 0);
    const over = used > cap;

    // the screen
    const free = cap - used;
    const all = free > cap * 0.002 ? list.concat([{ key: '_free', name: 'Free', bytes: free, free: true }]) : list;
    const laid = treemap(all, Math.max(cap, used));
    const map = $('#map'), seen = new Set(), first = !tiles.size;
    laid.forEach((t, i) => {
      let e = tiles.get(t.key);
      const fresh = !e;
      if (fresh) {
        e = el('div', 'tile enter');
        if (t.icon) e.appendChild(el('img', '', { src: t.icon, alt: '', draggable: 'false' }));
        if (t.tag) e.appendChild(el('b')).textContent = t.tag;
        map.appendChild(e);
        tiles.set(t.key, e);
        if (t.id || t.rom) e.addEventListener('click', () => (t.rom ? removeRom(t.rom) : toggle(t.id)));
      }
      e.classList.remove('leave');
      e.classList.toggle('free', !!t.free);
      e.style.left = (100 * t.x / W) + '%';
      e.style.top = (100 * t.y / H) + '%';
      e.style.width = (100 * t.w / W) + '%';
      e.style.height = (100 * t.h / H) + '%';
      e.style.cursor = t.id || t.rom ? 'pointer' : '';
      if (!t.free)
        e.style.background = 'radial-gradient(130% 130% at 30% 25%, ' + mix(t.color, 0.45) + ' 0%, ' + t.color + ' 45%, ' + mix(t.color, -0.55) + ' 100%)';
      const s = Math.min(t.w, t.h);
      const img = e.querySelector('img');
      if (img) {
        img.style.width = (100 * Math.min(44, s * 0.62) / Math.max(t.w, 1)) + '%';
        img.style.opacity = s >= 13 ? 1 : 0;
      }
      const b = e.querySelector('b');
      if (b) b.style.opacity = s >= 14 ? 1 : 0;
      e.dataset.tip = t.free ? 'Free|' + size(t.bytes) : t.name + '|' + size(t.bytes);
      if (fresh) {
        if (first) {  // (the first time, the blocks come in one after another, the biggest first)
          e.style.transitionDelay = (18 * i) + 'ms';
          setTimeout(() => { e.style.transitionDelay = ''; }, 18 * i + 400);
        }
        requestAnimationFrame(() => requestAnimationFrame(() => e.classList.remove('enter')));
      }
      seen.add(t.key);
    });
    for (const [k, e] of tiles) if (!seen.has(k)) { e.classList.add('leave'); tiles.delete(k); setTimeout(() => e.remove(), 260); }

    // the bar beside it: the same blocks, stacked
    const bar = $('#bar'), keys = new Set();
    list.forEach((it, i) => {
      let e = segs.get(it.key);
      if (!e) {
        e = el('span', 'seg');
        let next = null;  // (in its place among the blocks there already)
        for (let j = i + 1; j < list.length && !next; j++) next = segs.get(list[j].key) || null;
        segs.set(it.key, e);
        bar.insertBefore(e, next);
        void e.offsetHeight;  // (so its height grows from nothing)
      }
      e.style.height = Math.min(100, 100 * it.bytes / cap) + '%';
      e.style.background = 'linear-gradient(90deg, ' + mix(it.color, 0.25) + ', ' + it.color + ' 55%, ' + mix(it.color, -0.3) + ')';
      e.dataset.tip = it.name + '|' + size(it.bytes);
      keys.add(it.key);
    });
    for (const [k, e] of segs) if (!keys.has(k)) { e.style.height = '0'; segs.delete(k); setTimeout(() => e.remove(), 260); }
    bar.classList.toggle('over', over);
    $('#screen').classList.toggle('over', over);
    bar.setAttribute('aria-label', 'Space used: ' + size(used) + ' of ' + size(cap));
    $('#screen').setAttribute('aria-label', 'Space on the calculator: ' + size(used) + ' of ' + size(cap) + ', each game a block its size');

    // the numbers
    const from = shownUsed, t0 = performance.now(), cancel = ++tween;
    const step = (now) => {
      if (cancel !== tween) return;
      const k = Math.min(1, (now - t0) / 220), e = 1 - Math.pow(1 - k, 3);
      shownUsed = from + (used - from) * e;
      $('#used').textContent = (shownUsed / MiB).toFixed(2);
      $('#status-line').textContent = (shownUsed / MiB).toFixed(2) + ' OF ' + (cap / MiB).toFixed(cap === XL_SPACE ? 1 : 2) + ' MB';
      $('#battery').style.width = 'calc(' + Math.min(100, 100 * shownUsed / cap) + '% - 2px)';
      if (k < 1) requestAnimationFrame(step);
    };
    requestAnimationFrame(step);

    // the icons
    for (const [id, b] of buttons) {
      const g = kit.games.find((x) => x.id === id), on = st.picked.has(id);
      b.classList.toggle('on', on);
      b.classList.toggle('off', !on && used + g.flash <= cap);
      b.classList.toggle('nofit', !on && used + g.flash > cap);
      b.setAttribute('aria-pressed', on);
      b.dataset.tip = g.title + '|' + size(g.flash);
    }
    for (const b of document.querySelectorAll('.cap button')) b.setAttribute('aria-checked', b.dataset.cap === st.cap);
    for (const b of document.querySelectorAll('.variants button')) b.setAttribute('aria-checked', b.dataset.v === st.variant);
    $('#brand-icon').src = KIT + 'variants/' + st.variant + '.png';
    renderCarts();
    save();
  }

  function renderCarts() {
    const box = $('#carts'), add = $('#add-rom');
    if (!rg) { box.hidden = true; return; }
    const have = new Set();
    for (const r of st.roms) {
      have.add('c' + r.key);
      if (box.querySelector('[data-k="c' + r.key + '"]')) continue;
      const s = SYS[r.sys];
      const c = el('button', 'cart enter', { type: 'button', 'data-k': 'c' + r.key, 'aria-label': 'Remove ' + r.name });
      c.style.background = s.shell;
      c.dataset.tip = r.name + '|' + s.name + ', ' + size(r.bytes.length);
      const lab = c.appendChild(el('span', 'label'));
      lab.style.background = s.color;
      lab.textContent = s.tag;
      c.appendChild(el('span', 'x')).innerHTML =
        '<svg viewBox="0 0 24 24" width="10" height="10" fill="none" stroke="currentColor" stroke-width="3.4" stroke-linecap="round"><path d="M6 6l12 12M18 6L6 18"/></svg>';
      c.addEventListener('click', () => removeRom(r));
      c.addEventListener('animationend', () => c.classList.remove('enter'));
      box.insertBefore(c, add);
    }
    for (const c of box.querySelectorAll('.cart[data-k]')) if (!have.has(c.dataset.k)) c.remove();
  }

  // ------------------------------------------------------------------ picking
  function bump(e, cls) {
    e.classList.remove('pop', 'shake');
    void e.offsetWidth;
    e.classList.add(cls);
    setTimeout(() => e.classList.remove(cls), 320);
  }
  function overflow(bytes) {
    toast("That doesn't fit: " + size(bytes) + ' too much');
    const bar = $('#bar');
    bar.classList.add('over');
    setTimeout(() => render(), 500);
  }
  function toggle(id) {
    const g = kit.games.find((x) => x.id === id), b = buttons.get(id);
    if (st.picked.has(id)) st.picked.delete(id);
    else {
      const over = usedBytes() + g.flash - capacity();
      if (over > 0) { if (b) bump(b, 'shake'); overflow(over); return; }
      st.picked.add(id);
      if (b) bump(b, 'pop');
      fetchObj('games/' + id + '.o');  // (ready by the time you install)
    }
    render();
  }

  async function addRoms(files) {
    const ok = new Set(rg && rg.systems ? rg.systems : [1, 2, 3]);
    for (const f of files) {
      const ext = (f.name.split('.').pop() || '').toLowerCase(), sys = EXT[ext];
      if (!sys || !ok.has(sys)) {
        toast('Retro-Go plays ' + [...ok].map((s) => SYS[s].name).join(', ').replace(/, ([^,]*)$/, ' and $1') + ' games');
        continue;
      }
      const bytes = new Uint8Array(await f.arrayBuffer());
      const r = { key: ++romKey, sys, bytes, name: romName(f.name) };
      const extra = (st.roms.length ? 0 : rg.flash + ROM_HEAD) + romBytes(r);
      const over = usedBytes() + extra - capacity();
      if (over > 0) { bump($('#add-rom'), 'shake'); overflow(over); continue; }
      st.roms.push(r);
      fetchObj('games/' + rg.id + '.o');
    }
    render();
  }
  function romName(file) {  // (its name without the extension, at most 40 bytes of UTF-8, no letter cut)
    const enc = new TextEncoder();
    let n = '';
    for (const ch of file.replace(/\.[^.]*$/, '')) {
      if (enc.encode(n + ch).length > 40) break;
      n += ch;
    }
    return n;
  }
  function removeRom(r) {
    st.roms = st.roms.filter((x) => x !== r);
    render();
  }

  // ------------------------------------------------------------------ remembering a pick
  function save() {
    const h = 'games=' + kit.games.filter((g) => st.picked.has(g.id)).map((g) => g.id).join(',') +
      (st.cap === 'xl' ? '&space=xl' : '') + (st.variant !== 'NumPlay' ? '&icon=' + st.variant : '');
    try { history.replaceState(null, '', '#' + h); } catch (e) { /* (a file:// page) */ }
    try { localStorage.setItem('numplay-studio', h); } catch (e) { /* (private browsing) */ }
  }
  function restore() {
    let h = location.hash.slice(1);
    if (!h) try { h = localStorage.getItem('numplay-studio') || ''; } catch (e) { h = ''; }
    const p = new URLSearchParams(h);
    if (p.has('games')) {
      const ids = new Set(p.get('games').split(',').filter(Boolean));
      for (const g of kit.games) if (ids.has(g.id) && g !== rg) st.picked.add(g.id);
    } else for (const g of kit.games) if (g.default) st.picked.add(g.id);
    if (p.get('space') === 'xl') st.cap = 'xl';
    if (kit.variants.includes(p.get('icon'))) st.variant = p.get('icon');
    // a pick that no longer fits the calculator (the kit grew): drop the biggest games until it does
    const order = kit.games.filter((g) => st.picked.has(g.id)).sort((a, b) => a.flash - b.flash);
    while (usedBytes() > capacity() && order.length) st.picked.delete(order.pop().id);
  }

  // ------------------------------------------------------------------ the kit
  const cache = new Map();
  function fetchBytes(path) {
    if (!cache.has(path))
      cache.set(path, fetch(path).then((r) => {
        if (!r.ok) throw new Error(path + ': ' + r.status);
        if (!path.endsWith('.gz')) return r.arrayBuffer().then((b) => new Uint8Array(b));
        return new Response(r.body.pipeThrough(new DecompressionStream('gzip'))).arrayBuffer().then((b) => new Uint8Array(b));
      }));
    const p = cache.get(path);
    p.catch(() => cache.delete(path));
    return p;
  }
  const fetchObj = (rel) => fetchBytes(KIT + rel + '.gz');

  let tools = null;
  function script(src) {
    return new Promise((ok, no) => {
      const s = document.createElement('script');
      s.src = src; s.onload = ok; s.onerror = () => no(new Error(src));
      document.head.appendChild(s);
    });
  }
  function loadTools() {
    if (!tools) tools = (async () => {
      const wasm = Promise.all([fetchBytes('ld/ld.wasm'), fetchBytes('ld/objcopy.wasm')]);
      await script('ld/ld.js');
      const ld = window.Module;
      await script('ld/objcopy.js');
      const oc = window.Module;
      const [lw, ow] = await wasm;
      return { ld: [ld, lw], oc: [oc, ow] };
    })().catch((e) => { tools = null; throw e; });
    return tools;
  }
  async function run([factory, wasm], args, files) {
    let log = '';
    const m = await factory({ wasmBinary: wasm, print: (s) => { log += s + '\n'; }, printErr: (s) => { log += s + '\n'; } });
    for (const [n, d] of Object.entries(files)) m.FS.writeFile(n, d);
    let code = 0;
    try { code = m.callMain(args); } catch (e) { if (e && typeof e.status === 'number') code = e.status; else throw e; }
    return { m, code, log };
  }

  // The ROM table Retro-Go reads (np_roms, in its own section): "NPRM", version 1, the count,
  // then 52 bytes per game (offset, size, system, 3 spare bytes, its name in 40), then the games.
  function romTable(roms) {
    let at = align4(ROM_HEAD + ROM_ENTRY * roms.length);
    const offs = roms.map((r) => { const o = at; at = align4(at + r.bytes.length); return o; });
    const d = new Uint8Array(at), v = new DataView(d.buffer), enc = new TextEncoder();
    v.setUint32(0, 0x4D52504E, true); v.setUint32(4, 1, true); v.setUint32(8, roms.length, true);
    roms.forEach((r, i) => {
      const e = ROM_HEAD + ROM_ENTRY * i;
      v.setUint32(e, offs[i], true); v.setUint32(e + 4, r.bytes.length, true); d[e + 8] = r.sys;
      d.set(enc.encode(r.name).slice(0, 40), e + 12);
      d.set(r.bytes, offs[i]);
    });
    return d;
  }
  // ...as a relocatable ARM object: one section, .rodata.np_roms, and the symbol np_roms (as
  // tools/romtable.py makes it, e_flags 0 like nwlink's icon objects)
  function romObject(table) {
    const shstr = '\0.rodata.np_roms\0.symtab\0.strtab\0.shstrtab\0', str = '\0np_roms\0';
    const enc = new TextEncoder(), shs = enc.encode(shstr), sts = enc.encode(str);
    const dataOff = 52, symOff = align4(dataOff + table.length), strOff = symOff + 48;
    const shsOff = strOff + sts.length, shOff = align4(shsOff + shs.length), total = shOff + 40 * 5;
    const b = new Uint8Array(total), v = new DataView(b.buffer);
    b.set([0x7f, 0x45, 0x4c, 0x46, 1, 1, 1], 0);
    v.setUint16(16, 1, true); v.setUint16(18, 40, true); v.setUint32(20, 1, true);
    v.setUint32(32, shOff, true);
    v.setUint16(40, 52, true); v.setUint16(46, 40, true); v.setUint16(48, 5, true); v.setUint16(50, 4, true);
    b.set(table, dataOff);
    // symbols: none, the section, np_roms (global object)
    v.setUint8(symOff + 16 + 12, 3); v.setUint16(symOff + 16 + 14, 1, true);
    v.setUint32(symOff + 32, 1, true); v.setUint32(symOff + 32 + 8, table.length, true);
    v.setUint8(symOff + 32 + 12, 0x11); v.setUint16(symOff + 32 + 14, 1, true);
    b.set(sts, strOff); b.set(shs, shsOff);
    const sh = (i, name, type, flags, off, sz, link, info, al, es) => {
      const o = shOff + 40 * i;
      [name, type, flags, 0, off, sz, link, info, al, es].forEach((x, k) => v.setUint32(o + 4 * k, x, true));
    };
    sh(1, shstr.indexOf('.rodata.np_roms'), 1, 2, dataOff, table.length, 0, 0, 4, 0);
    sh(2, shstr.indexOf('.symtab'), 2, 0, symOff, 48, 3, 2, 4, 16);
    sh(3, shstr.indexOf('.strtab'), 3, 0, strOff, sts.length, 0, 0, 1, 0);
    sh(4, shstr.indexOf('.shstrtab'), 3, 0, shsOff, shs.length, 0, 0, 1, 0);
    return b;
  }

  // Bytes the installer writes for an .nwa (tools/kit.py's installed_size), less its own start code.
  function installedSize(nwa) {
    const v = new DataView(nwa.buffer, nwa.byteOffset, nwa.byteLength);
    const shoff = v.getUint32(32, true), shnum = v.getUint16(48, true), shstrndx = v.getUint16(50, true);
    const sh = (i) => { const o = shoff + 40 * i; return [0, 1, 2, 3, 4, 5, 6, 7, 8, 9].map((k) => v.getUint32(o + 4 * k, true)); };
    const strs = sh(shstrndx), dec = new TextDecoder();
    const name = (n) => { let e = strs[4] + n; while (nwa[e]) e++; return dec.decode(nwa.subarray(strs[4] + n, e)); };
    const secs = [];
    for (let i = 0; i < shnum; i++) { const h = sh(i); if (h[2] & 2 && h[1] !== 8) secs.push([name(h[0]), h[5], Math.max(h[8], 1)]); }
    const group = (n) => n === '.rodata.eadk_app_name' ? 0 : n === '.rodata.eadk_app_icon' ? 1 :
      n === '.text' || n.startsWith('.text.') ? 2 : n === '.rodata.eadk_api_level' ? -1 :
      n === '.data' || n.startsWith('.data.') ? 4 : 3;
    let at = 32;
    for (let g = 0; g < 5; g++) {
      if (g === 2 || g === 4) at = align4(at);
      for (const [n, sz, al] of secs) if (group(n) === g) at = ((at + al - 1) & ~(al - 1)) + sz;
    }
    return at;
  }

  async function build() {
    const sel = kit.games.filter((g) => st.picked.has(g.id) && g !== rg).map((g) => g.id);
    const roms = rg && st.roms.length;
    if (roms) sel.push(rg.id);
    const need = ['base.o', 'arena.o', 'variants/' + st.variant + '-name.o', 'variants/' + st.variant + '-icon.o'].concat(sel.map((id) => 'games/' + id + '.o'));
    const [t, ...objs] = await Promise.all([loadTools()].concat(need.map(fetchObj)));
    const files = { 'base.o': objs[0], 'arena.o': objs[1], 'name.o': objs[2], 'icon.o': objs[3] };
    sel.forEach((id, i) => { files[id + '.o'] = objs[4 + i]; });
    const args = kit.link.concat(['-T', 'numplay.ld', 'base.o', 'arena.o', 'name.o', 'icon.o'], sel.map((id) => id + '.o'));
    if (roms) {
      files['roms.o'] = romObject(romTable(st.roms));
      args.push('roms.o');
    }
    files['numplay.ld'] = kit.script.head + sel.map((id) => kit.script.blocks[id]).join('') + kit.script.tail;
    const l = await run(t.ld, args.concat(['-o', 'out.nwa']), files);
    if (l.code) throw new Error(l.log || 'ld ' + l.code);
    const s = await run(t.oc, ['--strip-unneeded', 'in.nwa', 'out.nwa'], { 'in.nwa': l.m.FS.readFile('out.nwa') });
    if (s.code) throw new Error(s.log || 'objcopy ' + s.code);
    const nwa = s.m.FS.readFile('out.nwa');
    const n = installedSize(nwa) + kit.runtime;
    if (n > capacity()) throw new Error("That's " + size(n - capacity()) + ' more than the calculator holds');
    return nwa;
  }

  function download(bytes) {
    const a = el('a', '', { download: 'NumPlay.nwa', href: URL.createObjectURL(new Blob([bytes], { type: 'application/octet-stream' })) });
    document.body.appendChild(a);
    a.click();
    setTimeout(() => { URL.revokeObjectURL(a.href); a.remove(); }, 2000);
  }
  async function go(btn, sheet) {
    if (btn.classList.contains('busy')) return;
    btn.classList.add('busy');
    try {
      download(await build());
      if (sheet) openSheet();
    } catch (e) {
      console.error(e);
      toast(/more than/.test(e.message) ? e.message : "Couldn't make NumPlay.nwa, try again");
    } finally {
      btn.classList.remove('busy');
    }
  }
  function prefetch() {
    loadTools().catch(() => {});
    for (const p of ['base.o', 'arena.o', 'variants/' + st.variant + '-name.o', 'variants/' + st.variant + '-icon.o']) fetchObj(p).catch(() => {});
    for (const id of st.picked) fetchObj('games/' + id + '.o').catch(() => {});
    if (st.roms.length) fetchObj('games/' + rg.id + '.o').catch(() => {});
  }

  // ------------------------------------------------------------------ the sheet, tips and toasts
  function openSheet() {
    const s = $('#sheet');
    for (const li of s.querySelectorAll('.xl-only')) li.hidden = st.cap !== 'xl';
    s.hidden = false;
    s.querySelector('.sheet-card a.install').focus();
  }
  function closeSheet() { $('#sheet').hidden = true; }
  let toastTimer = 0;
  function toast(msg) {
    const t = $('#toast');
    t.textContent = msg;
    t.classList.add('show');
    clearTimeout(toastTimer);
    toastTimer = setTimeout(() => t.classList.remove('show'), 2600);
  }
  function tips() {
    const tip = $('#tip');
    let on = null;
    document.addEventListener('pointerover', (e) => {
      if (e.pointerType === 'touch') return;
      const t = e.target.closest('[data-tip]');
      if (t === on) return;
      on = t;
      if (!t) { tip.classList.remove('show'); return; }
      const [a, b] = t.dataset.tip.split('|');
      tip.textContent = a;
      if (b) tip.appendChild(el('small')).textContent = b;
      const r = t.getBoundingClientRect(), w = tip.offsetWidth;
      const x = Math.max(8, Math.min(innerWidth - w - 8, r.left + r.width / 2 - w / 2));
      const y = r.top - tip.offsetHeight - 8 < 8 ? r.bottom + 8 : r.top - tip.offsetHeight - 8;
      tip.style.left = x + 'px';
      tip.style.top = y + 'px';
      tip.classList.add('show');
    });
    document.addEventListener('pointerdown', () => tip.classList.remove('show'));
  }

  // ------------------------------------------------------------------ start
  async function start() {
    try {
      kit = await fetch(KIT + 'kit.json').then((r) => { if (!r.ok) throw new Error(r.status); return r.json(); });
    } catch (e) {
      toast("The games didn't load, try again in a minute");
      return;
    }
    rg = kit.games.find((g) => g.roms) || null;
    const small = $('#small'), big = $('#big');
    for (const g of kit.games) {
      if (g === rg) continue;
      const b = el('button', 'app off', { type: 'button', 'aria-label': g.title, 'aria-pressed': 'false' });
      b.appendChild(el('img', '', { src: KIT + 'icons/' + g.id + '.png', alt: '', draggable: 'false' }));
      b.addEventListener('click', () => toggle(g.id));
      (g.xl ? big : small).appendChild(b);
      buttons.set(g.id, b);
    }
    const vs = $('#variants');
    for (const v of kit.variants) {
      const b = el('button', '', { type: 'button', role: 'radio', 'data-v': v, 'aria-label': VARIANT_NAMES[v] || v, 'data-tip': VARIANT_NAMES[v] || v });
      b.appendChild(el('img', '', { src: KIT + 'variants/' + v + '.png', alt: '' }));
      b.addEventListener('click', () => { st.variant = v; render(); });
      vs.appendChild(b);
    }
    for (const b of document.querySelectorAll('.cap button'))
      b.addEventListener('click', () => {
        if (b.dataset.cap === 'std' && st.cap === 'xl') {
          const over = usedBytes() - kit.appSpace;
          if (over > 0) { bump(b, 'shake'); toast('Take ' + size(over) + ' of games out first'); return; }
        }
        st.cap = b.dataset.cap;
        render();
      });
    $('#install').addEventListener('click', (e) => go(e.currentTarget, true));
    $('#download').addEventListener('click', (e) => go(e.currentTarget, false));
    for (const b of [$('#install'), $('#download')]) {
      b.addEventListener('pointerenter', prefetch, { once: true });
      b.addEventListener('focus', prefetch, { once: true });
    }
    $('#rom-file').addEventListener('change', (e) => { addRoms([...e.target.files]); e.target.value = ''; });
    $('#sheet').addEventListener('click', (e) => { if (e.target.id === 'sheet' || e.target.closest('.sheet-close')) closeSheet(); });
    document.addEventListener('keydown', (e) => { if (e.key === 'Escape') closeSheet(); });
    if (rg) {
      let depth = 0;
      const files = (e) => e.dataTransfer && [...e.dataTransfer.types].includes('Files');
      addEventListener('dragenter', (e) => { if (files(e)) { depth++; document.body.classList.add('dragging'); e.preventDefault(); } });
      addEventListener('dragover', (e) => { if (files(e)) e.preventDefault(); });
      addEventListener('dragleave', () => { if (--depth <= 0) { depth = 0; document.body.classList.remove('dragging'); } });
      addEventListener('drop', (e) => {
        if (!files(e)) return;
        e.preventDefault();
        depth = 0;
        document.body.classList.remove('dragging');
        addRoms([...e.dataTransfer.files]);
      });
    }
    tips();
    restore();
    render();
    addEventListener('hashchange', () => { st.picked.clear(); st.cap = 'std'; st.variant = 'NumPlay'; restore(); render(); });
    $('#studio').setAttribute('aria-busy', 'false');
    (window.requestIdleCallback || setTimeout)(() => loadTools().catch(() => {}), { timeout: 3000 });
  }
  start();
})();
