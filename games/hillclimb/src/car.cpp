/* A vehicle and its world (car.h), as the game builds them: its PlayerCar classes' bodies, fixtures, joints and
 * driving, its terrain's edges, rope bridges and trees, on Box2D 2.2.1 like the game. The numbers come from the
 * game's own code (Android libgame.so; see the README's credits). */
#include <math.h>
#include <new>
#include <string.h>
#include <Box2D/Collision/Shapes/b2CircleShape.h>
#include <Box2D/Collision/Shapes/b2EdgeShape.h>
#include <Box2D/Collision/Shapes/b2PolygonShape.h>
#include <Box2D/Collision/b2Collision.h>
#include <Box2D/Dynamics/Contacts/b2Contact.h>
#include <Box2D/Dynamics/Joints/b2DistanceJoint.h>
#include <Box2D/Dynamics/Joints/b2RevoluteJoint.h>
#include <Box2D/Dynamics/Joints/b2WheelJoint.h>
#include <Box2D/Dynamics/b2Body.h>
#include <Box2D/Dynamics/b2Fixture.h>
#include <Box2D/Dynamics/b2World.h>
#include "car.h"
#include "stage.h"
#include "vehicle.h"

#ifndef HOST
/* (no C++ runtime on the calculator: the few things Box2D's classes refer to; nothing is deleted) */
void operator delete(void *) noexcept {}
void operator delete(void *, unsigned int) noexcept {}
extern "C" void __cxa_pure_virtual(void) {
  for (;;) {
  }
}
#endif

