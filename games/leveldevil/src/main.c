/* Level Devil: run to the door. The level has other plans.
 *
 * Unept's troll platformer (2023): sixteen worlds of five short rooms in flat colours, with a tiny
 * black runner. Floors drop away under you, spikes pop up, walls push, the door runs off, the
 * controls turn round, gravity flips... The rooms are the game's own: their tiles, their actors and
 * what their scripts do (tools/rooms.py, from tools/scenes.json).
 *
 * A room: its ground (tiles of 16 pixels, 33 x 15), its objects (blocks, spikes, saws, springs,
 * doors, triggers, coins, warps, buttons...) and its script: steps, each set off by an event (the
 * room starting, the runner touching a trigger, a key) after so many 60ths of a second, that move,
 * grow, show or hide objects, or change the runner (its speed, its jump, its size, gravity). The
 * game steps 60 times a second, in the scene's pixels; it is drawn at 3/4 (the screen shows x 50
 * to 477 of the 528 pixels, all 240 high), in the launcher's strips (launcher/src/gfx.c). */
#include <eadk.h>
#include "../../../launcher/src/gfx.h"
#include "../../common/np_text.h"
#include "../../common/epsilon_app.h"
#include "../../common/epsilon_files.h"

const char eadk_app_name[] __attribute__((section(".rodata.eadk_app_name"))) = "Level Devil";
const uint32_t eadk_api_level __attribute__((section(".rodata.eadk_api_level"))) = 0;
void np_push(int x, int y, int w, int h, const color_t *px) {
  eadk_display_push_rect((eadk_rect_t){(uint16_t)x, (uint16_t)y, (uint16_t)w, (uint16_t)h}, px);
}

#define KEY(k) (1ull << (k))
#define ROOMS 5
#define LEVELS 16

/* the packed rooms (tools/rooms.py), column by column: 80 rooms' modes and runners, their ground
   (33 columns of 15 bits), how many spans each has, the spans' fields, how many steps, the steps' */
#include "rooms.inc" /* data, ROOM_BYTES, SPANS, STEPS */
#define D_GROUND (5 * 80)
#define D_NSPAN (D_GROUND + 66 * 80)
#define D_KIND (D_NSPAN + 80)
#define D_FLAG (D_KIND + SPANS)
#define D_EV (D_FLAG + SPANS)
#define D_X (D_EV + SPANS)
#define D_Y (D_X + 2 * SPANS)
#define D_W (D_Y + 2 * SPANS)
#define D_H (D_W + SPANS)
#define D_N (D_H + SPANS)
#define D_NSTEP (D_N + SPANS)
#define D_SEV (D_NSTEP + 80)
#define D_ST (D_SEV + STEPS)
#define D_CODE (D_ST + 2 * STEPS)
#define D_OBJ (D_CODE + STEPS)
#define D_SN (D_OBJ + STEPS)
#define D_COND (D_SN + STEPS)
#define D_A (D_COND + STEPS)
#define D_B (D_A + 2 * STEPS)
#define D_C (D_B + 2 * STEPS)
#define D_K (D_C + 2 * STEPS)

enum { K_BLOCK, K_SPIKE, K_SAW, K_SPRING, K_DOOR, K_TRIG, K_COIN, K_BAD, K_WARP, K_BUTTON, K_THRU, K_GSPIKE, K_DEATH };
#define F_HIDE 1
#define F_GHOST 2  /* nothing to stand on */
#define F_OFF 4    /* not there (yet, or any more) */
#define F_BIG 8    /* a saw's second picture, twice as big */
#define F_USED 16  /* a trigger gone off, a coin taken, a button pressed */
#define F_ICE 32
#define F_HOP 64   /* thrown up, falling back */
enum { R_WRAP = 1, R_WRAPY = 2, R_SWAP = 4, R_FLAPPY = 16, R_GRAVITY = 32 };
enum { O_VEL, O_MOVE, O_SET, O_GROW, O_FLAG, O_SPAWN, O_WARP, O_HERO, O_GRAV, O_HGROW, O_SHAKE, O_FIRE, O_REARM,
       O_TOWARD, O_RULE, O_ACCEL, O_HOP, O_KEY };
enum { H_RUN, H_JUMP, H_MAXUP, H_MAXDOWN, H_BACK, H_DOWNJUMP, H_DIR, H_SLIDE, H_PUSHX };
enum { U_NONE, U_PATROL, U_FOLLOW, U_STOPX, U_WRAPX };
#define EV_KEY 9
#define EV_MOVE 10
#define EVENTS 11

/* the colours of the sixteen worlds: background, ground */
static const uint32_t colors[LEVELS][2] = {
  {0xFEB854, 0x996B07}, {0xF5B13A, 0xBE7708}, {0xF8C750, 0xCD8507}, {0xF8C750, 0xE37B07},
  {0xE4C448, 0xE36821}, {0xFD8E57, 0xD65C2C}, {0xFECB30, 0xD34F2B}, {0xED9970, 0xB44D2A},
  {0xCDB67D, 0xA24829}, {0xF48B7C, 0x844934}, {0xFFBC73, 0x754E39}, {0xF49E7C, 0x624C40},
  {0xEA7851, 0x4A3B35}, {0xCD7647, 0x40312A}, {0xAB7050, 0x401D0D}, {0xCD7647, 0x2F1509},
};
#define BLACK 0
#define WHITE 0xFFFF
#define GREY HEX(0xC8C8C8)
#define DARK HEX(0x6D1102) /* the game's dark red: its transitions, its buttons */
#define YELLOW HEX(0xFCF965)

/* leveldevil.sav */
static struct {
  uint8_t magic, version, flags, open; /* flags: 1 the keys were shown; open: the levels open, 1 to 16 */
  uint8_t level, room;                 /* the level last played, and the room reached in it */
  uint16_t done;                       /* the levels cleared, a bit each */
  uint32_t deaths;                     /* in all */
  uint16_t run, sel;                   /* deaths since that level began; the door picked last */
  uint16_t best[LEVELS];               /* the fewest deaths each level was cleared with */
} sv;
#define SAVE_NAME "leveldevil.sav"
#define COPY_NAME "leveldevil_saves.py"

typedef struct {
  int32_t x, y, tx, ty;   /* its centre, and where a move takes it (256ths of a pixel) */
  int16_t w, h, gw, gh;   /* half its size, and what it grows to (16ths of a pixel) */
  int16_t vx, vy, ax, ay; /* 256ths of a pixel a step; and a step each step */
  int16_t rest;           /* thrown up: the y it comes down to (pixels) */
  uint16_t ml, gl;        /* steps left of its move, of its growing */
  uint8_t kind, f, ev, t; /* kind (and its turn, 90 degrees a step, in bits 4 and 5); flags; t: a spring's push */
} obj_t;
typedef struct {
  uint16_t t;
  uint8_t ev, cond, code, o, done;
  int16_t a, b, c;
} step_t;
static obj_t ob[120];
static step_t st[240];
static struct {
  uint8_t o, type;
  int16_t p, q;
} rule[12];
static int no, ns, nr;
static uint16_t gcol[33];

