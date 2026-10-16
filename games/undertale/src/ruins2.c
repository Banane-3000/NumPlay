/* The Ruins, from the Monster Candy to Toriel's tree: the cracked floors, the rocks, Napstablook, the Spider
 * Bake Sale, the Froggits' hall, the one switch, the rotating rooms, the balcony over Home. Toriel calls on
 * the way. Texts are the game's (the Undertale Wiki's records of them) where known. */
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

/* ---------------------------------------------------------------- falling through a cracked floor */
/* where each fall lands: the room under, Frisk's top-left there (x -1: where he was; y -1: 400 under) */
typedef struct {
  uint8_t room;
  int16_t x, y;
} Fall;
enum { FALL_RUINS8, FALL_LEAVES, FALL_ONE_A, FALL_ROTATE = FALL_ONE_A + 6 };
static const Fall falls[] = {
  [FALL_RUINS8] = {ROOM_RUINS8B, 5258, 5132},
  [FALL_LEAVES] = {ROOM_RUINS10B, -1, -1},
  [FALL_ONE_A] = {ROOM_RUINS16A, 8340, 4945}, {ROOM_RUINS16B, 8460, 4945}, {ROOM_RUINS16C, 8580, 4945},
  {ROOM_RUINS16D, 8340, 5065}, {ROOM_RUINS16E, 8460, 5065}, {ROOM_RUINS16F, 8580, 5065},
  [FALL_ROTATE] = {ROOM_RUINS17X, 9270, 4570},
};

static Obj *crack(int x, int y, int w, int h, int fall) {
  Obj *o = obj_trigger(x, y, w, h, EV_FALLING, ACTOR_NONE);
  if (o) o->arg = (int16_t)fall;
  return o;
}

static int ev_fall(Ev *c) {
  EV_BEGIN;
  {
    const Fall *f = &falls[c->self ? c->self->arg : c->v[4]];
    c->v[0] = f->room;
    c->v[1] = f->x < 0 ? pl.x : f->x;
    c->v[2] = f->y < 0 ? pl.y + 400 : f->y;
  }
  if (g_room == ROOM_RUINS10) set_flag(F_FALL_LEAVES, flag(F_FALL_LEAVES) + 1);
  pl.hidden = true;
  WAIT(6);
  FADE_TO(c->v[0], c->v[1], c->v[2] - 60, DIR_DOWN);
  pl.hidden = false;
  /* down onto the leaves */
  for (c->v[3] = 0; c->v[3] < 12; c->v[3]++) {
    pl.y = (int16_t)(pl.y + 5);
    world_camera();
    WAIT(0);
  }
  WAIT(10);
  EV_END;
}

/* the leaf puzzle: the tiles that give way (under them, the leaves) */
static const int16_t leaf_holes[][4] = {
  {6160, 4740, 80, 160}, {6280, 4780, 20, 100}, {6340, 4840, 20, 60}, {6400, 4780, 60, 40}, {6400, 4820, 40, 60}};
/* the one switch puzzle: its six cracks */
static const int16_t one_cracks[6][2] = {{8342, 4582}, {8462, 4582}, {8582, 4582}, {8342, 4702}, {8462, 4702}, {8582, 4702}};

/* ---------------------------------------------------------------- the rotating rooms */
/* each room's three switches: x, y, colour (0 red, 1 blue, 2 green), right? A hidden one is under a pillar
 * (its box the pillar's) */
typedef struct {
  int16_t x, y, w, h;
  uint8_t color, right, hidden;
} CSwitch;
static const CSwitch cswitch[4][3] = {
  {{9020, 4780, 20, 20, 0, 0, 0}, {8800, 4680, 20, 20, 1, 0, 0}, {8970, 4620, 20, 20, 2, 0, 0}},
  {{8830, 4397, 20, 20, 0, 0, 0}, {8916, 4156, 48, 78, 1, 1, 1}, {8990, 4320, 20, 20, 2, 0, 0}},
  {{8400, 4200, 20, 20, 0, 1, 0}, {8620, 4300, 20, 20, 1, 0, 0}, {8460, 4360, 20, 20, 2, 0, 0}},
  {{8576, 3676, 48, 78, 0, 0, 1}, {8490, 3920, 20, 20, 1, 0, 0}, {8416, 3716, 48, 78, 2, 1, 1}},
};
static int rotate_fails;

