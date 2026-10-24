/* The song (game.h): Friday Night Funkin's rules, after its own code (FunkinCrew/Funkin, Apache-2.0: PlayState,
 * Scoring, Strumline, BaseCharacter, Conductor, Countdown, PopUpStuff, Constants). */
#include <math.h>
#include <string.h>
#include "game.h"
#include "inflate.h"

/* PBOT1's points for a press |ms| off: 500 under 5 ms, then int(500 * (1 - 1 / (1 + e^(-0.08 (ms - 54.99)))) + 9)
 * (Scoring.scoreNotePBOT1, worked out in double precision as the game does) */
static const int16_t PBOT1[161] = {
    500, 500, 500, 500, 500, 499, 499, 498, 497, 496, 495, 494, 493, 492, 490, 489,
    487, 486, 484, 482, 480, 478, 475, 473, 470, 467, 464, 460, 457, 453, 449, 445,
    440, 435, 430, 424, 419, 413, 406, 400, 393, 385, 378, 370, 362, 353, 345, 336,
    327, 317, 308, 298, 288, 278, 268, 258, 248, 238, 229, 219, 209, 200, 190, 181,
    172, 163, 155, 147, 139, 131, 124, 117, 111, 104, 98, 92, 87, 82, 77, 72,
    68, 64, 60, 57, 53, 50, 47, 44, 42, 39, 37, 35, 33, 31, 30, 28,
    27, 25, 24, 23, 22, 21, 20, 19, 18, 17, 17, 16, 16, 15, 15, 14,
    14, 13, 13, 13, 12, 12, 12, 11, 11, 11, 11, 11, 10, 10, 10, 10,
    10, 10, 10, 10, 10, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9,
    9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9,
    9,
};

/* health (millionths of the bar's 2.0): Constants.HEALTH_* */
#define H_SICK 30000      /* +1.5% of the bar */
#define H_GOOD 15000
#define H_SHIT (-20000)
#define H_MISS (-80000)   /* -4%, a ghost miss too */
#define MISS_POINTS 100
#define GHOST_POINTS 10
#define HOLD_DROP_MIN 160000   /* a hold let go of with more than this left costs points */

int fnf_judge(int32_t ms) {
  int32_t a = ms < 0 ? -ms : ms;
  return a <= 45 ? J_SICK : a <= 90 ? J_GOOD : a <= 135 ? J_BAD : a <= 160 ? J_SHIT : J_MISS;
}

int fnf_points(int32_t ms) {
  int32_t a = ms < 0 ? -ms : ms;
  return a > 160 ? -MISS_POINTS : PBOT1[a];
}

int32_t fnf_completion(const fnf_game_t *g) {
  if (!g->total) return 0;
  int32_t n = (int32_t)g->tally[J_SICK] + g->tally[J_GOOD] - g->tally[J_MISS];
  if (n <= 0) return 0;
  int32_t c = (int32_t)((int64_t)n * 10000 / g->total);
  return c > 10000 ? 10000 : c;
}

int fnf_rank(const fnf_game_t *g) {
  if (!g->total) return 0;
  if (g->tally[J_SICK] == g->total) return 5;
  /* (Scoring.tallyCompletion is a float: 0.9 is reached at 90%, so compare exactly) */
  int64_t n = (int64_t)g->tally[J_SICK] + g->tally[J_GOOD] - g->tally[J_MISS], t = g->total;
  if (n >= t) return 4;
  if (n * 10 >= t * 9) return 3;
  if (n * 10 >= t * 8) return 2;
  if (n * 10 >= t * 6) return 1;
  return 0;
}

int32_t fnf_score(const fnf_game_t *g) { return (int32_t)(g->score / 1000); }   /* (Std.int: toward 0) */

/* ------------------------------------------------------------------------------------------------ the beat */
static int64_t floor_div(int64_t a, int64_t b) { return a >= 0 ? a / b : -((-a + b - 1) / b); }

static const fnf_tempo_t *tempo_at(const fnf_game_t *g, int32_t t) {
  const fnf_var_t *v = &fnf_var[g->var];
  const fnf_tempo_t *tp = &fnf_tempo[v->tempo];
  for (int i = 1; i < v->ntempo; i++)
    if (fnf_tempo[v->tempo + i].t <= t) tp = &fnf_tempo[v->tempo + i];
  return tp;
}

int32_t fnf_steps(const fnf_game_t *g, int32_t t) {
  const fnf_tempo_t *tp = tempo_at(g, t);
  return tp->steps0 + (int32_t)floor_div((int64_t)(t - tp->t) * (1 << 24), tp->step);
}

int32_t fnf_step_len(const fnf_game_t *g, int32_t t) { return (int32_t)(tempo_at(g, t)->step >> 8); }

