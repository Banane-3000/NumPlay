/* Objects: running all 128 each frame (only Sonic's 32 and those on screen keep moving or showing
 * while he dies), the sprite list built from the display queue's 8 layers, the object manager that
 * places the act's objects as the camera reaches them (and remembers the ones that must not come
 * back), animation scripts, and the little routines every object uses. */
#include "s1.h"

struct spritequeue_s spritequeue[8];

int obj_d7; /* the objects left to run after this one (the original's d7, which some objects read) */

void ExecuteObjects(void) {
  int n = NOBJ;
  if (player->routine >= 6) n = NRESERVED;
  for (int i = 0; i < n; i++) {
    Obj *o = &objs[i];
    obj_d7 = n - 1 - i;
    if (o->id && obj_index[o->id]) obj_index[o->id](o);
  }
  if (n == NOBJ) return;
  for (int i = NRESERVED; i < NOBJ; i++) {
    Obj *o = &objs[i];
    if (o->id && (o->render & 0x80)) DisplaySprite(o);
  }
}

void DisplaySprite(Obj *o) {
  struct spritequeue_s *q = &spritequeue[o->priority & 7];
  if (q->n >= 63) return;
  q->o[q->n++] = o;
}

void DeleteObject(Obj *o) { memset(o, 0, sizeof *o); }

Obj *FindFreeObj(void) {
  for (int i = NRESERVED; i < NOBJ; i++)
    if (!objs[i].id) return &objs[i];
  return NULL;
}

Obj *FindNextFreeObj(Obj *parent) {
  for (Obj *o = parent; o < objs + NOBJ; o++)
    if (!o->id) return o;
  return NULL;
}

void SpeedToPos(Obj *o) {
  o->x.l += (int32_t)o->vx << 8;
  o->y.l += (int32_t)o->vy << 8;
}

void ObjectFall(Obj *o) {
  int16_t vy = o->vy;
  o->vy = (int16_t)(o->vy + 0x38);
  o->x.l += (int32_t)o->vx << 8;
  o->y.l += (int32_t)vy << 8;
}

bool ChkObjectVisible(Obj *o) {
  int16_t x = (int16_t)(o->x.w.i - V.cam[0].x.w.i), y = (int16_t)(o->y.w.i - V.cam[0].y.w.i);
  return x >= 0 && x < 320 && y >= 0 && y < 224;
}

bool ChkPartiallyVisible(Obj *o) {
  int16_t x = (int16_t)(o->x.w.i - V.cam[0].x.w.i + o->actwid);
  if (x < 0) return false;
  if ((int16_t)(x - o->actwid * 2) >= 320) return false;
  int16_t y = (int16_t)(o->y.w.i - V.cam[0].y.w.i);
  return y >= 0 && y < 224;
}

/* out_of_range: far from the camera (more than about 192 pixels beyond either side) */
bool obj_far(Obj *o) {
  uint16_t d = (uint16_t)((o->x.w.i & 0xFF80) - ((V.cam[0].x.w.i - 128) & 0xFF80));
  return d > 128 + 320 + 192;
}
bool obj_far_x(int16_t x) {
  uint16_t d = (uint16_t)((x & 0xFF80) - ((V.cam[0].x.w.i - 128) & 0xFF80));
  return d > 128 + 320 + 192;
}

/* off screen for good: let the object manager place it again later */
void RememberState(Obj *o) {
  if (obj_far(o)) {
    if (o->respawn) V.objstate[2 + o->respawn] &= 0x7F;
    DeleteObject(o);
    return;
  }
  DisplaySprite(o);
}

/* ------------------------------------------------------------------ animation */
void AnimateSprite(Obj *o, const uint8_t *ani) {
  uint8_t a = o->anim;
  if (a != o->prevani) {
    o->prevani = a;
    o->aniframe = 0;
    o->timeframe = 0;
  }
  if ((int8_t)--o->timeframe >= 0) return;
  const uint8_t *s = ani + ((const uint16_t *)(const void *)ani)[1 + a];
  o->timeframe = s[0];
  int i = o->aniframe;
  uint8_t f = s[1 + i];
  if (f & 0x80) {
    switch (f) {
    case 0xFF: o->aniframe = 0, f = s[1]; break;
    case 0xFE: {
      uint8_t back = s[2 + i];
      o->aniframe = (uint8_t)(o->aniframe - back);
      f = s[1 + (uint8_t)(i - back)];
      break;
    }
    case 0xFD: o->anim = s[2 + i]; return;
    case 0xFC: o->routine += 2; return;
    case 0xFB: o->aniframe = 0, o->routine2 = 0; return;
    case 0xFA: o->routine2 += 2; return;
    default: return;
    }
  }
  o->frame = f & 0x1F;
  uint8_t flip = (uint8_t)(((f >> 5) | (f << 3)) & 0xFF); /* rol.b #3 */
  flip = (uint8_t)((flip ^ o->status) & 3);
  o->render = (uint8_t)((o->render & ~3) | flip);
  o->aniframe++;
}

