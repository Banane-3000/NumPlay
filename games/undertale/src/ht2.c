/* Hotland, its second floor: out of the elevator, Sans's hot dog stand and Heats Flamesman; the crossroads (the
 * invisible path up to the Art Club's room, the Stained Apron down south); the conveyor belt puzzle and the vent
 * puzzle; the Bad Opinion Zone; the Royal Guards at the bend; Mettaton's news report and its six bombs; the CORE
 * again, and the elevator up to the third floor. (As the Undertale Wiki describes them; what they say is ours, in short.) */
#include <stddef.h>
#include <stdio.h>
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
#include "world.h"

static void gone(Obj *o) { o->kind = OBJ_NONE; }
static Obj *thing(int x, int y, int w, int h, int event, int name, int arg) {
  Obj *o = obj_trigger(x, y, w, h, event, name);
  if (o) o->flags = OF_TALK | OF_SOLID, o->arg = (int16_t)arg;
  return o;
}
static Obj *call_at(int x, int y, int w, int h, int arg) {
  Obj *t = obj_trigger(x, y, w, h, EV_HT2_CALL, ACTOR_TRIGGER);
  if (t) t->arg = (int16_t)arg;
  return t;
}
/* a force field: a wall of light across the way */
static void field(int x, int y, int w, int h) {
  Obj *o = obj_trigger(x, y, w, h, EV_NONE, ACTOR_FIELD);
  if (o) o->flags = OF_SOLID;
}
static void fields_down(void) {
  for (Obj *o = obj_find(ACTOR_FIELD); o; o = obj_find(ACTOR_FIELD)) gone(o);
}
static void draw_fields(void) {
  for (int i = 0; i < MAX_OBJS; i++) {
    const Obj *o = &objs[i];
    if (o->kind == OBJ_NONE || o->name != ACTOR_FIELD) continue;
    uint16_t c = (g_frame / 3) % 2 ? rgb565(0x30FF60) : rgb565(0x10A040);
    gfx_rect(o->x - cam_x, o->y - cam_y, o->bw, o->bh, c, 200);
  }
}

/* ---------------------------------------------------------------- the conveyor belt puzzle */
/* three switches over the top belt (it runs left): pressed right to left, each soon after the last */
#define SW_Y 2322
#define SW_WINDOW 45
static const int16_t sw_x[3] = {5200, 5285, 5370};
static int sw_lit, sw_t;   /* lit: a bit each; t: frames since the last press */

/* ---------------------------------------------------------------- the vent puzzle */
/* eight platforms, each with its vents (the ways they blow, by DIR_*); landing on a panel turns every vent the other
 * way, landing on the top one turns them back; blown off where there's no platform, Frisk lands on the belts
 * below, or back at the start */
typedef struct {
  int16_t x, y;
  uint8_t vents, panel;   /* panel: 1 turns the vents, 2 turns them back */
} Plat;
#define VB(d) (1 << (d))
static const Plat plats[] = {
  {4620, 2160, VB(DIR_DOWN), 2},                       /* the top one */
  {4520, 2260, VB(DIR_RIGHT), 1},                      /* turned, it blows left: the way out */
  {4620, 2260, VB(DIR_UP) | VB(DIR_RIGHT), 1},
  {4720, 2260, VB(DIR_LEFT), 1},                       /* the start */
  {4520, 2360, 15, 1},
  {4620, 2360, VB(DIR_UP) | VB(DIR_DOWN), 1},
  {4520, 2460, 15, 0},
  {4620, 2460, 15, 0},
};
#define NPLATS ((int)(sizeof plats / sizeof plats[0]))
#define VP_START_X 4740
#define VP_START_Y 2340
static const int8_t ddx[4] = {0, 0, -1, 1}, ddy[4] = {1, -1, 0, 0};
static bool vp_turned;

/* ---------------------------------------------------------------- Mettaton's news report */
enum { NEWS_DARK, NEWS_PICK, NEWS_TIMER, NEWS_OVER };
#define NEWS_TIME (120 * 30)
static int news, news_t, news_why;
static uint8_t defused;
/* where each bomb's feet are, t frames into the show (the dog, the game and the present stay put: the map shows
 * them) */
