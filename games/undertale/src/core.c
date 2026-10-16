/* The CORE: its approach (two black shapes back away inside), the lobby and its broken elevator, the fire pit, the
 * doorway's ambush (Madjick), the switch and its lasers (Alphys gets their order backwards), the crossroads and the
 * room north (Knight Knight), the laser bridge (Alphys cuts the power; it keeps coming back), the Core Branch: the
 * sage's path (its puzzle), the warrior's path (three fights, a switch) and the force field east that a patient one
 * waits out; The End's bridge, the Core End, and Mettaton's last show; the hallway, Alphys, and the elevator up to
 * the king's castle (newhome.c). (As the Undertale Wiki describes them; what they say is ours, in
 * short.) */
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
#include "res.h"
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
static Obj *touch(int x, int y, int w, int h, int event, int arg) {
  Obj *o = obj_trigger(x, y, w, h, event, ACTOR_TRIGGER);
  if (o) o->arg = (int16_t)arg;
  return o;
}
static Obj *npc(int name, uint16_t spr, int x, int y) {
  const uint16_t s[4] = {spr, spr, spr, spr};
  Obj *o = obj_npc(name, s, x, y, DIR_DOWN, 0);
  if (o) o->flags &= (uint16_t)~OF_SOLID;
  return o;
}
static void field(int x, int y, int w, int h) {
  Obj *o = obj_trigger(x, y, w, h, EV_NONE, ACTOR_FIELD);
  if (o) o->flags = OF_SOLID;
}
static void fields_down(void) {
  for (Obj *o = obj_find(ACTOR_FIELD); o; o = obj_find(ACTOR_FIELD)) gone(o);
}

/* Alphys's calls, one bit each (F_CORE_CALLS, then F_CORE_CALLS2) */
enum { CALL_IN, CALL_LOBBY, CALL_LIFT, CALL_PIT, CALL_AMBUSH, CALL_SWITCH, CALL_ORDER, CALL_EAST, CALL_NORTH,
       CALL_KNIGHT, CALL_POWER, CALL_STOP, CALL_BRANCH };
static bool called(int n) { return flag(n < 8 ? F_CORE_CALLS : F_CORE_CALLS2) >> (n & 7) & 1; }
static void set_called(int n) {
  int f = n < 8 ? F_CORE_CALLS : F_CORE_CALLS2;
  set_flag(f, flag(f) | 1 << (n & 7));
}
static void call_at(int x, int y, int w, int h, int n) {
  if (!called(n)) touch(x, y, w, h, EV_CORE_CALL, n);
}

/* ---------------------------------------------------------------- the lasers */
/* the ambush's: three beams that sweep across from the bridge once the switch is pressed (blue, blue, orange);
 * the bridge's: its emitters, and the wall of blue ones, while the power is on */
#define AMB_Y0 1339
#define AMB_Y1 1486
#define BR_Y0 1300
#define BR_Y1 1402
static int sweep_t = -1;     /* frames since the switch, -1 none */
static int power_t;          /* the bridge's power: on while power_t % CYCLE < ON */
#define CYCLE 130
#define ON 55
static bool power_on;
static int laser_cool;
static const int16_t bridge_lasers[][2] = {   /* x, blue */
  {1267, 1}, {1309, 1}, {1348, 0}, {1388, 1}, {1428, 0}, {1469, 1},
};
#define WALL_X0 1504
#define WALL_X1 2000
#define WALL_STEP 19

