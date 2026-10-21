/* Five Nights at Freddy's: the game's state and its parts.
 *
 * game.c: the scenes (title, newspaper, the night's card, 6 AM, paychecks, game over, custom night), keys and
 * saves. night.c: a night (time, power, the doors and lights, the cameras, the animatronics) as the game plays
 * it, 60 steps a second. view.c: drawing the office, the cameras and what is around them. calls.c: the phone
 * calls, as subtitles. */
#ifndef FNAF_GAME_H
#define FNAF_GAME_H
#include <stdbool.h>
#include <stdint.h>

#define DT (1.0f / 60)       /* a step */
#define OFFICE_PAN 213       /* how far the office view turns (the office is 533 wide, the screen 320) */
#define CAM_PAN 213

enum { CAM_1A, CAM_1B, CAM_1C, CAM_2A, CAM_2B, CAM_3, CAM_4A, CAM_4B, CAM_5, CAM_6, CAM_7, CAM_COUNT };
enum { AT_DOOR = CAM_COUNT, IN_OFFICE };   /* where Freddy, Bonnie and Chica go past the cameras */
enum { FREDDY, BONNIE, CHICA, FOXY };
enum { LEFT, RIGHT };

/* scenes */
enum { SC_TITLE, SC_NEWS, SC_KEYS, SC_INTRO, SC_NIGHT, SC_SCARE, SC_SIX, SC_PAPER, SC_OVER, SC_CUSTOM, SC_GOLDEN };
/* jumpscares */
enum { SCARE_BONNIE, SCARE_CHICA, SCARE_FREDDY, SCARE_FOXY, SCARE_DARK };

/* A night, all of it: kept as it is when Home quits (save.c), so the next start goes on from there. */
typedef struct {
  uint8_t night;         /* 1 to 7 (7: the custom night) */
  int8_t ai[4];          /* Freddy, Bonnie, Chica, Foxy: 0 to 20 */
  uint8_t hour;          /* 0 (12 AM) to 6 */
  uint8_t usage;         /* 1 to 5 bars */
  int16_t power;         /* 999: 99.9 % */
  uint8_t door[2];       /* closed (or closing) */
  uint8_t light;         /* 0, 1 + LEFT or 1 + RIGHT */
  uint8_t monitor;       /* up, or going up */
  uint8_t cam;           /* the camera picked */
  int8_t want;           /* a door or light pressed while looking away: turning there first (-1 none) */
  uint8_t pos[3];        /* Freddy, Bonnie, Chica: a CAM_*, AT_DOOR or IN_OFFICE */
  uint8_t freddy_state;  /* 0 waits, 1 counts down, 2 goes at the next chance */
  uint8_t foxy;          /* 0 behind the curtain, 1 peeking, 2 out, 3 gone down the hall */
  uint8_t foxy_pounds;   /* times he hit the closed door */
  uint8_t gold;          /* Golden Freddy: 0 may come, 1 on 2B's poster, 2 seen there, 3 in the office, 4 gone */
  uint8_t call_muted;
  uint8_t dark;          /* the power is out: 1 dark, 2 Freddy at the door, 3 all black */
  uint8_t pad[4];
  float t;               /* seconds since 12 AM */
  float hour_t;
  float drain_t, penalty_t;
  float door_anim[2];    /* 0 open to 1 closed */
  float pan, pan_v;      /* the office view's left edge */
  float flip;            /* the monitor: 0 down to 1 up */
  float cam_t;           /* the camera's slow turn: 14 seconds there and back */
  float glitch;          /* seconds of disruption on the camera watched */
  float force_t;         /* someone came in while the monitor was up: it comes down after 30 seconds */
  float move_t[4];       /* until the next movement opportunity */
  float freddy_count;    /* Freddy's countdown, in frames */
  float freddy_try;      /* in the office: a try a second */
  float foxy_stun;       /* frames Foxy fails every opportunity (the monitor was up) */
  float foxy_wait;       /* frames out of the cove */
  float foxy_run;        /* frames since he was seen running, or -1 */
  float kitchen_t, circus_t, hum_t, gold_t, halluc, dark_t, dark_step;
} Night;

typedef struct {
  int scene;
  float t;               /* seconds in the scene */
  uint32_t held, pressed;
  bool running;
  /* the save: the night reached (1 to 5), the stars and whether the keys were shown */
  uint8_t reached, stars, keys_seen;
  uint8_t menu, menu_n;
  uint8_t paused, confirm, pause_item;
  uint8_t custom_sel, paper;
  uint8_t scare, after_keys;
  int8_t custom[4];      /* the custom night's levels */
  float scare_t;
  float blip;            /* the cameras' white flicker when switched */
  float repeat;          /* a held key repeats */
  const char *cap[4];    /* what is heard, as captions (the newest last) */
  float cap_t[4];
  Night n;
} Game;

extern Game g;

/* game.c */
void game_init(void);
void game_step(uint32_t keys);
void game_draw(void);
uint32_t rnd(void);
void start_night(int night);
void caption(const char *s);

/* night.c */
void night_begin(int night);
void night_step(void);

/* view.c */
void view_night(void);
void view_scare(void);
void view_captions(void);

/* calls.c: the phone call's line at t seconds into the night, or NULL; and when it ends */
const char *call_line(int night, float t);
float call_end(int night);

/* save.c */
void save_restore_copy(void);
void save_progress(void);
void save_load(void);
void save_resume(void);     /* the night as it is (Home) */
bool save_resume_load(void);
#endif
