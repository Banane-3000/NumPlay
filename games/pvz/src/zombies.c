/* The zombies: how each kind walks, eats, jumps, digs, flies and dies (game.h). */
#include <string.h>
#include "game.h"
#include "save.h"

/* ---------------------------------------------------------------- facts */

static int base_health(int kind) {
  switch (kind) {
  case Z_GARGANTUAR: return 3000;
  case Z_CATAPULT: return 850;
  case Z_BUNGEE: return 450;
  case Z_ZOMBONI: return 1350;
  case Z_POLE: case Z_DANCER: case Z_DOLPHIN: case Z_JACK: case Z_LADDER: return 500;
  default: return 270;
  }
}

bool zombie_headless(const Zombie *z) {
  if (z->kind == Z_ZOMBONI || z->kind == Z_CATAPULT || z->kind == Z_GARGANTUAR || z->kind == Z_BUNGEE) return false;
  int full = z->kind == Z_POLE || z->kind == Z_DANCER || z->kind == Z_DOLPHIN || z->kind == Z_JACK || z->kind == Z_LADDER
                 ? 500 : 270;
  if (z->flags & ZF_MINI) full /= 4;
  return z->health < full / 3;
}

bool zombie_flying(const Zombie *z) { return z->kind == Z_BALLOON && ((z->flags & ZF_FLYING) || z->pop > 0); }

bool zombie_submerged(const Zombie *z) {
  return z->kind == Z_IMP ? z->u.flight.left > 0 || z->landing > 0
       : (z->flags & ZF_UNDERGROUND) || z->emerge > 0 || (z->kind == Z_BUNGEE && z->phase != BP_WAIT) ||
         (z->kind == Z_SNORKEL && water_row(z->row) && z->x > 100 && z->x < 770 && !(z->flags & ZF_EATING));
}

int zombie_total_health(const Zombie *z) { return z->health + z->armor + z->shield; }

/* the ground its walk covers in a loop of its animation, and the loop's frames (the .reanim's _ground track) */
static void walk_track(const Zombie *z, float *dist, int *frames) {
  switch (z->kind) {
  case Z_GARGANTUAR: *dist = 124.1f, *frames = 49; return;
  case Z_IMP: *dist = 62.9f, *frames = 33; return;
  case Z_DANCER: case Z_BACKUP: *dist = 28.6f, *frames = 21; return;
  case Z_SNORKEL: *dist = 59.9f, *frames = 37; return;
  case Z_FOOTBALL: *dist = 30, *frames = 30; return;
  case Z_NEWSPAPER: *dist = 57.8f, *frames = 47; return;
  case Z_POLE: *dist = z->flags & ZF_POLE ? 30 : 67.8f, *frames = z->flags & ZF_POLE ? 37 : 45; return;
  default: *dist = 49.8f, *frames = 47;
  }
}

/* Zombie::UpdateAnimSpeed: the speed is its animation's, its feet keep to the ground */
float walking_speed(const Zombie *z) {
  if (z->kind == Z_ZOMBONI || z->kind == Z_CATAPULT || (z->kind == Z_BOBSLED && z->u.sled.sled) || (z->flags & ZF_FLYING))
    return z->speed;
  float d;
  int f;
  walk_track(z, &d, &f);
  return z->speed * 0.47f * f / (f - 1);
}

/* ---------------------------------------------------------------- arriving */

