/* Snowdin's monsters: stats, ACTs and texts (from the Undertale Wiki's records of the game), their formations,
 * where they come out, and their attacks, as the wiki describes them. Papyrus is in papyrus.c. */
#include <stdio.h>
#include <string.h>
#include "battle.h"
#include "game.h"
#include "gfx.h"
#include "items.h"
#include "monsters.h"
#include "res.h"
#include "snowdin.h"
#include "world.h"

#define PICK(arr) ((arr)[rnd_n((int)(sizeof(arr) / sizeof((arr)[0])))])

/* whether the player ACTed on the monster this turn (else it's a "neutral" turn: Snowdrake tells a pun) */
static bool acted_on(const Monster *m) { return m->state2 & 0x100; }
static void mark_acted(Monster *m) { m->state2 = (int16_t)(m->state2 | 0x100); }
static void clear_acted(Monster *m) { m->state2 = (int16_t)(m->state2 & ~0x100); }

/* the drakes and Jerry: a joke makes the others laugh, and they can be spared */
static void others_laugh(Battle *b, Monster *self) {
  for (int i = 0; i < b->nmon; i++) {
    Monster *o = &b->mon[i];
    if (o == self || !o->alive || o->gone) continue;
    o->spare = true, o->state = 9;
  }
}
static bool others_here(Battle *b, Monster *self) {
  for (int i = 0; i < b->nmon; i++)
    if (&b->mon[i] != self && b->mon[i].alive && !b->mon[i].gone) return true;
  return false;
}

/* ---------------------------------------------------------------- Snowdrake, Chilldrake */
static void act_snowdrake(Battle *b, Monster *m, int act, char *t, int cap) {
  mark_acted(m);
  if (act == 0) {   /* Heckle */
    if (rnd_n(6) == 0) {
      snprintf(t, (size_t)cap, "* You tell Snowdrake their jokes will never land.\n* Crushed, they slink away...");
      m->gone = true, m->fade = 1;
    } else {
      snprintf(t, (size_t)cap, "* You boo Snowdrake.");
      m->state = 3;
    }
  } else if (act == 1) {   /* Laugh */
    snprintf(t, (size_t)cap, m->state == 1 ? "* You laugh at Snowdrake's pun." : "* You laugh at nothing in particular.");
    if (m->state == 1) m->spare = true, m->state = 2;
    else m->state = 4;
  } else {   /* Joke */
    if (others_here(b, m)) {
      snprintf(t, (size_t)cap, "* You make an ice pun.\n* The other monsters think it's hilarious...");
      others_laugh(b, m);
      m->state = 5;
    } else {
      snprintf(t, (size_t)cap, "* You make an ice pun.\n* Snowdrake isn't impressed.");
      m->state = 6;
    }
  }
}
static const char *talk_snowdrake(Battle *b, Monster *m) {
  int s = m->state;
  bool acted = acted_on(m);
  clear_acted(m);
  if (s == 2) { m->state = 7; return "See!? Laughs!\nDad was\nwrong!"; }
  if (s == 3) { m->state = 0; return "THIS won't\nbe funny\neither!"; }
  if (s == 4) { m->state = 0; return "What are YOU\nlaughin' at?!?"; }
  if (s == 5) { m->state = 0; return "That's not\nlike me..."; }
  if (s == 6) { m->state = 0; return PICK(((const char *const[]){"Is that s'posed\nto be funny?", "Ha... Ha...\nNice try.", "I've heard\nthat one."})); }
  if (s == 7) return "Thanks, you're\nall great.";
  /* a neutral turn: his next pun (laugh at it to spare him) */
  if (!acted || s == 1) m->state = 1;
  static const char *const puns[] = {"\"Ice\" to\nmeet you.", "Ice puns are\n\"snow\" problem", "Fights you in\n\"cold\" blood.",
                                     "Better not\nsnow \"flake\"\nout!", "My fave ice\ncereals:\n\"frosted\"", "\"Chill\"\nout...",
                                     "M.. m..\nmacaroni and\n\"freeze\""};
  return PICK(puns);
}
static int attack_drake(Battle *b, Monster *m) { (void)b, (void)m; return rnd_n(2) ? ATK_DRAKE_ARCS : ATK_DRAKE_LINES; }

static void act_chilldrake(Battle *b, Monster *m, int act, char *t, int cap) {
  mark_acted(m);
  if (act == 0) {   /* Clash */
    snprintf(t, (size_t)cap, "* You tell Chilldrake it's all wrong.");
    m->state = 3;
  } else if (act == 1) {   /* Agree */
    if (m->state == 1) {
      snprintf(t, (size_t)cap, "* You agree with Chilldrake.\n* It seems to get very uncomfortable...");
      m->spare = true, m->state = 2;
    } else {
      snprintf(t, (size_t)cap, "* You agree with Chilldrake before it gives its speech.");
      m->state = 4;
    }
  } else {   /* Joke */
    if (others_here(b, m)) {
      snprintf(t, (size_t)cap, "* You make fun of Chilldrake.\n* The other monsters think it's hilarious...");
      others_laugh(b, m);
    } else {
      snprintf(t, (size_t)cap, "* You make fun of Chilldrake.");
    }
    m->state = 5;
  }
}
static const char *talk_chilldrake(Battle *b, Monster *m) {
  int s = m->state;
  bool acted = acted_on(m);
  clear_acted(m);
  (void)b;
  if (s == 2) { m->state = 6; return "To tell you\nthe truth..."; }
  if (s == 3) { m->state = 0; return "WRONG! Let's\nfight and die."; }
  if (s == 4) { m->state = 0; return "You don't\nknow my\ncause!"; }
  if (s == 5) { m->state = 0; return "Only Snowy\ncan do that."; }
  if (s == 6) return "Do you...\nreally...";
  if (!acted || s == 1) m->state = 1;
  static const char *const q[] = {"Brush my\nteeth? No way\nin heck!", "NEVER do your\nhomework !!", "No bedtime!\nOnly DEADTIME",
                                  "*turns music\nup all the\nway*"};
  return PICK(q);
}

