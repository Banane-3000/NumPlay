/* Waterfall, past the bridge: the garbage dump and Mad Dummy, Napstablook, the Quiet Area and the small bird, Blook
 * Acres, Gerson's shop, the twin waterfalls, the mushroom maze and Temmie Village. (As the Undertale Wiki describes
 * them; what they say is ours, in short.) */
#include <stdio.h>
#include "battle.h"
#include "event.h"
#include "events.h"
#include "game.h"
#include "items.h"
#include "monsters.h"
#include "save.h"
#include "shop.h"
#include "waterfall.h"
#include "world.h"

static void gone(Obj *o) { o->kind = OBJ_NONE; }

/* something on the map to talk to (solid: it stands there) */
static Obj *thing(int x, int y, int w, int h, int event, int name, int arg) {
  Obj *o = obj_trigger(x, y, w, h, event, name);
  if (o) o->flags = OF_TALK | OF_SOLID, o->arg = (int16_t)arg;
  return o;
}
static Obj *solid_sign(int x, int y, int w, int h, const char *text) {
  Obj *o = obj_sign(x, y, w, h, text);
  if (o) o->flags |= OF_SOLID;
  return o;
}

static Obj *obj_napstablook(int x, int y) {
  static const uint16_t s[4] = {SPR_NAPSTA_OW, SPR_NAPSTA_OW, SPR_NAPSTA_OW, SPR_NAPSTA_OW};
  Obj *o = obj_npc(ACTOR_NAPSTA, s, x, y, DIR_DOWN, 0);
  if (o) o->alpha = 200;
  return o;
}

/* the small bird over the gap of bright water (F_WF_BIRD: 1, it waits on the west side) */
void wf_bird_setup(int room) {
  bool west = flag(F_WF_BIRD) == 1;
  if (room == ROOM_WF_MARSH_TOP) {
    if (west) {
      obj_deco(SPR_BIRD, 8466, 578, false);
      thing(8462, 574, 22, 26, EV_WF_BIRD, ACTOR_BIRD, 2);
    } else {
      obj_deco(SPR_BIRD, 8544, 578, false);   /* (across the gap: out of reach) */
    }
  } else if (room == ROOM_WF_BIRD2 && !west) {
    obj_deco(SPR_BIRD, 18523, 2341, false);
    thing(18519, 2337, 24, 26, EV_WF_BIRD, ACTOR_BIRD, 1);
  }
}

