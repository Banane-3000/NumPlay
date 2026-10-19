/* The monsters, and how each moves, as in Rebirth: health, speed and the
 * way it hunts or shoots. Speeds are pixels a frame (the game's units
 * halved); a shot goes 3 to 4 a frame. */
#include <string.h>
#include "game.h"
#include "gfx.h"
#include "res.h"

enum {
  AI_CHASE, AI_FLY, AI_ATTACK_FLY, AI_POOTER, AI_HORF, AI_HOST, AI_CLOTTY, AI_FLEE, AI_HIVE, AI_CHARGER, AI_GLOBIN,
  AI_BOOM_FLY, AI_MAW, AI_HOPPER, AI_LEAPER, AI_SPIDER, AI_KNIGHT, AI_DIP, AI_GUSHER, AI_SKINNY, AI_BONY, AI_BABY,
  AI_VIS, AI_EYE, AI_SUCKER, AI_BOIL, AI_SPITTY, AI_BRAIN, AI_NEST, AI_DEATHS_HEAD, AI_MOMS_HAND, AI_GRIMACE,
  AI_BAT, AI_IMP, AI_TUMOR, AI_POKY, AI_BURROW, AI_LEECH, AI_TETHER, AI_WALL, AI_GAPING, AI_FATTY, AI_KEEPER,
  AI_SWARMER, AI_MULLIGAN, AI_WANDER, AI_BLUE_FLY
};

typedef struct {
  const char *name;
  uint8_t ai;
  uint8_t hp;
  uint8_t r;          /* radius */
  uint8_t speed;      /* tenths of a pixel a frame */
  uint8_t flying;
  int16_t spr;
} MonInfo;

