/* Level Devil: run to the door. The level has other plans.
 *
 * Unept's troll platformer (2023), its first world: sixteen doors of five short rooms in flat
 * colours, with its tiny black runner. Floors drop away under you, spikes pop up in front of the
 * door, walls slide out, the door runs off, the controls turn round, gravity flips... Every trap
 * waits for the runner to reach its spot, so a room catches you the first time and is easy once
 * you know it; some rooms change again after you die in them.
 *
 * A room is a list of objects (blocks, spikes, saws, springs, warps, switches and the door), each
 * with an optional trap: what sets it off (the runner crossing a line, touching it, the trap
 * before it, the room starting), after how many deaths it is there, and what it does (slide,
 * loop, chase, fall, appear or vanish). The rooms are written in tools/rooms.py, which packs them
 * (DEFLATE); they are unpacked into RAM when the game starts. The game steps 60 times a second
 * and only the rows that change are drawn again, in the launcher's strips (launcher/src/gfx.c).
 * Inside NumPlay it uses the launcher's shapes, fonts, files and inflater; on its own it brings
 * them along. */
#include <eadk.h>
#include "../../../launcher/src/gfx.h"
#include "../../../launcher/src/inflate.h"
#include "../../common/np_text.h"

#if NP_MODULE /* (np_file_read and np_file_write: NumPlay's, launcher/src/np.h) */
#define np_app_begin()
#define np_app_end() 0
#else
#include "../../common/epsilon_app.h"
#include "../../common/epsilon_files.h"
#define np_file_read ef_read
#define np_file_write ef_write
const char eadk_app_name[] __attribute__((section(".rodata.eadk_app_name"))) = "Level Devil";
const uint32_t eadk_api_level __attribute__((section(".rodata.eadk_api_level"))) = 0;
void np_push(int x, int y, int w, int h, const color_t *px) {
  eadk_display_push_rect((eadk_rect_t){(uint16_t)x, (uint16_t)y, (uint16_t)w, (uint16_t)h}, px);
}
#endif

#define KEY(k) (1ull << (k))
#define G 2    /* screen pixels to a pixel of the game, each way */
#define FW 160 /* the room, in the game's pixels */
#define FH 120
#define TOP 10 /* the ceiling: the band with the level, its rooms and the deaths */
#define WALL 4
#define PW 5   /* the runner's box */
#define PH 7
#define DW 11  /* the door */
#define DH 20
#define ROOMS 5
#define LEVELS 16

/* A room (tools/rooms.py): its modes (R_*), where the runner starts (x, and the y of its feet), how
 * many objects, then the objects. An object: its kind and flags, then x, y, w and h in pixels of the
 * game (a door: h 1 when it hangs from the ceiling; a warp: its two ends, w and h the second one's
 * x and y; a switch or a button: dx the modes it toggles). With F_TRAP, five more bytes, its trap: what
 * sets it off (T_* and a value: the line the runner crosses, how near it comes, the object whose
 * trap goes first), how many deaths in the room it waits for (1-3: there from that death on; 5-7:
 * there until death 1-3), how it moves (M_*), by how much (dx, dy, signed), how fast (speeds[]),
 * and after how many 4ths of 16 steps. */
enum { K_BLOCK, K_SECRET, K_SPIKE_U, K_SPIKE_D, K_SPIKE_L, K_SPIKE_R, K_DOOR, K_SAW, K_SPRING, K_WARP, K_SWITCH, K_BUTTON };
#define F_HIDDEN 0x10 /* not there until its trap goes off */
#define F_VANISH 0x20 /* gone when its trap goes off */
#define F_GHOST 0x40  /* drawn, but nothing to stand on (a fake block), or a fake door */
#define F_TRAP 0x80
enum { T_START, T_RIGHT, T_LEFT, T_UP, T_DOWN, T_TOUCH, T_AFTER, T_NEAR };
enum { M_ONCE, M_LOOP, M_CHASE, M_FALL };
/* the modes of a room, which switches and buttons toggle (their dx) */
#define R_BACK 1  /* left and right swapped */
#define R_ICE 2   /* the floor is slippery */
#define R_AIR 4   /* jump in the air as often as you like */
#define R_FLIP 8  /* gravity upwards */
#define R_WRAP 16 /* no side walls: out one side, in at the other */
#include "rooms.inc" /* rooms_z, ROOM_BYTES */
static uint8_t rooms[ROOM_BYTES];