namespace {

const float DT = 1.0f / CAR_HZ;

/* the game's object types (PhysicsObject::Type): what a body is, as the contacts tell */
enum { T_COIN = 2, T_FUEL = 4, T_CAR = 8, T_DRIVER = 32, T_GROUND = 64, T_BRIDGE = 128, T_TREE = 256, T_ICE = 512 };
/* collision filters: category, mask */
const uint16 CAR_MASK = 0x7C6, DRIVER_MASK = 0x1C6, GROUND_MASK = 296, BRIDGE_MASK = 40, TREE_MASK = 120;
const uint16 NONLETHAL = 0x300;   /* touching these with the head is fine (trees, ice) */

/* ------------------------------------------------------------------ the vehicles' numbers (PlayerCar*) */
struct Spec {
  float hz, damp0, damp_k;       /* suspension: spring frequency; damping ratio damp0 x damp_k^L */
  int damp_cap;                  /* the damping's level counts up to this (0: no cap) */
  float speed0, speed_k, speed_m;   /* top wheel speed speed0 x speed_k^L x speed_m (rad/s) */
  float turbo0, turbo_k;         /* the turbo's cap turbo0 x turbo_k^L */
  float rate0;                   /* the turbo's build rate rate0 x 1.1^(L-1) */
  float torque;                  /* the engine's torque (N m) */
  float tire0, tire_k;           /* the tires' friction tire0 x tire_k^L */
  float awd;                     /* the front wheel's share of the torque with no 4WD upgrade */
  float wy, r, rr, hwb;          /* wheels: below the spawn point (-), radius, rear radius, half wheelbase */
  float cdens, cfric, wdens;     /* chassis density and friction, wheels' density */
  float com_k;                   /* the centre of mass lowered by com_k a suspension level */
  float front_fric;              /* the front tyre's friction share */
  bool fork;                     /* a raked front fork (bikes) */
  float seat[2];                 /* Bill's seat from the spawn point */
  float neck_limit, driver_fric; /* his joints' bend, his friction */
  float air;                     /* the air control's torque */
  int nv;
  float poly[8][2];              /* the chassis */
  float down0, down_k;           /* downforce coefficient down0 + down_k x L (0, 0: none) */
};

/* indexes match vehicle.h (V_JEEP...) */
const Spec SPECS[NVEHICLES] = {
  /* Hill Climber */
  {3, 0.2f, 1.11f, 14, 28, 1.05f, 1, 0.5f, 1.15f, 1.0f, 10, 0.5f, 1.115f, 0, 0, 0.4f, 0.4f, 1.15f, 1, 0.2f, 0.5f, 0.045f,
   1, false, {-0.1f, 0.8f}, 0.3f, 0.5f, 20, 6, {{-1, -0.5f}, {1, -0.5f}, {1.5f, 0}, {0, 0.5f}, {-1.15f, 0.5f}, {-1.5f, 0.2f}},
   0, 0},
  /* Motocross Bike */
  {3.5f, 0.2f, 1.11f, 14, 28, 1.05f, 1, 0.5f, 1.15f, 2.0f, 15, 1.85f, 1.115f, 0, -0.2f, 0.5f, 0.5f, 1.25f, 1, 1.5f, 0.6f,
   0.045f, 0.15f, true, {-0.3f, 1.35f}, 0.4f, 0.75f, 20, 6,
   {{-1.3f, 0.2f}, {-1.3f, -0.1f}, {-0.5f, -0.4f}, {0.5f, -0.4f}, {1.45f, -0.1f}, {1.45f, 0.2f}}, 0, 0},
  /* Monster Truck */
  {3, 0.2f, 1.11f, 0, 28, 1.05f, 0.55f, 0.5f, 1.15f, 2.5f, 20, 0.65f, 1.115f, 0, 0, 0.7f, 0.7f, 1.45f, 0.5f, 0.2f, 0.2f,
   0.05f, 1, false, {-0.1f, 0.8f}, 0.3f, 0.5f, 20, 6,
   {{-1.275f, -0.5f}, {1.275f, -0.5f}, {1.7f, 0}, {0, 0.5f}, {-1.3175f, 0.5f}, {-1.7f, 0.2f}}, 0, 0},
  /* Quad Bike */
  {3.25f, 0.2f, 1.11f, 14, 25, 1.05f, 1, 0.5f, 1.15f, 2.0f, 15, 1.85f, 1.115f, 0, -0.2f, 0.6f, 0.6f, 1.25f, 1, 1.5f, 0.3f,
   0.045f, 0.15f, true, {-0.3f, 1.35f}, 0.4f, 0.75f, 20, 6,
   {{-1.4f, 0.3f}, {-1.4f, -0.1f}, {-0.5f, -0.4f}, {0.5f, -0.4f}, {1.55f, -0.1f}, {1.55f, 0.3f}}, 0, 0},
  /* Race Car */
  {5.5f, 0.2f, 1.11f, 0, 28, 1.05f, 1, 0.5f, 1.15f, 2.5f, 20, 1.25f, 1.115f, 0, -0.4f, 0.5f, 0.625f, 1.45f, 1, 0.2f, 0.2f,
   0.045f, 1, false, {0.15f, 0.65f}, 0.3f, 0.5f, 20, 6,
   {{-1.875f, -0.5f}, {1.6875f, -0.4f}, {1.6875f, 0}, {0, 0.4f}, {-1.9375f, 0.55f}, {-2, 0.55f}}, 0.25f, 0.4f},
  /* Rally Car */
  {5, 0.2f, 1.11f, 0, 33, 1.05f, 1, 0.5f, 1.15f, 2.5f, 20, 1.25f, 1.115f, 0.5f, -0.25f, 0.5f, 0.5f, 1.3f, 1, 0.2f, 0.2f,
   0.045f, 1, false, {-0.25f, 0.9f}, 0.3f, 0.5f, 30, 6,
   {{-1.875f, -0.5f}, {1.6875f, -0.4f}, {1.6875f, 0}, {0, 0.4f}, {-1.9375f, 0.55f}, {-2, 0.55f}}, 0.25f, 0.3f},
  /* Dune Buggy */
  {4.5f, 0.2f, 1.11f, 0, 21, 1.05f, 1, 0.5f, 1.15f, 2.5f, 22, 1.35f, 1.115f, 0.5f, -0.35f, 0.5f, 0.625f, 1.6f, 1, 0.2f, 0.2f,
   0.045f, 1, false, {-0.15f, 0.85f}, 0.3f, 0.5f, 20, 6,
   {{-1.875f, -0.5f}, {1.6875f, -0.4f}, {1.6875f, 0}, {0, 0.4f}, {-1.9375f, 0.55f}, {-2, 0.55f}}, 0.25f, 0},
  /* Tank */
  {3, 0.3f, 1.0875f, 0, 34, 1.048f, 0.55f, 0.87f, 1.14f, 3.33f, 6, 0.5f, 1.1612f, 0.5f, 0, 0.35f, 0.35f, 1.45f, 0.5f, 0.2f,
   0.2f, 0.0707142857f, 1, false, {-0.2f, 1.4f}, 0.3f, 0.5f, 3.5f, 6,
   {{-1.275f, -0.4f}, {1.275f, -0.4f}, {2.04f, 0.5f}, {0, 1}, {-2.1675f, 0.5f}, {-2.04f, 0.2f}}, 0, 0},
};
/* the Tank's six wheels, rear to front: place from the spawn point, the joint's axis (not normalised, as in the
 * game), spring frequency (x the suspension's for 1 and 1.5), damping ratio */
const float TANK_WHEELS[6][6] = {
  {-2.2f, 1.0f, 0.8f, 0.2f, 10.1f, 1}, {-1.5f, 0.1f, 0, 1, -1, 1}, {-0.5f, -1.5e-17f, 0, 1, -1.5f, 1},
  {0.5f, -1.5e-17f, 0, 1, -1.5f, 1}, {1.5f, 0.1f, 0, 1, -1, 1}, {2.2f, 1.0f, -0.8f, 0.2f, 10.1f, 1},
};

float powi(float b, int n) {
  float r = 1;
  for (int i = 0; i < n; i++) r *= b;
  return r;
}
float clampf(float v, float lo, float hi) { return v < lo ? lo : v > hi ? hi : v; }

/* ------------------------------------------------------------------ the world */
alignas(8) unsigned char world_mem[sizeof(b2World)];
b2World *W;
b2Body *ground;

/* what a body is: its type, and which part of the car */
struct Tag {
  int type;
  int part;   /* 0 chassis, 1.. wheels (index + 1), -1 torso, -2 head, -3 none */
};
Tag tag_ground = {T_GROUND, -3}, tag_bridge = {T_BRIDGE, -3}, tag_tree = {T_TREE, -3};
Tag tag_car[CAR_MAXW + 1], tag_torso = {T_CAR, -1}, tag_head = {T_CAR, -2};

/* ------------------------------------------------------------------ the vehicle (PlayerCar) */
struct Vehicle {
  const Spec *s;
  int veh, nw;
  b2Body *chassis, *wheel[CAR_MAXW], *torso, *head;
  b2WheelJoint *joint[CAR_MAXW];
  b2RevoluteJoint *neck;          /* torso to head */
  b2DistanceJoint *spring[2];     /* chassis to torso, torso to head */
  float wr[CAR_MAXW], share[CAR_MAXW];
  float hz, damp, speed, turbo_cap, rate, tires, awd, air, down;
  float turbo, rev;
  int air_frames;
  unsigned touch_chassis, touch_wheel[CAR_MAXW];
  bool decel_air;
  bool dead, head_hit;
  int flips_k;                    /* the turn handleFlips counts from */
  bool alive_at_turn;
  bool accelerating;
} V;

/* the level of the vehicle's upgrade of a kind, or -1 */
int level_of(int veh, const uint8_t lv[4], int kind) {
  for (int u = 0; u < 4; u++)
    if (vehicles[veh].up[u] == kind) return lv[u];
  return -1;
}

b2Body *make_body(float x, float y, float angle, bool bullet, Tag *tag) {
  b2BodyDef bd;
  bd.type = b2_dynamicBody;
  bd.position.Set(x, y);
  bd.angle = angle;
  bd.bullet = bullet;
  bd.userData = tag;
  return W->CreateBody(&bd);
}

void add_fixture(b2Body *b, const b2Shape *sh, float friction, float density, uint16 cat, uint16 mask, int16 group) {
  b2FixtureDef fd;
  fd.shape = sh;
  fd.friction = friction;
  fd.density = density;
  fd.restitution = 0;
  fd.filter.categoryBits = cat;
  fd.filter.maskBits = mask;
  fd.filter.groupIndex = group;
  b->CreateFixture(&fd);
}

/* PlayerCar::createDriver: a torso and a head, each on a revolute joint and a spring */
void create_driver(float x, float y) {
  const Spec *s = V.s;
  b2Body *prev = V.chassis;
  for (int i = 0; i < 2; i++) {
    b2PolygonShape box;
    box.SetAsBox(0.3f, 0.3f);
    b2CircleShape circle;
    circle.m_radius = 0.33f;
    b2Body *b = make_body(x, y + 0.3f + 0.6f * (float)i, 3.14159f, false, i ? &tag_head : &tag_torso);
    add_fixture(b, i ? (b2Shape *)&circle : (b2Shape *)&box, s->driver_fric, 0.01f, T_DRIVER, DRIVER_MASK, 0);
    b2RevoluteJointDef rj;
    rj.bodyA = prev;
    rj.bodyB = b;
    rj.localAnchorA = i ? b2Vec2(0, -0.3f) : prev->GetLocalPoint(b2Vec2(x, y));
    rj.localAnchorB.Set(-0.0f, 0.3f);
    rj.referenceAngle = i ? 0 : 3.14159265f;
    rj.enableLimit = true;
    rj.lowerAngle = -s->neck_limit;
    rj.upperAngle = s->neck_limit;
    b2RevoluteJoint *rev = (b2RevoluteJoint *)W->CreateJoint(&rj);
    if (i) V.neck = rev;
    /* a zero-length spring beside the joint keeps him upright */
    b2DistanceJointDef dj;
    dj.bodyA = prev;
    dj.bodyB = b;
    b2Vec2 side = b->GetWorldPoint(b2Vec2(-0.15f, 0.3f));
    dj.localAnchorA = i ? b2Vec2(-0.15f, -0.3f) : prev->GetLocalPoint(side);
    dj.localAnchorB.Set(-0.15f, 0.3f);
    dj.length = (prev->GetWorldPoint(dj.localAnchorA) - b->GetWorldPoint(dj.localAnchorB)).Length();
    float k = 1 + 0.25f * (float)i;
    dj.frequencyHz = 10.1f / k;
    dj.dampingRatio = 10.0f / k;
    V.spring[i] = (b2DistanceJoint *)W->CreateJoint(&dj);
    if (i) V.head = b;
    else V.torso = b;
    prev = b;
  }
}

b2WheelJoint *wheel_joint(b2Body *wheel, b2Vec2 axis, float hz, float damp) {
  b2WheelJointDef jd;
  jd.Initialize(V.chassis, wheel, wheel->GetPosition(), axis);
  jd.enableMotor = true;
  jd.maxMotorTorque = 500;
  jd.motorSpeed = 0;
  jd.frequencyHz = hz;
  jd.dampingRatio = damp;
  return (b2WheelJoint *)W->CreateJoint(&jd);
}

/* PlayerCar*::PlayerCar and ::create: at x (the ground's height there + 3) */
void create_vehicle(int veh, const uint8_t lv[4], float x, float y) {
  const Spec *s = &SPECS[veh];
  memset(&V, 0, sizeof V);
  V.s = s, V.veh = veh;
  int le = level_of(veh, lv, UP_ENGINE), ls = level_of(veh, lv, UP_SUSPENSION), lt = level_of(veh, lv, UP_TIRES);
  if (lt < 0) lt = level_of(veh, lv, UP_TRACKS);
  int la = level_of(veh, lv, UP_AWD), lair = level_of(veh, lv, UP_AIR), ld = level_of(veh, lv, UP_DOWNFORCE);
  if (le < 1) le = 1;
  if (ls < 1) ls = 1;
  if (lt < 1) lt = 1;
  V.hz = s->hz;
  V.damp = s->damp0 * powi(s->damp_k, s->damp_cap && ls > s->damp_cap ? s->damp_cap : ls);
  V.speed = s->speed0 * powi(s->speed_k, le) * s->speed_m;
  V.turbo_cap = s->turbo0 * powi(s->turbo_k, le);
  V.rate = s->rate0 * powi(1.1f, le - 1);
  V.tires = s->tire0 * powi(s->tire_k, lt);
  V.awd = la > 0 ? (float)(la - 1) / 18.0f : s->awd;
  V.air = s->air;
  if (veh == V_BIKE) V.air = 0.8f * powi(1.15f, (lair > 0 ? lair : 1) - 1);
  if (veh == V_QUAD) V.air = 0.8f * powi(1.15f, 4);   /* (the Quad's own, fixed) */
  V.down = s->down0 + s->down_k * (float)(ld > 0 ? ld : 0);
  /* the chassis */
  b2PolygonShape poly;
  b2Vec2 vs[8];
  for (int i = 0; i < s->nv; i++) vs[i].Set(s->poly[i][0], s->poly[i][1]);
  poly.Set(vs, s->nv);
  tag_car[0] = {T_CAR, 0};
  V.chassis = make_body(x, y + 1, 0, true, &tag_car[0]);
  add_fixture(V.chassis, &poly, s->cfric, s->cdens, T_CAR, CAR_MASK, veh == V_TANK ? 1 : 0);
  /* the suspension lowers the centre of mass */
  b2MassData md;
  V.chassis->GetMassData(&md);
  md.center.y -= (float)(ls - 1) * s->com_k;
  V.chassis->SetMassData(&md);
  /* the wheels */
  if (veh == V_TANK) {
    V.nw = 6;
    for (int i = 0; i < 6; i++) {
      const float *t = TANK_WHEELS[i];
      b2CircleShape c;
      c.m_radius = V.wr[i] = s->r;
      tag_car[i + 1] = {T_CAR, i + 1};
      V.wheel[i] = make_body(x + t[0], y + t[1], 0, true, &tag_car[i + 1]);
      add_fixture(V.wheel[i], &c, V.tires - (float)(i + 1) / 6.0f * (V.tires * 0.25f), s->wdens, T_CAR, CAR_MASK, -1);
      float hz = t[4] > 0 ? t[4] : -t[4] * V.hz, damp = t[4] > 0 ? t[5] : V.damp;
      V.joint[i] = wheel_joint(V.wheel[i], b2Vec2(t[2], t[3]), hz, damp);
    }
  } else {
    V.nw = 2;
    for (int i = 0; i < 2; i++) {
      b2CircleShape c;
      c.m_radius = V.wr[i] = i ? s->r : s->rr;
      tag_car[i + 1] = {T_CAR, i + 1};
      V.wheel[i] = make_body(x + (i ? s->hwb : -s->hwb), y - s->wy, 0, true, &tag_car[i + 1]);
      add_fixture(V.wheel[i], &c, i ? V.tires * s->front_fric : V.tires, s->wdens, T_CAR, CAR_MASK, 0);
      b2Vec2 axis = i && s->fork ? b2Vec2(-0.410365f, 0.911922f) : b2Vec2(0, 1);
      V.joint[i] = wheel_joint(V.wheel[i], axis, V.hz, V.damp);
    }
    V.share[0] = 1 - V.awd, V.share[1] = V.awd;
  }
  create_driver(x + s->seat[0], y + s->seat[1]);
  V.alive_at_turn = true;
}

/* PlayerCar::reportTouch: what touched which part */
void report_touch(int type, const Tag *part) {
  if (part->part == 0) V.touch_chassis |= (unsigned)type;
  else if (part->part > 0) V.touch_wheel[part->part - 1] |= (unsigned)type;
  else if (part->part == -2 && !V.dead && !(type & NONLETHAL)) V.head_hit = true;
}

struct Listener : b2ContactListener {
  void PreSolve(b2Contact *contact, const b2Manifold *old) override {
    (void)old;
    Tag *a = (Tag *)contact->GetFixtureA()->GetBody()->GetUserData();
    Tag *b = (Tag *)contact->GetFixtureB()->GetBody()->GetUserData();
    if (!a || !b) return;
    if (a->type == T_CAR && b->type != T_CAR) report_touch(b->type, a);
    else if (b->type == T_CAR && a->type != T_CAR) report_touch(a->type, b);
  }
} listener;

/* ------------------------------------------------------------------ the ground: 0.5 m edges, as the game's Terrain */
const float SEG = 0.5f, X0 = -40.0f;
const int RING = 64;   /* the edges kept around the car (the game keeps 200; only these near it can be touched) */
struct Ring {
  b2Fixture *f[RING];
  int k0, k1;          /* edges k0..k1-1 exist; edge k from vertex k to k + 1 */
  bool ceiling;
};
Ring floor_ring, ceil_ring;
int game_end;          /* the game's last vertex: 40 m ahead of the farthest the car got */
b2Fixture *wall;
float wall_x;

float vertex_y(int k, bool ceiling) {
  /* the game's Terrain::addSegment: vertex k at X0 + 0.5 k has the height of the x before it */
  return ceiling ? stage_roof_vertex(k) : stage_vertex(k);
}

b2Fixture *edge(float xa, float ya, float xb, float yb) {
  b2EdgeShape e;
  e.Set(b2Vec2(xa, ya), b2Vec2(xb, yb));
  b2FixtureDef fd;
  fd.shape = &e;
  fd.friction = stage->friction;
  fd.restitution = 0;
  fd.density = 0;
  fd.filter.categoryBits = T_GROUND;
  fd.filter.maskBits = GROUND_MASK;
  return ground->CreateFixture(&fd);
}

void ring_set(Ring *r, int lo, int hi) {
  if (hi - lo > RING) lo = hi - RING;
  /* drop what left the window */
  while (r->k0 < r->k1 && r->k0 < lo) ground->DestroyFixture(r->f[r->k0++ % RING]);
  while (r->k1 > r->k0 && r->k1 > hi) ground->DestroyFixture(r->f[--r->k1 % RING]);
  if (r->k0 >= r->k1) r->k0 = r->k1 = lo;
  /* and add what entered it, on both sides */
  while (r->k0 > lo) {
    int k = --r->k0;
    r->f[k % RING] = edge(X0 + SEG * (float)k, vertex_y(k, r->ceiling), X0 + SEG * (float)(k + 1),
                          vertex_y(k + 1, r->ceiling));
  }
  while (r->k1 < hi) {
    int k = r->k1++;
    r->f[k % RING] = edge(X0 + SEG * (float)k, vertex_y(k, r->ceiling), X0 + SEG * (float)(k + 1),
                          vertex_y(k + 1, r->ceiling));
  }
}

/* the ground's edges around the car; the game's wall behind (Terrain::updateBackWall: 20 edges into its ring of 200) */
void update_ground(float cx) {
  int want = (int)ceilf((cx + 40 - X0) / SEG);
  if (want > game_end) game_end = want;
  int start = game_end - 200 < 0 ? 0 : game_end - 200;
  int wk = start + 20;
  float wx = X0 + SEG * (float)wk;
  if (game_end - start >= 21 && wx != wall_x) {
    if (wall) ground->DestroyFixture(wall);
    float wy = vertex_y(wk, false);
    wall = edge(wx, wy - 100, wx, wy + 1000);
    wall_x = wx;
  }
  /* (wider in the Forest: its fallen trees roll on ahead of the car) */
  float back = stage->feature == FEAT_TREES ? 12 : 8, ahead = stage->feature == FEAT_TREES ? 18 : 10;
  int lo = (int)floorf((cx - back - X0) / SEG), hi = (int)ceilf((cx + ahead - X0) / SEG);
  if (lo < start) lo = start;
  if (hi > game_end) hi = game_end;
  ring_set(&floor_ring, lo, hi);
  if (stage->ceiling) ring_set(&ceil_ring, lo, hi);
}

/* ------------------------------------------------------------------ rope bridges (Bridge::create) */
const int MAXPLANK = 20;
struct Bridge {
  int k;               /* which bridge, or -1 */
  int n;
  b2Body *plank[MAXPLANK];
} bridges[2];

void bridge_free(Bridge *br) {
  for (int i = 0; i < br->n; i++) W->DestroyBody(br->plank[i]);   /* (its joints go with it) */
  br->n = 0, br->k = -1;
}

void bridge_make(Bridge *br, int k) {
  float x0, len, y0;
  if (!stage_bridge_k(k, &x0, &len, &y0)) return;
  int n = (int)len;
  if (n > MAXPLANK) n = MAXPLANK;
  float x1 = x0 + len, t = 0.3f;
  float w = (x1 - x0) / (float)(n - 1), dx = (x1 - x0) / (float)n;
  b2PolygonShape box;
  box.SetAsBox(w * 0.5f, t * 0.5f);
  b2Body *prev = ground;
  float cx = x0 + w, cy = y0 - t * 0.5f;
  br->k = k, br->n = n;
  for (int i = 0; i < n; i++) {
    b2BodyDef bd;
    bd.type = b2_dynamicBody;
    bd.position.Set(cx, cy);
    bd.userData = &tag_bridge;
    b2Body *p = W->CreateBody(&bd);
    b2FixtureDef fd;
    fd.shape = &box;
    fd.friction = 0.5f;
    fd.density = 1;
    fd.filter.categoryBits = T_BRIDGE;
    fd.filter.maskBits = BRIDGE_MASK;
    p->CreateFixture(&fd);
    b2RevoluteJointDef rj;
    rj.Initialize(prev, p, b2Vec2(cx - w * 0.5f, cy + t * 0.5f));
    W->CreateJoint(&rj);
    br->plank[i] = prev = p;
    cx += dx;
  }
  b2RevoluteJointDef rj;
  rj.Initialize(prev, ground, b2Vec2(cx - w * 0.5f, cy + t * 0.5f));
  W->CreateJoint(&rj);
}

/* the bridges near the car exist, the others not */
void update_bridges(float cx) {
  if (!stage->bridges) return;
  int near = -1;
  for (int k = stage_bridge_first(cx - 40);; k++) {
    float x0, len, y0;
    if (!stage_bridge_k(k, &x0, &len, &y0) || x0 > cx + 45) break;
    if (x0 + len < cx - 30) continue;
    near = k;
    break;
  }
  for (int i = 0; i < 2; i++)
    if (bridges[i].k >= 0 && bridges[i].k != near) bridge_free(&bridges[i]);
  if (near < 0) return;
  for (int i = 0; i < 2; i++)
    if (bridges[i].k == near) return;
  for (int i = 0; i < 2; i++)
    if (bridges[i].k < 0) {
      bridge_make(&bridges[i], near);
      return;
    }
}

/* ------------------------------------------------------------------ trees (the Forest's: Tree::create) */
const int MAXTREE = 10;
struct TreeBody {
  int k;               /* the tree's index, or -1 */
  b2Body *b;
  b2RevoluteJoint *j;
  float t;             /* seconds since it broke */
  float held;          /* seconds pushed against its stop */
} trees[MAXTREE];
int tree_broken;

void tree_free(TreeBody *t) {
  if (t->b) W->DestroyBody(t->b);
  t->b = NULL, t->j = NULL, t->k = -1;
}

/* a tree is four parts 0.45 x 0.85 m (density 0.2 x the vehicle's obstacle weight, friction 0.8), each held to the
 * one under it by a joint that bends +-0.05 rad and a spring; here the four are one trunk on one such joint */
void tree_make(TreeBody *t, int k, float weight) {
  float x, y;
  if (!stage_tree(k, &x, &y)) return;
  b2PolygonShape box;
  box.SetAsBox(0.225f, 0.425f * 4, b2Vec2(0, 0.425f * 4), 0);
  b2BodyDef bd;
  bd.type = b2_dynamicBody;
  bd.position.Set(x, y);
  bd.userData = &tag_tree;
  t->b = W->CreateBody(&bd);
  b2FixtureDef fd;
  fd.shape = &box;
  fd.friction = 0.8f;
  fd.density = 0.2f * weight;
  fd.filter.categoryBits = T_TREE;
  fd.filter.maskBits = TREE_MASK;
  t->b->CreateFixture(&fd);
  b2RevoluteJointDef rj;
  rj.Initialize(ground, t->b, b2Vec2(x, y));
  rj.enableLimit = true;
  rj.lowerAngle = -0.05f;
  rj.upperAngle = 0.05f;
  t->j = (b2RevoluteJoint *)W->CreateJoint(&rj);
  t->k = k, t->t = 0, t->held = 0;
}

const float OBSTACLE_WEIGHT[NVEHICLES] = {0.85f, 1.2f, 0.45f, 1.0f, 1.0f, 1.0f, 0.95f, 0.6f};

void update_trees(float cx) {
  tree_broken = -1;
  if (stage->feature != FEAT_TREES) return;
  for (int i = 0; i < MAXTREE; i++) {
    TreeBody *t = &trees[i];
    if (t->k < 0) continue;
    float x = t->b->GetPosition().x;
    /* Tree::onSimulate: a part bent past 0.3 rad breaks off. The game's four light parts give way to a knock or
     * a push; the one trunk here breaks on a knock or when held at its stop for 0.75 s */
    if (t->j) {
      bool stop = fabsf(t->j->GetJointAngle()) > 0.045f;
      t->held = stop ? t->held + DT : 0;
    }
    if (t->j && (fabsf(t->j->GetJointAngle()) > 0.3f || fabsf(t->j->GetReactionTorque(CAR_HZ)) > 40 || t->held > 0.75f)) {
      W->DestroyJoint(t->j);
      t->j = NULL;
      stage_tree_fall(t->k);
      tree_broken = t->k;
    }
    if (!t->j) {
      t->t += DT;
      if (t->t > 1) {   /* fallen: passes through after a second, gone after four */
        b2Filter f;
        f.categoryBits = 0, f.maskBits = 0;
        t->b->GetFixtureList()->SetFilterData(f);
      }
      if (t->t > 4) tree_free(t);
      continue;
    }
    if (x < cx - 12 || x > cx + 24) tree_free(t);
  }
  for (int k = stage_tree_first(cx - 10);; k++) {
    float x, y;
    if (!stage_tree(k, &x, &y) || x > cx + 20) break;
    if (x < cx - 10 || !stage_tree_standing(k)) continue;
    bool have = false;
    for (int i = 0; i < MAXTREE; i++) have = have || trees[i].k == k;
    if (have) continue;
    for (int i = 0; i < MAXTREE; i++)
      if (trees[i].k < 0) {
        tree_make(&trees[i], k, OBSTACLE_WEIGHT[V.veh]);
        break;
      }
  }
}

/* ------------------------------------------------------------------ driving (accelerate, decelerate, idleEngine) */
float wheel_w(int i) { return V.wheel[i]->GetAngularVelocity(); }

/* the air control: cars push while 5 steps in the air; the bikes ease in over 15 more, always */
void air_control(int pedal) {
  b2Body *ch = V.chassis;
  float w = ch->GetAngularVelocity();
  bool bike = V.veh == V_BIKE || V.veh == V_QUAD;
  int reps = V.veh == V_TANK ? V.nw : 1;   /* (the Tank pushes once a wheel) */
  for (int r = 0; r < reps; r++) {
    if (bike) {
      float a = clampf((float)(V.air_frames - 5) / 15.0f, 0, 1) * V.air;
      if (pedal > 0) ch->ApplyTorque(20 * a * clampf(a - w, 0, 1));
      else ch->ApplyTorque(20 * a * clampf(-a - w, -1, 0));
    } else if (pedal > 0 && V.air_frames > 5) {
      ch->ApplyTorque(V.air * clampf(1 - w, 0, 1));
    } else if (pedal < 0 && V.air_frames > 0) {
      ch->ApplyTorque(V.air * clampf(-1 - w, -1, 0));
    }
  }
}

void idle_engine(void) {
  for (int i = 0; i < V.nw; i++) V.joint[i]->SetMaxMotorTorque(0);
  V.accelerating = false;
}

void accelerate(void) {
  air_control(1);
  if (V.veh == V_TANK) {
    for (int i = 0; i < V.nw; i++) {
      float f = clampf(V.speed - fabsf(wheel_w(i)), 0, 1);
      float t = (float)(i + 1) / (float)V.nw * 2 + (V.turbo + 0.5f) * ((1 - V.awd) * V.s->torque * f);
      if (V.air_frames > 0) t *= 0.25f;
      V.joint[i]->SetMaxMotorTorque(t);
      V.joint[i]->SetMotorSpeed(-V.speed);
    }
  } else {
    for (int i = 0; i < 2; i++) {
      float f = clampf(V.speed - fabsf(wheel_w(i)), 0, 1);
      float t = V.share[i] * V.s->torque * f * (0.5f + V.turbo);
      if (V.air_frames > 0) t *= 0.25f;
      V.joint[i]->SetMaxMotorTorque(t);
      V.joint[i]->SetMotorSpeed(-V.speed);
    }
  }
  V.accelerating = true;
}

void decelerate(void) {
  V.decel_air = true;
  air_control(-1);
  bool tank = V.veh == V_TANK;
  if (V.chassis->GetLinearVelocity().x > 2.5f) {   /* rolling forward: brake */
    for (int i = 0; i < V.nw; i++) {
      float t = V.air_frames > 5 ? 1.0f : 20.0f;
      if (tank) t = (t + (float)(i + 1) / (float)V.nw * 2) * 0.5f;
      V.joint[i]->SetMaxMotorTorque(t);
      V.joint[i]->SetMotorSpeed(0);
    }
    V.accelerating = false;
    return;
  }
  for (int i = 0; i < V.nw; i++) {   /* else reverse */
    float f = powf(clampf(V.speed - fabsf(wheel_w(i)), 0, 1), 4.05f);
    float t;
    if (tank) t = ((float)(i + 1) / (float)V.nw * 2 + (0.5f + 0.5f * V.turbo) * ((1 - V.awd) * V.s->torque * f)) * 0.5f;
    else t = V.share[i] * V.s->torque * f * (0.5f + 0.5f * V.turbo);
    V.joint[i]->SetMaxMotorTorque(t);
    V.joint[i]->SetMotorSpeed(V.speed);
  }
  V.accelerating = true;
}

/* ------------------------------------------------------------------ after each step (PlayerCar*::update) */
void handle_head_hit(void) {
  if (!V.head_hit) return;
  V.head_hit = false;
  V.dead = true;
  for (int i = 0; i < 2; i++)
    if (V.spring[i]) W->DestroyJoint(V.spring[i]), V.spring[i] = NULL;
  V.neck->SetLimits(-1.22173f, 1.047198f);
}

/* PlayerCar::handleFlips: a turn counts once the chassis is back within 90 degrees of upright */
int handle_flips(void) {
  float deg = V.chassis->GetAngle() * 57.29578f;
  int id = (int)deg;
  int k = (deg > 0 ? id + 90 : id - 90) / 360 * 360;
  float d = deg - (float)k;
  if (!(d > -90 && d < 90)) return FLIP_NONE;
  int r = FLIP_NONE;
  if (k != V.flips_k) {
    if (V.alive_at_turn && V.dead) r = FLIP_NECK;
    else if (k < V.flips_k) r = FLIP_FRONT;
    else r = FLIP_BACK;
  }
  V.flips_k = k;
  V.alive_at_turn = !V.dead;
  return r;
}

void downforce(void) {
  if (V.down <= 0) return;
  b2Body *ch = V.chassis;
  b2Vec2 v = ch->GetLinearVelocity();
  float sp = v.Length();
  if (sp < 1.1920929e-7f) return;
  float a = ch->GetAngle();
  float f = clampf(sp - 3, 0, 150) * clampf((cosf(a) * v.x / sp + sinf(a) * v.y / sp - 0.75f) * 3, 0, 1) * V.down;
  ch->ApplyForceToCenter(b2Vec2(sinf(a) * f, -cosf(a) * f));
}

int update(void) {
  /* the engine's revs (the meter): towards 1 + |rear wheel| / 5 with gas, 1 without */
  if (V.accelerating) V.rev += DT * 6 * (1 + fabsf(wheel_w(0)) / 5 - V.rev);
  else V.rev += DT * 3 * (1 - V.rev);
  unsigned any = V.touch_chassis;
  for (int i = 0; i < V.nw; i++) any |= V.touch_wheel[i];
  if (any) V.air_frames = 0, V.decel_air = false;
  else V.air_frames++;
  handle_head_hit();
  int flip = handle_flips();
  downforce();
  /* the turbo builds while the gas is down */
  if (V.accelerating) V.turbo += (0.35f + 0.75f * V.turbo) * V.rate / 60.0f;
  else V.turbo -= DT * (1 + 4 * V.turbo);
  V.turbo = clampf(V.turbo, 0, V.turbo_cap);
  return flip;
}

void snapshot(Car *c, int flip) {
  b2Body *b[3] = {V.chassis, V.torso, V.head};
  CarBody *o[3] = {&c->ch, &c->torso, &c->head};
  for (int i = 0; i < 3; i++) o[i]->x = b[i]->GetPosition().x, o[i]->y = b[i]->GetPosition().y, o[i]->a = b[i]->GetAngle();
  for (int i = 0; i < V.nw; i++) {
    c->wh[i].x = V.wheel[i]->GetPosition().x, c->wh[i].y = V.wheel[i]->GetPosition().y;
    c->wh[i].a = V.wheel[i]->GetAngle();
    c->ground[i] = V.touch_wheel[i] != 0;
  }
  c->vx = V.chassis->GetLinearVelocity().x, c->vy = V.chassis->GetLinearVelocity().y;
  c->w = V.chassis->GetAngularVelocity();
  c->air = V.air_frames;
  c->dead = V.dead;
  c->flip = flip;
  c->rev = V.rev, c->turbo = V.turbo;
  c->tree_broken = tree_broken;
}

}   // namespace

