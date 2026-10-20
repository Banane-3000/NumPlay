/* The bosses: each floor's, Mom, Mom's Heart, Satan and Isaac, and the seven
 * deadly sins. Each moves as in Rebirth: Monstro's hops, leap and spray of
 * blood, Larry Jr.'s segments, the Duke's flies, the horsemen's charges...
 * Their health is Rebirth's, near enough. */
#include <string.h>
#include "game.h"
#include "gfx.h"
#include "res.h"

typedef struct {
  int16_t spr;
  int16_t hp;
  uint8_t r, flying;
  const char *name;
} BossInfo;

#define B(m, s, hp, r, fly, n) [m - MON_MONSTRO] = {s, hp, r, fly, n}
static const BossInfo bosses[MON_COUNT - MON_MONSTRO] = {
  B(MON_MONSTRO, SPR_MONSTRO, 250, 18, 0, "MONSTRO"),
  B(MON_LARRY, SPR_LARRY, 30, 8, 0, "LARRY JR."),
  B(MON_THE_HOLLOW, SPR_THE_HOLLOW, 40, 8, 0, "THE HOLLOW"),
  B(MON_DUKE, SPR_DUKE, 110, 18, 1, "THE DUKE OF FLIES"),
  B(MON_HUSK, SPR_HUSK, 350, 18, 1, "THE HUSK"),
  B(MON_GEMINI, SPR_GEMINI, 140, 10, 0, "GEMINI"),
  B(MON_GEMINI_BABY, SPR_GEMINI_BABY, 60, 7, 1, "SUTURE"),
  B(MON_STEVEN, SPR_STEVEN, 180, 10, 0, "STEVEN"),
  B(MON_STEVEN_BABY, SPR_STEVEN_BABY, 60, 7, 1, "STEVEN"),
  B(MON_BLIGHTED_OVUM, SPR_BLIGHTED_OVUM, 200, 10, 0, "THE BLIGHTED OVUM"),
  B(MON_GURDY, SPR_GURDY, 350, 30, 0, "GURDY"),
  B(MON_GURDY_JR, SPR_GURDY_JR, 150, 18, 0, "GURDY JR."),
  B(MON_PEEP, SPR_PEEP, 250, 16, 0, "PEEP"),
  B(MON_BLOAT, SPR_BLOAT, 500, 16, 0, "THE BLOAT"),
  B(MON_CHUB, SPR_CHUB, 140, 16, 0, "CHUB"),
  B(MON_CARRION_QUEEN, SPR_CARRION_QUEEN, 120, 16, 0, "CARRION QUEEN"),
  B(MON_LOKI, SPR_LOKI, 450, 14, 0, "LOKI"),
  B(MON_LOKII, SPR_LOKII, 350, 14, 0, "LOKII"),
  B(MON_MOM, SPR_MOM_EYE, 645, 16, 0, "MOM"),
  B(MON_MOMS_HEART, SPR_MOMS_HEART, 1200, 26, 0, "MOM'S HEART"),
  B(MON_IT_LIVES, SPR_IT_LIVES, 1500, 26, 0, "IT LIVES"),
  B(MON_FAMINE, SPR_FAMINE, 300, 18, 1, "FAMINE"),
  B(MON_PESTILENCE, SPR_PESTILENCE, 400, 18, 1, "PESTILENCE"),
  B(MON_WAR, SPR_WAR, 550, 18, 0, "WAR"),
  B(MON_CONQUEST, SPR_CONQUEST, 700, 18, 0, "CONQUEST"),
  B(MON_DEATH, SPR_DEATH, 700, 18, 1, "DEATH"),
  B(MON_PIN, SPR_PIN, 200, 10, 0, "PIN"),
  B(MON_SCOLEX, SPR_SCOLEX, 600, 10, 0, "SCOLEX"),
  B(MON_FRAIL, SPR_FRAIL, 500, 10, 0, "THE FRAIL"),
  B(MON_MONSTRO2, SPR_MONSTRO2, 600, 18, 0, "MONSTRO II"),
  B(MON_GISH, SPR_GISH, 650, 18, 0, "GISH"),
  B(MON_MASK_OF_INFAMY, SPR_MASK_OF_INFAMY, 9999, 18, 0, "MASK OF INFAMY"),
  B(MON_HEART_OF_INFAMY, SPR_HEART_OF_INFAMY, 600, 14, 0, "HEART OF INFAMY"),
  B(MON_WIDOW, SPR_WIDOW, 300, 20, 0, "THE WIDOW"),
  B(MON_WRETCHED, SPR_WRETCHED, 450, 20, 0, "THE WRETCHED"),
  B(MON_DADDY_LONG_LEGS, SPR_DADDY_LONG_LEGS, 700, 22, 1, "DADDY LONG LEGS"),
  B(MON_TRIACHNID, SPR_TRIACHNID, 700, 22, 1, "TRIACHNID"),
  B(MON_FISTULA, SPR_FISTULA_BIG, 160, 22, 0, "FISTULA"),
  B(MON_TERATOMA, SPR_TERATOMA_BIG, 180, 22, 0, "TERATOMA"),
  B(MON_BLASTOCYST, SPR_BLASTOCYST_BIG, 300, 22, 0, "BLASTOCYST"),
  B(MON_SATAN, SPR_FALLEN, 700, 22, 1, "SATAN"),
  B(MON_FALLEN, SPR_FALLEN, 700, 20, 1, "THE FALLEN"),
  B(MON_KRAMPUS, SPR_KRAMPUS, 700, 18, 0, "KRAMPUS"),
  B(MON_ISAAC, SPR_ISAAC_BOSS, 1500, 12, 0, "ISAAC"),
  B(MON_HEADLESS_HORSEMAN, SPR_HEADLESS_HORSEMAN, 500, 18, 0, "HEADLESS HORSEMAN"),
  B(MON_DINGLE, SPR_DINGLE, 250, 16, 0, "DINGLE"),
  B(MON_HAUNT, SPR_HAUNT, 300, 18, 1, "THE HAUNT"),
  B(MON_LIL_HAUNT, SPR_LIL_HAUNT, 20, 7, 1, "LIL HAUNT"),
  B(MON_MEGA_FATTY, SPR_MEGA_FATTY, 350, 20, 0, "MEGA FATTY"),
  B(MON_MAMA_GURDY, SPR_MAMA_GURDY, 700, 30, 0, "MAMA GURDY"),
  B(MON_DARK_ONE, SPR_DARK_ONE, 550, 18, 0, "THE DARK ONE"),
  B(MON_POLYCEPHALUS, SPR_POLYCEPHALUS, 350, 18, 0, "POLYCEPHALUS"),
  B(MON_MR_FRED, SPR_MR_FRED, 600, 18, 0, "MR. FRED"),
  B(MON_GURGLING, SPR_GURGLING, 120, 12, 0, "GURGLING"),
  B(MON_SLOTH, SPR_SLOTH, 100, 12, 0, "SLOTH"),
  B(MON_LUST, SPR_LUST, 100, 10, 0, "LUST"),
  B(MON_WRATH, SPR_WRATH, 120, 12, 0, "WRATH"),
  B(MON_GLUTTONY, SPR_GLUTTONY, 140, 14, 0, "GLUTTONY"),
  B(MON_GREED, SPR_GREED, 120, 12, 0, "GREED"),
  B(MON_ENVY, SPR_ENVY, 90, 12, 0, "ENVY"),
  B(MON_PRIDE, SPR_PRIDE, 160, 12, 0, "PRIDE"),
  B(MON_MOM_HAND, SPR_MOMS_HAND, 0, 14, 0, "MOM"),
  B(MON_MOM_FOOT, SPR_MOM_FOOT, 0, 24, 0, "MOM"),
  B(MON_SATAN_STOMP, SPR_SATAN_STOMP, 0, 24, 0, "SATAN"),
  B(MON_DEATH_SCYTHE, SPR_BONE_SHOT, 0, 5, 1, "DEATH"),
};