/* each level's background and ground */
static const color_t colors[LEVELS][2] = {
  {HEX(0xF49F7E), HEX(0x634D40)}, {HEX(0xF68048), HEX(0xB8461E)}, {HEX(0xF2BD48), HEX(0xA9690A)},
  {HEX(0xA5CF55), HEX(0x2E5A45)}, {HEX(0x84C9E6), HEX(0x274F6E)}, {HEX(0xB9A3DE), HEX(0x46356B)},
  {HEX(0x8FD9B6), HEX(0x285C49)}, {HEX(0xF29BB5), HEX(0x7A2E4A)}, {HEX(0xE6CF9C), HEX(0x6B5235)},
  {HEX(0x6CC7C2), HEX(0x1D5052)}, {HEX(0xF7B58B), HEX(0x8A4B2A)}, {HEX(0xA3B5F0), HEX(0x2F3D7A)},
  {HEX(0xCDB2E8), HEX(0x5B2F73)}, {HEX(0xD3ECF4), HEX(0x3A6A82)}, {HEX(0xF07C6C), HEX(0x5E2420)},
  {HEX(0xC8452A), HEX(0x2A0A06)},
};

/* the runner, 256ths of a pixel a step (60 a second) */
#define RUN 280
#define GRAV 56
#define JUMP 770
#define FALL 900
#define SPRING 1180

#define WHITE 0xFFFF
#define BLACK 0
#define GREY HEX(0xC8C8C8)
#define HUB_BG HEX(0xF2BD48)
#define HUB_FG HEX(0xC47A0A)
#define RED HEX(0xD9512C)
#define DARK_RED HEX(0x6E1A0E)

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

typedef struct {
  const uint8_t *r;     /* its record */
  int16_t x, y, v;      /* where it is, in 16ths of a pixel; its speed falling */
  uint8_t i, st, t, on; /* its place in the room; its trap's state and delay left; there or not */
} obj_t;
enum { S_WAIT, S_DELAY, S_GO, S_DONE };
static obj_t ob[28];
static int no;

static color_t strip[SCREEN_W * STRIP_H];
static const uint8_t *room;
static int level, rnum, tries, mode, scr, ov, sel, banner, dlo, dhi, full;
static int32_t px, py, vx, vy; /* the runner: its box's top-left and its speed, in 256ths of a pixel */
static int face = 1, ground, coyote, buffer, held_in, anim, dead, won, warped, curtain, sprung;
static uint32_t found, seed, banner_t, now;
static const obj_t *skip_ob; /* left out of collisions (a block pushing the runner) */
static color_t bg, fg;
static struct {
  int16_t x, y;
  int8_t vx, vy;
} bits[14]; /* the runner, in pieces */
enum { SC_HUB, SC_PLAY };
enum { OV_NONE, OV_PAUSE, OV_HELP, OV_QUIT, OV_END };
enum { I_LEFT = 1, I_RIGHT = 2, I_JUMP = 4 };

static uint32_t rnd(void) {
  seed ^= seed << 13;
  seed ^= seed >> 17;
  return seed ^= seed << 5;
}
static int imin(int a, int b) { return a < b ? a : b; }
static int imax(int a, int b) { return a > b ? a : b; }
static int clampi(int v, int a, int b) { return v < a ? a : v > b ? b : v; }
static char *cat(char *o, const char *s) { /* s at o; returns its end */
  while (*s) *o++ = *s++;
  *o = 0;
  return o;
}
static char *num(char *o, uint32_t v) { /* v at o; returns its end */
  char t[10];
  int n = 0;
  do t[n++] = (char)('0' + v % 10); while (v /= 10);
  while (n) *o++ = t[--n];
  *o = 0;
  return o;
}
static char *label(char *o, const char *s, uint32_t v) { /* a text, then a number; returns o */
  num(cat(o, s), v);
  return o;
}
static void save(void) {
  sv.magic = 'L', sv.version = 1;
  np_file_write(SAVE_NAME, &sv, sizeof sv);
}

/* ------------------------------------------------------------ the room */
static const uint8_t *next_room(const uint8_t *p) {
  int n = p[3];
  for (p += 4; n--;) p += *p & F_TRAP ? 10 : 5;
  return p;
}
/* the box of an object, in pixels */
static void box(const obj_t *o, int *b) {
  const uint8_t *r = o->r;
  int k = r[0] & 15;
  b[0] = o->x >> 4, b[1] = o->y >> 4, b[2] = r[3], b[3] = r[4];
  if (k == K_DOOR) b[2] = DW, b[3] = DH;
  if (k == K_WARP) b[2] = 8, b[3] = 12;
}
static bool overlap(int x, int y, int w, int h, const int *b) {
  return x < b[0] + b[2] && b[0] < x + w && y < b[1] + b[3] && b[1] < y + h;
}
static bool solid(const obj_t *o) {
  int k = o->r[0] & 15;
  return o->on && (k == K_SECRET || (k == K_BLOCK && !(o->r[0] & F_GHOST)));
}
/* What the runner's box would hit at (x, y): a solid object, the walls (ob itself), or nothing. */
static const obj_t *blocked(int x, int y) {
  int b[4];
  if (y < TOP || (!(mode & R_WRAP) && (x < WALL || x + PW > FW - WALL))) return ob;
  for (int k = 0; k < no; k++) {
    obj_t *o = &ob[k];
    if (o == skip_ob || !solid(o)) continue;
    box(o, b);
    if (overlap(x, y, PW, PH, b)) {
      found |= 1u << o->i; /* a secret block, found by bumping into it */
      return o;
    }
  }
  return 0;
}
/* Moves the runner along one axis, a pixel at a time; true when something stopped it. */
static bool move(int32_t *p, int32_t d, int axis) {
  int a = *p >> 8, b = (*p + d) >> 8, s = b > a ? 1 : -1;
  for (; a != b; a += s)
    if (axis ? blocked(px >> 8, a + s) : blocked(a + s, py >> 8)) {
      *p = a * 256 + (s > 0 ? 255 : 0);
      return true;
    }
  *p += d;
  return false;
}
/* the runner's box, a pixel bigger all round, against an object */
static bool touching(const obj_t *o) {
  int b[4];
  box(o, b);
  return overlap((px >> 8) - 1, (py >> 8) - 1, PW + 2, PH + 2, b);
}