/* ------------------------------------------------------------------ the C side */
extern "C" void car_init(Car *c, int veh, const uint8_t levels[4]) {
  car_free();
  W = new (world_mem) b2World(b2Vec2(0, -stage->gravity));
  W->SetContactListener(&listener);
  b2BodyDef bd;
  bd.userData = &tag_ground;
  ground = W->CreateBody(&bd);
  memset(&floor_ring, 0, sizeof floor_ring);
  memset(&ceil_ring, 0, sizeof ceil_ring);
  ceil_ring.ceiling = true;
  game_end = 0, wall = NULL, wall_x = -1e9f;
  for (int i = 0; i < 2; i++) bridges[i].k = -1, bridges[i].n = 0;
  for (int i = 0; i < MAXTREE; i++) trees[i].k = -1, trees[i].b = NULL, trees[i].j = NULL;
  /* GameContentLayer::init: the vehicle 3 m over the ground at 0 */
  float y = stage_floor(0) + 3;
  update_ground(0);
  create_vehicle(veh, levels, 0, y);
  memset(c, 0, sizeof *c);
  c->veh = veh, c->nw = V.nw;
  for (int i = 0; i < V.nw; i++) c->wr[i] = V.wr[i];
  snapshot(c, FLIP_NONE);
}

extern "C" void car_free(void) {
  if (W) W->~b2World();
  W = NULL;
}

