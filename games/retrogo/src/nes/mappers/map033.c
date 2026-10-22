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
** map33.c
**
** mapper 33 interface
** $Id: map033.c,v 1.2 2001/04/27 14:37:11 neil Exp $
*/

#include "noftypes.h"
#include "nes_mmc.h"
#include "nes_ppu.h"

/* mapper 33: Taito TC0190*/
static void map33_write(uint32 address, uint8 value)
{
   int page = (address >> 13) & 3;
   int reg = address & 3;

   switch (page)
   {
   case 0: /* $800X */
      switch (reg)
      {
      case 0:
         mmc_bankrom(8, 0x8000, value);
         break;

      case 1:
         mmc_bankrom(8, 0xA000, value);
         break;

      case 2:
         mmc_bankvrom(2, 0x0000, value);
         break;

      case 3:
         mmc_bankvrom(2, 0x0800, value);
         break;
      }
      break;

   case 1: /* $A00X */
      {
         int loc = 0x1000 + (reg << 10);
         mmc_bankvrom(1, loc, value);
      }
      break;

   case 2: /* $C00X */
   case 3: /* $E00X */
      switch (reg)
      {
      case 0:
         /* irqs maybe ? */
         //break;
      
      case 1:
         /* this doesn't seem to work just right */
         if (value & 1)
            ppu_mirror(0, 0, 1, 1); /* horizontal */
         else
            ppu_mirror(0, 1, 0, 1);
         break;

      default:
         break;
      }
      break;
   }
}


static map_memwrite map33_memwrite[] =
{
   { 0x8000, 0xFFFF, map33_write },
   {     -1,     -1, NULL }
};

mapintf_t map33_intf =
{
   33, /* mapper number */
   "Taito TC0190", /* mapper name */
   NULL, /* init routine */
   NULL, /* vblank callback */
   NULL, /* hblank callback */
   NULL, /* get state (snss) */
   NULL, /* set state (snss) */
   NULL, /* memory read structure */
   map33_memwrite, /* memory write structure */
   NULL /* external sound device */
};

