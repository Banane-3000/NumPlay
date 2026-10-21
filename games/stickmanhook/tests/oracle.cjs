// The physics against the web game's own code: ORIGINAL is a file with its functions as they are in the bundle
// (the vector, bumpers, walls and the stickman's step, hookTo, unhook and onHit), kept with the reference, not here.
// every step of the bot's replays (written by `play test` to cases.json) is played again from the same state
// by the original stickman, bumpers and walls, and must land where the port did.
//   ORIGINAL=.../original.js node tests/oracle.cjs build/cases.json
const fs = require('fs'), path = require('path');
const {Vec, install, bumper, wall} = require(path.resolve(process.env.ORIGINAL));
const outlines = JSON.parse(fs.readFileSync(path.join(__dirname, 'walls.json')));
const names = Object.keys(outlines);
const {parts, cases} = JSON.parse(fs.readFileSync(process.argv[2]));
const SCALE = 5e-4, keys = ['x', 'y', 'vx', 'vy', 'theta', 'omega', 'r', 'rotation', 'spin'];

function stickman(s) {
  const n = {position: new Vec(s.x, s.y), speed: new Vec(s.vx, s.vy), acceleration: new Vec(), radius: .017, r: s.r,
    theta: s.theta, vtheta: s.omega, hookPullRemaining: s.pull, rotation: s.rotation, angularVelocity: s.spin,
    applyGravity: !s.completed, hook: s.hooked ? {position: new Vec(s.hx, s.hy)} : null,
    rope: {position: new Vec(), scale: {}}, scale: {x: 1, y: 1}, shadow: {x: 0},
    parent: {addChildAt() {}, removeChild() {}}, gotoFrame() {}};
  install(n, {instance: {gravity: new Vec(0, 1)}, AIR_FRICTION: 0});
  return n;
}

// the course's objects, as the game's level scene makes them (`sc`, `ys`, `Ds.invalidateShapes`)
function course(s) {
  const hooks = [], bumpers = [], walls = [];
  for (const [part, offset] of s.sections) {
    const p = parts[part];
    for (const [x, y] of p.hooks) hooks.push({position: new Vec(x + offset, y)});
    for (const [x, y, rotation, bounce, force, angle, aimed] of p.bumpers)
      bumpers.push({position: new Vec(x + offset, y), rotation, w: .15, h: .04, radius: Math.sqrt(.15 * .15),
        bounciness: bounce, maxSpeed: 3, minSpeed: .1, bounceForce: force || undefined,
        bounceAngle: aimed ? angle : undefined});
    for (const [shape, x, y, rotation] of p.walls) {
      let radius = 0;
      const shapes = outlines[names[shape]].map(poly => {
        const pts = [];
        for (let i = 0; i < poly.length; i += 2) {
          const v = new Vec(poly[i] - 256, poly[i + 1] - 256).rotateAround({x: 0, y: 0}, rotation || 0);
          v.x *= SCALE, v.y *= SCALE;
          radius = Math.max(radius, Math.sqrt(v.x * v.x + v.y * v.y));
          pts.push({x: v.x + x + offset, y: v.y + y});
        }
        return pts;
      });
      walls.push({position: new Vec(x + offset, y), radius, shapes});
    }
  }
  return {hooks, bumpers, walls};
}

let worst = Object.fromEntries(keys.map(k => [k, 0])), walled = 0, bumped = 0, hooked = 0, finished = 0;
for (const c of cases) {
  const s = c.before, n = stickman(s), {hooks, bumpers, walls} = course(s);
  // input: a press hooks the closest hook (`findClosestHook`), a release lets go
  if (c.held && !s.held && !s.hooked && !s.completed) {
    let best = null, d = Infinity;
    for (const h of hooks) {
      const e = n.position.distanceToSquared(h.position);
      if (e < d) d = e, best = h;
    }
    n.hookTo(best);
    hooked++;
  }
  if (!c.held && s.held && !s.completed) n.unhook();
  n.ignoreNextCollision = !1;
  const t = (s.completed ? 1 / 60 * .2 : 1 / 60) / 4;
  let hitWall = false, hitBumper = false;
  n.onHit = (on => function(o) { (o.shapes ? (hitWall = true) : (hitBumper = true)); on(o); })(n.onHit);
  for (let i = 0; i < 4; i++) {
    n.step(t);
    bumpers.forEach(b => bumper(n, b));
    walls.forEach(o => wall(n, o));
  }
  walled += hitWall, bumped += hitBumper;
  // the finish (`onComplete`): let go, no gravity, no spin, turned along its way
  if (!s.completed && n.position.x > s.finish) {
    n.hook && n.unhook();
    n.applyGravity = !1, n.angularVelocity = 0, n.rotation = n.speed.angle() + Math.PI / 2;
    finished++;
  }
  const got = {x: n.position.x, y: n.position.y, vx: n.speed.x, vy: n.speed.y, theta: n.theta, omega: n.vtheta, r: n.r,
    rotation: n.rotation, spin: n.angularVelocity};
  const a = c.after;
  // the angle after the finish: the same direction, whatever the turn
  if (!s.completed && a.completed) got.rotation -= Math.round((got.rotation - a.rotation) / (2 * Math.PI)) * 2 * Math.PI;
  for (const k of keys) {
    const e = Math.abs(got[k] - a[k]);
    worst[k] = Math.max(worst[k], e);
    const allowed = ['vx', 'vy', 'omega', 'spin'].includes(k) ? 2e-3 + Math.abs(got[k]) * 1e-3 : 2e-4 + Math.abs(got[k]) * 1e-4;
    if (!(e <= allowed)) {
      console.error('FAIL', {key: k, port: a[k], original: got[k], before: s, held: c.held});
      process.exit(1);
    }
  }
  // the pull: running, over (0), or just under 0 as in the game (still "pulling" for the walls)
  const sign = v => v > 0 ? 1 : v < 0 ? -1 : 0;
  if (sign(n.hookPullRemaining) !== sign(a.pull) && Math.abs(n.hookPullRemaining - a.pull) > 2e-4) {
    console.error('FAIL', {key: 'pull', port: a.pull, original: n.hookPullRemaining, before: s, held: c.held});
    process.exit(1);
  }
}
console.log(`oracle: ${cases.length} steps as the web game's code plays them (${hooked} hooks, ${bumped} with a bumper, ` +
  `${walled} with a wall, ${finished} finishes); largest gaps: ` +
  keys.map(k => `${k} ${worst[k].toPrecision(2)}`).join(', '));