static void mark(int y, int h) { /* rows y to y + h (game pixels) to draw again */
  dlo = imin(dlo, clampi(y * G - 2, 0, SCREEN_H)), dhi = imax(dhi, clampi((y + h) * G + 2, 0, SCREEN_H));
}

static void reset(void) { /* the room as it starts, the runner at the start */
  const uint8_t *p = room;
  mode = p[0], px = p[1] * 256, py = (p[2] - PH) * 256, vx = vy = 0;
  ground = coyote = buffer = dead = won = warped = sprung = 0, face = 1;
  int n = p[3];
  no = 0, p += 4;
  for (int i = 0; i < n; i++) {
    const uint8_t *r = p;
    p += *r & F_TRAP ? 10 : 5;
    if (*r & F_TRAP) { /* there only after (or before) so many deaths */
      int d = r[5] >> 3 & 7;
      if ((d && d < 4 && tries < d) || (d > 4 && tries >= d - 4)) continue;
    }
    obj_t *o = &ob[no++];
    o->r = r, o->i = (uint8_t)i, o->x = (int16_t)(r[1] << 4), o->y = (int16_t)(r[2] << 4), o->v = 0;
    o->st = *r & F_TRAP ? S_WAIT : S_DONE, o->on = !(*r & F_HIDDEN), o->t = 0;
  }
  ground = blocked(px >> 8, (py >> 8) + 1) != 0, full = 1;
}
static void enter(void) { /* the room rnum of the level */
  room = rooms;
  for (int i = level * ROOMS + rnum; i--;) room = next_room(room);
  bg = colors[level][0], fg = colors[level][1];
  found = 0, tries = 0;
  reset();
}

static void die(void) {
  dead = 1;
  sv.deaths++, sv.run++;
  for (int i = 0; i < 14; i++) {
    bits[i].x = (int16_t)((px >> 4) + 32), bits[i].y = (int16_t)((py >> 4) + 48);
    bits[i].vx = (int8_t)((int)(rnd() % 41) - 20), bits[i].vy = (int8_t)(-(int)(rnd() % 36) - 6);
  }
  save();
}

/* has the trap of o gone off? */
static bool fired(const obj_t *o) {
  const uint8_t *r = o->r;
  int v = r[6], x = px >> 8, y = py >> 8, c;
  switch (r[5] & 7) {
    case T_START: return true;
    case T_RIGHT: return x + PW > v;
    case T_LEFT: return x < v;
    case T_UP: return y < v;
    case T_DOWN: return y + PH > v;
    case T_TOUCH: return touching(o);
    case T_AFTER:
      for (int k = 0; k < no; k++)
        if (ob[k].i == v) return ob[k].st >= S_GO;
      return false;
    default: /* T_NEAR */
      c = (o->x >> 4) + r[3] / 2 - x - PW / 2;
      return c < v && c > -v;
  }
}

/* Moves o to (x, y) (16ths), carrying the runner standing on it and pushing it when it runs
   into it; the runner crushed against something else dies. */
static void shift(obj_t *o, int x, int y) {
  int b[4], g = mode & R_FLIP ? -1 : 1, rx = px >> 8, ry = py >> 8;
  box(o, b);
  int ox = b[0], oy = b[1];
  bool on_it = solid(o) && (o->r[5] >> 6) != M_FALL && rx < b[0] + b[2] && b[0] < rx + PW &&
               (g > 0 ? ry + PH == b[1] : ry == b[1] + b[3]);
  mark(b[1], b[3]);
  o->x = (int16_t)x, o->y = (int16_t)y;
  box(o, b);
  mark(b[1], b[3]);
  int dx = b[0] - ox, dy = b[1] - oy;
  if ((!dx && !dy) || dead) return;
  skip_ob = o;
  if (on_it) move(&px, dx * 256, 0), move(&py, dy * 256, 1);
  if (solid(o) && overlap(px >> 8, py >> 8, PW, PH, b)) {
    if (dx) move(&px, (dx > 0 ? b[0] + b[2] - (px >> 8) : b[0] - PW - (px >> 8)) * 256, 0);
    if (dy) move(&py, (dy > 0 ? b[1] + b[3] - (py >> 8) : b[1] - PH - (py >> 8)) * 256, 1);
    if (overlap(px >> 8, py >> 8, PW, PH, b)) die();
  }
  skip_ob = 0;
}

