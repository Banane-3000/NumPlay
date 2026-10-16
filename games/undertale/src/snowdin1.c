/* Snowdin Forest, from the Ruins' door to the word search: Sans and his brother Papyrus, Doggo, the ice, the
 * Snowman, the Invisible Electricity Maze, the Ball Game, the Nice Cream guy. Signs and short lines are the
 * game's (as the Undertale Wiki records them); the longer talks are kept short. */
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

/* ---------------------------------------------------------------- Sans and Papyrus */
Obj *obj_sans(int x, int y, int dir) {
  static const uint16_t s[4] = {SPR_SANS_D};
  Obj *o = obj_npc(ACTOR_SANS, s, x, y, dir, 0);
  if (o) o->flags &= (uint16_t)~OF_WALKANIM;
  return o;
}
Obj *obj_papyrus(int x, int y, int dir) {
  static const uint16_t s[4] = {SPR_PAPYRUS_D, SPR_PAPYRUS_D, SPR_PAPYRUS_L, SPR_PAPYRUS_R};
  Obj *o = obj_npc(ACTOR_PAPYRUS, s, x, y, dir, 0);
  if (o) o->anim_speed = 4;
  return o;
}
static void gone(Obj *o) { o->kind = OBJ_NONE; }

/* ---------------------------------------------------------------- the electricity maze */
/* its path (the feet's middle), from the entrance to Papyrus at the exit */
static const int16_t maze_path[][2] = {{5280, 380}, {5340, 380}, {5340, 340}, {5400, 340}, {5400, 440}, {5470, 440}};
#define MAZE_POINTS 6
/* the invisible walls: the maze's box but the path, 26 pixels wide */
static const int16_t maze_walls[][4] = {
  {5280, 270, 190, 57}, {5280, 327, 47, 40}, {5280, 393, 107, 73}, {5353, 353, 34, 40}, {5413, 327, 57, 100},
  {5387, 453, 83, 13}};
static int maze_prints;   /* how far back from the exit Papyrus's footprints go (pixels) */
static bool maze_growing; /* he's walking it, leaving them */
static int maze_len(void) {
  int n = 0;
  for (int i = 1; i < MAZE_POINTS; i++)
    n += abs_i(maze_path[i][0] - maze_path[i - 1][0]) + abs_i(maze_path[i][1] - maze_path[i - 1][1]);
  return n;
}

/* ---------------------------------------------------------------- the Ball Game */
#define BALL_X 5890
#define BALL_Y 352
#define HOLE_X 5961
#define HOLE_Y 690
static struct {
  int vx, vy;       /* 1/16 px a frame */
  int t;            /* frames since first kicked (0: not yet) */
  bool sunk;
} ball;

static void ball_place(Obj *b) {
  b->x = BALL_X, b->y = BALL_Y, b->frame = 0;
  obj_set_box(b, -9, -9, 18, 18);
  ball.vx = ball.vy = 0, ball.t = 0, ball.sunk = false;
}

static void ball_update(void) {
  Obj *b = obj_find(ACTOR_BALL);
  if (!b || ball.sunk) return;
  int r = 10 - b->frame;
  /* Frisk walking into it kicks it the way he faces */
  int fx = pl.x + 3, fy = pl.y + 20;
  if (pl.moving && fx < b->x + r + 2 && fx + 14 > b->x - r - 2 && fy < b->y + r + 2 && fy + 9 > b->y - r - 2) {
    static const int8_t dx[4] = {0, 0, -1, 1}, dy[4] = {1, -1, 0, 0};
    ball.vx = dx[pl.dir] * 4 * FX, ball.vy = dy[pl.dir] * 4 * FX;
    if (!ball.t) ball.t = 1;
  }
  if (!ball.t) return;
  ball.t++;
  /* it melts as it goes: smaller every three seconds (faster on ice), then gone, then a new one */
  bool ice = room_ice(b->x, b->y);
  if (ball.t % (ice ? 45 : 90) == 0) {
    if (b->frame >= 7) {
      ball_place(b);
      return;
    }
    b->frame++;
    r = 10 - b->frame;
    obj_set_box(b, -r + 1, -r + 1, 2 * r - 2, 2 * r - 2);
  }
  /* rolls on, slowing (less on ice); bounces off what stops it */
  for (int axis = 0; axis < 2; axis++) {
    int v = axis ? ball.vy : ball.vx;
    if (!v) continue;
    int nx = b->x + (axis ? 0 : v / FX), ny = b->y + (axis ? v / FX : 0);
    if (world_walkable(nx - r + 1, ny - r + 1, 2 * r - 2, 2 * r - 2, (int)(b - objs))) b->x = (int16_t)nx, b->y = (int16_t)ny;
    else if (axis) ball.vy = -ball.vy / 2;
    else ball.vx = -ball.vx / 2;
  }
  int fr = ice ? 1 : 3;
  ball.vx += ball.vx > 0 ? -fr : ball.vx < 0 ? fr : 0;
  ball.vy += ball.vy > 0 ? -fr : ball.vy < 0 ? fr : 0;
  if (abs_i(ball.vx) < fr) ball.vx = 0;
  if (abs_i(ball.vy) < fr) ball.vy = 0;
  /* in the hole */
  if (abs_i(b->x - HOLE_X) < 7 && abs_i(b->y - HOLE_Y) < 6 && !event_running()) {
    ball.sunk = true;
    event_start(EV_BALL_HOLE, b);
  }
}

