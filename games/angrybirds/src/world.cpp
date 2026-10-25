/* The physics (world.h): Box2D 2.2.1 as Angry Birds sets it up.
 *
 * As in the game: gravity (0, 20) (y points down), steps of 33 ms with 10 and 10 iterations, warm starting and
 * continuous collision, a linear slop of 0.05 and the other settings it changes (see b2Settings.h). A body for
 * each object, of the object's material (density, friction, restitution) and shape (a circle about the body's
 * origin or the polygon as given, 20 of its pixels a meter); angular damping 1, a bird's 2, a bird a bullet that
 * doesn't turn until it is launched. Joints: distance (4 Hz, damping 0.5, the anchors' distance), weld (at the
 * first body's centre) and revolute (the reference angle as the game's Box2D gives it: first minus second). */
#include <new>
#include <Box2D/Common/b2BlockAllocator.h>
#include <Box2D/Dynamics/b2World.h>
#include <Box2D/Dynamics/b2Body.h>
#include <Box2D/Dynamics/b2Fixture.h>
#include <Box2D/Dynamics/b2WorldCallbacks.h>
#include <Box2D/Dynamics/Contacts/b2Contact.h>
#include <Box2D/Dynamics/Joints/b2DistanceJoint.h>
#include <Box2D/Dynamics/Joints/b2RevoluteJoint.h>
#include <Box2D/Dynamics/Joints/b2WeldJoint.h>
#include <Box2D/Collision/Shapes/b2CircleShape.h>
#include <Box2D/Collision/Shapes/b2PolygonShape.h>
#include "world.h"
extern "C" {
#include "res.h"
}

#define BODY_CAP (MAX_OBJECTS + 64)

/* (Box2D's classes have virtual destructors, which name delete; nothing is ever deleted: all is in the arena) */
void operator delete(void *) noexcept {}
void operator delete(void *, unsigned int) noexcept {}
void operator delete(void *, unsigned long) noexcept {}

static b2World *world;
static b2Body *bodies[BODY_CAP];
static b2Shape *shapes[TYPE_COUNT];
static b2Body *ground;

static inline int obj_of(b2Fixture *f) { return (int)(intptr_t)f->GetUserData() - 1; }

class Listener : public b2ContactListener {
 public:
  void BeginContact(b2Contact *c) {
    int a = obj_of(c->GetFixtureA()), b = obj_of(c->GetFixtureB());
    if (a >= 0 && b >= 0) game_contact_begin(a, b);
  }
  void PreSolve(b2Contact *c, const b2Manifold *old) {
    (void)old;
    int a = obj_of(c->GetFixtureA()), b = obj_of(c->GetFixtureB());
    if (a >= 0 && b >= 0) c->SetEnabled(game_contact_enabled(a, b));
  }
};
/* (made at run time: the app runs no constructors of its own) */
static union { char mem[sizeof(Listener)]; void *align; } listener_mem;
static Listener *listener;

extern "C" bool world_begin(void *mem, int size, int proxies) {
  world = 0;
  ground = 0;
  for (int i = 0; i < BODY_CAP; i++) bodies[i] = 0;
  for (int i = 0; i < TYPE_COUNT; i++) shapes[i] = 0;
  b2ArenaReset(mem, size);
  /* the broad phase sized for the level's shapes and what joins them (split birds, eggs, the Mighty Eagle) */
  b2_treeCapacity = 2 * (proxies + 24);
  b2_moveCapacity = proxies + 24;
  b2_pairCapacity = 256;
  void *w = b2ArenaAlloc(sizeof(b2World));
  if (!w) return false;
  world = new (w) b2World(b2Vec2(0.0f, 20.0f));
  world->SetAllowSleeping(true);
  world->SetWarmStarting(true);
  world->SetContinuousPhysics(true);
  if (!listener) listener = new (listener_mem.mem) Listener;
  world->SetContactListener(listener);
  return b2ArenaFailures() == 0;
}

extern "C" void world_grow(int size) { b2ArenaGrow(size); }

extern "C" void world_end(void) {
  world = 0;   /* (everything was in the arena) */
  ground = 0;
}

static b2Shape *shape_of(int type) {
  if (shapes[type]) return shapes[type];
  const Shape *s = res_shape(res_type(type)->shape);
  if (s->kind == 0) {
    void *m = b2ArenaAlloc(sizeof(b2CircleShape));
    if (!m) return 0;
    b2CircleShape *c = new (m) b2CircleShape;
    c->m_radius = s->r;
    shapes[type] = c;
  } else {
    void *m = b2ArenaAlloc(sizeof(b2PolygonShape));
    if (!m) return 0;
    b2PolygonShape *p = new (m) b2PolygonShape;
    b2Vec2 v[b2_maxPolygonVertices];
    for (int i = 0; i < s->n; i++) v[i].Set(s->v[2 * i], s->v[2 * i + 1]);
    p->Set(v, s->n);
    shapes[type] = p;
  }
  return shapes[type];
}