static int sweep_x(int i) { return 560 - (sweep_t - i * 60) * 3; }
static bool sweep_blue(int i) { return i < 2; }
static bool laser_hits(int x, bool blue, int y0, int y1) {
  if (pl.x + 3 > x + 1 || pl.x + 17 < x - 1 || pl.y + 29 < y0 || pl.y + 20 > y1) return false;
  return blue ? pl.moving : !pl.moving;
}
static void laser_hit(void) {
  if (laser_cool || event_running()) return;
  event_start(EV_CORE_LASER, NULL);
}
static void lasers_watch(void) {
  if (laser_cool) laser_cool--;
  if (g_room == ROOM_CORE_WEST && sweep_t >= 0) {
    sweep_t++;
    for (int i = 0; i < 3; i++) {
      int x = sweep_x(i);
      if (x < 560 && x > 100 && laser_hits(x, sweep_blue(i), AMB_Y0, AMB_Y1)) laser_hit();
    }
    if (sweep_x(2) < 90) {
      sweep_t = -1;
      if (!called(CALL_ORDER)) event_start(EV_CORE_ORDER, NULL);
    }
  }
  if (g_room == ROOM_CORE_BRIDGE && flag(F_CORE_POWER) == 1 && called(CALL_STOP)) {
    power_t++;
    power_on = power_t % CYCLE < ON;
    if (power_on && power_t % CYCLE > 6) {   /* (a moment to stop when it comes back on) */
      for (unsigned i = 0; i < sizeof bridge_lasers / sizeof bridge_lasers[0]; i++)
        if (laser_hits(bridge_lasers[i][0], bridge_lasers[i][1], BR_Y0, BR_Y1)) laser_hit();
      for (int x = WALL_X0; x <= WALL_X1; x += WALL_STEP)
        if (laser_hits(x, true, BR_Y0, BR_Y1)) laser_hit();
    }
  } else power_on = false;
}
static void draw_beam(int x, int y0, int y1, bool blue) {
  uint16_t c = blue ? rgb565(0x14A9FF) : rgb565(0xFCA600);
  gfx_rect(x - 1 - cam_x, y0 - cam_y, 3, y1 - y0, c, (g_frame / 2) % 2 ? 255 : 200);
}

void core_draw(void) {
  if (world_room_info()->area != AREA_CORE) return;
  for (int i = 0; i < MAX_OBJS; i++) {
    const Obj *o = &objs[i];
    if (o->kind == OBJ_NONE || o->name != ACTOR_FIELD) continue;
    uint16_t c = (g_frame / 3) % 2 ? rgb565(0x60FFFF) : rgb565(0x20A0C0);
    gfx_rect(o->x - cam_x, o->y - cam_y, o->bw, o->bh, c, 200);
  }
  if (g_room == ROOM_CORE_WEST && sweep_t >= 0)
    for (int i = 0; i < 3; i++) {
      int x = sweep_x(i);
      if (x < 560 && x > 100) draw_beam(x, AMB_Y0, AMB_Y1, sweep_blue(i));
    }
  /* the bridge's: on until Alphys cuts the power, then on and off */
  bool on = g_room == ROOM_CORE_BRIDGE && (!flag(F_CORE_POWER) || power_on);
  /* (the power flickers a moment before it comes back) */
  bool flicker = g_room == ROOM_CORE_BRIDGE && flag(F_CORE_POWER) == 1 && called(CALL_STOP) && power_t % CYCLE >= CYCLE - 20 &&
                 g_frame % 4 < 2;
  if (on || flicker) {
    for (unsigned i = 0; i < sizeof bridge_lasers / sizeof bridge_lasers[0]; i++)
      draw_beam(bridge_lasers[i][0], BR_Y0, BR_Y1, bridge_lasers[i][1]);
    for (int x = WALL_X0; x <= WALL_X1; x += WALL_STEP) draw_beam(x, BR_Y0, BR_Y1, true);
  }
}

/* the force field by the east bridge: the sign says patience (a minute in front of it, and it's gone) */
static int field_wait;
void core_update(void) {
  if (world_room_info()->area != AREA_CORE) return;
  lasers_watch();
  if (g_room == ROOM_CORE_BRANCH && !flag(F_CORE_FIELD)) {
    if (pl.x > 2440 && pl.x < 2530 && pl.y > 830 && pl.y < 880) {
      if (++field_wait >= 60 * 30) {
        set_flag(F_CORE_FIELD, 1);
        fields_down();
        event_start(EV_CORE_FIELD, NULL);
      }
    } else field_wait = 0;
  }
}

