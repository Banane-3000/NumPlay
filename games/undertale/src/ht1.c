/* Hotland, its first floor: the crossroads before the Lab, Alphys and Mettaton's quiz, the conveyor belts, the
 * Magma Chamber's steam vents and the burnt pan, the laser walkway, the shooting puzzles that open the door, the
 * vents over the lava, Mettaton's cooking show and the elevator by the CORE's view. (As the Undertale Wiki describes
 * them; what they say is ours, in short.) */
#include <stddef.h>
#include "battle.h"
#include "event.h"
#include "events.h"
#include "game.h"
#include "gfx.h"
#include "hotland.h"
#include "items.h"
#include "menu.h"
#include "monsters.h"
#include "save.h"
#include "shoot.h"
#include "snowdin.h"
#include "world.h"

static void gone(Obj *o) { o->kind = OBJ_NONE; }
static Obj *thing(int x, int y, int w, int h, int event, int name, int arg) {
  Obj *o = obj_trigger(x, y, w, h, event, name);
  if (o) o->flags = OF_TALK | OF_SOLID, o->arg = (int16_t)arg;
  return o;
}
static Obj *obj_alphys(int x, int y, int dir) {
  static const uint16_t s[4] = {SPR_ALPHYS_OW, SPR_ALPHYS_OW, SPR_ALPHYS_OW, SPR_ALPHYS_OW};
  return obj_npc(ACTOR_ALPHYS, s, x, y, dir, 0);
}
static Obj *obj_mtt(int x, int y) {
  static const uint16_t s[4] = {SPR_MTT_OW, SPR_MTT_OW, SPR_MTT_OW, SPR_MTT_OW};
  Obj *o = obj_npc(ACTOR_METTATON, s, x, y, DIR_DOWN, 0);
  if (o) o->flags |= OF_ANIM, o->flags &= (uint16_t)~OF_SOLID, o->anim_speed = 5;
  return o;
}

/* ---------------------------------------------------------------- the conveyor belts */
/* each pushes Frisk along it while Frisk's feet are on it, where there's floor */
typedef struct {
  int16_t room, x, y, w, h;
  int8_t dx, dy;
} Belt;
static const Belt belts[] = {
  /* the first room: up the left belt, down the right one */
  {ROOM_HT_BELT1, 2904, 5450, 36, 440, 0, -2}, {ROOM_HT_BELT1, 2980, 5450, 36, 440, 0, 2},
  /* the second: up the left side, down the right */
  {ROOM_HT_BELT2, 2840, 4730, 120, 450, 0, -1}, {ROOM_HT_BELT2, 2980, 4730, 120, 450, 0, 1},
  /* the second floor: the belt puzzle's (the top one runs left, the bottom right: as fast as Frisk walks, so
   * walking against one stands still); round the vent puzzle (down, along, up: back to its start); the news
   * room's, up to its vent */
  {ROOM_HT_BELTPUZ, 5130, 2340, 300, 50, -3, 0}, {ROOM_HT_BELTPUZ, 5130, 2440, 300, 50, 3, 0},
  {ROOM_HT_VENTPUZ, 4420, 2310, 60, 290, 0, 3}, {ROOM_HT_VENTPUZ, 4420, 2560, 360, 40, 3, 0},
  {ROOM_HT_VENTPUZ, 4720, 2430, 60, 170, 0, -3},
  {ROOM_HT_NEWS, 4300, 1790, 100, 160, 0, -2},
  /* the third floor's, on the way to the south puzzle: down the left side, along the bottom (past the drones), up the
   * right; back along the top */
  {ROOM_HT_BELTS3, 4800, 1400, 60, 130, 0, 3}, {ROOM_HT_BELTS3, 4800, 1520, 600, 60, 3, 0},
  {ROOM_HT_BELTS3, 5380, 1430, 60, 150, 0, -3}, {ROOM_HT_BELTS3, 4850, 1320, 520, 60, -3, 0},
};
static void belts_update(void) {
  if (event_running()) return;
  int fx = pl.x + 10, fy = pl.y + 24;
  for (unsigned i = 0; i < sizeof belts / sizeof belts[0]; i++) {
    const Belt *b = &belts[i];
    if (b->room != g_room || fx < b->x || fx >= b->x + b->w || fy < b->y || fy >= b->y + b->h) continue;
    if (world_walkable(pl.x + 3 + b->dx, pl.y + 20 + b->dy, 14, 9, -1)) pl.x = (int16_t)(pl.x + b->dx), pl.y = (int16_t)(pl.y + b->dy);
  }
}