static const uint8_t speeds[16] = {0, 2, 4, 6, 8, 12, 16, 20, 24, 32, 40, 48, 64, 80, 96, 128};
static void traps(void) {
  for (int k = 0; k < no; k++) {
    obj_t *o = &ob[k];
    const uint8_t *r = o->r;
    int kind = r[0] & 15, b[4];
    if (kind == K_SAW || o->t > 200) mark((o->y >> 4) - 1, r[4] + 2); /* turning, springing */
    if (kind == K_SPRING && o->t > 200 && ++o->t == 0) o->t = 0;
    if (kind == K_BUTTON && o->st == S_DONE && !touching(o)) o->st = S_WAIT, mark(o->y >> 4, r[4]);
    if (o->st == S_WAIT && !dead && !won && fired(o)) o->st = S_DELAY, o->t = (uint8_t)((r[9] >> 4) * 4);
    if (o->st == S_DELAY && !(o->t && o->t--)) {
      o->st = S_GO;
      box(o, b);
      mark(b[1], b[3]);
      if (*r & F_HIDDEN) o->on = 1;
      if (*r & F_VANISH) o->on = 0, o->st = S_DONE;
      if (kind == K_SWITCH || kind == K_BUTTON) mode ^= r[7], o->st = S_DONE, full = 1;
      if (o->on && solid(o) && overlap(px >> 8, py >> 8, PW, PH, b)) die(); /* a block appears on the runner */
    }
    if (o->st != S_GO) continue;
    int m = r[5] >> 6, s = speeds[r[9] & 15], tx = (r[1] + (int8_t)r[7]) * 16, ty = (r[2] + (int8_t)r[8]) * 16;
    if (!s) s = 4096;
    if (m == M_FALL) {
      o->v = (int16_t)imin(o->v + 4, 80);
      shift(o, o->x, o->y + o->v);
      if (o->y >> 4 > FH) o->on = 0, o->st = S_DONE;
      continue;
    }
    if (m == M_CHASE) /* towards the runner, along the axes it moves on */
      tx = r[7] ? (int)(px >> 4) + PW * 8 - r[3] * 8 : o->x, ty = r[8] ? (int)(py >> 4) + PH * 8 - r[4] * 8 : o->y;
    if (m == M_LOOP && o->v) tx = r[1] << 4, ty = r[2] << 4; /* on the way back */
    shift(o, o->x + clampi(tx - o->x, -s, s), o->y + clampi(ty - o->y, -s, s));
    if (o->x == tx && o->y == ty && m != M_CHASE) {
      if (m == M_LOOP) o->v = !o->v;
      else o->st = S_DONE;
    }
  }
}

/* what the runner touches: springs, warps, the door, what kills */
static void contacts(void) {
  int x = px >> 8, y = py >> 8, b[4], g = mode & R_FLIP ? -1 : 1, in_warp = 0;
  if (y > FH + 2 || y < -PH - 2) die();
  for (int k = 0; k < no && !dead; k++) {
    obj_t *o = &ob[k];
    int kind = o->r[0] & 15;
    if (!o->on) continue;
    box(o, b);
    if (kind >= K_SPIKE_U && kind <= K_SPIKE_R) { /* the points, a little inside the teeth */
      int u = kind == K_SPIKE_U, d = kind == K_SPIKE_D, l = kind == K_SPIKE_L, rr = kind == K_SPIKE_R;
      int hb[4] = {b[0] + (u || d ? 1 : 2 * l), b[1] + (l || rr ? 1 : 2 * u), b[2] - (u || d ? 2 : 2),
                   b[3] - (l || rr ? 2 : 2)};
      if (overlap(x, y, PW, PH, hb)) die();
    } else if (kind == K_SAW) { /* the nearest point of the box to the middle of the saw */
      int c = b[2] / 2, cx = b[0] + c, cy = b[1] + c, dx = cx - clampi(cx, x, x + PW - 1), dy = cy - clampi(cy, y, y + PH - 1);
      if (dx * dx + dy * dy < (c - 1) * (c - 1)) die();
    } else if (kind == K_SPRING) {
      if (overlap(x, y + PH - 1, PW, 2, b) && vy * g >= 0) vy = -g * SPRING, o->t = 236, ground = 0, sprung = 1;
    } else if (kind == K_WARP) {
      int e[4] = {o->r[3], o->r[4], 8, 12}, a = overlap(x, y, PW, PH, b), z = overlap(x, y, PW, PH, e);
      if ((a || z) && !warped) {
        mark(y, PH);
        px = ((z ? b[0] : e[0]) + 2) * 256, py = ((z ? b[1] : e[1]) + 12 - PH) * 256;
        mark(py >> 8, PH);
      }
      in_warp |= a | z;
    } else if (kind == K_DOOR && !(o->r[0] & F_GHOST)) {
      int in[4] = {b[0] + 3, b[1] + 4, DW - 6, DH - 4};
      if (overlap(x, y, PW, PH, in)) won = 1, mark(b[1], DH);
    }
  }
  warped = in_warp;
}

