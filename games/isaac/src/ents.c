/* The room's things: making them, updating them, hurting monsters, and
 * drawing all that stands in the room by its depth, Isaac among it. */
#include <string.h>
#include "game.h"
#include "gfx.h"
#include "res.h"

Ent ents[MAX_ENTS];

void grid_draw_row(int r);
void pedestal_update(Ent *e);
void pedestal_draw(Ent *e);
void pickup_draw(Ent *e);
void machine_draw(Ent *e);
void fx_update(Ent *e);
void fx_draw(Ent *e);
void bomb_update(Ent *e);
void creep_update(Ent *e);
void trapdoor_update(Ent *e);

Ent *ent_new(int kind, float x, float y) {
  for (int i = 0; i < MAX_ENTS; i++)
    if (!ents[i].kind) {
      Ent *e = &ents[i];
      memset(e, 0, sizeof *e);
      e->kind = (uint8_t)kind, e->x = x, e->y = y, e->alpha = 32, e->parent = e->child = -1;
      return e;
    }
  return NULL;
}

void ents_clear(void) { memset(ents, 0, sizeof ents); }

/* monsters that keep the doors shut (stone turrets, Poky and Death's Heads don't) */
int ents_alive_monsters(void) {
  int n = 0;
  for (int i = 0; i < MAX_ENTS; i++) {
    Ent *e = &ents[i];
    if (e->kind == EK_MON && !(e->flags & (EF_FRIEND | EF_NOCOUNT))) n++;
  }
  return n;
}

Ent *ent_nearest_monster(float x, float y, float maxd) {
  Ent *best = NULL;
  float bd = maxd * maxd;
  for (int i = 0; i < MAX_ENTS; i++) {
    Ent *e = &ents[i];
    if (e->kind != EK_MON || (e->flags & (EF_FRIEND | EF_HIDDEN | EF_INVULN))) continue;
    float d = dist2(x, y, e->x, e->y);
    if (d < bd) bd = d, best = e;
  }
  return best;
}

void ent_kill(Ent *e) {
  if (e->kind != EK_MON) return;
  if (boss_on_death(e)) return;
  if (!mon_on_death(e)) return;
  bool flying = e->flags & EF_FLYING;
  decal_blood(e->x, e->y, e->r * (flying ? 0.8f : 1.2f));
  for (int i = 0; i < 3 + (int)(e->r / 4); i++) fx_spawn(FX_GIB, e->x, e->y - 4, 16 + rnd(10));
  if (e->flags & EF_CHAMPION) pickup_drop_random(e->x, e->y);
  /* Charm of the Vampire: half a heart every thirteen kills */
  if (pl.has[62] && ++pl.vampire >= 13) pl.vampire = 0, player_heal(1);
  e->kind = EK_NONE;
}

void ent_damage(Ent *e, float dmg, uint32_t flags, float kx, float ky) {
  (void)flags;
  if (e->kind != EK_MON || (e->flags & EF_INVULN) || e->hp <= 0) return;
  e->hp -= dmg;
  e->flash = 3;
  if (!(e->flags & (EF_BOSS | EF_NOKNOCK))) e->vx += kx, e->vy += ky;
  if (e->hp <= 0) ent_kill(e);
}

/* ---------------------------------------------------------------- updating */
static void touch_player(Ent *e) {
  if (e->flags & (EF_FRIEND | EF_HIDDEN | EF_NOHIT) || e->z < -12) return;
  float rr = e->r + 5, dx = pl.x - e->x, dy = pl.y - e->y;
  if (dx * dx + dy * dy < rr * rr) {
    if (pl.unicorn > 0 || pl.nail > 0) {   /* (Unicorn, the Nail: he hurts them instead) */
      ent_damage(e, 40.0f / FPS * 3, 0, -dx * 0.2f, -dy * 0.2f);
      return;
    }
    /* half a heart, a whole one from the Womb (and from the Depths' bosses) */
    player_hurt(fl.stage >= 7 || ((e->flags & EF_BOSS) && fl.stage >= 5) ? 2 : 1, e->type);
  }
}

void ents_update(void) {
  for (int i = 0; i < MAX_ENTS; i++) {
    Ent *e = &ents[i];
    switch (e->kind) {
      case EK_MON:
        if (e->flash) e->flash--;
        /* poison, slowness, freezing: what tears did */
        if (e->poison) {
          e->poison--;
          if (e->poison % 15 == 0) ent_damage(e, e->poison_dmg > 0 ? e->poison_dmg : 2, 0, 0, 0);
          if (e->kind != EK_MON) break;
        }
        if (e->freeze) {
          e->freeze--;
          touch_player(e);
          break;
        }
        if (e->slow) e->slow--;
        if (e->confuse) e->confuse--;
        if (e->fear) e->fear--;
        if (pl.hourglass > 0 && (g_frame & 1)) break;
        if (e->slow && (g_frame & 1)) break;
        if (!boss_update(e)) mon_update(e);
        if (e->kind == EK_MON) touch_player(e);
        break;
      case EK_PICKUP: pickup_update(e); break;
      case EK_PEDESTAL: pedestal_update(e); break;
      case EK_MACHINE: machine_update(e); break;
      case EK_BOMB: bomb_update(e); break;
      case EK_FX: fx_update(e); break;
      case EK_FAMILIAR: familiar_update(e); break;
      case EK_CREEP: creep_update(e); break;
      case EK_TRAPDOOR: trapdoor_update(e); break;
      default: break;
    }
  }
  /* creep under Isaac's feet (Lemon Mishap's is his own) */
}

