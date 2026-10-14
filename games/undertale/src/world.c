/* The overworld: rooms, the camera, Frisk and the room's objects (world.h). */
#include <string.h>
#include "event.h"
#include "game.h"
#include "gfx.h"
#include "res.h"
#include "text.h"
#include "world.h"

void pl_walk_step(void);   /* event.c */

Obj objs[MAX_OBJS];
Player pl;
int g_room = -1;
static RoomInfo room;
static Door doors[16];
static int ndoors;
int cam_x, cam_y;
bool cam_locked;
uint8_t g_fade;

/* Frisk's box: his feet (from his sprite's top-left), as the game's mask */
#define PBX 3
#define PBY 20
#define PBW 14
#define PBH 9
#define SPEED 3

const RoomInfo *world_room_info(void) { return g_room >= 0 ? &room : NULL; }

/* the room of the same area whose rectangle holds (x, y), -1 none */
static int room_at(int x, int y) {
  if (x >= room.x && x < room.x + room.w && y >= room.y && y < room.y + room.h) return g_room;
  for (int r = 0; r < NUM_ROOMS; r++) {
    RoomInfo o;
    room_info(r, &o);
    if (o.area == room.area && x >= o.x && x < o.x + o.w && y >= o.y && y < o.y + o.h) return r;
  }
  return -1;
}

/* the 4-pixel cell at (x, y) walkable? (in whichever room of the area holds it) */
static bool cell_walkable(int x, int y) {
  const RoomInfo *ri = &room;
  RoomInfo o;
  if (!(x >= room.x && x < room.x + room.w && y >= room.y && y < room.y + room.h)) {
    int r = room_at(x, y);
    if (r < 0) return false;
    room_info(r, &o);
    ri = &o;
  }
  int cx = (x - ri->x) >> 2, cy = (y - ri->y) >> 2;
  if (cx < 0 || cy < 0 || cx >= ri->cw || cy >= ri->ch) return false;
  return ri->coll[cy * ((ri->cw + 7) >> 3) + (cx >> 3)] >> (cx & 7) & 1;
}

static bool boxes_meet(int ax, int ay, int aw, int ah, int bx, int by, int bw, int bh) {
  return ax < bx + bw && bx < ax + aw && ay < by + bh && by < ay + ah;
}

bool world_walkable(int x, int y, int w, int h, int ignore) {
  /* every cell the box covers (its corners and every 4 pixels between) */
  for (int yy = y;; yy += 4) {
    if (yy > y + h - 1) yy = y + h - 1;
    for (int xx = x;; xx += 4) {
      if (xx > x + w - 1) xx = x + w - 1;
      if (!cell_walkable(xx, yy)) return false;
      if (xx == x + w - 1) break;
    }
    if (yy == y + h - 1) break;
  }
  for (int i = 0; i < MAX_OBJS; i++) {
    const Obj *o = &objs[i];
    if (i == ignore || o->kind == OBJ_NONE || !(o->flags & OF_SOLID)) continue;
    if (boxes_meet(x, y, w, h, o->x + o->bx, o->y + o->by, o->bw, o->bh)) return false;
  }
  return true;
}

static bool player_free(int x, int y) { return world_walkable(x + PBX, y + PBY, PBW, PBH, -1); }

Obj *obj_new(int kind, int x, int y) {
  for (int i = 0; i < MAX_OBJS; i++)
    if (objs[i].kind == OBJ_NONE) {
      Obj *o = &objs[i];
      memset(o, 0, sizeof *o);
      o->kind = (uint8_t)kind, o->x = (int16_t)x, o->y = (int16_t)y, o->alpha = 255;
      return o;
    }
  return NULL;
}
Obj *obj_find(int name) {
  for (int i = 0; i < MAX_OBJS; i++)
    if (objs[i].kind != OBJ_NONE && objs[i].name == name) return &objs[i];
  return NULL;
}
void obj_clear_all(void) { memset(objs, 0, sizeof objs); }

void world_camera(void) {
  if (cam_locked) return;
  int cx = pl.x + 10 - SCREEN_W / 2, cy = pl.y + 15 - SCREEN_H / 2;
  if (room.w <= SCREEN_W) cx = room.x + (room.w - SCREEN_W) / 2;
  else if (cx < room.x) cx = room.x;
  else if (cx > room.x + room.w - SCREEN_W) cx = room.x + room.w - SCREEN_W;
  if (room.h <= SCREEN_H) cy = room.y + (room.h - SCREEN_H) / 2;
  else if (cy < room.y) cy = room.y;
  else if (cy > room.y + room.h - SCREEN_H) cy = room.y + room.h - SCREEN_H;
  cam_x = cx, cam_y = cy;
}

