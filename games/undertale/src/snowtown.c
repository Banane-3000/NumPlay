/* Snowdin Town: the shop, the inn, Grillby's, the library, the townsfolk, the foggy path where Papyrus fights,
 * his house and the time spent with him, and the way on to Waterfall. Signs and books are the game's; the
 * townsfolk's talks are kept short. */
#include <stdio.h>
#include <string.h>
#include "battle.h"
#include "event.h"
#include "events.h"
#include "game.h"
#include "items.h"
#include "menu.h"
#include "monsters.h"
#include "save.h"
#include "shop.h"
#include "snowdin.h"
#include "world.h"

static void gone(Obj *o) { o->kind = OBJ_NONE; }

/* someone of the town who's drawn on the map: a box to talk to, what they say */
static void folk(int x, int y, const char *text) {
  Obj *o = obj_sign(x - 12, y - 18, 24, 26, text);
  (void)o;
}

void town_setup(int room, int from) {
  (void)from;
  switch (room) {
    case ROOM_SNOWTOWN:
      obj_save(518, 1076, "* (The sight of such a friendly town fills you with determination.)");
      obj_sign(266, 1046, 30, 24, "* (Welcome to Snowdin Town!)");
      obj_sign(776, 1048, 20, 20, "* (Don't want to walk to the other side of town?)\n* (Try the undersnow tunnels!)|* (They're simple, quick, and convenient!)");
      {
        Obj *b = obj_trigger(566, 1072, 26, 24, EV_BOX, ACTOR_ITEM);
        if (b) b->flags = OF_TALK | OF_SOLID;
        Obj *shop = obj_trigger(446, 1066, 28, 24, EV_SHOP, ACTOR_TRIGGER);
        (void)shop;
        Obj *house = obj_trigger(2238, 1086, 28, 26, EV_TOWN_DOOR, ACTOR_TRIGGER2);
        if (house) house->arg = 0;
        Obj *shed = obj_trigger(2398, 1084, 28, 16, EV_TOWN_DOOR, ACTOR_TRIGGER3);
        if (shed) shed->arg = 1;
      }
      folk(850, 1029, "* My sister's walking our little brother on a leash again.\n* Is that weird? It's weird, right?");
      folk(911, 1101, "* I'm taking little Cinnamon for a walk!");
      folk(991, 1081, "* Teens kept decorating Gyftrot's horns as a prank...|* So now everyone decorates a tree and puts presents under it instead.");
      folk(1202, 1043, "* Papyrus reports straight to the Royal Guard.\n* Around here, that's about all the government we've got.");
      folk(1457, 1042, "* Everyone makes light of how things are down here.\n* I don't think it's funny.");
      folk(1515, 1040, "* If we didn't laugh about it, we'd just be sad.\n* So we laugh.");
      folk(1840, 1044, "* (The sign says \"Librarby.\")");
      if (flag(F_PAPYRUS) == 1) {
        Obj *p = obj_papyrus(2190, 1100, DIR_DOWN);
        if (p) p->event = EV_PAP_HOUSE, p->flags |= OF_TALK;
        if (!flag(F_GRILLBYS)) {
          Obj *s = obj_sans(1290, 1104, DIR_DOWN);
          if (s) s->event = EV_GRILLBYS, s->flags |= OF_TALK;
        }
      }
      folk(1099, 1093, "* Yo! You're a kid too, right?\n* I can tell 'cause you're wearing a striped shirt.|* I'm going to see Undyne fight someday.\n* She's the coolest!");
      break;
    case ROOM_TOWNNORTH:
      folk(1463, 851, "* The kids are playing \"monsters and humans.\"\n* Ah, to be young again...");
      folk(1513, 880, "* I'm the human! You can't catch me!");
      folk(1505, 807, "* (Knock knock.)\n* ... Who's there?\n* ...|* Oh, I just love it when someone knocks.");
      folk(1910, 789, "* I throw ice into the water.\n* It floats all the way to the CORE.");
      break;
    case ROOM_TOWNDOCK:
      folk(1611, 589, "* Tra la la.\n* The boat isn't going anywhere today.");
      break;
    case ROOM_INN1: {
      Obj *k = obj_trigger(636, 770, 26, 26, EV_INN, ACTOR_ITEM);
      if (k) k->flags = OF_TALK | OF_SOLID;
      folk(698, 815, "* My mom runs this inn.\n* Sleep in a bed and you'll feel all better!");
      break;
    }
    case ROOM_INN2:
      obj_sign(680, 520, 80, 80, "* (Two people are snoring loudly.)");
      break;
    case ROOM_GRILLBYS:
      folk(1246, 770, "* (Grillby is silently cleaning a glass.)");
      folk(1214, 792, "* Cheep cheep!\n* (The dogs here are the Royal Guard's Snowdin squad.)");
      folk(1194, 786, "* I'm fishing for a date.\n* Literally.");
      folk(1156, 782, "* (Lesser Dog is playing poker against itself.)");
      folk(1174, 834, "* (Bark!)");
      folk(1134, 860, "* I'm thinking of getting a collar...\n* Then maybe someone would take me for a walk.");
      folk(1316, 903, "* I want new drinks...\n* And hot guys!");
      folk(1324, 833, "* Monster food turns into energy right away.\n* I want to try human food someday.");
      folk(1329, 767, "* New Home's getting crowded.\n* I bet someday everyone moves out here.");
      if (flag(F_DOGI) != 2) {
        folk(1209, 871, "* Watch out for Greater Dog.\n* It'll love you to death.");
        folk(1238, 853, "* Nobody respects the sentries...");
      }
      break;
    case ROOM_LIBRARY:
      obj_sign(1830, 506, 30, 40, "* Monster funerals, technically speaking, are cool as heck.|* When monsters get old and kick the bucket, they turn into dust.\n* At funerals, we take that dust and spread it on that person's favorite thing.|* Then their essence will live on in that thing...\n* Uhhh, am I at the page minimum yet? I'm kinda sick of writing this.");
      obj_sign(1874, 506, 30, 40, "* While monsters are mostly made of magic, human beings are mostly made of water.|* Humans, with their physical forms, are far stronger than us.\n* But they will never know the joy of expressing themselves through magic.|* They will never get a bullet-pattern birthday card...");
      obj_sign(1918, 506, 30, 40, "* (Here I am... Writing this book.)\n* (A person comes in and picks up the book...)\n* (They start reading it...!)");
      obj_sign(1962, 506, 30, 40, "* Because they are made of magic, monsters' bodies are attuned to their SOUL.|* If a monster doesn't want to fight, its defenses will weaken.\n* And the crueller the intentions of our enemies, the more their attacks will hurt us.|* Therefore, if a being with a powerful SOUL struck with the desire to kill...\n* Um, let's end the chapter here...");
      obj_sign(2006, 506, 30, 40, "* Fearing the humans no longer, we moved out of our old city, HOME.|* We braved harsh cold, damp swampland, and searing heat...\n* Until we reached what we now call our capital.|* \"NEW HOME.\"\n* Again, our King is really bad at names...?");
      obj_sign(2050, 506, 30, 40, "* Love, hope, compassion...\n* This is what people say monster SOULs are made of.|* But the absolute nature of the \"SOUL\" is unknown.\n* After all, humans have proven their SOULs don't need these things to exist.");
      folk(1896, 640, "* Yes, the sign outside says \"Librarby.\"\n* We made a typo when we were making it.");
      folk(2031, 655, flag(F_WORDSEARCH) == 2 ? "* I can see it in your eyes...\n* You think Junior Jumble is harder." : "* I can see it in your eyes...\n* You think crosswords are harder.");
      folk(2002, 589, "* There's no news around here, so the newspaper is mostly comics and puzzles.");
      folk(2044, 590, "* I make the word searches.\n* Number one in the underground!");
      break;
    case ROOM_PSHOUSE:
      {
        Obj *couch = obj_trigger(2300, 892, 82, 40, EV_COUCH, ACTOR_ITEM);
        if (couch) couch->flags = OF_TALK;
      }
      obj_sign(2316, 834, 52, 30, "* (A TV.)");
      obj_sign(2398, 800, 64, 50, "* (The kitchen.)");
      if (flag(F_PAP_DATE) == 1) {
        Obj *p = obj_papyrus(2276, 940, DIR_RIGHT);
        if (p) p->event = EV_PAP_DATE, p->flags |= OF_TALK;
      }
      break;
    case ROOM_SANSROOM:
      obj_sign(2560, 600, 40, 40, "* (It's dark... A trash tornado swirls in the corner.)");
      break;
    case ROOM_SHED:
      obj_sign(2650, 770, 30, 20, flag(F_PAP_CAPTURED) <= 1 ? "* SORRY, I HAVE TO LOCK YOU IN THE GUEST ROOM UNTIL UNDYNE ARRIVES.\n* FEEL FREE TO MAKE YOURSELF AT HOME!!!|* REFRESHMENTS AND ACCOMODATIONS HAVE BEEN PROVIDED.\n* - NYEHFULLY YOURS, PAPYRUS"
                                                    : flag(F_PAP_CAPTURED) == 2 ? "* PLEASE ASK BEFORE YOU ESCAPE!!!\n* WHEN YOU WENT MISSING, I GOT WORRIED SICK!!!\n* - SLIGHTLY BONETROUSLED, PAPYRUS"
                                                                                : "* IF YOU'RE JUST LOOKING FOR A PLACE TO STAY...\n* JUST ASK!!! YOU DON'T NEED TO FIGHT ME!!!\n* - YOUR HOST, PAPYRUS");
      obj_sign(2680, 820, 40, 30, "* (A dog bed. It's warm.)");
      break;
    case ROOM_FOGROOM:
      if (!flag(F_PAPYRUS)) obj_trigger(3290, 1030, 30, 150, EV_PAP_FIGHT, ACTOR_TRIGGER);
      obj_trigger(3596, 1040, 24, 140, EV_SNOW_END, ACTOR_TRIGGER2);
      break;
  }
}

