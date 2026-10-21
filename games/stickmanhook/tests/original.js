/* Stickman Hook's physics as they are in its web build's bundle.js (git blob 65da83d4): the vector, the bumpers,
 * the walls, and the stickman's step, hookTo, unhook and onHit. Only the glue lines (`var ta`, `function h`, `const
 * wall`, `function install`, `module.exports`) are not Madbox's. For tests/oracle.cjs; not in the calculator app. */
function $o(t, e) {
        this.x = t || 0, this.y = e || 0
    }
    Object.defineProperties($o.prototype, {
        width: {
            get: function() {
                return this.x
            },
            set: function(t) {
                this.x = t
            }
        },
        height: {
            get: function() {
                return this.y
            },
            set: function(t) {
                this.y = t
            }
        }
    }), Object.assign($o.prototype, {
        isVector2: !0,
        set: function(t, e) {
            return this.x = t, this.y = e, this
        },
        setScalar: function(t) {
            return this.x = t, this.y = t, this
        },
        setX: function(t) {
            return this.x = t, this
        },
        setY: function(t) {
            return this.y = t, this
        },
        setComponent: function(t, e) {
            switch (t) {
                case 0:
                    this.x = e;
                    break;
                case 1:
                    this.y = e;
                    break;
                default:
                    throw new Error("index is out of range: " + t)
            }
            return this
        },
        getComponent: function(t) {
            switch (t) {
                case 0:
                    return this.x;
                case 1:
                    return this.y;
                default:
                    throw new Error("index is out of range: " + t)
            }
        },
        clone: function() {
            return new this.constructor(this.x, this.y)
        },
        copy: function(t) {
            return this.x = t.x, this.y = t.y, this
        },
        add: function(t, e) {
            return void 0 !== e ? (console.warn("THREE.Vector2: .add() now only accepts one argument. Use .addVectors( a, b ) instead."), this.addVectors(t, e)) : (this.x += t.x, this.y += t.y, this)
        },
        addScalar: function(t) {
            return this.x += t, this.y += t, this
        },
        addVectors: function(t, e) {
            return this.x = t.x + e.x, this.y = t.y + e.y, this
        },
        addScaledVector: function(t, e) {
            return this.x += t.x * e, this.y += t.y * e, this
        },
        sub: function(t, e) {
            return void 0 !== e ? (console.warn("THREE.Vector2: .sub() now only accepts one argument. Use .subVectors( a, b ) instead."), this.subVectors(t, e)) : (this.x -= t.x, this.y -= t.y, this)
        },
        subScalar: function(t) {
            return this.x -= t, this.y -= t, this
        },
        subVectors: function(t, e) {
            return this.x = t.x - e.x, this.y = t.y - e.y, this
        },
        multiply: function(t) {
            return this.x *= t.x, this.y *= t.y, this
        },
        multiplyScalar: function(t) {
            return this.x *= t, this.y *= t, this
        },
        divide: function(t) {
            return this.x /= t.x, this.y /= t.y, this
        },
        divideScalar: function(t) {
            return this.multiplyScalar(1 / t)
        },
        applyMatrix3: function(t) {
            var e = this.x,
                n = this.y,
                r = t.elements;
            return this.x = r[0] * e + r[3] * n + r[6], this.y = r[1] * e + r[4] * n + r[7], this
        },
        min: function(t) {
            return this.x = Math.min(this.x, t.x), this.y = Math.min(this.y, t.y), this
        },
        max: function(t) {
            return this.x = Math.max(this.x, t.x), this.y = Math.max(this.y, t.y), this
        },
        clamp: function(t, e) {
            return this.x = Math.max(t.x, Math.min(e.x, this.x)), this.y = Math.max(t.y, Math.min(e.y, this.y)), this
        },
        clampScalar: function(t, e) {
            return this.x = Math.max(t, Math.min(e, this.x)), this.y = Math.max(t, Math.min(e, this.y)), this
        },
        clampLength: function(t, e) {
            var n = this.length();
            return this.divideScalar(n || 1).multiplyScalar(Math.max(t, Math.min(e, n)))
        },
        floor: function() {
            return this.x = Math.floor(this.x), this.y = Math.floor(this.y), this
        },
        ceil: function() {
            return this.x = Math.ceil(this.x), this.y = Math.ceil(this.y), this
        },
        round: function() {
            return this.x = Math.round(this.x), this.y = Math.round(this.y), this
        },
        roundToZero: function() {
            return this.x = this.x < 0 ? Math.ceil(this.x) : Math.floor(this.x), this.y = this.y < 0 ? Math.ceil(this.y) : Math.floor(this.y), this
        },
        negate: function() {
            return this.x = -this.x, this.y = -this.y, this
        },
        dot: function(t) {
            return this.x * t.x + this.y * t.y
        },
        cross: function(t) {
            return this.x * t.y - this.y * t.x
        },
        lengthSq: function() {
            return this.x * this.x + this.y * this.y
        },
        length: function() {
            return Math.sqrt(this.x * this.x + this.y * this.y)
        },
        manhattanLength: function() {
            return Math.abs(this.x) + Math.abs(this.y)
        },
        normalize: function() {
            return this.divideScalar(this.length() || 1)
        },
        angle: function() {
            var t = Math.atan2(this.y, this.x);
            return t < 0 && (t += 2 * Math.PI), t
        },
        distanceTo: function(t) {
            return Math.sqrt(this.distanceToSquared(t))
        },
        distanceToSquared: function(t) {
            var e = this.x - t.x,
                n = this.y - t.y;
            return e * e + n * n
        },
        manhattanDistanceTo: function(t) {
            return Math.abs(this.x - t.x) + Math.abs(this.y - t.y)
        },
        setLength: function(t) {
            return this.normalize().multiplyScalar(t)
        },
        lerp: function(t, e) {
            return this.x += (t.x - this.x) * e, this.y += (t.y - this.y) * e, this
        },
        lerpVectors: function(t, e, n) {
            return this.subVectors(e, t).multiplyScalar(n).add(t)
        },
        equals: function(t) {
            return t.x === this.x && t.y === this.y
        },
        fromArray: function(t, e) {
            return void 0 === e && (e = 0), this.x = t[e], this.y = t[e + 1], this
        },
        toArray: function(t, e) {
            return void 0 === t && (t = []), void 0 === e && (e = 0), t[e] = this.x, t[e + 1] = this.y, t
        },
        fromBufferAttribute: function(t, e, n) {
            return void 0 !== n && console.warn("THREE.Vector2: offset has been removed from .fromBufferAttribute()."), this.x = t.getX(e), this.y = t.getY(e), this
        },
        rotateAround: function(t, e) {
            var n = Math.cos(e),
                r = Math.sin(e),
                i = this.x - t.x,
                o = this.y - t.y;
            return this.x = i * n - o * r + t.x, this.y = i * r + o * n + t.y, this
        },
        normal: function() {
            var t = new $o(this.x, this.y);
            return t.rotateAround({
                x: 0,
                y: 0
            }, 90 * Math.PI / 180), t.normalize()
        }
    });

