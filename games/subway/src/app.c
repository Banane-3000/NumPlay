#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC optimize("Os")   /* screens over the game: smaller over faster */
#endif
/* The screens and the way between them (app.h). */
#include <string.h>
#include "actors.h"
#include "app.h"
#include "data.h"
#include "draw.h"
#include "game.h"
#include "icons.h"
#include "plat.h"
#include "save.h"
#include "scene.h"

enum { S_HELP, S_TITLE, S_RUN, S_PAUSE, S_SAVEME, S_RESULTS, S_SHOP, S_MISSIONS, S_QUIT };
bool app_running = true;
static int state, back_to;      /* the screen, and where Back goes from the shop, the missions or Quit */
static int sel;                 /* the button in focus */
static float clock, intro, saveme_t, state_t;
static bool new_best;
static int shop_tab, shop_sel, shop_top;
static char note[40];           /* the shop's word on the last try */
static float note_t;
static float cam_x, cam_y;      /* the run's camera, smoothed */
static bool drawn;              /* a screen that does not move is drawn once, then again when something changes */
static bool hit_train;          /* the crash was into a train's front */
static void runner_anim(float dt);
static Anim a_run, a_guard, a_dog, a_shop;
/* the game's run camera: 33 units up and 33 behind the runner, following 0.75 of its moves across */
#define CAM_Y (33 * U)
#define CAM_BACK (33 * U)
#define CAM_PITCH 0.34f
#define CAM_F 250.0f

/* colours of the game's screens */
#define NAVY 0x0B2A55
#define WHITE 0xFFFFFF
#define BLUE_T 0x2FA6F2
#define BLUE_B 0x1466C4
#define GOLD_T 0xFFE15A
#define GOLD_B 0xF5A300

static void fmt(char *out, uint32_t v, bool commas) {
  char t[16];
  int n = 0;
  do {
    if (commas && n % 4 == 3) t[n++] = ',';
    t[n++] = (char)('0' + v % 10), v /= 10;
  } while (v);
  for (int i = 0; i < n; i++) out[i] = t[n - 1 - i];
  out[n] = 0;
}
static void cat(char *out, const char *a) {
  while (*out) out++;
  while (*a) *out++ = *a++;
  *out = 0;
}

static void title_world(void) {
  world_reset(plat_random());
  world_title();
  memset(&pl, 0, sizeof pl);
  pl.lane = 1, pl.x = 0, pl.speed = 13;
}

/* the title's yard behind the screen */
static bool title_scene(void) {
  return state == S_TITLE || state == S_HELP || state == S_MISSIONS || (state == S_QUIT && back_to == S_TITLE);
}

static void go(int s) {
  drawn = false;
  state = s;
  state_t = 0;
  sel = 0;
}

void app_init(void) {
  save_load();
  scene_init();
  title_world();
  go(pg.help_seen ? S_TITLE : S_HELP);
}

static void start_run(void) {
  if (state != S_TITLE) title_world();
  run_start();
  intro = 0;
  cam_x = 0, cam_y = CAM_Y;
  go(S_RUN);
}

static void end_run(void) {
  new_best = run.score > pg.best && run.score > 0;
  run_bank();
  go(S_RESULTS);
}

/* ---------------------------------------------------------------- the shop's shelves */
enum { TAB_UPGRADES, TAB_ITEMS, TAB_CHARS, TAB_BOARDS, TABS };
static const char *const tab_names[TABS] = {"Upgrades", "Items", "Characters", "Boards"};
static const char *const upg_names[NUPG] = {"Jetpack", "Super Sneakers", "Coin Magnet", "2x Multiplier"};
static const uint8_t upg_icons[NUPG] = {ICON_JETPACK, ICON_SNEAKERS, ICON_MAGNET, ICON_DOUBLE};
static const char *const item_names[4] = {"Hoverboard", "Headstart", "Score Booster", "Mystery Box"};
static const char *const item_text[4] = {"A crash forgiven: OK in a run", "Fly ahead: 1 at the start",
                                         "+5 multiplier: 3 at the start", "Coins, keys and more inside"};
static const uint16_t item_price[4] = {300, 2000, 3000, 500};
static const uint8_t item_icons[4] = {ICON_BOARD, ICON_HEADSTART, ICON_BOOSTER, ICON_MYSTERY};

static int shop_rows(void) { return shop_tab == TAB_UPGRADES ? NUPG : shop_tab == TAB_ITEMS ? 4 : shop_tab == TAB_CHARS ? NCHARS : NBOARDS; }

static void say(const char *s) {
  strncpy(note, s, sizeof note - 1);
  note[sizeof note - 1] = 0;
  note_t = 2;
}

static bool pay(uint32_t price) {
  if (pg.coins < price) {
    say("Not enough coins");
    return false;
  }
  pg.coins -= price;
  return true;
}