static const BossInfo *info(int type) { return type >= MON_MONSTRO && type < MON_COUNT ? &bosses[type - MON_MONSTRO] : NULL; }
const char *boss_name(int type) {
  const BossInfo *b = info(type);
  return b && b->name ? b->name : "";
}
int boss_sprite(int type) {
  const BossInfo *b = info(type);
  if (type == MON_SATAN) return SPR_SATAN;
  return b && b->spr ? b->spr : -1;
}

static Ent *boss_new(int type, float x, float y, int hp_scale_pct) {
  const BossInfo *b = info(type);
  if (!b || !b->spr) return NULL;
  Ent *e = ent_new(EK_MON, x, y);
  if (!e) return NULL;
  e->type = (uint8_t)type, e->spr = b->spr, e->r = b->r;
  e->hp = e->maxhp = b->hp * hp_scale_pct / 100.0f;
  e->flags = EF_BOSS | EF_NOKNOCK | (b->flying ? EF_FLYING : 0);
  e->t = 40, e->a = rndf() * 6.2831853f;
  return e;
}

static int sins_count;
void boss_spawn(int type, float x, float y) {
  Ent *e;
  switch (type) {
    case MON_LARRY: case MON_THE_HOLLOW: {   /* a worm of segments, each following the one before */
      int n = type == MON_LARRY ? 6 : 10, prev = -1;
      for (int i = 0; i < n; i++) {
        e = boss_new(type, x - i * 10, y, 100);
        if (!e) break;
        e->flags |= EF_SEGMENT;
        e->parent = (int8_t)prev, e->sub = (uint8_t)i, e->c = DIR_RIGHT;
        if (prev >= 0) ents[prev].child = (int8_t)(e - ents);
        prev = (int)(e - ents);
      }
      return;
    }
    case MON_CHUB: case MON_CARRION_QUEEN: {
      int prev = -1;
      for (int i = 0; i < 3; i++) {
        e = boss_new(type, x - i * 18, y, 100);
        if (!e) break;
        e->flags |= EF_SEGMENT, e->parent = (int8_t)prev, e->sub = (uint8_t)i;
        if (prev >= 0) ents[prev].child = (int8_t)(e - ents);
        prev = (int)(e - ents);
      }
      return;
    }
    case MON_GEMINI: case MON_STEVEN: case MON_BLIGHTED_OVUM: {
      e = boss_new(type, x - 20, y, 100);
      Ent *b = boss_new(type == MON_STEVEN ? MON_STEVEN_BABY : MON_GEMINI_BABY, x + 20, y, 100);
      if (e && b) b->parent = (int8_t)(e - ents), e->child = (int8_t)(b - ents);
      if (b && type == MON_BLIGHTED_OVUM) b->spr = SPR_GEMINI_BABY;
      return;
    }
    case MON_LOKII:
      boss_new(MON_LOKII, x - 40, y, 100);
      boss_new(MON_LOKII, x + 40, y, 100);
      return;
    case MON_MASK_OF_INFAMY: {
      e = boss_new(MON_MASK_OF_INFAMY, x - 30, y, 100);
      if (e) e->flags |= EF_INVULN | EF_NOCOUNT;
      Ent *h = boss_new(MON_HEART_OF_INFAMY, x + 30, y, 100);
      if (e && h) e->parent = (int8_t)(h - ents);
      return;
    }
    case MON_HAUNT:
      e = boss_new(MON_HAUNT, x, y - 10, 100);
      for (int i = 0; i < 3; i++) {
        Ent *l = boss_new(MON_LIL_HAUNT, x, y, 100);
        if (l) l->sub = (uint8_t)i, l->flags &= (uint16_t)~EF_BOSS;
      }
      return;
    case MON_MOM:   /* Mom herself is the room: her eye, hands and foot come from the doors and the sky */
      e = boss_new(MON_MOM, 160, 120, 100);
      if (e) e->flags |= EF_HIDDEN | EF_NOHIT, e->t = 50;
      return;
    case MON_FISTULA: case MON_TERATOMA: case MON_BLASTOCYST:
      e = boss_new(type, x, y, 100);
      if (e) e->sub = 0;   /* (its size: 0 big, 1 medium, 2 small) */
      return;
    case MON_SATAN:
      e = boss_new(MON_SATAN, x, y - 20, 100);
      if (e) e->spr = SPR_FALLEN, e->state = 0;
      return;
    case MON_ENVY: case MON_GLUTTONY: case MON_SLOTH: case MON_LUST: case MON_WRATH: case MON_GREED: case MON_PRIDE:
      sins_count++;
      e = boss_new(type, x, y, 100);
      if (e) e->flags &= (uint16_t)~EF_BOSS, e->flags |= EF_NOKNOCK;
      return;
    default:
      e = boss_new(type, x, y, 100);
      return;
  }
}

float bosses_hp(float *max) {
  float h = 0, m = 0;
  for (int i = 0; i < MAX_ENTS; i++) {
    Ent *e = &ents[i];
    if (e->kind != EK_MON || !(e->flags & EF_BOSS) || (e->flags & EF_NOCOUNT)) continue;
    if (e->type == MON_MOM_HAND || e->type == MON_MOM_FOOT || e->type == MON_SATAN_STOMP || e->type == MON_DEATH_SCYTHE) continue;
    h += e->hp > 0 ? e->hp : 0, m += e->maxhp;
  }
  if (max) *max = m;
  return h;
}

