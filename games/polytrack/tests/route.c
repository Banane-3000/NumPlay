/* Finds a run to the finish of each official track with the game's own physics, for tests/routes.h:
 *
 *   route [TRACK...]     prints the routes (C) of the tracks given, or of all
 *
 * How: a map of how far each place is from the next checkpoint along the track (2 m cubes near the track's
 * surfaces, walked from the checkpoint's box), and a beam search: every 100 ms each of the best few cars tries the
 * keys, looks half a second ahead, and the ones nearest the next checkpoint go on. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../src/car.h"
#include "../src/data.h"
#include "../src/world.h"

#define VOX 2.0f
#define INF 0xFFFF

typedef struct {
  int nx, ny, nz;
  float x0, y0, z0;
  uint8_t *walk;                    /* 1: a car can be there */
  uint8_t *wall;                    /* 1: a barrier; 2: next to one or to a drop, 3: one further in */
  uint8_t *cut;                     /* bits 1, 2, 4: a surface between the cube and the next one in x, y, z */
  uint16_t *dist[16];               /* to each target: the checkpoint orders, then the finish */
  int ntarget;
} field_t;

static field_t F;

static int vidx(int x, int y, int z) { return (z * F.ny + y) * F.nx + x; }
static bool vox_of(v3 p, int *x, int *y, int *z) {
  *x = (int)((p.x - F.x0) / VOX), *y = (int)((p.y - F.y0) / VOX), *z = (int)((p.z - F.z0) / VOX);
  return *x >= 0 && *y >= 0 && *z >= 0 && *x < F.nx && *y < F.ny && *z < F.nz;
}

/* the cubes a car's centre goes through on a triangle: up to 2.5 m off its face, on its front side only (so the
   map doesn't go through a thin wall) */
static void mark_tri(uint8_t *g, v3 a, v3 b, v3 c, v3 n, uint8_t kind) {
  float l = pt_maxf(vlen(vsub(b, a)), pt_maxf(vlen(vsub(c, a)), vlen(vsub(c, b))));
  int m = (int)(l / (VOX * 0.5f)) + 1;
  for (int i = 0; i <= m; i++)
    for (int j = 0; j <= m - i; j++) {
      float u = (float)i / (float)m, v = (float)j / (float)m;
      v3 p = vadd(a, vadd(vmul(vsub(b, a), u), vmul(vsub(c, a), v)));
      for (int k = 0; k < 3; k++) {
        int x, y, z;
        if (vox_of(vadd(p, vmul(n, 0.5f + (float)k)), &x, &y, &z) && g[vidx(x, y, z)] < kind) g[vidx(x, y, z)] = kind;
      }
    }
}

/* parts a car drives on whatever their slope: the wall tracks, loops, tilted roads, slopes to vertical */
static bool drive_part(int id) {
  static const uint8_t L[] = {30, 31, 32, 50, 51, 66, 67, 79, 80, 81, 87, 88, 90, 98, 134, 135, 136, 137, 138, 139, 142,
                              143, 144, 156, 157, 158, 159, 160, 161, 162, 163, 166, 167};
  for (unsigned i = 0; i < sizeof L; i++)
    if (L[i] == id) return true;
  return false;
}

static void floor_tri(float *g, v3 a, v3 b, v3 c) {
  float l = pt_maxf(vlen(vsub(b, a)), pt_maxf(vlen(vsub(c, a)), vlen(vsub(c, b))));
  int n = (int)(l / (VOX * 0.5f)) + 1;
  for (int i = 0; i <= n; i++)
    for (int j = 0; j <= n - i; j++) {
      float u = (float)i / (float)n, v = (float)j / (float)n;
      v3 p = vadd(a, vadd(vmul(vsub(b, a), u), vmul(vsub(c, a), v)));
      int x, y, z;
      if (vox_of(p, &x, &y, &z) && p.y < g[vidx(x, y, z)]) g[vidx(x, y, z)] = p.y;
    }
}

