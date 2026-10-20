/* Snowdin Forest, from the spaghetti to the bridge into town: the snow puzzle and the Dogi, the XOXO puzzles,
 * the Multicolor Tile Puzzle, the Dog House, the ice puzzle and the slide through the trees, the snow poffs and
 * Greater Dog, the Gauntlet of Deadly Terror, the cliff below and the mysterious door. */
#include <stdio.h>
#include <string.h>
#include "battle.h"
#include "event.h"
#include "events.h"
#include "game.h"
#include "gfx.h"
#include "items.h"
#include "monsters.h"
#include "snowdin.h"
#include "world.h"

static void gone(Obj *o) { o->kind = OBJ_NONE; }

/* ---------------------------------------------------------------- the XOXO puzzles */
/* a puzzle's tiles (their top-left), its switch, its spikes */
typedef struct {
  const int16_t (*tiles)[2];
  int n;
  int16_t sw_x, sw_y;
  int flag;
} Xoxo;
static const int16_t x1_tiles[][2] = {{7460, 1240}, {7540, 1240}};
static const int16_t x2_tiles[][2] = {{8180, 1200}, {8240, 1200}, {8280, 1200}, {8240, 1220}, {8280, 1220}, {8200, 1240},
                                      {8240, 1240}, {8260, 1240}, {8280, 1240}, {8180, 1260}, {8240, 1260}, {8180, 1280},
                                      {8200, 1280}, {8220, 1280}, {8240, 1280}};
static const int16_t x3_tiles[][2] = {{9540, 1160}, {9700, 1160}, {9540, 1180}, {9580, 1180}, {9660, 1180}, {9620, 1220},
                                      {9580, 1240}, {9620, 1240}, {9580, 1260}, {9660, 1260}, {9540, 1300}, {9700, 1300}};
static const Xoxo xoxos[3] = {
  {x1_tiles, 2, 7500, 1196, F_XOXO1}, {x2_tiles, 15, 8098, 1240, F_XOXO2}, {x3_tiles, 12, 9743, 1224, F_XOXO3}};
static const Xoxo *xo;            /* the room's, or NULL */
static uint8_t xo_state[16];      /* 0 X, 1 O */
static int xo_on = -1;            /* the tile under Frisk */
static bool xo_kill;              /* XOXO 2: the switch in the tree was pressed */

static void xo_show(void) {
  for (int i = 0; i < MAX_OBJS; i++) {
    Obj *o = &objs[i];
    if (o->kind != OBJ_DECO || o->name != ACTOR_TILE) continue;
    bool solved = flag(xo->flag);
    o->spr[0] = (uint16_t)(xo_state[o->arg] || solved ? SPR_OTILE : SPR_XTILE);
    o->frame = solved ? 1 : 0;
  }
}

static void xo_setup(int k) {
  xo = &xoxos[k];
  xo_on = -1, xo_kill = false;
  memset(xo_state, 0, sizeof xo_state);
  for (int i = 0; i < xo->n; i++) {
    Obj *o = obj_deco(SPR_XTILE, xo->tiles[i][0], xo->tiles[i][1], false);
    if (o) o->name = ACTOR_TILE, o->arg = (int16_t)i, o->flags |= OF_BACK, o->bh = 0;
  }
  Obj *sw = obj_deco(SPR_SNOWSWITCH, xo->sw_x, xo->sw_y, false);
  if (sw) sw->flags |= OF_BACK | OF_TALK, sw->event = EV_XOXO_SWITCH, obj_set_box(sw, 0, 2, 24, 22), sw->name = ACTOR_SWITCH;
  xo_show();
}

/* Frisk steps on a tile: X turns O, O turns back to X */
static void xo_update(void) {
  if (!xo || flag(xo->flag)) return;
  int fx = pl.x + 10, fy = pl.y + 25, on = -1;
  for (int i = 0; i < xo->n; i++)
    if (fx >= xo->tiles[i][0] && fx < xo->tiles[i][0] + 20 && fy >= xo->tiles[i][1] && fy < xo->tiles[i][1] + 20) on = i;
  if (on >= 0 && on != xo_on) {
    xo_state[on] = !xo_state[on];
    xo_show();
  }
  xo_on = on;
}