/* when step k starts */
static int32_t step_time(const fnf_game_t *g, int32_t k) {
  const fnf_var_t *v = &fnf_var[g->var];
  const fnf_tempo_t *tp = &fnf_tempo[v->tempo];
  int64_t s = (int64_t)k * 65536;
  for (int i = 1; i < v->ntempo; i++)
    if (fnf_tempo[v->tempo + i].steps0 <= s) tp = &fnf_tempo[v->tempo + i];
  int64_t num = (s - tp->steps0) * (int64_t)tp->step;
  return tp->t + (int32_t)-floor_div(-num, (int64_t)1 << 24);   /* (rounded up: the first microsecond in it) */
}

int32_t fnf_start_time(const fnf_game_t *g) {
  /* the countdown starts 5 beats before the song (Countdown.performCountdown), after half a second of the notes
   * coming in (PlayState's vwoosh) */
  return step_time(g, -20) - 500000;
}

/* ------------------------------------------------------------------------------------------------ characters */
static const fnf_anim_t *anim_of(const fnf_actor_t *a) {
  return a->anim == 255 ? 0 : &fnf_anim[fnf_char[a->chr].anim + a->anim];
}

static bool finished(const fnf_actor_t *a, int32_t t) {
  const fnf_anim_t *an = anim_of(a);
  if (!an) return true;
  if (an->flags & FNF_AF_LOOP) return false;
  return (int64_t)(t - a->since) * an->fps >= (int64_t)an->len * 1000000;
}

/* Bopper.playAnimation */
static void play(fnf_actor_t *a, int anim, bool restart, bool exclusive, int32_t t) {
  if (a->chr == 255 || anim == 255) return;
  if (a->locked && finished(a, t)) a->locked = 0;
  if (a->locked && !(anim == a->anim && restart)) return;
  if (!restart && anim == a->anim && !finished(a, t)) return;
  a->anim = (uint8_t)anim, a->since = t;
  const fnf_anim_t *an = anim_of(a);
  a->singing = an->flags & FNF_AF_SING ? (an->flags & FNF_AF_MISS ? 2 : 1) : 0;
  if (exclusive) a->locked = 1;
}

/* BaseCharacter.dance */
static void dance(fnf_actor_t *a, bool force, int32_t t) {
  if (a->chr == 255) return;
  const fnf_char_t *c = &fnf_char[a->chr];
  if (!force) {
    if (a->singing) return;
    const fnf_anim_t *an = anim_of(a);
    if (an && !(an->flags & FNF_AF_DANCE) && !finished(a, t)) return;
  }
  if (c->flags & FNF_CHAR_ALTERNATE) {
    play(a, c->slot[a->danced ? FNF_SLOT_DANCE_R : FNF_SLOT_DANCE_L], false, false, t);
    a->danced ^= 1;
  } else {
    play(a, c->slot[FNF_SLOT_DANCE_L], false, false, t);
  }
}

static int find_anim(const fnf_actor_t *a, int name) {
  const fnf_char_t *c = &fnf_char[a->chr];
  for (int i = 0; i < c->count; i++)
    if (fnf_anim_name[c->anim + i] == name) return i;
  return 255;
}

/* singing a note (BaseCharacter.onNoteHit, and the characters' scripts for note kinds) */
static void sing(fnf_actor_t *a, int dir, int kind, int32_t t) {
  if (a->chr == 255) return;
  const fnf_char_t *c = &fnf_char[a->chr];
  int anim = c->slot[FNF_SLOT_SING + dir];
  if (kind) {
    for (int i = 0; i < FNF_KINDS; i++)
      if (fnf_kind[i].chr == a->chr && fnf_kind[i].kind == kind) {
        int k = fnf_kind[i].anim[dir];
        if (k == 254) return;   /* noanim */
        if (k != 255) {
          if (!(fnf_anim[c->anim + k].flags & FNF_AF_SING)) {   /* "ugh", "hehPrettyGood", a punch... */
            play(a, k, true, true, t);
            return;
          }
          anim = k;
        }
        break;
      }
  }
  play(a, anim, true, false, t);
  a->hold = t;
}

static void miss_anim(fnf_actor_t *a, int dir, int32_t t) {
  if (a->chr == 255) return;
  bool was = a->singing;
  play(a, fnf_char[a->chr].slot[FNF_SLOT_MISS + dir], true, false, t);
  if (!was) a->hold = t;   /* (the hold timer only runs while singing) */
}

