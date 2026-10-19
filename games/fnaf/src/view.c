/* Drawing a night: the office (turning, its lights, doors, buttons and fan), the monitor and its cameras
 * (the feed's slow turn, its static, the map), and around them the clock, the power, the call's subtitles
 * and the captions of what can be heard. */
#include <stddef.h>
#include "game.h"
#include "gfx.h"
#include "res.h"

#define N g.n
#define GREY RGB(0xB8B8B8)

/* chosen once a frame (the bands of a frame must agree) */
static struct {
  bool light_off;    /* the door light flickers */
  bool hall_lit;     /* 2A's light flickers on */
  int fan;
  int halluc;        /* 0, or what flashes: 1 Golden Freddy, 2 the endoskeleton, 3 IT'S ME */
  int freddy_face;   /* in the dark */
  int noise, shake;
  uint32_t seed;
} f;

static void prepare(void) {
  f.light_off = rnd() % 10 <= 1;
  f.hall_lit = rnd() % 10 < 3;
  f.fan = (int)(N.t * 30) % 3;
  f.halluc = N.halluc > 0 && rnd() % 10 == 0 ? 1 + (int)(rnd() % 3) : 0;
  f.freddy_face = rnd() % 4 == 0;
  f.noise = 4 + (int)(rnd() % 4) + (rnd() % 16 == 0 ? 6 : 0);
  f.shake = (int)(rnd() % 7) - 3;
  f.seed = rnd();
}

/* ------------------------------------------------------------------ text helpers */
static void small_digits(int font, int x, int y, int v, uint16_t c) {
  char s[4];
  int n = 0;
  if (v >= 100) s[n++] = (char)('0' + v / 100);
  if (v >= 10) s[n++] = (char)('0' + v / 10 % 10);
  s[n++] = (char)('0' + v % 10);
  s[n] = 0;
  gfx_text(font, x, y, s, c);
}

/* a 3 x 5 font for the map's buttons: 1 to 7, A, B, C */
static const uint16_t tiny[10] = {0x2C97, 0x73E7, 0x73CF, 0x5BC9, 0x79CF, 0x79EF, 0x7249, 0x2BED, 0x6BAE, 0x3923};
static void tiny_text(int x, int y, const char *s, uint16_t c) {
  for (; *s; s++, x += 4) {
    int k = *s >= '1' && *s <= '7' ? *s - '1' : *s - 'A' + 7;
    uint16_t b = tiny[k];
    for (int r = 0; r < 5; r++)
      for (int col = 0; col < 3; col++)
        if (b >> (14 - r * 3 - col) & 1) gfx_fill(x + col, y + r, 1, 1, c);
  }
}

/* wraps s to lines of at most w pixels: the start and length of line k, false past the last */
static bool wrap(int font, const char *s, int w, int k, const char **start, int *len) {
  for (;;) {
    while (*s == ' ') s++;
    if (!*s) return false;
    int n = 0, last_space = -1;
    while (s[n] && gfx_text_wn(font, s, n + 1) <= w) {
      if (s[n] == ' ') last_space = n;
      n++;
    }
    if (s[n] && last_space > 0) n = last_space;
    if (!k--) {
      *start = s, *len = n;
      return true;
    }
    s += n;
  }
}

/* ------------------------------------------------------------------ around the view */
static void clock_and_night(void) {
  char h[3] = {'1', '2', 0};   /* 12 AM, then 1 to 5 */
  if (N.hour) h[0] = (char)('0' + N.hour), h[1] = 0;
  int x = 310 - gfx_text_w(FONT_BIG, " AM") - gfx_text_w(FONT_CLOCK, h);
  x += gfx_text(FONT_CLOCK, x, 6, h, WHITE);
  gfx_text(FONT_BIG, x, 8, " AM", WHITE);
  int nx = 310 - gfx_text_w(FONT_HUD, "Night ") - gfx_text_w(FONT_PIXEL, "0");
  nx += gfx_text(FONT_HUD, nx, 31, "Night ", WHITE);
  small_digits(FONT_PIXEL, nx, 30, N.night, WHITE);
}