static void bomb_at(int k, int t, int *x, int *y) {
  switch (k) {
    case BOMB_DOG: *x = 4015, *y = 1730; break;
    case BOMB_GAME: *x = 3918, *y = 1978; break;
    case BOMB_PRESENT: *x = 4366, *y = 1588; break;
    case BOMB_SCRIPT: *x = 4332, *y = 1946 - t % 150; break;   /* riding the belt up */
    case BOMB_BALL: {   /* bouncing along the top room */
      int p = t % 300, b = gfx_sin(t * 256 / 30 % 256);
      *x = 3950 + 250 * (p < 150 ? p : 300 - p) / 150, *y = 1604 - 18 * (b < 0 ? -b : b) / 1024;
      break;
    }
    default:   /* the glass of water darts every way */
      *x = 4070 + 130 * gfx_sin(t * 3 % 256) / 1024, *y = 1626 + 24 * gfx_sin((t * 5 + 64) % 256) / 1024;
      break;
  }
}
static bool bomb_moves(int k) { return k == BOMB_SCRIPT || k == BOMB_BALL || k == BOMB_WATER; }
static uint16_t bomb_spr(int k) {
  if (k == BOMB_SCRIPT) return news >= NEWS_TIMER ? SPR_BOMB_SCRIPT : SPR_NEWS_SCRIPT;
  return k == BOMB_BALL ? SPR_BOMB_BALL : SPR_BOMB_WATER;
}
static int news_frame;
static void bombs_setup(void) {
  for (int k = 0; k < BOMB_COUNT; k++) {
    int x, y;
    bomb_at(k, news_frame, &x, &y);
    Obj *o;
    if (bomb_moves(k)) {
      o = obj_deco(bomb_spr(k), x, y, false);
      if (o) o->name = ACTOR_BOMB, obj_set_box(o, -10, -16, 20, 16), o->event = EV_BOMB;
    } else {
      /* (the dog lies along the wall: its box reaches down to where Frisk can stand) */
      o = obj_trigger(x - 12, y - 18, 24, k == BOMB_DOG ? 26 : 18, EV_BOMB, ACTOR_BOMB);
    }
    if (!o) continue;
    o->arg = (int16_t)k;
    /* before the show: things to report on; during it, bombs to touch; defused, nothing */
    o->flags = (uint16_t)((o->flags & ~(OF_TOUCH | OF_TALK | OF_SOLID)) |
                          (defused & (1 << k) || news == NEWS_OVER || flag(F_HT_NEWS) ? 0
                           : news == NEWS_TIMER ? OF_TOUCH : OF_TALK | (bomb_moves(k) ? 0 : OF_SOLID)));
  }
}
static void bombs_arm(void) {
  for (int i = 0; i < MAX_OBJS; i++) {
    Obj *o = &objs[i];
    if (o->kind != OBJ_NONE && o->name == ACTOR_BOMB) o->flags = (uint16_t)((o->flags & ~(OF_TALK | OF_SOLID)) | OF_TOUCH);
  }
}
static void bombs_move(void) {
  if (news == NEWS_DARK) return;
  news_frame++;
  for (int i = 0; i < MAX_OBJS; i++) {
    Obj *o = &objs[i];
    if (o->kind == OBJ_NONE || o->name != ACTOR_BOMB || !bomb_moves(o->arg) || (defused & (1 << o->arg))) continue;
    int x, y;
    bomb_at(o->arg, news_frame, &x, &y);
    o->x = (int16_t)x, o->y = (int16_t)y, o->spr[0] = bomb_spr(o->arg);
  }
}
static void news_fields(void) {
  field(3796, 1756, 6, 48);    /* back west */
  field(4580, 1700, 64, 6);    /* on, up north */
}