/* past the first XOXO, Papyrus waits by the spikes */
static void pap_xoxo1(void) {
  if (flag(F_PAP_XOXO1)) return;
  obj_papyrus(7720, 1252, DIR_LEFT);
  obj_trigger(7600, 1180, 30, 120, EV_PAP_XOXO, ACTOR_TRIGGER);
}

static int ev_xoxo_switch(Ev *c) {
  EV_BEGIN;
  if (!xo || flag(xo->flag)) {
    NARRATE("* (The switch is already pressed.)");
    return 1;
  }
  c->v[0] = 1;
  for (int i = 0; i < xo->n; i++) c->v[0] &= xo_state[i];
  if (c->v[0] || (xo == &xoxos[1] && xo_kill)) {
    set_flag(xo->flag, 1);
    xo_show();
    /* the spikes go down, or (the ice puzzle) the bridge over to the trees is there */
    for (int i = 0; i < MAX_OBJS; i++) {
      if (objs[i].name == ACTOR_SPIKES) obj_spikes_set(&objs[i], false), objs[i].flags |= OF_HIDDEN;
      if (objs[i].name == ACTOR_BLOCK) objs[i].flags &= (uint16_t)~OF_SOLID;
    }
    if (xo == &xoxos[0]) pap_xoxo1();
    NARRATE("* (Click!)");
  } else {
    memset(xo_state, 0, sizeof xo_state);
    xo_show();
    NARRATE("* (Click...)\n* (The puzzle reset.)");
  }
  EV_END;
}

/* ---------------------------------------------------------------- the Multicolor Tile Puzzle */
#define TILES_X 8660
#define TILES_Y 1200
#define TILES_W 8
#define TILES_H 6
static int tiles_t;   /* frames left of its colours shuffling, -1 off */

/* ---------------------------------------------------------------- the rooms' things */
static Obj *spikes(int x, int y, int rows, bool raised) {
  Obj *o = obj_spikes(x, y, 1, rows, raised, ACTOR_SPIKES);
  if (o) o->spr[0] = SPR_SNOWSPIKES, o->frame = 0, o->flags |= OF_BACK;
  if (o && !raised) o->flags |= OF_HIDDEN;
  return o;
}