Zombie *spawn_zombie(int kind, int row, float x, int wave) {
  if (B.nzombies >= MAX_ZOMBIES) return NULL;
  /* the rake: the first zombie of its lane steps on it */
  if (B.rake.on == 1 && x >= 780 && kind != Z_BUNGEE && kind != Z_SNORKEL && kind != Z_DOLPHIN && kind != Z_DUCKY &&
      !water_row(B.rake.row)) {
    row = B.rake.row, B.rake.on = 2;
  }
  Zombie *z = &B.zombies[B.nzombies++];
  memset(z, 0, sizeof *z);
  z->id = new_id(), z->kind = (uint8_t)kind, z->row = (uint8_t)row, z->x = x, z->wave = (uint8_t)wave;
  z->armor = (int16_t)(kind == Z_DIGGER ? 100 : kind == Z_FOOTBALL ? 1400 : kind == Z_CONE ? 370 : kind == Z_BUCKET ? 1100 : 0);
  z->shield = (int16_t)(kind == Z_LADDER ? 500 : kind == Z_NEWSPAPER ? 150 : kind == Z_DOOR ? 1100 : 0);
  z->health = (int16_t)base_health(kind);
  switch (kind) {
  case Z_POGO: z->speed = 0.45f; break;
  case Z_LADDER: z->speed = 0.79f + frand() * 0.02f; break;
  case Z_DIGGER: z->speed = 0.66f + frand() * 0.02f; break;
  case Z_DOLPHIN: z->speed = 0.89f + frand() * 0.02f; break;
  case Z_BOBSLED: z->speed = 0.6f; break;
  case Z_POLE: case Z_FOOTBALL: case Z_SNORKEL: case Z_JACK: z->speed = 0.66f + frand() * 0.02f; break;
  case Z_DANCER: case Z_BACKUP: case Z_FLAG: z->speed = 0.45f; break;
  default: z->speed = 0.23f + frand() * 0.14f;
  }
  z->bite = 4;
  z->hit_at = (uint32_t)-1000;
  if (B.level == 25 && B.mode == MODE_CONVEYOR) {
    z->flags |= ZF_MINI;
    z->health /= 4, z->armor /= 4, z->shield /= 4;
  }
  if (kind == Z_POLE) z->flags |= ZF_POLE;
  if (kind == Z_DANCER) z->phase = DP_INTRO, z->u.dance.dance_in = (int16_t)(300 + irand(12));
  if (kind == Z_DIGGER) z->flags |= ZF_UNDERGROUND;
  if (kind == Z_POGO) z->flags |= ZF_POGO;
  if (kind == Z_CATAPULT) z->u.catapult.ammo = 20, z->phase = CP_WALK;
  if (kind == Z_GARGANTUAR) z->phase = GP_WALK;
  if (kind == Z_DOLPHIN) z->flags |= ZF_DOLPHIN;
  if (kind == Z_BALLOON) z->flags |= ZF_FLYING;
  if (kind == Z_BUNGEE) {
    /* a plant no other bungee is after, or anywhere in the back */
    Plant *targets[MAX_PLANTS];
    int n = 0;
    for (int i = 0; i < B.nplants; i++) {
      Plant *p = &B.plants[i];
      if (p->health <= 0) continue;
      bool taken = false;
      for (int k = 0; k < B.nzombies - 1; k++) {
        const Zombie *o = &B.zombies[k];
        taken |= o->kind == Z_BUNGEE && o->u.bungee.column == p->col && o->row == p->row;
      }
      if (!taken) targets[n++] = p;
    }
    int col;
    if (n) {
      Plant *t = targets[irand(n)];
      col = t->col, z->row = t->row;
    } else
      col = 4 + irand(5);
    z->x = cell_x(col), z->phase = BP_DOWN, z->u.bungee.left = 375, z->u.bungee.column = (uint8_t)col;
    z->u.bungee.drop = 0xFF;
  }
  if (kind == Z_JACK) {
    float dist = (450 + irand(300)) / (frand() < 0.05f ? 3.0f : 1.0f);
    z->u.jack.in = (int16_t)(B.mode == MODE_VASE ? 10 : (int)(dist / z->speed) * 2);
  }
  return z;
}

/* a bungee zombie brings a zombie down from the sky (Bungee Blitz, the roof's last wave) */
void drop_from_sky(int kind, int row, int wave) {
  int col = 4 + irand(5);
  Zombie *z = spawn_zombie(Z_BUNGEE, row, cell_x(col), wave);
  if (!z) return;
  z->row = (uint8_t)row, z->x = cell_x(col), z->phase = BP_DOWN, z->u.bungee.left = 375, z->u.bungee.column = (uint8_t)col;
  z->u.bungee.drop = (uint8_t)kind;
}

/* ---------------------------------------------------------------- hurting */

void damage_zombie(Zombie *z, int amount, bool through_shield) {
  if (!through_shield && z->shield > 0) {
    int a = z->shield < amount ? z->shield : amount;
    z->shield = (int16_t)(z->shield - a), amount -= a;
    if (z->kind == Z_LADDER && z->shield == 0) z->ladder_in = 0, z->speed = 0.3f;
    if (z->kind == Z_NEWSPAPER && z->shield == 0) {
      z->rage = 150, z->flags &= (uint16_t)~ZF_EATING, z->flags |= ZF_PAPER_GONE;
    }
  }
  int a = z->armor < amount ? z->armor : amount;
  z->armor = (int16_t)(z->armor - a);
  int hp = z->health - (amount - a);
  z->health = (int16_t)(hp < -30000 ? -30000 : hp);
}