static void power_hud(void) {
  int x = 8 + gfx_text(FONT_HUD, 8, 206, "Power left: ", WHITE);
  int p = N.power / 10;
  small_digits(FONT_PIXEL, x, 205, p, WHITE);
  gfx_text(FONT_HUD, x + gfx_text_w(FONT_PIXEL, p >= 10 ? "00" : "0") + 1, 206, "%", WHITE);
  x = 8 + gfx_text(FONT_HUD, 8, 222, "Usage: ", WHITE);
  static const uint32_t bars[5] = {0x00C000, 0x00C000, 0xE6E600, 0xE00000, 0xE00000};
  for (int i = 0; i < N.usage && i < 5; i++) {
    gfx_fill(x + 9 * i, 222, 7, 12, RGB(bars[i]));
    gfx_fill(x + 9 * i + 1, 223, 5, 2, gfx_mix(WHITE, RGB(bars[i]), 12));
  }
}

static void call_hud(void) {
  if (N.night > 5 || N.call_muted || N.t >= call_end(N.night)) return;
  if (N.t > 3) {
    gfx_blend(6, 6, 74, 15, RGB(0x202020), 22);
    gfx_frame_rect(6, 6, 74, 15, GREY);
    gfx_text(FONT_HUD, 12, 8, "MUTE CALL", WHITE);
    gfx_text(FONT_TEXT, 84, 8, "0", RGB(0x909090));
  }
  const char *s = call_line(N.night, N.t);
  if (!s) return;
  const char *line;
  int len, n = 0;
  while (n < 3 && wrap(FONT_TEXT, s, 300, n, &line, &len)) n++;
  int y = 52;
  gfx_blend(4, y - 2, 312, 14 * n + 4, BLACK, 18);
  for (int k = 0; k < n; k++) {
    wrap(FONT_TEXT, s, 300, k, &line, &len);
    int w = gfx_text_wn(FONT_TEXT, line, len);
    gfx_text_n(FONT_TEXT, 160 - w / 2 + 1, y + 14 * k + 1, line, len, BLACK);
    gfx_text_n(FONT_TEXT, 160 - w / 2, y + 14 * k, line, len, RGB(0xF2F2C8));
  }
}

void view_captions(void) {
  int y = 186;
  for (int i = 3; i >= 0; i--) {
    if (!g.cap[i] || g.cap_t[i] <= 0) continue;
    int a = g.cap_t[i] > 0.5f ? 32 : (int)(g.cap_t[i] * 64);
    gfx_text(FONT_TEXT, 9, y + 1, g.cap[i], BLACK);
    gfx_text(FONT_TEXT, 8, y, g.cap[i], gfx_mix(RGB(0xD8E8FF), BLACK, a));
    y -= 13;
  }
}

/* the bar at the bottom to bring the monitor up or down */
static void flip_bar(void) {
  gfx_blend(118, 229, 84, 9, WHITE, 6);
  gfx_frame_rect(118, 229, 84, 9, gfx_mix(WHITE, BLACK, 14));
  for (int k = 0; k < 2; k++)
    for (int i = 0; i < 4; i++) {
      gfx_fill(152 + 9 * k + i, 235 - i, 1, 1, WHITE);
      gfx_fill(152 + 9 * k + 7 - i, 235 - i, 1, 1, WHITE);
    }
}

/* ------------------------------------------------------------------ the office */
static void office(void) {
  int px = (int)N.pan;
  if (N.dark) {
    if (N.dark == 3 && (N.dark_t > 0.35f || (int)(N.dark_t * 30) & 1)) {
      gfx_fill(0, gfx_y0, SCREEN_W, BAND_H, BLACK);
      return;
    }
    pic_part(N.dark == 2 && f.freddy_face ? PIC_DARK_FREDDY : PIC_DARK, 0, 0, px, 0, SCREEN_W, SCREEN_H);
    return;
  }
  int base = PIC_OFFICE;
  if (N.light == 1 + LEFT && !f.light_off) base = N.pos[BONNIE] == AT_DOOR ? PIC_OFFICE_LLB : PIC_OFFICE_LL;
  if (N.light == 1 + RIGHT && !f.light_off) base = N.pos[CHICA] == AT_DOOR ? PIC_OFFICE_RLC : PIC_OFFICE_RL;
  pic_part(base, 0, 0, px, 0, SCREEN_W, SCREEN_H);
  pic_draw(PIC_FAN0 + f.fan, FAN_X - px, FAN_Y);
  pic_part(PIC_PANEL_L0 + (N.door[LEFT] | (N.light == 1 + LEFT) << 1), PANEL_L_X - px, PANEL_Y, 0, 0, 26, 80);
  pic_draw(PIC_PANEL_R0 + (N.door[RIGHT] | (N.light == 1 + RIGHT) << 1), PANEL_R_X - px, PANEL_Y);
  static const int doors[2][4] = {{PIC_DOOR_L, DOOR_L_X, DOOR_L_W, DOOR_L_H}, {PIC_DOOR_R, DOOR_R_X, DOOR_R_W, DOOR_R_H}};
  for (int s = 0; s < 2; s++) {
    int vis = (int)(N.door_anim[s] * doors[s][3] + 0.5f);
    if (vis > 0) pic_part(doors[s][0], doors[s][1] - px, 0, 0, doors[s][3] - vis, doors[s][2], vis);
  }
  if (N.gold == 3) pic_part(PIC_OFFICE_GOLD, GOLD_X - px, GOLD_Y, 0, 0, GOLD_W, GOLD_H);
}