/* ---------------------------------------------------------------- Ice Cap */
static void act_icecap(Battle *b, Monster *m, int act, char *t, int cap) {
  (void)b;
  mark_acted(m);
  if (m->spr_override) {   /* "Ice" now */
    snprintf(t, (size_t)cap, act == 0 ? "* You inform Ice Cap that it still looks fine..." : "* ...");
    return;
  }
  if (act == 0) {   /* Compliment */
    snprintf(t, (size_t)cap, "* You inform Ice Cap that it has a great hat!");
    m->state = 1;
  } else if (act == 1) {   /* Ignore */
    m->state2 = (int16_t)(m->state2 + 1);
    if ((m->state2 & 0xFF) >= 2) {
      snprintf(t, (size_t)cap, "* You continue not looking at Ice Cap's hat.\n* It seems defeated...");
      m->spare = true;
    } else {
      snprintf(t, (size_t)cap, "* You manage to tear your eyes away from Ice Cap's hat.\n* It looks annoyed...");
    }
    m->state = 2;
  } else {   /* Steal */
    if (m->spare) {
      snprintf(t, (size_t)cap, "* You tried to steal Ice Cap's hat...\n* And succeeded!\n* (It melts in your hands...)");
      m->spr_override = SPR_ICECAP_HATLESS;
      m->exp = 25, m->gold = 40, m->spare_gold = 35, m->at = 1, m->df = -100;
    } else {
      snprintf(t, (size_t)cap, "* You tried to steal Ice Cap's hat...\n* ... but it's not weakened enough!");
      m->state = 3;
    }
  }
}
static const char *talk_icecap(Battle *b, Monster *m) {
  (void)b;
  clear_acted(m);
  if (m->spr_override) return "I... I...";
  int s = m->state;
  m->state = 0;
  if (s == 9) return "Haha! That was\nACTUALLY\nfunny!";
  if (s == 1) return PICK(((const char *const[]){"DUH! Who\nDOESN'T know?", "Envious? TOO\nBAD!", "My hat's too\nloud for me to\nhear you."}));
  if (s == 2) return (m->state2 & 0xFF) >= 2 ? "Fine!!! I\ndon't care!!!" : "HELLO??? My\nhat's up here.";
  if (s == 3) return "I KNEW IT!!!\nTHIEF!!";
  static const char *const q[] = {"Where's YOUR\nhat?", "Your head\nlooks so\n...NAKED", "What a great\nhat! (Mine)",
                                  "Snow? No! It's\nhat residue.", "I just looove\nmy hat. OK?"};
  return PICK(q);
}
static int attack_icecap(Battle *b, Monster *m) {
  (void)b;
  return m->spr_override ? ATK_IDLE : rnd_n(2) ? ATK_ICECAP_HATS : ATK_ICECAP_WAVES;
}
static const char *flavor_icecap(Battle *b, Monster *m) {
  (void)b;
  if (m->spr_override) return "* 'Ice Cap' is no more.";
  if (m->spare) return "* Ice Cap is desperate for attention.";
  return NULL;
}

/* ---------------------------------------------------------------- Jerry */
static void act_jerry(Battle *b, Monster *m, int act, char *t, int cap) {
  (void)act;
  snprintf(t, (size_t)cap, "* You and the other monsters ditch Jerry when it looks away!");
  m->gone = true, m->fade = 1, m->spare_gold = 0;
  b->state[7] = 1;
}
static const char *talk_jerry(Battle *b, Monster *m) {
  if (m->state == 9) { m->state = 0; return "Hahaha! Say\nsomething else\nfunny!"; }
  if (!others_here(b, m)) return PICK(((const char *const[]){"Well... can YOU\ngive me a ride\nhome?", "Awkwarrrd.", "The wi-fi here\nsucks."}));
  static const char *const q[] = {"Guys, it's\nCOLD. Does\nANYONE care?", "Why are we\ndoing this?\nWhat a fail.",
                                  "Wow, you guys\nSUCK at this.", "Ka-SIGH."};
  return PICK(q);
}
static bool hit_jerry(Battle *b, Monster *m, int dmg) {
  (void)b;
  /* it takes a fifth of every hit */
  m->hp = (int16_t)(m->hp - (dmg + 4) / 5);
  m->hurt = 30;
  if (m->hp <= 0) m->hp = 0, m->killed = true;
  return true;
}

/* ---------------------------------------------------------------- Gyftrot */
static const char *const gyft_decor[] = {
  "* You remove the striped cane that says \"I use this tiny cane to walk\" on it.",
  "* You remove the box of non-dog-related raisins.",
  "* You remove the lenticular bookmark of a smug teen winking.",
  "* You remove the barbed wire made of pipe cleaners.",
  "* You remove a small, confused dog.",
  "* You remove a stocking filled with chicken nuggets."};
static void act_gyftrot(Battle *b, Monster *m, int act, char *t, int cap) {
  (void)b;
  mark_acted(m);
  /* state: decorations left (3 at first, kept as 3 - state2 & 7), state: what it says next */
  int left = 3 - (m->state2 & 7);
  if (act == 0) {   /* Decorate */
    snprintf(t, (size_t)cap, "* You add some googly eyes you found on the ground.");
    m->state = left == 0 ? 5 : 1;
    if (left == 0) m->state2 = (int16_t)(m->state2 | 0x10);   /* decorated again: no gift now */
  } else if (act == 1) {   /* Undecorate */
    if (left > 0) {
      snprintf(t, (size_t)cap, "%s", gyft_decor[(m->state2 & 7) + rnd_n(3)]);
      m->state2 = (int16_t)(m->state2 + 1);
      m->state = left == 1 ? 3 : 2;
      if (left == 1) m->spare = true;
    } else {
      snprintf(t, (size_t)cap, "* You try to undecorate...?");
      m->state = 4;
    }
  } else {   /* Gift */
    if (left > 0 || (m->state2 & 0x10)) {
      snprintf(t, (size_t)cap, "* Gyftrot refuses your gift.");
      m->state = left > 0 ? 6 : 7;
    } else {
      snprintf(t, (size_t)cap, "* You give Gyftrot a gift.");
      m->spare_gold = 75, m->state = 8;
    }
  }
}
static const char *talk_gyftrot(Battle *b, Monster *m) {
  (void)b;
  clear_acted(m);
  int s = m->state;
  m->state = 0;
  switch (s) {
    case 1: return "GOSHDARN\nTEENAGE GOOGLY\nEYES!!!";
    case 2: return "That's a\nlittle better.";
    case 3: return "A weight has\nbeen lifted.";
    case 4: return "Er... Those\nare real.";
    case 5: return "I WAS STARTING\nTO TRUST YOU!!!";
    case 6: return "How do I know\nit's not a\ntrick?";
    case 7: return "I don't want\nyour gift!";
    case 8: return "Aw, you\nshouldn'\nhave...";
  }
  if (m->spare) return "Thanks.";
  static const char *const q[] = {"Is this funny\nto you?", "Don't put any\nmore on me!", "Leave me\nalone...",
                                  "Hohoho! Go\nahead and\nlaugh...", "Get this off\nof me..."};
  return PICK(q);
}
static int attack_gyftrot(Battle *b, Monster *m) { (void)b, (void)m; return rnd_n(2) ? ATK_GYFT_PRESENTS : ATK_GYFT_SNOW; }

/* ---------------------------------------------------------------- the dogs: the Stick spares them */
static bool stick_dog(Battle *b, Monster *m, int item, char *t, int cap) {
  if (item != ITEM_STICK) return false;
  bool pair = m->def == mon_def(MON_DOGAMY) || m->def == mon_def(MON_DOGARESSA);
  snprintf(t, (size_t)cap, pair ? "* You threw the stick and the dogs ran to get it.\n* You played fetch for a while."
                                : "* You threw the stick and the dog ran to get it.\n* You played fetch for a while.");
  for (int i = 0; i < b->nmon; i++)
    if (b->mon[i].alive && !b->mon[i].gone) b->mon[i].spare = true, b->mon[i].state = 20;
  if (m->def == mon_def(MON_DOGGO)) snow_doggo_stick();
  return true;
}

