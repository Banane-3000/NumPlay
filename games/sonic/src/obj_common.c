/* Objects of every zone: the shield and invincibility stars, springs, spikes, the giant ring to
 * the special stage and its flash, hidden bonus points, the prison capsule at the end of each third
 * act, buttons, invisible walls; and what objects share: smashing into pieces, a boss's explosions
 * and its movement. */
#include "s1.h"
#include "game.h"
#include "objs.h"

/* ------------------------------------------------------------------ shield and stars */
#define stars_lag 0x30

void ShieldItem(Obj *o) {
  switch (o->routine) {
  case 0:
    o->routine += 2;
    o->map = map_get(MAP_Shield);
    o->render = sprite_cam_field;
    o->priority = 1;
    o->actwid = 32 / 2;
    if (o->anim) {
      o->routine += 2;
      o->gfx = ArtTile_Invincibility;
    } else o->gfx = ArtTile_Shield;
    return;
  case 2:
    if (V.invinc) return;
    if (!V.shield) {
      DeleteObject(o);
      return;
    }
    o->x.w.i = player->x.w.i;
    o->y.w.i = player->y.w.i;
    o->status = player->status;
    break;
  default: {
    if (!V.invinc) {
      DeleteObject(o);
      return;
    }
    uint8_t d0 = (uint8_t)V.trackpos;
    uint8_t d1 = (uint8_t)((o->anim - 1) * 24 + 4);
    d0 = (uint8_t)(d0 - d1);
    d1 = OB(o, stars_lag);
    d0 = (uint8_t)(d0 - d1);
    d1 = (uint8_t)(d1 + 4);
    if (d1 >= 6 * 4) d1 = 0;
    OB(o, stars_lag) = d1;
    const uint16_t *p = tracksonic[d0 >> 2];
    o->x.w.i = (int16_t)p[0];
    o->y.w.i = (int16_t)p[1];
    o->status = player->status;
    break;
  }
  }
  AnimateSprite(o, ani_get(ANI_Shield));
  DisplaySprite(o);
}

/* ------------------------------------------------------------------ springs */
#define spring_pow 0x30

void Springs(Obj *o) {
  Obj *a1 = player;
  switch (o->routine) {
  case 0: {
    o->routine += 2;
    o->map = map_get(MAP_Spring);
    o->gfx = ArtTile_Spring_Horizontal;
    o->render |= sprite_cam_field;
    o->actwid = 32 / 2;
    o->priority = 4;
    uint8_t d0 = obSubtype(o);
    if (d0 & 0x10) {
      o->routine = 8;
      o->anim = 1;
      o->frame = 3;
      o->gfx = ArtTile_Spring_Vertical;
      o->actwid = 16 / 2;
    }
    if (d0 & 0x20) {
      o->routine = 0xE;
      o->status |= 2;
    }
    if (d0 & 2) o->gfx |= 0x2000;
    OW(o, spring_pow) = (d0 & 0xF) ? -0xA00 : -0x1000;
    break;
  }
  case 2:
    SolidObject(o, 32 / 2 + sonic_solid_width, 16 / 2, 32 / 2, o->x.w.i);
    if (!o->routine2) break;
    o->routine += 2;
    a1->y.w.i = (int16_t)(a1->y.w.i + 8);
    a1->vy = OW(o, spring_pow);
    a1->status |= status_in_air;
    a1->status &= (uint8_t)~status_on_object;
    a1->anim = id_Spring;
    a1->routine = 2;
    o->status &= (uint8_t)~status_on_object;
    o->routine2 = 0;
    /* fall through */
  case 4:
  case 0xA:
  case 0x10:
    AnimateSprite(o, ani_get(ANI_Spring));
    break;
  case 6:
  case 0x12:
    o->prevani = 1;
    o->routine -= 4;
    break;
  case 8:
    SolidObject(o, 16 / 2 + sonic_solid_width, 28 / 2, 30 / 2, o->x.w.i);
    if (o->routine == 2) o->routine = 8;
    if (!(o->status & status_pushing)) break;
    o->routine += 2;
    a1->vx = OW(o, spring_pow);
    a1->x.w.i = (int16_t)(a1->x.w.i + 8);
    if (!(o->status & 1)) {
      a1->x.w.i = (int16_t)(a1->x.w.i - 16);
      a1->vx = (int16_t)-a1->vx;
    }
    OW(a1, locktime) = 15;
    a1->inertia = a1->vx;
    a1->status ^= 1;
    if (!(a1->status & status_rolling)) a1->anim = id_Walk;
    o->status &= (uint8_t)~status_pushing;
    a1->status &= (uint8_t)~status_pushing;
    AnimateSprite(o, ani_get(ANI_Spring));
    break;
  case 0xC:
    o->prevani = 2;
    o->routine -= 4;
    break;
  case 0xE: {
    int d4 = SolidObject(o, 32 / 2 + sonic_solid_width, 16 / 2, 32 / 2, o->x.w.i);
    if (o->routine == 2) o->routine = 0xE;
    if (o->routine2 || d4 >= 0) break;
    o->routine += 2;
    a1->y.w.i = (int16_t)(a1->y.w.i - 8);
    a1->vy = (int16_t)-OW(o, spring_pow);
    a1->status |= status_in_air;
    a1->status &= (uint8_t)~status_on_object;
    a1->routine = 2;
    o->status &= (uint8_t)~status_on_object;
    o->routine2 = 0;
    AnimateSprite(o, ani_get(ANI_Spring));
    break;
  }
  }
  DisplaySprite(o);
  if (obj_far(o)) DeleteObject(o);
}

