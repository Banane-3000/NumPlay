/* Mettaton's shows, and a laser's alarm: battles that play their own way (battle.c's scripts).
 *
 * The quiz: Mettaton asks, four answers wait in the box, and the SOUL picks one (Z over it) before the time runs
 * out; Alphys, beside the stage, signals the right one. A wrong answer halves Frisk's HP (never below 1).
 * The cooking show's jetpack: Frisk flies up the counter as it grows, dodging what Mettaton throws, before a minute
 * is up. The alarm: three lasers, orange or blue, sweep through the SOUL. (As the Undertale Wiki describes them;
 * the questions and what's said are ours.) */
#include <stdio.h>
#include <string.h>
#include "battle.h"
#include "game.h"
#include "gfx.h"
#include "monsters.h"
#include "res.h"
#include "text.h"

#define P(v) ((v) * FX)

static void bubble(const char *s) {
  writer_start(&bt.bw, s, 0, 0, FONT_SMALL);
  bt.bw.line_h = 9;
}
static void draw_bubble_at(int bx, int by, int w, int h) {
  gfx_rect(bx, by, w, h, C_WHITE, 255);
  gfx_rect(bx - 6, by + 16, 6, 5, C_WHITE, 255);
  bt.bw.x = bx + 5, bt.bw.y = by + 5;
  writer_draw(&bt.bw, C_BLACK);
}
static void draw_box(void) {
  gfx_rect(bt.x0, bt.y0, bt.x1 - bt.x0, bt.y1 - bt.y0, C_WHITE, 255);
  gfx_rect(bt.x0 + 2, bt.y0 + 2, bt.x1 - bt.x0 - 4, bt.y1 - bt.y0 - 4, C_BLACK, 255);
}
static void draw_soul(void) {
  if (!bt.inv || (bt.inv / 2) % 2) gfx_sprite(SPR_HEART, 0, soul_x(), soul_y(), 0, 255);
}
static void draw_mettaton(int x, int y) {
  gfx_sprite(SPR_MTT_B, (int)(g_frame / 6), x, y, 0, 255);
}
static void tick(uint32_t keys) {
  if (bt.inv) bt.inv--;
  soul_update(keys);
  bullets_update();
  soul_hit_test();
  if (sv.hp < 1) sv.hp = 1;   /* (these never kill) */
}

/* ---------------------------------------------------------------- the quiz */
typedef struct {
  const char *q;
  const char *a[4];
  int right;
} Question;
static const Question quiz[] = {
  {"First, an\neasy one!\nWhat color\nis a human\nSOUL?", {"Blue", "Red", "Green", "Plaid"}, 1},
  {"Who leads\nthe Royal\nGuard?", {"Papyrus", "Sans", "Undyne", "A dog"}, 2},
  {"How many\nhuman SOULs\ndoes the king\nneed?", {"Five", "Six", "Seven", "A dozen"}, 2},
  {"Quick math!\n7 x 6 - 24.5\nis...?", {"17.5", "16.5", "18", "42"}, 0},
  {"Which did\nyou NOT see\nin Waterfall?", {"Echo flowers", "Temmies", "Snowmen", "Bridge seeds"}, 2},
  {"And who's\nthe star of\nthis show?", {"You", "Alphys", "The crowd", "Mettaton"}, 3},
};
#define NQ ((int)(sizeof quiz / sizeof quiz[0]))
enum { QZ_HELLO, QZ_ASK, QZ_SAY, QZ_BYE };
/* bt.state: [0] step, [1] the question, [2] frames left to answer, [3] wrong answers */
#define QZ_TIME 450