#define M(n, ai, hp, r, sp, fl, spr) {n, ai, hp, r, sp, fl, spr}
static const MonInfo mons[] = {
  [MON_GAPER] = M("GAPER", AI_CHASE, 10, 7, 14, 0, SPR_GAPER),
  [MON_FROWNING_GAPER] = M("FROWNING GAPER", AI_CHASE, 10, 7, 12, 0, SPR_FROWNING_GAPER),
  [MON_GUSHER] = M("GUSHER", AI_GUSHER, 10, 7, 12, 0, SPR_GUSHER),
  [MON_PACER] = M("PACER", AI_WANDER, 10, 7, 14, 0, SPR_PACER),
  [MON_HORF] = M("HORF", AI_HORF, 10, 7, 0, 0, SPR_HORF),
  [MON_FLY] = M("FLY", AI_FLY, 5, 5, 6, 1, SPR_FLY),
  [MON_POOTER] = M("POOTER", AI_POOTER, 8, 6, 8, 1, SPR_POOTER),
  [MON_SUPER_POOTER] = M("SUPER POOTER", AI_POOTER, 14, 7, 8, 1, SPR_SUPER_POOTER),
  [MON_CLOTTY] = M("CLOTTY", AI_CLOTTY, 15, 8, 10, 0, SPR_CLOTTY),
  [MON_CLOT] = M("CLOT", AI_CLOTTY, 13, 7, 10, 0, SPR_CLOT),
  [MON_IBLOB] = M("I. BLOB", AI_CLOTTY, 20, 8, 10, 0, SPR_IBLOB),
  [MON_MULLIGAN] = M("MULLIGAN", AI_MULLIGAN, 13, 7, 14, 0, SPR_MULLIGAN),
  [MON_ATTACK_FLY] = M("ATTACK FLY", AI_ATTACK_FLY, 5, 5, 20, 1, SPR_ATTACK_FLY),
  [MON_MAGGOT] = M("MAGGOT", AI_CHARGER, 7, 6, 8, 0, SPR_MAGGOT),
  [MON_HIVE] = M("HIVE", AI_HIVE, 25, 9, 10, 0, SPR_HIVE),
  [MON_CHARGER] = M("CHARGER", AI_CHARGER, 10, 6, 10, 0, SPR_CHARGER),
  [MON_GLOBIN] = M("GLOBIN", AI_GLOBIN, 20, 8, 13, 0, SPR_GLOBIN),
  [MON_BOOM_FLY] = M("BOOM FLY", AI_BOOM_FLY, 20, 7, 15, 1, SPR_BOOM_FLY),
  [MON_RED_BOOM_FLY] = M("RED BOOM FLY", AI_BOOM_FLY, 20, 7, 15, 1, SPR_RED_BOOM_FLY),
  [MON_MAW] = M("MAW", AI_MAW, 15, 7, 8, 1, SPR_MAW),
  [MON_RED_MAW] = M("RED MAW", AI_MAW, 25, 7, 8, 1, SPR_RED_MAW),
  [MON_HOST] = M("HOST", AI_HOST, 14, 7, 0, 0, SPR_HOST),
  [MON_RED_HOST] = M("RED HOST", AI_HOST, 20, 7, 0, 0, SPR_RED_HOST),
  [MON_HOPPER] = M("HOPPER", AI_HOPPER, 10, 6, 0, 0, SPR_HOPPER),
  [MON_TRITE] = M("TRITE", AI_LEAPER, 10, 6, 0, 0, SPR_TRITE),
  [MON_BOIL] = M("BOIL", AI_BOIL, 15, 7, 0, 0, SPR_BOIL),
  [MON_SPITTY] = M("SPITTY", AI_SPITTY, 15, 7, 12, 0, SPR_SPITTY),
  [MON_BRAIN] = M("BRAIN", AI_BRAIN, 20, 8, 16, 0, SPR_BRAIN),
  [MON_LEAPER] = M("LEAPER", AI_LEAPER, 20, 8, 0, 0, SPR_LEAPER),
  [MON_MR_MAW] = M("MR. MAW", AI_TETHER, 22, 7, 14, 0, SPR_MR_MAW),
  [MON_BABY] = M("BABY", AI_BABY, 12, 7, 0, 1, SPR_BABY),
  [MON_ANGELIC_BABY] = M("ANGELIC BABY", AI_BABY, 25, 7, 0, 1, SPR_ANGELIC_BABY),
  [MON_VIS] = M("VIS", AI_VIS, 22, 8, 12, 0, SPR_VIS),
  [MON_GUTS] = M("GUTS", AI_WALL, 20, 7, 14, 0, SPR_GUTS),
  [MON_KNIGHT] = M("KNIGHT", AI_KNIGHT, 12, 7, 10, 0, SPR_KNIGHT),
  [MON_SELFLESS_KNIGHT] = M("SELFLESS KNIGHT", AI_KNIGHT, 18, 7, 10, 0, SPR_SELFLESS_KNIGHT),
  [MON_LEECH] = M("LEECH", AI_LEECH, 15, 6, 22, 0, SPR_LEECH),
  [MON_KAMIKAZE_LEECH] = M("KAMIKAZE LEECH", AI_LEECH, 12, 6, 24, 0, SPR_KAMIKAZE_LEECH),
  [MON_PARA_BITE] = M("PARA-BITE", AI_BURROW, 15, 6, 0, 0, SPR_PARA_BITE),
  [MON_EYE] = M("EYE", AI_EYE, 20, 7, 0, 0, SPR_EYE),
  [MON_BLOODSHOT_EYE] = M("BLOODSHOT EYE", AI_EYE, 25, 7, 0, 0, SPR_BLOODSHOT_EYE),
  [MON_SUCKER] = M("SUCKER", AI_SUCKER, 10, 6, 12, 1, SPR_SUCKER),
  [MON_SPIT] = M("SPIT", AI_SUCKER, 10, 6, 12, 1, SPR_SPIT),
  [MON_EMBRYO] = M("EMBRYO", AI_HOPPER, 10, 6, 0, 0, SPR_EMBRYO),
  [MON_MOTER] = M("MOTER", AI_ATTACK_FLY, 12, 6, 14, 1, SPR_MOTER),
  [MON_SPIDER] = M("SPIDER", AI_SPIDER, 5, 5, 30, 0, SPR_SPIDER),
  [MON_KEEPER] = M("KEEPER", AI_KEEPER, 18, 7, 0, 0, SPR_KEEPER),
  [MON_GURGLE] = M("GURGLE", AI_CHASE, 20, 7, 18, 0, SPR_GURGLE),
  [MON_WALKING_BOIL] = M("WALKING BOIL", AI_SPITTY, 25, 8, 9, 0, SPR_WALKING_BOIL),
  [MON_BUTTLICKER] = M("BUTTLICKER", AI_SPIDER, 20, 7, 22, 0, SPR_BUTTLICKER),
  [MON_HANGER] = M("HANGER", AI_POOTER, 20, 7, 10, 1, SPR_HANGER),
  [MON_SWARMER] = M("SWARMER", AI_SWARMER, 15, 6, 10, 1, SPR_SWARMER),
  [MON_HEART] = M("HEART", AI_FLY, 15, 6, 10, 1, SPR_HEART),
  [MON_MASK] = M("MASK", AI_CHARGER, 255, 7, 14, 0, SPR_MASK),
  [MON_BIG_SPIDER] = M("BIG SPIDER", AI_SPIDER, 13, 7, 26, 0, SPR_BIG_SPIDER),
  [MON_NEST] = M("NEST", AI_NEST, 25, 9, 0, 0, SPR_NEST),
  [MON_BABY_LONG_LEGS] = M("BABY LONG LEGS", AI_NEST, 40, 9, 10, 0, SPR_BABY_LONG_LEGS),
  [MON_FATTY] = M("FATTY", AI_FATTY, 25, 9, 7, 0, SPR_FATTY),
  [MON_PALE_FATTY] = M("PALE FATTY", AI_FATTY, 30, 9, 7, 0, SPR_PALE_FATTY),
  [MON_FAT_SACK] = M("FAT SACK", AI_FATTY, 35, 9, 6, 0, SPR_FAT_SACK),
  [MON_DEATHS_HEAD] = M("DEATH'S HEAD", AI_DEATHS_HEAD, 255, 7, 18, 1, SPR_DEATHS_HEAD),
  [MON_MOMS_HAND] = M("MOM'S HAND", AI_MOMS_HAND, 50, 10, 0, 1, SPR_MOMS_HAND),
  [MON_DIP] = M("DIP", AI_DIP, 6, 5, 0, 0, SPR_DIP),
  [MON_SQUIRT] = M("SQUIRT", AI_DIP, 15, 8, 0, 0, SPR_SQUIRT),
  [MON_SKINNY] = M("SKINNY", AI_SKINNY, 20, 7, 8, 0, SPR_SKINNY),
  [MON_BONY] = M("BONY", AI_BONY, 10, 7, 11, 0, SPR_BONY),
  [MON_ONE_TOOTH] = M("ONE TOOTH", AI_BAT, 15, 6, 16, 1, SPR_ONE_TOOTH),
  [MON_GAPING_MAW] = M("GAPING MAW", AI_GAPING, 30, 8, 0, 1, SPR_GAPING_MAW),
  [MON_RAGLING] = M("RAGLING", AI_GLOBIN, 16, 7, 16, 0, SPR_RAGLING),
  [MON_NIGHT_CRAWLER] = M("NIGHT CRAWLER", AI_BURROW, 20, 7, 0, 0, SPR_NIGHT_CRAWLER),
  [MON_DART_FLY] = M("DART FLY", AI_ATTACK_FLY, 8, 5, 26, 1, SPR_DART_FLY),
  [MON_FAT_BAT] = M("FAT BAT", AI_BAT, 25, 8, 12, 1, SPR_FAT_BAT),
  [MON_IMP] = M("IMP", AI_IMP, 25, 7, 0, 0, SPR_IMP),
  [MON_TUMOR] = M("TUMOR", AI_TUMOR, 20, 7, 6, 1, SPR_TUMOR),
  [MON_POKY] = M("POKY", AI_POKY, 255, 7, 20, 0, SPR_POKY),
  [MON_STONE_GRIMACE] = M("STONE GRIMACE", AI_GRIMACE, 255, 8, 0, 0, SPR_STONE_GRIMACE),
  [MON_CONJOINED_FATTY] = M("CONJOINED FATTY", AI_FATTY, 40, 10, 6, 0, SPR_CONJOINED_FATTY),
  [MON_ROUND_WORM] = M("ROUND WORM", AI_BURROW, 15, 6, 0, 0, SPR_ROUND_WORM),
  [MON_WALL_CREEP] = M("WALL CREEP", AI_WALL, 15, 7, 12, 0, SPR_WALL_CREEP),
  [MON_MEMBRAIN] = M("MEMBRAIN", AI_BRAIN, 45, 10, 10, 0, SPR_MEMBRAIN),
  [MON_LUMP] = M("LUMP", AI_BURROW, 20, 7, 0, 0, SPR_LUMP),
  [MON_FRED] = M("FRED", AI_BURROW, 30, 8, 0, 0, SPR_FRED),
  [MON_HOMUNCULUS] = M("HOMUNCULUS", AI_TETHER, 25, 7, 16, 0, SPR_HOMUNCULUS),
  [MON_BEGOTTEN] = M("BEGOTTEN", AI_TETHER, 35, 8, 16, 0, SPR_BEGOTTEN),
  [MON_CHUBBER] = M("CHUBBER", AI_VIS, 25, 8, 10, 0, SPR_CHUBBER),
  [MON_FLAMING_HOPPER] = M("FLAMING HOPPER", AI_HOPPER, 15, 6, 0, 0, SPR_FLAMING_HOPPER),
  [MON_BLUE_FLY] = M("BLUE FLY", AI_BLUE_FLY, 1, 4, 30, 1, SPR_FLY),
};

