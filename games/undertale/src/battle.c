/* Battles: the turns, the menus, FIGHT's bar, the box, the end (battle.h). */
#include <stdio.h>
#include <string.h>
#include "battle.h"
#include "game.h"
#include "gfx.h"
#include "items.h"
#include "res.h"
#include "save.h"
#include "text.h"
#include "menu.h"
#include "monsters.h"
#include "world.h"

/* the battles that play their own way: Flowey's first (flowey.c), Mettaton's shows and a laser's alarm (htshows.c) */
typedef struct {
  int form;
  void (*start)(void);
  void (*update)(uint32_t keys, uint32_t pressed);
  void (*draw)(void);
} Script;
static const Script scripts[] = {
  {FORM_FLOWEY, flowey_battle_start, flowey_battle_update, flowey_battle_draw},
  {FORM_MTT_QUIZ, quiz_start, quiz_update, quiz_draw},
  {FORM_MTT_COOK, cook_start, cook_update, cook_draw},
  {FORM_LASERS, lasers_start, lasers_update, lasers_draw},
};
static const Script *script;

Battle bt;
static bool over = true;
static int heart_x, heart_y;    /* the SOUL's start, from Frisk */
static int prev_mode;

enum {
  P_INTRO, P_MENU, P_TARGET, P_ACTS, P_ITEMS, P_MERCY, P_BAR, P_STRIKE, P_RESULT, P_TALK, P_BOX_IN, P_ATTACK,
  P_BOX_OUT, P_WON, P_FLEE, P_DEATH, P_END, P_GLARE
};
enum { B_FIGHT, B_ACT, B_ITEM, B_MERCY };

static const int16_t button_x[4] = {16, 92, 172, 250};
#define BUTTON_Y 216

/* ---------------------------------------------------------------- the box */
static void box_to(int x0, int y0, int x1, int y1) { bt.tx0 = (int16_t)x0, bt.ty0 = (int16_t)y0, bt.tx1 = (int16_t)x1, bt.ty1 = (int16_t)y1; }
static void box_menu(void) { box_to(BOX_X0, BOX_Y0, BOX_X1, BOX_Y1); }
void board_set(int w, int h) {
  int cx = 160, y1 = BOX_Y1;
  box_to(cx - w / 2, y1 - h, cx + (w + 1) / 2, y1);
}
void board_set_rect(int x0, int y0, int x1, int y1) { box_to(x0, y0, x1, y1); }
int board_cx(void) { return (bt.x0 + bt.x1) / 2; }
int board_cy(void) { return (bt.y0 + bt.y1) / 2; }
static bool box_settled(void) { return bt.x0 == bt.tx0 && bt.y0 == bt.ty0 && bt.x1 == bt.tx1 && bt.y1 == bt.ty1; }
static int16_t approach(int16_t v, int16_t t, int step) {
  if (v < t) return (int16_t)(v + step > t ? t : v + step);
  if (v > t) return (int16_t)(v - step < t ? t : v - step);
  return v;
}
static void box_step(void) {
  bt.x0 = approach(bt.x0, bt.tx0, 12), bt.x1 = approach(bt.x1, bt.tx1, 12);
  bt.y0 = approach(bt.y0, bt.ty0, 8), bt.y1 = approach(bt.y1, bt.ty1, 8);
}

/* ---------------------------------------------------------------- texts */
static void say(const char *s) {
  if (s != bt.text) snprintf(bt.text, sizeof bt.text, "%s", s);
  text_wrap_width(264);
  writer_start(&bt.w, bt.text, 26, 133, FONT_MAIN);
  bt.w.line_h = 16;
}

int count_alive(void) {
  int n = 0;
  for (int i = 0; i < bt.nmon; i++) n += bt.mon[i].alive && !bt.mon[i].gone;
  return n;
}

static const char *flavor_text(void) {
  /* a monster's own, else one of its neutral texts, low HP's or spareable's */
  int alive[MAX_MON], n = 0;
  for (int i = 0; i < bt.nmon; i++)
    if (bt.mon[i].alive && !bt.mon[i].gone) alive[n++] = i;
  if (!n) return "";
  Monster *m = &bt.mon[alive[rnd_n(n)]];
  const MonDef *d = m->def;
  if (d->flavor) {
    const char *s = d->flavor(&bt, m);
    if (s) return s;
  }
  if (m->spare && d->spareable) return d->spareable;
  if (m->hp < m->maxhp / 3 && d->low_hp) return d->low_hp;
  int k = 0;
  while (k < 6 && d->neutral[k]) k++;
  return k ? d->neutral[rnd_n(k)] : "";
}