/* ---------------------------------------------------------------- the events */
static int ev_shop(Ev *c) {
  EV_BEGIN;
  pl.y = (int16_t)(pl.y + 6);   /* (a step back from the door, for after) */
  shop_open(SHOP_SNOWDIN);
  WAIT_UNTIL(g_mode == MODE_WORLD);
  pl.dir = DIR_DOWN;
  EV_END;
}

static int ev_inn(Ev *c) {
  EV_BEGIN;
  NARRATE("* Hello, traveller.\n* Welcome to Snowed Inn.|* A room is 80G for the night.\n* Would you like to stay?@Stay@Leave");
  if (CHOICE() == 1) {
    NARRATE("* Come back anytime!");
  } else if (sv.gold < 80) {
    NARRATE("* Oh... You don't have enough money...|* ... Um, you know what? Take the room anyway.");
    c->v[0] = 1;
  } else {
    c->v[0] = 1;
  }
  if (c->v[0]) {
    NARRATE("* Here's your key.\n* Sleep well!");
    world_fade_hold(ROOM_INN2, 540, 620, DIR_DOWN, 90);
    WAIT_UNTIL(!world_fading());
    sv.hp = sv.maxhp;
    set_flag(F_INN, flag(F_INN) + 1);
    NARRATE("* (You slept soundly. Your HP was maxed out.)|* (The innkeeper gave you your money back.)");
  }
  EV_END;
}