/* ---------------------------------------------------------------- the rooms' things */
void snow1_setup(int room, int from) {
  (void)from;
  switch (room) {
    case ROOM_SNOWDIN1:
      if (!flag(F_BRANCH)) obj_trigger(1026, 380, 20, 100, EV_BRANCH, ACTOR_TRIGGER);
      else obj_sign(996, 384, 26, 30, "* (It's been smashed like it was nothing...)");
      break;
    case ROOM_SNOWDIN5: {
      Obj *l = obj_deco(SPR_LAMP, 2960, 365, true);
      if (l) {
        l->name = ACTOR_BLOCK, l->flags |= OF_TALK, l->event = EV_SIGN, l->text = "* (It's a conveniently-shaped lamp.)";
        obj_set_box(l, 2, 22, 15, 9);
      }
      obj_sign(3004, 300, 60, 66, "* (It's some sort of checkpoint or sentry-station.)|* (But there are bottles of ketchup, mustard, and relish sitting inside...)");
      if (!flag(F_SANS_MET)) obj_trigger(2572, 380, 40, 100, EV_SANS_INTRO, ACTOR_TRIGGER);
      break;
    }
    case ROOM_BOXROAD:
      obj_save(3240, 376, "* (The convenience of that lamp still fills you with determination.)");
      obj_sign(3420, 334, 20, 22, "* (This is a box.)|* (You can put an item inside or take an item out.)|* (The same box will appear later, so don't worry about coming back.)|* (Sincerely, a box lover.)");
      {
        Obj *b = obj_trigger(3444, 334, 26, 24, EV_BOX, ACTOR_ITEM);
        if (b) b->flags = OF_TALK | OF_SOLID;
      }
      break;
    case ROOM_RIVER: {
      Obj *r = obj_trigger(3428, 116, 26, 34, EV_FISHROD, ACTOR_ITEM);
      if (r) r->flags = OF_TALK;
      break;
    }
    case ROOM_PAPSENTRY:
      obj_sign(3966, 296, 52, 64, "* YOU OBSERVE THE WELL-CRAFTED SENTRY STATION.|* WHO COULD HAVE BUILT THIS, YOU PONDER...|* I BET IT WAS THAT VERY FAMOUS ROYAL GUARDSMAN!|* (NOTE: NOT YET A VERY FAMOUS ROYAL GUARDSMAN.)");
      if (!flag(F_PAP_SENTRY)) {
        obj_sans(3616, 414, DIR_RIGHT);
        obj_papyrus(3660, 414, DIR_LEFT);
        obj_trigger(3540, 370, 34, 110, EV_PAP_SENTRY, ACTOR_TRIGGER);
      }
      break;
    case ROOM_DOGGO:
      obj_sign(4204, 384, 22, 22, "* (Absolutely NO MOVING!!!)");
      obj_sign(4486, 398, 26, 16, "* (It's a bag of smoked dog treats.)");
      obj_sign(4296, 330, 88, 66, flag(F_DOGGO) == 2 ? "* (It's a sentry station. No one is inside.)" : "* (It's Doggo's sentry station.)");
      if (!flag(F_DOGGO)) obj_trigger(4296, 398, 70, 80, EV_DOGGO, ACTOR_TRIGGER);
      break;
    case ROOM_ICEFIELD:
      obj_sign(4864, 368, 24, 28, "* North: Ice\n  South: Ice\n  West: Ice|* East: Snowdin Town\n  (... and ice)");
      {
        Obj *s = obj_sans(4686, 409, DIR_DOWN);
        if (s) s->event = EV_SANS_TALK, s->arg = 1, s->flags |= OF_TALK;
      }
      break;
    case ROOM_SNOWMAN: {
      Obj *s = obj_trigger(4904, 60, 40, 44, EV_SNOWMAN, ACTOR_ITEM);
      if (s) s->flags = OF_TALK | OF_SOLID;
      break;
    }
    case ROOM_ELECMAZE:
      if (flag(F_ELECMAZE) < 2) {
        obj_papyrus(5484, 446, DIR_LEFT);
        if (!flag(F_ELECMAZE)) obj_trigger(5200, 280, 30, 200, EV_ELECMAZE, ACTOR_TRIGGER);
        else obj_trigger(5440, 420, 30, 40, EV_ELECMAZE, ACTOR_TRIGGER2);
      }
      if (flag(F_ELECMAZE) == 1)
        for (unsigned i = 0; i < sizeof maze_walls / sizeof maze_walls[0]; i++)
          obj_trigger(maze_walls[i][0], maze_walls[i][1], maze_walls[i][2], maze_walls[i][3], EV_MAZE_ZAP, ACTOR_BLOCK);
      {
        Obj *s = obj_sans(5464, 336, DIR_DOWN);
        if (s) s->event = EV_SANS_TALK, s->arg = 2, s->flags |= OF_TALK;
      }
      maze_prints = flag(F_ELECMAZE) ? maze_len() : 0;
      break;
    case ROOM_BALLGAME: {
      Obj *n = obj_trigger(5594, 280, 54, 30, EV_NICECREAM, ACTOR_ITEM);
      if (n) n->flags = OF_TALK | OF_SOLID;
      obj_sign(5682, 382, 18, 16, "* (It's a snowdecahedron.)");
      obj_sign(HOLE_X - 11, HOLE_Y - 8, 22, 16, "* Ah! Wow! Woah!\n* It's a \"#RHole#X\".");
      if (flag(F_KILLS_SNOWDIN) < 8) {
        Obj *b = obj_deco(SPR_SNOWBALL, BALL_X, BALL_Y, true);
        if (b) b->name = ACTOR_BALL, ball_place(b);
      }
      Obj *s = obj_sans(6136, 336, DIR_DOWN);
      if (s) s->event = EV_SANS_TALK, s->arg = 3, s->flags |= OF_TALK;
      break;
    }
    case ROOM_DOGI:
      obj_sign(6426, 310, 24, 24, "* SMELL DANGER RATING|* Snow Smell - Snowman\n  WHITE Rating\n  Can become #YYELLOW#X Rating|* Unsuspicious Smell - Puppy\n  #BBLUE#X Rating\n  Smell of rolling around.|* Weird Smell - Humans\n  #GGREEN#X Rating\n  Destroy at all costs!");
      break;
    case ROOM_WORDSEARCH:
      obj_sign(6434, 648, 18, 18, "* (It's a word search.)\n* (The top row says GIASFCLFUBREHBER.)");
      if (!flag(F_WORDSEARCH)) {
        obj_papyrus(6484, 652, DIR_LEFT);
        obj_trigger(6300, 600, 30, 160, EV_WORDSEARCH, ACTOR_TRIGGER);
      }
      {
        Obj *s = obj_sans(6536, 658, DIR_DOWN);
        if (s) s->event = EV_SANS_TALK, s->arg = 4, s->flags |= OF_TALK;
      }
      break;
  }
}

