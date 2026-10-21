#!/usr/bin/env node
// Runs the game's own main.js (and its English texts) with a pretend web page, then writes what it
// defines as JSON: the buildings, upgrades, achievements, tiers, the heavenly upgrades' tree, the
// golden cookie effects, the dragon's auras, the minigames' spells, gods and plants.
//
//   node tools/extract.js REF OUT.json      (REF: the folder tools/fetch.py filled)
'use strict';
const fs = require('fs');
const path = require('path');
const vm = require('vm');

const ref = process.argv[2], out = process.argv[3];
if (!ref || !out) { console.error('node tools/extract.js REF OUT.json'); process.exit(1); }

// a page where every element, style, canvas and event exists and does nothing
function any(name) {
  const f = function () { return any(name + '()'); };
  return new Proxy(f, {
    get(t, k) {
      if (k === Symbol.toPrimitive) return () => '';
      if (k === 'toString' || k === 'valueOf') return () => '';
      if (k === 'length') return 0;
      if (k === 'then') return undefined;
      if (k in t) return t[k];
      t[k] = any(name + '.' + String(k));
      return t[k];
    },
    set(t, k, v) { t[k] = v; return true; },
    apply() { return any(name + '()'); },
    construct() { return any('new ' + name); },
  });
}
const store = {};
const ctx = {
  console,  // (the built-ins are the context's own: an array the game makes must be its Array)
  setTimeout: () => 0, setInterval: () => 0, clearTimeout: () => 0, clearInterval: () => 0,
  requestAnimationFrame: () => 0,
  localStorage: { getItem: k => store[k] || null, setItem: (k, v) => { store[k] = String(v); }, removeItem: () => 0 },
  navigator: { userAgent: 'node', language: 'en' },
  location: { href: 'https://orteil.dashnet.org/cookieclicker/', pathname: '/cookieclicker/', hostname: 'x' },
  document: any('document'), Image: function () { return any('img'); }, Audio: function () { return any('audio'); },
  btoa: s => Buffer.from(s, 'binary').toString('base64'), atob: s => Buffer.from(s, 'base64').toString('binary'),
  URL, URLSearchParams, XMLHttpRequest: function () { return any('xhr'); }, performance: { now: () => 0 },
  App: 0, PRESETMODS: [], VERSION: 2.058, BETA: 0, SAVESUFFIX: "", DEV: 0, LOCAL: false, Steam: undefined, greenworks: undefined,
};
for (const k of ['Element', 'HTMLElement', 'HTMLCanvasElement', 'CanvasRenderingContext2D', 'Event', 'Node'])
  ctx[k] = function () {};
for (const k of ['addEventListener', 'removeEventListener', 'scrollTo', 'getComputedStyle', 'matchMedia', 'open',
  'alert', 'confirm', 'prompt', 'focus', 'blur', 'postMessage', 'dispatchEvent'])
  ctx[k] = () => any(k + '()');
Object.assign(ctx, { innerWidth: 1280, innerHeight: 720, devicePixelRatio: 1, screen: { width: 1280, height: 720 } });
ctx.window = ctx;
ctx.top = ctx;
ctx.self = ctx;
ctx.document.getElementById = () => any('el');
vm.createContext(ctx);
const run = (file) => vm.runInContext(fs.readFileSync(path.join(ref, file), 'utf8'), ctx, { filename: file });
run('main.js');
run('loc/EN.js');
vm.runInContext('locStringsFallback=locStrings;EN=true;Game.mods={};Game.sortedMods=[];Game.brokenMods=[];' +
  'Game.modSaveData={};Game.modHooks={};for (var i=0;i<Game.modHooksNames.length;i++)Game.modHooks[Game.modHooksNames[i]]=[];' +
  'Game.Launch();Game.Loader=new Loader();Game.Init();', ctx);
for (const m of ['Grimoire', 'Garden', 'Pantheon', 'Market']) run('minigame' + m + '.js');

