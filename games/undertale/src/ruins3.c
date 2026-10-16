/* The end of the Ruins: Toriel's home, the basement where she goes to destroy the exit, her fight, and Flowey
 * waiting past the door. Texts are the game's (the Undertale Wiki's records of them) where known. */
#include <stdio.h>
#include <string.h>
#include "battle.h"
#include "event.h"
#include "events.h"
#include "game.h"
#include "items.h"
#include "menu.h"
#include "monsters.h"
#include "ruins.h"
#include "save.h"
#include "world.h"

/* ---------------------------------------------------------------- the rooms' things */
void ruins3_setup(int room, int from) {
  (void)from;
  switch (room) {
    case ROOM_TORHOUSE1:
      obj_sign(8880, 2740, 24, 22, "* (Inside is an old calendar\n  from the beginning of 201X.)");
      obj_sign(9100, 2728, 44, 24, "* (It's a bookshelf.)\n* (The books are worn out.)");
      if (flag(F_TORIEL_HOME) && !flag(F_HOME_TOUR)) {
        obj_toriel(8996, 2800, DIR_DOWN);
        event_start(EV_HOME_TOUR, NULL);
      }
      if (!flag(F_TORIEL_BASEMENT)) obj_trigger(9078, 2734, 12, 28, EV_STAIRS, ACTOR_TRIGGER);
      break;
    case ROOM_TORHOUSE2: {
      bool reading = flag(F_HOME_TOUR) && !flag(F_TORIEL_BASEMENT);
      Obj *ch = obj_deco(reading ? SPR_O_CHAIRIEL : SPR_O_CHAIREMPTY, 8640, 2723, true);
      if (ch) ch->name = ACTOR_CHAIR, ch->flags |= OF_TALK, ch->event = EV_TORIEL_CHAIR, obj_set_box(ch, 10, 36, 44, 24);
      obj_sign(8673, 2690, 68, 66, flag(F_TORIEL_FIGHT) == 2 ? "* (The fire has gone out.)"
                                                             : "* (The fire isn't burning\n  hot...)\n* (Just pleasantly warm.)|* (You could put your hand\n  inside.)");
      obj_sign(8753, 2692, 60, 60, "* (It's a history book.)|* \"Trapped behind the barrier\n  and fearful of further human\n  attacks, we retreated.\"|* \"Far, far into the earth we\n  walked, until we reached the\n  cavern's end.\"|* \"This was our new home, which\n  we named...\"|* \"Home.\"|* \"As great as our king is, he\n  is pretty lousy at names.\"");
      break;
    }
    case ROOM_TORKITCHEN:
      obj_sign(8560, 2540, 38, 62, "* (For some reason, there is a\n  brand-name chocolate bar in\n  the fridge.)");
      obj_sign(8603, 2560, 22, 40, "* (There's a clump of white\n  fur stuck in the drain.)");
      obj_sign(8630, 2580, 22, 22, "* (Inside the cupboard are\n  cookie cutters for\n  gingerbread monsters.)");
      obj_sign(8654, 2560, 30, 40, flag(F_BED_SLEPT) ? "* (It's a butterscotch-\n  cinnamon pie.)\n* (Its size intimidates you.)"
                                                     : "* (It's a butterscotch-\n  cinnamon pie.)|* (It smells nice, but it's\n  too hot to eat.)");
      obj_sign(8687, 2560, 32, 40, flag(F_TORIEL_FIGHT) == 2 ? "* (No one will use this\n  anymore...)"
                                                             : "* (The stovetop is very\n  clean.)\n* (Toriel must use fire magic\n  instead.)");
      break;
    case ROOM_TORHOUSE3:
      obj_sign(9404, 2750, 24, 54, "* (You have seen this type of\n  plant before but do not know\n  its name.)");
      obj_sign(9492, 2750, 24, 54, "* (You have seen this type of\n  plant before but do not know\n  its name.)");
      obj_sign(9746, 2775, 30, 47, "* (Room under renovations.)");
      obj_sign(9810, 2770, 60, 52, "* (It's you!)");
      if (flag(F_TORIEL_HOME) && !flag(F_HOME_TOUR)) obj_toriel(9370, 2780, DIR_DOWN);
      break;
    case ROOM_ASRIELROOM:
      {
        Obj *b = obj_sign(9368, 2522, 52, 70, NULL);
        if (b) b->event = EV_BED;
      }
      obj_sign(9260, 2498, 62, 34, "* (Toys you aren't interested\n  in.)|* (An empty photo frame.)");
      obj_sign(9335, 2498, 32, 48, "* (A box of shoes in many\n  different sizes.)");
      if (flag(F_BED_SLEPT) == 1) {
        Obj *p = obj_deco(SPR_O_PIESLICE, 9322, 2572, false);
        if (p) p->flags |= OF_TALK | OF_BACK, p->event = EV_ITEM, p->arg = flag(F_HARDMODE) ? ITEM_SNAIL_PIE : ITEM_BUTTERSCOTCH_PIE,
                   p->arg2 = F_PIE;
      }
      break;
    case ROOM_TORIELROOM:
      obj_sign(9540, 2520, 74, 92, "* (It's bigger than a\n  twin-sized bed.)");
      obj_sign(9592, 2598, 26, 30, "* (Toriel's small chair.)\n* (Its name is Chairiel.)");
      obj_sign(9690, 2528, 62, 36, "* (It's Toriel's diary.)|* (You read the passage...)|* \"Why did the skeleton want a\n  friend?\"|* \"Because she was feeling\n  BONELY...\"");
      obj_sign(9632, 2478, 58, 54, "* (You peek inside...)\n* (Scandalous!)|* (It's TORIEL's sock drawer.)");
      break;
    case ROOM_BASEMENT1:
      if (flag(F_TORIEL_BASEMENT) && !flag(F_TORIEL_FIGHT)) {
        static const int16_t tx[2] = {9030, 9560};
        int k = flag(F_BASEMENT_TALK);
        if (k < 2) {
          obj_toriel(tx[k], 2300, DIR_RIGHT);
          obj_trigger(tx[k] - 70, 2300, 16, 80, EV_BASEMENT_TALK, ACTOR_TRIGGER);
        }
      }
      break;
    case ROOM_BASEMENT2:
      if (flag(F_TORIEL_BASEMENT) && !flag(F_TORIEL_FIGHT)) {
        if (flag(F_BASEMENT_TALK) < 3) {
          obj_toriel(10004, 2110, DIR_UP);
          obj_trigger(9960, 2170, 140, 14, EV_BASEMENT_TALK, ACTOR_TRIGGER);
        } else {
          obj_toriel(10004, 1984, DIR_DOWN);
          obj_trigger(9960, 2060, 140, 14, EV_TORIEL_DOOR, ACTOR_TRIGGER2);
        }
      }
      break;
    case ROOM_RUINSEXIT:
      if (!flag(F_FLOWEY_END)) {
        static const uint16_t s[4] = {SPR_FLOWEY};
        Obj *f = obj_npc(ACTOR_FLOWEY, s, 10008, 205, DIR_DOWN, 0);
        if (f) f->flags = OF_HIDDEN, obj_set_box(f, 4, 16, 17, 8);
        obj_trigger(9980, 250, 80, 20, EV_FLOWEY_END, ACTOR_TRIGGER);
      }
      obj_trigger(9990, 70, 60, 14, EV_RUINS_DOOR, ACTOR_TRIGGER2);
      break;
  }
}