/* gone: a corpse to watch, a coin perhaps, and the place the level's reward drops */
void kill_zombie(Zombie *z, int how) {
  if (z->health > 0) z->health = 0;
  if (z->flags & ZF_SPAWNED) return;   /* (already counted) */
  z->flags |= ZF_SPAWNED;
  float y = row_y(z->row, z->x);
  if (!(z->flags & ZF_HYPNO)) B.last_x = z->x, B.last_y = y;
  if (how == DIE_BURN || how == DIE_NORMAL || how == DIE_MOWED) {
    Fx *f = fx_corpse(z, how == DIE_BURN ? 1 : 0, how == DIE_BURN ? 150 : 140, how == DIE_BURN);
    if (f && how == DIE_MOWED) f->flags |= 8;
  }
  /* coins, from the night on (Zombie::DropLoot) */
  if (!(z->flags & ZF_HYPNO) && B.mode != MODE_BOWLING && (B.level >= 11 || progress.finished) && B.mode != MODE_VASE) {
    int hit = B.level == 11 && !B.first_coin && B.wave > 5 && !progress.finished ? 1000 : irand(30000);
    int factor = zombie_defs[z->kind].value;
    if (hit < 2500 * factor) {
      add_coin(z->x, y - 15, hit < 14 * factor && progress.slots > 6 ? 1000 : hit < 250 * factor ? 50 : 10);
      B.first_coin = 1;
    }
  }
}

/* ---------------------------------------------------------------- the kinds' own ways */

static void eat(Zombie *z, Plant *food);

static void update_zomboni(Zombie *z) {
  if (z->health <= 200) {
    kill_zombie(z, DIE_NORMAL);
    return;
  }
  float f = (z->x - 30 - 300) / 400;
  z->speed = 0.05f + 0.2f * (f < 0 ? 0 : f > 1 ? 1 : f);
  z->x -= z->speed, z->flags &= (uint16_t)~ZF_EATING;
  float x = z->x + 88 < 25 ? 25 : z->x + 88;
  if (x < 800) {
    Ice *ice = &B.ice[z->row];
    if (ice->left > 0) ice->x = ice->x < x ? ice->x : x;
    else ice->x = x;
    ice->left = 3000;
  }
  for (int i = 0; i < B.nplants; i++) {
    Plant *p = &B.plants[i];
    if (p->health <= 0 || p->row != z->row || cell_x(p->col) - z->x >= 55 || z->x - cell_x(p->col) >= 55) continue;
    p->health = 0;
    if (p->kind == P_SPIKEWEED) {
      kill_zombie(z, DIE_NORMAL);
      fx_add(FX_POW, z->x, row_y(z->row, z->x) - 30, 20, 0);
      break;
    }
  }
}

static void crash_sled(uint16_t sled) {
  for (int i = 0; i < B.nzombies; i++) {
    Zombie *o = &B.zombies[i];
    if (o->kind != Z_BOBSLED || o->u.sled.sled != sled) continue;
    o->u.sled.sled = 0, o->u.sled.leader = 0, o->armor = 0, o->crash = 150, o->speed = 0.23f + frand() * 0.14f;
  }
}

static void update_sled(Zombie *z) {
  if (z->crash > 0) {
    z->crash--;
    return;
  }
  Zombie *leader = z->u.sled.leader ? zombie_by_id(z->u.sled.leader) : z;
  Ice *ice = &B.ice[z->row];
  if (!leader || leader->health <= 0 || ice->left <= 0 || leader->x < ice->x || leader->armor == 0) {
    crash_sled(z->u.sled.sled);
    return;
  }
  z->x -= 0.6f;
  if (ice->left < 500) ice->left = 500;
  for (int i = 0; i < B.nplants; i++) {
    Plant *p = &B.plants[i];
    if (p->health <= 0 || p->row != z->row || cell_x(p->col) - z->x >= 45 || z->x - cell_x(p->col) >= 45) continue;
    p->health = 0;
    if (p->kind == P_SPIKEWEED) {
      crash_sled(z->u.sled.sled);
      break;
    }
  }
}