/* ---------------------------------------------------------------- drawing by depth */
typedef struct { int16_t y, what; } Draw;   /* what: -1 Isaac, -2 - r a grid row, else an ent */

void ents_draw(void) {
  /* first what lies flat: decals, creep, the trapdoor */
  for (int i = 0; i < MAX_ENTS; i++) {
    Ent *e = &ents[i];
    if (e->kind == EK_DECAL) {
      if (e->a > 0) {
        gfx_ellipse((int)e->x, (int)e->y, (int)e->a, (int)(e->a * 0.55f), rgb565(0x7A0A0A), 20);
        gfx_ellipse((int)e->x + 2, (int)e->y - 1, (int)(e->a * 0.5f), (int)(e->a * 0.3f), rgb565(0x9A1010), 22);
      } else
        gfx_ellipse((int)e->x, (int)e->y, (int)-e->a, (int)(-e->a * 0.55f), C_BLACK, 14);
    } else if (e->kind == EK_CREEP) {
      static const uint32_t col[4] = {0xB01818, 0x40B020, 0xE0D040, 0xF0F0F0};
      int a = e->t < 20 ? e->t : 20;
      gfx_ellipse((int)e->x, (int)e->y, (int)e->r, (int)(e->r * 0.6f), rgb565(col[e->type & 3]), a);
    } else if (e->kind == EK_TRAPDOOR) {
      if (e->type == 1) {   /* the beam of light to the Cathedral */
        gfx_ellipse((int)e->x, (int)e->y, 14, 6, C_WHITE, 20);
      } else
        gfx_sprite(SPR_TRAPDOOR, 0, (int)e->x, (int)e->y, 0, 32);
    }
  }
  /* shadows */
  for (int i = 0; i < MAX_ENTS; i++) {
    Ent *e = &ents[i];
    if ((e->kind == EK_MON && !(e->flags & EF_HIDDEN)) || e->kind == EK_PICKUP || e->kind == EK_FAMILIAR || e->kind == EK_BOMB) {
      int rx = (int)(e->r * 0.9f) + 1;
      if (e->z < -30) rx = rx * 2 / 3;
      gfx_ellipse((int)e->x, (int)e->y, rx, rx / 3 + 1, C_BLACK, 11);
    }
  }
  player_draw_shadow();
  /* then everything standing, the deepest first */
  static Draw list[MAX_ENTS + ROWS + 2];
  int n = 0;
  for (int r = 0; r < ROWS; r++) list[n].y = (int16_t)(ROOM_Y + r * TILE + TILE / 2 + 2), list[n++].what = (int16_t)(-2 - r);
  list[n].y = (int16_t)pl.y, list[n++].what = -1;
  for (int i = 0; i < MAX_ENTS; i++) {
    Ent *e = &ents[i];
    if (!e->kind || e->kind == EK_DECAL || e->kind == EK_CREEP || e->kind == EK_TRAPDOOR) continue;
    if (e->kind == EK_FX && e->type == FX_EXPLOSION) continue;   /* (drawn over all) */
    int y = (int)e->y;
    if (e->kind == EK_MON && (e->flags & EF_FLYING)) y += 6;
    list[n].y = (int16_t)y, list[n++].what = (int16_t)i;
  }
  for (int i = 1; i < n; i++) {   /* (insertion sort: nearly sorted from one frame to the next) */
    Draw d = list[i];
    int j = i - 1;
    while (j >= 0 && list[j].y > d.y) list[j + 1] = list[j], j--;
    list[j + 1] = d;
  }
  for (int i = 0; i < n; i++) {
    int w = list[i].what;
    if (w == -1) {
      player_draw();
      continue;
    }
    if (w <= -2) {
      grid_draw_row(-2 - w);
      continue;
    }
    Ent *e = &ents[w];
    switch (e->kind) {
      case EK_MON: mon_draw(e); break;
      case EK_PICKUP: case EK_BOMB: pickup_draw(e); break;
      case EK_PEDESTAL: pedestal_draw(e); break;
      case EK_MACHINE: machine_draw(e); break;
      case EK_FAMILIAR: familiar_draw(e); break;
      case EK_FX: fx_draw(e); break;
      default: break;
    }
  }
  for (int i = 0; i < MAX_ENTS; i++)
    if (ents[i].kind == EK_FX && ents[i].type == FX_EXPLOSION) fx_draw(&ents[i]);
}