/* ------------------------------------------------------------------ the sprite list */
static int spr_n; /* v_spritecount */

/* a frame's pieces into the sprite list at x, y (plus 128), flipped as render says (BuildSpr_Normal...) */
void BuildSpr_Pieces(const uint8_t *p, int count, int16_t x, int16_t y, uint8_t r, uint16_t gfx) {
  for (int i = 0; i < count && spr_n < 80; i++, p += 6) {
    int8_t py = (int8_t)p[0], px = (int8_t)p[4];
    uint8_t size = p[1];
    uint16_t tile = (uint16_t)(p[2] | p[3] << 8);
    int16_t sy = py, sx = px;
    if (r & 2) sy = (int16_t)(-sy - (((size << 3) & 0x18) + 8));
    if (r & 1) sx = (int16_t)(-sx - (((size << 1) & 0x18) + 8));
    tile = (uint16_t)(tile + gfx);
    tile ^= (uint16_t)((r & 1) << 11 | (r & 2) << 11);
    Sprite *s = &spritetable[spr_n];
    s->y = (int16_t)(sy + y);
    s->size = size;
    s->tile = tile;
    int16_t fx = (int16_t)((sx + x) & 0x1FF);
    s->x = fx ? fx : 1;
    spr_n++;
    s->link = (uint8_t)spr_n;
  }
}

/* the end of the list: its last sprite links to none */
void BuildSpr_End(void) {
  int n = spr_n;
  if (n == 80) spritetable[79].link = 0;
  else spritetable[n].link = 0, spritetable[n].y = 0, spritetable[n].size = 0, spritetable[n].tile = 0, spritetable[n].x = 0;
  if (n == 0) spritetable[0].y = 0;
}

void BuildSprites(void) {
  spr_n = 0;
  for (int layer = 0; layer < 8; layer++) {
    struct spritequeue_s *q = &spritequeue[layer];
    for (int k = 0; k < q->n; k++) {
      Obj *o = q->o[k];
      if (!o->id) continue;
      /* (no mappings: the title's PRESS START BUTTON, never set up there in the original, nor here) */
      if (!o->map) continue;
      o->render &= 0x7F;
      uint8_t r = o->render;
      int16_t x, y;
      if (r & 0x0C) {
        const Cam *c = (r & 0x0C) == 4 ? &V.cam[0] : (r & 0x0C) == 8 ? &V.cam[1] : &V.cam[3];
        int16_t d3 = (int16_t)(o->x.w.i - c->x.w.i);
        if ((int16_t)(d3 + o->actwid) < 0 || (int16_t)(d3 - o->actwid) >= 320) continue;
        x = (int16_t)(d3 + 0x80);
        if (r & 0x10) {
          int16_t d2 = (int16_t)(o->y.w.i - c->y.w.i);
          if ((int16_t)(d2 + o->height) < 0 || (int16_t)(d2 - o->height) >= 224) continue;
          y = (int16_t)(d2 + 0x80);
        } else {
          y = (int16_t)(o->y.w.i - c->y.w.i + 0x80);
          if ((uint16_t)y < 0x80 - 32 || (uint16_t)y >= 0x80 + 224 + 32) continue;
        }
      } else {
        y = obScreenY(o);
        x = o->x.w.i;
      }
      if (r & 0x20) BuildSpr_Pieces(o->map, 1, x, y, r, o->gfx); /* a single piece */
      else {
        const uint8_t *p = map_frame(o->map, o->frame);
        BuildSpr_Pieces(p + 2, p[0], x, y, r, o->gfx);
      }
      o->render |= 0x80;
    }
    q->n = 0;
  }
  BuildSpr_End();
}

/* ------------------------------------------------------------------ the object manager */
typedef struct {
  uint16_t x, y;
  uint8_t id, sub;
} ObjPos;
#define OP(off) ((const ObjPos *)DATA(off))

