/* The Ruins, from the bed of golden flowers to the leaf pile: Flowey, then Toriel guiding the fallen human
 * through her puzzles, the Dummy, the spikes, the independence test. Texts are the game's. */
#include <stdio.h>
#include <string.h>
#include "battle.h"
#include "event.h"
#include "events.h"
#include "game.h"
#include "menu.h"
#include "monsters.h"
#include "ruins.h"
#include "save.h"
#include "world.h"

/* ---------------------------------------------------------------- the rooms' things */
void ruins1_setup(int room, int from) {
  (void)from;
  switch (room) {
    case ROOM_AREA1:
      if (flag(F_FLOWEY_MET)) obj_sign(120, 6015, 60, 40, "* (Golden flowers.)\n* (They must have broken your\n  fall.)");
      break;
    case ROOM_AREA1_2:
      if (!flag(F_FLOWEY_MET)) {
        static const uint16_t s[4] = {SPR_FLOWEY};
        Obj *f = obj_npc(ACTOR_FLOWEY, s, 508, 5768, DIR_DOWN, 0);
        if (f) f->flags = OF_SOLID, obj_set_box(f, 4, 16, 17, 8);
        obj_trigger(430, 5800, 180, 40, EV_FLOWEY_MEET, ACTOR_TRIGGER);
      }
      break;
    case ROOM_RUINS1:
      obj_save(510, 5275, "* (The shadow of the ruins\n  looms above, filling you with\n  determination.)");
      if (flag(F_TORIEL_MET) && !flag(F_RUINS1_DONE)) {
        obj_toriel(504, 5330, DIR_DOWN);
        obj_trigger(380, 5380, 280, 180, EV_RUINS1_TORIEL, ACTOR_TRIGGER);
      }
      break;
    case ROOM_RUINS2:
      obj_sign(440, 4880, 20, 20, "* \"Only the fearless may\n  proceed.\"|* \"Brave ones, foolish ones.\"|* \"Both walk not the middle\n  road.\"");
      for (int k = 0; k < 6; k++) {
        static const int16_t px[6] = {540, 580, 560, 540, 580, 560}, py[6] = {4938, 4938, 4958, 4978, 4978, 4998};
        Obj *p = obj_deco(SPR_O_GROUNDSWITCH1, px[k], py[k], false);
        if (p) p->flags |= OF_BACK, p->frame = flag(F_RUINS2_DONE) && (k == 0 || k == 1 || k == 3 || k == 4) ? 1 : 0, p->arg = (int16_t)k;
      }
      {
        Obj *lever = obj_deco(SPR_O_SWITCH, 560, 4888, false);
        if (lever) lever->flags |= OF_BACK, lever->frame = flag(F_RUINS2_DONE) ? 1 : 0, lever->name = ACTOR_SWITCH;
      }
      if (!flag(F_RUINS2_DONE)) {
        Obj *d = obj_deco(SPR_O_RUINSDOOR1, 480, 4860, true);
        if (d) d->name = ACTOR_DOOR, obj_set_box(d, 0, 30, 40, 30);
        obj_toriel(500, 4952, DIR_DOWN);
        obj_trigger(400, 4920, 240, 140, EV_RUINS2_TORIEL, ACTOR_TRIGGER);
      }
      break;
    case ROOM_RUINS3: {
      obj_sign(488, 4724, 20, 18, "* \"Stay on the path.\"");
      static const int16_t sx[3] = {742, 942, 980};
      for (int k = 0; k < 3; k++) {
        Obj *s = obj_deco(SPR_O_SWITCH, sx[k], 4650, false);
        if (!s) continue;
        s->name = (uint8_t)(ACTOR_SWITCH + k), s->flags = OF_TALK | OF_BACK, s->event = EV_RUINS3_SWITCH;
        obj_set_box(s, 0, 0, 20, 34);
        s->frame = flag(F_RUINS3_SW1 + k) ? 1 : 0;
      }
      obj_spikes(1060, 4740, 1, 2, !flag(F_RUINS3_DONE), ACTOR_SPIKES);
      if (!flag(F_RUINS3_DONE)) {
        obj_toriel(470, 4716, DIR_DOWN);
        if (!flag(F_RUINS3_SW1) && !flag(F_RUINS3_SW2)) obj_trigger(440, 4760, 120, 70, EV_RUINS3_TORIEL, ACTOR_TRIGGER);
      }
      break;
    }
    case ROOM_RUINS4: {
      Obj *d = obj_deco(SPR_O_DUMMY, 1300, 4690, true);
      if (d) d->name = ACTOR_DUMMY, d->flags |= OF_TALK, d->event = EV_RUINS4_DUMMY, obj_set_box(d, 2, 14, 16, 18);
      if (!flag(F_DUMMY)) {
        obj_toriel(1236, 4690, DIR_RIGHT);
        obj_trigger(1080, 4680, 120, 140, EV_RUINS4_TORIEL, ACTOR_TRIGGER);
      }
      break;
    }
    case ROOM_RUINS5:
      obj_sign(1680, 4478, 20, 20, "* \"The western room is the\n  eastern room's blueprint.\"");
      if (!flag(F_RUINS5_DONE)) {
        obj_toriel(1250, 4470, DIR_DOWN);
        obj_trigger(1230, 4500, 90, 100, EV_RUINS5_TORIEL, ACTOR_TRIGGER);
      }
      break;
    case ROOM_RUINS6: {
      Obj *p = obj_deco(SPR_O_PILLAR, 4602, 4400, true);
      if (p) p->name = ACTOR_PILLAR, p->flags |= OF_TALK, p->event = EV_RUINS6_PILLAR, obj_set_box(p, 4, 44, 32, 16);
      if (!flag(F_RUINS6_DONE)) {
        obj_toriel(2330, 4466, DIR_RIGHT);
        obj_trigger(2280, 4440, 120, 110, EV_RUINS6_TORIEL, ACTOR_TRIGGER);
      }
      break;
    }
    case ROOM_RUINS7: {
      obj_save(5010, 4500, "* (Playfully crinkling through\n  the leaves fills you with\n  determination.)");
      static const uint16_t fs[4] = {SPR_O_SMALLFROG};
      if (!sv.kills) obj_npc(ACTOR_FROGGIT, fs, 4901, 4440, DIR_DOWN, EV_RUINS7_FROGGIT);
      if (flag(F_PHONE) && !flag(F_LEAFPILE_CALL)) event_start(EV_RUINS7_CALL, NULL);
      break;
    }
  }
}

