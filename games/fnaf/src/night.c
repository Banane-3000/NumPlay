/* A night of Five Nights at Freddy's, 60 steps a second, as the game plays it.
 *
 * Every few seconds each animatronic has a movement opportunity (Freddy every 3.02, Bonnie 4.97, Chica 4.98,
 * Foxy 5.01): a number from 1 to 20, and it moves if its AI level (0 to 20, by night, rising at 2, 3 and 4 AM)
 * is that number or more. Bonnie and Chica wander their sides of the building to the doors; at a closed door
 * they go back, at an open one they come in, and get you when the monitor next comes down. Freddy goes his
 * one way to the right door, only while the monitor is down (but for his last step, which he takes while you
 * watch another camera), and waits a while after each chance, longer when you watch him. Foxy comes out of
 * Pirate Cove when the cameras stay down, then runs for the left door. The power drains a point (of 999)
 * a second for each bar of usage, and a point every few seconds more from the second night on; when it is
 * gone, the doors open, the lights die, and Freddy comes. */
#include <string.h>
#include "game.h"
#include "plat.h"

#define N g.n

static const int8_t AI[6][4] = {   /* Freddy, Bonnie, Chica, Foxy at 12 AM, nights 1 to 6 */
  {0, 0, 0, 0}, {0, 3, 1, 1}, {1, 0, 5, 2}, {1, 2, 4, 6}, {3, 5, 7, 5}, {4, 10, 12, 16}};
static const float OPPORTUNITY[4] = {3.02f, 4.97f, 4.98f, 5.01f};
static const float PENALTY[8] = {0, 0, 6, 5, 4, 3, 3, 3};   /* seconds between the extra points, by night */
static const uint8_t BONNIE_GOES[CAM_COUNT][2] = {
  [CAM_1A] = {CAM_5, CAM_1B}, [CAM_1B] = {CAM_5, CAM_2A}, [CAM_5] = {CAM_1B, CAM_2A},
  [CAM_2A] = {CAM_3, CAM_2B}, [CAM_3] = {AT_DOOR, CAM_2A}, [CAM_2B] = {CAM_3, AT_DOOR}};
static const uint8_t CHICA_GOES[CAM_COUNT][2] = {
  [CAM_1A] = {CAM_1B, CAM_1B}, [CAM_1B] = {CAM_7, CAM_6}, [CAM_7] = {CAM_6, CAM_4A},
  [CAM_6] = {CAM_7, CAM_4A}, [CAM_4A] = {CAM_1B, CAM_4B}, [CAM_4B] = {CAM_4A, AT_DOOR}};
static const uint8_t FREDDY_GOES[CAM_COUNT] = {
  [CAM_1A] = CAM_1B, [CAM_1B] = CAM_7, [CAM_7] = CAM_6, [CAM_6] = CAM_4A, [CAM_4A] = CAM_4B, [CAM_4B] = AT_DOOR};
/* the cameras around each on the map: left, right, up, down */
static const int8_t NEXT_CAM[CAM_COUNT][4] = {
  [CAM_1A] = {-1, -1, -1, CAM_1B}, [CAM_1B] = {CAM_5, CAM_7, CAM_1A, CAM_1C}, [CAM_1C] = {CAM_5, CAM_7, CAM_1B, CAM_3},
  [CAM_2A] = {CAM_3, CAM_4A, CAM_1C, CAM_2B}, [CAM_2B] = {CAM_3, CAM_4B, CAM_2A, -1}, [CAM_3] = {-1, CAM_2A, CAM_1C, -1},
  [CAM_4A] = {CAM_2A, CAM_6, -1, CAM_4B}, [CAM_4B] = {CAM_2B, -1, CAM_4A, -1}, [CAM_5] = {-1, CAM_1B, -1, CAM_1C},
  [CAM_6] = {CAM_4A, -1, CAM_7, -1}, [CAM_7] = {CAM_1B, -1, -1, CAM_6}};

static int roll(void) { return 1 + (int)(rnd() % 20); }
static bool watching(void) { return N.flip >= 1; }
static bool pressed(uint32_t k) { return (g.pressed & k) != 0; }
static bool held(uint32_t k) { return (g.held & k) != 0; }