void wf2_setup(int room, int from) {
  (void)from;
  if (room < ROOM_WF_DUMP || room > ROOM_WF_TEMMIE) return;
  switch (room) {
    case ROOM_WF_DUMP:
      if (!flag(F_WF_DUMP)) {
        Obj *t = obj_trigger(17660, 3220, 80, 50, EV_DUMP_WAKE, ACTOR_TRIGGER);
        if (t) t->flags = OF_TOUCH;
      }
      solid_sign(18030, 3292, 80, 44, "* (A heap of garbage.)\n* (A rusty bike sticks out of it.)");
      solid_sign(18120, 3292, 80, 44, "* (A heap of garbage.)\n* (There's an old computer in there.)");
      obj_sign(17880, 3250, 24, 20, "* (Water flows in from somewhere far above.)");
      break;
    case ROOM_WF_TRASH:
      obj_save(18540, 3234, "* (Bits of garbage float past and drop into the dark.)\n* (You're filled with determination.)");
      solid_sign(18560, 3170, 40, 50, "* (A pile of trash.)\n* (Someone left a box of old tapes.)");
      solid_sign(18690, 3170, 40, 50, "* (A pile of trash.)\n* (A DVD case. Something with claws scratched it.)");
      break;
    case ROOM_WF_WALKWAY:
      if (flag(F_WF_COOLER) < 2) thing(18922, 2816, 36, 22, EV_COOLER, ACTOR_ITEM, 0);
      else solid_sign(18922, 2816, 36, 22, "* (An empty cooler.)");
      solid_sign(18916, 2736, 60, 60, "* (A heap of garbage.)\n* (It smells like old rain.)");
      solid_sign(18824, 2836, 46, 58, "* (A heap of garbage.)\n* (A deflated ball is stuck on top.)");
      if (!flag(F_MAD_DUMMY)) {
        static const uint16_t s[4] = {SPR_MADDUMMY_OW, SPR_MADDUMMY_OW, SPR_MADDUMMY_OW, SPR_MADDUMMY_OW};
        obj_npc(ACTOR_MADDUMMY, s, 18929, 2730, DIR_DOWN, EV_MAD_DUMMY);
      }
      break;
    case ROOM_WF_QUIET:
      obj_save(18900, 2340, "* (The quiet of this place fills you with determination.)");
      obj_sign(18966, 2256, 22, 24, "* (West: the bird.\n  North: the houses.\n  East: the shop.)");
      break;
    case ROOM_WF_BIRD2:
      wf_bird_setup(room);
      break;
    case ROOM_WF_ACRES:
      thing(18568, 2060, 32, 44, EV_LOCKED_HOUSE, ACTOR_DOOR, 0);
      thing(18926, 2080, 26, 30, EV_LOCKED_HOUSE, ACTOR_DOOR, 1);
      obj_sign(18450, 2124, 64, 34, "* (A doormat shaped like a fish.)");
      obj_sign(19200, 2020, 30, 30, "* (The snail farm.\n* Thundersnail races: ask the ghost.)");
      if (flag(F_MAD_DUMMY)) {
        Obj *n = obj_napstablook(19316, 2092);
        if (n) n->event = EV_SNAILS, n->flags |= OF_TALK, obj_set_box(n, 0, 20, 26, 36);   /* (over the fence) */
      }
      break;
    case ROOM_WF_NAPSTA_IN:
      solid_sign(18820, 1790, 30, 36, "* (A fridge.)\n* (There's a ghost sandwich inside. Your hand goes right through it.)");
      solid_sign(18866, 1800, 32, 30, "* (An old TV, showing a ghost show.)");
      solid_sign(18930, 1786, 36, 30, "* (A computer with a song playing on it.)\n* (It sounds spooky... and kind of nice.)");
      if (flag(F_NAPSTA_HOME)) {
        Obj *n = obj_napstablook(18969, 1810);
        if (n) n->event = EV_NAPSTA_ROOM, n->flags |= OF_TALK;
      }
      break;
    case ROOM_WF_GERSON:
      thing(19356, 2256, 44, 50, EV_GERSON, ACTOR_DOOR, 0);   /* (the shop's doorway) */
      thing(19384, 2286, 26, 20, EV_BOX, ACTOR_ITEM, 0);
      obj_sign(19690, 2288, 30, 20, "* (Gerson's shop, behind you.\n* Waterfall goes on, ahead.)");
      break;
    case ROOM_WF_TWIN:
      obj_sign(19860, 2290, 30, 20, "* (The water here is shallow. You wade on.)");
      break;
    case ROOM_WF_TEMMIE:
      set_flag(F_TEM_VILLAGE, 1);
      obj_save(21325, 3120, "* (The Temmies' cheer fills you with determination.)");
      thing(21360, 3118, 26, 24, EV_BOX, ACTOR_ITEM, 0);
      thing(21334, 3020, 48, 56, EV_TEM_SHOP, ACTOR_DOOR, 0);
      obj_sign(21176, 3002, 78, 34, "* (A painting of a Temmie, very proud of itself.)");
      obj_sign(21122, 3062, 22, 26, "* (Welcome to Temmie Village!)");
      obj_sign(21316, 3056, 24, 26, "* (Tem Shop! Tem sells!)");
      for (int i = 0; i < 4; i++) {
        static const int16_t tx[4] = {21168, 21202, 21234, 21268}, ty[4] = {3072, 3080, 3068, 3074};
        thing(tx[i], ty[i], 24, 30, EV_TEMMIE, ACTOR_TEMMIE, i);
      }
      break;
  }
}

/* ---------------------------------------------------------------- the events */
static int ev_dump_wake(Ev *c) {
  EV_BEGIN;
  /* (a dream while Frisk lies there) */
  NARRATE("* (...)|* (In the dark, a voice keeps calling someone's name.)\n* (It isn't yours.)");
  WAIT(30);
  NARRATE("* (You wake up on a bed of golden flowers.)\n* (Garbage floats all around.)");
  set_flag(F_WF_DUMP, 1);
  if (c->self) gone(c->self);
  EV_END;
}

static int ev_cooler(Ev *c) {
  EV_BEGIN;
  NARRATE("* (A cooler, with Astronaut\n  Food inside. Take some?)@Yes@No");
  if (CHOICE() == 0) {
    if (!item_add(ITEM_ASTRONAUT_FOOD)) {
      NARRATE("* (You're carrying too much.)");
    } else {
      set_flag(F_WF_COOLER, flag(F_WF_COOLER) + 1);
      NARRATE("* (You got the Astronaut Food.)");
      if (flag(F_WF_COOLER) >= 2 && c->self) gone(c->self);
    }
  }
  EV_END;
}