/* ---------------------------------------------------------------- the start and the end */
void battle_start(int form) {
  memset(&bt, 0, sizeof bt);
  bt.form = form;
  bt.atk = -1;
  form_setup(form, &bt);
  for (int i = 0; i < bt.nmon; i++) {
    Monster *m = &bt.mon[i];
    const MonDef *d = m->def;
    if (!m->maxhp) m->hp = m->maxhp = d->hp;
    if (!m->at) m->at = d->at;
    if (!m->df) m->df = d->df;
    if (!m->exp) m->exp = d->exp;
    if (!m->gold) m->gold = d->gold;
    m->alive = true;
    m->spare = d->flags & MF_SPARE_START;
    m->spare_gold = d->spare_gold;
  }
  bt.x0 = bt.tx0 = BOX_X0, bt.y0 = bt.ty0 = BOX_Y0, bt.x1 = bt.tx1 = BOX_X1, bt.y1 = bt.ty1 = BOX_Y1;
  bt.speed = 2 * FX;
  soul_set_mode(SOUL_RED);
  bt.hp_shown = sv.hp;
  heart_x = pl.x + 10 - cam_x, heart_y = pl.y + 17 - cam_y;
  prev_mode = g_mode;
  g_mode = MODE_BATTLE;
  bt.phase = P_INTRO;
  bt.t = 0;
  say(form_text(form));
  bt.w.finished = true;   /* (typed once the intro is over) */
  script = NULL;
  for (unsigned i = 0; i < sizeof scripts / sizeof scripts[0]; i++)
    if (scripts[i].form == form) script = &scripts[i], script->start();
  over = false;
  bullets_clear();
}

bool battle_over(void) { return over; }
int battle_result(void) { return bt.result; }
int battle_ui(void) {
  switch (bt.phase) {
    case P_MENU: return BUI_MENU;
    case P_TARGET: return BUI_TARGET;
    case P_ACTS: return BUI_ACTS;
    case P_ITEMS: return BUI_ITEMS;
    case P_MERCY: return BUI_MERCY;
    case P_BAR: return BUI_BAR;
    default: return BUI_NONE;
  }
}

static void battle_end(int result) {
  bt.result = result;
  bt.phase = P_END;
  over = true;
  bullets_clear();
  if (result == BR_DIED) {
    menu_gameover_open();
    return;
  }
  g_mode = MODE_WORLD;
  world_camera();
}

/* ---------------------------------------------------------------- FIGHT */
static int fight_damage(Monster *m, int dist) {
  /* the game's: (AT + weapon - DEF + a little luck), times how near the middle the bar was stopped */
  int at = sv.at + weapon_at();
  float base = (float)(at - m->df + rnd_n(3));
  float stretch = dist <= 12 ? 2.2f : (1.0f - (float)dist / 273.0f) * 2.0f;
  int dmg = (int)(base * stretch + 0.5f);
  if (m->def->flags & MF_ONE_HIT) dmg = m->hp;
  return dmg < 0 ? 0 : dmg;
}

static void strike(void) {
  Monster *m = &bt.mon[bt.target];
  /* the bar's distance from the middle, in the original's pixels */
  int x = bt.bar_x / FX;            /* 0..281 across the target */
  int dist = abs_i(x - 140) * 2;
  int dmg = bt.bar_x < 0 ? -1 : fight_damage(m, dist);
  bt.strike_dmg = dmg;
  bt.phase = P_STRIKE;
  bt.t = 0;
}

static void after_player_turn(void);

static void strike_land(void) {
  Monster *m = &bt.mon[bt.target];
  int dmg = bt.strike_dmg;
  m->dmg_shown = (int16_t)dmg, m->dmg_t = 45;
  m->bar_shown = m->hp;
  if (dmg <= 0 && (m->def->flags & MF_MISS_IS_SPARE) && m->def->on_spare) m->def->on_spare(&bt, m);
  if (dmg > 0) {
    if (m->def->on_hit && m->def->on_hit(&bt, m, dmg)) return;
    m->hp = (int16_t)(m->hp - dmg);
    m->hurt = 30, m->shake = 8;
    if (m->hp <= 0) {
      m->hp = 0;
      m->killed = true;
    }
  }
}

/* ---------------------------------------------------------------- the turns */
static void begin_menu(void) {
  /* the first Froggit: Toriel comes and glares at it, and it leaves */
  if (bt.form == FORM_FROGGIT_FIRST && bt.turn >= 1 && count_alive()) {
    bt.phase = P_GLARE, bt.t = 0;
    box_menu();
    say("");
    return;
  }
  bt.phase = P_MENU;
  box_menu();
  say(flavor_text());
  bt.turn++;
  /* the Stained Apron heals a little, every other turn */
  if (sv.armor == ITEM_STAINED_APRON && bt.turn % 2 == 0 && sv.hp < sv.maxhp) sv.hp++;
}

