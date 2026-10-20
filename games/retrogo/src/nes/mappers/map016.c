/*
** Nofrendo (c) 1998-2000 Matthew Conte (matt@conte.com)
**
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
** Any permitted reproduction of these routines, in whole or in part,
** must bear this legend.
**
**
** map16.c
**
** mapper 16 interface
** $Id: map016.c,v 1.2 2001/04/27 14:37:11 neil Exp $
*/

#include "noftypes.h"
#include "nes_mmc.h"
#include "nes_ppu.h"
#include "nes.h"

static struct
{
   int counter;
   bool enabled;
} irq;

/* mapper 16: Bandai */

static void map16_init(void)
{
   mmc_bankrom(16, 0x8000, 0);
   mmc_bankrom(16, 0xC000, MMC_LASTBANK);
   irq.counter = 0;
   irq.enabled = false;
}

static void map16_write(uint32 address, uint8 value)
{
   int reg = address & 0xF;

   if (reg < 8)
   {
      mmc_bankvrom(1, reg << 10, value);
   }
   else
   {
      switch (address & 0x000F)
      {
      case 0x8:
         mmc_bankrom(16, 0x8000, value);
         break;

      case 0x9:
         switch (value & 3)
         {
         case 0:
            ppu_mirror(0, 0, 1, 1); /* horizontal */
            break;
      
         case 1:
            ppu_mirror(0, 1, 0, 1); /* vertical */
            break;
      
         case 2:
            ppu_mirror(0, 0, 0, 0);
            break;
      
         case 3:
            ppu_mirror(1, 1, 1, 1);
            break;
         }
         break;
   
      case 0xA:
         irq.enabled = (value & 1) ? true : false;
         break;
 
      case 0xB:
         irq.counter = (irq.counter & 0xFF00) | value;
         break;
   
      case 0xC:
         irq.counter = (value << 8) | (irq.counter & 0xFF);
         break;
   
      case 0xD:
         /* eeprom I/O port? */
         break;
      }
   }
}

static void map16_hblank(int vblank)
{
   UNUSED(vblank);

   if (irq.enabled)
   {
      if (irq.counter)
      {
         if (0 == --irq.counter)
            nes_irq();
      }
   }
}

static void map16_getstate(SnssMapperBlock *state)
{
   state->extraData.mapper16.irqCounterLowByte = irq.counter & 0xFF;
   state->extraData.mapper16.irqCounterHighByte = irq.counter >> 8;
   state->extraData.mapper16.irqCounterEnabled = irq.enabled;
}

static void map16_setstate(SnssMapperBlock *state)
{
   irq.counter = (state->extraData.mapper16.irqCounterHighByte << 8)
                       | state->extraData.mapper16.irqCounterLowByte;
   irq.enabled = state->extraData.mapper16.irqCounterEnabled;
}

static map_memwrite map16_memwrite[] =
{
   { 0x6000, 0x600D, map16_write },
   { 0x7FF0, 0x7FFD, map16_write },
   { 0x8000, 0x800D, map16_write },
   {     -1,     -1, NULL }
};

mapintf_t map16_intf = 
{
   16, /* mapper number */
   "Bandai", /* mapper name */
   map16_init, /* init routine */
   NULL, /* vblank callback */
   map16_hblank, /* hblank callback */
   map16_getstate, /* get state (snss) */
   map16_setstate, /* set state (snss) */
   NULL, /* memory read structure */
   map16_memwrite, /* memory write structure */
   NULL /* external sound device */
};