/* ---------------------------------------------------------------- helpers */
static float toward(Ent *e, float *dx, float *dy) {
  float x = pl.x - e->x, y = pl.y - e->y, d = fsqrt(x * x + y * y);
  if (d < 0.01f) d = 0.01f;
  *dx = x / d, *dy = y / d;
  return d;
}
static void glide(Ent *e, float dx, float dy, float s, float acc, bool fly) {
  e->vx += (dx * s - e->vx) * acc, e->vy += (dy * s - e->vy) * acc;
  move_ent(&e->x, &e->y, &e->vx, &e->vy, e->r * 0.7f, fly, fly);
}
static void bounce(Ent *e, float s) {
  if (!e->vx && !e->vy) e->vx = s * (rnd(2) ? 1 : -1), e->vy = s * 0.8f * (rnd(2) ? 1 : -1);
  float ox = e->vx, oy = e->vy;
  move_ent(&e->x, &e->y, &e->vx, &e->vy, e->r * 0.7f, true, true);
  if (e->vx == 0) e->vx = -ox;
  if (e->vy == 0) e->vy = -oy;
}
/* a spray of shots at him, the way Monstro coughs them up */
static void spray(Ent *e, int n, float speed, float spread) {
  float dx, dy;
  toward(e, &dx, &dy);
  float a0 = fatan2(dy, dx);
  for (int i = 0; i < n; i++) {
    float a = a0 + (rndf() - 0.5f) * spread, s = speed * (0.7f + rndf() * 0.5f);
    Tear *t = enemy_shot(e->x, e->y - 12, fcos(a) * s, fsin(a) * s, 0);
    if (t) t->range = 90 + rnd(80);
  }
}
static int count_type(int type) {
  int n = 0;
  for (int i = 0; i < MAX_ENTS; i++) n += ents[i].kind == EK_MON && ents[i].type == type;
  return n;
}
static void random_spot(float *x, float *y, float away) {
  for (int k = 0; k < 30; k++) {
    float a = ROOM_X + 30 + rnd(200), b = ROOM_Y + 25 + rnd(90);
    if (!solid_at(a, b, false) && dist2(a, b, pl.x, pl.y) > away * away) {
      *x = a, *y = b;
      return;
    }
  }
  *x = 160, *y = 100;
}

/* the hop of Monstro and his kind: a short hop at him, a leap that lands on him, or a cough of blood */
static void jumper(Ent *e, int big_n, float leap_h, bool creep, bool laser) {
  float dx, dy, d = toward(e, &dx, &dy);
  switch (e->state) {
    case 0:   /* waiting */
      e->z = 0;
      if (--e->t > 0) break;
      {
        int r = rnd(10);
        if (r < 5) e->state = 1, e->t = e->t3 = 20, e->a = fatan2(dy, dx), e->b = d < 50 ? d : 50;
        else if (r < 7) e->state = 2, e->t = e->t3 = 50, e->flags |= EF_NOHIT;
        else e->state = 3, e->t = 30;
      }
      break;
    case 1: {  /* a hop */
      float k = 1 - (float)e->t / e->t3;
      e->z = -16 * 4 * k * (1 - k);
      e->vx = fcos(e->a) * e->b / e->t3, e->vy = fsin(e->a) * e->b / e->t3;
      move_ent(&e->x, &e->y, &e->vx, &e->vy, e->r * 0.7f, true, false);
      if (--e->t <= 0) e->state = 0, e->t = 15 + rnd(20), e->z = 0;
      break;
    }
    case 2: {  /* the leap: up out of sight, his shadow follows Isaac, down on him */
      float k = 1 - (float)e->t / e->t3;
      e->z = k < 0.5f ? -leap_h * k * 2 : -leap_h * (1 - k) * 2;
      if (k > 0.3f && k < 0.75f) e->x += (pl.x - e->x) * 0.1f, e->y += (pl.y - e->y) * 0.1f;
      if (--e->t <= 0) {
        e->state = 0, e->t = 25, e->z = 0, e->flags &= (uint16_t)~EF_NOHIT;
        g_shake = 8;
        enemy_shots_ring(e->x, e->y - 6, big_n, 3.2f, rndf());
        if (creep) creep_spawn(e->x, e->y, 22, 200, true, 0);
      }
      break;
    }
    case 3:   /* the cough */
      if (e->t == 12) {
        if (laser) laser_fire(e->x, e->y - 12, absf(pl.x - e->x) > absf(pl.y - e->y) ? (pl.x < e->x ? DIR_LEFT : DIR_RIGHT) : (pl.y < e->y ? DIR_UP : DIR_DOWN), 11, 0, 18, 0, true);
        else spray(e, 12, 3.8f, 0.9f);
      }
      if (--e->t <= 0) e->state = 0, e->t = 20 + rnd(20);
      break;
  }
}

/* the horsemen's charge across the room */
static void horseman(Ent *e) {
  float dx, dy;
  toward(e, &dx, &dy);
  switch (e->state) {
    case 0:   /* floats about; then a charge or its own attack */
      glide(e, fcos(e->a), fsin(e->a) * 0.5f, 1.2f, 0.05f, true);
      e->a += 0.03f;
      if (--e->t <= 0) {
        e->state = rnd(2) ? 1 : 2, e->t = 20;
        e->c = pl.y;
      }
      break;
    case 1:   /* to the side, at his height, then across */
      e->x += ((pl.x < 160 ? ROOM_R - 20 : ROOM_X + 20) - e->x) * 0.15f;
      e->y += (e->c - e->y) * 0.15f;
      if (--e->t <= 0) e->state = 3, e->vx = e->x < 160 ? 7 : -7, e->vy = 0, e->t = 50;
      break;
    case 2:   /* its own attack */
      if (e->t == 10) {
        switch (e->type) {
          case MON_FAMINE: for (int i = 0; i < 2; i++) mon_spawn(MON_ATTACK_FLY, e->x + i * 8, e->y); break;
          case MON_PESTILENCE: enemy_shots_ring(e->x, e->y - 10, 8, 3, 0); creep_spawn(e->x, e->y, 18, 200, true, 1); break;
          case MON_WAR: for (int i = 0; i < 2; i++) pickup_spawn(PK_TROLL_BOMB, 0, e->x + i * 20 - 10, e->y + 10, true); break;
          case MON_CONQUEST: for (int i = 0; i < 3; i++) {
              Laser *l = laser_fire(pl.x + (i - 1) * 40, pl.y, -1, 12, 0, 20, 2, true);
              (void)l;
            }
            break;
          case MON_DEATH: if (count_type(MON_DEATH_SCYTHE) < 4) for (int i = 0; i < 2; i++) boss_spawn(MON_DEATH_SCYTHE, e->x, e->y);
            break;
          default: spray(e, 8, 3.5f, 1.2f); break;
        }
      }
      if (--e->t <= 0) e->state = 0, e->t = 60 + rnd(40);
      break;
    case 3:   /* the charge */
      e->x += e->vx;
      if (e->x < ROOM_X - 30 || e->x > ROOM_R + 30 || --e->t <= 0) {
        e->x = e->x < 160 ? ROOM_X + 10 : ROOM_R - 10;
        e->state = 0, e->t = 50, e->vx = 0;
        if (e->type == MON_DEATH && e->hp < e->maxhp / 2) e->spr = SPR_DEATH_HORSE;
      }
      break;
  }
}