static void build_field(void) {
  const pt_track *t = W.t;
  v3 lo = V3(1e9f, 1e9f, 1e9f), hi = V3(-1e9f, -1e9f, -1e9f);
  for (int i = 0; i < t->ninst; i++) {
    v3 a, b;
    world_box(world_inst(i), &a, &b);
    lo = V3(pt_minf(lo.x, a.x), pt_minf(lo.y, a.y), pt_minf(lo.z, a.z));
    hi = V3(pt_maxf(hi.x, b.x), pt_maxf(hi.y, b.y), pt_maxf(hi.z, b.z));
  }
  F.x0 = lo.x - 40.0f, F.y0 = -2.0f, F.z0 = lo.z - 40.0f;
  F.nx = (int)((hi.x - lo.x + 80.0f) / VOX) + 1;
  F.ny = (int)((hi.y + 30.0f) / VOX) + 1;
  F.nz = (int)((hi.z - lo.z + 80.0f) / VOX) + 1;
  size_t n = (size_t)F.nx * F.ny * F.nz;
  uint8_t *surf = calloc(n, 1);
  F.walk = calloc(n, 1);
  float *floor = malloc(n * sizeof(float));
  for (size_t i = 0; i < n; i++) floor[i] = 1e9f;
  v3 *walls = NULL;
  int nwall = 0, cap = 0;
  /* every physics triangle */
  for (int i = 0; i < t->ninst; i++) {
    const pt_inst *in = world_inst(i);
    const pt_part *p = &pt_parts[in->part];
    uint32_t mask = p->simple ? pt_masks[t->mask + in->mask] : 0xFFFFFFFFu;
    for (int k = 0; k < p->ncol; k++) {
      if (k < 32 && !(mask & (1u << k))) continue;
      const uint16_t *tr = &pt_ctri[(p->col + k) * 3];
      v3 v[3];
      for (int j = 0; j < 3; j++) {
        const int16_t *q = &pt_pool[(p->vbase + tr[j]) * 3];
        v[j] = world_apply(in, V3((float)q[0] / PT_FIX, (float)q[1] / PT_FIX, (float)q[2] / PT_FIX));
      }
      v3 nn = vnorm(vcross(vsub(v[1], v[0]), vsub(v[2], v[0])));
      /* (a part's floor is driven on however the part is turned: a road up a wall) */
      const int8_t *o = world_orient(in);
      float local_up = (float)o[1] * nn.x + (float)o[4] * nn.y + (float)o[7] * nn.z;
      bool dp = drive_part(pt_part_id[in->part]) || (in->orient >> 2 && local_up > 0.5f);
      /* flat floors (1), and what can be climbed: slopes, walls, loops (2) */
      if (nn.y > 0.5f || dp) {
        mark_tri(surf, v[0], v[1], v[2], nn, nn.y >= 0.96f && !dp ? 1 : 2);
        floor_tri(floor, v[0], v[1], v[2]);
      } else if (nn.y > -0.5f) {
        if (nwall + 3 > cap) cap = cap ? cap * 2 : 65536, walls = realloc(walls, sizeof(v3) * (size_t)cap);
        walls[nwall++] = v[0], walls[nwall++] = v[1], walls[nwall++] = v[2];
      }
    }
  }
  /* near a surface (and the ground everywhere) */
  for (int z = 0; z < F.nz; z++)
    for (int y = 0; y < F.ny; y++)
      for (int x = 0; x < F.nx; x++) {
        F.walk[vidx(x, y, z)] = (uint8_t)(surf[vidx(x, y, z)] + 1);   /* 1 air, 2 over a floor, 3 by a slope or a wall */
      }
  /* not inside blocks */
  for (int i = 0; i < t->ninst; i++) {
    const pt_inst *in = world_inst(i);
    int id = pt_part_id[in->part];
    if (id != 29 && id != 53 && id != 54) continue;   /* Block, HalfBlock, QuarterBlock */
    v3 a, b;
    world_box(in, &a, &b);
    for (float zz = a.z + 1.0f; zz < b.z - 0.5f; zz += VOX)
      for (float yy = a.y + 1.0f; yy < b.y - 0.5f; yy += VOX)
        for (float xx = a.x + 1.0f; xx < b.x - 0.5f; xx += VOX) {
          int x, y, z;
          if (vox_of(V3(xx, yy, zz), &x, &y, &z)) F.walk[vidx(x, y, z)] = 0;
        }
  }
  /* barriers: walls standing on a floor, where a car can't go; next to them a cube costs more */
  F.wall = calloc(n, 1);
  for (int w = 0; w < nwall; w += 3) {
    v3 a = walls[w], b = walls[w + 1], c = walls[w + 2];
    float l = pt_maxf(vlen(vsub(b, a)), pt_maxf(vlen(vsub(c, a)), vlen(vsub(c, b))));
    int m = (int)(l / (VOX * 0.25f)) + 1;
    for (int i = 0; i <= m; i++)
      for (int j = 0; j <= m - i; j++) {
        float u = (float)i / (float)m, v = (float)j / (float)m;
        v3 p = vadd(a, vadd(vmul(vsub(b, a), u), vmul(vsub(c, a), v)));
        int x, y, z;
        if (!vox_of(p, &x, &y, &z)) continue;
        int k = vidx(x, y, z);
        bool on = floor[k] < p.y - 0.25f || (y > 0 && floor[vidx(x, y - 1, z)] < p.y - 0.25f);
        if (!on && p.y > 0.25f && p.y < VOX) on = true;     /* (the ground) */
        if (on) F.wall[k] = 1;
      }
  }
  for (size_t i = 0; i < n; i++)
    if (F.wall[i] == 1) F.walk[i] = 0;
  /* edges: next to a barrier or a drop (2), and one further in (3); the way keeps off them */
  for (size_t i = 0; i < n; i++) F.wall[i] = F.wall[i] == 1;
  for (int pass = 2; pass <= 3; pass++)
    for (int z = 1; z < F.nz - 1; z++)
      for (int y = 0; y < F.ny; y++)
        for (int x = 1; x < F.nx - 1; x++) {
          int i = vidx(x, y, z);
          if (F.walk[i] < 2 || F.wall[i]) continue;
          int nb[4] = {i - 1, i + 1, vidx(x, y, z - 1), vidx(x, y, z + 1)};
          for (int k = 0; k < 4; k++) {
            int j = nb[k];
            bool edge = pass == 2 ? F.walk[j] < 2 : F.wall[j] == 2;
            if (edge) F.wall[i] = (uint8_t)pass;
          }
        }
  /* surfaces between neighbours (so the way doesn't pass through a thin wall or floor) */
  F.cut = calloc(n, 1);
  static wtri near[PT_CACHE * 8];
  for (int z = 0; z < F.nz - 1; z++)
    for (int y = 0; y < F.ny - 1; y++)
      for (int x = 0; x < F.nx - 1; x++) {
        int i = vidx(x, y, z);
        int nb[3] = {i + 1, vidx(x, y + 1, z), vidx(x, y, z + 1)};
        bool any = false;
        for (int k = 0; k < 3; k++) any |= F.walk[i] >= 2 || F.walk[nb[k]] >= 2;
        if (!any || !F.walk[i]) continue;
        v3 a = V3(F.x0 + ((float)x + 0.5f) * VOX, F.y0 + ((float)y + 0.5f) * VOX, F.z0 + ((float)z + 0.5f) * VOX);
        int m = world_gather(vsub(a, V3(0.1f, 0.1f, 0.1f)), vadd(a, V3(VOX + 0.1f, VOX + 0.1f, VOX + 0.1f)), near,
                             PT_CACHE * 8);
        for (int k = 0; k < 3; k++) {
          if (!F.walk[nb[k]] || (F.walk[i] < 2 && F.walk[nb[k]] < 2)) continue;
          v3 b = vadd(a, V3(k == 0 ? VOX : 0.0f, k == 1 ? VOX : 0.0f, k == 2 ? VOX : 0.0f));
          for (int q = 0; q < m; q++) {
            const wtri *w = &near[q];
            float da = vdot(w->n, a) - w->d, db = vdot(w->n, b) - w->d;
            if ((da > 0.0f) == (db > 0.0f)) continue;
            float u = da / (da - db);
            v3 p = vadd(a, vmul(vsub(b, a), u));
            if (vdot(vcross(vsub(w->b, w->a), vsub(p, w->a)), w->n) < 0.0f) continue;
            if (vdot(vcross(vsub(w->c, w->b), vsub(p, w->b)), w->n) < 0.0f) continue;
            if (vdot(vcross(vsub(w->a, w->c), vsub(p, w->c)), w->n) < 0.0f) continue;
            F.cut[i] |= (uint8_t)(1 << k);
            break;
          }
        }
      }
  free(walls);
  free(floor);
  free(surf);
}