/* ---------------------------------------------------------------- the rooms */
void core_setup(int room, int from) {
  (void)from;
  if (room < ROOM_CORE_APPROACH || room > ROOM_CORE_HALL) return;
  sweep_t = -1, power_t = 0, field_wait = 0;
  switch (room) {
    case ROOM_CORE_APPROACH:
      if (!flag(F_CORE_IN)) {
        npc(ACTOR_ITEM, SPR_SAUSAGE, 316, 2560);
        npc(ACTOR_ANY, SPR_SAUSAGE, 326, 2528);
        touch(290, 2650, 60, 20, EV_CORE_APPROACH, 0);
      }
      break;
    case ROOM_CORE_LOBBY:
      call_at(298, 2080, 42, 30, CALL_LOBBY);
      thing(296, 1986, 48, 54, EV_CORE_LIFT, ACTOR_DOOR, 0);
      obj_sign(186, 1980, 28, 26, "* (A screen: CORE, the source of\n  the Underground's power.)");
      break;
    case ROOM_CORE_PIT:
      call_at(440, 1800, 79, 20, CALL_PIT);
      obj_sign(440, 1740, 79, 22, "* (A pit of fire, far below.\n  You can feel its heat.)");
      break;
    case ROOM_CORE_DOORWAY:
      if (!flag(F_CORE_MADJICK)) touch(100, 1770, 120, 16, EV_CORE_MADJICK, 0);
      break;
    case ROOM_CORE_WEST:
      call_at(119, 1420, 159, 16, CALL_AMBUSH);
      thing(250, 1310, 20, 50, EV_CORE_SWITCH, ACTOR_SWITCH, 0);
      if (!flag(F_CORE_SWITCH)) field(282, 1350, 6, 40);
      call_at(740, 1339, 12, 66, CALL_EAST);
      call_at(880, 1339, 12, 66, CALL_NORTH);
      if (!flag(F_CORE_KNIGHT)) touch(766, 1150, 147, 14, EV_CORE_KNIGHT, 0);
      break;
    case ROOM_CORE_BRIDGE:
      if (flag(F_CORE_POWER) < 2) {
        call_at(1232, 1358, 12, 44, CALL_POWER);
        call_at(1490, 1358, 12, 44, CALL_STOP);
        touch(2062, 1300, 58, 12, EV_CORE_POWER, 0);
      }
      break;
    case ROOM_CORE_BRANCH:
      if (!called(CALL_BRANCH)) touch(2060, 1118, 58, 12, EV_CORE_BRANCH, 0);   /* (on the way in) */
      obj_save(2080, 1100, "* (The air smells of ozone. It\n  fills you with determination.)");
      obj_sign(2010, 1056, 20, 44, "* (North: the warrior's path.)\n* (West: the sage's path.)\n* (Any path leads to The End.)");
      obj_sign(2150, 1056, 20, 44, "* (East... The End.)");
      obj_sign(1670, 816, 22, 44, "* (Solve this puzzle, and The\n  End will open.)");
      obj_sign(1770, 312, 22, 44, "* (Why not stop for a snack?)");
      obj_sign(2170, 816, 22, 44, "* (Get lost... and stay lost.)");
      obj_sign(2010, 576, 22, 44, "* (Cross the room to the north,\n  and The End will open.)");
      obj_sign(2330, 816, 22, 44, "* (East! This way is The End.)");
      obj_sign(2470, 816, 22, 44, "* (I can't fight. I can't think.)\n* (But with patience, I'll make\n  my way through.)");
      thing(1452, 834, 34, 28, EV_CORE_SAGE, ACTOR_ITEM, 0);
      thing(1758, 372, 22, 20, EV_CORE_TRASH, ACTOR_ITEM, 0);
      thing(2398, 372, 22, 20, EV_CORE_TRASH, ACTOR_ITEM, 1);
      if (!flag(F_CORE_FIELD)) field(2524, 856, 10, 48);
      break;
    case ROOM_CORE_WARRIOR:
      for (int i = flag(F_CORE_WARRIOR); i < 3; i++) touch(2220 + i * 130, 160, 10, 40, EV_CORE_WARRIOR, i);
      thing(2660, 128, 20, 30, EV_CORE_WSWITCH, ACTOR_SWITCH, 0);
      break;
    case ROOM_CORE_END:
      set_flag(F_CORE_END, 1);
      obj_save(3478, 866, "* (The elevator to the king's\n  castle must be past this door.)\n* (You're filled with determination.)");
      thing(3418, 806, 40, 50, EV_CORE_LIFT, ACTOR_DOOR, 1);
      break;
    case ROOM_CORE_MTT:
      if (!flag(F_CORE_MTT)) {
        Obj *m = npc(ACTOR_METTATON, SPR_MTT_OW, 3360, 500);
        if (m) m->flags |= OF_ANIM, m->anim_speed = 5;
        touch(3342, 620, 36, 12, EV_CORE_MTT, 0);
        field(3342, 336, 36, 8);   /* (the way out north: shut until the show's over) */
      } else {
        Obj *m = npc(ACTOR_METTATON, flag(F_CORE_MTT) == 2 ? SPR_MTT_OW : SPR_MTTEX_OW, 3360, 500);
        (void)m;
        obj_sign(3346, 480, 30, 26, flag(F_CORE_MTT) == 2 ? "* (It's completely trashed.)" : "* (Mettaton. Out of power.)");
      }
      break;
    case ROOM_CORE_HALL:
      if (!flag(F_CORE_HALL)) {
        static const uint16_t s[4] = {SPR_ALPHYS_OW, SPR_ALPHYS_OW, SPR_ALPHYS_OW, SPR_ALPHYS_OW};
        Obj *a = obj_npc(ACTOR_ALPHYS, s, 3370, 230, DIR_UP, 0);
        if (a) a->flags &= (uint16_t)~OF_SOLID;
        touch(4220, 118, 10, 42, EV_CORE_HALL, 0);
      }
      thing(4278, 92, 46, 28, EV_CORE_LIFT, ACTOR_DOOR, 2);
      break;
  }
}