var ta=$o;function h(){return Object.assign;}
var ms = function(t, e, n) {
            if (0 === n) return !1;
            var r = e.x - t.x,
                i = e.y - t.y;
            return r * r + i * i <= n * n
        },
        gs = {
            x: 0,
            y: 0
        };
    var ws = function(t, e, n, r) {
        if (ms(t, n, r)) return t;
        if (ms(e, n, r)) return e;
        var i = t.x,
            o = t.y,
            a = e.x,
            s = e.y,
            A = n.x,
            c = n.y,
            u = a - i,
            l = s - o,
            d = u * u + l * l,
            p = u,
            f = l;
        if (d > 0) {
            var y = ((A - i) * u + (c - o) * l) / d;
            p *= y, f *= y
        }
        var x = gs;
        x.x = i + p, x.y = o + f;
        var m = p * p + f * f;
        return !!(ms(x, n, r) && m <= d && p * u + f * l >= 0) && h()({}, x)
    };
function vs(t, e) {
        var n = function(t, e) {
            if (!e.w) return !1;
            var n = {
                    width: e.w,
                    height: e.h
                },
                r = {
                    x: t.position.x - e.position.x,
                    y: t.position.y - e.position.y
                },
                i = r.x * r.x + r.y * r.y,
                o = t.radius + e.radius;
            if (i > o * o) return !1;
            var a = e.rotation,
                s = bs(r, a);
            return !(Math.abs(s.x) > n.width / 2 + t.radius || Math.abs(s.y) > n.height / 2 + t.radius) && s
        }(t, e);
        if (n) {
            t.onHit && t.onHit(e), e.onHit && e.onHit(t);
            var r = e.bounciness || fs;
            if (t.hook) {
                if (t.ignoreNextCollision) return;
                return t.ignoreNextCollision = !0, t.theta -= t.vtheta / 60, void(e.bounceForce ? t.vtheta = e.bounceForce * (t.vtheta > 0 ? -1 : 1) / t.r : (t.vtheta *= -r, t.vtheta * t.r > 3 && (t.vtheta = 3 / t.r)))
            }
            var i, o = e.rotation,
                a = bs(t.speed, o),
                s = bs(t.position, o),
                A = bs(e.position, o),
                h = e.w,
                c = e.h,
                u = Math.abs(n.y) - c / 2 > Math.abs(n.x) - h / 2,
                l = u ? "y" : "x";
            i = u ? n.y > 0 ? A.y + c / 2 + t.radius : A.y - c / 2 - t.radius : n.x > 0 ? A.x + h / 2 + t.radius : A.x - h / 2 - t.radius, e.bounceForce ? a[l] = e.bounceForce * (a[l] > 0 ? -1 : 1) : a[l] *= -1 * r;
            var d = a[l] > 0 ? 1 : -1;
            e.minSpeed && (a[l] = Math.max(Math.abs(a[l]), e.minSpeed) * d), e.maxSpeed && (a[l] = Math.min(Math.abs(a[l]), e.maxSpeed) * d), s[l] = 2 * i - s[l], t.speed.copy(bs(a, -o)), void 0 !== e.bounceAngle && t.speed.rotateAround({
                x: 0,
                y: 0
            }, -t.speed.angle() + e.bounceAngle);
            var p = bs(s, -o);
            t.position.x = p.x, t.position.y = p.y
        }
    }
