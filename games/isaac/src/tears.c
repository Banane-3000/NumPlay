/* Tears, the monsters' shots, lasers and bombs.
 *
 * A tear flies at its height over its shadow until it has gone its range
 * (23.75 is 6.5 cells), then drops and splashes. Brimstone's laser runs to
 * the wall and hurts all it crosses for a moment; a bomb goes off after a
 * second and a half, a heart's worth of hurt for Isaac in its blast. */
#include <string.h>
#include "game.h"
#include "gfx.h"
#include "res.h"

Tear tears[MAX_TEARS];
Laser lasers[MAX_LASERS];

#define RANGE_PX (130.0f / 23.75f)   /* pixels of flight for each point of range */

void tears_clear(void) {
  memset(tears, 0, sizeof tears);
  memset(lasers, 0, sizeof lasers);
}

static Tear *tear_new(void) {
  for (int i = 0; i < MAX_TEARS; i++)
    if (!tears[i].active) {
      memset(&tears[i], 0, sizeof tears[i]);
      tears[i].active = 1;
      tears[i].lasthit = -1;
      return &tears[i];
    }
  return NULL;
}

static float tear_size(float dmg, uint32_t flags) {
  float s = 0.75f + 0.25f * fsqrt(dmg / 3.5f);
  if (flags & TF_BIG) s *= 1.6f;
  if (s > 2.6f) s = 2.6f;
  return s;
}

Tear *tear_fire(float x, float y, float vx, float vy, float dmg, uint32_t flags) {
  Tear *t = tear_new();
  if (!t) return NULL;
  t->x = x, t->y = y + 1, t->z = -13, t->vx = vx, t->vy = vy;
  t->dmg = dmg, t->flags = flags;
  /* Tough Love: a tooth now and then, three times as hard */
  if ((flags & TF_TOOTH) && rnd(100) < 10 + (int)(pl.luck * 3)) t->dmg *= 3.2f, t->flags |= TF_TOOTH << 0;
  else t->flags &= ~(uint32_t)TF_TOOTH;
  t->size = tear_size(t->dmg, flags);
  t->range = pl.range * RANGE_PX;
  if (flags & TF_ARC) t->vz = -3.2f, t->range = 1e9f;
  return t;
}

Tear *enemy_shot(float x, float y, float vx, float vy, int kind) {
  Tear *t = tear_new();
  if (!t) return NULL;
  t->x = x, t->y = y, t->z = -10, t->vx = vx, t->vy = vy;
  t->dmg = 1, t->size = 1, t->flags = TF_ENEMY | (kind == 1 ? TF_BONE : kind == 2 ? TF_FIRE : 0);
  t->range = 260;
  return t;
}

void enemy_shots_ring(float x, float y, int n, float speed, float a0) {
  for (int i = 0; i < n; i++) {
    float a = a0 + 6.2831853f * i / n;
    enemy_shot(x, y, fcos(a) * speed, fsin(a) * speed, 0);
  }
}

void enemy_shot_at(float x, float y, float speed) {
  float dx = pl.x - x, dy = pl.y - y, d = fsqrt(dx * dx + dy * dy);
  if (d < 1) d = 1;
  enemy_shot(x, y, dx / d * speed, dy / d * speed, 0);
}

static void splash(Tear *t) {
  if (t->flags & TF_EXPLODE) explode(t->x, t->y, 26, t->dmg + 30, false, 1);
  else fx_spawn(FX_SPLASH, t->x, t->y + t->z, 6);
  t->active = 0;
}