static void step(int in) {
  anim++;
  if (dead) {
    for (int i = 0; i < 14; i++) {
      mark(bits[i].y >> 4, 2);
      bits[i].x += bits[i].vx, bits[i].y += bits[i].vy, bits[i].vy = (int8_t)imin(bits[i].vy + 3, 60);
      mark(bits[i].y >> 4, 2);
    }
    if (++dead > 42) tries++, reset();
    return;
  }
  if (won) return;
  int g = mode & R_FLIP ? -1 : 1;
  mark((py >> 8) - 1, PH + 2);
  traps();
  if (dead) return;
  int dir = !!(in & I_RIGHT) - !!(in & I_LEFT);
  if (mode & R_BACK) dir = -dir;
  if (in & I_JUMP & ~held_in) buffer = 7;
  held_in = in;
  int ice = mode & R_ICE && ground, acc = ground ? (ice ? 5 : 60) : 40, dec = ground ? (ice ? 3 : 90) : 20;
  if (dir) vx = clampi(vx + dir * acc, -RUN, RUN), face = dir;
  else vx = vx > dec ? vx - dec : vx < -dec ? vx + dec : 0;
  if (buffer && (buffer--, coyote || mode & R_AIR)) vy = -g * JUMP, buffer = coyote = 0;
  sprung &= vy * g < 0; /* (a spring's lift isn't cut short like a jump let go) */
  vy += g * (vy * g < 0 && !(in & I_JUMP) && !sprung ? 3 * GRAV : GRAV);
  if (vy * g > FALL) vy = g * FALL;
  if (move(&px, vx, 0)) vx = 0;
  if (move(&py, vy, 1)) vy = 0;
  if (mode & R_WRAP) px = (px + (FW << 8) + (PW << 7)) % (FW << 8) - (PW << 7); /* out one side, in at the other */
  ground = blocked(px >> 8, (py >> 8) + g) != 0;
  coyote = ground ? 6 : coyote ? coyote - 1 : 0;
  contacts();
  mark((py >> 8) - 1, PH + 2);
}

/* ------------------------------------------------------------ drawing */
static void fillg(int x, int y, int w, int h, color_t c) { gfx_fill(x * G, y * G, w * G, h * G, c); }

/* a picture of w x n pixels (a byte a row, its first pixel the top bit), s screen pixels each,
   turned round left to right and upside down as asked */
static void bitmap(const uint8_t *rows, int n, int w, int x, int y, int s, color_t c, int fx, int fy) {
  for (int j = 0; j < n; j++)
    for (int i = 0, v = rows[fy ? n - 1 - j : j]; i < w; i++)
      if (v >> (7 - (fx ? w - 1 - i : i)) & 1) gfx_fill(x + i * s, y + j * s, s, s, c);
}
/* the runner: standing, two steps running, jumping (6 x 7) */
static const uint8_t runner[4][7] = {
  {0x30, 0x30, 0x78, 0x78, 0x78, 0x48, 0x48},
  {0x30, 0x30, 0x78, 0x78, 0xF8, 0x8C, 0x04},
  {0x30, 0x30, 0x78, 0x78, 0x78, 0x30, 0x30},
  {0x18, 0x18, 0x78, 0x7C, 0x78, 0xCC, 0x80},
};
static const uint8_t skull[5] = {0x70, 0xF8, 0xA8, 0xF8, 0x50};
static const uint8_t horns[3] = {0x81, 0xC3, 0x66};

static void draw_runner(int x, int y, int f, int flip) {
  bitmap(runner[f], 7, 6, x - (face < 0) * G, y, G, BLACK, face < 0, flip);
}

/* a door, s screen pixels a pixel of the game; its window in w */
static void door(int x, int y, int s, color_t frame, color_t w, int up) {
  gfx_fill(x + s, y, 9 * s, s, frame);
  gfx_fill(x, y + s, 11 * s, 19 * s, frame);
  if (up) y += 10 * s; /* (hanging from the ceiling: the window at the bottom) */
  gfx_fill(x + 2 * s, y + s, 7 * s, 9 * s, w);
  gfx_fill(x + s, y + 2 * s, 9 * s, 7 * s, w);
}

static void spikes(int kind, int x, int y, int w, int h) {
  int n = (kind < K_SPIKE_L ? w : h) / 5;
  for (int t = 0; t < n; t++)
    for (int j = 0; j < 3; j++) { /* the three steps of a tooth, from its point */
      int a = 2 - j, l = 1 + 2 * j, d = j ? 2 * j - 1 : 0, e = j ? 2 : 1, u = t * 5 + a;
      if (kind == K_SPIKE_U) fillg(x + u, y + d, l, e, fg);
      if (kind == K_SPIKE_D) fillg(x + u, y + h - d - e, l, e, fg);
      if (kind == K_SPIKE_L) fillg(x + d, y + u, e, l, fg);
      if (kind == K_SPIKE_R) fillg(x + w - d - e, y + u, e, l, fg);
    }
}

