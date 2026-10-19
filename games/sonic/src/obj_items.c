/* Rings (in groups of up to 7 from one placement, each remembered once collected), the rings Sonic
 * scatters when hurt, monitors and what comes out of them, the explosion of an enemy with the
 * animal it freed and its points, and the bomb's explosion. */
#include "s1.h"
#include "collide.h"
#include "game.h"
#include "objs.h"

#define ring_origX 0x32
#define ring_respawnbit 0x34

void CollectRing(void) {
  V.rings++;
  V.f_ringcount |= 1;
  if (V.rings < 100) return;
  if (!(V.lifecount & 2)) {
    V.lifecount |= 2;
  } else {
    if (V.rings < 200 || (V.lifecount & 4)) return;
    V.lifecount |= 4;
  }
  V.lives++;
  V.f_lifecount++;
}

void Rings(Obj *o) {
  static const int8_t pos[16][2] = {{0x10, 0}, {0x18, 0}, {0x20, 0}, {0, 0x10}, {0, 0x18}, {0, 0x20},
                                    {0x10, 0x10}, {0x18, 0x18}, {0x20, 0x20}, {-0x10, 0x10}, {-0x18, 0x18},
                                    {-0x20, 0x20}, {0x10, 8}, {0x18, 0x10}, {-0x10, 8}, {-0x18, 0x10}};
  switch (o->routine) {
  case 0: {
    uint8_t *st = &V.objstate[2 + o->respawn];
    uint8_t d4 = *st;
    int n = obSubtype(o) & 7;
    if (n == 7) n = 6;
    int8_t d5 = pos[obSubtype(o) >> 4][0], d6 = pos[obSubtype(o) >> 4][1];
    int16_t x = o->x.w.i, y = o->y.w.i;
    Obj *a1 = o;
    for (int i = 0; i <= n; i++, x += d5, y += d6, d4 >>= 1) {
      if (d4 & 1) continue;
      *st &= 0x7F;
      if (i) {
        a1 = FindFreeObj();
        if (!a1) break;
      }
      a1->id = id_Rings;
      a1->routine += 2;
      a1->x.w.i = x;
      OW(a1, ring_origX) = o->x.w.i;
      a1->y.w.i = y;
      a1->map = map_get(MAP_Ring);
      a1->gfx = ArtTile_Ring | 0x2000;
      a1->render = 4;
      a1->priority = 2;
      a1->coltype = 0x47; /* col_12x12 | col_item */
      a1->actwid = 8;
      a1->respawn = o->respawn;
      OB(a1, ring_respawnbit) = (uint8_t)i;
    }
    if (*st & 1) {
      DeleteObject(o);
      return;
    }
  }
    /* fall through */
  case 2:
    o->frame = V.ani_frame[1];
    DisplaySprite(o);
    if (obj_far_x(OW(o, ring_origX))) DeleteObject(o);
    return;
  case 4:
    o->routine += 2;
    o->coltype = 0;
    o->priority = 1;
    CollectRing();
    V.objstate[2 + o->respawn] |= (uint8_t)(1 << OB(o, ring_respawnbit));
    /* fall through */
  case 6:
    AnimateSprite(o, ani_get(ANI_Ring));
    DisplaySprite(o);
    return;
  default: DeleteObject(o); return;
  }
}

extern int obj_d7;