static void tear_hit_monster(Tear *t, Ent *e) {
  float dmg = t->dmg;
  if (t->flags & TF_COAL) dmg *= 1 + t->dist / 110;
  if (t->flags & TF_PROPTOSIS) {
    float k = 1 - t->dist / 150;
    dmg *= k < 0.25f ? 0.25f : k;
  }
  float d = fsqrt(t->vx * t->vx + t->vy * t->vy);
  float kx = d > 0 ? t->vx / d : 0, ky = d > 0 ? t->vy / d : 0;
  ent_damage(e, dmg, t->flags, kx * (t->flags & TF_CONFUSE ? 2.5f : 1.2f), ky * (t->flags & TF_CONFUSE ? 2.5f : 1.2f));
  /* what the items add */
  int luck = (int)pl.luck;
  if ((t->flags & TF_POISON) && rnd(100) < 35 + luck * 5) e->poison = 60, e->poison_dmg = pl.damage * 0.25f;
  if ((t->flags & TF_SLOW) && rnd(100) < 25 + luck * 5) e->slow = 75;
  if ((t->flags & TF_FREEZE) && rnd(100) < 20 + luck * 5) e->freeze = 45;
  if ((t->flags & TF_CONFUSE) && rnd(100) < 15 + luck * 5) e->confuse = 50;
  if (t->flags & TF_SPLIT) {   /* The Parasite: two smaller tears off to the sides */
    for (int s = -1; s <= 1; s += 2) {
      Tear *n = tear_new();
      if (!n) break;
      *n = *t;
      n->vx = -t->vy * s * 0.8f, n->vy = t->vx * s * 0.8f;
      n->dmg = t->dmg * 0.5f, n->size = t->size * 0.7f, n->flags &= ~(uint32_t)TF_SPLIT, n->lasthit = (int8_t)(e - ents);
      n->dist = 0, n->range = 60;
    }
  }
  if (t->flags & TF_EXPLODE) {
    explode(t->x, t->y, 26, t->dmg + 30, false, 1);
    t->active = 0;
    return;
  }
  if (t->flags & TF_PIERCE) {
    t->lasthit = (int8_t)(e - ents);
    return;
  }
  splash(t);
}

static void tear_update(Tear *t) {
  bool enemy = t->flags & TF_ENEMY;
  /* homing: a turn towards the nearest monster */
  if ((t->flags & TF_HOMING) && !enemy) {
    Ent *e = ent_nearest_monster(t->x, t->y, 140);
    if (e) {
      float sp = fsqrt(t->vx * t->vx + t->vy * t->vy), dx = e->x - t->x, dy = e->y - t->y, d = fsqrt(dx * dx + dy * dy);
      if (d > 1) {
        t->vx += (dx / d * sp - t->vx) * 0.12f, t->vy += (dy / d * sp - t->vy) * 0.12f;
        float n = fsqrt(t->vx * t->vx + t->vy * t->vy);
        if (n > 0) t->vx *= sp / n, t->vy *= sp / n;
      }
    }
  }
  /* My Reflection: back to Isaac past half its way */
  if ((t->flags & TF_BOOMERANG) && t->dist > t->range * 0.5f && !(t->flags & TF_ENEMY)) {
    float dx = pl.x - t->x, dy = pl.y - t->y, d = fsqrt(dx * dx + dy * dy);
    float sp = fsqrt(t->vx * t->vx + t->vy * t->vy);
    if (d < 8) {
      t->active = 0;
      return;
    }
    t->vx += (dx / d * sp - t->vx) * 0.15f, t->vy += (dy / d * sp - t->vy) * 0.15f;
  }
  t->x += t->vx, t->y += t->vy;
  float step = fsqrt(t->vx * t->vx + t->vy * t->vy);
  t->dist += step;
  t->life++;
  /* height: a tear falls once it has gone its range (an arc from the start for Ipecac) */
  if (t->flags & TF_ARC) {
    t->vz += 0.22f, t->z += t->vz;
    if (t->z >= 0) {
      splash(t);
      return;
    }
  } else if (t->dist > t->range) {
    t->vz += 0.55f, t->z += t->vz;
    if (t->z >= 0) {
      splash(t);
      return;
    }
  }
  /* walls */
  if (t->x < ROOM_X - 4 || t->x > ROOM_R + 4 || t->y < ROOM_Y - 4 || t->y > ROOM_B + 4) {
    if (t->flags & TF_BOOMERANG) t->vx = -t->vx, t->vy = -t->vy;
    else {
      splash(t);
      return;
    }
  }
  /* the grid */
  int c = (int)((t->x - ROOM_X) / TILE), r = (int)((t->y - ROOM_Y) / TILE);
  if (t->x >= ROOM_X && t->y >= ROOM_Y && c < COLS && r < ROWS && grid_blocks_tears(c, r) && !(t->flags & (TF_SPECTRAL | TF_ARC))) {
    if (!enemy) grid_hit(c, r, t->dmg);
    splash(t);
    return;
  }
  if (enemy) {
    float dx = pl.x - t->x, dy = (pl.y - 4) - t->y;
    if (dx * dx + dy * dy < 7 * 7 * t->size) {
      if (player_hurt(1, -10)) {}
      splash(t);
    }
    return;
  }
  /* monsters */
  for (int i = 0; i < MAX_ENTS; i++) {
    Ent *e = &ents[i];
    if (e->kind != EK_MON || (e->flags & (EF_FRIEND | EF_HIDDEN | EF_NOHIT)) || i == t->lasthit) continue;
    float rr = e->r + 3 * t->size;
    float dx = e->x - t->x, dy = e->y - t->y;
    if (dx * dx + dy * dy < rr * rr && (e->flags & EF_FLYING || t->z > -40)) {
      tear_hit_monster(t, e);
      if (!t->active) return;
    }
  }
  /* fire places and machines take tears too */
  for (int i = 0; i < MAX_ENTS; i++) {
    Ent *e = &ents[i];
    if (e->kind != EK_MACHINE || e->type > MA_BOMB_BUM) continue;
    float dx = e->x - t->x, dy = (e->y - 8) - t->y;
    if (dx * dx + dy * dy < 12 * 12) {
      splash(t);
      return;
    }
  }
}