/* ---------------------------------------------------------------- the events */
static int ev_approach(Ev *c) {
  Obj *a = obj_get(ACTOR_ITEM), *b = obj_get(ACTOR_ANY);
  EV_BEGIN;
  if (c->self) gone(c->self);
  /* two shapes, black, back away up the path, and in */
  WALK_TO(b, b->x, 2290, 2);
  WALK(a, a->x, 2300, 2);
  gone(obj_get(ACTOR_ITEM)), gone(obj_get(ACTOR_ANY));
  set_flag(F_CORE_IN, 1);
  SAY(FACE_ALPHYS, "* Ring...|* Th-that's the CORE! We made it!|* ...wait. Were those\n  monsters? Nobody's supposed\n  to be in there...|* It's fine. Probably.\n* I'll guide you through!");
  EV_END;
}

static int ev_call(Ev *c) {
  EV_BEGIN;
  if (c->self) c->v[0] = c->self->arg;
  if (c->self) gone(c->self);
  if (called(c->v[0])) return 1;
  set_called(c->v[0]);
  switch (c->v[0]) {
    case CALL_LOBBY:
      SAY(FACE_ALPHYS, "* Ring...|* The elevator in the middle\n  goes right to the castle!\n* Head for it!");
      break;
    case CALL_PIT:
      SAY(FACE_ALPHYS, "* Ring...|* A pit of fire? That's not on\n  my map...|* Um. Go back, and try the\n  other way.");
      break;
    case CALL_AMBUSH:
      SAY(FACE_ALPHYS, "* Ring...|* See that switch? It turns off\n  the force field...|* ...but it sets off lasers!\n* Orange, orange, then blue.\n* You'll be fine!");
      break;
    case CALL_ORDER:
      SAY(FACE_ALPHYS, "* Ring...|* Wait. That was blue, blue,\n  then orange.|* I had it backwards.\n* S-sorry...");
      break;
    case CALL_EAST:
      SAY(FACE_ALPHYS, "* Ring...|* East! Keep going east!");
      break;
    case CALL_NORTH:
      SAY(FACE_ALPHYS, "* Ring...|* N-no, wait! North!\n* Go north first!");
      break;
    case CALL_POWER:
      SAY(FACE_ALPHYS, "* Ring...|* Those lasers... I can't turn\n  them off.|* So I'll cut the power to the\n  whole room!");
      if (!flag(F_CORE_POWER)) set_flag(F_CORE_POWER, 1);
      NARRATE("* (The lights go out.)");
      field(1228, 1356, 6, 48);   /* (back west: a field on its own battery) */
      break;
    case CALL_STOP:
      SAY(FACE_ALPHYS, "* Ring...|* Stop! STOP MOVING!\n* The power's back on!|* I can't keep it off!\n* Move while it's off, and\n  freeze when it's on!");
      break;
  }
  EV_END;
}

/* the bridge crossed: its power stays off */
static int ev_power(Ev *c) {
  EV_BEGIN;
  if (c->self) gone(c->self);
  set_flag(F_CORE_POWER, 2);
  fields_down();
  SAY(FACE_ALPHYS, "* Ring...|* Phew... you made it.\n* Everything's under control!\n* Totally!");
  EV_END;
}