void RingLoss(Obj *o) {
  switch (o->routine) {
  case 0: {
    Obj *a1 = o;
    int n = V.rings < 32 ? V.rings : 32;
    int16_t d4 = 0x288, d2 = 0, d3 = 0;
    for (int i = 0; i < n; i++) {
      if (i) {
        a1 = FindFreeObj();
        if (!a1) break;
      }
      a1->id = id_RingLoss;
      a1->routine += 2;
      a1->height = a1->width = 8;
      a1->x.w.i = o->x.w.i, a1->y.w.i = o->y.w.i;
      a1->map = map_get(MAP_Ring);
      a1->gfx = ArtTile_Ring | 0x2000;
      a1->render = 4;
      a1->priority = 3;
      a1->coltype = 0x47;
      a1->actwid = 8;
      V.ani_time[3] = 0xFF;
      if (d4 >= 0) {
        int16_t c, s = CalcSine((uint8_t)d4, &c);
        int sh = (uint16_t)d4 >> 8;
        d2 = (int16_t)(s << sh), d3 = (int16_t)(c << sh);
        uint8_t lo = (uint8_t)d4;
        if ((uint16_t)lo + 0x10 > 0xFF) { /* (carry out of the low byte) */
          d4 = (int16_t)((d4 & 0xFF00) | (uint8_t)(lo + 0x10));
          if ((uint16_t)d4 < 0x80) d4 = 0x288;
          else d4 = (int16_t)(d4 - 0x80);
        } else d4 = (int16_t)((d4 & 0xFF00) | (uint8_t)(lo + 0x10));
      }
      a1->vx = d2, a1->vy = d3;
      d2 = (int16_t)-d2;
      d4 = (int16_t)-d4;
    }
    V.rings = 0;
    V.f_ringcount = 0x80;
    V.lifecount = 0;
  }
    /* fall through */
  case 2:
    o->frame = V.ani_frame[3];
    SpeedToPos(o);
    o->vy += 0x18;
    if (o->vy >= 0 && !((uint8_t)((uint8_t)V.vblank_count + obj_d7) & 3)) {
      int16_t d1 = ObjFloorDist(o, NULL);
      if (d1 < 0) {
        o->y.w.i += d1;
        o->vy = (int16_t)(o->vy - (o->vy >> 2));
        o->vy = (int16_t)-o->vy;
      }
    }
    if (!V.ani_time[3] || (uint16_t)(V.limitbtm2 + 224) < (uint16_t)o->y.w.i) {
      DeleteObject(o);
      return;
    }
    DisplaySprite(o);
    return;
  case 4:
    o->routine += 2;
    o->coltype = 0;
    o->priority = 1;
    CollectRing();
    /* fall through */
  case 6:
    AnimateSprite(o, ani_get(ANI_Ring));
    DisplaySprite(o);
    return;
  default: DeleteObject(o); return;
  }
}

/* ------------------------------------------------------------------ monitors */
static int mon_sides(Obj *o, int16_t d1, int16_t d2, int16_t *d0o, int16_t *d3o) {
  Obj *a1 = player;
  int16_t d0 = (int16_t)(a1->x.w.i - o->x.w.i + d1);
  if (d0 < 0 || (uint16_t)d0 > (uint16_t)(d1 * 2)) return 0;
  d2 = (int16_t)(d2 + (int8_t)a1->height);
  int16_t d3 = (int16_t)(a1->y.w.i - o->y.w.i + d2);
  if (d3 < 0 || (uint16_t)d3 >= (uint16_t)(d2 * 2)) return 0;
  if ((V.f_playerctrl & 0x80) || player->routine >= 6 || V.debuguse) return 0;
  if ((uint16_t)d1 < (uint16_t)d0) d0 = (int16_t)(d0 - d1 * 2);
  *d0o = d0, *d3o = d3;
  if ((uint16_t)d3 >= 0x10) return 1;
  int16_t e = (int16_t)(o->actwid + 4), w = (int16_t)(e * 2);
  e = (int16_t)(e + a1->x.w.i - o->x.w.i);
  if (e < 0 || (uint16_t)e >= (uint16_t)w) return 1;
  return -1;
}