const G = ctx.Game;
const fn = (f) => (typeof f === 'function' ? String(f) : undefined);
const num = (v) => (typeof v === 'number' ? v : undefined);
const vals = (o) => (Array.isArray(o) ? o : Object.values(o));
const data = {};
data.objects = vals(G.ObjectsById).map(o => ({
  id: o.id, name: o.name, single: o.single, plural: o.plural, actionName: o.actionName, extraName: o.extraName,
  extraPlural: o.extraPlural, desc: o.desc, basePrice: o.basePrice, baseCps: o.baseCps, icon: o.icon,
  iconColumn: o.iconColumn, art: o.art ? { base: o.art.base, xV: o.art.xV, yV: o.art.yV, w: o.art.w, rows: o.art.rows,
    x: o.art.x, y: o.art.y, pic: typeof o.art.pic === 'string' ? o.art.pic : fn(o.art.pic), bg: o.art.bg,
    frames: o.art.frames } : null,
  minigameUrl: o.minigameUrl, minigameName: o.minigameName, tieredUpgrades: Object.keys(o.tieredUpgrades || {}),
  synergies: (o.synergies || []).map(u => u.name), fortune: o.fortune ? o.fortune.name : null,
  grandma: o.grandma ? o.grandma.name : null, cps: fn(o.cps), buyFunction: fn(o.buyFunction),
}));
data.upgrades = vals(G.UpgradesById).map(u => ({
  id: u.id, name: u.name, dname: u.dname, desc: u.desc, ddesc: u.ddesc, basePrice: u.basePrice,
  priceFunc: fn(u.priceFunc), icon: u.icon, iconFunction: fn(u.iconFunction), pool: u.pool, order: u.order,
  tier: u.tier, buildingTie: u.buildingTie ? u.buildingTie.name : null, buildingTie1: u.buildingTie1 ? u.buildingTie1.name : null,
  buildingTie2: u.buildingTie2 ? u.buildingTie2.name : null, power: typeof u.power === 'function' ? fn(u.power) : u.power,
  kitten: u.kitten, unlockAt: u.unlockAt ? { cookies: u.unlockAt.cookies, require: u.unlockAt.require, season: u.unlockAt.season,
    name: u.unlockAt.name } : null, season: u.season, parents: (u.parents || []).map(p => p.name), posX: u.posX, posY: u.posY,
  showIf: fn(u.showIf), choicesFunction: fn(u.choicesFunction), buyFunction: fn(u.buyFunction), lasting: u.lasting,
  descFunc: fn(u.descFunc), displayFuncWhenOwned: fn(u.displayFuncWhenOwned), timerDisplay: fn(u.timerDisplay),
  activateFunction: fn(u.activateFunction), vanilla: u.vanilla, hide: u.hide, isVaulted: undefined,
  require: typeof u.require === 'string' ? u.require : (u.require && u.require.name) || undefined,
}));
data.achievements = vals(G.AchievementsById).map(a => ({
  id: a.id, name: a.name, dname: a.dname, desc: a.desc, ddesc: a.ddesc, icon: a.icon, pool: a.pool, order: a.order,
  tier: a.tier, buildingTie: a.buildingTie ? a.buildingTie.name : null, vanilla: a.vanilla,
}));
data.tiers = Object.fromEntries(Object.entries(G.Tiers).map(([k, t]) => [k, { name: t.name, unlock: t.unlock,
  achievUnlock: t.achievUnlock, iconRow: t.iconRow, color: t.color, price: t.price, special: t.special, req: t.req }]));
data.goldenChoices = fn(G.shimmerTypes.golden.popFunc);
data.buffTypes = Object.keys(G.buffTypesByName || {});
data.dragonLevels = (G.dragonLevels || []).map(l => ({ name: l.name, action: l.action, pic: l.pic, cost: fn(l.cost),
  buy: fn(l.buy), costStr: fn(l.costStr) }));