static color_t strip[SCREEN_W * STRIP_H];
static int level, rnum, ri, scr, ov, sel, banner, full, tries, modes, frame, shake, shake_n;
static uint8_t cnt[EVENTS];
static uint16_t at_[EVENTS];
/* the runner: its box's corner and its speed (256ths of a pixel, a step), its size (pixels) */
static int32_t hx, hy, vx, vy;
static int hw, hh, hsx, hsy, tsx, tsy, hgl;
static int run, jumpv, maxup, maxdown, back, downjump, dir, slide, pushx, gx, gy, keyev;
static int face, ground, coyote, jumping, anim, dead, won, frozen, born, started, flapped, held_in, fade, waiting;
static uint32_t seed, banner_t, now;
static color_t bg, fg;
static struct {
  int32_t x, y;
  int16_t vx, vy;
} bits[8]; /* the runner, in pieces */
enum { SC_HUB, SC_PLAY };
enum { OV_NONE, OV_PAUSE, OV_HELP, OV_QUIT, OV_END };
enum { I_LEFT = 1, I_RIGHT = 2, I_UP = 4, I_DOWN = 8 };

static uint32_t rnd(void) {
  seed ^= seed << 13;
  seed ^= seed >> 17;
  return seed ^= seed << 5;
}
static int imin(int a, int b) { return a < b ? a : b; }
static int imax(int a, int b) { return a > b ? a : b; }
static int clampi(int v, int a, int b) { return v < a ? a : v > b ? b : v; }
static int iabs(int v) { return v < 0 ? -v : v; }
static int rd16(int at) { return (int16_t)(data[at] | data[at + 1] << 8); }
static char *cat(char *o, const char *s) {
  while (*s) *o++ = *s++;
  *o = 0;
  return o;
}
static char *num(char *o, uint32_t v) {
  char t[10];
  int n = 0;
  do t[n++] = (char)('0' + v % 10); while (v /= 10);
  while (n) *o++ = t[--n];
  *o = 0;
  return o;
}
static char *label(char *o, const char *s, uint32_t v) {
  num(cat(o, s), v);
  return o;
}
/* The save, and a copy of it in a Python script: installing apps from the NumWorks website keeps only
   Python scripts, so the save comes back from the copy when the file itself is gone. The copy is a
   comment line "#>leveldevil.sav:base64" (the way NumPlay and the big games keep theirs). */
static const char b64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
static const char copy_head[] = "# Level Devil keeps a copy of your progress here, so that\n"
                                "# reinstalling it doesn't erase it. If you delete this\n"
                                "# file, the game writes it again.\n#>" SAVE_NAME ":";
static void save(void) {
  static char copy[sizeof copy_head + 72];
  const uint8_t *d = (const uint8_t *)&sv;
  char *p = copy;
  sv.magic = 'L', sv.version = 1;
  ef_write(SAVE_NAME, &sv, sizeof sv);
  *p++ = 0; /* (the script's status byte) */
  for (const char *h = copy_head; *h;) *p++ = *h++;
  for (unsigned i = 0; i < sizeof sv; i += 3) { /* (48 bytes: no padding) */
    uint32_t v = (uint32_t)d[i] << 16 | d[i + 1] << 8 | d[i + 2];
    for (int k = 18; k >= 0; k -= 6) *p++ = b64[v >> k & 63];
  }
  *p++ = '\n', *p++ = 0;
  ef_write(COPY_NAME, copy, (uint32_t)(p - copy));
}
/* the save back from the copy, if only the copy is left */
static void restore(void) {
  uint32_t len, n = sizeof copy_head;
  const uint8_t *c = ef_read(COPY_NAME, &len);
  uint8_t out[sizeof sv];
  if (ef_read(SAVE_NAME, &len) || !(c = ef_read(COPY_NAME, &len)) || len < n + 64) return;
  for (uint32_t i = 0; i < n - 1; i++)
    if (c[1 + i] != (uint8_t)copy_head[i]) return;
  for (unsigned i = 0, j = n; i < sizeof sv; i += 3, j += 4) {
    uint32_t v = 0;
    for (int k = 0; k < 4; k++) {
      int x = 0;
      while (x < 64 && (uint8_t)b64[x] != c[j + k]) x++;
      if (x == 64) return;
      v = v << 6 | (uint32_t)x;
    }
    out[i] = (uint8_t)(v >> 16), out[i + 1] = (uint8_t)(v >> 8), out[i + 2] = (uint8_t)v;
  }
  ef_write(SAVE_NAME, out, sizeof out);
}
#define PS(v) ((v) * 64 / 15) /* pixels a second: 256ths of a pixel a step */
#define PS2(v) ((v) * 64 / 900) /* pixels a second, each second: 256ths a step, each step */

/* ------------------------------------------------------------ the room */
static int cell(int tx, int ty) { return tx >= 0 && tx < 33 && ty >= 0 && ty < 15 && gcol[tx] >> ty & 1; }
/* any ground in the pixels [x0, x1) x [y0, y1)? */
static int ground_in(int x0, int y0, int x1, int y1) {
  for (int tx = x0 >> 4; tx <= (x1 - 1) >> 4; tx++)
    for (int ty = y0 >> 4; ty <= (y1 - 1) >> 4; ty++)
      if (cell(tx, ty)) return 1;
  return 0;
}
static void box(const obj_t *o, int *b) { /* its box in pixels: x0, y0, x1, y1 */
  int w = o->w >> 4, h = o->h >> 4;
  b[0] = (o->x >> 8) - w, b[1] = (o->y >> 8) - h, b[2] = (o->x >> 8) + w, b[3] = (o->y >> 8) + h;
}
static int hit(const int *b, int x0, int y0, int x1, int y1) { return x0 < b[2] && b[0] < x1 && y0 < b[3] && b[1] < y1; }
static int there(const obj_t *o) { return !(o->f & F_OFF); }
static int solid(const obj_t *o) { return (o->kind & 15) == K_BLOCK && !(o->f & (F_GHOST | F_OFF)) && o->w >= 8 && o->h >= 8; }

/* what the runner's box would hit at (x, y): the ground, a block, the top of a one-way tile it is
   above (feet: where its feet were, or -1); 0 for nothing, 1 for the ground, else 2 + the object */