static bool update_jack(Zombie *z) {
  if (--z->u.jack.in > 0) return (z->flags & ZF_OPENING) != 0;
  if (!(z->flags & ZF_OPENING)) {
    z->flags |= ZF_OPENING, z->u.jack.in = 110, z->flags &= (uint16_t)~ZF_EATING, z->anim = 0;
    return true;
  }
  /* the box goes off */
  bool hyp = (z->flags & ZF_HYPNO) != 0;
  for (int i = 0; i < B.nplants && !hyp; i++) {
    Plant *p = &B.plants[i];
    float dx = cell_x(p->col) - z->x, dy = row_y(p->row, cell_x(p->col)) - row_y(z->row, z->x);
    if (dx * dx + dy * dy < 90 * 90) p->health = 0;
  }
  for (int i = 0; i < B.nzombies; i++) {
    Zombie *o = &B.zombies[i];
    if (!(o->flags & ZF_HYPNO) != hyp && o->row - z->row <= 1 && z->row - o->row <= 1 && o->x - z->x < 115 &&
        z->x - o->x < 115) {
      damage_zombie(o, 1800, true);
      if (o->health <= 0) kill_zombie(o, DIE_BURN);
    }
  }
  if (B.mode == MODE_VASE)
    for (int i = 0; i < B.nvases; i++) {
      int dc = B.vases[i].col - col_at(z->x), dr = B.vases[i].row - z->row;
      if (dc >= -1 && dc <= 1 && dr >= -1 && dr <= 1) board_break_vase(B.vases[i].col, B.vases[i].row), i = -1;
    }
  fx_add(FX_BOOM, z->x, row_y(z->row, z->x) - 35, 60, 0);
  kill_zombie(z, DIE_GONE);
  return true;
}

static bool dancer_moving(const Zombie *z) {
  if (z->kind != Z_DANCER && z->kind != Z_BACKUP) return true;
  if (z->kind == Z_DANCER && z->phase == DP_INTRO) return true;
  if ((B.ticks / 20) % 23 > 11) return false;
  const Zombie *leader = z->kind == Z_DANCER ? z : zombie_by_id(z->u.dance.leader);
  for (int i = 0; i < B.nzombies; i++) {
    const Zombie *o = &B.zombies[i];
    if (o->health <= 0 || (o->kind != Z_DANCER && o->kind != Z_BACKUP)) continue;
    bool group = o == leader || (leader && o->kind == Z_BACKUP && o->u.dance.leader == leader->id);
    if (group && ((o->flags & ZF_EATING) || o->frozen > 0)) return false;
  }
  return true;
}

/* the dancer calls his four backup dancers around him, again whenever one is missing */
static bool update_dancer(Zombie *z) {
  if (z->flags & ZF_EATING) return false;
  static const int8_t dr[4] = {-1, 1, 0, 0}, dx[4] = {0, 0, -100, 100};
  bool missing[4];
  int nmissing = 0;
  for (int s = 0; s < 4; s++) {
    int row = z->row + dr[s];
    missing[s] = false;
    if (row < 0 || row >= B.nrows || !(B.rows >> row & 1) || water_row(row)) continue;
    bool there = false;
    for (int i = 0; i < B.nzombies; i++) {
      const Zombie *o = &B.zombies[i];
      there |= o->kind == Z_BACKUP && o->u.dance.leader == z->id && o->u.dance.slot == s && o->health > 0;
    }
    if (!there) missing[s] = true, nmissing++;
  }
  if (z->phase == DP_INTRO) {
    if (--z->u.dance.dance_in <= 0) z->phase = DP_POINT, z->u.dance.dance_in = 38, z->anim = 0;
    return false;
  }
  if (z->phase == DP_POINT) {
    if (--z->u.dance.dance_in <= 0) {
      uint16_t id = z->id;
      int row = z->row, wave = z->wave;
      float x = z->x;
      bool hyp = (z->flags & ZF_HYPNO) != 0;
      for (int s = 0; s < 4; s++) {
        if (!missing[s]) continue;
        Zombie *f = spawn_zombie(Z_BACKUP, row + dr[s], x + dx[s], wave);
        if (!f) break;
        f->rise = 150, f->u.dance.leader = id, f->u.dance.slot = (uint8_t)s;
        if (hyp) f->flags |= ZF_HYPNO;
      }
      z = zombie_by_id(id);
      if (!z) return true;
      z->phase = DP_HOLD, z->u.dance.dance_in = 200;
    }
    return true;
  }
  if (z->phase == DP_HOLD) {
    if (--z->u.dance.dance_in <= 0) z->phase = DP_DANCE;
    return true;
  }
  if (nmissing) {
    if (z->u.dance.summon_in == 0) z->u.dance.summon_in = 100;
    else if (z->u.dance.summon_in > 1) z->u.dance.summon_in--;
    else if ((B.ticks / 20) % 23 == 12 && z->x < 730) {
      z->phase = DP_POINT, z->u.dance.dance_in = 38, z->u.dance.summon_in = 0, z->anim = 0;
      return true;
    }
  }
  return false;
}

static Plant *umbrella_near(int col, int row) {
  for (int i = 0; i < B.nplants; i++) {
    Plant *p = &B.plants[i];
    if (p->kind == P_UMBRELLA && p->health > 0 && p->col - col <= 1 && col - p->col <= 1 && p->row - row <= 1 &&
        row - p->row <= 1)
      return p;
  }
  return NULL;
}

