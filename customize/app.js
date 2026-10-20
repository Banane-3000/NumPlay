/* NumPlay Studio: pick games, add Retro-Go games, and link a NumPlay.nwa of your own in the
 * browser. The kit (kit/, made by `make kit` and `make kit-langs`, tools/kit.py) holds the objects
 * the Makefile links NumPlay.nwa from; GNU ld and objcopy, built as WebAssembly (ld/), link them
 * here the same way. */
'use strict';
(() => {
  const KIT = 'kit/';
  const PREVIEWS = '../docs/media/gallery/';
  const MiB = 1048576;
  // Enlarged Extra Apps (nwagyu's Nwagra): it moves the end of the app area from the end of
  // Epsilon's slot to the end of the 8 MB flash, so apps go from 0x90200000 to 0x90800000
  const XL_SPACE = 6 * MiB;
  const NWAGRA = 'https://nwagyu.org/guide/help/enlarge-your-memory.html';
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
    picked: new Set(), order: [], roms: [], cap: 'std', lang: 'en',
    app: { icon: 'NumPlay', name: 'NumPlay', open: 'numplay', secret: 0, hint: true },
  };
  let custom = null;       // an icon of your own: { url, obj }
  let romKey = 0;

  // ------------------------------------------------------------------ sizes
  const capacity = () => st.cap === 'xl' ? XL_SPACE : kit.appSpace;
  const langKit = () => kits[st.lang] || kit;
  const flash = (id) => ((langKit().byId || {})[id] || byId[id]).flash;
  const romBytes = (r) => ROM_ENTRY + align4(r.bytes.length);
  const iconUrl = () => st.app.icon === 'custom' && custom ? custom.url : KIT + 'variants/' + st.app.icon + '.png';
  const ordered = () => st.order.filter((id) => st.picked.has(id));
  function items() {
    const out = [{ key: '_core', name: st.app.name.trim() || 'NumPlay', bytes: langKit().base, color: '#3a3a3c', icon: iconUrl() }];
    for (const id of ordered()) {
      const g = byId[id];
      out.push({ key: id, id, name: g.title, bytes: flash(id), color: hex(g.top), icon: g.icon ? KIT + 'icons/' + id + '.png' : null });
    }
    if (rg && st.roms.length) {
      out.push({ key: rg.id, id: rg.id, name: rg.title, bytes: flash(rg.id) + ROM_HEAD, color: hex(rg.top), icon: rg.icon ? KIT + 'icons/' + rg.id + '.png' : null });
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
  let shownUsed = 0, tween = 0, previewing = null;

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
        if (t.key === '_core' && img.getAttribute('src') !== t.icon) img.src = t.icon;
        img.style.width = (100 * Math.min(44, s * 0.62) / Math.max(t.w, 1)) + '%';
        img.style.opacity = s >= 13 ? 1 : 0;
      }
      const b = e.querySelector('b');
      if (b) b.style.opacity = s >= 14 ? 1 : 0;
      e.dataset.tip = (t.free ? 'Free' : t.name) + '|' + size(t.bytes);
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
    // (the order of the blocks follows the games' order)
    const want = list.map((it) => segs.get(it.key));
    const live = [...bar.children].filter((c) => want.includes(c));
    if (live.some((c, i) => c !== want[i])) for (const c of want) bar.appendChild(c);
    bar.classList.toggle('over', over);
    $('#screen').classList.toggle('over', over);
    bar.setAttribute('aria-label', 'Space used: ' + size(used) + ' of ' + size(cap));
    $('#screen').setAttribute('aria-label', 'Space on the calculator: ' + size(used) + ' of ' + size(cap) + ', each game a block its size');

    // the numbers
    const from = shownUsed, t0 = performance.now(), cancel = ++tween;
    const step = (now) => {
      if (cancel !== tween) return;
      const k = Math.min(1, Math.max(0, (now - t0) / 220)), e = 1 - Math.pow(1 - k, 3);  // (a frame's time can be before t0)
      shownUsed = from + (used - from) * e;
      $('#used').textContent = (shownUsed / MiB).toFixed(2);
      if (!previewing) $('#status-line').textContent = (shownUsed / MiB).toFixed(2) + ' OF ' + +(cap / MiB).toFixed(2) + ' MB';
      $('#battery').style.width = 'calc(' + Math.min(100, 100 * shownUsed / cap) + '% - 2px)';
      if (k < 1) requestAnimationFrame(step);
    };
    step(t0);

    // the icons, in the order they go in the launcher
    for (const [id, b] of buttons) {
      const g = byId[id], on = st.picked.has(id), fits = used + flash(id) <= cap;
      b.classList.toggle('on', on);
      b.classList.toggle('off', !on && fits);
      b.classList.toggle('nofit', !on && !fits);
      b.setAttribute('aria-pressed', on);
      b.dataset.tip = g.title + '|' + size(flash(id)) + ' · drag to move';
    }
    for (const box of [$('#small'), $('#big')]) {
      const want2 = st.order.map((id) => buttons.get(id)).filter((b) => b && b.parentNode === box);
      if ([...box.children].some((c, i) => c !== want2[i])) for (const b of want2) box.appendChild(b);
    }
    for (const b of document.querySelectorAll('.cap button')) b.setAttribute('aria-checked', b.dataset.cap === st.cap);
    $('#app-chip-icon').src = iconUrl();
    $('#lang-chip-label').textContent = LANGS[st.lang][0];
    renderCarts();
    save();
  }

  function renderCarts() {
    const box = $('#carts');
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
    const src = PREVIEWS + id + '.gif';
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
  function bump(e, cls) {
    if (!e) return;
    e.classList.remove('bounce', 'shake');
    void e.offsetWidth;
    e.classList.add(cls);
    setTimeout(() => e.classList.remove(cls), 320);
  }
  function overflow(bytes) {
    toast("That doesn't fit: " + size(bytes) + ' too much' + (st.cap === 'std' ? '. 6 MB can take more' : ''));
    $('#bar').classList.add('over');
    setTimeout(() => render(), 500);
  }
  function toggle(id, quiet) {
    const b = buttons.get(id);
    if (st.picked.has(id)) st.picked.delete(id);
    else {
      const over = usedBytes() + flash(id) - capacity();
      if (over > 0) { bump(b, 'shake'); if (!quiet) overflow(over); return; }
      st.picked.add(id);
      bump(b, 'bounce');
      if (!quiet) fetchObj(objPath(id)).catch(() => {});  // (ready by the time you install)
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
      const extra = (st.roms.length ? 0 : flash(rg.id) + ROM_HEAD) + romBytes(r);
      const over = usedBytes() + extra - capacity();
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
  function sortable(box) {
    box.addEventListener('pointerdown', (e) => {
      const b = e.target.closest('.app');
      if (!b || e.button || intro.on) return;
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
      dragging = { b, box, x: e.clientX, y: e.clientY, on: false, ghost: null };
    });
    addEventListener('pointermove', (e) => {
      const d = dragging;
      if (!d || d.box !== box) return;
      if (!d.on) {
        if (Math.hypot(e.clientX - d.x, e.clientY - d.y) < 7) return;
        d.on = true;
        hidePreview();
        const r = d.b.getBoundingClientRect();
        d.dx = d.x - r.left; d.dy = d.y - r.top;
        d.ghost = d.b.cloneNode(true);
        d.ghost.className = 'app on ghost';
        Object.assign(d.ghost.style, { position: 'fixed', left: '0', top: '0', width: r.width + 'px', zIndex: 60, pointerEvents: 'none', margin: 0 });
        document.body.appendChild(d.ghost);
        d.b.classList.add('placeholder');
        document.body.classList.add('dragging-icon');
      }
      d.ghost.style.transform = 'translate(' + (e.clientX - d.dx) + 'px,' + (e.clientY - d.dy) + 'px) scale(1.08) rotate(-3deg)';
      const over = document.elementFromPoint(e.clientX, e.clientY);
      const t = over && over.closest('.app');
      if (t && t !== d.b && t.parentNode === box) {
        const kids = [...box.children], i = kids.indexOf(d.b), j = kids.indexOf(t);
        flip(box, () => box.insertBefore(d.b, i < j ? t.nextSibling : t));
      }
    });
    const end = () => {
      const d = dragging;
      if (!d || d.box !== box) return;
      dragging = null;
      if (!d.on) return;
      justDragged = true;
      setTimeout(() => { justDragged = false; }, 0);
      d.b.classList.remove('placeholder');
      document.body.classList.remove('dragging-icon');
      d.ghost.remove();
      commitOrder();
    };
    addEventListener('pointerup', end);
    addEventListener('pointercancel', end);
  }

  // ------------------------------------------------------------------ remembering a pick
  function save() {
    if (intro.on) return;  // (the intro empties the calculator for a moment: not a pick to keep)
    const a = st.app, p = new URLSearchParams();
    p.set('games', ordered().join(','));
    if (st.cap === 'xl') p.set('space', 'xl');
    if (st.lang !== 'en') p.set('lang', st.lang);
    if (a.icon !== 'NumPlay') p.set('icon', a.icon);
    if (a.name !== (PRESETS[a.icon] || [0, 'NumPlay'])[1]) p.set('name', a.name);
    if (a.open === 'matrices') p.set('open', 'matrices' + (a.secret ? '-' + a.secret : '') + (a.hint ? '' : '-nohint'));
    const h = p.toString();
    try { history.replaceState(null, '', '#' + h); } catch (e) { /* (a file:// page) */ }
    store('numplay-studio', h);
    store('numplay-order', st.order.join(','));
  }
  function restore() {
    let h = location.hash.slice(1);
    if (!h) h = store('numplay-studio') || '';
    const p = new URLSearchParams(h);
    const known = kit.games.filter((g) => g !== rg).map((g) => g.id);
    const saved = (store('numplay-order') || '').split(',').filter((id) => known.includes(id));
    const linked = p.has('games') ? p.get('games').split(',').filter((id) => known.includes(id)) : null;
    // the order: the link's picks first, in its order, then the rest as last time, then the kit's
    st.order = [...new Set([...(linked || []), ...saved, ...known])];
    st.picked = new Set(linked || kit.games.filter((g) => g.default).map((g) => g.id));
    st.cap = p.get('space') === 'xl' ? 'xl' : 'std';
    st.lang = LANGS[p.get('lang')] && kit.langs.includes(p.get('lang')) ? p.get('lang') : 'en';
    const icon = p.get('icon');
    st.app.icon = PRESETS[icon] ? icon : icon === 'custom' && custom ? 'custom' : 'NumPlay';
    st.app.name = p.has('name') ? p.get('name').slice(0, 20) : (PRESETS[st.app.icon] || [0, 'NumPlay'])[1];
    const open = (p.get('open') || '').split('-');
    st.app.open = open[0] === 'matrices' ? 'matrices' : 'numplay';
    st.app.secret = Math.min(SECRETS.length - 1, Math.max(0, parseInt(open[1], 10) || 0));
    st.app.hint = !open.includes('nohint');
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
  const objPath = (id) => (st.lang !== 'en' && kits[st.lang] && kits[st.lang].byId[id] ? st.lang + '/' : '') + 'games/' + id + '.o';
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

  async function build() {
    await loadKit(st.lang);
    const k = langKit();
    const sel = ordered();
    const roms = rg && st.roms.length;
    if (roms) sel.push(rg.id);
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
    // the launcher shows the games in the order of their entries: yours, not the kit's
    const entries = sel.map((id) => 'KEEP(*(.rodata.np_games.' + String((k.byId[id] || byId[id]).index).padStart(2, '0') + '))').join('\n    ');
    const head = k.script.head.replace('KEEP(*(SORT_BY_NAME(.rodata.np_games.*)))', entries);
    const block = (id) => k.script.blocks[id] || kit.script.blocks[id];
    files['numplay.ld'] = head + sel.map(block).join('') + k.script.tail;
    const l = await run(t.ld, args.concat(['-o', 'out.nwa']), files);
    if (l.code) throw new Error(l.log || 'ld ' + l.code);
    const s = await run(t.oc, ['--strip-unneeded', 'in.nwa', 'out.nwa'], { 'in.nwa': l.m.FS.readFile('out.nwa') });
    if (s.code) throw new Error(s.log || 'objcopy ' + s.code);
    const nwa = s.m.FS.readFile('out.nwa');
    const n = installedSize(nwa) + k.runtime;
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
    loadKit(st.lang).then(() => {
      const lang = st.lang === 'en' ? '' : st.lang + '/';
      for (const p of [lang + 'base.o', lang + 'arena.o', 'variants/' + (PRESETS[st.app.icon] ? st.app.icon : 'NumPlay') + '-icon.o']) fetchObj(p).catch(() => {});
      for (const id of st.picked) fetchObj(objPath(id)).catch(() => {});
      if (st.roms.length) fetchObj(objPath(rg.id)).catch(() => {});
    }).catch(() => {});
  }

  // ------------------------------------------------------------------ the app's own settings
  let popOpen = null;
  function openPop(pop, chip, side) {
    if (popOpen === pop) return closePops();
    closePops();
    pop.hidden = false;
    popOpen = pop;
    chip.setAttribute('aria-expanded', 'true');
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
    for (const c of document.querySelectorAll('.chip, .cap button')) c.setAttribute('aria-expanded', 'false');
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
      const b = el('button', '', { type: 'button', role: 'radio', 'aria-checked': st.app.icon === 'custom', 'aria-label': 'Your icon', 'data-tip': 'Your icon' });
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
        st.lang = l;
        closePops(); renderApp(); render();
      });
      langs.appendChild(b);
    }
  }

  // the 6 MB of Enlarged Extra Apps: what it is and how, before switching to it
  function askXL(btn) {
    let pop = $('#xl-pop');
    if (!pop) {
      pop = el('div', 'pop', { id: 'xl-pop', role: 'dialog', 'aria-label': 'Enlarged Extra Apps' });
      pop.innerHTML = '<b class="xl-title">6 MB with Enlarged Extra Apps</b>' +
        '<p class="xl-text">The calculator\'s installer gives apps 1.94 MB. nwagyu\'s <b>Nwagra</b> bookmark makes it use the rest of the flash, 6 MB in all, so several big games fit in one NumPlay.</p>' +
        '<ol class="xl-steps"><li>Add the Nwagra bookmark from nwagyu\'s guide</li><li>Update your calculator twice on my.numworks.com</li><li>On the installer page, click the bookmark, then drop NumPlay.nwa</li></ol>' +
        '<p class="xl-warn">It only works on older calculator software. On the newest, the calculator shows exam mode.</p>' +
        '<div class="xl-buttons"><a class="xl-guide" href="' + NWAGRA + '" target="_blank" rel="noopener">nwagyu\'s guide</a><button type="button" class="install xl-ok">Use 6 MB</button></div>';
      document.body.appendChild(pop);
      pop.querySelector('.xl-ok').addEventListener('click', () => { st.cap = 'xl'; store('numplay-xl-seen', '1'); closePops(); render(); });
    }
    openPop(pop, btn, true);
  }

  // ------------------------------------------------------------------ the intro
  // A few words while the calculator fills up, the first time and when the logo is clicked
  const intro = { on: false, run: 0 };
  function caption(text) {
    const c = $('#caption');
    c.classList.remove('show');
    setTimeout(() => { c.textContent = text; if (text) c.classList.add('show'); }, text ? 180 : 0);
  }
  async function playIntro() {
    if (intro.on) return;
    if (matchMedia('(prefers-reduced-motion: reduce)').matches) return;
    intro.on = true;
    const run = ++intro.run, alive = () => intro.run === run;
    closePops(); hidePreview();
    const target = ordered(), roms = st.roms;
    const body = document.body;
    const skip = el('button', 'intro-skip', { type: 'button' });
    skip.textContent = 'Skip';
    skip.addEventListener('click', endIntro);
    body.appendChild(skip);
    st.picked = new Set(); st.roms = [];
    body.classList.add('intro', 's0', 'start');
    render();
    intro.restore = () => { st.picked = new Set(target); st.roms = roms; };
    await wait(60);
    if (!alive()) return;
    body.classList.remove('start');
    caption('Free games for your NumWorks');
    await wait(1500);
    if (!alive()) return;
    body.classList.replace('s0', 's1');
    caption('Pick the ones you want');
    await wait(500);
    for (const id of target) {
      if (!alive()) return;
      st.picked.add(id);
      bump(buttons.get(id), 'bounce');
      render();
      await wait(target.length > 16 ? 45 : 80);
    }
    await wait(700);
    if (!alive()) return;
    body.classList.replace('s1', 's2');
    if (rg) {
      caption('Add Game Boy, NES and Sega games');
      await wait(1600);
      if (!alive()) return;
    }
    body.classList.replace('s2', 's3');
    caption('Install it in one click');
    await wait(1700);
    if (alive()) endIntro();
  }
  function endIntro() {
    if (!intro.on) return;
    intro.on = false;
    intro.run++;
    if (intro.restore) intro.restore();
    document.body.classList.remove('intro', 's0', 's1', 's2', 's3', 'start');
    const s = document.querySelector('.intro-skip');
    if (s) s.remove();
    caption('');
    store('numplay-intro', '1');
    render();
  }

  // ------------------------------------------------------------------ the logo's keys, GitHub
  function logoKeys() {
    const keys = [...document.querySelectorAll('.keys i')];
    let busy = false;
    const wave = () => {
      if (busy) return;
      busy = true;
      keys.forEach((k, i) => {
        setTimeout(() => k.classList.add('down'), 45 * i);
        setTimeout(() => k.classList.remove('down'), 45 * i + 130);
      });
      setTimeout(() => { busy = false; }, 45 * keys.length + 200);
    };
    $('#logo').addEventListener('pointerenter', wave);
    $('#logo').addEventListener('click', () => { wave(); if (intro.on) endIntro(); else playIntro(); });
  }
  async function stars() {
    try {
      let n = sessionStorage.getItem('numplay-stars');
      if (!n) {
        const r = await fetch('https://api.github.com/repos/Mason363/NumPlay');
        if (!r.ok) return;
        n = String((await r.json()).stargazers_count);
        sessionStorage.setItem('numplay-stars', n);
      }
      const v = +n, b = $('#stars');
      b.textContent = v >= 1000 ? (v / 1000).toFixed(v >= 10000 ? 0 : 1) + 'k' : String(v);
      b.hidden = false;
    } catch (e) { /* (offline, or GitHub said no: the button still works) */ }
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

  // ------------------------------------------------------------------ start
  async function start() {
    try {
      kit = await fetch(KIT + 'kit.json').then((r) => { if (!r.ok) throw new Error(r.status); return r.json(); });
    } catch (e) {
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
    const badge = '<span class="badge"><svg class="tick" viewBox="0 0 12 12"><path d="M2.5 6.2l2.3 2.3 4.7-5" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"/></svg>' +
      '<svg class="plus" viewBox="0 0 12 12"><path d="M6 2.5v7M2.5 6h7" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round"/></svg></span>';
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
      if (popOpen && !e.target.closest('.pop, .chip, .cap button')) closePops();
    });

    for (const b of document.querySelectorAll('.cap button'))
      b.addEventListener('click', () => {
        if (b.dataset.cap === 'std' && st.cap === 'xl') {
          const over = usedBytes() - kit.appSpace;
          if (over > 0) { bump(b, 'shake'); toast('Take ' + size(over) + ' of games out first'); return; }
          st.cap = 'std'; closePops(); render(); return;
        }
        if (b.dataset.cap === 'xl' && st.cap !== 'xl') askXL(b);
      });
    $('#install').addEventListener('click', (e) => go(e.currentTarget, true));
    $('#download').addEventListener('click', (e) => go(e.currentTarget, false));
    for (const b of [$('#install'), $('#download'), $('#app-chip'), $('#lang-chip')]) {
      b.addEventListener('pointerenter', prefetch, { once: true });
      b.addEventListener('focus', prefetch, { once: true });
    }
    $('#sheet').addEventListener('click', (e) => { if (e.target.id === 'sheet' || e.target.closest('.sheet-close')) closeSheet(); });
    document.addEventListener('keydown', (e) => {
      if (e.key === 'Escape') { closeSheet(); closePops(); if (holding) { holding.classList.remove('held'); holding = null; } }
      if (intro.on && e.key !== 'Tab') endIntro();
    });

    if (rg) {
      $('#retro').hidden = false;
      $('#retro-icon').src = KIT + 'icons/' + rg.id + '.png';
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
    tips();
    logoKeys();
    const first = (!store('numplay-intro') && !location.hash) || /[?&]intro\b/.test(location.search);  // (or ?intro, to see it)
    restore();
    if (st.lang !== 'en') loadKit(st.lang).then(render).catch(() => { st.lang = 'en'; render(); });
    render();
    addEventListener('hashchange', () => { restore(); render(); });
    stars();
    (window.requestIdleCallback || setTimeout)(() => loadTools().catch(() => {}), { timeout: 3000 });
    if (first) playIntro();
  }
  start();
})();