static int blocked(int x, int y, int feet) {
  int b[4];
  if (ground_in(x, y, x + hw, y + hh)) return 1;
  for (int k = 0; k < no; k++) {
    obj_t *o = &ob[k];
    box(o, b);
    if (solid(o) || ((o->kind & 15) == K_THRU && there(o) && feet >= 0 && feet <= b[1]))
      if (hit(b, x, y, x + hw, y + hh)) return 2 + k;
  }
  return 0;
}
/* moves the runner d along an axis, a pixel at a time; what stopped it, or 0 */
static int step_axis(int32_t *p, int32_t d, int axis) {
  int a = *p >> 8, e = (*p + d) >> 8, s = e > a ? 1 : -1, w;
  for (; a != e; a += s)
    if ((w = axis ? blocked(hx >> 8, a + s, s > 0 ? a + hh : -1) : blocked(a + s, hy >> 8, -1))) {
      *p = a * 256 + (s > 0 ? 255 : 0);
      return w;
    }
  *p += d;
  return 0;
}
/* out of what it is in, a pixel at a time, the nearest way; false when it cannot get out */
static int unstick(void) {
  for (int d = 0; d < 64; d++)
    for (int i = 0; i < 4; i++) {
      int x = (hx >> 8) + (i == 2 ? -d : i == 3 ? d : 0), y = (hy >> 8) + (i == 0 ? -d : i == 1 ? d : 0);
      if (!blocked(x, y, -1)) {
        hx = x * 256 + (hx & 255), hy = y * 256 + (hy & 255);
        return 1;
      }
    }
  return 0;
}

static void fire(int e) {
  cnt[e]++, at_[e] = (uint16_t)frame;
  for (int i = 0; i < ns; i++)
    if (st[i].ev == e) st[i].done = 0;
}

static void size_hero(void) { /* its box from its scale, about its centre */
  int w = imax(1, 5 * hsx / 100), h = imax(1, 13 * hsy / 100);
  hx -= (w - hw) * 128, hy -= (h - hh) * 128, hw = w, hh = h;
}

static void reset(void) { /* the room as it starts */
  int s0 = 0, r0 = 0;
  for (int i = 0; i < ri; i++) s0 += data[D_NSPAN + i], r0 += data[D_NSTEP + i];
  const uint8_t *h = data + ri * 5;
  modes = h[0];
  hsx = hsy = tsx = tsy = 100, hgl = 0, hw = 5, hh = 13;
  hx = (rd16(ri * 5 + 1) + 5) * 256, hy = (rd16(ri * 5 + 3) + 3) * 256, vx = vy = 0;
  for (int i = 0; i < 33; i++) gcol[i] = (uint16_t)rd16(D_GROUND + ri * 66 + 2 * i);
  no = nr = 0;
  for (int s = s0; s < s0 + data[D_NSPAN + ri]; s++) {
    int k = data[D_KIND + s];
    for (int i = 0; i < data[D_N + s]; i++) {
      obj_t *o = &ob[no++], z = {0};
      *o = z;
      o->kind = (uint8_t)(k & 63), o->f = data[D_FLAG + s], o->ev = data[D_EV + s];
      o->x = o->tx = (rd16(D_X + 2 * s) + (k & 64 ? 0 : 16 * i)) * 256;
      o->y = o->ty = (rd16(D_Y + 2 * s) + (k & 64 ? 16 * i : 0)) * 256;
      o->w = o->gw = (int16_t)(data[D_W + s] * 16), o->h = o->gh = (int16_t)(data[D_H + s] * 16);
    }
  }
  ns = 0;
  for (int r = r0; r < r0 + data[D_NSTEP + ri]; r++) {
    int o0 = data[D_OBJ + r], k = rd16(D_K + 2 * r);
    for (int i = 0; i < data[D_SN + r]; i++) {
      step_t *p = &st[ns++];
      p->ev = data[D_SEV + r], p->cond = data[D_COND + r], p->code = data[D_CODE + r];
      p->o = (uint8_t)(o0 == 255 ? 255 : o0 + i), p->done = 1;
      p->t = (uint16_t)(rd16(D_ST + 2 * r) + (k ? ((ob[o0 + i].x - ob[o0].x) >> 8) * k / 256 : 0));
      p->a = (int16_t)rd16(D_A + 2 * r), p->b = (int16_t)rd16(D_B + 2 * r), p->c = (int16_t)rd16(D_C + 2 * r);
    }
  }
  for (int e = 0; e < EVENTS; e++) cnt[e] = 0;
  int fl = modes & R_FLAPPY;
  run = PS(fl ? 70 : 80), jumpv = PS(fl ? 251 : 188), maxup = PS(fl ? 9999 : 300), maxdown = PS(fl ? 300 : 200);
  gx = 0, gy = PS2(fl ? 1200 : 800), back = downjump = slide = pushx = 0, dir = 1, keyev = -1;
  face = 1, ground = coyote = jumping = dead = won = frozen = started = flapped = waiting = 0, born = 18;
  frame = 0, shake = 0, full = 1;
  fire(0);
}
static void enter(void) { /* room rnum of the level */
  ri = level * ROOMS + rnum;
  bg = gfx_rgb(colors[level][0]), fg = gfx_rgb(colors[level][1]);
  tries = 0;
  reset();
}

static void die(void) {
  if (dead || won) return;
  dead = 1, shake = 5, shake_n = 30;
  sv.deaths++, sv.run++;
  for (int i = 0; i < 8; i++) {
    bits[i].x = hx + hw * 128, bits[i].y = hy + hh * 128;
    bits[i].vx = (int16_t)PS((int)(rnd() % 361) - 180), bits[i].vy = (int16_t)-PS(300 + (int)(rnd() % 100));
  }
  save();
}