/* ---------------------------------------------------------------- drawn, and watched */
void ht2_draw(void) {
  if (g_room < ROOM_HT_HOTDOG || g_room > ROOM_HT_COREVIEW2) return;
  draw_fields();
  if (g_room == ROOM_HT_BELTPUZ)
    for (int i = 0; i < 3; i++) {
      bool on = flag(F_HT_BELT) || (sw_lit >> i & 1);
      gfx_rect(sw_x[i] - 4 - cam_x, SW_Y + 4 - cam_y, 8, 6, on ? rgb565(0x40FF40) : rgb565(0x802020), 255);
    }
  if (g_room == ROOM_HT_VENTPUZ) {
    static const uint16_t arrow[4] = {SPR_VENT_D, SPR_VENT_U, SPR_VENT_L, SPR_VENT_R};
    for (int p = 0; p < NPLATS; p++)
      for (int g = 0; g < 4; g++)
        if (plats[p].vents & VB(g))
          gfx_sprite(arrow[g ^ vp_turned], 0, plats[p].x + 20 + ddx[g] * 20 - cam_x, plats[p].y + 20 + ddy[g] * 20 - cam_y, 0, 255);
  }
}

/* over everything: the news report's clock */
void ht2_draw_top(void) {
  if (g_room != ROOM_HT_NEWS || news != NEWS_TIMER) return;
  char s[16];
  int secs = (news_t + 29) / 30;
  snprintf(s, sizeof s, "%d:%02d", secs / 60, secs % 60);
  gfx_rect(4, 4, 52, 18, C_BLACK, 200);
  gfx_text(FONT_MAIN, 10, 6, s, secs <= 20 ? C_RED : C_WHITE, 255);
}

void ht2_update(void) {
  if (g_room < ROOM_HT_HOTDOG || g_room > ROOM_HT_COREVIEW2) return;
  if (sw_t < 1000) sw_t++;
  if (sw_lit && sw_t > SW_WINDOW && !flag(F_HT_BELT)) sw_lit = 0;
  if (g_room == ROOM_HT_NEWS) {
    bombs_move();
    if (news == NEWS_TIMER && !event_running()) {
      if (defused == (1 << BOMB_COUNT) - 1) news_why = 1, event_start(EV_NEWS, NULL);
      else if (--news_t <= 0) news_why = 2, event_start(EV_NEWS, NULL);
    }
  }
}

/* ---------------------------------------------------------------- the rooms */
static Obj *obj_rg(int name, int x, int y) {
  static const uint16_t s1[4] = {SPR_RG01_OW, SPR_RG01_OW, SPR_RG01_OW, SPR_RG01_OW};
  static const uint16_t s2[4] = {SPR_RG02_OW, SPR_RG02_OW, SPR_RG02_OW, SPR_RG02_OW};
  Obj *o = obj_npc(name, name == ACTOR_RG01 ? s1 : s2, x, y, DIR_RIGHT, 0);
  if (o) o->flags &= (uint16_t)~OF_SOLID;
  return o;
}