extern "C" bool world_body(int obj, int type, float x, float y, float angle, int flags) {
  if (!world || obj < 0 || obj >= BODY_CAP || bodies[obj]) return false;
  const Type *t = res_type(type);
  const Material *m = res_material(t->material);
  b2Shape *s = shape_of(type);
  if (!s) return false;
  b2BodyDef bd;
  bd.type = m->is_static ? b2_staticBody : b2_dynamicBody;
  bd.position.Set(x, y);
  bd.angle = angle;
  bd.angularDamping = 1.0f;
  bd.allowSleep = true;
  bd.awake = !(flags & W_ASLEEP);
  bd.bullet = (flags & W_BULLET) != 0;
  bd.fixedRotation = (flags & W_FIXED_ROTATION) != 0;
  if (t->cls >= C_RED && t->cls <= C_EAGLE) bd.angularDamping = 2.0f;
  b2Body *b = world->CreateBody(&bd);
  if (!b) return false;
  b2FixtureDef fd;
  fd.shape = s;
  fd.density = m->density;
  fd.friction = m->friction;
  fd.restitution = m->restitution;
  fd.userData = (void *)(intptr_t)(obj + 1);
  if (!b->CreateFixture(&fd)) {
    world->DestroyBody(b);
    return false;
  }
  bodies[obj] = b;
  return true;
}

extern "C" bool world_rect(int obj, int material, float x0, float y0, float x1, float y1) {
  if (!world) return false;
  if (!ground) {
    b2BodyDef bd;
    ground = world->CreateBody(&bd);
    if (!ground) return false;
  }
  void *mem = b2ArenaAlloc(sizeof(b2PolygonShape));
  if (!mem) return false;
  b2PolygonShape *p = new (mem) b2PolygonShape;
  p->SetAsBox((x1 - x0) / 2, (y1 - y0) / 2, b2Vec2((x0 + x1) / 2, (y0 + y1) / 2), 0.0f);
  const Material *m = res_material(material);
  b2FixtureDef fd;
  fd.shape = p;
  fd.density = m->density;
  fd.friction = m->friction;
  fd.restitution = m->restitution;
  fd.userData = (void *)(intptr_t)(obj + 1);
  return ground->CreateFixture(&fd) != 0;
}

extern "C" bool world_joint(int kind, int a, int b, bool collide, float x1, float y1, float x2, float y2, bool motor,
                            bool limit, float speed, float torque, float lower, float upper) {
  if (!world || !world_has(a) || !world_has(b)) return false;
  b2Body *A = bodies[a], *B = bodies[b];
  if (kind == W_DISTANCE) {
    b2DistanceJointDef d;
    d.Initialize(A, B, A->GetWorldPoint(b2Vec2(x1, y1)), B->GetWorldPoint(b2Vec2(x2, y2)));
    d.frequencyHz = 4.0f;
    d.dampingRatio = 0.5f;
    d.collideConnected = collide;
    return world->CreateJoint(&d) != 0;
  }
  if (kind == W_WELD) {
    b2WeldJointDef d;
    d.Initialize(A, B, A->GetWorldCenter());
    d.collideConnected = collide;
    return world->CreateJoint(&d) != 0;
  }
  b2RevoluteJointDef d;
  d.Initialize(A, B, A->GetWorldPoint(b2Vec2(x1, y1)));
  d.referenceAngle = A->GetAngle() - B->GetAngle();   /* (as the game's Box2D sets it) */
  d.collideConnected = collide;
  d.enableMotor = motor;
  d.enableLimit = limit;
  d.motorSpeed = speed;
  d.maxMotorTorque = torque;
  d.lowerAngle = lower;
  d.upperAngle = upper;
  return world->CreateJoint(&d) != 0;
}

extern "C" void world_break_joints(void) {
  if (!world) return;
  while (b2Joint *j = world->GetJointList()) world->DestroyJoint(j);
}

/* a step's temporary memory (the islands, the contacts' constraints) from the work buffer first (world.h): the
   app's RAM goes to the bodies and contacts */
static void *work_mem;
static int work_size;
extern "C" void world_work(void *mem, int size) { work_mem = mem, work_size = size; }
extern "C" void world_step(void) {
  if (!world) return;
  b2ArenaSide(work_mem, work_size);
  world->Step(W_STEP, 10, 10);
  b2ArenaSide(0, 0);
}

extern "C" void world_destroy(int obj) {
  if (!world_has(obj)) return;
  world->DestroyBody(bodies[obj]);
  bodies[obj] = 0;
}

extern "C" bool world_has(int obj) { return world && obj >= 0 && obj < BODY_CAP && bodies[obj]; }

extern "C" void world_get(int obj, WBody *o) {
  if (!world_has(obj)) {
    o->x = o->y = o->angle = o->vx = o->vy = o->w = 0;
    return;
  }
  b2Body *b = bodies[obj];
  const b2Vec2 &p = b->GetPosition(), &v = b->GetLinearVelocity();
  o->x = p.x, o->y = p.y, o->angle = b->GetAngle(), o->vx = v.x, o->vy = v.y, o->w = b->GetAngularVelocity();
}