static void begin_talk(void) {
  bt.phase = P_TALK;
  bt.t = 0;
  for (int i = 0; i < bt.nmon; i++) {
    Monster *m = &bt.mon[i];
    m->say = NULL;
    if (!m->alive || m->gone) continue;
    if (m->def->talk) m->say = m->def->talk(&bt, m);
    else {
      int k = 0;
      while (k < 6 && m->def->quotes[k]) k++;
      if (k) m->say = m->def->quotes[rnd_n(k)];
    }
  }
  bt.talk_i = -1;
  for (int i = 0; i < bt.nmon; i++)
    if (bt.mon[i].say) {
      bt.talk_i = i;
      break;
    }
  if (bt.talk_i >= 0) {
    writer_start(&bt.bw, bt.mon[bt.talk_i].say, 0, 0, FONT_SMALL);
    bt.bw.line_h = 9;
  }
}
static bool all_gone(void);
static void win(void);

static void begin_attack(void) {
  /* the attack: one of the monsters' (at random among those alive) */
  int who[MAX_MON], n = 0;
  for (int i = 0; i < bt.nmon; i++)
    if (bt.mon[i].alive && !bt.mon[i].gone && bt.mon[i].def->attack) who[n++] = i;
  if (!n) {
    begin_menu();
    return;
  }
  int i = who[rnd_n(n)];
  bt.atk_owner = i;
  bt.atk = bt.mon[i].def->attack(&bt, &bt.mon[i]);
  if (bt.atk < 0) {
    begin_menu();
    return;
  }
  bt.atk_len = attack_length(bt.atk);
  bt.atk_t = 0;
  board_set(BOARD_W, BOARD_H);
  attack_run(&bt, bt.atk, -1);   /* (t -1: the attack sets its board) */
  bt.phase = P_BOX_IN;
  bullets_clear();
}

static void win(void) {
  int exp = 0, gold = 0;
  for (int i = 0; i < bt.nmon; i++) {
    if (bt.mon[i].killed) exp += bt.mon[i].exp, gold += bt.mon[i].gold;
    else if (bt.mon[i].gone) gold += bt.mon[i].spare_gold;
  }
  gold += bt.gold_won;
  sv.gold = (uint16_t)min_i(sv.gold + gold, 9999);
  bool up = gain_exp(exp);
  bt.exp_won = exp, bt.gold_won = gold;
  char s[200];
  snprintf(s, sizeof s, "* YOU WON!\n* You earned %d EXP and %d gold.%s", exp, gold, up ? "\n* Your LOVE increased." : "");
  say(s);
  bt.phase = P_WON;
  box_menu();
  bullets_clear();
}

static bool all_gone(void) { return count_alive() == 0; }

static void after_player_turn(void) {
  if (all_gone()) {
    bool any_killed = false;
    for (int i = 0; i < bt.nmon; i++) any_killed |= bt.mon[i].killed;
    if (any_killed || bt.gold_won) win();
    else {
      bt.result = BR_SPARED;
      win();
    }
    return;
  }
  begin_talk();
}

static void do_spare(void) {
  bool any = false;
  bt.last_action = 3;
  for (int i = 0; i < bt.nmon; i++) {
    Monster *m = &bt.mon[i];
    if (m->alive && !m->gone && m->spare) m->gone = true, m->fade = 1, any = true;
    else if (m->alive && !m->gone && m->def->on_spare) m->def->on_spare(&bt, m);
  }
  (void)any;
  bt.text[0] = 0;
  after_player_turn();   /* (all spared: the battle's won, as BR_SPARED) */
}

static void do_flee(void) {
  bool can = !bt.no_flee;
  bt.last_action = 4;
  if (!can) {
    after_player_turn();
    return;
  }
  bool ok = sv.armor == ITEM_BANDAGE || rnd_n(100) < 50 + 10 * bt.turn;
  if (!ok) {
    after_player_turn();
    return;
  }
  static const char *const lines[] = {"* Escaped...", "* I'm outta here.", "* I've got better to do.", "* Don't slow me down."};
  say(lines[rnd_n(4)]);
  bt.phase = P_FLEE;
  bt.t = 0;
}

/* ---------------------------------------------------------------- input */
static int alive_index(int k) {   /* the k-th monster still there */
  for (int i = 0; i < bt.nmon; i++)
    if (bt.mon[i].alive && !bt.mon[i].gone && k-- == 0) return i;
  return -1;
}

