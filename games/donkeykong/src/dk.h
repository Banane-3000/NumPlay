/* Donkey Kong (Nintendo, 1981): the arcade game's program, ported to C from the disassembly (sim.c).
 *
 * The game keeps all its state in the arcade's own memory layout: its 3 KB of work RAM (with the sprite
 * table at 0x6900) and its 1 KB tile map. sim.c runs one frame of the original's 60 Hz vblank routine and
 * foreground tasks per dk_tick(); draw.c paints the tile map and the sprite table with new pictures. */
#ifndef DK_H
#define DK_H
#include <stdbool.h>
#include <stdint.h>

/* the joystick and the button, as the arcade reads them (IN0) */
enum { DK_RIGHT = 1, DK_LEFT = 2, DK_UP = 4, DK_DOWN = 8, DK_JUMP = 16 };

typedef struct {
  uint8_t ram[0x1000]; /* 0x6000-0x6FFF (the arcade has 0x6000-0x6BFF) */
  uint8_t vram[0x400]; /* 0x7400-0x77FF */
  uint8_t palette;     /* the palette bank: bit 0 REG_PALETTE_A, bit 1 REG_PALETTE_B */
  uint8_t over;        /* the game over box has been shown */
  uint8_t pad[2];
  uint32_t loop;       /* drives RngTimer2 (the foreground loop's own counter in the arcade) */
} dk_state;

#define DK_RAM(s, a) ((s)->ram[((a) - 0x6000) & 0xFFF])
#define DK_VRAM(s, a) ((s)->vram[((a) - 0x7400) & 0x3FF])

void dk_new_game(dk_state *s, uint32_t seed);
void dk_tick(dk_state *s, uint8_t joy);
bool dk_valid(const dk_state *s);

/* what the screens around the game read */
uint32_t dk_score(const dk_state *s);      /* the 6-digit BCD score, as a number */
uint32_t dk_bcd(const uint8_t *p);         /* 3 BCD bytes, low first */
int dk_screen(const dk_state *s);          /* 1 25m, 2 50m, 3 75m, 4 100m */
int dk_mode(const dk_state *s);            /* GameMode2 */
int dk_level(const dk_state *s);
int dk_lives(const dk_state *s);
bool dk_alive(const dk_state *s);

/* GameMode2 values the app looks at */
enum { DK_MODE_INTRO = 7, DK_MODE_HOWHIGH = 8, DK_MODE_PLAY = 0x0C, DK_MODE_DEAD = 0x0D, DK_MODE_OVER = 0x10,
       DK_MODE_CLEAR = 0x16 };
#endif
