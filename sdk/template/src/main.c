/* Star Catcher: NumPlay's game template. Catch the falling stars, miss three and it's over.
 *
 * A whole small game in one file, written the way NumPlay's small games are (Block Blast, Sokoban, Sudoku):
 * - it draws with the launcher's strips, shapes and fonts (launcher/src/gfx.h): a scene function paints
 *   the screen 16 rows at a time, so no frame buffer has to fit in RAM, and only the rows that changed are
 *   sent to the screen;
 * - the game thinks at a fixed rate (TICK_MS), however fast the calculator draws, catching up when behind;
 * - Back pauses (Resume, Restart, Quit game, which asks first), Home quits from anywhere, and the keys are
 *   written on the screen;
 * - the best score is saved as packed bytes with a magic letter and a version, read back or reset cleanly.
 *
 * Inside NumPlay (built with NP_MODULE=1) it uses the launcher's drawing, fonts and files; on its own
 * (StarCatcher.nwa) it brings them along. tools/new_game.py copies it into games/<your game>/ to start
 * a new game: see docs/make-a-game.md. */
#include <eadk.h>
#include "../../../launcher/src/gfx.h"
#include "../../../games/common/np_text.h"

#if NP_MODULE /* (np_file_read and np_file_write: NumPlay's, launcher/src/np.h) */
#define np_app_begin()
#define np_app_end() 0
#else
#include "../../../games/common/epsilon_app.h"
#include "../../../games/common/epsilon_files.h"
#define np_file_read ef_read
#define np_file_write ef_write
#ifndef HOST /* (a computer build has no app header) */
const char eadk_app_name[] __attribute__((section(".rodata.eadk_app_name"))) = "Star Catcher";
const uint32_t eadk_api_level __attribute__((section(".rodata.eadk_api_level"))) = 0;
#endif
void np_push(int x, int y, int w, int h, const color_t *px) {
  eadk_display_push_rect((eadk_rect_t){(uint16_t)x, (uint16_t)y, (uint16_t)w, (uint16_t)h}, px);
}
#endif

#define KEY(k) (1ull << (k))
#define TICK_MS 20 /* the game thinks 50 times a second */
#define SKY HEX(0x0B1638)
#define NIGHT HEX(0x1C2B5E)
#define GOLD HEX(0xFFD34E)
#define WHITE 0xFFFF
#define HUD 22 /* the band on top */

/* starcatcher.sav: the best score, packed, with a magic letter and a version */
#define SAVE_NAME "starcatcher.sav"
static struct {
  uint8_t magic, version, flags, pad; /* flags: 1 the guide was read */
  uint32_t best;
} sv;

static color_t strip[SCREEN_W * STRIP_H];
static int bx = 160, score, missed, sel, over;
static struct {
  int16_t x, y, speed; /* y and speed in 1/16 pixels */
} stars[4];
static uint32_t seed = 1;
enum { PLAY, PAUSE, ASK, GUIDE, OVER };
static int state = GUIDE;

static uint32_t rnd(uint32_t n) { /* a number from 0 to n - 1 */
  seed ^= seed << 13, seed ^= seed >> 17, seed ^= seed << 5;
  return seed % n;
}
static void drop(int i) {
  stars[i].x = (int16_t)(16 + rnd(SCREEN_W - 32));
  stars[i].y = (int16_t)(-(int)rnd(160) * 16 - 160);
  stars[i].speed = (int16_t)(20 + rnd(12) + score / 2);
}
static void restart(void) {
  bx = 160, score = missed = 0;
  for (int i = 0; i < 4; i++) drop(i);
  state = PLAY;
}
static void save(void) {
  sv.magic = 'C', sv.version = 1;
  np_file_write(SAVE_NAME, &sv, sizeof sv);
}
static void load(void) {
  uint32_t len = 0;
  const uint8_t *d = np_file_read(SAVE_NAME, &len);
  if (d && len == sizeof sv && d[0] == 'C' && d[1] == 1)
    for (uint32_t i = 0; i < len; i++) ((uint8_t *)&sv)[i] = d[i]; /* (a record isn't aligned: byte by byte) */
}

/* one step of the game, TICK_MS long */
static void tick(uint64_t keys) {
  int dx = (keys & KEY(eadk_key_right) ? 4 : 0) - (keys & KEY(eadk_key_left) ? 4 : 0);
  bx = bx + dx < 20 ? 20 : bx + dx > SCREEN_W - 20 ? SCREEN_W - 20 : bx + dx;
  for (int i = 0; i < 4; i++) {
    stars[i].y = (int16_t)(stars[i].y + stars[i].speed);
    int y = stars[i].y / 16;
    if (y >= 206 && y < 222 && stars[i].x > bx - 22 && stars[i].x < bx + 22) score++, drop(i); /* caught */
    else if (y > SCREEN_H) {                                                                     /* missed */
      drop(i);
      if (++missed == 3) {
        over = score > (int)sv.best;
        if (over) sv.best = (uint32_t)score;
        save();
        state = OVER, sel = 0;
      }
    }
  }
}