/* ------------------------------------------------------------------ spikes */
#define spikes_origX 0x30
#define spikes_origY 0x32
#define spikes_move_pos 0x34
#define spikes_move_direction 0x36
#define spikes_move_delay 0x38

static void Spikes_WaitAndMove(Obj *o) {
  if (OW(o, spikes_move_delay)) {
    OW(o, spikes_move_delay)--;
    return;
  }
  if (OW(o, spikes_move_direction)) {
    if (OUW(o, spikes_move_pos) >= 0x800) {
      OUW(o, spikes_move_pos) -= 0x800;
      return;
    }
    OW(o, spikes_move_pos) = 0;
    OW(o, spikes_move_direction) = 0;
    OW(o, spikes_move_delay) = 60;
    return;
  }
  OUW(o, spikes_move_pos) += 0x800;
  if (OUW(o, spikes_move_pos) < 0x2000) return;
  OW(o, spikes_move_pos) = 0x2000;
  OW(o, spikes_move_direction) = 1;
  OW(o, spikes_move_delay) = 60;
}

void Spikes(Obj *o) {
  static const uint8_t config[6][2] = {{0, 20}, {1, 16}, {2, 4}, {3, 28}, {4, 64}, {5, 16}};
  if (o->routine == 0) {
    o->routine += 2;
    o->map = map_get(MAP_Spike);
    o->gfx = ArtTile_Spikes;
    o->render |= sprite_cam_field;
    o->priority = 4;
    int k = (obSubtype(o) & 0xF0) >> 4;
    obSubtype(o) &= 0xF;
    o->frame = config[k][0];
    o->actwid = config[k][1];
    OW(o, spikes_origX) = o->x.w.i;
    OW(o, spikes_origY) = o->y.w.i;
  }
  switch (obSubtype(o)) {
  case 1:
    Spikes_WaitAndMove(o);
    o->y.w.i = (int16_t)(OW(o, spikes_origY) + OWH(o, spikes_move_pos));
    break;
  case 2:
    Spikes_WaitAndMove(o);
    o->x.w.i = (int16_t)(OW(o, spikes_origX) + OWH(o, spikes_move_pos));
    break;
  }
  bool hurt;
  if (o->frame == 5 || o->frame == 1) {
    int16_t d2 = o->frame == 1 ? 40 / 2 : 8 / 2;
    int d4 = SolidObject(o, 32 / 2 + sonic_solid_width, d2, (int16_t)(d2 + 1), o->x.w.i);
    hurt = !(o->status & status_on_object) && d4 == 1;
  } else {
    int d4 = SolidObject(o, (int16_t)(o->actwid + sonic_solid_width), 32 / 2, 32 / 2 + 1, o->x.w.i);
    hurt = (o->status & status_on_object) || d4 < 0;
  }
  if (hurt && !V.invinc && player->routine < 4) {
    player->y.l -= (int32_t)player->vy << 8;
    HurtSonic(player, o);
  }
  DisplaySprite(o);
  if (obj_far_x(OW(o, spikes_origX))) DeleteObject(o);
}