void snow1_update(void) {
  if (g_room == ROOM_BALLGAME) ball_update();
}

/* (while a cutscene runs too) */
static void snow1_scene_update(void) {
  if (maze_growing && maze_prints < maze_len()) maze_prints += 2;
}

void snow1_draw(void) {
  if (g_room != ROOM_ELECMAZE || !maze_prints) return;
  /* Papyrus's footprints along the maze's path: two dents every 8 pixels */
  snow1_scene_update();
  int left = maze_prints;
  for (int i = MAZE_POINTS - 1; i > 0 && left > 0; i--) {
    int x0 = maze_path[i][0], y0 = maze_path[i][1], x1 = maze_path[i - 1][0], y1 = maze_path[i - 1][1];
    int len = abs_i(x1 - x0) + abs_i(y1 - y0);
    for (int d = 0; d < len && d < left; d += 8) {
      int x = x0 + (x1 > x0 ? d : x1 < x0 ? -d : 0), y = y0 + (y1 > y0 ? d : y1 < y0 ? -d : 0);
      int ox = y1 != y0 ? 3 : 0, oy = x1 != x0 ? 3 : 0;
      gfx_rect(x - ox - 1 - cam_x, y - oy - 1 - cam_y, 3, 2, rgb565(0x55607F), 255);
      gfx_rect(x + ox - 1 - cam_x, y + oy - cam_y, 3, 2, rgb565(0x55607F), 255);
    }
    left -= len;
  }
}