/* ---------------------------------------------------------------- the steam vents */
/* stepping on a grate, Frisk is blown its way, over the lava, to (tx, ty) (Frisk's top-left: never on another
 * vent's grate, but where a chain of them is meant); a vent that turns (dir -1: the hub's, a quarter clockwise each
 * time it's used; -2: the pan's, left and right all the time) has a landing each way */
typedef struct {
  int16_t room, gx, gy;
  int8_t dir;
  int16_t t[4][2];   /* by DIR_*: down, up, left, right */
} Vent;
static const Vent vents[] = {
  {ROOM_HT_MAGMA, 3120, 4000, DIR_DOWN, {{3120, 4110}}},
  {ROOM_HT_MAGMA, 3362, 4000, DIR_LEFT, {[DIR_LEFT] = {3241, 3986}}},
  {ROOM_HT_MAGMA, 3503, 4060, DIR_RIGHT, {[DIR_RIGHT] = {3580, 4060}}},
  {ROOM_HT_MAGMA, 3420, 4180, DIR_UP, {[DIR_UP] = {3420, 4048}}},
  {ROOM_HT_MAGMA, 3225, 4240, DIR_RIGHT, {[DIR_RIGHT] = {3302, 4226}}},
  {ROOM_HT_MAGMA, 3325, 4240, DIR_LEFT, {[DIR_LEFT] = {3244, 4226}}},
  {ROOM_HT_MAGMA, 3525, 4240, DIR_LEFT, {[DIR_LEFT] = {3421, 4226}}},
  {ROOM_HT_PAN, 3620, 4480, -2, {[DIR_LEFT] = {3501, 4466}, [DIR_RIGHT] = {3739, 4466}}},
  {ROOM_HT_HUB, 4820, 3620, -1, {{4820, 3680}, {4820, 3490}, {4700, 3610}, {4945, 3610}}},
  {ROOM_HT_HUB, 4820, 3560, DIR_DOWN, {{4795, 3592}}},
  {ROOM_HT_HUB, 4740, 3620, DIR_RIGHT, {[DIR_RIGHT] = {4785, 3610}}},
  {ROOM_HT_HUB, 4900, 3620, DIR_LEFT, {[DIR_LEFT] = {4855, 3610}}},
  {ROOM_HT_VENTISLE, 4940, 3120, DIR_RIGHT, {[DIR_RIGHT] = {5051, 3106}}},
  {ROOM_HT_VENTISLE, 5060, 3120, DIR_RIGHT, {[DIR_RIGHT] = {5171, 3106}}},
  {ROOM_HT_VENTISLE, 5180, 3120, DIR_RIGHT, {[DIR_RIGHT] = {5312, 3106}}},
  /* the news room's: up to the top room's corner, and from there over to the rest of it */
  {ROOM_HT_NEWS, 4350, 1722, DIR_UP, {[DIR_UP] = {4350, 1600}}},
  {ROOM_HT_NEWS, 4320, 1600, DIR_LEFT, {[DIR_LEFT] = {4200, 1590}}},
  /* the third floor's islands: each grate blows the way its arrow points, to the next island that way (off the
   * edges: the corridor back west, the walkways north and south, the platform east to the door) */
  {ROOM_HT_HUB3, 4740, 740, DIR_DOWN, {[DIR_DOWN] = {4740, 786}}},
  {ROOM_HT_HUB3, 4820, 700, DIR_UP, {[DIR_UP] = {4820, 590}}},
  {ROOM_HT_HUB3, 4820, 740, DIR_DOWN, {[DIR_DOWN] = {4820, 786}}},
  {ROOM_HT_HUB3, 4880, 720, DIR_LEFT, {[DIR_LEFT] = {4820, 706}}},
  {ROOM_HT_HUB3, 4680, 800, DIR_RIGHT, {[DIR_RIGHT] = {4740, 786}}},
  {ROOM_HT_HUB3, 4740, 780, DIR_UP, {[DIR_UP] = {4740, 706}}},
  {ROOM_HT_HUB3, 4740, 820, DIR_DOWN, {[DIR_DOWN] = {4740, 866}}},
  {ROOM_HT_HUB3, 4820, 820, DIR_DOWN, {[DIR_DOWN] = {4820, 866}}},
  {ROOM_HT_HUB3, 4800, 800, DIR_LEFT, {[DIR_LEFT] = {4740, 786}}},
  {ROOM_HT_HUB3, 4900, 780, DIR_UP, {[DIR_UP] = {4900, 706}}},
  {ROOM_HT_HUB3, 4880, 800, DIR_LEFT, {[DIR_LEFT] = {4820, 786}}},
  {ROOM_HT_HUB3, 4960, 800, DIR_LEFT, {[DIR_LEFT] = {4900, 786}}},
  {ROOM_HT_HUB3, 4660, 860, DIR_UP, {[DIR_UP] = {4660, 786}}},
  {ROOM_HT_HUB3, 4640, 880, DIR_LEFT, {[DIR_LEFT] = {4572, 866}}},
  {ROOM_HT_HUB3, 4740, 900, DIR_DOWN, {[DIR_DOWN] = {4740, 946}}},
  {ROOM_HT_HUB3, 4840, 880, DIR_RIGHT, {[DIR_RIGHT] = {4900, 866}}},
  {ROOM_HT_HUB3, 4900, 860, DIR_UP, {[DIR_UP] = {4900, 786}}},
  {ROOM_HT_HUB3, 4980, 860, DIR_UP, {[DIR_UP] = {4980, 786}}},
  {ROOM_HT_HUB3, 4980, 900, DIR_DOWN, {[DIR_DOWN] = {4980, 946}}},
  {ROOM_HT_HUB3, 4960, 880, DIR_LEFT, {[DIR_LEFT] = {4900, 866}}},
  {ROOM_HT_HUB3, 5000, 880, DIR_RIGHT, {[DIR_RIGHT] = {5070, 866}}},
  {ROOM_HT_HUB3, 4660, 940, DIR_UP, {[DIR_UP] = {4660, 866}}},
  {ROOM_HT_HUB3, 4720, 960, DIR_LEFT, {[DIR_LEFT] = {4660, 946}}},
  {ROOM_HT_HUB3, 4760, 960, DIR_RIGHT, {[DIR_RIGHT] = {4820, 946}}},
  {ROOM_HT_HUB3, 4820, 940, DIR_UP, {[DIR_UP] = {4820, 866}}},
  {ROOM_HT_HUB3, 4840, 960, DIR_RIGHT, {[DIR_RIGHT] = {4900, 946}}},
  {ROOM_HT_HUB3, 4900, 940, DIR_UP, {[DIR_UP] = {4900, 866}}},
  {ROOM_HT_HUB3, 4900, 980, DIR_DOWN, {[DIR_DOWN] = {4900, 1026}}},
  {ROOM_HT_HUB3, 4920, 960, DIR_RIGHT, {[DIR_RIGHT] = {4980, 946}}},
  {ROOM_HT_HUB3, 4980, 940, DIR_UP, {[DIR_UP] = {4980, 866}}},
  {ROOM_HT_HUB3, 4740, 1020, DIR_UP, {[DIR_UP] = {4740, 946}}},
  {ROOM_HT_HUB3, 4820, 1060, DIR_DOWN, {[DIR_DOWN] = {4820, 1130}}},
  {ROOM_HT_HUB3, 4800, 1040, DIR_LEFT, {[DIR_LEFT] = {4740, 1026}}},
  {ROOM_HT_HUB3, 4880, 1040, DIR_LEFT, {[DIR_LEFT] = {4820, 1026}}},
  {ROOM_HT_HUB3, 4600, 880, DIR_RIGHT, {[DIR_RIGHT] = {4660, 866}}},
  {ROOM_HT_HUB3, 4820, 660, DIR_DOWN, {[DIR_DOWN] = {4820, 706}}},
  {ROOM_HT_HUB3, 4820, 1100, DIR_UP, {[DIR_UP] = {4820, 1026}}},
  {ROOM_HT_HUB3, 5040, 880, DIR_LEFT, {[DIR_LEFT] = {4980, 866}}},
};
#define NVENTS ((int)(sizeof vents / sizeof vents[0]))
/* the way a vent blows now */
static int hub_turns;
static int vent_dir(const Vent *v) {
  static const uint8_t round[4] = {DIR_UP, DIR_RIGHT, DIR_DOWN, DIR_LEFT};
  if (v->dir == -1) return round[hub_turns % 4];
  if (v->dir == -2) return (g_frame / 20) % 2 ? DIR_RIGHT : DIR_LEFT;
  return v->dir;
}
static void vents_setup(void) {
  for (int i = 0; i < NVENTS; i++) {
    const Vent *v = &vents[i];
    if (v->room != g_room) continue;
    Obj *o = obj_trigger(v->gx + 4, v->gy + 4, 12, 12, EV_VENT, ACTOR_VENT);
    if (o) o->arg = (int16_t)i;
  }
}