static void draw_object(const obj_t *o, int blocks) {
  const uint8_t *r = o->r;
  int kind = r[0] & 15, b[4];
  if (!o->on || (kind == K_BLOCK || kind == K_SECRET) != blocks) return;
  box(o, b);
  if (b[1] * G >= gfx_y1 || (b[1] + b[3]) * G <= gfx_y0) {
    if (kind != K_WARP) return;
  }
  int x = b[0], y = b[1], w = b[2], h = b[3];
  if (kind == K_BLOCK) fillg(x, y, w, h, fg);
  else if (kind == K_SECRET) {
    if (found >> o->i & 1) fillg(x, y, w, h, fg);
  } else if (kind <= K_SPIKE_R) spikes(kind, x, y, w, h);
  else if (kind == K_DOOR) door(x * G, y * G, G, fg, won && !(r[0] & F_GHOST) ? fg : GREY, r[4] == 1);
  else if (kind == K_SAW) { /* a disc with eight teeth, turning */
    int c = w / 2, a = anim >> 1;
    gfx_circle((x + c) * G, (y + c) * G, c * G - 2, fg, 256);
    for (int t = 0; t < 8; t++) {
      static const int8_t sx[8] = {0, 7, 10, 7, 0, -7, -10, -7};
      int q = (t + a) & 7, z = c * G - 1;
      gfx_fill((x + c) * G + sx[q] * z / 10 - 2, (y + c) * G + sx[(q + 6) & 7] * z / 10 - 2, 4, 4, fg);
    }
    gfx_circle((x + c) * G, (y + c) * G, 3, bg, 256);
  } else if (kind == K_SPRING) {
    int up = o->t > 200 && o->t < 248 ? 1 : 2;
    fillg(x, y + 2, w, 1, fg);
    fillg(x + 1, y + 2 - up, 1, up, fg), fillg(x + w - 2, y + 2 - up, 1, up, fg);
    fillg(x, y + 1 - up, w, 1, fg);
  } else if (kind == K_WARP) { /* both of its ends: rings */
    for (int e = 0; e < 2; e++) {
      int ex = (e ? r[3] : x) * G + 8, ey = (e ? r[4] : y) * G + 12;
      gfx_rrect(ex - 8, ey - 12, 16, 24, 8, WHITE, 256);
      gfx_rrect(ex - 5, ey - 9, 10, 18, 5, fg, 256);
    }
  } else if (kind == K_BUTTON) { /* (a ghost one: on a ceiling) */
    int down = touching(o), up = r[0] & F_GHOST;
    fillg(x + 1, y + (up ? 0 : down), w - 2, h - down, RED);
    fillg(x, up ? y : y + h - 1, w, 1, fg);
  }
}

static void say(const np_font_t *f, int cx, int base, const char *s, color_t c, color_t shade) {
  gfx_text_center(f, cx + 1, base + 2, s, shade, 256);
  gfx_text_center(f, cx, base, s, c, 256);
}
static void button(int y, const char *s, bool on, color_t a, color_t b) {
  if (on) gfx_rrect(96, y, 128, 24, 12, a, 256);
  gfx_text_center(&np_font_body, 160, y + 17, s, on ? b : a, 256);
}

static const char *const pause_items[] = {T("Resume"), T("Restart"), T("Levels"), T("How to play"), T("Quit game")};
static void overlay(void) {
  color_t a = scr == SC_HUB ? HUB_BG : bg, b = scr == SC_HUB ? HUB_FG : fg;
  char s[24];
  gfx_fill_alpha(0, 0, SCREEN_W, SCREEN_H, BLACK, 100);
  if (ov == OV_PAUSE) {
    gfx_rrect(72, 28, 176, 190, 12, b, 256);
    gfx_text_center(&np_font_title, 160, 60, T("Paused"), a, 256);
    for (int i = 0; i < 5; i++) button(72 + 28 * i, pause_items[i], sel == i, a, b);
  } else if (ov == OV_QUIT) {
    gfx_rrect(72, 76, 176, 96, 12, b, 256);
    gfx_text_center(&np_font_title, 160, 110, T("Quit game?"), a, 256);
    for (int i = 0; i < 2; i++) {
      if (sel == i) gfx_rrect(92 + 72 * i, 128, 64, 24, 12, a, 256);
      gfx_text_center(&np_font_body, 124 + 72 * i, 145, i ? T("Yes") : T("No"), sel == i ? b : a, 256);
    }
  } else if (ov == OV_HELP) {
    static const char *const keys[][2] = {{T("Left Right"), T("run")}, {T("Up, OK, EXE"), T("jump (hold: higher)")},
                                          {T("Back"), T("pause")}};
    gfx_rrect(40, 30, 240, 180, 12, b, 256);
    gfx_text_center(&np_font_title, 160, 62, T("How to play"), a, 256);
    gfx_paragraph(&np_font_body, 160, 86, 208, 15, T("Reach the door. The level has other plans: trust nothing."), a,
                  256);
    for (int i = 0; i < 3; i++) {
      gfx_text_right(&np_font_body, 136, 136 + 18 * i, keys[i][0], WHITE, 256);
      gfx_text(&np_font_body, 146, 136 + 18 * i, keys[i][1], a, 256);
    }
    button(176, T("Play"), true, a, b);
  } else { /* OV_END: the Devil beaten */
    gfx_rrect(40, 40, 240, 160, 12, RED, 256);
    say(&np_font_title, 160, 82, T("You beat the Devil!"), WHITE, DARK_RED);
    gfx_paragraph(&np_font_body, 160, 108, 200, 15, T("Every door of Level Devil is open behind you."), WHITE, 256);
    gfx_text_center(&np_font_body, 160, 152, label(s, T("Deaths in all: "), sv.deaths), WHITE, 256);
    button(164, T("OK"), true, WHITE, RED);
  }
}