void snow2_setup(int room, int from) {
  (void)from;
  xo = NULL, tiles_t = -1;
  switch (room) {
    case ROOM_SPAGHETTI:
      obj_save(6745, 695, "* (Knowing the mouse might one day find a way to heat up the spaghetti...)\n* (It fills you with determination.)");
      obj_sign(6690, 658, 16, 20, "* (A note by the spaghetti.)|* ENJOY, HUMAN!|* (IT'S A TRAP: YOU'LL BE TOO\n  BUSY EATING TO MOVE ON!)\n* - PAPYRUS");
      obj_sign(6734, 646, 38, 34, "* (A plate of frozen spaghetti.)|* (It's so cold, it's stuck\n  to the table.)");
      obj_sign(6788, 644, 38, 36, "* (It's a microwave.)|* (Its only setting is\n  \"spaghetti.\")\n* (It's unplugged.)");
      obj_sign(6742, 585, 18, 14, "* (Squeak!)");
      break;
    case ROOM_SNOWPUZZLE:
      obj_sign(6984, 632, 20, 22, "* Warning: Dog Marriage|* (Yes, you read that correctly.)");
      obj_sign(6980, 920, 80, 80, "* (It's a map of this room, drawn in the snow.)\n* (An X marks its top right.)");
      {
        Obj *sw = obj_trigger(7300, 650, 46, 68, EV_SNOW_SWITCH, ACTOR_SWITCH);
        if (sw) sw->flags = OF_TALK;
      }
      spikes(7120, 940, 3, !flag(F_SNOWPUZZLE));
      if (!flag(F_DOGI)) obj_trigger(7300, 900, 140, 140, EV_DOGI, ACTOR_TRIGGER);
      break;
    case ROOM_XOXO1:
      obj_sign(7456, 1262, 20, 20, "* Turn every X into an O.\n* Then press the switch.");
      xo_setup(0);
      spikes(7680, 1220, 3, !flag(F_XOXO1));
      if (flag(F_XOXO1)) pap_xoxo1();
      break;
    case ROOM_XOXO2:
      obj_sign(8096, 1198, 20, 22, "* Turn every X into an O.\n* Then press the switch.");
      xo_setup(1);
      spikes(8360, 1160, 8, !flag(F_XOXO2));
      {
        /* the switch in the tree */
        Obj *k = obj_trigger(7956, 1150, 30, 56, EV_XOXO_SWITCH, ACTOR_TRIGGER3);
        if (k) k->flags = OF_TALK, k->arg = 1;
      }
      if (!flag(F_XOXO2)) {
        Obj *p = obj_papyrus(7880, 1222, DIR_DOWN);
        if (p) p->event = EV_PAP_XOXO, p->arg = 2, p->flags |= OF_TALK;
        if (!flag(F_PAP_XOXO1) || flag(F_PAP_XOXO1) == 1) obj_trigger(7700, 1160, 30, 140, EV_PAP_XOXO, ACTOR_TRIGGER);
      } else {
        Obj *s = obj_sans(8390, 1216, DIR_DOWN);
        if (s) s->event = EV_SNOW_SANS_CLIFF, s->arg = 1, s->flags |= OF_TALK;
      }
      break;
    case ROOM_TILEPUZZLE:
      obj_sign(8818, 1176, 26, 30, "* (The machine isn't working.)");
      if (!flag(F_TILES)) {
        obj_papyrus(8840, 1252, DIR_LEFT);
        obj_trigger(8560, 1160, 30, 100, EV_TILES, ACTOR_TRIGGER);
        /* (the tiles aren't there yet: nothing to walk on past the bridge) */
        Obj *b = obj_trigger(8656, 1196, 8, 130, 0, ACTOR_BLOCK);
        if (b) b->flags = OF_SOLID;
      }
      {
        Obj *s = obj_sans(8834, 1290, DIR_DOWN);
        if (s) s->event = EV_SNOW_SANS_CLIFF, s->arg = 2, s->flags |= OF_TALK;
      }
      break;
    case ROOM_DOGHOUSE:
      obj_save(9260, 1300, flag(F_LESSERDOG) == 2 ? "* (Snow can always be broken down and rebuilt into something more useful.)\n* (This simple fact fills you with determination.)"
                                                   : "* (Knowing the dog will never give up trying to make the perfect snowdog...)\n* (It fills you with determination.)");
      obj_sign(9186, 1196, 20, 22, "* AWARE OF DOG\n* pleas pet dog");
      obj_sign(9356, 1170, 26, 34, "* (Lesser Dog keeps building a snow dog...)\n* (It gets taller every time.)");
      break;
    case ROOM_XOXO3:
      xo_setup(2);
      if (!flag(F_XOXO3)) {
        /* the gap to the trees, until the bridge comes */
        Obj *b = obj_trigger(9776, 1206, 10, 40, 0, ACTOR_BLOCK);
        if (b) b->flags = OF_SOLID;
      }
      /* off the ice's edge: down to the room below */
      obj_trigger(9522, 1322, 262, 14, EV_XOXO_FALL, ACTOR_TRIGGER2);
      break;
    case ROOM_XOXOFAIL:
      obj_sign(9520, 1440, 30, 40, "* (It's a snow Papyrus.)");
      obj_sign(9636, 1466, 30, 20, "* (It's a lump of snow with \"Sans\" written on it.)");
      break;
    case ROOM_POFFZONE: {
      static const int16_t poffs[][2] = {{10330, 1215}, {10485, 1235}, {10370, 1275}, {10465, 1285}, {10560, 1205},
                                         {10620, 1245}, {10665, 1215}, {10570, 1290}, {10745, 1270}, {10882, 1284}};
      for (int i = 0; i < 10; i++) {
        Obj *p = obj_trigger(poffs[i][0] - 14, poffs[i][1] - 10, 30, 14, EV_POFF, ACTOR_ITEM);
        if (p) p->flags = OF_TALK | OF_SOLID, p->arg = (int16_t)i;
      }
      obj_sign(10600, 1186, 18, 20, "* (Woof.)");
      if (!flag(F_GREATERDOG)) obj_trigger(10840, 1200, 30, 110, EV_GREATERDOG, ACTOR_TRIGGER);
      break;
    }
    case ROOM_DANGERBRIDGE:
      if (!flag(F_GAUNTLET)) {
        obj_papyrus(11830, 1236, DIR_LEFT);
        obj_sans(11850, 1200, DIR_DOWN);
        obj_trigger(11300, 1220, 30, 40, EV_GAUNTLET, ACTOR_TRIGGER);
      } else {
        Obj *s = obj_sans(11792, 1214, DIR_DOWN);
        if (s) s->event = EV_SNOW_SANS_CLIFF, s->arg = 3, s->flags |= OF_TALK;
      }
      break;
    case ROOM_SOUTHCLIFF: {
      Obj *s = obj_sans(10552, 2312, DIR_DOWN);
      if (s) s->event = EV_SNOW_SANS_CLIFF, s->arg = 4, s->flags |= OF_TALK;
      break;
    }
    case ROOM_DOORROOM: {
      Obj *d = obj_trigger(9890, 1636, 40, 54, EV_MYSTERY_DOOR, ACTOR_ITEM);
      if (d) d->flags = OF_TALK | OF_SOLID;
      break;
    }
  }
}