/* ---------------------------------------------------------------- the lasers */
/* the walkway's sweep to and fro across it: an orange one hurts if Frisk stands in it, a blue one if Frisk moves */
typedef struct {
  int16_t room, x, y, h, swing;
  uint8_t blue;
} Laser;
static const Laser lasers[] = {
  {ROOM_HT_LASERS, 3258, 3796, 62, 30, 0}, {ROOM_HT_LASERS, 3425, 3796, 62, 40, 1},
  {ROOM_HT_LASERS, 3567, 3796, 62, 30, 0}, {ROOM_HT_LASERS, 3717, 3796, 62, 40, 1},
  {ROOM_HT_LASERS, 3840, 3796, 62, 30, 0}, {ROOM_HT_LASERS, 3958, 3796, 62, 40, 1},
  /* the news room's bottom: blue, orange, blue */
  {ROOM_HT_NEWS, 3965, 1946, 56, 16, 1}, {ROOM_HT_NEWS, 4025, 1946, 56, 16, 0}, {ROOM_HT_NEWS, 4085, 1946, 56, 16, 1},
  /* the third floor's drones over the bottom belt, blue: ride it without a step */
  {ROOM_HT_BELTS3, 4967, 1496, 84, 0, 1}, {ROOM_HT_BELTS3, 5149, 1496, 84, 0, 1}, {ROOM_HT_BELTS3, 5249, 1496, 84, 0, 1},
};
static int laser_cool;
static int laser_x(int i) {
  const Laser *l = &lasers[i];
  int ph = (int)((g_frame * (l->blue ? 3 : 4) + (uint32_t)i * 40) % 256);
  return l->x + l->swing * gfx_sin(ph) / 1024;
}
static void lasers_watch(void) {
  if (laser_cool) laser_cool--;
  if (event_running() || laser_cool) return;
  for (int i = 0; i < (int)(sizeof lasers / sizeof lasers[0]); i++) {
    const Laser *l = &lasers[i];
    if (l->room != g_room) continue;
    int x = laser_x(i);
    if (pl.x + 3 > x + 1 || pl.x + 17 < x - 1 || pl.y + 29 < l->y || pl.y + 20 > l->y + l->h) continue;
    if (l->blue ? pl.moving : !pl.moving) {
      event_start(EV_LASER_HIT, NULL);
      return;
    }
  }
}
/* the blue one in front of the left puzzle, that Alphys switches off */
#define BLUE_X 4380
#define BLUE_Y 3584
#define BLUE_W 40