static void menu_input(uint32_t pressed) {
  switch (bt.phase) {
    case P_MENU:
      if (pressed & K_LEFT) bt.button = (bt.button + 3) & 3;
      if (pressed & K_RIGHT) bt.button = (bt.button + 1) & 3;
      if (pressed & K_Z) {
        bt.target = 0, bt.act = 0, bt.item = 0, bt.mercy = 0, bt.page = 0;
        if (bt.button == B_FIGHT || bt.button == B_ACT) bt.phase = P_TARGET;
        else if (bt.button == B_ITEM) {
          if (item_count()) bt.phase = P_ITEMS;
        } else bt.phase = P_MERCY;
      }
      break;
    case P_TARGET: {
      int n = count_alive();
      if (pressed & K_UP) bt.target = (bt.target + n - 1) % n;
      if (pressed & K_DOWN) bt.target = (bt.target + 1) % n;
      if (pressed & K_X) bt.phase = P_MENU;
      if (pressed & K_Z) {
        if (bt.button == B_FIGHT) {
          bt.last_action = 0;
          bt.target = alive_index(bt.target);
          bt.phase = P_BAR;
          bt.bar_dir = (sv.weapon == ITEM_TOY_KNIFE && rnd_n(2)) ? -1 : 1;
          bt.bar_x = bt.bar_dir > 0 ? 0 : 281 * FX;
          bt.t = 0;
        } else {
          bt.phase = P_ACTS;
          bt.act = 0;
        }
      }
      break;
    }
    case P_ACTS: {
      int m = alive_index(bt.target);
      int n = 1 + bt.mon[m].def->nacts;
      if (pressed & K_LEFT && bt.act % 2) bt.act--;
      if (pressed & K_RIGHT && !(bt.act % 2) && bt.act + 1 < n) bt.act++;
      if (pressed & K_UP && bt.act >= 2) bt.act -= 2;
      if (pressed & K_DOWN && bt.act + 2 < n) bt.act += 2;
      if (pressed & K_X) bt.phase = P_TARGET;
      if (pressed & K_Z) {
        Monster *mo = &bt.mon[m];
        bt.target = m;
        bt.last_action = 1;
        if (bt.act == 0) {
          snprintf(bt.text, sizeof bt.text, "* %s - ATK %d DEF %d\n* %s", mo->def->name, mo->at, mo->df, mo->def->check);
          for (char *p = bt.text + 2; *p && *p != ' '; p++)
            if (*p >= 'a' && *p <= 'z') *p = (char)(*p - 32);
        } else {
          bt.text[0] = 0;
          mo->acts[bt.act - 1]++;
          if (mo->def->act) mo->def->act(&bt, mo, bt.act - 1, bt.text, sizeof bt.text);
        }
        say(bt.text);
        bt.phase = P_RESULT;
      }
      break;
    }
    case P_ITEMS: {
      int n = item_count();
      if (pressed & K_LEFT) bt.item = bt.item % 2 ? bt.item - 1 : (bt.item >= 4 ? bt.item - 3 : bt.item);
      if (pressed & K_RIGHT) bt.item = !(bt.item % 2) && bt.item + 1 < n ? bt.item + 1 : (bt.item % 2 && bt.item + 3 < n ? bt.item + 3 : bt.item);
      if (pressed & K_UP && bt.item % 4 >= 2) bt.item -= 2;
      if (pressed & K_DOWN && bt.item % 4 < 2 && bt.item + 2 < n) bt.item += 2;
      if (bt.item >= n) bt.item = n - 1;
      if (pressed & K_X) bt.phase = P_MENU;
      if (pressed & K_Z) {
        bt.last_action = 2;
        /* a monster may answer the item (a dog and the Stick) */
        bool answered = false;
        for (int i = 0; i < bt.nmon && !answered; i++) {
          Monster *m = &bt.mon[i];
          if (m->alive && !m->gone && m->def->on_item)
            answered = m->def->on_item(&bt, m, sv.items[bt.item], bt.text, sizeof bt.text);
        }
        if (!answered) item_use(bt.item, bt.text, sizeof bt.text, 1);
        say(bt.text);
        bt.phase = P_RESULT;
      }
      break;
    }
    case P_MERCY:
      if (pressed & K_UP) bt.mercy = 0;
      if (pressed & K_DOWN && !bt.no_flee) bt.mercy = 1;
      if (pressed & K_X) bt.phase = P_MENU;
      if (pressed & K_Z) {
        if (bt.mercy == 0) do_spare();
        else do_flee();
      }
      break;
  }
}