static void hud(void) {
  char s[16];
  fillg(0, 0, FW, TOP, fg);
  gfx_text(&np_font_small, 10, 14, label(s, T("Level "), (uint32_t)level + 1), bg, 256);
  for (int i = 0; i < ROOMS; i++) /* the rooms of the level: done, this one, still to come */
    gfx_circle(136 + 12 * i, 10, i == rnum ? 4 : 3, i <= rnum ? bg : gfx_mix(fg, bg, 12), 256);
  num(s, sv.run);
  gfx_text_right(&np_font_small, 310, 14, s, bg, 256);
  bitmap(skull, 5, 5, 298 - gfx_text_width(&np_font_small, s), 5, 2, bg, 0, 0);
}

static void play_scene(void *ctx) {
  fillg(0, TOP, FW, FH - TOP, bg);
  for (int pass = 0; pass < 2; pass++)
    for (int k = 0; k < no; k++) draw_object(&ob[k], pass);
  if (!(mode & R_WRAP)) fillg(0, 0, WALL, FH, fg), fillg(FW - WALL, 0, WALL, FH, fg);
  if (dead)
    for (int i = 0; i < 14; i++) gfx_fill((bits[i].x >> 4) * G, (bits[i].y >> 4) * G, G, G, BLACK);
  else if (!won) {
    int x = (px >> 8) * G, y = (py >> 8) * G, f = !ground ? 3 : vx ? 1 + (anim >> 3 & 1) : 0, flip = mode & R_FLIP;
    draw_runner(x, y, f, flip);
    if (mode & R_WRAP) draw_runner(x + (x < FW ? FW * G : -FW * G), y, f, flip);
  }
  hud();
  if (curtain) fillg(0, TOP, FW, (FH - TOP) * curtain / 16, fg);
  if (ov) overlay();
}


/* the doors to the levels: two rows of eight, on two floors */
static void hub_door(int i, int *x, int *y) { *x = 13 + 38 * (i & 7), *y = 70 + 80 * (i >> 3); }
static void hub_scene(void *ctx) {
  char s[48];
  int sel_ = sv.sel, best = sv.best[sel_];
  gfx_fill(0, 0, SCREEN_W, SCREEN_H, HUB_BG);
  gfx_fill(0, 0, SCREEN_W, 44, RED);
  if (banner) say(&np_font_title, 160, 32, label(s, T("Level cleared! Deaths: "), (uint32_t)banner - 1), WHITE, DARK_RED);
  else say(&np_font_title, 160, 32, "LEVEL DEVIL", DARK_RED, HEX(0xA33219));
  for (int i = 0; i < LEVELS; i++) {
    int x, y, open = i < sv.open;
    hub_door(i, &x, &y);
    if (!(i & 7)) gfx_fill(0, y + 40, SCREEN_W, 8, HUB_FG);
    color_t frame = i == 15 ? RED : open ? HUB_FG : gfx_mix(HUB_FG, HUB_BG, 18);
    if (i == 15) bitmap(horns, 3, 8, x + 3, y - 5, 2, RED, 0, 0);
    door(x, y, 2, frame, sv.done >> i & 1 ? colors[i][0] : open ? GREY : frame, 0);
    num(s, (uint32_t)i + 1);
    gfx_text_center(&np_font_small, x + 11, y - 7 - 5 * (i == 15), s, frame, 256);
    if (i == sel_) draw_runner(x - 12, y + 26, 0, 0);
  }
  gfx_fill(0, 214, SCREEN_W, 26, HUB_FG);
  if (sel_ >= sv.open) cat(s, T("Locked"));
  else if (sv.done >> sel_ & 1) label(s, T("Cleared. Fewest deaths: "), best);
  else cat(s, T("Not cleared yet"));
  gfx_text(&np_font_body, 8, 232, s, HUB_BG, 256);
  gfx_text_right(&np_font_body, 312, 232, label(s, T("Deaths "), sv.deaths), HUB_BG, 256);
  if (ov) overlay();
}