/* ------------------------------------------------------------------ the cameras */
static const char *const cam_names[CAM_COUNT] = {"Show Stage", "Dining Area", "Pirate Cove", "West Hall",
                                                 "W. Hall Corner", "Supply Closet", "East Hall", "E. Hall Corner",
                                                 "Backstage", "Kitchen", "Restrooms"};
static const char *const cam_labels[CAM_COUNT] = {"1A", "1B", "1C", "2A", "2B", "3", "4A", "4B", "5", "6", "7"};

static bool here(int who, int cam) { return N.pos[who] == cam; }

static int cam_picture(void) {
  bool b = here(BONNIE, N.cam), c = here(CHICA, N.cam), fr = here(FREDDY, N.cam);
  switch (N.cam) {
  case CAM_1A:
    if (!fr) return PIC_CAM1A_EMPTY;
    return b && c ? PIC_CAM1A_BCF : b ? PIC_CAM1A_BF : c ? PIC_CAM1A_CF : PIC_CAM1A_F;
  case CAM_1B: return PIC_CAM1B_EMPTY + (b && c && fr ? 7 : b && c ? 4 : b && fr ? 5 : c && fr ? 6 : b ? 1 : c ? 2 : fr ? 3 : 0);
  case CAM_1C: return PIC_CAM1C_0 + (N.foxy < 3 ? N.foxy : 3);
  case CAM_2A: return !f.hall_lit ? PIC_CAM2A_DARK : b ? PIC_CAM2A_B : PIC_CAM2A_EMPTY;
  case CAM_2B: return (N.gold == 1 || N.gold == 2) && !b ? PIC_CAM2B_GOLD : b ? PIC_CAM2B_B : PIC_CAM2B_EMPTY;
  case CAM_3: return b ? PIC_CAM3_B : PIC_CAM3_EMPTY;
  case CAM_4A: return c && fr ? PIC_CAM4A_CF : c ? PIC_CAM4A_C : fr ? PIC_CAM4A_F : PIC_CAM4A_EMPTY;
  case CAM_4B: return c ? PIC_CAM4B_C : fr ? PIC_CAM4B_F : PIC_CAM4B_EMPTY;
  case CAM_5: return b ? PIC_CAM5_B : PIC_CAM5_EMPTY;
  case CAM_7: return c && fr ? PIC_CAM7_CF : c ? PIC_CAM7_C : fr ? PIC_CAM7_F : PIC_CAM7_EMPTY;
  }
  return -1;
}

static void camera_map(void) {
  const uint8_t *m = section(SEC_MAP);
  int size = (int)rd16(m), stride = (size + 7) >> 3;
  int mx = SCREEN_W - size - 4, my = SCREEN_H - size - 12;
  for (int r = 0; r < size; r++) {
    int y = my + r;
    if (y < gfx_y0 || y >= gfx_y0 + BAND_H) continue;
    const uint8_t *row = m + 4 + r * stride;
    uint16_t *d = gfx_band + (y - gfx_y0) * SCREEN_W + mx;
    for (int c = 0; c < size; c++)
      if (row[c >> 3] >> (c & 7) & 1) d[c] = WHITE;
  }
  const uint8_t *centres = m + 4 + size * stride;
  bool blink = !((int)(g.t * 3) & 1);
  for (int k = 0; k < CAM_COUNT; k++) {
    int cx = mx + (int)rd16(centres + 4 * k), cy = my + (int)rd16(centres + 4 * k + 2);
    bool sel = k == N.cam;
    gfx_fill(cx - 8, cy - 5, 17, 11, sel && blink ? RGB(0x8BC34A) : RGB(0x424242));
    gfx_frame_rect(cx - 8, cy - 5, 17, 11, RGB(0xD0D0D0));
    tiny_text(cx - (cam_labels[k][1] ? 3 : 1), cy - 2, cam_labels[k], WHITE);
  }
  gfx_text(FONT_HUD, mx - 2, my - 18, cam_names[N.cam], WHITE);
}