void world_enter(int r, int x, int y, int dir) {
  int from = g_room;
  g_room = r;
  room_info(r, &room);
  pl.x = (int16_t)x, pl.y = (int16_t)y, pl.dir = (uint8_t)dir;
  pl.frame = 0, pl.anim = 0;
  obj_clear_all();
  ndoors = room_doors(r, doors, 16);
  cam_locked = false;
  room_setup(r, from);
  world_camera();
}

/* ---------------------------------------------------------------- fades */
static struct {
  int room, x, y, dir;
  int phase, t;    /* 1: going black, 2: coming back */
} fade;
#define FADE_FRAMES 4

void world_fade_to(int r, int x, int y, int dir) {
  fade.room = r, fade.x = x, fade.y = y, fade.dir = dir;
  fade.phase = 1, fade.t = 0;
}
bool world_fading(void) { return fade.phase != 0; }

static void fade_step(void) {
  if (fade.phase == 1) {
    if (++fade.t >= FADE_FRAMES) {
      world_enter(fade.room, fade.x, fade.y, fade.dir);
      fade.phase = 2, fade.t = 0;
    }
    g_fade = (uint8_t)(255 * fade.t / FADE_FRAMES);
  } else if (fade.phase == 2) {
    if (++fade.t >= FADE_FRAMES) fade.phase = 0;
    g_fade = (uint8_t)(255 - 255 * fade.t / FADE_FRAMES);
  } else {
    g_fade = 0;
  }
}

/* ---------------------------------------------------------------- Frisk */
static const uint8_t frames_of[4] = {4, 4, 2, 2};

static void face_from_keys(uint32_t keys, uint32_t pressed) {
  static const uint32_t bit[4] = {K_DOWN, K_UP, K_LEFT, K_RIGHT};
  /* a direction just pressed turns him; letting go of the one he faces turns him to one still held */
  for (int d = 0; d < 4; d++)
    if (pressed & bit[d]) pl.dir = (uint8_t)d;
  if (!(keys & bit[pl.dir]))
    for (int d = 0; d < 4; d++)
      if (keys & bit[d]) {
        pl.dir = (uint8_t)d;
        break;
      }
}

/* one pixel along an axis, sliding past a corner (diagonal walls are steps of cells) */
static bool step(int sx, int sy, bool slide) {
  if (player_free(pl.x + sx, pl.y + sy)) {
    pl.x = (int16_t)(pl.x + sx), pl.y = (int16_t)(pl.y + sy);
    return true;
  }
  if (!slide) return false;
  for (int k = 1; k <= 2; k++) {
    bool a = sx ? player_free(pl.x + sx, pl.y - k) : player_free(pl.x - k, pl.y + sy);
    bool b = sx ? player_free(pl.x + sx, pl.y + k) : player_free(pl.x + k, pl.y + sy);
    if (a != b) {
      int o = a ? -1 : 1;
      if (sx) pl.y = (int16_t)(pl.y + o), pl.x = (int16_t)(pl.x + sx);
      else pl.x = (int16_t)(pl.x + o), pl.y = (int16_t)(pl.y + sy);
      return true;
    }
    if (a && b) break;
  }
  return false;
}

static void player_move(uint32_t keys, uint32_t pressed) {
  int dx = ((keys & K_RIGHT) != 0) - ((keys & K_LEFT) != 0);
  int dy = ((keys & K_DOWN) != 0) - ((keys & K_UP) != 0);
  face_from_keys(keys, pressed);
  bool moved = false;
  for (int i = 0; i < SPEED; i++) {
    if (dx) moved |= step(dx, 0, !dy);
    if (dy) moved |= step(0, dy, !dx);
  }
  if (dx || dy) {
    pl.anim++;
    pl.frame = (uint8_t)((1 + pl.anim / 6) % frames_of[pl.dir]);
    if (moved) pl.steps++;
  } else {
    pl.anim = 0, pl.frame = 0;
  }
  pl.moving = moved;
}

/* the box in front of Frisk, where Z looks */
static void front_box(int *x, int *y, int *w, int *h) {
  int fx = pl.x + PBX, fy = pl.y + PBY, fw = PBW, fh = PBH;
  switch (pl.dir) {
    case DIR_DOWN: fy += fh, fh = 12; break;
    case DIR_UP: fy -= 14, fh = 14; break;
    case DIR_LEFT: fx -= 12, fw = 12; break;
    default: fx += fw, fw = 12; break;
  }
  *x = fx, *y = fy, *w = fw, *h = fh;
}

static void interact(void) {
  int x, y, w, h;
  front_box(&x, &y, &w, &h);
  for (int i = 0; i < MAX_OBJS; i++) {
    Obj *o = &objs[i];
    if (o->kind == OBJ_NONE || !(o->flags & OF_TALK)) continue;
    if (boxes_meet(x, y, w, h, o->x + o->bx, o->y + o->by, o->bw, o->bh)) {
      event_start(o->event, o);
      return;
    }
  }
}