/* a worm of segments (Larry Jr., the Hollow): the head turns on the grid, the others follow its trail */
static void worm(Ent *e) {
  if (e->parent >= 0 && ents[e->parent].kind == EK_MON && ents[e->parent].type == e->type) {
    Ent *p = &ents[e->parent];
    float dx = p->x - e->x, dy = p->y - e->y, d = fsqrt(dx * dx + dy * dy);
    if (d > 11) e->x += dx / d * (d - 11), e->y += dy / d * (d - 11);
    return;
  }
  /* the head */
  e->parent = -1;
  int dd = (int)e->c;
  float s = e->type == MON_LARRY ? 2.2f : 2.6f;
  e->vx = DIR_X[dd] * s, e->vy = DIR_Y[dd] * s;
  float ox = e->x, oy = e->y;
  move_ent(&e->x, &e->y, &e->vx, &e->vy, 7, false, false);
  bool stuck = absf(e->x - ox) + absf(e->y - oy) < 0.5f;
  if (stuck || --e->t <= 0) {
    int nd = rnd(4);
    if (nd == (dd + 2) % 4) nd = (nd + 1) % 4;
    float pdx = pl.x - e->x, pdy = pl.y - e->y;
    if (rnd(2)) nd = absf(pdx) > absf(pdy) ? (pdx < 0 ? DIR_LEFT : DIR_RIGHT) : (pdy < 0 ? DIR_UP : DIR_DOWN);
    e->c = (float)nd, e->t = 20 + rnd(30);
  }
  if (rnd(60) == 0 && e->type == MON_LARRY) enemy_shot_at(e->x, e->y - 6, 3.5f);
}

/* burrowing worms (Pin, Scolex, the Frail, Polycephalus, Mr. Fred): under the floor, up by him, a spit, down */
static void burrower(Ent *e) {
  switch (e->state) {
    case 0:
      e->flags |= EF_HIDDEN | EF_NOHIT | EF_INVULN;
      if (--e->t <= 0) {
        random_spot(&e->x, &e->y, 40);
        if (rnd(2)) e->x = pl.x + (rnd(2) ? 50 : -50), e->y = pl.y;
        e->x = clampf(e->x, ROOM_X + 20, ROOM_R - 20), e->y = clampf(e->y, ROOM_Y + 20, ROOM_B - 15);
        e->state = 1, e->t = 50;
        e->flags &= (uint16_t)~(EF_HIDDEN | EF_NOHIT | EF_INVULN);
        g_shake = 4;
      }
      break;
    case 1:
      e->z = e->t > 40 ? -(50 - e->t) * 2.0f : e->t < 10 ? -e->t * 2.0f : -20;
      if (e->t == 30) {
        if (e->type == MON_POLYCEPHALUS || e->type == MON_MR_FRED) enemy_shots_ring(e->x, e->y - 14, 10, 3, rndf());
        else if (e->type == MON_FRAIL) laser_fire(e->x, e->y - 14, pl.x < e->x ? DIR_LEFT : DIR_RIGHT, 9, 0, 16, 0, true);
        else spray(e, 6, 3.5f, 0.6f);
      }
      if (--e->t <= 0) e->state = 0, e->t = 25 + rnd(30), e->z = 0;
      break;
  }
}

/* ---------------------------------------------------------------- Mom */
static void mom_update(Ent *e) {
  e->flags |= EF_HIDDEN | EF_NOHIT;
  if (--e->t > 0) return;
  int r = rnd(10);
  /* what comes next: her eye in a door, her hand reaching, her foot, or her friends */
  static const float wx[4] = {160, ROOM_X - 2, 160, ROOM_R + 2}, wy[4] = {ROOM_B + 4, 120, ROOM_Y - 2, 120};
  int w = rnd(4);
  if (r < 4) {
    Ent *f = boss_new(MON_MOM_FOOT, pl.x, pl.y, 100);
    if (f) f->parent = (int8_t)(e - ents), f->state = 0, f->t = 30, f->z = -200, f->flags |= EF_NOHIT | EF_NOCOUNT, f->hp = f->maxhp = 1000;
    e->t = 70;
  } else if (r < 7) {
    Ent *f = boss_new(MON_MOM, wx[w], wy[w], 100);
    if (f) f->parent = (int8_t)(e - ents), f->sub = (uint8_t)(1 + w), f->t = 70, f->flags |= EF_NOCOUNT, f->hp = f->maxhp = 1000;
    e->t = 80;
  } else if (r < 9) {
    Ent *f = boss_new(MON_MOM_HAND, wx[w], wy[w], 100);
    if (f) f->parent = (int8_t)(e - ents), f->sub = (uint8_t)w, f->t = 60, f->flags |= EF_NOCOUNT, f->hp = f->maxhp = 1000;
    e->t = 70;
  } else {
    static const uint8_t friends[] = {MON_GAPER, MON_HOPPER, MON_ATTACK_FLY, MON_POOTER, MON_SPITTY, MON_BONY};
    for (int i = 0; i < 2; i++) mon_spawn(friends[rnd(sizeof friends)], wx[w] + (w & 1 ? (w == 1 ? 14 : -14) : 0), wy[w] + (w & 1 ? 0 : (w == 0 ? -14 : 14)));
    e->t = 60;
  }
}

/* a part of Mom: hurting it hurts her */
static Ent *mom_of(Ent *e) {
  if (e->parent >= 0 && ents[e->parent].kind == EK_MON && ents[e->parent].type == MON_MOM && ents[e->parent].sub == 0) return &ents[e->parent];
  return NULL;
}