void night_begin(int night) {
  memset(&N, 0, sizeof N);
  N.night = (uint8_t)night;
  if (night < 7) {
    memcpy(N.ai, AI[night - 1], sizeof N.ai);
    if (night == 4) N.ai[FREDDY] = (int8_t)(1 + (rnd() & 1));
  } else
    memcpy(N.ai, g.custom, sizeof N.ai);
  N.power = 999;
  N.usage = 1;
  N.pan = OFFICE_PAN / 2;
  N.want = -1;
  N.cam = CAM_1A;
  N.pos[FREDDY] = N.pos[BONNIE] = N.pos[CHICA] = CAM_1A;
  for (int k = 0; k < 4; k++) N.move_t[k] = OPPORTUNITY[k];
  N.foxy_run = -1;
}

/* ------------------------------------------------------------------ things that end the night */
static void scare(int who) {
  g.scene = SC_SCARE;
  g.scare = (uint8_t)who;
  g.scare_t = 0;
  N.monitor = 0;
  N.flip = 0;
}

static void six_am(void) {
  g.scene = SC_SIX;
  g.t = 0;
}

static void power_out(void) {
  N.power = 0;
  N.dark = 1;
  N.dark_t = N.dark_step = 0;
  N.door[LEFT] = N.door[RIGHT] = 0;
  N.light = 0;
  N.monitor = 0;
  N.want = -1;
  caption("*the power goes out*");
}

bool night_power_out(void) { return N.dark != 0; }

/* ------------------------------------------------------------------ the office */
static bool side_in_view(int side) { return side == LEFT ? N.pan <= 24 : N.pan >= OFFICE_PAN - 24; }

/* a door or light button: k 0 the door, 1 the light */
static void button(int side, int k) {
  if (N.pos[side == LEFT ? BONNIE : CHICA] == IN_OFFICE) {
    caption("*click* ...it won't work");
    return;
  }
  if (k == 0) {
    float a = N.door_anim[side];
    if (a <= 0 || a >= 1) N.door[side] ^= 1;
  } else
    N.light = N.light == 1 + side ? 0 : (uint8_t)(1 + side);
}

static void office_keys(void) {
  static const uint32_t keys[2][2] = {{K_DIGIT(7), K_DIGIT(1)}, {K_DIGIT(9), K_DIGIT(3)}};
  for (int side = 0; side < 2; side++)
    for (int k = 0; k < 2; k++)
      if (pressed(keys[side][k])) {
        if (side_in_view(side)) {
          button(side, k);
          N.want = -1;
        } else
          N.want = (int8_t)(side * 2 + k);
      }
  /* turning: held, or toward a button pressed */
  float target = 0;
  if (held(K_LEFT | K_DIGIT(4))) target = -380, N.want = -1;
  else if (held(K_RIGHT | K_DIGIT(6))) target = 380, N.want = -1;
  if (N.want >= 0) {
    int side = N.want >> 1;
    if (side_in_view(side)) {
      button(side, N.want & 1);
      N.want = -1;
    } else
      target = side == LEFT ? -720 : 720;
  }
  float dv = target - N.pan_v, step = 4200 * DT;
  N.pan_v += dv > step ? step : dv < -step ? -step : dv;
  N.pan += N.pan_v * DT;
  if (N.pan < 0) N.pan = 0, N.pan_v = 0;
  if (N.pan > OFFICE_PAN) N.pan = OFFICE_PAN, N.pan_v = 0;
  if (pressed(K_DIGIT(5)) && N.pan > 40 && N.pan < 190) caption("*honk*");   /* Freddy's nose, on the poster */
}

static void monitor_keys(void) {
  if (N.flip < 1) return;
  /* (Golden Freddy holds the camera on 2B) */
  if (N.cam == CAM_2B && (N.gold == 1 || N.gold == 2)) return;
  int to = -1;
  static const uint32_t arrows[4] = {K_LEFT, K_RIGHT, K_UP, K_DOWN};
  for (int d = 0; d < 4; d++)
    if (pressed(arrows[d]) && NEXT_CAM[N.cam][d] >= 0) to = NEXT_CAM[N.cam][d];
  if (pressed(K_DIGIT(1))) to = N.cam == CAM_1A ? CAM_1B : N.cam == CAM_1B ? CAM_1C : CAM_1A;
  if (pressed(K_DIGIT(2))) to = N.cam == CAM_2A ? CAM_2B : CAM_2A;
  if (pressed(K_DIGIT(3))) to = CAM_3;
  if (pressed(K_DIGIT(4))) to = N.cam == CAM_4A ? CAM_4B : CAM_4A;
  if (pressed(K_DIGIT(5))) to = CAM_5;
  if (pressed(K_DIGIT(6))) to = CAM_6;
  if (pressed(K_DIGIT(7))) to = CAM_7;
  if (to >= 0 && to != N.cam) {
    N.cam = (uint8_t)to;
    N.glitch = 0;
    g.blip = 0.12f;
  }
}