extern "C" void car_step(Car *c, int pedal, bool engine) {
  if (!W) return;
  /* GameContentLayer::preFrameUpdate: the controls */
  if (V.dead || !engine || pedal == 0) idle_engine();
  else if (pedal < 0) decelerate();
  else accelerate();
  /* PhysicsSimulation::simulate */
  float cx = V.chassis->GetPosition().x;
  update_ground(cx);
  update_bridges(cx);
  update_trees(cx);
  V.touch_chassis = 0;
  for (int i = 0; i < V.nw; i++) V.touch_wheel[i] = 0;
  W->Step(DT, 8, 3);
  snapshot(c, update());
}

extern "C" void car_point(const Car *c, float lx, float ly, float *wx, float *wy) {
  float s = sinf(c->ch.a), co = cosf(c->ch.a);
  *wx = c->ch.x + co * lx - s * ly;
  *wy = c->ch.y + s * lx + co * ly;
}

extern "C" float car_speed(const Car *c) { return sqrtf(c->vx * c->vx + c->vy * c->vy); }

extern "C" bool car_airborne(const Car *c) { return c->air > 0; }

extern "C" bool car_touches_box(const Car *c, float x, float y, float half) {
  (void)c;
  if (!W) return false;
  b2PolygonShape box;
  box.SetAsBox(half, half);
  b2Transform bx;
  bx.Set(b2Vec2(x, y), 0);
  b2Body *parts[CAR_MAXW + 3] = {V.chassis, V.torso, V.head};
  int n = 3;
  for (int i = 0; i < V.nw; i++) parts[n++] = V.wheel[i];
  for (int i = 0; i < n; i++) {
    b2Vec2 p = parts[i]->GetPosition();
    if (fabsf(p.x - x) > half + 3 || fabsf(p.y - y) > half + 3) continue;
    for (b2Fixture *f = parts[i]->GetFixtureList(); f; f = f->GetNext())
      if (b2TestOverlap(&box, 0, f->GetShape(), 0, bx, parts[i]->GetTransform())) return true;
  }
  return false;
}

extern "C" float car_fuel_rate(int veh, const uint8_t levels[4]) {
  int lf = level_of(veh, levels, UP_FUEL);
  if (veh == V_BUGGY) return 1.65f - 0.9f * (float)lf / 21.0f;
  if (veh == V_TANK) return 1.5f - 0.9f * (float)lf / 21.0f;
  return 1;
}

extern "C" int car_planks(CarBody *out, int most) {
  int n = 0;
  for (int i = 0; i < 2; i++)
    for (int j = 0; j < bridges[i].n && n < most; j++, n++) {
      b2Body *p = bridges[i].plank[j];
      out[n].x = p->GetPosition().x, out[n].y = p->GetPosition().y, out[n].a = p->GetAngle();
    }
  return n;
}

extern "C" int car_trees(CarBody *out, int *which, int most) {
  int n = 0;
  for (int i = 0; i < MAXTREE && n < most; i++) {
    if (trees[i].k < 0) continue;
    which[n] = trees[i].k;
    out[n].x = trees[i].b->GetPosition().x, out[n].y = trees[i].b->GetPosition().y, out[n].a = trees[i].b->GetAngle();
    n++;
  }
  return n;
}