static int ev_town_door(Ev *c) {
  int k = c->self ? c->self->arg : 0;
  EV_BEGIN;
  if (k == 0 && flag(F_TOWN_HOUSE)) {   /* Papyrus and Sans's house */
    FADE_TO(ROOM_PSHOUSE, 2436, 914, DIR_UP);
    return 1;
  }
  NARRATE("* (It's locked.)");
  pl.y = (int16_t)(pl.y + 6);   /* (a step back off the door) */
  EV_END;
}

static int ev_pap_fight(Ev *c) {
  Obj *p = obj_get(ACTOR_PAPYRUS);
  EV_BEGIN;
  /* through the fog, a tall silhouette */
  p = obj_papyrus(3440, 1100, DIR_LEFT);
  p = obj_get(ACTOR_PAPYRUS);
  pl.dir = DIR_RIGHT;
  WALK(p, 3400, 1100, 1);
  if (flag(F_PAP_CAPTURED) >= 3) {
    SAY(FACE_PAPYRUS, "`0* HUMAN! YOU KEEP COMING BACK...\n* WOULD YOU LIKE TO SKIP OUR BATTLE?@Yes@No");
    if (CHOICE() == 0) {
      SAY(FACE_PAPYRUS, "`2* WELL THEN! LET'S BE FRIENDS INSTEAD!!!");
      set_flag(F_PAPYRUS, 1);
      goto spared;
    }
  } else if (flag(F_PAP_CAPTURED)) {
    SAY(FACE_PAPYRUS, "`0* HUMAN! YOU ESCAPED!\n* LET'S TRY THIS AGAIN!!!");
  } else {
    SAY(FACE_PAPYRUS, "`0* HUMAN. I HAVE BEEN THINKING.\n* YOU LIKE PASTA. YOU SOLVE PUZZLES. YOU SEEM... COOL.|`2* I COULD BE YOUR FRIEND!!!|`3* NO! A ROYAL GUARD CAN'T BEFRIEND A HUMAN!\n* I MUST CAPTURE YOU AND JOIN THE GUARD!!!");
  }
  sv.hp = sv.maxhp;
  BATTLE(FORM_PAPYRUS);
  set_flag(F_SNOWDIN_BOSSES, flag(F_SNOWDIN_BOSSES) + 1);
  if (battle_result() == BR_SCRIPTED) {
    /* captured: off to the shed */
    set_flag(F_PAP_CAPTURED, flag(F_PAP_CAPTURED) + 1);
    if (flag(F_PAP_CAPTURED) == 1)
      SAY(FACE_PAPYRUS, "`2* NYEH HEH HEH! CAPTURED!!!\n* OFF TO THE CAPTURE ZONE WITH YOU! (OUR SHED.)");
    else if (flag(F_PAP_CAPTURED) == 2)
      SAY(FACE_PAPYRUS, "`2* CAPTURED AGAIN!!!\n* THE SHED HAS BEEN IMPROVED SINCE YOUR LAST VISIT.");
    else
      SAY(FACE_PAPYRUS, "`2* YOU ARE... PERSISTENT!\n* BUT I AM THE PERSISTENTEST!!!");
    gone(p);
    world_fade_hold(ROOM_SHED, 2640, 840, DIR_DOWN, 40);
    WAIT_UNTIL(!world_fading());
    return 1;
  }
  if (bt.mon[0].killed) {
    set_flag(F_PAPYRUS, 2);
    gone(p);
    SAY(FACE_PAPYRUS, "`4* WELL... I DIDN'T SEE THAT COMING.|* STILL, I THINK YOU CAN BE BETTER THAN THIS.\n* I BELIEVE IN YOU!");
    if (c->self) gone(c->self);
    return 1;
  }
  set_flag(F_PAPYRUS, 1);
spared:
  SAY(FACE_PAPYRUS, "`4* NYOO HOO HOO... I COULDN'T EVEN CAPTURE YOU.\n* SO MUCH FOR THE ROYAL GUARD.|`0* WAIT... YOU WANT TO BE FRIENDS???|`2* WOWIE!!! COME HANG OUT AT MY HOUSE LATER!|`0* (THE CAPITAL IS FURTHER ON, PAST THE TOWN.)");
  set_flag(F_PAP_DATE, 1);
  set_flag(F_TOWN_HOUSE, 1);
  WALK(p, 3240, 1100, 6);
  gone(p);
  if (c->self) gone(c->self);
  EV_END;
}