void tears_update(void) {
  for (int i = 0; i < MAX_TEARS; i++)
    if (tears[i].active) tear_update(&tears[i]);
}

void tears_draw(void) {
  for (int i = 0; i < MAX_TEARS; i++) {
    Tear *t = &tears[i];
    if (!t->active) continue;
    int s = (int)(t->size * 170);
    if (t->flags & TF_ENEMY) {
      gfx_ellipse((int)t->x, (int)t->y, 3, 1, C_BLACK, 10);
      int spr = (t->flags & TF_BONE) ? SPR_BONE_SHOT : SPR_ENEMY_SHOT;
      gfx_sprite_scaled(spr, 0, (int)t->x, (int)(t->y + t->z), 180, 180, 0, (t->flags & TF_FIRE) ? rgb565(0xFF8020) : 0,
                        (t->flags & TF_FIRE) ? 16 : 0, 32);
      continue;
    }
    gfx_ellipse((int)t->x, (int)t->y, 2 + (int)t->size, 1, C_BLACK, 9);
    int spr = (t->flags & TF_BLOOD) ? SPR_BLOOD_TEAR : SPR_TEAR;
    uint16_t tint = 0;
    int amount = 0;
    if (t->flags & TF_HOMING) tint = rgb565(0xB040E0), amount = 14;
    else if (t->flags & TF_POISON) tint = rgb565(0x40C040), amount = 16;
    else if (t->flags & TF_FREEZE) tint = rgb565(0xFFFFFF), amount = 16;
    else if (t->flags & TF_SLOW) tint = rgb565(0xA0A0A0), amount = 18;
    else if (t->flags & TF_EXPLODE) tint = rgb565(0x30C030), amount = 18;
    else if (t->flags & TF_COAL) tint = rgb565(0x202020), amount = 22;
    else if (t->flags & TF_SPECTRAL) tint = rgb565(0xFFFFFF), amount = 10;
    if (pl.has[6] && !(t->flags & TF_BLOOD)) tint = rgb565(0xF0E040), amount = 18;
    gfx_sprite_scaled(spr, 0, (int)t->x, (int)(t->y + t->z), s, s, 0, tint, amount, (t->flags & TF_SPECTRAL) ? 26 : 32);
  }
}