/* ------------------------------------------------------------------ the giant ring */
#define gring_parent 0x3C

void GiantRing(Obj *o) {
  switch (o->routine) {
  case 0:
    o->map = map_get(MAP_GRing);
    o->gfx = ArtTile_Giant_Ring | Tile_Pal2;
    o->render |= sprite_cam_field;
    o->actwid = 128 / 2;
    if (o->render & 0x80) {
      if (V.emeralds == 6) {
        DeleteObject(o);
        return;
      }
      if (V.rings < 50) return;
      o->routine += 2;
      o->priority = 2;
      o->coltype = col_16x32 | col_item;
      V.gfxbigring = (uint16_t)(DATAW(D_ARTS)[ART_Art_BigRing * 2 + 1] * 32);
    }
    break;
  case 4: {
    o->routine -= 2;
    o->coltype = col_none;
    Obj *a1 = FindFreeObj();
    if (a1) {
      a1->id = id_RingFlash;
      a1->x.w.i = o->x.w.i;
      a1->y.w.i = o->y.w.i;
      OUW(a1, gring_parent) = OADDR(o);
      if ((uint16_t)player->x.w.i >= (uint16_t)o->x.w.i) a1->render |= sprite_xflip;
    }
    break;
  }
  case 6: DeleteObject(o); return;
  }
  o->frame = V.ani_frame[0];
  if (obj_far(o)) {
    DeleteObject(o);
    return;
  }
  DisplaySprite(o);
}

void RingFlash(Obj *o) {
  switch (o->routine) {
  case 0:
    o->routine += 2;
    o->map = map_get(MAP_Flash);
    o->gfx = ArtTile_Giant_Ring_Flash | Tile_Pal2;
    o->render |= sprite_cam_field;
    o->priority = 0;
    o->actwid = 64 / 2;
    o->frame = 0xFF;
    /* fall through */
  case 2:
    if ((int8_t)--o->timeframe < 0) {
      o->timeframe = 1;
      o->frame++;
      if (o->frame >= 8) {
        o->routine += 2;
        player->id = 0, player->render = 0;
        return;
      }
      if (o->frame == 3) {
        OPTR(OUW(o, gring_parent))->routine = 6;
        player->anim = id_Null;
        V.f_bigring = 1;
        V.invinc = 0;
        V.shield = 0;
      }
    }
    if (obj_far(o)) {
      DeleteObject(o);
      return;
    }
    DisplaySprite(o);
    return;
  default: DeleteObject(o);
  }
}

/* ------------------------------------------------------------------ hidden bonus points */
#define bonus_timelen 0x30

void HiddenBonus(Obj *o) {
  if (o->routine == 0) {
    if ((uint16_t)(player->x.w.i - o->x.w.i + 16) < 32 && (uint16_t)(player->y.w.i - o->y.w.i + 16) < 32 &&
        !V.debuguse && !V.f_bigring) {
      o->routine += 2;
      o->map = map_get(MAP_Bonus);
      o->gfx = ArtTile_Hidden_Points | Tile_Prio;
      o->render |= sprite_cam_field;
      o->priority = 0;
      o->actwid = 32 / 2;
      o->frame = obSubtype(o);
      OW(o, bonus_timelen) = 120 - 1;
      static const uint16_t points[4] = {0, 1000, 100, 1};
      AddPoints(points[obSubtype(o) & 3]);
    }
    if (obj_far(o)) DeleteObject(o);
    return;
  }
  if (--OW(o, bonus_timelen) < 0 || obj_far(o)) {
    DeleteObject(o);
    return;
  }
  DisplaySprite(o);
}