/* ---------------------------------------------------------------- the events */
static int ev_branch(Ev *c) {
  EV_BEGIN;
  /* behind Frisk, the branch snaps */
  set_flag(F_BRANCH, 1);
  g_shake = 10;
  pl.frozen = true;
  WAIT(12);
  pl.dir = DIR_LEFT;
  WAIT(30);
  pl.dir = DIR_RIGHT;
  pl.frozen = false;
  if (c->self) gone(c->self);
  obj_sign(996, 384, 26, 30, "* (It's been smashed like it was nothing...)");
  EV_END;
}

static int ev_sans_intro(Ev *c) {
  Obj *s = obj_get(ACTOR_SANS), *p = obj_get(ACTOR_PAPYRUS), *lamp = obj_get(ACTOR_BLOCK);
  EV_BEGIN;
  /* on the bridge, steps behind Frisk */
  PL_WALK(2598, 392, 2);
  pl.dir = DIR_RIGHT;
  WAIT(20);
  s = obj_sans(2556, 422, DIR_RIGHT);
  if (s) s->flags |= OF_DARK;
  s = obj_get(ACTOR_SANS);
  WALK(s, 2584, 422, 1);
  NARRATE("* Human.|* Don't you know how to greet a new pal?|* Turn around and shake my hand.");
  pl.dir = DIR_LEFT;
  WAIT(30);
  g_shake = 4;
  WAIT(20);
  s->flags &= (uint16_t)~OF_DARK;
  SAY(FACE_SANS, "`1* heh. never gets old.\n`0* i'm sans. sans the skeleton.|* i'm meant to be watching for humans... but i'm not big on capturing.|* my brother papyrus, though? he's really into it.|* speak of the devil. quick, go through the gate.\n* the bars are too wide to stop anyone anyway.");
  /* through the gate, then behind the lamp */
  WALK_TO(s, 2660, 432, 2);
  PL_WALK(2720, 392, 2);
  SAY(FACE_SANS, "* quick, behind that conveniently-shaped lamp.");
  WALK_TO(s, 2920, 432, 3);
  PL_WALK(2959, 392, 3);
  PL_WALK(2959, 357, 2);
  pl.dir = DIR_DOWN;
  lamp->flags |= OF_FRONT;
  WAIT_UNTIL(!s->walking);
  s->dir = DIR_RIGHT;
  /* Papyrus comes, scolds his brother and storms off */
  p = obj_papyrus(3180, 432, DIR_LEFT);
  p = obj_get(ACTOR_PAPYRUS);
  WALK(p, 3000, 432, 3);
  SAY(FACE_SANS, "* sup, bro?");
  SAY(FACE_PAPYRUS, "`3* SANS!!! YOUR PUZZLES STILL AREN'T RECALIBRATED!\n* YOU'VE BEEN LAZING AROUND FOR DAYS!!!");
  SAY(FACE_SANS, "* staring at this lamp. it's really cool.");
  SAY(FACE_PAPYRUS, "`3* WHAT IF A HUMAN SHOWS UP!?!|`2* I'LL BE READY! I'LL CAPTURE THEM!\n* AND THEN THE ROYAL GUARD WILL HAVE TO LET ME IN!!!");
  SAY(FACE_SANS, "* hmm... maybe this lamp will help you.");
  SAY(FACE_PAPYRUS, "`3* SANS!!! YOU LAZYBONES!|* YOU GET LAZIER AND LAZIER EVERY DAY!!!");
  SAY(FACE_SANS, "* hey, take it easy. i've gotten a ton of work done today.\n`2* a skele-ton.");
  SAY(FACE_PAPYRUS, "`3* SANS!!!");
  SAY(FACE_SANS, "`1* come on. you're smiling.");
  SAY(FACE_PAPYRUS, "`3* I AM AND I HATE IT!|`3* UGH!!!\n`0* I WILL ATTEND TO MY PUZZLES...|* AS FOR YOUR WORK?\n`2* PUT A LITTLE MORE, \"BACKBONE\" INTO IT!!!!|* NYEH HEH HEH HEH HEH HEH!!!");
  WALK(p, 3180, 432, 4);
  gone(p);
  WAIT(30);
  p = obj_papyrus(3180, 432, DIR_LEFT);
  p = obj_get(ACTOR_PAPYRUS);
  WALK(p, 3140, 432, 4);
  SAY(FACE_PAPYRUS, "`2* HEH!");
  WALK(p, 3180, 432, 4);
  gone(p);
  SAY(FACE_SANS, "* ok, you can come out now.");
  lamp->flags &= (uint16_t)~OF_FRONT;
  PL_WALK(2959, 400, 2);
  pl.dir = DIR_LEFT;
  SAY(FACE_SANS, "* you should keep moving.\n`1* unless you want to hear more of my jokes.");
  SAY(FACE_SANS, "`0* oh, one favor?|* my brother's never met a human. seeing you would make his day.|* he's harmless, promise. i'll be up ahead.");
  set_flag(F_SANS_MET, 1);
  set_flag(F_SANS_FAVOR, 1);
  /* (the wrong way) */
  WALK(s, 2556, 432, 2);
  gone(s);
  if (c->self) gone(c->self);
  EV_END;
}