void ht2_setup(int room, int from) {
  if (room < ROOM_HT_HOTDOG || room > ROOM_HT_COREVIEW2) return;
  switch (room) {
    case ROOM_HT_HOTDOG:
      thing(6585, 2346, 58, 36, EV_HOTDOG, ACTOR_ITEM, 0);
      thing(7056, 2420, 28, 30, EV_HEATS, ACTOR_ITEM, 0);
      thing(6940, 2330, 80, 64, EV_HT2_ELEVATOR, ACTOR_DOOR, 0);
      obj_sign(6696, 2364, 30, 30, "* Snow on a hot dog stand?\n* Here, in Hotland?|* Guess somebody's too lazy\n  to clean it off.");
      obj_sign(6618, 2436, 28, 30, "* TOASTY BUN!");
      break;
    case ROOM_HT_ARTCLUB:
      obj_sign(5896, 2146, 30, 30, "* (ART CLUB ROOM.)\n* (It meets on one night a year,\n  at one hour only.)");
      break;
    case ROOM_HT_APRON:
      if (!flag(F_HT_APRON)) {
        Obj *p = obj_deco(SPR_APRON, 5836, 2618, false);
        if (p) p->name = ACTOR_ITEM;
        Obj *t = obj_trigger(5832, 2614, 30, 26, EV_APRON, ACTOR_ITEM);
        if (t) t->flags = OF_TALK;
      }
      break;
    case ROOM_HT_BELTPUZ:
      if (!flag(F_HT_CALL_BELT)) call_at(5640, 2370, 12, 70, 0);
      if (!flag(F_HT_BELT)) field(5066, 2370, 6, 70);
      for (int i = 0; i < 3; i++) {
        Obj *s = obj_trigger(sw_x[i] - 12, SW_Y, 24, 22, EV_BELT_SWITCH, ACTOR_ITEM);
        if (s) s->flags = OF_TALK, s->arg = (int16_t)i;
      }
      sw_lit = 0;
      break;
    case ROOM_HT_VENTPUZ:
      for (int p = 0; p < NPLATS; p++)
        for (int g = 0; g < 4; g++)
          if (plats[p].vents & VB(g)) {
            Obj *o = obj_trigger(plats[p].x + 24 + ddx[g] * 20, plats[p].y + 24 + ddy[g] * 20, 12, 12, EV_VENTPUZ, ACTOR_VENT);
            if (o) o->arg = (int16_t)(p * 4 + g);
          }
      break;
    case ROOM_HT_BOZ:
      obj_save(4005, 2252, "* (The smell of melted cheese fills you with determination.)");
      obj_sign(3918, 2240, 36, 32, "* (A safe on a little table.)\n* (The cheese inside has melted\n  onto the table.)");
      obj_sign(3958, 2246, 16, 20, "* (A mouse hole.)\n* (Someone wants that cheese.)");
      if (!flag(F_HT_CALL_BOZ)) call_at(4300, 2250, 12, 80, 2);
      break;
    case ROOM_HT_GUARDS:
      if (!flag(F_HT_GUARDS)) obj_trigger(3560, 2240, 10, 100, EV_GUARDS, ACTOR_TRIGGER);
      break;
    case ROOM_HT_NEWS:
      if (!flag(F_HT_NEWS)) {
        /* (come in from elsewhere mid-show: only a SAVE loaded, as its exits are shut; it starts over) */
        if (from != ROOM_HT_NEWS) news = NEWS_DARK;
        if (news == NEWS_DARK) {
          defused = 0, news_frame = 0;
          obj_trigger(3880, 1750, 10, 60, EV_NEWS, ACTOR_TRIGGER);
        } else news_fields();
        Obj *m = obj_deco(SPR_MTT_NEWS, 4186, 1781, true);
        if (m) m->name = ACTOR_METTATON, m->flags |= OF_ANIM, m->anim_speed = 8, obj_set_box(m, -16, -10, 32, 10);
      }
      bombs_setup();
      break;
    case ROOM_HT_COREVIEW2:
      if (!flag(F_HT_CALL_CORE2)) call_at(4596, 1420, 48, 10, 3);
      thing(3740, 1300, 80, 60, EV_HT2_ELEVATOR, ACTOR_DOOR, 1);
      break;
  }
}

/* ---------------------------------------------------------------- the events */
static int ev_hotdog(Ev *c) {
  EV_BEGIN;
  SAY(FACE_SANS, "* hey. hot dogs, 30G apiece.\n* want one?@Buy@No thanks");
  if (CHOICE() == 0) {
    if (sv.gold < 30) {
      SAY(FACE_SANS, "* you're a little short, kid.");
    } else if (!item_add(ITEM_HOT_DOG)) {
      SAY(FACE_SANS, "* hands full, huh?\n* come back when they aren't.");
    } else {
      sv.gold = (uint16_t)(sv.gold - 30);
      SAY(FACE_SANS, "* here you go. enjoy.");
    }
  }
  EV_END;
}

static int ev_heats(Ev *c) {
  EV_BEGIN;
  if (!flag(F_HT_HEATS)) {
    set_flag(F_HT_HEATS, 1);
    NARRATE("* I'M HEATS FLAMESMAN!!!\n* REMEMBER MY NAME!!!");
  } else {
    NARRATE("* DON'T YOU FORGET IT!!!\n* HEATS FLAMESMAN!!!");
  }
  EV_END;
}