void snow2_update(void) {
  xo_update();
}

void snow2_draw(void) {
  /* the ice puzzle's bridge, once solved */
  if (g_room == ROOM_XOXO3 && flag(F_XOXO3)) gfx_rect(9780 - cam_x, 1214 - cam_y, 60, 24, rgb565(0xCFDBFF), 255);
  /* the tile puzzle: its colours shuffle, then settle (the map's red and pink) */
  if (g_room == ROOM_TILEPUZZLE && tiles_t > 0) {
    static const uint32_t cols[] = {0xFF4040, 0xFFC0C0, 0xFFFF40, 0x40C040, 0xFF8000, 0x4060FF, 0xA040FF};
    for (int y = 0; y < TILES_H; y++)
      for (int x = 0; x < TILES_W; x++)
        gfx_rect(TILES_X + x * 20 - cam_x, TILES_Y + y * 20 - cam_y, 20, 20,
                 rgb565(cols[(x * 7 + y * 3 + tiles_t / 4) % 7]), 255);
    tiles_t--;
  } else if (g_room == ROOM_TILEPUZZLE && !flag(F_TILES)) {
    /* (before the machine: plain grey tiles) */
    gfx_rect(TILES_X - cam_x, TILES_Y - cam_y, TILES_W * 20, TILES_H * 20, rgb565(0x9090A0), 255);
  }
}

/* ---------------------------------------------------------------- the events */
/* XOXO 2's switch in the tree: then the puzzle's own switch solves it */
static int ev_tree_switch(Ev *c) {
  EV_BEGIN;
  xo_kill = true;
  NARRATE("* (There's a switch on this tree.)\n* (You press it.)");
  EV_END;
}

static int ev_snow_switch(Ev *c) {
  EV_BEGIN;
  if (flag(F_SNOWPUZZLE)) {
    NARRATE("* (The switch is already pressed.)");
  } else {
    NARRATE("* (There's a switch under the snow.)\n* (You press it.)");
    set_flag(F_SNOWPUZZLE, 1);
    for (int i = 0; i < MAX_OBJS; i++)
      if (objs[i].name == ACTOR_SPIKES) obj_spikes_set(&objs[i], false), objs[i].flags |= OF_HIDDEN;
  }
  EV_END;
}

static int ev_dogi(Ev *c) {
  Obj *a = obj_get(ACTOR_DOGAMY), *b = obj_get(ACTOR_DOGARESSA);
  EV_BEGIN;
  /* they come sniffing */
  a = obj_deco(SPR_DOGAMY_OW, 7330, 900, false);
  if (a) a->name = ACTOR_DOGAMY;
  b = obj_deco(SPR_DOGARESSA_OW, 7410, 900, false);
  if (b) b->name = ACTOR_DOGARESSA;
  a = obj_get(ACTOR_DOGAMY), b = obj_get(ACTOR_DOGARESSA);
  WALK_TO(a, pl.x - 8, pl.y + 26, 2);
  WALK(b, pl.x + 30, pl.y + 26, 2);
  NARRATE("* Sniff... sniff... a strange smell.|* (A strange smell must be... eliminated!)");
  BATTLE(FORM_DOGI);
  set_flag(F_DOGI, bt.mon[0].killed || bt.mon[1].killed ? 2 : 1);
  gone(a), gone(b);
  if (c->self) gone(c->self);
  EV_END;
}