/* Doggo: he can be pet once he's survived an attack without seeing you move */
static void act_doggo(Battle *b, Monster *m, int act, char *t, int cap) {
  (void)act;
  bool calm = b->turn >= 1 && sv.hp >= m->state2;
  if (!calm && !m->spare) {
    snprintf(t, (size_t)cap, "* Doggo is too suspicious of your movements.");
    m->state = 1;
    return;
  }
  m->acts[5]++;
  snprintf(t, (size_t)cap, "* You pet Doggo.");
  m->spare = true, m->state = 2;
  if (m->acts[5] >= 5) m->df = 0;
}
static const char *talk_doggo(Battle *b, Monster *m) {
  int s = m->state;
  m->state = 0;
  if (s == 20) return "HUH!!! A FUN\nSTICK APPEARS!";
  if (s == 2) {
    switch (m->acts[5]) {
      case 1: return "WHAT!!! I'VE\nBEEN PET!!!|PET?|PAT?|POT?";
      case 2: return "WHERE'S THAT\nCOMING FROM!";
      case 3: return "THERE'S NO END\nTO IT!!";
      case 4: return "WELL, THIS IS\nTHOROUGH !!!";
      default: return "OK. That's\nenough.";
    }
  }
  if (b->turn == 0) return "Don't move an\ninch!";
  if (sv.hp < m->state2) return "MOVING?";
  return "Will it move\nthis time?";
}
static int attack_doggo(Battle *b, Monster *m) {
  (void)b;
  m->state2 = sv.hp;   /* (unhurt by this attack: calm enough to pet) */
  return ATK_DOGGO_SWORD;
}
static const char *flavor_doggo(Battle *b, Monster *m) {
  if (m->spare) return "* Doggo has been pet.";
  if (b->turn >= 1 && sv.hp >= m->state2) return "* Doggo can't seem to find anything.";
  return NULL;
}

/* Lesser Dog: a pet spares it; more pets raise its head ever higher */
static void act_lesserdog(Battle *b, Monster *m, int act, char *t, int cap) {
  (void)b, (void)act;
  static const char *const pet[] = {
    "* You barely lifted your hand and Lesser Dog got excited.",
    "* You lightly touched the Dog.\n* It's already overexcited...",
    "* You pet the Dog.\n* It raises its head up to meet your hand.",
    "* You pet the Dog.\n* It was a good Dog.",
    "* You pet the Dog.\n* Its excitement knows no bounds.",
    "* Critical pet!\n* Dog excitement increased.",
    "* You have to jump up to pet the Dog.",
    "* You don't even pet it.\n* It gets more excited.",
    "* There is no way to stop this madness.",
    "* Lesser Dog enters the realm of the clouds.",
    "* You call the Dog but it is too late.\n* It cannot hear you.",
    "* ...",
    "* You can reach Lesser Dog again."};
  int n = m->acts[5]++;
  if (n < 13) snprintf(t, (size_t)cap, "%s", pet[n]);
  else if (n < 19) snprintf(t, (size_t)cap, "* You pet Lesser Dog.");
  else if (n < 33) snprintf(t, (size_t)cap, "* It's possible that you may have a problem.");
  else if (n < 44) snprintf(t, (size_t)cap, "* Lesser Dog is unpettable but appreciates the attempt.");
  else snprintf(t, (size_t)cap, "* Perhaps mankind was not meant to pet this much.");
  m->spare = true;
  m->state = 1;
}
static const char *talk_lesserdog(Battle *b, Monster *m) {
  (void)b;
  int n = m->acts[5];
  if (m->state == 20) { m->state = 0; return "(Wag wag)"; }
  if (m->state == 1) {
    m->state = 0;
    if (n <= 3) return "(Pant! Pant!)";
    if (n <= 6) return "(Excited\nnoises)";
    static const char *const q[] = {"(Motor\nrevving)", "(Plane\ntakeoff)", "(Kettle\nwhistle)", "(...)", "(Faraway\nbark)", "(...)", "(Bark)"};
    if (n <= 13) return q[n - 7];
    return "(Pant pant)";
  }
  static const char *const q[] = {"(Pant pant)", "(Wag wag)", "(Thinks of\nfood)", "(Tiny bark)"};
  return PICK(q);
}
static int attack_lesserdog(Battle *b, Monster *m) {
  (void)b;
  if (m->acts[5] >= 6) return ATK_IDLE;   /* (a critical pet: too excited to attack) */
  return rnd_n(2) ? ATK_LDOG_JUMP : ATK_LDOG_SPEARS;
}
static void draw_lesserdog(Battle *b, Monster *m, int x, int y) {
  (void)b;
  /* each pet lifts its head: its neck grows */
  int neck = min_i(m->acts[5], 13) * 7;
  if (m->acts[5] >= 34) neck = 0;
  if (neck) gfx_rect(x - 4, y - 46 - neck, 9, neck + 4, C_WHITE, 255);
  gfx_sprite(m->hurt ? SPR_LESSERDOG_HURT : SPR_LESSERDOG_B, m->hurt ? 0 : (int)(g_frame / 6), x, y - neck * 0, 0, 255);
}

/* the Dogi: roll in the snow, let them re-sniff you, then pet them both */
static void act_dogi(Battle *b, Monster *m, int act, char *t, int cap) {
  bool husband = m->def == mon_def(MON_DOGAMY);
  /* b->state[0]: rolled around, [1]: re-sniffed after, [2], [3]: Dogamy, Dogaressa pet */
  if (act == 0) {   /* Pet */
    if (!b->state[1]) {
      snprintf(t, (size_t)cap, husband ? "* The Dogs are too suspicious of your smell." : "* The Dogaressa is too suspicious of your smell.");
    } else {
      snprintf(t, (size_t)cap, husband ? "* You pet Dogamy." : "* You pet the Dogaressa.");
      b->state[husband ? 2 : 3] = 1;
      m->state = 1;
      /* both pet: their minds expanded */
      if (b->state[2] && b->state[3])
        for (int i = 0; i < b->nmon; i++) b->mon[i].spare = true;
      else if (count_alive() == 1)
        m->spare = true;
    }
  } else if (act == 1) {   /* Re-sniff */
    if (b->state[1]) snprintf(t, (size_t)cap, "* The Dogs already know you smell fine.");
    else if (b->state[0]) snprintf(t, (size_t)cap, "* The Dogs sniff you again...\n* After rolling in the dirt, you smell all right!"), b->state[1] = 1;
    else snprintf(t, (size_t)cap, "* The Dogs sniff you again...\n* But you smell just as weird as before!");
  } else {   /* Roll Around */
    snprintf(t, (size_t)cap, "* You roll around in the dirt and snow.\n* You smell like a weird puppy.");
    b->state[0] = 1;
  }
}
static const char *talk_dogamy(Battle *b, Monster *m) {
  if (count_alive() == 1) return "...";
  if (b->state[2] && b->state[3]) return "Dogs can pet\nother dogs???";
  if (m->state == 20) return "A stick!";
  if (b->state[1]) return "Smells like a\nlittle puppy!";
  if (b->state[0]) return "Hmm? Huh?";
  static const char *const q[] = {"Hmm? Is that\na...", "Huh? What's\nthat smell?", "Don't touch\nmy wife!"};
  return PICK(q);
}
static const char *talk_dogaressa(Battle *b, Monster *m) {
  if (count_alive() == 1) return "...";
  if (b->state[2] && b->state[3]) return "A new world\n... !";
  if (m->state == 20) return "Fetch!!!";
  if (b->state[1]) return "(A puppy?)";
  static const char *const q[] = {"(I smell it\ntoo...)", "(Hands off\nmy hubby!)", "(Weird smell...)"};
  return PICK(q);
}
static int attack_dogi(Battle *b, Monster *m) {
  /* alone, each has its own: Dogamy's sad hearts, Dogaressa's angry axes; together they take turns */
  if (count_alive() == 1) return m->def == mon_def(MON_DOGAMY) ? ATK_DOGI_HEARTS : ATK_DOGI_AXES;
  return b->turn % 2 ? ATK_DOGI_HEARTS : ATK_DOGI_AXES;
}