static int rotate_index(int room) {
  switch (room) {
    case ROOM_RUINS17A: return 0;
    case ROOM_RUINS17B: return 1;
    case ROOM_RUINS17C: return 2;
    case ROOM_RUINS17D: return 3;
    default: return -1;
  }
}

static void rotate_setup(int k) {
  for (int i = 0; i < 3; i++) {
    const CSwitch *s = &cswitch[k][i];
    Obj *o = obj_sign(s->x, s->y, s->w, s->h, NULL);
    if (o) o->event = EV_COLOR_SWITCH, o->arg = (int16_t)(k * 3 + i), o->name = (uint8_t)(ACTOR_SWITCH + i);
    if (o && !s->hidden) obj_set_box(o, 2, 6, 16, 14);
  }
  static const char *const plaque[4] = {
    "* \"The far door is not an\n  exit.\"|* \"It simply marks a rotation\n  in perspective.\"",
    "* \"If you can read this,\n  press the blue switch.\"",
    "* \"If you can read this,\n  press the red switch.\"",
    "* \"If you can read this,\n  press the green switch.\""};
  static const int16_t px[4][2] = {{8800, 4536}, {8800, 4238}, {8622, 4238}, {8502, 3656}};
  obj_sign(px[k][0], px[k][1], 40, 20, plaque[k]);
  /* the doors' spikes: where Frisk came in, down; the far door's, up until the right switch */
  switch (k) {
    case 0:
      obj_spikes(8760, 4620, 1, 2, false, ACTOR_NONE);
      obj_spikes(8960, 4840, 2, 1, false, ACTOR_NONE);
      break;
    case 1:
      obj_spikes(8960, 4140, 2, 1, false, ACTOR_NONE);
      obj_spikes(8760, 4340, 1, 2, !flag(F_ROTATE2), ACTOR_SPIKES);
      break;
    case 2:
      obj_spikes(8660, 4340, 1, 2, false, ACTOR_NONE);
      obj_spikes(8440, 4140, 2, 1, !flag(F_ROTATE3), ACTOR_SPIKES);
      break;
    case 3:
      obj_spikes(8440, 3960, 2, 1, false, ACTOR_NONE);
      obj_spikes(8660, 3740, 1, 2, !flag(F_ROTATE4), ACTOR_SPIKES);
      break;
  }
}

static int ev_color_switch(Ev *c) {
  EV_BEGIN;
  {
    int k = c->self ? c->self->arg : 0;
    const CSwitch *s = &cswitch[k / 3][k % 3];
    if (flag(F_ROTATE1 + k / 3) && s->right) {
      NARRATE("* (The switch is already\n  pressed.)");
      return 1;
    }
    c->v[0] = k;
  }
  if (cswitch[c->v[0] / 3][c->v[0] % 3].hidden) NARRATE("* (You found a switch hidden\n  behind the pillar.)");
  NARRATE("* (You pressed the switch.)");
  if (cswitch[c->v[0] / 3][c->v[0] % 3].right) {
    set_flag(F_ROTATE1 + c->v[0] / 3, 1);
    obj_spikes_set(obj_find(ACTOR_SPIKES), false);
    return 1;
  }
  rotate_fails++;
  /* the floor gives way */
  c->self = NULL, c->v[4] = FALL_ROTATE;
  c->at = NULL, c->id = EV_FALLING;
  return ev_fall(c);
  EV_END;
}

/* ---------------------------------------------------------------- the rooms' things */
static const uint16_t frog_spr[4] = {SPR_O_SMALLFROG};