/* ---------------------------------------------------------------- lasers */
Laser *laser_fire(float x, float y, int dir, float width, float dmg, int life, int kind, bool enemy) {
  for (int i = 0; i < MAX_LASERS; i++)
    if (!lasers[i].active) {
      Laser *l = &lasers[i];
      memset(l, 0, sizeof *l);
      l->active = 1, l->x = x, l->y = y, l->dir = (int8_t)dir, l->width = width, l->dmg = dmg, l->life = l->max = (int16_t)life;
      l->kind = (uint8_t)kind, l->enemy = enemy;
      return l;
    }
  return NULL;
}

/* the beam's rectangle on the floor */
static void laser_box(const Laser *l, float *x0, float *y0, float *x1, float *y1) {
  float hw = l->width / 2;
  if (l->dir < 0) {   /* (a beam from the sky: Crack the Sky) */
    *x0 = l->x - hw, *x1 = l->x + hw, *y0 = 0, *y1 = l->y;
    return;
  }
  float fy = l->y + 10;   /* (its floor line: the head is 10 over the feet) */
  switch (l->dir) {
    case DIR_UP: *x0 = l->x - hw, *x1 = l->x + hw, *y0 = 0, *y1 = fy; break;
    case DIR_DOWN: *x0 = l->x - hw, *x1 = l->x + hw, *y0 = fy, *y1 = SCREEN_H; break;
    case DIR_LEFT: *x0 = 0, *x1 = l->x, *y0 = fy - hw, *y1 = fy + hw; break;
    default: *x0 = l->x, *x1 = SCREEN_W, *y0 = fy - hw, *y1 = fy + hw; break;
  }
}

void lasers_update(void) {
  for (int i = 0; i < MAX_LASERS; i++) {
    Laser *l = &lasers[i];
    if (!l->active) continue;
    if (!l->enemy && l->kind == 0 && l->dir >= 0) l->x = pl.x, l->y = pl.y - 10;   /* (Brimstone follows him) */
    float x0, y0, x1, y1;
    laser_box(l, &x0, &y0, &x1, &y1);
    if (l->enemy) {
      if (pl.x + 4 > x0 && pl.x - 4 < x1 && pl.y > y0 && pl.y - 8 < y1 && l->life < l->max - 4) player_hurt(1, -11);
    } else {
      for (int k = 0; k < MAX_ENTS; k++) {
        Ent *e = &ents[k];
        if (e->kind != EK_MON || (e->flags & (EF_FRIEND | EF_HIDDEN))) continue;
        if (e->x + e->r > x0 && e->x - e->r < x1 && e->y + e->r > y0 && e->y - e->r < y1) ent_damage(e, l->dmg, 0, 0, 0);
      }
      /* poop and fire burn away under it */
      if ((l->life & 3) == 0)
        for (int r = 0; r < ROWS; r++)
          for (int c = 0; c < COLS; c++) {
            float cx = ROOM_X + c * TILE + TILE / 2, cy = ROOM_Y + r * TILE + TILE / 2;
            if (cx > x0 && cx < x1 && cy > y0 && cy < y1) grid_hit(c, r, l->dmg);
          }
    }
    if (--l->life <= 0) l->active = 0;
  }
}