/* Greater Dog: call it (or ignore it), pet it, play with it, pet it twice */
static void act_greaterdog(Battle *b, Monster *m, int act, char *t, int cap) {
  (void)b;
  int s = m->state & 0xFF;
  if (act == 0) {   /* Pet */
    if (s == 1) {
      snprintf(t, (size_t)cap, "* Greater Dog flops into your lap for pets and dozes off...|* ...then bolts awake, thrilled!");
      m->state = 2, m->df = -20;
    } else if (s == 3) {
      snprintf(t, (size_t)cap, "* You pet the dog. It leans all its weight on you.\n* More pets are needed...!");
      m->state = 4;
    } else if (s == 4) {
      snprintf(t, (size_t)cap, "* One last, perfect pet.\n* The dog rolls over, completely satisfied.");
      m->state = 5, m->spare = true;
    } else if (s == 5) {
      snprintf(t, (size_t)cap, "* Tummy rubs are forbidden.");
    } else if (s == 2) {
      snprintf(t, (size_t)cap, "* Greater Dog's excitement is creating a power field that prevents petting.");
    } else {
      snprintf(t, (size_t)cap, "* Greater Dog is too far away to pet.\n* You just pet the air.");
    }
  } else if (act == 1) {   /* Beckon */
    if (s == 0) snprintf(t, (size_t)cap, "* You call the Greater Dog.\n* It bounds towards you, flecking slobber into your face."), m->state = 1;
    else snprintf(t, (size_t)cap, "* The Greater Dog's ears perk up.\n* Nothing else happens.");
  } else if (act == 2) {   /* Play */
    if (s == 2) {
      snprintf(t, (size_t)cap, "* You toss a snowball. It falls apart.|* Greater Dog fetches ALL the snow anyway.\n* Worn out, it rests its head on you...");
      m->state = 3;
    } else if (s >= 3) {
      snprintf(t, (size_t)cap, "* Greater Dog is too tired to play.");
    } else {
      snprintf(t, (size_t)cap, "* Greater Dog is not excited enough to play with.");
    }
  } else {   /* Ignore */
    m->state2 = (int16_t)(m->state2 + 1);
    if (m->state2 >= 4) {
      snprintf(t, (size_t)cap, "* Greater Dog decides you are too boring.");
      m->gone = true, m->fade = 1;
    } else {
      snprintf(t, (size_t)cap, "* Greater Dog inches closer.");
      if (s == 0) m->state = 1;
    }
  }
}
static const char *flavor_greaterdog(Battle *b, Monster *m) {
  (void)b;
  switch (m->state & 0xFF) {
    case 1: return "* Greater Dog just wants affection.";
    case 2: return "* Greater Dog is patting the ground with its front paws.";
    case 3: return "* Greater Dog wants some TLC.";
    case 4: return "* Pet capacity is 40-percent.";
    case 5: return "* Greater Dog is contented.";
  }
  return NULL;
}
static int attack_greaterdog(Battle *b, Monster *m) { (void)b, (void)m; return rnd_n(2) ? ATK_GDOG_SPEAR : ATK_GDOG_BARK; }

/* ---------------------------------------------------------------- Glyde */
static void act_glyde(Battle *b, Monster *m, int act, char *t, int cap) {
  (void)b;
  /* state: applause (to 2), state2: nothings after them */
  if (act == 0) {
    m->state++;
    snprintf(t, (size_t)cap, m->state == 1 ? "* You clap really sloppily.\n* Glyde sucks up your praise like a vacuum cleaner."
                                           : "* You clap like a gorilla.\n* Glyde is becoming addicted to your praise.");
    m->state2 = 0;
  } else if (act == 1) {
    if (m->state >= 2 && m->state2 >= 2) {
      snprintf(t, (size_t)cap, "* You boo loudly.\n* Glyde leaves to look elsewhere for praise.");
      m->gone = true, m->fade = 1;
    } else {
      snprintf(t, (size_t)cap, "* You boo... but haters only make Glyde stronger.\n* Glyde ATTACK UP+DEFENSE DOWN.");
      m->at = (int16_t)(m->at + 2), m->df = (int16_t)(m->df - 10);
    }
  } else {
    if (m->state < 2) {
      snprintf(t, (size_t)cap, "* You do nothing.\n* No effect.");
    } else {
      m->state2++;
      if (m->state2 >= 3) snprintf(t, (size_t)cap, "* You do nothing.\n* Glyde leaves to look elsewhere for praise."), m->gone = true, m->fade = 1;
      else if (m->state2 == 2) snprintf(t, (size_t)cap, "* You do nothing.\n* Glyde looks desperate for attention.");
      else snprintf(t, (size_t)cap, "* You do nothing.\n* Glyde looks disappointed you aren't paying attention.");
    }
  }
}
static const char *talk_glyde(Battle *b, Monster *m) {
  (void)b;
  if (m->state == 1 && m->state2 == 0) return "OK! I rule!\nI admit it!";
  if (m->state >= 2 && m->state2 == 0) return "What else do\nyou have to\nsay!?";
  if (m->state >= 2 && m->state2 == 1) return "Eh? You forgot\nto clap.";
  if (m->state >= 2 && m->state2 == 2) return "HELLO?? I'M\nRIGHT HERE!!!";
  static const char *const q[] = {"How great\nI am.", "Look. Watch.\nObserve.", "Wow. Check out\nmy pecs.", "Sorry... for\nNOTHING\n*ollies*"};
  return PICK(q);
}
static int attack_glyde(Battle *b, Monster *m) { (void)b, (void)m; return ATK_GLYDE_STARS; }

