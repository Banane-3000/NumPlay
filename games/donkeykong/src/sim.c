/* Donkey Kong's program, ported from the arcade's Z80 code (furrykef's dkdasm and tcdev's listing).
 *
 * Every routine below is one of the ROM's, named by its address, and keeps its arithmetic: 8-bit
 * values that wrap, the same tables (rom.h), the same RAM addresses (the arcade's 0x6000-0x6BFF, with
 * its sprite table at 0x6900) and the same tile map (0x7400-0x77FF), which the game reads back to find
 * girders under Mario, barrels and fires. dk_tick() is one vblank: the game routine of the current
 * GameMode2, then the foreground tasks (score, bonus box), the extra life, the difficulty clock and the
 * oil drum's fire clock. Coins, the attract mode, two players, the name entry and sound are left out. */
#include <string.h>
#include "dk.h"
#include "rom.h"

static dk_state *S;
#define R(a) (S->ram[((uint16_t)(a) - 0x6000) & 0xFFF])
#define V(a) (S->vram[((uint16_t)(a) - 0x7400) & 0x3FF])
#define U8(x) ((uint8_t)(x))

/* the arcade's named variables */
#define GAME_MODE1 0x6005
#define NO_CREDITS 0x6007
#define WAIT_LSB 0x6008
#define WAIT_MSB 0x6009
#define GAME_MODE2 0x600A
#define INPUT 0x6010   /* the joystick, with bit 7 for a jump just pressed */
#define RAW 0x6011     /* IN0 as read */
#define RNG1 0x6018
#define RNG2 0x6019
#define FRAME 0x601A   /* counts down once a frame */
#define START_LIVES 0x6020
#define EXTRA_AT 0x6021
#define SCREEN 0x6227  /* 1 25m, 2 50m, 3 75m, 4 100m */
#define LIVES 0x6228
#define LEVEL 0x6229
#define DIFFICULTY 0x6380
#define MARIO_ALIVE 0x6200
#define MARIO_X 0x6203
#define MARIO_Y 0x6205

/* ------------------------------------------------------------------ the Z80 helpers */

static uint8_t rng(void) { /* 0x0057 */
  uint8_t a = U8(R(RNG1) + R(FRAME) + R(RNG2));
  R(RNG1) = a;
  return a;
}

static bool rst18(void) { return --R(WAIT_MSB) == 0; } /* RST #18: true when the wait is over */

static bool screen_in(uint8_t mask) { /* RST #30 */
  uint8_t b = R(SCREEN), a = mask, cy = 0;
  do {
    cy = a & 1;
    a = U8(a >> 1 | cy << 7);
  } while (--b);
  return cy;
}

static void add_each(uint16_t hl, int n, uint8_t c, int step) { /* 0x003D: (HL) += C, n times, step apart */
  for (int i = 0; i < n; i++) R(hl + i * step) = U8(R(hl + i * step) + c);
}
static void move_kong(uint16_t hl, uint8_t c) { add_each(hl, 10, c, 4); } /* RST #38 */

static void copy_rom(uint16_t dst, uint16_t src, int n) {
  for (int i = 0; i < n; i++) R(dst + i) = ROM(src + i);
}
static void kong_pose(uint16_t src) { copy_rom(0x6908, src, 0x28); } /* 0x004E */

static void write3(uint16_t hl, uint8_t a, int de) { /* 0x0514 */
  for (int i = 0; i < 3; i++) V(hl + i * de) = U8(a - i);
}

static uint16_t vram_at(uint8_t x, uint8_t y) { /* 0x2FF0: the tile under game point (x, y) */
  return (uint16_t)(0x7400 + ((U8(~x) & 0xF8) << 2) + (y >> 3));
}
#define TILE(x, y) V(vram_at(x, y))

/* ------------------------------------------------------------------ foreground tasks (0x309F, 0x02E3) */

static void task(uint8_t d, uint8_t e) {
  uint8_t l = R(0x60B0);
  if (!(R(0x6000 + l) & 0x80)) return; /* the list is full */
  R(0x6000 + l) = d;
  l++;
  R(0x6000 + l) = e;
  l++;
  if (l < 0xC0) l = 0xC0;
  R(0x60B0) = l;
}

static void show_digits(uint16_t ix, uint16_t hl, int n) { /* 0x057C / 0x0583 */
  for (int b = 0; b < n; b++, hl--) {
    V(ix) = R(hl) >> 4;
    ix -= 0x20;
    V(ix) = R(hl) & 0x0F;
    ix -= 0x20;
  }
}
static void show_score(void) { show_digits(0x7781, 0x60B4, 3); }
static void show_high(void) { show_digits(0x7641, 0x60BA, 3); }

static uint8_t daa_add(uint8_t a, uint8_t b, unsigned *cy) { /* ADC A,b then DAA */
  unsigned r = a + b + *cy, h = (a & 15) + (b & 15) + *cy > 15, c = r > 0xFF, diff = 0;
  r &= 0xFF;
  if (h || (r & 15) > 9) diff |= 6;
  if (c || r > 0x99) diff |= 0x60, c = 1;
  *cy = c;
  return U8(r + diff);
}

static uint8_t daa_sub(uint8_t a, uint8_t b) { /* SUB b then DAA */
  unsigned r = U8(a - b), h = (a & 15) < (b & 15), c = a < b, diff = 0;
  if (h || (r & 15) > 9) diff |= 6;
  if (c || r > 0x99) diff |= 0x60;
  return U8(r - diff);
}

static void add_score(uint8_t p) { /* task 0, 0x051C: p from the table at 0x3529 */
  unsigned cy = 0;
  for (int i = 0; i < 3; i++) R(0x60B2 + i) = daa_add(R(0x60B2 + i), ROM(0x3529 + U8(p * 3) + i), &cy);
  show_score();
  for (int i = 2; i >= 0; i--) { /* a new high score? */
    if (R(0x60B2 + i) < R(0x60B8 + i)) return;
    if (R(0x60B2 + i) > R(0x60B8 + i)) {
      for (int k = 0; k < 3; k++) R(0x60B8 + k) = R(0x60B2 + k);
      show_high();
      return;
    }
  }
}

static void print_text(uint8_t p) { /* task 3, 0x05E9 */
  uint8_t a2 = U8(p * 2);
  bool erase = p & 0x80;
  uint16_t t = ROM(0x364B + (a2 & 0x7F)) | ROM(0x364C + (a2 & 0x7F)) << 8;
  if (t < 0x368A) return; /* (the name entry's rows, in RAM: not used) */
  uint16_t hl = ROM(t) | ROM(t + 1) << 8, de = t + 2;
  for (;;) {
    uint8_t a = ROM(de);
    if (a == 0x3F) return;
    V(hl) = erase ? 0x10 : a;
    de++;
    hl -= 0x20;
  }
}

static void bonus_box(void) { /* 0x064D..0x0690: the box and its digits */
  uint16_t de = 0x7465, src = 0x384A;
  for (int col = 0; col < 6; col++, de += 0x20)
    for (int k = 0; k < 3; k++) V(de + k) = ROM(src++);
  uint8_t t = R(0x638C), b = t & 0x0F, a = t >> 4;
  if (!a) {
    R(0x6089) = 3; /* (the running-out music) */
    V(0x7486) = 0x70;
    V(0x74A6) = 0x70;
    b = U8(0x70 + b);
    a = 0x10;
  }
  V(0x74E6) = a;
  V(0x74C6) = b;
}

static void bonus_task(uint8_t p) { /* task 5, 0x062A */
  if (p == 0) { /* 0x0691: the bonus into the score */
    uint8_t b = R(0x638C);
    add_score(b & 0x0F);
    add_score(U8((b >> 4) + 0x0A));
    return;
  }
  uint8_t t = R(0x638C);
  if (t == 0) {
    if (R(0x63B8)) return;
    uint8_t a = R(0x62B0), n = 0; /* how many tens (0x063D, and its wrap on level 22) */
    do {
      n++;
      a = U8(a - 10);
    } while (a);
    R(0x638C) = U8(n << 4 | n >> 4);
    bonus_box();
    return;
  }
  /* 0x06A8: a hundred less, in BCD */
  if (t == 1) R(0x63B8) = 1;
  R(0x638C) = daa_sub(t, 1);
  bonus_box();
}

static void lives_task(uint8_t p) { /* task 6, 0x06B8: lives (less p) and L= */
  for (int i = 0; i < 6; i++) V(0x7783 - i * 0x20) = 0x10;
  uint8_t n = U8(R(LIVES) - p);
  for (int i = 0; i < n && 0x7783 - i * 0x20 >= 0x7403; i++) V(0x7783 - i * 0x20) = 0xFF;
  V(0x7503) = 0x1C;
  V(0x74E3) = 0x34;
  if (R(LEVEL) >= 100) R(LEVEL) = 99;
  uint8_t a = R(LEVEL), tens = 0;
  while (a >= 10) a -= 10, tens++;
  V(0x74A3) = a;
  V(0x74C3) = tens;
}

static void run_task(uint8_t t, uint8_t p) {
  switch (t & 0x0F) {
  case 0: add_score(p); break;
  case 1: /* clear a score, then show it */
    if (p == 0) R(0x60B2) = R(0x60B3) = R(0x60B4) = 0;
    show_score();
    break;
  case 2:
    if (p == 0 || p == 3) show_score();
    if (p >= 2) show_high();
    break;
  case 3: print_text(p); break;
  case 5: bonus_task(p); break;
  case 6: lives_task(p); break;
  }
}

static void run_tasks(void) { /* 0x02BD */
  for (int guard = 0; guard < 64; guard++) {
    uint8_t l = R(0x60B1), t = R(0x6000 + l);
    if (t & 0x80) return;
    R(0x6000 + l) = 0xFF;
    l++;
    uint8_t p = R(0x6000 + l);
    R(0x6000 + l) = 0xFF;
    l++;
    if (l < 0xC0) l = 0xC0;
    R(0x60B1) = l;
    run_task(t, p);
  }
}

/* ------------------------------------------------------------------ the foreground's own clocks */

static void flash_1up(void) { /* 0x0315 */
  uint8_t b = R(FRAME);
  if (b & 0x0F) return;
  if (b & 0x10) {
    V(0x7740) = V(0x7720) = V(0x7700) = 0x10;
    return;
  }
  V(0x7740) = 1;
  V(0x7720) = 0x25;
  V(0x7700) = 0x20;
}

static void extra_life(void) { /* 0x0350 */
  if (R(0x622D)) return;
  uint8_t a = U8((R(0x60B3) & 0xF0) | (R(0x60B4) & 0x0F));
  a = U8(a >> 4 | a << 4);
  if (a < R(EXTRA_AT)) return;
  R(0x622D) = 1;
  R(LIVES)++;
  lives_task(1);
}

static void difficulty_clock(void) { /* 0x037F */
  uint8_t a = R(0x6384)++;
  if (a) return;
  uint8_t b = R(0x6381)++;
  if (b & 7) return;
  a = U8((U8(b >> 3 | b << 5)) + R(LEVEL));
  if (a >= 5) a = 5;
  R(DIFFICULTY) = a;
}

static void drum_flame(uint8_t b) { /* 0x03F2 */
  R(0x6A29) = b;
  if (R(RNG2) & 1) return;
  R(0x6A29) = U8(b + 1);
}

static void fire_clock(void) { /* 0x03A2: the oil drum's flame and its fires (25m, 50m) */
  if (!screen_in(3) || !(R(MARIO_ALIVE) & 1) || (R(0x6350) & 1)) return;
  if (--R(0x62B8)) return;
  R(0x62B8) = 4;
  uint8_t a = R(0x62B9);
  if (!(a & 1)) return;
  if (!(a & 2)) {
    R(0x66A9) = 2;
    R(0x66AA) = 0;
    drum_flame(0x40);
    R(0x62BA) = 0x10;
    return;
  }
  R(0x66A9) = 2;
  R(0x66AA) = 2;
  drum_flame(0x42);
  if (--R(0x62BA)) return;
  R(0x62B9) = 1;
  R(0x63A0) = 1;
  R(0x62BA) = 0x10;
}

/* ------------------------------------------------------------------ screens: clearing and drawing boards */

static void clear_all(void) { /* 0x0852 */
  memset(S->vram, 0x10, sizeof S->vram);
  memset(&R(0x6900), 0, 0x180);
}

static void clear_play(void) { /* 0x0874: rows 4-31, the middle of rows 2-3, and the sprites */
  for (int col = 0; col < 32; col++)
    for (int row = 4; row < 32; row++) V(0x7400 + col * 32 + row) = 0x10;
  for (int i = 0; i < 14; i++) V(0x7522 + i * 0x20) = V(0x7523 + i * 0x20) = 0x10;
  memset(&R(0x6900), 0, 0x180);
}

