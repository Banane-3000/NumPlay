/* The Master System's Z80 (src/sms/z80.c) on a computer, running the CP/M Z80 instruction
 * exercisers as superzazu's z80 tests do: prelim.com, zexdoc.cim, zexall.cim (Frank Cringle's,
 * freely distributable; not in NumPlay). It prints what they print, then the clocks they took,
 * which must be the original z80's to the clock.
 *   host_z80 FILE EXPECTED_CYCLES */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../src/sms/z80.h"

static uint8_t memory[0x10000];
const uint8_t *z80_rmap[64];
static z80 cpu;
static int finished;
static uint32_t end_cyc;

void z80_bus_write(uint16_t addr, uint8_t val) { memory[addr] = val; }

/* "in a,(0)" at 0x0005: CP/M's BDOS, C = 2 a letter in E, 9 a text at DE up to '$' */
uint8_t z80_bus_in(uint8_t port) {
  (void)port;
  if (cpu.c == 2) putchar(cpu.e);
  else if (cpu.c == 9)
    for (uint16_t a = (uint16_t)(cpu.d << 8 | cpu.e); memory[a] != '$'; a++) putchar(memory[a]);
  fflush(stdout);
  return 0xFF;
}

/* "out (0),a" at 0x0000: the end (the clocks as they are with it; then halted, so the run stops) */
void z80_bus_out(uint8_t port, uint8_t val) {
  (void)port, (void)val;
  finished = 1;
  end_cyc = cpu.cyc;
  cpu.halted = 1;
}

int main(int argc, char **argv) {
  if (argc < 3) return fprintf(stderr, "usage: host_z80 FILE EXPECTED_CYCLES\n"), 2;
  FILE *f = fopen(argv[1], "rb");
  if (!f) return fprintf(stderr, "can't read %s\n", argv[1]), 1;
  size_t n = fread(memory + 0x100, 1, sizeof memory - 0x100, f);
  fclose(f);
  if (!n) return 1;
  for (int i = 0; i < 64; i++) z80_rmap[i] = memory + i * 0x400;
  memory[0x0000] = 0xD3, memory[0x0001] = 0x00;                         /* out (0),a */
  memory[0x0005] = 0xDB, memory[0x0006] = 0x00, memory[0x0007] = 0xC9; /* in a,(0); ret */
  z80_init(&cpu);
  cpu.pc = 0x100;
  /* (superzazu's tests start from these, not from a reset's) */
  cpu.sp = 0xFFFF, cpu.ix = cpu.iy = 0, cpu.a = 0xFF;
  unsigned long long total = 0, expected = strtoull(argv[2], NULL, 10);
  while (!finished) {
    uint32_t before = cpu.cyc;
    z80_run(&cpu, before + 1000000);
    total += (uint32_t)((finished ? end_cyc : cpu.cyc) - before);
  }
  printf("\n%llu cycles (expected %llu)\n", total, expected);
  return total == expected ? 0 : 1;
}