void lasers_draw(void) {
  for (int i = 0; i < MAX_LASERS; i++) {
    Laser *l = &lasers[i];
    if (!l->active) continue;
    float x0, y0, x1, y1;
    laser_box(l, &x0, &y0, &x1, &y1);
    /* drawn at its height: the beam a head over its floor line */
    int lift = l->dir == DIR_LEFT || l->dir == DIR_RIGHT ? 10 : 0;
    int a = l->life > 3 ? 32 : l->life * 8;
    uint16_t edge, core;
    switch (l->kind) {
      case 1: edge = rgb565(0xE02040), core = rgb565(0xFFC0D0); break;
      case 2: edge = rgb565(0xFFF0A0), core = rgb565(0xFFFFFF); break;
      default: edge = rgb565(0x900000), core = rgb565(0xFF4040); break;
    }
    if (l->enemy) edge = rgb565(0x700000), core = rgb565(0xFF2020);
    int ix0 = (int)x0, iy0 = (int)y0 - lift, w = (int)(x1 - x0), h = (int)(y1 - y0);
    gfx_rect(ix0, iy0, w, h, edge, a * 3 / 4);
    if (l->dir == DIR_UP || l->dir == DIR_DOWN || l->dir < 0) gfx_rect(ix0 + w / 4, iy0, w / 2 > 0 ? w / 2 : 1, h, core, a);
    else gfx_rect(ix0, iy0 + h / 4, w, h / 2 > 0 ? h / 2 : 1, core, a);
  }
}

/* ---------------------------------------------------------------- bombs and explosions */
void bomb_place(float x, float y, int kind) {
  Ent *b = ent_new(EK_BOMB, x, y);
  if (!b) return;
  b->type = (uint8_t)kind;
  b->t = 45;
  b->r = 6;
  b->spr = kind == 1 ? SPR_TROLL_BOMB_LIT : (pl.has[106] && kind == 0) || kind == 2 ? SPR_BIG_BOMB
           : pl.has[140] && kind == 0 ? SPR_POISON_BOMB : SPR_BOMB;
  b->hp = kind == 2 ? 185 : pl.has[106] ? 185 : 60;   /* (its damage) */
}

void explode(float x, float y, float radius, float dmg, bool hurt_player, int kind) {
  Ent *f = ent_new(EK_FX, x, y);
  if (f) f->type = FX_EXPLOSION, f->t = 14, f->a = radius;
  g_shake = 8;
  for (int i = 0; i < MAX_ENTS; i++) {
    Ent *e = &ents[i];
    if (e->kind != EK_MON && e->kind != EK_PICKUP && e->kind != EK_BOMB && e->kind != EK_MACHINE) continue;
    float dx = e->x - x, dy = e->y - y, d2 = dx * dx + dy * dy;
    if (d2 > (radius + e->r) * (radius + e->r)) continue;
    float d = fsqrt(d2);
    if (d < 1) d = 1;
    if (e->kind == EK_MON && !(e->flags & EF_FRIEND)) {
      ent_damage(e, dmg, 0, dx / d * 4, dy / d * 4);
      if (kind == 2 || pl.has[140]) e->poison = 60, e->poison_dmg = 3;
    } else if (e->kind == EK_PICKUP || e->kind == EK_BOMB) {
      e->vx += dx / d * 4, e->vy += dy / d * 4;
      if (e->kind == EK_BOMB && e->t > 2) e->t = 2;   /* (a chain) */
    } else if (e->kind == EK_MACHINE && e->type <= MA_BOMB_BUM && e->state != 255) {
      /* a blown-up machine pays out once */
      e->state = 255;
      fx_spawn(FX_POOF, e->x, e->y, 10);
      if (e->type == MA_SLOT) for (int k = 0; k < 2 + rnd(3); k++) pickup_spawn(PK_PENNY, 0, e->x, e->y, true);
      else pickup_drop_random(e->x, e->y);
      e->kind = EK_NONE;
    }
  }
  grid_explode(x, y, radius);
  float dx = pl.x - x, dy = pl.y - y;
  if (hurt_player && dx * dx + dy * dy < (radius + 4) * (radius + 4) && !pl.has[190]) player_hurt(2, -1);
  decal_blood(x, y, -radius * 0.7f);   /* (a scorch mark: negative is black) */
}