/* ---------------------------------------------------------------- the frame */
void battle_update(uint32_t keys, uint32_t pressed) {
  if (over) return;
  bt.t++;
  box_step();
  if (bt.scripted && bt.phase != P_INTRO) {
    script->update(keys, pressed);
    if (bt.result == BR_SCRIPTED) battle_end(BR_SCRIPTED);
    return;
  }
  /* (a monster's ways may end it at once: Mettaton, his switch flipped or his show a hit) */
  if (bt.result == BR_SCRIPTED) {
    battle_end(BR_SCRIPTED);
    return;
  }
  for (int i = 0; i < bt.nmon; i++) {
    Monster *m = &bt.mon[i];
    if (m->hurt) m->hurt--;
    if (m->dmg_t) m->dmg_t--;
    if (m->fade && m->fade < 30) m->fade++;
    if (m->dust && m->dust < 60) m->dust++;
  }
  if (bt.inv) bt.inv--;
  switch (bt.phase) {
    case P_INTRO:
      if (bt.t >= 24) {
        bt.phase = P_MENU;
        say(bt.text);
        bt.turn = 1;
      }
      break;
    case P_MENU:
      writer_update(&bt.w, pressed & K_X ? K_X : 0);
      menu_input(pressed);
      break;
    case P_TARGET:
    case P_ACTS:
    case P_ITEMS:
    case P_MERCY:
      menu_input(pressed);
      break;
    case P_BAR:
      bt.bar_x += bt.bar_dir * 7 * FX / 2;
      if (pressed & K_Z) strike();
      else if (bt.bar_x < 0 || bt.bar_x > 281 * FX) {
        bt.bar_x = -1;
        bt.strike_dmg = -1;
        bt.target = bt.target;
        bt.mon[bt.target].dmg_shown = -1, bt.mon[bt.target].dmg_t = 40;
        if ((bt.mon[bt.target].def->flags & MF_MISS_IS_SPARE) && bt.mon[bt.target].def->on_spare)
          bt.mon[bt.target].def->on_spare(&bt, &bt.mon[bt.target]);
        bt.phase = P_STRIKE, bt.t = 20;
      }
      break;
    case P_STRIKE:
      if (bt.t == 18) strike_land();
      if (bt.t >= 60) {
        Monster *m = &bt.mon[bt.target];
        if (m->killed && m->alive) {
          m->alive = false, m->gone = true, m->dust = 1;
          sv.kills++;
          {
            const RoomInfo *ri = world_room_info();
            if (ri && (ri->area == AREA_SNOWDIN || ri->area == AREA_SNOWTOWN)) set_flag(F_KILLS_SNOWDIN, flag(F_KILLS_SNOWDIN) + 1);
          }
        }
        after_player_turn();
      }
      break;
    case P_RESULT:
      if (!writer_update(&bt.w, pressed)) {
        if (bt.phase == P_RESULT) after_player_turn();
      }
      if (bt.w.finished && bt.phase == P_RESULT) after_player_turn();
      break;
    case P_TALK:
      if (bt.talk_i < 0) {
        begin_attack();
        break;
      }
      if (!writer_update(&bt.bw, pressed)) {
        /* the next monster with something to say */
        int i = bt.talk_i + 1;
        while (i < bt.nmon && !bt.mon[i].say) i++;
        if (i < bt.nmon) {
          bt.talk_i = i;
          writer_start(&bt.bw, bt.mon[i].say, 0, 0, FONT_SMALL);
          bt.bw.line_h = 9;
        } else {
          /* some monsters leave after they have spoken (Napstablook) */
          for (int k = 0; k < bt.nmon; k++) {
            Monster *m = &bt.mon[k];
            if (m->alive && !m->gone && m->def->ends && m->def->ends(&bt, m)) m->gone = true, m->fade = 1;
          }
          if (all_gone()) win();
          else begin_attack();
        }
      }
      break;
    case P_BOX_IN:
      if (box_settled()) {
        bt.sx = board_cx() * FX, bt.sy = board_cy() * FX;
        bt.phase = P_ATTACK;
      }
      break;
    case P_ATTACK:
      soul_update(keys);
      attack_run(&bt, bt.atk, bt.atk_t);
      bullets_update();
      soul_hit_test();
      if (sv.hp <= 0 && bt.captures) {
        sv.hp = 1;
        battle_end(BR_SCRIPTED);
        break;
      }
      if (sv.hp <= 0) {
        bt.phase = P_DEATH, bt.t = 0;
        break;
      }
      if (++bt.atk_t >= bt.atk_len) {
        bullets_clear();
        bt.phase = P_BOX_OUT;
        box_menu();
      }
      break;
    case P_BOX_OUT:
      if (box_settled()) begin_menu();
      break;
    case P_WON:
      if (!writer_update(&bt.w, pressed)) battle_end(bt.result == BR_SPARED ? BR_SPARED : BR_WON);
      break;
    case P_FLEE:
      writer_update(&bt.w, 0);
      if (bt.t >= 40) battle_end(BR_FLED);
      break;
    case P_DEATH:
      if (bt.t >= 70) battle_end(BR_DIED);
      break;
    case P_GLARE:
      if (bt.t == 40) bt.mon[0].gone = true, bt.mon[0].fade = 1;
      if (bt.t >= 80) win();
      break;
  }
}