const char *mon_name(int type) {
  if (type >= 0 && type < (int)(sizeof mons / sizeof mons[0]) && mons[type].name) return mons[type].name;
  return NULL;
}

void mon_set_sprite(Ent *e) {
  if (e->type < sizeof mons / sizeof mons[0] && mons[e->type].name) e->spr = mons[e->type].spr;
}

Ent *mon_spawn(int type, float x, float y) {
  if (type >= MON_MONSTRO) {
    boss_spawn(type, x, y);
    return NULL;
  }
  if (type < 0 || type >= (int)(sizeof mons / sizeof mons[0]) || !mons[type].name) return NULL;
  Ent *e = ent_new(EK_MON, x, y);
  if (!e) return NULL;
  const MonInfo *m = &mons[type];
  e->type = (uint8_t)type;
  e->hp = e->maxhp = m->hp == 255 ? 1000 : m->hp * (1 + (fl.stage - 1) * 0.12f);
  e->r = m->r;
  e->spr = m->spr;
  if (m->flying) e->flags |= EF_FLYING;
  if (m->hp == 255) e->flags |= EF_INVULN | EF_NOCOUNT;
  if (m->ai == AI_HOST) e->flags |= EF_INVULN;
  if (m->ai == AI_MOMS_HAND) e->flags |= EF_HIDDEN | EF_NOHIT;
  if (m->ai == AI_BURROW) e->flags |= EF_HIDDEN | EF_NOHIT | EF_INVULN;
  if (type == MON_BLUE_FLY) e->flags |= EF_FRIEND;
  e->t = (int16_t)(20 + rnd(40));
  e->a = rndf() * 6.2831853f;
  e->c = x, e->b = y;   /* (where it began: tethers) */
  if (m->ai == AI_CHARGER || m->ai == AI_KNIGHT || m->ai == AI_LEECH || m->ai == AI_WALL || m->ai == AI_VIS)
    e->c = (float)rnd(4);   /* (for these, the way they go) */
  return e;
}

/* ---------------------------------------------------------------- helpers */
static float to_player(Ent *e, float *dx, float *dy) {
  float x = pl.x - e->x, y = pl.y - e->y, d = fsqrt(x * x + y * y);
  if (d < 0.01f) d = 0.01f;
  *dx = x / d, *dy = y / d;
  return d;
}
static float spd(Ent *e) {
  float s = mons[e->type].speed / 10.0f;
  if (e->flags & EF_CHAMPION && e->sub == 1) s *= 1.4f;
  return s;
}
static void walk(Ent *e, float dx, float dy, float s, float acc) {
  if (e->fear) dx = -dx, dy = -dy;
  if (e->confuse) dx = fcos(e->a), dy = fsin(e->a), e->a += 0.1f;
  e->vx += (dx * s - e->vx) * acc, e->vy += (dy * s - e->vy) * acc;
  move_ent(&e->x, &e->y, &e->vx, &e->vy, e->r * 0.8f, e->flags & EF_FLYING, false);
}
static bool aligned(Ent *e, float tol) { return absf(pl.x - e->x) < tol || absf(pl.y - e->y) < tol; }
static void shot4(Ent *e, float s, bool diag) {
  for (int d = 0; d < 4; d++) {
    float ax = DIR_X[d], ay = DIR_Y[d];
    if (diag) ax = (d & 1) ? 0.7071f : -0.7071f, ay = (d & 2) ? 0.7071f : -0.7071f;
    enemy_shot(e->x, e->y - 6, ax * s, ay * s, 0);
  }
}
static int dir_to_player(Ent *e) {
  float dx = pl.x - e->x, dy = pl.y - e->y;
  if (absf(dx) > absf(dy)) return dx < 0 ? DIR_LEFT : DIR_RIGHT;
  return dy < 0 ? DIR_UP : DIR_DOWN;
}