static Obj *item_here(uint16_t spr, int x, int y, int item, int f) {
  if (flag(f)) return NULL;
  Obj *o = obj_deco(spr, x, y, false);
  if (o) o->flags |= OF_TALK | OF_BACK, o->event = EV_ITEM, o->arg = (int16_t)item, o->arg2 = (int16_t)f;
  return o;
}

/* the second rock puzzle's talking rock, by how far it has been asked */
static void rock3_place(Obj *r) {
  int s = flag(F_RUINS11_ROCK);
  static const int16_t at[6][2] = {{6720, 4875}, {6740, 4875}, {6740, 4855}, {6780, 4875}, {6740, 4875}, {6780, 4875}};
  r->x = at[s][0], r->y = at[s][1];
}

void ruins2_setup(int room, int from) {
  (void)from;
  int k = rotate_index(room);
  if (k >= 0) rotate_setup(k);
  switch (room) {
    case ROOM_RUINS7A: {
      static const uint16_t dish[3] = {SPR_O_CANDYDISH, SPR_O_CANDYDISH2, SPR_O_CANDYDISH_BAD};
      Obj *d = obj_deco(dish[flag(F_CANDY_SPILT) ? 2 : 0], 4950, 4215, true);
      if (d) d->flags |= OF_TALK, d->event = EV_CANDY, d->name = ACTOR_CANDY;
      break;
    }
    case ROOM_RUINS8:
      crack(5244, 4683, 72, 79, FALL_RUINS8);
      break;
    case ROOM_RUINS9: {
      obj_sign(5600, 4640, 20, 20, "* \"Three out of four gray rocks\n  recommend you push them.\"");
      Obj *r = obj_rock(5620, 4735, ACTOR_ROCK, 0);
      if (r && flag(F_RUINS9_ROCK)) r->x = 5680;
      obj_spikes(5740, 4680, 1, 6, !flag(F_RUINS9_ROCK), ACTOR_SPIKES);
      if (!flag(F_FLAVOR_CALL)) event_start(EV_CALL, NULL), event_cur_arg(CALL_FLAVOR);
      break;
    }
    case ROOM_RUINS10:
      obj_sign(6380, 4740, 20, 20, "* \"Didn't you read the sign\n  downstairs?\"");
      if (flag(F_FALL_LEAVES) < 10)
        for (unsigned i = 0; i < sizeof leaf_holes / sizeof leaf_holes[0]; i++)
          crack(leaf_holes[i][0] + 2, leaf_holes[i][1] + 2, leaf_holes[i][2] - 4, leaf_holes[i][3] - 4, FALL_LEAVES);
      if (flag(F_FLAVOR_CALL) == 1) event_start(EV_CALL, NULL), event_cur_arg(CALL_FLAVOR2);
      break;
    case ROOM_RUINS10B:
      obj_sign(6380, 5140, 20, 20, "* \"Please don't step on the\n  leaves.\"");
      break;
    case ROOM_RUINS11: {
      obj_rock(6700, 4795, ACTOR_ROCK, 0);
      obj_rock(6680, 4835, ACTOR_ROCK2, 0);
      Obj *r = obj_rock(6720, 4875, ACTOR_ROCK3, EV_ROCK_TALK);
      if (r) rock3_place(r);
      int s = flag(F_RUINS11_ROCK);
      obj_spikes(6880, 4820, 2, 3, s != 3 && s != 5, ACTOR_SPIKES);
      if (s == 3) obj_trigger(6830, 4800, 30, 100, EV_ROCK_OFF, ACTOR_TRIGGER);
      if (flag(F_FLAVOR_CALL) == 2) event_start(EV_CALL, NULL), event_cur_arg(CALL_ALLERGY);
      break;
    }
    case ROOM_RUINS12:
      obj_save(7170, 4860, "* (Knowing the mouse might one\n  day leave its hole and get\n  the cheese...)|* (It fills you with\n  determination.)");
      obj_sign(7240, 4860, 30, 20, "* (The cheese has been stuck\n  to the table for so long...)|* (...it's crystallized.)");
      obj_sign(7164, 4798, 14, 12, "* (Squeak!)");
      break;
    case ROOM_RUINS13:
      if (!flag(F_NAPSTA)) {
        Obj *n = obj_deco(SPR_O_NAPSTABLOOK_GR, 7525, 4832, true);
        if (n) n->name = ACTOR_NAPSTA, n->flags |= OF_TALK, n->event = EV_NAPSTA_SLEEP, obj_set_box(n, -7, -14, 44, 44);
        obj_trigger(7496, 4818, 20, 44, EV_NAPSTA_SLEEP, ACTOR_TRIGGER);
      }
      break;
    case ROOM_RUINS14: {
      obj_sign(7834, 4818, 22, 20, "* \"Spider Bake Sale\"|* \"All proceeds go to real\n  spiders.\"");
      Obj *w = obj_sign(7788, 4782, 30, 30, NULL);
      if (w) w->event = EV_WEB, w->arg = 0;
      w = obj_sign(7850, 4758, 48, 50, NULL);
      if (w) w->event = EV_WEB, w->arg = 1;
      break;
    }
    case ROOM_RUINS15: {
      static const int16_t fx[3] = {7660, 7780, 7900};
      for (int i = 0; i < 3; i++) {
        if (i == 0 && flag(F_SKIPS) > 5) continue;
        Obj *f = obj_npc(ACTOR_FROGGIT, frog_spr, fx[i], 4540, DIR_DOWN, EV_FROGGIT);
        if (f) f->arg = (int16_t)i, f->flags |= OF_ANIM, f->anim_speed = 20;
      }
      if (!flag(F_ITEM_CALL)) {
        Obj *t = obj_trigger(7960, 4540, 40, 80, EV_CALL, ACTOR_TRIGGER);
        if (t) t->arg = CALL_ITEMS;
      }
      break;
    }
    case ROOM_RUINS16:
      obj_sign(8180, 4520, 20, 20, "* \"There is just one switch.\"");
      for (int i = 0; i < 6; i++) crack(one_cracks[i][0], one_cracks[i][1], 16, 17, FALL_ONE_A + i);
      obj_spikes(8680, 4620, 1, 2, !flag(F_ONESWITCH), ACTOR_SPIKES);
      break;
    case ROOM_RUINS16A:
    case ROOM_RUINS16F: {
      Obj *v = obj_deco(SPR_O_VEGETABLEOUTSIDE, room == ROOM_RUINS16A ? 8300 : 8660, room == ROOM_RUINS16A ? 4980 : 5117, true);
      if (v) v->flags |= OF_TALK, v->event = EV_VEGGIE;
      break;
    }
    case ROOM_RUINS16B: {
      Obj *s = obj_deco(SPR_O_SWITCH, 8460, 4935, false);
      if (s) s->flags |= OF_TALK | OF_BACK, s->event = EV_ONE_SWITCH, s->frame = flag(F_ONESWITCH) ? 1 : 0, obj_set_box(s, 0, 0, 20, 30);
      break;
    }
    case ROOM_RUINS16D:
      item_here(SPR_O_RIBBON, 8260, 5120, ITEM_FADED_RIBBON, F_RIBBON);
      break;
    case ROOM_RUINS16E:
      if (flag(F_NAPSTA) == 1 && !flag(F_NAPSTA_LIE)) {
        Obj *n = obj_deco(SPR_O_NAPSTABLOOK_GR, 8495, 5117, true);
        if (n) n->name = ACTOR_NAPSTA, n->flags |= OF_TALK, n->event = EV_NAPSTA_LIE;
      }
      break;
    case ROOM_RUINS17B:
      if (rotate_fails >= 2 && !flag(F_ROTATE2)) say_on_entry("* (You notice a switch hidden\n  behind the pillar.)");
      break;
    case ROOM_RUINS19: {
      Obj *f = obj_npc(ACTOR_FROGGIT, frog_spr, 9400, 3700, DIR_DOWN, EV_FROGGIT);
      if (f) f->arg = 3, f->flags |= OF_ANIM, f->anim_speed = 20;
      break;
    }
    case ROOM_RUINS20:
      item_here(SPR_O_GROUNDKNIFE, 9364, 3566, ITEM_TOY_KNIFE, F_TOYKNIFE);
      break;
    case ROOM_RUINS21:
      obj_save(8940, 3120, "* (Seeing such a cute, tidy\n  house in the RUINS gives\n  you determination.)");
      obj_sign(9004, 3290, 34, 30, "* (Every time this old tree\n  grows any leaves, they fall\n  right off.)");
      if (!flag(F_TORIEL_HOME)) {
        Obj *t = obj_toriel(8996, 3240, DIR_DOWN);
        if (t) t->spr[0] = SPR_O_TORIEL_D_PHONE, t->flags &= (uint16_t)~OF_WALKANIM;
        obj_trigger(8880, 3330, 280, 40, EV_TORIEL_TREE, ACTOR_TRIGGER);
      }
      break;
  }
}