/* a choice of a menu, the one picked in gold */
static void choice(int y, const char *s, bool on) {
  if (on) gfx_rrect(100, y - 17, 120, 24, 12, GOLD, 256);
  gfx_text_center(&np_font_body, 160, y, s, on ? SKY : WHITE, 256);
}
static void star(int x, int y, color_t c) {
  gfx_fill(x - 1, y - 6, 3, 13, c), gfx_fill(x - 6, y - 1, 13, 3, c), gfx_circle(x, y, 3, c, 256);
}
static void scene(void *ctx) {
  char s[24];
  gfx_vgrad(0, 0, SCREEN_W, SCREEN_H, 0x0B1638, 0x1C2B5E);
  for (int i = 0; i < 4; i++) star(stars[i].x, stars[i].y / 16, GOLD);
  gfx_rrect(bx - 20, 210, 40, 10, 5, WHITE, 256); /* the basket */
  gfx_fill(0, 0, SCREEN_W, HUD, SKY);
  int n = 0;
  for (int v = score; v || !n; v /= 10) s[n++] = (char)('0' + v % 10);
  for (int i = 0; i < n / 2; i++) { char t = s[i]; s[i] = s[n - 1 - i], s[n - 1 - i] = t; }
  s[n] = 0;
  gfx_text(&np_font_body, 8, 16, s, GOLD, 256);
  for (int i = 0; i < 3; i++) star(300 - 16 * i, 11, i < 3 - missed ? WHITE : NIGHT);
  if (state == PLAY) {
    gfx_text_center(&np_font_small, 160, 236, T("Left Right: move   Back: pause"), WHITE, 160);
    return;
  }
  gfx_fill_alpha(0, 0, SCREEN_W, SCREEN_H, 0, 120);
  gfx_rrect(70, 56, 180, 132, 14, NIGHT, 256);
  if (state == GUIDE) {
    gfx_text_center(&np_font_title, 160, 86, T("Star Catcher"), GOLD, 256);
    gfx_paragraph(&np_font_body, 160, 112, 160, 16, T("Catch the stars in the basket. Miss three and it's over."),
                  WHITE, 256);
    choice(174, T("Play"), true);
  } else if (state == OVER) {
    gfx_text_center(&np_font_title, 160, 86, over ? T("New best!") : T("Game over"), GOLD, 256);
    choice(142, T("Play again"), sel == 0), choice(172, T("Quit game"), sel == 1);
  } else {
    gfx_text_center(&np_font_title, 160, 86, state == PAUSE ? T("Paused") : T("Quit game?"), GOLD, 256);
    if (state == PAUSE) choice(118, T("Resume"), sel == 0), choice(146, T("Restart"), sel == 1), choice(174, T("Quit game"), sel == 2);
    else choice(132, T("No"), sel == 0), choice(162, T("Yes"), sel == 1);
  }
}

int main(void) {
  np_app_begin();
  color_t *their_strip = gfx_buf; /* (inside NumPlay, the launcher's: given back at the end) */
  gfx_buf = strip;
  load();
  seed = (uint32_t)eadk_timing_millis() | 1;
  restart();
  if (sv.flags & 1) state = PLAY;
  else state = GUIDE;
  uint64_t held = eadk_keyboard_scan(); /* keys still down from the launcher wait to be let go */
  uint32_t next = (uint32_t)eadk_timing_millis();
  for (bool quit = false; !quit;) {
    uint64_t k = eadk_keyboard_scan(), hit = k & ~held;
    held = k;
    if (k & (KEY(eadk_key_home) | KEY(eadk_key_on_off))) break;
    bool ok = hit & (KEY(eadk_key_ok) | KEY(eadk_key_exe)), back = hit & KEY(eadk_key_back);
    int n = state == PAUSE ? 3 : 2;
    if (state != PLAY && hit & KEY(eadk_key_up)) sel = (sel + n - 1) % n;
    if (state != PLAY && hit & KEY(eadk_key_down)) sel = (sel + 1) % n;
    if (state == PLAY && back) state = PAUSE, sel = 0;
    else if (state == GUIDE && ok) sv.flags |= 1, save(), state = PLAY;
    else if (state == PAUSE && (ok || back)) state = !ok || sel == 0 ? PLAY : sel == 1 ? (restart(), PLAY) : ASK, sel = 0;
    else if (state == ASK && (ok || back)) quit = ok && sel == 1, state = PAUSE, sel = 2;
    else if (state == OVER && ok) sel == 0 ? restart() : (void)(quit = true);
    /* the game's ticks since the last frame (at most 5: then it slows down instead of jumping) */
    uint32_t now = (uint32_t)eadk_timing_millis();
    for (int i = 0; i < 5 && (int32_t)(now - next) >= 0; i++, next += TICK_MS)
      if (state == PLAY) tick(k);
    if ((int32_t)(now - next) > 5 * TICK_MS) next = now;
    gfx_render(scene, 0, 0, SCREEN_H);
    eadk_timing_msleep(1);
  }
  save();
  gfx_buf = their_strip;
  return np_app_end();
}