/* Dijkstra with a bucket queue: near a drivable surface a cube costs 2; in the air 6 across, 3 down and 24 up
   (a jump goes across or down, not up) */
#define NB 32
static int vcost = 1;      /* up or down a slope or a wall (VC=) */
static int air_up = 0;     /* up through the air (AU=; 0: never) */
static int *bucket[NB], bcount[NB], bcap[NB];
static void push(int b, int i) {
  if (bcount[b] == bcap[b]) bcap[b] = bcap[b] ? bcap[b] * 2 : 4096, bucket[b] = realloc(bucket[b], sizeof(int) * (size_t)bcap[b]);
  bucket[b][bcount[b]++] = i;
}
static void bfs(uint16_t *d, v3 lo, v3 hi) {
  size_t n = (size_t)F.nx * F.ny * F.nz;
  for (size_t i = 0; i < n; i++) d[i] = INF;
  for (int b = 0; b < NB; b++) bcount[b] = 0;
  for (float zz = lo.z; zz <= hi.z + 0.01f; zz += 1.0f)
    for (float yy = lo.y; yy <= hi.y + 0.01f; yy += 1.0f)
      for (float xx = lo.x; xx <= hi.x + 0.01f; xx += 1.0f) {
        int x, y, z;
        if (!vox_of(V3(xx, yy, zz), &x, &y, &z)) continue;
        int i = vidx(x, y, z);
        if (d[i] == INF) d[i] = 0, push(0, i);
      }
  /* (walked backwards from the target: a step from j to i costs what going from i to j would) */
  for (uint32_t cur = 0, left = 1; left && cur < 60000; cur++) {
    int b = (int)(cur % NB);
    left = 0;
    for (int k = 0; k < NB; k++) left += (uint32_t)bcount[k];
    while (bcount[b]) {
      int i = bucket[b][--bcount[b]];
      if (d[i] != cur) continue;
      int x = i % F.nx, y = (i / F.nx) % F.ny, z = i / (F.nx * F.ny);
      static const int D[6][3] = {{1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1}};
      for (int k = 0; k < 6; k++) {
        int a = x + D[k][0], bb = y + D[k][1], c = z + D[k][2];
        if (a < 0 || bb < 0 || c < 0 || a >= F.nx || bb >= F.ny || c >= F.nz) continue;
        int j = vidx(a, bb, c);
        int w = F.walk[j];
        if (!w) continue;
        /* (through a surface: no) */
        int lo = j < i ? j : i, axis = k / 2;
        if (F.cut[lo] & (1 << axis)) continue;
        /* the car would go from j to i: up when i is above j */
        int cost;
        int wi = F.walk[i];
        if (D[k][1] == -1) {
          /* i above j: up only along a slope or a wall */
          if (wi != 3 && w != 3) {
            /* (up through the air: off a ramp, say, dearly) */
            if (!air_up) continue;
            cost = air_up;
          } else cost = wi == 3 && w == 3 ? vcost : 2;
        } else if (D[k][1] == 1 && wi == 3 && w == 3) cost = vcost;
        else if (w >= 2 && wi >= 2) cost = 2;
        else if (D[k][1] == 1) cost = 3;        /* down */
        else cost = 6;
        if (F.wall[j] == 2) cost += 4;
        else if (F.wall[j] == 3) cost += 2;
        uint32_t nd = cur + (uint32_t)cost;
        if (nd < d[j]) d[j] = (uint16_t)nd, push((int)(nd % NB), j);
      }
    }
  }
}