/* ------------------------------------------------------------------ shared */
/* the object's frame as pieces flying apart at these speeds */
void SmashObject(Obj *o, const int16_t *speeds, int n, int16_t gravity) {
  const uint8_t *p = map_frame(o->map, o->frame) + 2;
  o->render |= sprite_rawmappings;
  uint8_t id = o->id, r = o->render;
  Obj *a1 = o;
  for (int i = 0; i < n; i++) {
    if (i) {
      a1 = FindFreeObj();
      if (!a1) break;
      p += 6;
    }
    a1->routine = 4;
    a1->id = id;
    a1->map = p;
    a1->render = r;
    a1->x.w.i = o->x.w.i;
    a1->y.w.i = o->y.w.i;
    a1->gfx = o->gfx;
    a1->priority = o->priority;
    a1->actwid = o->actwid;
    a1->vx = *speeds++;
    a1->vy = *speeds++;
    if (a1 < o) { /* (already passed this frame: moved and shown now) */
      SpeedToPos(a1);
      a1->vy = (int16_t)(a1->vy + gravity);
      DisplaySprite(a1);
    }
  }
}

/* explosions over a defeated boss, one every 8 frames */
void BossDefeated(Obj *o) {
  if (vblank_byte & 7) return;
  Obj *a1 = FindFreeObj();
  if (!a1) return;
  a1->id = id_Explosion;
  a1->x.w.i = o->x.w.i;
  a1->y.w.i = o->y.w.i;
  uint32_t r = RandomNumber();
  a1->x.w.i = (int16_t)(a1->x.w.i + ((uint8_t)r >> 2) - 0x20);
  a1->y.w.i = (int16_t)(a1->y.w.i + ((uint8_t)(r >> 8) >> 3));
}

void BossMove(Obj *o) {
  obBossX(o).l += (int32_t)o->vx << 8;
  obBossY(o).l += (int32_t)o->vy << 8;
}

/* ------------------------------------------------------------------ the prison capsule */
#define pri_origY 0x30
#define animal_prisondelay 0x36

void Prison(Obj *o) {
  static const uint8_t var[4][4] = {{2, 64 / 2, 4, 0}, {4, 24 / 2, 5, 1}, {6, 32 / 2, 4, 3}, {8, 32 / 2, 3, 5}};
  switch (o->routine) {
  case 0: {
    o->map = map_get(MAP_Pri);
    o->gfx = ArtTile_Prison_Capsule;
    o->render = sprite_cam_field;
    OW(o, pri_origY) = o->y.w.i;
    int k = obSubtype(o) & 3;
    o->routine = var[k][0];
    o->actwid = var[k][1];
    o->priority = var[k][2];
    o->frame = var[k][3];
    if (obSubtype(o) == 2) {
      o->coltype = col_32x32 | col_boss;
      obBossHits(o) = 8;
    }
    break;
  }
  case 2:
    if (V.bossstatus != 2) {
      SolidObject(o, 64 / 2 + sonic_solid_width, 48 / 2, 48 / 2, o->x.w.i);
      break;
    }
    if (o->routine2) {
      o->routine2 = 0;
      player->status &= (uint8_t)~status_on_object;
      player->status |= status_in_air;
    }
    o->frame = 2;
    break;
  case 4:
    SolidObject(o, 24 / 2 + sonic_solid_width, 16 / 2, 16 / 2, o->x.w.i);
    AnimateSprite(o, ani_get(ANI_Pri));
    o->y.w.i = OW(o, pri_origY);
    if (!o->routine2) break;
    o->y.w.i = (int16_t)(o->y.w.i + 8);
    o->routine = 0xA;
    set_timew(o, 60);
    V.f_timecount = 0;
    V.f_lockscreen = 0;
    V.f_lockctrl = 1;
    V.jpadhold2 = btnR, V.jpadpress2 = 0;
    o->routine2 = 0;
    player->status &= (uint8_t)~status_on_object;
    player->status |= status_in_air;
    break;
  case 6:
  case 8:
  case 0xA:
    BossDefeated(o);
    set_timew(o, (uint16_t)(timew(o) - 1));
    if (timew(o)) break;
    V.bossstatus = 2;
    o->routine = 0xC;
    o->frame = 6;
    set_timew(o, 2 * 60 + 30);
    o->y.w.i = (int16_t)(o->y.w.i + 32);
    for (int i = 0, d5 = 2 * 60 + 34, d4 = -28; i < 8; i++, d4 += 7, d5 -= 8) {
      Obj *a1 = FindFreeObj();
      if (!a1) break;
      a1->id = id_Animals;
      a1->x.w.i = (int16_t)(o->x.w.i + d4);
      a1->y.w.i = o->y.w.i;
      OW(a1, animal_prisondelay) = (int16_t)d5;
    }
    break;
  case 0xC: {
    if (!(vblank_byte & 7)) {
      Obj *a1 = FindFreeObj();
      if (a1) {
        a1->id = id_Animals;
        a1->x.w.i = o->x.w.i;
        a1->y.w.i = o->y.w.i;
        int16_t d0 = (int16_t)((RandomNumber() & 0x1F) - 6);
        if ((int16_t)V.random < 0) d0 = (int16_t)-d0;
        a1->x.w.i = (int16_t)(a1->x.w.i + d0);
        OW(a1, animal_prisondelay) = 12;
      }
    }
    set_timew(o, (uint16_t)(timew(o) - 1));
    if (!timew(o)) o->routine += 2, set_timew(o, 3 * 60);
    break;
  }
  case 0xE:
    for (int i = 1; i < 64; i++)
      if (objs[i].id == id_Animals) goto show;
    GotThroughAct();
    DeleteObject(o);
    return;
  }
show:
  if (obj_far(o)) {
    DeleteObject(o);
    return;
  }
  DisplaySprite(o);
}