static void quiz_ask(void) {
  bt.state[0] = QZ_ASK, bt.state[2] = QZ_TIME;
  bubble(quiz[bt.state[1]].q);
  bt.sx = P(board_cx()), bt.sy = P(board_cy());
}
static int zone_at(int x, int y) {
  /* the four answers, two by two in the box */
  int col = x >= board_cx(), row = y >= board_cy();
  return row * 2 + col;
}
void quiz_start(void) {
  bt.scripted = true;
  bt.state[0] = QZ_HELLO, bt.state[1] = 0, bt.state[3] = 0;
  board_set_rect(40, 128, 280, 194);
  bt.x0 = bt.tx0, bt.y0 = bt.ty0, bt.x1 = bt.tx1, bt.y1 = bt.ty1;
  bt.sx = P(board_cx()), bt.sy = P(board_cy());
  bubble("Welcome,\nbeauties, to\nthe quiz show\nof the\nunderground!|Answer right,\nand you win!\nAnswer wrong,\nand...|Well! Let's\nnot spoil the\nsurprise!");
}
void quiz_update(uint32_t keys, uint32_t pressed) {
  switch (bt.state[0]) {
    case QZ_HELLO:
    case QZ_SAY:
      if (!writer_update(&bt.bw, pressed)) {
        if (bt.state[0] == QZ_SAY && bt.state[1] >= NQ) {
          bt.state[0] = QZ_BYE;
          bubble("Hmph! Too\neasy, with a\ncertain doctor\nwaving the\nanswers!|Oh well. That\nwas only our\nfirst episode!|Until next\ntime, darlings!");
        } else {
          quiz_ask();
        }
      }
      break;
    case QZ_ASK: {
      writer_update(&bt.bw, 0);
      tick(keys);
      /* (a Z left over from the talk doesn't answer: the question first) */
      bool chose = ((pressed & K_Z) && bt.state[2] < QZ_TIME - 20) || --bt.state[2] <= 0;
      if (!chose) break;
      int z = bt.state[2] <= 0 ? -1 : zone_at(soul_x(), soul_y());
      const Question *q = &quiz[bt.state[1]];
      if (z == q->right) {
        static const char *const yes[] = {"Correct!\nHow\nthrilling!", "Right\nagain!", "Ooh, you're\ngood!"};
        bubble(yes[bt.state[1] % 3]);
      } else {
        sv.hp = (int16_t)max_i(1, sv.hp / 2);
        bt.inv = 30, bt.state[3]++;
        bubble(z < 0 ? "Time's up!\nThat's a\nshock for\nyou!" : "Wrong!\nThat's a\nshock for\nyou!");
        /* (worn out: straight to the end) */
        if (sv.hp <= 1) bt.state[1] = NQ - 1;
      }
      bt.state[1]++;
      bt.state[0] = QZ_SAY;
      break;
    }
    case QZ_BYE:
      if (!writer_update(&bt.bw, pressed)) bt.result = BR_SCRIPTED;
      break;
  }
}
void quiz_draw(void) {
  gfx_sprite(SPR_BATTLEBG, 0, 0, 0, 0, 255);
  draw_mettaton(120, 118);
  draw_bubble_at(182, 18, 104, 76);
  draw_box();
  if (bt.state[0] == QZ_ASK) {
    const Question *q = &quiz[bt.state[1]];
    static const char *const letter[4] = {"A", "B", "C", "D"};
    for (int i = 0; i < 4; i++) {
      int x = (i % 2 ? board_cx() + 6 : bt.x0 + 8), y = (i / 2 ? board_cy() + 6 : bt.y0 + 8);
      char s[32];
      snprintf(s, sizeof s, "%s %s", letter[i], q->a[i]);
      bool on = zone_at(soul_x(), soul_y()) == i;
      gfx_text(FONT_SMALL, x, y, s, on ? C_YELLOW : C_WHITE, 255);
    }
    gfx_rect(board_cx(), bt.y0 + 2, 1, bt.y1 - bt.y0 - 4, rgb565(0x404040), 255);
    gfx_rect(bt.x0 + 2, board_cy(), bt.x1 - bt.x0 - 4, 1, rgb565(0x404040), 255);
    char s[8];
    snprintf(s, sizeof s, "%d", (bt.state[2] + 14) / 15);
    gfx_text(FONT_SMALL, bt.x0 - 22, bt.y0 + 2, s, C_WHITE, 255);
    /* Alphys, off to the side, signals the answer */
    if (bt.state[2] < QZ_TIME - 60) {
      gfx_sprite(SPR_FACE_ALPHYS, 0, 282, 104, 0, 255);
      gfx_text(FONT_MAIN, 300, 92, letter[q->right], C_YELLOW, 255);
    }
    draw_soul();
  }
  battle_draw_hud();
}