static int ev_sans_talk(Ev *c) {
  int k = c->self ? c->self->arg : 0;
  EV_BEGIN;
  switch (k) {
    case 1:   /* by the ice: Papyrus's special attack */
      if (!flag(F_SANS_HINT)) {
        set_flag(F_SANS_HINT, 1);
        SAY(FACE_SANS, "* a tip: my brother's got a #Bspecial attack#X.|* when it's #Lblue#X, stay still and it can't hurt you.|* think of a stop sign... but blue.");
      } else {
        SAY(FACE_SANS, "* remember... #Lblue#X stop sign.");
      }
      break;
    case 2:   /* by the maze: his brother's outfit */
      if (flag(F_ELECMAZE) < 2) SAY(FACE_SANS, "* go on. he made this one just for you.");
      else if (!flag(F_SANS_COOL)) {
        SAY(FACE_SANS, "* thanks. he's having a blast.|* that outfit? party costume. he won't take it off.\n* calls it his \"battle body.\"|* cool brother, right?@Cool@Uncool");
        set_flag(F_SANS_COOL, CHOICE() + 1);
        if (CHOICE() == 0) SAY(FACE_SANS, "`1* you'd better tell him that.");
        else SAY(FACE_SANS, "* hey, pal. sarcasm isn't funny, okay?\n* my brother's a real star.");
      } else {
        SAY(FACE_SANS, "* my brother's a real star.\n* he's the person who pushed me to get this sentry job.");
      }
      break;
    case 3:   /* by the Ball Game: fried snow, ever dearer, and then none */
      set_flag(F_FRIED_SNOW, flag(F_FRIED_SNOW) + 1);
      if (flag(F_FRIED_SNOW) == 1) SAY(FACE_SANS, "* hey. want some fried snow?\n* it's 5G.");
      else if (flag(F_FRIED_SNOW) == 2) SAY(FACE_SANS, "* fried snow? 10G.");
      else if (flag(F_FRIED_SNOW) == 3) SAY(FACE_SANS, "* fried snow? 50G.");
      else SAY(FACE_SANS, "`1* actually, i'm all out of snow.");
      break;
    case 4:   /* by the word search */
      if (!flag(F_WORDSEARCH)) SAY(FACE_SANS, "* trust me. there's no way they can get past this one.");
      else SAY(FACE_SANS, "* papyrus tried to solve yesterday's horoscope.\n`1* he says it was really tough.");
      break;
  }
  EV_END;
}

static int ev_pap_sentry(Ev *c) {
  Obj *s = obj_get(ACTOR_SANS), *p = obj_get(ACTOR_PAPYRUS);
  EV_BEGIN;
  pl.dir = DIR_RIGHT;
  SAY(FACE_PAPYRUS, "`0* SO, AS I WAS SAYING ABOUT UNDYNE...");
  SAY(FACE_SANS, "* hey, check it out.");
  /* the double take */
  for (c->v[0] = 0; c->v[0] < 6; c->v[0]++) {
    p->dir = (uint8_t)(c->v[0] % 2 ? DIR_RIGHT : DIR_LEFT);
    WAIT(c->v[0] < 4 ? 6 : 14);
  }
  p->dir = DIR_LEFT;
  SAY(FACE_PAPYRUS, "`3* SANS!! OH MY GOD!!\n* IS THAT... A HUMAN!?!?");
  SAY(FACE_SANS, "* uhhhh... actually, i think that's a rock.");
  SAY(FACE_PAPYRUS, "`4* OH.");
  SAY(FACE_SANS, "* hey, what's that in front of the rock?");
  SAY(FACE_PAPYRUS, "`3* OH MY GOD!!! (IS THAT A HUMAN?)");
  SAY(FACE_SANS, "* yes.");
  SAY(FACE_PAPYRUS, "`2* SANS!!! IT'S A HUMAN!!! I'M GOING TO BE SO POPULAR!!!|`0* HUMAN! THE GREAT PAPYRUS WILL STOP YOU AND CAPTURE YOU!|`4* AND THEN... WELL, I'LL WORK THAT OUT LATER.|`2* ONWARD, IF YOU DARE!!! NYEH HEH HEH!!!");
  WALK(p, 4140, 414, 6);
  gone(p);
  SAY(FACE_SANS, "* well, that went well.\n* don't sweat it, kid.|* i'll keep an eye out for ya.");
  WALK(s, 4140, 414, 2);
  gone(s);
  set_flag(F_PAP_SENTRY, 1);
  if (c->self) gone(c->self);
  EV_END;
}

