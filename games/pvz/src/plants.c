/* The plants: what each one does, and what they shoot (game.h). */
#include <string.h>
#include "game.h"

/* ---------------------------------------------------------------- shooting */

static Shot *new_shot(int kind, int row, float x, float y) {
  if (B.nshots >= MAX_SHOTS) return NULL;
  Shot *s = &B.shots[B.nshots++];
  memset(s, 0, sizeof *s);
  s->id = new_id(), s->kind = (uint8_t)kind, s->row = (uint8_t)row, s->x = x, s->y = y, s->vx = 3.33f;
  s->torch = 0xFF;
  return s;
}

static bool targetable(const Zombie *z) {
  return !(z->flags & ZF_HYPNO) && z->health > 0 && z->rise == 0;
}

static void fire_pea(Plant *p, int row, bool force) {
  float x = cell_x(p->col);
  float range = p->kind == P_PUFFSHROOM || p->kind == P_SEASHROOM ? 250 : p->kind == P_FUMESHROOM ? 360 : 825 - x;
  if (p->kind == P_CACTUS) {
    bool air = false;
    for (int i = 0; i < B.nzombies; i++) {
      const Zombie *z = &B.zombies[i];
      air |= targetable(z) && (z->flags & ZF_FLYING) && z->row == row && z->x > x - 10 && z->x < 825;
    }
    if (air) p->flags |= PF_TALL;
    else p->flags &= (uint8_t)~PF_TALL;
  }
  bool tall = (p->flags & PF_TALL) != 0, any = false;
  for (int i = 0; i < B.nzombies; i++) {
    Zombie *z = &B.zombies[i];
    if (!targetable(z) || zombie_flying(z) != tall || (z->flags & ZF_UNDERGROUND) || z->emerge > 0) continue;
    if (zombie_submerged(z) && !(z->kind == Z_SNORKEL && p->kind == P_FUMESHROOM)) continue;
    if (z->row != row || z->x <= x - 10 || z->x >= x + range) continue;
    any = true;
    if (p->kind == P_FUMESHROOM && z->vault <= 0) {
      damage_zombie(z, 20, true);
      z->hit_at = B.ticks;
      if (z->health <= 0) kill_zombie(z, DIE_NORMAL);
    }
  }
  if (!any && !force) return;
  p->shot = B.ticks;
  if (p->kind == P_FUMESHROOM) {
    fx_add(FX_SMOKE, x + 25, row_y(row, x) - 45, 32, 3);
    return;
  }
  int kind = p->kind == P_SNOWPEA ? S_ICE : p->kind == P_PUFFSHROOM || p->kind == P_SCAREDYSHROOM || p->kind == P_SEASHROOM
             ? S_PUFF : p->kind == P_CACTUS ? S_SPIKE : S_PEA;
  Shot *s = new_shot(kind, row, x + 27, row_y(p->row, x) - 43);
  if (!s) return;
  if (tall) s->flags |= SF_HIGH;
  if (p->kind == P_SCAREDYSHROOM) s->flags |= SF_LONG;
  if (B.env == ENV_ROOF) s->flags |= SF_ROOFCHECK;
}

static void fire_rear(Plant *p) {
  float x = cell_x(p->col);
  bool any = false;
  for (int i = 0; i < B.nzombies; i++) {
    const Zombie *z = &B.zombies[i];
    any |= targetable(z) && !zombie_flying(z) && !zombie_submerged(z) && z->row == p->row && z->x < x;
  }
  if (!any) return;
  Shot *s = new_shot(S_PEA, p->row, x - 25, row_y(p->row, x) - 43);
  if (s) s->vx = -3.33f;
  if (s && B.env == ENV_ROOF) s->flags |= SF_ROOFCHECK;
  p->shot = B.ticks;
}