/* ---------------------------------------------------------------- the events */
static const char flowey_hi[] =
  "`0* Howdy!^2 I'm #YFLOWEY#X.^2\n* #YFLOWEY#X the #YFLOWER#X!|"
  "`1* Hmmm...|"
  "`0* You're new to the\n  UNDERGROUND, aren'tcha?|"
  "`1* Golly, you must be so\n  confused.|"
  "`0* Someone ought to teach you\n  how things work around here!|"
  "`0* I guess little old me will\n  have to do.|"
  "`7* Ready?^2 Here we go!";

static int ev_flowey_meet(Ev *c) {
  Obj *f;
  EV_BEGIN;
  if (c->self) c->self->kind = OBJ_NONE;
  SAY(FACE_FLOWEY, flowey_hi);
  BATTLE(FORM_FLOWEY);
  /* Toriel stands where Flowey was */
  f = obj_find(ACTOR_FLOWEY);
  if (f) f->kind = OBJ_NONE;
  obj_toriel(500, 5742, DIR_DOWN);
  set_flag(F_FLOWEY_MET, 1);
  set_flag(F_TORIEL_MET, 1);
  WAIT(10);
  SAY(FACE_TORIEL, "`0* This way.");
  LEAVE(obj_find(ACTOR_TORIEL), 2, 504, 5580);
  EV_END;
}

static int ev_ruins1_toriel(Ev *c) {
  if (c->self) c->self->kind = OBJ_NONE;
  /* she goes up the stairs, then through the door */
  LEAVE(obj_find(ACTOR_TORIEL), 2, 580, 5300, 606, 5250, 606, 5160, 504, 5150, 504, 5100);
  set_flag(F_RUINS1_DONE, 1);
  return 1;
}

