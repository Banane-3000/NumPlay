/* The Z80 of the Master System and the Game Gear: superzazu's z80 (MIT License, see z80.c and
 * LICENSE.z80), changed to run inside the machine (sms.c): memory read through 1 KB pages, the
 * interrupt line held by the video chip, and whole stretches of clocks at a time. */
#ifndef Z80_Z80_H_
#define Z80_Z80_H_

#include <stdbool.h>
#include <stdint.h>

/* (only bytes: a save state is this as it is) */
typedef struct z80 {
  uint32_t cyc; // cycle count (t-states)

  uint16_t pc, sp, ix, iy; // special purpose registers
  uint16_t mem_ptr; // "wz" register
  uint8_t a, b, c, d, e, h, l; // main registers
  uint8_t a_, b_, c_, d_, e_, h_, l_, f_; // alternate registers
  uint8_t i, r; // interrupt vector, memory refresh

  // flags: sign, zero, yf, half-carry, xf, parity/overflow, negative, carry
  // (a byte each, 0 or 1)
  uint8_t sf, zf, yf, hf, xf, pf, nf, cf;

  uint8_t ei_shadow; // the instruction after EI: no interrupt before it
  uint8_t interrupt_mode;
  uint8_t iff1, iff2;
  uint8_t halted;
  uint8_t irq_line; // the INT line, held low by the machine while it wants an interrupt
  uint8_t nmi_pending;
  uint8_t pad_[3];
} z80;

/* The machine around the Z80 (sms.c, or a test's): 64 pages of 1 KB to read from, and the
   rest through functions. */
extern const uint8_t *z80_rmap[64];
void z80_bus_write(uint16_t addr, uint8_t val);
uint8_t z80_bus_in(uint8_t port);
void z80_bus_out(uint8_t port, uint8_t val);

void z80_init(z80* const z);
/* runs instructions until the cycle count reaches `until` (a little past it, the last one
   finished) */
void z80_run(z80* const z, uint32_t until);
void z80_gen_nmi(z80* const z);
/* a state read back: whatever it held, values the Z80 can have */
void z80_fix(z80* const z);

#endif // Z80_Z80_H_