function bs(t, e) {
        var n = new ta;
        return n.set(t.x * Math.cos(e) + t.y * Math.sin(e), -t.x * Math.sin(e) + t.y * Math.cos(e), t.z), n
    }
const wall=function(t, e) {
                                var n = t.position.x - e.position.x,
                                    r = t.position.y - e.position.y,
                                    i = n * n + r * r,
                                    o = t.radius + e.radius;
                                if (!(i > o * o)) {
                                    var a = e.shapes,
                                        s = null,
                                        A = null;
                                    if (a.some(function(e) {
                                            for (var n = 0; n < e.length; n++) {
                                                var r = e[n],
                                                    i = e[n < e.length - 1 ? n + 1 : 0];
                                                if (A = ws(r, i, t.position, t.radius)) return s = A === r || A === i ? new ta(-A.x + t.position.x, -A.y + t.position.y).normal() : new ta(i.x - r.x, i.y - r.y), A
                                            }
                                        })) {
                                        var h = !!t.hookPullRemaining;
                                        if (t.onHit(e), t.hook) t.ignoreNextCollision = !0, t.theta -= t.vtheta / 60, h ? t.r += .01 : t.vtheta *= -.1;
                                        else {
                                            var c = s.clone().normal().setLength(t.radius + 1e-4);
                                            t.position.x = A.x - c.x, t.position.y = A.y - c.y;
                                            var u = s.angle(),
                                                l = bs(t.speed, u),
                                                d = bs(s.normal(), u);
                                            l[Math.abs(d.y) > Math.abs(d.x) ? "y" : "x"] *= -.1, t.speed = bs(l, -u), t.angularVelocity = 20 * t.speed.x
                                        }
                                    }
                                }
                            };
function install(n,Sc){const ia=1.075,oa=1.2;n.step = function(t) {
                    if (n.hook) {
                        n.hookPullRemaining > 0 && (n.r -= .3 * Math.min(t, n.hookPullRemaining), n.r < .05 && (n.r = .05, n.hookPullRemaining = 0), n.hookPullRemaining -= t);
                        var e = -Sc.instance.gravity.y * Math.sin(n.theta) / n.r;
                        n.vtheta += e * t, n.theta += n.vtheta * t, n.position.set(n.r * Math.sin(n.theta) + n.hook.position.x, n.r * Math.cos(n.theta) + n.hook.position.y), n.rotation = -n.theta, n.rope.rotation = -n.theta + Math.PI / 2, n.rope.position.set(n.hook.position.x, n.hook.position.y), n.rope.visible = !0, n.rope.scale.x = n.r, n.vtheta > 0 ? n.scale.x = n.scale.y : (n.scale.x = -n.scale.y, n.shadow.x *= -1), Math.abs(n.vtheta) < .5 ? n.gotoFrame("hooked0") : n.theta < 0 && n.vtheta < 0 || n.theta > 0 && n.vtheta > 0 ? n.gotoFrame("hooked1") : n.gotoFrame("hooked2"), n.speed.set(n.r * n.vtheta * Math.cos(n.theta), -n.r * n.vtheta * Math.sin(n.theta), 0)
                    } else {
                        n.acceleration = n.speed.clone().multiplyScalar(-Sc.AIR_FRICTION), n.applyGravity && n.acceleration.add(Sc.instance.gravity), n.speed.add(n.acceleration.clone().multiplyScalar(t));
                        var r = n.speed.clone().multiplyScalar(t);
                        n.position.set(n.position.x + r.x, n.position.y + r.y), n.rotation += n.angularVelocity * t
                    }
                };
n.hookTo = function(t) {
                    var e = {
                        x: -1 * (t.position.x - n.position.x),
                        y: 1 * (t.position.y - n.position.y)
                    };
                    n.theta = Math.atan2(e.x, -e.y), n.r = Math.sqrt(e.x * e.x + e.y * e.y), n.vtheta = -(Math.cos(n.theta) * -n.speed.x + Math.sin(n.theta) * n.speed.y) / n.r, n.vtheta *= ia, n.hook = t, n.hookPullRemaining = .2, n.parent.addChildAt(n.rope, 1), n.gotoFrame("hooked2")
                };
n.unhook = function() {
                    n.hook && (n.speed.multiplyScalar(oa), n.angularVelocity = 20 * n.speed.x, n.parent.removeChild(n.rope), n.hook = null, n.hookPullRemaining = 0, n.gotoFrame("ball"), n.scale.x = n.scale.y)
                };
n.onHit = function(t) {
                    n.hookPullRemaining = 0, n.hook || (n.gotoFrame("ball"), n.angularVelocity = 20 * n.speed.x)
                };}
module.exports={Vec:ta,install,bumper:vs,wall};