/* ---------------------------------------------------------------- drawing */
static void draw_monster(Monster *m, int i) {
  const MonDef *d = m->def;
  if (m->gone && !m->dust && m->fade >= 30) return;
  if (!m->alive && !m->dust) return;
  int x = m->x + (m->hurt ? (m->hurt / 2 % 2 ? 2 : -2) * (m->hurt > 10) : 0), y = m->y;
  if (d->draw) {
    d->draw(&bt, m, x, y);
    return;
  }
  uint16_t s = m->spr_override ? (uint16_t)m->spr_override : m->hurt && d->spr_hurt ? d->spr_hurt : d->spr_idle;
  int fr = m->hurt ? 0 : (int)(g_frame / (d->idle_speed ? d->idle_speed : 6));
  if (m->dust) {
    /* turning to dust: drawn whiter and thinner, from the top down */
    int a = 255 - m->dust * 4;
    if (a > 0) gfx_sprite_color(s, 0, x, y, 0, C_WHITE, (uint8_t)a);
    return;
  }
  uint8_t alpha = m->fade ? (uint8_t)max_i(0, 128 - m->fade * 4) : 255;
  gfx_sprite(s, fr, x, y, 0, alpha);
  (void)i;
}

void battle_draw_hud(void) {
  char s[40];
  gfx_text(FONT_SMALL, 15, 201, sv.name, C_WHITE, 255);
  snprintf(s, sizeof s, "LV %d", sv.lv);
  gfx_text(FONT_SMALL, 66, 201, s, C_WHITE, 255);
  gfx_sprite(SPR_HPNAME, 0, 122, 202, 0, 255);
  int w = sv.maxhp * 6 / 10 + 1, f = max_i(0, sv.hp) * 6 / 10;
  if (sv.hp > 0 && f == 0) f = 1;
  gfx_rect(137, 200, w, 10, rgb565(0xC00000), 255);
  gfx_rect(137, 200, f, 10, C_YELLOW, 255);
  snprintf(s, sizeof s, "%d / %d", max_i(0, sv.hp), sv.maxhp);
  gfx_text(FONT_SMALL, 137 + w + 8, 201, s, C_WHITE, 255);
}

void battle_draw_buttons(void) {
  static const uint16_t spr[4] = {SPR_FIGHTBT, SPR_TALKBT, SPR_ITEMBT, SPR_SPAREBT};
  bool menu = bt.phase >= P_MENU && bt.phase <= P_MERCY;
  for (int i = 0; i < 4; i++) {
    bool on = menu && bt.button == i;
    gfx_sprite(spr[i], on ? 1 : 0, button_x[i], BUTTON_Y, 0, 255);
    if (on && bt.phase == P_MENU) gfx_sprite(SPR_HEART, 0, button_x[i] + 8, BUTTON_Y + 11, 0, 255);
  }
}

/* "* Name" lines in the box, two columns or one */
static void draw_list(const char *const *names, int n, int sel, bool two_cols, const uint16_t *colors) {
  for (int i = 0; i < n; i++) {
    int col = two_cols ? i % 2 : 0, row = two_cols ? i / 2 : i;
    int x = 50 + col * 128, y = 133 + row * 16;
    char s[48];
    snprintf(s, sizeof s, "* %s", names[i]);
    gfx_text(FONT_MAIN, x, y, s, colors ? colors[i] : C_WHITE, 255);
    if (i == sel) gfx_sprite(SPR_HEART, 0, x - 14, y + 7, 0, 255);
  }
}