/* ---------------------------------------------------------------- the events */
static int ev_home_tour(Ev *c) {
  EV_BEGIN;
  WAIT_UNTIL(!world_fading());
  SAY(FACE_TORIEL, "`0* Smell that?|`2* Surprise! A butterscotch-\n  cinnamon pie, for you!|`0* No snail pie tonight.|`2* And one more surprise...");
  WALK(obj_get(ACTOR_TORIEL), 8996, 2840, 3);
  WALK(obj_get(ACTOR_TORIEL), 9200, 2840, 3);
  obj_get(ACTOR_TORIEL)->kind = OBJ_NONE;
  PL_WALK(pl.x + 40, pl.y, 3);
  FADE_TO(ROOM_TORHOUSE3, 9196, 2836, DIR_RIGHT);
  PL_WALK(9330, 2836, 3);
  pl.dir = DIR_UP;
  SAY(FACE_TORIEL, "`2* A room of your own.|* I hope you like it!");
  WAIT(20);
  SAY(FACE_TORIEL, "`7* ...!|`3* Is something burning...?|`0* Um, make yourself at home!");
  WALK(obj_get(ACTOR_TORIEL), 9370, 2840, 4);
  WALK(obj_get(ACTOR_TORIEL), 9180, 2840, 5);
  obj_get(ACTOR_TORIEL)->kind = OBJ_NONE;
  set_flag(F_HOME_TOUR, 1);
  EV_END;
}

