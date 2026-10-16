/* Papyrus, Snowdin's boss: his bones, the blue SOUL he gives you, his special attack and the dog that steals
 * it (as the Undertale Wiki describes the fight). He never kills you: he captures you (the battle ends,
 * snowtown.c takes it from there). */
#include <stdio.h>
#include <string.h>
#include "battle.h"
#include "game.h"
#include "gfx.h"
#include "monsters.h"
#include "res.h"

/* b->state[0]: 0 before he's FOUGHT or spared, 1 the blue attack next, 2 blue; [1] flirts, [2] insults;
 * [3] blue turns; [4] FOUGHT (1) or spared (2) first; [5] he spares you; [6] his speech this turn */
enum { PAP_RED, PAP_BLUE_NEXT, PAP_BLUE };

static const char *const turn_lines[] = {
  "BEHOLD!", "AND I HAVE A\n#RSPECIAL\nATTACK#X TOO!", "I CAN ALMOST\nHEAR THE\nCHEERING!!!", "PAPYRUS,\nROYAL\nGUARD!", "PAPYRUS,\nPASTA\nMASTER!",
  "UNDYNE WILL\nBE SO\nPROUD!!", "THEY'LL MAKE\nA STATUE OF\nME! A BIG\nONE!!!", "SANS WILL...\nPROBABLY\nNAP THROUGH\nIT.",
  "SO MANY\nFANS!!\nBUT...", "WILL THEY\nLIKE THE\nREAL ME???", "YOU'RE NOT\nLIKE MOST\nPEOPLE...", "ONCE YOU'RE\nCAPTURED...",
  "THEY WON'T\nLET YOU\nGO, WILL\nTHEY...", "NO! GIVE UP\nALREADY!!", "OR FACE MY\n#RSPECIAL\nATTACK#X!!!", "IT'S COMING\nSOON! MY\n#RSPECIAL\nATTACK#X!",
  "ANY TURN NOW\nTHE #RSPECIAL\nATTACK#X!!!", "LAST CHANCE\nBEFORE MY\n#RSPECIAL\nATTACK#X!!"};
#define NTURNS (int)(sizeof turn_lines / sizeof turn_lines[0])
#define SPECIAL_TURN NTURNS     /* the dog steals it */
#define NORMAL_TURN (NTURNS + 1)

static void act_papyrus(Battle *b, Monster *m, int act, char *t, int cap) {
  (void)m;
  if (b->state[0] == PAP_BLUE) {
    snprintf(t, (size_t)cap, act == 0 ? "* Papyrus is too busy FIGHTing to flirt back." : "* Papyrus is too busy FIGHTing to accept your insult.");
    return;
  }
  if (act == 0) {
    b->state[1]++;
    snprintf(t, (size_t)cap, b->state[1] >= 3 ? "* You FLIRT, but to no avail.\n* Seems ACTing won't escalate this battle..." : "* You flirt with Papyrus.");
  } else {
    b->state[2]++;
    snprintf(t, (size_t)cap, b->state[2] >= 4 ? "* You INSULT, but to no avail.\n* Seems ACTing won't escalate this battle..." : "* You insult Papyrus.");
  }
  b->state[6] = act + 1;
}

static const char *talk_papyrus(Battle *b, Monster *m) {
  (void)m;
  int said = b->state[6];
  b->state[6] = 0;
  /* the first FIGHT or MERCY: the blue attack comes */
  if (b->state[0] == PAP_RED && (b->last_action == 0 || b->last_action == 3)) {
    b->state[0] = PAP_BLUE_NEXT;
    return b->last_action == 0 ? "OH, A FIGHT,\nIS IT?|THEN TRY MY\nLEGENDARY\nBLUE ATTACK!"
                               : "NO FIGHTING?\nHMPH!|THEN TRY MY\nLEGENDARY\nBLUE ATTACK!";
  }
  if (b->last_action == 2 && b->state[0] != PAP_BLUE) return "OOH, SNACKS!\nI SHOULD HAVE\nPACKED SOME.";
  if (said == 1) {
    if (b->state[1] == 1) return "FL-FLIRTING!?|I HAVE VERY\nHIGH\nSTANDARDS!!!|A DATE...\nAFTER THE\nCAPTURE!";
    return "AFTER THE\nCAPTURE! THEN\nWE'LL SEE!";
  }
  if (said == 2) {
    if (b->state[2] == 1) return "AH, I SEE!|YOU'RE BEING\nMEAN SO I\nFEEL OKAY\nFIGHTING YOU!";
    if (b->state[2] == 2) return "PRICKLY\nOUTSIDE, SOFT\nINSIDE!\nLIKE A\nCACTUS!";
    return "SAVE YOUR\nBREATH!";
  }
  if (b->state[0] != PAP_BLUE) return b->state[0] == PAP_RED ? "NYEH HEH\nHEH!" : NULL;
  /* his absolutely normal attack done: he spares you (and one hit would end him now) */
  if (m->state == 1) {
    b->state[5] = 1, m->spare = true, m->state = 2;
    m->df = (int16_t)(-2 * m->hp);
    return "*HUFF*\nYOU CAN'T\nBEAT ME!\n*HUFF*|SO I'LL BE\nGENEROUS...|I #RSPARE#X\nYOU, HUMAN!|ACCEPT MY\n#RMERCY#X!";
  }
  if (b->state[5]) return NULL;
  int k = b->state[3];
  if (k < NTURNS) return turn_lines[k];
  if (k == SPECIAL_TURN)
    return "AND NOW...\nMY #RSPECIAL\nATTACK#X!|HEY!!!|THE DOG TOOK\nIT!!!|...|FINE. A\nREGULAR\nATTACK,\nTHEN.";
  if (k == NORMAL_TURN) return "*SIGH*\nA PERFECTLY\nORDINARY\nATTACK.";
  return NULL;
}