/* ------------------------------------------------------------------ invisible walls */
void Invisibarrier(Obj *o) {
  if (o->routine == 0) {
    o->routine += 2;
    o->map = map_get(MAP_Invis);
    o->gfx = ArtTile_Monitor | Tile_Prio;
    o->render |= sprite_cam_field;
    o->actwid = (uint8_t)(((obSubtype(o) & 0xF0) + 0x10) >> 1);
    o->height = (uint8_t)(((obSubtype(o) & 0xF) + 1) << 3);
  }
  if (ChkObjectVisible(o))
    SolidObject_NoRenderChk(o, (int16_t)(o->actwid + sonic_solid_width), o->height, (int16_t)(o->height + 1),
                            o->x.w.i);
  if (obj_far(o)) {
    DeleteObject(o);
    return;
  }
  if (V.debuguse) DisplaySprite(o);
}

/* ------------------------------------------------------------------ buttons */
/* MZ: a pushable block on the button */
static bool But_MZBlock(Obj *o) {
  int16_t d2 = (int16_t)(o->x.w.i - 0x10), d3 = (int16_t)(o->y.w.i - 8);
  for (int i = NRESERVED; i < NOBJ; i++) {
    Obj *a1 = &objs[i];
    if (!(a1->render & 0x80) || a1->id != id_PushBlock) continue;
    uint16_t ax = (uint16_t)(a1->x.w.i - 0x10), d0 = (uint16_t)(ax - (uint16_t)d2);
    if (ax >= (uint16_t)d2) {
      if (d0 > 0x20) continue;
    } else if ((uint32_t)d0 + 0x20 <= 0xFFFF) continue;
    uint16_t ay = (uint16_t)(a1->y.w.i - 0x10), e0 = (uint16_t)(ay - (uint16_t)d3);
    if (ay >= (uint16_t)d3) {
      if (e0 > 0x10) continue;
    } else if ((uint32_t)e0 + 0x20 <= 0xFFFF) continue;
    return true;
  }
  return false;
}