static bool doggo_stick;   /* (snowmon.c: spared with the stick) */
void snow_doggo_stick(void) { doggo_stick = true; }

static int ev_doggo(Ev *c) {
  Obj *d = obj_get(ACTOR_DOGGO);
  EV_BEGIN;
  d = obj_deco(SPR_DOGGO_OW, 4340, 386, false);
  if (d) d->name = ACTOR_DOGGO, d->flags |= OF_FRONT;
  NARRATE("* Something moved? Or did it?|* I only see things that move.\n* And moving humans don't stay moving for long!");
  doggo_stick = false;
  BATTLE(FORM_DOGGO);
  set_flag(F_DOGGO, bt.mon[0].killed ? 2 : 1);
  if (flag(F_DOGGO) == 1) {
    if (doggo_stick) NARRATE("* A stick? Then... no stick?|* I'll need a dog treat to figure this out.");
    else NARRATE("* S-S-S-Something pet me...\n* Something that isn't m-m-moving...|* I'm gonna need some dog treats for this!!!");
  }
  gone(d);
  if (c->self) gone(c->self);
  EV_END;
}

static int ev_snowman(Ev *c) {
  EV_BEGIN;
  {
    bool has = false;
    for (int i = 0; i < NUM_ITEMS; i++) has |= sv.items[i] == ITEM_SNOWMAN_PIECE;
    c->v[0] = has;
  }
  if (c->v[0]) {
    NARRATE("* Thank you... good luck!");
  } else if (flag(F_SNOWMAN) >= 2) {
    NARRATE("* If you keep taking pieces of me, there'll be nothing left...");
  } else {
    if (!flag(F_SNOWMAN)) NARRATE("* I'm a snowman, and I'd love to travel.|* Would you carry a piece of me far away?@Yes@No");
    else NARRATE("* Did you lose my piece...?\n* Will you take another?@Yes@No");
    if (CHOICE() == 0) {
      if (item_add(ITEM_SNOWMAN_PIECE)) {
        set_flag(F_SNOWMAN, flag(F_SNOWMAN) + 1);
        NARRATE("* Thank you... good luck!\n* (You got the Snowman Piece.)");
      } else {
        NARRATE("* (You're carrying too much.)");
      }
    } else {
      NARRATE("* I see... good journey, then.");
    }
  }
  EV_END;
}

static int ev_elecmaze(Ev *c) {
  Obj *p = obj_get(ACTOR_PAPYRUS);
  EV_BEGIN;
  if (flag(F_ELECMAZE) == 1) goto crossed;
  pl.dir = DIR_RIGHT;
  SAY(FACE_PAPYRUS, "`0* HUMAN!! BEHOLD: THE INVISIBLE ELECTRICITY MAZE!!!|* TOUCH A WALL, AND THIS ORB ZAPS YOU!\n`2* NYEH HEH HEH!!!");
  /* Frisk steps in: Papyrus, holding the orb, gets the shock */
  FREE(true);
  WAIT_UNTIL(pl.x + 10 >= 5282);
  FREE(false);
  p->spr[DIR_LEFT] = SPR_PAPYRUS_ZAP, p->flags |= OF_ANIM, p->anim_speed = 3;
  g_shake = 12;
  WAIT(40);
  p->spr[DIR_LEFT] = SPR_PAPYRUS_L, p->flags &= (uint16_t)~OF_ANIM, p->frame = 0;
  SAY(FACE_PAPYRUS, "`3* SANS!!! WHAT HAPPENED???");
  SAY(FACE_SANS, "* i think you have to hold the orb.");
  SAY(FACE_PAPYRUS, "`4* OH, OKAY.");
  /* he walks the maze to Frisk (his footprints show the way), gives the orb, and walks back */
  maze_growing = true;
  for (c->v[0] = MAZE_POINTS - 2; c->v[0] >= 0; c->v[0]--) WALK(p, maze_path[c->v[0]][0], maze_path[c->v[0]][1] + 6, 2);
  maze_growing = false;
  maze_prints = maze_len();
  p->dir = DIR_LEFT;
  SAY(FACE_PAPYRUS, "`0* HOLD THIS PLEASE.");
  NARRATE("* (You got the orb.)");
  for (c->v[0] = 1; c->v[0] < MAZE_POINTS; c->v[0]++) WALK(p, maze_path[c->v[0]][0] + (c->v[0] == MAZE_POINTS - 1 ? 14 : 0), maze_path[c->v[0]][1] + 6, 3);
  p->dir = DIR_LEFT;
  SAY(FACE_PAPYRUS, "`2* OKAY! TRY IT NOW!!!");
  set_flag(F_ELECMAZE, 1);
  if (c->self) gone(c->self);
  for (unsigned i = 0; i < sizeof maze_walls / sizeof maze_walls[0]; i++)
    obj_trigger(maze_walls[i][0], maze_walls[i][1], maze_walls[i][2], maze_walls[i][3], EV_MAZE_ZAP, ACTOR_BLOCK);
  obj_trigger(5440, 420, 30, 40, EV_ELECMAZE, ACTOR_TRIGGER2);
  return 1;
crossed:
  SAY(FACE_PAPYRUS, "`2* YOU SOLVED IT! FAR TOO EASILY...|`0* THE NEXT ONE WON'T BE SO KIND!!!\n`2* NYEH HEH HEH!!!");
  WALK(p, 5520, 446, 5);
  gone(p);
  set_flag(F_ELECMAZE, 2);
  for (int i = 0; i < MAX_OBJS; i++)
    if (objs[i].event == EV_MAZE_ZAP || objs[i].event == EV_ELECMAZE) objs[i].kind = OBJ_NONE;
  EV_END;
}