static void fire_stars(Plant *p) {
  bool any = false;
  for (int i = 0; i < B.nzombies; i++) {
    const Zombie *z = &B.zombies[i];
    any |= targetable(z) && !zombie_flying(z) && !zombie_submerged(z) && z->x < 825;
  }
  if (!any) return;
  static const float v[5][2] = {{-3.33f, 0}, {0, -3.33f}, {0, 3.33f}, {2.35f, -2.35f}, {2.35f, 2.35f}};
  float x = cell_x(p->col);
  for (int k = 0; k < 5; k++) {
    Shot *s = new_shot(S_STAR, p->row, x, row_y(p->row, x) - 35);
    if (s) s->vx = v[k][0], s->vy = v[k][1];
  }
  p->shot = B.ticks;
}

/* cabbage, kernel (or butter), melon: lobbed at the first zombie, where it will be when it lands */
static void fire_lob(Plant *p) {
  float x = cell_x(p->col);
  Zombie *t = NULL;
  for (int i = 0; i < B.nzombies; i++) {
    Zombie *z = &B.zombies[i];
    if (!targetable(z) || zombie_flying(z) || (z->kind == Z_IMP && z->u.flight.left > 0) || z->landing > 0 ||
        (z->flags & ZF_UNDERGROUND) || z->emerge > 0 || (z->kind == Z_BUNGEE && z->phase != BP_WAIT))
      continue;
    if (z->row != p->row || z->x <= x - 10 || z->x >= 825) continue;
    if (!t || z->x < t->x) t = z;
  }
  bool at_boss = !t && B.has_boss && boss_vulnerable();
  if (!t && !at_boss) return;
  int kind = p->kind == P_MELONPULT ? S_MELON : p->kind == P_CABBAGEPULT ? S_CABBAGE : frand() < 0.25f ? S_BUTTER : S_KERNEL;
  float ex = 740;
  if (t) {
    bool still = (t->flags & ZF_EATING) || t->kind == Z_BUNGEE || (t->kind == Z_GARGANTUAR && t->phase != GP_WALK) ||
                 (t->kind == Z_CATAPULT && t->phase != CP_WALK);
    ex = t->x - (still ? 0 : walking_speed(t) * 100);
  }
  Shot *s = new_shot(kind, p->row, x, row_y(p->row, x) - 60);
  if (!s) return;
  s->target = t ? t->id : 0, s->sx = x, s->ex = ex, s->sy = row_y(p->row, x) - 60, s->ey = row_y(p->row, ex) - 55;
  s->duration = 100;
  p->shot = B.ticks;
}

/* ---------------------------------------------------------------- the plants' own ways */

static void explode(float x, float y, int row, float range_x, int rows, bool through) {
  for (int i = 0; i < B.nzombies; i++) {
    Zombie *z = &B.zombies[i];
    if ((z->flags & ZF_HYPNO) || z->health <= 0) continue;
    int dr = z->row - row;
    if (dr < -rows || dr > rows || z->x - x > range_x || x - z->x > range_x) continue;
    damage_zombie(z, 1800, through);
    if (z->health <= 0) kill_zombie(z, DIE_BURN);
  }
  (void)y;
}

static void remove_ladders(int col, int row, int dc, int dr) {
  for (int i = 0; i < B.nladders; i++) {
    int c = B.ladders[i].col - col, r = B.ladders[i].row - row;
    if (c >= -dc && c <= dc && r >= -dr && r <= dr) B.ladders[i--] = B.ladders[--B.nladders];
  }
}

static void update_kelp(Plant *p) {
  if (!p->state) {
    for (int i = 0; i < B.nzombies; i++) {
      Zombie *z = &B.zombies[i];
      if (!targetable(z) || zombie_flying(z) || z->vault > 0 || zombie_headless(z) || z->row != p->row) continue;
      if (z->x <= 90 || z->x >= 780 || z->x - cell_x(p->col) >= 60 || cell_x(p->col) - z->x >= 60) continue;
      bool held = false;
      for (int k = 0; k < B.nplants; k++)
        held |= B.plants[k].kind == P_TANGLEKELP && B.plants[k].state && B.plants[k].target == z->id;
      if (held) continue;
      p->state = 1, p->target = z->id, p->aux = 100;
      return;
    }
  } else if (--p->aux <= 0 || p->health <= 0) {
    Zombie *z = zombie_by_id(p->target);
    if (z) kill_zombie(z, DIE_GONE), fx_add(FX_SPLASH, z->x, row_y(z->row, z->x), 50, 1);
    p->health = 0;
  }
}