/* Girlfriend cheering a combo, or crying over a lost one (BaseCharacter.playComboAnimation, playComboDropAnimation) */
static void gf_combo(fnf_game_t *g, unsigned combo, bool drop, int32_t t) {
  for (int i = 0; i < FNF_ACTORS; i++) {
    fnf_actor_t *a = &g->actor[i];
    if (a->chr == 255 || a->side != 2) continue;
    int best = 255;
    if (!drop) {
      int name = combo == 50 ? FNF_ANIM_COMBO50 : combo == 200 ? FNF_ANIM_COMBO200 : -1;
      if (name >= 0) best = find_anim(a, name);
    } else if (combo >= 70) {
      best = find_anim(a, FNF_ANIM_DROP70);
    }
    if (best != 255) play(a, best, true, true, t);
  }
}

static bool any_held(const fnf_game_t *g) { return (g->held & 15) != 0; }

/* the hold timer: singing stops singTime steps after the last note (twice that for a miss), unless the player
 * still holds a key (BaseCharacter.onUpdate) */
static void stop_singing(fnf_game_t *g, int32_t t) {
  int32_t step = fnf_step_len(g, t);
  for (int i = 0; i < FNF_ACTORS; i++) {
    fnf_actor_t *a = &g->actor[i];
    if (a->chr == 255 || !a->singing) continue;
    int64_t limit = (int64_t)fnf_char[a->chr].sing * step * (a->singing == 2 ? 2 : 1);
    if (t - a->hold <= limit) continue;
    if (a->side == 0 && any_held(g)) continue;
    a->singing = 0;
    a->locked = 0;
    dance(a, true, t);
  }
}

/* ------------------------------------------------------------------------------------------------ pop ups */
static uint32_t rnd(fnf_game_t *g, uint32_t lo, uint32_t hi) {   /* lo..hi, as FlxG.random.int */
  g->rng = g->rng * 1664525u + 1013904223u;
  return lo + (g->rng >> 8) % (hi - lo + 1);
}

static fnf_pop_t *new_pop(fnf_game_t *g) {
  fnf_pop_t *p = &g->pop[g->popn];
  g->popn = (uint8_t)((g->popn + 1) % FNF_POPS);
  return p;
}

/* PopUpStuff, in screen pixels: the HUD's 1280 x 720 at a quarter across and a third down */
static void pop_rating(fnf_game_t *g, int j, int32_t t) {
  fnf_pop_t *p = new_pop(g);
  p->what = (uint8_t)j, p->at = t;
  p->x = (int16_t)(1280 * 474 / 1000 / 4), p->y = (int16_t)((720 * 45 / 100 - 60) / 3);
  p->ay = 550 / 3;
  p->vy = (int16_t)-((int32_t)rnd(g, 140, 175) / 3);
  p->vx = (int16_t)-((int32_t)rnd(g, 0, 10) / 4);
  p->fade = t + fnf_step_len(g, t) * 4;
}

static void pop_combo(fnf_game_t *g, unsigned combo, int32_t t) {
  int digits[6], n = 0;
  unsigned c = combo;
  while (c && n < 6) digits[n++] = (int)(c % 10), c /= 10;
  while (n < 3) digits[n++] = 0;
  for (int i = 0; i < n; i++) {
    fnf_pop_t *p = new_pop(g);
    p->what = (uint8_t)(10 + digits[i]), p->at = t;
    p->x = (int16_t)((1280 * 507 / 1000 - 36 * (i + 1) - 65 + 47 * 45 / 100) / 4);
    p->y = (int16_t)((720 * 44 / 100 + 119 * 45 / 200) / 3);
    p->ay = (int16_t)(rnd(g, 250, 300) / 3);
    p->vy = (int16_t)-((int32_t)rnd(g, 130, 150) / 3);
    p->vx = (int16_t)(((int32_t)rnd(g, 0, 10) - 5) / 4);
    p->fade = t + fnf_step_len(g, t) * 8;
  }
}

/* ------------------------------------------------------------------------------------------------ scoring */
static void health(fnf_game_t *g, int32_t d) {
  g->health += d;
  if (g->health > FNF_HEALTH_MAX) g->health = FNF_HEALTH_MAX;
  if (g->health <= 0) g->health = 0;
}

/* PlayState.applyScore */
static void apply(fnf_game_t *g, int32_t points_milli, int j, int32_t dh, bool breaks, int32_t t) {
  if (j >= 0) g->tally[j]++;
  health(g, dh);
  if (breaks) {
    if (g->combo >= 10) pop_combo(g, 0, t);
    g->combo = 0;
  } else {
    g->combo++;
    if (g->combo > g->max_combo) g->max_combo = g->combo;
  }
  g->score += points_milli;
}