static int ev_pap_house(Ev *c) {
  EV_BEGIN;
  if (flag(F_PAP_DATE) == 1) {
    SAY(FACE_PAPYRUS, "`0* HUMAN! READY TO HANG OUT?\n* COME INSIDE!!!");
    FADE_TO(ROOM_PSHOUSE, 2436, 914, DIR_UP);
  } else {
    SAY(FACE_PAPYRUS, "`2* HELLO, FRIEND!!!");
  }
  EV_END;
}

static int ev_pap_date(Ev *c) {
  EV_BEGIN;
  /* his room, his spaghetti, his number */
  SAY(FACE_PAPYRUS, "`0* WELCOME TO MY HOUSE!\n* THIS IS WHERE I LIVE WITH MY BROTHER, SANS.|* LET'S GO TO MY ROOM!!!");
  FADE_TO(ROOM_PAPROOM, 2290, 650, DIR_UP);
  {
    Obj *p = obj_papyrus(2330, 640, DIR_LEFT);
    (void)p;
  }
  SAY(FACE_PAPYRUS, "`0* MY ROOM!!! IMPRESSIVE, ISN'T IT?|`4* NOW... HANGING OUT. I READ A GUIDE ABOUT THIS.|`0* STEP ONE: A PRESENT!\n* FOR YOU: SPAGHETTI!@Eat it@Keep it");
  if (CHOICE() == 0) SAY(FACE_PAPYRUS, "`2* WOWIE!!! YOU LIKED IT?!?\n* I'LL HAVE TO MAKE MORE!!!");
  else SAY(FACE_PAPYRUS, "`2* SAVING IT FOR LATER?\n* A TRUE CONNOISSEUR!");
  SAY(FACE_PAPYRUS, "`4* HUMAN... I'VE THOUGHT ABOUT IT.\n* LET'S STAY FRIENDS! THE BEST OF FRIENDS!!!|`2* TAKE MY PHONE NUMBER! CALL ANY TIME!\n* NYEH HEH HEH!!!");
  NARRATE("* (You got Papyrus's phone number.)");
  set_flag(F_PAP_DATE, 2);
  FADE_TO(ROOM_SNOWTOWN, 2240, 1096, DIR_DOWN);
  EV_END;
}