static void update_squash(Plant *p) {
  float x = cell_x(p->col);
  if (!p->state) {
    Zombie *best = NULL;
    for (int i = 0; i < B.nzombies; i++) {
      Zombie *z = &B.zombies[i];
      if (!targetable(z) || zombie_flying(z) || (z->kind == Z_IMP && z->u.flight.left > 0) || z->landing > 0 ||
          zombie_headless(z) || z->row != p->row || z->vault > 0)
        continue;
      float d = z->x - x;
      d = d < 0 ? -d : d;
      if (d < 100 && (!best || d < (best->x - x < 0 ? x - best->x : best->x - x))) best = z;
    }
    if (best) p->state = 1, p->aux = 185, p->tx = best->x, p->target = best->id, p->shot = B.ticks;
    return;
  }
  p->aux--;
  if (p->aux == 60) {
    Zombie *z = zombie_by_id(p->target);
    if (z) p->tx = z->x - 30 * walking_speed(z);
  }
  if (p->aux <= 0) {
    for (int i = 0; i < B.nzombies; i++) {
      Zombie *z = &B.zombies[i];
      if ((z->flags & ZF_HYPNO) || zombie_flying(z) || (z->kind == Z_IMP && z->u.flight.left > 0) || z->landing > 0 ||
          z->row != p->row || z->vault > 0 || z->health <= 0)
        continue;
      if (z->x - p->tx < 65 && p->tx - z->x < 65) {
        damage_zombie(z, 1800, true);
        if (z->health <= 0) kill_zombie(z, DIE_SQUASH);
      }
    }
    p->health = 0, p->state = 0xEE;
    fx_add(FX_SMOKE, p->tx, row_y(p->row, p->tx), 30, 0);
  }
}

static void update_chomper(Plant *p) {
  float x = cell_x(p->col);
  Zombie *t = NULL;
  for (int i = 0; i < B.nzombies; i++) {
    Zombie *z = &B.zombies[i];
    if (!targetable(z) || zombie_flying(z) || (z->kind == Z_IMP && z->u.flight.left > 0) || z->landing > 0 ||
        (z->flags & ZF_UNDERGROUND) || (z->kind == Z_BUNGEE && z->phase != BP_WAIT) || z->row != p->row)
      continue;
    float d = z->x - x;
    if (d >= -10 && d <= 100 && (!t || z->x < t->x)) t = z;
  }
  if (p->state == CH_READY) {
    if (t) p->state = CH_BITE, p->aux = 70, p->shot = B.ticks;
    return;
  }
  if (--p->aux > 0) return;
  if (p->state == CH_BITE) {
    if (t && (t->kind == Z_ZOMBONI || t->kind == Z_CATAPULT || t->kind == Z_GARGANTUAR)) {
      damage_zombie(t, 40, true);
      if (t->health <= 0) kill_zombie(t, DIE_NORMAL);
      p->state = CH_RECOVER, p->aux = 30;
    } else if (t && !(t->flags & ZF_POLE) && t->vault <= 0) {
      kill_zombie(t, DIE_GONE);
      p->state = CH_DIGEST, p->aux = 4000;
    } else
      p->state = CH_RECOVER, p->aux = 30;
  } else
    p->state = CH_READY;
}