void ht1_draw(void) {
  if (world_room_info()->area != AREA_HOTLAND) return;
  for (int i = 0; i < (int)(sizeof lasers / sizeof lasers[0]); i++) {
    const Laser *l = &lasers[i];
    if (l->room != g_room) continue;
    uint16_t c = l->blue ? rgb565(0x14A9FF) : rgb565(0xFCA600);
    gfx_rect(laser_x(i) - 1 - cam_x, l->y - cam_y, 3, l->h, c, (g_frame / 2) % 2 ? 255 : 200);
  }
  if (g_room == ROOM_HT_HUB && !flag(F_HT_BLUE_OFF))
    gfx_rect(BLUE_X - cam_x, BLUE_Y - cam_y, BLUE_W, 3, rgb565(0x14A9FF), (g_frame / 2) % 2 ? 255 : 200);
  /* the turning vents' arrows, over the map's */
  static const uint16_t arrow[4] = {SPR_VENT_D, SPR_VENT_U, SPR_VENT_L, SPR_VENT_R};
  for (int i = 0; i < NVENTS; i++) {
    const Vent *v = &vents[i];
    if (v->room == g_room && v->dir < 0) gfx_sprite(arrow[vent_dir(v)], 0, v->gx - cam_x, v->gy - cam_y, 0, 255);
  }
}