static void draw_board(uint16_t de) { /* 0x0DA7 */
  for (;;) {
    uint8_t type = ROM(de);
    R(0x63B3) = type;
    if (type == 0xAA) return;
    uint8_t x1 = ROM(de + 1), y1 = ROM(de + 2), x2 = ROM(de + 3), y2 = ROM(de + 4);
    uint16_t start = vram_at(x1, y1), end = vram_at(x2, y2);
    uint8_t k1 = y1 & 7, k2 = y2 & 7;
    uint8_t dx = U8(x2 - x1);
    if (x2 < x1) dx = U8(-dx); /* (NEG) */
    uint8_t dy = U8(y2 - y1);
    de += 5;
    if (type < 2) { /* a ladder (1: broken) */
      uint8_t len = U8(U8(dy - 0x10) + k1);
      uint16_t hl = start;
      V(hl) = U8(k1 + 0xF0);
      hl++;
      V(hl) = U8(k1 + 0xC0);
      if (type == 1) len = 0;
      for (;;) {
        uint8_t n = U8(len - 8);
        bool borrow = len < 8;
        len = n;
        if (borrow) break;
        hl++;
        V(hl) = 0xC0;
      }
      hl = end;
      V(hl) = U8(k2 + 0xD0);
      if (type == 1) V(hl - 1) = 0xC0;
      if (k2) V(hl + 1) = U8(k2 + 0xE0);
    } else if (type == 2) { /* a girder, right to left: two tiles per pixel of slope */
      uint8_t t = U8(k1 + 0xF0), left = dx;
      uint16_t hl = start;
      for (;;) {
        V(hl) = t; /* 0x0E62 */
        hl++;
        if ((hl & 0x1F) && t != 0xF0) V(hl) = U8(t - 0x10);
        hl += 0x1F;
        if (left < 8) break;
        left = U8(left - 8);
        if (dy == 0) continue;
        V(hl) = t; /* 0x0E8F */
        hl++;
        if (hl & 0x1F) V(hl) = U8(t - 0x10);
        hl += 0x1F;
        if (left < 8) break;
        left = U8(left - 8);
        if (!(dy & 0x80)) {
          t++;
          if (t == 0xF8) {
            hl++;
            t = 0xF0;
          }
          if (!(hl & 0x1F)) break;
        } else {
          t--;
          if ((int8_t)U8(t - 0xF0) < 0) {
            hl--;
            t = 0xF7;
          }
        }
      }
    } else if (type == 3) { /* a conveyor */
      uint16_t hl = start;
      V(hl) = 0xB3;
      hl += 0x20;
      bool borrow = dx < 0x10;
      uint8_t left = U8(dx - 0x10);
      while (!borrow) {
        V(hl) = 0xB1;
        hl += 0x20;
        borrow = left < 8;
        left = U8(left - 8);
      }
      V(hl) = 0xB2;
    } else if (type <= 6) { /* 4: blank (E0), 5: a 100m girder (B0), 6: the 50m cross (FE) */
      uint8_t t = type == 4 ? 0xE0 : type == 5 ? 0xB0 : 0xFE;
      uint16_t hl = start;
      int left = dx;
      for (;;) {
        V(hl) = t;
        hl += 0x20;
        left -= 8;
        if (left < 0) break;
      }
    }
  }
}

/* ------------------------------------------------------------------ a board's start (0x0C92..0x0DA6, 0x0F56..0x123B) */

static void copy4(uint16_t hl, uint16_t de, int b, uint8_t c) { /* 0x122A */
  for (; b > 0; b--) {
    for (int i = 0; i < 4; i++) R((de & 0xFF00) | U8(de + i)) = ROM(hl + i);
    de = (de & 0xFF00) | U8(de + 4 + c);
  }
}

static void place2(uint16_t hl, uint16_t de, int b, uint8_t c) { /* 0x11EC: X and Y from a table */
  for (; b > 0; b--) {
    R(de) = ROM(hl++);
    de = (de & 0xFF00) | U8(de + 2);
    R(de) = ROM(hl++);
    de = (de & 0xFF00) | U8(de + c);
  }
}

static void to_sprites(uint16_t ix, uint16_t hl, int b, int de) { /* 0x11D3 */
  for (; b > 0; b--, ix += de, hl += 4) {
    R(hl) = R(ix + 3);
    R(hl + 1) = R(ix + 7);
    R(hl + 2) = R(ix + 8);
    R(hl + 3) = R(ix + 5);
  }
}

static void hammers(uint16_t hl) { /* 0x11A6 */
  place2(hl, 0x6683, 2, 0x0E);
  copy4(0x3E08, 0x6687, 2, 0x0C);
  R(0x6680) = 1;
  R(0x6690) = 1;
  to_sprites(0x6680, 0x6A18, 2, 0x10);
}

static void drum_fire(uint16_t hl) { /* 0x11FA */
  R(0x66A0) = 1;
  R(0x66A3) = R(0x6A28) = ROM(hl);
  R(0x66A7) = R(0x6A29) = ROM(hl + 1);
  R(0x66A8) = R(0x6A2A) = ROM(hl + 2);
  R(0x66A5) = R(0x6A2B) = ROM(hl + 3);
  R(0x66A9) = ROM(hl + 4);
  R(0x66AA) = ROM(hl + 5);
}

static void springs_init(void) { /* 0x1186 */
  copy4(0x11A2, 0x6507, 10, 0x0C);
  to_sprites(0x6500, 0x6980, 10, 0x10);
}

static void board_ram(void) { /* 0x0F56 */
  memset(&R(0x6200), 0, 0x27);
  memset(&R(0x6280), 0, 0x880);
  copy_rom(0x6280, 0x3D9C, 0x40);
  uint8_t l = R(LEVEL), a = U8(U8(l * 10) + 0x28);
  if (a >= 0x51) a = 0x50;
  R(0x62B0) = R(0x62B1) = R(0x62B2) = a;
  a = U8(0xDC - U8(a * 2));
  if (a < 0x28) a = 0x28;
  R(0x62B3) = R(0x62B4) = a;
  R(0x6209) = 4;
  R(0x620A) = 8;
  uint8_t screen = R(SCREEN);
  if (!(screen & 4)) /* black squares over the girders at the top of Kong's ladders */
    for (int i = 0; i < 3; i++) {
      R(0x6A00 + i * 4) = U8(0x4F + i * 0x10);
      R(0x6A01 + i * 4) = 0x3A;
      R(0x6A02 + i * 4) = 0x0F;
      R(0x6A03 + i * 4) = 0x18;
    }
  switch (screen) {
  case 1: /* 0x0FD7 */
    copy_rom(0x69A8, 0x3DDC, 0x10);
    copy4(0x3DEC, 0x6407, 5, 0x1C);
    drum_fire(0x3DF4);
    copy_rom(0x69FC, 0x3E00, 4);
    hammers(0x3E0C);
    copy4(0x101B, 0x6707, 8, 0x1C);
    copy4(0x101B, 0x6807, 2, 0x1C);
    break;
  case 2: /* 0x101F */
    copy4(0x3DEC, 0x6407, 5, 0x1C);
    springs_init();
    copy4(0x3E18, 0x65A7, 6, 0x0C);
    to_sprites(0x65A0, 0x69B8, 6, 0x10);
    drum_fire(0x3DFA);
    copy_rom(0x69FC, 0x3E04, 4);
    copy_rom(0x6944, 0x3E1C, 8);
    copy_rom(0x69E4, 0x3E24, 0x18);
    hammers(0x3E10);
    copy_rom(0x6A0C, 0x3E3C, 0x0C);
    R(0x62B9) = 1;
    break;
  case 3: /* 0x1087 */
    copy4(0x3DEC, 0x6407, 5, 0x1C);
    springs_init();
    for (int i = 0; i < 6; i++) R(0x6600 + i * 0x10) = 1;
    for (int i = 0; i < 3; i++) R(0x660D + i * 0x10) = 8;
    place2(0x3E64, 0x6603, 6, 0x0E);
    copy4(0x3E60, 0x6607, 6, 0x0C);
    to_sprites(0x6600, 0x6958, 6, 0x10);
    copy_rom(0x6A0C, 0x3E48, 0x0C);
    R(0x6400) = 1, R(0x6403) = R(0x640E) = 0x58, R(0x6405) = R(0x640F) = 0x80;
    R(0x6420) = 1, R(0x6423) = R(0x642E) = 0xEB, R(0x6425) = R(0x642F) = 0x60;
    copy_rom(0x6970, 0x1121, 0x10);
    break;
  case 4: /* 0x1131 */
    copy4(0x3DF0, 0x6407, 5, 0x1C);
    hammers(0x3E14);
    copy_rom(0x6A0C, 0x3E54, 0x0C);
    place2(0x1182, 0x64A3, 2, 0x1E);
    copy4(0x117E, 0x64A7, 2, 0x1C);
    R(0x64A0) = 1;
    R(0x64C0) = 1;
    to_sprites(0x64A0, 0x6950, 2, 0x20);
    break;
  }
}

static void ladder_table(void) { /* 0x2441: ladders at 0x6300 (x), +0x15 (top), +0x2A (bottom) */
  static const uint16_t boards[4] = {0x3AE4, 0x3B5D, 0x3BE5, 0x3C8B};
  uint16_t hl = boards[(R(SCREEN) - 1) & 3], ix = 0x6300, iy = 0x6310;
  for (int guard = 0; guard < 64; guard++) {
    uint8_t a = ROM(hl);
    if (a == 0xAA) return;
    if (a > 1) {
      hl += 5;
      continue;
    }
    uint16_t *p = a == 0 ? &ix : &iy;
    R(*p) = ROM(hl + 1);
    R(*p + 0x15) = ROM(hl + 2);
    R(*p + 0x2A) = ROM(hl + 4);
    (*p)++;
    hl += 5;
  }
}

static void start_board(void) { /* 0x0C92 */
  clear_play();
  R(0x638C) = 0;
  task(5, 1);
  S->palette = 2;
  uint16_t table;
  switch (R(SCREEN)) {
  case 1: table = 0x3AE4, R(0x6089) = 8; break;
  case 2: table = 0x3B5D, S->palette = 1, R(0x6089) = 9; break;
  case 3: /* 0x0D27: the elevator shafts */
    for (int s = 0; s < 2; s++) {
      uint16_t hl = s ? 0x760D : 0x770D;
      for (int i = 0; i < 0x11; i++) V(hl++) = 0xFD;
      hl += 0x0F;
      for (int i = 0; i < 0x11; i++) V(hl++) = 0xFC;
    }
    R(0x6089) = 0x0A;
    table = 0x3BE5;
    break;
  default: /* 0x0D43: the bars beside Kong */
    for (int s = 0; s < 2; s++) {
      uint16_t hl = s ? 0x7547 : 0x7687;
      for (int i = 0; i < 4; i++) V(hl++) = 0xFD;
      hl += 0x1C;
      for (int i = 0; i < 4; i++) V(hl++) = 0xFC;
    }
    S->palette = 3;
    R(0x6089) = 0x0B;
    table = 0x3C8B;
    break;
  }
  draw_board(table);
  if (R(SCREEN) == 4) /* 0x0D00: the rivets */
    for (int i = 0; i < 8; i++) {
      uint16_t de = ROM(0x0D17 + i * 2) | ROM(0x0D18 + i * 2) << 8;
      V(de) = 0xB8;
      V(de + 1) = 0xB7;
    }
  if (R(SCREEN) == 2) /* 0x3FA6: the tops of the moving ladders are sprites */
    for (int i = 0; i < 2; i++) {
      uint16_t hl = i ? 0x748C : 0x776C;
      V(hl) = 0x10;
      V(hl + 2) = 0xC0;
    }
  board_ram(); /* 0x0D5F */
  ladder_table();
  R(WAIT_MSB) = 0x40;
  R(GAME_MODE2)++;
  kong_pose(0x385C);
  copy_rom(0x6900, 0x3884, 8);
  if (R(SCREEN) == 4) { /* 0x0D8B */
    move_kong(0x6908, 0x44);
    add_each(0x6900, 2, 0x10, 4);
    add_each(0x6903, 2, 0xF8, 4);
  } else if (R(SCREEN) == 1) {
    move_kong(0x690B, 0xFC);
  }
}

static void mario_start(void) { /* 0x123C */
  if (!rst18()) return;
  uint8_t x = 0x3F, y = 0xF0;
  if (R(SCREEN) == 3) x = 0x16, y = 0xE0;
  R(MARIO_ALIVE) = 1;
  R(MARIO_X) = R(0x694C) = x;
  R(0x6207) = R(0x694D) = 0x80;
  R(0x6208) = R(0x694E) = 2;
  R(MARIO_Y) = R(0x694F) = y;
  R(0x620F) = 1;
  R(GAME_MODE2)++;
  task(6, 1);
}

/* ------------------------------------------------------------------ the intro: Kong climbs and bends the girders (0x0A76) */

static void kong_climb_step(void) { /* 0x306F */
  uint8_t a = ++R(0x62AF);
  if (a & 7) return;
  move_kong(0x690B, 0xFC);
  for (int k = 0; k < 2; k++) { /* 0x3096 on 0x6909 and 0x691D, with DE = 4 (set by RST #38) */
    uint16_t hl = k ? 0x691D : 0x6909;
    for (int b = 0; b < 2; b++, hl += 4) R(hl) ^= 0x81;
  }
  R(0x692D) ^= rng() & 0x80; /* Pauline wriggles */
}

static void roll_ladder(void) { /* 0x304A: the ladder behind Kong rolls up */
  uint8_t c = R(0x638E);
  for (int i = 0; i < 2; i++) {
    uint16_t hl = (i ? 0x75C0 : 0x7600) + c;
    V(hl - 0x20) = V(hl);
  }
  R(0x638E)--;
}