static void shop_buy(void) {
  int i = shop_sel;
  switch (shop_tab) {
    case TAB_UPGRADES: {
      int c = upgrade_cost(pg.upg[i]);
      if (!c) say("Already at the top");
      else if (pay((uint32_t)c)) pg.upg[i]++, say("Upgraded!");
      break;
    }
    case TAB_ITEMS:
      if (!pay(item_price[i])) break;
      if (i == 0) pg.boards++, say("+1 hoverboard");
      else if (i == 1) pg.headstarts++, say("+1 headstart");
      else if (i == 2) pg.boosters++, say("+1 score booster");
      else {
        pg.total[CNT_MYSTERY]++;
        uint32_t r = plat_random() % 100;
        if (r < 55) {
          uint32_t c = 100 + plat_random() % 9 * 100;
          pg.coins += c;
          char s[40] = "+";
          fmt(s + 1, c, true);
          cat(s, " coins");
          say(s);
        } else if (r < 70) pg.keys++, say("A key!");
        else if (r < 85) pg.boards++, say("A hoverboard!");
        else if (r < 93) pg.headstarts++, say("A headstart!");
        else if (r < 99) pg.boosters++, say("A score booster!");
        else pg.coins += 20000, say("JACKPOT! 20,000 coins");
      }
      break;
    case TAB_CHARS:
      if (pg.chars & 1 << i) pg.character = (uint8_t)i, say("Selected");
      else if (pay(char_price[i])) pg.chars |= (uint8_t)(1 << i), pg.character = (uint8_t)i, say("Unlocked!");
      break;
    default:
      if (pg.ownboards & 1 << i) pg.board = (uint8_t)i, say("Selected");
      else if (pay(board_price[i])) pg.ownboards |= (uint8_t)(1 << i), pg.board = (uint8_t)i, say("Unlocked!");
      break;
  }
  save_write();
}

/* ---------------------------------------------------------------- a 60th of a second */
static void camera_follow(void) {
  float tx = pl.x * 0.75f, ty = CAM_Y + pl.ground * 0.85f + (pl.y - pl.ground) * 0.35f;
  if (pl.fly > 0) ty = CAM_Y + pl.y * 0.8f;
  /* under a roof */
  float roof = world_roof(pl.z - CAM_BACK) < world_roof(pl.z + 4) ? world_roof(pl.z - CAM_BACK) : world_roof(pl.z + 4);
  if (ty > roof - 0.4f) ty = roof - 0.4f;
  /* (the game's follow speeds: 15 across, 7.5 up and down) */
  cam_x += (tx - cam_x) * (1 - 1 / (1 + 15 * DT));
  cam_y += (ty - cam_y) * (1 - 1 / (1 + 7.5f * DT));
}

void app_step(uint32_t keys, uint32_t pressed) {
  if (pressed) drawn = false;
  clock += DT;
  state_t += DT;
  if (note_t > 0) note_t -= DT;
  if (pressed & K_HOME) {
    /* save and quit, from anywhere: a run's coins and progress are kept */
    if (state == S_RUN || state == S_PAUSE || state == S_SAVEME || (state == S_QUIT && back_to == S_PAUSE)) run_bank();
    else save_write();
    app_running = false;
    return;
  }
  switch (state) {
    case S_HELP:
      if (pressed & (K_OK | K_BACK)) {
        pg.help_seen = 1;
        save_write();
        go(S_TITLE);
      }
      break;
    case S_TITLE:
      /* OK plays; Down goes to the buttons below: the shop, the missions, how to play */
      if (sel == 0) {
        if (pressed & K_OK) start_run();
        else if (pressed & K_DOWN) sel = 1;
        else if (pressed & K_BACK) back_to = S_TITLE, go(S_QUIT), sel = 0;
      } else {
        if (pressed & K_LEFT && sel > 1) sel--;
        if (pressed & K_RIGHT && sel < 3) sel++;
        if (pressed & (K_UP | K_BACK)) sel = 0;
        if (pressed & K_OK) {
          int s = sel;
          back_to = S_TITLE;
          if (s == 1) go(S_SHOP), shop_sel = shop_top = 0;
          else if (s == 2) go(S_MISSIONS);
          else go(S_HELP);
        }
      }
      break;
    case S_RUN:
      if (intro < 1) intro += DT / 0.9f;
      if (pressed & K_BACK && !pl.dead) {
        go(S_PAUSE);
        break;
      }
      run_step(keys, pressed);
      if (pl.ev & EV_CRASH && pl.dead) {
        /* into a train's front? */
        hit_train = false;
        for (int i = 0; i < nobs; i++)
          if ((obs[i].type == O_TRAIN || obs[i].type == O_MOVING) && obs[i].lane == pl.lane && obs[i].z0 > pl.z - 0.5f && obs[i].z0 < pl.z + 1.5f)
            hit_train = true;
      }
      camera_follow();
      if (run_over()) {
        if (pg.keys + run.keys >= (uint32_t)run_save_cost()) {
          /* (keys picked up this run count too) */
          pg.keys += run.keys, run.keys = 0;
          go(S_SAVEME);
        } else end_run();
      }
      break;
    case S_PAUSE:
      if (pressed & (K_UP | K_DOWN)) sel ^= 1;
      if (pressed & K_BACK || (pressed & K_OK && sel == 0)) go(S_RUN);
      else if (pressed & K_OK) back_to = S_PAUSE, go(S_QUIT);
      break;
    case S_SAVEME:
      saveme_t += DT;
      if (pressed & K_OK) {
        run_revive();
        go(S_RUN);
      } else if (pressed & K_BACK || state_t > 5) end_run();
      break;
    case S_RESULTS:
      if (pressed & K_LEFT && sel > 0) sel--;
      if (pressed & K_RIGHT && sel < 2) sel++;
      if (pressed & K_BACK) title_world(), go(S_TITLE);
      else if (pressed & K_OK) {
        if (sel == 0) {
          title_world();
          start_run();
        } else if (sel == 1) back_to = S_RESULTS, go(S_SHOP), shop_sel = shop_top = 0;
        else title_world(), go(S_TITLE);
      }
      break;
    case S_SHOP:
      anim_step(&a_shop, DT);
      if (pressed & K_LEFT) shop_tab = (shop_tab + TABS - 1) % TABS, shop_sel = shop_top = 0;
      if (pressed & K_RIGHT) shop_tab = (shop_tab + 1) % TABS, shop_sel = shop_top = 0;
      if (pressed & K_UP && shop_sel > 0) shop_sel--;
      if (pressed & K_DOWN && shop_sel < shop_rows() - 1) shop_sel++;
      if (shop_sel < shop_top) shop_top = shop_sel;
      if (shop_sel > shop_top + 3) shop_top = shop_sel - 3;
      if (pressed & K_OK) shop_buy();
      if (pressed & K_BACK) {
        int b = back_to;
        go(b);
        if (b == S_RESULTS) sel = 1;
        if (b == S_TITLE) sel = 1;
      }
      break;
    case S_MISSIONS:
      if (pressed & (K_BACK | K_OK)) go(S_TITLE), sel = 2;
      break;
    case S_QUIT:
      if (pressed & (K_LEFT | K_RIGHT)) sel ^= 1;
      if (pressed & K_BACK || (pressed & K_OK && sel == 0)) {
        int b = back_to;
        go(b);
        if (b == S_PAUSE) sel = 1;
      } else if (pressed & K_OK) {
        if (back_to == S_TITLE) {
          save_write();
          app_running = false;
        } else end_run();
      }
      break;
  }
  if (title_scene()) pl.t += DT;
  if (state == S_RUN || title_scene()) runner_anim(DT);
}

