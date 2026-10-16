/* Hotland, its third floor: out of the elevator, the spiders' bake sale; the vent islands, the shooting puzzles north
 * (the Diamond Boys) and south (over the belts, past the drones); the spider entrance and Muffet's lair, its webs;
 * Mettaton's musical, the trapdoor down to the tile puzzle and his fight; the crossroads, where the way north leads to
 * the MTT Resort. (As the Undertale Wiki describes them; what they say is ours, in short.) */
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
#include "shoot.h"
#include "snowdin.h"
#include "world.h"

static void gone(Obj *o) { o->kind = OBJ_NONE; }
static Obj *thing(int x, int y, int w, int h, int event, int name, int arg) {
  Obj *o = obj_trigger(x, y, w, h, event, name);
  if (o) o->flags = OF_TALK | OF_SOLID, o->arg = (int16_t)arg;
  return o;
}
static Obj *npc(int name, uint16_t spr, int x, int y) {
  const uint16_t s[4] = {spr, spr, spr, spr};
  Obj *o = obj_npc(name, s, x, y, DIR_DOWN, 0);
  if (o) o->flags &= (uint16_t)~OF_SOLID;
  return o;
}
static Obj *obj_mtt(int x, int y) {
  Obj *o = npc(ACTOR_METTATON, SPR_MTT_OW, x, y);
  if (o) o->flags |= OF_ANIM, o->anim_speed = 5;
  return o;
}

/* ---------------------------------------------------------------- Muffet's lair: its webs hold Frisk back */
typedef struct {
  int16_t x, y, w, h;
  uint8_t speed;
} Web;
static const Web webs[] = {
  {5800, 466, 60, 32, 2}, {5880, 466, 60, 32, 2}, {5975, 466, 60, 32, 2},   /* the small ones */
  {6083, 454, 400, 46, 1},                                                  /* the big one, Muffet's */
  {6540, 466, 60, 32, 2},
};
static bool muffet_holds;
int ht3_speed(int x, int y) {
  if (g_room != ROOM_HT_LAIR) return 3;
  if (muffet_holds) return 0;
  for (unsigned i = 0; i < sizeof webs / sizeof webs[0]; i++)
    if (x >= webs[i].x && x < webs[i].x + webs[i].w && y >= webs[i].y && y < webs[i].y + webs[i].h) return webs[i].speed;
  return 3;
}

/* ---------------------------------------------------------------- the tile puzzle (under the stage) */
/* the map's tiles: R red (a wall: the room's collision leaves it out), Y yellow (a shock: back to the last tile
 * stepped on), O orange (Frisk smells of oranges), U purple (soap: lemons, and a slide on to the next tile), B blue
 * (water: the piranhas bite if Frisk smells of oranges; next to yellow, it shocks), P pink and G green (nothing) */
#define TX 6640
#define TY 620
#define TW 39
#define TH 4
static const char tiles[TH][TW + 1] = {
  "BGYPGUPYGRYPRGPOYRRORYPYYGGBYBOGGGRGGGR",
  "GOUBORRBYRRBBBRRRPPRBPGRUOBOUUPBRGRGRGR",
  "PRPPBBRORPBURPYGUOUYUURRPOBUUBYPRGRGRGR",
  "GYBYRPGOUUOPROPORBBPOBGOPBUOYBPPGGGGRGG",
};
#define TILES_TIME (30 * 30)
enum { TILES_OFF, TILES_ON, TILES_DONE };
static int tiles_state, tiles_t, tiles_why, flavor, last_r = -99, last_c = -99, safe_x, safe_y, tile_act, flames_in;
static char tile_at(int r, int c) { return r < 0 || c < 0 || r >= TH || c >= TW ? 0 : tiles[r][c]; }
static bool tile_bad(int r, int c) {
  char t = tile_at(r, c);
  if (t == 'Y') return true;
  if (t != 'B') return false;
  if (flavor == 'O') return true;
  return tile_at(r - 1, c) == 'Y' || tile_at(r + 1, c) == 'Y' || tile_at(r, c - 1) == 'Y' || tile_at(r, c + 1) == 'Y';
}
static void tiles_watch(void) {
  if (tiles_state != TILES_ON) return;
  if (flames_in) {
    /* out of time: the flames close in, then Alphys */
    if (++flames_in > 40) tiles_why = 2, event_start(EV_TILES3, NULL);
    return;
  }
  if (--tiles_t <= 0) {
    flames_in = 1;
    return;
  }
  int fx = pl.x + 10, fy = pl.y + 24;
  if (fx >= TX + TW * 20) {
    tiles_why = 1, event_start(EV_TILES3, NULL);
    return;
  }
  int c = fx >= TX ? (fx - TX) / 20 : -1, r = fy >= TY ? (fy - TY) / 20 : -1;
  if (c < 0 || r < 0 || r >= TH) {
    last_r = last_c = -99, safe_x = pl.x, safe_y = pl.y;
    return;
  }
  if (r != last_r || c != last_c) {
    last_r = r, last_c = c;
    if (tile_bad(r, c)) {
      tile_act = 1, event_start(EV_TILE, NULL);
      return;
    }
    if (tile_at(r, c) == 'O') flavor = 'O';
    if (tile_at(r, c) == 'U') {
      flavor = 'L', tile_act = 2, event_start(EV_TILE, NULL);
      return;
    }
  }
  if (tile_at(r, c) != 'U') safe_x = pl.x, safe_y = pl.y;
}