/* a step of the script */
static void exec(const step_t *p) {
  obj_t *o = &ob[p->o == 255 ? 0 : p->o];
  int a = p->a, b = p->b, c = p->c, big = o->f & F_BIG ? 2 : 1;
  switch (p->code) {
    case O_VEL: o->vx = (int16_t)PS(a), o->vy = (int16_t)PS(b), o->ml = 0; break;
    case O_MOVE:
      o->tx = o->x + a * 256, o->ty = o->y + b * 256, o->ml = (uint16_t)c;
      if (!c) o->x = o->tx, o->y = o->ty;
      break;
    case O_SET:
      if (c & 1) o->x = o->tx = a * 256;
      if (c & 2) o->y = o->ty = b * 256;
      break;
    case O_GROW:
      o->gw = (int16_t)(a * 16 * big), o->gh = (int16_t)(b * 16 * big), o->gl = (uint16_t)c;
      if (!c) o->w = o->gw, o->h = o->gh;
      break;
    case O_FLAG:
      if (a & ~o->f & F_BIG) o->w *= 2, o->h *= 2, o->gw *= 2, o->gh *= 2;
      o->f = (uint8_t)((o->f | a) & ~b);
      break;
    case O_SPAWN:
      o->f &= ~F_OFF;
      if (a != 255) o->x = o->tx = ob[a].x + b * 256, o->y = o->ty = ob[a].y + c * 256;
      break;
    case O_WARP: hx = (a + 5) * 256, hy = (b + 3) * 256, vx = vy = 0, frozen = 45; break;
    case O_HERO:
      if (a == H_RUN) run = PS(b);
      if (a == H_JUMP) jumpv = PS(b);
      if (a == H_MAXUP) maxup = PS(b);
      if (a == H_MAXDOWN) maxdown = PS(b);
      if (a == H_BACK) back = b;
      if (a == H_DOWNJUMP) downjump = b;
      if (a == H_DIR) dir = b;
      if (a == H_SLIDE) slide = b;
      if (a == H_PUSHX) pushx = PS(b), run = PS(300);
      break;
    case O_GRAV: gx = PS2(a), gy = PS2(b); break;
    case O_HGROW: tsx = a, tsy = b, hgl = c ? c : 1; break;
    case O_SHAKE: shake = a, shake_n = b; break;
    case O_FIRE: fire(a); break;
    case O_REARM: o->f &= ~F_USED; break;
    case O_TOWARD: o->vx = (int16_t)PS(hx < o->x ? -a : a); break;
    case O_RULE:
      if (a == U_NONE) {
        for (int i = 0; i < nr; i++)
          if (rule[i].o == p->o && (!b || rule[i].type == b)) rule[i] = rule[--nr], i--;
      } else if (nr < 12) {
        rule[nr].o = p->o, rule[nr].type = (uint8_t)a, rule[nr].p = (int16_t)b, rule[nr++].q = (int16_t)c;
      }
      break;
    case O_ACCEL: o->ax = (int16_t)PS2(a), o->ay = (int16_t)PS2(b); break;
    case O_HOP:
      if (!(o->f & F_HOP)) o->f |= F_HOP, o->rest = (int16_t)(o->y >> 8), o->vy = (int16_t)-PS(a);
      break;
    default: keyev = a; /* O_KEY */
  }
}

static void script(void) {
  for (int i = 0; i < ns; i++) {
    step_t *p = &st[i];
    int c = p->cond;
    if (p->done || !cnt[p->ev] || (uint16_t)(frame - at_[p->ev]) < p->t) continue;
    p->done = 1;
    if (!c || (c < 16 ? cnt[p->ev] == c : c < 32 ? cnt[c & 15] : !cnt[c & 15])) exec(p);
  }
}

/* the runner pushed out of the way of a block that moved by (dx, dy) pixels into it */
static void shove(obj_t *o, int dx, int dy) {
  int b[4];
  box(o, b);
  if (!hit(b, hx >> 8, hy >> 8, (hx >> 8) + hw, (hy >> 8) + hh)) return;
  int32_t px = hx, py = hy;
  o->f |= F_GHOST; /* (the block left out while the runner moves out of it) */
  if (dy) step_axis(&hy, (dy < 0 ? b[1] - hh - (hy >> 8) : b[3] - (hy >> 8)) * 256, 1);
  else if (dx) step_axis(&hx, (dx < 0 ? b[0] - hw - (hx >> 8) : b[2] - (hx >> 8)) * 256, 0);
  o->f &= ~F_GHOST;
  if (hit(b, hx >> 8, hy >> 8, (hx >> 8) + hw, (hy >> 8) + hh) || (px == hx && py == hy && !unstick())) die();
}

static void objects(void) {
  int b[4];
  for (int k = 0; k < no; k++) {
    obj_t *o = &ob[k];
    int kind = o->kind & 15;
    if (!there(o)) continue;
    int32_t ox = o->x, oy = o->y;
    if (o->gl) o->w += (o->gw - o->w) / o->gl, o->h += (o->gh - o->h) / o->gl, o->gl--;
    if (o->ml) o->x += (o->tx - o->x) / o->ml, o->y += (o->ty - o->y) / o->ml, o->ml--;
    if (o->f & F_HOP || kind == K_GSPIKE) o->vy = (int16_t)clampi(o->vy + gy, -PS(300), PS(300));
    o->vx += o->ax, o->vy += o->ay;
    o->x += o->vx;
    box(o, b);
    if ((o->ax || kind == K_GSPIKE) && ground_in(b[0], b[1], b[2], b[3])) o->x -= o->vx, o->vx = 0;
    o->y += o->vy;
    box(o, b);
    if (kind == K_GSPIKE) { /* falling with gravity, onto the ground and the blocks */
      int c[4], stop = ground_in(b[0], b[1], b[2], b[3]);
      for (int j = 0; j < no; j++)
        if (solid(&ob[j])) box(&ob[j], c), stop |= hit(c, b[0], b[1], b[2], b[3]);
      if (stop) o->y -= o->vy, o->vy = 0;
    }
    if (o->f & F_HOP && o->y >> 8 >= o->rest) o->y = o->rest * 256, o->vy = 0, o->f &= ~F_HOP;
    if (solid(o) && !dead) shove(o, (o->x >> 8) - (ox >> 8), (o->y >> 8) - (oy >> 8));
    if (kind == K_SPRING && o->t) o->t--;
  }
  for (int i = 0; i < nr; i++) {
    obj_t *o = &ob[rule[i].o];
    int x = o->x >> 8, p = rule[i].p, q = rule[i].q;
    switch (rule[i].type) {
      case U_PATROL:
        if (x <= p) o->vx = (int16_t)PS(70);
        if (x >= q) o->vx = (int16_t)-PS(70);
        break;
      case U_FOLLOW: o->x = ob[p].x + q * 256; break;
      case U_STOPX:
        if ((o->vx > 0 && x >= p) || (o->vx < 0 && x <= p)) o->vx = 0, o->x = q * 256;
        break;
      case U_WRAPX:
        if (o->vx < 0 && x < p) o->x = q * 256;
        if (o->vx > 0 && x > q) o->x = p * 256;
    }
  }
}