/* ------------------------------------------------------------ the levels */
static void open_room(void) { /* the room rnum, behind a curtain that lifts */
  enter();
  curtain = 16;
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

/* a step of the room: the curtain, the door reached, or the runner */
static void tick(int in) {
  if (curtain && !won) {
    curtain--, full = 1;
    return;
  }
  if (won && ++won > 10) curtain = imin(won - 10, 16), full = 1;
  if (won > 28) {
    won = 0;
    if (++rnum == ROOMS) level_done();
    else sv.room = (uint8_t)rnum, save(), open_room();
    return;
  }
  step(in);
}

static void load(void) {
  uint32_t len = 0;
  const uint8_t *d = np_file_read(SAVE_NAME, &len);
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
  np_inflate(rooms, ROOM_BYTES, rooms_z, sizeof rooms_z, (np_inflate_tables_t *)(void *)strip);
  load();
  if (!(sv.flags & 1)) ov = OV_HELP;
  full = 1, dlo = SCREEN_H;
  const uint64_t left = KEY(eadk_key_left) | KEY(eadk_key_four), right = KEY(eadk_key_right) | KEY(eadk_key_six);
  const uint64_t up = KEY(eadk_key_up) | KEY(eadk_key_eight), down = KEY(eadk_key_down) | KEY(eadk_key_two);
  const uint64_t ok = KEY(eadk_key_ok) | KEY(eadk_key_exe) | KEY(eadk_key_five), back = KEY(eadk_key_back);
  uint64_t held = eadk_keyboard_scan(), stale = held; /* keys still down from the launcher wait to be let go */
  uint32_t last = now, acc = 0;
  for (;;) {
    now = (uint32_t)eadk_timing_millis();
    uint64_t k = eadk_keyboard_scan(), hit = k & ~held;
    stale &= k, held = k, k &= ~stale;
    if (k & (KEY(eadk_key_home) | KEY(eadk_key_on_off))) break;
    int dx = !!(hit & right) - !!(hit & left), dy = !!(hit & down) - !!(hit & up), quit = 0;
    if (hit) full |= scr == SC_HUB || ov;
    if (ov) {
      if (ov == OV_PAUSE) sel = (sel + dy + 5) % 5;
      if (ov == OV_QUIT) sel ^= dx != 0;
      if (hit & ok) {
        if (ov == OV_PAUSE && sel == 1) { /* Restart: the runner bursts, as when it dies */
          ov = OV_NONE;
          if (!dead && !won) die();
        }
        else if (ov == OV_PAUSE && sel) ov = sel == 2 ? (scr = SC_HUB, save(), OV_NONE) : sel == 3 ? OV_HELP : OV_QUIT, sel = 0;
        else if (ov == OV_QUIT && sel) quit = 1;
        else ov = ov == OV_QUIT && scr == SC_PLAY ? (sel = 4, OV_PAUSE) : OV_NONE;
        if (!(sv.flags & 1)) sv.flags |= 1, save();
      } else if (hit & back) {
        ov = ov == OV_QUIT && scr == SC_PLAY ? (sel = 4, OV_PAUSE) : OV_NONE;
      }
    } else if (scr == SC_HUB) {
      sv.sel = (uint16_t)clampi(sv.sel + dx + 8 * dy, 0, LEVELS - 1);
      if (hit & ok && sv.sel < sv.open) start_level(sv.sel);
      if (hit & back) ov = OV_QUIT, sel = 0;
    } else if (hit & back) {
      ov = OV_PAUSE, sel = 0, full = 1;
      save();
    }
    if (quit) break;
    if (banner && now - banner_t > 2600) banner = 0, full = 1;
    acc += (now - last) * 3, last = now;
    if (scr == SC_PLAY && !ov) {
      int in = (k & left ? I_LEFT : 0) | (k & right ? I_RIGHT : 0) | (k & (up | ok) ? I_JUMP : 0);
      for (int n = 0; acc >= 50 && n < 4; acc -= 50, n++) tick(in);
      if (acc >= 50) acc = 0; /* (too far behind: let it go) */
    } else {
      acc = 0;
    }
    if (full || dlo < dhi) {
      eadk_display_wait_for_vblank();
      if (full) dlo = 0, dhi = SCREEN_H;
      gfx_render(scr == SC_HUB ? hub_scene : play_scene, 0, dlo & ~1, dhi);
      full = 0, dlo = SCREEN_H, dhi = 0;
    } else {
      eadk_timing_msleep(8);
    }
  }
  if (scr == SC_PLAY) sv.room = (uint8_t)rnum;
  save();
  gfx_buf = their_strip;
  return np_app_end();
}