/* ---------------------------------------------------------------- drawn, and watched */
static void draw_flame_wall(int x) {
  for (int k = 0; k < 6; k++)
    gfx_sprite_color(SPR_FIREBULLET, (int)(g_frame / 4 + (uint32_t)k) % 2, x - cam_x, TY - 10 + k * 18 - cam_y, 0,
                     (g_frame / 3 + (uint32_t)k) % 2 ? rgb565(0xFF8000) : rgb565(0xFFC000), 255);
}
void ht3_draw(void) {
  if (g_room != ROOM_HT_TILES || tiles_state != TILES_ON) return;
  int in = flames_in * 6;
  draw_flame_wall(TX - 14 + in);
  draw_flame_wall(TX + TW * 20 + 170 - in);
}
void ht3_draw_top(void) {
  if (g_room != ROOM_HT_TILES || tiles_state != TILES_ON) return;
  char s[16];
  int secs = (tiles_t + 29) / 30;
  snprintf(s, sizeof s, "0:%02d", secs);
  gfx_rect(4, 4, 52, 18, C_BLACK, 200);
  gfx_text(FONT_MAIN, 10, 6, s, secs <= 10 ? C_RED : C_WHITE, 255);
}
void ht3_update(void) {
  if (g_room == ROOM_HT_TILES) tiles_watch();
}

/* ---------------------------------------------------------------- the rooms */
static void diamond_boys(void) {
  Obj *a = npc(ACTOR_ITEM, SPR_DIAMOND1, 4750, 452);
  if (a) a->flags |= OF_SOLID;
  Obj *b = npc(ACTOR_ITEM, SPR_DIAMOND2, 4905, 454);
  if (b) b->flags |= OF_SOLID;
  obj_sign(4736, 424, 30, 32, "* Did you see Mettaton's last\n  show? He was AMAZING!|* When he strikes a pose, the\n  ratings go through the roof!");
  obj_sign(4890, 420, 30, 38, "* My mom says never touch the\n  switch on his back.|* I wonder what it does...");
}

