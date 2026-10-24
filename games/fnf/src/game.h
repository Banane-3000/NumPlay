/* A song being played: the chart's notes against your presses, scored as the game scores them (PBOT1: the
 * points follow how close each press is, the judgement is Sick, Good, Bad or Shit), the health, the combo, the
 * holds, the misses and the characters singing and dancing to the beat. Times are microseconds of the song (0: its
 * first beat; the countdown is before). Everything is whole numbers, so a song plays the same everywhere. */
#ifndef FNF_GAME_H
#define FNF_GAME_H
#include <stdbool.h>
#include "data.h"

#define FNF_ACTIVE 128            /* notes on their way at once */
#define FNF_WINDOW 160000         /* the hit window, each side (us) */
#define FNF_HEALTH_MAX 2000000    /* health, in millionths */
#define FNF_ACTORS 6
#define FNF_POPS 12

enum { J_SICK, J_GOOD, J_BAD, J_SHIT, J_MISS };
/* a note's head: on its way, gone (hit, or the place is free), hit late or early (Bad, Shit: it scrolls on, faded),
 * missed (it scrolls on); its hold: none (or not reached yet), held, done, let go of (hidden), missed (scrolls on) */
enum { NOTE_WAIT, NOTE_GONE, NOTE_FADED, NOTE_MISSED };
enum { HOLD_NONE, HOLD_ON, HOLD_DONE, HOLD_DROPPED, HOLD_MISSED };
enum { FNF_PLAYING, FNF_WON, FNF_DIED };
enum { ROLE_PLAYER, ROLE_OPPONENT, ROLE_GF };
enum { STRUM_STATIC, STRUM_PRESS, STRUM_CONFIRM };

typedef struct {
  int32_t t, len;                 /* when, and a hold's length */
  int32_t hit;                    /* when it was hit */
  uint8_t lane, kind, state, hold;
} fnf_note_t;

typedef struct {
  uint8_t chr;                    /* fnf_char, 255: nobody */
  uint8_t side;                   /* sings the player's notes (0), the opponent's (1), or neither (2) */
  uint8_t visible, danced;        /* danced: the last dance was danceLeft (next: danceRight) */
  uint8_t anim;                   /* its own animation number, 255: none yet */
  uint8_t locked;                 /* a special animation nothing else may cut until it ends */
  uint8_t singing;                /* 1: a sing animation, 2: a miss one */
  int32_t since;                  /* when the animation started */
  int32_t hold;                   /* the hold timer's start (it sings on until singTime after it) */
  int16_t x, y;                   /* its feet on the screen */
} fnf_actor_t;

typedef struct {
  uint8_t what;                   /* 0-3 a judgement, 10-19 a digit, 255 none */
  int32_t at;                     /* when it popped */
  int16_t x, y, vx, vy, ay;       /* pixels, pixels a second, pixels a second squared */
  int32_t fade;                   /* when it starts fading (0.2 s) */
} fnf_pop_t;

typedef struct {
  /* what's played */
  uint16_t chart;
  uint8_t var, style;
  const uint8_t *notes;           /* the chart, unpacked */
  uint32_t pos, end;
  int32_t next;                   /* the next note's time, not on its way yet (INT32_MAX: none left) */
  fnf_note_t note[FNF_ACTIVE];
  uint16_t active, peak;
  /* time */
  int32_t now, length;
  int32_t step;                   /* the last step reached (whole steps, can be negative) */
  uint16_t tempo, event;
  int32_t offset;                 /* the note offset: presses count this much earlier (us) */
  /* the score */
  int64_t score;                  /* thousandths of a point */
  int32_t health;
  int32_t bonus_rest, score_rest; /* (the hold bonus's leftover fractions) */
  uint16_t combo, max_combo, tally[5], hits, total;
  uint8_t held;                   /* the lanes held down */
  uint8_t over;                   /* FNF_PLAYING, FNF_WON, FNF_DIED */
  int32_t over_at;
  /* what's seen */
  fnf_actor_t actor[FNF_ACTORS];
  int32_t bop_at;                 /* the last camera bop */
  int32_t bop_cam, bop_hud;       /* its strength: the camera's zoom - 1 and the HUD's, millionths */
  int32_t bop_intensity;          /* SetCameraBop's, thousandths */
  uint16_t bop_rate;              /* a bop every bop_rate steps (0: never), from bop_offset */
  uint8_t bop_offset;
  int32_t icon_at;                /* the icons' last bop */
  uint8_t icon[2];                /* the player's and the opponent's health icons */
  uint8_t icon_flip[2];           /* (facing the other way: the player's faces left, HealthIcon) */
  uint8_t icon_hidden;            /* (LE SSERAFIM hides the opponent's) */
  int32_t health_shown;           /* the health bar, catching up with the health */
  int32_t shown_at;
  fnf_pop_t pop[FNF_POPS];
  uint8_t popn;
  uint32_t rng;
  int32_t splash_at[4];
  uint8_t strum[8];               /* STRUM_..., each lane */
  int32_t strum_at[8];
  int32_t speed, speed_from, speed_to, speed_t0, speed_t1;   /* scroll speed x 1000, and its change */
  uint8_t speed_ease;
  int32_t dark_from, dark_to, dark_t0, dark_t1;              /* LE SSERAFIM's dimming, thousandths */
  uint8_t story;                  /* (no arrows fading in, in story mode) */
} fnf_game_t;

/* The chart's notes are unpacked into buf (FNF_CHART_RAW_MAX bytes). False if the chart is damaged. */
bool fnf_start(fnf_game_t *g, unsigned chart, uint8_t *buf, int32_t offset_ms, bool story);
void fnf_advance(fnf_game_t *g, int32_t t);              /* to time t (never back) */
void fnf_press(fnf_game_t *g, int lane, int32_t t);      /* a lane's key down at time t */
void fnf_release(fnf_game_t *g, int lane, int32_t t);
int32_t fnf_start_time(const fnf_game_t *g);             /* the countdown's start */
/* the song's beat at time t (16.16 steps: 4 a beat), and a step's length (us) */
int32_t fnf_steps(const fnf_game_t *g, int32_t t);
int32_t fnf_step_len(const fnf_game_t *g, int32_t t);

/* the rules, on their own */
int fnf_judge(int32_t ms);                 /* J_... for a press ms off (whole ms, signed) */
int fnf_points(int32_t ms);
int32_t fnf_completion(const fnf_game_t *g);   /* (sick + good - missed) / total, ten-thousandths, 0-10000 */
int fnf_rank(const fnf_game_t *g);             /* 0 LOSS, 1 GOOD, 2 GREAT, 3 EXCELLENT, 4 PERFECT, 5 PERFECT GOLD */
int32_t fnf_score(const fnf_game_t *g);        /* whole points, as the game shows and keeps them */

/* a character's frame now: its sprite (FNF_NONE: none) */
unsigned fnf_actor_sprite(const fnf_game_t *g, int i);
/* the combo's sound-free cues: the countdown picture showing (0 none, 1 ready, 2 set, 3 go) and its age (us) */
int fnf_countdown(const fnf_game_t *g, int32_t *age);
#endif