/* what the runner touches */
static void contacts(void) {
  int x = hx >> 8, y = hy >> 8, b[4];
  for (int k = 0; k < no && !dead && !won; k++) {
    obj_t *o = &ob[k];
    int kind = o->kind & 15, t = o->kind >> 4 & 3, w = o->w >> 4, h = o->h >> 4;
    if (!there(o)) continue;
    box(o, b);
    if (kind == K_SPIKE) { /* the teeth: the 5 pixels of 16 they stand in, turned with it */
      if (t == 0) b[1] = b[3] - h * 5 / 8;
      if (t == 2) b[3] = b[1] + h * 5 / 8;
      if (t == 1) b[2] = b[0] + w * 5 / 8;
      if (t == 3) b[0] = b[2] - w * 5 / 8;
    }
    if (kind == K_COIN || kind == K_BAD || kind == K_DEATH) b[0] += w / 3, b[1] += h / 3, b[2] -= w / 3, b[3] -= h / 3;
    if (kind == K_SPRING) b[1] = b[3] - h * 3 / 8;
    if (kind == K_DOOR) b[0] += w / 4, b[2] -= w / 4, b[1] += h / 4;
    if (!hit(b, x, y, x + hw, y + hh)) continue;
    if (kind == K_SAW) { /* round: the nearest point of the box to its middle */
      int cx = o->x >> 8, cy = o->y >> 8, dx = cx - clampi(cx, x, x + hw - 1), dy = cy - clampi(cy, y, y + hh - 1);
      if (dx * dx + dy * dy < (w - 1) * (w - 1)) die();
    } else if (kind == K_SPIKE || kind == K_BAD || kind == K_GSPIKE || kind == K_DEATH) {
      die();
    } else if (kind == K_SPRING && !o->t) {
      vy = -maxup, jumping = 1, o->t = 18;
    } else if (kind == K_TRIG && !(o->f & F_USED)) {
      o->f |= F_USED, fire(o->ev);
    } else if (kind == K_COIN) {
      o->f |= F_OFF;
    } else if (kind == K_BUTTON) {
      o->f |= F_USED;
    } else if (kind == K_DOOR) {
      won = 1;
    }
  }
}

static void hero(int in) {
  int down = gy < 0 ? -1 : 1, b[4];
  if (hgl) { /* growing or shrinking, out of what it grows into */
    hsx += (tsx - hsx) / hgl, hsy += (tsy - hsy) / hgl, hgl--;
    size_hero();
    if (blocked(hx >> 8, hy >> 8, -1) && !unstick()) die();
  }
  int m = !!(in & I_RIGHT) - !!(in & I_LEFT), jump = (downjump ? I_DOWN : I_UP) & in & ~held_in;
  if (back) m = -m;
  if (m) face = m;
  if (modes & R_FLAPPY) {
    if (!started && in & ~held_in && frame > 30) started = 1;
    if (started) vx = clampi(vx + dir * PS(52), -run, run);
    if (started && in & I_UP & ~held_in) vy = -jumpv, flapped = 8;
    if (vy > maxdown) vy = maxdown;
  } else {
    int under = blocked(hx >> 8, (hy >> 8) + down, down > 0 ? (hy >> 8) + hh - 1 : -1);
    if (pushx) vx = pushx;
    else if (m) vx = m * run;
    else if (ground) {
      obj_t *o = under > 1 ? &ob[under - 2] : 0;
      if (o && (o->vx || o->vy)) vx = o->vx, vy = o->vy; /* carried by what it stands on */
      else if (!(o && o->f & F_ICE)) vx = iabs(vx) <= iabs(gy) ? 0 : vx - (vx > 0 ? 1 : -1) * iabs(gy);
    } else if (!slide) {
      vx = iabs(vx) > PS(10) ? vx * 4 / 5 : 0;
    }
    if (modes & R_GRAVITY) {
      if (in & I_UP & ~held_in && started && (ground || blocked(hx >> 8, (hy >> 8) - down, -1))) gy = -gy, ground = 0;
    } else if (jump && (ground || (coyote < 5 && !jumping))) {
      vy = -down * jumpv, jumping = 1;
    }
  }
  vx += gx, vy += gy;
  if (vy >= maxdown) vy = maxdown;
  if (vy <= -maxup) vy = -maxup;
  if (!pushx) vx = clampi(vx, -run, run);
  if (step_axis(&hx, vx, 0)) vx = 0;
  if (step_axis(&hy, vy, 1)) vy = 0;
  /* where its picture is on the screen (the game's camera: x 50 to 477) */
  int ax = (hx >> 8) - 55, ay = (hy >> 8) - 3;
  if (modes & R_WRAP) { /* off one side of the screen, back in at the other */
    if (ax < -6) hx += 424 * 256;
    if (ax > 419) hx -= 424 * 256;
    if (ay > 234) hy -= 242 * 256;
    if (modes & R_WRAPY && ay < -9) hy += 239 * 256;
    ax = (hx >> 8) - 55, ay = (hy >> 8) - 3;
  }
  if (ay < -16 || ay > 240 || ax < -8 || ax > 421) die();
  ground = blocked(hx >> 8, (hy >> 8) + down, down > 0 ? (hy >> 8) + hh - 1 : -1) != 0;
  if (ground) coyote = 0, jumping &= vy * down < 0;
  else coyote++;
  if (frame == 30) started = 1;
  (void)b;
  contacts();
}

static int sdx, sdy; /* the screen shaking (pixels of the screen) */
static void step(int in) {
  frame++, anim++;
  if (shake_n) shake_n--;
  sdx = shake_n ? ((int)(rnd() % 3) - 1) * (1 + shake / 4) : 0, sdy = shake_n ? ((int)(rnd() % 3) - 1) * (1 + shake / 4) : 0;
  if (dead) {
    for (int i = 0; i < 8; i++) bits[i].x += bits[i].vx, bits[i].y += bits[i].vy, bits[i].vy += PS2(800);
    if (++dead == 30) waiting = 1;
    return;
  }
  if (won) {
    won++;
    return;
  }
  script();
  objects();
  if (born) born--;
  else if (frozen) frozen--;
  else if (!dead) hero(in);
  if ((in & (I_LEFT | I_RIGHT)) && !cnt[EV_MOVE]) fire(EV_MOVE);
  if (keyev >= 0 && in & I_UP & ~held_in) fire(keyev);
  held_in = in;
}

/* ------------------------------------------------------------ drawing */
static int sxp(int32_t x) { return (int)((x * 3 >> 10) - 38) + sdx; } /* 256ths of a scene pixel: the screen's */
static int syp(int32_t y) { return (int)((y * 3 >> 10) + 30) + sdy; }
static void put(int x, int y, color_t c) {
  if (x >= 0 && x < SCREEN_W && y >= gfx_y0 && y < gfx_y1) gfx_buf[(y - gfx_y0) * SCREEN_W + x] = c;
}

/* pictures: 16 rows of 16 bits (the first pixel the lowest) */
static const uint16_t spike_px[16] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0x318C, 0x318C, 0x7BDE, 0x7BDE, 0xFFFF};
static const uint16_t button_px[2][16] = {{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0x7E0, 0xFF0, 0xFF0, 0xFF0, 0xFF0, 0x1FF8},
                                          {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0xFF0, 0x1FF8}};