void ht3_setup(int room, int from) {
  (void)from;
  if (room < ROOM_HT_L3 || room > ROOM_HT_CROSS3) return;
  if (room != ROOM_HT_TILES && tiles_state == TILES_ON) tiles_state = TILES_OFF;   /* (a SAVE loaded mid-puzzle) */
  switch (room) {
    case ROOM_HT_L3:
      thing(3740, 800, 80, 56, EV_HT3_ELEVATOR, ACTOR_DOOR, 1);
      thing(4128, 846, 74, 36, EV_BAKESALE3, ACTOR_ITEM, 0);
      obj_sign(4276, 892, 44, 40, "* I didn't even want a donut...|* The spiders scared me into\n  buying one.\n* There goes all my money.");
      break;
    case ROOM_HT_HUB3:
      if (!flag(F_HT_CALL_HUB3)) {
        Obj *t = obj_trigger(4566, 856, 10, 70, EV_HT3_CALL, ACTOR_TRIGGER);
        if (t) t->arg = 0;
      }
      if (!(flag(F_HT3_PUZ_N) && flag(F_HT3_PUZ_S))) {
        Obj *d = obj_trigger(5200, 700, 60, 12, EV_NONE, ACTOR_DOOR);
        if (d) d->flags = OF_SOLID;
        obj_sign(5200, 712, 60, 16, "* (The door is locked.)\n* (Both puzzles have to be solved.)");
      }
      break;
    case ROOM_HT_NORTH3:
      thing(4812, 154, 36, 30, EV_SHOOT3, ACTOR_ITEM, SHOOT_NORTH);
      if (flag(F_HT3_PUZ_N)) diamond_boys();
      break;
    case ROOM_HT_BELTS3:
      obj_sign(5118, 1286, 30, 34, "* (A cactus, all on its own.)\n* (You stop to look at it.)");
      break;
    case ROOM_HT_SOUTH3:
      thing(5524, 1144, 36, 30, EV_SHOOT3, ACTOR_ITEM, SHOOT_SOUTH);
      break;
    case ROOM_HT_SPIDERENT:
      obj_save(5300, 420, "* (The smell of cobwebs fills you with determination.)");
      break;
    case ROOM_HT_LAIR:
      if (!flag(F_HT_MUFFET)) obj_trigger(6240, 455, 8, 50, EV_MUFFET, ACTOR_TRIGGER);
      break;
    case ROOM_HT_STAGE:
      if (!flag(F_HT_MUSICAL)) obj_trigger(6760, 440, 8, 66, EV_MUSICAL, ACTOR_TRIGGER);
      break;
    case ROOM_HT_TILES:
      if (!flag(F_HT_TILES3) && tiles_state != TILES_ON) obj_mtt(6430, 626);
      break;
    case ROOM_HT_CROSS3:
      thing(7444, 360, 70, 58, EV_NICECREAM3, ACTOR_ITEM, 0);
      if (flag(F_HT_GUARDS) == 1) {
        static const uint16_t s1[4] = {SPR_RG01_OW, SPR_RG01_OW, SPR_RG01_OW, SPR_RG01_OW};
        static const uint16_t s2[4] = {SPR_RG02_OW, SPR_RG02_OW, SPR_RG02_OW, SPR_RG02_OW};
        obj_npc(ACTOR_RG01, s1, 7600, 412, DIR_DOWN, 0);
        obj_npc(ACTOR_RG02, s2, 7642, 412, DIR_DOWN, 0);
        obj_sign(7580, 374, 84, 40, "* Dude, this ice cream is, like,\n  the best.|* ... ...yeah.");
      }
      thing(8054, 430, 24, 30, EV_HEATS3, ACTOR_ITEM, 0);
      thing(7934, 340, 80, 80, EV_HT3_ELEVATOR, ACTOR_DOOR, 0);
      obj_trigger(7522, 296, 56, 10, EV_HT3_END, ACTOR_TRIGGER);
      break;
  }
}

/* ---------------------------------------------------------------- the events */
static int ev_bakesale(Ev *c) {
  static const char *const opts[] = {"Spider Donut - 9999G", "Spider Cider - 9999G"};
  EV_BEGIN;
  NARRATE("* (A spider bake sale.)\n* (All proceeds go to real spiders.)");
  CHOOSE(opts, 2);
  if (CHOSEN() < 0) return 1;
  if (sv.gold < 9999) {
    NARRATE("* (You don't have enough gold.)");
    return 1;
  }
  if (!item_add(CHOSEN() ? ITEM_SPIDER_CIDER : ITEM_SPIDER_DONUT)) {
    NARRATE("* (You're carrying too much.)");
    return 1;
  }
  sv.gold = (uint16_t)(sv.gold - 9999);
  NARRATE("* (The spiders take your money.)");
  EV_END;
}

/* Alphys's calls: 0 by the vents; after the south puzzle, the anime (in ev_shoot) */
static int ev_call(Ev *c) {
  EV_BEGIN;
  if (c->self) gone(c->self);
  set_flag(F_HT_CALL_HUB3, 1);
  SAY(FACE_ALPHYS, "* Ring...|* Hi! It's Alphys.|* The door on the right opens\n  once both puzzles are done.|* One's up north, one's down\n  south. Good luck!");
  EV_END;
}

static int ev_shoot(Ev *c) {
  EV_BEGIN;
  c->v[0] = c->self ? c->self->arg : SHOOT_NORTH;
  c->v[1] = c->v[0] == SHOOT_NORTH ? F_HT3_PUZ_N : F_HT3_PUZ_S;
  if (flag(c->v[1])) {
    NARRATE("* (This puzzle is solved.)");
    return 1;
  }
  shoot_open(c->v[0]);
  WAIT_UNTIL(g_mode == MODE_WORLD);
  if (!shoot_result()) return 1;
  set_flag(c->v[1], 1);
  if (flag(F_HT3_PUZ_N) && flag(F_HT3_PUZ_S)) {
    NARRATE("* (Somewhere, a door unlocks.)");
  } else {
    NARRATE("* (One down.)");
  }
  if (c->v[0] == SHOOT_NORTH) {
    diamond_boys();
    return 1;
  }
  /* the south one: Alphys calls, about anime */
  SAY(FACE_ALPHYS, "* Ring...|* H-hey! Um... when all this\n  is over...|* want to watch anime with me?@Yes@No");
  if (CHOICE() == 0) {
    set_flag(F_HT_ANIME, 1);
    SAY(FACE_ALPHYS, "* R-really?! Okay!|* There's this show about a\n  magical girl, and she...");
    NARRATE("* (Alphys talks about it for a\n  long, long time.)");
    SAY(FACE_ALPHYS, "* ...oh! Sorry! Bye!");
  } else {
    set_flag(F_HT_ANIME, 2);
    SAY(FACE_ALPHYS, "* Oh. Okay. That's... fine.\n* Bye!");
  }
  EV_END;
}

