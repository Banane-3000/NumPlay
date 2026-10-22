/* The NES's sound registers, for games that read them back (Retro-Go on NumPlay plays no sound:
 * the calculator has no speaker). Written for Retro-Go on NumPlay; GPL-3.0-or-later. */
#ifndef _NES_APU_H_
#define _NES_APU_H_
#include "noftypes.h"

/* (the sound chips of some mappers: not emulated) */
typedef struct apuext_s apuext_t;

uint8 apu_read(uint32 address);
void apu_write(uint32 address, uint8 value);
void apu_reset(void);
/* a 120th of a second: the length counters count down */
void apu_halfframe(void);

/* what a save state keeps */
typedef struct
{
   uint8 regs[0x18];
   uint8 enabled;
   uint8 length[4];
} apu_t;
extern apu_t apu;

#endif