static void intro(void) { /* 0x0A76 */
  switch (R(0x6385)) {
  case 0: /* 0x0A8A */
    S->palette = 2;
    draw_board(0x380D);
    V(0x76A3) = V(0x7663) = 0x10;
    V(0x75AA) = 0xD4;
    R(0x62AF) = 0;
    R(0x63C2) = 0xB4, R(0x63C3) = 0x38;
    R(0x63C4) = 0xCB, R(0x63C5) = 0x38;
    R(WAIT_MSB) = 0x40;
    R(0x6385)++;
    break;
  case 1: /* 0x0ABF */
    if (!rst18()) return;
    kong_pose(0x388C);
    move_kong(0x6908, 0x30);
    move_kong(0x690B, 0x99);
    R(0x638E) = 0x1F;
    R(0x690C) = 0;
    R(0x608A) = 1;
    R(0x6385)++;
    break;
  case 2: /* 0x0AE8 */
    kong_climb_step();
    if (!(R(0x62AF) & 0x0F)) roll_ladder();
    if (R(0x690B) >= 0x5D) return;
    R(WAIT_MSB) = 0x20;
    R(0x6385)++;
    break;
  case 3:
  case 5: /* 0x3069 */
    if (rst18()) R(0x6385)++;
    break;
  case 4: { /* 0x0B06: Kong jumps up onto the top girder */
    if (R(FRAME) & 1) return;
    uint16_t hl = R(0x63C2) | R(0x63C3) << 8;
    uint8_t a = ROM(hl);
    if (a != 0x7F) {
      hl++;
      R(0x63C2) = U8(hl), R(0x63C3) = hl >> 8;
      move_kong(0x690B, a);
      return;
    }
    kong_pose(0x385C);
    copy_rom(0x6900, 0x3884, 8);
    move_kong(0x6908, 0x50);
    move_kong(0x690B, 0xFC);
    do roll_ladder(); while (R(0x638E) != 0x0A);
    R(0x6082) = 3;
    draw_board(0x392C);
    V(0x74AA) = V(0x748A) = 0x10;
    R(0x638D) = 5;
    R(WAIT_MSB) = 0x20;
    R(0x6385)++;
    break;
  }
  case 6: { /* 0x0B68: five jumps to the left, each bending a girder */
    if (R(FRAME) & 1) return;
    uint16_t hl = R(0x63C4) | R(0x63C5) << 8;
    uint8_t a = ROM(hl);
    if (a != 0x7F) {
      hl++;
      R(0x63C4) = U8(hl), R(0x63C5) = hl >> 8;
      move_kong(0x690B, a);
      move_kong(0x6908, 0xFF);
      return;
    }
    R(0x63C4) = 0xCB, R(0x63C5) = 0x38;
    R(0x6082) = 3;
    draw_board(0x38DC + (U8(R(0x638D) - 1) << 4));
    if (--R(0x638D)) return;
    R(WAIT_MSB) = 0xB0;
    R(0x6385)++;
    break;
  }
  case 7: /* 0x0BB3: the roar */
    if (R(WAIT_MSB) == 0x90) {
      R(0x608A) = 0x0F;
      R(0x608B) = 3;
      R(0x6919)++;
    } else if (R(WAIT_MSB) == 0x18) {
      R(0x6919)--;
    }
    if (!rst18()) return;
    R(0x6385) = 0;
    R(WAIT_MSB)++;
    R(GAME_MODE2)++;
    break;
  }
}

/* ------------------------------------------------------------------ HOW HIGH CAN YOU GET? (0x0BDA) */

static void how_high(void) {
  if (!rst18()) return;
  clear_play();
  task(6, R(MARIO_ALIVE));
  S->palette = 1;
  R(0x608A) = 2;
  R(0x608B) = 3;
  R(0x63A7) = 0;
  R(0x63A8) = 0xDC, R(0x63A9) = 0x76;
  if (R(0x622E) >= 6) R(0x622E) = 5;
  if (R(0x622A) != R(0x622F)) R(0x622E)++;
  R(0x622F) = R(0x622A);
  uint8_t b = R(0x622E);
  uint16_t hl = 0x75BC;
  do {
    for (uint8_t c = 0x50;; c++) { /* one Kong: 6 columns of 4 tiles */
      V(hl) = c++;
      hl--;
      V(hl) = c++;
      hl--;
      V(hl) = c++;
      hl--;
      V(hl) = c;
      if (c == 0x67) break;
      hl += 0x23;
    }
    uint8_t a = R(0x63A7)++;
    uint16_t t = 0x3CF0 + U8(a << 2), ix = R(0x63A8) | R(0x63A9) << 8;
    V(ix + 0x60) = ROM(t);
    V(ix + 0x40) = ROM(t + 1);
    V(ix + 0x20) = ROM(t + 2);
    V(ix - 0x20) = 0x8B;
    ix -= 4;
    R(0x63A8) = U8(ix), R(0x63A9) = ix >> 8;
    hl -= 0xA1;
  } while (--b);
  task(3, 7);
  R(WAIT_MSB) = 0xA0;
  R(GAME_MODE2) += 2;
}

/* ------------------------------------------------------------------ shared: ladders, slopes, motion, collisions */

/* 0x236E: a ladder at exactly x with an end at d. a = 1 at its top (b: its bottom), 0 at its bottom (b: its top);
   c: the ladders left in the search (4 or less: a broken one, listed from 0x6310) */
static bool find_ladder(uint8_t x, uint8_t d, uint8_t *a, uint8_t *b, uint8_t *c) {
  int i = 0, left = 0x15;
  while (left > 0) {
    int at = -1;
    while (left > 0) {
      uint8_t v = R(0x6300 + i);
      i++, left--;
      if (v == x) {
        at = i - 1;
        break;
      }
    }
    if (at < 0) return false;
    if (d == R(0x6315 + at)) {
      *a = 1, *b = R(0x632A + at), *c = U8(left);
      return true;
    }
    if (d == R(0x632A + at)) {
      *a = 0, *b = R(0x6315 + at), *c = U8(left);
      return true;
    }
  }
  return false;
}

static uint8_t slope(uint8_t h, uint8_t l, uint8_t b) { /* 0x2333: Y on a 25m girder after a step */
  uint8_t a = h & 0x0F;
  if (U8(b - 1) == 0) {
    if (a >= 1) return l;
    b = 1;
  } else {
    if (a < 0x0F) return l;
    b = 0xFF;
  }
  if (l == 0xF0) return (h & 0x80) ? U8(l - b) : l;
  if (l == 0x4C) return h < 0x98 ? l : U8(l + b);
  return (l & 0x20) ? U8(l - b) : U8(l + b);
}

#define RRC(v) (cy = (v) & 1, (v) = U8((v) >> 1 | cy << 7))
#define RLC(v) (cy = (v) >> 7, (v) = U8((v) << 1 | cy))
static uint8_t step_anim(uint8_t a, uint8_t b) { /* 0x3009: the next of four 2-bit states */
  uint8_t d = a, c, cy;
  RRC(a);
  if (cy) {
    c = 0xB4;
    RRC(a);
    RRC(a);
    if (cy) c = 0x1E;
    if (b & 4) b--;
  } else {
    c = 0x93;
    RRC(a);
    RRC(a);
    if (cy) c = 0x6C;
    RLC(a);
    if (!cy) c &= 0xF0;
  }
  for (int k = 0; k < 4; k++) {
    a = c;
    RRC(a);
    RRC(a);
    c = a;
    if ((a & 3) == b) break;
  }
  a = c;
  RRC(a);
  RRC(a);
  a &= 3;
  if (a != 3) return a;
  d &= U8(~4);
  return U8(d - 1) ? a : 4;
}

static void motion(uint16_t ix) { /* 0x239C: 16-bit X += (+10,+11); 16-bit Y += 16t+8 - (+12,+13) */
  unsigned s = R(ix + 4) + R(ix + 0x11);
  R(ix + 4) = U8(s);
  R(ix + 3) = U8(R(ix + 3) + R(ix + 0x10) + (s >> 8));
  unsigned y = (unsigned)R(ix + 5) << 8 | R(ix + 6), v0 = (unsigned)R(ix + 0x12) << 8 | R(ix + 0x13);
  unsigned t = R(ix + 0x14), hl = (y - v0 + (2 * t + 1) * 8) & 0xFFFF;
  R(ix + 5) = U8(hl >> 8);
  R(ix + 6) = U8(hl);
  R(ix + 0x14)++;
}

static uint16_t speed_now(uint16_t ix) { /* 0x2407: 16t - v0 */
  unsigned t = R(ix + 0x14);
  return (uint16_t)((t * 16 - ((unsigned)R(ix + 0x12) << 8 | R(ix + 0x13))) & 0xFFFF);
}

static bool girder_tile(uint8_t t) { return t >= 0xB0 && (t & 0x0F) < 8; }

/* 0x2913: is anything active in b slots from ix (de apart) within (h + its +9) across and (l + its +0A) down
   of (iy's X, c)? *left: the slots left when it was found (0 if none) */
static bool collide(uint16_t iy, uint8_t c, uint8_t h, uint8_t l, uint16_t ix, int b, int de, int *left) {
  for (; b > 0; b--, ix += de) {
    if (!(R(ix) & 1)) continue;
    uint8_t y = R(ix + 5), a = U8(c - y);
    if (c < y) a = U8(-a);
    a = U8(a + 1);
    if (a >= l && U8(a - l) >= R(ix + 0x0A)) continue;
    uint8_t ox = R(iy + 3), x = R(ix + 3), d = U8(ox - x);
    if (ox < x) d = U8(-d);
    if (d >= h && U8(d - h) >= R(ix + 9)) continue;
    if (left) *left = b;
    return true;
  }
  if (left) *left = 0;
  return false;
}

static uint16_t hit_ix;
static int hit_de, hit_left;
static bool hostile_hit(uint16_t iy, uint8_t c, uint8_t h, uint8_t l) { /* 0x286F, by board */
  static const struct { uint16_t ix; uint8_t n, de; } groups[4][3] = {
      {{0x6700, 10, 0x20}, {0x6400, 5, 0x20}, {0x66A0, 1, 0}},
      {{0x6400, 5, 0x20}, {0x65A0, 6, 0x10}, {0x66A0, 1, 0}},
      {{0x6400, 5, 0x20}, {0x6500, 10, 0x10}, {0, 0, 0}},
      {{0x6400, 7, 0x20}, {0, 0, 0}, {0, 0, 0}}};
  int s = (R(SCREEN) - 1) & 3;
  for (int g = 0; g < 3 && groups[s][g].n; g++) {
    R(0x63B9) = groups[s][g].n;
    if (collide(iy, c, h, l, groups[s][g].ix, groups[s][g].n, groups[s][g].de, &hit_left)) {
      hit_ix = groups[s][g].ix, hit_de = groups[s][g].de;
      return true;
    }
  }
  return false;
}

static uint8_t count_over(uint16_t ix, int b, uint8_t c, uint8_t h, uint8_t l) { /* 0x3EC3 */
  for (; b > 0; b--, ix += 0x20) {
    if (!(R(ix) & 1)) continue;
    uint8_t y = R(ix + 5), a = U8(c - y);
    if (c < y) a = U8(-a);
    a = U8(a + 1);
    if (a >= l && U8(a - l) >= R(ix + 0x0A)) continue;
    uint8_t ox = R(MARIO_X), x = R(ix + 3), d = U8(ox - x);
    if (ox < x) d = U8(-d);
    if (d >= h && U8(d - h) >= R(ix + 9)) continue;
    R(0x6060)++;
  }
  return R(0x6060);
}

static uint8_t jumped_over(void) { /* 0x2853, 0x3E88: at the top of a jump */
  uint8_t c = U8(R(MARIO_Y) + 0x0C), h = (R(INPUT) & 3) ? 0x13 : 5, l = 8;
  if (R(SCREEN) != 1) return hostile_hit(0x6200, c, h, l);
  R(0x6060) = 0;
  count_over(0x6700, 10, c, h, l);
  uint8_t n = count_over(0x6400, 5, c, h, l);
  return n == 0 ? 0 : n == 1 ? 1 : n < 3 ? 3 : 7;
}

/* ------------------------------------------------------------------ Mario */

static void mario_sprite(void) { /* 0x1DA6 */
  R(0x694C) = R(MARIO_X);
  R(0x694D) = R(0x6207);
  R(0x694E) = R(0x6208);
  R(0x694F) = R(MARIO_Y);
}

static void walls(uint8_t *d, uint8_t *e) { /* 0x241F: d left wall, e right wall */
  uint8_t x = R(MARIO_X);
  *d = 1, *e = 0;
  if (x < 0x16) return;
  *d = 0, *e = 1;
  if (x >= 0xEA) return;
  *e = 0;
  if (!(R(SCREEN) & 1) || R(MARIO_Y) >= 0x58 || x >= 0x6C) return;
  *d = 1;
}

static void hammer_grab(void) { /* 0x2954 */
  if (!screen_in(0x0B)) return;
  int left;
  uint8_t a = collide(0x6200, R(MARIO_Y), 4, 8, 0x6680, 2, 0x10, &left);
  R(0x6218) = a;
  R(0x6085) = U8(a << 6);
  if (!left) return;
  if (left == 1) R(0x6691) = 1;
  else R(0x6681) = 1;
}

static void push_out(void) { /* 0x2B7A: off the side of a platform */
  uint8_t x = R(MARIO_X);
  x = R(0x6210) == 0 ? U8((U8(x - 8) | 7) + 4) : U8((x | 7) - 4);
  R(MARIO_X) = R(0x694C) = x;
}

enum { CLEAR = 0, HEAD = 2, LANDED = 1 };
static int floor_test(uint8_t h, uint8_t l, uint8_t *cout) { /* 0x2B9B: the girder in the tile at (h, l) */
  uint8_t e = l, t = TILE(h, l), c;
  *cout = 0;
  if (!girder_tile(t) || t == 0xC0) return CLEAR;
  if (t < 0xC0) {
    c = U8((e & 0xF8) - 1);
  } else {
    uint8_t off = (t < 0xD0 || (t >= 0xE0 && t < 0xF0)) ? U8((t & 0x0F) - 9) : U8((t & 0x0F) - 1);
    c = U8((e & 0xF8) + off);
    if (c >= e) return CLEAR;
  }
  *cout = c;
  uint8_t a = U8(R(0x620C) - R(MARIO_Y) + e);
  if (a > c) return HEAD;
  R(MARIO_Y) = U8(c - 7);
  return LANDED;
}

static void elevator_landing(uint8_t *ra, uint8_t *rb) { /* 0x29AF */
  *ra = *rb = 0;
  if (!screen_in(4)) return;
  int left;
  if (!collide(0x6200, R(MARIO_Y), 4, 8, 0x6600, 6, 0x10, &left)) return;
  uint16_t ix = 0x6600 + (6 - left) * 0x10;
  uint8_t d = U8(R(ix + 5) - 4);
  if (U8(R(0x620C) + 5) < d) {
    R(MARIO_Y) = U8(d - 8);
    R(0x6398) = 1;
    *ra = *rb = 1;
    return;
  }
  if (U8(R(0x620C) - 0x0E) >= d) {
    R(MARIO_ALIVE) = 0;
    return;
  }
  push_out();
  *ra = 1;
}