static void cameras(void) {
  float c = N.cam_t;   /* 5 seconds there, 2 still, 5 back, 2 still */
  float a = c < 5 ? c / 5 : c < 7 ? 1 : c < 12 ? 1 - (c - 7) / 5 : 0;
  int px = (int)(a * CAM_PAN);
  if (N.glitch > 0) {
    gfx_fill(0, gfx_y0, SCREEN_W, BAND_H, BLACK);
    gfx_static(0, gfx_y0, SCREEN_W, BAND_H, 16, f.seed);
  } else if (N.cam == CAM_6) {
    gfx_fill(0, gfx_y0, SCREEN_W, BAND_H, BLACK);
    gfx_static(0, gfx_y0, SCREEN_W, BAND_H, 7, f.seed);
    gfx_text_c(FONT_BIG, 100, 64, "CAMERA DISABLED", WHITE);
    gfx_text_c(FONT_BIG, 100, 86, "-AUDIO ONLY-", WHITE);
  } else if (N.cam == CAM_2A && N.foxy_run >= 0 && N.foxy_run < 35) {
    pic_draw(PIC_FOXYRUN0 + (int)(N.foxy_run / 35 * FOXYRUN_N), 0, 0);
    gfx_static(0, gfx_y0, SCREEN_W, BAND_H, f.noise, f.seed);
  } else {
    pic_part(cam_picture(), 0, 0, px, 0, SCREEN_W, SCREEN_H);
    gfx_static(0, gfx_y0, SCREEN_W, BAND_H, f.noise, f.seed);
  }
  if (g.blip > 0) gfx_blend(0, gfx_y0, SCREEN_W, BAND_H, WHITE, (int)(g.blip * 120));
  gfx_frame_rect(5, 5, 310, 230, gfx_mix(WHITE, BLACK, 20));
  if ((int)(g.t * 1.5f) & 1) {   /* the recording dot */
    static const uint8_t dot[7] = {2, 1, 0, 0, 0, 1, 2};
    for (int r = 0; r < 7; r++) gfx_fill(14 + dot[r], 26 + r, 7 - 2 * dot[r], 1, RGB(0xE02020));
  }
  camera_map();
}

/* ------------------------------------------------------------------ the night */
static void hallucination(void) {
  if (!f.halluc) return;
  if (f.halluc == 1) pic_draw(PIC_GOLDEN, 0, 0);
  else if (f.halluc == 2) pic_draw(PIC_TITLE3, 0, 0);
  else {
    gfx_fill(0, gfx_y0, SCREEN_W, BAND_H, BLACK);
    gfx_text_c(FONT_HUGE, 160, 104, "IT'S ME", WHITE);
  }
}

static void night_draw(void) {
  bool cams = N.flip >= 1 && !N.dark;
  if (cams) cameras();
  else {
    office();
    if (N.flip > 0) {   /* the monitor coming up, or down */
      int k = (int)(N.flip * FLIP_N);
      pic_draw(PIC_FLIP0 + (k < FLIP_N ? k : FLIP_N - 1), 0, 0);
    }
  }
  hallucination();
  if (!N.dark) {
    clock_and_night();
    power_hud();
    call_hud();
    if (!cams || N.flip >= 1) flip_bar();
  }
  view_captions();
}

void view_night(void) {
  if (gfx_y0 == 0) prepare();
  night_draw();
}

/* ------------------------------------------------------------------ jumpscares */
void view_scare(void) {
  static const int first[5] = {PIC_BONNIE0, PIC_CHICA0, PIC_FREDDY0, PIC_FOXY0, PIC_POWEROUT0};
  static const int count[5] = {BONNIE_N, CHICA_N, FREDDY_N, FOXY_N, POWEROUT_N};
  static const float fps[5] = {30, 30, 25, 25, 30};
  static const bool loops[5] = {true, true, false, false, false};
  if (gfx_y0 == 0) prepare();
  int w = g.scare, k = (int)(g.scare_t * fps[w]);
  k = loops[w] ? k % count[w] : k < count[w] ? k : count[w] - 1;
  int dx = g.scare_t > 0.1f ? f.shake : 0;
  gfx_fill(0, gfx_y0, SCREEN_W, BAND_H, BLACK);
  pic_draw(first[w] + k, dx, 0);
}