static int ev_ruins2_toriel(Ev *c) {
  Obj *t = obj_find(ACTOR_TORIEL);
  EV_BEGIN;
  if (c->self) c->self->kind = OBJ_NONE;
  if (!t) return 1;
  t->dir = DIR_DOWN;
  SAY(FACE_TORIEL, "`0* Welcome to your new\n  home, little one.|* Let me show you how things\n  work in the RUINS.");
  /* over the switches, not the middle road */
  WALK(t, 534, 4952, 2);
  WALK(t, 534, 4898, 2);
  c->v[0] = 0;
  for (c->v[1] = 0; c->v[1] < MAX_OBJS; c->v[1]++)
    if (objs[c->v[1]].kind == OBJ_DECO && objs[c->v[1]].spr[0] == SPR_O_GROUNDSWITCH1 && objs[c->v[1]].arg == 0) objs[c->v[1]].frame = 1;
  WALK(t, 574, 4898, 2);
  for (c->v[1] = 0; c->v[1] < MAX_OBJS; c->v[1]++)
    if (objs[c->v[1]].kind == OBJ_DECO && objs[c->v[1]].spr[0] == SPR_O_GROUNDSWITCH1 && objs[c->v[1]].arg == 1) objs[c->v[1]].frame = 1;
  WALK(t, 574, 4938, 2);
  for (c->v[1] = 0; c->v[1] < MAX_OBJS; c->v[1]++)
    if (objs[c->v[1]].kind == OBJ_DECO && objs[c->v[1]].spr[0] == SPR_O_GROUNDSWITCH1 && objs[c->v[1]].arg == 4) objs[c->v[1]].frame = 1;
  WALK(t, 534, 4938, 2);
  for (c->v[1] = 0; c->v[1] < MAX_OBJS; c->v[1]++)
    if (objs[c->v[1]].kind == OBJ_DECO && objs[c->v[1]].spr[0] == SPR_O_GROUNDSWITCH1 && objs[c->v[1]].arg == 3) objs[c->v[1]].frame = 1;
  WALK(t, 544, 4880, 2);
  t->dir = DIR_UP;
  WAIT(12);
  {
    Obj *l = obj_find(ACTOR_SWITCH), *d = obj_find(ACTOR_DOOR);
    if (l) l->frame = 1;
    if (d) d->kind = OBJ_NONE;
  }
  WAIT(15);
  t->dir = DIR_DOWN;
  SAY(FACE_TORIEL, "`0* The RUINS are full of\n  puzzles that open the way.|* Solve each one to reach\n  the next room.");
  set_flag(F_RUINS2_DONE, 1);
  LEAVE(t, 2, 484, 4900, 484, 4840);
  EV_END;
}

static int ev_ruins3_toriel(Ev *c) {
  Obj *t = obj_find(ACTOR_TORIEL);
  EV_BEGIN;
  if (c->self) c->self->kind = OBJ_NONE;
  if (!t) return 1;
  SAY(FACE_TORIEL, "`0* Here you must flip some\n  switches.|* I marked the right ones\n  for you.");
  WALK_ON(t, 3, DIR_LEFT, 560, 4716, 990, 4716);
  EV_END;
}

static int ev_ruins3_switch(Ev *c) {
  int k = c->self ? c->self->name - ACTOR_SWITCH : 0;
  EV_BEGIN;
  if (flag(F_RUINS3_SW1 + k)) {
    NARRATE("* (The switch is already\n  flipped.)");
    return 1;
  }
  if (k == 2 && !flag(F_RUINS3_DONE) && obj_find(ACTOR_TORIEL)) {
    SAY(FACE_TORIEL, "`3* Oh? Not that switch.|`0* Only the ones I marked,\n  my child.");
    return 1;
  }
  set_flag(F_RUINS3_SW1 + k, 1);
  c->self->frame = 1;
  NARRATE("* (You flipped the switch.)");
  if (flag(F_RUINS3_SW1) && flag(F_RUINS3_SW2) && !flag(F_RUINS3_DONE)) {
    set_flag(F_RUINS3_DONE, 1);
    {
      Obj *s = obj_find(ACTOR_SPIKES);
      if (s) s->frame = 1, s->flags &= (uint16_t)~OF_SOLID;
    }
    WAIT(10);
    if (obj_find(ACTOR_TORIEL)) {
      SAY(FACE_TORIEL, "`2* Splendid!\n* I am proud of you, little\n  one.|`0* Let us move to the next\n  room.");
      LEAVE(obj_get(ACTOR_TORIEL), 3, 1040, 4730, 1100, 4730);
    }
  }
  EV_END;
}