static int ev_maze_zap(Ev *c) {
  EV_BEGIN;
  /* the orb shocks Frisk (no harm) and he's back at the start */
  g_shake = 10;
  pl.frozen = true;
  WAIT(20);
  pl.x = 5258, pl.y = 356, pl.dir = DIR_RIGHT;
  pl.frozen = false;
  EV_END;
}

static int ev_nicecream(Ev *c) {
  EV_BEGIN;
  if (!flag(F_NICECREAM)) {
    set_flag(F_NICECREAM, 1);
    NARRATE("* Nobody wants something cold today. Odd...|* OH!!!! A CUSTOMER!!!");
    NARRATE("* Hello! Would you like some Nice Cream?|* It's the frozen treat that warms your heart!\n* Now just 15G!@Yes@No");
  } else {
    NARRATE("* Nice Cream! It's the frozen treat that warms your heart!\n* Now just 15G!@Yes@No");
  }
  if (CHOICE() == 1) {
    NARRATE("* Well then... Tell your friends...|* There's ice cream... Out in the middle of the woods...");
  } else if (sv.gold < 15) {
    NARRATE("* Huh? You don't have enough money...|* I wish I could make Nice Cream for free...");
  } else if (!item_add(ITEM_NICE_CREAM)) {
    NARRATE("* It looks like you're holding too much stuff! Oh well!");
  } else {
    sv.gold = (uint16_t)(sv.gold - 15);
    NARRATE("* Here you go!\n* Have a super-duper day!\n* (You got the Nice Cream.)");
  }
  EV_END;
}

static char ball_text[260];
static int ev_ball_hole(Ev *c) {
  Obj *b = c->self;
  EV_BEGIN;
  if (b) b->flags |= OF_HIDDEN;
  {
    /* the flag that pops up: by how long the ball took, or how small it was */
    int secs = ball.t / 30, g;
    uint16_t spr;
    const char *t;
    if (secs < 6 && flag(F_BALLGAME) != 7) {
      spr = SPR_FLAG_RED, g = 50, t = "* Bravery. Justice. Integrity. Kindness. Perseverance. Patience.|* Using these, you were able to win at \"Ball Game.\"";
    } else if (secs < 6) {
      spr = SPR_FLAG_RED, g = 10, t = "* #RRED#X - Try as you might, you continue to be yourself.";
    } else if (b && b->frame >= 5) {
      spr = SPR_FLAG_LIGHTBLUE, g = 4, t = "* #LLIGHT BLUE#X - \"Ball\" is \"Small.\"|* You waited, still, for this opportunity...\n* ... then dethroned \"Ball\" with a sharp attack.";
    } else if (secs < 8) {
      spr = SPR_FLAG_ORANGE, g = 5, t = "* #OORANGE#X - You are the kind of person who rushes fists-first through all obstacles.";
    } else if (secs < 11) {
      spr = SPR_FLAG_YELLOW, g = 3, t = "* #YYELLOW#X - Your sure-fire accuracy put an end to the mayhem of \"Ball.\"";
    } else if (secs < 15) {
      spr = SPR_FLAG_BLUE, g = 2, t = "* #BBLUE#X - Hopping and twirling, your original style pulled you through.";
    } else if (secs < 20) {
      spr = SPR_FLAG_GREEN, g = 1, t = "* #GGREEN#X - Your concern and care for \"Ball\" led you to a delicious victory.";
    } else {
      spr = SPR_FLAG_PURPLE, g = 2, t = "* #PPURPLE#X - Even when you felt trapped, you took notes and achieved the end of \"Ball.\"";
    }
    if (spr == SPR_FLAG_RED) set_flag(F_BALLGAME, 7);
    else if (!flag(F_BALLGAME)) set_flag(F_BALLGAME, 1);
    sv.gold = (uint16_t)min_i(9999, sv.gold + g);
    snprintf(ball_text, sizeof ball_text, "%s|* (You won %dG.)", t, g);
    Obj *f = obj_deco(spr, HOLE_X, HOLE_Y + 4, false);
    if (f) f->name = ACTOR_DOG;
  }
  NARRATE(ball_text);
  WAIT(30);
  gone(obj_get(ACTOR_DOG));
  if (b) b->flags &= (uint16_t)~OF_HIDDEN, ball_place(b);
  EV_END;
}