/* each frame: the rocks on their switches */
void ruins2_update(void) {
  if (g_room == ROOM_RUINS9 && !flag(F_RUINS9_ROCK)) {
    Obj *r = obj_find(ACTOR_ROCK);
    if (r && r->x >= 5676 && r->x <= 5684) {
      set_flag(F_RUINS9_ROCK, 1);
      r->x = 5680;
      r->flags &= (uint16_t)~OF_SOLID;   /* it stays */
      obj_spikes_set(obj_find(ACTOR_SPIKES), false);
    }
  }
}

/* ---------------------------------------------------------------- the events */
static int ev_candy(Ev *c) {
  EV_BEGIN;
  if (flag(F_CANDY_SPILT)) {
    NARRATE("* Look at what you've done.");
    return 1;
  }
  if (item_count() >= NUM_ITEMS) {
    NARRATE("* You tried to take a piece of\n  candy, but you didn't have\n  any room.");
    return 1;
  }
  set_flag(F_CANDY_TAKEN, flag(F_CANDY_TAKEN) + 1);
  if (flag(F_CANDY_TAKEN) >= (flag(F_HARDMODE) ? 3 : 4)) {
    set_flag(F_CANDY_SPILT, 1);
    if (c->self) c->self->spr[0] = SPR_O_CANDYDISH_BAD;
    NARRATE("* You took too much too fast.\n* The candy spills onto the\n  floor.");
    return 1;
  }
  item_add(ITEM_MONSTER_CANDY);
  if (flag(F_CANDY_TAKEN) == 1) NARRATE("* You took a piece of candy.\n* (Press [TOOLBOX] to open\n  the menu.)");
  else if (flag(F_CANDY_TAKEN) == 2) NARRATE("* You took more candy.\n* How disgusting...");
  else if (flag(F_HARDMODE)) NARRATE("* In this hellish world, you\n  can only take 3 pieces of\n  candy...");
  else NARRATE("* You take another piece.\n* You feel like the scum of\n  the earth...");
  EV_END;
}