static int ev_ruins4_toriel(Ev *c) {
  EV_BEGIN;
  if (c->self) c->self->kind = OBJ_NONE;
  SAY(FACE_TORIEL, "`0* Monsters down here may\n  try to fight you.|`2* Do not fear!\n* It is simple.|`0* In a FIGHT, talk kindly\n  and wait.|* I will come and sort\n  things out.|* Try talking to the\n  dummy.");
  EV_END;
}

static int ev_ruins4_dummy(Ev *c) {
  EV_BEGIN;
  if (flag(F_DUMMY)) {
    NARRATE("* (A cotton dummy.)");
    return 1;
  }
  BATTLE(FORM_DUMMY);
  if (battle_result() == BR_FLED) {
    set_flag(F_DUMMY, 3);
    SAY(FACE_TORIEL, "`3* ...you ran from it.|`0* Not a bad choice, really.\n* Avoiding a fight is often\n  wise.|* Still, it is just a\n  cotton dummy.|* Come along.");
  } else if (bt.mon[0].killed) {
    set_flag(F_DUMMY, 2);
    if (c->self) c->self->kind = OBJ_NONE;
    SAY(FACE_TORIEL, "`3* Oh! Dummies are for\n  talking, not hitting!|`1* We do not want to hurt\n  anyone.|`0* Come along.");
  } else if (bt.turn >= 5 && !flag(F_DUMMY)) {
    set_flag(F_DUMMY, 4);
    SAY(FACE_TORIEL, "`3* ...the dummy grew tired\n  of being looked at.|`0* Well. Let us move on.");
  } else {
    set_flag(F_DUMMY, 1);
    SAY(FACE_TORIEL, "`2* Ah, very good!\n* You are very good.");
  }
  LEAVE(obj_find(ACTOR_TORIEL), 2, 1244, 4640, 1244, 4560);
  EV_END;
}

/* the spikes' safe way: the cells of the blueprint (tools/rooms.py has the same) */
static const int16_t spike_path[][2] = {
  {1916, 4497}, {2000, 4497}, {2000, 4458}, {2060, 4458}, {2060, 4497}, {2160, 4497}, {2160, 4458}, {2236, 4458}};

static int ev_ruins5_toriel(Ev *c) {
  Obj *t = obj_find(ACTOR_TORIEL);
  EV_BEGIN;
  if (c->self) c->self->kind = OBJ_NONE;
  if (!t) return 1;
  /* she goes ahead; halfway, a Froggit */
  FREE(true);
  WALK(t, 1500, 4470, 3);
  WALK(obj_get(ACTOR_TORIEL), 1820, 4480, 3);
  obj_get(ACTOR_TORIEL)->dir = DIR_LEFT;
  WAIT_UNTIL(pl.x > 1500);
  FREE(false);
  BATTLE(FORM_FROGGIT_FIRST);
  FREE(true);
  WAIT_UNTIL(pl.x > 1740);
  FREE(false);
  t = obj_find(ACTOR_TORIEL);
  t->dir = DIR_LEFT;
  SAY(FACE_TORIEL, "`0* This is the puzzle,\n  but...|* Here, take my hand for a\n  moment.");
  /* hand in hand over the spikes */
  PL_WALK(t->x - 14, t->y + 10, 3);
  for (c->v[0] = 0; c->v[0] < (int)(sizeof spike_path / sizeof spike_path[0]); c->v[0]++) {
    WALK_TO(obj_get(ACTOR_TORIEL), spike_path[c->v[0]][0] + 6, spike_path[c->v[0]][1] - 30, 2);
    pl_walk_to(spike_path[c->v[0]][0] - 16 > pl.x ? spike_path[c->v[0]][0] - 16 : pl.x, spike_path[c->v[0]][1] - 20, 2);
    WAIT_UNTIL(!obj_get(ACTOR_TORIEL)->walking && !pl_walking());
  }
  pl_walk_to(2240, 4444, 2);
  WAIT_UNTIL(!pl_walking());
  obj_get(ACTOR_TORIEL)->dir = DIR_LEFT;
  SAY(FACE_TORIEL, "`0* Puzzles seem too dangerous\n  for now.");
  set_flag(F_RUINS5_DONE, 1);
  LEAVE(obj_find(ACTOR_TORIEL), 3, 2320, 4440);
  EV_END;
}