static void update_magnet(Plant *p) {
  if (p->action > 0) {
    p->action--;
    return;
  }
  float x = cell_x(p->col);
  Zombie *best = NULL;
  for (int i = 0; i < B.nzombies; i++) {
    Zombie *z = &B.zombies[i];
    if ((z->flags & ZF_HYPNO) || zombie_headless(z) || z->health <= 0) continue;
    int dr = z->row - p->row;
    if (dr < -2 || dr > 2) continue;
    float dx = z->x - x, dy = dr * cell_h();
    float reach = z->flags & ZF_EATING ? 320 : 270;
    if (dx * dx + dy * dy >= reach * reach) continue;
    bool metal = ((z->kind == Z_BUCKET || z->kind == Z_FOOTBALL) && z->armor > 0) ||
                 ((z->kind == Z_DOOR || z->kind == Z_LADDER) && z->shield > 0) || (z->flags & ZF_POGO) ||
                 (z->kind == Z_DIGGER && !(z->flags & ZF_METAL_GONE)) ||
                 (z->kind == Z_JACK && !(z->flags & (ZF_METAL_GONE | ZF_OPENING)));
    if (!metal) continue;
    if (!best || (dx < 0 ? -dx : dx) < (best->x - x < 0 ? x - best->x : best->x - x)) best = z;
  }
  if (best) {
    Zombie *z = best;
    if (z->kind == Z_BUCKET || z->kind == Z_FOOTBALL) z->armor = 0;
    else if (z->kind == Z_DOOR || z->kind == Z_LADDER) z->shield = 0, z->ladder_in = 0, z->speed = 0.3f;
    else if (z->kind == Z_POGO) z->flags &= (uint16_t)~ZF_POGO, z->speed = 0.3f;
    else if (z->kind == Z_DIGGER && (z->flags & ZF_UNDERGROUND)) {
      z->flags &= (uint16_t)~(ZF_UNDERGROUND | ZF_BACKWARDS), z->emerge = 330, z->speed = 0.3f;
    }
    z->flags |= ZF_METAL_GONE;
    p->action = 1500, p->shot = B.ticks, p->aux = (int16_t)z->kind;
    return;
  }
  for (int i = 0; i < B.nladders; i++) {
    int dr = B.ladders[i].row - p->row;
    float dx = (B.ladders[i].col - p->col) * 80.0f, dy = dr * cell_h();
    if (dr >= -2 && dr <= 2 && dx * dx + dy * dy < 270 * 270) {
      B.ladders[i] = B.ladders[--B.nladders];
      p->action = 1500, p->shot = B.ticks, p->aux = Z_LADDER;
      return;
    }
  }
}

static bool scared(const Plant *p) {
  float x = cell_x(p->col);
  for (int i = 0; i < B.nzombies; i++) {
    const Zombie *z = &B.zombies[i];
    if ((z->flags & ZF_HYPNO) || z->health <= 0 || zombie_headless(z)) continue;
    int dr = z->row - p->row;
    if (dr < -1 || dr > 1) continue;
    float dx = z->x - x;
    dx = dx < 0 ? -dx : dx;
    dx = dx > 20 ? dx - 20 : 0;
    float dy = (dr < 0 ? -dr : dr) * cell_h() - 50;
    dy = dy > 0 ? dy : 0;
    if (dx * dx + dy * dy <= 120 * 120) return true;
  }
  return false;
}