static int ev_wordsearch(Ev *c) {
  Obj *p = obj_get(ACTOR_PAPYRUS);
  EV_BEGIN;
  pl.dir = DIR_RIGHT;
  SAY(FACE_PAPYRUS, "`0* HUMAN!!!\n* THE NEXT PUZZLE WILL NOT BE EASY!\n* IT IS DESIGNED BY MY BROTHER, SANS!");
  SAY(FACE_SANS, "* it's right there. on the ground.\n* trust me. there's no way they can get past this one.");
  FREE(true);
  WAIT_UNTIL(pl.x >= 6440);
  FREE(false);
  pl.dir = DIR_RIGHT;
  SAY(FACE_PAPYRUS, "`3* SANS!!! THE PUZZLE DIDN'T DO ANYTHING!!!");
  SAY(FACE_SANS, "* whoops. i knew i should have used today's crossword instead.");
  SAY(FACE_PAPYRUS, "`3* CROSSWORD!?\n* IN MY OPINION, JUNIOR JUMBLE IS EASILY THE HARDEST.");
  SAY(FACE_SANS, "* what? really, dude? that easy-peasy word scramble?\n* that's for baby bones.");
  SAY(FACE_PAPYRUS, "`3* UNBELIEVABLE!!!\n* HUMAN!! SOLVE THIS DISPUTE!@Crossword@Junior Jumble");
  set_flag(F_WORDSEARCH, CHOICE() + 1);
  if (CHOICE() == 0) SAY(FACE_PAPYRUS, "`3* HUMANS... YOU'RE ALL THE SAME!!!");
  else SAY(FACE_PAPYRUS, "`2* HA! THE HUMAN AGREES WITH ME!!!");
  SAY(FACE_PAPYRUS, "`2* NYEH HEH HEH HEH!!!");
  WALK(p, 6640, 652, 6);
  gone(p);
  if (c->self) gone(c->self);
  EV_END;
}

static int ev_fishrod(Ev *c) {
  EV_BEGIN;
  NARRATE("* (It's a fishing rod affixed to the ground.)\n* (Reel it in?)@Yes@No");
  if (CHOICE() == 0) {
    set_flag(F_FISH, 1);
    NARRATE("* (You reeled in the line...)\n* (There's a note on the hook.)|* \"Call me! Here's my number!\"");
  }
  EV_END;
}

int box_menu(Ev *c);   /* shop.c: the Dimensional Box */

int snow1_event(Ev *c) {
  switch (c->id) {
    case EV_BRANCH: return ev_branch(c);
    case EV_SANS_INTRO: return ev_sans_intro(c);
    case EV_SANS_TALK: return ev_sans_talk(c);
    case EV_PAP_SENTRY: return ev_pap_sentry(c);
    case EV_DOGGO: return ev_doggo(c);
    case EV_SNOWMAN: return ev_snowman(c);
    case EV_ELECMAZE: return ev_elecmaze(c);
    case EV_MAZE_ZAP: return ev_maze_zap(c);
    case EV_NICECREAM: return ev_nicecream(c);
    case EV_BALL_HOLE: return ev_ball_hole(c);
    case EV_WORDSEARCH: return ev_wordsearch(c);
    case EV_FISHROD: return ev_fishrod(c);
    case EV_BOX: return box_menu(c);
  }
  return -1;
}