void Monitor(Obj *o) {
  Obj *a1 = player;
  switch (o->routine) {
  case 0:
    o->routine = 2;
    o->height = o->width = 14;
    o->map = map_get(MAP_Monitor);
    o->gfx = ArtTile_Monitor;
    o->render = 4;
    o->priority = 3;
    o->actwid = 15;
    V.objstate[2 + o->respawn] &= 0x7F;
    if (V.objstate[2 + o->respawn] & 1) {
      o->routine = 8;
      o->frame = 0xB;
      return;
    }
    o->coltype = 0x46;
    o->anim = obSubtype(o);
    /* fall through */
  case 2:
    if (o->routine2) {
      if (o->routine2 == 2) {
        ExitPlatform(o, (int16_t)(o->actwid + 11));
        if (a1->status & 8) MvSonicOnPtfm(o, o->x.w.i, 16);
        else o->routine2 = 0;
      } else {
        ObjectFall(o);
        int16_t d1 = ObjFloorDist(o, NULL);
        if (d1 < 0) {
          o->y.w.i += d1;
          o->vy = 0;
          o->routine2 = 0;
        }
      }
      goto animate;
    } else {
      int16_t d0 = 0, d3 = 0;
      int r = mon_sides(o, 15 + 11, 15, &d0, &d3);
      if (!r || (a1->vy >= 0 && a1->anim == id_Roll)) {
        if (o->status & 0x20) {
          a1->anim = 0, a1->prevani = id_Run;
          o->status &= (uint8_t)~0x20;
          a1->status &= (uint8_t)~0x20;
        }
        goto animate;
      }
      if (r < 0) {
        a1->y.w.i -= d3;
        Plat_NoCheck_ext(o);
        o->routine2 = 2;
        goto animate;
      }
      if (d0 > 0) {
        if (a1->vx >= 0) goto stop;
      } else if (d0 < 0) {
        if (a1->vx < 0) goto stop;
      }
      goto push;
    stop:
      a1->x.w.i -= d0;
      a1->inertia = 0;
      a1->vx = 0;
    push:
      if (a1->status & 2) {
        o->status &= (uint8_t)~0x20;
        a1->status &= (uint8_t)~0x20;
      } else {
        a1->status |= 0x20;
        o->status |= 0x20;
      }
    }
    /* fall through */
  animate:
  case 6:
    AnimateSprite(o, ani_get(ANI_Monitor));
    /* fall through */
  case 8:
    DisplaySprite(o);
    if (obj_far(o)) DeleteObject(o);
    return;
  case 4: {
    o->routine += 2;
    o->coltype = 0;
    Obj *p = FindFreeObj();
    if (p) {
      p->id = id_PowerUp;
      p->x.w.i = o->x.w.i, p->y.w.i = o->y.w.i;
      p->anim = o->anim;
    }
    p = FindFreeObj();
    if (p) {
      p->id = id_ExplosionItem;
      p->routine += 2;
      p->x.w.i = o->x.w.i, p->y.w.i = o->y.w.i;
    }
    V.objstate[2 + o->respawn] |= 1;
    o->anim = 9;
    DisplaySprite(o);
    return;
  }
  }
}

void PowerUp(Obj *o) {
  switch (o->routine) {
  case 0: {
    o->routine = 2;
    o->gfx = ArtTile_Monitor;
    o->render = 0x24;
    o->priority = 3;
    o->actwid = 8;
    o->vy = -0x300;
    uint8_t f = (uint8_t)(o->anim + 2);
    o->frame = f;
    o->map = map_frame(map_get(MAP_Monitor), f) + 2;
  }
    /* fall through */
  case 2:
    if (o->vy < 0) {
      SpeedToPos(o);
      o->vy += 0x18;
      break;
    }
    o->routine += 2;
    OW(o, 0x3E) = 30 - 1; /* (obTimeFrame, as a word) */
    switch (o->anim) {
    case 2:
      V.lives++, V.f_lifecount++;
      break;
    case 3:
      V.shoes = 1;
      OW(player, 0x34) = 20 * 60;
      V.sonspeedmax = 0x600 * 2, V.sonspeedacc = 0xC * 2, V.sonspeeddec = 0x80;
      break;
    case 4:
      V.shield = 1;
      OBJ(6)->id = id_ShieldItem;
      break;
    case 5:
      V.invinc = 1;
      OW(player, 0x32) = 20 * 60;
      for (int i = 0; i < 4; i++) OBJ(8 + i)->id = id_ShieldItem, OBJ(8 + i)->anim = (uint8_t)(1 + i);
      break;
    case 6:
      V.rings += 10;
      V.f_ringcount |= 1;
      if (V.rings < 100) break;
      if (!(V.lifecount & 2)) {
        V.lifecount |= 2;
        V.lives++, V.f_lifecount++;
        break;
      }
      if (V.rings < 200 || (V.lifecount & 4)) break;
      V.lifecount |= 4;
      V.lives++, V.f_lifecount++;
      break;
    default: break;
    }
    break;
  default:
    if (--OW(o, 0x3E) < 0) {
      DeleteObject(o);
      return;
    }
    break;
  }
  DisplaySprite(o);
}