static void build_targets(void) {
  const pt_track *t = W.t;
  size_t n = (size_t)F.nx * F.ny * F.nz;
  int no = car_checkpoints();
  F.ntarget = no + 1;
  /* the checkpoint detectors, by order */
  detector_t d;
  int k = 0;
  uint16_t last = 0xFFFF;
  int target = -1;
  for (int i = 0; i < t->ncp; i++) {
    world_detector(i, &d);
    if (d.order != last) target++, last = d.order, F.dist[target] = malloc(n * 2), k = 0;
    /* (alternatives share a map: the nearest box) */
    if (k == 0) bfs(F.dist[target], d.lo, d.hi);
    else {
      uint16_t *tmp = malloc(n * 2);
      bfs(tmp, d.lo, d.hi);
      for (size_t j = 0; j < n; j++)
        if (tmp[j] < F.dist[target][j]) F.dist[target][j] = tmp[j];
      free(tmp);
    }
    k++;
  }
  F.dist[no] = malloc(n * 2);
  for (int i = 0; i < t->nfin; i++) {
    world_detector(t->ncp + i, &d);
    if (i == 0) bfs(F.dist[no], d.lo, d.hi);
    else {
      uint16_t *tmp = malloc(n * 2);
      bfs(tmp, d.lo, d.hi);
      for (size_t j = 0; j < n; j++)
        if (tmp[j] < F.dist[no][j]) F.dist[no][j] = tmp[j];
      free(tmp);
    }
  }
}

static void free_field(void) {
  free(F.walk);
  free(F.cut);
  free(F.wall);
  for (int i = 0; i < F.ntarget; i++) free(F.dist[i]);
}

/* the car's cube on the map: a metre up from it (its own up), else two, else just above it; -1 when off the map */
static int car_vox(const car_t *c, int k) {
  static const float UP[3] = {1.0f, 2.0f, 0.4f};
  v3 up = mcolumn(&c->basis, 1);
  for (int i = 0; i < 3; i++) {
    int x, y, z;
    if (!vox_of(vadd(c->pos, vmul(up, UP[i])), &x, &y, &z)) continue;
    int v = vidx(x, y, z);
    if (F.dist[k][v] != INF) return v;
  }
  return -1;
}

/* how far the car is from its next target (in map units, about metres; 1e5 when lost) */
static float dist_of(const car_t *c) {
  int k = c->next_cp;
  if (k >= F.ntarget) return 0.0f;
  int v = car_vox(c, k);
  return v < 0 ? 1e5f : (float)F.dist[k][v];
}

