// The port against the web game's own code. ORIGINAL is its js/game.js (kept with the reference, not here); without
// it this test is skipped. Only pure code runs, in a sandbox with nothing but the language: p2.js 0.7.1 (the physics
// engine bundled at the top of the file), CreateTerrainSegment, InputHandler.update and SpawnCoinsInAir.
//   ORIGINAL=.../js/game.js node tests/oracle.cjs build/cases.json
// From cases.json (written by `play test`):
//  - steps: the rider's physics steps, each played again by p2 from the same state (the ball, the dunes near it as
//    the game builds their Line shapes, the materials) and compared with where the port landed;
//  - terrain: CreateTerrainSegment with the same random numbers: the curve, the Line shapes, the coin points;
//  - input: InputHandler.update from the same states;  air: SpawnCoinsInAir with the same random numbers.
'use strict';
const fs = require('fs'), vm = require('vm');
const file = process.env.ORIGINAL;
if (!file || !fs.existsSync(file)) {
  console.log('oracle: skipped (set ORIGINAL to the web game\'s js/game.js)');
  process.exit(0);
}
const src = fs.readFileSync(file, 'utf8');
const cases = JSON.parse(fs.readFileSync(process.argv[2] || 'build/cases.json', 'utf8'));
let failed = 0;
const fail = (...a) => { failed++; console.log('FAIL', ...a); };
const sandbox = extra => {   // (the context has its own Array, Float32Array...; Math is a copy we can give a random to)
  const ctx = vm.createContext(extra);
  ctx.Math = vm.runInContext('Object.create(Math)', ctx);
  return ctx;
};

// ---------------------------------------------------------------- p2.js, from the bundle
const p2start = src.indexOf('!(function (t) {');
const p2end = src.indexOf('\n    }),\n        function () {\n            var t = this,');
if (p2start < 0 || p2end < 0) throw Error('p2 not found in ORIGINAL');
const p2ctx = sandbox({module: {exports: {}}});
p2ctx.exports = p2ctx.module.exports;
vm.runInContext(src.slice(p2start, p2end) + '\n    });', p2ctx);
const p2 = p2ctx.module.exports;
if (!p2.World || p2.version !== undefined && !String(p2.version).startsWith('0.7')) throw Error('not p2 0.7');

// Phaser's units: p2 metres are -0.05 of a point (pxmi), points -20 metres (mpxi)
const pxmi = v => -0.05 * v, mpxi = v => -20 * v, pxm = v => 0.05 * v;