static void draw_box_contents(void) {
  switch (bt.phase) {
    case P_MENU:
    case P_RESULT:
    case P_WON:
    case P_FLEE:
      writer_draw(&bt.w, C_WHITE);
      break;
    case P_TARGET: {
      const char *names[MAX_MON];
      uint16_t cols[MAX_MON];
      int n = 0;
      for (int i = 0; i < bt.nmon; i++)
        if (bt.mon[i].alive && !bt.mon[i].gone) {
          names[n] = bt.mon[i].def->name;
          cols[n] = bt.mon[i].spare && flag(F_FROGGIT_YELLOW) != 1 ? (flag(F_FROGGIT_YELLOW) == 2 ? rgb565(0xFF80C0) : C_YELLOW) : C_WHITE;
          if (bt.button == B_FIGHT) {
            /* the HP bar, in the game's green over red */
            int y = 133 + n * 16 + 3, hp = bt.mon[i].hp * 50 / max_i(1, bt.mon[i].maxhp);
            gfx_rect(200, y, 50, 8, rgb565(0xC00000), 255);
            gfx_rect(200, y, hp, 8, rgb565(0x00FF00), 255);
          }
          n++;
        }
      draw_list(names, n, bt.target, false, cols);
      break;
    }
    case P_ACTS: {
      Monster *m = &bt.mon[alive_index(bt.target)];
      const char *names[6] = {"Check"};
      int n = 1;
      for (int i = 0; i < m->def->nacts; i++) names[n++] = m->def->acts[i];
      draw_list(names, n, bt.act, true, NULL);
      break;
    }
    case P_ITEMS: {
      const char *names[4];
      int page = bt.item / 4, n = 0;
      for (int i = page * 4; i < page * 4 + 4 && i < item_count(); i++) names[n++] = item_info(sv.items[i])->short_name;
      draw_list(names, n, bt.item % 4, true, NULL);
      char s[16];
      snprintf(s, sizeof s, "PAGE %d", page + 1);
      gfx_text(FONT_MAIN, 236, 165, s, C_WHITE, 255);
      break;
    }
    case P_MERCY: {
      const char *names[2] = {"Spare", "Flee"};
      bool any = false;
      for (int i = 0; i < bt.nmon; i++) any |= bt.mon[i].alive && !bt.mon[i].gone && bt.mon[i].spare;
      uint16_t cols[2] = {any && flag(F_FROGGIT_YELLOW) != 1 ? (flag(F_FROGGIT_YELLOW) == 2 ? rgb565(0xFF80C0) : C_YELLOW) : C_WHITE, C_WHITE};
      draw_list(names, bt.no_flee ? 1 : 2, bt.mercy, false, cols);
      break;
    }
    case P_BAR:
    case P_STRIKE:
      gfx_sprite(SPR_TARGET, 0, 19, 128, 0, 255);
      if (bt.phase == P_BAR || (bt.t / 2) % 2) {
        int bx = 19 + bt.bar_x / FX - 3;
        if (bt.bar_x >= 0) gfx_sprite(SPR_TARGETCHOICE, bt.phase == P_STRIKE ? (bt.t / 3) % 2 : 0, bx, 128, 0, 255);
      }
      break;
    default:
      break;
  }
}

static void draw_bubble(void) {
  if (bt.phase != P_TALK || bt.talk_i < 0) return;
  Monster *m = &bt.mon[bt.talk_i];
  int bx = m->x + 30, by = m->y - 70;
  if (bx + 104 > 316) bx = 316 - 104;
  int w = 100, h = 44;
  gfx_rect(bx, by, w, h, C_WHITE, 255);
  gfx_rect(bx - 6, by + 18, 6, 4, C_WHITE, 255);
  bt.bw.x = bx + 6, bt.bw.y = by + 5;
  writer_draw(&bt.bw, C_BLACK);
}

static void draw_damage(void) {
  for (int i = 0; i < bt.nmon; i++) {
    Monster *m = &bt.mon[i];
    if (!m->dmg_t) continue;
    int y = m->y - 70 - (m->dmg_t > 35 ? (45 - m->dmg_t) : 10);
    char s[16];
    if (m->dmg_shown <= 0) {
      gfx_text(FONT_MAIN, m->x - 16, y, "MISS", rgb565(0xC0C0C0), 255);
    } else {
      snprintf(s, sizeof s, "%d", m->dmg_shown);
      gfx_text(FONT_MAIN, m->x - 4 * (int)strlen(s), y, s, C_RED, 255);
      /* the HP bar, going down */
      int w = 60, shown = m->bar_shown - (int)((m->bar_shown - max_i(0, m->hp)) * min_i(45 - m->dmg_t, 15) / 15);
      gfx_rect(m->x - w / 2, y + 14, w, 7, rgb565(0x404040), 255);
      gfx_rect(m->x - w / 2, y + 14, shown * w / max_i(1, m->maxhp), 7, rgb565(0x00FF00), 255);
    }
  }
}