/* ---------------------------------------------------------------- drawing: the 3D part */
static float lerp(float a, float b, float t) { return a + (b - a) * t; }

static bool clip_in(const Anim *an, const Clip *const *list, int n) {
  for (int i = 0; i < n; i++)
    if (an->a == list[i]) return true;
  return false;
}

/* the runner's clip, from what happened this step: the game's own moves */
static void runner_anim(float dt) {
  Anim *an = &a_run;
  uint8_t ev = pl.ev;
  float rate = 1;
  if (title_scene()) {
    anim_play(an, &clip_runner_idle, 0, 0.25f);
  } else if (pl.dead) {
    anim_play(an, hit_train ? &clip_runner_death_train : &clip_runner_death, 0, 0.06f);
  } else if (pl.fly > 0) {
    if (ev & EV_LEFT) anim_play(an, &clip_runner_fly_l, 0, 0.05f), an->ta = 0;
    else if (ev & EV_RIGHT) anim_play(an, &clip_runner_fly_r, 0, 0.05f), an->ta = 0;
    else if (an->a != &clip_runner_fly && (anim_done(an) || (an->a != &clip_runner_fly_l && an->a != &clip_runner_fly_r)))
      anim_play(an, &clip_runner_fly, 0, 0.2f);
  } else if (pl.hover > 0) {
    static const Clip *const once[] = {&clip_runner_board_jump, &clip_runner_board_land, &clip_runner_board_roll, &clip_runner_board_l,
                                       &clip_runner_board_r};
    if (ev & EV_JUMP) anim_play(an, &clip_runner_board_jump, 0, 0.05f);
    else if (ev & EV_ROLL) anim_play(an, &clip_runner_board_roll, 0, 0.05f);
    else if (ev & EV_LAND) anim_play(an, &clip_runner_board_land, 0, 0.05f);
    else if (ev & EV_LEFT && pl.state != ST_AIR) anim_play(an, &clip_runner_board_l, 0, 0.06f);
    else if (ev & EV_RIGHT && pl.state != ST_AIR) anim_play(an, &clip_runner_board_r, 0, 0.06f);
    if (!clip_in(an, once, 5) || anim_done(an)) {
      if (pl.roll > 0) anim_play(an, &clip_runner_board_roll, 0, 0.1f);
      else if (pl.state == ST_AIR) anim_play(an, &clip_runner_board_hang, 0, 0.12f);
      else anim_play(an, &clip_runner_board, 0, 0.12f);
    }
  } else {
    static const Clip *const once[] = {&clip_runner_jump, &clip_runner_land, &clip_runner_roll, &clip_runner_dodge_l, &clip_runner_dodge_r,
                                       &clip_runner_stumble_l, &clip_runner_stumble_r, &clip_runner_salto, &clip_runner_intro};
    bool super = pl.power[PW_SNEAKERS] > 0;
    if (ev & EV_STUMBLE) anim_play(an, pl.x < LANE_X(pl.lane) ? &clip_runner_stumble_r : &clip_runner_stumble_l, 0, 0.05f);
    else if (ev & EV_JUMP) anim_play(an, ev & EV_SUPER ? &clip_runner_salto : &clip_runner_jump, 0, 0.05f);
    else if (ev & EV_ROLL) {
      anim_play(an, &clip_runner_roll, 0, 0.05f);
      an->ta = 0;
    } else if (ev & EV_LAND && pl.roll <= 0) anim_play(an, &clip_runner_land, 0, 0.05f);
    else if (ev & EV_LEFT && pl.state != ST_AIR && pl.roll <= 0) anim_play(an, &clip_runner_dodge_l, 0, 0.06f);
    else if (ev & EV_RIGHT && pl.state != ST_AIR && pl.roll <= 0) anim_play(an, &clip_runner_dodge_r, 0, 0.06f);
    if (!clip_in(an, once, 9) || anim_done(an)) {
      if (pl.roll > 0) anim_play(an, &clip_runner_roll, 0.3f, 0.1f);
      else if (pl.state == ST_AIR) anim_play(an, &clip_runner_hang, 0, an->a == &clip_runner_jump ? 0.1f : 0.2f);
      else anim_play(an, super ? &clip_runner_super_run : &clip_runner_run, 0, 0.12f);
    }
    if (an->a == &clip_runner_run || an->a == &clip_runner_super_run)
      rate = 0.9f + 0.35f * (pl.speed - SPEED_MIN) / (SPEED_MAX - SPEED_MIN);
  }
  anim_step(an, dt * rate);
  /* the Inspector and his dog */
  if (pl.dead && pl.crash_t > 0.25f) {
    anim_play(&a_guard, &clip_inspector_catch, 0, 0.15f);
    anim_play(&a_dog, &clip_dog_catch, 0, 0.15f);
  } else {
    anim_play(&a_guard, &clip_inspector_run, 0, 0.15f);
    anim_play(&a_dog, &clip_dog_run, 0, 0.15f);
  }
  float grate = title_scene() ? 0 : 1;
  anim_step(&a_guard, dt * grate);
  anim_step(&a_dog, dt * grate);
}