static const char *const chair_opts[2] = {"When can I go home?", "How to exit the RUINS"};

static int ev_toriel_chair(Ev *c) {
  EV_BEGIN;
  if (!flag(F_HOME_TOUR) || flag(F_TORIEL_BASEMENT)) {
    NARRATE("* (Seems like the right size\n  for Toriel.)");
    return 1;
  }
  SAY(FACE_TORIEL, "`0* What is it?");
  CHOOSE(chair_opts, 2);
  if (CHOSEN() < 0) return 1;
  if (CHOSEN() == 0) {
    SAY(FACE_TORIEL, "`3* What?|`1* This is your home now.|`0* Would you rather hear about\n  my book on snails?@Yes@No");
    if (CHOICE() == 0) SAY(FACE_TORIEL, "`2* A snail fact!|`0* Snails have a tongue like a\n  tiny saw.");
    else SAY(FACE_TORIEL, "`0* Oh...^2 Alright.");
    return 1;
  }
  SAY(FACE_TORIEL, "`1* ...|* Um...^2 I have to do\n  something.|* Stay here.");
  set_flag(F_TORIEL_BASEMENT, 1);
  if (c->self) c->self->spr[0] = SPR_O_CHAIRIELGETUP;
  for (c->v[0] = 0; c->v[0] < 6; c->v[0]++) {
    obj_get(ACTOR_CHAIR)->frame = (uint8_t)c->v[0];
    WAIT(4);
  }
  obj_get(ACTOR_CHAIR)->frame = 0;
  if (c->self) c->self->spr[0] = SPR_O_CHAIREMPTY;
  {
    Obj *t = obj_toriel(8660, 2760, DIR_RIGHT);
    if (t) t->flags &= (uint16_t)~OF_SOLID;
  }
  WALK(obj_get(ACTOR_TORIEL), 8800, 2780, 4);
  WALK(obj_get(ACTOR_TORIEL), 8860, 2780, 4);
  obj_get(ACTOR_TORIEL)->kind = OBJ_NONE;
  EV_END;
}

static int ev_stairs(Ev *c) {
  EV_BEGIN;
  if (flag(F_TORIEL_BASEMENT)) {
    if (c->self) c->self->kind = OBJ_NONE;
    return 1;
  }
  if (!flag(F_HOME_TOUR)) return 1;
  SAY(FACE_TORIEL, "`1* My child...|`0* There is nothing for you\n  down there.|* Let us go back upstairs.");
  PL_WALK(pl.x + 14, pl.y + 14, 2);
  EV_END;
}

static int ev_bed(Ev *c) {
  EV_BEGIN;
  NARRATE("* (You lie down on the bed.)");
  if (!flag(F_BED_SLEPT) && !flag(F_TORIEL_BASEMENT)) set_flag(F_BED_SLEPT, 1);
  sv.hp = sv.maxhp;
  world_fade_hold(g_room, 9376, 2590, DIR_DOWN, 90);
  WAIT_UNTIL(!world_fading());
  EV_END;
}