static void monitor_flip(void) {
  bool was = watching();
  if (N.dark) N.monitor = 0;
  N.flip += (N.monitor ? 1 : -1) * DT / 0.22f;
  if (N.flip < 0) N.flip = 0;
  if (N.flip > 1) N.flip = 1;
  if (!was && watching()) {   /* up */
    N.light = 0;
    g.blip = 0.12f;
    if (N.pos[BONNIE] == IN_OFFICE || N.pos[CHICA] == IN_OFFICE) N.force_t = 0;
  }
  if (was && !watching()) {   /* down: whoever came in is here */
    N.glitch = 0;
    if (N.pos[BONNIE] == IN_OFFICE) scare(SCARE_BONNIE);
    else if (N.pos[CHICA] == IN_OFFICE) scare(SCARE_CHICA);
    else if (N.gold == 2) {
      N.gold = 3;
      N.gold_t = 0;
      N.halluc = 1.66f;
    }
  }
}

static void keys(void) {
  if (pressed(K_DIGIT(0) | K_DEL) && N.night <= 5 && !N.call_muted && N.t < call_end(N.night)) N.call_muted = 1;
  if (N.dark) {   /* no power: only looking around */
    float target = held(K_LEFT | K_DIGIT(4)) ? -380 : held(K_RIGHT | K_DIGIT(6)) ? 380 : 0;
    N.pan += target * DT;
    N.pan = N.pan < 0 ? 0 : N.pan > OFFICE_PAN ? OFFICE_PAN : N.pan;
    return;
  }
  bool toggle = pressed(K_OK | K_DIGIT(8)) || (N.flip <= 0 && pressed(K_UP));
  if (toggle && (N.flip <= 0 || N.flip >= 1)) {
    N.monitor ^= 1;
    N.want = -1;
  }
  if (N.flip <= 0 && !N.monitor) office_keys();
  else monitor_keys();
}

/* ------------------------------------------------------------------ time and power */
static void time_passes(void) {
  N.t += DT;
  N.hour_t += DT;
  if (N.hour_t < (N.hour == 0 ? 90 : 89)) return;
  N.hour_t -= N.hour == 0 ? 90 : 89;
  N.hour++;
  if (N.hour >= 2 && N.hour <= 4) N.ai[BONNIE]++;
  if (N.hour == 3 || N.hour == 4) N.ai[CHICA]++, N.ai[FOXY]++;
  for (int k = 0; k < 4; k++)
    if (N.ai[k] > 20) N.ai[k] = 20;
  if (N.hour >= 6) six_am();
}

static void drain(int points) {
  N.power = (int16_t)(N.power > points ? N.power - points : 0);
}

static void power(void) {
  N.usage = (uint8_t)(1 + watching() + N.door[LEFT] + N.door[RIGHT] + (N.light != 0));
  N.drain_t += DT;
  if (N.drain_t >= 1) {
    N.drain_t -= 1;
    drain(N.usage);
  }
  if (PENALTY[N.night] > 0) {
    N.penalty_t += DT;
    if (N.penalty_t >= PENALTY[N.night]) {
      N.penalty_t -= PENALTY[N.night];
      drain(1);
    }
  }
  if (N.power <= 0) power_out();
}

/* ------------------------------------------------------------------ the animatronics */
/* what a move sounds like from the office (the game's footsteps, by where they go) */
static void footsteps(int to, int who) {
  static const uint8_t loud[2][CAM_COUNT] = {   /* Bonnie, Chica: 0 none, 1 faint, 2, 3 close */
    {[CAM_1B] = 1, [CAM_2A] = 2, [CAM_2B] = 3, [CAM_3] = 2},
    {[CAM_7] = 1, [CAM_6] = 1, [CAM_4A] = 2, [CAM_4B] = 2}};
  int l = to < CAM_COUNT ? loud[who - BONNIE][to] : 3;
  static const char *const say[4] = {NULL, "*faint footsteps*", "*footsteps*", "*footsteps, close by*"};
  if (say[l]) caption(say[l]);
}

static bool opportunity(int who) {
  N.move_t[who] -= DT;
  if (N.move_t[who] > 0) return false;
  N.move_t[who] += OPPORTUNITY[who];
  return roll() <= N.ai[who];
}