static void draw_runner(void) {
  int who = pg.character;
  float lt = scene_light(pl.z);
  bool title = title_scene();
  float yaw = title ? 3.14159f : 0;
  if (!title && run.start_t < 0.35f && !pl.dead) yaw += 3.14159f * (1 - run.start_t / 0.35f);   /* turning round to run */
  V3 at = {pl.x, pl.y, pl.z};
  bool blink = pl.safe > 0.15f && ((int)(clock * 12) & 1);
  if (!title && pl.hover > 0 && !pl.dead && pl.fly <= 0) {
    board_draw(at, pg.board, yaw, clock, lt);
    at.y += 0.18f;
  }
  if (!blink) {
    actor_draw(who, &a_run, at, yaw, 0, lt);
    if (pl.power[PW_JETPACK] > 0 || pl.headstart > 0) jetpack_draw(clock, lt);
  }
  float g = pl.fly > 0 ? world_ground(pl.x, pl.z, 0) : pl.ground;
  shadow_draw(pl.x, pl.z, g, pl.y - g);
  /* the Inspector and his dog, while they are after the runner */
  if (!title && (pl.chase > 0 || pl.dead)) {
    float back = pl.dead ? 1.3f - (pl.crash_t > 1 ? 1 : pl.crash_t) * 0.5f : 2.4f + (pl.chase < 1 ? (1 - pl.chase) * 9 : 0);
    if (run.start_t < 1) back = 2.4f + (1 - run.start_t) * 3;
    float ix = pl.x + 0.55f, dx = pl.x - 0.75f;
    actor_draw(WHO_INSPECTOR, &a_guard, (V3){ix, pl.dead ? pl.ground : 0, pl.z - back}, 0, 0, lt);
    actor_draw(WHO_DOG, &a_dog, (V3){dx, pl.dead ? pl.ground : 0, pl.z - back + 0.7f}, 0, 0, lt);
  }
}

/* coins picked up: a star of light that grows and fades, carried along with the runner */
static void draw_sparks(void) {
  for (int i = 0; i < NSPARKS; i++) {
    const Spark *s = &sparks[i];
    if (s->t >= 0.3f) continue;
    float k = s->t / 0.3f, r = 0.25f + 0.55f * k, x = pl.x + (s->x - pl.x) * 0.4f, y = s->y, z = pl.z + 0.4f;
    int a = (int)(230 * (1 - k));
    V3 h[4] = {{x - r, y + 0.07f, z}, {x + r, y + 0.07f, z}, {x + r, y - 0.07f, z}, {x - r, y - 0.07f, z}};
    V3 v[4] = {{x - 0.07f, y + r, z}, {x + 0.07f, y + r, z}, {x + 0.07f, y - r, z}, {x - 0.07f, y - r, z}};
    r_poly(h, 4, 0xFFF6B0, P_ALPHA | P_NOHAZE | P_TWOSIDED, a);
    r_poly(v, 4, 0xFFF6B0, P_ALPHA | P_NOHAZE | P_TWOSIDED, a);
  }
}

static void draw_world(void) {
  r_begin();
  bool title = title_scene();
  /* the title: in front of the runner, a little to the side, looking back at the yard */
  float tx = 0.9f, ty = 1.3f, tz = -3.6f, tp = 0.02f, tf = 230;
  if (title) scene_camera(tx, ty, tz, tp, tf);
  else {
    /* the camera swings round from the title's view */
    float k = intro >= 1 ? 1 : intro * intro * (3 - 2 * intro), pitch = CAM_PITCH + (pl.fly > 0 ? 0.08f : 0);
    /* a shake when the runner crashes */
    float shake = 0;
    if (pl.dead && pl.crash_t < 0.35f) {
      float s, c;
      r_sincos(pl.crash_t * 70, &s, &c);
      shake = s * 0.15f * (1 - pl.crash_t / 0.35f);
    }
    scene_camera(lerp(tx, cam_x, k) + shake, lerp(ty, cam_y, k) + shake * 0.5f, lerp(tz, pl.z - CAM_BACK, k), lerp(tp, pitch, k),
                 lerp(tf, CAM_F, k));
  }
  draw_runner();
  scene_draw(clock);
  if (!title) draw_sparks();
}

/* ---------------------------------------------------------------- drawing: over the picture */
static char s_score[16], s_coins[16], s_mult[8], s_best[24], s_keys[16], s_total[16];

static void button(int x, int y, int w, int h, const char *text, bool focus) {
  if (y > r_y1 || y + h < r_y0) return;
  d_rrect(x + 1, y + 3, w, h, 9, C(0x061A36), 110);
  d_panel(x, y, w, h, 9, 2, NAVY, focus ? GOLD_T : BLUE_T, focus ? GOLD_B : BLUE_B);
  d_label(&font_mid, x + w / 2, y + h / 2 + 6, text, C(WHITE), C(NAVY), 2, 0);
}

static void panel(int x, int y, int w, int h) {
  if (y > r_y1 || y + h < r_y0) return;
  d_rrect(x + 2, y + 4, w, h, 12, C(0x061A36), 120);
  d_panel(x, y, w, h, 12, 3, NAVY, 0x3CB4FF, 0x1773D6);
}