static double score(const car_t *c) {
  if (c->finish) return 1e12 - (double)c->finish;
  double s = (double)c->next_cp * 1e7 - (double)dist_of(c) * 1000.0;
  s += (double)pt_absf(c->speed) * 2.0;
  if (c->pos.y < -3.0f) s -= 1e6;
  return s;
}

/* ------------------------------------------------------------------ a driver that follows the map */
/* the place L cubes further down the map from the car (towards target k) */
static v3 ahead(int k, const car_t *c, int L) {
  int v = car_vox(c, k);
  if (v < 0) return c->pos;
  int x = v % F.nx, y = (v / F.nx) % F.ny, z = v / (F.nx * F.ny);
  uint16_t best = F.dist[k][v];
  for (int s = 0; s < L && best > 0; s++) {
    int nx = x, ny = y, nz = z;
    for (int dz = -1; dz <= 1; dz++)
      for (int dy = -1; dy <= 1; dy++)
        for (int dx = -1; dx <= 1; dx++) {
          int a = x + dx, b = y + dy, cc = z + dz;
          if (a < 0 || b < 0 || cc < 0 || a >= F.nx || b >= F.ny || cc >= F.nz) continue;
          uint16_t d = F.dist[k][vidx(a, b, cc)];
          if (d < best) best = d, nx = a, ny = b, nz = cc;
        }
    if (nx == x && ny == y && nz == z) break;
    x = nx, y = ny, z = nz;
  }
  return V3(F.x0 + ((float)x + 0.5f) * VOX, F.y0 + ((float)y + 0.5f) * VOX, F.z0 + ((float)z + 0.5f) * VOX);
}

/* the keys for the next 10 ms: steer towards the place L cubes ahead, gas up to vmax km/h */
static unsigned pilot(const car_t *c, int L, float vmax) {
  int k = c->next_cp < F.ntarget ? c->next_cp : F.ntarget - 1;
  v3 d = mcol(&c->basis, vsub(ahead(k, c, L), c->pos));
  float ang = pt_atan2f(d.x, d.z);
  float want = pt_clampf(ang * 2.5f, -1.0f, 1.0f);
  unsigned keys = 0;
  if (want > c->steer_in + 0.05f) keys |= IN_LEFT;
  else if (want < c->steer_in - 0.05f) keys |= IN_RIGHT;
  else if (want > 0.3f) keys |= IN_LEFT;
  else if (want < -0.3f) keys |= IN_RIGHT;
  if (c->speed < vmax) keys |= IN_UP;
  else if (c->speed > vmax + 20.0f) keys |= IN_DOWN;
  return keys;
}

/* ------------------------------------------------------------------ the search */
#define SLOT 10                     /* ms a key is held, at least */
#define STEP 10                     /* slots a try lasts */
#define MAXN 8000000
typedef struct {
  int32_t parent;
  uint8_t keys[STEP];
} node_t;
static node_t *nodes;
static int nnodes;
typedef struct {
  car_t car;
  int32_t node;
} beam_t;
static beam_t beam[1];

/* the tries: the driver looking near, far or between, at full speed or slower; and keys held */
typedef struct { int L; float vmax; uint8_t keys; } action_t;
static const action_t ACTIONS[] = {
  {4, 999.0f, 0}, {7, 999.0f, 0}, {11, 999.0f, 0}, {16, 999.0f, 0}, {22, 999.0f, 0}, {8, 60.0f, 0}, {10, 110.0f, 0},
  {0, 0.0f, IN_UP}, {0, 0.0f, IN_UP | IN_LEFT}, {0, 0.0f, IN_UP | IN_RIGHT}, {0, 0.0f, 0}, {0, 0.0f, IN_DOWN},
  {0, 0.0f, IN_DOWN | IN_LEFT}, {0, 0.0f, IN_DOWN | IN_RIGHT},
};
#define NACT ((int)(sizeof ACTIONS / sizeof ACTIONS[0]))

/* a try: STEP slots; writes the keys used */
static void act(car_t *c, const action_t *a, uint8_t *keys) {
  for (int s = 0; s < STEP; s++) {
    unsigned k = a->L ? pilot(c, a->L, a->vmax) : a->keys;
    if (keys) keys[s] = (uint8_t)k;
    for (int i = 0; i < SLOT && !c->finish; i++) car_step(c, k);
  }
}