/* ---------------------------------------------------------------- the monsters' minds */
void mon_update(Ent *e) {
  const MonInfo *m = &mons[e->type];
  float dx, dy, d = to_player(e, &dx, &dy), s = spd(e);
  e->t--;
  if (e->flags & EF_FRIEND && e->type != MON_BLUE_FLY) {   /* (charmed: it goes for the others) */
    Ent *o = ent_nearest_monster(e->x, e->y, 400);
    if (o) {
      float ox = o->x - e->x, oy = o->y - e->y, od = fsqrt(ox * ox + oy * oy) + 0.01f;
      walk(e, ox / od, oy / od, s > 0 ? s : 1, 0.2f);
      if (od < e->r + o->r) ent_damage(o, 1, 0, 0, 0);
    }
    return;
  }
  switch (m->ai) {
    case AI_CHASE: {   /* walks at him, around what's in the way */
      float px, py;
      path_dir(e->x, e->y, pl.x, pl.y, false, &px, &py);
      walk(e, px, py, s, 0.15f);
      if (e->type == MON_GURGLE && (e->t & 7) == 0) creep_spawn(e->x, e->y, 7, 90, true, 0);
      break;
    }
    case AI_WANDER: case AI_GUSHER:   /* headless: wanders, sprays blood now and then */
      if (e->t <= 0) e->a = rndf() * 6.2831853f, e->t = (int16_t)(30 + rnd(40));
      walk(e, fcos(e->a), fsin(e->a), s, 0.1f);
      if (e->vx == 0 || e->vy == 0) e->a += 1.5f;
      if (m->ai == AI_GUSHER && rnd(70) == 0) enemy_shot(e->x, e->y - 10, (rndf() - 0.5f) * 3, -1 - rndf() * 2, 0);
      break;
    case AI_FLY: {     /* a lazy buzz towards him */
      e->a += (rndf() - 0.5f) * 0.8f;
      float wx = dx + fcos(e->a) * 0.9f, wy = dy + fsin(e->a) * 0.9f;
      walk(e, wx, wy, s, 0.1f);
      break;
    }
    case AI_ATTACK_FLY: case AI_BLUE_FLY: {
      if (m->ai == AI_BLUE_FLY) {   /* Isaac's: at the nearest monster, then pop */
        Ent *o = ent_nearest_monster(e->x, e->y, 400);
        if (!o) {
          walk(e, dx * (d > 30 ? 1 : -0.2f), dy * (d > 30 ? 1 : -0.2f), 2, 0.2f);
          break;
        }
        float ox = o->x - e->x, oy = o->y - e->y, od = fsqrt(ox * ox + oy * oy) + 0.01f;
        walk(e, ox / od, oy / od, s, 0.25f);
        if (od < e->r + o->r) ent_damage(o, pl.damage * 2, 0, 0, 0), e->kind = EK_NONE;
        break;
      }
      e->a += (rndf() - 0.5f) * 0.4f;
      walk(e, dx + fcos(e->a) * 0.3f, dy + fsin(e->a) * 0.3f, s, 0.12f);
      if (e->type == MON_DART_FLY && e->t <= 0) e->vx = dx * 5, e->vy = dy * 5, e->t = 50;
      break;
    }
    case AI_POOTER: {  /* floats this way and that; a shot at him every so often */
      e->a += (rndf() - 0.5f) * 0.5f;
      walk(e, fcos(e->a) * 0.7f + dx * 0.3f, fsin(e->a) * 0.7f + dy * 0.3f, s, 0.06f);
      if (e->t <= 0 && d < 160) {
        e->t = (int16_t)(50 + rnd(30));
        if (e->type == MON_SUPER_POOTER) {
          for (int k = -1; k <= 1; k += 2) enemy_shot(e->x, e->y - 8, dx * 3.5f - dy * k * 0.6f, dy * 3.5f + dx * k * 0.6f, 0);
        } else
          enemy_shot_at(e->x, e->y - 8, 3.5f);
      }
      break;
    }
    case AI_HORF:      /* shakes where it is; shoots when he comes near */
      if (e->t <= 0 && d < 150) e->t = 45, enemy_shot_at(e->x, e->y - 6, 4);
      break;
    case AI_HOST:      /* a skull, shut: it opens to fire three, then shuts */
      if (e->state == 0) {
        e->flags |= EF_INVULN;
        if (e->t <= 0 && d < 140) e->state = 1, e->t = 30, e->flags &= (uint16_t)~EF_INVULN;
      } else {
        if (e->t == 18) {
          float a = fatan2(dy, dx);
          for (int k = -1; k <= 1; k++) enemy_shot(e->x, e->y - 8, fcos(a + k * 0.3f) * 3.6f, fsin(a + k * 0.3f) * 3.6f, 0);
          if (e->type == MON_RED_HOST) for (int k = -2; k <= 2; k += 4) enemy_shot(e->x, e->y - 8, fcos(a + k * 0.3f) * 3.6f, fsin(a + k * 0.3f) * 3.6f, 0);
        }
        if (e->t <= 0) e->state = 0, e->t = (int16_t)(40 + rnd(30));
      }
      break;
    case AI_CLOTTY: {  /* blobs about; four shots at once (an I. Blob, eight) */
      if (e->t <= 0) {
        e->t = (int16_t)(50 + rnd(40));
        if (e->type == MON_IBLOB) shot4(e, 3, false), shot4(e, 3, true);
        else shot4(e, 3, e->type == MON_CLOT);
        e->a = rndf() * 6.2831853f;
      }
      walk(e, fcos(e->a), fsin(e->a), e->t > 25 ? s : 0, 0.1f);
      break;
    }
    case AI_FLEE: case AI_MULLIGAN: case AI_HIVE:   /* runs from him; a hive lets flies out */
      walk(e, -dx + fcos(e->a) * 0.4f, -dy + fsin(e->a) * 0.4f, d < 120 ? s : s * 0.3f, 0.1f);
      e->a += 0.05f;
      if (m->ai == AI_HIVE && e->t <= 0) {
        e->t = 80;
        int n = 0;
        for (int i = 0; i < MAX_ENTS; i++) n += ents[i].kind == EK_MON && ents[i].type == MON_FLY;
        if (n < 4) mon_spawn(MON_FLY, e->x, e->y - 6);
      }
      break;
    case AI_CHARGER: case AI_KNIGHT: {   /* straight lines; a charge when he is in line */
      if (e->state == 0) {
        if (e->t <= 0 || (e->vx == 0 && e->vy == 0 && e->t < 40)) e->c = (float)rnd(4), e->t = (int16_t)(30 + rnd(50));
        int dd = (int)e->c;
        walk(e, DIR_X[dd], DIR_Y[dd], s, 0.3f);
        e->frame = 0;
        if (e->type != MON_MAGGOT && aligned(e, 8) && d < 200 && grid_line_clear(e->x, e->y, pl.x, pl.y)) {
          int pd = dir_to_player(e);
          if (e->type == MON_KNIGHT || e->type == MON_SELFLESS_KNIGHT || pd == dd || e->type == MON_CHARGER || e->type == MON_MASK)
            e->state = 1, e->c = (float)pd, e->t = 40;
        }
      } else {
        int dd = (int)e->c;
        walk(e, DIR_X[dd], DIR_Y[dd], s * 3, 0.4f);
        if (e->t <= 0 || (e->vx == 0 && e->vy == 0)) e->state = 0, e->t = 30;
      }
      if (m->ai == AI_KNIGHT) e->b = e->c;   /* (the way its shield faces) */
      break;
    }
    case AI_GLOBIN:    /* falls into goo when beaten, gets back up unless hurt again */
      if (e->state == 1) {
        e->vx = e->vy = 0;
        if (e->t <= 0) e->state = 0, e->hp = e->maxhp, e->flags &= (uint16_t)~EF_NOHIT;
        break;
      }
      {
        float px, py;
        path_dir(e->x, e->y, pl.x, pl.y, false, &px, &py);
        walk(e, px, py, s, 0.15f);
      }
      break;
    case AI_BOOM_FLY:  /* bounces about on the diagonals */
      if (!e->vx && !e->vy) e->vx = s * (rnd(2) ? 1 : -1), e->vy = s * (rnd(2) ? 1 : -1);
      {
        float ox = e->vx, oy = e->vy;
        move_ent(&e->x, &e->y, &e->vx, &e->vy, e->r, true, false);
        if (e->vx == 0) e->vx = -ox;
        if (e->vy == 0) e->vy = -oy;
      }
      break;
    case AI_MAW:       /* a floating head: drifts at him, shoots when near */
      walk(e, dx, dy, s, 0.05f);
      if (e->t <= 0 && d < 130) e->t = (int16_t)(60 + rnd(30)), enemy_shot_at(e->x, e->y - 8, 3.5f);
      break;
    case AI_HOPPER: case AI_LEAPER: {   /* hops; a leaper's jump lands on him */
      if (e->state == 0) {
        e->z = 0;
        if (e->t <= 0) {
          e->state = 1;
          float range = m->ai == AI_LEAPER ? (d < 100 ? d : 100) : 40;
          float a = m->ai == AI_LEAPER ? fatan2(dy, dx) : fatan2(dy, dx) + (rndf() - 0.5f) * 2;
          e->a = a, e->b = range;
          e->t2 = m->ai == AI_LEAPER ? 30 : 16;
          e->t3 = e->t2;
          e->flags |= m->ai == AI_LEAPER ? EF_NOHIT : 0;
        }
      } else {
        float k = 1 - (float)e->t2 / e->t3;
        e->z = -(m->ai == AI_LEAPER ? 60 : 14) * 4 * k * (1 - k);
        float step = e->b / e->t3;
        e->vx = fcos(e->a) * step, e->vy = fsin(e->a) * step;
        move_ent(&e->x, &e->y, &e->vx, &e->vy, e->r * 0.8f, e->z < -8, false);
        if (--e->t2 <= 0) {
          e->state = 0, e->z = 0, e->t = (int16_t)(m->ai == AI_LEAPER ? 40 + rnd(30) : 10 + rnd(25));
          e->flags &= (uint16_t)~EF_NOHIT;
          if (e->type == MON_LEAPER) shot4(e, 3, false);
          if (e->type == MON_FLAMING_HOPPER) creep_spawn(e->x, e->y, 8, 60, true, 2);
        }
      }
      break;
    }
    case AI_SPIDER:    /* skitters at him in quick dashes */
      if (e->t <= 0) {
        e->t = (int16_t)(12 + rnd(20));
        float a = fatan2(dy, dx) + (rndf() - 0.5f) * 1.6f;
        e->a = a, e->state = (uint8_t)(rnd(3) != 0);
      }
      walk(e, fcos(e->a), fsin(e->a), e->state ? s : 0, 0.3f);
      break;
    case AI_DIP:       /* slides in spurts */
      if (e->t <= 0) {
        e->t = (int16_t)(25 + rnd(20));
        float a = rndf() * 6.2831853f;
        float sp = e->type == MON_SQUIRT ? 4.5f : 3.5f;
        e->vx = fcos(a) * sp, e->vy = fsin(a) * sp;
      }
      e->vx *= 0.9f, e->vy *= 0.9f;
      move_ent(&e->x, &e->y, &e->vx, &e->vy, e->r * 0.8f, false, false);
      break;
    case AI_SKINNY:    /* slow until he hurts it, then it runs */
      {
        float px, py;
        path_dir(e->x, e->y, pl.x, pl.y, false, &px, &py);
        walk(e, px, py, e->hp < e->maxhp ? s * 2.2f : s, 0.15f);
      }
      break;
    case AI_BONY:      /* walks, throws bones when he is in line */
      {
        float px, py;
        path_dir(e->x, e->y, pl.x, pl.y, false, &px, &py);
        walk(e, px, py, d > 60 ? s : 0, 0.15f);
        if (e->t <= 0 && aligned(e, 10) && d < 180) {
          int pd = dir_to_player(e);
          enemy_shot(e->x, e->y - 6, DIR_X[pd] * 4, DIR_Y[pd] * 4, 1);
          e->t = 50;
        }
      }
      break;
    case AI_BABY: case AI_IMP:   /* gone, and back elsewhere; then a shot */
      if (e->state == 0) {
        if (e->t <= 0) e->state = 1, e->t = 12;
      } else if (e->state == 1) {
        e->alpha = (uint8_t)(e->t * 2);
        if (e->t <= 0) {
          for (int k = 0; k < 20; k++) {
            float x = ROOM_X + 20 + rnd(220), y = ROOM_Y + 20 + rnd(100);
            if (!solid_at(x, y, false) && dist2(x, y, pl.x, pl.y) > 50 * 50) {
              e->x = x, e->y = y;
              break;
            }
          }
          e->state = 2, e->t = 12;
        }
      } else {
        e->alpha = (uint8_t)(32 - e->t * 2);
        if (e->t <= 0) {
          e->alpha = 32, e->state = 0, e->t = (int16_t)(40 + rnd(30));
          if (m->ai == AI_IMP) shot4(e, 3.5f, false);
          else enemy_shot_at(e->x, e->y - 10, 3.5f);
        }
      }
      break;
    case AI_VIS:       /* walks; a blood beam when he is in line */
      if (e->state == 0) {
        float px, py;
        path_dir(e->x, e->y, pl.x, pl.y, false, &px, &py);
        walk(e, px, py, s, 0.12f);
        if (e->t <= 0 && aligned(e, 8) && d < 200) e->state = 1, e->t = 20, e->c = (float)dir_to_player(e);
      } else {
        e->vx = e->vy = 0;
        if (e->t == 8) laser_fire(e->x, e->y - 10, (int)e->c, 7, 0, 14, 0, true);
        if (e->t <= 0) e->state = 0, e->t = 70;
      }
      break;
    case AI_EYE:       /* stares; a beam (or shots) when he is in line */
      if (e->t <= 0 && aligned(e, 9)) {
        e->t = 70;
        int pd = dir_to_player(e);
        if (e->type == MON_EYE) laser_fire(e->x, e->y - 10, pd, 5, 0, 12, 0, true);
        else enemy_shot(e->x, e->y - 8, DIR_X[pd] * 4, DIR_Y[pd] * 4, 0), enemy_shot(e->x, e->y - 8, DIR_X[pd] * 3, DIR_Y[pd] * 3, 0);
      }
      break;
    case AI_SUCKER: case AI_SWARMER:   /* flies at him */
      e->a += (rndf() - 0.5f) * 0.6f;
      walk(e, dx + fcos(e->a) * 0.5f, dy + fsin(e->a) * 0.5f, s, 0.08f);
      break;
    case AI_BOIL:      /* grows, then spits a fountain */
      if (e->t <= 0 && d < 170) {
        e->t = 80;
        for (int k = 0; k < 4; k++) {
          Tear *t = enemy_shot(e->x, e->y - 8, dx * (2 + rndf()) + (rndf() - 0.5f), dy * (2 + rndf()) + (rndf() - 0.5f), 0);
          if (t) t->vz = -2.5f, t->flags |= TF_ARC, t->range = 1e9f;
        }
      }
      break;
    case AI_SPITTY: {  /* walks at him, spits when in line */
      float px, py;
      path_dir(e->x, e->y, pl.x, pl.y, false, &px, &py);
      walk(e, px, py, s, 0.12f);
      if (e->t <= 0 && aligned(e, 8) && d < 200) {
        int pd = dir_to_player(e);
        enemy_shot(e->x, e->y - 6, DIR_X[pd] * 4, DIR_Y[pd] * 4, 0);
        e->t = 45;
      }
      break;
    }
    case AI_BRAIN:     /* rolls and bounces, leaving creep */
      if (!e->vx && !e->vy) e->vx = s * (rnd(2) ? 1 : -1), e->vy = s * 0.7f * (rnd(2) ? 1 : -1);
      {
        float ox = e->vx, oy = e->vy;
        move_ent(&e->x, &e->y, &e->vx, &e->vy, e->r, false, false);
        if (e->vx == 0) e->vx = -ox;
        if (e->vy == 0) e->vy = -oy;
      }
      if ((e->t & 7) == 0) creep_spawn(e->x, e->y, 6, 80, true, 0);
      if (e->type == MON_MEMBRAIN && e->t <= 0) e->t = 60, enemy_shots_ring(e->x, e->y - 8, 6, 3, 0);
      break;
    case AI_NEST:      /* spiders come out of it */
      if (e->type == MON_BABY_LONG_LEGS) walk(e, dx, dy, d > 70 ? s : 0, 0.1f);
      if (e->t <= 0) {
        e->t = 100;
        int n = 0;
        for (int i = 0; i < MAX_ENTS; i++) n += ents[i].kind == EK_MON && ents[i].type == MON_SPIDER;
        if (n < 3) mon_spawn(MON_SPIDER, e->x, e->y + 6);
      }
      break;
    case AI_DEATHS_HEAD: /* a skull that bounces about for good (it dies with the room's last monster) */
      if (!e->vx && !e->vy) e->vx = s, e->vy = s;
      {
        float ox = e->vx, oy = e->vy;
        move_ent(&e->x, &e->y, &e->vx, &e->vy, e->r, true, false);
        if (e->vx == 0) e->vx = -ox;
        if (e->vy == 0) e->vy = -oy;
      }
      if (!ents_alive_monsters()) ent_kill(e), e->kind = EK_NONE;
      break;
    case AI_MOMS_HAND: /* Mom's hand: a shadow on him, then it comes down; it takes him away */
      if (e->state == 0) {
        e->flags |= EF_HIDDEN | EF_NOHIT;
        if (e->t <= 0) e->state = 1, e->t = 40;
      } else if (e->state == 1) {
        e->x += (pl.x - e->x) * 0.15f, e->y += (pl.y - e->y) * 0.15f;
        if (e->t <= 0) e->state = 2, e->t = 10, e->z = -80;
      } else if (e->state == 2) {
        e->flags &= (uint16_t)~(EF_HIDDEN | EF_NOHIT);
        e->z += 8;
        if (e->z >= 0) {
          e->z = 0, e->state = 3, e->t = 30;
          if (dist2(e->x, e->y, pl.x, pl.y) < 15 * 15 && pl.invuln == 0) {
            player_hurt(1, MON_MOMS_HAND);
            e->state = 4, e->t = 20;   /* got him */
          }
        }
      } else if (e->state == 3) {
        if (e->t <= 0) e->state = 0, e->t = 60, e->z = -80;
      } else {
        pl.x = e->x, pl.y = e->y, e->z -= 4;
        if (e->t <= 0) room_teleport(fl.start), e->kind = EK_NONE;
      }
      break;
    case AI_GRIMACE:   /* a stone face in a wall: shoots along its line, can't be hurt */
      if (e->t <= 0 && aligned(e, 10)) {
        e->t = 40;
        int pd = dir_to_player(e);
        enemy_shot(e->x, e->y - 6, DIR_X[pd] * 4, DIR_Y[pd] * 4, 0);
      }
      break;
    case AI_BAT:       /* flits about, then swoops at him */
      e->a += (rndf() - 0.5f) * 0.6f;
      walk(e, fcos(e->a) * 0.8f + dx * 0.5f, fsin(e->a) * 0.8f + dy * 0.5f, s, 0.1f);
      if (e->type == MON_FAT_BAT && e->t <= 0) e->t = 70, enemy_shot_at(e->x, e->y - 8, 3.5f);
      break;
    case AI_TUMOR:     /* floats; spreads shots */
      walk(e, fcos(e->a), fsin(e->a), s, 0.05f);
      e->a += 0.02f;
      if (e->t <= 0 && d < 170) e->t = 70, enemy_shots_ring(e->x, e->y - 8, 5, 3, fatan2(dy, dx));
      break;
    case AI_POKY:      /* spikes that slide back and forth for good */
      if (!e->vx && !e->vy) {
        if (e->state & 1) e->vy = e->state & 2 ? -s : s;
        else e->vx = e->state & 2 ? -s : s;
        e->state ^= 2;
      }
      {
        float ox = e->vx, oy = e->vy;
        move_ent(&e->x, &e->y, &e->vx, &e->vy, e->r, false, false);
        if (ox && !e->vx) e->vx = 0, e->vy = 0;
        if (oy && !e->vy) e->vx = 0, e->vy = 0;
      }
      if (!ents_alive_monsters()) e->kind = EK_NONE;
      break;
    case AI_BURROW:    /* under the floor; comes up near him to shoot, then down again */
      if (e->state == 0) {
        e->flags |= EF_HIDDEN | EF_NOHIT | EF_INVULN;
        if (e->t <= 0) {
          for (int k = 0; k < 20; k++) {
            float a = rndf() * 6.2831853f, r = 40 + rnd(40);
            float x = pl.x + fcos(a) * r, y = pl.y + fsin(a) * r;
            if (!solid_at(x, y, false)) {
              e->x = x, e->y = y;
              break;
            }
          }
          e->state = 1, e->t = 50;
          e->flags &= (uint16_t)~(EF_HIDDEN | EF_NOHIT | EF_INVULN);
        }
      } else {
        if (e->t == 30) {
          if (e->type == MON_ROUND_WORM) enemy_shot_at(e->x, e->y - 8, 4);
          else if (e->type == MON_LUMP) enemy_shots_ring(e->x, e->y - 8, 6, 3, 0);
          else if (e->type == MON_PARA_BITE || e->type == MON_NIGHT_CRAWLER) {
            int pd = dir_to_player(e);
            e->vx = DIR_X[pd] * 2, e->vy = DIR_Y[pd] * 2;
          } else
            enemy_shot_at(e->x, e->y - 8, 3.5f), enemy_shot_at(e->x + 3, e->y - 8, 3);
        }
        e->vx *= 0.9f, e->vy *= 0.9f;
        move_ent(&e->x, &e->y, &e->vx, &e->vy, e->r, false, false);
        if (e->t <= 0) e->state = 0, e->t = (int16_t)(30 + rnd(40));
      }
      break;
    case AI_LEECH:     /* races straight at him along a line */
      if (e->state == 0 || e->t <= 0) {
        e->c = (float)dir_to_player(e), e->state = 1, e->t = 40;
      }
      {
        int dd = (int)e->c;
        walk(e, DIR_X[dd], DIR_Y[dd], s, 0.2f);
        if (e->vx == 0 && e->vy == 0) e->state = 0;
      }
      if (e->type == MON_KAMIKAZE_LEECH && d < e->r + 8) explode(e->x, e->y, 30, 20, true, 0), e->kind = EK_NONE;
      break;
    case AI_TETHER: {  /* on a cord from where it began: lunges at him */
      float px, py;
      path_dir(e->x, e->y, pl.x, pl.y, false, &px, &py);
      walk(e, px, py, s, 0.12f);
      float ox = e->x - e->c, oy = e->y - e->b, od = fsqrt(ox * ox + oy * oy);
      if (od > 60) e->x = e->c + ox / od * 60, e->y = e->b + oy / od * 60;
      if (e->type == MON_MR_MAW && e->t <= 0 && d < 100) e->t = 60, enemy_shot_at(e->x, e->y - 8, 3.5f);
      break;
    }
    case AI_WALL:      /* along the walls, shooting across the room */
      if (e->t <= 0) e->t = (int16_t)(40 + rnd(40)), e->c = (float)rnd(4);
      {
        int dd = (int)e->c;
        walk(e, DIR_X[dd], DIR_Y[dd], s, 0.2f);
      }
      if (e->type == MON_WALL_CREEP && (e->t % 30) == 0 && aligned(e, 12)) {
        int pd = dir_to_player(e);
        enemy_shot(e->x, e->y - 6, DIR_X[pd] * 4, DIR_Y[pd] * 4, 0);
      }
      break;
    case AI_GAPING:    /* breathes him in */
      if (d < 140) pl.vx -= dx * 0.12f, pl.vy -= dy * 0.12f;
      break;
    case AI_FATTY: {   /* slow; a fat sack lets flies out, a pale one sprays */
      float px, py;
      path_dir(e->x, e->y, pl.x, pl.y, false, &px, &py);
      walk(e, px, py, s, 0.1f);
      if (e->type == MON_PALE_FATTY && e->t <= 0 && d < 100) e->t = 80, enemy_shots_ring(e->x, e->y - 10, 6, 2.5f, 0);
      if (e->type == MON_FAT_SACK && e->t <= 0) e->t = 120, mon_spawn(MON_ATTACK_FLY, e->x, e->y - 10);
      break;
    }
    case AI_KEEPER:    /* hops at him and spits */
      if (e->t <= 0) {
        e->t = 45;
        e->vx = dx * 3, e->vy = dy * 3;
        enemy_shot_at(e->x, e->y - 10, 3.5f);
      }
      e->vx *= 0.88f, e->vy *= 0.88f;
      move_ent(&e->x, &e->y, &e->vx, &e->vy, e->r, false, false);
      break;
    default: break;
  }
}