static int ev_apron(Ev *c) {
  EV_BEGIN;
  if (!item_add(ITEM_STAINED_APRON)) {
    NARRATE("* (You're carrying too much.)");
    return 1;
  }
  set_flag(F_HT_APRON, 1);
  NARRATE("* (You got the Stained Apron.)");
  gone(obj_get(ACTOR_ITEM));
  if (c->self) gone(c->self);
  EV_END;
}

static int ev_belt_switch(Ev *c) {
  int next = !sw_lit ? 2 : sw_lit == 4 ? 1 : 0;
  EV_BEGIN;
  c->v[0] = c->self ? c->self->arg : 0;
  if (flag(F_HT_BELT)) return 1;
  /* right to left: the first restarts it, one out of turn spoils it */
  if (c->v[0] == next) sw_lit |= 1 << c->v[0];
  else sw_lit = c->v[0] == 2 ? 4 : 0;
  sw_t = 0;
  if (sw_lit != 6) return 1;
  /* two of three: Alphys calls to cheer Frisk on, just as the last one goes by */
  SAY(FACE_ALPHYS, "* Ring...|* Now! Hit the last one now!|* ...|* Oh no. I made you miss it.|* Um! Actually, only the\n  first two mattered!\n* Really!|* I'll just turn off the\n  field.");
  set_flag(F_HT_BELT, 1);
  fields_down();
  NARRATE("* (The force field switches off.)");
  EV_END;
}

static int ev_ventpuz(Ev *c) {
  EV_BEGIN;
  c->v[0] = c->self ? c->self->arg / 4 : 0;
  c->v[1] = (c->self ? c->self->arg % 4 : 0) ^ vp_turned;
  c->v[2] = -1;
  for (int q = 0; q < NPLATS; q++)
    if (plats[q].x == plats[c->v[0]].x + ddx[c->v[1]] * 100 && plats[q].y == plats[c->v[0]].y + ddy[c->v[1]] * 100) c->v[2] = q;
  c->v[3] = plats[c->v[0]].x + 20 + ddx[c->v[1]] * 100, c->v[4] = plats[c->v[0]].y + 6 + ddy[c->v[1]] * 100;
  pl.dir = (uint8_t)c->v[1];
  if (c->v[2] < 0 && !world_walkable(c->v[3] + 3, c->v[4] + 20, 14, 9, -1)) {
    /* nowhere to land: off into the haze, and back at the start */
    PL_WALK(plats[c->v[0]].x + 20 + ddx[c->v[1]] * 40, plats[c->v[0]].y + 6 + ddy[c->v[1]] * 40, 7);
    vp_turned = false;
    FADE_TO(ROOM_HT_VENTPUZ, VP_START_X, VP_START_Y, DIR_UP);
    return 1;
  }
  PL_WALK(c->v[3], c->v[4], 7);
  if (c->v[2] >= 0) {
    if (plats[c->v[2]].panel == 2) vp_turned = false;
    else if (plats[c->v[2]].panel) vp_turned = !vp_turned;
  } else if (c->v[3] < plats[1].x) {
    set_flag(F_HT_VENTPUZ, 1);   /* (out, west) */
  }
  EV_END;
}

/* Alphys's calls: 0 the belt puzzle ahead, 2 in the Bad Opinion Zone, 3 by the CORE again */
static int ev_call(Ev *c) {
  EV_BEGIN;
  c->v[0] = c->self ? c->self->arg : 0;
  if (c->self) gone(c->self);
  switch (c->v[0]) {
    case 0:
      set_flag(F_HT_CALL_BELT, 1);
      SAY(FACE_ALPHYS, "* Ring...|* It's Alphys! There's a belt\n  puzzle up ahead.|* Ride the top belt, and hit\n  all three switches, one\n  after another.|* Don't worry, I'll help you\n  with the timing!");
      break;
    case 2:
      set_flag(F_HT_CALL_BOZ, 1);
      SAY(FACE_ALPHYS, "* Ring...|* Oh! H-hi. Sorry, I'm in\n  a bad mood.|* The sequel to my favorite\n  anime came out, and... ugh.|* They got EVERYTHING wrong.\n* Don't get me started.|* ...|* I already started, huh.\n* Sorry! Bye!");
      break;
    case 3:
      set_flag(F_HT_CALL_CORE2, 1);
      SAY(FACE_ALPHYS, "* Ring...|* Phew! You made it through\n  the news.|* Mettaton's out of control.|* The elevator's just ahead.\n* Keep going!");
      break;
  }
  EV_END;
}