static const char *flavor_papyrus(Battle *b, Monster *m) {
  (void)m;
  if (b->state[5]) return "* Papyrus is sparing you.";
  if (b->state[0] == PAP_BLUE && b->state[3] == 1) return "* You're blue now.";
  if (b->state[3] > SPECIAL_TURN) return "* Papyrus is getting ready for a regular attack.";
  if (b->state[1] && b->state[0] != PAP_BLUE) return "* Papyrus is thinking about what to wear for his date.";
  return NULL;
}

static int attack_papyrus(Battle *b, Monster *m) {
  if (b->state[5]) return -1;
  if (b->state[0] == PAP_RED) return ATK_PAP_FIRST;          /* bones low along the floor */
  if (b->state[0] == PAP_BLUE_NEXT) {
    b->state[0] = PAP_BLUE, b->state[3] = 0;
    return ATK_PAP_FIRST + 1;                                 /* the blue attack, then the SOUL goes blue */
  }
  int k = ++b->state[3];
  if (k == SPECIAL_TURN) return ATK_PAP_FIRST + 9;            /* the dog */
  if (k == NORMAL_TURN) {
    m->state = 1;                                            /* (after it, he spares you) */
    return ATK_PAP_FIRST + 10;
  }
  if (k <= 6) return ATK_PAP_FIRST + 1 + k;                  /* the wiki's patterns, in order */
  return ATK_PAP_FIRST + 3 + rnd_n(6);
}

static bool hit_papyrus(Battle *b, Monster *m, int dmg) {
  (void)b;
  if (b->state[0] == PAP_RED) b->state[4] = 1;
  (void)m, (void)dmg;
  return false;
}

const MonDef papyrus_def = {
  .name = "Papyrus", .hp = 680, .at = 8, .df = 2, .exp = 200, .gold = 0, .spr_idle = SPR_PAPYRUS_B,
  .spr_hurt = SPR_PAPYRUS_HURT, .check = "He likes to say:\n* \"Nyeh heh heh!\"", .acts = {"Flirt", "Insult"}, .nacts = 2,
  .neutral = {"* Papyrus is preparing a bone attack.", "* Papyrus is cackling.", "* Papyrus whispers \"Nyeh heh heh!\"",
              "* Papyrus is rattling his bones.", "* Papyrus is trying hard to play it cool.",
              "* Papyrus is considering his options."},
  .flags = MF_BOSS, .act = act_papyrus, .talk = talk_papyrus, .attack = attack_papyrus, .flavor = flavor_papyrus,
  .on_hit = hit_papyrus,
};

/* ---------------------------------------------------------------- his bones */
#define P(v) ((v) * FX)
static int bx0(void) { return bt.x0 + 3; }
static int bx1(void) { return bt.x1 - 3; }
static int by0(void) { return bt.y0 + 3; }
static int by1(void) { return bt.y1 - 3; }

/* a bone of a pattern: when (frames), how tall (from the floor, or hanging from the ceiling when negative),
 * blue, fast, and from the right instead of the left */
typedef struct {
  uint16_t at;
  uint8_t h, ceil, blue, fast, right;
} Bone;
static void bone(const Bone *k) {
  bool right = k->right;
  int sp = k->fast ? P(4) : P(2);
  int y = k->ceil ? by0() + k->h / 2 : by1() - k->h / 2;
  Bullet *b = bullet_new(SPR_BONE, P(right ? bx1() + 4 : bx0() - 4), P(y), right ? -sp : sp, 0);
  if (!b) return;
  b->flags |= BF_BOX, b->w = 5, b->h = k->h, b->owner = (uint8_t)bt.atk_owner;
  if (k->blue) b->color = BC_BLUE;
}

/* the patterns (heights halved from the game's: small 10, medium 20, large 30): a floor bone F, one hanging from
 * the ceiling C, a blue one BL, from the right R (C from the right: RC), fast from the right FR */