static void counter(int rx, int y, int which, const char *text) {
  int w = d_text_width(&font_mid, text);
  icon(which, (float)(rx - w - 14), (float)y - 6, 18);
  d_label(&font_mid, rx, y, text, C(WHITE), C(NAVY), 2, 1);
}

/* the game's power-up timer: the picture on a blue tile, a bar of ten steps going down */
static void timer(int x, int y, int which, float f, uint32_t fill) {
  if (y > r_y1 || y + 24 < r_y0) return;
  d_rrect(x, y, 24, 24, 6, C(0xFFFFFF), 255);
  d_rrect(x + 1, y + 1, 22, 22, 5, C(0x2E8FE0), 255);
  icon(which, (float)x + 12, (float)y + 12, 20);
  d_rrect(x + 27, y + 7, 62, 11, 4, C(0xFFFFFF), 230);
  int n = (int)(f * 10 + 0.99f);
  for (int i = 0; i < 10; i++) d_fill(x + 29 + i * 6, y + 9, 5, 7, i < n ? C(fill) : C(0xC8CED6));
}

static void hud(void) {
  /* the pause button, top left (Back) */
  if (r_y0 < 34) {
    d_rrect(6, 6, 26, 26, 7, C(0xFFFFFF), 255);
    d_rrect(8, 8, 22, 22, 6, C(0x4AA8F0), 255);
    d_fill(14, 13, 4, 12, C(0xFFFFFF));
    d_fill(20, 13, 4, 12, C(0xFFFFFF));
  }
  /* top right, on dark panels: the multiplier in yellow and the score; the coins under them */
  char sc[16];
  int n = 0;
  while (s_score[n]) n++;
  int pad = n < 6 ? 6 - n : 0;
  for (int i = 0; i < pad; i++) sc[i] = '0';
  sc[pad] = 0;
  cat(sc, s_score);
  int sw = d_text_width(&font_big, sc), mw = d_text_width(&font_mid, s_mult);
  bool dbl = pl.power[PW_DOUBLE] > 0;
  if (r_y0 < 34) {
    d_rrect(314 - sw - mw - 22, 6, sw + mw + 18, 26, 6, C(0x14141E), 150);
    d_label(&font_mid, 314 - sw - 10, 26, s_mult, C(dbl ? 0xFF8AF0 : 0xFFD23F), C(0x2A2008), 1, 1);
    d_label(&font_big, 310, 28, sc, C(WHITE), C(0x14141E), 1, 1);
  }
  if (r_y0 < 62 && r_y1 > 34) {
    int cw = d_text_width(&font_mid, s_coins);
    d_rrect(314 - cw - 30, 36, cw + 30, 22, 6, C(0x14141E), 150);
    d_label(&font_mid, 292, 53, s_coins, C(WHITE), C(0x14141E), 1, 1);
    icon(ICON_COIN, 303, 47, 16);
  }
  /* power-ups running, at the bottom: the classic timers, side by side */
  static const uint8_t pics[PW_COUNT] = {ICON_JETPACK, ICON_SNEAKERS, ICON_MAGNET, ICON_DOUBLE};
  int count = (pl.hover > 0) + (pl.headstart > 0);
  for (int i = 0; i < PW_COUNT; i++) count += pl.power[i] > 0;
  int x = 160 - (count * 98 - 8) / 2;
  if (r_y1 > 206) {
    for (int i = 0; i < PW_COUNT; i++)
      if (pl.power[i] > 0) timer(x, 208, pics[i], pl.power[i] / power_time(i), 0xFFC21F), x += 98;
    if (pl.headstart > 0) timer(x, 208, ICON_HEADSTART, pl.headstart / 4.5f, 0xFF8A20), x += 98;
    if (pl.hover > 0) timer(x, 208, ICON_BOARD, pl.hover / 30, 0x7CFF6A);
  }
  /* boards to use, bottom right */
  if (r_y1 > 200) {
    if (pl.hover <= 0 && pg.boards && !pl.dead && count == 0) {
      char s[12] = "OK x";
      fmt(s + 4, pg.boards, false);
      d_rrect(244, 208, 70, 24, 6, C(0x14141E), 150);
      icon(ICON_BOARD, 258, 220, 20);
      d_label(&font_small, 272, 225, s, C(WHITE), C(0x14141E), 1, -1);
    }
    /* the first seconds: a headstart (1), a score booster (3) */
    if (run.start_t < 5 && !pl.dead) {
      x = 6;
      if (pg.headstarts && pl.headstart <= 0) {
        d_panel(x, 204, 100, 30, 8, 2, NAVY, GOLD_T, GOLD_B);
        icon(ICON_HEADSTART, (float)x + 16, 219, 22);
        d_label(&font_small, x + 30, 216, "1: Headstart", C(WHITE), C(NAVY), 1, -1);
        char s[8] = "x";
        fmt(s + 1, pg.headstarts, false);
        d_label(&font_small, x + 30, 229, s, C(WHITE), C(NAVY), 1, -1);
        x += 106;
      }
      if (pg.boosters && !run.boosted) {
        d_panel(x, 204, 112, 30, 8, 2, NAVY, GOLD_T, GOLD_B);
        icon(ICON_BOOSTER, (float)x + 16, 219, 22);
        d_label(&font_small, x + 30, 216, "3: Score Booster", C(WHITE), C(NAVY), 1, -1);
        char s[8] = "x";
        fmt(s + 1, pg.boosters, false);
        d_label(&font_small, x + 30, 229, s, C(WHITE), C(NAVY), 1, -1);
      }
    }
  }
  /* a word on top: a mission done, a key... */
  if (run.toast_t > 0 && r_y0 < 100) {
    int w = d_text_width(&font_mid, run.toast) + 28, slide = run.toast_t > 2 ? (int)((2.2f - run.toast_t) * 150) : 30;
    if (slide > 30) slide = 30;
    int y = 52 + slide;
    d_panel(160 - w / 2, y, w, 26, 10, 2, NAVY, 0x7CFF6A, 0x2E9E3E);
    d_label(&font_mid, 160, y + 19, run.toast, C(WHITE), C(NAVY), 2, 0);
  }
}