static void touches(void) {
  int x = pl.x + PBX, y = pl.y + PBY;
  for (int i = 0; i < MAX_OBJS; i++) {
    Obj *o = &objs[i];
    if (o->kind == OBJ_NONE || !(o->flags & OF_TOUCH)) continue;
    if (boxes_meet(x, y, PBW, PBH, o->x + o->bx, o->y + o->by, o->bw, o->bh)) {
      event_start(o->event, o);
      return;
    }
  }
}

/* ---------------------------------------------------------------- objects walking */
static void objs_update(void) {
  for (int i = 0; i < MAX_OBJS; i++) {
    Obj *o = &objs[i];
    if (o->kind == OBJ_NONE) continue;
    if (o->walking) {
      int dx = o->tx - o->x, dy = o->ty - o->y;
      int sp = o->speed ? o->speed : 2;
      if (dx) o->dir = dx < 0 ? DIR_LEFT : DIR_RIGHT;
      if (abs_i(dy) > abs_i(dx)) o->dir = dy < 0 ? DIR_UP : DIR_DOWN;
      o->x = (int16_t)(o->x + clamp_i(dx, -sp, sp));
      o->y = (int16_t)(o->y + clamp_i(dy, -sp, sp));
      if (o->x == o->tx && o->y == o->ty) o->walking = 0;
      if (o->flags & OF_WALKANIM) {
        o->anim_t++;
        o->frame = (uint8_t)(1 + o->anim_t / (o->anim_speed ? o->anim_speed : 6));
      }
      if (!o->walking && (o->flags & OF_WALKANIM)) o->frame = 0, o->anim_t = 0;
    } else if (o->flags & OF_ANIM) {
      if (++o->anim_t >= (o->anim_speed ? o->anim_speed : 6)) o->anim_t = 0, o->frame++;
    }
  }
}

void world_update(uint32_t keys, uint32_t pressed) {
  fade_step();
  if (fade.phase == 1) return;
  objs_update();
  bool busy = event_running() || text_active() || fade.phase;
  if (!busy && !pl.frozen) {
    player_move(keys, pressed);
    /* out of the room's rectangle: into the room that holds his feet */
    int fx = pl.x + PBX + PBW / 2, fy = pl.y + PBY + PBH / 2;
    int r = room_at(fx, fy);
    if (r >= 0 && r != g_room) {
      world_fade_to(r, pl.x, pl.y, pl.dir);
      return;
    }
    /* a door */
    for (int i = 0; i < ndoors; i++)
      if (boxes_meet(pl.x + PBX, pl.y + PBY, PBW, PBH, doors[i].x, doors[i].y, doors[i].w, doors[i].h)) {
        world_fade_to(doors[i].room, doors[i].tx, doors[i].ty, doors[i].dir);
        return;
      }
    if (pressed & K_Z) interact();
    else touches();
    if (!event_running() && (pressed & K_C)) game_open_menu();
    if (!event_running()) encounter_step();
  } else {
    pl_walk_step();
  }
  world_camera();
}

/* ---------------------------------------------------------------- drawing */
typedef struct {
  int16_t depth;
  int8_t who;     /* object index, -1 Frisk */
} DrawRef;

static uint16_t obj_sprite(const Obj *o) { return o->spr[o->spr[o->dir] ? o->dir : 0]; }

void world_draw(void) {
  gfx_tiles(cam_x, cam_y);
  /* the room's objects and Frisk, the lower ones in front */
  DrawRef order[MAX_OBJS + 1];
  int n = 0;
  for (int i = 0; i < MAX_OBJS; i++) {
    const Obj *o = &objs[i];
    if (o->kind == OBJ_NONE || (o->flags & OF_HIDDEN) || !o->spr[0]) continue;
    int d = o->y + o->by + o->bh + o->depth_bias;
    if (o->flags & OF_BACK) d = -30000;
    if (o->flags & OF_FRONT) d = 30000;
    order[n++] = (DrawRef){(int16_t)d, (int8_t)i};
  }
  if (!pl.hidden) order[n++] = (DrawRef){(int16_t)(pl.y + PBY + PBH), -1};
  for (int i = 1; i < n; i++)
    for (int j = i; j > 0 && order[j - 1].depth > order[j].depth; j--) {
      DrawRef t = order[j];
      order[j] = order[j - 1], order[j - 1] = t;
    }
  static const uint16_t frisk[4] = {SPR_FRISK_D, SPR_FRISK_U, SPR_FRISK_L, SPR_FRISK_R};
  for (int i = 0; i < n; i++) {
    if (order[i].who < 0) {
      gfx_sprite(frisk[pl.dir], pl.frame, pl.x - cam_x, pl.y - cam_y, 0, 255);
    } else {
      const Obj *o = &objs[order[i].who];
      gfx_sprite(obj_sprite(o), o->frame, o->x - cam_x, o->y - cam_y, 0, o->alpha);
    }
  }
}