#define F(at, h) {at, h, 0, 0, 0, 0}
#define C(at, h) {at, h, 1, 0, 0, 0}
#define BL(at) {at, 60, 0, 1, 0, 0}
#define R(at, h) {at, h, 0, 0, 0, 1}
#define RC(at, h) {at, h, 1, 0, 0, 1}
#define FR(at, h) {at, h, 0, 0, 1, 1}
static const Bone pat_red[] = {F(0, 8), F(16, 10), F(32, 12), F(48, 14), F(64, 16), F(80, 14), F(96, 12), F(112, 10), F(128, 8)};
static const Bone pat_blue[] = {BL(0), BL(20), BL(40), BL(60), F(120, 10)};
static const Bone pat_2[] = {F(0, 10), F(20, 10), F(40, 10), F(60, 10), F(90, 20)};
static const Bone pat_3[] = {F(0, 10), F(30, 20), F(55, 20), F(80, 20), F(110, 30), F(140, 30), F(170, 30)};
static const Bone pat_4[] = {F(0, 30), C(40, 30), F(70, 10), F(85, 10), F(100, 10), F(125, 20), C(160, 30)};
static const Bone pat_5[] = {F(0, 30), F(35, 30), F(70, 30), BL(105), FR(140, 10)};
static const Bone pat_6[] = {F(0, 12), C(0, 30), F(40, 12), C(40, 30), RC(80, 30), F(120, 8), F(126, 12), F(132, 16),
                             F(138, 20), F(144, 20), F(150, 16), F(156, 12), F(162, 8)};
static const Bone pat_7[] = {F(0, 12), C(0, 30), F(25, 12), C(25, 30), F(60, 22), C(60, 22), F(85, 22), C(85, 22), BL(120),
                             R(150, 30), F(185, 12), C(185, 30), R(185, 12), RC(185, 30)};
static const Bone pat_normal[] = {F(0, 10), F(15, 10), F(30, 20), F(50, 30), C(70, 30), FR(95, 10), F(110, 20), BL(125),
                                  F(150, 12), C(150, 30), F(175, 12), C(175, 30), F(205, 8), F(210, 12), F(215, 16),
                                  F(220, 20), F(225, 24), F(230, 20), F(235, 16), F(240, 12), F(245, 8), F(275, 30),
                                  FR(300, 30), F(320, 10), F(330, 10), F(340, 10), F(350, 10)};
typedef struct {
  const Bone *bones;
  int n, len;
} Pattern;
#define PAT(a, len) {a, (int)(sizeof a / sizeof a[0]), len}
static const Pattern pats[] = {
  PAT(pat_red, 200), PAT(pat_blue, 200), PAT(pat_2, 170), PAT(pat_3, 250), PAT(pat_4, 240), PAT(pat_5, 220),
  PAT(pat_6, 240), PAT(pat_7, 270), PAT(pat_2, 170), {NULL, 0, 220}, PAT(pat_normal, 420)};

int papyrus_attack_length(int atk) { return pats[atk - ATK_PAP_FIRST].len; }

void papyrus_attack(int atk, int t) {
  int k = atk - ATK_PAP_FIRST;
  if (t == 0) {
    /* his board: wide and low; the SOUL on the floor once it's blue */
    if (k == 0) board_set(110, 60);
    else board_set(140, 60);
    if (k >= 2) soul_set_mode(SOUL_BLUE);
    if (bt.soul_mode == SOUL_BLUE) bt.sy = P(by1() - 3);
  }
  if (k == 1 && t == 100) {   /* the blue bones passed: now your SOUL's blue */
    soul_set_mode(SOUL_BLUE);
  }
  const Pattern *p = &pats[k];
  for (int i = 0; i < p->n; i++)
    if (p->bones[i].at == t) bone(&p->bones[i]);
  /* the dog: it trots in, takes the special attack's bone and runs off with it (harmless) */
  if (k == 9) {
    if (t == 20) {
      Bullet *b = bullet_new(SPR_BONE, P((bx0() + bx1()) / 2), P(by1() - 12), 0, 0);
      if (b) b->flags |= BF_BOX | BF_HARMLESS, b->w = 24, b->h = 6, b->kind = BK_SCRIPT, b->a = 1;
    }
    if (t == 60) {
      Bullet *d = bullet_new(SPR_DOG_OW, P(bx1() + 10), P(by1() - 6), -P(1), 0);
      if (d) d->flags |= BF_HARMLESS, d->anim = 6;
    }
    for (int i = 0; i < MAX_BULLETS; i++) {
      Bullet *d = &bullets[i];
      if (d->spr != SPR_DOG_OW || d->kind == BK_NONE) continue;
      /* at the bone: it takes it and goes the other way */
      if (d->vx < 0 && d->x <= P((bx0() + bx1()) / 2 + 6)) {
        d->vx = P(2), d->spr = SPR_DOG_BONE;
        for (int j = 0; j < MAX_BULLETS; j++)
          if (bullets[j].kind == BK_SCRIPT && bullets[j].a == 1) bullets[j].kind = BK_NONE;
      }
    }
  }
  /* the normal attack's dog: it runs across, and it hurts */
  if (k == 10 && t == 260) {
    Bullet *d = bullet_new(SPR_DOG_OW, P(bx0() - 10), P(by1() - 6), P(3), 0);
    if (d) d->owner = (uint8_t)bt.atk_owner, d->dmg = 3, d->r = 6, d->anim = 4;
  }
}

void papyrus_attack_draw(int atk, int t) {
  (void)atk, (void)t;
}