let worst = {x: 0, v: 0, w: 0}, contacts = 0, sticky = 0, spun = 0;
for (const c of cases.steps) {
  const world = new p2.World({gravity: [0, 0], broadphase: new p2.SAPBroadphase()});
  world.gravity[1] = pxmi(1000);
  const ballMat = new p2.Material(), groundMat = new p2.Material();
  const cm = new p2.ContactMaterial(ballMat, groundMat, {});
  cm.stiffness = 1e11, cm.restitution = 0, cm.friction = 0;   // ApplyMaterials
  world.addContactMaterial(cm);
  const [x, y, vx, vy, w] = c.b;
  const ball = new p2.Body({mass: 1, position: [pxmi(x), pxmi(y)]});
  ball.addShape(new p2.Circle({radius: pxm(42)}));
  ball.shapes[0].material = ballMat;
  ball.fixedRotation = !c.turns;
  ball.updateMassProperties();
  ball.damping = 0, ball.angularDamping = 0;   // (the port's state is taken after the step's damping)
  ball.velocity[0] = pxmi(vx), ball.velocity[1] = pxmi(vy), ball.angularVelocity = w;
  world.addBody(ball);
  for (const s of c.segs) {
    // the game's terrain body: kinematic at (x, 392), Lines between every 8th point, + 0.1 long, three deep
    const body = new p2.Body({mass: 0, position: [pxmi(s.x), pxmi(392)]});
    body.type = p2.Body.KINEMATIC;
    const p = s.p;
    for (let k = 0; k < 8; k++) {
      const ax = p[2 * k], ay = p[2 * k + 1], bx = p[2 * k + 2], by = p[2 * k + 3];
      const len = Math.hypot(ax - bx, ay - by) + 0.1, a = Math.atan2(by - ay, bx - ax);
      for (const dy of [0, 18, 36]) {
        const line = new p2.Line({length: pxm(len)});
        body.addShape(line, [pxmi(ax + (bx - ax) / 2), pxmi(dy + ay + (by - ay) / 2)], a);
        if (!s.sticky) line.material = groundMat;   // (the segment set aside got no material: p2's default one)
      }
    }
    world.addBody(body);
    sticky += s.sticky;
  }
  world.step(1 / 60);
  contacts += world.narrowphase.contactEquations.length > 0;
  spun += c.a[4] !== c.b[4];
  const ex = Math.max(Math.abs(mpxi(ball.position[0]) - c.a[0]), Math.abs(mpxi(ball.position[1]) - c.a[1]));
  const ev = Math.max(Math.abs(mpxi(ball.velocity[0]) - c.a[2]), Math.abs(mpxi(ball.velocity[1]) - c.a[3]));
  const ew = Math.abs(ball.angularVelocity - c.a[4]);
  worst.x = Math.max(worst.x, ex), worst.v = Math.max(worst.v, ev), worst.w = Math.max(worst.w, ew);
  if (ex > 0.05 || ev > 1 || ew > 0.02) fail(`step: off by ${ex.toFixed(4)} points, ${ev.toFixed(3)} points/s, ${ew.toFixed(4)} rad/s`, JSON.stringify(c.b));
}
console.log(`oracle: ${cases.steps.length} physics steps (${contacts} with contacts, ${spun} spun by friction) played by p2: at most ${worst.x.toFixed(4)} points, ` +
            `${worst.v.toFixed(3)} points/s, ${worst.w.toFixed(4)} rad/s apart`);

// ---------------------------------------------------------------- CreateTerrainSegment
const terrainCode = src.slice(src.indexOf('function CreateTerrainSegment('), src.indexOf('function ReskinSegment('));
let tworst = 0;
for (const t of cases.terrain) {
  const queue = t.r.slice(), lines = [];
  const sprite = {loadTexture() {}, anchor: {setTo() {}}};
  const ctx = sandbox({
    TERRAIN_RES_FACTOR: 1, currentLevel: 0, DrawTerrainToBitmap() {}, groundCG: 0, playerCG: 0, SceneGame: {instance: {}},
    makeSprite: () => sprite,
    game: {add: {bitmapData: (w, h) => ({generateTexture(k, cb, self) { cb.call(self, {width: w, height: h}); }, destroy() {}})},
           rnd: {uuid: () => ''},
           physics: {p2: {enableBody(s) { s.body = {clearShapes() {}, setCollisionGroup() {}, collides() {}, addLine(len, cx, cy, a) { lines.push([len, cx, cy, a]); }}; }}}},
  });
  ctx.Math.random = () => queue.shift();
  vm.runInContext('Array.prototype.last = function (a) { return this[this.length - 1 - (a || 0)]; };' + terrainCode, ctx);
  const seg = ctx.CreateTerrainSegment(false, !!t.flat, {});
  const err = (a, b, what) => {
    const e = Math.abs(a - b) / Math.max(1, Math.abs(a));
    tworst = Math.max(tworst, e);
    if (e > 1e-4) fail(`terrain ${what}: ${a} vs ${b}`);
  };
  err(seg.width, t.w, 'width');
  for (let n = 0; n < 64; n++) err(seg.points[n].y, t.y[n], `curve ${n}`);
  for (let k = 0; k < 8; k++) {
    const [len, cx, cy, a] = lines[3 * k], hx = Math.cos(a) * len / 2, hy = Math.sin(a) * len / 2;
    const L = t.lines.slice(4 * k, 4 * k + 4);
    err(cx - hx, L[0], 'line'), err(cy - hy, L[1], 'line'), err(cx + hx, L[2], 'line'), err(cy + hy, L[3], 'line');
    if (Math.abs(lines[3 * k + 1][2] - cy - 18) > 1e-6 || Math.abs(lines[3 * k + 2][2] - cy - 36) > 1e-6) fail('the lines aren\'t three deep');
  }
  if (lines.length !== 24) fail(`terrain: ${lines.length} lines`);
  if (seg.coinPoints.length * 2 !== t.coins.length) fail(`coin points ${seg.coinPoints.length} vs ${t.coins.length / 2}`);
  else seg.coinPoints.forEach((p, i) => { err(p.x, t.coins[2 * i], 'coin x'); err(p.y, t.coins[2 * i + 1], 'coin y'); });
}
console.log(`oracle: ${cases.terrain.length} dunes from CreateTerrainSegment, within ${tworst.toExponential(1)} of the port`);