static int ev_grillbys(Ev *c) {
  EV_BEGIN;
  SAY(FACE_SANS, "* hey.\n* thanks for keeping my brother entertained.|* want to grab some lunch?@Sure@Not now");
  if (CHOICE() == 1) {
    SAY(FACE_SANS, "* ok. maybe later.");
    return 1;
  }
  SAY(FACE_SANS, "* well, i know a shortcut.");
  FADE_TO(ROOM_GRILLBYS, 1230, 880, DIR_UP);
  SAY(FACE_SANS, "* here's a good seat.|* so, what'll it be?\n`2* fries or burger?@Fries@Burger");
  SAY(FACE_SANS, "* nice.|* say... papyrus says a #Yflower#X talks to him sometimes.\n* gives him advice. weird, right?|* if you see a talking flower, tell me, ok?|`1* oh, and thanks for paying.");
  set_flag(F_GRILLBYS, 1);
  FADE_TO(ROOM_SNOWTOWN, 1310, 1096, DIR_DOWN);
  EV_END;
}

static int ev_snow_end(Ev *c) {
  EV_BEGIN;
  /* on to Waterfall: not made yet */
  set_flag(F_SNOW_EXIT, 1);
  sv.room = (int16_t)g_room, sv.x = (int16_t)(pl.x - 20), sv.y = pl.y;
  save_write_auto();
  menu_end_open();
  EV_END;
}

static int ev_couch(Ev *c) {
  EV_BEGIN;
  if (!flag(F_COUCH_GOLD)) {
    set_flag(F_COUCH_GOLD, 1);
    sv.gold = (uint16_t)min_i(9999, sv.gold + 20);
    NARRATE("* (There's 20G under the couch!)");
  } else {
    NARRATE("* (It's a couch.)");
  }
  EV_END;
}

int town_event(Ev *c) {
  switch (c->id) {
    case EV_SHOP: return ev_shop(c);
    case EV_INN: return ev_inn(c);
    case EV_TOWN_DOOR: return ev_town_door(c);
    case EV_PAP_FIGHT: return ev_pap_fight(c);
    case EV_PAP_HOUSE: return ev_pap_house(c);
    case EV_PAP_DATE: return ev_pap_date(c);
    case EV_GRILLBYS: return ev_grillbys(c);
    case EV_SNOW_END: return ev_snow_end(c);
    case EV_COUCH: return ev_couch(c);
  }
  return -1;
}