static int ev_ruins6_toriel(Ev *c) {
  Obj *t = obj_find(ACTOR_TORIEL);
  EV_BEGIN;
  if (c->self) c->self->kind = OBJ_NONE;
  if (!t) return 1;
  t->dir = DIR_LEFT;
  SAY(FACE_TORIEL, "`0* You are doing very well,\n  my child.|`1* Now, a hard request...|`0* Walk to the end of this\n  room on your own.|`1* I am sorry.");
  /* she hurries off, and hides behind the pillar */
  FREE(true);
  WALK(obj_get(ACTOR_TORIEL), 4560, 4466, 6);
  WALK(obj_get(ACTOR_TORIEL), 4606, 4392, 3);
  obj_get(ACTOR_TORIEL)->dir = DIR_DOWN;
  obj_get(ACTOR_TORIEL)->flags &= (uint16_t)~OF_SOLID;
  obj_trigger(4470, 4430, 16, 130, EV_RUINS6_REVEAL, ACTOR_TRIGGER2);
  EV_END;
}

static int ev_ruins6_pillar(Ev *c) {
  Obj *t = obj_find(ACTOR_TORIEL);
  EV_BEGIN;
  if (!t || flag(F_RUINS6_DONE)) {
    NARRATE("* (A pillar.)");
    return 1;
  }
  SAY(FACE_TORIEL, "`0* ...");
  EV_END;
}

static int ev_ruins6_reveal(Ev *c) {
  Obj *t = obj_find(ACTOR_TORIEL);
  EV_BEGIN;
  if (!t) return 1;
  if (c->self) c->self->kind = OBJ_NONE;
  WALK(t, 4570, 4440, 2);
  t->dir = DIR_LEFT;
  SAY(FACE_TORIEL, "`0* There you are!|* I was just behind the\n  pillar.|`2* Thank you for trusting\n  me.|`0* That was a test of your\n  independence.|`1* I have errands to run.\n* Please wait here for me.|`2* Take this CELL PHONE.|`0* Call me if you need\n  anything. Be good!");
  set_flag(F_PHONE, 1);
  LEAVE(obj_find(ACTOR_TORIEL), 3, 4700, 4470, 4860, 4470);
  set_flag(F_RUINS6_DONE, 1);
  /* (the game saves here when she leaves) */
  sv.room = (int16_t)g_room, sv.x = pl.x, sv.y = pl.y;
  save_write_auto();
  EV_END;
}

static int ev_ruins7_call(Ev *c) {
  EV_BEGIN;
  WAIT_UNTIL(!world_fading());
  set_flag(F_LEAFPILE_CALL, 1);
  NARRATE("* Ring...");
  SAY(FACE_TORIEL, "`0* Hello? It is TORIEL.|* You are still in that\n  room, yes?|* The puzzles ahead are\n  tricky. Wait for me.|* Be good!");
  EV_END;
}

static int ev_ruins7_froggit(Ev *c) {
  EV_BEGIN;
  NARRATE("* Ribbit, ribbit.|* (Some advice, human.)|* (ACT the right way, or weaken\n  a monster...)|* (...and it may stop wanting\n  to fight.)|* (Then show it MERCY.)\n* Ribbit.");
  EV_END;
}

int ruins1_event(Ev *c) {
  switch (c->id) {
    case EV_FLOWEY_MEET: return ev_flowey_meet(c);
    case EV_RUINS1_TORIEL: return ev_ruins1_toriel(c);
    case EV_RUINS2_TORIEL: return ev_ruins2_toriel(c);
    case EV_RUINS3_TORIEL: return ev_ruins3_toriel(c);
    case EV_RUINS3_SWITCH: return ev_ruins3_switch(c);
    case EV_RUINS4_TORIEL: return ev_ruins4_toriel(c);
    case EV_RUINS4_DUMMY: return ev_ruins4_dummy(c);
    case EV_RUINS5_TORIEL: return ev_ruins5_toriel(c);
    case EV_RUINS6_TORIEL: return ev_ruins6_toriel(c);
    case EV_RUINS6_PILLAR: return ev_ruins6_pillar(c);
    case EV_RUINS6_REVEAL: return ev_ruins6_reveal(c);
    case EV_RUINS7_CALL: return ev_ruins7_call(c);
    case EV_RUINS7_FROGGIT: return ev_ruins7_froggit(c);
  }
  return -1;
}