void ht1_update(void) {
  if (world_room_info()->area != AREA_HOTLAND) return;
  belts_update();
  lasers_watch();
}

/* ---------------------------------------------------------------- the rooms */
void ht1_setup(int room, int from) {
  (void)from;
  vents_setup();
  if (room < ROOM_HT_LABFRONT || room > ROOM_HT_COREVIEW) return;
  switch (room) {
    case ROOM_HT_LABFRONT: {
      obj_save(1045, 6243, "* (The heat of Hotland fills you with determination.)");
      Obj *g = obj_deco(SPR_RGUARDS_OW, 1012, 6146, true);
      if (g) g->name = ACTOR_GUARDS, obj_set_box(g, 0, 30, 74, 24);
      obj_sign(1010, 6176, 80, 30, "* (Two Royal Guards stand in front of the elevator.)|* Nobody goes up.\n* Not until the captain's back.");
      obj_sign(1290, 6150, 60, 30, "* (The Lab. Its door is open.)");
      break;
    }
    case ROOM_HT_RIVER:
      obj_sign(1010, 6440, 50, 60, "* (Someone in a hood waits by\n  a boat, humming.)|* (Maybe they'll give you a\n  ride some other time.)");
      break;
    case ROOM_HT_LAB:
      if (!flag(F_HT_LAB)) obj_trigger(1450, 5860, 16, 460, EV_HT_LAB, ACTOR_TRIGGER);
      thing(2025, 6175, 34, 64, EV_HT_FRIDGE, ACTOR_ITEM, 0);
      obj_sign(1775, 6140, 108, 90, "* (A huge screen.)\n* (It shows you, standing right here.)");
      obj_sign(2317, 6150, 50, 75, "* (The door is locked.)\n* (Someone inside is humming.)");
      obj_sign(1880, 5920, 60, 40, "* (Shelves stuffed with comics.)\n* (Most of them are about magical girls.)");
      obj_sign(2050, 5920, 90, 50, "* (A bed, half buried in figurines.)");
      break;
    case ROOM_HT_MAGMA:
      obj_save(2968, 4226, "* (The bubbling magma fills you with determination.)");
      break;
    case ROOM_HT_PAN:
      if (!flag(F_HT_PAN)) {
        Obj *p = obj_deco(SPR_BURNTPAN, 3476, 4468, false);
        if (p) p->name = ACTOR_ITEM;
        Obj *t = obj_trigger(3472, 4462, 26, 26, EV_HT_ITEM, ACTOR_ITEM);
        if (t) t->flags = OF_TALK;
      }
      break;
    case ROOM_HT_LASERS:
      if (!flag(F_HT_CALL_LASER)) {
        Obj *t = obj_trigger(3180, 3780, 12, 90, EV_HT_CALL, ACTOR_TRIGGER);
        if (t) t->arg = 0;
      }
      break;
    case ROOM_HT_HUB:
      if (!flag(F_HT_CALL_HUB)) {
        Obj *t = obj_trigger(4770, 3700, 120, 12, EV_HT_CALL, ACTOR_TRIGGER);
        if (t) t->arg = 1;
      }
      if (!flag(F_HT_BLUE_OFF)) {
        Obj *b = obj_trigger(BLUE_X, BLUE_Y - 6, BLUE_W, 12, EV_NONE, ACTOR_LASER);
        if (b) b->flags = OF_SOLID;
        Obj *t = obj_trigger(4380, 3560, 60, 100, EV_HT_CALL, ACTOR_TRIGGER2);
        if (t) t->arg = 2;
      }
      thing(4372, 3410, 34, 34, EV_SHOOT, ACTOR_ITEM, SHOOT_LEFT);
      thing(5250, 3410, 34, 34, EV_SHOOT, ACTOR_ITEM, SHOOT_RIGHT);
      if (!(flag(F_HT_PUZ_L) && flag(F_HT_PUZ_R))) {
        Obj *d = obj_trigger(4796, 3398, 68, 16, EV_NONE, ACTOR_DOOR);
        if (d) d->flags = OF_SOLID;
        obj_sign(4796, 3412, 68, 20, "* (The door is locked.)\n* (Both puzzles have to be solved.)");
      }
      obj_sign(4278, 3570, 26, 110, "* Phew, the laser's off!\n* ...now what do I do?|* I guess I'll keep standing here.");
      obj_sign(5236, 3666, 60, 40, "* Did you catch Mettaton's show?\n* That human looked so real!|* His fan club is HUGE, you know.");
      obj_sign(5286, 3432, 24, 24, "* My co-workers won't help me\n  with this puzzle.|* Do I even WANT to go to work?");
      break;
    case ROOM_HT_VENTISLE:
      if (!flag(F_HT_CALL_ISLE)) {
        Obj *t = obj_trigger(4800, 3150, 60, 12, EV_HT_CALL, ACTOR_TRIGGER);
        if (t) t->arg = 3;
      }
      break;
    case ROOM_HT_COOKING:
      if (!flag(F_HT_COOK)) {
        obj_mtt(5662, 3138);
        obj_trigger(5330, 3000, 12, 260, EV_HT_COOK, ACTOR_TRIGGER);
      }
      obj_sign(5500, 3005, 110, 85, "* (The set's kitchen: a fridge, an oven, a counter.)");
      break;
    case ROOM_HT_COREVIEW:
      if (!flag(F_HT_CALL_CORE)) {
        Obj *t = obj_trigger(6300, 3080, 12, 120, EV_HT_CALL, ACTOR_TRIGGER);
        if (t) t->arg = 4;
      }
      thing(6950, 3070, 76, 48, EV_HT_ELEVATOR, ACTOR_DOOR, 0);
      break;
  }
}