/* ---------------------------------------------------------------- the table */
extern const MonDef papyrus_def;   /* papyrus.c */
static const MonDef snow_defs[MON_SNOW_COUNT - MON_RUINS_COUNT] = {
  [MON_SNOWDRAKE - MON_RUINS_COUNT] = {
    .name = "Snowdrake", .spare_gold = 12, .hp = 74, .at = 6, .df = 2, .exp = 22, .gold = 18, .spr_idle = SPR_SNOWDRAKE_B,
    .spr_hurt = SPR_SNOWDRAKE_HURT, .idle_speed = 8, .check = "This teen comedian fights to keep a captive audience.",
    .acts = {"Heckle", "Laugh", "Joke"}, .nacts = 3,
    .neutral = {"* Snowdrake is assessing the crowd.", "* Snowdrake is practicing its next pun.",
                "* Snowdrake is smiling at its own bad joke."},
    .act = act_snowdrake, .talk = talk_snowdrake, .attack = attack_drake},
  [MON_ICECAP - MON_RUINS_COUNT] = {
    .name = "Ice Cap", .spare_gold = 13, .hp = 48, .at = 6, .df = 0, .exp = 17, .gold = 17, .spr_idle = SPR_ICECAP_B,
    .spr_hurt = SPR_ICECAP_HURT, .idle_speed = 6, .check = "This teen wonders why it isn't named 'Ice Hat.'",
    .acts = {"Compliment", "Ignore", "Steal"}, .nacts = 3,
    .neutral = {"* It's snowing dandruff.", "* Ice Cap also wants a hat for its nose.",
                "* Ice Cap makes sure its hat is still there.", "* Ice Cap is thinking about a certain article of clothing.",
                "* Here comes that new clothes smell."},
    .low_hp = "* Ice Cap's hat is loose.", .act = act_icecap, .talk = talk_icecap, .attack = attack_icecap,
    .flavor = flavor_icecap},
  [MON_GYFTROT - MON_RUINS_COUNT] = {
    .name = "Gyftrot", .spare_gold = 20, .hp = 114, .at = 7, .df = 3, .exp = 35, .gold = 30, .spr_idle = SPR_GYFTROT_B,
    .spr_hurt = SPR_GYFTROT_HURT, .idle_speed = 8, .check = "Some teens \"decorated\" it as a prank.",
    .acts = {"Decorate", "Undecorate", "Gift"}, .nacts = 3,
    .neutral = {"* Gyftrot laments its lack of hands.", "* Gyftrot eyes you with suspicion.",
                "* Gyftrot distrusts your youthful demeanor.", "* Ah, the scent of fresh pine needles.",
                "* Gyftrot tries vainly to remove its decorations."},
    .act = act_gyftrot, .talk = talk_gyftrot, .attack = attack_gyftrot},
  [MON_DOGGO - MON_RUINS_COUNT] = {
    .name = "Doggo", .spare_gold = 30, .hp = 70, .at = 6, .df = 1, .exp = 30, .gold = 30, .spr_idle = SPR_DOGGO_B, .spr_hurt = SPR_DOGGO_HURT,
    .idle_speed = 6, .check = "Easily excited by movement. Hobbies include: squirrels.", .acts = {"Pet"}, .nacts = 1,
    .neutral = {"* Doggo is too suspicious of your movements."},
    .act = act_doggo, .talk = talk_doggo, .attack = attack_doggo, .flavor = flavor_doggo, .on_item = stick_dog},
  [MON_LESSERDOG - MON_RUINS_COUNT] = {
    .name = "Lesser Dog", .spare_gold = 15, .hp = 60, .at = 6, .df = 0, .exp = 18, .gold = 15, .spr_idle = SPR_LESSERDOG_B,
    .spr_hurt = SPR_LESSERDOG_HURT, .idle_speed = 6, .check = "Wields a stone dogger made of pomer-granite.",
    .acts = {"Pet", "Pet", "Pet", "Pet", "Pet"}, .nacts = 5,
    .neutral = {"* Lesser Dog cocks its head to one side.", "* Lesser Dog thinks your weapon is a dog treat.",
                "* Lesser Dog is really not paying attention.", "* Smells like dog chow."},
    .act = act_lesserdog, .talk = talk_lesserdog, .attack = attack_lesserdog, .draw = draw_lesserdog,
    .on_item = stick_dog},
  [MON_DOGAMY - MON_RUINS_COUNT] = {
    .name = "Dogamy", .spare_gold = 20, .hp = 108, .at = 6, .df = 4, .exp = 30, .gold = 25, .spr_idle = SPR_DOGAMY_B, .spr_hurt = SPR_DOGAMY_B,
    .check = "Husband of Dogaressa.\n* Knows only what he smells.", .acts = {"Pet", "Re-sniff", "Roll Around"}, .nacts = 3,
    .neutral = {"* The Dogs keep shifting their axes to protect each other.", "* The Dogs are re-evaluating your smell.",
                "* The Dogs are practicing for the next couples contest.",
                "* The Dogs are saying sickly sweet things to each other."},
    .act = act_dogi, .talk = talk_dogamy, .attack = attack_dogi, .on_item = stick_dog},
  [MON_DOGARESSA - MON_RUINS_COUNT] = {
    .name = "Dogaressa", .spare_gold = 20, .hp = 108, .at = 6, .df = 4, .exp = 30, .gold = 25, .spr_idle = SPR_DOGARESSA_B,
    .spr_hurt = SPR_DOGARESSA_B, .check = "This dog's wife.\n* Only knows what she smells.", .acts = {"Pet", "Re-sniff", "Roll Around"},
    .nacts = 3,
    .neutral = {"* The Dogs keep shifting their axes to protect each other.", "* The Dogs are re-evaluating your smell."},
    .act = act_dogi, .talk = talk_dogaressa, .attack = attack_dogi, .on_item = stick_dog},
  [MON_GREATERDOG - MON_RUINS_COUNT] = {
    .name = "Greater Dog", .spare_gold = 40, .hp = 105, .at = 6, .df = 4, .exp = 80, .gold = 60, .spr_idle = SPR_GREATERDOG_B,
    .spr_hurt = SPR_GREATERDOG_HURT, .idle_speed = 10, .check = "It's so excited that it thinks fighting is just play.",
    .acts = {"Pet", "Beckon", "Play", "Ignore"}, .nacts = 4,
    .neutral = {"* Greater Dog is watching you intently.", "* Greater Dog is waiting for your command.",
                "* It smells like freshly-squeezed puppy juice.", "* Greater Dog is seeking affection."},
    .quotes = {"(Bark!)", "(Pant pant)", "(Wag wag)"},
    .low_hp = "* Greater Dog is panting slowly.", .act = act_greaterdog, .attack = attack_greaterdog,
    .flavor = flavor_greaterdog, .on_item = stick_dog},
  [MON_JERRY - MON_RUINS_COUNT] = {
    .name = "Jerry", .hp = 80, .at = 1, .df = 8, .exp = 1, .gold = 55, .spr_idle = SPR_JERRY_B, .spr_hurt = SPR_JERRY_HURT,
    .idle_speed = 6, .dy = 10, .check = "Everyone knows Jerry.\n* Makes attacks 2 seconds longer.", .acts = {"Ditch"},
    .nacts = 1,
    .neutral = {"* Jerry eats powdery food and licks its hands loudly.", "* Jerry sneezes without covering its nose.",
                "* Jerry tells everyone it has to go to the bathroom.", "* Jerry lets out a yawn.",
                "* Smells like....... Jerry."},
    .low_hp = "* Jerry is wounded.", .act = act_jerry, .talk = talk_jerry, .on_hit = hit_jerry},
  [MON_CHILLDRAKE - MON_RUINS_COUNT] = {
    .name = "Chilldrake", .spare_gold = 12, .hp = 74, .at = 6, .df = 2, .exp = 22, .gold = 18, .spr_idle = SPR_CHILLDRAKE_B,
    .spr_hurt = SPR_CHILLDRAKE_B, .idle_speed = 8, .check = "Rebels against everything!!\n* Looking for its friend Snowy.",
    .acts = {"Clash", "Agree", "Joke"}, .nacts = 3,
    .neutral = {"* Chilldrake starts a one-monster riot.", "* Chilldrake is chanting an anarchist spell.",
                "* Chilldrake is eating its own homework.", "* It smells like \"Ice\" scented body-spray.",
                "* Chilldrake is wondering where Snowy went."},
    .act = act_chilldrake, .talk = talk_chilldrake, .attack = attack_drake},
  [MON_GLYDE - MON_RUINS_COUNT] = {
    .name = "Glyde", .spare_gold = 120, .hp = 220, .at = 9, .df = -20, .exp = 100, .gold = 140, .spr_idle = SPR_GLYDE_B,
    .spr_hurt = SPR_GLYDE_HURT, .idle_speed = 6, .dy = -6, .check = "Refuses to give more details about its statistics.",
    .acts = {"Applaud", "Boo", "Nothing"}, .nacts = 3,
    .neutral = {"* Glyde does fancy flips.", "* Glyde is giving itself a high five.\n* ... somehow.",
                "* Glyde sees its reflection and gets jealous.", "* An arrogant-smelling wind blows through."},
    .act = act_glyde, .talk = talk_glyde, .attack = attack_glyde},
};
const MonDef *snow_mon_def(int id) {
  if (id >= MON_SNOW_COUNT) return wf_mon_def(id);
  if (id == MON_PAPYRUS) return &papyrus_def;
  return &snow_defs[id - MON_RUINS_COUNT];
}