static const char call_hello[] = "* Ring...|`0* Hello?\n* This is TORIEL.|";

static int ev_call(Ev *c) {
  EV_BEGIN;
  if (c->self) c->v[0] = c->self->arg, c->self->kind = OBJ_NONE;
  WAIT_UNTIL(!world_fading());
  WAIT(15);
  switch (c->v[0]) {
    case CALL_FLAVOR:
      set_flag(F_FLAVOR_CALL, 1);
      SAY(FACE_TORIEL, "* Ring...|`0* Hello? TORIEL here.|* A small question...|* Cinnamon or butterscotch?@Cinnamon@Bscotch");
      set_flag(F_FLAVOR, CHOICE() ? 2 : 1);
      SAY(FACE_TORIEL, "`0* Oh, I see.\n* Thank you very much!");
      break;
    case CALL_FLAVOR2:
      set_flag(F_FLAVOR_CALL, 2);
      SAYF(FACE_TORIEL, "%s* You would still eat\n  %s, though?|* Just checking!|* Thank you for waiting.",
           call_hello, flag(F_FLAVOR) == 2 ? "cinnamon" : "butterscotch");
      break;
    case CALL_ALLERGY:
      set_flag(F_FLAVOR_CALL, 3), set_flag(F_ALLERGY_CALL, 1);
      SAY(FACE_TORIEL, "* Ring...|`0* Hello?|* Any allergies, by the way?|`1* Why do I ask? Oh...|`0* No reason at all.");
      break;
    case CALL_ITEMS:
      set_flag(F_ITEM_CALL, 1);
      SAY(FACE_TORIEL, "* Ring...|`0* Hello?|* Things are a bit messy\n  around here, I am afraid.|* Pick up what you like,\n  but leave room in your\n  pockets.|* You never know what you\n  will want later.");
      break;
  }
  EV_END;
}