/* the runner (columns 3 to 11, rows 2 to 15): standing, jumping, running (four) */
static const uint16_t hero_px[6][14] = {
  {0, 0x1C0, 0x1C0, 0x1C0, 0x3E0, 0x3E0, 0x3E0, 0x3E0, 0x3E0, 0x3E0, 0x3E0, 0x360, 0x360, 0x360},
  {0, 0x1C0, 0x1C0, 0x1C0, 0x7E0, 0x7E0, 0x7E0, 0x7E0, 0x3E0, 0x3F8, 0xFF8, 0xF18, 0, 0},
  {0, 0, 0x1C0, 0x1C0, 0x1C0, 0x3E0, 0x3E0, 0x3E0, 0x3E0, 0x3E0, 0x3F0, 0x3F0, 0x330, 0x630},
  {0x1C0, 0x1C0, 0x1C0, 0x7E0, 0x7E0, 0x7E0, 0x3E0, 0x3E0, 0xFE0, 0xFF8, 0xF18, 0, 0, 0},
  {0, 0x1C0, 0x1C0, 0x1C0, 0x3E0, 0x3E0, 0x3E0, 0x3E0, 0x3E0, 0x3E0, 0x7E0, 0x7E0, 0x660, 0xC60},
  {0x1C0, 0x1C0, 0x3F0, 0x3F0, 0x3F0, 0x3E0, 0x3E0, 0xFE0, 0xFF8, 0xF18, 0, 0, 0, 0},
};

/* a picture over the box (x0, y0)-(x1, y1) of the screen, turned t quarters, upside down if fy */
static void picture(const uint16_t *px, int rows, int x0, int y0, int x1, int y1, int t, int fx, int fy, color_t c) {
  int w = x1 - x0, h = y1 - y0;
  if (w <= 0 || h <= 0 || y1 <= gfx_y0 || y0 >= gfx_y1) return;
  for (int y = imax(y0, gfx_y0); y < imin(y1, gfx_y1); y++)
    for (int x = x0; x < x1; x++) {
      int u = (x - x0) * 16 / w, v = (y - y0) * 16 / h, s;
      if (fx) u = 15 - u;
      if (fy) v = 15 - v;
      if (t & 1) s = u, u = v, v = 15 - s;
      if (t & 2) u = 15 - u, v = 15 - v;
      v -= 16 - rows;
      if (v >= 0 && px[v] >> u & 1) put(x, y, c);
    }
}

static void draw_hero(int32_t x, int32_t y, int w, int h, int flip, int f) {
  /* its picture is 16 x 16 about the 5 x 13 box, the box at (5, 3) */
  int32_t px = x - w * 256, py = y - h * 3 * 256 / 13, pw = w * 16 * 256 / 5, ph = h * 16 * 256 / 13;
  if (flip) py = y + h * 256 - ph + h * 3 * 256 / 13; /* (upside down: the box at the picture's top) */
  picture(hero_px[f], 14, sxp(px), syp(py), sxp(px + pw), syp(py + ph), 0, face < 0, flip, BLACK);
}

static void draw_door(int x0, int y0, int x1, int y1, int shut) {
  int w = x1 - x0, h = y1 - y0, e = imax(1, w / 8);
  gfx_rrect(x0, y0, w, h + e, e * 2, fg, 256);
  gfx_fill(x0 + e, y0 + e, w - 2 * e, h - e, GREY);
  if (shut) gfx_fill(x0 + e, y0 + e, w - 2 * e, (h - e) * imin(shut, 12) / 12, fg);
}

static void draw_object(obj_t *o) {
  int kind = o->kind & 15, t = o->kind >> 4 & 3, b[4];
  if (o->f & (F_HIDE | F_OFF) || kind == K_TRIG || o->w <= 0 || o->h <= 0) return;
  box(o, b);
  int x0 = sxp(b[0] * 256), y0 = syp(b[1] * 256), x1 = sxp(b[2] * 256), y1 = syp(b[3] * 256), w = x1 - x0, h = y1 - y0;
  if (y1 < gfx_y0 - 4 || y0 > gfx_y1 + 4) return;
  if (kind == K_BLOCK && b[1] <= 0) y0 = 0, h = y1; /* (on through the margins, as the ground) */
  if (kind == K_BLOCK && b[3] >= 240) h = SCREEN_H - y0;
  if (kind == K_BLOCK || kind == K_GSPIKE) {
    int s = kind == K_GSPIKE ? h * 5 / 26 : 0;
    gfx_fill(x0, y0 + s, w, h - 2 * s, fg);
    if (s) picture(spike_px, 5, x0, y0 - h * 11 / 26, x1, y0 + s, 0, 0, 0, fg), picture(spike_px, 5, x0, y1 - s, x1, y1 + h * 11 / 26, 2, 0, 0, fg);
  } else if (kind == K_SPIKE) {
    picture(spike_px, 5, x0, y0, x1, y1, t, 0, 0, fg);
  } else if (kind == K_SAW) {
    int r = w / 2;
    gfx_circle(x0 + r, y0 + r, r, fg, 256);
    gfx_fill(x0 + r - imax(1, r / 8), y0 + r - imax(1, r / 8), imax(2, r / 4), imax(2, r / 4), bg);
  } else if (kind == K_SPRING) { /* its plate, up on its stem while it pushes */
    int up = o->t ? 8 : 0;
    gfx_fill(x0, y0 + h * (13 - up) / 16, w, imax(1, h / 16), fg);
    gfx_fill(x0 + w * 6 / 16, y0 + h * (14 - up) / 16, w / 4, h * (2 + up) / 16, fg);
  } else if (kind == K_DOOR) {
    if (modes & R_SWAP) draw_hero(o->x - 5 * 128 + 256, o->y - 128, 5, 13, 0, 0);
    else draw_door(x0, y0, x1, y1, won ? won : 0);
  } else if (kind == K_COIN || kind == K_BAD) {
    gfx_rrect(x0 + w * 5 / 16, y0 + h * 5 / 16, w * 6 / 16, h * 6 / 16, 1, fg, 256);
  } else if (kind == K_WARP) { /* rings of squares, turning */
    for (int i = (anim >> 3 & 1); i < 7; i += 2) {
      int d = w * (i + 1) / 16;
      gfx_fill(x0 + d, y0 + d, w - 2 * d, imax(1, w / 16), fg), gfx_fill(x0 + d, y1 - d - 1, w - 2 * d, imax(1, w / 16), fg);
      gfx_fill(x0 + d, y0 + d, imax(1, w / 16), h - 2 * d, fg), gfx_fill(x1 - d - 1, y0 + d, imax(1, w / 16), h - 2 * d, fg);
    }
  } else if (kind == K_BUTTON) {
    picture(button_px[!!(o->f & F_USED)], 6, x0, y0, x1, y1, t, 0, 0, fg);
  } else if (kind == K_THRU) {
    for (int i = 0; i < 3; i++) gfx_fill(x0 + w * (1 + 5 * i) / 16, y0, w / 4, imax(1, h / 8), fg);
  }
}