void Button(Obj *o) {
  if (o->routine == 0) {
    o->routine += 2;
    o->map = map_get(MAP_But);
    o->gfx = ArtTile_Button_Main | Tile_Pal3;
    if (V.zone != id_MZ) o->gfx = ArtTile_Button_Main;
    o->render = sprite_cam_field;
    o->actwid = 32 / 2;
    o->priority = 4;
    o->y.w.i = (int16_t)(o->y.w.i + 3);
  }
  if (o->render & 0x80) {
    SolidObject(o, 32 / 2 + sonic_solid_width, 10 / 2, 10 / 2, o->x.w.i);
    o->frame &= (uint8_t)~1;
    uint8_t *a3 = &V.f_switch[obSubtype(o) & 0xF];
    int d3 = (obSubtype(o) & 0x40) ? 7 : 0;
    bool pressed = ((obSubtype(o) & 0x80) && But_MZBlock(o)) || o->routine2;
    if (pressed) {
      *a3 |= (uint8_t)(1 << d3);
      o->frame |= 1;
    } else *a3 &= (uint8_t)~(1 << d3);
    if ((obSubtype(o) & 0x20) && (int8_t)--o->timeframe < 0) {
      o->timeframe = 7;
      o->frame ^= 2;
    }
  }
  DisplaySprite(o);
  if (obj_far(o)) DeleteObject(o);
}

/* ------------------------------------------------------------------ GHZ: the waterfall's sound, edge walls */
void WaterSound(Obj *o) {
  if (o->routine == 0) {
    o->routine += 2;
    o->render = sprite_cam_field;
  }
  if (obj_far(o)) DeleteObject(o);
}

/* a wall Sonic can't pass but can stand on: pushes him out sideways, or stops him from below */
static void EdgeWall_SolidWall(Obj *o, int16_t d1, int16_t d2) {
  Obj *a1 = player;
  int d4 = 0;
  int16_t d0 = (int16_t)(a1->x.w.i - o->x.w.i + d1), d3 = 0;
  if (d0 >= 0 && (uint16_t)d0 <= (uint16_t)(d1 * 2)) {
    d2 = (int16_t)(d2 + (int8_t)a1->height);
    d3 = (int16_t)(a1->y.w.i - o->y.w.i + d2);
    int16_t d4w = (int16_t)(d2 * 2);
    if (d3 >= 0 && (uint16_t)d3 < (uint16_t)d4w && !(V.f_playerctrl & 0x80) && a1->routine < 6 && !V.debuguse) {
      uint16_t d5 = (uint16_t)d0;
      if ((uint16_t)d1 < (uint16_t)d0) {
        d0 = (int16_t)(d0 - d1 * 2);
        d5 = (uint16_t)-d0;
      }
      uint16_t e1 = (uint16_t)d3;
      if ((uint16_t)d2 < (uint16_t)d3) {
        d3 = (int16_t)(d3 - d4w);
        e1 = (uint16_t)-d3;
      }
      d4 = d5 > e1 ? -1 : 1;
    }
  }
  if (d4 == 0) {
    if (!(o->status & status_pushing)) return;
    o->status &= (uint8_t)~status_pushing;
    a1->status &= (uint8_t)~status_pushing;
    return;
  }
  if (d4 < 0) {
    if (a1->vy < 0 && d3 < 0) {
      a1->y.w.i = (int16_t)(a1->y.w.i - d3);
      a1->vy = 0;
    }
    return;
  }
  if (d0 && ((d0 < 0) == (a1->vx < 0))) {
    a1->x.w.i = (int16_t)(a1->x.w.i - d0);
    a1->inertia = 0;
    a1->vx = 0;
  }
  if (a1->status & status_in_air) {
    o->status &= (uint8_t)~status_pushing;
    a1->status &= (uint8_t)~status_pushing;
    return;
  }
  a1->status |= status_pushing;
  o->status |= status_pushing;
}

void EdgeWalls(Obj *o) {
  if (o->routine == 0) {
    o->routine += 2;
    o->map = map_get(MAP_Edge);
    o->gfx = ArtTile_GHZ_Edge_Wall | Tile_Pal3;
    o->render |= sprite_cam_field;
    o->actwid = 16 / 2;
    o->priority = 6;
    o->frame = obSubtype(o);
    if (o->frame & 0x10) {
      o->frame &= (uint8_t)~0x10;
      o->routine += 2;
    }
  }
  if (o->routine == 2) EdgeWall_SolidWall(o, 38 / 2, 80 / 2);
  DisplaySprite(o);
  if (obj_far(o)) DeleteObject(o);
}