static int ev_basement_talk(Ev *c) {
  EV_BEGIN;
  if (c->self) c->self->kind = OBJ_NONE;
  obj_get(ACTOR_TORIEL)->dir = DIR_LEFT;
  switch (flag(F_BASEMENT_TALK)) {
    case 0:
      SAY(FACE_TORIEL, "`0* You want to go \"home\".|* The exit from the RUINS is\n  just ahead.|`6* I will seal it, so no one\n  can leave.|`0* Go upstairs, please.");
      break;
    case 1:
      SAY(FACE_TORIEL, "`6* Every human who falls here\n  leaves... and is never\n  seen again.|* Out there, #RASGORE#X will\n  take your life.|* I am keeping you safe.|* ...Go to your room.");
      break;
    default:
      obj_get(ACTOR_TORIEL)->dir = DIR_DOWN;
      SAY(FACE_TORIEL, "`6* Do not try to stop me.|* This is your final warning.");
      break;
  }
  set_flag(F_BASEMENT_TALK, flag(F_BASEMENT_TALK) + 1);
  if (g_room == ROOM_BASEMENT1 && flag(F_BASEMENT_TALK) == 1) {
    /* on ahead, to wait further down */
    obj_get(ACTOR_TORIEL)->dir = DIR_RIGHT;
    WALK(obj_get(ACTOR_TORIEL), 9560, 2300, 4);
    obj_get(ACTOR_TORIEL)->dir = DIR_RIGHT;
    obj_trigger(9490, 2300, 16, 80, EV_BASEMENT_TALK, ACTOR_TRIGGER);
    return 1;
  }
  if (g_room == ROOM_BASEMENT1) {
    WALK(obj_get(ACTOR_TORIEL), 10120, 2300, 5);
  } else {
    WALK(obj_get(ACTOR_TORIEL), 10004, 1984, 4);
    obj_get(ACTOR_TORIEL)->dir = DIR_DOWN;
    obj_trigger(9960, 2060, 140, 14, EV_TORIEL_DOOR, ACTOR_TRIGGER2);
    return 1;
  }
  obj_get(ACTOR_TORIEL)->kind = OBJ_NONE;
  EV_END;
}

static int ev_toriel_door(Ev *c) {
  EV_BEGIN;
  if (c->self) c->self->kind = OBJ_NONE;
  SAY(FACE_TORIEL, "`6* So you insist on leaving.|* Like all the others.|* Then show me you are\n  strong enough to survive.");
  BATTLE(FORM_TORIEL);
  if (flag(F_TORIEL_FIGHT) != 1) {
    obj_get(ACTOR_TORIEL)->kind = OBJ_NONE;
    if (!flag(F_TORIEL_FIGHT)) set_flag(F_TORIEL_FIGHT, battle_result() == BR_FLED ? 1 : 2);
    return 1;
  }
  SAY(FACE_TORIEL, "`1* ...|* I see.|* You would not be happy\n  here.|* This small place is no\n  home for you to grow up in.|`0* My worries can wait.\n* For you, my child.");
  /* she holds Frisk */
  PL_WALK(10010, 2040, 2);
  pl.hidden = true;
  obj_get(ACTOR_TORIEL)->spr[0] = SPR_O_TORIEL_HUG, obj_get(ACTOR_TORIEL)->dir = DIR_DOWN;
  obj_get(ACTOR_TORIEL)->y = 2000;
  WAIT(70);
  pl.hidden = false;
  obj_get(ACTOR_TORIEL)->spr[0] = SPR_TORIEL_D, obj_get(ACTOR_TORIEL)->y = 1984;
  SAY(FACE_TORIEL, "`1* If you must leave...|* I will not stop you.|* But please do not come\n  back.|`0* I hope you understand.");
  WAIT(20);
  SAY(FACE_TORIEL, "`0* Goodbye, my child.");
  obj_get(ACTOR_TORIEL)->flags &= (uint16_t)~OF_SOLID;
  WALK(obj_get(ACTOR_TORIEL), 10040, 2050, 2);
  WALK(obj_get(ACTOR_TORIEL), 10040, 2240, 2);
  obj_get(ACTOR_TORIEL)->kind = OBJ_NONE;
  EV_END;
}