static int ev_rock_talk(Ev *c) {
  Obj *r = obj_find(ACTOR_ROCK3);
  EV_BEGIN;
  if (!r) return 1;
  switch (flag(F_RUINS11_ROCK)) {
    case 0:
      SAY(FACE_NONE, "* Hey now! Who's shoving?|* ...|* Oh, you want me to move?\n* You could have just asked.");
      set_flag(F_RUINS11_ROCK, 1);
      WALK(obj_get(ACTOR_ROCK3), 6740, 4875, 1);
      NARRATE("* (The rock moved a little,\n  and stopped.)");
      break;
    case 1:
      SAY(FACE_NONE, "* Huh? Not far enough?\n* Alright, alright.");
      set_flag(F_RUINS11_ROCK, 2);
      WALK(obj_get(ACTOR_ROCK3), 6740, 4855, 1);
      break;
    case 2:
      NARRATE("* (You tell the rock it's\n  going the wrong way.)");
      SAY(FACE_NONE, "* Oh! Why didn't ya say so,\n  pardner?");
      set_flag(F_RUINS11_ROCK, 3);
      WALK(obj_get(ACTOR_ROCK3), 6740, 4875, 1);
      WALK(obj_get(ACTOR_ROCK3), 6780, 4875, 1);
      obj_spikes_set(obj_find(ACTOR_SPIKES), false);
      obj_trigger(6830, 4800, 30, 100, EV_ROCK_OFF, ACTOR_TRIGGER);
      break;
    case 3:
    case 5:
      SAY(FACE_NONE, "* Well, here I am, pardner.");
      break;
    case 4:
      NARRATE("* (You ask the rock to stay\n  on the switch.)");
      SAY(FACE_NONE, "* Stay here? Sure thing.|* Asking works, you know.");
      set_flag(F_RUINS11_ROCK, 5);
      WALK(obj_get(ACTOR_ROCK3), 6780, 4875, 1);
      obj_spikes_set(obj_find(ACTOR_SPIKES), false);
      break;
  }
  EV_END;
}

static int ev_rock_off(Ev *c) {
  EV_BEGIN;
  if (c->self) c->self->kind = OBJ_NONE;
  if (flag(F_RUINS11_ROCK) != 3) return 1;
  set_flag(F_RUINS11_ROCK, 4);
  WALK(obj_get(ACTOR_ROCK3), 6740, 4875, 1);
  obj_spikes_set(obj_find(ACTOR_SPIKES), true);
  NARRATE("* (The rock rolled off the\n  switch.)");
  EV_END;
}