/* ---------------------------------------------------------------- each boss */
bool boss_update(Ent *e) {
  if (e->type < MON_MONSTRO) return false;
  float dx, dy, d = toward(e, &dx, &dy);
  /* parts pass their hurt on */
  Ent *mom = (e->type == MON_MOM && e->sub) || e->type == MON_MOM_HAND || e->type == MON_MOM_FOOT ? mom_of(e) : NULL;
  switch (e->type) {
    case MON_MONSTRO: jumper(e, 8, 90, false, false); break;
    case MON_MONSTRO2: jumper(e, 10, 100, false, rnd(3) == 0 && e->state == 0); break;
    case MON_GISH: jumper(e, 10, 90, true, false); break;
    case MON_MEGA_FATTY:
      glide(e, dx, dy, 0.7f, 0.05f, false);
      if (--e->t <= 0) {
        e->t = 70;
        if (rnd(2)) enemy_shots_ring(e->x, e->y - 14, 10, 2.8f, 0);
        else mon_spawn(MON_ATTACK_FLY, e->x, e->y - 10), mon_spawn(MON_ATTACK_FLY, e->x + 6, e->y - 10);
      }
      break;
    case MON_LARRY: case MON_THE_HOLLOW: worm(e); break;
    case MON_CHUB: case MON_CARRION_QUEEN: {
      if (e->parent >= 0 && ents[e->parent].kind == EK_MON && ents[e->parent].type == e->type) {
        Ent *p = &ents[e->parent];
        float px = p->x - e->x, py = p->y - e->y, pd = fsqrt(px * px + py * py);
        if (pd > 16) e->x += px / pd * (pd - 16), e->y += py / pd * (pd - 16);
        break;
      }
      e->parent = -1;
      if (e->state == 0) {
        glide(e, fcos(e->a), fsin(e->a), 1.4f, 0.1f, false);
        if (e->vx == 0 || e->vy == 0) e->a += 1.4f;
        if (--e->t <= 0 && (absf(pl.x - e->x) < 12 || absf(pl.y - e->y) < 12)) {
          e->state = 1, e->t = 40;
          e->a = absf(pl.x - e->x) > absf(pl.y - e->y) ? (pl.x < e->x ? 3.14159f : 0) : (pl.y < e->y ? -1.5708f : 1.5708f);
        } else if (e->t <= -60) e->t = 0;
      } else {
        e->vx = fcos(e->a) * 6, e->vy = fsin(e->a) * 6;
        move_ent(&e->x, &e->y, &e->vx, &e->vy, e->r * 0.7f, false, false);
        if (e->type == MON_CARRION_QUEEN && (e->t & 3) == 0) creep_spawn(e->x, e->y, 8, 90, true, 0);
        if (--e->t <= 0 || (e->vx == 0 && e->vy == 0)) e->state = 0, e->t = 50, e->a = rndf() * 6.28f;
      }
      break;
    }
    case MON_DUKE: case MON_HUSK:   /* bounces about; flies out of it, or blown at him */
      bounce(e, 0.9f);
      if (--e->t <= 0) {
        e->t = 70 + rnd(40);
        int flies = count_type(MON_ATTACK_FLY) + count_type(MON_FLY);
        if (flies < 6) for (int i = 0; i < (e->type == MON_HUSK ? 3 : 2); i++) mon_spawn(rnd(2) ? MON_ATTACK_FLY : MON_FLY, e->x + rnd(16) - 8, e->y + 6);
        else for (int i = 0; i < MAX_ENTS; i++)
            if (ents[i].kind == EK_MON && (ents[i].type == MON_FLY || ents[i].type == MON_ATTACK_FLY)) ents[i].vx = dx * 5, ents[i].vy = dy * 5;
        if (e->type == MON_HUSK) spray(e, 6, 3.5f, 0.8f);
      }
      break;
    case MON_GEMINI: case MON_STEVEN: case MON_BLIGHTED_OVUM: {   /* the big one chases; it goes wild when alone */
      bool alone = e->child < 0 || ents[e->child].kind != EK_MON;
      float px, py;
      path_dir(e->x, e->y, pl.x, pl.y, false, &px, &py);
      glide(e, px, py, alone ? 2.6f : 1.5f, 0.12f, false);
      if (e->type == MON_STEVEN && --e->t <= 0) e->t = 60, enemy_shot_at(e->x, e->y - 10, 4);
      break;
    }
    case MON_GEMINI_BABY: case MON_STEVEN_BABY: {   /* the little one, on its cord, shoots */
      Ent *p = e->parent >= 0 && ents[e->parent].kind == EK_MON ? &ents[e->parent] : NULL;
      if (p) {
        float ox = e->x - p->x, oy = e->y - p->y, od = fsqrt(ox * ox + oy * oy);
        glide(e, -dx * 0.3f + fcos(e->a), -dy * 0.3f + fsin(e->a), 1.6f, 0.06f, true);
        e->a += 0.05f;
        if (od > 60) e->x = p->x + ox / od * 60, e->y = p->y + oy / od * 60;
      } else
        glide(e, dx, dy, 2.2f, 0.08f, true);
      if (--e->t <= 0) e->t = 45, enemy_shot_at(e->x, e->y - 8, 3.8f);
      break;
    }
    case MON_GURDY: case MON_MAMA_GURDY:   /* sits; spits spreads of blood, calls Pooters and flies */
      if (--e->t <= 0) {
        e->t = 55 + rnd(30);
        int r = rnd(3);
        if (r == 0) spray(e, 6, 3.6f, 0.6f);
        else if (r == 1) enemy_shots_ring(e->x, e->y - 20, 8, 3, rndf());
        else if (count_type(MON_POOTER) + count_type(MON_ATTACK_FLY) < 4) mon_spawn(rnd(2) ? MON_POOTER : MON_ATTACK_FLY, e->x + rnd(30) - 15, e->y + 16);
      }
      break;
    case MON_GURDY_JR: case MON_DINGLE: case MON_DARK_ONE:   /* charges at him; spits */
      if (e->state == 0) {
        glide(e, fcos(e->a), fsin(e->a), 0.8f, 0.1f, false);
        e->a += 0.04f;
        if (--e->t <= 0) {
          e->state = 1, e->t = 25;
          e->vx = dx * 5.5f, e->vy = dy * 5.5f;
        }
      } else {
        move_ent(&e->x, &e->y, &e->vx, &e->vy, e->r * 0.7f, false, false);
        if (--e->t <= 0 || (e->vx == 0 && e->vy == 0)) {
          e->state = 0, e->t = 40 + rnd(30);
          if (e->type == MON_DINGLE && count_type(MON_DIP) < 4) mon_spawn(MON_DIP, e->x, e->y + 8);
          else if (e->type == MON_DARK_ONE) enemy_shots_ring(e->x, e->y - 10, 8, 3, 0);
          else spray(e, 5, 3.5f, 0.5f);
        }
      }
      break;
    case MON_PEEP: case MON_BLOAT:   /* bounces; pees creep; shoots */
      bounce(e, 1.1f);
      if ((g_frame & 15) == 0) creep_spawn(e->x, e->y, 10, 150, true, 2);
      if (--e->t <= 0) {
        e->t = 50 + rnd(30);
        if (e->type == MON_BLOAT && rnd(2)) laser_fire(e->x, e->y - 12, DIR_DOWN, 9, 0, 18, 0, true);
        else enemy_shots_ring(e->x, e->y - 12, 6, 3, rndf());
      }
      break;
    case MON_LOKI: case MON_LOKII:   /* gone, back elsewhere, shots four or eight ways */
      if (e->state == 0) {
        if (--e->t <= 0) e->state = 1, e->t = 14;
      } else if (e->state == 1) {
        e->alpha = (uint8_t)(e->t * 2);
        if (--e->t <= 0) random_spot(&e->x, &e->y, 50), e->state = 2, e->t = 14;
      } else {
        e->alpha = (uint8_t)(32 - e->t * 2);
        if (--e->t <= 0) {
          e->alpha = 32, e->state = 0, e->t = 40 + rnd(30);
          int r = rnd(3);
          if (r == 0) enemy_shots_ring(e->x, e->y - 10, 4, 3.5f, 0);
          else if (r == 1) enemy_shots_ring(e->x, e->y - 10, 8, 3, 0.39f);
          else if (count_type(MON_ATTACK_FLY) < 3) mon_spawn(MON_ATTACK_FLY, e->x, e->y), mon_spawn(MON_ATTACK_FLY, e->x + 6, e->y);
        }
      }
      break;
    case MON_FAMINE: case MON_PESTILENCE: case MON_WAR: case MON_CONQUEST: case MON_DEATH: case MON_HEADLESS_HORSEMAN:
      horseman(e);
      break;
    case MON_DEATH_SCYTHE:   /* Death's scythes: they fly at him */
      e->flags |= EF_NOCOUNT | EF_INVULN;
      glide(e, dx, dy, 2.6f, 0.05f, true);
      if (count_type(MON_DEATH) == 0 || ++e->t3 > 400) e->kind = EK_NONE;
      break;
    case MON_PIN: case MON_SCOLEX: case MON_FRAIL: case MON_POLYCEPHALUS: case MON_MR_FRED:
      burrower(e);
      break;
    case MON_MASK_OF_INFAMY:   /* the mask charges, the heart behind it shoots; the heart is what dies */
      e->flags |= EF_INVULN | EF_NOCOUNT;
      if (e->parent < 0 || ents[e->parent].kind != EK_MON) {
        e->kind = EK_NONE;
        break;
      }
      if (e->state == 0) {
        glide(e, dx, dy, 1.2f, 0.08f, false);
        if (--e->t <= 0 && (absf(pl.x - e->x) < 10 || absf(pl.y - e->y) < 10)) e->state = 1, e->t = 30, e->vx = dx * 6, e->vy = dy * 6;
        if (e->t < -60) e->t = 0;
      } else {
        move_ent(&e->x, &e->y, &e->vx, &e->vy, e->r * 0.7f, false, false);
        if (--e->t <= 0 || (!e->vx && !e->vy)) e->state = 0, e->t = 40;
      }
      break;
    case MON_HEART_OF_INFAMY:
      glide(e, -dx + fcos(e->a), -dy + fsin(e->a), 1.0f, 0.06f, false);
      e->a += 0.05f;
      if (--e->t <= 0) e->t = 60, spray(e, 4, 3.4f, 0.5f);
      break;
    case MON_WIDOW: case MON_WRETCHED:   /* jumps at him; spiders; a web of creep */
      if (e->state == 0) {
        glide(e, dx, dy, 0.8f, 0.05f, false);
        if (--e->t <= 0) {
          int r = rnd(3);
          if (r == 0) e->state = 1, e->t = e->t3 = 30, e->a = fatan2(dy, dx), e->b = d < 80 ? d : 80;
          else if (r == 1 && count_type(MON_SPIDER) < 4) mon_spawn(MON_SPIDER, e->x, e->y + 8), mon_spawn(MON_SPIDER, e->x + 6, e->y + 8), e->t = 50;
          else spray(e, 6, 3.4f, 0.7f), creep_spawn(e->x, e->y, 20, 200, true, 3), e->t = 50;
        }
      } else {
        float k = 1 - (float)e->t / e->t3;
        e->z = -40 * 4 * k * (1 - k);
        e->vx = fcos(e->a) * e->b / e->t3, e->vy = fsin(e->a) * e->b / e->t3;
        move_ent(&e->x, &e->y, &e->vx, &e->vy, e->r * 0.6f, true, false);
        if (--e->t <= 0) e->state = 0, e->t = 40, e->z = 0;
      }
      break;
    case MON_DADDY_LONG_LEGS: case MON_TRIACHNID:   /* stomps where he stands, again and again */
      if (e->state == 0) {
        e->z = -200, e->flags |= EF_NOHIT;
        e->x += (pl.x - e->x) * 0.08f, e->y += (pl.y - e->y) * 0.08f;
        if (--e->t <= 0) e->state = 1, e->t = 12;
      } else if (e->state == 1) {
        e->z += 18;
        if (e->z >= 0) {
          e->z = 0, e->state = 2, e->t = 40, g_shake = 6, e->flags &= (uint16_t)~EF_NOHIT;
          if (e->type == MON_TRIACHNID) enemy_shots_ring(e->x, e->y - 10, 8, 3, 0);
          else if (rnd(2)) mon_spawn(MON_SPIDER, e->x, e->y + 10);
        }
      } else {
        if (--e->t <= 0) e->state = 3, e->t = 15;
      }
      if (e->state == 3) {
        e->z -= 14;
        if (--e->t <= 0) e->state = 0, e->t = 30 + rnd(20);
      }
      break;
    case MON_FISTULA: case MON_TERATOMA: case MON_BLASTOCYST:   /* bounce about; split when beaten */
      if (e->type == MON_BLASTOCYST) {
        if (e->state == 0) {
          e->z = 0;
          if (--e->t <= 0) e->state = 1, e->t = e->t3 = 24, e->a = fatan2(dy, dx), e->b = d < 60 ? d : 60;
        } else {
          float k = 1 - (float)e->t / e->t3;
          e->z = -30 * 4 * k * (1 - k);
          e->vx = fcos(e->a) * e->b / e->t3, e->vy = fsin(e->a) * e->b / e->t3;
          move_ent(&e->x, &e->y, &e->vx, &e->vy, e->r * 0.6f, true, false);
          if (--e->t <= 0) e->state = 0, e->t = 20 + rnd(20), e->z = 0, enemy_shots_ring(e->x, e->y - 8, 6 - e->sub * 2, 3, 0);
        }
      } else
        bounce(e, 1.2f + e->sub * 0.5f);
      break;
    case MON_HAUNT:   /* drifts; its Lil Haunts first; then its own beam */
      glide(e, fcos(e->a), 0.3f * fsin(e->a * 2), 1.0f, 0.05f, true);
      e->a += 0.02f;
      e->flags = (uint16_t)((e->flags & ~EF_INVULN) | (count_type(MON_LIL_HAUNT) ? EF_INVULN : 0));
      if (!count_type(MON_LIL_HAUNT) && --e->t <= 0) {
        e->t = 70;
        if (rnd(2)) laser_fire(e->x, e->y - 10, DIR_DOWN, 10, 0, 20, 0, true);
        else spray(e, 6, 3.4f, 0.8f);
      }
      break;
    case MON_LIL_HAUNT: {
      Ent *h = NULL;
      for (int i = 0; i < MAX_ENTS; i++)
        if (ents[i].kind == EK_MON && ents[i].type == MON_HAUNT) h = &ents[i];
      if (h) {
        float a = g_frame * 0.05f + e->sub * 2.094f;
        e->x = h->x + fcos(a) * 30, e->y = h->y + fsin(a) * 18;
      } else
        glide(e, dx, dy, 1.4f, 0.08f, true);
      if (--e->t <= 0) e->t = 60 + rnd(40), enemy_shot_at(e->x, e->y - 8, 3.4f);
      break;
    }
    case MON_MOM:
      if (e->sub == 0) {
        mom_update(e);
        break;
      }
      /* her eye in a door: it looks at him and shoots */
      e->flags &= (uint16_t)~(EF_HIDDEN | EF_NOHIT);
      if (e->t == 40 || e->t == 20) spray(e, 3, 3.6f, 0.4f);
      if (--e->t <= 0) e->kind = EK_NONE;
      break;
    case MON_MOM_HAND:   /* her hand comes in from a door at him */
      if (e->t > 20) glide(e, dx, dy, 1.6f, 0.1f, true);
      if (--e->t <= 0) e->kind = EK_NONE;
      break;
    case MON_MOM_FOOT: case MON_SATAN_STOMP:   /* a foot comes down where he stands */
      if (e->state == 0) {
        e->x += (pl.x - e->x) * 0.12f, e->y += (pl.y - e->y) * 0.12f;
        if (--e->t <= 0) e->state = 1;
      } else if (e->state == 1) {
        e->z += 25;
        if (e->z >= 0) {
          e->z = 0, e->state = 2, e->t = 22, g_shake = 10, e->flags &= (uint16_t)~EF_NOHIT;
          fx_spawn(FX_SHOCKWAVE, e->x, e->y, 14);
        }
      } else if (e->state == 2) {
        if (--e->t <= 0) e->state = 3, e->flags |= EF_NOHIT;
      } else {
        e->z -= 25;
        if (e->z < -220) e->kind = EK_NONE;
      }
      break;
    case MON_MOMS_HEART: case MON_IT_LIVES:   /* beats; showers of blood; her womb's friends */
      if (--e->t <= 0) {
        e->t = 50 + rnd(20);
        int r = rnd(4);
        if (r == 0) enemy_shots_ring(e->x, e->y - 20, 12, 3, rndf());
        else if (r == 1) spray(e, 10, 3.8f, 1.0f);
        else if (r == 2 && count_type(MON_FISTULA) + count_type(MON_ATTACK_FLY) < 3) mon_spawn(MON_ATTACK_FLY, e->x - 20, e->y), mon_spawn(MON_ATTACK_FLY, e->x + 20, e->y);
        else for (int i = 0; i < 3; i++) laser_fire(pl.x + (i - 1) * 30 + rnd(10), pl.y, -1, 10, 0, 20, 0, true);
      }
      e->flags = (uint16_t)((e->flags & ~EF_INVULN) | (e->t > 40 ? EF_INVULN * 0 : 0));
      break;
    case MON_SATAN:   /* the Fallen first; then Satan's own hands and hoof */
      if (e->state == 0) {
        glide(e, fcos(e->a), fsin(e->a) * 0.4f, 1.2f, 0.04f, true);
        e->a += 0.02f;
        if (--e->t <= 0) {
          e->t = 60;
          if (rnd(2)) laser_fire(e->x, e->y - 14, dx * dx > dy * dy ? (dx < 0 ? DIR_LEFT : DIR_RIGHT) : (dy < 0 ? DIR_UP : DIR_DOWN), 12, 0, 22, 0, true);
          else enemy_shots_ring(e->x, e->y - 14, 12, 3, rndf());
        }
        if (e->hp < e->maxhp / 2) e->state = 1, e->spr = SPR_SATAN, e->t = 60, g_shake = 15;
      } else {
        e->x += (160 - e->x) * 0.05f, e->y += (85 - e->y) * 0.05f;
        if (--e->t <= 0) {
          e->t = 70;
          int r = rnd(3);
          if (r == 0) { Ent *f = boss_new(MON_SATAN_STOMP, pl.x, pl.y, 100); if (f) f->t = 30, f->z = -200, f->flags |= EF_NOHIT | EF_NOCOUNT | EF_INVULN; }
          else if (r == 1) for (int i = 0; i < 2; i++) laser_fire(e->x + (i ? 40 : -40), e->y, DIR_DOWN, 12, 0, 22, 0, true);
          else spray(e, 14, 3.8f, 1.4f);
        }
      }
      break;
    case MON_ISAAC:   /* Isaac, in the Cathedral: shots, then light from the sky, then wings */
      glide(e, fcos(e->a) * 0.4f, fsin(e->a) * 0.3f, 0.6f, 0.05f, false);
      e->a += 0.02f;
      if (--e->t <= 0) {
        e->t = e->hp > e->maxhp * 2 / 3 ? 50 : 40;
        int phase = e->hp > e->maxhp * 2 / 3 ? 0 : e->hp > e->maxhp / 3 ? 1 : 2;
        if (phase == 0) spray(e, 8, 3.5f, 1.0f);
        else if (phase == 1) for (int i = 0; i < 4; i++) laser_fire(ROOM_X + 30 + rnd(220), ROOM_Y + 20 + rnd(110), -1, 12, 0, 22, 2, true);
        else enemy_shots_ring(e->x, e->y - 12, 14, 3.2f, rndf()), mon_spawn(MON_ANGELIC_BABY, e->x, e->y);
      }
      break;
    case MON_KRAMPUS: case MON_FALLEN:
      glide(e, dx, dy, 1.0f, 0.05f, e->type == MON_FALLEN);
      if (--e->t <= 0) {
        e->t = 60;
        if (rnd(2)) laser_fire(e->x, e->y - 12, dx * dx > dy * dy ? (dx < 0 ? DIR_LEFT : DIR_RIGHT) : (dy < 0 ? DIR_UP : DIR_DOWN), 11, 0, 20, 0, true);
        else spray(e, 8, 3.6f, 1.0f);
      }
      break;
    case MON_GURGLING:
      glide(e, dx, dy, 1.6f, 0.08f, false);
      if ((g_frame & 7) == 0) creep_spawn(e->x, e->y, 8, 120, true, 0);
      break;
    /* the seven deadly sins */
    case MON_SLOTH:
      glide(e, dx, dy, 0.6f, 0.05f, false);
      if (--e->t <= 0) e->t = 70, spray(e, 3, 2.6f, 0.4f), creep_spawn(e->x, e->y, 12, 150, true, 1);
      break;
    case MON_LUST: {
      float px, py;
      path_dir(e->x, e->y, pl.x, pl.y, false, &px, &py);
      glide(e, px, py, 2.6f, 0.12f, false);
      break;
    }
    case MON_WRATH:
      glide(e, dx, dy, 1.3f, 0.08f, false);
      if (--e->t <= 0) e->t = 60, bomb_place(e->x, e->y + 6, 1);
      break;
    case MON_GLUTTONY:
      glide(e, dx, dy, 0.8f, 0.06f, false);
      if (--e->t <= 0) {
        e->t = 60;
        if (absf(pl.x - e->x) < 12 || absf(pl.y - e->y) < 12)
          laser_fire(e->x, e->y - 10, absf(pl.x - e->x) > absf(pl.y - e->y) ? (pl.x < e->x ? DIR_LEFT : DIR_RIGHT) : (pl.y < e->y ? DIR_UP : DIR_DOWN), 9, 0, 18, 0, true);
        else spray(e, 5, 3, 0.5f), creep_spawn(e->x, e->y, 12, 150, true, 0);
      }
      break;
    case MON_GREED:
      glide(e, -dx * 0.3f + fcos(e->a), -dy * 0.3f + fsin(e->a), 1.2f, 0.05f, false);
      e->a += 0.03f;
      if (--e->t <= 0) e->t = 50, spray(e, 3, 3.8f, 0.3f);
      break;
    case MON_ENVY:
      bounce(e, 1.4f + e->sub * 0.6f);
      break;
    case MON_PRIDE:
      glide(e, dx, dy, 1.1f, 0.06f, false);
      if (--e->t <= 0) {
        e->t = 60;
        if (rnd(2)) laser_fire(e->x, e->y - 10, rnd(4), 7, 0, 18, 0, true);
        else enemy_shots_ring(e->x, e->y - 10, 6, 3, rndf());
      }
      break;
    default: break;
  }
  /* the parts of Mom take the hurt for her */
  if (mom && e->hp < e->maxhp) {
    mom->hp -= e->maxhp - e->hp, e->hp = e->maxhp;
    mom->flash = 3;
    if (mom->hp <= 0) ent_kill(mom);
  }
  (void)d;
  return true;
}