static void ground_draw(void) {
  for (int ty = 0; ty < 15; ty++) {
    int y0 = syp(ty * 4096), y1 = syp(ty * 4096 + 4096);
    if (ty == 0) y0 = 0;
    if (ty == 14) y1 = SCREEN_H;
    if (y1 <= gfx_y0 || y0 >= gfx_y1) continue;
    for (int tx = 3; tx < 30; tx++) {
      if (!cell(tx, ty)) continue;
      int e = tx;
      while (e < 29 && cell(e + 1, ty)) e++;
      gfx_fill(imax(0, sxp(tx * 4096)), y0, sxp(e * 4096 + 4096) - imax(0, sxp(tx * 4096)), y1 - y0, fg);
      tx = e;
    }
  }
}

static void say(int cx, int base, const char *s) {
  gfx_text_center(&np_font_title, cx + 1, base + 2, s, BLACK, 256);
  gfx_text_center(&np_font_title, cx, base, s, YELLOW, 256);
}
static void button(int y, const char *s, bool on) {
  if (on) gfx_rrect(96, y, 128, 24, 12, YELLOW, 256);
  gfx_text_center(&np_font_body, 160, y + 17, s, on ? DARK : WHITE, 256);
}

static const char *const pause_items[] = {T("Resume"), T("Restart"), T("Levels"), T("Quit game")};
static void overlay(void) {
  char s[24];
  gfx_fill_alpha(0, 0, SCREEN_W, SCREEN_H, BLACK, 100);
  gfx_rrect(56, 36, 208, 168, 12, DARK, 256);
  if (ov == OV_PAUSE) {
    say(160, 68, T("Paused"));
    for (int i = 0; i < 4; i++) button(84 + 28 * i, pause_items[i], sel == i);
  } else if (ov == OV_QUIT) {
    say(160, 100, T("Quit game?"));
    for (int i = 0; i < 2; i++) {
      if (sel == i) gfx_rrect(92 + 72 * i, 128, 64, 24, 12, YELLOW, 256);
      gfx_text_center(&np_font_body, 124 + 72 * i, 145, i ? T("Yes") : T("No"), sel == i ? DARK : WHITE, 256);
    }
  } else if (ov == OV_HELP) {
    static const char *const keys[][2] = {{T("Left Right"), T("run")}, {T("Up, OK, EXE"), T("jump")}, {T("Back"), T("pause")}};
    say(160, 66, T("How to play"));
    gfx_paragraph(&np_font_body, 160, 88, 192, 15, T("Reach the door. The level has other plans: trust nothing."), WHITE,
                  256);
    for (int i = 0; i < 3; i++) {
      gfx_text_right(&np_font_body, 136, 136 + 16 * i, keys[i][0], YELLOW, 256);
      gfx_text(&np_font_body, 146, 136 + 16 * i, keys[i][1], WHITE, 256);
    }
    button(172, T("Play"), true);
  } else { /* OV_END: the Devil beaten */
    say(160, 80, T("You beat the Devil!"));
    gfx_paragraph(&np_font_body, 160, 106, 184, 15, T("Every door of Level Devil is open behind you."), WHITE, 256);
    gfx_text_center(&np_font_body, 160, 150, label(s, T("Deaths in all: "), sv.deaths), WHITE, 256);
    button(164, T("OK"), true);
  }
}

static void play_scene(void *ctx) {
  char s[16];
  gfx_fill(0, 0, SCREEN_W, SCREEN_H, bg);
  for (int k = 0; k < no; k++) draw_object(&ob[k]);
  ground_draw();
  if (dead) {
    for (int i = 0; i < 8; i++) gfx_fill(sxp(bits[i].x), syp(bits[i].y), 3, 2, BLACK);
  } else if (!won && !born && !(frozen && frozen < 45)) {
    int f = !ground && !(modes & R_FLAPPY) ? 1 : vx ? 2 + (anim >> 2 & 3) : 0;
    if (modes & R_SWAP) {
      int b[4] = {(hx >> 8) - 6, (hy >> 8) - 3, (hx >> 8) + 11, (hy >> 8) + 13};
      draw_door(sxp(b[0] * 256), syp(b[1] * 256), sxp(b[2] * 256), syp(b[3] * 256), 0);
    } else {
      draw_hero(hx, hy, hw, hh, gy < 0, f);
    }
  }
  if (born) /* coming together */
    for (int i = 0; i < 8; i++) {
      int d = born * born / 6;
      gfx_fill(sxp(hx + hw * 128 + (i & 3) * d * 64 - d * 96), syp(hy + hh * 128 + (i >> 2) * d * 128 - d * 64), 3, 2, BLACK);
    }
  for (int i = 0; i < ROOMS; i++) { /* the rooms of the level, as the game shows them */
    int x = 136 + 10 * i;
    gfx_fill(x, 6, 9, 8, BLACK);
    if (i > rnum || (i == rnum && frame < 45)) gfx_fill(x + 1, 7, 7, 6, gfx_mix(BLACK, fg, 20));
  }
  if (waiting) { /* the game's retry button */
    gfx_rrect(134, 104, 52, 30, 4, DARK, 256);
    gfx_circle(160, 119, 9, YELLOW, 256), gfx_circle(160, 119, 6, DARK, 256);
    gfx_fill(146, 110, 14, 9, DARK), gfx_fill(152, 106, 4, 13, YELLOW);
    gfx_text_center(&np_font_body, 160, 96, T("Press any key"), WHITE, 256);
  }
  (void)s;
  if (fade) gfx_fill_alpha(0, 0, SCREEN_W, SCREEN_H, DARK, fade * 16);
  if (ov) overlay();
}

/* the doors to the levels: two rows of eight, on two floors */
static void hub_door(int i, int *x, int *y) { *x = 13 + 38 * (i & 7), *y = 70 + 80 * (i >> 3); }
static void hub_scene(void *ctx) {
  char s[48];
  int sel_ = sv.sel;
  bg = gfx_rgb(colors[0][0]);
  gfx_fill(0, 0, SCREEN_W, SCREEN_H, bg);
  gfx_fill(0, 0, SCREEN_W, 44, DARK);
  if (banner) say(160, 32, label(s, T("Level cleared! Deaths: "), (uint32_t)banner - 1));
  else say(160, 32, "LEVEL DEVIL");
  for (int i = 0; i < LEVELS; i++) {
    int x, y, open = i < sv.open;
    hub_door(i, &x, &y);
    fg = gfx_rgb(colors[i][1]);
    if (!(i & 7)) gfx_fill(0, y + 40, SCREEN_W, 8, gfx_rgb(colors[0][1]));
    if (!open) fg = gfx_mix(fg, bg, 20);
    draw_door(x, y, x + 22, y + 40, sv.done >> i & 1 ? 12 : 0);
    num(s, (uint32_t)i + 1);
    gfx_text_center(&np_font_small, x + 11, y - 6, s, fg, 256);
    if (i == sel_) picture(hero_px[0], 14, x - 22, y + 8, x - 2, y + 40, 0, 0, 0, BLACK);
  }
  fg = gfx_rgb(colors[0][1]);
  gfx_fill(0, 214, SCREEN_W, 26, fg);
  if (sel_ >= sv.open) cat(s, T("Locked"));
  else if (sv.done >> sel_ & 1) label(s, T("Cleared. Fewest deaths: "), sv.best[sel_]);
  else cat(s, T("Not cleared yet"));
  gfx_text(&np_font_body, 8, 232, s, bg, 256);
  gfx_text_right(&np_font_body, 312, 232, label(s, T("Deaths "), sv.deaths), bg, 256);
  if (ov) overlay();
}