static int ev_laser(Ev *c) {
  EV_BEGIN;
  g_exclaim = 20;
  WAIT(15);
  BATTLE(FORM_LASERS);
  laser_cool = 50;
  EV_END;
}

static int ev_switch(Ev *c) {
  EV_BEGIN;
  if (flag(F_CORE_SWITCH)) {
    NARRATE("* (The switch is already pressed.)");
    return 1;
  }
  NARRATE("* (You press the switch.)\n* (The force field shuts off.)");
  set_flag(F_CORE_SWITCH, 1);
  fields_down();
  sweep_t = 0;
  EV_END;
}

static int ev_madjick(Ev *c) {
  EV_BEGIN;
  if (c->self) gone(c->self);
  SAY(FACE_ALPHYS, "* Ring...|* Watch out! Something's\n  coming at you!");
  g_exclaim = 20;
  WAIT(15);
  BATTLE(FORM_MADJICK);
  set_flag(F_CORE_MADJICK, bt.mon[0].killed ? 2 : 1);
  SAY(FACE_ALPHYS, "* Ring...|* Are you okay?|* Why are there so many\n  monsters in here...?");
  EV_END;
}

static int ev_knight(Ev *c) {
  EV_BEGIN;
  if (c->self) gone(c->self);
  g_exclaim = 20;
  WAIT(15);
  BATTLE(FORM_KNIGHT);
  set_flag(F_CORE_KNIGHT, bt.mon[0].killed ? 2 : 1);
  set_called(CALL_KNIGHT);
  SAY(FACE_ALPHYS, "* Ring...|* Are you all right?!\n* I'm so sorry...|* Okay. East now. Really.");
  EV_END;
}

static int ev_branch(Ev *c) {
  EV_BEGIN;
  if (c->self) gone(c->self);
  set_called(CALL_BRANCH);
  SAY(FACE_ALPHYS, "* Ring...|* Okay, so, from here you go...\n* ...um...|* ...|* I don't know. My map's all\n  wrong.|* I'm sorry. I'll... stop\n  calling.");
  EV_END;
}

static int ev_sage(Ev *c) {
  EV_BEGIN;
  if (flag(F_CORE_SAGE)) {
    NARRATE("* (The puzzle is solved.)");
    return 1;
  }
  shoot_open(SHOOT_SAGE);
  WAIT_UNTIL(g_mode == MODE_WORLD);
  if (shoot_result()) {
    set_flag(F_CORE_SAGE, 1);
    set_flag(F_CORE_FIELD, 1);
    fields_down();   /* (the force field east, in this same room) */
    NARRATE("* (Far to the east, something\n  hums, then goes quiet.)");
  }
  EV_END;
}

static int ev_warrior(Ev *c) {
  static const uint8_t forms[3] = {FORM_WHIM_FROG, FORM_KNIGHT_MADJICK, FORM_FROG_ASTIG_WHIM};
  EV_BEGIN;
  c->v[0] = c->self ? c->self->arg : 0;
  if (c->self) gone(c->self);
  if (flag(F_CORE_WARRIOR) > c->v[0]) return 1;
  g_exclaim = 20;
  WAIT(15);
  BATTLE(forms[c->v[0]]);
  if (battle_result() != BR_FLED && battle_result() != BR_DIED) set_flag(F_CORE_WARRIOR, c->v[0] + 1);
  EV_END;
}

static int ev_wswitch(Ev *c) {
  EV_BEGIN;
  if (flag(F_CORE_WARRIOR) >= 4) {
    NARRATE("* (The switch is already flipped.)");
    return 1;
  }
  NARRATE("* (You flip the switch.)\n* (Far away, something hums,\n  then goes quiet.)");
  set_flag(F_CORE_WARRIOR, 4);
  set_flag(F_CORE_FIELD, 1);
  EV_END;
}

static int ev_field(Ev *c) {
  EV_BEGIN;
  NARRATE("* (The force field flickers...\n  and fades away.)");
  EV_END;
}