/* Bonnie and Chica: who 1 or 2 */
static void wanderer(int who) {
  int side = who == BONNIE ? LEFT : RIGHT;
  uint8_t *pos = &N.pos[who];
  if (*pos == IN_OFFICE) {
    if (watching()) {
      N.force_t += DT;
      if (N.force_t >= 30) N.monitor = 0;   /* (it comes down by itself) */
    }
    return;
  }
  if (!opportunity(who)) return;
  int from = *pos, to;
  if (from == AT_DOOR) {
    to = N.door[side] ? (who == BONNIE ? CAM_1B : CAM_4A) : IN_OFFICE;
    if (to == IN_OFFICE && watching()) caption("*something is breathing*");
  } else
    to = (who == BONNIE ? BONNIE_GOES : CHICA_GOES)[from][rnd() & 1];
  *pos = (uint8_t)to;
  if (to != from) {
    footsteps(to, who);
    if (watching() && N.cam != CAM_6 && (N.cam == from || N.cam == to)) N.glitch = 5;
  }
  if ((to == AT_DOOR) != (from == AT_DOOR)) {
    if (N.light == 1 + side) N.light = 0;
  }
}

static void freddy(void) {
  uint8_t *pos = &N.pos[FREDDY];
  if (*pos == IN_OFFICE) {   /* a try every second, 1 in 4 */
    N.freddy_try += DT;
    if (N.freddy_try >= 1) {
      N.freddy_try -= 1;
      if (rnd() % 4 == 0) scare(SCARE_FREDDY);
    }
    return;
  }
  if (N.freddy_state == 1) N.freddy_count += 1;
  N.move_t[FREDDY] -= DT;
  bool chance = N.move_t[FREDDY] <= 0;
  if (chance) N.move_t[FREDDY] += OPPORTUNITY[FREDDY];
  if (watching() && *pos != CAM_4B) {   /* he waits while the monitor is up, longer if he is watched */
    if (N.cam == *pos) N.freddy_count = 0;
    return;
  }
  if (N.freddy_state == 1 && N.freddy_count >= 1000 - 100 * N.ai[FREDDY]) {
    N.freddy_count = 0;
    N.freddy_state = 2;
  }
  if (chance && N.freddy_state == 0 && roll() <= N.ai[FREDDY]) {
    N.freddy_state = 1;
    return;
  }
  if (N.freddy_state != 2) return;
  if (*pos == CAM_4B && (!watching() || N.cam == CAM_4B)) {
    N.freddy_count = 0;
    return;
  }
  if (*pos == CAM_1A && (N.pos[BONNIE] == CAM_1A || N.pos[CHICA] == CAM_1A)) return;
  int to = FREDDY_GOES[*pos];
  if (to == AT_DOOR) to = N.door[RIGHT] ? CAM_4A : IN_OFFICE;
  N.freddy_state = 0;
  *pos = (uint8_t)to;
  static const char *const laugh[] = {"*a deep laugh, far away*", "*a deep laugh*", "*a deep laugh, close by*"};
  caption(laugh[to == CAM_1B || to == CAM_7 ? 0 : to == CAM_6 || to == CAM_4A ? 1 : 2]);
  if (to == CAM_6) caption("*a music box plays*");
}

static void foxy_reaches_door(void) {
  if (N.door[LEFT]) {
    caption("*BANG BANG BANG*");
    drain(10 + 50 * N.foxy_pounds);
    if (N.foxy_pounds < 40) N.foxy_pounds++;
    N.foxy = (uint8_t)(rnd() & 1);
    N.foxy_wait = 0;
    N.foxy_run = -1;
    N.move_t[FOXY] = OPPORTUNITY[FOXY];
    if (N.power <= 0) power_out();
  } else
    scare(SCARE_FOXY);
}

static void foxy(void) {
  if (watching()) N.foxy_stun = 50 + (float)(rnd() % 1000);   /* the cameras up: he can't come out */
  else if (N.foxy_stun > 0) N.foxy_stun -= 1;
  if (N.foxy == 3) {
    if (watching() && N.cam == CAM_2A && N.foxy_run < 0) {
      N.foxy_run = 0;
      caption("*running footsteps*");
    }
    if (N.foxy_run >= 0) {
      N.foxy_run += 1;
      if (N.foxy_run >= 100) foxy_reaches_door();
      return;
    }
    N.foxy_wait += 1;
    if (N.foxy_wait >= 1500) {
      caption("*running footsteps*");
      foxy_reaches_door();
    }
    return;
  }
  N.move_t[FOXY] -= DT;
  if (N.move_t[FOXY] > 0) return;
  N.move_t[FOXY] += OPPORTUNITY[FOXY];
  if (N.foxy_stun > 0 || roll() > N.ai[FOXY]) return;
  if (++N.foxy == 3) N.foxy_wait = 0, N.foxy_run = -1;
}