static void update_bungee(Zombie *z) {
  if (--z->u.bungee.left > 0) return;
  if (z->phase == BP_DOWN) {
    Plant *u = umbrella_near(z->u.bungee.column, z->row);
    if (u) {
      u->shot = B.ticks;
      z->phase = BP_UP, z->u.bungee.left = 100;
      return;
    }
    if (z->u.bungee.drop != 0xFF) {
      spawn_zombie(z->u.bungee.drop, z->row, z->x, z->wave);
      z = &B.zombies[0] + (z - B.zombies);   /* (the array didn't move: spawning appends) */
      z->phase = BP_UP, z->u.bungee.left = 100;
    } else
      z->phase = BP_WAIT, z->u.bungee.left = 300;
  } else if (z->phase == BP_WAIT) {
    /* it takes the plant, its pot and lily pad last */
    Plant *victim = NULL, *pumpkin = NULL, *base = NULL;
    for (int i = 0; i < B.nplants; i++) {
      Plant *p = &B.plants[i];
      if (p->col != z->u.bungee.column || p->row != z->row || p->health <= 0) continue;
      if (p->kind == P_PUMPKIN) pumpkin = p;
      else if (p->kind == P_FLOWERPOT || p->kind == P_LILYPAD) base = p;
      else victim = p;
    }
    if (!victim) victim = pumpkin ? pumpkin : base;
    if (victim) victim->health = 0, victim->state = 0xEE;   /* (taken: no remains) */
    z->phase = BP_UP, z->u.bungee.left = 100;
  } else {
    z->health = 0, z->flags |= ZF_SPAWNED;   /* gone up, not killed */
  }
}

/* Zombie::UpdateZombieCatapult: the leftmost plant, 20 balls */
static Plant *catapult_target(const Zombie *z) {
  Plant *best = NULL;
  for (int i = 0; i < B.nplants; i++) {
    Plant *p = &B.plants[i];
    if (p->health <= 0 || p->row != z->row || cell_x(p->col) >= z->x - 100) continue;
    if (!best || p->col < best->col ||
        (p->col == best->col && ((best->kind == P_FLOWERPOT) > (p->kind == P_FLOWERPOT) ||
                                 (best->kind == P_PUMPKIN) > (p->kind == P_PUMPKIN))))
      best = p;
  }
  return best;
}

static void update_catapult(Zombie *z) {
  if (z->phase == CP_WALK) {
    z->flags &= (uint16_t)~ZF_EATING;
    if (z->x <= 650 && z->u.catapult.ammo > 0 && catapult_target(z)) {
      z->phase = CP_SHOOT, z->u.catapult.left = 300, z->anim = 0;
    } else {
      z->x -= z->speed;
      for (int i = 0; i < B.nplants; i++) {
        Plant *p = &B.plants[i];
        if (p->row == z->row && cell_x(p->col) - z->x < 40 && z->x - cell_x(p->col) < 40) p->health = 0;
      }
    }
    return;
  }
  z->u.catapult.left--;
  if (z->phase == CP_SHOOT && z->u.catapult.left == 230) {
    Plant *p = catapult_target(z);
    if (p && B.nshots < MAX_SHOTS) {
      Shot *s = &B.shots[B.nshots++];
      memset(s, 0, sizeof *s);
      s->id = new_id(), s->kind = S_BASKETBALL, s->row = z->row, s->torch = 0xFF;
      s->sx = z->x + 65, s->ex = cell_x(p->col), s->sy = row_y(z->row, s->sx) - 110, s->ey = row_y(z->row, s->ex) - 45;
      s->x = s->sx, s->y = s->sy, s->duration = 120, s->target = p->id;
    }
  }
  if (z->phase == CP_SHOOT && z->u.catapult.left == 171) z->u.catapult.ammo--, z->phase = CP_RELOAD;
  if (z->u.catapult.left <= 0) {
    z->phase = z->u.catapult.ammo > 0 && catapult_target(z) ? CP_SHOOT : CP_WALK;
    if (z->phase == CP_SHOOT) z->u.catapult.left = 300;
    z->anim = 0;
  }
}

static Plant *giant_food(const Zombie *z) {
  for (int i = 0; i < B.nplants; i++) {
    Plant *p = &B.plants[i];
    if (p->health > 0 && p->row == z->row && z->x - cell_x(p->col) < 55 && z->x - cell_x(p->col) > -30) return p;
  }
  return NULL;
}