static const Form snow_forms[FORM_SNOW_COUNT - FORM_RUINS_COUNT] = {
  [FORM_SNOWDRAKE - FORM_RUINS_COUNT] = {{MON_SNOWDRAKE}, 1, "* Snowdrake flutters forth!"},
  [FORM_ICECAP - FORM_RUINS_COUNT] = {{MON_ICECAP}, 1, "* Icecap struts into view."},
  [FORM_LESSERDOG - FORM_RUINS_COUNT] = {{MON_LESSERDOG}, 1, "* Lesser Dog appears."},
  [FORM_CHILLDRAKE - FORM_RUINS_COUNT] = {{MON_CHILLDRAKE}, 1, "* Chilldrake saunters up!"},
  [FORM_ICECAP_JERRY - FORM_RUINS_COUNT] = {{MON_ICECAP, MON_JERRY}, 2, "* Icecap appears.\n* Jerry came too."},
  [FORM_ICECAP_JERRY_SNOWDRAKE - FORM_RUINS_COUNT] = {{MON_SNOWDRAKE, MON_ICECAP, MON_JERRY}, 3,
                                                      "* Icecap and Snowdrake confront you, sighing.\n* Jerry."},
  [FORM_ICECAP_JERRY_CHILLDRAKE - FORM_RUINS_COUNT] = {{MON_CHILLDRAKE, MON_ICECAP, MON_JERRY}, 3,
                                                       "* Icecap and Chilldrake confront you, sighing.\n* Jerry."},
  [FORM_GYFTROT - FORM_RUINS_COUNT] = {{MON_GYFTROT}, 1, "* Gyftrot confronts you!"},
  [FORM_GLYDE - FORM_RUINS_COUNT] = {{MON_GLYDE}, 1, "* Glyde swooped in!"},
  [FORM_DOGGO - FORM_RUINS_COUNT] = {{MON_DOGGO}, 1, "* Doggo blocks the way!"},
  [FORM_DOGI - FORM_RUINS_COUNT] = {{MON_DOGAMY, MON_DOGARESSA}, 2, "* Dogi assault you!"},
  [FORM_GREATERDOG - FORM_RUINS_COUNT] = {{MON_GREATERDOG}, 1, "* It's the Greater Dog."},
  [FORM_PAPYRUS - FORM_RUINS_COUNT] = {{MON_PAPYRUS}, 1, "* Papyrus blocks the way!"},
};
const Form *snow_form(int form) { return form >= FORM_SNOW_COUNT ? wf_form(form) : &snow_forms[form - FORM_RUINS_COUNT]; }

/* random encounters: the wiki's pools (no more of them once Papyrus is spared with no one killed here) */
int snow_encounter(int room) {
  if (flag(F_PAPYRUS) == 1 && !flag(F_KILLS_SNOWDIN)) return -1;
  int drake = flag(F_SNOWDRAKE_DEAD) ? FORM_CHILLDRAKE : FORM_SNOWDRAKE;
  bool lesser = !flag(F_LESSERDOG) && rnd_n(4) == 0;
  switch (room) {
    case ROOM_BOXROAD: case ROOM_PAPSENTRY: case ROOM_ICEFIELD:
      if (lesser) return FORM_LESSERDOG;
      return rnd_n(2) ? drake : FORM_ICECAP;
    case ROOM_SNOWPUZZLE: case ROOM_DANGERBRIDGE:
      if (lesser) return FORM_LESSERDOG;
      if (rnd_n(2)) return FORM_ICECAP_JERRY;
      return flag(F_SNOWDRAKE_DEAD) ? FORM_ICECAP_JERRY_CHILLDRAKE : FORM_ICECAP_JERRY_SNOWDRAKE;
    case ROOM_SOUTHCLIFF: return FORM_GYFTROT;
    case ROOM_DOORROOM: return FORM_GLYDE;
  }
  return -1;
}

/* ---------------------------------------------------------------- attacks */
static int bx0(void) { return bt.x0 + 3; }
static int bx1(void) { return bt.x1 - 3; }
static int by0(void) { return bt.y0 + 3; }
static int by1(void) { return bt.y1 - 3; }
#define P(v) ((v) * FX)
static Bullet *shot(uint16_t spr, int x, int y, int vx, int vy) {
  Bullet *b = bullet_new(spr, P(x), P(y), vx, vy);
  if (b) b->owner = (uint8_t)bt.atk_owner;
  return b;
}
/* aimed at the SOUL, sp (1/16 px) a frame */
static void aim(Bullet *b, int sp) {
  int dx = bt.sx - b->x, dy = bt.sy - b->y;
  int d = abs_i(dx) + abs_i(dy);
  if (d) b->vx = dx * sp / d, b->vy = dy * sp / d;
}