bool mon_on_death(Ent *e) {
  switch (e->type) {
    case MON_MULLIGAN: case MON_HIVE:
      for (int i = 0; i < 3 + rnd(3); i++) mon_spawn(MON_FLY, e->x + rnd(10) - 5, e->y + rnd(10) - 5);
      break;
    case MON_MOTER:
      for (int i = 0; i < 2; i++) mon_spawn(MON_ATTACK_FLY, e->x + i * 6 - 3, e->y);
      break;
    case MON_BOOM_FLY:
      explode(e->x, e->y, 30, 15, true, 0);
      break;
    case MON_RED_BOOM_FLY:
      enemy_shots_ring(e->x, e->y - 8, 6, 3.5f, 0);
      break;
    case MON_SUCKER:
      shot4(e, 3.5f, true);
      break;
    case MON_SPIT:
      creep_spawn(e->x, e->y, 12, 150, true, 1);
      break;
    case MON_BIG_SPIDER:
      for (int i = 0; i < 2; i++) mon_spawn(MON_SPIDER, e->x + i * 8 - 4, e->y);
      break;
    case MON_SQUIRT:
      for (int i = 0; i < 2; i++) mon_spawn(MON_DIP, e->x + i * 8 - 4, e->y);
      break;
    case MON_CONJOINED_FATTY:
      mon_spawn(MON_FATTY, e->x, e->y);
      break;
    case MON_SWARMER:
      mon_spawn(MON_ATTACK_FLY, e->x, e->y);
      break;
    case MON_GLOBIN: case MON_RAGLING:
      if (e->state == 0) {   /* down, not dead: unless hurt again soon */
        e->state = 1, e->t = 120, e->hp = e->maxhp * 0.3f, e->flags &= (uint16_t)~EF_NOHIT;
        decal_blood(e->x, e->y, 6);
        return false;
      }
      break;
    case MON_FAT_SACK:
      for (int i = 0; i < 2; i++) mon_spawn(MON_ATTACK_FLY, e->x, e->y);
      break;
    default: break;
  }
  return true;
}