void plants_tick(void) {
  int n = B.nplants;
  for (int i = 0; i < n; i++) {
    Plant *p = &B.plants[i];
    if (p->kind == P_MARIGOLD && B.wave == B.nwaves && p->tx == 0) p->tx = (float)(B.ticks + 6000);
    if (p->kind == P_TANGLEKELP) {
      update_kelp(p);
      continue;
    }
    if (p->health <= 0 || sleeping(p)) continue;
    switch (p->kind) {
    case P_WALLNUT: case P_TALLNUT: case P_TORCHWOOD: case P_HYPNOSHROOM: case P_LILYPAD: case P_FLOWERPOT:
    case P_PUMPKIN: case P_GARLIC: case P_PLANTERN: case P_UMBRELLA:
      continue;
    }
    if (p->kind == P_SUNSHROOM && B.ticks - p->born >= 12000) p->flags |= PF_GROWN;
    if (p->kind == P_MAGNETSHROOM) {
      update_magnet(p);
      continue;
    }
    if (p->kind == P_COFFEEBEAN) {
      if (--p->action <= 0) {
        for (int k = 0; k < B.nplants; k++) {
          Plant *m = &B.plants[k];
          if (m->col == p->col && m->row == p->row && sleeping(m)) m->flags |= PF_AWAKE;
        }
        p->health = 0, p->state = 0xEE;
      }
      continue;
    }
    if (p->kind == P_SPLITPEA && p->aux > 0 && --p->aux == 0) fire_rear(p);
    if (p->kind == P_SPIKEWEED) {
      if (--p->action <= 0) {
        bool hit = false;
        for (int k = 0; k < B.nzombies; k++) {
          Zombie *z = &B.zombies[k];
          if ((z->flags & ZF_HYPNO) || zombie_flying(z) || (z->flags & ZF_UNDERGROUND) || z->health <= 0 ||
              z->row != p->row || z->vault > 0 || z->x - cell_x(p->col) >= 45 || cell_x(p->col) - z->x >= 45)
            continue;
          damage_zombie(z, 20, true);
          z->hit_at = B.ticks;
          if (z->health <= 0) kill_zombie(z, DIE_NORMAL);
          hit = true;
        }
        if (hit) p->shot = B.ticks, p->action = 100;
      }
      continue;
    }
    if (p->kind == P_SQUASH) {
      update_squash(p);
      continue;
    }
    if (p->kind == P_SCAREDYSHROOM) {
      if (scared(p)) {
        p->flags |= PF_SCARED, p->action = 150;
        continue;
      }
      p->flags &= (uint8_t)~PF_SCARED;
    }
    if (p->kind == P_SUNSHROOM && B.ticks - p->born >= 12000 && B.ticks - p->born < 12092) continue;
    if (p->kind == P_GRAVEBUSTER) {
      if (--p->action <= 0) {
        for (int k = 0; k < B.ngraves; k++)
          if (B.graves[k].row == p->row && B.graves[k].col == p->col) B.graves[k--] = B.graves[--B.ngraves];
        p->health = 0, p->state = 0xEE;
        fx_add(FX_SMOKE, cell_x(p->col), row_y(p->row, cell_x(p->col)) - 20, 30, 0);
      }
      continue;
    }
    if (p->kind == P_CHOMPER) {
      update_chomper(p);
      continue;
    }
    if (p->kind == P_POTATOMINE) {
      if (p->action > 0) {
        p->action--;
        if (p->action == 0) fx_add(FX_DIRT, cell_x(p->col), row_y(p->row, cell_x(p->col)), 30, 0);
        continue;
      }
      float x = cell_x(p->col);
      bool near = false;
      for (int k = 0; k < B.nzombies; k++) {
        const Zombie *z = &B.zombies[k];
        near |= !(z->flags & ZF_HYPNO) && !zombie_flying(z) && z->health > 0 && z->vault <= 0 && z->rise == 0 &&
                z->row == p->row && z->x - x < 35 && x - z->x < 35;
      }
      if (near) {
        for (int k = 0; k < B.nzombies; k++) {
          Zombie *z = &B.zombies[k];
          if ((z->flags & ZF_HYPNO) || zombie_flying(z) || z->vault > 0 || z->rise > 0 || z->row != p->row) continue;
          if (z->x - x <= 60 && x - z->x <= 60) {
            damage_zombie(z, 1800, true);
            if (z->health <= 0) kill_zombie(z, DIE_BURN);
          }
        }
        p->health = 0, p->state = 0xEE;
        fx_add(FX_BOOM, x, row_y(p->row, x) - 20, 60, 1);   /* spudow! */
      }
      continue;
    }
    p->action--;
    if (p->kind == P_REPEATER && p->action == 25) fire_pea(p, p->row, false);
    if (p->action > 0) continue;
    float x = cell_x(p->col), y = row_y(p->row, x);
    switch (p->kind) {
    case P_BLOVER:
      if (B.ticks - p->born <= 50) {
        for (int k = 0; k < B.nzombies; k++)
          if (B.zombies[k].flags & ZF_FLYING) B.zombies[k].blown = 100;
        B.fog_blown = 4000, p->action = 100, p->shot = B.ticks;
      } else
        p->health = 0, p->state = 0xEE;
      break;
    case P_JALAPENO:
      if (B.has_boss) {
        boss_damage(1800);
        boss.frozen = 0, boss.chill = 0;
        if (boss.ball.on && boss.ball.kind == 1 && boss.ball.row == p->row) boss.ball.on = 0;
      }
      remove_ladders(0, p->row, 99, 0);
      for (int k = 0; k < B.nzombies; k++) {
        Zombie *z = &B.zombies[k];
        if ((z->flags & ZF_HYPNO) || z->row != p->row) continue;
        damage_zombie(z, 1800, true);
        z->chill = 0, z->frozen = 0;
        if (z->health <= 0) kill_zombie(z, DIE_BURN);
      }
      if (B.ice[p->row].left > 20) B.ice[p->row].left = 20;
      p->health = 0, p->state = 0xEE;
      fx_add(FX_FIRE, 0, y, 100, p->row);
      break;
    case P_ICESHROOM:
      if (B.has_boss) {
        if (boss.ball.on && boss.ball.kind == 0) boss.ball.on = 0;
        if (boss_vulnerable()) {
          boss_damage(20);
          boss.frozen = (int16_t)(boss.chill > 0 ? 300 + irand(101) : 400 + irand(201));
          boss.chill = 2000;
        }
      }
      for (int k = 0; k < B.nzombies; k++) {
        Zombie *z = &B.zombies[k];
        if ((z->flags & ZF_HYPNO) || z->health <= 0 || z->kind == Z_ZOMBONI || z->kind == Z_CATAPULT ||
            (z->kind == Z_BOBSLED && z->u.sled.sled))
          continue;
        bool cold = z->chill > 0 || z->frozen > 0;
        z->frozen = (int16_t)(water_row(z->row) ? 300 : cold ? 300 + irand(101) : 400 + irand(201));
        z->chill = 2000;
        damage_zombie(z, 20, true);
        if (z->health <= 0) kill_zombie(z, DIE_NORMAL);
      }
      B.pool_frozen = 300, p->health = 0, p->state = 0xEE;
      fx_add(FX_FREEZE, 400, 300, 60, 0);
      break;
    case P_DOOMSHROOM:
      for (int k = 0; k < B.nzombies; k++) {
        Zombie *z = &B.zombies[k];
        if ((z->flags & ZF_HYPNO) || z->health <= 0) continue;
        int dr = z->row - p->row;
        float dx = z->x - x, dy = row_y(z->row, z->x) - y;
        if (dr >= -3 && dr <= 3 && dx * dx + dy * dy <= 250 * 250) {
          damage_zombie(z, 1800, true);
          if (z->health <= 0) kill_zombie(z, DIE_BURN);
        }
      }
      for (int k = 0; k < B.nplants; k++)
        if (B.plants[k].col == p->col && B.plants[k].row == p->row) B.plants[k].health = 0, B.plants[k].state = 0xEE;
      if (B.ncraters < 16)
        B.craters[B.ncraters].row = p->row, B.craters[B.ncraters].col = p->col, B.craters[B.ncraters++].left = 18000;
      fx_add(FX_DOOM, x, y - 35, 120, 0);
      break;
    case P_SPLITPEA:
      fire_pea(p, p->row, false);
      fire_rear(p);
      p->aux = 25, p->action = (int16_t)(150 - irand(15));
      break;
    case P_STARFRUIT:
      fire_stars(p);
      p->action = (int16_t)(150 - irand(15));
      break;
    case P_CABBAGEPULT: case P_KERNELPULT: case P_MELONPULT:
      fire_lob(p);
      p->action = (int16_t)(300 - irand(15));
      break;
    case P_THREEPEATER: {
      bool any = false;
      for (int k = 0; k < B.nzombies; k++) {
        const Zombie *z = &B.zombies[k];
        int dr = z->row - p->row;
        any |= targetable(z) && !zombie_flying(z) && !zombie_submerged(z) && dr >= -1 && dr <= 1 && z->x > x && z->x < 825;
      }
      if (any)
        for (int r = p->row - 1; r <= p->row + 1; r++)
          if (r >= 0 && r < B.nrows && (B.rows >> r & 1)) fire_pea(p, r, true);
      p->shot = B.ticks, p->action = (int16_t)(150 - irand(15));
      break;
    }
    case P_MARIGOLD:
      if (p->tx != 0 && B.ticks >= (uint32_t)p->tx) break;
      add_coin(x + 12, y - 10, frand() < 0.1f ? 50 : 10);
      p->action = (int16_t)(2500 - 150 + irand(151)), p->shot = B.ticks;
      break;
    case P_CHERRYBOMB:
      remove_ladders(p->col, p->row, 1, 1);
      explode(x, y, p->row, 115, 1, true);
      p->health = 0, p->state = 0xEE;
      fx_add(FX_BOOM, x, y - 35, 60, 0);
      break;
    case P_SUNFLOWER: case P_SUNSHROOM:
      add_sun(x + 12, y - 55, y - 15, p->kind == P_SUNSHROOM && !(p->flags & PF_GROWN) ? 15 : 25, false);
      p->action = (int16_t)(2500 - 150 + irand(151)), p->shot = B.ticks;
      break;
    default:
      p->action = (int16_t)(150 - irand(15));
      fire_pea(p, p->row, false);
    }
  }
}