static void update_giant(Zombie *z, float speed_factor) {
  if (z->phase == GP_WALK) {
    z->flags &= (uint16_t)~ZF_EATING;
    if (!z->u.giant.thrown && z->health < 1500 && z->x > 400) z->phase = GP_THROW, z->u.giant.left = 138, z->anim = 0;
    else if (giant_food(z)) z->phase = GP_SMASH, z->u.giant.left = 134, z->anim = 0;
    else {
      float v = walking_speed(z) * speed_factor;
      z->x -= v, z->walk += v;
    }
    return;
  }
  float before = z->u.giant.left;
  z->u.giant.left -= speed_factor;
  if (z->u.giant.left < 0) z->u.giant.left = 0;
  if (z->phase == GP_SMASH && before > 48 && z->u.giant.left <= 48) {
    Plant *p = giant_food(z);
    if (p) {
      int col = p->col, row = p->row;
      for (int i = 0; i < B.nplants; i++)
        if (B.plants[i].col == col && B.plants[i].row == row) B.plants[i].health = 0;
    }
    fx_add(FX_SMOKE, z->x - 40, row_y(z->row, z->x), 30, 0);
  }
  if (z->phase == GP_THROW && before > 36 && z->u.giant.left <= 36) {
    z->u.giant.thrown = 1;
    float dist = z->x - 360 - (B.env == ENV_ROOF ? 180 : 0);
    float lo = B.env == ENV_ROOF ? -140 : 40;
    if (dist < lo) dist = lo;
    if (dist > 140) dist -= frand() * 100;
    uint16_t id = z->id;
    float zx = z->x;
    int row = z->row, wave = z->wave;
    Zombie *imp = spawn_zombie(Z_IMP, row, zx - 133, wave);
    z = zombie_by_id(id);
    if (imp) {
      int dur = (int)((dist < 0 ? -dist : dist) / 3 + 0.5f);
      if (dur < 30) dur = 30;
      imp->u.flight.sx = imp->x, imp->u.flight.ex = imp->x - dist, imp->u.flight.duration = (int16_t)dur;
      imp->u.flight.left = (int16_t)dur;
    }
    if (!z) return;
  }
  if (z->u.giant.left <= 0) z->phase = GP_WALK, z->anim = 0;
}

/* ---------------------------------------------------------------- every tick */

static void eat(Zombie *z, Plant *food) {
  if (food->kind == P_HYPNOSHROOM && !sleeping(food)) {
    food->health = 0;
    z->flags |= ZF_HYPNO, z->speed = 0.17f, z->flags &= (uint16_t)~ZF_EATING;
    if (z->kind == Z_DANCER)
      for (int i = 0; i < B.nzombies; i++) {
        Zombie *f = &B.zombies[i];
        if (f->kind == Z_BACKUP && f->u.dance.leader == z->id) f->flags |= ZF_HYPNO, f->speed = 0.17f;
      }
    fx_add(FX_SMOKE, z->x, row_y(z->row, z->x) - 50, 40, 2);
  } else
    food->health = (int16_t)(food->health - 4);
  food->shot = B.ticks;   /* (a bitten plant flashes) */
  if (food->kind == P_GARLIC) {
    int rows[2], n = 0;
    for (int d = -1; d <= 1; d += 2) {
      int r = z->row + d;
      if (r >= 0 && r < B.nrows && (B.rows >> r & 1) && !water_row(r)) rows[n++] = r;
    }
    if (n) z->garlic_row = (uint8_t)rows[irand(n)], z->garlic = 100;
  }
  z->bite = (int8_t)(z->chill > 0 ? 8 : 4);
}