/* the archive */
#define MAXC 30000
#define TEMP 25.0
static double wcum[MAXC];
#define HASH 65536
static int32_t slot[HASH];          /* cell + 1 by key */
static int wn;
static double pt_expd(double x) {
  /* e^x for x <= 0, near enough */
  double r = 1.0 + x / 1024.0;
  for (int i = 0; i < 10; i++) r *= r;
  return r;
}
static double sqrt_d(double x) {
  double r = x > 1.0 ? x : 1.0;
  for (int i = 0; i < 30; i++) r = 0.5 * (r + x / r);
  return r;
}
typedef struct {
  car_t car;
  int32_t node;
  int32_t tries;
  uint32_t key;
  uint32_t frames;
} cell_t;
static uint32_t cell_key(const car_t *c) {
  int d = (int)(dist_of(c) / 4.0f);
  return (uint32_t)c->next_cp << 16 | (uint32_t)(d < 65535 ? d : 65535);
}
static double progress(const cell_t *e) { return (double)(e->key >> 16) * 1e6 - (double)(e->key & 0xFFFF); }
static uint32_t lcg(uint32_t *s) { return *s = *s * 1103515245u + 12345u, *s >> 8; }

static bool route(int t, FILE *out) {
  world_load(t);
  static car_t start;
  car_reset(&start);
  build_field();
  build_targets();
  if (getenv("TRACE")) {
    /* the way down the map from a point: TRACE="target x y z" */
    int k;
    float px, py, pz;
    sscanf(getenv("TRACE"), "%d %f %f %f", &k, &px, &py, &pz);
    int x, y, z;
    vox_of(V3(px, py, pz), &x, &y, &z);
    for (int step = 0; step < 400; step++) {
      int i = vidx(x, y, z);
      uint16_t best = F.dist[k][i];
      int bx = x, by = y, bz = z;
      for (int dz = -1; dz <= 1; dz++)
        for (int dy = -1; dy <= 1; dy++)
          for (int dx = -1; dx <= 1; dx++) {
            int a = x + dx, b = y + dy, c = z + dz;
            if (a < 0 || b < 0 || c < 0 || a >= F.nx || b >= F.ny || c >= F.nz) continue;
            uint16_t v = F.dist[k][vidx(a, b, c)];
            if (v < best) best = v, bx = a, by = b, bz = c;
          }
      if (step % 5 == 0 || best == 0)
        fprintf(stderr, "  %3d: %.0f %.0f %.0f dist %u walk %d\n", step, (double)(F.x0 + x * VOX), (double)(F.y0 + y * VOX),
                (double)(F.z0 + z * VOX), F.dist[k][i], F.walk[i]);
      if (best == 0 || (bx == x && by == y && bz == z)) break;
      x = bx, y = by, z = bz;
    }
    return false;
  }
  if (getenv("FIELD_DUMP")) {
    int k = atoi(getenv("FIELD_DUMP"));
    FILE *f = fopen("field.ppm", "wb");
    fprintf(f, "P6 %d %d 255\n", F.nx, F.nz);
    for (int z = 0; z < F.nz; z++)
      for (int x = 0; x < F.nx; x++) {
        uint16_t m = INF;
        int wy = 0;
        for (int y = 0; y < F.ny; y++) {
          uint16_t v = F.dist[k][vidx(x, y, z)];
          if (v < m) m = v, wy = y;
        }
        int walkable = 0;
        for (int y = 0; y < F.ny; y++) walkable |= F.walk[vidx(x, y, z)] == 2 && F.y0 + y * VOX > 4.0f;
        int c = m == INF ? 0 : 255 - (m / 8) % 200;
        fputc(m == INF ? 80 : c, f), fputc(m == INF ? 0 : (walkable ? 255 : c / 2), f), fputc(m == INF ? 0 : wy * 4 % 256, f);
      }
    fclose(f);
    fprintf(stderr, "field %d: origin %.0f %.0f size %d x %d\n", k, (double)F.x0, (double)F.z0, F.nx, F.nz);
    return false;
  }
  if (getenv("PILOT")) {
    /* the driver alone: PILOT="L vmax" */
    int L = 12;
    float vmax = 999.0f;
    sscanf(getenv("PILOT"), "%d %f", &L, &vmax);
    static car_t c;
    c = start;
    int until = getenv("UNTIL") ? atoi(getenv("UNTIL")) : 120000;
    for (int f = 0; f < until && !c.finish; f += SLOT) {
      unsigned k = pilot(&c, L, vmax);
      for (int i = 0; i < SLOT; i++) car_step(&c, k);
      if (getenv("PV2") && f >= atoi(getenv("PV2")) && f < atoi(getenv("PV2")) + 500)
        fprintf(stderr, "    f %d pos %.2f %.2f %.2f vel %.1f %.1f %.1f cache %d imp %.0f wheels %d%d%d%d up %.2f %.2f %.2f\n", f,
                (double)c.pos.x, (double)c.pos.y, (double)c.pos.z, (double)c.vel.x, (double)c.vel.y, (double)c.vel.z, c.ncache,
                (double)c.impulse, c.w[0].contact, c.w[1].contact, c.w[2].contact, c.w[3].contact, (double)c.basis.r[0].y,
                (double)c.basis.r[1].y, (double)c.basis.r[2].y);
      if (getenv("PV") && f < atoi(getenv("PV")) && f % 50 == 0) {
        v3 tg = ahead(c.next_cp, &c, L);
        v3 d = mcol(&c.basis, vsub(tg, c.pos));
        v3 fw = mcolumn(&c.basis, 2);
        fprintf(stderr, "    f %d pos %.1f %.1f fwd %.2f %.2f tgt %.1f %.1f local %.1f %.1f keys %u steer %.2f spd %.0f\n", f,
                (double)c.pos.x, (double)c.pos.z, (double)fw.x, (double)fw.z, (double)tg.x, (double)tg.z, (double)d.x,
                (double)d.z, k, (double)c.steer_in, (double)c.speed);
      }
      if (f % 1000 == 0)
        fprintf(stderr, "  %3d s: %7.1f %5.1f %7.1f cp %d dist %.0f speed %.0f\n", f / 1000, (double)c.pos.x, (double)c.pos.y,
                (double)c.pos.z, c.next_cp, (double)dist_of(&c), (double)c.speed);
    }
    fprintf(stderr, "  finish %u; up %.2f %.2f %.2f fwd %.2f %.2f %.2f wheels %d%d%d%d\n", c.finish, (double)c.basis.r[0].y,
            (double)c.basis.r[1].y, (double)c.basis.r[2].y, (double)c.basis.r[0].z, (double)c.basis.r[1].z,
            (double)c.basis.r[2].z, c.w[0].contact, c.w[1].contact, c.w[2].contact, c.w[3].contact);
    free_field();
    return c.finish != 0;
  }
  /* Go-Explore: an archive of cars, one per stretch of the way (the next checkpoint, the distance left in 4 m
     steps), the quickest there kept; again and again a car is taken from near the front, the less tried first, and
     driven on a while with the driver or keys held; new stretches go in the archive */
  nodes = malloc(sizeof(node_t) * MAXN);
  nnodes = 1;
  nodes[0].parent = -1;
  int ncell = 0;
  static cell_t cells[MAXC];
  memset(slot, 0, sizeof slot);
  cells[ncell++] = (cell_t){start, 0, 0, cell_key(&start), 0};
  {
    uint32_t h = (cells[0].key * 2654435761u) & (HASH - 1);
    slot[h] = 1;
  }
  int found = -1;
  uint32_t seed = 12345u + (uint32_t)t * 7919u;
  int tries = getenv("TRIES") ? atoi(getenv("TRIES")) : 200000;
  int best = 0;
  for (int it = 0; it < tries && found < 0 && nnodes + 64 < MAXN; it++) {
    /* the car to go on from: weighted by how far on it is (a factor e for each TEMP metres) over the root of how
       often it was tried, so that when the front gets nowhere the cars behind it get their turn */
    if (it % 64 == 0) {
      double top = -1e30;
      for (int i = 0; i < ncell; i++)
        if (progress(&cells[i]) > top) top = progress(&cells[i]);
      double acc = 0.0;
      for (int i = 0; i < ncell; i++) {
        double x = (progress(&cells[i]) - top) / TEMP;
        acc += x < -30.0 ? 0.0 : pt_expd(x) / sqrt_d(1.0 + cells[i].tries);
        wcum[i] = acc;
      }
      wn = ncell;
    }
    double r = (double)(lcg(&seed) % 1000000u) / 1000000.0 * wcum[wn - 1];
    int lo = 0, hi = wn - 1;
    while (lo < hi) {
      int mid = (lo + hi) / 2;
      if (wcum[mid] < r) lo = mid + 1;
      else hi = mid;
    }
    int pick = lo;
    cells[pick].tries++;
    static car_t c;
    c = cells[pick].car;
    car_cache_rebuild(&c);
    int node = cells[pick].node;
    int steps = 5 + (int)(lcg(&seed) % 26u), a = (int)(lcg(&seed) % (uint32_t)NACT), lost = 0;
    for (int s = 0; s < steps && !c.finish; s++) {
      if (lcg(&seed) % 10 >= 6) {
        /* (the driver more often than held keys) */
        a = (int)(lcg(&seed) % (uint32_t)(NACT + 7));
        if (a >= NACT) a = a - NACT;
      }
      nodes[nnodes].parent = node;
      act(&c, &ACTIONS[a], nodes[nnodes].keys);
      node = nnodes++;
      if (node == 1 && !(nodes[1].keys[0] & (IN_UP | IN_DOWN | IN_LEFT | IN_RIGHT))) break;   /* (the first keys start the clock) */
      if (c.finish) {
        found = node;
        cells[0].car = c, cells[0].node = node;
        best = 0;
        break;
      }
      float d = dist_of(&c);
      if (c.pos.y < -3.0f) break;
      if (d >= 1e5f) {
        /* (off the map, high in the air say: on, but not kept) */
        if (++lost > 30) break;
        continue;
      }
      lost = 0;
      uint32_t key = cell_key(&c);
      uint32_t h = (key * 2654435761u) & (HASH - 1);
      while (slot[h] && cells[slot[h] - 1].key != key) h = (h + 1) & (HASH - 1);
      int k = slot[h] ? slot[h] - 1 : -1;
      if (k < 0 && ncell < MAXC) {
        k = ncell++, cells[k].tries = 0, cells[k].key = key, cells[k].frames = 0xFFFFFFFFu;
        slot[h] = k + 1;
      }
      if (k >= 0 && c.frames < cells[k].frames) {
        cells[k].car = c, cells[k].node = node, cells[k].frames = c.frames;
        if (progress(&cells[k]) > progress(&cells[best])) best = k;
      }
    }
    if (it % 10000 == 0)
      fprintf(stderr, "  %s try %5d: %d stretches, furthest cp %d/%d dist %.0f at %.1f s\n", pt_tracks[t].name, it, ncell,
              cells[best].car.next_cp, F.ntarget - 1, (double)dist_of(&cells[best].car),
              (double)cells[best].car.frames / 1000.0);
  }
  beam[0].car = found >= 0 ? cells[0].car : cells[best].car;
  beam[0].node = found >= 0 ? found : cells[best].node;
  bool ok = found >= 0;
  if (ok || getenv("PARTIAL")) {
    /* the keys, from the start, a letter per slot */
    int n = 0;
    for (int i = beam[0].node; i > 0; i = nodes[i].parent) n++;
    int ns = n * STEP;
    uint8_t *k = malloc((size_t)ns);
    int j = n;
    for (int i = beam[0].node; i > 0; i = nodes[i].parent) memcpy(&k[--j * STEP], nodes[i].keys, STEP);
    /* (the same keys from the start must come to the same car) */
    static car_t chk;
    chk = start;
    chk.cache_ok = 0;
    for (int i = 0; i < ns && !chk.finish; i++)
      for (int f = 0; f < SLOT && !chk.finish; f++) car_step(&chk, k[i]);
    fprintf(stderr, "  replay: %08x %.3f %.3f %.3f, search: %08x %.3f %.3f %.3f\n", car_hash(&chk), (double)chk.pos.x,
            (double)chk.pos.y, (double)chk.pos.z, car_hash(&beam[0].car), (double)beam[0].car.pos.x,
            (double)beam[0].car.pos.y, (double)beam[0].car.pos.z);
    /* (to the finish only) */
    if (ok) ns = (int)((beam[0].car.finish + SLOT - 1) / SLOT);
    fprintf(out, "  /* %s: %u ms%s */\n  \"", pt_tracks[t].name, beam[0].car.finish, ok ? "" : " (not finished)");
    int col = 3;
    for (int i = 0; i < ns;) {
      int r = 1;
      while (i + r < ns && k[i + r] == k[i]) r++;
      char buf[16];
      int len = r > 1 ? snprintf(buf, sizeof buf, "%c%d", (k[i] & IN_RESET ? 'A' : 'a') + (k[i] & 15), r)
                      : snprintf(buf, sizeof buf, "%c", (k[i] & IN_RESET ? 'A' : 'a') + (k[i] & 15));
      if (col + len > 116) fprintf(out, "\"\n  \""), col = 3;
      fputs(buf, out), col += len;
      i += r;
    }
    fprintf(out, "\",\n");
    fflush(out);
    free(k);
  }
  free(nodes);
  free_field();
  return ok;
}

int main(int argc, char **argv) {
  if (getenv("VC")) vcost = atoi(getenv("VC"));
  if (getenv("AU")) air_up = atoi(getenv("AU"));
  int list[PT_NTRACKS], n = 0;
  for (int i = 1; i < argc; i++) list[n++] = atoi(argv[i]);
  if (!n)
    for (int i = 0; i < PT_NTRACKS; i++) list[n++] = i;
  for (int i = 0; i < n; i++) {
    bool ok = route(list[i], stdout);
    fprintf(stderr, "%s: %s\n", pt_tracks[list[i]].name, ok ? "finished" : "NOT FOUND");
  }
  return 0;
}