// ---------------------------------------------------------------- InputHandler.update
const inStart = src.indexOf('update: function () {', src.indexOf('var InputHandler ='));
const inputCode = '({' + src.slice(inStart, src.indexOf('        OnDown: function', inStart)).trim().replace(/,$/, '') + '})';
let iworst = 0;
for (const [vx, vy, ground, held, valid, ox, oy] of cases.input) {
  const ctx = sandbox({levelRunning: true, gameRunning: true, clickValid: !!valid, ballMoveInput: false,
                       cursors: {right: {justDown: false, isDown: !!held}}, game: {input: {activePointer: {isDown: false}}},
                       ball: {body: {velocity: {x: vx, y: vy}}, touchingGround: !!ground},
                       currentTerrain: {curveFunction: () => ({x: 0, y: 0})}, BallOnCurrentTerrainNormalized: () => 0.5});
  vm.runInContext(inputCode, ctx).update.call({});
  const e = Math.max(Math.abs(ctx.ball.body.velocity.x - ox), Math.abs(ctx.ball.body.velocity.y - oy));
  iworst = Math.max(iworst, e);
  if (e > 1e-3 * Math.max(1, Math.abs(ox))) fail(`input ${vx} ${vy}: ${ctx.ball.body.velocity.x} ${ctx.ball.body.velocity.y} vs ${ox} ${oy}`);
}
console.log(`oracle: ${cases.input.length} frames of InputHandler.update, within ${iworst.toFixed(4)}`);

// ---------------------------------------------------------------- SpawnCoinsInAir
const airCode = src.slice(src.indexOf('function SpawnCoinsInAir('), src.indexOf('function AddButtonEvents('));
let aworst = 0, shapes = 0;
for (const a of cases.air) {
  const queue = a.r.slice(), coins = [];
  const [x, y, vx, vy] = a.s;
  const ctx = sandbox({ball: {x, y, body: {velocity: {x: vx, y: vy}}}, line: {y: 200},
                       pools: {spawnCoin: () => { const c = {body: {}}; coins.push(c); return c; }}, clamp: (v, lo, hi) => v < lo ? lo : v > hi ? hi : v,
                       Phaser: {Point: function (px, py) { this.x = px; this.y = py; }}});
  ctx.Math.random = () => queue.shift();
  vm.runInContext(airCode + ';SpawnCoinsInAir();', ctx);
  if (coins.length * 2 !== a.c.length) { fail(`air coins: ${coins.length} vs ${a.c.length / 2}`); continue; }
  coins.forEach((c, i) => { aworst = Math.max(aworst, Math.abs(c.body.x - a.c[2 * i]), Math.abs(c.body.y - a.c[2 * i + 1])); });
  shapes += coins.length > 8;
}
if (aworst > 0.01) fail(`air coins off by ${aworst}`);
console.log(`oracle: ${cases.air.length} SpawnCoinsInAir (${shapes} with a shape of coins), within ${aworst.toFixed(4)}`);

console.log(failed ? `oracle: ${failed} FAILED` : 'oracle: all passed');
process.exit(failed ? 1 : 0);