static void landing(uint8_t *ra, uint8_t *rb) { /* 0x2B1C */
  uint8_t c;
  int r;
  *ra = *rb = 0;
  if (R(SCREEN) == 1) {
    uint8_t e = U8(R(MARIO_Y) + 7);
    r = floor_test(R(MARIO_X), e, &c);
    if (r == LANDED) *ra = *rb = 1;
    else if (r == HEAD && U8(e - c) < 4) R(MARIO_Y) = U8(c - 7), *ra = *rb = 1;
    return;
  }
  uint8_t h = U8(R(MARIO_X) - 3), e = U8(R(MARIO_Y) + 7);
  r = floor_test(h, e, &c);
  if (r == LANDED) {
    *ra = *rb = 1;
    return;
  }
  if (r != HEAD) {
    r = floor_test(U8(h + 7), e, &c);
    if (r == LANDED) {
      *ra = *rb = 1;
      return;
    }
    if (r == CLEAR) {
      elevator_landing(ra, rb);
      return;
    }
  }
  push_out();
  *ra = 1;
}

static void moving_jump(void) { /* 0x1BD8: off a wall */
  if (R(0x6220) != 1) {
    uint16_t hl = speed_now(0x6200);
    R(0x6212) = U8(hl >> 8);
    R(0x6213) = U8(hl);
    R(0x6214) = 0;
  }
  motion(0x6200);
}

static void jumping(void) { /* 0x1BB2 */
  R(0x620B) = R(MARIO_X);
  R(0x620C) = R(MARIO_Y);
  motion(0x6200);
  uint8_t d, e, a, b;
  walls(&d, &e);
  if (d == 1) {
    R(0x6210) = 0, R(0x6211) = 0x80, R(0x6207) |= 0x80;
    moving_jump();
  } else if (e == 1) {
    R(0x6210) = 0xFF, R(0x6211) = 0x80, R(0x6207) &= 0x7F;
    moving_jump();
  }
  landing(&a, &b);
  if (a == 1) {
    if (b == 1) { /* 0x1C4F: landed */
      R(0x6216) = 0;
      R(MARIO_ALIVE) = R(0x6220) ^ 1;
      R(0x6207) = U8((R(0x6207) & 0x80) | 0x0F);
      R(0x621E) = 4;
      R(0x621F) = 0;
      if (R(0x6225) == 1) R(0x6225) = 0; /* (0x1D95: the bonus sound) */
    } else { /* blocked: straight down from here */
      R(0x621F) = 1;
      memset(&R(0x6210), 0, 5);
    }
  } else if (R(0x621F) == 1) { /* 0x1C76: falling: too far? */
    if (U8(R(MARIO_Y) - 0x0F) >= R(0x620E)) R(0x6220) = 1, R(0x6084) = 3;
  } else if (R(0x6214) == 0x14) { /* the top of the jump: what was jumped */
    R(0x621F) = 1;
    uint8_t n = jumped_over();
    if (n) R(0x6342) = n, R(0x6340) = 1, R(0x6225) = 1;
  } else if (R(0x6214) == 0x13) {
    hammer_grab();
  }
  mario_sprite();
}

static void jump_start(void) { /* 0x1B6E */
  R(0x6216) = 1;
  uint8_t in = R(INPUT), b = 0, c = 0;
  if (in & 1) b = 0, c = 0x80;
  else if (in & 2) b = 0xFF, c = 0x80;
  R(0x6210) = b;
  R(0x6211) = c;
  R(0x6212) = 1;
  R(0x6213) = 0x48;
  R(0x6214) = 0;
  R(0x6204) = R(0x6206) = 0;
  R(0x6207) = U8((R(0x6207) & 0x80) | 0x0E);
  R(0x620E) = R(MARIO_Y);
  R(0x6081) = 3;
}

static void walk(uint8_t b) { /* 0x1C8F (b = 1), 0x1CAB (b = 0xFF) */
  if (R(0x620F)) {
    R(MARIO_X) = U8(R(MARIO_X) + b);
    if (R(SCREEN) == 1) R(MARIO_Y) = slope(R(MARIO_X), R(MARIO_Y), b);
    R(0x620F)--;
  } else {
    uint8_t a = step_anim(b == 1 ? 5 : 1, R(0x6202));
    R(0x6202) = a;
    a &= 3;
    if (b == 1) a |= 0x80;
    R(0x6207) = a;
    if (a & 1) R(0x6080) = 3;
    R(0x620F) = 2;
  }
  mario_sprite();
}

static void climb_step(uint8_t dy) { /* 0x1D11 */
  uint8_t y = U8(R(MARIO_Y) + dy);
  R(MARIO_Y) = y;
  R(0x6222) ^= 1;
  if (R(0x6222)) {
    R(MARIO_X) = U8((R(MARIO_X) | 3) & ~4);
    R(0x6224) ^= 1;
    if (!R(0x6224)) R(0x6080) = 3;
  } else {
    uint8_t t = U8(y + 8);
    if (t == R(0x621C) || t == R(0x621B)) { /* 0x1D67: off the ladder */
      R(0x6207) = 6;
      R(0x6219) = 0;
      R(0x6215) = 0;
      mario_sprite();
      return;
    }
    uint8_t d = U8(t - R(0x621B)), pic = d == 8 ? 5 : d == 12 ? 4 : 3;
    R(0x6207) = U8(((R(0x6207) & 0x80) ^ 0x80) | pic);
  }
  R(0x6215) = 1;
  mario_sprite();
}

static void climb_up(void) { /* 0x1D03 */
  if (R(0x620F)) { /* 0x1D76 */
    if (R(0x621A)) {
      R(0x6219) = R(0x621A);
      if (U8(R(0x621C) - 0x13) >= R(MARIO_Y)) return; /* a broken ladder ends here */
    }
    R(0x620F)--;
    return;
  }
  R(0x620F) = 4;
  climb_step(0xFE);
}

static void climb_down(void) { /* 0x1CF2 */
  if (R(0x620F)) {
    R(0x620F)--;
    return;
  }
  R(0x620F) = 3;
  climb_step(2);
}

static void ladder_keys(void) { /* 0x1B38 */
  if (R(INPUT) & 8) {
    climb_down();
    return;
  }
  if (!R(0x6215)) return;
  if (R(INPUT) & 4) climb_up();
}

static void mario_control(void) { /* 0x1AC3 */
  if (R(0x6216) == 1) {
    jumping();
    return;
  }
  if (R(0x621E)) { /* 0x1B55: landing */
    if (--R(0x621E)) return;
    R(0x6217) = R(0x6218);
    R(0x6207) &= 0x80;
    R(0x6202) = 0;
    mario_sprite();
    return;
  }
  if (R(0x6217) != 1) {
    if (R(0x6215) == 1) {
      ladder_keys();
      return;
    }
    if (R(INPUT) & 0x80) {
      jump_start();
      return;
    }
  }
  uint8_t d, e;
  walls(&d, &e);
  uint8_t in = R(INPUT);
  if (e != 1 && (in & 1)) {
    walk(1);
    return;
  }
  if (d != 1 && (in & 2)) {
    walk(0xFF);
    return;
  }
  if (R(0x6217) == 1) return;
  uint8_t dd = U8(R(MARIO_Y) + 8), x = U8((R(MARIO_X) | 3) & ~4), a, b, c;
  if (!find_ladder(x, dd, &a, &b, &c)) return;
  R(0x6207) = U8((R(0x6207) & 0x80) | 6);
  R(0x621A) = c <= 4;
  if (a == 0) { /* at the bottom: up only */
    R(0x621B) = b, R(0x621C) = dd;
    if (R(INPUT) & 4) climb_up();
    return;
  }
  if (R(0x621A)) return; /* never down a broken ladder */
  R(0x621B) = dd, R(0x621C) = b;
  ladder_keys();
}

static void fall_check(void) { /* 0x2A85: walked off an edge? */
  if (R(0x6215) || R(0x6216) || R(0x6398) == 1) return;
  uint8_t h = U8(R(MARIO_X) - 3), l = U8(R(MARIO_Y) + 0x0C);
  uint16_t hl = vram_at(h, l);
  if (girder_tile(V(hl))) return;
  if ((h & 7) && girder_tile(V(hl - 0x20))) return;
  R(0x6221) = 1;
}

static void fall_start(void) { /* 0x1F46 */
  if (!R(0x6221)) return;
  R(0x6204) = R(0x6206) = R(0x6221) = 0;
  memset(&R(0x6210), 0, 5);
  R(0x6216) = 1;
  R(0x621F) = 1;
  R(0x620E) = R(MARIO_Y);
}

/* ------------------------------------------------------------------ 25m: barrels (0x6700, 10 slots of 0x20) */

static uint8_t barrel_girder(uint16_t ix) { /* 0x2A2F: landed on a girder? (then on it) */
  uint8_t e = U8(R(ix + 5) + 4), t = TILE(R(ix + 3), e), a;
  if (!girder_tile(t) || t == 0xC0) return 0;
  if (t < 0xC0) a = 0xFF;
  else if (t < 0xD0 || (t >= 0xE0 && t < 0xF0)) a = U8((t & 0x0F) - 9);
  else a = U8((t & 0x0F) - 1);
  a = U8((e & 0xF8) + a);
  if (a >= e) return 0;
  R(ix + 5) = U8(a - 4);
  return 1;
}

static bool barrel_drum(uint16_t ix) { /* 0x24B4: into the oil drum (a blue one lets a fire out) */
  if (R(ix + 5) < 0xE8) return false;
  uint8_t x = R(ix + 3);
  if (x >= 0x2A || x < 0x20) return false;
  if (R(ix + 0x15)) R(0x62B9) = 3;
  R(ix) = 0;
  R(ix + 3) = 0;
  R(0x6082) = 3;
  if (!R(0x6348)) R(0x6348) = 1;
  return true;
}

static void barrel_turn(uint16_t ix, uint8_t c) { /* 0x23DE: a quarter turn every 4 frames (by flips) */
  uint8_t a = U8(R(ix + 0x0F) - 1);
  if (a == 0) {
    uint8_t b = U8((R(ix + 7) >> 7) << 1 | R(ix + 8) >> 7), r = step_anim(U8(3 | c), b);
    R(ix + 8) = U8((R(ix + 8) & 0x7F) | (r & 1) << 7);
    R(ix + 7) = U8((R(ix + 7) & 0x7F) | ((r >> 1) & 1) << 7);
    a = 4;
  }
  R(ix + 0x0F) = a;
}

static void tumble(uint16_t ix) { /* 0x1FCE */
  uint8_t a = U8(R(ix + 0x0F) - 1);
  if (a == 0) {
    R(ix + 7) ^= 1;
    a = 4;
  }
  R(ix + 0x0F) = a;
}

static void wild_aim(uint16_t ix) { /* 0x22CB: a wild barrel's sideways speed after each girder */
  uint8_t a;
  if (!R(0x6348)) {
    uint8_t l = R(LEVEL);
    a = l == 1 ? 1 : l == 2 ? 0xB1 : 0xE9;
    R(ix + 0x11) = a;
    R(ix + 0x10) = U8((a & 1) - 1);
    return;
  }
  switch (U8(R(DIFFICULTY) - 1)) {
  case 0:
  case 1:
    a = R(RNG1);
    R(ix + 0x11) = a;
    R(ix + 0x10) = U8((a & 1) - 1);
    break;
  case 2:
  case 3:
    R(ix + 0x11) = R(RNG1);
    R(ix + 0x10) = R(MARIO_X) >= R(ix + 3) ? 1 : 0xFF;
    break;
  case 4: {
    uint8_t d = U8(R(MARIO_X) - R(ix + 3)), c = R(MARIO_X) < R(ix + 3) ? 0xFF : 0, cy;
    for (int k = 0; k < 2; k++) {
      cy = d >> 7;
      d = U8(d << 1 | cy);
      c = U8(c << 1 | cy);
    }
    R(ix + 0x10) = c;
    R(ix + 0x11) = d;
    break;
  }
  }
}

static void barrel_ladder(uint16_t ix, uint8_t x, uint8_t d) { /* 0x216D: down this ladder? */
  uint8_t a, b, c;
  if (!find_ladder(x, d, &a, &b, &c) || a != 1) return;
  R(ix + 0x17) = U8(b - 5);
  if (R(0x6348)) { /* before the drum is lit it always takes it */
    if (U8(R(MARIO_Y) - 4) < d) return;
    uint8_t k = U8((R(DIFFICULTY) >> 1) + 1), r = R(RNG1);
    if ((r & 3) >= k) return;
    uint8_t mx = R(MARIO_X), in = R(INPUT);
    if (mx != x && !(mx > x ? (in & 2) : (in & 1)) && (r & 0x18)) return;
  }
  R(ix + 7)++;
  R(ix + 2) |= 1;
}

static void edge_step(uint16_t ix) { /* 0x2053: over an edge, falling and bouncing */
  motion(ix);
  if (!barrel_girder(ix)) {
    if (U8(R(ix + 3) + 8) < 0x10) {
      R(ix) = 0;
      R(ix + 3) = 0;
      return;
    }
    if (barrel_drum(ix)) return;
    barrel_turn(ix, U8((R(ix + 0x10) & 1) << 2));
    return;
  }
  uint8_t n = ++R(ix + 0x0E);
  if (n >= 3) {
    R(ix + 2) = R(ix + 0x10) == 1 ? 2 : 4;
    return;
  }
  if (n == 1 && (R(ix + 0x15) || U8(R(ix + 5) - 0x16) < R(MARIO_Y))) { /* it turns back (not when well below Mario) */
    if (R(ix + 0x10) == 0) R(ix + 0x11) = 0, R(ix + 0x10) = 0xFF;
    else R(ix + 0x10) = 1, R(ix + 0x11) = 0;
  }
  uint16_t hl = speed_now(ix) >> 2; /* 0x20C3: a bounce, a quarter as fast */
  R(ix + 0x12) = U8(hl >> 8);
  R(ix + 0x13) = U8(hl);
  R(ix + 0x14) = 0;
  R(ix + 4) = 0;
  R(ix + 6) = 0;
}