static void help_overlay(void) {
  d_dim(110);
  panel(14, 10, 292, 220);
  d_label(&font_mid, 160, 36, "How to play", C(WHITE), C(NAVY), 2, 0);
  static const char *const keys[7][2] = {
    {"Left, Right", "change tracks"}, {"Up", "jump"}, {"Down", "roll, or dive from a jump"},
    {"OK", "hoverboard: a crash forgiven"}, {"1, 3", "headstart, score booster"},
    {"Back", "pause"}, {"Home", "save and quit"},
  };
  for (int i = 0; i < 7; i++) {
    int y = 58 + i * 17;
    d_label(&font_small, 112, y, keys[i][0], C(GOLD_T), C(NAVY), 1, 1);
    d_label(&font_small, 122, y, keys[i][1], C(WHITE), C(NAVY), 1, -1);
  }
  d_label(&font_small, 160, 180, "Hit the side of a train: you stumble.", C(0xD6EEFF), C(NAVY), 1, 0);
  d_label(&font_small, 160, 194, "Stumble twice and the Inspector has you!", C(0xD6EEFF), C(NAVY), 1, 0);
  button(110, 200, 100, 24, "Got it!", true);
}

static void title_overlay(void) {
  if (r_y0 < 110) d_image(160 - im_logo.w / 2, 2, &im_logo);
  /* coins and keys, top right; the best, top left */
  if (r_y0 < 30) {
    counter(312, 22, ICON_COIN, s_total);
    d_label(&font_small, 8, 18, "BEST", C(GOLD_T), C(NAVY), 1, -1);
    d_label(&font_mid, 8, 34, s_best, C(WHITE), C(NAVY), 2, -1);
  }
  if (r_y0 < 50 && r_y1 > 30) counter(312, 44, ICON_KEY, s_keys);
  if (sel == 0 && ((int)(clock * 2.2f) & 1) == 0 && r_y1 > 168 && r_y0 < 192)
    d_label(&font_mid, 160, 186, "Press OK to play", C(WHITE), C(NAVY), 2, 0);
  if (r_y1 > 190) {
    static const char *const names[3] = {"Shop", "Missions", "Help"};
    for (int i = 0; i < 3; i++) button(16 + i * 100, 196, 88, 32, names[i], sel == i + 1);
    /* the multiplier on the missions button */
    d_panel(178, 186, 30, 18, 6, 2, NAVY, GOLD_T, GOLD_B);
    d_label(&font_small, 193, 199, s_mult, C(WHITE), C(NAVY), 1, 0);
  }
}

static void pause_overlay(void) {
  d_dim(120);
  panel(80, 60, 160, 120);
  d_label(&font_big, 160, 98, "Paused", C(WHITE), C(NAVY), 2, 0);
  button(100, 110, 120, 28, "Resume", sel == 0);
  button(100, 144, 120, 28, "Quit game", sel == 1);
}

static void quit_overlay(void) {
  d_dim(120);
  panel(50, 70, 220, 100);
  d_label(&font_mid, 160, 100, back_to == S_TITLE ? "Quit Subway Surfers?" : "Quit this run?", C(WHITE), C(NAVY), 2, 0);
  if (back_to != S_TITLE) d_label(&font_small, 160, 118, "The coins you have are kept", C(WHITE), C(NAVY), 1, 0);
  button(66, 128, 86, 28, "No", sel == 0);
  button(168, 128, 86, 28, "Yes", sel == 1);
}

static void saveme_overlay(void) {
  d_dim(100);
  panel(70, 40, 180, 160);
  d_label(&font_big, 160, 76, "Save me!", C(WHITE), C(NAVY), 2, 0);
  float f = 1 - state_t / 5;
  if (f < 0) f = 0;
  d_ring(160, 120, 30, 6, C(0x0B3A75), 255);
  /* the time left, as a ring going round */
  for (int i = 0; i < 40 && i < (int)(f * 40); i++) {
    float s, c;
    r_sincos((float)i * 0.157f, &s, &c);
    d_disc(160 + s * 30, 120 - c * 30, 3.2f, C(0x7CFF6A), 255);
  }
  icon(ICON_KEY, 152, 120, 26);
  char s[12] = "x";
  fmt(s + 1, (uint32_t)run_save_cost(), false);
  d_label(&font_mid, 178, 128, s, C(WHITE), C(NAVY), 2, -1);
  char h[32] = "You have ";
  fmt(h + 9, pg.keys, false);
  cat(h, pg.keys == 1 ? " key" : " keys");
  d_label(&font_small, 160, 166, h, C(WHITE), C(NAVY), 1, 0);
  d_label(&font_small, 160, 184, "OK: use keys   Back: no", C(GOLD_T), C(NAVY), 1, 0);
}