static void actors_on(fnf_game_t *g, int side, int dir, int kind, bool miss, int32_t t) {
  for (int i = 0; i < FNF_ACTORS; i++) {
    fnf_actor_t *a = &g->actor[i];
    if (a->chr == 255 || a->side != side) continue;
    if (miss) miss_anim(a, dir, t);
    else sing(a, dir, kind, t);
  }
}

static bool scoreable(const fnf_note_t *n) { return n->kind != FNF_KIND_NON_SCOREABLE; }

static bool holding(const fnf_note_t *n) { return n->hold == HOLD_ON; }

static void hit(fnf_game_t *g, fnf_note_t *n, int32_t judged) {
  int32_t t = g->now;
  int32_t ms = (judged - n->t) / 1000;   /* (Std.int: toward 0) */
  int j = fnf_judge(ms);
  int32_t dh = j == J_SICK ? H_SICK : j == J_GOOD ? H_GOOD : j == J_SHIT ? H_SHIT : 0;
  bool breaks = j == J_BAD || j == J_SHIT;
  unsigned gf_count = scoreable(n) ? g->combo + 1u : g->combo;
  actors_on(g, 0, n->lane & 3, n->kind, false, t);
  if (scoreable(n)) gf_combo(g, gf_count, breaks, t);
  g->strum[n->lane] = STRUM_CONFIRM, g->strum_at[n->lane] = t;
  if (j == J_SICK) g->splash_at[n->lane & 3] = t;
  n->state = breaks ? NOTE_FADED : NOTE_GONE;   /* (Strumline.hitNote: a combo break leaves it, faded) */
  n->hit = t;
  if (n->len > 0) n->hold = HOLD_ON;
  if (scoreable(n)) {
    g->hits++;
    apply(g, fnf_points(ms) * 1000, j, dh, breaks, t);
    pop_rating(g, j, t);
    if (g->combo >= 10) pop_combo(g, g->combo, t);
  }
}

static void miss(fnf_game_t *g, fnf_note_t *n, int32_t t) {
  if (!scoreable(n)) {   /* (NonScoreableNoteKind: hidden, no penalty) */
    n->state = NOTE_GONE;
    return;
  }
  unsigned before = g->combo;
  n->state = NOTE_MISSED;
  actors_on(g, 0, n->lane & 3, 0, true, t);
  gf_combo(g, before, true, t);
  apply(g, -MISS_POINTS * 1000, J_MISS, H_MISS, true, t);
  /* its hold goes with it: never held, its whole length counts */
  if (n->len > 0) n->hold = HOLD_MISSED;
  if (n->len > HOLD_DROP_MIN) {
    g->score -= (int64_t)n->len / 8;   /* -125 points a second left */
    if (g->combo >= 10) pop_combo(g, 0, t);
    g->combo = 0;
    actors_on(g, 0, n->lane & 3, 0, true, t);
  }
}

/* a hold let go of at time t (Strumline: noticed once the note's time has passed) */
static void drop(fnf_game_t *g, fnf_note_t *n, int32_t t) {
  int32_t left = n->t + n->len - t;
  n->hold = HOLD_DROPPED;
  if (!scoreable(n) || left <= HOLD_DROP_MIN) return;
  g->score -= (int64_t)left / 8;
  unsigned before = g->combo;
  if (g->combo >= 10) pop_combo(g, 0, t);
  g->combo = 0;
  actors_on(g, 0, n->lane & 3, 0, true, t);
  gf_combo(g, before, true, t);
}

/* ------------------------------------------------------------------------------------------------ the notes */
static uint32_t leb(fnf_game_t *g, bool *ok) {
  uint32_t v = 0;
  for (int s = 0; s < 35; s += 7) {
    if (g->pos >= g->end) {
      *ok = false;
      return 0;
    }
    uint8_t b = g->notes[g->pos++];
    v |= (uint32_t)(b & 127) << s;
    if (!(b & 128)) return v;
  }
  *ok = false;
  return 0;
}

/* the next note out of the chart into a free place */
static bool next_note(fnf_game_t *g, fnf_note_t *out) {
  bool ok = true;
  uint32_t dt = leb(g, &ok), info = leb(g, &ok);
  uint32_t len = info & 8 ? leb(g, &ok) : 0;
  if (!ok || (info >> 4) >= FNF_NOTE_KINDS || dt > 600000000u || len > 600000000u) return false;
  out->t = g->next + (int32_t)dt;
  out->len = (int32_t)len;
  out->lane = (uint8_t)(info & 7), out->kind = (uint8_t)(info >> 4);
  out->state = NOTE_WAIT, out->hold = HOLD_NONE, out->hit = 0;
  return true;
}