static void wild_step(uint16_t ix) { /* 0x20EC */
  motion(ix);
  if (U8(R(ix + 5) - 0x1A) >= R(ix + 0x19)) {
    if (barrel_girder(ix)) {
      if (R(ix + 5) >= 0xE0) { /* at the bottom it rolls left as a normal barrel */
        R(ix + 7) = U8((R(ix + 7) & 0xFC) | 1);
        R(ix + 1) = 0;
        R(ix + 2) = 0;
        R(ix + 0x10) = 0xFF;
        R(ix + 0x11) = 0;
        R(ix + 0x12) = 0;
        R(ix + 0x13) = 0xB0;
        R(ix + 0x0E) = 1;
      } else {
        wild_aim(ix);
        R(ix + 0x19) = R(ix + 5);
      }
      R(ix + 0x14) = 0;
      R(ix + 4) = 0;
      R(ix + 6) = 0;
      return;
    }
    if (barrel_drum(ix)) return;
  }
  if (U8(R(ix + 3) + 8) < 0x10) {
    R(ix) = 0;
    R(ix + 3) = 0;
    return;
  }
  tumble(ix);
}

static void barrel_step(uint16_t ix) { /* 0x1F93 */
  if (R(ix + 1) == 1) {
    wild_step(ix);
    return;
  }
  uint8_t m = R(ix + 2), b, c;
  if (m & 1) { /* down a ladder */
    R(ix + 5)++;
    if (R(ix + 0x17) == R(ix + 5)) {
      R(ix + 7) = U8((R(ix + 0x15) << 2) + 0x15);
      R(ix + 2) ^= 7;
      return;
    }
    tumble(ix);
    return;
  }
  if (m & 2) b = 1, c = 0, R(ix + 3)++;
  else if (m & 4) b = 0xFF, c = 4, R(ix + 3)--;
  else {
    edge_step(ix);
    return;
  }
  uint8_t h = R(ix + 3), l = R(ix + 5);
  if ((h & 7) == 3) {
    barrel_ladder(ix, h, U8(l + 5));
    return;
  }
  R(ix + 5) = U8(slope(h, U8(l - 3), b) + 3);
  barrel_turn(ix, c);
  if (barrel_drum(ix)) return;
  uint8_t x = R(ix + 3);
  if (x < 0x1C) R(ix + 0x10) = 0xFF, R(ix + 0x11) = 0xA0;
  else if (x >= 0xE4) R(ix + 0x10) = 0, R(ix + 0x11) = 0x60;
  else return;
  R(ix + 0x12) = 0xFF; /* 0x2038: off the end */
  R(ix + 0x13) = 0xF0;
  R(ix + 0x14) = 0;
  R(ix + 0x0E) = 0;
  R(ix + 4) = 0;
  R(ix + 6) = 0;
  R(ix + 2) = 8;
}

static void barrels_roll(void) { /* 0x1F72 */
  if (R(SCREEN) != 1) return;
  for (int i = 0; i < 10; i++) {
    uint16_t ix = 0x6700 + i * 0x20, hl = 0x6980 + i * 4;
    if (R(ix) != 1) continue;
    barrel_step(ix);
    R(hl) = R(ix + 3); /* 0x21BA */
    R(hl + 1) = R(ix + 7);
    R(hl + 2) = R(ix + 8);
    R(hl + 3) = R(ix + 5);
  }
}

static void barrel_start(void) { /* 0x2C03: is it time for Kong to take a barrel? */
  if (R(SCREEN) != 1 || !(R(MARIO_ALIVE) & 1) || (R(0x6393) & 1)) return;
  uint8_t c = R(0x62B1), a;
  if (!c) return;
  if (U8(R(0x62B0) - 2) < c) { /* the board's first barrel is wild, the second normal */
    a = R(0x62B0) == c ? 1 : 2;
    R(0x6382) = a;
    a++;
  } else if (R(0x6382) & 2) {
    R(0x6382) = 0, a = 3;
  } else {
    uint8_t b = R(DIFFICULTY), f = R(FRAME) & 0x1F;
    while (b && f != b) b--;
    if (!b) return;
    if (U8(R(0x62B0) >> 1) >= c && !(R(RNG2) & 1)) return;
    if (rng() & 0x0F) R(0x6382) = 0, a = 3;
    else R(0x6382) = 1, a = 2;
  }
  R(0x638F) = a;
  R(0x6392) = 1;
  if (R(0x62B2) != c) return;
  R(0x62B2) = U8(R(0x62B2) - 8); /* a blue one, if a fire can still come out */
  for (int i = 0; i < 5; i++)
    if (!R(0x6400 + i * 0x20)) {
      R(0x6382) |= 0x80;
      return;
    }
}

static void barrel_throw(void) { /* 0x2C8F: Kong takes, holds and lets go of a barrel */
  if (R(SCREEN) != 1 || !(R(MARIO_ALIVE) & 1)) return;
  if (!(R(0x6393) & 1)) {
    if (!(R(0x6392) & 1)) return;
    int i;
    for (i = 0; i < 10; i++)
      if (!(R(0x6700 + i * 0x20) & 3)) break;
    if (i == 10) return;
    uint16_t ix = 0x6700 + i * 0x20, spr = 0x6980 + i * 4;
    R(0x62AA) = U8(ix), R(0x62AB) = U8(ix >> 8);
    R(ix) = 2;
    R(0x62AC) = U8(spr), R(0x62AD) = U8(spr >> 8);
    R(0x6393) = 1;
    task(5, 1);
    if (--R(0x62B1) == 0) R(0x6386) = 1;
    uint8_t n = R(0x62B1);
    if (n < 4) R(0x69A8 + n * 4) = 0; /* the pile by Kong gets smaller */
    R(ix + 7) = 0x15, R(ix + 8) = 0x0B, R(ix + 0x15) = 0;
    if (R(0x6382) & 0x80) R(ix + 7) = 0x19, R(ix + 8) = 0x0C, R(ix + 0x15) = 1;
  }
  if (--R(0x62AF)) return;
  R(0x62AF) = 0x18;
  uint8_t st = R(0x638F);
  if (st) {
    uint8_t c = st;
    if (!(R(0x6382) & 1)) c--;
    kong_pose(0x3932 + U8(c * 0x28));
    if (!--R(0x638F)) {
      R(0x62AF) = 1;
      if (R(0x6382) & 1) R(0x62A8) = 0xCC, R(0x62A9) = 0x39;
    }
  }
  uint16_t hl = R(0x62A8) | R(0x62A9) << 8; /* 0x2D54: along the path to the girder */
  uint8_t a = ROM(hl);
  uint16_t ix = R(0x62AA) | R(0x62AB) << 8, de = R(0x62AC) | R(0x62AD) << 8;
  if (a != 0x7F) {
    R(de) = a & 0x7F;
    uint8_t s = R(ix + 7);
    if (a & 0x80) s ^= 3;
    R(de + 1) = s;
    R(ix + 7) = s;
    R(de + 2) = R(ix + 8);
    hl++;
    R(de + 3) = ROM(hl);
    hl++;
    R(0x62A8) = U8(hl), R(0x62A9) = U8(hl >> 8);
    return;
  }
  R(0x62A8) = 0xC3, R(0x62A9) = 0x39; /* 0x2D8C: let go */
  R(ix + 1) = 1;
  if (!(R(0x6382) & 1)) R(ix + 1) = 0, R(ix + 2) = 2;
  R(ix) = 1;
  R(ix + 0x0F) = 1;
  for (int k = 0x10; k <= 0x14; k++) R(ix + k) = 0;
  R(0x6393) = 0;
  R(0x6392) = 0;
  R(ix + 3) = R(de);
  R(ix + 5) = R(de + 3);
  kong_pose(0x385C);
  move_kong(0x690B, 0xFC);
}

/* ------------------------------------------------------------------ fires and firefoxes (0x6400, 5 slots of 0x20) */

static void fire_anim(uint16_t ix) { /* 0x3409 */
  if (R(ix + 0x15)) {
    R(ix + 0x15)--;
    return;
  }
  R(ix + 0x15) = 2;
  R(ix + 7)++;
  if ((R(ix + 7) & 0x0F) == 0x0F) R(ix + 7) ^= 2;
}

static void fire_slope(uint16_t ix) { /* 0x33C3 */
  if (R(SCREEN) != 1) return;
  R(ix + 0x0F) = slope(R(ix + 0x0E), R(ix + 0x0F), R(ix + 0x0D));
}

static void fire_walk(uint16_t ix) { /* 0x33AD */
  if (R(ix + 0x0D) == 1) R(ix + 7) |= 0x80, R(ix + 0x0E)++;
  else R(ix + 7) &= 0x7F, R(ix + 0x0E)--;
  fire_anim(ix);
  fire_slope(ix);
}

static void fire_climb(uint16_t ix) { /* 0x33E7: up at a third of the speed down */
  fire_anim(ix);
  if (R(ix + 0x0D) != 8) {
    R(ix + 0x0F)++;
    return;
  }
  if (R(ix + 0x14)) {
    R(ix + 0x14)--;
    return;
  }
  R(ix + 0x14) = 2;
  R(ix + 0x0F)--;
}

static void fire_turn(uint16_t ix) { /* 0x330F: every 43 moves, left or right at random */
  if (!R(ix + 0x16)) {
    R(ix + 0x16) = 0x2B;
    R(ix + 0x0D) = R(RNG1) & 1;
  }
  R(ix + 0x16)--;
}

static void fire_ladder(uint16_t ix) { /* 0x333D */
  uint8_t dir = R(ix + 0x0D), a, b, c;
  if (dir == 8 || dir == 4) {
    if (U8(R(ix + 0x0F) + 8) != R(ix + 0x1F)) return;
    R(ix + 0x0D) = 0;
    if (dir == 8 && R(ix + 0x19) == 2) R(ix + 0x1D) = 1;
    return;
  }
  if (screen_in(7) && R(ix + 0x0F) < 0x59) return; /* 0x33A1: never down from the top girder */
  if (!find_ladder(R(ix + 0x0E), U8(R(ix + 0x0F) + 8), &a, &b, &c)) return;
  R(ix + 0x1F) = b;
  if (a == 0) {
    R(ix + 0x0D) = 8;
    return;
  }
  if (R(ix + 0x0F) >= R(MARIO_Y)) return; /* down only toward Mario */
  R(ix + 0x0D) = 4;
}

static void freezer(uint16_t ix) { /* 0x32D6 */
  if (R(ix + 0x1C)) {
    if (--R(ix + 0x1C)) {
      R(ix + 0x0D) = 0;
      return;
    }
  } else if (R(ix + 0x1D) != 1) {
    fire_turn(ix);
    return;
  } else {
    R(ix + 0x1D) = 0;
    if (R(MARIO_Y) >= R(ix + 0x0F)) {
      R(ix + 0x1C) = 0xFF;
      R(ix + 0x0D) = 0;
      return;
    }
  }
  R(ix + 0x19) = 0; /* 0x3303 */
  R(ix + 0x1C) = 0;
  fire_turn(ix);
}

static void fire_arc(uint16_t ix, uint16_t hl) { /* 0x3445: out of the drum, one point of the arc a frame */
  uint8_t a = ROM(hl);
  if (a != 0xAA) {
    R(ix + 5) = a;
    hl++;
    R(ix + 0x1A) = U8(hl), R(ix + 0x1B) = U8(hl >> 8);
    return;
  }
  R(ix + 0x13) = R(ix + 0x18) = R(ix + 0x0D) = R(ix + 0x1C) = 0;
  R(ix + 0x0E) = R(ix + 3);
  R(ix + 0x0F) = R(ix + 5);
  R(ix + 0x1A) = R(ix + 0x1B) = 0;
}

static void fire_spawn(uint16_t ix) { /* 0x32BD */
  uint16_t hl = R(ix + 0x1A) | R(ix + 0x1B) << 8;
  switch (R(SCREEN)) {
  case 1: /* 0x342C */
    if (!hl) hl = 0x3A8C, R(ix + 3) = 0x26;
    R(ix + 3)++;
    fire_arc(ix, hl);
    break;
  case 2: /* 0x3478: toward Mario's side */
    if (!hl) {
      hl = 0x3AAC;
      if (R(MARIO_X) & 0x80) R(ix + 0x0D) = 1, R(ix + 3) = 0x7E;
      else R(ix + 0x0D) = 2, R(ix + 3) = 0x80;
    }
    if (R(ix + 0x0D) == 1) R(ix + 3)++;
    else R(ix + 3)--;
    fire_arc(ix, hl);
    break;
  case 4: /* 0x34B9: on the side away from Mario */
    hl = (R(MARIO_X) & 0x80) ? 0x3AD4 : 0x3AC4;
    hl += R(RNG2) & 6;
    R(ix + 3) = R(ix + 0x0E) = ROM(hl);
    R(ix + 5) = R(ix + 0x0F) = ROM(hl + 1);
    R(ix + 0x0D) = R(ix + 0x18) = R(ix + 0x1C) = 0;
    break;
  }
}