static void results_overlay(void) {
  d_dim(90);
  panel(30, 14, 260, 172);
  d_label(&font_mid, 160, 40, "SCORE", C(GOLD_T), C(NAVY), 2, 0);
  d_label(&font_big, 160, 74, s_score, C(WHITE), C(NAVY), 2, 0);
  if (new_best && r_y0 < 100) {
    d_panel(96, 80, 128, 22, 8, 2, NAVY, 0xFF7AB8, 0xE0306E);
    d_label(&font_mid, 160, 97, "High score!", C(WHITE), C(NAVY), 2, 0);
  } else d_label(&font_small, 160, 96, s_best, C(WHITE), C(NAVY), 1, 0);
  icon(ICON_COIN, 132, 118, 22);
  d_label(&font_mid, 148, 125, s_coins, C(WHITE), C(NAVY), 2, -1);
  /* the missions: done or not */
  for (int i = 0; i < 3; i++) {
    int y = 140 + i * 14;
    icon(pg.mdone & 1 << i ? ICON_CHECK : ICON_STAR, 50, (float)y + 4, 12);
    Mission m;
    mission_get(pg.mset, i, &m);
    char t[48];
    mission_text(&m, t, sizeof t);
    d_label(&font_small, 62, y + 8, t, C(pg.mdone & 1 << i ? 0x9DFF8E : WHITE), C(NAVY), 1, -1);
  }
  static const char *const names[3] = {"Play", "Shop", "Menu"};
  for (int i = 0; i < 3; i++) button(30 + i * 90, 196, 80, 32, names[i], sel == i);
}

static void missions_overlay(void) {
  d_dim(110);
  panel(14, 10, 292, 220);
  d_label(&font_mid, 160, 36, "Missions", C(WHITE), C(NAVY), 2, 0);
  d_panel(130, 44, 60, 34, 10, 2, NAVY, GOLD_T, GOLD_B);
  d_label(&font_big, 160, 71, s_mult, C(WHITE), C(NAVY), 2, 0);
  for (int i = 0; i < 3; i++) {
    int y = 86 + i * 38;
    if (y > r_y1 || y + 34 < r_y0) continue;
    bool done = pg.mdone & 1 << i;
    d_rrect(26, y, 268, 34, 8, C(done ? 0x2E9E3E : 0x0E4C96), 230);
    Mission m;
    mission_get(pg.mset, i, &m);
    char t[48];
    mission_text(&m, t, sizeof t);
    icon(done ? ICON_CHECK : ICON_STAR, 42, (float)y + 17, 22);
    d_label(&font_small, 58, y + 14, t, C(WHITE), C(NAVY), 1, -1);
    uint32_t p = done ? m.target : mission_progress(i);
    if (p > m.target) p = m.target;
    d_rrect(58, y + 21, 150, 8, 3, C(0x061A36), 255);
    d_rrect(59, y + 22, (int)(148 * (float)p / (float)m.target) + 1, 6, 3, C(0x7CFF6A), 255);
    char n[24];
    fmt(n, p, true);
    cat(n, "/");
    fmt(n + strlen(n), m.target, true);
    d_label(&font_small, 284, y + 29, n, C(WHITE), C(NAVY), 1, 1);
  }
  char nx[40] = "Complete all three: multiplier x";
  int next = 2 + pg.mset > 30 ? 30 : 2 + pg.mset;
  fmt(nx + strlen(nx), (uint32_t)next, false);
  d_label(&font_small, 160, 212, nx, C(GOLD_T), C(NAVY), 1, 0);
}

static void shop_overlay(void) {
  /* the top: the shop, and the coins */
  if (r_y0 < 52) {
    d_label(&font_mid, 10, 24, "Shop", C(WHITE), C(NAVY), 2, -1);
    counter(312, 24, ICON_COIN, s_total);
    int x = 6;
    for (int t = 0; t < TABS; t++) {
      int w = d_text_width(&font_small, tab_names[t]) + 18;
      d_panel(x, 32, w, 20, 7, 2, NAVY, t == shop_tab ? GOLD_T : BLUE_T, t == shop_tab ? GOLD_B : BLUE_B);
      d_label(&font_small, x + w / 2, 46, tab_names[t], C(WHITE), C(NAVY), 1, 0);
      x += w + 4;
    }
  }
  bool preview = shop_tab == TAB_CHARS || shop_tab == TAB_BOARDS;
  int lw = preview ? 190 : 308;
  int n = shop_rows();
  for (int r = 0; r < 4 && shop_top + r < n; r++) {
    int i = shop_top + r, y = 58 + r * 38;
    if (y > r_y1 || y + 36 < r_y0) continue;
    bool focus = i == shop_sel;
    d_panel(6, y, lw, 34, 8, 2, NAVY, focus ? 0x5CC8FF : 0x2C8FE0, focus ? 0x2A86E0 : 0x1462BA);
    char name[24], sub[48], price[16];
    int pic = ICON_STAR;
    sub[0] = price[0] = 0;
    bool owned = false;
    switch (shop_tab) {
      case TAB_UPGRADES: {
        strcpy(name, upg_names[i]);
        pic = upg_icons[i];
        int c = upgrade_cost(pg.upg[i]);
        if (c) fmt(price, (uint32_t)c, true);
        else owned = true;
        /* six steps */
        for (int k = 0; k < 6; k++) d_rrect(44 + k * 16, y + 22, 13, 7, 2, C(k <= pg.upg[i] ? 0x7CFF6A : 0x0B3A75), 255);
        char s[12];
        fmt(s, (uint32_t)power_time(i), false);
        cat(s, " s");
        d_label(&font_small, 146, y + 29, s, C(WHITE), C(NAVY), 1, -1);
        break;
      }
      case TAB_ITEMS: {
        strcpy(name, item_names[i]);
        strcpy(sub, item_text[i]);
        pic = item_icons[i];
        fmt(price, item_price[i], true);
        uint32_t have = i == 0 ? pg.boards : i == 1 ? pg.headstarts : i == 2 ? pg.boosters : 0;
        if (i < 3) {
          char s[12] = "x";
          fmt(s + 1, have, false);
          cat(name, " ");
          cat(name, s);
        }
        break;
      }
      case TAB_CHARS:
        strcpy(name, char_names[i]);
        owned = pg.chars & 1 << i;
        pic = owned ? (pg.character == i ? ICON_CHECK : ICON_STAR) : ICON_LOCK;
        strcpy(sub, owned ? (pg.character == i ? "Running" : "OK to choose") : "");
        if (!owned) fmt(price, char_price[i], true);
        break;
      default:
        strcpy(name, board_names[i]);
        strcpy(sub, board_power[i]);
        owned = pg.ownboards & 1 << i;
        pic = owned ? (pg.board == i ? ICON_CHECK : ICON_BOARD) : ICON_LOCK;
        if (!owned) fmt(price, board_price[i], true);
        break;
    }
    icon(pic, 24, (float)y + 17, 24);
    d_label(&font_mid, 42, y + 16, name, C(WHITE), C(NAVY), 2, -1);
    if (sub[0] && shop_tab != TAB_UPGRADES) d_label(&font_small, 42, y + 29, sub, C(0xD6EEFF), C(NAVY), 1, -1);
    if (price[0]) {
      int w = d_text_width(&font_small, price) + 30;
      d_panel(6 + lw - w - 6, y + 6, w, 22, 7, 2, NAVY, GOLD_T, GOLD_B);
      icon(ICON_COIN, (float)(6 + lw - w + 6), (float)y + 17, 14);
      d_label(&font_small, 6 + lw - 10, y + 22, price, C(WHITE), C(NAVY), 1, 1);
    } else if (owned && shop_tab == TAB_UPGRADES) d_label(&font_small, 6 + lw - 10, y + 22, "MAX", C(GOLD_T), C(NAVY), 1, 1);
  }
  if (n > 4 && r_y1 > 210) {
    d_label(&font_small, 90, 226, shop_top > 0 ? "more above" : "more below", C(0xD6EEFF), C(NAVY), 1, 0);
  }
  if (note_t > 0 && r_y1 > 200) {
    int w = d_text_width(&font_mid, note) + 24;
    d_panel(160 - w / 2, 206, w, 26, 9, 2, NAVY, 0x7CFF6A, 0x2E9E3E);
    d_label(&font_mid, 160, 225, note, C(WHITE), C(NAVY), 2, 0);
  } else if (r_y1 > 214 && n <= 4) d_label(&font_small, 160, 230, "Left, Right: shelves   OK: buy   Back: leave", C(0xD6EEFF), C(NAVY), 1, 0);
}