static int ev_napsta_sleep(Ev *c) {
  EV_BEGIN;
  if (flag(F_NAPSTA)) return 1;
  NARRATE("* (A ghost says 'z' over and\n  over, faking sleep.)|* Push through it?@Yes@No");
  if (CHOICE() != 0) {
    /* he steps back out of its way */
    PL_WALK(pl.x - 12, pl.y, 2);
    return 1;
  }
  BATTLE(FORM_NAPSTABLOOK);
  if (!flag(F_NAPSTA)) set_flag(F_NAPSTA, battle_result() == BR_FLED ? 3 : 1);
  obj_get(ACTOR_NAPSTA)->kind = OBJ_NONE;
  {
    Obj *t = obj_find(ACTOR_TRIGGER);
    if (t) t->kind = OBJ_NONE;
  }
  EV_END;
}

static int ev_napsta_lie(Ev *c) {
  EV_BEGIN;
  SAY(FACE_NONE, "* oh...^2 it's you...|* i was just lying here...|* ...^2 i'll get out of your\n  way...");
  for (c->v[0] = 255; c->v[0] > 0; c->v[0] -= 15) {
    obj_get(ACTOR_NAPSTA)->alpha = (uint8_t)c->v[0];
    WAIT(1);
  }
  obj_get(ACTOR_NAPSTA)->kind = OBJ_NONE;
  set_flag(F_NAPSTA_LIE, 1);
  EV_END;
}

static int ev_web(Ev *c) {
  EV_BEGIN;
  c->v[0] = c->self ? c->self->arg : 0;
  if (c->v[0] == 0) NARRATE("* (There's a note on the web.)\n* \"Spider Donut: 7G\"|* Leave 7G in the web?@Yes@No");
  else NARRATE("* (There's a note on the web.)\n* \"Spider Cider: 18G\"|* Leave 18G in the web?@Yes@No");
  if (CHOICE() != 0) return 1;
  {
    int cost = c->v[0] ? 18 : 7;
    if (sv.gold < cost) {
      NARRATE("* You didn't have enough gold.");
      return 1;
    }
    if (item_count() >= NUM_ITEMS) {
      NARRATE("* You are carrying too many\n  items.");
      return 1;
    }
    sv.gold = (uint16_t)(sv.gold - cost);
    item_add(c->v[0] ? ITEM_SPIDER_CIDER : ITEM_SPIDER_DONUT);
    set_flag(c->v[0] ? F_CIDER_BOUGHT : F_DONUT_BOUGHT, 1);
  }
  if (c->v[0] == 0) NARRATE("* Some spiders crawled down\n  and gave you a donut.");
  else NARRATE("* Some spiders crawled down\n  and gave you a jug.");
  EV_END;
}

static int ev_froggit(Ev *c) {
  EV_BEGIN;
  c->v[0] = c->self ? c->self->arg : 0;
  switch (c->v[0]) {
    case 0:
      NARRATE("* Ribbit, ribbit.|* (Tip: [BACK] shows the\n  whole text at once.)");
      break;
    case 1:
      NARRATE("* Ribbit, ribbit.|* (Hey, human.)|* (Have you seen the key with\n  a 4 on your calculator?)|* (I'm told 4 means \"four\n  frogs.\")|* (But I only see three frogs\n  in this room...)");
      break;
    case 2:
      if (!flag(F_FROGGIT_YELLOW))
        NARRATE("* Ribbit, ribbit.|* (A #YYELLOW#X name means you\n  can SPARE that monster.)|* (Too easy, some say.\n  Should we drop yellow?)@Yes@No");
      else if (flag(F_FROGGIT_YELLOW) == 1)
        NARRATE("* Ribbit, ribbit.|* (Everyone dropped yellow.)|* (Try #PPINK#X names\n  instead?)@Yes@No");
      else
        NARRATE("* Ribbit, ribbit.|* (Pink names...^2 Or should\n  everyone go back to #YYELLOW#X?)@Yellow@Pink");
      if (flag(F_FROGGIT_YELLOW) == 2) set_flag(F_FROGGIT_YELLOW, CHOICE() == 0 ? 0 : 2);
      else if (CHOICE() == 0) set_flag(F_FROGGIT_YELLOW, flag(F_FROGGIT_YELLOW) + 1);
      NARRATE("* Ribbit.");
      break;
    case 3:
      NARRATE("* Ribbit, ribbit.|* (#BTORIEL#X went by with\n  groceries a while ago.)|* (Nobody dared ask what\n  for.)");
      break;
  }
  EV_END;
}