/* ---------------------------------------------------------------- the events */
static int ev_lab(Ev *c) {
  Obj *a = obj_get(ACTOR_ALPHYS), *m = obj_get(ACTOR_METTATON);
  EV_BEGIN;
  if (c->self) gone(c->self);
  /* the Lab is dark; then the lights come on, and its big screen shows Frisk, right here */
  NARRATE("* (It's dark in here.)");
  g_shake = 2;
  WAIT(20);
  NARRATE("* (The lights come on.)\n* (A huge screen shows... you.)");
  a = obj_alphys(2212, 6275, DIR_LEFT);
  WALK(a, pl.x + 70, pl.y + 29, 2);
  SAY(FACE_ALPHYS, "* Oh! Oh no, you're already here!|* Um. H-hi! I'm Dr. Alphys.\n* The Royal Scientist.|* I've been, um, watching you.\n* On my cameras.|* Ever since the Ruins.\n* The fights, the friends...|* It's been kind of amazing?|* I was supposed to stop you.\n* But, um... I'd rather help.");
  /* and through the wall comes Mettaton */
  g_shake = 10;
  m = obj_mtt(2215, 6232);
  WAIT(20);
  NARRATE("* GOOD EVENING, BEAUTIES!\n* WELCOME TO TODAY'S QUIZ SHOW!");
  BATTLE(FORM_MTT_QUIZ);
  m = obj_get(ACTOR_METTATON);
  WALK(m, m->x, m->y - 200, 6);
  gone(obj_get(ACTOR_METTATON));
  set_flag(F_HT_LAB, 1);
  a = obj_get(ACTOR_ALPHYS);
  SAY(FACE_ALPHYS, "* S-sorry about him.\n* I built him, a long time ago.|* He was meant to be an\n  entertainment robot...|* Oh! Your phone is SO old.\n* Let me fix it up!");
  NARRATE("* (Alphys upgrades your phone.)\n* (Now it opens a Dimensional Box, too.)");
  set_flag(F_HT_LAB, 2);
  SAY(FACE_ALPHYS, "* There! It has a few new...\n  features.|* I'll, um, call you if I can help.|* Now I have to go to the...\n  bathroom. Good luck!");
  a = obj_get(ACTOR_ALPHYS);
  WALK(a, 2342, 6225, 3);
  gone(obj_get(ACTOR_ALPHYS));
  EV_END;
}