static const char flowey_spared[] =
  "`1* Well, well. Aren't you\n  clever.|"
  "`0* Down here it's kill or be\n  killed... and you spared\n  her anyway.|`2* Proud of yourself? Hee hee.|"
  "`1* But what happens when you\n  meet someone who won't stop?|"
  "`3* Over and over, until you're\n  sick of it.|* Then what? Will you fight\n  back... or let ME take over?|";
static const char flowey_killed[] =
  "`1* Well, well. Aren't you\n  clever.|"
  "`0* Kill or be killed, right?\n`2* And you chose. Hee hee.|"
  "`3* You learn fast.|";
static const char flowey_others[] =
  "`1* Well, well. Aren't you\n  clever.|"
  "`0* You spared her...|"
  "`2* ...after everyone you hurt\n  on the way here. Hee hee.|";
static const char flowey_last[] =
  "`8* This world's future is\n  mine to decide.|`1* Go on, then.|"
  "* I won't get in your way.|`2* Have fun...|`8* Ha ha ha...";

static int ev_flowey_end(Ev *c) {
  EV_BEGIN;
  if (c->self) c->self->kind = OBJ_NONE;
  {
    Obj *f = obj_get(ACTOR_FLOWEY);
    f->flags &= (uint16_t)~OF_HIDDEN, f->spr[0] = SPR_O_FLOWEYGROW, f->frame = 0;
  }
  for (c->v[0] = 0; c->v[0] < 9; c->v[0]++) {
    obj_get(ACTOR_FLOWEY)->frame = (uint8_t)c->v[0];
    WAIT(3);
  }
  obj_get(ACTOR_FLOWEY)->spr[0] = SPR_FLOWEY, obj_get(ACTOR_FLOWEY)->frame = 0;
  if (flag(F_TORIEL_FIGHT) == 2) SAY(FACE_FLOWEY, flowey_killed);
  else if (sv.kills) SAY(FACE_FLOWEY, flowey_others);
  else SAY(FACE_FLOWEY, flowey_spared);
  SAY(FACE_FLOWEY, flowey_last);
  obj_get(ACTOR_FLOWEY)->spr[0] = SPR_O_FLOWEYSINK;
  for (c->v[0] = 0; c->v[0] < 6; c->v[0]++) {
    obj_get(ACTOR_FLOWEY)->frame = (uint8_t)c->v[0];
    WAIT(3);
  }
  obj_get(ACTOR_FLOWEY)->kind = OBJ_NONE;
  set_flag(F_FLOWEY_END, 1);
  EV_END;
}

/* the door out of the Ruins: the rest of the underground is still to come */
static int ev_ruins_door(Ev *c) {
  EV_BEGIN;
  /* out of the Ruins: Snowdin's snow */
  set_flag(F_RUINS_EXIT, 1);
  world_fade_hold(ROOM_SNOWDIN1, 34, 392, DIR_DOWN, 20);
  WAIT_UNTIL(!world_fading());
  EV_END;
}

int ruins3_event(Ev *c) {
  switch (c->id) {
    case EV_HOME_TOUR: return ev_home_tour(c);
    case EV_TORIEL_CHAIR: return ev_toriel_chair(c);
    case EV_STAIRS: return ev_stairs(c);
    case EV_BED: return ev_bed(c);
    case EV_BASEMENT_TALK: return ev_basement_talk(c);
    case EV_TORIEL_DOOR: return ev_toriel_door(c);
    case EV_FLOWEY_END: return ev_flowey_end(c);
    case EV_RUINS_DOOR: return ev_ruins_door(c);
  }
  return -1;
}