/* ------------------------------------------------------------ the levels */
static void open_room(void) { /* room rnum, coming out of the game's dark red */
  enter();
  fade = 15;
}
static void start_level(int l) {
  if (l != sv.level || sv.room >= ROOMS) sv.room = 0, sv.run = 0;
  level = sv.level = (uint8_t)l, rnum = sv.room;
  scr = SC_PLAY;
  open_room();
  save();
}
static void level_done(void) {
  int first = !(sv.done >> level & 1);
  if (first || sv.run < sv.best[level]) sv.best[level] = sv.run;
  sv.done |= (uint16_t)(1u << level);
  sv.open = (uint8_t)imax(sv.open, imin(level + 2, LEVELS));
  banner = sv.run + 1, banner_t = now;
  sv.room = 0, sv.run = 0, sv.sel = (uint16_t)imin(level + 1, LEVELS - 1);
  scr = SC_HUB, full = 1;
  if (level == LEVELS - 1 && first) ov = OV_END;
  save();
}

/* a step of the room: the fade, the door reached (it shuts), or the runner */
static void tick(int in) {
  full = 1;
  if (fade && !won) {
    fade--;
    return;
  }
  if (won > 30) fade = imin(won - 30, 15);
  if (won > 46) {
    won = 0, fade = 15;
    if (++rnum == ROOMS) level_done();
    else sv.room = (uint8_t)rnum, save(), enter();
    return;
  }
  step(in);
}

static void load(void) {
  uint32_t len = 0;
  restore();
  const uint8_t *d = ef_read(SAVE_NAME, &len);
  uint8_t *v = (uint8_t *)&sv;
  bool ok = d && len == sizeof sv && d[0] == 'L' && d[1] == 1;
  for (uint32_t i = 0; i < sizeof sv; i++) v[i] = ok ? d[i] : 0;
  sv.open = (uint8_t)clampi(sv.open, 1, LEVELS), sv.sel = (uint16_t)clampi(sv.sel, 0, LEVELS - 1);
  if (sv.level >= LEVELS) sv.level = 0;
}

int main(void) {
  np_app_begin();
  color_t *their_strip = gfx_buf;
  gfx_buf = strip;
  now = (uint32_t)eadk_timing_millis();
  seed = eadk_random() ^ now;
  if (!seed) seed = 1;
  load();
  if (!(sv.flags & 1)) ov = OV_HELP;
  full = 1;
  const uint64_t left = KEY(eadk_key_left) | KEY(eadk_key_four), right = KEY(eadk_key_right) | KEY(eadk_key_six);
  const uint64_t up = KEY(eadk_key_up) | KEY(eadk_key_eight), down = KEY(eadk_key_down) | KEY(eadk_key_two);
  const uint64_t ok = KEY(eadk_key_ok) | KEY(eadk_key_exe) | KEY(eadk_key_five), back_k = KEY(eadk_key_back);
  uint64_t held = eadk_keyboard_scan(), stale = held; /* keys still down from the launcher wait to be let go */
  uint32_t last = now, acc = 0;
  for (;;) {
    now = (uint32_t)eadk_timing_millis();
    uint64_t k = eadk_keyboard_scan(), hitk = k & ~held;
    stale &= k, held = k, k &= ~stale, hitk &= ~stale;
    if (k & (KEY(eadk_key_home) | KEY(eadk_key_on_off))) break;
    int dx = !!(hitk & right) - !!(hitk & left), dy = !!(hitk & down) - !!(hitk & up), quit = 0;
    if (hitk) full |= scr == SC_HUB || ov;
    if (ov) {
      if (ov == OV_PAUSE) sel = (sel + dy + 4) % 4;
      if (ov == OV_QUIT) sel ^= dx != 0;
      if (hitk & ok) {
        if (ov == OV_PAUSE && sel == 1) ov = OV_NONE, reset(); /* Restart */
        else if (ov == OV_PAUSE && sel) ov = sel == 2 ? (scr = SC_HUB, save(), OV_NONE) : OV_QUIT, sel = 0;
        else if (ov == OV_QUIT && sel) quit = 1;
        else ov = ov == OV_QUIT && scr == SC_PLAY ? (sel = 3, OV_PAUSE) : OV_NONE;
        if (!(sv.flags & 1)) sv.flags |= 1, save();
      } else if (hitk & back_k) {
        ov = ov == OV_QUIT && scr == SC_PLAY ? (sel = 3, OV_PAUSE) : OV_NONE;
      }
    } else if (scr == SC_HUB) {
      sv.sel = (uint16_t)clampi(sv.sel + dx + 8 * dy, 0, LEVELS - 1);
      if (hitk & ok && sv.sel < sv.open) start_level(sv.sel);
      if (hitk & back_k) ov = OV_QUIT, sel = 0;
    } else if (hitk & back_k) {
      ov = OV_PAUSE, sel = 0, full = 1;
      save();
    } else if (waiting && hitk) { /* the game's retry: any key */
      tries++, reset();
      hitk = 0;
    }
    if (quit) break;
    if (banner && now - banner_t > 2600) banner = 0, full = 1;
    acc += (now - last) * 3, last = now;
    if (scr == SC_PLAY && !ov) {
      int in = (k & left ? I_LEFT : 0) | (k & right ? I_RIGHT : 0) | (k & (up | ok) ? I_UP : 0) | (k & down ? I_DOWN : 0);
      for (int n = 0; acc >= 50 && n < 4; acc -= 50, n++) tick(in);
      if (acc >= 50) acc = 0; /* (too far behind: let it go) */
    } else {
      acc = 0;
    }
    if (full) {
      eadk_display_wait_for_vblank();
      gfx_render(scr == SC_HUB ? hub_scene : play_scene, 0, 0, SCREEN_H);
      full = 0;
    } else {
      eadk_timing_msleep(8);
    }
  }
  if (scr == SC_PLAY) sv.room = (uint8_t)rnum;
  save();
  gfx_buf = their_strip;
  return np_app_end();
}
