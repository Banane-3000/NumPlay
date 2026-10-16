/* The MTT Resort: the street outside (Sans, and dinner with him in the restaurant), the lobby (its fountain, the
 * MTT-Brand Burger Emporium, the elevator that's out of order, the door north to the CORE: core.c), the hotel's hallway and
 * its room service. (As the Undertale Wiki describes them; what they say is ours, in short.) */
#include <stddef.h>
#include "event.h"
#include "events.h"
#include "game.h"
#include "hotland.h"
#include "items.h"
#include "menu.h"
#include "save.h"
#include "shop.h"
#include "snowdin.h"
#include "world.h"

static void gone(Obj *o) { o->kind = OBJ_NONE; }
static Obj *thing(int x, int y, int w, int h, int event, int name, int arg) {
  Obj *o = obj_trigger(x, y, w, h, event, name);
  if (o) o->flags = OF_TALK | OF_SOLID, o->arg = (int16_t)arg;
  return o;
}
static int slot_of(int item) {
  for (int i = 0; i < NUM_ITEMS; i++)
    if (sv.items[i] == item) return i;
  return -1;
}

void res_setup(int room, int from) {
  (void)from;
  if (room < ROOM_RES_OUTSIDE || room > ROOM_RES_ROOM) return;
  set_flag(F_HT_FLOOR3, 1);
  switch (room) {
    case ROOM_RES_OUTSIDE:
      if (!flag(F_RES_SANS)) {
        Obj *s = obj_sans(1143, 851, DIR_DOWN);
        if (s) s->event = EV_RES_SANS, s->flags |= OF_TALK;
      }
      obj_sign(1260, 880, 18, 18, "* (A note: great deals up the\n  creepy alley on the right!)");
      thing(1382, 628, 36, 52, EV_RES_SHOP, ACTOR_DOOR, 1);   /* Bratty and Catty's, up the alley */
      break;
    case ROOM_RES_LOBBY:
      obj_save(1210, 448, "* (The calm of this hotel fills\n  you with determination.)");
      obj_sign(1180, 382, 80, 54, "* (Royal Memorial Fountain.\n  Built 201X.)\n* (Mettaton added last week.)");
      thing(1262, 226, 76, 54, EV_RES_SHOP, ACTOR_DOOR, 0);   /* the MTT-Brand Burger Emporium */
      obj_sign(1378, 228, 46, 52, "* (The elevator to the capital.)\n* (It's out of order.)");
      thing(1100, 236, 80, 44, EV_RES_DESK, ACTOR_ITEM, 0);
      obj_sign(1252, 496, 20, 24, "* Welcome to the MTT Resort!\n* Mettaton's own hotel!");
      obj_sign(1354, 292, 36, 40, "* (On the phone: they won't be\n  home tonight. The elevator.)");
      obj_sign(1432, 284, 22, 34, "* Out of order?! The ONE\n  elevator to the capital?!");
      obj_sign(1394, 320, 52, 34, "* The elevator's broken, so\n  we're trying to fit in all\n  the guests going up.");
      obj_sign(1290, 510, 22, 22, "* (A box. It's locked tight.)");
      break;
    case ROOM_RES_DINER:
      obj_sign(820, 330, 60, 52, "* Welcome to the restaurant!|* ...you needed a reservation,\n  darling. Sorry.");
      obj_sign(640, 312, 30, 40, "* My son ran away after his\n  mother fell down.|* ...I tell jokes here now.\n* Ice to meet you.");
      obj_sign(622, 386, 18, 22, "* The CORE's made of parts\n  you can swap around.|* So its layout changes when\n  it needs to. Neat, huh?");
      obj_sign(528, 376, 38, 44, "* Puzzles everywhere down here.\n* Ugh.|* They're from when we built\n  them to stop humans.");
      obj_sign(98, 440, 22, 30, "* (They forgot to book a table,\n  so they're licking the ficus\n  to look busy.)");
      break;
    case ROOM_RES_HALL:
      for (int i = 0; i < 4; i++) thing(1652 + i * 75, 330, 36, 52, EV_RES_DOOR, ACTOR_ITEM, i + 1);
      obj_sign(1930, 380, 44, 36, "* (They mop up the slime\n  dripping off them.)\n* (It keeps dripping.)");
      break;
    case ROOM_RES_ROOM:
      obj_sign(1680, 44, 120, 116, "* (A big, soft bed.)");
      break;
  }
}