static void fire_move(uint16_t ix) { /* 0x3202 */
  if (R(ix + 0x18) == 1) {
    fire_spawn(ix);
    return;
  }
  if ((int8_t)U8(R(ix + 0x0D) - 4) >= 0) goto ladder;
  if (R(ix + 0x19) == 2) {
    freezer(ix);
  } else {
    fire_turn(ix);
    if (R(RNG1) & 3) goto moved; /* a ladder is looked for one time in four */
  }
  if (R(ix + 0x0D) == 0) goto bob; /* left-moving fires skip those turns */
ladder:
  fire_ladder(ix);
moved:
  if ((int8_t)U8(R(ix + 0x0D) - 4) >= 0) {
    fire_climb(ix);
    goto bob;
  }
  fire_walk(ix);
  if (!girder_tile(TILE(R(ix + 0x0E), U8(R(ix + 0x0F) + 0x0C)))) { /* 0x298C: the end of a girder */
    if (R(ix + 0x0D) == 1) R(ix + 0x0D) = 2, R(ix + 0x0E)--;
    else R(ix + 0x0D) = 1, R(ix + 0x0E)++;
    fire_slope(ix);
    goto bob;
  }
  if (R(ix + 0x0E) < 0x10) R(ix + 0x0D) = 1;
  else if (R(ix + 0x0E) >= 0xF0) R(ix + 0x0D) = 2;
bob: { /* 0x3257 */
  uint8_t a = R(ix + 0x13);
  a = a ? U8(a - 1) : 0x11;
  R(ix + 0x13) = a;
  R(ix + 3) = R(ix + 0x0E);
  R(ix + 5) = U8(R(ix + 0x0F) + ROM(0x3A7A + a));
}
}

static bool fire_count(void) { /* 0x313C: counts them, colours them, lets a new one out */
  R(0x63A1) = 0;
  for (int i = 0; i < 5; i++) {
    uint16_t ix = 0x6400 + i * 0x20;
    if (R(ix)) {
      R(0x63A1)++;
      R(ix + 8) = R(0x6217) == 1 ? 0 : 1;
      continue;
    }
    if (R(0x63A1) == 5) continue;
    if (R(SCREEN) == 2 && R(0x63A1) == R(DIFFICULTY)) return true;
    if (R(0x63A0) != 1) continue;
    R(ix) = 1;
    R(ix + 0x18) = 1;
    R(0x63A0) = 0;
    R(0x63A1)++;
  }
  R(0x63A0) = 0;
  return R(0x63A1) != 0;
}

static void fires(void) { /* 0x30ED */
  uint8_t d = R(DIFFICULTY), f = R(FRAME);
  if (d >= 6) d = 5;
  switch (d) { /* 0x30FA: faster with the difficulty */
  case 0:
  case 1:
    if ((f & 1) != 1) return;
    break;
  case 2:
    if ((f & 7) >= 5) return;
    break;
  case 3:
  case 4:
    if ((f & 3) == 3) return;
    break;
  default:
    if ((f & 7) == 7) return;
    break;
  }
  if (!fire_count()) return;
  if ((int8_t)U8(R(DIFFICULTY) - 3) >= 0 && (R(RNG1) & 3) == 1 && R(FRAME) == 1) /* 0x31DD: freezers */
    R(0x6439) = R(0x6479) = 2;
  for (int i = 0; i < 5; i++) {
    uint16_t ix = 0x6400 + i * 0x20;
    R(0x63C8) = U8(ix), R(0x63C9) = U8(ix >> 8);
    if (R(ix)) fire_move(ix);
  }
  for (int i = 0; i < 5; i++) { /* 0x34F3 */
    uint16_t ix = 0x6400 + i * 0x20, de = 0x69D0 + i * 4;
    if (!R(ix)) continue;
    R(de) = R(ix + 3);
    R(de + 1) = R(ix + 7);
    R(de + 2) = R(ix + 8);
    R(de + 3) = R(ix + 5);
  }
}

static void fire_timer(void) { /* 0x2DDB: 50m and 100m */
  if (!screen_in(0x0A) || !(R(MARIO_ALIVE) & 1)) return;
  uint8_t b = U8(R(DIFFICULTY) + 1) >> 1, a = 0xFF;
  if (R(SCREEN) == 2) b++;
  while (--b) a >>= 1;
  if (R(FRAME) & a) return;
  R(0x63A0) = 1;
  R(0x639A) = 1;
}

/* ------------------------------------------------------------------ 50m: pies, conveyors, the moving ladders */

static void pies(void) { /* 0x24EA */
  if (R(SCREEN) != 2) return;
  if (R(0x639B)) { /* 0x2523: a new pie */
    R(0x639B)--;
  } else if (R(0x639A)) {
    int i;
    for (i = 0; i < 6; i++)
      if (!(R(0x65A0 + i * 0x10) & 1)) break;
    if (i < 6) {
      uint16_t ix = 0x65A0 + i * 0x10;
      uint8_t a = rng();
      bool right;
      R(ix + 5) = 0x7C;
      if (a < 0x60 || R(0x62A3) == 1) {
        R(ix + 5) = 0xCC;
        right = R(0x62A6) & 0x80;
      } else {
        right = rng() < 0x68;
      }
      R(ix + 3) = right ? 0xF8 : 0x07;
      R(ix) = 1;
      R(ix + 7) = 0x4B;
      R(ix + 9) = 8;
      R(ix + 0x0A) = 3;
      R(0x639B) = 0x7C;
      R(0x639A) = 0;
      R(RNG2)--; /* (0x258F decrements what HL points at: RngTimer2, left there by 0x0057) */
    }
  }
  for (int i = 0; i < 6; i++) { /* 0x2591: along the belts, into the fire or off the end */
    uint16_t ix = 0x65A0 + i * 0x10;
    if (!(R(ix) & 1)) continue;
    uint8_t h = R(ix + 3);
    bool gone = U8(h + 7) < 0x0E;
    if (!gone && R(ix + 5) == 0x7C) {
      if (h == 0x80) gone = true;
      else R(ix + 3) = U8((h >= 0x80 ? R(0x63A5) : R(0x63A4)) + h);
    } else if (!gone) {
      R(ix + 3) = U8(R(0x63A6) + h);
    }
    if (gone) {
      R(ix) = 0;
      R(ix + 3) = 0;
      R(0x69B8 + i * 4) = 0;
    }
  }
  for (int i = 0; i < 6; i++) {
    uint16_t ix = 0x65A0 + i * 0x10, hl = 0x69B8 + i * 4;
    if (!R(ix)) continue;
    R(hl) = R(ix + 3);
    R(hl + 1) = R(ix + 7);
    R(hl + 2) = R(ix + 8);
    R(hl + 3) = R(ix + 5);
  }
}

static uint8_t belt_vector(uint16_t hl) { /* 0x26E9: moving every other frame */
  if (!(R(FRAME) & 1)) return 0;
  uint8_t a = (R(hl) & 0x80) ? 0xFF : 1;
  R(hl) = a;
  return a;
}

static void belt_reverse(uint16_t hl) { R(hl) = (R(hl) & 0x80) ? 2 : 0xFE; } /* 0x26DE */

static uint8_t pulleys(uint16_t hl, uint16_t de) { /* 0x26A6 */
  uint8_t a;
  hl++;
  if (!(R(de) & 0x80)) {
    a = U8(R(hl) + 1);
    if (a == 0x53) a = 0x50;
    R(hl) = a;
    hl += 4;
    a = U8(R(hl) - 1);
    if (a == 0xCF) a = 0xD2;
  } else {
    a = U8(R(hl) - 1);
    if (a == 0x4F) a = 0x52;
    R(hl) = a;
    hl += 4;
    a = U8(R(hl) + 1);
    if (a == 0xD3) a = 0xD0;
  }
  R(hl) = a;
  return a;
}

static void belt_top(void) { /* 0x2602 */
  if (!(R(FRAME) & 1) && !--R(0x62A0)) {
    R(0x62A0) = 0x80;
    belt_reverse(0x62A1);
  }
  R(0x63A3) = belt_vector(0x62A1);
  if ((R(FRAME) & 0x1F) == 1) pulleys(0x69E4, 0x62A1);
}

static void conveyors(void) { /* 0x25F2 */
  if (R(SCREEN) != 2) return;
  belt_top();
  if (R(MARIO_Y) < 0xC0) { /* 0x262F: while Mario is up there the middle belts run inwards */
    if (!(R(0x62A3) & 0x80)) R(0x62A3) = 0xFF;
  } else if (!(R(FRAME) & 1) && !--R(0x62A2)) {
    R(0x62A2) = 0xC0;
    belt_reverse(0x62A3);
  }
  uint8_t a = belt_vector(0x62A3);
  R(0x63A5) = a;
  R(0x63A4) = U8(-a);
  if (!(R(FRAME) & 0x1F)) R(0x69ED) = pulleys(0x69EC, 0x62A2) & 0x7F;
  if (!(R(FRAME) & 1) && !--R(0x62A5)) { /* 0x2679 */
    R(0x62A5) = 0xFF;
    belt_reverse(0x62A6);
  }
  R(0x63A6) = belt_vector(0x62A6);
  if ((R(FRAME) & 0x1F) == 2) pulleys(0x69F4, 0x62A6);
  uint8_t x = R(MARIO_X), y = R(MARIO_Y); /* 0x2AD3: Mario on a belt */
  if (y == 0x50) a = R(0x63A3);
  else if (y == 0x78) a = x >= 0x80 ? R(0x63A5) : R(0x63A4);
  else if (y == 0xC8) a = R(0x63A6);
  else return;
  R(MARIO_X) = R(0x694C) = U8(a + x);
  uint8_t d, e;
  walls(&d, &e);
  if (e == 1) R(MARIO_X)--;
  else if (d == 1) R(MARIO_X)++;
}

static bool on_moving_ladder(uint16_t hl) { /* 0x2243 */
  return R(MARIO_Y) < 0x7A && !R(0x6216) && R(MARIO_X) == R(hl);
}

static void ladder_sprite(uint16_t hl) { R((hl & 8) ? 0x694B : 0x6947) = R(hl); } /* 0x22BD */

static void moving_ladders(void) { /* 0x2207 */
  if (R(SCREEN) != 2) return;
  uint16_t hl = (R(FRAME) & 1) ? 0x6280 : 0x6288;
  switch (R(hl)) {
  case 0: /* up */
    if (--R(hl + 1)) {
      if (on_moving_ladder(hl + 2)) R(0x621A) = 0;
      return;
    }
    R(hl)++;
    if (on_moving_ladder(hl + 2)) R(0x621A) = 1;
    return;
  case 1: /* going down */
    if (--R(hl + 4)) return;
    R(hl + 4) = 4;
    R(hl + 3)++;
    ladder_sprite(hl + 3);
    if (R(hl + 3) == 0x78) R(hl)++;
    if (!on_moving_ladder(hl + 2)) return;
    {
      uint8_t y = R(MARIO_Y);
      if (y < 0x68 || (y & 1)) {
        R(MARIO_Y)++;
        R(0x694D) = 3;
        R(0x694F)++;
      } else {
        R(0x6222) = (y & 2) ? 1 : 0;
      }
    }
    return;
  case 2: /* down */
    if (!(R(RNG1) & 0x3C)) R(hl)++;
    return;
  case 3: /* going up */
    if (--R(hl + 4)) return;
    R(hl + 4) = 2;
    R(hl + 3)--;
    ladder_sprite(hl + 3);
    if (R(hl + 3) != 0x68) return;
    R(hl + 1) = 0x80;
    R(hl) = 0;
    return;
  }
}

/* ------------------------------------------------------------------ 75m: elevators and springs */

static void elevator_dies(void) { /* 0x277F */
  R(MARIO_ALIVE) = 0;
  R(0x6398) = 0;
}

static void elevators(void) { /* 0x26FA */
  if (R(SCREEN) != 3) return;
  if (R(MARIO_Y) >= 0xF0) {
    elevator_dies();
    return;
  }
  uint8_t f = R(FRAME);
  bool move = R(LEVEL) == 1 ? (f & 3) == 0 : (f & 1);
  if (!move) {
    if (R(LEVEL) == 1 && (f & 3) != 1) return;
    if (!R(0x6398) || R(0x6216)) return; /* 0x2745: riding */
    uint8_t x = R(MARIO_X), y = R(MARIO_Y);
    if (x < 0x2C || (x >= 0x43 && x < 0x6C) || x >= 0x83) {
      R(0x6398) = 0;
      R(0x6221) = 1;
    } else if (x < 0x43) {
      if (y < 0x71) elevator_dies();
      else R(MARIO_Y) = R(0x694F) = U8(y - 1);
    } else {
      if (y >= 0xE8) elevator_dies();
      else R(MARIO_Y) = R(0x694F) = U8(y + 1);
    }
    return;
  }
  for (int i = 0; i < 6; i++) { /* 0x2797 */
    uint16_t ix = 0x6600 + i * 0x10;
    if (!(R(ix) & 1)) continue;
    if (R(ix + 0x0D) & 8) {
      if (--R(ix + 5) == 0x60) R(ix + 3) = 0x77, R(ix + 0x0D) = 4;
    } else if (++R(ix + 5) == 0xF8) {
      R(ix) = 0;
    }
  }
  if (R(0x62A7)) { /* 0x27DA: a new one every 0x34 moves */
    R(0x62A7)--;
  } else {
    for (int i = 0; i < 6; i++) {
      uint16_t ix = 0x6600 + i * 0x10;
      if (R(ix) & 1) continue;
      R(ix) = 1;
      R(ix + 3) = 0x37;
      R(ix + 5) = 0xF8;
      R(ix + 0x0D) = 8;
      R(0x62A7) = 0x33;
      break;
    }
  }
  for (int i = 0; i < 6; i++) {
    R(0x6958 + i * 4) = R(0x6603 + i * 0x10);
    R(0x695B + i * 4) = R(0x6605 + i * 0x10);
  }
}