/* ------------------------------------------------------------------ explosions, animals, points */
#define exitem_pointsframe 0x3E
#define animal_doublehop 0x29
#define animal_id 0x30
#define animal_speedX 0x32
#define animal_speedY 0x34
#define animal_prisondelay 0x36
#define animal_pointsframe 0x3E

static void exitem_animate(Obj *o) {
  if ((int8_t)--o->timeframe < 0) {
    o->timeframe = 7;
    if (++o->frame == 5) {
      DeleteObject(o);
      return;
    }
  }
  DisplaySprite(o);
}

void ExplosionItem(Obj *o) {
  switch (o->routine) {
  case 0: {
    o->routine += 2;
    Obj *a1 = FindFreeObj();
    if (a1) {
      a1->id = id_Animals;
      a1->x.w.i = o->x.w.i, a1->y.w.i = o->y.w.i;
      OW(a1, animal_pointsframe) = OW(o, exitem_pointsframe);
    }
  }
    /* fall through */
  case 2:
    o->routine += 2;
    o->map = map_get(MAP_ExplodeItem);
    o->gfx = ArtTile_Explosion;
    o->render = 4;
    o->priority = 1;
    o->coltype = 0;
    o->actwid = 12;
    o->timeframe = 7;
    o->frame = 0;
    /* fall through */
  default: exitem_animate(o); return;
  }
}

void Explosion(Obj *o) {
  if (o->routine == 0) {
    o->routine += 2;
    o->map = map_get(MAP_ExplodeBomb);
    o->gfx = ArtTile_Explosion;
    o->render = 4;
    o->priority = 1;
    o->coltype = 0;
    o->actwid = 12;
    o->timeframe = 7;
    o->frame = 0;
    return;
  }
  exitem_animate(o);
}

static const uint8_t anml_varindex[6][2] = {{0, 5}, {2, 3}, {6, 3}, {4, 5}, {4, 1}, {0, 1}};
static const struct {
  int16_t vx, vy;
  uint8_t map;
} anml_vars[7] = {{-0x200, -0x400, 0}, {-0x200, -0x300, 1}, {-0x180, -0x300, 0}, {-0x140, -0x180, 1},
                  {-0x1C0, -0x300, 2}, {-0x300, -0x400, 1}, {-0x280, -0x380, 2}};
static const int animal_maps[3] = {MAP_Animal1, MAP_Animal2, MAP_Animal3};
static const int16_t anml_endspeed[11][2] = {{-0x440, -0x400}, {-0x440, -0x400}, {-0x440, -0x400}, {-0x300, -0x400},
                                             {-0x300, -0x400}, {-0x180, -0x300}, {-0x180, -0x300}, {-0x140, -0x180},
                                             {-0x1C0, -0x300}, {-0x200, -0x300}, {-0x280, -0x380}};
static const uint8_t anml_endmap[11] = {1, 1, 1, 0, 0, 0, 0, 1, 2, 1, 2};
static const uint16_t anml_endvram[11] = {
  ArtTile_Ending_Flicky, ArtTile_Ending_Flicky, ArtTile_Ending_Flicky, ArtTile_Ending_Rabbit, ArtTile_Ending_Rabbit,
  ArtTile_Ending_Penguin, ArtTile_Ending_Penguin, ArtTile_Ending_Seal, ArtTile_Ending_Pig, ArtTile_Ending_Chicken,
  ArtTile_Ending_Squirrel};

static void anml_end_chkdel(Obj *o) {
  int16_t d0 = (int16_t)(o->x.w.i - player->x.w.i);
  if ((uint16_t)o->x.w.i >= (uint16_t)player->x.w.i && (int16_t)(d0 - (320 + 64)) < 0 && !(o->render & 0x80)) {
    DeleteObject(o);
    return;
  }
  DisplaySprite(o);
}

static void anml_animate2(Obj *o) {
  if ((int8_t)--o->timeframe < 0) {
    o->timeframe = 1;
    o->frame = (uint8_t)((o->frame + 1) & 1);
  }
}