static void overlay(void) {
  switch (state) {
    case S_HELP: help_overlay(); break;
    case S_TITLE: title_overlay(); break;
    case S_RUN:
      if (intro > 0.6f) hud();
      break;
    case S_PAUSE: hud(), pause_overlay(); break;
    case S_SAVEME: saveme_overlay(); break;
    case S_RESULTS: results_overlay(); break;
    case S_SHOP: shop_overlay(); break;
    case S_MISSIONS: missions_overlay(); break;
    case S_QUIT:
      if (back_to == S_TITLE) title_overlay();
      quit_overlay();
      break;
  }
}

/* the shop's background: a deep blue, lighter in the middle */
static void shop_background(void) {
  for (int y = r_y0; y < r_y1; y++) {
    uint16_t c = r_565(r_mix888(0x2C8FE0, 0x0B3A75, y * 256 / SCREEN_H));
    uint16_t *p = r_col + (y - r_y0) * SCREEN_W;
    for (int x = 0; x < SCREEN_W; x++) p[x] = c;
  }
}

static void draw_shop(void) {
  r_begin();
  if (shop_tab == TAB_CHARS || shop_tab == TAB_BOARDS) {
    /* a turning preview, on the right */
    r_camera(0, 0.75f, -2.6f, 0.06f, 240, 258, 136);
    float yaw = 3.14159f + clock * 0.9f;
    /* the stand */
    V3 st[8];
    for (int k = 0; k < 8; k++) {
      float s, c;
      r_sincos((float)k * 0.785f, &s, &c);
      st[k] = (V3){s * 0.7f, 0, c * 0.7f};
    }
    r_poly(st, 8, 0x1A4F9A, P_NOHAZE, 0);
    if (shop_tab == TAB_CHARS) {
      anim_play(&a_shop, &clip_runner_idle, 0, 0);
      actor_draw(shop_sel, &a_shop, (V3){0, 0, 0}, yaw, 0, 1);
    } else {
      anim_play(&a_shop, &clip_runner_board, 0, 0);
      board_draw((V3){0, 0.05f, 0}, shop_sel, yaw, clock, 1);
      actor_draw(pg.character, &a_shop, (V3){0, 0.23f, 0}, yaw, 0, 1);
    }
  }
  r_render(shop_background, overlay);
}

uint32_t perf_polys __attribute__((used));   /* (read by tools/emu.py and the tests) */

void app_draw(void) {
  /* the numbers on the screens, once a frame */
  fmt(s_score, run.score, false);
  fmt(s_coins, run.coins, false);
  fmt(s_total, pg.coins, false);
  fmt(s_keys, pg.keys, false);
  strcpy(s_mult, "x");
  fmt(s_mult + 1, (uint32_t)(state == S_RUN || state == S_PAUSE ? run.multiplier : multiplier()), false);
  if (state == S_RESULTS) {
    strcpy(s_best, "Best: ");
    fmt(s_best + 6, pg.best, false);
  } else fmt(s_best, pg.best, false);
  if (state == S_SHOP) {
    draw_shop();
    return;
  }
  /* the screens over a picture that stands still */
  bool still = state == S_RESULTS || state == S_PAUSE || state == S_QUIT || state == S_MISSIONS || state == S_HELP;
  if (still && drawn && state_t > 0.5f) return;
  drawn = true;
  draw_world();
  perf_polys = (uint32_t)r_count();
  r_render(scene_background, overlay);
}