/* a place a note no longer needs: gone with no hold going on, or scrolled off the screen */
static bool slot_free(const fnf_note_t *n, int32_t now) {
  if (n->state == NOTE_WAIT || n->hold == HOLD_ON) return false;
  if (n->state == NOTE_GONE) return n->len == 0 || n->hold != HOLD_MISSED;
  return now - (n->t + n->len) > 2000000;
}

/* notes come on when they are this far ahead: the screen's height at the slowest speed they may scroll at */
static int32_t lookahead(const fnf_game_t *g) {
  int32_t s = g->speed < g->speed_to ? g->speed : g->speed_to;
  if (s < 300) s = 300;
  return (int32_t)(1700000LL * 1000 / s);
}

static void bring_notes(fnf_game_t *g) {
  int32_t ahead = lookahead(g);
  while (g->pos < g->end) {
    /* (peek: the next note's time) */
    uint32_t save = g->pos;
    bool ok = true;
    uint32_t dt = leb(g, &ok);
    g->pos = save;
    if (!ok) return;
    if ((int64_t)g->next + dt > (int64_t)g->now + ahead) return;
    int free = -1;
    for (int i = 0; i < FNF_ACTIVE; i++)
      if (slot_free(&g->note[i], g->now)) {
        free = i;
        break;
      }
    if (free < 0) return;   /* (never: 128 places, the busiest charts use 70) */
    fnf_note_t n;
    if (!next_note(g, &n)) {
      g->pos = g->end;
      return;
    }
    g->next = n.t;
    g->note[free] = n;
    unsigned busy = 0;
    for (int i = 0; i < FNF_ACTIVE; i++) busy += !slot_free(&g->note[i], g->now);
    if (busy > g->peak) g->peak = (uint16_t)busy;
  }
}

/* ------------------------------------------------------------------------------------------------ events */
static void event(fnf_game_t *g, const fnf_event_t *e) {
  int32_t t = e->t;
  switch (e->type) {
    case FNF_EV_SETCAMERABOP:   /* SetCameraBopSongEvent */
      g->bop_intensity = e->v, g->bop_rate = (uint16_t)e->d, g->bop_offset = e->b;
      break;
    case FNF_EV_PLAYANIMATION:
      if (e->a < 3) {
        fnf_actor_t *a = &g->actor[e->a];
        if (a->chr != 255) {
          int k = find_anim(a, e->b);
          if (k != 255) {
            play(a, k, e->c, e->c, t);
          }
        }
      }
      break;
    case FNF_EV_SCROLLSPEED: {
      int32_t to = e->a ? e->v : (int32_t)((int64_t)fnf_chart[g->chart].speed * e->v / 1000);
      int32_t dur = (int32_t)((int64_t)e->d * fnf_step_len(g, t) / 1000);
      g->speed_from = g->speed, g->speed_to = to, g->speed_t0 = t, g->speed_t1 = t + dur;
      g->speed_ease = e->b;
      if ((e->b & 15) == 1 || dur <= 0) g->speed = to, g->speed_t1 = t;   /* INSTANT */
      break;
    }
    case FNF_EV_SETHEALTHICON:
      if (e->a < 2) g->icon[e->a] = e->b, g->icon_flip[e->a] = (uint8_t)(e->c ^ (e->a == 0));
      break;
    case FNF_EV_SSERAFIMSING: {   /* (sserafim.hxc: yunjin, kazuha, chaewon, eunchae, sakura, gf) */
      static const uint8_t who[6] = {3, 1, 4, 5, 0, 2};
      for (int i = 0; i < 6; i++) g->actor[who[i]].side = (uint8_t)(e->a >> i & 1 ? 0 : 1);
      break;
    }
    case FNF_EV_SSERAFIMSHOW: {
      static const uint8_t who[5] = {3, 1, 4, 5, 0};
      for (int i = 0; i < 5; i++) g->actor[who[i]].visible = (uint8_t)(e->a >> i & 1);
      break;
    }
    case FNF_EV_SSERAFIMDARK: {
      int32_t now_dark = g->dark_to;   /* (where it was heading: close enough as a start) */
      g->dark_from = now_dark, g->dark_to = e->v, g->dark_t0 = t, g->dark_t1 = t + e->d * 1000;
      break;
    }
    default: break;
  }
}