static int ev_mad_dummy(Ev *c) {
  Obj *d = obj_get(ACTOR_MADDUMMY), *n = obj_get(ACTOR_NAPSTA);
  EV_BEGIN;
  NARRATE("* (It's a training dummy.)\n* (Hit it?)@Yes@No");
  g_shake = 6;
  SAY(FACE_NONE, "* Hey! HEY! You!|* You're the one who picked on my cousin back in the Ruins!|* Ghosts like us live in dummies, you know! And you... you...|* GRR! Dummies, get them!!");
  BATTLE(FORM_MAD_DUMMY);
  /* Napstablook drifts in; their tears soak Mad Dummy, who storms off */
  n = obj_napstablook(d->x - 50, d->y);
  NARRATE("* (Napstablook drifts in, crying softly.)|* (Their tears rain down on Mad Dummy.)");
  SAY(FACE_NONE, "* Ugh! Gross! My dummy's all wet!|* This isn't over! I'll be back!!");
  d = obj_get(ACTOR_MADDUMMY);
  WALK(d, d->x, d->y - 120, 3);
  gone(obj_get(ACTOR_MADDUMMY));
  set_flag(F_MAD_DUMMY, 1);
  SAY(FACE_NONE, "* oh... sorry... i didn't mean to get in the way...|* um... i live just up north, past the pond.|* ...if you want, you could come over.\n* only if you want to...");
  set_flag(F_NAPSTA_HOME, 1);
  for (c->v[0] = 200; c->v[0] > 0; c->v[0] -= 10) {
    n = obj_get(ACTOR_NAPSTA);
    n->alpha = (uint8_t)c->v[0];
    WAIT(2);
  }
  gone(obj_get(ACTOR_NAPSTA));
  EV_END;
}

static int ev_napsta_room(Ev *c) {
  EV_BEGIN;
  if (flag(F_NAPSTA_HOME) >= 2) {
    SAY(FACE_NONE, "* oh... hi again.\n* it's nice to have company... i think.");
    return 1;
  }
  SAY(FACE_NONE, "* oh... you really came.|* uh... this is my room. it's not much.|* sometimes i just lie on\n  the floor for a while...|* want to do that with me?@Sure@No thanks");
  if (CHOICE() == 0) {
    NARRATE("* (You lie down on the floor next to Napstablook.)");
    WAIT(60);
    NARRATE("* (Time passes.)\n* (It's oddly peaceful.)");
    SAY(FACE_NONE, "* ...thanks. that was\n  really nice.|* i don't get to do that\n  with anyone.");
    set_flag(F_NAPSTA_HOME, 2);
  } else {
    SAY(FACE_NONE, "* oh... that's okay...");
  }
  EV_END;
}

static int ev_locked_house(Ev *c) {
  EV_BEGIN;
  if (c->self && c->self->arg == 0) NARRATE("* (The door is locked.)\n* (Someone inside is clanking on something metal.)");
  else NARRATE("* (The door is locked tight.)\n* (It has a big star on it.)");
  EV_END;
}

/* Thundersnail: 10G a race; cheering doesn't help much */
static int ev_snails(Ev *c) {
  EV_BEGIN;
  SAY(FACE_NONE, "* oh... hi. this is our snail farm.|* want to race the snails? it's 10G...@Race@No");
  if (CHOICE() == 1) return 1;
  if (sv.gold < 10) {
    SAY(FACE_NONE, "* oh... you don't have enough money... that's okay.");
    return 1;
  }
  sv.gold = (uint16_t)(sv.gold - 10);
  set_flag(F_SNAIL_RACE, flag(F_SNAIL_RACE) + 1);
  NARRATE("* (The snails are off!)|* (Your yellow snail creeps forward...)");
  WAIT(40);
  if (rnd_n(2)) {
    sv.gold = (uint16_t)(sv.gold + 9);
    SAY(FACE_NONE, "* oh... your snail won...\n  here's 9G.|* ...sorry, that's all the\n  prize money.");
  } else {
    SAY(FACE_NONE, "* oh... it lost. that's how it goes sometimes.");
  }
  EV_END;
}

static int ev_shop_door(Ev *c) {
  EV_BEGIN;
  shop_open(c->id == EV_GERSON ? SHOP_GERSON : SHOP_TEM);
  WAIT_UNTIL(g_mode == MODE_WORLD);
  EV_END;
}

static int ev_temmie(Ev *c) {
  static const char *const lines[4] = {
    "* hOI!!! im temmie!!|* and dis is my friend... temmie!!",
    "* hOI!! im temmie!!\n* don forget my friend!!",
    "* hOI!!!\n* yaya!!! human!!! so cute!!",
    "* hOI... tem is allergic to temmies...|* but tem still luvs dem!!"};
  EV_BEGIN;
  NARRATE(lines[(c->self ? c->self->arg : 0) & 3]);
  EV_END;
}

int wf2_event(Ev *c) {
  switch (c->id) {
    case EV_DUMP_WAKE: return ev_dump_wake(c);
    case EV_COOLER: return ev_cooler(c);
    case EV_MAD_DUMMY: return ev_mad_dummy(c);
    case EV_NAPSTA_ROOM: return ev_napsta_room(c);
    case EV_LOCKED_HOUSE: return ev_locked_house(c);
    case EV_SNAILS: return ev_snails(c);
    case EV_GERSON: case EV_TEM_SHOP: return ev_shop_door(c);
    case EV_TEMMIE: return ev_temmie(c);
  }
  return -1;
}