static int ev_trash(Ev *c) {
  EV_BEGIN;
  c->v[0] = c->self ? c->self->arg : 0;
  if (flag(c->v[0] ? F_CORE_TRASH2 : F_CORE_TRASH1)) {
    NARRATE("* (It's an empty trash can.)");
    return 1;
  }
  if (c->v[0]) {
    NARRATE("* (There's 100G inside the\n  trash can.)");
    sv.gold = (uint16_t)min_i(sv.gold + 100, 9999);
    set_flag(F_CORE_TRASH2, 1);
  } else if (item_add(ITEM_GLAMBURGER)) {
    NARRATE("* (There's a Glamburger in the\n  trash can. You take it.)");
    set_flag(F_CORE_TRASH1, 1);
  } else NARRATE("* (There's a Glamburger inside.)\n* (You're carrying too much.)");
  EV_END;
}

/* the elevators: the lobby's (it works once the far side's reached), the Core End's back to it; the last one, up
 * to the castle */
static int ev_lift(Ev *c) {
  EV_BEGIN;
  c->v[0] = c->self ? c->self->arg : 0;
  if (c->v[0] == 2) {
    if (!flag(F_CORE_HALL)) {
      NARRATE("* (The elevator to the king's\n  castle.)");
      return 1;
    }
    NARRATE("* (Take the elevator up to the\n  castle?)@Yes@No");
    if (CHOICE() == 1) return 1;
    set_flag(F_CORE_DONE, 1);
    FADE_TO(ROOM_NH_TOWN, 90, 4545, DIR_DOWN);
    return 1;
  }
  if (c->v[0] == 0 && !flag(F_CORE_END)) {
    NARRATE("* (The elevator doesn't work.)");
    if (!called(CALL_LIFT)) {
      set_called(CALL_LIFT);
      SAY(FACE_ALPHYS, "* Ring...|* It's not working?\n* B-but my map says...|* Okay. Um. Go left!\n* No, right! ...Either!");
    }
    return 1;
  }
  NARRATE(c->v[0] ? "* (Take the elevator down to\n  the lobby?)@Yes@No" : "* (Take the elevator to the\n  Core End?)@Yes@No");
  if (CHOICE() == 1) return 1;
  if (c->v[0]) FADE_TO(ROOM_CORE_LOBBY, 310, 2045, DIR_DOWN);
  else FADE_TO(ROOM_CORE_END, 3428, 862, DIR_DOWN);
  EV_END;
}

/* Mettaton's room: the door locks; he tells Frisk the truth; his box, his switch, his new body */
static int ev_mtt(Ev *c) {
  Obj *m = obj_get(ACTOR_METTATON);
  EV_BEGIN;
  if (c->self) gone(c->self);
  NARRATE("* (The door locks behind you.)");
  WAIT_UNTIL(cam_pan(3200, 420, 4));   /* (up to him) */
  text_set_top(0);
  NARRATE("* OH YES. IT WAS ME.\n* THE MONSTERS. THE ELEVATOR.\n  THE MAZE.|* AND ALL THOSE PUZZLES?\n* ALPHYS PLANNED THEM, SO SHE\n  COULD SAVE YOU FROM ME.|* SHE WANTED TO BE YOUR HERO.\n* BUT I GOT BORED, DARLING.");
  NARRATE("* WITH YOUR SOUL, I'LL CROSS THE\n  BARRIER...|* ...AND BE A STAR UP THERE!\n* NOW... FOR THE GRAND FINALE!");
  text_set_top(-1);
  BATTLE(FORM_MTT_BOX);
  if (battle_result() == BR_DIED) {
    cam_free();
    return 1;
  }
  text_set_top(0);
  g_shake = 10;
  NARRATE("* (Smoke pours out of Mettaton.)\n* WELL, WELL...|* MY SWITCH, DARLING? COULDN'T\n  WAIT FOR MY NEW LOOK?|* NAUGHTY. BUT FINE...\n* IT'S TIME FOR MY PREMIERE!");
  text_set_top(-1);
  mttex_reset();
  BATTLE(FORM_MTT_EX);
  if (battle_result() == BR_DIED) {
    cam_free();
    return 1;
  }
  text_set_top(0);
  m = obj_get(ACTOR_METTATON);
  if (bt.mon[0].killed) {
    set_flag(F_CORE_MTT, 2);
    NARRATE("* (Mettaton falls back into his\n  box. It's completely trashed.)");
  } else {
    set_flag(F_CORE_MTT, 1);
    m->spr[0] = m->spr[1] = m->spr[2] = m->spr[3] = SPR_MTTEX_OW, m->flags &= (uint16_t)~OF_ANIM;
    NARRATE("* THESE RATINGS... THE BEST\n  I'VE EVER HAD!|* AND... WE'RE GETTING CALLS?\n* GO AHEAD, CALLER!");
    NARRATE("* (A small, sad voice.)\n* oh... hi mettaton...|* i just... wanted to say...\n* your show is all i watch...|* please don't leave...");
    NARRATE("* MORE CALLS... ALL SAYING THE\n  SAME THING.|* ...SO THE UNDERGROUND NEEDS\n  ME MORE THAN I THOUGHT.|* THEN I'LL STAY.\n* AND YOU, DARLING...\n  YOU CAN STAND UP TO ASGORE.");
    NARRATE("* ...THOUGH THIS BODY EATS\n  BATTERY LIKE...|* (Mettaton runs out of power.)");
  }
  text_set_top(-1);
  fields_down();
  /* Alphys, in through the door */
  {
    static const uint16_t s[4] = {SPR_ALPHYS_OW, SPR_ALPHYS_OW, SPR_ALPHYS_OW, SPR_ALPHYS_OW};
    Obj *a = obj_npc(ACTOR_ALPHYS, s, 3360, 760, DIR_UP, 0);
    if (a) a->flags &= (uint16_t)~OF_SOLID;
  }
  WALK(obj_get(ACTOR_ALPHYS), 3360, 600, 3);
  if (flag(F_CORE_MTT) == 2) SAY(FACE_ALPHYS, "* I-I got the door open...\n* ...oh.|* Mettaton...|* ...let's just go.");
  else SAY(FACE_ALPHYS, "* I-I got the door open! Are\n  you okay?! Is Mettaton...|* ...he's just out of power.\n* Oh, thank goodness...|* Come on. The elevator's\n  this way.");
  gone(obj_get(ACTOR_ALPHYS));
  cam_free();
  EV_END;
}