/* ------------------------------------------------------------------------------------------------ a step */
static void step_hit(fnf_game_t *g, int32_t k, int32_t t) {
  stop_singing(g, t);
  /* the camera and the HUD bop (PlayState.stepHit) */
  if (g->bop_rate && (k + g->bop_offset) % g->bop_rate == 0) {
    /* what's left of the HUD's last bop: lerp(1, zoom, 0.95^(dt x 60)) each frame; no bop past 1.35 */
    float left = g->bop_at ? g->bop_hud * powf(0.95f, (float)(t - g->bop_at) * 60e-6f) : 0;
    if (left < 350000) {
      g->bop_at = t;
      g->bop_cam = g->bop_intensity * 15;                  /* (Constants.DEFAULT_BOP_INTENSITY - 1) x intensity */
      g->bop_hud = (int32_t)left + g->bop_intensity * 30;  /* twice that, added to the HUD's zoom */
    }
  }
  if (k % 4 == 0) g->icon_at = t;               /* HealthIcon.onStepHit: every beat */
  for (int i = 0; i < FNF_ACTORS; i++) {
    fnf_actor_t *a = &g->actor[i];
    if (a->chr == 255) continue;
    int every = fnf_char[a->chr].dance;
    if (every && k % every == 0) dance(a, false, t);
  }
}

/* ------------------------------------------------------------------------------------------------ time passing */
static void bonus(fnf_game_t *g, int32_t from, int32_t to) {
  /* holds held: +250 points and +6% of the bar a second (Constants.SCORE_HOLD_BONUS_PER_SECOND, ...) */
  for (int i = 0; i < FNF_ACTIVE; i++) {
    fnf_note_t *n = &g->note[i];
    if (!holding(n) || n->lane >= 4 || !scoreable(n)) continue;
    int32_t a = from > n->hit ? from : n->hit, b = to < n->t + n->len ? to : n->t + n->len;
    if (b <= a) continue;
    int32_t d = b - a;
    /* (the game adds these as fractions, each frame: kept whole here, the leftovers carried on) */
    int64_t sc = (int64_t)d + g->score_rest;
    g->score += sc / 4;
    g->score_rest = (int32_t)(sc % 4);
    int64_t h = (int64_t)d * 12 + g->bonus_rest;
    g->bonus_rest = (int32_t)(h % 100);
    health(g, (int32_t)(h / 100));
  }
}

/* the earliest thing to happen after the clock's now: a step, an event or a note's deadline */
static int32_t next_thing(fnf_game_t *g) {
  int32_t t = step_time(g, g->step + 1);
  const fnf_var_t *v = &fnf_var[g->var];
  if (g->event < v->event + v->nevent && fnf_event[g->event].t < t) t = fnf_event[g->event].t;
  for (int i = 0; i < FNF_ACTIVE; i++) {
    const fnf_note_t *n = &g->note[i];
    int32_t d = INT32_MAX;
    if (n->state == NOTE_WAIT) d = n->lane >= 4 ? n->t : n->t + FNF_WINDOW + g->offset + 1;
    else if (holding(n)) {
      d = n->t + n->len;
      if (n->lane < 4 && !(g->held >> n->lane & 1) && n->t + 1 < d) d = n->t + 1 > g->now ? n->t + 1 : g->now;
    }
    if (d < t) t = d;
  }
  if (g->length < t) t = g->length;
  return t;
}

static void at(fnf_game_t *g, int32_t t) {
  bonus(g, g->now, t);
  g->now = t;
}

static void settle(fnf_game_t *g) {
  int32_t t = g->now;
  const fnf_var_t *v = &fnf_var[g->var];
  while (g->event < v->event + v->nevent && fnf_event[g->event].t <= t) event(g, &fnf_event[g->event++]);
  for (int32_t k = (int32_t)floor_div(fnf_steps(g, t), 1 << 16); g->step < k;) {
    g->step++;
    step_hit(g, g->step, step_time(g, g->step));
  }
  for (int i = 0; i < FNF_ACTIVE; i++) {
    fnf_note_t *n = &g->note[i];
    if (n->state == NOTE_WAIT) {
      if (n->lane >= 4) {
        if (t >= n->t) {   /* the opponent hits its notes on time */
          actors_on(g, 1, n->lane & 3, n->kind, false, t);
          g->strum[n->lane] = STRUM_CONFIRM, g->strum_at[n->lane] = t;
          n->hit = t;
          n->state = NOTE_GONE;
          if (n->len > 0) n->hold = HOLD_ON;
        }
      } else if (t - g->offset > n->t + FNF_WINDOW) {
        miss(g, n, t);
      }
    } else if (holding(n)) {
      int32_t end = n->t + n->len;
      if (n->lane >= 4) {
        for (int k = 0; k < FNF_ACTORS; k++)   /* the opponent sings on through its holds */
          if (g->actor[k].side == 1 && g->actor[k].singing) g->actor[k].hold = t < end ? t : end;
        if (t >= end) n->hold = HOLD_DONE;
      } else if (t >= end) {
        n->hold = HOLD_DONE;
      } else if (t > n->t && !(g->held >> n->lane & 1)) {
        drop(g, n, t);
      }
    }
  }
  if (g->health <= 0 && g->over == FNF_PLAYING) g->over = FNF_DIED, g->over_at = t;
  if (t >= g->length && g->over == FNF_PLAYING) g->over = FNF_WON, g->over_at = t;
}