static const uint32_t objpos_index[7][4] = {
  {D_OBJPOS_GHZ1, D_OBJPOS_GHZ2, D_OBJPOS_GHZ3, D_OBJPOS_GHZ1},
  {D_OBJPOS_LZ1, D_OBJPOS_LZ2, D_OBJPOS_LZ3, D_OBJPOS_SBZ3},
  {D_OBJPOS_MZ1, D_OBJPOS_MZ2, D_OBJPOS_MZ3, D_OBJPOS_MZ1},
  {D_OBJPOS_SLZ1, D_OBJPOS_SLZ2, D_OBJPOS_SLZ3, D_OBJPOS_SLZ1},
  {D_OBJPOS_SYZ1, D_OBJPOS_SYZ2, D_OBJPOS_SYZ3, D_OBJPOS_SYZ1},
  {D_OBJPOS_SBZ1, D_OBJPOS_SBZ2, D_OBJPOS_FZ, D_OBJPOS_SBZ1},
  {D_OBJPOS_ENDING, D_OBJPOS_ENDING, D_OBJPOS_ENDING, D_OBJPOS_ENDING},
};

/* places the object at a (true), or, if its slot in the state list says it was used, skips it */
static bool spawn(uint32_t *a, uint8_t d2) {
  const ObjPos *e = OP(*a);
  uint8_t *st = V.objstate;
  if (e->id & 0x80) {
    uint8_t old = st[2 + d2] & 0x80;
    st[2 + d2] |= 0x80;
    if (old) {
      *a += 6;
      return true;
    }
  }
  Obj *o = FindFreeObj();
  if (!o) return false;
  *a += 6;
  o->x.w.i = (int16_t)e->x;
  o->y.w.i = (int16_t)(e->y & 0xFFF);
  uint8_t flip = (uint8_t)((uint16_t)(e->y << 2 | e->y >> 14) & 3);
  o->render = o->status = flip;
  uint8_t id = e->id;
  if (id & 0x80) id &= 0x7F, o->respawn = d2;
  o->id = id;
  obSubtype(o) = e->sub;
  return true;
}

void ObjPosLoad(void) {
  uint8_t *st = V.objstate;
  uint8_t d2 = 0;
  uint32_t a;
  if (V.opl_routine == 0) {
    V.opl_routine = 2;
    uint32_t l = objpos_index[V.zone > 6 ? 6 : V.zone][V.act & 3];
    V.opl_data[0] = V.opl_data[1] = l;
    memset(st, 0, sizeof V.objstate);
    st[0] = st[1] = 1;
    int16_t d6 = (int16_t)(V.cam[0].x.w.i - 128);
    if ((uint16_t)V.cam[0].x.w.i < 128) d6 = 0;
    d6 &= (int16_t)0xFF80;
    a = V.opl_data[0];
    while ((uint16_t)d6 > OP(a)->x) {
      if (OP(a)->id & 0x80) d2 = st[0]++;
      a += 6;
    }
    V.opl_data[0] = a;
    a = V.opl_data[1];
    uint16_t d6l = (uint16_t)(d6 - 128);
    if ((uint16_t)d6 >= 128) {
      while (d6l > OP(a)->x) {
        if (OP(a)->id & 0x80) st[1]++;
        a += 6;
      }
    }
    V.opl_data[1] = a;
    V.opl_screen = -1;
  }
  d2 = 0;
  int16_t d6 = (int16_t)(V.cam[0].x.w.i & 0xFF80);
  if (d6 == V.opl_screen) return;
  if (d6 < V.opl_screen) {
    V.opl_screen = d6;
    a = V.opl_data[1];
    int16_t l = (int16_t)(d6 - 128);
    if ((uint16_t)d6 >= 128) {
      while (!(l >= (int16_t)OP(a - 6)->x)) {
        a -= 6;
        if (OP(a)->id & 0x80) d2 = --st[1];
        if (!spawn(&a, d2)) {
          if (OP(a)->id & 0x80) st[1]++;
          a += 6;
          break;
        }
        a -= 6;
      }
    }
    V.opl_data[1] = a;
    a = V.opl_data[0];
    l = (int16_t)(d6 + 320 + 320);
    while (!(l > (int16_t)OP(a - 6)->x)) {
      if (OP(a - 6)->id & 0x80) st[0]--;
      a -= 6;
    }
    V.opl_data[0] = a;
    return;
  }
  V.opl_screen = d6;
  a = V.opl_data[0];
  uint16_t r = (uint16_t)(d6 + 320 + 320);
  while (r > OP(a)->x) {
    if (OP(a)->id & 0x80) d2 = st[0]++;
    if (!spawn(&a, d2)) break;
  }
  V.opl_data[0] = a;
  a = V.opl_data[1];
  uint16_t l = (uint16_t)(d6 + 320 + 320 - (320 + 320 + 128));
  if ((uint16_t)(d6 + 320 + 320) >= 320 + 320 + 128) {
    while (l > OP(a)->x) {
      if (OP(a)->id & 0x80) st[1]++;
      a += 6;
    }
  }
  V.opl_data[1] = a;
}