/* ---------------------------------------------------------------- drawing */
void mon_draw(Ent *e) {
  if (e->type >= MON_MONSTRO) {
    boss_draw(e);
    return;
  }
  const MonInfo *m = &mons[e->type];
  int x = (int)e->x, y = (int)(e->y + e->z), flags = 0;
  if (e->flags & EF_HIDDEN) {
    if (m->ai == AI_MOMS_HAND && e->state == 1)   /* its shadow, coming */
      gfx_ellipse(x, (int)e->y, 12, 5, C_BLACK, 16);
    else if (m->ai == AI_BURROW)
      gfx_ellipse(x, (int)e->y, 5, 2, rgb565(0x3A2010), 16);
    return;
  }
  if (e->vx < -0.1f) flags = GFX_FLIPX;
  /* life: bobbing, squashing, wings */
  int sx = 256, sy = 256, lift = 0;
  if (e->flags & EF_FLYING) lift = 8 + (int)((g_frame + e->x) / 3) % 3;
  switch (m->ai) {
    case AI_CHASE: case AI_SKINNY: case AI_BONY: case AI_SPITTY: case AI_WANDER: case AI_GUSHER: case AI_FATTY:
    case AI_TETHER: case AI_WALL: case AI_KNIGHT: case AI_CHARGER: case AI_VIS:
      if (absf(e->vx) + absf(e->vy) > 0.2f) {
        int k = (int)((g_frame + (int)e->c) / 3) % 4;
        sy = 256 + (k == 1 ? 14 : k == 3 ? -14 : 0), sx = 512 - sy;
      }
      break;
    case AI_HORF: if (e->t < 10) x += (g_frame & 1) ? 1 : -1; break;
    case AI_HOST: if (e->state == 0) sy = 170, sx = 256; break;
    case AI_CLOTTY: case AI_DIP: case AI_GLOBIN: {
      int k = (int)(g_frame / 4 + e->x) % 6;
      sy = 256 + (k < 3 ? k : 6 - k) * 10 - 15, sx = 512 - sy;
      if (m->ai == AI_GLOBIN && e->state == 1) sy = 90, sx = 380;
      break;
    }
    case AI_BOIL: sx = sy = 200 + (80 - e->t > 0 ? (80 - e->t) : 0); if (sx > 300) sx = sy = 300; break;
    case AI_LEAPER: case AI_HOPPER: if (e->state == 0 && e->t < 6) sy = 220, sx = 290; break;
    default: break;
  }
  uint16_t tint = 0;
  int amount = 0;
  if (e->flash) tint = C_WHITE, amount = 20;
  else if (e->freeze) tint = rgb565(0xC0C0C0), amount = 24;
  else if (e->poison) tint = rgb565(0x40C040), amount = 12;
  else if (e->flags & EF_CHAMPION) {
    static const uint32_t champ[4] = {0xFF2020, 0xFFE020, 0x30C030, 0x202020};
    tint = rgb565(champ[e->sub & 3]), amount = 14;
  }
  if (e->flags & EF_FRIEND && e->type != MON_BLUE_FLY) tint = rgb565(0xFF80C0), amount = 14;
  if (e->type == MON_BLUE_FLY) tint = rgb565(0x4080FF), amount = 22;
  gfx_sprite_scaled(e->spr, 0, x, y + 3 - lift, sx, sy, flags, tint, amount, e->alpha);
}