static int ev_pap_xoxo(Ev *c) {
  Obj *p = obj_get(ACTOR_PAPYRUS);
  int k = c->self && c->self->kind == OBJ_NPC ? c->self->arg : 0;
  EV_BEGIN;
  if (g_room == ROOM_XOXO1) {
    /* past the first XOXO: the spaghetti */
    pl.dir = DIR_RIGHT;
    SAY(FACE_PAPYRUS, "`0* HUMAN!!! YOU SOLVED IT!\n* BUT... WHAT ABOUT THE SPAGHETTI?|* DID YOU EAT IT? OR LEAVE IT ALONE?@Ate it@Left it");
    if (CHOICE() == 0) SAY(FACE_PAPYRUS, "`2* WOWIE!!! YOU ATE MY SPAGHETTI!!!|* IT MUST HAVE BEEN DELICIOUS!\n* NYEH HEH HEH!");
    else SAY(FACE_PAPYRUS, "`4* YOU LEFT IT ALONE?|`0* SO THERE'S STILL SOME LEFT FOR ME?\n`2* NYEH HEH HEH!");
    set_flag(F_PAP_XOXO1, 1);
    LEAVE(p, 6, 7740, 1252);
    if (c->self) gone(c->self);
    return 1;
  }
  /* the second XOXO: his face, frozen in the snow */
  if (k == 2) {
    SAY(FACE_PAPYRUS, "`0* NEED HELP WITH THE PUZZLE?@Yes@No");
    if (CHOICE() == 0) SAY(FACE_PAPYRUS, "`4* WELL... THERE MAY OR MAY NOT BE A SWITCH ON THAT TREE OVER THERE.");
    else SAY(FACE_PAPYRUS, "`2* GOOD! A HUMAN WHO SOLVES PUZZLES!\n* NYEH HEH HEH!");
    return 1;
  }
  pl.dir = DIR_RIGHT;
  SAY(FACE_PAPYRUS, "`4* SO... WHILE I WAITED, I REDID THE SNOW TO LOOK LIKE MY FACE.|* THEN IT FROZE. THE PUZZLE CHANGED.\n`0* ASK ME IF YOU NEED A HINT!");
  set_flag(F_PAP_XOXO1, 2);
  if (c->self) gone(c->self);
  EV_END;
}

static int ev_tiles(Ev *c) {
  Obj *p = obj_get(ACTOR_PAPYRUS);
  EV_BEGIN;
  pl.dir = DIR_RIGHT;
  SAY(FACE_PAPYRUS, "`0* HUMAN!!! THE MULTICOLOR\n  TILE PUZZLE!|* ONE PULL, AND THE TILES\n  CHANGE COLOR!|* EVERY COLOR DOES SOMETHING\n  DIFFERENT!|* RED: WALL. YELLOW: ZAP.\n* GREEN: MONSTER.\n* PINK: NOTHING.|* ORANGE, BLUE AND PURPLE... IT GETS COMPLICATED.|`2* GOT ALL THAT???@Yes@No");
  if (CHOICE() == 1)
    SAY(FACE_PAPYRUS, "`3* OKAY! AGAIN!\n* PINK TILES ARE... WAIT, NO...|`4* ...|`0* JUST TRY IT ON YOUR OWN!");
  SAY(FACE_PAPYRUS, "`2* LET'S DO IT!!!");
  /* the machine: the colours shuffle, then settle */
  tiles_t = 90;
  WAIT(95);
  tiles_t = 0;
  SAY(FACE_PAPYRUS, "`4* ...");
  set_flag(F_TILES, 1);
  for (int i = 0; i < MAX_OBJS; i++)
    if (objs[i].name == ACTOR_BLOCK) objs[i].kind = OBJ_NONE;
  /* he spins out of the room */
  LEAVE(p, 8, 8920, 1252);
  if (c->self) gone(c->self);
  EV_END;
}

static int ev_xoxo_fall(Ev *c) {
  EV_BEGIN;
  /* off the ice's edge: down to the room under it */
  FADE_TO(ROOM_XOXOFAIL, 9600, 1460, DIR_DOWN);
  EV_END;
}

static int ev_poff(Ev *c) {
  int k = c->self ? c->self->arg : 0;
  EV_BEGIN;
  if (k == 8 && !flag(F_POFF_GOLD)) {
    set_flag(F_POFF_GOLD, 1);
    sv.gold = (uint16_t)min_i(9999, sv.gold + 30);
    NARRATE("* (It's a snow poff.)|* (There's 30G inside!)");
  } else {
    NARRATE("* (It's a snow poff.)");
  }
  EV_END;
}

