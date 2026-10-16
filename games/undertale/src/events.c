/* The events (event.h): the common ones (signs, SAVE Points, the phone), then each area's. */
#include <stdio.h>
#include <string.h>
#include "event.h"
#include "events.h"
#include "battle.h"
#include "game.h"
#include "hotland.h"
#include "menu.h"
#include "ruins.h"
#include "snowdin.h"
#include "waterfall.h"

static int ev_sign(Ev *c) {
  EV_BEGIN;
  if (c->self && c->self->text) SAY(FACE_NONE, c->self->text);
  EV_END;
}

void say_on_entry(const char *text) {
  static Obj note;
  note.text = text;
  event_start(EV_SIGN, &note);
}

static char save_text[200];
static int ev_save(Ev *c) {
  EV_BEGIN;
  /* its words, the HP restored, then SAVE */
  if (sv.hp < sv.maxhp) snprintf(save_text, sizeof save_text, "%s|* (HP fully restored.)", c->self ? c->self->text : "");
  else snprintf(save_text, sizeof save_text, "%s", c->self ? c->self->text : "");
  sv.hp = sv.maxhp;
  SAY(FACE_NONE, save_text);
  menu_save_open();
  WAIT_UNTIL(g_mode == MODE_WORLD);
  EV_END;
}

static int ev_phone(Ev *c) {
  static const char *const opts[] = {"Say Hello", "Puzzle Help", "About Yourself", "Call Her \"Mom\"", "Flirt"};
  EV_BEGIN;
  NARRATE("* Dialing...");
  if (c->v[0] == 1) {   /* Papyrus */
    SAY(FACE_PAPYRUS, "`0* HELLO! THIS IS PAPYRUS!\n* HOW DID YOU GET THIS NUMBER?|`2* OH, RIGHT, I GAVE IT TO YOU!\n* NYEH HEH HEH!");
    return 1;
  }
  CHOOSE(opts, 5);
  switch (CHOSEN()) {
    case 0:
      set_flag(F_CALL_HELLO, flag(F_CALL_HELLO) + 1);
      if (flag(F_CALL_HELLO) == 1)
        SAY(FACE_TORIEL, "`0* TORIEL here.|* Just saying hello?|`2* Well, hello to you too!");
      else if (flag(F_CALL_HELLO) == 2)
        SAY(FACE_TORIEL, "`0* This is TORIEL.|* You want to say hello again?|`2* 'Salutations!'|`0* Is that enough?");
      else if (flag(F_CALL_HELLO) == 3)
        SAY(FACE_TORIEL, "`0* TORIEL here.|`1* Bored? I should have\n  left you a book.|`0* Pretend to be royalty!|`2* Rule the leaf pile!");
      else
        SAY(FACE_TORIEL, "`0* TORIEL here.|`1* I have little to say...|`0* But it is nice to hear\n  from you.");
      break;
    case 1:
      SAY(FACE_TORIEL, "`0* TORIEL here.|* A puzzle?|`1* Please wait for me; we\n  can solve it together.");
      break;
    case 2:
      SAY(FACE_TORIEL, "`0* TORIEL here.|* About me?|`2* Oh, there is not much to\n  tell. I worry a lot!");
      break;
    case 3:
      set_flag(F_CALL_MOM, 1);
      SAY(FACE_TORIEL, "`0* This is TORIEL.|`3* Huh? Did you just call me...\n  \"Mom\"?|`8* Well... I suppose...|* Would that make you happy?|* To call me... \"Mother?\"|`2* Well then, call me whatever\n  you like!");
      break;
    case 4:
      set_flag(F_CALL_FLIRT, flag(F_CALL_FLIRT) + 1);
      if (flag(F_CALL_FLIRT) == 1)
        SAY(FACE_TORIEL, "`3* ...oh?|`8* Ha ha ha!|`2* How sweet of you.|`0* But you can do better than\n  an old lady like me.");
      else if (flag(F_CALL_MOM))
        SAY(FACE_TORIEL, "`3* Oh dear, are you serious...?|* And after you said you want\n  to call me \"mother...\"|`1* You are an... ...\n  \"interesting\" child.");
      else
        SAY(FACE_TORIEL, "`3* Oh dear, are you serious...?|`1* I do not know if this is\n  pathetic, or endearing.");
      break;
    default:
      break;
  }
  EV_END;
}

static int ev_encounter(Ev *c) {
  EV_BEGIN;
  g_exclaim = 18;
  WAIT(18);
  g_exclaim = 0;
  BATTLE(c->v[0]);
  snow_battle_done(c->v[0]);
  wf_battle_done(c->v[0]);
  EV_END;
}

int event_run(Ev *c) {
  switch (c->id) {
    case EV_ENCOUNTER: return ev_encounter(c);
    case EV_SIGN: return ev_sign(c);
    case EV_SAVE: return ev_save(c);
    case EV_PHONE_MENU: return ev_phone(c);
  }
  int r = ruins1_event(c);
  if (r >= 0) return r;
  r = ruins2_event(c);
  if (r >= 0) return r;
  r = ruins3_event(c);
  if (r >= 0) return r;
  r = snow1_event(c);
  if (r >= 0) return r;
  r = snow2_event(c);
  if (r >= 0) return r;
  r = town_event(c);
  if (r >= 0) return r;
  r = wf1_event(c);
  if (r >= 0) return r;
  r = wf2_event(c);
  if (r >= 0) return r;
  r = wf3_event(c);
  if (r >= 0) return r;
  r = ht1_event(c);
  if (r >= 0) return r;
  return 1;
}