/* ---------------------------------------------------------------- the cooking show: the jetpack */
/* bt.state: [0] step, [1] how high (0..CK_TOP), [2] frames left */
enum { CK_HELLO, CK_FLY, CK_BYE };
#define CK_TOP 600
#define CK_TIME 1800
void cook_start(void) {
  bt.scripted = true;
  bt.state[0] = CK_HELLO, bt.state[1] = 0, bt.state[2] = CK_TIME;
  board_set_rect(100, 40, 220, 194);
  bt.x0 = bt.tx0, bt.y0 = bt.ty0, bt.x1 = bt.tx1, bt.y1 = bt.ty1;
  bt.sx = P(board_cx()), bt.sy = P(bt.y1 - 14);
  bubble("The counter's\ngrowing! Get\nthe substitute\nin under a\nminute!|Use your\nphone's\nJETPACK!\nUp, up and\naway!");
}
void cook_update(uint32_t keys, uint32_t pressed) {
  switch (bt.state[0]) {
    case CK_HELLO:
      if (!writer_update(&bt.bw, pressed)) bt.state[0] = CK_FLY, bt.t = 0;
      break;
    case CK_FLY: {
      int hp = sv.hp;
      tick(keys);
      /* what Mettaton throws down (faster as the counter goes up) */
      if (bt.t % max_i(8, 22 - bt.state[1] / 50) == 0) {
        Bullet *b = bullet_new(rnd_n(3) ? SPR_ORB : SPR_BOMB, P(bt.x0 + 8 + rnd_n(bt.x1 - bt.x0 - 16)), P(bt.y0 + 2), 0,
                               P(2) + P(bt.state[1]) / 300);
        if (b) b->r = 4, b->dmg = 3, b->flags |= BF_NOCLIP;
      }
      /* up the jetpack goes, set back by every hit */
      bt.state[1] = (int16_t)min_i(CK_TOP, bt.state[1] + 1 - (sv.hp < hp ? 60 : 0));
      if (bt.state[1] < 0) bt.state[1] = 0;
      if (bt.state[1] >= CK_TOP) {
        bt.state[0] = CK_BYE, bt.state[3] = 1;
        bullets_clear();
        bubble("You made it!\nThe vegan\nsubstitute!|...and I'd\nalready baked\nthe cake.\nTa-da!|That's our\nshow! Ciao!");
      } else if (--bt.state[2] <= 0) {
        bt.state[0] = CK_BYE, bt.state[3] = 0;
        bullets_clear();
        bubble("Oh dear,\nout of time!|...but we're\nat the break.\nNo killing\nwithout an\naudience!|Ciao for now!");
      }
      break;
    }
    case CK_BYE:
      if (!writer_update(&bt.bw, pressed)) bt.result = BR_SCRIPTED;
      break;
  }
}
int cook_won(void) { return bt.state[3]; }
void cook_draw(void) {
  gfx_sprite(SPR_BATTLEBG, 0, 0, 0, 0, 255);
  draw_mettaton(272, 118);
  draw_box();
  gfx_clip(bt.x0 + 2, bt.y0 + 2, bt.x1 - bt.x0 - 4, bt.y1 - bt.y0 - 4);
  /* the counter's side scrolling past: lines going down as Frisk goes up */
  for (int k = 0; k < 8; k++) {
    int y = bt.y0 + ((k * 20 + bt.state[1]) % 160);
    gfx_rect(bt.x0 + 2, y, bt.x1 - bt.x0 - 4, 1, rgb565(0x303030), 255);
  }
  bullets_draw();
  if (bt.state[0] == CK_FLY) draw_soul();
  gfx_clip_all();
  /* how far up, and the time */
  int h = (bt.y1 - bt.y0) * bt.state[1] / CK_TOP;
  gfx_rect(bt.x0 - 14, bt.y0, 6, bt.y1 - bt.y0, rgb565(0x404040), 255);
  gfx_rect(bt.x0 - 14, bt.y1 - h, 6, h, C_YELLOW, 255);
  char s[8];
  snprintf(s, sizeof s, "%d", (bt.state[2] + 29) / 30);
  gfx_text(FONT_MAIN, bt.x0 - 34, bt.y0 - 2, s, C_WHITE, 255);
  if (bt.state[0] != CK_FLY) draw_bubble_at(182, 18, 104, 76);
  battle_draw_hud();
}

/* ---------------------------------------------------------------- the lasers' alarm */
void lasers_start(void) {
  bt.scripted = true;
  bt.state[0] = 0;
  board_set(120, 60);
  bt.x0 = bt.tx0, bt.y0 = bt.ty0, bt.x1 = bt.tx1, bt.y1 = bt.ty1;
  bt.sx = P(board_cx()), bt.sy = P(board_cy());
  bt.t = 0;
}
void lasers_update(uint32_t keys, uint32_t pressed) {
  (void)pressed;
  tick(keys);
  if (bt.t % 40 == 10 && bt.t < 100) {
    Bullet *b = bullet_new(SPR_ORB, P(bt.x1 + 4), P(board_cy()), -P(4), 0);
    if (b) b->color = rnd_n(2) ? BC_ORANGE : BC_BLUE, b->flags |= BF_BOX | BF_NOCLIP, b->w = 3,
           b->h = (int16_t)(bt.y1 - bt.y0), b->dmg = 3, b->life = 60;
  }
  if (bt.t > 170) bt.result = BR_SCRIPTED;
}
void lasers_draw(void) {
  gfx_rect(0, 0, SCREEN_W, SCREEN_H, C_BLACK, 255);
  draw_box();
  for (int i = 0; i < MAX_BULLETS; i++) {
    const Bullet *b = &bullets[i];
    if (b->kind == BK_NONE || !(b->flags & BF_BOX)) continue;
    int x = b->x / FX;
    if (x < bt.x0 + 2 || x > bt.x1 - 2) continue;
    gfx_rect(x - 1, bt.y0 + 2, 3, bt.y1 - bt.y0 - 4, b->color == BC_ORANGE ? rgb565(0xFCA600) : rgb565(0x14A9FF), 255);
  }
  draw_soul();
  battle_draw_hud();
}