/* the hallway: by the elevator, Alphys tells Frisk the truth about the way home */
static int ev_hall(Ev *c) {
  Obj *a = obj_get(ACTOR_ALPHYS);
  EV_BEGIN;
  if (c->self) gone(c->self);
  WALK(a, pl.x - 30, pl.y + 10, 3);
  SAY(FACE_ALPHYS, "* W-wait! Before you go...|* There's something I didn't\n  tell you.|* To leave, you need a human\n  SOUL, and a monster's...");
  SAY(FACE_ALPHYS, "* ASGORE's the only one who\n  can help you through...|* ...but he needs your SOUL\n  for the barrier.|* To go home, you'll have to\n  fight him. And...");
  SAY(FACE_ALPHYS, "* ...one of you won't come\n  back.|* I'm sorry. I made it all up,\n  so I'd seem like a hero.|* Good luck. You deserve a\n  better friend than me.");
  set_flag(F_CORE_HALL, 1);
  a = obj_get(ACTOR_ALPHYS);
  WALK(a, 3360, a->y, 3);
  gone(obj_get(ACTOR_ALPHYS));
  EV_END;
}

int core_event(Ev *c) {
  switch (c->id) {
    case EV_CORE_APPROACH: return ev_approach(c);
    case EV_CORE_CALL: return ev_call(c);
    case EV_CORE_ORDER:
      if (!c->at) c->v[0] = CALL_ORDER;
      return ev_call(c);
    case EV_CORE_BRANCH: return ev_branch(c);
    case EV_CORE_POWER: return ev_power(c);
    case EV_CORE_LASER: return ev_laser(c);
    case EV_CORE_SWITCH: return ev_switch(c);
    case EV_CORE_MADJICK: return ev_madjick(c);
    case EV_CORE_KNIGHT: return ev_knight(c);
    case EV_CORE_SAGE: return ev_sage(c);
    case EV_CORE_WARRIOR: return ev_warrior(c);
    case EV_CORE_WSWITCH: return ev_wswitch(c);
    case EV_CORE_FIELD: return ev_field(c);
    case EV_CORE_TRASH: return ev_trash(c);
    case EV_CORE_LIFT: return ev_lift(c);
    case EV_CORE_MTT: return ev_mtt(c);
    case EV_CORE_HALL: return ev_hall(c);
  }
  return -1;
}