extern "C" bool world_xf(int obj, float *x, float *y, float *s, float *c) {
  if (!world_has(obj)) return false;
  const b2Transform &t = bodies[obj]->GetTransform();
  *x = t.p.x, *y = t.p.y, *s = t.q.s, *c = t.q.c;
  return true;
}

extern "C" bool world_is_static(int obj) { return !world_has(obj) || bodies[obj]->GetType() == b2_staticBody; }
extern "C" bool world_awake(int obj) { return world_has(obj) && bodies[obj]->IsAwake(); }
extern "C" bool world_jointed(int obj) { return world_has(obj) && bodies[obj]->GetJointList() != 0; }
extern "C" float world_mass(int obj) { return world_has(obj) ? bodies[obj]->GetMass() : 0.0f; }

extern "C" void world_set_velocity(int obj, float vx, float vy) {
  if (world_has(obj)) bodies[obj]->SetLinearVelocity(b2Vec2(vx, vy));
}
extern "C" void world_set_angular(int obj, float w) {
  if (world_has(obj)) bodies[obj]->SetAngularVelocity(w);
}
extern "C" void world_set_transform(int obj, float x, float y, float angle) {
  if (world_has(obj)) bodies[obj]->SetTransform(b2Vec2(x, y), angle);
}
extern "C" void world_set_awake(int obj, bool awake) {
  if (world_has(obj)) bodies[obj]->SetAwake(awake);
}
extern "C" void world_free_rotation(int obj) {
  if (world_has(obj)) bodies[obj]->SetFixedRotation(false);
}
extern "C" void world_impulse(int obj, float ix, float iy, float px, float py) {
  if (world_has(obj)) bodies[obj]->ApplyLinearImpulse(b2Vec2(ix, iy), b2Vec2(px, py));
}
extern "C" void world_angular_impulse(int obj, float i) {
  if (world_has(obj)) bodies[obj]->ApplyAngularImpulse(i);
}

struct Query : public b2QueryCallback {
  void (*f)(int, void *);
  void *ctx;
  bool ReportFixture(b2Fixture *fx) {
    int o = obj_of(fx);
    if (o >= 0) f(o, ctx);
    return true;
  }
};

extern "C" void world_query(float x0, float y0, float x1, float y1, void (*f)(int obj, void *ctx), void *ctx) {
  if (!world) return;
  Query q;
  q.f = f, q.ctx = ctx;
  b2AABB box;
  box.lowerBound.Set(x0, y0);
  box.upperBound.Set(x1, y1);
  world->QueryAABB(&q, box);
}

extern "C" int world_bodies(void) { return world ? world->GetBodyCount() : 0; }
extern "C" int world_contacts(void) { return world ? world->GetContactCount() : 0; }
extern "C" int world_arena_used(void) { return b2ArenaUsed(); }
extern "C" int world_arena_peak(void) { return b2ArenaPeak(); }
extern "C" int world_arena_failures(void) { return b2ArenaFailures(); }
extern "C" int world_side_peak(void) { return b2ArenaSidePeak(); }

#ifdef HOST
#include <string.h>
/* (tests) contacts whose shapes touch */
extern "C" int world_touching(void) {
  int n = 0;
  if (world)
    for (b2Contact *c = world->GetContactList(); c; c = c->GetNext()) n += c->IsTouching();
  return n;
}
struct Snap {
  b2World *world;
  b2Body *bodies[BODY_CAP];
  b2Shape *shapes[TYPE_COUNT];
  b2Body *ground;
  int32 tree, move, pair;
};
extern "C" int world_snapshot_size(int arena_size) { return (int)sizeof(Snap) + b2ArenaStateSize() + arena_size; }
extern "C" void world_snapshot_save(void *to, const void *arena, int arena_size) {
  Snap *s = (Snap *)to;
  s->world = world, s->ground = ground;
  memcpy(s->bodies, bodies, sizeof bodies);
  memcpy(s->shapes, shapes, sizeof shapes);
  s->tree = b2_treeCapacity, s->move = b2_moveCapacity, s->pair = b2_pairCapacity;
  b2ArenaStateSave((char *)to + sizeof(Snap));
  memcpy((char *)to + sizeof(Snap) + b2ArenaStateSize(), arena, (size_t)arena_size);
}
extern "C" void world_snapshot_load(const void *from, void *arena, int arena_size) {
  const Snap *s = (const Snap *)from;
  world = s->world, ground = s->ground;
  memcpy(bodies, s->bodies, sizeof bodies);
  memcpy(shapes, s->shapes, sizeof shapes);
  b2_treeCapacity = s->tree, b2_moveCapacity = s->move, b2_pairCapacity = s->pair;
  b2ArenaStateLoad((const char *)from + sizeof(Snap));
  memcpy(arena, (const char *)from + sizeof(Snap) + b2ArenaStateSize(), (size_t)arena_size);
}
#endif