static int ev_guards(Ev *c) {
  Obj *a = obj_get(ACTOR_RG01), *b = obj_get(ACTOR_RG02);
  EV_BEGIN;
  if (c->self) gone(c->self);
  /* two of the Royal Guard, from round the bend */
  a = obj_rg(ACTOR_RG01, 3300, pl.y + 29);
  b = obj_rg(ACTOR_RG02, 3250, pl.y + 29);
  WALK_TO(b, pl.x - 70, pl.y + 29, 3);
  WALK(a, pl.x - 30, pl.y + 29, 3);
  NARRATE("* Hey! You! Hold up!|* So, like, we got a tip about a\n  human in a striped shirt.\n* Around here, right now.|* Scary, huh? But stay chill.\n* We'll get you somewhere safe.");
  NARRATE("* ...|* Huh? What is it, bro?|* Their shirt? What about it?|* ...|* Oh. OH.|* Dude, this is, like, SO\n  awkward.|* We totally have to fight\n  you now.");
  BATTLE(FORM_RGUARDS);
  a = obj_get(ACTOR_RG01), b = obj_get(ACTOR_RG02);
  if (bt.mon[0].killed || bt.mon[1].killed) {
    set_flag(F_HT_GUARDS, 2);
    if (bt.mon[0].killed) gone(a);
    if (bt.mon[1].killed) gone(b);
    return 1;
  }
  set_flag(F_HT_GUARDS, 1);
  NARRATE("* So, uh... we're gonna go get\n  some ice cream now.|* Later, dude!");
  WALK_TO(b, 3230, b->y, 3);
  WALK(a, 3230, a->y, 3);
  gone(obj_get(ACTOR_RG01));
  gone(obj_get(ACTOR_RG02));
  EV_END;
}

static int ev_news(Ev *c) {
  Obj *m = obj_get(ACTOR_METTATON);
  EV_BEGIN;
  if (c->self) gone(c->self);
  c->v[0] = news_why;
  if (c->v[0] == 0) {
    /* the lights, and the show */
    NARRATE("* (It's pitch dark in here.)");
    SAY(FACE_ALPHYS, "* Ring...|* It's dark in there, huh?\n* Hang on, I'll get the\n  lights...");
    NARRATE("* (The lights come on.)");
    NARRATE("* OHHH YES! GOOD EVENING, AND\n  WELCOME TO... MTT NEWS!|* I'M YOUR ANCHOR, METTATON.|* TONIGHT, OUR BRAVE REPORTER\n  FINDS THE STORY OF THE YEAR!|* GO ON, DARLING! FIND SOMETHING\n  NEWSWORTHY TO REPORT ON!");
    news_fields();
    news = NEWS_PICK;
    return 1;
  }
  /* the end: every bomb defused, or out of time */
  if (c->v[0] == 1) {
    NARRATE("* WHAT?! YOU DEFUSED THEM ALL?!|* ...NO MATTER! THE BIG ONE GOES\n  OFF IN TWO SECONDS!|* ONE... TWO...");
    SAY(FACE_ALPHYS, "* Ring...|* Got it! I hacked into it!\n* It's off!");
  } else {
    NARRATE("* AND THAT'S TIME!!!|* ...|* ...HM? IT ISN'T GOING OFF.");
    SAY(FACE_ALPHYS, "* Ring...|* Phew! I got into the big\n  one. Just in time!");
  }
  NARRATE("* ALPHYS HACKED MY BOMB?!|* UGH! THAT'S ALL FOR TONIGHT,\n  FOLKS!");
  m = obj_get(ACTOR_METTATON);
  WALK(m, m->x, m->y - 200, 6);
  gone(obj_get(ACTOR_METTATON));
  fields_down();
  news = NEWS_OVER;
  set_flag(F_HT_NEWS, c->v[0]);
  for (int i = 0; i < MAX_OBJS; i++)
    if (objs[i].kind != OBJ_NONE && objs[i].name == ACTOR_BOMB) objs[i].flags &= (uint16_t)~(OF_TOUCH | OF_TALK | OF_SOLID);
  EV_END;
}