/* ---------------------------------------------------------------- the events */
static int ev_sans(Ev *c) {
  Obj *s = obj_get(ACTOR_SANS);
  EV_BEGIN;
  if (flag(F_RES_SANS)) {
    SAY(FACE_SANS, "* hey. good luck in there.");
    return 1;
  }
  SAY(FACE_SANS, "* hey. you made it.|* how about dinner?\n* my treat.@Sure@Not now");
  if (CHOICE() == 1) {
    SAY(FACE_SANS, "* ok. i'll be around.");
    return 1;
  }
  /* the restaurant: a table, and what he has to say */
  FADE_TO(ROOM_RES_DINER, 236, 418, DIR_RIGHT);
  s = obj_sans(338, 450, DIR_LEFT);
  SAY(FACE_SANS, "* so. i keep watch in the\n  forest by snowdin, right?|* out there's this big door.\n  locked tight.|* so i'd knock and tell jokes\n  through it. for fun.|* one day, somebody on the\n  other side knocked back.|* an old lady. laughed at all\n  of my jokes.|* we kept at it. day after\n  day.");
  SAY(FACE_SANS, "* then she asked me for a\n  favor.|* if a human ever came out of\n  that door...|* ...would i look after them?\n  keep them safe?|* ...|* i don't like promises.\n  but i made that one.|* and without it...|* well. you'd be dust by now,\n  buddy.");
  SAY(FACE_SANS, "* ...heh. lighten up.\n* thanks for dinner.|* wait. i said i'd pay, huh.\n* ...catch you later.");
  set_flag(F_RES_SANS, 1);
  s = obj_get(ACTOR_SANS);
  WALK(s, s->x, 362, 2);   /* up past the tables, out east */
  s = obj_get(ACTOR_SANS);
  WALK(s, 780, 362, 3);
  gone(obj_get(ACTOR_SANS));
  EV_END;
}

static int ev_shop(Ev *c) {
  EV_BEGIN;
  shop_open(c->self && c->self->arg ? SHOP_BRATTY : SHOP_BURGER);
  WAIT_UNTIL(g_mode == MODE_WORLD);
  EV_END;
}

/* the hotel's front desk: a room for 200G (door 2) */
static int ev_desk(Ev *c) {
  EV_BEGIN;
  if (flag(F_RES_ROOM)) {
    NARRATE(flag(F_RES_ROOM) == 1 ? "* Your room is the second\n  door down the hall." : "* We hope you enjoyed your stay!");
    return 1;
  }
  NARRATE("* Welcome to the MTT Resort!\n* A room is 200G a night.@Stay@Leave");
  if (CHOICE() == 1) return 1;
  if (sv.gold < 200) {
    NARRATE("* ...you don't have enough\n  money. Sorry!");
    return 1;
  }
  sv.gold = (uint16_t)(sv.gold - 200);
  set_flag(F_RES_ROOM, 1);
  NARRATE("* Your room is the second\n  door down the hall. Enjoy!");
  EV_END;
}

/* the hotel's doors: room service for 1 and 3; 2 the room from the desk; a dog behind 4 */
static int ev_door(Ev *c) {
  EV_BEGIN;
  c->v[0] = c->self ? c->self->arg : 1;
  if (c->v[0] == 2) {
    if (flag(F_RES_ROOM) != 1) {
      NARRATE("* (The door is locked.)");
      return 1;
    }
    world_fade_hold(ROOM_RES_ROOM, 1730, 158, DIR_DOWN, 90);
    WAIT_UNTIL(!world_fading());
    sv.hp = sv.maxhp;
    set_flag(F_RES_ROOM, 2);
    NARRATE("* (You slept soundly. Your HP\n  was maxed out.)");
    return 1;
  }
  if (c->v[0] == 4) {
    if (flag(F_RES_DOG) || slot_of(ITEM_HOT_DOG) < 0) {
      NARRATE("* (Something behind the door is\n  panting.)\n* (It sounds like a dog.)");
      return 1;
    }
    NARRATE("* (Something behind the door is\n  panting.)|* (Slide a Hot Dog under it?)@Yes@No");
    if (CHOICE() == 1) return 1;
    item_remove_at(slot_of(ITEM_HOT_DOG));
    item_add(ITEM_HUSH_PUPPY);
    set_flag(F_RES_DOG, 1);
    NARRATE("* (It tugs at the Hot Dog a few\n  times, then gets it.)|* (A Hush Puppy slides back out.)");
    return 1;
  }
  c->v[1] = c->v[0] == 1 ? ITEM_SEA_TEA : ITEM_CINNAMON_BUNNY;
  c->v[2] = c->v[0] == 1 ? F_RES_TEA : F_RES_BUN;
  if (flag(c->v[2])) {
    NARRATE("* (Nobody's ordered anything\n  else.)");
    return 1;
  }
  NARRATE(c->v[0] == 1 ? "* (A note on the door: room\n  service, please. A Sea Tea.)"
                       : "* (A note on the door: room\n  service, please. A Cinnamon\n  Bunny.)");
  if (slot_of(c->v[1]) < 0) return 1;
  NARRATE("* (Slide it under the door?)@Yes@No");
  if (CHOICE() == 1) return 1;
  item_remove_at(slot_of(c->v[1]));
  sv.gold = (uint16_t)min_i(sv.gold + 99, 9999);
  set_flag(c->v[2], 1);
  NARRATE("* (Someone takes it, and slides\n  99G back under the door.)");
  EV_END;
}

int res_event(Ev *c) {
  switch (c->id) {
    case EV_RES_SANS: return ev_sans(c);
    case EV_RES_SHOP: return ev_shop(c);
    case EV_RES_DOOR: return ev_door(c);
    case EV_RES_DESK: return ev_desk(c);
  }
  return -1;
}