static int ev_fridge(Ev *c) {
  EV_BEGIN;
  if (!flag(F_HT_NOODLES)) {
    NARRATE("* (The fridge is full of\n  instant noodles. Take one?)@Take@Leave");
    if (CHOICE() == 0) {
      if (!item_add(ITEM_INSTANT_NOODLES)) {
        NARRATE("* (You're carrying too much.)");
      } else {
        set_flag(F_HT_NOODLES, 1);
        NARRATE("* (You got the Instant Noodles.)");
      }
    }
  } else {
    NARRATE("* (Noodles, as far as the eye can see.)");
  }
  EV_END;
}

static int ev_item(Ev *c) {
  EV_BEGIN;
  if (!item_add(ITEM_BURNT_PAN)) {
    NARRATE("* (You're carrying too much.)");
    return 1;
  }
  set_flag(F_HT_PAN, 1);
  NARRATE("* (You got the Burnt Pan.)");
  gone(obj_get(ACTOR_ITEM));
  if (c->self) gone(c->self);
  EV_END;
}

static int ev_vent(Ev *c) {
  EV_BEGIN;
  c->v[0] = c->self ? c->self->arg : 0;
  c->v[1] = vent_dir(&vents[c->v[0]]);
  /* the steam blows Frisk over the lava */
  pl.dir = (uint8_t)c->v[1];
  if (vents[c->v[0]].dir == -1) hub_turns++;
  PL_WALK(vents[c->v[0]].t[c->v[1]][0], vents[c->v[0]].t[c->v[1]][1], 7);
  EV_END;
}

static int ev_laser_hit(Ev *c) {
  EV_BEGIN;
  g_exclaim = 20;
  WAIT(15);
  BATTLE(FORM_LASERS);
  /* (out of the beam's way) */
  pl.x = (int16_t)(pl.x - 26);
  laser_cool = 60;
  EV_END;
}

/* Alphys's calls: 0 the lasers, 1 the puzzle hub, 2 the blue laser off, 3 the door already open, 4 by the CORE */
static int ev_call(Ev *c) {
  EV_BEGIN;
  c->v[0] = c->self ? c->self->arg : 0;
  if (c->self) gone(c->self);
  switch (c->v[0]) {
    case 0:
      set_flag(F_HT_CALL_LASER, 1);
      SAY(FACE_ALPHYS, "* Ring...|* H-hi! It's Alphys!|* Lasers up ahead!\n* Orange ones: keep moving!|* Blue ones: stand still.\n* Like the attacks, see?|* Okay! Bye!");
      break;
    case 1:
      set_flag(F_HT_CALL_HUB, 1);
      SAY(FACE_ALPHYS, "* Ring...|* It's Alphys again!|* That door north stays shut until\n  both puzzles are done.|* The one on the right is easier.\n* Try it first!");
      break;
    case 2:
      set_flag(F_HT_BLUE_OFF, 1);
      SAY(FACE_ALPHYS, "* Ring...|* Oh! A laser's blocking you?|* Hang on, hang on...");
      gone(obj_get(ACTOR_LASER));
      NARRATE("* (The blue laser switches off.)");
      SAY(FACE_ALPHYS, "* There! Got it!");
      break;
    case 3:
      set_flag(F_HT_CALL_ISLE, 1);
      SAY(FACE_ALPHYS, "* Ring...|* Hi! So, those puzzles by\n  the door: shoot the blocks\n  out of the way, then...|* ...|* Oh. You already did them.|* Ha ha... never mind!\n* Bye!");
      break;
    case 4:
      set_flag(F_HT_CALL_CORE, 1);
      SAY(FACE_ALPHYS, "* Ring...|* See that, far off?\n* That's the CORE.|* It powers the whole\n  underground.|* The king's castle is past it.\n* We're almost there!|* D-don't worry. I've got your back.");
      break;
  }
  EV_END;
}