bool boss_on_death(Ent *e) {
  if (e->type < MON_MONSTRO) return false;
  switch (e->type) {
    case MON_FISTULA: case MON_TERATOMA: case MON_BLASTOCYST: case MON_ENVY:
      if (e->sub < 2) {   /* in two, smaller */
        static const int16_t spr_fis[3] = {SPR_FISTULA_BIG, SPR_FISTULA_MED, SPR_FISTULA_SMALL};
        static const int16_t spr_bla[3] = {SPR_BLASTOCYST_BIG, SPR_BLASTOCYST_MED, SPR_BLASTOCYST_SMALL};
        static const int16_t spr_env[3] = {SPR_ENVY, SPR_ENVY_HALF, SPR_ENVY_QUARTER};
        for (int i = 0; i < 2; i++) {
          Ent *c = ent_new(EK_MON, e->x + (i ? 8 : -8), e->y);
          if (!c) break;
          *c = *e;
          c->x = e->x + (i ? 8 : -8);
          c->sub = (uint8_t)(e->sub + 1);
          c->maxhp = c->hp = e->maxhp * 0.45f;
          c->r = e->r * 0.65f;
          c->spr = e->type == MON_BLASTOCYST ? spr_bla[c->sub] : e->type == MON_ENVY ? spr_env[c->sub] : spr_fis[c->sub];
          c->vx = i ? 2 : -2, c->vy = (rnd(2) ? 1 : -1) * 1.5f, c->state = 0, c->t = 20, c->z = 0;
        }
      }
      break;
    case MON_HEART_OF_INFAMY:
      for (int i = 0; i < MAX_ENTS; i++)
        if (ents[i].kind == EK_MON && ents[i].type == MON_MASK_OF_INFAMY) ents[i].kind = EK_NONE;
      break;
    case MON_LARRY: case MON_THE_HOLLOW: case MON_CHUB: case MON_CARRION_QUEEN:
      /* (the segment after it leads now) */
      if (e->child >= 0 && ents[e->child].kind == EK_MON) ents[e->child].parent = -1;
      if (e->parent >= 0 && ents[e->parent].kind == EK_MON) ents[e->parent].child = -1;
      break;
    case MON_MOM:
      if (e->sub == 0)   /* Mom dead: her parts go with her */
        for (int i = 0; i < MAX_ENTS; i++)
          if (ents[i].kind == EK_MON && (ents[i].type == MON_MOM_FOOT || ents[i].type == MON_MOM_HAND || ents[i].type == MON_MOM)) ents[i].kind = EK_NONE;
      break;
    default: break;
  }
  decal_blood(e->x, e->y, e->r * 1.3f);
  for (int i = 0; i < 6; i++) fx_spawn(FX_GIB, e->x, e->y - 6, 16 + rnd(12));
  e->kind = EK_NONE;
  g_shake = 10;
  return true;
}