/* Snowdrake: crescents falling in arcs from the top corners / in lines at the SOUL */
static void atk_drake_arcs(int t) {
  if (t < 120 && t % 14 == 0) {
    bool left = (t / 14) % 2;
    Bullet *b = shot(SPR_ICESHARD, left ? bx0() : bx1(), by0(), (left ? 1 : -1) * (P(1) + rnd_n(P(1))), -P(1));
    if (b) b->kind = BK_GRAVITY, b->ay = 3, b->r = 3;
  }
}
static void atk_drake_lines(int t) {
  if (t < 120 && t % 20 == 0) {
    int x = bx0() + 8 + rnd_n(bx1() - bx0() - 16);
    for (int k = 0; k < 3; k++) {
      Bullet *b = shot(SPR_ICESHARD, x + (k - 1) * 10, by0() - 10 - k * 6, 0, 0);
      if (b) aim(b, P(2)), b->r = 3;
    }
  }
}
/* Ice Cap: ice shot up from hats at the bottom, falling back; zig-zag walls closing top and bottom */
static void atk_icecap_hats(int t) {
  if (t < 120 && t % 18 == 0) {
    Bullet *b = shot(SPR_ICESHARD, bx0() + 8 + rnd_n(bx1() - bx0() - 16), by1(), rnd_n(P(1)) - P(1) / 2, -P(4));
    if (b) b->kind = BK_GRAVITY, b->ay = 5, b->r = 3;
  }
}
static void atk_icecap_waves(int t) {
  /* columns of blocks scroll in from the right, their gaps zig-zagging */
  if (t < 130 && t % 6 == 0) {
    int h = by1() - by0();
    int gap_mid = by0() + h / 2 + ((t / 6) % 8 < 4 ? (t / 6) % 4 : 4 - (t / 6) % 4) * h / 10 - h / 5;
    for (int side = 0; side < 2; side++) {
      int y0 = side ? gap_mid + 13 : by0(), y1 = side ? by1() : gap_mid - 13;
      if (y1 <= y0) continue;
      Bullet *b = shot(SPR_BONE, bx1() + 4, (y0 + y1) / 2, -P(2), 0);
      if (b) b->flags |= BF_BOX, b->w = 5, b->h = (int16_t)(y1 - y0);
    }
  }
}
/* Gyftrot: three presents rise from the bottom (one blue: keep still), snowflakes fall in waves */
static void atk_gyft_presents(int t) {
  if (t % 50 == 0 && t < 130) {
    int blue = rnd_n(3);
    for (int k = 0; k < 3; k++) {
      Bullet *b = shot(SPR_PRESENT, bx0() + (bx1() - bx0()) * (k + 1) / 4, by1() + 4, 0, -P(1) - P(1) / 2);
      if (b) b->color = (uint8_t)(k == blue ? BC_BLUE : BC_WHITE), b->r = 4;
    }
  }
}
static void atk_gyft_snow(int t) {
  if (t < 130 && t % 8 == 0) {
    Bullet *b = shot(SPR_SNOWFLAKE, bx0() + 6 + rnd_n(bx1() - bx0() - 12), by0() - 4, 0, P(1) + P(1) / 4);
    if (b) b->kind = BK_SCRIPT, b->a = (int16_t)(b->x / FX), b->r = 3;
  }
  for (int i = 0; i < MAX_BULLETS; i++) {
    Bullet *b = &bullets[i];
    if (b->kind != BK_SCRIPT || b->spr != SPR_SNOWFLAKE) continue;
    b->t++;
    b->y += b->vy;
    b->x = P(b->a) + P(8) * gfx_sin(b->t * 4) / 1024;
    if (b->y > P(by1() + 8)) b->kind = BK_NONE;
  }
}
/* Doggo: a blue blade sweeps across the board (still, it can't see you) */
static void atk_doggo_sword(int t) {
  if (t == 10 || t == 80) {
    bool right = t == 80;
    Bullet *b = shot(SPR_BONE, right ? bx1() + 6 : bx0() - 6, (by0() + by1()) / 2, right ? -P(3) : P(3), 0);
    if (b) b->flags |= BF_BOX, b->w = 4, b->h = (int16_t)(by1() - by0()), b->color = BC_BLUE;
  }
}
/* Lesser Dog: a little dog runs on the spot, then leaps at the SOUL; or spears, blue then white */
static void atk_ldog_jump(int t) {
  if (t == 0) {
    Bullet *b = shot(SPR_DOG_OW, bx1() - 10, by1() - 8, 0, 0);
    if (b) b->kind = BK_SCRIPT, b->r = 6, b->anim = 4;
  }
  for (int i = 0; i < MAX_BULLETS; i++) {
    Bullet *b = &bullets[i];
    if (b->kind != BK_SCRIPT || b->spr != SPR_DOG_OW) continue;
    b->t++;
    if (b->t % 4 == 0) b->frame++;
    if (b->t == 40 || b->t == 100) aim(b, P(3)), b->vy -= P(2), b->c = 1;
    if (b->c) {
      b->vy += 4;
      b->x += b->vx, b->y += b->vy;
      if (b->y > P(by1() - 8)) b->y = P(by1() - 8), b->vx = b->vy = 0, b->c = 0;
      if (b->x < P(bx0() + 6)) b->x = P(bx0() + 6);
      if (b->x > P(bx1() - 6)) b->x = P(bx1() - 6);
    }
  }
}
static void atk_ldog_spears(int t) {
  if (t == 5) {
    Bullet *b = shot(SPR_BONE, bx1() + 20, (by0() + by1()) / 2, -P(3), 0);
    if (b) b->flags |= BF_BOX, b->w = 40, b->h = 4, b->color = BC_BLUE;
  }
  if (t == 70) {
    bool left = rnd_n(2);
    Bullet *b = shot(SPR_BONE, left ? bx0() - 20 : bx1() + 20, by0() + 10 + rnd_n(by1() - by0() - 20), left ? P(3) : -P(3), 0);
    if (b) b->flags |= BF_BOX, b->w = 40, b->h = 4, b->kind = BK_SCRIPT, b->a = left;
  }
  for (int i = 0; i < MAX_BULLETS; i++) {
    Bullet *b = &bullets[i];
    if (b->kind != BK_SCRIPT || b->spr != SPR_BONE) continue;
    b->t++;
    if (b->t < 26) b->x += b->vx;
    else if (b->t > 50) b->x -= b->vx;
  }
}
/* the Dogi: two axes rise, then swing down crossing; a ring of hearts barked from one to the other (half blue) */
static void atk_dogi_axes(int t) {
  if (t == 0 || t == 70) {
    for (int k = 0; k < 2; k++) {
      Bullet *b = shot(SPR_KNIFE, k ? bx1() - 14 : bx0() + 14, by0() + 18, 0, -P(1) / 2);
      if (b) b->kind = BK_SCRIPT, b->a = (int16_t)k, b->r = 5;
    }
  }
  for (int i = 0; i < MAX_BULLETS; i++) {
    Bullet *b = &bullets[i];
    if (b->kind != BK_SCRIPT || b->spr != SPR_KNIFE) continue;
    b->t++;
    if (b->t < 16) b->y += b->vy;
    else b->x += b->a ? -P(2) : P(2), b->y += P(1);
    if (b->t > 70) b->kind = BK_NONE;
  }
}
static void atk_dogi_hearts(int t) {
  if (t % 50 == 5 && t < 120) {
    int cy = by0() + 12 + rnd_n(by1() - by0() - 24);
    for (int k = 0; k < 8; k++) {
      Bullet *b = shot(SPR_HEARTBULLET, bx0() - 10, cy, P(2), 0);
      if (b) b->kind = BK_ORBIT, b->a = (int16_t)(bx0() - 10), b->b = (int16_t)cy, b->c = P(13), b->d = (int16_t)(k * 32 * 16),
             b->vx = 40, b->vy = 0, b->color = (uint8_t)(k % 2 ? BC_BLUE : BC_WHITE), b->r = 3, b->ax = P(2);
    }
  }
  /* the ring's middle moves across */
  for (int i = 0; i < MAX_BULLETS; i++) {
    Bullet *b = &bullets[i];
    if (b->kind == BK_ORBIT && b->spr == SPR_HEARTBULLET) b->a = (int16_t)(b->a + 2);
  }
}
/* Greater Dog: its spear flies right to left flickering blue and white; a sleeping dog barks if you move */
static void atk_gdog_spear(int t) {
  if (t % 45 == 0 && t < 130) {
    Bullet *b = shot(SPR_BONE, bx1() + 20, by0() + 8 + rnd_n(by1() - by0() - 16), -P(3), 0);
    if (b) b->flags |= BF_BOX, b->w = 36, b->h = 4, b->kind = BK_SCRIPT;
  }
  for (int i = 0; i < MAX_BULLETS; i++) {
    Bullet *b = &bullets[i];
    if (b->kind != BK_SCRIPT || b->spr != SPR_BONE) continue;
    b->t++;
    b->x += b->vx;
    b->color = (uint8_t)((b->t / 10) % 2 ? BC_BLUE : BC_WHITE);
    if (b->x < P(bx0() - 30)) b->kind = BK_NONE;
  }
}
static void atk_gdog_bark(int t) {
  static int32_t lx, ly;
  if (t == 0) {
    lx = bt.sx, ly = bt.sy;
    Bullet *d = shot(SPR_DOG_OW, (bx0() + bx1()) / 2, by1() - 6, 0, 0);
    if (d) d->kind = BK_SCRIPT, d->flags |= BF_HARMLESS;
  }
  if (t > 10 && t % 16 == 0 && (bt.sx != lx || bt.sy != ly)) {
    Bullet *b = shot(SPR_HEARTBULLET, (bx0() + bx1()) / 2, by1() - 14, 0, 0);
    if (b) aim(b, P(2)), b->r = 3;
  }
  lx = bt.sx, ly = bt.sy;
}
/* Glyde: waves of stars */
static void atk_glyde_stars(int t) {
  if (t < 140 && t % 24 == 0) {
    bool left = (t / 24) % 2;
    for (int k = 0; k < 5; k++) {
      Bullet *b = shot(SPR_SNOWFLAKE, left ? bx0() - 6 : bx1() + 6, by0() + 6 + k * (by1() - by0() - 12) / 4, left ? P(2) : -P(2),
                       (k - 2) * 6);
      if (b) b->r = 3, b->angle = 1;
    }
  }
}