/* ------------------------------------------------------------------ the rest */
static void golden(void) {
  switch (N.gold) {
  case 0:
    N.gold_t += DT;
    if (N.gold_t >= 1) {
      N.gold_t -= 1;
      if (rnd() % 32768 == 0) N.gold = 1;
    }
    break;
  case 1:
    if (watching() && N.cam == CAM_2B && N.pos[BONNIE] != CAM_2B) {
      N.gold = 2;
      caption("*a little girl giggles*");
    }
    break;
  case 3:
    if (watching()) {
      N.gold = 4;
      break;
    }
    N.gold_t += DT;
    if (N.gold_t >= 5) {
      g.scene = SC_GOLDEN;
      g.t = 0;
    }
    break;
  }
}

static void sounds(void) {
  /* the kitchen (no picture, only its sound): Chica's pots and pans, Freddy's music box */
  N.kitchen_t += DT;
  if (N.kitchen_t >= 4) {
    N.kitchen_t -= 4;
    bool near = watching() && N.cam == CAM_6;
    if (N.pos[CHICA] == CAM_6 && rnd() % 10 < 5) caption(near ? "*pots and pans clatter*" : "*pots and pans, far away*");
    if (N.pos[FREDDY] == CAM_6 && near) caption("*a music box plays*");
  }
  N.circus_t += DT;
  if (N.circus_t >= 5) {
    N.circus_t -= 5;
    if (rnd() % 30 == 0) caption("*circus music, far away*");
  }
  N.hum_t += DT;
  if (N.hum_t >= 4) {
    N.hum_t -= 4;
    if (N.foxy == 0 && rnd() % 30 == 0) caption("*humming from Pirate Cove*");
    /* from the fourth night: voices on the corners' cameras */
    if (N.night >= 4 && watching() && ((N.cam == CAM_2B && N.pos[BONNIE] == CAM_2B) ||
                                       (N.cam == CAM_4B && N.pos[CHICA] == CAM_4B)))
      caption("*a garbled voice*");
  }
  if (N.halluc > 0) N.halluc -= DT;
  else if ((int)(N.t * 60) % 60 == 0 && rnd() % 1000 == 0) N.halluc = 1.66f;
}

/* no power: Freddy at the left door, his song, then the dark */
static void dark(void) {
  N.door_anim[LEFT] -= DT / 0.3f;
  N.door_anim[RIGHT] -= DT / 0.3f;
  for (int s = 0; s < 2; s++)
    if (N.door_anim[s] < 0) N.door_anim[s] = 0;
  N.flip -= DT / 0.22f;
  if (N.flip < 0) N.flip = 0;
  N.dark_t += DT;
  N.dark_step += DT;
  switch (N.dark) {
  case 1:
    if ((N.dark_step >= 5 && (N.dark_step -= 5, rnd() % 5 == 0)) || N.dark_t >= 20) {
      N.dark = 2;
      N.dark_t = N.dark_step = 0;
      caption("*a music box: the Toreador March*");
    }
    break;
  case 2:
    if ((N.dark_step >= 5 && (N.dark_step -= 5, rnd() % 5 == 0)) || N.dark_t >= 20) {
      N.dark = 3;
      N.dark_t = N.dark_step = 0;
      caption("*silence*");
    }
    break;
  case 3:
    if ((N.dark_step >= 2 && (N.dark_step -= 2, rnd() & 1)) || N.dark_t >= 20) scare(SCARE_DARK);
    break;
  }
}

void night_step(void) {
  time_passes();
  if (g.scene != SC_NIGHT) return;
  keys();
  if (N.dark) {
    dark();
    return;
  }
  for (int s = 0; s < 2; s++) {
    N.door_anim[s] += (N.door[s] ? 1 : -1) * DT / 0.3f;
    N.door_anim[s] = N.door_anim[s] < 0 ? 0 : N.door_anim[s] > 1 ? 1 : N.door_anim[s];
  }
  monitor_flip();
  if (g.scene != SC_NIGHT) return;
  power();
  if (N.dark) return;
  if (N.glitch > 0) N.glitch -= DT;
  N.cam_t += DT;
  if (N.cam_t >= 14) N.cam_t -= 14;
  wanderer(BONNIE);
  wanderer(CHICA);
  freddy();
  if (g.scene == SC_NIGHT) foxy();
  if (g.scene == SC_NIGHT) golden();
  if (g.scene == SC_NIGHT) sounds();
}