void battle_draw(void) {
  if (bt.phase == P_INTRO) {
    /* the SOUL flickers where Frisk was, then flies to FIGHT */
    if (bt.t < 12) {
      if ((bt.t / 2) % 2 == 0) gfx_sprite(SPR_HEART, 0, heart_x, heart_y, 0, 255);
      return;
    }
    int k = bt.t - 12;
    int tx = bt.scripted ? soul_x() : button_x[0] + 8, ty = bt.scripted ? soul_y() : BUTTON_Y + 11;
    int x = heart_x + (tx - heart_x) * k / 12, y = heart_y + (ty - heart_y) * k / 12;
    gfx_sprite(SPR_HEART, 0, x, y, 0, 255);
    return;
  }
  if (bt.phase == P_DEATH) {
    int x = soul_x(), y = soul_y();
    if (bt.t < 25) gfx_sprite(SPR_HEART, 0, x, y, 0, 255);
    else if (bt.t < 45) gfx_sprite(SPR_HEARTBREAK, 0, x, y, 0, 255);
    else
      for (int k = 0; k < 6; k++) {
        int dt = bt.t - 45;
        gfx_sprite(SPR_HEARTSHARDS, (dt / 4) % 4, x + (k - 3) * dt / 2, y - 10 + dt * dt / 30 - (k % 3) * 6, 0, 255);
      }
    return;
  }
  if (bt.scripted) {
    script->draw();
    return;
  }
  gfx_sprite(SPR_BATTLEBG, 0, 0, 0, 0, 255);
  for (int i = 0; i < bt.nmon; i++) draw_monster(&bt.mon[i], i);
  if (bt.phase == P_GLARE || (bt.form == FORM_FROGGIT_FIRST && bt.phase == P_WON))
    gfx_sprite(SPR_TORIEL_B, 0, 330 - min_i(bt.phase == P_GLARE ? bt.t : 30, 30) * 2, 120, 0, 255);
  /* the box */
  gfx_rect(bt.x0, bt.y0, bt.x1 - bt.x0, bt.y1 - bt.y0, C_WHITE, 255);
  gfx_rect(bt.x0 + 2, bt.y0 + 2, bt.x1 - bt.x0 - 4, bt.y1 - bt.y0 - 4, C_BLACK, 255);
  gfx_clip(bt.x0 + 2, bt.y0 + 2, bt.x1 - bt.x0 - 4, bt.y1 - bt.y0 - 4);
  draw_box_contents();
  if (bt.phase == P_ATTACK) {
    if (bt.soul_mode == SOUL_PURPLE)
      for (int i = 0; i < NSTRINGS; i++) gfx_rect(bt.x0 + 2, soul_string_y(i), bt.x1 - bt.x0 - 4, 1, rgb565(0xD535D9), 255);
    attack_draw(&bt, bt.atk, bt.atk_t);
    bullets_draw();
    if (!bt.inv || (bt.inv / 2) % 2) gfx_sprite(SPR_HEART, bt.soul_mode, soul_x(), soul_y(), 0, 255);
    if (bt.soul_mode == SOUL_GREEN) {
      /* its ring, and the shield on the side it faces */
      int cx = soul_x(), cy = soul_y();
      for (int a = 0; a < 256; a += 8)
        gfx_rect(cx + 13 * gfx_cos(a) / 1024, cy + 13 * gfx_sin(a) / 1024, 1, 1, rgb565(0x00C000), 255);
      static const int8_t ox[4] = {0, 0, -11, 11}, oy[4] = {11, -11, 0, 0};
      int x = cx + ox[bt.shield], y = cy + oy[bt.shield];
      if (bt.shield < 2) gfx_rect(x - 7, y - 1, 15, 3, rgb565(0x3070FF), 255);
      else gfx_rect(x - 1, y - 7, 3, 15, rgb565(0x3070FF), 255);
    }
  }
  gfx_clip_all();
  if (bt.phase == P_FLEE) gfx_sprite(SPR_HEARTGTFO, (bt.t / 3) % 2, button_x[3] + 8 - bt.t * 3, BUTTON_Y + 11, 0, 255);
  if (bt.phase == P_STRIKE && bt.t < 18 && bt.strike_dmg >= 0) {
    Monster *m = &bt.mon[bt.target];
    gfx_sprite(SPR_STRIKE, bt.t / 3, m->x, m->y - 30, 0, 255);
  }
  draw_damage();
  draw_bubble();
  battle_draw_hud();
  battle_draw_buttons();
}