static void anml_chkdel(Obj *o) {
  if (obSubtype(o)) {
    anml_end_chkdel(o);
    return;
  }
  if (!(o->render & 0x80)) {
    DeleteObject(o);
    return;
  }
  DisplaySprite(o);
}

static void anml_normal(Obj *o) {
  ObjectFall(o);
  o->frame = 1;
  if (o->vy >= 0) {
    o->frame = 0;
    int16_t d1 = ObjFloorDist(o, NULL);
    if (d1 < 0) o->y.w.i += d1, o->vy = OW(o, animal_speedY);
  }
  anml_chkdel(o);
}

static void anml_slow(Obj *o) {
  SpeedToPos(o);
  o->vy += 0x18;
  if (o->vy >= 0) {
    int16_t d1 = ObjFloorDist(o, NULL);
    if (d1 < 0) {
      o->y.w.i += d1;
      o->vy = OW(o, animal_speedY);
      if (obSubtype(o) && obSubtype(o) != 0xA) {
        o->vx = (int16_t)-o->vx;
        o->render ^= 1;
      }
    }
  }
  anml_animate2(o);
  anml_chkdel(o);
}

static int16_t close_to_sonic(Obj *o) { return (int16_t)(player->x.w.i - o->x.w.i - (160 + 24)); }

static void anml_end_bounce(Obj *o) {
  o->frame = 1;
  if (o->vy < 0) return;
  o->frame = 0;
  int16_t d1 = ObjFloorDist(o, NULL);
  if (d1 < 0) o->y.w.i += d1, o->vy = OW(o, animal_speedY);
}
static void anml_face_sonic(Obj *o) {
  o->render |= 1;
  if ((uint16_t)o->x.w.i < (uint16_t)player->x.w.i) o->render &= (uint8_t)~1;
}