/* ---------------------------------------------------------------- the shots */

static void hit_fx(float x, float y, int kind) { fx_add(FX_SPLAT, x, y, 18, kind); }

void shots_tick(void) {
  for (int i = 0; i < B.nshots; i++) {
    Shot *s = &B.shots[i];
    s->age++;
    bool gone = false;
    if (s->kind >= S_CABBAGE) {
      /* lobbed: a parabola to where its zombie will be */
      float t = (float)s->age / s->duration;
      s->x = s->sx + (s->ex - s->sx) * t;
      float arc = t * (1 - t) * 4;
      s->y = s->sy + (s->ey - s->sy) * t - arc * 150;
      if (t >= 1) {
        gone = true;
        if (s->kind == S_BASKETBALL) {
          for (int k = 0; k < B.nplants; k++) {
            Plant *p = &B.plants[k];
            if (p->id != s->target || p->health <= 0) continue;
            Plant *u = NULL;
            for (int j = 0; j < B.nplants; j++) {
              Plant *q = &B.plants[j];
              if (q->kind == P_UMBRELLA && q->health > 0 && q->col - p->col <= 1 && p->col - q->col <= 1 &&
                  q->row - p->row <= 1 && p->row - q->row <= 1)
                u = q;
            }
            if (u) u->shot = B.ticks;
            else p->health = (int16_t)(p->health - 75), p->shot = B.ticks;
          }
        } else if (!s->target && B.has_boss) {
          boss_damage(s->kind == S_MELON ? 80 : s->kind == S_KERNEL ? 20 : 40);
          hit_fx(s->x, s->y, s->kind);
        } else {
          Zombie *t = zombie_by_id(s->target);
          if (t && t->health > 0 && !(t->flags & ZF_UNDERGROUND) && (t->kind != Z_BUNGEE || t->phase == BP_WAIT) &&
              t->x - s->ex < 65 && s->ex - t->x < 65) {
            if (s->kind == S_MELON) {
              int near = 0;
              for (int k = 0; k < B.nzombies; k++) {
                const Zombie *z = &B.zombies[k];
                int dr = z->row - t->row;
                near += z != t && !(z->flags & ZF_HYPNO) && !zombie_flying(z) && !zombie_submerged(z) && z->health > 0 &&
                        dr >= -1 && dr <= 1 && z->x - t->x <= 80 && t->x - z->x <= 80;
              }
              int splash = near * 26 > 560 ? (80 * 560 / (near * 26 * 3) > 1 ? 80 * 560 / (near * 26 * 3) : 1) : 26;
              for (int k = 0; k < B.nzombies; k++) {
                Zombie *z = &B.zombies[k];
                int dr = z->row - t->row;
                bool hit = z == t || (!(z->flags & ZF_HYPNO) && !zombie_flying(z) && !zombie_submerged(z) && z->health > 0 &&
                                      dr >= -1 && dr <= 1 && z->x - t->x <= 80 && t->x - z->x <= 80);
                if (!hit) continue;
                int dmg = z == t ? 80 : splash;
                if (z->shield > 0) damage_zombie(z, z->shield < dmg ? z->shield : dmg, false);
                damage_zombie(z, dmg, true);
                z->hit_at = B.ticks;
                if (z->health <= 0) kill_zombie(z, DIE_NORMAL);
              }
            } else {
              damage_zombie(t, s->kind == S_KERNEL ? 20 : 40, true);
              t->hit_at = B.ticks;
              if (s->kind == S_BUTTER && t->kind != Z_CATAPULT && t->kind != Z_ZOMBONI) t->butter = 400;
              if (t->health <= 0) kill_zombie(t, DIE_NORMAL);
            }
            hit_fx(s->x, s->y, s->kind);
          }
        }
      }
    } else if (s->kind == S_STAR) {
      s->x += s->vx, s->y += s->vy;
      for (int k = 0; k < B.nzombies; k++) {
        Zombie *z = &B.zombies[k];
        if ((z->flags & ZF_HYPNO) || zombie_flying(z) || zombie_submerged(z) || z->health <= 0) continue;
        float dy = row_y(z->row, z->x) - 43 - s->y;
        if (z->x - s->x < 22 && s->x - z->x < 22 && dy < 32 && dy > -32) {
          damage_zombie(z, 20, false);
          z->hit_at = B.ticks;
          if (z->health <= 0) kill_zombie(z, DIE_NORMAL);
          hit_fx(s->x, s->y, S_STAR);
          gone = true;
          break;
        }
      }
      if (s->y < -80 || s->y > 650) gone = true;
    } else {
      float old = s->x;
      s->x += s->vx;
      if ((s->flags & SF_ROOFCHECK) && s->y > row_y(s->row, s->x) - 20) gone = true;
      if (s->kind == S_PEA || s->kind == S_ICE) {
        for (int k = 0; k < B.nplants; k++) {
          const Plant *p = &B.plants[k];
          if (p->kind != P_TORCHWOOD || p->row != s->row || p->col == s->torch || p->health <= 0) continue;
          float cx = cell_x(p->col);
          if ((s->vx > 0 && old <= cx + 10 && s->x >= cx - 10) || (s->vx < 0 && old >= cx - 10 && s->x <= cx + 10)) {
            s->torch = p->col;
            s->kind = s->kind == S_ICE ? S_PEA : S_FIRE;
          }
        }
      }
      if (s->kind == S_PUFF && !(s->flags & SF_LONG) && s->age >= 75) gone = true;
      Zombie *t = NULL;
      float lo = old < s->x ? old : s->x, hi = old < s->x ? s->x : old;
      for (int k = 0; k < B.nzombies && !gone; k++) {
        Zombie *z = &B.zombies[k];
        if ((z->flags & ZF_HYPNO) || !(s->flags & SF_HIGH) != !zombie_flying(z) || zombie_submerged(z) || z->health <= 0 ||
            z->row != s->row || z->vault > 0 || z->rise > 0)
          continue;
        if (lo <= z->x + 18 && hi >= z->x - 18 && (!t || (s->vx > 0 ? z->x < t->x : z->x > t->x))) t = z;
      }
      if (t) {
        bool body = t->shield < 20;
        if ((s->flags & SF_HIGH) && (t->flags & ZF_FLYING)) {
          t->flags &= (uint16_t)~ZF_FLYING, t->pop = 50;
        } else if (s->kind == S_FIRE) {
          for (int k = 0; k < B.nzombies; k++) {
            Zombie *z = &B.zombies[k];
            if ((z->flags & ZF_HYPNO) || zombie_flying(z) || zombie_submerged(z) || z->health <= 0 || z->row != s->row) continue;
            if (z->x - t->x <= 50 && t->x - z->x <= 50) {
              damage_zombie(z, 40, false);
              z->chill = 0, z->frozen = 0;
              if (z->health <= 0) kill_zombie(z, DIE_NORMAL);
            }
          }
        } else {
          damage_zombie(t, 20, false);
          if (t->health <= 0) kill_zombie(t, DIE_NORMAL);
        }
        if (s->kind == S_ICE && body && t->kind != Z_ZOMBONI && t->kind != Z_CATAPULT && !(t->kind == Z_BOBSLED && t->u.sled.sled))
          t->chill = 1000;
        t->hit_at = B.ticks;
        hit_fx(s->x, s->y, s->kind);
        gone = true;
      }
      if (s->x > 850 || s->x < -60) gone = true;
    }
    if (gone) B.shots[i--] = B.shots[--B.nshots];
  }
}