static int ev_muffet(Ev *c) {
  static const uint16_t s[4] = {SPR_MUFFET_OW, SPR_MUFFET_OW, SPR_MUFFET_OW, SPR_MUFFET_OW};
  Obj *m = obj_get(ACTOR_MUFFET);
  EV_BEGIN;
  if (c->self) gone(c->self);
  /* the web holds Frisk fast, and down comes Muffet */
  muffet_holds = true;
  NARRATE("* (The web holds you fast.)\n* (You can't move!)");
  m = obj_npc(ACTOR_MUFFET, s, pl.x + 60, pl.y - 30, DIR_LEFT, 0);
  if (m) m->flags &= (uint16_t)~OF_SOLID;
  m = obj_get(ACTOR_MUFFET);
  WALK(m, pl.x + 60, pl.y + 29, 2);
  NARRATE("* Ahuhuhu~|* Did you think you could just\n  walk through my lair, deary?|* Someone pays very well for\n  human SOULs, you know~|* And you didn't buy a single\n  donut. How stingy~");
  BATTLE(FORM_MUFFET);
  muffet_holds = false;
  if (bt.mon[0].killed) {
    set_flag(F_HT_MUFFET, 2);
    gone(obj_get(ACTOR_MUFFET));
    return 1;
  }
  set_flag(F_HT_MUFFET, 1);
  NARRATE("* Ahuhu... no hard feelings,\n  deary~|* Do come back to our bake sale!");
  m = obj_get(ACTOR_MUFFET);
  WALK(m, m->x + 200, m->y, 3);
  gone(obj_get(ACTOR_MUFFET));
  EV_END;
}

static int ev_musical(Ev *c) {
  Obj *m = obj_get(ACTOR_METTATON);
  EV_BEGIN;
  if (c->self) gone(c->self);
  NARRATE("* (The lights go down.)\n* (A spotlight finds you.)");
  /* (the view: up to his balcony, and back) */
  WAIT_UNTIL(cam_pan(7000, 214, 6));
  text_set_top(0);   /* (the box below, out of his way) */
  NARRATE("* OHHH! WHO IS THIS, BENEATH MY\n  BALCONY?|* A HUMAN, COME TO SAVE ME FROM\n  MY TOWER?|* ALAS! A ROBOT AND A HUMAN...\n* IT COULD NEVER BE!|* SO, MY LOVE... YOU'LL JUST HAVE\n  TO DROP IN!");
  text_set_top(-1);
  WAIT_UNTIL(cam_pan(pl.x - 150, pl.y - 105, 6));
  cam_free();
  g_shake = 6;
  NARRATE("* (The floor opens up under you!)");
  set_flag(F_HT_MUSICAL, 1);
  FADE_TO(ROOM_HT_TILES, 6420, 680, DIR_RIGHT);
  /* below: Mettaton, the tiles, thirty seconds */
  m = obj_get(ACTOR_METTATON);
  NARRATE("* OH, HELLO AGAIN, DARLING!|* REMEMBER THIS PUZZLE? FROM\n  WAY BACK IN SNOWDIN?|* THEN YOU KNOW THE RULES.\n* THIRTY SECONDS! GO!");
  tiles_state = TILES_ON, tiles_t = TILES_TIME, flavor = 0, flames_in = 0, last_r = last_c = -99;
  safe_x = pl.x, safe_y = pl.y;
  (void)m;
  EV_END;
}

static const int8_t ddx[4] = {0, 0, -1, 1}, ddy[4] = {1, -1, 0, 0};
static int ev_tile(Ev *c) {
  EV_BEGIN;
  c->v[0] = tile_act;
  if (c->v[0] == 1) {
    /* a shock (or the piranhas): back to the last tile stepped on */
    g_shake = 4;
    PL_WALK(safe_x, safe_y, 4);
    last_r = last_c = -99;
    return 1;
  }
  /* the soap: on to the next tile that way, unless a wall's there */
  c->v[1] = last_r + ddy[pl.dir], c->v[2] = last_c + ddx[pl.dir];
  c->v[3] = TX + c->v[2] * 20, c->v[4] = TY + c->v[1] * 20 - 14;
  if (!world_walkable(c->v[3] + 3, c->v[4] + 20, 14, 9, -1)) return 1;
  PL_WALK(c->v[3], c->v[4], 4);
  EV_END;
}