static const char *const reports[BOMB_COUNT] = {
  [BOMB_DOG] = "* A DOG! HOW... ORDINARY.",
  [BOMB_WATER] = "* A GLASS OF WATER!\n* IT'S... VERY QUICK.",
  [BOMB_SCRIPT] = "* THE SCRIPT OF MY NEXT FILM!\n* A FINE CHOICE, DARLING.",
  [BOMB_BALL] = "* A BASKETBALL! NO, WAIT, A\n  FASHION STATEMENT!",
  [BOMB_PRESENT] = "* A PRESENT! FOR ME?\n* YOU SHOULDN'T HAVE!",
  [BOMB_GAME] = "* A VIDEO GAME! I'M IN IT, YOU\n  KNOW. NEAR THE END.",
};
static int ev_bomb(Ev *c) {
  EV_BEGIN;
  c->v[0] = c->self ? c->self->arg : 0;
  if (news == NEWS_PICK) {
    /* the story: it's a bomb, and so is everything else */
    NARRATE(reports[c->v[0]]);
    NARRATE("* ...BUT WAIT!!!|* IT'S A BOMB!!!|* IN FACT, EVERYTHING IN HERE\n  IS A BOMB! SURPRISE!|* AND THE BIG ONE UP NORTH GOES\n  OFF IN TWO MINUTES!|* STAY TUNED, VIEWERS!\n* THIS IS GOING TO BE EXPLOSIVE!");
    SAY(FACE_ALPHYS, "* Ring...|* D-don't panic! I put a bomb\n  defuser in your phone!|* Touch a bomb, then defuse\n  it while it's in the\n  DEFUSE ZONE!");
    news = NEWS_TIMER, news_t = NEWS_TIME;
    bombs_arm();
    return 1;
  }
  if (news != NEWS_TIMER || (defused >> c->v[0] & 1)) return 1;
  g_bomb_kind = c->v[0];
  BATTLE(FORM_BOMB);
  if (battle_result() == BR_SPARED) {
    defused = (uint8_t)(defused | 1 << c->v[0]);
    for (int i = 0; i < MAX_OBJS; i++)
      if (objs[i].kind != OBJ_NONE && objs[i].name == ACTOR_BOMB && objs[i].arg == c->v[0]) objs[i].flags &= (uint16_t)~OF_TOUCH;
  }
  EV_END;
}

static int ev_elevator(Ev *c) {
  EV_BEGIN;
  c->v[0] = c->self ? c->self->arg : 0;
  if (c->v[0] == 0) {
    NARRATE("* (The elevator back down.)\n* (Get in?)@Yes@No");
    if (CHOICE() == 1) return 1;
    FADE_TO(ROOM_HT_COREVIEW, 6930, 3110, DIR_DOWN);
    return 1;
  }
  NARRATE("* (The elevator up.)\n* (Get in?)@Yes@No");
  if (CHOICE() == 1) return 1;
  set_flag(F_HT_FLOOR2, 1);
  FADE_TO(ROOM_HT_L3, 3770, 842, DIR_DOWN);
  EV_END;
}

int ht2_event(Ev *c) {
  switch (c->id) {
    case EV_HOTDOG: return ev_hotdog(c);
    case EV_HEATS: return ev_heats(c);
    case EV_APRON: return ev_apron(c);
    case EV_BELT_SWITCH: return ev_belt_switch(c);
    case EV_VENTPUZ: return ev_ventpuz(c);
    case EV_HT2_CALL: return ev_call(c);
    case EV_GUARDS: return ev_guards(c);
    case EV_NEWS: return ev_news(c);
    case EV_BOMB: return ev_bomb(c);
    case EV_HT2_ELEVATOR: return ev_elevator(c);
  }
  return -1;
}