static int ev_shoot(Ev *c) {
  EV_BEGIN;
  c->v[0] = c->self ? c->self->arg : 0;
  if (flag(c->v[0] == SHOOT_LEFT ? F_HT_PUZ_L : F_HT_PUZ_R)) {
    NARRATE("* (This puzzle is solved.)");
    return 1;
  }
  shoot_open(c->v[0]);
  WAIT_UNTIL(g_mode == MODE_WORLD);
  if (shoot_result()) {
    set_flag(c->v[0] == SHOOT_LEFT ? F_HT_PUZ_L : F_HT_PUZ_R, 1);
    if (flag(F_HT_PUZ_L) && flag(F_HT_PUZ_R)) {
      gone(obj_get(ACTOR_DOOR));
      NARRATE("* (Somewhere, a door unlocks.)");
    } else {
      NARRATE("* (One down.)");
    }
  }
  EV_END;
}

static int ev_cook(Ev *c) {
  Obj *m = obj_get(ACTOR_METTATON);
  EV_BEGIN;
  if (c->self) gone(c->self);
  /* the lights: Mettaton's cooking show */
  NARRATE("* (The lights come on.)");
  NARRATE("* GREETINGS, BEAUTIES!\n* WELCOME TO COOKING WITH A KILLER ROBOT!|* TODAY'S RECIPE: A CAKE!\n* MY LOVELY ASSISTANT WILL FETCH\n  THE SUGAR, MILK AND EGGS!");
  PL_WALK(5600, 3114, 3);
  NARRATE("* (You bring the sugar, the milk\n  and the eggs.)");
  NARRATE("* PERFECT! AND THE LAST INGREDIENT...\n* A HUMAN SOUL!");
  g_shake = 6;
  SAY(FACE_ALPHYS, "* Ring...|* W-wait! Mettaton!\n* What about a vegan option?\n* For the viewers at home?");
  NARRATE("* ...OH, VERY WELL.|* THE SUBSTITUTE IS ON THE\n  COUNTER, OVER THERE.\n* GO GET IT!");
  PL_WALK(5900, 3114, 3);
  NARRATE("* (The counter starts to grow, taller and taller...)");
  SAY(FACE_ALPHYS, "* Your phone has a JETPACK button!\n* I'm turning it on... now!");
  BATTLE(FORM_MTT_COOK);
  set_flag(F_HT_COOK, cook_won() ? 1 : 2);
  m = obj_get(ACTOR_METTATON);
  WALK(m, m->x, m->y - 200, 6);
  gone(obj_get(ACTOR_METTATON));
  SAY(FACE_ALPHYS, "* Ring...|* Phew... are you okay?\n* Keep going! Right, and up!");
  EV_END;
}

static int ev_elevator(Ev *c) {
  EV_BEGIN;
  NARRATE("* (The elevator up.)\n* (Get in?)@Yes@No");
  if (CHOICE() == 1) return 1;
  set_flag(F_HT_FLOOR1, 1);
  FADE_TO(ROOM_HT_HOTDOG, 6966, 2380, DIR_DOWN);
  EV_END;
}

int ht1_event(Ev *c) {
  switch (c->id) {
    case EV_HT_LAB: return ev_lab(c);
    case EV_HT_FRIDGE: return ev_fridge(c);
    case EV_HT_ITEM: return ev_item(c);
    case EV_VENT: return ev_vent(c);
    case EV_LASER_HIT: return ev_laser_hit(c);
    case EV_HT_CALL: return ev_call(c);
    case EV_SHOOT: return ev_shoot(c);
    case EV_HT_COOK: return ev_cook(c);
    case EV_HT_ELEVATOR: return ev_elevator(c);
  }
  return -1;
}