static int ev_tiles_end(Ev *c) {
  Obj *m = obj_get(ACTOR_METTATON);
  EV_BEGIN;
  c->v[0] = tiles_why;
  tiles_state = TILES_DONE;
  if (c->v[0] == 1) {
    NARRATE("* OH! YOU MADE IT ACROSS.\n* HOW... ANTICLIMACTIC.|* NO MATTER! I'LL BE THE MONSTER\n  YOU MEET AT THE END!");
  } else {
    NARRATE("* (The flames close in...!)");
    SAY(FACE_ALPHYS, "* Ring...|* H-hang on! Turning off the\n  firewall!");
    NARRATE("* (The flames die down.)");
    NARRATE("* TSK! FINE. THEN I'LL BE THE\n  MONSTER YOU MEET!");
  }
  m = obj_get(ACTOR_METTATON);
  if (m->kind == OBJ_NONE) obj_mtt(pl.x + 60, pl.y + 29);
  else WALK(m, pl.x + 60, pl.y + 29, 6);
  BATTLE(FORM_MTT_TILES);
  set_flag(F_HT_TILES3, c->v[0]);
  NARRATE("* OHHH... I'M ALL OUT OF POWER.|* UNTIL NEXT TIME, DARLING!\n* STAY TUNED!");
  m = obj_get(ACTOR_METTATON);
  WALK(m, m->x, m->y - 220, 6);
  gone(obj_get(ACTOR_METTATON));
  EV_END;
}

static int ev_heats(Ev *c) {
  EV_BEGIN;
  if (flag(F_HT_HEATS3)) {
    NARRATE("* I'LL GET YOU NEXT TIME!!!");
    return 1;
  }
  NARRATE("* HEY!!! YOU!!!|* DO YOU REMEMBER MY NAME???@Yes@No");
  set_flag(F_HT_HEATS3, 1);
  NARRATE("* WHAT?!|* HOW COULD I LOSE SO EASILY?!\n* GRRR...!!!");
  EV_END;
}

static int ev_nicecream(Ev *c) {
  EV_BEGIN;
  if (flag(F_HT_GUARDS) == 1) {
    NARRATE("* Sorry, I'm all sold out!|* Those two guards bought every\n  last one!");
    return 1;
  }
  NARRATE("* Nice Cream! Just 12G!\n* Want one?@Yes@No");
  if (CHOICE() == 1) return 1;
  if (sv.gold < 12) {
    NARRATE("* Oh... you're short on gold.");
    return 1;
  }
  if (!item_add(ITEM_NICE_CREAM)) {
    NARRATE("* Your hands are full!");
    return 1;
  }
  sv.gold = (uint16_t)(sv.gold - 12);
  NARRATE("* Here you go! Have a super day!");
  EV_END;
}

static int ev_elevator(Ev *c) {
  EV_BEGIN;
  c->v[0] = c->self ? c->self->arg : 0;
  NARRATE("* (The elevator down.)\n* (Get in?)@Yes@No");
  if (CHOICE() == 1) return 1;
  if (c->v[0]) {
    FADE_TO(ROOM_HT_COREVIEW2, 3770, 1348, DIR_DOWN);
  } else {
    FADE_TO(ROOM_HT_HOTDOG, 6966, 2380, DIR_DOWN);
  }
  EV_END;
}

static int ev_end(Ev *c) {
  EV_BEGIN;
  NARRATE("* (The MTT Resort is just up\n  these stairs.)");
  set_flag(F_HT_FLOOR3, 1);
  sv.room = (int16_t)g_room, sv.x = 7530, sv.y = 420;
  save_write_auto();
  menu_end_open();
  EV_END;
}

int ht3_event(Ev *c) {
  switch (c->id) {
    case EV_BAKESALE3: return ev_bakesale(c);
    case EV_SHOOT3: return ev_shoot(c);
    case EV_HT3_CALL: return ev_call(c);
    case EV_MUFFET: return ev_muffet(c);
    case EV_MUSICAL: return ev_musical(c);
    case EV_TILE: return ev_tile(c);
    case EV_TILES3: return ev_tiles_end(c);
    case EV_HEATS3: return ev_heats(c);
    case EV_NICECREAM3: return ev_nicecream(c);
    case EV_HT3_ELEVATOR: return ev_elevator(c);
    case EV_HT3_END: return ev_end(c);
  }
  return -1;
}
