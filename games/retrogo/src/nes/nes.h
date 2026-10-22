/*
** Nofrendo (c) 1998-2000 Matthew Conte (matt@conte.com)
**
** This program is free software; you can redistribute it and/or modify it under the terms of
** the GNU General Public License as published by the Free Software Foundation; either version 3
** of the License, or (at your option) any later version. (Nofrendo was released under version
** 2 of the GNU Library General Public License; as its section 3 allows, this copy, changed for
** Retro-Go on NumPlay, is under the GNU General Public License instead.)
**
** This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
** without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
** See the GNU General Public License for more details.
**
** Any permitted reproduction of these routines, in whole or in part, must bear this legend.
**
** nes.h: the NES machine. Retro-Go on NumPlay keeps one, static, with its cartridge read in
** place.
*/

#ifndef _NES_H_
#define _NES_H_

#include "noftypes.h"
#include "nes_apu.h"
#include "nes_mmc.h"
#include "nes_ppu.h"
#include "nes_rom.h"
#include "nes6502.h"

#define  NES_SCREEN_WIDTH     256
#define  NES_SCREEN_HEIGHT    240
#define  NES_REFRESH_RATE     60

#define  MAX_MEM_HANDLERS     32

enum
{
   SOFT_RESET,
   HARD_RESET
};

typedef struct nes_s
{
   nes6502_memread readhandler[MAX_MEM_HANDLERS];
   nes6502_memwrite writehandler[MAX_MEM_HANDLERS];
   rominfo_t rominfo;

   bool fiq_occurred;
   uint8 fiq_state;
   int fiq_cycles;

   int scanline;

   /* Timing stuff */
   float scanline_cycles;
} nes_t;

extern nes_t nes;
/* the cartridge's bytes: nothing writes there */
extern uint8 *nes_rom_lo, *nes_rom_hi;
extern uint8 *nes6502_null_page;

extern nes_t *nes_getcontextptr(void);
extern void nes_setfiq(uint8 state);
extern uint8 nes_clearfiq(void);
extern void nes_nmi(void);
extern void nes_irq(void);
extern void nes_reset(int reset_type);

/* the pads */
#define  INP_JOYPAD0          0x0001
#define  INP_JOYPAD1          0x0002
#define  INP_ZAPPER           0x0004
extern void input_strobe(void);
extern uint8 input_get(int port);

#endif /* _NES_H_ */