void zombies_tick(void) {
  bool hypnos = false;
  for (int i = 0; i < B.nzombies; i++) hypnos |= (B.zombies[i].flags & ZF_HYPNO) != 0;
  int n = B.nzombies;   /* (the ones spawned this tick wait for the next) */
  for (int i = 0; i < n && i < B.nzombies; i++) {
    Zombie *z = &B.zombies[i];
    if (z->health <= 0) continue;
    z->anim += 1;
    if (z->blown > 0) {
      z->x += 6;
      if (--z->blown == 0) kill_zombie(z, DIE_GONE);
      continue;
    }
    bool held = false;
    for (int k = 0; k < B.nplants; k++) held |= B.plants[k].kind == P_TANGLEKELP && B.plants[k].state && B.plants[k].target == z->id;
    if (held) {
      z->flags &= (uint16_t)~ZF_EATING;
      continue;
    }
    if (z->butter > 0) {
      z->butter--;
      continue;
    }
    if (z->chill > 0) z->chill--;
    if (z->frozen > 0) {
      z->frozen--;
      z->anim -= 1;
      continue;
    }
    float sf = z->chill > 0 ? 0.4f : 1;
    z->anim += sf - 1;
    if (z->rise > 0) {
      z->rise--, z->flags &= (uint16_t)~ZF_EATING;
      continue;
    }
    if (z->kind == Z_IMP && z->u.flight.left > 0) {
      z->u.flight.left--;
      float t = 1 - (float)z->u.flight.left / z->u.flight.duration;
      z->x = z->u.flight.sx + (z->u.flight.ex - z->u.flight.sx) * t;
      if (z->u.flight.left == 0) z->landing = 21;
      continue;
    }
    if (z->landing > 0) {
      z->landing--;
      continue;
    }
    if (z->kind == Z_CATAPULT) {
      update_catapult(z);
      continue;
    }
    if (z->kind == Z_GARGANTUAR) {
      update_giant(z, sf);
      continue;
    }
    if (z->kind == Z_BUNGEE) {
      update_bungee(z);
      continue;
    }
    if (z->flags & ZF_UNDERGROUND) {
      z->x -= z->speed * sf;
      if (z->x < 45) z->flags &= (uint16_t)~ZF_UNDERGROUND, z->emerge = 230, z->flags |= ZF_BACKWARDS, z->speed = 0.12f;
      continue;
    }
    if (z->emerge > 0) {
      z->emerge--;
      continue;
    }
    if (z->garlic > 0) {
      if (--z->garlic == 0) z->row = z->garlic_row, z->flags &= (uint16_t)~ZF_EATING;
      continue;
    }
    if (z->ladder_in > 0) {
      if (--z->ladder_in == 0) {
        if (B.nladders < 24) {
          int col = (int)((z->x - 35 - 80) / 80 + 0.5f);
          B.ladders[B.nladders].col = (uint8_t)col, B.ladders[B.nladders++].row = z->row;
        }
        z->shield = 0, z->speed = 0.3f;
      }
      continue;
    }
    if (z->kind == Z_ZOMBONI) {
      update_zomboni(z);
      continue;
    }
    if (z->kind == Z_BOBSLED && (z->u.sled.sled || z->crash)) {
      update_sled(z);
      continue;
    }
    if (z->kind == Z_JACK && !(z->flags & ZF_METAL_GONE) && !zombie_headless(z) && update_jack(z)) continue;
    if (z->health <= 0) continue;
    if (z->pop > 0) {
      if (--z->pop == 0 && water_row(z->row)) kill_zombie(z, DIE_GONE);
      continue;
    }
    if (z->flags & ZF_FLYING) {
      z->flags &= (uint16_t)~ZF_EATING;
      z->x -= z->speed * sf;
      continue;
    }
    if (z->rage > 0 && !zombie_headless(z)) {
      z->rage -= z->rage < sf ? z->rage : sf;
      z->flags &= (uint16_t)~ZF_EATING;
      if (z->rage <= 0) z->speed = 0.89f + frand() * 0.02f;
      continue;
    }
    if (zombie_headless(z)) {
      /* below a third of its health: it can't eat, falls apart as it goes, and can't get in the house */
      z->vault = 0, z->flags &= (uint16_t)~(ZF_POLE | ZF_EATING);
      float v = walking_speed(z) * sf;
      z->x += (z->flags & (ZF_HYPNO | ZF_BACKWARDS) ? 1 : -1) * v, z->walk += v;
      if (frand() < 0.2f) z->health--;
      if (z->flags & (ZF_HYPNO | ZF_BACKWARDS) ? z->x > 900 : z->x < 35) z->health = 0;
      if (z->health <= 0) kill_zombie(z, DIE_NORMAL);
      continue;
    }
    if (z->vault > 0) {
      float step = sf < z->vault ? sf : z->vault;
      z->vault -= step;
      z->x -= (z->kind == Z_DOLPHIN ? 150.0f / 120 : 150.0f / 168) * step;
      if (z->vault <= 0) z->speed = 0.23f + frand() * 0.14f;
      continue;
    }
    if (z->kind == Z_DOLPHIN && (z->flags & ZF_DOLPHIN) && z->x < 780) {
      Plant *barrier = NULL;
      for (int k = 0; k < B.nplants; k++) {
        Plant *p = &B.plants[k];
        if (p->health <= 0 || p->row != z->row || p->kind == P_TANGLEKELP) continue;
        if (z->x - cell_x(p->col) < 95 && z->x > cell_x(p->col) && (!barrier || p->kind == P_TALLNUT)) barrier = p;
      }
      if (barrier) {
        z->flags &= (uint16_t)~(ZF_DOLPHIN | ZF_EATING);
        if (barrier->kind == P_TALLNUT) z->x = cell_x(barrier->col) + 32, z->speed = 0.3f;
        else z->vault = 120, z->anim = 0;
      } else {
        z->x -= 0.9f * sf, z->walk += 0.9f * sf;
        continue;
      }
    }
    if (z->kind == Z_DANCER && update_dancer(z)) continue;
    z = &B.zombies[i];
    if (!(z->flags & ZF_HYPNO) && (z->flags & ZF_POLE)) {
      Plant *barrier = NULL;
      for (int k = 0; k < B.nplants; k++) {
        Plant *p = &B.plants[k];
        if (p->health <= 0 || p->row != z->row || p->kind == P_SPIKEWEED || (p->kind == P_POTATOMINE && p->action > 0)) continue;
        float d = z->x - cell_x(p->col);
        if (d < 95 && d > 0) {
          barrier = p;
          break;
        }
      }
      if (barrier) {
        z->flags &= (uint16_t)~(ZF_POLE | ZF_EATING);
        if (barrier->kind == P_TALLNUT) z->x = cell_x(barrier->col) + 32, z->speed = 0.3f;
        else z->vault = 168, z->anim = 0;
        continue;
      }
    }
    if ((z->flags & ZF_POGO) && !zombie_headless(z)) {
      Plant *tall = NULL;
      for (int k = 0; k < B.nplants; k++) {
        Plant *p = &B.plants[k];
        float d = z->x - cell_x(p->col);
        if (p->kind == P_TALLNUT && p->health > 0 && p->row == z->row && d > 0 && d < 65) tall = p;
      }
      if (tall) z->flags &= (uint16_t)~ZF_POGO, z->speed = 0.3f;
      else {
        z->flags &= (uint16_t)~ZF_EATING;
        z->x -= 0.45f * sf, z->walk += 0.45f * sf;
        continue;
      }
    }
    /* a zombie of the other side to fight */
    bool back = (z->flags & (ZF_HYPNO | ZF_BACKWARDS)) != 0;
    if (hypnos) {
      Zombie *rival = NULL;
      for (int k = 0; k < B.nzombies; k++) {
        Zombie *o = &B.zombies[k];
        if (o == z || !(o->flags & ZF_HYPNO) == !(z->flags & ZF_HYPNO) || o->health <= 0 || zombie_flying(o) ||
            zombie_headless(o) || o->row != z->row || o->rise > 0 || o->vault > 0)
          continue;
        float d = (o->x - z->x) * (back ? 1 : -1);
        if (d >= -30 && d < 40) {
          rival = o;
          break;
        }
      }
      if (rival) {
        z->flags |= ZF_EATING;
        if (--z->bite <= 0) {
          damage_zombie(rival, 4, true);
          rival->hit_at = B.ticks;
          if (rival->health <= 0) kill_zombie(rival, DIE_NORMAL);
          z->bite = (int8_t)(z->chill > 0 ? 8 : 4);
        }
        continue;
      }
    }
    /* a plant to eat: its pumpkin first, then the plant, then what it stands on */
    Plant *food = NULL;
    int prio = 3;
    if (!(z->flags & ZF_HYPNO))
      for (int k = 0; k < B.nplants; k++) {
        Plant *p = &B.plants[k];
        if (p->kind == P_TANGLEKELP || p->kind == P_SPIKEWEED || p->health <= 0 || p->row != z->row) continue;
        float d = (z->x - cell_x(p->col)) * (back ? -1 : 1);
        if (d >= 35 || d <= -30) continue;
        bool laddered = false;
        for (int l = 0; l < B.nladders; l++) laddered |= B.ladders[l].col == p->col && B.ladders[l].row == p->row;
        if (laddered) continue;
        int pr = p->kind == P_PUMPKIN ? 0 : p->kind == P_LILYPAD || p->kind == P_FLOWERPOT ? 2 : 1;
        if (pr < prio) {
          food = p, prio = pr;
          if (!pr) break;
        }
      }
    if (food) {
      if (!(z->flags & ZF_EATING)) z->anim = 0;
      z->flags |= ZF_EATING;
      if (z->kind == Z_LADDER && z->shield > 0 &&
          (food->kind == P_WALLNUT || food->kind == P_TALLNUT || food->kind == P_PUMPKIN)) {
        z->ladder_in = 100, z->x = cell_x(food->col) + 35, z->anim = 0;
        continue;
      }
      if (--z->bite <= 0) eat(z, food);
    } else {
      if (z->flags & ZF_EATING) z->anim = 0;
      z->flags &= (uint16_t)~ZF_EATING;
      if (dancer_moving(z)) {
        float v = walking_speed(z) * sf;
        z->x += (back ? 1 : -1) * v, z->walk += v;
      }
      z->bite = 4;
    }
  }
}