void Animals(Obj *o) {
  switch (o->routine) {
  case 0: {
    if (obSubtype(o)) {
      int d0 = obSubtype(o) * 2;
      o->routine = (uint8_t)d0;
      int k = (d0 - 0x14) / 2;
      o->gfx = anml_endvram[k];
      o->map = map_get(animal_maps[anml_endmap[k]]);
      OW(o, animal_speedX) = o->vx = anml_endspeed[k][0];
      OW(o, animal_speedY) = o->vy = anml_endspeed[k][1];
      o->height = 12;
      o->render = 4 | 1;
      o->priority = 6;
      o->actwid = 8;
      o->timeframe = 7;
      DisplaySprite(o);
      return;
    }
    o->routine += 2;
    int r = RandomNumber() & 1;
    uint8_t id = anml_varindex[V.zone > 5 ? 0 : V.zone][r];
    OB(o, animal_id) = id;
    OW(o, animal_speedX) = anml_vars[id].vx;
    OW(o, animal_speedY) = anml_vars[id].vy;
    o->map = map_get(animal_maps[anml_vars[id].map]);
    o->gfx = id & 1 ? ArtTile_Animal_2 : ArtTile_Animal_1;
    o->height = 12;
    o->render = 4 | 1;
    o->priority = 6;
    o->actwid = 8;
    o->timeframe = 7;
    o->frame = 2;
    o->vy = -0x400;
    if (V.bossstatus) {
      o->routine = 0x12;
      o->vx = 0;
      DisplaySprite(o);
      return;
    }
    Obj *a1 = FindFreeObj();
    if (a1) {
      a1->id = id_Points;
      a1->x.w.i = o->x.w.i, a1->y.w.i = o->y.w.i;
      a1->frame = (uint8_t)((uint16_t)OW(o, animal_pointsframe) >> 1);
    }
    DisplaySprite(o);
    return;
  }
  case 2:
    if (!(o->render & 0x80)) {
      DeleteObject(o);
      return;
    }
    ObjectFall(o);
    if (o->vy >= 0) {
      int16_t d1 = ObjFloorDist(o, NULL);
      if (d1 < 0) {
        o->y.w.i += d1;
        o->vx = OW(o, animal_speedX), o->vy = OW(o, animal_speedY);
        o->frame = 1;
        o->routine = (uint8_t)(OB(o, animal_id) * 2 + 4);
        if (V.bossstatus && (V.vblank_count & 0x10)) {
          o->vx = (int16_t)-o->vx;
          o->render ^= 1;
        }
      }
    }
    DisplaySprite(o);
    return;
  case 4: case 8: case 0xA: case 0xC: case 0x10: anml_normal(o); return;
  case 6: case 0xE: anml_slow(o); return;
  case 0x12:
    if (!(o->render & 0x80)) {
      DeleteObject(o);
      return;
    }
    if (!--OW(o, animal_prisondelay)) o->routine = 2, o->priority = 3;
    DisplaySprite(o);
    return;
  case 0x14: case 0x16: /* fly to the left */
    if ((uint16_t)close_to_sonic(o) >= 0x8000 || 1) {
      int16_t d = close_to_sonic(o);
      if ((uint16_t)(player->x.w.i - o->x.w.i) >= 160 + 24) {
        (void)d;
        anml_end_chkdel(o);
        return;
      }
    }
    o->vx = OW(o, animal_speedX), o->vy = OW(o, animal_speedY);
    o->routine = 0xE;
    anml_slow(o);
    return;
  case 0x18: /* stay and face Sonic, slowly */
    if (close_to_sonic(o) >= 0) break;
    o->vx = 0, OW(o, animal_speedX) = 0;
    SpeedToPos(o);
    o->vy += 0x18;
    anml_end_bounce(o);
    anml_face_sonic(o);
    anml_animate2(o);
    break;
  case 0x1A: /* hop to the left */
    if (close_to_sonic(o) >= 0) break;
    o->vx = OW(o, animal_speedX), o->vy = OW(o, animal_speedY);
    o->routine = 4;
    anml_normal(o);
    return;
  case 0x1C: case 0x20: case 0x24: /* stay and face Sonic */
    if (close_to_sonic(o) >= 0) break;
    o->vx = 0, OW(o, animal_speedX) = 0;
    ObjectFall(o);
    anml_end_bounce(o);
    anml_face_sonic(o);
    break;
  case 0x1E: case 0x22: /* hop around */
    if (close_to_sonic(o) >= 0) break;
    ObjectFall(o);
    o->frame = 1;
    if (o->vy >= 0) {
      o->frame = 0;
      int16_t d1 = ObjFloorDist(o, NULL);
      if (d1 < 0) {
        o->vx = (int16_t)-o->vx;
        o->render ^= 1;
        o->y.w.i += d1;
        o->vy = OW(o, animal_speedY);
      }
    }
    break;
  case 0x26: /* fly, turning every other bounce */
    if (close_to_sonic(o) >= 0) break;
    SpeedToPos(o);
    o->vy += 0x18;
    if (o->vy >= 0) {
      int16_t d1 = ObjFloorDist(o, NULL);
      if (d1 < 0) {
        OB(o, animal_doublehop) = (uint8_t)~OB(o, animal_doublehop);
        if (!OB(o, animal_doublehop)) o->vx = (int16_t)-o->vx, o->render ^= 1;
        o->y.w.i += d1;
        o->vy = OW(o, animal_speedY);
      }
    }
    anml_animate2(o);
    break;
  case 0x28: /* hop, turning every other bounce */
    ObjectFall(o);
    o->frame = 1;
    if (o->vy >= 0) {
      o->frame = 0;
      int16_t d1 = ObjFloorDist(o, NULL);
      if (d1 < 0) {
        OB(o, animal_doublehop) = (uint8_t)~OB(o, animal_doublehop);
        if (!OB(o, animal_doublehop)) o->vx = (int16_t)-o->vx, o->render ^= 1;
        o->y.w.i += d1;
        o->vy = OW(o, animal_speedY);
      }
    }
    break;
  default: break;
  }
  anml_end_chkdel(o);
}

void Points(Obj *o) {
  if (o->routine == 0) {
    o->routine += 2;
    o->map = map_get(MAP_Poi);
    o->gfx = ArtTile_Points | 0x2000;
    o->render = 4;
    o->priority = 1;
    o->actwid = 8;
    o->vy = -0x300;
  }
  if (o->vy >= 0) DeleteObject(o);
  else SpeedToPos(o), o->vy += 0x18;
  DisplaySprite(o);
}
