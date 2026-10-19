/* Space Invaders on a computer, for tests and pictures (make host, make test).
 *
 *   play test           a bot plays for an hour of the game's time: it must clear a wave, hit the
 *                       saucer, lose a game and be back on the attract screen, with the rack, the
 *                       shots and the picture always agreeing
 *   play film DIR N S   N frames of the bot's game from frame S, a picture every 2 frames
 *                       (DIR/f_NNNN.ppm)
 */
#define main si_main
#include "../src/main.c"
#undef main
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint16_t fb[SCREEN_H][SCREEN_W];
void eadk_display_push_rect(eadk_rect_t r, const eadk_color_t *p) {
  for (int j = 0; j < r.height; j++)
    for (int i = 0; i < r.width; i++) fb[r.y + j][r.x + i] = p[j * r.width + i];
}
void eadk_display_push_rect_uniform(eadk_rect_t r, eadk_color_t c) {
  for (int j = 0; j < r.height; j++)
    for (int i = 0; i < r.width; i++) fb[r.y + j][r.x + i] = c;
}
bool eadk_display_wait_for_vblank(void) { return true; }
uint64_t eadk_keyboard_scan(void) { return 0; }
uint64_t eadk_timing_millis(void) { return 0; }
void eadk_timing_msleep(uint32_t ms) { (void)ms; }
uint32_t eadk_random(void) { return 0x9E3779B9u; }
void *np_alloc(uint32_t size) { return malloc(size); }

static void picture(const char *path) {
  FILE *f = fopen(path, "wb");
  if (!f) exit(2);
  fprintf(f, "P6 320 240 255\n");
  for (int y = 0; y < SCREEN_H; y++)
    for (int x = 0; x < SCREEN_W; x++) {
      uint16_t c = fb[y][x];
      fputc((c >> 11) * 255 / 31, f), fputc((c >> 5 & 63) * 255 / 63, f), fputc((c & 31) * 255 / 31, f);
    }
  fclose(f);
}

/* the bot: out of the way of the shots coming down, else under the lowest invader nearest (or where
   the saucer will be), firing; a new press each shot */
static void step(uint32_t k) { /* a frame, as main() runs it: OK on the attract screen starts a game */
  if (state == S_TITLE && k & KEY(eadk_key_ok)) start(), fire_held = 1;
  tick(k);
}

static uint32_t bot(void) {
  static int press;
  press ^= 1;
  if (state != S_PLAY) return press && frames % 40 < 2 ? KEY(eadk_key_ok) : 0;
  int c = px + 7, target = -1, best = 1 << 30;
  for (int k = 0; k < 3; k++) /* a shot above the cannon and close: step aside */
    if (as[k].on && as[k].y > PY - 70 && as[k].x + 4 > px - 2 && as[k].x < px + 18)
      return as[k].x + 1 < c ? KEY(eadk_key_right) : KEY(eadk_key_left);
  if (udx && !u_hit) target = ux + 8 + udx * 44;
  for (int i = 0; target < 0 && i < 55; i++)
    if (st[i] & 1) {
      int d = abs(ax[i] + 7 - c) - 4 * ay[i];
      if (d < best) best = d, target = ax[i] + 7;
    }
  if (target < 0) return 0;
  if (target > c + 1) return KEY(eadk_key_right);
  if (target < c - 1) return KEY(eadk_key_left);
  return press ? KEY(eadk_key_ok) : 0;
}

/* the rack and the picture agree: every live invader is drawn where it is (a burst going may cut
   into one, as in the arcade, until it moves again) */
static int check(void) {
  int n = 0;
  for (int i = 0; i < 55; i++)
    if (st[i] & 1 && spawn == 55 && state == S_PLAY) {
      int lit = 0;
      n++;
      lit = blit(look(i), ax[i], ay[i], TEST);
      if (!lit)
        return printf("invader %d not drawn at %d, %d (cursor %d, saucer %d %d, burst %d %d %d, wave %d)\n", i, ax[i], ay[i],
                      cur, ux, udx, pb_x, pb_y, pb_t, wave), 1;
    }
  if (spawn == 55 && state == S_PLAY && n != live) return printf("%d alive, %d counted\n", n, live), 1;
  if (score < 0 || score > 9999 || lives < 0 || lives > 9) return printf("score %d, lives %d\n", score, lives), 1;
  return 0;
}

int main(int argc, char **argv) {
  gfx_buf = strip;
  load();
  title();
  if (argc >= 5 && !strcmp(argv[1], "film")) {
    int n = atoi(argv[3]), s = atoi(argv[4]);
    for (int f = 0; f < s + n; f++) {
      step(bot());
      if (f >= s && (f - s) % 2 == 0) {
        char path[256];
        present();
        gfx_render(scene, 0, 0, SCREEN_H);
        snprintf(path, sizeof path, "%s/f_%04d.ppm", argv[2], (f - s) / 2);
        picture(path);
      }
    }
    return 0;
  }
  int waves = 0, saucers = 0, overs = 0, titles = 0, was = state;
  for (int f = 0; f < 60 * 60 * 60; f++) {
    step(bot());
    if (u_hit == 119) saucers++;
    if (state == S_PLAY && wave > waves) waves = wave;
    if (state != was) overs += state == S_OVER, titles += state == S_TITLE, was = state;
    if (f % 7 == 0) present();
    if (check()) return printf("frame %d\n", f), 1;
  }
  printf("waves reached %d, saucers hit %d, games over %d, back to the title %d, best %u\n", waves, saucers, overs,
         titles, sv.hi);
  if (waves < 2 || saucers < 1 || overs < 1 || titles < 1) return printf("FAIL\n"), 1;
  printf("ok\n");
  return 0;
}