static void springs(void) { /* 0x2E04 */
  if (R(SCREEN) != 3 || !(R(MARIO_ALIVE) & 1)) return;
  for (int i = 0; i < 10; i++) {
    uint16_t ix = 0x6500 + i * 0x10, iy = 0x6980 + i * 4;
    if (!(R(ix) & 1)) {
      if (!(R(0x6396) & 1)) continue;
      R(0x6396) = 0; /* 0x2EAE */
      R(ix + 5) = 0x50;
      R(ix + 0x0D) = 1;
      R(ix + 3) = U8((rng() & 0x0F) + 0xF8);
      R(ix) = 1;
      R(ix + 0x0E) = 0xAA, R(ix + 0x0F) = 0x39;
      continue;
    }
    if (!(R(FRAME) & 0x0F)) R(iy + 1) ^= 7;
    if (R(ix + 0x0D) == 4) {
      uint8_t y = U8(R(ix + 5) + 3);
      R(ix + 5) = y;
      if (y >= 0xF8) R(ix + 3) = 0, R(ix) = 0;
    } else {
      R(ix + 3) = U8(R(ix + 3) + 2);
      uint16_t hl = R(ix + 0x0E) | R(ix + 0x0F) << 8;
      uint8_t c = ROM(hl);
      if (c == 0x7F) {
        hl = 0x39AA;
        R(0x6083) = 3;
      } else {
        hl++;
        R(ix + 5) = U8(c + R(ix + 5));
      }
      R(ix + 0x0E) = U8(hl), R(ix + 0x0F) = U8(hl >> 8);
      if (R(ix + 3) >= 0xB7 && c == 0x7F) {
        R(ix + 0x0D) = 4;
        R(0x6083) = 0;
        R(0x6084) = 3;
      }
    }
    R(iy) = R(ix + 3);
    R(iy + 3) = R(ix + 5);
  }
}

/* ------------------------------------------------------------------ 100m: rivets */

static void rivets(void) { /* 0x1A33 */
  if (R(SCREEN) != 4) return;
  uint8_t x = R(MARIO_X);
  if (x == 0x4B || x == 0xB3) {
    R(0x6291) = 1;
    return;
  }
  if (R(0x6291) != 1) return;
  R(0x6291) = 0;
  uint8_t a = U8(R(MARIO_Y) - 1), b = 0;
  if (a >= 0xD0) return;
  if (a & 0x80) b |= 4;
  if (a & 0x20) b |= 2;
  if (((a >> 5) & 7) == 6) b |= 2;
  if (x & 0x80) b |= 1;
  if (!R(0x6292 + b)) return;
  R(0x6292 + b) = 0;
  R(0x6290)--;
  uint16_t hl = (b & 1) ? 0x012B : 0x02CB;
  hl += 5 * (b >> 1);
  hl += 0x7400;
  V(hl) = V(hl - 1) = V(hl + 1) = 0x10;
  R(0x6340) = R(0x6342) = R(0x6225) = 1;
  if (!R(0x6216)) R(0x6225) = 0;
}

/* ------------------------------------------------------------------ items, points, the hammer */

static void items(void) { /* 0x19DA: Pauline's hat, purse and umbrella */
  uint8_t x = R(MARIO_X);
  for (int i = 0; i < 3; i++) {
    uint16_t hl = 0x6A0C + i * 4;
    if (R(hl) != x) continue;
    if (R(MARIO_Y) != R(hl + 3) || (R(hl + 1) & 8)) return;
    R(0x6343) = U8(hl), R(0x6344) = U8(hl >> 8);
    R(0x6342) = 0;
    R(0x6340) = 1;
    return;
  }
}

static void score_sprite(uint8_t x, uint8_t pic, uint8_t y) { /* 0x1E36 */
  R(0x6A30) = x;
  R(0x6A31) = pic;
  R(0x6A32) = 7;
  R(0x6A33) = y;
  if (screen_in(5)) R(0x6085) = 3;
}

static void bonus_points(void) { /* 0x1DBD */
  if (R(0x6340) == 2) {
    if (--R(0x6341)) return;
    R(0x6A30) = 0;
    R(0x6340) = 0;
    return;
  }
  if (R(0x6340) != 1) return;
  R(0x6341) = 0x40;
  R(0x6340) = 2;
  uint8_t s = R(0x6342), pic, pts;
  if (s & 1) { /* jumped (or a rivet): 100, 300, 500 (shown as 800: the ROM's slip) */
    uint8_t a = s >> 1;
    if (!(a & 1)) pts = 1, pic = 0x7B;
    else if (!(a & 2)) pts = 3, pic = 0x7D;
    else pts = 5, pic = 0x7F;
    task(0, pts);
    score_sprite(R(MARIO_X), pic, U8(R(MARIO_Y) + 0x14));
    return;
  }
  if (s & 2) pts = 3, pic = 0x7D; /* a barrel smashed */
  else if (s & 4) { /* something else smashed */
    uint8_t r = R(RNG1);
    if (r & 1) pts = 5, pic = 0x7E;
    else if (r & 2) pts = 8, pic = 0x7F;
    else pts = 3, pic = 0x7D;
  } else { /* an item */
    R(0x6085) = 3;
    uint8_t l = R(LEVEL);
    if (l == 1) pts = 3, pic = 0x7D;
    else if (l == 2) pts = 5, pic = 0x7E;
    else pts = 8, pic = 0x7F;
  }
  task(0, pts);
  uint16_t hl = R(0x6343) | R(0x6344) << 8;
  uint8_t x = R(hl);
  R(hl) = 0;
  score_sprite(x, pic, R(hl + 3));
}

static void smash(void) { /* 0x1E96: everything stops while a hit flashes */
  switch (R(0x6345)) {
  case 0: {
    uint8_t g = R(0x6352);
    uint16_t hl = g == 0x65 ? 0x69B8 : g < 0x65 ? 0x69D0 : 0x6980;
    uint16_t ix = R(0x6351) | R(0x6352) << 8;
    for (uint8_t n = R(0x6354); n; n--) hl += 4, ix += R(0x6353);
    R(ix) = 0;
    R(0x6342) = R(ix + 0x15) ? 4 : 2;
    R(0x6A2C) = R(hl);
    R(hl) = 0;
    R(0x6A2D) = 0x60;
    R(0x6A2E) = 0x0C;
    R(0x6A2F) = R(hl + 3);
    R(0x6345)++;
    R(0x6346) = 6;
    R(0x6347) = 5;
    R(0x608A) = 6;
    R(0x608B) = 3;
    break;
  }
  case 1:
    if (--R(0x6346)) return;
    R(0x6346) = 6;
    if (--R(0x6347)) {
      R(0x6A2D) ^= 1;
      return;
    }
    R(0x6347) = 4;
    R(0x6345)++;
    break;
  case 2:
    if (--R(0x6346)) return;
    R(0x6346) = 0x0C;
    if (--R(0x6347)) {
      R(0x6A2D)++;
      return;
    }
    R(0x6345) = 0;
    R(0x6350) = 0;
    R(0x6340) = 1;
    R(0x6343) = 0x2C, R(0x6344) = 0x6A;
    break;
  }
}

static void hammer(void) { /* 0x2ED4 */
  if (!screen_in(0x0B) || !(R(MARIO_ALIVE) & 1)) return;
  uint16_t de = 0x6A18, ix = 0x6680;
  if (!(R(ix + 1) & 1)) de = 0x6A1C, ix = 0x6690;
  R(ix + 0x0E) = 0;
  R(ix + 0x0F) = 0xF0;
  uint8_t b, c;
  if (R(0x6217) & 1) {
    R(0x6218) = 0;
    R(0x6089) = 4;
    R(ix + 9) = 6;
    R(ix + 0x0A) = 3;
    b = 0x1E;
    uint8_t a = R(0x6207), cy = a >> 7;
    a = U8(a << 1);
    if (cy) a |= 0x80, b |= 0x80;
    c = a | 8;
    if (R(0x6394) & 8) { /* swung down */
      b |= 1;
      c |= 1;
      R(ix + 9) = 5;
      R(ix + 0x0A) = 6;
      R(ix + 0x0F) = 0;
      R(ix + 0x0E) = (c & 0x80) ? 0x10 : 0xF0;
    }
    R(0x694D) = c;
    c = 7;
    if (++R(0x6394) == 0) {
      if (++R(0x6395) == 2) { /* used up */
        R(0x6395) = 0;
        R(0x6217) = 0;
        R(ix + 1) = 0;
        R(ix + 0x0E) = U8(-R(MARIO_X));
        R(0x694D) = R(0x6207);
        R(ix) = 0;
        R(0x6089) = R(0x6389);
      } else if (R(FRAME) & 8) {
        c = 1;
      }
    } else if (R(0x6395) && (R(FRAME) & 8)) {
      c = 1; /* the second half flashes */
    }
  } else {
    if (!(R(0x6218) & 1)) return;
    R(ix + 9) = 6; /* 0x2F9C: picked up, over Mario's head */
    R(ix + 0x0A) = 3;
    b = U8((R(0x6207) & 0x80) | 0x1E);
    c = 7;
    R(0x6389) = R(0x6089);
  }
  uint8_t x = U8(R(MARIO_X) + R(ix + 0x0E)), y = U8(R(MARIO_Y) + R(ix + 0x0F)); /* 0x2F7C */
  R(de) = R(ix + 3) = x;
  R(de + 1) = b;
  R(de + 2) = c;
  R(de + 3) = R(ix + 5) = y;
}

static void hammer_hits(void) { /* 0x281D */
  for (int i = 0; i < 2; i++) {
    uint16_t iy = 0x6680 + i * 0x10;
    if (!(R(iy + 1) & 1)) continue;
    if (!hostile_hit(iy, R(iy + 5), R(iy + 9), R(iy + 0x0A))) return;
    R(0x6350) = 1;
    R(0x6354) = U8(R(0x63B9) - hit_left);
    R(0x6353) = U8(hit_de);
    R(0x6351) = U8(hit_ix), R(0x6352) = U8(hit_ix >> 8);
    return;
  }
}

/* ------------------------------------------------------------------ Kong and Pauline (0x03FB) */

static void kong_and_pauline(void) {
  if (R(SCREEN) == 2) { /* Kong rides the top belt */
    move_kong(0x6908, R(0x63A3));
    R(0x63B7) = U8(R(0x6910) - 0x3B);
  }
  if (!R(0x6391)) {
    if (R(FRAME)) goto help;
    R(0x6391) = 1;
  }
  {
    uint8_t t = ++R(0x6390);
    if (t == 0x80) {
      R(0x6390) = R(0x6391) = 0;
      if (R(0x6393)) goto help;
      kong_pose(0x385C);
    } else {
      if (R(0x6393) || (t & 0x1F)) goto help;
      kong_pose((t & 0x20) ? 0x39CF : 0x39F7); /* beating his chest */
      R(0x6082) = 3;
    }
    uint8_t s = R(SCREEN);
    if (s == 1) move_kong(0x690B, 0xFC);
    else if (s == 2) move_kong(0x6908, R(0x63B7));
    else if (s == 4) move_kong(0x6908, 0x44);
  }
help: {
  uint8_t c = R(0x6390);
  if (R(SCREEN) != 4) {
    write3(0x75C4, (c && (c & 0x40)) ? 0xEF : 0x10, 0x20);
    if (!(c & 0x40) || (c & 7)) return;
    R(0x6905) ^= 3;
    return;
  }
  write3(0x7623, 0x10, 0x20); /* 100m: she calls from Mario's side */
  write3(0x7583, 0x10, 0x20);
  bool left = R(MARIO_X) < 0x80;
  if (c & 0x40) write3(left ? 0x7623 : 0x7583, left ? 0xDF : 0xEF, 0x20);
  if (left) R(0x6901) |= 0x80, R(0x6905) |= 0x80;
  else R(0x6901) &= 0x7F, R(0x6905) &= 0x7F;
  if (!(c & 0x40) || (c & 7)) return;
  R(0x6905) ^= 3;
}
}

/* ------------------------------------------------------------------ the main routine (0x197A) */

static bool board_done(void) { /* 0x1E57 */
  uint8_t s = R(SCREEN);
  if (s & 4) {
    if (R(0x6290)) return false;
  } else {
    if (R(MARIO_Y) >= ((s & 1) ? 0x31 : 0x51)) return false;
    R(0x694D) = (R(MARIO_X) & 0x80) ? 0x00 : 0x80; /* he faces Pauline */
  }
  R(GAME_MODE2) = 0x16;
  return true;
}

static bool time_out(void) { /* 0x1A07 */
  switch (R(0x6386)) {
  case 1:
    R(0x6387) = 0;
    R(0x6386) = 2;
    break;
  case 2:
    if (!--R(0x6387)) R(0x6386) = 3;
    break;
  case 3:
    if (R(0x6216)) break; /* never in the middle of a jump */
    R(GAME_MODE2)++;
    R(WAIT_MSB) = 0x40;
    return true;
  }
  return false;
}

static void bonus_tick(void) { /* 0x2FCB: all but 25m */
  if (R(SCREEN) == 1 || --R(0x62B4)) return;
  R(0x62B9) = 3;
  R(0x6396) = 3;
  task(5, 1);
  R(0x62B4) = R(0x62B3);
  if (!--R(0x62B1)) R(0x6386) = 1;
}

static void play(void) {
  bonus_points();
  if (R(0x6350)) {
    smash();
    return;
  }
  mario_control();
  barrels_roll();
  barrel_throw();
  barrel_start();
  fires();
  springs();
  pies();
  fire_timer();
  hammer();
  moving_ladders();
  rivets();
  fall_check();
  fall_start();
  elevators();
  conveyors();
  items();
  kong_and_pauline();
  if (hostile_hit(0x6200, R(MARIO_Y), 4, 7)) R(MARIO_ALIVE) = 0; /* 0x2808 */
  hammer_hits();
  if (board_done() || time_out()) return;
  bonus_tick();
  if (R(MARIO_ALIVE)) return;
  R(0x6082) = 3; /* 0x19CA */
  R(GAME_MODE2)++;
  R(WAIT_MSB) = 0x40;
}

/* ------------------------------------------------------------------ death (0x127C) and lives (0x12F2) */

static void clear_sprites(uint16_t hl, int n) { /* 0x30E4 */
  for (int i = 0; i < n; i++) R(hl + i * 4) = 0;
}
static void clear_hostiles(void) { /* 0x30BD */
  clear_sprites(0x6950, 2);
  clear_sprites(0x6980, 10);
  clear_sprites(0x69B8, 11);
  clear_sprites(0x6A0C, 5);
}