static void visuals(fnf_game_t *g, int32_t t) {
  /* the health bar catches up: lerp(shown, health, 0.15) each frame at 60 a second */
  if (t > g->shown_at) {
    float k = 1 - powf(0.85f, (float)(t - g->shown_at) * 60e-6f);
    g->health_shown += (int32_t)((g->health - g->health_shown) * k);
    g->shown_at = t;
  }
  /* the scroll speed's change */
  if (g->speed_t1 > g->speed_t0 && t < g->speed_t1) {
    int64_t x = (int64_t)(t - g->speed_t0) * 1000 / (g->speed_t1 - g->speed_t0);
    if (x < 0) x = 0;
    g->speed = g->speed_from + (int32_t)((g->speed_to - g->speed_from) * x / 1000);   /* (linear; drawn eased) */
  } else {
    g->speed = g->speed_to;
  }
  stop_singing(g, t);
  for (int i = 0; i < FNF_ACTORS; i++)
    if (g->actor[i].locked && finished(&g->actor[i], t)) g->actor[i].locked = 0;
  /* the opponent's arrows light up 0.15 s (after the strum's confirm), the player's go back after theirs if held */
  for (int l = 0; l < 8; l++) {
    if (g->strum[l] != STRUM_CONFIRM) continue;
    bool on = false;
    for (int i = 0; i < FNF_ACTIVE; i++)
      if (g->note[i].lane == l && holding(&g->note[i])) on = true;
    if (on) continue;
    int32_t limit = l >= 4 ? 150000 : 167000 + 150000;   /* (confirm: 4 frames at 24 fps) */
    if (t - g->strum_at[l] >= limit) g->strum[l] = l < 4 && (g->held >> l & 1) ? STRUM_PRESS : STRUM_STATIC;
  }
}

void fnf_advance(fnf_game_t *g, int32_t t) {
  if (t < g->now) return;
  while (g->over == FNF_PLAYING) {
    bring_notes(g);
    int32_t n = next_thing(g);
    if (n > t) break;
    if (n < g->now) n = g->now;
    at(g, n);
    settle(g);
    if (n == t) break;
  }
  if (g->over == FNF_PLAYING && g->now < t) {
    at(g, t);
    settle(g);
  } else if (g->now < t) {
    g->now = t;
  }
  bring_notes(g);
  visuals(g, g->now);
}

/* ------------------------------------------------------------------------------------------------ keys */
void fnf_press(fnf_game_t *g, int lane, int32_t t) {
  if (lane < 0 || lane > 3 || (g->held >> lane & 1)) return;
  fnf_advance(g, t);
  if (g->over != FNF_PLAYING) return;
  g->held |= (uint8_t)(1 << lane);
  for (int i = 0; i < FNF_ACTORS; i++)
    if (g->actor[i].side == 0 && g->actor[i].singing) g->actor[i].hold = t;   /* (justPressedNote) */
  int32_t judged = t - g->offset;
  fnf_note_t *best = 0;
  for (int i = 0; i < FNF_ACTIVE; i++) {
    fnf_note_t *n = &g->note[i];
    if (n->state != NOTE_WAIT || n->lane != lane) continue;
    int32_t d = judged - n->t;
    if (d < -FNF_WINDOW || d > FNF_WINDOW) continue;
    if (!best || n->t < best->t) best = n;
  }
  if (best) {
    hit(g, best, judged);
  } else {   /* a ghost miss (no ghost tapping in the game: PlayState.ghostNoteMiss) */
    health(g, H_MISS);
    g->score -= GHOST_POINTS * 1000;
    actors_on(g, 0, lane, 0, true, t);
    g->strum[lane] = STRUM_PRESS, g->strum_at[lane] = t;
  }
  settle(g);
  visuals(g, g->now);
}

void fnf_release(fnf_game_t *g, int lane, int32_t t) {
  if (lane < 0 || lane > 3 || !(g->held >> lane & 1)) return;
  fnf_advance(g, t);
  g->held &= (uint8_t)~(1 << lane);
  g->strum[lane] = STRUM_STATIC, g->strum_at[lane] = t;
  if (g->over == FNF_PLAYING) settle(g);
  visuals(g, g->now);
}

/* ------------------------------------------------------------------------------------------------ the start */
static bool check(fnf_game_t *g, const fnf_chart_t *c) {
  /* every note readable, in order, as many as the chart says */
  unsigned count = 0;
  int32_t t = 0;
  g->pos = 0;
  while (g->pos < g->end) {
    fnf_note_t n;
    g->next = t;
    if (!next_note(g, &n)) return false;
    t = n.t;
    count++;
  }
  g->pos = 0, g->next = 0;
  return count == c->notes;
}

