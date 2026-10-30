/* NumPlay Studio: pick games, add Retro-Go games, and link a NumPlay.nwa of your own in the
 * browser. The kit (kit/, made by `make kit` and `make kit-langs`, tools/kit.py) holds the objects
 * the Makefile links NumPlay.nwa from; GNU ld and objcopy, built as WebAssembly (ld/), link them
 * here the same way. */
'use strict';
(() => {
  const KIT = 'kit/';
  const MEDIA = '../docs/media/';  // the games' clips (kit.json: previews/ or gallery/)
  const MiB = 1048576;
  // The calculator's space for apps (kit.json "spaces"): from where its software ends, 0x90200000 or, on Epsilon 26,
  // 0x90180000, to 0x903F0000; with Enlarged Extra Apps (nwagyu's Nwagra) on to the end of the flash, 0x90800000
  const SPACES = { std: 'Every NumWorks', e26: 'Epsilon 26', xl: 'Nwagra', xl26: 'Nwagra on Epsilon 26' };
  const XL_SPACE = 6 * MiB;  // (kits from before kit.json had spaces)
  const SYS = {
    1: { tag: 'GB', name: 'Game Boy', color: '#6d8a0c', shell: '#c9c9cf' },
    2: { tag: 'GBC', name: 'Game Boy Color', color: '#6a3fb5', shell: '#b9a6d8' },
    3: { tag: 'NES', name: 'NES', color: '#c43a2f', shell: '#d9d9de' },
    4: { tag: 'SMS', name: 'Master System', color: '#2f62c4', shell: '#cfd6e4' },
    5: { tag: 'GG', name: 'Game Gear', color: '#3b3b3b', shell: '#c8c8cc' },
  };
  const EXT = { gb: 1, gbc: 2, nes: 3, sms: 4, gg: 5 };
  const ROM_ENTRY = 52, ROM_HEAD = 12;
  const PRESETS = { 'NumPlay': ['NumPlay', 'NumPlay'], 'NumPlay-Invisible': ['Invisible', ' '], 'NumPlay-Matrices': ['Matrices', 'Matrices'] };
  const LANGS = { en: ['EN', 'English'], fr: ['FR', 'Français'], zh: ['中文', '中文'] };
  // launcher/src/settings.c's secrets, in order (NP_SECRET_*)
  const SECRETS = ['x,n,t key', 'var key', 'Toolbox key', 'π key', '√ key', 'Menu: Examples', 'Konami code'];
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
  const wait = (ms) => new Promise((r) => setTimeout(r, ms));
  const store = (k, v) => { try { if (v === undefined) return localStorage.getItem(k); localStorage.setItem(k, v); } catch (e) { return null; } return null; };

  let kit = null, rg = null;
  const kits = {};         // the kit of each language, loaded when picked
  const byId = {};
  const st = {
    picked: new Set(), order: [], roms: [], nwas: [], cap: 'std', lang: 'en',
    app: { icon: 'NumPlay', name: 'NumPlay', open: 'numplay', secret: 0, hint: true },
  };
  let custom = null;       // an icon of your own: { url, obj }
  let romKey = 0, nwaKey = 0;

  // ------------------------------------------------------------------ sizes
  const spaces = () => kit.spaces && kit.spaces.length ? kit.spaces :
    [{ id: 'std', size: kit.appSpace }, { id: 'xl', size: kit.xlAppSpace || XL_SPACE }];
  const space = () => spaces().find((s) => s.id === st.cap) || spaces()[0];
  const capacity = () => space().size;
  const nwagra = () => st.cap.startsWith('xl');
  const mb = (b) => +(b / MiB).toFixed(2) + ' MB';
  const langKit = () => kits[st.lang] || kit;
  const game = (id) => (langKit().byId || {})[id] || byId[id];  // (as the app's language has it)
  const flash = (id) => game(id).flash;
  const romBytes = (r) => ROM_ENTRY + align4(r.bytes.length);
  // your own .nwa games: each becomes a game of NumPlay, with its card (see importNwas)
  const MAXN = 100, NL = 29;
  const nwaBytes = () => st.nwas.reduce((a, g) => a + g.flash, 0);
  const needNP = () => true;
  const nwaHue = (g) => { let h = 0; for (const c of g.name + g.key) h = (h * 31 + c.charCodeAt(0)) % 360; return h; };
  const nwaColor = (g) => {  // hsl(h 62% 46%) as #rrggbb
    const h = nwaHue(g), a = 0.62 * Math.min(0.46, 1 - 0.46);
    const f = (n) => { const k = (n + h / 30) % 12; return Math.round(255 * (0.46 - a * Math.max(-1, Math.min(k - 3, 9 - k, 1)))); };
    return '#' + [f(0), f(8), f(4)].map((v) => v.toString(16).padStart(2, '0')).join('');
  };
  // what an object takes in flash once linked: its loaded sections, not .bss
  function flashSize(b) {
    const v = new DataView(b.buffer, b.byteOffset, b.byteLength);
    const shoff = v.getUint32(32, true), shsz = v.getUint16(46, true), shn = v.getUint16(48, true);
    let t = 0;
    for (let i = 0; i < shn; i++) {
      const o = shoff + i * shsz;
      if ((v.getUint32(o + 8, true) & 2) && v.getUint32(o + 4, true) !== 8) t += align4(v.getUint32(o + 20, true));
    }
    return t;
  }
  const iconUrl = () => st.app.icon === 'custom' && custom ? custom.url : KIT + 'variants/' + st.app.icon + '.png';
  const ordered = () => st.order.filter((id) => st.picked.has(id));
  // NumPlay's own bytes: with the library functions NumPlay's games call, or only the launcher when it has big
  // games only (they bring their own)
  const own = (ids, roms) => {
    const k = langKit();
    return st.nwas.length || ids.some((id) => !byId[id].xl) || (rg && roms.length) ? k.base : k.baseAlone || k.base;
  };
  // what a NumPlay of these games (and Game Boy, NES and Sega games) takes
  function total(ids, roms = st.roms) {
    let n = needNP(ids, roms) ? own(ids, roms) : 0;
    for (const id of ids) n += flash(id);
    if (rg && roms.length) n += flash(rg.id) + ROM_HEAD + roms.reduce((a, r) => a + romBytes(r), 0);
    return n + nwaBytes();
  }
  // a big game too big to go beside the launcher in this space (kit.json "alone") goes on its own there,
  // as its own app: as if you installed just that game
  const alone = (id) => !!game(id).alone && own([id], []) + flash(id) > capacity();
  const solo = () => ordered().find(alone) || null;
  let before = null;  // your games while one of those is picked, back when it's taken out
  function items() {
    const s = solo();
    if (s) {
      const g = byId[s];
      return [{ key: s, id: s, name: g.title, bytes: game(s).alone.size, color: hex(g.top), icon: g.icon ? KIT + 'icons/' + s + '.png' : null }];
    }
    const out = !needNP(ordered(), st.roms) ? [] : [{ key: '_core', name: st.app.name.trim() || 'NumPlay', bytes: own(ordered(), st.roms), color: '#3a3a3c', icon: iconUrl() }];
    for (const id of ordered()) {
      const g = byId[id];
      out.push({ key: id, id, name: g.title, bytes: flash(id), color: hex(g.top), icon: g.icon ? KIT + 'icons/' + id + '.png' : null });
    }
    if (rg && st.roms.length) {
      out.push({ key: rg.id, id: rg.id, name: rg.title, bytes: flash(rg.id) + ROM_HEAD, color: hex(rg.top), icon: rg.icon ? KIT + 'icons/' + rg.id + '.png' : null });
      for (const r of st.roms)
        out.push({ key: 'rom' + r.key, rom: r, name: r.name, bytes: romBytes(r), color: SYS[r.sys].color, tag: SYS[r.sys].tag });
    }
    for (const g of st.nwas)
      out.push({ key: 'nwa' + g.key, name: g.name, bytes: g.flash, color: hex(g.theme[0]), icon: g.iconUrl, tag: g.name.replace(/[^A-Za-z0-9]/g, '').slice(0, 3).toUpperCase() });
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
  let shownUsed = 0, previewing = null;

  function render() {
    const list = items(), cap = capacity();
    const used = list.reduce((s, it) => s + it.bytes, 0);
    const over = used > cap;

    // the screen
    const free = cap - used;
    const all = free > cap * 0.002 ? list.concat([{ key: '_free', name: 'Free', bytes: free, free: true }]) : list;
    const laid = treemap(all, Math.max(cap, used));
    const map = $('#map'), seen = new Set();
    laid.forEach((t) => {
      let e = tiles.get(t.key);
      const fresh = !e;
      if (fresh) {
        e = el('div', 'tile');
        if (t.icon) e.appendChild(el('img', '', { src: t.icon, alt: '', draggable: 'false' }));
        if (t.tag) e.appendChild(el('b')).textContent = t.tag;
        map.appendChild(e);
        tiles.set(t.key, e);
        if (t.id || t.rom) e.addEventListener('click', () => (t.rom ? removeRom(t.rom) : toggle(t.id)));
      }
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
        if (t.key === '_core' && img.getAttribute('src') !== t.icon) img.src = t.icon;
        img.style.width = (100 * Math.min(44, s * 0.62) / Math.max(t.w, 1)) + '%';
        img.style.opacity = s >= 13 ? 1 : 0;
      }
      const b = e.querySelector('b');
      if (b) b.style.opacity = s >= 14 ? 1 : 0;
      e.dataset.tip = (t.free ? 'Free' : t.name) + '|' + size(t.bytes);
      seen.add(t.key);
    });
    for (const [k, e] of tiles) if (!seen.has(k)) { e.remove(); tiles.delete(k); }

    // the bar beside it: the same blocks, stacked in the launcher's order
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
    const want = list.map((it) => segs.get(it.key));
    if ([...bar.children].filter((c) => want.includes(c)).some((c, i) => c !== want[i])) for (const c of want) bar.appendChild(c);
    bar.classList.toggle('over', over);
    $('#used').textContent = (used / MiB).toFixed(2);
    $('#cap-of').textContent = 'of ' + +(cap / MiB).toFixed(2) + ' MB';

    $('#screen').classList.toggle('over', over);
    $('#screen').setAttribute('aria-label', 'Space on the calculator: ' + size(used) + ' of ' + size(cap) + ', each game a block its size');

    // the numbers
    shownUsed = used;
    if (!previewing) $('#status-line').textContent = (used / MiB).toFixed(2) + ' OF ' + +(cap / MiB).toFixed(2) + ' MB';
    $('#battery').style.width = 'calc(' + Math.min(100, 100 * used / cap) + '% - 2px)';

    // the icons, in the order they go in the launcher
    const s0 = solo(), picks = ordered();
    for (const [id, b] of buttons) {
      const g = byId[id], on = st.picked.has(id);
      const fits = alone(id) || (!s0 && total(picks.concat([id])) <= cap);
      b.classList.toggle('on', on);
      b.classList.toggle('off', !on && fits);
      b.classList.toggle('nofit', !on && !fits);
      b.setAttribute('aria-pressed', on);
      b.dataset.tip = g.title + '|' + (alone(id) ? size(game(id).alone.size) + ', on its own' : size(flash(id)) +
        (g.xl && st.lang !== 'en' ? ' · in English' : '') + ' · drag to move');
    }
    for (const box of [$('#small'), $('#big')]) {
      const want2 = st.order.map((id) => buttons.get(id)).filter((b) => b && b.parentNode === box);
      if ([...box.children].some((c, i) => c !== want2[i])) for (const b of want2) box.appendChild(b);
    }
    $('#space-chip-label').textContent = mb(cap);
    $('#space-chip').dataset.tip = SPACES[space().id] + '|your calculator\'s space for apps';
    $('#dock-used').textContent = (used / MiB).toFixed(2) + ' of ' + mb(cap);
    $('#dock').classList.toggle('over', over);
    // (a game on its own has its own icon, name and language)
    const s = solo(), file = s ? game(s).alone.file : 'NumPlay.nwa';
    for (const c of [$('#app-chip'), $('#lang-chip')]) c.disabled = !!s;
    $('#download').setAttribute('aria-label', 'Download ' + file);
    $('#download').dataset.tip = 'Download ' + file;
    $('#dock-download').setAttribute('aria-label', 'Download ' + file);
    $('#app-chip-icon').src = iconUrl();
    $('#app-chip-icon').classList.toggle('custom', st.app.icon === 'custom');
    $('#app-chip').dataset.tip = (s ? byId[s].title : st.app.name.trim() || 'No name') + '|its icon and name on your calculator, and how it opens';
    $('#lang-chip').dataset.tip = LANGS[st.lang][1] + '|the language of its menus';
    $('#lang-chip-label').textContent = LANGS[st.lang][0];
    renderCarts();
    save();
  }

  function renderCarts() {
    renderNwaCarts();
    const box = $('#carts');
    const have = new Set();
    for (const r of st.roms) {
      have.add('c' + r.key);
      if (box.querySelector('[data-k="c' + r.key + '"]')) continue;
      const s = SYS[r.sys];
      const c = el('button', 'cart enter', { type: 'button', 'data-k': 'c' + r.key, 'aria-label': 'Remove ' + r.name });
      c.addEventListener('animationend', () => c.classList.remove('enter'), { once: true });
      c.style.background = s.shell;
      c.dataset.tip = r.name + '|' + s.name + ', ' + size(r.bytes.length);
      const lab = c.appendChild(el('span', 'label'));
      lab.style.background = s.color;
      lab.textContent = s.tag;
      c.appendChild(el('span', 'x')).innerHTML =
        '<svg viewBox="0 0 24 24" width="10" height="10" fill="none" stroke="currentColor" stroke-width="3.4" stroke-linecap="round"><path d="M6 6l12 12M18 6L6 18"/></svg>';
      c.addEventListener('click', () => removeRom(r));
      box.appendChild(c);
    }
    for (const c of box.querySelectorAll('.cart[data-k]')) if (!have.has(c.dataset.k)) c.remove();
  }

  // ------------------------------------------------------------------ the game on the screen
  let previewTimer = 0;
  function showPreview(id) {
    const g = byId[id];
    if (!g || !g.preview || dragging) return;
    clearTimeout(previewTimer);
    previewing = id;
    const p = $('#preview'), img = p.querySelector('img');
    const src = MEDIA + (g.preview === true ? 'gallery/' + id + '.gif' : g.preview);
    if (img.getAttribute('src') !== src) img.src = src;
    p.classList.add('show');
    $('#status-line').textContent = g.title.toUpperCase();
  }
  function hidePreview(later) {
    clearTimeout(previewTimer);
    const go = () => {
      previewing = null;
      $('#preview').classList.remove('show');
      $('#status-line').textContent = (shownUsed / MiB).toFixed(2) + ' OF ' + +(capacity() / MiB).toFixed(2) + ' MB';
    };
    if (later) previewTimer = setTimeout(go, later); else go();
  }

  // ------------------------------------------------------------------ picking
  // the icon's answer to a press: up with a tick, back down, or no
  function bump(b, cls) {
    if (!b) return;
    b.classList.remove('pop-on', 'pop-off', 'shake');
    void b.offsetWidth;  // (restarts it)
    b.classList.add(cls);
    b.addEventListener('animationend', () => b.classList.remove(cls), { once: true });
  }
  function overflow(bytes) {
    toast("That doesn't fit: " + size(bytes) + ' too much' + (st.cap === 'std' ? '. Epsilon 26 and Nwagra give more space' : ''));
    $('#screen').classList.add('over');
    setTimeout(() => render(), 500);
  }
  function toggle(id, quiet) {
    const s = solo(), b = quiet ? null : buttons.get(id);
    if (st.picked.has(id)) {
      st.picked.delete(id);
      bump(b, 'pop-off');
      if (alone(id) && before) { st.picked = new Set(before); before = null; }
    } else if (alone(id)) {
      if (!s) before = ordered();
      st.picked = new Set([id]);
      bump(b, 'pop-on');
      if (!quiet) {
        toast(byId[id].title + ' fills the whole calculator, so it goes on its own. Pick it again to get your games back');
        fetchBytes(KIT + 'apps/' + id + '.nwa.gz').catch(() => {});
      }
    } else {
      if (s) { if (!quiet) { bump(b, 'shake'); toast(byId[s].title + ' takes the whole calculator. Pick it again to take it out'); } return; }
      const over = total(ordered().concat([id])) - capacity();
      if (over > 0) { if (!quiet) { bump(b, 'shake'); overflow(over); } return; }
      st.picked.add(id);
      bump(b, 'pop-on');
      if (!quiet) fetchObj(objPath(id)).catch(() => {});  // (ready by the time you install)
    }
    render();
  }

  // (the page's own text says the .nwa go into a second app, Multijeux: they go into NumPlay)
  function niFixText() {
    const fix = () => {
      const w = document.createTreeWalker(document.body, NodeFilter.SHOW_TEXT);
      for (let n = w.nextNode(); n; n = w.nextNode())
        if (/Multijeux|deuxi[eè]me application|second app/i.test(n.nodeValue)) n.nodeValue = st.lang === 'fr' ? '.nwa · dans NumPlay, avec les autres jeux' : '.nwa · inside NumPlay, with the other games';
    };
    fix();
    new MutationObserver(fix).observe(document.body, { childList: true, subtree: true, characterData: true });
  }
  function renderNwaCarts() {
    const box = $('#nwa-carts');
    box.textContent = '';
    for (const g of st.nwas) {
      const c = el('button', 'cart', { type: 'button', 'aria-label': 'Remove ' + g.name });
      c.style.background = '#3a3a40';
      c.dataset.tip = g.name + '|' + size(g.flash) + ' · double-click to rename';
      const lab = c.appendChild(el('span', 'label'));
      lab.style.background = g.iconUrl ? 'url(' + g.iconUrl + ') center / cover' : hex(g.theme[0]);
      lab.style.imageRendering = 'pixelated';
      lab.textContent = g.iconUrl ? '' : g.name.replace(/[^A-Za-z0-9]/g, '').slice(0, 3).toUpperCase();
      c.appendChild(el('span', 'x')).innerHTML =
        '<svg viewBox="0 0 24 24" width="10" height="10" fill="none" stroke="currentColor" stroke-width="3.4" stroke-linecap="round"><path d="M6 6l12 12M18 6L6 18"/></svg>';
      c.addEventListener('click', () => { st.nwas = st.nwas.filter((x) => x !== g); render(); });
      c.addEventListener('dblclick', () => {
        const n = prompt('Name in NumPlay', g.name);
        if (n && n.trim()) { g.name = n.trim().slice(0, NL - 1); render(); }
      });
      box.appendChild(c);
    }
  }
  async function addNwas(files) {
    if (solo()) { bump($('#nwa-drop'), 'shake'); toast(byId[solo()].title + ' takes the whole calculator, so there is no room for .nwa games'); return; }
    for (const f of files) {
      if (st.nwas.length >= MAXN) { toast(MAXN + ' games at most'); break; }
      const bytes = new Uint8Array(await f.arrayBuffer());
      if (bytes.length < 52 || bytes[0] !== 0x7f || bytes[1] !== 0x45 || bytes[2] !== 0x4c || bytes[3] !== 0x46 || bytes[16] !== 1) {
        toast(f.name + " isn't an object .nwa (ELF relocatable), skipped"); continue;
      }
      let info;
      try { info = niInfo(bytes); } catch (x) { toast(f.name + ' ' + x.message + ', skipped'); continue; }
      if (!info.main) { toast(f.name + ' has no main, skipped'); continue; }
      const base = f.name.replace(/\.nwa$/i, '').replace(/[_-]/g, ' ').replace(/"/g, '');
      const name = (/^[\x20-\x7e]{2,}$/.test(info.title) ? info.title : base).slice(0, NL - 1);
      if (st.nwas.some((g) => g.name === name && g.bytes.length === bytes.length)) continue;
      const g = { key: ++nwaKey, name, bytes, flash: flashSize(bytes) + 4096, rgba: info.rgba, iconUrl: niIconUrl(info.rgba) };
      g.theme = niTheme(info.rgba, g.key);
      st.nwas.push(g);
    }
    render();
  }
  async function addRoms(files) {
    const nw = files.filter((f) => /\.nwa$/i.test(f.name));
    if (nw.length) { files = files.filter((f) => !/\.nwa$/i.test(f.name)); await addNwas(nw); if (!files.length) return; }
    if (solo()) { bump($('#drop'), 'shake'); toast(byId[solo()].title + " takes the whole calculator, so there's no room for Retro-Go"); return; }
    const ok = new Set(rg && rg.systems ? rg.systems : [1, 2, 3]);
    for (const f of files) {
      const ext = (f.name.split('.').pop() || '').toLowerCase(), sys = EXT[ext];
      if (!sys || !ok.has(sys)) {
        toast('Retro-Go plays ' + [...ok].map((s) => SYS[s].name).join(', ').replace(/, ([^,]*)$/, ' and $1') + ' games');
        continue;
      }
      const bytes = new Uint8Array(await f.arrayBuffer());
      const r = { key: ++romKey, sys, bytes, name: romName(f.name) };
      const over = total(ordered(), st.roms.concat([r])) - capacity();
      if (over > 0) { bump($('#drop'), 'shake'); overflow(over); continue; }
      st.roms.push(r);
      fetchObj(objPath(rg.id)).catch(() => {});
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

  // ------------------------------------------------------------------ moving games around
  // Drag an icon to change where its game sits in the launcher; a click still picks it. On a
  // phone, hold an icon, then tap where it goes.
  let dragging = null, justDragged = false, holding = null;
  function flip(box, change) {
    const kids = [...box.children], before = new Map(kids.map((k) => [k, k.getBoundingClientRect()]));
    change();
    for (const k of kids) {
      const a = before.get(k), b = k.getBoundingClientRect();
      const dx = a.left - b.left, dy = a.top - b.top;
      if (!dx && !dy) continue;
      k.style.transition = 'none';
      k.style.translate = dx + 'px ' + dy + 'px';
      requestAnimationFrame(() => { k.style.transition = ''; k.style.translate = ''; });
    }
  }
  function commitOrder() {
    const small = [...$('#small').children].map((b) => b.dataset.id), big = [...$('#big').children].map((b) => b.dataset.id);
    st.order = small.concat(big, st.order.filter((id) => !small.includes(id) && !big.includes(id)));
    render();
  }
  // The slot under the pointer comes from where the slots were when the drag began (they don't
  // move: only which icon sits in them), so an icon sliding away can never send it back.
  function sortable(box) {
    box.addEventListener('pointerdown', (e) => {
      const b = e.target.closest('.app');
      if (!b || e.button) return;
      if (e.pointerType === 'touch') {
        if (holding === b) { b.classList.remove('held'); holding = null; justDragged = true; return; }
        if (holding && holding !== b) {  // (a phone: the held icon goes where you tap)
          const from = holding;
          holding.classList.remove('held'); holding = null; justDragged = true;
          if (from.parentNode === box) flip(box, () => box.insertBefore(from, [...box.children].indexOf(from) < [...box.children].indexOf(b) ? b.nextSibling : b));
          commitOrder();
          return;
        }
        const t = setTimeout(() => { holding = b; b.classList.add('held'); justDragged = true; toast('Now tap where it goes'); }, 450);
        const stop = () => { clearTimeout(t); removeEventListener('pointerup', stop); removeEventListener('pointercancel', stop); };
        addEventListener('pointerup', stop); addEventListener('pointercancel', stop);
        return;
      }
      dragging = { b, box, x: e.clientX, y: e.clientY, on: false };
    });
    let frame = 0;
    addEventListener('pointermove', (e) => {
      const d = dragging;
      if (!d || d.box !== box) return;
      if (!d.on) {
        if (Math.hypot(e.clientX - d.x, e.clientY - d.y) < 6) return;
        d.on = true;
        hidePreview();
        $('#tip').classList.remove('show');
        d.slots = [...box.children].map((k) => { const r = k.getBoundingClientRect(); return [r.left + r.width / 2, r.top + r.height / 2]; });
        const r = d.b.getBoundingClientRect();
        d.dx = d.x - r.left; d.dy = d.y - r.top;
        d.ghost = el('div', 'drag-ghost');
        d.ghost.appendChild(el('img', '', { src: d.b.querySelector('img').src, alt: '' }));
        d.ghost.style.width = r.width + 'px';
        document.body.appendChild(d.ghost);
        d.b.classList.add('placeholder');
        document.body.classList.add('dragging-icon');
      }
      d.px = e.clientX; d.py = e.clientY;
      if (frame) return;
      frame = requestAnimationFrame(() => {  // (once a frame: the pointer can move faster than the screen)
        frame = 0;
        if (!dragging || !d.on) return;
        d.ghost.style.transform = 'translate(' + (d.px - d.dx) + 'px,' + (d.py - d.dy) + 'px) scale(1.1) rotate(-4deg)';
        let best = 0, bd = Infinity;
        d.slots.forEach(([x, y], i) => { const dd = (x - d.px) ** 2 + (y - d.py) ** 2; if (dd < bd) { bd = dd; best = i; } });
        const kids = [...box.children], now = kids.indexOf(d.b);
        if (best !== now) flip(box, () => box.insertBefore(d.b, best > now ? kids[best].nextSibling : kids[best]));
      });
    });
    const end = () => {
      const d = dragging;
      if (!d || d.box !== box) return;
      dragging = null;
      if (!d.on) return;
      justDragged = true;
      setTimeout(() => { justDragged = false; }, 0);
      const r = d.b.getBoundingClientRect(), g = d.ghost;
      g.style.transition = 'transform 160ms cubic-bezier(0.2, 0.9, 0.25, 1)';  // (into its place)
      g.style.transform = 'translate(' + r.left + 'px,' + r.top + 'px) scale(1) rotate(0deg)';
      setTimeout(() => { g.remove(); d.b.classList.remove('placeholder'); }, 170);
      document.body.classList.remove('dragging-icon');
      commitOrder();
    };
    addEventListener('pointerup', end);
    addEventListener('pointercancel', end);
  }

  // ------------------------------------------------------------------ remembering a pick
  function save() {
    const a = st.app, p = new URLSearchParams();
    p.set('games', ordered().join(','));
    if (st.cap !== 'std') p.set('space', st.cap);
    if (st.lang !== 'en') p.set('lang', st.lang);
    if (a.icon !== 'NumPlay') p.set('icon', a.icon);
    if (a.name !== (PRESETS[a.icon] || [0, 'NumPlay'])[1]) p.set('name', a.name);
    if (a.open === 'matrices') p.set('open', 'matrices' + (a.secret ? '-' + a.secret : '') + (a.hint ? '' : '-nohint'));
    const h = p.toString();
    try { history.replaceState(null, '', '#' + h); } catch (e) { /* (a file:// page) */ }
    store('numplay-studio2', h);  // (2: since the games are ranked)
    store('numplay-order2', st.order.join(','));  // (2: since the games are ranked)
  }
  function restore() {
    let h = location.hash.slice(1);
    if (!h) h = store('numplay-studio2') || '';
    const p = new URLSearchParams(h);
    const known = kit.games.filter((g) => g !== rg).sort((a, b) => (a.xl - b.xl) || ((a.rank || 99) - (b.rank || 99))).map((g) => g.id);
    const saved = (store('numplay-order2') || '').split(',').filter((id) => known.includes(id));
    const linked = p.has('games') ? p.get('games').split(',').filter((id) => known.includes(id)) : null;
    // the order: the link's picks first, in its order, then the rest as last time, then the kit's
    st.order = [...new Set([...(linked || []), ...saved, ...known])];
    st.picked = new Set(linked || kit.games.filter((g) => g.default).map((g) => g.id));
    st.cap = spaces().some((x) => x.id === p.get('space')) ? p.get('space') : 'std';
    st.lang = LANGS[p.get('lang')] && kit.langs.includes(p.get('lang')) ? p.get('lang') : 'en';
    const icon = p.get('icon');
    st.app.icon = PRESETS[icon] ? icon : icon === 'custom' && custom ? 'custom' : 'NumPlay';
    st.app.name = p.has('name') ? p.get('name').slice(0, 20) : (PRESETS[st.app.icon] || [0, 'NumPlay'])[1];
    const open = (p.get('open') || '').split('-');
    st.app.open = open[0] === 'matrices' ? 'matrices' : 'numplay';
    st.app.secret = Math.min(SECRETS.length - 1, Math.max(0, parseInt(open[1], 10) || 0));
    st.app.hint = !open.includes('nohint');
    if (solo()) st.picked = new Set([solo()]);
    // a pick that no longer fits the calculator (the kit grew): drop the biggest games until it does
    const big = ordered().sort((a, b) => flash(a) - flash(b));
    while (usedBytes() > capacity() && big.length) st.picked.delete(big.pop());
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
  // a game's object in the app's language, or English when that language's kit hasn't got it
  // (a big game's is the English one: kit.json "en")
  const objPath = (id) => {
    const g = st.lang !== 'en' && kits[st.lang] && kits[st.lang].byId[id];
    return (g && !g.en ? st.lang + '/' : '') + 'games/' + id + '.o';
  };
  async function loadKit(lang) {
    if (lang === 'en' || kits[lang]) return;
    const k = await fetch(KIT + lang + '/kit.json').then((r) => { if (!r.ok) throw new Error(r.status); return r.json(); });
    k.byId = Object.fromEntries(k.games.map((g) => [g.id, g]));
    kits[lang] = k;
  }

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

  // A relocatable ARM object (e_flags 0, like nwlink's): one section and one global symbol for
  // each of `secs` ({ name, data, sym, flags, type }).
  function elfObject(secs) {
    const enc = new TextEncoder();
    const shstr = '\0' + secs.map((s) => s.name).join('\0') + '\0.symtab\0.strtab\0.shstrtab\0';
    const str = '\0' + secs.map((s) => s.sym).join('\0') + '\0';
    const shs = enc.encode(shstr), sts = enc.encode(str);
    let at = 52;
    const offs = secs.map((s) => { const o = at; at = align4(at + s.data.length); return o; });
    const symOff = at, nsym = secs.length + 1, strOff = symOff + 16 * nsym;
    const shsOff = strOff + sts.length, shOff = align4(shsOff + shs.length), nsh = secs.length + 4;
    const b = new Uint8Array(shOff + 40 * nsh), v = new DataView(b.buffer);
    b.set([0x7f, 0x45, 0x4c, 0x46, 1, 1, 1], 0);
    v.setUint16(16, 1, true); v.setUint16(18, 40, true); v.setUint32(20, 1, true);
    v.setUint32(32, shOff, true);
    v.setUint16(40, 52, true); v.setUint16(46, 40, true); v.setUint16(48, nsh, true); v.setUint16(50, nsh - 1, true);
    secs.forEach((s, i) => b.set(s.data, offs[i]));
    let sn = 1;
    secs.forEach((s, i) => {
      const o = symOff + 16 * (i + 1);
      v.setUint32(o, sn, true); sn += enc.encode(s.sym).length + 1;
      v.setUint32(o + 8, s.data.length, true); v.setUint8(o + 12, 0x10 | (s.type === undefined ? 1 : s.type)); v.setUint16(o + 14, i + 1, true);
    });
    b.set(sts, strOff); b.set(shs, shsOff);
    const sh = (i, name, type, flags, off, sz, link, info, al, es) => {
      const o = shOff + 40 * i;
      [name, type, flags, 0, off, sz, link, info, al, es].forEach((x, k) => v.setUint32(o + 4 * k, x, true));
    };
    let nameAt = 1;
    secs.forEach((s, i) => { sh(i + 1, nameAt, 1, s.flags || 2, offs[i], s.data.length, 0, 0, s.align || 1, 0); nameAt += enc.encode(s.name).length + 1; });
    const n = secs.length;
    sh(n + 1, shstr.indexOf('.symtab'), 2, 0, symOff, 16 * nsym, n + 2, 1, 4, 16);
    sh(n + 2, shstr.indexOf('.strtab'), 3, 0, strOff, sts.length, 0, 0, 1, 0);
    sh(n + 3, shstr.indexOf('.shstrtab'), 3, 0, shsOff, shs.length, 0, 0, 1, 0);
    return b;
  }

  // The ROM table Retro-Go reads (np_roms, as tools/romtable.py makes it): "NPRM", version 1, the
  // count, then 52 bytes per game (offset, size, system, 3 spare bytes, its name in 40), then the games.
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
  const romObject = (table) => elfObject([{ name: '.rodata.np_roms', sym: 'np_roms', data: table, align: 4 }]);
  // The app's name, and how it starts (launcher/app_name.c): 'D', 1, flags (1 Matrices, 2 no hint), the secret
  function appObject() {
    const a = st.app, name = new TextEncoder().encode((a.name || ' ').slice(0, 20));
    const nz = new Uint8Array(name.length + 1); nz.set(name);
    const flags = (a.open === 'matrices' ? 1 : 0) | (a.hint ? 0 : 2);
    return elfObject([{ name: '.rodata.eadk_app_name', sym: 'eadk_app_name', data: nz },
                      { name: '.rodata.np_defaults', sym: 'np_defaults', data: new Uint8Array([0x44, 1, flags, a.secret]) }]);
  }

  // An icon of your own: 55x56 pixels in RGB565, LZ4 compressed, as nwlink's png-icon-o makes them
  function lz4(src) {
    const n = src.length, out = [], hash = new Int32Array(4096).fill(-1);
    const len = (l) => { while (l >= 255) { out.push(255); l -= 255; } out.push(l); };
    let anchor = 0, i = 0;
    while (i + 12 <= n) {  // (a match starts 12 bytes before the end at the latest, and ends 5 before)
      const k = Math.imul(src[i] | src[i + 1] << 8 | src[i + 2] << 16 | src[i + 3] << 24, 2654435761) >>> 20, ref = hash[k];
      hash[k] = i;
      if (ref >= 0 && i - ref < 65536 && src[ref] === src[i] && src[ref + 1] === src[i + 1] && src[ref + 2] === src[i + 2] && src[ref + 3] === src[i + 3]) {
        let m = 4;
        while (i + m < n - 5 && src[ref + m] === src[i + m]) m++;
        const lit = i - anchor;
        out.push((Math.min(lit, 15) << 4) | Math.min(m - 4, 15));
        if (lit >= 15) len(lit - 15);
        for (let j = anchor; j < i; j++) out.push(src[j]);
        out.push((i - ref) & 255, (i - ref) >> 8);
        if (m - 4 >= 15) len(m - 4 - 15);
        i += m; anchor = i;
      } else i++;
    }
    const lit = n - anchor;
    out.push(Math.min(lit, 15) << 4);
    if (lit >= 15) len(lit - 15);
    for (let j = anchor; j < n; j++) out.push(src[j]);
    return new Uint8Array(out);
  }
  async function iconFrom(file) {
    const img = await createImageBitmap(file);
    const c = document.createElement('canvas');
    c.width = 55; c.height = 56;
    const g = c.getContext('2d');
    g.fillStyle = '#ffffff';  // (the calculator's home screen is white: see-through parts become white)
    g.fillRect(0, 0, 55, 56);
    const s = Math.max(55 / img.width, 56 / img.height);
    g.imageSmoothingQuality = 'high';
    g.drawImage(img, (55 - img.width * s) / 2, (56 - img.height * s) / 2, img.width * s, img.height * s);
    const px = g.getImageData(0, 0, 55, 56).data, raw = new Uint8Array(55 * 56 * 2);
    for (let p = 0, q = 0; p < px.length; p += 4, q += 2) {
      const v = ((px[p] >> 3) << 11) | ((px[p + 1] >> 2) << 5) | (px[p + 2] >> 3);
      raw[q] = v & 255; raw[q + 1] = v >> 8;
    }
    const obj = elfObject([{ name: '.rodata.eadk_app_icon', sym: 'eadk_app_icon', data: lz4(raw), flags: 3, type: 0 }]);
    return { url: c.toDataURL('image/png'), obj };
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

  // ===== Your own .nwa games, as real games of NumPlay =====
  // What the kit's build does for a big game made from its own .nwa (Makefile, tools/npmodule.py, tools/gen_games.py),
  // here in the browser: the game's RAM goes into NumPlay's shared arena (so RAM never adds up), it gets its own
  // entry in the launcher's table, its own block of flash, its own card.
  const NI_HEAD = 0x3147504E, NI_TAIL = 0x444E4550;
  const NI_MODULE_LD = `SECTIONS {
  /DISCARD/ : {
    *(.rodata.eadk_app_name) *(.rodata.eadk_app_icon) *(.rodata.eadk_api_level)
    *(.ARM.exidx*) *(.ARM.extab*) *(.comment) *(.eh_frame*) *(.note*) *(.debug*)
    *(.init_array*) *(.fini_array*) *(.preinit_array*)
  }
  .text : {
    *(.text .text.* .rodata .rodata.* .glue_7 .glue_7t .vfp11_veneer .v4_bx)
  }
  .data : { *(.data .data.*) }
  .bss : { *(.bss .bss.* COMMON) }
}
`;
  const niCstr = (a, o) => { let n = o; while (n < a.length && a[n]) n++; return new TextDecoder().decode(a.subarray(o, n)); };

  // reads a 32-bit little-endian ARM relocatable ELF
  function niParse(b) {
    if (b.length < 52 || b[0] !== 0x7f || b[1] !== 0x45 || b[2] !== 0x4c || b[3] !== 0x46 || b[4] !== 1 || b[5] !== 1) throw new Error("isn't an ELF file");
    const v = new DataView(b.buffer, b.byteOffset, b.byteLength);
    if (v.getUint16(16, true) !== 1 || v.getUint16(18, true) !== 40) throw new Error("isn't an object .nwa (ARM relocatable ELF)");
    const e = { flags: v.getUint32(0x24, true), secs: [], syms: [] };
    const shoff = v.getUint32(0x20, true), shentsize = v.getUint16(0x2e, true), shnum = v.getUint16(0x30, true), shstrndx = v.getUint16(0x32, true);
    for (let i = 0; i < shnum; i++) {
      const o = shoff + i * shentsize;
      const s = { nameOff: v.getUint32(o, true), type: v.getUint32(o + 4, true), flags: v.getUint32(o + 8, true), off: v.getUint32(o + 16, true),
        size: v.getUint32(o + 20, true), link: v.getUint32(o + 24, true), info: v.getUint32(o + 28, true), align: v.getUint32(o + 32, true) };
      s.data = s.type === 8 ? new Uint8Array(0) : b.slice(s.off, s.off + s.size);
      e.secs.push(s);
    }
    const shstr = e.secs[shstrndx].data;
    for (const s of e.secs) s.name = niCstr(shstr, s.nameOff);
    const symtabs = e.secs.map((s, i) => [s, i]).filter(([s]) => s.type === 2);
    if (symtabs.length !== 1) throw new Error('expected one symbol table');
    const [st, sti] = symtabs[0];
    e.symtabIndex = sti;
    const strtab = e.secs[st.link].data;
    const sv = new DataView(st.data.buffer, st.data.byteOffset, st.data.byteLength);
    for (let k = 0; k < st.data.length / 16; k++) {
      const info = st.data[16 * k + 12];
      e.syms.push({ name: niCstr(strtab, sv.getUint32(16 * k, true)), value: sv.getUint32(16 * k + 4, true), size: sv.getUint32(16 * k + 8, true),
        bind: info >> 4, type: info & 15, other: st.data[16 * k + 13], shndx: sv.getUint16(16 * k + 14, true) });
    }
    return e;
  }
  // the REL relocations of section `target`: [offset, type, symbol index]
  function niRels(e, target) {
    const out = [];
    for (const s of e.secs) {
      if (s.type === 9 && s.info === target) {
        const dv = new DataView(s.data.buffer, s.data.byteOffset, s.data.byteLength);
        for (let k = 0; k < s.data.length / 8; k++) { const info = dv.getUint32(8 * k + 4, true); out.push([dv.getUint32(8 * k, true), info & 255, info >>> 8]); }
      } else if (s.type === 4 && s.info === target) throw new Error('RELA relocations are not supported');
    }
    return out;
  }

  // writes a relocatable ARM ELF. secs: { name, type, flags, align, data | size, relocs: [[offset, type, symbol]] };
  // syms: { name, value, size, bind (0 local, 1 global, 2 weak), type, sec (a section above; none: undefined), abs }
  function niElf(flags, secs, syms) {
    const enc = new TextEncoder();
    const locals = syms.filter((s) => !s.bind), globals = syms.filter((s) => s.bind);
    const order = [null].concat(locals, globals);
    const symIndex = new Map(order.map((s, i) => [s, i]));
    const rels = secs.map((s, i) => [s, i]).filter(([s]) => s.relocs && s.relocs.length);
    const symtabI = 1 + secs.length + rels.length;
    const strtab = [0], strOff = new Map();
    const addStr = (t) => {
      if (!t) return 0;
      if (strOff.has(t)) return strOff.get(t);
      const o = strtab.length;
      strOff.set(t, o);
      for (const c of enc.encode(t)) strtab.push(c);
      strtab.push(0);
      return o;
    };
    const symtab = new Uint8Array(16 * order.length), sv = new DataView(symtab.buffer);
    order.forEach((s, i) => {
      if (!s) return;
      sv.setUint32(16 * i, addStr(s.name), true);
      sv.setUint32(16 * i + 4, s.value || 0, true);
      sv.setUint32(16 * i + 8, s.size || 0, true);
      symtab[16 * i + 12] = (s.bind << 4) | (s.type || 0);
      symtab[16 * i + 13] = s.other || 0;
      sv.setUint16(16 * i + 14, s.abs ? 0xFFF1 : s.sec ? secs.indexOf(s.sec) + 1 : 0, true);
    });
    const parts = secs.map((s) => ({ name: s.name, type: s.type, flags: s.flags, link: 0, info: 0, align: s.align || 1, entsize: 0,
      data: s.data || null, size: s.data ? s.data.length : s.size }));
    for (const [s, i] of rels) {
      const d = new Uint8Array(8 * s.relocs.length), dv = new DataView(d.buffer);
      s.relocs.forEach(([off, type, sym], k) => {
        if (!symIndex.has(sym)) throw new Error('relocation against an unknown symbol');
        dv.setUint32(8 * k, off, true); dv.setUint32(8 * k + 4, ((symIndex.get(sym) << 8) | type) >>> 0, true);
      });
      parts.push({ name: '.rel' + s.name, type: 9, flags: 0x40, link: symtabI, info: i + 1, align: 4, entsize: 8, data: d, size: d.length });
    }
    parts.push({ name: '.symtab', type: 2, flags: 0, link: symtabI + 1, info: 1 + locals.length, align: 4, entsize: 16, data: symtab, size: symtab.length });
    parts.push({ name: '.strtab', type: 3, flags: 0, link: 0, info: 0, align: 1, entsize: 0, data: Uint8Array.from(strtab), size: strtab.length });
    const shstr = [0], nameOff = [];
    for (const p of parts.concat([{ name: '.shstrtab' }])) {
      nameOff.push(shstr.length);
      for (const c of enc.encode(p.name)) shstr.push(c);
      shstr.push(0);
    }
    parts.push({ name: '.shstrtab', type: 3, flags: 0, link: 0, info: 0, align: 1, entsize: 0, data: Uint8Array.from(shstr), size: shstr.length });
    let at = 52;
    for (const p of parts) {
      at = (at + 3) & ~3;
      p.off = at;
      if (p.type !== 8) at += p.size;
    }
    const shoff = (at + 3) & ~3, nsh = parts.length + 1;
    const b = new Uint8Array(shoff + 40 * nsh), v = new DataView(b.buffer);
    b.set([0x7f, 0x45, 0x4c, 0x46, 1, 1, 1], 0);
    v.setUint16(16, 1, true); v.setUint16(18, 40, true); v.setUint32(20, 1, true); v.setUint32(32, shoff, true); v.setUint32(36, flags >>> 0, true);
    v.setUint16(40, 52, true); v.setUint16(46, 40, true); v.setUint16(48, nsh, true); v.setUint16(50, nsh - 1, true);
    parts.forEach((p, i) => {
      if (p.type !== 8 && p.data) b.set(p.data, p.off);
      const o = shoff + 40 * (i + 1);
      v.setUint32(o, nameOff[i], true); v.setUint32(o + 4, p.type, true); v.setUint32(o + 8, p.flags, true);
      v.setUint32(o + 16, p.off, true); v.setUint32(o + 20, p.size, true); v.setUint32(o + 24, p.link, true);
      v.setUint32(o + 28, p.info, true); v.setUint32(o + 32, p.align, true); v.setUint32(o + 36, p.entsize, true);
    });
    return b;
  }

  // tools/npmodule.py: the game linked with module.ld (one .text, .data, .bss) becomes a module. Its RAM is not
  // allocated any more: what pointed into its .data, .bss or heap points into NumPlay's arena (np_arena, which the
  // kit's arena.o defines), at the offsets gen_games.py gives a game: .data first, then .bss, then the heap.
  // opt.symbols: as the python tool, with np_<game>_data and np_<game>_bss left to the linker (for the tests).
  function niModule(bytes, g, index, opt = {}) {
    const e = niParse(bytes);
    const fail = (m) => { throw new Error(m); };
    const byName = {};
    e.secs.forEach((s, i) => {
      if (s.flags & 2) {
        if (!['.text', '.data', '.bss'].includes(s.name)) fail('unexpected allocated section ' + s.name);
        byName[s.name] = i;
      }
    });
    if (byName['.text'] === undefined) fail('no .text section');
    const textI = byName['.text'], dataI = byName['.data'], bssI = byName['.bss'];
    const text = e.secs[textI], data = dataI !== undefined ? e.secs[dataI] : null, bss = bssI !== undefined ? e.secs[bssI] : null;
    const dataSize = data ? data.size : 0, bssSize = bss ? bss.size : 0;
    const bssOff = (dataSize + 7) & ~7, heapOff = (bssOff + bssSize + 7) & ~7;
    const ramAlign = Math.max(8, data ? data.align : 0, bss ? bss.align : 0);
    const base = `.rodata.np.${index}.${g}`;
    const code = { name: base, type: 1, flags: 6, align: Math.max(4, text.align), data: null, relocs: [] };
    const init = { name: base + '.init', type: 1, flags: 2, align: data ? Math.max(4, data.align) : 4, data: null, relocs: [] };
    const outSecs = [code, init];
    const attrs = e.secs.find((s) => s.type === 0x70000003);
    if (attrs) outSecs.push({ name: '.ARM.attributes', type: 0x70000003, flags: 0, align: 1, data: attrs.data });
    const syms = [];
    const sym = (o) => { const s = Object.assign({ name: '', value: 0, size: 0, bind: 0, type: 0, other: 0 }, o); syms.push(s); return s; };
    const codeSec = sym({ type: 3, sec: code }), initSec = sym({ type: 3, sec: init });
    const remap = new Map(), ramSym = new Map();
    const undefinedSyms = new Map();
    let entry = null;
    const heapName = `np_${g}_heap`;
    e.syms.forEach((s, k) => {
      if (k === 0) return;
      const sh = s.shndx;
      if (s.type === 3) {
        if (sh === textI) remap.set(k, codeSec);
        else if (sh === dataI) ramSym.set(k, ['data', 0]);
        else if (sh === bssI) ramSym.set(k, ['bss', 0]);
        return;
      }
      if (s.type === 4) return;
      if (dataI !== undefined && sh === dataI) { ramSym.set(k, ['data', s.value]); return; }
      if (bssI !== undefined && sh === bssI) { ramSym.set(k, ['bss', s.value]); return; }
      if (sh === 0) {
        if (s.bind === 0) return;
        if (!opt.symbols && s.name === heapName) { ramSym.set(k, ['heap', 0]); return; }
        if (!undefinedSyms.has(s.name)) undefinedSyms.set(s.name, { name: s.name, value: 0, size: 0, bind: s.bind, type: 0, other: 0 });
        return;
      }
      if (sh === textI) {
        const n = Object.assign({}, s, { sec: code });
        if (s.name === 'main' && s.bind !== 0) {
          n.name = `np_${g}_main`; n.bind = 1; n.other = 0; entry = n; remap.set(k, n);
        } else {
          n.bind = 0; n.other = 0; syms.push(n); remap.set(k, n);
        }
        return;
      }
      if (sh === 0xFFF1) { const n = Object.assign({}, s, { bind: 0, abs: true }); syms.push(n); remap.set(k, n); return; }
      fail(`symbol ${s.name} defined in unexpected section ${sh}`);
    });
    if (!entry) fail('entry symbol main not found');
    syms.push(entry);
    const initSym = sym({ name: `np_${g}_init`, size: dataSize, bind: 1, type: 1, sec: init });
    const ramIndex = {};
    if (opt.symbols) {
      ramIndex.data = sym({ name: `np_${g}_data`, bind: 1 });
      ramIndex.bss = sym({ name: `np_${g}_bss`, bind: 1 });
    } else {
      ramIndex.data = ramIndex.bss = ramIndex.heap = sym({ name: 'np_arena', bind: 1 });
    }
    const undOut = new Map();
    for (const [name, n] of undefinedSyms) { const x = Object.assign({}, n); syms.push(x); undOut.set(name, x); }
    e.syms.forEach((s, k) => { if (k && s.shndx === 0 && s.bind !== 0 && s.type !== 3 && undOut.has(s.name)) remap.set(k, undOut.get(s.name)); });
    const regionBase = opt.symbols ? { data: 0, bss: 0, heap: 0 } : { data: 0, bss: bssOff, heap: heapOff };
    const convert = (target, blob, out) => {
      blob = blob.slice();
      const dv = new DataView(blob.buffer, blob.byteOffset, blob.byteLength);
      for (const [off, typ, si] of niRels(e, target)) {
        if (ramSym.has(si)) {
          const [region, value] = ramSym.get(si);
          if (typ !== 2) fail(`relocation type ${typ} at 0x${off.toString(16)} points into RAM; only ABS32 is handled`);
          const add = value + regionBase[region];
          if (add) dv.setUint32(off, (dv.getUint32(off, true) + add) >>> 0, true);
          out.relocs.push([off, typ, ramIndex[region]]);
        } else if (remap.has(si)) out.relocs.push([off, typ, remap.get(si)]);
        else fail(`relocation at 0x${off.toString(16)} uses an unsupported symbol (${e.syms[si].name})`);
      }
      out.data = blob;
    };
    convert(textI, text.data, code);
    if (data) convert(dataI, data.data, init); else init.data = new Uint8Array(0);
    for (const s of e.secs) if (s.type === 9 && s.info !== textI && s.info !== dataI && (e.secs[s.info].flags & 2)) fail(`relocations for unexpected section ${e.secs[s.info].name}`);
    return { obj: niElf(e.flags, outSecs, syms), code: code.data.length, data: dataSize, bss: bssSize, ramAlign, bssOff, heapOff };
  }

  // the little objects around a module: its entry in the launcher's table, the marks around its block of flash, its card
  const niPut32 = (d, o, v) => { d[o] = v & 255; d[o + 1] = (v >>> 8) & 255; d[o + 2] = (v >>> 16) & 255; d[o + 3] = (v >>> 24) & 255; };
  function niEntry(g, index, mod, info, flags = 0) {
    // np_game_t of launcher/src/np.h, as tools/gen_games.py fills it (the calculator's layout, 80 bytes)
    const enc = new TextEncoder();
    const strs = [info.id, info.title, info.tagline].map((s) => enc.encode(s + '\0'));
    const sdata = new Uint8Array(strs.reduce((a, s) => a + s.length, 0));
    const soff = [];
    let at = 0;
    strs.forEach((s) => { soff.push(at); sdata.set(s, at); at += s.length; });
    const table = { name: `.rodata.np_games.${index}`, type: 1, flags: 2, align: 4, data: new Uint8Array(80), relocs: [] };
    const str = { name: `.rodata.np.str.${g}`, type: 1, flags: 2, align: 1, data: sdata };
    const lists = { name: `.rodata.np.lists.${g}`, type: 1, flags: 2, align: 4, data: new Uint8Array(8) };
    const syms = [], mk = (o) => { const s = Object.assign({ name: '', value: 0, size: 0, bind: 0, type: 0 }, o); syms.push(s); return s; };
    const sTab = mk({ type: 3, sec: table }), sStr = mk({ type: 3, sec: str }), sLists = mk({ type: 3, sec: lists });
    const und = (n) => mk({ name: n, bind: 1 });
    const begin = und(`np_${g}_begin`), end = und(`np_${g}_end`), main = und(`np_${g}_main`), init = und(`np_${g}_init`),
      shots = und(`np_${g}_shots`), arena = und('np_arena');
    const d = table.data, r = table.relocs;
    const ptr = (off, sym, add) => { niPut32(d, off, add || 0); r.push([off, 2, sym]); };
    ptr(0, sStr, soff[0]); ptr(4, sStr, soff[1]); ptr(8, sStr, soff[2]);              // id, title, tagline (credit: none)
    niPut32(d, 16, info.top); niPut32(d, 20, info.bottom); niPut32(d, 24, info.accent);
    ptr(28, begin); ptr(32, end); ptr(36, main);
    ptr(40, arena, 0); ptr(44, init); niPut32(d, 48, mod.data);                        // data, data_init, data_size
    ptr(52, arena, mod.bssOff); niPut32(d, 56, mod.bss);                               // bss, bss_size
    ptr(60, shots); d[64] = 1;                                                         // shots, nshots
    ptr(68, sLists, 0); ptr(72, sLists, 4);                                            // records, progress (none)
    niPut32(d, 76, 0);                                                                 // heap: the least RAM after its own
    return niElf(flags, [table, str, lists], syms);
  }
  function niMarks(g, index, flags = 0) {
    const w = (v) => { const d = new Uint8Array(4); niPut32(d, 0, v); return d; };
    return niElf(flags, [
      { name: `.rodata.np.${index}.${g}.0head`, type: 1, flags: 2, align: 4, data: w(NI_HEAD) },
      { name: `.rodata.np.${index}.${g}.zztail`, type: 1, flags: 2, align: 4, data: w(NI_TAIL) },
    ], []);
  }
  // np_shot_t: w, h, ncolors, zsize, palette, z (the palette's RGB565 colours, then the DEFLATE of w*h palette indices)
  function niShots(g, index, card, flags = 0) {
    const pal = card.palette.length * 2, total = 20 + pal + card.z.length;
    const d = new Uint8Array(total), dv = new DataView(d.buffer);
    dv.setUint16(0, card.w, true); dv.setUint16(2, card.h, true); dv.setUint16(4, card.palette.length, true); dv.setUint32(8, card.z.length, true);
    dv.setUint32(12, 20, true); dv.setUint32(16, 20 + pal, true);
    card.palette.forEach((c, i) => dv.setUint16(20 + 2 * i, c, true));
    d.set(card.z, 20 + pal);
    const sec = { name: `.rodata.np.${index}.${g}.shots`, type: 1, flags: 2, align: 4, data: d, relocs: [] };
    const secSym = { name: '', value: 0, size: 0, bind: 0, type: 3, sec };
    sec.relocs.push([12, 2, secSym], [16, 2, secSym]);
    return niElf(flags, [sec], [secSym, { name: `np_${g}_shots`, value: 0, size: total, bind: 1, type: 1, sec }]);
  }
  // stand-ins, weak, for the C library's system calls a game's own libc may leave undefined, and a main
  // that calls the game's (renamed nwa_main) with no arguments:  push {r3, lr}; movs r0,#0; movs r1,#0; bl nwa_main; pop {r3, pc}
  function niStub(flags = 0) {
    const th = [];
    const fn = (name, ...ops) => { th.push([name, ops]); };
    fn('_close', 0x2000, 0x3801, 0x4770);   // movs r0,#0; subs r0,#1; bx lr   (-1)
    fn('_fstat', 0x2000, 0x3801, 0x4770);
    fn('_lseek', 0x2000, 0x3801, 0x4770);
    fn('_read', 0x2000, 0x3801, 0x4770);
    fn('_kill', 0x2000, 0x3801, 0x4770);
    fn('_isatty', 0x2000, 0x4770);          // 0
    fn('_getpid', 0x2001, 0x4770);          // 1
    fn('_write', 0x4610, 0x4770);           // mov r0, r2 (the count); bx lr
    const total = th.reduce((a, [, o]) => a + 2 * o.length, 0) + 12;
    const d = new Uint8Array((total + 3) & ~3), dv = new DataView(d.buffer);
    const sec = { name: '.text', type: 1, flags: 6, align: 4, data: d, relocs: [] };
    const syms = [{ name: '', value: 0, size: 0, bind: 0, type: 3, sec }];
    let at = 0;
    for (const [name, ops] of th) {
      ops.forEach((o, i) => dv.setUint16(at + 2 * i, o, true));
      syms.push({ name, value: at | 1, size: 2 * ops.length, bind: 2, type: 2, sec });
      at += 2 * ops.length;
    }
    [0xB508, 0x2000, 0x2100, 0xF7FF, 0xFFFE, 0xBD08].forEach((o, i) => dv.setUint16(at + 2 * i, o, true));
    syms.push({ name: 'main', value: at | 1, size: 12, bind: 1, type: 2, sec });
    const callee = { name: 'nwa_main', bind: 1, type: 0 };
    syms.push(callee);
    sec.relocs.push([at + 6, 10, callee]);  // R_ARM_THM_CALL
    return niElf(flags, [sec], syms);
  }

  // the icon inside a .nwa: nwlink's LZ4 block of 55 x 56 RGB565 pixels
  function niLz4(src, size) {
    const out = new Uint8Array(size);
    let i = 0, o = 0;
    while (i < src.length) {
      const t = src[i++];
      let lit = t >> 4;
      if (lit === 15) { let b; do { b = src[i++]; lit += b; } while (b === 255); }
      for (let k = 0; k < lit; k++) out[o++] = src[i++];
      if (i >= src.length) break;
      const off = src[i] | (src[i + 1] << 8); i += 2;
      let m = t & 15;
      if (m === 15) { let b; do { b = src[i++]; m += b; } while (b === 255); }
      m += 4;
      for (let k = 0; k < m; k++) { out[o] = out[o - off]; o++; }
    }
    if (o !== size) throw new Error('icon size');
    return out;
  }
  // what the Studio shows of a .nwa: its name, its icon (RGBA, 55 x 56) if it has one, and whether it has a main
  function niInfo(bytes) {
    const e = niParse(bytes);
    const sec = (n) => e.secs.find((s) => s.name === n);
    const nm = sec('.rodata.eadk_app_name'), ic = sec('.rodata.eadk_app_icon');
    let main = false;
    for (const s of e.syms) if (s.name === 'main' && s.bind === 1 && s.shndx !== 0) main = true;
    let rgba = null;
    try {
      if (ic) {
        const raw = niLz4(ic.data, 55 * 56 * 2);
        rgba = new Uint8ClampedArray(55 * 56 * 4);
        for (let p = 0; p < 55 * 56; p++) {
          const c = raw[2 * p] | (raw[2 * p + 1] << 8);
          rgba[4 * p] = ((c >> 11) * 255 / 31) | 0; rgba[4 * p + 1] = (((c >> 5) & 63) * 255 / 63) | 0; rgba[4 * p + 2] = ((c & 31) * 255 / 31) | 0; rgba[4 * p + 3] = 255;
        }
      }
    } catch (x) { rgba = null; }
    return { title: nm ? niCstr(nm.data, 0).trim() : '', rgba, main };
  }
  // 0xRRGGBB colours of a game's card, from its icon's: [top, bottom, accent]
  function niTheme(rgba, seed) {
    let r = 0, g = 0, b = 0, n = 0;
    if (rgba) for (let p = 0; p < rgba.length; p += 4) if (!(rgba[p] > 235 && rgba[p + 1] > 235 && rgba[p + 2] > 235)) { r += rgba[p]; g += rgba[p + 1]; b += rgba[p + 2]; n++; }
    if (!n) { const h = (seed * 47) % 360; return niHls(h / 360, 0.42, 0.55).map((c, i) => c); }
    return niHls(...niRgb2hl(r / n / 255, g / n / 255, b / n / 255));
  }
  function niRgb2hl(r, g, b) {
    const mx = Math.max(r, g, b), mn = Math.min(r, g, b), d = mx - mn;
    let h = 0;
    if (d) h = mx === r ? ((g - b) / d + 6) % 6 : mx === g ? (b - r) / d + 2 : (r - g) / d + 4;
    const l = (mx + mn) / 2, s = d ? d / (1 - Math.abs(2 * l - 1)) : 0;
    return [h / 6, Math.max(0.45, s)];
  }
  function niHls(h, s = 0.55) {
    const f = (ll) => {
      const a = s * Math.min(ll, 1 - ll), k = (n) => { const t = (n + h * 12) % 12; return ll - a * Math.max(-1, Math.min(t - 3, 9 - t, 1)); };
      return ((Math.round(k(0) * 255) << 16) | (Math.round(k(8) * 255) << 8) | Math.round(k(4) * 255)) >>> 0;
    };
    return [f(0.42), f(0.12), f(0.72)];
  }
  // median-cut: the `n` colours that stand for an RGBA picture, and each pixel's index
  function niQuantize(rgba, n) {
    const px = [];
    for (let p = 0; p < rgba.length; p += 4) px.push([rgba[p], rgba[p + 1], rgba[p + 2]]);
    let boxes = [px.map((_, i) => i)];
    const range = (bx) => {
      const lo = [255, 255, 255], hi = [0, 0, 0];
      for (const i of bx) for (let c = 0; c < 3; c++) { lo[c] = Math.min(lo[c], px[i][c]); hi[c] = Math.max(hi[c], px[i][c]); }
      return [0, 1, 2].map((c) => hi[c] - lo[c]);
    };
    while (boxes.length < n) {
      let best = -1, bc = 0, bv = 0;
      boxes.forEach((bx, i) => { const rg = range(bx); const m = Math.max(...rg); if (bx.length > 1 && m > bv) { bv = m; best = i; bc = rg.indexOf(m); } });
      if (best < 0) break;
      const bx = boxes[best].slice().sort((a, b) => px[a][bc] - px[b][bc]), half = bx.length >> 1;
      boxes.splice(best, 1, bx.slice(0, half), bx.slice(half));
    }
    const pal = boxes.map((bx) => [0, 1, 2].map((c) => Math.round(bx.reduce((a, i) => a + px[i][c], 0) / bx.length)));
    const idx = new Uint8Array(px.length);
    const cache = new Map();
    px.forEach((q, i) => {
      const key = q[0] << 16 | q[1] << 8 | q[2];
      let k = cache.get(key);
      if (k === undefined) {
        let bd = 1e9; k = 0;
        pal.forEach((c, j) => { const d = (c[0] - q[0]) ** 2 + (c[1] - q[1]) ** 2 + (c[2] - q[2]) ** 2; if (d < bd) { bd = d; k = j; } });
        cache.set(key, k);
      }
      idx[i] = k;
    });
    return { palette: pal.map((c) => ((c[0] & 0xF8) << 8) | ((c[1] & 0xFC) << 3) | (c[2] >> 3)), idx };
  }
  async function niDeflate(bytes) {
    const s = new Blob([bytes]).stream().pipeThrough(new CompressionStream('deflate-raw'));
    return new Uint8Array(await new Response(s).arrayBuffer());
  }

  // the size of a card (the picture of a game in the launcher): that of the kit's own, read off one of its games
  let niCardDims = null;
  async function niCardSize() {
    if (niCardDims) return niCardDims;
    const tries = ['sudoku'].concat(kit.games.filter((g) => !g.xl && g.id !== 'sudoku').slice(0, 4).map((g) => g.id));
    for (const id of tries) {
      try {
        const e = niParse(await fetchObj(objPath(id)));
        const s = e.secs.find((x) => /\.shots$/.test(x.name));
        if (!s || s.data.length < 6) continue;
        const dv = new DataView(s.data.buffer, s.data.byteOffset, s.data.byteLength);
        const w = dv.getUint16(0, true), h = dv.getUint16(2, true);
        if (w >= 16 && w <= 400 && h >= 16 && h <= 400) return (niCardDims = { w, h });
      } catch (x) { /* (the next one) */ }
    }
    return (niCardDims = { w: 160, h: 120 });
  }
  function niIconUrl(rgba) {
    if (!rgba) return null;
    const c = el('canvas'); c.width = 55; c.height = 56;
    c.getContext('2d').putImageData(new ImageData(rgba, 55, 56), 0, 0);
    return c.toDataURL('image/png');
  }
  async function niMakeCard(g, w, h) {
    const cv = el('canvas'); cv.width = w; cv.height = h;
    const c = cv.getContext('2d');
    const col = (v) => '#' + v.toString(16).padStart(6, '0');
    const gr = c.createLinearGradient(0, 0, 0, h);
    gr.addColorStop(0, col(g.theme[0])); gr.addColorStop(1, col(g.theme[1]));
    c.fillStyle = gr; c.fillRect(0, 0, w, h);
    if (g.rgba) {
      const ic = el('canvas'); ic.width = 55; ic.height = 56;
      ic.getContext('2d').putImageData(new ImageData(g.rgba, 55, 56), 0, 0);
      const s = Math.floor(h * 0.56), iw = Math.round(s * 55 / 56);
      c.imageSmoothingEnabled = false;
      c.drawImage(ic, Math.round((w - iw) / 2), Math.round(h * 0.08), iw, s);
    }
    let fs = Math.max(8, Math.floor(h * 0.13));
    c.textAlign = 'center'; c.textBaseline = 'middle'; c.fillStyle = col(g.theme[2]);
    do { c.font = 'bold ' + fs + 'px sans-serif'; fs--; } while (c.measureText(g.name).width > w - 8 && fs > 6);
    c.fillText(g.name, w / 2, Math.round(h * 0.88));
    const q = niQuantize(c.getImageData(0, 0, w, h).data, 16);
    return { w, h, palette: q.palette, z: await niDeflate(q.idx) };
  }

  // Your .nwa games, ready to be linked into NumPlay: for each, its module, its entry, its marks and its card, and
  // what numplay.ld needs (its block of flash, its place in the table)
  async function importNwas() {
    const t = await loadTools();
    const files = {}, names = [], entries = [], blocks = [];
    const dims = await niCardSize();
    // ld refuses objects whose EABI version differs: ours get the kit's (what its base.o has), the stub the game's
    const baseObj = await fetchObj((st.lang === 'en' ? '' : st.lang + '/') + 'base.o');
    const kitFlags = new DataView(baseObj.buffer, baseObj.byteOffset, baseObj.byteLength).getUint32(36, true);
    for (let i = 0; i < st.nwas.length; i++) {
      const g = st.nwas[i], id = 'dyn' + (i + 1), ix = 'x' + (i + 1);
      const bad = (what, r) => Object.assign(new Error(g.name + ': ' + what + (r && r.log ? ' (' + r.log.trim().split('\n').slice(-2).join(' ') + ')' : '')), { ni: true });
      try {
        const stub = niStub(new DataView(g.bytes.buffer, g.bytes.byteOffset, g.bytes.byteLength).getUint32(36, true));
        let r = await run(t.oc, ['--redefine-sym', 'main=nwa_main', 'in.o', 'a.o'], { 'in.o': g.bytes });
        if (r.code) throw bad('objcopy', r);
        const merged = { 'a.o': r.m.FS.readFile('a.o'), 'stub.o': stub, 'module.ld': NI_MODULE_LD };
        r = await run(t.ld, ['-r', '-d', '-z', 'noexecstack', '--force-group-allocation', '-T', 'module.ld', 'a.o', 'stub.o', '-o', 'm1.o'], merged);
        if (r.code && /unrecogni[sz]ed|unknown option|invalid option/i.test(r.log))  // (an older ld: no such option)
          r = await run(t.ld, ['-r', '-d', '-z', 'noexecstack', '-T', 'module.ld', 'a.o', 'stub.o', '-o', 'm1.o'], merged);
        if (r.code) throw bad('ld', r);
        r = await run(t.oc, ['--keep-global-symbol=main',
          '--redefine-sym', 'eadk_keyboard_scan=np_keyboard_scan', '--redefine-sym', 'eadk_event_get=np_event_get',
          '--redefine-sym', 'eadk_display_draw_string=np_display_draw_string',
          '--redefine-sym', '_heap_start=np_' + id + '_heap', '--redefine-sym', '_sbrk=np_sbrk',
          'm1.o', 'm2.o'], { 'm1.o': r.m.FS.readFile('m1.o') });
        if (r.code) throw bad('objcopy', r);
        const mod = niModule(r.m.FS.readFile('m2.o'), id, ix);
        const card = await niMakeCard(g, dims.w, dims.h);
        files[id + '.o'] = mod.obj;
        files[id + '_entry.o'] = niEntry(id, 'zz' + (i + 1), mod, { id, title: g.name, tagline: 'Your .nwa', top: g.theme[0], bottom: g.theme[1], accent: g.theme[2] }, kitFlags);
        files[id + '_marks.o'] = niMarks(id, ix, kitFlags);
        files[id + '_shots.o'] = niShots(id, ix, card, kitFlags);
        names.push(id + '.o', id + '_entry.o', id + '_marks.o', id + '_shots.o');
        entries.push('KEEP(*(.rodata.np_games.zz' + (i + 1) + '))');
        const p = '.rodata.np.' + ix + '.' + id;
        blocks.push(['  ' + p + ' : {', '    np_' + id + '_begin = .;', '    KEEP(*(' + p + '.0head))', '    *(' + p + ')', '    *(' + p + '.init)',
          '    KEEP(*(' + p + '.shots))', '    KEEP(*(' + p + '.zztail))', '    np_' + id + '_end = .;', '  }'].join('\n') + '\n');
      } catch (e) {
        if (e.ni) throw e;
        throw Object.assign(new Error(g.name + ': ' + e.message), { ni: true });
      }
    }
    return { files, names, entries, blocks };
  }

  async function build() {
    const one = solo();
    if (one) return { bytes: await fetchBytes(KIT + 'apps/' + one + '.nwa.gz'), file: game(one).alone.file };
    await loadKit(st.lang);
    const k = langKit();
    const sel = ordered();
    const roms = rg && st.roms.length;
    if (roms) sel.push(rg.id);
    const dyn = st.nwas.length ? await importNwas() : null;
    const lang = st.lang === 'en' ? '' : st.lang + '/';
    const iconObj = st.app.icon === 'custom' && custom ? Promise.resolve(custom.obj) : fetchObj('variants/' + st.app.icon + '-icon.o');
    const [t, base, arena, icon, ...objs] = await Promise.all([loadTools(), fetchObj(lang + 'base.o'), fetchObj(lang + 'arena.o'), iconObj]
      .concat(sel.map((id) => fetchObj(objPath(id)))));
    const files = { 'base.o': base, 'arena.o': arena, 'app.o': appObject(), 'icon.o': icon };
    sel.forEach((id, i) => { files[id + '.o'] = objs[i]; });
    const args = k.link.concat(['-T', 'numplay.ld', 'base.o', 'arena.o', 'app.o', 'icon.o'], sel.map((id) => id + '.o'));
    if (roms) {
      files['roms.o'] = romObject(romTable(st.roms));
      args.push('roms.o');
    }
    if (dyn) { Object.assign(files, dyn.files); args.push(...dyn.names); }
    // the launcher shows the games in the order of their entries: yours, not the kit's
    const entries = sel.map((id) => 'KEEP(*(.rodata.np_games.' + String((k.byId[id] || byId[id]).index).padStart(2, '0') + '))').concat(dyn ? dyn.entries : []).join('\n    ');
    const head = k.script.head.replace('KEEP(*(SORT_BY_NAME(.rodata.np_games.*)))', entries);
    // ...and the games' blocks in the kit's order: NumPlay's own games first, in the app area, then the big ones,
    // which find the calculator's files past it too (Enlarged Extra Apps)
    const block = (id) => k.script.blocks[id] || kit.script.blocks[id];
    const index = (id) => (k.byId[id] || byId[id]).index;
    files['numplay.ld'] = head + sel.slice().sort((a, b) => index(a) - index(b)).map(block).join('') + (dyn ? dyn.blocks.join('') : '') + k.script.tail;
    const l = await run(t.ld, args.concat(['-o', 'out.nwa']), files);
    if (l.code) throw new Error(l.log || 'ld ' + l.code);
    const s = await run(t.oc, ['--strip-unneeded', 'in.nwa', 'out.nwa'], { 'in.nwa': l.m.FS.readFile('out.nwa') });
    if (s.code) throw new Error(s.log || 'objcopy ' + s.code);
    const nwa = s.m.FS.readFile('out.nwa');
    const n = installedSize(nwa) + k.runtime;
    if (n > capacity()) throw new Error("That's " + size(n - capacity()) + ' more than the calculator holds');
    return { bytes: nwa, file: 'NumPlay.nwa' };
  }

  function download(bytes, file) {
    const a = el('a', '', { download: file, href: URL.createObjectURL(new Blob([bytes], { type: 'application/octet-stream' })) });
    document.body.appendChild(a);
    a.click();
    setTimeout(() => { URL.revokeObjectURL(a.href); a.remove(); }, 2000);
  }
  async function go(btn, sheet) {
    if (btn.classList.contains('busy')) return;
    btn.classList.add('busy');
    const key = btn.closest('.key3d, .fkey');
    if (key) key.classList.add('down');  // (its key stays in while NumPlay is made)
    try {
      const out = await build();
      download(out.bytes, out.file);
      for (const x of out.extra || []) { await wait(500); download(x.bytes, x.file); }
      if (sheet) openSheet(out.extra && out.extra.length ? out.file + ' and ' + out.extra[0].file : out.file);
    } catch (e) {
      console.error(e);
      toast(/more than/.test(e.message) || e.ni ? e.message : solo() ? "Couldn't get " + game(solo()).alone.file + ', try again' : "Couldn't make NumPlay.nwa, try again");
    } finally {
      btn.classList.remove('busy');
      if (key) key.classList.remove('down');
    }
  }
  function prefetch() {
    if (solo()) { fetchBytes(KIT + 'apps/' + solo() + '.nwa.gz').catch(() => {}); return; }
    loadTools().catch(() => {});
    loadKit(st.lang).then(() => {
      const lang = st.lang === 'en' ? '' : st.lang + '/';
      for (const p of [lang + 'base.o', lang + 'arena.o', 'variants/' + (PRESETS[st.app.icon] ? st.app.icon : 'NumPlay') + '-icon.o']) fetchObj(p).catch(() => {});
      for (const id of st.picked) fetchObj(objPath(id)).catch(() => {});
      if (st.roms.length) fetchObj(objPath(rg.id)).catch(() => {});
    }).catch(() => {});
  }

  // ------------------------------------------------------------------ the app's own settings
  let popOpen = null, popAt = null;
  function openPop(pop, chip, side) {
    if (popOpen === pop) return closePops();
    closePops();
    pop.hidden = false;
    popOpen = pop;
    popAt = [chip, side];
    chip.setAttribute('aria-expanded', 'true');
    placePop();
  }
  function placePop() {
    const pop = popOpen, [chip, side] = popAt;
    const r = chip.getBoundingClientRect(), w = pop.offsetWidth, h = pop.offsetHeight;
    if (side && r.right + 16 + w < innerWidth - 12) {  // (beside it, over the calculator, not over the games)
      pop.style.left = (r.right + 16) + 'px';
      pop.style.top = Math.max(12, Math.min(innerHeight - h - 12, r.bottom - h)) + 'px';
      return;
    }
    pop.style.left = Math.max(12, Math.min(innerWidth - w - 12, r.left + r.width / 2 - w / 2)) + 'px';
    pop.style.top = (r.top - h - 12 < 12 ? r.bottom + 12 : r.top - h - 12) + 'px';
  }
  function closePops() {
    for (const p of document.querySelectorAll('.pop')) p.hidden = true;
    for (const c of document.querySelectorAll('.chip, .space-chip')) c.setAttribute('aria-expanded', 'false');
    popOpen = null;
  }
  function renderApp() {
    const box = $('#icons');
    box.textContent = '';
    for (const v of kit.variants) {
      const b = el('button', '', { type: 'button', role: 'radio', 'aria-checked': st.app.icon === v, 'aria-label': PRESETS[v][0], 'data-tip': PRESETS[v][0] });
      b.appendChild(el('img', '', { src: KIT + 'variants/' + v + '.png', alt: '' }));
      b.addEventListener('click', () => {
        st.app.icon = v;
        st.app.name = PRESETS[v][1];
        if (v === 'NumPlay-Matrices') st.app.open = 'matrices';
        renderApp(); render();
      });
      box.appendChild(b);
    }
    if (custom) {
      const b = el('button', 'custom', { type: 'button', role: 'radio', 'aria-checked': st.app.icon === 'custom', 'aria-label': 'Your icon', 'data-tip': 'Your icon' });
      b.appendChild(el('img', '', { src: custom.url, alt: '' }));
      b.addEventListener('click', () => { st.app.icon = 'custom'; renderApp(); render(); });
      box.appendChild(b);
    }
    const up = el('button', 'upload', { type: 'button', 'aria-label': 'Your own picture', 'data-tip': 'Your own picture' });
    up.innerHTML = '<svg viewBox="0 0 24 24" width="18" height="18" fill="none" stroke="currentColor" stroke-width="2.2" stroke-linecap="round"><path d="M12 5v14"/><path d="M5 12h14"/></svg>';
    up.addEventListener('click', () => $('#icon-file').click());
    box.appendChild(up);
    $('#app-name').value = st.app.name;
    for (const b of document.querySelectorAll('[data-open]')) b.setAttribute('aria-checked', b.dataset.open === st.app.open);
    $('#secret-row').hidden = st.app.open !== 'matrices';
    $('#secret').value = String(st.app.secret);
    $('#hint').checked = st.app.hint;
    const langs = $('#langs');
    langs.textContent = '';
    for (const l of kit.langs) {
      const b = el('button', '', { type: 'button', role: 'radio', 'aria-checked': st.lang === l });
      b.textContent = LANGS[l][1];
      b.addEventListener('click', async () => {
        try { await loadKit(l); } catch (e) { toast("That language didn't load, try again"); return; }
        const was = st.lang;
        st.lang = l;
        const over = usedBytes() - capacity();  // (NumPlay in another language is a little bigger or smaller)
        if (over > 0) { st.lang = was; toast('In ' + LANGS[l][1] + ' that\'s ' + size(over) + ' too much: take a game out first'); return; }
        closePops(); renderApp(); render();
      });
      langs.appendChild(b);
    }
  }

  // ------------------------------------------------------------------ the calculator's space for apps
  // Pick yours: its name and size, the installer's figure on each; with Nwagra, how to get it
  function renderSpaces() {
    const box = $('#spaces');
    box.textContent = '';
    for (const sp of spaces()) {
      const b = el('button', '', { type: 'button', role: 'radio', 'aria-checked': st.cap === sp.id,
        'data-tip': Math.round(sp.size / 1024) + ' KB in NumWorks\' installer' });
      b.appendChild(el('span')).textContent = SPACES[sp.id] || sp.id;
      b.appendChild(el('b')).textContent = mb(sp.size);
      b.addEventListener('click', () => {
        if (!setSpace(sp.id)) return;
        if (nwagra()) { renderSpaces(); placePop(); } else closePops();
      });
      box.appendChild(b);
    }
    $('#nwagra').hidden = !nwagra();
  }
  function setSpace(id) {
    if (id === st.cap) return true;
    const was = st.cap, picked = new Set(st.picked), had = before;
    st.cap = id;
    const one = ordered().find(alone);
    if (one && ordered().length > 1) {  // (it goes on its own here: the rest come back when it's out)
      before = ordered().filter((x) => x !== one);
      st.picked = new Set([one]);
      toast(byId[one].title + ' goes on its own in ' + mb(capacity()) + '. Pick it again to get your other games back');
    } else if (before && !solo()) {  // (room beside it here: your other games come back, as many as fit)
      for (const x of before) if (total(ordered().concat([x])) <= capacity()) st.picked.add(x);
      before = null;
    }
    const over = usedBytes() - capacity();
    if (over > 0) { st.cap = was; st.picked = picked; before = had; toast('Take ' + size(over) + ' of games out first'); return false; }
    render();
    return true;
  }

  // ------------------------------------------------------------------ the calculator in 3D, and its sticker
  // The calculator is built from its photo (1018 by 2000 pixels; --u is one of them on the page): its front
  // has a hole where the screen sits, down in a recess 14 deep, with the recess's walls round it; its long
  // edges are rounded (a quarter circle of 30, as the photo shows them) before its sides, 132 deep. The
  // star sticker is one piece of foil laid over all of that: flat on the frame by the screen, down the step
  // into the recess, and round the edge onto the side. Each surface is lit from the same light.
  const STAR = { size: 230, from: 849, lip: 915, step: 14, edge: 988, top: 215 };  // (the artwork's span, unfolded)
  const EDGE_R = 30, EDGE_N = 9, EDGE_C = 2 * EDGE_R * Math.sin(Math.PI / 4 / EDGE_N), DEPTH = 132;
  const EDGE_A = Array.from({ length: EDGE_N }, (_, i) => (i + 0.5) * 90 / EDGE_N);  // (each facet's angle)
  const LIGHT = [-0.25, -0.6, 0.76];
  const lit = (n) => 0.78 + 0.22 * (n[0] * LIGHT[0] + n[1] * LIGHT[1] + n[2] * LIGHT[2]) / 0.76;  // (the front is 1)
  const U = (n) => 'calc(var(--u) * ' + (+n.toFixed(3)) + ')';
  const grey = (n, base) => { const v = Math.round(Math.min(255, base * lit(n))); return 'rgb(' + v + ',' + v + ',' + v + ')'; };
  const part = (parent, tag, cls, css) => { const e = el(tag, cls); Object.assign(e.style, css); parent.appendChild(e); return e; };
  const deg = (a) => a * Math.PI / 180;
  // The sticker itself is a picture drawn by a small WebGL shader, from a map of what's where on it (drawn
  // with a 2D canvas: the white vinyl die-cut round a star, the gold foil on it, the ink printed on the foil,
  // and the height of it all, for its rounded edge and the line embossed in the foil). The vinyl is matte
  // white with a soft edge and a thin shadow under it; the foil is metal, so it shows the room it's in (soft
  // lights up left and up right, a strip light above, a darker floor), with a fine grain, and that sliding
  // across it as the calculator turns; the ink is matte; a clear gloss coat over it all catches the
  // pointer's light. Every piece of the sticker on the calculator copies this one picture.
  const ART = 512;
  const sticker = { draw: null, leaves: [], text: ['STAR', 'ON GITHUB'], view: [0.3, 0.05], light: [-0.5, 0.6, 0.8] };
  function starPath(ctx, k, scale) {  // (the star of app.css's --star-paper: 100 units, turned -12 degrees)
    ctx.beginPath();
    for (let i = 0; i < 10; i++) {
      const a = deg(-90 + 36 * i - 12), r = (i % 2 ? 17.5 : 40) * scale;
      const x = (50 + r * Math.cos(a)) * k, y = (52 + r * Math.sin(a)) * k;
      if (i) ctx.lineTo(x, y); else ctx.moveTo(x, y);
    }
    ctx.closePath();
  }
  function stickerMap() {
    const k = ART / 100;
    const layer = (draw, blur) => {
      const c = el('canvas');
      c.width = c.height = ART;
      const x = c.getContext('2d', { willReadFrequently: true });
      if (blur) x.filter = 'blur(' + blur + 'px)';
      x.fillStyle = x.strokeStyle = '#ffffff';
      x.lineJoin = 'round';
      draw(x);
      return x.getImageData(0, 0, ART, ART).data;
    };
    const font = (w, px) => w + ' ' + px + 'px -apple-system, BlinkMacSystemFont, "Helvetica Neue", Arial, sans-serif';
    const vinyl = (x) => { starPath(x, k, 1); x.lineWidth = 14 * k; x.fill(); x.stroke(); };
    const foil = (x) => { starPath(x, k, 1); x.lineWidth = 3 * k; x.fill(); x.stroke(); };
    const ridge = (x) => { starPath(x, k, 0.8); x.lineWidth = 0.8 * k; x.stroke(); };
    const ink = (x) => {
      const [big, small] = sticker.text;
      x.translate(50 * k, 53 * k);
      x.rotate(deg(-12));
      x.textAlign = 'center';
      x.font = font(800, (big.length > 3 ? 11.5 : 14.5) * k);
      x.fillText(big, 0, 2.5 * k);
      x.font = font(750, 3.6 * k);
      if ('letterSpacing' in x) x.letterSpacing = (0.5 * k) + 'px';
      x.fillText(small, 0, 8.6 * k);
    };
    const V = layer(vinyl), F = layer(foil), I = layer(ink), Vb = layer(vinyl, 0.9 * k), Fb = layer(foil, 0.3 * k), Rb = layer(ridge, 0.35 * k);
    const out = new Uint8Array(ART * ART * 4);
    for (let i = 3; i < out.length; i += 4) {
      out[i - 3] = V[i]; out[i - 2] = F[i]; out[i - 1] = I[i];
      out[i] = Math.min(255, Vb[i] * 0.6 + Fb[i] * 0.16 + Rb[i] * 0.3 + I[i] * 0.04);
    }
    return out;
  }
  const STICKER_FS = `precision highp float;
    varying vec2 uv;
    uniform sampler2D map;
    uniform vec3 light;  // toward the light, in the sticker's space (x right, y up, z out of it)
    uniform vec2 view;   // where the viewer is, as the calculator turns
    uniform float px;
    uniform float shadowed;  // (the pieces round the edge overlap a little: only the flat ones draw the shadow)
    float H(vec2 q) { return texture2D(map, q).a; }
    float hash(vec2 q) { return fract(sin(dot(q, vec2(12.9898, 78.233))) * 43758.5453); }
    float blob(vec2 q, vec2 c, float r) { vec2 d = q - c; return exp(-dot(d, d) / r); }
    // the room behind the viewer, as a mirror at this angle sees it: brighter above, a soft box up left, a
    // strip light across the ceiling, a window to the right, something dark low down
    float room(vec3 R) {
      vec2 q = R.xy / max(R.z, 0.2);
      float e = 0.34 + 0.24 * clamp(q.y + 0.35, 0.0, 1.0);
      e += 1.3 * blob(q, vec2(-0.55, 0.12), 0.035);
      e += 0.85 * exp(-pow((q.y - 0.3 + 0.18 * q.x) / 0.035, 2.0));
      e += 0.75 * blob(q, vec2(0.22, -0.08), 0.02);
      e -= 0.24 * blob(q, vec2(-0.25, -0.5), 0.06);
      return max(e, 0.05);
    }
    void main() {
      vec4 m = texture2D(map, uv);
      float g = hash(floor(uv * 640.0));
      vec3 n = normalize(vec3((H(uv - vec2(px, 0.0)) - H(uv + vec2(px, 0.0))) * 9.0 + (g - 0.5) * 0.05 * m.g,
        (H(uv + vec2(0.0, px)) - H(uv - vec2(0.0, px))) * 9.0 + (hash(floor(uv * 640.0) + 7.0) - 0.5) * 0.05 * m.g, 1.0));
      vec2 p = (uv - 0.5) * vec2(1.0, -1.0);
      vec3 V = normalize(vec3(view - p * 0.9, 1.0)), L = normalize(light), R = reflect(-V, n);
      float e = room(R) + 1.1 * pow(max(dot(R, L), 0.0), 16.0);
      float flake = pow(g, 28.0) * pow(max(dot(R, L), 0.0), 3.0) * 1.4;
      vec3 gold = vec3(1.0, 0.76, 0.33);
      vec3 foil = gold * e * (0.92 + 0.16 * g) + vec3(1.0, 0.95, 0.82) * (pow(max(e - 0.9, 0.0), 2.0) * 0.5 + flake);
      float diff = max(dot(n, L), 0.0);
      vec3 vinyl = vec3(0.968, 0.964, 0.955) * (0.8 + 0.2 * diff) * (0.985 + 0.03 * hash(floor(uv * 860.0)));
      vec3 ink = vec3(0.2, 0.12, 0.02) * (0.85 + 0.15 * diff);
      vec3 c = mix(vinyl, foil, m.g);
      c = mix(c, ink, m.b * m.g);
      vec3 h = normalize(L + V);
      c += pow(max(dot(n, h), 0.0), 160.0) * 0.6 + pow(1.0 - max(dot(n, V), 0.0), 4.0) * 0.1;
      float shadow = shadowed * (1.0 - m.r) * texture2D(map, uv + vec2(-light.x, light.y) * 2.5 * px).a * 0.34;
      gl_FragColor = vec4(c * m.r, m.r + shadow);
    }`;
  function stickerRenderer() {
    const c = el('canvas');
    c.width = c.height = ART;
    const gl = c.getContext('webgl', { premultipliedAlpha: true, preserveDrawingBuffer: true, antialias: false });
    if (!gl) {  // (no WebGL: the same sticker, flat)
      const x = c.getContext('2d'), k = ART / 100;
      x.lineJoin = 'round';
      return () => {
        x.clearRect(0, 0, ART, ART);
        starPath(x, k, 1); x.lineWidth = 14 * k; x.fillStyle = x.strokeStyle = '#f6f5f2'; x.fill(); x.stroke();
        const gr = x.createLinearGradient(0, 0, ART, ART);
        gr.addColorStop(0, '#fbe7a1'); gr.addColorStop(0.5, '#c8901a'); gr.addColorStop(1, '#f2cf6a');
        starPath(x, k, 1); x.lineWidth = 3 * k; x.fillStyle = x.strokeStyle = gr; x.fill(); x.stroke();
        return c;
      };
    }
    const sh = (type, src) => { const o = gl.createShader(type); gl.shaderSource(o, src); gl.compileShader(o); return o; };
    const prog = gl.createProgram();
    gl.attachShader(prog, sh(gl.VERTEX_SHADER, 'attribute vec2 p; varying vec2 uv; void main() { uv = vec2(p.x * 0.5 + 0.5, 0.5 - p.y * 0.5); gl_Position = vec4(p, 0.0, 1.0); }'));
    gl.attachShader(prog, sh(gl.FRAGMENT_SHADER, STICKER_FS));
    gl.linkProgram(prog);
    gl.useProgram(prog);
    gl.bindBuffer(gl.ARRAY_BUFFER, gl.createBuffer());
    gl.bufferData(gl.ARRAY_BUFFER, new Float32Array([-1, -1, 1, -1, -1, 1, -1, 1, 1, -1, 1, 1]), gl.STATIC_DRAW);
    const p = gl.getAttribLocation(prog, 'p');
    gl.enableVertexAttribArray(p);
    gl.vertexAttribPointer(p, 2, gl.FLOAT, false, 0, 0);
    gl.bindTexture(gl.TEXTURE_2D, gl.createTexture());
    for (const [k, v] of [[gl.TEXTURE_MIN_FILTER, gl.LINEAR], [gl.TEXTURE_MAG_FILTER, gl.LINEAR], [gl.TEXTURE_WRAP_S, gl.CLAMP_TO_EDGE], [gl.TEXTURE_WRAP_T, gl.CLAMP_TO_EDGE]])
      gl.texParameteri(gl.TEXTURE_2D, k, v);
    const upload = (data) => gl.texImage2D(gl.TEXTURE_2D, 0, gl.RGBA, ART, ART, 0, gl.RGBA, gl.UNSIGNED_BYTE, data);
    sticker.lose = () => { const e = gl.getExtension('WEBGL_lose_context'); if (e) e.loseContext(); };
    upload(stickerMap());
    sticker.remap = () => upload(stickerMap());
    const u = (n) => gl.getUniformLocation(prog, n);
    gl.uniform1f(u('px'), 1 / ART);
    gl.viewport(0, 0, ART, ART);
    return (shadowed) => {
      gl.uniform1f(u('shadowed'), shadowed ? 1 : 0);
      gl.uniform3fv(u('light'), sticker.light);
      gl.uniform2fv(u('view'), sticker.view);
      gl.clearColor(0, 0, 0, 0);
      gl.clear(gl.COLOR_BUFFER_BIT);
      gl.drawArrays(gl.TRIANGLES, 0, 6);
      return c;
    };
  }
  // draw the sticker, then each of its pieces on the calculator
  function paintSticker() {
    if (!sticker.draw) return;
    for (const shadowed of [true, false]) {
      const src = sticker.draw(shadowed);
      for (const l of sticker.leaves) if (l.shadowed === shadowed) { l.x.clearRect(0, 0, ART, ART); l.x.drawImage(src, 0, 0); }
    }
  }

  // The keys at the top of the keypad, where the photo has them (measured from it): the D-pad's cross with
  // its corners rounded (a quadratic curve through each corner), OK and Back's circles, and pills.
  const KEY_H = 12;
  function roundPoly(pts, radii, n) {
    const out = [];
    pts.forEach((p, i) => {
      const a = pts[(i + pts.length - 1) % pts.length], b = pts[(i + 1) % pts.length], r = radii[i];
      const da = Math.hypot(a[0] - p[0], a[1] - p[1]), db = Math.hypot(b[0] - p[0], b[1] - p[1]);
      const p1 = [p[0] + (a[0] - p[0]) * r / da, p[1] + (a[1] - p[1]) * r / da], p2 = [p[0] + (b[0] - p[0]) * r / db, p[1] + (b[1] - p[1]) * r / db];
      for (let k = 0; k <= n; k++) {
        const t = k / n, s = 1 - t;
        out.push([s * s * p1[0] + 2 * s * t * p[0] + t * t * p2[0], s * s * p1[1] + 2 * s * t * p[1] + t * t * p2[1]]);
      }
    });
    return out;
  }
  const circlePts = (cx, cy, r, n = 40) => Array.from({ length: n }, (_, i) => [cx + r * Math.cos(i * 2 * Math.PI / n), cy + r * Math.sin(i * 2 * Math.PI / n)]);
  const pill = (x0, y0, x1, y1, r, n = 7) => roundPoly([[x0, y0], [x1, y0], [x1, y1], [x0, y1]], [r, r, r, r], n);
  // an outline moved out by d (along each corner's bisector), for the hole a key stands in
  function grow(pts, d) {
    const nrm = (a, b) => { const dx = b[0] - a[0], dy = b[1] - a[1], l = Math.hypot(dx, dy) || 1; return [dy / l, -dx / l]; };
    return pts.map((p, i) => {
      const a = pts[(i + pts.length - 1) % pts.length], b = pts[(i + 1) % pts.length];
      const n1 = nrm(a, p), n2 = nrm(p, b), bx = n1[0] + n2[0], by = n1[1] + n2[1], l = Math.hypot(bx, by) || 1;
      const k = d / Math.max(0.35, (bx * n1[0] + by * n1[1]) / l);
      return [p[0] + bx / l * k, p[1] + by / l * k];
    });
  }
  // a shape as a mask (outlines in the photo's pixels, drawn on a canvas, w x h from x0, y0; evenodd: holes)
  function maskOf(outlines, x0, y0, w, h, scale, evenodd) {
    const c = el('canvas');
    c.width = Math.ceil(w * scale); c.height = Math.ceil(h * scale);
    const x = c.getContext('2d');
    x.scale(scale, scale);
    x.translate(-x0, -y0);
    x.beginPath();
    for (const pts of outlines) { pts.forEach(([px, py], i) => (i ? x.lineTo(px, py) : x.moveTo(px, py))); x.closePath(); }
    x.fill(evenodd ? 'evenodd' : 'nonzero');
    return 'url(' + c.toDataURL() + ')';
  }
  function keyOutlines() {
    // Measured from the photo: each key's outline runs along the outer edge of the dark line the photo draws
    // round it, so its top carries that line and none of the front's white. Its sides follow a coarser outline
    // (fewer walls) laid just outside it, and it stands in a hole a hair wider, the gap only a crease.
    const cross = [[193, 797], [266.5, 797], [266.5, 887.5], [355.5, 887.5], [355.5, 957.5], [266.5, 957.5], [266.5, 1050.5], [193, 1050.5],
      [193, 957.5], [104, 957.5], [104, 887.5], [193, 887.5]];
    const cr = [22, 22, 7.5, 22, 22, 7.5, 22, 22, 7.5, 22, 22, 7.5];
    const white = [222, 222, 219];
    const keys = [
      { id: 'dpad', pts: roundPoly(cross, cr, 6), walls: grow(roundPoly(cross, cr, 3), 0.6), side: white },
      { id: 'home', box: [439.5, 817.5, 578.5, 906.5], side: [212, 148, 34] },
      { id: 'power', box: [439.5, 940.5, 578.5, 1029.5], side: [36, 36, 38] },
      { id: 'ok', pts: circlePts(718.5, 924, 56.5), walls: circlePts(718.5, 924, 57, 20), side: white },
      { id: 'back', pts: circlePts(858, 924, 56.5), walls: circlePts(858, 924, 57, 20), side: white },
    ];
    const xs = [[102.5, 216.5], [241.5, 356.5], [381.5, 495.5], [521.5, 635.5], [661.5, 775.5], [800.5, 914.5]];
    [[1092.5, 1169.5], [1204.5, 1281.5], [1316.5, 1393.5]].forEach(([y0, y1], r) =>
      xs.forEach(([x0, x1], c) => keys.push({ id: 'row' + r + c, box: [x0, y0, x1, y1], side: white })));
    for (const k of keys) {
      if (k.box) {
        const [x0, y0, x1, y1] = k.box, r = (y1 - y0) / 2;
        k.pts = pill(x0, y0, x1, y1, r);
        k.walls = grow(pill(x0, y0, x1, y1, r, 3), 0.6);
      }
      k.hole = grow(k.pts, 0.4);
    }
    return keys;
  }
  // each key: its top, cut from the photo, standing on its sides, a wall at each step of its outline lit by
  // the calculator's light, a little darker where it meets the front and dark down in its hole. Under them
  // all, the keypad's bed. OK and Back carry Install and Download.
  const KEY_DOWN = 8;
  function keys3d(calc, keys) {
    part(calc, 'i', 'keybed', { left: U(90), top: U(780), width: U(840), height: U(1220), transform: 'translateZ(' + U(-KEY_DOWN) + ')' });
    for (const k of keys) {
      const xs = k.pts.map((p) => p[0]), ys = k.pts.map((p) => p[1]);
      const x0 = Math.floor(Math.min(...xs)) - 1, y0 = Math.floor(Math.min(...ys)) - 1;
      const w = Math.ceil(Math.max(...xs)) + 1 - x0, h = Math.ceil(Math.max(...ys)) + 1 - y0;
      const g = part(calc, 'div', 'key3d', { left: U(x0), top: U(y0), width: U(w), height: U(h) });
      g.dataset.key = k.id;
      k.walls.forEach(([x, y], i) => {
        const [x2, y2] = k.walls[(i + 1) % k.walls.length], len = Math.hypot(x2 - x, y2 - y);
        if (len < 0.01) return;
        const phi = Math.atan2(y2 - y, x2 - x), n = [Math.sin(phi), -Math.cos(phi), 0];  // (facing out)
        const tone = (f) => 'rgb(' + k.side.map((v) => Math.round(Math.min(255, v * lit(n) * f))).join(',') + ')';
        const at = (z) => (100 * (z + KEY_DOWN) / (KEY_H + KEY_DOWN)).toFixed(1) + '%';
        part(g, 'i', 'kwall', { left: U(x - x0), top: U(y - y0), width: U(len + 1.4), height: U(KEY_H + KEY_DOWN),  // (over its neighbours: no seams)
          transform: 'rotateZ(' + (phi * 180 / Math.PI).toFixed(2) + 'deg) translate3d(' + U(-0.7) + ', 0, ' + U(-KEY_DOWN) + ') rotateX(90deg)',
          background: 'linear-gradient(' + tone(0.4) + ', ' + tone(0.78) + ' ' + at(0) + ', ' + tone(0.94) + ' ' + at(KEY_H * 0.45) + ', ' + tone(1) + ')' });
      });
      const top = part(g, 'i', 'ktop', { transform: 'translateZ(' + U(KEY_H) + ')' });
      top.style.setProperty('--key', maskOf([k.pts], x0, y0, w, h, 3));
      const img = el('img', '', { src: 'calculator.webp', alt: '', draggable: 'false' });
      Object.assign(img.style, { left: U(-x0), top: U(-y0), width: U(1018), height: U(2000) });
      top.appendChild(img);
    }
    for (const [key, b] of [['ok', $('#install')], ['back', $('#download')]]) {
      calc.querySelector('.key3d[data-key="' + key + '"]').appendChild(b);
      b.style.transform = 'translateZ(' + U(KEY_H + 0.5) + ')';
    }
  }

  function build3d() {
    const body = document.querySelector('.body3d'), star = $('#star');
    // the rounded edges and the sides: the quarter circle in facets, then the side
    const step = 90 / EDGE_N;
    for (const dir of [1, -1]) {
      let f = part(body, 'i', 'fold', { left: U(dir > 0 ? 1018 - EDGE_R : EDGE_R), transform: 'rotateY(' + step / 2 * dir + 'deg)' });
      EDGE_A.forEach((a, i) => {
        const n0 = [Math.sin(deg(a - step / 2)) * dir, 0, Math.cos(deg(a - step / 2))], n1 = [Math.sin(deg(a + step / 2)) * dir, 0, Math.cos(deg(a + step / 2))];
        part(f, 'i', 'facet', { left: dir > 0 ? '0' : U(-EDGE_C - 1.2), width: U(EDGE_C + 1.2),
          background: 'linear-gradient(' + (dir > 0 ? 90 : 270) + 'deg, ' + grey(n0, 248) + ', ' + grey(n1, 248) + ')' });
        f = part(f, 'i', 'fold', { left: U(EDGE_C * dir), transform: 'rotateY(' + (i < EDGE_N - 1 ? step : step / 2) * dir + 'deg)' });
      });
      const side = DEPTH - EDGE_R, n = [dir, 0, 0];  // (its front edge meets the last facet's light)
      part(f, 'i', 'facet', { left: dir > 0 ? '0' : U(-side), width: U(side),
        background: 'linear-gradient(' + (dir > 0 ? 90 : 270) + 'deg, ' + grey(n, 246) + ', ' + grey(n, 240) + ' 46%, ' + grey(n, 214) + ' 47%, ' +
          grey(n, 236) + ' 48.5%, ' + grey(n, 222) + ')' });
    }
    // the top, its ends following the rounded edges
    const prof = [[0, 0]];
    for (const a of EDGE_A) { const [x, y] = prof[prof.length - 1]; prof.push([x + EDGE_C * Math.cos(deg(a)), y + EDGE_C * Math.sin(deg(a))]); }
    const right = prof.map(([x, y]) => [1018 - EDGE_R + x, y]), left = prof.map(([x, y]) => [EDGE_R - x, y]);
    const poly = right.concat([[1018, DEPTH], [0, DEPTH]], left.slice().reverse())
      .map(([x, y]) => (100 * x / 1018).toFixed(3) + '% ' + U(y)).join(', ');
    part(body, 'i', 'top', { height: U(DEPTH), clipPath: 'polygon(' + poly + ')',
      background: 'linear-gradient(' + grey([0, -1, 0], 244) + ', ' + grey([0, -1, 0], 236) + ' 46%, ' + grey([0, -1, 0], 212) + ' 47%, ' +
        grey([0, -1, 0], 232) + ' 48.5%, ' + grey([0, -1, 0], 220) + ')' });
    // the walls of the screen's recess, all the way round its rounded corners
    const X0 = 104, X1 = 915, Y0 = 107, Y1 = 743, R = 36, ring = [];
    const corner = (cx, cy, a0) => { for (let i = 0; i <= 4; i++) { const a = deg(a0 + 22.5 * i); ring.push([cx + R * Math.cos(a), cy + R * Math.sin(a)]); } };
    corner(X1 - R, Y0 + R, -90); corner(X1 - R, Y1 - R, 0); corner(X0 + R, Y1 - R, 90); corner(X0 + R, Y0 + R, 180);
    ring.forEach(([x, y], i) => {
      const [x2, y2] = ring[(i + 1) % ring.length], len = Math.hypot(x2 - x, y2 - y);
      if (len < 0.01) return;
      const phi = Math.atan2(y2 - y, x2 - x), n = [-Math.sin(phi), Math.cos(phi), 0];  // (facing into the recess)
      part(body, 'i', 'rim', { left: U(x), top: U(y), width: U(len + 0.6), height: U(14.6),
        transform: 'rotateZ(' + (phi * 180 / Math.PI).toFixed(2) + 'deg) rotateX(-90deg)',
        background: 'linear-gradient(' + grey(n, 236) + ', ' + grey(n, 214) + ')' });
    });
    // the keys, standing up off the front, each a hole in its photo
    const keys = keyOutlines();
    keys3d(document.querySelector('#calc'), keys);
    $('#calc').style.setProperty('--plate-mask', maskOf([[[0, 0], [1018, 0], [1018, 2000], [0, 2000]], pill(104, 107, 915, 743, 36)]
      .concat(keys.map((k) => k.hole)), 0, 0, 1018, 2000, 2, true));
    // the sticker: each piece shows its own span of the one artwork
    const S = STAR;
    Object.assign(star.style, { left: (100 * S.lip / 1018) + '%', top: (100 * S.top / 2000) + '%', width: U(S.edge - S.lip), height: U(S.size) });
    star.style.setProperty('--star', U(S.size));
    const paper = el('canvas');
    paper.width = paper.height = 256;
    const px = paper.getContext('2d');
    starPath(px, 2.56, 1);
    px.lineWidth = 14 * 2.56; px.lineJoin = 'round'; px.fill(); px.stroke();
    star.style.setProperty('--star-paper', 'url(' + paper.toDataURL() + ')');
    const art = (key) => {
      const a = el('canvas', 'art');
      a.width = a.height = ART;
      sticker.leaves.push({ x: a.getContext('2d'), shadowed: ['front', 'step', 'floor'].includes(key) });
      return a;
    };
    const leaf = (parent, from, to, x, key, over = 0) => {  // (over: how far it runs under the next piece)
      const l = part(parent, 'span', 'leaf', { left: U(x), width: U(to - from + over) });
      const a = art(key);
      a.style.left = U(S.from - from);
      l.appendChild(a);
      const sh = part(l, 'span', 'shade', { left: U(S.from - from), backgroundSize: U(to - from + over) + ' 100%', backgroundPosition: U(from - S.from) + ' 0' });
      sh.dataset.piece = key;
      sh.dataset.end = (100 * (to - from) / (to - from + over)).toFixed(2);  // (its light runs on over the overlap)
      return l;
    };
    const fold = (parent, x, transform) => part(parent, 'span', 'fold', { left: U(x), transform });
    leaf(star, S.lip, S.edge, 0, 'front', 1.5);
    let f = fold(star, 0, 'rotateY(-90deg)');  // down the step...
    leaf(f, S.lip - S.step, S.lip, -S.step, 'step');
    f = fold(f, -S.step, 'rotateY(90deg)');  // ...onto the recess's floor
    leaf(f, S.from, S.lip - S.step, -(S.lip - S.step - S.from), 'floor');
    let g = fold(star, S.edge - S.lip, 'rotateY(var(--b0))');  // round the edge, onto the side
    let u = S.edge;
    EDGE_A.forEach((a, i) => {
      leaf(g, u, u + EDGE_C, 0, 'bend' + i, 1.5);  // (a little over the next, so no hairline shows between them)
      u += EDGE_C;
      g = fold(g, EDGE_C, 'rotateY(var(--b' + (i < EDGE_N - 1 ? 1 : 2) + '))');
    });
    leaf(g, u, S.from + S.size, 0, 'side');
    shadeStar(0);
    sticker.draw = stickerRenderer();
    paintSticker();
  }
  // how much light each piece of the sticker gets, as the part round the edge lifts (0 to 1)
  function shadeStar(lift) {
    const k = 1 - 0.85 * lift, step = 90 / EDGE_N * k, star = $('#star');
    star.style.setProperty('--b0', (step / 2).toFixed(2) + 'deg');
    star.style.setProperty('--b1', step.toFixed(2) + 'deg');
    star.style.setProperty('--b2', (step / 2).toFixed(2) + 'deg');
    // (each piece from the light at its one edge to the light at its other, so the curve has no bands)
    const at = (a) => lit([Math.sin(deg(a)), 0, Math.cos(deg(a))]);
    const dark = (l) => 'rgba(20, 14, 0, ' + Math.max(0, 1 - l).toFixed(3) + ')';
    const light = { front: [1, 1], step: [lit([-1, 0, 0]) * 0.94, lit([-1, 0, 0]) * 0.94], floor: [0.97, 0.97], side: [at(90 * k), at(90 * k) * 0.97] };
    EDGE_A.forEach((a, i) => { light['bend' + i] = [at((a - 45 / EDGE_N) * k), at((a + 45 / EDGE_N) * k)]; });
    for (const sh of star.querySelectorAll('.shade')) {
      const [l0, l1] = light[sh.dataset.piece];
      sh.style.backgroundImage = 'linear-gradient(90deg, ' + dark(l0) + ', ' + dark(l1) + ' ' + sh.dataset.end + '%, ' + dark(l1) + ')';
    }
  }

  // The calculator faces you straight, and turns a little toward the pointer when it's on it. The light on
  // the foil comes from where the pointer is, and under the pointer the part of the sticker round the edge
  // peels up off the side.
  function depth() {
    if (matchMedia('(prefers-reduced-motion: reduce)').matches || light.on) return;
    const calc = $('#calc'), star = $('#star'), dev = document.querySelector('.device');
    let mx = innerWidth * 0.6, my = innerHeight * 0.2, tx = 0, ty = 0, x = 0, y = 0;
    let over = false, lift = 0, raf = 0;
    const clamp = (v, a, b) => Math.max(a, Math.min(b, v));
    const draw = (now) => {
      raf = 0;
      if (light.on) return;
      frameTime(now);
      x += (tx - x) * 0.07; y += (ty - y) * 0.07;
      calc.style.setProperty('--ry', (x * 18).toFixed(2) + 'deg');
      calc.style.setProperty('--rx', (-y * 10).toFixed(2) + 'deg');
      calc.style.setProperty('--lx', (50 - x * 100).toFixed(1) + '%');
      const r = star.getBoundingClientRect(), cx = r.left + r.width * 0.68, cy = r.top + r.height / 2;
      const want = over ? 0.45 + 0.55 * clamp((mx - r.left) / (r.width * 1.3), 0, 1) : 0;
      lift += (want - lift) * 0.14;
      // (the pointer is the light; the calculator's turn is where the sticker is seen from)
      sticker.light = [(mx - cx) / 260, -(my - cy) / 260, 0.85];
      sticker.view = [-x * 18 / 38 + 0.12, -y * 10 / 38];
      paintSticker();
      shadeStar(lift);
      if (Math.abs(tx - x) + Math.abs(ty - y) > 0.0005 || Math.abs(want - lift) > 0.002) raf = requestAnimationFrame(draw);
    };
    const kick = () => { if (!raf && !light.on) raf = requestAnimationFrame(draw); };
    addEventListener('pointermove', (e) => {
      if (e.pointerType === 'touch') return;
      mx = e.clientX; my = e.clientY;
      const r = dev.getBoundingClientRect(), on = mx >= r.left && mx <= r.right && my >= r.top && my <= r.bottom;
      tx = on ? clamp((mx - r.left) / r.width - 0.5, -0.5, 0.5) : 0;
      ty = on ? clamp((my - r.top) / r.height - 0.4, -0.5, 0.5) : 0;
      kick();
    }, { passive: true });
    document.documentElement.addEventListener('pointerleave', () => { tx = 0; ty = 0; kick(); });
    star.addEventListener('pointerenter', (e) => { if (e.pointerType !== 'touch') { over = true; kick(); } });
    star.addEventListener('pointerleave', () => { over = false; kick(); });
    kick();
  }

  // ------------------------------------------------------------------ the light calculator
  // The 3D calculator is heavy for slow computers and phones. The light one is the same photo, flat: OK and Back
  // go in when pressed, and the sticker is drawn once, as the 3D one looks from the front. It's the light one from
  // the start on small or touch screens, with reduced motion, or with ?flat in the address (?3d: never); otherwise
  // the page times its frames while things move, in its first seconds and as the calculator turns, and goes light
  // for the rest of the visit when they're slow.
  const light = { on: false, later: false, list: [], last: 0 };
  const flag = (f) => new RegExp('[?&]' + f + '(=|&|$)').test(location.search);
  function lightFromStart() {
    if (flag('3d')) return false;
    if (flag('flat')) return true;
    try { if (sessionStorage.getItem('numplay-flat')) return true; } catch (e) { /* (no storage) */ }
    return innerWidth < 900 || matchMedia('(pointer: coarse)').matches || matchMedia('(prefers-reduced-motion: reduce)').matches;
  }
  // each frame drawn while something moves: when half of 30 in a row take over 30 ms (fewer than 33 a second), slow
  function frameTime(now) {
    if (light.on || flag('3d') || now - light.last < 1) return;
    if (document.hidden) { light.last = 0; return; }
    if (light.last && now - light.last < 500) light.list.push(now - light.last);
    light.last = now;
    if (light.list.length < 30) return;
    const median = light.list.sort((a, b) => a - b)[15];
    light.list = [];
    if (median > 30) goLight();
  }
  function watchFrames() {  // (the first seconds: the page coming up, and the intro the first time)
    const t0 = performance.now();
    const tick = (now) => {
      if (light.on || now - t0 > 6000) { light.last = 0; return; }
      if (now - t0 > 700) frameTime(now);
      requestAnimationFrame(tick);
    };
    requestAnimationFrame(tick);
  }
  function goLight() {
    if (light.on) return;
    if (introRun) { light.later = true; return; }  // (once the intro is over)
    try { sessionStorage.setItem('numplay-flat', '1'); } catch (e) { /* (no storage: light until the page is left) */ }
    // the 3D calculator goes: its sides, its keys (OK and Back keep their buttons), its sticker
    const calc = $('#calc'), plate = calc.querySelector('.plate');
    for (const b of [$('#install'), $('#download')]) { b.style.transform = ''; plate.appendChild(b); }
    for (const e of calc.querySelectorAll('.key3d, .keybed')) e.remove();
    document.querySelector('.body3d').textContent = '';
    if (sticker.lose) sticker.lose();
    Object.assign(sticker, { draw: null, remap: null, lose: null, leaves: [] });
    const star = $('#star');
    star.textContent = '';
    star.removeAttribute('style');
    for (const v of ['--plate-mask', '--rx', '--ry', '--lx']) calc.style.removeProperty(v);
    buildFlat();
  }
  function buildFlat() {
    light.on = true;
    document.documentElement.classList.add('flat');
    const plate = $('#calc').querySelector('.plate'), r = 57.5, cy = 924;
    for (const [key, b, cx] of [['ok', $('#install'), 718.5], ['back', $('#download'), 858]]) {
      const k = part(plate, 'div', 'fkey', { left: U(cx - r), top: U(cy - r), width: U(2 * r), height: U(2 * r) });
      k.dataset.key = key;
      const img = el('img', '', { src: 'calculator.webp', alt: '', draggable: 'false' });
      Object.assign(img.style, { left: U(r - cx), top: U(r - cy), width: U(1018), height: U(2000) });
      part(k, 'i', 'ftop', {}).appendChild(img);
      k.appendChild(b);
    }
    flatStar();
  }
  // The sticker, flat: its picture (the white vinyl, the gold foil and the line embossed in it, the ink), then seen
  // from the front as it lies on the calculator: over the screen's bezel, a little darker down in it, and round the
  // edge, squeezed and darker as the edge turns away; the part on the side is out of sight.
  function stickerArt() {
    const c = el('canvas');
    c.width = c.height = ART;
    const x = c.getContext('2d'), k = ART / 100;
    x.lineJoin = 'round';
    x.save();
    x.shadowColor = 'rgba(0, 0, 0, 0.3)'; x.shadowBlur = 1.4 * k; x.shadowOffsetY = 0.5 * k;
    starPath(x, k, 1); x.lineWidth = 14 * k; x.fillStyle = x.strokeStyle = '#f7f6f2'; x.fill(); x.stroke();
    x.restore();
    const g = x.createLinearGradient(14 * k, 8 * k, 84 * k, 96 * k);
    [[0, '#fff0b5'], [0.2, '#e8bd52'], [0.42, '#b47e14'], [0.6, '#f4d47c'], [0.78, '#c28b1c'], [1, '#f2d27a']]
      .forEach(([o, col]) => g.addColorStop(o, col));
    starPath(x, k, 1); x.lineWidth = 3 * k; x.fillStyle = x.strokeStyle = g; x.fill(); x.stroke();
    for (const [d, col] of [[0.3, 'rgba(120, 76, 0, 0.55)'], [-0.3, 'rgba(255, 249, 222, 0.75)']]) {
      x.save(); x.translate(d * k, d * k); starPath(x, k, 0.8); x.lineWidth = 0.7 * k; x.strokeStyle = col; x.stroke(); x.restore();
    }
    const [big, small] = sticker.text;
    x.save();
    x.translate(50 * k, 53 * k);
    x.rotate(deg(-12));
    x.textAlign = 'center';
    x.fillStyle = 'rgba(52, 31, 4, 0.92)';
    const font = (w, px) => w + ' ' + px + 'px -apple-system, BlinkMacSystemFont, "Helvetica Neue", Arial, sans-serif';
    x.font = font(800, (big.length > 3 ? 11.5 : 14.5) * k);
    x.fillText(big, 0, 2.5 * k);
    x.font = font(750, 3.6 * k);
    if ('letterSpacing' in x) x.letterSpacing = (0.5 * k) + 'px';
    x.fillText(small, 0, 8.6 * k);
    x.restore();
    return c;
  }
  function flatStar() {
    // (the step down into the bezel, which the front doesn't see, shifts it as much: its tip at the screen's edge)
    const S = STAR, star = $('#star'), K = ART / S.size, x0 = S.from + S.step, x1 = 1018;
    const art = stickerArt(), c = el('canvas'), w = Math.ceil((x1 - x0) * K);
    c.width = w; c.height = ART;
    const x = c.getContext('2d');
    for (let i = 0; i < w; i++) {
      const px = x0 + (i + 0.5) / K;  // (in the photo's pixels)
      let u = px - S.step, du = 1, l = 1;
      if (px < S.lip) l = px > S.lip - 1.6 ? 0.84 : 0.96;  // (down in the bezel, and its edge)
      else if (px > S.edge) {  // (round the edge)
        const a = Math.asin(Math.min(1, (px - S.edge) / EDGE_R));
        u = S.edge - S.step + EDGE_R * a; du = 1 / Math.max(0.2, Math.cos(a)); l = lit([Math.sin(a), 0, Math.cos(a)]);
      }
      x.drawImage(art, (u - S.from) * K - du / 2, 0, du, ART, i, 0, 1, ART);
      if (l < 1) {
        x.save(); x.globalCompositeOperation = 'source-atop'; x.fillStyle = 'rgba(20, 14, 0, ' + (1 - l).toFixed(3) + ')';
        x.fillRect(i, 0, 1, ART); x.restore();
      }
    }
    star.textContent = '';
    Object.assign(star.style, { left: (100 * x0 / 1018) + '%', top: (100 * S.top / 2000) + '%', width: U(x1 - x0), height: U(S.size) });
    star.appendChild(c);
  }

  // ------------------------------------------------------------------ GitHub's star count, for the sticker
  async function stars() {
    try {
      let n = sessionStorage.getItem('numplay-stars');
      if (!n) {
        const r = await fetch('https://api.github.com/repos/Mason363/NumPlay');
        if (!r.ok) return;
        n = String((await r.json()).stargazers_count);
        sessionStorage.setItem('numplay-stars', n);
      }
      const v = +n, t = v >= 1000 ? (v / 1000).toFixed(v >= 10000 ? 0 : 1) + 'k' : String(v);
      sticker.text = [t, v === 1 ? 'STAR' : 'STARS'];
      if (sticker.remap) { sticker.remap(); paintSticker(); }
      if (light.on) flatStar();
    } catch (e) { /* (offline, or GitHub said no: the button still works) */ }
  }

  // ------------------------------------------------------------------ on a phone: install from under your thumb
  // Once the calculator's keys are scrolled away, a bar at the bottom has them, with the space used.
  function dock() {
    const d = $('#dock'), keys = $('.legend.ok');
    if (!window.IntersectionObserver) return;
    new IntersectionObserver(([e]) => {
      const below = !e.isIntersecting && e.boundingClientRect.top < 0;
      d.classList.toggle('show', below);
      document.body.classList.toggle('docked', below);
    }).observe(keys);
  }

  // ------------------------------------------------------------------ the sheet, tips and toasts
  function openSheet(file) {
    const s = $('#sheet');
    for (const f of s.querySelectorAll('.sheet-file')) f.textContent = file;
    for (const li of s.querySelectorAll('.xl-only')) li.hidden = !nwagra();
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
    toastTimer = setTimeout(() => t.classList.remove('show'), 2800);
  }
  function tips() {
    const tip = $('#tip');
    let on = null;
    document.addEventListener('pointerover', (e) => {
      if (e.pointerType === 'touch' || dragging) return;
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

  // ------------------------------------------------------------------ the intro
  // The page puts itself together. The calculator comes up into place, then the name and what NumPlay is,
  // and the games take their places. Then each game you have goes into the calculator, in launcher order:
  // its tick comes on, it lifts off its place, flies into the screen, and its block opens there as the
  // count of space goes up. Last, the big games and your own come in beside it. Any key or click ends it.
  let introRun = null;
  function intro(force) {
    const ready = () => document.documentElement.classList.remove('intro-wait');
    if (matchMedia('(prefers-reduced-motion: reduce)').matches || innerWidth < 900) return ready();
    if (!force && store('numplay-intro3')) return ready();
    store('numplay-intro3', '1');
    if (introRun) introRun();
    const anims = [], flyers = [], E = {
      out: 'cubic-bezier(0.16, 1, 0.3, 1)', spring: 'cubic-bezier(0.34, 1.36, 0.64, 1)', soft: 'cubic-bezier(0.22, 1, 0.36, 1)',
    };
    const anim = (node, kf, opt) => { if (node) anims.push(node.animate(kf, { fill: 'backwards', ...opt })); };
    const rect = (n) => n.getBoundingClientRect();
    hidePreview();
    document.body.classList.add('intro-on');
    $('#tip').classList.remove('show');
    // where everything ends up, before anything moves
    const list = items(), cap = capacity(), used = list.reduce((a, it) => a + it.bytes, 0);
    const games = ordered().filter((id) => buttons.get(id) && buttons.get(id).parentNode.id === 'small');
    const from = new Map(games.map((id) => [id, rect(buttons.get(id).querySelector('img'))]));
    const to = new Map(games.map((id) => [id, tiles.get(id) && rect(tiles.get(id))]));
    // 1. the calculator, then the words, then the games in their places (a wave down the grid)
    anim($('.device'), [{ opacity: 0 }, { opacity: 1 }], { duration: 700, easing: 'linear' });
    anim($('#calc'), [{ translate: '0 80px', scale: '0.93' }, { translate: '0 0', scale: '1' }], { duration: 1400, easing: E.out });
    anim($('.meter'), [{ opacity: 0, transform: 'translateY(24px)' }, { opacity: 1, transform: 'none' }], { duration: 900, delay: 500, easing: E.out });
    anim($('.title span'), [{ transform: 'translateY(108%)' }, { transform: 'none' }], { duration: 1000, delay: 180, easing: E.out });
    anim($('.lede'), [{ opacity: 0, transform: 'translateY(16px)' }, { opacity: 1, transform: 'none' }], { duration: 900, delay: 340, easing: E.out });
    anim($('.chips'), [{ opacity: 0, transform: 'translateY(10px)' }, { opacity: 1, transform: 'none' }], { duration: 800, delay: 420, easing: E.out });
    anim($('.games .row-head'), [{ opacity: 0 }, { opacity: 1 }], { duration: 600, delay: 520, easing: 'linear' });
    [...$('#small').children].forEach((b, i) => {
      anim(b.querySelector('img'), [{ opacity: 0, transform: 'translateY(14px) scale(0.6)' }, { opacity: 1, transform: 'none' }],
        { duration: 700, delay: 560 + i * 24, easing: E.spring });
      if (!st.picked.has(b.dataset.id))  // (their + comes with them)
        anim(b.querySelector('.badge'), [{ opacity: 0 }, { opacity: 1 }], { duration: 300, delay: 900 + i * 24, easing: 'linear' });
    });
    // 2. on the screen: the free space and NumPlay's own block first, the games' blocks as they land
    const T0 = 1150, GAP = 70, FLY = 760, landed = [];
    for (const [key, e] of tiles) {
      const id = key, gi = games.indexOf(id);
      const at = key === '_free' ? 700 : key === '_core' ? 980 : gi >= 0 ? T0 + gi * GAP + FLY * 0.82 : T0 + games.length * GAP + FLY;
      anim(e, [{ opacity: 0, scale: '0.55' }, { opacity: 1, scale: '1' }], { duration: key === '_free' ? 500 : 620, delay: at, easing: key === '_free' ? 'linear' : E.spring });
      const sg = segs.get(key);
      if (sg) anim(sg, [{ height: '0%' }, { height: sg.style.height }], { duration: 560, delay: at, easing: E.out });
      const it = list.find((x) => x.key === key);
      if (it) landed.push([at, it.bytes]);
    }
    // 3. the flights: each game's copy lifts off its place and arcs into its block on the screen
    games.forEach((id, i) => {
      const a = from.get(id), b = to.get(id), btn = buttons.get(id);
      anim(btn.querySelector('.badge'), [{ transform: 'scale(0)' }, { transform: 'scale(1)' }], { duration: 420, delay: T0 + i * GAP - 60, easing: E.spring });
      if (!a || !b) return;
      const f = el('div', 'flyer');
      f.appendChild(el('img', '', { src: btn.querySelector('img').src, alt: '' }));
      f.style.width = a.width + 'px';
      f.style.opacity = '0';
      document.body.appendChild(f);
      const tw = Math.max(10, Math.min(b.width, b.height) * 0.62);
      flyers.push({ f, start: T0 + i * GAP, x0: a.left, y0: a.top, w0: a.width, x1: b.left + b.width / 2 - tw / 2, y1: b.top + b.height / 2 - tw / 2, w1: tw });
    });
    // the count of space and the flights, a frame at a time
    const t0 = performance.now(), ease = (t) => (t < 0.5 ? 4 * t * t * t : 1 - Math.pow(-2 * t + 2, 3) / 2);
    let raf = 0, shown = 0;
    const end = T0 + games.length * GAP + FLY + 300;
    const frame = (now) => {
      const t = now - t0;
      for (const fl of flyers) {
        const k = (t - fl.start) / FLY;
        if (k < 0 || k > 1.05) { fl.f.style.opacity = '0'; continue; }
        const e = ease(Math.min(1, k)), lift = 150 + 0.1 * Math.abs(fl.x1 - fl.x0);
        const cx = (fl.x0 + fl.x1) / 2, cy = Math.min(fl.y0, fl.y1) - lift;
        const x = (1 - e) * (1 - e) * fl.x0 + 2 * (1 - e) * e * cx + e * e * fl.x1;
        const y = (1 - e) * (1 - e) * fl.y0 + 2 * (1 - e) * e * cy + e * e * fl.y1;
        const sc = (fl.w0 + (fl.w1 - fl.w0) * e) / fl.w0;
        fl.f.style.transform = 'translate(' + x.toFixed(1) + 'px,' + y.toFixed(1) + 'px) scale(' + sc.toFixed(3) + ')';
        fl.f.style.opacity = String(k < 0.06 ? k / 0.06 : k > 0.86 ? Math.max(0, (1 - k) / 0.14) : 1);
      }
      const target = landed.reduce((a, [at, bytes]) => a + (t >= at ? bytes : 0), 0);
      shown += (target - shown) * 0.2;
      $('#status-line').textContent = (shown / MiB).toFixed(2) + ' OF ' + +(cap / MiB).toFixed(2) + ' MB';
      $('#used').textContent = (shown / MiB).toFixed(2);
      $('#battery').style.width = 'calc(' + Math.min(100, 100 * shown / cap) + '% - 2px)';
      if (t < end) raf = requestAnimationFrame(frame); else finish();
    };
    raf = requestAnimationFrame(frame);
    ready();
    // 4. beside it: the big games and your own
    const S = T0 + games.length * GAP + 200;
    const side = $('.side');
    [...side.querySelectorAll(':scope > .row-head, #retro > .row-head, .drop, .foot')].forEach((n, i) =>
      anim(n, [{ opacity: 0, transform: 'translateX(26px)' }, { opacity: 1, transform: 'none' }], { duration: 900, delay: S + (i ? 260 + i * 70 : 0), easing: E.out }));
    [...$('#big').children].forEach((b, i) =>
      anim(b, [{ opacity: 0, translate: '18px 0' }, { opacity: 1, translate: '0 0' }], { duration: 800, delay: S + 80 + i * 26, easing: E.out }));
    function finish(e) {
      // (a click that ends it is only that: it doesn't also pick the game under it)
      if (e && e.type === 'pointerdown') addEventListener('click', (c) => { c.stopPropagation(); c.preventDefault(); }, { capture: true, once: true });
      cancelAnimationFrame(raf);
      for (const a of anims) a.finish();
      for (const fl of flyers) fl.f.remove();
      flyers.length = 0;
      document.body.classList.remove('intro-on');
      removeEventListener('pointerdown', finish, true);
      removeEventListener('keydown', finish, true);
      introRun = null;
      hidePreview();
      render();
      if (light.later) goLight();
    }
    addEventListener('pointerdown', finish, true);
    addEventListener('keydown', finish, true);
    introRun = finish;
  }

  // the games in NumPlay fill their column down to the bottom of the window: as many to a row, and as big, as fits
  // (in the narrower layouts, where more comes under them, five to a row)
  function fitGrid() {
    const box = $('#small'), n = box.children.length;
    if (!n || innerWidth <= 1180) { box.classList.remove('fit'); return; }
    const W = box.clientWidth, H = innerHeight - (box.getBoundingClientRect().top + scrollY) - 34;
    let best = null;
    for (let c = 3; c <= 7; c++) {
      const r = Math.ceil(n / c), s = Math.min(W / (c + (c - 1) * 0.22), H / (r + (r - 1) * 0.3));
      if (!best || s > best.s) best = { c, s };
    }
    if (H < 160 || best.s < 36) { box.classList.remove('fit'); return; }
    box.style.setProperty('--cols', best.c);
    box.style.setProperty('--s', Math.floor(Math.min(best.s, 124)) + 'px');
    box.classList.add('fit');
  }

  // ------------------------------------------------------------------ start
  async function start() {
    try {
      kit = await fetch(KIT + 'kit.json').then((r) => { if (!r.ok) throw new Error(r.status); return r.json(); });
    } catch (e) {
      document.documentElement.classList.remove('intro-wait');
      toast("The games didn't load, try again in a minute");
      return;
    }
    kit.langs = kit.langs || ['en'];
    for (const g of kit.games) byId[g.id] = g;
    kit.byId = byId;
    kits.en = kit;
    rg = kit.games.find((g) => g.roms) || null;
    try {
      const c = store('numplay-icon');
      if (c) { const j = JSON.parse(c); custom = { url: j.url, obj: Uint8Array.from(atob(j.obj), (x) => x.charCodeAt(0)) }; }
    } catch (e) { custom = null; }

    const small = $('#small'), big = $('#big');
    const badge = '<span class="badge" aria-hidden="true">' +
      '<svg class="b-tick" viewBox="0 0 12 12"><path d="M2.5 6.2l2.3 2.3 4.7-5" fill="none" stroke="currentColor" stroke-width="2.4" stroke-linecap="round" stroke-linejoin="round"/></svg>' +
      '<svg class="b-plus" viewBox="0 0 12 12"><path d="M6 2.5v7M2.5 6h7" fill="none" stroke="currentColor" stroke-width="2.2" stroke-linecap="round"/></svg></span>';
    for (const g of kit.games) {
      if (g === rg) continue;
      const b = el('button', 'app off', { type: 'button', 'aria-label': g.title, 'aria-pressed': 'false', 'data-id': g.id });
      b.appendChild(el('img', '', { src: KIT + 'icons/' + g.id + '.png', alt: '', draggable: 'false' }));
      b.insertAdjacentHTML('beforeend', badge);
      b.addEventListener('click', () => {
        if (justDragged) { justDragged = false; return; }
        toggle(g.id);
        if (matchMedia('(hover: none)').matches) { showPreview(g.id); hidePreview(1600); }
      });
      b.addEventListener('pointerenter', (e) => { if (e.pointerType !== 'touch') showPreview(g.id); });
      b.addEventListener('pointerleave', (e) => { if (e.pointerType !== 'touch') hidePreview(120); });
      b.addEventListener('focus', () => showPreview(g.id));
      b.addEventListener('contextmenu', (e) => { if (holding || e.pointerType === 'touch') e.preventDefault(); });
      b.addEventListener('blur', () => hidePreview(120));
      (g.xl ? big : small).appendChild(b);
      buttons.set(g.id, b);
    }
    sortable(small);
    sortable(big);
    fitGrid();
    addEventListener('resize', () => requestAnimationFrame(fitGrid));
    if (document.fonts) document.fonts.ready.then(fitGrid);

    // the app's settings
    const sec = $('#secret');
    SECRETS.forEach((s, i) => { const o = el('option', '', { value: String(i) }); o.textContent = s; sec.appendChild(o); });
    sec.addEventListener('change', () => { st.app.secret = +sec.value; render(); });
    $('#hint').addEventListener('change', (e) => { st.app.hint = e.target.checked; render(); });
    $('#app-name').addEventListener('input', (e) => { st.app.name = e.target.value.slice(0, 20); render(); });
    for (const b of document.querySelectorAll('[data-open]'))
      b.addEventListener('click', () => { st.app.open = b.dataset.open; renderApp(); render(); });
    $('#icon-file').addEventListener('change', async (e) => {
      const f = e.target.files[0];
      e.target.value = '';
      if (!f) return;
      try {
        custom = await iconFrom(f);
        store('numplay-icon', JSON.stringify({ url: custom.url, obj: btoa(String.fromCharCode(...custom.obj)) }));
        st.app.icon = 'custom';
        if (PRESETS[st.app.name] === undefined && st.app.name === ' ') st.app.name = 'NumPlay';
        renderApp(); render();
      } catch (err) { toast("That picture didn't work, try a PNG or JPEG"); }
    });
    $('#app-chip').addEventListener('click', (e) => { renderApp(); openPop($('#app-pop'), e.currentTarget); });
    $('#lang-chip').addEventListener('click', (e) => { renderApp(); openPop($('#lang-pop'), e.currentTarget); });
    document.addEventListener('pointerdown', (e) => {
      if (popOpen && !e.target.closest('.pop, .chip, .space-chip')) closePops();
    });

    $('#space-chip').addEventListener('click', (e) => { renderSpaces(); openPop($('#space-pop'), e.currentTarget, true); });
    $('#install').addEventListener('click', (e) => go(e.currentTarget, true));
    $('#download').addEventListener('click', (e) => go(e.currentTarget, false));
    $('#dock-install').addEventListener('click', (e) => go(e.currentTarget, true));
    $('#dock-download').addEventListener('click', (e) => go(e.currentTarget, false));
    dock();
    for (const b of [$('#install'), $('#download'), $('#dock-install'), $('#dock-download'), $('#app-chip'), $('#lang-chip')]) {
      b.addEventListener('pointerenter', prefetch, { once: true });
      b.addEventListener('focus', prefetch, { once: true });
    }
    $('#sheet').addEventListener('click', (e) => { if (e.target.id === 'sheet' || e.target.closest('.sheet-close')) closeSheet(); });
    document.addEventListener('keydown', (e) => {
      if (e.key === 'Escape') { closeSheet(); closePops(); if (holding) { holding.classList.remove('held'); holding = null; } }
    });

    if (rg) {
      $('#retro').hidden = false;
      const exts = Object.keys(EXT).filter((x) => (rg.systems || [1, 2, 3]).includes(EXT[x]));
      $('#rom-file').accept = exts.map((x) => '.' + x).join(',');
      $('#drop-sub').textContent = exts.map((x) => '.' + x).join(' ');
      $('#rom-file').addEventListener('change', (e) => { addRoms([...e.target.files]); e.target.value = ''; });
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
    $('#nwa-file').addEventListener('change', (e) => { addNwas([...e.target.files]); e.target.value = ''; });
    niFixText();
    tips();
    if (lightFromStart()) buildFlat();
    else { build3d(); depth(); watchFrames(); }
    addEventListener('resize', () => { if (innerWidth < 900) goLight(); });
    restore();
    if (st.lang !== 'en') loadKit(st.lang).then(render).catch(() => { st.lang = 'en'; render(); });
    render();
    $('#replay').addEventListener('click', () => intro(true));
    requestAnimationFrame(() => intro(new URLSearchParams(location.search).has('intro')));
    addEventListener('hashchange', () => { restore(); render(); });
    stars();
    (window.requestIdleCallback || setTimeout)(() => loadTools().catch(() => {}), { timeout: 3000 });
  }
  start();
})();