static int ev_item(Ev *c) {
  EV_BEGIN;
  if (!c->self) return 1;
  if (item_count() >= NUM_ITEMS) {
    NARRATE("* (You're carrying too much.)");
    return 1;
  }
  item_add(c->self->arg);
  set_flag(c->self->arg2, 1);
  c->v[0] = c->self->arg;
  c->self->kind = OBJ_NONE;
  SAYF(FACE_NONE, "* (You got the %s.)", item_info(c->v[0])->name);
  EV_END;
}

static int ev_one_switch(Ev *c) {
  EV_BEGIN;
  if (flag(F_ONESWITCH)) {
    NARRATE("* (The switch is already\n  flipped.)");
    return 1;
  }
  set_flag(F_ONESWITCH, 1);
  if (c->self) c->self->frame = 1;
  NARRATE("* (You flipped the switch.)\n* (Somewhere, spikes lowered.)");
  EV_END;
}

static int ev_veggie(Ev *c) {
  EV_BEGIN;
  NARRATE("* (It's a vegetable.)|* (...it looks back at you.)");
  BATTLE(FORM_VEGETOID);
  if (c->self) c->self->kind = OBJ_NONE;
  EV_END;
}

static int ev_toriel_tree(Ev *c) {
  Obj *t = obj_find(ACTOR_TORIEL);
  EV_BEGIN;
  if (c->self) c->self->kind = OBJ_NONE;
  if (!t) return 1;
  SAY(FACE_TORIEL, "`0* Hello?\n* This is TORIEL.|`7* ...!");
  t = obj_get(ACTOR_TORIEL);
  t->spr[0] = SPR_TORIEL_D;
  t->flags |= OF_WALKANIM;
  WALK(t, 8996, pl.y - 52, 3);
  SAY(FACE_TORIEL, "`7* How did you get here, my\n  child?|`3* Are you hurt?");
  if (sv.hp >= sv.maxhp) SAY(FACE_TORIEL, "`0* Not a scratch...\n* Impressive!|`1* But still...");
  else if (sv.hp > 2) SAY(FACE_TORIEL, "`0* There, there, I will heal\n  you.");
  else SAY(FACE_TORIEL, "`5* Who did this to you?\n* You will get an apology.");
  sv.hp = sv.maxhp;
  SAY(FACE_TORIEL, "`1* I left you alone too long.\n* I am sorry.|`0* Oh, the surprise is\n  spoiled now...|`2* Come along, little one!");
  set_flag(F_TORIEL_HOME, 1);
  WALK(obj_get(ACTOR_TORIEL), 9006, 3070, 3);
  obj_get(ACTOR_TORIEL)->kind = OBJ_NONE;
  EV_END;
}

int ruins2_event(Ev *c) {
  switch (c->id) {
    case EV_FALLING: return ev_fall(c);
    case EV_COLOR_SWITCH: return ev_color_switch(c);
    case EV_CANDY: return ev_candy(c);
    case EV_CALL: return ev_call(c);
    case EV_ROCK_TALK: return ev_rock_talk(c);
    case EV_ROCK_OFF: return ev_rock_off(c);
    case EV_NAPSTA_SLEEP: return ev_napsta_sleep(c);
    case EV_NAPSTA_LIE: return ev_napsta_lie(c);
    case EV_WEB: return ev_web(c);
    case EV_FROGGIT: return ev_froggit(c);
    case EV_ITEM: return ev_item(c);
    case EV_ONE_SWITCH: return ev_one_switch(c);
    case EV_VEGGIE: return ev_veggie(c);
    case EV_TORIEL_TREE: return ev_toriel_tree(c);
  }
  return -1;
}