bool fnf_start(fnf_game_t *g, unsigned chart, uint8_t *buf, int32_t offset_ms, bool story) {
  memset(g, 0, sizeof *g);
  if (chart >= FNF_CHARTS) return false;
  const fnf_chart_t *c = &fnf_chart[chart];
  const fnf_var_t *v = &fnf_var[c->var];
  const fnf_stage_t *st = &fnf_stage[v->stage];
  if (c->raw > FNF_CHART_RAW_MAX ||
      fnf_inflate(fnf_blob + c->at, c->size, buf, FNF_CHART_RAW_MAX) != (int32_t)c->raw)
    return false;
  g->chart = (uint16_t)chart, g->var = (uint8_t)c->var, g->style = v->style, g->story = story;
  g->notes = buf, g->pos = 0, g->end = c->raw;
  if (!check(g, c)) return false;
  for (int i = 0; i < FNF_ACTIVE; i++) g->note[i].state = NOTE_GONE;
  g->offset = offset_ms * 1000;
  g->total = c->total;
  g->health = g->health_shown = FNF_HEALTH_MAX / 2;
  g->length = (int32_t)(v->length * 1000);
  g->tempo = v->tempo, g->event = v->event;
  g->speed = g->speed_from = g->speed_to = c->speed;
  g->bop_intensity = 1000, g->bop_rate = 16, g->bop_offset = 0;
  g->rng = 0x9E3779B9u ^ chart;
  for (int i = 0; i < FNF_ACTORS; i++) g->actor[i].chr = 255, g->actor[i].anim = 255;
  for (int r = 0; r < 3; r++) {
    fnf_actor_t *a = &g->actor[r];
    a->chr = v->chr[r], a->side = (uint8_t)r, a->visible = 1;
    int place = r == ROLE_OPPONENT && (v->flags & FNF_VF_OPPONENT_AT_GF) ? ROLE_GF : r;
    a->x = st->feet[place][0], a->y = st->feet[place][1];
  }
  for (int w = 0; w < 2; w++) {
    unsigned c = v->chr[w];
    g->icon[w] = c != 255 ? fnf_char[c].icon : 0;
    g->icon_flip[w] = (uint8_t)((c != 255 && (fnf_char[c].flags & FNF_CHAR_ICON_FLIP) ? 1 : 0) ^ (w == 0));
  }
  if (v->extra) {   /* LE SSERAFIM: six of them, who sings and who's seen set by the song's events */
    static const uint8_t extra[3] = {FNF_CHR_SSERAFIM_YUNJIN, FNF_CHR_SSERAFIM_CHAEWON, FNF_CHR_SSERAFIM_EUNCHAE};
    for (int i = 0; i < 3; i++) {
      fnf_actor_t *a = &g->actor[3 + i];
      a->chr = extra[i], a->side = 1, a->x = st->extra[i][0], a->y = st->extra[i][1];
    }
    /* baseVisible: yunjin only (Girlfriend's stays); baseSinging: none sing yours */
    g->actor[3].visible = 1, g->actor[1].visible = 0, g->actor[0].visible = 0, g->actor[2].visible = 1;
    for (int i = 0; i < FNF_ACTORS; i++) g->actor[i].side = 1;
    g->icon_hidden = 1;
  }
  g->now = g->shown_at = fnf_start_time(g);
  g->step = (int32_t)floor_div(fnf_steps(g, g->now), 1 << 16);
  g->next = 0;
  for (int i = 0; i < FNF_ACTORS; i++) dance(&g->actor[i], true, g->now);
  bring_notes(g);
  return true;
}

/* ------------------------------------------------------------------------------------------------ for the screen */
unsigned fnf_actor_sprite(const fnf_game_t *g, int i) {
  const fnf_actor_t *a = &g->actor[i];
  const fnf_anim_t *an = anim_of(a);
  if (!an) return FNF_NONE;
  int64_t f = (int64_t)(g->now - a->since) * an->fps / 1000000;
  if (f < 0) f = 0;
  if (an->flags & FNF_AF_LOOP) f %= an->len ? an->len : 1;
  int k = 0;
  for (int j = 1; j < an->n; j++)
    if (an->at[j] <= f) k = j;
  return an->sprite + (unsigned)k;
}

int fnf_countdown(const fnf_game_t *g, int32_t *age) {
  int32_t beat = (int32_t)floor_div(fnf_steps(g, g->now), 1 << 18);
  if (beat < -3 || beat > -1) return 0;
  *age = g->now - step_time(g, beat * 4);
  return beat + 4;
}