data.dragonAuras = Object.values(G.dragonAuras || {}).map(a => ({ name: a.name, desc: a.desc, pic: a.pic }));
data.milks = (G.Milks || []).map(m => ({ name: m.name, bname: m.bname, pic: m.pic, icon: m.icon }));
// what ties the tables together: by id
const uid = (u) => (u ? (typeof u === 'string' ? (G.Upgrades[u] ? G.Upgrades[u].id : -1) : u.id) : -1);
const aid = (a) => (a ? (typeof a === 'string' ? (G.Achievements[a] ? G.Achievements[a].id : -1) : a.id) : -1);
data.links = {
  objects: vals(G.ObjectsById).map(o => ({
    tieredUpgrades: Object.fromEntries(Object.entries(o.tieredUpgrades || {}).map(([t, u]) => [t, u.id])),
    tieredAchievs: Object.fromEntries(Object.entries(o.tieredAchievs || {}).map(([t, a]) => [t, a.id])),
    synergies: (o.synergies || []).map(u => u.id), fortune: uid(o.fortune), grandma: uid(o.grandma),
    unshackle: uid(o.unshackleUpgrade), level10: aid(o.levelAchiev10), production: (o.productionAchievs || []).map(p => [p.pow, p.achiev.id]),
  })),
  tierUnshackle: Object.fromEntries(Object.entries(G.Tiers).map(([k, t]) => [k, uid(t.unshackleUpgrade)])),
  cookieUpgrades: (G.cookieUpgrades || []).map(u => u.id),
  bank: (G.BankAchievements || []).map(a => [a.threshold, a.id]),
  cps: (G.CpsAchievements || []).map(a => [a.threshold, a.id]),
  unlockAt: (G.UnlockAt || []).map(u => ({ cookies: u.cookies, up: uid(u.name), ach: aid(u.name),
    requireUp: uid(u.require), requireAch: aid(u.require), season: u.season || null })),
  grandmaSynergies: (G.GrandmaSynergies || []).map(uid),
  kittens: ((G.UpgradesByPool || {}).kitten || []).map(u => u.id),
  goldenCookieUpgrades: (G.goldenCookieUpgrades || []).map(uid),
  lasting: vals(G.UpgradesById).filter(u => u.lasting).map(u => u.id),
  noPerm: vals(G.UpgradesById).filter(u => u.noPerm).map(u => u.id),
  priceLumps: vals(G.UpgradesById).filter(u => u.priceLumps).map(u => [u.id, u.priceLumps]),
  toggleInto: vals(G.UpgradesById).filter(u => u.toggleInto).map(u => [u.id, uid(u.toggleInto)]),
  showIf: vals(G.UpgradesById).filter(u => u.showIf).map(u => u.id),
  requiresConfirmation: vals(G.UpgradesById).filter(u => u.clickFunction).map(u => u.id),
  heavenly: (G.PrestigeUpgrades || []).map(u => u.id),
  easterEggs: (G.easterEggs || []).map(uid), halloweenDrops: (G.halloweenDrops || []).map(uid),
  heartDrops: (G.heartDrops || []).map(uid), santaDrops: (G.santaDrops || []).map(uid),
  reindeerDrops: (G.reindeerDrops || []).map(uid), seasons: Object.fromEntries(Object.entries(G.seasons || {}).map(
    ([k, s]) => [k, { name: s.name, start: s.start, over: s.over, trigger: uid(s.trigger) }])),
  fortuneTier: (G.Tiers.fortune.upgrades || []).map(u => u.id),
};
fs.writeFileSync(out, JSON.stringify(data, null, 1));
console.log(`extract: ${data.objects.length} buildings, ${data.upgrades.length} upgrades, ` +
  `${data.achievements.length} achievements, ${Object.keys(data.tiers).length} tiers, ${data.milks.length} milks`);