static int ev_greaterdog(Ev *c) {
  Obj *d = obj_get(ACTOR_GREATERDOG);
  EV_BEGIN;
  /* out of a poff it jumps */
  d = obj_deco(SPR_GREATERDOG_OW, 10900, 1280, false);
  if (d) d->name = ACTOR_GREATERDOG;
  g_shake = 6;
  WAIT(20);
  BATTLE(FORM_GREATERDOG);
  set_flag(F_GREATERDOG, bt.mon[0].killed ? 2 : 1);
  set_flag(F_SNOWDIN_BOSSES, flag(F_SNOWDIN_BOSSES) + 1);
  gone(obj_get(ACTOR_GREATERDOG));
  if (c->self) gone(c->self);
  EV_END;
}

static int ev_gauntlet(Ev *c) {
  Obj *p = obj_get(ACTOR_PAPYRUS), *s = obj_get(ACTOR_SANS);
  EV_BEGIN;
  pl.dir = DIR_RIGHT;
  SAY(FACE_PAPYRUS, "`0* HUMAN!!!\n* THIS IS YOUR FINAL AND MOST DANGEROUS CHALLENGE!!!|`2* BEHOLD!\n* THE GAUNTLET OF DEADLY TERROR!");
  /* the gauntlet comes down: a spear, a spiked ball, a dog on a rope, a fire, a cannon */
  g_shake = 8;
  WAIT(40);
  SAY(FACE_PAPYRUS, "`0* ONE WORD FROM ME AND IT ALL SWINGS INTO ACTION!!!|`3* HERE IT GOES!|`4* ...|* ANY MOMENT NOW...");
  SAY(FACE_SANS, "* well? what's the holdup?");
  SAY(FACE_PAPYRUS, "`3* I'M GETTING TO IT!!!|`4* ...ACTUALLY, IT'S TOO EASY.\n* NOT FAIR AT ALL.|`2* A SKELETON HAS STANDARDS!\n* NYEH HEH HEH!!!");
  WALK(p, 12000, 1236, 6);
  gone(p);
  SAY(FACE_SANS, "* ...|* i don't know what my brother's going to do.|* if i were you, i would keep going.");
  LEAVE(s, 2, 12000, 1200);
  set_flag(F_GAUNTLET, 1);
  if (c->self) gone(c->self);
  EV_END;
}

static int ev_sans_cliff(Ev *c) {
  int k = c->self ? c->self->arg : 0;
  EV_BEGIN;
  switch (k) {
    case 1: SAY(FACE_SANS, "* hey, you could have asked\n  me for help.|`2* i know a thing or two\n  about trees."); break;
    case 2: SAY(FACE_SANS, "* his cooking's getting\n  better.|* give it a year. it might\n  even be food."); break;
    case 3:
      if (!flag(F_SNOW_SANS_TALK)) {
        set_flag(F_SNOW_SANS_TALK, 1);
        SAY(FACE_SANS, "* hey, buddy.\n* my brother's up ahead.|* remember: if you see a #Lblue#X attack, don't move.");
      } else {
        SAY(FACE_SANS, "* blue stop sign. got it?");
      }
      break;
    case 4: SAY(FACE_SANS, "* hey.\n* nice view down here, huh?"); break;
  }
  EV_END;
}

static int ev_mystery_door(Ev *c) {
  EV_BEGIN;
  NARRATE("* (It's locked.)");
  EV_END;
}

int snow2_event(Ev *c) {
  switch (c->id) {
    case EV_XOXO_SWITCH: return c->self && c->self->name == ACTOR_TRIGGER3 ? ev_tree_switch(c) : ev_xoxo_switch(c);
    case EV_SNOW_SWITCH: return ev_snow_switch(c);
    case EV_DOGI: return ev_dogi(c);
    case EV_PAP_XOXO: return ev_pap_xoxo(c);
    case EV_TILES: return ev_tiles(c);
    case EV_XOXO_FALL: return ev_xoxo_fall(c);
    case EV_POFF: return ev_poff(c);
    case EV_GREATERDOG: return ev_greaterdog(c);
    case EV_GAUNTLET: return ev_gauntlet(c);
    case EV_SNOW_SANS_CLIFF: return ev_sans_cliff(c);
    case EV_MYSTERY_DOOR: return ev_mystery_door(c);
  }
  return -1;
}