static void clear_box(uint16_t hl) { /* 0x1826: 5 rows by 14 columns */
  for (int c = 0; c < 14; c++, hl -= 0x20)
    for (int r = 0; r < 5; r++) V(hl + r) = 0x10;
}

static void dying(void) {
  bonus_points();
  switch (R(0x639D)) {
  case 0:
    if (!rst18()) return;
    R(0x694D) = U8((R(0x694D) & 0x80) | 0x78);
    R(0x639D)++;
    R(0x639E) = 0x0D;
    R(WAIT_MSB) = 8;
    clear_hostiles();
    R(0x6088) = 3;
    break;
  case 1: /* 13 turns, 8 frames apart */
    if (!rst18()) return;
    R(WAIT_MSB) = 8;
    if (--R(0x639E)) {
      uint8_t s = R(0x694D), b = U8((s & 1) << 7 | 1);
      R(0x694D) = s ^ b;
      R(0x694E) ^= b & 0x80;
      return;
    }
    R(0x694D) = U8((R(0x694D) & 0x80) | 0x7A);
    R(0x639D)++;
    R(WAIT_MSB) = 0x80;
    break;
  case 2:
    if (!rst18()) return;
    R(0x694C) = 0; /* 0x30DB */
    clear_sprites(0x6958, 6);
    R(GAME_MODE2)++;
    R(WAIT_MSB) = 1;
    break;
  }
}

static void lose_life(void) {
  R(0x622C) = 0;
  uint8_t n = --R(LIVES);
  memcpy(&R(0x6040), &R(0x6228), 8);
  if (n) {
    R(GAME_MODE2) = 8;
    return;
  }
  clear_box(0x76D4);
  task(3, 0); /* GAME OVER */
  R(WAIT_MSB) = 0xC0;
  R(GAME_MODE2) = 0x10;
}

/* ------------------------------------------------------------------ a board cleared (0x1615) */

static void next_board(void) { /* 0x178F / 0x193D */
  uint16_t hl = (R(0x622A) | R(0x622B) << 8) + 1;
  uint8_t a = ROM(hl);
  if (a == 0x7F) hl = 0x3A73, a = ROM(hl);
  R(0x622A) = U8(hl), R(0x622B) = U8(hl >> 8);
  R(SCREEN) = a;
}

static void heart(void) { /* 0x1708 */
  R(0x6A20) = 0x80, R(0x6A21) = 0x76, R(0x6A22) = 9, R(0x6A23) = 0x20;
  R(0x6905) = 0x13;
  write3(0x75C4, 0x10, 0x20);
  R(0x608A) = 7;
  R(0x608B) = 3;
}

static void kong_takes_her(void) { /* 0x168A / 0x16EE: the climbing pose, without her yet */
  kong_pose(0x388C);
  R(0x690C) = 0x66;
  R(0x6924) = 0;
  R(0x692C) = 0;
  R(0x62AF) = 0;
}

static void board_clear(void) {
  clear_hostiles();
  uint8_t s = R(SCREEN), step = R(0x6388);
  if (s == 4) {
    bonus_points();
    switch (step) {
    case 0: /* 0x17B7: the middle of the rivet girders falls */
      R(0x608A) = 0x0E;
      R(0x608B) = 3;
      write3(0x7623, 0x10, 0x20);
      write3(0x7583, 0x10, 0x20);
      clear_box(0x76DA), draw_board(0x3A47);
      clear_box(0x76D5), draw_board(0x3A4D);
      clear_box(0x76D0), draw_board(0x3A53);
      clear_box(0x76CB), draw_board(0x3A59);
      kong_pose(0x385C);
      move_kong(0x6908, 0x44);
      R(0x6905) = 0x13;
      R(WAIT_MSB) = 0x20;
      R(0x6390) = 0x80;
      R(0x6388)++;
      break;
    case 1:
      if (rst18()) R(0x6388)++;
      break;
    case 2: { /* 0x1839: Kong beats his chest */
      uint8_t t = ++R(0x6390);
      if (!t) {
        kong_pose(0x385C);
        move_kong(0x6908, 0x44);
        R(WAIT_MSB) = 0x20;
        R(0x6388)++;
      } else if (!(t & 7)) {
        kong_pose((t & 8) ? 0x39CF : 0x39F7);
        move_kong(0x6908, 0x44);
      }
      break;
    }
    case 3: /* 0x186F: upside down */
      if (!rst18()) return;
      kong_pose(0x3A1F);
      R(0x6084) = 3;
      R(0x6388)++;
      break;
    case 4: /* 0x1880: he falls */
      move_kong(0x690B, 1);
      if (R(0x691B) != 0xD0) return;
      R(0x6919) = 0x20;
      R(0x6A24) = 0x7F, R(0x6A25) = 0x39, R(0x6A26) = 1, R(0x6A27) = 0xD8;
      clear_box(0x76C6);
      draw_board(0x3A5F);
      add_each(0x6903, 2, 0x28, 4);
      R(0x62AF) = 0;
      R(0x6082) = 3;
      R(0x6388)++;
      break;
    case 5: /* 0x18C6: Mario meets Pauline */
      if (!--R(0x62AF)) {
        next_board();
        R(LEVEL)++;
        task(5, 0);
        R(0x622E) = 0;
        R(0x6388) = 0;
        R(WAIT_MSB) = 0xE0;
        R(GAME_MODE2) = 8;
        return;
      }
      if (R(0x62AF) & 7) return;
      R(0x6A25) ^= 0x80;
      R(0x6919) = step_anim(0, R(0x6919) & ~0x20) | 0x20;
      if (R(0x62AF) == 0xE0) {
        R(0x694F) = 0x50;
        R(0x694D) = 0;
        R(0x694C) = 0x9F;
        if (R(MARIO_X) < 0x80) R(0x694D) = 0x80, R(0x694C) = 0x5F;
      }
      if (R(0x62AF) == 0xC0) {
        R(0x608A) = (R(LEVEL) & 1) ? 0x0C : 0x05;
        R(0x608B) = 3;
        R(0x6A23) = 0x40, R(0x6A22) = 9, R(0x6A21) = 0x76, R(0x6A20) = 0x8F;
        if (R(MARIO_X) < 0x80) R(0x6A20) = 0x6F;
      }
      break;
    }
    return;
  }
  /* 25m and 75m: 0 heart, 1 a pose, 2 the climb pose, 3 up to her, 4 away; 50m: 0 heart, 1 back along the belt */
  if (s == 2 && step >= 2) step++;
  switch (step) {
  case 0:
    heart();
    if (s == 2) {
      uint8_t x = U8(R(0x6910) - 0x3B);
      kong_pose(0x385C);
      move_kong(0x6908, x);
      R(0x6388)++;
      break;
    }
    kong_pose(0x385C);
    R(WAIT_MSB) = 0x20;
    R(0x6388)++;
    if (s == 1) move_kong(0x690B, 0xFC);
    break;
  case 1:
    if (s == 2) { /* 0x16BB: back along the top belt, to his ladder */
      R(0x62A0) = 0;
      uint8_t c = R(0x63A3), x = R(0x6910);
      bool reverse;
      if (x < 0x5A) reverse = c & 0x80;
      else if (x >= 0x5D) reverse = !(c & 0x80);
      else {
        kong_takes_her();
        R(0x6388)++;
        break;
      }
      if (reverse) R(0x62A0) = 1;
      belt_top();
      move_kong(0x6908, R(0x63A3));
      break;
    }
    if (!rst18()) return;
    kong_pose(0x3932);
    R(WAIT_MSB) = 0x20;
    R(0x6388)++;
    if (s == 3) move_kong(0x690B, 4);
    break;
  case 2:
    if (!rst18()) return;
    kong_takes_her();
    R(0x6388)++;
    if (s == 1) move_kong(0x690B, 0xFC);
    break;
  case 3: /* 0x1732: up the ladder; at her level he takes her */
    kong_climb_step();
    if (R(0x6913) >= 0x2C) return;
    R(0x6900) = R(0x6904) = 0;
    R(0x690C) = 0;
    R(0x6924) = 0x6B;
    R(0x692C) = 0x6A;
    R(0x6A21)++; /* the heart breaks */
    R(0x6388)++;
    break;
  case 4: { /* 0x1757: until he is gone */
    kong_climb_step();
    for (int i = 9; i >= 0; i--)
      if (R(0x690B + i * 4) < 0x19) R(0x6908 + i * 4) = 0;
    for (int i = 0; i < 10; i++)
      if (R(0x6908 + i * 4)) return;
    R(WAIT_MSB) = 0x40;
    R(0x6388)++;
    break;
  }
  case 5: /* 0x178E */
    if (!rst18()) return;
    next_board();
    task(5, 0);
    R(0x6388) = 0;
    R(WAIT_MSB) = 0x30;
    R(GAME_MODE2) = 8;
    break;
  }
}

/* ------------------------------------------------------------------ the frame */

static void game_modes(void) { /* 0x06FE */
  switch (R(GAME_MODE2)) {
  case 0x00: /* 0x0986 */
    clear_all();
    R(GAME_MODE2) = 1;
    break;
  case 0x01: /* 0x09AB */
    memcpy(&R(0x6228), &R(0x6040), 8);
    R(SCREEN) = ROM(R(0x622A) | R(0x622B) << 8);
    R(WAIT_MSB) = 1;
    R(GAME_MODE2) = 5;
    break;
  case 0x05: /* 0x0A37 */
    task(3, 4);
    task(2, 2);
    task(2, 0);
    task(6, 0);
    R(GAME_MODE2)++;
    V(0x7740) = 1, V(0x7720) = 0x25, V(0x7700) = 0x20;
    break;
  case 0x06: /* 0x0A63 */
    if (!rst18()) return;
    clear_play();
    R(WAIT_MSB) = 1;
    R(GAME_MODE2) = R(0x622C) ? 7 : 8;
    break;
  case 0x07: intro(); break;
  case 0x08: how_high(); break;
  case 0x0A:
    if (rst18()) start_board();
    break;
  case 0x0B: mario_start(); break;
  case 0x0C: play(); break;
  case 0x0D: dying(); break;
  case 0x0E: lose_life(); break;
  case 0x10:
    if (!S->over && rst18()) S->over = 1;
    break;
  case 0x16: board_clear(); break;
  }
}

static void foreground(void) { /* 0x02BD: what runs between two vblanks */
  run_tasks();
  flash_1up();
  extra_life();
  S->loop ^= S->loop << 13, S->loop ^= S->loop >> 17, S->loop ^= S->loop << 5;
  R(RNG2) = U8(R(RNG2) + 1 + (S->loop & 0xFF)); /* its own turns: as many as the vblank left time for */
  R(0x6383) = R(FRAME);
  difficulty_clock();
  fire_clock();
  run_tasks();
}

void dk_tick(dk_state *s, uint8_t joy) {
  S = s;
  uint8_t b = joy & 0x1F, c = b & 0x0F; /* 0x0098: InputState gets bit 7 when jump is first pressed */
  R(INPUT) = U8((U8(~R(RAW)) & b & 0x10) << 3 | c);
  R(RAW) = b;
  R(FRAME)--;
  rng();
  game_modes();
  foreground();
}

void dk_new_game(dk_state *s, uint32_t seed) {
  memset(s, 0, sizeof *s);
  S = s;
  memset(s->vram, 0x10, sizeof s->vram);
  R(START_LIVES) = 3; /* the factory settings: 3 lives, a bonus Mario at 7000 */
  R(EXTRA_AT) = 0x07;
  R(GAME_MODE1) = 3;
  R(0x60B0) = R(0x60B1) = 0xC0;
  memset(&R(0x60C0), 0xFF, 0x40);
  R(0x60B8) = 0x50, R(0x60B9) = 0x76, R(0x60BA) = 0x00; /* the high score table's top: 7650 */
  static const uint8_t start[8] = {3, 0x01, 0x65, 0x3A, 0x01, 0, 0, 0}; /* 0x095E: level 1, the order, intro */
  memcpy(&R(0x6040), start, 8);
  task(1, 0);
  R(RNG1) = U8(seed);
  R(RNG2) = U8(seed >> 8);
  R(FRAME) = U8(seed >> 16);
  s->loop = seed | 1;
}

/* ------------------------------------------------------------------ for the app */

uint32_t dk_bcd(const uint8_t *p) {
  uint32_t v = 0;
  for (int i = 2; i >= 0; i--) v = v * 100 + (p[i] >> 4) * 10 + (p[i] & 15);
  return v;
}
uint32_t dk_score(const dk_state *s) { return dk_bcd(&DK_RAM(s, 0x60B2)); }
int dk_screen(const dk_state *s) { return DK_RAM(s, 0x6227); }
int dk_mode(const dk_state *s) { return DK_RAM(s, 0x600A); }
int dk_level(const dk_state *s) { return DK_RAM(s, 0x6229); }
int dk_lives(const dk_state *s) { return DK_RAM(s, 0x6228); }
bool dk_alive(const dk_state *s) { return DK_RAM(s, 0x6200) & 1; }

bool dk_valid(const dk_state *s) {
  static const uint8_t modes[] = {0, 1, 5, 6, 7, 8, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x10, 0x16};
  uint8_t m = DK_RAM(s, 0x600A);
  bool ok = false;
  for (unsigned i = 0; i < sizeof modes; i++) ok |= m == modes[i];
  uint16_t order = DK_RAM(s, 0x622A) | DK_RAM(s, 0x622B) << 8;
  return ok && DK_RAM(s, 0x6005) == 3 && order >= 0x3A65 && order < 0x3A79 && DK_RAM(s, 0x6385) < 8 &&
         DK_RAM(s, 0x60B0) >= 0xC0 && DK_RAM(s, 0x60B1) >= 0xC0 && s->over <= 1 &&
         (m < 0x0A || (DK_RAM(s, 0x6227) >= 1 && DK_RAM(s, 0x6227) <= 4));
}