void boss_draw(Ent *e) {
  int x = (int)e->x, y = (int)(e->y + e->z), flags = 0;
  if (e->flags & EF_HIDDEN) {
    if (e->type >= MON_PIN && e->type <= MON_FRAIL) gfx_ellipse(x, (int)e->y, 8, 3, rgb565(0x3A2010), 18);
    if (e->type == MON_POLYCEPHALUS || e->type == MON_MR_FRED) gfx_ellipse(x, (int)e->y, 10, 4, rgb565(0x3A2010), 18);
    return;
  }
  if (e->vx < -0.1f || (e->type >= MON_FAMINE && e->type <= MON_DEATH && e->vx < 0)) flags = GFX_FLIPX;
  uint16_t tint = 0;
  int amount = 0;
  if (e->flash) tint = rgb565(0xFF5050), amount = e->flash > 1 ? 12 : 6;
  else if (e->freeze) tint = rgb565(0xC0C0C0), amount = 24;
  else if (e->poison) tint = rgb565(0x40C040), amount = 12;
  int sx = 256, sy = 256;
  if (e->type == MON_MOMS_HEART || e->type == MON_IT_LIVES) {   /* a heartbeat */
    int k = (int)(g_frame % 20);
    sy = sx = 256 + (k < 4 ? k * 6 : 0);
  }
  if (e->type == MON_MOM_FOOT || e->type == MON_SATAN_STOMP || e->type == MON_DADDY_LONG_LEGS || e->type == MON_TRIACHNID) {
    gfx_ellipse(x, (int)e->y, (int)e->r, (int)e->r / 3, C_BLACK, e->z < -100 ? 10 : 16);   /* its shadow, coming */
  }
  if (e->type == MON_MOM && e->sub) {   /* her eye peers in from the wall */
    static const uint8_t fl_of[5] = {0, GFX_FLIPY, 0, 0, GFX_FLIPX};
    (void)fl_of;
    gfx_sprite_tint(e->spr, 0, x, y + 16, 0, tint, amount, 32);
    return;
  }
  if (e->type == MON_DEATH_SCYTHE) {
    gfx_sprite_scaled(SPR_BONE_SHOT, 0, x, y - 8, 384, 384, (g_frame & 4) ? GFX_FLIPX : 0, rgb565(0x606070), 16, 32);
    return;
  }
  gfx_sprite_scaled(e->spr, 0, x, y + 3, sx, sy, flags, tint, amount, e->alpha);
}