static const int16_t snow_lengths[ATK_SNOW_COUNT - ATK_RUINS_COUNT] = {
  [ATK_DRAKE_ARCS - ATK_RUINS_COUNT] = 150, [ATK_DRAKE_LINES - ATK_RUINS_COUNT] = 150,
  [ATK_ICECAP_HATS - ATK_RUINS_COUNT] = 150, [ATK_ICECAP_WAVES - ATK_RUINS_COUNT] = 180,
  [ATK_GYFT_PRESENTS - ATK_RUINS_COUNT] = 170, [ATK_GYFT_SNOW - ATK_RUINS_COUNT] = 160,
  [ATK_DOGGO_SWORD - ATK_RUINS_COUNT] = 150, [ATK_LDOG_JUMP - ATK_RUINS_COUNT] = 150,
  [ATK_LDOG_SPEARS - ATK_RUINS_COUNT] = 140, [ATK_DOGI_AXES - ATK_RUINS_COUNT] = 150,
  [ATK_DOGI_HEARTS - ATK_RUINS_COUNT] = 170, [ATK_GDOG_SPEAR - ATK_RUINS_COUNT] = 160,
  [ATK_GDOG_BARK - ATK_RUINS_COUNT] = 150, [ATK_GLYDE_STARS - ATK_RUINS_COUNT] = 170,
};
int papyrus_attack_length(int atk);           /* papyrus.c */
void papyrus_attack(int atk, int t);
void papyrus_attack_draw(int atk, int t);

int snow_attack_length(int atk) {
  if (atk >= ATK_SNOW_COUNT) return wf_attack_length(atk);
  if (atk >= ATK_PAP_FIRST && atk <= ATK_PAP_LAST) return papyrus_attack_length(atk);
  int n = snow_lengths[atk - ATK_RUINS_COUNT] ? snow_lengths[atk - ATK_RUINS_COUNT] : 150;
  /* Jerry makes attacks 2 seconds longer */
  for (int i = 0; i < bt.nmon; i++)
    if (bt.mon[i].def == mon_def(MON_JERRY) && bt.mon[i].alive && !bt.mon[i].gone) n += 60;
  return n;
}

void snow_attack_run(Battle *b, int atk, int t) {
  if (atk >= ATK_SNOW_COUNT) {
    wf_attack_run(b, atk, t);
    return;
  }
  (void)b;
  if (t < 0) return;
  switch (atk) {
    case ATK_DRAKE_ARCS: atk_drake_arcs(t); break;
    case ATK_DRAKE_LINES: atk_drake_lines(t); break;
    case ATK_ICECAP_HATS: atk_icecap_hats(t); break;
    case ATK_ICECAP_WAVES: atk_icecap_waves(t); break;
    case ATK_GYFT_PRESENTS: atk_gyft_presents(t); break;
    case ATK_GYFT_SNOW: atk_gyft_snow(t); break;
    case ATK_DOGGO_SWORD: atk_doggo_sword(t); break;
    case ATK_LDOG_JUMP: atk_ldog_jump(t); break;
    case ATK_LDOG_SPEARS: atk_ldog_spears(t); break;
    case ATK_DOGI_AXES: atk_dogi_axes(t); break;
    case ATK_DOGI_HEARTS: atk_dogi_hearts(t); break;
    case ATK_GDOG_SPEAR: atk_gdog_spear(t); break;
    case ATK_GDOG_BARK: atk_gdog_bark(t); break;
    case ATK_GLYDE_STARS: atk_glyde_stars(t); break;
    default:
      if (atk >= ATK_PAP_FIRST && atk <= ATK_PAP_LAST) papyrus_attack(atk, t);
      break;
  }
}

void snow_attack_draw(Battle *b, int atk, int t) {
  (void)b;
  if (atk >= ATK_PAP_FIRST && atk <= ATK_PAP_LAST) papyrus_attack_draw(atk, t);
}

/* after a battle in Snowdin: who was killed, who's been met */
void snow_battle_done(int form) {
  for (int i = 0; i < bt.nmon; i++) {
    const Monster *m = &bt.mon[i];
    if (m->def == mon_def(MON_SNOWDRAKE) && m->killed) set_flag(F_SNOWDRAKE_DEAD, 1);
  }
  if (form == FORM_LESSERDOG) set_flag(F_LESSERDOG, bt.mon[0].killed ? 2 : 1);
}
